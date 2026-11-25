// v3.32 DESC332: native test of src\descfit_sim.h (test\build_descfit_test.cmd).
//   descfit_sim_test.exe [fontdata_kr.bin]          the tests (with the game's font file: also its font 1 metrics)
//   descfit_sim_test.exe --cli fontdata_kr.bin maxW  one plain-text description a line on stdin -> its fit, a line each
// Pure logic: the measure (the layout 0x557660's first pass), wrap / tighten / shorten, the box rules, word-mode strings,
// strings left alone, and a randomised invariant run (every drawn line within the width, the block within the band).
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/descfit_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned char g_flat[0x200];  // a synthetic font: every glyph 10 units wide, space 6, no kerning
static unsigned char *g_real;        // font 1 of fontdata_kr.bin when given

static int Enc(unsigned char *o, const char *s)  // Encode(): 00 80, 0x100 - ASCII (space 00), FF FF
{
    int n = 0;
    o[n++] = 0x00, o[n++] = 0x80;
    for (; *s; ++s) o[n++] = *s == ' ' ? 0x00 : (unsigned char)(0x100 - (unsigned char)*s);
    o[n++] = 0xFF, o[n++] = 0xFF;
    return n;
}
static int EncW(unsigned char *o, const char *s)  // a word-mode string (a stock message): u16 0xE0 - (c - 0x20), space 0
{
    int n = 0;
    for (; *s; ++s) {
        const unsigned v = *s == ' ' ? 0 : 0xE0u - ((unsigned char)*s - 0x20u);
        o[n++] = (unsigned char)(v & 0xFF), o[n++] = (unsigned char)(v >> 8);
    }
    o[n++] = 0xFF, o[n++] = 0xFF;
    return n;
}

static DfBox Box(float maxW, float rowsBottom, float floorY)
{
    DfBox b;
    b.maxW = maxW, b.spacing = 2.5f, b.minSpacing = 1.0f, b.y = 360.0f, b.rowsBottom = rowsBottom, b.floorY = floorY;
    b.lineH = 26.0f * 448.0f / 480.0f, b.gap = 4.0f;
    return b;
}

// the drawn lines of a plan's output (or the source): split at the newline, each measured at the plan's spacing
static int Lines(const unsigned char *s, int len, int word, const DfFont *f, float sp, float w[DF_MAXLINES + 2])
{
    unsigned char seg[DF_MAXBYTES + 8];
    int i = word ? 0 : 2, a = i, n = 0;
    for (;;) {
        const int nl = word ? (i + 1 < len && s[i] == 0xFD && s[i + 1] == 0xFF) : (i + 1 < len && s[i] == 0xFF && s[i + 1] == 0xFD);
        const int end = i + 1 < len && s[i] == 0xFF && s[i + 1] == 0xFF;
        if (nl || end || i >= len) {
            int m = 0;
            DfToks k;
            if (!word) seg[m++] = 0x00, seg[m++] = 0x80;
            memcpy(seg + m, s + a, (size_t)(i - a));
            m += i - a;
            seg[m++] = 0xFF, seg[m++] = 0xFF;
            if (n < DF_MAXLINES + 2) w[n] = DfTokenize(seg, m, f, &k) ? DfLineW(&k, 0, k.n, sp) : -1.0f;
            ++n;
            if (!nl) break;
            i += 2, a = i;
            continue;
        }
        i += word ? 2 : (s[i] == 0xE0 ? 3 : s[i] > 0xE0 ? 2 : 1);
    }
    return n;
}

