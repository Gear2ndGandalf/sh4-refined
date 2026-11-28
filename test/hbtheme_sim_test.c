// v3.59 FT359: native test of src\hbtheme_sim.h (test\build_hbt_test.cmd): the enemy bars' looks under the special
// filters. Every look at every Filter Strength keeps the fill readable against the empty part; the stock look is v3.58's
// palette exactly; the heat scale rises; the hide rule; Ghost Teleport's spirit tint.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include "../src/hbtheme_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// preview_sim.h kPvFsK (Low, Medium, High, Ultra): the strengths the bars are mixed by
static const float kFs[4] = { 0.35f, 0.65f, 1.0f, 1.35f };
static const char *const kFsName[4] = { "Low", "Medium", "High", "Ultra" };

// v3.58's hudbars.inc constants, written out again: the stock look must be them exactly
static void TestStock(void)
{
    HbTheme t;
    int f;
    for (f = 0; f < 4; ++f) {
        HbtBuild(HBT_STOCK, kFs[f], &t);
        CHECK(t.look == HBT_STOCK && t.k == 0.0f && t.over == 0.0f, "stock at %s: k %g over %g", kFsName[f], t.k, t.over);
        CHECK(t.c.rim == 0x000000u && t.c.back0 == 0x17110Fu && t.c.back1 == 0x090706u, "stock frame colours");
        CHECK(t.c.chipHot == 0xFFF0D8u && t.c.trail == 0xC2B094u && t.c.sheen == 0xFFFFFFu, "stock chip / trail / sheen");
        CHECK(t.c.guard == 0xDDF4F0u && t.c.lock == 0xE2D8C8u, "stock guard / lock");
        CHECK(t.c.fill[0][0] == 0xB0362Cu && t.c.fill[0][1] == 0x5C0E0Bu && t.c.fill[1][0] == 0x9CC0C0u && t.c.fill[1][1] == 0x3A5A60u &&
              t.c.fill[2][0] == 0xC0441Eu && t.c.fill[2][1] == 0x5A0C06u, "stock fills");
        CHECK(t.c.rev[0] == 0xD2C4FFu && t.c.rev[1] == 0x6A54B0u && t.c.steel[0] == 0xB4BCC8u && t.c.steel[1] == 0x4C5460u, "stock revive / steel");
        CHECK(!t.heat && !t.flames && !t.scan && !t.grain && !t.halo, "stock: no overlay");
    }
    // an out-of-range look is the stock look
    HbtBuild(-1, 1.0f, &t);
    CHECK(t.look == HBT_STOCK && t.c.fill[0][0] == 0xB0362Cu, "look -1 -> stock");
    HbtBuild(HBT_N, 1.0f, &t);
    CHECK(t.look == HBT_STOCK && t.c.back0 == 0x17110Fu, "look N -> stock");
    // a look at strength 0 (or NaN) is the stock palette, no overlays
    {
        const float nan = sqrtf(-1.0f);
        HbtBuild(HBT_NV, nan, &t);
        CHECK(t.k == 0.0f && t.over == 0.0f && t.c.fill[0][0] == 0xB0362Cu && !t.scan, "NaN strength -> stock colours");
    }
}

// the fill reads against the empty part: mean luma of the fill (top + bottom) minus the back's, at least 0.15 (the stock
// monster bar has 0.165), for every kind, every u of Thermal's heat scale, every strength
static void TestReadable(void)
{
    HbTheme t;
    int look, f, kind, s;
    float worst = 9.0f;
    for (look = HBT_STOCK; look < HBT_N; ++look)
        for (f = 0; f < 4; ++f) {
            HbtBuild(look, kFs[f], &t);
            for (kind = 0; kind < 3; ++kind)
                for (s = 0; s <= 10; ++s) {
                    unsigned top, bot;
                    float d;
                    HbtFillAt(&t, kind, (float)s / 10.0f, &top, &bot);
                    d = 0.5f * (HbtLuma(top) + HbtLuma(bot)) - 0.5f * (HbtLuma(t.c.back0) + HbtLuma(t.c.back1));
                    if (d < worst) worst = d;
                    CHECK(d >= 0.15f, "%s at %s, kind %d, u %.1f: fill %06X / %06X vs back %06X / %06X: contrast %.3f", kHbtName[look],
                          kFsName[f], kind, (float)s / 10.0f, top, bot, t.c.back0, t.c.back1, d);
                }
            // the lost part (trail) and the chip read against the back too
            CHECK(HbtLuma(t.c.trail) - HbtLuma(t.c.back0) >= 0.25f, "%s at %s: trail %06X vs back %06X", kHbtName[look], kFsName[f],
                  t.c.trail, t.c.back0);
            CHECK(HbtLuma(t.c.chipHot) - HbtLuma(t.c.back0) >= 0.5f, "%s at %s: chip", kHbtName[look], kFsName[f]);
            // the revive bar (its back 0x100C18 in hudbars.inc)
            CHECK(0.5f * (HbtLuma(t.c.rev[0]) + HbtLuma(t.c.rev[1])) - HbtLuma(0x100C18u) >= 0.2f, "%s at %s: revive", kHbtName[look],
                  kFsName[f]);
            CHECK(0.5f * (HbtLuma(t.c.steel[0]) + HbtLuma(t.c.steel[1])) - HbtLuma(0x100C18u) >= 0.2f, "%s at %s: seal", kHbtName[look],
                  kFsName[f]);
        }
    printf("readable: the weakest fill / back contrast over every look, strength, kind: %.3f\n", worst);
    // Thermal Vision: the lost part is cold, unlike the heat scale's fill (it must not read as more health)
    HbtBuild(HBT_THERMAL, 1.0f, &t);
    {
        const unsigned tr = t.c.trail;
        CHECK((tr & 0xFF) > ((tr >> 16) & 0xFF), "Thermal's lost part is cold (blue over red): %06X", tr);
    }
}

