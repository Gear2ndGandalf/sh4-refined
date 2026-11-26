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
//
// v3.51 PT351 (USER_FEEDBACK_v350 item 7; staging/v351_feedback/w3_portal/PLAN.md): Fade + Portal's portal is now the
// game's own sticky tar portal, the one its victims tear out of walls with: a dark blot with runny drips and thin dark
// strands stretched from it to the ghost's body, which snap one by one as it pulls free. The game's wall portal is three
// draws that only read blocks (fadein_sim.h): Refined fills a WallGhost / WallGhostBody block of its own from its timeline
// and calls the grid draw 0x4868B0 and the blots / drips draw 0x486E80 after the game's pass 2 (phase 0's step list entry
// 0x62BC98 = 0x4EF900, wrapped by FiPhaseDraw: the same moment the game draws its own portals, the room's true depth);
// the portal's task update 0x487650 (its grab and damage) is never made or run. The strands are the ghost's own bone trail
// (victims and Cynthia have one, GhostWork +0x4C): started by 0x478440 at the portal's plane (state +0x7D = 1, as 0x482CC0
// does), stepped and drawn by the ghost's own code, snapped one by one (TrailItem +0x1A) and let go by 0x482CF0. The portal
// sits on a collision wall up to 180 u behind the ghost (seen from the camera; 0x561CD0), else in the air 70 u behind it.
// When the game's draw cannot be used (its textures F000F020 / F001F020 not loaded, FadeInPortalDraw=Refined, a fault):
// Refined's own tar portal (near black, a deep red core, black drips, 12 strands of its own that snap one by one).
// The hold: from the start of every tick (QsTick 0x4EF982, before the game's enemy pass) a held ghost's level move of the
// last tick is cut to 40 % (1.5 s), easing to its full speed at 2.5 s (fadein_sim.h FiHoldFactor; put back through the
// game's own place setter 0x45CF20), and its one-action-a-tick latch (work +0x5C bit 0, 0x45BBF0) is set, so the deferred
// grab / attack actions it posted are refused until it is out. The hold starts at the portal: the arrival's placeholder
// (0x486030: it stands, it cannot act) gets Refined's strands, the real ghost made from it (0x4852B0, same id) takes them
// and the rest of the hold over; a ghost that the game itself sends out of a wall (its own portal, GhostWork +0x50, or
// 0x482CC0's strands) is let go at once: the game's own portal does it. v3.50's crimson vortex stays as
// FadeInPortalDraw=Vortex (and when the step list is not stock).
#include "fadein_sim.h"

enum { FIM_L, FIM_V0, FIM_D0 = FIM_V0 + FI_MODE_N, FIM_NA = FIM_D0 + FI_MODE_N, FIM_N };
#define MSG_FI0 (MSG_V50 + 8)
typedef char FiTextsFit[FIM_N <= 8 ? 1 : -1];  // MSG_V50 + 8 .. 15 (BRIEF_COMMON: w4's ids)
static const char *const kFiDescTxt[FI_MODE_N] = { "Monsters and ghosts appear at once, as in the game.",
                                                   "Monsters and ghosts that appear fade in.",
                                                   "They fade in; ghosts rise from a blood portal." };