static void TestMeasure(void)
{
    DfFont f = { g_flat, 20.0f, 1 };
    DfToks k;
    unsigned char s[256];
    int n = Enc(s, "ab cd");
    CHECK(DfTokenize(s, n, &f, &k), "tokenize");
    CHECK(k.n == 6, "tokens %d (header + 5)", k.n);
    // 4 glyphs x (10 + 2.5) + space (6 + 2.5) - 2.5 = 56 units = 44.8 px
    CHECK(fabs(DfLineW(&k, 0, k.n, 2.5f) - 44.8f) < 0.01f, "width %f", DfLineW(&k, 0, k.n, 2.5f));
    n = EncW(s, "ab cd");
    CHECK(DfTokenize(s, n, &f, &k) && fabs(DfLineW(&k, 0, k.n, 2.5f) - 44.8f) < 0.01f, "word mode width");
    {   // kerning nibbles: lo +2, hi -1 (0xF) on 'a'
        static unsigned char kf[0x200];
        DfFont g = { kf, 20.0f, 1 };
        memcpy(kf, g_flat, sizeof kf);
        kf[0xF0 + ('a' - 0x20)] = 0xF2;
        n = Enc(s, "a");
        CHECK(DfTokenize(s, n, &g, &k) && fabs(DfLineW(&k, 0, k.n, 2.5f) - 11.0f / 1.25f) < 0.01f, "kerning");
    }
    {   // codes the measure cannot follow: drawn as they are
        static const unsigned char nl[] = { 0x00, 0x80, 0xBF, 0xFF, 0xFD, 0xBF, 0xFF, 0xFF };
        static const unsigned char sub[] = { 0x00, 0x80, 0xBF, 0xFF, 0xE0, 0xFF, 0xFF };
        static const unsigned char unterminated[] = { 0x00, 0x80, 0xBF, 0xBF };
        CHECK(!DfTokenize(nl, sizeof nl, &f, &k), "a newline: not measured");
        CHECK(!DfTokenize(sub, sizeof sub, &f, &k), "a sub-text: not measured");
        CHECK(!DfTokenize(unterminated, sizeof unterminated, &f, &k), "no end: not measured");
        CHECK(!DfTokenize(NULL, 10, &f, &k), "NULL");
    }
}

