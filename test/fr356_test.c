// v3.56 FR356: native test of src\rateguard_sim.h (the rate guard's step logic) and src\t60final_sim.h (the final room's
// per-tick values), test\build_fr356_test.cmd. The guard is fed synthetic play frames (held rates, slow PCs, hitches,
// pauses, the effect budget shedding, a retry after 10 minutes); the final room's swing and spear-spot step are run as the
// game runs them (0x4E4C50: phase += dt / 2 / (i + 1), angle += c x sin(phase) a tick; 0x5424F0: x += (t - x) x a a tick)
// at 30 with the stock values and at 60 / 120 / 180 with the converted ones, and compared per second of real time.
#include <stdio.h>
#include <math.h>
#include "../src/rateguard_sim.h"
#include "../src/t60final_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// feeds `secs` of frames of `ms` at the guard's own rate (as the Governor would run it); the first DOWN / RETRY event's time
typedef struct { int downs, retries, misses, held, oks; float firstDownAt; int lastCap; } Run;
static Run Feed(FrgState *g, float ms, float secs, int shedding)
{
    Run r = { 0, 0, 0, 0, 0, -1.0f, 0 };
    float t = 0.0f;
    while (t < secs * 1000.0f) {
        const int ev = FrgFrame(g, ms, FrgRate(g), shedding);
        t += ms;
        if (ev == FRG_EV_DOWN) {
            ++r.downs;
            if (r.firstDownAt < 0.0f) r.firstDownAt = t / 1000.0f;
        }
        r.retries += ev == FRG_EV_RETRY;
        r.misses += ev == FRG_EV_MISS;
        r.held += ev == FRG_EV_HELD;
        r.oks += ev == FRG_EV_OK;
    }
    r.lastCap = g->cap;
    return r;
}

static void TestLadder(void)
{
    CHECK(FrgLower(180) == 120 && FrgLower(120) == 60 && FrgLower(60) == 0 && FrgLower(30) == 0, "the ladder down: 180 120 60, none below");
    CHECK(FrgHigher(60) == 120 && FrgHigher(120) == 180 && FrgHigher(180) == 0 && FrgHigher(30) == 0, "the ladder up");
    CHECK(FrgDownIdx(120) == 0 && FrgDownIdx(180) == 1 && FrgDownIdx(60) == -1, "down counters for 120 / 180 only");
}

static void TestHeld(void)
{
    FrgState g;
    Run r;
    FrgReset(&g, 180);
    r = Feed(&g, 1000.0f / 180.0f, 600.0f, 0);
    CHECK(r.downs == 0 && g.cap == 0 && r.oks > 150, "180 held for 10 minutes: no step (%d ok windows)", r.oks);
    FrgReset(&g, 180);
    r = Feed(&g, 5.6f, 600.0f, 0);  // the user's v3.55 log: rooms 2:04 / 2:02 at 180, mean 5.6 ms
    CHECK(r.downs == 0, "the user's 5.6 ms at 180 (1.008 x): no step");
    FrgReset(&g, 180);
    r = Feed(&g, 1000.0f / 180.0f * 1.149f, 120.0f, 0);
    CHECK(r.downs == 0 && r.misses == 0, "1.149 x the frame: not a miss");
    FrgReset(&g, 180);
    r = Feed(&g, 1000.0f / 180.0f * 1.16f, 30.0f, 0);
    CHECK(r.downs == 1 && g.cap == 120, "1.16 x the frame: a step to 120");
    FrgReset(&g, 60);
    r = Feed(&g, 40.0f, 120.0f, 0);
    CHECK(r.downs == 0 && g.cap == 0 && r.misses == 0, "top 60: the guard never acts (even at 40 ms)");
    FrgReset(&g, 30);
    r = Feed(&g, 40.0f, 60.0f, 0);
    CHECK(r.downs == 0, "top 30: never acts");
}

