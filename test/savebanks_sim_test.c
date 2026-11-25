// v3.47 SB347: native test of src\savebanks_sim.h (test\build_sb_test.cmd): the save / load list's Bank row layout (canvas
// units), the mouse's hit boxes and click steps, the arrows shown, and the label: "\h" turns it into Latin glyphs of the
// list's font (what [Textures] MenuFont redraws), where v3.46's plain ASCII gave the special glyphs (codes >= 0xE0, never
// the Menu Font: the user's QA 2026-11-24, user_screens\save_bank_v346.webp).
#include <stdio.h>
#include <string.h>
#include "../src/savebanks_sim.h"
#include "../src/font_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SBT_MAX 99  // SB_MAX (sh4windowmod.c)

// A model of 556730 / 5560F0 (SH4Decomp src\system\text_555cd0.c TextAsciiConvertEscapes, the parts a label can reach):
// "\h" half width on, then a character c is 0x100 - c, a space 0; before it letters / digits are the special glyph codes
// (TextAsciiToCode: >= 0xE0), a space 0xFFFE; a bare 'e' (or "\e") ends the text. Returns the codes written.
static int ModelConvert(const char *s, unsigned short *out, int max, int *ended)
{
    int n = 0, half = 0;
    *ended = 0;
    while (*s && n < max) {
        unsigned char c = (unsigned char)*s++;
        if (c == 'e') { *ended = 1; break; }
        if (c == '\\') {
            c = (unsigned char)*s++;
            if (c == 'h' || c == 'H' || c == '!') half = 1;
            else if (c == 'z' || c == 'Z') half = 0;
            else if (c == 'e' || c == 'E') { *ended = 1; break; }
            continue;
        }
        if (c == ' ') { out[n++] = half ? 0 : 0xFFFE; continue; }
        if (half) out[n++] = (unsigned short)(0x100 - c);
        else if (c >= '0' && c <= '9') out[n++] = (unsigned short)(c + 0x142);
        else if (c >= 'A' && c <= 'Z') out[n++] = (unsigned short)(c + 0x13B);
        else if (c >= 'a' && c <= 'z') out[n++] = (unsigned short)(c + 0x135);
        else out[n++] = 0xE0;  // punctuation: D_005C5DFC.. + 0xE0
    }
    return n;
}

// TextQueueString's glyph code for a raw code c (0x557660: c < 0xE0 -> 0xE0 - c of the current font, else c), as the
// glyph cache holds it for font f; 1 = a glyph the .sh4font sheet holds (FnCodeGlyph), so the Menu Font draws it.
static int MenuFontDraws(unsigned short c, int f)
{
    int font, glyph;
    const unsigned int code = (unsigned int)f * FN_CODES + (c < 0xE0 ? 0xE0u - c : c);
    return FnCodeGlyph(code, &font, &glyph);
}

static void TestLayout(void)
{
    SbLayout L;
    memset(&L, 0x55, sizeof L);
    SbLayoutMake(&L);
    CHECK(L.textX == 256 && L.textY == 0x24, "label centred at 256, y 0x24 (%d, %d)", L.textX, L.textY);
    CHECK(L.bar.x0 == 192 && L.bar.x1 == 320 && L.bar.y0 == 0x24 && L.bar.y1 == 0x24 + 24, "bar 192..320 x 36..60");
    CHECK(L.lBase == 178 && L.lTip == 158 && L.rBase == 334 && L.rTip == 354, "arrows 158..178 / 334..354 (%d %d %d %d)",
          L.lTip, L.lBase, L.rBase, L.rTip);
    CHECK(L.lBase - L.lTip == 0x14 && L.rTip - L.rBase == 0x14, "arrows 20 long (56FB80)");
    CHECK(L.bar.x0 - L.lBase == 0x0E && L.rBase - L.bar.x1 == 0x0E, "arrows 14 outside the bar (the table's gap)");
    CHECK(L.left.x1 == L.bar.x0 && L.bar.x1 == L.right.x0, "click boxes touch: no gap, no overlap");
    CHECK(L.left.x0 <= L.lTip && L.right.x1 >= L.rTip, "each arrow inside its click box");
    CHECK(L.row.x0 == L.left.x0 && L.row.x1 == L.right.x1 && L.row.y0 == L.bar.y0 && L.row.y1 == L.bar.y1, "row = the union");
    CHECK(L.row.x0 + L.row.x1 == 512, "row centred on the canvas (%d..%d)", L.row.x0, L.row.x1);
    CHECK(L.row.x0 >= 32 && L.row.x1 <= 480, "row inside the list's width (32..480)");
    CHECK(L.row.y1 <= 0x43, "row above the list's first row (y 0x43), so the list's own row boxes never take it");
    CHECK(L.row.y0 >= 0, "row on the canvas");
}