static void TestPlans(void)
{
    DfFont f = { g_flat, 20.0f, 1 };
    unsigned char s[512], out[600];
    DfPlan p;
    float w[DF_MAXLINES + 2];
    int n, ln, i;
    DfBox b = Box(416.0f, 350.0f, 444.0f);  // a 7-row page (rows end at 318) would be 318; 350 = a 10-row page
    // fits: untouched
    n = Enc(s, "A shine shows where items lie.");
    DescFit(s, n, &f, &b, out, sizeof out, &p);
    CHECK(p.mode == DF_SAME && p.outLen == 0 && p.dy == 0.0f && p.spacing == 2.5f, "short: same (mode %d)", p.mode);
    // 50 letters + 9 spaces = (50 x 12.5 + 9 x 8.5 - 2.5) / 1.25 = 559.2 px: two lines at the game's spacing, below the stock line
    n = Enc(s, "aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa");
    DescFit(s, n, &f, &b, out, sizeof out, &p);
    CHECK(p.mode == DF_WRAP && p.lines == 2 && p.spacing == 2.5f && p.dy == 0.0f, "wrap (mode %d lines %d dy %f)", p.mode, p.lines, p.dy);
    CHECK(fabs(p.wBefore - 559.2f) < 0.05f, "before %f", p.wBefore);
    ln = Lines(out, p.outLen, 0, &f, p.spacing, w);
    CHECK(ln == 2 && w[0] <= 416.0f && w[1] <= 416.0f && fabs(w[0] - w[1]) < 60.0f, "balanced lines %d: %f / %f", ln, w[0], w[1]);
    CHECK(fabs((w[0] > w[1] ? w[0] : w[1]) - p.wAfter) < 0.01f, "wAfter %f", p.wAfter);
    CHECK(p.outLen == n + 1, "one space became FF FD (%d -> %d)", n, p.outLen);
    for (i = 0; i < n && out[i] == s[i]; ++i) {}
    CHECK(i < n && s[i] == 0x00 && out[i] == 0xFF && out[i + 1] == 0xFD && !memcmp(out + i + 2, s + i + 1, (size_t)(n - i - 1)),
          "the rest of the bytes unchanged");
    // the same in word mode (a stock message string): u16 FFFD in place of the space
    n = EncW(s, "aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa");
    DescFit(s, n, &f, &b, out, sizeof out, &p);
    ln = Lines(out, p.outLen, 1, &f, p.spacing, w);
    CHECK(p.mode == DF_WRAP && p.outLen == n && ln == 2 && w[0] <= 416.0f && w[1] <= 416.0f, "word mode wrap");
    // no room for a second line (rows end at 386, something drawn from y 400 down): a slightly tighter spacing, one line
    {
        DfBox t = Box(416.0f, 386.0f, 400.0f);
        n = Enc(s, "aaaaaa aaaaaa aaaaaa aaaaaa aaaaaa aaaaa");  // 35 x 10 + 5 x 6.8 = 384 px at 2.5; 35 x 12.5 ... fits at 2.5? no:
        n = Enc(s, "aaaaaaa aaaaaaa aaaaaaa aaaaaaa aaaaaaa");  // 35 + 4 spaces: 350 + 27.2 = 377.2: fits
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        CHECK(p.mode == DF_SAME, "fits");
        n = Enc(s, "aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa");  // 40 + 4 = 400 + 27.2 = 427.2 px > 416
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        CHECK(DfLinesRoom(&t) == 1, "room 1");
        CHECK(p.mode == DF_TIGHT && p.lines == 1 && p.spacing < 2.5f && p.spacing >= 1.0f && p.wAfter <= 416.0f && p.outLen == 0 && p.dy == 0.0f,
              "tight (mode %d sp %f w %f)", p.mode, p.spacing, p.wAfter);
        // far too long for one line even at spacing 1.0: shortened with "..."
        n = Enc(s, "aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa");
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        ln = Lines(out, p.outLen, 0, &f, p.spacing, w);
        CHECK(p.mode == DF_SHORT && ln == 1 && w[0] <= 416.0f && p.outLen > 0, "short (mode %d, %d lines, %f)", p.mode, ln, w[0]);
        CHECK(p.outLen >= 5 && out[p.outLen - 3] == 0xD2 && out[p.outLen - 4] == 0xD2 && out[p.outLen - 5] == 0xD2, "ends with ...");
        CHECK(out[p.outLen - 6] != 0x00, "no space before ...");
    }
    // a 10-row page (rows end at 360, at the stock line): two lines below it, the first line in place
    {
        DfBox t = Box(416.0f, 360.0f, 444.0f);
        n = Enc(s, "aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa");
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        CHECK(DfLinesRoom(&t) == 3 && p.mode == DF_WRAP && p.lines == 2 && p.dy == 0.0f && p.spacing == 2.5f, "10 rows: wrap below (mode %d)", p.mode);
    }
    // three lines when two are not enough and the box has room (rows end at 318, floor 444)
    {
        DfBox t = Box(416.0f, 318.0f, 444.0f);
        n = Enc(s, "aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa aaaaaaaaa");  // ~960 px
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        ln = Lines(out, p.outLen, 0, &f, p.spacing, w);
        CHECK(DfLinesRoom(&t) == 3, "room 3 (%d)", DfLinesRoom(&t));
        CHECK(p.mode == DF_WRAP && p.lines == 3 && ln == 3 && w[0] <= 416.0f && w[1] <= 416.0f && w[2] <= 416.0f, "three lines");
        CHECK(p.dy == 0.0f, "below the stock line (360 + 3 x 24.3 = 432.8 <= 444)");
    }
    // the Subtitle Style page: its preview line under the description (floor 376): two lines lifted above y 360
    {
        DfBox t = Box(416.0f, 318.0f, 376.0f);
        n = Enc(s, "aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa");
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        CHECK(p.mode == DF_WRAP && p.lines == 2 && p.dy < 0.0f, "lifted (dy %f)", p.dy);
        CHECK(360.0f + p.dy >= 318.0f + 4.0f && 360.0f + p.dy + 2 * t.lineH <= 376.0f + 0.01f, "within the band");
        // a one-line description stays at the stock place even under a low floor
        n = Enc(s, "Subtitle text colour.");
        DescFit(s, n, &f, &t, out, sizeof out, &p);
        CHECK(p.mode == DF_SAME && p.dy == 0.0f, "one line in place");
    }
    // a string longer than the output buffer: drawn as it was
    {
        unsigned char tiny[8];
        n = Enc(s, "aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa aaaaa");
        DescFit(s, n, &f, &b, tiny, sizeof tiny, &p);
        CHECK(p.outLen == 0 && (p.mode == DF_PASS || p.mode == DF_TIGHT), "no room in out (mode %d)", p.mode);
    }
}

