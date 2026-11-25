// v3.43 GT343: Ghost Teleport, the pure rules (no game memory, no Windows): included by src/teleport.inc and by the
// native test test/gt_sim_test.c. Plan: staging/v343_feedback/w2_teleport/PLAN.md; v3.44: staging/v344_feedback/
// w3_teleport/PLAN.md.
//
// v3.44 GT344 (the user's v3.43 QA): the trigger is Run + Action pressed together (both down within GT_CHORD_S, either
// first; no standing still, no hold); the transition is ghost Richard's own: his see-through fade (ScratchUpdateAlpha
// 0x4B9900: 0.5 s out, the warp, 0.5 s in; the transparency in GtFadeSee, the draw's FIX value in GtFadeFix) and his
// sound; with no standing enemy in range she warps GtFwdKey units the way she faces (GtFwdCand, shortened step by step).
// The target spot rules (behind the nearest standing enemy, the room's edges, fail closed) are v3.43's.
// Then a cooldown (default GT_COOL_DEF). A refused or failed warp waits GT_FAIL_COOL.
//
// v3.49 GT349 (the user's v3.48 QA; staging/v349_feedback/w2_teleport/PLAN.md, CRASH_ANALYSIS.md): no cooldown by
// default (GT_COOL_DEF 0: the next warp as soon as she has faded back in); holding Run + Action keeps warping
// (GtHold*); a warp never crosses or ends in a walk-in event's trigger zone the game would still fire (GtZoneHit:
// v3.48 jumped over the Subway 2:2 cut-scene box, Cynthia's companion task then came back in 2:3 without her files and
// the game crashed at 0x43E775).
#ifndef TELEPORT_SIM_H
#define TELEPORT_SIM_H
#include <float.h>
#include <math.h>

#define GT_RANGE_DEF   2500.0f  // [Gameplay] GhostTeleportRange (hidden): enemies within this (xz) of her
#define GT_RANGE_MIN   500.0f
#define GT_RANGE_MAX   6000.0f
#define GT_COOL_DEF    0.0f     // [Gameplay] GhostTeleportCooldown (hidden): seconds after a warp (v3.49 GT349: 0, was 3)
#define GT_COOL_MIN    0.5f     //   (a positive value in the ini: clamped to MIN .. MAX)
#define GT_COOL_MAX    30.0f
#define GT_FAIL_COOL   0.5f     // after a refused or failed warp (the spot search is not run every tick)
#define GT_DY_MAX      600.0f   // an enemy this far above / below her feet is on another floor: not taken
#define GT_ACTOR_MAX   128      // GT343-004: the enemy list is walked to its end (this bounds a broken list only: the
                                // records table holds 64 enemies, Enemy Copies included), every actor kept
#define GT_PICK_MAX    3        // the nearest this many enemies are tried, nearest first
#define GT_RINGS       3        // spot rings behind the enemy: its body radius + GT_RING0 .. (GT_RING_STEP apart)
#define GT_RING0       70.0f
#define GT_RING_STEP   55.0f
#define GT_ANGS        7        // angles on a ring: straight behind, then +-30, +-60, +-90 degrees off it
#define GT_SPOT_N      (GT_RINGS * GT_ANGS)
#define GT_FLOOR_TOL   40.0f    // the spot's floor within this of the anchor's floor (no ledge, hole or other level)
#define GT_ENEMY_GAP   70.0f    // the spot this far (xz) from every other enemy
#define GT_HELD_CHECK  0.10f    // v3.44 GT344: seconds after the move she must stay near the spot (else an anomaly);
#define GT_HELD_D      120.0f   //   near = GT_HELD_D + GT_HELD_V x the time (she keeps her stride: she may run on)
#define GT_HELD_V      800.0f

