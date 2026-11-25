# Clean-room spec: per-frame constant-upload pairing (interp_pair.inc)

Context (functional only). A Direct3D 8 frame recorder keeps, for every frame, a list of vertex-shader constant
uploads (each: which shader, the first register, how many 4-float registers, and where the float values are stored in
that frame's memory arena). To draw an in-between picture it needs, for each upload of the current frame, the
matching upload of the previous frame, and a verdict on whether the values moved a little (blend them), did not move,
or jumped (do not blend). Two frames are kept: slot `cur` (being recorded) and slot `cur ^ 1` (the previous frame).
After each frame the host flips `cur`.

You write ONE file, `interp_pair.inc`, that the host `#include`s inside its own C file (C89-style MSVC C, x86, no
C++). Everything in it is `static`. The host already defines, before the include:

```c
#include <windows.h>   // DWORD, BYTE, WORD
#include <math.h>      // fabsf
enum { ST_NONE, ST_SAME, ST_MOVE, ST_REJ };              // verdict of one upload
typedef struct { DWORD off, prevOff, reg, vs; WORD n; BYTE st, pad; } Up;
// off     = byte offset of this upload's n*4 floats in the CURRENT frame's arena
// prevOff = (out) byte offset of the partner's floats in the PREVIOUS frame's arena
// reg, vs = first register, vertex shader handle;  n = register count (1..256)
// st      = (out) verdict, ST_NONE until paired
#define U_MAX 8192                 // at most this many uploads per frame
static Up g_up[U_MAX];
static int g_nu;                   // uploads recorded this frame (host increments it)
static int g_np;                   // pending uploads waiting for their draw
static int g_pend[256];            // indices into g_up of the pending uploads
static int g_cur;                  // current slot (0 or 1)
static int g_prevOk;               // 1 when slot cur^1 holds a usable previous frame
typedef struct { BYTE *p; DWORD n, cap; } Arena;
static Arena g_arena[2];           // g_arena[slot].p + off -> the floats
```

## What you must provide

1. `static DWORD HashCombine(DWORD h, DWORD v)` — fold the 32-bit value `v` into the running hash `h` and return a
   well-mixed 32-bit result (good avalanche: used for open-addressing tables with power-of-two sizes and for draw
   signatures). Use any well-known public-domain mixing step you like (e.g. a MurmurHash3-style finaliser or similar),
   but design it yourself.

2. `static void PairBeginFrame(int slot)` — called when recording of a new frame starts in `slot`: forget everything
   stored in that slot's pairing table (must be O(1) or cheap — this runs 30 times a second; a generation counter is
   fine) and reset this module's per-frame counters to 0.

3. `static void PairPending(DWORD sig)` — called at each draw call. `sig` is a 32-bit signature of that draw (the
   host computes it). Every upload index in `g_pend[0 .. g_np-1]` belongs to that draw. For each of them:
   - Build a key from (`sig`, `vs`, `reg`, `n`). Several uploads in one frame can share the same (sig, vs, reg, n):
     the k-th such upload in the frame (k = 0, 1, 2 …, in recording order) must get a key distinct from the others
     but the SAME key as the k-th such upload of the next frame (so frames with the same structure pair up in order).
     Cap k at 63: uploads beyond that are left unpaired (st stays ST_NONE).
   - Store key -> (off, n) in the CURRENT slot's table. The table must hold at least U_MAX entries per frame; if it
     is too full to insert (keep load under ~50 %), leave the upload unpaired.
   - If `g_prevOk`, look the key up in the PREVIOUS slot's table. A partner counts only if found AND its n equals
     this upload's n. Then: increment the "matched" counter, set `prevOff` to the partner's offset, set `st` by
     `Classify` on (current floats, partner floats, n), add Classify's changed/jump counts to this frame's counters,
     and if `st == ST_MOVE` increment the "moved" counter.
   - Finally set `g_np = 0`.

4. `static int Classify(const float *c, const float *p, int n, int *chg, int *rej)` — compare n registers
   (n*4 floats) of the current values `c` with the previous `p`. Per register (4 lanes):
   - diff = the largest |c[l] - p[l]| over the 4 lanes; mag = the largest of |c[l]|, |p[l]| over the 4 lanes.
   - If any lane is NaN or infinite (in c, p or the difference), the register counts as changed AND jumped.
   - The register "changed" if diff > 0; it "jumped" if diff > 0.35 * mag + 1e-4.
   - Add the number of changed registers to `*chg` and of jumped registers to `*rej`.
   - Return ST_REJ if any register jumped, else ST_MOVE if any changed, else ST_SAME.

5. `static int Decide(void)` — after the frame, using this frame's counters (uploads = `g_nu`):
   return W_STILL if no upload has st == ST_MOVE (moved counter is 0); else W_PAIRS if matched * 2 < g_nu;
   else W_CUT if jumped-registers * 3 > changed-registers; else W_OK. Define
   `enum { W_OK, W_STILL, W_PAIRS, W_CUT };` in your file.

Rules: no allocation (static arrays only), no CRT beyond fabsf, no globals other than your own `static` ones with a
`pm_` / `g_pm` prefix, MSVC /W4 clean. Write tests in `test_pair.c` that include a tiny host shim (the definitions
above), then your file, and check: distinct keys for repeated tuples, same keys across frames, n mismatch never pairs,
load cap, generation reset, Classify edge cases (no change, small move, 0 -> 1 is a jump, 1e-4 floor for tiny values,
NaN), all four Decide outcomes. Build with `cl /W4 /nologo test_pair.c` (VS 2022 x86 vcvars32) and run it.
