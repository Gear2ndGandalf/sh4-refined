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
    memset(&b, 0, sizeof b);
    b.sceneOn = b.sceneOk = 1;
    CHECK(PvBackdrop(&b) == PVB_SCENE, "the title menu: Refined's scene");
    CHECK(!PvBackNeedsPicture(0, 1, 1), "the scene decides: the picture file is not even loaded");
    b.picture = 1;
    CHECK(PvBackdrop(&b) == PVB_SCENE, "Scene wins over an old F7 picture");
    b.sceneOn = 0;
    CHECK(PvBackdrop(&b) == PVB_PICTURE && PvBackNeedsPicture(0, 0, 1), "PreviewScene=Picture: the picture (as v3.55)");
    b.picture = 0;
    CHECK(PvBackdrop(&b) == PVB_SCENE, "Picture without a file: the scene (not the hallway)");
    b.sceneOk = 0;
    CHECK(PvBackdrop(&b) == PVB_STOCK, "the scene off after a fault, no file: the stock hallway");
    b.sceneOn = 1;
    CHECK(PvBackdrop(&b) == PVB_STOCK && PvBackNeedsPicture(0, 1, 0), "Scene but off after a fault: the picture is asked");
    b.pausePic = 1;
    CHECK(PvBackdrop(&b) == PVB_PAUSE && !PvBackNeedsPicture(1, 0, 0), "a paused game: its own picture, always");
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
int main(int argc, char **argv)
{
    TestCapture();
    TestPage();
    TestParse();
    TestKey();
    TestScene();
    TestFire();
    if (argc > 2 && !strcmp(argv[1], "--dump")) Dump(argv[2]);
    printf("preview_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
