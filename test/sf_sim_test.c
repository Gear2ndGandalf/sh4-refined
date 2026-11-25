// v3.28 FIRE: the native test of src/spiritfire_sim.h (health scaling, the going-out timeline, the hit flicker and the
// whole life of a fire at 30 / 60 / 144 Hz, the atlas). test\build_sf_test.cmd builds and runs it.
// sf_sim_test.exe --dump DIR: also writes DIR\atlas.pgm (the 512 x 512 atlas) and DIR\<kind>_<state>.txt (the sprite
// lists of a 60 Hz run, every 2nd frame = 30 fps), which tools/flame_preview.py renders.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "../src/spiritfire_sim.h"

static int g_fail, g_checks;
#define CHECK(c, ...) do { ++g_checks; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- a scripted being (the game side, as spiritfire.inc)
// The game changes HP on its 30 Hz ticks; the renderer samples at its own rate and computes the drop since its last
// sample, as SfReadHp does.
typedef struct { int scen; float dieS; int kind; float size; int player; float hit; unsigned key; } Being;  // hit: SC_HIT's drop
enum { SC_IDLE, SC_LOW, SC_HIT, SC_DIE, SC_LIFE };
static float TickT(float t) { return floorf(t * 30.0f + 1e-4f) / 30.0f; }  // the last game tick at or before t
static float ScenHp(int scen, float t, float hit)
{
    const float k = TickT(t);
    switch (scen) {
    case SC_LOW: return 0.15f;
    case SC_HIT: return k >= 1.0f ? 1.0f - hit : 1.0f;
    case SC_DIE: return k >= 0.5f ? 0.0f : 1.0f;
    case SC_LIFE: {  // a hit at 1.0 (25 %), a Death Presence drain 2..3 s (10 % a second), a hit at 3.5 (10 %), dead at 4.0
        float h = 1.0f;
        if (k >= 1.0f) h -= 0.25f;
        if (k >= 2.0f) h -= 0.10f * ((k < 3.0f ? k : 3.0f) - 2.0f);
        if (k >= 3.5f) h -= 0.10f;
        if (k >= 4.0f) h = 0.0f;
        return h;
    }
    default: return 1.0f;
    }
}
static float ScenDeath(int scen) { return scen == SC_DIE ? 0.5f : scen == SC_LIFE ? 4.0f : -1.0f; }

typedef struct { SfWorld W; float hpLast; int have; SfSpr spr[SF_SPR_MAX]; int nSpr; } Run;
static void RunFrame(Run *R, const Being *b, float t, float base)
{
    SfIn in;
    const float hp = ScenHp(b->scen, t, b->hit), td = ScenDeath(b->scen);
    memset(&in, 0, sizeof in);
    in.key = b->key, in.kind = b->kind, in.size = b->size, in.dieS = b->dieS;
    in.o[0] = 0.0f, in.o[1] = -130.0f, in.o[2] = 0.0f;
    in.hp = hp;
    in.drop = R->have && R->hpLast > hp ? R->hpLast - hp : 0.0f;
    R->hpLast = hp, R->have = 1;
    in.deadAge = -1.0f;
    if (td >= 0.0f && TickT(t) >= td) {
        if (b->player) in.deadNow = 1;
        else in.deadAge = t - td;
    }
    // the game adds no soul once its heat is 0 (DfHeat over SoulFadeSeconds): the fire finishes on its own
    R->nSpr = SfFrame(&R->W, &in, in.deadAge >= b->dieS ? 0 : 1, base + t, R->spr, SF_SPR_MAX);
}
static int LiveParts(const SfWorld *W)
{
    int i, n = 0;
    for (i = 0; i < SF_PARTS; ++i) n += W->part[i].type != SFP_NONE;
    return n;
}

