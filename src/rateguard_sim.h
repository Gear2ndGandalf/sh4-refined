// v3.56 FR356: the rate guard's pure logic (USER_FEEDBACK_v355 issue 6: "the game was running slow on 180fps, but fast on
// 120 and lower"). A true logic rate (FrameMode 0) runs the game one tick a frame: when this PC cannot draw `rate` frames a
// second, the whole game runs in slow motion. The guard watches the frames of plain gameplay at a rate above 60 and, when
// their mean over a 3 s window stays above 1.15 x the frame target for FRG_MISS_WINS windows in a row, steps the rate down
// to the next one the game supports (180 -> 120 -> 60) for this run. The ini's FrameRate never changes.
//  * Only play frames count (the caller passes nothing else): not a menu, a cut-scene, the front end, a load or a frame
//    while the game is not in front. After any other frame (a break) the next FRG_GRACE_MS are not measured; a frame
//    longer than FRG_HITCH_MS (a load, a disk read, a first use) is left out on its own.
//  * The effect budget (budget.inc, and w5 PW356's power shedding) acts first: a missed window while it is still shedding
//    is held (does not count), FRG_HOLD_MAX windows at most until a clean one; after that every miss counts.
//  * Back up: after FRG_RETRY_MS of clean windows at the lower rate, the next room change tries the higher rate again. A
//    rate the guard stepped down from FRG_LOCK_DOWNS times in this run is not tried again.
//  * A new choice in the Frame Rate row starts it all again (FrgReset); the V-Sync cap lowers the ladder's top (FrgSetTop).
//  * v3.58 GX358 (USER_FEEDBACK_v357 item 13): while the effect budget has nothing left to shed (budget_sim.h
//    BgRateMayStep with its quick rules: the caller's `fast`), the last FRG_FAST_MS of play frames decide too: kept in
//    FRG_SLOTS - 1 closed buckets of FRG_BUCKET_MS, judged as each bucket closes; a mean over FRG_MISS_K x the frame steps
//    down at once (the 3 s windows go on beside it). A break (a pause, a load, a capture, a door, the game out of front)
//    drops the buckets as it drops the window.
// Native test: test\fr356_test.c (test\build_fr356_test.cmd); with the budget: test\gx358_test.c. No game address in here.
#ifndef RATEGUARD_SIM_H
#define RATEGUARD_SIM_H

#define FRG_WIN_MS      3000.0f    // one window of play frames
#define FRG_MISS_K      1.15f      // a window misses when its mean frame is longer than this x the frame target
#define FRG_MISS_WINS   2          // missed windows in a row before a step down (6 s of play)
#define FRG_HOLD_MAX    3          // missed windows held at most while the effect budget is still shedding
#define FRG_GRACE_MS    1500.0f    // not measured after a break (a pause, a door, a rate switch, the game out of front)
#define FRG_HITCH_MS    250.0f     // a frame this long is a hitch: left out
#define FRG_RETRY_MS    600000.0f  // 10 minutes of clean play at the lower rate: the higher one is tried at the next room change
#define FRG_LOCK_DOWNS  2          // stepped down from a rate this often in a run: it is not tried again
#define FRG_FAST_MS     1000.0f    // v3.58 GX358: the window while the effect budget has nothing left to shed ...
#define FRG_BUCKET_MS   250.0f     // ... in buckets this long (judged as each one closes)
#define FRG_SLOTS       5          // ... the open one and the FRG_FAST_MS / FRG_BUCKET_MS closed ones

enum { FRG_EV_NONE, FRG_EV_OK, FRG_EV_MISS, FRG_EV_HELD, FRG_EV_DOWN, FRG_EV_RETRY };

