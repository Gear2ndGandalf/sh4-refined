// v3.46 OM346: native test of src\omni_sim.h (test\build_omni_sim_test.cmd): Stick Movement's pure maths: the ini
// texts, the dead zone, the Direction / Analog maps (length, angle, sign convention, continuity, monotony), the bindings
// classification, the per-frame agreement rule and the self-check's verdicts, windows and re-checks.
// v3.48 OM348: Omni-Directional Movement, Off / On (On = v3.47's Analog; Direction removed): the reader's new and old
// spellings, the map and the export with two modes.
// v3.49 LK349: the analog look (OmniLook's text, where it acts, the look bindings, the look map, the latch / 8-way-band
// diagnostics, the look self-check) against a model of the game's AxisEvent / BuildStickAxes / 0x553D20 / Room302Look,
// and Room 302's X-turns rule for the left stick's x.
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

// ---------------------------------------------------------------- v3.49 LK349: the analog look
// A model of the game's look path (SH4Decomp src/input/dinput_4062b0.cpp AxisEvent 0x406F50 / BuildStickAxes 0x4067C0,
// src/system/sys_553d20.c STICK, src/player/pl_52d480.c Room302Look 0x52DBD0) for one axis pair: the stock AxisEvent
// (no release on a direct reversal: the latch) or Playable Eileen's PadAxis (the old side released first).
typedef struct { unsigned bits; long last[2]; } LookModel;
static void ModelAxis(LookModel *m, long v, int a, int pe)  // a 0 = x (bits 0 / 1: + / -), 1 = y (bits 2 / 3: - (up) / +)
{
    const unsigned plus = a ? 8u : 1u, minus = a ? 4u : 2u;  // the bit of a code 3 (> 300) and of a code 1 (< -300)
    long nv = v;
    int dir = 0;
    if (v > -300 && v < 300) nv = 0;
    else if (v < -300) dir = 1;
    else if (v > 300) dir = 3;
    if (m->last[a] == nv) return;
    if (dir) {
        if (pe && m->last[a] != 0 && (m->last[a] < 0) != (nv < 0)) m->bits &= ~(plus | minus);
        m->bits |= dir == 3 ? plus : minus;
        m->last[a] = nv;
    } else if (m->last[a] != 0) {
        m->last[a] = 0;
        m->bits &= ~(plus | minus);
    }
}
static float ModelFloat(unsigned bits, int a)  // stick[1].x / y from the bits (BuildStickAxes + STICK, one axis)
{
    // x: bit 0 +0x7FFF, bit 1 -0x8000; y: the short from bit 2 (+0x7FFF) / bit 3 (-0x8000), negated by STICK
    int sh = 0;
    if (!a) sh += (bits & 1) ? 0x7FFF : 0, sh += (bits & 2) ? -0x8000 : 0;
    else sh += (bits & 4) ? 0x7FFF : 0, sh += (bits & 8) ? -0x8000 : 0;
    if (sh < 10 && sh > -10) sh = 0;
    if (!sh) return 0.0f;
    return a ? ((float)sh + 0.5f) * -3.0518044e-05f : ((float)sh + 0.5f) * 3.0518044e-05f;
}
static void TestLookParse(void)
{
    static const struct { const char *s; int v, known; } kCases[] = {
        { "Off", OML_OFF, 1 }, { "off", OML_OFF, 1 }, { "Room302", OML_ROOM302, 1 }, { "room302", OML_ROOM302, 1 },
        { "ROOM 302", OML_ROOM302, 1 }, { "All", OML_ALL, 1 }, { "aLL", OML_ALL, 1 }, { "0", OML_OFF, 1 }, { "1", OML_ROOM302, 1 },
        { "2", OML_ALL, 1 }, { " All\r\n", OML_ALL, 1 }, { "\tOff ", OML_OFF, 1 }, { "", OML_ROOM302, 1 }, { "  ", OML_ROOM302, 1 },
        { "3", OML_ROOM302, 0 }, { "On", OML_ROOM302, 0 }, { "Room", OML_ROOM302, 0 }, { "Room3020", OML_ROOM302, 0 },
        { "Alll", OML_ROOM302, 0 }, { "-1", OML_ROOM302, 0 }, { "Room  302", OML_ROOM302, 0 },
    };
    int i, k;
    for (i = 0; i < (int)(sizeof kCases / sizeof kCases[0]); ++i) {
        const int v = OmLookParse(kCases[i].s, &k);
        CHECK(v == kCases[i].v && k == kCases[i].known, "OmniLook \"%s\": %d known %d (want %d %d)", kCases[i].s, v, k,
              kCases[i].v, kCases[i].known);
    }
    CHECK(OmLookParse(NULL, NULL) == OML_ROOM302, "NULL: the default Room302");
    for (i = 0; i < OML_MODES; ++i) CHECK(OmLookParse(kOmLookName[i], &k) == i && k, "\"%s\" written reads as itself", kOmLookName[i]);
    CHECK(OmLookWhere(OML_ROOM302, 1) && !OmLookWhere(OML_ROOM302, 10) && !OmLookWhere(OML_ROOM302, 2) &&
          !OmLookWhere(OML_ROOM302, 0), "Room302: stage 1 only (stage 10 is Room 302's third-person visit)");
    CHECK(OmLookWhere(OML_ALL, 1) && OmLookWhere(OML_ALL, 2) && OmLookWhere(OML_ALL, 10) && !OmLookWhere(OML_OFF, 1) &&
          !OmLookWhere(OML_OFF, 2) && !OmLookWhere(7, 1), "All: every stage; Off / unknown: none");
}
static void TestLookBindings(void)
{
    const unsigned stock[4] = { 0x1403, 0x1401, 0x0801, 0x0803 }, swap[4] = { 0x0003, 0x0001, 0x0401, 0x0403 };
    const unsigned hi[4] = { 0xAB1403, 0x771401, 0x10801, 0xFF0803 }, keys[4] = { 0, 0, 0, 0 };
    const unsigned inv[4] = { 0x1403, 0x1401, 0x0803, 0x0801 }, mix[4] = { 0x1403, 0x1401, 0x0401, 0x0403 };
    const unsigned btn[4] = { 0x3180, 0x3280, 0x3380, 0x3480 };
    CHECK(OmLookBindings(stock) == OMLB_RIGHT, "the stock table: the right stick (lRx / lRy)");
    CHECK(OmLookBindings(hi) == OMLB_RIGHT, "only the low 16 bits count (as 0x406C50)");
    CHECK(OmLookBindings(swap) == OMLB_LEFT, "swapped sticks: the left stick's axes");
    CHECK(OmLookBindings(keys) == OMLB_OTHER && OmLookBindings(inv) == OMLB_OTHER && OmLookBindings(mix) == OMLB_OTHER &&
          OmLookBindings(btn) == OMLB_OTHER, "keys only, an inverted Y, a mixed layout, buttons: not a stick");
}
static void TestLookMap(void)
{
    float x = 9, y = 9, px = -1;
    int a, bad = 0, badAng = 0, mono = 1;
    long m;
    CHECK(!OmLookMap(0, 0, OM_LOOK_DZ, &x, &y) && x == 9 && y == 9, "centred: nothing written");
    CHECK(!OmLookMap(200, 0, OM_LOOK_DZ, &x, &y) && !OmLookMap(140, 140, OM_LOOK_DZ, &x, &y), "inside 0.20 (radial): nothing");
    CHECK(OmLookMap(150, 150, OM_LOOK_DZ, &x, &y) && x > 0 && y > 0 && sqrtf(x * x + y * y) < 0.02f,
          "a diagonal just past 0.20: a tiny push (continuous from 0)");
    CHECK(OmLookMap(1000, 0, OM_LOOK_DZ, &x, &y) && fabs(x - 1.0f) < 1e-6 && fabs(y) < 1e-6, "full right: x +1");
    CHECK(OmLookMap(0, -1000, OM_LOOK_DZ, &x, &y) && fabs(y + 1.0f) < 1e-6 && fabs(x) < 1e-6, "full up: y -1 (the game's)");
    CHECK(OmLookMap(0, 1000, OM_LOOK_DZ, &x, &y) && fabs(y - 1.0f) < 1e-6, "full down: y +1");
    CHECK(OmLookMap(1000, 1000, OM_LOOK_DZ, &x, &y) && fabs(sqrtf(x * x + y * y) - 1.0f) < 1e-5 && fabs(x - y) < 1e-6,
          "a full diagonal: length 1 at 45 degrees (the game's 0.707 / 0.816 is longer and skewed)");
    CHECK(OmLookMap(600, 0, OM_LOOK_DZ, &x, &y) && fabs(x - 0.5f) < 1e-5, "a 0.6 push: 0.5 (rescaled from the dead zone)");
    CHECK(!OmLookMap(1200, 0, OM_LOOK_DZ, &x, &y) && !OmLookMap(0, -40000, OM_LOOK_DZ, &x, &y), "out of the game's range: nothing");
    CHECK(OmLookMap(400, 0, -1.0f, &x, &y) && fabs(x - 0.25f) < 1e-5, "a bad dead zone is the default 0.20");
    for (a = 0; a < 3600; ++a) {  // every tenth of a degree at a 0.8 push: length 0.75, the stick's own angle
        const double t = a * PI / 1800.0;
        const long rx = (long)floor(800.0 * cos(t) + 0.5), ry = (long)floor(800.0 * sin(t) + 0.5);
        const double want = (sqrt((double)rx * rx + (double)ry * ry) / 1000.0 - 0.2) / 0.8;
        if (!OmLookMap(rx, ry, OM_LOOK_DZ, &x, &y) || fabs(sqrt(x * x + y * y) - want) > 1e-4) ++bad;
        else if (fabs(atan2(y, x) - atan2((double)ry, (double)rx)) > 1e-4 && fabs(fabs(atan2(y, x) - atan2((double)ry, (double)rx)) - 2 * PI) > 1e-4) ++badAng;
    }
    CHECK(bad == 0 && badAng == 0, "every angle: the rescaled length and the stick's own angle (%d bad, %d off)", bad, badAng);
    for (m = 201; m <= 1100; ++m) {
        if (!OmLookMap(m, 0, OM_LOOK_DZ, &x, &y) || x < px || x > 1.0f) mono = 0;
        px = x;
    }
    CHECK(mono, "monotone from the dead zone to a full push, never past 1");
}
static void TestLookLatch(void)
{
    LookModel st, pe;
    float x, y;
    int i, latched = 0, peStuck = 0, analogTurns = 0, frames = 0;
    // a direct flick: full right, then in one poll full left (no poll in the middle), held left for 30 polls
    memset(&st, 0, sizeof st);
    memset(&pe, 0, sizeof pe);
    ModelAxis(&st, 800, 0, 0);
    ModelAxis(&pe, 800, 0, 1);
    CHECK(ModelFloat(st.bits, 0) > 0.99f && ModelFloat(pe.bits, 0) > 0.99f && !OmLatch(st.bits), "full right: +1 in both");
    for (i = 0; i < 30; ++i) {
        ModelAxis(&st, -800, 0, 0);
        ModelAxis(&pe, -800, 0, 1);
        ++frames;
        latched += ModelFloat(st.bits, 0) == 0.0f && OmLatch(st.bits) == 1;
        peStuck += ModelFloat(pe.bits, 0) > -0.99f;
        analogTurns += OmLookMap(-800, 0, OM_LOOK_DZ, &x, &y) && x < -0.7f;
    }
    CHECK(latched == frames, "the stock latch: held left after a direct flick reads centred every frame (%d of %d), and "
          "OmLatch sees it", latched, frames);
    CHECK(peStuck == 0, "Playable Eileen's PadAxis: no latch (%d)", peStuck);
    CHECK(analogTurns == frames, "the analog look turns left through the latch (from the raw axes, %d of %d)", analogTurns, frames);
    ModelAxis(&st, 0, 0, 0);
    CHECK(st.bits == 0 && !OmLatch(st.bits), "let go: the latch clears (both released on the neutral path)");
    // y: up then down directly
    memset(&st, 0, sizeof st);
    ModelAxis(&st, -900, 1, 0);
    CHECK(ModelFloat(st.bits, 1) < -0.99f, "full up: y -1");
    ModelAxis(&st, 900, 1, 0);
    CHECK(ModelFloat(st.bits, 1) == 0.0f && OmLatch(st.bits) == 2, "the y latch: centred, OmLatch 2");
    CHECK(OmLatch(0x3) == 1 && OmLatch(0xC) == 2 && OmLatch(0xF) == 3 && OmLatch(0x5) == 0 && OmLatch(0xA) == 0 && !OmLatch(0),
          "OmLatch: both bits of an axis");
    // the game's 8-way band: a push under 0.30 does nothing in the game; the analog look turns
    memset(&st, 0, sizeof st);
    ModelAxis(&st, 280, 0, 0);
    CHECK(st.bits == 0 && OmBandMiss(280, 0, OM_LOOK_DZ) && OmLookMap(280, 0, OM_LOOK_DZ, &x, &y) && x > 0.09f,
          "a 0.28 push: the game turns nothing (no bit), the band miss is counted, the analog look turns");
    CHECK(!OmBandMiss(310, 0, OM_LOOK_DZ) && !OmBandMiss(150, 0, OM_LOOK_DZ) && OmBandMiss(150, 150, OM_LOOK_DZ) &&
          !OmBandMiss(0, -301, OM_LOOK_DZ) && !OmBandMiss(5000, 0, OM_LOOK_DZ), "OmBandMiss: outside the dead zone, inside the game's band");
}
static void TestLookCheck(void)
{
    OmCheck c;
    int i, r = OMC_NOTHING;
    OmLookCheckInit(&c);
    CHECK(c.verdict == OMS_PENDING && c.rejected[1] && !c.rejected[0], "the look check: GetDeviceState only (no call sites)");
    for (i = 0; i < OM_CHECK_N && r == OMC_NOTHING; ++i) r = OmCheckFeed(&c, OmEval(800, 0, 1, 0), OME_SKIP);
    CHECK(r == OMC_VERDICT && c.verdict == OMS_DEVICE && i == OM_CHECK_N && c.lastOk[0] == OM_CHECK_N,
          "40 agreeing frames: GetDeviceState (%d %d)", r, c.verdict);
    CHECK(OmEval(-800, 0, 3, 0) == OME_AGREE && OmEval(0, 800, 0xC, 0) == OME_AGREE,
          "a latched axis (both bits) still agrees with a raw push past the band");
    OmLookCheckInit(&c);
    for (i = 0, r = OMC_NOTHING; i < OM_CHECK_N && r == OMC_NOTHING; ++i) r = OmCheckFeed(&c, OmEval(0, 0, 1, 0), OME_SKIP);
    CHECK(r == OMC_VERDICT && c.verdict == OMS_NONE, "a stale / blank read (centred while the game sees a push): none");
    OmLookCheckInit(&c);
    for (i = 0; i < 200; ++i) r = OmCheckFeed(&c, OmEval(800, 0, 1, 1), OME_SKIP);
    CHECK(c.verdict == OMS_PENDING && c.n[0] == 0, "a look key held: no verdict at all");
    OmLookCheckInit(&c);
    for (i = 0; i < OM_CHECK_N; ++i) OmCheckFeed(&c, OME_AGREE, OME_SKIP);
    for (i = 0, r = OMC_NOTHING; i < OM_WIN && r == OMC_NOTHING; ++i) r = OmCheckFeed(&c, i % 5 ? OME_AGREE : OME_DISAGREE, OME_SKIP);
    CHECK(r == OMC_RECHECK && c.verdict == OMS_NONE, "a window with 20 %% disagreement: rejected, no other source: none");
}
// Room 302's turn rule: the right stick's yaw only while the left stick's x is 0 (X turns; 0x52DC5E), modelled
static int ModelYawLive(float stickX, int xTurns) { return !(xTurns && !(0.01f >= stickX && -0.01f <= stickX)); }
static void TestRoomTurnX(void)
{
    float fx, fy;
    int blocked = 0, live = 0;
    long ang;
    // a forward walk a few degrees off the axis: v3.48 wrote a small x, which silenced the right stick's turn
    for (ang = -15; ang <= 15; ++ang) {
        const double t = (ang - 90) * PI / 180.0;
        const long rx = (long)floor(900.0 * cos(t) + 0.5), ry = (long)floor(900.0 * sin(t) + 0.5);
        if (!OmMap(rx, ry, OM_ON, 0.30f, &fx, &fy)) continue;
        blocked += !ModelYawLive(fx, 1);
        OmRoomTurnX(1, 1, 0.0f, &fx);  // the game's own x is 0 (|rx| <= 300: no turn bit)
        live += ModelYawLive(fx, 1);
    }
    CHECK(blocked >= 28 && live == 31, "a forward walk within 15 degrees: v3.48 silenced the right stick's turn in %d of 31 "
          "pushes; with the rule it turns in %d of 31", blocked, live);
    fx = 0.05f;
    CHECK(OmRoomTurnX(1, 1, 0.0f, &fx) == 1 && fx == 0.0f, "Room 302, X turns, the game's x 0: x 0");
    fx = 0.6f;
    CHECK(OmRoomTurnX(1, 1, 1.0f, &fx) == 0 && fx == 0.6f, "the game's own x turns (past +-300 or a key): the analog x stays");
    fx = 0.05f;
    CHECK(OmRoomTurnX(1, 0, 0.0f, &fx) == 0 && fx == 0.05f, "X strafes (byte 0x1091AE7 != 0): the x stays (no yaw rule there)");
    fx = 0.05f;
    CHECK(OmRoomTurnX(0, 1, 0.0f, &fx) == 0 && fx == 0.05f, "not Room 302's first-person view: the x stays");
    fx = 0.0f;
    CHECK(OmRoomTurnX(1, 1, 0.0f, &fx) == 0, "x already 0: nothing to do");
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
    TestLookParse();     // v3.49 LK349
    TestLookBindings();  // v3.49 LK349
    TestLookMap();       // v3.49 LK349
    TestLookLatch();     // v3.49 LK349
    TestLookCheck();     // v3.49 LK349
    TestRoomTurnX();     // v3.49 LK349
    printf("omni_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
