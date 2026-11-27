// v3.56 PV356: native test of src\preview_sim.h (test\build_pv_test.cmd): the capture rule (which layers a capture frame
// needs, which lists the preview collects itself: never twice with a live look; why none), the page's backdrop and the
// layer over it (the pause's only for its own picture, the scene's for Thermal / Spirit Vision), PreviewScene's values,
// the heat key shader (proven forms: PsLint; black keeps the world, a hot body covers it), the title scene at 4:3 ..
// 32:9 and 480 .. 2160 lines (built, nothing lost, every vertex finite and near the screen, the figure in the page's
// clear column between the title and the description, the ghost's lower half behind the cabinet, the figure hot in
// Thermal, the ghost's glow additive and brighter than its own look), the soul fire (sprites at the chest, both batches).
// Usage: build_pv_test.cmd [--dump DIR] (writes pv_scene.txt for tools\pv_mockup.py)
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/preview_sim.h"
#include "../src/texpack_core.h"  // v3.58 PV358: TpDdsParse (the stock picture as FxDdsLoad reads it)

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static PvCapIn Cap(void)
{
    PvCapIn in;
    memset(&in, 0, sizeof in);
    in.on = in.shOn = in.hooks = in.gameplay = in.heatOk = in.edgesOk = 1;
    in.capture = 1;
    return in;
}
static void TestCapture(void)
{
    PvCapIn in = Cap();
    int why;
    CHECK(PvNeed(&in) == (PVL_HEAT | PVL_SPIRIT), "a capture frame in play needs both layers");
    CHECK(PvCollectMask(&in) == (PVL_HEAT | PVL_SPIRIT), "no live look: the preview collects both lists");
    in.heatLive = 1;
    CHECK(PvCollectMask(&in) == PVL_SPIRIT && PvNeed(&in) == (PVL_HEAT | PVL_SPIRIT), "Thermal live: it collects its bodies, the preview the ghosts");
    in.heatLive = 0, in.ghostLive = 1;
    CHECK(PvCollectMask(&in) == PVL_HEAT, "Spirit Vision live: the preview collects the bodies only");
    in = Cap(), in.phase = 1;
    CHECK(PvNeed(&in) == PVL_HEAT, "a real-time cut-scene: heat only (ghosts in play only, FXV2-004)");
    in.phase = 2;
    CHECK(PvNeed(&in) == 0, "a paused frame: nothing");
    in = Cap(), in.capture = 2;
    CHECK(PvNeed(&in) == 0, "a frame over the frozen copy (picture 2): nothing");
    in = Cap(), in.capture = 0;
    CHECK(PvNeed(&in) == 0 && PvCollectMask(&in) == 0, "an ordinary frame of play: nothing collected (no cost)");
    in = Cap(), in.on = 0;
    CHECK(PvNeed(&in) == 0 && !PvCaptureMask(&in, 1, 1, 1, &why) && why == PVW_OFF, "PreviewLayers=0: off");
    in = Cap(), in.shOn = 0;
    CHECK(!PvCaptureMask(&in, 1, 1, 1, &why) && why == PVW_NOTARGETS, "no device-reset hook: no targets of ours");
    in = Cap(), in.hooks = 0;
    CHECK(!PvCaptureMask(&in, 1, 1, 1, &why) && why == PVW_NOHOOKS, "no model hooks");
    in = Cap(), in.fails = PV_FAILS_OFF;
    CHECK(PvNeed(&in) == 0 && !PvCaptureMask(&in, 1, 1, 1, &why) && why == PVW_FAILED, "off after %d failures", PV_FAILS_OFF);
    in = Cap(), in.fails = PV_FAILS_OFF - 1;
    CHECK(PvNeed(&in) == (PVL_HEAT | PVL_SPIRIT), "under the limit: still on");
    in = Cap();
    CHECK(!PvCaptureMask(&in, 0, 1, 1, &why) && why == PVW_NOPIC, "the pause picture read back (not on the GPU): no layers (nothing to restore from)");
    CHECK(PvCaptureMask(&in, 1, 1, 0, &why) == PVL_HEAT && why == PVW_OK, "no ghost or soul: the heat layer only");
    CHECK(!PvCaptureMask(&in, 1, 0, 0, &why) && why == PVW_EMPTY, "nothing collected: nothing drawn");
    in.heatOk = 0;
    CHECK(PvCaptureMask(&in, 1, 1, 1, &why) == PVL_SPIRIT, "no heat redraw (ThermalHeat=0 / faults): the spirit layer only");
    in = Cap(), in.gameplay = 0;
    CHECK(!PvCaptureMask(&in, 1, 1, 1, &why) && why == PVW_NOTPLAY, "the title menu: nothing");
    for (why = 0; why < PVW_N; ++why) CHECK(kPvWhy[why] && (why == 0) == (kPvWhy[why][0] == 0), "a reason text %d", why);
}
static void TestPage(void)
{
    PvBackIn b;
    PvPageIn p;
    int m;
    // v3.58 PV358: by Preview Off / Stock / Live
    memset(&b, 0, sizeof b);
    b.mode = PVM_LIVE, b.sceneOk = 1;
    CHECK(PvBackdrop(&b) == PVB_SCENE, "Live, the title menu, no files: Refined's scene");
    CHECK(PvBackNeedsStock(PVM_LIVE, 0) && PvBackNeedsPicture(PVM_LIVE, 0, 0), "Live without a pause asks the stock picture, then PreviewImage");
    b.picture = 1;
    CHECK(PvBackdrop(&b) == PVB_PICTURE, "Live, no stock file: PreviewImage (F7's preview.dds)");
    b.stockOk = 1;
    CHECK(PvBackdrop(&b) == PVB_STOCKPIC && !PvBackNeedsPicture(PVM_LIVE, 0, 1), "Live, the title menu: the stock picture first; PreviewImage not loaded");
    b.pausePic = 1;
    CHECK(PvBackdrop(&b) == PVB_PAUSE && !PvBackNeedsStock(PVM_LIVE, 1) && !PvBackNeedsPicture(PVM_LIVE, 1, 0),
          "Live in a pause: the paused scene; no file is loaded");
    b.mode = PVM_STOCK;
    CHECK(PvBackdrop(&b) == PVB_STOCKPIC && !PvBackNeedsPause(PVM_STOCK) && PvBackNeedsStock(PVM_STOCK, 1),
          "Stock in a pause: the stock picture, not the paused scene (the pause picture is not even asked)");
    b.stockOk = 0;
    CHECK(PvBackdrop(&b) == PVB_SCENE && !PvBackNeedsPicture(PVM_STOCK, 0, 0), "Stock without the file: Refined's scene (never PreviewImage)");
    b.pausePic = 0;
    CHECK(PvBackdrop(&b) == PVB_SCENE, "Stock, the title menu, no file: Refined's scene");
    b.sceneOk = 0;
    CHECK(PvBackdrop(&b) == PVB_STOCK, "Stock, no file, the scene off after a fault: the hallway");
    for (m = 0; m < 16; ++m) {
        b.mode = PVM_OFF, b.pausePic = m & 1, b.stockOk = m >> 1 & 1, b.picture = m >> 2 & 1, b.sceneOk = m >> 3 & 1;
        CHECK(PvBackdrop(&b) == PVB_OFF, "Off: no backdrop whatever there is (%d)", m);
    }
    CHECK(!PvBackNeedsPause(PVM_OFF) && !PvBackNeedsStock(PVM_OFF, 0) && !PvBackNeedsPicture(PVM_OFF, 0, 0), "Off loads nothing");
    for (m = 0; m < PVB_N; ++m) CHECK(kPvBack[m] && kPvBack[m][0], "a backdrop name %d", m);
    // the F7 sentence only where F7 changes what the page shows
    CHECK(PvF7Shows(PVM_LIVE, PVB_PICTURE) && PvF7Shows(PVM_LIVE, PVB_SCENE) && PvF7Shows(PVM_LIVE, PVB_STOCK),
          "Live outside a pause without the stock picture: F7's picture is (or becomes) the backdrop");
    CHECK(!PvF7Shows(PVM_LIVE, PVB_STOCKPIC) && !PvF7Shows(PVM_LIVE, PVB_PAUSE) && !PvF7Shows(PVM_LIVE, -1),
          "Live over the stock picture / a pause / no page: no sentence");
    CHECK(!PvF7Shows(PVM_STOCK, PVB_SCENE) && !PvF7Shows(PVM_STOCK, PVB_STOCKPIC) && !PvF7Shows(PVM_OFF, PVB_OFF),
          "Stock and Off never show F7's picture: no sentence");
    // the stock file is never F7's
    CHECK(PvSamePath("G:\\Games\\SH4\\SH4Refined_preview_stock.dds", "g:/games/sh4/sh4refined_PREVIEW_stock.DDS"), "the same path, case / slash blind");
    CHECK(!PvSamePath("G:\\Games\\SH4\\preview.dds", "G:\\Games\\SH4\\SH4Refined_preview_stock.dds") && !PvSamePath("", "") &&
          !PvSamePath(NULL, "x") && !PvSamePath("G:\\a.dds", "G:\\a.dds.tmp"), "other paths differ (also a prefix, empty, NULL)");
    memset(&p, 0, sizeof p);
    p.thermal = 1, p.back = PVB_PAUSE, p.layersSeq = p.picSeq = 7, p.haveHeat = p.haveSpirit = 1;
    CHECK(PvPageLayer(&p) == PVP_PAUSE_HEAT, "Thermal over this pause: the heat layer");
    p.thermal = 0, p.spirit = 1;
    CHECK(PvPageLayer(&p) == PVP_PAUSE_SPIRIT, "Spirit Vision over this pause: the spirit layer");
    p.picSeq = 8;
    CHECK(PvPageLayer(&p) == PVP_NONE, "the layers of an older pause: none");
    p.picSeq = 7, p.haveSpirit = 0;
    CHECK(PvPageLayer(&p) == PVP_NONE, "no spirit layer kept: none");
    p.layersSeq = p.picSeq = -1, p.haveSpirit = 1;
    CHECK(PvPageLayer(&p) == PVP_NONE, "no layers yet (-1): none");
    p.spirit = 0, p.layersSeq = p.picSeq = 3;
    CHECK(PvPageLayer(&p) == PVP_NONE, "CRT and the other filters: no layer (they are in the picture)");
    p.back = PVB_SCENE, p.thermal = 1;
    CHECK(PvPageLayer(&p) == PVP_SCENE_HEAT, "the scene under Thermal: its heat");
    p.thermal = 0, p.spirit = 1;
    CHECK(PvPageLayer(&p) == PVP_SCENE_SPIRIT, "the scene under Spirit Vision: its spirit");
    p.back = PVB_PICTURE;
    CHECK(PvPageLayer(&p) == PVP_NONE, "the F7 picture: no layer (it has no bodies of its own)");
    p.back = PVB_STOCK;
    CHECK(PvPageLayer(&p) == PVP_NONE, "the hallway: no layer");
    p.back = PVB_STOCKPIC, p.thermal = 1, p.spirit = 0;
    CHECK(PvPageLayer(&p) == PVP_NONE, "v3.58: the stock picture under Thermal: its palette only (a picture has no bodies)");
    p.back = PVB_OFF;
    CHECK(PvPageLayer(&p) == PVP_NONE, "v3.58: Preview Off: no layer");
}
static void TestParse(void)
{
    int k;
    CHECK(PvParseScene("Scene", &k) == 1 && k && PvParseScene("scene", &k) == 1 && k, "Scene");
    CHECK(PvParseScene("Picture", &k) == 0 && k && PvParseScene("PICTURE", &k) == 0 && k, "Picture");
    CHECK(PvParseScene("1", &k) == 1 && k && PvParseScene("0", &k) == 0 && k, "digits");
    CHECK(PvParseScene("", &k) == 1 && k && PvParseScene(NULL, &k) == 1 && k, "empty: the default Scene");
    CHECK(PvParseScene("Hallway", &k) == 1 && !k, "not a value: Scene, flagged");
}
static void TestKey(void)
{
    int at = 0;
    const char *why = NULL;
    CHECK(PsLint(kPvPsKey, 64, &at, &why) == PSL_OK, "the key shader uses proven forms only (at %d: %s)", at, why ? why : "");
    CHECK(fabsf(PvKeyStay(0, 0, 0) - 1.0f) < 1e-6f, "black: the world stays");
    CHECK(PvKeyStay(1, 1, 1) == 0.0f && PvKeyStay(1.0f, 0.66f, 0.0f) == 0.0f, "white / orange heat: the body covers the world");
    CHECK(PvKeyStay(0.04f, 0.0f, 0.0f) > 0.85f, "a faint edge stays mostly world");
    {
        float o[3];
        static const float kCloth[3] = { 0.30f, 0.30f, 0.30f };
        PvHeatRamp(kCloth, 1.0f, o);
        CHECK(PvKeyStay(o[0], o[1], o[2]) == 0.0f, "a living body's darkest heat still covers fully");
    }
}
static void Bounds(const PvV *v, int n, const PvScene *s, float *x0, float *y0, float *x1, float *y1)
{
    int i;
    *x0 = *y0 = 1e9f, *x1 = *y1 = -1e9f;
    for (i = 0; i < n; ++i) {
        if (v[i].x < *x0) *x0 = v[i].x;
        if (v[i].x > *x1) *x1 = v[i].x;
        if (v[i].y < *y0) *y0 = v[i].y;
        if (v[i].y > *y1) *y1 = v[i].y;
    }
    (void)s;
}
static int Finite(const PvV *v, int n, float w, float h)
{
    int i;
    for (i = 0; i < n; ++i)
        if (!_finite(v[i].x + v[i].y) || v[i].x < -2.0f * w || v[i].x > 3.0f * w || v[i].y < -1.0f * h || v[i].y > 2.0f * h) return 0;
    return n % 3 == 0;
}
static float CanvasPx(const PvScene *s, float cx) { return 0.5f * s->w + (cx - 256.0f) * s->w / (512.0f * s->S); }
static PvScene g_s;
static void TestScene(void)
{
    static const float kSize[][3] = { { 640, 480, 1 }, { 1024, 768, 1 }, { 1280, 720, 1.3333f }, { 1920, 1080, 1.3333f },
                                      { 2560, 1440, 1.3333f }, { 3440, 1440, 1.7917f }, { 3840, 2160, 1.3333f }, { 5120, 1440, 2.6667f },
                                      { 1280, 1024, 0.9375f } };
    int t;
    for (t = 0; t < (int)(sizeof kSize / sizeof kSize[0]); ++t) {
        PvScene *s = &g_s;
        const float w = kSize[t][0], h = kSize[t][1], S = kSize[t][2];
        float x0, y0, x1, y1, gx0, gy0, gx1, gy1;
        int i, hot = 1, glow = 1;
        CHECK(PvSceneBuild(s, w, h, S), "%gx%g: built, nothing lost", w, h);
        CHECK(s->nRoom > 300 && s->nRoom <= PV_ROOM_MAX && s->nGlow > 0 && s->nFig > 60 && s->nGhost > 60 && s->nFront > 20 &&
              s->nHeat == 120 + s->nFig && s->nSpirit > s->nGhost * 3, "%gx%g: the lists' sizes (room %d, glow %d, figure %d, ghost %d, "
              "front %d, heat %d, spirit %d)", w, h, s->nRoom, s->nGlow, s->nFig, s->nGhost, s->nFront, s->nHeat, s->nSpirit);
        CHECK(Finite(s->room, s->nRoom, w, h) && Finite(s->glow, s->nGlow, w, h) && Finite(s->fig, s->nFig, w, h) &&
              Finite(s->ghost, s->nGhost, w, h) && Finite(s->front, s->nFront, w, h) && Finite(s->heat, s->nHeat, w, h) &&
              Finite(s->spirit, s->nSpirit, w, h), "%gx%g: every vertex finite, whole triangles", w, h);
        Bounds(s->fig, s->nFig, s, &x0, &y0, &x1, &y1);
        CHECK(x0 >= CanvasPx(s, 222.0f) && x1 <= CanvasPx(s, 330.0f), "%gx%g: the figure in the clear column (%.0f .. %.0f of %.0f .. %.0f)",
              w, h, x0, x1, CanvasPx(s, 222.0f), CanvasPx(s, 330.0f));
        CHECK(y0 >= 70.0f / 448.0f * h && y1 <= 352.0f / 448.0f * h, "%gx%g: the figure below the title, above the description (%.0f .. %.0f)",
              w, h, y0, y1);
        CHECK(y1 - y0 > 0.38f * h, "%gx%g: the figure is large (%.0f px)", w, h, y1 - y0);
        Bounds(s->ghost, s->nGhost, s, &gx0, &gy0, &gx1, &gy1);
        CHECK(gx0 > x1 - 0.02f * h, "%gx%g: the ghost beside the figure, not over it", w, h);
        {
            float cx0, cy0, cx1, cy1;
            Bounds(s->front, s->nFront, s, &cx0, &cy0, &cx1, &cy1);
            CHECK(cx0 <= gx0 + 0.02f * h && cx1 >= gx1 - 0.02f * h && cy0 < gy0 + 0.6f * (gy1 - gy0) && cy1 >= gy1 - 0.02f * h,
                  "%gx%g: the cabinet hides the ghost's lower half (cabinet %.0f..%.0f x %.0f..%.0f, ghost %.0f..%.0f x %.0f..%.0f)", w, h,
                  cx0, cx1, cy0, cy1, gx0, gx1, gy0, gy1);
        }
        for (i = 0; i < s->nFig; ++i) {
            const PvU32 c = s->heat[120 + i].c;
            if ((c >> 24) != 0xFF || PvChan(c, 16) < 0.99f || PvChan(c, 8) < 0.30f) hot = 0;
        }
        CHECK(hot, "%gx%g: Thermal: every vertex of the figure red-hot to white, opaque", w, h);
        for (i = 0; i < s->nSpirit; ++i)
            if ((s->spirit[i].c >> 24) != 0xFF) glow = 0;
        CHECK(glow, "%gx%g: the spirit glow is additive (premultiplied, alpha 1)", w, h);
        CHECK(fabsf(s->chestX - 0.5f * (x0 + x1)) < 0.02f * h && s->chestY > y0 + 0.2f * (y1 - y0) && s->chestY < y0 + 0.4f * (y1 - y0),
              "%gx%g: the soul fire at the chest", w, h);
        {   // the room covers the screen: its triangles' bounding box includes the screen
            float rx0, ry0, rx1, ry1;
            Bounds(s->room, s->nRoom, s, &rx0, &ry0, &rx1, &ry1);
            CHECK(rx0 <= 0.0f && rx1 >= w && ry0 <= 0.0f && ry1 >= h, "%gx%g: the room reaches every edge", w, h);
        }
    }
    {   // the pulse changes the glow, not its shape
        const int n = g_s.nSpirit;
        PvU32 c0;
        CHECK(PvSceneSpirit(&g_s, 1, 1.0f) && g_s.nSpirit == n, "the glow rebuilt: the same count");
        c0 = g_s.spirit[n - 1].c;
        CHECK(PvSceneSpirit(&g_s, 1, 0.6f) && g_s.nSpirit == n && g_s.spirit[n - 1].c != c0, "a lower pulse: a dimmer glow");
        CHECK(PvSceneSpirit(&g_s, 0, 1.0f) && g_s.nSpirit == n, "White ghosts: the same shape");
    }
    CHECK(!PvSceneBuild(&g_s, 0, 0, 1) && !PvSceneBuild(&g_s, 32, 32, 1) && !PvSceneBuild(&g_s, 1e9f, 720, 1), "silly sizes: not built");
}
static SfWorld g_w;
static SfSpr g_spr[SF_SPR_MAX];
static PvTV g_q[PV_FIRE_MAX];
static void TestFire(void)
{
    SfIn in;
    int f, n = 0, nq = 0, pre = 0, i, ok = 1;
    float t = 0.0f;
    PvSceneBuild(&g_s, 1920, 1080, 1.3333f);
    memset(&g_w, 0, sizeof g_w);
    PvFireIn(&in);
    for (f = 0; f < 120; ++f, t += 1.0f / 60.0f) n = SfFrame(&g_w, &in, 1, t, g_spr, SF_SPR_MAX);
    CHECK(n > 6, "a burning fire after 2 s (%d sprites)", n);
    nq = PvFireQuads(g_spr, n, g_s.chestX, g_s.chestY, g_s.figH / PV_PERSON_U, 1080.0f / 720.0f, g_q, PV_FIRE_MAX, &pre);
    CHECK(nq > 0 && nq % 6 == 0 && pre <= nq && pre % 6 == 0, "its quads (%d vertices, %d premultiplied first)", nq, pre);
    for (i = 0; i < nq; ++i)
        if (!_finite(g_q[i].x + g_q[i].y) || fabsf(g_q[i].x - g_s.chestX) > 0.5f * g_s.figH || fabsf(g_q[i].y - g_s.chestY) > 0.7f * g_s.figH ||
            g_q[i].u < 0 || g_q[i].u > 1 || g_q[i].v < 0 || g_q[i].v > 1)
            ok = 0;
    CHECK(ok, "every quad finite, near the chest, in the atlas");
    {
        float y0 = 1e9f;
        for (i = pre; i < nq; ++i) y0 = g_q[i].y < y0 ? g_q[i].y : y0;
        CHECK(y0 < g_s.chestY - 0.08f * g_s.figH, "the flames rise above the chest (top %.0f, chest %.0f)", y0, g_s.chestY);
    }
}
static void Dump(const char *dir)
{
    char p[512];
    FILE *f;
    int i;
    static const char *const kName[7] = { "room", "glow", "fig", "ghost", "front", "heat", "spirit" };
    PvSceneBuild(&g_s, 1280, 720, 1.3333f);
    sprintf(p, "%s\\pv_scene.txt", dir);
    f = fopen(p, "w");
    if (!f) {
        printf("cannot write %s\n", p);
        return;
    }
    fprintf(f, "size %g %g\n", g_s.w, g_s.h);
    for (i = 0; i < 7; ++i) {
        const PvV *v = i == 0 ? g_s.room : i == 1 ? g_s.glow : i == 2 ? g_s.fig : i == 3 ? g_s.ghost : i == 4 ? g_s.front : i == 5 ? g_s.heat : g_s.spirit;
        const int n = i == 0 ? g_s.nRoom : i == 1 ? g_s.nGlow : i == 2 ? g_s.nFig : i == 3 ? g_s.nGhost : i == 4 ? g_s.nFront : i == 5 ? g_s.nHeat : g_s.nSpirit;
        int k;
        fprintf(f, "list %s %d\n", kName[i], n);
        for (k = 0; k < n; ++k) fprintf(f, "%.2f %.2f %08X\n", v[k].x, v[k].y, v[k].c);
    }
    {   // the fire at 2 s
        SfIn in;
        int fr, n = 0, nq, pre;
        float t = 0.0f;
        memset(&g_w, 0, sizeof g_w);
        PvFireIn(&in);
        for (fr = 0; fr < 120; ++fr, t += 1.0f / 60.0f) n = SfFrame(&g_w, &in, 1, t, g_spr, SF_SPR_MAX);
        nq = PvFireQuads(g_spr, n, g_s.chestX, g_s.chestY, g_s.figH / PV_PERSON_U, 1.0f, g_q, PV_FIRE_MAX, &pre);
        fprintf(f, "fire %d %d\n", nq, pre);
        for (fr = 0; fr < nq; ++fr) fprintf(f, "%.2f %.2f %.5f %.5f %08X\n", g_q[fr].x, g_q[fr].y, g_q[fr].u, g_q[fr].v, g_q[fr].c);
    }
    fclose(f);
    printf("wrote %s\n", p);
    sprintf(p, "%s\\pv_atlas.raw", dir);  // the fire atlas (SfAtlasTexel, 512 x 512 bytes), as FXX_FLAME is baked
    f = fopen(p, "wb");
    if (f) {
        int x, y;
        for (y = 0; y < SF_TEX; ++y)
            for (x = 0; x < SF_TEX; ++x) fputc((int)(SfAtlasTexel(x, y) * 255.0f + 0.5f), f);
        fclose(f);
        printf("wrote %s\n", p);
    }
}

