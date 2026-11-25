// v3.43 FN343: the HD menu font's pure logic (src\font.inc; native test test\font_test.c, test\build_font_test.cmd).
//
// The game's text layer (SH4Decomp src\text\text_556770.c) keeps ONE glyph cache page: 512 x 512 texels, 19 x 15 cells
// of 26 x 34 texels (cell c at x = c % 19 * 26, y = c / 19 * 34), filled on demand by TextDecodeGlyph 0x556BC0 from the
// font files at 4 bits a texel; 0x5567F0 (TextRewritePageTexture) unpacks the page into gText.pixels (a byte a texel)
// and writes it through the 16-colour palette into the A8R8G8B8 texture gText.texture (0x12A1D7C) whenever the page
// changed. Every glyph quad samples that texture with u, v / 512 (GfxFlush2dBatch is passed 512.0f, 512.0f), so a
// 2048 x 2048 texture with each cell drawn at 4x in the same place shows the same layout four times sharper.
// A cell's glyph: gText.cache[c].code (0x125BBD8 + 6 c + 4) = font * 0x2C80 + glyph; glyph i < 0xE0 = Latin-1 character
// i + 0x20 (fonts 0 and 1: 24 x 32 / 20 x 26 layout units).
//
// .sh4font (tools\font\sh4font_make.py): a 64-byte header ('SH4F', version 1, scale 4, nfonts 2, cell 104 x 136, 16 x 14
// cells, name[32]) and per font: index, glyph w / h, CRC32 of the font's width + kerning tables, have[0xE0], padding to 16,
// then the sheet at 4 bits a texel (low nibble = even x). Glyph i sits in sheet cell (i % 16, i / 16).
#ifndef FONT_SIM_H
#define FONT_SIM_H

#include <stddef.h>
#include <string.h>

#define FN_SCALE 4
#define FN_PAGE 512                          // the stock page (texels)
#define FN_HD (FN_PAGE * FN_SCALE)           // 2048: the HD page
#define FN_CELLS 285                         // TEXT_CACHE_MAX (19 x 15)
#define FN_PCOLS 19
#define FN_CW 26                             // a cache cell, stock texels
#define FN_CH 34
#define FN_HCW (FN_CW * FN_SCALE)            // 104
#define FN_HCH (FN_CH * FN_SCALE)            // 136
#define FN_NGLYPH 0xE0
#define FN_CODES 0x2C80                      // glyph codes a font (TextCacheGlyph: code += fontId * 0x2C80)
#define FN_NFONTS 2
#define FN_HDR 64
#define FN_FONTHDR (((16 + FN_NGLYPH) + 15) & ~15)
#define FN_SHEET_COLS 16
#define FN_SHEET_ROWS 14
#define FN_SHEET_W (FN_SHEET_COLS * FN_HCW)  // 1664
#define FN_SHEET_H (FN_SHEET_ROWS * FN_HCH)  // 1904
#define FN_SHEET_BYTES ((size_t)FN_SHEET_W * FN_SHEET_H / 2)
#define FN_FILE_BYTES (FN_HDR + FN_NFONTS * (FN_FONTHDR + FN_SHEET_BYTES))
#define FN_WHITE0 0x00FFFFFFu                // a clear HD texel: white at alpha 0 (no dark fringe when filtered)

typedef struct {
    int present;
    unsigned int crc;              // CRC32 of the stock font's widths + kerning the sheet was made for
    int glyphW, glyphH;
    const unsigned char *have;     // FN_NGLYPH flags
    const unsigned char *pix;      // FN_SHEET_BYTES, 4 bits a texel
} FnFont;

typedef struct {
    char name[32];
    FnFont font[FN_NFONTS];
} FnSheet;

static unsigned int FnRd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

