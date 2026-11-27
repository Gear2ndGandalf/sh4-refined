// v3.35 FX335: the effect budget (staging/v335_feedback/w2_fx/PLAN.md, DESIGN.md; the user's choice for the v3.34 fps
// slow-downs with every special mod on: "Auto budget: everything stays on; when a frame runs over, the heavy effects thin
// out and come back when there is room"). Pure code (test\budget_sim_test.c): the per-frame cost ledger's names and its
// "top costs" text, and the governor: one level for Echo Doubles, After Shock and Death Presence together, stepped in a
// fixed order (BG_L*), down when frames run late while the effects cost a real share of the frame, back up after a calm
// period, with hysteresis (a hold after every step, a back-off that grows when a step up had to be taken back).
// budget.inc measures (QueryPerformanceCounter) and applies the levels (each effect fades what it thins: no popping).
// v3.56 PW356 (staging/v356_feedback/w5_powers/PLAN.md; the user's v3.55 request 10: "a fallback to reduce all powers
// effects if they slow down the game in 120fps or higher"): at a high rate (a frame target of 1000 / 115 ms or less) a
// frame is late from 1.06 x the target (a true logic rate slows the game on every miss), the effects' measured share
// counts from a quarter of their target, two spark levels come first (BGL_SPARK1 / 2: After Shock's arcs, tendrils,
// sparks and streak glow, Death Presence's trail fire and wisps at 60 / 35 %, Echo's light look), and frames that run
// late while the effects draw but cost little CPU (their GPU share is not measurable under d3d8) may take a BLIND step:
// kept only when the frames get 2 % shorter by its settle time, else taken back and not tried again for 20 s (doubling
// to 120 s). Below 115 fps the ladder and every rule are v3.55's (the spark levels are skipped). BgRateMayStep tells the
// frame-rate guard (w7 FR356) when the budget has nothing left to shed.
#ifndef BUDGET_SIM_H
#define BUDGET_SIM_H
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- the ledger: Refined's per-frame work, by part
// Exclusive times (a nested part is not counted in its parent). Draw parts (the scene pass and the filter), then the
// gameplay tick's parts (0x4EF982, the same frame). BGC_ECHO .. BGC_DFX are the governed effects' own draw work.
enum {
    BGC_ECHO, BGC_AI, BGC_DP, BGC_DFX,              // the governed effects (depth fix: inside Echo's / Death Presence's draws)
    BGC_ITEMS, BGC_FX, BGC_HUD, BGC_SUBS, BGC_MENU, BGC_WARM, BGC_LOG,
    BGC_SHADOW,                                     // v3.40 SH340: the soft shadow composite and the Shadows page's preview
    BGC_DOF,                                        // v3.43 DF343: depth of field (the frame before the light grid, its page preview)
    BGC_POST,                                       // v3.43 PP343: bloom, colour grading, the Otherworld lens (post.inc)
    BGC_T_ECHO, BGC_T_AI, BGC_T_DP, BGC_T_ENM, BGC_T_DROP, BGC_T_CAM, BGC_T_OTHER,
    BGC_N
};
#define BGC_EFFECTS 4        // BGC_ECHO .. BGC_DFX
#define BGC_FIRST_TICK BGC_T_ECHO
static const char *const kBgName[BGC_N] = {
    "echo", "after shock", "death presence", "depth fix",
    "item shine", "screen effects", "hud bars", "subtitles", "menus / quick save", "warm-up", "log",
    "shadows",  // v3.40 SH340
    "depth of field",  // v3.43 DF343
    "post effects",  // v3.43 PP343
    "echo tick", "after shock tick", "death presence tick", "enemies tick", "drops tick", "camera tick", "other ticks" };

