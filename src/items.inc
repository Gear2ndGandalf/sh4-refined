// ================================================================ v3.3 [Items] Sparkle: a cue on items lying in the world
//
// User (2026-11-09): "Add a visual cue and cheat option in our Refinement Mod for all item types in the world. Not
// unlike Resident Evil games ... Like a sparkle." Research: staging\item_sparkle\RE.md (static, 2026-11-09).
//
// Items lying in a room are room objects (descriptor array 0x1093AF0, 0xB0 bytes each, high-water count [0x10A0930]):
// type 4 = item pickup, type 8 = memo / document. desc+0x10 = the object (pool 0x11B6020, 300 x 0x430; +0x404 tick fn,
// 0 = free; +0x408 ev; +0x41C back to the descriptor). ev+0x10 = world position (y down), ev+0x22 = item id (0 = an
// empty spot), ev+0x40 = its model. desc+0xC bit 0 = enabled: the game recomputes it every frame (0x4D00F0 -> 0x4CF300
// -> 0x4CF120) from the story flags, the difficulty, the per-pickup taken bit and the memo flag, and draws the item's
// model only while it is set. So the sparkle follows exactly what the player can see and take: items not yet in the
// story, taken items and read memos have none.
//
// Drawn at the HUD call of the render phase (0x4EFBFD: call 0x4F4A20; neither mod patches it), after the 3D frame and
// before the HUD, message boxes and fades, only while [0xFD5C20] == 0 (gameplay) and byte [0x1079C30] != 0 (the HUD gate:
// off in pause, cutscenes, events and the item close-up). Not in Room 302 (stages 1 and 0x0A): its items are look-at
// targets, some inside cupboards. Position: the game's own world-to-screen path (0x55BF50 builds the matrix, 0x421820
// projects; 2D "GS" units, centre 2048 / 2048 on the 512 x 448 canvas), mapped to pixels with the game's native 2D
// constants (the world camera is Hor+). Walls: a segment from the camera eye ([0xFD5AC0] row 3) to the item through the
// camera's collision query 0x561CD0 (the room's BSP and dynamic meshes, as Playable Eileen's camera uses it); a wall
// plane crossed before the item hides the sparkle. Far items fade out by [Items] SparkleRange.
//
// v3.3.5 (2026-11-10): ITEM SHINE. The user-visible name is Item Shine ([Items] Shine*, the old Sparkle* keys are
// converted by ShineMigrate). What changed in the pass, and why:
// * Pickups [static; phase and gate values inferred, not yet seen live]. Taking an item runs the generic pickup event
//   (event start 0x4D06A0 -> 0x4D11F0; steps 0x618548) without the cutscene switch 0x4CCBF0, so game phase stays 0.
//   For types 4 and 7 the event start closes the HUD gate (0x4D06EF -> 0x4F5090, branch 0x4D06E1 by the type table
//   0x4D07CC) and blocks overlay requests ([0x107BF50] = 0); the record locks the player (0x530580). The "got it" text
//   is the in-game message box (0x50ACD0), still phase 0. The per-pickup taken bit is set only when the text is gone
//   (0x4D1180: 0x50ABC0 test, then 0x4F3A90 at 0x4D11B9), and the next enabled-bit pass hides the item (a dropped item
//   is destroyed, 0x52B020). v3.3.4 returned as soon as the HUD gate closed, so every shine vanished mid-flash at the
//   press, and the taken item's never came back. Now phase 0 draws whatever the HUD gate says, and an item that
//   disappears while it flashes finishes that flash where it lay (g_spTr), so nothing is cut off.
// * Pause. Opening an overlay (pause 0x4F8BA0, map, items, memos...) closes the HUD gate and asks for a frozen copy of
//   the frame in the same frame (0x4F9151 -> 0x55A160(1)); the frame end copies it (0x55ABF0 -> 0x55A5C0: 0x4036E0
//   CopyRects the back buffer [0x6E7DA0], so everything drawn this frame, the shine included, is in it) and every
//   overlay frame starts by drawing that copy (0x55AE20 -> 0x55A8C0), phase 2, no 3D pass. v3.3.4 drew nothing in that
//   frame (HUD gate) nor in phase 2, so the paused picture had no shine. Now the request frame (phase 0) is drawn as
//   usual, so the frozen copy holds the shine; phase 2 draws nothing on top (it would count twice), and the shine clock
//   g_spClock stops in phase 2, so play goes on from the same moment of each flash.
// * Characters. ShineDepthTest stays off (v3.3.2's inferred depth hid everything), so characters hide the shine only
//   through the screen capsules of SpGatherOccluders, which had the player and the 0x45E760 list (monsters) but not the
//   companions: Eileen is the task [0xFCE6B4] (0x44ADE0 / 0x44B110 / 0x44B3D0), Cynthia [0xFD5A50] (0x4ED000 /
//   0x4ED4F0), neither in that list. They are added from their model ([work+0x18]: +0x10 position, +0xE4 bit 2 = not
//   drawn, which 0x55C650 tests), as Refined's EntitySmooth reads them.
// * Real-time cutscenes (phase 1) still show no shine; the front end ([0x1236D4C] != 6) neither.
// * v3.32 DHS332 (the user: "allow Item Shine to shine through downed ghosts"): a ghost lying down (types 0x0B..0x0F /
//   0x11 in class 9 / 0x0A / 0x0B: HbGhostLying, RE_GHOST_HEALTH.md) is no occluder, so an item under it shines; standing
//   ghosts, monsters, the player and the companions still hide it. [Items] ShineThroughDowned=0 (hidden) = v3.31.
// * v3.35 GH335 (the user, 2026-11-20): every enemy lying down or dead (classes 9 / 0x0A / 0x0B, any type) lets the shine
//   through, not only the ghosts (Supply Drops lie where monsters die); standing enemies still hide it.
#include <float.h>  // _finite
#include "items_draw.inc"
#include "hudbars_sim.h"  // v3.32 DHS332: HbGhostLying (include-guarded; hudbars.inc includes it again later)

