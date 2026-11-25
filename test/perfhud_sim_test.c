// v3.37 CP337: native test of src/perfhud_sim.h (staging/v337_feedback/w3_camera_perf/PLAN.md, Tests).
// 1. The ring: wraps at PERF_RING, refuses non-frames (0, negative, NaN, a minute).
// 2. The 1 % low: the 99th-percentile frame time of the latest 5 s (newest first), known distributions, too few frames,
//    the window cut (old slow frames outside 5 s do not count), a 180 fps run fills the window inside the ring.
// 3. The half second shown and the lines: fps / ms / Refined / address space text and colours, the "-" cases, clamps.
// 4. The Direct3D chain's names (every case the detection can give) fit Option 4's description with its base text.
// 5. The font: every character the lines can hold has a glyph; the rectangles cover each glyph's lit pixels exactly
//    once (rasterised back); the worst line fits the draw buffer; the scale per height.
// Build and run: test\build_perf_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/perfhud_sim.h"

static int g_fail, g_ok;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
    else ++g_ok;
}

static void TestRing(void)
{
    static PerfRing r;
    int i, ok = 1;
    PerfRingReset(&r);
    Check(!PerfRingPush(&r, 0.0f) && !PerfRingPush(&r, -3.0f) && !PerfRingPush(&r, NAN) && !PerfRingPush(&r, 60000.0f) && r.n == 0,
          "ring: 0, negative, NaN and a minute are not frames");
    for (i = 0; i < PERF_RING + 10; ++i) PerfRingPush(&r, (float)(i + 1));
    for (i = 0; i < PERF_RING; ++i) {  // the newest PERF_RING remain: values 11 .. PERF_RING + 10
        const float v = r.ms[(r.head - 1 - i + PERF_RING) % PERF_RING];
        if (v != (float)(PERF_RING + 10 - i)) ok = 0;
    }
    Check(r.n == PERF_RING && ok, "ring: wraps, keeps the newest 1024 in order");
}

static void TestLow(void)
{
    static PerfRing r;
    char msg[200];
    float p99, low;
    int used, i;
    PerfRingReset(&r);
    for (i = 0; i < 10; ++i) PerfRingPush(&r, 16.7f);
    low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
    Check(low == 0.0f && used == 10, "low: fewer than 20 frames: no 1% low yet");
    PerfRingReset(&r);
    for (i = 0; i < 300; ++i) PerfRingPush(&r, 1000.0f / 60.0f);  // 5 s at 60
    low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
    sprintf_s(msg, sizeof msg, "low: a steady 60 fps: 1%% low %.2f (p99 %.2f ms, %d frames)", low, p99, used);
    Check(fabsf(low - 60.0f) < 0.01f && used == 300, msg);
    PerfRingReset(&r);
    for (i = 0; i < 297; ++i) PerfRingPush(&r, 10.0f);  // 300 frames, 3 of them 50 ms (1 %): p99 = the 297th of 300 = 10 ms
    for (i = 0; i < 3; ++i) PerfRingPush(&r, 50.0f);
    low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
    sprintf_s(msg, sizeof msg, "low: 1 %% of frames at 50 ms: p99 %.1f ms (nearest rank 297 of 300 = 10 ms), 1%% low %.1f", p99, low);
    Check(fabsf(p99 - 10.0f) < 1e-3f, msg);
    PerfRingPush(&r, 50.0f);  // a 4th slow one inside the window: now more than 1 %
    low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
    sprintf_s(msg, sizeof msg, "low: 4 slow frames in %d: p99 %.1f ms, 1%% low %.1f fps", used, p99, low);
    Check(fabsf(p99 - 50.0f) < 1e-3f && fabsf(low - 20.0f) < 1e-3f, msg);
    {   // the window: 20 old 100 ms frames, then 5 s of 10 ms frames: the old ones are outside the 5 s looked at
        PerfRingReset(&r);
        for (i = 0; i < 20; ++i) PerfRingPush(&r, 100.0f);
        for (i = 0; i < 500; ++i) PerfRingPush(&r, 10.0f);
        low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
        sprintf_s(msg, sizeof msg, "low: old slow frames outside the last 5 s do not count (%d frames used, p99 %.1f ms)", used, p99);
        Check(used == 500 && fabsf(p99 - 10.0f) < 1e-3f, msg);
    }
    {   // 180 fps: 5 s = 900 frames, inside the ring
        PerfRingReset(&r);
        for (i = 0; i < 2000; ++i) PerfRingPush(&r, 1000.0f / 180.0f);
        low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
        sprintf_s(msg, sizeof msg, "low: 180 fps: %d frames looked at (want 900 or 901), 1%% low %.1f", used, low);
        Check(used >= 900 && used <= 901 && fabsf(low - 180.0f) < 0.1f, msg);
    }
    {   // a random mix against a brute-force percentile
        static float all[PERF_RING];
        unsigned seed = 7;
        int k, ok = 1;
        for (k = 0; k < 20; ++k) {
            int n = 0, j;
            float want;
            PerfRingReset(&r);
            for (i = 0; i < 400; ++i) {
                seed = seed * 1664525u + 1013904223u;
                PerfRingPush(&r, 5.0f + (float)((seed >> 8) % 4000) / 100.0f);
            }
            low = PerfLowFps(&r, PERF_LOW_MS, &p99, &used);
            {   // brute force: the newest frames up to 5 s, sorted
                float sum = 0.0f;
                for (j = 0; j < r.n && sum < PERF_LOW_MS; ++j) {
                    all[n++] = r.ms[(r.head - 1 - j + PERF_RING) % PERF_RING];
                    sum += all[n - 1];
                }
                qsort(all, (size_t)n, sizeof all[0], PerfCmpF);
                want = all[(int)ceil(0.99 * n) - 1];
            }
            if (n != used || fabsf(want - p99) > 1e-4f) ok = 0;
        }
        Check(ok, "low: 20 random runs: the same frames and the same nearest-rank p99 as a brute force");
    }
}

