// v3.53 OL2: native test of src\look_sim.h (test\build_look_test.cmd): byte-mode decode / encode, a page's own name in
// its title (taken whole: v3.53 MERGE), the "Parent > Page" titles (at most 28 characters), the glyph count, the grey
// rows' reason-first description (one line of at most 52: v3.53 MERGE),
// the page strip's fit, and the plan's text rules on the reasons (PLAN section 3: one line of at most 52 characters,
// worded with the plan's row names; the lint of staging/options_layout/tools/gen_layout.py, done here for these texts).
// v3.55 OL56: + the strip's mouse click (LkStripHit: the name under the cursor; LkStripRoute: slide, fade or nothing).
#include <stdio.h>
#include <string.h>
#include "../src/look_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestCodec(void)
{
    unsigned char b[64];
    char a[64];
    const int n = LkEncode(b, sizeof b, "Option 1 (Video&Sound)");
    CHECK(n == 2 + 22 + 2 && b[0] == 0x00 && b[1] == 0x80 && b[n - 2] == 0xFF && b[n - 1] == 0xFF, "encode: %d bytes", n);
    CHECK(b[2] == 0x100 - 'O' && b[8] == 0x00, "encode: letters 0x100 - c, space 00");
    CHECK(LkDecode(b, a, sizeof a) && !strcmp(a, "Option 1 (Video&Sound)"), "decode: \"%s\"", a);
    {   // "16:10" from the skill's example: 00 80 CF CA C6 CF D0 FF FF
        static const unsigned char k[] = { 0x00, 0x80, 0xCF, 0xCA, 0xC6, 0xCF, 0xD0, 0xFF, 0xFF };
        CHECK(LkDecode(k, a, sizeof a) && !strcmp(a, "16:10"), "decode the skill's 16:10: \"%s\"", a);
    }
    {   // not plain: a newline (FF FD), a u16 character, word mode, NULL, a small buffer
        static const unsigned char nl[] = { 0x00, 0x80, 0xBF, 0xFF, 0xFD, 0xBE, 0xFF, 0xFF };
        static const unsigned char u16[] = { 0x00, 0x80, 0xE0, 0x41, 0x00, 0xFF, 0xFF };
        static const unsigned char word[] = { 0x41, 0x00, 0xFF, 0xFF };
        CHECK(!LkDecode(nl, a, sizeof a) && a[0] == 0, "decode: a control is not plain");
        CHECK(!LkDecode(u16, a, sizeof a), "decode: a u16 character is not plain");
        CHECK(!LkDecode(word, a, sizeof a), "decode: word mode is not plain");
        CHECK(!LkDecode(NULL, a, sizeof a), "decode: NULL");
        CHECK(!LkDecode(b, a, 8) && a[0] == 0, "decode: no room");
    }
    CHECK(LkEncode(b, 3, "x") == 0, "encode: no room");
    CHECK(LkEncode(b, 6, "abcdef") == 6 && b[4] == 0xFF && b[5] == 0xFF, "encode: cut, still ended");
}