static unsigned Rnd(unsigned *st) { *st = *st * 1664525u + 1013904223u; return *st >> 8; }

// random sentences, random boxes: every plan keeps its lines within the width and its block within the band
static void TestRandom(const DfFont *f, const char *name)
{
    unsigned st = 12345;
    int it, bad = 0, modes[5] = { 0 };
    for (it = 0; it < 20000; ++it) {
        char txt[200];
        unsigned char s[256], out[600];
        int len = 0, words = 1 + (int)(Rnd(&st) % 14), wi, ln, i, ok = 1;
        DfPlan p;
        DfBox b;
        float w[DF_MAXLINES + 2];
        for (wi = 0; wi < words && len < 120; ++wi) {
            int L = 1 + (int)(Rnd(&st) % 11), c;
            if (wi) txt[len++] = ' ';
            for (c = 0; c < L && len < 150; ++c) txt[len++] = (char)(0x21 + Rnd(&st) % 94);
        }
        txt[len] = 0;
        b = Box(200.0f + (float)(Rnd(&st) % 300), 300.0f + (float)(Rnd(&st) % 100), 370.0f + (float)(Rnd(&st) % 80));
        len = Enc(s, txt);
        DescFit(s, len, f, &b, out, sizeof out, &p);
        modes[p.mode + 1]++;
        if (p.mode == DF_PASS) continue;
        ln = p.outLen ? Lines(out, p.outLen, 0, f, p.spacing, w) : Lines(s, len, 0, f, p.spacing, w);
        if (p.mode != DF_SAME)
            for (i = 0; i < ln && i < DF_MAXLINES + 2; ++i)
                if (w[i] > b.maxW + 0.01f) ok = 0;
        if (ln != p.lines || ln > DfLinesRoom(&b)) ok = 0;
        if (p.lines > 1 && (360.0f + p.dy < DfTop(&b) - 0.01f || 360.0f + p.dy + p.lines * b.lineH > b.floorY + 0.02f)) ok = 0;
        if (p.mode == DF_SAME && (p.wBefore > b.maxW || p.outLen)) ok = 0;
        if (p.spacing < b.minSpacing - 0.001f || p.spacing > b.spacing) ok = 0;
        if (p.outLen && (out[p.outLen - 1] != 0xFF || out[p.outLen - 2] != 0xFF)) ok = 0;
        if (!ok && bad++ < 5) printf("  bad plan: \"%s\" mode %d lines %d/%d w0 %f max %f\n", txt, p.mode, p.lines, ln, w[0], b.maxW);
    }
    CHECK(!bad, "%s: %d of 20000 random plans broke a rule", name, bad);
    printf("  %s random plans: same %d, wrap %d, tight %d, short %d, pass %d\n", name, modes[1], modes[2], modes[3], modes[4], modes[0]);
}

static unsigned char *LoadFont1(const char *path)
{
    FILE *fp = fopen(path, "rb");
    unsigned char *d, *r;
    long sz;
    unsigned n, o1, o2;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    d = (unsigned char *)malloc((size_t)sz);
    if (!d || fread(d, 1, (size_t)sz, fp) != (size_t)sz) { fclose(fp); free(d); return NULL; }
    fclose(fp);
    memcpy(&n, d, 4);
    if (n < 2) { free(d); return NULL; }
    memcpy(&o1, d + 8, 4);
    memcpy(&o2, d + 12, 4);
    r = (unsigned char *)malloc(0x200);
    memcpy(r, d + o1, 0x200);  // entry 1 = font 1 (only its width / kerning tables are used)
    free(d);
    return r;
}

