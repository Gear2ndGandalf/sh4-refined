// ================================================================ v3.32 NC332 / v3.33 NC333: No Clip (Option 5 > Special)
// [Gameplay] NoClip = Off / On / Fly (0 / 1 / 2; v3.32's Off / On still read). The player passes through walls, doors and
// props; floors and stairs still carry her (On) or she moves where the camera looks (Fly, only while an over-the-shoulder
// camera drives the view: CamOtsOn()); the companions (escort Eileen, Cynthia) catch up through walls; enemies keep their
// collision. Plans, RE and the live checklists: staging/v332_feedback/w4_noclip/PLAN.md,
// staging/v333_feedback/w3_noclip/PLAN.md. The pure rules: noclip_sim.h (test/noclip_sim_test.c, and the real-collision
// harness staging/v333_feedback/w3_noclip/re: the On rule walked through the Subway's dumped collision); the game's
// collision queries: noclip_world.h.
// Included at the end of gameplay.inc: it reuses GpPatch / GpBranch / GpApplyAll (byte-checked, all-or-nothing, listed
// in SH4WM_Patches through Poke), GpLock, FillRow, StepValue, Encode and gameplay.inc's text block (GPM_NC*).
//
// One exe site (GOG v7): 0x534D29 `call 5339D0` (E8 A2 EC FF FF) in the third-person player update PlayerRoomTaskUpdate
// 0x534AE0: her walk step through PlayerWallFloorCollide (wall slide + floor snap; old, wanted, out; cdecl, the caller
// pops). The call goes to NcCollide, which runs the stock function unless No Clip handles the step. Only this call: the
// knock-back slide 0x53B83C, the hit reaction 0x53C922, the grab follow 0x53F417 and Room 302 (0x52DF8F, stage 10) keep
// the stock collision; PlayerBodyPushOut 0x5338A0 (right after, 0x534D4D) is untouched: it pushes her out only when
// event-added walls (the polygon tables 0x12F6D80) are near, so event boundaries keep holding her.
// Free of cheat-table AOBs / asserts / writes (ct_conflicts --span 0x534D29-0x534D2E: none; the table's own Henry Noclip,
// entry 7890, patches 0x534CF4 and, when on, jumps past this call) and of Playable Eileen patches.
//
// v3.33 (the user's QA: "pass through everything on the horizontal", "unable to climb or descend stairs", "Eileen will
// not follow"): the room's tree2 is a BSP of solid space; v3.32's floor probe from 60 above her feet found nothing
// whenever that point was inside a wall, a tall prop or a stair block, so she held 120 later (stairs flanked by walls:
// stuck at one height). On now = the stock step while she stands on a floor and it lets her make the full step, else a
// horizontal pass-through with a floor rule that follows stairs (noclip_sim.h NcDecideOn). No exit guard any more (v3.32
// gave walk-in exits 250 of stock collision: "too much gets in the way"; their lock checks 0x4FDE90 still apply, as for
// doors). Companions: placed beside the player 2 s after their line to her is blocked beyond 250 once she has gone
// through geometry (v3.32: 600 for 3 s, which a 120 void limit never let her reach), on a spot with a floor, a free body
// and a clear line to her. Fly: NcFlyStep (the camera's view direction from the drawn camera 0xFD5AC0); leaving Fly
// (the row, or the camera turned off) sinks her to the floor below or puts her back on the last floor point.
// Suspended (stock collision, the latch kept): Room 302 (stages 1 and 10) and game phase [0xFD5C20] != 0 (real-time
// cut-scenes, menus). Turned off while inside geometry: active until she is clear.
// v3.37 MN337 (staging/v337_feedback/w1_menu/PLAN.md): the row moved to Option 5's top level (above Curse Meter); Fly has
// its own speed (noclip_sim.h NcFlySpeed / NcFlyAbsStep): running input flies hidden [Gameplay] NoClipFlySpeed (1.0 ..
// 4.0, default 2.5) x the run speed 663 u/s, walking input 663 u/s, eased with the game's run value (the input block
// 0x10A1A00 + 0x4C, read only) and scaled by the stick (InputLeftStickGetX / Y 0x4F8610 / 0x4F8660 and the block's
// +0x0C tank side step, through Fly's own envelope: full in 0.12 s, stopped in 0.10 s, about 83 units at full speed; a
// push alone never flies her); Movement Speed does not stack; no input, no move; the room's box still holds her.

#include "noclip_sim.h"
#include "noclip_world.h"

static int CamOtsOn(void);  // v3.33 NC333: 1 while an over-the-shoulder camera drives the view (camera.inc, included
                            // before gameplay.inc; v3.33 MERGE: the stub is gone)

