// v3.37 CP337: the performance overlay's pure code (perfhud.inc runs it in the game; test\perfhud_sim_test.c checks it).
// No Windows, no game memory: the rolling frame statistics (fps, frame time, the 1 % low over the last seconds), the
// readout's text lines and colours, the Direct3D chain's name for Option 4's description, and a 5 x 7 pixel font turned
// into rectangles (the overlay draws them as untextured quads).
#ifndef PERFHUD_SIM_H
#define PERFHUD_SIM_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PERF_RING 1024        // frame times kept: 5 s at 180 fps fit (900)
#define PERF_LOW_MS 5000.0f   // the 1 % low looks at this much of the latest play (frames newest first)
#define PERF_LOW_MIN 20       // fewer frames than this in the window: no 1 % low yet
#define PERF_SHOW_MS 500.0f   // the readout's numbers change twice a second (each line an average of that half second)
#define PERF_LINES 4
#define PERF_LINE_MAX 32

// ---------------------------------------------------------------- the frame times
typedef struct {
    float ms[PERF_RING];
    int head, n;              // the next slot, the frames held
} PerfRing;
static void PerfRingReset(PerfRing *r) { r->head = r->n = 0; }
// 0 = refused (not a frame length: not finite, 0 or less, a minute or more)
static int PerfRingPush(PerfRing *r, float ms)
{
    if (!(ms > 0.0f) || !(ms < 60000.0f)) return 0;
    r->ms[r->head] = ms;
    r->head = (r->head + 1) % PERF_RING;
    if (r->n < PERF_RING) ++r->n;
    return 1;
}
static int PerfCmpF(const void *a, const void *b)
{
    const float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
// The 1 % low of the latest windowMs of frames: the 99th percentile frame time (99 of 100 frames were at least this fast;
// MSI Afterburner's "1% low"), as fps. *p99 = that frame time (ms). 0 = not enough frames yet. *used = frames looked at.
static float PerfLowFps(const PerfRing *r, float windowMs, float *p99, int *used)
{
    static float tmp[PERF_RING];
    float sum = 0.0f;
    int k = 0, i, idx;
    if (p99) *p99 = 0.0f;
    if (used) *used = 0;
    while (k < r->n && sum < windowMs) {  // newest first
        i = (r->head - 1 - k + PERF_RING) % PERF_RING;
        tmp[k] = r->ms[i];
        sum += r->ms[i];
        ++k;
    }
    if (used) *used = k;
    if (k < PERF_LOW_MIN) return 0.0f;
    qsort(tmp, (size_t)k, sizeof tmp[0], PerfCmpF);
    idx = (int)ceilf(0.99f * (float)k) - 1;  // nearest rank
    if (idx < 0) idx = 0;
    if (idx > k - 1) idx = k - 1;
    if (p99) *p99 = tmp[idx];
    return tmp[idx] > 0.0f ? 1000.0f / tmp[idx] : 0.0f;
}

// ---------------------------------------------------------------- the half second shown
typedef struct {
    int frames;
    double ms, worst;         // the frames' total and the longest (ms)
    double own;               // Refined's per-frame work (the budget ledger's frame total, loghealth.inc)
    int ownN;
} PerfWin;
static void PerfWinReset(PerfWin *w) { memset(w, 0, sizeof *w); }
static void PerfWinFrame(PerfWin *w, float ms)
{
    ++w->frames;
    w->ms += ms;
    if (ms > w->worst) w->worst = ms;
}
static void PerfWinOwn(PerfWin *w, float ms)
{
    if (!(ms >= 0.0f) || !(ms < 60000.0f)) return;
    w->own += ms;
    ++w->ownN;
}

typedef struct {
    float fps, frameMs, lowFps, ownMs;  // ownMs < 0: no ledger frame in the half second (the front end: no HUD call)
    int vaUsedMb, vaTotalMb;           // 0 total: not read
    int target;                        // the game's frame target now ([0x5D5248]: 30 in menus, the chosen rate in play)
} PerfShown;
static void PerfShownFrom(const PerfWin *w, float lowFps, int vaUsed, int vaTotal, int target, PerfShown *s)
{
    s->fps = w->frames && w->ms > 0.0 ? (float)(1000.0 * w->frames / w->ms) : 0.0f;
    s->frameMs = w->frames ? (float)(w->ms / w->frames) : 0.0f;
    s->lowFps = lowFps;
    s->ownMs = w->ownN ? (float)(w->own / w->ownN) : -1.0f;
    s->vaUsedMb = vaUsed;
    s->vaTotalMb = vaTotal;
    s->target = target;
}

// colours (0xRRGGBB): at the target, a little under it, well under it; the address space's levels
#define PERF_RGB_TEXT 0xE8E4DCu
#define PERF_RGB_OK   0x7CE08Cu
#define PERF_RGB_WARN 0xF0C850u
#define PERF_RGB_BAD  0xF06050u
static unsigned PerfRateRgb(float fps, int target)
{
    if (target < 10 || target > 1000 || !(fps > 0.0f)) return PERF_RGB_TEXT;
    if (fps >= 0.95f * (float)target) return PERF_RGB_OK;
    if (fps >= 0.75f * (float)target) return PERF_RGB_WARN;
    return PERF_RGB_BAD;
}
static unsigned PerfVaRgb(int used, int total)
{
    if (total <= 0) return PERF_RGB_TEXT;
    if (used >= total - 200) return PERF_RGB_BAD;   // the log's ANOMALY starts near here (loghealth_sim.h LhVaStep)
    if (used >= total - 400) return PERF_RGB_WARN;
    return PERF_RGB_OK;
}
static float PerfClampShow(float v, float hi) { return !(v > 0.0f) ? 0.0f : v > hi ? hi : v; }
// The four lines (upper case: the font has no lower case) and their colours:
//   "60.0 FPS  16.7 MS" / "1% LOW 55.1 FPS" / "REFINED 1.23 MS" / "MEM 812 / 2048 MB"
static void PerfLines(const PerfShown *s, char out[PERF_LINES][PERF_LINE_MAX], unsigned rgb[PERF_LINES])
{
    const float fps = PerfClampShow(s->fps, 9999.0f), ms = PerfClampShow(s->frameMs, 9999.0f);
    sprintf_s(out[0], PERF_LINE_MAX, "%.1f FPS  %.1f MS", fps, ms);
    rgb[0] = PerfRateRgb(s->fps, s->target);
    if (s->lowFps > 0.0f) {
        sprintf_s(out[1], PERF_LINE_MAX, "1%% LOW %.1f FPS", PerfClampShow(s->lowFps, 9999.0f));
        rgb[1] = PerfRateRgb(s->lowFps, s->target);
    } else {
        strcpy_s(out[1], PERF_LINE_MAX, "1% LOW -");
        rgb[1] = PERF_RGB_TEXT;
    }
    if (s->ownMs >= 0.0f) sprintf_s(out[2], PERF_LINE_MAX, "REFINED %.2f MS", PerfClampShow(s->ownMs, 9999.0f));
    else strcpy_s(out[2], PERF_LINE_MAX, "REFINED -");
    rgb[2] = PERF_RGB_TEXT;
    if (s->vaTotalMb > 0) {
        const int u = s->vaUsedMb < 0 ? 0 : s->vaUsedMb > 99999 ? 99999 : s->vaUsedMb;
        const int t = s->vaTotalMb > 99999 ? 99999 : s->vaTotalMb;
        sprintf_s(out[3], PERF_LINE_MAX, "MEM %d / %d MB", u, t);
    } else {
        strcpy_s(out[3], PERF_LINE_MAX, "MEM -");
    }
    rgb[3] = PerfVaRgb(s->vaUsedMb, s->vaTotalMb);
}

// ---------------------------------------------------------------- the Direct3D chain (Option 4's description)
// folderD3d9: the d3d9.dll in use is the game folder's; conf: dgVoodoo.conf is there; api: its [General] OutputAPI (may
// be ""); d3d12 / d3d11: those system dlls are loaded (the device exists). "Direct3D: GOG -> dgVoodoo2 D3D12",
// "Direct3D: GOG -> D3D9", ... v3.39 PR339: variant = the SH4 Graphics Plus variant dgVoodoo.conf is (its manifest label,
// e.g. "Direct3D 11 (16-bit)"; NULL / "" = none): "Direct3D: GOG -> dgVoodoo2, Direct3D 11 (16-bit)"
static void PerfBackendName(int folderD3d9, int conf, const char *api, int d3d12, int d3d11, const char *variant, char *out, size_t n)
{
    const char *to = "";
    if (!folderD3d9) {
        sprintf_s(out, n, "Direct3D: GOG -> D3D9");
        return;
    }
    if (!conf) {
        sprintf_s(out, n, "Direct3D: GOG -> the game folder's d3d9.dll");
        return;
    }
    if (variant && variant[0]) {  // v3.39 PR339
        sprintf_s(out, n, "Direct3D: GOG -> dgVoodoo2, %s", variant);
        return;
    }
    if (api && !_strnicmp(api, "d3d12", 5)) to = " D3D12";
    else if (api && !_stricmp(api, "d3d11warp")) to = " D3D11 (WARP)";
    else if (api && !_strnicmp(api, "d3d11", 5)) to = " D3D11";
    else if (d3d12) to = " D3D12";  // "bestavailable" or not set: what got loaded
    else if (d3d11) to = " D3D11";
    sprintf_s(out, n, "Direct3D: GOG -> dgVoodoo2%s", to);
}

// ---------------------------------------------------------------- the 5 x 7 font
// Each glyph: 7 rows top to bottom, bit 4 = the leftmost of 5 columns. Upper case, digits and the signs the lines use.
typedef struct { char c; unsigned char row[7]; } PerfGlyph;
static const PerfGlyph kPerfFont[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'A', { 0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } }, { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 } }, { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } }, { '%', { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } },
    { '/', { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00 } }, { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } },
    { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } }, { '>', { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } },
    { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
};
#define PERF_GLYPHS ((int)(sizeof kPerfFont / sizeof kPerfFont[0]))
#define PERF_ADV 6            // a glyph's advance in font pixels (5 + 1 apart)
#define PERF_LINE_H 9         // a line's height in font pixels (7 + 2 apart)
static const PerfGlyph *PerfGlyphOf(char c)
{
    int i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (i = 0; i < PERF_GLYPHS; ++i)
        if (kPerfFont[i].c == c) return &kPerfFont[i];
    return NULL;
}