// the heat scale: brightness rises along the bar, red at the empty end, white-hot at full; the stops are exact
static void TestHeat(void)
{
    int i;
    float last = -1.0f;
    for (i = 0; i <= 100; ++i) {
        const unsigned c = HbtHeat((float)i / 100.0f);
        const float l = HbtLuma(c);
        CHECK(l >= last - 1e-4f, "heat luma falls at u %.2f: %.3f after %.3f", (float)i / 100.0f, l, last);
        last = l;
    }
    for (i = 0; i < HBT_HEAT_N; ++i) CHECK(HbtHeat(kHbtHeatU[i]) == kHbtHeatC[i], "heat stop %d: %06X", i, HbtHeat(kHbtHeatU[i]));
    CHECK(HbtHeat(-1.0f) == kHbtHeatC[0] && HbtHeat(2.0f) == kHbtHeatC[HBT_HEAT_N - 1], "heat clamps");
    CHECK(HbtHeat(sqrtf(-1.0f)) == kHbtHeatC[0], "heat NaN -> the empty end");
    {
        const unsigned lo = HbtHeat(0.0f), hi = HbtHeat(1.0f);
        CHECK(((lo >> 16) & 0xFF) > 0xB0 && (lo & 0xFF) < 0x40, "the empty end is red: %06X", lo);
        CHECK(((hi >> 16) & 0xFF) >= 0xF0 && ((hi >> 8) & 0xFF) >= 0xF0 && (hi & 0xFF) >= 0xD0, "full is white-hot: %06X", hi);
    }
    // Thermal at Low: a third of the way from the stock fill to the scale
    {
        HbTheme t;
        unsigned top, bot;
        HbtBuild(HBT_THERMAL, 0.35f, &t);
        HbtFillAt(&t, 0, 1.0f, &top, &bot);
        CHECK(top == HbtLerp(0xB0362Cu, HbtHeat(1.0f), 0.35f), "Thermal Low top %06X", top);
        CHECK(bot == HbtLerp(0x5C0E0Bu, HbtScale(HbtHeat(1.0f), HBT_HEAT_BOTTOM), 0.35f), "Thermal Low bottom %06X", bot);
    }
}

// the strengths: colours at most all of the look; overlays follow, Ultra above 1 only for the Night Visions
static void TestStrength(void)
{
    HbTheme t;
    int look, f;
    for (look = HBT_THERMAL; look < HBT_N; ++look)
        for (f = 0; f < 4; ++f) {
            const float want = kFs[f] > 1.0f ? 1.0f : kFs[f];
            HbtBuild(look, kFs[f], &t);
            CHECK(fabsf(t.k - want) < 1e-6f, "%s at %s: k %g", kHbtName[look], kFsName[f], t.k);
            if (look == HBT_NV || look == HBT_MONO) CHECK(fabsf(t.over - kFs[f]) < 1e-6f, "%s at %s: over %g", kHbtName[look], kFsName[f], t.over);
            else CHECK(fabsf(t.over - want) < 1e-6f, "%s at %s: over %g (Ultra = High)", kHbtName[look], kFsName[f], t.over);
            CHECK(t.heat == (look == HBT_THERMAL), "%s: heat %d", kHbtName[look], t.heat);
            CHECK(t.flames == (look == HBT_SPIRIT), "%s: flames %d", kHbtName[look], t.flames);
            CHECK(t.scan == (look == HBT_NV || look == HBT_MONO) && t.grain == t.scan, "%s: scan %d grain %d", kHbtName[look], t.scan, t.grain);
            CHECK(t.halo == (look == HBT_NV), "%s: halo %d", kHbtName[look], t.halo);
        }
    // at High every colour is the look's own palette
    for (look = HBT_THERMAL; look < HBT_N; ++look) {
        HbtBuild(look, 1.0f, &t);
        CHECK(!memcmp(&t.c, &kHbtPal[look], sizeof t.c), "%s at High: the palette itself", kHbtName[look]);
    }
    // Ultra's overlays stay drawable (alpha x over <= 1)
    HbtBuild(HBT_NV, 1.35f, &t);
    CHECK(t.scanA * t.over <= 1.0f && t.grainA * t.over <= 1.0f && t.haloA * t.over <= 1.0f, "Ultra overlay alphas");
}

