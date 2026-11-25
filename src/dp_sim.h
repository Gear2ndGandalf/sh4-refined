// ================================================================ v3.27.0 Death Presence: the pure state code
// staging/v327_death_presence/PLAN.md sections 3-7. Pure C (no D3D, no game memory): included by deathpresence.inc and
// by the native test test/dp_sim_test.c. Every amount is the EXACT integral of the ramp over the step, so the damage a
// being takes over any interval is the same at any tick rate (30 / 60 / 120 / 180 Hz, a 30 Hz gate): PLAN A3.
#ifndef DP_SIM_H
#define DP_SIM_H
#include <math.h>
#include <float.h>   // v3.41 DP341: _finite (DpGateStep); v3.42 SM342: + DpOffHere, DpViewStep (Room 302, the view gate)
#include <string.h>

#define DP_LEVELS     3         // Off / Weak / Strong
#define DP_RAMP_S     10.0f     // time inside to the full rate
#define DP_DECAY_K    2.0f      // outside: the time inside falls 2 x as fast
#define DP_FORGET_S   3.0f      // a being outside this long is forgotten
#define DP_VICTIMS    64        // tracked beings (monsters, ghosts)
#define DP_GRACE_S    5.0f      // Eileen: no fog damage this long after she is up from a collapse
#define DP_GAP_S      0.2f      // a being missing from the list this long was removed: its slot starts clean (DP-R401)

typedef unsigned int DpU32;
typedef struct { float R, lo, hi; } DpLevel;  // radius (units); fraction of max HP a second at entry / after DP_RAMP_S
static const DpLevel kDpLevel[DP_LEVELS] = { { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.02f, 0.08f }, { 400.0f, 0.05f, 0.20f } };

