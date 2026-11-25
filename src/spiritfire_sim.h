// ================================================================ v3.28 FIRE: Spirit Vision's spirit fires, the pure code
// staging/v328_spirit/b_fire/DESIGN.md. Pure C (no D3D, no game memory): included by spiritfire.inc (the game side) and
// by the native test test/sf_sim_test.c, which also dumps the atlas and the sprite lists tools/flame_preview.py renders
// (the preview draws what this header produces: no second port to drift).
// World units, y DOWN (the game's convention: up = -y). Real seconds. Every look value is a pure function of the state
// and the time: agitation / flares are closed-form impulse sums, the death timeline a function of the age, particles
// closed-form in their age, spawned from exact accumulators (back-dated inside the frame). Only the flipbook phase is
// integrated (rate x dt). The same life at 30 / 60 / 144 Hz gives the same fire (the native test checks).
#ifndef SF_SIM_H
#define SF_SIM_H
#include <math.h>
#include <string.h>

typedef unsigned int SfU32;

enum { SFK_MONSTER, SFK_WALTER, SFK_PERSON, SFK_N };  // palettes (the player is a person)
#define SF_H          72.0f   // the flame's height at size 1 (world units; was SV_FLAME_H)
#define SF_FIRES      32      // = SV_SOULS
#define SF_PARTS      512     // embers, sparks, falling embers, smoke puffs (all fires)
#define SF_SPR_MAX    1024    // sprites a frame (bodies 8 a fire + the particles)
#define SF_FRAMES     28      // flipbook frames: cells 0..27 of the 8 x 4 atlas of 64 x 128 texels
#define SF_TEX        512     // the atlas (512 x 512)
#define SF_LOOP_S     1.4f    // one flipbook loop at rest (20 frames a second, crossfaded)
#define SF_HP_TAU     0.35f   // health smoothing (seconds)
#define SF_HP_MIN     0.40f   // the fire's size at 0 health (x its full size)
#define SF_WOUNDED    0.35f   // below this health the fire sputters
#define SF_DIE_HP     0.45f   // a dying fire starts from at least this health's size (the drama needs room)
#define SF_AG_TAU     0.55f   // agitation ("going wild") decay
#define SF_AG_MAX     1.6f
#define SF_AG_HIT     0.9f    // agitation per hit (x its strength)
#define SF_AG_DRAIN   6.0f    // agitation per fraction of max HP drained without a hit (Death Presence, holy damage)
#define SF_HIT_DROP   0.015f  // a drop of at least this (fraction of max HP) in one sample is a hit
#define SF_DRAIN_MIN  0.0005f // below this a drop is noise
#define SF_HIT_MIN    0.35f   // hit strength = MIN + K x drop, at most 1
#define SF_HIT_K      3.0f
#define SF_FL_UP      0.02f   // the hit flare: 20 ms attack ...
#define SF_FL_TAU     0.12f   // ... 120 ms decay
#define SF_END_S      2.4f    // the fixed ending of a going-out (flare, collapse, coal, smoke); the rest gutters
#define SF_SMOKE_N    16      // smoke puffs of the wisp
#define SF_FALL_N     6       // embers falling when the fire goes out
#define SF_FLARE_N    5       // embers of the last flare
#define SF_UNSEEN_S   0.5f    // a living fire not seen this long is forgotten
// atlas sub-cells (64 x 64, in cells 28..30), numbered after the frames
enum { SFC_EMBER = SF_FRAMES, SFC_GLOW, SFC_SPARK, SFC_COAL, SFC_SMOKE0, SFC_SMOKE1, SFC_END };
enum { SFP_NONE, SFP_EMBER, SFP_SPARK, SFP_FALL, SFP_SMOKE };

