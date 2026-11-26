// v3.52 SND352: native test of src\sound_sim.h (test\build_sound_test.cmd): the smooth pan law and Stereo Width, the
// Headphones floor, the stream pans, Night Mode's range compression, the Mono fold, the mode <-> the game's Sound byte, and
// the ini parsers. The report's table (staging/research_sound/REPORT.md 1.3) is checked against the stock law too.
#include <stdio.h>
#include <stdlib.h>
#include "../src/sound_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// the other ear's level (dB) of a DirectSound pan
static double OtherEar(int pan) { return -SndAbs(pan) / 100.0; }
// the game's 2D pan of an angle off the camera's axis (0x56C5E0: (sin + 1) / 2 * 127, truncated as the game's ftol)
static int PanOfAngle(double deg) { return (int)((sin(deg * 3.14159265358979323846 / 180.0) + 1.0) / 2.0 * 127.0); }

static void TestStockTable(void)
{
    // REPORT 1.3: pan 63 / 69 / 74 / 79 / 85 / 95 / 108 / 127 at 0 / 5 / 10 / 15 / 20 / 30 / 45 / 90 degrees
    static const double deg[8] = { 0, 5, 10, 15, 20, 30, 45, 90 };
    static const int pan[8] = { 63, 69, 74, 79, 85, 95, 108, 127 };
    static const int stockDb[8] = { 0, -9, -17, -25, -34, -50, -70, -100 };
    static const int smoothDb[8] = { 0, -1, -2, -3, -5, -8, -12, -100 };
    int i;
    for (i = 0; i < 8; ++i) {
        const int p = PanOfAngle(deg[i]);
        const double s = OtherEar(SndPanStock2D(p)), e = OtherEar(SndPanSmooth(SndPos2D(p), 100, 0));
        CHECK(p == pan[i], "angle %g: pan %d, the report says %d", deg[i], p, pan[i]);
        CHECK(fabs(s - stockDb[i]) <= 1.0, "angle %g: stock other ear %.1f dB, the report %d", deg[i], s, stockDb[i]);
        CHECK(fabs(e - smoothDb[i]) <= 1.0, "angle %g: smooth other ear %.1f dB, the report %d", deg[i], e, smoothDb[i]);
    }
}

static void TestSmooth(void)
{
    int t[128], s[31], p, w, prev;
    SndBuildPans(t, s, 100, 0);
    CHECK(t[63] == 0, "centre %d", t[63]);
    CHECK(t[127] == SND_PAN_MAX && t[0] == -SND_PAN_MAX, "hard sides %d / %d", t[127], t[0]);
    for (p = 1, prev = t[0]; p < 128; prev = t[p++]) CHECK(t[p] >= prev, "2D pan not rising at %d (%d < %d)", p, t[p], prev);
    for (p = 0; p <= 62; ++p) CHECK(t[p] < 0, "left of centre %d gives %d", p, t[p]);
    for (p = 64; p < 128; ++p) CHECK(t[p] > 0, "right of centre %d gives %d", p, t[p]);
    // symmetry: 63 + k against 63 - k within the step asymmetry of the game's 0..127 (63 left steps, 64 right)
    for (p = 1; p <= 63; ++p) {
        const double a = SndPos2D(63 + p), b = -SndPos2D(63 - p);
        CHECK(fabs(a - b) < 0.02, "positions %d: %.3f / %.3f", p, a, b);
    }
    // the smooth law is never steeper than the stock one (the other ear never quieter)
    for (p = 0; p < 128; ++p) CHECK(SndAbs(t[p]) <= SndAbs(SndPanStock2D(p)) || SndAbs(t[p]) == SND_PAN_MAX,
                                    "pan %d: smooth %d steeper than stock %d", p, t[p], SndPanStock2D(p));
    // one ear only (40 dB or more down): the stock law from 26 steps off centre, the smooth one only in the last steps
    {
        int ns = 0, ne = 0;
        for (p = 0; p < 128; ++p) ns += SndAbs(SndPanStock2D(p)) >= SND_ONE_EAR, ne += SndAbs(t[p]) >= SND_ONE_EAR;
        CHECK(ns > 60 && ne <= 4, "one-ear pans: stock %d, smooth %d of 128", ns, ne);
    }
    // streams: -15..15
    CHECK(s[15] == 0 && s[30] == SND_PAN_MAX && s[0] == -SND_PAN_MAX, "stream centre / sides %d %d %d", s[15], s[30], s[0]);
    for (p = 1; p < 31; ++p) CHECK(s[p] > s[p - 1], "stream pan not rising at %d", p - 15);
    for (p = 0; p < 31; ++p) CHECK(s[p] == -s[30 - p], "stream pans not symmetric at %d", p - 15);
    CHECK(SndPanStockStream(15) == 9990 && SndPanStockStream(-1) == -666, "stock stream law");
    // width: 0 = all centre, a smaller width never wider, 50 % = x 0.5
    for (w = 0; w <= 100; w += SND_WIDTH_STEP) {
        int tw[128], sw[31];
        SndBuildPans(tw, sw, w, 0);
        for (p = 0; p < 128; ++p) {
            if (w == 0) CHECK(tw[p] == 0, "width 0, pan %d gives %d", p, tw[p]);
            CHECK(SndAbs(tw[p]) <= SndAbs(t[p]), "width %d wider than 100 at %d", w, p);
        }
        for (p = 0; p < 31; ++p) CHECK(SndAbs(sw[p]) <= SndAbs(s[p]), "stream width %d wider at %d", w, p - 15);
    }
    {   // x 0.5: tan(3 pi / 8) = 2.414 -> 765
        const int h = SndPanSmooth(1.0, 50, 0);
        CHECK(h >= 760 && h <= 770, "width 50 at the hard right: %d", h);
    }
    CHECK(SndPanSmooth(0.7, -5, 0) == 0 && SndPanSmooth(0.7, 250, 0) == SndPanSmooth(0.7, 100, 0), "width clamped");
    CHECK(SndPos2D(-9) == -1.0 && SndPos2D(400) == 1.0 && SndPosStream(99) == 1.0, "positions clamped");
}