static void TestSlow(void)
{
    FrgState g;
    Run r;
    FrgReset(&g, 180);
    r = Feed(&g, 7.0f, 60.0f, 0);  // 7 ms at 180 (1.26 x): 120 holds 7 ms (8.33 target)
    CHECK(r.downs == 1 && g.cap == 120, "7 ms at 180: one step, to 120 (cap %d)", g.cap);
    CHECK(r.firstDownAt > 7.0f && r.firstDownAt < 8.0f, "the step after the grace + 2 windows of 3 s: %.2f s", r.firstDownAt);
    CHECK(g.downs[1] == 1 && g.downs[0] == 0, "one step down from 180 counted");
    FrgReset(&g, 180);
    r = Feed(&g, 12.0f, 60.0f, 0);  // 12 ms: 120 misses too (9.58 limit), 60 holds
    CHECK(r.downs == 2 && g.cap == 60, "12 ms at 180: 180 -> 120 -> 60 (cap %d)", g.cap);
    r = Feed(&g, 30.0f, 120.0f, 0);
    CHECK(g.cap == 60 && r.downs == 0, "never below 60 (30 ms frames at 60)");
    FrgReset(&g, 120);
    r = Feed(&g, 10.0f, 60.0f, 0);
    CHECK(r.downs == 1 && g.cap == 60, "10 ms at 120: to 60");
}

static void TestHitchesAndBreaks(void)
{
    FrgState g;
    int i, ev, downs = 0;
    FrgReset(&g, 180);
    for (i = 0; i < 20000; ++i) {  // held frames with a 400 ms hitch every 2 s: the hitches are left out
        ev = FrgFrame(&g, (i % 360) == 0 ? 400.0f : 5.556f, 180, 0);
        downs += ev == FRG_EV_DOWN;
    }
    CHECK(downs == 0 && g.cap == 0, "hitches (400 ms) are left out: no step");
    FrgReset(&g, 180);
    for (i = 0; i < 2000; ++i) FrgFrame(&g, 7.0f, 180, 0);  // grace 1.5 s, then 12.5 s: past 2 windows -> a step
    CHECK(g.cap == 120, "a slow stretch steps down");
    // breaks: a pause between two missed windows does not wipe the count, but the grace starts again
    FrgReset(&g, 180);
    for (i = 0; i < (int)((1500.0f + 3100.0f) / 7.0f); ++i) FrgFrame(&g, 7.0f, 180, 0);
    CHECK(g.misses == 1 && g.cap == 0, "one missed window (misses %d)", g.misses);
    FrgBreak(&g);  // the pause menu
    CHECK(g.misses == 1 && g.graceMs == FRG_GRACE_MS && g.winN == 0, "a break keeps the count, drops the window, starts the grace");
    for (i = 0; i < (int)(1400.0f / 7.0f); ++i) FrgFrame(&g, 7.0f, 180, 0);
    CHECK(g.winN == 0, "inside the grace nothing is measured");
    for (i = 0; i < (int)(3300.0f / 7.0f); ++i) FrgFrame(&g, 7.0f, 180, 0);
    CHECK(g.cap == 120, "the second missed window after the pause steps down");
    // a frame at another rate (a menu at 30 that slipped through, a switch pending) breaks
    FrgReset(&g, 180);
    for (i = 0; i < 400; ++i) FrgFrame(&g, 7.0f, 180, 0);
    ev = FrgFrame(&g, 33.3f, 30, 0);
    CHECK(ev == FRG_EV_NONE && g.winN == 0 && g.graceMs == FRG_GRACE_MS, "a frame at another rate: a break");
    CHECK(FrgFrame(&g, 0.0f, 180, 0) == FRG_EV_NONE && FrgFrame(&g, -1.0f, 180, 0) == FRG_EV_NONE, "no time: nothing");
}

static void TestShedding(void)
{
    FrgState g;
    Run r;
    FrgReset(&g, 180);
    r = Feed(&g, 7.0f, 1.5f + 3.0f * FRG_HOLD_MAX + 0.5f, 1);
    CHECK(r.held == FRG_HOLD_MAX && r.downs == 0 && g.misses == 0, "the budget shedding: %d windows held, no step (held %d)", FRG_HOLD_MAX, r.held);
    r = Feed(&g, 7.0f, 6.5f, 1);
    CHECK(r.downs == 1 && g.cap == 120, "still missing after the holds: the step comes anyway");
    FrgReset(&g, 180);
    r = Feed(&g, 7.0f, 1.5f + 3.0f + 0.2f, 1);  // one held window
    r = Feed(&g, 1000.0f / 180.0f, 6.0f, 1);  // the shedding worked: frames held again
    CHECK(g.cap == 0 && g.holds == 0 && g.misses == 0, "the budget fixed it: no step, the holds forgotten");
}

