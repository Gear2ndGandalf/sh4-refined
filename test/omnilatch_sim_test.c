// v3.59 CT359: native test of src\omnilatch_sim.h (test\build_omnilatch_test.cmd): the control-direction latch's release
// rule (Refined's 15 degrees on top of the game's 0.7), the game's triggers as 0x502040 decides them (cut, drawn-camera
// jump, the rig's look rate), the camera yaw, the ini texts of [Input] OmniLatch / [Gameplay] LockOnWalk, and Lock-On
// Walk's step gate (the move amount raised to a full push's; the step scale) against a model of the game's latch and of
// its ready-stance gates (0x53462E / 0x53CA17, stock 0.9 or Playable Eileen's 0.9 x 30 / rate).
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/omnilatch_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define PI 3.14159265358979

static float Nan(void) { volatile float z = 0.0f; return z / z; }
static void Dir(float deg, float len, float *x, float *y)  // a stick vector at deg (0 = up: y = -1, as the game's floats)
{
    *x = (float)(sin(deg * PI / 180.0) * len);
    *y = (float)(-cos(deg * PI / 180.0) * len);
}

static void TestParse(void)
{
    static const struct { const char *s; int v, known; } kLt[] = {
        { "0", LT_STOCK, 1 }, { "Off", LT_STOCK, 1 }, { "off", LT_STOCK, 1 }, { "STOCK", LT_STOCK, 1 }, { " 0 ", LT_STOCK, 1 },
        { "1", LT_REFINED, 1 }, { "On", LT_REFINED, 1 }, { "on\r\n", LT_REFINED, 1 }, { "\t1", LT_REFINED, 1 },
        { "", LT_REFINED, 1 }, { "   ", LT_REFINED, 1 },
        { "2", LT_REFINED, 0 }, { "yes", LT_REFINED, 0 }, { "Refined", LT_REFINED, 0 }, { "01", LT_REFINED, 0 },
        { "a very long text", LT_REFINED, 0 },
    };
    static const struct { const char *s; int v, known; } kLw[] = {
        { "Off", LW_OFF, 1 }, { "OFF", LW_OFF, 1 }, { "0", LW_OFF, 1 }, { " off ", LW_OFF, 1 },
        { "On", LW_ON, 1 }, { "on", LW_ON, 1 }, { "1", LW_ON, 1 }, { "", LW_ON, 1 },
        { "Stock", LW_ON, 0 }, { "2", LW_ON, 0 }, { "Of", LW_ON, 0 }, { "Offf", LW_ON, 0 },
    };
    int i, k;
    for (i = 0; i < (int)(sizeof kLt / sizeof kLt[0]); ++i) {
        const int v = LtParse(kLt[i].s, &k);
        CHECK(v == kLt[i].v && k == kLt[i].known, "OmniLatch \"%s\": %d known %d", kLt[i].s, v, k);
    }
    CHECK(LtParse(NULL, NULL) == LT_REFINED, "OmniLatch NULL: 1");
    for (i = 0; i < (int)(sizeof kLw / sizeof kLw[0]); ++i) {
        const int v = LwParse(kLw[i].s, &k);
        CHECK(v == kLw[i].v && k == kLw[i].known, "LockOnWalk \"%s\": %d known %d", kLw[i].s, v, k);
    }
    CHECK(!strcmp(kLwName[LW_OFF], "Off") && !strcmp(kLwName[LW_ON], "On"), "LockOnWalk writes Off / On");
    CHECK(LtParse(kLtModeName[LT_STOCK], NULL) == LT_STOCK && LtParse(kLtModeName[LT_REFINED], NULL) == LT_REFINED,
          "OmniLatch's own words read back");
    CHECK(LwParse(kLwName[LW_OFF], NULL) == LW_OFF && LwParse(kLwName[LW_ON], NULL) == LW_ON, "LockOnWalk's own words read back");
}