static float BgSum(const float ms[BGC_N], int from, int to)  // [from, to)
{
    float s = 0.0f;
    int i;
    for (i = from; i < to; ++i) s += ms[i] > 0.0f ? ms[i] : 0.0f;
    return s;
}
static float BgEffectsMs(const float ms[BGC_N]) { return BgSum(ms, 0, BGC_EFFECTS); }
static float BgTotalMs(const float ms[BGC_N]) { return BgSum(ms, 0, BGC_N); }
// The top costs of a frame, largest first: at most `max` parts of at least minMs (none: ""). "echo 21.3 ms, after shock
// 8.1, death presence 3.2" (no parentheses: logcheck.py reads the room line's worst-frame context up to its ')').
static int BgTopText(const float ms[BGC_N], int max, float minMs, char *out, size_t n)
{
    int used[BGC_N], k, i, o = 0, got = 0;
    memset(used, 0, sizeof used);
    if (n) out[0] = 0;
    for (k = 0; k < max; ++k) {
        int best = -1;
        for (i = 0; i < BGC_N; ++i)
            if (!used[i] && ms[i] >= minMs && (best < 0 || ms[i] > ms[best])) best = i;
        if (best < 0) break;
        used[best] = 1;
        if (o >= 0 && (size_t)o + 48 < n) {
            const int m = sprintf_s(out + o, n - (size_t)o, got ? ", %s %.1f" : "%s %.1f ms", kBgName[best], ms[best]);
            if (m > 0) o += m;
        }
        ++got;
    }
    return got;
}

// ---------------------------------------------------------------- the governor
// Levels, in the fixed order they are taken (each includes the ones before it). A level that changes nothing with the
// current settings (Echo with one shadow has no 3rd body to thin) is skipped (the `useful` mask, budget.inc BgUseful).
enum {
    BGL_FULL,        // 0: everything as v3.34
    BGL_SPARK1,      // 1: v3.56 PW356, at a high rate only: the sparks, arcs and smoke at 60 % (After Shock's aura arcs, tendrils,
                     //    sparks and streak glow, Death Presence's trail fire and wisps; Echo's light look on the 2nd / 3rd shadow)
    BGL_SPARK2,      // 2: v3.56 PW356, at a high rate only: the same at 35 % (+ no feet streaks; Echo's light look on every shadow)
    BGL_ECHO3,       // 3: Echo: the 3rd shadow's body fades into its smoke (its flame coat, eyes and strikes stay)
    BGL_ECHOWEP,     // 4: Echo: no weapon redraw on the 2nd / 3rd shadow (v3.31's level 1)
    BGL_ECHOLITE,    // 5: Echo: the light look on the 2nd / 3rd (half the smoke, no edge layer) (v3.31's level 2)
    BGL_ECHO2,       // 6: Echo: the 2nd shadow's body fades into its smoke too
    BGL_AI5,         // 7: After Shock: fewer copies along the same path (Long 8 -> 5, Medium 5 -> 4; the stun keeps all)
    BGL_ECHOALL,     // 8: Echo: no weapon redraw at all, the ground discs fade (v3.31's level 3)
    BGL_AI3DP,       // 9: After Shock 3 copies; Death Presence 70 % of its volume's smoke puffs
    BGL_DP50,        // 10: Death Presence 50 %
    BGL_ECHO1,       // 11: Echo: the last shadow's body fades into its smoke too (the last resort)
    BGL_N            // (v3.56 PW356: the levels 1 .. 9 of v3.35 - v3.55 are 3 .. 11 now; the log names each level's text)
};
#define BG_MAX (BGL_N - 1)
#define BG_HR_LEVELS (1u << BGL_SPARK1 | 1u << BGL_SPARK2)  // v3.56 PW356: the levels taken only at a high rate
static const char *const kBgLevelText[BGL_N] = {
    "everything full",
    "high rate: the sparks, arcs and smoke at 60 %",  // v3.56 PW356
    "high rate: the sparks, arcs and smoke at 35 %",  // v3.56 PW356
    "Echo: the 3rd shadow's body fades into its smoke",
    "Echo: no weapon redraw on the 2nd / 3rd shadow",
    "Echo: the light look on the 2nd / 3rd shadow",
    "Echo: the 2nd shadow's body fades into its smoke",
    "After Shock: fewer copies along the same path, 8 -> 5 and 5 -> 4",  // (no parentheses: the room line's field)
    "Echo: no weapon redraw, the ground discs fade",
    "After Shock 3 copies; Death Presence 70 % of its puffs",
    "Death Presence 50 % of its puffs",
    "Echo: every shadow's body fades into its smoke" };

