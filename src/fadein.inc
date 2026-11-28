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
//
// v3.58 FI358 (USER_FEEDBACK_v357 Improvements: "it is smaller than the victims ... add to the effect with more similar
// effects on and around it ... stretchy tar-like substance stick to the victims for a few seconds and let the portal stay
// open for that long too"; staging/v358_feedback/w5_portal/PLAN.md). Reveal Method's Portal: in the tar portal's draw
// (after the game's pass 2, this frame's bones) the arriving body is measured (its bones, else its type's box after
// 0.35 s) and the portal is laid out to 1.24 x its height: the first portal at v3.57's size plus up to FadeInPortalPieces
// (9) more of the same around it (below, above, the sides, the corners, one over it: the game's own grid 0x4868B0 called
// once a piece, or Refined's disc; each at most the first one's size, never one stretched piece; on a wall a piece that
// is not on the wall is left out) and two more blot sets over the whole (the game's 0x486E80, 25 blots and their drips
// each). Refined's sticky strands (FadeInStrands 16; 8 beside a victim's / Cynthia's own 32) hold on to its bones (else
// points of its box) and follow it for FadeInStrandSeconds (3 s) after it is in: thinner as they age and stretch, more
// sag, a blob of tar where each holds on; they snap one by one over the last 1.3 s (one stretched past 900 u at once),
// the wall end whipping back, the body end dropping off. The ghost's own strands go on as long (0x482CF0 then). The
// portal stays open until then and closes, the pieces around first (it shrinks to its centre). The hold (2.5 s, slowed,
// no attack) and Enemy Pursuit's start after it are unchanged. The effect budget's spark levels thin the pieces around
// it (60 / 35 %) and the two outer blot sets. One verbose line when a portal closes (its pieces, size against the body,
// the strands' time). Monsters keep the fade only: the game has no portal of its own for them (WallGhostCreate is the
// ghosts'). FadeInStrandSeconds 0 + FadeInPortalPieces 0 = v3.57's size and timeline (the strands keep the new look).
//
// v3.59 FI359 (USER_FEEDBACK_v358 QA 9: "the portal clips to the floor in the subway restrooms while the victim fly's out
// of it to a new spot with its legs floating above it folded in"; Issues: "Spirit Vision still uses the old portal
// effect"; staging/v359_feedback/w6_portal/PLAN.md). The floor: the game's floor ray 0x459490 under the arrival spot (and
// the collision segment 0x561CD0 for a ceiling over it) keeps the whole portal, its blots and its drips above the floor (it
// is moved up its plane; a low ceiling makes it as tall as the room allows, the first portal smaller, never squashed). The
// victim (type 0B): one still coming out of a wall by itself (its AI state 9, steps 3 .. 5: its appearance, held at its
// wall point's height, legs folded, then rising) with no wall portal of the game's near it is kept unseen until it is out
// (at most 3 s; it posts no grab before that), then its portal opens where it is; at its hold's start (still unseen) a
// victim lower than its own height (size x 180 above the floor) is lifted to it (0x45CF20; its height spring reset). One
// verbose "arrives" line a portal. Spirit Vision: Refined's portals (pieces, blots, drips, its sticky strands) and the
// game's own wall portals (v3.17's crimson vortex before) are drawn as spirit-blue tar in its pass, after the edge world
// (the scene's tar draw is skipped there: the edge world replaced it). Thermal: a ghost hidden by Thermal is noted at its
// first sight too (its portal opens as a cold, dark void; v3.58 gave every ghost of the room a portal when Thermal was
// switched off). Night Vision / Night Mono: the tar is drawn before the filter, which amplifies it as it does Item Shine.
// Hidden [Gameplay] FadeInSpiritLook=0: v3.58's Spirit Vision (the vortex).
#include "fadein_sim.h"

enum { FIM_L, FIM_V0, FIM_D0 = FIM_V0 + FI_MODE_N, FIM_NA = FIM_D0 + FI_MODE_N, FIM_N };
#define MSG_FI0 (MSG_V50 + 8)
typedef char FiTextsFit[FIM_N <= 8 ? 1 : -1];  // MSG_V50 + 8 .. 15 (BRIEF_COMMON: w4's ids)
static const char *const kFiDescTxt[FI_MODE_N] = { OLW_ENEMIES_REVEAL_METHOD_VD_OFF, OLW_ENEMIES_REVEAL_METHOD_VD_FADE_IN,  // v3.53 OL1: the plan's lines  // v3.58 MN358: Reveal Method (was Fade In), Fade In (was Fade)
                                                   OLW_FADEIN_V_VORTEX };