static void TestFloor(void)
{
    int t[128], s[31], p, f;
    for (f = SND_FLOOR_MIN; f <= SND_FLOOR_MAX; f += 300) {
        SndBuildPans(t, s, 100, f);
        for (p = 0; p < 128; ++p) CHECK(SndAbs(t[p]) <= f, "floor %d: pan %d gives %d", f, p, t[p]);
        for (p = 0; p < 31; ++p) CHECK(SndAbs(s[p]) <= f, "floor %d: stream %d gives %d", f, p - 15, s[p]);
        CHECK(t[127] == f && t[0] == -f && t[63] == 0, "floor %d: the sides at the floor, centre 0", f);
    }
    // the floor only caps: a pan inside it is the plain smooth law's
    SndBuildPans(t, s, 100, SND_FLOOR_DEF);
    for (p = 0; p < 128; ++p) {
        const int plain = SndPanSmooth(SndPos2D(p), 100, 0);
        if (SndAbs(plain) <= SND_FLOOR_DEF) CHECK(t[p] == plain, "pan %d: %d, the plain law %d", p, t[p], plain);
    }
}

static void TestNight(void)
{
    int db, lvl;
    for (db = SND_VOL_MIN; db <= 0; db += 25) CHECK(SndNightDb(db, 0) == db, "Off changes %d", db);
    for (lvl = 1; lvl < SND_NIGHT_N; ++lvl) {
        int prev = SND_VOL_MIN - 1;
        for (db = SND_VOL_MIN; db <= 0; db += 25) {
            const int o = SndNightDb(db, lvl);
            CHECK(o <= 0 && o >= SND_VOL_MIN, "level %d: %d -> %d out of range", lvl, db, o);
            CHECK(o >= prev, "level %d not monotonic at %d (%d < %d)", lvl, db, o, prev);
            if (db <= SND_SILENT) CHECK(o == db, "level %d: silence %d moved to %d", lvl, db, o);
            if (db > SND_SILENT && db < SND_NIGHT_PIVOT) CHECK(o > db, "level %d: quiet %d not raised (%d)", lvl, db, o);
            if (db > SND_NIGHT_PIVOT) CHECK(o < db, "level %d: loud %d not lowered (%d)", lvl, db, o);
            prev = o;
        }
        CHECK(SndNightDb(SND_NIGHT_PIVOT, lvl) == SND_NIGHT_PIVOT, "the pivot moves");
    }
    // the game's own range: a 2D sound at most -14 dB (x 0.2), at least -60 dB; 3D up to 0 dB
    CHECK(SndNightDb(-1400, 1) == -1580 && SndNightDb(-1400, 2) == -1700, "-14 dB: %d / %d", SndNightDb(-1400, 1), SndNightDb(-1400, 2));
    CHECK(SndNightDb(-6000, 1) == -4800 && SndNightDb(-6000, 2) == -4000, "-60 dB: %d / %d", SndNightDb(-6000, 1), SndNightDb(-6000, 2));
    CHECK(SndNightDb(0, 1) == -600 && SndNightDb(0, 2) == -1000, "0 dB: %d / %d", SndNightDb(0, 1), SndNightDb(0, 2));
    CHECK(SndNightDb(500, 2) == -1000 && SndNightDb(-1400, 7) == -1400 && SndNightDb(-1400, -1) == -1400, "bad input");
    {   // the spread of the game's 2D range narrows: 46 dB -> 32 (Low) / 23 (High)
        const int lo = SndNightDb(-1400, 1) - SndNightDb(-6000, 1), hi = SndNightDb(-1400, 2) - SndNightDb(-6000, 2);
        CHECK(lo == 3220 && hi == 2300, "spread %d / %d", lo, hi);
    }
}

