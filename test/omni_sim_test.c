// v3.46 OM346: native test of src\omni_sim.h (test\build_omni_sim_test.cmd): Stick Movement's pure maths: the ini
// texts, the dead zone, the Direction / Analog maps (length, angle, sign convention, continuity, monotony), the bindings
// classification, the per-frame agreement rule and the self-check's verdicts, windows and re-checks.
// v3.48 OM348: Omni-Directional Movement, Off / On (On = v3.47's Analog; Direction removed): the reader's new and old
// spellings, the map and the export with two modes.
#include <stdio.h>
#include <math.h>
#include <string.h>  // v3.48 OM348: strcmp
#include "../src/omni_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define PI 3.14159265358979

static void TestParse(void)  // v3.48 OM348: Off / On, 0 / 1; v3.46 / v3.47's Direction / Analog / 2 = On (legacy)
{
    static const struct { const char *s; int v, known, legacy; } kCases[] = {
        { "Off", OM_OFF, 1, 0 }, { "off", OM_OFF, 1, 0 }, { "OFF", OM_OFF, 1, 0 }, { "oFf", OM_OFF, 1, 0 },
        { "On", OM_ON, 1, 0 }, { "on", OM_ON, 1, 0 }, { "ON", OM_ON, 1, 0 }, { "oN", OM_ON, 1, 0 },
        { "0", OM_OFF, 1, 0 }, { "1", OM_ON, 1, 0 },  // 1 = v3.47's Direction too: On either way, no write-back
        { "Analog", OM_ON, 1, 1 }, { "analog", OM_ON, 1, 1 }, { "ANALOG", OM_ON, 1, 1 }, { "AnAlOg", OM_ON, 1, 1 },
        { "Direction", OM_ON, 1, 1 }, { "direction", OM_ON, 1, 1 }, { "DIRECTION", OM_ON, 1, 1 }, { "2", OM_ON, 1, 1 },
        { " On", OM_ON, 1, 0 }, { "On ", OM_ON, 1, 0 }, { "\tOff\t", OM_OFF, 1, 0 }, { "  1  ", OM_ON, 1, 0 },
        { " 0", OM_OFF, 1, 0 }, { " Analog ", OM_ON, 1, 1 }, { "\tDirection", OM_ON, 1, 1 }, { "2 ", OM_ON, 1, 1 },
        { "On\r\n", OM_ON, 1, 0 }, { "Analog\r", OM_ON, 1, 1 },
        { "3", OM_OFF, 0, 0 }, { "-1", OM_OFF, 0, 0 }, { "01", OM_OFF, 0, 0 }, { "10", OM_OFF, 0, 0 }, { "1.0", OM_OFF, 0, 0 },
        { "Analogue", OM_OFF, 0, 0 }, { "Dir", OM_OFF, 0, 0 }, { "O", OM_OFF, 0, 0 }, { "Onn", OM_OFF, 0, 0 },
        { "Of", OM_OFF, 0, 0 }, { "Off On", OM_OFF, 0, 0 }, { "O n", OM_OFF, 0, 0 }, { "Yes", OM_OFF, 0, 0 },
        { "True", OM_OFF, 0, 0 }, { "", OM_OFF, 0, 0 }, { "   ", OM_OFF, 0, 0 }, { "\t", OM_OFF, 0, 0 },
    };
    int i, k, l;
    for (i = 0; i < (int)(sizeof kCases / sizeof kCases[0]); ++i) {
        const int v = OmParseMode(kCases[i].s, &k, &l);
        CHECK(v == kCases[i].v && k == kCases[i].known && l == kCases[i].legacy, "OmniMove \"%s\": %d known %d legacy %d "
              "(want %d %d %d)", kCases[i].s, v, k, l, kCases[i].v, kCases[i].known, kCases[i].legacy);
    }
    CHECK(OmParseMode(NULL, NULL, NULL) == OM_OFF, "NULL: Off");
    k = l = 7;
    CHECK(OmParseMode(NULL, &k, &l) == OM_OFF && k == 0 && l == 0, "NULL: unknown, not legacy");
    CHECK(OmParseMode("Analog", NULL, NULL) == OM_ON && OmParseMode("On", &k, NULL) == OM_ON && k == 1 &&
          OmParseMode("2", NULL, &l) == OM_ON && l == 1, "the out pointers may be NULL");
    l = 1;
    CHECK(OmParseMode("Off", &k, &l) == OM_OFF && l == 0, "*legacy is cleared for a new value");
    CHECK(OM_MODES == 2 && !strcmp(kOmModeName[OM_OFF], "Off") && !strcmp(kOmModeName[OM_ON], "On"), "two modes: Off / On");
    // what the dll writes back is read again as the same value, never as an old one (no second write-back)
    for (i = 0; i < OM_MODES; ++i)
        CHECK(OmParseMode(kOmModeName[i], &k, &l) == i && k && !l, "\"%s\" written back reads as itself", kOmModeName[i]);
    CHECK(OmClampDz(0.30f) == 0.30f && OmClampDz(0.0f) == OM_DZ_MIN && OmClampDz(2.0f) == OM_DZ_MAX &&
          OmClampDz(-1.0f) == OM_DZ_MIN, "dead zone clamp");
    {
        volatile float z = 0.0f;
        CHECK(OmClampDz(z / z) == OM_DZ_DEF, "a NaN dead zone is the default");
    }
}