static void TestLeaf(void)  // v3.53 MERGE: a title is taken whole (w2 OL1's plain names); nothing is parsed or cut
{
    static const char *const kIn[][2] = {
        { OLW_T_DISPLAY, "Display" }, { OLW_T_CONTROLS, "Controls" }, { OLW_T_INTERFACE, "Interface" },
        { OLW_T_PRESENTATION, "Presentation" }, { OLW_T_GAMEPLAY, "Gameplay" }, { OLW_T_PAUSE, "Pause Menu" },
        { OLW_T_SUBTITLES, "Subtitles" }, { OLW_T_EFFECTS, "Effects" }, { OLW_T_SHADOWS, "Shadows" },
        { OLW_T_ROOM302, "Room 302" }, { OLW_T_PLAYER, "Player" }, { OLW_T_ENEMIES, "Enemies" }, { OLW_T_POWERS, "Powers" },
        { OLW_T_RANDO, "Randomizer" }, { OLW_T_SOUND, "Sound" }, { " Sound ", "Sound" },
        { "Option 4 (Presentation)", "Option 4 (Presentation)" }, { "Gameplay: Player", "Gameplay: Player" },
        { "A > B > C", "A > B > C" }, { "Night Mono: (x)", "Night Mono: (x)" },
    };
    int i;
    for (i = 0; i < (int)(sizeof kIn / sizeof kIn[0]); ++i) {
        char o[LK_TXT];
        LkLeaf(kIn[i][0], o, sizeof o);
        CHECK(!strcmp(o, kIn[i][1]), "leaf of \"%s\": \"%s\", want \"%s\"", kIn[i][0], o, kIn[i][1]);
    }
    {   // every hidden page's crumb with w2's names (look.inc LkParentOf: LP_SUB / LP_FX / LP_SHD -> Interface, ...)
        static const char *const kCrumb[][3] = {
            { OLW_T_INTERFACE, OLW_T_SUBTITLES, "Interface > Subtitles" }, { OLW_T_INTERFACE, OLW_T_EFFECTS, "Interface > Effects" },
            { OLW_T_INTERFACE, OLW_T_SHADOWS, "Interface > Shadows" }, { OLW_T_PRESENTATION, OLW_T_ROOM302, "Presentation > Room 302" },
            { OLW_T_GAMEPLAY, OLW_T_PLAYER, "Gameplay > Player" }, { OLW_T_GAMEPLAY, OLW_T_ENEMIES, "Gameplay > Enemies" },
            { OLW_T_GAMEPLAY, OLW_T_POWERS, "Gameplay > Powers" }, { OLW_T_GAMEPLAY, OLW_T_RANDO, "Gameplay > Randomizer" },
            { OLW_T_DISPLAY, OLW_T_SOUND, "Display > Sound" },
        };
        for (i = 0; i < (int)(sizeof kCrumb / sizeof kCrumb[0]); ++i) {
            char par[LK_TXT], leaf[LK_TXT], o[LK_TXT + 8];
            LkLeaf(kCrumb[i][0], par, sizeof par);
            LkLeaf(kCrumb[i][1], leaf, sizeof leaf);
            CHECK(LkCrumb(par, leaf, LK_TITLE_MAX, o, sizeof o) && !strcmp(o, kCrumb[i][2]), "crumb of w2's names: \"%s\"", o);
        }
    }
}

static void TestCrumb(void)
{
    char o[LK_TXT + 8];
    CHECK(LkCrumb("Gameplay", "Enemies", LK_TITLE_MAX, o, sizeof o) && !strcmp(o, "Gameplay > Enemies"), "crumb: \"%s\"", o);
    CHECK(LkCrumb("Interface", "Subtitles", LK_TITLE_MAX, o, sizeof o) && !strcmp(o, "Interface > Subtitles"), "crumb: \"%s\"", o);
    CHECK(LkCrumb("Presentation", "Screen Effect", LK_TITLE_MAX, o, sizeof o) && strlen(o) == 28, "crumb: 28 characters fit");
    CHECK(!LkCrumb("Presentation", "Screen Effectss", LK_TITLE_MAX, o, sizeof o) && !strcmp(o, "Screen Effectss"),
          "crumb: 29 characters: the page alone (\"%s\")", o);
    CHECK(!LkCrumb("", "Sound", LK_TITLE_MAX, o, sizeof o) && !strcmp(o, "Sound"), "crumb: no parent");
    CHECK(!LkCrumb("Gameplay", "Enemies", LK_TITLE_MAX, o, 12) && !strcmp(o, "Enemies"), "crumb: no room in the buffer");
}

static void TestGlyphs(void)
{
    unsigned char b[128];
    LkEncode(b, sizeof b, "Needs Film Grain on.");
    CHECK(LkGlyphs(b) == 17, "glyphs: %d of \"Needs Film Grain on.\" (no spaces)", LkGlyphs(b));
    {
        static const unsigned char k[] = { 0x00, 0x80, 0xBF, 0x00, 0xE0, 0x41, 0x00, 0xF0, 0x21, 0xFF, 0xFD, 0xFC, 0x04, 0xBE, 0xFF, 0xFF };
        CHECK(LkGlyphs(k) == 4, "glyphs: a letter, a u16, an icon, a letter (no space, newline or move): %d", LkGlyphs(k));
    }
    {
        static const unsigned char w[] = { 0x41, 0x00, 0x00, 0x00, 0x42, 0x00, 0xFF, 0xFF };
        CHECK(LkGlyphs(w) == 2, "glyphs: word mode %d", LkGlyphs(w));
    }
    CHECK(LkGlyphs(NULL) == 0, "glyphs: NULL");
}