static const char kFiDescTar[] = OLW_ENEMIES_REVEAL_METHOD_VD_PORTAL;  // v3.53 OL1  // v3.58 MN358  // v3.51 PT351 (FadeInPortalDraw=Vortex: the line above)
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
    float sa[FI_STRANDS_MAX][3], sb[FI_STRANDS_MAX][3];  // Refined's strands: anchors on the plane, body points last seen
    int sbone[FI_STRANDS_MAX];      // (v3.58 FI358: up to FI_STRANDS_MAX; -1 = a point of its box, sbo)
    int drawN, drawnNow;            // frames its tar portal was drawn
    double drawMs;
    float drawMax;
    // v3.58 FI358: the bigger, stickier portal (fadein_sim.h FiBig / FiBody / FiStick*)
    float sEnd;                     // the strands' end (s after the hold's start): the portal closes from here
    float sCut;                     // an early end of the strands (the hold let go early, the ghost gone; < 0 none)
    int strEnded;                   // the ghost's own strands let go (0x482CF0) or not ours any more
    int measured;                   // the body was measured (its bones, else its type) and the pieces laid out
    FiBody body;
    FiBig big;
    FiTarLayout L2[FI_SETS_MAX - 1];   // the blot sets over the whole portal
    int nOwn, ownWant, ownStarted, ownBox;  // Refined's sticky strands (beside the ghost's own: half as many)
    float sbo[FI_STRANDS_MAX][3];   // a box strand's point from the work position
    float sat[FI_STRANDS_MAX], len0[FI_STRANDS_MAX], sdrop[FI_STRANDS_MAX][3];  // snap moment, first length, where it snapped
    int sdropSet[FI_STRANDS_MAX], nLoose;
    int gridsMax, setsMax;          // the most pieces / blot sets drawn in one frame (the close line)
    // v3.59 FI359: the floor under it, a ceiling over it, the fit, the arriving victim (the arrive line)
    int floorOk, ceilOk;            // the floor (0x459490) under the arrival spot / a ceiling (0x561CD0) over it
    float floorY, ceilY;            // their world y (y down)
    float raised, fitK;             // the portal moved up its plane (u); the first portal's scale under a low ceiling
    float lift, waited;             // the victim lifted to its own height (u); kept unseen before (s)
    float above0, rest0;            // when first seen: its centre above the floor, its own height (a victim; else 0)
    int ai0, step0, anim0, said59;  // its AI state, step and animation when first seen; the arrive line said
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
static float g_fiStick = FI_STICK_DEFAULT;     // v3.58 FI358: [Gameplay] FadeInStrandSeconds (hidden)
static int g_fiPieces = FI_PIECES_DEFAULT;     // v3.58 FI358: [Gameplay] FadeInPortalPieces (hidden)
static int g_fiStrN = FI_STRANDS_DEFAULT;      // v3.58 FI358: [Gameplay] FadeInStrands (hidden)
static volatile LONG g_fiQuotaClose;           // v3.58 FI358: the close lines
static float g_fiDtLast, g_fiFrameEma = 1000.0f / 60.0f;  // v3.58 FI358: the last drawn frame's step (s); frames (ms, smoothed)
static float g_fiMsEma, g_fiSelfK = 1.0f;      // v3.58 FI358: the portals' draw (ms a frame, smoothed); their own share
static volatile LONG g_fiQuotaSelf;
static int g_fiFloorFnOk;                      // v3.59 FI359: 0x459490 (the enemies' floor ray) is stock: the portal on the floor
static int g_fiSvLook = 1;                     // v3.59 FI359: [Gameplay] FadeInSpiritLook (hidden): Spirit Vision's spirit tar
static const FiPal *g_fiPal = &kFiPalTar;      // v3.59 FI359: the colours Refined's own tar draw uses now
static volatile LONG g_fiQuotaArrive, g_fiQuotaWait;  // v3.59 FI359: the arrive lines, the wall-exit waits
static float g_fiWaitedNow;                    // v3.59 FI359: FiSee -> FiPortalStart: how long the one seen now was kept unseen
typedef struct { const BYTE *task; unsigned id; double t0; } FiWait;  // v3.59 FI359: victims kept unseen until they are out
static FiWait g_fiWait[FI_WAIT_N];
static int g_fiSvSaid;                         // v3.59 FI359: Spirit Vision's look said once
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
    g_fiDtLast = (float)FiClockStep(dt, g_fxFrame == g_fiClockFrame + 1);  // v3.58 FI358 (kept for the portals' own limit)
    if (g_fxFrame == g_fiClockFrame + 1 && dt > 0.0 && dt < 0.25)
        g_fiFrameEma += ((float)dt * 1000.0f - g_fiFrameEma) * 0.05f;
    g_fiClock += g_fiDtLast;
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
    memset(g_fiWait, 0, sizeof g_fiWait);  // v3.59 FI359
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
// v3.59 FI359: the floor under (x, z) at or below y0 (the game's floor ray 0x459490, as Spawn Site and Copies use it);
// 1 = *fy holds it (within FI_FLOOR_REACH below y0)
static int FiFloorUnder(float x, float y0, float z, float *fy)
{
    __declspec(align(16)) float q[4];
    __declspec(align(16)) EnmFloorHit hit;
    if (!g_fiFloorFnOk || !_finite(x + y0 + z)) return 0;
    memset(&hit, 0, sizeof hit);
    q[0] = x, q[1] = y0, q[2] = z, q[3] = 1.0f;
    if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, q) != 1 || !_finite(hit.pos[1])) return 0;
    if (!(hit.pos[1] >= y0 - 1.0f && hit.pos[1] <= y0 + FI_FLOOR_REACH)) return 0;
    *fy = hit.pos[1];
    return 1;
}
// v3.59 FI359: a ceiling straight above c (the collision segment 0x561CD0, Spirit Vision's SvvCastT): a level surface
static int FiCeilOver(const float c[3], float *cy)
{
    const float top[3] = { c[0], c[1] - FI_CEIL_REACH, c[2] };
    float hit[3], nrm[3];
    if (!SvvCastT(c, top, hit, nrm, 0.0f) || !(fabsf(nrm[1]) > 0.7f)) return 0;
    *cy = hit[1];
    return 1;
}
// v3.59 FI359: v (down the plane, from its point) of a world height y; 0 = the plane is not upright enough to say
static int FiPlaneV(const FiPortal *p, float y, float *v)
{
    if (!(p->ay[1] > 0.3f)) return 0;
    *v = (y - p->c[1]) / p->ay[1];
    return 1;
}
static void FiPlaneMove(FiPortal *p, float dv)  // v3.59 FI359: the plane's point moved dv down its plane (- = up)
{
    int j;
    for (j = 0; j < 3; ++j) p->c[j] += p->ay[j] * dv;
}
// v3.59 FI359: the first portal (before the body is measured) above the floor under the arrival spot
static void FiFirstOnFloor(FiPortal *p, const float *w)
{
    float vF, vc, ty, k;
    p->fitK = 1.0f;
    if (!FiFloorUnder(w[0], w[1] - FI_FLOOR_FROM, w[2], &p->floorY)) return;
    p->floorOk = 1;
    if (!FiPlaneV(p, p->floorY - FI_FLOOR_GAP, &vF)) return;
    if (FiFloorFit(vF, -1e30f, 0.0f, p->sy, p->sy, &vc, &ty, &k) & FI_FIT_RAISED) {
        FiPlaneMove(p, vc);
        p->raised += -vc;
    }
}
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
    for (i = 0; i < FI_PORTAL_MAX; ++i)  // v3.58 FI358: nor one whose ghost's own strands are still ours (they hold on after the hold)
        if (!(g_fiPortal[i].held && (!g_fiPortal[i].released || !g_fiPortal[i].strEnded)) &&
            (k < 0 || g_fiPortal[i].t0 < g_fiPortal[k].t0)) k = i;
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
    // v3.58 FI358: the strands hold to sEnd and the portal stays open as long; the first portal alone until the body is
    // measured (FiMeasure, in the draw: this frame's bones); the blot sets over the whole portal
    p->sEnd = FiStickEnd(g_fiSec, g_fiStick);
    p->sCut = -1.0f;
    if (kind != FI_TD_VORTEX) p->rel = p->sEnd;
    FiBigMake(&p->big, p->L.seed, p->sx, p->sy, p->sx, p->sy, 0.0f, 0.0f, 0);
    FiTarLayoutMake(&p->L2[0], FiHash(p->L.seed ^ 0xA11Cu));
    FiTarLayoutMake(&p->L2[1], FiHash(p->L.seed ^ 0xB22Du));
    // v3.59 FI359: the first portal above the floor; the arrival as it was seen (the arrive line)
    p->fitK = 1.0f;
    p->waited = g_fiWaitedNow;
    p->ai0 = ENM_AISTATE(task), p->step0 = *(const WORD *)(task + 0x412), p->anim0 = *(const int *)(w + 0x48);
    if (kind != FI_TD_VORTEX) {
        FiFirstOnFloor(p, pw);
        if (p->floorOk) {
            p->above0 = p->floorY - pw[1];
            if (type == FI_VICTIM_TYPE) p->rest0 = p->floorY - FiRestY(p->floorY, *(const float *)(w + 0x60));
        }
    }
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
    // v3.58 FI358: after the hold too while Refined's strands still hold on to it (they follow the body)
    if (!p->on || p->kind == FI_TD_VORTEX || p->id != id || (p->released && (p->nOwn == 0 || p->sCut >= 0.0f))) return;
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
// v3.59 FI359: a victim seen for the first time while it still comes out of a wall by itself (its appearance at its wall
// point's height, legs folded, before it rises: AI state 9 steps 3 .. 5) with no wall portal of the game's near it is kept
// unseen (Reveal Method Portal, a ghost's arrival, not the room load) until it is out or FI_WAIT_MAX_S passed; then its
// first sight is now (g_fiWaitedNow: how long it waited). It posts no grab before (0x4B28C0: state 9 below step 6).
static FiEnt g_fiWaitEnt;  // (FiSee's answer for such a one: drawn by nobody)
static int FiWaitNow(const BYTE *task, const BYTE *w, unsigned id, int type)
{
    const int stage = ENM_STAGE;
    int i, k = -1, freeK = -1, want;
    if (g_fiMode != FI_MODE_PORTAL || type != FI_VICTIM_TYPE || *(const DWORD *)(task + 0x404) == 0x486030) return 0;
    if (!(stage >= 2 && stage <= 8) || *(volatile int *)0xFD5C20 != 0 || g_fiClock - g_fiRoomT0 < FI_GRACE_S) return 0;
    want = FiWallOutWait(type, ENM_AISTATE(task), *(const WORD *)(task + 0x412), 0);
    if (want && FiGhostPortal(w, 0)) want = 0;  // the game's own wall portal: it does it (FiFirst: "own portal")
    for (i = 0; i < FI_WAIT_N; ++i) {
        if (g_fiWait[i].task == task && g_fiWait[i].id == id) k = i;
        else if (!g_fiWait[i].task && freeK < 0) freeK = i;
    }
    if (k >= 0) {
        const float age = (float)(g_fiClock - g_fiWait[k].t0);
        if (want && age < FI_WAIT_MAX_S) return 1;
        g_fiWaitedNow = age > 0.0f ? age : 0.0f;
        memset(&g_fiWait[k], 0, sizeof g_fiWait[k]);
        if (want && LogQuota(&g_fiQuotaWait, 100))
            LogV("fade in: %08X (type %02X) still comes out of a wall by itself after %.1f s: it fades in where it is", id, type,
                 g_fiWaitedNow);
        return 0;
    }
    if (!want) return 0;
    if (freeK < 0) {  // (the oldest waits no more: it fades in at its next draw)
        for (i = 0, freeK = 0; i < FI_WAIT_N; ++i)
            if (g_fiWait[i].t0 < g_fiWait[freeK].t0) freeK = i;
    }
    g_fiWait[freeK].task = task, g_fiWait[freeK].id = id, g_fiWait[freeK].t0 = g_fiClock;
    return 1;
}
static int FiWaiting(const BYTE *task, unsigned id)  // v3.59 FI359: kept unseen now (FiBodyShown: no bar for it)
{
    int i;
    for (i = 0; i < FI_WAIT_N; ++i)
        if (g_fiWait[i].task == task && g_fiWait[i].id == id) return 1;
    return 0;
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
    g_fiWaitedNow = 0.0f;
    if (FiWaitNow(task, w, id, type)) return &g_fiWaitEnt;  // v3.59 FI359: still coming out of a wall by itself: unseen
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
        if (e == &g_fiWaitEnt) {
            done = 1;  // v3.59 FI359: kept unseen (the stock draw skipped) until it is out of its wall
        } else if (e && e->state == FI_E_FADE) {
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
        if (e == &g_fiWaitEnt) ok = 0;  // v3.59 FI359: unseen, no shadow
        else if (e && e->state == FI_E_FADE) ok = FiShadowShown(FiFrac((float)(g_fiClock - e->t0), g_fiSec));
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the shadow draw");
        ok = 1;
    }
    return ok;
}