static void TestRects(void)
{
    SbLayout L;
    float r[4];
    SbLayoutMake(&L);
    SbBoxRect(&L.left, r);
    CHECK(r[0] == 154.0f && r[1] == 36.0f && r[2] == 38.0f && r[3] == 24.0f, "left box as 405760's x, y, w, h (%g %g %g %g)",
          r[0], r[1], r[2], r[3]);
    SbBoxRect(&L.right, r);
    CHECK(r[0] == 320.0f && r[2] == 38.0f, "right box (%g %g)", r[0], r[2]);
    SbBoxRect(&L.bar, r);
    CHECK(r[0] == 192.0f && r[2] == 128.0f, "bar box (%g %g)", r[0], r[2]);
}

static void TestHits(void)
{
    SbLayout L;
    int x, y, bad = 0, nl = 0, nb = 0, nr = 0;
    SbLayoutMake(&L);
    for (y = 0; y < 448; y += 2)
        for (x = 0; x < 512; ++x) {
            const float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            const int h = SbHitAt(&L, fx, fy);
            const int g = SbHitOf(SbInBox(&L.left, fx, fy), SbInBox(&L.bar, fx, fy), SbInBox(&L.right, fx, fy));
            const int inRow = SbInBox(&L.row, fx, fy);
            if (h != g || (h != SB_HIT_NONE) != inRow) ++bad;
            nl += h == SB_HIT_LEFT;
            nb += h == SB_HIT_BAR;
            nr += h == SB_HIT_RIGHT;
        }
    CHECK(!bad, "SbHitAt = SbHitOf of the three box tests, and a hit exactly inside the row (%d bad)", bad);
    CHECK(nl == nr && nl > 0 && nb > nl, "left and right boxes the same size, the bar bigger (%d %d %d)", nl, nb, nr);
    CHECK(SbHitAt(&L, 168.0f, 48.0f) == SB_HIT_LEFT, "the left arrow's middle");
    CHECK(SbHitAt(&L, 344.0f, 48.0f) == SB_HIT_RIGHT, "the right arrow's middle");
    CHECK(SbHitAt(&L, 185.0f, 48.0f) == SB_HIT_LEFT, "the gap between the left arrow and the bar counts as the arrow");
    CHECK(SbHitAt(&L, 256.0f, 48.0f) == SB_HIT_BAR, "the label");
    CHECK(SbHitAt(&L, 256.0f, 70.0f) == SB_HIT_NONE, "the list's first row is not the Bank row");
    CHECK(SbHitAt(&L, 100.0f, 48.0f) == SB_HIT_NONE && SbHitAt(&L, 400.0f, 48.0f) == SB_HIT_NONE, "beside the row");
    CHECK(SbClickStep(SB_HIT_LEFT) == -1 && SbClickStep(SB_HIT_RIGHT) == 1 && SbClickStep(SB_HIT_BAR) == 1 &&
          SbClickStep(SB_HIT_NONE) == 0, "click steps: left -1, right / bar +1, none 0");
}

