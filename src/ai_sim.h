// ================================================================ v3.29 AFTERIMAGE: the pure state code of the Volt Trail
// staging/special_mods/r3_afterimage/PLAN.md sections 2-3. Pure C (no D3D, no game memory): included by afterimage.inc
// and by the native test test/ai_sim_test.c. The pose ring is stamped in GAME seconds (Refined's own clock, advanced
// in the logic tick by 1 / [0x5D5248]) and sampled at most every 1/60 s, so the same echo lags read the same past poses
// at 30, 60, 120, 144, 180 or 240 frames a second; the per-enemy cooldown runs in the same seconds.
// v3.30 AS330: renamed After Shock; its row value is the trail length (Short / Medium / Long: AiTrailIndex), the nearest
// contact over every live echo is one pure function (AiBestContact: one stun per enemy whatever the echo count), and the
// frame-time guard steps the echo count down a ladder (AiCapStep: 8 -> 5 -> 3) instead of straight to 3.
// v3.32 AS332 (staging/v332_feedback/w2_aftershock/PLAN.md; the user's v3.31 test, item A): the copies sit evenly along
// the PATH she travelled over the trail's span (AiEchoAt: never two on one spot, never a gap at a turn, a stop-start or a
// capped count), shown by how far she has come since (AiGate: path, plus an overlap fade inside her body; v3.31 used the
// straight distance, which folds to 0 mid-trail on a turn); the lens gate is gone: a copy along the view axis is moved
// sideways (AiFanShift / AiFanSideStep) and fades only very near the lens relative to her distance (AiNearK); the aura's
// charge follows her speed (AiRingSpeed, AiChargeStep). Every time constant is an exact exponential in game seconds.
#ifndef AI_SIM_H
#define AI_SIM_H
#include <math.h>
#include <string.h>

#define AI_RING        64            // pose samples kept (>= 1.07 s at the 1/60 s spacing; the Long trail is 0.6 s: 36)
#define AI_SAMPLE_S    (1.0 / 60.0)  // a new sample once this much game time has passed since the last
#define AI_SAMPLE_EPS  1e-4          // (float steps: 2 x 1/120 must count as 1/60)
#define AI_GAP_S       0.25          // samples further apart than this: a break (the ring starts again)
#define AI_JUMP_U      300.0f        // her pelvis moved more than this between two samples: a teleport (ring reset)
#define AI_ECHO_MAX    8             // echoes at most (the Long trail; v3.30 AS330: 6 -> 8)
#define AI_TRAILS      3             // v3.30 AS330: Short / Medium / Long = the row's values 1..3 (was the hidden AfterImageTrail)
#define AI_KEYS        16            // the key bones of an echo (deathpresence_sprites.inc HB_N, kDpHerBone order)
#define AI_LIVE_K      0.35f         // an echo touches (stuns) only when its movement gate is at least this
#define AI_MOVE0       20.0f         // the movement gate: smoothstep(20, 70) of the echo's pelvis distance from hers
#define AI_MOVE1       70.0f
#define AI_LENS0       (-90.0f)      // the lens gate: an echo this much nearer the camera than she is is gone ...
#define AI_LENS1       (-10.0f)      // ... and full from this (over-the-shoulder camera: the trail runs at the lens)
#define AI_CD_S        3.0f          // after a stun: no new stun for this long ...
#define AI_UP_S        2.0f          // ... and not before the enemy has stood this long since it was last down / stunned
#define AI_FOES        48            // enemies tracked
#define AI_FOE_GAP_S   0.2f          // an enemy missing from the list this long was removed: its slot starts clean
#define AI_FOE_FORGET_S 10.0f        // an enemy not seen this long is forgotten
#define AI_STUNS_TICK  4             // stuns a tick at most
#define AI_CAP_R       18.0f         // a body capsule's radius (units)
#define AI_LINE_R      25.0f         // the trail line between consecutive echoes' pelvises
#define AI_DY          150.0f        // an enemy's centre must be within this height of a capsule's span
#define AI_STACK0      2.0f          // v3.30 AS330: an echo this near (pelvis, 3D) the last one drawn is not drawn ...
#define AI_STACK1      10.0f         // ... and full from this (a walk spaces Medium / Long echoes 12-13 u apart)
// v3.32 AS332
#define AI_PATH0       20.0f         // the path gate: smoothstep(20, 70) of the path she travelled (XZ) since the echo's moment
#define AI_PATH1       70.0f
#define AI_OVER0       10.0f         // the overlap fade: an echo whose pelvis is this near hers (XZ) is inside her body ...
#define AI_OVER1       30.0f         // ... and clear of it from this
#define AI_FAN_T       0.5f          // the lens fan: a copy d units along the view axis from her sits at least 0.5 d beside it
#define AI_FAN_TAU     0.2f          // the fan's side follows the trail's own side with this time constant (s) ...
#define AI_FAN_CLEAR   0.35f         // ... once the tail's true lateral part is this x the fan's (else the side holds)
#define AI_NEARR0      0.35f         // the near fade: a copy at a view depth under 0.35 x hers (2.9 x her size) is gone ...
#define AI_NEARR1      0.60f         // ... full from 0.60 x ...
#define AI_NEARA0      45.0f         // ... and never within 45 u of the lens (full from 100 u)
#define AI_NEARA1      100.0f
#define AI_STALE_S     0.05          // the ring's newest sample older than this: it stalled (no echoes until it fills again)
#define AI_CHARGE_IDLE 0.3f          // the aura's charge: 0.3 standing, 1 from 300 u/s (smoothstep from 40 u/s)
#define AI_CHARGE_V0   40.0f
#define AI_CHARGE_V1   300.0f
#define AI_CHARGE_UP   0.08f         // its time constants (s): it flares fast, settles slowly
#define AI_CHARGE_DN   0.45f
#define AI_SPEED_S     0.1           // her speed: the path of the last 0.1 s

typedef unsigned int AiU32;
typedef struct { int n; float span; } AiTrail;  // echoes, seconds from her to the last one
// v3.30 AS330 (the user: "trail lengths as its values"): Short 3 over 0.2 s, Medium 5 over 0.35 s (= v3.29's On), Long 8
// over 0.6 s (v3.29's hidden Short 4 / 0.24 and Long 6 / 0.6 are gone)
static const AiTrail kAiTrail[AI_TRAILS] = { { 3, 0.20f }, { 5, 0.35f }, { 8, 0.60f } };
// the row's value (0 Off, 1 Short, 2 Medium, 3 Long) -> its trail; -1 = off
static int AiTrailIndex(int level) { return level >= 1 && level <= AI_TRAILS ? level - 1 : -1; }
// the lag of echo k (1..n) of a trail: evenly spaced, the last one `span` behind her
static float AiLag(const AiTrail *t, int k) { return t->span * (float)k / (float)t->n; }

static AiU32 AiHash(AiU32 a)  // (the same mixer as dp_sim.h DpHash; own copy so this header stands alone)
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float AiRnd01(AiU32 a) { return (float)(AiHash(a) >> 8) * (1.0f / 16777216.0f); }  // 0 .. 1
static float AiSmooth(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return t * t * (3.0f - 2.0f * t);
}

