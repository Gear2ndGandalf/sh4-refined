// v3.59 FR359: native test of src\cutrate_sim.h (which phase-1 scenes run at the gameplay rate, the stream clock, the
// camera-cut judge) and src\wv359_table.h (the window view's per-tick values), test\build_fr359_test.cmd.
// The stream is played as DemoTrackPlayerUpdate 0x560860 plays it: frame += step in a float each tick, the pair of
// blocks cur = floor(frame), t = frame - cur, and with the snap t = 0 on a pair judged a cut.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/cutrate_sim.h"

typedef unsigned long DWORD;
typedef unsigned char BYTE;
enum { RT_F32, RT_I8, RT_I32, RT_PTR, RT_I16 };
typedef struct { DWORD va; int off, type, len; BYTE orig[16]; double v[3]; int group; } RateTail;
#include "../src/wv359_table.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned g_seed = 12345;
static double Rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return (double)((g_seed >> 8) & 0xFFFF) / 65536.0; }

static void TestPhaseRule(void)
{
    CHECK(CrViewKind(1, 0x69843C, 0) == CR_WINDOW && CrViewKind(1, 0x698468, 0) == CR_WINDOW, "window records");
    CHECK(CrViewKind(1, 0x61B6B8, 0) == CR_PEEP && CrViewKind(1, 0x61B86C, 0) == CR_PEEP && CrViewKind(1, 0x61B790, 0) == CR_PEEP, "peepholes");
    CHECK(CrViewKind(1, 0, 1) == CR_PEEP, "the peeping flag");
    CHECK(CrViewKind(1, 0, 0) == CR_SCENE && CrViewKind(2, 0x69843C, 1) == CR_SCENE, "a Room 302 story scene, another stage");
    CHECK(CrViewKind(1, 0x61C3FC, 0) == CR_SCENE, "an examine record is no view");
    CHECK(CrPhase1Fast(CR_SCENE, 1, 0, 1, 1, 0) && !CrPhase1Fast(CR_SCENE, 0, 1, 1, 1, 1), "CutsceneRate decides scenes");
    CHECK(!CrPhase1Fast(CR_SCENE, 1, 1, 0, 1, 1) && !CrPhase1Fast(CR_SCENE, 1, 1, 1, 0, 1), "a scene needs the snap and the clock");
    CHECK(CrPhase1Fast(CR_WINDOW, 0, 1, 0, 0, 1) && !CrPhase1Fast(CR_WINDOW, 1, 1, 1, 1, 0) && !CrPhase1Fast(CR_WINDOW, 1, 0, 1, 1, 1),
          "the window view: ViewRate and its sites");
    CHECK(CrPhase1Fast(CR_PEEP, 0, 1, 1, 1, 0) && !CrPhase1Fast(CR_PEEP, 1, 0, 1, 1, 1) && !CrPhase1Fast(CR_PEEP, 0, 1, 0, 1, 1),
          "peepholes: ViewRate, the snap and the clock");
}

// plays `secs` at `rate` with frame times from `ms(i)`; returns the stream's lag behind real time in frames at the end
static double PlayClock(int rate, double secs, double lateShare, double lateK, double *maxStep, int useClock)
{
    CrClock c;
    float frame = 0.0f;
    double real = 0.0;
    const double nom = 1.0 / rate;
    CrClockReset(&c);
    *maxStep = 0.0;
    while (real < secs) {
        const double dt = Rnd() < lateShare ? nom * lateK : nom;
        float step;
        real += dt;
        step = useClock ? CrClockStep(&c, rate, dt, frame) : (float)(30.0f / (float)rate);
        if (step > *maxStep) *maxStep = step;
        frame += step;
    }
    return real * 30.0 - frame;
}