static void TestRetryAndLock(void)
{
    FrgState g;
    Run r;
    FrgReset(&g, 180);
    r = Feed(&g, 7.0f, 10.0f, 0);
    CHECK(g.cap == 120, "down to 120");
    r = Feed(&g, 7.0f, 590.0f, 0);
    CHECK(r.retries == 0 && !g.retry, "9:50 clean at 120: no retry yet");
    r = Feed(&g, 7.0f, 20.0f, 0);
    CHECK(r.retries == 1 && g.retry, "10 minutes clean at 120: a retry waits for the room change");
    r = Feed(&g, 7.0f, 600.0f, 0);
    CHECK(r.retries == 0 && g.cap == 120, "the retry waits (no second event), the cap holds until a room change");
    CHECK(FrgRoomChange(&g) == 1 && g.cap == 0 && FrgRate(&g) == 180 && g.lastFrom == 120, "the room change: back to 180");
    CHECK(FrgRoomChange(&g) == 0, "a second room change changes nothing");
    r = Feed(&g, 7.0f, 10.0f, 0);
    CHECK(r.downs == 1 && g.cap == 120 && g.downs[1] == 2 && FrgLocked(&g, 180), "still slow: down again, 180 now locked");
    r = Feed(&g, 7.0f, 1300.0f, 0);
    CHECK(r.retries == 0 && !g.retry, "a locked rate is never tried again (21 minutes clean)");
    // a miss at the lower rate starts the clean time again
    FrgReset(&g, 180);
    Feed(&g, 7.0f, 10.0f, 0);
    Feed(&g, 7.0f, 500.0f, 0);
    FrgBreak(&g);  // a pause: the next window starts after the grace
    Feed(&g, 9.9f, 1.5f + 3.1f, 0);  // one whole window over 9.58 at 120: a miss
    CHECK(g.cleanMs < 3001.0f && g.cap == 120, "a missed window at 120 starts the clean time again (%.0f ms)", g.cleanMs);
    r = Feed(&g, 7.0f, 300.0f, 0);
    CHECK(r.retries == 0, "5 more minutes: not yet");
    r = Feed(&g, 7.0f, 310.0f, 0);
    CHECK(r.retries == 1, "10 minutes after the miss: the retry");
    // from 60 back to 120 (top 180)
    FrgReset(&g, 180);
    Feed(&g, 12.0f, 20.0f, 0);
    CHECK(g.cap == 60, "down to 60");
    r = Feed(&g, 12.0f, 610.0f, 0);
    CHECK(r.retries == 1, "10 minutes clean at 60: 120 tried");
    CHECK(FrgRoomChange(&g) == 1 && g.cap == 120, "the room change: 60 -> 120 (the top stays 180)");
}

static void TestTopAndReset(void)
{
    FrgState g;
    FrgReset(&g, 180);
    Feed(&g, 7.0f, 10.0f, 0);
    CHECK(g.cap == 120, "down to 120");
    FrgSetTop(&g, 120);  // the V-Sync cap became 120
    CHECK(g.cap == 0 && FrgRate(&g) == 120, "the top at the cap: the cap means nothing, lifted");
    FrgSetTop(&g, 180);
    CHECK(g.cap == 0 && g.top == 180 && g.downs[1] == 1, "the top back up: no cap, the run's count kept");
    Feed(&g, 7.0f, 10.0f, 0);
    FrgSetTop(&g, 180);
    CHECK(g.cap == 120, "the same top: nothing changes");
    FrgReset(&g, 180);
    CHECK(g.cap == 0 && g.downs[0] == 0 && g.downs[1] == 0 && !g.retry && g.misses == 0, "a new choice: everything starts again");
}

