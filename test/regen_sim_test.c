// v3.47 RG347: native test of src\regen_sim.h (test\build_regen_test.cmd): Refined's own Regeneration: the ini texts,
// Playable Eileen's seed, the rates at any max HP and frame rate (30 .. 180 fps), the RegenDelay wait after a loss, the
// aura / Room 302 / busy / pause / door rules, full HP, who heals and the row's mode, Life Steal's heal route, the log runs.
#include <stdio.h>
#include <math.h>
#include "../src/regen_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static RgIn In(float hp, float mx, int level, int rate)
{
    RgIn in;
    memset(&in, 0, sizeof in);
    in.hp = hp;
    in.mx = mx;
    in.room = 0x00030001u;
    in.dt = RgDt(rate);
    in.perSec = RgPerSec(level, mx);
    in.delay = 10.0f;
    return in;
}
// runs the regen for `secs` game seconds at `rate` fps from hp (healing applied as the game would: capped at max)
static float Run(RgState *s, float hp, float mx, int level, int rate, double secs, int *lastState)
{
    const int n = (int)(secs * rate + 0.5);
    int i, st = RGS_OFF;
    float lost;
    for (i = 0; i < n; ++i) {
        RgIn in = In(hp, mx, level, rate);
        const float add = RgStep(s, &in, &st, &lost);
        hp = hp + add > mx ? mx : hp + add;
        RgNoteHp(s, hp);
    }
    if (lastState) *lastState = st;
    return hp;
}

static void TestParse(void)
{
    int k;
    CHECK(RgParseLevel("Off", &k) == 0 && k, "Off");
    CHECK(RgParseLevel("x1", &k) == 1 && k && RgParseLevel("X1.25", &k) == 2 && k && RgParseLevel("x2", &k) == 3 && k, "x1 / X1.25 / x2");
    CHECK(RgParseLevel("on", &k) == 1 && k, "On reads as x1");
    CHECK(RgParseLevel("x3", &k) == 0 && !k, "x3 is unknown: Off");
    CHECK(RgParseLevel("x1.2", &k) == 0 && !k && RgParseLevel("", &k) == 0 && !k && RgParseLevel(NULL, &k) == 0 && !k,
          "a prefix, empty, NULL: Off (unknown)");
    CHECK(RgParseLevel("Custom", &k) == 0 && !k, "the v3.46 row's Custom is not a Refined value");
    CHECK(RgClampDelay(5) == 10 && RgClampDelay(10) == 10 && RgClampDelay(45) == 45 && RgClampDelay(500) == 120 &&
          RgClampDelay(-3) == 10, "RegenDelay 10..120");
}

static void TestSeed(void)
{
    CHECK(RgLevelFromPe("1", "15") == 1 && RgLevelFromPe("1", "18.75") == 2 && RgLevelFromPe("1", "30") == 3, "PE's rates seed x1 / x1.25 / x2");
    CHECK(RgLevelFromPe("0", "15") == 0 && RgLevelFromPe("1", "0") == 0, "PE's Regen 0 or rate 0 seeds Off");
    CHECK(RgLevelFromPe("1", "20") == -1 && RgLevelFromPe("1", "50") == -1, "another rate: no seed");
    CHECK(RgLevelFromPe(NULL, NULL) == 1, "missing keys: PE's defaults 1 / 15 = x1");
    CHECK(RgLevelFromPe("1", "15.004") == 1 && RgLevelFromPe("1", "15.02") == -1, "rates compared within 0.01");
}

