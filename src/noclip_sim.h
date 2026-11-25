// v3.32 NC332 / v3.33 NC333: No Clip, the pure rules (no game memory, no Windows): included by src/noclip.inc, by the
// native test test/noclip_sim_test.c and by the real-collision harness staging/v333_feedback/w3_noclip/re (nc_harness).
// Plans: staging/v332_feedback/w4_noclip/PLAN.md, staging/v333_feedback/w3_noclip/PLAN.md.
//
// v3.33 (the user's QA of v3.32: "pass through everything on the horizontal; unable to climb or descend stairs"):
// the room's second collision tree (tree2, 0x561CD0 / 0x561CB0) is a BSP of SOLID space: walls, props, stair blocks
// and the ground are solid volumes; a line that starts inside solid finds nothing. v3.32 probed the floor from 60 above
// her feet: inside a wall, a tall prop or a stair block that start point is solid, so she walked on at her height and
// held 120 later; on stairs (40-unit risers flanked by full-height walls in the Subway) any drift into a side wall left
// her stuck at one height (the softlock), and long props held her. v3.33:
//   1. While she stands on a floor (feet open), the stock walk step 0x5339D0 runs first; when it lets her make the full
//      step it is used as it is (stairs, slopes and steps exactly as in the stock game).
//   2. Otherwise (the stock step blocks or slides her, or she is inside geometry) she passes through horizontally:
//      her feet inside solid -> she walks on at her height with no distance limit (any wall or prop), climbing out on
//      top only when that top is within NC_RISE_FLAT, or, while she is on stairs (recent height changes, NcWalk.va),
//      within NC_RISER + NC_SLOPE x the distance walked since the last floor (a wall beside a staircase);
//      her feet in open space -> the first floor from her feet down NC_DROP (more on stairs) carries her;
//      no floor at all (past the end of a floor, out of the room's box) -> NC_GAP from the last floor point, then she
//      holds; a move back toward that point or into solid is always allowed (never stuck).
//   Distances only: the same rule at every frame rate (va decays by distance walked, not by time).
#ifndef NOCLIP_SIM_H
#define NOCLIP_SIM_H

#include <math.h>

#define NC_FEET 8.0f          // the feet test point: this far above her feet (y grows downward)
#define NC_SCAN 20.0f         // the step of the upward scan through solid for a top
#define NC_RISE_FLAT 24.0f    // passing through off stairs: a top this low is stepped onto (thresholds), higher props not
#define NC_RISER 45.0f        // on stairs: one riser (the Subway's are 40) ...
#define NC_SLOPE 0.75f        // ... plus this per unit walked inside solid since the last floor (stairs rise <= 0.6)
#define NC_CLIMB_MAX 600.0f   // the scan never looks higher than this
#define NC_DROP 100.0f        // feet in the open: the floor is looked for this far down (as the stock snap)
#define NC_GAP 120.0f         // no floor: she walks on this far from the last floor point (xz), then holds
#define NC_FULL 0.5f          // the stock step counts as full when it ends this close (xz) to the wanted point
#define NC_STAIR_VA 15.0f     // va at or above this = she is on stairs or a slope
#define NC_VA_LEN 150.0f      // va decays by e per this distance walked on floors
#define NC_LAND_SPEED 2400.0f // leaving Fly: she sinks this fast (units / s) to the floor below
#define NC_LAND_RANGE 6000.0f // ... found this far down at most
#define NC_WARP_NEAR 250.0f   // a companion farther than this, the line to the player blocked, after the player went
#define NC_WARP_WAIT 2.0f     // through geometry (NC_THRU_MEM), for this long, is placed beside the player
#define NC_WARP_DIST 600.0f   // without a recent pass-through: farther than this ...
#define NC_WARP_WAIT_FAR 3.0f // ... for this long (= v3.32)
#define NC_WARP_COOL 2.0f     // and not again for this long
#define NC_THRU_MEM 30.0f     // a pass-through counts as recent this long (s)
#define NC_HELD_ANOMALY 3.0f  // input held this long without moving: a softlock suspicion

enum { NC_HOLD, NC_STOCK, NC_FLOOR, NC_CLIMB, NC_SOLID, NC_AIR };
static const char *const kNcDecName[6] = { "hold", "stock", "floor", "climb", "solid", "air" };