static DWORD g_spHudFn = 0x4F4A20;
static int g_spInstalled;
static int g_spDowned = 1;  // v3.32 DHS332: [Items] ShineThroughDowned (hidden, default 1; 0 = v3.31: lying ghosts hide the shine)
static int g_spDownedN;     // v3.32 DHS332: lying ghosts skipped as occluders (the log)
static DWORD g_spLastLog, g_spLastCount = 0xFFFFFFFF;
static __declspec(align(16)) float g_spM[16], g_spO[4], g_spSeg[8], g_spPlane[4], g_spPos[4];

static void SparkleUserChanged(int on)
{
    InterlockedExchange(&g_sparkleOn, on ? 1 : 0);
    if (!g_iniFallback) WritePrivateProfileStringA("Items", "Shine", on ? "1" : "0", g_iniPath);  // v3.3.5: was Sparkle
    Log("items: item shine %s", on ? "on" : "off");
}

// Project a world point: 1 and pixels (render target) when it is in front of the camera; *pz (if given) = the depth
// the game's own world sprites get: the projected GS z through the native 2D constants (c2.z = 1 / 2^24, c3.z = 0 by
// the game's formulas). v3.3.2: the 3D pass builds its projection with 0x41D510, the same screen matrix as 0x41D4C0
// (both through 0x41D3A0), so this is the depth the world was drawn with.
static int SpProject(const float *p, const Viewport8 *vp, float *px, float *py, float *pz)
{
    float cx, cy;
    g_spPos[0] = p[0];
    g_spPos[1] = p[1];
    g_spPos[2] = p[2];
    g_spPos[3] = 1.0f;
    ((void(__cdecl *)(float *, const float *, const float *))0x421820)(g_spO, g_spM, g_spPos);
    if (!(g_spO[3] > 1.0f) || !_finite(g_spO[0]) || !_finite(g_spO[1])) return 0;
    if (g_haveConsts && g_native.c2[0] != 0.0f && g_native.c2[1] != 0.0f) {  // the canvas -> clip constants
        cx = g_spO[0] * g_native.c2[0] + g_native.c3[0];
        cy = g_spO[1] * g_native.c2[1] + g_native.c3[1];
    } else {  // the stock formulas (c2 = 1/256, -1/224; c3 = -8, 64/7)
        cx = (g_spO[0] - 2048.0f) / 256.0f;
        cy = -(g_spO[1] - 2048.0f) / 224.0f;
    }
    if (pz) {
        const float c2z = g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c2[2] : 1.0f / 16777216.0f;
        const float c3z = g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c3[2] : 0.0f;
        float z = g_spO[2] * c2z + c3z;
        if (!(z > 0.0f)) z = 0.0f;
        if (!(z < 1.0f)) z = 0.99999f;
        *pz = z;
    }
    *px = (float)vp->x + (cx + 1.0f) * 0.5f * (float)vp->w;
    *py = (float)vp->y + (1.0f - cy) * 0.5f * (float)vp->h;
    return 1;
}

// 1 = a wall plane lies between the camera eye and the point
static int SpBlocked(const float *eye, const float *p)
{
    float a, b;
    const float *n = g_spPlane;
    if (g_spWalls || !*(volatile DWORD *)0x12F6D74) return 0;  // through walls asked for, or no collision loaded
    g_spSeg[0] = eye[0];
    g_spSeg[1] = eye[1];
    g_spSeg[2] = eye[2];
    g_spSeg[3] = 1.0f;
    g_spSeg[4] = p[0];
    g_spSeg[5] = p[1];
    g_spSeg[6] = p[2];
    g_spSeg[7] = 1.0f;
    if (!((int(__cdecl *)(const float *, float *))0x561CD0)(g_spSeg, g_spPlane)) return 0;
    a = n[0] * g_spSeg[0] + n[1] * g_spSeg[1] + n[2] * g_spSeg[2] + n[3];
    b = n[0] * g_spSeg[4] + n[1] * g_spSeg[5] + n[2] * g_spSeg[6] + n[3];
    if (!((a < 0.0f) != (b < 0.0f)) || a == b) return 0;  // the segment does not cross that plane
    return a / (a - b) < 0.93f;  // crossed well before the item (the item's own shelf or floor is at the far end)
}