// ---- the pose ring: stamps and her pelvis per slot (the bone matrices live beside it in afterimage.inc, same slot)
typedef struct {
    double stamp[AI_RING];
    float pelvis[AI_RING][3];
    int head;   // the next slot to write
    int count;  // valid samples (0 .. AI_RING)
} AiRing;
static void AiRingReset(AiRing *r) { r->head = 0, r->count = 0; }
static int AiRingNewest(const AiRing *r) { return (r->head + AI_RING - 1) % AI_RING; }
static int AiRingDue(const AiRing *r, double now)  // 1 = a sample is due at this game time
{
    return !r->count || now - r->stamp[AiRingNewest(r)] >= AI_SAMPLE_S - AI_SAMPLE_EPS;
}
// takes a sample at `now` with her pelvis there; the ring starts again first when the sample breaks the trail (time
// backwards, a gap over AI_GAP_S, a pelvis jump over AI_JUMP_U: a door, a teleport). Returns the slot to fill.
static int AiRingPush(AiRing *r, double now, const float pelvis[3], int *broke)
{
    int s;
    *broke = 0;
    if (r->count) {
        const int n = AiRingNewest(r);
        const float dx = pelvis[0] - r->pelvis[n][0], dy = pelvis[1] - r->pelvis[n][1], dz = pelvis[2] - r->pelvis[n][2];
        const double gap = now - r->stamp[n];
        if (!(gap >= 0.0) || gap > AI_GAP_S || !(dx * dx + dy * dy + dz * dz <= AI_JUMP_U * AI_JUMP_U)) {
            AiRingReset(r);
            *broke = 1;
        }
    }
    s = r->head;
    r->stamp[s] = now;
    memcpy(r->pelvis[s], pelvis, sizeof r->pelvis[s]);
    r->head = (s + 1) % AI_RING;
    if (r->count < AI_RING) ++r->count;
    return s;
}
// the two samples that bracket game time t and the blend between them (out = s0 + (s1 - s0) f). 0 = no echo: fewer
// than 2 samples, or t older than the oldest sample (the trail is still growing out of her). A t at or after the
// newest sample reads the newest one.
static int AiRingAt(const AiRing *r, double t, int *i0, int *i1, float *f)
{
    int k, newer;
    if (r->count < 2) return 0;
    newer = AiRingNewest(r);
    if (t >= r->stamp[newer]) {
        *i0 = *i1 = newer;
        *f = 0.0f;
        return 1;
    }
    for (k = 1; k < r->count; ++k) {
        const int s = (r->head + AI_RING - 1 - k) % AI_RING;
        if (r->stamp[s] <= t) {
            const double d = r->stamp[newer] - r->stamp[s];
            *i0 = s;
            *i1 = newer;
            *f = d > 1e-9 ? (float)((t - r->stamp[s]) / d) : 0.0f;
            return 1;
        }
        newer = s;
    }
    return 0;
}
static void AiLerp3(float o[3], const float a[3], const float b[3], float f)
{
    int j;
    for (j = 0; j < 3; ++j) o[j] = a[j] + (b[j] - a[j]) * f;
}

// ---- the look's per-echo factors
// the movement gate: an echo sitting inside her (she stands, turns on the spot, attacks in place) is not there
static float AiMoveK(const float echoPelvis[3], const float herPelvis[3])
{
    const float dx = echoPelvis[0] - herPelvis[0], dz = echoPelvis[2] - herPelvis[2];
    return AiSmooth(AI_MOVE0, AI_MOVE1, sqrtf(dx * dx + dz * dz));
}
// the lens gate (look only, never the contact): dEcho / dHer = the echo's / her pelvis distance from the camera. A trail
// running toward the camera would pile its echoes over her (the preview's OTS frame); side views and a trail running
// away from the camera (behind her) are untouched
static float AiLensK(float dEcho, float dHer) { return AiSmooth(AI_LENS0, AI_LENS1, dEcho - dHer); }
// v3.30 AS330: the stack fade: echoes on one spot (she stood, then leapt: Long's 0.6 s holds several copies of the
// standing pose) would add up to one over-bright copy; each is faded by its pelvis distance to the last echo drawn
static float AiStackK(const float p[3], const float last[3])
{
    const float dx = p[0] - last[0], dy = p[1] - last[1], dz = p[2] - last[2];
    return AiSmooth(AI_STACK0, AI_STACK1, sqrtf(dx * dx + dy * dy + dz * dz));
}
// the brightness fall along the chain: echo k of n (1 = nearest her)
static float AiFall(int k, int n) { return n > 0 ? powf(1.0f - (float)(k - 1) / (float)n, 1.4f) : 0.0f; }
// electric flicker of echo k at game time t: 0.8 + 0.2 x value noise at 24 Hz; a 6 % chance per 50 ms of a 1.5 x crack.
// Deterministic in t (the same at any frame rate), never a regular strobe.
static float AiFlicker(double t, int k)
{
    const double u = t * 24.0 + 17.3 * (double)k;
    const double fl = floor(u);
    const float fr = (float)(u - fl);
    const AiU32 i = (AiU32)(long long)fl, kk = (AiU32)k * 0x9E3779B9u;
    const float a = AiRnd01(i ^ kk), b = AiRnd01((i + 1u) ^ kk), s = fr * fr * (3.0f - 2.0f * fr);
    float v = 0.8f + 0.2f * (a + (b - a) * s);
    if (AiRnd01((AiU32)(long long)floor(t / 0.05) * 0x85EBCA6Bu ^ (kk + 0x51u)) < 0.06f) v *= 1.5f;
    return v;
}

// ---- contact: an enemy centre c (its work position) of radius rType against a capsule a-b of radius rCap.
// Horizontal distance from c to the segment's ground projection < rCap + rType, and c's height within AI_DY of the
// segment's span. On a hit, out = the capsule's axis point nearest c (the record's contact point). Y is down (only
// differences are used).
static float AiSegDistXZ(const float c[3], const float a[3], const float b[3], float *tOut)
{
    const float ex = b[0] - a[0], ez = b[2] - a[2], l2 = ex * ex + ez * ez;
    float t = l2 > 1e-6f ? ((c[0] - a[0]) * ex + (c[2] - a[2]) * ez) / l2 : 0.0f, dx, dz;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    dx = c[0] - (a[0] + ex * t);
    dz = c[2] - (a[2] + ez * t);
    if (tOut) *tOut = t;
    return sqrtf(dx * dx + dz * dz);
}
static int AiCapsuleHit(const float c[3], float rType, const float a[3], const float b[3], float rCap, float out[3], float *dOut)
{
    float t, d;
    const float lo = (a[1] < b[1] ? a[1] : b[1]) - AI_DY, hi = (a[1] > b[1] ? a[1] : b[1]) + AI_DY;
    if (!(c[1] >= lo && c[1] <= hi)) return 0;
    d = AiSegDistXZ(c, a, b, &t);
    if (!(d < rCap + rType)) return 0;
    AiLerp3(out, a, b, t);
    if (dOut) *dOut = d;
    return 1;
}
// a type's body radius for the contact (0 = never stunned: not an enemy, Wall Man / the Wall boss, Walter / the Bodies,
// scripted beings); ghosts 0x0B-0x0F, 0x11 are stunned only while standing (the caller's class gate)
static float AiTypeR(int type)
{
    switch (type) {
    case 0x02: return 50.0f;  // Mush
    case 0x03: return 35.0f;  // Hummer
    case 0x06: return 60.0f;  // Wheelchair
    case 0x07: return 50.0f;  // Gum Head
    case 0x08: return 70.0f;  // Twin Victim
    case 0x09: return 60.0f;  // Tremer
    case 0x0A: return 55.0f;  // Sniffer Dog
    case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x11: return 40.0f;  // ghosts
    default: return 0.0f;
    }
}

