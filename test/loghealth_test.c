// v3.33 LOG333: native test of src/loghealth_sim.h (the pure part of the gameplay health log, loghealth.inc).
// MD5 against the RFC 1321 vectors (also fed in odd chunks), the frame classes and room statistics at 30 / 60 / 120 / 180
// fps (steady play gives no spike; one hitch is a spike at every rate; p99 within one 0.25 ms bin of the frame time), the
// percentile's overflow bin, the merge, the freeze / stutter rules, the rate limit, the "on but never acted" streak, the
// Supply Drops zero-drop bound, the flicker counter, the address-space margin steps and the ini default compare.
// Build and run: test\build_lh_test.cmd (cl /O2 x86, as build.cmd).
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/loghealth_sim.h"

static int g_fail, g_n;
static void Check(int ok, const char *what)
{
    ++g_n;
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}

static void Md5Str(const char *s, char hex[33], int chunk)
{
    LhMd5 m;
    unsigned char d[16];
    size_t n = strlen(s), i;
    LhMd5Init(&m);
    if (chunk <= 0) {
        LhMd5Update(&m, s, n);
    } else {
        for (i = 0; i < n; i += (size_t)chunk) LhMd5Update(&m, s + i, n - i < (size_t)chunk ? n - i : (size_t)chunk);
    }
    LhMd5Final(&m, d);
    LhHex(d, 16, hex);
}

