// ================================================================ v3.43 GT343: Ghost Teleport (Option 5 > Special)
// [Gameplay] GhostTeleport = Off / On (0 / 1; default Off). Plans, RE and the live checklists:
// staging/v343_feedback/w2_teleport/PLAN.md (v3.43: the target, the room checks) and staging/v344_feedback/w3_teleport/
// PLAN.md (v3.44). The pure rules: teleport_sim.h (test/gt_sim_test.c).
//
// v3.44 GT344 (the user's v3.43 QA: "Pass!", three wishes):
// (a) The trigger is Run + Action pressed together (both down within 80 ms, either first; no standing still, no hold;
//     teleport_sim.h GtChordStep). When Action is the second press, its press is taken back on this tick before the
//     player's update reads it (the game tick's first call 0x4EF982 runs before the phase's tasks): the bits of its
//     binding (gInputActionMask[0] 0x107A1A0) are cleared from this frame's pressed mask (gPad.pressed 0x1236C34, the
//     word InputGetPressedMask 0x554250 returns), so the doors, pick-ups and examines (InputActionPressed(0, 0) and the
//     move builder 0x52EEE0's action flags) do not fire. When Action went first its press reached the game a tick
//     before; whatever it started (a door, an event, a pick-up) then refuses the warp by the usual gates. The press is
//     only taken back when the warp's gates pass (in Room 302, a raised weapon, an event, ... Action stays the game's).
// (b) The transition is ghost Richard's (EnemyScratchGhostState06 / 07): his warp sound 0xEC86 at her place, his
//     see-through fade: 0.5 s out, the warp, 0.5 s in (ScratchUpdateAlpha 0x4B9900), drawn on her model with his
//     draw's own states: the ghost's pre-draw 0x45C7A0 (pixel shader 4, c0 = FIX / 128, GS ALPHA (Cs - Cd) x FIX + Cd
//     by 0x429ED0(FIX << 32 | 0x64), the blend mode of TEST 0x53001 (z write off, colour only) by 0x429F00, the flag
//     0x429F60(1)) and his post-draw 0x507240 (the stock ALPHA, blend mode 8, pixel shader 8). They are put in her body's
//     and her weapon's actor records (+0x24 pre-draw, +0x28 post-draw: the slots ActorDrawModels 0x52AE50 calls around
//     each model draw, the ghost's own way: ActorSetPreDraw 0x52ACB0 / ActorSetPostDraw 0x52ACD0 in func_0045D490) only
//     while the fade runs, only into empty slots, and taken out again (only while still ours). No exe patch: the draw
//     loop's two calls and every function used are byte-checked. Echo Doubles' smoke (v3.43's stand-in) is gone:
//     Richard's warp has none.
// (c) With no standing enemy in range she warps [Gameplay] GhostTeleportForward (hidden, 800 u) the way she faces,
//     shortened 100 u a try down to 200 u, onto a floor spot that passes the same spot rules from her own place (her
//     line of sight, the room's box and door lines: fail closed), keeping her facing. None fits: refused.
// The target is fixed when the fade-out ends (Richard's ScratchWarp runs then too): the search runs at the chord (no
// spot = refused at once, no fade) and again at the warp (the enemy may have moved; none now = she fades back in where
// she stands). During the fade-out an event, a hit or a grab (GtCanGo refuses) calls it off and she fades back in where
// she stands (GtCallOff). A room change, a cut-scene, an overlay (the pause, a menu, a message), a movie, no player or the
// option Off end the fade at once instead: she is drawn as stock on that tick and the draw slots come off (the room's actor
// records are freed and reused at a room change, so the slots are never kept across one; Codex inspection 6,
// GT344-ROOMFADE-1: by design, this comment corrected).
//
// Game functions called (each prologue byte-checked at the start; none is a Playable Eileen hook or a cheat-table
// site, ct_conflicts --span: none): 0x4F8440 InputActionHeld / 0x4F84A0 InputActionPressed (Run 0x10, Action 0),
// 0x463F50 EnemyEventPauseTest, 0x45CE10 the enemy model's rotation, 0x46F700 not inside geometry, 0x459490
// EnemyFloorRayCheck, 0x458F00 room to stand, 0x454000 line test, 0x46F5A0 the sub-area of a point, 0x52CDA0
// PlayerSetPosition, 0x52CDF0 PlayerSetYaw, 0x46E7C0 a one-shot 3D sound, 0x4FA040 / 0x4FA070 ExitGetNum / ExitGet,
// 0x561A70 the room's collision box; v3.44: 0x404B80 RenderSetPixelShader, 0x429ED0 / 0x429F00 / 0x429F60 (the GS
// mirror), 0x507240 (the ghost's post-draw); read-checked: 0x554250 (gPad.pressed's address), 0x4F82C0 (the action
// masks' address), 0x52AE88 / 0x52AEBD (ActorDrawModels' pre-draw / post-draw calls).
// Never during events, cut-scenes, menus, movies, door moves, peeps, grabs, hits, falls, a raised weapon, any mode but
// the free one, or in Room 302 (stages 1 and 0x0A); no valid spot = no warp (a LogV line). Henry and Playable Eileen
// alike (both are gPlayer[0]; her body record is the player's model record either way).
//
// v3.49 GT349 (the user's v3.48 QA: a crash after two warps in Subway room 2:2, and "keep holding the combo to teleport
// continuously, no 3 s cooldown"; staging/v349_feedback/w2_teleport/PLAN.md and CRASH_ANALYSIS.md):
// (a) The crash: warp 15 jumped over the walk-in box of the stock cut-scene event 00A (record 0x62A6D0, story flag 0x130
//     "Creatures I"), which the stock game cannot skip; Cynthia's companion task stayed alive, the door to 2:3 brought
//     it back in a room without her files and EileenAnimIkUpdate 0x43E670 read a NULL clip (0x43E775). Now every spot is
//     also tested against the room's walk-in event zones the game would still fire (GtZonesRead: the event objects
//     0x13D8200 that EventObjectsTick 0x4D00F0 tests, of actor type 7 / 9, enabled, not running, no use-item, shape
//     cylinder / box / sphere without the action-button or inverted flag): a straight path from her place that crosses
//     one, or a spot in one, is refused like a wall (GTS_EVENT; teleport_sim.h GtZoneHit). Nearer spots still go.
// (b) More gates (GtCanGo: before the warp, through its fade-out and at the warp): a door in use (gGame.exit 0xFD5C3C), a
//     room change requested (gGame.nextStage / nextRoom 0xFD5AA0 / AA4), the player update held by a script (0x10A1E98),
//     an interaction event (0xFD5C28), the screen fading (0x137F98C / 0x137F990: a door, a cut-scene's start).
// (c) No cooldown by default ([Gameplay] GhostTeleportCooldown, hidden, now 0; a value in the ini still applies).
//     Holding Run + Action keeps warping: the next warp starts once she has faded back in (GtHold); a refused try waits
//     GT_FAIL_COOL. The hold's warps after the first GT_HOLD_LOGN are logged per tick only (LogT); its end logs one
//     summary line. A press let go before the fade ends starts nothing more (one press = one warp).
//
// v3.50 TP350 (the user's v3.49 QA, staging/v350_feedback/USER_FEEDBACK_v349.md A1-A3; staging/v350_feedback/w1_teleport/
// PLAN.md):
// (a) A2 / A3: with standing enemies near, ONLY the one she faces: nearest her facing inside +-[Gameplay]
//     GhostTeleportCone (hidden, 35 degrees; ties within 5 degrees: the nearer), never another; not in this room (beyond
//     a door line, outside the box): the next one in the cone. Its spots all refused: no warp (never another enemy, never
//     the warp ahead). None in the cone: the warp ahead. The reach doubled: GhostTeleportRange 5000 (was 2500; 500 ..
//     12000; an old value in the ini still applies).
// (b) A1: the warp ahead may pass a wall, a turnstile, a fence, a counter or a gap in the floor ([Gameplay]
//     GhostTeleportThroughWalls, hidden, 1; 0 = v3.49's warp ahead only): teleport_sim.h GtThruSearch along her facing and
//     +-10 / +-20 degrees, every 40 u up to GhostTeleportForward; past the first sample she could not walk to straight
//     (GtThruOpen on the room's solid map), the nearest spot that passes GtThruLand: v3.49's spot rules without the line
//     of sight (the enemies' gap, the room's box incl. its height, its door lines and no doorway crossed, the walk-in
//     event zones on the straight segment from her feet, not inside geometry, the game's floor ray 0x459490 at her level,
//     room to stand 0x458F00, the sub-area), her body's three spheres (GtBodyFree: 0x561B80) and the room's floor map
//     (GtMapHas: the floor a route of the room reaches, flooded from the room's doors and every place she stood at a warp,
//     teleport_sim.h GtMap; built once a room visit, GT_MAP_PRESS cells at the press, GT_MAP_TICK a tick through the
//     fade-out, GT_MAP_PRESS more at the warp). Not past any obstacle: v3.44's warp ahead (farthest first). At the press a
//     spot the unfinished map does not hold yet starts the fade; the warp decides (refused there: v3.44's warp ahead,
//     else she fades back in). The collision queries are No Clip's (noclip_world.h: 0x561CD0 / 0x41E9C0 the tree2 line,
//     0x561CB0 solid, 0x561B80 the sphere), byte-checked here too; a room without a solid map (tree2): no through-warp.
// (c) The fade-out owns Action: a press of Action (0) or Check (0xE, the examine points' InputActionPressed(0, 0xE) in
//     0x52C410) during the fade-out is taken back like the chord's own (v3.49's log: 7 fades called off by an
//     "interaction event" 0.15 .. 0.27 s after the press, in Subway rooms 2:4 and 2:7).
// (d) The log: a quota per refusal kind (GTN_*), and one line a room visit with its counts (warps by kind, refusals by
//     kind, presses taken back, the floor map's cells and time); logcheck's "ghost teleport" line sums them.
//
// v3.51 TP351 (the user's v3.50 QA item 2, staging/v351_feedback/USER_FEEDBACK_v350.md; staging/v351_feedback/w1_teleport/
// PLAN.md): "If there is collision in the way, do not teleport behind enemies on the other side, just teleport to or
// through the collision." An enemy in the cone counts as the one she faces only in the open: teleport_sim.h GtFoeSight
// on the through-warp's collision (the tree2 line 0x561CD0 from her chest to its chest, and her low line to its on one
// level; its chest not inside solid 0x561CB0; a victim / story ghost with its own wall portal open is in its wall). One
// behind collision is skipped (GtPickFacedVia: the next in the cone with a clear line); none left = the warp ahead
// (through the wall onto reachable floor, or up to it), as with no enemy in the cone. Hidden [Gameplay]
// GhostTeleportBehindCollision 1 = v3.50's rule. The warp / refusal lines say what was skipped and why; the room visit's
// line counts the skips. No exe patch, no new site (the queries are v3.50's byte-checked ones; the mover line 0x454000
// stands in when they are not stock).
//
// v3.52 TP352 (the user's v3.51 QA D4, staging/v352_feedback/USER_FEEDBACK_v351.md; staging/v352_feedback/w5_teleport/
// PLAN.md): stuck in a Water Prison wall after warp 129 (behind type 09, spot 2 on its far side). The spots behind the
// enemy and v3.44's spots ahead never tested her body against the walls. Now:
// (a) every landing kind (behind, ahead, through) passes the same proof (teleport_sim.h GtLandProof): her body's three
//     spheres free and on the room's floor map (a route of the room reaches it by her walk rules); the map is built for
//     every try (it was built only for the through-warp); a room without a solid map, or the queries not stock: no
//     landing is proven, no warp (GTS_PROOF). At the press an unfinished map's spot starts the fade; the warp decides.
// (b) the map takes no seed inside collision (GtCellFree): her feet inside a wall never make a closed space "reached".
// (c) after every warp her body is watched (GtFreeWatch, 1 s): inside collision 0.15 s in a row (GtStuck) = she is moved
//     to the nearest proven floor cell within 480 u (GtFreeCell; the same proof plus the game's floor ray, room to stand,
//     the door lines, the walk-in zones); a "freed from a wall" line (bounded) and the room visit's count.
// (d) a warp that starts inside a wall and finds no other landing moves her to that cell too (kind 3, "freed").
// No exe patch, no new game function (every query is v3.50's byte-checked kWorld / kFn).

#include "teleport_sim.h"

enum { GTM_L, GTM_D0, GTM_D1, GTM_NA, GTM_N };
static BYTE g_txtGt[GTM_N][64];
typedef char GtTextsFit[GTM_N <= MSG_FN0 - MSG_GT0 ? 1 : -1];  // the MSG_GT0 block (16 ids)

static volatile LONG g_gtSet;  // the row: 0 Off, 1 On
static LONG g_gtSnap;
static int g_gtUnsaved, g_gtReady, g_gtFault;
static DWORD g_gtIds[2];
static volatile float g_gtRange = GT_RANGE_DEF, g_gtCoolS = GT_COOL_DEF, g_gtFwd = GT_FWD_DEF;  // GT344: + Forward
static volatile LONG g_gtSound = 1;
static const char *const kGtName[2] = { "Off", "On" };
// game thread from here on (GtTick, the draw)
static GtChord g_gtChord;           // v3.44 GT344: Run + Action
static GtFade g_gtFade;             // v3.44 GT344: Richard's fade
static float g_gtCool;              // seconds of cooldown left
static float g_gtEat;               // v3.44 GT344: a chord's Action press is taken back while this runs (Action held)
static DWORD g_gtRoomKey = 0xFFFFFFFFu;
static struct { int on; float t, spot[3]; } g_gtHeld;  // the check after a move
static int g_gtWarps, g_gtRefused, g_gtMaskSaid;
static float g_gtFrom[3];           // v3.44 GT344: where the fade started (the log)
static volatile LONG g_gtQGo, g_gtQNo, g_gtQCool, g_gtQAn, g_gtQStart, g_gtQOff, g_gtQDraw;
// v3.49 GT349: the hold (Run + Action kept down) and the room's walk-in event zones
#define GT_HOLD_LOGN 3              // a hold's first warps get their LogV lines, the rest LogT (its summary says how many)
#define GT_ZONE_MAX 64
static GtHold g_gtHold;
static volatile LONG g_gtQHold, g_gtQZone;
static GtZone g_gtZone[GT_ZONE_MAX];
static int g_gtZoneKey[GT_ZONE_MAX];  // each zone's event key (the log)
static int g_gtZoneN, g_gtZoneSkipped;
static DWORD g_gtZoneSaid = 0xFFFFFFFFu;  // the room key the zones were last logged for
static float g_gtHerAt[3];          // her feet at this search (the start of every warp path)
// v3.50 TP350: the faced enemy's cone, the through-warp, the room's floor map, the room's counts
#define GT_MAP_PRESS 3000           // the floor map's cells expanded at a press and at a warp (2:9's whole map: 2648)
#define GT_MAP_TICK 600             //   and each tick of a fade-out
#define GT_NO_KIND_MAX 60           // LogV lines per refusal kind (the rest LogT; the room's line counts them all)
enum { GTN_STATE, GTN_NONE, GTN_EDGES, GTN_ZONES, GTN_INZONE, GTN_FACED, GTN_AHEAD, GTN_N };
static const char *const kGtNoKind[GTN_N] = { "her state", "nothing in reach", "the room's edges", "the zones",
                                              "inside a zone", "behind the faced enemy", "ahead" };
