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
//
// v3.50 TP350 (the user's v3.49 QA, staging/v350_feedback/USER_FEEDBACK_v349.md A1-A3; staging/v350_feedback/
// w1_teleport/PLAN.md): behind the enemy she FACES only (GtPickFaced: the standing enemy nearest her facing inside a
// cone), from twice as far (GT_RANGE_DEF 5000); with none in the cone the warp ahead may pass a wall, a turnstile, a
// fence, a counter or a gap (GtThruSearch) onto a spot where she can stand whose floor a route of the room reaches
// (GtMapHas: the room's floor map, a flood by her own walking rules from the room's doors and her own standing places).
//
// v3.51 TP351 (the user's v3.50 QA item 2; staging/v351_feedback/w1_teleport/PLAN.md): an enemy she faces only through
// collision (a wall, a prop, a turnstile between, inside a wall, a ghost in its wall) is not "faced": the next one in
// the cone she has a clear line to, else the warp ahead (GtFoeSight, GtPickFacedVia).
//
// v3.52 TP352 (the user's v3.51 QA D4, staging/v352_feedback/USER_FEEDBACK_v351.md; staging/v352_feedback/w5_teleport/
// PLAN.md): stuck in a Water Prison wall after a warp behind an enemy. Every landing (behind, ahead, through) passes one
// proof by her own walk rules and collision (GtLandProof: her body free, on the room's floor map); the floor map takes
// no seed inside collision (GtCellFree); after a warp, or at a warp that starts inside a wall, she is moved to the
// nearest proven floor cell when her body overlaps the room's collision (GtStuck, GtFreeWatch, GtFreeCell).
#ifndef TELEPORT_SIM_H
#define TELEPORT_SIM_H
#include <float.h>
#include <math.h>
#include <string.h>  // v3.50 TP350: memset (GtMap)

#define GT_RANGE_DEF   5000.0f  // [Gameplay] GhostTeleportRange (hidden): enemies within this (xz) of her  // v3.50 TP350: 5000 (was 2500; A3)
#define GT_RANGE_MIN   500.0f
#define GT_RANGE_MAX   12000.0f  // v3.50 TP350: 12000 (was 6000); the enemy must still be in this room (box, door lines)
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

// ---------------------------------------------------------------- v3.58 GT358: a clean fade (the user's v3.57 QA)
// "Ghost Teleport shows all these weird black line artifacts during fade." Two causes in v3.44 .. v3.57's draw (the
// ghost's own states on her model, staging/v358_feedback/w6_teleport/PLAN.md):
//  1. pixel shader 4 (0x5D5538: r0 = t0 x v0; r0.a = c0.a) takes the fade as the alpha of EVERY texel, and blend mode 5
//     (TEST 0x53001 -> RenderModelSetBlend(5)) has the alpha test off: the cut-out texels of Eileen's lashes and hair
//     cards (DXT3 alpha 0, dark RGB) are drawn at the fade's alpha: thin dark strips and fringes;
//  2. blend mode 5 writes no depth: every surface of her model blends, the overlapping strips at the seams of her body
//     parts and the hair over the scalp twice: lines and layers.
// The clean draw (GT_FD_CLEAN, the default): her stock draw in the actor loop builds the pose but draws nothing; right
// after the loop (0x4EF905, every actor model drawn: no enemy behind her is cut out of the picture) her body and weapon
// are drawn again: a depth pre-pass (the texel's alpha > GT_PRE_REF writes her nearest surface's depth) and one colour
// pass on it (GREATEREQUAL, no depth write) with the game's pixel shader 5 (0x5D5590: r0.a = t0.a x c0.a, the alpha
// test on): her nearest surface blends once, the cut-out texels not at all. GT_FD_ONE: the colour pass only, in place
// (shader 5; the layers stay); GT_FD_STOCK: v3.57 exactly (shader 4). Hidden [Gameplay] GhostTeleportFadeDraw = 0 / 1 / 2.
// The same opacity (GtFadeOp: the fix the draw uses / 128) drives her heat body under Thermal (a blended heat body on its
// own pre-pass, at the heat's alpha x her opacity), her soul fire under Spirit Vision (its sprites x her opacity) and her
// real-time shadow (drawn while her opacity is at least GT_SHADOW_OP: Fade In's rule; one stencil mask cannot fade one
// caster).
#define GT_FD_STOCK    0        // v3.57: shader 4, one pass in place (the cut-out texels drawn)
#define GT_FD_ONE      1        // shader 5, one pass in place (no cut-out texels; the layers stay)
#define GT_FD_CLEAN    2        // shader 5 after the actor loop on a depth pre-pass (default)
#define GT_FD_DEF      GT_FD_CLEAN
#define GT_SHADOW_OP   0.5f     // her shadow is drawn while her opacity is at least this
#define GT_PRE_REF     127      // the pre-pass's alpha test: texels above this write her depth (a hair card's soft edge
                                //   below it blends over what lies behind it, as the stock opaque draw)