// v3.46: the hover box was the bar alone (326..456, the arrows at 292..312 / 470..490): moving the mouse from the bar to
// an arrow left the box, sb_onrow dropped and the arrows were no longer drawn - the user could not click them. v3.47: the
// whole way from the bar's middle to either arrow tip stays on the row.
static void TestMoveToArrows(void)
{
    SbLayout L;
    float x;
    int v346Lost = 0, v347Lost = 0;
    SbLayoutMake(&L);
    for (x = 391.0f; x <= 490.0f; x += 1.0f)  // v3.46: from its bar's middle to its right arrow's tip
        if (!(x >= 326.0f && x < 456.0f)) ++v346Lost;
    for (x = 256.0f; x <= (float)L.rTip; x += 1.0f)
        if (SbHitAt(&L, x, 48.0f) == SB_HIT_NONE) ++v347Lost;
    for (x = 256.0f; x >= (float)L.lTip; x -= 1.0f)
        if (SbHitAt(&L, x, 48.0f) == SB_HIT_NONE) ++v347Lost;
    CHECK(v346Lost > 0, "model: v3.46's hover box lost the row on the way to the arrow (%d points)", v346Lost);
    CHECK(v347Lost == 0, "v3.47: the row stays under the mouse all the way to both arrow tips (%d points lost)", v347Lost);
}

static void TestArrows(void)
{
    CHECK(!SbArrowShown(1, SBT_MAX, -1) && SbArrowShown(1, SBT_MAX, 1), "bank 1: right only");
    CHECK(SbArrowShown(2, SBT_MAX, -1) && SbArrowShown(2, SBT_MAX, 1), "bank 2: both");
    CHECK(SbArrowShown(SBT_MAX, SBT_MAX, -1) && !SbArrowShown(SBT_MAX, SBT_MAX, 1), "the last bank: left only");
    CHECK(!SbArrowShown(999, 2000, 1), "never past bank 999 (the file name rule)");
}

static void TestLabel(void)
{
    char buf[64], tiny[4];
    unsigned short codes[64];
    int b, n, k, ended, latin, special;
    n = SbLabel(buf, (int)sizeof buf, 2, 0);
    CHECK(n == 8 && !strcmp(buf, "\\hBank 2"), "label \\hBank 2 (%d \"%s\")", n, buf);
    n = SbLabel(buf, (int)sizeof buf, 12, 1);
    CHECK(n == 10 && !strcmp(buf, "\\hBank 12?"), "stale label \\hBank 12? (%d \"%s\")", n, buf);
    CHECK(SbLabel(tiny, (int)sizeof tiny, 99, 1) == -1 && tiny[sizeof tiny - 1] == 0, "too small a buffer: -1, terminated");
    CHECK(SbLabel(NULL, 10, 1, 0) == -1 && SbLabel(buf, 0, 1, 0) == -1, "no buffer: -1");
    for (b = 1; b <= 999; ++b) {
        n = SbLabel(buf, 0x40, b, b & 1);  // sb_lbl_buf is 0x40 bytes
        k = ModelConvert(buf, codes, 64, &ended);
        latin = special = 0;
        {
            int i;
            for (i = 0; i < k; ++i) {
                if (codes[i] == 0) continue;  // a space: the font's own space width
                if (MenuFontDraws(codes[i], 0) && MenuFontDraws(codes[i], 1)) ++latin;
                else ++special;
            }
        }
        if (n < 0 || ended || special || latin != n - 3 || n >= SB_LABEL_MAX) {
            CHECK(0, "bank %d: label \"%s\" (%d), ended %d, %d Latin, %d special", b, buf, n, ended, latin, special);
            break;
        }
    }
    CHECK(b == 1000, "banks 1..999: every label character is a Latin glyph the Menu Font sheet draws, no 'e' ends it early");
    // v3.46 drew "Bank 2" and "Q/E L1/R1:" without "\h": special glyphs (>= 0xE0), never from the sheet (the old stroke look)
    k = ModelConvert("Bank 2", codes, 64, &ended);
    latin = 0;
    for (n = 0; n < k; ++n) latin += codes[n] != 0xFFFE && MenuFontDraws(codes[n], 0);
    CHECK(k == 6 && latin == 0, "model: v3.46's plain \"Bank 2\" had no glyph the Menu Font draws (%d of %d)", latin, k);
    k = ModelConvert("\\h02. Return", codes, 64, &ended);
    CHECK(ended && k == 5, "model: a bare 'e' ends a text (why a label must never hold one)");
}

