// ================================================================ v3.50 FI350: Fade In (victims and monsters fade in)
// Plan: staging/v350_feedback/w4_fadein/PLAN.md (USER_FEEDBACK_v349 C2: "Victims just pop-in out of thin air ... add a
// bloody portal effect where they fade in"; C3: "Monsters pop in rooms too. Make them fade in."). Included by
// sh4windowmod.c after rzmon.inc (it reads enemies.inc's watch list, ghost tags and helpers, and draws with screenfx.inc's
// helpers). The pure rules are fadein_sim.h (native test: test/fadein_sim_test.c). GOG v7 VAs. No exe byte is patched.
//
// The row: Option 5 > Enemies > Fade In, Off / Fade / Fade + Portal ([Gameplay] FadeIn, default Fade + Portal; the hidden
// key FadeInSeconds, 0.2 .. 3.0, default 0.8). Texts MSG_V50 + 8 .. 15 (BRIEF_COMMON).
//
// How: every enemy actor drawn by the registry model loop (0x52AE50, the hook 0x52AEB5 of screenfx.inc: ThermDrawFi) is
// remembered at its first sight in a room visit (task + work + spawn id). One that may fade is then drawn by the game's
// own see-through enemy draw 0x45C880(model, transparency) (matched in SH4Decomp: the draw its ghosts use when they go
// into or come out of a wall, and its enemy echoes: pixel shader 4 with c0 = the level, the GS ALPHA (Cs - Cd) x FIX + Cd,
// TEST 0x53001 = no depth write by its bits [inferred], then the stock states back) instead of the plain ModelDraw, from transparency 1 (not
// drawn) to 0 over FadeInSeconds (smoothstep); its shadow (0x52AF95) joins when the body is half in. A ghost that is
// already see-through by its own draw (pre-draw 0x45C710 / 0x45C7A0, work +0x80) gets both: 1 - (1 - own) x frac.
// Never faded: outside the monster worlds (stage 2 .. 8), in a cut-scene (phase 1: and a fade running when one starts
// ends), bosses (Walter / the Bodies 0x10, the Wall boss), Wall Men, mm, Wheelchairs, the Patient, event spawns (a spawn
// mode other than the ordinary ones), dead bodies, a draw state of its own (another pre-draw), a ghost that comes out of
// the game's own portal (its wall work, or an open portal within 400 units) or with its own fade already running.
// A fading enemy acts as usual (no AI, hit box or damage change: the stock pop-in acts at once too).
// Fade + Portal: a ghost (victims, the story ghosts, their copies) that comes in after the room's first 1.5 s gets a
// blood-red vortex of Refined's own (procedural: additive crimson rings, the Spirit Vision portal's look, no game art)
// standing behind it, facing the camera, depth-tested against the room, opening as it starts to fade in and closing
// 0.6 s after it is in (drawn in the scene pass 0x4EFBFD, items.inc SparkleFrame, before Item Shine and the filter).
// Sources (log): room load (first seen in the room's first 1.5 s), follower (enemies.inc's watch list, or a victim tagged
// by the ghosts' room rule), wanderer, copy (Enemy Copies' ids), revive (Revive Enemies' notes, FiNoteRevive), ghost (a
// ghost the game brought in later), monster (a monster the game made later). One line per room visit with any of them.
#include "fadein_sim.h"

enum { FIM_L, FIM_V0, FIM_D0 = FIM_V0 + FI_MODE_N, FIM_NA = FIM_D0 + FI_MODE_N, FIM_N };
#define MSG_FI0 (MSG_V50 + 8)
typedef char FiTextsFit[FIM_N <= 8 ? 1 : -1];  // MSG_V50 + 8 .. 15 (BRIEF_COMMON: w4's ids)
static const char *const kFiDescTxt[FI_MODE_N] = { "Monsters and ghosts appear at once, as in the game.",
                                                   "Monsters and ghosts that appear fade in.",
                                                   "They fade in; ghosts rise from a blood portal." };