static volatile LONG g_ncSet;  // the row: 0 Off, 1 On, 2 Fly
static LONG g_ncSnap;
static int g_ncUnsaved, g_ncReady, g_ncFault;
static int g_ncLatched;        // game thread only (NcCollide) from here on
static unsigned g_ncRoomKey = 0xFFFFFFFF;
static NcWalk g_ncWalk;
static NcWorld g_ncWorld;
static NcWarp g_ncWarpE, g_ncWarpC;  // Eileen, Cynthia
static NcLand g_ncLand;
static NcHeld g_ncHeld;
static int g_ncFlying, g_ncEffLast = -1;
static volatile float g_ncFlyK = NC_FLY_K_DEF;  // v3.37 MN337: [Gameplay] NoClipFlySpeed (hidden; x the run speed)
static float g_ncFlyEnv;  // v3.37 MN337: Fly's speed envelope 0..1 (game thread)
#define NC_IN_VA 0x10A1A00u   // v3.37 MN337: PlayerMoveInput D_010A1A00 (+0 forward, +0x0C side; 0x52EEE0 / 0x52F260 fill it each tick)
#define NC_RUN_VA 0x10A1A4Cu  // v3.37 MN337: its .run (0..1, eased by the game)
static float g_ncThru;         // seconds left in which a pass-through counts as recent (the companions' rule)
static float g_ncLastOut[3];
static int g_ncLastOutOk;
static DWORD g_ncIds[3];
static __declspec(align(16)) float g_ncCand[4], g_ncTarget[4];
typedef struct {     // v3.34 NC334: a companion just placed: the feet watch
    NcFeetWatch w;
    BYTE *work;      // whose (a new task / room cancels it)
    float probeY;    // the floor our probe found at the spot
    float setY;      // her y after the game's placement 0x4416C0
} NcPlaced;
static NcPlaced g_ncPlacedE, g_ncPlacedC;
#define NC_FOOT_IK ((NcFootIk *)0xFCE020)  // v3.34 NC334: EileenFootIk[2] (0xFCE020, 0xFCE070), the companion's foot IK
static const char *const kNcName[3] = { "Off", "On", "Fly" };

// One line per `ms` at most (game thread)
static int NcEvery(DWORD *last, DWORD ms)
{
    const DWORD now = GetTickCount();
    if (*last && now - *last < ms) return 0;
    *last = now ? now : 1;
    return 1;
}
static int NcRoomNow(void) { return (int)(*(volatile const unsigned *)0xFD5A94 & 0xFFFF); }
static int NcStageNow(void) { return *(volatile const int *)0xFD5A90; }

// Event-added walls (the polygon tables) round her body sphere at the wanted point (0x5339D0's first sphere: 72 up, r 70)
static int NcEventWalls(const float *p)
{
    NwSphereAt(p[0], p[1] - 72.0f, p[2], 70.0f);
    ((void(__cdecl *)(void))0x561AC0)();
    ((void(__cdecl *)(float *))0x5621B0)(g_nwSph);
    return ((int(__cdecl *)(void))0x561AD0)();
}