static float GtClampF(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
// the hidden keys: missing (has 0) or not a positive number = the default; else clamped to the bounds
static float GtRangeKey(int has, float v) { return !has || !(v > 0.0f) ? GT_RANGE_DEF : GtClampF(v, GT_RANGE_MIN, GT_RANGE_MAX); }
static float GtCoolKey(int has, float v) { return !has || !(v > 0.0f) ? GT_COOL_DEF : GtClampF(v, GT_COOL_MIN, GT_COOL_MAX); }
// v3.44 GT344: still near the spot t seconds after the move?
static int GtHeldOk(float d, float t) { return _finite(d) && d <= GT_HELD_D + GT_HELD_V * (t > 0.0f ? t : 0.0f); }

// ---------------------------------------------------------------- v3.44 GT344: the trigger, Run + Action together
// Run = action 0x10, Action = action 0 (the game's own ids: InputActionHeld 0x4F8440; the bound pad button or key of
// each, Xbox A / B, PlayStation Cross / Circle by GOG's layout). A chord = the tick the second of the two goes down while
// the first went down at most GT_CHORD_S before and is still held; either may be first. Each press is used once (a
// chord uses both). A press held from before (the reset, a menu) is no new press; v3.44 GT344-CHORD-1: on a tick outside
// gameplay (live = 0: a menu, the pause, a cut-scene, a load) every press is marked used, so a press made or held there
// never pairs with a fresh one in play: both must be let go and pressed anew. GTC_ACT_LAST: Action went down last
// (on this tick: the caller takes its press back before the game reads it, so doors, pick-ups and examines do not fire);
// GTC_RUN_LAST: Run went down last (Action's press reached the game on an earlier tick).
#define GT_CHORD_S     0.08f
#define GT_AGE_NONE    1e9f     // not held, or its press already used
enum { GTC_NONE = 0, GTC_ACT_LAST = 1, GTC_RUN_LAST = 2, GTC_HELD = 3 };  // v3.49 GT349: + the hold's next warp
typedef struct { float runAge, actAge; int run, act; } GtChord;
static void GtChordReset(GtChord *c) { c->runAge = c->actAge = GT_AGE_NONE, c->run = c->act = 1; }  // (as if held)
static float GtAge(float age, int held, int fresh, float dt)
{
    if (!held) return GT_AGE_NONE;
    if (fresh) return 0.0f;
    return age >= GT_AGE_NONE ? GT_AGE_NONE : age + dt;
}
static int GtChordStep(GtChord *c, int run, int act, float dt, int live)
{
    const int newRun = run && !c->run, newAct = act && !c->act;
    int r;
    if (!(dt > 0.0f) || dt > 0.25f) dt = 0.0f;  // (a stall or a bad clock: no time passes)
    c->run = run != 0, c->act = act != 0;
    if (!live) {  // GT344-CHORD-1: outside gameplay nothing is chord material (the buttons are still followed above)
        c->runAge = c->actAge = GT_AGE_NONE;
        return GTC_NONE;
    }
    c->runAge = GtAge(c->runAge, run, newRun, dt);
    c->actAge = GtAge(c->actAge, act, newAct, dt);
    if (!newRun && !newAct) return GTC_NONE;
    if (!(c->runAge <= GT_CHORD_S + 1e-4f) || !(c->actAge <= GT_CHORD_S + 1e-4f)) return GTC_NONE;
    r = newAct ? GTC_ACT_LAST : GTC_RUN_LAST;  // (both on one tick: Action is new on this tick, its press is taken back)
    c->runAge = c->actAge = GT_AGE_NONE;      // both presses used
    return r;
}
// The two bindings must be different buttons: a shared bit would make every Action press a chord (or none)
static int GtMasksOk(int run, int act) { return run != 0 && act != 0 && (run & act) == 0; }

// ---------------------------------------------------------------- v3.44 GT344: Richard's see-through fade
// EnemyScratchGhostState06 / 07 (0x4BD980 / 0x4BDB90): the sound, fade_in = 0.5; when it has run out, ScratchWarp and
// fade_out = 0.5. ScratchUpdateAlpha 0x4B9900 (rate 1 outside the stun flicker): alpha = fade_in * 2 while fade_in runs,
// else 1, times (1 - fade_out * 2) while fade_out runs; GhostSetAlpha(1 - alpha) = the transparency here (0 = drawn as
// stock, 1 = gone). func_0045D490 draws a transparency strictly between 0 and 1 with the pre-draw 0x45C7A0 (GtFadeFix).
#define GT_FADE_S      0.5f
enum { GTF_IDLE = 0, GTF_OUT = 1, GTF_IN = 2 };
typedef struct { int phase; float fi, fo; } GtFade;  // fi = Richard's fade_in, fo = his fade_out (seconds left)
static void GtFadeReset(GtFade *f) { f->phase = GTF_IDLE, f->fi = f->fo = 0.0f; }
static void GtFadeStart(GtFade *f) { f->phase = GTF_OUT, f->fi = GT_FADE_S, f->fo = 0.0f; }
static float GtFadeSee(const GtFade *f)
{
    float a = 0.0f;
    if (f->phase == GTF_OUT) a = 1.0f - f->fi * 2.0f;
    else if (f->phase == GTF_IN) a = f->fo * 2.0f;
    return _finite(a) ? GtClampF(a, 0.0f, 1.0f) : 0.0f;
}
// One tick of gameplay time; 1 = the fade-out ended on this tick: the warp now (then the fade-in runs)
static int GtFadeStep(GtFade *f, float dt)
{
    if (!(dt > 0.0f) || dt > 0.25f) dt = 0.0f;
    if (f->phase == GTF_OUT) {
        f->fi -= dt;
        if (f->fi <= 0.0f) {
            f->fi = 0.0f, f->phase = GTF_IN, f->fo = GT_FADE_S;
            return 1;
        }
    } else if (f->phase == GTF_IN) {
        f->fo -= dt;
        if (f->fo <= 0.0f) f->fo = 0.0f, f->phase = GTF_IDLE;
    }
    return 0;
}
// Called off during the fade-out (an event, a hit, a menu): she fades back in from the transparency she has reached
static void GtFadeBack(GtFade *f)
{
    if (f->phase != GTF_OUT) return;
    f->fo = GtClampF(GT_FADE_S - f->fi, 0.0f, GT_FADE_S), f->fi = 0.0f, f->phase = f->fo > 0.0f ? GTF_IN : GTF_IDLE;
}
// 0x45C7A0 (the ghost's see-through pre-draw): fix = (1 - transparency) x 128 - 0.49, stored by fistp (round to
// nearest); then the pixel shader 4 with c0 = fix / 128 and the GS ALPHA FIX = fix (0x429ED0(fix << 32 | 0x64))
static int GtFadeFix(float see)
{
    float x;
    int fix;
    if (!(see > 0.0f)) see = 0.0f;
    if (see > 1.0f) see = 1.0f;
    x = (1.0f - see) * 128.0f - 0.49f;
    fix = (int)floorf(x + 0.5f);
    return fix < 0 ? 0 : fix > 128 ? 128 : fix;
}

// ---------------------------------------------------------------- v3.44 GT344: no enemy near: the way she faces
// [Gameplay] GhostTeleportForward (hidden): the distance (default GT_FWD_DEF, GT_FWD_MIN .. GT_FWD_MAX; 0 = off: with
// no enemy in range the warp is refused as in v3.43). Tries at the full distance, then GT_FWD_STEP shorter each, down
// to GT_FWD_SHORT; the first spot that passes the same spot rules (her line of sight instead of the enemy's) is taken.
#define GT_FWD_DEF     800.0f
#define GT_FWD_MIN     300.0f
#define GT_FWD_MAX     2000.0f
#define GT_FWD_STEP    100.0f
#define GT_FWD_SHORT   200.0f
#define GT_FWD_TRIES_MAX 19     // (2000 - 200) / 100 + 1
static float GtFwdKey(int has, float v)
{
    if (!has || !_finite(v)) return GT_FWD_DEF;
    if (v == 0.0f) return 0.0f;
    return v < 0.0f ? GT_FWD_DEF : GtClampF(v, GT_FWD_MIN, GT_FWD_MAX);
}
// v3.44 GT344-FWD-1: the tries always end at exactly GT_FWD_SHORT, also for a distance off the 100 u grid
// (850: 850, 750 .. 250, then 200 = 8 tries; 800: 800 .. 200 = 7)
static int GtFwdTries(float dist)  // how many distances are tried (0 = none: off)
{
    int n;
    if (!(dist >= GT_FWD_SHORT)) return 0;
    n = (int)ceilf((dist - GT_FWD_SHORT) / GT_FWD_STEP - 1e-3f) + 1;
    return n > GT_FWD_TRIES_MAX ? GT_FWD_TRIES_MAX : n;
}
static float GtFwdDist(float dist, int k)  // try k's distance: GT_FWD_STEP shorter each, never below GT_FWD_SHORT
{
    const float d = dist - GT_FWD_STEP * (float)k;
    return d > GT_FWD_SHORT ? d : GT_FWD_SHORT;
}
// Try k (0 = the full distance): the point GtFwdDist(dist, k) from p along her facing (sin yaw, cos yaw); y = p's
static void GtFwdCand(const float p[3], float yaw, float dist, int k, float out[3])
{
    const float d = GtFwdDist(dist, k);
    out[0] = p[0] + sinf(yaw) * d, out[1] = p[1], out[2] = p[2] + cosf(yaw) * d;
}

// The candidates (an enemy each): its xz distance and height difference to her, ok = standing (hittable, not down,
// not a boss). Writes the indices of the nearest `max` within range (nearest first); returns how many.
typedef struct { float d, dy; int ok; } GtCand;
static int GtPick(const GtCand *c, int n, float range, int *order, int max)
{
    int i, m = 0, q;
    if (max < 1) return 0;
    for (i = 0; i < n; ++i) {
        if (!c[i].ok || !(c[i].d < range) || !(fabsf(c[i].dy) <= GT_DY_MAX)) continue;
        if (m < max) q = m++;
        else if (c[i].d < c[order[max - 1]].d) q = max - 1;
        else continue;
        while (q > 0 && c[order[q - 1]].d > c[i].d) order[q] = order[q - 1], --q;
        order[q] = i;
    }
    return m;
}

// The direction (xz, unit) from the enemy toward its back: minus its facing (the model's rotation quaternion q =
// x, y, z, w turns +Z to its forward; the game's yaw convention: forward = (sin yaw, cos yaw)). A rotation that does
// not give a level forward (not finite, tipped over) falls back to the far side from her: e - pp. 1 = from its facing.
static int GtBackDir(const float q[4], const float e[3], const float pp[3], float out[2])
{
    const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
    float l = sqrtf(fx * fx + fz * fz);
    if (_finite(fx) && _finite(fz) && l > 0.3f && l < 1.5f) {
        out[0] = -fx / l, out[1] = -fz / l;
        return 1;
    }
    out[0] = e[0] - pp[0], out[1] = e[2] - pp[2];
    l = sqrtf(out[0] * out[0] + out[1] * out[1]);
    if (l > 1.0f && _finite(l)) out[0] /= l, out[1] /= l;
    else out[0] = 0.0f, out[1] = 1.0f;
    return 0;
}
// Spot candidate k (0 .. GT_SPOT_N - 1) behind the enemy at e (xz; y = its floor): ring k / GT_ANGS at its radius +
// GT_RING0 + ring x GT_RING_STEP; angle 0, +30, -30, +60, -60, +90, -90 degrees off its back. Straight behind first.
static void GtSpotCand(const float e[3], const float back[2], float rad, int k, float out[3])
{
    static const float kAng[GT_ANGS] = { 0.0f, 0.5235988f, -0.5235988f, 1.0471976f, -1.0471976f, 1.5707963f, -1.5707963f };
    const int ring = k / GT_ANGS, a = k % GT_ANGS;
    const float r = rad + GT_RING0 + GT_RING_STEP * (float)ring, c = cosf(kAng[a]), s = sinf(kAng[a]);
    const float dx = back[0] * c - back[1] * s, dz = back[0] * s + back[1] * c;
    out[0] = e[0] + dx * r, out[1] = e[1], out[2] = e[2] + dz * r;
}
// Her yaw on the spot, facing the enemy (its back)
static float GtFaceYaw(const float spot[3], const float e[3]) { return atan2f(e[0] - spot[0], e[2] - spot[2]); }
// Is the spot clear of the other enemies (xz)? others: their positions (xz in [0], [2]), n of them
static int GtClearOf(const float spot[3], const float (*others)[3], int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        const float dx = spot[0] - others[i][0], dz = spot[2] - others[i][2];
        if (dx * dx + dz * dz < GT_ENEMY_GAP * GT_ENEMY_GAP) return 0;
    }
    return 1;
}