// A rectangle in pixels: x, y, w, h.
typedef struct { float x, y, w, h; } PerfRect;
// The text as rectangles: each row's runs of lit pixels, merged downwards with the same run of the rows below (the
// digit 1's stem is one rectangle). Its top-left at (x, y), u pixels a font pixel. Returns the rectangles written (at
// most max); *dropped += those that did not fit.
static int PerfTextRects(const char *t, float x, float y, float u, PerfRect *out, int max, int *dropped)
{
    int n = 0;
    for (; *t; ++t, x += PERF_ADV * u) {
        const PerfGlyph *g = PerfGlyphOf(*t);
        unsigned char used[7];
        int r;
        if (!g) continue;
        memset(used, 0, sizeof used);
        for (r = 0; r < 7; ++r) {
            int c = 0;
            while (c < 5) {
                const unsigned bit = 0x10u >> c;
                int c1, mask, r1;
                if (!(g->row[r] & bit) || (used[r] & bit)) {
                    ++c;
                    continue;
                }
                for (c1 = c; c1 < 5 && (g->row[r] & (0x10u >> c1)) && !(used[r] & (0x10u >> c1)); ++c1) {}
                mask = 0;
                {
                    int k;
                    for (k = c; k < c1; ++k) mask |= 0x10 >> k;
                }
                // the rows below that hold the whole run too (their other lit pixels become rectangles of their own)
                for (r1 = r + 1; r1 < 7; ++r1)
                    if ((g->row[r1] & mask) != mask || (used[r1] & mask)) break;
                {
                    int k;
                    for (k = r; k < r1; ++k) used[k] |= (unsigned char)mask;
                }
                if (n < max) {
                    out[n].x = x + (float)c * u;
                    out[n].y = y + (float)r * u;
                    out[n].w = (float)(c1 - c) * u;
                    out[n].h = (float)(r1 - r) * u;
                    ++n;
                } else if (dropped) {
                    ++*dropped;
                }
                c = c1;
            }
        }
    }
    return n;
}
static int PerfTextWidth(const char *t) { return t && *t ? (int)strlen(t) * PERF_ADV - 1 : 0; }  // font pixels
// The font pixel size for a target h pixels high: 1 below 720, 2 at 720 .. 1199 (1080: 2), 3 at 1200 .. 1679 (1440: 3), 5 at 2160
static int PerfScale(int h) { const int u = (h + 240) / 480; return u < 1 ? 1 : u > 8 ? 8 : u; }