// v3.3.3: bodies in front. v3.3.2's depth-buffer test hid every sparkle on the main rig (the user's session ran with
// SparkleDepthTest=1 throughout), so it is now off by default and kept only as an experiment. Instead each sparkle is
// tested against the characters between it and the camera: the player (0x10A0F60) and every actor of the room's list
// (0x45E760(prev), position [[actor+0x408]+0x10], as the game's own fatigue scan 0x541E60 reads it), each an upright
// capsule from the feet to 330 above them, about 45 units in radius, projected to the screen.
#define SP_OCC 40
typedef struct { float fx, fy, hx, hy, r, dist; } SpOcc;
static int SpAddOcc(const float *f0, const float *eye, const Viewport8 *vp, SpOcc *o)
{
    const float f[3] = { f0[0], f0[1], f0[2] }, h[3] = { f0[0], f0[1] - 330.0f, f0[2] }, s[3] = { f0[0], f0[1] - 165.0f, f0[2] };
    float fx, fy, hx, hy, sx, sy, dx, dy, dz;  // y points down: the head is above the feet at y - 330
    if (!_finite(f[0] + f[1] + f[2])) return 0;
    if (!SpProject(f, vp, &fx, &fy, NULL) || !SpProject(h, vp, &hx, &hy, NULL) || !SpProject(s, vp, &sx, &sy, NULL))
        return 0;
    dx = s[0] - eye[0];
    dy = s[1] - eye[1];
    dz = s[2] - eye[2];
    o->fx = fx;
    o->fy = fy;
    o->hx = hx;
    o->hy = hy;
    o->r = sqrtf((hx - fx) * (hx - fx) + (hy - fy) * (hy - fy)) * (45.0f / 330.0f);
    o->dist = sqrtf(dx * dx + dy * dy + dz * dz);
    return 1;
}
// v3.3.5 (user: "The item shine renders through the escort Eileen"): a companion's feet, or NULL when there is none or
// she is not drawn. taskAt holds the task (Eileen [0xFCE6B4], Cynthia [0xFD5A50]); it must be a live pool slot running
// one of her tick functions; her model is [[task+0x408]+0x18] (the companion code's own read, 0x43E450), position +0x10,
// +0xE4 bit 2 = skipped by the character draw 0x55C650 (she is hidden).
static const float *SpCompanionFeet(DWORD taskAt, const DWORD *fns, int nfn)
{
    const BYTE *task = *(BYTE *const *)taskAt, *work, *model = NULL;
    DWORD fn;
    int k;
    if (!task) return NULL;
    if (!TaskSlotOk(task)) return NULL;  // v3.36 TP336: a slot of the task pool (pools_sim.h)
    fn = *(const DWORD *)(task + 0x404);
    for (k = 0; k < nfn && fns[k] != fn; ++k) {}
    if (k == nfn) return NULL;
    __try {  // her work block and model live outside the pool: a bad read skips her, not the whole pass
        work = *(BYTE *const *)(task + 0x408);
        if ((ULONG_PTR)work >= 0x10000) model = *(BYTE *const *)(work + 0x18);
        if ((ULONG_PTR)model < 0x10000 || (model[0xE4] & 4) || !_finite(((const float *)(model + 0x10))[0])) model = NULL;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        model = NULL;
    }
    return model ? (const float *)(model + 0x10) : NULL;
}

static int SpGatherOccluders(const float *eye, const Viewport8 *vp, SpOcc *occ)
{
    static const DWORD kEileenFn[] = { 0x44ADE0, 0x44B110, 0x44B3D0 }, kCynthiaFn[] = { 0x4ED000, 0x4ED4F0 };
    int n = 0, guard = 0;
    const BYTE *a = NULL;
    const float *cf;
    n += SpAddOcc((const float *)0x10A0F60, eye, vp, &occ[n]);  // the player
    if ((cf = SpCompanionFeet(0xFCE6B4, kEileenFn, 3)) != NULL) n += SpAddOcc(cf, eye, vp, &occ[n]);  // v3.3.5
    if ((cf = SpCompanionFeet(0xFD5A50, kCynthiaFn, 2)) != NULL) n += SpAddOcc(cf, eye, vp, &occ[n]);
    while (n < SP_OCC && guard++ < 64) {  // the room's actors
        const BYTE *w;
        float pos[3];
        a = ((const BYTE *(__cdecl *)(const BYTE *))0x45E760)(a);
        if (!a) break;
        w = *(const BYTE *const *)(a + 0x408);
        if (!w) continue;
        // v3.32 DHS332: a lying ghost; v3.35 GH335: any enemy lying down or dead (classes 9 / 0x0A / 0x0B, any type)
        if (g_spDowned && (HbGhostLying(*(const int *)(w + 0x20), *(const int *)(w + 0x3C)) || *(const int *)(w + 0x3C) == 9 ||
                           *(const int *)(w + 0x3C) == 0x0A || *(const int *)(w + 0x3C) == 0x0B)) {
            ++g_spDownedN;
            continue;
        }
        memcpy(pos, w + 0x10, sizeof pos);
        n += SpAddOcc(pos, eye, vp, &occ[n]);
    }
    return n;
}