typedef struct {
    int top;            // the chosen rate after the V-Sync cap (the ladder's top)
    int cap;            // 0 = none, else the rate the guard holds the game at (< top)
    float winMs;        // the open window: its play time, frame count and sum
    double winSum;
    unsigned winN;
    float graceMs;      // left of the grace after a break
    int misses, holds;  // missed windows in a row; held windows in a row
    float cleanMs;      // clean play at the capped rate (for the retry)
    int retry;          // 1 = a step up waits for the next room change
    int downs[2];       // steps down from 120 [0] and from 180 [1] in this run
    float lastMean, lastTarget;  // the last closed window (for the log)
    int lastFrom;       // the rate of the last step (for the log)
    // v3.58 GX358: the last second in buckets (the quick window); the last step was a quick one
    float bMs[FRG_SLOTS];
    double bSum[FRG_SLOTS];
    unsigned bN[FRG_SLOTS];
    int bOpen, bClosed;
    int lastFast;
} FrgState;

static int FrgLower(int rate) { return rate >= 180 ? 120 : rate >= 120 ? 60 : 0; }
static int FrgHigher(int rate) { return rate == 60 ? 120 : rate == 120 ? 180 : 0; }
static int FrgDownIdx(int rate) { return rate == 120 ? 0 : rate == 180 ? 1 : -1; }
static int FrgRate(const FrgState *g) { return g->cap ? g->cap : g->top; }  // the gameplay rate the guard allows
static int FrgLocked(const FrgState *g, int rate)  // stepped down from too often: not tried again in this run
{
    const int i = FrgDownIdx(rate);
    return i >= 0 && g->downs[i] >= FRG_LOCK_DOWNS;
}

// the open window is dropped and the next FRG_GRACE_MS are not measured (the missed-window count carries on)
static void FrgBreak(FrgState *g)
{
    int k;
    g->winMs = 0.0f;
    g->winSum = 0.0;
    g->winN = 0;
    g->graceMs = FRG_GRACE_MS;
    for (k = 0; k < FRG_SLOTS; ++k) g->bMs[k] = 0.0f, g->bSum[k] = 0.0, g->bN[k] = 0;  // v3.58 GX358
    g->bOpen = g->bClosed = 0;
}
static void FrgReset(FrgState *g, int top)  // a new choice (the Frame Rate row): everything starts again
{
    int i;
    g->top = top;
    g->cap = 0;
    g->misses = g->holds = 0;
    g->cleanMs = 0.0f;
    g->retry = 0;
    for (i = 0; i < 2; ++i) g->downs[i] = 0;
    g->lastMean = g->lastTarget = 0.0f;
    g->lastFrom = 0;
    g->lastFast = 0;  // v3.58 GX358
    FrgBreak(g);
}
// the ladder's top moved (the V-Sync cap or the monitor changed): a cap at or above it means nothing any more
static void FrgSetTop(FrgState *g, int top)
{
    if (top == g->top) return;
    g->top = top;
    if (g->cap && g->cap >= top) {
        g->cap = 0;
        g->retry = 0;
        g->cleanMs = 0.0f;
    }
    g->misses = g->holds = 0;
    FrgBreak(g);
}

// v3.58 GX358: a step down now (the window's or the quick one's)
static int FrgStepDown(FrgState *g, int rate, int fast)
{
    const int i = FrgDownIdx(rate);
    if (i >= 0) ++g->downs[i];
    g->lastFrom = rate;
    g->cap = FrgLower(rate);
    g->misses = g->holds = 0;
    g->retry = 0;
    g->lastFast = fast;
    FrgBreak(g);
    return FRG_EV_DOWN;
}
// v3.58 GX358: the frame into the open bucket; when it closes and the last FRG_FAST_MS are all in, their mean (else -1)
static float FrgBucketAdd(FrgState *g, float frameMs)
{
    double s = 0.0;
    unsigned n = 0;
    int k;
    g->bMs[g->bOpen] += frameMs;
    g->bSum[g->bOpen] += frameMs;
    ++g->bN[g->bOpen];
    if (g->bMs[g->bOpen] < FRG_BUCKET_MS) return -1.0f;
    g->bOpen = (g->bOpen + 1) % FRG_SLOTS;
    g->bMs[g->bOpen] = 0.0f, g->bSum[g->bOpen] = 0.0, g->bN[g->bOpen] = 0;
    if (g->bClosed < FRG_SLOTS - 1) ++g->bClosed;
    if (g->bClosed < FRG_SLOTS - 1) return -1.0f;
    for (k = 0; k < FRG_SLOTS; ++k)
        if (k != g->bOpen) s += g->bSum[k], n += g->bN[k];
    return n ? (float)(s / (double)n) : -1.0f;
}