static BYTE g_txtFi[FIM_N][56];
static DWORD g_fiIds[FI_MODE_N];
static volatile LONG g_fiMode = FI_MODE_DEFAULT;  // the row (any thread writes it under gameplay.inc's lock)
static LONG g_fiSnap;
static int g_fiUnsaved, g_fiRow = -1;
static float g_fiSec = FI_SECONDS_DEFAULT;     // [Gameplay] FadeInSeconds (ini only)
static int g_fiReady;                          // the model / shadow hooks are in and 0x45C880 is the stock code
static int g_fiPortalOk;                       // the scene pass is in: the vortex can be drawn
static volatile int g_fiFail;                  // fail-closed: a fault turns Fade In off for the session
static DWORD g_fiFaultCode;
static void *g_fiFaultAt;
static FiTable g_fiTab;                        // this room visit's enemies
static FiStats g_fiSt;                         // this room visit's counts (the room line)
static FiNotes g_fiNotes;                      // Revive Enemies' ids of this room load
static double g_fiClock, g_fiRoomT0;           // the draw clock (s) and the room visit's first frame on it
static LONG g_fiClockFrame = -100;
static LARGE_INTEGER g_fiQpc;
static DWORD g_fiKey = 0xFFFFFFFFu;            // the room visit (enemies.inc's door serial, stage, room)
static int g_fiKeyStage, g_fiKeyRoom;
typedef struct { int on; float c[3]; double t0; } FiPortal;
static FiPortal g_fiPortal[FI_PORTAL_MAX];
static int g_fiPortalN;                        // vortices on (cheap test for the draw)
static Viewport8 g_fiVp;                       // the 3D viewport in force at the model draws (for the vortex)
static int g_fiVpOk;
static LONG g_fiVpStamp = -1;
static volatile LONG g_fiQuotaStart, g_fiQuotaRoom, g_fiQuotaDraw;
static int g_fiFullSaid;  // v3.50 FI350-001: the table was full of enemies drawn at once (said once a room visit)
#define FI_SEE_THROUGH(m, t) ((void(__cdecl *)(void *, float))0x45C880)((m), (t))  // the game's see-through enemy draw