// ---------------------------------------------------------------- the companions' catch-up
// The companion's line to the player: EileenLineBlocked 0x440300(work, pos) (two lines 100 above the feet; the companion
// AI's own test, shared by Eileen and Cynthia), asked every tick here (the AI asks only while it follows).
static int NcLineBlocked(BYTE *w, const float *pp)
{
    g_ncTarget[0] = pp[0]; g_ncTarget[1] = pp[1]; g_ncTarget[2] = pp[2]; g_ncTarget[3] = 1.0f;
    return ((int(__cdecl *)(void *, float *))0x440300)(w, g_ncTarget) != 0;
}
// A spot beside the player: 120 (then 200) away at 135, -135, 180, 90, -90, 45, -45, 0 degrees from her facing, with a
// floor within 60 above / 100 below her feet, the feet in the open, a free body (no polygon 50..170 up, r 60) and a clear
// line from her chest. Then the game's own placement from that spot (0x4416C0(work, 0): EileenFindFreeSpot round the
// model position, the feet, the sphere, the heading, henry_dist) and the route reset 0x442410. 0 = no spot.
// v3.34 NC334: then her foot IK moves with her (NcIkCarry: the set-up EileenMotionCreate gives the feet at a room
// entry, done at the new spot; without it a planted foot stayed on the old spot: "her legs float for a moment").
// *setY = her y after 0x4416C0, *carried = feet carried (0..2), *stale = feet left alone (not hers).
static int NcPlace(BYTE *w, const float *pp, float *where, float *setY, int *carried, int *stale)
{
    static const float kAng[8] = { 2.3561945f, -2.3561945f, 3.1415927f, 1.5707964f, -1.5707964f, 0.7853982f, -0.7853982f, 0.0f };
    const float yaw = *(volatile const float *)0x10A0FC4;  // the player's facing (sin, cos = x, z)
    void *model = *(void **)(w + 0x18);
    const int s5r12 = NcStageNow() == 5 && NcRoomNow() == 0xC;  // v3.34 NC334: 0x4416C0 sets y 0 there for y > 0
    int ring, k;
    *carried = *stale = 0;
    if (!model || !NcFinite(yaw)) return 0;
    for (ring = 0; ring < 2; ++ring)
        for (k = 0; k < 8; ++k) {
            const float a = yaw + kAng[k], r = ring ? 200.0f : 120.0f;
            const float cx = pp[0] + sinf(a) * r, cz = pp[2] + cosf(a) * r;
            const float *mp = (const float *)((BYTE *)model + 0x10);  // v3.34 NC334: her model position (Model.bone.pos)
            float fy, from[3], d[3];
            int f;
            if (!NwFloor(0, cx, pp[1] - 60.0f, pp[1] + 100.0f, cz, &fy)) continue;
            if (s5r12 && fy > 0.5f) continue;  // v3.34 NC334: the game would put her at y 0 there (floating)
            if (NwSolid(0, cx, fy - NC_FEET, cz) == 1) continue;
            if (NwSphereCount(cx, fy - 110.0f, cz, 60.0f) != 0) continue;
            g_nwLine[0] = pp[0]; g_nwLine[1] = pp[1] - 100.0f; g_nwLine[2] = pp[2]; g_nwLine[3] = 1.0f;
            g_nwLine[4] = cx; g_nwLine[5] = fy - 100.0f; g_nwLine[6] = cz; g_nwLine[7] = 1.0f;
            if (((int(__cdecl *)(float *, float *))0x561CD0)(g_nwLine, g_nwPlane)) continue;
            from[0] = mp[0], from[1] = mp[1], from[2] = mp[2];  // v3.34 NC334
            g_ncCand[0] = cx; g_ncCand[1] = fy; g_ncCand[2] = cz; g_ncCand[3] = 1.0f;
            ((void(__cdecl *)(void *, const float *))0x55C360)(model, g_ncCand);  // ModelSetPosition
            ((int(__cdecl *)(void *, int))0x4416C0)(w, 0);
            ((void(__cdecl *)(void *))0x442410)(w);
            *(int *)(w + 0xEC) = 0;  // henry_line_blocked: a fresh look next tick
            where[0] = cx, where[1] = fy, where[2] = cz;
            *setY = mp[1];
            // v3.34 NC334: the feet with her (the player's update runs before hers: her next tick starts from these)
            d[0] = mp[0] - from[0], d[1] = mp[1] - from[1], d[2] = mp[2] - from[2];
            for (f = 0; f < 2; ++f) {
                NcFootIk *ik = &NC_FOOT_IK[f];
                float floorY = 0.0f;
                int hf = 0, rc;
                if (ik->target && NcFinite3(ik->pos) && NcFinite3(d))
                    hf = NwFloor(0, ik->pos[0] + d[0], ik->pos[1] + d[1] - 100.0f, ik->pos[1] + d[1] + 200.0f, ik->pos[2] + d[2], &floorY);
                rc = NcIkCarry(ik, from, d, hf, floorY);
                if (rc > 0) ++*carried;
                else if (rc < 0) ++*stale;
            }
            return 1;
        }
    return 0;
}
static float NcCompDist(BYTE *w, const float *pp)
{
    const float *q = *(const float *const *)(w + 0x20);  // her position (EileenWork.pos)
    if (q && NcFinite3(q)) {
        const float dx = q[0] - pp[0], dy = q[1] - pp[1], dz = q[2] - pp[2];
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }
    return *(const float *)(w + 0x90);  // henry_dist (the AI's, from the player's last position)
}
// v3.34 NC334: the best foot's gap to its floor now (< 0 unknown); her body's y and the floor under it
static float NcFeetGapNow(BYTE *w, float *bodyY, float *bodyFloor, int *haveBody, float *gap)
{
    const float *mp = (const float *)(*(BYTE *const *)(w + 0x18) + 0x10);
    float best = -1.0f;
    int f;
    for (f = 0; f < 2; ++f) {
        const NcFootIk *ik = &NC_FOOT_IK[f];
        float fy = 0.0f;
        int hf = 0;
        gap[f] = -1.0f;
        if (!ik->target) continue;
        if (NcFinite3(ik->pos)) hf = NwFloor(0, ik->pos[0], ik->pos[1] - 100.0f, ik->pos[1] + 200.0f, ik->pos[2], &fy);
        gap[f] = NcFootGap(ik->pos, mp, hf, fy);
        if (gap[f] >= 0.0f && (best < 0.0f || gap[f] < best)) best = gap[f];
    }
    *bodyY = mp[1];
    *bodyFloor = 0.0f;
    *haveBody = NcFinite3(mp) && NwFloor(0, mp[0], mp[1] - 100.0f, mp[1] + 200.0f, mp[2], bodyFloor);
    return best;
}
// v3.34 NC334: after a placement: one line after her first tick there, an anomaly if no foot stood on the floor
// NC_FEET_CHECK .. + NC_FEET_SPAN s later (rate-limited: placements are 2 s apart at least)
static void NcPlacedTick(NcPlaced *p, BYTE *w, float dt, const char *who)
{
    static volatile LONG s_nFloat;
    float by, bf, g[2], best;
    int hb, r;
    if (!p->w.active) return;
    if (!w || w != p->work || !*(void *const *)(w + 0x18)) {
        p->w.active = 0;  // her task changed (a room, an event): nothing to judge
        return;
    }
    best = NcFeetGapNow(w, &by, &bf, &hb, g);
    r = NcFeetStep(&p->w, best, dt);
    if (p->w.ticks == 1)
        LogV("no clip: %s one tick after the placement: y %.0f (the floor under her %s%.0f; our probe found %.0f, the game's placement "
             "set %.0f); feet %.0f / %.0f off their floor (target %.0f above it)", who, by, hb ? "" : "(none) ", bf, p->probeY, p->setY,
             g[0], g[1], NC_ANKLE);
    if (r == 2 && LogQuota(&s_nFloat, 20))
        LogAnomaly("no clip", "%s's feet float %.1f s after a placement: no foot within %.0f of the floor (best %.0f; now %.0f / %.0f), "
                   "y %.0f, floor under her %s%.0f, stage %d room %d", who, NC_FEET_CHECK, NC_FEET_OFF, p->w.best, g[0], g[1], by,
                   hb ? "" : "(none) ", bf, NcStageNow(), NcRoomNow());
    else if (r == 1)
        LogV("no clip: %s's feet are on the floor %.1f s after the placement (the best foot %.0f off)", who, NC_FEET_CHECK, p->w.best);
    else if (r == 3)
        LogV("no clip: %s's feet after the placement: no floor found under them (not judged)", who);
}
static void NcWarpTick(NcWarp *s, BYTE *w, int eligible, const char *why, const float *pp, float dt, const char *who,
                       int *count, DWORD *lastWhy, NcPlaced *placed)
{
    static volatile LONG s_nSetY, s_nStale;
    const float dist = NcCompDist(w, pp);
    const int thru = g_ncThru > 0.0f;
    const int blocked = NcLineBlocked(w, pp);
    float at[3], setY;
    int carried, stale;
    if (!eligible) {
        if (blocked && dist > NC_WARP_NEAR && thru && NcEvery(lastWhy, 15000))
            LogV("no clip: %s %.0f away behind a wall: not placed (%s)", who, dist, why);
        s->behind = 0.0f;
        return;
    }
    if (!NcWarpStep(s, eligible, blocked, dist, thru, dt)) return;
    if (!NwClearAt(pp)) {  // the player stands inside geometry: wait until she is clear (no placement into a wall)
        s->behind = thru ? NC_WARP_WAIT : NC_WARP_WAIT_FAR;
        s->cool = 0.0f;
        if (NcEvery(lastWhy, 10000)) LogV("no clip: %s %.0f away behind a wall: waits until you stand clear of walls and props", who, dist);
        return;
    }
    if (!NcPlace(w, pp, at, &setY, &carried, &stale)) {
        if (NcEvery(lastWhy, 10000))
            LogV("no clip: %s %.0f away behind a wall: no free spot beside you (floor, room, clear line); tries again in %.0f s", who, dist, NC_WARP_COOL);
        return;
    }
    ++*count;
    LogV("no clip: %s was %.0f away behind a wall for %.0f s (%s): placed beside you at %.0f, %.0f, %.0f (%d this session); "
         "floor y %.0f, the game set y %.0f; her foot IK moved with her (%d of 2 feet)", who,  // v3.34 NC334: floor, y, feet
         dist, thru ? NC_WARP_WAIT : NC_WARP_WAIT_FAR, thru ? "you went through geometry" : "far behind", at[0], at[1], at[2], *count,
         at[1], setY, carried);
    // v3.34 NC334: the checks of the placement
    if (!(fabsf(setY - at[1]) <= NC_FEET_OFF) && LogQuota(&s_nSetY, 10))
        LogAnomaly("no clip", "%s: the game's placement 0x4416C0 set her at y %.0f, %.0f from the floor found there (%.0f, %.0f, %.0f), stage %d room %d",
                   who, setY, setY - at[1], at[0], at[1], at[2], NcStageNow(), NcRoomNow());
    if (stale && LogQuota(&s_nStale, 10))
        LogAnomaly("no clip", "%s: %d foot IK target(s) at 0xFCE020 were not near her (another body's?): left alone", who, stale);
    placed->work = w;
    placed->probeY = at[1];
    placed->setY = setY;
    NcFeetBegin(&placed->w);
}
static void NcCompanions(const float *pp, float dt)
{
    static int s_nE, s_nC;
    static DWORD s_whyE, s_whyC;
    BYTE *t, *w;
    // Eileen: her companion task (0x44ADE0), in the party with Henry ([0xFD5BC4] 1), in his room (w+0xF0), in a follow
    // state (1 idle, 2 turn, 3 walk, 4, 5 step, 6 path, 9 pant, 10 idle variant; never 18 the event animation, the
    // attacks, flinch, knock-down, cower, collapse or Henry down). The companion is always this task (Playable Eileen
    // swaps the player's model; its ghost mode hides her: party / state then say no).
    t = *(BYTE *const *)0xFCE6B4;
    w = t && *(const DWORD *)(t + 0x404) == 0x44ADE0 ? *(BYTE *const *)(t + 0x408) : NULL;
    if (w && *(void *const *)(w + 0x18)) {
        const WORD st = *(const WORD *)(t + 0x410);
        const int party = *(volatile const int *)0xFD5BC4;
        const int fixed = NcStageNow() == 3 && NcRoomNow() == 0x16;  // 0x4416C0's fixed spot there
        const int stOk = st == 1 || st == 2 || st == 3 || st == 4 || st == 5 || st == 6 || st == 9 || st == 10;
        const char *why = party != 1 ? "not with you (party)" : *(const int *)(w + 0xF0) ? "in another room" :
                          !stOk ? "busy (not a follow state)" : fixed ? "stage 3 room 22 uses a fixed spot" : "";
        NcPlacedTick(&g_ncPlacedE, w, dt, "Eileen");  // v3.34 NC334: last placement's feet (before a new one)
        NcWarpTick(&g_ncWarpE, w, !*why, why, pp, dt, "Eileen", &s_nE, &s_whyE, &g_ncPlacedE);
    } else {
        g_ncWarpE.behind = 0.0f;
        g_ncPlacedE.w.active = 0;  // v3.34 NC334
    }
    // Cynthia: her task (0x4ED000, not the scripted 0x4ED4F0) runs (0x4ED460), she follows ([0xFD5BC8] != 0), states
    // 2 idle / 3 turn / 4 follow, not in room 1 (her script keeps her back there, 0x4EBD70)
    t = *(BYTE *const *)0xFD5A50;
    w = t && *(const DWORD *)(t + 0x404) == 0x4ED000 ? *(BYTE *const *)(t + 0x408) : NULL;
    if (w && *(void *const *)(w + 0x18) && ((int(__cdecl *)(void))0x4ED460)() == 1) {
        const WORD st = *(const WORD *)(t + 0x410);
        const char *why = *(volatile const int *)0xFD5BC8 == 0 ? "not following" : NcRoomNow() == 1 ? "room 1 keeps her back" :
                          !(st == 2 || st == 3 || st == 4) ? "busy (not a follow state)" : "";
        NcPlacedTick(&g_ncPlacedC, w, dt, "Cynthia");  // v3.34 NC334
        NcWarpTick(&g_ncWarpC, w, !*why, why, pp, dt, "Cynthia", &s_nC, &s_whyC, &g_ncPlacedC);
    } else {
        g_ncWarpC.behind = 0.0f;
        g_ncPlacedC.w.active = 0;  // v3.34 NC334
    }
}

