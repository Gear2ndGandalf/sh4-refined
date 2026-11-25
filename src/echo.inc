// ================================================================ v3.30 ECHO: the Echo double (special-mod round R4)
// v3.30 ECHO330 (staging/v330_feedback/w1_echo/PLAN.md, DESIGN.md): Echo Doubles. Off / Shadow Copy / Shadow Cast, 1-3
// shadows (the Echo Count row), the shadow-soldier look (near-black body, deep violet edge light, two violet eyes, a black
// aura of flame wisps, violet motes, a ground shadow; teleports dissolve into smoke and reform from it).
// v3.29 plan: staging/special_mods/r4_echo/PLAN.md (RE: re_echo.md; shared groundwork ../DESIGN.md 2 / 5, ../re_special.md C).
// Included by sh4windowmod.c after afterimage.inc. GOG v7 VAs. No exe patch of its own: it runs in Refined's tick
// dispatcher (QsTick 0x4EF982, after DpTick), the scene pass (SparkleFrame 0x4EFBFD, before DpDrawFrame), the model
// collect at 0x52AEB5 (ThermDrawC, beside DpCollect), Spirit Vision's GvPass and Thermal's FxPass (after their own body
// redraws) and one more mode of the draw-call override 0x4292EF / 0x429338 (g_fxDipOvr, screenfx.inc). Game code it calls
// (no patch): 0x55C650, 0x5615F0, 0x5619A0, 0x454000, 0x459490 (the floor under a point), 0x45BB60, 0x470060, 0x4FDF40,
// 0x4FDF80, 0x4FE0D0, 0x550D20, 0x45E760, 0x561CD0 (SvvCastT), 0x55BF50.
// The pure state code (rings, transforms, delays, teleports, records, damage, target spread, the sprite look) is
// echo_sim.h (test/echo_sim_test.c).
//
// Each shadow is her pose redrawn by the game's own character draw 0x55C650 from her bone matrices moved by its own
// transform D, so it has her exact pose and weapon. Its hits are her attack shapes moved by the same D, tested with the
// game's own shape pair test 0x5619A0 against the enemies' hurt shapes and landed as hit records in the live list
// 0xFCE8B0 (Death Presence's last-blow route): the enemies' own reactions play; One-Hit Kill applies (OhkDamage); the HP
// block gets +0x20 only, never the serial +0x1C.
//   Shadow Copy: her pose and moves late by its own delay (0.15-0.6 s, a new random value every 2-6 s, eased: EchoDly),
//          beside where she was; it turns (75 degrees at most, 360 deg/s) and glides (100 u) toward its own target during
//          a replayed attack.
//   Shadow Cast: the same copy, but while she attacks (and 0.8 s after) each shadow dissolves into smoke and reforms at an
//          enemy on her floor (another or the same: the fewest shadows first), from a random side, facing it, and replays
//          her attack there (its swing pulled onto the enemy within reach, as PE's MeleeAssist); when she stops it
//          returns beside her the same way.
// It never takes damage, blocks nothing, uses no ammo or durability; enemies cannot target it (re_echo 1).
// v3.31 ECHO331 (staging/v331_feedback/w1_echo/PLAN.md, DESIGN.md; the user's v3.30 test, items 1-4 and the slow frames):
//   - her busy moments (a Holy Candle set down, a floor pickup: EchoBusyStep): with an enemy near (the Shadow Cast rules)
//     each shadow replays her LAST ATTACK (its poses and windows, recorded: EchoRec*) at it (Shadow Cast: teleported
//     there; Shadow Copy: a short lunge from its place), again and again; with none, or no attack recorded yet, it stands
//     in her last idle pose beside her (never her candle / pickup pose). Pose sources: ESRC_LIVE / _REC / _IDLE, 0.2 s
//     cross-fades between them.
//   - one large black flame per shadow with a faint violet edge (echo_sim.h EchoLookBuild): no halos, wisps or motes; the
//     edge shell off by default (EchoRim=1 brings it back: on Playable Eileen's two-sided body it drew nothing at the
//     cost of a whole body pass).
//   - the eyes on the head bone's own axes (they stayed on the body's facing frame while the head turned and nodded).
//   - the places beside her in the frame of an anchor heading (EchoAnc) that holds while she attacks: a spin turns the
//     shadows in place, they no longer orbit her.
//   - the cost: no morph re-upload per redraw (EchoDrawOne), at most one Shadow Cast plan a tick and a floor cache, the
//     budget reacts to a single slow frame, the room line gives the tick / bodies / sprites times.
//   Draw entry points (W3 DEPTH331 adds its depth call here): EchoDrawFrame (scene pass) -> EchoDrawC -> EchoDrawAll;
//   EchoPostDraw (Spirit Vision / Thermal, after their filter) -> EchoDrawAll; inside EchoDrawAll: EchoBodies (the body
//   passes) then EchoSprites (ground discs, then the flame / eye / smoke list).
// v3.32 ECHO332 (staging/v332_feedback/w1_echo/PLAN.md, DESIGN.md; the user's v3.31 test, items 3-5):
//   - calm vs striking (EchoCalm, per shadow): not attacking, a shadow stands 1.5 x farther from her (its slot) at 45 %
//     opacity (its body through the depth prepass: the nearest surface only), so it no longer hides her; the moment it
//     strikes (its replayed sample is in a swing / shot / stomp or has a window, a busy replay, a Shadow Cast teleport to
//     an enemy or its stay there) it turns solid at the v3.31 distance in 0.12 s; 0.35 s after its last strike it eases
//     back over 0.5 s. Eased, by real time: the same path at any frame rate.
//   - a busy moment never shows her pickup / candle pose: the calm pose is always her idle pose (or, when none of her body
//     is kept yet, her pose from just before the busy moment, taken from the pose ring), also on the way to an enemy, at a
//     fallen one and in the delay after the busy moment ends (EchoLiveOk); the HUD gate closing one tick before the pickup
//     mode no longer dissolves them (ECHO_GATE_S). The v3.31 log of the user's stairs pickups (stage 2 room 5, Shadow
//     Cast turned on 9 s before): "stand idle beside her (no attack of hers kept yet)", but no idle pose of her body was
//     kept yet (kept only after 0.3 s standing with the row on), and that case fell back to her live (pickup) pose.
//   - the flame coat (echo_sim.h EchoLookBuild): black tongues along her body and limbs instead of one tall column, fine
//     purple wisps off its edges, the eyes' glow wider and softer.
// v3.33 ECHO333 (staging/v333_feedback/w1_echo/PLAN.md, DESIGN.md; the user's v3.32 QA item 1):
//   - "with weapons equipped 1 shadow stays transparent out of combat while the other two stay solid": the budget's level 2
//     (one frame over 6 ms: in the user's log within seconds of most rooms once each shadow redraws a weapon) dropped the
//     depth prepass of shadows 2 and 3; a calm 45 % body without it blends every surface of her model and reads ~83 %
//     (solid). Every body that stays see-through (calm, or under a vision filter) keeps its prepass now (EchoBodyPlan);
//     the budget drops it only in short fades.
//   - vision filters (Thermal, Spirit Vision, Night Vision / Mono): every shadow see-through at all times (50 % striking,
//     35 % calm: EchoBodyOp) in the filter's theme: Thermal a cold body with its coat as heat and hot points, Spirit Vision
//     pale spectral, night vision a phosphor edge with pale filaments (echo_sim.h EchoLookBuild).
// v3.34 FX334 (staging/v334_feedback/w2_fx/DESIGN.md; the user's v3.33 QA item 3: under Spirit Vision they "look too much like
//   the player"): Spirit Vision's shadows are dark specters: the body a near-black cold ink (EP_SPECTER: alpha-blended over
//   its prepass, texture detail mostly gone, 62 % striking .. 45 % calm), a coat of dark smoke, dark wisps, a faint cold rim
//   behind the coat, the eyes' pale light (echo_sim.h). Thermal and the night visions are unchanged.
//   - the log: calm / striking per shadow with its reason (LogV, 24 a room), the look switch, ANOMALY lines for a shadow
//     solid 2 s with no strike of hers, Shadow Copy shadows disagreeing 2 s, and a see-through shadow drawn opaque or
//     drawn unlike the others for 2 s (EchoCalmLog, EchoDisagreeCheck, EchoOpCheck); trace: each shadow's state a second.
#include "echo_sim.h"

#define ECHO_SIDE_D      120.0f   // beside her
#define ECHO_MIN_D       60.0f    // a wall may shrink it to this, then the other side, then a fade
#define ECHO_CHEST_DY    150.0f   // the wall rays' height above her feet (-Y is up)
#define ECHO_MAXB        48       // body matrices kept (Henry's 30, Playable Eileen's 30-36, KB 3)
#define ECHO_MAXW        16       // weapon matrices
#define ECHO_POSE_N      48       // the pose ring (decimated to 60 Hz: 0.8 s and more)
#define ECHO_TICK_N      160      // the tick ring (0.66 s at 240 Hz)
#define ECHO_SHAPES      3        // attack shapes kept per tick
#define ECHO_SPAWN_S     0.35f    // (it forms from smoke: the smoke gathers in ECHO_TP_IN)
#define ECHO_DESPAWN_S   0.4f
#define ECHO_FADE_S      0.2f     // wall / side-swap fade
#define ECHO_ENTER_S     0.5f     // after a room's gameplay starts
#define ECHO_FORM_GAP    0.15f    // the 2nd / 3rd shadow forms this much after the one before
#define ECHO_TAU         0.12f    // the placed offset follows its target with this time constant
#define ECHO_TURN_RATE   6.2831853f  // 360 degrees a second
#define ECHO_TURN_MAX    1.3089969f  // 75 degrees from her (sampled) facing
#define ECHO_TGT_D       600.0f
#define ECHO_GLIDE_D     100.0f
#define ECHO_GLIDE_V     400.0f
#define ECHO_WALL_DT     (1.0f / 30.0f)  // each shadow measures its walls at most 30 times a second
#define ECHO_PEND_N      24
#define ECHO_PEND_S      0.5f     // a gated enemy's pending twin hit is dropped after this
#define ECHO_NEAR0       60.0f    // the near-camera fade (units from the eye)
#define ECHO_NEAR1       220.0f
#define ECHO_RIM_K       1.035f   // the edge shell: each bone's 3 x 3 part x this about its origin
#define ECHO_TELEPORT    400.0f   // she moved this far in one tick: a door / a warp: the shadows form again
// Shadow Cast
#define ECHO_CAST_HOLD   0.8f     // engaged while she attacks and this long after
#define ECHO_CAST_R      1500.0f  // enemies within this (xz) of her
#define ECHO_CAST_SO     70.0f    // the spot: this far beyond the enemy's body radius (20..90)
#define ECHO_CAST_TURN   9.424778f   // 540 degrees a second (its facing toward its enemy)
#define ECHO_CAST_MOVED  220.0f   // its enemy got this much farther than the stand-off: it follows (a new spot)
#define ECHO_CAST_COOL   0.5f     // no enemy found: asked again after this
#define ECHO_FLOOR_SPOT  40.0f    // the spot's floor within this of its enemy's floor (no ledge, no hole)
#define ECHO_MAG_R       130.0f   // a replayed swing within this (+ its shape's size) of its enemy is pulled onto it
#define ECHO_EVT_N       3        // smoke events kept per shadow
// v3.31 ECHO331: her busy moments
#define ECHO_BZ_R        700.0f   // Shadow Copy: enemies within this (xz) of her count as near (the Shadow Cast rules)
#define ECHO_LUNGE_D     300.0f   // Shadow Copy: its lunge from its place toward its enemy at most this far
#define ECHO_LUNGE_S     0.15f    // ... in this long
#define ECHO_SRC_FADE    0.20f    // the cross-fade between two pose sources
#define ECHO_IDLE_V      25.0f    // u/s: she stands (her last idle pose is kept 10 times a second after 0.3 s of it)
#define ECHO_FLOOR_N     16       // the floor cache (an enemy's floor is measured at most every 0.3 s)
#define ECHO_FLOOR_KEEP  0.3f
// v3.32 ECHO332
#define ECHO_GATE_S      0.12f    // the HUD gate closed this long before they dissolve (an item event closes it a tick before
                                  // the pickup mode starts: v3.31 dissolved and re-formed them at every pickup)
#define ECHO_BZ_MARGIN   0.05f    // her live pose this far from a busy moment's start / end (EchoLiveOk)
#define ECHO_BZ_SNAP     0.10f    // no idle pose of her body kept: her pose this long before the busy moment instead
enum { ESRC_LIVE, ESRC_REC, ESRC_IDLE };  // a shadow's pose: hers its delay ago, her last attack, her last idle pose

// ---------------------------------------------------------------- settings (gameplay.inc GP_ECHO / GP_ECHON; hidden ini keys)
// g_echoMode (0 Off, 1 Shadow Copy, 2 Shadow Cast: ECHO_* in echo_sim.h), g_echoCountV (0..2 = 1..3 shadows) and
// g_echoReady are in gameplay.inc
static int g_echoInstalled;
static volatile int g_echoFail;
static float g_echoShare = 0.5f;     // [Gameplay] EchoDamage / 100 (EchoShareFor: x 1.5, then x 1 / 0.8 / 0.67)
static int g_echoSidePref;           // [Gameplay] EchoSide: 0 Auto, 1 Right, 2 Left
static int g_echoColour;             // [Gameplay] EchoColour: 0 Violet, 1 Blue, 2 Crimson, 3 White
static int g_echoDepth = 2;          // [Gameplay] EchoDepth: a depth prepass while fading: 0 never, 1 in every look (v3.30's
                                     // meaning, kept: ECHO-331-002), 2 under Thermal / Spirit Vision only (the v3.31 default
                                     // when the key is absent: in the normal look the black flame hides a fading body's
                                     // overlaps: one pass)
static int g_echoInkEq;              // v3.31 ECHO331: this shadow's colour pass sits on its own prepass (ZFUNC EQUAL)
static int g_echoRim = 0;            // [Gameplay] EchoRim: the edge shell (v3.31 ECHO331: default 0, the flame's edge)
static int g_echoAura = 1;           // [Gameplay] EchoAura: the sprites (the flame, eyes, smoke, ground shadow)
static int g_echoMorphKeep = 1;      // v3.31 ECHO331 [Gameplay] EchoMorphKeep: 1 = a redraw keeps the face morph the game
                                     // uploaded this frame (no second upload of its vertex buffer); 0 = v3.30
enum { ELG_PESHOT, ELG_HEAP, ELG_SHOT, ELG_SWAP, ELG_BOTH, ELG_BONES, ELG_WEP, ELG_SHADER, ELG_DRAWS, ELG_CULL, ELG_RSIDE,
       ELG_OLDINI, ELG_FLOOR, ELG_CAST, ELG_HOME, ELG_NOTGT, ELG_BUDGET, ELG_WARM, ELG_SPR,
       ELG_BUSY, ELG_REC, ELG_RECBAD, ELG_BZATK, ELG_BZIDLE, ELG_MORPH,
       ELG_SNAP, ELG_CALM, ELG_FREEZE, ELG_DEPTH0, ELG_N };  // v3.31 ECHO331: + 6; v3.32 ECHO332: + 3; v3.33 ECHO333: + 1
static int g_echoLogged[ELG_N];
static const float kEchoLight[4][3] = { { 0.55f, 0.20f, 1.0f },   // Violet (the edge light, the eyes, the motes)
                                        { 0.22f, 0.45f, 1.0f },   // Blue
                                        { 1.0f, 0.16f, 0.26f },   // Crimson
                                        { 0.85f, 0.85f, 1.0f } }; // White
static const float kEchoInk[4][3] = { { 0.583f, 0.417f, 1.0f }, { 0.35f, 0.55f, 1.0f }, { 1.0f, 0.30f, 0.36f }, { 0.85f, 0.85f, 1.0f } };

// gameplay.inc reads the ini through this: v3.29's Shadow / Twin / Mirror are Shadow Copy now (logged once)
static void EchoIniAlias(char *buf, size_t n)
{
    if (!EchoOldName(buf)) return;
    if (!g_echoLogged[ELG_OLDINI]++)
        Log("echo: [Gameplay] Echo=%s is a v3.29 value: read as Shadow Copy (the ini keeps its text until the row is changed)", buf);
    strcpy_s(buf, n, "Shadow Copy");
}

