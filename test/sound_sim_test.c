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

// v3.56 SD356: the four buffer sites (the flags' immediate gains exactly GLOBALFOCUS), Background's parser, the keep-awake
// signal (seamless loop, never all silence, peak = the level), the device watch's steps and names
static void TestSd356(void)
{
    int i, k, nz = 0, peak = 0, pending = 0;
    static short pcm[SND_KA_SAMPLES];
    char nm[16];
    const unsigned short wide[] = { 'H', 'e', 'a', 'd', 0x00E9, '"', 1, 'x', 0 };
    for (i = 0; i < 4; ++i) {
        const SndBgSite *s = &kSndBgSites[i];
        unsigned char b[8];
        unsigned long before, after;
        int immAt = s->at - 1;
        CHECK(s->len <= 8 && s->at >= 1 && s->at < s->len && s->stock[s->at] == 0x00, "site %d: the flags' second byte is 00", i);
        memcpy(b, s->stock, sizeof b);
        memcpy(&before, b + immAt, 4);
        b[s->at] = SndBgByte(1);
        memcpy(&after, b + immAt, 4);
        CHECK(after == (before | SND_DSBCAPS_GLOBALFOCUS) && !(before & SND_DSBCAPS_GLOBALFOCUS), "site %d: Play adds GLOBALFOCUS only (%lX -> %lX)",
              i, before, after);
        CHECK((before == 0xC0 || before == 0x90) && (after & 0xFFFF00FFu) == before, "site %d: CTRLPAN / CTRL3D | CTRLVOLUME kept", i);
        b[s->at] = SndBgByte(0);
        CHECK(!memcmp(b, s->stock, s->len), "site %d: Mute is the stock byte", i);
        for (k = 0; k < i; ++k) CHECK(kSndBgSites[k].va + kSndBgSites[k].len <= s->va, "sites %d / %d apart, in order", k, i);
    }
    // the sites the v3.52 sound round and Playable Eileen write are elsewhere (0x409AD0 / 0x409691 / 0x409BC5 / 0x40AB4A)
    CHECK(kSndBgSites[1].va > 0x409AD0 + 6 && kSndBgSites[1].va + 5 <= 0x409BC5 && kSndBgSites[0].va + 6 <= 0x409691, "apart from PE / SND352");
    CHECK(SndParseBg("Mute") == SND_BG_MUTE && SndParseBg("play") == SND_BG_PLAY && SndParseBg(" PLAY") == SND_BG_PLAY &&
          SndParseBg("0") == 0 && SndParseBg("1") == 1 && SndParseBg("On") == -1 && SndParseBg("") == -1 && SndParseBg(NULL) == -1,
          "Background parser");
    for (k = 0; k < SND_BG_N; ++k) CHECK(SndParseBg(kSndBgName[k]) == k, "Background name %s", kSndBgName[k]);
    SndKeepAwakeFill(pcm, SND_KA_SAMPLES, SND_KA_LEVEL_DEF);
    for (i = 0; i < SND_KA_SAMPLES; ++i) {
        nz += pcm[i] != 0;
        if (SndAbs(pcm[i]) > peak) peak = SndAbs(pcm[i]);
    }
    CHECK(SND_KA_SAMPLES % SND_KA_PERIOD == 0 && SND_KA_RATE / SND_KA_PERIOD == 45, "a whole number of 45 Hz periods");
    CHECK(peak == SND_KA_LEVEL_DEF && nz > SND_KA_SAMPLES * 9 / 10, "keep-awake: peak %d, %d of %d samples not zero", peak, nz, SND_KA_SAMPLES);
    for (i = 0; i + SND_KA_PERIOD < SND_KA_SAMPLES; i += 97) CHECK(pcm[i] == pcm[i + SND_KA_PERIOD], "keep-awake: periodic at %d", i);
    CHECK(pcm[0] == 0 && SndAbs(pcm[SND_KA_SAMPLES - 1]) <= 1, "keep-awake: the loop is seamless (%d .. %d)", pcm[SND_KA_SAMPLES - 1], pcm[0]);
    SndKeepAwakeFill(pcm, 100, 0);
    for (i = 0, nz = 0; i < 100; ++i) nz += pcm[i] != 0;
    CHECK(nz == 0, "keep-awake level 0 = digital silence");
    SndKeepAwakeFill(pcm, SND_KA_SAMPLES, 1000);
    for (i = 0, peak = 0; i < SND_KA_SAMPLES; ++i) if (SndAbs(pcm[i]) > peak) peak = SndAbs(pcm[i]);
    CHECK(peak == SND_KA_LEVEL_MAX, "keep-awake level clamped to %d (%d)", SND_KA_LEVEL_MAX, peak);
    CHECK(SndKeepLevelOk(0) && SndKeepLevelOk(64) && !SndKeepLevelOk(65) && !SndKeepLevelOk(-1), "level range");
    CHECK(SndKeepLevelDb10(8) == -722 && SndKeepLevelDb10(64) == -542 && SndKeepLevelDb10(1) == -903 && SndKeepLevelDb10(0) == -1000,
          "level in dB (%d)", SndKeepLevelDb10(8));
    CHECK(SndDevStep(1, &pending) == SND_DEV_SAME && !pending, "the game's device again: nothing");
    CHECK(SndDevStep(0, &pending) == SND_DEV_NEW && pending, "a new default");
    CHECK(SndDevStep(0, &pending) == SND_DEV_NEW_AGAIN && pending, "another new default");
    CHECK(SndDevStep(1, &pending) == SND_DEV_BACK && !pending, "back to the game's device");
    CHECK(!strcmp(SndDevStateName(1), "active") && !strcmp(SndDevStateName(8), "unplugged") && !strcmp(SndDevStateName(3), "in an unknown state"),
          "device states");
    SndDevNameAscii(wide, nm, sizeof nm);
    CHECK(!strcmp(nm, "Head???x"), "device name in ASCII (%s)", nm);
    SndDevNameAscii(wide, nm, 3);
    CHECK(!strcmp(nm, "He"), "device name cut (%s)", nm);
    SndDevNameAscii(NULL, nm, sizeof nm);
    CHECK(nm[0] == 0, "no name");
}