static const BYTE *FiText(int id) { return id >= MSG_FI0 && id < MSG_FI0 + FIM_N ? g_txtFi[id - MSG_FI0] : g_txtFi[0]; }
static int FiFaultNote(EXCEPTION_POINTERS *e)
{
    g_fiFaultCode = e->ExceptionRecord->ExceptionCode;
    g_fiFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static void FiFailNow(const char *where)
{
    if (g_fiFail) return;
    g_fiFail = 1;
    Log("fade in: a fault in %s (code %08lX at %p): Fade In is off for this session (enemies are drawn as the game draws them)",
        where, g_fiFaultCode, g_fiFaultAt);
    LogAnomaly("fade in", "a fault in %s (code %08lX at %p): off for the session", where, g_fiFaultCode, g_fiFaultAt);
}

// ---------------------------------------------------------------- the clock and the room visit
static void FiClockTick(void)  // once a drawn frame (g_fxFrame)
{
    LARGE_INTEGER now, f;
    double dt = 0.0;
    if (g_fxFrame == g_fiClockFrame) return;
    QueryPerformanceCounter(&now);
    if (g_fiQpc.QuadPart && QueryPerformanceFrequency(&f) && f.QuadPart) dt = (double)(now.QuadPart - g_fiQpc.QuadPart) / (double)f.QuadPart;
    g_fiClock += FiClockStep(dt, g_fxFrame == g_fiClockFrame + 1);
    g_fiQpc = now;
    g_fiClockFrame = g_fxFrame;
}
static void FiRoomSay(void)  // the room visit's line (only when something faded or was kept from fading)
{
    char b[400];
    if (!FiStatsAny(&g_fiSt) || !LogQuota(&g_fiQuotaRoom, 600)) return;
    FiStatsText(&g_fiSt, b, sizeof b);
    Log("fade in: room %d:%d: %s", g_fiKeyStage, g_fiKeyRoom, b);
}
static void FiFrameTick(void)  // the model hooks and the scene pass: the clock, a new room visit
{
    const int stage = ENM_STAGE, room = ENM_ROOM;
    // a room visit = enemies.inc's door serial (+ 1 at every room load, before the room's script; the room words may change
    // earlier, while the old room is still drawn); without enemies.inc's room load hook: the stage and room
    const DWORD key = g_enmInstalled && !g_enmFail ? (DWORD)g_enmDoorSerial : 0x80000000u | (DWORD)(stage & 0xFF) << 8 | (DWORD)(room & 0xFF);
    FiClockTick();
    if (key == g_fiKey) return;
    FiRoomSay();
    g_fiKey = key, g_fiKeyStage = stage, g_fiKeyRoom = room;
    g_fiRoomT0 = g_fiClock;
    FiTableClear(&g_fiTab);
    g_fiFullSaid = 0;
    memset(&g_fiSt, 0, sizeof g_fiSt);
    memset(g_fiPortal, 0, sizeof g_fiPortal);
    g_fiPortalN = 0;
    FiNoteKeepRoom(&g_fiNotes, stage, room);
}
// enemies.inc EnmReviveRoom / EnmReviveAwake (game thread, the room load): an id that stands again in this room
static void FiNoteRevive(int stage, int room, unsigned id)
{
    if (g_fiReady && !g_fiFail) FiNotePut(&g_fiNotes, stage, room, id);
}

// ---------------------------------------------------------------- what enemies.inc knows of an id (read only)
static int FiArrivedKind(unsigned id, int room)  // its watch list: 0 follower, 1 wanderer, 2 copy follower, 3 wandering copy
{
    int i;
    for (i = 0; i < g_enmArrivedN && i < ENM_WATCH_MAX; ++i)
        if (g_enmArrived[i].id == id && g_enmArrived[i].room == room) return g_enmArrived[i].kind & 3;
    return -1;
}
static int FiGhostTagged(unsigned id, int stage)  // a victim the ghosts' room rule lets in as a follower
{
    int i;
    if (g_enmGhostTagStage != stage) return 0;
    for (i = 0; i < g_enmGhostTagN && i < ENM_FOLLOW_MAX; ++i)
        if (g_enmGhostTag[i] == id) return 1;
    return 0;
}
// a ghost that comes out of the game's own portal: its wall work (ghost block [work+0x28] +0x50), or any open portal task
// (update 0x487650, work +0 position, +0xEC open 0..1; KB 113) near its spot
static int FiGhostPortal(const BYTE *w, int placeholder)
{
    const BYTE *g = placeholder ? NULL : *(const BYTE *const *)(w + 0x28);  // (a placeholder has no ghost block: its follow block is +0x24)
    const void *wall = (ULONG_PTR)g >= 0x10000 ? *(const void *const *)(g + 0x50) : NULL;
    const float *p = (const float *)w;
    int i;
    for (i = 0; i < g_tpN; ++i) {
        const BYTE *t = TaskSlotAt(i), *pw;
        float dx, dy, dz;
        if (*(const DWORD *)(t + 0x404) != 0x487650) continue;
        pw = *(const BYTE *const *)(t + 0x408);
        if ((ULONG_PTR)pw < 0x10000 || !(*(const float *)(pw + 0xEC) > 0.02f)) continue;
        if (pw == wall) return 1;
        dx = ((const float *)pw)[0] - p[0], dy = ((const float *)pw)[1] - p[1], dz = ((const float *)pw)[2] - p[2];
        if (dx * dx + dz * dz < FI_PORTAL_NEAR * FI_PORTAL_NEAR && dy * dy < FI_PORTAL_NEAR * FI_PORTAL_NEAR) return 1;
    }
    return 0;
}
static void FiDrawSay(int type, DWORD fn)  // a type with a draw state of its own (once per callback, verbose)
{
    static DWORD s_seen[8];
    static int s_n;
    int i;
    for (i = 0; i < s_n; ++i)
        if (s_seen[i] == fn) return;
    if (s_n < 8) s_seen[s_n++] = fn;
    LogV("fade in: type %02X is drawn with a draw state of its own (pre-draw %08lX): drawn as the game draws it", type, fn);
}

// ---------------------------------------------------------------- first sight
static int FiPortalStart(const BYTE *w)
{
    const float *p = (const float *)w;
    int i, k = -1;
    if (!g_fiPortalOk || !_finite(p[0] + p[1] + p[2])) return 0;
    for (i = 0; i < FI_PORTAL_MAX; ++i)
        if (!g_fiPortal[i].on && k < 0) k = i;
    if (k < 0)  // all on: the oldest makes room
        for (k = 0, i = 1; i < FI_PORTAL_MAX; ++i)
            if (g_fiPortal[i].t0 < g_fiPortal[k].t0) k = i;
    if (!g_fiPortal[k].on) ++g_fiPortalN;
    g_fiPortal[k].on = 1;
    g_fiPortal[k].c[0] = p[0], g_fiPortal[k].c[1] = p[1] - 100.0f, g_fiPortal[k].c[2] = p[2];  // (y down: 100 up, mid-body)
    g_fiPortal[k].t0 = g_fiClock;
    return 1;
}
static FiEnt *FiFirst(const BYTE *task, const BYTE *w, unsigned id, int type, int pd, float own, DWORD fn)
{
    const int stage = ENM_STAGE, room = ENM_ROOM, phase = *(volatile int *)0xFD5C20, mode = (int)g_fiMode;
    const unsigned mw = EnmModeWord(task);
    const int ghost = FiGhostType(type), inGrace = g_fiClock - g_fiRoomT0 < FI_GRACE_S;
    const int arrived = FiArrivedKind(id, room), tagged = ghost && FiGhostTagged(id, stage);
    const int revived = FiNoteHas(&g_fiNotes, stage, room, id);
    int ownFade = pd == FI_PD_OWNFADE && own > 0.01f, ownPortal = 0, dead;
    FiEnt *e = FiAdd(&g_fiTab, task, w, id, g_fiClock);
    if (!e) {  // v3.50 FI350-001: the table is full of enemies drawn just now: this one is drawn as the game draws it
        if (!g_fiFullSaid++) LogV("fade in: %d enemies drawn at once in room %d:%d: more than the fade table holds (%d): the rest "
                                  "appear without a fade", FI_MAX, stage, room, FI_MAX);
        return NULL;
    }
    e->type = type;
    const int placeholder = *(const DWORD *)(task + 0x404) == 0x486030;  // the game's arriving victim (0x486070) before it is real
    if (ghost && !placeholder) {  // the ghost block's own see-through amount (GhostWork +0x8C, given to 0x45D490 each frame)
        const BYTE *g = *(const BYTE *const *)(w + 0x28);
        if ((ULONG_PTR)g >= 0x10000 && *(const float *)(g + 0x8C) > 0.01f) ownFade = 1;
    }
    dead = ENM_DEAD(task) != 0;
    if (ghost && mode != FI_MODE_OFF && !ownFade) ownPortal = FiGhostPortal(w, placeholder);
    e->skip = FiSkipWhy(mode != FI_MODE_OFF, stage >= 2 && stage <= 8, phase == 1, FiTypeFades(type), FiBoss(type, mw),
                        FiModeOk(type, mw), dead, pd != FI_PD_FOREIGN, ownPortal, ownFade);
    e->src = FiSource(inGrace, arrived, tagged, revived, EnmIsCopyId(id), ghost);
    if (e->skip) {
        if (e->skip != FI_SKIP_OFF) ++g_fiSt.skips[e->skip];
        if (e->skip == FI_SKIP_DRAW) FiDrawSay(type, fn);
        return e;  // (state FI_E_DONE: drawn as the game draws it)
    }
    e->state = FI_E_FADE;
    ++g_fiSt.fades[e->src];
    if (FiPortalFor(mode, type, inGrace) && FiPortalStart(w)) e->portal = 1, ++g_fiSt.portals;
    if (LogQuota(&g_fiQuotaStart, 400))
        LogV("fade in: %s %08X (type %02X) fades in over %.2f s%s at %.0f %.0f %.0f (room %d:%d, %.1f s into the visit)",
             kFiSrcName[e->src], id, type, g_fiSec, e->portal ? " from a blood portal" : "", ((const float *)w)[0],
             ((const float *)w)[1], ((const float *)w)[2], stage, room, g_fiClock - g_fiRoomT0);
    return e;
}
// the entry of an enemy actor drawn now (NULL: not an enemy actor); *pd / *own: its pre-draw kind and own see-through
static FiEnt *FiSee(const BYTE *task, int *pd, float *own)
{
    const BYTE *w, *rec, *model = NULL;
    DWORD fn = 0;
    unsigned id;
    int type, k, hidden;
    *pd = FI_PD_PLAIN, *own = 0.0f;
    if (task == *(const BYTE *const volatile *)0xFCE6B4 || task == *(const BYTE *const volatile *)0xFD5A50) return NULL;  // companions
    type = *(const short *)(task + 0x414);
    if (type < 1 || type > 0x12) return NULL;
    w = *(const BYTE *const *)(task + 0x408);
    if ((ULONG_PTR)w < 0x10000 || *(const short *)(w + 0x20) != type) return NULL;  // an enemy actor's own type (RE_thermal 1.1)
    FiFrameTick();
    rec = *(const BYTE *const *)(task + 0x41C);
    if ((ULONG_PTR)rec >= 0x10000) {
        fn = *(const DWORD *)(rec + 0x24);
        model = *(const BYTE *const *)(rec + 0x14);
    }
    if (model && model == *(const BYTE *const volatile *)(0x10A0F20 + 0x1B0)) return NULL;  // the player's model
    hidden = (ULONG_PTR)model < 0x10000 || (*(const WORD *)(model + 0xE4) & 4) != 0;  // no model, or flag 4: not drawn (0x55C420)
    *pd = FiPreDrawKind(fn);
    if (*pd == FI_PD_OWNFADE) *own = *(const float *)(w + 0x80);
    id = ENM_ID(task);
    k = FiFind(&g_fiTab, task, w, id);
    if (k >= 0) {
        FiAge(&g_fiTab.e[k], g_fiClock, hidden);  // a hidden one keeps its fade for when it shows
        return &g_fiTab.e[k];
    }
    if (hidden) return NULL;  // its first sight is its first visible draw (a ghost in a wall, a hidden type)
    {   // the same enemy in a new task (the game's arrival: its placeholder becomes the real ghost, 0x4852B0): it goes on
        FiEnt *e = FiTakeOver(&g_fiTab, task, w, id, g_fiClock);
        if (e) {
            if (LogQuota(&g_fiQuotaStart, 400))
                LogV("fade in: %08X (type %02X) is the same enemy in a new task (%s): %s", id, type,
                     e->type != type ? "another type" : "the game made it again", e->state == FI_E_FADE ? "its fade goes on" :
                     "nothing new fades");
            e->type = type;
            return e;
        }
    }
    FiDropTask(&g_fiTab, task);  // the slot held another enemy before
    return FiFirst(task, w, id, type, *pd, *own, fn);
}
static void FiVpCapture(void)
{
    void *dev = DEV;
    g_fiVpStamp = g_fxFrame;
    g_fiVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_fiVp)) && g_fiVp.w >= 16 && g_fiVp.h >= 16;
}