// The collision queries (the game's in noclip_world.h; synthetic ones in the test). y grows downward.
typedef struct NcWorld {
    int (*solid)(void *ctx, float x, float y, float z);  // 1 solid, 0 open, -1 unknown (no tree2)
    int (*floorAt)(void *ctx, float x, float yTop, float yBot, float z, float *fy);  // first surface from yTop down
    void (*stock)(void *ctx, const float *old, const float *want, float *out);      // the stock walk step (x, y, z)
    void *ctx;
    int boxValid;
    float boxMin[3], boxMax[3];  // the room's collision box
} NcWorld;

typedef struct {
    int valid;          // a floor point is known (reset at a room change)
    float fx, fy, fz;   // the last point she stood on a floor
    int onFloor;        // she stands on a floor with her feet in the open (the stock step is tried first)
    float va;           // vertical activity: |dy| summed over floor steps, decaying by distance (stairs / slopes)
    float dSolid;       // distance walked (xz) since the last floor step
    int lastDec;        // the last decision (NC_*)
    float lastTop;      // diagnostics: the top found by the last climb scan (or 0)
} NcWalk;

static int NcFinite(float v) { return v == v && v - v == 0.0f; }
static int NcFinite3(const float *p) { return NcFinite(p[0]) && NcFinite(p[1]) && NcFinite(p[2]); }

static float NcDist2XZ(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx, dz = az - bz;
    return dx * dx + dz * dz;
}

static void NcWalkReset(NcWalk *w)
{
    w->valid = 0;
    w->onFloor = 1;  // a room's entry point stands on a floor
    w->va = 0.0f;
    w->dSolid = 0.0f;
    w->lastDec = NC_STOCK;
    w->lastTop = 0.0f;
}

static int NcInBox(const NcWorld *wd, const float *p)
{
    if (!wd->boxValid) return 1;
    return p[0] >= wd->boxMin[0] && p[0] <= wd->boxMax[0] && p[2] >= wd->boxMin[2] && p[2] <= wd->boxMax[2];
}

// She stands on a floor at out after a step from old.
static void NcFloorStep(NcWalk *w, const float *old, const float *out)
{
    const float dxz = sqrtf(NcDist2XZ(old[0], old[2], out[0], out[2]));
    float dy = fabsf(out[1] - old[1]);
    if (dy > NC_RISER) dy = NC_RISER;  // one riser at most per step (a climb out of solid or a snap counts as one)
    w->va = w->va * expf(-dxz / NC_VA_LEN) + dy;
    w->valid = 1;
    w->fx = out[0];
    w->fy = out[1];
    w->fz = out[2];
    w->dSolid = 0.0f;
    w->onFloor = 1;
}

static int NcSet(float *out, float x, float y, float z, int dec)
{
    out[0] = x;
    out[1] = y;
    out[2] = z;
    return dec;
}

// The surface at (x, z) relative to base (rise = base - surface y; y grows downward): her feet point inside solid ->
// the top of that solid, found by stepping up NC_SCAN at a time to the first open point and probing down from it
// (1, *rise; 0 = solid all the way to maxUp: a wall or a tall prop); feet point open -> the floor below within NC_DROP
// (1, *rise <= NC_FEET; -1 = none: a drop or the void).
static int NcSurface(const NcWorld *wd, float x, float z, float base, float maxUp, float *rise)
{
    float h, fy;
    const int feet = wd->solid(wd->ctx, x, base - NC_FEET, z);
    if (feet != 1) {
        if (wd->floorAt(wd->ctx, x, base - NC_FEET, base + NC_DROP, z, &fy) && NcFinite(fy)) {
            *rise = base - fy;
            return 1;
        }
        return -1;
    }
    for (h = NC_FEET + NC_SCAN; h <= maxUp + NC_SCAN && h <= NC_CLIMB_MAX; h += NC_SCAN) {
        if (wd->solid(wd->ctx, x, base - h, z) != 0) continue;
        if (wd->floorAt(wd->ctx, x, base - h, base, z, &fy) && NcFinite(fy) && fy <= base) {
            *rise = base - fy;
            return *rise <= maxUp;
        }
        return 0;
    }
    return 0;
}