// 1 = a character nearer to the camera than the item covers its point on the screen
static int SpBehindBody(const SpOcc *occ, int nocc, float x, float y, float dist)
{
    int k;
    for (k = 0; k < nocc; ++k) {
        const SpOcc *o = &occ[k];
        float vx, vy, l2, t, cx, cy;
        if (!(o->dist < dist - 50.0f)) continue;  // not in front (the item's own carrier, or behind it)
        vx = o->hx - o->fx;
        vy = o->hy - o->fy;
        l2 = vx * vx + vy * vy;
        t = l2 > 1e-6f ? ((x - o->fx) * vx + (y - o->fy) * vy) / l2 : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        cx = o->fx + vx * t - x;
        cy = o->fy + vy * t - y;
        if (cx * cx + cy * cy < o->r * o->r) return 1;
    }
    return 0;
}

// v3.3.3: [Items] read again when SH4Refined.ini changes (checked once a second), so edits apply without a restart
static void GpIniPoll(void);  // v3.25.0 (gameplay.inc): [Gameplay]'s own poll, also run from here
static FILETIME g_spIniTime;
static DWORD g_spIniCheck;
static void SpReadIni(void)
{
    char b[32];
    ShineMigrate();  // v3.3.5: an old Sparkle* key typed in while the game runs is taken over too
    g_sparkleOn = GetPrivateProfileIntA("Items", "Shine", 1, g_iniPath) ? 1 : 0;
    g_spMemos = GetPrivateProfileIntA("Items", "ShineMemos", 1, g_iniPath) ? 1 : 0;
    g_spRange = GetPrivateProfileIntA("Items", "ShineRange", 3000, g_iniPath);
    if (g_spRange < 500) g_spRange = 500;
    if (g_spRange > 20000) g_spRange = 20000;
    g_spWalls = GetPrivateProfileIntA("Items", "ShineThroughWalls", 0, g_iniPath) ? 1 : 0;
    g_spDepth = GetPrivateProfileIntA("Items", "ShineDepthTest", 0, g_iniPath) ? 1 : 0;
    g_spBodies = GetPrivateProfileIntA("Items", "ShineBodies", 1, g_iniPath) ? 1 : 0;
    g_spDowned = GetPrivateProfileIntA("Items", "ShineThroughDowned", 1, g_iniPath) ? 1 : 0;  // v3.32 DHS332
    GetPrivateProfileStringA("Items", "ShineEvery", "7", b, sizeof b, g_iniPath);
    g_spEvery = (float)atof(b);
    GetPrivateProfileStringA("Items", "ShineFor", "2", b, sizeof b, g_iniPath);
    g_spFor = (float)atof(b);
    if (!(g_spEvery >= 2.0f)) g_spEvery = 2.0f;
    if (g_spEvery > 60.0f) g_spEvery = 60.0f;
    if (!(g_spFor >= 0.5f)) g_spFor = 0.5f;
    if (g_spFor > g_spEvery) g_spFor = g_spEvery;
}
static void SpIniPoll(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    const DWORD now = GetTickCount();
    GpIniPoll();  // v3.25.0 (review REV-325-007)
    if (g_iniFallback || now - g_spIniCheck < 1000) return;
    g_spIniCheck = now;
    if (!GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &a)) return;
    if (!g_spIniTime.dwLowDateTime && !g_spIniTime.dwHighDateTime) {
        g_spIniTime = a.ftLastWriteTime;
        return;
    }
    if (CompareFileTime(&a.ftLastWriteTime, &g_spIniTime) == 0) return;
    g_spIniTime = a.ftLastWriteTime;
    {   // v3.5.1 (review OPT-SHINE-INI-001): our own writes read back the value already set; a different value is an
        // edit made outside the game, which becomes the Options snapshot too, so "No" at the exit never undoes it
        const LONG before = g_sparkleOn;
        SpReadIni();
        if ((g_sparkleOn != 0) != (before != 0)) g_snapShine = g_sparkleOn != 0;
    }
    {   // v3.45 QS345: logged only when a value differs from the last logged set (v3.44: every re-read, 324 x in 28 min)
        static int s_logged;
        static char s_last[160];
        char now[160];
        // v3.45 QS345-01: %g, so a fractional change (2.01 -> 2.04) is a new line and shows in it
        sprintf_s(now, sizeof now, "shine %d, memos %d, range %d, through walls %d, bodies %d, depth test %d, a %g s flash every %g s",
                  (int)g_sparkleOn, g_spMemos, g_spRange, g_spWalls, g_spBodies, g_spDepth, g_spFor, g_spEvery);
        if (!s_logged || strcmp(now, s_last)) {
            Log("items: settings read again (%s)", now);
            strcpy_s(s_last, sizeof s_last, now);
            s_logged = 1;
        }
    }
}