// ---------------------------------------------------------------- unit checks
static void TestHealth(void)
{
    float h, last = -1.0f;
    CHECK(fabsf(SfHealthSize(0.0f) - SF_HP_MIN) < 1e-6f, "size at 0 hp %g", SfHealthSize(0.0f));
    CHECK(fabsf(SfHealthSize(1.0f) - 1.0f) < 1e-6f, "size at full hp %g", SfHealthSize(1.0f));
    CHECK(fabsf(SfHealthSize(-3.0f) - SF_HP_MIN) < 1e-6f && fabsf(SfHealthSize(7.0f) - 1.0f) < 1e-6f, "clamped");
    for (h = 0.0f; h <= 1.0f; h += 0.01f) {
        CHECK(SfHealthSize(h) > last, "monotonic at %g", h);
        last = SfHealthSize(h);
    }
    printf("health: size 0 hp %.2f, 25 %% %.3f, 50 %% %.3f, full %.2f; sputter below %.0f %%\n", SfHealthSize(0.0f),
           SfHealthSize(0.25f), SfHealthSize(0.5f), SfHealthSize(1.0f), 100.0f * SF_WOUNDED);
}
static void TestDie(float D)
{
    SfDie o, p;
    float a, prev = 2.0f, maxJump = 0.0f, gEnd, peak;
    SfDieEval(-1.0f, D, &o);
    CHECK(o.sizeH == 1.0f && o.bright == 1.0f && o.phase == 0, "alive look at D %g", D);
    SfDieEval(0.0f, D, &o);
    CHECK(fabsf(o.sizeH - 1.0f) < 1e-5f && fabsf(o.bright - 1.0f) < 1e-5f, "starts at full (D %g)", D);
    for (a = 0.0f; a < o.G; a += 0.001f) {  // gutter: never grows
        SfDieEval(a, D, &p);
        CHECK(p.sizeH <= prev + 1e-6f, "gutter grows at %g (D %g)", a, D);
        prev = p.sizeH;
    }
    SfDieEval(o.G - 1e-4f, D, &p);
    gEnd = p.sizeH;
    SfDieEval(o.G + 0.30f * o.e, D, &p);
    peak = p.sizeH;
    CHECK(peak > gEnd + 0.2f, "a last flare (end of gutter %.3f, peak %.3f; D %g)", gEnd, peak, D);
    SfDieEval(o.tOut, D, &p);
    CHECK(p.sizeH == 0.0f && p.bright == 0.0f && p.phase == 3, "out at tOut (D %g)", D);
    SfDieEval(o.tOut + 0.01f * o.e, D, &p);
    CHECK(p.coal > 0.9f, "a coal right after (D %g: %g)", D, p.coal);
    SfDieEval(o.tOut + 0.61f * o.e, D, &p);
    CHECK(p.coal == 0.0f, "the coal winks out (D %g)", D);
    CHECK(fabsf(o.tOut + 1.70f * o.e - (D < 0.1f ? 0.1f : D)) < 1e-4f, "the smoke ends at D (D %g: %g)", D, o.tOut + 1.7f * o.e);
    prev = -1.0f;
    for (a = 0.0f; a < o.tOut; a += 0.001f) {  // continuous (no pops between phases)
        SfDieEval(a, D, &p);
        if (prev >= 0.0f && fabsf(p.sizeH - prev) > maxJump) maxJump = fabsf(p.sizeH - prev);
        prev = p.sizeH;
    }
    CHECK(maxJump < 0.02f, "a jump of %g in 1 ms (D %g)", maxJump, D);
    printf("going out, D %4.1f s: gutter 0..%.2f, last flare %.2f..%.2f (peak %.2f), collapse ..%.2f, out %.2f, coal ..%.2f, "
           "smoke ..%.2f\n", D, o.G, o.G, o.G + 0.5f * o.e, o.G + 0.3f * o.e, o.tOut, o.tOut, o.tOut + 0.6f * o.e, o.tOut + 1.7f * o.e);
}
static void TestHitFlicker(void)
{
    SfFire F;
    float t, peak = 0.0f, tPeak = 0.0f;
    memset(&F, 0, sizeof F);
    F.flS = 1.0f, F.flT = 10.0f, F.agT = 10.0f, F.agA0 = 0.9f;
    for (t = 10.0f; t < 11.0f; t += 0.0005f) {
        const float f = SfFlareV(&F, t);
        if (f > peak) peak = f, tPeak = t - 10.0f;
    }
    CHECK(peak > 0.6f && tPeak < 0.06f, "the flare peaks at once (%g at %g s)", peak, tPeak);
    CHECK(SfFlareV(&F, 10.5f) < 0.02f, "the flare is gone by 0.5 s (%g)", SfFlareV(&F, 10.5f));
    CHECK(fabsf(SfAg(&F, 10.55f) - 0.9f * expf(-1.0f)) < 1e-4f, "agitation decays e^-t/0.55");
    printf("hit: flare peak %.2f at %.0f ms, %.2f at 0.2 s, %.3f at 0.5 s; agitation 0.9 -> %.2f at 0.55 s, %.2f at 1.5 s\n",
           peak, 1000.0f * tPeak, SfFlareV(&F, 10.2f), SfFlareV(&F, 10.5f), SfAg(&F, 10.55f), SfAg(&F, 11.5f));
}
// the same life at 30 / 60 / 144 Hz: compared at every 0.5 s (a time all three grids hit)
static void TestRates(void)
{
    static const int kHz[3] = { 30, 60, 144 };
    static Run R[3];
    const Being b = { SC_LIFE, 6.0f, SFK_MONSTER, 1.0f, 0, 0.0f, 0xA5A5F1u };
    int r, k;
    for (r = 0; r < 3; ++r) memset(&R[r], 0, sizeof R[r]);
    printf("rates (a life: hit 25 %% at 1.0, drain 2..3, hit 10 %% at 3.5, dead at 4.0, D 6):\n"
           "   t   |  hpK 30 / 60 / 144   |  agitation           |  flare               | embers spawned | parts\n");
    for (k = 1; k <= 22; ++k) {
        const float T = 0.5f * (float)k;
        float hpK[3], ag[3], fl[3], ph[3];
        unsigned em[3];
        int parts[3];
        for (r = 0; r < 3; ++r) {
            const int n0 = (int)floorf(0.5f * (float)(k - 1) * (float)kHz[r] + 0.5f), n1 = (int)floorf(T * (float)kHz[r] + 0.5f);
            int n;
            for (n = n0 + (k == 1 ? 0 : 1); n <= n1; ++n) RunFrame(&R[r], &b, (float)n / (float)kHz[r], 100.0f);
            hpK[r] = R[r].W.fire[0].hpK, ag[r] = SfAg(&R[r].W.fire[0], 100.0f + T), fl[r] = SfFlareV(&R[r].W.fire[0], 100.0f + T);
            ph[r] = R[r].W.fire[0].phase, em[r] = R[r].W.fire[0].nEmber, parts[r] = LiveParts(&R[r].W);
        }
        printf("  %4.1f | %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f | %4u %4u %4u | %3d %3d %3d\n", T, hpK[0], hpK[1],
               hpK[2], ag[0], ag[1], ag[2], fl[0], fl[1], fl[2], em[0], em[1], em[2], parts[0], parts[1], parts[2]);
        for (r = 1; r < 3; ++r) {
            float dph = fabsf(ph[r] - ph[0]);
            if (dph > 0.5f) dph = 1.0f - dph;
            CHECK(fabsf(hpK[r] - hpK[0]) < 0.02f, "hpK at %g s, %d Hz: %g vs %g", T, kHz[r], hpK[r], hpK[0]);
            CHECK(fabsf(ag[r] - ag[0]) < 0.03f * (ag[0] > 1.0f ? ag[0] : 1.0f), "agitation at %g s, %d Hz: %g vs %g", T, kHz[r], ag[r], ag[0]);
            CHECK(fabsf(fl[r] - fl[0]) < 1e-3f, "flare at %g s, %d Hz: %g vs %g", T, kHz[r], fl[r], fl[0]);
            CHECK(dph < 0.02f, "flipbook phase at %g s, %d Hz: %g vs %g", T, kHz[r], ph[r], ph[0]);
            CHECK(abs((int)em[r] - (int)em[0]) <= 1, "embers spawned by %g s, %d Hz: %u vs %u", T, kHz[r], em[r], em[0]);
            CHECK(abs(parts[r] - parts[0]) <= 3, "live particles at %g s, %d Hz: %d vs %d", T, kHz[r], parts[r], parts[0]);
        }
    }
    CHECK(R[0].W.fire[0].live == 0 && R[2].W.fire[0].live == 0, "the fire is gone after D");
}
static void TestAtlas(void)
{
    static float tex[SF_TEX * SF_TEX];
    LARGE_INTEGER f, a, b;
    int x, y, c, bad = 0;
    double dSeam = 0.0, dStep = 0.0;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    for (y = 0; y < SF_TEX; ++y)
        for (x = 0; x < SF_TEX; ++x) tex[y * SF_TEX + x] = SfAtlasTexel(x, y);
    QueryPerformanceCounter(&b);
    for (c = 0; c < 32; ++c) {  // every cell's border is 0 (no bleeding under linear filtering)
        const int x0 = (c & 7) * 64, y0 = (c >> 3) * 128;
        for (y = 0; y < 128; ++y)
            for (x = 0; x < 64; ++x)
                if ((x == 0 || x == 63 || y == 0 || y == 127 || (c >= SF_FRAMES && (y == 63 || y == 64))) &&
                    tex[(y0 + y) * SF_TEX + x0 + x] > 1.0f / 255.0f)
                    ++bad;
    }
    CHECK(!bad, "%d border texels lit", bad);
    for (c = 0; c < SF_FRAMES; ++c) {  // the loop: frame 27 -> 0 differs no more than any step
        const int n = (c + 1) % SF_FRAMES;
        double d = 0.0;
        for (y = 0; y < 128; ++y)
            for (x = 0; x < 64; ++x)
                d += fabs(tex[((c >> 3) * 128 + y) * SF_TEX + (c & 7) * 64 + x] - tex[((n >> 3) * 128 + y) * SF_TEX + (n & 7) * 64 + x]);
        if (n == 0) dSeam = d;
        else dStep += d / (SF_FRAMES - 1);
    }
    CHECK(dSeam < 1.5 * dStep, "the loop seam %.1f vs a step %.1f", dSeam, dStep);
    printf("atlas: %d x %d baked in %.1f ms; loop seam %.1f vs mean step %.1f (texel sums)\n", SF_TEX, SF_TEX,
           1000.0 * (double)(b.QuadPart - a.QuadPart) / (double)f.QuadPart, dSeam, dStep);
}