// ---------------------------------------------------------------- the hooks' side (screenfx.inc)
// ThermDrawFi (0x52AEB5, the stock draw would run): 1 = drawn here see-through (the stock call is skipped)
static int FiDrawC(const BYTE *task, BYTE *model)
{
    int done = 0, pd;
    float own;
    if (!g_fiReady || g_fiFail || !task || !model) return 0;
    __try {
        FiEnt *e = FiSee(task, &pd, &own);
        if (e && e->state == FI_E_FADE) {
            const float frac = FiFrac((float)(g_fiClock - e->t0), g_fiSec);
            if (g_fiMode == FI_MODE_OFF || frac >= 1.0f || *(volatile int *)0xFD5C20 == 1 || pd == FI_PD_FOREIGN) {
                e->state = FI_E_DONE;  // in, switched off, a cut-scene began, or a draw state of its own now: the game's draw
            } else {
                const float t = FiTransparency(frac, own);
                if (g_fiPortalN && g_fiVpStamp != g_fxFrame) FiVpCapture();
                if (t > 0.0f) FI_SEE_THROUGH(model, t);  // (t >= 1: 0x45C880 draws nothing, as the game's own fades)
                done = t > 0.0f;
            }
        }
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the model draw");
        done = 0;
    }
    return done;
}
// ThermShadowC (0x52AF95): 0 = no shadow yet (the body is less than half in)
static int FiShadowC(const BYTE *task)
{
    int ok = 1, pd;
    float own;
    if (!g_fiReady || g_fiFail || g_fiMode == FI_MODE_OFF || !task) return 1;
    __try {
        const FiEnt *e = FiSee(task, &pd, &own);
        if (e && e->state == FI_E_FADE) ok = FiShadowShown(FiFrac((float)(g_fiClock - e->t0), g_fiSec));
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the shadow draw");
        ok = 1;
    }
    return ok;
}