// v3.48 OM348 (was TestDirection): On at every angle outside the dead zone gives a vector in the stick's own direction (no
// 8 snaps) with Analog's length for the push; the signs are the game's (raw lY < 0 = up = stick.y < 0; raw lX > 0 =
// right = stick.x > 0).
static void TestAngles(void)
{
    int a, bad = 0, badAng = 0;
    float x, y;
    const float want = OmAnalogLen(0.9f, 0.30f);
    for (a = 0; a < 3600; ++a) {
        const double t = a * PI / 1800.0;
        const long rx = (long)floor(900.0 * cos(t) + 0.5), ry = (long)floor(900.0 * sin(t) + 0.5);
        if (!OmMap(rx, ry, OM_ON, 0.30f, &x, &y)) {
            ++bad;
            continue;
        }
        if (fabs(sqrt(x * x + y * y) - want) > 2e-3) ++bad;
        if (fabs(atan2(y, x) - atan2((double)ry, (double)rx)) > 0.002 &&
            fabs(fabs(atan2(y, x) - atan2((double)ry, (double)rx)) - 2 * PI) > 0.002) ++badAng;
    }
    CHECK(bad == 0, "On: Analog's length (%.3f at a 0.9 push) at every tenth of a degree (%d bad)", want, bad);
    CHECK(badAng == 0, "On: the stick's own angle, no 8-way snap (%d off)", badAng);
    CHECK(OmMap(0, -1000, OM_ON, 0.30f, &x, &y) && fabs(x) < 1e-6 && fabs(y + 1.0f) < 1e-6, "up = stick.y -1 (the game's)");
    CHECK(OmMap(1000, 0, OM_ON, 0.30f, &x, &y) && fabs(x - 1.0f) < 1e-6 && fabs(y) < 1e-6, "right = stick.x +1");
    CHECK(OmMap(-1000, 1000, OM_ON, 0.30f, &x, &y) && fabs(x + 0.70710678f) < 1e-5 && fabs(y - 0.70710678f) < 1e-5,
          "a full diagonal corner (raw length 1.41) is clamped to 1, not the stock 1.08 / 40.9 degrees");
    CHECK(!OmMap(200, 200, OM_ON, 0.30f, &x, &y), "inside the dead zone (0.28): nothing written");
    CHECK(!OmMap(299, 0, OM_ON, 0.30f, &x, &y) && OmMap(301, 0, OM_ON, 0.30f, &x, &y),
          "the default dead zone is the game's own +-300 on an axis");
    CHECK(OmMap(220, 220, OM_ON, 0.30f, &x, &y), "radial: a diagonal at 0.31 walks (the game's square band does not)");
    CHECK(!OmMap(0, 0, OM_ON, 0.05f, &x, &y), "centred: nothing, even at the smallest dead zone");
    CHECK(!OmMap(1000, 0, OM_OFF, 0.30f, &x, &y) && !OmMap(1000, 0, 2, 0.30f, &x, &y) && !OmMap(1000, 0, 3, 0.30f, &x, &y) &&
          !OmMap(1000, 0, -1, 0.30f, &x, &y), "Off / a bad mode (v3.47's Analog index 2 too): nothing written");
    CHECK(!OmMap(1200, 0, OM_ON, 0.30f, &x, &y) && !OmMap(0, -32768, OM_ON, 0.30f, &x, &y) &&
          OmMap(1100, 0, OM_ON, 0.30f, &x, &y), "raw axes past +-1100 (another range) are not used");
}