static const char kFiDescTar[] = "They fade in; ghosts tear free of a tar portal.";  // v3.51 PT351 (FadeInPortalDraw=Vortex: the line above)
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
// v3.51 PT351: a portal is v3.50's vortex or a tar portal (with the ghost it holds)
typedef struct {
    int on, kind;                   // FI_TD_*
    float c[3];                     // the vortex's centre (mid-body) / the tar portal's centre on its plane
    double t0, tr;                  // opened; the hold's start (< 0: not yet, the arrival's placeholder)
    float rel;                      // seconds after t0 when it lets go (the hold's end, an early release, FI_TAR_WAIT_S)
    float n[3], ax[3], ay[3];       // the plane: normal (toward the ghost), across, down
    float sx, sy, back;             // half sizes; how far behind the ghost it is
    int wall;                       // 1 on a collision wall, 0 in the air
    FiTarLayout L;
    const BYTE *task, *work;        // the ghost
    unsigned id;
    DWORD fn;                       // its task's update (the ghost is still that task)
    int type, placeholder, held, released, said;
    float prev[3];                  // the hold: where it was left at the last tick
    int havePrev;
    float moved, unheld, relAge;    // level distance it went / would have gone; the hold's age at the release
    int strands, nStr;              // FI_TS_*
    const BYTE *gb, *pool;          // the game's strands: its ghost block and bone trail ...
    float anchor[3];                // ... and the trail's plane point (it is still ours)
    float sa[FI_STR_OWN][3], sb[FI_STR_OWN][3];  // Refined's strands: anchors on the plane, bone points last seen
    int sbone[FI_STR_OWN];
    int drawN, drawnNow;            // frames its tar portal was drawn
    double drawMs;
    float drawMax;
} FiPortal;
static FiPortal g_fiPortal[FI_PORTAL_MAX];
static int g_fiTdm = FI_TDM_AUTO;              // v3.51 PT351: [Gameplay] FadeInPortalDraw (hidden)
static int g_fiHoldOn = 1;                     // v3.51 PT351: [Gameplay] FadeInHold (hidden; 0 = no hold, no latch)
static int g_fiPhaseIn;                        // v3.51 PT351: the step list's entry 0x62BC98 is FiPhaseDraw
static int g_fiGameOk, g_fiStrandOk, g_fiHoldFnOk;  // v3.51 PT351: the game code it calls is stock (fadein_sim.h kFiTarFns)
static volatile int g_fiGameFail;              // v3.51 PT351: the game's draw faulted once: Refined's own from then on
static DWORD g_fiGameFaultCode;
static void *g_fiGameFaultAt;
static volatile LONG g_fiQuotaHold, g_fiQuotaTar;
static __declspec(align(16)) BYTE g_fiWg[0x140], g_fiWb[0x3F0];  // v3.51 PT351: our WallGhost / WallGhostBody blocks ...
static __declspec(align(16)) BYTE g_fiTkA[0x430], g_fiTkB[0x430];  // ... and the task slots the two draws read (+0x408)
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
static void FiPortalsDropAll(void);  // v3.51 PT351 (below): a new room visit: the holds end (logged, no game call)
static void FiFrameTick(void)  // the model hooks and the scene pass: the clock, a new room visit
{
    const int stage = ENM_STAGE, room = ENM_ROOM;
    // a room visit = enemies.inc's door serial (+ 1 at every room load, before the room's script; the room words may change
    // earlier, while the old room is still drawn); without enemies.inc's room load hook: the stage and room
    const DWORD key = g_enmInstalled && !g_enmFail ? (DWORD)g_enmDoorSerial : 0x80000000u | (DWORD)(stage & 0xFF) << 8 | (DWORD)(room & 0xFF);
    FiClockTick();
    if (key == g_fiKey) return;
    FiPortalsDropAll();  // v3.51 PT351 (before the room line: its holds count)
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
// v3.51 PT351: which portal: v3.50's vortex (FadeInPortalDraw=Vortex, or the step list is not ours), the game's own draw
// (Auto, its code stock, no fault, its textures resident: the game's own victims' wall exits use the same two), Refined's
static int FiTexIn(void)
{
    typedef const void *(__cdecl *Lookup_t)(unsigned);
    return ((Lookup_t)0x5736D0)(FI_TEX_PORTAL) != NULL && ((Lookup_t)0x5736D0)(FI_TEX_STRAND) != NULL;
}
static int FiTarKind(void)
{
    if (g_fiTdm == FI_TDM_VORTEX || !g_fiPhaseIn) return FI_TD_VORTEX;
    if (g_fiTdm == FI_TDM_AUTO && g_fiGameOk && !g_fiGameFail && FiTexIn()) return FI_TD_GAME;
    return FI_TD_OWN;
}
// the tar portal's plane: the nearest collision wall behind the ghost as the camera sees it (straight back, 40 degrees
// either way, FI_TAR_WALL_R at mid-body; Spirit Vision's caster SvvCastT = the game's 0x561CD0), else the air
// FI_TAR_AIR_BACK behind it, facing the camera
static int FiTarPlane(FiPortal *p, const float *w)
{
    static const float kA[3] = { 0.0f, 0.7f, -0.7f };
    const float body[3] = { w[0], w[1], w[2] };  // the work position: the body's centre (200 above a victim's feet, KB)
    float eye[3], d[3], l, best = 1.0e9f;
    int k, j;
    if (!SvEye(eye)) return 0;
    d[0] = body[0] - eye[0], d[1] = 0.0f, d[2] = body[2] - eye[2];
    l = sqrtf(d[0] * d[0] + d[2] * d[2]);
    if (!(l > 1.0f)) return 0;
    d[0] /= l, d[2] /= l;
    p->wall = 0;
    for (k = 0; k < 3; ++k) {
        const float ca = cosf(kA[k]), sa = sinf(kA[k]);
        const float r[3] = { d[0] * ca - d[2] * sa, 0.0f, d[0] * sa + d[2] * ca };
        float p1[3], hit[3], nrm[3], dx, dz, dist;
        for (j = 0; j < 3; ++j) p1[j] = body[j] + r[j] * FI_TAR_WALL_R;
        if (!SvvCastT(body, p1, hit, nrm, 0.0f) || !FiWallOk(nrm)) continue;
        dx = hit[0] - body[0], dz = hit[2] - body[2];
        dist = sqrtf(dx * dx + dz * dz);
        if (dist < best) {
            best = dist;
            memcpy(p->c, hit, sizeof p->c);
            memcpy(p->n, nrm, sizeof p->n);
            p->wall = 1;
        }
    }
    if (!p->wall) {
        for (j = 0; j < 3; ++j) p->c[j] = body[j] + d[j] * FI_TAR_AIR_BACK, p->n[j] = -d[j];
        best = FI_TAR_AIR_BACK;
    }
    p->back = best;
    if (!FiPlaneAxes(p->n, p->ax, p->ay)) return 0;
    p->sx = p->wall ? 100.0f : 60.0f;  // (the victims' own portal: 120 x 200)
    p->sy = p->wall ? 150.0f : 95.0f;
    return 1;
}
static int FiPortalSlot(void)  // a free one; else the oldest that holds no ghost; -1 = every one holds one
{
    int i, k = -1;
    for (i = 0; i < FI_PORTAL_MAX; ++i)
        if (!g_fiPortal[i].on) return i;
    for (i = 0; i < FI_PORTAL_MAX; ++i)
        if (!(g_fiPortal[i].held && !g_fiPortal[i].released) && (k < 0 || g_fiPortal[i].t0 < g_fiPortal[k].t0)) k = i;
    return k;
}
// returns its index + 1 (0: none)
static int FiPortalStart(const BYTE *task, const BYTE *w, unsigned id, int type, int placeholder)
{
    const float *pw = (const float *)w;
    FiPortal *p;
    int k, kind = FiTarKind();
    if (!_finite(pw[0] + pw[1] + pw[2])) return 0;
    if (kind == FI_TD_VORTEX && !g_fiPortalOk) return 0;
    k = FiPortalSlot();
    if (k < 0) return 0;
    p = &g_fiPortal[k];
    if (!p->on) ++g_fiPortalN;
    memset(p, 0, sizeof *p);
    p->c[0] = pw[0], p->c[1] = pw[1] - 100.0f, p->c[2] = pw[2];
    if (kind != FI_TD_VORTEX && !FiTarPlane(p, pw)) {  // no camera: v3.50's vortex if it can be drawn
        p->c[0] = pw[0], p->c[1] = pw[1] - 100.0f, p->c[2] = pw[2];
        kind = FI_TD_VORTEX;
        if (!g_fiPortalOk) {
            p->on = 0;
            --g_fiPortalN;
            return 0;
        }
    }
    p->on = 1;
    p->kind = kind;
    p->t0 = g_fiClock;
    p->tr = -1.0;
    p->rel = FiTarRelease(-1.0f);
    p->task = task, p->work = w, p->id = id, p->type = type, p->placeholder = placeholder;
    p->fn = *(const DWORD *)(task + 0x404);
    FiTarLayoutMake(&p->L, FiHash(id ^ (unsigned)(g_fiClock * 1000.0) ^ 0x5EEDu));
    if (kind != FI_TD_VORTEX) p->tr = g_fiClock, p->rel = FiTarRelease(0.0f);  // (the hold starts with the portal)
    ++g_fiSt.byDraw[kind];
    return k + 1;
}
static void FiPortalSay(const FiPortal *p, char *b, size_t n)  // the start line's part (logcheck reads it)
{
    if (p->kind == FI_TD_VORTEX) {
        _snprintf_s(b, n, _TRUNCATE, " from a blood portal");
        return;
    }
    _snprintf_s(b, n, _TRUNCATE, " from a tar portal (%s, %s %.0f u behind; %s)", kFiTdName[p->kind],
                p->wall ? "on a wall" : "in the air", p->back,
                !g_fiHoldOn || !g_qsTickOn ? "not held" : p->placeholder ? "held 2.50 s, the arrival's placeholder" :
                "held 2.50 s, 40 % for 1.50 s");
}
// the same enemy in a new task (FiTakeOver): the portal follows it (the arrival's placeholder became the real ghost: the
// hold, Refined's strands and the action latch go on in it)
static void FiPortalFollow(FiPortal *p, const BYTE *task, const BYTE *w, unsigned id, int type)
{
    const int was = p->placeholder;
    if (!p->on || p->kind == FI_TD_VORTEX || p->id != id || p->released) return;
    p->task = task, p->work = w, p->type = type;
    p->fn = *(const DWORD *)(task + 0x404);
    p->havePrev = 0;
    p->placeholder = p->fn == 0x486030;
    if (was && !p->placeholder && LogQuota(&g_fiQuotaHold, 400))
        LogV("fade in: %08X (type %02X) is real now, %.2f s after its tar portal opened: its hold goes on", id, type,
             g_fiClock - p->t0);
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
    // v3.51 PT351: e->portal = the portal's index + 1 (a tar portal holds the ghost)
    if (FiPortalFor(mode, type, inGrace) && (e->portal = FiPortalStart(task, w, id, type, placeholder)) != 0) ++g_fiSt.portals;
    if (LogQuota(&g_fiQuotaStart, 400)) {
        char pt[160] = "";
        if (e->portal) FiPortalSay(&g_fiPortal[e->portal - 1], pt, sizeof pt);
        LogV("fade in: %s %08X (type %02X) fades in over %.2f s%s at %.0f %.0f %.0f (room %d:%d, %.1f s into the visit)",
             kFiSrcName[e->src], id, type, g_fiSec, pt, ((const float *)w)[0],
             ((const float *)w)[1], ((const float *)w)[2], stage, room, g_fiClock - g_fiRoomT0);
    }
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
            if (e->portal > 0 && e->portal <= FI_PORTAL_MAX) FiPortalFollow(&g_fiPortal[e->portal - 1], task, w, id, type);  // v3.51 PT351
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
static int FiPortalsExpire(void)  // the portals whose time is over go (any Fade In value); returns those left
{
    const float life = FiPortalLife(g_fiSec);
    int i, live = 0;
    for (i = 0; i < FI_PORTAL_MAX; ++i) {
        FiPortal *p = &g_fiPortal[i];
        if (p->on) {
            const float age = (float)(g_fiClock - p->t0);
            if (p->kind == FI_TD_VORTEX) {
                if (age > life) p->on = 0;
            } else if (FiTarDone(age, p->rel) && !(p->held && !p->released)) {  // v3.51 PT351: a holding one waits for its tick
                if (p->placeholder && LogQuota(&g_fiQuotaHold, 400))
                    LogV("fade in: %08X (type %02X): its tar portal closed after %.1f s: it never came out (still the arrival's "
                         "placeholder)", p->id, p->type, age);
                p->on = 0;
            }
        }
        live += p->on;
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
            if (g_fiPortal[i].on && g_fiPortal[i].kind == FI_TD_VORTEX) {  // v3.51 PT351: the tar portals draw after pass 2
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
static int FiAnyVortex(void)  // v3.51 PT351
{
    int i;
    for (i = 0; i < FI_PORTAL_MAX; ++i)
        if (g_fiPortal[i].on && g_fiPortal[i].kind == FI_TD_VORTEX) return 1;
    return 0;
}
static void FiDrawFrame(void)  // items.inc SparkleFrame (the scene pass 0x4EFBFD, game thread)
{
    if (!g_fiReady || g_fiFail) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay()) return;
    __try {
        FiFrameTick();  // a room visit with no enemy drawn yet starts its clock here
        if (g_fiPortalN && FiPortalsExpire() && g_fiMode == FI_MODE_PORTAL && g_fiPortalOk && FiAnyVortex()) FiPortalsDraw();  // v3.51 PT351: vortices only
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the portal draw");
    }
}

// ================================================================ v3.51 PT351: the hold and the strands (the tick)
// the held task is still that ghost: its update, its work block, its spawn id (a freed slot made again fails one)
static int FiAlive(const FiPortal *p)
{
    return (ULONG_PTR)p->task >= 0x10000 && *(const DWORD *)(p->task + 0x404) == p->fn &&
           *(const BYTE *const *)(p->task + 0x408) == p->work && ENM_ID(p->task) == p->id;
}
// the ghost's bone trail is still the one we started (its ghost block, the trail, state 1, our plane point)
static int FiStrandsOurs(const FiPortal *p)
{
    const BYTE *g;
    if (p->strands != FI_TS_GAME || !FiAlive(p)) return 0;
    g = *(const BYTE *const *)(p->work + 0x28);
    return g == p->gb && *(const BYTE *const *)(g + 0x4C) == p->pool && *(const signed char *)(g + 0x7D) == 1 &&
           !memcmp(p->pool + 0x18, p->anchor, sizeof p->anchor);
}
// bone b of the ghost's model this frame ([work + 0x30] = its model; [model + 0xC0] the bone matrices, 0x40 each, the
// position at +0x30; [model + 0xE0] + 0x14 the bone count); 0 = none, or not near the body
static int FiBone(const BYTE *work, int b, float o[3])
{
    const BYTE *model = *(const BYTE *const *)(work + 0x30), *skel;
    const float *mats, *w = (const float *)work;
    int nb = 25;
    if ((ULONG_PTR)model < 0x10000) return 0;
    mats = *(const float *const *)(model + 0xC0);
    skel = *(const BYTE *const *)(model + 0xE0);
    if ((ULONG_PTR)mats < 0x10000) return 0;
    if ((ULONG_PTR)skel >= 0x10000) nb = *(const int *)(skel + 0x14);
    if (b < 0 || b >= nb || b >= 64) return 0;
    memcpy(o, mats + 16 * b + 12, 12);
    return _finite(o[0] + o[1] + o[2]) && fabsf(o[0] - w[0]) < 400.0f && fabsf(o[1] - w[1]) < 400.0f && fabsf(o[2] - w[2]) < 400.0f;
}
// Refined's own strands: up to FI_STR_OWN bones in front of the plane, each tied to the point of the plane behind it
static void FiOwnStrandsStart(FiPortal *p)
{
    const unsigned s0 = p->L.seed;
    int tries, n = 0;
    for (tries = 0; tries < 48 && n < FI_STR_OWN; ++tries) {
        const int b = (int)((s0 + (unsigned)tries * 7u) % 24u);
        float q[3], d;
        int j, k, dup = 0;
        for (k = 0; k < n; ++k) dup |= p->sbone[k] == b;
        if (dup || !FiBone(p->work, b, q)) continue;
        d = (q[0] - p->c[0]) * p->n[0] + (q[1] - p->c[1]) * p->n[1] + (q[2] - p->c[2]) * p->n[2];
        if (d < 5.0f) continue;  // behind the plane (in the wall): no strand
        {
            const unsigned h = FiHash(s0 + (unsigned)n * 977u);
            const float ju = (FiRnd01(h) - 0.5f) * 48.0f, jv = (FiRnd01(h ^ 1u) - 0.5f) * 48.0f;
            for (j = 0; j < 3; ++j) p->sa[n][j] = q[j] - p->n[j] * d + p->ax[j] * ju + p->ay[j] * jv;
        }
        memcpy(p->sb[n], q, sizeof q);
        p->sbone[n++] = b;
    }
    p->nStr = n;
    p->strands = n ? FI_TS_OWN : FI_TS_NONE;
}
// the hold's first tick: the ghost's own strands from the portal's plane to its bones (0x478440 as 0x482CC0 calls it, with
// our plane for its wall point; state +0x7D = 1: its update steps them, its record draw 0x482D60 shows them), else ours
static void FiStrandsStart(FiPortal *p)
{
    p->strands = FI_TS_NONE;
    if (p->kind == FI_TD_GAME && g_fiStrandOk && FiGhostType(p->type) && !p->placeholder) {
        const BYTE *g = *(const BYTE *const *)(p->work + 0x28);
        BYTE *pool = (ULONG_PTR)g >= 0x10000 ? *(BYTE *const *)(g + 0x4C) : NULL;
        if ((ULONG_PTR)pool >= 0x10000 && *(const signed char *)(g + 0x7D) == 0 && *(const BYTE *const *)(pool + 0x0C) == p->task) {
            static __declspec(align(16)) float s_pos[4], s_dir[4];
            int j;
            for (j = 0; j < 3; ++j) s_pos[j] = p->c[j], s_dir[j] = p->n[j];
            s_pos[3] = 1.0f, s_dir[3] = 0.0f;
            ((void(__cdecl *)(void *, float *, float *))0x478440)(pool, s_pos, s_dir);
            p->nStr = *(const int *)(pool + 8);  // (PoolList count: 32 for a victim; fewer if the heap is short)
            if (p->nStr > 0) {
                *(volatile signed char *)(g + 0x7D) = 1;
                p->gb = g, p->pool = pool;
                memcpy(p->anchor, pool + 0x18, sizeof p->anchor);
                p->strands = FI_TS_GAME;
            }
        }
    }
    if (p->strands == FI_TS_NONE && p->kind != FI_TD_VORTEX) FiOwnStrandsStart(p);
}
// the ghost's own strands snap one by one (fadein_sim.h FiSnapAt): TrailItem +0x1A loose = 1, as its update does when a
// bone pulls too far; its update then fades the strand back into the wall
static void FiStrandsSnap(FiPortal *p, float age)
{
    BYTE *it;
    int size, i = 0;
    if (!FiStrandsOurs(p)) return;
    size = *(const int *)(p->pool + 4);  // (PoolList: head, item size = the link word's offset, count)
    if (size < 0x24 || size > 0x40) return;
    for (it = *(BYTE *const *)p->pool; (ULONG_PTR)it >= 0x10000 && i < FI_STR_MAX; it = *(BYTE *const *)(it + size), ++i)
        if (!it[0x1A] && FiSnapAt(i, p->nStr, p->L.seed) <= age) it[0x1A] = 1;
}
// the end of a hold: why NULL = held to the end; mayCall = the ghost's own strands may be let go (0x482CF0: every strand
// loose, its state 2, as its own wall states do); a release before the end closes the portal from now
static void FiRelease(FiPortal *p, const char *why, int mayCall)
{
    const float age = p->tr >= 0.0 ? (float)(g_fiClock - p->tr) : 0.0f;
    if (p->released) return;
    p->released = 1;
    p->relAge = age;
    if (why) {
        const float now = (float)(g_fiClock - p->t0);
        if (now < p->rel) p->rel = now;
    }
    if (mayCall && FiStrandsOurs(p)) ((void(__cdecl *)(const BYTE *))0x482CF0)(p->task);
    if (p->held && LogQuota(&g_fiQuotaHold, 400))
        LogV("fade in: %08X (type %02X) let go by its tar portal (%s) after %.2f s (%s): strands: %s %d; moved %.0f of %.0f u; no "
             "attack for %.2f s; portal draw %.3f ms a frame (max %.3f, %d frames)", p->id, p->type, kFiTdName[p->kind], age,
             why ? why : "held to the end", kFiTsName[p->strands], p->nStr, p->moved, p->unheld, g_fiHoldOn ? age : 0.0f,
             p->drawN ? p->drawMs / p->drawN : 0.0, p->drawMax, p->drawN);
}
// the game sends this ghost out of a wall by itself: its own wall portal (FiGhostPortal: GhostWork +0x50, or an open one
// near), or its own strands started by 0x482CC0 while ours are not the ghost's own (Refined's / none)
static int FiGameTook(const FiPortal *p)
{
    const BYTE *g;
    if (!FiGhostType(p->type)) return 0;
    if (FiGhostPortal(p->work, p->placeholder)) return 1;
    if (p->placeholder || p->strands == FI_TS_GAME) return 0;
    g = *(const BYTE *const *)(p->work + 0x28);
    return (ULONG_PTR)g >= 0x10000 && *(const signed char *)(g + 0x7D) != 0;
}
static void FiHoldTick(FiPortal *p)
{
    const float age = (float)(g_fiClock - p->tr);
    BYTE *w;
    if (!FiAlive(p)) {
        FiRelease(p, "it is gone", 0);
        return;
    }
    if (g_fiMode != FI_MODE_PORTAL) {
        FiRelease(p, "Fade In changed", 1);
        return;
    }
    if (age >= FI_HOLD_END_S) {
        FiRelease(p, NULL, 1);
        return;
    }
    w = (BYTE *)p->work;
    if (FiGameTook(p)) {
        FiRelease(p, "the game's own portal took it", 1);  // it comes out of a wall by itself: no second portal
        return;
    }
    if (!p->held) {
        p->held = 1;
        ++g_fiSt.held;
        FiStrandsStart(p);
    }
    if (p->strands == FI_TS_GAME) FiStrandsSnap(p, age);
    if (!g_fiHoldOn) return;
    // 0x45BBF0's once-a-tick latch (EnemyWork +0x5C bit 0; its update's pre-update 0x45DD80 clears it): the grab / attack
    // actions it posted last tick, run by EnemyManagerFrame right after this, are refused while it is held
    *(volatile int *)(w + 0x5C) |= 1;
    if (g_fiHoldFnOk && DpUpdatesNow(p->task)) {  // (EntityGate: only on its own update ticks)
        float cur[3], out[3];
        memcpy(cur, w, sizeof cur);
        if (!_finite(cur[0] + cur[1] + cur[2])) return;
        if (!p->havePrev) {
            memcpy(p->prev, cur, sizeof cur);
            p->havePrev = 1;
            return;
        }
        {
            const float f = FiHoldFactor(age), dx = cur[0] - p->prev[0], dz = cur[2] - p->prev[2], d = sqrtf(dx * dx + dz * dz);
            if (!FiHoldStep(p->prev, cur, f, out)) {
                p->unheld += d;
                p->moved += f * d;
                if (fabsf(out[0] - cur[0]) + fabsf(out[2] - cur[2]) > 0.01f) {
                    static __declspec(align(16)) float s_v[4];
                    s_v[0] = out[0], s_v[1] = out[1], s_v[2] = out[2], s_v[3] = ((const float *)w)[3];
                    ((void(__cdecl *)(const BYTE *, float *))0x45CF20)(p->task, s_v);  // the game's place setter: work + model
                }
            }
            memcpy(p->prev, out, sizeof out);
        }
    }
}
static void FiTick(void)  // quicksave.inc QsTick (0x4EF982: the start of every main-loop tick, before the enemies' pass)
{
    int i, ph;
    if (!g_fiReady || g_fiFail || !g_fiPortalN) return;
    ph = *(volatile int *)0xFD5C20;
    if (ph != 0 && ph != 1) return;  // a pause / menu: nothing moves, nothing posted runs
    __try {
        for (i = 0; i < FI_PORTAL_MAX; ++i) {
            FiPortal *p = &g_fiPortal[i];
            if (!p->on || p->kind == FI_TD_VORTEX || p->tr < 0.0 || p->released) continue;
            if (ph == 1) FiRelease(p, "a cut-scene began", 1);
            else FiHoldTick(p);
        }
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the hold");
    }
}
// v3.51 MERGE (EP351 x PT351): this enemy is in a tar portal's hold now (the hold started, not let go, younger than
// FI_HOLD_END_S, FadeInHold on), by its task or its spawn id (the arrival's placeholder and the real ghost made from it).
// pursuit_room.inc EpOne leaves such a ghost alone: Enemy Pursuit's in-room timers start when the hold ends, and it never
// sets the state of, withdraws the requests of, or moves a ghost the hold slows down. Reads Refined's own table only.
static int FiHolds(const BYTE *task, unsigned id)
{
    int i;
    if (!g_fiReady || g_fiFail || !g_fiPortalN || !g_fiHoldOn) return 0;
    for (i = 0; i < FI_PORTAL_MAX; ++i) {
        const FiPortal *p = &g_fiPortal[i];
        if (!p->on || p->kind == FI_TD_VORTEX || p->tr < 0.0 || p->released || !(g_fiClock - p->tr < FI_HOLD_END_S)) continue;
        if (p->task == task || (id && p->id == id)) return 1;
    }
    return 0;
}
static void FiPortalsDropAll(void)  // a new room visit (FiFrameTick): its holds end, no game call (the room's tasks go)
{
    int i;
    for (i = 0; i < FI_PORTAL_MAX; ++i)
        if (g_fiPortal[i].on && g_fiPortal[i].held && !g_fiPortal[i].released) FiRelease(&g_fiPortal[i], "the room changed", 0);
}

// ================================================================ v3.51 PT351: the tar portal's draws (after pass 2)
// the game's own: our WallGhost (0x140) and WallGhostBody (0x3F0) blocks, laid out as ghost_484730.h has them, filled from
// the timeline; only the fields its two draws read (the update 0x487650 / 0x486A80, the attack, the task: never)
#define FI_WF(b, o) (*(float *)((b) + (o)))
#define FI_WS(b, o) (*(short *)((b) + (o)))
static void FiGameBlocks(const FiPortal *p, float age, float amt)
{
    BYTE *A = g_fiWg, *B = g_fiWb;
    const float now = (float)g_fiClock, grow = 0.55f + 0.45f * amt, k = p->wall ? 1.0f : 0.55f;
    const int shown = FiTarShown(age);
    int i, j;
    memset(A, 0, sizeof g_fiWg);
    memset(B, 0, sizeof g_fiWb);
    for (j = 0; j < 3; ++j) {
        FI_WF(A, 4 * j) = p->c[j], FI_WF(A, 0x10 + 4 * j) = p->n[j];        // pos, dir
        FI_WF(B, 4 * j) = p->c[j], FI_WF(B, 0x1F0 + 4 * j) = p->n[j];
    }
    FI_WF(A, 0x0C) = 1.0f, FI_WF(B, 0x0C) = 1.0f;
    FI_WF(A, 0x40) = p->sx, FI_WF(A, 0x44) = p->sy;                         // param.width / width2 (not read by the draw)
    for (i = 0; i < 9; ++i) {  // strip / strip2: the grid's columns and rows, each at least its left neighbour, a slow wobble
        float u = ((float)i - 4.0f) * p->sx * grow * 0.25f, v = ((float)i - 4.0f) * p->sy * grow * 0.25f;
        if (i != 0 && i != 8) {
            u += p->sx * 0.03f * sinf(1.3f * now + 1.7f * (float)i);
            v += p->sy * 0.03f * sinf(1.1f * now + 2.3f * (float)i);
        }
        if (i && u < FI_WF(A, 0x5C + 4 * (i - 1))) u = FI_WF(A, 0x5C + 4 * (i - 1));
        if (i && v < FI_WF(A, 0xA4 + 4 * (i - 1))) v = FI_WF(A, 0xA4 + 4 * (i - 1));
        FI_WF(A, 0x5C + 4 * i) = u;
        FI_WF(A, 0xA4 + 4 * i) = v;
    }
    FI_WF(A, 0xEC) = amt;  // amount; spring[4]: the four texture layers' strength (128 = whole), as its springs follow it
    for (i = 0; i < 4; ++i)
        FI_WF(A, 0xF0 + 4 * i) = (1.0f - (1.0f - amt) * (1.0f - amt)) * 128.0f * (0.88f + 0.12f * sinf(0.9f * now + 1.9f * (float)i));
    *(int *)(A + 0x118) = 1;  // state: open (unk_110 / unk_114 0: one face, flat)
    *(int *)(A + 0x124) = 1;  // mode (3 would draw nothing)
    for (i = 0; i < FI_TAR_SPLATS; ++i) *(signed char *)(B + 0x3CC + i) = -1;  // no drip
    for (i = 0; i < 16; ++i) *(int *)(B + 0x314 + 4 * i) = -1;
    for (i = 0; i < shown; ++i) {
        const FiSplat *s = &p->L.s[i];
        float *pt = (float *)(B + 0x10 + 16 * i);                           // the 25 points (w = the turn)
        for (j = 0; j < 3; ++j) pt[j] = p->c[j] + p->ax[j] * s->u * p->sx * grow + p->ay[j] * s->v * p->sy * grow;
        pt[3] = s->ang;
        FI_WF(B, 0x230 + 4 * i) = s->sc * k;                                // size
        FI_WS(B, 0x354 + 2 * i) = FiTarLifeShort(FiTarLife(age, p->rel, s->off));  // life x 1024
        if (s->drip > 0 && s->drip <= FI_TAR_DRIPS) {
            const FiDrip *d = &p->L.d[(int)s->drip];
            *(signed char *)(B + 0x3CC + i) = s->drip;                       // its drip slot
            *(int *)(B + 0x314 + 4 * s->drip) = d->kind;                     // piece 2 / 3
            FI_WF(B, 0x294 + 4 * s->drip) = FiDripLen(age - FiTarAppear(i), d->len * k);  // length run down
            FI_WS(B, 0x386 + 2 * s->drip) = FiTarLifeShort(FiTarLife(age, p->rel, d->off));
        }
    }
    FI_WF(B, 0x3B4) = FI_TAR_FADE_S;  // the fade time (unk_3B0 0: on the plane; unk_3C0 0: no floor blots; unk_3C2 0: flat)
    FI_WS(B, 0x3BC) = (short)shown;
    FI_WS(B, 0x3BE) = (short)p->L.nd;
    FI_WS(B, 0x3C6) = 1;
    *(BYTE **)(g_fiTkA + 0x408) = A;  // task->work_ptr, all the two draws read of their task
    *(BYTE **)(g_fiTkB + 0x408) = B;
}
static int FiGameFaultNote(EXCEPTION_POINTERS *e)
{
    g_fiGameFaultCode = e->ExceptionRecord->ExceptionCode;
    g_fiGameFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static int FiGameDraw(void *dev, FiPortal *p)
{
    static const DWORD kRs[3] = { 14 /*ZWRITEENABLE*/, 22 /*CULLMODE*/, 28 /*FOGENABLE*/ };  // what its 2D flush sets raw
    DWORD rs[3];
    const float age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel);
    int k, got = 0, ok = 0;
    if (!(amt > 0.002f)) return 0;
    FiGameBlocks(p, age, amt);
    for (k = 0; k < 3; ++k)
        if (SUCCEEDED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[k], &rs[k]))) got |= 1 << k;
    __try {
        ((void(__cdecl *)(BYTE *))0x4868B0)(g_fiTkA);  // EffectWallGhostDraw: the 9 x 9 grid in its four texture layers
        ((void(__cdecl *)(BYTE *))0x486E80)(g_fiTkB);  // the blots and the drips (EffectDrawDecal)
        ok = 1;
    } __except (FiGameFaultNote(GetExceptionInformation())) {
        int i;
        g_fiGameFail = 1;
        for (i = 0; i < FI_PORTAL_MAX; ++i)
            if (g_fiPortal[i].kind == FI_TD_GAME) g_fiPortal[i].kind = FI_TD_OWN;
        Log("fade in: a fault in the game's own portal draw (code %08lX at %p): Refined's own tar portal from now on",
            g_fiGameFaultCode, g_fiGameFaultAt);
        LogAnomaly("fade in", "a fault in the game's own portal draw (code %08lX at %p): Refined's own from now on",
                   g_fiGameFaultCode, g_fiGameFaultAt);
    }
    for (k = 0; k < 3; ++k)
        if (got >> k & 1) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[k], rs[k]);
    return ok;
}

// Refined's own: untextured vertex colour; darken = the frame x (1 - colour) (near black, its red kept in the core), or add
static void FiDarkStates(void *dev, int add)
{
    SvUntextured(dev);
    if (!add) {
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 1 /*ZERO*/);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 4 /*INVSRCCOLOR*/);
    }
}
// a camera-facing strip through n points (half widths hw; at least about a pixel), strength k, colour rgb
static void FiRibbon(void *dev, const Viewport8 *vp, const float (*P)[3], const float *hw, int n, float k, const float rgb[3])
{
    GvHv a0, b0, a1, b1;
    int i, j;
    for (i = 0; i < n; ++i) {
        const int i0 = i ? i - 1 : 0, i1 = i < n - 1 ? i + 1 : n - 1;
        float t[3], e[3], s[3], q0[3], q1[3], l, de, h = hw[i];
        for (j = 0; j < 3; ++j) t[j] = P[i1][j] - P[i0][j], e[j] = g_svEye[j] - P[i][j];
        s[0] = t[1] * e[2] - t[2] * e[1], s[1] = t[2] * e[0] - t[0] * e[2], s[2] = t[0] * e[1] - t[1] * e[0];
        l = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
        de = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        if (!(l > 1e-6f)) return;
        if (h < 0.0007f * de) h = 0.0007f * de;
        for (j = 0; j < 3; ++j) q0[j] = P[i][j] + s[j] / l * h, q1[j] = P[i][j] - s[j] / l * h;
        SvPoint(&a1, q0, k, rgb);
        SvPoint(&b1, q1, k, rgb);
        if (i) {
            GvTri(dev, &a0, &b0, &b1, vp, NULL);
            GvTri(dev, &a0, &b1, &a1, vp, NULL);
        }
        a0 = a1, b0 = b1;
    }
}
// the blot (a ragged near-black disc, a deep red core) and its drips (black, running down, a bead at the tip)
static int FiOwnBlot(void *dev, const Viewport8 *vp, FiPortal *p)
{
    enum { NR = 7, NS = 24 };
    static const float kCore[3] = { 0.60f, 0.97f, 0.96f }, kEdge[3] = { 0.93f, 0.95f, 0.95f }, kDrip[3] = { 0.90f, 0.96f, 0.96f };
    const float now = (float)g_fiClock, age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel);
    const float grow = 0.35f + 0.65f * FiTarOpenAt(age), k = p->wall ? 1.0f : 0.6f;
    GvHv g[2][NS + 1];
    float edge[NS + 1], o[3];
    int r, s, i, j, m, cur = 0;
    if (!(amt > 0.002f)) return 0;
    for (j = 0; j < 3; ++j) o[j] = p->c[j] + p->n[j] * 3.0f;  // a little off the wall
    for (s = 0; s <= NS; ++s) {  // a ragged outline, crawling slowly
        const float th = 6.2831853f * (float)(s % NS) / NS;
        edge[s] = 0.70f + 0.30f * FxValueNoise(2.2f * cosf(th) + 5.3f, 2.2f * sinf(th) + 0.12f * now, p->L.seed);
    }
    for (r = 0; r <= NR; ++r) {
        const float rn = (float)r / NR, kk = amt * (rn < 0.8f ? 0.96f : 0.96f * (1.0f - (rn - 0.8f) / 0.2f));
        float rgb[3];
        SvMix(rgb, kCore, kEdge, powf(rn, 0.6f));
        for (s = 0; s <= NS; ++s) {
            const float th = 6.2831853f * (float)s / NS, e = rn * edge[s] * grow;
            float q[3];
            for (j = 0; j < 3; ++j) q[j] = o[j] + p->ax[j] * cosf(th) * e * p->sx + p->ay[j] * sinf(th) * e * p->sy;
            SvPoint(&g[cur][s], q, kk, rgb);
        }
        if (r)
            for (s = 0; s < NS; ++s) {
                GvTri(dev, &g[!cur][s], &g[!cur][s + 1], &g[cur][s + 1], vp, NULL);
                GvTri(dev, &g[!cur][s], &g[cur][s + 1], &g[cur][s], vp, NULL);
            }
        cur = !cur;
    }
    for (i = 0; i < FiTarShown(age); ++i) {
        const FiSplat *sp = &p->L.s[i];
        const FiDrip *d;
        float top[3], P[8][3], hw[8], len, a, w0;
        if (sp->drip <= 0 || sp->drip > FI_TAR_DRIPS) continue;
        d = &p->L.d[(int)sp->drip];
        len = FiDripLen(age - FiTarAppear(i), d->len * k * 0.8f);
        a = FiTarLifeAlpha(FiTarLife(age, p->rel, d->off)) * FiTarOpenAt(age);
        if (len < 2.0f || a < 0.01f) continue;
        for (j = 0; j < 3; ++j)
            top[j] = o[j] + p->n[j] + p->ax[j] * sp->u * p->sx * grow * 0.85f + p->ay[j] * sp->v * p->sy * grow * 0.85f;
        w0 = 6.0f * d->w * k;
        for (m = 0; m < 8; ++m) {
            const float t = (float)m / 7.0f, wig = 1.5f * sinf(3.0f * t + sp->ang);
            for (j = 0; j < 3; ++j) P[m][j] = top[j] + p->ax[j] * wig;
            P[m][1] += len * t;  // straight down (y points down)
            hw[m] = w0 * (1.0f - 0.65f * t) + w0 * 0.55f * expf(-(1.0f - t) * (1.0f - t) / 0.012f);  // a bead at the tip
        }
        FiRibbon(dev, vp, (const float(*)[3])P, hw, 8, 0.95f * a, kDrip);
    }
    p->drawnNow = 1;
    return 1;
}
static void FiOwnCore(void *dev, const Viewport8 *vp, const FiPortal *p)  // its faint deep red core (additive)
{
    enum { NR = 3, NS = 16 };
    static const float kRed[3] = { 0.34f, 0.015f, 0.02f };
    const float age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel), grow = 0.35f + 0.65f * FiTarOpenAt(age);
    const float pulse = 0.85f + 0.15f * sinf(2.1f * (float)g_fiClock);
    GvHv g[2][NS + 1];
    float o[3];
    int r, s, j, cur = 0;
    if (!(amt > 0.002f)) return;
    for (j = 0; j < 3; ++j) o[j] = p->c[j] + p->n[j] * 4.0f;
    for (r = 0; r <= NR; ++r) {
        const float rn = (float)r / NR, kk = amt * pulse * (1.0f - rn) * (1.0f - rn);
        for (s = 0; s <= NS; ++s) {
            const float th = 6.2831853f * (float)s / NS, e = rn * 0.38f * grow;
            float q[3];
            for (j = 0; j < 3; ++j) q[j] = o[j] + p->ax[j] * cosf(th) * e * p->sx + p->ay[j] * sinf(th) * e * p->sy;
            SvPoint(&g[cur][s], q, kk, kRed);
        }
        if (r)
            for (s = 0; s < NS; ++s) {
                GvTri(dev, &g[!cur][s], &g[!cur][s + 1], &g[cur][s + 1], vp, NULL);
                GvTri(dev, &g[!cur][s], &g[cur][s + 1], &g[cur][s], vp, NULL);
            }
        cur = !cur;
    }
}
// Refined's strands: thin, near black, sagging a little; each thins, snaps and whips back at its moment (FiSnapAt; a
// release before the end snaps the rest one by one at once)
static int FiOwnStrands(void *dev, const Viewport8 *vp, FiPortal *p)
{
    static const float kStr[3] = { 0.90f, 0.96f, 0.96f };
    const float ageR = p->tr >= 0.0 ? (float)(g_fiClock - p->tr) : -1.0f;
    const int alive = !p->released && FiAlive(p);
    int i, j, m, drawn = 0;
    if (ageR < 0.0f || !p->held) return 0;
    for (i = 0; i < p->nStr; ++i) {
        float at = FiSnapAt(i, p->nStr, p->L.seed), k, b[3], P[9][3], hw[9], len, sag;
        int ph;
        if (p->released && at > p->relAge + 0.03f * (float)i) at = p->relAge + 0.03f * (float)i;
        ph = FiStrandPhase(ageR, at, &k);
        if (ph == 3) continue;
        if (alive && FiBone(p->work, p->sbone[i], b)) memcpy(p->sb[i], b, sizeof b);
        memcpy(b, p->sb[i], sizeof b);
        len = sqrtf((b[0] - p->sa[i][0]) * (b[0] - p->sa[i][0]) + (b[1] - p->sa[i][1]) * (b[1] - p->sa[i][1]) +
                    (b[2] - p->sa[i][2]) * (b[2] - p->sa[i][2]));
        sag = (ph == 0 ? 0.07f : 0.02f) * len;
        if (ph <= 1) {  // whole (or straining: taut and thinning)
            for (m = 0; m < 9; ++m) {
                const float t = (float)m / 8.0f;
                for (j = 0; j < 3; ++j) P[m][j] = p->sa[i][j] + (b[j] - p->sa[i][j]) * t;
                P[m][1] += sag * 4.0f * t * (1.0f - t);
                hw[m] = (1.5f - 0.8f * t) * k;
            }
            FiRibbon(dev, vp, (const float(*)[3])P, hw, 9, 0.92f, kStr);
        } else {  // snapped: each end whips back toward the wall / the body, thinning out
            float mid[3];
            int e;
            for (j = 0; j < 3; ++j) mid[j] = 0.5f * (p->sa[i][j] + b[j]);
            mid[1] += sag;
            for (e = 0; e < 2; ++e) {
                const float *from = e ? b : p->sa[i];
                for (m = 0; m < 5; ++m) {
                    const float t = (float)m / 4.0f * (1.0f - k);
                    for (j = 0; j < 3; ++j) P[m][j] = from[j] + (mid[j] - from[j]) * t;
                    hw[m] = (1.2f - 0.6f * (float)m / 4.0f) * (1.0f - k);
                }
                if (1.0f - k > 0.02f) FiRibbon(dev, vp, (const float(*)[3])P, hw, 5, 0.92f * (1.0f - k), kStr);
            }
        }
        ++drawn;
    }
    if (drawn) p->drawnNow = 1;
    return drawn;
}
static int FiOwnDraw(void *dev, Viewport8 *vpOut)  // Refined's blots, drips and strands in one bracket (every state back)
{
    FxSaved sv;
    float pull;
    int i, drawn = 0;
    if (!FxSave(dev, &sv, 0)) return 0;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);  // the 3D viewport in force here (FxSave set the full screen)
    sv.vpSet = 1;
    *vpOut = sv.vp;
    pull = g_svPull;
    __try {  // whatever happens below, the device states go back (a fault itself is caught by FiPass2Draw)
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);  // this frame's world -> GS matrix
        SvNearFar();
        FiCamProj();  // the exact depth: right after pass 2 the depth buffer is the room's and the bodies' own
        g_svEyeOk = SvEye(g_svEye);
        g_svPull = 0.0f;
        if (g_svEyeOk) {
            FxBase(dev);
            FiDarkStates(dev, 0);
            SvZ(dev, 2);  // the visible part only (GREATEREQUAL), no z write
            for (i = 0; i < FI_PORTAL_MAX; ++i) {
                FiPortal *p = &g_fiPortal[i];
                if (!p->on) continue;
                if (p->kind == FI_TD_OWN) drawn += FiOwnBlot(dev, &sv.vp, p);
                if (p->strands == FI_TS_OWN) drawn += FiOwnStrands(dev, &sv.vp, p) > 0;
            }
            GvFlush(dev);
            FiDarkStates(dev, 1);
            for (i = 0; i < FI_PORTAL_MAX; ++i)
                if (g_fiPortal[i].on && g_fiPortal[i].kind == FI_TD_OWN) FiOwnCore(dev, &sv.vp, &g_fiPortal[i]);
            GvFlush(dev);
        }
    } __finally {
        g_svZScaleNow = SV_ZSCALE;  // Spirit Vision keeps its own calibration
        g_svPull = pull;
        SvZOff(dev);
        FxRestore(dev, &sv);
    }
    return drawn;
}
static void FiPass2Draw(void)  // FiPhaseDraw: right after the game's pass 2 (where its own wall portals draw), game thread
{
    LARGE_INTEGER a, b, f;
    Viewport8 vp;
    void *dev;
    int i, game = 0, own = 0, need = 0;
    if (!g_fiReady || g_fiFail || !g_fiPortalN || g_fiMode != FI_MODE_PORTAL) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay() || !(dev = DEV)) return;
    QueryPerformanceCounter(&a);
    memset(&vp, 0, sizeof vp);
    __try {
        FiFrameTick();
        if (FiPortalsExpire()) {
            for (i = 0; i < FI_PORTAL_MAX; ++i) {
                FiPortal *p = &g_fiPortal[i];
                p->drawnNow = 0;
                if (!p->on || p->kind == FI_TD_VORTEX) continue;
                if (p->kind == FI_TD_GAME && FiGameDraw(dev, p)) p->drawnNow = 1, game = 1;
                if (p->kind == FI_TD_OWN || p->strands == FI_TS_OWN) need = 1;
            }
            if (need) own = FiOwnDraw(dev, &vp);
            if (!vp.w) ((Viewport_t)VT(dev, FXV_GETVP))(dev, &vp);
        }
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the tar portal draw");
        return;
    }
    if (!game && !own) return;
    QueryPerformanceCounter(&b);
    if (QueryPerformanceFrequency(&f) && f.QuadPart) {  // the cost (the room line, the release line; budget.inc's ledger too)
        const float ms = (float)((double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart);
        ++g_fiSt.drawFrames;
        g_fiSt.drawMs += ms;
        if (ms > g_fiSt.drawMax) g_fiSt.drawMax = ms;
        for (i = 0; i < FI_PORTAL_MAX; ++i)
            if (g_fiPortal[i].on && g_fiPortal[i].drawnNow) {
                ++g_fiPortal[i].drawN;
                g_fiPortal[i].drawMs += ms;
                if (ms > g_fiPortal[i].drawMax) g_fiPortal[i].drawMax = ms;
            }
    }
    if (LogQuota(&g_fiQuotaTar, 3))
        Log("fade in: a tar portal drawn (%s, viewport 3D %lux%lu, after the game's pass 2 004EF900)",
            kFiTdName[game ? FI_TD_GAME : FI_TD_OWN], vp.w, vp.h);
}
static void __cdecl FiPhaseDraw(void)  // phase 0's step 8 (0x62BC98): the game's own 0x4EF900, then the tar portals
{
    ((void(__cdecl *)(void))FI_PHASE_FN)();
    BG_RUN(BGC_FX, FiPass2Draw());
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
// v3.51 PT351-001 (Codex): DllMain's DLL_PROCESS_DETACH: the step list's entry 0x62BC98 back to the stock 0x4EF900 before
// the dll's code goes away (a FreeLibrary while the game runs); left alone when another component has replaced it since
static void FiUninstall(void)
{
    const DWORD fn = (DWORD)(ULONG_PTR)FiPhaseDraw, st = FI_PHASE_FN;
    if (!g_fiPhaseIn) return;
    __try {
        // v3.51 PT351-002 (Codex): the state clears only on a restore read back; FiInit pinned the dll, so a failed restore
        // cannot be followed by an unmap during the game (a detach then only happens at process exit)
        if (*(const DWORD *)FI_PHASE_VA == fn && Poke(FI_PHASE_VA, &st, 4) && *(const DWORD *)FI_PHASE_VA == st)
            g_fiPhaseIn = 0;
        else if (*(const DWORD *)FI_PHASE_VA != fn)
            g_fiPhaseIn = 0;  // another component replaced it since: not ours to touch
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
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
    // v3.51 PT351: the hidden keys FadeInPortalDraw (Auto / Refined / Vortex) and FadeInHold (1 / 0)
    GetPrivateProfileStringA("Gameplay", "FadeInPortalDraw", "", sb, sizeof sb, g_iniPath);
    g_fiTdm = FiTdmParse(sb, &known);
    if (!known) Log("fade in: [Gameplay] FadeInPortalDraw \"%s\" is not Auto / Refined / Vortex: Auto (the default)", sb);
    g_fiHoldOn = GetPrivateProfileIntA("Gameplay", "FadeInHold", 1, g_iniPath) != 0;
    Encode(g_txtFi[FIM_L], 56, "Fade In");
    for (v = 0; v < FI_MODE_N; ++v) {
        Encode(g_txtFi[FIM_V0 + v], 56, kFiModeName[v]);
        Encode(g_txtFi[FIM_D0 + v], 56, v == FI_MODE_PORTAL && g_fiTdm != FI_TDM_VORTEX ? kFiDescTar : kFiDescTxt[v]);  // v3.51 PT351
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
    {   // v3.51 PT351: the tar portal: the game code it calls (checked, never written) and the step list's entry 8 (data,
        // byte-checked, put back on a failed write; not listed in SH4WM_Patches: like EntityGate's entries 7 / 14, data)
        const char *why = NULL;
        char tar[400];
        int i, ok[3] = { 1, 1, 1 };
        __try {
            for (i = 0; i < FI_TAR_NFN; ++i)
                if (memcmp((const void *)(ULONG_PTR)kFiTarFns[i].va, kFiTarFns[i].b, sizeof kFiTarFns[i].b)) {
                    if (PeHookedCallee(kFiTarFns[i].va, "fade in")) continue;  // v3.51 MERGE: Playable Eileen's hook (0x5736D0) is fine, as drops.inc
                    ok[kFiTarFns[i].group] = 0;
                    Log("fade in: %08X (%s) is not the stock code (another mod?): the tar portal does without it", kFiTarFns[i].va,
                        kFiTarFns[i].name);
                }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok[0] = ok[1] = ok[2] = 0;
        }
        g_fiGameOk = ok[0], g_fiStrandOk = ok[1], g_fiHoldFnOk = ok[2];
        if (g_fiTdm == FI_TDM_VORTEX) {
            why = "FadeInPortalDraw=Vortex";
        } else if (*(const DWORD *)FI_PHASE_VA != FI_PHASE_FN) {
            why = "0062BC98 is not the stock 004EF900: another mod?";
        } else {
            const DWORD fn = (DWORD)(ULONG_PTR)FiPhaseDraw, st = FI_PHASE_FN;
            HMODULE pin = NULL;
            // v3.51 PT351-002 (Codex): pin the dll before the game's step list points into it: a FreeLibrary while the
            // game runs then never unmaps FiPhaseDraw (no pin = no hook)
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                    (LPCSTR)(ULONG_PTR)FiPhaseDraw, &pin)) {
                why = "the dll could not be pinned in memory";
            } else if (Poke(FI_PHASE_VA, &fn, 4) && *(const DWORD *)FI_PHASE_VA == fn) {
                g_fiPhaseIn = 1;
            } else {
                Poke(FI_PHASE_VA, &st, 4);
                why = "0062BC98 could not be written";
            }
        }
        if (g_fiPhaseIn)
            _snprintf_s(tar, sizeof tar, _TRUNCATE, "after the game's pass 2 (its step list 0062BC98): %s; strands: %s; %s",
                        g_fiTdm == FI_TDM_REFINED ? "Refined's draw (FadeInPortalDraw=Refined)"
                        : g_fiGameOk ? "the game's own wall-portal draw 004868B0 / 00486E80 (textures F000F020 / F001F020), else "
                                       "Refined's" : "Refined's draw (the game's is not stock)",
                        g_fiStrandOk ? "the ghost's own 00478440 / 00482CF0, else Refined's" : "Refined's (the bone trail is not stock)",
                        !g_fiHoldOn ? "no hold (FadeInHold=0)" : !g_qsTickOn ? "no hold (the tick 004EF982 is not in)"
                        : g_fiHoldFnOk ? "the hold from the tick 004EF982 (0045CF20 and the action latch)"
                                       : "the hold: the action latch only (0045CF20 is not stock)");
        else
            _snprintf_s(tar, sizeof tar, _TRUNCATE, "NOT available (%s): v3.50's blood vortex", why);
        Log("fade in: in (no code patch: the model draw 0052AEB5 and shadow 0052AF95 hooks; the game's see-through enemy draw "
            "0045C880; the portal %s; the tar portal %s); [Gameplay] FadeIn %s, FadeInSeconds %.2f, FadeInPortalDraw %s, "
            "FadeInHold %d; never bosses, Wall Men, Walter, event or cut-scene figures; a fading enemy acts as usual; a held "
            "ghost cannot grab", g_fiPortalOk ? "from the scene pass 004EFBFD" : "NOT available (no scene pass)", tar,
            kFiModeName[g_fiMode], g_fiSec, kFiTdmName[g_fiTdm], g_fiHoldOn);
    }
}
