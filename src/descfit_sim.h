// v3.32 DESC332: Options description fit, the pure logic (no Windows, no game memory; test\descfit_sim_test.c).
//
// The Options description (0x524160, font 1) is one line at canvas x 64, y 360 (the caller 0x526653: x =
// (48 - W) * f + W + 16 with the fade f -> 1, W = the 512 canvas). The canvas ends at x 512 at every aspect ratio
// (4:3 fills the screen with it; Menus43 keeps it in the 4:3 middle of wide shapes; without Menus43 it stretches), so a
// line wider than 448 px ran off the screen at 4:3 (in wide shapes it ran into the pillar, where it stayed visible).
// DescFit measures a game string the way the layout 0x557660 does (first pass: per glyph g = 0xE0 - code the width
// [font + 0x10 + g] + the two signed kerning nibbles of [font + 0xF0 + g] + the letter spacing; space = [font + 0x10] +
// spacing; 0x7C = 22 / 16 units without spacing; a u16 character = the cell width + spacing; the last spacing of a line
// taken off; 640-wide units, canvas px = units / 1.25) and, when it is too wide:
//   1. wraps it at spaces into 2 (then 3) balanced lines (the narrowest widest line), if the box has room below the rows;
//   2. else tightens the letter spacing (2.5 -> 2.0 / 1.5 / 1.0 units, the game's own spacing value: a slight
//      horizontal squeeze without touching the glyphs), wrapped if there is room;
//   3. else shortens the last line at a word and ends it with "...".
// Strings the measure cannot follow (an existing newline, a sub-text, the keyboard-name icon F002) are left as they are.
#ifndef DESCFIT_SIM_H
#define DESCFIT_SIM_H

#define DF_MAXTOK 256
#define DF_MAXBYTES 1024
#define DF_MAXLINES 3

enum { DF_PASS = -1, DF_SAME = 0, DF_WRAP = 1, DF_TIGHT = 2, DF_SHORT = 3 };

typedef struct {
    const unsigned char *font;  // the game's font block: widths [0x10 + g], kerning nibbles [0xF0 + g]; NULL: 13 units a glyph
    float cellW;                // the font's cell width (0x6999B4 table, 640 units): a u16 character's advance
    int fontId;                 // 0x7C is 22 units in font 0, 16 in the others
} DfFont;

typedef struct {
    unsigned short code;
    unsigned short off, len;  // the token's bytes in the source
    unsigned char bytes;      // 1 = read in byte mode
    unsigned char sp;         // 1 = the letter spacing follows it
    float w;                  // its advance without the spacing (640 units)
} DfTok;

typedef struct {
    int n, endOff, endLen, endBytes;
    DfTok t[DF_MAXTOK];
} DfToks;

typedef struct {
    float maxW;        // canvas px a line may take
    float spacing;     // the letter spacing now (640 units; the game's is 2.5)
    float minSpacing;  // the tightest spacing allowed
    float y;           // the stock first line (canvas y: 360)
    float rowsBottom;  // the lowest row's text ends here (canvas y)
    float floorY;      // nothing of the description below this (canvas y)
    float lineH;       // a text line (canvas px)
    float gap;         // the least room between the rows and the description
} DfBox;

typedef struct {
    int mode;          // DF_*
    int lines;
    float spacing;     // the letter spacing to draw with
    float dy;          // added to the stock y (0 for one line)
    float wBefore;     // the string as one line at the stock spacing (canvas px)
    float wAfter;      // the widest line drawn (canvas px)
    int outLen;        // > 0: draw the rebuilt string (out), else the source
} DfPlan;

// TextNext (0x557660's reader): word mode reads u16; after 0x8000 byte mode, where 0xE0 takes the next two bytes as a
// word (low first) and a byte above 0xE0 is the high byte of a word.
static int DfNext(const unsigned char *s, int *pos, int bytes, int maxBytes, unsigned short *c)
{
    int p = *pos;
    if (bytes) {
        if (p >= maxBytes) return 0;
        *c = s[p++];
        if (*c == 0xE0) {
            if (p + 2 > maxBytes) return 0;
            *c = (unsigned short)(s[p] | s[p + 1] << 8);
            p += 2;
        } else if (*c > 0xE0) {
            if (p >= maxBytes) return 0;
            *c = (unsigned short)(*c << 8 | s[p++]);
        }
    } else {
        if (p + 2 > maxBytes) return 0;
        *c = (unsigned short)(s[p] | s[p + 1] << 8);
        p += 2;
    }
    *pos = p;
    return 1;
}