// ---------------------------------------------------------------- fail-closed (the DpFailNow pattern)
static DWORD g_echoFaultCode;
static void *g_echoFaultAt;
static int EchoFaultNote(EXCEPTION_POINTERS *e)
{
    g_echoFaultCode = e->ExceptionRecord->ExceptionCode;
    g_echoFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static void EchoFailNow(const char *where)
{
    if (g_echoFail) return;
    g_echoFail = 1;
    g_echoReady = 0;  // the rows say "not available"
    InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
    Log("echo: %s faulted (code %08lX at %p): the Echo Doubles are off for this session", where, g_echoFaultCode, g_echoFaultAt);
}

// ---------------------------------------------------------------- the tick side: her samples and the shadows' state
typedef struct {
    float t, pos[3], fwd[2], right[2];
    float afwd[2], aright[2];  // v3.31 ECHO331: the anchor heading's frame (the places beside her: EchoAnc)
    int st;                    // v3.32 ECHO332: her mode P+0x214 this tick (a shadow replaying a swing / shot / stomp strikes)
    int n;                 // attack shapes this tick (0: no open window)
    EcU32 S;               // the window (her attack serial)
    int id, flags, line;   // her attack id, flags (bit 1: dir valid), a gun line
    float dmg, push, dir[4];
    __declspec(align(16)) unsigned char shape[ECHO_SHAPES][ECHO_SHAPE_SZ];
} EchoTickS;
static __declspec(align(16)) EchoTickS g_echoTs[ECHO_TICK_N];
static EchoRing g_echoTR;
typedef struct {           // one shadow
    int on, leaving, blocked, flash;
    float sp, vis, rise, age, formedAt;        // spawn 0..1, wall / swap visibility 0..1, the dissolve's rise
    float side, dist, off[3], hold, wallT, freeD, flipWant;  // its slot (side +1 = the slot's own side, -1 mirrored; v3.32:
                                                             // dist / freeD the free fraction of the slot, 1 = all of it)
    float yaw, glide, gdir[2], glideMax;       // Copy: its own turn / glide toward its own target
    EchoDly dly;
    float delay;                               // this tick's delay (the draw reads it)
    float replayT;                             // the last replayed tick sample's time
    EcU32 rS;                                  // the replayed window
    const BYTE *tgt, *tgtW;                    // its own target this window (task + work)
    EchoFoeTab foes;                           // its records: one per enemy per window
    EcU32 T, TFor;                             // its twin serial, made for window TFor
    EchoTp tp;                                 // Shadow Cast: the teleport
    const BYTE *ct, *ctW, *nt, *ntW;           // its enemy (task + work) and the planned one
    float nspot[3], nso, so, cface, cool;      // the planned spot and stand-off; its stand-off; its facing (world); retry time
    EchoPl pl;                                 // its place this tick (the draw's transform)
    float faceA;                               // its facing angle (world: (sin, cos))
    int evN;                                   // smoke events made (the ring index)
    // v3.31 ECHO331: its pose source (ESRC_*), the one before and the cross-fade 0..1; the replay of her last attack (time
    // in its repetition, the repetition, stop after this one), its enemy then (task + work), the lunge (Shadow Copy:
    // direction, length, 0..1), the facing toward its enemy (world angle); the places of the replay and the idle pose
    int src, srcOld;
    float srcK;
    float bzT, bzLocal;
    int bzRep, bzStop;
    const BYTE *bt, *btW;
    float bzDir[2], bzLen, bzK, bzFace, bzCool;
    EchoPl bzPl, idPl;
    EchoCalm calm;                             // v3.32 ECHO332: calm (farther, faded) vs striking (solid)
    float idFace;                              // v3.32 ECHO332: the anchor heading its idle pose faces (world angle)
    // v3.33 ECHO333: the log's view of calm vs striking: striking (from its first strike tick until k is back at 1), since,
    // the reason now / the last one and when, how long it has been solid (k < 0.5), its anomaly said this solid stretch
    int striking, why, lastWhy, anomSolid;
    float strikeT0, lastWhyT, solidT;
} EchoShd;
typedef struct {           // shared
    int on, mode, n;       // shown; the behaviour they were made with; the count
    float clock, lastPos[3], wait, formT, sideWant, lastAtk, herH, herHT, herFloor;
    DWORD room;
    int gen, engaged, engage, herFloorOk;      // gen +1 at every reset (the draw clears its pose ring); engagements
    float rsign;           // her right = rsign x (cos yaw, -sin yaw) (calibrated from her shoulders in the draw)
    int rvote, herFloorTick, ticks;
    EcU32 seed;
    // v3.31 ECHO331: the anchor heading; her busy moment (EBZ_*) and its latch; the last busy tick with an attack to replay
    // (Shadow Cast's engagement); her idle time (the idle pose is kept after 0.3 s); Shadow Cast plans this tick (<= 1)
    EchoAnc anc;
    EchoBusyL bl;
    int busy, busyLog, plans;
    float lastBusy, idleT;
    // v3.32 ECHO332: the busy moment's start and end (tick clock; busyT1 < 0: none in this room), its count (the draw takes
    // a pre-busy pose once per busy moment), the HUD gate's closed time
    float busyT0, busyT1, gateT;
    int busyGen;
    // v3.33 ECHO333: the last tick with an open hit window of hers, how long her mode has been a swing / stomp (3 / 4 / 5),
    // how long the shadows have disagreed (Shadow Copy: one solid, one calm), that anomaly said this stretch
    float lastWin, modeT, disT;
    int anomDis;
} EchoCtl;
// v3.31 ECHO331: her last attack (outside g_ec: it outlives a room change; a body switch drops it). The tick keeps its
// window samples (g_echoRecHR / g_echoRecHS), the draw its poses (g_echoRecPR ... in the draw section).
typedef struct {
    int state;             // 0 none, 1 recording, 2 complete
    int gen;               // +1 at each new recording (the draw starts its pose list again)
    float t0, t1, lastAct, root0[3], face0;    // start / end (tick clock), her last activity, her place and facing at the start
    const void *file;      // her body's model file when it was recorded
} EchoRecS;
static EchoRecS g_echoRec;
static EchoRing g_echoRecHR;
static __declspec(align(16)) EchoTickS g_echoRecHS[ECHO_REC_HIT_N];
typedef struct { const BYTE *w; float y, t; int ok; } EchoFloorC;  // the floor cache (an enemy's work, its floor, when)
static EchoFloorC g_echoFloorC[ECHO_FLOOR_N];
static EchoCtl g_ec;
static EchoShd g_es[ECHO_MAXN];
static EchoWin g_echoWinA;       // her attack object 0x10A4FE0
typedef struct { int id, flags; float dmg, push, dir[4]; } EchoAtkV;
static EchoAtkV g_echoWinV;      // the window's last live values (its release tail uses them)
static EcU32 g_echoLastB;        // Playable Eileen's weapon attack 0xFCE5C0: the last serial seen
static DpU32 g_echoRng = 0x5EC0u;
static DWORD g_echoTypeLog;
typedef struct { const BYTE *task, *work; EchoHit h; float base, age; int type; } EchoPend;
static EchoPend g_echoPend[ECHO_PEND_N];
static int g_echoPendN;
typedef struct { EchoEvt e; int filled; float tries; } EchoEvtS;  // a smoke event; filled: its bones captured by the draw
static EchoEvtS g_echoEv[ECHO_MAXN][ECHO_EVT_N];
static const float kEchoSlot[ECHO_MAXN][2] = { { 120.0f, 0.0f }, { -120.0f, -20.0f }, { 70.0f, -125.0f } };  // (right, forward)

static int EchoF3(const float *p) { return _finite(p[0]) && _finite(p[1]) && _finite(p[2]); }
static void EchoRightOf(float yaw, float rs, float r[2]) { r[0] = rs * cosf(yaw), r[1] = -rs * sinf(yaw); }
static BYTE *EchoHerModel(void) { return *(BYTE *const volatile *)(0x10A0F20 + 0x1B0); }
static int EchoCount(void) { const int v = (int)g_echoCountV; return v < 0 ? 1 : v >= ECHO_MAXN ? ECHO_MAXN : v + 1; }

static void EchoShdClear(EchoShd *e)
{
    memset(e, 0, sizeof *e);
    e->vis = 1.0f, e->side = 1.0f, e->flipWant = 1.0f, e->dist = e->freeD = 1.0f;  // v3.32 ECHO332: fractions of the slot
    e->replayT = -1.0e30f;
    e->srcK = 1.0f;  // v3.31 ECHO331 (ESRC_LIVE, no cross-fade)
}
static void EchoReset(int respawnWait)
{
    const float rs = g_ec.rsign ? g_ec.rsign : 1.0f, hh = g_ec.herH;
    const int gen = g_ec.gen + 1, vote = g_ec.rvote, engage = g_ec.engage, busyGen = g_ec.busyGen;  // v3.32 ECHO332: + busyGen (the pre-busy pose is taken once per busy moment)
    const DWORD room = g_ec.room;
    const EcU32 seed = g_ec.seed;
    int i;
    memset(&g_ec, 0, sizeof g_ec);
    g_ec.gen = gen, g_ec.rsign = rs, g_ec.rvote = vote, g_ec.room = room, g_ec.herH = hh > 0.0f ? hh : 170.0f;
    g_ec.engage = engage, g_ec.seed = seed, g_ec.busyGen = busyGen;
    g_ec.sideWant = 1.0f;
    g_ec.lastAtk = -1.0e9f;
    g_ec.wait = respawnWait ? ECHO_ENTER_S : 0.0f;
    g_ec.busyT1 = -1.0f;  // v3.32 ECHO332
    g_ec.lastWin = -1.0e9f;  // v3.33 ECHO333
    for (i = 0; i < ECHO_MAXN; ++i) EchoShdClear(&g_es[i]);
    memset(g_echoEv, 0, sizeof g_echoEv);
    EchoRingInit(&g_echoTR, ECHO_TICK_N);
    memset(&g_echoWinA, 0, sizeof g_echoWinA);
    memset(&g_echoWinV, 0, sizeof g_echoWinV);
    g_echoPendN = 0;
    memset(g_echoFloorC, 0, sizeof g_echoFloorC);  // v3.31 ECHO331
}
static void EchoEvtAdd(int i, int kind)  // a smoke event at its place now (the draw captures its bones)
{
    EchoShd *e = &g_es[i];
    EchoEvtS *s = &g_echoEv[i][e->evN++ % ECHO_EVT_N];
    memset(s, 0, sizeof *s);
    s->e.t0 = g_ec.clock;
    s->e.kind = kind;
    s->e.seed = EchoHash(g_ec.seed ^ (EcU32)(i * 7919 + e->evN * 104729));
}

// the shapes of a hit body (+0x24 shape[], +0x3C count), node-bound ones recomputed from their bone (0x5615F0 on a copy:
// the game's own refresh, without Playable Eileen's MeleeAssist, which moves the world centre onto her target)
static int EchoCopyShapes(const BYTE *body, unsigned char (*out)[ECHO_SHAPE_SZ], int *node)
{
    int i, n, k = 0;
    if (!body) return 0;
    n = body[0x3C];
    for (i = 0; i < n && i < 6 && k < ECHO_SHAPES; ++i) {
        const BYTE *s = *(const BYTE *const *)(body + 0x24 + 4 * i);
        const int kind = s ? s[0xA0] : 0;
        if (!s || kind < 1 || kind > 5) continue;
        memcpy(out[k], s, ECHO_SHAPE_SZ);
        if (*(void *const *)(s + 0xA8)) {
            ((void(__cdecl *)(void *))0x5615F0)(out[k]);
            if (node) *node = 1;
        }
        if (!EchoF3((const float *)out[k])) continue;
        ++k;
    }
    return k;
}
// this tick's attack sample (her state as the previous tick's player update left it). Henry and Playable Eileen alike:
// swings, charged swings, stomps and Henry's guns all build sAttack 0x10A4FE0 (0x543C70 / 0x543E70 / 0x544470, pl_weapon.c);
// Playable Eileen's own weapon shots arm 0xFCE5C0 (below)
static void EchoSampleAttack(EchoTickS *s, float now)
{
    const BYTE *A = (const BYTE *)0x10A4FE0;
    const BYTE *cur = *(const BYTE *const volatile *)0x10A4EF0;
    const int mode = *(const volatile int *)(0x10A0F20 + 0x214);
    const int live = cur == A && *(const DWORD *)(A + 0x3C) == 0x10A0F20;
    const EcU32 S = live ? *(const EcU32 *)(A + 0x38) : 0;
    const BYTE *body = live ? *(const BYTE *const *)(A + 0x0C) : *(const BYTE *const *)(0x10A0F20 + 0x3A0);
    int node = 0, line = 0, open, n;
    __declspec(align(16)) static unsigned char tmp[ECHO_SHAPES][ECHO_SHAPE_SZ];
    s->n = 0;
    n = body && (live || g_echoWinA.S) ? EchoCopyShapes(body, tmp, &node) : 0;  // (only in an attack or its tail)
    line = n > 0 && tmp[0][0xA0] == 1;
    open = EchoWinStep(&g_echoWinA, (S >> 16) == 0xBA1E ? S : 0, live && (S >> 16) == 0xBA1E, mode == 3 || mode == 4, node, line, now);
    if (open && n) {
        if (live) {  // her attack's values (damage / push already scaled by Playable Eileen and the table, re_echo 2)
            g_echoWinV.id = *(const int *)(A + 0x00);
            g_echoWinV.dmg = *(const float *)(A + 0x04);
            g_echoWinV.push = *(const float *)(A + 0x08);
            g_echoWinV.flags = *(const int *)(A + 0x34);
            memcpy(g_echoWinV.dir, A + 0x20, 16);
        }  // (the release tail: the window's last live values)
        s->id = g_echoWinV.id, s->dmg = g_echoWinV.dmg, s->push = g_echoWinV.push, s->flags = g_echoWinV.flags;
        memcpy(s->dir, g_echoWinV.dir, 16);
        s->S = g_echoWinA.S, s->line = line, s->n = n;
        memcpy(s->shape, tmp, (size_t)n * ECHO_SHAPE_SZ);
        return;
    }
    {   // Playable Eileen's SMG on the player: each shot arms 0xFCE5C0 with a new serial (id 0x48, the line 0xFCE650)
        const BYTE *obj = *(const BYTE *const volatile *)0xFCE5B0;
        const BYTE *B = (const BYTE *)0xFCE5C0;
        const EcU32 sb = *(const volatile EcU32 *)(B + 0x38);
        int mine = 0;
        if (obj && TaskSlotOk(obj) &&  // v3.36 TP336
            *(const DWORD *)(obj + 0x404) == 0x4456C0 && *(const DWORD *)(obj + 0x428) == 0x443AC0) {
            const BYTE *w = *(const BYTE *const *)(obj + 0x408), *owner = w ? *(const BYTE *const *)(w + 0xF0) : NULL;
            mine = owner && *(const DWORD *)(owner + 0x18) == 0x10A0F20 + 0xC0;  // PE's stand-in owner (KB 8): the player's model
        }
        if (!g_echoLastB || !mine) {
            g_echoLastB = sb;
            return;
        }
        if (sb == g_echoLastB || (sb >> 16) != 0xBA1E) return;
        g_echoLastB = sb;
        n = EchoCopyShapes((const BYTE *)0xFCE650, tmp, NULL);
        if (!n) return;
        s->id = *(const int *)(B + 0x00), s->dmg = *(const float *)(B + 0x04), s->push = *(const float *)(B + 0x08);
        s->flags = *(const int *)(B + 0x34);
        memcpy(s->dir, B + 0x20, 16);
        s->S = sb, s->line = tmp[0][0xA0] == 1, s->n = n;
        memcpy(s->shape, tmp, (size_t)n * ECHO_SHAPE_SZ);
        if (!g_echoLogged[ELG_PESHOT]++) Log("echo: Playable Eileen's weapon shot seen (attack 00FCE5C0, id %02X): the shadows fire too", s->id);
    }
}

// ---------------------------------------------------------------- landing a twin hit
static int EchoHittable(const BYTE *t, BYTE **wOut, float **hpOut, int *typeOut)
{
    BYTE *w = *(BYTE *const *)(t + 0x408);
    float *hp;
    int type, cls;
    if (!w) return 0;
    type = *(const int *)(w + 0x20);
    if (!DpTypeOk(type)) return 0;  // never 1, 4, 5 (Wall Man / the Wall boss), 0x10 (Walter, the Bodies), 0x12+
    cls = *(const int *)(w + 0x3C);
    if (cls == 0x0B || cls >= 0x10) return 0;  // dead, removed, scripted
    if (DpIsGhost(type) && (cls == 9 || cls == 0x0A)) return 0;  // a downed ghost: any hit halves its countdown (re_special A2)
    hp = *(float *const *)(w + 0x34);
    if (!hp || !(hp[1] > 0.0f) || !_finite(hp[0]) || !(hp[0] > 0.0f)) return 0;
    *wOut = w, *hpOut = hp, *typeOut = type;
    return 1;
}
static int EchoIsDown(const BYTE *t) { return ((int(__cdecl *)(const BYTE *))0x45BB60)(t) != 0; }  // EnemyIsDown (matched)
static void EchoAim(const BYTE *w, float p[3])  // the enemy's aim point (work +0x10), else its position
{
    memcpy(p, w + 0x10, 12);
    if (!EchoF3(p)) memcpy(p, w, 12);
}
static int EchoClear(const float a[3], const float b[3])  // 0x454000 (matched): 0 = nothing between
{
    __declspec(align(16)) float pa[4], pb[4];
    memcpy(pa, a, 12), memcpy(pb, b, 12);
    pa[3] = pb[3] = 1.0f;
    return !((int(__cdecl *)(void *, float *, float *))0x454000)(NULL, pa, pb);
}
// the floor under p: EnemyFloorRayCheck 0x459490 (matched, en_451090.c: a ray from 4 u above p down 200, then 2000 and
// 4000 more; a pure query on one of the mover state's 4 ray slots, freed at its end). Only when a slot is free (states at
// 0xFCEE40 + 0x30 k: 3 = free): with none free the stock picker would run past the array. 1 = found (y).
static int EchoFloorY(const float p[3], float *y)
{
    __declspec(align(16)) float pos[4], out[12];
    int k, freeSlot = 0;
    for (k = 0; k < 4; ++k) freeSlot |= *(const volatile int *)(0xFCEE40 + 0x30 * k) == 3;
    if (!freeSlot) {
        if (!g_echoLogged[ELG_FLOOR]++) Log("echo: no free mover ray for the floor query (the same floor is not checked this time)");
        return 0;
    }
    memcpy(pos, p, 12);
    pos[3] = 1.0f;
    memset(out, 0, sizeof out);
    if (!((int(__cdecl *)(float *, float *))0x459490)(out, pos)) return 0;
    if (!_finite(out[1])) return 0;
    *y = out[1];
    return 1;
}
// the record (0x44F410's rules, re_echo 6.4): resist, random range, share, One-Hit Kill; never hp+0x1C
static int EchoLand(const BYTE *t, BYTE *w, float *hp, int type, EchoHit *h, float base)
{
    float rate, d;
    DWORD bits;
    BYTE *rec;
    if (EchoIdDropped(h->id, type)) return 0;
    if (((int(__cdecl *)(int))0x4FE0D0)(h->id) && !EchoIsDown(t)) return 0;  // stomps: downed only
    rate = ((float(__cdecl *)(int, int))0x470060)(type, ((int(__cdecl *)(int))0x4FDF80)(h->id));
    d = EchoDamage(base, rate, DpRnd(&g_echoRng), 1.0f, h->id);
    if (!(d > 0.0f)) return 0;
    memcpy(&bits, &d, 4);
    bits = OhkDamage((BYTE *)t, hp, bits, NULL);  // the One-Hit Kill row (records bypass 0x44F605)
    memcpy(&d, &bits, 4);
    if (!(d > 0.0f) || !_finite(d)) return 0;
    rec = ((BYTE *(__cdecl *)(DWORD))0x550D20)(0xFCE8B0);
    if (!rec) {
        if (!g_echoLogged[ELG_HEAP]++) Log("echo: no hit record could be allocated (heap): a twin hit is lost");
        return 0;
    }
    h->dmg = d;
    h->cat = ((int(__cdecl *)(int))0x4FDF40)(h->id);
    h->task = t;
    EchoRecFill(rec, h);
    EchoHpMark((BYTE *)hp, h->id);
    if (type < 32 && !(g_echoTypeLog >> type & 1)) {
        g_echoTypeLog |= 1u << type;
        Log("echo: first twin hit on type %02X (attack %02X, %.1f damage, hp %.0f / %.0f%s)", type, h->id, d, hp[0], hp[1],
            DpUpdatesNow(t) ? "" : ", held for its update tick");
    }
    (void)w;
    return 1;
}
static void EchoPendAdd(const BYTE *t, const BYTE *w, const EchoHit *h, float base, int type)
{
    EchoPend *p;
    if (g_echoPendN >= ECHO_PEND_N) return;
    p = &g_echoPend[g_echoPendN++];
    p->task = t, p->work = w, p->h = *h, p->base = base, p->age = 0.0f, p->type = type;
}
static void EchoPendStep(float dt)  // EntityGate: a gated monster gets its twin hit on its next update tick (once)
{
    int i, k = 0;
    for (i = 0; i < g_echoPendN; ++i) {
        EchoPend *p = &g_echoPend[i];
        BYTE *w;
        float *hp;
        int type;
        p->age += dt;
        if (p->age > ECHO_PEND_S || *(BYTE *const *)(p->task + 0x408) != p->work) continue;  // too late / another being
        if (!DpUpdatesNow(p->task)) {
            g_echoPend[k++] = *p;
            continue;
        }
        if (EchoHittable(p->task, &w, &hp, &type)) EchoLand(p->task, w, hp, type, &p->h, p->base);
    }
    g_echoPendN = k;
}
static int EchoHurtOk(const BYTE *hs)  // a hurt shape in the shape pool (flags & 8, the 0x44F900 test)
{
    if (!HpShapeInPool(hs)) return 0;  // v3.36 HE336: the game's 100 shapes at 0x12F1C30 or hitpool.inc's block (was the 100 only)
    return (*(const int *)(hs + 0xA4) & 8) != 0;
}
// One window's shapes, moved by M, against every hittable enemy: at most one record per enemy per window per shadow; a gun
// line hits the nearest enemy along it with a clear line from its start. aim: its shot goes there; mag: Shadow Cast's
// enemy (task + work): a swing within reach is pulled onto its nearest hurt shape first.
static void EchoStrike(EchoShd *e, const EchoTickS *s, const float M[16], const float aim[3], const BYTE *mag, const BYTE *magW)
{
    __declspec(align(16)) static unsigned char sh[ECHO_SHAPES][ECHO_SHAPE_SZ];
    float c[ECHO_SHAPES][3], r[ECHO_SHAPES], share;
    const BYTE *t = NULL, *best = NULL;
    BYTE *bw = NULL;
    float *bhp = NULL, bd = 1.0e30f, bc[3] = { 0, 0, 0 };
    int i, guard = 0, btype = 0;
    if (!s->n || !s->S) return;
    share = EchoShareFor(g_echoShare, g_ec.n);
    for (i = 0; i < s->n; ++i) {  // the shapes of the shadow (and their bounding spheres, as 0x561710)
        float *f = (float *)sh[i];
        memcpy(sh[i], s->shape[i], ECHO_SHAPE_SZ);
        EchoShapeXform(sh[i], M);
        if (s->line && aim && sh[i][0xA0] == 1) {  // its shot goes at its own target
            float d[3], l = 0.0f, L;
            int j;
            for (j = 0; j < 3; ++j) d[j] = f[4 + j] - f[j];
            L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            for (j = 0; j < 3; ++j) d[j] = aim[j] - f[j], l += d[j] * d[j];
            l = sqrtf(l);
            if (l > 1.0f && L > l)
                for (j = 0; j < 3; ++j) f[4 + j] = f[j] + d[j] / l * L;
        }
        {
            const int k = sh[i][0xA0];
            r[i] = k == 2 ? f[4] : k == 5 ? sqrtf(f[16] * f[16] + f[17] * f[17] + f[18] * f[18])
                                          : 0.5f * sqrtf((f[4] - f[0]) * (f[4] - f[0]) + (f[5] - f[1]) * (f[5] - f[1]) + (f[6] - f[2]) * (f[6] - f[2]));
        }
        if (mag && !s->line && *(const BYTE *const *)(mag + 0x408) == magW && magW) {  // Shadow Cast: the reach
            const BYTE *hb = *(const BYTE *const *)(magW + 0x38);
            float cc[3], bestD = 1.0e30f, bestC[3];
            int k, cnt;
            EchoShapeCentre(sh[i], cc);
            cnt = hb ? hb[0x3C] : 0;
            for (k = 0; k < cnt && k < 6; ++k) {
                const BYTE *hs = *(const BYTE *const *)(hb + 0x24 + 4 * k);
                float hc[3], dd;
                if (!EchoHurtOk(hs)) continue;
                EchoShapeCentre(hs, hc);
                dd = (hc[0] - cc[0]) * (hc[0] - cc[0]) + (hc[1] - cc[1]) * (hc[1] - cc[1]) + (hc[2] - cc[2]) * (hc[2] - cc[2]);
                if (dd < bestD) bestD = dd, memcpy(bestC, hc, sizeof bestC);
            }
            if (bestD < 1.0e29f) EchoMagnet(sh[i], bestC, r[i] + ECHO_MAG_R);
        }
        EchoShapeCentre(sh[i], c[i]);
    }
    if (e->TFor != s->S) e->TFor = s->S, e->T = 0;
    while ((t = DpNext(t)) != NULL && guard++ < 96) {
        BYTE *w, *hb;
        float *hp, ac[3];
        int type, k, cnt, hit = 0;
        if (!EchoHittable(t, &w, &hp, &type)) continue;
        hb = *(BYTE *const *)(w + 0x38);  // the mover; its HitBody at +0 (include/enemy_mover.h)
        if (!hb) continue;
        cnt = hb[0x3C];
        for (k = 0; k < cnt && k < 6 && !hit; ++k) {
            const BYTE *hs = *(const BYTE *const *)(hb + 0x24 + 4 * k);
            float hc[3], dx, dy, dz;
            int j;
            if (!EchoHurtOk(hs)) continue;
            EchoShapeCentre(hs, hc);
            for (j = 0; j < s->n && !hit; ++j) {
                float hr = hs[0xA0] == 2 ? ((const float *)hs)[4] : 150.0f;
                dx = hc[0] - c[j][0], dy = hc[1] - c[j][1], dz = hc[2] - c[j][2];
                if (!s->line && dx * dx + dy * dy + dz * dz > (r[j] + hr + 60.0f) * (r[j] + hr + 60.0f)) continue;
                if (((int(__cdecl *)(const void *, const void *))0x5619A0)(sh[j], hs)) {
                    hit = 1;
                    memcpy(ac, hc, sizeof ac);
                }
            }
        }
        if (!hit) continue;
        if (s->line) {  // the nearest along the line
            const float *f = (const float *)sh[0];
            const float dd = (ac[0] - f[0]) * (ac[0] - f[0]) + (ac[1] - f[1]) * (ac[1] - f[1]) + (ac[2] - f[2]) * (ac[2] - f[2]);
            if (dd < bd && EchoClear(f, ac)) bd = dd, best = t, bw = w, bhp = hp, btype = type, memcpy(bc, ac, sizeof bc);
            continue;
        }
        {
            EchoFoe *fo = EchoFoeFind(&e->foes, t, w);
            EchoHit h;
            float d[3], l;
            int j;
            if (!EchoFoeTake(fo, s->S)) continue;
            memset(&h, 0, sizeof h);
            memcpy(h.pos, ac, 12), h.pos[3] = 1.0f;
            if (s->flags & 2) EchoVec(M, s->dir, d);  // her attack's own direction, turned with the shadow
            else for (j = 0; j < 3; ++j) d[j] = ac[j] - c[0][j];
            l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (l > 1e-3f) for (j = 0; j < 3; ++j) h.dir[j] = d[j] / l;
            else h.dir[2] = 1.0f;
            h.id = s->id, h.push = s->push * share;
            if (!e->T) e->T = EchoSerial((EcU32 *)0x10834BC);
            h.serial = e->T;
            if (!DpUpdatesNow(t)) EchoPendAdd(t, w, &h, s->dmg * share, type);
            else EchoLand(t, w, hp, type, &h, s->dmg * share);
        }
    }
    if (best) {
        EchoFoe *fo = EchoFoeFind(&e->foes, best, bw);
        if (EchoFoeTake(fo, s->S)) {
            EchoHit h;
            const float *f = (const float *)sh[0];
            float d[3], l;
            int j;
            memset(&h, 0, sizeof h);
            memcpy(h.pos, bc, 12), h.pos[3] = 1.0f;
            for (j = 0; j < 3; ++j) d[j] = f[4 + j] - f[j];
            l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (l > 1e-3f) for (j = 0; j < 3; ++j) h.dir[j] = d[j] / l;
            h.id = s->id, h.push = s->push * share;
            if (!e->T) e->T = EchoSerial((EcU32 *)0x10834BC);
            h.serial = e->T;
            if (!DpUpdatesNow(best)) EchoPendAdd(best, bw, &h, s->dmg * share, btype);
            else EchoLand(best, bw, bhp, btype, &h, s->dmg * share);
            if (!g_echoLogged[ELG_SHOT]++) Log("echo: first twin shot (attack %02X) hits type %02X", s->id, btype);
        }
    }
}

// ---------------------------------------------------------------- placing a shadow beside her (walls, side, its turn)
static float EchoCastD(const float a[3], const float b[3])  // the free distance from a toward b (xz), b's own if clear
{
    float hit[3], nrm[3];
    const float L = sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[2] - a[2]) * (b[2] - a[2]));
    if (!SvvCastT(a, b, hit, nrm, 0.0f)) return L;
    return sqrtf((hit[0] - a[0]) * (hit[0] - a[0]) + (hit[2] - a[2]) * (hit[2] - a[2]));
}
static void EchoSideStep(const float base[3], const float right[2], float dt)  // the side all shadows take (shared)
{
    float want1 = g_echoSidePref == 2 ? -1.0f : 1.0f, eye[3];
    static float s_hold;
    if (g_echoSidePref == 0 && SvEye(eye)) {  // Auto follows the camera (over her right shoulder: the first shadow on her left)
        const float c = (eye[0] - base[0]) * right[0] + (eye[2] - base[2]) * right[1];
        if (c > 40.0f) want1 = -1.0f;
        else if (c > -40.0f) want1 = g_ec.sideWant;  // centred: keep
    }
    if (want1 != g_ec.sideWant) {
        if ((s_hold += dt) < 0.5f) return;  // a camera swing must last 0.5 s before the shadows change sides
    }
    s_hold = 0.0f;
    g_ec.sideWant = want1;
}
// its slot in her frame (kEchoSlot x the shared side x its own flip), the walls measured at most 30 times a second
static float EchoSlotFree(const float a[3], const float right[2], const float fwd[2], float lr, float lf)
{
    const float L = sqrtf(lr * lr + lf * lf);
    float b[3];
    b[0] = a[0] + right[0] * lr + fwd[0] * lf, b[1] = a[1], b[2] = a[2] + right[1] * lr + fwd[1] * lf;
    return (EchoCastD(a, b) - 25.0f) / (L > 1.0f ? L : 1.0f);  // the free fraction of the slot
}
// v3.32 ECHO332: the slot x EchoCalmDist (1.5 calm, 1 striking); dist / freeD are fractions of it (the wall rays at 30 Hz
// give the fraction: a slot growing or shrinking with the calm ease moves smoothly between them)
static void EchoShdPlace(EchoShd *e, int i, const float base[3], const float right[2], const float fwd[2], float dt, int snap)
{
    const float sc = EchoCalmDist(e->calm.k), lr = kEchoSlot[i][0] * sc, lf = kEchoSlot[i][1] * sc, L = sqrtf(lr * lr + lf * lf),
                kMin = ECHO_MIN_D / L;
    float a[3], want[3], k;
    int j;
    a[0] = base[0], a[1] = base[1] - ECHO_CHEST_DY, a[2] = base[2];
    if (e->hold > 0.0f) e->hold -= dt;
    e->wallT -= dt;
    if (e->wallT <= 0.0f || snap) {
        const float pref = e->hold > 0.0f ? e->flipWant : 1.0f, sd = g_ec.sideWant;
        float f1 = EchoSlotFree(a, right, fwd, lr * sd * pref, lf);
        e->wallT = e->wallT < -0.1f ? ECHO_WALL_DT : e->wallT + ECHO_WALL_DT;
        if (f1 >= kMin) {
            e->flipWant = pref, e->blocked = 0, e->freeD = f1 < 1.0f ? f1 : 1.0f;
        } else {  // this side is walled: the mirrored slot (the second cast)
            const float f2 = EchoSlotFree(a, right, fwd, -lr * sd * pref, lf);
            if (f2 >= kMin) {
                if (e->flipWant == pref) {
                    e->hold = 1.0f;
                    if (g_echoLogged[ELG_SWAP]++ < 3) Log("echo: a wall beside her: shadow %d takes the other side", i + 1);
                }
                e->flipWant = -pref, e->blocked = 0, e->freeD = f2 < 1.0f ? f2 : 1.0f;
            } else {
                if (!e->blocked && g_echoLogged[ELG_BOTH]++ < 3) Log("echo: walls on both sides: shadow %d fades until there is room", i + 1);
                e->blocked = 1, e->freeD = 1.0f;
            }
        }
    }
    {   // the visibility: a swap or a block fades out (0.2 s), the swap happens unseen, then it fades in
        const float effSide = g_ec.sideWant * e->flipWant;
        if (snap && !e->blocked) e->side = effSide, e->dist = e->freeD, e->vis = 1.0f;
        if (e->blocked || effSide != e->side) {
            e->vis -= dt / ECHO_FADE_S;
            if (e->vis <= 0.0f) {
                e->vis = 0.0f;
                if (!e->blocked && effSide != e->side) e->side = effSide, e->dist = e->freeD, snap = 1;
            }
        } else if (e->vis < 1.0f) {
            e->vis += dt / ECHO_FADE_S;
            if (e->vis > 1.0f) e->vis = 1.0f;
        }
    }
    k = snap ? 1.0f : EchoSmoothK(dt, ECHO_TAU);
    e->dist += (e->freeD - e->dist) * k;
    for (j = 0; j < 3; j += 2) want[j] = ((j ? right[1] : right[0]) * lr * e->side + (j ? fwd[1] : fwd[0]) * lf) * e->dist;
    want[1] = 0.0f;
    if (g_ec.mode == ECHO_COPY && e->glide > 0.0f) {  // its glide toward its target (wall-checked at the wall rate)
        if (e->wallT >= ECHO_WALL_DT - dt - 1e-6f || e->glideMax <= 0.0f) {
            float b[3], g[3];
            b[0] = a[0] + want[0], b[1] = a[1], b[2] = a[2] + want[2];
            g[0] = b[0] + e->gdir[0] * ECHO_GLIDE_D, g[1] = b[1], g[2] = b[2] + e->gdir[1] * ECHO_GLIDE_D;
            e->glideMax = EchoCastD(b, g) - 25.0f;
        }
        if (e->glide > e->glideMax) e->glide = e->glideMax > 0.0f ? e->glideMax : 0.0f;
        want[0] += e->gdir[0] * e->glide, want[2] += e->gdir[1] * e->glide;
    }
    for (j = 0; j < 3; ++j) e->off[j] += (want[j] - e->off[j]) * k;
}
// Shadow Copy's own target: at the start of each replayed window, the nearest hittable enemy within 600 of the shadow, in
// its front half, with a clear line; then its turn (75 degrees at most) and glide follow it while the window is open
static void EchoPickOwn(EchoShd *e, const float me[3], const float fwd[2])
{
    EchoCand c[32];
    const BYTE *tk[32], *t = NULL;
    int n = 0, guard = 0, i;
    while ((t = DpNext(t)) != NULL && guard++ < 96 && n < 32) {
        BYTE *w;
        float *hp, chest[3];
        int type;
        if (!EchoHittable(t, &w, &hp, &type)) continue;
        EchoAim(w, c[n].pos);
        chest[0] = me[0], chest[1] = me[1] - ECHO_CHEST_DY, chest[2] = me[2];
        c[n].ok = EchoF3(c[n].pos) && EchoClear(chest, c[n].pos);
        tk[n++] = t;
    }
    i = EchoPickTarget(c, n, me, fwd, ECHO_TGT_D);
    e->tgt = i >= 0 ? tk[i] : NULL;
    e->tgtW = i >= 0 ? *(const BYTE *const *)(tk[i] + 0x408) : NULL;
}
static void EchoOwnStep(EchoShd *e, const float me[3], const float fwd[2], int windowOpen, float dt)
{
    float want = 0.0f, glide = 0.0f;
    if (windowOpen && e->tgt) {
        BYTE *w;
        float *hp, p[3];
        int type;
        if (EchoHittable(e->tgt, &w, &hp, &type) && w == e->tgtW) {
            float d[2], l;
            EchoAim(w, p);
            d[0] = p[0] - me[0], d[1] = p[2] - me[2];
            l = sqrtf(d[0] * d[0] + d[1] * d[1]);
            if (l > 1.0f) {
                d[0] /= l, d[1] /= l;
                want = EchoClampF(EchoAngleTo(fwd, d), -ECHO_TURN_MAX, ECHO_TURN_MAX);
                e->gdir[0] = d[0], e->gdir[1] = d[1];
                glide = EchoClampF(l - 90.0f, 0.0f, ECHO_GLIDE_D);
            }
        } else {
            e->tgt = NULL;
            want = e->yaw;  // its target fell: it holds its turn to the end of the window
        }
    }
    e->yaw = EchoTurnStep(e->yaw, want, ECHO_TURN_RATE, dt);
    e->glide = EchoTurnStep(e->glide, glide, ECHO_GLIDE_V, dt);
    if (e->glide <= 0.0f) e->glideMax = 0.0f;
}