// ---- the per-enemy cooldown (keyed by task + work: pool reuse = a new being, as dp_sim.h DpFind)
typedef struct {
    const void *task, *work;
    double seen;   // the table clock when last stepped
    float cd;      // seconds until the next stun is allowed (runs always)
    float upT;     // seconds on its feet since it was last down or stunned
    int stuns;
} AiFoe;
typedef struct { AiFoe f[AI_FOES]; double clock; int drops; } AiFoes;
static void AiFoesReset(AiFoes *t) { memset(t->f, 0, sizeof t->f); }
static AiFoe *AiFoeFind(AiFoes *t, const void *task, const void *work)
{
    int i, freeI = -1, oldI = 0;
    if (!task) return NULL;
    for (i = 0; i < AI_FOES; ++i) {
        AiFoe *f = &t->f[i];
        if (f->task == task) {
            if (f->work == work && t->clock - f->seen <= AI_FOE_GAP_S) {
                f->seen = t->clock;
                return f;
            }
            freeI = i;  // the task was reused for another being (or removed and re-made): a clean slot
            break;
        }
        if (f->task && t->clock - f->seen > AI_FOE_FORGET_S) memset(f, 0, sizeof *f);
        if (!f->task && freeI < 0) freeI = i;
        if (f->seen < t->f[oldI].seen) oldI = i;
    }
    if (freeI < 0) {
        freeI = oldI;
        ++t->drops;
    }
    {
        AiFoe *f = &t->f[freeI];
        memset(f, 0, sizeof *f);
        f->task = task;
        f->work = work;
        f->seen = t->clock;
        f->upT = AI_UP_S;  // a new being may be stunned at once
        return f;
    }
}
// one tick of dt seconds; down = class 9 / 0x0A this tick. The cooldown runs always; the time on its feet restarts
// while it is down, so a knocked-down enemy gets AI_UP_S of action after it rises before the next stun (no stun-lock)
static void AiFoeStep(AiFoe *f, int down, float dt)
{
    if (!(dt > 0.0f)) return;
    if (f->cd > 0.0f) f->cd -= dt;
    if (down) f->upT = 0.0f;
    else if (f->upT < 1.0e6f) f->upT += dt;
}
static int AiFoeReady(const AiFoe *f) { return !(f->cd > 0.0f) && f->upT >= AI_UP_S - 1e-4f; }
static void AiFoeStunned(AiFoe *f)
{
    f->cd = AI_CD_S;
    f->upT = 0.0f;
    ++f->stuns;
}

// ---- v3.30 AS330: the nearest contact of an enemy centre c (radius r) over the live echoes' body capsules (cap: ncap
// pairs of key bones) and the trail lines between consecutive live echoes' pelvises. One answer per enemy and tick
// (the echo it came from, or -1), whatever the echo count: more echoes can only find a contact sooner, never a second
// stun (the caller stuns once, then the cooldown above holds).
static int AiBestContact(const float c[3], float r, const float (*eb)[AI_KEYS][3], const int *live, int n,
                         const unsigned char (*cap)[2], int ncap, int pelvis, float at[3], float *best)
{
    int k, j, hitK = -1;
    float p[3], d;
    *best = 1.0e9f;
    for (k = 0; k < n; ++k) {
        if (!live[k]) continue;
        for (j = 0; j < ncap; ++j)
            if (AiCapsuleHit(c, r, eb[k][cap[j][0]], eb[k][cap[j][1]], AI_CAP_R, p, &d) && d < *best)
                *best = d, hitK = k, memcpy(at, p, 12);
        if (k + 1 < n && live[k + 1] && AiCapsuleHit(c, r, eb[k][pelvis], eb[k + 1][pelvis], AI_LINE_R, p, &d) && d < *best)
            *best = d, hitK = k, memcpy(at, p, 12);
    }
    return hitK;
}
// ---- v3.30 AS330: the frame-time guard's ladder: after 30 frames in a row over the cap the echoes drawn step down
// 8 -> 5 -> 3 (a capped trail keeps its length: the echoes are spread over it); 3 is the floor (v3.29: straight to 3)
static int AiCapStep(int cap) { return cap > 5 ? 5 : 3; }
// the drawn echo kk (1..T->n) of the k-th of n drawn ones (n <= T->n): spread evenly, the last one at the tail
// (v3.31; v3.32 AS332 no longer uses it: a capped 5 -> 3 trail drew echoes 2, 4, 5 = a gap at her and two copies together,
// the "broken trail" of the user's Long test, whose log stepped 8 -> 5 -> 3 within 0.5 s under dgVoodoo2)
static int AiSpreadEcho(int k, int n, int total) { return n >= total ? k : (k * total + n - 1) / n; }

// ---- v3.32 AS332: the path, the copies along it, the gates, the lens fan, the near fade, the aura's charge
static float AiDistXZ(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dz = a[2] - b[2];
    return sqrtf(dx * dx + dz * dz);
}
// the path she travelled back to each ring slot (XZ): cum[slot] = her pelvis -> the newest sample -> ... -> that slot
static void AiRingCum(const AiRing *r, const float her[3], float cum[AI_RING])
{
    int k, prev;
    float acc;
    if (!r->count) return;
    prev = AiRingNewest(r);
    acc = AiDistXZ(her, r->pelvis[prev]);
    cum[prev] = acc;
    for (k = 1; k < r->count; ++k) {
        const int s = (r->head + AI_RING - 1 - k) % AI_RING;
        acc += AiDistXZ(r->pelvis[s], r->pelvis[prev]);
        cum[s] = acc;
        prev = s;
    }
}
// the path back to game time t (AiRingAt's bracket); -1 = no sample there
static float AiPathAt(const AiRing *r, const float cum[AI_RING], double t)
{
    int i0, i1;
    float f;
    if (!AiRingAt(r, t, &i0, &i1, &f)) return -1.0f;
    return cum[i1] + (cum[i0] - cum[i1]) * (1.0f - f);
}
// 1 = the ring is being filled (its newest sample is at most AI_STALE_S old)
static int AiRingFresh(const AiRing *r, double now) { return r->count >= 2 && now - r->stamp[AiRingNewest(r)] <= AI_STALE_S; }
// the moment of copy k (1..n) of a trail of `span` seconds: the copies sit evenly along the path she travelled in that time
// (L = the path back to now - span, or to the oldest sample while the ring is younger), copy k at k / n of L. At a steady
// speed this is the v3.31 time spacing; a pause, a turn, a stop-start or a capped count can no longer pile copies on one
// spot or leave a gap. 0 = no copy (fewer than 2 samples). *path = its path distance from her, *t = its game time.
static int AiEchoAt(const AiRing *r, const float cum[AI_RING], double now, float span, int k, int n, double *t, float *path)
{
    int j, prev;
    float L, want;
    if (r->count < 2 || n < 1) return 0;
    L = AiPathAt(r, cum, now - span);
    if (L < 0.0f) L = cum[(r->head + AI_RING - r->count) % AI_RING];  // the oldest sample (the trail grows out of her)
    want = L * (float)k / (float)n;
    prev = AiRingNewest(r);
    if (want <= cum[prev]) {  // between her and the newest sample
        *t = r->stamp[prev];
        *path = cum[prev];
        return 1;
    }
    for (j = 1; j < r->count; ++j) {
        const int s = (r->head + AI_RING - 1 - j) % AI_RING;
        if (cum[s] >= want) {  // the most recent moment she was this far back along her path
            const float seg = cum[s] - cum[prev], f = seg > 1e-4f ? (want - cum[prev]) / seg : 0.0f;
            *t = r->stamp[prev] + (r->stamp[s] - r->stamp[prev]) * (double)f;
            *path = want;
            return 1;
        }
        prev = s;
    }
    *t = r->stamp[prev];  // (float rounding at the far end: the oldest sample)
    *path = cum[prev];
    return 1;
}
// the gate of a copy (look and contact): how far she has come along her path since, times the overlap fade (a copy whose
// pelvis is inside her body, e.g. the fold of an about-turn, adds nothing but glare)
static float AiGate(float path, float chordXZ) { return AiSmooth(AI_PATH0, AI_PATH1, path) * AiSmooth(AI_OVER0, AI_OVER1, chordXZ); }
// the near fade from view depths (camera forward): relative to her own depth (a copy over 2.9 x her size on screen is
// gone) and never right at the lens; a copy behind the camera (dEcho <= 0) is gone
static float AiNearK(float dEcho, float dHer) { return AiSmooth(AI_NEARR0 * dHer, AI_NEARR1 * dHer, dEcho) * AiSmooth(AI_NEARA0, AI_NEARA1, dEcho); }
// the lens fan: a copy at `along` (view axis, from her) and `lat` (across it) is moved across by the returned amount so it
// sits at least AI_FAN_T x |along| beside the axis on the fan's side (side -1 .. 1; |side| < 1 while it swings over). A
// side view (along 0) or a copy already beside the axis is untouched; a copy far on the other side (an S-bend) stays
static float AiFanShift(float along, float lat, float side)
{
    const float m = AI_FAN_T * fabsf(along), x = (side >= 0.0f ? 1.0f : -1.0f) * lat;
    if (!(m > 1e-3f) || x >= m) return 0.0f;
    return side * (m - x) * AiSmooth(-1.5f * m, -0.5f * m, x);
}
static float AiExpK(float dt, float tau) { return dt > 0.0f && tau > 0.0f ? 1.0f - expf(-dt / tau) : 0.0f; }
// the fan's side: it follows the trail tail's own (true) side once that is clear, else it holds (no flapping on a straight
// run away from the camera); exact exponential in game time
static float AiFanSideStep(float side, float latTail, float alongTail, float dt)
{
    float target = side >= 0.0f ? 1.0f : -1.0f;
    if (fabsf(latTail) > 4.0f && fabsf(latTail) > AI_FAN_CLEAR * AI_FAN_T * fabsf(alongTail)) target = latTail > 0.0f ? 1.0f : -1.0f;
    return side + (target - side) * AiExpK(dt, AI_FAN_TAU);
}
// her speed (u/s): the path of the last AI_SPEED_S; 0 while the ring is younger
static float AiRingSpeed(const AiRing *r, const float cum[AI_RING], double now)
{
    const float p = AiPathAt(r, cum, now - AI_SPEED_S);
    return p >= 0.0f ? p / (float)AI_SPEED_S : 0.0f;
}
// the aura's charge (0.3 standing .. 1 at a run): up fast, down slowly
static float AiChargeStep(float c, float speed, float dt)
{
    const float target = AI_CHARGE_IDLE + (1.0f - AI_CHARGE_IDLE) * AiSmooth(AI_CHARGE_V0, AI_CHARGE_V1, speed);
    return c + (target - c) * AiExpK(dt, target > c ? AI_CHARGE_UP : AI_CHARGE_DN);
}

