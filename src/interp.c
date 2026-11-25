// ================================================================ FRAME INTERPOLATION (v2.1; v2.2: even pacing, matrix jump rule,
// culling margin in sh4windowmod.c, diagnostics in interp_diag.inc; researchramerate4)
// SH4 Refined (winmm.dll proxy). Research: research\framerate\v3\interpolation.md (design, D3D8 inventory, tests).
//
// Why: the game's logic is a fixed 30 Hz tick with a long tail of per-tick constants, so running the logic faster
// (the FRAME RATE ENGINE, FrameMode=0) makes enemies, NPCs and UI faster. sh4vita (github.com/vallfrr/sh4vita,
// port/src/d3d8/qgl.c) keeps the logic at 30 and draws in-between frames: the frame's command stream is kept until its
// Present and played again with the vertex-shader constants (camera, object and bone matrices) blended between the
// previous frame and this one. This file ports that to the PC: Direct3D 8 through GOG's d3d8.dll wrapper.
//
// How (all on the game thread; every other thread passes straight through):
// * The IDirect3DDevice8 vtable of the game's device ([0x6E7D94], shared by every caller: the game, Playable Eileen's
//   HUD, this dll's Menus43 code) is patched. Every call still goes to the device AT ONCE (the real frame is drawn as
//   without the mod: Get* calls, readbacks and screen copies stay right) and is ALSO recorded between two Presents:
//   states, textures, streams, shaders, constants, render targets, viewports, clears, draws (UP vertex / index data
//   copied), screen copies (CopyRects), scenes; dynamic vertex / index buffer writes are copied at Unlock.
// * At the Present of tick T (the back buffer holds the real frame T) with k = rate / 30 presents per tick:
//   copy the back buffer aside; for j = 1 .. k-1 replay the record with the VS constants at fraction j/k between the
//   previous frame and this one, Present; copy the real frame back, Present. The presents are paced over the tick
//   (QueryPerformanceCounter, a high-resolution waitable timer) and never run into the next tick.
// * Replay state: the state each element (render state, stage state, texture, stream, index buffer, shader, constant
//   register, render target, viewport) had at its first change of the frame is kept (S0, references held); a replay
//   puts S0 back first and the end-of-frame state (S1) after, so the next tick sees exactly the device it left.
//   Dynamic buffers: a Lock of a buffer already drawn from this frame saves its old bytes (the prelude, put back in
//   reverse order before a replay); each write is replayed where it happened.
// * Pairing (sh4vita): each constant upload is keyed by (vertex shader, register, count, signature of the next draw,
//   occurrence) and paired with the previous frame's upload of the same key. Blend per upload: if any vec4 moved by
//   more than 0.35 x its magnitude + 1e-4 the upload keeps its new value (cuts, wrapping scrolls); sh4vita did this per
//   vec4, here the whole upload so a matrix is never half blended. A frame is not interpolated when fewer than half
//   of its uploads pair, when more than a third of the changed vec4 jump (a camera cut), when nothing moved, outside
//   gameplay (front end, menus, loading), after a gap in the ticks, or when the record is unsafe (see below).
// * Safety: anything the replay cannot reproduce makes that frame present once, real only: a record over the size
//   cap, a method that is not recorded (transforms, lights, materials, clip planes, state blocks, palettes, patches,
//   ProcessVertices), a DISCARD lock or texture lock of a resource already used in the frame, a shader deleted
//   mid-frame, too many held references. Failures of the copy / texture creation or an exception in a replay turn the
//   interpolation off for the session (logged once). The real frame never depends on the record.
// Not smooth by design: everything the CPU moves itself (UP sprites, particles, 2D, CPU-written vertices) stays 30 Hz;
// only what the vertex shaders place through constants (camera, objects, GPU-skinned bones) is interpolated.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "interp.h"

typedef char interp_x86_only[sizeof(void *) == 4 ? 1 : -1];  // pointers are kept in DWORDs (the game is 32-bit)

static InterpHost H;
#define LOG(...) do { if (H.log) H.log(__VA_ARGS__); } while (0)

// ---------------------------------------------------------------- IDirect3DDevice8 vtable indices
enum {
    VI_ADDREF = 1, VI_RELEASE = 2, VI_RESET = 14, VI_PRESENT = 15, VI_GETBACKBUFFER = 16, VI_CREATETEXTURE = 20,
    VI_CREATEVB = 23, VI_CREATEIB = 24, VI_COPYRECTS = 28, VI_UPDATETEXTURE = 29, VI_GETFRONTBUFFER = 30, VI_SETRT = 31,
    VI_GETRT = 32, VI_GETDS = 33, VI_BEGINSCENE = 34, VI_ENDSCENE = 35, VI_CLEAR = 36, VI_SETTRANSFORM = 37,
    VI_MULTIPLYTRANSFORM = 39, VI_SETVIEWPORT = 40, VI_GETVIEWPORT = 41, VI_SETMATERIAL = 42, VI_SETLIGHT = 44,
    VI_LIGHTENABLE = 46, VI_SETCLIPPLANE = 48, VI_SETRS = 50, VI_GETRS = 51, VI_BEGINSB = 52, VI_ENDSB = 53,
    VI_APPLYSB = 54, VI_CAPTURESB = 55, VI_CREATESB = 57, VI_SETCLIPSTATUS = 58, VI_GETTEXTURE = 60, VI_SETTEXTURE = 61,
    VI_GETTSS = 62, VI_SETTSS = 63, VI_SETPALETTE = 66, VI_SETCURPALETTE = 68, VI_DP = 70, VI_DIP = 71, VI_DPUP = 72,
    VI_DIPUP = 73, VI_PROCESSVERTICES = 74, VI_SETVS = 76, VI_GETVS = 77, VI_DELETEVS = 78, VI_SETVSC = 79,
    VI_GETVSC = 80, VI_SETSTREAM = 83, VI_GETSTREAM = 84, VI_SETINDICES = 85, VI_GETINDICES = 86, VI_SETPS = 88,
    VI_GETPS = 89, VI_DELETEPS = 90, VI_SETPSC = 91, VI_GETPSC = 92, VI_DRAWRECTPATCH = 94, VI_DRAWTRIPATCH = 95,
    VI_DELETEPATCH = 96, VI_N = 97
};
// resources: IUnknown 0-2; vertex / index buffer Lock 11, Unlock 12, GetDesc 13; texture GetSurfaceLevel 15,
// LockRect 16; surface GetDesc 8
enum { RV_LOCK = 11, RV_UNLOCK = 12, RV_GETDESC = 13, TV_GETSURFACE = 15, TV_LOCKRECT = 16, SV_GETDESC = 8 };
#define D3DLOCK_READONLY 0x10
#define D3DLOCK_DISCARD 0x2000
#define VT(o) (*(void ***)(o))

typedef HRESULT(STDMETHODCALLTYPE *F0)(void *);
typedef HRESULT(STDMETHODCALLTYPE *F1)(void *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F2)(void *, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F3)(void *, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F4)(void *, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F5)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F6)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F7)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *F8)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef ULONG(STDMETHODCALLTYPE *FRef)(void *);
#define P(x) ((DWORD)(UINT_PTR)(x))
#define PTR(x) ((void *)(UINT_PTR)(x))

static void *g_o[VI_N];  // the device's original methods (the wrapper's)
static void *g_dev;      // the hooked device
static DWORD g_tid;      // the game thread
static int g_inOwn;      // this file is calling the device (replays, presents): hooks pass through

// ---------------------------------------------------------------- settings / pacing state
static int g_userRate = 30, g_capRate, g_kMax = 1, g_k = 1;
static volatile LONG g_rateDirty;
static int g_disabled, g_attached, g_started;
static unsigned __int64 g_qpf;
static HANDLE g_timer;
static unsigned __int64 g_phi, g_replayEst;  // phase of the first in-between frame after the tick start; replay cost
static int g_noWait, g_autoLower = 1;  // tests; [Window] InterpAutoLower
// Auto-lowering (Judge): only the steady state counts. The verdict is whether the GAME's ticks stretch.
#define WARM_TICKS 90     // gameplay ticks not judged after attach, a reset, a rate change, a gap, a hitch, menus
#define WIN_TICKS 128     // interpolated steady ticks per verdict (about 4 s)
#define RETRY_TICKS 1800  // gameplay ticks at a lowered rate before one step up is tried again (x2 if that fails, up to x8)
static int g_warm = WARM_TICKS, g_winN, g_lowTicks, g_retryWait = RETRY_TICKS, g_raised, g_heldTicks;
static unsigned __int64 g_winSpan, g_winStep, g_prevFs;
static int g_prevFsOk, g_prevInterp;
#define ARENA_INIT (8u << 20)
static unsigned g_arenaCap = 32u << 20;  // per frame; two frames are kept

// ---------------------------------------------------------------- shadow state
#define EL_RS(s) (s)
#define EL_TSS(st, t) (256 + (st) * 32 + (t))
#define EL_TEX(st) (512 + (st))
#define EL_STREAM(n) (520 + (n))
#define EL_IDX 536
#define EL_VS 537
#define EL_PS 538
#define EL_RT 539
#define EL_VP 540
#define EL_VSC(r) (541 + (r))
#define EL_PSC(r) (797 + (r))
#define EL_N 829
#define NVSC 256
#define NPSC 32
static DWORD g_sv[EL_N][6];  // what the hooks last saw set (S1 at the end of a frame)
static BYTE g_known[EL_N];
static DWORD g_s0[EL_N][6];  // the value before the frame's first change (S0)
static BYTE g_s0known[EL_N];
static DWORD g_dirtyGen[EL_N], g_gen = 1;
static WORD g_dirty[EL_N];
static int g_ndirty, g_maxStream;
static DWORD g_vpEnd[6];
static int g_vpEndOk;

// ---------------------------------------------------------------- the record
enum { OP_RS = 1, OP_TSS, OP_TEX, OP_STREAM, OP_IDX, OP_VS, OP_PS, OP_VSC, OP_PSC, OP_RT, OP_VP, OP_CLEAR, OP_BEGIN,
       OP_END, OP_DP, OP_DIP, OP_DPUP, OP_DIPUP, OP_COPY, OP_BUF };