// ---------------------------------------------------------------- the final room
static void TestFinalValues(void)
{
    const double c = 0.0785, a = 0.2f;
    CHECK(T60fValue(c, T60F_SCALE, -1) == c && T60fValue(a, T60F_APPROACH, -1) == a, "30: the stock values themselves");
    CHECK(fabs(T60fValue(c, T60F_SCALE, 0) - 0.03925) < 1e-12 && fabs(T60fValue(c, T60F_SCALE, 1) - 0.019625) < 1e-12 &&
              fabs(T60fValue(c, T60F_SCALE, 2) - 0.0785 / 6.0) < 1e-12, "the swing step: 0.03925 / 0.019625 / 0.0130833 (the research's 60 / 120 rows)");
    CHECK(fabs(T60fValue(a, T60F_APPROACH, 0) - (1.0 - sqrt(1.0 - a))) < 1e-12, "the spot step at 60: 1 - 0.8^0.5");
    CHECK(fabs(T60fValue(a, T60F_APPROACH, 2) - (1.0 - pow(1.0 - a, 1.0 / 6.0))) < 1e-12, "the spot step at 180: 1 - 0.8^(1/6)");
    CHECK(T60fValue(c, T60F_SCALE, 3) == c && T60fValue(c, T60F_SCALE, -2) == c, "out of the table: stock");
}

// 0x4E4C50 as the game runs it for `secs`: the four swinging parts' angles (bones 2 / 4 / 6 / 8), their widest swing
static void Swing(int rate, double step, double secs, double out[4], double widest[4])
{
    double phase[5] = { 0 }, angle[5] = { 0 };
    const double dt = 1.0 / rate;
    const int ticks = (int)(secs * rate + 0.5);
    int t, i;
    for (i = 0; i < 4; ++i) widest[i] = 0.0;
    for (t = 0; t < ticks; ++t)
        for (i = 1; i < 5; ++i) {
            phase[i] += dt * 0.5 / (i + 1);
            angle[i] += sin(phase[i]) * step;
            if (fabs(angle[i]) > widest[i - 1]) widest[i - 1] = fabs(angle[i]);
        }
    for (i = 0; i < 4; ++i) out[i] = angle[i + 1];
}

static void TestFinalSwing(void)
{
    static const int rates[3] = { 60, 120, 180 };
    double a30[4], w30[4], a[4], w[4];
    int k, i;
    Swing(30, 0.0785, 40.0, a30, w30);
    for (k = 0; k < 3; ++k) {
        double worst = 0.0;
        Swing(rates[k], T60fValue(0.0785, T60F_SCALE, k), 40.0, a, w);
        for (i = 0; i < 4; ++i) {
            const double d = fabs(a[i] - a30[i]) / (w30[i] > 1e-9 ? w30[i] : 1.0);
            if (d > worst) worst = d;
        }
        CHECK(worst < 0.01, "the swing at %d with the converted step follows the 30 Hz one over 40 s (worst %.4f of its width)", rates[k], worst);
        Swing(rates[k], 0.0785, 40.0, a, w);  // the bug: the stock step at N
        CHECK(w[0] > w30[0] * (rates[k] / 30.0) * 0.95, "the stock step at %d swings %.1f x as wide (the user's report)", rates[k], w[0] / w30[0]);
    }
    CHECK(w30[0] > 18.5 && w30[0] < 19.0, "at 30 the first part swings 0 .. 2 x 0.0785 x 120 = 18.8 rad (widest %.2f)", w30[0]);
}

// 0x5424F0: what is left of the way to the spot after `secs`
static double Spot(int rate, double a, double secs)
{
    double x = 100.0;
    const int ticks = (int)(secs * rate + 0.5);
    int t;
    for (t = 0; t < ticks; ++t) x -= x * a;
    return x;
}
static void TestFinalSpot(void)
{
    static const int rates[3] = { 60, 120, 180 };
    const double l30 = Spot(30, 0.2f, 0.5);
    int k;
    for (k = 0; k < 3; ++k) {
        const double l = Spot(rates[k], T60fValue(0.2f, T60F_APPROACH, k), 0.5);
        CHECK(fabs(l - l30) < 1e-6 * 100.0, "the spot step at %d: %.4f left after 0.5 s, as at 30 (%.4f)", rates[k], l, l30);
    }
    CHECK(Spot(180, 0.2f, 0.5) < l30 * 0.01, "the stock step at 180 arrives 6x as fast (the leftover)");
}

int main(void)
{
    TestLadder();
    TestHeld();
    TestSlow();
    TestHitchesAndBreaks();
    TestShedding();
    TestRetryAndLock();
    TestTopAndReset();
    TestFinalValues();
    TestFinalSwing();
    TestFinalSpot();
    printf("fr356_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