// ---------------------------------------------------------------- Shadow Cast: the enemies, the spots
static int EchoHerFloor(float *y)  // the floor under her, once a tick (only asked while engaged)
{
    if (g_ec.herFloorTick != g_ec.ticks) {
        float p[3];
        memcpy(p, (const void *)0x10A0F60, 12);
        p[1] -= 40.0f;
        g_ec.herFloorTick = g_ec.ticks;
        g_ec.herFloorOk = EchoFloorY(p, &g_ec.herFloor);
        if (!g_ec.herFloorOk) g_ec.herFloor = *(const volatile float *)(0x10A0F60 + 4), g_ec.herFloorOk = 1;  // her feet then
    }
    *y = g_ec.herFloor;
    return g_ec.herFloorOk;
}
// v3.31 ECHO331: an enemy's floor through a cache (at most one ray per enemy every ECHO_FLOOR_KEEP): Shadow Cast plans
// ask the floor under up to 16 enemies, and three shadows used to ask in the same tick
static int EchoFloorOf(const BYTE *w, const float p[3], float *y)
{
    EchoFloorC *c = NULL, *old = &g_echoFloorC[0];
    int k;
    for (k = 0; k < ECHO_FLOOR_N; ++k) {
        EchoFloorC *q = &g_echoFloorC[k];
        if (q->w == w) {
            c = q;
            break;
        }
        if (q->t < old->t) old = q;
    }
    if (c && g_ec.clock - c->t < ECHO_FLOOR_KEEP && g_ec.clock >= c->t) {
        *y = c->y;
        return c->ok;
    }
    if (!c) c = old;
    c->w = w, c->t = g_ec.clock;
    c->ok = EchoFloorY(p, &c->y);
    *y = c->y;
    return c->ok;
}
typedef struct { const BYTE *t, *w; float d, pos[3], aim[3], fl, rad; } EchoCc;
// The enemies a shadow may take (v3.30's EchoCastPlan, its first half): hittable, not down, not a boss (EchoHittable /
// DpTypeOk), within R (xz) of her, on her floor (the floors under both within EchoFloorTol of her height) and in clear
// line from her chest: the nearest ECHO_MAXN of them, nearest first. *mOut: the candidates within R.
static int EchoEligible(float R, EchoCc *v, int *mOut, float *hfOut, float *tolOut)
{
    EchoCc c[16];
    const BYTE *t = NULL;
    float her[3], chest[3], hf, tol;
    int m = 0, nv = 0, guard = 0, k;
    memcpy(her, (const void *)0x10A0F60, 12);
    EchoHerFloor(&hf);
    tol = EchoFloorTol(g_ec.herH);
    chest[0] = her[0], chest[1] = her[1] - ECHO_CHEST_DY, chest[2] = her[2];
    while ((t = DpNext(t)) != NULL && guard++ < 96) {  // the candidates, nearest first
        BYTE *w;
        float *hp, d;
        int type, q;
        EchoCc cc;
        if (!EchoHittable(t, &w, &hp, &type) || EchoIsDown(t)) continue;
        memcpy(cc.pos, w, 12);
        if (!EchoF3(cc.pos)) continue;
        d = sqrtf((cc.pos[0] - her[0]) * (cc.pos[0] - her[0]) + (cc.pos[2] - her[2]) * (cc.pos[2] - her[2]));
        if (!(d < R)) continue;
        cc.t = t, cc.w = w, cc.d = d;
        EchoAim(w, cc.aim);
        {
            const BYTE *hb = *(const BYTE *const *)(w + 0x38);  // the mover's HitBody: +0x10 its bounding radius
            const float rr = hb ? *(const float *)(hb + 0x10) : 40.0f;
            cc.rad = _finite(rr) ? EchoClampF(rr, 20.0f, 90.0f) : 40.0f;
        }
        cc.fl = 0.0f;
        if (m == 16 && d >= c[15].d) continue;
        for (q = m < 16 ? m : 15; q > 0 && c[q - 1].d > d; --q)
            if (q < 16) c[q] = c[q - 1];
        if (q < 16) c[q] = cc;
        if (m < 16) ++m;
    }
    for (k = 0; k < m && nv < ECHO_MAXN; ++k) {  // the nearest three on her floor, in clear line
        float p[3], ef;
        memcpy(p, c[k].pos, sizeof p);
        p[1] -= 40.0f;
        if (!EchoFloorOf(c[k].w, p, &ef)) continue;  // v3.30 REV-330-002 (inspection 2): no measured floor = not on her floor
        if (fabsf(ef - hf) > tol) continue;
        if (!EchoClear(chest, c[k].aim)) continue;
        c[k].fl = ef;
        v[nv++] = c[k];
    }
    if (mOut) *mOut = m;
    if (hfOut) *hfOut = hf;
    if (tolOut) *tolOut = tol;
    return nv;
}
// the shadows other than i on enemy t now (Shadow Cast: there or on the way; a busy replay: its enemy)
static int EchoOnIt(int i, const BYTE *t)
{
    int j, n = 0;
    for (j = 0; j < ECHO_MAXN; ++j) {
        const EchoShd *o = &g_es[j];
        if (j == i || !o->on) continue;
        if ((o->tp.ph == ETP_AWAY && o->ct == t) || ((o->tp.ph == ETP_OUT || o->tp.ph == ETP_IN) && o->tp.dest && o->nt == t) ||
            (o->src == ESRC_REC && o->bt == t))
            ++n;
    }
    return n;
}
// Plans shadow i's teleport: an eligible enemy (EchoEligible within ECHO_CAST_R), the one the fewest shadows are on; then
// a spot round it from a hashed side (8 tries): clear of walls from the enemy, with its own floor within 40 u of the
// enemy's. 1 = planned (nt / ntW / nspot / nso), 0 = none, -1 = not this tick (v3.31 ECHO331: one plan a tick; the
// caller asks again next tick).
static int EchoCastPlan(EchoShd *e, int i)
{
    EchoCc v[ECHO_MAXN];
    float hf, tol;
    int m = 0, nv, k, pick, onIt[ECHO_MAXN];
    if (g_ec.plans >= 1) return -1;
    ++g_ec.plans;
    nv = EchoEligible(ECHO_CAST_R, v, &m, &hf, &tol);
    if (!nv) return 0;
    for (k = 0; k < nv; ++k) onIt[k] = EchoOnIt(i, v[k].t);
    pick = EchoCastChoose(onIt, nv);
    {
        const EchoCc *q = &v[pick];
        const EcU32 seed = g_ec.seed ^ (EcU32)(ULONG_PTR)q->t;
        const float so = q->rad + ECHO_CAST_SO;
        float ec[3];
        ec[0] = q->pos[0], ec[1] = q->fl - ECHO_CHEST_DY, ec[2] = q->pos[2];
        for (k = 0; k < 8; ++k) {
            const float a = EchoSpotAngle(seed, i, g_ec.engage, k);
            float sc[3], sp[3], sf;
            sc[0] = q->pos[0] + so * sinf(a), sc[1] = ec[1], sc[2] = q->pos[2] + so * cosf(a);
            if (EchoCastD(ec, sc) < so - 12.0f || !EchoClear(ec, sc)) continue;  // a wall between it and the enemy
            sp[0] = sc[0], sp[1] = q->fl - 60.0f, sp[2] = sc[2];
            if (!EchoFloorY(sp, &sf) || fabsf(sf - q->fl) > ECHO_FLOOR_SPOT) continue;  // a ledge, a hole, another floor
            e->nt = q->t, e->ntW = q->w, e->nso = so;
            e->nspot[0] = sc[0], e->nspot[1] = sf, e->nspot[2] = sc[2];
            if (!g_echoLogged[ELG_CAST]++)
                Log("echo: Shadow Cast: shadow %d goes to type %02X %.0f u from her (floor %+.0f u of hers, tolerance %.0f; %d of %d enemies "
                    "on her floor in reach)", i + 1, *(const int *)(q->w + 0x20), q->d, q->fl - hf, tol, nv, m);
            return 1;
        }
    }
    return 0;
}
// v3.31 ECHO331: Shadow Copy during a busy moment: shadow i (at home = its place beside her) takes an eligible enemy
// within ECHO_BZ_R of her (the fewest shadows on it), and its lunge: toward the enemy, to the stand-off (its body radius
// + 70) but at most ECHO_LUNGE_D, short of a wall, onto a floor within 40 u of hers (else none: it strikes from its place,
// the reach pulls its swing in). 1 = taken (bt / btW / bzDir / bzLen / bzFace), 0 = none, -1 = not this tick.
static int EchoBzPick(EchoShd *e, int i, const float home[3])
{
    EchoCc v[ECHO_MAXN];
    float hf, tol, d[2], l, L;
    int m = 0, nv, k, pick, onIt[ECHO_MAXN];
    if (g_ec.plans >= 1) return -1;
    ++g_ec.plans;
    nv = EchoEligible(ECHO_BZ_R, v, &m, &hf, &tol);
    if (!nv) return 0;
    for (k = 0; k < nv; ++k) onIt[k] = EchoOnIt(i, v[k].t);
    pick = EchoCastChoose(onIt, nv);
    e->bt = v[pick].t, e->btW = v[pick].w;
    d[0] = v[pick].pos[0] - home[0], d[1] = v[pick].pos[2] - home[2];
    l = sqrtf(d[0] * d[0] + d[1] * d[1]);
    if (l > 1.0f) d[0] /= l, d[1] /= l;
    else d[0] = 0.0f, d[1] = 1.0f;
    e->bzDir[0] = d[0], e->bzDir[1] = d[1];
    e->bzFace = atan2f(d[0], d[1]);
    L = EchoClampF(l - (v[pick].rad + ECHO_CAST_SO), 0.0f, ECHO_LUNGE_D);
    if (L > 1.0f) {
        float a[3], b[3], sp[3], sf;
        a[0] = home[0], a[1] = home[1] - ECHO_CHEST_DY, a[2] = home[2];
        b[0] = a[0] + d[0] * L, b[1] = a[1], b[2] = a[2] + d[1] * L;
        L = EchoClampF(EchoCastD(a, b) - 25.0f, 0.0f, L);
        sp[0] = home[0] + d[0] * L, sp[1] = hf - 60.0f, sp[2] = home[2] + d[1] * L;
        if (L > 1.0f && (!EchoFloorY(sp, &sf) || fabsf(sf - hf) > ECHO_FLOOR_SPOT)) L = 0.0f;
    }
    e->bzLen = L;
    if (!g_echoLogged[ELG_BZATK]++)
        Log("echo: busy: shadow %d attacks type %02X %.0f u from her (lunge %.0f u; %d of %d enemies near on her floor)", i + 1,
            *(const int *)(v[pick].w + 0x20), v[pick].d, L, nv, m);
    return 1;
}
static int EchoCastValid(const EchoShd *e)  // its enemy still there, hittable, not down
{
    BYTE *w;
    float *hp;
    int type;
    return e->ct && EchoHittable(e->ct, &w, &hp, &type) && w == e->ctW && !EchoIsDown(e->ct);
}
static void EchoGo(int i, int dest)  // a teleport (smoke out where it is now)
{
    EchoShd *e = &g_es[i];
    if (EchoTpGo(&e->tp, dest)) EchoEvtAdd(i, 0);
}
static float EchoFaceOf(const float f[2]) { return atan2f(f[0], f[1]); }

// ---------------------------------------------------------------- v3.31 ECHO331: her busy moments, her last attack
// (the draw side's) her last attack's poses (g_echoRecPGen: the recording they belong to) and her last idle pose
typedef struct { float t, pos[3], yaw; int nb, nw; const BYTE *wm; const void *file, *skel; } EchoRecPH;
static EchoRing g_echoRecPR;
static int g_echoRecPGen = -1;
static EchoRecPH g_echoRecPH[ECHO_REC_POSE_N];
static __declspec(align(16)) float g_echoRecPB[ECHO_REC_POSE_N][ECHO_MAXB][16], g_echoRecPW[ECHO_REC_POSE_N][ECHO_MAXW][16];
typedef struct { int ok; float t, pos[3], yaw; int nb, nw; const BYTE *wm; const void *file, *skel; } EchoIdleS;
static EchoIdleS g_echoIdle;
static __declspec(align(16)) float g_echoIdleB[ECHO_MAXB][16], g_echoIdleW[ECHO_MAXW][16];
static const void *EchoHerFile(void)
{
    const BYTE *hm = EchoHerModel();
    return hm ? *(const void *const *)(hm + 0xB4) : NULL;
}
static int EchoRecOk(void)  // her last attack can be replayed (complete, poses and windows kept, the same body)
{
    return g_echoRec.state == 2 && g_echoRecPGen == g_echoRec.gen && g_echoRecPR.count >= 4 && g_echoRecHR.count > 0 &&
           g_echoRec.file && g_echoRec.file == EchoHerFile() && g_echoRec.t1 - g_echoRec.t0 >= ECHO_REC_MIN_S;
}
static int EchoIdleOk(void) { return g_echoIdle.ok && g_echoIdle.file && g_echoIdle.file == EchoHerFile(); }
// the recording (the tick: her activity and her open windows; the draw keeps the poses while state == 1)
static void EchoRecTick(const EchoTickS *s, int st, float yaw)
{
    EchoRecS *r = &g_echoRec;
    const float now = g_ec.clock;
    const void *file = EchoHerFile();
    int act = st == 3 || st == 4;  // a swing, a charged swing, a shot (pl_53b690.c); a stomp (5) is not: its targets are down
    if (!act && s->n) act = !((int(__cdecl *)(int))0x4FE0D0)(s->id);  // Playable Eileen's shot (a window alone), not a stomp id
    if (r->state == 1 && now < r->lastAct) {  // the tick clock started again (a room, a warp): it ends where it was
        r->t1 = r->lastAct;
        r->state = r->t1 - r->t0 >= ECHO_REC_MIN_S && g_echoRecHR.count > 0 ? 2 : 0;
    }
    if (act && r->state != 1) {
        r->state = 1, ++r->gen, r->t0 = r->t1 = r->lastAct = now, r->face0 = yaw, r->file = file;
        memcpy(r->root0, s->pos, sizeof r->root0);
        EchoRingInit(&g_echoRecHR, ECHO_REC_HIT_N);
    }
    if (r->state != 1) return;
    if (act) r->lastAct = now;
    if (s->n && g_echoRecHR.count < ECHO_REC_HIT_N && EchoRingDue(&g_echoRecHR, now, 1.0f / 90.0f))
        g_echoRecHS[EchoRingPush(&g_echoRecHR, now)] = *s;
    if (now - r->lastAct > ECHO_REC_END_S || now - r->t0 > ECHO_REC_MAX_S || file != r->file) {
        r->t1 = now - r->t0 > ECHO_REC_MAX_S ? r->t0 + ECHO_REC_MAX_S : now;
        r->state = r->t1 - r->t0 >= ECHO_REC_MIN_S && g_echoRecHR.count > 0 && file == r->file ? 2 : 0;
        if (r->state == 2 && !g_echoLogged[ELG_REC]++)
            Log("echo: her last attack kept for the busy moments (%.2f s, %d window sample(s), %d pose(s))", r->t1 - r->t0,
                g_echoRecHR.count, g_echoRecPGen == r->gen ? g_echoRecPR.count : 0);
    }
}
static int EchoBzValid(const EchoShd *e)  // its busy enemy still there, hittable, not down (Shadow Copy: still near her)
{
    BYTE *w;
    float *hp;
    int type;
    if (!e->bt || !EchoHittable(e->bt, &w, &hp, &type) || w != e->btW || EchoIsDown(e->bt)) return 0;
    if (g_ec.mode != ECHO_CAST) {
        const float *p = (const float *)w, *her = (const float *)0x10A0F60, dx = p[0] - her[0], dz = p[2] - her[2];
        if (dx * dx + dz * dz > 1.69f * ECHO_BZ_R * ECHO_BZ_R) return 0;
    }
    return 1;
}
// the windows of her last attack with replay times in (from, to], moved to the shadow's replay place (each repetition a
// window of its own: a new twin serial, one record per enemy), aimed at and pulled onto its enemy
static void EchoRecStrikes(EchoShd *e, int i, float from, float to)
{
    static __declspec(align(16)) EchoTickS s;
    float aim[3], M[16];
    int k, haveAim;
    // v3.31 ECHO-331-001 (inspection 2): no longer gated on the pose cross-fade srcK (a window in the first 0.1 s was
    // skipped); the replay clock itself waits while the shadow cannot strike (EchoBzTick), so no window is passed over
    if (e->sp < 0.5f || e->tp.ph == ETP_OUT || e->tp.ph == ETP_IN || e->vis < 0.5f) return;
    haveAim = EchoBzValid(e);
    if (haveAim) EchoAim(e->btW, aim);
    for (k = 0; k < g_echoRecHR.count; ++k) {
        const EchoTickS *r = &g_echoRecHS[EchoRingSlot(&g_echoRecHR, k)];
        const float rel = r->t - g_echoRec.t0;
        EchoPl pl;
        if (!(rel > from && rel <= to)) continue;
        s = *r;
        s.S = EchoHash(r->S ^ (EcU32)e->bzRep * 0x9E3779B1u ^ (EcU32)(i + 1) * 0x85EBCA6Bu) | 1u;
        pl = e->bzPl;
        pl.yaw = EchoWrapPi(EchoFaceOf(r->fwd) - e->bzFace);
        EchoPlXform(M, r->pos, &pl);
        EchoStrike(e, &s, M, haveAim ? aim : NULL, haveAim ? e->bt : NULL, haveAim ? e->btW : NULL);
    }
}
static void EchoSrcSet(EchoShd *e, int i, int src)
{
    if (src == e->src) return;
    e->srcOld = e->src, e->src = src, e->srcK = 0.0f;
    if (src == ESRC_REC) e->bzT = 0.0f, e->bzLocal = -1.0f, e->bzRep = 0, e->bzStop = 0, e->bzK = 0.0f;  // (-1: a window at 0 is struck)
    if (src == ESRC_IDLE && !g_echoLogged[ELG_BZIDLE]++)
        Log("echo: busy: shadow %d stands idle beside her (%s)", i + 1, !EchoRecOk() ? "no attack of hers kept yet" : "no enemy near on her floor");
}
// A shadow's pose source and places this tick (me: its place now: beside her, or its spot at an enemy). Busy (a candle,
// a pickup) with her last attack kept and an enemy near: it replays that attack at the enemy (Shadow Copy: a short lunge
// from its place; Shadow Cast: the enemy it was sent to); busy otherwise: her last idle pose beside her; else hers.
static void EchoBzTick(EchoShd *e, int i, const EchoTickS *b, const float me[3], float dt)
{
    const int cast = g_ec.mode == ECHO_CAST, recOk = EchoRecOk();
    // v3.32 ECHO332: during a busy moment, and after it until the shadow's replay time has passed its end (its delay), the
    // calm pose is always the idle one (v3.31: her live pose when no idle pose of her body was kept yet: her pickup);
    // without an idle pose the draw takes her pose from just before the busy moment (EchoPoseSample)
    const int hold = !EchoLiveOk(g_ec.clock - e->delay, g_ec.busy, g_ec.busyT0, g_ec.busyT1, ECHO_BZ_MARGIN);
    const int calm = hold ? ESRC_IDLE : ESRC_LIVE;
    const float len = g_echoRec.t1 - g_echoRec.t0;
    int k;
    if (e->srcK < 1.0f) e->srcK = e->srcK + dt / ECHO_SRC_FADE > 1.0f ? 1.0f : e->srcK + dt / ECHO_SRC_FADE;
    if (e->src == ESRC_REC) {
        if (!recOk) {
            EchoSrcSet(e, i, calm);  // (a body switch dropped it)
        } else {
            const float prev = e->bzLocal;
            // v3.31 ECHO-331-001: the replay clock runs only while the shadow can strike (formed, not teleporting, shown),
            // so a window is never passed while EchoRecStrikes would refuse it; once she is done it runs out anyway
            const int ready = e->sp >= 0.5f && e->tp.ph != ETP_OUT && e->tp.ph != ETP_IN && e->vis >= 0.5f;
            int rep;
            if (ready || e->bzStop || !g_ec.busy) e->bzT += dt;
            e->bzLocal = EchoRecPhase(len, e->bzT, &rep);
            if (!g_ec.busy) e->bzStop = 1;
            if (e->bzStop && rep == e->bzRep && e->bzLocal >= len) {  // she is done: no beat after its last swing
                EchoRecStrikes(e, i, prev, len + 1.0f);
                EchoSrcSet(e, i, calm);
            } else if (rep != e->bzRep) {  // a repetition ended: its last windows, then again, another enemy, or done
                EchoRecStrikes(e, i, prev, len + 1.0f);
                e->bzRep = rep;
                if (e->bzStop) {
                    EchoSrcSet(e, i, calm);
                } else if (!cast && !EchoBzValid(e)) {  // its enemy fell: the next one (a new lunge), else idle
                    const int r = EchoBzPick(e, i, me);
                    if (r > 0) {
                        e->bzK = 0.0f;
                    } else {
                        EchoSrcSet(e, i, calm);  // (r < 0: asked again next tick, below)
                        if (r == 0) e->bzCool = g_ec.clock + ECHO_CAST_COOL;
                    }
                }
                if (e->src == ESRC_REC && e->bzRep == rep) EchoRecStrikes(e, i, -1.0f, e->bzLocal);
            } else {
                EchoRecStrikes(e, i, prev, e->bzLocal);
            }
        }
    } else if (g_ec.busy && !cast) {
        int r = 0;
        if (recOk && g_ec.clock >= e->bzCool) {
            r = EchoBzPick(e, i, me);
            if (r == 0) e->bzCool = g_ec.clock + ECHO_CAST_COOL;
        }
        EchoSrcSet(e, i, r > 0 ? ESRC_REC : calm);  // (r < 0: not this tick; it stands idle meanwhile)
    } else if (g_ec.busy && cast) {
        if (e->tp.ph == ETP_AWAY && recOk && EchoCastValid(e)) {
            e->bt = e->ct, e->btW = e->ctW;
            EchoSrcSet(e, i, ESRC_REC);
        } else {  // v3.32 ECHO332: also on the way and at a fallen enemy (v3.31: home only; else her live pickup pose)
            EchoSrcSet(e, i, calm);
        }
    } else {
        EchoSrcSet(e, i, calm);  // v3.32 ECHO332: hers again once its replay time is past the busy moment
    }
    // the places: the replay (Shadow Copy: its lunge out from its place; Shadow Cast: its spot), the idle pose (beside her,
    // facing the anchor heading)
    if (e->src == ESRC_REC || e->srcOld == ESRC_REC) {
        e->bzPl.away = 1;
        memcpy(e->bzPl.anchor, g_echoRec.root0, sizeof e->bzPl.anchor);
        if (!cast) {
            float g;
            if (e->src == ESRC_REC) e->bzK = e->bzK + dt / ECHO_LUNGE_S > 1.0f ? 1.0f : e->bzK + dt / ECHO_LUNGE_S;
            g = EchoEaseOut(e->bzK) * e->bzLen;
            if (EchoBzValid(e)) {  // facing its enemy (and the lunge's direction follows it)
                float ap[3], d0, d1;
                EchoAim(e->btW, ap);
                d0 = ap[0] - me[0], d1 = ap[2] - me[2];
                if (d0 * d0 + d1 * d1 > 1.0f) e->bzFace = EchoTurnWrap(e->bzFace, atan2f(d0, d1), ECHO_CAST_TURN, dt);
            }
            e->bzPl.spot[0] = me[0] + sinf(e->bzFace) * g, e->bzPl.spot[1] = me[1], e->bzPl.spot[2] = me[2] + cosf(e->bzFace) * g;
        } else {
            memcpy(e->bzPl.spot, e->pl.spot, sizeof e->bzPl.spot);
            e->bzFace = e->cface;
        }
        e->bzPl.yaw = EchoWrapPi(g_echoRec.face0 - e->bzFace);  // (the draw and the strikes use each sample's own facing)
    }
    {   // v3.32 ECHO332: its idle place beside her (its spot now) facing the anchor heading; the pose's own place and facing
        // are applied in the draw (EchoSrcPose), which may fill the pre-busy pose in this very frame
        e->idPl.away = 1;
        for (k = 0; k < 3; ++k) e->idPl.spot[k] = me[k];
        e->idFace = EchoFaceOf(b->afwd);
    }
}