static void TestFold(void)
{
    short s[8] = { 1000, -1000, 32767, 32767, -32768, -32768, 300, 100 };
    short c[8];
    int i;
    CHECK(SndFold(1000, -1000) == 0 && SndFold(32767, 32767) == 32767 && SndFold(-32768, -32768) == -32768, "fold ends");
    CHECK(SndFold(300, 100) == 200 && SndFold(-3, 0) == -2, "fold rounds down (%d)", SndFold(-3, 0));
    SndFoldStereo16(s, 4);
    for (i = 0; i < 4; ++i) CHECK(s[2 * i] == s[2 * i + 1], "frame %d not folded", i);
    CHECK(s[0] == 0 && s[2] == 32767 && s[4] == -32768 && s[6] == 200, "fold values");
    memcpy(c, s, sizeof s);
    SndFoldStereo16(s, 4);
    CHECK(!memcmp(c, s, sizeof s), "a second fold changes the samples");
}

// v3.52 MERGE: the keeper x Quick Load from the pause (QL352): no re-apply while the sound is held, one on the first frame
// after; Night Mode keeps the held master's silence (a master of 0 gives the game's -10000) at every level
static void TestKeepHold(void)
{
    int f, lvl, applies = 0, during = 0, firstAfter = -1, engine = 1;
    const int byte = 0, holdFrom = 10, holdTo = 1810;  /* 30 s at 60 fps; the byte changed before the hold */
    CHECK(SndKeepStep(1, 1, 0) == SND_KEEP_NONE && SndKeepStep(1, 1, 1) == SND_KEEP_NONE, "same mode: nothing");
    CHECK(SndKeepStep(1, 0, 0) == SND_KEEP_APPLY && SndKeepStep(2, 1, 1) == SND_KEEP_WAIT, "differs: apply, or wait when held");
    for (f = 0; f < 2000; ++f) {
        const int held = f >= holdFrom && f < holdTo;
        const int k = SndKeepStep(engine, f < 5 ? engine : byte, held);
        if (k == SND_KEEP_APPLY) {
            ++applies;
            if (held) ++during;
            if (firstAfter < 0) firstAfter = f;
            engine = byte;
        }
    }
    CHECK(applies == 1 && during == 0, "one apply, none in the hold (%d, %d)", applies, during);
    CHECK(firstAfter == 5, "a change before the hold applies at once (%d)", firstAfter);
    engine = 1, applies = 0, during = 0, firstAfter = -1;
    for (f = 0; f < 2000; ++f) {  /* the byte differs only inside the hold (a load copied a save's Options) */
        const int held = f >= holdFrom && f < holdTo;
        const int k = SndKeepStep(engine, f >= 100 ? byte : engine, held);
        if (k == SND_KEEP_APPLY) {
            ++applies;
            if (held) ++during;
            if (firstAfter < 0) firstAfter = f;
            engine = byte;
        }
    }
    CHECK(applies == 1 && during == 0 && firstAfter == holdTo, "inside the hold: applied on the first frame after (%d at %d)",
          applies, firstAfter);
    for (lvl = 0; lvl < SND_NIGHT_N; ++lvl)
        CHECK(SndNightDb(SND_VOL_MIN, lvl) == SND_VOL_MIN, "Night Mode %d un-mutes the held master", lvl);
}