// A step up from level ground? A top NC_RISE_FLAT .. NC_RISER above her (the stock step's or one she is inside) is
// taken only when the ground along her move is a staircase (another rise of 20 .. NC_RISER within 180) or a floor (level
// for 315 on). A low box, a bed or a seat is level for less and then drops; a backrest or a wall rises more than a
// riser: she walks through those at floor level. Samples every 45 along the move from the wanted point.
#define NC_AHEAD_STEP 45.0f
#define NC_AHEAD_N 7
static int NcStepOk(const NcWorld *wd, const float *old, const float *want, float base, float rise0)
{
    float dx = want[0] - old[0], dz = want[2] - old[2], l = sqrtf(dx * dx + dz * dz), prev = rise0, rk;
    int k;
    if (!(l > 1e-4f)) return 0;
    dx /= l, dz /= l;
    for (k = 1; k <= NC_AHEAD_N; ++k) {
        const float d = NC_AHEAD_STEP * (float)k;
        if (NcSurface(wd, want[0] + dx * d, want[2] + dz * d, base, 225.0f, &rk) != 1) return 0;
        if (rk < prev - 4.0f || rk - prev > NC_RISER) return 0;
        if (rk - prev >= 20.0f) return k <= 4;  // the next riser within 180: a staircase
        prev = rk;
    }
    return 1;  // level for 315: a floor (a platform, a raised walkway)
}

// One walk step while No Clip is On. old / want: x, y, z; out: x, y, z (out[3] is not touched). Returns NC_*.
static int NcDecideOn(NcWalk *w, const NcWorld *wd, const float *old, const float *want, float *out)
{
    const float x = want[0], z = want[2];
    const float dxz = NcFinite3(old) && NcFinite3(want) ? sqrtf(NcDist2XZ(old[0], old[2], x, z)) : 0.0f;
    const int stairs = w->va >= NC_STAIR_VA;
    float hint, dS, fy = 0.0f;
    int feet;
    w->lastTop = 0.0f;
    if (!NcFinite3(old) || !NcFinite3(want)) {
        if (NcFinite3(old)) NcSet(out, old[0], old[1], old[2], NC_HOLD);
        return w->lastDec = NC_HOLD;
    }
    hint = old[1];
    if (w->onFloor) {  // 1. the stock step first
        float s[3];
        wd->stock(wd->ctx, old, want, s);
        if (NcFinite3(s)) {
            if (NcDist2XZ(s[0], s[2], x, z) <= NC_FULL * NC_FULL && fabsf(s[1] - old[1]) <= NC_DROP + 1.0f &&
                (old[1] - s[1] <= NC_RISE_FLAT ||
                 (old[1] - s[1] <= NC_RISER && (stairs || NcStepOk(wd, old, want, old[1], old[1] - s[1]))))) {
                NcSet(out, s[0], s[1], s[2], NC_STOCK);
                NcFloorStep(w, old, out);
                return w->lastDec = NC_STOCK;
            }
            // blocked or slid: the height it reached along the wall carries on (a staircase beside a wall); a step up
            // onto a prop (not a staircase or a floor) is not taken: she passes through it at her height
            if (NcDist2XZ(s[0], s[2], old[0], old[2]) > 0.0001f && fabsf(s[1] - old[1]) <= NC_RISER &&
                (stairs || old[1] - s[1] <= NC_RISE_FLAT))
                hint = s[1];
        }
    }
    // 2. pass through
    dS = w->dSolid + dxz;
    feet = NcInBox(wd, want) ? wd->solid(wd->ctx, x, hint - NC_FEET, z) : 2;
    if (feet == 1) {  // inside a wall, a prop or a stair block
        const float allow = stairs ? NC_RISER + NC_SLOPE * dS : NC_RISE_FLAT;
        float rise = 0.0f;
        const int top = NcSurface(wd, x, z, hint, allow > NC_RISER ? allow : NC_RISER, &rise);
        if (top == 1 && rise >= 0.0f &&
            (rise <= allow || (w->onFloor && rise <= NC_RISER && NcStepOk(wd, old, want, hint, rise)))) {
            w->lastTop = rise;
            NcSet(out, x, hint - rise, z, NC_CLIMB);
            NcFloorStep(w, old, out);
            return w->lastDec = NC_CLIMB;
        }
        w->dSolid = dS;
        w->onFloor = 0;
        return w->lastDec = NcSet(out, x, hint, z, NC_SOLID);
    }
    if (feet == 0 || feet == -1) {  // feet in the open (or no solid tree: the v3.32-like probe)
        const float up = feet == 0 ? NC_FEET : NC_RISE_FLAT;
        float down = NC_DROP;
        if (stairs && NC_RISER + NC_SLOPE * dS > down) down = NC_RISER + NC_SLOPE * dS;
        if (wd->floorAt(wd->ctx, x, hint - up, hint + down, z, &fy) && NcFinite(fy)) {
            NcSet(out, x, fy, z, NC_FLOOR);
            NcFloorStep(w, old, out);
            return w->lastDec = NC_FLOOR;
        }
    }
    // 3. no floor (past the end of a floor, out of the room's box): a short way, then hold; back is always allowed
    w->onFloor = 0;
    if (!w->valid) {
        if (feet != 2) {
            w->dSolid = dS;
            return w->lastDec = NcSet(out, x, old[1], z, NC_AIR);  // nothing known: fail open (inside the box)
        }
    } else {
        const float d2 = NcDist2XZ(x, z, w->fx, w->fz);
        if (d2 <= NC_GAP * NC_GAP || d2 <= NcDist2XZ(old[0], old[2], w->fx, w->fz) + 0.01f) {
            w->dSolid = dS;
            return w->lastDec = NcSet(out, x, old[1], z, NC_AIR);
        }
    }
    return w->lastDec = NcSet(out, old[0], old[1], old[2], NC_HOLD);
}