// ---------------------------------------------------------------- hashes and noise (FxHash's constants)
static SfU32 SfHash(SfU32 a)
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float SfRnd(SfU32 a) { return (float)(SfHash(a) & 0xFFFFFFu) / 16777216.0f; }  // 0..1
static float SfSat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float SfSmooth(float e0, float e1, float x)
{
    const float t = SfSat((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
static float SfMix(float a, float b, float t) { return a + (b - a) * t; }
// smooth 0..1 value noise; py > 0: periodic in y with period py (the flipbook loops)
static float SfNoiseP(float x, float y, int py, SfU32 seed)
{
    const int ix = (int)floorf(x), iy = (int)floorf(y);
    const float fx = x - (float)ix, fy = y - (float)iy;
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    int y0 = iy, y1 = iy + 1;
    SfU32 hx0, hx1, hy0, hy1;
    float a, b, c, d;
    if (py > 0) {
        y0 = (y0 % py + py) % py;
        y1 = (y1 % py + py) % py;
    }
    hx0 = (SfU32)ix * 73856093u, hx1 = (SfU32)(ix + 1) * 73856093u;
    hy0 = (SfU32)y0 * 19349663u, hy1 = (SfU32)y1 * 19349663u;
    a = SfRnd(seed ^ hx0 ^ hy0), b = SfRnd(seed ^ hx1 ^ hy0), c = SfRnd(seed ^ hx0 ^ hy1), d = SfRnd(seed ^ hx1 ^ hy1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}
static float SfNoiseT(float t, SfU32 seed) { return 2.0f * SfNoiseP(t, 0.37f, 0, seed) - 1.0f; }  // -1..1, in time
static float SfFbm2(float x, float y, int py, SfU32 seed)
{
    return (SfNoiseP(x, y, py, seed) + 0.5f * SfNoiseP(2.0f * x + 5.3f, 2.0f * y, 2 * py, seed + 77u)) / 1.5f;
}
static float SfFbm4(float x, float y, SfU32 seed)
{
    float s = 0.0f, t = 0.0f, a = 0.5f;
    int o;
    for (o = 0; o < 4; ++o) {
        s += a * SfNoiseP(x, y, 0, seed + (SfU32)o * 101u);
        t += a;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 9.7f;
        a *= 0.5f;
    }
    return s / t;
}

// ---------------------------------------------------------------- the atlas (premultiplied grey, baked once)
// Frame f of the flame loop (tau = f / SF_FRAMES): a teardrop body (round foot, thin tip) warped by y-periodic noise
// advected upward by a whole number of its periods per loop (seamless), eroded from the top into licking tongues.
// cu 0..1 across, cv 0..1 down the 64 x 128 cell.
static float SfFlameTexel(float cu, float cv, int f)
{
    static float s_prof[128];  // the width profile per row (round foot, thin tip): rows only
    static int s_profOk;
    const float tau = (float)f / (float)SF_FRAMES, u = 2.0f * cu - 1.0f, v = 1.0f - cv;  // v 0 at the foot, 1 at the top
    const int row = (int)(cv * 128.0f);
    float w1, n1, n2, n3, uw, prof, d, body, tongue, a;
    if (!s_profOk) {
        int r;
        for (r = 0; r < 128; ++r) {
            const float rv = 1.0f - ((float)r + 0.5f) / 128.0f, vv = SfSat(rv * 0.96f + 0.02f);
            s_prof[r] = 0.95f * powf(sinf(3.14159265f * powf(vv, 0.6f)), 0.6f) * (1.0f - 0.5f * rv);  // a round foot
        }
        s_profOk = 1;
    }
    w1 = SfNoiseP(u * 1.0f + 3.1f, v * 1.5f - tau * 3.0f, 3, 0x5F11u);   // slow sway (1 period a loop)
    n1 = SfFbm2(u * 2.0f + 7.0f, v * 2.0f - tau * 4.0f, 4, 0x5F12u);     // body turbulence
    n2 = SfFbm2(u * 3.5f + 1.7f, v * 3.0f - tau * 6.0f, 6, 0x5F13u);     // the tongues
    n3 = SfNoiseP(u * 6.0f + 2.3f, v * 5.0f - tau * 10.0f, 10, 0x5F14u); // detail
    uw = u + (w1 - 0.5f) * 0.70f * v + (n1 - 0.5f) * 0.60f * powf(v, 1.2f);
    prof = s_prof[row < 0 ? 0 : row > 127 ? 127 : row];
    d = prof > 0.01f ? fabsf(uw) / prof : 9.0f;
    body = 1.0f - SfSmooth(0.35f, 1.0f, d);
    tongue = SfSmooth(0.0f, 0.25f, 0.80f * n2 + 0.35f * n3 + 0.62f - 1.15f * v - 0.35f * d);  // eroded from the top
    a = body * tongue * SfSmooth(0.0f, 0.16f, v) * (0.55f + 0.45f * n1) * (0.80f + 0.55f * (1.0f - v) * SfSat(1.0f - d));
    a *= SfSmooth(0.01f, 0.08f, cu) * SfSmooth(0.01f, 0.08f, 1.0f - cu) * SfSmooth(0.005f, 0.04f, cv) * SfSmooth(0.005f, 0.04f, 1.0f - cv);
    return SfSat(a / 1.2f);
}
// the 64 x 64 sub-cells: 0 ember, 1 glow, 2 spark, 3 coal, 4 / 5 smoke puffs (0 at the border: no bleeding)
static float SfSmallTexel(int s, float cu, float cv)
{
    const float du = cu - 0.5f, dv = cv - 0.5f, r = sqrtf(du * du + dv * dv), edge = 1.0f - SfSmooth(0.36f, 0.48f, r);
    float a;
    switch (s) {
    case 0:  // ember: a hot point and a small halo
        a = expf(-(r / 0.09f) * (r / 0.09f)) + 0.35f * expf(-(r / 0.20f) * (r / 0.20f));
        break;
    case 1: {  // glow: a soft radial
        const float q = 1.0f - SfSmooth(0.0f, 0.5f, r);
        a = q * q;
        break;
    }
    case 2:  // spark: a four-point glint
        a = expf(-(r / 0.05f) * (r / 0.05f)) + 0.15f * expf(-(r / 0.16f) * (r / 0.16f)) +
            0.55f * expf(-fabsf(du) / 0.014f) * expf(-fabsf(dv) / 0.16f) + 0.55f * expf(-fabsf(dv) / 0.014f) * expf(-fabsf(du) / 0.16f);
        break;
    case 3: {  // coal: a lumpy glowing ember
        const float n = SfNoiseP(cu * 6.0f, cv * 6.0f, 0, 0xC0A1u), rr = r * (1.0f + 0.5f * (n - 0.5f));
        a = (1.0f - SfSmooth(0.10f, 0.24f, rr)) * (0.65f + 0.35f * SfNoiseP(cu * 11.0f, cv * 11.0f, 0, 0xC0A2u)) +
            0.30f * expf(-(r / 0.22f) * (r / 0.22f));
        break;
    }
    case 4: case 5: {  // smoke puff: fBm with a lumpy outline
        const float f = SfFbm4(cu * 3.5f, cv * 3.5f, 0x5A0Cu + (SfU32)s * 131u);
        const float lo = SfFbm2(cu * 1.6f + 4.0f, cv * 1.6f, 0, 0x5A0Du + (SfU32)s * 131u);
        const float rr = r * (1.0f + 1.1f * (lo - 0.5f) + 0.4f * (f - 0.5f));
        a = 0.95f * SfSat((f - 0.22f) * 2.0f) * (1.0f - SfSmooth(0.06f, 0.44f, rr));
        break;
    }
    default:
        a = 0.0f;
        break;
    }
    return SfSat(a * edge);
}
static float SfAtlasTexel(int x, int y)  // 0..1 (texel x, y of the 512 x 512 atlas)
{
    const int c = (x >> 6) + 8 * (y >> 7);
    int s;
    if (c < SF_FRAMES) return SfFlameTexel(((float)(x & 63) + 0.5f) / 64.0f, ((float)(y & 127) + 0.5f) / 128.0f, c);
    s = (c - SF_FRAMES) * 2 + ((y & 127) >= 64);
    if (s >= SFC_END - SFC_EMBER) return 0.0f;
    return SfSmallTexel(s, ((float)(x & 63) + 0.5f) / 64.0f, ((float)(y & 63) + 0.5f) / 64.0f);
}
static void SfCellUv(int cell, float uv[4])  // u0, v0, u1, v1 (half a texel inside)
{
    float x0, y0, w, h;
    if (cell < SF_FRAMES) {
        x0 = (float)((cell & 7) * 64), y0 = (float)((cell >> 3) * 128), w = 64.0f, h = 128.0f;
    } else {
        const int s = cell - SF_FRAMES, c = SF_FRAMES + s / 2;
        x0 = (float)((c & 7) * 64), y0 = (float)((c >> 3) * 128 + (s & 1) * 64), w = h = 64.0f;
    }
    uv[0] = (x0 + 0.5f) / (float)SF_TEX, uv[1] = (y0 + 0.5f) / (float)SF_TEX;
    uv[2] = (x0 + w - 0.5f) / (float)SF_TEX, uv[3] = (y0 + h - 0.5f) / (float)SF_TEX;
}

// ---------------------------------------------------------------- palettes
typedef struct { float layer[3][3], ember[3], coal[3]; float emRate, calm; } SfPal;  // layers: outer, mid, core
static const SfPal kSfPal[SFK_N] = {
    { { { 0.85f, 0.20f, 0.05f }, { 1.00f, 0.55f, 0.12f }, { 1.00f, 0.90f, 0.58f } }, { 1.00f, 0.55f, 0.16f }, { 1.00f, 0.22f, 0.04f },
      4.5f, 1.0f },  // monsters: crimson-orange tips, orange body, pale yellow core
    { { { 0.75f, 0.00f, 0.02f }, { 0.95f, 0.07f, 0.04f }, { 1.00f, 0.45f, 0.30f } }, { 1.00f, 0.20f, 0.08f }, { 0.90f, 0.05f, 0.02f },
      5.0f, 1.0f },  // Walter: blood red
    { { { 0.90f, 0.62f, 0.18f }, { 1.00f, 0.85f, 0.50f }, { 1.00f, 1.00f, 0.95f } }, { 1.00f, 0.85f, 0.45f }, { 1.00f, 0.45f, 0.10f },
      2.5f, 0.6f } };  // people and the player: white-gold, calmer
static const float kSfCool[3][3] = { { 0.50f, 0.05f, 0.02f }, { 0.78f, 0.18f, 0.04f }, { 0.95f, 0.42f, 0.14f } };  // a dying fire
static const float kSfSmoke[3] = { 0.62f, 0.65f, 0.70f };  // pale cold grey (premultiplied)
static const float kSfLayerSc[3] = { 1.35f, 1.0f, 0.55f }, kSfLayerK[3] = { 0.60f, 0.80f, 0.95f };
static const float kSfLayerOff[3] = { 0.0f, 0.37f, 0.71f }, kSfLayerLean[3] = { 1.0f, 0.8f, 0.5f };
#define SF_K 0.90f  // overall strength (SV_FLAME_K)

// ---------------------------------------------------------------- the going-out timeline (a pure function of the age)
// D = SoulFadeSeconds. The last SF_END_S seconds are fixed (flare 0.3 + 0.2, collapse 0.2, coal 0.6, smoke to the end);
// before them the fire gutters. A short D (under SF_END_S / 0.7) compresses the ending by e and gutters 0.3 D.
typedef struct {
    float sizeW, sizeH, bright, cool, speed, sputter, coal, emK;
    float tFlare, tOut, tSmoke0, tSmokeDt, e, G;
    int phase;  // 0 gutter (or alive), 1 last flare, 2 collapse, 3 out
} SfDie;
static void SfDieEval(float age, float D, SfDie *o)
{
    float G, e, tFp, tC0, tOut, tCoal;
    if (!(D >= 0.1f)) D = 0.1f;  // (0 = at once is the caller's: no soul is added)
    if (D >= SF_END_S / 0.7f) G = D - SF_END_S, e = 1.0f;
    else G = 0.3f * D, e = 0.7f * D / SF_END_S;
    tFp = G + 0.30f * e, tC0 = G + 0.50f * e, tOut = G + 0.70f * e, tCoal = tOut + 0.60f * e;
    o->G = G, o->e = e, o->tFlare = tFp - 0.05f * e, o->tOut = tOut, o->tSmoke0 = tOut - 0.05f * e;
    o->tSmokeDt = 0.70f * e / (float)SF_SMOKE_N;
    o->sizeW = o->sizeH = o->bright = o->speed = o->emK = 1.0f;
    o->cool = o->sputter = o->coal = 0.0f;
    o->phase = 0;
    if (!(age >= 0.0f)) return;
    if (age < G) {  // 1. gutter: shrinks to 28 %, dims to 55 %, cools, slows, sputters harder and harder
        const float g = age / G, s = 1.0f - 0.72f * powf(g, 1.3f);
        o->sizeW = o->sizeH = s;
        o->bright = 1.0f - 0.35f * g;
        o->cool = powf(g, 1.5f);
        o->speed = 1.0f - 0.3f * g;
        o->sputter = g;
        o->emK = 1.0f - 0.8f * g;
    } else if (age < tC0) {  // 2. the last flare: back up to 62 %, then down to 45 %
        float s, b;
        if (age < tFp) {
            b = SfSmooth(G, tFp, age);
            s = 0.28f + 0.34f * b;
        } else {
            const float y = (age - tFp) / (tC0 - tFp);
            b = 1.0f - y;
            s = 0.62f - 0.17f * y;
        }
        o->sizeW = o->sizeH = s;
        o->bright = 0.55f + 0.75f * b;
        o->cool = 1.0f - 0.55f * b;
        o->speed = 0.7f + 0.8f * b;
        o->sputter = 0.25f * (1.0f - b);
        o->emK = 0.2f + 1.3f * b;
        o->phase = 1;
    } else if (age < tOut) {  // 3. collapse: the tongues sink into the base (height first, width later)
        const float c = (age - tC0) / (tOut - tC0);
        o->sizeH = 0.45f * (1.0f - c) * (1.0f - c);
        o->sizeW = 0.45f * (1.0f - 0.8f * c * c);
        o->bright = 0.55f * powf(1.0f - c, 1.2f);
        o->cool = 1.0f;
        o->speed = 0.7f;
        o->sputter = 0.4f;
        o->emK = 0.2f * (1.0f - c);
        o->coal = SfSmooth(0.0f, 1.0f, c);
        o->phase = 2;
    } else {  // 4. out: a coal winks out; the smoke and the falling embers are particles
        o->sizeW = o->sizeH = o->bright = o->emK = 0.0f;
        o->cool = 1.0f;
        o->speed = 0.7f;
        o->coal = 1.0f - SfSmooth(tOut, tCoal, age);
        o->phase = 3;
    }
}
static float SfHealthSize(float h) { return SF_HP_MIN + (1.0f - SF_HP_MIN) * SfSat(h); }

// ---------------------------------------------------------------- the state
typedef struct {
    SfU32 key;        // 0: no being this frame (unused)
    int kind;         // SFK_*
    float size;       // 1 monsters, 2.2 Walter, 0.8 people
    float dieS;       // SoulFadeSeconds
    float o[3];       // the chest (world)
    float hp;         // health 0..1; < 0 unknown (full)
    float drop;       // the fraction of max HP lost since the last sample (>= 0)
    int hit;          // a new hit record (enemies) this sample
    float deadAge;    // seconds since death (EnemyIsDead's clock); < 0 alive
    int deadNow;      // dead, the age unknown (the player at 0 HP): the fire's own clock starts
} SfIn;
typedef struct {
    SfU32 key;        // 0 free
    int kind, dying, ev, smokeN, live;
    float size, seed, dieS, o[3], tSeen, tLast;
    float hpK, agA0, agT, flS, flT, tDeath, hDie0, phase, rate, emAcc;
    SfU32 nEmber, nHit;
    // the game side's last health sample (spiritfire.inc): fraction, damage counter, hit serial
    float gHp, gDmg;
    int gSer, gHave;
} SfFire;
typedef struct {
    int type, kind, cell;
    float t0, life, p0[3], v0[3], grav, drag, size, k, wob, ph;
} SfPart;
typedef struct {
    SfFire fire[SF_FIRES];
    SfPart part[SF_PARTS];
    int next;
} SfWorld;
// one sprite (world space): cell < SF_FRAMES: a flame quad anchored at p (30 % of ry below, 70 % above), rx the half
// width, the top edge shifted by lean x ry across the screen, mirror flips u; else a round sprite centred at p, radius
// rx (= ry), rotated by rot. add 1 additive, 0 premultiplied. minPx: the least screen size (720-line pixels).
typedef struct {
    float p[3], rx, ry, lean, rot, rgb[3], a, minPx;
    int cell, add, mirror, fire;
} SfSpr;

static float SfAg(const SfFire *F, float now)
{
    const float t = now - F->agT;
    return F->agA0 > 0.0f ? F->agA0 * expf(-(t > 0.0f ? t : 0.0f) / SF_AG_TAU) : 0.0f;
}
static void SfAgitate(SfFire *F, float now, float add)
{
    float a = SfAg(F, now) + add;
    F->agA0 = a > SF_AG_MAX ? SF_AG_MAX : a;
    F->agT = now;
}
static float SfFlareV(const SfFire *F, float now)
{
    const float t = now - F->flT;
    if (!(t >= 0.0f) || F->flS <= 0.0f) return 0.0f;
    return F->flS * (1.0f - expf(-t / SF_FL_UP)) * expf(-t / SF_FL_TAU);
}

typedef struct { float H, W, bright, lean, fl, ag, cool, speed, hS, Href; SfDie die; } SfLookV;
static void SfLook(const SfFire *F, float now, SfLookV *L)
{
    const SfPal *P = &kSfPal[F->kind];
    const float calm = P->calm, sd = F->seed, ag = SfAg(F, now), fl = SfFlareV(F, now);
    const SfU32 k = F->key;
    const float n5 = SfNoiseT(now * 5.0f + sd, k + 3u), n6 = SfNoiseT(now * 6.0f + sd, k + 1u), n15 = SfNoiseT(now * 15.0f + sd, k + 2u);
    const float n7 = SfNoiseT(now * 7.0f + sd, k + 8u), n11 = SfNoiseT(now * 11.0f + sd, k + 5u), n24 = SfNoiseT(now * 24.0f + sd, k + 4u);
    const float n9 = SfNoiseT(now * 9.0f + sd, k + 6u), n23 = SfNoiseT(now * 2.3f + sd, k + 7u), nd = SfNoiseT(now * 7.0f + sd, k + 9u);
    float sput, dip;
    SfDieEval(F->dying ? now - F->tDeath : -1.0f, F->dieS, &L->die);
    L->hS = SfHealthSize(F->hpK);
    L->Href = SF_H * F->size * L->hS;
    sput = F->dying ? 0.0f : 0.7f * SfSat(1.0f - F->hpK / SF_WOUNDED);  // a wounded fire struggles
    if (L->die.sputter > sput) sput = L->die.sputter;
    dip = SfSmooth(0.0f, 0.6f, nd) * sput;  // a sputter: random dips of brightness and height
    L->ag = ag, L->fl = fl;
    L->H = L->Href * (1.0f + 0.45f * fl) * L->die.sizeH * (1.0f + calm * 0.08f * n5 + ag * 0.30f * n11) * (1.0f - 0.35f * dip);
    L->W = 0.55f * L->Href * (1.0f + 0.45f * fl) * L->die.sizeW * (1.0f + 0.04f * n7);
    L->bright = (0.75f + 0.25f * F->hpK) * (1.0f + calm * (0.10f * n6 + 0.06f * n15) + ag * 0.35f * n24) * (1.0f + 0.9f * fl) *
                L->die.bright * (1.0f - 0.6f * dip);
    if (L->bright < 0.0f) L->bright = 0.0f;
    L->lean = calm * (0.06f * sinf(now * 4.7f + sd) + 0.05f * n23) + ag * 0.45f * n9;
    L->cool = L->die.cool;
    L->speed = (1.0f + 1.5f * ag) * L->die.speed;
}

// ---------------------------------------------------------------- particles
static SfPart *SfPartNew(SfWorld *W)
{
    int i;
    for (i = 0; i < SF_PARTS; ++i) {
        SfPart *q = &W->part[(W->next + i) % SF_PARTS];
        if (q->type == SFP_NONE) {
            W->next = (W->next + i + 1) % SF_PARTS;
            return q;
        }
    }
    i = W->next;  // full: the oldest slot of the ring
    W->next = (W->next + 1) % SF_PARTS;
    return &W->part[i];
}
// one particle of a stream: seed = hash(key, stream, index); spawned at time t0 at the fire's anchor (o) with the
// fire's reference height Href
static void SfSpawn(SfWorld *W, const SfFire *F, int type, SfU32 stream, SfU32 idx, float t0, const float base[3], float Href, float e)
{
    SfPart *q = SfPartNew(W);
    const SfU32 s = SfHash(F->key * 0x9E3779B9u ^ stream * 0x85EBCA6Bu ^ idx * 0xC2B2AE35u);
    const float r1 = SfRnd(s), r2 = SfRnd(s ^ 0x11u), r3 = SfRnd(s ^ 0x22u), r4 = SfRnd(s ^ 0x33u), r5 = SfRnd(s ^ 0x44u);
    const float ang = 6.2831853f * r2;
    memset(q, 0, sizeof *q);
    q->type = type, q->kind = F->kind, q->t0 = t0, q->ph = 6.2831853f * r5;
    q->p0[0] = base[0], q->p0[1] = base[1], q->p0[2] = base[2];
    switch (type) {
    case SFP_EMBER:  // born in the lower flame, rises with drag, wanders, twinkles; short-lived
        q->p0[0] += 0.10f * Href * cosf(ang) * r3;
        q->p0[2] += 0.10f * Href * sinf(ang) * r3;
        q->p0[1] -= Href * (-0.15f + 0.40f * r4);
        q->v0[0] = 0.15f * Href * (r3 - 0.5f), q->v0[2] = 0.15f * Href * (r4 - 0.5f);
        q->v0[1] = -(0.9f + 0.7f * r1) * Href * (stream == 2u ? 1.4f : 1.0f);  // stream 2: the last flare's burst, faster
        q->drag = 0.7f, q->wob = 0.06f * Href;
        q->life = 0.45f + 0.55f * r1;
        q->size = 0.08f * Href, q->k = stream == 2u ? 1.0f : 0.90f, q->cell = SFC_EMBER;
        break;
    case SFP_SPARK: {  // a hit's sparks: burst out and up, fall
        const float up = 0.3f + 0.7f * r3, c = sqrtf(1.0f - up * up), sp = (1.2f + 1.3f * r1) * Href;
        q->v0[0] = cosf(ang) * c * sp, q->v0[1] = -up * sp, q->v0[2] = sinf(ang) * c * sp;
        q->grav = 2.2f * Href;
        q->life = 0.35f + 0.35f * r4;
        q->size = 0.09f * Href, q->k = 1.0f, q->cell = (s & 3u) == 0u ? SFC_SPARK : SFC_EMBER;
        break;
    }
    case SFP_FALL:  // the going-out: embers fall from where the fire was, cooling
        q->p0[0] += 0.10f * Href * cosf(ang) * r3;
        q->p0[2] += 0.10f * Href * sinf(ang) * r3;
        q->v0[0] = 0.5f * Href * (r3 - 0.5f) * 2.0f * cosf(ang), q->v0[2] = 0.5f * Href * (r3 - 0.5f) * 2.0f * sinf(ang);
        q->v0[1] = -(0.3f + 0.5f * r4) * Href;
        q->grav = 2.5f * Href;
        q->life = 0.6f + 0.5f * r1;
        q->size = 0.07f * Href, q->k = 0.9f, q->cell = SFC_EMBER;
        break;
    case SFP_SMOKE:  // a puff of the thin wisp: rises, slows, widens, curls
        q->p0[0] += 0.03f * Href * (r3 - 0.5f), q->p0[2] += 0.03f * Href * (r4 - 0.5f);
        q->v0[0] = 0.05f * Href * (r3 - 0.5f), q->v0[2] = 0.05f * Href * (r4 - 0.5f);
        q->v0[1] = -1.2f * Href;
        q->drag = 1.2f, q->wob = 0.20f * Href;
        q->life = (0.95f + 0.2f * r1) * (e > 0.5f ? e : 0.5f);
        q->size = 0.065f * Href, q->k = 0.60f, q->cell = (s & 1u) ? SFC_SMOKE1 : SFC_SMOKE0;
        break;
    }
}
// the sprite of a particle at time now; 0 = gone (the slot is freed)
static int SfPartSpr(SfPart *q, float now, SfSpr *o)
{
    const SfPal *P = &kSfPal[q->kind];
    const float t = now - q->t0;
    float u, Td, amp, rgbT;
    int j;
    if (q->type == SFP_NONE) return 0;
    if (t >= q->life) {
        q->type = SFP_NONE;
        return 0;
    }
    if (t < 0.0f) return 0;
    u = t / q->life;
    Td = q->drag > 0.0f ? q->drag * (1.0f - expf(-t / q->drag)) : t;
    for (j = 0; j < 3; ++j) o->p[j] = q->p0[j] + q->v0[j] * Td;
    o->p[1] += 0.5f * q->grav * t * t;
    amp = q->type == SFP_SMOKE ? q->wob * u : q->wob * SfSat(t * 3.0f);
    o->p[0] += amp * sinf(q->ph + 6.3f * t);
    o->p[2] += amp * cosf(1.3f * q->ph + 5.1f * t);
    o->rot = q->ph, o->lean = 0.0f, o->mirror = 0, o->cell = q->cell, o->add = 1, o->minPx = 1.6f, o->fire = -1;
    o->rx = o->ry = q->size;
    switch (q->type) {
    case SFP_EMBER:
        o->a = q->k * SfSmooth(0.0f, 0.06f, t) * (1.0f - SfSmooth(0.45f, 1.0f, u)) * (0.75f + 0.25f * sinf(25.0f * t + q->ph));
        o->rx = o->ry = q->size * (1.0f - 0.4f * u);
        rgbT = u;
        for (j = 0; j < 3; ++j) o->rgb[j] = SfMix(P->ember[j], P->coal[j], rgbT);
        break;
    case SFP_SPARK:
        o->a = q->k * (1.0f - powf(u, 1.5f));
        o->rx = o->ry = q->size * (1.0f - 0.5f * u);
        o->rot = q->ph + 3.0f * t;
        for (j = 0; j < 3; ++j) o->rgb[j] = SfMix(P->layer[2][j], P->ember[j], u);
        break;
    case SFP_FALL:
        o->a = q->k * SfSmooth(0.0f, 0.03f, t) * (1.0f - SfSmooth(0.5f, 1.0f, u)) * (0.8f + 0.2f * sinf(31.0f * t + q->ph));
        for (j = 0; j < 3; ++j) o->rgb[j] = SfMix(P->ember[j], P->coal[j], 0.4f + 0.6f * u);
        break;
    default:  // SFP_SMOKE
        o->a = q->k * SfSmooth(0.0f, 0.12f, u) * (1.0f - SfSmooth(0.35f, 1.0f, u));
        o->rx = o->ry = q->size * (1.0f + 1.5f * u);
        o->rot = q->ph + 0.8f * t;
        o->add = 0, o->minPx = 2.0f;
        for (j = 0; j < 3; ++j) o->rgb[j] = kSfSmoke[j];
        break;
    }
    return o->a > 0.003f;
}

// ---------------------------------------------------------------- one fire, one frame
static void SfAnchor(const SfFire *F, const SfLookV *L, float sc, float p[3])  // a dying fire keeps its foot in place
{
    p[0] = F->o[0], p[1] = F->o[1], p[2] = F->o[2];
    if (F->dying) p[1] += 0.30f * (F->hDie0 - L->H) * sc;
}
static void SfBase(const SfFire *F, float p[3])  // where the coal / smoke / falling embers start
{
    p[0] = F->o[0], p[1] = F->o[1] + 0.30f * 0.85f * F->hDie0, p[2] = F->o[2];
}
static void SfInit(SfFire *F, const SfIn *in, float now)
{
    memset(F, 0, sizeof *F);
    F->key = in->key, F->kind = in->kind, F->size = in->size, F->dieS = in->dieS;
    F->seed = (float)(SfHash(in->key ^ 0x5EEDu) & 1023u) * 0.0061f;
    F->phase = SfRnd(in->key ^ 0xF1A5u);
    F->hpK = in->hp < 0.0f ? 1.0f : SfSat(in->hp);
    F->flT = F->agT = now - 100.0f;
    F->tSeen = F->tLast = now;
    F->rate = 1.0f / SF_LOOP_S;
    F->live = 1;
    memcpy(F->o, in->o, sizeof F->o);
}
static void SfStep(SfWorld *W, SfFire *F, const SfIn *in, float now)
{
    const SfPal *P = &kSfPal[F->kind];
    const int dead = in->deadAge >= 0.0f || in->deadNow;
    float dt = now - F->tLast;
    SfLookV L;
    if (!(dt >= 0.0f)) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;  // a pause / load: no catch-up
    F->tLast = now;
    memcpy(F->o, in->o, sizeof F->o);
    SfLook(F, now, &L);  // the flipbook phase: the trapezoid of the rate over the step, before this frame's events
    F->phase += 0.5f * dt * (F->rate + L.speed / SF_LOOP_S);
    F->phase -= floorf(F->phase);
    if (!F->dying && (in->hit || in->drop >= SF_HIT_DROP)) {  // a hit: flare, go wild, sparks (also the killing blow)
        const float s = SfSat(SF_HIT_MIN + SF_HIT_K * in->drop);
        int i, n;
        SfAgitate(F, now, SF_AG_HIT * s);
        F->flS = s, F->flT = now;
        n = 6 + (int)(10.0f * s);
        for (i = 0; i < n; ++i) SfSpawn(W, F, SFP_SPARK, 3u + F->nHit * 16u, (SfU32)i, now, F->o, L.Href, 1.0f);
        ++F->nHit;
    } else if (!F->dying && in->drop > SF_DRAIN_MIN) {  // a drain: unsettled in proportion, no flare
        SfAgitate(F, now, SF_AG_DRAIN * in->drop);
    }
    if (dead && !F->dying) {  // it dies: the fire starts going out from its size before the blow (at least SF_DIE_HP's)
        SfDie D0;
        F->dying = 1;
        F->tDeath = now - (in->deadAge >= 0.0f ? in->deadAge : 0.0f);
        if (F->hpK < SF_DIE_HP) F->hpK = SF_DIE_HP;
        F->hDie0 = SF_H * F->size * SfHealthSize(F->hpK);
        SfDieEval(now - F->tDeath, F->dieS, &D0);  // first seen part-way: the events already past are not replayed
        F->ev = 0, F->smokeN = 0;
        if (now - F->tDeath > D0.tFlare + 0.1f) F->ev |= 1;
        if (now - F->tDeath > D0.tOut + 0.1f) F->ev |= 2;
        while (F->smokeN < SF_SMOKE_N && now - F->tDeath > D0.tSmoke0 + (float)F->smokeN * D0.tSmokeDt + 0.1f) ++F->smokeN;
    } else if (!dead && F->dying) {  // seen alive again (it got up, or a reused slot): relit with a flare
        F->dying = 0;
        F->flS = 0.8f, F->flT = now;
        SfAgitate(F, now, 0.6f);
    }
    if (!F->dying) {  // the health, smoothed (exact for a constant target at any step)
        const float target = in->hp < 0.0f ? 1.0f : SfSat(in->hp);
        F->hpK += (target - F->hpK) * (1.0f - expf(-dt / SF_HP_TAU));
    }
    SfLook(F, now, &L);
    F->rate = L.speed / SF_LOOP_S;  // the left end of the next step
    {   // ephemeral embers: an exact accumulator, each spawn back-dated to its moment inside the frame
        const float er = P->emRate * (0.5f + 0.5f * F->hpK) * L.die.emK + 14.0f * L.ag;
        float base[3];
        SfAnchor(F, &L, 1.0f, base);
        if (er > 0.0f) {
            F->emAcc += er * dt;
            while (F->emAcc >= 1.0f) {
                F->emAcc -= 1.0f;
                SfSpawn(W, F, SFP_EMBER, 1u, F->nEmber++, now - F->emAcc / er, base, L.Href, 1.0f);
            }
        }
    }
    if (F->dying) {  // the going-out's events, on their own schedule (back-dated: frame-rate independent)
        const float age = now - F->tDeath;
        float base[3];
        int i;
        SfBase(F, base);
        if (!(F->ev & 1) && age >= L.die.tFlare) {
            F->ev |= 1;
            for (i = 0; i < SF_FLARE_N; ++i) SfSpawn(W, F, SFP_EMBER, 2u, (SfU32)i, F->tDeath + L.die.tFlare, F->o, F->hDie0 * 0.62f, L.die.e);
        }
        if (!(F->ev & 2) && age >= L.die.tOut) {
            F->ev |= 2;
            for (i = 0; i < SF_FALL_N; ++i) SfSpawn(W, F, SFP_FALL, 4u, (SfU32)i, F->tDeath + L.die.tOut, base, F->hDie0, L.die.e);
        }
        while (F->smokeN < SF_SMOKE_N && age >= L.die.tSmoke0 + (float)F->smokeN * L.die.tSmokeDt) {
            SfSpawn(W, F, SFP_SMOKE, 5u, (SfU32)F->smokeN, F->tDeath + L.die.tSmoke0 + (float)F->smokeN * L.die.tSmokeDt, base,
                    F->hDie0, L.die.e);
            ++F->smokeN;
        }
    }
}
// the body sprites of one fire (3 layers x 2 crossfaded frames, the glow, the coal); returns the count
static int SfFireSprites(const SfFire *F, float now, int fi, SfSpr *out, int max)
{
    const SfPal *P = &kSfPal[F->kind];
    SfLookV L;
    int n = 0, Ly, j;
    SfLook(F, now, &L);
    if (L.H > 0.5f && L.bright > 0.002f) {
        for (Ly = 0; Ly < 3; ++Ly) {
            const float sc = kSfLayerSc[Ly], a = kSfLayerK[Ly] * SF_K * L.bright;
            float rgb[3], ph = F->phase + kSfLayerOff[Ly], fp, x, p[3];
            int f0, m;
            ph -= floorf(ph);
            fp = ph * (float)SF_FRAMES;
            f0 = (int)fp;
            if (f0 >= SF_FRAMES) f0 = SF_FRAMES - 1;
            x = fp - (float)f0;
            for (j = 0; j < 3; ++j) {
                rgb[j] = SfMix(P->layer[Ly][j], kSfCool[Ly][j], 0.85f * L.cool);
                if (Ly == 2) rgb[j] = SfMix(rgb[j], 1.0f, 0.25f * SfSat(L.ag));  // a wild fire burns whiter
            }
            SfAnchor(F, &L, sc, p);
            for (m = 0; m < 2 && n < max; ++m) {
                SfSpr *s = &out[n];
                const float w = m ? x : 1.0f - x;
                if (w * a <= 0.002f) continue;
                memcpy(s->p, p, sizeof s->p);
                s->rx = 0.5f * L.W * sc, s->ry = L.H * sc, s->lean = L.lean * kSfLayerLean[Ly], s->rot = 0.0f;
                memcpy(s->rgb, rgb, sizeof s->rgb);
                s->a = a * w, s->minPx = 10.0f * sc;
                s->cell = (f0 + m) % SF_FRAMES, s->add = 1, s->mirror = Ly == 1, s->fire = fi;
                ++n;
            }
        }
        if (n < max) {  // a dim glow around the foot (brighter in a flare)
            SfSpr *s = &out[n];
            float p[3];
            SfAnchor(F, &L, 1.0f, p);
            p[1] -= 0.10f * L.H;
            memcpy(s->p, p, sizeof s->p);
            s->rx = s->ry = 0.9f * L.H, s->lean = 0.0f, s->rot = 0.0f;
            memcpy(s->rgb, P->layer[1], sizeof s->rgb);
            s->a = 0.10f * L.bright * (1.0f + 2.5f * L.fl), s->minPx = 8.0f;
            s->cell = SFC_GLOW, s->add = 1, s->mirror = 0, s->fire = fi;
            ++n;
        }
    }
    if (F->dying && L.die.coal > 0.002f && n < max) {  // the coal at the foot
        SfSpr *s = &out[n];
        SfBase(F, s->p);
        s->rx = s->ry = 0.11f * SF_H * F->size, s->lean = 0.0f, s->rot = F->seed;
        memcpy(s->rgb, P->coal, sizeof s->rgb);
        s->a = L.die.coal * (0.85f + 0.15f * SfNoiseT(now * 9.0f, F->key + 11u)), s->minPx = 2.0f;
        s->cell = SFC_COAL, s->add = 1, s->mirror = 0, s->fire = fi;
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------- the frame: the beings seen, then every fire and particle
static SfFire *SfAcquire(SfWorld *W, const SfIn *in, float now)
{
    int i, freeI = -1, oldI = 0;
    for (i = 0; i < SF_FIRES; ++i) {
        SfFire *F = &W->fire[i];
        if (F->key == in->key && F->live) {
            if (F->kind != in->kind) SfInit(F, in, now);
            F->size = in->size, F->dieS = in->dieS;
            return F;
        }
        if (!F->live && freeI < 0) freeI = i;
        if (W->fire[i].tSeen < W->fire[oldI].tSeen) oldI = i;
    }
    i = freeI >= 0 ? freeI : oldI;
    SfInit(&W->fire[i], in, now);
    return &W->fire[i];
}
// steps every fire (ins: the beings drawn this frame), lets the unseen dying ones finish in place, forgets the rest,
// and writes the frame's sprites (premultiplied and additive mixed: the draw batches them); returns the count
static int SfFrame(SfWorld *W, const SfIn *ins, int nIn, float now, SfSpr *out, int max)
{
    int i, n = 0;
    for (i = 0; i < nIn; ++i) {
        SfFire *F = SfAcquire(W, &ins[i], now);
        SfStep(W, F, &ins[i], now);
        F->tSeen = now;
    }
    for (i = 0; i < SF_FIRES; ++i) {
        SfFire *F = &W->fire[i];
        if (!F->live || F->tSeen == now) continue;
        if (F->dying && now - F->tDeath < F->dieS + 0.25f) {  // its body went away (or its soul ended): finish in place
            SfIn v;
            memset(&v, 0, sizeof v);
            v.key = F->key, v.kind = F->kind, v.size = F->size, v.dieS = F->dieS, v.hp = -1.0f;
            v.deadAge = now - F->tDeath;
            memcpy(v.o, F->o, sizeof v.o);
            SfStep(W, F, &v, now);
        } else if (F->dying || now - F->tSeen > SF_UNSEEN_S) {
            F->live = 0, F->key = 0;
        }
    }
    for (i = 0; i < SF_FIRES && n < max; ++i)
        if (W->fire[i].live) n += SfFireSprites(&W->fire[i], now, i, out + n, max - n);
    for (i = 0; i < SF_PARTS && n < max; ++i)
        if (W->part[i].type != SFP_NONE && SfPartSpr(&W->part[i], now, &out[n])) ++n;
    return n;
}
#endif