// ================================================================ v3.48 SB348
// v3.47's sb_rep (savebanks.inc), kept here as the reference: SbRepStep must step on exactly the same frames.
static unsigned o_hold, o_next;
static int o_hdir, o_quiet;
static int OldRep(int held, int press)
{
    unsigned c, iv;
    o_quiet = 0;
    if (!held) {
        o_hdir = 0;
        o_hold = 0;
        return press;
    }
    if (held != o_hdir) {
        o_hdir = held;
        o_hold = 0;
        o_next = 15;
        return held;
    }
    c = ++o_hold;
    if (c < o_next) return 0;
    iv = 4;
    if (c >= 45) {
        iv = 2;
        if (c >= 90) iv = 1;
    }
    o_next = c + iv;
    o_quiet = 1;
    return held;
}

static unsigned g_rnd = 12345u;
static unsigned Rnd(void) { return g_rnd = g_rnd * 1103515245u + 12345u, (g_rnd >> 16) & 0x7FFF; }

static void TestRepeat(void)
{
    SbRep r;
    int f, q, k, bad = 0, steps = 0, first = -1, at[8], na = 0;
    memset(&r, 0, sizeof r);
    o_hold = o_next = 0, o_hdir = 0;
    for (f = 0; f < 20000; ++f) {  // random holds, releases, taps and direction changes: the v3.47 frames exactly
        const unsigned x = Rnd() % 100;
        const int held = x < 50 ? (o_hdir ? o_hdir : 1) : x < 60 ? -1 : x < 65 ? 10 : x < 68 ? -10 : 0;
        const int press = held ? 0 : (Rnd() % 7 == 0 ? (Rnd() & 1 ? 1 : -1) : 0);
        const int a = SbRepStep(&r, &kSbRep, held, press, &q), b = OldRep(held, press);
        if (a != b || q != o_quiet) ++bad;
    }
    CHECK(!bad, "SbRepStep steps on the same frames as v3.47's sb_rep (20000 random frames, %d differ)", bad);
    SbRepReset(&r);
    for (f = 0; f < 300; ++f) {  // one hold of 10 seconds at 30 frames a second
        k = SbRepStep(&r, &kSbRep, 1, 0, &q);
        if (k) {
            ++steps;
            if (first < 0) first = f;
            if (na < 8) at[na++] = f;
            if ((f == 0) == q) ++bad;  // only the first step is loud
        }
    }
    CHECK(first == 0 && na == 8 && at[1] == 15 && at[2] == 19 && at[3] == 23, "a hold: a step at once, then at 15, 19, 23 ... (%d %d %d)",
          at[1], at[2], at[3]);
    CHECK(!bad, "only the hold's first step may buzz (the repeat is quiet)");
    // frame 0; 15 19 .. 43 every 4 (8); after 43 the next is 47: 47 49 .. 89 every 2 (22); after 89 the next is 91: 91 .. 299
    // every frame (209)
    CHECK(steps == 1 + 8 + 22 + 209, "a 10 s hold: %d steps (1 + 8 + 22 + 209), it accelerates", steps);
    SbRepReset(&r);
    CHECK(SbRepStep(&r, &kSbRep, 0, 1, &q) == 1 && !q, "a tap shorter than a frame (a new press, nothing held) steps once");
    CHECK(SbRepStep(&r, &kSbRep, 1, 0, &q) == 1 && SbRepStep(&r, &kSbRep, 10, 0, &q) == 10 && !q,
          "from 1 to 10 while held (Right, then E as well): a new hold, a step of 10 at once");
    CHECK(SbRepStep(&r, &kSbRep, -10, 0, &q) == -10, "a change of direction: a new hold");
}