static DpU32 DpHash(DpU32 a)
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float DpRnd(DpU32 *s)  // Refined's own stream (never the game's 0x4215D0): 0 .. 1
{
    *s = DpHash(*s + 0x9E3779B9u);
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

// ---- the ramp: tin (seconds inside, 0..DP_RAMP_S). Returns the fraction of max HP to take over this step of dt
// seconds: the integral of lo + (hi - lo) tin / DP_RAMP_S while inside (0 outside), tin updated.
static float DpRampStep(float *tin, int inside, float dt, float lo, float hi)
{
    float t0, t1, ramp, a;
    if (!(dt > 0.0f)) return 0.0f;
    t0 = *tin;
    if (!(t0 >= 0.0f)) t0 = 0.0f;  // (NaN too)
    if (!inside) {
        t1 = t0 - DP_DECAY_K * dt;
        *tin = t1 > 0.0f ? t1 : 0.0f;
        return 0.0f;
    }
    if (t0 > DP_RAMP_S) t0 = DP_RAMP_S;
    t1 = t0 + dt < DP_RAMP_S ? t0 + dt : DP_RAMP_S;
    ramp = t1 - t0;  // the part of the step on the ramp, then the rest at hi
    a = lo * ramp + (hi - lo) * (t1 * t1 - t0 * t0) / (2.0f * DP_RAMP_S) + hi * (dt - ramp);
    *tin = t1;
    return a;
}
static float DpRampFrac(float tin) { return tin <= 0.0f ? 0.0f : tin >= DP_RAMP_S ? 1.0f : tin / DP_RAMP_S; }  // r 0..1 (art)

// ---- v3.27.1 (user item 2): the stain s (0..1) a being carries: it follows the ramp r while inside and, once outside,
// fades with a half-life of DP_STAIN_HALF seconds (0.5 after 3 s, 0.16 after 8 s, under DP_STAIN_MIN after 17 s, when
// the being leaves the art list). Exponential, so the value at any time is the same at any tick rate (B4).
#define DP_STAIN_HALF 3.0f
#define DP_STAIN_MIN  0.02f
// the art's fade at the end of a stain: 0 at DP_STAIN_MIN (where the being leaves the art list), 1 from 0.15 (review
// DP3271-001: a cue scaled like 0.2 + 0.8 s would otherwise pop out at 0.02)
static float DpStainFade(float s)
{
    float x = (s - DP_STAIN_MIN) / (0.15f - DP_STAIN_MIN);
    x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    return x * x * (3.0f - 2.0f * x);
}
static float DpStainStep(float *s, int inside, float r, float dt)
{
    float v = *s;
    if (!(v >= 0.0f)) v = 0.0f;  // (NaN too)
    if (v > 1.0f) v = 1.0f;
    if (dt > 0.0f) v *= exp2f(-dt / DP_STAIN_HALF);
    if (inside && r > v) v = r > 1.0f ? 1.0f : r;
    if (v < 1e-4f) v = 0.0f;
    *s = v;
    return v;
}

// ---- avoidance (PLAN 7): a hesitate / commit cycle per monster
enum { AV_HESITATE, AV_COMMIT };
typedef struct { float hMax, hCommit, vBack, hes0, hes1, com0, com1; } DpAvoidK;
static const DpAvoidK kDpAvoid[DP_LEVELS] = { { 0, 0, 0, 0, 0, 0, 0 },
                                              { 0.60f, 0.20f, 35.0f, 1.0f, 2.0f, 3.0f, 5.0f },
                                              { 0.80f, 0.35f, 70.0f, 1.8f, 3.5f, 2.5f, 4.0f } };
typedef struct { int phase, armed; float t, tout; } DpAvoid;
// inside / outside this step; attacking (class 5) or hit by her = commit (at least 1 s)
static void DpAvoidStep(DpAvoid *a, const DpAvoidK *k, int inside, int attacking, int hit, float dt, DpU32 *rng)
{
    if (!(dt > 0.0f)) return;
    if (!inside) {
        a->tout += dt;
        if (a->tout > 1.5f) a->armed = 0;  // re-armed: the next entry starts hesitating
        return;
    }
    a->tout = 0.0f;
    if (!a->armed) {
        a->armed = 1;
        a->phase = AV_HESITATE;
        a->t = k->hes0 + (k->hes1 - k->hes0) * DpRnd(rng);
    }
    if (attacking || hit) {
        a->phase = AV_COMMIT;
        if (a->t < 1.0f) a->t = 1.0f;
        return;
    }
    a->t -= dt;
    if (a->t <= 0.0f) {
        a->phase ^= 1;
        a->t = a->phase == AV_HESITATE ? k->hes0 + (k->hes1 - k->hes0) * DpRnd(rng) : k->com0 + (k->com1 - k->com0) * DpRnd(rng);
    }
}
// the share of this step's inward motion removed at depth u (0 at the rim .. 1 at 0.3 R inside)
static float DpAvoidCut(const DpAvoid *a, const DpAvoidK *k, float R, float d)
{
    float u;
    if (!(R > 0.0f) || !(d < R)) return 0.0f;
    u = (R - d) / (0.3f * R);
    u = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    u = u * u * (3.0f - 2.0f * u);
    return (a->phase == AV_HESITATE ? k->hMax : k->hCommit) * u;
}

// ---- the victim table: keyed by task + work (pool reuse: a new work for the same task is a new being)
typedef struct {
    const void *task, *work;
    float tin, seen;           // seconds inside; the clock when last updated
    float pend;                // fraction of max HP carried to the being's next own update tick (review DP-007)
    float lastIn;              // the clock when it was last inside (review DP-I002: 3 s outside = exposure forgotten)
    int blowSent, lastCls;     // the last blow went out in this class
    int hold;                  // a downed ghost's countdown is held
    float held;                // ... at this value
    DpU32 serial;              // ... the hit serial when it was taken
    DpAvoid av;
    DpU32 avSerial;            // the hit serial avoidance last saw (review DP-R601: not the hold's `serial`)
    DpU32 rng;
    float stain;               // v3.27.1: the stain s the art shows (DpStainStep); 0 on a new being
    float artT;                // v3.27.1: the art's own exposure (seconds inside, DpRampStep, never read by damage)
} DpVictim;
typedef struct { DpVictim v[DP_VICTIMS]; float clock; DpU32 roomKey; int drops; } DpTable;

static void DpTableReset(DpTable *t, DpU32 roomKey)
{
    memset(t->v, 0, sizeof t->v);
    t->roomKey = roomKey;
}
// the slot of (task, work); a new being takes a free / forgotten slot, else the stalest. NULL: no task.
static DpVictim *DpFind(DpTable *t, const void *task, const void *work)
{
    int i, freeI = -1, oldI = 0;
    if (!task) return NULL;
    for (i = 0; i < DP_VICTIMS; ++i) {
        DpVictim *v = &t->v[i];
        if (v->task == task) {
            if (v->work == work && t->clock - v->seen <= DP_GAP_S) {
                v->seen = t->clock;
                return v;
            }  // review DP-R401: a new work, or the same addresses after a gap (removed and re-made): a new being
            memset(v, 0, sizeof *v);  // the task was reused for another being: start clean
            freeI = i;
            break;
        }
        if (v->task && t->clock - v->seen > DP_FORGET_S) memset(v, 0, sizeof *v);
        if (!v->task && freeI < 0) freeI = i;
        if (v->seen < t->v[oldI].seen) oldI = i;
    }
    if (freeI < 0) {
        freeI = oldI;
        ++t->drops;
    }
    {
        DpVictim *v = &t->v[freeI];
        memset(v, 0, sizeof *v);
        v->task = task;
        v->work = work;
        v->seen = t->clock;
        v->lastCls = -1;
        v->lastIn = t->clock;
        v->rng = DpHash((DpU32)(size_t)task ^ (DpU32)(size_t)work * 0x9E3779B9u);
        return v;
    }
}

// review DP-I002: a being's exposure follows inside / outside, not table visits: inside stamps lastIn; outside for
// DP_FORGET_S forgets its time inside (its pending damage stays: DP-I003)
static void DpVictimSeen(DpVictim *v, int inside, float clock)
{
    if (inside) v->lastIn = clock;
    else if (clock - v->lastIn > DP_FORGET_S) v->tin = 0.0f;
}

// ---- Eileen (PLAN 4): her own ramp; no damage while she is down, a grace after she is up
typedef struct { float tin, grace, acc, lastIn; int down; float stain, artT; } DpEileen;  // v3.27.1: stain, art exposure
// down = her state is the collapse (0x14 / 0x15) this tick. Returns 1 when fog damage may be applied this tick.
static int DpEileenGate(DpEileen *e, int down, float dt)
{
    if (down) {
        e->down = 1;
        e->tin = 0.0f;  // her ramp restarts after a collapse
        e->acc = 0.0f;
        return 0;
    }
    if (e->down) {
        e->down = 0;
        e->grace = DP_GRACE_S;
    }
    if (e->grace > 0.0f) {
        e->grace -= dt;
        return 0;
    }
    return 1;
}

// ---- the ghost hold (PLAN 5): returns the countdown to write back, or a negative value for "leave it".
// Review DP-I201: the value taken is written back at once and is at least DP_HOLD_MIN, so a countdown caught just
// before it runs out cannot expire in the ghost's update that follows (one update takes at most 1/30 s).
#define DP_HOLD_MIN 0.1f
static float DpHoldStep(DpVictim *v, int inside, float cd, DpU32 serial)
{
    if (!inside || !(cd > 0.0f)) {
        v->hold = 0;
        return -1.0f;
    }
    if (!v->hold || serial != v->serial || cd > v->held) {  // a new down / a new hit / the game raised it: take it
        v->hold = 1;
        v->held = cd > DP_HOLD_MIN ? cd : DP_HOLD_MIN;
        v->serial = serial;
    }
    return v->held;
}

// ---- v3.41 DP341 (USER_FEEDBACK_v340 "Added by the user" 1): the fog's draw gate. The fog belongs round the player in
// the room she is in, drawn over that room's depth. It is not drawn (every layer: pool, puffs, wisps, trail, cloak,
// victims, columns) while
//   - the room's geometry is off ([0xFD6C78] gRoomModels.drawn == 0: a room exit / load, the Room 302 door scope and wall
//     hole views, the window look-out, the Water Prison peep, the item box, the save screen: the scene shows another
//     place or nothing, so nothing of the room hides the fog and it stood in that view),
//   - the Room 302 peeping flag is on ([0xFD5BE8] gPeepFlag188),
//   - a move of the player is pending ([0x10A12FC] gPlayer[0].warp: a room change's entry place not applied yet; at most
//     DP_GATE_WARP_MAX),
//   - or her position is not finite;
// and it settles after the room key changed or she jumped more than DP_GATE_JUMP (or DP_GATE_JUMP_V x dt) in a frame (a
// teleport: a room entry applied a frame or more after the room number changed, a warp, the Water Prison peep's 1000 u
// lift): drawing
// resumes once the scene has been ready and her position still (no jump) for DP_GATE_SETTLE_S and DP_GATE_SETTLE_F
// frames, and that frame starts every anchored piece again at her (*reset), so nothing anchored at an old or stale
// place (the wisps keep their birth place for 4-7 s) is drawn in the new room. A zeroed DpGate holds (DPW_START).
#define DP_GATE_SETTLE_S 0.30f
#define DP_GATE_SETTLE_F 4
#define DP_GATE_JUMP     150.0f
#define DP_GATE_JUMP_V   3000.0f  // ... or this many u/s x the frame's dt when larger (No Clip's Fly reaches 2652 u/s: at 15 fps
                                  // 177 u a frame is not a jump)
#define DP_GATE_WARP_MAX 2.0f   // a pending move holds at most this long (an event may set one with no room change; the
                                // game applies it only at the next player start): then it is ignored until it clears
enum { DPG_HOLD, DPG_SHOW };
enum { DPW_START, DPW_ROOMOFF, DPW_PEEP, DPW_WARP, DPW_BADPOS, DPW_ROOM, DPW_JUMP, DPW_N };
typedef struct {
    DpU32 room;          // the room key (stage << 16 | room)
    int roomDrawn;       // [0xFD6C78]
    int peep;            // [0xFD5BE8]
    int warp;            // [0x10A12FC]
    float pos[3];        // her position 0x10A0F60
} DpGateIn;
typedef struct {
    int state, why;      // DPG_*; the reason of the current / last hold (DPW_*)
    int roomOk, prevOk, frames;
    DpU32 room;
    float prev[3], t;    // her last position; seconds ready and still (the settle)
    float heldS;         // seconds of the current hold (for the log)
    float warpS;         // seconds the pending move has been seen (DP_GATE_WARP_MAX)
    int changed;         // 1 on a step that went from showing to holding
    int warpCapped;      // 1 on the step a pending move passed DP_GATE_WARP_MAX (for the log)
} DpGate;
static const char *DpGateWhy(int why)
{
    static const char *const k[DPW_N] = { "the start", "the room is not drawn (a view, a load or a menu)", "the Room 302 peeping flag",
                                          "a move of the player is pending", "her position is not valid", "a room change",
                                          "a jump of her position" };
    return why >= 0 && why < DPW_N ? k[why] : "?";
}
// one drawn frame of dt real seconds (0 after a gap): 1 = draw the fog; *reset = 1 on the frame it resumes
static int DpGateStep(DpGate *g, const DpGateIn *in, float dt, int *reset)
{
    const int fin = _finite(in->pos[0]) && _finite(in->pos[1]) && _finite(in->pos[2]);
    int why = -1;
    *reset = 0;
    g->changed = g->warpCapped = 0;
    if (!(dt >= 0.0f) || dt > 0.25f) dt = dt > 0.25f ? 0.25f : 0.0f;  // (NaN: 0)
    if (in->warp) {
        const float w0 = g->warpS;
        g->warpS += dt;
        g->warpCapped = w0 < DP_GATE_WARP_MAX && g->warpS >= DP_GATE_WARP_MAX;
    } else {
        g->warpS = 0.0f;
    }
    if (!in->roomDrawn) why = DPW_ROOMOFF;
    else if (in->peep) why = DPW_PEEP;
    else if (in->warp && g->warpS < DP_GATE_WARP_MAX) why = DPW_WARP;
    else if (!fin) why = DPW_BADPOS;
    else if (g->roomOk && in->room != g->room) why = DPW_ROOM;
    else if (g->prevOk) {
        const float dx = in->pos[0] - g->prev[0], dy = in->pos[1] - g->prev[1], dz = in->pos[2] - g->prev[2];
        const float lim = DP_GATE_JUMP_V * dt > DP_GATE_JUMP ? DP_GATE_JUMP_V * dt : DP_GATE_JUMP;
        if (!(dx * dx + dy * dy + dz * dz <= lim * lim)) why = DPW_JUMP;
    }
    g->room = in->room;
    g->roomOk = 1;
    if (fin) memcpy(g->prev, in->pos, sizeof g->prev), g->prevOk = 1;
    if (why >= 0) {
        if (g->state == DPG_SHOW) g->state = DPG_HOLD, g->heldS = 0.0f, g->changed = 1;
        g->why = why;
        g->t = 0.0f;
        g->frames = 0;
        g->heldS += dt;
        return 0;
    }
    if (g->state == DPG_SHOW) return 1;
    g->t += dt;
    g->heldS += dt;
    ++g->frames;
    if (g->t >= DP_GATE_SETTLE_S && g->frames >= DP_GATE_SETTLE_F) {
        g->state = DPG_SHOW;
        *reset = 1;
        return 1;
    }
    return 0;
}

// ---- v3.42 SM342 (USER_FEEDBACK_v341 item 7: "go ahead and remove Death Presence from Room 302"): Room 302 is stage 1
// in both halves of the game and stage 0x0A (the crawl; GameStageIsRoom302 0x4FC810 reads both, as items.inc / echo.inc
// do). There Death Presence is off: no fog, no haunting drained (the hauntings run as stock), no avoidance, no back-off.
// The hidden [ScreenEffects] DeathPresenceRoom302=1 brings v3.41's behaviour back (stage 1: its haunting branch, 0x0A: the
// usual one).
static int DpIsRoom302(DpU32 stage) { return stage == 1u || stage == 0x0Au; }
static int DpOffHere(DpU32 stage, int room302On) { return DpIsRoom302(stage) && !room302On; }
// once per entry into Room 302: *in = the last stage was Room 302 (-1 = none seen yet). 1 = this stage enters it (log)
static int DpR302Enter(int *in, DpU32 stage)
{
    const int now = DpIsRoom302(stage);
    const int enter = now && *in != 1;
    *in = now;
    return enter;
}

// ---- v3.42 SM342 (USER_FEEDBACK_v341, the workers' open questions: "Echo Doubles / After Shock hidden in peep views
// too: yes"): the view gate of Echo Doubles and After Shock. The first three signals of DpGateStep hide them: the room's
// geometry off ([0xFD6C78] == 0: the Room 302 door scope and wall hole, the window look-out, the Water Prison peep, a
// load, the item box), the Room 302 peeping flag ([0xFD5BE8]) and a pending move of the player ([0x10A12FC], at most
// DP_GATE_WARP_MAX). They are drawn again once those have been clear for DP_GATE_SETTLE_S and DP_GATE_SETTLE_F frames
// (the moment the fog comes back after a view). Only the drawing stops: their ticks (stuns, the shadows' hits, timers)
// run on and nothing is reset. A zeroed DpView shows (an effect that starts in a view hides on its first frame).
enum { DPV_SHOW, DPV_HIDE };
typedef struct {
    int state, why;      // DPV_*; the reason of the current / last hide (DPW_ROOMOFF, DPW_PEEP, DPW_WARP)
    int frames;          // frames clear since the signals went (the settle)
    float t, heldS;      // seconds clear (the settle); seconds of the current hide (for the log)
    float warpS;         // seconds the pending move has been seen
    int changed, resumed, warpCapped;  // this step: went hidden / shown again / a pending move passed DP_GATE_WARP_MAX
} DpView;
// one drawn frame of dt real seconds (0 after a gap): 1 = draw the effect
static int DpViewStep(DpView *g, int roomDrawn, int peep, int warp, float dt)
{
    int why = -1;
    g->changed = g->resumed = g->warpCapped = 0;
    if (!(dt >= 0.0f) || dt > 0.25f) dt = dt > 0.25f ? 0.25f : 0.0f;  // (NaN: 0)
    if (warp) {
        const float w0 = g->warpS;
        g->warpS += dt;
        g->warpCapped = w0 < DP_GATE_WARP_MAX && g->warpS >= DP_GATE_WARP_MAX;
    } else {
        g->warpS = 0.0f;
    }
    if (!roomDrawn) why = DPW_ROOMOFF;
    else if (peep) why = DPW_PEEP;
    else if (warp && g->warpS < DP_GATE_WARP_MAX) why = DPW_WARP;
    if (why >= 0) {
        if (g->state == DPV_SHOW) g->state = DPV_HIDE, g->heldS = 0.0f, g->changed = 1;
        g->why = why;
        g->t = 0.0f;
        g->frames = 0;
        g->heldS += dt;
        return 0;
    }
    if (g->state == DPV_SHOW) return 1;
    g->t += dt;
    g->heldS += dt;
    ++g->frames;
    if (g->t >= DP_GATE_SETTLE_S && g->frames >= DP_GATE_SETTLE_F) {
        g->state = DPV_SHOW;
        g->resumed = 1;
        return 1;
    }
    return 0;
}
#endif