// ---------------------------------------------------------------- v3.33 ECHO333: the calm / striking log
// Why a shadow strikes this tick: ESW_* / EchoWhyOf (echo_sim.h)
static const char *const kEchoWhy[5] = { "no strike", "her attack replayed", "her last attack replayed (busy)", "at an enemy (Shadow Cast)",
                                         "teleporting to an enemy (Shadow Cast)" };
static volatile LONG g_echoCalmQ;  // the calm / striking lines this room (LogQuota; reset at a room change)
#define ECHO_CALM_Q    24
#define ECHO_SOLID_S   2.0f        // solid this long with no strike of hers behind it: an anomaly
static void EchoCalmLine(const char *fmt, int a, const char *why, double x, double y)
{
    if (LogQuota(&g_echoCalmQ, ECHO_CALM_Q)) LogV(fmt, a, why, x, y);
    else if (g_echoCalmQ == ECHO_CALM_Q + 1) LogV("echo: more than %d calm / striking lines in this room: the rest are not logged", ECHO_CALM_Q);
}
// a reason that may hold a shadow solid for ECHO_SOLID_S and more: her attacks go on (a hit window in the last 3 s, or a
// swing / stomp mode for less than 4 s: a held charge), she is busy (or was, the replay runs out its repetition), Shadow
// Cast is engaged; none while k eases back (the hold and the ramp)
static int EchoWhyOk(const EchoShd *e, int why)
{
    switch (why) {
    case ESW_SWING: return g_ec.clock - g_ec.lastWin < 3.0f || (g_ec.modeT > 0.0f && g_ec.modeT < 4.0f);
    case ESW_REPLAY: return g_ec.busy || (g_ec.busyT1 >= 0.0f && g_ec.clock - g_ec.busyT1 < ECHO_REC_MAX_S + ECHO_REC_GAP_S + 0.3f);
    case ESW_AWAY: return g_ec.engaged;
    case ESW_TELEPORT: return g_ec.engaged || e->tp.ph == ETP_OUT || e->tp.ph == ETP_IN;
    default: return g_ec.clock - e->lastWhyT < ECHO_CALM_HOLD + ECHO_CALM_OUT + 0.1f;
    }
}
static void EchoCalmLog(EchoShd *e, int i, int why, const EchoTickS *b, float dt)
{
    e->why = why;
    if (why != ESW_NONE) e->lastWhy = why, e->lastWhyT = g_ec.clock;
    if (why != ESW_NONE && !e->striking) {
        e->striking = 1, e->strikeT0 = g_ec.clock;
        EchoCalmLine("echo: shadow %d strikes (%s; her mode then %.0f, replayed %.2f s late): solid at the v3.31 distance", i + 1, kEchoWhy[why],
                     (double)b->st, (double)e->delay);
    } else if (e->striking && why == ESW_NONE && e->calm.k >= 1.0f) {
        e->striking = 0;
        EchoCalmLine("echo: shadow %d calm again (last reason: %s; %.1f s since its first strike; then %.1f x farther, see-through)", i + 1,
                     kEchoWhy[e->lastWhy], (double)(g_ec.clock - e->strikeT0), (double)ECHO_CALM_DIST);
    }
    if (e->calm.k < 0.5f) {
        e->solidT += dt;
    } else {
        e->solidT = 0.0f, e->anomSolid = 0;
    }
    if (e->solidT > ECHO_SOLID_S && !e->anomSolid && !EchoWhyOk(e, why)) {
        e->anomSolid = 1;
        LogAnomaly("echo", "shadow %d solid for %.1f s with no strike of hers behind it (reason now: %s; her mode %d for %.1f s, last hit window "
                   "%.1f s ago, busy %d, Shadow Cast engaged %d, its source %d, teleport phase %d, calm k %.2f)", i + 1, e->solidT, kEchoWhy[why],
                   *(const volatile int *)(0x10A0F20 + 0x214), g_ec.modeT, g_ec.clock - g_ec.lastWin, g_ec.busy, g_ec.engaged, e->src, e->tp.ph, e->calm.k);
    }
}
// Shadow Copy, not busy: every shadow replays the same samples (0.15-0.6 s apart), so one solid and another calm for
// ECHO_SOLID_S means a shadow's own state is stuck (Shadow Cast: each has its own enemy, so they may differ)
static void EchoDisagreeCheck(float dt)
{
    int i, solid = -1, calm = -1, at = 0;
    char buf[320];
    for (i = 0; i < ECHO_MAXN; ++i) {
        const EchoShd *e = &g_es[i];
        if (!e->on || e->leaving || e->sp < 1.0f) continue;
        if (e->calm.k <= 0.05f) solid = i;
        if (e->calm.k >= 0.95f) calm = i;
    }
    if (g_ec.mode != ECHO_COPY || g_ec.busy || (g_ec.busyT1 >= 0.0f && g_ec.clock - g_ec.busyT1 < 2.6f) || solid < 0 || calm < 0) {
        g_ec.disT = 0.0f, g_ec.anomDis = 0;
        return;
    }
    g_ec.disT += dt;
    if (g_ec.disT <= ECHO_SOLID_S || g_ec.anomDis) return;
    g_ec.anomDis = 1;
    buf[0] = 0;
    for (i = 0; i < ECHO_MAXN && at < (int)sizeof buf - 1; ++i) {
        const EchoShd *e = &g_es[i];
        int w;
        if (!e->on) continue;
        w = _snprintf_s(buf + at, sizeof buf - (size_t)at, _TRUNCATE, "%s%d: k %.2f %s, src %d, delay %.2f", at ? "; " : "", i + 1, e->calm.k,
                        e->why ? kEchoWhy[e->why] : "no strike", e->src, e->delay);
        at = w < 0 ? (int)sizeof buf - 1 : at + w;
    }
    LogAnomaly("echo", "Shadow Copy: shadow %d solid while shadow %d is calm for %.1f s (they replay the same moves 0.15-0.6 s apart): %s",
               solid + 1, calm + 1, g_ec.disT, buf);
}

// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread)
static void EchoShdTick(int i, int want, const EchoTickS *s, float dt)
{
    EchoShd *e = &g_es[i];
    const float clock = g_ec.clock;
    const EchoTickS *b;
    float tr, me[3], M[16];
    int i0, i1, k;
    float a;
    if (!want || e->leaving) {  // not wanted (the count went down): it dissolves
        if (e->on) {
            if (!e->leaving) e->leaving = 1, EchoEvtAdd(i, 0);
            e->sp -= dt / ECHO_DESPAWN_S, e->rise += dt / ECHO_DESPAWN_S;
            if (e->sp <= 0.0f) {
                const int evN = e->evN;
                EchoShdClear(e);
                e->evN = evN;
            }
        }
        if (want && !e->on) e->leaving = 0;
        return;
    }
    if (!e->on) {
        if (clock < g_ec.formT + ECHO_FORM_GAP * (float)i) return;
        EchoShdClear(e);
        e->on = 1, e->flash = 1;
        EchoDlyInit(&e->dly, EchoHash(g_ec.seed + 0x1000u * (EcU32)(i + 1)), clock, EchoDlyFirst(g_ec.seed, i, ECHO_MAXN));
        e->formedAt = clock;
        e->replayT = clock - EchoDlyAt(&e->dly, clock);  // no backlog of old windows
        e->wallT = 0.0f;
        EchoEvtAdd(i, 1);  // it forms from smoke
    }
    e->age += dt;
    if (e->sp < 1.0f) e->sp = e->sp + dt / ECHO_SPAWN_S > 1.0f ? 1.0f : e->sp + dt / ECHO_SPAWN_S;
    e->delay = EchoDlyAt(&e->dly, clock);
    tr = clock - e->delay;
    if (!EchoRingFind(&g_echoTR, tr, &i0, &i1, &a)) return;
    b = &g_echoTs[a < 0.5f ? i0 : i1];
    (void)s;
    if (g_ec.mode == ECHO_CAST) {  // the teleports
        int ev, r;
        if (e->tp.ph == ETP_HOME && g_ec.engaged && clock >= e->cool) {
            r = EchoCastPlan(e, i);
            if (r > 0) EchoGo(i, 1);
            else if (r == 0) e->cool = clock + ECHO_CAST_COOL;
        } else if (e->tp.ph == ETP_AWAY) {
            if (!g_ec.engaged) {
                EchoGo(i, 0);
                if (!g_echoLogged[ELG_HOME]++) Log("echo: Shadow Cast: she stopped attacking: the shadows return beside her");
            } else if (!EchoCastValid(e)) {  // its enemy fell (down, dead, gone): another, else home
                r = EchoCastPlan(e, i);
                if (r > 0) EchoGo(i, 1);
                else if (r == 0) EchoGo(i, 0);
            } else {
                const float *ep = (const float *)e->ctW, dx = ep[0] - e->pl.spot[0], dz = ep[2] - e->pl.spot[2];
                if (dx * dx + dz * dz > (e->so + ECHO_CAST_MOVED) * (e->so + ECHO_CAST_MOVED) && clock >= e->cool) {
                    r = EchoCastPlan(e, i);
                    if (r > 0) EchoGo(i, 1);  // it moved off: follow it
                    if (r >= 0) e->cool = clock + ECHO_CAST_COOL;
                }
            }
        } else if ((e->tp.ph == ETP_OUT || e->tp.ph == ETP_IN) && e->tp.dest && !g_ec.engaged) {
            EchoGo(i, 0);  // called back on the way
        }
        ev = EchoTpStep(&e->tp, dt);
        if (ev == 1) {  // unseen now: the new place
            if (e->tp.dest) {
                float d[2];
                e->ct = e->nt, e->ctW = e->ntW, e->so = e->nso;
                e->pl.away = 1;
                e->vis = 1.0f, e->blocked = 0;  // (no wall fade at an enemy: the spot was checked)
                memcpy(e->pl.spot, e->nspot, sizeof e->pl.spot);
                memcpy(e->pl.anchor, b->pos, sizeof e->pl.anchor);
                {
                    float ap[3];
                    EchoAim(e->ctW, ap);
                    d[0] = ap[0] - e->nspot[0], d[1] = ap[2] - e->nspot[2];
                }
                e->cface = atan2f(d[0], d[1]);
                e->tgt = e->ct, e->tgtW = e->ctW;
                if (e->src == ESRC_REC) {  // v3.31 ECHO331: a busy replay starts again at the new enemy
                    e->bt = e->ct, e->btW = e->ctW;
                    e->bzT = 0.0f, e->bzLocal = -1.0f, e->bzRep = 0, e->srcK = 1.0f;
                }
            } else {
                e->pl.away = 0;
                e->ct = e->ctW = NULL;
                e->yaw = e->glide = 0.0f;
                EchoShdPlace(e, i, b->pos, b->aright, b->afwd, dt, 1);  // v3.31 ECHO331: the anchor frame
                if (e->src == ESRC_REC) {  // v3.31 ECHO331: home unseen: no replay there (no cross-fade needed)
                    EchoSrcSet(e, i, g_ec.busy && EchoIdleOk() ? ESRC_IDLE : ESRC_LIVE);
                    e->srcK = 1.0f;
                }
            }
            e->replayT = tr;  // no strikes replayed from the dissolve
            EchoEvtAdd(i, 1);
        } else if (ev == 2) {
            e->formedAt = clock;
        }
    }
    if (!e->pl.away) {  // v3.31 ECHO331: its place in the anchor heading's frame (a spin of hers turns it in place)
        EchoShdPlace(e, i, b->pos, b->aright, b->afwd, dt, e->age <= dt * 1.5f);  // (its first tick: straight to its slot)
        for (k = 0; k < 3; ++k) me[k] = b->pos[k] + e->off[k];
    } else {
        float ap[3], d0, d1;
        if (EchoCastValid(e)) {
            EchoAim(e->ctW, ap);
            d0 = ap[0] - e->pl.spot[0], d1 = ap[2] - e->pl.spot[2];
            if (d0 * d0 + d1 * d1 > 1.0f) e->cface = EchoTurnWrap(e->cface, atan2f(d0, d1), ECHO_CAST_TURN, dt);
        }
        memcpy(me, e->pl.spot, sizeof me);
    }
    EchoBzTick(e, i, b, me, dt);  // v3.31 ECHO331: her busy moments
    // replay every tick sample that became its delay old, once and in order (EchoRingNext); none while it dissolves,
    // reforms or is walled out (no backlog either); none while it shows another pose (v3.31: a busy moment)
    while ((k = EchoRingNext(&g_echoTR, e->replayT, tr)) >= 0) {
        const EchoTickS *r = &g_echoTs[k];
        EchoPl pl;
        BYTE *w;
        float *hp, aim[3];
        int type, haveAim = 0;
        e->replayT = r->t;
        if (!r->n || e->sp < 0.5f || e->tp.ph == ETP_OUT || e->tp.ph == ETP_IN || e->vis < 0.5f || e->src != ESRC_LIVE) continue;
        if (r->S != e->rS) {  // a replayed window opens: its own target
            e->rS = r->S;
            if (!e->pl.away) EchoPickOwn(e, me, r->fwd);
            else e->tgt = e->ct, e->tgtW = e->ctW;
        }
        if (e->tgt && EchoHittable(e->tgt, &w, &hp, &type) && w == e->tgtW) EchoAim(w, aim), haveAim = 1;
        pl = e->pl;
        if (!pl.away) pl.yaw = e->yaw, memcpy(pl.off, e->off, sizeof pl.off);
        else pl.yaw = EchoWrapPi(EchoFaceOf(r->fwd) - e->cface);
        EchoPlXform(M, r->pos, &pl);
        EchoStrike(e, r, M, haveAim ? aim : NULL, e->pl.away ? e->ct : NULL, e->pl.away ? e->ctW : NULL);
    }
    if (!e->pl.away) {
        EchoOwnStep(e, me, b->fwd, b->n && b->S == e->rS && e->src == ESRC_LIVE, dt);  // its turn / glide while the replayed window is open
        e->pl.yaw = e->yaw;
        memcpy(e->pl.off, e->off, sizeof e->pl.off);
        e->faceA = EchoWrapPi(EchoFaceOf(b->fwd) - e->yaw);
    } else {
        e->pl.yaw = EchoWrapPi(EchoFaceOf(b->fwd) - e->cface);
        e->faceA = e->cface;
    }
    if (e->src == ESRC_REC) e->faceA = e->bzFace;  // v3.31 ECHO331
    {   // v3.32 ECHO332: striking: its replayed sample is a swing / charged swing / shot / stomp (modes 3 / 4 / 5) or has
        // an open window (its own pose shows it), a busy replay, a Shadow Cast teleport to an enemy or its stay there
        // v3.33 ECHO333: the same rule, its reason kept for the log (EchoCalmLog: transitions, the solid-too-long anomaly)
        const int why = EchoWhyOf(e->src == ESRC_LIVE, e->src == ESRC_REC, b->st, b->n, e->tp.ph, e->tp.dest);
        EchoCalmStep(&e->calm, why != ESW_NONE, dt);
        EchoCalmLog(e, i, why, b, dt);
    }
}
static void EchoTickC(void)
{
    const int want = (int)g_echoMode, rate = *(volatile int *)0x5D5248;
    const DWORD stage = *(volatile DWORD *)0xFD5A90, room = stage << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    BYTE *mdl;
    float pos[3], yaw, dt, fwd[2], right[2], mdx = 0.0f, mdz = 0.0f;
    int live, i, n, any, st;
    EchoTickS *s;
    if (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0) return;  // a menu, a scene, a movie: frozen
    if (rate < 1 || rate > 240 || !*(void *volatile *)0xFD5BA0) return;
    if (want <= 0 && !g_ec.on) {  // Off: nothing runs (the rings start again when it is turned on)
        if (g_echoTR.count) EchoReset(0);
        return;
    }
    dt = 1.0f / (float)rate;
    memcpy(pos, (const void *)0x10A0F60, sizeof pos);
    yaw = *(volatile float *)0x10A0FC4;
    if (!EchoF3(pos) || !_finite(yaw)) return;
    if (room != g_ec.room) {  // a new room: everything again, the shadows form 0.5 s after its gameplay starts
        g_ec.room = room;
        g_ec.seed = EchoHash((EcU32)GetTickCount() ^ room * 0x9E3779B9u);
        EchoReset(1);
        memcpy(g_ec.lastPos, pos, sizeof pos);
        g_echoTypeLog = 0;
        g_echoCalmQ = 0;  // v3.33 ECHO333: the calm / striking lines again in every room
    }
    {
        const float dx = pos[0] - g_ec.lastPos[0], dz = pos[2] - g_ec.lastPos[2];
        memcpy(g_ec.lastPos, pos, sizeof pos);
        if (dx * dx + dz * dz > ECHO_TELEPORT * ECHO_TELEPORT && g_ec.on) {
            EchoReset(1);  // a warp: they form beside her again
            memcpy(g_ec.lastPos, pos, sizeof pos);
        } else {
            mdx = dx, mdz = dz;  // v3.31 ECHO331 (her speed: the anchor heading)
        }
    }
    st = *(const volatile int *)(0x10A0F20 + 0x214);
    {   // v3.31 ECHO331: her busy moment (a Holy Candle set down, a floor pickup: EchoBusyStep); an item event may hide the
        // HUD (the gate below): during her pickup the shadows stay
        const int was = g_ec.busy;
        g_ec.busy = EchoBusyStep(&g_ec.bl, st, *(const volatile int *)0x10A19F4, *(const volatile int *)(0x10A0F20 + 0x218),
                                 *(const volatile int *)(0x10A0F20 + 0x3A8));
        if (g_ec.busy && !was) g_ec.busyT0 = g_ec.clock + dt, ++g_ec.busyGen;  // v3.32 ECHO332 (this tick's clock, below)
        if (!g_ec.busy && was) g_ec.busyT1 = g_ec.clock + dt;
        if (g_ec.busy && !was && g_ec.on && g_echoLogged[ELG_BUSY]++ < 4)
            Log("echo: busy (%s): the shadows %s", g_ec.busy == EBZ_CANDLE ? "a Holy Candle" : "a pickup",
                EchoRecOk() ? "attack an enemy near her with her last attack, else stand idle" : "stand idle beside her (no attack of hers kept yet)");
    }
    mdl = EchoHerModel();
    {   // v3.32 ECHO332: how long the HUD gate has been closed (an item event closes it a tick before the pickup mode)
        if (*(volatile BYTE *)0x1079C30 != 0 || g_ec.busy) g_ec.gateT = 0.0f;
        else g_ec.gateT += dt;
    }
    live = want > 0 && want < ECHO_MODES && stage != 1 && stage != 0x0A &&  // not Room 302 (no body), not the crawl
           (*(volatile BYTE *)0x1079C30 != 0 || g_ec.busy ||                // the HUD gate (events, pickups; v3.31: not her own)
            (g_ec.on && g_ec.gateT <= ECHO_GATE_S)) &&                       // v3.32 ECHO332: closed for 0.12 s first
           *(volatile float *)(0x10A0F20 + 0x08) > 0.0f &&                     // she is alive
           mdl && !(*(const WORD *)(mdl + 0xE4) & 4);                           // her body is drawn
    if (g_ec.on && want != g_ec.mode) live = 0;  // another behaviour: these dissolve, the new ones form
    g_ec.clock += dt;
    ++g_ec.ticks;
    for (i = 0; i < ECHO_MAXN; ++i) g_es[i].foes.clock = g_ec.clock;
    fwd[0] = sinf(yaw), fwd[1] = cosf(yaw);  // KB: her facing in xz = (sin yaw, cos yaw), yaw = [P+0xA4]
    EchoRightOf(yaw, g_ec.rsign ? g_ec.rsign : 1.0f, right);
    s = &g_echoTs[EchoRingPush(&g_echoTR, g_ec.clock)];
    s->t = g_ec.clock;
    memcpy(s->pos, pos, sizeof pos);
    memcpy(s->fwd, fwd, sizeof fwd);
    memcpy(s->right, right, sizeof right);
    s->st = st;  // v3.32 ECHO332
    EchoSampleAttack(s, g_ec.clock);
    if (s->n) g_ec.lastWin = g_ec.clock;  // v3.33 ECHO333: (the solid-too-long anomaly's evidence of her attacks)
    g_ec.modeT = st == 3 || st == 4 || st == 5 ? g_ec.modeT + dt : 0.0f;
    {   // her attack activity (Shadow Cast): an open window, a swing / charged swing / shot (states 3 / 4, pl_53b690.c
        // PlayerSwingStart; a stomp, state 5, alone does not send them: its targets are down, which they skip)
        if (s->n || st == 3 || st == 4) g_ec.lastAtk = g_ec.clock;
    }
    if (want > 0) EchoRecTick(s, st, yaw);  // v3.31 ECHO331: her last attack (kept also before the shadows form)
    {   // v3.31 ECHO331: the anchor heading (holds while she attacks or is busy) and its frame; her idle time
        const int hold = g_ec.busy || g_ec.clock - g_ec.lastAtk < 0.4f || st == 3 || st == 4 || st == 5;
        const float H = EchoAncStep(&g_ec.anc, yaw, mdx, mdz, dt, hold);
        s->afwd[0] = sinf(H), s->afwd[1] = cosf(H);
        EchoRightOf(H, g_ec.rsign ? g_ec.rsign : 1.0f, s->aright);
        if (st == 0 && !g_ec.busy && g_ec.anc.vs < ECHO_IDLE_V && g_ec.clock - g_ec.lastAtk > 0.5f) g_ec.idleT += dt;
        else g_ec.idleT = 0.0f;
        g_ec.plans = 0;
    }
    if ((g_ec.clock - g_ec.herHT > 0.5f || g_ec.herH <= 0.0f) && mdl) {  // her height (the same-floor tolerance), twice a second
        const float *m = *(const float *const *)(mdl + 0xC0);
        g_ec.herHT = g_ec.clock;
        if (m && !((ULONG_PTR)m & 15)) {
            const float h = 0.5f * (m[16 * 7 + 13] + m[16 * 4 + 13]) - m[16 * 11 + 13];  // feet minus head (Y down)
            // v3.33 ECHO333: up to 420 (v3.32: 260; the preview's idle bodies measure 280 (Playable Eileen) / 292 (Henry) from
            // the foot bones to the head bone, which v3.32 rejected); the measured height logged once per body (the effects
            // skill's Henry / Eileen sizes) and out-of-range values as an anomaly (a broken skeleton, a wrong bone)
            static const void *s_hFile;
            static volatile LONG s_hBad;
            if (_finite(h) && h > 80.0f && h < 420.0f) {
                g_ec.herH = g_ec.herH > 0.0f ? g_ec.herH + (h + 12.0f - g_ec.herH) * 0.5f : h + 12.0f;
                if (s_hFile != EchoHerFile() && g_es[0].on) {
                    s_hFile = EchoHerFile();
                    LogV("echo: her body %p: feet to head bone %.0f u (the sprite sizes x %.2f, a 160 u figure = 1; same-floor tolerance %.0f u)",
                         s_hFile, h, EchoClampF(h, 120.0f, 420.0f) / 160.0f, EchoFloorTol(g_ec.herH));
                }
            } else if ((!_finite(h) || h >= 420.0f) && LogQuota(&s_hBad, 3)) {  // (under 80: lying down, knocked down: normal)
                LogAnomaly("echo", "her height from the bones is %.1f u (feet bones 7 / 4 to head bone 11; expected 80-420): kept %.0f u", h, g_ec.herH);
            }
        }
    }
    if (!live) {
        any = 0;
        for (i = 0; i < ECHO_MAXN; ++i) {  // they dissolve into smoke (rising a little)
            EchoShd *e = &g_es[i];
            if (!e->on) continue;
            if (!e->leaving) {
                e->leaving = 1;
                EchoEvtAdd(i, 0);
                if (!i) Log("echo: the shadows dissolve (%s)", want <= 0 ? "Off" : want != g_ec.mode ? "behaviour change" : "event, death or hidden body");
            }
            e->sp -= dt / ECHO_DESPAWN_S, e->rise += dt / ECHO_DESPAWN_S;
            if (e->sp <= 0.0f) {
                const int evN = e->evN;
                EchoShdClear(e);
                e->evN = evN;
            } else {
                any = 1;
            }
        }
        if (!any && g_ec.on) {
            g_ec.on = 0;
            g_ec.wait = g_ec.wait > 0.25f ? g_ec.wait : 0.25f;  // back on: a short beat first
        }
        return;
    }
    if (!g_ec.on) {
        if (g_ec.wait > 0.0f) {
            g_ec.wait -= dt;
            return;
        }
        g_ec.on = 1, g_ec.mode = want, g_ec.formT = g_ec.clock;
        for (i = 0; i < ECHO_MAXN; ++i) EchoShdClear(&g_es[i]);
        Log("echo: %d %s shadow(s) form beside her (side %s, %.0f %% damage each, delays 0.15-0.6 s, re-rolled every 2-6 s)",
            EchoCount(), want == ECHO_COPY ? "Shadow Copy" : "Shadow Cast", g_echoSidePref == 0 ? "auto" : g_echoSidePref == 1 ? "right" : "left",
            100.0f * EchoShareFor(g_echoShare, EchoCount()));
    }
    n = EchoCount();
    if (n != g_ec.n) {
        if (g_ec.n) Log("echo: %d shadow(s) now (was %d)", n, g_ec.n);
        g_ec.n = n;
    }
    {   // Shadow Cast's engagement: her attack activity within ECHO_CAST_HOLD (v3.31 ECHO331: or a busy moment of hers with
        // her last attack kept: the shadows go and replay it)
        int eng;
        if (g_ec.busy && EchoRecOk()) g_ec.lastBusy = g_ec.clock;
        eng = g_ec.mode == ECHO_CAST && (g_ec.clock - g_ec.lastAtk < ECHO_CAST_HOLD || (g_ec.lastBusy > 0.0f && g_ec.clock - g_ec.lastBusy < ECHO_CAST_HOLD));
        if (eng && !g_ec.engaged) ++g_ec.engage;
        g_ec.engaged = eng;
    }
    EchoSideStep(pos, s->aright, dt);  // v3.31 ECHO331: the anchor frame
    for (i = 0; i < ECHO_MAXN; ++i) EchoShdTick(i, i < n, s, dt);
    EchoDisagreeCheck(dt);  // v3.33 ECHO333
    if (g_logLevel >= 3 && g_ec.ticks % (rate > 0 ? rate : 60) == 0)  // v3.33 ECHO333: trace: each shadow once a second
        LogT("echo: k %.2f / %.2f / %.2f, reason %d / %d / %d, source %d / %d / %d, teleport %d / %d / %d, her mode %d, busy %d, engaged %d",
             g_es[0].calm.k, g_es[1].calm.k, g_es[2].calm.k, g_es[0].why, g_es[1].why, g_es[2].why, g_es[0].src, g_es[1].src, g_es[2].src,
             g_es[0].tp.ph, g_es[1].tp.ph, g_es[2].tp.ph, st, g_ec.busy, g_ec.engaged);
    if (g_es[0].on && g_es[0].calm.k <= 0.0f && !g_echoLogged[ELG_CALM]++)  // v3.32 ECHO332
        Log("echo: calm vs striking: a calm shadow stands %.1f x farther at %.0f %% opacity; striking (shadow 1 now) it is solid at the "
            "v3.31 distance", ECHO_CALM_DIST, 100.0f * ECHO_CALM_OPA);
    EchoPendStep(dt);
}
static float g_echoTickMs;  // v3.31 ECHO331: the tick's slowest this room (the draw's room line)
static void EchoTick(void)
{
    LARGE_INTEGER t0, t1, f;
    if (!g_echoInstalled || g_echoFail) return;
    QueryPerformanceCounter(&t0);
    __try {
        EchoTickC();
    } __except (EchoFaultNote(GetExceptionInformation())) {
        EchoFailNow("the tick");
    }
    QueryPerformanceCounter(&t1);
    if (g_ec.on && QueryPerformanceFrequency(&f) && f.QuadPart) {
        const float ms = (float)((double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart);
        if (ms > g_echoTickMs) g_echoTickMs = ms;
    }
}

// ---------------------------------------------------------------- the draw side: her pose, the shadows' matrices
static LONG g_echoHerFrame = -1, g_echoVpStamp = -1;
static Viewport8 g_echoVp;
static int g_echoVpOk, g_echoWepSeen;
static void EchoCollect(const BYTE *task, BYTE *model)  // 0x52AEB5 (ThermDrawC), every registry model draw
{
    (void)task;
    if (!g_echoInstalled || g_echoFail || (!g_ec.on && g_echoMode <= 0) || !model) return;
    if (g_echoVpStamp != g_fxFrame) {  // the first body of a frame: the 3D viewport in force
        void *dev = DEV;
        g_echoVpStamp = g_fxFrame;
        g_echoVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_echoVp)) && g_echoVp.w >= 16 && g_echoVp.h >= 16;
    }
    if (model == EchoHerModel()) g_echoHerFrame = g_fxFrame;
    else if (model == *(BYTE *const volatile *)(0x10A0F20 + 0x340)) g_echoWepSeen |= 1;
}
typedef struct { float t, pos[3]; int nb, nw; const BYTE *wm; const void *file, *skel; float yaw; } EchoPoseH;  // v3.30 REV330-001: + the body's file / skeleton; v3.32 ECHO332: + her facing
static EchoRing g_echoPR;
static EchoPoseH g_echoPH[ECHO_POSE_N];
static __declspec(align(16)) float g_echoPB[ECHO_POSE_N][ECHO_MAXB][16], g_echoPW[ECHO_POSE_N][ECHO_MAXW][16];
static __declspec(align(16)) float g_echoBody[ECHO_MAXN][ECHO_MAXB][16], g_echoWep[ECHO_MAXN][ECHO_MAXW][16],
    g_echoRimM[ECHO_MAXN][ECHO_MAXB][16], g_echoTmpB[ECHO_MAXB][16], g_echoTmpW[ECHO_MAXW][16],
    g_echoOldB[ECHO_MAXB][16], g_echoOldW[ECHO_MAXW][16];  // v3.31 ECHO331: the pose source a shadow cross-fades from