static void TestPick(void)
{
    int h, p;
    SbPick(1, 1, 0, 0, &h, &p);
    CHECK(h == 1 && p == 1, "Right alone: 1 (%d %d)", h, p);
    SbPick(0, 0, 1, 0, &h, &p);
    CHECK(h == 10 && p == 0, "E / RB (R1, R): 10 (%d %d)", h, p);
    SbPick(-1, -1, 1, 0, &h, &p);
    CHECK(h == 10 && p == 0, "Left and E together: the batch wins (%d %d)", h, p);
    SbPick(0, 0, -1, -1, &h, &p);
    CHECK(h == -10 && p == -10, "Q / LB (L1, L): -10 (%d %d)", h, p);
    SbPick(0, 0, 0, 0, &h, &p);
    CHECK(h == 0 && p == 0, "nothing: 0");
}

static void TestTarget(void)
{
    CHECK(SbTarget(4, -10, SBT_MAX) == 1, "bank 4 by -10 lands on bank 1 (the spec's example)");
    CHECK(SbTarget(1, -10, SBT_MAX) == 1 && SbTarget(1, -1, SBT_MAX) == 1, "at bank 1 already: no step (the buzzer)");
    CHECK(SbTarget(95, 10, SBT_MAX) == 99 && SbTarget(99, 10, SBT_MAX) == 99, "by +10 near the end: the last bank, then no step");
    CHECK(SbTarget(50, 10, SBT_MAX) == 60 && SbTarget(50, -10, SBT_MAX) == 40, "by 10 in the middle");
    CHECK(SbTarget(2, -1, SBT_MAX) == 1 && SbTarget(98, 1, SBT_MAX) == 99, "single steps as v3.47");
    CHECK(SbTarget(995, 10, 2000) == 999, "never past bank 999 (the file name rule)");
    CHECK(SbTarget(0, 1, SBT_MAX) == 0 && SbTarget(120, -1, SBT_MAX) == 120, "a bad current bank: no step");
}

static void TestMouseHold(void)
{
    SbMh m;
    SbRep r;
    int f, q, d, steps = 0, bad = 0;
    memset(&m, 0, sizeof m);
    SbRepReset(&r);
    d = SbMhFrame(&m, 1, 1, SB_HIT_RIGHT);
    CHECK(d == 1 && SbRepStep(&r, &kSbRep, d, 0, &q) == 1 && !q, "a press on the right arrow: one step at once, loud");
    for (f = 1; f < 90; ++f) {  // 3 s held on the arrow
        d = SbMhFrame(&m, 0, 1, SB_HIT_RIGHT);
        if (d != 1) ++bad;
        if (SbRepStep(&r, &kSbRep, d, 0, &q)) ++steps;
    }
    CHECK(!bad && steps == 8 + 22, "held 3 s on the arrow: the key repeat (%d more steps)", steps);
    d = SbMhFrame(&m, 0, 1, SB_HIT_BAR);
    CHECK(d == 0 && SbRepStep(&r, &kSbRep, d, 0, &q) == 0, "the mouse slides onto the bar: the hold stops");
    d = SbMhFrame(&m, 0, 1, SB_HIT_RIGHT);
    CHECK(d == 0, "back on the arrow with the button still down: no step without a new press");
    d = SbMhFrame(&m, 1, 1, SB_HIT_LEFT);
    CHECK(d == -1, "a new press on the left arrow: the previous bank");
    d = SbMhFrame(&m, 0, 0, SB_HIT_LEFT);
    CHECK(d == 0, "released: the hold stops");
    CHECK(SbMhFrame(&m, 1, 1, SB_HIT_BAR) == 0 && m.part == SB_HIT_NONE, "a press on the bar: no hold (a click: the next bank)");
    CHECK(SbMhFrame(&m, 1, 1, SB_HIT_NONE) == 0, "a press off the row: nothing");
    d = SbMhFrame(&m, 1, 1, SB_HIT_LEFT);
    d = SbMhFrame(&m, 0, 1, SB_HIT_NONE);
    CHECK(d == 0, "the mouse leaves the row while held: the hold stops");
}