// Fly: she moves where the camera looks. fwd = the camera's view direction (unit, y down). The game's walk step
// (want - old, xz) is split into the part along the camera's heading, which follows the camera's pitch (look up and
// walk to rise, look down to sink; back = the reverse), and the part across it, which stays level. No input: she stays.
// No floors, no walls; the room's collision box keeps her in the room. Returns 1 when she moved.
static int NcFlyStep(const NcWorld *wd, const float *old, const float *want, const float *fwd, float *out)
{
    float dx, dz, l2, fx, fy, fz, fh, hx, hz, along, n;
    int i;
    out[0] = old[0];
    out[1] = old[1];
    out[2] = old[2];
    if (!NcFinite3(old) || !NcFinite3(want) || !NcFinite3(fwd)) return 0;
    dx = want[0] - old[0];
    dz = want[2] - old[2];
    l2 = dx * dx + dz * dz;
    if (!(l2 > 1e-8f)) return 0;
    fx = fwd[0], fy = fwd[1], fz = fwd[2];
    n = sqrtf(fx * fx + fy * fy + fz * fz);
    if (!(n > 1e-6f)) return 0;
    fx /= n, fy /= n, fz /= n;
    fh = sqrtf(fx * fx + fz * fz);
    if (fh < 0.1f) {  // (nearly) straight up / down: the pitch is held at about 84 degrees; the heading is the camera's
        const float l = sqrtf(l2);  // own while it has one, else her walk direction (all of the step counts as forward)
        hx = fh > 1e-3f ? fx / fh : dx / l;
        hz = fh > 1e-3f ? fz / fh : dz / l;
        fy = fy < 0.0f ? -0.99498744f : 0.99498744f;
        fh = 0.1f;
    } else {
        hx = fx / fh, hz = fz / fh;
    }
    along = dx * hx + dz * hz;
    out[0] = old[0] + (dx - along * hx) + along * fh * hx;
    out[2] = old[2] + (dz - along * hz) + along * fh * hz;
    out[1] = old[1] + along * fy;
    if (wd && wd->boxValid)
        for (i = 0; i < 3; ++i) {
            if (out[i] < wd->boxMin[i]) out[i] = wd->boxMin[i];
            if (out[i] > wd->boxMax[i]) out[i] = wd->boxMax[i];
        }
    if (!NcFinite3(out)) {
        out[0] = old[0], out[1] = old[1], out[2] = old[2];
        return 0;
    }
    return 1;
}