enum { GTD_NONE = 0, GTD_INPLACE = 1, GTD_LATE = 2 };
typedef struct { int path, ps, prepass, fix, drawn; } GtFadeDraw;
static int GtFadeDrawKey(int has, int v) { return has && v >= GT_FD_STOCK && v <= GT_FD_CLEAN ? v : GT_FD_DEF; }
static const char *GtFadeDrawName(int mode)
{
    return mode == GT_FD_STOCK ? "Stock (v3.57: one pass, pixel shader 4)" : mode == GT_FD_ONE ? "One pass (pixel shader 5)" :
           "Clean (after the actor models: depth pre-pass + one blend, pixel shader 5)";
}
// her opacity in the fade's draw: the draw's own quantised alpha (fix / 128): 1 = stock, 0 = gone
static float GtFadeOp(float see) { return (float)GtFadeFix(see) * 0.0078125f; }
// The plan of one draw of her body or weapon: see = the fade's transparency, mode the key, lateOk = the 0x4EF905 hook and
// the draw-call override are in (and the late draw has not failed), phase0 = gameplay phase 0 now (the step list with the
// hook), ovrFree = no other redraw holds the draw-call override. path GTD_NONE = drawn as stock.
static void GtFadePlan(GtFadeDraw *o, int mode, float see, int lateOk, int phase0, int ovrFree)
{
    o->path = GTD_NONE, o->ps = 8, o->prepass = 0, o->fix = 128, o->drawn = 1;
    if (!(see > 0.0f)) return;
    o->fix = GtFadeFix(see);
    o->drawn = o->fix > 0;  // (0: nothing of her is drawn this frame, as the stock fade at its end)
    if (mode == GT_FD_CLEAN && lateOk && phase0 && ovrFree) o->path = GTD_LATE, o->ps = 5, o->prepass = 1;
    else o->path = GTD_INPLACE, o->ps = mode == GT_FD_STOCK ? 4 : 5;
}
// her real-time shadow this frame (v3.57 always)
static int GtShadowKeep(int mode, float see) { return mode == GT_FD_STOCK || GtFadeOp(see) >= GT_SHADOW_OP; }
// Thermal's heat body: its alpha (the heat's own alpha base x her opacity) and whether it stays the solid one-pass draw
static float GtHeatAlpha(int mode, float base, float see) { return mode == GT_FD_STOCK ? base : base * GtFadeOp(see); }
static int GtHeatSolid(int mode, int solid, float see) { return solid && (mode == GT_FD_STOCK || GtFadeOp(see) >= 1.0f); }
// Spirit Vision's soul fire: its sprites' alpha factor
static float GtSoulK(int mode, float see) { return mode == GT_FD_STOCK ? 1.0f : GtFadeOp(see); }

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
typedef struct { float d, dy; int ok; float ang; int skip; } GtCand;  // v3.50 TP350: + ang (GtOffFacing), skip (not in this room)
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

// ================================================================ v3.50 TP350 A2 / A3: behind the enemy she faces
// The standing enemy nearest her facing direction inside +-cone (degrees: [Gameplay] GhostTeleportCone, hidden, default
// GT_CONE_DEF) within GhostTeleportRange (A3: default 5000, was 2500), at her height (GT_DY_MAX), in this room (the
// caller marks one beyond a door line or outside the room's box `skip`). Only that one is tried: never another enemy.
// Ties: of the enemies within GT_CONE_TIE degrees of the smallest angle, the nearer; then the first in the list.
#define GT_CONE_DEF    35.0f
#define GT_CONE_MIN    10.0f
#define GT_CONE_MAX    90.0f
#define GT_CONE_TIE    5.0f
#define GT_DEG         0.017453293f
static float GtConeKey(int has, float v) { return !has || !(v > 0.0f) ? GT_CONE_DEF : GtClampF(v, GT_CONE_MIN, GT_CONE_MAX); }
// the angle (radians, 0 .. pi) between her facing (sin yaw, cos yaw) and the way (xz) from her feet p to e (an enemy on
// her own spot, nearer than 1 u: 0, straight ahead); not a number: pi (never in a cone)
static float GtOffFacing(const float p[3], float yaw, const float e[3])
{
    const float dx = e[0] - p[0], dz = e[2] - p[2], l = sqrtf(dx * dx + dz * dz);
    float c;
    if (!_finite(l) || !_finite(yaw)) return 3.14159265f;
    if (!(l > 1.0f)) return 0.0f;
    c = (dx * sinf(yaw) + dz * cosf(yaw)) / l;
    return acosf(GtClampF(c, -1.0f, 1.0f));
}
static int GtFacedOk(const GtCand *c, float range, float cone)
{
    return c->ok && !c->skip && c->d < range && fabsf(c->dy) <= GT_DY_MAX && _finite(c->ang) && c->ang <= cone;
}
// the index of the faced enemy (cone in degrees), or -1: none in the cone (the warp ahead)
static int GtPickFaced(const GtCand *c, int n, float range, float cone)
{
    const float coneR = GtClampF(cone, GT_CONE_MIN, GT_CONE_MAX) * GT_DEG;
    float amin = 1e9f;
    int i, best = -1;
    for (i = 0; i < n; ++i)
        if (GtFacedOk(&c[i], range, coneR) && c[i].ang < amin) amin = c[i].ang;
    if (!(amin < 1e8f)) return -1;
    for (i = 0; i < n; ++i) {
        if (!GtFacedOk(&c[i], range, coneR) || c[i].ang > amin + GT_CONE_TIE * GT_DEG) continue;
        if (best < 0 || c[i].d < c[best].d) best = i;  // (equal distance: the first stays)
    }
    return best;
}
// how many standing enemies in range and height are inside the cone (the log)
static int GtConeCount(const GtCand *c, int n, float range, float cone)
{
    const float coneR = GtClampF(cone, GT_CONE_MIN, GT_CONE_MAX) * GT_DEG;
    int i, m = 0;
    for (i = 0; i < n; ++i) m += GtFacedOk(&c[i], range, coneR);
    return m;
}