static int Cli(const char *font, float maxW, float rowsBottom, float floorY)
{
    char line[512];
    DfFont f;
    DfBox b = Box(maxW, rowsBottom, floorY);  // default: the worst page, 10 rows (Player with Playable Eileen), nothing below
    g_real = LoadFont1(font);
    if (!g_real) return 2;
    f.font = g_real, f.cellW = 20.0f, f.fontId = 1;
    while (fgets(line, sizeof line, stdin)) {
        unsigned char s[600], out[700];
        float w[DF_MAXLINES + 2];
        DfPlan p;
        int n, ln, i;
        char desc[160];
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        n = Enc(s, line);
        DescFit(s, n, &f, &b, out, sizeof out, &p);
        ln = p.outLen ? Lines(out, p.outLen, 0, &f, p.spacing, w) : Lines(s, n, 0, &f, p.spacing, w);
        sprintf(desc, "%s %.1f->", p.mode == DF_SAME ? "same" : p.mode == DF_WRAP ? "WRAP" : p.mode == DF_TIGHT ? "TIGHT" : p.mode == DF_SHORT ? "SHORT" : "pass",
                p.wBefore);
        for (i = 0; i < ln && i < DF_MAXLINES + 2; ++i) sprintf(desc + strlen(desc), "%s%.1f", i ? "+" : "", w[i]);
        if (p.spacing != 2.5f) sprintf(desc + strlen(desc), " sp%.1f", p.spacing);
        {   // the drawn text, lines split by |, for the mockup (tools\desc_mockup.py)
            const unsigned char *o = p.outLen ? out : s;
            const int m = p.outLen ? p.outLen : n;
            char txt[600];
            int t = 0;
            for (i = 2; i + 1 < m && t < 590; ++i) {
                if (o[i] == 0xFF && o[i + 1] == 0xFF) break;
                if (o[i] == 0xFF && o[i + 1] == 0xFD) { txt[t++] = 0x7C; ++i; continue; }
                txt[t++] = o[i] == 0x00 ? 0x20 : (char)(0x100 - o[i]);
            }
            txt[t] = 0;
            printf("%s\t%.2f\t%.1f\t%s\t%s\n", desc, p.dy, p.spacing, txt, line);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    if (argc >= 4 && !strcmp(argv[1], "--cli"))
        return Cli(argv[2], (float)atof(argv[3]), argc >= 5 ? (float)atof(argv[4]) : 360.0f, argc >= 6 ? (float)atof(argv[5]) : 444.0f);
    for (i = 0; i < 0xE0; ++i) g_flat[0x10 + i] = 10, g_flat[0xF0 + i] = 0;
    g_flat[0x10] = 6;
    TestMeasure();
    TestPlans();
    {
        DfFont f = { g_flat, 20.0f, 1 };
        TestRandom(&f, "flat font");
    }
    if (argc >= 2 && (g_real = LoadFont1(argv[1])) != NULL) {
        DfFont f = { g_real, 20.0f, 1 };
        unsigned char s[256], out[600];
        DfPlan p;
        DfBox b = Box(416.0f, 350.0f, 444.0f);
        float w[DF_MAXLINES + 2];
        int n = Enc(s, "Companion Eileen's own curse, as the game keeps it."), ln;
        DescFit(s, n, &f, &b, out, sizeof out, &p);
        ln = Lines(out, p.outLen, 0, &f, p.spacing, w);
        CHECK(fabs(p.wBefore - 461.6f) < 0.05f, "font 1: the widest v3.31 line measures %f (desc_measure.py: 461.6)", p.wBefore);
        CHECK(p.mode == DF_WRAP && ln == 2 && w[0] <= 416.0f && w[1] <= 416.0f, "font 1: wrapped %f + %f", w[0], w[1]);
        TestRandom(&f, "font 1");
    } else if (argc >= 2) {
        printf("FAIL: cannot read %s\n", argv[1]);
        ++g_fail;
    }
    printf("%d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