// v3.37 MN337 (the user: "Give the No Clip Fly setting its own speed that's adequately fast ~2x to 3x normal running
// speed"): Fly flies at a speed of its own. The game's step (want - old) gives only the direction; its length is set
// here: running input (the game's input block's run, 0x10A1A00 + 0x4C, 0..1, eased by the game over about 1 s) flies
// [Gameplay] NoClipFlySpeed x the run speed (663 u/s; default 2.5 = about 1660 u/s), walking input the run speed, between
// them in step with the run value; no input, no move (NcFlyTarget / NcFlyEnvStep: the stick's deflection scales it
// through a short envelope of Fly's own: full in 0.12 s, stopped in 0.10 s). Movement Speed does not stack (only the direction is taken). Frame rate free: the
// length is speed x the game tick's frame time (0x55AE60, clamped to 0.1 s by NcStep).
#define NC_RUN_US 663.0f      // the game's run speed, units a second (walk 175.5)
#define NC_FLY_K_DEF 2.5f     // [Gameplay] NoClipFlySpeed (hidden), 1.0 .. 4.0
#define NC_FLY_K_MIN 1.0f
#define NC_FLY_K_MAX 4.0f
static float NcFlyK(float k)  // the ini value clamped (not a number = the default)
{
    if (!(k == k)) return NC_FLY_K_DEF;
    return k < NC_FLY_K_MIN ? NC_FLY_K_MIN : k > NC_FLY_K_MAX ? NC_FLY_K_MAX : k;
}
// the speed (u/s) for the game's run value (0 walk .. 1 run; anything else is clamped, not a number = walk)
static float NcFlySpeed(float run, float k)
{
    if (!(run > 0.0f)) run = 0.0f;
    if (run > 1.0f) run = 1.0f;
    k = NcFlyK(k);
    return NC_RUN_US * (1.0f + (k - 1.0f) * run);
}
// How much she is asked to move, 0..1, from the game's input block (0x10A1A00: +0x00 forward, +0x0C side, both scaled by
// the builders 0x52EEE0 / 0x52F260 to frame time x stick x 30): the stick's deflection (side steps reach 0.8, so 0.8 and
// more is full). No input = 0 (a push alone, e.g. an enemy shoving her, never flies her). Fly takes only the side step
// from here (NcFlyTarget): the forward value is the game's eased one (0x52F260's D_010A191C, about 0.67 s each way).
// Not a number = 0.
static float NcFlyInputAmount(float forward, float side, float dt)
{
    float a, s;
    if (!(dt > 0.0f) || !NcFinite(forward) || !NcFinite(side)) return 0.0f;
    a = forward < 0.0f ? -forward : forward;
    s = side < 0.0f ? -side : side;
    a = (a > s ? a : s) / (dt * 30.0f) / 0.8f;
    return a > 1.0f ? 1.0f : a;
}
// v3.37 MN337 (the coordinator's review: "a 680-unit glide after release is too floaty"): Fly does not use the game's
// eased stick (0x52F260 eases the magnitude at 1.5 a second both ways: about 0.67 s to start and to stop) but the stick
// itself (InputLeftStickGetX / Y 0x4F8610 / 0x4F8660, which the builders read; the keyboard's move keys come through them
// too) and the tank side step (the block's side, not eased), with a short envelope of its own: full in NC_FLY_UP_S, to 0
// in NC_FLY_DOWN_S (at 1657.5 u/s she stops within about 83 units). No stick and no side step = target 0: a push alone
// never flies her.
#define NC_FLY_UP_S 0.12f     // the start ramp, seconds from rest to full
#define NC_FLY_DOWN_S 0.10f   // the stop, seconds from full to rest
#define NC_FLY_DEAD 0.05f     // stick deflection below this is no input
// the target 0..1 from the stick (x, y: -1..1; the larger of the two, 0.8 and more = full) and the block's side (as
// NcFlyInputAmount); not a number = 0
static float NcFlyTarget(float stickX, float stickY, float side, float dt)
{
    float m, s;
    stickX = NcFinite(stickX) ? (stickX < 0.0f ? -stickX : stickX) : 0.0f;
    stickY = NcFinite(stickY) ? (stickY < 0.0f ? -stickY : stickY) : 0.0f;
    m = stickX > stickY ? stickX : stickY;
    m = m < NC_FLY_DEAD ? 0.0f : m / 0.8f;
    if (m > 1.0f) m = 1.0f;
    s = NcFlyInputAmount(0.0f, side, dt);
    return m > s ? m : s;
}
// the envelope one tick toward the target (up at 1 / NC_FLY_UP_S, down at 1 / NC_FLY_DOWN_S a second)
static float NcFlyEnvStep(float env, float target, float dt)
{
    if (!NcFinite(env) || env < 0.0f) env = 0.0f;
    if (!NcFinite(target) || target < 0.0f) target = 0.0f;
    if (!(dt > 0.0f)) return env;
    if (env < target) {
        env += dt / NC_FLY_UP_S;
        if (env > target) env = target;
    } else if (env > target) {
        env -= dt / NC_FLY_DOWN_S;
        if (env < target) env = target;
    }
    return env > 1.0f ? 1.0f : env;
}
// The game's step (want - old, xz) rescaled to speed x dt: *w2 = old + its direction x that length (y = old's: the
// vertical part comes from NcFlyStep, along the camera's pitch). Returns 0 when there is no step (no input) or no time.
static int NcFlyAbsStep(const float *old, const float *want, float speed, float dt, float *w2)
{
    float dx, dz, l;
    w2[0] = old[0], w2[1] = old[1], w2[2] = old[2];
    if (!NcFinite3(old) || !NcFinite3(want) || !(speed > 0.0f) || !(dt > 0.0f) || !NcFinite(speed * dt)) return 0;
    dx = want[0] - old[0];
    dz = want[2] - old[2];
    l = sqrtf(dx * dx + dz * dz);
    if (!(l > 1e-4f)) return 0;
    w2[0] = old[0] + dx / l * speed * dt;
    w2[2] = old[2] + dz / l * speed * dt;
    return 1;
}