static void TestClock(void)
{
    static const int rates[3] = { 60, 120, 180 };
    int r;
    double mx, lag;
    for (r = 0; r < 3; ++r) {
        lag = PlayClock(rates[r], 240.0, 0.0, 1.0, &mx, 1);
        CHECK(fabs(lag) < 0.5, "%d: steady frames, 4 min: the stream %.3f frames off real time", rates[r], lag);
        lag = PlayClock(rates[r], 240.0, 0.4, 1.4, &mx, 1);
        CHECK(fabs(lag) < 1.0, "%d: 40 %% of frames 1.4 x late: the stream %.3f frames off real time (the voice plays on)", rates[r], lag);
        CHECK(mx <= 3.0 * 30.0 / rates[r] + 1e-4, "%d: a step is never over 3 nominal steps (%.4f)", rates[r], mx);
        lag = PlayClock(rates[r], 240.0, 0.4, 1.4, &mx, 0);
        CHECK(lag > 30.0, "%d: without the clock the same frames put the stream %.1f frames behind (why it is needed)", rates[r], lag);
    }
    {   // float rounding: 180 at steady frames for 4 min; stock step accumulates, the clock does not
        double lagStock = PlayClock(180, 240.0, 0.0, 1.0, &mx, 0), lagClock = PlayClock(180, 240.0, 0.0, 1.0, &mx, 1);
        CHECK(fabs(lagClock) < 0.05, "180: the clock's float error %.4f frames", lagClock);
        printf("info: 180 fps, 4 min, steady: the float step alone drifts %.3f frames, the clock %.4f\n", lagStock, lagClock);
    }
    {   // a hitch of 80 ms is caught up within 0.1 s; a pause (0.5 s gap) restarts without a jump; a new scene resyncs
        CrClock c;
        float frame = 100.0f, s;
        int i;
        double before;
        CrClockReset(&c);
        for (i = 0; i < 180; ++i) frame += CrClockStep(&c, 180, 1.0 / 180, frame);
        before = frame;
        frame += CrClockStep(&c, 180, 0.080, frame);  // the hitch: 2.4 frames due, at most 0.5 now
        CHECK(frame - before <= 0.5f + 1e-4f, "the hitch tick steps at most 3 nominal (%.3f)", frame - before);
        for (i = 0; i < 18; ++i) frame += CrClockStep(&c, 180, 1.0 / 180, frame);
        CHECK(fabs((frame - before) - (0.080 + 18.0 / 180) * 30.0) < 0.05, "caught up after 0.1 s: %.3f of %.3f frames",
              frame - before, (0.080 + 18.0 / 180) * 30.0);
        before = frame;
        s = CrClockStep(&c, 180, 0.5, frame);  // v3.59 FR359-001: a 0.5 s gap with no break is a hitch: caught up, bounded
        CHECK(fabs(s - 0.5f) < 1e-4f && c.debt > 14.0, "a 0.5 s hitch: the bounded step %.4f, %.2f frames still due", s, c.debt);
        CrClockBreak(&c);  // v3.59 FR359-001: a known break (the pause): no catch-up
        s = CrClockStep(&c, 180, 0.5, frame);
        CHECK(fabs(s - 30.0f / 180) < 1e-4f && c.debt == 0.0, "after a break: a nominal step (%.4f), no debt", s);
        frame = 0.0f;  // the next scene starts at its first frame
        s = CrClockStep(&c, 180, 1.0 / 180, frame);
        CHECK(fabs(s - 30.0f / 180) < 1e-4f, "a new scene (the frame jumped back): resync, a nominal step (%.4f)", s);
        CHECK(CrClockStep(&c, 5, 0.01, 0.0f) == 1.0f, "a rate out of range: 1");
    }
}

// v3.59 FR359-001: a scene played at `rate` (frames on time, or 40 % of them 1.4 x late) with one event at 10 s; the voice
// plays on in real time except during a pause. Returns the stream's offset from the voice at the end, in frames;
// *maxStep = the largest step, *backAt = seconds after the event until the stream is within 1 frame of the voice.
enum { EV_NONE, EV_HITCH, EV_PAUSE, EV_ALTTAB, EV_SCENE, EV_REPEAT };
static double PlayEvent(int rate, int ev, double len, double lateShare, double secs, double *maxStep, double *backAt)
{
    CrClock c;
    float frame = 0.0f;
    double real = 0.0, voice = 0.0, evAt = -1.0;
    const double nom = 1.0 / rate;
    int done = 0, broke = 0;
    CrClockReset(&c);
    *maxStep = 0.0;
    *backAt = -1.0;
    while (real < secs) {
        double dt = Rnd() < lateShare ? nom * 1.4 : nom;
        float step;
        if (!done && real >= 10.0 && ev != EV_NONE && ev != EV_REPEAT) {
            done = 1;
            evAt = real;
            if (ev == EV_HITCH) dt = len;               // one tick took len s; the voice played on
            if (ev == EV_PAUSE || ev == EV_ALTTAB) {    // the frame end saw the break; the voice was paused for len s
                CrClockBreak(&c);
                dt = len;
                voice -= len;                           // (the voice resumes where it stopped)
            }
            if (ev == EV_SCENE) {                       // a new scene: its stream and voice start at 0
                CrClockBreak(&c);
                frame = 0.0f;
                voice = -dt;
            }
            broke = ev != EV_HITCH;
        }
        if (ev == EV_REPEAT && real > 1.0 && real < secs - 5.0 && fmod(real, 2.0) < nom) dt = len;  // a hitch every 2 s (from 1 s on, none in the last 5 s)
        real += dt;
        voice += dt;
        step = CrClockStep(&c, rate, dt, frame);
        if (step > *maxStep) *maxStep = step;
        frame += step;
        if (evAt >= 0.0 && *backAt < 0.0 && fabs(voice * 30.0 - frame) < 1.0) *backAt = real - evAt;
    }
    (void)broke;
    return voice * 30.0 - frame;
}