typedef struct {           // one shadow this frame
    int ok, bonesOk, nb, nw;  // ok: drawn; bonesOk: its bones are this frame's (its smoke can be placed)
    BYTE *body, *wep;
    float alpha, rimA, b[EB_N][3], vel[3], lastPel[3], lastT, floorY;
    int velOk;
    float head[3][3];      // v3.31 ECHO331: its head bone's axes (unit rows 0-2 of bone 11): the eyes
    float calm;            // v3.32 ECHO332: its calm k this frame (the sprites; the body's alpha has it already)
    float sprVis;          // v3.32 ECHO332: the sprites' visibility (alpha without the calm opacity)
    int held;              // v3.32 ECHO332: the last frame's body matrices are still in g_echoBody (a pose to hold)
    const void *heldFile;  // ... of this model file
    // v3.33 ECHO333: its body plan this frame (EchoBodyPlan): the opacity a reader sees, its prepass, the look, the frame;
    // how long it has been calm but drawn opaque (the anomaly), that anomaly said this room
    float op, opBadT;
    int depth, look, planF, anomOp;
} EchoDrawS;
static EchoDrawS g_ed[ECHO_MAXN];
static int g_echoPoseGen = -1;
static int g_echoDrawN;
static LONG g_echoPostFrame = -1, g_echoPostDone = -1;
static int g_echoPostLook;
static DWORD g_echoPostBroken[4], g_echoVisit = 1, g_echoDrawRoom = 0xFFFFFFFFu;
static float g_echoMsMax;
static float g_echoMsBody, g_echoMsSpr;  // v3.31 ECHO331: the slowest body passes / sprites this room (the room line)
static int g_echoMorphParts;              // v3.31 ECHO331: the face morph parts a redraw keeps (the log)
static int g_echoDrawFaults, g_echoSprMax;
static EchoBud g_echoBud;
// v3.35 FX335: the effect budget (budget.inc). With it on, Echo's level is the governor's (BgEchoLevel: it comes back when
// there is room; v3.31's per-room EchoBudStep only with [Graphics] EffectBudget=0), a shadow beyond BgEchoBodies keeps its
// flame coat, eyes and strikes while its body fades into the smoke (g_echoGovK, 0.4 s), the ground discs fade (g_echoDiscK)
static float g_echoGovK[ECHO_MAXN] = { 1.0f, 1.0f, 1.0f }, g_echoDiscK = 1.0f;
static LONG g_echoGovStamp = -1;
static int EchoBudLv(void) { return g_bgOn ? BgEchoLevel(BgLevel()) : g_echoBud.level; }
static void EchoGovStep(void)  // once a frame (the scene pass or the post-filter one)
{
    int i;
    if (g_echoGovStamp == g_bgFrameNo) return;
    g_echoGovStamp = g_bgFrameNo;
    if (!g_bgOn) {
        for (i = 0; i < ECHO_MAXN; ++i) g_echoGovK[i] = 1.0f;
        g_echoDiscK = g_echoBud.level < 3 ? 1.0f : 0.0f;
        return;
    }
    for (i = 0; i < ECHO_MAXN; ++i) g_echoGovK[i] = BgRamp(g_echoGovK[i], i < BgEchoBodies(BgLevel()) ? 1.0f : 0.0f, g_bgDt, 2.5f);
    g_echoDiscK = BgRamp(g_echoDiscK, EchoBudLv() < 3 ? 1.0f : 0.0f, g_bgDt, 2.5f);
}
static void *g_echoWarmDev;
static int g_echoWarm;    // the override's warm mode: every pixel fails the alpha test (nothing is drawn)