// v3.3.5: the shine clock. It runs in gameplay and events and stops in game phase 2 (pause, map, items, memos, save
// screens: the frozen picture), so a flash goes on from where it was when play resumes. Real time between calls,
// at most 0.1 s per frame (a hitch or a long load does not jump the cycle).
static double g_spClock;
static LARGE_INTEGER g_spClockT;
static void SpClockTick(int run)
{
    LARGE_INTEGER now, f;
    double dt = 0.0;
    QueryPerformanceCounter(&now);
    if (g_spClockT.QuadPart && QueryPerformanceFrequency(&f) && f.QuadPart)
        dt = (double)(now.QuadPart - g_spClockT.QuadPart) / (double)f.QuadPart;
    g_spClockT = now;
    if (!(dt >= 0.0) || dt > 0.1) dt = 0.1;
    if (run) g_spClock += dt;
}

// An item's place in its flash cycle (v3.3.2: SparkleEvery seconds, each item at its own moment): *k = which flash,
// return u = 0..1 while it flashes (SparkleFor seconds), >= 1 between flashes.
static double SpCycle(float ph, double *k)
{
    const double every = (double)g_spEvery, x = g_spClock + fmod((double)ph * 2.39, every);
    const double kk = floor(x / every);
    *k = kk;
    return (x - kk * every) / (double)g_spFor;
}

// v3.3.5: items seen this room, so one that is taken (or otherwise gone) while it flashes finishes that flash where it
// lay. Keyed by descriptor, object and item id; cleared on a room change.
#define SP_TRACK 64
typedef struct { const BYTE *d, *o; short id; char seen, flashing; float p[3], ph; double k; } SpTrack;
static SpTrack g_spTr[SP_TRACK];
static int g_spTrN;
static DWORD g_spTrRoom = 0xFFFFFFFF;
static int g_spRoomFrames;  // gameplay frames since the room changed (the first ones project garbage: v3.3.4's log)
static DWORD g_spLinger;    // flashes finished after their item went (for the log)

static SpTrack *SpTrackGet(const BYTE *d, const BYTE *o, short id)
{
    int k;
    for (k = 0; k < g_spTrN; ++k)
        if (g_spTr[k].d == d && g_spTr[k].o == o && g_spTr[k].id == id) return &g_spTr[k];
    if (g_spTrN >= SP_TRACK) return NULL;
    memset(&g_spTr[g_spTrN], 0, sizeof g_spTr[0]);
    g_spTr[g_spTrN].d = d;
    g_spTr[g_spTrN].o = o;
    g_spTr[g_spTrN].id = id;
    return &g_spTr[g_spTrN++];
}

#define SP_MAX 48
// One frame's drawing context, shared by live items and the ones finishing their last flash
typedef struct {
    Viewport8 vp;
    const float *eye;
    float range;
    SpOcc occ[SP_OCC];
    int nocc;
    SpPt pts[SP_MAX * 2];
    int np, nWall, nBody, nOff, nDrawn, nBack;
} SpFrame;

// Projects, tests and adds one flashing item (u in 0..1, its envelope sin(pi u)); returns 'O' off screen, 'B' behind
// the camera, 'W' behind a wall, 'C' behind a character, 0 drawn
static char SpEmit(SpFrame *f, const float *p, float dist, float ph, float u)
{
    const float dx = p[0] - f->eye[0], dy = p[1] - f->eye[1], dz = p[2] - f->eye[2];
    float x, y, z, x2, y2, size, fade;
    const float env = sinf(u * 3.14159265f);
    if (f->np + 2 > SP_MAX * 2) return 'O';
    fade = (f->range - dist) / (f->range * 0.25f);
    if (fade > 1.0f) fade = 1.0f;
    if (!(fade > 0.0f)) return 'O';
    if (!SpProject(p, &f->vp, &x, &y, NULL)) {
        ++f->nBack;
        return 'B';
    }
    {   // depth: the point moved 40 units toward the camera, so the floor or shelf it lies on does not cut it
        const float k = dist > 41.0f ? 40.0f / dist : 0.0f;
        const float pb[3] = { p[0] - dx * k, p[1] - dy * k, p[2] - dz * k };
        float bx, by;
        if (!SpProject(pb, &f->vp, &bx, &by, &z)) z = 0.0f;
    }
    if (x < f->vp.x - 64.0f || y < f->vp.y - 64.0f || x > f->vp.x + f->vp.w + 64.0f || y > f->vp.y + f->vp.h + 64.0f) {
        ++f->nOff;
        return 'O';
    }
    if (SpBlocked(f->eye, p)) {
        ++f->nWall;
        return 'W';
    }
    if (g_spBodies) {
        if (f->nocc < 0) f->nocc = SpGatherOccluders(f->eye, &f->vp, f->occ);
        if (SpBehindBody(f->occ, f->nocc, x, y, dist)) {
            ++f->nBody;
            return 'C';
        }
    }
    ++f->nDrawn;
    {   // size: about 30 world units across, held between 14 and 70 pixels at 720 lines
        const float up[3] = { p[0], p[1] - 30.0f, p[2] }, k = (float)f->vp.h / 720.0f;
        size = SpProject(up, &f->vp, &x2, &y2, NULL) ? sqrtf((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y)) * 2.0f : 30.0f * k;
        if (size < 14.0f * k) size = 14.0f * k;
        if (size > 70.0f * k) size = 70.0f * k;
    }
    f->pts[f->np].x = x;  // soft glow
    f->pts[f->np].y = y;
    f->pts[f->np].z = z;
    f->pts[f->np].size = size * (1.1f + 0.5f * env);
    f->pts[f->np].alpha = 0.4f * env * fade;
    f->pts[f->np].spin = 0.785398f;
    ++f->np;
    f->pts[f->np].x = x;  // the star: grows, brightens and turns a quarter while it flashes
    f->pts[f->np].y = y;
    f->pts[f->np].z = z;
    f->pts[f->np].size = size * (0.55f + 0.6f * env);
    f->pts[f->np].alpha = (0.35f + 0.9f * env) * env * fade;
    f->pts[f->np].spin = ph + u * 1.5707963f;
    ++f->np;
    return 0;
}