// ---------------------------------------------------------------- the vortex (scene pass, items.inc SparkleFrame)
static void FiCamProj(void)  // the exact stored depth from the camera's own projection (as Death Presence: DpCamProj)
{
    float P[16];
    memcpy(P, (const void *)0x12E3ED0, sizeof P);
    if (_finite(P[10] + P[11] + P[14] + P[15]) && fabsf(P[11]) > 0.5f && P[10] != 0.0f && P[10] != 1.0f) {
        memcpy(g_svP, P, sizeof g_svP);
        g_svPOk = 1;
    }
    g_svZScaleNow = 1.0f;
}
static int FiPortalMesh(void *dev, const Viewport8 *vp, const FiPortal *p, float open, float now)
{
    static const float kCore[3] = { 1.0f, 0.12f, 0.06f }, kEdge[3] = { 0.42f, 0.0f, 0.05f };
    enum { NR = 10, NS = 28 };
    GvHv g[2][NS + 1];
    float c[3], nn[3], u[3], a, b, l, gr;
    int r, s, cur = 0;
    nn[0] = g_svEye[0] - p->c[0], nn[1] = 0.0f, nn[2] = g_svEye[2] - p->c[2];  // toward the camera, level
    l = sqrtf(nn[0] * nn[0] + nn[2] * nn[2]);
    if (!(l > 1.0f)) return 0;
    nn[0] /= l, nn[2] /= l;
    u[0] = -nn[2], u[1] = 0.0f, u[2] = nn[0];
    c[0] = p->c[0] - 25.0f * nn[0], c[1] = p->c[1], c[2] = p->c[2] - 25.0f * nn[2];  // just behind the body
    gr = 0.3f + 0.7f * open;  // it grows as it opens
    a = 66.0f * gr, b = 110.0f * gr;  // half the victims' wall portal (120 x 200, KB 113), a little larger
    for (r = 0; r <= NR; ++r) {
        const float rn = (float)r / NR;
        float rgb[3];
        SvMix(rgb, kCore, kEdge, rn);
        for (s = 0; s <= NS; ++s) {
            const float th = 6.2831853f * (float)s / NS, cs = cosf(th), sn = sinf(th);
            float k = 0.30f * powf(1.0f - rn, 0.8f), q[3];
            int m;
            for (m = 0; m < 3; ++m) {  // three swirling rings, turning both ways
                const float ri = 0.35f + 0.28f * (float)m, rim = 1.0f - fabsf(rn - ri) / 0.09f;
                if (rim > 0.0f) k += rim * (0.55f + 0.45f * sinf(4.0f * th + (m & 1 ? -1.0f : 1.0f) * now * (2.0f + 0.7f * (float)m)));
            }
            q[0] = c[0] + rn * a * cs * u[0];
            q[1] = c[1] + rn * b * sn;
            q[2] = c[2] + rn * a * cs * u[2];
            SvPoint(&g[cur][s], q, k * 0.8f * open, rgb);
        }
        if (r)
            for (s = 0; s < NS; ++s) {
                GvTri(dev, &g[!cur][s], &g[!cur][s + 1], &g[cur][s + 1], vp, NULL);
                GvTri(dev, &g[!cur][s], &g[cur][s + 1], &g[cur][s], vp, NULL);
            }
        cur = !cur;
    }
    return 1;
}
static int FiPortalsExpire(void)  // the vortices whose time is over go (any Fade In value); returns those left
{
    const float life = FiPortalLife(g_fiSec);
    int i, live = 0;
    for (i = 0; i < FI_PORTAL_MAX; ++i) {
        if (g_fiPortal[i].on && g_fiClock - g_fiPortal[i].t0 > life) g_fiPortal[i].on = 0;
        live += g_fiPortal[i].on;
    }
    return g_fiPortalN = live;
}
static void FiPortalsDraw(void)
{
    void *dev = DEV;
    FxSaved sv;
    const Viewport8 *vp;
    float pull;
    int i, drawn = 0;
    if (!dev || !FxSave(dev, &sv, 0)) return;
    vp = g_fiVpOk && g_fiVpStamp == g_fxFrame ? &g_fiVp : &sv.vp;  // the 3D viewport the world was drawn in
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv.vpSet = 1;
    pull = g_svPull;
    __try {  // whatever happens below, the device states go back (a fault itself is caught by FiDrawFrame)
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);  // this frame's world -> GS matrix
        SvNearFar();
        FiCamProj();
        g_svEyeOk = SvEye(g_svEye);
        g_svPull = 0.0f;
        FxBase(dev);
        SvUntextured(dev);  // additive vertex colour
        SvZ(dev, 2);        // the visible part only (reversed Z: GREATEREQUAL), no z write
        for (i = 0; i < FI_PORTAL_MAX && g_svEyeOk; ++i)
            if (g_fiPortal[i].on) {
                const float open = FiPortalOpen((float)(g_fiClock - g_fiPortal[i].t0), g_fiSec);
                if (open > 0.002f) drawn += FiPortalMesh(dev, vp, &g_fiPortal[i], open, (float)g_fiClock);
            }
        GvFlush(dev);
    } __finally {
        g_svZScaleNow = SV_ZSCALE;  // Spirit Vision keeps its own calibration
        g_svPull = pull;
        SvZOff(dev);
        FxRestore(dev, &sv);
    }
    if (drawn && LogQuota(&g_fiQuotaDraw, 3))
        Log("fade in: a blood portal drawn (viewport %s %lux%lu, the exact depth of 012E3ED0)", vp == &g_fiVp ? "3D" : "saved", vp->w, vp->h);
}
static void FiDrawFrame(void)  // items.inc SparkleFrame (the scene pass 0x4EFBFD, game thread)
{
    if (!g_fiReady || g_fiFail) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay()) return;
    __try {
        FiFrameTick();  // a room visit with no enemy drawn yet starts its clock here
        if (g_fiPortalN && FiPortalsExpire() && g_fiMode == FI_MODE_PORTAL && g_fiPortalOk) FiPortalsDraw();
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the portal draw");
    }
}