static void TestLines(void)
{
    PerfWin w;
    PerfShown sh;
    char L[PERF_LINES][PERF_LINE_MAX], msg[200];
    unsigned rgb[PERF_LINES];
    int i;
    PerfWinReset(&w);
    for (i = 0; i < 30; ++i) PerfWinFrame(&w, 1000.0f / 60.0f);
    PerfWinOwn(&w, 1.0f);
    PerfWinOwn(&w, 1.5f);
    PerfWinOwn(&w, -1.0f);  // not a value
    PerfShownFrom(&w, 55.12f, 812, 2048, 60, &sh);
    PerfLines(&sh, L, rgb);
    sprintf_s(msg, sizeof msg, "lines: \"%s\" / \"%s\" / \"%s\" / \"%s\"", L[0], L[1], L[2], L[3]);
    Check(!strcmp(L[0], "60.0 FPS  16.7 MS") && !strcmp(L[1], "1% LOW 55.1 FPS") && !strcmp(L[2], "REFINED 1.25 MS") &&
              !strcmp(L[3], "MEM 812 / 2048 MB"), msg);
    Check(rgb[0] == PERF_RGB_OK && rgb[1] == PERF_RGB_WARN && rgb[2] == PERF_RGB_TEXT && rgb[3] == PERF_RGB_OK,
          "lines: colours: 60 of 60 green, 55.1 of 60 (under 95 %) yellow, Refined plain, 812 of 2048 MB green");
    PerfShownFrom(&w, 50.0f, 1700, 2048, 60, &sh);
    PerfLines(&sh, L, rgb);
    Check(rgb[1] == PERF_RGB_WARN && rgb[3] == PERF_RGB_WARN, "lines: 50 of 60 yellow; 1700 of 2048 MB yellow");
    PerfShownFrom(&w, 40.0f, 1900, 2048, 60, &sh);
    PerfLines(&sh, L, rgb);
    Check(rgb[1] == PERF_RGB_BAD && rgb[3] == PERF_RGB_BAD, "lines: 40 of 60 red; 1900 of 2048 MB red");
    PerfShownFrom(&w, 30.0f, 812, 2048, 30, &sh);
    PerfLines(&sh, L, rgb);
    Check(rgb[0] == PERF_RGB_OK, "lines: in a menu the target is 30: 60 fps there is green");
    PerfWinReset(&w);
    PerfShownFrom(&w, 0.0f, 0, 0, 60, &sh);
    PerfLines(&sh, L, rgb);
    sprintf_s(msg, sizeof msg, "lines: nothing yet: \"%s\" / \"%s\" / \"%s\" / \"%s\"", L[0], L[1], L[2], L[3]);
    Check(!strcmp(L[0], "0.0 FPS  0.0 MS") && !strcmp(L[1], "1% LOW -") && !strcmp(L[2], "REFINED -") && !strcmp(L[3], "MEM -"), msg);
    PerfWinReset(&w);
    PerfWinFrame(&w, 0.001f);
    PerfShownFrom(&w, 1.0e7f, 123456, 999999, 60, &sh);
    PerfLines(&sh, L, rgb);
    sprintf_s(msg, sizeof msg, "lines: huge values are clamped and fit (%s / %s / %s)", L[0], L[1], L[3]);
    Check(strlen(L[0]) < PERF_LINE_MAX && strstr(L[0], "9999.0 FPS") && strstr(L[1], "9999.0") && !strcmp(L[3], "MEM 99999 / 99999 MB"), msg);
}