static volatile float g_gtCone = GT_CONE_DEF;
static volatile LONG g_gtThruOn = 1;
static int g_gtWorldOk;             // the tree2 / sphere queries are stock (GtInstall)
static GtMap g_gtMap;
static DWORD g_gtMapKey = 0xFFFFFFFFu;
static double g_gtMapMs;
static int g_gtMapSaid;             // 1 begun, 2 finished, 3 overflow said (this map)
static volatile LONG g_gtQNoK[GTN_N], g_gtQMap, g_gtQRoom, g_gtQBack;
static float g_gtThruFrom[3];       // her feet on her floor (GtThruSearch's callbacks)
static const float (*g_gtOthersP)[3];
static int g_gtNOthers;
static int g_gtRoomW[3], g_gtRoomNo[GTN_N], g_gtRoomBack;  // this room visit: warps by kind, refusals by kind, presses taken back
// v3.51 TP351: an enemy she faces only through collision is skipped (teleport_sim.h GtFoeSight); hidden [Gameplay]
// GhostTeleportBehindCollision 1 = v3.50's rule (allowed; default 0); this room visit: enemies skipped, the tries with one
static volatile LONG g_gtBehindColl;
static int g_gtRoomSkip, g_gtRoomSkipTries;
// v3.52 TP352: the landing proof (this search's: the solid map's queries stock and present), the watch after a warp,
// this room visit's frees (after a warp, at a warp from inside a wall)
static int g_gtProofOk;
static GtFreeWatch g_gtFreeW;
static int g_gtFreeWarp, g_gtFreeKind, g_gtFreeSaid;  // the watched warp's number and kind; its gate wait said
static int g_gtRoomFreed;
static volatile LONG g_gtQFree;

typedef struct { const BYTE *t, *w; float pos[3], rad, floorY; int type; } GtFoe;
enum { GTS_OK, GTS_ENEMY, GTS_SIGHT, GTS_WALL, GTS_RAY, GTS_FLOOR, GTS_LEDGE, GTS_ROOM, GTS_AREA,
       GTS_BOX, GTS_DOOR_NEAR, GTS_DOOR_BEYOND, GTS_DOOR_CROSS, GTS_FOE_DOOR, GTS_EVENT,
       GTS_BODY, GTS_ROUTE, GTS_PENDING, GTS_PROOF, GTS_N };  // GT343-001: + the room's edges  // v3.50 TP350: + body, route, pending  // v3.52 TP352: + proof
static const char *const kGtWhy[GTS_N] = { "", "near an enemy", "a wall between", "in a wall", "no free floor ray",
                                           "no floor", "another level", "no room to stand", "another area",
                                           "outside the room's box", "at a door line", "beyond a door line (the next room)",
                                           "through a doorway", "the enemy beyond a door line",
                                           "an event's trigger zone on the way",  // v3.49 GT349: + GTS_EVENT
                                           "no room for her body", "no route of the room reaches it",  // v3.50 TP350
                                           "not on the room's floor map yet",
                                           "the landing cannot be proven (no solid map, or its queries are not stock)" };  // v3.52 TP352
typedef char GtWhyFit[sizeof kGtWhy / sizeof kGtWhy[0] == GTS_N ? 1 : -1];  // v3.52 TP352
// GT343-001: this room's edges, read once an attempt (GtRoomRead): its door lines and its collision box
static GtExit g_gtExit[GT_EXIT_MAX];
static int g_gtExitN, g_gtBoxOk, g_gtAreaHer, g_gtExitAsked;  // GT343-001c: + ExitGetNum's own count
static float g_gtBoxLo[3], g_gtBoxHi[3];
// v3.44 GT344: a found target (GtFind): behind an enemy (kind 0) or ahead of her (kind 1)
typedef struct {
    int kind, type, foe, np, spot, fromFace, tries, nTries;
    float at[3], yaw, d, gap;
    int ray, kObst, refusedBefore, pending, nCone;  // v3.50 TP350: kind 2 = through a wall (GtThru); kind 0: the cone's count
    float dObst, ang;                               //   the obstacle's distance; the faced enemy's angle (degrees)
    int nSkip, skipWhy, skipType;                   // v3.51 TP351: enemies in the cone skipped behind collision (the first's
    float skipD, skipAng;                           //   GTV_* reason, type, distance, angle off her facing in degrees)
} GtPlan;

// ---------------------------------------------------------------- the game's queries
static int GtHeld(int action) { return ((int(__cdecl *)(int, int))0x4F8440)(0, action) != 0; }
static int GtPressed(int action) { return ((int(__cdecl *)(int, int))0x4F84A0)(0, action) != 0; }  // GT344
static int GtFloorHit(const float p[3], float pos[4], float nrm[4])  // 1 found, 0 none, -1 no free mover ray (echo.inc's rule)
{
    __declspec(align(16)) float in[4], out[12];
    int k, freeSlot = 0;
    for (k = 0; k < 4; ++k) freeSlot |= *(const volatile int *)(0xFCEE40 + 0x30 * k) == 3;
    if (!freeSlot) return -1;
    memcpy(in, p, 12);
    in[3] = 1.0f;
    memset(out, 0, sizeof out);
    if (((int(__cdecl *)(float *, float *))0x459490)(out, in) != 1) return 0;
    memcpy(pos, out, 16), memcpy(nrm, out + 4, 16);
    return _finite(pos[0]) && _finite(pos[1]) && _finite(pos[2]);
}
static int GtArea(const float p[3])  // 0x46F5A0: the sub-area (stages 2 and 5), else 0
{
    __declspec(align(16)) float a[4];
    memcpy(a, p, 12);
    a[3] = 1.0f;
    return ((int(__cdecl *)(float *))0x46F5A0)(a);
}
// v3.49 GT349: does the warp from her feet (g_gtHerAt) to p skip or land in a walk-in event zone?
static int GtZoneBlocks(const float p[3]) { return GtZonesHit(g_gtZone, g_gtZoneN, g_gtHerAt, p) >= 0; }
static int GtProveSpot(const float hp[3]);  // v3.52 TP352 (after the world's queries below)
// Spot candidate c (xz; y = the anchor's floor) seen from the anchor `from` (x, its floor y, z: the enemy, or v3.44
// GT344 her own place for the warp ahead; areaFrom = its sub-area): GTS_OK (out = the floor point) or why not
// v3.52 TP352: + the landing proof (GTS_PENDING: the floor map is not finished; out holds the floor point then too)
static int GtSpotTest(const float from[3], int areaFrom, const float c[3], const float (*others)[3], int nOthers, float out[3])
{
    __declspec(align(16)) float p[4], hp[4], hn[4];
    float ec[3], sc[3];
    int r;
    if (!GtClearOf(c, others, nOthers)) return GTS_ENEMY;
    if (g_gtBoxOk && !GtInBox(g_gtBoxLo, g_gtBoxHi, c)) return GTS_BOX;  // GT343-001: the room's own edges first
    r = GtExitTest(g_gtExit, g_gtExitN, from, c);
    if (r != GTX_OK) return r == GTX_NEAR ? GTS_DOOR_NEAR : r == GTX_BEYOND ? GTS_DOOR_BEYOND : GTS_DOOR_CROSS;
    if (GtZoneBlocks(c)) return GTS_EVENT;  // v3.49 GT349: her path from where she stands, not the anchor's
    ec[0] = from[0], ec[1] = from[1] - ECHO_CHEST_DY, ec[2] = from[2];
    sc[0] = c[0], sc[1] = from[1] - ECHO_CHEST_DY, sc[2] = c[2];
    if (!EchoClear(ec, sc)) return GTS_SIGHT;  // the spot is on the anchor's side of every wall
    p[0] = c[0], p[1] = from[1] - 4.0f, p[2] = c[2], p[3] = 1.0f;
    if (!((int(__cdecl *)(float *))0x46F700)(p)) return GTS_WALL;
    p[1] = from[1] - 60.0f;
    if ((r = GtFloorHit(p, hp, hn)) < 0) return GTS_RAY;
    if (!r) return GTS_FLOOR;
    if (fabsf(hp[1] - from[1]) > GT_FLOOR_TOL) return GTS_LEDGE;
    if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hp, hn, 60.0f, 1.0f) != 2) return GTS_ROOM;
    if (GtArea(hp) != areaFrom || GtArea(hp) != g_gtAreaHer) return GTS_AREA;  // GT343-001: + hers (stages 2 / 5)
    if (g_gtBoxOk && !GtInBox(g_gtBoxLo, g_gtBoxHi, hp)) return GTS_BOX;  // (the floor point itself, again)
    r = GtExitTest(g_gtExit, g_gtExitN, from, hp);
    if (r != GTX_OK) return r == GTX_NEAR ? GTS_DOOR_NEAR : r == GTX_BEYOND ? GTS_DOOR_BEYOND : GTS_DOOR_CROSS;
    if (GtZoneBlocks(hp)) return GTS_EVENT;  // v3.49 GT349: (the floor point itself, again)
    out[0] = hp[0], out[1] = hp[1], out[2] = hp[2];
    return GtProveSpot(hp);  // v3.52 TP352: her body and the room's floor map (GTS_OK, or body / route / pending / proof)
}
// v3.49 GT349: this room's walk-in event zones that the game would still fire, read like EventObjectsTick 0x4D00F0
// reads them (gEventObjList 0x13D8200: +0 record, +4 task, +0x0C next): no use-item (0x4CF090), its record not running
// (EventIsRecordRunning 0x4CFFC0), enabled by the game's last test (ActorIsEnabled 0x52B1C0), actor type 7 (examine)
// or 9 (trigger) (ActorGetType 0x52B230), its zone (task + 0x424: +0 pos, +0x10 shape, +0x14 flags, +0x18 heading,
// +0x20 size) a cylinder, box or sphere without flag 4 (needs the action button) or 0x10 (inverted). Read-only; the
// calls are the ones the game makes every tick. 0 = complete; else the list could not be read whole (no warp).
static int GtZonesRead(void)
{
    typedef int(__cdecl * GtFn1)(const void *);
    const BYTE *o = *(const BYTE *const volatile *)0x13D8200;
    int guard = 0;
    g_gtZoneN = 0, g_gtZoneSkipped = 0;
    for (; o; o = *(const BYTE *const *)(o + 0x0C)) {
        const BYTE *task, *rec, *z;
        int type, shape, flags, armed;
        if ((ULONG_PTR)o < 0x10000 || ++guard > 512) return 1;  // a broken list
        task = *(const BYTE *const *)(o + 4), rec = *(const BYTE *const *)o;
        if ((ULONG_PTR)task < 0x10000 || !TaskSlotOk(task)) continue;
        z = *(const BYTE *const *)(task + 0x424);
        if ((ULONG_PTR)z < 0x10000) continue;                       // no zone: nothing a walk fires
        type = ((GtFn1)0x52B230)(task);
        shape = *(const int *)(z + 0x10), flags = *(const int *)(z + 0x14);
        // v3.49 GT349-001: the filter is teleport_sim.h GtZoneArmed (use-item, running, enabled: the game's own calls)
        if ((armed = GtZoneArmed(((GtFn1)0x4CF090)(task) != 0, ((GtFn1)0x4CFFC0)(rec) != 0, ((GtFn1)0x52B1C0)(task) != 0,
                                 type, shape, flags)) <= 0) {
            if (armed < 0) ++g_gtZoneSkipped;                       // view / always / door zones, a press, inverted
            continue;
        }
        if (g_gtZoneN >= GT_ZONE_MAX) return 2;                     // more than can be checked
        {
            GtZone *g = &g_gtZone[g_gtZoneN];
            const ULONG_PTR rp = (ULONG_PTR)rec & 0x4FFFFFFFu;  // EVENT_PTR
            g->x = *(const float *)(z + 0x00), g->y = *(const float *)(z + 0x04), g->z = *(const float *)(z + 0x08);
            g->shape = shape, g->heading = *(const float *)(z + 0x18);
            g->sx = *(const float *)(z + 0x20), g->sy = *(const float *)(z + 0x24), g->sz = *(const float *)(z + 0x28);
            g_gtZoneKey[g_gtZoneN] = rp >= 0x400000u && rp < 0x13E0000u ? (int)(*(const DWORD *)rp & 0xFFF) : -1;
            ++g_gtZoneN;
        }
    }
    return 0;
}
static void GtZonesSay(void)  // once a room (and again when the count changes): the zones a warp may not cross
{
    const DWORD key = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF) | (DWORD)g_gtZoneN << 24;
    char list[200];
    int k, o = 0;
    if (key == g_gtZoneSaid || !LogQuota(&g_gtQZone, 60)) return;
    g_gtZoneSaid = key;
    list[0] = 0;
    for (k = 0; k < g_gtZoneN && k < 4 && o < (int)sizeof list - 60; ++k) {
        const GtZone *z = &g_gtZone[k];
        if (z->shape == 2)
            o += sprintf_s(list + o, sizeof list - (size_t)o, "%skey %03X box at (%.0f, %.0f) %.0f x %.0f", o ? "; " : "",
                           g_gtZoneKey[k] & 0xFFF, z->x, z->z, z->sx, z->sz);
        else
            o += sprintf_s(list + o, sizeof list - (size_t)o, "%skey %03X %s at (%.0f, %.0f) r %.0f", o ? "; " : "",
                           g_gtZoneKey[k] & 0xFFF, z->shape == 1 ? "cylinder" : "sphere", z->x, z->z, z->sx);
    }
    LogV("ghost teleport: room %d:%d: %d walk-in event zone(s) the game would still fire: no warp across or into them%s%s%s "
         "(%d other zone(s) need a press or a look)", *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94, g_gtZoneN,
         g_gtZoneN ? " (" : "", list, g_gtZoneN > 4 ? ", ...)" : g_gtZoneN ? ")" : "", g_gtZoneSkipped);
}
// GT343-001: this room's door lines (ExitGetNum 0x4FA040 / ExitGet 0x4FA070 with gGame.room, as the game's own exit
// checks 0x4F06D0 / 0x46FB50 ask them; Exit: +0 x0, +8 z0, +0x10 x1, +0x14 y, +0x18 z1, +0x20 pos on the room's side)
// and its collision box (0x561A70: out[0] = max, out[1] = min, written only with a tree; No Clip's NwBox)
// GT343-001c: returns GTB_OK only when all of it is complete and valid (GtBoundsVerdict); else the warp is refused
static int GtRoomRead(const float pp[3])
{
    const int room = *(volatile const int *)0xFD5A94;
    const int roomOk = room >= 1 && room < 0x100;
    __declspec(align(16)) float box[8];
    int i, n, k;
    g_gtExitN = 0, g_gtBoxOk = 0;
    n = roomOk ? ((int(__cdecl *)(unsigned))0x4FA040)((unsigned)room) : 0;
    g_gtExitAsked = n;
    for (i = 0; i < n && g_gtExitN < GT_EXIT_MAX; ++i) {
        const BYTE *x = ((const BYTE *(__cdecl *)(unsigned, unsigned))0x4FA070)((unsigned)room, (unsigned)i);
        GtExit *e = &g_gtExit[g_gtExitN];
        if (!x) continue;  // (counted as invalid: nValid < n)
        e->x0 = *(const float *)(x + 0x00), e->z0 = *(const float *)(x + 0x08);
        e->x1 = *(const float *)(x + 0x10), e->y = *(const float *)(x + 0x14), e->z1 = *(const float *)(x + 0x18);
        e->px = *(const float *)(x + 0x20), e->pz = *(const float *)(x + 0x28);
        if (_finite(e->x0) && _finite(e->z0) && _finite(e->x1) && _finite(e->z1) && _finite(e->y) && _finite(e->px) && _finite(e->pz))
            ++g_gtExitN;
    }
    for (k = 0; k < 8; ++k) box[k] = 1e30f;
    ((void(__cdecl *)(float *))0x561A70)(box);
    g_gtBoxOk = box[0] < 1e29f && box[4] < 1e29f;
    for (k = 0; k < 3 && g_gtBoxOk; ++k) {
        const float a = box[4 + k], b = box[k];
        g_gtBoxLo[k] = a < b ? a : b, g_gtBoxHi[k] = a < b ? b : a;
        if (!(g_gtBoxHi[k] - g_gtBoxLo[k] > 1.0f) || !(g_gtBoxHi[k] - g_gtBoxLo[k] < 1e6f)) g_gtBoxOk = 0;
    }
    g_gtAreaHer = GtArea(pp);
    return GtBoundsVerdict(roomOk, n, g_gtExitN, g_gtBoxOk, g_gtBoxOk && GtInBox(g_gtBoxLo, g_gtBoxHi, pp));
}

