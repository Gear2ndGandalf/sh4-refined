// PLAYER PACK: native test of src/pc_rules.h (staging/special_mods/player_cheats/p1_combat/PLAN.md, Verification).
// The ammo rule; the Dark Pact factors (both ways); the grab start and its release time at 30 / 60 / 120 / 180 Hz, run
// through a copy of the stock hold loop 0x53C250 (h -= dt at 0x53C571, h -= dt x shake x 0.5 at 0x53C716..0x53C72B,
// release when h < 0 at 0x53C731..0x53C73C); v3.43 QC343: Quick Charge (replaced Tireless) through a copy of the
// ready stance's charge loop (P+0x21C += 0x544D40's rate each tick the attack is held, full at 100) at 30 / 60 / 120 /
// 180 Hz; the Life Steal heal amount.
// Build and run: test\build_pc_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/pc_rules.h"

static int g_fail;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}

// the stock hold loop, from a grab whose meter starts at `hold`, with no stick shake (or a steady `shake` per tick):
// returns the ticks until she is released (the tick on which h < 0), -1 after 60 s
static long HoldTicks(float hold, int v, int hz, float shake)
{
    const float dt = 1.0f / (float)hz;  // 0x55AE60: [0x5B729C] (1.0) / [0x5D5248]
    float h = PcGrabStart(hold, v);
    long t;
    for (t = 1; t <= 60L * hz; ++t) {
        h = h - dt;                       // 0x53C56C: call 55AE60; fsubr [10A1150]
        h = h - dt * shake * 0.5f;        // 0x53C716: dt x shake x [0x5B79E0] (0.5); fsubr
        if (h < 0.0f) return t;           // 0x53C731: fcomp 0; test ah, 5; jp (= not below 0)
    }
    return -1;
}

// v3.43 QC343: the stock rate 0x544D40: fld time; call 55AE60 (dt); fmul [5B7E0C] (100.0); fdiv time
static float StockRate(float dt, float time)
{
    float r = dt * 100.0f;
    return r / time;
}
// the ready stance's charge (0x53CEA1: P+0x21C += rate; 0x53CB50 fires the charged swing at >= 100): ticks to full
static long ChargeTicks(float time, int v, int hz)
{
    const float dt = 1.0f / (float)hz;
    float c = 0.0f;
    long t;
    for (t = 1; t <= 60L * hz; ++t) {
        c += PcQcRate(dt, time, v);
        if (c >= 100.0f) return t;
    }
    return -1;
}

