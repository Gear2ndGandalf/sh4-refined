// ================================================================ v3.26.0 Noise Type Rain on glass: the simulation
// staging/v326_sv_rain/PLAN.md section 3. Pure C (no D3D, no game memory): included by screenfx.inc and by the native
// identity test test/rain_sim_test.c. Coordinates are 720-line pixels (y down; the width is 720 x the aspect). The state
// advances only in fixed 1/120 s steps (RnAdvance), and every random choice is a hash of a drop serial / slot / step, so
// for the same elapsed rain time the state is bit-identical at any frame rate (review SVR-005).
#ifndef RAIN_SIM_H
#define RAIN_SIM_H
#include <math.h>
#include <string.h>

#define RN_HZ          120      // simulation steps a second
#define RN_MAX_STEPS   60       // steps a frame at most (0.5 s; a longer frame loses the excess: a stall)
#define RN_DROPS       64       // drops alive (impacts, sitting, sliding)
#define RN_SLIDERS     10       // sliding at once
#define RN_SEGS        2048     // trail segments (10 sliders x 30 / s x 6 s = 1800)
#define RN_BEADS       1024     // beads left by thinning trails (expected <= 165 / s x 5.6 s = 924 alive; admission keeps live ones)
#define RN_MIST        280      // micro-droplets
#define RN_R0          3.0f     // drop radius 3 + 11 u^2 (u uniform): mostly small
#define RN_R1          14.0f
#define RN_RMAX        20.0f    // merged drops at most this
#define RN_BEAD_R      2.5f     // a slider smaller than this stops (a bead)
#define RN_GROW_S      0.08f    // impact: grows from 0.4 r to r over this
#define RN_SIT0        1.5f     // sit 1.5 .. 5 s before it may slide
#define RN_SIT1        3.5f
#define RN_LIFE_FADE   0.6f     // a drop that does not slide fades over this after its sit + 2 .. 6 s
#define RN_SLIDE_P0    0.45f    // slide chance 0.35 + 0.4 x r / 14
#define RN_SLIDE_P1    0.40f
#define RN_ANGLE       0.61f    // heading within +-35 degrees of straight down
#define RN_WALK        0.05f    // heading random walk per step (rad, uniform +-)
#define RN_PULL        0.02f    // ... pulled back toward the start heading per step
#define RN_SLOT0       0.15f    // stick-slip slots 0.15 .. 0.6 s
#define RN_SLOT1       0.45f
#define RN_SLIP_P      0.65f    // a slot is a slip with this chance (else a stick)
#define RN_SPD0        80.0f    // slip speed (80 .. 280) x r / 10 px / s, at most RN_SPD_MAX
#define RN_SPD1        200.0f
#define RN_SPD_MAX     380.0f
#define RN_SPD_EASE    0.08f    // per step toward the slot's speed
#define RN_STICK_DAMP  0.85f    // per step while sticking
#define RN_SHRINK      0.12f    // a slider loses 12 % of its radius per 100 px slid (into its trail)
#define RN_SEG_MIN     3.0f     // a new trail segment every max(3 px, speed / 30 s)
#define RN_TRAIL_W     0.70f    // trail width at birth x the slider's radius
#define RN_TRAIL0      3.0f     // trail life 3 .. 6 s
#define RN_TRAIL1      3.0f
#define RN_BEAD_AT     0.40f    // a segment beads from this share of its life
#define RN_BEAD_P      0.55f    // ... with this chance
#define RN_BEAD_GAP0   6.0f     // bead spacing 6 .. 14 px
#define RN_BEAD_GAP1   8.0f
#define RN_BEAD_EXTRA  2.0f     // beads live up to this past their segment
#define RN_MIST_FILL   6.0f     // micro-drops fade in over this (accumulating rain)
#define RN_MIST_BACK0  8.0f     // a cleared micro-drop comes back after 8 .. 15 s
#define RN_MIST_BACK1  7.0f
#define RN_MIST_CLEAR  0.2f     // ... and vanishes over this when a slider crosses it
#define RN_SPAWN0      0.06f    // a new drop every (0.06 .. 0.20 s) / max(0.5, 2 x strength)
#define RN_SPAWN1      0.14f

typedef unsigned int RnU32;
static RnU32 RnHash(RnU32 a)  // = FxHash
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float RnRnd(RnU32 a) { return (float)(RnHash(a) & 0xFFFFFF) / 16777216.0f; }
#define RN_R(s, n) RnRnd((s) * 0x9E3779B9u + (RnU32)(n) * 0x85EBCA6Bu + 0x1234567u)