static void TestRates(void)
{
    static const int kRate[4] = { 30, 60, 120, 180 };
    int r, lv;
    CHECK(RgPerSec(0, 200.0f) == 0.0f && RgPerSec(4, 200.0f) == 0.0f && RgPerSec(1, 0.0f) == 0.0f, "no rate: Off, out of range, no max");
    CHECK(fabs(RgPerSec(1, 200.0f) - 0.5f) < 1e-6f && fabs(RgPerSec(3, 200.0f) - 1.0f) < 1e-6f && fabs(RgPerSec(2, 100.0f) - 0.3125f) < 1e-6f,
          "x1 = 15 %% of 200 a minute = 0.5 HP/s; x2 = 1 HP/s; x1.25 at 100 = 0.3125 HP/s");
    CHECK(fabs(RgDt(60) - 1.0f / 60.0f) < 1e-7f && fabs(RgDt(0) - 1.0f / 30.0f) < 1e-7f && fabs(RgDt(9) - 1.0f / 30.0f) < 1e-7f &&
          fabs(RgDt(241) - 1.0f / 30.0f) < 1e-7f && fabs(RgDt(180) - 1.0f / 180.0f) < 1e-7f, "dt = 1 / rate (10..240), else 1/30");
    for (lv = 1; lv < RG_LEVELS; ++lv)
        for (r = 0; r < 4; ++r) {
            RgState s;
            float hp;
            int st;
            RgReset(&s);
            hp = Run(&s, 40.0f, 200.0f, lv, kRate[r], 60.0, &st);  // one minute: + rate % of max (one tick of sampling)
            {
                const float want = 40.0f + 200.0f * kRgRatePct[lv] / 100.0f, tick = RgPerSec(lv, 200.0f) / (float)kRate[r];
                CHECK(fabs(hp - want) <= tick + 0.05f && st == RGS_HEAL, "level %d at %d fps: %.3f after a minute (want %.3f)", lv, kRate[r], hp, want);
            }
        }
    {   // the same feel at any max HP: 0 -> full takes 100 / rate minutes
        RgState s;
        int st;
        float hp;
        RgReset(&s);
        hp = Run(&s, 10.0f, 100.0f, 3, 60, 179.0, &st);
        CHECK(hp < 100.0f && hp > 99.0f && st == RGS_HEAL, "x2 at max 100: 10 -> %.2f in 179 s (full needs 180 s)", hp);
        hp = Run(&s, hp, 100.0f, 3, 60, 2.0, &st);
        CHECK(hp == 100.0f && st == RGS_FULL, "then full exactly (%.3f), state full", hp);
    }
}

static void TestDelay(void)
{
    RgState s;
    RgIn in;
    float add, lost, hp;
    int st, i;
    RgReset(&s);
    in = In(150.0f, 200.0f, 1, 60);
    RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_RESET && s.have && s.lastHp == 150.0f, "the first tick takes the sample");
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_HEAL && add > 0.0f && lost == 0.0f, "then heals (no loss before)");
    RgNoteHp(&s, 150.0f + add);
    in.hp = 120.0f;  // a hit
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_WAIT && add == 0.0f && fabs(lost - (30.0f + RgPerSec(1, 200.0f) / 60.0f)) < 0.01f, "a hit: the wait starts (lost %.2f)", lost);
    hp = 120.0f;
    for (i = 0; i < 60 * 10 - 2; ++i) {
        in.hp = hp;
        add = RgStep(&s, &in, &st, &lost);
    }
    CHECK(st == RGS_WAIT && add == 0.0f, "still waiting just before 10 s");
    for (i = 0; i < 4; ++i) {
        in.hp = hp;
        add = RgStep(&s, &in, &st, &lost);
        hp += add;
        RgNoteHp(&s, hp);
    }
    CHECK(st == RGS_HEAL && hp > 120.0f, "heals again after 10 s unhurt");
    in.hp = hp - 0.005f;  // a rounding wobble is not a hit
    RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_HEAL && lost == 0.0f, "a drop of 0.005 HP is not a hit");
    in.hp = hp - 5.0f;
    RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_WAIT && fabs(RgWaitLeft(&s, 10.0f) - 10.0) < 1e-9, "a second hit restarts the full wait (%.3f left)", RgWaitLeft(&s, 10.0f));
    {   // a rise from an item inside the wait does not end it, nor is it a loss
        const float h = hp - 5.0f;
        in.hp = h + 50.0f;
        RgStep(&s, &in, &st, &lost);
        CHECK(st == RGS_WAIT && lost == 0.0f, "an item heal inside the wait: still waiting, no loss");
    }
    {   // the wait counts while Refined does not heal (Off / PE's own): turned on later, it still waits
        RgState t;
        RgIn o = In(100.0f, 200.0f, 0, 30);
        RgReset(&t);
        RgStep(&t, &o, &st, &lost);
        o.hp = 90.0f;
        RgStep(&t, &o, &st, &lost);
        CHECK(st == RGS_OFF && lost > 9.9f, "Off: the loss is still seen");
        o.perSec = RgPerSec(1, 200.0f);
        RgStep(&t, &o, &st, &lost);
        CHECK(st == RGS_WAIT, "turned on right after: it waits out the delay");
    }
}