#define CF_SKIP 1  // OP_BUF: not replayed (the buffer already holds these bytes)
typedef struct { WORD op, fl; DWORD size; } Cmd;
typedef struct { Cmd h; DWORD a, b, c; } CmdA3;         // RS (a, b), TSS (a, b, c), VS / PS / BEGIN / END (a)
typedef struct { Cmd h; DWORD a; void *p; DWORD b; } CmdP; // TEX (a, p), STREAM (a, p, b), IDX (p, b)
typedef struct { Cmd h; DWORD reg, n; int u; } CmdK;      // VSC / PSC, floats follow
typedef struct { Cmd h; void *rt, *ds; } CmdRT;
typedef struct { Cmd h; DWORD vp[6]; } CmdVP;
typedef struct { Cmd h; DWORD count, flags, color; float z; DWORD stencil; } CmdClear;  // rects follow
typedef struct { Cmd h; DWORD type, a, b, c, d; } CmdDraw;
typedef struct { Cmd h; DWORD type, prim, stride, vbytes; } CmdDPUP;  // vertices follow
typedef struct { Cmd h; DWORD type, minI, numV, prim, ifmt, stride, ibytes, vbytes; } CmdDIPUP;  // indices, vertices
typedef struct { Cmd h; void *src, *dst; DWORD n, hasRects, hasPoints; } CmdCopy;  // rects, points follow
typedef struct { Cmd h; void *buf; DWORD off, size, flags; } CmdBuf;  // bytes follow

typedef struct { BYTE *p; unsigned n, cap; } Arena;
static Arena g_arena[2], g_pre;  // two frames (the current record, the previous frame's constants); the prelude
static int g_cur;                // g_arena[g_cur] = the frame being recorded
static int g_rec, g_stop, g_prevOk;
static unsigned g_bad;  // why this frame cannot be replayed (BAD_*)
enum { BAD_BIG = 1, BAD_METHOD = 2, BAD_DISCARD = 4, BAD_TEXLOCK = 8, BAD_SHADER = 16, BAD_REFS = 32, BAD_RANGE = 64 };
static const char *const kBadName[] = { "record too big", "a method that is not recorded", "a DISCARD lock of a buffer used earlier in the frame",
                                        "a texture written after use in the frame", "a shader deleted mid-frame", "too many held references",
                                        "a register / stage out of range" };
static unsigned g_badLogged;
static DWORD g_preOff[1024];
static int g_npre;

// constant uploads of the frame
#define U_MAX 8192
enum { ST_NONE, ST_SAME, ST_MOVE, ST_REJ };
typedef struct { DWORD off, prevOff, reg, vs; WORD n; BYTE st, pad; } Up;
static Up g_up[U_MAX];
static int g_nu, g_np;
static int g_pend[256];
// Pairing of the uploads with the previous frame's (HashCombine, PairBeginFrame, PairPending, Classify, Decide):
// a clean-room implementation written from a functional spec (scratchpad cleanroom\SPEC.md, kept with the
// research) by an author who did not see sh4vita's source; the method itself is sh4vita's idea.
#include "interp_pair.inc"
static unsigned g_prevTick, g_curTick;

// per-frame pointer sets
typedef struct { const void *p; DWORD gen, a, b; } PSlot;
#define BUF_N 4096
#define TEX_N 4096
#define REF_N 16384
#define REF_MAX 8192
static PSlot g_bufInfo[BUF_N], g_texSet[TEX_N], g_refSet[REF_N];
static DWORD g_hgen = 1, g_refGen = 1;
static int g_nbuf, g_ntex;
static void *g_refs[REF_MAX];
static int g_nrefs;

// resource classes whose Lock / LockRect are hooked
typedef struct { void **vt; void *lock, *unlock; } BufClass;
typedef struct { void **vt; void *lockRect; } TexClass;
static BufClass g_bufc[8];
static TexClass g_texc[8];
typedef struct { void *buf, *ptr; DWORD off, size, flags; } ActiveLock;
static ActiveLock g_locks[16];

// the real frame, kept while the in-between frames are drawn
static void *g_savedTex, *g_savedSurf;
static DWORD g_savedW, g_savedH, g_savedFmt;
static int g_copyBack = 1;

// statistics (logged every 30 s of gameplay)
static struct {
    unsigned ticks, interp, cuts, still, pairs, nottime, bad, big, squeezed, overran, plain, presents, kb, kbMax, spanN;
    unsigned __int64 replay, spanSum;
} S;
static unsigned g_logTicks;

#include "interp_diag.inc"

// ---------------------------------------------------------------- small helpers
static unsigned __int64 Qpc(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (unsigned __int64)t.QuadPart;
}

static void WaitUntil(unsigned __int64 t)
{
    const unsigned __int64 ms = g_qpf / 1000;
    if (g_noWait) return;
    for (;;) {
        const unsigned __int64 now = Qpc();
        unsigned __int64 rem;
        if (now >= t) return;
        rem = t - now;
        if (g_timer && rem > 2 * ms) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)((rem - ms) * 10000000u / g_qpf);  // relative, 100 ns units
            if (SetWaitableTimer(g_timer, &due, 0, NULL, NULL, FALSE)) WaitForSingleObject(g_timer, 50);
            else g_timer = NULL;
        } else if (rem > ms / 2) {
            SwitchToThread();
        } else {
            YieldProcessor();
        }
    }
}

static PSlot *PFind(PSlot *t, DWORD n, DWORD gen, const void *p, int add, int *isNew)
{
    DWORD i = HashCombine(0x2545F491u, P(p)) & (n - 1), k;
    if (isNew) *isNew = 0;
    for (k = 0; k < n; ++k, i = (i + 1) & (n - 1)) {
        PSlot *s = &t[i];
        if (s->gen != gen) {
            if (!add) return NULL;
            s->p = p;
            s->gen = gen;
            s->a = s->b = 0;
            if (isNew) *isNew = 1;
            return s;
        }
        if (s->p == p) return s;
    }
    return NULL;
}

static void MarkBad(unsigned why, int method)
{
    int i;
    g_bad |= why;
    for (i = 0; i < 7; ++i)
        if ((why >> i & 1) && !(g_badLogged >> i & 1)) {
            g_badLogged |= 1u << i;
            if (method >= 0) LOG("interp: a frame is shown without in-between frames: %s (device method %d); logged once", kBadName[i], method);
            else LOG("interp: a frame is shown without in-between frames: %s; logged once", kBadName[i]);
        }
}

// holds a reference until the frame's replays are done (each object once per frame)
static void Ref(void *o)
{
    int isNew;
    PSlot *s;
    if (!o) return;
    if (g_nrefs >= REF_MAX || (s = PFind(g_refSet, REF_N, g_refGen, o, 1, &isNew)) == NULL) {
        MarkBad(BAD_REFS, -1);
        return;
    }
    if (!isNew) return;
    ((FRef)VT(o)[VI_ADDREF])(o);
    g_refs[g_nrefs++] = o;
}

static void ReleaseRefs(void)
{
    int i;
    for (i = 0; i < g_nrefs; ++i) ((FRef)VT(g_refs[i])[VI_RELEASE])(g_refs[i]);
    g_nrefs = 0;
    if (++g_refGen == 0) g_refGen = 1;
}

static int ArenaInit(Arena *a, unsigned cap)
{
    if (a->p) return 1;
    a->p = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cap);
    a->cap = a->p ? cap : 0;
    a->n = 0;
    return a->p != NULL;
}

// room for `size` bytes (4-aligned) in the arena; NULL = over the cap or out of memory (the frame stops recording)
static void *Grow(Arena *a, unsigned size)
{
    size = (size + 3) & ~3u;
    if (a->n + size > g_arenaCap || a->n + size < a->n) return NULL;
    if (a->n + size > a->cap) {
        unsigned nc = a->cap ? a->cap : ARENA_INIT;
        BYTE *np;
        while (nc < a->n + size) nc *= 2;
        if (nc > g_arenaCap) nc = g_arenaCap;
        np = (BYTE *)(a->p ? HeapReAlloc(GetProcessHeap(), 0, a->p, nc) : HeapAlloc(GetProcessHeap(), 0, nc));
        if (!np) return NULL;
        a->p = np;
        a->cap = nc;
    }
    a->n += size;
    return a->p + a->n - size;
}

static Cmd *Emit(int op, unsigned size)
{
    Cmd *c;
    if (!g_rec || g_stop) return NULL;
    c = (Cmd *)Grow(&g_arena[g_cur], size);
    if (!c) {
        g_stop = 1;
        MarkBad(BAD_BIG, -1);
        return NULL;
    }
    c->op = (WORD)op;
    c->fl = 0;
    c->size = (size + 3) & ~3u;
    return c;
}

static int Mine(void *d) { return d == g_dev && !g_inOwn && GetCurrentThreadId() == g_tid; }
static int Rec(void) { return g_rec && !g_stop; }

// ---------------------------------------------------------------- state elements: read, write, first change
static void ReleaseObj(void *o)
{
    if (o) ((FRef)VT(o)[VI_RELEASE])(o);
}

static void Fetch(int el)
{
    void *d = g_dev, *p = NULL, *q = NULL;
    DWORD *v = g_sv[el];
    HRESULT hr = E_FAIL;
    g_inOwn++;
    if (el < 256) hr = ((F2)g_o[VI_GETRS])(d, (DWORD)el, P(&v[0]));
    else if (el < 512) hr = ((F3)g_o[VI_GETTSS])(d, (DWORD)(el - 256) / 32, (DWORD)(el - 256) % 32, P(&v[0]));
    else if (el < 520) {
        hr = ((F2)g_o[VI_GETTEXTURE])(d, (DWORD)(el - 512), P(&p));
        v[0] = P(p);
        ReleaseObj(p);  // bound: the device keeps it
    } else if (el < 536) {
        hr = ((F3)g_o[VI_GETSTREAM])(d, (DWORD)(el - 520), P(&p), P(&v[1]));
        v[0] = P(p);
        ReleaseObj(p);
    } else if (el == EL_IDX) {
        hr = ((F2)g_o[VI_GETINDICES])(d, P(&p), P(&v[1]));
        v[0] = P(p);
        ReleaseObj(p);
    } else if (el == EL_VS) hr = ((F1)g_o[VI_GETVS])(d, P(&v[0]));
    else if (el == EL_PS) hr = ((F1)g_o[VI_GETPS])(d, P(&v[0]));
    else if (el == EL_RT) {
        hr = ((F1)g_o[VI_GETRT])(d, P(&p));
        if (SUCCEEDED(hr) && FAILED(((F1)g_o[VI_GETDS])(d, P(&q)))) q = NULL;  // no depth buffer
        v[0] = P(p);
        v[1] = P(q);
        ReleaseObj(p);
        ReleaseObj(q);
    } else if (el == EL_VP) hr = ((F1)g_o[VI_GETVIEWPORT])(d, P(v));
    else if (el < EL_PSC(0)) hr = ((F3)g_o[VI_GETVSC])(d, (DWORD)(el - EL_VSC(0)), P(v), 1);
    else hr = ((F3)g_o[VI_GETPSC])(d, (DWORD)(el - EL_PSC(0)), P(v), 1);
    g_inOwn--;
    g_known[el] = SUCCEEDED(hr);
}