static void TestMd5(void)
{
    static const struct { const char *in, *out; } kV[] = {
        { "", "d41d8cd98f00b204e9800998ecf8427e" },
        { "a", "0cc175b9c0f1b6a831c399e269772661" },
        { "abc", "900150983cd24fb0d6963f7d28e17f72" },
        { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
        { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
          "57edf4a22be3c955ac49da2e2107b67a" },
    };
    char hex[33], msg[96];
    int i, c;
    for (i = 0; i < (int)(sizeof kV / sizeof kV[0]); ++i) {
        Md5Str(kV[i].in, hex, 0);
        sprintf(msg, "md5 RFC 1321 vector %d (\"%.20s\")", i, kV[i].in);
        Check(!strcmp(hex, kV[i].out), msg);
        for (c = 1; c <= 13; c += 6) {
            Md5Str(kV[i].in, hex, c);
            sprintf(msg, "md5 vector %d fed in chunks of %d", i, c);
            Check(!strcmp(hex, kV[i].out), msg);
        }
    }
    {   // a million 'a' (crosses the 64-byte block and the length words many times)
        static char a[1000];
        LhMd5 m;
        unsigned char d[16];
        memset(a, 'a', sizeof a);
        LhMd5Init(&m);
        for (i = 0; i < 1000; ++i) LhMd5Update(&m, a, sizeof a);
        LhMd5Final(&m, d);
        LhHex(d, 16, hex);
        Check(!strcmp(hex, "7707d6ae4e027c70eea2a935c2296f21"), "md5 of one million 'a'");
    }
}

static void TestClassify(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    int k;
    Check(LhSpikeMs(1000.0f / 30) == 100.0f, "spike threshold at 30 fps: 100 ms (three frames)");
    Check(LhSpikeMs(1000.0f / 60) == 50.0f, "spike threshold at 60 fps: 50 ms (the floor)");
    Check(LhSpikeMs(1000.0f / 120) == 50.0f && LhSpikeMs(1000.0f / 180) == 50.0f, "spike threshold at 120 / 180 fps: 50 ms");
    Check(LhSpikeMs(0.0f) == 100.0f, "unknown rate counts as 30 fps");
    Check(LhClassify(0.0f, 16.7f) == LHF_SKIP && LhClassify(-3.0f, 16.7f) == LHF_SKIP, "zero / negative: skipped");
    {
        volatile float z = 0.0f;
        Check(LhClassify(z / z, 16.7f) == LHF_SKIP, "NaN: skipped");
    }
    Check(LhClassify(1000.0f, 16.7f) == LHF_STALL && LhClassify(5000.0f, 33.3f) == LHF_STALL, "1 s or more: a stall");
    Check(LhClassify(60.0f, 33.3f) == LHF_NORMAL, "60 ms at 30 fps: normal (under three frames)");
    Check(LhClassify(60.0f, 16.7f) == LHF_SPIKE, "60 ms at 60 fps: a spike");
    for (k = 0; k < 4; ++k) {
        const float period = 1000.0f / (float)kHz[k];
        LhStats s;
        int i, steady = 1;
        char msg[128];
        LhStatsReset(&s);
        for (i = 0; i < kHz[k] * 60; ++i) {  // one minute of play with +-10 % jitter
            const float ms = period * (i % 3 == 0 ? 1.1f : i % 3 == 1 ? 0.9f : 1.0f);
            steady &= LhStatsAdd(&s, ms, period) == LHF_NORMAL;
        }
        sprintf(msg, "%d fps: a minute of steady play has no spike", kHz[k]);
        Check(steady && s.spikes == 0 && s.stalls == 0, msg);
        sprintf(msg, "%d fps: mean %.3f ms = the frame time %.3f", kHz[k], LhStatsMean(&s), period);
        Check(fabs(LhStatsMean(&s) - period) < 0.01, msg);
        sprintf(msg, "%d fps: p99 %.2f within one bin of the slowest steady frame %.2f", kHz[k], LhStatsPct(&s, 99.0f),
                period * 1.1f);
        Check(LhStatsPct(&s, 99.0f) >= period * 1.1f - 0.001f && LhStatsPct(&s, 99.0f) <= period * 1.1f + LH_BIN_MS, msg);
        sprintf(msg, "%d fps: one 120 ms hitch is a spike and the worst", kHz[k]);
        Check(LhStatsAdd(&s, 120.0f, period) == LHF_SPIKE && s.spikes == 1 && s.worst == 120.0f, msg);
        sprintf(msg, "%d fps: a 2 s stall is counted apart (not in the mean)", kHz[k]);
        {
            const float before = LhStatsMean(&s);
            const unsigned n = s.n;
            Check(LhStatsAdd(&s, 2000.0f, period) == LHF_STALL && s.stalls == 1 && s.n == n && LhStatsMean(&s) == before, msg);
        }
    }
}

static void TestStats(void)
{
    LhStats a, b;
    int i;
    LhStatsReset(&a);
    Check(LhStatsPct(&a, 99.0f) == 0.0f && LhStatsMean(&a) == 0.0f, "empty statistics: 0");
    for (i = 0; i < 99; ++i) LhStatsAdd(&a, 10.0f, 16.7f);
    LhStatsAdd(&a, 300.0f, 16.7f);  // over the histogram: the overflow bin
    Check(LhStatsPct(&a, 50.0f) == 10.25f, "p50 of 99 x 10 ms + one 300 ms: 10.25 ms (the upper edge of the 10 ms bin)");
    Check(LhStatsPct(&a, 99.0f) == 10.25f, "p99 of 100 frames: the 99th frame (10 ms, its bin edge 10.25)");
    Check(LhStatsPct(&a, 100.0f) == 300.0f, "p100 in the overflow bin: the worst frame");
    Check(a.spikes == 1 && a.worst == 300.0f, "the 300 ms frame is a spike and the worst");
    LhStatsReset(&b);
    for (i = 0; i < 50; ++i) LhStatsAdd(&b, 20.0f, 16.7f);
    LhStatsAdd(&b, 5000.0f, 16.7f);
    LhStatsMerge(&a, &b);
    Check(a.n == 150 && a.stalls == 1 && a.spikes == 1 && a.worst == 300.0f, "merge: counts, stalls, spikes, worst");
    Check(fabs(LhStatsMean(&a) - (99 * 10.0 + 300.0 + 50 * 20.0) / 150.0) < 1e-4, "merge: the mean of both");
    Check(LhStatsPct(&a, 90.0f) == 20.25f, "merge: p90 lands in the second room's bin (20.25)");
    LhStatsReset(&a);
    LhStatsAdd(&a, 16.6f, 16.7f);
    Check(LhStatsPct(&a, 99.0f) == 16.6f, "a single frame: p99 is that frame (the edge 16.75 clamped to the worst)");
}

static void TestRules(void)
{
    Check(LhFreezeIsAnomaly(300.0f, 5000.0f), "a 300 ms freeze 5 s into the room: ANOMALY");
    Check(!LhFreezeIsAnomaly(300.0f, 1000.0f), "a 300 ms freeze 1 s into the room (files load): no");
    Check(!LhFreezeIsAnomaly(200.0f, 9000.0f), "a 200 ms spike: no (under 250 ms)");
    Check(!LhFreezeIsAnomaly(1500.0f, 9000.0f), "a stall (1 s or more): no (a load or Alt+Tab)");
    Check(LhStutter(10, 60.0f), "10 spikes in a minute of play: stutter");
    Check(!LhStutter(10, 200.0f), "10 spikes in 200 s: no (3 a minute)");
    Check(!LhStutter(9, 10.0f), "9 spikes: no (under 10)");
    Check(!LhStutter(30, 0.0f), "no play time: no");
    {
        LhRate r = { 0, 0 };
        int a = LhRateOk(&r, 1000, 5000, 2), b = LhRateOk(&r, 2000, 5000, 2), c = LhRateOk(&r, 3000, 5000, 2);
        int d = LhRateOk(&r, 6500, 5000, 2), e = LhRateOk(&r, 6600, 5000, 2), f = LhRateOk(&r, 6700, 5000, 2);
        Check(a && b && !c, "rate limit: 2 in the window, the third refused");
        Check(d && e && !f, "rate limit: a new window after 5 s");
        memset(&r, 0, sizeof r);
        a = LhRateOk(&r, 0xFFFFFF00u, 0xFFFFFFFFu, 2), b = LhRateOk(&r, 5, 0xFFFFFFFFu, 2), c = LhRateOk(&r, 900000, 0xFFFFFFFFu, 2);
        Check(a && b && !c, "rate limit with an endless window (per room): 2, also across the tick wrap");
    }
}

static void TestIdle(void)
{
    LhIdle w = { 0, 0 };
    int r1, r2, r3, r4;
    r1 = LhIdleStep(&w, 1, 1, 0, 3);
    r2 = LhIdleStep(&w, 1, 1, 0, 3);
    r3 = LhIdleStep(&w, 1, 1, 0, 3);
    r4 = LhIdleStep(&w, 1, 1, 0, 3);
    Check(!r1 && !r2 && r3 && !r4, "idle: flagged once at the third eligible idle room, not again at the fourth");
    LhIdleStep(&w, 1, 1, 1, 3);
    Check(w.streak == 0 && !w.flagged, "idle: an action ends the streak and re-arms");
    LhIdleStep(&w, 1, 1, 0, 3);
    LhIdleStep(&w, 1, 0, 0, 3);
    LhIdleStep(&w, 1, 0, 0, 3);
    Check(w.streak == 1, "idle: ineligible rooms (Room 302, short visits) neither count nor reset");
    r1 = LhIdleStep(&w, 1, 1, 0, 3);
    r2 = LhIdleStep(&w, 1, 1, 0, 3);
    Check(!r1 && r2, "idle: the streak goes on after the ineligible rooms");
    LhIdleStep(&w, 0, 1, 0, 3);
    Check(w.streak == 0 && !w.flagged, "idle: the row off ends the streak");
}

static void TestDrops(void)
{
    Check(!LhDropsSuspicious(16, 0, 0.25f), "Generous (25 %): 16 kills without a drop is 1.0 % likely: not yet");
    Check(LhDropsSuspicious(17, 0, 0.25f), "Generous (25 %): 17 kills without a drop (0.75 %): ANOMALY");
    Check(!LhDropsSuspicious(36, 0, 0.12f), "Normal (12 %): 36 kills (1.01 %): not yet");
    Check(LhDropsSuspicious(37, 0, 0.12f), "Normal (12 %): 37 kills (0.89 %): ANOMALY");
    Check(!LhDropsSuspicious(100, 1, 0.25f), "a drop came: never");
    Check(!LhDropsSuspicious(0, 0, 0.25f) && !LhDropsSuspicious(50, 0, 0.0f), "no kills / no chance: never");
}

static void TestFlipVa(void)
{
    LhFlip f;
    int low = 0;
    LhFlipReset(&f);
    Check(!LhFlipStep(&f, 1) && !LhFlipStep(&f, 1) && LhFlipStep(&f, 0) && LhFlipStep(&f, 1) && f.flips == 2,
          "flicker: the first value is no flip, each change is one");
    LhFlipReset(&f);
    Check(f.flips == 0 && !LhFlipStep(&f, 0), "flicker: a reset starts again");
    Check(!LhVaStep(&low, 1000, 2048), "address space: 1048 MB left: nothing");
    Check(LhVaStep(&low, 1700, 2048) && low == 1, "address space: 348 MB left: the first warning");
    Check(!LhVaStep(&low, 1720, 2048), "address space: 328 MB left: the same step, quiet");
    Check(LhVaStep(&low, 1850, 2048) && low == 2, "address space: 198 MB left: the next step");
    Check(LhVaStep(&low, 1990, 2048) && low == 3, "address space: 58 MB left: the next step");
    Check(!LhVaStep(&low, 1800, 2048), "address space: freed again: no line (steps only go down)");
    Check(!LhVaStep(&low, 100, 0), "address space: total unknown: nothing");
}

static void TestIni(void)
{
    Check(LhIniSame("3000", "3000"), "ini: the same number");
    Check(LhIniSame(" On ", "on"), "ini: case and outer spaces ignored");
    Check(LhIniSame("2.0", "2") && LhIniSame("-1", "-1") && LhIniSame("+5", "5"), "ini: the same value as numbers");
    Check(!LhIniSame("x2", "Normal") && !LhIniSame("Shadow Cast", "Off"), "ini: other values differ");
    Check(LhIniSame("", "") && !LhIniSame("", "0"), "ini: empty is the empty default only");
    Check(!LhIniSame("0x10", "16"), "ini: hex text is text");
    Check(!LhIniSame("save\\quick2", "save\\quick"), "ini: paths compared as text");
    Check(LhIniSame("Monsters + Victims", "monsters + victims"), "ini: long values, case ignored");
}

int main(void)
{
    TestMd5();
    TestClassify();
    TestStats();
    TestRules();
    TestIdle();
    TestDrops();
    TestFlipVa();
    TestIni();
    printf("%d of %d checks passed\n", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