// ---------------------------------------------------------------- Fly: the camera's view direction
// The drawn camera 0xFD5AC0 (rows 0..2 its axes, row 3 the eye; copied from gGame.camera 0xFD5B00 each frame): row 2 is
// the view axis; it is turned to point from the eye toward her chest if needed (whoever drives the camera: Refined's OTS,
// Playable Eileen's, the game's). 0 = not usable.
static int NcCamForward(const float *pp, float *f)
{
    const volatile float *m = (const volatile float *)0xFD5AC0;
    const float vx = pp[0] - m[12], vy = pp[1] - 100.0f - m[13], vz = pp[2] - m[14];
    float l;
    f[0] = m[8], f[1] = m[9], f[2] = m[10];
    if (!NcFinite3(f)) return 0;
    l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(l > 0.5f && l < 2.0f)) return 0;
    f[0] /= l, f[1] /= l, f[2] /= l;
    if (f[0] * vx + f[1] * vy + f[2] * vz < 0.0f) f[0] = -f[0], f[1] = -f[1], f[2] = -f[2];
    return 1;
}

// ---------------------------------------------------------------- the walk step
static void NcRoomEnter(unsigned key)
{
    g_ncRoomKey = key;
    g_ncLatched = 0;  // a new room's entry point is clear
    NcWalkReset(&g_ncWalk);
    memset(&g_ncWarpE, 0, sizeof g_ncWarpE);
    memset(&g_ncWarpC, 0, sizeof g_ncWarpC);
    memset(&g_ncPlacedE, 0, sizeof g_ncPlacedE);  // v3.34 NC334
    memset(&g_ncPlacedC, 0, sizeof g_ncPlacedC);
    memset(&g_ncLand, 0, sizeof g_ncLand);
    memset(&g_ncHeld, 0, sizeof g_ncHeld);
    g_ncLastOutOk = 0;
    NwInit(&g_ncWorld);
}
// The checks after a step: the softlock watch and the impossible states (rate-limited)
static void NcAfter(const float *old, const float *want, const float *out, int dec, float dt)
{
    static volatile LONG s_nDrop, s_nHeld;
    const float in2 = NcDist2XZ(want[0], want[2], old[0], old[2]);
    const float mv2 = NcDist2XZ(out[0], out[2], old[0], old[2]);
    if (dec != NC_STOCK && dec != NC_FLOOR && dec != NC_CLIMB && dec >= 0 && out[1] > old[1] + 0.5f && LogQuota(&s_nDrop, 20))
        LogAnomaly("no clip", "her height dropped %.1f without a floor (decision %s) at %.0f, %.0f, %.0f, stage %d room %d",
                   out[1] - old[1], dec >= 0 ? kNcDecName[dec] : "fly", old[0], old[1], old[2], NcStageNow(), NcRoomNow());
    if (NcHeldStep(&g_ncHeld, in2 > 0.0025f, mv2 > 0.0004f || fabsf(out[1] - old[1]) > 0.02f, dt) && LogQuota(&s_nHeld, 30))
        LogAnomaly("no clip", "softlock? no movement for %.0f s while you push to move: stage %d room %d at %.0f, %.0f, %.0f, decision %s, "
                   "stairs %.0f, last floor %s%.0f, %.0f, %.0f, inside %.0f u (walk back the way you came; tell us where this was)",
                   NC_HELD_ANOMALY, NcStageNow(), NcRoomNow(), old[0], old[1], old[2], dec >= 0 ? kNcDecName[dec] : "fly",
                   g_ncWalk.va, g_ncWalk.valid ? "" : "(none) ", g_ncWalk.fx, g_ncWalk.fy, g_ncWalk.fz, g_ncWalk.dSolid);
}
// Decision lines for the tester (rate-limited per room): stairs, climbs, the first pass-through, holds
static void NcLogDecision(const float *old, const float *out, int dec, float inside)
{
    static DWORD s_climb, s_solid, s_hold, s_air, s_stairs;
    static int s_lastStairs;
    const int stairs = g_ncWalk.va >= NC_STAIR_VA;
    if (dec == NC_CLIMB && NcEvery(&s_climb, 2000))
        LogV("no clip: stairs: climbed %.0f onto a step at %.0f, %.0f, %.0f (stairs %.0f, after %.0f u inside geometry)",
             g_ncWalk.lastTop, out[0], out[1], out[2], g_ncWalk.va, inside);
    if (dec == NC_SOLID && NcEvery(&s_solid, 8000))
        LogV("no clip: through geometry at %.0f, %.0f, %.0f at her height (%s, %.0f u inside; a top within %.0f would be climbed)",
             out[0], out[1], out[2], stairs ? "on stairs" : "flat ground", g_ncWalk.dSolid,
             stairs ? NC_RISER + NC_SLOPE * g_ncWalk.dSolid : NC_RISE_FLAT);
    if (dec == NC_AIR && NcEvery(&s_air, 8000))
        LogV("no clip: no floor at %.0f, %.0f (a drop of more than %.0f or out of the room): she walks on at her height up to %.0f from the last floor",
             out[0], out[2], NC_DROP, NC_GAP);
    if (dec == NC_HOLD && NcEvery(&s_hold, 8000))
        LogV("no clip: holds at %.0f, %.0f, %.0f: no floor within %.0f of the last floor point %.0f, %.0f (walk back or into a wall)",
             old[0], old[1], old[2], NC_GAP, g_ncWalk.fx, g_ncWalk.fz);
    if (stairs != s_lastStairs && NcEvery(&s_stairs, 3000))
        LogV("no clip: %s at %.0f, %.0f, %.0f (height activity %.0f)", stairs ? "on stairs / a slope" : "level ground again",
             out[0], out[1], out[2], g_ncWalk.va);
    s_lastStairs = stairs;
}
// 1 = handled (out written), 0 = run the stock collision
static int NcStep(float *old, float *want, float *out)
{
    static int s_logLatch;
    static DWORD s_logEvent, s_logCam, s_logNan;
    const int set = (int)g_ncSet;
    const int stage = NcStageNow();
    const unsigned key = (unsigned)stage << 16 | (unsigned)NcRoomNow();
    const int roomChanged = key != g_ncRoomKey;
    float dt, inside;
    int suspended, clear = 0, wasLatched, eff, dec;
    eff = set == 2 ? (CamOtsOn() ? 2 : 1) : set;
    if (eff != g_ncEffLast) {
        if (g_ncEffLast >= 0 || eff)
            LogV("no clip: %s%s (stage %d room %d)", kNcName[eff], set == 2 && eff == 1 ? ": Fly waits for an over-the-shoulder camera, On meanwhile" : "",
                 stage, (int)(key & 0xFFFF));
        g_ncEffLast = eff;
    }
    if (!set && !g_ncLatched && !g_ncFlying && !g_ncLand.active) {  // Off: the stock call, nothing else (= v3.31.0)
        g_ncRoomKey = 0xFFFFFFFF;  // (the room is set up again when No Clip is next active)
        return 0;
    }
    dt = ((float(__cdecl *)(void))0x55AE60)();  // the game tick's frame time
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (!NcFinite3(old)) {
        if (NcEvery(&s_logNan, 10000)) LogAnomaly("no clip", "her position is not a number (stage %d room %d): the stock collision", stage, (int)(key & 0xFFFF));
        return 0;
    }
    if (roomChanged) {
        NcRoomEnter(key);
        if (g_ncFlying) g_ncWalk.valid = 1, g_ncWalk.fx = old[0], g_ncWalk.fy = old[1], g_ncWalk.fz = old[2];  // the entry point
    }
    suspended = stage == 1 || stage == 10 || *(volatile const int *)0xFD5C20 != 0;
    if (suspended) return 0;  // Room 302, scenes, menus: the stock collision; the latch and Fly are kept
    // leaving Fly (the row, or the camera turned off): down to the floor below, or back to the last floor point
    if (g_ncFlying && eff != 2) {
        const int r = NcLandBegin(&g_ncLand, &g_ncWorld, &g_ncWalk, old);
        g_ncFlying = 0;
        if (r == 1) LogV("no clip: leaving Fly: she sinks %.0f to the floor below", g_ncLand.ty - old[1]);
        else if (r == 2) LogV("no clip: leaving Fly: no floor below her (inside geometry or over the void): back to the last floor point %.0f, %.0f, %.0f",
                              g_ncWalk.fx, g_ncWalk.fy, g_ncWalk.fz);
        else if (r == 0) LogV("no clip: leaving Fly: she stands on a floor");
        else LogAnomaly("no clip", "leaving Fly at %.0f, %.0f, %.0f: no floor below and no floor point known: she stays (walk to a floor)",
                        old[0], old[1], old[2]);
    }
    if (g_ncLand.active) {
        if (!NcLandStep(&g_ncLand, &g_ncWalk, old, dt, out)) {
            NcWalkReset(&g_ncWalk);
            g_ncWalk.valid = 1, g_ncWalk.fx = out[0], g_ncWalk.fy = out[1], g_ncWalk.fz = out[2];
            LogV("no clip: landed at %.0f, %.0f, %.0f", out[0], out[1], out[2]);
        }
        out[3] = old[3];
        return 1;
    }
    wasLatched = g_ncLatched;
    if (!set && g_ncLatched && !roomChanged) clear = NwClearAt(old);
    if (!NcLatchStep(&g_ncLatched, set != 0, 0, roomChanged, clear)) {
        if (wasLatched && !g_ncLatched) LogV("no clip: %s: the stock collision again", roomChanged ? "a new room" : "she is clear");
        return 0;
    }
    if (!set && !s_logLatch++) LogV("no clip: turned off while she is inside geometry: kept on until she is clear");
    // a jump (load, scripted move, warp): the last floor point no longer counts
    if (g_ncLastOutOk && NcDist2XZ(old[0], old[2], g_ncLastOut[0], g_ncLastOut[2]) > 300.0f * 300.0f) NcWalkReset(&g_ncWalk);
    if (g_ncThru > 0.0f) g_ncThru -= dt;
    NcCompanions(old, dt);
    if (NcEventWalls(want) > 0) {
        if (NcEvery(&s_logEvent, 15000)) LogV("no clip: event walls near her at %.0f, %.0f, %.0f: they hold her (stock collision while they touch)", want[0], want[1], want[2]);
        g_ncLastOutOk = 0;
        g_ncWalk.onFloor = 1;
        return 0;
    }
    if (eff == 2) {  // Fly
        float f[3];
        // v3.37 MN337: her own speed: the game's step gives the direction only (NcFlyAbsStep)
        const float run = *(volatile const float *)(ULONG_PTR)NC_RUN_VA;
        // v3.37 MN337: the stick itself (0x4F8610 / 0x4F8660, read-only calls) and the tank side step, through Fly's own
        // short envelope (noclip_sim.h NcFlyTarget / NcFlyEnvStep: full in 0.12 s, stopped in 0.10 s)
        const float sx = ((float(__cdecl *)(int))0x4F8610)(0), sy = ((float(__cdecl *)(int))0x4F8660)(0);
        const float target = NcFlyTarget(sx, sy, *(volatile const float *)(ULONG_PTR)(NC_IN_VA + 0x0C), dt);
        float amount, speed;
        __declspec(align(16)) float w2[4];
        if (!g_ncFlying) g_ncFlyEnv = 0.0f;  // (from rest)
        amount = g_ncFlyEnv = NcFlyEnvStep(g_ncFlyEnv, target, dt);
        speed = NcFlySpeed(run, g_ncFlyK) * amount;
        if (!g_ncFlying) {
            g_ncFlying = 1;
            if (g_ncWalk.onFloor || NwClearAt(old)) g_ncWalk.valid = 1, g_ncWalk.fx = old[0], g_ncWalk.fy = old[1], g_ncWalk.fz = old[2];
            LogV("no clip: Fly from %.0f, %.0f, %.0f (back to this floor point if Fly ends over no floor); speed %.0f u/s running "
                 "([Gameplay] NoClipFlySpeed %.2f x the run speed %.0f), %.0f u/s walking", old[0], old[1], old[2],
                 NcFlySpeed(1.0f, g_ncFlyK), NcFlyK(g_ncFlyK), NC_RUN_US, NcFlySpeed(0.0f, g_ncFlyK));  // v3.37 MN337: + the speed
        }
        if (!(run == run) || run < -0.01f || run > 1.01f) {  // v3.37 MN337: the game's run value is 0..1
            static volatile LONG s_qRun;
            if (LogQuota(&s_qRun, 5)) LogAnomaly("no clip", "Fly: the game's run value (0x10A1A4C) reads %.3f, not 0..1: flown as %s", run,
                                                 run > 1.0f ? "a run" : "a walk");
        }
        if (!NcCamForward(old, f)) {
            out[0] = old[0], out[1] = old[1], out[2] = old[2];
            if (NcEvery(&s_logCam, 10000)) LogAnomaly("no clip", "Fly: the drawn camera (0xFD5AC0) has no usable view axis: she stays where she is");
        } else if (NcFlyAbsStep(old, want, speed, dt, w2)) {  // v3.37 MN337 (was: the game's step itself)
            w2[3] = want[3];
            NcFlyStep(&g_ncWorld, old, w2, f, out);
            LogT("no clip: fly %.0f u/s (run %.2f, input %.2f, dt %.4f): %.1f, %.1f, %.1f -> %.1f, %.1f, %.1f", speed, run, amount, dt, old[0],
                 old[1], old[2], out[0], out[1], out[2]);
        } else {
            out[0] = old[0], out[1] = old[1], out[2] = old[2];  // no input: she stays
        }
        out[3] = old[3];
        g_ncThru = NC_THRU_MEM;
        g_ncWalk.onFloor = 0;
        NcAfter(old, want, out, -1, dt);
        g_ncLastOut[0] = out[0], g_ncLastOut[2] = out[2], g_ncLastOutOk = 1;
        return 1;
    }
    inside = g_ncWalk.dSolid;
    dec = NcDecideOn(&g_ncWalk, &g_ncWorld, old, want, out);
    out[3] = dec == NC_HOLD ? old[3] : want[3];
    if (dec == NC_SOLID || dec == NC_AIR || dec == NC_CLIMB) g_ncThru = NC_THRU_MEM;
    NcLogDecision(old, out, dec, inside);
    NcAfter(old, want, out, dec, dt);
    LogT("no clip: %s %.1f, %.1f, %.1f -> %.1f, %.1f, %.1f (stairs %.0f)", kNcDecName[dec], old[0], old[1], old[2], out[0], out[1], out[2], g_ncWalk.va);
    g_ncLastOut[0] = out[0];
    g_ncLastOut[2] = out[2];
    g_ncLastOutOk = 1;
    return 1;
}
// 0x534D29: was `call 5339D0` (old, wanted, out; the caller pops)
static void __cdecl NcCollide(float *old, float *want, float *out)
{
    int done = 0;
    if (!g_ncFault) {
        __try {
            done = NcStep(old, want, out);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_ncFault = 1;  // fail closed: the stock collision from now on
            g_ncLatched = 0;
            g_ncFlying = 0;
            g_ncLand.active = 0;
            done = 0;
            LogAnomaly("no clip", "faulted (code %08lX): off for this session", GetExceptionCode());
        }
    }
    if (!done) ((void(__cdecl *)(float *, float *, float *))0x5339D0)(old, want, out);
}