static void TestAnalog(void)
{
    const float dzs[4] = { 0.05f, 0.24f, 0.30f, 0.90f };
    int d, i, mono = 1;
    float x, y, prev;
    for (d = 0; d < 4; ++d) {
        const float dz = dzs[d];
        CHECK(fabs(OmAnalogLen(dz, dz) - OM_ANALOG_FLOOR) < 1e-6, "Analog: 0.6 at the dead zone's edge (dz %.2f)", dz);
        CHECK(fabs(OmAnalogLen(dz + 1e-4f, dz) - OM_ANALOG_FLOOR) < 1e-3, "Analog: continuous at the edge from outside (dz %.2f)", dz);
        CHECK(fabs(OmAnalogLen(1.0f, dz) - 1.0f) < 1e-6 && fabs(OmAnalogLen(1.5f, dz) - 1.0f) < 1e-6, "Analog: 1 at a full push (dz %.2f)", dz);
        prev = -1.0f;
        for (i = 0; i <= 1000; ++i) {
            const float m = dz + (1.0f - dz) * i / 1000.0f, l = OmAnalogLen(m, dz);
            if (l < prev - 1e-7f || l < OM_ANALOG_FLOOR - 1e-6f || l > 1.0f + 1e-6f) mono = 0;
            prev = l;
        }
    }
    CHECK(mono, "Analog: monotone, within 0.6 .. 1 for every dead zone");
    CHECK(OmMap(0, -650, OM_ON, 0.30f, &x, &y) && fabs(y + 0.8f) < 1e-4 && fabs(x) < 1e-6,
          "On: a push of 0.65 (halfway out of the 0.30 dead zone) walks at 0.8 (%f)", y);  // v3.48 OM348: On (was OM_ANALOG)
    CHECK(OmMap(0, -1000, OM_ON, 0.30f, &x, &y) && fabs(y + 1.0f) < 1e-6, "On: a full push = length 1");
    CHECK(OmMap(707, 707, OM_ON, 0.30f, &x, &y) && fabs(sqrt(x * x + y * y) - OmAnalogLen(0.99985f, 0.30f)) < 1e-3 &&
          fabs(x - y) < 1e-6, "On: the angle is kept, the length follows the push");
    // the ready-stance step needs forward > 0.9: Analog gives that above (0.75 of the way out of the dead zone)
    CHECK(OmAnalogLen(0.83f, 0.30f) > 0.9f && OmAnalogLen(0.82f, 0.30f) < 0.9f, "Analog: forward > 0.9 from a push of ~0.825");
    CHECK(OmPushed(301, 0, 0.30f) && !OmPushed(299, 0, 0.30f) && !OmPushed(5000, 0, 0.30f), "OmPushed: radial, in range only");
}

static void TestBindings(void)
{
    const unsigned stock[4] = { 0x0003, 0x0001, 0x0401, 0x0403 }, swap[4] = { 0x1403, 0x1401, 0x0801, 0x0803 },
                   user[4] = { 0x0003, 0x0001, 0x0401, 0x0403 },  // the user's input.ini (TurnRight / Left, MoveForward / Backward)
                   dpad[4] = { 0x2002, 0x2004, 0x2001, 0x2003 }, mix[4] = { 0x0003, 0x0001, 0x0801, 0x0803 },
                   inv[4] = { 0x0003, 0x0001, 0x0403, 0x0401 }, btn[4] = { 0x3580, 0x3480, 0x0401, 0x0403 },
                   high[4] = { 0x10003, 0x20001, 0xFFFF0401, 0x0403 }, zero[4] = { 0, 0, 0, 0 };
    CHECK(OmBindings(stock) == OMB_LEFT, "the stock table: the left stick");
    CHECK(OmBindings(user) == OMB_LEFT, "the user's input.ini: the left stick");
    CHECK(OmBindings(swap) == OMB_SWAPPED, "the swapped-sticks table: the right stick's axes");
    CHECK(OmBindings(dpad) == OMB_OTHER, "the D-pad: not a stick");
    CHECK(OmBindings(mix) == OMB_OTHER, "lX with lRy: not one stick");
    CHECK(OmBindings(inv) == OMB_OTHER, "an inverted Y: not the stock layout (off)");
    CHECK(OmBindings(btn) == OMB_OTHER, "turning on buttons: off");
    CHECK(OmBindings(zero) == OMB_OTHER, "nothing bound: off");
    CHECK(OmBindings(high) == OMB_LEFT, "only the low 16 bits count (as the game's 0x406C50)");
}

static void TestEval(void)
{
    // bits: 1 x+, 2 x-, 4 up (y-), 8 down (y+)
    CHECK(OmEval(800, 0, 1, 0) == OME_AGREE, "right: bit 0");
    CHECK(OmEval(-800, 0, 2, 0) == OME_AGREE, "left: bit 1");
    CHECK(OmEval(0, -800, 4, 0) == OME_AGREE, "up: bit 2");
    CHECK(OmEval(0, 800, 8, 0) == OME_AGREE, "down: bit 3");
    CHECK(OmEval(700, -700, 5, 0) == OME_AGREE, "up-right: bits 0 and 2");
    CHECK(OmEval(700, -700, 1, 0) == OME_DISAGREE, "up-right with only bit 0: disagree");
    CHECK(OmEval(800, 0, 2, 0) == OME_DISAGREE, "right with the left bit: disagree");
    CHECK(OmEval(800, 0, 3, 0) == OME_AGREE, "right with both x bits (the stock reversal latch): agree");
    CHECK(OmEval(800, 0, 1 | 4, 1) == OME_SKIP && OmEval(800, 0, 1, 1) == OME_SKIP && OmEval(800, 0, 2, 1) == OME_SKIP,
          "a key held: no verdict either way (v3.46 OM346 inspection 4)");
    CHECK(OmEval(800, 0, 0, 0) == OME_DISAGREE, "pushed, the game sees nothing: disagree");
    CHECK(OmEval(0, 0, 4, 0) == OME_DISAGREE, "centred, the game sees a push, no key: disagree (a blank state)");
    CHECK(OmEval(0, 0, 4, 1) == OME_SKIP, "centred, the game sees a push, a key held: no verdict");
    CHECK(OmEval(0, 0, 0, 0) == OME_AGREE, "centred, nothing: agree");
    CHECK(OmEval(300, 0, 0, 0) == OME_SKIP && OmEval(300, 0, 1, 0) == OME_SKIP, "near the band: no verdict");
    CHECK(OmEval(449, 0, 0, 0) == OME_SKIP && OmEval(450, 0, 1, 0) == OME_AGREE && OmEval(149, -149, 0, 0) == OME_AGREE &&
          OmEval(150, 0, 0, 0) == OME_SKIP, "the thresholds 450 / 150");
    CHECK(OmEval(800, 300, 1, 0) == OME_AGREE && OmEval(800, 300, 9, 0) == OME_AGREE, "an axis near the band is not judged");
    CHECK(OmEval(800, 0, 0x10 | 1, 0) == OME_AGREE && OmEval(0, 0, 0x30, 0) == OME_AGREE, "bits above 3 (camera) are ignored");
}