// her weapon model: Henry's items at P+0x340 (0x544DF0 parents them to bone 0x19; pistol, revolver, every melee item);
// Playable Eileen's own items: the weapon object [0xFCE5B0] (its model at work +0) when its owner is the player's
// stand-in (re_echo 4)
static BYTE *EchoPeWeapon(void)  // Playable Eileen's own weapon model when its owner is the player's stand-in (shown or not)
{
    const BYTE *obj = *(const BYTE *const volatile *)0xFCE5B0;
    if (obj && TaskSlotOk(obj) &&  // v3.36 TP336
        *(const DWORD *)(obj + 0x404) == 0x4456C0 && *(const DWORD *)(obj + 0x428) == 0x443AC0) {
        BYTE *w = *(BYTE *const *)(obj + 0x408);
        const BYTE *owner = w ? *(const BYTE *const *)(w + 0xF0) : NULL;
        if (owner && *(const DWORD *)(owner + 0x18) == 0x10A0F20 + 0xC0) return w;
    }
    return NULL;
}
// v3.31 DEPTH-331-001 (inspection 2): depthfix.inc's collector uses the same validated lookup
static BYTE *DfxPeWeapon(void) { return EchoPeWeapon(); }
static BYTE *EchoWeaponModel(void)
{
    BYTE *wm = *(BYTE *const volatile *)(0x10A0F20 + 0x340), *pw;
    if (wm && !(*(const WORD *)(wm + 0xE4) & 4)) return wm;
    pw = EchoPeWeapon();
    if (pw && !(*(const WORD *)(pw + 0xE4) & 4)) return pw;
    return NULL;
}
// v3.31 ECHO331: a weapon model kept with a pose (her last attack, her idle pose) that is still hers now, shown or hidden
// (her hand is busy with a candle or a pickup: the shadow's replay keeps its weapon; drawn with the hide bit off)
static int EchoWepStill(const BYTE *w) { return w && (w == *(BYTE *const volatile *)(0x10A0F20 + 0x340) || w == EchoPeWeapon()); }
static int EchoBones(const BYTE *m, const float **mats)  // its matrices and their count (0: not drawable)
{
    const BYTE *sk;
    int n;
    if (!m || ((ULONG_PTR)m & 15)) return 0;
    *mats = *(const float *const *)(m + 0xC0);
    sk = *(const BYTE *const *)(m + 0xE0);
    if (!*mats || ((ULONG_PTR)*mats & 15) || !sk) return 0;
    n = *(const int *)(sk + 0x14);
    if (n < 1 || n > 128) return 0;
    {   // v3.30 REV330-001: the palettes may index up to the model file's count (After Shock's AS330 rule): copy those too
        int hdr = -1;
        __try {
            const BYTE *file = *(const BYTE *const *)(m + 0xB4);
            if (file) hdr = *(const int *)(file + 0x0C);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hdr = -1;
        }
        if (hdr > n) n = hdr <= 128 ? hdr : 129;  // above 128: the callers' bounds fail closed
    }
    return n;
}
// v3.32 ECHO332: the idle pose from her pose ring at time t (blended as the live replay; her own body only): 1 = taken
static int g_echoSnapGen = -1;
static int EchoIdleFromRing(float t, const BYTE *body, int nb)
{
    const void *const f = *(const void *const *)(body + 0xB4), *const sk = *(const void *const *)(body + 0xE0);
    EchoIdleS *h = &g_echoIdle;
    int i0, i1, b, k;
    float a;
    if (g_echoPoseGen != g_ec.gen || !EchoRingFind(&g_echoPR, t, &i0, &i1, &a)) return 0;
    if (g_echoPH[i0].file != f || g_echoPH[i1].file != f || g_echoPH[i0].skel != sk || g_echoPH[i1].skel != sk ||
        g_echoPH[i0].nb != nb || g_echoPH[i1].nb != nb)
        return 0;
    for (b = 0; b < nb; ++b) EchoMatBlend(g_echoIdleB[b], g_echoPB[i0][b], g_echoPB[i1][b], a);
    h->nw = g_echoPH[i0].nw == g_echoPH[i1].nw && g_echoPH[i0].wm == g_echoPH[i1].wm ? g_echoPH[i0].nw : 0;
    h->wm = h->nw ? g_echoPH[i0].wm : NULL;
    for (b = 0; b < h->nw; ++b) EchoMatBlend(g_echoIdleW[b], g_echoPW[i0][b], g_echoPW[i1][b], a);
    for (k = 0; k < 3; ++k) h->pos[k] = g_echoPH[i0].pos[k] + (g_echoPH[i1].pos[k] - g_echoPH[i0].pos[k]) * a;
    h->yaw = EchoWrapPi(g_echoPH[i0].yaw + EchoWrapPi(g_echoPH[i1].yaw - g_echoPH[i0].yaw) * a);
    h->nb = nb, h->file = f, h->skel = sk, h->t = g_ec.clock, h->ok = 1;
    if (!g_echoLogged[ELG_SNAP]++)
        Log("echo: busy before any idle pose of her body was kept: the shadows take her pose from %.2f s before it (never the pickup / "
            "candle pose)", ECHO_BZ_SNAP);
    return 1;
}
// her pose into the ring (stamped with the tick clock, at most 60 a second), also before the shadows form
static void EchoPoseSample(void)
{
    BYTE *body = EchoHerModel(), *wm;
    const float *bm = NULL, *wmats = NULL;
    int nb, nw = 0;
    if (g_echoHerFrame != g_fxFrame || (g_echoMode <= 0 && !g_ec.on)) return;
    if (g_echoPoseGen != g_ec.gen) {  // the tick reset (a room, a warp, a behaviour): a fresh pose ring
        g_echoPoseGen = g_ec.gen;
        EchoRingInit(&g_echoPR, ECHO_POSE_N);
    }
    nb = EchoBones(body, &bm);
    if (nb < 18 || nb > ECHO_MAXB) return;
    wm = EchoWeaponModel();
    if (wm && ((nw = EchoBones(wm, &wmats)) == 0 || nw > ECHO_MAXW)) wm = NULL, nw = 0;
    if (EchoRingDue(&g_echoPR, g_ec.clock, 1.0f / 60.0f) && g_ec.clock > EchoRingNewest(&g_echoPR) + 1e-6f) {
        const int i = EchoRingPush(&g_echoPR, g_ec.clock);
        EchoPoseH *h = &g_echoPH[i];
        h->t = g_ec.clock, h->nb = nb, h->nw = nw, h->wm = wm;
        h->file = *(const void *const *)(body + 0xB4), h->skel = *(const void *const *)(body + 0xE0);  // v3.30 REV330-001
        h->yaw = *(volatile float *)0x10A0FC4;  // v3.32 ECHO332
        memcpy(h->pos, (const void *)0x10A0F60, sizeof h->pos);
        memcpy(g_echoPB[i], bm, (size_t)nb * 64);
        if (nw) memcpy(g_echoPW[i], wmats, (size_t)nw * 64);
    }
    // v3.31 ECHO331: her last attack's poses while the tick records it (ECHO_REC_POSE_HZ, at most ECHO_REC_POSE_N), and her
    // idle pose (10 a second after 0.3 s standing): what the shadows show during a busy moment
    if (g_echoRec.state == 1) {
        if (g_echoRecPGen != g_echoRec.gen) {
            g_echoRecPGen = g_echoRec.gen;
            EchoRingInit(&g_echoRecPR, ECHO_REC_POSE_N);
        }
        if (g_echoRecPR.count < ECHO_REC_POSE_N && EchoRingDue(&g_echoRecPR, g_ec.clock, 1.0f / ECHO_REC_POSE_HZ) &&
            g_ec.clock > EchoRingNewest(&g_echoRecPR) + 1e-6f) {
            const int i = EchoRingPush(&g_echoRecPR, g_ec.clock);
            EchoRecPH *h = &g_echoRecPH[i];
            h->t = g_ec.clock, h->nb = nb, h->nw = nw, h->wm = wm, h->yaw = *(volatile float *)0x10A0FC4;
            h->file = *(const void *const *)(body + 0xB4), h->skel = *(const void *const *)(body + 0xE0);
            memcpy(h->pos, (const void *)0x10A0F60, sizeof h->pos);
            memcpy(g_echoRecPB[i], bm, (size_t)nb * 64);
            if (nw) memcpy(g_echoRecPW[i], wmats, (size_t)nw * 64);
        }
    }
    if (g_ec.busy && g_echoSnapGen != g_ec.busyGen) {  // v3.32 ECHO332: a busy moment began: no idle pose of this body kept
        // yet (the row was turned on, her body switched): her pose from just before it, out of the pose ring, is the idle one
        g_echoSnapGen = g_ec.busyGen;
        if (!EchoIdleOk()) EchoIdleFromRing(g_ec.busyT0 - ECHO_BZ_SNAP, body, nb);
    }
    if (g_ec.idleT >= 0.3f && (!g_echoIdle.ok || g_ec.clock - g_echoIdle.t >= 0.1f || g_ec.clock < g_echoIdle.t)) {
        EchoIdleS *h = &g_echoIdle;
        h->ok = 1, h->t = g_ec.clock, h->nb = nb, h->nw = nw, h->wm = wm, h->yaw = *(volatile float *)0x10A0FC4;
        h->file = *(const void *const *)(body + 0xB4), h->skel = *(const void *const *)(body + 0xE0);
        memcpy(h->pos, (const void *)0x10A0F60, sizeof h->pos);
        memcpy(g_echoIdleB, bm, (size_t)nb * 64);
        if (nw) memcpy(g_echoIdleW, wmats, (size_t)nw * 64);
    }
}
static void EchoRSide(const float *bm, float yaw)  // her right: the sign of (cos yaw, -sin yaw) from her shoulders (12 right, 17 left)
{
    const float *r = bm + 16 * 12 + 12, *l = bm + 16 * 17 + 12;
    const float c = (r[0] - l[0]) * cosf(yaw) - (r[2] - l[2]) * sinf(yaw);
    if (fabsf(c) > 8.0f) g_ec.rvote = (int)EchoClampF((float)(g_ec.rvote + (c > 0.0f ? 1 : -1)), -20.0f, 20.0f);
    if (abs(g_ec.rvote) == 20 && !g_echoLogged[ELG_RSIDE]++)
        Log("echo: the player's right side from the shoulders: right = %s(cos yaw, -sin yaw)", g_ec.rvote < 0 ? "-" : "");
    g_ec.rsign = g_ec.rvote < 0 ? -1.0f : 1.0f;
}
// v3.31 ECHO331: one pose source of shadow i this frame, placed: its world body (and weapon) matrices into ob / ow (*nwo
// weapon matrices), its transform D and the source root (her place then); 0 = not available this frame.
//   ESRC_LIVE: her pose its delay ago (the pose ring) at its place (e->pl: beside her, or its spot at an enemy);
//   ESRC_REC:  her last attack at its replay time (e->bzLocal), at its replay place (e->bzPl), facing its enemy (each
//              pose's own facing turned onto e->bzFace);
//   ESRC_IDLE: her last idle pose beside her (e->idPl), facing the anchor heading.
static int EchoSrcPose(int i, int src, int nb, BYTE *wm, int nw, float (*ob)[16], float (*ow)[16], int *nwo, float D[16],
                       float rootOut[3], BYTE **wmo)
{
    const EchoShd *e = &g_es[i];
    const BYTE *const hm = EchoHerModel();
    const void *const f = hm ? *(const void *const *)(hm + 0xB4) : NULL, *const sk = hm ? *(const void *const *)(hm + 0xE0) : NULL;
    EchoPl pl;
    float root[3], a = 0.0f, t[16];
    int i0, i1, b, k, nwu = 0;
    if (!hm) return 0;
    if (src == ESRC_REC) {
        const EchoRecPH *h0, *h1;
        if (g_echoRecPGen != g_echoRec.gen || !EchoRingFind(&g_echoRecPR, g_echoRec.t0 + e->bzLocal, &i0, &i1, &a)) return 0;
        h0 = &g_echoRecPH[i0], h1 = &g_echoRecPH[i1];
        if (h0->file != f || h1->file != f || h0->skel != sk || h1->skel != sk || h0->nb != nb || h1->nb != nb) return 0;
        for (k = 0; k < 3; ++k) root[k] = h0->pos[k] + (h1->pos[k] - h0->pos[k]) * a;
        for (b = 0; b < nb; ++b) EchoMatBlend(ob[b], g_echoRecPB[i0][b], g_echoRecPB[i1][b], a);
        {   // its weapon: the one kept with it, while still hers (shown or not)
            const float *wmats = NULL;
            wm = (BYTE *)h0->wm;
            nwu = wm && h1->wm == wm && h0->nw == h1->nw && h0->nw <= ECHO_MAXW && EchoWepStill(wm) && EchoBones(wm, &wmats) == h0->nw ? h0->nw : 0;
        }
        for (b = 0; b < nwu; ++b) EchoMatBlend(ow[b], g_echoRecPW[i0][b], g_echoRecPW[i1][b], a);
        pl = e->bzPl;
        pl.yaw = EchoWrapPi(h0->yaw + EchoWrapPi(h1->yaw - h0->yaw) * a - e->bzFace);
    } else if (src == ESRC_IDLE) {
        const EchoIdleS *h = &g_echoIdle;
        if (!h->ok || h->file != f || h->skel != sk || h->nb != nb) return 0;
        memcpy(root, h->pos, sizeof root);
        memcpy(ob, g_echoIdleB, (size_t)nb * 64);
        {
            const float *wmats = NULL;
            wm = (BYTE *)h->wm;
            nwu = wm && h->nw <= ECHO_MAXW && EchoWepStill(wm) && EchoBones(wm, &wmats) == h->nw ? h->nw : 0;
        }
        if (nwu) memcpy(ow, g_echoIdleW, (size_t)nwu * 64);
        pl = e->idPl;  // v3.32 ECHO332: the idle pose's own place and facing (it may have been taken this frame)
        memcpy(pl.anchor, h->pos, sizeof pl.anchor);
        pl.yaw = EchoWrapPi(h->yaw - e->idFace);
    } else {
        const float tr = g_ec.clock - e->delay;
        // v3.32 ECHO332: never her pose inside a busy moment (a shadow still dissolving, a source missing this frame): its
        // pose from just before it while the ring still has it, else not available (EchoPrepare holds the last frame's)
        const float tu = g_ec.busy && tr >= g_ec.busyT0 - ECHO_BZ_MARGIN ? g_ec.busyT0 - ECHO_BZ_MARGIN : tr;
        if (!EchoLiveOk(tu, g_ec.busy, g_ec.busyT0, g_ec.busyT1, ECHO_BZ_MARGIN) || (tu != tr && g_echoPR.count && tu < EchoRingOldest(&g_echoPR) - 1e-4f))
            return 0;
        if (g_echoPoseGen != g_ec.gen || !EchoRingFind(&g_echoPR, tu, &i0, &i1, &a)) return 0;
        // v3.30 REV330-001: a body switch keeps the inline model object 0x10A0FE0 and may keep the bone count: compare the
        // model file and skeleton too
        if (g_echoPH[i0].file != f || g_echoPH[i1].file != f || g_echoPH[i0].skel != sk || g_echoPH[i1].skel != sk ||
            g_echoPH[i0].nb != nb || g_echoPH[i1].nb != nb) {  // her model changed (Playable Eileen's live switch): again
            EchoRingInit(&g_echoPR, ECHO_POSE_N);
            return 0;
        }
        for (k = 0; k < 3; ++k) root[k] = g_echoPH[i0].pos[k] + (g_echoPH[i1].pos[k] - g_echoPH[i0].pos[k]) * a;
        for (b = 0; b < nb; ++b) EchoMatBlend(ob[b], g_echoPB[i0][b], g_echoPB[i1][b], a);
        nwu = g_echoPH[i0].nw == g_echoPH[i1].nw && g_echoPH[i0].wm == wm && g_echoPH[i1].wm == wm ? nw : 0;
        for (b = 0; b < nwu; ++b) EchoMatBlend(ow[b], g_echoPW[i0][b], g_echoPW[i1][b], a);
        pl = e->pl;
    }
    EchoPlXform(D, root, &pl);
    for (b = 0; b < nb; ++b) EchoMul(t, ob[b], D), memcpy(ob[b], t, sizeof t);
    for (b = 0; b < nwu; ++b) EchoMul(t, ow[b], D), memcpy(ow[b], t, sizeof t);
    *nwo = nwu;
    *wmo = nwu ? wm : NULL;
    memcpy(rootOut, root, sizeof root);
    return 1;
}
// shadow i's matrices this frame (its pose source placed: EchoSrcPose; v3.31 cross-faded from the one before; the
// dissolve's swell / the forming's growth), its alpha, its key bones and its head's axes; 0 = nothing to draw
static int EchoPrepare(int i, const float *bmNow, int nb, BYTE *wm, int nw, float frameDt)
{
    EchoShd *e = &g_es[i];
    EchoDrawS *d = &g_ed[i];
    float D[16], D2[16], root[3], root2[3], eye[3], vis, tpv;
    BYTE *wu = NULL, *wo = NULL;
    int b, k, src = e->src, nwo = 0, nwl = nw;
    d->ok = d->bonesOk = 0;
    if (!e->on || e->sp <= 0.0f) return (d->held = 0);
    (void)bmNow;
    if (!EchoSrcPose(i, src, nb, wm, nwl, g_echoBody[i], g_echoWep[i], &nw, D, root, &wu) &&
        (src == ESRC_LIVE || !EchoSrcPose(i, src = ESRC_LIVE, nb, wm, nwl, g_echoBody[i], g_echoWep[i], &nw, D, root, &wu))) {
        // v3.32 ECHO332: no pose this frame (its source is missing and her live pose is a busy one): the last frame's held
        // (its matrices are still in g_echoBody; no weapon), else nothing drawn
        if (!d->held || d->heldFile != EchoHerFile() || d->body != EchoHerModel() || d->nb != nb) return (d->held = 0);
        if (!g_echoLogged[ELG_FREEZE]++) Log("echo: shadow %d holds its last pose a moment (no pose but her busy one this frame)", i + 1);
        d->nw = 0, d->wep = NULL;
        d->calm = e->calm.k;
        d->sprVis = FxSat(e->sp) * FxSat(e->vis) * EchoTpVis(&e->tp);
        d->alpha = d->sprVis * EchoCalmOp(e->calm.k, ECHO_CALM_OPA);
        d->bonesOk = 1;
        d->ok = d->alpha > 0.004f;
        return d->ok;
    }
    if (e->srcK < 1.0f && e->srcOld != src && EchoSrcPose(i, e->srcOld, nb, wm, nwl, g_echoOldB, g_echoOldW, &nwo, D2, root2, &wo)) {
        const float kk = EchoSmooth(0.0f, 1.0f, e->srcK);  // from the old source to the new one
        float t[16];
        for (b = 0; b < nb; ++b) EchoMatMix(t, g_echoOldB[b], g_echoBody[i][b], kk), memcpy(g_echoBody[i][b], t, sizeof t);
        if (nwo == nw && wo == wu)
            for (b = 0; b < nw; ++b) EchoMatMix(t, g_echoOldW[b], g_echoWep[i][b], kk), memcpy(g_echoWep[i][b], t, sizeof t);
        if (kk < 0.5f) memcpy(D, D2, sizeof D), memcpy(root, root2, sizeof root);
    }
    tpv = EchoTpVis(&e->tp);
    {   // the dissolve swells (x1.08) and rises 40 u; the forming grows from x0.88 about its pelvis
        const float sw = e->leaving ? 1.0f + 0.08f * EchoSat(e->rise) : e->tp.ph == ETP_OUT ? 1.0f + 0.08f * (1.0f - tpv)
                       : e->tp.ph == ETP_IN ? 0.88f + 0.12f * tpv : e->sp < 1.0f ? 0.88f + 0.12f * EchoEaseOut(e->sp) : 1.0f;
        const float rise = e->leaving ? 40.0f * EchoSat(e->rise) : 0.0f;
        if (sw != 1.0f || rise != 0.0f) {
            float pel[3], S[16], t[16];
            memcpy(pel, &g_echoBody[i][8][12], sizeof pel);  // (bone 8: the pelvis of the shadow)
            EchoIdent(S);
            S[0] = S[5] = S[10] = sw;
            for (k = 0; k < 3; ++k) S[12 + k] = pel[k] * (1.0f - sw);
            S[13] -= rise;
            for (b = 0; b < nb; ++b) EchoMul(t, g_echoBody[i][b], S), memcpy(g_echoBody[i][b], t, sizeof t);
            for (b = 0; b < nw; ++b) EchoMul(t, g_echoWep[i][b], S), memcpy(g_echoWep[i][b], t, sizeof t);
        }
    }
    d->nb = nb, d->nw = nw, d->body = EchoHerModel(), d->wep = nw ? wu : NULL;
    for (k = 0; k < EB_N; ++k) memcpy(d->b[k], g_echoBody[i][kEchoBone[k]] + 12, 12);
    for (k = 0; k < 3; ++k) {  // v3.31 ECHO331: its head's axes this frame (the eyes follow every turn and nod)
        const float *r = g_echoBody[i][11] + 4 * k, l = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        int j;
        for (j = 0; j < 3; ++j) d->head[k][j] = l > 1e-4f && _finite(l) ? r[j] / l : 0.0f;
    }
    d->bonesOk = 1;
    {
        float r2[3];
        EchoPoint(D, root, r2);
        d->floorY = r2[1];
    }
    {   // its velocity (the flame leans back from it): the pelvis between frames, smoothed 0.15 s; a jump (a teleport) resets
        const float *p = d->b[EB_PELVIS], dtc = g_ec.clock - d->lastT;
        if (d->velOk && dtc > 1e-4f && dtc < 0.25f) {
            float v[3], kk = 1.0f - expf(-dtc / 0.15f);
            int ok = 1;
            for (k = 0; k < 3; ++k) v[k] = (p[k] - d->lastPel[k]) / dtc, ok &= fabsf(v[k]) < 900.0f;
            if (ok) for (k = 0; k < 3; ++k) d->vel[k] += (v[k] - d->vel[k]) * kk;
            else memset(d->vel, 0, sizeof d->vel);
        } else if (!d->velOk) {
            memset(d->vel, 0, sizeof d->vel);
        }
        if (dtc > 1e-4f || !d->velOk) memcpy(d->lastPel, p, 12), d->lastT = g_ec.clock;
        d->velOk = 1;
    }
    (void)frameDt;
    vis = FxSat(e->sp) * FxSat(e->vis) * tpv;
    d->calm = e->calm.k;  // v3.32 ECHO332
    d->held = 1, d->heldFile = EchoHerFile();
    {   // the near-camera fade (the over-the-shoulder camera sits close)
        const float *ch = d->b[EB_CHEST];
        float nearK = 1.0f;
        if (SvEye(eye)) {
            const float dx = ch[0] - eye[0], dy = ch[1] - eye[1], dz = ch[2] - eye[2];
            nearK = FxSmooth(ECHO_NEAR0, ECHO_NEAR1, sqrtf(dx * dx + dy * dy + dz * dz));
        }
        d->sprVis = vis * nearK;                                         // v3.32 ECHO332: the sprites' (they apply the calm
        d->alpha = vis * nearK * EchoCalmOp(e->calm.k, ECHO_CALM_OPA);  // their own way); the body x the calm opacity
        d->rimA = vis * nearK * (e->age < 0.3f ? 1.0f + 2.0f * (1.0f - e->age / 0.3f) : 1.0f);
    }
    if (g_echoRim)
        for (b = 0; b < nb; ++b) {  // the edge shell: each bone's 3 x 3 part x ECHO_RIM_K about its own origin
            memcpy(g_echoRimM[i][b], g_echoBody[i][b], 64);
            for (k = 0; k < 12; ++k) if ((k & 3) != 3) g_echoRimM[i][b][k] *= ECHO_RIM_K;
        }
    d->ok = d->alpha > 0.004f;
    return d->ok;
}

// ---------------------------------------------------------------- the draw-call override's Echo mode
enum { EP_DEPTH, EP_INK, EP_GLOW, EP_THERM, EP_RIM, EP_SOLID, EP_SPECTER };  // v3.34 FX334: + EP_SPECTER (Spirit Vision)
static int g_echoPass;
static float g_echoC[6][4];  // the bound shader's constants
static DWORD g_echoPs;
static DWORD g_echoRimWrote, g_echoRimGame;
static DWORD g_echoCullSeen[4];
static void SetRS(void *dev, DWORD s, DWORD v) { ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, s, v); }
static void __cdecl EchoOverride(void *dev)  // at each of the renderer's draw calls while our redraw runs
{
    const int p = g_echoPass;
    DWORD cull = 0;
    ((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 22 /*CULLMODE*/, &cull);
    SetRS(dev, 7 /*ZENABLE*/, 1);
    SetRS(dev, 52 /*STENCILENABLE*/, 0);
    SetRS(dev, 15 /*ALPHATESTENABLE*/, 1);  // cut-out hair keeps its shape; a fading shadow is not cut by the game's ref
    SetRS(dev, 25 /*ALPHAFUNC*/, 5 /*GREATER*/);
    SetRS(dev, 24 /*ALPHAREF*/, g_echoWarm ? 255 : 1);  // the warm-up: every pixel fails (the pipeline is made, nothing drawn)
    SetRS(dev, 171 /*BLENDOP*/, 1 /*ADD*/);
    if (p == EP_DEPTH) {  // the depth of its nearest surface only: then the far arm hides behind the torso
        SetRS(dev, 23 /*ZFUNC*/, 7 /*GREATEREQUAL*/);
        SetRS(dev, 14 /*ZWRITEENABLE*/, 1);
        SetRS(dev, 168 /*COLORWRITEENABLE*/, 0);
        SetRS(dev, 27 /*ALPHABLENDENABLE*/, 0);
    } else if (p == EP_SOLID) {  // the steady body: opaque near-black, its own depth (one pass)
        SetRS(dev, 23, 7);
        SetRS(dev, 14, 1);
        SetRS(dev, 168, 7);
        SetRS(dev, 27, 0);
    } else if (p == EP_RIM) {  // the inflated shell's back faces only: a thin rim outside the silhouette
        DWORD game = cull;
        if (cull != g_echoRimWrote || !g_echoRimGame) g_echoRimGame = cull;  // a new material set it; else ours stands
        else game = g_echoRimGame;
        if (game < 4) ++g_echoCullSeen[game];
        SetRS(dev, 23 /*ZFUNC*/, 7);
        SetRS(dev, 14, 0);
        SetRS(dev, 27, 1);
        SetRS(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
        SetRS(dev, 20 /*DESTBLEND*/, 2 /*ONE*/);
        SetRS(dev, 28 /*FOGENABLE*/, 0);
        if (game == 2 || game == 3) {
            g_echoRimWrote = game == 2 ? 3 : 2;
            SetRS(dev, 22, g_echoRimWrote);
            SetRS(dev, 168, 7);
        } else {
            SetRS(dev, 168, 0);  // a two-sided material (hair cards): no rim from it
        }
    } else {
        SetRS(dev, 23 /*ZFUNC*/, g_echoInkEq ? 3 /*EQUAL*/ : 7 /*GREATEREQUAL*/);
        SetRS(dev, 14, 0);
        SetRS(dev, 168, 7);
        SetRS(dev, 27, 1);
        if (p == EP_GLOW) {
            SetRS(dev, 19, 2), SetRS(dev, 20, 2);
            SetRS(dev, 28, 0);
        } else {
            SetRS(dev, 19, 5 /*SRCALPHA*/), SetRS(dev, 20, 6 /*INVSRCALPHA*/);
            if (p == EP_THERM || p == EP_SPECTER) SetRS(dev, 28, 0);  // (the ink keeps the game's fog: it fades into the room's fog;
                                                                      // v3.34 FX334: after Spirit Vision's filter, no fog)
        }
    }
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_echoPs);
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, g_echoC, 6);
}
// kPsHeat as an ink shader: V = luma x detail + base; rgb = sat(4 V c3) = V x tint (c3 = tint / 4, starts 0); a = t0.a x alpha
static void EchoInkC(const float tint[3], float base, float detail, float alpha)
{
    int j;
    memset(g_echoC, 0, sizeof g_echoC);
    g_echoC[0][0] = 0.299f * detail, g_echoC[0][1] = 0.587f * detail, g_echoC[0][2] = 0.114f * detail;
    g_echoC[1][0] = g_echoC[1][1] = g_echoC[1][2] = base;
    for (j = 0; j < 3; ++j) g_echoC[3][j] = 0.25f * tint[j], g_echoC[4][j] = 1.0f;
    g_echoC[5][3] = FxSat(alpha);
}
// kPsGvGlow: rgb = sat(2 t0 c0 + c1), a = t0.a (c0.a 0.5)
static void EchoGlowC(const float col[3], float tex, float floor, float k)
{
    int j;
    memset(g_echoC, 0, sizeof g_echoC);
    for (j = 0; j < 3; ++j) g_echoC[0][j] = FxSat(col[j] * tex * 0.5f * k), g_echoC[1][j] = FxSat(col[j] * floor * k);
    g_echoC[0][3] = 0.5f;
}
// v3.31 ECHO331: the face morph of her body (decomp sgmodel_427250.c / sgdraw_428170.c): every model draw rebuilds all
// morph points (0x427660: the file's +0x44 points, ~3 500 on Playable Eileen's subdivided heads) and, for each part with
// a morph (part +0x14 count), LOCKS that part's vertex buffer with no flags (RenderBufferLock(vb, 0, 0, .., 0), a managed
// buffer the GPU may still be reading for the game's own draw of this frame) and writes the positions again. A redraw of
// the same body in the same frame needs none of it (the same blink: the game's draw filled the buffer this frame), so
// for that one call the parts' morph counts and the file's point count are 0: no rebuild, no lock, no second upload.
// Part lists: file +0x20 / +0x24 and +0x28 / +0x2C (count, offset; 0x429590), each part +0 its size. Restored at once.
#define ECHO_MORPH_N 64
typedef struct { int *at; int v; } EchoMorphSv;
static int EchoMorphHold(BYTE *m, EchoMorphSv *sv, int max)  // the entries saved (and zeroed); 0 none
{
    BYTE *const file = *(BYTE *const *)(m + 0xB4);
    int n = 0, list, k;
    if (!g_echoMorphKeep || !file || *(const DWORD *)file != 0xFFFF0003u || *(const int *)(file + 0x44) <= 0) return 0;
    for (list = 0; list < 2; ++list) {
        const int cnt = *(const int *)(file + (list ? 0x28 : 0x20)), off = *(const int *)(file + (list ? 0x2C : 0x24));
        BYTE *part = file + off;
        if (cnt <= 0 || cnt > 1024 || off <= 0) continue;
        for (k = 0; k < cnt; ++k) {
            const int size = *(const int *)part, mc = *(const int *)(part + 0x14);
            if (mc > 0 && mc < 4096) {
                if (n >= max) return 0;  // (more than kept: as v3.30, nothing held)
                sv[n].at = (int *)(part + 0x14), sv[n].v = mc, ++n;
            }
            if (size <= 0 || size > 0x400000) break;
            part += size;
        }
    }
    if (!n) return 0;
    sv[n].at = (int *)(file + 0x44), sv[n].v = *(const int *)(file + 0x44), ++n;
    for (k = 0; k < n; ++k) *sv[k].at = 0;
    if (!g_echoLogged[ELG_MORPH]++)
        Log("echo: a redraw keeps her face morph of this frame (%d morph part(s), %d points): no second lock / upload per shadow",
            n - 1, sv[n - 1].v);
    g_echoMorphParts = n - 1;
    return n;
}
// v3.31 MERGE: depthfix.inc's depth redraws use the same hold (DfxMorphSv has EchoMorphSv's layout)
static int DfxMorphHold(BYTE *m, DfxMorphSv *sv, int max) { return EchoMorphHold(m, (EchoMorphSv *)sv, max); }
static int EchoDrawOne(BYTE *m, float (*mats)[16])  // her model drawn again with our matrices (re_special C3)
{
    void *const mtx0 = *(void **)(m + 0xC0), *const sk0 = *(void **)(m + 0xE0);
    EchoMorphSv sv[ECHO_MORPH_N + 1];
    int ok = 1, nsv = 0;
    *(void **)(m + 0xC0) = mats;
    *(void **)(m + 0xE0) = NULL;  // no rebuild (and so no second step of Playable Eileen's springs at 0x55C794)
    __try {
        nsv = EchoMorphHold(m, sv, ECHO_MORPH_N);  // v3.31 ECHO331
        ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(m);
    } __except (EchoFaultNote(GetExceptionInformation())) {
        ok = 0;
    }
    while (nsv > 0) --nsv, *sv[nsv].at = sv[nsv].v;  // v3.31 ECHO331: the morph counts back
    *(void **)(m + 0xC0) = mtx0;  // both back at once
    *(void **)(m + 0xE0) = sk0;
    return ok;
}
// one body pass of shadow i (and its weapon unless noWep): constants set by the caller
static int EchoPassOne(int i, int pass, int noWep)
{
    EchoDrawS *d = &g_ed[i];
    int ok;
    g_echoPass = pass;
    InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, (void *)EchoOverride);
    ok = EchoDrawOne(d->body, pass == EP_RIM ? g_echoRimM[i] : g_echoBody[i]);
    if (ok && d->wep && pass != EP_RIM && !noWep) {
        WORD *const fl = (WORD *)(d->wep + 0xE4);  // v3.31 ECHO331: a kept weapon her hand hides now: drawn for this call
        const WORD f0 = *fl;
        *fl = (WORD)(f0 & ~4);
        ok = EchoDrawOne(d->wep, g_echoWep[i]);
        *fl = f0;
    }
    InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
    return ok;
}
// The bodies (look 0 normal, 1 Thermal, 2 night vision, 3 Spirit Vision); the device states are the caller's (FxReapply)
static int EchoBodies(void *dev, int look, int *passes)
{
    const int ci = g_echoColour & 3;
    int i, ok = 1;
    if (!FxShader(dev, FXT_HEAT) || (g_echoRim && !FxShader(dev, FXT_GVGLOW))) {  // v3.34 FX334: Spirit Vision needs only the ink shader
        if (!g_echoLogged[ELG_SHADER]++) Log("echo: its shaders could not be made: no shadow drawn");
        return 0;
    }
    EchoGovStep();  // v3.35 FX335
    for (i = 0; i < ECHO_MAXN && ok; ++i) {
        EchoDrawS *d = &g_ed[i];
        // v3.33 ECHO333: the passes from EchoBodyPlan (echo_sim.h). v3.32's rule dropped the prepass of the 2nd / 3rd shadow at
        // budget level 2 also while calm: their 45 % bodies blended every surface of her model and read solid ("1 shadow
        // stays transparent while the other two stay solid"); a body see-through for longer than a fade keeps it now. The
        // alpha is the look's: the normal look calm .. solid, a vision look 35 .. 50 % at all times (EchoBodyOp).
        const float a = d->sprVis * EchoBodyOp(look, d->calm) * g_echoGovK[i];  // v3.35 FX335: x the budget's body fade
        EchoBodyPl pl;
        int noWep, depth;
        if (!d->ok) continue;
        if (g_echoGovK[i] < 0.02f) {  // v3.35 FX335: thinned by the budget: its body has faded into its smoke (no pass)
            d->op = 0.0f, d->depth = 0, d->look = look, d->planF = g_fxFrame;
            continue;
        }
        EchoBodyPlan(&pl, look, EchoBudLv(), i, a, d->calm, g_echoDepth);
        noWep = pl.noWep, depth = pl.depth;
        d->op = pl.op, d->depth = depth, d->look = look, d->planF = g_fxFrame;  // (EchoOpCheck: the anomaly)
        g_echoInkEq = depth;  // v3.31 ECHO331 (the colour passes test against the prepass only when there is one)
        if (look == 0 || look == 2) {
            if (pl.solid) {  // steady: one opaque pass (its own depth)
                g_echoPs = g_fxPs[FXT_HEAT];
                EchoInkC(kEchoInk[ci], ci == 3 ? 0.03f : 0.012f, ci == 3 ? 0.12f : 0.07f, 1.0f);
                ok = EchoPassOne(i, EP_SOLID, noWep), ++*passes;
            } else {  // fading: the depth prepass (nearest surface), then the colour there, blended
                g_echoPs = g_fxPs[FXT_HEAT];
                EchoInkC(kEchoInk[ci], 0.012f, 0.07f, a);
                if (depth) ok = EchoPassOne(i, EP_DEPTH, noWep), ++*passes;
                if (ok) ok = EchoPassOne(i, EP_INK, noWep), ++*passes;
            }
            if (ok && g_echoRim && EchoBudLv() < 2 && !(EchoBudLv() >= 1 && i > 0)) {  // the edge shell
                float rc[3];
                int j;
                for (j = 0; j < 3; ++j) rc[j] = kEchoLight[ci][j] * 0.55f;
                g_echoPs = g_fxPs[FXT_GVGLOW];
                g_echoRimWrote = g_echoRimGame = 0;
                EchoGlowC(rc, 0.0f, 1.0f, d->rimA * FxPulse() * (look == 2 ? 0.5f : 1.0f));
                ok = EchoPassOne(i, EP_RIM, 1), ++*passes;
            }
        } else {
            g_echoPs = g_fxPs[FXT_HEAT];
            EchoInkC(kEchoInk[ci], 0.05f, 0.25f, a);
            if (depth) ok = EchoPassOne(i, EP_DEPTH, noWep), ++*passes;
            if (!ok) break;
            if (look == 3) {  // Spirit Vision: v3.34 FX334 a dark specter: near-black cold ink, its texture detail mostly gone,
                // alpha-blended over its prepass (v3.33: a pale spectral additive glow, read like her own pale body there)
                static const float kSpecter[3] = { 0.55f, 0.62f, 1.0f };  // (x (0.03 + 0.05 luma): 0.02 .. 0.08, cold black)
                g_echoPs = g_fxPs[FXT_HEAT];
                EchoInkC(kSpecter, 0.03f, 0.05f, a);
                ok = EchoPassOne(i, EP_SPECTER, noWep), ++*passes;
            } else {  // Thermal: cold (a shadow has no heat): v3.33 ECHO333 a see-through cold body (deep indigo), colder than the room
                static const float kCold[3] = { 0.15f, 0.25f, 1.0f };  // (deep indigo: colder than the navy floor)
                g_echoPs = g_fxPs[FXT_HEAT];
                EchoInkC(kCold, 0.05f, 0.10f, a);
                ok = EchoPassOne(i, EP_THERM, noWep), ++*passes;
            }
        }
    }
    return ok;
}