static void Apply(int el, const DWORD *v)
{
    void *d = g_dev;
    if (el < 256) ((F2)g_o[VI_SETRS])(d, (DWORD)el, v[0]);
    else if (el < 512) ((F3)g_o[VI_SETTSS])(d, (DWORD)(el - 256) / 32, (DWORD)(el - 256) % 32, v[0]);
    else if (el < 520) ((F2)g_o[VI_SETTEXTURE])(d, (DWORD)(el - 512), v[0]);
    else if (el < 536) ((F3)g_o[VI_SETSTREAM])(d, (DWORD)(el - 520), v[0], v[1]);
    else if (el == EL_IDX) ((F2)g_o[VI_SETINDICES])(d, v[0], v[1]);
    else if (el == EL_VS) ((F1)g_o[VI_SETVS])(d, v[0]);
    else if (el == EL_PS) ((F1)g_o[VI_SETPS])(d, v[0]);
    else if (el == EL_RT) { if (v[0]) ((F2)g_o[VI_SETRT])(d, v[0], v[1]); }
    else if (el == EL_VP) ((F1)g_o[VI_SETVIEWPORT])(d, P(v));
    else if (el < EL_PSC(0)) ((F3)g_o[VI_SETVSC])(d, (DWORD)(el - EL_VSC(0)), P(v), 1);
    else ((F3)g_o[VI_SETPSC])(d, (DWORD)(el - EL_PSC(0)), P(v), 1);
}

// before the frame's first change of el: keep its value (S0) and a reference to what it binds
static void Touch(int el)
{
    if (g_dirtyGen[el] == g_gen) return;
    g_dirtyGen[el] = g_gen;
    if (!g_known[el] || el == EL_VP) Fetch(el);  // the viewport also changes with SetRenderTarget: always read
    memcpy(g_s0[el], g_sv[el], sizeof g_s0[el]);
    g_s0known[el] = g_known[el];
    g_dirty[g_ndirty++] = (WORD)el;
    if (g_s0known[el]) {
        if ((el >= 512 && el < 537)) Ref(PTR(g_s0[el][0]));
        else if (el == EL_RT) {
            Ref(PTR(g_s0[el][0]));
            Ref(PTR(g_s0[el][1]));
        }
    }
}

// puts S0 (start = 1) or S1 (start = 0) back for every element the frame changed: render target first (it resets
// the viewport), then the viewport, then the rest
static void RestoreState(int start)
{
    int i, pass;
    for (pass = 0; pass < 3; ++pass)
        for (i = 0; i < g_ndirty; ++i) {
            const int el = g_dirty[i];
            if ((pass == 0) != (el == EL_RT) || (pass == 1) != (el == EL_VP)) continue;
            if (start) {
                if (g_s0known[el]) Apply(el, g_s0[el]);
            } else if (el == EL_VP) {
                if (g_vpEndOk) Apply(el, g_vpEnd);
            } else if (g_known[el]) {
                Apply(el, g_sv[el]);
            }
        }
}

// ---------------------------------------------------------------- per-frame sets: buffers, textures
// the frame's buffers: a = 1 used by a draw, 2 prelude taken; b = number of writes recorded
static PSlot *BufInfo(const void *b) { return PFind(g_bufInfo, BUF_N, g_hgen, b, 0, NULL); }
static void BufUse(const void *b)
{
    PSlot *s;
    int isNew;
    if (!b) return;
    if (g_nbuf >= BUF_N / 2) { MarkBad(BAD_REFS, -1); return; }
    s = PFind(g_bufInfo, BUF_N, g_hgen, b, 1, &isNew);
    if (!s) { MarkBad(BAD_REFS, -1); return; }
    if (isNew) g_nbuf++;
    s->a |= 1;
}

static void TexMark(const void *t)
{
    int isNew;
    if (!t) return;
    if (g_ntex >= TEX_N / 2 || !PFind(g_texSet, TEX_N, g_hgen, t, 1, &isNew)) { MarkBad(BAD_REFS, -1); return; }
    if (isNew) g_ntex++;
}

// ---------------------------------------------------------------- constant blending
static void Blend(float *out, const float *c, const float *p, int n, float a)
{
    int i;
    for (i = 0; i < n * 4; ++i) out[i] = p[i] + (c[i] - p[i]) * a;
}

static DWORD DrawSig(int op, DWORD type, DWORD a, DWORD b, DWORD c, DWORD d, int buffers)
{
    DWORD h = HashCombine(HashCombine(HashCombine(HashCombine(HashCombine(HashCombine((DWORD)op, type), a), b), c), d), g_sv[EL_VS][0]);
    h = HashCombine(h, g_sv[EL_TEX(0)][0]);
    if (buffers) h = HashCombine(HashCombine(HashCombine(h, g_sv[EL_STREAM(0)][0]), g_sv[EL_STREAM(0)][1]), g_sv[EL_IDX][0]);
    return h;
}

static void DrawUses(int indexed)
{
    int n;
    for (n = 0; n <= g_maxStream; ++n)
        if (g_sv[EL_STREAM(n)][0]) BufUse(PTR(g_sv[EL_STREAM(n)][0]));
    if (indexed && g_sv[EL_IDX][0]) BufUse(PTR(g_sv[EL_IDX][0]));
}

// ---------------------------------------------------------------- hooks: state
static HRESULT STDMETHODCALLTYPE hkSetRS(void *d, DWORD s, DWORD v)
{
    if (Mine(d)) {
        if (s < 256) {
            if (Rec()) {
                CmdA3 *c;
                Touch(EL_RS(s));
                if ((c = (CmdA3 *)Emit(OP_RS, sizeof *c)) != NULL) { c->a = s; c->b = v; }
            }
            g_sv[EL_RS(s)][0] = v;
            g_known[EL_RS(s)] = 1;
        } else if (Rec()) MarkBad(BAD_RANGE, VI_SETRS);
    }
    return ((F2)g_o[VI_SETRS])(d, s, v);
}

static HRESULT STDMETHODCALLTYPE hkSetTSS(void *d, DWORD st, DWORD t, DWORD v)
{
    if (Mine(d)) {
        if (st < 8 && t < 32) {
            if (Rec()) {
                CmdA3 *c;
                Touch(EL_TSS(st, t));
                if ((c = (CmdA3 *)Emit(OP_TSS, sizeof *c)) != NULL) { c->a = st; c->b = t; c->c = v; }
            }
            g_sv[EL_TSS(st, t)][0] = v;
            g_known[EL_TSS(st, t)] = 1;
        } else if (Rec()) MarkBad(BAD_RANGE, VI_SETTSS);
    }
    return ((F3)g_o[VI_SETTSS])(d, st, t, v);
}

static HRESULT STDMETHODCALLTYPE hkSetTexture(void *d, DWORD st, void *tex)
{
    if (Mine(d)) {
        if (st < 8) {
            if (Rec()) {
                CmdP *c;
                Touch(EL_TEX(st));
                if ((c = (CmdP *)Emit(OP_TEX, sizeof *c)) != NULL) { c->a = st; c->p = tex; }
                Ref(tex);
                TexMark(tex);
            }
            g_sv[EL_TEX(st)][0] = P(tex);
            g_known[EL_TEX(st)] = 1;
        } else if (Rec()) MarkBad(BAD_RANGE, VI_SETTEXTURE);
    }
    return ((F2)g_o[VI_SETTEXTURE])(d, st, P(tex));
}

static void HookBufClass(void *obj);

static HRESULT STDMETHODCALLTYPE hkSetStream(void *d, DWORD n, void *vb, DWORD stride)
{
    if (Mine(d)) {
        if (n < 16) {
            if (vb) HookBufClass(vb);
            if (Rec()) {
                CmdP *c;
                Touch(EL_STREAM(n));
                if ((c = (CmdP *)Emit(OP_STREAM, sizeof *c)) != NULL) { c->a = n; c->p = vb; c->b = stride; }
                Ref(vb);
            }
            g_sv[EL_STREAM(n)][0] = P(vb);
            g_sv[EL_STREAM(n)][1] = stride;
            g_known[EL_STREAM(n)] = 1;
            if (vb && (int)n > g_maxStream) g_maxStream = (int)n;
        } else if (Rec()) MarkBad(BAD_RANGE, VI_SETSTREAM);
    }
    return ((F3)g_o[VI_SETSTREAM])(d, n, P(vb), stride);
}

static HRESULT STDMETHODCALLTYPE hkSetIndices(void *d, void *ib, DWORD base)
{
    if (Mine(d)) {
        if (ib) HookBufClass(ib);
        if (Rec()) {
            CmdP *c;
            Touch(EL_IDX);
            if ((c = (CmdP *)Emit(OP_IDX, sizeof *c)) != NULL) { c->p = ib; c->b = base; }
            Ref(ib);
        }
        g_sv[EL_IDX][0] = P(ib);
        g_sv[EL_IDX][1] = base;
        g_known[EL_IDX] = 1;
    }
    return ((F2)g_o[VI_SETINDICES])(d, P(ib), base);
}

static HRESULT STDMETHODCALLTYPE hkSetVS(void *d, DWORD h)
{
    if (Mine(d)) {
        if (Rec()) {
            CmdA3 *c;
            Touch(EL_VS);
            if ((c = (CmdA3 *)Emit(OP_VS, sizeof *c)) != NULL) c->a = h;
        }
        g_sv[EL_VS][0] = h;
        g_known[EL_VS] = 1;
    }
    return ((F1)g_o[VI_SETVS])(d, h);
}

static HRESULT STDMETHODCALLTYPE hkSetPS(void *d, DWORD h)
{
    if (Mine(d)) {
        if (Rec()) {
            CmdA3 *c;
            Touch(EL_PS);
            if ((c = (CmdA3 *)Emit(OP_PS, sizeof *c)) != NULL) c->a = h;
        }
        g_sv[EL_PS][0] = h;
        g_known[EL_PS] = 1;
    }
    return ((F1)g_o[VI_SETPS])(d, h);
}