static int Feed(OmCheck *c, int n, int e0, int e1, int *verdicts, int *rechecks)
{
    int i, r, last = OMC_NOTHING;
    for (i = 0; i < n; ++i) {
        r = OmCheckFeed(c, e0, e1);
        if (r == OMC_VERDICT) ++*verdicts;
        if (r == OMC_RECHECK) ++*rechecks;
        if (r != OMC_NOTHING) last = r;
    }
    return last;
}

static void TestCheck(void)
{
    OmCheck c;
    int v = 0, rc = 0, i;
    // both agree: GetDeviceState after OM_CHECK_N frames
    OmCheckInit(&c);
    Feed(&c, OM_CHECK_N - 1, OME_AGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_PENDING && v == 0, "no verdict before %d frames", OM_CHECK_N);
    Feed(&c, 1, OME_AGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_DEVICE && v == 1 && c.checks == 1 && c.lastN[0] == OM_CHECK_N && c.lastOk[0] == OM_CHECK_N,
          "both agree: GetDeviceState (check 1, %d / %d)", c.lastOk[0], c.lastN[0]);
    // skips do not count
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, 500, OME_SKIP, OME_SKIP, &v, &rc);
    CHECK(c.verdict == OMS_PENDING && c.n[0] == 0 && c.n[1] == 0, "frames without a verdict never decide");
    // 90 % passes, less does not
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, 4, OME_DISAGREE, OME_AGREE, &v, &rc);
    Feed(&c, OM_CHECK_N - 4, OME_AGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_DEVICE, "36 of 40 (90 %%) passes");
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, 5, OME_DISAGREE, OME_AGREE, &v, &rc);
    Feed(&c, OM_CHECK_N - 5, OME_AGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_CALLS && c.rejected[0] && v == 1, "35 of 40 rejects GetDeviceState: the call sites");
    // GetDeviceState fails every frame (a failed read = disagree): the call sites
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, OM_CHECK_N, OME_DISAGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_CALLS && v == 1, "a GetDeviceState that always fails: the call sites");
    // GetDeviceState never has a verdict (always near the band) while the call sites pass: after 3 x N the call sites
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, 3 * OM_CHECK_N - 1, OME_SKIP, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_PENDING, "the call sites passed: wait for GetDeviceState up to 3 x N");
    Feed(&c, 1, OME_SKIP, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_CALLS && c.rejected[0], "then the call sites (GetDeviceState not used)");
    // both disagree: none
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, OM_CHECK_N, OME_DISAGREE, OME_DISAGREE, &v, &rc);
    CHECK(c.verdict == OMS_NONE && v == 1, "both disagree: none");
    CHECK(Feed(&c, 2000, OME_AGREE, OME_AGREE, &v, &rc) == OMC_NOTHING && c.verdict == OMS_NONE && v == 1 && rc == 0,
          "none stays none (no more lines)");
    // the swapped-sticks case: only GetDeviceState is fed; it passes alone
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, OM_CHECK_N, OME_AGREE, OME_SKIP, &v, &rc);
    CHECK(c.verdict == OMS_DEVICE, "GetDeviceState alone (no call-site value) passes");
    // the window after a verdict: 10 % disagreement is fine, more re-checks with the other source
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, OM_CHECK_N, OME_AGREE, OME_AGREE, &v, &rc);
    for (i = 0; i < OM_WIN; ++i) OmCheckFeed(&c, i < OM_WIN / 10 ? OME_DISAGREE : OME_AGREE, OME_AGREE);
    CHECK(c.verdict == OMS_DEVICE && c.wn == 0, "a window with 10 %% disagreement keeps GetDeviceState");
    for (i = 0; i < OM_WIN - 1; ++i) {
        const int r = OmCheckFeed(&c, i <= OM_WIN / 10 ? OME_DISAGREE : OME_AGREE, OME_AGREE);
        if (r == OMC_RECHECK) ++rc;
    }
    CHECK(rc == 0 && c.verdict == OMS_DEVICE, "no re-check before the window is full");
    CHECK(OmCheckFeed(&c, OME_AGREE, OME_AGREE) == OMC_RECHECK && c.verdict == OMS_PENDING && c.rejected[0] && c.lastSrc == 0 &&
          c.lastN[0] == OM_WIN && c.lastOk[0] == OM_WIN - OM_WIN / 10 - 1, "61 of 600 disagree: re-check (GetDeviceState rejected)");
    v = 0;
    Feed(&c, OM_CHECK_N, OME_AGREE, OME_AGREE, &v, &rc);
    CHECK(c.verdict == OMS_CALLS && v == 1 && c.checks == 2, "the re-check picks the call sites (check 2)");
    rc = 0;
    Feed(&c, OM_WIN, OME_AGREE, OME_DISAGREE, &v, &rc);
    CHECK(rc == 1 && c.verdict == OMS_NONE && c.lastSrc == 1, "the call sites fail their window too: none");
    // a skip in the window does not count; the window uses the chosen source only
    OmCheckInit(&c);
    v = rc = 0;
    Feed(&c, OM_CHECK_N, OME_DISAGREE, OME_AGREE, &v, &rc);
    Feed(&c, 5 * OM_WIN, OME_DISAGREE, OME_SKIP, &v, &rc);
    CHECK(c.verdict == OMS_CALLS && rc == 0 && c.wn == 0, "the call sites' window ignores GetDeviceState and skips");
    // at most OM_CHECKS verdicts: a source that keeps passing and failing ends at none
    OmCheckInit(&c);
    c.checks = OM_CHECKS;
    c.verdict = OMS_DEVICE;
    rc = 0;
    Feed(&c, OM_WIN, OME_DISAGREE, OME_AGREE, &v, &rc);
    CHECK(rc == 1 && c.verdict == OMS_NONE, "after %d verdicts a failed window ends at none", OM_CHECKS);
}