static void TestModes(void)
{
    int m, b;
    CHECK(SndModeByte(SND_MODE_MONO) == 0 && SndModeByte(SND_MODE_STEREO) == 1 && SndModeByte(SND_MODE_HEADPHONES) == 1 &&
          SndModeByte(SND_MODE_SURROUND) == 2, "mode -> byte");
    for (m = 0; m < SND_MODE_N; ++m) {
        CHECK(SndModeFromByte(SndModeByte(m), m) == m, "mode %d does not come back from its byte", m);
        CHECK(SndModeByte(m) >= 0 && SndModeByte(m) <= 2, "byte of %d out of the stock range", m);
    }
    // the first start's migration: 0 -> Mono, 1 -> Stereo, 2 -> Surround
    CHECK(SndModeFromByte(0, -1) == SND_MODE_MONO && SndModeFromByte(1, -1) == SND_MODE_STEREO &&
          SndModeFromByte(2, -1) == SND_MODE_SURROUND, "migration");
    CHECK(SndModeFromByte(1, SND_MODE_HEADPHONES) == SND_MODE_HEADPHONES && SndModeFromByte(1, SND_MODE_SURROUND) == SND_MODE_STEREO,
          "byte 1 keeps Headphones only");
    for (b = 3; b < 256; b += 50) CHECK(SndModeFromByte(b, -1) == SND_MODE_STEREO, "an edited byte %d", b);
    CHECK(SndModeFromByte(-1, SND_MODE_MONO) == SND_MODE_STEREO, "byte -1");
    for (m = 0; m < SND_MODE_N; ++m) {
        CHECK(SndParseMode(kSndModeName[m]) == m, "ini name %s", kSndModeName[m]);
        CHECK(SndParseMode(kSndModeLabel[m]) == m, "row label %s", kSndModeLabel[m]);
    }
    CHECK(SndParseMode(" stereo ") == SND_MODE_STEREO && SndParseMode("HEADPHONES") == SND_MODE_HEADPHONES &&
          SndParseMode("Monaural") == SND_MODE_MONO && SndParseMode("Dolby Digital") == SND_MODE_SURROUND &&
          SndParseMode("dolby") == SND_MODE_SURROUND, "ini spellings");
    CHECK(SndParseMode("") == -1 && SndParseMode("2") == -1 && SndParseMode("5.1") == -1 && SndParseMode(NULL) == -1 &&
          SndParseMode("Stereophonic sound with a very long name") == -1, "not modes");
    CHECK(SndParseOnOff("On") == 1 && SndParseOnOff("off") == 0 && SndParseOnOff("1") == 1 && SndParseOnOff("0") == 0 &&
          SndParseOnOff("yes") == -1 && SndParseOnOff(NULL) == -1, "On / Off");
    CHECK(SndParseNight("Off") == 0 && SndParseNight("low") == 1 && SndParseNight("HIGH") == 2 && SndParseNight("2") == 2 &&
          SndParseNight("3") == -1 && SndParseNight("Medium") == -1 && SndParseNight("") == -1, "Night Mode");
    CHECK(SndParseWidth("100") == 100 && SndParseWidth("0") == 0 && SndParseWidth(" 80 %") == 80 && SndParseWidth("35%") == 35 &&
          SndParseWidth("101") == -1 && SndParseWidth("-5") == -1 && SndParseWidth("abc") == -1 && SndParseWidth("") == -1 &&
          SndParseWidth("10000") == -1, "Width");
    CHECK(!strcmp(SndSpeakerName(0x00140004), "stereo") && !strcmp(SndSpeakerName(1), "headphones") &&
          !strcmp(SndSpeakerName(9), "5.1 surround") && !strcmp(SndSpeakerName(77), "unknown"), "speaker names");
}

int main(void)
{
    TestStockTable();
    TestSmooth();
    TestFloor();
    TestNight();
    TestFold();
    TestModes();
    TestKeepHold();  // v3.52 MERGE
    printf("sound_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