// ---------------------------------------------------------------- the sprites (Death Presence's sprite list and its draw)
static float g_echoSprT;  // (the clock the list was built at)
static void EchoFlame(const EchoSpr *s)  // a flame tongue: its foot 20 % up the quad at p, its tip at tip (DpTrFlame's way)
{
    const float foot = 0.20f;
    float hb[4], ht[4], xb, yb, xt, yt, ex, ey, el, wl, c[3], hw = s->r, p[3], tp[3];
    int k;
    memcpy(p, s->p, sizeof p), memcpy(tp, s->tip, sizeof tp);
    if (s->push > 0.0f && g_svEyeOk) DpAway(s->p, s->push, p), DpAway(s->tip, s->push, tp);
    GvXform(p, hb), GvXform(tp, ht);
    if (!GvScreen(hb, g_svVp, &xb, &yb) || !GvScreen(ht, g_svVp, &xt, &yt)) return;
    ex = (xt - xb) / g_svPxX * hb[3], ey = (yt - yb) / g_svPxY * hb[3];
    el = sqrtf(ex * ex + ey * ey);
    wl = sqrtf((tp[0] - p[0]) * (tp[0] - p[0]) + (tp[1] - p[1]) * (tp[1] - p[1]) + (tp[2] - p[2]) * (tp[2] - p[2]));
    if (!(el > 0.05f * wl) || !_finite(el)) ex = 0.0f, ey = -1.0f, el = 1.0f;  // seen along its axis: upright
    ex /= el, ey /= el;
    if (el < 0.45f * wl) el = 0.45f * wl;
    for (k = 0; k < 3; ++k) c[k] = p[k] + (tp[k] - p[k]) * ((0.5f - foot) / (1.0f - foot));
    g_dpStretchK = el / (1.0f - foot) * 0.5f / hw;
    if (g_dpStretchK < 1.0f) g_dpStretchK = 1.0f;
    g_dpStretchD[0] = ex, g_dpStretchD[1] = ey;
    g_dpFloorY = s->floor, g_dpCeilH = 1.0e9f;  // v3.31 ECHO331: the big flame fades out on its floor (0..35 u) in its own plane,
    DpSlAdd(c, hw, atan2f(ex, -ey), DPT_FLAME, s->cell % 28, s->add, s->rgb, s->a, 0.0f,  // its own depth (DPF_CEIL, top off)
            (s->pad & 1) && g_dpBasisOk ? DPF_FLOOR | DPF_CEIL : 0, 2, 0.0f);
    g_dpStretchK = 1.0f;
}
// v3.32 ECHO332: a piece of a wisp (ESK_WISP): FXX_SMOKE's soft cell centred half way from p to tip and stretched along it
// (soft ends, no tip: a strand of smoke), its own depth, no floor clip
static void EchoStreak(const EchoSpr *s)
{
    float hb[4], ht[4], xb, yb, xt, yt, ex, ey, el, c[3], p[3], tp[3];
    int k;
    memcpy(p, s->p, sizeof p), memcpy(tp, s->tip, sizeof tp);
    if (s->push > 0.0f && g_svEyeOk) DpAway(s->p, s->push, p), DpAway(s->tip, s->push, tp);
    GvXform(p, hb), GvXform(tp, ht);
    if (!GvScreen(hb, g_svVp, &xb, &yb) || !GvScreen(ht, g_svVp, &xt, &yt)) return;
    ex = (xt - xb) / g_svPxX * hb[3], ey = (yt - yb) / g_svPxY * hb[3];  // (world units at its depth)
    el = sqrtf(ex * ex + ey * ey);
    if (!(el > 1e-3f) || !_finite(el)) ex = 0.0f, ey = -1.0f, el = 0.0f;
    else ex /= el, ey /= el;
    for (k = 0; k < 3; ++k) c[k] = 0.5f * (p[k] + tp[k]);
    g_dpStretchK = (0.5f * el + 0.5f * s->r) / s->r;
    if (g_dpStretchK < 1.0f || !_finite(g_dpStretchK)) g_dpStretchK = 1.0f;
    g_dpStretchD[0] = ex, g_dpStretchD[1] = ey;
    DpSlAdd(c, s->r, atan2f(ex, -ey), DPT_SMOKE, DPS_SOFT, s->add, s->rgb, s->a, 0.0f, 0, 2, 0.0f);
    g_dpStretchK = 1.0f;
}
static void EchoGround(void *dev, const EchoSpr *s, void *tex)  // a flat soft dark disc on its floor (FXX_SMOKE's soft cell)
{
    SpVtx v[4], t6[6];
    const float u0 = 0.25f * (float)(DPS_SOFT & 3) + 0.5f / 1024.0f, v0 = 0.5f * (float)(DPS_SOFT >> 2) + 0.5f / 512.0f;
    const float u1 = u0 + 0.25f - 1.0f / 1024.0f, v1 = v0 + 0.5f - 1.0f / 512.0f;
    const float a = FxSat(s->a);
    const DWORD col = (DWORD)(a * 255.0f + 0.5f) << 24;  // premultiplied black
    int k;
    for (k = 0; k < 4; ++k) {
        float p[3], h[4], x, y;
        p[0] = s->p[0] + ((k & 1) ? s->r : -s->r), p[1] = s->p[1], p[2] = s->p[2] + ((k & 2) ? s->r : -s->r);
        GvXform(p, h);
        if (!(h[3] > GV_NEAR_W) || !GvScreen(h, g_svVp, &x, &y)) return;  // (near the camera: none; it is a soft shadow)
        v[k].x = x - 0.5f, v[k].y = y - 0.5f, v[k].z = SvZH(h), v[k].rhw = 1.0f / h[3], v[k].c = col;
        v[k].u = (k & 1) ? u1 : u0, v[k].v = (k & 2) ? v1 : v0;
    }
    t6[0] = v[0], t6[1] = v[1], t6[2] = v[2], t6[3] = v[1], t6[4] = v[3], t6[5] = v[2];
    DpSlStates(dev, tex, 0);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, 2, t6, sizeof(SpVtx));
}
static int GtSmokeList(int look, EchoSpr *o, int n, int max);  // v3.43 GT343: teleport.inc (Ghost Teleport's smoke)
static int GtSmokeLive(void);  // v3.43 GT343
static int EchoSpriteList(int look, EchoSpr *o, int max)  // every shadow's list this frame
{
    int i, n = 0;
    g_echoSprT = g_ec.clock;
    for (i = 0; i < ECHO_MAXN; ++i) {
        EchoShd *e = &g_es[i];
        EchoDrawS *d = &g_ed[i];
        EchoEvt ev[ECHO_EVT_N];
        EchoLookIn in;
        int k, ne = 0;
        for (k = 0; k < ECHO_EVT_N; ++k) {  // its smoke events: the bones of its place when it was made
            EchoEvtS *s = &g_echoEv[i][k];
            const float age = g_ec.clock - s->e.t0;
            if (!(s->e.t0 > 0.0f) || age < 0.0f || age > ECHO_PUFF_OUT + 0.25f) continue;  // v3.32 ECHO332: + the decoration's fade-in
            if (!s->filled) {
                if (!d->bonesOk) {
                    if (age > 0.1f) s->e.t0 = 0.0f;  // never placed in time: dropped
                    continue;
                }
                memcpy(s->e.b, d->b, sizeof s->e.b);
                s->filled = 1;
            }
            ev[ne++] = s->e;
        }
        if (!d->bonesOk && !ne) continue;  // (a gone shadow's last smoke: drawn alone, vis 0)
        memset(&in, 0, sizeof in);
        memcpy(in.b, d->b, sizeof in.b);
        in.face[0] = sinf(e->faceA), in.face[1] = cosf(e->faceA);
        memcpy(in.vel, d->vel, sizeof in.vel);
        memcpy(in.light, kEchoLight[g_echoColour & 3], sizeof in.light);
        in.t = g_ec.clock, in.vis = d->ok ? d->sprVis : 0.0f, in.look = look, in.idx = i, in.floorY = d->floorY;  // v3.32: sprVis
        in.flare = e->formedAt > 0.0f ? 1.0f - EchoSat((g_ec.clock - e->formedAt) / 0.4f) : 0.0f;
        in.lite = (EchoBudLv() >= 2 && i > 0) || EchoBudLv() >= 3;  // v3.31 ECHO331; v3.35 FX335: the budget's level
        if (d->bonesOk) memcpy(in.head, d->head, sizeof in.head);           // v3.31 ECHO331: the eyes on the head bone
        in.calm = d->bonesOk ? d->calm : 0.0f;                              // v3.32 ECHO332
        n = EchoLookBuild(&in, ev, ne, o, n, max);
    }
    n = GtSmokeList(look, o, n, max);  // v3.43 GT343: Ghost Teleport's smoke, after the shadows'
    return n;
}
// the sprites of every shadow: the ground discs (ground 1) or the rest (ground 0), both after the bodies. The caller
// set FxBase / SvZ (the visible part, no z write) and this frame's matrix, projection and eye.
static int EchoSprites(void *dev, const Viewport8 *vp, int look, int ground, const EchoSpr *o, int n)
{
    const int saveLook = g_dpLook, saveDrawn = g_dpSLDrawn, saveBas = g_dpBasisOk;
    const float saveA = g_dpAlphaK, saveS = g_dpStretchK, saveD0 = g_dpStretchD[0], saveD1 = g_dpStretchD[1], sN0 = g_dpNear0, sN1 = g_dpNear1;
    const float saveFl = g_dpFloorY, saveCe = g_dpCeilH;  // v3.31 ECHO331: Death Presence's floor clip, borrowed for the flames
    float saveRW[3], saveDW[3];
    int i, drawn = 0;
    g_svVp = vp;
    if (!SvSprScale(vp)) return 0;
    g_svUnit = (float)vp->h / 720.0f;
    if (ground) {
        void *tex = FxTexture(dev, FXX_SMOKE);
        if (!tex) return 0;
        for (i = 0; i < n; ++i)
            if (o[i].kind == ESK_GROUND) {
                EchoSpr g = o[i];
                g.a *= g_echoDiscK;  // v3.35 FX335: the budget's fade (1 without it, or below its level 3)
                EchoGround(dev, &g, tex), ++drawn;
            }
        return drawn;
    }
    g_dpSLN = 0;
    g_dpAlphaK = 1.0f, g_dpStretchK = 1.0f;
    g_dpNear0 = 40.0f, g_dpNear1 = 140.0f;
    g_dpLook = look == 3 ? 3 : 0;  // (DpSlAdd: no Thermal here)
    memcpy(saveRW, g_dpRW, sizeof saveRW), memcpy(saveDW, g_dpDW, sizeof saveDW);
    for (i = 0; i < n; ++i)  // v3.31 ECHO331: the camera's world right / down at a flame (the floor clip's corner heights)
        if (o[i].kind == ESK_FLAME) {
            DpBasis(vp, o[i].p);
            break;
        }
    for (i = 0; i < n; ++i) {
        const EchoSpr *s = &o[i];
        float p[3];
        if (s->kind == ESK_GROUND) continue;
        if (s->kind == ESK_FLAME) {
            EchoFlame(s);
            continue;
        }
        if (s->kind == ESK_WISP) {  // v3.32 ECHO332
            EchoStreak(s);
            continue;
        }
        memcpy(p, s->p, sizeof p);
        if (s->push > 0.0f && g_svEyeOk) DpAway(s->p, s->push, p);
        if (s->kind == ESK_ORB) DpSlAdd(p, s->r, 0.0f, DPT_ATLAS, SVC_ORB, s->add, s->rgb, s->a, 1.3f * g_svUnit, 0, 2, 0.0f);
        else DpSlAdd(p, s->r, 0.37f * (float)i, DPT_SMOKE, s->kind == ESK_SOFT ? DPS_SOFT : s->kind == ESK_SMOKE0 ? DPS_SMOKE0 :
                     s->kind == ESK_SMOKE1 ? DPS_SMOKE1 : DPS_CREST0, s->add, s->rgb, s->a, 0.0f, 0, 2, 0.0f);
    }
    drawn = g_dpSLN;
    DpSlDraw(dev);
    g_dpLook = saveLook, g_dpSLDrawn = saveDrawn, g_dpAlphaK = saveA, g_dpStretchK = saveS;  // (Death Presence's frame state)
    g_dpStretchD[0] = saveD0, g_dpStretchD[1] = saveD1, g_dpNear0 = sN0, g_dpNear1 = sN1;
    g_dpFloorY = saveFl, g_dpCeilH = saveCe, g_dpBasisOk = saveBas;  // v3.31 ECHO331
    memcpy(g_dpRW, saveRW, sizeof saveRW), memcpy(g_dpDW, saveDW, sizeof saveDW);
    return drawn;
}