static void TestHitches(void)
{
    static const int rates[3] = { 60, 120, 180 };
    int r;
    double mx, back, lag;
    for (r = 0; r < 3; ++r) {
        const int n = rates[r];
        const double cap = 3.0 * 30.0 / n < 1.0 ? 3.0 * 30.0 / n : 1.0, speed = cap * n / 30.0;  // the catch-up pace
        lag = PlayEvent(n, EV_HITCH, 1.0, 0.0, 30.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && back > 0.0 && back < 1.0 + 1.0 / (speed - 1.0) + 0.2 && mx <= cap + 1e-4,
              "%d: a 1 s hitch is caught up in %.2f s (step at most %.3f of %.3f), the end %.3f frames off", n, back, mx, cap, lag);
        lag = PlayEvent(n, EV_HITCH, 5.0, 0.0, 30.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && back > 0.0 && back < 5.0 + 5.0 / (speed - 1.0) + 0.2 && mx <= cap + 1e-4,
              "%d: a 5 s hitch is caught up in %.2f s (step at most %.3f), the end %.3f frames off", n, back, mx, lag);
        lag = PlayEvent(n, EV_HITCH, 5.0, 0.4, 40.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && mx <= cap + 1e-4, "%d: a 5 s hitch among 40 %% late frames: the end %.3f frames off", n, lag);
        lag = PlayEvent(n, EV_PAUSE, 8.0, 0.0, 30.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && mx <= 30.0 / n + 1e-4, "%d: an 8 s pause (break, the voice paused): no catch-up (step at most %.3f), %.3f frames off", n, mx, lag);
        lag = PlayEvent(n, EV_ALTTAB, 3.0, 0.0, 30.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && mx <= 30.0 / n + 1e-4, "%d: Alt+Tab 3 s (break): no catch-up, %.3f frames off", n, lag);
        lag = PlayEvent(n, EV_SCENE, 0.0, 0.0, 30.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && mx <= 30.0 / n + 1e-4, "%d: a new scene: in step with its voice (%.3f frames), no burst", n, lag);
        lag = PlayEvent(n, EV_REPEAT, 0.15, 0.0, 240.0, &mx, &back);
        CHECK(fabs(lag) < 1.0 && mx <= cap + 1e-4, "%d: a 150 ms hitch every 2 s for 4 min: %.3f frames off at the end", n, lag);
        lag = PlayEvent(n, EV_NONE, 0.0, 0.4, 240.0, &mx, &back);
        CHECK(fabs(lag) < 1.0, "%d: 40 %% of frames 1.4 x late for 4 min: %.3f frames off", n, lag);
    }
    {   // past 10 s behind the rest is lost (counted), never a burst; a stall over 30 s restarts
        CrClock c;
        float frame = 0.0f, s;
        CrClockReset(&c);
        frame += CrClockStep(&c, 180, 1.0 / 180, frame);
        frame += CrClockStep(&c, 180, 1.0 / 180, frame);
        s = CrClockStep(&c, 180, 20.0, frame);
        CHECK(s <= 0.5f + 1e-4f && c.debt <= CR_DEBT_MAX && c.lostF > 290.0, "a 20 s hitch: step %.3f, debt %.1f, %.1f frames lost", s, c.debt, c.lostF);
        frame += s;
        s = CrClockStep(&c, 180, 40.0, frame);
        CHECK(fabs(s - 30.0f / 180) < 1e-4f && c.debt == 0.0, "a 40 s stall restarts (%.4f)", s);
    }
}