// Leaving Fly (the row, or the camera turned off): she sinks to the floor below, or goes back to the last floor point.
typedef struct {
    int active;
    int mode;        // 1 sinking to ty, 2 placed at the last floor point (done in one step)
    float ty;        // the floor below
} NcLand;

// Starts a landing at p. Returns 1 = sink to the floor below (l->ty), 2 = back to the last floor point, 0 = she
// already stands on a floor, 3 = no floor below and no floor point known: she stays (the On rules carry on).
static int NcLandBegin(NcLand *l, const NcWorld *wd, const NcWalk *w, const float *p)
{
    float fy;
    const int feet = NcFinite3(p) && NcInBox(wd, p) ? wd->solid(wd->ctx, p[0], p[1] - NC_FEET, p[2]) : 2;
    l->active = 0;
    l->mode = 0;
    if (feet == 0 || feet == -1) {
        if (wd->floorAt(wd->ctx, p[0], p[1] - NC_FEET, p[1] + NC_LAND_RANGE, p[2], &fy) && NcFinite(fy) && fy >= p[1] - NC_FEET) {
            if (fy - p[1] < 1.0f) return 0;  // on the floor already
            l->active = 1;
            l->mode = 1;
            l->ty = fy;
            return 1;
        }
    }
    if (w->valid) {
        l->active = 1;
        l->mode = 2;
        return 2;
    }
    return 3;
}
// One landing step: out = where she is this tick. Returns 1 while it goes on, 0 when done (out written either way).
static int NcLandStep(NcLand *l, const NcWalk *w, const float *old, float dt, float *out)
{
    out[0] = old[0];
    out[1] = old[1];
    out[2] = old[2];
    if (!l->active) return 0;
    if (l->mode == 2) {
        out[0] = w->fx, out[1] = w->fy, out[2] = w->fz;
        l->active = 0;
        return 0;
    }
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    out[1] = old[1] + NC_LAND_SPEED * dt;
    if (!(out[1] < l->ty)) {
        out[1] = l->ty;
        l->active = 0;
        return 0;
    }
    return 1;
}

// The latch: turning No Clip off while she stands inside a wall or a prop would leave her stuck (the stock collision
// cannot move her out), so it stays active until she is clear (a floor under her and no polygon round her body).
//   roomChanged: a new room (she starts at its entry point: clear) -> the latch is dropped
//   suspended:   Room 302, an event, a cut-scene, a menu: the stock collision runs, the latch is KEPT
// Returns 1 when this step uses No Clip.
static int NcLatchStep(int *latched, int setting, int suspended, int roomChanged, int clear)
{
    if (roomChanged) *latched = 0;
    if (suspended) return 0;
    if (setting) {
        *latched = 1;
        return 1;
    }
    if (*latched && clear) *latched = 0;
    return *latched;
}