static void TestGates(void)
{
    RgState s;
    RgIn in;
    float add, lost;
    int st;
    double c;
    RgReset(&s);
    in = In(100.0f, 200.0f, 2, 60);
    RgStep(&s, &in, &st, &lost);
    in.aura = 1;
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_AURA && add == 0.0f, "in a ghost aura: no heal");
    in.aura = 0;
    in.r302 = 1;
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_R302 && add == 0.0f, "Room 302's own heal runs: no heal (no stacking)");
    in.r302 = 0;
    in.busy = 1;
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_BUSY && add == 0.0f, "an event / door / dying state: no heal");
    in.busy = 0;
    c = s.clock;
    in.paused = 1;
    in.hp = 50.0f;  // a scripted loss while paused
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_PAUSED && add == 0.0f && s.clock == c && s.lastHp == 100.0f, "paused: no heal, the clock and the sample stand still");
    in.paused = 0;
    add = RgStep(&s, &in, &st, &lost);
    CHECK(st == RGS_WAIT && lost > 49.9f, "after the pause the loss counts: the wait starts");
    {
        RgState d;
        RgIn z = In(0.0f, 200.0f, 1, 60);
        RgReset(&d);
        RgStep(&d, &z, &st, &lost);
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_DEAD && add == 0.0f, "no healing at 0 HP");
    }
    {   // a door / load / stall / max change restarts the sample: no loss counted across it
        RgState d;
        RgIn z = In(150.0f, 200.0f, 1, 60);
        RgReset(&d);
        RgStep(&d, &z, &st, &lost);
        z.room = 0x00030002u;
        z.hp = 80.0f;
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_RESET && add == 0.0f && lost == 0.0f && d.lastHp == 80.0f, "a new room: the sample restarts, no loss");
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_HEAL, "then it heals (no wait: nothing was lost in play)");
        z.gap = 1;
        z.hp = 40.0f;
        RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_RESET && lost == 0.0f, "a stall of the game loop (a load): restart");
        z.gap = 0;
        z.mx = 220.0f;
        RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_RESET && d.lastMax == 220.0f, "a new max HP: restart");
        z.mx = 0.0f;
        RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_RESET && !d.have, "no max HP: no sample");
        {
            volatile float zero = 0.0f;
            z.mx = 200.0f;
            z.hp = zero / zero;
            RgStep(&d, &z, &st, &lost);
            CHECK(st == RGS_RESET && !d.have, "a NaN HP: no sample");
        }
    }
    {   // a hit and the aura together: the wait is reported first; full HP is reported only when nothing else holds
        RgState d;
        RgIn z = In(200.0f, 200.0f, 1, 60);
        RgReset(&d);
        RgStep(&d, &z, &st, &lost);
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_FULL && add == 0.0f, "full HP: nothing to heal");
        z.hp = 205.0f;  // above max (another mod): no heal
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_FULL && add == 0.0f, "above max: full");
        z.hp = 199.999f;
        add = RgStep(&d, &z, &st, &lost);
        CHECK(st == RGS_WAIT, "a drop from 205 to 200 is a loss: waiting");
    }
    {   // a big dt (a hitch) is clamped
        RgState d;
        RgIn z = In(10.0f, 200.0f, 3, 60);
        RgReset(&d);
        RgStep(&d, &z, &st, &lost);
        z.dt = 5.0f;
        add = RgStep(&d, &z, &st, &lost);
        CHECK(fabs(add - RgPerSec(3, 200.0f) * RG_DT_MAX) < 1e-6f, "a 5 s tick heals at most 0.1 s worth (%.4f)", add);
        z.hp += add;
        RgNoteHp(&d, z.hp);
        z.dt = -1.0f;
        add = RgStep(&d, &z, &st, &lost);
        CHECK(add == 0.0f && lost == 0.0f, "a negative dt heals nothing");
    }
}

