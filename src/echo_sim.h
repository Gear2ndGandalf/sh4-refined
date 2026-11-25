// ================================================================ v3.30 ECHO: the Echo double, the pure state code
// staging/special_mods/r4_echo/PLAN.md. Pure C (no D3D, no game memory): included by echo.inc and by the native test
// test/echo_sim_test.c. Matrices are the game's: 4 x 4 floats, row vectors (p' = p x M), row 3 = the translation.
// The xz rotation convention is the one 0x534F07 / KB 3.1 use for her facing: forward = (sin yaw, cos yaw); a turn by a
// maps (x, z) to (x cos a - z sin a, x sin a + z cos a), so EchoAngleTo(f, EchoRot(f, a)) == a.
// v3.30 ECHO330 (staging/v330_feedback/w1_echo/PLAN.md): Echo Doubles: Off / Shadow Copy / Shadow Cast, 1-3 shadows; each
// shadow's delay is its own smooth random schedule (EchoDly); Shadow Cast's teleports (EchoTp, EchoPl), target spread and
// melee reach (EchoCastChoose, EchoMagnet); the shadow-soldier look as a pure sprite list (EchoLook*), which the game
// side projects and the preview (tools/echo_preview.py through echo_sim_test --look) rasterises: no second port.
// v3.31 ECHO331 (staging/v331_feedback/w1_echo/PLAN.md): the anchor heading of their places (EchoAnc: a spin turns a
// shadow in place, it no longer orbits her), her busy moments (EchoBusyStep: a Holy Candle set down, a floor pickup) and
// the replay of her last attack then (EchoRecPhase), one large black flame per shadow (EchoLookBuild), the eyes on the
// head bone's own axes, the budget's single-spike rule (EchoBudStep).
// v3.32 ECHO332 (staging/v332_feedback/w1_echo/PLAN.md, DESIGN.md): calm vs striking (EchoCalm: not attacking, a shadow
// stands 1.5 x farther from her at 45 % opacity; striking it is solid at the v3.31 distance; linear ramps eased where
// applied, the same path at any tick rate), the flame coat (EchoLookBuild: black tongues along the body and limbs, fine
// purple wisps off their edges, softer eyes), the busy pose gate (EchoLiveOk: her live pose never shows during or just
// after a busy moment).
// v3.33 ECHO333 (staging/v333_feedback/w1_echo/PLAN.md, DESIGN.md): the body plan (EchoBodyPlan: a body drawn
// see-through for longer than a fade, calm or under a vision filter, always keeps its depth prepass: the budget's level 2
// dropped it on the 2nd / 3rd shadow, whose calm 45 % body then blended all of her model's surfaces and read solid: "1
// shadow stays transparent while the other two stay solid"); the vision looks (EchoBodyOp, EchoLookBuild): under Thermal,
// Spirit Vision and the night visions every shadow is see-through at all times and wears the filter's theme.
// v3.34 FX334 (staging/v334_feedback/w2_fx/DESIGN.md; the user's v3.33 QA item 3: under Spirit Vision the shadows "look too
// much like the player"): Spirit Vision's shadows are dark specters: a near-black cold body (its texture detail mostly gone,
// 62 % striking .. 45 % calm: EchoBodyOp), a coat of dark smoke, dark smoke wisps and a faint cold rim behind the coat
// (EchoLookBuild); the eyes keep their pale light. Thermal and the night visions are unchanged.
#ifndef ECHO_SIM_H
#define ECHO_SIM_H
#include <float.h>
#include <math.h>
#include <string.h>

typedef unsigned int EcU32;
enum { ECHO_OFF, ECHO_COPY, ECHO_CAST, ECHO_MODES };  // the row's order (v3.30 ECHO330: was Off / Shadow / Twin / Mirror)
#define ECHO_MAXN 3                                   // shadows at most (the Echo Count row: 1 / 2 / 3)

// ---------------------------------------------------------------- the ini's old values (v3.29: Shadow / Twin / Mirror)
static int EchoLowerC(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int EchoStrIEq(const char *a, const char *b)
{
    while (*a && *b && EchoLowerC((unsigned char)*a) == EchoLowerC((unsigned char)*b)) ++a, ++b;
    return !*a && !*b;
}
// 1 when s is one of v3.29's values (all read as Shadow Copy now)
static int EchoOldName(const char *s) { return EchoStrIEq(s, "Shadow") || EchoStrIEq(s, "Twin") || EchoStrIEq(s, "Mirror"); }

// ---------------------------------------------------------------- the time-stamped ring (index math only)
// The payload lives in the caller's arrays, slot i <-> t[i]. Times only grow; a smaller time resets the ring.
#define ECHO_RING_MAX 192
#define ECHO_GAP_S    0.25f   // two neighbours farther apart than this are not blended (a pause, a hitch): the nearer wins
typedef struct { int cap, head, count; float t[ECHO_RING_MAX]; } EchoRing;

static void EchoRingInit(EchoRing *r, int cap)
{
    memset(r, 0, sizeof *r);
    r->cap = cap < 1 ? 1 : cap > ECHO_RING_MAX ? ECHO_RING_MAX : cap;
}
static void EchoRingClear(EchoRing *r) { r->head = r->count = 0; }
static int EchoRingSlot(const EchoRing *r, int k)  // k-th oldest (0 = oldest), or -1
{
    if (k < 0 || k >= r->count) return -1;
    return (r->head - r->count + k + 2 * r->cap) % r->cap;
}
static float EchoRingNewest(const EchoRing *r) { return r->count ? r->t[EchoRingSlot(r, r->count - 1)] : -1.0e30f; }
static float EchoRingOldest(const EchoRing *r) { return r->count ? r->t[EchoRingSlot(r, 0)] : 1.0e30f; }
// 1 when a sample stamped t is due (at least minStep after the newest; the first always): the caller then pushes it
static int EchoRingDue(EchoRing *r, float t, float minStep)
{
    if (!r->count) return 1;
    if (t < EchoRingNewest(r) - 1e-6f) {  // the clock went back (a reset the caller missed): start again
        EchoRingClear(r);
        return 1;
    }
    return t - EchoRingNewest(r) >= minStep - 1e-5f;
}
static int EchoRingPush(EchoRing *r, float t)  // the slot to fill (the oldest is dropped when full)
{
    const int i = r->head;
    r->t[i] = t;
    r->head = (r->head + 1) % r->cap;
    if (r->count < r->cap) ++r->count;
    return i;
}
// The two slots around time t and the blend between them (t0 + a (t1 - t0) = t); before the oldest / after the newest:
// that one, a = 0. A gap over ECHO_GAP_S: the nearer slot alone. Returns 0 when empty.
static int EchoRingFind(const EchoRing *r, float t, int *i0, int *i1, float *a)
{
    int lo, hi;
    if (!r->count) return 0;
    *a = 0.0f;
    if (t <= EchoRingOldest(r)) {
        *i0 = *i1 = EchoRingSlot(r, 0);
        return 1;
    }
    if (t >= EchoRingNewest(r)) {
        *i0 = *i1 = EchoRingSlot(r, r->count - 1);
        return 1;
    }
    lo = 0, hi = r->count - 1;  // binary search for the last slot with time <= t
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (r->t[EchoRingSlot(r, mid)] <= t) lo = mid;
        else hi = mid;
    }
    *i0 = EchoRingSlot(r, lo);
    *i1 = EchoRingSlot(r, hi);
    {
        const float t0 = r->t[*i0], t1 = r->t[*i1], d = t1 - t0;
        if (!(d > 1e-6f)) {
            *i1 = *i0;
            return 1;
        }
        if (d > ECHO_GAP_S) {  // no blend across a gap
            if (t - t0 > t1 - t) *i0 = *i1;
            else *i1 = *i0;
            return 1;
        }
        *a = (t - t0) / d;
    }
    return 1;
}
// Replay in order: the oldest slot with after < time <= upTo, or -1 (the caller sets after = that slot's time)
static int EchoRingNext(const EchoRing *r, float after, float upTo)
{
    int k;
    for (k = 0; k < r->count; ++k) {
        const int i = EchoRingSlot(r, k);
        if (r->t[i] > after && r->t[i] <= upTo) return i;
    }
    return -1;
}