// 1 = measurable; 0 = drawn as it is (too long, unterminated, or a code whose width or line the measure cannot follow)
static int DfTokenize(const unsigned char *s, int maxBytes, const DfFont *f, DfToks *o)
{
    int pos = 0, bytes = 0;
    unsigned short c;
    o->n = 0;
    if (!s) return 0;
    if (maxBytes > DF_MAXBYTES) maxBytes = DF_MAXBYTES;
    for (;;) {
        const int at = pos, wasBytes = bytes;
        DfTok *t;
        if (!DfNext(s, &pos, bytes, maxBytes, &c)) return 0;
        if (c == 0xFFFF) {
            o->endOff = at, o->endLen = pos - at, o->endBytes = wasBytes;
            return 1;
        }
        if (o->n >= DF_MAXTOK) return 0;
        t = &o->t[o->n++];
        t->code = c, t->off = (unsigned short)at, t->len = (unsigned short)(pos - at), t->bytes = (unsigned char)wasBytes;
        t->sp = 0, t->w = 0.0f;
        if (c < 0x7FFF) {
            if (c >= 0xE0) t->w = f->cellW, t->sp = 1;
            else if (c == 0) t->w = f->font ? (float)f->font[0x10] : 13.0f, t->sp = 1;
            else if (c == 0x7C) t->w = f->fontId == 0 ? 22.0f : 16.0f;
            else {
                if (f->font) {
                    const int g = 0xE0 - c, k = f->font[0xF0 + g];
                    int lo = k & 0x0F, hi = k >> 4;
                    if (lo >= 8) lo -= 16;
                    if (hi >= 8) hi -= 16;
                    t->w = (float)(f->font[0x10 + g] + lo + hi);
                } else {
                    t->w = 13.0f;
                }
                t->sp = 1;
            }
        } else if ((c & 0xFF00) == 0xFF00) {
            if (c == 0xFFFD || c == 0xFFFA || c == 0xFFF6 || c == 0xFFF7 || (c >= 0xFFE0 && c <= 0xFFE9)) return 0;
            if (c == 0xFFFE) t->w = f->cellW, t->sp = 1;
            else if (c == 0xFFEF) t->w = (float)((int)f->cellW / 2), t->sp = 1;
        } else if ((c & 0xFF00) == 0xFC00) {
            t->w = -(float)(c & 0xFF);
        } else if ((c & 0xFF00) == 0xF000) {
            if (c == 0xF002) return 0;
            t->w = 35.0f * f->cellW / 24.0f, t->sp = 1;  // an icon: advance 0x23 in font 0, scaled by the cell width
        } else if ((c & 0xFF00) == 0xFD00) {
            t->w = (float)(c & 0xFF);
        } else if ((c & 0xFF00) == 0xFE00) {
            t->w = (float)(c & 0xFF), t->sp = 1;
        } else if (c == 0x8000) {
            bytes = 1;
        }
    }
}

// tokens [a, b) as one line, canvas px
static float DfLineW(const DfToks *k, int a, int b, float sp)
{
    float u = 0.0f;
    int i;
    for (i = a; i < b; ++i) u += k->t[i].w + (k->t[i].sp ? sp : 0.0f);
    if (u != 0.0f) u -= sp;
    return u / 1.25f;
}

static int DfIsBreak(const DfToks *k, int i) { return i > 0 && i < k->n - 1 && k->t[i].code == 0; }