static void RecConst(int ps, DWORD reg, const float *f, DWORD n)
{
    const DWORD lim = ps ? NPSC : NVSC;
    DWORD r;
    if (!f || !n) return;
    if (reg >= lim || n > lim - reg) {
        if (Rec()) MarkBad(BAD_RANGE, ps ? VI_SETPSC : VI_SETVSC);
        return;
    }
    if (Rec()) {
        CmdK *c;
        for (r = 0; r < n; ++r) Touch(ps ? EL_PSC(reg + r) : EL_VSC(reg + r));
        if ((c = (CmdK *)Emit(ps ? OP_PSC : OP_VSC, sizeof *c + n * 16)) != NULL) {
            c->reg = reg;
            c->n = n;
            c->u = -1;
            memcpy(c + 1, f, n * 16);
            if (!ps && g_nu < U_MAX) {
                Up *u = &g_up[g_nu];
                u->off = (DWORD)((BYTE *)(c + 1) - g_arena[g_cur].p);
                u->prevOff = 0;
                u->reg = reg;
                u->vs = g_sv[EL_VS][0];
                u->n = (WORD)n;
                u->st = ST_NONE;
                c->u = g_nu;
                if (g_np < (int)(sizeof g_pend / sizeof g_pend[0])) g_pend[g_np++] = g_nu;  // beyond: not paired
                g_nu++;
            }
        }
    }
    for (r = 0; r < n; ++r) {
        const int el = ps ? EL_PSC(reg + r) : EL_VSC(reg + r);
        memcpy(g_sv[el], f + r * 4, 16);
        g_known[el] = 1;
    }
}

static HRESULT STDMETHODCALLTYPE hkSetVSC(void *d, DWORD reg, const void *data, DWORD n)
{
    if (Mine(d)) RecConst(0, reg, (const float *)data, n);
    return ((F3)g_o[VI_SETVSC])(d, reg, P(data), n);
}

static HRESULT STDMETHODCALLTYPE hkSetPSC(void *d, DWORD reg, const void *data, DWORD n)
{
    if (Mine(d)) RecConst(1, reg, (const float *)data, n);
    return ((F3)g_o[VI_SETPSC])(d, reg, P(data), n);
}

static HRESULT STDMETHODCALLTYPE hkSetRT(void *d, void *rt, void *ds)
{
    if (Mine(d)) {
        if (Rec()) {
            CmdRT *c;
            Touch(EL_RT);
            Touch(EL_VP);
            if ((c = (CmdRT *)Emit(OP_RT, sizeof *c)) != NULL) { c->rt = rt; c->ds = ds; }
            Ref(rt);
            Ref(ds);
        }
        if (rt) {
            g_sv[EL_RT][0] = P(rt);
            g_known[EL_RT] = 1;
        }
        g_sv[EL_RT][1] = P(ds);
    }
    return ((F2)g_o[VI_SETRT])(d, P(rt), P(ds));
}

static HRESULT STDMETHODCALLTYPE hkSetViewport(void *d, const DWORD *vp)
{
    if (Mine(d) && Rec() && vp) {
        CmdVP *c;
        Touch(EL_VP);
        if ((c = (CmdVP *)Emit(OP_VP, sizeof *c)) != NULL) memcpy(c->vp, vp, sizeof c->vp);
    }
    return ((F1)g_o[VI_SETVIEWPORT])(d, P(vp));
}

// ---------------------------------------------------------------- hooks: drawing
static HRESULT STDMETHODCALLTYPE hkClear(void *d, DWORD count, const void *rects, DWORD flags, DWORD color, float z, DWORD stencil)
{
    if (Mine(d) && Rec()) {
        const DWORD n = rects ? count : 0;
        CmdClear *c;
        if (n > 4096) MarkBad(BAD_RANGE, VI_CLEAR);
        else if ((c = (CmdClear *)Emit(OP_CLEAR, sizeof *c + n * 16)) != NULL) {
            c->count = n;
            c->flags = flags;
            c->color = color;
            c->z = z;
            c->stencil = stencil;
            if (n) memcpy(c + 1, rects, n * 16);
        }
    }
    return ((HRESULT(STDMETHODCALLTYPE *)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD))g_o[VI_CLEAR])(d, count, rects, flags, color, z, stencil);
}

static HRESULT STDMETHODCALLTYPE hkBeginScene(void *d)
{
    if (Mine(d) && Rec()) Emit(OP_BEGIN, sizeof(CmdA3));
    return ((F0)g_o[VI_BEGINSCENE])(d);
}

static HRESULT STDMETHODCALLTYPE hkEndScene(void *d)
{
    if (Mine(d) && Rec()) Emit(OP_END, sizeof(CmdA3));
    return ((F0)g_o[VI_ENDSCENE])(d);
}

static HRESULT STDMETHODCALLTYPE hkDP(void *d, DWORD type, DWORD start, DWORD count)
{
    if (Mine(d) && Rec()) {
        CmdDraw *c;
        DrawUses(0);
        if ((c = (CmdDraw *)Emit(OP_DP, sizeof *c)) != NULL) {
            c->type = type; c->a = start; c->b = count; c->c = c->d = 0;
        }
        DiagDraw(_AddressOfReturnAddress(), OP_DP, g_sv[EL_STREAM(0)][1], count, count, NULL);
        PairPending(DrawSig(OP_DP, type, start, count, 0, 0, 1));
    }
    return ((F3)g_o[VI_DP])(d, type, start, count);
}

static HRESULT STDMETHODCALLTYPE hkDIP(void *d, DWORD type, DWORD minI, DWORD numV, DWORD start, DWORD prim)
{
    if (Mine(d) && Rec()) {
        CmdDraw *c;
        DrawUses(1);
        if ((c = (CmdDraw *)Emit(OP_DIP, sizeof *c)) != NULL) {
            c->type = type; c->a = minI; c->b = numV; c->c = start; c->d = prim;
        }
        DiagDraw(_AddressOfReturnAddress(), OP_DIP, g_sv[EL_STREAM(0)][1], numV, prim, NULL);
        PairPending(DrawSig(OP_DIP, type, minI, numV, start, prim, 1));
    }
    return ((F5)g_o[VI_DIP])(d, type, minI, numV, start, prim);
}

static DWORD VertexCount(DWORD type, DWORD prim)
{
    switch (type) {
    case 1: return prim;          // point list
    case 2: return prim * 2;      // line list
    case 3: return prim + 1;      // line strip
    case 4: return prim * 3;      // triangle list
    case 5: case 6: return prim + 2;  // strip, fan
    }
    return 0;
}