// v3.53 MERGE: the grey line is ONE line of at most 52 characters: the reason, then the row's own line (whole, else its first
// sentences that fit), else the reason alone; checked on every dependent row's w2 OL1 line
static void TestCompose(void)
{
    char o[256];
    unsigned char e[128];
    int n, i, j;
    n = LkComposeLine("Needs Echo Doubles on.", OLW_POWERS_ECHO_COUNT_D, LK_DESC_MAX, o, sizeof o);
    CHECK(n == (int)strlen(o) && !strcmp(o, "Needs Echo Doubles on. How many shadow doubles."), "compose whole: \"%s\"", o);
    n = LkComposeLine(kLkReason[LKR_GRAIN], OLW_GRAIN_STRENGTH_D, LK_DESC_MAX, o, sizeof o);
    CHECK(!strcmp(o, "Needs Film Grain on. How much grain."), "compose: the first sentence that fits: \"%s\"", o);
    n = LkComposeLine(kLkReason[LKR_SHADOW], OLW_SHADOWS_DARKNESS_D, LK_DESC_MAX, o, sizeof o);
    CHECK(!strcmp(o, "Needs Shadows on. How dark shadows are."), "compose: Darkness: \"%s\"", o);
    n = LkComposeLine(kLkReason[LKR_WIDTH], OLW_SOUND_STEREO_WIDTH_D, LK_DESC_MAX, o, sizeof o);
    CHECK(!strcmp(o, kLkReason[LKR_WIDTH]), "compose: no sentence fits: the reason alone: \"%s\"", o);
    n = LkComposeLine(kLkReason[LKR_DOF], NULL, LK_DESC_MAX, o, sizeof o);
    CHECK(n == 24 && !strcmp(o, "Needs Depth of Field on."), "compose: no own line: the reason (%d)", n);
    n = LkComposeLine(kLkReason[LKR_DOF], "  ", LK_DESC_MAX, o, sizeof o);
    CHECK(!strcmp(o, "Needs Depth of Field on."), "compose: a blank own line: the reason");
    n = LkComposeLine("Needs X on.", "Off this session (see SH4Refined.log).", 30, o, sizeof o);
    CHECK(!strcmp(o, "Needs X on."), "compose: a dot inside a word is no sentence end: \"%s\"", o);
    {   // exactly 52 fits, 53 does not
        static const char r[] = "Needs A on.";  // 11 + 1 + 40 = 52
        n = LkComposeLine(r, "Forty characters of a line, all of them.", LK_DESC_MAX, o, sizeof o);
        CHECK(n == 52 && (int)strlen(o) == 52, "compose: 52 characters fit (%d)", n);
        n = LkComposeLine(r, "Forty-one characters of a line, all here.", LK_DESC_MAX, o, sizeof o);
        CHECK(n == 11 && !strcmp(o, r), "compose: 53 characters: the reason alone (%d)", n);
    }
    n = LkComposeLine(kLkReason[LKR_GRAIN], OLW_GRAIN_STRENGTH_D, LK_DESC_MAX, o, 12);
    CHECK(n == 11 && (int)strlen(o) == 11, "compose: a small buffer cuts the reason, never overruns (%d)", n);
    CHECK(LkComposeLine("x", "y", 52, o, 0) == 0, "compose: no buffer");
    {   // every dependent row's w2 lines (look.inc kLkOwnIn order) under each reason: one line of at most 52, the reason first
        static const char *const kOwn[] = {
            OLW_GRAIN_STRENGTH_D, OLW_GRAIN_GRAIN_TYPE_D, OLW_GRAIN_GRAIN_COLOUR_D, OLW_EFFECTS_BLUR_STRENGTH_D, OLW_OFF_SESSION,
            OLW_POWERS_ECHO_COUNT_D, OLW_SOUND_STEREO_WIDTH_D, OLW_SOUND_STEREO_WIDTH_VD_100, OLW_SND_WIDTH_STOCK, OLW_NA_SESSION,
            OLW_SHADOWS_EDGE_VD_STOCK, OLW_SHADOWS_EDGE_VD_SOFT, OLW_SHADOWS_DARKNESS_D, OLW_SHD_DARK_NOPREVIEW,
            OLW_STEERING_WALL_MARGIN_D,
        };
        for (i = 0; i < LKR_N; ++i)
            for (j = 0; j < (int)(sizeof kOwn / sizeof kOwn[0]); ++j) {
                char a[256];
                n = LkComposeLine(kLkReason[i], kOwn[j], LK_DESC_MAX, o, sizeof o);
                CHECK(n == (int)strlen(o) && n <= LK_DESC_MAX && !strncmp(o, kLkReason[i], strlen(kLkReason[i])),
                      "compose %d x %d: \"%s\" (%d)", i, j, o, n);
                CHECK(LkEncode(e, sizeof e, o) == n + 4 && LkDecode(e, a, sizeof a) && !strcmp(a, o), "compose %d x %d: encodes", i, j);
            }
    }
}

