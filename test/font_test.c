// v3.43 FN343: native test of src\font_sim.h (test\build_font_test.cmd): the .sh4font parser, the CRC the loader compares,
// the cache-cell geometry, the code -> glyph rule, the sheet and stock cell fills, the ini value. When the generated
// sheets (fonts\*.sh4font) and the game's data\fontdata_kr.bin are there, also that each sheet parses and was made for the
// installed fonts' metrics (the tool and the loader agree on the CRC).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/font_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void Wr32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

// A sheet laid out as tools\font\sh4font_make.py writes it.
static unsigned char *MakeSheet(void)
{
    unsigned char *b = (unsigned char *)calloc(1, FN_FILE_BYTES);
    size_t p = FN_HDR;
    int f;
    memcpy(b, "SH4F", 4);
    Wr32(b + 4, 1);
    Wr32(b + 8, 4);
    Wr32(b + 12, 2);
    Wr32(b + 16, 104);
    Wr32(b + 20, 136);
    Wr32(b + 24, 16);
    Wr32(b + 28, 14);
    memcpy(b + 32, "Test Font", 9);
    for (f = 0; f < 2; ++f) {
        Wr32(b + p, (unsigned int)f);
        Wr32(b + p + 4, f ? 20 : 24);
        Wr32(b + p + 8, f ? 26 : 32);
        Wr32(b + p + 12, 0x12345678u + (unsigned int)f);
        b[p + 16 + 0x21] = 1;
        p += FN_FONTHDR + FN_SHEET_BYTES;
    }
    return b;
}

static void SetNib(unsigned char *pix, int x, int y, int v)
{
    const size_t j = (size_t)y * FN_SHEET_W + (size_t)x;
    if (j & 1) pix[j >> 1] = (unsigned char)((pix[j >> 1] & 0x0F) | (v << 4));
    else pix[j >> 1] = (unsigned char)((pix[j >> 1] & 0xF0) | v);
}

static void TestParse(void)
{
    unsigned char *b = MakeSheet();
    FnSheet s;
    const char *why;
    CHECK(FN_FONTHDR == 240, "font header %d", FN_FONTHDR);
    CHECK(FN_FILE_BYTES == 3168800, "file bytes %lu (the tool's output)", (unsigned long)FN_FILE_BYTES);
    why = FnSheetParse(b, FN_FILE_BYTES, &s);
    CHECK(!why, "good sheet: %s", why ? why : "");
    CHECK(!strcmp(s.name, "Test Font"), "name %s", s.name);
    CHECK(s.font[0].present && s.font[1].present, "both fonts");
    CHECK(s.font[0].glyphW == 24 && s.font[1].glyphH == 26, "glyph sizes");
    CHECK(s.font[1].crc == 0x12345679u, "crc %08X", s.font[1].crc);
    CHECK(FnHaveCount(&s.font[0]) == 1 && s.font[0].have[0x21], "have");
    CHECK(s.font[1].pix == b + FN_HDR + FN_FONTHDR + FN_SHEET_BYTES + FN_FONTHDR, "font 1 pixels");
    CHECK(FnSheetParse(b, FN_FILE_BYTES - 1, &s) != NULL, "short file refused");
    CHECK(FnSheetParse(b, 10, &s) != NULL, "header-less file refused");
    CHECK(FnSheetParse(NULL, 0, &s) != NULL, "no file refused");
    b[0] = 'X';
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "magic");
    b[0] = 'S';
    Wr32(b + 4, 2);
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "version");
    Wr32(b + 4, 1);
    Wr32(b + 8, 2);
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "scale");
    Wr32(b + 8, 4);
    Wr32(b + FN_HDR + FN_FONTHDR + FN_SHEET_BYTES, 0);  // font 0 twice
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "duplicate font");
    Wr32(b + FN_HDR + FN_FONTHDR + FN_SHEET_BYTES, 7);
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "font index range");
    Wr32(b + FN_HDR + FN_FONTHDR + FN_SHEET_BYTES, 1);
    Wr32(b + FN_HDR + 8, 40);
    CHECK(FnSheetParse(b, FN_FILE_BYTES, &s) != NULL, "glyph height range");
    Wr32(b + FN_HDR + 8, 32);
    b[32] = 0x07;  // a control character in the name is logged as '?'
    CHECK(!FnSheetParse(b, FN_FILE_BYTES, &s) && s.name[0] == '?', "name sanitised");
    free(b);
}