static void TestBackend(void)
{
    static const struct { int folder, conf; const char *api; int d12, d11; const char *variant, *want; } kT[] = {
        { 0, 0, "", 0, 0, NULL, "Direct3D: GOG -> D3D9" },
        { 0, 1, "d3d12_fl12_0", 1, 0, NULL, "Direct3D: GOG -> D3D9" },  // the system d3d9 is in use: the conf does not matter
        { 1, 0, "", 0, 0, NULL, "Direct3D: GOG -> the game folder's d3d9.dll" },
        { 1, 1, "d3d12_fl12_0", 1, 0, NULL, "Direct3D: GOG -> dgVoodoo2 D3D12" },  // the user's set-up (2.87.6)
        { 1, 1, "d3d12_fl11_0", 0, 0, NULL, "Direct3D: GOG -> dgVoodoo2 D3D12" },
        { 1, 1, "d3d11_fl11_0", 0, 1, NULL, "Direct3D: GOG -> dgVoodoo2 D3D11" },
        { 1, 1, "d3d11warp", 0, 1, NULL, "Direct3D: GOG -> dgVoodoo2 D3D11 (WARP)" },
        { 1, 1, "bestavailable", 1, 1, NULL, "Direct3D: GOG -> dgVoodoo2 D3D12" },
        { 1, 1, "", 0, 1, NULL, "Direct3D: GOG -> dgVoodoo2 D3D11" },
        { 1, 1, "", 0, 0, NULL, "Direct3D: GOG -> dgVoodoo2" },
        // v3.39 PR339: a Graphics Plus variant is named by its manifest label (the user's: Direct3D 11 (16-bit))
        { 1, 1, "d3d11_fl11_0", 0, 1, "Direct3D 11 (16-bit)", "Direct3D: GOG -> dgVoodoo2, Direct3D 11 (16-bit)" },
        { 1, 1, "d3d12_fl12_0", 1, 0, "Direct3D 12", "Direct3D: GOG -> dgVoodoo2, Direct3D 12" },
        { 1, 1, "", 0, 0, "Graphics Plus d3d12_hdr", "Direct3D: GOG -> dgVoodoo2, Graphics Plus d3d12_hdr" },
        { 1, 1, "d3d11_fl11_0", 0, 1, "", "Direct3D: GOG -> dgVoodoo2 D3D11" },  // "" = no variant
        { 1, 1, "", 0, 0, "Graphics Plus abcdefghijklmnopqrstuvwxyz01234", "Direct3D: GOG -> dgVoodoo2, Graphics Plus abcdefghijklmnopqrstuvwxyz01234" },  // the longest (14 + 31)
        { 0, 1, "d3d11", 0, 1, "Direct3D 11", "Direct3D: GOG -> D3D9" },  // the system d3d9: never a variant
        { 1, 0, "", 0, 0, "Direct3D 11", "Direct3D: GOG -> the game folder's d3d9.dll" },  // no conf: never a variant
    };
    char out[96], msg[200], full[200];
    int i, longest = 0;
    for (i = 0; i < (int)(sizeof kT / sizeof kT[0]); ++i) {
        PerfBackendName(kT[i].folder, kT[i].conf, kT[i].api, kT[i].d12, kT[i].d11, kT[i].variant, out, sizeof out);  // v3.39 PR339: + variant
        sprintf_s(msg, sizeof msg, "backend: d3d9 %s, conf %d, OutputAPI \"%s\", d3d12 %d d3d11 %d -> \"%s\"", kT[i].folder ? "folder" : "system",
                  kT[i].conf, kT[i].api, kT[i].d12, kT[i].d11, out);
        Check(!strcmp(out, kT[i].want), msg);
        sprintf_s(full, sizeof full, "Sets the shape of the picture. %s.", out);
        if ((int)strlen(full) > longest) longest = (int)strlen(full);
    }
    sprintf_s(msg, sizeof msg, "backend: the longest description is %d characters (perfhud.inc's text: 112 bytes = 108 characters)", longest);
    Check(longest <= 108, msg);
}