// From bank 1, E held: steps of 10 with the repeat, clamped at the last bank; quiet repeats at the end, the first loud
static void TestBatchHold(void)
{
    SbRep r;
    int f, q, bank = 1, loudEnd = 0, quietEnd = 0, seen[8], ns = 0;
    SbRepReset(&r);
    for (f = 0; f < 120; ++f) {
        const int k = SbRepStep(&r, &kSbRep, 10, 0, &q);
        int n;
        if (!k) continue;
        n = SbTarget(bank, k, SBT_MAX);
        if (n == bank) {
            if (q) ++quietEnd;
            else ++loudEnd;
        } else if (ns < 8) {
            seen[ns++] = n;
        }
        bank = n;
    }
    CHECK(ns == 8 && seen[0] == 11 && seen[1] == 21 && seen[7] == 81, "E held from bank 1: 11, 21 .. 81 (%d %d %d)", seen[0],
          seen[1], seen[7]);
    CHECK(bank == SBT_MAX && loudEnd == 0 && quietEnd > 0, "then 91, 99 (clamped), then quiet repeats at the end (%d %d %d)", bank,
          loudEnd, quietEnd);
    CHECK(SbRepStep(&r, &kSbRep, 0, 0, &q) == 0 && SbRepStep(&r, &kSbRep, 10, 0, &q) == 10 && !q &&
          SbTarget(bank, 10, SBT_MAX) == bank, "pressed again at the last bank: a loud step that does not move (the buzzer)");
}

static void TestSettle(void)
{
    SbSettle s;
    char line[160], tiny[8];
    int n;
    memset(&s, 0, sizeof s);
    CHECK(!SbSettleDue(&s, 5000, 1), "nothing open: nothing to write");
    SbSettleStep(&s, 3, SB_SRC_KEYS, 0, 1000);
    SbSettleStep(&s, 4, SB_SRC_PAD, 1, 1100);
    SbSettleStep(&s, 14, SB_SRC_PAD, 1, 1200);
    SbSettleStep(&s, 24, SB_SRC_MOUSE, 0, 1300);
    SbSettleStep(&s, 25, SB_SRC_KEYS, 0, 1400);
    CHECK(s.from == 3 && s.steps == 5 && s.by10 == 2 && s.src[0] == 2 && s.src[1] == 2 && s.src[2] == 1, "the steps add up");
    CHECK(!SbSettleDue(&s, 2399, 0) && SbSettleDue(&s, 2400, 0), "settled 1 s after the last step");
    CHECK(SbSettleDue(&s, 1401, 1), "forced (the list opened again, a save): at once");
    n = SbSettleLine(&s, 26, "load", line, (int)sizeof line);
    CHECK(n > 0 && !strcmp(line, "bank 3 -> 26 in the load list (5 steps: keys 2, pad 2, mouse 1; by 10: 2)"), "the line: \"%s\"", line);
    memset(&s, 0, sizeof s);
    SbSettleStep(&s, 7, SB_SRC_MOUSE, 0, 0xFFFFFF00u);  // GetTickCount wraps
    CHECK(!SbSettleDue(&s, 0x00000100u, 0) && SbSettleDue(&s, 0x00000300u, 0), "settles across GetTickCount's wrap");
    n = SbSettleLine(&s, 8, "save", line, (int)sizeof line);
    CHECK(n > 0 && !strcmp(line, "bank 7 -> 8 in the save list (1 step: keys 0, pad 0, mouse 1; by 10: 0)"), "one step: \"%s\"", line);
    CHECK(SbSettleLine(&s, 8, "save", tiny, (int)sizeof tiny) == -1 && tiny[sizeof tiny - 1] == 0, "too small: -1, terminated");
    s.open = 0;
    SbSettleStep(&s, 9, SB_SRC_KEYS, 0, 5);
    CHECK(s.from == 9 && s.steps == 1 && s.src[2] == 0, "a new change starts from zero");
}