// a synthetic stream: one camera (0x80 rot, 0x81 pos, 0x82 view angle) and one actor bone (kind 4)
static BYTE g_player[16 * 12], g_rec[2][64], g_blk[2][CR_BLOCK_MAX];
static void MakeStream(int version)
{
    short *r;
    memset(g_player, 0, sizeof g_player);
    memset(g_rec, 0, sizeof g_rec);
    *(void **)(g_player + 0) = (void *)g_player;  // any non-null object
    *(BYTE **)(g_player + 8) = g_rec[0];
    *(void **)(g_player + 12) = (void *)g_player;
    *(BYTE **)(g_player + 20) = g_rec[1];
    r = (short *)g_rec[0];  // camera: data at 16, three channels at rec + 8
    r[0] = 1, r[1] = 16, r[2] = 3, r[3] = 8;
    r[4] = version < 1 ? 0xD : 0x80, r[5] = 0, r[6] = 0;        // rotation at 16
    r[8] = version < 1 ? 0xE : 0x81, r[9] = 0, r[10] = 16;      // position at 32
    r[12] = version < 1 ? 0xF : 0x82, r[13] = 0, r[14] = 32;    // value 0 at 48
    r = (short *)g_rec[1];  // actor: data at 64, one position channel
    r[0] = 2, r[1] = 64, r[2] = 1, r[3] = 8;
    r[4] = 4, r[5] = 0, r[6] = 0;
}
static void SetCam(BYTE *blk, float x, float yawDeg, float fov, float actorX)
{
    float *q = (float *)(blk + 16), *p = (float *)(blk + 32), *v = (float *)(blk + 48), *a = (float *)(blk + 64);
    q[0] = 0.0f, q[1] = sinf(yawDeg * 0.5f / 57.29578f), q[2] = 0.0f, q[3] = cosf(yawDeg * 0.5f / 57.29578f);
    p[0] = x, p[1] = -1500.0f, p[2] = 300.0f, p[3] = 1.0f;
    v[0] = fov;
    a[0] = actorX, a[1] = 0.0f, a[2] = 0.0f, a[3] = 1.0f;
}
static int Judge(float x0, float y0, float f0, float a0, float x1, float y1, float f1, float a1, float prev, int version)
{
    CrJudge j;
    MakeStream(version);
    SetCam(g_blk[0], x0, y0, f0, a0);
    SetCam(g_blk[1], x1, y1, f1, a1);
    CrJudgeBlocks(g_player, g_blk[0], g_blk[1], version, prev, &j);
    return j.mask;
}

static void TestJudge(void)
{
    int v;
    for (v = 0; v < 2; ++v) {
        CHECK(Judge(0, 10, 0.785f, 0, 5, 11, 0.785f, 3, 5, v) == 0, "v%d: a dolly of 5 u and 1 deg, an actor walking 3 u: no cut", v);
        CHECK(Judge(0, 10, 0.785f, 0, 500, 10, 0.785f, 0, 5, v) & CR_CUT_CAM_MOVE, "v%d: the camera 500 u away: a cut", v);
        CHECK(Judge(0, 10, 0.785f, 0, 0, 40, 0.785f, 0, 0, v) & CR_CUT_CAM_TURN, "v%d: the camera turned 30 deg: a cut", v);
        CHECK(Judge(0, 10, 0.785f, 0, 0, 10, 0.5f, 0, 0, v) & CR_CUT_CAM_FOV, "v%d: the view angle 0.785 -> 0.5: a cut", v);
        CHECK(Judge(0, 10, 0.785f, 0, 0, 10, 0.785f, 400, 0, v) == CR_CUT_OBJ, "v%d: an actor 400 u elsewhere: held", v);
        CHECK(Judge(0, 10, 0.785f, 0, 45, 10, 0.785f, 0, 2, v) & CR_CUT_CAM_MOVE, "v%d: 45 u after 2 u: a cut", v);
        CHECK(Judge(0, 10, 0.785f, 0, 45, 10, 0.785f, 0, 20, v) == 0, "v%d: 45 u after 20 u: a fast dolly", v);
        CHECK(Judge(0, 10, 0.785f, 0, sqrtf(-1.0f), 10, 0.785f, 0, 0, v) & CR_CUT_CAM_MOVE, "v%d: a NaN position: held", v);
    }
    CHECK(fabsf(CrQuatAngleDeg((const float[]){ 0, 0, 0, 1 }, (const float[]){ 0, 0, 0, -1 })) < 1e-3f, "q and -q are one rotation");
}