static void TestCrc(void)
{
    unsigned char font[0x1D0];
    int i;
    CHECK(FnCrc32((const unsigned char *)"123456789", 9, 0) == 0xCBF43926u, "CRC32 check value");
    memset(font, 0xEE, sizeof font);
    for (i = 0; i < 0xE0; ++i) {
        font[0x10 + i] = (unsigned char)i;
        font[0xF0 + i] = (unsigned char)(i * 7);
    }
    // python: zlib.crc32(bytes(range(224)) + bytes((i * 7) & 255 for i in range(224)))
    CHECK(FnMetricsCrc(font) == 0x179909FDu, "metrics crc %08X (the tool's zlib.crc32 of widths + kerning)", FnMetricsCrc(font));
}

static void TestCells(void)
{
    int x, y, font, glyph;
    CHECK(FnCellXY(0, &x, &y) && x == 0 && y == 0, "cell 0");
    CHECK(FnCellXY(18, &x, &y) && x == 468 && y == 0, "cell 18");
    CHECK(FnCellXY(19, &x, &y) && x == 0 && y == 34, "cell 19");
    CHECK(FnCellXY(284, &x, &y) && x == 468 && y == 476, "cell 284");
    CHECK(x * FN_SCALE + FN_HCW <= FN_HD && y * FN_SCALE + FN_HCH <= FN_HD, "the last HD cell fits 2048");
    CHECK(!FnCellXY(285, &x, &y) && !FnCellXY(-1, &x, &y), "out of range cells");
    CHECK(FnCodeGlyph(0x21, &font, &glyph) && font == 0 && glyph == 0x21, "font 0 'A'");
    CHECK(FnCodeGlyph(0x2C80 + 0x41, &font, &glyph) && font == 1 && glyph == 0x41, "font 1 'a'");
    CHECK(!FnCodeGlyph(0xE0, &font, &glyph), "a kanji code of font 0");
    CHECK(!FnCodeGlyph(0x2C80 * 2 + 5, &font, &glyph), "font 2");
    CHECK(!FnCodeGlyph(0x59E0, &font, &glyph), "a remapped code (0x4000+)");
}