// The companions' catch-up: they path-find round walls and cannot follow her through one. A companion that can follow
// (eligible) and whose line to the player is blocked is placed beside the player after
//   NC_WARP_WAIT s farther than NC_WARP_NEAR while the player went through geometry in the last NC_THRU_MEM s, or
//   NC_WARP_WAIT_FAR s farther than NC_WARP_DIST otherwise (v3.32's rule).
// dt = the game tick's frame time. Returns 1 = place her now.
typedef struct {
    float behind;  // seconds behind so far
    float cool;    // seconds until the next warp may happen
} NcWarp;

static int NcWarpStep(NcWarp *s, int eligible, int blocked, float dist, int thruRecent, float dt)
{
    const float nearDist = thruRecent ? NC_WARP_NEAR : NC_WARP_DIST;
    const float wait = thruRecent ? NC_WARP_WAIT : NC_WARP_WAIT_FAR;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;  // a hitch or a long pause counts as one short tick
    if (s->cool > 0.0f) s->cool -= dt;
    if (!eligible || !blocked || !(dist > nearDist)) {
        s->behind = 0.0f;
        return 0;
    }
    s->behind += dt;
    if (s->behind >= wait && !(s->cool > 0.0f)) {
        s->behind = 0.0f;
        s->cool = NC_WARP_COOL;
        return 1;
    }
    return 0;
}

// The softlock watch: input held (a wanted step) but no movement. Returns 1 once when it passes NC_HELD_ANOMALY s.
typedef struct {
    float t;
    int reported;
} NcHeld;

static int NcHeldStep(NcHeld *h, int input, int moved, float dt)
{
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (!input || moved) {
        h->t = 0.0f;
        h->reported = 0;
        return 0;
    }
    h->t += dt;
    if (h->t >= NC_HELD_ANOMALY && !h->reported) {
        h->reported = 1;
        return 1;
    }
    return 0;
}

// Distance (xz) from p to the segment a-b, squared.
static float NcSegDist2XZ(float px, float pz, float ax, float az, float bx, float bz)
{
    const float ex = bx - ax, ez = bz - az, l2 = ex * ex + ez * ez;
    float t = 0.0f;
    if (l2 > 0.0f) {
        t = ((px - ax) * ex + (pz - az) * ez) / l2;
        if (t < 0.0f) t = 0.0f;
        else if (t > 1.0f) t = 1.0f;
    }
    return NcDist2XZ(px, pz, ax + t * ex, az + t * ez);
}

// ---------------------------------------------------------------- v3.34 NC334: the companion's feet after a placement
// The user's QA of v3.33: "Eileen's legs float for a moment" after No Clip placed her beside the player. The cause
// (SH4Decomp src/eileen/eileen_43d120.c): the companion's foot IK (EileenFeetIkRootUpdate 0x43E450, run by her task
// after the clip every tick) keeps each foot's target in WORLD space in two globals, EileenFootIk 0xFCE020 / 0xFCE070
// (shared by Eileen and Cynthia, both made by EileenMotionCreate 0x43DF30). The model position (0x55C360) and
// 0x4416C0 move her body, not those targets: a planted foot (mode 1) stayed on the old spot 1141 away until the IK
// solver gave up (mode 2) and the target blended back over 5 ticks (0x43D3E0, blend +0.2), a swinging foot (mode 3)
// blended from there too (+0.3): her legs stretched toward where she came from. The game never moves a built
// companion: at a room entry EileenBuildModel 0x445BD0 calls 0x4416C0 and THEN EileenMotionCreate, whose foot set-up
// (0x43DF30, FootIkInit) is: offset 0, unk_04 0, mode 0, target = the bone's position. NcIkCarry does that set-up on
// the existing motion at her new spot: the target moves with her (+d), the modes start again (step 0: the next tick
// picks stance / swing from the clip and a planted foot goes onto the new floor), and the offset starts at the floor
// under that foot (stairs, slopes: the foot is on its tread at once; 0 on a flat floor or with no floor found).
#define NC_IK_NEAR 400.0f    // a foot target farther than this (xz) from her old spot is not hers (Cynthia / Eileen
                             // share the globals): left alone
#define NC_ANKLE 30.0f       // the IK sets a planted foot's target this far above the floor (0x43D290: ground - 30)
#define NC_IK_OFF_MAX 60.0f  // the starting offset only within this (the IK ignores ground farther than 100)
#define NC_FEET_OFF 20.0f    // anomaly: no foot within this of the floor ...
#define NC_FEET_CHECK 0.5f   // ... from this long after a placement ...
#define NC_FEET_SPAN 0.3f    // ... over this long (the best foot of any tick: a walking foot is lifted half the time)
#define NC_FEET_FAR 150.0f   // a foot target this far (xz) from her body counts as off the floor (the v3.33 stretch)