// v3.49 SB349: the lists' highlight bar. The model runs the stock bytes and the patch: the stock run gives 56FDA0's yellow,
// the patch 56FCC0 style 0's blue (MenuDrawModeRowBar: (a | 0FFFF8000h) << 16), for every pulse and both focus states.
static void TestListBar(void)
{
    int f, k, focus, bad = 0, badBlue = 0, badPulse = 0, known;
    unsigned lo = 0xFFFFFFFFu, hi = 0;
    for (f = 0; f < 256; ++f)
        for (focus = 0; focus < 2; ++focus) {
            const unsigned a = focus ? SbPulse((unsigned char)f) : 0x60u;
            const unsigned y = SbListBarColour((unsigned char)f, focus, 0), b = SbListBarColour((unsigned char)f, focus, 1);
            unsigned mode;
            bad += SbBarExec(kSbBarStock, SB_BAR_LEN, a) != y || y != (0x80000000u | a << 8 | a);
            mode = (a | 0xFFFF8000u) << 16;  // 56FCC0 style 0 (and savebanks.inc sb_bar)
            badBlue += SbBarExec(kSbBarBlue, SB_BAR_LEN, a) != b || b != mode;
            badBlue += (b & 0x0000FFFFu) != 0 || (b >> 24) != 0x80u;  // no red, no green: blue only, alpha 0x80
            if (focus) {
                lo = a < lo ? a : lo;
                hi = a > hi ? a : hi;
            }
        }
    CHECK(!bad, "the stock bytes model 56FDA0's yellow 0x8000aaaa (%d bad)", bad);
    CHECK(!badBlue, "the patch gives 56FCC0's blue 0x80aa0000 (%d bad)", badBlue);
    CHECK(lo == 0x40 && hi == 0x80, "the pulse runs 0x40 .. 0x80 (%X .. %X)", lo, hi);
    for (f = 0; f < 256; ++f) {  // the pulse as 56FDA0's bytes compute it (byte arithmetic)
        unsigned char al = (unsigned char)f, cl;
        if (al & 0x20) cl = (unsigned char)((unsigned char)((al & 0x1F) + 0x20) << 1);
        else cl = (unsigned char)((unsigned char)(0x40 - (al & 0x1F)) << 1);
        badPulse += SbPulse((unsigned char)f) != cl;
    }
    CHECK(!badPulse, "SbPulse = 56FDA0's al / cl steps (%d bad)", badPulse);
    CHECK(SbListBarColour(0, 0, 1) == 0x80600000u && SbListBarColour(0, 0, 0) == 0x80006060u, "unfocused: a = 0x60");
    CHECK(SbBarExec((const unsigned char *)"\xCC", 1, 1) == 0xFFFFFFFFu, "an unknown opcode is reported");
    for (k = 0; k < SB_BAR_LEN && kSbBarStock[k] == kSbBarBlue[k]; ++k) {}
    CHECK(k == 3 && sizeof kSbBarStock == sizeof kSbBarBlue && SB_BAR_LEN == 11, "same length, the first change at byte 3");
    CHECK(SbBarParse("Blue", &known) == SB_BAR_BLUE && known, "Blue");
    CHECK(SbBarParse("blue", &known) == SB_BAR_BLUE && known, "blue (case)");
    CHECK(SbBarParse("1", &known) == SB_BAR_BLUE && known, "1");
    CHECK(SbBarParse("Yellow", &known) == SB_BAR_YELLOW && known, "Yellow");
    CHECK(SbBarParse("YELLOW", &known) == SB_BAR_YELLOW && known, "YELLOW");
    CHECK(SbBarParse("0", &known) == SB_BAR_YELLOW && known, "0");
    CHECK(SbBarParse("", &known) == SB_BAR_BLUE && known, "empty: the default Blue");
    CHECK(SbBarParse(NULL, NULL) == SB_BAR_BLUE, "NULL: Blue");
    CHECK(SbBarParse("Red", &known) == SB_BAR_BLUE && !known, "unknown: Blue, flagged");
    CHECK(SbBarParse("2", &known) == SB_BAR_BLUE && !known, "2: Blue, flagged");
}

int main(void)
{
    TestLayout();
    TestRects();
    TestHits();
    TestMoveToArrows();
    TestArrows();
    TestLabel();
    TestRepeat();  // v3.48 SB348
    TestPick();
    TestTarget();
    TestMouseHold();
    TestBatchHold();
    TestSettle();
    TestListBar();  // v3.49 SB349
    printf("savebanks_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