// the best split into `lines` lines (breaks at spaces): the narrowest widest line, every line <= maxW; 1 = found
static int DfBest(const DfToks *k, int lines, float sp, float maxW, int brk[DF_MAXLINES - 1], float *worst)
{
    int i, j, found = 0;
    float best = 0.0f;
    if (lines <= 1) {
        const float w = DfLineW(k, 0, k->n, sp);
        if (w > maxW) return 0;
        *worst = w;
        return 1;
    }
    for (i = 1; i < k->n - 1; ++i) {
        if (!DfIsBreak(k, i)) continue;
        if (lines == 2) {
            const float a = DfLineW(k, 0, i, sp), b = DfLineW(k, i + 1, k->n, sp), m = a > b ? a : b;
            if (m <= maxW && (!found || m < best)) found = 1, best = m, brk[0] = i;
            continue;
        }
        for (j = i + 2; j < k->n - 1; ++j) {
            float a, b, c, m;
            if (!DfIsBreak(k, j)) continue;
            a = DfLineW(k, 0, i, sp), b = DfLineW(k, i + 1, j, sp), c = DfLineW(k, j + 1, k->n, sp);
            m = a > b ? a : b;
            if (c > m) m = c;
            if (m <= maxW && (!found || m < best)) found = 1, best = m, brk[0] = i, brk[1] = j;
        }
    }
    if (found) *worst = best;
    return found;
}

// the first line's canvas y for a block of n lines: the stock line when it fits above the floor, else lifted
static float DfY0(const DfBox *b, int n)
{
    float y0 = b->y;
    if (n > 1 && y0 + (float)n * b->lineH > b->floorY) y0 = b->floorY - (float)n * b->lineH;
    return y0;
}

// the highest a block may start: under the rows, but never required above the stock line (it is where the one line was)
static float DfTop(const DfBox *b)
{
    const float t = b->rowsBottom + b->gap;
    return t < b->y ? t : b->y;
}
// the most lines the box holds between the rows and the floor (1 at least: one line is always drawn where it was)
static int DfLinesRoom(const DfBox *b)
{
    int n, room = 1;
    for (n = 2; n <= DF_MAXLINES; ++n)
        if (DfY0(b, n) >= DfTop(b) - 0.01f && DfY0(b, n) + (float)n * b->lineH <= b->floorY + 0.01f) room = n;
    return room;
}

static int DfPut(unsigned char *out, int outN, int *o, const unsigned char *src, int len)
{
    if (*o + len > outN) return 0;
    memcpy(out + *o, src, (size_t)len);
    *o += len;
    return 1;
}
static int DfPutNewline(unsigned char *out, int outN, int *o, int bytes)
{
    static const unsigned char kB[2] = { 0xFF, 0xFD }, kW[2] = { 0xFD, 0xFF };  // byte mode FF FD; word mode u16 0xFFFD
    return DfPut(out, outN, o, bytes ? kB : kW, 2);
}

// rebuilds the string with newlines at brk[0 .. nb-1]; cut >= 0: tokens from `cut` on are dropped and "..." added
static int DfBuild(const unsigned char *s, const DfToks *k, const int *brk, int nb, int cut, unsigned char *out, int outN)
{
    int i, b = 0, o = 0, last = k->n;
    if (cut >= 0 && cut < last) last = cut;
    for (i = 0; i < last; ++i) {
        if (b < nb && i == brk[b]) {
            if (!DfPutNewline(out, outN, &o, k->t[i].bytes)) return 0;
            ++b;
            continue;
        }
        if (!DfPut(out, outN, &o, s + k->t[i].off, k->t[i].len)) return 0;
    }
    if (cut >= 0) {
        const int bytes = last > 0 ? k->t[last - 1].bytes : k->endBytes;
        static const unsigned char kDotB[3] = { 0xD2, 0xD2, 0xD2 }, kDotW[6] = { 0xD2, 0, 0xD2, 0, 0xD2, 0 };  // "..."
        if (!DfPut(out, outN, &o, bytes ? kDotB : kDotW, bytes ? 3 : 6)) return 0;
    }
    if (!DfPut(out, outN, &o, s + k->endOff, k->endLen)) return 0;
    return o;
}

// "...": three '.' glyphs (code 0xD2) at spacing sp, plus the spacing between the text and them (canvas px)
static float DfDotsW(const DfFont *f, float sp)
{
    float w = 13.0f;
    if (f->font) {
        const int g = 0xE0 - 0xD2, k = f->font[0xF0 + g];
        int lo = k & 0x0F, hi = k >> 4;
        if (lo >= 8) lo -= 16;
        if (hi >= 8) hi -= 16;
        w = (float)(f->font[0x10 + g] + lo + hi);
    }
    return 3.0f * (w + sp) / 1.25f;
}