// EileenFootIk (SH4Decomp include/eileen.h; checked against 0x43D290: +0x08 mode, +0x0C step, +0x14 target, +0x20 pos,
// +0x44 ground). 0x50 bytes (x86).
typedef struct NcFootIk {
    float offset;      // +0x00 height added to the clip's target (moves 4 per tick)
    float unk04;       // +0x04
    int mode;          // +0x08 1 stance, 2 stance out of reach, 3 swing, else the clip (0x43D3E0)
    int step;          // +0x0C 0 = the mode's first tick
    void *ik;          // +0x10
    void *target;      // +0x14 the target bone (NULL: no IK)
    void *foot;        // +0x18
    float roll;        // +0x1C
    float pos[4];      // +0x20 the world position given to the target bone
    float normal[4];   // +0x30
    int grounded;      // +0x40
    float ground;      // +0x44 stance: the height the foot goes to
    float blend;       // +0x48
    int unk4C;         // +0x4C
} NcFootIk;
typedef char NcFootIkSize[sizeof(NcFootIk) == 0x50 ? 1 : -1];

// One foot after the companion moved by d from `from` (her model position before the move). haveFloor / floorY: the
// floor under the moved target. 1 = carried, 0 = no IK on this foot, -1 = not hers / not a number (left alone).
static int NcIkCarry(NcFootIk *f, const float *from, const float *d, int haveFloor, float floorY)
{
    float off;
    if (!f->target) return 0;
    if (!NcFinite3(f->pos) || !NcFinite3(d) || !NcFinite3(from)) return -1;
    if (NcDist2XZ(f->pos[0], f->pos[2], from[0], from[2]) > NC_IK_NEAR * NC_IK_NEAR) return -1;
    f->pos[0] += d[0], f->pos[1] += d[1], f->pos[2] += d[2];
    f->ground += d[1];
    off = haveFloor && NcFinite(floorY) ? floorY - NC_ANKLE - f->pos[1] : 0.0f;
    f->offset = fabsf(off) <= NC_IK_OFF_MAX ? off : 0.0f;
    f->unk04 = 0.0f;
    f->mode = 0;
    f->step = 0;
    return 1;
}

// How far one foot target is off its floor (|floor - NC_ANKLE - y|); a target NC_FEET_FAR (xz) from her body counts as
// that far off. < 0 = unknown (no floor found).
static float NcFootGap(const float *foot, const float *body, int haveFloor, float floorY)
{
    float far2;
    if (!NcFinite3(foot) || !NcFinite3(body)) return 1e6f;
    far2 = NcDist2XZ(foot[0], foot[2], body[0], body[2]);
    if (far2 >NC_FEET_FAR * NC_FEET_FAR) return sqrtf(far2);
    if (!haveFloor || !NcFinite(floorY)) return -1.0f;
    return fabsf(floorY - NC_ANKLE - foot[1]);
}

// The watch after a placement: gap = the best foot's gap this tick (< 0 unknown). Returns 0 while it runs, then once:
// 1 = a foot was within NC_FEET_OFF of the floor at some tick from NC_FEET_CHECK to NC_FEET_CHECK + NC_FEET_SPAN s,
// 2 = no foot was (her feet float: an anomaly), 3 = no gap was known in that time. Seconds: the same at every rate.
typedef struct {
    int active, ticks;
    float t, best;
} NcFeetWatch;

static void NcFeetBegin(NcFeetWatch *w)
{
    w->active = 1;
    w->ticks = 0;
    w->t = 0.0f;
    w->best = -1.0f;
}
static int NcFeetStep(NcFeetWatch *w, float gap, float dt)
{
    if (!w->active) return 0;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    w->t += dt;
    ++w->ticks;
    if (w->t >= NC_FEET_CHECK - 1e-4f && gap >= 0.0f && (w->best < 0.0f || gap < w->best)) w->best = gap;
    if (w->t < NC_FEET_CHECK + NC_FEET_SPAN - 1e-4f) return 0;
    w->active = 0;
    return w->best < 0.0f ? 3 : w->best > NC_FEET_OFF ? 2 : 1;
}

#endif