// ---------------------------------------------------------------- the preview's dump
static void Dump(const char *dir)
{
    static const struct { const char *name; int kind; float size; int player; float hit; } kKind[4] = {  // the player: a 10 % hit
        { "monster", SFK_MONSTER, 1.0f, 0, 0.2f }, { "person", SFK_PERSON, 0.8f, 0, 0.2f }, { "player", SFK_PERSON, 0.8f, 1, 0.1f },
        { "walter", SFK_WALTER, 2.2f, 0, 0.2f } };
    static const struct { const char *name; int scen; float t0, t1; } kState[4] = {
        { "idle", SC_IDLE, 0.0f, 2.5f }, { "low", SC_LOW, 0.0f, 2.5f }, { "hit", SC_HIT, 0.0f, 2.5f }, { "die", SC_DIE, 0.0f, 11.5f } };
    static Run R;
    char path[512];
    FILE *fp;
    int x, y, k, s, n, i;
    sprintf(path, "%s\\atlas.pgm", dir);
    fp = fopen(path, "wb");
    if (!fp) {
        printf("cannot write %s\n", path);
        ++g_fail;
        return;
    }
    fprintf(fp, "P5\n%d %d\n255\n", SF_TEX, SF_TEX);
    for (y = 0; y < SF_TEX; ++y)
        for (x = 0; x < SF_TEX; ++x) fputc((int)(SfAtlasTexel(x, y) * 255.0f + 0.5f), fp);
    fclose(fp);
    for (k = 0; k < 4; ++k)
        for (s = 0; s < 4; ++s) {
            const Being b = { kState[s].scen, 10.0f, kKind[k].kind, kKind[k].size, kKind[k].player, kKind[k].hit, 0xA5A5F1u + 0x1000u * (unsigned)k };
            memset(&R, 0, sizeof R);
            sprintf(path, "%s\\%s_%s.txt", dir, kKind[k].name, kState[s].name);
            fp = fopen(path, "w");
            if (!fp) continue;
            for (n = 0; n <= (int)(kState[s].t1 * 60.0f); ++n) {
                const float t = (float)n / 60.0f;
                RunFrame(&R, &b, t, 100.0f);
                if ((n & 1) || t < kState[s].t0) continue;
                fprintf(fp, "F %.4f %d %.3f %.3f %d\n", t, R.nSpr, R.W.fire[0].hpK, SfAg(&R.W.fire[0], 100.0f + t), R.W.fire[0].dying);
                for (i = 0; i < R.nSpr; ++i) {
                    const SfSpr *q = &R.spr[i];
                    fprintf(fp, "%d %d %d %.3f %.3f %.3f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.5f %.2f\n", q->cell, q->add, q->mirror, q->p[0],
                            q->p[1], q->p[2], q->rx, q->ry, q->lean, q->rot, q->rgb[0], q->rgb[1], q->rgb[2], q->a, q->minPx);
                }
            }
            fclose(fp);
        }
    printf("dumped the atlas and 16 sprite runs to %s\n", dir);
}

int main(int argc, char **argv)
{
    TestHealth();
    TestDie(10.0f);
    TestDie(6.0f);
    TestDie(3.0f);
    TestDie(1.0f);
    TestHitFlicker();
    TestRates();
    TestAtlas();
    if (argc > 2 && !strcmp(argv[1], "--dump")) Dump(argv[2]);
    printf("%s: %d of %d checks failed\n", g_fail ? "FAILED" : "PASSED", g_fail, g_checks);
    return g_fail ? 1 : 0;
}