static void TestPick(void)  // v3.46 OM346 inspection 1 (OM346-001)
{
    CHECK(OmPickSource(OMS_DEVICE, 1, 1, 0, 0) == 0 && OmPickSource(OMS_DEVICE, 1, 0, 0, 0) == 0, "GetDeviceState chosen and read: it");
    CHECK(OmPickSource(OMS_DEVICE, -1, 1, 0, 0) == 1, "GetDeviceState chosen, its read failed, a fresh capture: the call sites");
    CHECK(OmPickSource(OMS_DEVICE, -1, 0, 0, 0) == -1 && OmPickSource(OMS_DEVICE, 0, 0, 0, 0) == -1,
          "GetDeviceState chosen, no read and no capture: the game's floats");
    CHECK(OmPickSource(OMS_CALLS, 1, 1, 0, 0) == 1 && OmPickSource(OMS_CALLS, 0, 0, 0, 0) == -1, "call sites chosen: the capture only");
    CHECK(OmPickSource(OMS_PENDING, 1, 1, 0, 0) == -1 && OmPickSource(OMS_NONE, 1, 1, 0, 0) == -1, "pending / none: nothing written");
    // inspection 5 (OM346-005): GetDeviceState chosen, a movement key held: this frame's capture wins (a stale device vector
    // cannot be caught while keys are held); without a capture, or with the call sites rejected, GetDeviceState as before
    CHECK(OmPickSource(OMS_DEVICE, 1, 1, 1, 0) == 1, "device chosen, a key held, a fresh capture: the capture");
    CHECK(OmPickSource(OMS_DEVICE, 1, 0, 1, 0) == 0, "device chosen, a key held, no capture (swapped sticks): the device");
    CHECK(OmPickSource(OMS_DEVICE, 1, 1, 1, 1) == 0, "device chosen, a key held, the call sites rejected: the device");
    CHECK(OmPickSource(OMS_DEVICE, 1, 1, 0, 0) == 0, "device chosen, no key: the device (unchanged)");
    CHECK(OmPickSource(OMS_CALLS, 1, 1, 1, 0) == 1 && OmPickSource(OMS_PENDING, 1, 1, 1, 0) == -1,
          "calls chosen / pending with a key: unchanged");
}

static void TestDzText(void)  // v3.46 OM346 inspection 2 (OM346-002)
{
    double d = -1.0;
    CHECK(OmDzNumber("0.4", &d) && d == 0.4, "0.4 is a number");
    CHECK(OmDzNumber(" 0.25 ", &d) && d == 0.25 && OmDzNumber("0.3\r\n", &d) && d == 0.3, "spaces / line ends around it");
    CHECK(OmDzNumber("1", &d) && d == 1.0 && OmDzNumber(".5", &d) && d == 0.5, "1 and .5");
    d = 7.0;
    CHECK(!OmDzNumber("0.4junk", &d) && d == 7.0, "0.4junk is not a number (the default; out left alone)");
    CHECK(!OmDzNumber("abc", &d) && !OmDzNumber(".", &d) && !OmDzNumber("", &d) && !OmDzNumber(NULL, &d),
          "abc / . / empty / NULL: not a number");
    CHECK(!OmDzNumber("0.3 0.4", &d), "two numbers: not a number");
}