// CRC32 (IEEE, reflected, as zlib.crc32): crc = 0 to start.
static unsigned int FnCrc32(const unsigned char *p, size_t n, unsigned int crc)
{
    size_t i;
    int k;
    crc = ~crc;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

// The fingerprint of a live font struct ([0x1261B98 + 4 f]): its 0xE0 widths (+0x10) then 0xE0 kerning bytes (+0xF0).
static unsigned int FnMetricsCrc(const unsigned char *font)
{
    return FnCrc32(font + 0xF0, FN_NGLYPH, FnCrc32(font + 0x10, FN_NGLYPH, 0));
}

// Checks a .sh4font image and points s at its parts. NULL = good, else why not.
static const char *FnSheetParse(const unsigned char *b, size_t n, FnSheet *s)
{
    int f;
    size_t p = FN_HDR;
    memset(s, 0, sizeof *s);
    if (!b || n < FN_HDR) return "shorter than its header";
    if (memcmp(b, "SH4F", 4)) return "not a .sh4font (magic)";
    if (FnRd32(b + 4) != 1) return "version is not 1";
    if (FnRd32(b + 8) != FN_SCALE) return "scale is not 4";
    if (FnRd32(b + 12) != FN_NFONTS) return "font count is not 2";
    if (FnRd32(b + 16) != FN_HCW || FnRd32(b + 20) != FN_HCH) return "cell is not 104 x 136";
    if (FnRd32(b + 24) != FN_SHEET_COLS || FnRd32(b + 28) != FN_SHEET_ROWS) return "sheet is not 16 x 14 cells";
    if (n != FN_FILE_BYTES) return "wrong file size";
    memcpy(s->name, b + 32, 31);
    s->name[31] = 0;
    {   // printable name only (it is logged)
        int k;
        for (k = 0; s->name[k]; ++k)
            if ((unsigned char)s->name[k] < 0x20 || (unsigned char)s->name[k] > 0x7E) s->name[k] = '?';
    }
    for (f = 0; f < FN_NFONTS; ++f) {
        const unsigned int idx = FnRd32(b + p);
        FnFont *t;
        if (idx >= FN_NFONTS) return "font index out of range";
        t = &s->font[idx];
        if (t->present) return "a font is in the file twice";
        t->present = 1;
        t->glyphW = (int)FnRd32(b + p + 4);
        t->glyphH = (int)FnRd32(b + p + 8);
        t->crc = FnRd32(b + p + 12);
        t->have = b + p + 16;
        t->pix = b + p + FN_FONTHDR;
        if (t->glyphW < 1 || t->glyphW > FN_CW || t->glyphH < 1 || t->glyphH > FN_CH) return "glyph size out of range";
        p += FN_FONTHDR + FN_SHEET_BYTES;
    }
    return NULL;
}

// The glyphs a font of the sheet holds.
static int FnHaveCount(const FnFont *t)
{
    int i, n = 0;
    if (!t->present) return 0;
    for (i = 0; i < FN_NGLYPH; ++i) n += t->have[i] != 0;
    return n;
}

// The top-left texel of cache cell c on the stock page (TextDecodeGlyph / 0x5570B0). 0 = no such cell.
static int FnCellXY(int c, int *x, int *y)
{
    if (c < 0 || c >= FN_CELLS) return 0;
    *x = c % FN_PCOLS * FN_CW;
    *y = c / FN_PCOLS * FN_CH;
    return 1;
}

// A cached code -> font and glyph; 1 = a glyph an .sh4font can hold (fonts 0 / 1, glyphs 0..0xDF).
static int FnCodeGlyph(unsigned int code, int *font, int *glyph)
{
    *font = (int)(code / FN_CODES);
    *glyph = (int)(code % FN_CODES);
    return *font < FN_NFONTS && *glyph < FN_NGLYPH;
}

// One HD cell from the sheet: white, alpha = the 4-bit coverage x 17. dst = the cell's top-left texel, pitch in texels.
static void FnFillSheetCell(unsigned int *dst, int pitch, const FnFont *t, int glyph)
{
    const int ox = glyph % FN_SHEET_COLS * FN_HCW, oy = glyph / FN_SHEET_COLS * FN_HCH;
    int x, y;
    for (y = 0; y < FN_HCH; ++y) {
        const size_t row = (size_t)(oy + y) * FN_SHEET_W + (size_t)ox;
        unsigned int *d = dst + (size_t)y * (size_t)pitch;
        for (x = 0; x < FN_HCW; ++x) {
            const size_t j = row + (size_t)x;
            const unsigned int a = (j & 1) ? (unsigned int)(t->pix[j >> 1] >> 4) : (unsigned int)(t->pix[j >> 1] & 15);
            d[x] = a ? ((a * 17u) << 24) | 0xFFFFFFu : FN_WHITE0;
        }
    }
}

static void FnFillBlankCell(unsigned int *dst, int pitch)
{
    int x, y;
    for (y = 0; y < FN_HCH; ++y)
        for (x = 0; x < FN_HCW; ++x) dst[(size_t)y * (size_t)pitch + (size_t)x] = FN_WHITE0;
}

// One HD cell from the stock page (a glyph the sheet does not hold: other fonts, Japanese, a metrics mismatch): the
// cell's texels (gText.pixels, a palette index a texel, 512 wide) through the palette, bilinear at 4x inside the cell.
static void FnFillStockCell(unsigned int *dst, int pitch, const unsigned char *pixels, const unsigned int *pal, int cx, int cy)
{
    int X, Y, ch;
    for (Y = 0; Y < FN_HCH; ++Y) {
        // source y = (Y + 0.5) / 4 - 0.5 in texels of the cell, in 1/8 steps: (2Y - 3) / 8
        int sy8 = 2 * Y - 3, y0, fy;
        if (sy8 < 0) sy8 = 0;
        if (sy8 > (FN_CH - 1) * 8) sy8 = (FN_CH - 1) * 8;
        y0 = sy8 >> 3;
        fy = sy8 & 7;
        for (X = 0; X < FN_HCW; ++X) {
            int sx8 = 2 * X - 3, x0, fx;
            unsigned int c00, c10, c01, c11, out = 0;
            const unsigned char *r0, *r1;
            if (sx8 < 0) sx8 = 0;
            if (sx8 > (FN_CW - 1) * 8) sx8 = (FN_CW - 1) * 8;
            x0 = sx8 >> 3;
            fx = sx8 & 7;
            r0 = pixels + (size_t)(cy + y0) * FN_PAGE + (size_t)(cx + x0);
            r1 = y0 + 1 < FN_CH ? r0 + FN_PAGE : r0;
            c00 = pal[r0[0] & 15];
            c10 = pal[(x0 + 1 < FN_CW ? r0[1] : r0[0]) & 15];
            c01 = pal[r1[0] & 15];
            c11 = pal[(x0 + 1 < FN_CW ? r1[1] : r1[0]) & 15];
            for (ch = 0; ch < 32; ch += 8) {
                const int a = (int)((c00 >> ch) & 255) * (8 - fx) + (int)((c10 >> ch) & 255) * fx;
                const int b = (int)((c01 >> ch) & 255) * (8 - fx) + (int)((c11 >> ch) & 255) * fx;
                const int v = (a * (8 - fy) + b * fy + 32) >> 6;
                out |= (unsigned int)(v > 255 ? 255 : v) << ch;
            }
            dst[(size_t)Y * (size_t)pitch + (size_t)X] = out;
        }
    }
}

// The ini value -> a file name stem: letters, digits, '_' and '-' kept, spaces dropped; "", "0", "Stock", "Off" and
// "Default" (any case) = stock (returns 0). Anything else that leaves no usable stem is stock too.
static int FnIniName(const char *v, char *stem, size_t n)
{
    size_t k = 0;
    if (!n) return 0;
    stem[0] = 0;
    if (!v) return 0;
    for (; *v && k + 1 < n; ++v) {
        const char c = *v;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            stem[k++] = c;
        else if (c != ' ')
            return stem[0] = 0, 0;  // path characters, dots, anything else: not a font name
    }
    stem[k] = 0;
    if (!k || !_stricmp(stem, "0") || !_stricmp(stem, "Stock") || !_stricmp(stem, "Off") || !_stricmp(stem, "Default"))
        return stem[0] = 0, 0;
    return 1;
}

// ---------------------------------------------------------------- v3.44 FN344: the Menu Font row (Options > Option 1)
// Values 0..3: Stock and the three sheets SH4 Refined ships (fonts\<stem>.sh4font); value 4 (FN_ROW_CUSTOM) only when the
// ini names another sheet at start (a user's own .sh4font): the row then has 5 values and value 4 writes that stem back.
#define FN_ROW_N 4
#define FN_ROW_CUSTOM 4
#define FN_ROW_MAX 5
static const char *const kFnRowStem[FN_ROW_N] = { "Stock", "EBGaramond", "CormorantGaramond", "CrimsonText" };  // the ini words
static const char *const kFnRowLabel[FN_ROW_N] = { "Stock", "EB Garamond", "Cormorant Garamond", "Crimson Text" };

// The row value of an ini value: 0 = Stock (FnIniName's stock words, or no usable name), 1..3 = a shipped sheet (any case,
// spaces dropped: "EB Garamond" is EBGaramond), FN_ROW_CUSTOM = another sheet (its stem in stem, n bytes; "" otherwise).
static int FnRowOfIni(const char *v, char *stem, size_t n)
{
    char st[32];
    int i;
    if (n) stem[0] = 0;
    if (!FnIniName(v, st, sizeof st)) return 0;
    for (i = 1; i < FN_ROW_N; ++i)
        if (!_stricmp(st, kFnRowStem[i])) return i;
    if (n) {
        size_t k;
        for (k = 0; st[k] && k + 1 < n; ++k) stem[k] = st[k];
        stem[k] = 0;
    }
    return FN_ROW_CUSTOM;
}

// The file stem value v loads ("" = Stock: nothing), custom = the ini's own sheet (value FN_ROW_CUSTOM)
static const char *FnRowStem(int v, const char *custom)
{
    if (v > 0 && v < FN_ROW_N) return kFnRowStem[v];
    if (v == FN_ROW_CUSTOM && custom && custom[0]) return custom;
    return "";
}

// What a live switch from the font in use (`from`: "" = Stock) to value v's stem `to` does
enum { FN_SW_NONE, FN_SW_STOCK, FN_SW_LOAD };
static int FnSwitchKind(const char *from, const char *to)
{
    if (!to || !to[0]) return from && from[0] ? FN_SW_STOCK : FN_SW_NONE;
    return from && !_stricmp(from, to) ? FN_SW_NONE : FN_SW_LOAD;
}

#endif