static void TestStrip(void)
{
    // R1's six (Display .. Extra Menu) at about 8 px a character, and today's widest case
    const float w6[6] = { 56, 64, 72, 96, 64, 80 }, wide[6] = { 92, 60, 52, 100, 68, 86 };
    const int a6[6] = { 7, 8, 9, 12, 8, 10 }, aw[6] = { 11, 7, 6, 12, 8, 10 };
    float gap, sp, total;
    CHECK(LkStripFit(w6, a6, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && gap > 12.7f && gap < 12.9f && sp == 2.5f &&
          total == 496.0f, "strip: R1's names, the gap narrower (%.1f, %.2f, %.1f)", gap, sp, total);
    {
        const float fin[6] = { 56, 64, 72, 40, 56, 64 };  // the plan's six (Display .. Gameplay): the widest gap
        CHECK(LkStripFit(fin, a6, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && gap == 22.0f && total == 352.0f + 110.0f,
              "strip: the plan's six at the widest gap (%.1f, %.1f)", gap, total);
    }
    {
        const float big[6] = { 76, 76, 76, 76, 76, 76 };  // 456 + 5 x 8 = 496
        CHECK(LkStripFit(big, a6, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && gap == 8.0f && sp == 2.5f &&
              total == 496.0f, "strip: a narrower gap (%.1f)", gap);
    }
    {
        const float tight[6] = { 86, 86, 86, 86, 86, 86 };  // 516 + 30: spacing
        const int ok = LkStripFit(tight, a6, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total);
        CHECK(ok && gap == 6.0f && sp < 2.5f && sp >= 1.0f && total <= 496.01f, "strip: a tighter spacing (%.2f, %.1f)", sp, total);
        CHECK(LkWidthAt(86.0f, 10, 2.5f, sp) < 86.0f, "strip: a name narrower at the tighter spacing");
    }
    {
        const float huge[6] = { 200, 200, 200, 200, 200, 200 };
        CHECK(!LkStripFit(huge, a6, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && sp == 1.0f && total > 496.0f,
              "strip: wider than the canvas at spacing 1.0: 0 (%.1f)", total);
    }
    CHECK(LkStripFit(wide, aw, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total), "strip: today's names fit (%.1f)", total);
    CHECK(LkStripFit(w6, a6, 1, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && total == 22.0f * 0 + 56.0f, "strip: one name");
    CHECK(LkStripFit(w6, a6, 0, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total) && total == 0.0f, "strip: none");
}

// v3.55 OL56 (R6): the page strip's click
static void TestStripClick(void)
{
    // the plan's six laid out as LkStripDraw does (centred, gap from LkStripFit)
    const float w[6] = { 56, 64, 72, 40, 56, 64 };
    const int a[6] = { 7, 8, 9, 5, 7, 8 };
    float x[6], gap, sp, total, cx, h = 22.0f * (448.0f / 480.0f) + 3.0f;
    int i, cur, t, n6[6] = { 0 }, slides = 0, fades = 0;
    CHECK(LkStripFit(w, a, 6, 2.5f, 496.0f, 6.0f, 22.0f, &gap, &sp, &total), "click: the six fit");
    x[0] = 256.0f - total * 0.5f;
    for (i = 1; i < 6; ++i) x[i] = x[i - 1] + w[i - 1] + gap;
    CHECK(gap >= 2.0f * LK_HIT_PAD, "click: no two boxes meet (gap %.1f)", gap);
    for (i = 0; i < 6; ++i) {
        CHECK(LkStripHit(x, w, 6, 68.0f, h, x[i] + w[i] * 0.5f, 78.0f) == i, "click: the middle of name %d", i);
        CHECK(LkStripHit(x, w, 6, 68.0f, h, x[i] - LK_HIT_PAD, 68.0f) == i, "click: name %d's left edge, the strip's top", i);
        CHECK(LkStripHit(x, w, 6, 68.0f, h, x[i] + w[i] + LK_HIT_PAD - 0.01f, 68.0f + h - 0.01f) == i, "click: name %d's right edge, bottom", i);
        CHECK(LkStripHit(x, w, 6, 68.0f, h, x[i] + w[i] * 0.5f, 67.9f) == -1, "click: above the strip (the page arrows end at 67.2)");
        CHECK(LkStripHit(x, w, 6, 68.0f, h, x[i] + w[i] * 0.5f, 68.0f + h) == -1, "click: below the strip (row 0 at 96)");
        CHECK(68.0f + h <= 96.0f, "click: the strip's box ends above row 0 (%.1f)", 68.0f + h);
    }
    for (cx = 0.0f; cx < 512.0f; cx += 0.5f) {  // every x: at most one name, and in a gap none
        const int k = LkStripHit(x, w, 6, 68.0f, h, cx, 80.0f);
        if (k >= 0) {
            ++n6[k];
            CHECK(cx >= x[k] - LK_HIT_PAD && cx < x[k] + w[k] + LK_HIT_PAD, "click: x %.1f is in name %d's box", cx, k);
        }
    }
    for (i = 0; i < 6; ++i) CHECK(n6[i] > 0, "click: name %d can be hit", i);
    CHECK(LkStripHit(NULL, w, 6, 68.0f, h, 100.0f, 80.0f) == -1 && LkStripHit(x, w, 0, 68.0f, h, x[0] + 1.0f, 80.0f) == -1, "click: no boxes");
    // the routes: from each of the 22 ids to each top page
    for (cur = 0; cur < 22; ++cur) {
        for (t = -1; t <= 6; ++t) {
            const int r1 = LkStripRoute(cur, t, 6, 1), r0 = LkStripRoute(cur, t, 6, 0);
            if (t < 0 || t >= 6 || t == cur) {
                CHECK(r1 == LK_GO_NONE && r0 == LK_GO_NONE, "route %d -> %d: none", cur, t);
                continue;
            }
            if (cur < 6 && t % 3 != cur % 3) {
                CHECK(r1 == LK_GO_SLIDE && r0 == LK_GO_SLIDE, "route %d -> %d: the slide (slot %d -> %d)", cur, t, cur % 3, t % 3);
                ++slides;
            } else {
                CHECK(r1 == LK_GO_FADE && r0 == LK_GO_NONE, "route %d -> %d: the fade (or nothing without it)", cur, t);
                ++fades;
            }
            if (r1 == LK_GO_SLIDE) CHECK(t % 3 != cur % 3, "route %d -> %d: a slide never refills the slot on screen", cur, t);
        }
    }
    CHECK(LkStripRoute(0, 3, 6, 1) == LK_GO_FADE && LkStripRoute(1, 4, 6, 1) == LK_GO_FADE && LkStripRoute(2, 5, 6, 1) == LK_GO_FADE,
          "route: Display -> Sound, Controls -> Effects, Interface -> Gameplay share a slot: the fade");
    CHECK(LkStripRoute(0, 1, 6, 1) == LK_GO_SLIDE && LkStripRoute(0, 5, 6, 1) == LK_GO_SLIDE && LkStripRoute(3, 2, 6, 1) == LK_GO_SLIDE,
          "route: the others slide");
    CHECK(LkStripRoute(21, 1, 6, 1) == LK_GO_FADE && LkStripRoute(21, 0, 6, 1) == LK_GO_FADE, "route: from Steering (hidden): the fade");
    CHECK(LkStripRoute(-1, 0, 6, 1) == LK_GO_NONE, "route: no page shown");
    printf("strip click: %d slide and %d fade routes\n", slides, fades);
}

// PLAN section 3's text rules for the reasons (this worker's only new texts), and their row names
static void TestReasonRules(void)
{
    static const char *const kNames[LKR_N] = { OLW_L_FILM_GRAIN, OLW_EFFECTS_DEPTH_OF_FIELD_L, OLW_POWERS_ECHO_DOUBLES_L,
                                               OLW_SOUND_SOUND_MODE_V_MONO, OLW_L_SHADOWS, OLW_CAMERA_CAMERA_V_SHOULDER,
                                               OLW_STEERING_WALL_ZOOM_L };  // v3.53 MERGE: w2 OL1's row names
    int i;
    for (i = 0; i < LKR_N; ++i) {
        const char *s = kLkReason[i];
        const int n = (int)strlen(s);
        CHECK(n > 0 && n <= LK_DESC_MAX, "reason %d: %d characters (at most %d)", i, n, LK_DESC_MAX);
        CHECK(!strncmp(s, "Needs ", 6) && s[n - 1] == '.', "reason %d starts with the reason (\"Needs ...\") and ends a sentence: \"%s\"", i, s);
        CHECK(strstr(s, kNames[i]) != NULL, "reason %d names the plan's row \"%s\": \"%s\"", i, kNames[i], s);
        CHECK(!strstr(s, "Option "), "reason %d: never \"Option N\" in new text", i);
    }
}

int main(void)
{
    TestCodec();
    TestLeaf();
    TestCrumb();
    TestGlyphs();
    TestCompose();
    TestStrip();
    TestStripClick();  // v3.55 OL56
    TestReasonRules();
    printf("look_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