#define BG_T60_MS     6.0f    // the effects' share of a 60 fps frame (16.7 ms): 36 %; scaled by the frame target
#define BG_T_MIN      1.5f
#define BG_T_MAX      16.0f
#define BG_TAU        0.40f   // s: the smoothing of the late share and of the cost
#define BG_LATE_K     1.25f   // a frame longer than 1.25 x the target is late
#define BG_LATE_ON    0.25f   // over: at least 25 % of the frames late ...
#define BG_COST_ON    0.50f   // ... while the effects cost at least half their target,
#define BG_COST_HARD  2.00f   // or the effects alone cost twice their target (late or not)
#define BG_LATE_CALM  0.05f   // calm: under 5 % late
#define BG_DOWN_S     0.75f   // over this long: one level down (a 0.25 s burst of late frames, a door, a disk read: not)
#define BG_HOLD_DOWN  1.00f   // between two steps down (the new level's cost is seen first)
#define BG_HOLD_UP    2.00f   // after any step, before a step up
#define BG_UP_S       3.00f   // calm this long: one level back up ...
#define BG_UP_MAX    30.00f   // ... the back-off doubles (to 30 s) when a step up is taken back within BG_REBOUND
#define BG_REBOUND   10.00f
#define BG_FORGET    60.00f   // no step down for this long: the back-off is 3 s again
#define BG_SETTLE_S   1.50f   // the cost a level settles at: its smoothed cost this long after it was taken
#define BG_LIGHTER    0.70f   // the scene got lighter: the cost under 70 % of that: back up after BG_UP_S, the back-off forgotten
#define BG_SPIKE_MS  100.0f   // a frame this long is a hitch (a load, a first use, a disk read): not counted at all
// v3.56 PW356: the high frame-rate fallback
#define BG_HR_FRAME_MS (1000.0f / 115.0f)  // a frame target this short or shorter (120, 144, 180 fps) is a high rate
#define BG_HR_LATE_K    1.06f  // at a high rate a frame over 1.06 x the target is late (a true logic rate: every miss slows the game)
#define BG_HR_CALM      0.15f  // ... and calm under 15 % late (a frame limiter's jitter at 5.6 ms is not a miss)
#define BG_HR_COST_ON   0.25f  // the effects' measured work that counts as evidence at a high rate (x their target)
#define BG_HR_ACTIVE_MS 0.05f  // the effects draw at all (their smoothed work): a blind step may be tried
#define BG_HR_GAIN      0.03f  // a blind step must make the frames 3 % shorter (their mean from 0.5 s after it to its settle time
                               // against the smoothed length when it was taken), else it is taken back
#define BG_HR_POST0     0.50f  // s after a blind step: its frames' mean starts (the fade of what it thinned is done by then)
#define BG_HR_RETRY    20.0f   // s before the next blind try after a take-back; doubles each time ...
#define BG_HR_RETRY_MAX 120.0f // ... to this; a new room starts again at BG_HR_RETRY
#define BG_YIELD_S      1.0f   // BgRateMayStep: frames have missed this long with nothing the budget can shed

static float BgTargetMs(float t60, float frameMs)
{
    float t;
    if (!(t60 > 0.0f)) t60 = BG_T60_MS;
    if (!(frameMs > 1.0f) || frameMs > 200.0f) frameMs = 1000.0f / 60.0f;
    t = t60 * frameMs / (1000.0f / 60.0f);
    return t < BG_T_MIN ? BG_T_MIN : t > BG_T_MAX ? BG_T_MAX : t;
}