// v3.58 PV358: Preview's and Filter Strength's ini words, the migration, v3.15's leftover
static void TestIni358(void)
{
    int v, k, i;
    for (i = 0; i < PVM_N; ++i) CHECK(PvParseMode(kPvModeName[i], &k) == i && k, "Preview %s reads", kPvModeName[i]);
    CHECK(PvParseMode("live", &k) == PVM_LIVE && k && PvParseMode("OFF", &k) == PVM_OFF && k && PvParseMode("1", &k) == PVM_STOCK && k,
          "case-blind, digits");
    CHECK(PvParseMode("Scene", &k) == PVM_LIVE && !k && PvParseMode("", &k) == PVM_LIVE && !k && PvParseMode(NULL, &k) == PVM_LIVE && !k &&
          PvParseMode("3", &k) == PVM_LIVE && !k, "not a value: Live, flagged");
    CHECK(PvModeIni(NULL, NULL, &v) == PVI_MISSING && v == PVM_LIVE, "both missing: Live, added");
    CHECK(PvModeIni(NULL, "Scene", &v) == PVI_MIGRATE && v == PVM_LIVE, "v3.56's PreviewScene=Scene alone: migrated to Live");
    CHECK(PvModeIni(NULL, "Picture", &v) == PVI_MIGRATE && v == PVM_LIVE, "PreviewScene=Picture alone: Live too");
    CHECK(PvModeIni(NULL, "", &v) == PVI_MIGRATE && v == PVM_LIVE, "PreviewScene= (empty) alone: Live");
    CHECK(PvModeIni("Stock", "Scene", &v) == PVI_STALE && v == PVM_STOCK, "both: Preview decides, the old key goes");
    CHECK(PvModeIni("Off", NULL, &v) == PVI_OK && v == PVM_OFF, "Preview alone: read");
    CHECK(PvModeIni("Sometimes", NULL, &v) == PVI_BAD && v == PVM_LIVE, "Preview not a value: Live, rewritten");
    CHECK(PVM_DEFAULT == PVM_LIVE && !strcmp(kPvModeName[PVM_DEFAULT], "Live"), "the default is Live");
    for (i = 0; i < PVS_N; ++i) CHECK(PvParseFs(kPvFsName[i], &k) == i && k, "FilterStrength %s reads", kPvFsName[i]);
    CHECK(PVS_DEFAULT == PVS_HIGH && !strcmp(kPvFsName[PVS_DEFAULT], "High"), "the default is High (v3.16 - v3.57's strength)");
    CHECK(PvFsIni("Medium", NULL, &v) == PVF_LEGACY && v == PVS_HIGH, "v3.15's FilterStrength=Medium without the marker: High (the user's ini)");
    CHECK(PvFsIni("Low", "345", &v) == PVF_LEGACY && v == PVS_HIGH, "another marker: not ours, High");
    CHECK(PvFsIni("Low", PV_FS_MARK, &v) == PVF_OK && v == PVS_LOW, "with the marker: the value");
    CHECK(PvFsIni("Ultra", PV_FS_MARK, &v) == PVF_OK && v == PVS_ULTRA, "Ultra with the marker");
    CHECK(PvFsIni("Very High", PV_FS_MARK, &v) == PVF_BAD && v == PVS_HIGH, "not a value with the marker: High, rewritten");
    CHECK(PvFsIni(NULL, NULL, &v) == PVF_MISSING && v == PVS_HIGH && PvFsIni("", PV_FS_MARK, &v) == PVF_MISSING, "missing: High, added");
}
// v3.58 PV358: Filter Strength: High is the identity (byte for byte), Low / Medium mix, Ultra scales; Thermal / Spirit
static void TestStrength358(void)
{
    // the looks of screenfx.inc's kFxLook (mix, con, bri, px, vig, scan, mask, grain, flicker), by family
    static const PvFsLookV kL[] = {
        { PVT_CRT, -0.12f, 1.06f, 0.03f, 1.0f, 0.30f, 0.30f, 0.18f, 0, 0 }, { PVT_VIVID, -0.85f, 1.15f, 0, 1.0f, 0, 0, 0, 0, 0 },
        { PVT_TONE, 0.88f, 1.04f, 0.01f, 0, 0.30f, 0, 0, 0, 0 }, { PVT_TONE, 0.92f, 1.12f, 0.02f, 0, 0.62f, 0, 0, 0.12f, 0.035f },
        { PVT_NV, 1, 1, 0, 2.5f, 0.40f, 0, 0, 0.22f, 0 }, { PVT_TONE, 1, 1.06f, 0.02f, 0, 0.30f, 0, 0, 0.12f, 0 },
        { PVT_THERMAL, 1, 1, 0, 0, 0, 0, 0, 0, 0 }, { PVT_GHOST, 1, 1, 0, 1.5f, 0, 0, 0, 0, 0 }, { PVT_OTHER, 0, 1, 0, 0, 0.45f, 0, 0, 0, 0 } };
    int i, v, r;
    for (i = 0; i < (int)(sizeof kL / sizeof kL[0]); ++i) {
        for (v = 0; v < PVS_N; ++v) {
            PvFsLookV L = kL[i];
            const int ch = PvFsLook(&L, v);
            if (v != PVS_ULTRA) CHECK(!ch && !memcmp(&L, &kL[i], sizeof L), "look %d at %s: untouched, byte for byte", i, kPvFsName[v]);
        }
        {
            PvFsLookV L = kL[i];
            const int ch = PvFsLook(&L, PVS_ULTRA);
            if (kL[i].fam == PVT_THERMAL || kL[i].fam == PVT_GHOST)
                CHECK(!ch && !memcmp(&L, &kL[i], sizeof L), "look %d (Thermal / Spirit Vision) at Ultra: as High", i);
            else {
                CHECK(ch && L.vig >= kL[i].vig && L.vig <= 1.0f && L.grain >= kL[i].grain && L.grain <= 1.0f && L.scan <= 1.0f && L.mask <= 1.0f,
                      "look %d at Ultra: the overlays stronger, at most 1", i);
                CHECK(fabsf(L.con - 1.0f) >= fabsf(kL[i].con - 1.0f) && fabsf(L.bri) >= fabsf(kL[i].bri) && fabsf(L.mix) >= fabsf(kL[i].mix) - 1e-6f &&
                      (L.fam != PVT_TONE || L.mix <= 1.0f), "look %d at Ultra: further from neutral (the tone's share at most 1)", i);
                CHECK(L.con > 0.0f && L.con < 2.0f && L.mix > -2.0f, "look %d at Ultra: in the shaders' range", i);
            }
        }
    }
    CHECK(PvFsMixAlpha(PVS_HIGH) == 0.0f && PvFsMixAlpha(PVS_ULTRA) == 0.0f && !PvFsNeedsCopy(PVS_HIGH, 1) && !PvFsNeedsCopy(PVS_ULTRA, 1),
          "High / Ultra: no copy, nothing laid back (High draws exactly v3.57's frame)");
    CHECK(fabsf(PvFsMixAlpha(PVS_LOW) - 0.65f) < 1e-6f && fabsf(PvFsMixAlpha(PVS_MEDIUM) - 0.35f) < 1e-6f, "Low lays 65 %%, Medium 35 %% of the plain picture back");
    CHECK(PvFsNeedsCopy(PVS_LOW, 1) && PvFsNeedsCopy(PVS_MEDIUM, 1) && !PvFsNeedsCopy(PVS_LOW, 0), "a copy only with a filter on");
    CHECK(kPvFsK[PVS_HIGH] == 1.0f && kPvFsK[PVS_LOW] < kPvFsK[PVS_MEDIUM] && kPvFsK[PVS_MEDIUM] < 1.0f && kPvFsK[PVS_ULTRA] > 1.0f, "the strengths rise");
    for (r = 0; r < 12; ++r)
        for (v = 0; v < PVS_N; ++v) {
            const int want = v == PVS_ULTRA && r == 2;
            CHECK(PvFsConstK(PVT_VIVID, v, r) == want && PvFsConstK(PVT_NV, v, r) == (v == PVS_ULTRA && r == 4) && !PvFsConstK(PVT_THERMAL, v, r) &&
                  !PvFsConstK(PVT_TONE, v, r), "the constants scaled: Vivid c2 / Night Vision c4 at Ultra only (c%d, %s)", r, kPvFsName[v]);
        }
    {   // Low on Thermal: the hot body stays its colour (drawn after the mix), the world is 35 percent palette, 65 percent picture
        const float world = 0.2f, plain = 0.6f, a = PvFsMixAlpha(PVS_LOW), out = world * (1.0f - a) + plain * a;
        CHECK(fabsf(out - (0.35f * world + 0.65f * plain)) < 1e-5f, "the mix: SRCALPHA / INVSRCALPHA with the plain picture at 1 - k");
    }
}
// v3.58 PV358: the stock picture the installer places: the user's F7 picture (1280 x 720 32-bit) reads as F7's do
static void TestStockFile(void)
{
    static const char *const kAt[] = { "../../../preview_user_F7_2026-11-26_2338.dds", "../../../../preview_user_F7_2026-11-26_2338.dds",
                                         "../../preview_user_F7_2026-11-26_2338.dds" };  // v3.58 merge: + merge358/test
    FILE *f = NULL;
    unsigned char *buf;
    long n;
    int i;
    for (i = 0; i < 3 && !f; ++i) f = fopen(kAt[i], "rb");  // v3.58 merge: 3 places
    if (!f) {
        printf("(the user's F7 picture is not beside this sandbox: its parse is not checked)\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)n);
    CHECK(buf && fread(buf, 1, (size_t)n, f) == (size_t)n, "the stock picture reads (%ld bytes)", n);
    fclose(f);
    if (buf) {
        TpDds d;
        const char *why = TpDdsParse(buf, (size_t)n, &d);
        CHECK(!why && d.w == 1280 && d.h == 720 && d.fmt == TP_FMT_X8R8G8B8 && d.levels == 1 && n == 128 + 1280 * 720 * 4,
              "the user's F7 picture is a 1280 x 720 X8R8G8B8 DDS as F7 writes (%s; %ux%u, %ld bytes)", why ? why : "ok", d.w, d.h, n);
        free(buf);
    }
}
int main(int argc, char **argv)
{
    TestCapture();
    TestPage();
    TestParse();
    TestKey();
    TestScene();
    TestFire();
    TestIni358();  // v3.58 PV358
    TestStrength358();
    TestStockFile();
    if (argc > 2 && !strcmp(argv[1], "--dump")) Dump(argv[2]);
    printf("preview_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