static void TestFont(void)
{
    static const char kChars[] = "0123456789.%/:- >ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static PerfRect rc[4096];
    char msg[200];
    int i, ok = 1, bad = 0, worst = 0;
    for (i = 0; kChars[i]; ++i)
        if (!PerfGlyphOf(kChars[i])) ok = 0;
    Check(ok, "font: every digit, letter and sign the lines use has a glyph");
    {   // the exact cover: rasterise the rectangles of each glyph back (u = 1) and compare with its bitmap
        for (i = 0; i < PERF_GLYPHS; ++i) {
            char t[2] = { kPerfFont[i].c, 0 };
            unsigned char hit[7][5];
            int n, k, x, y, over = 0;
            memset(hit, 0, sizeof hit);
            n = PerfTextRects(t, 0.0f, 0.0f, 1.0f, rc, 4096, NULL);
            if (n > worst) worst = n;
            for (k = 0; k < n; ++k)
                for (y = (int)rc[k].y; y < (int)(rc[k].y + rc[k].h); ++y)
                    for (x = (int)rc[k].x; x < (int)(rc[k].x + rc[k].w); ++x) {
                        if (x < 0 || x >= 5 || y < 0 || y >= 7) { over = 1; continue; }
                        if (hit[y][x]++) over = 1;
                    }
            for (y = 0; y < 7; ++y)
                for (x = 0; x < 5; ++x)
                    if ((hit[y][x] != 0) != ((kPerfFont[i].row[y] & (0x10 >> x)) != 0)) over = 1;
            if (over) {
                ++bad;
                printf("      glyph '%c' is not covered exactly\n", kPerfFont[i].c);
            }
        }
        sprintf_s(msg, sizeof msg, "font: the rectangles cover every glyph's pixels exactly once (%d glyphs, at most %d rectangles one)",
                  PERF_GLYPHS, worst);
        Check(!bad, msg);
    }
    {   // the widest the lines get, in rectangles: within the draw buffer (perfhud.inc PERF_MAXR 1024)
        const char *lines[4] = { "9999.0 FPS  9999.0 MS", "1% LOW 9999.0 FPS", "REFINED 9999.00 MS", "MEM 99999 / 99999 MB" };
        int total = 0, dropped = 0;
        for (i = 0; i < 4; ++i) total += PerfTextRects(lines[i], 0.0f, 0.0f, 3.0f, rc, 4096, &dropped);
        sprintf_s(msg, sizeof msg, "font: the widest readout is %d rectangles (the buffer holds 1024), widest line %d font pixels", total,
                  PerfTextWidth(lines[3]));
        Check(total < 1024 && !dropped, msg);
        dropped = 0;
        PerfTextRects("8888888888", 0.0f, 0.0f, 1.0f, rc, 5, &dropped);
        Check(dropped > 0, "font: a full buffer counts what it dropped (never writes past it)");
    }
    {   // positions: glyph k starts at x0 + 6 k u
        int n = PerfTextRects("1 1", 10.0f, 20.0f, 2.0f, rc, 4096, NULL), k, sawSecond = 0;
        for (k = 0; k < n; ++k)
            if (rc[k].x >= 10.0f + 12.0f * 2.0f) sawSecond = 1;
        Check(sawSecond && rc[0].y >= 20.0f, "font: the advance is 6 font pixels (a space between: 12), the top at y");
    }
    Check(PerfScale(480) == 1 && PerfScale(720) == 2 && PerfScale(1080) == 2 && PerfScale(1440) == 3 && PerfScale(2160) == 5 &&
              PerfScale(0) == 1 && PerfScale(100000) == 8, "font: the pixel size: 480 1, 720 / 1080 2, 1440 3, 2160 5 (1 .. 8)");
}