static void TestKeysLostPad(void)  // v3.46 OM346 inspection 3 (OM346-003)
{
    OmCheck c;
    int v = 0, rc = 0, i;
    CHECK(OmEvalDevice(-1, 0, 0, 4, 1) == OME_SKIP, "a failed read while a movement key is held: skipped");
    CHECK(OmEvalDevice(-1, 0, 0, 4, 0) == OME_DISAGREE, "a failed read while the game sees a push and no key: disagree");
    CHECK(OmEvalDevice(0, 0, 0, 4, 0) == OME_SKIP, "not asked: skipped");
    CHECK(OmEvalDevice(1, 1000, 0, 1, 0) == OmEval(1000, 0, 1, 0) && OmEvalDevice(1, 0, 0, 4, 1) == OmEval(0, 0, 4, 1),
          "a read: OmEval's verdict");
    // keyboard walking (bit 4 = W) with a lost pad for many frames, no call-site capture (the swapped binding):
    // GetDeviceState is never rejected; once the pad reads again (a push right, bit 1) it passes
    OmCheckInit(&c);
    for (i = 0; i < 10 * OM_CHECK_N; ++i) {
        const int r = OmCheckFeed(&c, OmEvalDevice(-1, 0, 0, 4, 1), OME_SKIP);
        if (r == OMC_VERDICT) ++v;
        if (r == OMC_RECHECK) ++rc;
    }
    CHECK(c.verdict == OMS_PENDING && !c.rejected[0] && c.n[0] == 0 && v == 0,
          "keys + a lost pad: GetDeviceState not judged (n %d, rejected %d)", c.n[0], c.rejected[0]);
    for (i = 0; i < OM_CHECK_N; ++i) {
        const int r = OmCheckFeed(&c, OmEvalDevice(1, 1000, 0, 1, 0), OME_SKIP);
        if (r == OMC_VERDICT) ++v;
    }
    CHECK(c.verdict == OMS_DEVICE && v == 1, "then the pad reads again: GetDeviceState chosen");
    // the same after the verdict: keyboard frames with failed reads never fill a rejecting window
    for (i = 0; i < 2 * OM_WIN; ++i) rc += OmCheckFeed(&c, OmEvalDevice(-1, 0, 0, 4, 1), OME_SKIP) == OMC_RECHECK;
    CHECK(c.verdict == OMS_DEVICE && rc == 0 && c.wn == 0, "after the verdict: keyboard + a lost pad never re-checks");
    // inspection 4 (OM346-004): keyboard right held, the pad pushed up; GetDeviceState stale at right, the capture up:
    // the game bits hold both; neither frame may give a verdict (a stale device would otherwise pass and win)
    OmCheckInit(&c);
    v = 0;
    for (i = 0; i < 3 * OM_CHECK_N; ++i)
        v += OmCheckFeed(&c, OmEvalDevice(1, 1000, 0, 1 | 4, 1), OmEval(0, -1000, 1 | 4, 1)) == OMC_VERDICT;
    CHECK(c.verdict == OMS_PENDING && v == 0 && c.n[0] == 0 && c.n[1] == 0,
          "a key held with a stale device vector: no verdict for either source");
}

// ---------------------------------------------------------------- v3.47 CT347
// The Option 2 table check, with the stock exe's own table 0x654D88 (dumped from SH4Decomp orig/sh4.exe, md5 15a85581)
static void TestControlTable(void)
{
    static const unsigned long kStock[7 * 7] = {
        0x524940, 0x51EB10, 0, 0x654C54, 1, 0x27, 0x523130,   // Button Config
        0x524EB0, 0x413CE0, 0, 0x654C84, 2, 0x6D, 0x5254B0,   // Default Control
        0x524980, 0x413C60, 0, 0x654C18, 2, 0x23, 0x525450,   // Vibration
        0x524690, 0x413C30, 0, 0x654C0C, 3, 0x20, 0x525420,   // Head Motion
        0x525140, 0x413D10, 0, 0x654C8C, 2, 0x156, 0x5254D0,  // First Person Movement
        0x5251E0, 0x413D00, 0, 0x654C94, 2, 0x157, 0x5254E0,  // First Person Camera
        0x524F90, 0x51EB10, 0, 0x654C54, 1, 0x30, 0x401010 }; // Exit
    unsigned long t[7 * 7];
    int i;
    CHECK(OmControlTableOk(kStock, 7), "the stock Option 2 table passes");
    CHECK(!OmControlTableOk(kStock, 8) && !OmControlTableOk(kStock, 6) && !OmControlTableOk(NULL, 7), "another count / no table: refused");
    for (i = 0; i < 7 * 7; ++i) t[i] = kStock[i];
    t[14 + 0] = 0x10001234, t[14 + 1] = 0x10001240, t[14 + 3] = 0x10005000, t[14 + 4] = 4, t[14 + 6] = 0x10001250;
    CHECK(OmControlTableOk(t, 7), "Refined's Vibration copy (row 2 ours) passes");
    t[0] = 0x10009999;
    CHECK(!OmControlTableOk(t, 7), "row 0 not Button Config: refused");
    for (i = 0; i < 7 * 7; ++i) t[i] = kStock[i];
    t[6 * 7 + 5] = 0x31;
    CHECK(!OmControlTableOk(t, 7), "row 6 not Exit: refused");
    CHECK(OM_CTL_ROW == 6 && OM_CTL_ROWS + 1 == 8, "Omni-Directional Movement is row 7 of 8, above Exit");  // v3.48 OM348: the name
}