typedef struct {
    int level;              // 0 .. BG_MAX
    float lateE, costE;     // smoothed: the share of late frames, the effects' ms a frame
    float overT, calmT;     // s over / calm in a row (over decays)
    float holdT;            // s since the last step
    float sinceUp, sinceDown;
    float backoff;          // s of calm a step up needs now
    float settled;          // the cost this level settled at (BG_SETTLE_S after it was taken; 0 = not yet)
    int downs, ups, primed;
    // v3.56 PW356: the high-rate fallback
    int hr;                 // the last counted frame's target was a high rate
    int hrOff;              // set by the caller: no high-rate rule at any rate (v3.55's governor; [Graphics] EffectBudgetHighRate=0)
    float frameE;           // smoothed frame length (ms; the blind steps' verdict)
    int blindPend, blindBase;  // a blind step waits for its verdict; the level it was taken from
    float blindAt, blindGain;  // frameE when it was taken; the last verdict's gain (share)
    double postSum, postW;  // the frames since BG_HR_POST0 after it: their mean weighted by their length (as frameE's
                            // smoothing weights them: a time mean), sum(ms x ms) / sum(ms)
    float blindHold, blindRetry;  // s until a blind step may be tried again; the next hold-off
    int verdict;            // this frame: 1 a blind step kept, 2 taken back (0 none)
    int lastBlind;          // the last step down was blind
    int blindSteps, blindKept, takeBacks;
    float yieldT;           // s of frames missing at a high rate with nothing the budget can shed (BgRateMayStep)
} BgGov;
static void BgGovReset(BgGov *g)
{
    memset(g, 0, sizeof *g);
    g->backoff = BG_UP_S;
    g->holdT = g->sinceUp = g->sinceDown = 1.0e6f;
    g->blindRetry = BG_HR_RETRY;  // v3.56 PW356
}
// v3.56 PW356: a frame target of a high rate (120 fps and up)
static int BgHighRate(float targetFrameMs) { return targetFrameMs > 1.0f && targetFrameMs <= BG_HR_FRAME_MS + 1e-3f; }
// v3.56 PW356: a new room: a blind step may be tried at once again (a new scene), the hold-off starts at BG_HR_RETRY
static void BgGovRoomNew(BgGov *g)
{
    g->blindHold = 0.0f;
    g->blindRetry = BG_HR_RETRY;
}
// v3.56 PW356: the frame-rate guard (w7 FR356) may lower the logic rate now: frames have missed at a high rate for
// BG_YIELD_S while the budget had nothing to shed (no useful level left, no power drawing, or blind steps on hold)
static int BgRateMayStep(const BgGov *g) { return g->hr && g->yieldT >= BG_YIELD_S; }
// the next useful level above / below `from` (useful bit L = level L changes something now); -1 none
static int BgNextUp(unsigned useful, int from)  // a heavier thinning
{
    int l;
    for (l = from + 1; l <= BG_MAX; ++l)
        if (useful >> l & 1u) return l;
    return -1;
}
static int BgNextDown(unsigned useful, int from)  // a lighter one (0 is always useful)
{
    int l;
    for (l = from - 1; l > 0; --l)
        if (useful >> l & 1u) return l;
    return 0;
}
// One frame: dtS its length (s), frameMs the same in ms, costMs the effects' work in it, targetFrameMs the game's frame
// (1000 / rate), t60 the target share at 60 fps (BgTargetMs scales it). skip: a frame that must not count (the room's or
// a load's first 1.5 s, a warm-up, the first frames after a pause; the caller decides). Returns +1 a level down (thinner),
// -1 a level up, 0 no change.
// v3.56 PW356: at a high rate (BgHighRate) the late rule is BG_HR_LATE_K, the effects' evidence BG_HR_COST_ON, calm
// BG_HR_CALM, the spark levels are useful, and a blind step (late frames, the effects drawing, too little CPU work to
// be evidence) is judged at its settle time: kept (verdict 1) or taken back (verdict 2, returns -1). Below a high rate
// every rule is v3.55's and the spark levels are masked out.
static int BgGovStep(BgGov *g, float dtS, float frameMs, float costMs, float targetFrameMs, float t60, unsigned useful, int skip)
{
    const float T = BgTargetMs(t60, targetFrameMs);
    const int hr = !g->hrOff && BgHighRate(targetFrameMs);  // v3.56 PW356
    float a, late;
    int over, calm, nx, ev;
    g->verdict = 0;  // v3.56 PW356
    if (skip || !(dtS > 0.0f) || dtS > 0.5f || !(frameMs > 0.0f) || frameMs >= BG_SPIKE_MS || !(targetFrameMs > 1.0f)) return 0;
    if (!hr) useful &= ~BG_HR_LEVELS;  // v3.56 PW356: the spark levels only at a high rate
    if (!(costMs >= 0.0f)) costMs = 0.0f;
    if (costMs > 4.0f * T) costMs = 4.0f * T;  // (one slow part does not swamp the smoothing)
    late = frameMs > (hr ? BG_HR_LATE_K : BG_LATE_K) * targetFrameMs ? 1.0f : 0.0f;  // v3.56 PW356: BG_HR_LATE_K at a high rate
    a = 1.0f - expf(-dtS / BG_TAU);
    if (!g->primed) g->primed = 1, g->lateE = late, g->costE = costMs, g->frameE = frameMs;
    else g->lateE += (late - g->lateE) * a, g->costE += (costMs - g->costE) * a, g->frameE += (frameMs - g->frameE) * a;
    g->hr = hr;
    g->holdT += dtS, g->sinceUp += dtS, g->sinceDown += dtS;
    if (g->blindHold > 0.0f) g->blindHold -= dtS;  // v3.56 PW356
    if (g->sinceDown > BG_FORGET) g->backoff = BG_UP_S;
    if (!(g->settled > 0.0f) && g->holdT >= BG_SETTLE_S) g->settled = g->costE > 0.01f ? g->costE : 0.01f;
    if (hr) {  // v3.56 PW356: evidence from the measured work, or a blind try while the effects draw
        ev = (g->lateE >= BG_LATE_ON && g->costE >= BG_HR_COST_ON * T) || g->costE >= BG_COST_HARD * T;
        over = ev || (g->lateE >= BG_LATE_ON && g->costE >= BG_HR_ACTIVE_MS && !(g->blindHold > 0.0f) && !g->blindPend);
        calm = g->lateE < BG_HR_CALM && g->costE < BG_COST_HARD * T;
        {   // the frame-rate guard's go-ahead: frames miss and nothing can be shed now
            const int canHelp = g->blindPend || (BgNextUp(useful, g->level) > 0 && (ev || (g->costE >= BG_HR_ACTIVE_MS && !(g->blindHold > 0.0f))));
            g->yieldT = g->lateE >= BG_LATE_ON && !canHelp ? g->yieldT + dtS : 0.0f;
        }
    } else {
        ev = over = (g->lateE >= BG_LATE_ON && g->costE >= BG_COST_ON * T) || g->costE >= BG_COST_HARD * T;
        calm = g->lateE < BG_LATE_CALM && g->costE < BG_COST_HARD * T;
        g->yieldT = 0.0f;
    }
    if (over) g->overT += dtS, g->calmT = 0.0f;
    else {
        g->overT -= dtS;
        if (g->overT < 0.0f) g->overT = 0.0f;
        g->calmT = calm ? g->calmT + dtS : 0.0f;
    }
    if (g->blindPend && hr && g->holdT >= BG_HR_POST0) g->postSum += (double)frameMs * frameMs, g->postW += frameMs;  // v3.56 PW356
    if (g->blindPend && (!hr || g->holdT >= BG_SETTLE_S)) {  // v3.56 PW356: the blind step's verdict (none when the rate fell)
        const float post = g->postW > 0.0 ? (float)(g->postSum / g->postW) : g->frameE;
        const float gain = g->blindAt > 0.0f ? (g->blindAt - post) / g->blindAt : 0.0f;
        g->blindPend = 0;
        if (hr) {
            g->blindGain = gain;
            if (gain >= BG_HR_GAIN) {
                g->verdict = 1;
                ++g->blindKept;
            } else {  // no faster: taken back, no blind try for the hold-off (it doubles)
                g->verdict = 2;
                ++g->takeBacks;
                g->level = g->blindBase;
                g->holdT = g->sinceUp = 0.0f;
                g->overT = g->calmT = 0.0f;
                g->settled = 0.0f;
                g->blindHold = g->blindRetry;
                g->blindRetry = g->blindRetry * 2.0f > BG_HR_RETRY_MAX ? BG_HR_RETRY_MAX : g->blindRetry * 2.0f;
                ++g->ups;
                return -1;
            }
        }
    }
    if (g->overT >= BG_DOWN_S && g->holdT >= BG_HOLD_DOWN && !g->blindPend && (nx = BgNextUp(useful, g->level)) > 0) {
        if (g->sinceUp < BG_REBOUND) g->backoff = g->backoff * 2.0f > BG_UP_MAX ? BG_UP_MAX : g->backoff * 2.0f;
        g->lastBlind = hr && !ev;  // v3.56 PW356: no evidence now: a blind step (judged at its settle time)
        if (g->lastBlind) g->blindPend = 1, g->blindBase = g->level, g->blindAt = g->frameE, g->postSum = g->postW = 0.0, ++g->blindSteps;
        g->level = nx;
        g->holdT = g->sinceDown = 0.0f;
        g->overT = g->calmT = 0.0f;
        g->settled = 0.0f;
        ++g->downs;
        return 1;
    }
    if (g->level > 0 && g->holdT >= BG_HOLD_UP) {
        // back up: after the back-off's calm, or sooner (BG_UP_S) when the scene got lighter than this level settled at (the
        // fight is over: the old back-off no longer applies)
        const int lighter = g->settled > 0.0f && g->costE < BG_LIGHTER * g->settled;
        if (g->calmT >= (lighter ? BG_UP_S : g->backoff)) {
            if (lighter) g->backoff = BG_UP_S;
            g->level = BgNextDown(useful, g->level);
            g->holdT = g->sinceUp = 0.0f;
            g->calmT = 0.0f;
            g->settled = 0.0f;
            ++g->ups;
            return -1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------- what a level means for each effect
static int BgEchoLevel(int L)  // Echo's own budget level (v3.31 echo_sim.h: 1 no weapon on 2nd / 3rd, 2 the light look, 3 no weapon, no discs)
{
    return L >= BGL_ECHOALL ? 3 : L >= BGL_ECHOLITE ? 2 : L >= BGL_ECHOWEP ? 1 : 0;
}
static int BgEchoBodies(int L)  // shadows (by index) that keep a body: 3, 2, 1 or 0
{
    return L >= BGL_ECHO1 ? 0 : L >= BGL_ECHO2 ? 1 : L >= BGL_ECHO3 ? 2 : 3;
}
static int BgAiCopies(int L, int n)  // After Shock: copies drawn of a trail of n
{
    if (L >= BGL_AI3DP) return n < 3 ? n : 3;
    if (L >= BGL_AI5) return n >= 8 ? 5 : n >= 5 ? 4 : n;
    return n;
}
// 1 when copy k (1 = nearest .. n = the tail) of a trail of n is drawn with m of them: the nearest and the tail always,
// the rest spread evenly along the same path (the trail keeps its length; the positions do not move)
static int BgAiKeep(int k, int n, int m)
{
    int j;
    if (m >= n || n <= 1) return 1;
    if (m <= 1) return k == 1;
    for (j = 0; j < m; ++j)
        if ((int)floorf(1.0f + (float)j * (float)(n - 1) / (float)(m - 1) + 0.5f) == k) return 1;
    return 0;
}
static float BgDpKeep(int L) { return L >= BGL_DP50 ? 0.5f : L >= BGL_AI3DP ? 0.7f : 1.0f; }
// v3.56 PW356: the sparks, arcs and smoke kept at level L (1 below a high rate: the spark levels belong to it)
static float BgSparkK(int L, int hr) { return !hr ? 1.0f : L >= BGL_SPARK2 ? 0.35f : L >= BGL_SPARK1 ? 0.6f : 1.0f; }
// v3.56 PW356: n things thinned to the share k: how many to make (at least `min`, at most n), and the last one's alpha
// (its fraction of a whole one: as k falls the last one fades before it goes; 1 when nothing is thinned)
static int BgThinN(int n, float k, int min, float *lastA)
{
    float x;
    int m;
    if (lastA) *lastA = 1.0f;
    if (n <= 0) return 0;
    if (!(k < 0.999f)) return n;
    if (!(k > 0.0f)) k = 0.0f;
    x = (float)n * k;
    m = (int)ceilf(x - 1e-4f);
    if (m < min) m = min, x = (float)min;
    if (m > n) m = n, x = (float)n;
    if (lastA && m >= 1 && x < (float)m) *lastA = x - (float)(m - 1);
    if (lastA && *lastA > 1.0f) *lastA = 1.0f;  // (x a hair over a whole number)
    return m;
}
// a fade toward `to` at `perS` a second (the same at any frame rate)
static float BgRamp(float cur, float to, float dtS, float perS)
{
    const float d = perS * (dtS > 0.0f && dtS < 0.5f ? dtS : 0.0f);
    if (cur < to) return cur + d > to ? to : cur + d;
    return cur - d < to ? to : cur - d;
}
// Death Presence: puff i of n kept with share k (continuous: the last kept ones fade one after another, 4 puffs wide)
static float BgDpPuffK(int i, int n, float k)
{
    const float keepN = k * (float)n, x = (keepN - (float)i) / 4.0f;
    if (k >= 0.999f) return 1.0f;
    return x <= 0.0f ? 0.0f : x >= 1.0f ? 1.0f : x * x * (3.0f - 2.0f * x);
}
#endif