// ---------------------------------------------------------------- the row and [Gameplay] NoClip
static void NcSetLocked(int v)
{
    if (v < 0 || v > 2) return;
    InterlockedExchange(&g_ncSet, v);
    g_ncUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "NoClip", kNcName[v], g_iniPath)) {
        g_ncUnsaved = 1;
        Log("gameplay: NoClip %s applies now but could not be saved in SH4Refined.ini (error %lu)", kNcName[v], GetLastError());
    } else {
        Log("gameplay: NoClip %s", kNcName[v]);
    }
}
static int __cdecl NcIn(BYTE *cfg, int count)
{
    int v, cur;
    (void)cfg;
    (void)count;
    if (!g_ncReady) return 0;
    GpLock();
    cur = (int)g_ncSet;
    v = StepValue(cur, 3);
    if (v == 2 && !CamOtsOn()) {  // Fly only with an over-the-shoulder camera: skipped, with the buzzer
        v = v == (cur + 1) % 3 ? 0 : 1;
        ((Fn1)0x56CC40)(0x9C44);
        LogV("no clip: Fly refused: no over-the-shoulder camera is on (Option 6 > Camera, or Playable Eileen's [Camera] Enable)");
    }
    if (v >= 0) NcSetLocked(v);
    GpUnlock();
    return 0;
}
static int __cdecl NcGet(void) { return (int)g_ncSet; }
static int __cdecl NcDesc(void)
{
    if (!g_ncReady) return MSG_GP0 + GPM_DPNA;
    return g_ncSet == 2 ? MSG_GP0 + GPM_NCD2 : g_ncSet ? MSG_GP0 + GPM_NCD1 : MSG_GP0 + GPM_NCD;
}
// 1 = draw this value greyed (Fly while no over-the-shoulder camera is on); the row refuses Fly meanwhile.
// v3.33 MERGE: drawn grey by sh4windowmod.c RowValueC (the value hook 0x525802) through NcValueIdGreyed.
static int NcValueGreyed(int v) { return v == 2 && !CamOtsOn(); }
static int NcValueIdGreyed(int id) { return id == MSG_GP0 + GPM_NCV2 && NcValueGreyed(2); }  // v3.33 MERGE: by value id
static void NcFillRow(DWORD *r)  // v3.37 MN337: Option 5's top row (was the Special page's row 4); Off / On are One-Hit Kill's ids
{
    g_ncIds[0] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 0;
    g_ncIds[1] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 1;
    g_ncIds[2] = MSG_GP0 + GPM_NCV2;
    FillRow(r, NcIn, NcGet, g_ncIds, 3, MSG_GP0 + GPM_NCL, NcDesc);
}
static void NcTexts(void)
{
    Encode(g_txtGp[GPM_NCL], 56, "No Clip");
    Encode(g_txtGp[GPM_NCD], 56, "Walk through walls and props (not in Room 302).");
    Encode(g_txtGp[GPM_NCD1], 56, "On: through walls and props; stairs still carry you.");
    Encode(g_txtGp[GPM_NCV2], 56, "Fly");
    Encode(g_txtGp[GPM_NCD2], 56, "Fly: fly fast where the camera looks (OTS camera).");  // v3.37 MN337: its own speed
}
static int NcParse(const char *s)
{
    if (!_stricmp(s, "Fly") || !strcmp(s, "2")) return 2;
    return !_stricmp(s, "On") || !strcmp(s, "1");
}
// v3.37 MN337: hidden [Gameplay] NoClipFlySpeed (x the run speed, 1.0 .. 4.0; missing or not a number = 2.5). 1 = changed.
static int NcReadFlyK(void)
{
    char buf[32];
    float k;
    GetPrivateProfileStringA("Gameplay", "NoClipFlySpeed", "", buf, sizeof buf, g_iniPath);
    k = buf[0] ? (float)atof(buf) : NC_FLY_K_DEF;
    if (buf[0] && !(k > 0.0f)) k = NC_FLY_K_DEF;  // (text that is no number: the default)
    k = NcFlyK(k);
    if (k == g_ncFlyK) return 0;
    g_ncFlyK = k;
    return 1;
}
static void NcReadIni(void)
{
    char buf[32];
    IniStr("Gameplay", "NoClip", buf, sizeof buf);
    g_ncSet = NcParse(buf);
    NcReadFlyK();  // v3.37 MN337
}
static void NcIniReread(void)  // under GpLock (GpIniReread)
{
    char buf[32];
    int v;
    if (!g_iniFallback && NcReadFlyK())  // v3.37 MN337
        Log("gameplay: [Gameplay] NoClipFlySpeed read again: %.2f (Fly runs at %.0f u/s, walks at %.0f)", g_ncFlyK,
            NcFlySpeed(1.0f, g_ncFlyK), NcFlySpeed(0.0f, g_ncFlyK));
    if (g_iniFallback || g_ncUnsaved) return;
    GetPrivateProfileStringA("Gameplay", "NoClip", kNcName[g_ncSet >= 0 && g_ncSet <= 2 ? g_ncSet : 0], buf, sizeof buf, g_iniPath);
    v = NcParse(buf);
    if (v == (int)g_ncSet) return;
    InterlockedExchange(&g_ncSet, v);
    g_ncSnap = v;
    Log("gameplay: [Gameplay] NoClip read again: %s", kNcName[v]);
}
static void NcSnapshot(void) { g_ncSnap = g_ncSet; }
static int NcChanged(void) { return g_ncSet != g_ncSnap; }
static int NcRowChanged(void) { return g_snapOn && g_ncSet != g_ncSnap; }
static void NcDiscard(void)  // under GpLock (GpDiscard)
{
    if (g_ncSet != g_ncSnap) NcSetLocked((int)g_ncSnap);
}
static void NcInstall(void)
{
    static const Site kSite = { 0x534D24, { 0x8D, 0x44, 0x24, 0x38, 0x50, 0xE8, 0xA2, 0xEC, 0xFF, 0xFF }, 10 };  // lea; push; call 5339D0
    GpPatch set[1];
    if (!SiteOk(&kSite)) {
        Log("no clip: 00534D29 is not the stock `call 005339D0` (another mod?): the option does nothing");
        return;
    }
    GpBranch(&set[0], 0x534D29, 0xE8, NcCollide, 5);
    g_ncReady = GpApplyAll(set, 1, "no clip");
    Log("no clip: %s (00534D29), [Gameplay] NoClip %s; Fly %s; Fly speed %.0f u/s running, %.0f walking (NoClipFlySpeed %.2f)",
        g_ncReady ? "in" : "OFF", kNcName[g_ncSet >= 0 && g_ncSet <= 2 ? g_ncSet : 0],
        CamOtsOn() ? "available (an over-the-shoulder camera is on)" : "waits for an over-the-shoulder camera",
        NcFlySpeed(1.0f, g_ncFlyK), NcFlySpeed(0.0f, g_ncFlyK), g_ncFlyK);  // v3.37 MN337: + the Fly speed
    if (!g_ncReady && g_ncSet) LogAnomaly("no clip", "[Gameplay] NoClip is %s but the hook is not in: the option does nothing", kNcName[g_ncSet]);
}