// ---------------------------------------------------------------- the double's transform
static void EchoIdent(float M[16])
{
    memset(M, 0, 16 * sizeof(float));
    M[0] = M[5] = M[10] = M[15] = 1.0f;
}
static void EchoMul(float out[16], const float A[16], const float B[16])  // out = A x B (out may not alias)
{
    int r, c;
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            out[4 * r + c] = A[4 * r] * B[c] + A[4 * r + 1] * B[4 + c] + A[4 * r + 2] * B[8 + c] + A[4 * r + 3] * B[12 + c];
}
static void EchoPoint(const float M[16], const float p[3], float o[3])  // the point p (w 1) x M
{
    int j;
    float t[3];
    for (j = 0; j < 3; ++j) t[j] = p[0] * M[j] + p[1] * M[4 + j] + p[2] * M[8 + j] + M[12 + j];
    memcpy(o, t, sizeof t);
}
static void EchoVec(const float M[16], const float v[3], float o[3])  // the direction v (w 0) x M
{
    int j;
    float t[3];
    for (j = 0; j < 3; ++j) t[j] = v[0] * M[j] + v[1] * M[4 + j] + v[2] * M[8 + j];
    memcpy(o, t, sizeof t);
}
static float EchoDet3(const float M[16])
{
    return M[0] * (M[5] * M[10] - M[6] * M[9]) - M[1] * (M[4] * M[10] - M[6] * M[8]) + M[2] * (M[4] * M[9] - M[5] * M[8]);
}
// a turn by a about the vertical (Y) axis through the origin (the convention in the header)
static void EchoRotY(float M[16], float a)
{
    const float c = cosf(a), s = sinf(a);
    EchoIdent(M);
    M[0] = c, M[2] = s;    // x' = x c - z s, z' = x s + z c  (row vectors: x' = x M00 + z M20; z' = x M02 + z M22)
    M[8] = -s, M[10] = c;
}
static void EchoRot2(const float f[2], float a, float o[2])  // the same turn on an xz vector
{
    const float c = cosf(a), s = sinf(a), x = f[0], z = f[1];
    o[0] = x * c - z * s;
    o[1] = x * s + z * c;
}
// A shift (the spawn slide, the rise). A shadow: p' = (p - root) R(yaw) + root + off (EchoPl below adds Shadow Cast's
// place at an enemy).
static void EchoXformShift(float M[16], const float off[3])
{
    EchoIdent(M);
    M[12] = off[0], M[13] = off[1], M[14] = off[2];
}
static void EchoXformShadow(float M[16], const float root[3], float yaw, const float off[3])
{
    float R[16], rr[3];
    int j;
    EchoRotY(R, yaw);
    memcpy(M, R, sizeof R);
    EchoVec(R, root, rr);
    for (j = 0; j < 3; ++j) M[12 + j] = root[j] - rr[j] + off[j];
}
// ---------------------------------------------------------------- angles (xz plane, the header's convention)
static float EchoAngleTo(const float f[2], const float d[2])  // the signed turn from f to d (radians, -pi..pi)
{
    const float c = f[0] * d[0] + f[1] * d[1], s = f[0] * d[1] - f[1] * d[0];
    if (c == 0.0f && s == 0.0f) return 0.0f;
    return atan2f(s, c);
}
static float EchoClampF(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float EchoTurnStep(float cur, float want, float rate, float dt)  // at most rate x dt toward want
{
    const float step = rate * dt, d = want - cur;
    if (!(dt > 0.0f)) return cur;
    if (d > step) return cur + step;
    if (d < -step) return cur - step;
    return want;
}
static float EchoSmoothK(float dt, float tau) { return tau > 0.0f && dt > 0.0f ? 1.0f - expf(-dt / tau) : 1.0f; }
static float EchoEaseOut(float x)  // 0..1, quick start, soft landing
{
    x = EchoClampF(x, 0.0f, 1.0f);
    return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
}

// ---------------------------------------------------------------- the pose blend (Shadow, between two ring samples)
// Rows 0-2 lerped and given back the lerped lengths (the bone's scale stays), row 3 lerped. A turn over 30 degrees
// between the samples (a fast spin) is not blended: the nearer sample is used (PLAN 5.2).
static void EchoMatBlend(float o[16], const float a[16], const float b[16], float t)
{
    int r, j;
    {
        const float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        const float d = la > 1e-6f && lb > 1e-6f ? (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb) : 1.0f;
        if (d < 0.8660254f) {
            memcpy(o, t < 0.5f ? a : b, 16 * sizeof(float));
            return;
        }
    }
    for (r = 0; r < 3; ++r) {
        float v[3], l, la, lb;
        la = sqrtf(a[4 * r] * a[4 * r] + a[4 * r + 1] * a[4 * r + 1] + a[4 * r + 2] * a[4 * r + 2]);
        lb = sqrtf(b[4 * r] * b[4 * r] + b[4 * r + 1] * b[4 * r + 1] + b[4 * r + 2] * b[4 * r + 2]);
        for (j = 0; j < 3; ++j) v[j] = a[4 * r + j] + (b[4 * r + j] - a[4 * r + j]) * t;
        l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        for (j = 0; j < 3; ++j) o[4 * r + j] = l > 1e-6f ? v[j] * ((la + (lb - la) * t) / l) : a[4 * r + j];
        o[4 * r + 3] = a[4 * r + 3] + (b[4 * r + 3] - a[4 * r + 3]) * t;
    }
    for (j = 0; j < 4; ++j) o[12 + j] = a[12 + j] + (b[12 + j] - a[12 + j]) * t;
}
// v3.31 ECHO331: a cross-fade between two placed poses (two pose sources of a shadow, 0.2 s): as EchoMatBlend but a turn
// up to 120 degrees is still blended (the rows renormalised); only beyond it the nearer one is taken
static void EchoMatMix(float o[16], const float a[16], const float b[16], float t)
{
    const float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    const float la2 = sqrtf(a[8] * a[8] + a[9] * a[9] + a[10] * a[10]), lb2 = sqrtf(b[8] * b[8] + b[9] * b[9] + b[10] * b[10]);
    const float d = la > 1e-6f && lb > 1e-6f ? (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb) : 1.0f;
    const float d2 = la2 > 1e-6f && lb2 > 1e-6f ? (a[8] * b[8] + a[9] * b[9] + a[10] * b[10]) / (la2 * lb2) : 1.0f;
    int r, j;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    if (d < -0.5f || d2 < -0.5f) {
        memcpy(o, t < 0.5f ? a : b, 16 * sizeof(float));
        return;
    }
    for (r = 0; r < 3; ++r) {
        float v[3], l, ra, rb;
        ra = sqrtf(a[4 * r] * a[4 * r] + a[4 * r + 1] * a[4 * r + 1] + a[4 * r + 2] * a[4 * r + 2]);
        rb = sqrtf(b[4 * r] * b[4 * r] + b[4 * r + 1] * b[4 * r + 1] + b[4 * r + 2] * b[4 * r + 2]);
        for (j = 0; j < 3; ++j) v[j] = a[4 * r + j] + (b[4 * r + j] - a[4 * r + j]) * t;
        l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        for (j = 0; j < 3; ++j) o[4 * r + j] = l > 1e-6f ? v[j] * ((ra + (rb - ra) * t) / l) : (t < 0.5f ? a : b)[4 * r + j];
        o[4 * r + 3] = a[4 * r + 3] + (b[4 * r + 3] - a[4 * r + 3]) * t;
    }
    for (j = 0; j < 4; ++j) o[12 + j] = a[12 + j] + (b[12 + j] - a[12 + j]) * t;
}

// ---------------------------------------------------------------- a hit shape moved with the double (HitShape layout)
// hit.h: +0x00 pos (sphere centre / capsule start / box matrix row 0 ..), +0x10 end (kind 1) or radius (kind 2), kinds 3 /
// 4 an axis box (+0x00 min, +0x10 max), kind 5 a box (+0x00 a 4 x 4 matrix, +0x40 half extents), +0xA0 the kind byte.
// An axis box cannot turn: its centre moves, its extents stay (the stomp box is about as wide as deep).
#define ECHO_SHAPE_SZ 0xD0
static int EchoShapeXform(unsigned char *s, const float M[16])
{
    float *f = (float *)s;
    const int kind = s[0xA0];
    if (kind == 1) {
        EchoPoint(M, f, f);
        EchoPoint(M, f + 4, f + 4);
    } else if (kind == 2) {
        EchoPoint(M, f, f);
    } else if (kind == 3 || kind == 4) {
        float c[3], h[3], n[3];
        int j;
        for (j = 0; j < 3; ++j) c[j] = 0.5f * (f[j] + f[4 + j]), h[j] = 0.5f * (f[4 + j] - f[j]);
        EchoPoint(M, c, n);
        for (j = 0; j < 3; ++j) f[j] = n[j] - h[j], f[4 + j] = n[j] + h[j];
    } else if (kind == 5) {
        float o[16];
        EchoMul(o, f, M);  // the box's own matrix x M (row 3 = its centre)
        memcpy(f, o, sizeof o);
    } else {
        return 0;
    }
    return 1;
}
static void EchoShapeCentre(const unsigned char *s, float c[3])  // as 0x561680
{
    const float *f = (const float *)s;
    const int kind = s[0xA0];
    int j;
    if (kind == 2) memcpy(c, f, 12);
    else if (kind == 5) memcpy(c, f + 12, 12);
    else for (j = 0; j < 3; ++j) c[j] = 0.5f * (f[j] + f[4 + j]);
}

// ---------------------------------------------------------------- the enemies a window hit (one record per window)
#define ECHO_FOES  64
#define ECHO_GONE_S 0.2f  // missing from the list this long: removed (the same addresses later are a new being)
typedef struct { const void *task, *work; float seen; EcU32 win; } EchoFoe;
typedef struct { EchoFoe f[ECHO_FOES]; float clock; int drops; } EchoFoeTab;
static void EchoFoeReset(EchoFoeTab *t) { memset(t, 0, sizeof *t); }
static EchoFoe *EchoFoeFind(EchoFoeTab *t, const void *task, const void *work)
{
    int i, freeI = -1, oldI = 0;
    if (!task) return 0;
    for (i = 0; i < ECHO_FOES; ++i) {
        EchoFoe *f = &t->f[i];
        if (f->task == task) {
            if (f->work == work && t->clock - f->seen <= ECHO_GONE_S) {
                f->seen = t->clock;
                return f;
            }
            memset(f, 0, sizeof *f);  // the task slot holds another being now: clean
            freeI = i;
            break;
        }
        if (!f->task && freeI < 0) freeI = i;
        if (f->seen < t->f[oldI].seen) oldI = i;
    }
    if (freeI < 0) {
        freeI = oldI;
        ++t->drops;
    }
    memset(&t->f[freeI], 0, sizeof t->f[freeI]);
    t->f[freeI].task = task, t->f[freeI].work = work, t->f[freeI].seen = t->clock;
    return &t->f[freeI];
}
static int EchoFoeTake(EchoFoe *f, EcU32 win)  // 1 = the window's first hit on this being (marked); 0 = already hit
{
    if (!f || !win || f->win == win) return 0;
    f->win = win;
    return 1;
}

// ---------------------------------------------------------------- the record and the HP block (0x44F410's rules)
// The twin record is a HitRecord (0x50 bytes, include/enemy_chara.h) in the live list 0xFCE8B0. The HP block gets the
// attack id at +0x20 (the stun holds and the kill credit read it) and NEVER the serial at +0x1C: a twin serial there
// would let her own blade (serial S) hit the same enemy again in the same swing (re_echo 3).
typedef struct { float pos[4], dir[4]; int id, cat; EcU32 serial; float dmg, push; const void *task; } EchoHit;
static void EchoRecFill(unsigned char *rec, const EchoHit *h)
{
    memset(rec, 0, 0x50);
    memcpy(rec + 0x00, h->pos, 16);
    memcpy(rec + 0x10, h->dir, 16);
    memcpy(rec + 0x20, &h->id, 4);
    memcpy(rec + 0x24, &h->cat, 4);
    memcpy(rec + 0x28, &h->serial, 4);
    memcpy(rec + 0x2C, &h->dmg, 4);
    memcpy(rec + 0x30, &h->push, 4);
    memcpy(rec + 0x40, &h->task, sizeof(void *));  // +0x44 attack = NULL (zeroed): only 0x44F410's own test reads it
}
static void EchoHpMark(unsigned char *hp, int id) { memcpy(hp + 0x20, &id, 4); }  // +0x20 only (never +0x1C)
static EcU32 EchoSerial(EcU32 *counter)  // as 0x4FE100: (s & 0xFFFF) | 0xBA1E0000, the counter advanced
{
    const EcU32 s = *counter;
    *counter = s + 1;
    return (s & 0xFFFFu) | 0xBA1E0000u;
}
// 0x44F410's id rules: dropped ids (the monsters' own 0x49-0x65, 0x6D; the spray on the wheelchair)
static int EchoIdDropped(int id, int type) { return (id >= 0x49 && id <= 0x65) || id == 0x6D || (id == 0x2B && type == 6); }
// damage = hers x resist x (0.85 + 0.3 r; 0.833 + 0.167 r when the resist is > 1) x share (x2 for 0x66-0x6C), 0 if none
static float EchoDamage(float base, float rate, float rnd, float share, int id)
{
    float d;
    if (!(base > 0.0f) || !(rate > 0.0f) || !(share > 0.0f)) return 0.0f;
    d = base * rate * (rate > 1.0f ? 0.8333333f + 0.16666666f * rnd : 0.85f + 0.3f * rnd) * share;
    if (id >= 0x66 && id <= 0x6C) d += d;
    return d > 0.0f ? d : 0.0f;
}

// ---------------------------------------------------------------- Shadow's own target
typedef struct { float pos[3]; int ok; } EchoCand;  // ok: alive, hittable type, line of sight clear (the caller's tests)
// the nearest candidate within maxD of me, in my front half (xz); -1 = none
static int EchoPickTarget(const EchoCand *c, int n, const float me[3], const float fwd[2], float maxD)
{
    int i, best = -1;
    float bd = maxD * maxD;
    for (i = 0; i < n; ++i) {
        const float dx = c[i].pos[0] - me[0], dz = c[i].pos[2] - me[2], d2 = dx * dx + dz * dz;
        if (!c[i].ok || !(d2 < bd)) continue;
        if (dx * fwd[0] + dz * fwd[1] <= 0.0f) continue;  // behind it
        bd = d2;
        best = i;
    }
    return best;
}

// ---------------------------------------------------------------- the per-window state machine (tick side)
// A window is her attack serial S. It opens on a new S, stays open while her attack carries S and, for a swing (a
// node-attached box), a short tail after the game released the attack (a normal swing releases one tick after its first
// hit, 0x545160): the tail lasts as long as she held it plus ECHO_TAIL_S, at most ECHO_TAIL_MAX after the opening, and
// only while she is still in the swing modes. A gun line is tested on its first ECHO_LINE_T ticks only.
#define ECHO_TAIL_S   0.15f
#define ECHO_TAIL_MAX 0.45f
#define ECHO_LINE_T   2
typedef struct { EcU32 S; float open, held; int live, ticks, line, tail; } EchoWin;
// live: her attack object carries S this tick; swing: she is in a swing mode (3 / 4); node: the body has a bone-bound
// box; line: the shape is a gun line. Returns 1 while the window is open (the caller tests its shapes this tick).
static int EchoWinStep(EchoWin *w, EcU32 serial, int live, int swing, int node, int line, float now)
{
    if (live && serial && serial != w->S) {  // a new window
        memset(w, 0, sizeof *w);
        w->S = serial, w->open = now, w->live = 1, w->line = line;
    }
    if (!w->S) return 0;
    if (live && serial == w->S) {
        w->held = now - w->open;
        w->tail = 0;
        ++w->ticks;
        return !w->line || w->ticks <= ECHO_LINE_T;
    }
    w->live = 0;  // released (or another attack): only a swing's tail stays open
    if (w->line || !node || !swing) {
        w->S = 0;
        return 0;
    }
    if (now - w->open > (w->held + ECHO_TAIL_S < ECHO_TAIL_MAX ? w->held + ECHO_TAIL_S : ECHO_TAIL_MAX)) {
        w->S = 0;
        return 0;
    }
    w->tail = 1;
    ++w->ticks;
    return 1;
}
// ================================================================ v3.30 ECHO330: Echo Doubles
// ---------------------------------------------------------------- hashes
static EcU32 EchoHash(EcU32 a)  // (FxHash's / SfHash's mixer)
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float EchoRnd(EcU32 a) { return (float)(EchoHash(a) & 0xFFFFFFu) / 16777216.0f; }  // 0..1
static float EchoRnd2(EcU32 a, EcU32 b) { return EchoRnd(a * 0x9E3779B9u ^ EchoHash(b + 0x632BE5ABu)); }
static float EchoLerp(float a, float b, float t) { return a + (b - a) * t; }
static float EchoSat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float EchoSmooth(float e0, float e1, float x)
{
    const float t = e1 != e0 ? EchoSat((x - e0) / (e1 - e0)) : (x >= e1 ? 1.0f : 0.0f);
    return t * t * (3.0f - 2.0f * t);
}
static float EchoWrapPi(float a)
{
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

// ---------------------------------------------------------------- each shadow's delay: a smooth random schedule
// Segment k starts at `next`: the delay eases (smoothstep) from where it is to a new random value in MIN..MAX over T,
// T long enough that it never changes faster than SLEW a second (smoothstep's steepest slope is 1.5 x the mean), then
// holds; the next segment starts 2-6 s later (and never before the ease is done). Everything is a function of the
// segment index and the seed: the same delay at any tick rate. Her replay time (clock - delay) only moves forward (at
// 1 - SLEW .. 1 + SLEW of real time): no skipped poses, no pops.
#define ECHO_DLY_MIN   0.15f
#define ECHO_DLY_MAX   0.60f
#define ECHO_DLY_SLEW  0.30f
#define ECHO_DLY_IV0   2.0f
#define ECHO_DLY_IV1   6.0f
typedef struct { EcU32 seed; int k; float t0, d0, d1, T, next; } EchoDly;
static void EchoDlyInit(EchoDly *d, EcU32 seed, float now, float d0)
{
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->t0 = now;
    d->d0 = d->d1 = EchoLerp(ECHO_DLY_MIN, ECHO_DLY_MAX, EchoSat((d0 - ECHO_DLY_MIN) / (ECHO_DLY_MAX - ECHO_DLY_MIN)));
    d->next = now + EchoLerp(ECHO_DLY_IV0, ECHO_DLY_IV1, EchoRnd2(seed ^ 0x51EDu, 0u));
}
static float EchoDlyAt(EchoDly *d, float now)
{
    int guard = 0;
    while (now >= d->next && guard++ < 64) {  // (a long pause: the segments it skipped, in order)
        float iv;
        const float cur = d->d1;  // the ease of the last segment is complete by its `next`
        ++d->k;
        d->t0 = d->next;
        d->d0 = cur;
        d->d1 = EchoLerp(ECHO_DLY_MIN, ECHO_DLY_MAX, EchoRnd2(d->seed, (EcU32)d->k));
        d->T = 1.5f * fabsf(d->d1 - d->d0) / ECHO_DLY_SLEW;
        if (d->T < 0.3f) d->T = 0.3f;
        iv = EchoLerp(ECHO_DLY_IV0, ECHO_DLY_IV1, EchoRnd2(d->seed ^ 0x51EDu, (EcU32)d->k));
        if (iv < d->T + 0.25f) iv = d->T + 0.25f;
        d->next = d->t0 + iv;
    }
    if (!(d->T > 0.0f) || now >= d->t0 + d->T) return d->d1;
    if (now <= d->t0) return d->d0;
    return d->d0 + (d->d1 - d->d0) * EchoSmooth(0.0f, 1.0f, (now - d->t0) / d->T);
}
// the first delay of shadow i of n: spread over the range (stratified), so several shadows never start in step
static float EchoDlyFirst(EcU32 seed, int i, int n)
{
    const float w = (ECHO_DLY_MAX - ECHO_DLY_MIN) / (float)(n < 1 ? 1 : n);
    return ECHO_DLY_MIN + w * ((float)i + 0.15f + 0.7f * EchoRnd2(seed, 0xD1u + (EcU32)i));
}

// ---------------------------------------------------------------- a shadow's place: beside her, or at an enemy
// away 0: p' = (p - root) R(yaw) + root + off (the v3.29 Shadow form). away 1 (Shadow Cast at an enemy): her replayed root
// lands on spot + R (root - anchor) with that drift capped at ECHO_DRIFT (she steps in a swing: the shadow steps the same,
// turned; she runs off: it stays put), her facing turned by yaw (toward its enemy).
#define ECHO_DRIFT 100.0f
typedef struct { int away; float yaw, off[3], spot[3], anchor[3]; } EchoPl;
static void EchoPlXform(float M[16], const float root[3], const EchoPl *p)
{
    float d[3], R[16], dr[3], off[3], l;
    int j;
    if (!p->away) {
        EchoXformShadow(M, root, p->yaw, p->off);
        return;
    }
    for (j = 0; j < 3; ++j) d[j] = root[j] - p->anchor[j];
    l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l > ECHO_DRIFT)
        for (j = 0; j < 3; ++j) d[j] *= ECHO_DRIFT / l;
    EchoRotY(R, p->yaw);
    EchoVec(R, d, dr);
    for (j = 0; j < 3; ++j) off[j] = p->spot[j] + dr[j] - root[j];
    EchoXformShadow(M, root, p->yaw, off);
}
static float EchoTurnWrap(float cur, float want, float rate, float dt)  // as EchoTurnStep, the short way round
{
    const float d = EchoWrapPi(want - cur), step = rate * dt;
    if (!(dt > 0.0f)) return cur;
    return EchoWrapPi(cur + (d > step ? step : d < -step ? -step : d));
}

// ---------------------------------------------------------------- Shadow Cast: the teleport (smoke out, smoke in)
#define ECHO_TP_OUT 0.16f  // dissolving into smoke where it stands
#define ECHO_TP_IN  0.24f  // reforming from smoke at the new place
enum { ETP_HOME, ETP_OUT, ETP_IN, ETP_AWAY };
typedef struct { int ph, dest; float t; } EchoTp;  // dest 1: to an enemy, 0: back beside her
// starts (or turns round) a teleport; 1 when a dissolve starts now (the caller's smoke-out), 0 when one was under way
static int EchoTpGo(EchoTp *p, int dest)
{
    if (p->ph == ETP_OUT) {
        p->dest = dest;
        return 0;
    }
    if (p->ph == ETP_IN) {  // half formed: dissolve from the same visibility (smoothstep is symmetric: continuous)
        p->t = ECHO_TP_OUT * (1.0f - EchoSat(p->t / ECHO_TP_IN));
    } else {
        p->t = 0.0f;
    }
    p->ph = ETP_OUT;
    p->dest = dest;
    return 1;
}
// 1 at the switch out -> in (the caller moves the shadow and makes its smoke-in), 2 when it has formed again
static int EchoTpStep(EchoTp *p, float dt)
{
    if (p->ph == ETP_OUT) {
        p->t += dt;
        if (p->t >= ECHO_TP_OUT) {
            p->ph = ETP_IN;
            p->t -= ECHO_TP_OUT;
            if (p->t > ECHO_TP_IN) p->t = ECHO_TP_IN;
            return 1;
        }
    } else if (p->ph == ETP_IN) {
        p->t += dt;
        if (p->t >= ECHO_TP_IN) {
            p->ph = p->dest ? ETP_AWAY : ETP_HOME;
            p->t = 0.0f;
            return 2;
        }
    }
    return 0;
}
static float EchoTpVis(const EchoTp *p)
{
    if (p->ph == ETP_OUT) return 1.0f - EchoSmooth(0.0f, 1.0f, p->t / ECHO_TP_OUT);
    if (p->ph == ETP_IN) return EchoSmooth(0.0f, 1.0f, p->t / ECHO_TP_IN);
    return 1.0f;
}
// the enemy a shadow takes: the one the fewest shadows are on now, the nearest of those (cands sorted nearest first)
static int EchoCastChoose(const int *onIt, int m)
{
    int i, best = -1;
    for (i = 0; i < m; ++i)
        if (best < 0 || onIt[i] < onIt[best]) best = i;
    return best;
}
// the direction (radians, the facing convention) shadow i comes in from round its enemy, try k of 8: a start hashed from
// the seed (the caller's: the enemy and the engagement) and the plan, shadows on the same enemy 120 degrees apart, then
// round in 45 degree steps
static float EchoSpotAngle(EcU32 seed, int i, int plan, int k)
{
    return 6.2831853f * (EchoRnd2(seed ^ (EcU32)plan * 0x2545F491u, 0xA5u) + (float)i / 3.0f + (float)k / 8.0f);
}
// the same floor: the floors under her and under the enemy within tol, tol from her own height (half of it, 60..110)
static float EchoFloorTol(float herH) { return EchoClampF(0.5f * herH, 60.0f, 110.0f); }
// a melee shape that comes within reach of its enemy's hurt centre moves onto it (as Playable Eileen's MeleeAssist moves
// her box): 1 = moved. The shape's centre and kind as EchoShapeCentre / EchoShapeXform (a gun line is never moved).
static int EchoMagnet(unsigned char *s, const float tgt[3], float reach)
{
    float c[3], d[3], M[16];
    int j;
    if (s[0xA0] == 1) return 0;
    EchoShapeCentre(s, c);
    for (j = 0; j < 3; ++j) d[j] = tgt[j] - c[j];
    if (!(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= reach * reach)) return 0;
    EchoXformShift(M, d);
    return EchoShapeXform(s, M);
}
// each shadow's damage share: EchoDamage % x 1.5 (at most 100 %), x 1 / 0.8 / 0.67 for 1 / 2 / 3 shadows (75 / 120 /
// 150 % of her damage in all at the default 50)
static float EchoShareFor(float base, int n)
{
    static const float kK[ECHO_MAXN] = { 1.0f, 0.8f, 0.6666667f };
    float s = base * 1.5f;
    if (s > 1.0f) s = 1.0f;
    return s * kK[n < 1 ? 0 : n > ECHO_MAXN ? ECHO_MAXN - 1 : n - 1];
}

// ---------------------------------------------------------------- the draw budget
// The double's CPU cost a frame over ECHO_MS_CAP for ECHO_SLOW_N frames in a row: one level down; a new room starts again
// at 0. v3.31 ECHO331: also a single frame over ECHO_MS_SPIKE (the v3.30 log: single frames of 10-22 ms the 30-frame rule
// never saw), at most one such step every ECHO_SPIKE_GAP frames (one long hitch does not run through every level). The
// levels (echo.inc): 1 no weapon redraw on the 2nd / 3rd, 2 the light look (no violet edge layer on the 2nd / 3rd, half
// the smoke) and no depth prepass on the 2nd / 3rd, 3 no weapon redraw at all and no ground discs.
#define ECHO_MS_CAP    3.0f
#define ECHO_SLOW_N    30
#define ECHO_MS_SPIKE  6.0f
#define ECHO_SPIKE_GAP 20
typedef struct { int level, slow, gap; } EchoBud;
static int EchoBudStep(EchoBud *b, float ms)  // 1 when the level went down now (the 30-frame rule), 2 (a single spike)
{
    if (b->gap > 0) --b->gap;
    if (ms > ECHO_MS_SPIKE && b->level < 3 && !b->gap) {
        ++b->level;
        b->slow = 0;
        b->gap = ECHO_SPIKE_GAP;
        return 2;
    }
    if (!(ms > ECHO_MS_CAP)) {
        b->slow = 0;
        return 0;
    }
    if (++b->slow < ECHO_SLOW_N || b->level >= 3) return 0;
    ++b->level;
    b->slow = 0;
    b->gap = ECHO_SPIKE_GAP;
    return 1;
}

// ================================================================ v3.31 ECHO331
// ---------------------------------------------------------------- the anchor heading of their places
// The slots beside her (and the Auto side) are in the frame of this heading, not of her facing that tick: while she
// walks it follows her facing (ECHO_ANC_TAU, at most ECHO_ANC_RATE); while she attacks, or is busy, it holds (a spin, a
// lock-on turn, a chain spin: the shadows turn in place, they no longer orbit her); standing, an idle turn of more than
// ECHO_ANC_TURN held for ECHO_ANC_HOLD re-settles it slowly (ECHO_ANC_SETTLE) until within 2 degrees. Her facing angle is
// the yaw of the header's convention (forward = (sin yaw, cos yaw)). Exponential steps: the same path at any tick rate.
#define ECHO_ANC_MOVE   90.0f       // u/s (xz, smoothed 0.1 s): she walks
#define ECHO_ANC_TAU    0.30f
#define ECHO_ANC_RATE   4.712389f   // 270 degrees a second
#define ECHO_ANC_TURN   0.8726646f  // 50 degrees
#define ECHO_ANC_HOLD   1.0f
#define ECHO_ANC_SETTLE 0.6f
#define ECHO_ANC_DONE   0.0349066f  // 2 degrees
typedef struct { int init, settling; float H, vs, held; } EchoAnc;
static float EchoAncStep(EchoAnc *a, float yaw, float dx, float dz, float dt, int hold)
{
    float d, v, step;
    if (!a->init || !_finite(a->H)) {
        memset(a, 0, sizeof *a);
        a->init = 1, a->H = EchoWrapPi(yaw);
        return a->H;
    }
    if (!(dt > 0.0f) || !_finite(yaw)) return a->H;
    v = sqrtf(dx * dx + dz * dz) / dt;
    if (!(v < 3000.0f)) v = 0.0f;  // a warp (the caller resets anyway)
    a->vs += (v - a->vs) * EchoSmoothK(dt, 0.1f);
    d = EchoWrapPi(yaw - a->H);
    if (hold) {
        a->held = 0.0f, a->settling = 0;
        return a->H;
    }
    if (a->vs > ECHO_ANC_MOVE) {
        a->held = 0.0f, a->settling = 0;
        step = d * EchoSmoothK(dt, ECHO_ANC_TAU);
    } else {
        if (fabsf(d) > ECHO_ANC_TURN) a->held += dt;
        else if (!a->settling) a->held = 0.0f;
        if (a->held >= ECHO_ANC_HOLD) a->settling = 1;
        if (!a->settling) return a->H;
        step = d * EchoSmoothK(dt, ECHO_ANC_SETTLE);
    }
    step = EchoClampF(step, -ECHO_ANC_RATE * dt, ECHO_ANC_RATE * dt);
    a->H = EchoWrapPi(a->H + step);
    if (a->settling && fabsf(EchoWrapPi(yaw - a->H)) < ECHO_ANC_DONE) a->settling = 0, a->held = 0.0f;
    return a->H;
}

// ---------------------------------------------------------------- her busy moments (decomp, pl_53f7b0.c / ev_4d21a0.c)
// A Holy Candle set down: PlayerCandleUse 0x540110 runs while P+0x218 != 0 with the item use P+0x3A8 == 4 (the dispatch
// in pl_537950.c: `if (p->unk_218 != 0) PlayerCandleHandler(p)`). A floor pickup: the item event's step 0x4D21A0 calls
// 0x530650 / 0x530680: mode P+0x214 = 0x4F and the clip [0x10A19F4] = 0x6D / 0x6E; 0x5306C0 turns 0x4F into 0x4E for
// the rest of the clip. Playable Eileen's possession fits write clip 109 (0x6D) with mode 0x4E directly: only a 0x4E that
// came from a 0x4F counts (the latch).
enum { EBZ_NONE, EBZ_CANDLE, EBZ_PICKUP };
typedef struct { int latch; } EchoBusyL;
static int EchoBusyStep(EchoBusyL *b, int mode, int clip, int use, int useItem)
{
    if (use != 0 && useItem == 4) {
        b->latch = 0;
        return EBZ_CANDLE;
    }
    if (mode == 0x4F && (clip == 0x6D || clip == 0x6E)) b->latch = 1;
    else if (mode != 0x4E) b->latch = 0;
    return b->latch ? EBZ_PICKUP : EBZ_NONE;
}

// ---------------------------------------------------------------- her last attack, recorded and replayed
// While she attacks (a swing / charged swing / shot: modes 3 / 4, or a shot of Playable Eileen's own weapon) the tick
// keeps her open windows' samples and the draw her poses (ECHO_REC_POSE_HZ); her attack activity gone ECHO_REC_END_S (or
// ECHO_REC_MAX_S long): it is complete when it lasted ECHO_REC_MIN_S and had a window. During a busy moment a shadow
// replays it at an enemy, again and again: repetition k at k x (length + ECHO_REC_GAP_S); in the gap it holds the last
// pose (a beat between swings), and every repetition is a new window (its own twin serial).
#define ECHO_REC_POSE_N  72          // poses kept (40 a second: 1.8 s)
#define ECHO_REC_POSE_HZ 40.0f
#define ECHO_REC_HIT_N   168         // window samples kept (at most 90 a second); v3.31 ECHO-331-001 (inspection 1): was 64
                                     // (0.71 s), now the whole ECHO_REC_MAX_S at 90 a second (162) + slack
#define ECHO_REC_MAX_S   1.8f
#if ECHO_REC_HIT_N > ECHO_RING_MAX
#error ECHO_REC_HIT_N exceeds the ring
#endif
#define ECHO_REC_MIN_S   0.20f
#define ECHO_REC_END_S   0.20f
#define ECHO_REC_GAP_S   0.35f
// the replay time within a repetition (0..len; in the gap: len) and the repetition at time t since the replay began
static float EchoRecPhase(float len, float t, int *rep)
{
    const float cyc = len + ECHO_REC_GAP_S;
    float k, local;
    if (!(len > 0.0f) || !(t > 0.0f)) {
        *rep = 0;
        return 0.0f;
    }
    k = floorf(t / cyc);
    local = t - k * cyc;
    if (local < 0.0f) local = 0.0f;
    *rep = (int)k;
    return local < len ? local : len;
}

// ---------------------------------------------------------------- v3.32 ECHO332: calm vs striking
// Not attacking, a shadow is calm: 1.5 x farther from her (its slot) and at 45 % opacity (EchoCalmOp), so it no longer
// stands between the camera and her; the moment it strikes (its replayed sample is in a swing / shot / stomp or has an
// open window, a busy replay, a Shadow Cast teleport to an enemy or its stay there) it turns solid and comes back to the
// v3.31 distance in ECHO_CALM_IN; ECHO_CALM_HOLD after its last strike it eases back over ECHO_CALM_OUT. k is a linear
// ramp (0 striking .. 1 calm) stepped by real time, the remainder of a tick carried over: the same path at any tick rate;
// the callers apply it eased (EchoEase): no pops.
#define ECHO_CALM_IN   0.12f
#define ECHO_CALM_OUT  0.50f
#define ECHO_CALM_HOLD 0.35f
#define ECHO_CALM_OPA  0.45f
#define ECHO_CALM_DIST 1.5f
typedef struct { int init; float k, hold; } EchoCalm;
static float EchoCalmStep(EchoCalm *c, int strike, float dt)
{
    if (!c->init) {
        c->init = 1, c->k = strike ? 0.0f : 1.0f, c->hold = strike ? ECHO_CALM_HOLD : 0.0f;
        return c->k;
    }
    if (!(dt > 0.0f)) return c->k;
    if (strike) {
        c->hold = ECHO_CALM_HOLD;
        c->k -= dt / ECHO_CALM_IN;
        if (c->k < 0.0f) c->k = 0.0f;
        return c->k;
    }
    if (c->hold > 0.0f) {
        c->hold -= dt;
        if (c->hold >= 0.0f) return c->k;
        dt = -c->hold;  // (the part of this tick after the hold ran out eases)
        c->hold = 0.0f;
    }
    c->k += dt / ECHO_CALM_OUT;
    if (c->k > 1.0f) c->k = 1.0f;
    return c->k;
}
static float EchoCalmDist(float k) { return 1.0f + (ECHO_CALM_DIST - 1.0f) * EchoSmooth(0.0f, 1.0f, EchoSat(k)); }
// v3.32 ECHO332: her live pose at replay time tr may be shown: never inside a busy moment (busy now since t0; else the
// last one ran t0 .. t1, t1 < 0 none) with `margin` s on both sides: the pickup / candle pose is never mimicked, also not
// in the delay after it ends (the shadow's replay time is still inside it then)
static int EchoLiveOk(float tr, int busyNow, float t0, float t1, float margin)
{
    if (busyNow) return tr < t0 - margin;
    if (!(t1 >= 0.0f)) return 1;
    return tr > t1 + margin || tr < t0 - margin;
}

// ---------------------------------------------------------------- v3.33 ECHO333: the body plan and the vision opacity
// The user's v3.32 QA: "with weapons equipped 1 shadow stays transparent out of combat while the other two stay solid".
// The cause (the user's log 2026-11-19 / 20: "budget level 2 for this room (+ the light look and no depth prepass on the
// 2nd / 3rd)" within seconds of most rooms once a weapon is redrawn per shadow): the budget's level 2 dropped the depth
// prepass of shadows 2 and 3, and a calm shadow's 45 % body drawn without it blends EVERY surface of her model (Playable
// Eileen's materials are mostly two-sided: front and back faces, her jacket over her shirt, hair cards; Henry: an arm over
// the torso): 3 layers of 45 % are 83 %, so they read solid while shadow 1, whose prepass the budget kept, stayed at 45 %.
// Bare-handed there is no weapon redraw, the frames stay cheaper and the budget seldom reached level 2: hence "with
// weapons". v3.33: a body drawn see-through for longer than a fade (calm, or under a vision filter: EchoBodySteady)
// always keeps its prepass, at every budget level (EchoDepth=0 aside: the ini asks for one pass); the budget still drops
// it for the short fades (forming, a teleport, a wall) and keeps dropping the weapon redraws and the light look.
// Looks: 0 normal, 1 Thermal, 2 night vision, 3 Spirit Vision (as EchoLookIn.look).
#define ECHO_LAYERS     3.0f    // her body's surfaces one pixel blends without the prepass (the estimate the log uses)
#define ECHO_VIS_STRIKE 0.50f   // under a vision filter every body is see-through: striking 50 %, calm 35 %
#define ECHO_VIS_CALM   0.35f
#define ECHO_SV_STRIKE  0.62f   // v3.34 FX334: Spirit Vision's dark specters: near-black, so a little denser to read (still
#define ECHO_SV_CALM    0.45f   //   see-through: under the 70 % EchoOpCheck calls solid; calm = the normal look's calm 45 %)
// the body's opacity (x its visibility) in a look at calm k: the normal look EchoCalmOp (solid .. 45 %); a vision look
// see-through at all times (50 % striking .. 35 % calm; v3.34 FX334: Spirit Vision 62 .. 45 %), eased
static float EchoBodyOp(int look, float k)
{
    float e;
    k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
    e = k * k * (3.0f - 2.0f * k);
    if (look == 0) return 1.0f - (1.0f - ECHO_CALM_OPA) * e;
    if (look == 3) return ECHO_SV_STRIKE + (ECHO_SV_CALM - ECHO_SV_STRIKE) * e;  // v3.34 FX334
    return ECHO_VIS_STRIKE + (ECHO_VIS_CALM - ECHO_VIS_STRIKE) * e;
}
// Why a shadow strikes this tick (v3.32's rule, moved here to be tested): its replayed sample of her (a live pose source)
// is a swing / charged swing / stomp (modes 3 / 4 / 5) or has an open hit window; it replays her last attack (a busy
// moment); Shadow Cast: it stands at an enemy, or teleports to one. Her ready stance (mode 2, a weapon raised), walking
// with a weapon, aiming: no strike.
enum { ESW_NONE, ESW_SWING, ESW_REPLAY, ESW_AWAY, ESW_TELEPORT };
static int EchoWhyOf(int srcLive, int srcRec, int st, int n, int tpPh, int tpDest)
{
    if (srcLive && (st == 3 || st == 4 || st == 5 || n > 0)) return ESW_SWING;
    if (srcRec) return ESW_REPLAY;
    if (tpPh == ETP_AWAY) return ESW_AWAY;
    if ((tpPh == ETP_OUT || tpPh == ETP_IN) && tpDest) return ESW_TELEPORT;
    return ESW_NONE;
}
// see-through for longer than a fade: a vision look, or calm in the normal look
static int EchoBodySteady(int look, float k) { return look != 0 || EchoBodyOp(0, k) < 0.995f; }
typedef struct {
    int solid;  // one opaque pass (its own depth)
    int depth;  // the depth prepass (nearest surface only), then the colour on it
    int noWep;  // the weapon not redrawn (the budget)
    int lite;   // the light look (the budget)
    float op;   // the opacity a reader sees (the estimate: without the prepass 1 - (1 - alpha)^ECHO_LAYERS)
} EchoBodyPl;
// shadow idx's body passes this frame: alpha its body alpha (visibility x EchoBodyOp), k its calm, level the budget's,
// depthCfg [Gameplay] EchoDepth (0 never, 1 every fade, 2 Thermal / Spirit Vision fades: the default)
static void EchoBodyPlan(EchoBodyPl *p, int look, int level, int idx, float alpha, float k, int depthCfg)
{
    const int cut = level >= 2 && idx > 0;  // the budget's level 2: the 2nd / 3rd shadow
    memset(p, 0, sizeof *p);
    p->noWep = (level >= 1 && idx > 0) || level >= 3;
    p->lite = cut || level >= 3;
    if ((look == 0 || look == 2) && alpha >= 0.995f) {
        p->solid = 1, p->op = 1.0f;
        return;
    }
    if (depthCfg != 0 && EchoBodySteady(look, k)) p->depth = 1;  // v3.33 ECHO333: never cut by the budget
    else p->depth = (depthCfg == 1 || (depthCfg == 2 && (look == 1 || look == 3))) && !cut;
    p->op = p->depth ? alpha : 1.0f - powf(1.0f - (alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha), ECHO_LAYERS);
}

// ---------------------------------------------------------------- the shadow-soldier look: a pure sprite list
// Bones (PBONES of the shared henry01 / Eileen hierarchy, KB section 3; the same as Death Presence's kDpHerBone): head 11,
// neck 10, chest 9, pelvis 8, shoulders 18 / 13, elbows 19 / 14, wrists 20 / 15, hips 5 / 2, knees 6 / 3, feet 7 / 4.
// World units, Y DOWN. Every sprite is a closed-form function of the time and the hashed slot: the same look at any rate.
enum { EB_HEAD, EB_NECK, EB_CHEST, EB_PELVIS, EB_LSH, EB_RSH, EB_LELB, EB_RELB, EB_LHAND, EB_RHAND, EB_LHIP, EB_RHIP, EB_LKNEE,
       EB_RKNEE, EB_LFOOT, EB_RFOOT, EB_N };
static const unsigned char kEchoBone[EB_N] = { 11, 10, 9, 8, 18, 13, 19, 14, 20, 15, 5, 2, 6, 3, 7, 4 };
// sprite kinds: the game side maps them to its textures (ESK_SOFT FXX_SMOKE soft cell, ESK_ORB the SV atlas orb, ESK_FLAME
// the spirit fire flipbook (tip / half width), ESK_SMOKE0 / 1 / CREST the FXX_SMOKE puffs, ESK_GROUND a flat disc on the floor)
enum { ESK_SOFT, ESK_ORB, ESK_FLAME, ESK_SMOKE0, ESK_SMOKE1, ESK_CREST, ESK_GROUND, ESK_WISP, ESK_N };  // v3.32 ECHO332: + ESK_WISP
// (ESK_WISP: a piece of a wisp: FXX_SMOKE's soft cell stretched from p to tip, its centre half way: a soft streak, no tip)
typedef struct { float p[3], tip[3], r, a, rgb[3], push; unsigned char kind, add, cell, pad; float floor; } EchoSpr;  // v3.31: + floor (pad bit 0: a flame fades out on it)
// the looks: 0 the normal view, 2 night vision (drawn before its filter, as the world), 3 Spirit Vision (after its pass);
// Thermal (1): v3.32 drew no sprites (a shadow has no heat); v3.33 ECHO333: a cold see-through body wreathed in its coat
// as heat, with hot points (DESIGN.md: the vision looks)
typedef struct {
    float b[EB_N][3];   // the shadow's bones this frame (world)
    float face[2];      // its facing (xz, unit)
    float vel[3];       // its smoothed velocity (u/s): the flames lean back from it
    float light[3];     // its light colour (EchoColour: violet by default)
    float t, vis, flare, floorY;  // clock (s), visibility 0..1, the eyes' flare 0..1 (after it formed), the floor under it
    int look, idx;      // look; the shadow's index (its hashes)
    int lite;           // v3.31 ECHO331: the budget's light look (no violet edge layer, half the smoke)
    float head[3][3];   // v3.31 ECHO331: the head bone's own axes this frame (its world matrix rows 0-2, unit length: x side,
                        // y down the face, z forward); all zero: the facing frame (v3.30's eyes)
    float calm;         // v3.32 ECHO332: 0 striking (solid) .. 1 calm (faded): EchoCalm's linear k, eased here
} EchoLookIn;
typedef struct { float t0; int kind; EcU32 seed; float b[EB_N][3]; } EchoEvt;  // a teleport's smoke: 0 out, 1 in
#define ECHO_PUFF_N   24
#define ECHO_PUFF_OUT 0.60f   // a smoke-out puff's life
#define ECHO_PUFF_IN  0.50f   // a smoke-in puff's life (it gathers in ECHO_TP_IN)
#define ECHO_SPR_MAX  48      // sprites a shadow at most (v3.32: <= 39 steady, 17 + 2 smoke events of 15 in a teleport; v3.31: 8
                              // steady + 2 smoke events of 18; v3.30: 160)
// v3.31 ECHO331: the eyes in the head bone's frame (x side, y down, z forward): measured on both bodies (tools/
// echo_preview.py, the idle pose): v3.30's place on Playable Eileen's face is (+-7.5, -9.0, 25.5) there; Henry's face
// front at eye height is 23-24.6 in front (nose tip -6.5 / 26.8); y -11.5, z 25 sits on both faces
#define ECHO_EYE_X  7.5f
#define ECHO_EYE_Y  -11.5f
#define ECHO_EYE_Z  25.0f
static EchoSpr *EchoSprNew(EchoSpr *o, int *n, int max, int kind, int add, const float p[3], float r, float a, const float rgb[3])
{
    EchoSpr *s;
    if (*n >= max || !(a > 0.004f) || !(r > 0.0f)) return NULL;
    s = &o[(*n)++];
    memset(s, 0, sizeof *s);
    memcpy(s->p, p, sizeof s->p);
    s->r = r, s->a = a, s->kind = (unsigned char)kind, s->add = (unsigned char)(add != 0);
    memcpy(s->rgb, rgb, sizeof s->rgb);
    return s;
}
static void EchoScale3(float o[3], const float c[3], float k) { o[0] = c[0] * k, o[1] = c[1] * k, o[2] = c[2] * k; }
// v3.32 ECHO332: the flame coat (DESIGN.md). The spirit fire cell's tongue (spiritfire_sim.h, 64 x 128) is widest in its
// lower part (its visible half width there ~0.62 of the quad's half width; the round foot at 97 % down) and ends 19 % below
// its top; with the foot point 20 % up the quad (EchoFlame), the visible tongue runs from 0.21 below the foot point to
// 0.7625 of the way to the tip point. Each body segment gets one tongue: its foot on the segment's lower end (a flat
// segment: its inner end), its visible tip `rise` u over the other end (world up: flames rise in any pose), its half width
// (r + ECHO_TG_MARGIN) / ECHO_TG_VIS: the black stands ~8 u outside the limb (x s = her height / 160) instead of v3.31's
// column of 1.92 x her height. Every tongue at least 1.84 half widths long (the cell's own aspect is 2 : 1).
#define ECHO_TG_N      12       // black tongues: torso, 2 shoulders, head, 2 upper arms, 2 forearms, 2 thighs, 2 shins
#define ECHO_TG_LITE   6        // the light look: torso, head, 2 whole arms, 2 whole legs
#define ECHO_TG_VIS    0.62f
#define ECHO_TG_TOP    0.7625f
#define ECHO_TG_MARGIN 8.0f
#define ECHO_TG_BLACK  0.92f    // premultiplied black x the cell
#define ECHO_TG_EDGE   0.30f    // the faint violet edge layer behind the torso, shoulders and head (premultiplied violet)
#define ECHO_TG_FPS    15.0f    // the flipbook (28 frames, a seamless loop)
#define ECHO_WISP_N    8        // fine purple wisps, 2 streaks each (the light look: 3)
#define ECHO_WISP_A    0.60f
#define ECHO_EYE_R     15.0f    // the eyes' soft glow (v3.31: 10 u at 0.55) and their faint wide haze
#define ECHO_EYE_HAZE  30.0f
// v3.32 ECHO332: a body segment of the coat: bones a -> b (a the inner end), the limb's radius (u at s = 1; < 0: the
// torso, from the hips' half width + 9), the visible tip's rise over the upper end, the push behind the body
typedef struct { unsigned char a, b; float r, rise, push; } EchoSegD;
enum { ESG_TORSO, ESG_LSHO, ESG_RSHO, ESG_HEAD, ESG_LUARM, ESG_RUARM, ESG_LFARM, ESG_RFARM, ESG_LTHIGH, ESG_RTHIGH, ESG_LSHIN, ESG_RSHIN };
static const EchoSegD kEchoSeg[ECHO_TG_N] = {
    { EB_PELVIS, EB_NECK, -1.0f, 4.0f, 12.0f },
    { EB_LSH, EB_LSH, 8.0f, 18.0f, 10.0f }, { EB_RSH, EB_RSH, 8.0f, 18.0f, 10.0f },  // (a = b: a tongue rising off it)
    { EB_NECK, EB_HEAD, 12.0f, 40.0f, 9.0f },
    { EB_LSH, EB_LELB, 7.5f, 16.0f, 7.0f }, { EB_RSH, EB_RELB, 7.5f, 16.0f, 7.0f },
    { EB_LELB, EB_LHAND, 6.5f, 12.0f, 6.0f }, { EB_RELB, EB_RHAND, 6.5f, 12.0f, 6.0f },
    { EB_LHIP, EB_LKNEE, 10.0f, 6.0f, 8.0f }, { EB_RHIP, EB_RKNEE, 10.0f, 6.0f, 8.0f },
    { EB_LKNEE, EB_LFOOT, 7.5f, 6.0f, 7.0f }, { EB_RKNEE, EB_RFOOT, 7.5f, 6.0f, 7.0f } };
static const EchoSegD kEchoSegLite[ECHO_TG_LITE] = {
    { EB_PELVIS, EB_NECK, -1.0f, 8.0f, 12.0f }, { EB_NECK, EB_HEAD, 12.0f, 40.0f, 9.0f },
    { EB_LSH, EB_LHAND, 7.5f, 12.0f, 7.0f }, { EB_RSH, EB_RHAND, 7.5f, 12.0f, 7.0f },
    { EB_LHIP, EB_LFOOT, 9.0f, 6.0f, 8.0f }, { EB_RHIP, EB_RFOOT, 9.0f, 6.0f, 8.0f } };
// the segments the wisps come off (weighted by repetition: the head, the shoulders and arms most)
static const unsigned char kEchoWispSeg[12] = { ESG_HEAD, ESG_HEAD, ESG_LSHO, ESG_RSHO, ESG_LUARM, ESG_RUARM, ESG_LFARM, ESG_RFARM,
                                                ESG_TORSO, ESG_TORSO, ESG_LTHIGH, ESG_RTHIGH };
// the teleport smoke's puffs (v3.32: 10 of the 24, both sides of the body; v3.31 took every other one, all on her right)
static const unsigned char kEchoPuffUse[10] = { 0, 1, 4, 5, 6, 9, 10, 13, 14, 17 }, kEchoPuffLite[5] = { 0, 4, 5, 6, 13 };
static float EchoEase(float k) { return EchoSmooth(0.0f, 1.0f, k); }
// v3.32 ECHO332: the opacity of a calm shadow's parts (k = EchoCalm's linear value): the body x 0.45 (echo.inc), its coat
// and ground shadow x 0.30 (lighter than the body: the figure stays the darkest part, no dark ring round a pale one), the
// wisps x 0.55, the eyes x 0.70 (they keep it readable without blocking anything)
#define ECHO_CALM_COAT 0.30f
static float EchoCalmOp(float k, float lo) { return 1.0f - (1.0f - lo) * EchoEase(EchoSat(k)); }
// v3.33 ECHO333: the vision looks (every part see-through at all times; calm a little fainter: x 0.70 at k = 1)
#define ECHO_VIS_SPR_CALM 0.70f
#define ECHO_NV_COAT   0.50f    // night vision: the black coat at most this (see-through), phosphor edges / wisps / eyes
#define ECHO_SV_COAT   0.18f    // (v3.33's Spirit Vision coat: pale spectral, additive; v3.32 0.22 violet; unused since v3.34)
// v3.34 FX334: Spirit Vision's dark specters. The coat, wisps and teleport smoke are premultiplied near-black cold smoke; a
// faint cold rim: each coat tongue again 1.2 x wide and 1.06 x long, 8 u farther behind, premultiplied cold light (drawn
// first, far first: the dark tongue covers all of it but a fringe outside the coat), brighter while striking
#define ECHO_SV_SMOKE  0.62f    // the dark coat (x vis x the vision calm factor)
#define ECHO_SV_WISP   0.75f    // the dark wisps (x their own life alpha)
#define ECHO_SV_RIM    0.26f    // the cold rim (x 1.5 striking .. x 1 calm)
static const float kEchoSvSmoke[3] = { 0.020f, 0.024f, 0.045f };  // near-black, cold (premultiplied colour of the smoke)
static void EchoSvCold(float o[3], const float L[3])  // the cold rim colour: cold blue-white, 25 % of the light colour
{
    static const float kCold[3] = { 0.55f, 0.75f, 1.0f };
    int j;
    for (j = 0; j < 3; ++j) o[j] = 0.75f * kCold[j] + 0.25f * L[j];
}
#define ECHO_TH_COAT   0.34f    // Thermal: the coat as heat (additive dark red), its hot points 0.75 (yellow-white)
#define ECHO_TH_HOT    0.75f
static const float kEchoThCoat[3] = { 0.62f, 0.12f, 0.03f }, kEchoThHot[3] = { 1.0f, 0.80f, 0.34f },
                   kEchoThEye[3] = { 1.0f, 0.97f, 0.85f }, kEchoThGlow[3] = { 1.0f, 0.50f, 0.08f }, kEchoThWisp[3] = { 0.85f, 0.30f, 0.05f };
static const float kEchoNvPale[3] = { 0.80f, 0.88f, 0.80f };  // (night vision maps luminance to its green: a pale = phosphor)
static void EchoPale(float o[3], const float L[3], float k, float m)  // the light colour toward white by k, x m (Spirit Vision)
{
    int j;
    for (j = 0; j < 3; ++j) o[j] = EchoSat((L[j] + (1.0f - L[j]) * k) * m);
}
typedef struct { float p[3], tip[3], hw; } EchoTg;
// one tongue over the segment A -> B (A the inner end) of radius r (u), its visible tip `rise` over the upper end
static void EchoTongue(const float A[3], const float B[3], float r, float rise, float s, EchoTg *g)
{
    float d[3], L, T[3], len, need;
    const float *lo = A, *hi = B;
    int k;
    for (k = 0; k < 3; ++k) d[k] = B[k] - A[k];
    L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (fabsf(d[1]) >= 0.5f * L && d[1] < 0.0f) lo = A, hi = B;        // steep, B above A (Y down)
    else if (fabsf(d[1]) >= 0.5f * L) lo = B, hi = A;                   // steep, A above B
    memcpy(g->p, lo, sizeof g->p);
    memcpy(T, hi, sizeof T);
    if (L < 1e-3f) g->p[1] += r * s;  // (a tongue rising off a joint: its foot a radius below it)
    T[1] -= rise * s;
    g->hw = (r + ECHO_TG_MARGIN) * s / ECHO_TG_VIS;
    for (k = 0; k < 3; ++k) d[k] = (T[k] - g->p[k]) / ECHO_TG_TOP;
    len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    need = 1.84f * g->hw;
    if (len < need) {  // too short for its width: longer, upward if it has no direction of its own
        if (len > 1e-3f) for (k = 0; k < 3; ++k) d[k] *= need / len;
        else d[0] = d[2] = 0.0f, d[1] = -need;
    }
    for (k = 0; k < 3; ++k) g->tip[k] = g->p[k] + d[k];
}
static EchoSpr *EchoTgAdd(EchoSpr *o, int *n, int max, int add, const EchoTg *g, float kw, float kl, float a, const float rgb[3],
                          float push, int cell, float floorY)
{
    float tp[3];
    EchoSpr *s;
    int k;
    for (k = 0; k < 3; ++k) tp[k] = g->p[k] + (g->tip[k] - g->p[k]) * kl;
    s = EchoSprNew(o, n, max, ESK_FLAME, add, g->p, g->hw * kw, a, rgb);
    if (s) memcpy(s->tip, tp, sizeof s->tip), s->cell = (unsigned char)(cell % 28), s->push = push, s->floor = floorY, s->pad = 1;
    return s;
}
// the smoke of a teleport event at age a: puff j of ECHO_PUFF_N (bone, offset, radius, alpha)
static int EchoPuffAt(const EchoEvt *e, int j, float age, float p[3], float *r, float *al)
{
    static const unsigned char kPB[ECHO_PUFF_N] = { EB_HEAD, EB_CHEST, EB_CHEST, EB_PELVIS, EB_PELVIS, EB_LSH, EB_RSH, EB_LELB, EB_RELB,
                                                    EB_LHAND, EB_RHAND, EB_LHIP, EB_RHIP, EB_LKNEE, EB_RKNEE, EB_LFOOT, EB_RFOOT, EB_NECK,
                                                    EB_HEAD, EB_CHEST, EB_PELVIS, EB_LKNEE, EB_RKNEE, EB_CHEST };
    const EcU32 h = e->seed * 0x85EBCA6Bu + (EcU32)j * 0x27D4EB2Fu;
    const float th = 6.2831853f * EchoRnd(h), up = EchoLerp(-0.35f, 0.75f, EchoRnd(h + 1u));  // up: + = rising
    const float hz = sqrtf(1.0f - up * up), dir[3] = { hz * sinf(th), -up, hz * cosf(th) };
    const float *b = e->b[kPB[j]];
    int k;
    if (age < 0.0f) return 0;
    if (e->kind == 0) {  // out: bursts from the body (drag 0.22 s), rises, swells, thins
        const float life = ECHO_PUFF_OUT * EchoLerp(0.75f, 1.0f, EchoRnd(h + 2u)), f = age / life;
        const float v = EchoLerp(70.0f, 150.0f, EchoRnd(h + 3u)), s = v * 0.22f * (1.0f - expf(-age / 0.22f));
        if (f >= 1.0f) return 0;
        for (k = 0; k < 3; ++k) p[k] = b[k] + dir[k] * s;
        p[1] -= 40.0f * age + 30.0f * age * age;
        *r = EchoLerp(20.0f, 30.0f, EchoRnd(h + 4u)) * (1.0f + 1.2f * EchoSmooth(0.0f, 1.0f, f));
        *al = EchoSmooth(0.0f, 0.06f, f) * (1.0f - EchoSmooth(0.30f, 1.0f, f));
    } else {  // in: gathers onto the body from 55-85 u out over ECHO_TP_IN (ease-out), then thins on it
        const float R0 = EchoLerp(55.0f, 85.0f, EchoRnd(h + 3u)), u = EchoSat(age / ECHO_TP_IN), g = 1.0f - (1.0f - u) * (1.0f - u);
        if (age >= ECHO_PUFF_IN) return 0;
        for (k = 0; k < 3; ++k) p[k] = b[k] + dir[k] * R0 * (1.0f - g);
        p[1] -= 20.0f * age;
        *r = EchoLerp(26.0f, 36.0f, EchoRnd(h + 4u)) * (1.0f - 0.45f * g);
        *al = EchoSmooth(0.0f, 0.05f, age) * (1.0f - EchoSmooth(ECHO_TP_IN * 0.8f, ECHO_PUFF_IN, age));
    }
    return 1;
}
// The sprite list of one shadow (append to o; returns the new count). evts: its live smoke events.
// v3.32 ECHO332: the flame coat (black tongues along the body, a faint violet edge behind the torso, shoulders and head),
// fine purple wisps off its edges, the ground shadow, the eyes (a hot orb, a soft glow, a faint wide haze), the teleport
// smoke (10 puffs an event, 5 with a crest). At most ECHO_SPR_MAX for the shadow: while its smoke is thick the decoration
// (edges, wisps, the eyes' haze) fades out (no slot is taken by it then), so ground + eyes + tongues + two events fit.
static int EchoLookBuild(const EchoLookIn *in, const EchoEvt *evts, int nEvt, EchoSpr *o, int n, int max)
{
    const int sv = in->look == 3, th = in->look == 1, nv = in->look == 2, lite = in->lite != 0;  // v3.33 ECHO333: + th, nv
    const float vis = EchoSat(in->vis), t = in->t, *L = in->light, ph = (float)in->idx * 2.17f;
    const int cap = max < n + ECHO_SPR_MAX ? max : n + ECHO_SPR_MAX;
    const int fr = t > 0.0f ? (int)(t * ECHO_TG_FPS) : 0;
    // v3.33 ECHO333: under a vision filter every part is see-through at all times (calm x 0.70): the normal look's calm
    // opacities (coat 30 %, wisps 55 %, eyes 70 %) are the normal look's only
    const float opV = 1.0f - (1.0f - ECHO_VIS_SPR_CALM) * EchoEase(EchoSat(in->calm));
    const float opB = in->look ? opV : EchoCalmOp(in->calm, ECHO_CALM_COAT), opW = in->look ? opV : EchoCalmOp(in->calm, 0.55f),
                opE = in->look ? opV : EchoCalmOp(in->calm, 0.70f);
    float up[3], fw[3], rt[3], l, s, h, deco = 1.0f;
    int i, k;
    for (i = 0; i < nEvt; ++i) {  // smoke in the air: the decoration waits; it fades back in over 0.22 s once the event's
        // last puff is gone (an out puff lives at most ECHO_PUFF_OUT, an in puff ECHO_PUFF_IN)
        const float age = t - evts[i].t0, end = evts[i].kind ? ECHO_PUFF_IN : ECHO_PUFF_OUT, k2 = EchoSmooth(end, end + 0.22f, age);
        if (age >= 0.0f && k2 < deco) deco = k2;
    }
    for (k = 0; k < 3; ++k) up[k] = in->b[EB_HEAD][k] - in->b[EB_NECK][k];
    l = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (l > 1e-3f) for (k = 0; k < 3; ++k) up[k] /= l;
    else up[0] = up[2] = 0.0f, up[1] = -1.0f;
    {   // forward: its facing without its part along the head's up; right = up x forward (Y down: a left-handed look)
        const float d = in->face[0] * up[0] + in->face[1] * up[2];
        fw[0] = in->face[0] - d * up[0], fw[1] = -d * up[1], fw[2] = in->face[1] - d * up[2];
        l = sqrtf(fw[0] * fw[0] + fw[1] * fw[1] + fw[2] * fw[2]);
        if (l > 1e-3f) for (k = 0; k < 3; ++k) fw[k] /= l;
        else fw[0] = in->face[0], fw[1] = 0.0f, fw[2] = in->face[1];
        rt[0] = up[1] * fw[2] - up[2] * fw[1], rt[1] = up[2] * fw[0] - up[0] * fw[2], rt[2] = up[0] * fw[1] - up[1] * fw[0];
    }
    h = in->floorY - in->b[EB_HEAD][1];  // her height to the head bone (Y down)
    if (!(h > 120.0f)) h = 120.0f;
    if (h > 420.0f) h = 420.0f;
    s = h / 160.0f;  // (the sizes below are for a 160 u figure: Playable Eileen ~165, Henry ~170)
    if (vis > 0.004f) {
        const float blk[3] = { 0, 0, 0 };
        const EchoSegD *seg = lite ? kEchoSegLite : kEchoSeg;
        const int ns = lite ? ECHO_TG_LITE : ECHO_TG_N;
        EchoTg tg[ECHO_TG_N];
        float lean[2], hips;
        {
            const float *a = in->b[EB_LHIP], *b = in->b[EB_RHIP];
            hips = 0.5f * sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[2] - b[2]) * (a[2] - b[2])) / s;
            hips = EchoClampF(hips, 6.0f, 16.0f);
        }
        lean[0] = -0.08f * in->vel[0], lean[1] = -0.08f * in->vel[2];  // the tips lean back from its motion (at most 30 u)
        l = sqrtf(lean[0] * lean[0] + lean[1] * lean[1]);
        if (l > 30.0f * s) lean[0] *= 30.0f * s / l, lean[1] *= 30.0f * s / l;
        if (!sv && in->look == 0) {  // the soft ground shadow (a flat disc on its floor)
            const float g[3] = { 0.5f * (in->b[EB_LFOOT][0] + in->b[EB_RFOOT][0]), in->floorY - 2.0f, 0.5f * (in->b[EB_LFOOT][2] + in->b[EB_RFOOT][2]) };
            EchoSprNew(o, &n, cap, ESK_GROUND, 0, g, 70.0f * s, 0.55f * vis * opB, blk);
        }
        {   // the eyes: a hot orb, a soft glow round it and a faint wide haze (v3.32: a wider, softer alpha gradient), at the
            // front of the head on the head bone's own axes (its face hides them from behind)
            const float fl = 1.0f + 1.2f * EchoSat(in->flare);
            const float hx = in->head[0][0] * in->head[0][0] + in->head[0][1] * in->head[0][1] + in->head[0][2] * in->head[0][2];
            for (i = 0; i < 2; ++i) {
                float p[3], c[3], w[3];
                const float sd = i ? -ECHO_EYE_X : ECHO_EYE_X;
                if (hx > 0.25f) {
                    for (k = 0; k < 3; ++k)
                        p[k] = in->b[EB_HEAD][k] + in->head[0][k] * sd + in->head[1][k] * ECHO_EYE_Y + in->head[2][k] * ECHO_EYE_Z;
                } else {
                    for (k = 0; k < 3; ++k) p[k] = in->b[EB_HEAD][k] + fw[k] * 21.0f + rt[k] * sd + up[k] * 17.0f;
                }
                w[0] = EchoSat(0.70f + 0.30f * L[0]), w[1] = EchoSat(0.55f + 0.30f * L[1]), w[2] = EchoSat(0.75f + 0.25f * L[2]);
                if (th) memcpy(w, kEchoThEye, sizeof w);  // v3.33 ECHO333: Thermal: white-hot
                EchoSprNew(o, &n, cap, ESK_ORB, 1, p, 2.6f, vis * opE * (sv ? 0.9f : 1.0f) * (0.92f + 0.08f * sinf(31.0f * t + (float)i)), w);
                if (th) EchoScale3(c, kEchoThGlow, 0.80f);  // v3.33 ECHO333: Thermal orange heat, Spirit Vision pale
                else if (sv) EchoPale(c, L, 0.5f, 0.80f);
                else EchoScale3(c, L, 0.80f);
                EchoSprNew(o, &n, cap, ESK_SOFT, 1, p, ECHO_EYE_R * (0.85f + 0.15f * fl), vis * opE * 0.34f * fl * (nv ? 1.25f : 1.0f), c);
            }
            if (!lite)
                for (i = 0; i < 2; ++i) {  // (decoration: waits in thick smoke)
                    float p[3], c[3];
                    const float sd = i ? -ECHO_EYE_X : ECHO_EYE_X;
                    if (hx > 0.25f) {
                        for (k = 0; k < 3; ++k)
                            p[k] = in->b[EB_HEAD][k] + in->head[0][k] * sd + in->head[1][k] * ECHO_EYE_Y + in->head[2][k] * ECHO_EYE_Z;
                    } else {
                        for (k = 0; k < 3; ++k) p[k] = in->b[EB_HEAD][k] + fw[k] * 21.0f + rt[k] * sd + up[k] * 17.0f;
                    }
                    if (th) EchoScale3(c, kEchoThCoat, 1.0f);  // v3.33 ECHO333: Thermal dark red, Spirit Vision pale
                    else if (sv) EchoPale(c, L, 0.5f, 0.70f);
                    else EchoScale3(c, L, 0.70f);
                    EchoSprNew(o, &n, cap, ESK_SOFT, 1, p, ECHO_EYE_HAZE, vis * opE * deco * 0.11f * fl, c);
                }
        }
        // the coat: one tongue a segment (a dissolve makes it flare up as it fades), its tip swaying and leaning back from
        // its motion, its width breathing; each its own flipbook frame (15 fps)
        for (i = 0; i < ns; ++i) {
            const EchoSegD *d = &seg[i];
            const float r = d->r < 0.0f ? hips + 9.0f : d->r, q = ph + 1.37f * (float)i;
            EchoTongue(in->b[d->a], in->b[d->b], r, d->rise * (1.0f + 0.5f * (1.0f - vis)), s, &tg[i]);
            {
                float dl[3], len;
                for (k = 0; k < 3; ++k) dl[k] = tg[i].tip[k] - tg[i].p[k];
                len = sqrtf(dl[0] * dl[0] + dl[1] * dl[1] + dl[2] * dl[2]);
                tg[i].tip[0] += lean[0] * EchoSat(len / (120.0f * s)) + 0.05f * len * sinf(1.9f * t + q);
                tg[i].tip[2] += lean[1] * EchoSat(len / (120.0f * s)) + 0.05f * len * cosf(1.6f * t + 1.3f * q);
                tg[i].hw *= 1.0f + 0.04f * sinf(3.1f * t + q);
            }
            {
                const int cell = (int)(EchoHash(0xF1A3u + (EcU32)in->idx * 31u + (EcU32)i * 977u) % 28u) + fr;
                if (sv) {  // Spirit Vision: v3.34 FX334 dark smoke (v3.33: pale spectral light, additive 0.18; v3.32: violet)
                    EchoTgAdd(o, &n, cap, 0, &tg[i], 1.0f, 1.0f, ECHO_SV_SMOKE * vis * opB, kEchoSvSmoke, d->push, cell, in->floorY);
                } else if (th) {  // v3.33 ECHO333: Thermal: the coat burns (additive dark red heat)
                    EchoTgAdd(o, &n, cap, 1, &tg[i], 1.0f, 1.0f, ECHO_TH_COAT * vis * opB, kEchoThCoat, d->push, cell, in->floorY);
                } else {  // (night vision: see-through, at most ECHO_NV_COAT)
                    EchoTgAdd(o, &n, cap, 0, &tg[i], 1.0f, 1.0f, ECHO_TG_BLACK * vis * opB * (nv ? ECHO_NV_COAT : 1.0f), blk, d->push, cell, in->floorY);
                }
            }
        }
        if (th)  // v3.33 ECHO333: Thermal: a hot point in every tongue (yellow-white, flickering; decoration: waits in smoke)
            for (i = 0; i < ns; ++i) {
                const float q = ph + 1.37f * (float)i, u = ECHO_TG_TOP * (0.42f + 0.22f * EchoRnd2(0x7C3u + (EcU32)in->idx * 31u, (EcU32)i));
                float p[3];
                for (k = 0; k < 3; ++k) p[k] = tg[i].p[k] + (tg[i].tip[k] - tg[i].p[k]) * u;
                EchoSprNew(o, &n, cap, ESK_SOFT, 1, p, s * (2.5f + 1.5f * EchoRnd2(0x5E1u + (EcU32)in->idx * 17u, (EcU32)i)),
                           ECHO_TH_HOT * vis * opB * deco * (0.70f + 0.30f * sinf(9.1f * t + 2.3f * q)), kEchoThHot);
            }
        if (sv)  // v3.34 FX334: Spirit Vision: the faint cold rim behind every tongue (decoration: waits in smoke; kept in the light look)
            for (i = 0; i < ns; ++i) {
                float c[3];
                const float pulse = 0.85f + 0.15f * sinf(5.3f * t + ph + 1.7f * (float)i), strike = 1.0f + 0.5f * (1.0f - EchoEase(EchoSat(in->calm)));
                const int cell = (int)(EchoHash(0x5C0u + (EcU32)in->idx * 7u + (EcU32)i) % 28u) + fr;
                EchoSvCold(c, L);
                EchoTgAdd(o, &n, cap, 0, &tg[i], 1.20f, 1.06f, ECHO_SV_RIM * strike * vis * opB * deco * pulse, c, seg[i].push + 8.0f, cell, in->floorY);
            }
        if (!sv && !th && !lite)  // the faint violet edge behind the torso, the shoulders and the head (the outline in a dark room)
            for (i = 0; i < 4; ++i) {
                float c[3];
                const int si = i;  // (ESG_TORSO, ESG_LSHO, ESG_RSHO, ESG_HEAD)
                const float pulse = 0.85f + 0.15f * sinf(7.3f * t + ph + (float)i);
                const int cell = (int)(EchoHash(0xE0Au + (EcU32)in->idx * 7u + (EcU32)i) % 28u) + fr;
                if (nv) {  // v3.33 ECHO333: night vision: a pale additive edge (the filter turns it into a phosphor outline)
                    EchoTgAdd(o, &n, cap, 1, &tg[si], 1.20f, 1.06f, 0.12f * vis * opB * deco * pulse, kEchoNvPale, seg[si].push + 8.0f, cell, in->floorY);
                    continue;
                }
                EchoScale3(c, L, 1.0f);
                EchoTgAdd(o, &n, cap, 0, &tg[si], 1.20f, 1.06f, ECHO_TG_EDGE * vis * opB * deco * pulse, c, seg[si].push + 8.0f, cell, in->floorY);
            }
        {   // fine purple wisps off the coat's edges: each a soft S-shaped strand of two streaks (ESK_WISP), on its own hashed
            // life (0.8-1.2 s): born on a segment's side on the coat's edge (mostly her right / left: the edges a camera behind or
            // beside her sees), rising and drifting out, its curl growing, thinning out at the top, gone
            const int nw = lite ? 3 : ECHO_WISP_N;
            for (i = 0; i < nw; ++i) {
                const float life = EchoLerp(0.80f, 1.20f, EchoRnd2(0x3157u + (EcU32)in->idx * 31u, (EcU32)i));
                const float tt = t / life + EchoRnd2(0x77A1u + (EcU32)in->idx, (EcU32)i);
                const float cy = floorf(tt), f = tt - cy;
                const EcU32 hh = EchoHash((EcU32)(int)cy * 0x9E3779B9u ^ (EcU32)i * 0x85EBCA6Bu ^ (EcU32)in->idx * 0xC2B2AE35u);
                const int si = kEchoWispSeg[hh % 12u];
                const EchoSegD *d = &kEchoSeg[si];
                const float side = EchoRnd(hh + 2u) < 0.5f ? -1.0f : 1.0f, mix = EchoLerp(-0.6f, 0.6f, EchoRnd(hh + 3u));
                const float r = d->r < 0.0f ? hips + 9.0f : d->r, u = EchoRnd(hh + 1u), sw = 6.2831853f * EchoRnd(hh + 4u);
                const float a = ECHO_WISP_A * vis * opW * deco * EchoSmooth(0.0f, 0.20f, f) * (1.0f - EchoSmooth(0.40f, 1.0f, f));
                float P[3], dir[3], p[3], m[3], tp[3], dl, len, hw, c[3], cu;
                if (!(a > 0.004f)) continue;
                for (k = 0; k < 3; ++k) {
                    P[k] = in->b[d->a][k] + (in->b[d->b][k] - in->b[d->a][k]) * u;
                    dir[k] = rt[k] * side + fw[k] * mix;
                }
                dir[1] = 0.0f;
                dl = sqrtf(dir[0] * dir[0] + dir[2] * dir[2]);
                if (dl > 1e-3f) dir[0] /= dl, dir[2] /= dl;
                if (si == ESG_HEAD) P[1] -= 20.0f * s;  // (off the top of the head)
                if (si == ESG_LSHO || si == ESG_RSHO) P[1] -= 6.0f * s;
                for (k = 0; k < 3; ++k) P[k] += dir[k] * (r + 0.8f * ECHO_TG_MARGIN) * s;  // (on the coat's edge)
                // rising (accelerating), drifting out and back from its motion
                for (k = 0; k < 3; ++k) p[k] = P[k] + dir[k] * 7.0f * s * f;
                p[1] -= s * (4.0f + 24.0f * f + 12.0f * f * f);
                p[0] += lean[0] * 0.8f * f, p[2] += lean[1] * 0.8f * f;
                len = s * (14.0f + 20.0f * sinf(3.1415927f * f));
                hw = s * (2.0f + 1.0f * sinf(3.1415927f * f));
                cu = s * (3.0f + 7.0f * f) * sinf(6.2831853f * 0.9f * f + sw);  // the S curl, across its rise, growing
                for (k = 0; k < 3; ++k) m[k] = p[k] + dir[k] * 0.15f * len;
                m[1] -= 0.5f * len;
                m[0] += -dir[2] * cu, m[2] += dir[0] * cu;
                for (k = 0; k < 3; ++k) tp[k] = m[k] + dir[k] * 0.20f * len;
                tp[1] -= 0.5f * len;
                tp[0] -= -dir[2] * cu * 0.8f, tp[2] -= dir[0] * cu * 0.8f;
                // v3.33 ECHO333: Spirit Vision pale, Thermal rising heat (orange), night vision pale filaments (phosphor):
                // additive light in every vision look; the normal look's purple stays premultiplied
                const int lit = in->look != 0 && !sv;  // v3.34 FX334: Spirit Vision's wisps are dark smoke (premultiplied)
                const float aw = sv ? ECHO_SV_WISP * a : nv ? 0.55f * a : th ? 0.45f * a : a;
                if (sv) memcpy(c, kEchoSvSmoke, sizeof c);
                else if (th) memcpy(c, kEchoThWisp, sizeof c);
                else if (nv) memcpy(c, kEchoNvPale, sizeof c);
                else c[0] = EchoSat(0.30f + 0.40f * L[0]), c[1] = EchoSat(0.04f + 0.30f * L[1]), c[2] = EchoSat(0.20f + 0.55f * L[2]);
                {
                    EchoSpr *q = EchoSprNew(o, &n, cap, ESK_WISP, lit, p, hw, aw, c);
                    if (q) memcpy(q->tip, m, sizeof q->tip), q->push = 3.0f;
                    q = EchoSprNew(o, &n, cap, ESK_WISP, lit, m, 0.75f * hw, aw * 0.65f, c);  // (thinner, fainter: it thins out)
                    if (q) memcpy(q->tip, tp, sizeof q->tip), q->push = 3.0f;
                }
            }
        }
    }
    for (i = 0; i < nEvt; ++i) {  // its teleport smoke (also while it is unseen: the smoke outlives the shape)
        const float age = t - evts[i].t0;
        const int np = lite ? 5 : 10;
        int j;
        for (j = 0; j < np; ++j) {
            float p[3], r, al, c[3];
            const int pj = lite ? kEchoPuffLite[j] : kEchoPuffUse[j], cell = j & 1;
            if (!EchoPuffAt(&evts[i], pj, age, p, &r, &al)) continue;
            r *= 1.15f;  // (fewer puffs, a little larger)
            if (sv) {  // v3.34 FX334: dark smoke, a cold crest on every other puff (v3.33: pale soft light; v3.32: violet x 0.45)
                EchoSprNew(o, &n, cap, cell ? ESK_SMOKE1 : ESK_SMOKE0, 0, p, r, 0.80f * al, kEchoSvSmoke);
                if (!(j & 1)) {
                    EchoSvCold(c, L);
                    EchoScale3(c, c, 0.35f);
                    EchoSprNew(o, &n, cap, ESK_CREST, 1, p, r * 1.05f, 0.80f * al, c);
                }
            } else {
                const float blk[3] = { 0, 0, 0 };
                EchoSprNew(o, &n, cap, cell ? ESK_SMOKE1 : ESK_SMOKE0, 0, p, r, (th ? 0.70f : 0.85f) * al, blk);  // (Thermal: cold smoke)
                if (!(j & 1)) {  // a violet crest on every other one: the smoke reads in a dark room too (Thermal: embers)
                    if (th) EchoScale3(c, kEchoThWisp, 0.50f);
                    else EchoScale3(c, L, nv ? 0.50f : 0.45f);
                    EchoSprNew(o, &n, cap, ESK_CREST, 1, p, r * 1.05f, 0.90f * al, c);
                }
            }
        }
    }
    return n;
}
#endif