static void SparkleFrameC(void)
{
    static SpFrame fr;  // game thread only; too big for the hook's stack
    int nFar = 0, nWait = 0, nLinger = 0;
    // v3.3.4 (user: "Items are currently failing to sparkle at all"; v3.3.3's log showed every flashing item off screen):
    // what happened to each item this frame, for the log line
    struct { short id; char why; float dist, x, y, w; } dg[6];
    int ndg = 0;
    const BYTE *base = (const BYTE *)0x1093AF0;
    int i, n, found = 0;
    DWORD phase, stage, room;
    void *dev = DEV;
    SpIniPoll();
    phase = *(volatile DWORD *)0xFD5C20;
    SpClockTick(phase != 2);  // v3.3.5: frozen while paused
    if (!g_sparkleOn || !dev) return;
    // v3.3.5: gameplay, whatever the HUD gate says (pickup events and the frame the pause picture is taken from close it);
    // not the front end, not real-time cutscenes (1), not overlays (2: their frozen picture already holds the shine)
    if (*(volatile int *)0x1236D4C != 6 || phase != 0) return;
    stage = *(volatile DWORD *)0xFD5A90;
    room = *(volatile DWORD *)0xFD5A94;
    if (stage == 1 || stage == 0x0A) return;  // Room 302
    if ((stage << 16 | (room & 0xFFFF)) != g_spTrRoom) {  // a new room: forget its items, let the camera settle
        g_spTrRoom = stage << 16 | (room & 0xFFFF);
        g_spTrN = 0;
        g_spRoomFrames = 0;
    }
    if (g_spRoomFrames < 1000) ++g_spRoomFrames;
    n = *(volatile int *)0x10A0930;
    if (n > 300) n = 300;
    if (n <= 0 && !g_spTrN) return;
    if (FAILED(((Viewport_t)VT(dev, VT_GETVP))(dev, &fr.vp)) || fr.vp.w < 16 || fr.vp.h < 16) return;
    ((void(__cdecl *)(float *))0x55BF50)(g_spM);
    fr.eye = (const float *)(0xFD5AC0 + 48);
    fr.range = (float)g_spRange;
    fr.nocc = -1;
    g_spDownedN = 0;  // v3.32 DHS332: counted by this frame's occluder gather
    fr.np = fr.nWall = fr.nBody = fr.nOff = fr.nDrawn = fr.nBack = 0;
    for (i = 0; i < g_spTrN; ++i) g_spTr[i].seen = 0;
    for (i = 0; i < n; ++i) {
        const BYTE *d = base + i * 0xB0, *o = *(BYTE *const *)(d + 0x10), *ev;
        const WORD type = *(const WORD *)(d + 6);
        float p[3], dx, dy, dz, dist, ph;
        double u, k;
        SpTrack *tr;
        if (!o || (type != 4 && !(type == 8 && g_spMemos))) continue;
        if (!TaskSlotOk(o)) continue;  // not a pool slot (v3.36 TP336: pools_sim.h)
        if (*(BYTE *const *)(o + 0x41C) != d || !*(const DWORD *)(o + 0x404)) continue;  // free or stale
        if (!(*(const DWORD *)(d + 0xC) & 1)) continue;  // not there for the player (taken, read, not yet in the story)
        ev = *(BYTE *const *)(o + 0x408);
        if (!ev || !*(void *const *)(ev + 0x40) || !*(const short *)(ev + 0x22)) continue;  // empty spot / no model
        ++found;
        p[0] = ((const float *)(ev + 0x10))[0];
        p[1] = ((const float *)(ev + 0x10))[1] - 25.0f;  // a little above the model's origin (y points down)
        p[2] = ((const float *)(ev + 0x10))[2];
        if (!_finite(p[0] + p[1] + p[2])) continue;
        dx = p[0] - fr.eye[0];
        dy = p[1] - fr.eye[1];
        dz = p[2] - fr.eye[2];
        dist = sqrtf(dx * dx + dy * dy + dz * dz);
        ph = (float)i * 1.7f + (float)*(const short *)(ev + 0x22) * 0.61f;
        u = SpCycle(ph, &k);
        if ((tr = SpTrackGet(d, o, *(const short *)(ev + 0x22))) != NULL) {
            memcpy(tr->p, p, sizeof p);
            tr->ph = ph;
            tr->k = k;
            tr->flashing = u < 1.0;
            tr->seen = 1;
        }
        {   // v3.3.4: where it lands, for the log (also for items that wait for their flash)
            float qx = 0.0f, qy = 0.0f;
            const int qok = SpProject(p, &fr.vp, &qx, &qy, NULL);
            if (ndg < 6) {
                dg[ndg].id = *(const short *)(ev + 0x22);
                dg[ndg].dist = dist;
                dg[ndg].x = qx;
                dg[ndg].y = qy;
                dg[ndg].w = g_spO[3];
                dg[ndg].why = !(dist < fr.range) ? 'F' : !qok ? 'B' : 0;
                ++ndg;
            }
        }
        if (!(dist < fr.range)) {
            ++nFar;
            continue;
        }
        if (!(u < 1.0)) {
            ++nWait;
            if (ndg && dg[ndg - 1].why == 0) dg[ndg - 1].why = 'W';
            continue;
        }
        if (g_spRoomFrames <= 3) continue;  // the camera of a new room is not settled yet
        {
            const char why = SpEmit(&fr, p, dist, ph, (float)u);
            if (why == 'O' && ndg && dg[ndg - 1].why == 0) dg[ndg - 1].why = 'O';
        }
    }
    // v3.3.5 (user: "When the player picks up an item, the shine stops abruptly"): an item gone since the last frame
    // (taken, or its object destroyed) finishes the flash it was in, at the place it lay; then it is forgotten
    for (i = 0; i < g_spTrN;) {
        SpTrack *tr = &g_spTr[i];
        if (!tr->seen) {
            double k;
            const double u = SpCycle(tr->ph, &k);
            float dist;
            if (!tr->flashing || k != tr->k || !(u < 1.0)) {  // its flash is over (or it was waiting): drop it
                g_spTr[i] = g_spTr[--g_spTrN];
                continue;
            }
            dist = sqrtf((tr->p[0] - fr.eye[0]) * (tr->p[0] - fr.eye[0]) + (tr->p[1] - fr.eye[1]) * (tr->p[1] - fr.eye[1]) +
                         (tr->p[2] - fr.eye[2]) * (tr->p[2] - fr.eye[2]));
            if (dist < fr.range && g_spRoomFrames > 3) {
                SpEmit(&fr, tr->p, dist, tr->ph, (float)u);
                ++nLinger;
            }
            if (tr->flashing == 1) {
                tr->flashing = 2;  // counted once
                ++g_spLinger;
            }
        }
        ++i;
    }
    if (((DWORD)found != g_spLastCount && GetTickCount() - g_spLastLog > 2000) ||
        (found && GetTickCount() - g_spLastLog > 30000)) {  // v3.3.3: and every 30 s, with this frame's outcome
        g_spLastCount = (DWORD)found;
        g_spLastLog = GetTickCount();
        Log("items: %d item(s) or memo(s) here for the player (stage %lu room %lu); this frame: %d flashing and drawn, "
            "%d behind walls, %d behind characters (%d characters, %d lying enemies let it through), %d off screen, %d "
            "finishing after pickup (%lu so far); HUD gate %d", found, stage, room, fr.nDrawn, fr.nWall, fr.nBody,  // v3.32 DHS332
            fr.nocc < 0 ? 0 : fr.nocc, fr.nocc < 0 ? 0 : g_spDownedN, fr.nOff, nLinger, g_spLinger, (int)*(volatile BYTE *)0x1079C30);
        {   // v3.3.4: the self-check. Eileen's chest should land on the screen in third person; if it does not, the
            // projection (0x55BF50 at the HUD call) is not the camera the world was drawn with.
            const float *pp = (const float *)0x10A0F60;
            const float chest[3] = { pp[0], pp[1] - 120.0f, pp[2] };
            float cx = 0.0f, cy = 0.0f;
            const int cok = _finite(chest[0] + chest[1] + chest[2]) && SpProject(chest, &fr.vp, &cx, &cy, NULL);
            char b[512];
            int k, o;
            o = sprintf_s(b, sizeof b, "items: check: viewport %lu,%lu %lux%lu; player chest %s at %.0f,%.0f (w %.1f); "
                          "%d too far, %d between flashes, %d behind the camera", fr.vp.x, fr.vp.y, fr.vp.w, fr.vp.h,
                          cok ? "projects" : "does not project", cx, cy, g_spO[3], nFar, nWait, fr.nBack);
            for (k = 0; k < ndg && o > 0 && o < (int)sizeof b - 64; ++k)
                o += sprintf_s(b + o, sizeof b - o, "; item %d at %.0f,%.0f dist %.0f w %.1f%s", dg[k].id, dg[k].x,
                               dg[k].y, dg[k].dist, dg[k].w, dg[k].why == 'F' ? " far" : dg[k].why == 'B' ? " behind" :
                               dg[k].why == 'W' ? " waits" : dg[k].why == 'O' ? " off screen" : "");
            Log("%s", b);
        }
    }
    if (fr.np) SparkleDrawList(fr.pts, fr.np, 0xFFF0C8, g_spDepth);
}