// ---- v3.33 AS333 (staging/v333_feedback/w2_aftershock/PLAN.md, DESIGN.md; the user's v3.32 test, items 4 and 6)
// The lens fan's width from the hidden [Gameplay] AfterImageFan (%): v3.33 default 0 = the copies stay in line along her
// path in every camera ("do not fan them out to the side"); 1..200 = v3.32's fan x pct / 100 (100 = v3.32 exactly)
#define AI_FAN_DEFAULT 0
static float AiFanWidthK(int pct) { return (float)(pct < 0 ? 0 : pct > 200 ? 200 : pct) / 100.0f; }
// the aura's level for the log: 0 standing, 1 walking, 2 running from the charge (0.3 standing, ~0.67 at a walk of 175 u/s,
// 1 at a run), with a 0.04 hysteresis so a charge sitting on an edge does not flap
#define AI_BAND_WALK   0.45f
#define AI_BAND_RUN    0.85f
#define AI_BAND_HYST   0.04f
static int AiAuraBand(int prev, float c)
{
    int b = c >= AI_BAND_RUN ? 2 : c >= AI_BAND_WALK ? 1 : 0;
    if (prev >= 0 && prev <= 2 && b != prev) {
        const float edgeUp = b == 2 ? AI_BAND_RUN : AI_BAND_WALK, edgeDn = b == 0 ? AI_BAND_WALK : AI_BAND_RUN;
        if (b > prev ? c < edgeUp + AI_BAND_HYST : c > edgeDn - AI_BAND_HYST) b = prev;
    }
    return b;
}
// the frame guard's watch: the trail held under its setting since game time t0 (-1 = not held); 1 once per visit when that
// has lasted AI_CUT_S (the caller writes the ANOMALY line). Game seconds: the same moment at any frame rate.
#define AI_CUT_S 5.0
typedef struct { double t0; int fired; } AiCutW;
static void AiCutReset(AiCutW *w) { w->t0 = -1.0, w->fired = 0; }
static int AiCutWatch(AiCutW *w, int held, double now)
{
    if (!held) { w->t0 = -1.0; return 0; }
    if (w->t0 < 0.0) w->t0 = now;
    if (w->fired || now - w->t0 < AI_CUT_S - 1e-6) return 0;
    w->fired = 1;
    return 1;
}

// ---- v3.33 AS333: the cloak. One skin-tight electric-blue cloak hugging her body (replaces v3.32's corona / bloom sprites):
// six chains of points along her bones (the spine from the hips to the top of the head, the shoulder line, both arms to the
// fingertips, both legs to the toe tips), each drawn as a soft camera-facing fill as wide as that part of her body (+30 %:
// the glow spills just past her outline) and two thin crackling edge lines exactly on her outline (the shoulder line: its
// top edge only). The widths come from the ACTIVE body: two measured presets (Playable Eileen's and vanilla Henry's meshes,
// BUILD_NOTES "size facts") blended by her hip-bone spacing (rigid: Eileen 24.1 u, Henry 36.0 u) and scaled by her bone
// lengths; the torso is an ellipse (width x depth) seen from the camera. Pure maths: the caller draws the ribbons.
// v3.34 FX334 (staging/v334_feedback/w2_fx/DESIGN.md; the user's v3.33 QA item 6, "remove ... just the hard edges", the
// screenshot: thin bright lines along her arms, legs and sides): those lines were the edge lines themselves, two per chain
// (a 0.8 u core drawn at least 0.75 px wide plus a 2.8 u glow, additive, 0.20 standing .. 0.72 at a run) laid exactly on her
// outline. Not a shell, not grazing-angle blending, not a second pass of her body: the cloak is camera-facing strips. v3.34
// keeps the soft fill only and makes it END at her silhouette: half width 1.10 x her half extent + 0.3 u (v3.33: 1.3 x + 2 u,
// a glow spilling past her outline), so the strand profile (1 - smoothstep(0.15, 1, |u|)) is 3-7 % at her outline (fingers to
// torso) and 0 just past it; both end points of every chain carry no light (no square strip end at the crown, fingertips, toes, shoulders).
// The outline points stay (kind 1, no light): Thermal's white-hot points still crawl along them. The fill is a little
// stronger and its crawling light more marked (the current the edges used to carry). The lightning (arcs, tendrils,
// strikes) is unchanged.
// v3.35 FX335 (the user's v3.34 QA: "remove the blue glow from the player on After Shock"): afterimage.inc no longer draws
// the fill either and builds the cloak only under Thermal (its outline points: the hot points); every other look measures
// her body with AiBodyMeasure alone (the arcs' girth; test section 9: the same body as the build's). This code is unchanged.
enum { AIB_HEAD, AIB_NECK, AIB_CHEST, AIB_PELVIS, AIB_LSH, AIB_RSH, AIB_LELB, AIB_RELB, AIB_LHAND, AIB_RHAND, AIB_LHIP, AIB_RHIP,
       AIB_LKNEE, AIB_RKNEE, AIB_LFOOT, AIB_RFOOT, AIB_LTOE, AIB_RTOE, AIB_N };  // = deathpresence_sprites.inc HB_* + toes 24 / 22