// ---------------------------------------------------------------- the row and [Gameplay] FadeIn (gameplay.inc's lock)
static void FiSetLocked(int v)
{
    if (v < 0 || v >= FI_MODE_N) return;
    InterlockedExchange(&g_fiMode, v);
    g_fiUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "FadeIn", kFiModeName[v], g_iniPath)) {
        g_fiUnsaved = 1;
        Log("fade in: FadeIn %s applies now but could not be saved in SH4Refined.ini (error %lu)", kFiModeName[v], GetLastError());
    } else {
        Log("fade in: FadeIn %s", kFiModeName[v]);
    }
}
static int __cdecl FiIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    GpLock();
    v = StepValue((int)g_fiMode, FI_MODE_N);
    if (v >= 0) FiSetLocked(v);
    GpUnlock();
    return 0;
}
static int __cdecl FiGet(void) { return (int)g_fiMode; }
static int __cdecl FiDesc(void)
{
    const int v = (int)g_fiMode;
    if (!g_fiReady || g_fiFail) return MSG_FI0 + FIM_NA;
    return MSG_FI0 + FIM_D0 + (v >= 0 && v < FI_MODE_N ? v : 0);
}
static int FiEnemyRow(DWORD *t, int n)  // gameplay.inc GpBuildPages: the row on the Enemies page
{
    FillRow(t + 7 * n, FiIn, FiGet, g_fiIds, FI_MODE_N, MSG_FI0 + FIM_L, FiDesc);
    g_fiRow = n;
    return n + 1;
}
static void FiSnapshot(void) { g_fiSnap = g_fiMode; }
static int FiChanged(void) { return g_fiMode != g_fiSnap; }
static int FiRowChanged(int page, int row)  // -1 = not this row
{
    if (page != LP_ENEMY || row != g_fiRow || row < 0) return -1;
    return g_snapOn && g_fiMode != g_fiSnap;
}
static void FiDiscard(void)
{
    if (g_fiMode != g_fiSnap) FiSetLocked((int)g_fiSnap);
}
static void FiIniReread(void)  // gameplay.inc GpIniReread (the ini changed on disk; under the lock)
{
    char buf[32];
    int v;
    if (g_iniFallback || g_fiUnsaved) return;
    GetPrivateProfileStringA("Gameplay", "FadeIn", kFiModeName[g_fiMode], buf, sizeof buf, g_iniPath);
    v = FiModeParse(buf, NULL);
    if (v == g_fiMode) return;
    InterlockedExchange(&g_fiMode, v);
    g_fiSnap = v;
    Log("fade in: [Gameplay] FadeIn read again: %s", kFiModeName[v]);
}
static void FiInit(void)  // gameplay.inc GameplayInstall (Init, before the Options tables)
{
    char buf[32], sb[32];
    int v, known = 1;
    IniStr("Gameplay", "FadeIn", buf, sizeof buf);
    g_fiMode = FiModeParse(buf, &known);
    if (!known && buf[0])
        Log("fade in: [Gameplay] FadeIn \"%s\" is not Off / Fade / Fade + Portal: %s (the default)", buf, kFiModeName[g_fiMode]);
    GetPrivateProfileStringA("Gameplay", "FadeInSeconds", "", sb, sizeof sb, g_iniPath);
    g_fiSec = FiSecondsClamp(sb[0] ? (float)atof(sb) : 0.0f);
    Encode(g_txtFi[FIM_L], 56, "Fade In");
    for (v = 0; v < FI_MODE_N; ++v) {
        Encode(g_txtFi[FIM_V0 + v], 56, kFiModeName[v]);
        Encode(g_txtFi[FIM_D0 + v], 56, kFiDescTxt[v]);
        g_fiIds[v] = MSG_FI0 + FIM_V0 + v;
    }
    Encode(g_txtFi[FIM_NA], 56, "Not available: see SH4Refined.log (fade in).");
}