// v3.58 SD358: Volume 0 = silent (the game's -60 dB floor), the 3D path, the 0x2774 stream's fade, and Background Mute by the
// sound session across focus changes, Mute / Play switches, a session lost and the next start
static void TestZero(void)
{
    int raw, n, worst = 0;
    // the bug: the game's own 0 reaches DirectSound as -60 dB (2D and streams), and Night Mode lifted it
    CHECK(SndGameAtt2D(SND_VOL_MIN) == -6000 && SndGameAtt2D(SND_ZERO_RAW) == -6000, "the game's 0 -> -60 dB (%d / %d)",
          SndGameAtt2D(SND_VOL_MIN), SndGameAtt2D(SND_ZERO_RAW));
    CHECK(SndNightDb(-6000, 2) == -4000 && SndNightDb(-6000, 1) == -4800, "Night Mode lifted the floor to -40 / -48 dB (%d / %d)",
          SndNightDb(-6000, 2), SndNightDb(-6000, 1));
    CHECK(SndGameAtt2D(0) == -1397 || SndGameAtt2D(0) == -1398, "full volume = -14 dB (%d)", SndGameAtt2D(0));
    // Refined: the game's 0 = silence; every other level as the game gives it
    CHECK(SndZero2D(SND_VOL_MIN, SndGameAtt2D(SND_VOL_MIN)) == SND_VOL_MIN && SndZero2D(-9600, SndGameAtt2D(-9600)) == SND_VOL_MIN,
          "the game's 0 is silent");
    for (raw = -9599, n = 0; raw <= 0; ++raw) n += SndZero2D(raw, SndGameAtt2D(raw)) != SndGameAtt2D(raw);
    CHECK(n == 0, "every level above the game's 0 unchanged (%d changed)", n);
    // the levels the game makes: SndDb(1) = -4207 (a sound at 1 of 127), the stream table's level 1 = -361 x 10
    CHECK(SndZero2D(-4207, SndGameAtt2D(-4207)) == SndGameAtt2D(-4207) && SndZero2D(-3610, SndGameAtt2D(-3610)) == SndGameAtt2D(-3610),
          "the quietest real levels stay (%d / %d)", SndGameAtt2D(-4207), SndGameAtt2D(-3610));
    // Night Mode never lifts silence
    CHECK(SndNightDb(SND_VOL_MIN, 1) == SND_VOL_MIN && SndNightDb(SND_VOL_MIN, 2) == SND_VOL_MIN, "Night Mode leaves silence");
    // 3D: silent exactly for the game's 0 (and below), the rest unchanged
    CHECK(SndGameAtt3D(-9600) == SND_ZERO_3D_OUT && SndGameAtt3D(-9599) > SND_ZERO_3D_OUT && SndGameAtt3D(SND_VOL_MIN) <= SND_ZERO_3D_OUT,
          "3D: the threshold (%d / %d / %d)", SndGameAtt3D(-9600), SndGameAtt3D(-9599), SndGameAtt3D(SND_VOL_MIN));
    for (raw = SND_VOL_MIN, n = 0; raw <= 0; ++raw) {
        const int o = SndGameAtt3D(raw), z = SndZero3D(o);
        if ((raw <= -9600) != (z == SND_VOL_MIN)) ++n, worst = raw;
        if (raw > -9600 && z != o) ++n, worst = raw;
    }
    CHECK(n == 0, "3D: silent iff the game's 0 (%d wrong, last %d)", n, worst);
    // streams: the level the game set decides; 0x2774's own fade (+500 a call up to 0) cannot lift a 0 any more
    {
        int vol = -9600, calls, loud = 0, unset = 0;
        for (calls = 0; calls < 40; ++calls) {
            vol += 500;
            if (vol > 0) vol = 0;
            loud += SndZeroStream(-9600, vol, SndGameAtt2D(vol)) != SND_VOL_MIN;
            unset += SndZeroStream(SND_STR_UNSET, vol, SndGameAtt2D(vol)) != SND_VOL_MIN;
        }
        CHECK(loud == 0, "0x2774 set to the game's 0 stays silent through its fade (%d loud calls)", loud);
        CHECK(unset == 40, "without the set level (no 0x40B2E7) the fade is the game's (%d)", unset);
        vol = -1980;  // Music Volume 1 of 10 (level 12 of 127: -198 x 10)
        for (calls = 0, loud = 0; calls < 10; ++calls) {
            vol += 500;
            if (vol > 0) vol = 0;
            loud += SndZeroStream(-1980, vol, SndGameAtt2D(vol)) == SndGameAtt2D(vol);
        }
        CHECK(loud == 10, "a stream set above 0 keeps the game's own level (%d of 10)", loud);
        CHECK(SndZeroStream(-9600, -9600, -6000) == SND_VOL_MIN && SndZeroStream(-3610, -3610, SndGameAtt2D(-3610)) == SndGameAtt2D(-3610),
              "a plain stream: by its level");
    }
}