// ================================================================ v3.50 TP350 A1: the warp ahead through walls
// Along her facing and a fan of +-10 / +-20 degrees (straight ahead first), samples every GT_THRU_STEP up to the warp
// distance (GhostTeleportForward, 800 by default). The caller's `open` says if she could walk straight to a sample
// (the floor at her level, nothing between her chest and its chest); the first sample that is not open is where the
// obstacle begins (a wall, a turnstile, a fence, a counter, a gap in the floor). From there on every sample is a
// candidate for the caller's `land` test (0 = a spot: the game's floor and room to stand, her body's spheres, the room's
// edges, the walk-in zones on the straight path, a route of the room to it), nearest first: the first that passes is
// the target. A ray with no obstacle within the warp distance gives none (the v3.44 warp ahead applies: farthest first).
#define GT_THRU_STEP   40.0f
#define GT_THRU_SAMPLES 50      // (GT_FWD_MAX / GT_THRU_STEP)
#define GT_FAN_N       5
#define GT_FAN_STEP    0.17453293f  // 10 degrees
typedef struct {
    void *ctx;
    int (*open)(void *ctx, const float c[3]);                // 1 = she could walk straight to c
    int (*land)(void *ctx, const float c[3], float out[3]);  // 0 = a spot (out: its floor point), else why not (1 .. nCnt - 1)
    int pending;  // land's code for "not known yet" (the room's floor map is not finished; 0 = none): the search stops there
} GtThruFns;
typedef struct { int ray, k, kObst, refused; float at[3], yaw, d, dObst; } GtThru;
static float GtFanYaw(float yaw, int r)  // ray r (0 .. GT_FAN_N - 1): 0, +10, -10, +20, -20 degrees off her facing
{
    static const signed char kF[GT_FAN_N] = { 0, 1, -1, 2, -2 };
    return yaw + GT_FAN_STEP * (float)kF[r < 0 ? 0 : r >= GT_FAN_N ? GT_FAN_N - 1 : r];
}
static int GtThruSamples(float dist)  // samples on a ray (0 = off)
{
    int n;
    if (!(dist >= GT_THRU_STEP) || !_finite(dist)) return 0;
    n = (int)floorf(dist / GT_THRU_STEP + 1e-3f);
    return n > GT_THRU_SAMPLES ? GT_THRU_SAMPLES : n;
}
static void GtThruCand(const float p[3], float yaw, int k, float out[3])  // sample k (1 ..) along yaw; y = p's
{
    const float d = GT_THRU_STEP * (float)k;
    out[0] = p[0] + sinf(yaw) * d, out[1] = p[1], out[2] = p[2] + cosf(yaw) * d;
}
// 1 = a spot past an obstacle (*t; she keeps her facing yaw); 0 = none; 2 = the nearest candidate is not known yet (land
// returned f->pending: *t holds it; nothing farther is taken before it). cnt (nCnt entries, may be NULL): the refused
// samples past an obstacle by the land test's reason; *nObst: the rays that met an obstacle
static int GtThruSearch(const GtThruFns *f, const float from[3], float yaw, float dist, GtThru *t, int *cnt, int nCnt,
                        int *nObst)
{
    // v3.50 TP350-001 (Codex inspection 1): every ray is searched and the NEAREST answer of all rays wins (by the sample's
    // distance along its ray; equal distances: the earlier ray, straight ahead first). Each ray's answer is its nearest
    // sample past its obstacle that passes or is pending; a pending one nearer than every passing one makes the result 2
    // (nothing farther is taken before it is known), as on a single ray before.
    const int n = GtThruSamples(dist);
    int r, k, best = 0;
    float bestK = 0.0f;
    *nObst = 0;
    if (!_finite(yaw) || !_finite(from[0]) || !_finite(from[1]) || !_finite(from[2])) return 0;
    for (r = 0; r < GT_FAN_N; ++r) {
        const float y = GtFanYaw(yaw, r);
        int kObst = 0, refused = 0;
        for (k = 1; k <= n; ++k) {
            float c[3], out[3];
            int why;
            if (best && (float)k >= bestK) break;  // (this ray cannot give a nearer answer)
            GtThruCand(from, y, k, c);
            if (!kObst) {
                if (f->open(f->ctx, c)) continue;
                kObst = k, ++*nObst;
                continue;  // (the obstacle's first sample is no spot)
            }
            if ((why = f->land(f->ctx, c, out)) == 0 || (f->pending && why == f->pending)) {
                const float dx = out[0] - from[0], dz = out[2] - from[2];
                t->ray = r, t->k = k, t->kObst = kObst, t->refused = refused;
                t->at[0] = out[0], t->at[1] = out[1], t->at[2] = out[2], t->yaw = yaw;
                t->d = sqrtf(dx * dx + dz * dz), t->dObst = GT_THRU_STEP * (float)kObst;
                best = why == 0 ? 1 : 2, bestK = (float)k;
                break;
            }
            ++refused;
            if (cnt && why > 0 && why < nCnt) ++cnt[why];
        }
    }
    return best;
}