static void TestActive(void)
{
    CHECK(!LtActive(LT_STOCK, 1, 1), "OmniLatch 0: never");
    CHECK(!LtActive(LT_REFINED, 0, 0), "8-way stick, the game's camera: the game's latch");
    CHECK(LtActive(LT_REFINED, 1, 0), "Omni On");
    CHECK(LtActive(LT_REFINED, 0, 1), "the Shoulder camera's view");
    CHECK(LtActive(LT_REFINED, 1, 1), "both");
}

static void TestRelease(void)
{
    float lx, ly, cx, cy;
    int a;
    // the same direction, small turns: held
    for (a = 0; a <= 14; ++a) {
        Dir(30.0f, 0.8f, &lx, &ly);
        Dir(30.0f + a, 0.8f, &cx, &cy);
        CHECK(LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "turn %d deg: held", a);
        Dir(30.0f - a, 1.0f, &cx, &cy);
        CHECK(LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "turn -%d deg (longer): held", a);
    }
    // past 15 degrees either way, any length outside the centre: let go
    for (a = 16; a <= 180; a += 7) {
        Dir(-100.0f, 0.6f, &lx, &ly);
        Dir(-100.0f + a, 0.6f, &cx, &cy);
        CHECK(LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_ANGLE, "turn %d deg: let go", a);
        Dir(-100.0f - a, 1.0f, &cx, &cy);
        CHECK(LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_ANGLE, "turn -%d deg: let go", a);
    }
    // a push from centre, the stick let go, both centred
    Dir(0.0f, 0.6f, &cx, &cy);
    CHECK(LtRelease(0.0f, 0.0f, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_PUSHED, "centred at the latch, pushed now");
    CHECK(LtRelease(0.1f, 0.1f, cx, cy, LT_MIN_PUSH, LT_COS_DEF) == LTR_PUSHED, "a drift at the latch, pushed now");
    CHECK(LtRelease(cx, cy, 0.0f, 0.0f, LT_MIN_PUSH, LT_COS_DEF) == LTR_LETGO, "pushed at the latch, let go now");
    CHECK(LtRelease(0.0f, 0.0f, 0.0f, 0.0f, LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "centred both: held (nothing to steer)");
    CHECK(LtRelease(0.1f, 0.0f, -0.1f, 0.0f, LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "drift both ways inside the centre: held");
    // NaN: the game's own rule decides
    CHECK(LtRelease(Nan(), 0.0f, 1.0f, 0.0f, LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "NaN latched: held");
    CHECK(LtRelease(1.0f, 0.0f, 0.0f, Nan(), LT_MIN_PUSH, LT_COS_DEF) == LTR_HOLD, "NaN now: held");
    CHECK(LtRelease(1.0f, 0.0f, 0.0f, 1.0f, LT_MIN_PUSH, Nan()) == LTR_HOLD, "NaN threshold: held");
    // the 8-way stick: every step is 45 degrees: Refined's rule lets go as the game's does
    {
        static const float kWay[8][2] = { { 0, -1 }, { 0.707f, -0.816f }, { 1, 0 }, { 0.707f, 0.816f }, { 0, 1 },
                                          { -0.707f, 0.816f }, { -1, 0 }, { -0.707f, -0.816f } };
        int i;
        for (i = 0; i < 8; ++i) {
            const int j = (i + 1) & 7;
            CHECK(LtRelease(kWay[i][0], kWay[i][1], kWay[j][0], kWay[j][1], LT_MIN_PUSH, LT_COS_DEF) == LTR_ANGLE, "8-way %d -> %d", i, j);
            CHECK(LtStickDist(kWay[i][0], kWay[i][1], kWay[j][0], kWay[j][1]) > 0.7f, "8-way %d -> %d: the game's 0.7 lets go too", i, j);
        }
    }
    CHECK(fabsf(LtStickDeg(1.0f, 0.0f, 0.0f, 1.0f) - 90.0f) < 0.01f, "the angle of two vectors");
    CHECK(LtStickDeg(0.0f, 0.0f, 0.0f, 1.0f) == 0.0f && LtStickDeg(Nan(), 0.0f, 1.0f, 0.0f) == 0.0f, "no angle when centred / NaN");
}

// A model of the game's latch (0x502040, its timer and its 0.7 release) with Refined's release rule on top, frame by frame,
// the stick turning at degPerS from its latched angle. Returns the seconds the latch held (the timer starts at 1.6).
static float ModelLatch(int refined, float degPerS, float len, int rate)
{
    const float dt = 1.0f / (float)rate;
    float timer = 1.6f, t = 0.0f, lx, ly, cx, cy;
    Dir(0.0f, len, &lx, &ly);
    while (timer > 0.0f && t < 5.0f) {
        t += dt;
        Dir(degPerS * t, len, &cx, &cy);
        if (refined && LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF) != LTR_HOLD) {  // what LtWrap does before the call:
            lx = cx + 1.0f;                                                               // the latched stick 1.0 away
            ly = cy;
        }
        timer = LtTimerLeft(timer, dt);
        if (LtStickDist(lx, ly, cx, cy) > 0.7f || timer <= 0.0f) break;  // the game's own release
    }
    return t;
}
static void TestModel(void)
{
    // an analog stick steering 30 degrees a second: the game holds the whole 1.6 s (a 0.7 distance needs 41 degrees, at
    // length 0.6 even 71); Refined lets go after 15 degrees (half a second)
    const float g = ModelLatch(0, 30.0f, 0.8f, 60), r = ModelLatch(1, 30.0f, 0.8f, 60);
    CHECK(g >= 1.59f, "the game's latch holds a slow analog turn for its full time (%.2f s)", g);
    CHECK(r > 0.45f && r < 0.6f, "Refined's lets go at 15 degrees (%.2f s)", r);
    CHECK(ModelLatch(0, 30.0f, 0.6f, 60) >= 1.59f, "a light push: the game's latch holds too");
    CHECK(ModelLatch(1, 0.0f, 0.8f, 60) >= 1.59f, "a held direction: Refined's latch holds its full time (the cut keeps its frame)");
    CHECK(ModelLatch(1, 5.0f, 1.0f, 180) >= 1.59f, "a slow drift (5 deg/s): held at 180 Hz too");
    CHECK(ModelLatch(0, 2000.0f, 1.0f, 30) < 0.1f && ModelLatch(1, 2000.0f, 1.0f, 30) < 0.1f, "a flick: both let go at once");
    // the forced release: the latched stick 1.0 away is past the game's 0.7 whatever the stick
    {
        float cx, cy;
        int a;
        for (a = 0; a < 360; a += 30) {
            Dir((float)a, 0.6f + a / 1000.0f, &cx, &cy);
            CHECK(LtStickDist(cx + 1.0f, cy, cx, cy) > 0.7f, "the forced release at %d deg passes the game's 0.7", a);
        }
        CHECK(LtStickDist(1.0f, 0.0f, 0.0f, 0.0f) > 0.7f, "the forced release with the stick centred (a stick lock) passes too");
    }
}

// The game's blend in its last second and the control length it hands to PlayerControlDirSet: |v| = |t ctrl + (1 - t) cam|,
// then v / sqrt(|v|): the length is sqrt(|v|) (0x5024D6 .. 0x502536). Below 0.95 the builder limits the turn and the move
// amount shrinks with it (forward = length x push): what Lock-On Walk's gate sees.
static float ModelControlLen(float t, float angDeg)
{
    const float a = (float)(angDeg * PI / 180.0);
    const float x = t * 0.0f + (1.0f - t) * sinf(a), z = t * 1.0f + (1.0f - t) * cosf(a);
    return sqrtf(sqrtf(x * x + z * z));
}
static void TestRateAndTrigger(void)
{
    CHECK(LtRateHold(0.0f) == 0.0f && LtRateHold(LT_HALF_PI) == 0.0f, "90 deg/s or slower: no hold");
    CHECK(fabsf(LtRateHold(LT_PI) - LT_HALF_PI * LT_RATE_K) < 1e-5f, "180 deg/s: 1.125 s");
    CHECK(LtRateHold(100.0f) == LT_RATE_MAX, "a fast swing: 1.8 s at most");
    CHECK(LtRateHold(Nan()) == 0.0f, "NaN: none");
    CHECK(LtTimerLeft(1.0f, 0.25f) == 0.75f && LtTimerLeft(0.1f, 0.25f) == 0.0f && LtTimerLeft(Nan(), 0.1f) == 0.0f, "the decrement");
    CHECK(LtTrigger(1, 0.0f, 0.0f, 0.0f, 1.8f, 0.0f) == LTT_CUT, "a cut first");
    CHECK(LtTrigger(0, 0.70f, 1.0f, 1.0f, 0.0f, 0.0f) == LTT_JUMP, "row 0 below 0.71: a jump");
    CHECK(LtTrigger(0, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f) == LTT_JUMP, "row 2 below 0.71: a jump");
    CHECK(LtTrigger(0, 1.0f, Nan(), 1.0f, 0.0f, 0.0f) == LTT_NONE, "a NaN dot is no jump (the game's jnp)");
    CHECK(LtTrigger(0, 1.0f, 1.0f, 1.0f, 0.5f, 0.4f) == LTT_RATE, "the rate raises the timer");
    CHECK(LtTrigger(0, 1.0f, 1.0f, 1.0f, 0.4f, 0.5f) == LTT_NONE, "the rate below the timer: nothing new");
    CHECK(LtTrigger(0, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f) == LTT_NONE, "a still camera: nothing");
    // the blend's control length (why a latch also stops the ready-stance steps)
    CHECK(fabsf(ModelControlLen(1.0f, 90.0f) - 1.0f) < 1e-5f && fabsf(ModelControlLen(0.0f, 90.0f) - 1.0f) < 1e-5f, "the ends: 1");
    CHECK(ModelControlLen(0.5f, 90.0f) < 0.95f, "a 90-degree blend half way: %.3f (the turn is limited)", ModelControlLen(0.5f, 90.0f));
    CHECK(ModelControlLen(0.5f, 180.0f) < 0.01f, "a 180-degree blend half way: no movement at all (%.3f)", ModelControlLen(0.5f, 180.0f));
}

static void TestCamYaw(void)
{
    float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    CHECK(fabsf(LtCamYaw(m)) < 1e-6f, "forward +z: yaw 0");
    m[8] = 1.0f, m[10] = 0.0f;
    CHECK(fabsf(LtCamYaw(m) - LT_HALF_PI) < 1e-5f, "forward +x: yaw pi/2");
    {   // pitched 30 degrees down, facing -x (yaw = atan2(x, z) = -pi/2): the flattened forward still points -x
        const float c = (float)cos(PI / 6), s = (float)sin(PI / 6);
        float p[16] = { 0, 0, 1, 0, -s, c, 0, 0, -c, -s, 0, 0, 0, 0, 0, 1 };
        CHECK(fabsf(LtCamYaw(p) + LT_HALF_PI) < 1e-4f, "pitched: yaw -pi/2 (%.4f)", LtCamYaw(p));
    }
    {
        float z[16] = { 0 };
        CHECK(LtCamYaw(z) == 0.0f, "a zero matrix: 0");
    }
    CHECK(fabsf(LtWrapAngle(3.0f * LT_PI) - LT_PI) < 1e-4f && fabsf(LtWrapAngle(-2.5f * LT_PI) + 0.5f * LT_PI) < 1e-4f, "the wrap");
    CHECK(LtWrapAngle(Nan()) == 0.0f && LtWrapAngle(1e9f) == 0.0f, "the wrap: NaN / huge = 0");
}

// Lock-On Walk against the game's gates: a step happens when the amount passes the gate (0.9, or 0.9 x 30 / rate)
static int StepPasses(float fwd, int rate, int stockGate) { return fwd > (stockGate ? 0.9f : 0.9f * 30.0f / (float)rate); }
static void TestLockOnWalk(void)
{
    static const int kRate[4] = { 30, 60, 120, 180 };
    int i, g;
    for (i = 0; i < 4; ++i)
        for (g = 0; g < 2; ++g) {
            const int rate = kRate[i];
            const float full = LwFull(rate, g), k = 30.0f / (float)rate;
            float push;
            CHECK(StepPasses(full, rate, g), "%d Hz, %s gate: a full push's amount passes", rate, g ? "stock" : "PE");
            for (push = 0.4f; push <= 1.0f; push += 0.05f) {
                const float fwd = push * k;  // the builder: push x control length 1 x dt x 30
                const float nf = LwForward(LW_ON, 2, 1, fwd, full);
                CHECK(StepPasses(nf, rate, g), "%d Hz %s: push %.2f steps", rate, g ? "stock" : "PE", push);
                CHECK(nf >= fwd, "never lowered");
                CHECK(LwForward(LW_OFF, 2, 1, fwd, full) == fwd, "Off: unchanged");
                CHECK(LwForward(LW_ON, 0, 1, fwd, full) == fwd, "free walking (mode 0): unchanged");
                CHECK(LwForward(LW_ON, 3, 1, fwd, full) == fwd, "a swing (mode 3): unchanged");
                CHECK(LwForward(LW_ON, 2, 0, fwd, full) == fwd, "Room 302's first person: unchanged");
            }
            CHECK(LwForward(LW_ON, 2, 1, 0.0f, full) == 0.0f, "no push: no step");
            CHECK(LwForward(LW_ON, 2, 1, -0.3f, full) == -0.3f, "a negative amount: unchanged");
            CHECK(LwForward(LW_ON, 2, 1, Nan(), full) != LwForward(LW_ON, 2, 1, Nan(), full), "NaN: unchanged (still NaN)");
            CHECK(LwForward(LW_ON, 2, 1, full * 1.08f, full) == full * 1.08f, "an 8-way diagonal above full: unchanged");
            // a latch blend's shorter control vector (length 0.84 at 90 degrees half way) at a full push
            CHECK(StepPasses(LwForward(LW_ON, 2, 1, 0.84f * k * 0.9f, full), rate, g), "%d Hz: a blend's amount steps", rate);
        }
    // without Playable Eileen at 60+ the game's own gate never passes even at a full push: the raised 1.0 does
    CHECK(!StepPasses(1.0f * 30.0f / 60.0f, 60, 1), "the game at 60 Hz: a full push never steps (the old bug)");
    CHECK(StepPasses(LwForward(LW_ON, 2, 1, 0.5f, LwFull(60, 1)), 60, 1), "Lock-On Walk at 60 Hz with the game's gate: steps");
    CHECK(LwFull(0, 0) == 1.0f && LwFull(5000, 0) == 1.0f, "a bad rate reads as 30");
    // the step's scale: the game's own 9.1 per call is for 30 calls a second
    CHECK(LwStepScale(30, 1) == 1.0f && LwStepScale(60, 1) == 0.5f && fabsf(LwStepScale(180, 1) - 1.0f / 6.0f) < 1e-6f, "stock operand");
    CHECK(LwStepScale(60, 0) == 1.0f && LwStepScale(180, 0) == 1.0f, "Playable Eileen's operand holds the rate: 1");
    CHECK(LwStepScale(0, 1) == 1.0f && LwStepScale(20, 1) == 1.0f, "a bad rate: 1");
    {   // the ground speed per second stays 9.1 x 30 at every rate with the stock operand
        for (i = 0; i < 4; ++i) CHECK(fabsf(9.1f * LwStepScale(kRate[i], 1) * kRate[i] - 273.0f) < 0.01f, "%d Hz: 273 u/s", kRate[i]);
    }
}

int main(void)
{
    TestParse();
    TestActive();
    TestRelease();
    TestModel();
    TestRateAndTrigger();
    TestCamYaw();
    TestLockOnWalk();
    printf("omnilatch_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