// GT343-001 (inspection 1): the same room on every stage. The game has no "room of a point" lookup (gGame.room is the
// loaded room; the sub-areas 0x46F5A0 exist only in stages 2 and 5); a room's edges to its neighbours are its exits
// (ExitGet(gGame.room, i) 0x4FA070: the door line (x0, z0)-(x1, z1) at height y and pos, "a point on the room's side of
// the line"; 0x4F9E90 ExitIsCrossed takes a player within 100 u of the line on the other side as crossing it). A spot
// is in this room when it is inside the room's collision box (0x561A70) and, for every door line of the room (at its
// height), not within GT_EXIT_NEAR of it, not beyond it (on the side away from pos, within its span and GT_EXIT_DEPTH),
// and not reached from the anchor through it (the segment anchor -> spot crossing the line, extended GT_EXIT_NEAR).
typedef struct { float x0, z0, x1, z1, y, px, pz; } GtExit;
#define GT_EXIT_NEAR   120.0f   // (the game's own crossing radius is 100)
#define GT_EXIT_DEPTH  400.0f
#define GT_EXIT_DY     600.0f   // a door line this far above / below the spot belongs to another level
enum { GTX_OK, GTX_NEAR, GTX_BEYOND, GTX_CROSS };
static float GtSide(const GtExit *x, float px, float pz) { return (x->x1 - x->x0) * (pz - x->z0) - (x->z1 - x->z0) * (px - x->x0); }
// distance (xz) from p to the door line; *u = where along it (0 .. 1 on the line)
static float GtExitDist(const GtExit *x, float px, float pz, float *u)
{
    const float dx = x->x1 - x->x0, dz = x->z1 - x->z0, l2 = dx * dx + dz * dz;
    float t = l2 > 1.0f ? ((px - x->x0) * dx + (pz - x->z0) * dz) / l2 : 0.0f, cx, cz;
    *u = t;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    cx = x->x0 + dx * t - px, cz = x->z0 + dz * t - pz;
    return sqrtf(cx * cx + cz * cz);
}
// does the segment a -> b (xz) cross the door line extended by `ext` at both ends?
static int GtCrossesExit(const GtExit *x, const float a[3], const float b[3], float ext)
{
    const float dx = x->x1 - x->x0, dz = x->z1 - x->z0, l = sqrtf(dx * dx + dz * dz);
    GtExit e = *x;
    float s1, s2, t1, t2;
    if (l < 1.0f) return 0;
    e.x0 -= dx / l * ext, e.z0 -= dz / l * ext, e.x1 += dx / l * ext, e.z1 += dz / l * ext;
    s1 = GtSide(&e, a[0], a[2]), s2 = GtSide(&e, b[0], b[2]);
    if (!(s1 * s2 < 0.0f)) return 0;
    {   // the door line's ends on either side of a -> b
        const float ux = b[0] - a[0], uz = b[2] - a[2];
        t1 = ux * (e.z0 - a[2]) - uz * (e.x0 - a[0]);
        t2 = ux * (e.z1 - a[2]) - uz * (e.x1 - a[0]);
    }
    return t1 * t2 < 0.0f;
}
// GTX_OK or why the spot s is not in this room by its door lines; from = the anchor (NULL: no crossing test)
static int GtExitTest(const GtExit *x, int n, const float *from, const float s[3])
{
    int i;
    for (i = 0; i < n; ++i) {
        const GtExit *e = &x[i];
        const float dx = e->x1 - e->x0, dz = e->z1 - e->z0, l = sqrtf(dx * dx + dz * dz);
        float u, d, sIn, sS;
        if (!(fabsf(e->y - s[1]) <= GT_EXIT_DY)) continue;
        d = GtExitDist(e, s[0], s[2], &u);
        if (d < GT_EXIT_NEAR) return GTX_NEAR;
        if (l >= 1.0f) {
            const float m = GT_EXIT_NEAR / l;
            sIn = GtSide(e, e->px, e->pz), sS = GtSide(e, s[0], s[2]);
            if (sIn * sS < 0.0f && u >= -m && u <= 1.0f + m && d < GT_EXIT_DEPTH) return GTX_BEYOND;
            if (from && GtCrossesExit(e, from, s, GT_EXIT_NEAR)) return GTX_CROSS;
        }
    }
    return GTX_OK;
}
// GT343-001c (inspection 5): the room's edges must be complete and valid, else no warp (fail closed): a room number
// the exit tables can hold, an exit count 0 .. GT_EXIT_MAX with every entry present and finite, the collision box
// present and sane, and her own feet inside it. A room without exits passes only with that box.
#define GT_EXIT_MAX 48
enum { GTB_OK, GTB_ROOM, GTB_TRUNC, GTB_ENTRY, GTB_NOBOX, GTB_OUTSIDE, GTB_N };
static const char *const kGtBoundsWhy[GTB_N] = { "", "the room number is out of the exit tables' range",
    "more exits than can be checked", "an exit entry is missing or not a number", "the room's collision box is not there",
    "she stands outside the room's collision box" };