static DWORD g_spFaultCode;
static void *g_spFaultAt;
static void SpFaultNote(EXCEPTION_POINTERS *e)
{
    g_spFaultCode = e->ExceptionRecord->ExceptionCode;
    g_spFaultAt = e->ExceptionRecord->ExceptionAddress;
}

static void SubExitFrame(void);  // subtitles.inc (included later)
static void HbFrame(void);       // HUD BARS: hudbars.inc (included later)
static void FiDrawFrame(void);   // v3.50 FI350: fadein.inc (included later): Fade In's clock and blood portals
static void __cdecl SparkleFrame(void)
{
    static int faults;
    LhFrameBegin();  // v3.33 LOG333 (W7): the health log times Refined's per-frame work (loghealth.inc)
    // v3.35 FX335: every part below is timed in the frame ledger (budget.inc BG_RUN: exclusive times, named in the log)
    BG_RUN(BGC_MENU, FovViewPoll());  // v3.3.5: Room 302's window and peephole views keep the game's own angle (sh4windowmod.c)
    BG_RUN(BGC_MENU, QsFrame());      // v3.6.0: quick save / load keys and feedback (quicksave.inc)
    BG_RUN(BGC_WARM, RfWarmFrame());  // v3.35 FX335: the effects made ready at a room's first frame (echo.inc)
    BG_RUN(BGC_ECHO, EchoDrawFrame());  // v3.30 ECHO: the Echo double, before the fog (its smoke blends over it) and the filter (echo.inc)
    BG_RUN(BGC_DP, DpDrawFrame());  // v3.27.0: the Death Presence fog, before Item Shine and the filter (deathpresence_draw.inc)
    BG_RUN(BGC_AI, AiDrawFrame());  // v3.29 AFTERIMAGE: the after-image trail, after the fog (afterimage.inc)
    BG_RUN(BGC_FX, FiDrawFrame());  // v3.50 FI350: Fade In's blood portals (fadein.inc), before Item Shine and the filter
    // v3.22.0 (user item 2: the shine was drawn after the filter, unfiltered): Item Shine first, then the filter over it
    if (faults <= 3) {
        BgPush(BGC_ITEMS);  // v3.35 FX335
        __try {
            SparkleFrameC();
        } __except (SpFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
            if (++faults == 1) Log("items: the shine pass faulted (code %08lX at %p); it stops after 3 more", g_spFaultCode, g_spFaultAt);
            if (faults > 3) Log("items: the shine pass is off for this session");
        }
        BgPop();  // v3.35 FX335
    }
    BG_RUN(BGC_FX, FxSceneFrame()); // v3.16.0: the Screen Effects scene filter, before the HUD (screenfx.inc, RE_order 2); v3.22.0: after Item Shine
    BG_RUN(BGC_HUD, HbFrame());      // HUD BARS: enemy health bars and the curse meter, after the filter (unfiltered), before the HUD
    BG_RUN(BGC_SUBS, SubExitFrame()); // v3.20.0: the subtitle clock and exits (subtitles.inc), before the message window and text flush
    LhFrameEnd();   // v3.33 LOG333 (W7): frame times, room lines, framework checks (loghealth.inc)
}