// stock (30) against 180 with and without the snap: a camera cut between frames 10 and 11
static void TestPlayback(void)
{
    const int rate = 180;
    float frame = 0.0f, step = 30.0f / rate, worstMix = 0.0f, worstMixSnap = 0.0f;
    int tick, cutShownAt = -1, cutShownAtSnap = -1;
    for (tick = 0; tick < 3 * rate; ++tick) {
        const int cur = (int)floorf(frame + 1e-4f);
        float t = frame - (float)cur, x, xs;
        const int isCut = cur == 10;
        if (t < 0.0f) t = 0.0f;
        // camera x: 0 before the cut, 1000 after it (frame 11 on)
        x = cur < 10 ? 0.0f : cur > 10 ? 1000.0f : 1000.0f * t;
        xs = cur < 10 ? 0.0f : cur > 10 ? 1000.0f : isCut ? 0.0f : 1000.0f * t;  // the snap: t = 0 on the cut pair
        if (x > 1.0f && x < 999.0f && x > worstMix) worstMix = x;
        if (xs > 1.0f && xs < 999.0f && xs > worstMixSnap) worstMixSnap = xs;
        if (cutShownAt < 0 && x > 1.0f) cutShownAt = tick;
        if (cutShownAtSnap < 0 && xs > 999.0f) cutShownAtSnap = tick;
        frame += step;
    }
    CHECK(worstMix > 1.0f, "without the snap 180 fps shows a blended camera (%.0f u between the shots)", worstMix);
    CHECK(worstMixSnap == 0.0f, "with the snap no picture is between the shots (%.0f)", worstMixSnap);
    CHECK(abs(cutShownAtSnap - 11 * rate / 30) <= 1, "the cut shows at frame 11 = tick %d (stock moment %d)", cutShownAtSnap, 11 * rate / 30);
}

static double WvStock(DWORD va)  // the stock constant each RT_PTR / RT_F32 row reads (research_window_view.md)
{
    switch (va) {
    case 0x54BB6D: case 0x54BBBA: return 1.0 / 24.0;
    case 0x54D045: return 0.05;
    case 0x54D0A4: return 1.0;
    case 0x54D0B6: return 0.2;
    case 0x54F81E: case 0x54F8FE: return 0.0001;
    case 0x54F854: return 0.01;
    case 0x69949C: return 15.0;
    case 0x54FBCE: return 0.2;
    case 0x54FBD5: return 0.1;
    case 0x699664: return 7.0;
    case 0x69966C: return 10.0;
    case 0x699674: return 6.0;
    case 0x69967C: return 9.0;
    case 0x699684: return 24.0;
    case 0x55036D: case 0x5504C5: return 15.0;
    }
    return -1.0;
}
static int WvRow(DWORD va)
{
    int i;
    for (i = 0; i < NWV359; ++i)
        if (kWv359[i].va == va) return i;
    return -1;
}