typedef struct {
    int st;                  // 0 free, 1 sitting, 2 sliding, 3 fading out
    RnU32 ser;               // spawn serial (hash seed)
    int shape, bead;         // atlas cell 0..3; 1 = a stopped slider (never slides again)
    float x, y, r;           // centre, radius (px at 720 lines)
    int age, sit, life;      // steps since spawn, sit length, total life (sitting drops)
    float hd, hd0, spd, lx, ly;  // heading (0 = down, + to the right), start heading, speed, last trail point
    int slot, slotLeft, slip;    // stick-slip slot index, its steps left, slip or stick
    int trail;               // trail life in steps for its segments
} RnDrop;
typedef struct { float x0, y0, x1, y1, w; int born, life; RnU32 seed; int beaded; } RnSeg;
typedef struct { float x, y, r; int born, life; RnU32 seed; } RnBead;
typedef struct { int gen, appear, cleared; } RnMist;  // position = hash(index, gen); cleared = step it was wiped (-1 = no)
typedef struct {
    int step;                // steps so far
    long long ticks;         // rain time so far in clock ticks (INSP326-007: integer, so equal elapsed ticks give
                             // exactly the same step count: step target = ticks x RN_HZ / freq, integer division)
    RnU32 serial;            // drops spawned
    int spawnLeft;           // steps to the next spawn
    float aw;                // the width (720 x aspect)
    float dens;              // density factor max(0.5, 2 x strength) (sets the spawn interval)
    int segNext, beadNext;
    int segRefused, beadRefused;  // SVR-301: new entries not made because the ring's next slot was still alive
    RnDrop d[RN_DROPS];
    RnSeg seg[RN_SEGS];
    RnBead bead[RN_BEADS];
    RnMist mist[RN_MIST];
} RnState;

