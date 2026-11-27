// v3.56 PW356: the v3.55 effect-budget governor (src/budget_sim.h of v3.55.0, BgTargetMs .. BgGovStep, renamed Ref*), kept
// for test/budget_sim_test.c's regression check: below a high rate (115 fps) the v3.56 governor must take exactly the
// same steps at the same moments, its levels 1 .. 9 renumbered 3 .. 11 (the two spark levels are only taken at a high
// rate). The BG_* tuning macros are the v3.56 header's (unchanged values); REF_MAX is v3.55's deepest level.
#ifndef BUDGET_REF355_H
#define BUDGET_REF355_H
#define REF_MAX 9
static float RefTargetMs(float t60, float frameMs)
{
    float t;
    if (!(t60 > 0.0f)) t60 = BG_T60_MS;
    if (!(frameMs > 1.0f) || frameMs > 200.0f) frameMs = 1000.0f / 60.0f;
    t = t60 * frameMs / (1000.0f / 60.0f);
    return t < BG_T_MIN ? BG_T_MIN : t > BG_T_MAX ? BG_T_MAX : t;
}

typedef struct {
    int level;              // 0 .. REF_MAX
    float lateE, costE;     // smoothed: the share of late frames, the effects' ms a frame
    float overT, calmT;     // s over / calm in a row (over decays)
    float holdT;            // s since the last step
    float sinceUp, sinceDown;
    float backoff;          // s of calm a step up needs now
    float settled;          // the cost this level settled at (BG_SETTLE_S after it was taken; 0 = not yet)
    int downs, ups, primed;
} RefGov;
static void RefGovReset(RefGov *g)
{
    memset(g, 0, sizeof *g);
    g->backoff = BG_UP_S;
    g->holdT = g->sinceUp = g->sinceDown = 1.0e6f;
}
// the next useful level above / below `from` (useful bit L = level L changes something now); -1 none
static int RefNextUp(unsigned useful, int from)  // a heavier thinning
{
    int l;
    for (l = from + 1; l <= REF_MAX; ++l)
        if (useful >> l & 1u) return l;
    return -1;
}
static int RefNextDown(unsigned useful, int from)  // a lighter one (0 is always useful)
{
    int l;
    for (l = from - 1; l > 0; --l)
        if (useful >> l & 1u) return l;
    return 0;
}
// One frame: dtS its length (s), frameMs the same in ms, costMs the effects' work in it, targetFrameMs the game's frame
// (1000 / rate), t60 the target share at 60 fps (RefTargetMs scales it). skip: a frame that must not count (the room's or
// a load's first 1.5 s, a warm-up, the first frames after a pause; the caller decides). Returns +1 a level down (thinner),
// -1 a level up, 0 no change.
static int RefGovStep(RefGov *g, float dtS, float frameMs, float costMs, float targetFrameMs, float t60, unsigned useful, int skip)
{
    const float T = RefTargetMs(t60, targetFrameMs);
    float a, late;
    int over, calm, nx;
    if (skip || !(dtS > 0.0f) || dtS > 0.5f || !(frameMs > 0.0f) || frameMs >= BG_SPIKE_MS || !(targetFrameMs > 1.0f)) return 0;
    if (!(costMs >= 0.0f)) costMs = 0.0f;
    if (costMs > 4.0f * T) costMs = 4.0f * T;  // (one slow part does not swamp the smoothing)
    late = frameMs > BG_LATE_K * targetFrameMs ? 1.0f : 0.0f;
    a = 1.0f - expf(-dtS / BG_TAU);
    if (!g->primed) g->primed = 1, g->lateE = late, g->costE = costMs;
    else g->lateE += (late - g->lateE) * a, g->costE += (costMs - g->costE) * a;
    g->holdT += dtS, g->sinceUp += dtS, g->sinceDown += dtS;
    if (g->sinceDown > BG_FORGET) g->backoff = BG_UP_S;
    if (!(g->settled > 0.0f) && g->holdT >= BG_SETTLE_S) g->settled = g->costE > 0.01f ? g->costE : 0.01f;
    over = (g->lateE >= BG_LATE_ON && g->costE >= BG_COST_ON * T) || g->costE >= BG_COST_HARD * T;
    calm = g->lateE < BG_LATE_CALM && g->costE < BG_COST_HARD * T;
    if (over) g->overT += dtS, g->calmT = 0.0f;
    else {
        g->overT -= dtS;
        if (g->overT < 0.0f) g->overT = 0.0f;
        g->calmT = calm ? g->calmT + dtS : 0.0f;
    }
    if (g->overT >= BG_DOWN_S && g->holdT >= BG_HOLD_DOWN && (nx = RefNextUp(useful, g->level)) > 0) {
        if (g->sinceUp < BG_REBOUND) g->backoff = g->backoff * 2.0f > BG_UP_MAX ? BG_UP_MAX : g->backoff * 2.0f;
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
            g->level = RefNextDown(useful, g->level);
            g->holdT = g->sinceUp = 0.0f;
            g->calmT = 0.0f;
            g->settled = 0.0f;
            ++g->ups;
            return -1;
        }
    }
    return 0;
}

#endif