// v3.38 HB338: the readout's place (never over the health HUD), the user's sizes and Playable Eileen's [HUD] keys
static float S43(float W, float H) { return W / H > 4.0f / 3.0f + 1e-3f ? (W / H) / (4.0f / 3.0f) : 1.0f; }
static void TestPlace(void)
{
    static const struct { int W, H; int wantPe, wantStock; } kS[] = {
        { 2048, 1536, PERF_AT_BELOW, PERF_AT_BELOW },   // 4:3, the user's screenshot
        { 1024, 768, PERF_AT_BELOW, PERF_AT_BELOW },    // 4:3
        { 1920, 1200, PERF_AT_BELOW, PERF_AT_BELOW },   // 16:10
        { 1280, 720, PERF_AT_BELOW, PERF_AT_BELOW },    // 16:9: u = 2, the box reaches past the HUD's left edge
        { 1920, 1080, PERF_AT_CORNER, PERF_AT_CORNER }, // 16:9: clear
        { 2560, 1440, PERF_AT_CORNER, PERF_AT_CORNER }, // 16:9: clear (375 px vs 391)
    };
    char msg[256];
    int i, k;
    PerfBox pe = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 24.0f, 24.0f, 1.0f, 0.0f);
    sprintf_s(msg, sizeof msg, "place: PE's HUD at 2048x1536 (X 24, Y 24, Scale 1): %.0f..%.0f x %.0f..%.0f px (the ring 82..394, + shake)",
              pe.x0, pe.x1, pe.y0, pe.y1);
    Check(fabsf(pe.x0 - 75.4f) < 1.0f && fabsf(pe.x1 - 401.0f) < 1.0f && fabsf(pe.y0 - 75.4f) < 1.0f && fabsf(pe.y1 - 401.0f) < 1.0f, msg);
    for (i = 0; i < (int)(sizeof kS / sizeof kS[0]); ++i)
        for (k = 0; k < 2; ++k) {
            const float W = (float)kS[i].W, H = (float)kS[i].H, S = S43(W, H);
            const PerfBox hud = k ? PerfStockGaugeBox(W, H, S) : PerfPeHudBox(W, H, S, 24.0f, 24.0f, 1.0f, 0.0f);
            float bx, by;
            const int at = PerfPlace(W, H, &hud, &bx, &by), u = PerfScale(kS[i].H);
            const PerfBox rb = PerfReadoutBox(bx, by, u);
            const int want = k ? kS[i].wantStock : kS[i].wantPe;
            sprintf_s(msg, sizeof msg, "place: %dx%d %s: %s at %.0f, %.0f (box to %.0f, %.0f; HUD %.0f..%.0f x %.0f..%.0f)",
                      kS[i].W, kS[i].H, k ? "stock gauge" : "PE HUD", at == PERF_AT_CORNER ? "corner" : at == PERF_AT_BELOW ? "under it" :
                      at == PERF_AT_RIGHT ? "right of it" : "OVER it", bx, by, rb.x1, rb.y1, hud.x0, hud.x1, hud.y0, hud.y1);
            Check(at == want && !PerfBoxesMeet(&rb, &hud, 0.0f) && rb.x0 >= 0.0f && rb.y0 >= 0.0f && rb.x1 <= W && rb.y1 <= H, msg);
        }
    {   // the same answer every time for the same inputs (decided once: no jumping); the corner when nothing is near
        float bx1, by1, bx2, by2;
        const PerfBox hud = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 24.0f, 24.0f, 1.0f, 0.0f);
        PerfPlace(2048.0f, 1536.0f, &hud, &bx1, &by1);
        PerfPlace(2048.0f, 1536.0f, &hud, &bx2, &by2);
        Check(bx1 == bx2 && by1 == by2 && bx1 == 9.0f, "place: deterministic; under the HUD it keeps the left edge (3u = 9 px)");
        Check(PerfPlace(2048.0f, 1536.0f, NULL, &bx1, &by1) == PERF_AT_CORNER && bx1 == 9.0f && by1 == 9.0f, "place: no HUD: the corner (3u, 3u)");
    }
    {   // PE's keys: Scale 1.5 at 4:3 goes further down; a HUD moved to the bottom leaves the corner; X / Y / Scale clamped
        float bx, by;
        const PerfBox big = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 24.0f, 24.0f, 1.5f, 0.0f);
        const PerfBox low = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 24.0f, 330.0f, 1.0f, 0.0f);
        const PerfBox odd = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 99999.0f, -99999.0f, 100.0f, 0.0f);
        const PerfBox clamp = PerfPeHudBox(2048.0f, 1536.0f, 1.0f, 700.0f, -200.0f, 4.0f, 0.0f);
        Check(PerfPlace(2048.0f, 1536.0f, &big, &bx, &by) == PERF_AT_BELOW && by > big.y1 && big.y1 > 550.0f,
              "place: [HUD] Scale 1.5: under the bigger ring");
        Check(PerfPlace(2048.0f, 1536.0f, &low, &bx, &by) == PERF_AT_CORNER, "place: [HUD] Y 330 (bottom left): the corner is free");
        Check(odd.x0 == clamp.x0 && odd.y0 == clamp.y0 && odd.x1 == clamp.x1, "place: X / Y / Scale clamped as PE reads them");
    }
    {   // a HUD too tall to go under: right of it; too wide as well: the corner over it (logged as an anomaly)
        float bx, by;
        PerfBox tall, all;
        tall.x0 = 0.0f, tall.y0 = 0.0f, tall.x1 = 300.0f, tall.y1 = 700.0f;
        all.x0 = 0.0f, all.y0 = 0.0f, all.x1 = 1000.0f, all.y1 = 700.0f;
        Check(PerfPlace(1024.0f, 768.0f, &tall, &bx, &by) == PERF_AT_RIGHT && bx > 300.0f && by == 6.0f, "place: no room under: right of it");
        Check(PerfPlace(1024.0f, 768.0f, &all, &bx, &by) == PERF_AT_OVER && bx == 6.0f, "place: no room anywhere: the corner (over)");
    }
    {   // Menus43 off at 16:9 (S 1: the 2D stretched): PE's ring at the left edge: under it
        float bx, by;
        const PerfBox hud = PerfPeHudBox(1920.0f, 1080.0f, 1.0f, 24.0f, 24.0f, 1.0f, 0.0f);
        Check(PerfPlace(1920.0f, 1080.0f, &hud, &bx, &by) == PERF_AT_BELOW, "place: 16:9 with Menus43 off: under PE's HUD");
        Check(PerfSqueeze(0.5f) == 1.0f && PerfSqueeze(1.333f) == 1.333f && PerfSqueeze(9.0f) == 1.0f, "place: S outside 1 .. 4 reads as 1 (PE's Hud43)");
    }
}

int main(void)
{
    TestRing();
    TestLow();
    TestLines();
    TestBackend();
    TestFont();
    TestPlace();  // v3.38 HB338
    printf("%s: %d passed, %d failed\n", g_fail ? "FAIL" : "ALL OK", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