// SH4R_StickAmount's value (v3.48 OM348: Off / On; the meaning is v3.47's with On = Analog)
static void TestExportAmount(void)
{
    volatile float z = 0.0f;
    float m, prev = 0.0f;
    int bad = 0;
    CHECK(OmExportAmount(OM_OFF, 1, 1, 0.8f) == 0.0f && OmExportAmount(OM_OFF, 1, 0, -1.0f) == 0.0f, "Off: 0");
    CHECK(OmExportAmount(OM_ON, 0, 0, -1.0f) == 0.0f && OmExportAmount(OM_ON, 0, 1, 0.8f) == 0.0f, "cannot act this session: 0");
    CHECK(OmExportAmount(OM_ON, 1, 0, -1.0f) == 1.0f, "On, not written this frame (pending, dead zone, menu, keys): 1");
    CHECK(OmExportAmount(OM_ON, 1, 1, 0.75f) == 0.75f, "On written: the vector's length");
    CHECK(OmExportAmount(OM_ON, 1, 1, 0.59999f) == OM_ANALOG_FLOOR && OmExportAmount(OM_ON, 1, 1, 1.00002f) == 1.0f &&
          OmExportAmount(OM_ON, 1, 1, z / z) == OM_ANALOG_FLOOR, "rounding and NaN stay in 0.6..1");
    CHECK(OmExportAmount(2, 1, 1, 0.8f) == 0.0f && OmExportAmount(7, 1, 1, 0.8f) == 0.0f && OmExportAmount(-1, 1, 1, 0.8f) == 0.0f,
          "an unknown mode (v3.47's Analog index 2 too): 0");
    // through the real map: every push from the dead zone's edge to full gives 0.6..1, monotone
    for (m = 0.31f; m <= 1.095f; m += 0.01f) {  // (raw past 1100 is never used: OmRawOk)
        float x, y, a;
        if (!OmMap((long)(m * 1000.0f), 0, OM_ON, 0.30f, &x, &y)) { ++bad; continue; }
        a = OmExportAmount(OM_ON, 1, 1, sqrtf(x * x + y * y));
        if (a < OM_ANALOG_FLOOR || a > 1.0f || a + 1e-6f < prev) ++bad;
        prev = a;
    }
    CHECK(bad == 0 && prev == 1.0f, "On through OmMap: 0.6..1, monotone, 1 at a full push (%d bad, last %.4f)", bad, prev);
}