// v3.33 ECHO333: what a reader sees, checked after each draw: a shadow meant to be see-through (calm, or any under a
// vision filter) drawn more than 70 % opaque for ECHO_SOLID_S reads solid (v3.32's level-2 case: 3 blended layers of 45 %
// = 83 %); shadows in the same calm state drawn more than 0.15 apart for ECHO_SOLID_S: one reads solid, another
// see-through (the user's "1 of 3"). Each once a room. EchoDepth=0 asks for one pass (overlaps blend): said once instead.
static float g_echoOpClock = -1.0f, g_echoOpDisT;
static int g_echoOpDisSaid, g_echoLookSaid = -1;
static void EchoOpCheck(int look)
{
    float dt = g_ec.clock - g_echoOpClock, lo = 2.0f, hi = -1.0f;
    int i, ilo = -1, ihi = -1;
    g_echoOpClock = g_ec.clock;
    if (!(dt > 0.0f) || dt > 0.1f) dt = 0.0f;  // (a menu, a room change, the post-filter draw of the same tick: no time)
    if (g_echoDepth == 0) {
        if (!g_echoLogged[ELG_DEPTH0]++)
            LogV("echo: [Gameplay] EchoDepth=0: see-through shadows are drawn in one pass, their overlapping surfaces blend (about %.0f %% "
                 "instead of %.0f %%)", 100.0f * (1.0f - powf(1.0f - ECHO_CALM_OPA, ECHO_LAYERS)), 100.0f * ECHO_CALM_OPA);
        return;
    }
    for (i = 0; i < ECHO_MAXN; ++i) {
        EchoDrawS *d = &g_ed[i];
        const EchoShd *e = &g_es[i];
        const int thin = look != 0 || d->calm >= 0.95f;  // meant to be see-through now
        if (!d->ok || d->planF != g_fxFrame || g_echoGovK[i] < 0.999f) {  // v3.35 FX335: + a body the budget is fading: not a reader's state
            d->opBadT = 0.0f;
            continue;
        }
        d->opBadT = thin && d->op > 0.70f ? d->opBadT + dt : 0.0f;
        if (d->opBadT > ECHO_SOLID_S && !d->anomOp) {
            d->anomOp = 1;
            LogAnomaly("echo", "shadow %d is %s but drawn %.0f %% opaque for %.1f s: it reads solid (look %d, calm k %.2f, body alpha %.2f, "
                       "depth prepass %d, budget level %d)", i + 1, look ? "under a vision filter" : "calm", 100.0f * d->op, d->opBadT, look,
                       d->calm, d->sprVis * EchoBodyOp(look, d->calm), d->depth, EchoBudLv());
        }
        if (e->on && !e->leaving && e->sp >= 1.0f && d->sprVis > 0.95f && d->calm >= 0.999f) {  // (fully calm, fully shown)
            if (d->op < lo) lo = d->op, ilo = i;
            if (d->op > hi) hi = d->op, ihi = i;
        }
    }
    g_echoOpDisT = ilo >= 0 && ihi >= 0 && hi - lo > 0.15f ? g_echoOpDisT + dt : 0.0f;
    if (g_echoOpDisT > ECHO_SOLID_S && !g_echoOpDisSaid) {
        g_echoOpDisSaid = 1;
        LogAnomaly("echo", "shadows in the same %s state drawn differently for %.1f s: shadow %d at %.0f %% (prepass %d), shadow %d at %.0f %% "
                   "(prepass %d); look %d, budget level %d: one reads solid, the other see-through", look ? "vision" : "calm", g_echoOpDisT,
                   ihi + 1, 100.0f * hi, g_ed[ihi].depth, ilo + 1, 100.0f * lo, g_ed[ilo].depth, look, EchoBudLv());
    }
}
// The shadows drawn in one look (0 normal, 1 Thermal, 2 night vision, 3 Spirit Vision). game: the states the game's
// model draws expect (GvPass's capture), NULL = the current ones. Every state, the index buffer and the viewport go back.
static int EchoDrawAll(void *dev, const FxSaved *game, int look)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    static EchoSpr spr[ECHO_SPR_MAX * ECHO_MAXN];
    FxSaved sv;
    DWORD rs[4];
    void *ib = NULL;
    UINT base = 0;
    const Viewport8 *vp;
    LARGE_INTEGER q0, q1, q2, qf;
    int i, ok = 1, passes = 0, nspr = 0, drawnSpr = 0;
    BG_RUN(BGC_DFX, DepthFixFrame(dev, game, DFW_ECHO, look));  // v3.31 DEPTH331: her true depth first (not in Spirit Vision; depthfix.inc); v3.35 FX335: its own ledger part
    q0.QuadPart = q1.QuadPart = q2.QuadPart = 0;  // v3.31 MERGE: W1's first statement, now after W3's depth call
    if (!FxSave(dev, &sv, 6)) return 0;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) ok = 0;
    if (!ok || FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) {
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        FxRestore(dev, &sv);
        return 0;
    }
    vp = g_echoVpOk && g_echoVpStamp == g_fxFrame ? &g_echoVp : (game ? (const Viewport8 *)&game->vp : &sv.vp);
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv.vpSet = 1;
    __try {
        // v3.33 ECHO333: also Thermal (the coat as heat, hot points: v3.32 drew no sprites there)
        const int aura = g_echoAura && FxTexture(dev, FXX_SMOKE) && FxTexture(dev, FXX_FLAME) && FxTexture(dev, FXX_SVATLAS);
        if (aura) {  // this frame's matrix, projection (the exact depth) and eye; the list; the ground shadows first
            ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
            SvNearFar();
            DpCamProj();
            g_svEyeOk = SvEye(g_svEye);
            nspr = EchoSpriteList(look, spr, ECHO_SPR_MAX * ECHO_MAXN);
            if (nspr > g_echoSprMax) g_echoSprMax = nspr;
        }
        FxReapply(dev, game ? game : &sv);  // the game's states (its renderer sets per material only what it changes)
        QueryPerformanceCounter(&q0);  // v3.31 ECHO331: the body passes and the sprites timed apart (the room line)
        ok = EchoBodies(dev, look, &passes);
        QueryPerformanceCounter(&q1);
        if (ok && aura && nspr) {
            FxBase(dev);
            SvZ(dev, 2);  // the visible part only (reversed Z: GREATEREQUAL), no z write: the bodies hide what is behind them
            if (look == 0 && g_echoDiscK > 0.01f) drawnSpr += EchoSprites(dev, vp, look, 1, spr, nspr);  // the ground shadows (v3.35 FX335: faded by the budget)
            drawnSpr += EchoSprites(dev, vp, look, 0, spr, nspr);
        }
        QueryPerformanceCounter(&q2);
    } __finally {
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        g_svZScaleNow = SV_ZSCALE;  // (DpCamProj set the exact depth: Spirit Vision keeps its own calibration)
        ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
        FxRestore(dev, &sv);
    }
    if (QueryPerformanceFrequency(&qf) && qf.QuadPart && q2.QuadPart >= q1.QuadPart && q1.QuadPart >= q0.QuadPart) {  // v3.31 ECHO331
        const float mb = (float)((double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart);
        const float ms = (float)((double)(q2.QuadPart - q1.QuadPart) * 1000.0 / (double)qf.QuadPart);
        if (mb > g_echoMsBody) g_echoMsBody = mb;
        if (ms > g_echoMsSpr) g_echoMsSpr = ms;
    }
    if (ok) EchoOpCheck(look);  // v3.33 ECHO333
    if (!ok) {
        ++g_echoDrawFaults;
        Log("echo: a redraw faulted (code %08lX at %p)%s", g_echoFaultCode, g_echoFaultAt, g_echoDrawFaults >= 3 ? "; off for this session" : "");
        if (g_echoDrawFaults >= 3) EchoFailNow("the redraw");
        return 0;
    }
    if (!g_echoLogged[ELG_DRAWS]++) {
        const EchoDrawS *d = &g_ed[0];
        Log("echo: the shadows draw (look %d, %d shadow(s), %d bones + %d weapon, %d body passes, %d sprites, depth prepass %d, rim %d, aura %d)",
            look, g_echoDrawN, d->nb, d->nw, passes, drawnSpr, g_echoDepth, g_echoRim, g_echoAura);
    }
    if (g_echoCullSeen[1] + g_echoCullSeen[2] + g_echoCullSeen[3] >= 200 && !g_echoLogged[ELG_CULL]++) {
        Log("echo: her materials' cull modes (rim pass): none %lu, CW %lu, CCW %lu%s", g_echoCullSeen[1], g_echoCullSeen[2],
            g_echoCullSeen[3], g_echoCullSeen[1] > g_echoCullSeen[2] + g_echoCullSeen[3] ? " (mostly two-sided: little rim; the halo sprites stay)" : "");
    }
    return 1;
}
// The first-use hitch (v3.29: "the double's slowest draw 44.14 ms" in the first room it drew in, right after "heat shader
// made"): GOG's wrapper builds a shader pipeline the first time a draw uses it, and our textures are generated on first
// use. So once per device, as soon as the row is on and her body was drawn (the room's first gameplay frame, under its
// fade-in, or the first frame after the menu; the shadows form 0.25-0.5 s later), every shader, texture and pass of
// ours is made and drawn once with nothing visible: her own pose through each body pass with every pixel failing the
// alpha test, and one zero-colour quad for each sprite batch.
static void EchoWarm(void *dev)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    BYTE *body = EchoHerModel();
    const float *bm = NULL;
    FxSaved sv;
    DWORD rs[4];
    void *ib = NULL;
    UINT base = 0;
    LARGE_INTEGER t0, t1, f;
    int nb, i, ok = 1, quads = 0;
    QueryPerformanceCounter(&t0);
    nb = EchoBones(body, &bm);
    if (nb < 18 || nb > ECHO_MAXB) return;
    if (!FxShader(dev, FXT_HEAT) || !FxShader(dev, FXT_GVGLOW)) return;
    FxTexture(dev, FXX_SMOKE), FxTexture(dev, FXX_FLAME), FxTexture(dev, FXX_SVATLAS);
    if (!FxSave(dev, &sv, 6)) return;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) ok = 0;
    if (!ok || FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) {
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        FxRestore(dev, &sv);
        return;
    }
    __try {
        static const int kPass[5] = { EP_SOLID, EP_DEPTH, EP_INK, EP_RIM, EP_SPECTER };  // v3.34 REV334-001: + the specter pass
        if (g_echoVpOk && g_echoVpStamp == g_fxFrame) ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_echoVp), sv.vpSet = 1;
        memcpy(g_echoBody[0], bm, (size_t)nb * 64);  // her own pose (nothing of it shows)
        memcpy(g_echoRimM[0], bm, (size_t)nb * 64);
        g_ed[0].body = body, g_ed[0].wep = NULL, g_ed[0].held = 0;  // v3.32 ECHO332: (its matrices are hers now: none to hold)
        FxReapply(dev, &sv);
        g_echoWarm = 1;
        for (i = 0; i < 5 && ok; ++i) {  // (alpha 0: nothing shows; the specter pass is Spirit Vision's blend / fog state)
            g_echoPs = g_fxPs[kPass[i] == EP_RIM ? FXT_GVGLOW : FXT_HEAT];
            if (kPass[i] == EP_RIM) EchoGlowC(kEchoLight[0], 0.0f, 0.0f, 0.0f);
            else EchoInkC(kEchoInk[0], 0.0f, 0.0f, 0.0f);
            ok = EchoPassOne(0, kPass[i], 1);
        }
        g_echoWarm = 0;
        {   // one quad of zero colour per sprite batch (premultiplied alpha 0 / additive black: no change)
            static const int kTex[3] = { FXX_SVATLAS, FXX_SMOKE, FXX_FLAME };
            SpVtx q[6];
            int tx, ad;
            memset(q, 0, sizeof q);
            for (i = 0; i < 6; ++i) q[i].x = (float)(i & 1) * 2.0f + 1.0f, q[i].y = (float)(i >> 1 & 1) * 2.0f + 1.0f, q[i].z = 0.5f, q[i].rhw = 1.0f;
            FxBase(dev);
            SvZ(dev, 2);
            for (tx = 0; tx < 3; ++tx) {
                void *tex = g_fxTex[kTex[tx]];
                if (!tex) continue;
                for (ad = 0; ad < 2; ++ad) {
                    DpSlStates(dev, tex, ad);
                    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, 2, q, sizeof(SpVtx));
                    ++quads;
                }
            }
        }
    } __finally {
        g_echoWarmDev = dev;  // done for this device (also after a fault: never twice)
        g_echoWarm = 0;
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
        FxRestore(dev, &sv);
    }
    QueryPerformanceCounter(&t1);
    if (!g_echoLogged[ELG_WARM]++ && QueryPerformanceFrequency(&f) && f.QuadPart)
        Log("echo: made ready for this device in %.1f ms (shaders, 3 textures, 4 body passes, %d sprite batches; drawn invisible, "
            "before the shadows form: their first draw has no first-use hitch)%s", (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart,
            quads, ok ? "" : " (a pass faulted)");
}
static int EchoLook(void)  // 3 Spirit Vision, 1 Thermal, 2 night vision, 0 the normal look (as DpLookNow)
{
    const int f = g_fxOn ? (int)g_fx[FX_FILTER] : 0;
    return f == FXF_GHOST ? 3 : f == FXF_THERMAL ? 1 : (f == FXF_NV || f == FXF_NVMONO) ? 2 : 0;
}
static int EchoPrepareAll(void)  // every shadow's matrices this frame; the number to draw
{
    BYTE *body = EchoHerModel(), *wm;
    const float *bm = NULL, *wmats = NULL;
    float yaw;
    int nb, nw = 0, i, n = 0;
    for (i = 0; i < ECHO_MAXN; ++i) g_ed[i].ok = g_ed[i].bonesOk = 0;
    if (g_echoHerFrame != g_fxFrame) return 0;
    nb = EchoBones(body, &bm);
    if (nb < 18) return 0;
    if (nb > ECHO_MAXB) {
        if (!g_echoLogged[ELG_BONES]++) Log("echo: the player's model has %d bones (more than %d): no shadows", nb, ECHO_MAXB);
        return 0;
    }
    wm = EchoWeaponModel();
    if (wm) {
        nw = EchoBones(wm, &wmats);
        if (nw > ECHO_MAXW) nw = 0;
        if (!nw) wm = NULL;
    }
    if (wm && !g_echoLogged[ELG_WEP]++) Log("echo: the player's weapon model %p (%d bones; %s at 0052AEB5)", wm, nw, g_echoWepSeen ? "seen" : "not seen");
    yaw = *(volatile float *)0x10A0FC4;
    if (_finite(yaw)) EchoRSide(bm, yaw);
    for (i = 0; i < ECHO_MAXN; ++i)
        if (EchoPrepare(i, bm, nb, wm, nw, 0.0f)) ++n;
    return n;
}
static void EchoDrawC(void)
{
    void *dev = DEV;
    int look;
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    {   // a new room: its own draw cost line for the last one, the post-filter paths asked again, the budget again
        const DWORD rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
        if (rk != g_echoDrawRoom) {
            if (g_echoDrawRoom != 0xFFFFFFFFu && g_echoMsMax > 0.0f)  // v3.31 ECHO331: + where it went
                Log("echo: stage %lX room %lX: the shadows' slowest frame %.2f ms (body passes %.2f, sprites %.2f; the tick %.2f; "
                    "%d at most, %d sprites at most, budget level %d)", g_echoDrawRoom >> 16, g_echoDrawRoom & 0xFFFF, g_echoMsMax,
                    g_echoMsBody, g_echoMsSpr, g_echoTickMs, g_ec.n, g_echoSprMax, EchoBudLv());  // v3.35 FX335: the level in force
            g_echoDrawRoom = rk;
            ++g_echoVisit;
            g_echoMsMax = g_echoMsBody = g_echoMsSpr = g_echoTickMs = 0.0f;
            g_echoSprMax = 0;
            memset(&g_echoBud, 0, sizeof g_echoBud);
            {   // v3.33 ECHO333: the look anomalies again in every room
                int k;
                for (k = 0; k < ECHO_MAXN; ++k) g_ed[k].anomOp = 0, g_ed[k].opBadT = 0.0f;
                g_echoOpDisSaid = 0, g_echoOpDisT = 0.0f;
            }
        }
    }
    if (!dev || !FxOnBackBuffer(dev)) return;
    if (g_echoWarmDev != dev && g_echoMode > 0 && g_echoHerFrame == g_fxFrame) {
        EchoWarm(dev);  // (not timed: it is not the shadows' draw; v3.35 FX335: RfWarmFrame has usually made it ready already)
        QueryPerformanceCounter(&t0);
    }
    {   // nothing shown and no smoke still in the air: nothing to draw (Off: nothing at all, as v3.29.0)
        int k, j, smoke = 0;
        for (k = 0; k < ECHO_MAXN; ++k)
            for (j = 0; j < ECHO_EVT_N; ++j) smoke |= g_echoEv[k][j].e.t0 > 0.0f && g_ec.clock - g_echoEv[k][j].e.t0 <= ECHO_PUFF_OUT;
        smoke |= GtSmokeLive();  // v3.43 GT343: Ghost Teleport's smoke draws here too (with Echo Doubles Off)
        if (g_echoMode <= 0 && !g_ec.on && !smoke) return;
        EchoPoseSample();
        g_echoDrawN = EchoPrepareAll();
        if (!g_echoDrawN && !smoke) return;
    }
    {   // v3.42 SM342 (USER_FEEDBACK_v341: hidden in peep / window views, with Death Presence's signals): not drawn while
        // the room is off, the peeping flag is on or a move of the player is pending (the tick and the pose sampling run on)
        static DpViewFx s_view = { { 0 }, { 0 }, -10 };
        if (!DpViewFrame(&s_view, "echo")) {
            g_echoDrawN = 0;  // (loghealth.inc: not an active frame)
            return;
        }
    }
    look = EchoLook();
    if (look != g_echoLookSaid && g_echoDrawN) {  // v3.33 ECHO333: the filter theme the shadows wear now (LogV, rate-limited)
        static const char *const kLk[4] = { "the normal look (calm 1.5 x farther at 45 %, solid while striking)",
                                            "Thermal: a cold see-through body (50 % striking, 35 % calm), its coat as heat with hot points",
                                            "night vision: see-through (50 / 35 %), a phosphor edge, pale filaments and eyes",
                                            "Spirit Vision: dark specters, see-through (62 / 45 %), dark smoke and a faint cold rim" };  // v3.34 FX334
        static volatile LONG s_q;
        if (LogQuota(&s_q, 30)) LogV("echo: the shadows wear %s (filter %d)", kLk[look & 3], g_fxOn ? (int)g_fx[FX_FILTER] : 0);
        g_echoLookSaid = look;
    }
    if (look == 3 || look == 1) {  // Spirit Vision / Thermal: drawn after the filter by GvPass / FxPass (EchoPostDraw)
        if (g_echoPostFrame == g_fxFrame - 1 && g_echoPostDone != g_fxFrame - 1 && g_echoPostLook == look &&
            g_echoPostBroken[look] != g_echoVisit) {
            g_echoPostBroken[look] = g_echoVisit;
            Log("echo: %s did not draw the shadows: the plain look in this room", look == 3 ? "Spirit Vision" : "Thermal");
        }
        if (g_echoPostBroken[look] != g_echoVisit) {
            g_echoPostFrame = g_fxFrame;
            g_echoPostLook = look;
            return;
        }
        look = 0;
    }
    EchoDrawAll(dev, NULL, look);
    {
        LARGE_INTEGER t1, f;
        QueryPerformanceCounter(&t1);
        if (QueryPerformanceFrequency(&f) && f.QuadPart) {
            const float ms = (float)((double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart);
            if (ms > g_echoMsMax) g_echoMsMax = ms;
            const int why = g_bgOn ? 0 : EchoBudStep(&g_echoBud, ms);  // v3.31 ECHO331: also a single slow frame; v3.35 FX335: only with the budget off
            if (why && g_echoLogged[ELG_BUDGET]++ < 6) {
                char how[64];
                if (why == 2) sprintf_s(how, sizeof how, "one frame over %.1f ms", ECHO_MS_SPIKE);
                else sprintf_s(how, sizeof how, "over %.1f ms for %d frames", ECHO_MS_CAP, ECHO_SLOW_N);
                Log("echo: %s (%.2f ms, %d shadows): budget level %d for this room (%s)", how, ms, g_echoDrawN, g_echoBud.level,
                    g_echoBud.level == 1 ? "no weapon redraw on the 2nd / 3rd" : g_echoBud.level == 2 ? "+ the light look on the 2nd / "
                    "3rd, no depth prepass in their short fades; a see-through body keeps it (v3.33)" : "+ no weapon redraw at all, no ground discs");
            }
        }
    }
}
static void EchoDrawFrame(void)  // the scene pass (SparkleFrame, before Death Presence's fog: its smoke blends over them)
{
    if (!g_echoInstalled || g_echoFail || !g_fxOn || !g_spInstalled) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay()) return;
    __try {
        EchoDrawC();
    } __except (EchoFaultNote(GetExceptionInformation())) {
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        EchoFailNow("the draw");
    }
}
// After the filter: Spirit Vision (GvPass, gameplay block, after the ghost glow; gameSv = its capture of the game's
// states) and Thermal (FxPass after ThHeatPass and Death Presence; gameSv NULL: the states are the game's there).
static void EchoPostDraw(void *dev, const FxSaved *gameSv, int look)
{
    if (!g_echoInstalled || g_echoFail || g_echoPostFrame != g_fxFrame || g_echoPostLook != look || !dev) return;
    __try {
        if (g_echoHerFrame == g_fxFrame) {
            g_echoPostDone = g_fxFrame;  // served
            EchoDrawAll(dev, gameSv, look);
        }
    } __except (EchoFaultNote(GetExceptionInformation())) {
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        EchoFailNow("the post-filter draw");
    }
}

// ---------------------------------------------------------------- install (Init, after DeathPresenceInstall)
static void EchoInstall(void)
{
    char buf[32];
    int v;
    g_ec.room = 0xFFFFFFFFu;
    g_ec.rsign = 1.0f;
    g_ec.herH = 170.0f;
    EchoRingInit(&g_echoTR, ECHO_TICK_N);
    EchoRingInit(&g_echoPR, ECHO_POSE_N);
    if (!g_qsTickOn || !g_fxOn || !g_spInstalled || !g_thermHooks || !g_gvDipHooks) {  // fail closed (as Death Presence)
        Log("echo: not available (tick dispatcher %s, scene pass %s, HUD hook %s, model hook %s, draw-call override %s)",
            g_qsTickOn ? "in" : "off", g_fxOn ? "in" : "off", g_spInstalled ? "in" : "off", g_thermHooks ? "in" : "off",
            g_gvDipHooks ? "in" : "off");
        g_echoFail = 1;
        return;
    }
    v = (int)GetPrivateProfileIntA("Gameplay", "EchoDamage", 50, g_iniPath);  // hidden keys (not written to the ini)
    g_echoShare = (float)(v < 0 ? 0 : v > 100 ? 100 : v) / 100.0f;
    GetPrivateProfileStringA("Gameplay", "EchoSide", "Auto", buf, sizeof buf, g_iniPath);
    g_echoSidePref = !_stricmp(buf, "Right") ? 1 : !_stricmp(buf, "Left") ? 2 : 0;
    GetPrivateProfileStringA("Gameplay", "EchoColour", "Violet", buf, sizeof buf, g_iniPath);
    g_echoColour = !_stricmp(buf, "Blue") ? 1 : !_stricmp(buf, "Crimson") ? 2 : !_stricmp(buf, "White") ? 3 : 0;
    v = (int)GetPrivateProfileIntA("Gameplay", "EchoDepth", 2, g_iniPath);  // v3.31 ECHO-331-002: absent = 2; 1 keeps v3.30's meaning
    g_echoDepth = v < 0 ? 0 : v > 2 ? 2 : v;
    g_echoAura = GetPrivateProfileIntA("Gameplay", "EchoAura", 1, g_iniPath) != 0;
    g_echoRim = GetPrivateProfileIntA("Gameplay", "EchoRim", 0, g_iniPath) != 0;  // v3.31 ECHO331: default 0 (the flame's edge)
    g_echoMorphKeep = GetPrivateProfileIntA("Gameplay", "EchoMorphKeep", 1, g_iniPath) != 0;  // v3.31 ECHO331
    g_echoInstalled = 1;
    g_echoReady = 1;
    Log("echo: in (no exe patch: tick 004EF982, scene pass 004EFBFD, models 0052AEB5, override 004292EF / 00429338); %s, %s "
        "shadow(s), damage %.0f %% (each %.0f %%), side %s, colour %d, depth prepass %d, rim %d, aura %d, morph kept %d; v3.32 "
        "flame coat, calm 1.5 x / 45 %%; v3.33 every see-through body keeps its prepass, vision filters see-through 50 / 35 %% (v3.34: Spirit Vision dark specters 62 / 45 %%; "
        "v3.35: thinned by the effect budget, %s)",
        kGpName[GP_ECHO][g_gp[GP_ECHO]], kGpName[GP_ECHON][g_gp[GP_ECHON]], 100.0f * g_echoShare, 100.0f * EchoShareFor(g_echoShare, EchoCount()),
        g_echoSidePref == 0 ? "auto" : g_echoSidePref == 1 ? "right" : "left", g_echoColour, g_echoDepth, g_echoRim, g_echoAura,
        g_echoMorphKeep, GetPrivateProfileIntA("Graphics", "EffectBudget", 1, g_iniPath) ? "it comes back when there is room" :
        "off: v3.31's budget level for the room");  // v3.34 FX334: + "(v3.34: Spirit Vision dark specters 62 / 45 %)"; v3.35 FX335: + the budget
}

// ================================================================ v3.35 FX335: the effects made ready at a room's first frame
// (staging/v335_feedback/w2_fx/PLAN.md; the user's log: a 276 ms freeze in play right after a load, "after shock 259.2 ms";
// another session: "pre-warm more effects at load, the way Echo Doubles already does"). The HUD hook runs this before the
// effects' own draws (items.inc SparkleFrame), on the first gameplay frame of every room visit (under its fade-in) and
// again when a row is turned on later: every shader and texture an effect that is on uses is made, and every draw state it
// will use is drawn once with nothing visible (Echo: EchoWarm; After Shock: AiWarm, also each enemy type's shock pass;
// Death Presence: DpWarm; the screen effects: every filter shader and the small textures, FxWarmScreen). Most of it once a
// device; each step's time is logged (LogV) so the log proves no first use is left for play.
static DWORD g_rfWarmRoom = 0xFFFFFFFFu;
static unsigned g_rfWarmOn;     // the effects already served in this room visit (bit 0 Echo, 1 After Shock, 2 Death Presence)
static int g_rfWarmTries;
static void *g_rfWarmScrDev;
static float RfMsSince(const LARGE_INTEGER *t0)
{
    LARGE_INTEGER t1, f;
    QueryPerformanceCounter(&t1);
    return QueryPerformanceFrequency(&f) && f.QuadPart ? (float)((double)(t1.QuadPart - t0->QuadPart) * 1000.0 / (double)f.QuadPart) : 0.0f;
}
static void RfWarmFrameC(void)
{
    void *dev = DEV;
    const DWORD room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    const unsigned on = (g_echoInstalled && !g_echoFail && g_echoMode > 0 ? 1u : 0u) | (g_aiInstalled && !g_aiFail && g_aiLevel > 0 ? 2u : 0u) |
                        (g_dpInstalled && !g_dpFail && g_dpLevel > 0 ? 4u : 0u);
    char line[600], types[64];
    LARGE_INTEGER t0, ta;
    int o = 0, did = 0, served = 0;
    if (room != g_rfWarmRoom) g_rfWarmRoom = room, g_rfWarmOn = 0, g_rfWarmTries = 0;
    if (!(on & ~g_rfWarmOn) && g_rfWarmScrDev == dev) return;  // everything that is on was served in this visit
    if (g_rfWarmTries > 90) return;  // (her body never drawn in 90 frames: given up for this visit)
    ++g_rfWarmTries;
    if (!FxOnBackBuffer(dev)) return;
    QueryPerformanceCounter(&t0);
    line[0] = 0;
    if (g_rfWarmScrDev != dev) {  // the screen effects (once a device)
        int tex = 0, sh;
        QueryPerformanceCounter(&ta);
        sh = FxWarmScreen(dev, &tex);
        g_rfWarmScrDev = dev;
        o += sprintf_s(line + o, sizeof line - (size_t)o, "screen effects %.1f ms (%d filter shaders, %d textures)", RfMsSince(&ta), sh, tex);
        did = 1;
    }
    if ((on & 4u) && !(g_rfWarmOn & 4u)) {  // Death Presence
        int quads = 0;
        QueryPerformanceCounter(&ta);
        if (DpWarm(dev, &quads) && o < (int)sizeof line - 120)
            o += sprintf_s(line + o, sizeof line - (size_t)o, "%sdeath presence %.1f ms (fog, wisp, smoke, atlas and flame textures; %d draw "
                           "states)", o ? "; " : "", RfMsSince(&ta), quads), did = 1;
        g_rfWarmOn |= 4u;
    }
    if ((on & 1u) && !(g_rfWarmOn & 1u)) {  // Echo (its own line says what; once a device)
        if (g_echoWarmDev == dev) {
            g_rfWarmOn |= 1u;
        } else if (g_echoHerFrame == g_fxFrame) {
            QueryPerformanceCounter(&ta);
            EchoWarm(dev);
            if (o < (int)sizeof line - 120)
                o += sprintf_s(line + o, sizeof line - (size_t)o, "%secho %.1f ms (shaders, 3 textures, 5 body passes, 6 sprite batches)",
                               o ? "; " : "", RfMsSince(&ta));
            did = 1;
            g_rfWarmOn |= 1u;
        }
    }
    if ((on & 2u) && !(g_rfWarmOn & 2u)) {  // After Shock: her passes once a device, the room's new enemy types every visit
        int bodies = 0, quads = 0, nt = 0;
        const int bodyDue = g_aiWarmDev != dev;
        if (!bodyDue || g_aiHerFrame == g_fxFrame) {
            QueryPerformanceCounter(&ta);
            if (AiWarm(dev, &bodies, &quads, &nt, types, sizeof types) && o < (int)sizeof line - 160) {
                if (bodyDue)
                    o += sprintf_s(line + o, sizeof line - (size_t)o, "%safter shock %.1f ms (her glow and heat passes: %d drawn; %d sprite / ribbon "
                                   "states; %d new enemy type(s) for the shock pass%s%s)", o ? "; " : "", RfMsSince(&ta), bodies, quads, nt,
                                   nt ? ": " : "", types);
                else
                    o += sprintf_s(line + o, sizeof line - (size_t)o, "%safter shock: %d new enemy type(s) for the shock pass (%s), drawn next frame",
                                   o ? "; " : "", nt, types);
                did = 1;
            }
            g_rfWarmOn |= 2u;
        }
    }
    served = !(on & ~g_rfWarmOn);
    if (did) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 400))
            LogV("warm-up: stage %lX room %lX: %s; %.1f ms in all, under the room's fade-in%s", room >> 16, room & 0xFFFF, line, RfMsSince(&t0),
                 served ? "" : " (her body not drawn yet: the rest next frame)");
    }
}
static void RfWarmFrame(void)
{
    static int s_faults;
    if (!g_fxOn || !g_spInstalled || s_faults >= 3 || !DEV) return;
    if (*(volatile int *)0xFD5C20 != 0 || !FxGameplay()) return;
    __try {
        RfWarmFrameC();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_echoWarm = 0, g_aiWarmNow = 0;
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        InterlockedExchange(&g_gvGlow, 0);
        InterlockedExchange(&g_thMode, 0);
        g_aiZ = 0;
        g_rfWarmOn = 7u;  // (nothing more this visit)
        if (++s_faults == 1 || s_faults == 3)
            Log("warm-up: faulted (code %08lX)%s", GetExceptionCode(), s_faults >= 3 ? ": off for this session (the effects make their own on first use)" : "");
    }
}