// One play frame of frameMs at `rate` (the rate in force while it ran); shedding = the effect budget stepped down within
// the last window. Returns FRG_EV_*: FRG_EV_DOWN with g->cap set to the new rate, FRG_EV_RETRY when a step up waits for the
// next room change; FRG_EV_OK / _MISS / _HELD when a window closed.
// v3.58 GX358: fast = the effect budget has nothing left to shed (its quick go-ahead): the last second decides too.
static int FrgFrameEx(FrgState *g, float frameMs, int rate, int shedding, int fast)
{
    float mean, target, quick;
    if (g->top <= 60 || rate != FrgRate(g) || rate < 10) {  // nothing to guard, or the frame ran at another rate
        FrgBreak(g);
        return FRG_EV_NONE;
    }
    if (!(frameMs > 0.0f) || frameMs > FRG_HITCH_MS) return FRG_EV_NONE;
    if (g->graceMs > 0.0f) {
        g->graceMs -= frameMs;
        return FRG_EV_NONE;
    }
    g->winMs += frameMs;
    g->winSum += frameMs;
    ++g->winN;
    quick = FrgBucketAdd(g, frameMs);  // v3.58 GX358: the last second
    if (fast && quick > 0.0f && quick > FRG_MISS_K * (1000.0f / (float)rate) && FrgLower(rate)) {
        g->lastMean = quick;
        g->lastTarget = 1000.0f / (float)rate;
        g->cleanMs = 0.0f;
        return FrgStepDown(g, rate, 1);
    }
    if (g->winMs < FRG_WIN_MS) return FRG_EV_NONE;
    mean = (float)(g->winSum / (double)g->winN);
    target = 1000.0f / (float)rate;
    g->lastMean = mean;
    g->lastTarget = target;
    {
        const float won = g->winMs;
        g->winMs = 0.0f;
        g->winSum = 0.0;
        g->winN = 0;
        if (mean > FRG_MISS_K * target) {
            g->cleanMs = 0.0f;
            if (shedding && g->holds < FRG_HOLD_MAX) {  // the holds run out: from then on every miss counts until a clean window
                ++g->holds;
                return FRG_EV_HELD;
            }
            if (++g->misses >= FRG_MISS_WINS && FrgLower(rate)) return FrgStepDown(g, rate, 0);  // v3.58 GX358: FrgStepDown
            return FRG_EV_MISS;
        }
        g->misses = g->holds = 0;
        if (g->cap) {
            const int up = FrgHigher(g->cap);
            g->cleanMs += won;
            if (!g->retry && g->cleanMs >= FRG_RETRY_MS && up && up <= g->top && !FrgLocked(g, up)) {
                g->retry = 1;
                return FRG_EV_RETRY;
            }
        }
        return FRG_EV_OK;
    }
}
static int FrgFrame(FrgState *g, float frameMs, int rate, int shedding) { return FrgFrameEx(g, frameMs, rate, shedding, 0); }  // v3.56's call

// A room change: a waiting step up is taken now (1 = the cap changed). The room's first frames are not measured.
static int FrgRoomChange(FrgState *g)
{
    int changed = 0;
    if (g->retry && g->cap) {
        const int up = FrgHigher(g->cap);
        g->lastFrom = g->cap;
        g->cap = up && up < g->top ? up : 0;
        changed = 1;
    }
    g->retry = 0;
    if (changed) {
        g->cleanMs = 0.0f;
        g->misses = g->holds = 0;
    }
    FrgBreak(g);
    return changed;
}

#endif