// roomOk: 1 .. 255; nExits: ExitGetNum's count; nValid: the entries read and valid; boxOk: the box present and sane;
// herIn: her feet inside it
static int GtBoundsVerdict(int roomOk, int nExits, int nValid, int boxOk, int herIn)
{
    if (!roomOk) return GTB_ROOM;
    if (nExits < 0 || nExits > GT_EXIT_MAX) return GTB_TRUNC;
    if (nValid != nExits) return GTB_ENTRY;
    if (!boxOk) return GTB_NOBOX;
    if (!herIn) return GTB_OUTSIDE;
    return GTB_OK;
}
// inside the room's collision box (xz; lo / hi its corners)?
static int GtInBox(const float lo[3], const float hi[3], const float s[3])
{
    return s[0] >= lo[0] && s[0] <= hi[0] && s[2] >= lo[2] && s[2] <= hi[2];
}

// ---------------------------------------------------------------- v3.49 GT349: hold Run + Action = keep warping
// A chord (a fresh press of both) starts a hold; it ends the tick either button is let go, or outside gameplay. While
// it holds, the next warp starts once the fade is over (she has fully faded back in) and the cooldown (0 by default)
// has run out; after a refusal it waits GT_FAIL_COOL before the next try. One press = one warp: a press let go
// before the fade has ended starts nothing more.
enum { GTH_NONE = 0, GTH_GO = 1, GTH_END = 2 };
typedef struct { int on, warps, refused; float wait; } GtHold;
static void GtHoldReset(GtHold *h) { h->on = 0, h->warps = h->refused = 0, h->wait = 0.0f; }
static void GtHoldStart(GtHold *h) { h->on = 1, h->warps = h->refused = 0, h->wait = 0.0f; }
static void GtHoldRefused(GtHold *h)  // a try of the hold was refused: count it, wait before the next
{
    if (!h->on) return;
    ++h->refused;
    h->wait = GT_FAIL_COOL;
}
// One tick. run / act: held now; live: a gameplay tick with the option on; idle: no fade runs; cool: cooldown left.
static int GtHoldStep(GtHold *h, int run, int act, int live, int idle, float cool, float dt)
{
    if (!h->on) return GTH_NONE;
    if (!run || !act || !live) {
        h->on = 0;
        return GTH_END;
    }
    if (!(dt > 0.0f) || dt > 0.25f) dt = 0.0f;
    if (h->wait > 0.0f) {
        h->wait -= dt;
        return GTH_NONE;
    }
    return idle && !(cool > 0.0f) ? GTH_GO : GTH_NONE;
}