static void RnInit(RnState *s, float aw)
{
    int i;
    memset(s, 0, sizeof *s);
    s->aw = aw;
    s->dens = 1.0f;
    s->spawnLeft = 1;
    for (i = 0; i < RN_MIST; ++i) s->mist[i].cleared = -1;
}
static void RnMistPos(const RnState *s, int i, float *x, float *y, float *r)
{
    const RnU32 h = RnHash((RnU32)i * 0x632BE5ABu + (RnU32)s->mist[i].gen * 0x9E3779B9u + 0x3157u);
    *x = RnRnd(h) * s->aw;
    *y = RnRnd(h ^ 0x51u) * 720.0f;
    *r = 1.2f + 2.2f * RnRnd(h ^ 0xA7u);
}
static int RnSliders(const RnState *s)
{
    int i, n = 0;
    for (i = 0; i < RN_DROPS; ++i) n += s->d[i].st == 2;
    return n;
}
static void RnSegAdd(RnState *s, const RnDrop *d, float x0, float y0, float x1, float y1)
{
    RnSeg *g;  // admission (SVR-301 / 401): the first expired slot in ring order from segNext; none free: not made
    int k, i = -1;
    for (k = 0; k < RN_SEGS && i < 0; ++k) {
        const int j = (s->segNext + k) % RN_SEGS;
        if (s->seg[j].life <= 0 || s->step - s->seg[j].born >= s->seg[j].life) i = j;
    }
    if (i < 0) {
        ++s->segRefused;
        return;
    }
    g = &s->seg[i];
    s->segNext = (i + 1) % RN_SEGS;
    g->x0 = x0, g->y0 = y0, g->x1 = x1, g->y1 = y1;
    g->w = RN_TRAIL_W * d->r;
    g->born = s->step;
    g->life = d->trail;
    g->seed = RnHash(d->ser * 0x27D4EB2Fu + (RnU32)s->step);
    g->beaded = 0;
}
static void RnBeadAdd(RnState *s, float x, float y, float r, int life, RnU32 seed)
{
    RnBead *b;  // admission as the segments: an admitted bead always lives to its end
    int k, i = -1;
    for (k = 0; k < RN_BEADS && i < 0; ++k) {
        const int j = (s->beadNext + k) % RN_BEADS;
        if (s->bead[j].life <= 0 || s->step - s->bead[j].born >= s->bead[j].life) i = j;
    }
    if (i < 0) {
        ++s->beadRefused;
        return;
    }
    b = &s->bead[i];
    s->beadNext = (i + 1) % RN_BEADS;
    b->x = x, b->y = y, b->r = r, b->born = s->step, b->life = life, b->seed = seed;
}
static void RnSpawn(RnState *s)
{
    int i;
    for (i = 0; i < RN_DROPS && s->d[i].st; ++i) {}
    if (i < RN_DROPS) {
        RnDrop *d = &s->d[i];
        const RnU32 n = ++s->serial;
        const float u = RN_R(n, 2);
        memset(d, 0, sizeof *d);
        d->st = 1;
        d->ser = n;
        d->shape = (int)(RN_R(n, 9) * 4.0f) & 3;
        d->x = RN_R(n, 0) * s->aw;
        d->y = RN_R(n, 1) * 720.0f * 0.92f;
        d->r = RN_R0 + RN_R1 * u * u;
        d->sit = (int)((RN_SIT0 + RN_SIT1 * RN_R(n, 3)) * RN_HZ);
        d->life = d->sit + (int)((2.0f + 4.0f * RN_R(n, 4)) * RN_HZ);
        d->hd0 = d->hd = RN_ANGLE * (2.0f * RN_R(n, 5) - 1.0f);
        d->trail = (int)((RN_TRAIL0 + RN_TRAIL1 * RN_R(n, 6)) * RN_HZ);
    }
    s->spawnLeft = (int)((RN_SPAWN0 + RN_SPAWN1 * RN_R(s->serial + 0x9000u, 7)) / s->dens * RN_HZ);
    if (s->spawnLeft < 1) s->spawnLeft = 1;
}
static void RnSlideStep(RnState *s, RnDrop *d)
{
    const float dt = 1.0f / RN_HZ;
    float tgt, dx, dy, dist, seg;
    int j;
    if (d->slotLeft <= 0) {  // the next stick-slip slot
        const float u = RN_R(d->ser * 31u + (RnU32)d->slot, 11);
        ++d->slot;
        d->slotLeft = (int)((RN_SLOT0 + RN_SLOT1 * u) * RN_HZ);
        if (d->slotLeft < 1) d->slotLeft = 1;
        d->slip = d->slot == 1 || RN_R(d->ser * 31u + (RnU32)d->slot, 12) < RN_SLIP_P;  // the first slot always slips
    }
    --d->slotLeft;
    d->hd += RN_WALK * (2.0f * RN_R(d->ser ^ (RnU32)s->step * 0x2C1B3C6Du, 13) - 1.0f);
    d->hd += (d->hd0 - d->hd) * RN_PULL;
    if (d->slip) {
        tgt = (RN_SPD0 + RN_SPD1 * RN_R(d->ser * 31u + (RnU32)d->slot, 14)) * d->r / 10.0f;
        if (tgt > RN_SPD_MAX) tgt = RN_SPD_MAX;
        d->spd += (tgt - d->spd) * RN_SPD_EASE;
    } else {
        d->spd *= RN_STICK_DAMP;
    }
    d->x += sinf(d->hd) * d->spd * dt;
    d->y += cosf(d->hd) * d->spd * dt;
    dx = d->x - d->lx, dy = d->y - d->ly;
    dist = sqrtf(dx * dx + dy * dy);
    seg = d->spd / 30.0f > RN_SEG_MIN ? d->spd / 30.0f : RN_SEG_MIN;
    if (dist >= seg) {  // a trail segment; the drop leaves water behind
        RnSegAdd(s, d, d->lx, d->ly, d->x, d->y);
        d->r *= 1.0f - RN_SHRINK * dist / 100.0f;
        d->lx = d->x, d->ly = d->y;
    }
    for (j = 0; j < RN_DROPS; ++j) {  // merging: it swallows the drops it touches and speeds up
        RnDrop *o = &s->d[j];
        float ox, oy;
        if (o == d || !o->st || o->st == 3) continue;
        ox = o->x - d->x, oy = o->y - d->y;
        if (ox * ox + oy * oy < (d->r + 0.8f * o->r) * (d->r + 0.8f * o->r)) {
            d->r = sqrtf(d->r * d->r + o->r * o->r);
            if (d->r > RN_RMAX) d->r = RN_RMAX;
            d->spd *= 1.2f;
            o->st = 0;  // its own segments stay in the pool
        }
    }
    for (j = 0; j < RN_MIST; ++j) {  // it wipes the micro-drops in its path
        RnMist *m = &s->mist[j];
        float mx, my, mr;
        if (m->cleared >= 0) continue;
        RnMistPos(s, j, &mx, &my, &mr);
        if ((mx - d->x) * (mx - d->x) + (my - d->y) * (my - d->y) < d->r * d->r) m->cleared = s->step;
    }
    if (d->r < RN_BEAD_R) {  // too small to go on: a bead that sits and fades
        d->st = 1;
        d->bead = 1;
        d->age = 0;
        d->sit = 0;
        d->life = (int)((1.5f + 2.0f * RN_R(d->ser, 15)) * RN_HZ);
    } else if (d->y > 720.0f + d->r || d->x < -d->r || d->x > s->aw + d->r) {
        d->st = 0;  // off the screen
    }
}
// one fixed step of 1/120 s
static void RnStep(RnState *s)
{
    int i;
    if (--s->spawnLeft <= 0) RnSpawn(s);
    for (i = 0; i < RN_DROPS; ++i) {
        RnDrop *d = &s->d[i];
        if (!d->st) continue;
        ++d->age;
        if (d->st == 1 && !d->bead && d->age >= d->sit) {  // sat long enough: slide, or stay and fade later
            const float p = RN_SLIDE_P0 + RN_SLIDE_P1 * d->r / 14.0f;
            if (RN_R(d->ser, 8) < p && RnSliders(s) < RN_SLIDERS) {
                d->st = 2;
                d->lx = d->x, d->ly = d->y;
                d->slot = d->slotLeft = 0;
                d->spd = 0.0f;
            } else {
                d->bead = 1;  // decided: it stays (never re-tested)
            }
        }
        if (d->st == 2) RnSlideStep(s, d);
        else if (d->st == 1 && d->age >= d->life) d->st = 3, d->age = 0;  // fades out
        else if (d->st == 3 && d->age >= (int)(RN_LIFE_FADE * RN_HZ)) d->st = 0;
    }
    for (i = 0; i < RN_SEGS; ++i) {  // beads where a thinning trail breaks up
        RnSeg *g = &s->seg[i];
        if (g->life <= 0 || g->beaded || s->step - g->born < (int)(RN_BEAD_AT * (float)g->life)) continue;
        g->beaded = 1;
        if (RnRnd(g->seed) < RN_BEAD_P) {
            const float dx = g->x1 - g->x0, dy = g->y1 - g->y0, len = sqrtf(dx * dx + dy * dy);
            const float gap = RN_BEAD_GAP0 + RN_BEAD_GAP1 * RnRnd(g->seed ^ 0x77u), f = RnRnd(g->seed ^ 0x99u);
            if (len >= gap * f)
                RnBeadAdd(s, g->x0 + dx * f, g->y0 + dy * f, 1.8f + 2.0f * RnRnd(g->seed ^ 0x55u),
                          g->life - (s->step - g->born) + (int)(RN_BEAD_EXTRA * RnRnd(g->seed ^ 0x33u) * RN_HZ), g->seed);
        }
    }
    for (i = 0; i < RN_MIST; ++i) {  // wiped micro-drops come back later at a new spot
        RnMist *m = &s->mist[i];
        if (m->cleared >= 0 && s->step - m->cleared >= (int)((RN_MIST_BACK0 + RN_MIST_BACK1 * RN_R((RnU32)i, m->gen + 20)) * RN_HZ)) {
            ++m->gen;
            m->cleared = -1;
            m->appear = s->step;
        }
    }
    ++s->step;
}
// the frame's time: dticks clock ticks of rain (0 after a frame without the layer) at freq ticks a second (the game's
// QueryPerformanceFrequency); the state after it depends only on the total ticks so far (and the stall rule).
static int RnAdvance(RnState *s, long long dticks, long long freq, float aw, float strength)
{
    long long target;
    int n = 0;
    s->aw = aw;
    s->dens = 2.0f * strength > 0.5f ? 2.0f * strength : 0.5f;
    if (dticks <= 0 || freq <= 0) return 0;
    s->ticks += dticks;
    target = s->ticks * RN_HZ / freq;
    while (s->step < target && n < RN_MAX_STEPS) {
        RnStep(s);
        ++n;
    }
    if (s->step < target)  // a stall (> 0.5 s in one frame): the excess is dropped; ticks set so the target is the step
        s->ticks = ((long long)s->step * freq + RN_HZ - 1) / RN_HZ;
    return n;
}
#endif