static void TestFills(void)
{
    unsigned char *b = MakeSheet();
    FnSheet s;
    static unsigned int dst[FN_HCH * 120];
    static unsigned char pixels[FN_PAGE * FN_PAGE];
    unsigned int pal[16];
    int x, y, ok;
    const int gx = 0x21 % 16 * FN_HCW, gy = 0x21 / 16 * FN_HCH;
    FnSheetParse(b, FN_FILE_BYTES, &s);
    SetNib((unsigned char *)s.font[0].pix, gx, gy, 15);
    SetNib((unsigned char *)s.font[0].pix, gx + 1, gy, 1);
    SetNib((unsigned char *)s.font[0].pix, gx + 103, gy + 135, 7);
    SetNib((unsigned char *)s.font[0].pix, gx + 104, gy, 9);  // the next glyph's cell: never read
    FnFillSheetCell(dst, 120, &s.font[0], 0x21);
    CHECK(dst[0] == 0xFFFFFFFFu, "full texel %08X", dst[0]);
    CHECK(dst[1] == 0x11FFFFFFu, "1/15 texel %08X", dst[1]);
    CHECK(dst[135 * 120 + 103] == 0x77FFFFFFu, "last texel %08X", dst[135 * 120 + 103]);
    CHECK(dst[2] == FN_WHITE0 && dst[50 * 120 + 50] == FN_WHITE0, "clear texels are white at alpha 0");
    ok = 1;
    for (y = 0; y < FN_HCH; ++y)
        for (x = FN_HCW; x < 120; ++x) ok &= dst[y * 120 + x] == 0;
    CHECK(ok, "nothing written right of the cell");
    FnFillBlankCell(dst, 120);
    CHECK(dst[0] == FN_WHITE0 && dst[135 * 120 + 103] == FN_WHITE0, "blank cell");
    // stock upscale: the stock palette (0x559EE0)
    memset(pal, 0, sizeof pal);
    for (x = 1; x < 7; ++x) pal[x] = ((unsigned int)(x * 0x20 + 0x3F) << 24) | 0xFFFFFFu;
    memset(pixels, 0, sizeof pixels);
    // cell 20 (x 26, y 34) clear; its neighbours full: the upscale must not read them
    for (y = 0; y < FN_PAGE; ++y)
        for (x = 0; x < FN_PAGE; ++x) {
            const int in = x >= 26 && x < 52 && y >= 34 && y < 68;
            pixels[y * FN_PAGE + x] = in ? 0 : 6;
        }
    FnFillStockCell(dst, 120, pixels, pal, 26, 34);
    ok = 1;
    for (y = 0; y < FN_HCH; ++y)
        for (x = 0; x < FN_HCW; ++x) ok &= dst[y * 120 + x] == 0;
    CHECK(ok, "the stock upscale stays inside its cell");
    for (y = 34; y < 68; ++y)
        for (x = 26; x < 52; ++x) pixels[y * FN_PAGE + x] = 6;
    FnFillStockCell(dst, 120, pixels, pal, 26, 34);
    ok = 1;
    for (y = 0; y < FN_HCH; ++y)
        for (x = 0; x < FN_HCW; ++x) ok &= dst[y * 120 + x] == 0xFFFFFFFFu;
    CHECK(ok, "a full cell upscales full");
    // one texel (5, 7) at value 6 in a clear cell: its 4 x 4 block's centre is near full, far texels clear
    for (y = 34; y < 68; ++y)
        for (x = 26; x < 52; ++x) pixels[y * FN_PAGE + x] = 0;
    pixels[(34 + 7) * FN_PAGE + 26 + 5] = 6;
    FnFillStockCell(dst, 120, pixels, pal, 26, 34);
    CHECK((dst[(7 * 4 + 2) * 120 + 5 * 4 + 2] >> 24) >= 0xC0, "the texel's centre %08X", dst[(7 * 4 + 2) * 120 + 5 * 4 + 2]);
    CHECK(dst[(7 * 4 + 1) * 120 + 5 * 4 + 1] == dst[(7 * 4 + 2) * 120 + 5 * 4 + 2], "the block's centre is symmetric");
    CHECK(dst[0] == 0 && dst[(7 * 4 + 8) * 120 + 5 * 4] == 0, "far texels clear");
    free(b);
}

static void TestIni(void)
{
    char st[32];
    CHECK(!FnIniName("Stock", st, sizeof st) && !st[0], "Stock");
    CHECK(!FnIniName("stock", st, sizeof st), "stock");
    CHECK(!FnIniName("", st, sizeof st) && !FnIniName("0", st, sizeof st) && !FnIniName("Off", st, sizeof st), "off values");
    CHECK(!FnIniName("Default", st, sizeof st) && !FnIniName("   ", st, sizeof st) && !FnIniName(NULL, st, sizeof st), "defaults");
    CHECK(FnIniName("EBGaramond", st, sizeof st) && !strcmp(st, "EBGaramond"), "EBGaramond");
    CHECK(FnIniName("EB Garamond", st, sizeof st) && !strcmp(st, "EBGaramond"), "spaces dropped: %s", st);
    CHECK(FnIniName("Crimson_Text-2", st, sizeof st) && !strcmp(st, "Crimson_Text-2"), "_ and -");
    CHECK(!FnIniName("..\\evil", st, sizeof st) && !st[0], "a path is no name");
    CHECK(!FnIniName("C:\\x", st, sizeof st) && !FnIniName("a.b", st, sizeof st) && !FnIniName("a/b", st, sizeof st), "paths");
    CHECK(FnIniName("AVeryLongFontNameThatGoesOnAndOnForeverAndEver", st, sizeof st) && strlen(st) == 31, "long names cut");
}

