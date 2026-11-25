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

typedef struct { const BYTE *t, *w; float pos[3], rad, floorY; int type; } GtFoe;
enum { GTS_OK, GTS_ENEMY, GTS_SIGHT, GTS_WALL, GTS_RAY, GTS_FLOOR, GTS_LEDGE, GTS_ROOM, GTS_AREA,
       GTS_BOX, GTS_DOOR_NEAR, GTS_DOOR_BEYOND, GTS_DOOR_CROSS, GTS_FOE_DOOR, GTS_EVENT, GTS_N };  // GT343-001: + the room's edges
static const char *const kGtWhy[GTS_N] = { "", "near an enemy", "a wall between", "in a wall", "no free floor ray",
                                           "no floor", "another level", "no room to stand", "another area",
                                           "outside the room's box", "at a door line", "beyond a door line (the next room)",
                                           "through a doorway", "the enemy beyond a door line",
                                           "an event's trigger zone on the way" };  // v3.49 GT349: + GTS_EVENT
// GT343-001: this room's edges, read once an attempt (GtRoomRead): its door lines and its collision box
static GtExit g_gtExit[GT_EXIT_MAX];
static int g_gtExitN, g_gtBoxOk, g_gtAreaHer, g_gtExitAsked;  // GT343-001c: + ExitGetNum's own count
static float g_gtBoxLo[3], g_gtBoxHi[3];
// v3.44 GT344: a found target (GtFind): behind an enemy (kind 0) or ahead of her (kind 1)
typedef struct {
    int kind, type, foe, np, spot, fromFace, tries, nTries;
    float at[3], yaw, d, gap;
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
// Spot candidate c (xz; y = the anchor's floor) seen from the anchor `from` (x, its floor y, z: the enemy, or v3.44
// GT344 her own place for the warp ahead; areaFrom = its sub-area): GTS_OK (out = the floor point) or why not
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
    return GTS_OK;
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
// v3.44 GT344 (was v3.43's GtTry without the move): the target now. 1 = *pl holds it; 0 = why says why not
static int GtFind(const float pp[3], float herFloor, GtPlan *pl, char *why, size_t nw)
{
    static GtFoe foe[GT_ACTOR_MAX];  // GT343-004 (game thread only; static: 3 KB less stack per call)
    static GtCand cand[GT_ACTOR_MAX];
    static float others[GT_ACTOR_MAX][3];
    int nf = 0, no, order[GT_PICK_MAX], np, k, guard = 0, cnt[GTS_N];
    const BYTE *t = NULL;
    char list[240];
    memset(pl, 0, sizeof *pl);
    // GT343-004: the whole list, every actor kept (GT_ACTOR_MAX only bounds a broken list): a nearer one late in the
    // list (Enemy Copies) is never dropped; GtPick keeps the nearest GT_PICK_MAX of them
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
        {
            const BYTE *hb = *(const BYTE *const *)(ww + 0x38);  // the mover's HitBody: +0x10 its bounding radius
            const float rr = hb ? *(const float *)(hb + 0x10) : 40.0f;
            f->rad = _finite(rr) ? EchoClampF(rr, 20.0f, 90.0f) : 40.0f;
        }
        ++nf;
    }
    np = GtPick(cand, nf, g_gtRange, order, GT_PICK_MAX);
    if (!np && !(g_gtFwd > 0.0f))
        return sprintf_s(why, nw, "no standing enemy within %.0f u (%d enemies in the room)", g_gtRange, nf), 0;
    memset(cnt, 0, sizeof cnt);
    if ((k = GtRoomRead(pp)) != GTB_OK)  // GT343-001: the room's door lines and box; GT343-001c: complete, else no warp
        return sprintf_s(why, nw, "the room's edges are not known (%s; room %d, %d of %d exit(s) read): no same-room check, "
                         "no warp", kGtBoundsWhy[k], *(volatile const int *)0xFD5A94, g_gtExitN, g_gtExitAsked), 0;
    // v3.49 GT349: the walk-in event zones (fail closed: a list that cannot be read whole = no warp)
    memcpy(g_gtHerAt, pp, sizeof g_gtHerAt);
    if ((k = GtZonesRead()) != 0)
        return sprintf_s(why, nw, "the room's event zones are not known (%s): no warp", k == 1 ? "the event object list is "
                         "broken or longer than 512" : "more than 64 walk-in zones"), 0;
    GtZonesSay();
    // v3.49 GT349-001: she stands inside a walk-in zone whose event has not started (she may have run in during the
    // fade-out): it fires first; no warp out of it (at the press and again at the end of the fade-out)
    if ((k = GtZonesInside(g_gtZone, g_gtZoneN, pp)) >= 0)
        return sprintf_s(why, nw, "inside an event's trigger zone (key %03X, not fired yet): no warp out of it",
                         g_gtZoneKey[k] & 0xFFF), 0;
    if (!np) {  // v3.44 GT344 (c): no standing enemy in range: the way she faces, shortened step by step
        const float yaw = *(volatile const float *)0x10A0FC4;  // gPlayer[0].rot.y
        const float from[3] = { pp[0], herFloor, pp[2] };
        const int n = GtFwdTries(g_gtFwd);
        if (!_finite(yaw)) return sprintf_s(why, nw, "no standing enemy within %.0f u and her facing is not a number", g_gtRange), 0;
        for (k = 0; k < nf; ++k) memcpy(others[k], foe[k].pos, 12);
        for (k = 0; k < n; ++k) {
            float c[3];
            int r;
            GtFwdCand(from, yaw, g_gtFwd, k, c);
            if ((r = GtSpotTest(from, g_gtAreaHer, c, (const float (*)[3])others, nf, pl->at)) != GTS_OK) {
                ++cnt[r];
                continue;
            }
            pl->kind = 1, pl->yaw = yaw, pl->tries = k + 1, pl->nTries = n;
            pl->d = sqrtf((pl->at[0] - pp[0]) * (pl->at[0] - pp[0]) + (pl->at[2] - pp[2]) * (pl->at[2] - pp[2]));
            return 1;
        }
        return sprintf_s(why, nw, "no free floor ahead of her in this room (no standing enemy within %.0f u; %d tries from %.0f "
                         "down to %.0f u, %d door line(s): %s)", g_gtRange, n, g_gtFwd, GtFwdDist(g_gtFwd, n - 1),
                         g_gtExitN, GtWhyList(cnt, list, sizeof list)), 0;
    }
    for (k = 0; k < np; ++k) {
        GtFoe *f = &foe[order[k]];
        __declspec(align(16)) float q[4];
        float back[2], fq[3];
        int j, s, fromFace;
        {   // its floor
            float e[3];
            memcpy(e, f->pos, sizeof e);
            e[1] -= 40.0f;
            if (!EchoFloorY(e, &f->floorY)) f->floorY = f->pos[1];
        }
        fq[0] = f->pos[0], fq[1] = f->floorY, fq[2] = f->pos[2];
        // GT343-001: an enemy standing beyond one of the room's door lines is not taken (it is at the next room)
        if (GtExitTest(g_gtExit, g_gtExitN, NULL, fq) == GTX_BEYOND) {
            ++cnt[GTS_FOE_DOOR];
            continue;
        }
        memcpy(q, ((const float *(__cdecl *)(const BYTE *))0x45CE10)(f->t), 16);
        fromFace = GtBackDir(q, f->pos, pp, back);
        for (j = 0, no = 0; j < nf; ++j)
            if (j != order[k]) memcpy(others[no++], foe[j].pos, 12);
        for (s = 0; s < GT_SPOT_N; ++s) {
            float c[3];
            int r;
            GtSpotCand(fq, back, f->rad, s, c);
            if ((r = GtSpotTest(fq, GtArea(f->pos), c, (const float (*)[3])others, no, pl->at)) != GTS_OK) {
                ++cnt[r];
                continue;
            }
            pl->kind = 0, pl->type = f->type & 0xFF, pl->foe = k + 1, pl->np = np, pl->spot = s, pl->fromFace = fromFace;
            pl->yaw = GtFaceYaw(pl->at, f->pos), pl->d = cand[order[k]].d;
            pl->gap = sqrtf((pl->at[0] - f->pos[0]) * (pl->at[0] - f->pos[0]) + (pl->at[2] - f->pos[2]) * (pl->at[2] - f->pos[2]));
            return 1;
        }
    }
    return sprintf_s(why, nw, "no free floor behind the nearest %d enemy(ies) in this room (type %02X %.0f u first; %d spots, "
                     "%d door line(s): %s)", np, foe[order[0]].type & 0xFF, cand[order[0]].d, np * GT_SPOT_N, g_gtExitN,
                     GtWhyList(cnt, list, sizeof list)), 0;
}
static void GtMove(const float pp[3], const GtPlan *pl)  // the warp itself (the fade-out has ended)
{
    __declspec(align(16)) float spot[4];
    spot[0] = pl->at[0], spot[1] = pl->at[1], spot[2] = pl->at[2], spot[3] = 1.0f;
    ((void(__cdecl *)(float *, int))0x52CDA0)(spot, 0);       // PlayerSetPosition (pos + draw_pos)
    ((void(__cdecl *)(float, int))0x52CDF0)(pl->yaw, 0);      // PlayerSetYaw (facing it, or her own facing kept)
    g_gtHeld.on = 1, g_gtHeld.t = 0.0f;
    memcpy(g_gtHeld.spot, pl->at, sizeof g_gtHeld.spot);
    ++g_gtWarps;
    g_gtCool = g_gtCoolS;
    if (g_gtHold.on && ++g_gtHold.warps > GT_HOLD_LOGN) {  // v3.49 GT349: a long hold: per tick only (its summary counts)
        LogT("ghost teleport: held, warp %d in a row: to (%.0f, %.0f)", g_gtHold.warps, pl->at[0], pl->at[2]);
        return;
    }
    if (!LogQuota(&g_gtQGo, 300)) return;
    if (pl->kind == 1)
        LogV("ghost teleport: ahead %.0f u the way she faces (no standing enemy within %.0f u; try %d of %d, floor %+.0f u of hers): "
             "from (%.0f, %.0f) to (%.0f, %.0f) (the fade began at (%.0f, %.0f)); cooldown %.1f s (warp %d; same room: %d door "
             "line(s), box %s)", pl->d, g_gtRange, pl->tries, pl->nTries, pl->at[1] - pp[1], pp[0], pp[2], pl->at[0], pl->at[2],
             g_gtFrom[0], g_gtFrom[2], g_gtCoolS, g_gtWarps, g_gtExitN, g_gtBoxOk ? "in" : "unknown");
    else
        LogV("ghost teleport: behind type %02X %.0f u away (enemy %d of %d tried, spot %d: %.0f u %s, floor %+.0f u of hers): from "
             "(%.0f, %.0f) to (%.0f, %.0f), facing it; cooldown %.1f s (warp %d; same room: %d door line(s), box %s)", pl->type,
             pl->d, pl->foe, pl->np, pl->spot, pl->gap, pl->fromFace ? "behind its back" : "on its far side (no usable facing)",
             pl->at[1] - pp[1], pp[0], pp[2], pl->at[0], pl->at[2], g_gtCoolS, g_gtWarps, g_gtExitN, g_gtBoxOk ? "in" : "unknown");
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
// A chord (c: GTC_ACT_LAST / GTC_RUN_LAST) on a gameplay tick. v3.44 GT344-INPUT-1: every immediate refusal (her state,
// a fade running, the cooldown, no enemy spot / forward spot) comes BEFORE Action is taken back: a refused chord leaves
// the press to the game. Once the fade begins the press is the warp's: a later call-off (GtCallOff, a room change, a
// cut-scene, the end-of-fade GtFind refusal) does not give it back (a press 0.5 s old replayed would act late, out of
// the player's hands), and the log line says so.
// v3.49 GT349: a refusal while Run + Action are held: counted on the hold (its next try waits GT_FAIL_COOL); only the
// hold's first refusal gets a LogV line (the rest LogT; the hold's summary counts them). 1 = log it with LogV.
static int GtSayNo(void)
{
    const int first = !g_gtHold.on || g_gtHold.refused == 0;
    GtHoldRefused(&g_gtHold);
    return first && LogQuota(&g_gtQNo, 300);
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
    char why[320];
    float fl;
    GtPlan pl;
    int back = 0;
    if (!GtMasksOk(maskRun, maskAct)) {  // (InputActionHeld never sees a zero mask held; a shared bit = every press a chord)
        if (!g_gtMaskSaid++) LogV("ghost teleport: Run and Action share a button (masks %08X / %08X): no chord", maskRun, maskAct);
        GtHoldReset(&g_gtHold);  // v3.49 GT349: no chord, no hold
        return;
    }
    if (!GtCanGo(pp, &fl, why, sizeof why)) {  // Action stays the game's: nothing taken back
        ++g_gtRefused;
        if (GtSayNo()) LogV("ghost teleport: not now: %s", why);
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
    if (!GtFind(pp, fl, &pl, why, sizeof why)) {  // Action stays the game's
        ++g_gtRefused;
        g_gtCool = GT_FAIL_COOL;
        if (GtSayNo()) LogV("ghost teleport: not now: %s (Action left to the game)", why);
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
    if (c == GTC_HELD && g_gtHold.warps >= GT_HOLD_LOGN) {  // v3.49 GT349: a long hold: per tick only
        LogT("ghost teleport: Run + Action (held: the next warp, %d so far)", g_gtHold.warps);
    } else if (LogQuota(&g_gtQStart, 300)) {
        if (pl.kind == 1)
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then %.0f u ahead the way "
                 "she faces", GtChordWhat(c, back), pl.d);
        else
            LogV("ghost teleport: Run + Action (%s; a later call-off keeps it): Richard's fade out (0.5 s), then behind type %02X "
                 "%.0f u away", GtChordWhat(c, back), pl.type, pl.d);
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
        g_gtRoomKey = roomKey;
        if (g_gtFade.phase != GTF_IDLE) {
            if (LogQuota(&g_gtQOff, 100)) LogV("ghost teleport: called off during the fade: a room change");
            GtFadeReset(&g_gtFade);
        }
        g_gtHeld.on = 0;
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
    if (g_gtFade.phase != GTF_IDLE && play) {
        char why[320];
        float fl;
        if (g_gtFade.phase == GTF_OUT && !GtCanGo(pp, &fl, why, sizeof why)) {
            GtCallOff(why);
        } else if (GtFadeStep(&g_gtFade, dt)) {  // the fade-out has ended: Richard's ScratchWarp moment
            GtPlan pl;
            if (GtCanGo(pp, &fl, why, sizeof why) && GtFind(pp, fl, &pl, why, sizeof why)) {
                GtMove(pp, &pl);
            } else {
                ++g_gtRefused;
                g_gtCool = GT_FAIL_COOL;
                if (GtSayNo()) LogV("ghost teleport: not now: at the end of the fade-out: %s (she fades back in where she is)", why);
                else LogT("ghost teleport: not now (held): at the end of the fade-out: %s", why);
            }
        } else if (g_gtFade.phase == GTF_IDLE) {
            GtFadeEnded();
        }
    }
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
    Encode(g_txtGt[GTM_D0], 64, "Warp behind the nearest enemy, or ahead, like Richard.");  // v3.44 GT344
    GtTextOn();
    Encode(g_txtGt[GTM_NA], 64, "Not available in this game (see SH4Refined.log).");
}
static int GtParse(const char *s) { return !_stricmp(s, "On") || !strcmp(s, "1"); }
// the hidden keys GhostTeleportRange / Cooldown / Sound / (v3.44 GT344) Forward; 1 = something changed
static int GtReadKeys(void)
{
    char buf[32];
    float range, cool, fwd;
    LONG snd;
    int ch;
    GetPrivateProfileStringA("Gameplay", "GhostTeleportRange", "", buf, sizeof buf, g_iniPath);
    range = GtRangeKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    GetPrivateProfileStringA("Gameplay", "GhostTeleportCooldown", "", buf, sizeof buf, g_iniPath);
    cool = GtCoolKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    GetPrivateProfileStringA("Gameplay", "GhostTeleportForward", "", buf, sizeof buf, g_iniPath);
    fwd = GtFwdKey(buf[0] != 0, buf[0] ? (float)atof(buf) : 0.0f);
    snd = GetPrivateProfileIntA("Gameplay", "GhostTeleportSound", 1, g_iniPath) != 0;
    ch = range != g_gtRange || cool != g_gtCoolS || snd != g_gtSound || fwd != g_gtFwd;
    g_gtRange = range, g_gtCoolS = cool, g_gtSound = snd, g_gtFwd = fwd;
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
        Log("gameplay: [Gameplay] GhostTeleportRange / Cooldown / Sound / Forward read again: %.0f u, %.1f s, sound %s, ahead %.0f u",
            g_gtRange, g_gtCoolS, g_gtSound ? "on" : "off", g_gtFwd);
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
    int i, ok = 1, fade = 1;
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
    g_gtReady = ok;
    Log("ghost teleport: %s (no exe patch; %d game functions checked), [Gameplay] GhostTeleport %s; Run + Action together "
        "(within %.0f ms): behind the nearest standing enemy within %.0f u, else %.0f u ahead the way she faces, cooldown %.1f s; "
        "held: the next warp once she has faded in; never across or into a walk-in event zone; "  // v3.49 GT349
        "Richard's transition: his warp sound 0xEC86 %s, his see-through fade (%.1f s out, %.1f s in) on her model %s",
        ok ? "in" : "OFF", (int)(sizeof kFn / sizeof kFn[0] + sizeof kFade / sizeof kFade[0]), kGtName[g_gtSet ? 1 : 0],
        GT_CHORD_S * 1000.0f, g_gtRange, g_gtFwd, g_gtCoolS, g_gtSound ? "on" : "off", GT_FADE_S, GT_FADE_S, fade ? "in" : "OFF");
    if (!ok && g_gtSet) LogAnomaly("ghost teleport", "[Gameplay] GhostTeleport is On but its game functions are not stock: the option does nothing");
}