static void TestWindowTable(void)
{
    static const struct { DWORD m, b; int M, B; } kDelay[] = {
        { 0x54E204, 0x54E20B, 15, 8 }, { 0x54E26D, 0x54E27A, 15, 8 }, { 0x54E42E, 0x54E435, 15, 8 }, { 0x54E461, 0x54E468, 15, 8 },
        { 0x54E216, 0x54E21D, 127, 32 }, { 0x54E28D, 0x54E29A, 127, 32 }, { 0x54E440, 0x54E447, 127, 32 }, { 0x54E473, 0x54E47A, 127, 32 },
        { 0x550BAA, 0x550BBD, 31, 16 }, { 0x550C3C, 0x550C58, 31, 32 }, { 0x550C48, 0x550C58, 63, 32 },
    };
    int i, r;
    for (i = 0; i < NWV359; ++i) {  // every float row: stock x m (the mouse, the tangents) or x k (everything else)
        const RateTail *e = &kWv359[i];
        const double s = WvStock(e->va);
        int up;
        if (e->type != RT_PTR && e->type != RT_F32) continue;
        CHECK(s > 0.0, "%06lX: a stock value is known", e->va);
        up = e->va == 0x54BB6D || e->va == 0x54BBBA || e->va == 0x55036D || e->va == 0x5504C5;
        if (e->type == RT_F32) {
            float f;
            memcpy(&f, e->orig, 4);
            CHECK(fabs(f - s) < 1e-6 * s, "%06lX: the stock bytes are %g", e->va, s);
        }
        for (r = 0; r < 3; ++r) {
            const double m = (r + 1) * 2.0, want = up ? s * m : s / m;
            CHECK(fabs(e->v[r] - want) <= 1e-5 * want, "%06lX at %d: %g, want %g", e->va, (r + 1) * 60, e->v[r], want);
        }
    }
    for (i = 0; i < (int)(sizeof kDelay / sizeof kDelay[0]); ++i) {  // rand() % M + B: CrDelay (the mean exact)
        const int im = WvRow(kDelay[i].m), ib = WvRow(kDelay[i].b);
        CHECK(im >= 0 && ib >= 0, "%06lX / %06lX in the table", kDelay[i].m, kDelay[i].b);
        if (im < 0 || ib < 0) continue;
        for (r = 0; r < 3; ++r) {
            const int m = (r + 1) * 2;
            int M, B;
            double mean30, meanN;
            CrDelay(kDelay[i].M, kDelay[i].B, m, &M, &B);
            CHECK((int)kWv359[im].v[r] == M && (int)kWv359[ib].v[r] == B, "%06lX at %d: M %d B %d, want %d %d", kDelay[i].m,
                  m * 30, (int)kWv359[im].v[r], (int)kWv359[ib].v[r], M, B);
            CHECK(B <= 127, "an imm8 base fits");
            mean30 = kDelay[i].B + (kDelay[i].M - 1) / 2.0;
            meanN = B + (M - 1) / 2.0;
            CHECK(fabs(meanN / m - mean30) < 0.51, "%06lX at %d: mean delay %.2f s, stock %.2f s", kDelay[i].m, m * 30, meanN / m / 30, mean30 / 30);
        }
    }
    for (r = 0; r < 3; ++r) {  // the drop model: rand() % 150 + 90 from a counter started at -90 (m - 1)
        const int m = (r + 1) * 2, M = (int)kWv359[WvRow(0x54FC82)].v[r], c0 = (int)kWv359[WvRow(0x54FC8A)].v[r];
        CHECK(M == 149 * m + 1 && c0 == -90 * (m - 1), "the drop wait at %d: M %d, counter %d", m * 30, M, c0);
    }
    for (r = 0; r < 3; ++r) {  // the sky prefill and its gaps: x m
        const int m = (r + 1) * 2;
        CHECK((int)kWv359[WvRow(0x550BD1)].v[r] == 0x400 * m && (int)kWv359[WvRow(0x550BF5)].v[r] == -32 * m &&
              (int)kWv359[WvRow(0x550C09)].v[r] == -48 * m && (int)kWv359[WvRow(0x550BEE)].v[r] == 30 * m + 1 &&
              (int)kWv359[WvRow(0x550C02)].v[r] == 62 * m + 1, "the prefill at %d", m * 30);
        CHECK((int)kWv359[WvRow(0x54D5BC)].v[r] == (r == 0 ? 8 : r == 1 ? 4 : 3), "the blink step at %d", m * 30);
    }
    for (i = 0; i < NWV359; ++i) {  // no site twice, no two rows overlapping
        int j;
        for (j = i + 1; j < NWV359; ++j)
            CHECK(kWv359[j].va >= kWv359[i].va + kWv359[i].len || kWv359[i].va >= kWv359[j].va + kWv359[j].len,
                  "%06lX and %06lX overlap", kWv359[i].va, kWv359[j].va);
        for (j = 0; j < NWV359CAVE; ++j)
            CHECK(kWv359Cave[j].va >= kWv359[i].va + kWv359[i].len || kWv359[i].va >= kWv359Cave[j].va + kWv359Cave[j].len,
                  "%06lX and the cave %06lX overlap", kWv359[i].va, kWv359Cave[j].va);
    }
    CHECK(NWV359 == 48 && NWV359CAVE == 6, "48 rows and 6 caves (%d, %d)", NWV359, NWV359CAVE);
}

int main(void)
{
    TestPhaseRule();
    TestClock();
    TestHitches();  // v3.59 FR359-001
    TestJudge();
    TestPlayback();
    TestWindowTable();
    printf("fr359_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