int main(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char what[160];
    int i, v;
    // ---- Ammo Saver
    {
        unsigned n = 0;
        int taken = 0, k;
        for (k = 0; k < 10; ++k) taken += PcAmmoRule(0, &n);
        Check(taken == 10, "ammo Normal: 10 shots take 10 rounds");
        n = 0;
        taken = 0;
        for (k = 0; k < 20; ++k) taken += PcAmmoRule(1, &n);
        Check(taken == 10, "ammo Saver: 20 shots take 10 rounds");
        n = 0;
        Check(PcAmmoRule(1, &n) == 1 && PcAmmoRule(1, &n) == 0 && PcAmmoRule(1, &n) == 1, "ammo Saver: the first shot takes, the second is free");
        taken = 0;
        for (k = 0; k < 50; ++k) taken += PcAmmoRule(2, &n);
        Check(taken == 0, "ammo Endless: no shot takes a round");
        Check(PcAmmoRule(1, NULL) == 1, "ammo Saver without a counter: stock");
        Check(PcAmmoGun(0x10) == 0 && PcAmmoGun(0x11) == 1, "ammo: the pistol and the revolver are covered");
        Check(PcAmmoGun(0x12) < 0 && PcAmmoGun(0x80) < 0 && PcAmmoGun(0x01) < 0, "ammo: any other item (PE's SMG, ammo boxes) is stock");
        {   // two guns keep their own count
            unsigned g[2] = { 0, 0 };
            int a = PcAmmoRule(1, &g[0]), b = PcAmmoRule(1, &g[1]), c = PcAmmoRule(1, &g[0]);
            Check(a == 1 && b == 1 && c == 0, "ammo Saver: the pistol and the revolver count apart");
        }
    }
    // ---- Dark Pact
    Check(PcPactK(0) == 1.0f && PcPactK(1) == 1.5f && PcPactK(2) == 2.0f && PcPactK(7) == 1.0f, "pact: factors 1 / 1.5 / 2 (unknown = 1)");
    {
        const float a = 0.2f, b = 0.9f;  // 0x5B7460 / 0x5B7520
        unsigned int x, y;
        float r;
        int ok = 1;
        memcpy(&x, &a, 4);
        y = 0x3E4CCCCD;
        ok &= x == y;
        memcpy(&x, &b, 4);
        y = 0x3F666666;
        ok &= x == y;
        Check(ok, "pact: 0.2f / 0.9f are the exe's bytes (CD CC 4C 3E / 66 66 66 3F), so x1 is stock");
        ok = 1;
        for (v = 0; v < 3; ++v)
            for (r = 0.0f; r <= 1.0f; r += 0.125f) {
                const float k = PcPactK(v), stock = r * 0.2f + 0.9f, pact = r * (0.2f * k) + 0.9f * k;
                ok &= fabsf(pact - k * stock) <= 1e-5f * k;
            }
        Check(ok, "pact taken: rnd x 0.2k + 0.9k = k x the stock factor (0.9 .. 1.1), every rnd");
        ok = 1;
        for (v = 0; v < 3; ++v) ok &= 40.0f * PcPactK(v) == (v == 0 ? 40.0f : v == 1 ? 60.0f : 80.0f);
        Check(ok, "pact dealt: a 40 hit becomes 40 / 60 / 80");
    }
    // ---- Grab Breaker: the hold loop at every rate (Sniffer Dog hold 2.0; a ghost hold 3.0 as an example)
    {
        static const float kHold[2] = { 2.0f, 3.0f };
        int h;
        for (h = 0; h < 2; ++h)
            for (v = 0; v < 3; ++v) {
                const float want = v == 2 ? PC_GRAB_INSTANT_S : kHold[h] * PcGrabK(v);
                int ok = 1;
                for (i = 0; i < 4; ++i) {
                    const long t = HoldTicks(kHold[h], v, kHz[i], 0.0f);
                    const double secs = (double)t / kHz[i];
                    ok &= t > 0 && fabs(secs - want) <= 1.0 / kHz[i] + 1e-4;
                }
                sprintf_s(what, sizeof what, "grab %s, hold %.1f: released after %.2f s (no shake) at 30 / 60 / 120 / 180 Hz",
                          v == 0 ? "Stock" : v == 1 ? "Quick" : "Instant", kHold[h], want);
                Check(ok, what);
            }
        {   // Quick is half of Stock, also with shaking (a steady shake per tick: the stock rate dependence stays the game's)
            int ok = 1;
            for (i = 0; i < 4; ++i) {
                const long s = HoldTicks(2.0f, 0, kHz[i], 1.0f), q = HoldTicks(2.0f, 1, kHz[i], 1.0f);
                ok &= s > 0 && q > 0 && labs(2 * q - s) <= 2;
            }
            Check(ok, "grab Quick with shaking: half the ticks of Stock (within 1 tick) at every rate");
        }
        {
            const float nanv = (float)(0.0 * HUGE_VAL);  // NaN (HUGE_VAL is inf; 0 x inf = NaN)
            const float g = PcGrabStart(nanv, 1);
            Check(g != g, "grab: a NaN hold stays the game's (not turned into a number)");
            Check(PcGrabStart(2.0f, 0) == 2.0f && PcGrabStart(2.0f, 1) == 1.0f && PcGrabStart(2.0f, 2) == PC_GRAB_INSTANT_S,
                  "grab start: x1 / x0.5 / 0.1 s");
            Check(PcGrabStart(0.05f, 2) == 0.05f, "grab Instant: a hold already shorter than 0.1 s is kept");
            {
                int ok = 1;
                for (i = 0; i < 4; ++i) ok &= HoldTicks(2.0f, 2, kHz[i], 1.0f) <= HoldTicks(2.0f, 2, kHz[i], 0.0f);
                Check(ok, "grab Instant: shaking never makes it longer");
            }
        }
    }
    // ---- v3.43 QC343: Quick Charge (the game's charge seconds: bat 4.0, cutter 0.42, spade 2.4, axe 3.0, pickaxe 6.0,
    // chainsaw 3.5 (0x697118); Playable Eileen's purse 1.2, night stick 1.75)
    {
        static const float kTime[8] = { 4.0f, 0.42f, 2.4f, 3.0f, 6.0f, 3.5f, 1.2f, 1.75f };
        static const int kMul[4] = { 100, 150, 200, 300 };
        int ok = 1, j, v;
        for (v = 0; v < 4; ++v) ok &= PcQcMul100(v) == kMul[v];
        ok &= PcQcMul100(-1) == 100 && PcQcMul100(4) == 100;
        Check(ok, "quick charge: factor x 100 = 100 / 150 / 200 / 300 (anything else 100)");
        ok = 1;
        for (i = 0; i < 4; ++i)
            for (j = 0; j < 8; ++j) {
                const float dt = 1.0f / (float)kHz[i], a = PcQcRate(dt, kTime[j], 0), b = StockRate(dt, kTime[j]);
                ok &= !memcmp(&a, &b, 4);
            }
        Check(ok, "quick charge Off: the stock rate, bit for bit (8 charge times x 4 rates)");
        for (v = 0; v < 4; ++v) {
            ok = 1;
            for (i = 0; i < 4; ++i)
                for (j = 0; j < 8; ++j) {
                    const long n = ChargeTicks(kTime[j], v, kHz[i]);
                    const float want = kTime[j] * 100.0f / (float)kMul[v], got = (float)n / (float)kHz[i];
                    ok &= n > 0 && got >= want - 0.5f / kHz[i] && got <= want + 1.0f / kHz[i] + 1e-4f;
                }
            sprintf_s(what, sizeof what, "quick charge %s: full in charge seconds / %.1f (within a tick) at 30 / 60 / 120 / 180 Hz",
                    v == 0 ? "Off" : v == 1 ? "1.5x" : v == 2 ? "2x" : "3x", kMul[v] / 100.0);
            Check(ok, what);
        }
        ok = 1;
        for (i = 0; i < 4; ++i)
            for (j = 0; j < 8; ++j)
                ok &= ChargeTicks(kTime[j], 3, kHz[i]) <= ChargeTicks(kTime[j], 2, kHz[i]) &&
                      ChargeTicks(kTime[j], 2, kHz[i]) <= ChargeTicks(kTime[j], 1, kHz[i]) &&
                      ChargeTicks(kTime[j], 1, kHz[i]) < ChargeTicks(kTime[j], 0, kHz[i]);
        Check(ok, "quick charge: 3x <= 2x <= 1.5x < Off for every weapon and rate");
        {
            const long a = ChargeTicks(4.0f, 0, 30), b = ChargeTicks(4.0f, 2, 30), c = ChargeTicks(4.0f, 3, 60);
            Check((a == 120 || a == 121) && (b == 60 || b == 61) && (c == 80 || c == 81),
                  "quick charge: the bat (4 s): 120 ticks at 30 Hz stock, 60 at 2x, 80 at 60 Hz 3x (+1 for float rounding)");
        }
        ok = PcQcAlias("Off") == 0 && PcQcAlias("off") == 0 && PcQcAlias("1.5x") == 1 && PcQcAlias("X1.5") == 1 &&
             PcQcAlias("150") == 1 && PcQcAlias("2x") == 2 && PcQcAlias("2") == 2 && PcQcAlias("3x") == 3 &&
             PcQcAlias("x3") == 3 && PcQcAlias("300") == 3;
        Check(ok, "quick charge ini: Off / 1.5x / 2x / 3x and the x1.5 / 1.5 / 150 spellings");
        ok = PcQcAlias("On") < 0 && PcQcAlias("") < 0 && PcQcAlias(NULL) < 0 && PcQcAlias("4x") < 0 && PcQcAlias("1.5 x") < 0;
        Check(ok, "quick charge ini: unknown texts (Tireless's On, empty, 4x) are not values (read as Off)");
    }
    // ---- Life Steal
    {
        Check(PcStealHeal(0, 50.0f, 100.0f, 50.0f, 100.0f) == 0.0f, "steal Off: no heal");
        Check(fabsf(PcStealHeal(1, 40.0f, 100.0f, 50.0f, 100.0f) - 2.0f) < 1e-5f, "steal 5 %: a 40 hit heals 2");
        Check(fabsf(PcStealHeal(2, 40.0f, 100.0f, 50.0f, 100.0f) - 4.0f) < 1e-5f, "steal 10 %: a 40 hit heals 4");
        Check(fabsf(PcStealHeal(2, 1e6f, 30.0f, 50.0f, 200.0f) - 3.0f) < 1e-5f, "steal: a one-hit kill counts only the HP the monster had");
        Check(PcStealHeal(2, 40.0f, 0.0f, 50.0f, 100.0f) == 0.0f, "steal: a hit on a dead monster (HP 0) heals nothing");
        Check(fabsf(PcStealHeal(2, 5000.0f, 5000.0f, 10.0f, 200.0f) - 20.0f) < 1e-4f, "steal: at most 10 % of her max a hit");
        Check(fabsf(PcStealHeal(2, 40.0f, 100.0f, 99.0f, 100.0f) - 1.0f) < 1e-5f, "steal: never past her max");
        Check(PcStealHeal(2, 40.0f, 100.0f, 100.0f, 100.0f) == 0.0f, "steal: at full health nothing");
        Check(PcStealHeal(2, 40.0f, 100.0f, 0.0f, 100.0f) == 0.0f, "steal: at 0 HP (dead) nothing");
        {
            const float nanv = (float)(0.0 * HUGE_VAL);
            Check(PcStealHeal(2, nanv, 100.0f, 50.0f, 100.0f) == 0.0f && PcStealHeal(2, 40.0f, nanv, 50.0f, 100.0f) == 0.0f,
                  "steal: NaN damage or HP heals nothing");
        }
    }
    printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
    return g_fail != 0;
}