// ---------------------------------------------------------------- install (Init, after EnemiesInstall)
static void FadeInInstall(void)
{
    static const Site kHead = { 0x45C880, { 0xD9, 0x44, 0x24, 0x08, 0x83, 0xEC, 0x14, 0xD8, 0x1D, 0x64, 0xE0, 0x5C, 0x00 }, 13 };
    if (!g_thermHooks) {
        Log("fade in: OFF (the model draw hooks 0052AEB5 / 0052AF95 are not in: see the screen effects lines); enemies appear as "
            "the game makes them");
        return;
    }
    if (!SiteOk(&kHead) || memcmp((const void *)0x45C880, kFiSeeThrough, sizeof kFiSeeThrough)) {
        Log("fade in: OFF (0045C880, the game's see-through enemy draw, is not the stock code: another mod?); enemies appear as "
            "the game makes them");
        return;
    }
    g_fiPortalOk = g_fxOn && g_spInstalled;
    g_fiReady = 1;
    Log("fade in: in (no exe patch: the model draw 0052AEB5 and shadow 0052AF95 hooks; the game's see-through enemy draw "
        "0045C880; the portal %s); [Gameplay] FadeIn %s, FadeInSeconds %.2f; never bosses, Wall Men, Walter, event or "
        "cut-scene figures; a fading enemy acts as usual", g_fiPortalOk ? "from the scene pass 004EFBFD" : "NOT available (no "
        "scene pass)", kFiModeName[g_fiMode], g_fiSec);
}