// the looks differ from each other and from stock where it shows: the fill and the back
static void TestDistinct(void)
{
    HbTheme a, b;
    int i, j;
    for (i = HBT_STOCK; i < HBT_N; ++i)
        for (j = i + 1; j < HBT_N; ++j) {
            HbtBuild(i, 1.0f, &a);
            HbtBuild(j, 1.0f, &b);
            CHECK(a.c.fill[0][0] != b.c.fill[0][0] || a.heat != b.heat, "%s and %s share the monster fill", kHbtName[i], kHbtName[j]);
            CHECK(a.c.back0 != b.c.back0, "%s and %s share the back", kHbtName[i], kHbtName[j]);
        }
    // Spirit Vision: the victims (ghosts) are spirit blue, the monsters warm
    HbtBuild(HBT_SPIRIT, 1.0f, &a);
    CHECK((a.c.fill[1][1] & 0xFF) > ((a.c.fill[1][1] >> 16) & 0xFF), "SV ghost fill blue: %06X", a.c.fill[1][1]);
    CHECK(((a.c.fill[0][1] >> 16) & 0xFF) > (a.c.fill[0][1] & 0xFF), "SV monster fill warm: %06X", a.c.fill[0][1]);
    CHECK(HbtLuma(a.c.rim) > HbtLuma(a.c.back0) + 0.2f, "SV rim is a light line on the black world: %06X", a.c.rim);
    // Night Vision is green, Night Mono grey
    HbtBuild(HBT_NV, 1.0f, &a);
    for (i = 0; i < 3; ++i) {
        const unsigned c = a.c.fill[i][0];
        CHECK(((c >> 8) & 0xFF) >= ((c >> 16) & 0xFF) && ((c >> 8) & 0xFF) >= (c & 0xFF), "NV fill %d green: %06X", i, c);
    }
    HbtBuild(HBT_MONO, 1.0f, &a);
    for (i = 0; i < 3; ++i) {
        const unsigned c = a.c.fill[i][0];
        CHECK(((c >> 16) & 0xFF) == ((c >> 8) & 0xFF) && ((c >> 8) & 0xFF) == (c & 0xFF), "Mono fill %d grey: %06X", i, c);
    }
}

static void TestHide(void)
{
    int on, live, cls;
    for (on = 0; on < 2; ++on)
        for (live = 0; live < 2; ++live)
            for (cls = 0; cls < 3; ++cls)
                CHECK(HbtHides(on, live, cls) == (on && live && cls == 2), "hide on %d live %d class %d", on, live, cls);
}

static void TestTint(void)
{
    float c[3] = { 1.0f, 0.85f, 0.55f }, d[3];
    memcpy(d, c, sizeof d);
    HbtSpiritTint(d, 1.0f);
    CHECK(!memcmp(c, d, sizeof c), "not fading: her fire unchanged");
    memcpy(d, c, sizeof d);
    HbtSpiritTint(d, 0.4f);
    CHECK(fabsf(d[0] - 0.55f) < 1e-5f && fabsf(d[1] - 0.75f) < 1e-5f && fabsf(d[2] - 1.0f) < 1e-5f, "60 %% faded: spirit blue");
    memcpy(d, c, sizeof d);
    HbtSpiritTint(d, 0.7f);
    CHECK(d[2] > c[2] && d[2] < 1.0f && d[0] < c[0], "half way: between (%.2f %.2f %.2f)", d[0], d[1], d[2]);
    memcpy(d, c, sizeof d);
    HbtSpiritTint(d, sqrtf(-1.0f));
    CHECK(_finite(d[0]) && _finite(d[1]) && _finite(d[2]), "NaN fade: finite colours");
}

int main(void)
{
    TestStock();
    TestReadable();
    TestHeat();
    TestStrength();
    TestDistinct();
    TestHide();
    TestTint();
    printf("hbtheme_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