// v3.44 FN344: the Menu Font row's values, the ini words it writes back, and what a live switch does
static void TestRow(void)
{
    char st[32];
    int v;
    CHECK(FnRowOfIni("Stock", st, sizeof st) == 0 && !st[0], "row: Stock is 0");
    CHECK(FnRowOfIni("", st, sizeof st) == 0 && FnRowOfIni(NULL, st, sizeof st) == 0 && FnRowOfIni("Off", st, sizeof st) == 0 &&
              FnRowOfIni("..\\x", st, sizeof st) == 0, "row: empty / off / a path: Stock");
    CHECK(FnRowOfIni("EBGaramond", st, sizeof st) == 1 && !st[0], "row: EBGaramond is 1");
    CHECK(FnRowOfIni("EB Garamond", st, sizeof st) == 1 && FnRowOfIni("ebgaramond", st, sizeof st) == 1, "row: any case, spaces");
    CHECK(FnRowOfIni("CormorantGaramond", st, sizeof st) == 2 && FnRowOfIni("Cormorant Garamond", st, sizeof st) == 2, "row: Cormorant 2");
    CHECK(FnRowOfIni("CrimsonText", st, sizeof st) == 3 && FnRowOfIni("Crimson Text", st, sizeof st) == 3, "row: Crimson 3");
    CHECK(FnRowOfIni("My Font", st, sizeof st) == FN_ROW_CUSTOM && !strcmp(st, "MyFont"), "row: another sheet is Custom (%s)", st);
    CHECK(FnRowOfIni("My Font", st, 4) == FN_ROW_CUSTOM && !strcmp(st, "MyF"), "row: Custom's stem cut to the buffer (%s)", st);
    for (v = 0; v < FN_ROW_N; ++v) {
        char back[32];
        CHECK(FnRowOfIni(kFnRowStem[v], back, sizeof back) == v, "row: value %d's ini word %s reads back", v, kFnRowStem[v]);
        CHECK(FnRowOfIni(kFnRowLabel[v], back, sizeof back) == v, "row: value %d's label %s reads as it too", v, kFnRowLabel[v]);
    }
    CHECK(!strcmp(FnRowStem(0, "X"), "") && !strcmp(FnRowStem(1, NULL), "EBGaramond") && !strcmp(FnRowStem(3, ""), "CrimsonText"),
          "row: the stems values load");
    CHECK(!strcmp(FnRowStem(FN_ROW_CUSTOM, "MyFont"), "MyFont") && !strcmp(FnRowStem(FN_ROW_CUSTOM, ""), "") &&
              !strcmp(FnRowStem(FN_ROW_CUSTOM, NULL), "") && !strcmp(FnRowStem(-1, "X"), "") && !strcmp(FnRowStem(9, "X"), ""),
          "row: Custom loads the ini's sheet; out of range is Stock");
    CHECK(FnSwitchKind("", "") == FN_SW_NONE && FnSwitchKind(NULL, NULL) == FN_SW_NONE, "switch: Stock -> Stock: nothing");
    CHECK(FnSwitchKind("EBGaramond", "") == FN_SW_STOCK && FnSwitchKind("EBGaramond", NULL) == FN_SW_STOCK, "switch: a font -> Stock");
    CHECK(FnSwitchKind("", "EBGaramond") == FN_SW_LOAD && FnSwitchKind(NULL, "CrimsonText") == FN_SW_LOAD, "switch: Stock -> a font");
    CHECK(FnSwitchKind("EBGaramond", "CrimsonText") == FN_SW_LOAD, "switch: a font -> another");
    CHECK(FnSwitchKind("EBGaramond", "ebgaramond") == FN_SW_NONE, "switch: the font in use again: nothing");
    CHECK(FN_ROW_MAX == FN_ROW_N + 1 && FN_ROW_CUSTOM == FN_ROW_N, "row: Custom is the one extra value");
    CHECK(1 + 2 * FN_ROW_MAX + 2 <= 16, "row: label, 5 values, 5 help texts and 2 notes fit the MSG_FN0 block (16 ids)");
}