// the shortened fallback: greedy lines at spacing sp, the last one cut at a word (else a letter) so that "..." fits
static int DfShorten(const unsigned char *s, const DfToks *k, const DfFont *f, int lines, float sp, float maxW,
                     unsigned char *out, int outN, float *worst, int *nLines)
{
    int brk[DF_MAXLINES - 1], nb = 0, start = 0, i, cut, lastSpace = -1;
    float w = 0.0f;
    *worst = 0.0f;
    *nLines = 1;
    while (nb < lines - 1) {  // full lines: the last space that keeps the line within maxW
        int best = -1;
        for (i = start + 1; i < k->n - 1; ++i)
            if (DfIsBreak(k, i) && DfLineW(k, start, i, sp) <= maxW) best = i;
        if (best < 0) break;
        w = DfLineW(k, start, best, sp);
        if (w > *worst) *worst = w;
        brk[nb++] = best;
        start = best + 1;
    }
    *nLines = nb + 1;
    if (DfLineW(k, start, k->n, sp) <= maxW) {  // the rest fits after all
        const float r = DfLineW(k, start, k->n, sp);
        if (r > *worst) *worst = r;
        return DfBuild(s, k, brk, nb, -1, out, outN);
    }
    cut = start;
    for (i = start + 1; i <= k->n; ++i) {
        if (DfLineW(k, start, i, sp) + DfDotsW(f, sp) > maxW) break;
        cut = i;
        if (i < k->n && k->t[i].code == 0) lastSpace = i;
    }
    if (lastSpace > start) cut = lastSpace;  // end at a word
    while (cut > start && k->t[cut - 1].code == 0) --cut;
    w = DfLineW(k, start, cut, sp) + (cut > start ? DfDotsW(f, sp) : DfDotsW(f, sp) - sp / 1.25f);
    if (w > *worst) *worst = w;
    return DfBuild(s, k, brk, nb, cut, out, outN);
}

// Plans the description. Draw `out` when p->outLen > 0, else the source; with p->spacing; p->dy added to the stock y.
static void DescFit(const unsigned char *s, int maxBytes, const DfFont *f, const DfBox *b, unsigned char *out, int outN,
                    DfPlan *p)
{
    static DfToks k;  // (one caller at a time: the game thread's menu draw; the test is single-threaded)
    int brk[DF_MAXLINES - 1], n, room, len;
    float sp, w;
    p->mode = DF_PASS, p->lines = 1, p->spacing = b->spacing, p->dy = 0.0f, p->wBefore = p->wAfter = 0.0f, p->outLen = 0;
    if (!DfTokenize(s, maxBytes, f, &k)) return;
    p->wBefore = p->wAfter = DfLineW(&k, 0, k.n, b->spacing);
    p->mode = DF_SAME;
    if (p->wBefore <= b->maxW) return;
    room = DfLinesRoom(b);
    for (n = 2; n <= room; ++n)  // 1. wrap at the game's spacing
        if (DfBest(&k, n, b->spacing, b->maxW, brk, &w) && (len = DfBuild(s, &k, brk, n - 1, -1, out, outN)) > 0) {
            p->mode = DF_WRAP, p->lines = n, p->wAfter = w, p->outLen = len, p->dy = DfY0(b, n) - b->y;
            return;
        }
    for (sp = b->spacing - 0.5f; sp >= b->minSpacing - 0.001f; sp -= 0.5f)  // 2. a slightly tighter spacing
        for (n = 1; n <= room; ++n)
            if (DfBest(&k, n, sp, b->maxW, brk, &w)) {
                len = n > 1 ? DfBuild(s, &k, brk, n - 1, -1, out, outN) : 0;
                if (n > 1 && len <= 0) continue;
                p->mode = DF_TIGHT, p->lines = n, p->spacing = sp, p->wAfter = w, p->outLen = len;
                p->dy = n > 1 ? DfY0(b, n) - b->y : 0.0f;
                return;
            }
    sp = b->minSpacing;  // 3. shortened
    len = DfShorten(s, &k, f, room, sp, b->maxW, out, outN, &w, &n);
    if (len <= 0) {  // no room in out: as it was
        p->mode = DF_PASS;
        return;
    }
    p->mode = DF_SHORT, p->lines = n, p->spacing = sp, p->wAfter = w, p->outLen = len;
    p->dy = n > 1 ? DfY0(b, n) - b->y : 0.0f;
}

#endif