// ---------------------------------------------------------------- v3.44 GT344: Richard's see-through fade on her model
#define GT_REC_LO 0x1093AF0u                    // gActorRecords: 300 records of 0xB0 bytes
#define GT_REC_HI (0x1093AF0u + 300u * 0xB0u)
static volatile float g_gtSee;  // the transparency her draw takes now (0 = stock; GhostSetAlpha's value for Richard)
static int g_gtFadeOk;          // the draw side's game code is stock (GtInstall)
static BYTE *g_gtRec[2];        // the actor records our pre / post draw sits in (0 her body, 1 her weapon)
static int g_gtPreOn[2];        // our pre-draw set the ghost's states for this draw (its post-draw puts them back)
static int g_gtDrawn[2];        // draws made see-through in this fade (body, weapon)
static int g_gtBusySaid[2];     // a record's slots were not free (said once a fade)
static volatile LONG g_gtDrawFault;
static void __cdecl GtPreDraw(BYTE *task);
static void __cdecl GtPostDraw(BYTE *task);
static int GtDrawWhich(const BYTE *task)  // 0 her body's task, 1 her weapon's, -1 neither
{
    if (!task) return -1;
    if (task == *(BYTE *const volatile *)(0x10A0F20 + 0x210)) return 0;  // gPlayer[0].unk_210: ActorSetModel(.., model)
    if (task == *(BYTE *const volatile *)(0x10A0F20 + 0x344)) return 1;  // weapon_task
    return -1;
}
static BYTE *GtRecOf(int k)  // her body's (0) or weapon's (1) actor record, validated, else NULL
{
    const BYTE *t = *(BYTE *const volatile *)(0x10A0F20 + (k ? 0x344 : 0x210));
    BYTE *r;
    if (!t || !TaskSlotOk(t)) return NULL;
    r = *(BYTE *const *)(t + 0x41C);
    if ((DWORD)(size_t)r < GT_REC_LO || (DWORD)(size_t)r >= GT_REC_HI || ((DWORD)(size_t)r - GT_REC_LO) % 0xB0u) return NULL;
    return *(const BYTE *const *)(r + 0x10) == t ? r : NULL;
}
static void __cdecl GtPreDraw(BYTE *task)  // ActorDrawModels: before her model (the ghost's 0x45C7A0 with her FIX)
{
    const int k = GtDrawWhich(task);
    float see = g_gtSee, c[4];
    int fix;
    void *dev;
    if (k < 0 || !g_gtFadeOk || g_gtFault || !(see > 0.0f)) return;
    dev = DEV;
    if (!dev) return;
    fix = GtFadeFix(see);
    c[0] = c[1] = c[2] = c[3] = (float)fix * 0.0078125f;
    g_gtPreOn[k] = 1;
    __try {
        ((void(__cdecl *)(int))0x404B80)(4);
        ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, 1);
        ((void(__cdecl *)(unsigned __int64))0x429ED0)(((unsigned __int64)(unsigned)fix << 32) | 0x64u);
        ((void(__cdecl *)(unsigned __int64))0x429F00)((unsigned __int64)0x53001u);
        ((void(__cdecl *)(int))0x429F60)(1);
        ++g_gtDrawn[k];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_gtFadeOk = 0;  // fail closed: no fade for this session (the tick logs it and takes ours out)
        InterlockedExchange(&g_gtDrawFault, (LONG)GetExceptionCode());
    }
}
static void __cdecl GtPostDraw(BYTE *task)  // after her model: the ghost's own post-draw 0x507240
{
    const int k = GtDrawWhich(task);
    if (k < 0 || !g_gtPreOn[k]) return;
    g_gtPreOn[k] = 0;
    __try {
        ((void(__cdecl *)(BYTE *))0x507240)(task);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_gtFadeOk = 0;
        InterlockedExchange(&g_gtDrawFault, (LONG)GetExceptionCode());
    }
}
// Every tick: her records carry our pre / post draw while see > 0, else not. Only empty slots are taken; ours are taken
// out only while still ours (a record freed and reused at a room change is cleared by ActorRecordAlloc 0x52B2C0)
static void GtDrawSync(float see)
{
    int k;
    g_gtSee = see > 0.0f && g_gtFadeOk && !g_gtFault ? see : 0.0f;
    for (k = 0; k < 2; ++k) {
        BYTE *want = g_gtSee > 0.0f ? GtRecOf(k) : NULL, *r = g_gtRec[k];
        if (r && r != want) {
            if (*(void *volatile *)(r + 0x24) == (void *)GtPreDraw) *(void *volatile *)(r + 0x24) = NULL;
            if (*(void *volatile *)(r + 0x28) == (void *)GtPostDraw) *(void *volatile *)(r + 0x28) = NULL;
            g_gtRec[k] = NULL, g_gtPreOn[k] = 0;
        }
        if (want && !g_gtRec[k]) {
            void *pre = *(void *volatile *)(want + 0x24), *post = *(void *volatile *)(want + 0x28);
            if (!pre && !post) {
                *(void *volatile *)(want + 0x28) = (void *)GtPostDraw;
                *(void *volatile *)(want + 0x24) = (void *)GtPreDraw;
                g_gtRec[k] = want;
            } else if (!g_gtBusySaid[k]) {
                g_gtBusySaid[k] = 1;
                if (LogQuota(&g_gtQDraw, 20))
                    LogV("ghost teleport: her %s's draw already has a pre / post draw (%p / %p: a scene or another mod): not "
                         "faded", k ? "weapon" : "body", pre, post);
            }
        }
    }
}
static void GtDrawOff(void)  // (also from the fault handler)
{
    __try {
        GtDrawSync(0.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_gtSee = 0.0f;
    }
}
static void GtFadeBegin(void) { g_gtDrawn[0] = g_gtDrawn[1] = 0, g_gtBusySaid[0] = g_gtBusySaid[1] = 0; }
static void GtFadeEnded(void)  // the fade is over: how often it was drawn (an anomaly if never)
{
    if (g_gtHold.on && g_gtHold.warps > GT_HOLD_LOGN)  // v3.49 GT349: a long hold: per tick only
        LogT("ghost teleport: Richard's fade ended (drawn see-through on %d body and %d weapon draw(s))", g_gtDrawn[0], g_gtDrawn[1]);
    else
        LogV("ghost teleport: Richard's fade ended (drawn see-through on %d body and %d weapon draw(s))", g_gtDrawn[0], g_gtDrawn[1]);
    if (g_gtFadeOk && !g_gtDrawn[0] && LogQuota(&g_gtQAn, 10))
        LogAnomaly("ghost teleport", "the fade ran but her body was never drawn see-through (record %p)", (void *)GtRecOf(0));
}

// v3.43 GT343 Echo Doubles' teleport smoke: gone in v3.44 GT344 (Richard's warp has no smoke); echo.inc still asks
static int GtSmokeLive(void) { return 0; }
static int GtSmokeList(int look, EchoSpr *o, int n, int max)
{
    (void)look, (void)o, (void)max;
    return n;
}

// ---------------------------------------------------------------- a warp
// The refusals before the search (why = the reason; 0 = refused); *floorY = the floor under her
static int GtCanGo(const float pp[3], float *floorY, char *why, size_t nw)
{
    const BYTE *P = (const BYTE *)0x10A0F20;
    const DWORD stage = *(volatile DWORD *)0xFD5A90;
    const int mode = *(volatile const int *)(P + 0x214);
    float fl;
    if (stage == 1 || stage == 0x0A) return sprintf_s(why, nw, "Room 302"), 0;
    if (*(void *volatile *)0xFD5BA0 != (void *)P) return sprintf_s(why, nw, "the game is not running the player's own update"), 0;
    if (*(volatile int *)0x10A1914 || *(volatile int *)0xFD5BB0 || *(volatile int *)0x10835D8)
        return sprintf_s(why, nw, "the game holds the controls (an event, a door, a look)"), 0;
    // v3.49 GT349: never across a door transition or a cut-scene's start (CRASH_ANALYSIS.md)
    if (*(void *volatile *)0xFD5C3C) return sprintf_s(why, nw, "a door is in use"), 0;  // gGame.exit
    if (*(volatile int *)0xFD5AA0 || *(volatile int *)0xFD5AA4)
        return sprintf_s(why, nw, "a room change is pending (room %d:%d asked)", *(volatile int *)0xFD5AA0, *(volatile int *)0xFD5AA4), 0;
    if (*(volatile int *)0x10A1E98) return sprintf_s(why, nw, "a script holds the player's update"), 0;
    if (*(volatile int *)0xFD5C28) return sprintf_s(why, nw, "an interaction event runs"), 0;
    if (*(volatile int *)0x137F98C || *(volatile int *)0x137F990)
        return sprintf_s(why, nw, "the screen is fading (a door or a scene; level %d, to %d)", *(volatile int *)0x137F98C,
                         *(volatile int *)0x137F990), 0;
    // v3.43 REV343 GT343-002: [0xFD6C78] gRoomModels.drawn is NONZERO in ordinary play (0 = the room is not drawn: a
    // peep / look / load); the old test refused every attempt in play
    if (!*(volatile int *)0xFD6C78 || *(volatile int *)0xFD5BE8 || *(volatile int *)0x10A12FC)
        return sprintf_s(why, nw, "the room is not drawn, a peep, a room change or a move of the player is on"), 0;
    if (((int(__cdecl *)(void))0x463F50)()) return sprintf_s(why, nw, "a menu is opening"), 0;
    if (mode != 0) return sprintf_s(why, nw, "she is busy (mode %X: weapon raised, an attack, a hit, a dodge or a scripted state)", mode), 0;
    if (*(volatile const int *)(P + 0x220) || *(volatile const int *)(P + 0x228))
        return sprintf_s(why, nw, "she is hit or held (hit phase %d, grab %d)", *(volatile const int *)(P + 0x220), *(volatile const int *)(P + 0x228)), 0;
    {
        float q[3];
        memcpy(q, pp, sizeof q);
        q[1] -= 40.0f;
        if (!EchoFloorY(q, &fl)) return sprintf_s(why, nw, "no floor found under her"), 0;
        if (fabsf(fl - pp[1]) > 50.0f) return sprintf_s(why, nw, "she is not on the floor (%+.0f u: a fall?)", fl - pp[1]), 0;
    }
    *floorY = fl;
    return 1;
}
static int GtListMore(int *guard, int nf)  // GT343-004: 0 = stop (only a list longer than GT_ACTOR_MAX: an anomaly)
{
    if (++*guard <= GT_ACTOR_MAX && nf < GT_ACTOR_MAX) return 1;
    if (LogQuota(&g_gtQAn, 10)) LogAnomaly("ghost teleport", "the enemy list is longer than %d: the rest is not looked at", GT_ACTOR_MAX);
    return 0;
}
static const char *GtWhyList(const int *cnt, char *list, size_t nl)  // "reason n, reason n" of the refused spots
{
    int k, o = 0;
    list[0] = 0;
    for (k = 1; k < GTS_N; ++k)
        if (cnt[k] && o < (int)nl - 48) o += sprintf_s(list + o, nl - (size_t)o, "%s%s %d", o ? ", " : "", kGtWhy[k], cnt[k]);
    return list;
}
// ---------------------------------------------------------------- v3.50 TP350: through walls on the room's collision
// teleport_sim.h's world: No Clip's queries (noclip_world.h; game thread, its aligned buffers)
static int GtWFloor(void *c, float x, float yTop, float yBot, float z, float *fy) { return NwFloor(c, x, yTop, yBot, z, fy); }
static int GtWSphere(void *c, float x, float y, float z, float r) { (void)c; return NwSphereCount(x, y, z, r); }
static int GtWSolid(void *c, float x, float y, float z) { return NwSolid(c, x, y, z); }
static int GtWLine(void *c, const float a[3], const float b[3])  // 0x561CD0: the line enters solid (tree2, the event polygons)
{
    (void)c;
    g_nwLine[0] = a[0], g_nwLine[1] = a[1], g_nwLine[2] = a[2], g_nwLine[3] = 1.0f;
    g_nwLine[4] = b[0], g_nwLine[5] = b[1], g_nwLine[6] = b[2], g_nwLine[7] = 1.0f;
    return ((int(__cdecl *)(float *, float *))0x561CD0)(g_nwLine, g_nwPlane) != 0;
}
static const GtWorld g_gtWorld = { NULL, GtWFloor, GtWSphere, GtWLine, GtWSolid };
static int GtRayFree(void)  // a free mover ray (0x459490 / 0x458F00 / 0x454000 take one; echo.inc's rule)
{
    int k, n = 0;
    for (k = 0; k < 4; ++k) n |= *(const volatile int *)(0xFCEE40 + 0x30 * k) == 3;
    return n;
}
static DWORD GtRoomKeyNow(void) { return *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF); }
// The room's floor map (teleport_sim.h GtMap): begun at this room visit's first through-warp search (the room's box and
// door lines from GtRoomRead, every door's point a seed), her feet a seed at every search, `cells` more expanded now
static void GtMapGrow(int cells);
static void GtMapEnsure(const float pp[3], float herFloor, int cells)
{
    const DWORD key = GtRoomKeyNow();
    int k;
    if (key != g_gtMapKey) {
        GtMapBegin(&g_gtMap, g_gtBoxLo, g_gtBoxHi, g_gtExit, g_gtExitN);
        for (k = 0; k < g_gtExitN; ++k)  // every door's point on the room's side
            GtMapSeed(&g_gtMap, &g_gtWorld, g_gtExit[k].px, g_gtExit[k].y, g_gtExit[k].pz, GT_MAP_SEED_DY);
        g_gtMapKey = key, g_gtMapMs = 0.0, g_gtMapSaid = 0;
        if (LogQuota(&g_gtQMap, 200))
            LogV("ghost teleport: room %d:%d: the floor map begins (a grid of %d x %d cells of %.0f u on the room's box; %d of %d "
                 "door(s) seeded, %d without a floor there, %d inside collision)%s", *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94,
                 g_gtMap.nx, g_gtMap.ny, g_gtMap.cell, g_gtMap.seeds, g_gtExitN, g_gtMap.seedsNoFloor, g_gtMap.seedsBlocked,
                 g_gtMap.on ? "" : ": NO MAP (the box)");  // v3.52 TP352: + inside collision
    }
    if (!g_gtMap.on) return;
    GtMapSeed(&g_gtMap, &g_gtWorld, pp[0], herFloor, pp[2], GT_REACH_UP);  // where she stands is the room's floor
    GtMapGrow(cells);
}
static void GtMapGrow(int cells)  // up to `cells` more of this room's floor map (timed; its finish or overflow logged once)
{
    LARGE_INTEGER a, b, f;
    if (!g_gtMap.on || GtMapFinished(&g_gtMap) || g_gtMap.overflow || cells <= 0) return;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    GtMapRun(&g_gtMap, &g_gtWorld, cells);
    QueryPerformanceCounter(&b);
    g_gtMapMs += f.QuadPart ? (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart : 0.0;
    if (g_gtMap.overflow && g_gtMapSaid < 3) {
        g_gtMapSaid = 3;
        if (LogQuota(&g_gtQAn, 10))
            LogAnomaly("ghost teleport", "room %d:%d: the floor map's queue overflowed at %d cells: spots it does not hold are "
                       "refused", *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94, g_gtMap.cells);
    } else if (GtMapFinished(&g_gtMap) && g_gtMapSaid < 2) {
        g_gtMapSaid = 2;
        if (LogQuota(&g_gtQMap, 200))
            LogV("ghost teleport: room %d:%d: the floor map is finished: %d cell(s) a route of the room reaches, from %d seed(s) "
                 "(%d without a floor); %.1f ms over %d run(s)%s; %d seed(s) inside collision refused", *(volatile int *)0xFD5A90,
                 *(volatile int *)0xFD5A94, g_gtMap.cells, g_gtMap.seeds, g_gtMap.seedsNoFloor, g_gtMapMs, g_gtMap.runs,
                 g_gtMap.unknown ? " (some queries could not answer: blocked)" : "", g_gtMap.seedsBlocked);  // v3.52 TP352: + the seeds refused
    }
}
// v3.52 TP352: the landing proof of every kind (teleport_sim.h GtLandProof on this room's floor map): GTS_OK, GTS_BODY,
// GTS_ROUTE, GTS_PENDING (the map is not finished) or GTS_PROOF (no map for this room: this search could not build one)
static int GtProveSpot(const float hp[3])
{
    static const int kWhy[GTP_N] = { GTS_OK, GTS_BODY, GTS_ROUTE, GTS_PENDING, GTS_PROOF };
    int r;
    if (!g_gtProofOk || g_gtMapKey != GtRoomKeyNow()) return GTS_PROOF;
    r = GtLandProof(&g_gtMap, &g_gtWorld, hp);
    return r >= 0 && r < GTP_N ? kWhy[r] : GTS_PROOF;
}
static int GtThruOpenCb(void *c, const float p[3])  // could she walk straight to p?
{
    (void)c;
    return g_gtBoxOk && GtInBox(g_gtBoxLo, g_gtBoxHi, p) && GtThruOpen(&g_gtWorld, g_gtThruFrom, p);
}
// A sample past an obstacle: GTS_OK (out = the floor point) or why not. v3.49's spot rules from her own place without the
// line of sight (her feet g_gtThruFrom, on her floor), her body, the room's floor map
static int GtThruLandCb(void *c, const float p[3], float out[3])
{
    __declspec(align(16)) float q[4], hp[4], hn[4];
    const float *from = g_gtThruFrom;
    int r;
    (void)c;
    if (!GtClearOf(p, g_gtOthersP, g_gtNOthers)) return GTS_ENEMY;
    if (!g_gtBoxOk || !GtInBox(g_gtBoxLo, g_gtBoxHi, p)) return GTS_BOX;
    r = GtExitTest(g_gtExit, g_gtExitN, from, p);
    if (r != GTX_OK) return r == GTX_NEAR ? GTS_DOOR_NEAR : r == GTX_BEYOND ? GTS_DOOR_BEYOND : GTS_DOOR_CROSS;
    if (GtZoneBlocks(p)) return GTS_EVENT;  // the straight segment from her feet
    q[0] = p[0], q[1] = from[1] - GT_REACH_LOW, q[2] = p[2], q[3] = 1.0f;
    if (!((int(__cdecl *)(float *))0x46F700)(q)) return GTS_WALL;
    if ((r = GtFloorHit(q, hp, hn)) < 0) return GTS_RAY;
    if (!r) return GTS_FLOOR;
    if (fabsf(hp[1] - from[1]) > GT_FLOOR_TOL) return GTS_LEDGE;
    if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hp, hn, 60.0f, 1.0f) != 2) return GTS_ROOM;
    if (GtArea(hp) != g_gtAreaHer) return GTS_AREA;
    if (!GtInBox(g_gtBoxLo, g_gtBoxHi, hp) || !(hp[1] >= g_gtBoxLo[1] && hp[1] <= g_gtBoxHi[1])) return GTS_BOX;
    r = GtExitTest(g_gtExit, g_gtExitN, from, hp);
    if (r != GTX_OK) return r == GTX_NEAR ? GTS_DOOR_NEAR : r == GTX_BEYOND ? GTS_DOOR_BEYOND : GTS_DOOR_CROSS;
    if (GtZoneBlocks(hp)) return GTS_EVENT;
    out[0] = hp[0], out[1] = hp[1], out[2] = hp[2];
    return GtProveSpot(hp);  // v3.52 TP352: the one landing proof (was: GtBodyFree, then GtMapHas here)
}
// 1 = *pl holds a spot past an obstacle (kind 2), 2 = one the map does not hold yet (at the press only), 0 = none (tw why)
static int GtThruFind(const float pp[3], float herFloor, float yaw, GtPlan *pl, int atWarp, char *tw, size_t ntw)
{
    GtThruFns fn;
    GtThru t;
    int cnt[GTS_N], nObst = 0, r;
    char list[200];
    tw[0] = 0;
    if (!g_gtThruOn || !g_gtWorldOk)
        return _snprintf_s(tw, ntw, _TRUNCATE, "through walls off%s", g_gtThruOn ? " (its queries are not stock)" : ""), 0;
    if (!NwHasTree2()) return _snprintf_s(tw, ntw, _TRUNCATE, "through walls: the room has no solid map"), 0;
    if (!GtRayFree()) return _snprintf_s(tw, ntw, _TRUNCATE, "through walls: no free mover ray now"), 0;
    g_gtThruFrom[0] = pp[0], g_gtThruFrom[1] = herFloor, g_gtThruFrom[2] = pp[2];
    // v3.52 TP352: the room's floor map is begun / grown by GtFind for every kind (GtMapEnsure there), not here
    memset(cnt, 0, sizeof cnt);
    memset(&t, 0, sizeof t);
    fn.ctx = NULL, fn.open = GtThruOpenCb, fn.land = GtThruLandCb, fn.pending = atWarp ? 0 : GTS_PENDING;
    r = GtThruSearch(&fn, g_gtThruFrom, yaw, g_gtFwd, &t, cnt, GTS_N, &nObst);
    if (r) {
        pl->kind = 2, pl->yaw = t.yaw, pl->d = t.d, pl->ray = t.ray, pl->kObst = t.kObst, pl->dObst = t.dObst;
        pl->refusedBefore = t.refused, pl->pending = r == 2;
        memcpy(pl->at, t.at, sizeof pl->at);
        return r;
    }
    _snprintf_s(tw, ntw, _TRUNCATE, "through walls: %d of %d ray(s) met an obstacle within %.0f u%s%s; the room's floor map %d "
                "cell(s)%s", nObst, GT_FAN_N, g_gtFwd, nObst ? ", past it: " : "", nObst ? (GtWhyList(cnt, list, sizeof list)[0] ?
                list : "nothing to try") : "", g_gtMap.cells, GtMapFinished(&g_gtMap) ? "" : " (not finished)");
    return 0;
}
// ---------------------------------------------------------------- v3.51 TP351: the faced enemy must be in the open
// The line test's collision (teleport_sim.h GtFoeSight): the through-warp's tree2 queries (0x561CD0 line, 0x561CB0 inside
// solid) when they are stock and the room has a solid map; else the mover line 0x454000 (EchoClear, v3.43's spot test;
// it takes a free mover ray: none = cannot answer = skipped) without the inside-solid test
static int GtWLineMover(void *c, const float a[3], const float b[3]) { (void)c; return !GtRayFree() ? -1 : !EchoClear(a, b); }
static int GtWSolidNone(void *c, float x, float y, float z) { (void)c, (void)x, (void)y, (void)z; return 0; }
static const GtWorld g_gtWorldMover = { NULL, GtWFloor, GtWSphere, GtWLineMover, GtWSolidNone };
// A victim or story ghost (types 0B .. 0E) in its wall state: the ghost framework's own wall portal is open (GhostWork =
// work + 0x28, its +0x3C = the enemy's task, +0x50 the portal's work that HauntingGhostObjectOpen 0x482B70 makes and
// HauntingGhostObjectClose 0x482C90 drops; the portal's +0xEC open 0 .. 1, KB 113; fadein.inc FiGhostPortal reads the same).
// Not the arriving placeholder (task 0x486030: no ghost block there).
static int GtFoeInWall(const GtFoe *f)
{
    const BYTE *g, *pw;
    float open;
    // v3.51 TP351-001 (Codex): Richard (0F) too; a ghost without the shared GhostWork layout fails the +0x3C check below
    if (f->type < 0x0B || f->type > 0x0F || *(const DWORD *)(f->t + 0x404) == 0x486030) return 0;
    g = *(const BYTE *const *)(f->w + 0x28);
    if ((ULONG_PTR)g < 0x10000 || *(const BYTE *const *)(g + 0x3C) != f->t) return 0;
    pw = *(const BYTE *const *)(g + 0x50);
    if ((ULONG_PTR)pw < 0x10000) return 0;
    open = *(const float *)(pw + 0xEC);
    return _finite(open) && open > 0.02f;
}
// GtPickFacedVia's test of a pick: in this room (v3.50: not beyond a door line, inside the box), then (v3.51) in the open
typedef struct { GtFoe *foe; const GtCand *cand; float her[3]; int *cnt; GtPlan *pl; const GtWorld *w; } GtFaceCtx;
static int GtFaceTest(void *ctx, int i)
{
    GtFaceCtx *x = (GtFaceCtx *)ctx;
    GtFoe *f = &x->foe[i];
    float e[3], fq[3];
    int v;
    memcpy(e, f->pos, sizeof e);
    e[1] -= 40.0f;
    if (!EchoFloorY(e, &f->floorY)) f->floorY = f->pos[1];
    fq[0] = f->pos[0], fq[1] = f->floorY, fq[2] = f->pos[2];
    if (GtExitTest(g_gtExit, g_gtExitN, NULL, fq) == GTX_BEYOND || !GtInBox(g_gtBoxLo, g_gtBoxHi, fq)) {
        ++x->cnt[GTS_FOE_DOOR];
        return 1;
    }
    if (g_gtBehindColl) return 0;  // [Gameplay] GhostTeleportBehindCollision 1: v3.50's rule
    if ((v = GtFoeSight(x->w, x->her, fq, GtFoeInWall(f))) == GTV_CLEAR) return 0;
    if (!x->pl->nSkip++)
        x->pl->skipWhy = v, x->pl->skipType = f->type & 0xFF, x->pl->skipD = x->cand[i].d, x->pl->skipAng = x->cand[i].ang / GT_DEG;
    LogT("ghost teleport: type %02X %.0f u, %.0f degrees off her facing: behind collision (%s): not the enemy she faces",
         f->type & 0xFF, x->cand[i].d, x->cand[i].ang / GT_DEG, kGtSightWhy[v]);
    return 2;
}
// "; N enemy(ies) in the cone skipped: behind collision (the first: why; type XX N u, N degrees off)", or ""
static const char *GtSkipSay(const GtPlan *pl, char *b, size_t nb)
{
    b[0] = 0;
    if (pl->nSkip > 0)
        _snprintf_s(b, nb, _TRUNCATE, "; %d enemy(ies) in the cone skipped: behind collision (the first: %s; type %02X %.0f u, "
                    "%.0f degrees off)", pl->nSkip, kGtSightWhy[pl->skipWhy >= 0 && pl->skipWhy < GTV_N ? pl->skipWhy : GTV_UNKNOWN],
                    pl->skipType, pl->skipD, pl->skipAng);
    return b;
}
static void GtSkipCount(const GtPlan *pl)  // the try's decision was made: the room visit counts its skips
{
    if (pl->nSkip > 0) g_gtRoomSkip += pl->nSkip, ++g_gtRoomSkipTries;
}
// v3.44 GT344 (was v3.43's GtTry without the move): the target now. 1 = *pl holds it; 0 = why says why not
// v3.50 TP350: *kind = the refusal's kind (GTN_*); atWarp: the end of the fade-out (a through-spot must be proven); the
// long texts are cut to the buffer (_TRUNCATE: a sprintf_s overflow would end the game)
static int g_gtFindRead;  // v3.52 TP352: this search read the room's edges and zones and she is in none (GtFind's free)
static float g_gtFoeAt[GT_ACTOR_MAX][3];  // v3.52 TP352: every enemy's place at this search (GtFreeOkCb)
static int g_gtFoeN;
static int GtFindLanding(const float pp[3], float herFloor, GtPlan *pl, char *why, size_t nw, int atWarp, int *kind)
{
    static GtFoe foe[GT_ACTOR_MAX];  // GT343-004 (game thread only; static: 3 KB less stack per call)
    static GtCand cand[GT_ACTOR_MAX];
    static float others[GT_ACTOR_MAX][3];
    const float yaw = *(volatile const float *)0x10A0FC4;  // gPlayer[0].rot.y
    int nf = 0, no, k, guard = 0, cnt[GTS_N], faced;
    const BYTE *t = NULL;
    char list[240];
    memset(pl, 0, sizeof *pl);
    *kind = GTN_NONE;
    // GT343-004: the whole list, every actor kept (GT_ACTOR_MAX only bounds a broken list): a nearer one late in the
    // list (Enemy Copies) is never dropped
    while ((t = DpNext(t)) != NULL && GtListMore(&guard, nf)) {  // this room's enemies
        BYTE *w;
        float *hp;
        int type;
        const BYTE *ww = *(const BYTE *const *)(t + 0x408);
        GtFoe *f = &foe[nf];
        if (!ww) continue;
        memcpy(f->pos, ww, 12);
        if (!EchoF3(f->pos)) continue;
        f->t = t, f->w = ww, f->type = *(const int *)(ww + 0x20);
        cand[nf].ok = EchoHittable(t, &w, &hp, &type) && !EchoIsDown(t);
        cand[nf].d = sqrtf((f->pos[0] - pp[0]) * (f->pos[0] - pp[0]) + (f->pos[2] - pp[2]) * (f->pos[2] - pp[2]));
        cand[nf].dy = f->pos[1] - pp[1];
        cand[nf].ang = GtOffFacing(pp, yaw, f->pos), cand[nf].skip = 0;  // v3.50 TP350
        {
            const BYTE *hb = *(const BYTE *const *)(ww + 0x38);  // the mover's HitBody: +0x10 its bounding radius
            const float rr = hb ? *(const float *)(hb + 0x10) : 40.0f;
            f->rad = _finite(rr) ? EchoClampF(rr, 20.0f, 90.0f) : 40.0f;
        }
        ++nf;
    }
    // v3.50 TP350 A2: only the standing enemy she faces (teleport_sim.h GtPickFaced), never the nearest elsewhere
    faced = _finite(yaw) ? GtPickFaced(cand, nf, g_gtRange, g_gtCone) : -1;
    for (k = 0; k < nf; ++k) memcpy(g_gtFoeAt[k], foe[k].pos, 12);  // v3.52 TP352
    g_gtFoeN = nf;
    memset(cnt, 0, sizeof cnt);
    if ((k = GtRoomRead(pp)) != GTB_OK)  // GT343-001: the room's door lines and box; GT343-001c: complete, else no warp
        return *kind = GTN_EDGES, _snprintf_s(why, nw, _TRUNCATE, "the room's edges are not known (%s; room %d, %d of %d exit(s) "
                                              "read): no same-room check, no warp", kGtBoundsWhy[k], *(volatile const int *)0xFD5A94,
                                              g_gtExitN, g_gtExitAsked), 0;
    // v3.49 GT349: the walk-in event zones (fail closed: a list that cannot be read whole = no warp)
    memcpy(g_gtHerAt, pp, sizeof g_gtHerAt);
    if ((k = GtZonesRead()) != 0)
        return *kind = GTN_ZONES, _snprintf_s(why, nw, _TRUNCATE, "the room's event zones are not known (%s): no warp", k == 1 ?
                                              "the event object list is broken or longer than 512" : "more than 64 walk-in zones"), 0;
    GtZonesSay();
    // v3.49 GT349-001: she stands inside a walk-in zone whose event has not started (she may have run in during the
    // fade-out): it fires first; no warp out of it (at the press and again at the end of the fade-out)
    if ((k = GtZonesInside(g_gtZone, g_gtZoneN, pp)) >= 0)
        return *kind = GTN_INZONE, _snprintf_s(why, nw, _TRUNCATE, "inside an event's trigger zone (key %03X, not fired yet): no "
                                               "warp out of it", g_gtZoneKey[k] & 0xFFF), 0;
    // v3.52 TP352 (a): the landing proof's floor map for every kind (was: the through-warp's only, in GtThruFind); no solid
    // map or its queries not stock: no landing is proven (GtProveSpot: GTS_PROOF)
    g_gtProofOk = g_gtWorldOk && NwHasTree2();
    if (g_gtProofOk) GtMapEnsure(pp, herFloor, GT_MAP_PRESS);
    g_gtFindRead = 1;
    if (faced < 0 && !(g_gtFwd > 0.0f))  // (v3.52 TP352: after the room's reads, was before them)
        return _snprintf_s(why, nw, _TRUNCATE, "no standing enemy she faces within %.0f u (+-%.0f degrees; %d enemies in the room)",
                           g_gtRange, g_gtCone, nf), 0;
    // v3.50 TP350: the faced enemy must stand in this room (GT343-001: not beyond one of its door lines; inside its box);
    // v3.51 TP351: and in the open (no collision between her and it: GtFoeSight); else the next one in the cone
    if (faced >= 0) {
        GtFaceCtx fc;
        fc.foe = foe, fc.cand = cand, fc.cnt = cnt, fc.pl = pl;
        fc.her[0] = pp[0], fc.her[1] = herFloor, fc.her[2] = pp[2];
        fc.w = g_gtWorldOk && NwHasTree2() ? &g_gtWorld : &g_gtWorldMover;
        faced = GtPickFacedVia(cand, nf, g_gtRange, g_gtCone, GtFaceTest, &fc);
    }
    if (faced < 0) {  // v3.44 GT344 (c): no standing enemy she faces: the way she faces (v3.50 TP350: through walls first)
        const float from[3] = { pp[0], herFloor, pp[2] };
        const int n = GtFwdTries(g_gtFwd);
        char tw[400], sk[224];
        int r;
        GtSkipSay(pl, sk, sizeof sk);  // v3.51 TP351
        if (!_finite(yaw))
            return *kind = GTN_AHEAD, _snprintf_s(why, nw, _TRUNCATE, "no standing enemy she faces and her facing is not a number"), 0;
        if (!(g_gtFwd > 0.0f))  // v3.51 TP351: the enemies in the cone were skipped and the warp ahead is off
            return *kind = GTN_FACED, _snprintf_s(why, nw, _TRUNCATE, "no standing enemy she faces in this room and in the open, and "
                                                  "the warp ahead is off ([Gameplay] GhostTeleportForward 0; %d beyond a door line%s)",
                                                  cnt[GTS_FOE_DOOR], sk), 0;
        for (k = 0; k < nf; ++k) memcpy(others[k], foe[k].pos, 12);
        g_gtOthersP = (const float (*)[3])others, g_gtNOthers = nf;
        if ((r = GtThruFind(pp, herFloor, yaw, pl, atWarp, tw, sizeof tw)) != 0) return 1;  // (pl->pending = r == 2)
        for (k = 0; k < n; ++k) {
            float c[3];
            int rr;
            GtFwdCand(from, yaw, g_gtFwd, k, c);
            // v3.52 TP352: + the landing proof (at the press a spot the unfinished map does not hold yet starts the fade)
            if ((rr = GtSpotTest(from, g_gtAreaHer, c, (const float (*)[3])others, nf, pl->at)) != GTS_OK &&
                !(rr == GTS_PENDING && !atWarp)) {
                ++cnt[rr];
                continue;
            }
            pl->kind = 1, pl->yaw = yaw, pl->tries = k + 1, pl->nTries = n, pl->pending = rr == GTS_PENDING;
            pl->d = sqrtf((pl->at[0] - pp[0]) * (pl->at[0] - pp[0]) + (pl->at[2] - pp[2]) * (pl->at[2] - pp[2]));
            return 1;
        }
        *kind = GTN_AHEAD;
        return _snprintf_s(why, nw, _TRUNCATE, "no free floor ahead of her in this room (no standing enemy she faces within %.0f u%s; %d "
                           "tries from %.0f down to %.0f u, %d door line(s): %s; %s)", g_gtRange, sk, n, g_gtFwd, GtFwdDist(g_gtFwd, n - 1),
                           g_gtExitN, GtWhyList(cnt, list, sizeof list), tw), 0;  // v3.51 TP351: + sk
    }
    {   // v3.50 TP350 A2: behind the faced enemy only (v3.43's spot rings; its floor read above)
        GtFoe *f = &foe[faced];
        __declspec(align(16)) float q[4];
        float back[2], fq[3];
        int j, s, fromFace;
        fq[0] = f->pos[0], fq[1] = f->floorY, fq[2] = f->pos[2];
        memcpy(q, ((const float *(__cdecl *)(const BYTE *))0x45CE10)(f->t), 16);
        fromFace = GtBackDir(q, f->pos, pp, back);
        for (j = 0, no = 0; j < nf; ++j)
            if (j != faced) memcpy(others[no++], foe[j].pos, 12);
        for (s = 0; s < GT_SPOT_N; ++s) {
            float c[3];
            int r;
            GtSpotCand(fq, back, f->rad, s, c);
            // v3.52 TP352: + the landing proof (D4: warp 129's spot 2 stood in the wall's reach of her body)
            if ((r = GtSpotTest(fq, GtArea(f->pos), c, (const float (*)[3])others, no, pl->at)) != GTS_OK &&
                !(r == GTS_PENDING && !atWarp)) {
                ++cnt[r];
                continue;
            }
            pl->pending = r == GTS_PENDING;
            pl->kind = 0, pl->type = f->type & 0xFF, pl->foe = 1, pl->np = 1, pl->spot = s, pl->fromFace = fromFace;
            pl->yaw = GtFaceYaw(pl->at, f->pos), pl->d = cand[faced].d;
            pl->gap = sqrtf((pl->at[0] - f->pos[0]) * (pl->at[0] - f->pos[0]) + (pl->at[2] - f->pos[2]) * (pl->at[2] - f->pos[2]));
            pl->ang = cand[faced].ang / GT_DEG, pl->nCone = GtConeCount(cand, nf, g_gtRange, g_gtCone);
            return 1;
        }
        *kind = GTN_FACED;
        {
            char sk[224];
            return _snprintf_s(why, nw, _TRUNCATE, "no free floor behind the enemy she faces (type %02X %.0f u, %.0f degrees off her "
                               "facing; %d spots, %d door line(s): %s%s)", f->type & 0xFF, cand[faced].d, cand[faced].ang / GT_DEG,
                               GT_SPOT_N, g_gtExitN, GtWhyList(cnt, list, sizeof list), GtSkipSay(pl, sk, sizeof sk)), 0;  // v3.51 TP351: + the skips
        }
    }
}
// ---------------------------------------------------------------- v3.52 TP352: out of a wall, onto the nearest proven floor
// A cell GtFreeCell may give her (teleport_sim.h: on the room's floor map, her body free and not stuck there): the
// rules of a spot from her own place without the line of sight (she stands inside collision): the room's box, its door
// lines (not near, not beyond, not crossed from her feet g_gtHerAt), the walk-in zones on the straight path, clear of the
// enemies (g_gtFoeAt), the game's floor ray at her level, room to stand 0x458F00, her sub-area
static int GtFreeOkCb(void *c, const float p[3])
{
    __declspec(align(16)) float q[4], hp[4], hn[4];
    (void)c;
    if (!g_gtBoxOk || !GtInBox(g_gtBoxLo, g_gtBoxHi, p) || !GtClearOf(p, (const float (*)[3])g_gtFoeAt, g_gtFoeN)) return 0;
    if (GtExitTest(g_gtExit, g_gtExitN, g_gtHerAt, p) != GTX_OK || GtZoneBlocks(p)) return 0;
    q[0] = p[0], q[1] = p[1] - GT_REACH_LOW, q[2] = p[2], q[3] = 1.0f;
    if (GtFloorHit(q, hp, hn) <= 0 || !(fabsf(hp[1] - p[1]) <= GT_FLOOR_TOL)) return 0;
    if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hp, hn, 60.0f, 1.0f) != 2) return 0;
    return GtArea(hp) == g_gtAreaHer;
}
// The nearest proven floor cell from her feet (x, herFloor, z) into *pl (kind 3: she keeps her facing). 1 = found, 2 = none
// yet but the map is not finished (at the press only: the fade starts, the warp decides), 0 = none (*tried: cells tested)
static int GtFreeFind(const float pp[3], float herFloor, GtPlan *pl, int atWarp, int *tried)
{
    const float from[3] = { pp[0], herFloor, pp[2] };
    float at[3];
    *tried = 0;
    if (!g_gtProofOk || g_gtMapKey != GtRoomKeyNow() || !g_gtMap.on || !GtRayFree()) return 0;
    if (GtFreeCell(&g_gtMap, &g_gtWorld, from, GT_FREE_R, GtFreeOkCb, NULL, at, tried)) {
        pl->kind = 3, pl->pending = 0, pl->yaw = *(volatile const float *)0x10A0FC4;
        memcpy(pl->at, at, sizeof pl->at);
        pl->d = sqrtf((at[0] - pp[0]) * (at[0] - pp[0]) + (at[2] - pp[2]) * (at[2] - pp[2]));
        return 1;
    }
    if (atWarp || GtMapFinished(&g_gtMap) || g_gtMap.overflow) return 0;
    pl->kind = 3, pl->pending = 1, pl->yaw = *(volatile const float *)0x10A0FC4, pl->d = 0.0f;
    memcpy(pl->at, from, sizeof pl->at);
    return 2;
}
// The target now (v3.44's GtFind, v3.52 TP352: GtFindLanding + (d)): when no landing is found and she stands inside
// collision (GtStuck), the nearest proven floor cell (kind 3, "freed from a wall") instead of the refusal
static int GtFind(const float pp[3], float herFloor, GtPlan *pl, char *why, size_t nw, int atWarp, int *kind)
{
    int r, tried = 0;
    size_t o;
    g_gtFindRead = 0;
    if ((r = GtFindLanding(pp, herFloor, pl, why, nw, atWarp, kind)) != 0) return r;
    if (!g_gtFindRead || !g_gtProofOk || GtStuck(&g_gtWorld, pp) != 1) return 0;
    if ((r = GtFreeFind(pp, herFloor, pl, atWarp, &tried)) != 0) return 1;
    o = strlen(why);
    if (o + 1 < nw)
        _snprintf_s(why + o, nw - o, _TRUNCATE, "; she stands inside collision and no proven floor is within %.0f u (%d cell(s) "
                    "tried)", GT_FREE_R, tried);
    return 0;
}
static void GtMove(const float pp[3], const GtPlan *pl)  // the warp itself (the fade-out has ended)
{
    __declspec(align(16)) float spot[4];
    spot[0] = pl->at[0], spot[1] = pl->at[1], spot[2] = pl->at[2], spot[3] = 1.0f;
    ((void(__cdecl *)(float *, int))0x52CDA0)(spot, 0);       // PlayerSetPosition (pos + draw_pos)
    if (pl->kind != 3 || _finite(pl->yaw))                    // v3.52 TP352: (kind 3: her own facing, as read)
        ((void(__cdecl *)(float, int))0x52CDF0)(pl->yaw, 0);  // PlayerSetYaw (facing it, or her own facing kept)
    g_gtHeld.on = 1, g_gtHeld.t = 0.0f;
    memcpy(g_gtHeld.spot, pl->at, sizeof g_gtHeld.spot);
    ++g_gtWarps;
    if (pl->kind == 3) ++g_gtRoomFreed;                        // v3.52 TP352: (not a warp of the three kinds)
    else ++g_gtRoomW[pl->kind >= 0 && pl->kind < 3 ? pl->kind : 1];  // v3.50 TP350: the room's counts
    GtFreeWatchStart(&g_gtFreeW);                              // v3.52 TP352 (c): her body is watched now
    g_gtFreeWarp = g_gtWarps, g_gtFreeKind = pl->kind, g_gtFreeSaid = 0;
    g_gtCool = g_gtCoolS;
    if (g_gtHold.on && ++g_gtHold.warps > GT_HOLD_LOGN) {  // v3.49 GT349: a long hold: per tick only (its summary counts)
        LogT("ghost teleport: held, warp %d in a row: to (%.0f, %.0f)", g_gtHold.warps, pl->at[0], pl->at[2]);
        return;
    }
    if (!LogQuota(&g_gtQGo, 300)) return;
    {
        char sk[224];  // v3.51 TP351: the enemies in the cone skipped behind collision ("" for none)
        GtSkipSay(pl, sk, sizeof sk);
        if (pl->kind == 3) {  // v3.52 TP352 (d)
            if (LogQuota(&g_gtQFree, 60))
                Log("ghost teleport: freed from a wall: at the warp she stood inside collision at (%.0f, %.0f) and no other landing "
                    "was proven: to the nearest proven floor cell (%.0f, %.0f), %.0f u away, floor %+.0f u of hers (warp %d)", pp[0],
                    pp[2], pl->at[0], pl->at[2], pl->d, pl->at[1] - pp[1], g_gtWarps);
            return;
        }
        if (pl->kind == 2) {  // v3.50 TP350 A1
            char ray[24];
            if (pl->ray) sprintf_s(ray, sizeof ray, "%+.0f degrees off", GtFanYaw(0.0f, pl->ray) / GT_DEG);
            else sprintf_s(ray, sizeof ray, "straight ahead");
            LogV("ghost teleport: through %.0f u the way she faces (%s; the obstacle from %.0f u, %d spot(s) past it refused first; "
                 "her body free, on the room's floor map, floor %+.0f u of hers): from (%.0f, %.0f) to (%.0f, %.0f) (the fade began at (%.0f, %.0f)); "
                 "cooldown %.1f s (warp %d; same room: %d door line(s), box %s%s)", pl->d, ray, pl->dObst, pl->refusedBefore,
                 pl->at[1] - pp[1], pp[0], pp[2], pl->at[0], pl->at[2], g_gtFrom[0], g_gtFrom[2], g_gtCoolS, g_gtWarps, g_gtExitN,
                 g_gtBoxOk ? "in" : "unknown", sk);
            return;
        }
        if (pl->kind == 1)
            LogV("ghost teleport: ahead %.0f u the way she faces (no standing enemy she faces within %.0f u; try %d of %d, floor %+.0f u of hers, "
                 "proven: her body free, on the room's floor map): "  // v3.52 TP352: + proven
                 "from (%.0f, %.0f) to (%.0f, %.0f) (the fade began at (%.0f, %.0f)); cooldown %.1f s (warp %d; same room: %d door "
                 "line(s), box %s%s)", pl->d, g_gtRange, pl->tries, pl->nTries, pl->at[1] - pp[1], pp[0], pp[2], pl->at[0], pl->at[2],
                 g_gtFrom[0], g_gtFrom[2], g_gtCoolS, g_gtWarps, g_gtExitN, g_gtBoxOk ? "in" : "unknown", sk);
        else
            LogV("ghost teleport: behind type %02X %.0f u away (the enemy she faces, %.0f degrees off her facing, %d in the +-%.0f "
                 "degree cone; spot %d: %.0f u %s, floor %+.0f u of hers, proven: her body free, on the room's floor map): from (%.0f, "
                 "%.0f) to (%.0f, %.0f), facing it; cooldown %.1f s "  // v3.52 TP352: + proven
                 "(warp %d; same room: %d door line(s), box %s%s)", pl->type, pl->d, pl->ang, pl->nCone, g_gtCone, pl->spot, pl->gap,
                 pl->fromFace ? "behind its back" : "on its far side (no usable facing)", pl->at[1] - pp[1], pp[0], pp[2], pl->at[0],
                 pl->at[2], g_gtCoolS, g_gtWarps, g_gtExitN, g_gtBoxOk ? "in" : "unknown", sk);  // v3.50 TP350
    }
}
// v3.44 GT344: take back this tick's Action press (Run + Action, Action last): 1 = the game no longer sees it pressed
static int GtTakeBackAction(void)
{
    const DWORD mask = *(volatile const DWORD *)0x107A1A0;  // gInputActionMask[0]
    *(volatile DWORD *)0x1236C34 &= ~mask;                    // gPad.pressed (InputGetPressedMask)
    if (!GtPressed(0)) return 1;
    if (LogQuota(&g_gtQAn, 10))  // (a script's override press: InputButtonsPressed returns its buttons instead)
        LogAnomaly("ghost teleport", "Action's press could not be taken back (mask %08lX, override %08lX): it may also act", mask,
                   *(volatile const DWORD *)0x107BE48);
    return 0;
}
// v3.50 TP350 (c): during the fade-out a new press of Action (0) or Check (0xE: the examine points, 0x52C410) is taken
// back too (the bits of both bindings that are not Run's); v3.49 saw examines call 7 fades off 0.15 .. 0.27 s in
static void GtTakeBackFade(void)
{
    const DWORD m0 = *(volatile const DWORD *)0x107A1A0, mE = *(volatile const DWORD *)(0x107A1A0 + 4 * 0xE);
    const DWORD mRun = *(volatile const DWORD *)(0x107A1A0 + 4 * 0x10), bits = (m0 | mE) & ~mRun;
    *(volatile DWORD *)0x1236C34 &= ~bits;  // gPad.pressed (InputGetPressedMask)
    ++g_gtRoomBack;
    if (!GtPressed(0) && !GtPressed(0xE)) {
        LogT("ghost teleport: a press of Action / Check during the fade-out taken back (masks %08lX / %08lX)", m0, mE);
        return;
    }
    if (LogQuota(&g_gtQBack, 10))  // (a script's override press, or a Check bound to Run's button)
        LogAnomaly("ghost teleport", "an Action / Check press during the fade-out could not be taken back (masks %08lX / %08lX, Run "
                   "%08lX, override %08lX)", m0, mE, mRun, *(volatile const DWORD *)0x107BE48);
}
// v3.50 TP350 (d): a room visit's counts, one line when it is left (or the option goes off), if it saw a try
static void GtRoomSay(DWORD key)
{
    int k, o = 0, nNo = 0;
    char list[200];
    list[0] = 0;
    for (k = 0; k < GTN_N; ++k) {
        nNo += g_gtRoomNo[k];
        if (g_gtRoomNo[k] && o < (int)sizeof list - 40)
            o += sprintf_s(list + o, sizeof list - (size_t)o, "%s%s %d", o ? ", " : "", kGtNoKind[k], g_gtRoomNo[k]);
    }
    if ((g_gtRoomW[0] + g_gtRoomW[1] + g_gtRoomW[2] + nNo + g_gtRoomBack + g_gtRoomFreed) && LogQuota(&g_gtQRoom, 300)) {  // v3.52 TP352: + freed
        char map[120];
        if (g_gtMapKey == key && g_gtMap.on)
            sprintf_s(map, sizeof map, "%d cell(s)%s, %.1f ms over %d run(s)", g_gtMap.cells, GtMapFinished(&g_gtMap) ? ", finished" :
                      g_gtMap.overflow ? ", OVERFLOW" : ", not finished", g_gtMapMs, g_gtMap.runs);
        else
            sprintf_s(map, sizeof map, "not built");
        LogV("ghost teleport: room %d:%d left: %d warp(s) (%d through walls, %d behind the enemy she faces, %d ahead), %d refused%s%s%s; "
             "%d Action / Check press(es) taken back in fade-outs; %d enemy(ies) in the cone skipped behind collision in %d try(ies); "
             "%d time(s) freed from a wall; floor map %s", (int)(key >> 16), (int)(key & 0xFFFF),
             g_gtRoomW[0] + g_gtRoomW[1] + g_gtRoomW[2], g_gtRoomW[2], g_gtRoomW[0], g_gtRoomW[1], nNo, nNo ? " (" : "", list,
             nNo ? ")" : "", g_gtRoomBack, g_gtRoomSkip, g_gtRoomSkipTries, g_gtRoomFreed, map);  // v3.51 TP351: + the skips  // v3.52 TP352: + freed
    }
    memset(g_gtRoomW, 0, sizeof g_gtRoomW), memset(g_gtRoomNo, 0, sizeof g_gtRoomNo), g_gtRoomBack = 0;
    g_gtRoomSkip = g_gtRoomSkipTries = 0;  // v3.51 TP351
    g_gtRoomFreed = 0;                     // v3.52 TP352
}
// A chord (c: GTC_ACT_LAST / GTC_RUN_LAST) on a gameplay tick. v3.44 GT344-INPUT-1: every immediate refusal (her state,
// a fade running, the cooldown, no enemy spot / forward spot) comes BEFORE Action is taken back: a refused chord leaves
// the press to the game. Once the fade begins the press is the warp's: a later call-off (GtCallOff, a room change, a
// cut-scene, the end-of-fade GtFind refusal) does not give it back (a press 0.5 s old replayed would act late, out of
// the player's hands), and the log line says so.
// v3.49 GT349: a refusal while Run + Action are held: counted on the hold (its next try waits GT_FAIL_COOL); only the
// hold's first refusal gets a LogV line (the rest LogT; the hold's summary counts them). 1 = log it with LogV.
static int GtSayNo(int kind)  // v3.50 TP350: + a quota per refusal kind (GT_NO_KIND_MAX) and the room's count
{
    const int first = !g_gtHold.on || g_gtHold.refused == 0;
    if (kind < 0 || kind >= GTN_N) kind = GTN_STATE;
    GtHoldRefused(&g_gtHold);
    ++g_gtRoomNo[kind];
    return first && LogQuota(&g_gtQNoK[kind], GT_NO_KIND_MAX) && LogQuota(&g_gtQNo, 300);
}
static const char *GtChordWhat(int c, int back)  // the chord line's first words (logcheck GT_CHORD)
{
    if (c == GTC_HELD) return "held: the next warp";  // v3.49 GT349
    if (c == GTC_ACT_LAST) return back ? "Action last: its own use taken back" : "Action last: NOT taken back";
    return "Action first: its press reached the game";
}
static void GtOnChord(int c, const float pp[3])
{
    const int maskRun = *(volatile const int *)(0x107A1A0 + 4 * 0x10), maskAct = *(volatile const int *)0x107A1A0;
    char why[640];  // v3.50 TP350: 640 (the through-walls summary)
    float fl;
    GtPlan pl;
    int back = 0, kind = GTN_STATE;
    if (!GtMasksOk(maskRun, maskAct)) {  // (InputActionHeld never sees a zero mask held; a shared bit = every press a chord)
        if (!g_gtMaskSaid++) LogV("ghost teleport: Run and Action share a button (masks %08X / %08X): no chord", maskRun, maskAct);
        GtHoldReset(&g_gtHold);  // v3.49 GT349: no chord, no hold
        return;
    }
    if (!GtCanGo(pp, &fl, why, sizeof why)) {  // Action stays the game's: nothing taken back
        ++g_gtRefused;
        if (GtSayNo(GTN_STATE)) LogV("ghost teleport: not now: %s", why);
        else LogT("ghost teleport: not now (held): %s", why);
        return;
    }
    if (g_gtFade.phase != GTF_IDLE) {  // Action stays the game's (v3.49 GT349: still held when the fade ends = the next warp)
        LogT("ghost teleport: Run + Action during the fade: nothing more now (Action left to the game)");
        return;
    }
    if (g_gtCool > 0.0f) {  // Action stays the game's
        if (LogQuota(&g_gtQCool, 100))
            LogV("ghost teleport: Run + Action during the cooldown (%.1f s left): no warp (Action left to the game)", g_gtCool);
        return;
    }
    if (!GtFind(pp, fl, &pl, why, sizeof why, 0, &kind)) {  // Action stays the game's
        GtSkipCount(&pl);  // v3.51 TP351: this try ends here (a started fade's skips are counted by the warp's own search)
        ++g_gtRefused;
        g_gtCool = GT_FAIL_COOL;
        if (GtSayNo(kind)) LogV("ghost teleport: not now: %s (Action left to the game)", why);
        else LogT("ghost teleport: not now (held): %s", why);
        return;
    }
    // GT344-INPUT-1: the warp is on: only now is an Action-last press taken back (and a press the pad reports a tick late)
    if (c == GTC_ACT_LAST) back = GtTakeBackAction(), g_gtEat = 0.1f;
    {   // Richard's warp begins: his sound at her place, his fade-out
        __declspec(align(16)) float at[4];
        at[0] = pp[0], at[1] = pp[1], at[2] = pp[2], at[3] = 1.0f;
        if (g_gtSound) ((int(__cdecl *)(int, float, float *))0x46E7C0)(0xEC86, 1.0f, at);
    }
    memcpy(g_gtFrom, pp, sizeof g_gtFrom);
    GtFadeStart(&g_gtFade);
    GtFadeBegin();
    GtFreeWatchStop(&g_gtFreeW);  // v3.52 TP352: a new warp's own search frees her (kind 3); its move starts a new watch
    if (c == GTC_HELD && g_gtHold.warps >= GT_HOLD_LOGN) {  // v3.49 GT349: a long hold: per tick only
        LogT("ghost teleport: Run + Action (held: the next warp, %d so far)", g_gtHold.warps);
    } else if (LogQuota(&g_gtQStart, 300)) {
        static const char kPend[] = " (the room's floor map is not finished: the warp decides)";  // v3.52 TP352: every kind
        if (pl.kind == 3)  // v3.52 TP352 (d)
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then out of the wall she "
                 "stands in: the nearest proven floor cell %.0f u away%s", GtChordWhat(c, back), pl.d, pl.pending ? kPend : "");
        else if (pl.kind == 2)  // v3.50 TP350
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then %.0f u ahead the way "
                 "she faces, through a wall%s", GtChordWhat(c, back), pl.d, pl.pending ? kPend : "");
        else if (pl.kind == 1)
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then %.0f u ahead the way "
                 "she faces%s", GtChordWhat(c, back), pl.d, pl.pending ? kPend : "");
        else
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then behind type %02X "
                 "%.0f u away (the enemy she faces, %.0f degrees off)%s", GtChordWhat(c, back), pl.type, pl.d, pl.ang,
                 pl.pending ? kPend : "");
    }
}
static void GtCallOff(const char *why)  // the fade-out stops: she fades back in where she stands
{
    GtFadeBack(&g_gtFade);
    g_gtCool = GT_FAIL_COOL;
    GtHoldRefused(&g_gtHold);  // v3.49 GT349: (counted on a hold; the cooldown above makes it wait)
    if (LogQuota(&g_gtQOff, 100)) LogV("ghost teleport: called off during the fade: %s (she fades back in)", why);
    if (g_gtFade.phase == GTF_IDLE) GtFadeEnded();
}
// v3.49 GT349: a hold ended (let go, a menu / scene / load, a room change, the option off): one summary line when it
// did more than one try
static void GtHoldSay(const char *how)
{
    if (g_gtHold.warps + g_gtHold.refused > 1 && LogQuota(&g_gtQHold, 200))
        LogV("ghost teleport: Run + Action %s after %d warp(s) in a row (%d try(ies) refused or called off while held)", how,
             g_gtHold.warps, g_gtHold.refused);
    GtHoldReset(&g_gtHold);
}
// v3.52 TP352 (c): after a warp her body overlaps the room's collision (GtFreeWatch says so): the nearest proven floor
// cell within GT_FREE_R. 1 = done (moved, or no cell: said), 0 = wait (a gate, no free mover ray, the map being built)
static int GtFreeNow(const float p[3])
{
    static const char *const kKind[4] = { "behind the enemy she faces", "ahead", "through a wall", "out of a wall" };
    const BYTE *t = NULL;
    char why[640];
    float fl, at[3], from[3];
    int tried = 0, guard = 0, k;
    if (!GtCanGo(p, &fl, why, sizeof why)) {  // (an event, a hit, a door, a menu opening: later, while the watch runs)
        if (!g_gtFreeSaid++ && LogQuota(&g_gtQFree, 60))
            LogV("ghost teleport: inside collision after warp %d: the move out waits (%s)", g_gtFreeWarp, why);
        return 0;
    }
    if ((k = GtRoomRead(p)) != GTB_OK || GtZonesRead() != 0 || GtZonesInside(g_gtZone, g_gtZoneN, p) >= 0 ||
        !(g_gtProofOk = g_gtWorldOk && NwHasTree2())) {
        if (LogQuota(&g_gtQFree, 60))
            LogV("ghost teleport: inside collision after warp %d at (%.0f, %.0f): not moved (%s)", g_gtFreeWarp, p[0], p[2],
                 k != GTB_OK ? kGtBoundsWhy[k] : !g_gtProofOk ? "no solid map: no floor is proven" : "the room's event zones are "
                 "not known, or she stands in one");
        return 1;
    }
    memcpy(g_gtHerAt, p, sizeof g_gtHerAt);
    g_gtFoeN = 0;
    while ((t = DpNext(t)) != NULL && GtListMore(&guard, g_gtFoeN)) {  // the enemies' places now (GtFreeOkCb)
        const BYTE *ww = *(const BYTE *const *)(t + 0x408);
        if (!ww) continue;
        memcpy(g_gtFoeAt[g_gtFoeN], ww, 12);
        if (EchoF3(g_gtFoeAt[g_gtFoeN])) ++g_gtFoeN;
    }
    GtMapEnsure(p, fl, GT_MAP_PRESS);  // (her feet inside the wall do not seed it: GtCellFree)
    if (!GtRayFree()) return 0;
    from[0] = p[0], from[1] = fl, from[2] = p[2];
    if (!GtFreeCell(&g_gtMap, &g_gtWorld, from, GT_FREE_R, GtFreeOkCb, NULL, at, &tried)) {
        if (g_gtMap.on && !GtMapFinished(&g_gtMap) && !g_gtMap.overflow) return 0;  // (built on next tick)
        if (LogQuota(&g_gtQFree, 60))
            LogV("ghost teleport: inside collision after warp %d at (%.0f, %.0f): no proven floor cell within %.0f u (%d tried; "
                 "floor map %d cell(s)): not moved (Ghost Teleport or No Clip moves her)", g_gtFreeWarp, p[0], p[2], GT_FREE_R, tried,
                 g_gtMap.cells);
        return 1;
    }
    {
        __declspec(align(16)) float spot[4];
        const float d = sqrtf((at[0] - p[0]) * (at[0] - p[0]) + (at[2] - p[2]) * (at[2] - p[2]));
        spot[0] = at[0], spot[1] = at[1], spot[2] = at[2], spot[3] = 1.0f;
        ((void(__cdecl *)(float *, int))0x52CDA0)(spot, 0);  // PlayerSetPosition (her facing kept)
        g_gtHeld.on = 1, g_gtHeld.t = 0.0f;
        memcpy(g_gtHeld.spot, at, sizeof g_gtHeld.spot);
        ++g_gtRoomFreed;
        if (LogQuota(&g_gtQFree, 60))
            Log("ghost teleport: freed from a wall: after warp %d (%s) her body stood inside collision at (%.0f, %.0f) for %.2f s: "
                "moved %.0f u to the nearest proven floor cell (%.0f, %.0f), floor %+.0f u of hers (%d cell(s) tried)", g_gtFreeWarp,
                kKind[g_gtFreeKind >= 0 && g_gtFreeKind < 4 ? g_gtFreeKind : 1], p[0], p[2], g_gtFreeW.stuck, d, at[0], at[2],
                at[1] - fl, tried);
    }
    return 1;
}
static void GtFreeTick(int play, float dt)  // every tick while the watch runs (her place read now: a warp may have moved her)
{
    float p[3];
    int st;
    memcpy(p, (const void *)0x10A0F60, sizeof p);
    if (!EchoF3(p)) {
        GtFreeWatchStop(&g_gtFreeW);
        return;
    }
    st = play && g_gtWorldOk ? GtStuck(&g_gtWorld, p) : -1;
    if (GtFreeWatchStep(&g_gtFreeW, play, st, dt) && GtFreeNow(p)) GtFreeWatchStop(&g_gtFreeW);
}
static void GtTickC(void)
{
    const int rate = *(volatile int *)0x5D5248;
    const int on = (int)g_gtSet == 1;
    const int play = *(volatile int *)0xFD5C20 == 0 && *(volatile int *)0xECB1B4 == 0 && *(void *volatile *)0xFD5BA0 != NULL;
    float dt, pp[3];
    int c, run, act;
    DWORD roomKey;
    if (rate < 1 || rate > 1000) return;
    dt = 1.0f / (float)rate;
    if (g_gtDrawFault) {
        LogAnomaly("ghost teleport", "her see-through draw faulted (code %08lX): no fade for this session", (DWORD)g_gtDrawFault);
        g_gtDrawFault = 0;
    }
    roomKey = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (roomKey != g_gtRoomKey) {  // a new room: no fade or check carried over (the cooldown runs on)
        if (g_gtRoomKey != 0xFFFFFFFFu) GtRoomSay(g_gtRoomKey);  // v3.50 TP350: the room visit's counts
        g_gtMapKey = 0xFFFFFFFFu;                                 //   and its floor map is dropped
        g_gtRoomKey = roomKey;
        if (g_gtFade.phase != GTF_IDLE) {
            if (LogQuota(&g_gtQOff, 100)) LogV("ghost teleport: called off during the fade: a room change");
            GtFadeReset(&g_gtFade);
        }
        g_gtHeld.on = 0;
        GtFreeWatchStop(&g_gtFreeW);  // v3.52 TP352
        if (g_gtHold.on) GtHoldSay("held over a room change: stopped");  // v3.49 GT349: a new room = a new press
    }
    if (g_gtFade.phase != GTF_IDLE && (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0 ||
                                        *(void *volatile *)0xFD5BA0 == NULL)) {
        // v3.44 GT344-FADE-1: every state that stops `play` calls the fade off, none freezes it (a frozen fade kept her
        // see-through): game phase [0xFD5C20] 1 (a cut-scene) or 2 (an overlay: the pause, a menu, a message), the movie
        // loop [0xECB1B4] != 0, no player [0xFD5BA0]. She is drawn as stock at once: this tick's GtDrawSync(0) takes the
        // draw slots off. (The press stays the warp's, as for every later call-off; no cooldown.)
        if (LogQuota(&g_gtQOff, 100))
            LogV("ghost teleport: called off during the fade: %s", *(volatile int *)0xECB1B4 != 0 ? "a movie began" :
                 *(void *volatile *)0xFD5BA0 == NULL ? "no player" : *(volatile int *)0xFD5C20 == 1 ? "a cut-scene began" :
                 "an overlay (the pause, a menu, a message) began");
        GtFadeReset(&g_gtFade);
    }
    // the chord's buttons are followed in every phase and setting (a press held over a menu, a load or the row's
    // change is no new press)
    run = GtHeld(0x10), act = GtHeld(0);
    c = GtChordStep(&g_gtChord, run, act, dt, play);  // GT344-CHORD-1: not play = every press used
    if (!on) {
        if (g_gtFade.phase != GTF_IDLE) GtFadeReset(&g_gtFade);
        GtDrawSync(0.0f);
        g_gtHeld.on = 0;
        GtFreeWatchStop(&g_gtFreeW);  // v3.52 TP352
        if (g_gtHold.on) GtHoldSay("stopped: the option was turned off");  // v3.49 GT349
        return;
    }
    memcpy(pp, (const void *)0x10A0F60, sizeof pp);
    if (!EchoF3(pp)) {
        GtFadeReset(&g_gtFade);
        GtDrawSync(0.0f);
        return;
    }
    if (play) g_gtCool = g_gtCool > dt ? g_gtCool - dt : 0.0f;
    if (g_gtHeld.on && play) {  // the move held? (the game may put her back: an event, a scripted placement)
        const float dx = pp[0] - g_gtHeld.spot[0], dz = pp[2] - g_gtHeld.spot[2], d = sqrtf(dx * dx + dz * dz);
        g_gtHeld.t += dt;
        if (!GtHeldOk(d, g_gtHeld.t)) {
            g_gtHeld.on = 0;
            if (LogQuota(&g_gtQAn, 10)) LogAnomaly("ghost teleport", "the move did not hold: she is %.0f u from the spot %.2f s after the warp", d, g_gtHeld.t);
        } else if (g_gtHeld.t >= GT_HELD_CHECK) {
            g_gtHeld.on = 0;
            LogT("ghost teleport: the move held (%.0f u from the spot after %.2f s)", d, g_gtHeld.t);
        }
    }
    if (g_gtEat > 0.0f) {  // the chord's Action press: still held, a late press report is taken back too
        if (!GtHeld(0) || !play) g_gtEat = 0.0f;
        else {
            if (GtPressed(0)) GtTakeBackAction();
            g_gtEat -= dt;
        }
    }
    if (c != GTC_NONE && play && on) {  // v3.49 GT349: a fresh chord starts a hold (one press = one warp unless kept down)
        if (g_gtHold.on) GtHoldSay("let go");
        GtHoldStart(&g_gtHold);
        GtOnChord(c, pp);
    } else {
        const int h = GtHoldStep(&g_gtHold, run, act, play && on, g_gtFade.phase == GTF_IDLE, g_gtCool, dt);
        if (h == GTH_GO) GtOnChord(GTC_HELD, pp);
        else if (h == GTH_END) GtHoldSay(play ? "let go" : "stopped by a menu, a scene or a load");
    }
    // v3.50 TP350 (c): the fade-out owns Action: a new press of Action or Check is taken back before the game reads it
    if (g_gtFade.phase == GTF_OUT && play && (GtPressed(0) || GtPressed(0xE))) GtTakeBackFade();
    if (g_gtFade.phase != GTF_IDLE && play) {
        char why[640];
        float fl;
        int kind = GTN_STATE;
        if (g_gtFade.phase == GTF_OUT && !GtCanGo(pp, &fl, why, sizeof why)) {
            GtCallOff(why);
        } else if (GtFadeStep(&g_gtFade, dt)) {  // the fade-out has ended: Richard's ScratchWarp moment
            GtPlan pl;
            if (!GtCanGo(pp, &fl, why, sizeof why)) kind = GTN_STATE;
            else if (GtFind(pp, fl, &pl, why, sizeof why, 1, &kind)) {
                GtSkipCount(&pl);  // v3.51 TP351: the try's deciding search
                GtMove(pp, &pl);
                kind = -1;
            } else {
                GtSkipCount(&pl);  // v3.51 TP351
            }
            if (kind >= 0) {
                ++g_gtRefused;
                g_gtCool = GT_FAIL_COOL;
                if (GtSayNo(kind)) LogV("ghost teleport: not now: at the end of the fade-out: %s (she fades back in where she is)", why);
                else LogT("ghost teleport: not now (held): at the end of the fade-out: %s", why);
            }
        } else if (g_gtFade.phase == GTF_IDLE) {
            GtFadeEnded();
        } else if (g_gtFade.phase == GTF_OUT && g_gtMapKey == GtRoomKeyNow()) {
            GtMapGrow(GT_MAP_TICK);  // v3.50 TP350: an unfinished floor map is built on through the fade-out
        }
    }
    if (g_gtFreeW.on) GtFreeTick(play, dt);  // v3.52 TP352 (c): her body after a warp
    GtDrawSync(GtFadeSee(&g_gtFade));
}
static void GtTick(void)  // the 0x4EF982 dispatcher (quicksave.inc QsTick)
{
    if (!g_gtReady || g_gtFault) return;
    __try {
        GtTickC();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_gtFault = 1;  // fail closed: no warp for this session; her draw back to stock
        GtDrawOff();
        LogAnomaly("ghost teleport", "faulted (code %08lX): off for this session", GetExceptionCode());
    }
}