// v3.59 FI359: ThermDrawFi (0x52AEB5) when Thermal hides a ghost (not drawn): its first sight is noted all the same (its
// fade's clock runs, its portal opens as a cold, dark void, its hold starts), so switching Thermal off later does not make
// every ghost of the room arrive again with a portal (v3.58). Draws nothing.
static void FiSeenOnly(const BYTE *task)
{
    int pd;
    float own;
    if (!g_fiReady || g_fiFail || g_fiMode == FI_MODE_OFF || !task) return;
    __try {
        FiSee(task, &pd, &own);
    } __except (FiFaultNote(GetExceptionInformation())) {
        FiFailNow("the hidden ghost's first sight");
    }
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
// v3.58 FI358: one verbose line when a tar portal closes: its pieces, its size against the body, the strands' time, when
static void FiCloseSay(const FiPortal *p, float age)
{
    FiClose c;
    char b[480];
    int j, drips = 0;
    if (!LogQuota(&g_fiQuotaClose, 400)) return;
    memset(&c, 0, sizeof c);
    c.id = p->id, c.type = p->type, c.at = age;
    c.grids = p->gridsMax, c.gridsOf = p->big.nOn, c.sets = p->setsMax;
    for (j = 0; j < p->setsMax && j < FI_SETS_MAX; ++j) drips += j ? p->L2[j - 1].nd : p->L.nd;
    c.drips = drips;
    c.w = 2.0f * p->big.tx, c.h = 2.0f * p->big.ty;
    c.body = p->measured ? FiBodyH(&p->body) : 0.0f;
    c.bones = !p->measured ? -1 : p->body.bones ? 1 : 0;
    c.strGame = p->strands == FI_TS_GAME ? p->nStr : 0;
    c.strOwn = p->nOwn, c.ownBox = p->ownBox, c.snapped = p->nOwn - p->nLoose, c.loose = p->nLoose;
    c.stickTo = p->sCut >= 0.0f && p->sCut < p->sEnd ? p->sCut : p->sEnd;
    c.stick = c.stickTo - g_fiSec > 0.0f ? c.stickTo - g_fiSec : 0.0f;
    c.ms = p->drawMs, c.msMax = p->drawMax, c.frames = p->drawN;
    FiCloseText(&c, b, sizeof b);
    LogV("fade in: %s", b);
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
            } else if ((FiTarDone(age, p->rel) && !(p->held && !p->released) && (p->strEnded || !p->held)) ||
                       age > p->rel + FI_TAR_CLOSE_S + 12.0f) {  // v3.51 PT351: a holding one waits for its tick (v3.58 FI358:
                                                                // and for its strands; never 12 s past its time)
                if (p->placeholder) {
                    if (LogQuota(&g_fiQuotaHold, 400))
                        LogV("fade in: %08X (type %02X): its tar portal closed after %.1f s: it never came out (still the "
                             "arrival's placeholder)", p->id, p->type, age);
                } else if (p->held) {
                    FiCloseSay(p, age);  // v3.58 FI358
                }
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
        if (!g_fiPortalN && g_fiSelfK < 1.0f) {  // v3.58 FI358: no portal: the own limit comes back (0.3 / s)
            g_fiMsEma = 0.0f;
            g_fiSelfK = FiSelfKeepStep(g_fiSelfK, 0.0f, g_fiFrameEma, g_fiDtLast);
        }
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
// v3.58 FI358: Refined's sticky strands start in the tar portal's draw (FiStickStart, after the game's pass 2: this frame's
// bones; v3.57 started them at the tick, where a ghost made this frame had no bones yet: the user's Jasper got "strands:
// none 0"). Here only how many: FadeInStrands, half of it beside the ghost's own 32 (v3.57's 12 alone with
// FadeInStrandSeconds 0)
static void FiStickWant(FiPortal *p)
{
    const int game = p->strands == FI_TS_GAME;
    if (!(g_fiStick > 0.0f)) p->ownWant = game ? 0 : (g_fiStrN < FI_STR_OWN ? g_fiStrN : FI_STR_OWN);
    else p->ownWant = game ? g_fiStrN / 2 : g_fiStrN;
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
    if (p->kind != FI_TD_VORTEX) FiStickWant(p);  // v3.58 FI358 (was: Refined's 12 here when the ghost has none)
}
// the ghost's own strands snap one by one (fadein_sim.h FiSnapAt): TrailItem +0x1A loose = 1, as its update does when a
// bone pulls too far; its update then fades the strand back into the wall. v3.58 FI358: over the last 1.3 s of the
// strands' time (FiStickWindow; v3.57's 1.5 .. 2.5 s with FadeInStrandSeconds 0)
static void FiStrandsSnap(FiPortal *p, float age)
{
    BYTE *it;
    float a, b;
    int size, i = 0;
    if (!FiStrandsOurs(p)) return;
    size = *(const int *)(p->pool + 4);  // (PoolList: head, item size = the link word's offset, count)
    if (size < 0x24 || size > 0x40) return;
    FiStickWindow(p->sEnd, &a, &b);
    for (it = *(BYTE *const *)p->pool; (ULONG_PTR)it >= 0x10000 && i < FI_STR_MAX; it = *(BYTE *const *)(it + size), ++i)
        if (!it[0x1A] && FiSnapAtW(i, p->nStr, p->L.seed, a, b) <= age) it[0x1A] = 1;
}
// v3.58 FI358: Refined's strands snap from `age` on, one by one (an early end: the hold let go early, the ghost is gone)
static void FiStickCut(FiPortal *p, float age)
{
    int i;
    if (p->sCut >= 0.0f && p->sCut <= age) return;
    p->sCut = age;
    for (i = 0; i < p->nOwn; ++i)
        if (p->sat[i] > age + 0.03f * (float)i) p->sat[i] = age + 0.03f * (float)i;
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
        FiStickCut(p, age);  // v3.58 FI358: Refined's strands snap now, one by one
        if (mayCall && FiStrandsOurs(p)) ((void(__cdecl *)(const BYTE *))0x482CF0)(p->task);
        p->strEnded = 1;     // v3.58 FI358 (gone / a new room: its trail goes with it)
    }
    // v3.58 FI358: held to the end: the strands hold on to sEnd (FiStrandTick lets the ghost's own go there)
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
// v3.59 FI359: at the hold's first tick (the body is still unseen: its fade has just begun), a victim lower than its own
// height over the floor under it (size x 180: where its idle float and its state 2 put it) by more than FI_LIFT_MIN is put
// there at once by the game's place setter 0x45CF20, its height spring (GhostWork +0x70 height, +0x74 speed: ghost.h)
// starting from there; its portal's point goes up with it. So it comes out where it floats, not rising out of the floor.
static void FiLiftNow(FiPortal *p)
{
    const BYTE *w = p->work, *g;
    float fy, ry, d;
    if (p->type != FI_VICTIM_TYPE || p->placeholder || !g_fiHoldFnOk || p->kind == FI_TD_VORTEX) return;
    g = *(const BYTE *const *)(w + 0x28);
    if ((ULONG_PTR)g < 0x10000) return;
    if (!FiFloorUnder(((const float *)w)[0], ((const float *)w)[1] - FI_FLOOR_FROM, ((const float *)w)[2], &fy)) return;
    ry = FiRestY(fy, *(const float *)(w + 0x60));
    d = FiLiftBy(((const float *)w)[1], ry);
    if (!(d > 0.0f)) return;
    {
        static __declspec(align(16)) float s_v[4];
        float vd;
        s_v[0] = ((const float *)w)[0], s_v[1] = ry, s_v[2] = ((const float *)w)[2], s_v[3] = ((const float *)w)[3];
        ((void(__cdecl *)(const BYTE *, float *))0x45CF20)(p->task, s_v);
        *(volatile float *)(g + 0x70) = ry;    // GhostWork.height (its update takes the work position's y anyway)
        *(volatile float *)(g + 0x74) = 0.0f;  // GhostWork.height_speed: no swing on from the rise it was in
        p->havePrev = 0;
        p->lift = d;
        if (FiPlaneV(p, p->c[1] - d, &vd)) FiPlaneMove(p, vd);  // its portal's point up with it (along its plane)
        p->floorY = fy, p->floorOk = 1;
    }
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
        FiLiftNow(p);  // v3.59 FI359: a victim below its own height comes out at it (still unseen: its fade's first frame)
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
// v3.58 FI358: after the hold, the ghost's own strands go on to sEnd (snapping one by one in the last 1.3 s), then 0x482CF0
// lets them go, as the hold's end did in v3.57; a cut-scene or another Fade In value ends them at once. Refined's own
// strands are the draw's (nothing in the game to let go).
static void FiStrandTick(FiPortal *p, int ph)
{
    const float age = (float)(g_fiClock - p->tr);
    if (p->strands != FI_TS_GAME || !FiStrandsOurs(p)) {  // (none, Refined's, the ghost gone or its trail the game's again)
        p->strEnded = 1;
        return;
    }
    if (ph == 1 || g_fiMode != FI_MODE_PORTAL || age >= p->sEnd) {
        if (age < p->sEnd) FiStickCut(p, age);
        ((void(__cdecl *)(const BYTE *))0x482CF0)(p->task);
        p->strEnded = 1;
        return;
    }
    FiStrandsSnap(p, age);
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
            if (!p->on || p->kind == FI_TD_VORTEX || p->tr < 0.0) continue;
            if (!p->released) {
                if (ph == 1) FiRelease(p, "a cut-scene began", 1);
                else FiHoldTick(p);
            }
            if (p->released && p->held && !p->strEnded) FiStrandTick(p, ph);  // v3.58 FI358: the strands hold on after the hold
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
// v3.58 FI358: one piece's grid (the first at v3.57's size, the ones around it smaller: g->s) and one blot set (set 0 around
// the first piece as v3.57, sets 1 / 2 over the whole portal, a blot outside every piece left out); the two draws read
// their block at once (0x4868B0 / 0x486E80 begin and flush their own 2D batch), so one block each is filled per call
static void FiGameGrid(const FiPortal *p, const FiPiece *g, float amt)
{
    BYTE *A = g_fiWg;
    const float now = (float)g_fiClock, grow = 0.55f + 0.45f * amt, hx = p->sx * g->s, hy = p->sy * g->s;
    int i, j;
    memset(A, 0, sizeof g_fiWg);
    for (j = 0; j < 3; ++j) {
        FI_WF(A, 4 * j) = p->c[j] + p->ax[j] * g->u + p->ay[j] * g->v;  // pos (on the plane), dir
        FI_WF(A, 0x10 + 4 * j) = p->n[j];
    }
    FI_WF(A, 0x0C) = 1.0f;
    FI_WF(A, 0x40) = hx, FI_WF(A, 0x44) = hy;                               // param.width / width2 (not read by the draw)
    for (i = 0; i < 9; ++i) {  // strip / strip2: the grid's columns and rows, each at least its left neighbour, a slow wobble
        float u = ((float)i - 4.0f) * hx * grow * 0.25f, v = ((float)i - 4.0f) * hy * grow * 0.25f;
        if (i != 0 && i != 8) {
            u += hx * 0.03f * sinf(1.3f * now + 1.7f * (float)i + g->phase);
            v += hy * 0.03f * sinf(1.1f * now + 2.3f * (float)i + g->phase);
        }
        if (i && u < FI_WF(A, 0x5C + 4 * (i - 1))) u = FI_WF(A, 0x5C + 4 * (i - 1));
        if (i && v < FI_WF(A, 0xA4 + 4 * (i - 1))) v = FI_WF(A, 0xA4 + 4 * (i - 1));
        FI_WF(A, 0x5C + 4 * i) = u;
        FI_WF(A, 0xA4 + 4 * i) = v;
    }
    FI_WF(A, 0xEC) = amt;  // amount; spring[4]: the four texture layers' strength (128 = whole), as its springs follow it
    for (i = 0; i < 4; ++i)
        FI_WF(A, 0xF0 + 4 * i) = (1.0f - (1.0f - amt) * (1.0f - amt)) * 128.0f *
                                 (0.88f + 0.12f * sinf(0.9f * now + 1.9f * (float)i + g->phase));
    *(int *)(A + 0x118) = 1;  // state: open (unk_110 / unk_114 0: one face, flat)
    *(int *)(A + 0x124) = 1;  // mode (3 would draw nothing)
    *(BYTE **)(g_fiTkA + 0x408) = A;  // task->work_ptr, all the grid draw reads of its task
}
static int FiGameSet(const FiPortal *p, int set, float age, float amt, float w)  // returns the blots placed
{
    BYTE *B = g_fiWb;
    const FiTarLayout *L = set ? &p->L2[set - 1] : &p->L;
    const float delay = 0.15f * (float)set, ag = age - delay, grow = 0.55f + 0.45f * amt, k = p->wall ? 1.0f : 0.55f;
    const float hx = set ? p->big.tx : p->sx, hy = set ? p->big.ty : p->sy;
    const float cu = set ? p->big.du : p->big.g[0].u, cv = set ? p->big.dv : p->big.g[0].v, offK = set ? 0.5f : 1.0f;
    const int shown = FiTarShown(ag);
    int i, j, placed = 0;
    memset(B, 0, sizeof g_fiWb);
    for (j = 0; j < 3; ++j) FI_WF(B, 4 * j) = p->c[j], FI_WF(B, 0x1F0 + 4 * j) = p->n[j];
    FI_WF(B, 0x0C) = 1.0f;
    for (i = 0; i < FI_TAR_SPLATS; ++i) *(signed char *)(B + 0x3CC + i) = -1;  // no drip
    for (i = 0; i < 16; ++i) *(int *)(B + 0x314 + 4 * i) = -1;
    for (i = 0; i < shown; ++i) {
        const FiSplat *s = &L->s[i];
        const float u = cu + s->u * hx * grow, v = cv + s->v * hy * grow;
        float *pt = (float *)(B + 0x10 + 16 * i);                           // the 25 points (w = the turn)
        const float yb = p->c[1] + p->ax[1] * u + p->ay[1] * v;  // v3.59 FI359: its world height
        const int in = (!set || FiBigCovers(&p->big, u, v, p->sx, p->sy)) && FiBlotClear(yb, s->sc * k, p->floorY, p->floorOk);
        for (j = 0; j < 3; ++j) pt[j] = p->c[j] + p->ax[j] * u + p->ay[j] * v;
        pt[3] = s->ang;
        FI_WF(B, 0x230 + 4 * i) = s->sc * k;                                // size (each blot keeps it: never stretched)
        FI_WS(B, 0x354 + 2 * i) = FiTarLifeShort(in ? FiLifeAtWeight(FiTarLife(age, p->rel, s->off * offK), w) : -1000.0f);
        placed += in;
        if (in && s->drip > 0 && s->drip <= FI_TAR_DRIPS) {
            const FiDrip *d = &L->d[(int)s->drip];
            *(signed char *)(B + 0x3CC + i) = s->drip;                       // its drip slot
            *(int *)(B + 0x314 + 4 * s->drip) = d->kind;                     // piece 2 / 3
            FI_WF(B, 0x294 + 4 * s->drip) = FiDripToFloor(FiDripLen(ag - FiTarAppear(i), d->len * k), yb, p->floorY,
                                                          p->floorOk);  // length run down (v3.59 FI359: never into the floor)
            FI_WS(B, 0x386 + 2 * s->drip) = FiTarLifeShort(FiLifeAtWeight(FiTarLife(age, p->rel, d->off * offK), w));
        }
    }
    FI_WF(B, 0x3B4) = FI_TAR_FADE_S;  // the fade time (unk_3B0 0: on the plane; unk_3C0 0: no floor blots; unk_3C2 0: flat)
    FI_WS(B, 0x3BC) = (short)shown;
    FI_WS(B, 0x3BE) = (short)L->nd;
    FI_WS(B, 0x3C6) = 1;
    *(BYTE **)(g_fiTkB + 0x408) = B;  // task->work_ptr, all the blot draw reads of its task
    return placed;
}
static int FiGameFaultNote(EXCEPTION_POINTERS *e)
{
    g_fiGameFaultCode = e->ExceptionRecord->ExceptionCode;
    g_fiGameFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
// v3.58 FI358: the effect budget's share of the pieces around the first (fadein_sim.h FiPieceKeep: budget.inc's spark
// levels 60 / 35 %, 35 % from the level that thins After Shock / Death Presence)
// and the portals' own limit (FiSelfKeepStep: their draw's share of the frame)
static float FiSparkNow(void)
{
    const float b = BgSparkNow();
    return b < g_fiSelfK ? b : g_fiSelfK;
}
static float FiKeepNow(int n) { return FiPieceKeep(FiSparkNow(), BgLevel() >= BGL_AI3DP, n); }
static int FiGameDraw(void *dev, FiPortal *p)
{
    static const DWORD kRs[3] = { 14 /*ZWRITEENABLE*/, 22 /*CULLMODE*/, 28 /*FOGENABLE*/ };  // what its 2D flush sets raw
    DWORD rs[3];
    const float age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel);
    const float keep = FiKeepNow(p->big.nOn - 1), spark = FiSparkNow();
    const int deep = BgLevel() >= BGL_AI3DP;
    int k, got = 0, ok = 0, grids = 0, sets = 0;
    if (!(amt > 0.002f)) return 0;
    for (k = 0; k < 3; ++k)
        if (SUCCEEDED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[k], &rs[k]))) got |= 1 << k;
    __try {
        int g, j, m = 0;
        for (g = 1; g < p->big.n; ++g) {  // v3.58 FI358: the pieces around it first (the first portal is drawn over them)
            const FiPiece *pc = &p->big.g[g];
            float a;
            if (!pc->on) continue;
            a = FiPieceAmount(age, p->rel, pc, 0) * FiPieceWeight(keep, ++m);
            if (!(a > 0.002f)) continue;
            FiGameGrid(p, pc, a);
            ((void(__cdecl *)(BYTE *))0x4868B0)(g_fiTkA);  // EffectWallGhostDraw: the 9 x 9 grid in its four texture layers
            ++grids;
        }
        FiGameGrid(p, &p->big.g[0], amt);
        ((void(__cdecl *)(BYTE *))0x4868B0)(g_fiTkA);      // the first portal (v3.57's)
        ++grids;
        for (j = 0; j < p->big.sets && j < FI_SETS_MAX; ++j) {  // the blots and the drips (EffectDrawDecal)
            const float w = FiSetWeight(spark, deep, j);
            if (!(w > 0.0f)) continue;
            if (FiGameSet(p, j, age, j ? FiPieceAmount(age, p->rel, &p->big.g[p->big.n > 1 ? 1 : 0], 0) : amt, w) > 0 || !j) {
                ((void(__cdecl *)(BYTE *))0x486E80)(g_fiTkB);
                ++sets;
            }
        }
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
    if (ok && grids > p->gridsMax) p->gridsMax = grids;  // v3.58 FI358 (the close line)
    if (ok && sets > p->setsMax) p->setsMax = sets;
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
// the blot (a ragged near-black disc, a deep red core) and its drips (black, running down, a bead at the tip). v3.58
// FI358: one disc a piece (the first at v3.57's size, the ones around it smaller, each with its own ragged edge), the drips
// of every blot set (set 0 around the first, sets 1 / 2 over the whole portal, only where a piece is)
// (v3.59 FI359: the plane's axes ax / ay instead of the portal: the game's own wall portals use it under Spirit Vision; the
// colours are g_fiPal's)
static void FiOwnDisc(void *dev, const Viewport8 *vp, const float o[3], const float ax[3], const float ay[3], float hx, float hy,
                      float amt, unsigned seed)
{
    enum { NR = 7, NS = 24 };
    const float *kCore = g_fiPal->core, *kEdge = g_fiPal->edge;
    const float now = (float)g_fiClock;
    GvHv g[2][NS + 1];
    float edge[NS + 1];
    int r, s, j, cur = 0;
    for (s = 0; s <= NS; ++s) {  // a ragged outline, crawling slowly
        const float th = 6.2831853f * (float)(s % NS) / NS;
        edge[s] = 0.70f + 0.30f * FxValueNoise(2.2f * cosf(th) + 5.3f, 2.2f * sinf(th) + 0.12f * now, seed);
    }
    for (r = 0; r <= NR; ++r) {
        const float rn = (float)r / NR, kk = amt * (rn < 0.8f ? 0.96f : 0.96f * (1.0f - (rn - 0.8f) / 0.2f));
        float rgb[3];
        SvMix(rgb, kCore, kEdge, powf(rn, 0.6f));
        for (s = 0; s <= NS; ++s) {
            const float th = 6.2831853f * (float)s / NS, e = rn * edge[s];
            float q[3];
            for (j = 0; j < 3; ++j) q[j] = o[j] + ax[j] * cosf(th) * e * hx + ay[j] * sinf(th) * e * hy;
            SvPoint(&g[cur][s], q, kk, rgb);
        }
        if (r)
            for (s = 0; s < NS; ++s) {
                GvTri(dev, &g[!cur][s], &g[!cur][s + 1], &g[cur][s + 1], vp, NULL);
                GvTri(dev, &g[!cur][s], &g[cur][s + 1], &g[cur][s], vp, NULL);
            }
        cur = !cur;
    }
}
static void FiOwnDrips(void *dev, const Viewport8 *vp, const FiPortal *p, int set, float age, float grow, float w)
{
    const float *kDrip = g_fiPal->drip;  // v3.59 FI359
    const FiTarLayout *L = set ? &p->L2[set - 1] : &p->L;
    const float k = p->wall ? 1.0f : 0.6f, ag = age - 0.15f * (float)set, offK = set ? 0.5f : 1.0f;
    const float hx = (set ? p->big.tx : p->sx) * grow * 0.85f, hy = (set ? p->big.ty : p->sy) * grow * 0.85f;
    const float cu = set ? p->big.du : p->big.g[0].u, cv = set ? p->big.dv : p->big.g[0].v;
    int i, j, m;
    for (i = 0; i < FiTarShown(ag); ++i) {
        const FiSplat *sp = &L->s[i];
        const FiDrip *d;
        const float u = cu + sp->u * hx, v = cv + sp->v * hy;
        float top[3], P[8][3], hw[8], len, a, w0;
        if (sp->drip <= 0 || sp->drip > FI_TAR_DRIPS) continue;
        if (set && !FiBigCovers(&p->big, u, v, p->sx, p->sy)) continue;
        d = &L->d[(int)sp->drip];
        len = FiDripLen(ag - FiTarAppear(i), d->len * k * 0.8f);
        len = FiDripToFloor(len, p->c[1] + p->ax[1] * u + p->ay[1] * v, p->floorY, p->floorOk);  // v3.59 FI359
        a = FiTarLifeAlpha(FiTarLife(age, p->rel, d->off * offK)) * FiTarOpenAt(ag) * w;
        if (len < 2.0f || a < 0.01f) continue;
        for (j = 0; j < 3; ++j) top[j] = p->c[j] + p->n[j] * 4.0f + p->ax[j] * u + p->ay[j] * v;
        w0 = 6.0f * d->w * k;
        for (m = 0; m < 8; ++m) {
            const float t = (float)m / 7.0f, wig = 1.5f * sinf(3.0f * t + sp->ang);
            for (j = 0; j < 3; ++j) P[m][j] = top[j] + p->ax[j] * wig;
            P[m][1] += len * t;  // straight down (y points down)
            hw[m] = w0 * (1.0f - 0.65f * t) + w0 * 0.55f * expf(-(1.0f - t) * (1.0f - t) / 0.012f);  // a bead at the tip
        }
        FiRibbon(dev, vp, (const float(*)[3])P, hw, 8, 0.95f * a, kDrip);
    }
}
static int FiOwnBlot(void *dev, const Viewport8 *vp, FiPortal *p)
{
    const float age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel);
    const float grow = 0.35f + 0.65f * FiTarOpenAt(age), keep = FiKeepNow(p->big.nOn - 1), spark = FiSparkNow();
    const int deep = BgLevel() >= BGL_AI3DP;
    float o[3];
    int g, j, m = 0, grids = 0, sets = 0;
    if (!(amt > 0.002f)) return 0;
    for (g = 1; g < p->big.n; ++g) {  // v3.58 FI358: the pieces around the first, then the first over them
        const FiPiece *pc = &p->big.g[g];
        float a, gr;
        if (!pc->on) continue;
        a = FiPieceAmount(age, p->rel, pc, 0) * FiPieceWeight(keep, ++m);
        if (!(a > 0.002f)) continue;
        gr = 0.35f + 0.65f * FiTarOpenAt(age - pc->open);
        for (j = 0; j < 3; ++j) o[j] = p->c[j] + p->n[j] * 2.0f + p->ax[j] * pc->u + p->ay[j] * pc->v;
        FiOwnDisc(dev, vp, o, p->ax, p->ay, p->sx * pc->s * gr, p->sy * pc->s * gr, a * g_fiPal->kAround,
                  FiHash(p->L.seed + 31u * (unsigned)g));  // v3.59 FI359: axes, the palette's strength
        ++grids;
    }
    for (j = 0; j < 3; ++j) o[j] = p->c[j] + p->n[j] * 3.0f + p->ax[j] * p->big.g[0].u + p->ay[j] * p->big.g[0].v;
    FiOwnDisc(dev, vp, o, p->ax, p->ay, p->sx * grow, p->sy * grow, amt * g_fiPal->kFirst, p->L.seed);  // a little off the wall
    ++grids;
    for (j = 0; j < p->big.sets && j < FI_SETS_MAX; ++j) {
        const float w = FiSetWeight(spark, deep, j);
        if (!(w > 0.0f)) continue;
        FiOwnDrips(dev, vp, p, j, age, grow, w);
        ++sets;
    }
    if (grids > p->gridsMax) p->gridsMax = grids;
    if (sets > p->setsMax) p->setsMax = sets;
    p->drawnNow = 1;
    return 1;
}
static void FiOwnCore(void *dev, const Viewport8 *vp, const FiPortal *p)  // its faint deep red core (additive)
{
    enum { NR = 3, NS = 16 };
    const float *kRed = g_fiPal->glow;  // v3.59 FI359
    const float age = (float)(g_fiClock - p->t0), amt = FiTarAmount(age, p->rel), grow = 0.35f + 0.65f * FiTarOpenAt(age);
    const float pulse = 0.85f + 0.15f * sinf(2.1f * (float)g_fiClock);
    GvHv g[2][NS + 1];
    float o[3];
    int r, s, j, cur = 0;
    if (!(amt > 0.002f)) return;
    for (j = 0; j < 3; ++j) o[j] = p->c[j] + p->n[j] * 4.0f + p->ax[j] * p->big.g[0].u + p->ay[j] * p->big.g[0].v;  // v3.58 FI358
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
// v3.58 FI358: the arriving body's box (this frame's bones after the game's pass 2, else its type's after 0.35 s) and the
// pieces laid out for it (fadein_sim.h FiBigTarget / FiBigMake); on a wall a piece that is not on the wall is left out
static int FiBoneCount(const BYTE *work)
{
    const BYTE *model = *(const BYTE *const *)(work + 0x30), *skel;
    int nb = 25;
    if ((ULONG_PTR)model < 0x10000) return 0;
    skel = *(const BYTE *const *)(model + 0xE0);
    if ((ULONG_PTR)skel >= 0x10000) nb = *(const int *)(skel + 0x14);
    return nb < 0 ? 0 : nb > 32 ? 32 : nb;
}
// v3.59 FI359: one verbose line a portal, once it is laid out: its foot over the floor, how the victim came out
static void FiArriveSay(FiPortal *p)
{
    FiArrive a;
    char b[400];
    if (p->said59 || !p->held || p->placeholder) return;
    p->said59 = 1;
    if (!LogQuota(&g_fiQuotaArrive, 400)) return;
    memset(&a, 0, sizeof a);
    a.id = p->id, a.type = p->type;
    a.floorOk = p->floorOk, a.ceilOk = p->ceilOk;
    a.foot = p->floorOk ? p->floorY - (p->c[1] + p->ay[1] * p->big.ty) : 0.0f;
    a.raised = p->raised > 0.0f ? p->raised : 0.0f;
    a.k = p->fitK > 0.0f ? p->fitK : 1.0f;
    a.above = p->above0, a.rest = p->rest0, a.lift = p->lift, a.waited = p->waited;
    a.ai = p->ai0, a.step = p->step0, a.anim = p->anim0;
    FiArriveText(&a, b, sizeof b);
    LogV("fade in: %s", b);
}
static void FiMeasure(FiPortal *p)
{
    const float age = (float)(g_fiClock - p->t0);
    const int alive = FiAlive(p);
    float pts[32][3], w0[3], d[3], tx, ty, du, dv;
    int b, n = 0, nb, j, k, on;
    if (p->measured) return;
    if (!p->held && age < FI_MEASURE_WAIT_S) return;  // v3.59 FI359: after the hold's first tick (a victim may be lifted there)
    for (j = 0; j < 3; ++j) w0[j] = p->c[j] + p->n[j] * p->back;  // (the body's centre the plane was found from)
    if (alive) {
        nb = FiBoneCount(p->work);
        for (b = 0; b < nb; ++b)
            if (FiBone(p->work, b, pts[n])) ++n;
        memcpy(w0, p->work, sizeof w0);
    }
    if (!(n >= 3 && FiBodyBones((const float(*)[3])pts, n, w0, &p->body))) {
        if (alive && age < FI_MEASURE_WAIT_S) return;  // (a ghost made this frame: its bones come with its first draws)
        FiBodyType(p->type, w0, &p->body);
    }
    FiBigTarget(&p->body, p->sx, p->sy, &tx, &ty);
    for (j = 0; j < 3; ++j) d[j] = p->body.c[j] - p->c[j];
    du = d[0] * p->ax[0] + d[1] * p->ax[1] + d[2] * p->ax[2];
    dv = d[0] * p->ay[0] + d[1] * p->ay[1] + d[2] * p->ay[2];
    {   // v3.59 FI359: the whole portal between the floor under the body and a ceiling over it (moved up its plane; a low
        // ceiling: as tall as the room allows, the first portal smaller, never squashed); then centred on it (dv 0)
        float fy, cy, vF = 1e30f, vC = -1e30f, vc, ty2, fk;
        if (FiFloorUnder(p->body.c[0], p->body.top - 10.0f, p->body.c[2], &fy)) p->floorY = fy, p->floorOk = 1;
        if (p->floorOk && !FiPlaneV(p, p->floorY - FI_FLOOR_GAP, &vF)) vF = 1e30f;
        if (FiCeilOver(p->body.c, &cy) && cy < p->body.c[1]) {
            p->ceilOk = 1, p->ceilY = cy;
            if (!FiPlaneV(p, cy + FI_CEIL_GAP, &vC)) vC = -1e30f;
        }
        if (p->floorOk || p->ceilOk) {
            FiFloorFit(vF, vC, dv, ty, p->sy, &vc, &ty2, &fk);
            FiPlaneMove(p, vc);
            p->raised = dv - vc;
            dv = 0.0f, ty = ty2;
            if (fk < 1.0f) {
                p->sx *= fk, p->sy *= fk, p->fitK = fk;
                if (tx > FI_COVER_MAXK * p->sx) tx = FI_COVER_MAXK * p->sx;
            }
        }
    }
    FiBigMake(&p->big, p->L.seed, p->sx, p->sy, tx, ty, du, dv, g_fiPieces);
    if (p->wall)
        for (k = 1; k < p->big.n; ++k) {  // each piece around the first on the wall too (a door, a corner: left out)
            FiPiece *g = &p->big.g[k];
            float q[3], q0[3], q1[3], hit[3], nrm[3], off;
            for (j = 0; j < 3; ++j) {
                q[j] = p->c[j] + p->ax[j] * g->u + p->ay[j] * g->v;
                q0[j] = q[j] + p->n[j] * 50.0f, q1[j] = q[j] - p->n[j] * 50.0f;
            }
            off = 999.0f;
            if (SvvCastT(q0, q1, hit, nrm, 0.0f) && nrm[0] * p->n[0] + nrm[1] * p->n[1] + nrm[2] * p->n[2] > 0.8f)
                off = (hit[0] - q[0]) * p->n[0] + (hit[1] - q[1]) * p->n[1] + (hit[2] - q[2]) * p->n[2];
            if (!(fabsf(off) < 30.0f)) g->on = 0;
        }
    for (k = 1, on = 0; k < p->big.n; ++k) on += p->big.g[k].on;
    p->big.nOn = 1 + on;
    p->big.sets = on ? (on >= 5 ? 3 : 2) : 1;
    p->measured = 1;
    FiArriveSay(p);  // v3.59 FI359
}
// Refined's sticky strands: up to ownWant of the body's bones in front of the plane (else points of its box), each tied
// to a point of the portal behind it; each snaps at its own moment in the last 1.3 s of the strands' time (FiStickWindow)
static void FiStickStart(FiPortal *p)
{
    const float ageR = (float)(g_fiClock - p->tr);
    const unsigned s0 = FiHash(p->L.seed ^ 0x57A7u);
    const int alive = FiAlive(p);
    int want = p->ownWant, n = 0, nb, tries, i, j, k;
    float a, b, w0[3];
    if (p->ownStarted || !p->held || !p->measured) return;
    if (want > FI_STRANDS_MAX) want = FI_STRANDS_MAX;
    if (want <= 0 || p->sCut >= 0.0f || ageR >= p->sEnd) {  // none wanted, or too late (let go early, its time over)
        p->ownStarted = 1;
        return;
    }
    for (j = 0; j < 3; ++j) w0[j] = p->body.c[j];
    if (alive) {
        nb = FiBoneCount(p->work);
        for (tries = 0; tries < 3 * nb && n < want; ++tries) {
            const int bi = (int)((s0 + (unsigned)tries * 7u) % (unsigned)nb);
            float q[3], dd;
            int dup = 0;
            for (k = 0; k < n; ++k) dup |= p->sbone[k] == bi;
            if (dup || !FiBone(p->work, bi, q)) continue;
            dd = (q[0] - p->c[0]) * p->n[0] + (q[1] - p->c[1]) * p->n[1] + (q[2] - p->c[2]) * p->n[2];
            if (dd < 5.0f) continue;  // behind the plane (in the wall): no strand
            memcpy(p->sb[n], q, sizeof q);
            p->sbone[n++] = bi;
        }
        memcpy(w0, p->work, sizeof w0);
    }
    if (n < (want < 3 ? want : 3)) {  // too few bones: points of its box, following its work position
        const float h = FiBodyH(&p->body);
        if (alive && ageR < FI_MEASURE_WAIT_S) return;  // (next frame)
        for (n = 0; n < want; ++n) {
            const unsigned hh = FiHash(s0 + (unsigned)n * 131u);
            const float u = (FiRnd01(hh) - 0.5f) * 1.6f * p->body.half;
            const float y = p->body.top + (0.12f + 0.76f * FiRnd01(hh ^ 1u)) * h - w0[1];
            for (j = 0; j < 3; ++j) p->sbo[n][j] = p->ax[j] * u - p->n[j] * 0.4f * p->body.half;
            p->sbo[n][1] += y;
            for (j = 0; j < 3; ++j) p->sb[n][j] = w0[j] + p->sbo[n][j];
            p->sbone[n] = -1;
        }
        p->ownBox = 1;
    }
    FiStickWindow(p->sEnd, &a, &b);
    for (i = 0; i < n; ++i) {  // its anchor: behind it on the plane, spread over the whole portal
        const unsigned hh = FiHash(s0 + (unsigned)i * 977u);
        const float dd = (p->sb[i][0] - p->c[0]) * p->n[0] + (p->sb[i][1] - p->c[1]) * p->n[1] + (p->sb[i][2] - p->c[2]) * p->n[2];
        float pu, pv, ru, rv, e, l2 = 0.0f;
        for (j = 0; j < 3; ++j) p->sa[i][j] = p->sb[i][j] - p->n[j] * dd;
        pu = (p->sa[i][0] - p->c[0]) * p->ax[0] + (p->sa[i][1] - p->c[1]) * p->ax[1] + (p->sa[i][2] - p->c[2]) * p->ax[2];
        pv = (p->sa[i][0] - p->c[0]) * p->ay[0] + (p->sa[i][1] - p->c[1]) * p->ay[1] + (p->sa[i][2] - p->c[2]) * p->ay[2];
        pu += (FiRnd01(hh) - 0.5f) * 0.7f * p->big.tx;
        pv += (FiRnd01(hh ^ 1u) - 0.5f) * 0.7f * p->big.ty;
        ru = (pu - p->big.du) / (0.9f * p->big.tx), rv = (pv - p->big.dv) / (0.9f * p->big.ty);  // within the portal
        e = ru * ru + rv * rv;
        if (e > 1.0f) {
            e = 1.0f / sqrtf(e);
            pu = p->big.du + (pu - p->big.du) * e, pv = p->big.dv + (pv - p->big.dv) * e;
        }
        for (j = 0; j < 3; ++j) {
            p->sa[i][j] = p->c[j] + p->n[j] * 2.0f + p->ax[j] * pu + p->ay[j] * pv;
            l2 += (p->sb[i][j] - p->sa[i][j]) * (p->sb[i][j] - p->sa[i][j]);
        }
        p->len0[i] = sqrtf(l2);
        p->sat[i] = FiSnapAtW(i, n, s0, a, b);
        if (p->sat[i] < ageR) p->sat[i] = ageR + 0.03f * (float)i;
        p->sdropSet[i] = 0;
    }
    p->nOwn = n;
    p->ownStarted = 1;
    if (p->strands == FI_TS_NONE && n) p->strands = FI_TS_OWN, p->nStr = n;
}
// a small camera-facing disc (strength k at the centre, softer at the rim)
static void FiBlob(void *dev, const Viewport8 *vp, const float c[3], float r, float k, const float rgb[3])
{
    enum { NS = 8 };
    float e[3], u[3], v[3], l;
    GvHv m, g[NS + 1];
    int s, j;
    for (j = 0; j < 3; ++j) e[j] = g_svEye[j] - c[j];
    l = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    if (!(l > 1.0f) || !(r > 0.2f)) return;
    for (j = 0; j < 3; ++j) e[j] /= l;
    u[0] = -e[2], u[1] = 0.0f, u[2] = e[0];  // e x (0, 1, 0)
    l = sqrtf(u[0] * u[0] + u[2] * u[2]);
    if (!(l > 1e-4f)) u[0] = 1.0f, u[2] = 0.0f, l = 1.0f;
    u[0] /= l, u[2] /= l;
    v[0] = e[1] * u[2] - e[2] * u[1], v[1] = e[2] * u[0] - e[0] * u[2], v[2] = e[0] * u[1] - e[1] * u[0];
    SvPoint(&m, c, k, rgb);
    for (s = 0; s <= NS; ++s) {
        const float th = 6.2831853f * (float)(s % NS) / NS, rr = r * (0.85f + 0.15f * sinf(3.0f * th));
        float q[3];
        for (j = 0; j < 3; ++j) q[j] = c[j] + (u[j] * cosf(th) + v[j] * sinf(th)) * rr;
        SvPoint(&g[s], q, 0.35f * k, rgb);
    }
    for (s = 0; s < NS; ++s) GvTri(dev, &m, &g[s], &g[s + 1], vp, NULL);
}
// Refined's sticky strands: near black, sagging more as they age, thinning as they stretch; a blob of tar on the body where
// each holds on, running down a little; at its moment it strains, snaps, its wall end whips back and its body end drops off
// (v3.58 FI358; v3.57's 12 thin strands that snapped between 1.5 and 2.5 s: FadeInStrandSeconds 0)
static int FiOwnStrands(void *dev, const Viewport8 *vp, FiPortal *p)
{
    const float *kStr = g_fiPal->str, *kBlob = g_fiPal->blob, ks = g_fiPal->kStr;  // v3.59 FI359
    const float ageR = p->tr >= 0.0 ? (float)(g_fiClock - p->tr) : -1.0f;
    const int alive = FiAlive(p);
    int i, j, m, drawn = 0;
    if (ageR < 0.0f || !p->held || p->nOwn <= 0) return 0;
    if (!alive && p->sCut < 0.0f) FiStickCut(p, ageR);  // the ghost is gone: its strands snap now
    for (i = 0; i < p->nOwn && i < FI_STRANDS_MAX; ++i) {
        const unsigned hh = FiHash(p->L.seed + (unsigned)i * 613u);
        float k, b[3], P[9][3], hw[9], len, sag, wk, t;
        int ph;
        if (alive) {
            if (p->sbone[i] >= 0) {
                if (FiBone(p->work, p->sbone[i], b)) memcpy(p->sb[i], b, sizeof b);
            } else {
                for (j = 0; j < 3; ++j) p->sb[i][j] = ((const float *)p->work)[j] + p->sbo[i][j];
            }
        }
        memcpy(b, p->sb[i], sizeof b);
        len = sqrtf((b[0] - p->sa[i][0]) * (b[0] - p->sa[i][0]) + (b[1] - p->sa[i][1]) * (b[1] - p->sa[i][1]) +
                    (b[2] - p->sa[i][2]) * (b[2] - p->sa[i][2]));
        if (len > FI_STR_LOOSE && p->sat[i] > ageR) {  // stretched past what tar holds: it snaps now
            p->sat[i] = ageR;
            ++p->nLoose;
        }
        ph = FiStrandPhase(ageR, p->sat[i], &k);
        if (ph >= 2 && !p->sdropSet[i]) {
            memcpy(p->sdrop[i], b, sizeof b);
            p->sdropSet[i] = 1;
        }
        wk = FiStickWidth(ageR, p->sEnd, len, p->len0[i]);
        if (ph <= 1) {  // whole (or straining: taut and thinning)
            float e[3], el, bl[3];
            sag = FiStickSag(ageR, p->sEnd, len) * (ph ? 0.4f : 1.0f);
            for (m = 0; m < 9; ++m) {
                t = (float)m / 8.0f;
                for (j = 0; j < 3; ++j) P[m][j] = p->sa[i][j] + (b[j] - p->sa[i][j]) * t;
                P[m][1] += sag * 4.0f * t * (1.0f - t);
                hw[m] = (2.2f - 1.0f * t + 0.6f * sinf(9.0f * t + (float)i)) * wk * k;  // (beaded: tar, not thread)
            }
            FiRibbon(dev, vp, (const float(*)[3])P, hw, 9, 0.92f * ks, kStr);
            for (j = 0; j < 3; ++j) e[j] = g_svEye[j] - b[j];  // the tar on the body: a little toward the camera
            el = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
            for (j = 0; j < 3; ++j) bl[j] = b[j] + (el > 1.0f ? e[j] / el * 6.0f : 0.0f);
            FiBlob(dev, vp, bl, (4.5f + 3.0f * FiRnd01(hh)) * (0.5f + 0.5f * wk) * k, 0.9f * ks, kBlob);
            {   // ... running down from it
                const float run = (8.0f + 14.0f * FiRnd01(hh ^ 2u)) * (ageR < 1.5f ? ageR / 1.5f : 1.0f);
                if (run > 1.5f) {
                    for (m = 0; m < 5; ++m) {
                        t = (float)m / 4.0f;
                        for (j = 0; j < 3; ++j) P[m][j] = bl[j];
                        P[m][1] += run * t;
                        hw[m] = (1.6f - 0.9f * t + 0.8f * expf(-(1.0f - t) * (1.0f - t) / 0.02f)) * (0.5f + 0.5f * wk);
                    }
                    FiRibbon(dev, vp, (const float(*)[3])P, hw, 5, 0.85f * k * ks, kStr);
                }
            }
        } else if (ph == 2) {  // snapped: the wall end whips back toward the portal, thinning out
            float mid[3];
            for (j = 0; j < 3; ++j) mid[j] = 0.5f * (p->sa[i][j] + p->sdrop[i][j]);
            mid[1] += FiStickSag(ageR, p->sEnd, len);
            for (m = 0; m < 5; ++m) {
                t = (float)m / 4.0f * (1.0f - k);
                for (j = 0; j < 3; ++j) P[m][j] = p->sa[i][j] + (mid[j] - p->sa[i][j]) * t;
                hw[m] = (1.6f - 0.8f * (float)m / 4.0f) * (1.0f - k) * wk;
            }
            if (1.0f - k > 0.02f) FiRibbon(dev, vp, (const float(*)[3])P, hw, 5, 0.92f * (1.0f - k) * ks, kStr);
        }
        if (p->sdropSet[i]) {  // its body end drops off: a falling run of tar, fading
            const float td = ageR - p->sat[i], left = FiDropLeft(td), fall = FiDropFall(td);
            if (left > 0.01f) {
                for (m = 0; m < 5; ++m) {
                    t = (float)m / 4.0f;
                    for (j = 0; j < 3; ++j) P[m][j] = p->sdrop[i][j];
                    P[m][1] += fall - (8.0f + 0.12f * fall) * (1.0f - t);
                    hw[m] = (0.8f + 1.4f * t) * left;
                }
                FiRibbon(dev, vp, (const float(*)[3])P, hw, 5, 0.9f * left * ks, kStr);
            } else if (ph == 3) {
                continue;  // (all gone)
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
                if (p->nOwn > 0) drawn += FiOwnStrands(dev, &sv.vp, p) > 0;  // v3.58 FI358: beside the ghost's own too
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
// ================================================================ v3.59 FI359: Spirit Vision's spirit tar
// Spirit Vision (screenfx.inc) draws a black world of edge lines over the scene, so the tar portal drawn after pass 2 was
// gone there and its S5 pass drew v3.17's crimson vortex for each of the game's wall-portal tasks. Now, in its pass (both
// depth passes: what lies behind geometry dim, the rest full; its states, its depth, its eye), Refined's portals are drawn
// with Refined's own tar shapes (the pieces, the first portal, the blot sets' drips, the core, its sticky strands and the
// tar on the body) in kFiPalSpirit (additive spirit blue: a dark-blue fill, a glowing ragged rim, pale-blue strands), and
// the game's own wall portals as one such piece. [Gameplay] FadeInSpiritLook=0: v3.58 (the vortex; nothing for Refined's).
static int FiSpiritNow(void) { return g_fiSvLook && GvLive(); }
static void FiSpiritSay(int n, int wall)
{
    if (g_fiSvSaid || !n) return;
    g_fiSvSaid = 1;
    Log("fade in: Spirit Vision: %s drawn as spirit-blue tar in its pass (FadeInSpiritLook 1)", wall ? "the game's own wall portal"
        : "Reveal Method's tar portal (its pieces, drips and strands)");
}
// screenfx.inc's Spirit Vision pass (SvUntextured's additive states and SvZ(pass) are set; g_svEye / g_gvM are this frame's):
// Refined's portals. Returns those drawn.
static int __cdecl FiSpiritDraw(void *dev, const Viewport8 *vp)
{
    int i, n = 0;
    if (!g_fiReady || g_fiFail || !g_fiSvLook || !g_fiPortalN || g_fiMode != FI_MODE_PORTAL) return 0;
    __try {
        g_fiPal = &kFiPalSpirit;
        for (i = 0; i < FI_PORTAL_MAX; ++i) {
            FiPortal *p = &g_fiPortal[i];
            int d = 0;
            if (!p->on || p->kind == FI_TD_VORTEX) continue;
            d += FiOwnBlot(dev, vp, p);  // (the game's grid has no Spirit Vision form: Refined's discs for both kinds)
            FiOwnCore(dev, vp, p);
            if (p->nOwn > 0) d += FiOwnStrands(dev, vp, p) > 0;
            n += d > 0;
        }
    } __except (FiFaultNote(GetExceptionInformation())) {
        g_fiPal = &kFiPalTar;
        FiFailNow("the Spirit Vision portal draw");
        return n;
    }
    g_fiPal = &kFiPalTar;
    FiSpiritSay(n, 0);
    return n;
}
// screenfx.inc SvPortals: one of the game's own wall portals (its centre c, a little off the wall; its normal; its full
// width / height; how far it is open): 1 = drawn as spirit tar (the vortex is not drawn), 0 = draw the vortex
static int __cdecl FiSpiritWall(void *dev, const Viewport8 *vp, const float c[3], const float nn[3], float a, float b,
                                float open, unsigned seed)
{
    float ax[3], ay[3], o[3];
    int j, ok = 0;
    if (!g_fiReady || g_fiFail || !g_fiSvLook || !FiPlaneAxes(nn, ax, ay)) return 0;
    __try {
        const float gr = 0.35f + 0.65f * open;
        g_fiPal = &kFiPalSpirit;
        for (j = 0; j < 3; ++j) o[j] = c[j];
        FiOwnDisc(dev, vp, o, ax, ay, 0.5f * a * gr, 0.5f * b * gr, open * kFiPalSpirit.kFirst, FiHash(seed ^ 0x5B1Du));
        FiOwnDisc(dev, vp, o, ax, ay, 0.2f * a * gr, 0.2f * b * gr, 0.6f * open, FiHash(seed ^ 0xC0DEu));  // its core
        ok = 1;
    } __except (FiFaultNote(GetExceptionInformation())) {
        ok = 0;
        g_fiSvLook = 0;  // (v3.58's vortex from now on)
        Log("fade in: a fault in the Spirit Vision wall portal draw (code %08lX at %p): v3.58's look for this session",
            g_fiFaultCode, g_fiFaultAt);
    }
    g_fiPal = &kFiPalTar;
    if (ok) FiSpiritSay(1, 1);
    return ok;
}

static void FiPass2Draw(void)  // FiPhaseDraw: right after the game's pass 2 (where its own wall portals draw), game thread
{
    LARGE_INTEGER a, b, f;
    Viewport8 vp;
    void *dev;
    int i, game = 0, own = 0, need = 0, sv;
    if (!g_fiReady || g_fiFail || !g_fiPortalN || g_fiMode != FI_MODE_PORTAL) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay() || !(dev = DEV)) return;
    QueryPerformanceCounter(&a);
    memset(&vp, 0, sizeof vp);
    __try {
        FiFrameTick();
        sv = FiSpiritNow();  // v3.59 FI359: Spirit Vision's edge world replaces the scene: the portals are drawn in its pass
        if (FiPortalsExpire()) {
            for (i = 0; i < FI_PORTAL_MAX; ++i) {
                FiPortal *p = &g_fiPortal[i];
                p->drawnNow = 0;
                if (!p->on || p->kind == FI_TD_VORTEX) continue;
                FiMeasure(p);                                // v3.58 FI358: this frame's bones (the game's pass 2 is drawn)
                if (p->held && !p->ownStarted) FiStickStart(p);
                if (sv) continue;                            // v3.59 FI359 (FiSpiritDraw)
                if (p->kind == FI_TD_GAME && FiGameDraw(dev, p)) p->drawnNow = 1, game = 1;
                if (p->kind == FI_TD_OWN || p->nOwn > 0) need = 1;
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
        {   // v3.58 FI358: the portals' own limit (a share of the frame; the budget's governor does not count them)
            const float k0 = g_fiSelfK, dt = g_fiDtLast > 0.0f ? g_fiDtLast : 0.0f;
            g_fiMsEma += (ms - g_fiMsEma) * (dt > 0.0f ? (dt < 0.25f ? dt / 0.25f : 1.0f) : 0.0f);
            g_fiSelfK = FiSelfKeepStep(g_fiSelfK, g_fiMsEma, g_fiFrameEma, dt);
            if (k0 >= 0.95f && g_fiSelfK < 0.95f && LogQuota(&g_fiQuotaSelf, 10))
                LogV("fade in: the tar portals' draw takes %.2f ms a frame (more than %.0f %% of a %.1f ms frame): the pieces around "
                     "them thin (to 35 %% at most) until it is under %.2f ms", g_fiMsEma, 100.0f * FI_SELF_SHARE, g_fiFrameEma,
                     0.6f * FI_SELF_SHARE * g_fiFrameEma);
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
    if (page != OP_ENEMY || row != g_fiRow || row < 0) return -1;
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
    // v3.58 FI358: the hidden keys FadeInStrandSeconds (0 .. 6, default 3), FadeInPortalPieces (0 .. 9, default 9) and
    // FadeInStrands (0 .. 24, default 16); all three 0 / 0 / 12 = v3.57's portal. A bad value: the default, said once.
    GetPrivateProfileStringA("Gameplay", "FadeInStrandSeconds", "", sb, sizeof sb, g_iniPath);
    {
        char *end = NULL;
        double v = sb[0] ? strtod(sb, &end) : -1.0;
        if (sb[0] && (end == sb || !(v >= 0.0))) {
            v = -1.0;
            Log("fade in: [Gameplay] FadeInStrandSeconds \"%s\" is not 0 .. 6: %.1f (the default)", sb, FI_STICK_DEFAULT);
        }
        g_fiStick = FiStickClamp((float)v);
    }
    g_fiPieces = FiCountClamp(GetPrivateProfileIntA("Gameplay", "FadeInPortalPieces", FI_PIECES_DEFAULT, g_iniPath),
                              FI_PIECES_DEFAULT, FI_PIECES_MAX);
    g_fiStrN = FiCountClamp(GetPrivateProfileIntA("Gameplay", "FadeInStrands", FI_STRANDS_DEFAULT, g_iniPath),
                            FI_STRANDS_DEFAULT, FI_STRANDS_MAX);
    g_fiSvLook = FiSpiritLookParse(GetPrivateProfileIntA("Gameplay", "FadeInSpiritLook", 1, g_iniPath));  // v3.59 FI359 (hidden)
    Encode(g_txtFi[FIM_L], 56, OLW_ENEMIES_REVEAL_METHOD_L);  // v3.58 MN358: Reveal Method (was "Fade In")
    for (v = 0; v < FI_MODE_N; ++v) {
        Encode(g_txtFi[FIM_V0 + v], 56, OlShow(OLM_ENEMIES_REVEAL_METHOD, kFiModeName[v]));  // v3.53 OL1: Fade + Portal shows Portal  // v3.58 MN358: Fade shows Fade In
        Encode(g_txtFi[FIM_D0 + v], 56, v == FI_MODE_PORTAL && g_fiTdm != FI_TDM_VORTEX ? kFiDescTar : kFiDescTxt[v]);  // v3.51 PT351
        g_fiIds[v] = MSG_FI0 + FIM_V0 + v;
    }
    Encode(g_txtFi[FIM_NA], 56, OLW_FADEIN_NA);
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
        int i, ok[4] = { 1, 1, 1, 1 };  // v3.59 FI359: + group 3, the floor ray
        __try {
            for (i = 0; i < FI_TAR_NFN; ++i)
                if (memcmp((const void *)(ULONG_PTR)kFiTarFns[i].va, kFiTarFns[i].b, sizeof kFiTarFns[i].b)) {
                    if (PeHookedCallee(kFiTarFns[i].va, "fade in")) continue;  // v3.51 MERGE: Playable Eileen's hook (0x5736D0) is fine, as drops.inc
                    ok[kFiTarFns[i].group] = 0;
                    Log("fade in: %08X (%s) is not the stock code (another mod?): the tar portal does without it", kFiTarFns[i].va,
                        kFiTarFns[i].name);
                }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok[0] = ok[1] = ok[2] = ok[3] = 0;
        }
        g_fiGameOk = ok[0], g_fiStrandOk = ok[1], g_fiHoldFnOk = ok[2];
        g_fiFloorFnOk = ok[3];  // v3.59 FI359
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
            "FadeInHold %d, FadeInStrandSeconds %.1f, FadeInPortalPieces %d, FadeInStrands %d; never bosses, Wall Men, Walter, "
            "event or cut-scene figures; a fading enemy acts as usual; a held ghost cannot grab; v3.58: the tar portal 1.24 x "
            "the arriving body's height (the first portal and up to %d more of the same around it, thinned by the effect "
            "budget), its strands on the body %.1f s after it is in, the portal open as long; v3.59: %s; a victim coming out "
            "of a wall by itself unseen until it is out (at most %.1f s), lifted to its own height; FadeInSpiritLook %d (Spirit "
            "Vision: %s)", g_fiPortalOk ?
            "from the scene pass 004EFBFD" : "NOT available (no scene pass)", tar, kFiModeName[g_fiMode], g_fiSec,
            kFiTdmName[g_fiTdm], g_fiHoldOn, g_fiStick, g_fiPieces, g_fiStrN, g_fiPieces, g_fiStick,  // v3.58 FI358
            g_fiFloorFnOk ? "on the floor (the floor ray 00459490, a ceiling by 00561CD0)" : "NOT on the floor (00459490 is "
            "not stock: laid out as v3.58)", FI_WAIT_MAX_S, g_fiSvLook, g_fiSvLook ? "spirit-blue tar" : "v3.58's vortex");  // v3.59 FI359
    }
}

// v3.58 EN358: how much of an enemy's body Fade In shows now (hudbars.inc fades its health bar in with the body: "the bar
// appears with the body"): 1 = all of it (not in the fade table, its fade done, Fade In off or out); else the opaque share
// of this frame's fade (FiFrac: 0 at first sight .. 1 at FadeInSeconds). Read on the game thread after the scene pass.
static float FiBodyShown(const BYTE *task, const BYTE *work, unsigned id)
{
    int k;
    if (!g_fiReady || g_fiFail || g_fiMode == FI_MODE_OFF || !task) return 1.0f;
    if (FiWaiting(task, id)) return 0.0f;  // v3.59 FI359: kept unseen until it is out of its wall
    k = FiFind(&g_fiTab, task, work, id);
    if (k < 0 || g_fiTab.e[k].state != FI_E_FADE) return 1.0f;
    return FiFrac((float)(g_fiClock - g_fiTab.e[k].t0), g_fiSec);
}