static void TestWho(void)
{
    CHECK(RgWho(1, 1, 0, 0, 0, 0, 0) == RGW_HENRY, "no PE: Refined heals Henry");
    CHECK(RgWho(1, 0, 0, 0, 0, 0, 0) == RGW_OFF, "no PE, Off: nobody");
    CHECK(RgWho(0, 1, 0, 0, 0, 0, 0) == RGW_NA, "not installed: nobody");
    CHECK(RgWho(1, 2, 1, 1, 0, 1, 0) == RGW_HENRY, "PE Off (Henry): Refined heals Henry");
    CHECK(RgWho(1, 2, 1, 1, 0, 1, 1) == RGW_EILEEN, "PE On, its regen off: Refined heals Eileen");
    CHECK(RgWho(1, 2, 1, 1, 1, 1, 1) == RGW_PE && RgWho(0, 0, 1, 1, 1, 1, 1) == RGW_PE, "PE's regen in charge: PE (whatever Refined's value)");
    CHECK(RgWho(1, 2, 1, 0, 0, 1, 1) == RGW_LEGACY && RgWho(1, 2, 1, 0, 0, 0, 0) == RGW_LEGACY, "PE without SH4PE_RegenActive: v3.46 (PE heals)");
    CHECK(RgWho(1, 2, 1, 1, 0, 0, 0) == RGW_HENRY, "PE with RegenActive but no SH4PE_Enabled: taken as Henry");
    CHECK(RgRowMode(0, 0, 0) == RGR_OWN && RgRowMode(1, 1, 0) == RGR_OWN && RgRowMode(1, 1, 1) == RGR_PE && RgRowMode(1, 0, 0) == RGR_LEGACY &&
          RgRowMode(1, 0, 1) == RGR_LEGACY, "the row's mode in each state");
    CHECK(!RgStealViaGame(0, 0, 0) && !RgStealViaGame(1, 1, 0) && RgStealViaGame(1, 1, 1) && RgStealViaGame(1, 0, 0),
          "Life Steal: directly unless PE's guard acts (its regen in charge, or an older PE)");
}

static void TestRun(void)
{
    RgRun r;
    memset(&r, 0, sizeof r);
    CHECK(RgRunStep(&r, RGS_WAIT, 0.0f, 50.0f, 1.0) == 0 && !r.on, "no run: nothing");
    CHECK(RgRunStep(&r, RGS_HEAL, 0.5f, 50.0f, 2.0) == 1 && r.on && r.from == 50.0f && r.gained == 0.5f, "the first heal starts a run");
    CHECK(RgRunStep(&r, RGS_HEAL, 0.5f, 50.5f, 3.0) == 0 && r.gained == 1.0f, "more heals add up");
    CHECK(RgRunStep(&r, RGS_PAUSED, 0.0f, 51.0f, 3.0) == 0 && r.on, "a pause only suspends it");
    CHECK(RgRunStep(&r, RGS_HEAL, 0.5f, 51.0f, 4.0) == 0 && r.gained == 1.5f && r.at == 4.0, "and it goes on");
    CHECK(RgRunStep(&r, RGS_RESET, 0.0f, 51.5f, 4.0) == 0 && r.on, "a door or load only suspends it too");
    CHECK(RgRunStep(&r, RGS_HEAL, 0.0f, 51.5f, 4.5) == 0 && r.on, "healing goes on after the door");
    CHECK(RgRunStep(&r, RGS_FULL, 0.0f, 200.0f, 5.0) == 2 && !r.on && r.why == RGS_FULL, "full ends it");
    CHECK(RgRunStep(&r, RGS_FULL, 0.0f, 200.0f, 6.0) == 0, "only once");
    CHECK(kRgStateName[RGS_RESET] && kRgWhoName[RGW_LEGACY] && !strcmp(kRgLevelName[2], "x1.25"), "names");
}

int main(void)
{
    TestParse();
    TestSeed();
    TestRates();
    TestDelay();
    TestGates();
    TestWho();
    TestRun();
    printf("regen_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