// v3.58 SD358-001: each session has an identity; Refined holds the ones it muted (touched by the user through the Volume
// Mixer's callback); a kill leaves SH4Refined.muted naming the held, untouched ones; the next start unmutes only those
// v3.58 SD358-002: the marker on disk (named) changes only when a whole write and its replace work; a session is muted only
// after the marker that names it is in place
// v3.58 SD358-003: each entry on disk is pending (written before the SetMute) or muted (rewritten after it worked); only a
// muted one unmutes at a start; SetMute(TRUE) can fail; one write can fail alone (script); a failed write is written again at
// the watch thread's next pass (dirty)
typedef struct { int muted, held, conf, touched, lastMute, user, named, staleUser, absent, leftPend; } SimSess;  // user = their mute;
// named = the entry on disk (SND_MARK_NONE / _PENDING / _MUTED); leftPend = Refined's own mute a start left (pending: logged)
typedef struct {
    int bg, front, sessOk, sitesPlay, keepHeld, wanted;
    int disk;  // 0 fine, 1 writes come out short, 2 the replace fails (the temporary file is written, never moved over)
    int notMuted;  // sessions left unmuted because the marker could not name them
    int muteFail, muteFailed;  // v3.58 SD358-003: SetMute(TRUE) fails while set; how many failed
    int dirty;                 // v3.58 SD358-003: the last write failed (written again at the next pass)
    int script[8], nscript, iscript;  // v3.58 SD358-003: the next writes' results (1 whole, 0 failed), before `disk` decides
    int pendLeft, undone, confFail;  // v3.58 SD358-003: pending entries a start found muted; user mutes a start undid; confirms lost
    SimSess s[3];
} SimBg;
// SdMarkerWrite: the new marker names `want` (held, untouched, + the one about to be muted); 1 = in place
static int SimMarkerWrite(SimBg *b, const int *want)
{
    int i, ok;
    if (b->iscript < b->nscript) {
        ok = b->script[b->iscript++];  // v3.58 SD358-003: a single write's result
    } else {
        const int writesOk = SndMarkWriteOk(100, b->disk == 1 ? 60 : 100, 1);
        ok = SndMarkCommit(1, writesOk, writesOk, 1, writesOk && b->disk != 2);
    }
    b->dirty = SndMarkDirty(ok);
    if (!ok) return 0;  // the old marker stays
    for (i = 0; i < 3; ++i) b->s[i].named = want[i];
    return 1;
}
static void SimScript(SimBg *b, int ok)  // v3.58 SD358-003: the next write (after those queued) comes out as `ok`
{
    if (b->iscript >= b->nscript) b->iscript = b->nscript = 0;
    if (b->nscript < 8) b->script[b->nscript++] = ok;
}
static void SimWant(const SimBg *b, int *want, int extra)  // v3.58 SD358-003: the state of each entry
{
    int i;
    for (i = 0; i < 3; ++i)
        want[i] = i == extra ? SND_MARK_PENDING
                : b->s[i].held && SndMarkKeep(b->s[i].touched) ? SndMarkHeldState(b->s[i].conf) : SND_MARK_NONE;
}
static void SimThreadMute(SimBg *b)  // the request, and the re-sweep every 2 s while behind other windows
{
    int i, want[3];
    for (i = 0; i < 3; ++i)
        if (b->sessOk && !b->s[i].absent && SndSessMuteAct(b->s[i].muted, b->s[i].held)) {
            SimSess *const s = &b->s[i];
            SimWant(b, want, i);
            if (!SndSessMayMute(SimMarkerWrite(b, want))) {  // the pending entry first
                ++b->notMuted;
                continue;
            }
            if (b->muteFail) {  // v3.58 SD358-003: SetMute failed: dropped; a failed clean-up leaves the entry pending
                ++b->muteFailed;
                SimWant(b, want, -1);
                SimMarkerWrite(b, want);
                continue;
            }
            s->muted = 1, s->held = 1, s->conf = 1, s->touched = SND_TOUCH_NONE, s->lastMute = 1, s->user = 0, s->staleUser = 0;
            s->leftPend = 0;
            SimWant(b, want, -1);
            if (!SimMarkerWrite(b, want)) ++b->confFail;  // v3.58 SD358-003: the confirm (dirty: again at the next pass)
        }
    b->wanted = 1;
}
static void SimThreadUnmute(SimBg *b)
{
    int i, want[3] = { 0, 0, 0 };
    for (i = 0; i < 3; ++i) {
        SimSess *const s = &b->s[i];
        if (s->held && SndSessUnmuteAct(s->muted, s->touched)) s->muted = 0;
        s->held = 0, s->conf = 0;
    }
    SimMarkerWrite(b, want);  // a failure keeps old names of sessions that are not muted now: harmless at the next start
    b->wanted = 0;
}
static void SimUser(SimBg *b, int i)  // the user toggles a session's mute in the Volume Mixer (another event context)
{
    SimSess *const s = &b->s[i];
    int want[3];
    s->muted = !s->muted, s->user = s->muted, s->staleUser = 0, s->leftPend = 0;
    if (s->held) {
        s->touched = SndSessTouchStep(s->touched, &s->lastMute, s->muted);
        SimWant(b, want, -1);
        if (!SimMarkerWrite(b, want) && s->named == SND_MARK_MUTED && s->user) s->staleUser = 1;  // README: the next start may undo this one
    } else if (s->user && s->named == SND_MARK_MUTED) {  // (the model's own rule, not the predicate under test)
        s->staleUser = 1;  // v3.58 SD358-003: a confirmed entry an unmute's / a start's failed rewrite left (README: until a rewrite works)
    }
}
static void SimVolume(SimBg *b, int i)  // the user moves a held session's volume slider only (the mute as it was)
{
    SimSess *const s = &b->s[i];
    if (s->held) s->touched = SndSessTouchStep(s->touched, &s->lastMute, s->muted);
}
static void SimKillRestart(SimBg *b)  // killed from outside; the next start reads SH4Refined.muted
{
    int i, want[3] = { 0, 0, 0 };
    for (i = 0; i < 3; ++i) {
        SimSess *const s = &b->s[i];
        if (SndSessStartAct(s->muted, SndMarkMayUnmute(s->named))) {  // v3.58 SD358-003: a confirmed entry only
            if (s->user && !s->staleUser) ++b->undone;  // the user's own mute undone by the start
            s->muted = 0, s->user = 0, s->staleUser = 0;
        } else if (s->muted && s->named == SND_MARK_PENDING) {
            ++b->pendLeft;            // left as it is, logged ("unmute it in the Windows Volume Mixer")
            s->leftPend = !s->user;   // Refined's own mute that never reached "muted" on disk (README: the remaining window)
        }
        s->held = 0, s->conf = 0, s->touched = SND_TOUCH_NONE;
    }
    SimMarkerWrite(b, want);  // pending entries dropped (a failure keeps them: the next start leaves them alone again)
    b->wanted = 0;
}
// one SndBgTick + the watch thread's answer
static void SimTick(SimBg *b)
{
    const int want = SndBgWantMute(b->bg, !b->front);
    if (b->dirty) {  // v3.58 SD358-003: the watch thread's pass writes a failed marker again
        int w[3];
        SimWant(b, w, -1);
        SimMarkerWrite(b, w);
    }
    if (want) SimThreadMute(b);
    else if (b->wanted) SimThreadUnmute(b);
    b->sitesPlay = SndBgSitesPlay(b->bg, b->sessOk);
    b->keepHeld = SndKeepHold(want, b->sessOk);
}
static void TestBgSession(void)
{
    SimBg b;
    unsigned seed = 12345;
    int step, i, bad = 0, silentStart = 0, kills = 0, refused = 0, pendLeft = 0, recovered = 0, confFail = 0, muteFailed = 0;
    unsigned long t;
    const char *id;
    size_t n;
    int lm, st;  // v3.58 SD358-003: + st
    memset(&b, 0, sizeof b);
    b.bg = SND_BG_MUTE, b.front = 1, b.sessOk = 1;
    b.s[2].muted = b.s[2].user = 1;  // the user muted one session of the game in Windows' mixer
    SimTick(&b);
    CHECK(b.sitesPlay && !b.keepHeld && !b.s[0].muted && b.s[2].muted, "in front, Mute by the session: sites Play, nothing muted");
    b.front = 0, SimTick(&b);
    CHECK(b.s[0].muted && b.s[1].muted && b.s[0].held && !b.s[2].held && !b.keepHeld, "behind: Refined mutes every session (the tone too)");
    b.front = 1, SimTick(&b);
    CHECK(!b.s[0].muted && !b.s[1].muted && b.s[2].muted, "in front: only Refined's unmuted; the user's stays muted");
    b.bg = SND_BG_PLAY, SimTick(&b), b.front = 0, SimTick(&b);
    CHECK(!b.s[0].muted && b.sitesPlay && !b.wanted, "Play behind other windows: nothing muted");
    b.bg = SND_BG_MUTE, SimTick(&b);  // Mute chosen behind other windows (a pad works there)
    CHECK(b.s[0].muted && b.s[1].muted, "Mute while behind: muted at once");
    b.bg = SND_BG_PLAY, SimTick(&b);  // the Options "No" discard back to Play while behind
    CHECK(!b.s[0].muted && !b.s[1].muted && b.s[2].muted, "back to Play while behind: unmuted at once, the user's kept");
    // no session: v3.56's rule (the sites' own byte) and the tone stopped behind other windows under Mute only
    b.sessOk = 0, b.bg = SND_BG_MUTE, b.front = 1, SimTick(&b);
    CHECK(!b.sitesPlay && !b.keepHeld, "no session, in front: sites Mute, tone plays");
    b.front = 0, SimTick(&b);
    CHECK(!b.sitesPlay && b.keepHeld && !b.s[0].muted, "no session, behind: sites Mute, tone stopped");
    b.bg = SND_BG_PLAY, SimTick(&b);
    CHECK(b.sitesPlay && !b.keepHeld, "no session, Play: sites Play, tone plays");
    // SD358-001: the start's rule is by the session's own entry, never by "a marker exists"
    CHECK(SndSessStartAct(1, 1) && !SndSessStartAct(1, 0) && !SndSessStartAct(0, 1) && !SndSessStartAct(0, 0), "the start's rule");
    memset(&b, 0, sizeof b);
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1;
    b.s[2].muted = b.s[2].user = 1;  // the user's own mute, before Refined's
    SimTick(&b), SimKillRestart(&b);  // killed behind other windows
    CHECK(!b.s[0].muted && !b.s[1].muted && b.s[2].muted, "killed behind: the next start unmutes Refined's two, the user's stays");
    SimTick(&b), SimUser(&b, 0), SimTick(&b);  // behind again; the user unmutes one in the mixer: not muted again by the re-sweep
    CHECK(!b.s[0].muted && b.s[0].touched == SND_TOUCH_UNMUTED && !b.s[0].named && b.s[1].muted && b.s[1].named,
          "unmuted by the user while behind: left playing, no longer named");
    SimUser(&b, 0), SimVolume(&b, 1);  // the user mutes it again (theirs now); a volume slider on the other changes nothing
    CHECK(b.s[0].muted && b.s[0].touched == SND_TOUCH_USER && b.s[1].touched == SND_TOUCH_NONE && b.s[1].named, "touch states");
    SimKillRestart(&b);
    CHECK(b.s[0].muted && !b.s[1].muted, "killed again: the user's re-mute stays, Refined's own is unmuted");
    b.front = 1, SimTick(&b), b.front = 0, SimTick(&b), SimUser(&b, 1), SimUser(&b, 1), b.front = 1, SimTick(&b);
    CHECK(b.s[1].muted && b.s[1].user, "the user unmuted and muted a held session: Refined does not unmute it in front");
    // SD358-002: a write that comes out short, a replace that fails
    CHECK(SndMarkWriteOk(100, 100, 1) && !SndMarkWriteOk(100, 99, 1) && !SndMarkWriteOk(100, 0, 1) && !SndMarkWriteOk(100, 100, 0),
          "a write counts only whole");
    CHECK(SndMarkCommit(1, 1, 1, 1, 1) && !SndMarkCommit(0, 1, 1, 1, 1) && !SndMarkCommit(1, 0, 1, 1, 1) && !SndMarkCommit(1, 1, 0, 1, 1) &&
          !SndMarkCommit(1, 1, 1, 0, 1) && !SndMarkCommit(1, 1, 1, 1, 0) && SndSessMayMute(1) && !SndSessMayMute(0), "the commit's steps");
    memset(&b, 0, sizeof b);
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1, b.disk = 1;  // every write short (a full disk)
    SimTick(&b);
    CHECK(!b.s[0].muted && !b.s[1].muted && !b.s[0].named && b.notMuted == 3, "short writes: nothing muted, nothing named (%d)", b.notMuted);
    SimKillRestart(&b);
    CHECK(!b.s[0].muted && !b.s[1].muted && !b.s[2].muted, "short writes, killed: no session left muted");
    memset(&b, 0, sizeof b);
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1;
    b.s[1].absent = 1, b.s[2].absent = 1;  // two sessions the game opens later
    SimTick(&b);
    CHECK(b.s[0].muted && b.s[0].named, "one session muted and named");
    b.disk = 2, b.s[1].absent = 0, SimTick(&b);  // a second one appears; the replace fails
    CHECK(!b.s[1].muted && !b.s[1].named && b.s[0].named && b.s[0].muted, "a failed replace: the new one not muted, the old marker kept");
    b.front = 1, SimTick(&b);  // back in front: unmuted; the marker cannot be rewritten: it still names an unmuted session
    CHECK(!b.s[0].muted && b.s[0].named, "unmuted in front; the old name stays (harmless)");
    b.disk = 0, SimKillRestart(&b);
    CHECK(!b.s[0].muted && !b.s[0].named, "the next start: the named one is not muted: nothing to do, the name dropped");
    // SD358-003: pending (before the SetMute) / muted (after it worked); only a confirmed entry unmutes at a start
    CHECK(SndMarkMayUnmute(SND_MARK_MUTED) && !SndMarkMayUnmute(SND_MARK_PENDING) && !SndMarkMayUnmute(SND_MARK_NONE) &&
          !SndSessStartAct(1, SndMarkMayUnmute(SND_MARK_PENDING)) && SndSessStartAct(1, SndMarkMayUnmute(SND_MARK_MUTED)) &&
          SndMarkHeldState(0) == SND_MARK_PENDING && SndMarkHeldState(1) == SND_MARK_MUTED && SndMarkKeepAtStart(0, SND_MARK_MUTED) &&
          !SndMarkKeepAtStart(1, SND_MARK_MUTED) && !SndMarkKeepAtStart(0, SND_MARK_PENDING) && SndMarkDirty(0) && !SndMarkDirty(1),
          "SD358-003: the entry states");
    memset(&b, 0, sizeof b);  // (1) the pending write fails: no mute
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1, b.s[1].absent = b.s[2].absent = 1;
    SimScript(&b, 0), SimTick(&b);
    CHECK(!b.s[0].muted && !b.s[0].held && !b.s[0].named && b.notMuted == 1 && b.dirty, "SD358-003: the pending write fails: not muted");
    SimTick(&b);  // the next pass: written again (nothing to name), the re-sweep mutes it
    CHECK(b.s[0].muted && b.s[0].named == SND_MARK_MUTED && !b.dirty, "SD358-003: the next pass mutes it, confirmed");
    memset(&b, 0, sizeof b);  // (2) SetMute fails and its clean-up write fails: the entry stays pending
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1, b.s[1].absent = b.s[2].absent = 1, b.muteFail = 1;
    SimScript(&b, 1), SimScript(&b, 0), SimTick(&b);
    CHECK(!b.s[0].muted && !b.s[0].held && b.s[0].named == SND_MARK_PENDING && b.dirty && b.muteFailed == 1,
          "SD358-003: SetMute failed, the clean-up failed: a pending entry for an unmuted session");
    b.disk = 2, b.muteFail = 0, b.bg = SND_BG_PLAY, SimTick(&b);  // the disk stays bad; Play behind other windows
    SimUser(&b, 0);  // the user mutes that session in the Volume Mixer
    CHECK(b.s[0].muted && b.s[0].user && !b.s[0].staleUser && b.s[0].named == SND_MARK_PENDING, "SD358-003: the user's mute on it");
    SimKillRestart(&b);
    CHECK(b.s[0].muted && b.s[0].user && !b.undone && b.pendLeft == 1 && !b.s[0].leftPend,
          "SD358-003: killed: the next start leaves the user's mute alone (a pending entry never unmutes)");
    memset(&b, 0, sizeof b);  // (3) the confirm write fails, the next pass writes it
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1, b.s[1].absent = b.s[2].absent = 1;
    SimScript(&b, 1), SimScript(&b, 0), SimTick(&b);
    CHECK(b.s[0].muted && b.s[0].held && b.s[0].named == SND_MARK_PENDING && b.dirty && b.confFail == 1,
          "SD358-003: muted; the confirm write failed: still pending on disk");
    SimTick(&b);
    CHECK(b.s[0].muted && b.s[0].named == SND_MARK_MUTED && !b.dirty, "SD358-003: the next pass confirms it");
    SimKillRestart(&b);
    CHECK(!b.s[0].muted && !b.pendLeft && !b.s[0].named, "SD358-003: killed after the retry: the next start unmutes it");
    memset(&b, 0, sizeof b);  // (4) the confirm write and every retry fail, then a kill: the remaining window
    b.bg = SND_BG_MUTE, b.front = 0, b.sessOk = 1, b.s[1].absent = b.s[2].absent = 1;
    SimScript(&b, 1), SimScript(&b, 0), SimTick(&b);
    b.disk = 1, SimTick(&b), SimTick(&b);
    CHECK(b.s[0].muted && b.s[0].named == SND_MARK_PENDING && b.dirty, "SD358-003: every retry failed: still pending");
    SimKillRestart(&b);
    CHECK(b.s[0].muted && !b.s[0].user && b.pendLeft == 1 && b.s[0].leftPend && !b.undone,
          "SD358-003: killed: the next start leaves it muted and logs it (README: unmute it in the Volume Mixer)");
    SimUser(&b, 0), b.disk = 0, b.front = 1, SimTick(&b);  // the user follows the log line (the game in front)
    CHECK(!b.s[0].muted && !b.s[0].leftPend && !b.s[0].user && b.s[0].named == SND_MARK_NONE && !b.dirty,
          "SD358-003: unmuted by the user; the pending entry dropped");
    // the marker's line format and age
    CHECK(SndMarkParse("1764200000\t{0.0.0.00000000}.{abc}|\\Device\\x.exe%b{GUID}\r\n", &t, &st, &id, &n) && t == 1764200000ul &&
          st == SND_MARK_MUTED && n == strlen("{0.0.0.00000000}.{abc}|\\Device\\x.exe%b{GUID}") && id[0] == '{',
          "marker line without a state (the v3.58 test builds): confirmed (%lu, %u)", t, (unsigned)n);
    CHECK(SndMarkParse("1764200000\tmuted\t{0.0.0.00000000}.{abc}|x%b{G}\r\n", &t, &st, &id, &n) && st == SND_MARK_MUTED &&
          n == strlen("{0.0.0.00000000}.{abc}|x%b{G}") && id[0] == '{' &&
          SndMarkParse("1764200001\tpending\t{a}\n", &t, &st, &id, &n) && st == SND_MARK_PENDING && t == 1764200001ul && n == 3 &&
          id[0] == '{' && SndMarkParse("5\tmutedx\t{a}", &t, &st, &id, &n) && st == SND_MARK_PENDING &&
          SndMarkParse("5\t\t{a}", &t, &st, &id, &n) && st == SND_MARK_PENDING && n == 3 && !SndMarkParse("5\tmuted\t\r\n", &t, &st, &id, &n) &&
          !SndMarkParse("5\tpending\t", &t, &st, &id, &n) && !strcmp(SndMarkWord(SND_MARK_MUTED), "muted") &&
          !strcmp(SndMarkWord(SND_MARK_PENDING), "pending"), "SD358-003: marker states read (an unknown state is pending)");
    CHECK(!SndMarkParse("# text", &t, &st, &id, &n) && !SndMarkParse("12 x", &t, &st, &id, &n) && !SndMarkParse("12\t\r\n", &t, &st, &id, &n) &&
          !SndMarkParse("\tx", &t, &st, &id, &n) && !SndMarkParse(NULL, &t, &st, &id, &n) &&
          !SndMarkParse("SH4 Refined muted ...", &t, &st, &id, &n), "marker: text, v3.58 SD358's old one-line file and bad lines are not entries");
    CHECK(!SndMarkExpired(1000000000ul, 1000000000ul - 86400ul) && SndMarkExpired(1000000000ul, 1000000000ul - SND_MARK_MAX_AGE - 1) &&
          SndMarkExpired(1000000000ul, 1000000000ul + 2 * 86400ul), "marker age");
    lm = 1;
    CHECK(SndSessTouchStep(SND_TOUCH_NONE, &lm, 1) == SND_TOUCH_NONE && lm == 1 && SndSessTouchStep(SND_TOUCH_NONE, &lm, 0) == SND_TOUCH_UNMUTED &&
          lm == 0 && SndSessTouchStep(SND_TOUCH_UNMUTED, &lm, 1) == SND_TOUCH_USER && SndSessTouchStep(SND_TOUCH_USER, &lm, 1) == SND_TOUCH_USER,
          "touch steps");
    CHECK(SndSessUnmuteAct(1, SND_TOUCH_NONE) && !SndSessUnmuteAct(1, SND_TOUCH_USER) && !SndSessUnmuteAct(0, SND_TOUCH_NONE) &&
          SndSessMuteAct(0, 0) && !SndSessMuteAct(0, 1) && !SndSessMuteAct(1, 0), "mute / unmute rules");
    // random walks with kills and disk failures: a session Refined holds (untouched) is always named on disk; Refined's sessions
    // are muted while Mute and behind (unless the user unmuted one, or the marker could not name it); the user's mute is never
    // undone by Refined (but the README's case: a rewrite failed after their change); a start never leaves Refined's mute on
    // v3.58 SD358-003: + SetMute failures and single failed writes (a lost confirm, a lost clean-up); a start never undoes the
    // user's mute (a pending entry never unmutes); a start leaves Refined's own mute on only for a pending entry (the remaining
    // window: the confirm and every retry failed), logged; the user then unmutes it in the mixer
    memset(&b, 0, sizeof b);
    b.front = 1, b.sessOk = 1;
    for (step = 0; step < 300000; ++step) {
        unsigned r;
        int k, refusedNow;
        const int nm0 = b.notMuted, mf0 = b.muteFailed;
        seed = seed * 1103515245u + 12345u;
        r = (seed >> 16) % 14;
        k = (int)((seed >> 8) % 3);
        if (r < 3) b.front = !b.front;
        else if (r < 5) b.bg = !b.bg;
        else if (r < 7) SimUser(&b, k);
        else if (r == 7) SimVolume(&b, k);
        else if (r == 8) {
            SimKillRestart(&b), ++kills;
            for (i = 0; i < 3; ++i) {
                silentStart += b.s[i].muted && !b.s[i].user && !b.s[i].leftPend;
                if (b.s[i].leftPend) SimUser(&b, i), ++recovered;  // the log line's advice: unmuted in the Volume Mixer
            }
        } else if (r == 9) {
            b.disk = (b.disk + 1 + (int)((seed >> 4) % 2)) % 3;  // the disk goes bad (short / no replace) or comes back
        } else if (r == 10) {
            b.muteFail = (seed >> 4) % 3 == 0;  // v3.58 SD358-003: SetMute(TRUE) refused for a while (a third of the time)
        } else if (r == 11) {  // v3.58 SD358-003: one write fails alone (a confirm, a clean-up, a pending entry, a retry)
            if ((seed >> 4) % 2) SimScript(&b, 1);  // (the one after the next: a mute's confirm)
            SimScript(&b, 0);
        } else if (r == 12) {
            SimScript(&b, 1), b.disk = 1;  // v3.58 SD358-003: one more whole write (a pending entry), then the disk goes bad
        }
        refused = b.notMuted;
        SimTick(&b);
        refusedNow = b.notMuted != nm0 || b.muteFailed != mf0;
        for (i = 0; i < 3; ++i) {
            const SimSess *const s = &b.s[i];
            const int want = SndBgWantMute(b.bg, !b.front);
            if (want && !s->muted && !(s->held && s->touched == SND_TOUCH_UNMUTED) && !b.disk && !refusedNow) ++bad;  // plays behind under Mute
            if (!want && s->held) ++bad;                             // Refined holds a mute in front / under Play
            if (s->user && !s->muted && !s->staleUser) ++bad;        // the user's mute undone
            if (s->held && s->touched == SND_TOUCH_NONE && !s->named) ++bad;  // SD358-002: muted by Refined, not on disk
            if (s->held && s->touched == SND_TOUCH_NONE && !b.dirty && s->named != SND_MARK_MUTED) ++bad;  // SD358-003: written = confirmed
            if (s->held && !s->conf) ++bad;                          // SD358-003: held = its SetMute worked
        }
        if (b.sitesPlay != 1) ++bad;  // with a session the sites carry the global focus in both modes
    }
    pendLeft = b.pendLeft, confFail = b.confFail, muteFailed = b.muteFailed;
    CHECK(bad == 0 && silentStart == 0 && b.undone == 0 && kills > 1000 && refused > 0 && muteFailed > 0 && confFail > 0,
          "random walk with %d kills, %d refused mutes, %d failed SetMutes, %d lost confirms: %d broken state(s), %d silent start(s), "
          "%d user mute(s) undone at a start", kills, refused, muteFailed, confFail, bad, silentStart, b.undone);
    printf("  (SD358-003 walk: %d pending entries found muted at a start, %d of them Refined's own: unmuted by the user)\n", pendLeft,
           recovered);
}

int main(void)
{
    TestSd356();  // v3.56 SD356
    TestZero();       // v3.58 SD358
    TestBgSession();  // v3.58 SD358  // v3.58 SD358-001: + identities, kills, the Volume Mixer  // v3.58 SD358-002: + disk failures  // v3.58 SD358-003: + pending / confirmed entries
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