// ---------------------------------------------------------------- v3.49 GT349: the room's walk-in event zones
// An event object's zone (ActorZone, task + 0x424; the game's tests 0x52B8D0 .. 0x52C410): shape 1 = an upright
// cylinder of radius sx and height sy hanging up from y (y grows downwards: the zone spans y - sy .. y), 2 = a box sx x
// sz (full sizes, centred on x / z, turned by heading about y) of height sy the same way, 3 = a sphere of radius sx. The
// player counts from her feet y to y - GT_PLAYER_H (PLAYER_HEIGHT 400). A warp from a to b (her feet) is refused when
// the straight path touches the zone (grown by pad) from outside, or ends in it: the game tests the zone every tick,
// so walking from a to b would fire it on the way. A turned box (heading not a multiple of 90 degrees) is taken as its
// enclosing circle (more refused, never less). A zone with a value that is not a number: refused (fail closed).
// v3.49 GT349-001 (Codex inspection 1): standing INSIDE a zone of the list is refused too (GtZoneInside): the list holds
// only zones whose event is not running (GtZoneArmed), so she may have run into it during the fade-out and its event
// has not started yet; a warp out would skip it (the v3.48 crash's class). A zone whose event runs is not in the list:
// standing in it, she may warp.
#define GT_PLAYER_H    400.0f
#define GT_ZONE_PAD    20.0f
typedef struct { float x, y, z; int shape; float heading, sx, sy, sz; } GtZone;
// a segment (xz) against the rectangle |x| <= hx, |z| <= hz (Liang-Barsky); 1 = it touches it
static int GtSegRect(float ax, float az, float bx, float bz, float hx, float hz)
{
    const float dx = bx - ax, dz = bz - az;
    const float p[4] = { -dx, dx, -dz, dz }, q[4] = { ax + hx, hx - ax, az + hz, hz - az };
    float t0 = 0.0f, t1 = 1.0f;
    int i;
    for (i = 0; i < 4; ++i) {
        if (p[i] == 0.0f) {
            if (q[i] < 0.0f) return 0;
        } else {
            const float t = q[i] / p[i];
            if (p[i] < 0.0f) {
                if (t > t1) return 0;
                if (t > t0) t0 = t;
            } else {
                if (t < t0) return 0;
                if (t < t1) t1 = t;
            }
        }
    }
    return t0 <= t1;
}
// the nearest distance (xz) from (cx, cz) to the segment a -> b
static float GtSegDist(float ax, float az, float bx, float bz, float cx, float cz)
{
    const float dx = bx - ax, dz = bz - az, l2 = dx * dx + dz * dz;
    float t = l2 > 1e-6f ? ((cx - ax) * dx + (cz - az) * dz) / l2 : 0.0f, ex, ez;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    ex = ax + dx * t - cx, ez = az + dz * t - cz;
    return sqrtf(ex * ex + ez * ez);
}
static int GtZoneOk(const GtZone *z)  // every value a number (sizes may be 0)
{
    return _finite(z->x) && _finite(z->y) && _finite(z->z) && _finite(z->heading) && _finite(z->sx) && _finite(z->sy) &&
           _finite(z->sz);
}
// the zone's footprint in the xz plane, in its own frame: 1 = a rectangle (*hx, *hz), 0 = a circle (*hx = radius)
static int GtZoneShapeXZ(const GtZone *z, float pad, float *hx, float *hz)
{
    *hz = 0.0f;
    if (z->shape == 2) {
        const float s = sinf(z->heading), c = cosf(z->heading);
        const float bx = 0.5f * fabsf(z->sx), bz = 0.5f * fabsf(z->sz);
        if (fabsf(s) < 1e-3f) {
            *hx = bx + pad, *hz = bz + pad;
            return 1;
        }
        if (fabsf(c) < 1e-3f) {
            *hx = bz + pad, *hz = bx + pad;
            return 1;
        }
        *hx = sqrtf(bx * bx + bz * bz) + pad;
        return 0;
    }
    *hx = fabsf(z->sx) + pad;
    return 0;
}
static int GtZoneInXZ(const GtZone *z, float pad, float px, float pz)
{
    float hx, hz;
    const float dx = px - z->x, dz = pz - z->z;
    if (GtZoneShapeXZ(z, pad, &hx, &hz)) return fabsf(dx) <= hx && fabsf(dz) <= hz;
    return dx * dx + dz * dz <= hx * hx;
}
static int GtZoneSegXZ(const GtZone *z, float pad, const float a[3], const float b[3])
{
    float hx, hz;
    if (GtZoneShapeXZ(z, pad, &hx, &hz))
        return GtSegRect(a[0] - z->x, a[2] - z->z, b[0] - z->x, b[2] - z->z, hx, hz);
    return GtSegDist(a[0], a[2], b[0], b[2], z->x, z->z) <= hx;
}
// her height span over the path (min(a, b) - GT_PLAYER_H .. max(a, b)) meets the zone's?
static int GtZoneMeetsY(const GtZone *z, float ay, float by)
{
    const float lo = (ay < by ? ay : by) - GT_PLAYER_H, hi = ay < by ? by : ay;
    float zlo, zhi;
    if (z->shape == 3) zlo = z->y - fabsf(z->sx), zhi = z->y + fabsf(z->sx);
    else zlo = z->y - fabsf(z->sy), zhi = z->y;
    return hi >= zlo && lo <= zhi;
}
// v3.49 GT349-001: is she (her feet a) inside the zone now? (a value that is not a number: yes, fail closed)
static int GtZoneInside(const GtZone *z, const float a[3])
{
    if (z->shape < 1 || z->shape > 3) return 0;
    if (!GtZoneOk(z) || !_finite(a[0]) || !_finite(a[1]) || !_finite(a[2])) return 1;
    return GtZoneInXZ(z, 0.0f, a[0], a[2]) && GtZoneMeetsY(z, a[1], a[1]);
}
// 1 = a warp from a to b (her feet) skips or lands in the zone, or starts inside it (GT349-001: its event not running yet)
static int GtZoneHit(const GtZone *z, const float a[3], const float b[3])
{
    if (z->shape < 1 || z->shape > 3) return 0;
    if (!GtZoneOk(z) || !_finite(a[0]) || !_finite(a[1]) || !_finite(a[2]) || !_finite(b[0]) || !_finite(b[1]) || !_finite(b[2]))
        return 1;
    if (GtZoneInside(z, a)) return 1;  // GT349-001: inside a zone that has not fired: it fires first, no warp out
    if (!GtZoneMeetsY(z, a[1], b[1])) return 0;
    return GtZoneSegXZ(z, GtZoneInXZ(z, GT_ZONE_PAD, a[0], a[2]) ? 0.0f : GT_ZONE_PAD, a, b);
}
// v3.49 GT349-001: the first of n zones she stands inside (its index), else -1
static int GtZonesInside(const GtZone *z, int n, const float a[3])
{
    int i;
    for (i = 0; i < n; ++i)
        if (GtZoneInside(&z[i], a)) return i;
    return -1;
}
// v3.49 GT349-001: which event objects' zones go into the list (teleport.inc GtZonesRead; EventObjectsTick 0x4D00F0's own
// test): no use-item, its record not running, enabled by the game's last test, actor type 7 or 9, zone shape 1 .. 3
// without flag 4 (a press) or 0x10 (inverted). 1 = armed (in the list), 0 = not, -1 = a zone of another shape / flags
// (counted as "needs a press or a look").
static int GtZoneArmed(int useItem, int running, int enabled, int type, int shape, int flags)
{
    if (useItem || running || !enabled || (type != 7 && type != 9)) return 0;
    if (shape < 1 || shape > 3 || (flags & 0x14)) return -1;
    return 1;
}
// the first of n zones a warp a -> b would skip or land in (its index), else -1
static int GtZonesHit(const GtZone *z, int n, const float a[3], const float b[3])
{
    int i;
    for (i = 0; i < n; ++i)
        if (GtZoneHit(&z[i], a, b)) return i;
    return -1;
}
#endif
