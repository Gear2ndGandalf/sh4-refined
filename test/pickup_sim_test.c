// v3.32 PM332: native test of src\pickup_sim.h (test\build_pickup_test.cmd): the downed-ghost pickup window at 30 / 60 /
// 120 / 180 ticks a second, the beings and classes it covers, and the yield rule.
#include <stdio.h>
#include <math.h>
#include "../src/pickup_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// A stomp of 2.2 s on a downed ghost starting at t = 0.5 s, then nothing. The window must stay full through the stomp
// and reach 0 exactly 3 s after its end (within one tick), at every rate.
static void TestWindow(int fps)
{
    const float dt = 1.0f / (float)fps;
    float g = 0.0f, lastFull = -1.0f, zeroAt = -1.0f;
    int f;
    for (f = 1; f <= 10 * fps; ++f) {
        const float t = (float)f * dt;
        const int att = t >= 0.5f - 1e-5f && t < 2.7f - 1e-5f;
        g = PkGraceStep(g, att, dt);
        if (att) {
            CHECK(g == PK_GRACE_S, "fps %d t %.3f: full while attacking (%f)", fps, t, g);
            lastFull = t;
        } else if (lastFull > 0.0f && zeroAt < 0.0f && g <= 0.0f) {
            zeroAt = t;
        }
        CHECK(g >= 0.0f && g <= PK_GRACE_S, "fps %d: grace in range (%f)", fps, g);
    }
    CHECK(lastFull > 0.0f && zeroAt > 0.0f, "fps %d: the window opened and closed", fps);
    CHECK(fabsf((zeroAt - lastFull) - PK_GRACE_S) <= dt + 1e-4f, "fps %d: closes %.4f s after the attack (want 3)", fps,
          zeroAt - lastFull);
}

static void TestRules(void)
{
    int type, cls;
    // beings: the ghosts 0x0B-0x0F, Walter 0x10, the Patient 0x11; not the Twin Victim, dogs, the player's kinds
    for (type = 0; type < 0x20; ++type)
        CHECK(PkGhostType(type) == (type >= 0x0B && type <= 0x11), "type %X", type);
    for (cls = 0; cls < 0x14; ++cls) CHECK(PkDownedClass(cls) == (cls == 9 || cls == 0x0A || cls == 0x0B), "class %X", cls);
    CHECK(!PkAttackState(0) && !PkAttackState(1) && !PkAttackState(2), "idle / raise / ready are no attack");
    CHECK(PkAttackState(3) && PkAttackState(4) && PkAttackState(5) && PkAttackState(6), "swing, charged, both stomps");
    CHECK(!PkAttackState(0x14) && !PkAttackState(0x63) && !PkAttackState(100), "dodge, lower, death");
    // the yield: every condition needed
    CHECK(PkYield(1, 1.0f, 0x0C, 0x0A, 1), "Cynthia down, window open, the item takes the press");
    CHECK(PkYield(1, 1.0f, 0x10, 0x0A, 1), "Walter down");
    CHECK(!PkYield(0, 1.0f, 0x0C, 0x0A, 1), "feature off");
    CHECK(!PkYield(1, 0.0f, 0x0C, 0x0A, 1), "window closed (the first attack is a stomp)");
    CHECK(!PkYield(1, 1.0f, 0x08, 0x0A, 1), "not a ghost (a downed monster)");
    CHECK(!PkYield(1, 1.0f, 0x0C, 0x09, 1), "class 9 (no stock stomp there)");
    CHECK(!PkYield(1, 1.0f, 0x0C, 0x02, 1), "standing");
    CHECK(!PkYield(1, 1.0f, 0x0C, 0x0A, 0), "no item takes the press (held button, out of reach)");
    // dt guards
    CHECK(PkGraceStep(2.0f, 0, 0.0f) == 2.0f && PkGraceStep(2.0f, 0, -1.0f) == 2.0f, "no time, no change");
    CHECK(PkGraceStep(2.0f, 0, 5.0f) == 2.0f - PK_DT_MAX, "a long tick counts PK_DT_MAX");
    CHECK(PkGraceStep(2.0f, 0, (float)NAN) == 2.0f, "NaN dt ignored");
}

int main(void)
{
    static const int kFps[] = { 30, 60, 120, 180 };
    int i;
    for (i = 0; i < 4; ++i) TestWindow(kFps[i]);
    TestRules();
    printf("pickup_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