static __declspec(naked) void hkHudSparkle(void)  // 4EFBFD: was `call 4F4A20` (the HUD)
{
    __asm {
        pushad
        pushfd
        call SparkleFrame
        popfd
        popad
        jmp dword ptr [g_spHudFn]
    }
}

static void SparkleInstall(void)
{
    static const Site kSite = { 0x4EFBFD, { 0xE8, 0x1E, 0x4E, 0x00, 0x00 }, 5 };  // call 4F4A20
    if (!SiteOk(&kSite)) {
        const BYTE *p = (const BYTE *)0x4EFBFD;
        Log("items: 004EFBFD is not the stock HUD call (%02X %02X %02X %02X %02X; another mod?): no item shine, and no "
            "Room 302 view guard for FOV First Person", p[0], p[1], p[2], p[3], p[4]);
        return;
    }
    PokeBranch(0x4EFBFD, 0xE8, hkHudSparkle, 5);
    g_spDowned = GetPrivateProfileIntA("Items", "ShineThroughDowned", 1, g_iniPath) ? 1 : 0;  // v3.32 DHS332
    g_spInstalled = 1;
    Log("items: item shine %s (memos %d, range %d, through walls %d, bodies %d incl. companions, depth test %d, a %.1f s "
        "flash every %.1f s; kept through pickups, frozen while paused); HUD call 004EFBFD hooked (also the Room 302 "
        "view guard)", g_sparkleOn ? "on" : "off", g_spMemos, g_spRange, g_spWalls, g_spBodies, g_spDepth, g_spFor,
        g_spEvery);
}