#define AI_CK_HIPS_E   24.1f   // hip bones (5 / 2) apart: Playable Eileen's 36-bone body ...
#define AI_CK_HIPS_H   36.0f   // ... vanilla Henry's 30-bone body (bind; rigid under animation)
#define AI_CK_LEN_E    249.5f  // pelvis -> chest -> neck -> head + hip -> knee -> ankle: Eileen 249.5 u ...
#define AI_CK_LEN_H    261.2f  // ... Henry 261.2 u
#define AI_CK_CH       6       // chains
#define AI_CK_CTL      10      // control points a chain at most
#define AI_CK_PTS      19      // points a chain after subdivision (2 per segment)
#define AI_CK_RIBS     32      // ribbons at most (v3.34 FX334: 6 fills + 11 outlines = 17, only the fills drawn; v3.33: + 11 glows = 28)
#define AI_CK_FILL_W   1.10f   // v3.34 FX334: the fill's half width = 1.10 x the body's half extent + 0.3 u: the strand profile is
#define AI_CK_FILL_PAD 0.3f    //   3-7 % at her outline, 0 just past it (v3.33: 1.3 x + 2 u, its glow reached past her outline)
#define AI_CK_OUT_W    0.8f    // v3.34 FX334: an outline's nominal half width (u; never drawn: Thermal's hot points only)
#define AI_CK_BAND     70.0f   // the crawling light: a band every 70 u along a chain ...
#define AI_CK_CRAWL    2.2f    // ... moving 2.2 bands a second (154 u/s)
#define AI_CK_F0       0.030f  // v3.34 FX334: the fill's strength 0.03 + 0.33 c^1.5 (standing 0.08, walking 0.21, running 0.36;
#define AI_CK_F1       0.33f   //   v3.33: 0.06 / 0.16 / 0.27 beside edge lines of 0.20 / 0.44 / 0.72)
#define AI_CK_BAND_K   0.60f   // v3.34 FX334: the crawling light in the fill x (0.75 + 0.6 band) (v3.33: 0.85 + 0.3 band)
// a control point: p = bone a + (bone b - bone a) f, then (ext 1) + unit(b - a) x X, or (ext 2) + her foot's horizontal
// forward x X (a toe tip); half extents W (across her body: lat 1 the hip line, 2 the shoulder line, 0 round) and D (front to
// back), Eileen [0] / Henry [1]. W < 0: the hips' half spacing + the thigh's radius (the spine's bottom meets the legs' edges)
typedef struct { signed char a, b; float f; signed char ext; float X[2], W[2], D[2]; signed char lat; } AiCkCtl;
typedef struct { signed char n, edges, ch; float kMul; AiCkCtl c[AI_CK_CTL]; } AiCkChain;  // edges: 2 both, 1 the top one
static const AiCkChain kAiCk[AI_CK_CH] = {
    { 10, 2, 0, 0.80f, {  // the spine: hips -> pelvis -> waist -> chest -> upper chest -> neck -> chin -> head -> crown -> top
        { AIB_LHIP, AIB_RHIP, 0.5f, 0, { 0, 0 }, { -1, -1 }, { 21, 27 }, 1 },
        { AIB_PELVIS, AIB_PELVIS, 0, 0, { 0, 0 }, { 28, 34 }, { 20, 27 }, 1 },
        { AIB_PELVIS, AIB_CHEST, 0.5f, 0, { 0, 0 }, { 24, 34 }, { 18, 27 }, 1 },
        { AIB_CHEST, AIB_CHEST, 0, 0, { 0, 0 }, { 25, 40 }, { 21, 26 }, 2 },
        { AIB_CHEST, AIB_NECK, 0.55f, 0, { 0, 0 }, { 22, 34 }, { 17, 23 }, 2 },
        { AIB_NECK, AIB_NECK, 0, 0, { 0, 0 }, { 10, 15.5f }, { 10.5f, 15.5f }, 2 },
        { AIB_NECK, AIB_HEAD, 0.35f, 0, { 0, 0 }, { 16, 17 }, { 18, 20 }, 2 },
        { AIB_NECK, AIB_HEAD, 1.0f, 0, { 0, 0 }, { 19, 19.5f }, { 22, 25 }, 2 },
        { AIB_NECK, AIB_HEAD, 1.0f, 1, { 25, 34 }, { 12, 13 }, { 14, 15 }, 2 },
        { AIB_NECK, AIB_HEAD, 1.0f, 1, { 33, 44 }, { 5, 5.5f }, { 6, 7 }, 2 } } },  // (a tapered dome: the ribbon ends square)
    { 3, 1, 1, 0.80f, {  // the shoulder line: left shoulder -> neck base -> right shoulder (its top edge only)
        { AIB_LSH, AIB_LSH, 0, 0, { 0, 0 }, { 11, 17.5f }, { 11, 17.5f }, 0 },
        { AIB_CHEST, AIB_NECK, 0.55f, 0, { 0, 0 }, { 13, 19 }, { 13, 19 }, 0 },
        { AIB_RSH, AIB_RSH, 0, 0, { 0, 0 }, { 11, 17.5f }, { 11, 17.5f }, 0 } } },
    { 6, 2, 2, 1.0f, {  // left arm: over the shoulder -> shoulder -> elbow -> wrist -> mid-hand -> fingertips
        { AIB_LELB, AIB_LSH, 1.0f, 1, { 5, 9 }, { 8, 14 }, { 8, 14 }, 0 },
        { AIB_LSH, AIB_LSH, 0, 0, { 0, 0 }, { 9.5f, 17 }, { 9.5f, 17 }, 0 },
        { AIB_LELB, AIB_LELB, 0, 0, { 0, 0 }, { 8, 14.5f }, { 8, 14.5f }, 0 },
        { AIB_LHAND, AIB_LHAND, 0, 0, { 0, 0 }, { 6.5f, 12.5f }, { 6.5f, 12.5f }, 0 },
        { AIB_LELB, AIB_LHAND, 1.0f, 1, { 14, 14 }, { 7, 10.5f }, { 7, 10.5f }, 0 },
        { AIB_LELB, AIB_LHAND, 1.0f, 1, { 27, 27 }, { 4, 5.5f }, { 4, 5.5f }, 0 } } },
    { 6, 2, 3, 1.0f, {  // right arm
        { AIB_RELB, AIB_RSH, 1.0f, 1, { 5, 9 }, { 8, 14 }, { 8, 14 }, 0 },
        { AIB_RSH, AIB_RSH, 0, 0, { 0, 0 }, { 9.5f, 17 }, { 9.5f, 17 }, 0 },
        { AIB_RELB, AIB_RELB, 0, 0, { 0, 0 }, { 8, 14.5f }, { 8, 14.5f }, 0 },
        { AIB_RHAND, AIB_RHAND, 0, 0, { 0, 0 }, { 6.5f, 12.5f }, { 6.5f, 12.5f }, 0 },
        { AIB_RELB, AIB_RHAND, 1.0f, 1, { 14, 14 }, { 7, 10.5f }, { 7, 10.5f }, 0 },
        { AIB_RELB, AIB_RHAND, 1.0f, 1, { 27, 27 }, { 4, 5.5f }, { 4, 5.5f }, 0 } } },
    { 7, 2, 4, 1.0f, {  // left leg: hip -> thigh -> knee -> calf -> ankle -> toe joint -> toe tip
        { AIB_LHIP, AIB_LHIP, 0, 0, { 0, 0 }, { 17, 21 }, { 17, 21 }, 0 },
        { AIB_LHIP, AIB_LKNEE, 0.5f, 0, { 0, 0 }, { 14, 19.5f }, { 14, 19.5f }, 0 },
        { AIB_LKNEE, AIB_LKNEE, 0, 0, { 0, 0 }, { 11, 18 }, { 11, 18 }, 0 },
        { AIB_LKNEE, AIB_LFOOT, 0.5f, 0, { 0, 0 }, { 10, 18 }, { 10, 18 }, 0 },
        { AIB_LFOOT, AIB_LFOOT, 0, 0, { 0, 0 }, { 8.5f, 15.5f }, { 8.5f, 15.5f }, 0 },
        { AIB_LTOE, AIB_LTOE, 0, 0, { 0, 0 }, { 8, 11 }, { 8, 11 }, 0 },
        { AIB_LFOOT, AIB_LTOE, 1.0f, 2, { 24, 22 }, { 5, 7 }, { 5, 7 }, 0 } } },
    { 7, 2, 5, 1.0f, {  // right leg
        { AIB_RHIP, AIB_RHIP, 0, 0, { 0, 0 }, { 17, 21 }, { 17, 21 }, 0 },
        { AIB_RHIP, AIB_RKNEE, 0.5f, 0, { 0, 0 }, { 14, 19.5f }, { 14, 19.5f }, 0 },
        { AIB_RKNEE, AIB_RKNEE, 0, 0, { 0, 0 }, { 11, 18 }, { 11, 18 }, 0 },
        { AIB_RKNEE, AIB_RFOOT, 0.5f, 0, { 0, 0 }, { 10, 18 }, { 10, 18 }, 0 },
        { AIB_RFOOT, AIB_RFOOT, 0, 0, { 0, 0 }, { 8.5f, 15.5f }, { 8.5f, 15.5f }, 0 },
        { AIB_RTOE, AIB_RTOE, 0, 0, { 0, 0 }, { 8, 11 }, { 8, 11 }, 0 },
        { AIB_RFOOT, AIB_RTOE, 1.0f, 2, { 24, 22 }, { 5, 7 }, { 5, 7 }, 0 } } },
};
typedef struct {
    float build;     // 0 = Playable Eileen's body, 1 = vanilla Henry's (from the hip spacing; -0.25 .. 1.25)
    float lenScale;  // her bone lengths / Eileen's (Henry 1.047)
    float sizeK;     // the radii x this (the presets exact for the two bodies; any other body scaled by its lengths)
    float hips;      // her hip bones apart (u)
} AiBody;
static float AiLen3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
static int AiFinite3(const float p[3]) { return p[0] == p[0] && p[1] == p[1] && p[2] == p[2] && fabsf(p[0]) < 1e7f && fabsf(p[1]) < 1e7f && fabsf(p[2]) < 1e7f; }
// her body's size from her bones (lengths and the hip spacing are rigid: the same in every pose); 0 = not a usable skeleton
static int AiBodyMeasure(const float bn[AIB_N][3], AiBody *o)
{
    int i;
    float len, b;
    for (i = 0; i < AIB_LTOE; ++i)
        if (!AiFinite3(bn[i])) return 0;
    len = AiLen3(bn[AIB_PELVIS], bn[AIB_CHEST]) + AiLen3(bn[AIB_CHEST], bn[AIB_NECK]) + AiLen3(bn[AIB_NECK], bn[AIB_HEAD]) +
          0.5f * (AiLen3(bn[AIB_LHIP], bn[AIB_LKNEE]) + AiLen3(bn[AIB_LKNEE], bn[AIB_LFOOT]) + AiLen3(bn[AIB_RHIP], bn[AIB_RKNEE]) +
                  AiLen3(bn[AIB_RKNEE], bn[AIB_RFOOT]));
    o->hips = AiLen3(bn[AIB_LHIP], bn[AIB_RHIP]);
    if (!(len > 60.0f && len < 1200.0f) || !(o->hips > 2.0f && o->hips < 200.0f)) return 0;
    b = (o->hips - AI_CK_HIPS_E) / (AI_CK_HIPS_H - AI_CK_HIPS_E);
    o->build = b < -0.25f ? -0.25f : b > 1.25f ? 1.25f : b;
    o->lenScale = len / AI_CK_LEN_E;
    b = o->build < 0.0f ? 0.0f : o->build > 1.0f ? 1.0f : o->build;
    o->sizeK = o->lenScale / (1.0f + (AI_CK_LEN_H / AI_CK_LEN_E - 1.0f) * b);
    if (o->sizeK < 0.5f) o->sizeK = 0.5f;
    if (o->sizeK > 2.0f) o->sizeK = 2.0f;
    return 1;
}
static float AiCkLerp(const float v[2], float b) { return v[0] + (v[1] - v[0]) * b; }
static void AiNorm3(float v[3], float fbx, float fby, float fbz)
{
    const float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-5f) v[0] /= l, v[1] /= l, v[2] /= l;
    else v[0] = fbx, v[1] = fby, v[2] = fbz;
}
// the half extent of an ellipse (w across the lateral axis X, d front to back) seen along / across unit direction u
static float AiCkExtent(float w, float d, const float X[3], const float u[3], int lat)
{
    float x;
    if (!lat || w == d) return w;
    x = u[0] * X[0] + u[1] * X[1] + u[2] * X[2];
    x *= x;
    if (x > 1.0f) x = 1.0f;
    return sqrtf(w * w * x + d * d * (1.0f - x));
}
// one ribbon of the cloak: points, half widths (world u), strengths; kind 0 fill (drawn), 1 her outline (v3.34 FX334: points
// only, strength 0, never drawn: Thermal's hot points; v3.33 a drawn edge core, and kind 2 its glow); ch its chain
typedef struct { int n, kind, ch; float P[AI_CK_PTS][3], W[AI_CK_PTS], K[AI_CK_PTS]; } AiCkRib;
typedef struct {
    int nRib, nPts;
    int nFill;       // v3.34 FX334: the drawn ribbons (the fills, 6)
    AiCkRib r[AI_CK_RIBS];
    AiBody body;
} AiCloak;
// builds the cloak for her bones bn (toesOk 0: the toe joints are estimated from the shin), the eye (eyeOk 0: viewed along +Z),
// the aura's charge c (0.3 standing .. 1) and game time t (the crackle re-rolls every 50 ms, the light crawls: the same picture
// at any frame rate). Returns the ribbon count (0: a bone is not finite or the skeleton is not usable).
static int AiCloakBuild(const float bnIn[AIB_N][3], int toesOk, const float eye[3], int eyeOk, float c, double t, AiCloak *o)
{
    float bn[AIB_N][3], Xh[3], Xs[3];
    const AiU32 e50 = (AiU32)(long long)floor(t / 0.05);
    float b, g, kf, cc;
    int ch, i, j, s;
    o->nRib = o->nPts = o->nFill = 0;  // v3.34 FX334: + nFill
    memcpy(bn, bnIn, sizeof bn);
    if (!AiBodyMeasure(bn, &o->body)) return 0;
    for (s = 0; s < 2; ++s) {  // the toe joints: given, or 0.6 of the way from the ankle to the floor and a little forward
        const int toe = s ? AIB_RTOE : AIB_LTOE, foot = s ? AIB_RFOOT : AIB_LFOOT, knee = s ? AIB_RKNEE : AIB_LKNEE;
        if (!toesOk || !AiFinite3(bn[toe]) || AiLen3(bn[toe], bn[foot]) > 120.0f) {
            float d[3];
            for (j = 0; j < 3; ++j) d[j] = bn[foot][j] - bn[knee][j];
            AiNorm3(d, 0.0f, 1.0f, 0.0f);
            for (j = 0; j < 3; ++j) bn[toe][j] = bn[foot][j] + d[j] * 18.0f;
        }
    }
    b = o->body.build;  // (the presets extrapolate a little past either body: -0.25 .. 1.25)
    g = o->body.sizeK;
    for (j = 0; j < 3; ++j) Xh[j] = bn[AIB_RHIP][j] - bn[AIB_LHIP][j], Xs[j] = bn[AIB_RSH][j] - bn[AIB_LSH][j];
    AiNorm3(Xh, 1.0f, 0.0f, 0.0f);
    AiNorm3(Xs, Xh[0], Xh[1], Xh[2]);
    cc = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
    cc = cc * sqrtf(cc);
    kf = AI_CK_F0 + AI_CK_F1 * cc;  // (v3.34 FX334: no edge strength: the edges are not drawn)
    for (ch = 0; ch < AI_CK_CH; ++ch) {
        const AiCkChain *C = &kAiCk[ch];
        float cp[AI_CK_CTL][3], cw[AI_CK_CTL], cd[AI_CK_CTL];
        signed char cl[AI_CK_CTL];
        float P[AI_CK_PTS][3], W[AI_CK_PTS], D[AI_CK_PTS], L[AI_CK_PTS], A[AI_CK_PTS][3], R[AI_CK_PTS], Rv[AI_CK_PTS];
        signed char lat[AI_CK_PTS];
        int n = 0;
        float fl;
        for (i = 0; i < C->n; ++i) {
            const AiCkCtl *k = &C->c[i];
            const float *pa = bn[k->a], *pb = bn[k->b];
            for (j = 0; j < 3; ++j) cp[i][j] = pa[j] + (pb[j] - pa[j]) * k->f;
            if (k->ext == 1) {
                float d[3];
                for (j = 0; j < 3; ++j) d[j] = pb[j] - pa[j];
                AiNorm3(d, 0.0f, -1.0f, 0.0f);
                for (j = 0; j < 3; ++j) cp[i][j] += d[j] * AiCkLerp(k->X, b) * g;
            } else if (k->ext == 2) {  // a toe tip: the foot's horizontal forward (the toe joint seen from the ankle), at the sole
                float d[3] = { pb[0] - pa[0], 0.0f, pb[2] - pa[2] };
                const float l = sqrtf(d[0] * d[0] + d[2] * d[2]);
                if (l > 1.0f) {
                    d[0] /= l, d[2] /= l;
                    for (j = 0; j < 3; j += 2) cp[i][j] += d[j] * AiCkLerp(k->X, b) * g;
                    cp[i][1] += 4.0f * g;  // (Y down: a little lower, toward the sole)
                }
            }
            cw[i] = (k->W[0] < 0.0f ? 0.5f * o->body.hips + AiCkLerp(kAiCk[4].c[0].W, b) * g : AiCkLerp(k->W, b) * g);
            cd[i] = AiCkLerp(k->D, b) * g;
            cl[i] = k->lat;
        }
        for (i = 0; i < C->n; ++i) {  // subdivide: each segment's midpoint (the crackle needs the points)
            for (s = 0; s < (i + 1 < C->n ? 2 : 1); ++s) {
                const float f = 0.5f * (float)s;
                const int i1 = s ? i + 1 : i;
                for (j = 0; j < 3; ++j) P[n][j] = cp[i][j] + (cp[i1][j] - cp[i][j]) * f;
                W[n] = cw[i] + (cw[i1] - cw[i]) * f;
                D[n] = cd[i] + (cd[i1] - cd[i]) * f;
                lat[n] = s && cl[i1] ? cl[i1] : cl[i];  // (a midpoint between a round and an elliptic point: elliptic)
                ++n;
            }
        }
        L[0] = 0.0f;
        for (i = 1; i < n; ++i) L[i] = L[i - 1] + AiLen3(P[i], P[i - 1]);
        for (i = 0; i < n; ++i) {  // across the view (the screen perpendicular of the chain), the extents
            float d[3], e[3];
            const int a = i > 0 ? i - 1 : i, z = i + 1 < n ? i + 1 : i;
            const float *X = lat[i] == 1 ? Xh : Xs;
            for (j = 0; j < 3; ++j) d[j] = P[z][j] - P[a][j], e[j] = eyeOk ? P[i][j] - eye[j] : (j == 2 ? 1.0f : 0.0f);
            A[i][0] = d[1] * e[2] - d[2] * e[1], A[i][1] = d[2] * e[0] - d[0] * e[2], A[i][2] = d[0] * e[1] - d[1] * e[0];
            {
                const float l = sqrtf(A[i][0] * A[i][0] + A[i][1] * A[i][1] + A[i][2] * A[i][2]);
                const float le = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]), ld = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                if (l > 1e-4f * (le * ld + 1e-6f)) {
                    for (j = 0; j < 3; ++j) A[i][j] /= l;
                } else {  // the chain points at the camera: horizontal, across the view
                    A[i][0] = -e[2], A[i][1] = 0.0f, A[i][2] = e[0];
                    AiNorm3(A[i], 1.0f, 0.0f, 0.0f);
                }
            }
            AiNorm3(e, 0.0f, 0.0f, 1.0f);
            R[i] = AiCkExtent(W[i], D[i], X, A[i], lat[i]);
            Rv[i] = AiCkExtent(W[i], D[i], X, e, lat[i]);
        }
        fl = AiFlicker(t * 1.3, 120 + ch);
        {   // the fill: on the axis moved toward the camera to her front surface, fading out at her outline (v3.34 FX334)
            AiCkRib *r = &o->r[o->nRib++];
            ++o->nFill;
            r->n = n, r->kind = 0, r->ch = ch;
            for (i = 0; i < n; ++i) {
                const float band = powf(0.5f + 0.5f * sinf(6.2831853f * (L[i] / (AI_CK_BAND * g) - AI_CK_CRAWL * (float)fmod(t, 1000.0)) + 2.1f * (float)ch + 4.0f), 6.0f);
                float push = Rv[i] + 1.5f * g, de[3], dl;
                for (j = 0; j < 3; ++j) de[j] = eyeOk ? eye[j] - P[i][j] : 0.0f;
                dl = sqrtf(de[0] * de[0] + de[1] * de[1] + de[2] * de[2]);
                if (!eyeOk || !(dl > push + 20.0f)) push = 0.0f, dl = 1.0f;
                for (j = 0; j < 3; ++j) r->P[i][j] = P[i][j] + de[j] / dl * push;
                r->W[i] = R[i] * AI_CK_FILL_W + AI_CK_FILL_PAD * g;
                r->K[i] = kf * C->kMul * fl * (0.75f + AI_CK_BAND_K * band);  // v3.34 FX334 (v3.33: 0.85 + 0.3 band)
            }
            r->K[0] = r->K[n - 1] = 0.0f;  // v3.34 FX334: both ends unlit, no square strip end (v3.33: the spine's bottom x 0.4)
        }
        for (s = 0; s < 2; ++s) {  // her outline: crackling (re-rolled every 50 ms). v3.34 FX334: points only, no light (Thermal's
            // hot points crawl along it); v3.33 drew a core and a glow line here: the user's hard outline lines
            const float sideIn = s ? -1.0f : 1.0f;
            AiCkRib *core;
            if (C->edges == 1) {  // the shoulder line: only its upper edge (Y down: the side whose offset goes up)
                float up = 0.0f;
                for (i = 0; i < n; ++i) up += A[i][1];
                if (s == 1 || o->nRib + 1 > AI_CK_RIBS) break;
                if (up > 0.0f) {  // +A points down: take -A
                    for (i = 0; i < n; ++i) A[i][0] = -A[i][0], A[i][1] = -A[i][1], A[i][2] = -A[i][2];
                }
            }
            if (o->nRib + 1 > AI_CK_RIBS) break;
            core = &o->r[o->nRib++];
            core->n = n, core->kind = 1, core->ch = ch;
            for (i = 0; i < n; ++i) {
                const AiU32 h = AiHash(e50 * 0x2C1B3C6Du + (AiU32)ch * 131u + (AiU32)s * 17u + (AiU32)i * 7u);
                const float jit = (AiRnd01(h) - 0.5f) * 2.0f * (0.5f + 0.9f * cc) * g, jit2 = (AiRnd01(h ^ 0x5Bu) - 0.5f) * 1.2f * g;
                const float off = sideIn * R[i] + jit;
                float push = 0.4f * Rv[i], de[3], dl, tg[3];
                const int a = i > 0 ? i - 1 : i, z = i + 1 < n ? i + 1 : i;
                for (j = 0; j < 3; ++j) de[j] = eyeOk ? eye[j] - P[i][j] : 0.0f, tg[j] = P[z][j] - P[a][j];
                AiNorm3(tg, 0.0f, 0.0f, 0.0f);
                dl = sqrtf(de[0] * de[0] + de[1] * de[1] + de[2] * de[2]);
                if (!eyeOk || !(dl > push + 20.0f)) push = 0.0f, dl = 1.0f;
                for (j = 0; j < 3; ++j) core->P[i][j] = P[i][j] + A[i][j] * off + tg[j] * jit2 + de[j] / dl * push;
                core->W[i] = AI_CK_OUT_W * g;
                core->K[i] = 0.0f;  // v3.34 FX334: never lit
            }
        }
        o->nPts += n;
    }
    return o->nRib;
}
// ---------------------------------------------------------------- v3.56 PW356: the over-the-shoulder camera's see-her fade
// (staging/v356_feedback/w5_powers/PLAN.md; the user's v3.55 request 9: "In OTS camera, go ahead and reduce the
// behind-player after image effect opacity so we can see the player better"). In the over-the-shoulder camera the trail
// runs from her toward the lens, so the copies are drawn (additively, nearer than her) over her own body. Only the copies
// that are nearer the camera than she is AND whose box on screen overlaps hers lose light, down to `minK` when one sits
// right over her; a copy beside her on screen (a strafe, a turn, a side step) or behind her keeps its look, as the stock
// cameras do (they are never changed). Her box is her own body's (AiOtsBox: Henry is broader than Eileen), a copy's the
// same body at its own depth: the nearer, the larger on screen.
#define AI_OTS_MIN_DEF 0.30f   // a copy right over her keeps 30 % of its light (hidden [Gameplay] AfterImageOtsFade, %)
#define AI_OTS_E0      0.45f   // the overlap: 0 = the copy's centre on hers, 1 = their boxes just touch; minK up to 0.45 ...
#define AI_OTS_E1      1.00f   // ... full light from 1
#define AI_OTS_D0      8.0f    // nearer the camera than her by this (view depth, u): the fade starts ...
#define AI_OTS_D1      40.0f   // ... whole from this
#define AI_OTS_TAU_S   0.20f   // s: the camera switching in / out of over-the-shoulder fades the rule in / out (no pop)
// her body's box: its centre (between the head bone and the ankles), half width (hips apart + 22 u: Eileen 46, Henry 58;
// her arms swing out of the hips' line) and half height (half the head-to-ankle span + 30 u: the crown and the soles).
// 0 when the bones are not usable.
static int AiOtsBox(const float head[3], const float lfoot[3], const float rfoot[3], const float lhip[3], const float rhip[3],
                    float c[3], float *W, float *H)
{
    float ank[3], dx, dy, dz, span, hips, w, h;
    int j;
    for (j = 0; j < 3; ++j) {
        if (!(head[j] == head[j]) || !(lfoot[j] == lfoot[j]) || !(rfoot[j] == rfoot[j]) || !(lhip[j] == lhip[j]) || !(rhip[j] == rhip[j])) return 0;
        ank[j] = 0.5f * (lfoot[j] + rfoot[j]);
        c[j] = 0.5f * (head[j] + ank[j]);
    }
    dx = head[0] - ank[0], dy = head[1] - ank[1], dz = head[2] - ank[2];
    span = sqrtf(dx * dx + dy * dy + dz * dz);
    dx = lhip[0] - rhip[0], dy = lhip[1] - rhip[1], dz = lhip[2] - rhip[2];
    hips = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!(span > 40.0f) || !(span < 600.0f) || !(hips < 120.0f)) return 0;
    w = hips + 22.0f, h = 0.5f * span + 30.0f;
    *W = w < 30.0f ? 30.0f : w > 80.0f ? 80.0f : w;
    *H = h < 60.0f ? 60.0f : h > 240.0f ? 240.0f : h;
    return 1;
}
// the light a copy centred at p keeps (minK .. 1): eye the camera, c / W / H her box (AiOtsBox)
static float AiOtsK(const float eye[3], const float c[3], float W, float H, const float p[3], float minK)
{
    float f[3], r[3], u[3], v[3], dH, l, along, x, y, ex, ey, e, front, k;
    int j;
    if (!(minK < 1.0f)) return 1.0f;
    if (!(minK > 0.0f)) minK = 0.0f;
    for (j = 0; j < 3; ++j) f[j] = c[j] - eye[j], v[j] = p[j] - eye[j];
    dH = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(dH > 1.0f)) return 1.0f;
    for (j = 0; j < 3; ++j) f[j] /= dH;
    along = v[0] * f[0] + v[1] * f[1] + v[2] * f[2];
    if (!(along > 1.0f)) return 1.0f;  // at or behind the lens: not over her on screen (the near plane cuts it)
    front = AiSmooth(AI_OTS_D0, AI_OTS_D1, dH - along);
    if (!(front > 0.0f)) return 1.0f;  // behind her (her body hides it) or beside her depth
    r[0] = -f[2], r[1] = 0.0f, r[2] = f[0];  // across the view, horizontal
    l = sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (!(l > 1e-3f)) r[0] = 1.0f, r[2] = 0.0f, l = 1.0f;  // (looking straight down: any horizontal)
    r[0] /= l, r[2] /= l;
    u[0] = f[1] * r[2] - f[2] * r[1], u[1] = f[2] * r[0] - f[0] * r[2], u[2] = f[0] * r[1] - f[1] * r[0];  // f x r: the screen's vertical
    x = (v[0] * r[0] + v[1] * r[1] + v[2] * r[2]) / along;
    y = (v[0] * u[0] + v[1] * u[1] + v[2] * u[2]) / along;
    ex = fabsf(x) / (W / dH + W / along);
    ey = fabsf(y) / (H / dH + H / along);
    e = ex > ey ? ex : ey;
    k = minK + (1.0f - minK) * AiSmooth(AI_OTS_E0, AI_OTS_E1, e);
    return 1.0f - front * (1.0f - k);
}
// the rule's weight: toward 1 while the over-the-shoulder camera draws, toward 0 otherwise (AI_OTS_TAU_S, linear)
static float AiOtsMixStep(float mix, int ots, float dt)
{
    const float d = dt > 0.0f && dt < 0.5f ? dt / AI_OTS_TAU_S : 0.0f;
    if (ots) return mix + d > 1.0f ? 1.0f : mix + d;
    return mix - d < 0.0f ? 0.0f : mix - d;
}
// the hidden AfterImageOtsFade (%: the light a copy right over her keeps; 100 = v3.55, no fade) -> minK
static float AiOtsMinFromIni(int pct) { return pct < 0 ? AI_OTS_MIN_DEF : pct > 100 ? 1.0f : (float)pct / 100.0f; }
#endif