// The GetDeviceState failure episodes (the user's v3.46 log: three anomalies in the second the taskbar came over the game)
static int FFeed(OmFail *e, int read, int front, unsigned long t) { return OmFailFeed(e, read, front, t); }
static void TestFail(void)
{
    OmFail e;
    unsigned long t;
    int r, n, anom, bgs, bge, end;
    // a) focus lost first, then the reads fail, then focus back and the reads work: one start, one end, no anomaly
    OmFailInit(&e);
    for (t = 0; t < 5000; t += 16) FFeed(&e, 1, 1, t);
    anom = bgs = bge = end = 0;
    for (n = 0; t < 8000; t += 16, ++n) {
        r = FFeed(&e, -1, 0, t);
        anom += r == OMF_ANOMALY, bgs += r == OMF_BG_START;
    }
    for (; t < 9000; t += 16) {
        r = FFeed(&e, 1, 1, t);
        bge += r == OMF_BG_END, anom += r == OMF_ANOMALY;
    }
    CHECK(anom == 0 && bgs == 1 && bge == 1 && e.frames == (unsigned long)n && !e.open,
          "a focus loss: one start line, one end line, no anomaly (%d %d %d, frames %lu of %d)", anom, bgs, bge, e.frames, n);
    // b) the user's case: the reads fail while the window still counts as in front, the focus goes 200 ms later
    OmFailInit(&e);
    for (t = 0; t < 5000; t += 16) FFeed(&e, 1, 1, t);
    anom = bgs = 0;
    for (; t < 5200; t += 16) {
        r = FFeed(&e, -1, 1, t);
        anom += r == OMF_ANOMALY, bgs += r == OMF_BG_START;
    }
    CHECK(anom == 0 && bgs == 0, "failing in front for 0.2 s: no line yet (the verdict waits)");
    for (; t < 7000; t += 16) {
        r = FFeed(&e, -1, 0, t);
        anom += r == OMF_ANOMALY, bgs += r == OMF_BG_START;
    }
    CHECK(anom == 0 && bgs == 1 && e.bg, "then the focus goes: the episode is a focus loss, not an anomaly");
    // c) failing with the game in front all along: the anomaly after 1 s, once; then reads again: the end line
    OmFailInit(&e);
    for (t = 0; t < 5000; t += 16) FFeed(&e, 1, 1, t);
    anom = bgs = end = 0;
    for (; t < 9000; t += 16) {
        r = FFeed(&e, -1, 1, t);
        if (r == OMF_ANOMALY && !anom) CHECK(t - 5008 >= OM_FG_GRACE - 16 && t - 5008 <= OM_FG_GRACE + 16, "the anomaly at about 1 s (%lu)", t - 5008);
        anom += r == OMF_ANOMALY, bgs += r == OMF_BG_START;
    }
    r = FFeed(&e, 1, 1, t);
    CHECK(anom == 1 && bgs == 0 && r == OMF_END && !e.open, "a real failure in front: one anomaly, then the end (%d, %d)", anom, r);
    // d) a short failure in front, the focus never goes: the anomaly when the second has passed, not before
    OmFailInit(&e);
    for (t = 0; t < 5000; t += 16) FFeed(&e, 1, 1, t);
    for (n = 0; n < 3; ++n, t += 16) CHECK(FFeed(&e, -1, 1, t) == OMF_NOTHING, "short failure frame %d: nothing yet", n);
    anom = 0;
    for (; t < 5000 + OM_FG_GRACE - 32; t += 16) anom += FFeed(&e, 1, 1, t) != OMF_NOTHING;
    CHECK(anom == 0 && e.open, "reads again inside the second: still waiting");
    for (r = OMF_NOTHING; t < 7000 && r == OMF_NOTHING; t += 16) r = FFeed(&e, 1, 1, t);
    CHECK(r == OMF_ANOMALY && !e.open && e.frames == 3, "then the anomaly for its 3 frames (%d, %lu)", r, e.frames);
    // e) a short failure in front, the focus goes 0.5 s after it ended: one combined focus-loss line, no anomaly
    OmFailInit(&e);
    for (t = 0; t < 5000; t += 16) FFeed(&e, 1, 1, t);
    for (n = 0; n < 3; ++n, t += 16) FFeed(&e, -1, 1, t);
    for (; t < 5500; t += 16) FFeed(&e, 1, 1, t);
    r = FFeed(&e, 1, 0, t);
    CHECK(r == OMF_BG_END && !e.told && !e.open, "the focus went after the failure: one line, a focus loss (%d)", r);
    // f) the focus comes back and the first reads fail (the wrapper re-acquires): a focus loss too
    OmFailInit(&e);
    for (t = 0; t < 3000; t += 16) FFeed(&e, 0, 0, t);
    for (; t < 3300; t += 16) FFeed(&e, 0, 1, t);
    r = FFeed(&e, -1, 1, t);
    CHECK(r == OMF_BG_START, "a failure 0.3 s after the focus came back: a focus loss (%d)", r);
    // g) a failure long after the last focus loss: not one
    OmFailInit(&e);
    for (t = 0; t < 100; t += 16) FFeed(&e, 0, 0, t);
    for (; t < 6000; t += 16) FFeed(&e, 1, 1, t);
    r = FFeed(&e, -1, 1, t);
    CHECK(r == OMF_NOTHING && !e.bg && !OmFailRecentBg(&e, t), "a failure 6 s after a focus loss: judged on its own");
    // h) ticks without a read (a menu) never close a failing episode or judge it
    OmFailInit(&e);
    FFeed(&e, -1, 1, 1000);
    for (t = 1016, anom = 0; t < 9000; t += 16) anom += FFeed(&e, 0, 1, t) != OMF_NOTHING;
    CHECK(anom == 0 && e.open && e.failing, "no reads (a menu): the episode waits");
    CHECK(FFeed(&e, -1, 1, t) == OMF_ANOMALY, "the next failed read, in front, 8 s later: the anomaly");
    // i) GetTickCount wraps
    OmFailInit(&e);
    FFeed(&e, 1, 0, 0xFFFFFF00ul);
    r = FFeed(&e, -1, 1, 0x00000100ul);
    CHECK(r == OMF_BG_START && OmFailRecentBg(&e, 0x00000100ul), "across the GetTickCount wrap (0x200 ms after the focus loss)");
}

int main(void)
{
    TestControlTable();
    TestExportAmount();
    TestFail();
    TestPick();
    TestKeysLostPad();
    TestDzText();
    TestParse();
    TestAngles();  // v3.48 OM348 (was TestDirection)
    TestAnalog();
    TestBindings();
    TestEval();
    TestCheck();
    printf("omni_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