static HRESULT STDMETHODCALLTYPE hkDPUP(void *d, DWORD type, DWORD prim, const void *data, DWORD stride)
{
    HRESULT hr;
    const int mine = Mine(d);
    if (mine && Rec()) {
        const unsigned __int64 bytes = (unsigned __int64)VertexCount(type, prim) * stride;
        CmdDPUP *c;
        Touch(EL_STREAM(0));
        if (!data || bytes > (16u << 20)) MarkBad(BAD_RANGE, VI_DPUP);
        else if ((c = (CmdDPUP *)Emit(OP_DPUP, sizeof *c + (unsigned)bytes)) != NULL) {
            c->type = type; c->prim = prim; c->stride = stride; c->vbytes = (DWORD)bytes;
            memcpy(c + 1, data, (size_t)bytes);
        }
        DiagDraw(_AddressOfReturnAddress(), OP_DPUP, stride, VertexCount(type, prim), prim, data);
        PairPending(DrawSig(OP_DPUP, type, prim, stride, 0, 0, 0));
    }
    hr = ((F4)g_o[VI_DPUP])(d, type, prim, P(data), stride);
    if (mine) {  // Direct3D 8: stream 0 is unset after a UP draw
        g_sv[EL_STREAM(0)][0] = g_sv[EL_STREAM(0)][1] = 0;
        g_known[EL_STREAM(0)] = 1;
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkDIPUP(void *d, DWORD type, DWORD minI, DWORD numV, DWORD prim, const void *idx, DWORD ifmt,
                                         const void *vtx, DWORD stride)
{
    HRESULT hr;
    const int mine = Mine(d);
    if (mine && Rec()) {
        const unsigned __int64 ib = (unsigned __int64)VertexCount(type, prim) * (ifmt == 101 ? 2 : 4);  // D3DFMT_INDEX16
        const unsigned __int64 vb = (unsigned __int64)(minI + numV) * stride;
        CmdDIPUP *c;
        Touch(EL_STREAM(0));
        Touch(EL_IDX);
        if (!idx || !vtx || ib + vb > (16u << 20)) MarkBad(BAD_RANGE, VI_DIPUP);
        else if ((c = (CmdDIPUP *)Emit(OP_DIPUP, sizeof *c + (((unsigned)ib + 3) & ~3u) + (unsigned)vb)) != NULL) {
            c->type = type; c->minI = minI; c->numV = numV; c->prim = prim; c->ifmt = ifmt; c->stride = stride;
            c->ibytes = (DWORD)ib; c->vbytes = (DWORD)vb;
            memcpy(c + 1, idx, (size_t)ib);
            memcpy((BYTE *)(c + 1) + ((ib + 3) & ~3u), vtx, (size_t)vb);
        }
        DiagDraw(_AddressOfReturnAddress(), OP_DIPUP, stride, numV, prim, (const BYTE *)vtx + minI * stride);
        PairPending(DrawSig(OP_DIPUP, type, minI, numV, prim, stride, 0));
    }
    hr = ((F8)g_o[VI_DIPUP])(d, type, minI, numV, prim, P(idx), ifmt, P(vtx), stride);
    if (mine) {  // stream 0 and the indices are unset after it
        g_sv[EL_STREAM(0)][0] = g_sv[EL_STREAM(0)][1] = 0;
        g_known[EL_STREAM(0)] = 1;
        g_sv[EL_IDX][0] = g_sv[EL_IDX][1] = 0;
        g_known[EL_IDX] = 1;
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkCopyRects(void *d, void *src, const RECT *rects, DWORD n, void *dst, const POINT *pts)
{
    if (Mine(d) && Rec()) {
        CmdCopy *c;
        const DWORD m = rects ? n : 0;
        if (m > 1024) MarkBad(BAD_RANGE, VI_COPYRECTS);
        else if ((c = (CmdCopy *)Emit(OP_COPY, sizeof *c + m * (sizeof(RECT) + sizeof(POINT)))) != NULL) {
            c->src = src; c->dst = dst; c->n = m; c->hasRects = rects != NULL; c->hasPoints = pts != NULL && m;
            if (m) memcpy(c + 1, rects, m * sizeof(RECT));
            if (m && pts) memcpy((BYTE *)(c + 1) + m * sizeof(RECT), pts, m * sizeof(POINT));
            Ref(src);
            Ref(dst);
        }
    }
    return ((F5)g_o[VI_COPYRECTS])(d, P(src), P(rects), n, P(dst), P(pts));
}

// ---------------------------------------------------------------- hooks: methods the replay cannot reproduce
static void Unsafe(void *d, int i)
{
    if (Mine(d) && Rec()) MarkBad(BAD_METHOD, i);
}
#define UNSAFE0(i) static HRESULT STDMETHODCALLTYPE hkU##i(void *d) { Unsafe(d, i); return ((F0)g_o[i])(d); }
#define UNSAFE1(i) static HRESULT STDMETHODCALLTYPE hkU##i(void *d, DWORD a) { Unsafe(d, i); return ((F1)g_o[i])(d, a); }
#define UNSAFE2(i) static HRESULT STDMETHODCALLTYPE hkU##i(void *d, DWORD a, DWORD b) { Unsafe(d, i); return ((F2)g_o[i])(d, a, b); }
#define UNSAFE3(i) static HRESULT STDMETHODCALLTYPE hkU##i(void *d, DWORD a, DWORD b, DWORD c) { Unsafe(d, i); return ((F3)g_o[i])(d, a, b, c); }
#define UNSAFE5(i) static HRESULT STDMETHODCALLTYPE hkU##i(void *d, DWORD a, DWORD b, DWORD c, DWORD e, DWORD f) { Unsafe(d, i); return ((F5)g_o[i])(d, a, b, c, e, f); }
UNSAFE2(37) UNSAFE2(39) UNSAFE1(42) UNSAFE2(44) UNSAFE2(46) UNSAFE2(48) UNSAFE0(52) UNSAFE1(53) UNSAFE1(54) UNSAFE1(55)
UNSAFE2(57) UNSAFE1(58) UNSAFE2(66) UNSAFE1(68) UNSAFE5(74) UNSAFE3(94) UNSAFE3(95) UNSAFE1(96)

static HRESULT STDMETHODCALLTYPE hkDeleteVS(void *d, DWORD h)
{
    if (Mine(d) && Rec()) MarkBad(BAD_SHADER, VI_DELETEVS);
    return ((F1)g_o[VI_DELETEVS])(d, h);
}
static HRESULT STDMETHODCALLTYPE hkDeletePS(void *d, DWORD h)
{
    if (Mine(d) && Rec()) MarkBad(BAD_SHADER, VI_DELETEPS);
    return ((F1)g_o[VI_DELETEPS])(d, h);
}
static HRESULT STDMETHODCALLTYPE hkUpdateTexture(void *d, void *src, void *dst)
{
    if (Mine(d) && Rec() && PFind(g_texSet, TEX_N, g_hgen, dst, 0, NULL)) MarkBad(BAD_TEXLOCK, VI_UPDATETEXTURE);
    return ((F2)g_o[VI_UPDATETEXTURE])(d, P(src), P(dst));
}
static HRESULT STDMETHODCALLTYPE hkGetFrontBuffer(void *d, void *surf)
{
    static int logged;
    if (Mine(d) && !logged++) LOG("interp: the game read the front buffer back (GetFrontBuffer); harmless, logged once");
    return ((F1)g_o[VI_GETFRONTBUFFER])(d, P(surf));
}

// ---------------------------------------------------------------- resource hooks: buffer Lock / Unlock, texture LockRect
typedef HRESULT(STDMETHODCALLTYPE *Lock_t)(void *, DWORD, DWORD, void **, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *TexLock_t)(void *, DWORD, void *, const RECT *, DWORD);

static BufClass *FindBufClass(void *o)
{
    int i;
    for (i = 0; i < 8 && g_bufc[i].vt; ++i)
        if (g_bufc[i].vt == VT(o)) return &g_bufc[i];
    return NULL;
}

static DWORD BufSize(void *b)
{
    DWORD desc[6] = { 0 };  // D3DVERTEXBUFFER_DESC / D3DINDEXBUFFER_DESC: Format, Type, Usage, Pool, Size (, FVF)
    return SUCCEEDED(((F1)VT(b)[RV_GETDESC])(b, P(desc))) ? desc[4] : 0;
}

static HRESULT STDMETHODCALLTYPE hkBufLock(void *b, DWORD off, DWORD size, void **pp, DWORD flags)
{
    BufClass *bc = FindBufClass(b);
    HRESULT hr = bc ? ((Lock_t)bc->lock)(b, off, size, pp, flags) : E_FAIL;
    if (SUCCEEDED(hr) && pp && *pp && g_rec && !g_stop && !g_inOwn && GetCurrentThreadId() == g_tid && !(flags & D3DLOCK_READONLY)) {
        int i;
        PSlot *s = BufInfo(b);
        if (!size) {
            const DWORD total = BufSize(b);
            size = total > off ? total - off : 0;
        }
        if (s && (s->a & 1)) {  // drawn from earlier in this frame: the replay needs the old bytes first
            if (flags & D3DLOCK_DISCARD) MarkBad(BAD_DISCARD, -1);
            else if (g_npre >= (int)(sizeof g_preOff / sizeof g_preOff[0])) MarkBad(BAD_BIG, -1);
            else {
                CmdBuf *c = (CmdBuf *)Grow(&g_pre, sizeof *c + size);
                if (!c) MarkBad(BAD_BIG, -1);
                else {
                    c->h.op = OP_BUF; c->h.fl = 0; c->h.size = (sizeof *c + size + 3) & ~3u;
                    c->buf = b; c->off = off; c->size = size; c->flags = 0;
                    memcpy(c + 1, *pp, size);
                    g_preOff[g_npre++] = (DWORD)((BYTE *)c - g_pre.p);
                    s->a |= 2;
                    Ref(b);
                }
            }
        }
        for (i = 0; i < 16; ++i)
            if (!g_locks[i].buf) {
                g_locks[i].buf = b; g_locks[i].ptr = *pp; g_locks[i].off = off; g_locks[i].size = size; g_locks[i].flags = flags;
                break;
            }
        if (i == 16) MarkBad(BAD_RANGE, -1);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkBufUnlock(void *b)
{
    BufClass *bc = FindBufClass(b);
    int i;
    for (i = 0; i < 16; ++i)
        if (g_locks[i].buf == b) {
            ActiveLock l = g_locks[i];
            g_locks[i].buf = NULL;
            if (g_rec && !g_stop && GetCurrentThreadId() == g_tid) {
                CmdBuf *c = (CmdBuf *)Emit(OP_BUF, sizeof *c + l.size);
                if (c) {
                    PSlot *s = BufInfo(b);
                    int isNew;
                    c->buf = b; c->off = l.off; c->size = l.size; c->flags = l.flags & D3DLOCK_DISCARD;
                    memcpy(c + 1, l.ptr, l.size);
                    DiagBuf(_AddressOfReturnAddress(), b, l.off, l.size, l.ptr);
                    Ref(b);
                    if (!s && g_nbuf < BUF_N / 2 && (s = PFind(g_bufInfo, BUF_N, g_hgen, b, 1, &isNew)) != NULL && isNew) g_nbuf++;
                    if (s) s->b++;
                    else MarkBad(BAD_REFS, -1);
                }
            }
            break;
        }
    return bc ? ((F0)bc->unlock)(b) : E_FAIL;
}

static void SetVtbl(void **slot, void *fn)
{
    DWORD old;
    if (*slot == fn) return;
    if (VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) {
        *slot = fn;
        VirtualProtect(slot, sizeof(void *), old, &old);
    }
}

static void HookBufClass(void *obj)
{
    void **vt = VT(obj);
    int i;
    for (i = 0; i < 8; ++i) {
        if (g_bufc[i].vt == vt) return;
        if (!g_bufc[i].vt) break;
    }
    if (i == 8) return;
    if (vt[RV_LOCK] == (void *)hkBufLock) return;  // another class entry already patched this vtable
    g_bufc[i].lock = vt[RV_LOCK];
    g_bufc[i].unlock = vt[RV_UNLOCK];
    g_bufc[i].vt = vt;
    SetVtbl(&vt[RV_LOCK], (void *)hkBufLock);
    SetVtbl(&vt[RV_UNLOCK], (void *)hkBufUnlock);
}

static HRESULT STDMETHODCALLTYPE hkTexLock(void *t, DWORD level, void *lr, const RECT *r, DWORD flags)
{
    int i;
    void *orig = NULL;
    for (i = 0; i < 8 && g_texc[i].vt; ++i)
        if (g_texc[i].vt == VT(t)) orig = g_texc[i].lockRect;
    if (g_rec && !g_stop && !g_inOwn && GetCurrentThreadId() == g_tid && !(flags & D3DLOCK_READONLY) &&
        PFind(g_texSet, TEX_N, g_hgen, t, 0, NULL))
        MarkBad(BAD_TEXLOCK, -1);
    return orig ? ((TexLock_t)orig)(t, level, lr, r, flags) : E_FAIL;
}

static void HookTexClass(void *obj)
{
    void **vt = VT(obj);
    int i;
    for (i = 0; i < 8; ++i) {
        if (g_texc[i].vt == vt) return;
        if (!g_texc[i].vt) break;
    }
    if (i == 8 || vt[TV_LOCKRECT] == (void *)hkTexLock) return;
    g_texc[i].lockRect = vt[TV_LOCKRECT];
    g_texc[i].vt = vt;
    SetVtbl(&vt[TV_LOCKRECT], (void *)hkTexLock);
}

static HRESULT STDMETHODCALLTYPE hkCreateTexture(void *d, DWORD w, DWORD h, DWORD lv, DWORD usage, DWORD fmt, DWORD pool, void **out)
{
    HRESULT hr = ((F7)g_o[VI_CREATETEXTURE])(d, w, h, lv, usage, fmt, pool, P(out));
    if (SUCCEEDED(hr) && out && *out && GetCurrentThreadId() == g_tid) HookTexClass(*out);
    return hr;
}
static HRESULT STDMETHODCALLTYPE hkCreateVB(void *d, DWORD len, DWORD usage, DWORD fvf, DWORD pool, void **out)
{
    HRESULT hr = ((F5)g_o[VI_CREATEVB])(d, len, usage, fvf, pool, P(out));
    if (SUCCEEDED(hr) && out && *out && GetCurrentThreadId() == g_tid) HookBufClass(*out);
    return hr;
}
static HRESULT STDMETHODCALLTYPE hkCreateIB(void *d, DWORD len, DWORD usage, DWORD fmt, DWORD pool, void **out)
{
    HRESULT hr = ((F5)g_o[VI_CREATEIB])(d, len, usage, fmt, pool, P(out));
    if (SUCCEEDED(hr) && out && *out && GetCurrentThreadId() == g_tid) HookBufClass(*out);
    return hr;
}

// ---------------------------------------------------------------- replay
static void BufWrite(const CmdBuf *c)
{
    BufClass *bc = FindBufClass(c->buf);
    void *p = NULL;
    if (!bc || !c->size) return;
    if (SUCCEEDED(((Lock_t)bc->lock)(c->buf, c->off, c->size, &p, c->flags)) && p) {
        memcpy(p, c + 1, c->size);
        ((F0)bc->unlock)(c->buf);
    }
}

// one pass over the record; a = fraction of the way from the previous frame (>= 1: the real values)
static void ReplayBody(float a)
{
    const Arena *ar = &g_arena[g_cur], *pv = &g_arena[g_cur ^ 1];
    void *d = g_dev;
    unsigned off;
    int i, scene = 0;
    float tmp[NVSC * 4];
    for (i = g_npre - 1; i >= 0; --i) BufWrite((const CmdBuf *)(g_pre.p + g_preOff[i]));  // the buffers' old bytes
    RestoreState(1);
    for (off = 0; off < ar->n; off += ((const Cmd *)(ar->p + off))->size) {
        const Cmd *h = (const Cmd *)(ar->p + off);
        switch (h->op) {
        case OP_RS: ((F2)g_o[VI_SETRS])(d, ((const CmdA3 *)h)->a, ((const CmdA3 *)h)->b); break;
        case OP_TSS: ((F3)g_o[VI_SETTSS])(d, ((const CmdA3 *)h)->a, ((const CmdA3 *)h)->b, ((const CmdA3 *)h)->c); break;
        case OP_TEX: ((F2)g_o[VI_SETTEXTURE])(d, ((const CmdP *)h)->a, P(((const CmdP *)h)->p)); break;
        case OP_STREAM: ((F3)g_o[VI_SETSTREAM])(d, ((const CmdP *)h)->a, P(((const CmdP *)h)->p), ((const CmdP *)h)->b); break;
        case OP_IDX: ((F2)g_o[VI_SETINDICES])(d, P(((const CmdP *)h)->p), ((const CmdP *)h)->b); break;
        case OP_VS: ((F1)g_o[VI_SETVS])(d, ((const CmdA3 *)h)->a); break;
        case OP_PS: ((F1)g_o[VI_SETPS])(d, ((const CmdA3 *)h)->a); break;
        case OP_VSC: {
            const CmdK *k = (const CmdK *)h;
            const float *f = (const float *)(k + 1);
            if (a < 1.0f && k->u >= 0 && g_up[k->u].st == ST_MOVE) {
                const Up *u = &g_up[k->u];
                Blend(tmp, f, (const float *)(pv->p + u->prevOff), u->n, a);
                f = tmp;
            }
            ((F3)g_o[VI_SETVSC])(d, k->reg, P(f), k->n);
            break;
        }
        case OP_PSC: ((F3)g_o[VI_SETPSC])(d, ((const CmdK *)h)->reg, P(((const CmdK *)h) + 1), ((const CmdK *)h)->n); break;
        case OP_RT: ((F2)g_o[VI_SETRT])(d, P(((const CmdRT *)h)->rt), P(((const CmdRT *)h)->ds)); break;
        case OP_VP: ((F1)g_o[VI_SETVIEWPORT])(d, P(((const CmdVP *)h)->vp)); break;
        case OP_CLEAR: {
            const CmdClear *c = (const CmdClear *)h;
            ((HRESULT(STDMETHODCALLTYPE *)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD))g_o[VI_CLEAR])(
                d, c->count, c->count ? (const void *)(c + 1) : NULL, c->flags, c->color, c->z, c->stencil);
            break;
        }
        case OP_BEGIN: ((F0)g_o[VI_BEGINSCENE])(d); scene++; break;
        case OP_END: ((F0)g_o[VI_ENDSCENE])(d); scene--; break;
        case OP_DP: ((F3)g_o[VI_DP])(d, ((const CmdDraw *)h)->type, ((const CmdDraw *)h)->a, ((const CmdDraw *)h)->b); break;
        case OP_DIP: {
            const CmdDraw *c = (const CmdDraw *)h;
            ((F5)g_o[VI_DIP])(d, c->type, c->a, c->b, c->c, c->d);
            break;
        }
        case OP_DPUP: {
            const CmdDPUP *c = (const CmdDPUP *)h;
            ((F4)g_o[VI_DPUP])(d, c->type, c->prim, P(c + 1), c->stride);
            break;
        }
        case OP_DIPUP: {
            const CmdDIPUP *c = (const CmdDIPUP *)h;
            const BYTE *ix = (const BYTE *)(c + 1), *vx = ix + ((c->ibytes + 3) & ~3u);
            ((F8)g_o[VI_DIPUP])(d, c->type, c->minI, c->numV, c->prim, P(ix), c->ifmt, P(vx), c->stride);
            break;
        }
        case OP_COPY: {
            const CmdCopy *c = (const CmdCopy *)h;
            const BYTE *r = (const BYTE *)(c + 1);
            ((F5)g_o[VI_COPYRECTS])(d, P(c->src), c->hasRects ? P(r) : 0, c->n, P(c->dst),
                                    c->hasPoints ? P(r + c->n * sizeof(RECT)) : 0);
            break;
        }
        case OP_BUF: if (!(h->fl & CF_SKIP)) BufWrite((const CmdBuf *)h); break;
        }
    }
    while (scene-- > 0) ((F0)g_o[VI_ENDSCENE])(d);
    RestoreState(0);
}

static int Replay(float a)
{
    int ok = 1;
    g_inOwn++;
    __try {
        ReplayBody(a);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
    }
    g_inOwn--;
    return ok;
}

// ---------------------------------------------------------------- the real frame aside and back
static void DropSaved(void)
{
    if (g_savedSurf) ((FRef)VT(g_savedSurf)[VI_RELEASE])(g_savedSurf);
    if (g_savedTex) ((FRef)VT(g_savedTex)[VI_RELEASE])(g_savedTex);
    g_savedSurf = g_savedTex = NULL;
}

static void DropAll(void)
{
    int i;
    ReleaseRefs();
    DropSaved();
    g_rec = g_stop = g_prevOk = 0;
    g_npre = 0;
    for (i = 0; i < 16; ++i) g_locks[i].buf = NULL;
    memset(g_known, 0, sizeof g_known);
}

static void Disable(const char *why, HRESULT hr)
{
    if (g_disabled) return;
    g_disabled = 1;
    LOG("interp: OFF for this session: %s (hr %08lX); every frame is shown once (30 per second)", why, (unsigned long)hr);
    DropAll();
}

static int SaveBackBuffer(void)
{
    void *d = g_dev, *bb = NULL;
    DWORD desc[8] = { 0 };  // D3DSURFACE_DESC: Format, Type, Usage, Pool, Size, MultiSampleType, Width, Height
    HRESULT hr = ((F3)g_o[VI_GETBACKBUFFER])(d, 0, 0, P(&bb));
    if (FAILED(hr) || !bb) { Disable("GetBackBuffer failed", hr); return 0; }
    hr = ((F1)VT(bb)[SV_GETDESC])(bb, P(desc));
    if (FAILED(hr) || desc[5]) {
        ReleaseObj(bb);
        Disable(FAILED(hr) ? "the back buffer's description cannot be read" : "the back buffer is multisampled", hr);
        return 0;
    }
    if (!g_savedSurf || g_savedW != desc[6] || g_savedH != desc[7] || g_savedFmt != desc[0]) {
        DropSaved();
        hr = ((F7)g_o[VI_CREATETEXTURE])(d, desc[6], desc[7], 1, 1 /* RENDERTARGET */, desc[0], 0 /* DEFAULT */, P(&g_savedTex));
        if (SUCCEEDED(hr) && g_savedTex) hr = ((F2)VT(g_savedTex)[TV_GETSURFACE])(g_savedTex, 0, P(&g_savedSurf));
        if (FAILED(hr) || !g_savedSurf) {
            ReleaseObj(bb);
            DropSaved();
            Disable("no texture for the real frame", hr);
            return 0;
        }
        g_savedW = desc[6];
        g_savedH = desc[7];
        g_savedFmt = desc[0];
        g_warm = WARM_TICKS;  // a new texture of this size: the first ticks with it are not the steady state
        LOG("interp: real frames are kept in a %lux%lu texture (format %lu) while the in-between frames show", desc[6], desc[7], desc[0]);
    }
    hr = ((F5)g_o[VI_COPYRECTS])(d, P(bb), 0, 0, P(g_savedSurf), 0);
    ReleaseObj(bb);
    if (FAILED(hr)) { Disable("copying the back buffer aside failed", hr); return 0; }
    return 1;
}

static int RestoreReal(void)
{
    void *d = g_dev, *bb = NULL;
    HRESULT hr;
    if (g_copyBack) {
        hr = ((F3)g_o[VI_GETBACKBUFFER])(d, 0, 0, P(&bb));
        if (SUCCEEDED(hr) && bb) {
            hr = ((F5)g_o[VI_COPYRECTS])(d, P(g_savedSurf), 0, 0, P(bb), 0);
            ReleaseObj(bb);
            if (SUCCEEDED(hr)) return 1;
        }
        g_copyBack = 0;
        LOG("interp: copying the real frame back failed (hr %08lX): the real frame is replayed from the record instead", (unsigned long)hr);
    }
    return Replay(1.0f);
}

// ---------------------------------------------------------------- frame boundaries
static void StartRecording(void)
{
    int i;
    if (++g_gen == 0) g_gen = 1;
    g_ndirty = 0;
    g_bad = 0;
    g_stop = 0;
    g_nu = g_np = 0;
    PairBeginFrame(g_cur);
    g_npre = 0;
    g_pre.n = 0;
    g_arena[g_cur].n = 0;
    if (++g_hgen == 0) g_hgen = 1;
    g_nbuf = g_ntex = 0;
    g_vpEndOk = 0;
    DiagFrameStart();
    for (i = 0; i < 16; ++i) g_locks[i].buf = NULL;
    g_rec = !g_disabled && g_k > 1 && g_arena[0].p && g_arena[1].p && (!H.gameplay || H.gameplay());
    if (g_rec)
        for (i = 0; i < 8; ++i)  // textures bound from before the frame count as used
            if (g_known[EL_TEX(i)] && g_sv[EL_TEX(i)][0]) TexMark(PTR(g_sv[EL_TEX(i)][0]));
}

static int KOfRate(int r) { return r >= 180 ? 6 : r >= 120 ? 4 : r >= 60 ? 2 : 1; }
static int LowerK(int k) { return k >= 6 ? 4 : k >= 4 ? 2 : 1; }
static int RaiseK(int k) { return k <= 1 ? 2 : k <= 2 ? 4 : 6; }

// Once per presented tick (recorded or not). "Too slow" = the game's own ticks stretch: the spacing of consecutive tick
// starts ([0x13DC390]) after interpolated ticks, averaged over WIN_TICKS steady ticks, against the step. A Present that
// now and then runs into the next tick (a vsync wait, a hitch) does not count: the game's loop catches up (deadline +=
// step) and the average stays at the step; only lost logic time (the loop resetting its deadline) raises it. Ticks
// after a hitch (one spacing over 3 steps), a gap, menus, a reset, a rate change or the attach are not judged.
static void Judge(int gameplay, int timing, unsigned __int64 fs, unsigned __int64 step, int interpolated)
{
    if (!gameplay || !timing) {
        g_warm = WARM_TICKS;
        g_prevFsOk = 0;
        return;
    }
    if (g_prevFsOk && fs > g_prevFs) {
        const unsigned __int64 span = fs - g_prevFs;
        S.spanSum += span;
        S.spanN++;
        if (span > 3 * step) g_warm = WARM_TICKS;  // a hitch (a load, something used for the first time)
        else if (!g_warm && g_prevInterp && g_k > 1) {
            g_winSpan += span;
            g_winStep += step;
            g_winN++;
        }
    }
    g_prevFs = fs;
    g_prevFsOk = 1;
    g_prevInterp = interpolated;
    if (g_warm) g_warm--;
    if (g_winN >= WIN_TICKS) {
        if (g_autoLower && g_k > 1 && g_winSpan * 100 > g_winStep * 105) {
            const double ms = (double)g_winSpan * 1000.0 / (double)g_qpf / g_winN, want = (double)g_winStep * 1000.0 / (double)g_qpf / g_winN;
            if (g_raised) g_retryWait = g_retryWait < 8 * RETRY_TICKS ? g_retryWait * 2 : 8 * RETRY_TICKS;  // failed again
            g_k = LowerK(g_k);
            g_raised = 0;
            g_lowTicks = 0;
            g_warm = WARM_TICKS;
            LOG("interp: the game's ticks stretched to %.2f ms on average (%.2f expected) over %d interpolated ticks: display rate "
                "lowered to %d; one step up is tried again after %d s of play", ms, want, g_winN, g_k * 30, g_retryWait / 30);
        }
        g_winN = 0;
        g_winSpan = g_winStep = 0;
    }
    if (g_raised && ++g_heldTicks >= RETRY_TICKS) {  // the raised rate held a minute: back to the short retry
        g_raised = 0;
        g_retryWait = RETRY_TICKS;
    }
    if (g_k < g_kMax && ++g_lowTicks >= g_retryWait) {
        g_k = RaiseK(g_k);
        g_lowTicks = 0;
        g_raised = 1;
        g_heldTicks = 0;
        g_warm = WARM_TICKS;
        g_winN = 0;
        g_winSpan = g_winStep = 0;
        LOG("interp: trying display rate %d again", g_k * 30);
    }
}

// The presents of a tick: picture j at base + (j - 1) * spacing, j = 1 .. k (k = the real frame). base is never before
// the first picture is ready; what is left of the tick, W = next tick - base, is all the time there is (the frame's own
// logic and drawing are already spent). Spacing = step / k while that fits, else W / (k - 1): k pictures have k - 1
// gaps, the real frame may sit right before the next tick.
// v2.2: v2.1 divided by k (a gap kept after the real frame "for a vsync wait"): once the game needed more than a
// quarter of the tick (k = 2) the two pictures bunched up (the "squeezed" count, 250-700 of 900 ticks in the user's
// Room 302 / world play) and the in-between picture often shared a refresh with the real one: it never showed.
// [Window] InterpPacing=0 (not in the default ini) brings back the v2.1 rule for comparison.
static int g_pacingV21;
static unsigned __int64 Spacing(unsigned __int64 *base, unsigned __int64 now, unsigned __int64 next, unsigned __int64 P, int k)
{
    unsigned __int64 w;
    const unsigned gaps = g_pacingV21 ? (unsigned)k : (unsigned)(k > 1 ? k - 1 : 1);
    if (*base < now) *base = now;
    w = next > *base ? (next - *base) / gaps : 0;
    return w < P ? w : P;
}
void Interp_SetPacing(int v21) { g_pacingV21 = v21; }
void Interp_SetMatrixRule(int on) { g_pmMatrixRule = on; }

static void Stats(int gameplay)
{
    if (!gameplay) return;
    if (++g_logTicks < 900) return;
    g_logTicks = 0;
    LOG("interp: last 900 ticks at %d fps (chosen %d): %u recorded, %u interpolated, %u still, %u cuts, %u few pairs, %u unsafe "
        "(%u too big), %u no timing, %u not recorded; %u squeezed (spacing under 3/4 of step / k), %u overran into the next tick; "
        "tick spacing %.2f ms avg; record %u KB avg / %u KB max; replay %.2f ms avg; %u presents%s",
        g_k * 30, g_kMax * 30, S.ticks, S.interp, S.still, S.cuts, S.pairs, S.bad, S.big, S.nottime, S.plain, S.squeezed, S.overran,
        S.spanN ? (double)S.spanSum * 1000.0 / (double)g_qpf / S.spanN : 0.0, S.ticks ? S.kb / S.ticks : 0, S.kbMax,
        S.interp ? (double)S.replay * 1000.0 / (double)g_qpf / (double)S.interp : 0.0, S.presents,
        g_k < g_kMax ? "; lowered, one step up is tried later" : "");
    if (g_diag == 1) LOG("interp diag: %u refresh intervals got a new picture in those 900 ticks (%.1f per second)", g_ivals, g_ivals / 30.0);
    g_ivals = 0;
    memset(&S, 0, sizeof S);
}

static void MarkSkips(void)
{
    // a buffer written once in the frame and never drawn from before that write already holds those bytes
    const Arena *ar = &g_arena[g_cur];
    unsigned off;
    for (off = 0; off < ar->n; off += ((const Cmd *)(ar->p + off))->size) {
        Cmd *h = (Cmd *)(ar->p + off);
        if (h->op == OP_BUF) {
            PSlot *s = BufInfo(((CmdBuf *)h)->buf);
            if (s && s->b == 1 && !(s->a & 2)) h->fl |= CF_SKIP;
        }
    }
}

typedef HRESULT(STDMETHODCALLTYPE *Present_t)(void *, const RECT *, const RECT *, HWND, const void *);

static HRESULT FinishFrame(void *d, const RECT *src, const RECT *dst, HWND wnd, const void *dirty)
{
    const Present_t present = (Present_t)g_o[VI_PRESENT];
    const unsigned __int64 t0 = Qpc();
    unsigned __int64 fs = 0, next = 0, step = 0, P = 0, base = 0;
    const int k = g_k, gameplay = !H.gameplay || H.gameplay();
    const unsigned tick = H.tickId ? H.tickId() : 0;
    int timing, why = -1, interp = 0, slotted, j, lost = 0;
    unsigned __int64 sp = 0;
    HRESULT hr = S_OK;
    timing = H.tick && H.tick(&fs, &next, &step) && step > 0 && next > fs && t0 >= fs;
    slotted = k > 1 && gameplay && timing && !g_disabled;
    if (g_rec) {
        if (g_bad) why = 100;
        else if (!gameplay) why = 101;
        else if (!g_prevOk || (H.tickId && tick != g_prevTick + 1)) why = 102;
        else if (!timing && !g_noWait) why = 103;
        else why = Decide();
        interp = why == W_OK && k > 1 && !g_disabled;
        DiagDump(why, k);
        if (gameplay) {
            const unsigned kb = (g_arena[g_cur].n + g_pre.n) >> 10;
            S.ticks++;
            S.kb += kb;
            if (kb > S.kbMax) S.kbMax = kb;
            if (why == W_STILL) S.still++;
            else if (why == W_PAIRS) S.pairs++;
            else if (why == W_CUT) S.cuts++;
            else if (why == 100) { S.bad++; if (g_bad & BAD_BIG) S.big++; }
            else if (why == 103) S.nottime++;
        }
    }
    if (slotted) {  // base: the phase of the first picture after the tick start, smoothed so the spacing stays even
        const unsigned __int64 ready = (t0 - fs) + g_replayEst + g_qpf / 2000;
        P = step / (unsigned)k;
        // review v2.1 H1: a tick that overran its step (a load, a first-use hitch) leaves the phase alone. Taken up, it
        // made every following tick wait until fs + phi (past its own end), decaying by 1/64 per tick: seconds of slow motion.
        if (ready >= step) {
        } else if (ready > g_phi) g_phi = ready;  // up at once, down slowly
        else g_phi -= (g_phi - ready) / 64;
        base = fs + g_phi;
    }
    g_inOwn++;
    // review v2.1 M1: a lost device (lock screen, UAC, mode change) shows the frame once; without this the first failing
    // copy / texture creation while lost turned the interpolation off for the whole session (Disable)
    if (interp && ((F0)g_o[3 /* TestCooperativeLevel */])(d) != S_OK) interp = 0;
    if (interp) {
        if (!g_vpEndOk) g_vpEndOk = SUCCEEDED(((F1)g_o[VI_GETVIEWPORT])(d, P(g_vpEnd)));
        MarkSkips();
        if (!SaveBackBuffer()) interp = 0;
    }
    if (interp) {
        for (j = 1; j < k; ++j) {
            const unsigned __int64 r0 = Qpc();
            unsigned __int64 r1;
            if (!Replay((float)j / (float)k)) {
                g_inOwn++;  // (already inside) the real frame back by copy only: the record is not trusted now
                if (g_copyBack && g_savedSurf) {
                    void *bb = NULL;
                    if (SUCCEEDED(((F3)g_o[VI_GETBACKBUFFER])(d, 0, 0, P(&bb))) && bb) {
                        ((F5)g_o[VI_COPYRECTS])(d, P(g_savedSurf), 0, 0, P(bb), 0);
                        ReleaseObj(bb);
                    }
                }
                g_inOwn--;
                Disable("an exception in a replay", 0);
                interp = 0;
                break;
            }
            r1 = Qpc();
            S.replay += r1 - r0;
            g_dt.rep += r1 - r0;
            g_replayEst = g_replayEst ? (g_replayEst * 7 + (r1 - r0)) / 8 : r1 - r0;
            if (slotted) {
                if (j == 1) {
                    sp = Spacing(&base, r1, next, P, k);
                    if (sp < P * 3 / 4) S.squeezed++;
                }
                WaitUntil(base + (j - 1) * sp);
            }
            if (g_diag == 1 && j == 1) { g_dt.p1s = Qpc(); g_dt.sq = sp < P * 3 / 4; }
            hr = present(d, src, dst, wnd, dirty);
            if (g_diag == 1) { const unsigned __int64 e = Qpc(); if (j == 1) { g_dt.p1e = e; g_dt.iv1 = DiagInterval(e); } DiagCountPicture(e); }
            S.presents++;
            if (FAILED(hr)) { lost = 1; break; }
        }
        if (interp && !lost) {
            if (!RestoreReal()) Disable("an exception in a replay", 0);
            S.interp++;
        }
    }
    if (!lost) {
        if (slotted) {
            if (!sp) sp = Spacing(&base, Qpc(), next, P, k);  // not interpolated: the real frame keeps its place in the cadence
            WaitUntil(base + (k - 1) * sp);
        }
        if (g_diag == 1) g_dt.p2s = Qpc();
        hr = present(d, src, dst, wnd, dirty);
        if (g_diag == 1) { g_dt.p2e = Qpc(); g_dt.iv2 = DiagInterval(g_dt.p2e); DiagCountPicture(g_dt.p2e); }
        S.presents++;
        if (slotted && Qpc() > next) S.overran++;
    }
    g_inOwn--;
    if (why == 102) g_warm = WARM_TICKS;  // a gap in the ticks
    Judge(gameplay, timing, fs, step, interp && !lost);
    ReleaseRefs();
    g_prevOk = g_rec;
    if (g_rec) {
        g_prevTick = tick;
        g_cur ^= 1;  // this frame's constants become the previous frame's
    }
    if (lost) g_prevOk = 0;
    if (g_diag == 1 && gameplay) {
        g_dt.fs = fs; g_dt.t0 = t0; g_dt.next = next; g_dt.k = k; g_dt.why = why; g_dt.interp = interp && !lost;
        DiagTickLine();
    }
    Stats(gameplay);
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkPresent(void *d, const RECT *src, const RECT *dst, HWND wnd, const void *dirty)
{
    HRESULT hr;
    if (!Mine(d)) return ((Present_t)g_o[VI_PRESENT])(d, src, dst, wnd, dirty);
    if (g_rateDirty) {
        g_rateDirty = 0;
        g_k = g_kMax;
        g_lowTicks = g_winN = g_raised = 0;
        g_retryWait = RETRY_TICKS;
        g_warm = WARM_TICKS;
    }
    if (!g_started || !g_rec) {  // shown once: menus, 30, the first frame after attaching; also a lowered rate of 30
        unsigned __int64 fs = 0, next = 0, step = 0;
        const int gameplay = !H.gameplay || H.gameplay();
        const int timing = H.tick && H.tick(&fs, &next, &step) && step > 0 && next > fs;
        g_inOwn++;  // (the wrapper's own calls inside Present are not the game's)
        hr = ((Present_t)g_o[VI_PRESENT])(d, src, dst, wnd, dirty);
        if (g_diag == 1 && gameplay) DiagCountPicture(Qpc());
        g_inOwn--;
        ReleaseRefs();
        g_prevOk = 0;
        if (g_started && gameplay) {
            S.plain++;
            S.presents++;
        }
        g_started = 1;
        Judge(gameplay, timing, fs, step, 0);  // keeps counting at a lowered rate, so the retry comes
        Stats(gameplay);
    } else {
        hr = FinishFrame(d, src, dst, wnd, dirty);
    }
    StartRecording();
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkReset(void *d, void *pp)
{
    HRESULT hr;
    const int mine = d == g_dev && GetCurrentThreadId() == g_tid;
    if (mine) DropAll();  // the device's DEFAULT-pool resources must all be gone: the references and the saved frame
    hr = ((F1)g_o[VI_RESET])(d, P(pp));
    if (mine) {
        memset(g_known, 0, sizeof g_known);  // a reset puts every state back to its default
        g_maxStream = 0;
        g_warm = WARM_TICKS;
        StartRecording();
    }
    return hr;
}

// ---------------------------------------------------------------- attaching
static void Attach(void *dev)
{
    static const struct { int i; void *fn; } kHooks[] = {
        { VI_RESET, (void *)hkReset }, { VI_PRESENT, (void *)hkPresent }, { VI_CREATETEXTURE, (void *)hkCreateTexture },
        { VI_CREATEVB, (void *)hkCreateVB }, { VI_CREATEIB, (void *)hkCreateIB }, { VI_COPYRECTS, (void *)hkCopyRects },
        { VI_UPDATETEXTURE, (void *)hkUpdateTexture }, { VI_GETFRONTBUFFER, (void *)hkGetFrontBuffer },
        { VI_SETRT, (void *)hkSetRT }, { VI_BEGINSCENE, (void *)hkBeginScene }, { VI_ENDSCENE, (void *)hkEndScene },
        { VI_CLEAR, (void *)hkClear }, { VI_SETVIEWPORT, (void *)hkSetViewport }, { VI_SETRS, (void *)hkSetRS },
        { VI_SETTEXTURE, (void *)hkSetTexture }, { VI_SETTSS, (void *)hkSetTSS }, { VI_DP, (void *)hkDP },
        { VI_DIP, (void *)hkDIP }, { VI_DPUP, (void *)hkDPUP }, { VI_DIPUP, (void *)hkDIPUP }, { VI_SETVS, (void *)hkSetVS },
        { VI_DELETEVS, (void *)hkDeleteVS }, { VI_SETVSC, (void *)hkSetVSC }, { VI_SETSTREAM, (void *)hkSetStream },
        { VI_SETINDICES, (void *)hkSetIndices }, { VI_SETPS, (void *)hkSetPS }, { VI_DELETEPS, (void *)hkDeletePS },
        { VI_SETPSC, (void *)hkSetPSC },
        { 37, (void *)hkU37 }, { 39, (void *)hkU39 }, { 42, (void *)hkU42 }, { 44, (void *)hkU44 }, { 46, (void *)hkU46 },
        { 48, (void *)hkU48 }, { 52, (void *)hkU52 }, { 53, (void *)hkU53 }, { 54, (void *)hkU54 }, { 55, (void *)hkU55 },
        { 57, (void *)hkU57 }, { 58, (void *)hkU58 }, { 66, (void *)hkU66 }, { 68, (void *)hkU68 }, { 74, (void *)hkU74 },
        { 94, (void *)hkU94 }, { 95, (void *)hkU95 }, { 96, (void *)hkU96 },
    };
    void **vt = VT(dev);
    int i;
    void *o = NULL;
    if (vt[VI_PRESENT] != (void *)hkPresent) {  // not this vtable yet (a new device of the same class keeps the hooks)
        for (i = 0; i < VI_N; ++i) g_o[i] = vt[i];
        for (i = 0; i < (int)(sizeof kHooks / sizeof kHooks[0]); ++i) SetVtbl(&vt[kHooks[i].i], kHooks[i].fn);
        if (vt[VI_PRESENT] != (void *)hkPresent) {
            Disable("the device's method table cannot be patched", 0);
            return;
        }
    }
    g_tid = GetCurrentThreadId();
    DropAll();
    g_dev = dev;
    g_started = 0;
    g_warm = WARM_TICKS;
    g_maxStream = 0;
    if (!ArenaInit(&g_arena[0], ARENA_INIT) || !ArenaInit(&g_arena[1], ARENA_INIT) || !ArenaInit(&g_pre, 1u << 20)) {
        Disable("out of memory for the frame record", 0);
        return;
    }
    // the lock methods of the game's resource classes (created before this point): one throwaway object each
    g_inOwn++;
    if (SUCCEEDED(((F7)g_o[VI_CREATETEXTURE])(dev, 1, 1, 1, 0, 21 /* A8R8G8B8 */, 1 /* MANAGED */, P(&o))) && o) {
        HookTexClass(o);
        ReleaseObj(o);
    }
    o = NULL;
    if (SUCCEEDED(((F5)g_o[VI_CREATEVB])(dev, 64, 0, 0, 1, P(&o))) && o) {
        HookBufClass(o);
        ReleaseObj(o);
    }
    o = NULL;
    if (SUCCEEDED(((F5)g_o[VI_CREATEIB])(dev, 64, 0, 101 /* INDEX16 */, 1, P(&o))) && o) {
        HookBufClass(o);
        ReleaseObj(o);
    }
    g_inOwn--;
    g_attached = 1;
    LOG("interp: device %p hooked (%d methods; buffer classes %d, texture classes %d); display %d fps, logic 30", dev,
        (int)(sizeof kHooks / sizeof kHooks[0]), g_bufc[1].vt ? 2 : g_bufc[0].vt ? 1 : 0, g_texc[0].vt ? 1 : 0, g_kMax * 30);
}

// ---------------------------------------------------------------- public
void Interp_Init(const InterpHost *host, int displayRate, int refreshHz)
{
    LARGE_INTEGER f;
    H = *host;
    QueryPerformanceFrequency(&f);
    g_qpf = (unsigned __int64)f.QuadPart;
    if (!g_qpf) g_qpf = 1000;
    g_timer = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    g_capRate = 0;
    if (refreshHz >= 30) {
        static const int kRates[] = { 30, 60, 120, 180 };
        int i;
        for (g_capRate = 30, i = 0; i < 4; ++i)
            if (kRates[i] <= refreshHz + 2) g_capRate = kRates[i];  // 59.94 Hz modes report 59
    }
    Interp_SetRate(displayRate);
    LOG("interp: frame interpolation mode: logic 30 per second, display %d%s (%s %d Hz)%s", g_kMax * 30,
        g_kMax * 30 < displayRate ? " (capped at the refresh)" : "", refreshHz ? "monitor" : "no own cap: the host caps; refresh", refreshHz,
        g_timer ? "" : "; no high-resolution timer: waits spin");
}

void Interp_SetRate(int displayRate)
{
    int r = displayRate;
    if (g_capRate && r > g_capRate) r = g_capRate;
    g_userRate = displayRate;
    g_kMax = KOfRate(r);
    InterlockedExchange(&g_rateDirty, 1);
    if (!g_started) g_k = g_kMax;
}

void Interp_FrameEnd(void *device)
{
    if (g_disabled || !device) return;
    if (g_kMax <= 1 && !g_attached) return;  // 30: nothing to do (hooks only once a higher rate is chosen)
    if (device != g_dev) Attach(device);
}

int Interp_RateInForce(void) { return (g_disabled ? 1 : g_k) * 30; }

void Interp_SetAutoLower(int on) { g_autoLower = on != 0; }

void Interp_OwnBegin(void) { g_inOwn++; }  // v3.18.0
void Interp_OwnEnd(void) { if (g_inOwn > 0) g_inOwn--; }
#ifdef INTERP_TEST
void Interp_TestSet(int noWait, int adaptive, unsigned arenaCapBytes)
{
    g_noWait = noWait;
    g_autoLower = adaptive;
    if (arenaCapBytes) g_arenaCap = arenaCapBytes;
}
#endif