static unsigned char *ReadAll(const char *path, size_t *n)
{
    FILE *f;
    unsigned char *b;
    long sz;
    if (fopen_s(&f, path, "rb") || !f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc(sz > 0 ? (size_t)sz : 1);
    *n = fread(b, 1, (size_t)sz, f);
    fclose(f);
    return b;
}

// The generated sheets against the installed game's fonts (skipped when either is missing).
static void TestRealSheets(void)
{
    static const char *const kDirs[] = { "..\\fonts\\", "..\\..\\..\\..\\..\\fonts\\", "..\\..\\..\\..\\fonts\\" };  // real tree, sandbox; v3.43 MERGE: + staging\v343_feedback\merge343
    static const char *const kNames[] = { "EBGaramond", "CormorantGaramond", "CrimsonText" };
    static const char *const kTitles[] = { "EB Garamond", "Cormorant Garamond", "Crimson Text" };
    size_t kn = 0;
    unsigned char *kr = ReadAll(getenv("SH4_FONTDATA_KR") ? getenv("SH4_FONTDATA_KR") : "fontdata_kr.bin", &kn);
    unsigned int live[2] = { 0, 0 };
    int d, k, found = 0;
    if (kr && kn > 0x30380) {
        unsigned int o0 = (unsigned int)kr[4] | (unsigned int)kr[5] << 8 | (unsigned int)kr[6] << 16 | (unsigned int)kr[7] << 24;
        unsigned int o1 = (unsigned int)kr[8] | (unsigned int)kr[9] << 8 | (unsigned int)kr[10] << 16 | (unsigned int)kr[11] << 24;
        if (o0 + 0x1D0 < kn && o1 + 0x1D0 < kn) {
            live[0] = FnMetricsCrc(kr + o0);
            live[1] = FnMetricsCrc(kr + o1);
        }
    }
    for (d = 0; d < (int)(sizeof kDirs / sizeof kDirs[0]) && !found; ++d)  // v3.43 MERGE: every folder (was 2)
        for (k = 0; k < 3; ++k) {
            char path[260];
            size_t n = 0;
            unsigned char *b;
            FnSheet s;
            const char *why;
            sprintf_s(path, sizeof path, "%s%s.sh4font", kDirs[d], kNames[k]);
            b = ReadAll(path, &n);
            if (!b) continue;
            ++found;
            why = FnSheetParse(b, n, &s);
            CHECK(!why, "%s: %s", path, why ? why : "");
            if (!why) {
                CHECK(!strcmp(s.name, kTitles[k]), "%s: name %s", path, s.name);
                CHECK(FnHaveCount(&s.font[0]) >= 200 && FnHaveCount(&s.font[1]) >= 200, "%s: glyphs %d / %d", path,
                      FnHaveCount(&s.font[0]), FnHaveCount(&s.font[1]));
                if (live[0]) CHECK(s.font[0].crc == live[0] && s.font[1].crc == live[1],
                                   "%s: made for fonts %08X / %08X, the game has %08X / %08X", path, s.font[0].crc,
                                   s.font[1].crc, live[0], live[1]);
            }
            free(b);
        }
    printf("real sheets: %d checked%s\n", found, live[0] ? " against the game's fontdata_kr.bin" : " (no game font file)");
    free(kr);
}

int main(void)
{
    TestParse();
    TestCrc();
    TestCells();
    TestFills();
    TestIni();
    TestRow();  // v3.44 FN344
    TestRealSheets();
    printf("font_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