// ---------------------------------------------------------------- v3.50 TP350: a route of the room reaches the spot
// The proof that a spot past a wall is playable floor, not a closed space: the room's floor map (GtMap), the floor a
// route of the room reaches: a flood over a grid on the room's box from every door of the room (ExitGet's point on the
// room's side of each door line) and from every place she stood at a warp in this room visit, by her own walking rules.
// A cell is floor she can walk onto from its neighbour (8 ways): the first surface from GT_REACH_UP above the
// neighbour's floor down to GT_REACH_UP below it, at most GT_REACH_STEP higher or lower (stairs: 40 per tread), not
// inside solid GT_REACH_LOW above it, no polygon in her upper body's sphere (GT_REACH_BODY above it, radius
// GT_REACH_R), nothing on the lines between the two cells GT_REACH_LOW, GT_REACH_LOW2 and GT_REACH_BODY above their
// floors (a wall, a prop; lower barriers she steps over), inside the room's box, not beyond one of its door lines. A
// spot is reached when its own cell or a neighbour cell holds a floor at its level (GT_REACH_HIT_DY) that she can step
// to from it. Built breadth first, a bounded number of cells at a time (GtMapRun: at the press, then every tick of the
// fade-out); a spot the unfinished map does not hold yet is "not known yet" (the press may start the fade, the warp
// decides). The grid: GT_MAP_DIM cells each way, GT_REACH_CELL wide or wider for a big room. A query that cannot answer
// counts as blocked (fail closed); a cell holds one floor (on two floors at one place the first reached is kept: a spot
// on the other is not proven).
#define GT_REACH_CELL  60.0f
#define GT_REACH_UP    100.0f
#define GT_REACH_STEP  50.0f
#define GT_REACH_LOW   60.0f
#define GT_REACH_LOW2  110.0f
#define GT_REACH_BODY  170.0f   // PlayerWallFloorCollide's upper sphere (r 70 there)
#define GT_REACH_R     45.0f
#define GT_REACH_HIT_DY 60.0f
#define GT_MAP_DIM     256      // cells each way at most
#define GT_MAP_RING    16384    // the flood's queue (its front; more = the map is not finished: overflow)
#define GT_MAP_SEED_DY 200.0f   // a door's point: its floor within this of the door line's height
typedef struct GtWorld {
    void *ctx;
    int (*floorAt)(void *ctx, float x, float yTop, float yBot, float z, float *fy);  // 1 = the first surface (y grows down)
    int (*sphere)(void *ctx, float x, float y, float z, float r);  // polygons the sphere touches (0 = free; < 0 unknown)
    int (*line)(void *ctx, const float a[3], const float b[3]);    // 1 = blocked, 0 = clear, < 0 unknown
    int (*solid)(void *ctx, float x, float y, float z);            // 1 = inside solid, 0 = free, < 0 unknown
} GtWorld;
enum { GTM_NONE = 0, GTM_HAS = 1, GTM_NOT = 2, GTM_UNKNOWN = 3 };  // GtMapHas
typedef struct {
    int on, nx, ny, overflow, unknown;
    float cell, x0, z0, lo[3], hi[3];                 // the grid: cell (i, j) at x0 + i cell, z0 + j cell
    unsigned char st[GT_MAP_DIM * GT_MAP_DIM];        // 1 = reached (queued or done)
    short fy[GT_MAP_DIM * GT_MAP_DIM];                // its floor (rounded)
    int ring[GT_MAP_RING], head, tail;                // cells to expand (FIFO)
    int cells, done, seeds, seedsNoFloor, runs;       // the log: cells reached, expanded; seeds taken, without a floor
    int seedsBlocked;                                 // v3.52 TP352: seeds refused inside collision (GtCellFree)
    GtExit x[GT_EXIT_MAX];
    int nExit;
} GtMap;
// a new map over the room's box (lo / hi) with its door lines (cells beyond them are not the room's)
static void GtMapBegin(GtMap *m, const float lo[3], const float hi[3], const GtExit *x, int nx)
{
    const float ex = hi[0] - lo[0], ez = hi[2] - lo[2], big = ex > ez ? ex : ez;
    int k;
    m->cell = GT_REACH_CELL;
    if (_finite(big) && big / m->cell > (float)(GT_MAP_DIM - 1)) m->cell = big / (float)(GT_MAP_DIM - 1) + 0.01f;
    m->x0 = lo[0], m->z0 = lo[2];
    m->nx = (int)(ex / m->cell) + 1, m->ny = (int)(ez / m->cell) + 1;
    m->on = _finite(ex) && _finite(ez) && ex > 0.0f && ez > 0.0f && m->nx <= GT_MAP_DIM && m->ny <= GT_MAP_DIM;
    for (k = 0; k < 3; ++k) m->lo[k] = lo[k], m->hi[k] = hi[k];
    memset(m->st, 0, sizeof m->st);
    m->head = m->tail = m->overflow = m->unknown = 0;
    m->cells = m->done = m->seeds = m->seedsNoFloor = m->runs = 0;
    m->seedsBlocked = 0;  // v3.52 TP352
    m->nExit = nx < 0 ? 0 : nx > GT_EXIT_MAX ? GT_EXIT_MAX : nx;
    if (m->nExit) memcpy(m->x, x, sizeof(GtExit) * (size_t)m->nExit);
}
static int GtMapIdx(const GtMap *m, float x, float z)  // the cell nearest (x, z), -1 off the grid
{
    const float fi = floorf((x - m->x0) / m->cell + 0.5f), fj = floorf((z - m->z0) / m->cell + 0.5f);
    if (!m->on || !(fi >= 0.0f && fi < (float)m->nx && fj >= 0.0f && fj < (float)m->ny)) return -1;
    return (int)fj * m->nx + (int)fi;
}
static void GtMapXZ(const GtMap *m, int c, float *x, float *z)
{
    *x = m->x0 + m->cell * (float)(c % m->nx), *z = m->z0 + m->cell * (float)(c / m->nx);
}
static int GtMapFinished(const GtMap *m) { return m->on && !m->overflow && m->head == m->tail && m->seeds > 0; }
// beyond one of the room's door lines (its side away from the room, within its span and GT_EXIT_DEPTH, at its level)?
static int GtExitBeyond(const GtExit *x, int n, const float s[3])
{
    int i;
    for (i = 0; i < n; ++i) {
        const GtExit *e = &x[i];
        const float dx = e->x1 - e->x0, dz = e->z1 - e->z0, l = sqrtf(dx * dx + dz * dz);
        float u, d;
        if (!(fabsf(e->y - s[1]) <= GT_EXIT_DY) || !(l >= 1.0f)) continue;
        d = GtExitDist(e, s[0], s[2], &u);
        if (GtSide(e, e->px, e->pz) * GtSide(e, s[0], s[2]) < 0.0f && u >= -GT_EXIT_NEAR / l && u <= 1.0f + GT_EXIT_NEAR / l &&
            d < GT_EXIT_DEPTH)
            return 1;
    }
    return 0;
}
// v3.52 TP352: a floor point (x, its floor fy, z) a cell may hold: not inside solid GT_REACH_LOW above it, no polygon in
// her upper body's sphere (GT_REACH_BODY above it, radius GT_REACH_R). 1 = free (GtWalkStep's own cell rules, moved here)
static int GtCellFree(const GtWorld *w, float x, float fy, float z, int *unknown)
{
    int r;
    if ((r = w->solid(w->ctx, x, fy - GT_REACH_LOW, z)) != 0) return *unknown += r < 0, 0;
    if ((r = w->sphere(w->ctx, x, fy - GT_REACH_BODY, z, GT_REACH_R)) != 0) return *unknown += r < 0, 0;
    return 1;
}
// may she walk from a (x, its floor y, z) onto (x, z)? 1 (*fy its floor), 0 no (*unknown counts queries that could not answer)
static int GtWalkStep(const GtWorld *w, const float a[3], float x, float z, float *fy, int *unknown)
{
    static const float kDy[3] = { GT_REACH_LOW, GT_REACH_LOW2, GT_REACH_BODY };
    float y, la[3], lb[3];
    int r, k;
    if ((r = w->floorAt(w->ctx, x, a[1] - GT_REACH_UP, a[1] + GT_REACH_UP, z, &y)) <= 0 || !_finite(y)) {
        *unknown += r < 0;
        return 0;
    }
    if (!(fabsf(y - a[1]) <= GT_REACH_STEP)) return 0;
    if (!GtCellFree(w, x, y, z, unknown)) return 0;  // v3.52 TP352: (the same two queries as before, in GtCellFree)
    la[0] = a[0], la[2] = a[2], lb[0] = x, lb[2] = z;
    for (k = 0; k < 3; ++k) {
        la[1] = a[1] - kDy[k], lb[1] = y - kDy[k];
        if ((r = w->line(w->ctx, la, lb)) != 0) return *unknown += r < 0, 0;
    }
    *fy = y;
    return 1;
}
static int GtMapTake(GtMap *m, int c, float fy)  // mark a cell reached and queue it; 0 = the queue is full
{
    const int next = (m->tail + 1) % GT_MAP_RING;
    if (next == m->head) {
        m->overflow = 1;
        return 0;
    }
    m->st[c] = 1, m->fy[c] = (short)GtClampF(floorf(fy + 0.5f), -32000.0f, 32000.0f), ++m->cells;
    m->ring[m->tail] = c, m->tail = next;
    return 1;
}
// a seed: the floor at (x, z) from dy above y down to dy below it (a door's point, her feet). 1 = taken, 0 = already in
// the map at that level, -1 = no floor there (or off the grid, or beyond a door line; v3.52 TP352: or inside collision,
// counted in seedsBlocked: her feet inside a wall never seed the map)
static int GtMapSeed(GtMap *m, const GtWorld *w, float x, float y, float z, float dy)
{
    float fy, p[3];
    const int c = GtMapIdx(m, x, z);
    int r, unk = 0;
    if (c < 0) return ++m->seedsNoFloor, -1;
    if ((r = w->floorAt(w->ctx, x, y - dy, y + dy, z, &fy)) <= 0 || !_finite(fy)) {
        m->unknown += r < 0;
        return ++m->seedsNoFloor, -1;
    }
    if (m->st[c] && fabsf((float)m->fy[c] - fy) <= GT_REACH_HIT_DY) return 0;
    if (m->st[c]) return ++m->seedsNoFloor, -1;  // (another floor holds the cell: one level a cell)
    p[0] = x, p[1] = fy, p[2] = z;
    if (GtExitBeyond(m->x, m->nExit, p)) return ++m->seedsNoFloor, -1;
    if (!GtCellFree(w, x, fy, z, &unk)) return m->unknown += unk, ++m->seedsBlocked, -1;  // v3.52 TP352
    if (!GtMapTake(m, c, fy)) return -1;
    ++m->seeds;
    return 1;
}
// expand up to maxCells cells; returns how many were expanded (GtMapFinished: done)
static int GtMapRun(GtMap *m, const GtWorld *w, int maxCells)
{
    static const signed char kD[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    int n = 0;
    if (!m->on) return 0;
    ++m->runs;
    while (m->head != m->tail && n < maxCells) {
        const int c = m->ring[m->head], ci = c % m->nx, cj = c / m->nx;
        float a[3];
        int d;
        m->head = (m->head + 1) % GT_MAP_RING;
        ++n, ++m->done;
        GtMapXZ(m, c, &a[0], &a[2]);
        a[1] = (float)m->fy[c];
        for (d = 0; d < 8; ++d) {
            const int ni = ci + kD[d][0], nj = cj + kD[d][1];
            float b[3], fy;
            int nc;
            if (ni < 0 || ni >= m->nx || nj < 0 || nj >= m->ny) continue;
            nc = nj * m->nx + ni;
            if (m->st[nc]) continue;
            GtMapXZ(m, nc, &b[0], &b[2]);
            if (!(b[0] >= m->lo[0] && b[0] <= m->hi[0] && b[2] >= m->lo[2] && b[2] <= m->hi[2])) continue;
            if (!GtWalkStep(w, a, b[0], b[2], &fy, &m->unknown)) continue;
            b[1] = fy;
            if (GtExitBeyond(m->x, m->nExit, b)) continue;
            if (!GtMapTake(m, nc, fy)) return n;
        }
    }
    return n;
}
// does the map hold the spot's floor (spot = its floor point)? GTM_HAS: its cell or a neighbour cell at its level that
// she can step to from it; GTM_NOT: no (the map is finished); GTM_UNKNOWN: not yet (the map is not finished); GTM_NONE:
// no map (off the grid, no room box)
static int GtMapHas(const GtMap *m, const GtWorld *w, const float spot[3])
{
    const int c = GtMapIdx(m, spot[0], spot[2]);
    int di, dj, unk = 0;
    if (c < 0) return GTM_NONE;
    if (m->st[c] && fabsf((float)m->fy[c] - spot[1]) <= GT_REACH_HIT_DY) return GTM_HAS;
    for (dj = -1; dj <= 1; ++dj)
        for (di = -1; di <= 1; ++di) {
            const int ni = c % m->nx + di, nj = c / m->nx + dj;
            float b[3], fy;
            int nc;
            if ((!di && !dj) || ni < 0 || ni >= m->nx || nj < 0 || nj >= m->ny) continue;
            nc = nj * m->nx + ni;
            if (!m->st[nc] || !(fabsf((float)m->fy[nc] - spot[1]) <= GT_REACH_HIT_DY)) continue;
            GtMapXZ(m, nc, &b[0], &b[2]);
            if (GtWalkStep(w, spot, b[0], b[2], &fy, &unk) && fabsf(fy - (float)m->fy[nc]) <= 2.0f) return GTM_HAS;
        }
    return GtMapFinished(m) ? GTM_NOT : GTM_UNKNOWN;
}

// ---------------------------------------------------------------- v3.50 TP350: her body and the open floor ahead
// Her body at a spot (fy = its floor): no polygon in three spheres, the player's own two (PlayerWallFloorCollide: 70
// at 72 and 170 above her feet; the lower one here 60 at 100, so a floor's slope does not touch it) and her head room
// (70 at 300: up to 370 of her 400). 1 = free; a query that cannot answer = not free.
#define GT_BODY_LO_DY  100.0f
#define GT_BODY_LO_R   60.0f
#define GT_BODY_HI_DY  170.0f
#define GT_BODY_HI_R   70.0f
#define GT_HEAD_DY     300.0f
#define GT_HEAD_R      70.0f
#define GT_CHEST_DY    150.0f   // (echo.inc ECHO_CHEST_DY)
static int GtBodyFree(const GtWorld *w, float x, float fy, float z)
{
    return w->sphere(w->ctx, x, fy - GT_BODY_LO_DY, z, GT_BODY_LO_R) == 0 && w->sphere(w->ctx, x, fy - GT_BODY_HI_DY, z, GT_BODY_HI_R) == 0 &&
           w->sphere(w->ctx, x, fy - GT_HEAD_DY, z, GT_HEAD_R) == 0;
}
// GtThruSearch's `open` on the room's collision: could she walk straight from her feet (her[1] = her floor) to the
// sample c? Its floor at her level (GT_FLOOR_TOL), nothing on the lines from her chest to its chest and from her low
// line (GT_REACH_LOW above the floor) to its (a turnstile, a fence, a counter below her chest). 1 = open.
static int GtThruOpen(const GtWorld *w, const float her[3], const float c[3])
{
    float fy, a[3], b[3];
    if (w->floorAt(w->ctx, c[0], her[1] - GT_REACH_UP, her[1] + GT_REACH_UP, c[2], &fy) <= 0 || !_finite(fy) ||
        !(fabsf(fy - her[1]) <= GT_FLOOR_TOL))
        return 0;
    a[0] = her[0], a[1] = her[1] - GT_CHEST_DY, a[2] = her[2], b[0] = c[0], b[1] = fy - GT_CHEST_DY, b[2] = c[2];
    if (w->line(w->ctx, a, b) != 0) return 0;
    a[1] = her[1] - GT_REACH_LOW, b[1] = fy - GT_REACH_LOW;
    return w->line(w->ctx, a, b) == 0;
}

// ================================================================ v3.51 TP351: never behind an enemy on the other side of collision
// The user's v3.50 QA item 2 (staging/v351_feedback/USER_FEEDBACK_v350.md): "If there is collision in the way, do not
// teleport behind enemies on the other side, just teleport to or through the collision." Before an enemy in the cone
// counts as the one she faces, the straight line from her to it is tested against the room's collision (the tree2 line
// 0x561CD0 that the through-warp search and the floor map cast: walls, props, turnstiles, never characters), as
// GtThruOpen tests a sample: from her chest (GT_CHEST_DY above her floor) to its chest (above its floor) and, when both
// stand on one level (GT_FLOOR_TOL), from her low line (GT_REACH_LOW) to its (a turnstile, a counter, a fence below her
// chest; on stairs or another level the low line would clip the steps' edges: the chest line only). Its chest inside
// solid (a ghost inside a wall), or a ghost in its wall state (the caller's wallState: its own wall portal open), counts
// as behind collision too. A query that cannot answer counts as blocked (fail closed: never behind an enemy not proven
// in the open). The caller then takes the next enemy in the cone (GtPickFacedVia); none = the warp ahead (through the
// wall onto reachable floor, or up to it), exactly as with no enemy in the cone.
enum { GTV_CLEAR = 0, GTV_CHEST, GTV_LOW, GTV_INSIDE, GTV_WALLSTATE, GTV_UNKNOWN, GTV_N };
static const char *const kGtSightWhy[GTV_N] = { "in the open", "a wall between (her chest line)",
    "a low barrier between (a turnstile, a counter, a fence)", "inside a wall", "a ghost in its wall (its portal open)",
    "the collision could not answer" };
// her: her feet (y = her floor); foe: its floor point (x, its floor y, z). GTV_CLEAR = she has a clear line to it.
static int GtFoeSight(const GtWorld *w, const float her[3], const float foe[3], int wallState)
{
    float a[3], b[3];
    int r;
    if (wallState) return GTV_WALLSTATE;
    if (!_finite(her[0]) || !_finite(her[1]) || !_finite(her[2]) || !_finite(foe[0]) || !_finite(foe[1]) || !_finite(foe[2]))
        return GTV_UNKNOWN;
    if ((r = w->solid(w->ctx, foe[0], foe[1] - GT_CHEST_DY, foe[2])) != 0) return r < 0 ? GTV_UNKNOWN : GTV_INSIDE;
    a[0] = her[0], a[1] = her[1] - GT_CHEST_DY, a[2] = her[2], b[0] = foe[0], b[1] = foe[1] - GT_CHEST_DY, b[2] = foe[2];
    if ((r = w->line(w->ctx, a, b)) != 0) return r < 0 ? GTV_UNKNOWN : GTV_CHEST;
    if (fabsf(foe[1] - her[1]) <= GT_FLOOR_TOL) {
        a[1] = her[1] - GT_REACH_LOW, b[1] = foe[1] - GT_REACH_LOW;
        if ((r = w->line(w->ctx, a, b)) != 0) return r < 0 ? GTV_UNKNOWN : GTV_LOW;
    }
    return GTV_CLEAR;
}
// GtPickFaced, then the caller's test of each pick (0 = it counts; else it is marked skip and the next in the cone is
// picked: v3.50's "not in this room", v3.51's "behind collision"). The index, or -1: none left in the cone. At most n + 1
// picks (each failed one leaves the cone for good).
static int GtPickFacedVia(GtCand *c, int n, float range, float cone, int (*test)(void *ctx, int i), void *ctx)
{
    int k, i;
    for (k = 0; k <= n; ++k) {
        if ((i = GtPickFaced(c, n, range, cone)) < 0) return -1;
        if (!test || test(ctx, i) == 0) return i;
        c[i].skip = 1;
    }
    return -1;
}

// ================================================================ v3.52 TP352: every landing proven; freed from a wall
// The user's v3.51 QA D4: stuck in a Water Prison wall (room 4:11) after warp 129, "behind type 09, 343 u away, spot 2:
// 160 u on its far side (no usable facing), floor +0 u", from (-1752, 788) to (-1590, 1248). The spots behind an enemy and
// v3.44's spots ahead passed only v3.43's rules (the mover's floor ray 0x459490, 0x458F00's flat floor, the point 4 u
// above the floor not inside tree2 0x46F700, the anchor's chest line 0x454000): nothing tested her BODY against the walls,
// so a spot 20 u from a wall passed (her spheres need 70). The through-warp's spots (v3.50) had the proof; now every
// landing kind has the same one (GtLandProof): her body's three spheres free (GtBodyFree, 0x561B80 the polygons the
// player's own wall collision 0x5339D0 asks) and the room's floor map holds the spot (GtMapHas: a route of the room
// reaches it by her walk rules). A map that is not finished: "not known yet" (the press may start the fade, the warp
// decides); no map (no solid map in the room, the queries not stock, off the grid): not proven (fail closed).
enum { GTP_OK = 0, GTP_BODY, GTP_ROUTE, GTP_PENDING, GTP_NOMAP, GTP_N };
static const char *const kGtProofWhy[GTP_N] = { "proven", "no room for her body", "no route of the room reaches it",
                                                "not on the room's floor map yet", "no floor map (not proven)" };
// spot = the landing's floor point (x, its floor y, z)
static int GtLandProof(const GtMap *m, const GtWorld *w, const float spot[3])
{
    int r;
    if (!_finite(spot[0]) || !_finite(spot[1]) || !_finite(spot[2])) return GTP_NOMAP;
    if (!m || !m->on) return GTP_NOMAP;
    if (!GtBodyFree(w, spot[0], spot[1], spot[2])) return GTP_BODY;
    r = GtMapHas(m, w, spot);
    return r == GTM_HAS ? GTP_OK : r == GTM_UNKNOWN ? GTP_PENDING : r == GTM_NOT ? GTP_ROUTE : GTP_NOMAP;
}
// Her body overlaps the room's collision at her feet p: No Clip's "clear" body sphere (noclip_world.h NwClearAt: 60 at
// 110 above her feet; the stock collision keeps her clear of it in every position it gives her) touches a polygon, or the
// point GT_REACH_LOW above her feet is inside solid (tree2). 1 = stuck, 0 = clear, -1 = the sphere query cannot answer
// (never "stuck" on an unknown: she is not moved on a guess).
#define GT_STUCK_DY    110.0f
#define GT_STUCK_R     60.0f
static int GtStuck(const GtWorld *w, const float p[3])
{
    int s, q;
    if (!_finite(p[0]) || !_finite(p[1]) || !_finite(p[2])) return -1;
    s = w->sphere(w->ctx, p[0], p[1] - GT_STUCK_DY, p[2], GT_STUCK_R);
    if (s > 0) return 1;
    q = w->solid(w->ctx, p[0], p[1] - GT_REACH_LOW, p[2]);
    if (q > 0) return 1;
    return s < 0 ? -1 : 0;  // (solid unknown, no polygon round her body: clear)
}
// After a warp her body is watched GT_FREE_WATCH_S of gameplay time (menus, scenes: held): stuck GT_FREE_STUCK_S in a row
// (the stock collision did not push her out) = free her (1, every tick until the caller stops the watch: it may have to
// wait for a gate); a stuck run still going at the window's end is followed to its end; never past GT_FREE_END_S.
#define GT_FREE_WATCH_S 1.0f
#define GT_FREE_STUCK_S 0.15f
#define GT_FREE_END_S   3.0f
typedef struct { int on; float t, stuck; } GtFreeWatch;
static void GtFreeWatchStart(GtFreeWatch *f) { f->on = 1, f->t = 0.0f, f->stuck = 0.0f; }
static void GtFreeWatchStop(GtFreeWatch *f) { f->on = 0, f->t = 0.0f, f->stuck = 0.0f; }
// one tick: live = a gameplay tick (outside it nothing counts); stuck = GtStuck now (1 / 0 / -1)
static int GtFreeWatchStep(GtFreeWatch *f, int live, int stuck, float dt)
{
    if (!f->on) return 0;
    if (!(dt > 0.0f) || dt > 0.25f) dt = 0.0f;  // (a stall or a bad clock: no time passes)
    if (!live) return 0;
    f->t += dt;
    if (f->t >= GT_FREE_END_S) {
        GtFreeWatchStop(f);
        return 0;
    }
    f->stuck = stuck > 0 ? f->stuck + dt : 0.0f;  // (unknown: not stuck)
    if (stuck > 0 && f->stuck >= GT_FREE_STUCK_S - 1e-4f) return 1;
    if (f->t >= GT_FREE_WATCH_S && !(f->stuck > 0.0f)) GtFreeWatchStop(f);
    return 0;
}
// The nearest proven floor cell to `from` (her feet: x, her floor, z; xz distance) within maxR: a cell of the room's
// floor map at her level (GT_FREE_DY), its floor found again at its centre (within 2 u of the map's), her body free there
// (GtBodyFree) and not stuck (GtStuck 0), and the caller's rules (ok: the room's door lines, the walk-in zones, the game's
// own floor ray and room to stand; NULL = none). Searched ring by ring outward; the first ring that cannot hold a nearer
// cell ends it. 1 = found (out: its floor point); *tried counts the cells tested.
#define GT_FREE_R      480.0f
#define GT_FREE_DY     100.0f
static int GtFreeCell(const GtMap *m, const GtWorld *w, const float from[3], float maxR, int (*ok)(void *ctx, const float p[3]),
                      void *ctx, float out[3], int *tried)
{
    float bestD2;
    int ci, cj, rad, r, found = 0;
    *tried = 0;
    if (!m || !m->on || !(maxR > 0.0f) || !_finite(from[0]) || !_finite(from[1]) || !_finite(from[2])) return 0;
    ci = (int)floorf((from[0] - m->x0) / m->cell + 0.5f), cj = (int)floorf((from[2] - m->z0) / m->cell + 0.5f);
    rad = (int)ceilf(maxR / m->cell);
    if (rad > GT_MAP_DIM) rad = GT_MAP_DIM;
    bestD2 = maxR * maxR;
    for (r = 0; r <= rad; ++r) {
        int di, dj;
        if (found && bestD2 <= ((float)r - 0.5f) * m->cell * ((float)r - 0.5f) * m->cell) break;  // (ring r is farther)
        for (dj = -r; dj <= r; ++dj)
            for (di = -r; di <= r; ++di) {
                const int i = ci + di, j = cj + dj;
                float x, z, fy, d2, p[3];
                int c;
                if ((di != -r && di != r && dj != -r && dj != r) || i < 0 || i >= m->nx || j < 0 || j >= m->ny) continue;
                c = j * m->nx + i;
                if (!m->st[c] || !(fabsf((float)m->fy[c] - from[1]) <= GT_FREE_DY)) continue;
                GtMapXZ(m, c, &x, &z);
                d2 = (x - from[0]) * (x - from[0]) + (z - from[2]) * (z - from[2]);
                if (d2 > bestD2 || (found && d2 >= bestD2)) continue;
                ++*tried;
                if (w->floorAt(w->ctx, x, (float)m->fy[c] - GT_REACH_UP, (float)m->fy[c] + GT_REACH_UP, z, &fy) <= 0 || !_finite(fy) ||
                    !(fabsf(fy - (float)m->fy[c]) <= 2.0f))
                    continue;
                if (!GtBodyFree(w, x, fy, z)) continue;
                p[0] = x, p[1] = fy, p[2] = z;
                if (GtStuck(w, p) != 0 || (ok && !ok(ctx, p))) continue;
                out[0] = x, out[1] = fy, out[2] = z, bestD2 = d2, found = 1;
            }
    }
    return found;
}
#endif