// ---------------------------------------------------------------- v3.38 HB338: the readout's place
// USER_FEEDBACK_v337 item 17: at 4:3 the readout sat on the health HUD (Playable Eileen's chain ring and plus, drawn
// where the stock gauge is). The HUD's box in back-buffer pixels: Playable Eileen's ([HUD] X / Y / Scale / Aspect, its
// ring 2 x 45.5 units: SH4EileenWeapons.cpp HudBuild / HudVert, HUD_RING_OUT) while it is loaded with a Style other than
// Original, else the stock gauge (frame sprite at canvas (22, 22) 200 x 48 and its 1-unit shake, the charge pie inside it:
// hudgauge_50dc60.c 0x50DEA0 / 0x50EC40). Both map canvas 512 x 448 into the 4:3 interface area squeezed by S
// (SH4WM_Scale2D). The readout stays in the corner unless its box (decided for PERF_FIT_CHARS characters, so it never
// jumps with the numbers) meets the HUD's; then it goes under the HUD (8 canvas units below), else right of it, else it
// stays in the corner. Always inside the back buffer (at 4:3 the pillarbox is outside it). Decided once per size, S and
// Playable Eileen ini change (perfhud.inc PerfPlaceCheck).
#define PERF_FIT_CHARS 20     // the lines are 8..18 characters; the place is decided for this width
#define PERF_STEP_CANVAS 8.0f  // the gap under / right of the HUD (canvas units)
typedef struct { float x0, y0, x1, y1; } PerfBox;
enum { PERF_AT_CORNER, PERF_AT_BELOW, PERF_AT_RIGHT, PERF_AT_OVER };
static float PerfCanvasX(float cx, float W, float S) { return 0.5f * W + (cx - 256.0f) * W / (512.0f * S); }
static float PerfCanvasY(float cy, float H) { return cy * H / 448.0f; }
static float PerfClampF(float v, float lo, float hi) { return !(v == v) ? lo : v < lo ? lo : v > hi ? hi : v; }
static float PerfSqueeze(float S) { return S > 1.0f && S < 4.0f ? S : 1.0f; }  // (as PE's Hud43)
// Playable Eileen's HUD: canvas x X*A .. (X + 91 sc)*A, y Y .. Y + 91 sc, A = its Aspect (0 = Auto: round on screen), plus
// 2 x sc units for its hit shake (1.5 x sc) and halo; X / Y / sc clamped as PE reads them
static PerfBox PerfPeHudBox(float W, float H, float S, float X, float Y, float sc, float aspect)
{
    float A, m;
    PerfBox b;
    S = PerfSqueeze(S);
    X = PerfClampF(X, -200.0f, 700.0f), Y = PerfClampF(Y, -200.0f, 600.0f), sc = PerfClampF(sc, 0.25f, 4.0f);
    A = aspect > 0.1f && aspect < 4.0f ? aspect : (H / 448.0f) / (W / 512.0f) * S;
    m = 2.0f * sc;
    b.x0 = PerfCanvasX((X - m) * A, W, S), b.x1 = PerfCanvasX((X + 91.0f * sc + m) * A, W, S);
    b.y0 = PerfCanvasY(Y - m, H), b.y1 = PerfCanvasY(Y + 91.0f * sc + m, H);
    return b;
}
static PerfBox PerfStockGaugeBox(float W, float H, float S)  // canvas 21..223 x 21..71
{
    PerfBox b;
    S = PerfSqueeze(S);
    b.x0 = PerfCanvasX(21.0f, W, S), b.x1 = PerfCanvasX(223.0f, W, S), b.y0 = PerfCanvasY(21.0f, H), b.y1 = PerfCanvasY(71.0f, H);
    return b;
}
// the readout's dark box with its top-left at (bx, by), u pixels a font pixel, PERF_FIT_CHARS wide
static PerfBox PerfReadoutBox(float bx, float by, int u)
{
    PerfBox b;
    b.x0 = bx, b.y0 = by;
    b.x1 = bx + (float)((PERF_FIT_CHARS * PERF_ADV - 1 + 6) * u);
    b.y1 = by + (float)((PERF_LINES * PERF_LINE_H - 2 + 6) * u);
    return b;
}
static int PerfBoxesMeet(const PerfBox *a, const PerfBox *b, float gap)
{
    return !(a->x1 + gap <= b->x0 || b->x1 + gap <= a->x0 || a->y1 + gap <= b->y0 || b->y1 + gap <= a->y0);
}
// Where the readout's box goes (W x H back-buffer pixels; hud NULL = none): *bx / *by = its top-left. Returns PERF_AT_*.
static int PerfPlace(float W, float H, const PerfBox *hud, float *bx, float *by)
{
    const int u = PerfScale((int)H);
    const float gap = (float)u, step = PerfCanvasY(PERF_STEP_CANVAS, H), edge = 3.0f * (float)u;
    PerfBox b = PerfReadoutBox(edge, edge, u);
    *bx = edge, *by = edge;
    if (!hud || !PerfBoxesMeet(&b, hud, gap)) return PERF_AT_CORNER;
    b = PerfReadoutBox(edge, hud->y1 + step, u);
    if (b.y1 <= H - edge) {
        *by = hud->y1 + step;
        return PERF_AT_BELOW;
    }
    b = PerfReadoutBox(hud->x1 + step, edge, u);
    if (b.x1 <= W - edge) {
        *bx = hud->x1 + step;
        return PERF_AT_RIGHT;
    }
    return PERF_AT_OVER;
}

#endif