// ---------------------------------------------------------------- the row and [Gameplay] GhostTeleport
static void GtSetLocked(int v)
{
    if (v < 0 || v > 1) return;
    InterlockedExchange(&g_gtSet, v);
    g_gtUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "GhostTeleport", kGtName[v], g_iniPath)) {
        g_gtUnsaved = 1;
        Log("gameplay: GhostTeleport %s applies now but could not be saved in SH4Refined.ini (error %lu)", kGtName[v], GetLastError());
    } else {
        Log("gameplay: GhostTeleport %s", kGtName[v]);
    }
}
static int GtAvail(void) { return g_gtReady && g_qsTickOn; }  // (the 0x4EF982 dispatcher runs the trigger)
static int __cdecl GtIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    if (!GtAvail()) {
        ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue((int)g_gtSet, 2);
    if (v >= 0) GtSetLocked(v);
    GpUnlock();
    return 0;
}
static int __cdecl GtGet(void) { return GtAvail() ? (int)g_gtSet : 0; }  // unavailable: shown Off
static int __cdecl GtDesc(void) { return MSG_GT0 + (!GtAvail() ? GTM_NA : g_gtSet ? GTM_D1 : GTM_D0); }
static void GtFillRow(DWORD *r)  // a complete 7-dword row (gameplay.inc GpBuildPages: the Special page, under Echo Count)
{
    g_gtIds[0] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 0;  // Off / On: One-Hit Kill's value texts (as No Clip's row)
    g_gtIds[1] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 1;
    FillRow(r, GtIn, GtGet, g_gtIds, 2, MSG_GT0 + GTM_L, GtDesc);
}
static const BYTE *GtText(int id)
{
    return id >= MSG_GT0 && id < MSG_GT0 + GTM_N ? g_txtGt[id - MSG_GT0] : g_txtGt[GTM_NA];
}
static void GtTextOn(void)  // the On line carries the cooldown in force
{
    char d[64];
    if (g_gtCoolS > 0.0f)  // v3.49 GT349: hold to keep warping; no cooldown unless the ini asks for one
        sprintf_s(d, sizeof d, "On: Run + Action warps you (%g s apart); hold to repeat.", (double)g_gtCoolS);
    else
        sprintf_s(d, sizeof d, "On: Run + Action warps you; hold both to keep warping.");
    Encode(g_txtGt[GTM_D1], 64, d);
}
static void GtTexts(void)
{
    Encode(g_txtGt[GTM_L], 64, "Ghost Teleport");
    Encode(g_txtGt[GTM_D0], 64, "Warp behind the enemy you face, or ahead through walls.");  // v3.44 GT344  // v3.50 TP350
    GtTextOn();
    Encode(g_txtGt[GTM_NA], 64, "Not available in this game (see SH4Refined.log).");
}
static int GtParse(const char *s) { return !_stricmp(s, "On") || !strcmp(s, "1"); }
// the hidden keys GhostTeleportRange / Cooldown / Sound / (v3.44 GT344) Forward / (v3.50 TP350) Cone, ThroughWalls;
// 1 = something changed
static int GtReadKeys(void)
{
    char buf[32];
    float range, cool, fwd, cone;
    LONG snd, thru, behind;
    int ch;
    GetPrivateProfileStringA("Gameplay", "GhostTeleportRange", "", buf, sizeof buf, g_iniPath);
    range = GtRangeKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    GetPrivateProfileStringA("Gameplay", "GhostTeleportCooldown", "", buf, sizeof buf, g_iniPath);
    cool = GtCoolKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    GetPrivateProfileStringA("Gameplay", "GhostTeleportForward", "", buf, sizeof buf, g_iniPath);
    fwd = GtFwdKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    snd = GetPrivateProfileIntA("Gameplay", "GhostTeleportSound", 1, g_iniPath) != 0;
    GetPrivateProfileStringA("Gameplay", "GhostTeleportCone", "", buf, sizeof buf, g_iniPath);  // v3.50 TP350
    cone = GtConeKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    thru = GetPrivateProfileIntA("Gameplay", "GhostTeleportThroughWalls", 1, g_iniPath) != 0;
    behind = GetPrivateProfileIntA("Gameplay", "GhostTeleportBehindCollision", 0, g_iniPath) != 0;  // v3.51 TP351 (hidden; 1 = v3.50)
    ch = range != g_gtRange || cool != g_gtCoolS || snd != g_gtSound || fwd != g_gtFwd || cone != g_gtCone || thru != g_gtThruOn ||
         behind != g_gtBehindColl;
    g_gtRange = range, g_gtCoolS = cool, g_gtSound = snd, g_gtFwd = fwd, g_gtCone = cone, g_gtThruOn = thru, g_gtBehindColl = behind;
    if (ch) GtTextOn();
    return ch;
}
static void GtReadIni(void)
{
    char buf[32];
    IniStr("Gameplay", "GhostTeleport", buf, sizeof buf);
    g_gtSet = GtParse(buf);
    if (!g_iniFallback) GtReadKeys();
}
static void GtIniReread(void)  // under GpLock (GpIniReread)
{
    char buf[32];
    int v;
    if (g_iniFallback) return;
    if (GtReadKeys())
        Log("gameplay: [Gameplay] GhostTeleportRange / Cooldown / Sound / Forward / Cone / ThroughWalls / BehindCollision read again: "
            "%.0f u, %.1f s, sound %s, ahead %.0f u, +-%.0f degrees, through walls %s, behind an enemy past collision %s", g_gtRange,
            g_gtCoolS, g_gtSound ? "on" : "off", g_gtFwd, g_gtCone, g_gtThruOn ? "on" : "off",
            g_gtBehindColl ? "allowed (v3.50)" : "never");  // v3.50 TP350  // v3.51 TP351: + BehindCollision
    if (g_gtUnsaved) return;
    GetPrivateProfileStringA("Gameplay", "GhostTeleport", kGtName[g_gtSet ? 1 : 0], buf, sizeof buf, g_iniPath);
    v = GtParse(buf);
    if (v == (int)g_gtSet) return;
    InterlockedExchange(&g_gtSet, v);
    g_gtSnap = v;
    Log("gameplay: [Gameplay] GhostTeleport read again: %s", kGtName[v]);
}
static void GtSnapshot(void) { g_gtSnap = g_gtSet; }
static int GtChanged(void) { return g_gtSet != g_gtSnap; }
static int GtRowChanged(void) { return g_snapOn && g_gtSet != g_gtSnap; }
static void GtDiscard(void)  // under GpLock (GpDiscard)
{
    if (g_gtSet != g_gtSnap) GtSetLocked((int)g_gtSnap);
}
static void GtInstall(void)  // GameplayInstall (after the ini read): no exe patch; the called functions byte-checked
{
    static const Site kFn[] = {
        { 0x52CDA0, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x4D }, 8 },  // PlayerSetPosition
        { 0x52CDF0, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x55 }, 8 },  // PlayerSetYaw
        { 0x46E7C0, { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x74 }, 8 },  // a 3D sound at a point
        { 0x4F8440, { 0x53, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14, 0x33, 0xDB, 0xE8, 0xF2, 0xFD, 0xFF, 0xFF }, 14 },  // InputActionHeld
        { 0x4F84A0, { 0x53, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14, 0x33, 0xDB, 0xE8, 0x92, 0xFD, 0xFF, 0xFF }, 14 },  // GT344: InputActionPressed
        { 0x554250, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9, 0x8B, 0x0D, 0x34, 0x6C, 0x23, 0x01 }, 14 },  // GT344: gPad.pressed
        { 0x4F82C0, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x04, 0x85, 0xA0, 0xA1, 0x07, 0x01, 0xC3 }, 12 },  // GT344: gInputActionMask
        { 0x46F5A0, { 0x55, 0x8B, 0xEC, 0x8B, 0x0D, 0x90, 0x5A, 0xFD }, 8 },  // the sub-area of a point
        { 0x45CE10, { 0x8B, 0x44, 0x24, 0x04, 0x0F, 0xBF, 0x88, 0x14 }, 8 },  // the enemy model's rotation
        { 0x463F50, { 0xE8, 0x5B, 0x4E, 0x09, 0x00, 0x85, 0xC0, 0x74 }, 8 },  // EnemyEventPauseTest
        { 0x458F00, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC }, 8 },  // room to stand
        { 0x46F700, { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x1D, 0x90, 0x5A }, 8 },  // not inside geometry
        { 0x459490, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC }, 8 },  // EnemyFloorRayCheck
        { 0x454000, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC }, 8 },  // a line test
        { 0x4FA040, { 0x55, 0x8B, 0xEC, 0xA1, 0x90, 0x5A, 0xFD, 0x00 }, 8 },  // GT343-001: ExitGetNum
        { 0x4FA070, { 0x55, 0x8B, 0xEC, 0xA1, 0x90, 0x5A, 0xFD, 0x00 }, 8 },  // GT343-001: ExitGet
        { 0x561A70, { 0x8B, 0x15, 0x74, 0x6D, 0x2F, 0x01, 0x85, 0xD2 }, 8 },  // GT343-001: the room's collision box
        // v3.49 GT349: the event objects' tests (GtZonesRead; the same bytes pickup.inc checks)
        { 0x4CF090, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8 }, 6 },                          // the object's use-item
        { 0x4CFFC0, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, 5 },                                // EventIsRecordRunning
        { 0x52B1C0, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x75, 0x04 }, 8 },              // ActorIsEnabled
        { 0x52B230, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x0A }, 8 },              // ActorGetType
        { 0x4D00F0, { 0x55, 0x56, 0x8B, 0x35, 0x00, 0x82, 0x3D, 0x01 }, 8 } };            // EventObjectsTick (the list 0x13D8200)
    // v3.44 GT344: Richard's fade on her model: the ghost's pre-draw calls, his post-draw, the draw loop's two calls
    static const Site kFade[] = {
        { 0x404B80, { 0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0x08, 0xA3, 0x40, 0x56, 0x5D, 0x00 }, 12 },  // RenderSetPixelShader
        { 0x429ED0, { 0x8B, 0x44, 0x24, 0x08, 0x8B, 0x4C, 0x24, 0x04, 0x50, 0x51, 0xE8, 0x81, 0xDC, 0xFF, 0xFF }, 15 },  // GS ALPHA
        { 0x429F00, { 0x8B, 0x44, 0x24, 0x08, 0x8B, 0x4C, 0x24, 0x04, 0x50, 0x51, 0xE8, 0xC1, 0xD7, 0xFF, 0xFF }, 15 },  // blend of TEST
        { 0x429F60, { 0xE9, 0x8B, 0xDC, 0xFF, 0xFF }, 5 },                                                    // the flag (-> 0x427BF0)
        { 0x507240, { 0xE8, 0xAB, 0x2C, 0xF2, 0xFF, 0xE8, 0xD6, 0x2C, 0xF2, 0xFF, 0xC7, 0x44, 0x24, 0x04, 0x08, 0x00 }, 16 },  // his post-draw
        { 0x52AE88, { 0x8B, 0xB7, 0x1C, 0x04, 0x00, 0x00, 0x8B, 0x46, 0x24, 0x85, 0xC0, 0x74, 0x06, 0x57, 0xFF, 0xD0 }, 16 },  // pre-draw call
        { 0x52AEBD, { 0x8B, 0x76, 0x28, 0x85, 0xF6, 0x74, 0x06, 0x57, 0xFF, 0xD6 }, 10 } };  // post-draw call
    // v3.50 TP350: the through-warp's collision queries (noclip_world.h's; the same bytes noclip.inc relies on)
    static const Site kWorld[] = {
        { 0x561CD0, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x24, 0x53, 0x56, 0x57 }, 12 },  // the tree2 line's plane
        { 0x41E9C0, { 0x8B, 0x4C, 0x24, 0x08, 0x56, 0x8B, 0x74, 0x24, 0x08, 0xD9, 0x46, 0x08 }, 12 },  // the point on it
        { 0x561B80, { 0xA1, 0x74, 0x6D, 0x2F, 0x01, 0x85, 0xC0, 0x75, 0x03, 0x33, 0xC0, 0xC3 }, 12 },  // the sphere's polygons
        { 0x561CB0, { 0xA1, 0x78, 0x6D, 0x2F, 0x01, 0x85, 0xC0, 0x75, 0x01, 0xC3, 0x8B, 0x4C }, 12 },  // inside solid
        { 0x561CA0, { 0x8B, 0x0D, 0x78, 0x6D, 0x2F, 0x01, 0x33, 0xC0, 0x85, 0xC9, 0x0F, 0x95 }, 12 } };  // tree2 present
    int i, ok = 1, fade = 1, world = 1;
    for (i = 0; i < (int)(sizeof kWorld / sizeof kWorld[0]); ++i)
        if (!SiteOk(&kWorld[i])) {
            Log("ghost teleport: %08lX is not the game's code (another mod?): no warp through walls", kWorld[i].va);
            world = 0;
        }
    for (i = 0; i < (int)(sizeof kFn / sizeof kFn[0]); ++i)
        if (!SiteOk(&kFn[i])) {
            Log("ghost teleport: %08lX is not the game's code (another mod?): the option does nothing", kFn[i].va);
            ok = 0;
        }
    for (i = 0; i < (int)(sizeof kFade / sizeof kFade[0]); ++i)
        if (!SiteOk(&kFade[i])) {
            Log("ghost teleport: %08lX is not the game's code (another mod?): the warp keeps Richard's timing and sound, no fade", kFade[i].va);
            fade = 0;
        }
    GtChordReset(&g_gtChord);
    GtFadeReset(&g_gtFade);
    GtHoldReset(&g_gtHold);  // v3.49 GT349
    g_gtFadeOk = fade;
    g_gtWorldOk = world;  // v3.50 TP350
    g_gtReady = ok;
    Log("ghost teleport: %s (no exe patch; %d game functions checked), [Gameplay] GhostTeleport %s; Run + Action together "
        "(within %.0f ms): behind the standing enemy she faces (+-%.0f degrees) within %.0f u%s, else up to %.0f u ahead the way she "
        "faces, through walls %s (onto floor a route of the room reaches), cooldown %.1f s; "  // v3.50 TP350
        "every landing proven (her body free, on the room's floor map); inside a wall after a warp: moved to the nearest proven floor; "  // v3.52 TP352
        "held: the next warp once she has faded in; never across or into a walk-in event zone; "  // v3.49 GT349
        "Richard's transition: his warp sound 0xEC86 %s, his see-through fade (%.1f s out, %.1f s in) on her model %s",
        ok ? "in" : "OFF", (int)(sizeof kFn / sizeof kFn[0] + sizeof kFade / sizeof kFade[0] + sizeof kWorld / sizeof kWorld[0]),
        kGtName[g_gtSet ? 1 : 0], GT_CHORD_S * 1000.0f, g_gtCone, g_gtRange,
        g_gtBehindColl ? " (also past collision: GhostTeleportBehindCollision 1)" : world ? " in the open (none past collision)" :
        " in the open (none past collision; the mover line: the tree2 queries are not stock)",  // v3.51 TP351
        g_gtFwd, !g_gtThruOn ? "off" : world ? "on" : "OFF",
        g_gtCoolS, g_gtSound ? "on" : "off", GT_FADE_S, GT_FADE_S, fade ? "in" : "OFF");
    if (!ok && g_gtSet) LogAnomaly("ghost teleport", "[Gameplay] GhostTeleport is On but its game functions are not stock: the option does nothing");
}
