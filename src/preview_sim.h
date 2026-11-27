// ================================================================ v3.56 PV356: the Effects page's preview, the pure code
// staging/v356_feedback/w6_preview/PLAN.md (USER_FEEDBACK_v355 issue 2 and improvement 19). Pure C (no D3D, no game
// memory): included by preview.inc (the game side) and by the native test test/preview_sim_test.c, which also dumps the
// title scene for tools/pv_mockup.py (the mock-ups draw what this header produces).
//
// 1. The pause's look layers (issue 2). Thermal's heat and Spirit Vision's ghosts / soul fires are drawn per body in the
//    live scene; a kept picture has none of it. In the capture frame (the frame whose copy becomes the pause picture) the
//    bodies are collected at 0x52AEB5 whatever the filter (PvNeed / PvCollectMask), and at 0x4EFBFD, right after the clean
//    pause picture is kept, each look's own parts are drawn once over a black frame and copied to a target of ours (the
//    heat layer, the spirit layer); the clean picture is then drawn back. The Effects page lays the layer of its filter
//    over the filtered picture: Thermal through a luma key (PvKey: what of the world stays = 1 - sat(8 L)), Spirit Vision
//    added (its parts are light).
// 2. The title scene (improvement 19): with no paused scene the page shows SH4 Refined's own room (triangles in vertex
//    colours: walls, floor, a window, a lamp, a door, a cabinet), a figure and a ghost (PvSceneBuild), and draws the
//    figure's heat (Thermal: the kPsHeat ramp of its colours), its footprints, the ghost's spirit glow and the figure's
//    soul fire (Spirit Vision: spiritfire_sim.h's own fire) after the filter, as play does. Nothing of the game's art.
#ifndef PV_SIM_H
#define PV_SIM_H
#include <math.h>
#include <string.h>
#include <float.h>         // _finite
#include "post_sim.h"        // PS_* tokens, PsTok, PsLint (the key shader uses proven ps.1.1 forms only)
#include "spiritfire_sim.h"  // the soul fire of the title scene's figure (the same fire as play's)

typedef unsigned int PvU32;
typedef struct { float x, y; PvU32 c; } PvV;            // a vertex on screen (pixels), ARGB
typedef struct { float x, y, u, v; PvU32 c; } PvTV;     // a textured vertex (the soul fire's quads)

// ---------------------------------------------------------------- 1. the capture rule
typedef struct {
    int on;          // [ScreenEffects] PreviewLayers
    int shOn;        // Refined's render targets can be made (shadows.inc: the device-reset hook is in)
    int hooks;       // the model-draw hooks (0x52AEB5) are in
    int gameplay;    // engine mode 6, sub-mode 2
    int phase;       // 0 play, 1 real-time cut-scene, 2 paused
    int capture;     // [0x12E3E2C]: 1 = this frame's copy becomes the pause picture
    int heatOk;      // the heat redraw can run (ThHeatOk)
    int edgesOk;     // Spirit Vision's shaders and the scene hook (GvEdgesOk, g_spInstalled)
    int heatLive;    // the live filter is Thermal: it collects its bodies itself (ThermHeatLive)
    int ghostLive;   // the live filter is Spirit Vision: it collects its ghosts and souls itself (GvLive)
    int fails;       // faults / failed copies this session (PV_FAILS_OFF: off)
} PvCapIn;
enum { PVL_HEAT = 1, PVL_SPIRIT = 2 };
#define PV_FAILS_OFF 3
// the layers this capture frame needs, as play's own rules: heat in play and real-time cut-scenes (ThermHeatLive's
// phases), ghosts and souls in play only (GvPhase0, FXV2-004)
static int PvNeed(const PvCapIn *in)
{
    int m = 0;
    if (!in->on || !in->shOn || !in->hooks || !in->gameplay || in->capture != 1 || in->fails >= PV_FAILS_OFF) return 0;
    if (in->heatOk && (in->phase == 0 || in->phase == 1)) m |= PVL_HEAT;
    if (in->edgesOk && in->phase == 0) m |= PVL_SPIRIT;
    return m;
}
// 0x52AEB5 / 0x4EE376: the lists the preview collects itself (a live look collects its own: never twice)
static int PvCollectMask(const PvCapIn *in)
{
    int m = PvNeed(in);
    if (in->heatLive) m &= ~PVL_HEAT;
    if (in->ghostLive) m &= ~PVL_SPIRIT;
    return m;
}
enum { PVW_OK, PVW_OFF, PVW_NOTARGETS, PVW_NOHOOKS, PVW_NOTPLAY, PVW_FAILED, PVW_NOPIC, PVW_EMPTY, PVW_N };
static const char *const kPvWhy[PVW_N] = {
    "", "[ScreenEffects] PreviewLayers=0", "Refined's render targets are not available (no device-reset hook)",
    "the model-draw hooks are not in place", "not a frame of play", "off for this session after failures",
    "the pause picture is not on the GPU this time", "nothing hot or spiritual was drawn in that frame" };
// 0x4EFBFD, the capture frame: the layers to draw (need & what was collected), and why none (PVW_*)
static int PvCaptureMask(const PvCapIn *in, int gpuPic, int haveHeat, int haveSpirit, int *why)
{
    int m;
    *why = !in->on ? PVW_OFF : !in->shOn ? PVW_NOTARGETS : !in->hooks ? PVW_NOHOOKS : in->fails >= PV_FAILS_OFF ? PVW_FAILED :
           !in->gameplay || in->capture != 1 || in->phase > 1 ? PVW_NOTPLAY : PVW_OK;
    if (*why) return 0;
    m = PvNeed(in);
    if (!gpuPic) {
        *why = PVW_NOPIC;
        return 0;
    }
    if (!haveHeat) m &= ~PVL_HEAT;
    if (!haveSpirit) m &= ~PVL_SPIRIT;
    if (!m) *why = PVW_EMPTY;
    return m;
}

// ---------------------------------------------------------------- 2. the page: its backdrop and the layer over it
enum { PVB_PAUSE, PVB_SCENE, PVB_PICTURE, PVB_STOCK };
static const char *const kPvBack[4] = { "the paused scene", "SH4 Refined's preview scene", "the preview picture (PreviewImage)",
                                        "the stock hallway" };
typedef struct {
    int pausePic;    // this pause's picture (FxPauseTex)
    int sceneOn;     // [ScreenEffects] PreviewScene=Scene
    int picture;     // PreviewImage loaded (asked only when it decides)
    int sceneOk;     // the scene can be drawn (not off after a fault)
} PvBackIn;
// with a paused game its own picture; else Scene: Refined's scene (then the picture, the hallway), Picture: the
// picture (then the scene, the hallway)
static int PvBackdrop(const PvBackIn *in)
{
    if (in->pausePic) return PVB_PAUSE;
    if (in->sceneOn && in->sceneOk) return PVB_SCENE;
    if (in->picture) return PVB_PICTURE;
    return in->sceneOk ? PVB_SCENE : PVB_STOCK;
}
// does the backdrop pick need the picture file? (only when the scene does not decide first)
static int PvBackNeedsPicture(int pausePic, int sceneOn, int sceneOk) { return !pausePic && !(sceneOn && sceneOk); }
enum { PVP_NONE, PVP_PAUSE_HEAT, PVP_PAUSE_SPIRIT, PVP_SCENE_HEAT, PVP_SCENE_SPIRIT, PVP_N };
static const char *const kPvPage[PVP_N] = { "no layer", "the pause's heat layer", "the pause's spirit layer",
                                            "the scene's heat (figure, footprints)", "the scene's spirit (ghost glow, soul fire)" };
typedef struct {
    int thermal, spirit;   // the page's filter
    int back;              // PVB_* drawn this frame (the pause picture only when it is this pause's GPU copy)
    int layersSeq, picSeq; // the pause the layers belong to, the pause picture's
    int haveHeat, haveSpirit;
} PvPageIn;
static int PvPageLayer(const PvPageIn *in)
{
    if (in->back == PVB_SCENE) return in->thermal ? PVP_SCENE_HEAT : in->spirit ? PVP_SCENE_SPIRIT : PVP_NONE;
    if (in->back != PVB_PAUSE || in->layersSeq < 0 || in->layersSeq != in->picSeq) return PVP_NONE;
    if (in->thermal && in->haveHeat) return PVP_PAUSE_HEAT;
    if (in->spirit && in->haveSpirit) return PVP_PAUSE_SPIRIT;
    return PVP_NONE;
}
// [ScreenEffects] PreviewScene: Scene (default) / Picture; 1 = Scene. *known 0: not a value (Scene)
static int PvParseScene(const char *s, int *known)
{
    *known = 1;
    if (!s || !s[0]) return 1;
    if (!_stricmp(s, "Scene") || !strcmp(s, "1")) return 1;
    if (!_stricmp(s, "Picture") || !strcmp(s, "0")) return 0;
    *known = 0;
    return 1;
}

// ---------------------------------------------------------------- 3. the heat key (the Thermal layer's composite)
// The layer was drawn over black: the world stays where it is black. Pass 1 (blend ZERO / SRCCOLOR): dst x (1 - sat(8 L));
// pass 2 (ONE / ONE): + the layer. Proven forms only (tex, dp3, add_sat, sub; one constant each). c0 luma weights,
// c1 (1, 1, 1, 1).
static const PsTok kPvPsKey[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_ADD, D_R(1) | D_SAT, S_R(1), S_R(1),         // x 2
    I_ADD, D_R(1) | D_SAT, S_R(1), S_R(1),         // x 4
    I_ADD, D_R(1) | D_SAT, S_R(1), S_R(1),         // x 8, saturated: the cover
    I_SUB, D_R(0), S_C(1), S_R(1),                 // 1 - cover: the share of the world that stays
    PS_END };
#define PV_KEY_K 8.0f
static float PvKeyStay(float r, float g, float b)  // the shader's arithmetic (8-bit steps aside)
{
    float L = 0.299f * r + 0.587f * g + 0.114f * b, c;
    c = SfSat(L + L), c = SfSat(c + c), c = SfSat(c + c);
    return 1.0f - c;
}

// ---------------------------------------------------------------- 4. the title scene
// Layout: x = w / 2 + X h, y = Y h (height units, X 0 at the screen's centre: the same room at any aspect); the figure
// and the ghost stand in the clear middle column of the page (canvas 222 .. 330 of 512, between the label and value
// panels: screenfx.inc FX_PANEL_LABEL_X1 / FX_PANEL_VALUE_X0), placed through the canvas map x = w / 2 + (cx - 256) sx,
// sx = w / (512 S) (FxPanel's).
#define PV_ROOM_MAX   2400   // vertices, a TRIANGLELIST each
#define PV_GLOW_MAX   240
#define PV_FIG_MAX    240
#define PV_FRONT_MAX  160
#define PV_HEAT_MAX   420
#define PV_SPIRIT_MAX 900
#define PV_FIRE_MAX   (SF_SPR_MAX * 6)
#define PV_FIG_CX     266.0f  // canvas x of the figure (the clear column 222 .. 330)
#define PV_GHOST_CX   312.0f
#define PV_FIG_FEET   0.700f  // height units
#define PV_FIG_H      0.420f
#define PV_GHOST_FEET 0.640f
#define PV_GHOST_H    0.300f
#define PV_PERSON_U   175.0f  // world units of a person's height (the soul fire's scale: its size is in world units)
#define PV_FIRE_SIZE  0.8f    // SV_FLAME_PLAYER: the player's fire
#define PV_FIRE_CHEST 0.30f   // the chest below the top of the head (x the height)
typedef struct {
    float w, h, S;
    PvV room[PV_ROOM_MAX]; int nRoom;      // opaque, no blending
    PvV glow[PV_GLOW_MAX]; int nGlow;      // additive light (the lamp, the window's haze)
    PvV fig[PV_FIG_MAX]; int nFig;         // the figure, opaque
    PvV ghost[PV_FIG_MAX]; int nGhost;     // the ghost's own look (SRCALPHA; not drawn under Thermal: ghosts are cold)
    PvV front[PV_FRONT_MAX]; int nFront;   // the cabinet in front of the ghost's lower half (opaque)
    PvV heat[PV_HEAT_MAX]; int nHeat;      // Thermal, after its palette: the footprints, then the figure in heat colours
    PvV spirit[PV_SPIRIT_MAX]; int nSpirit;  // Spirit Vision, after its edges: the ghost's glow (additive, through the cabinet)
    float figX, figTop, figH, chestX, chestY, ghostX, ghostY, ghostH, lampX, lampY;  // pixels
    float figXu, ghostXu;                  // height units
    int built;
} PvScene;
typedef struct { PvV *v; int n, max, lost; } PvBuf;
static PvU32 PvArgb(float r, float g, float b, float a)
{
    return (PvU32)(SfSat(a) * 255.0f + 0.5f) << 24 | (PvU32)(SfSat(r) * 255.0f + 0.5f) << 16 | (PvU32)(SfSat(g) * 255.0f + 0.5f) << 8 |
           (PvU32)(SfSat(b) * 255.0f + 0.5f);
}
static PvU32 PvArgb3(const float c[3], float a) { return PvArgb(c[0], c[1], c[2], a); }
static float PvChan(PvU32 c, int sh) { return (float)(c >> sh & 0xFFu) / 255.0f; }
static PvV PvPt(float x, float y, PvU32 c)
{
    PvV p;
    p.x = x, p.y = y, p.c = c;
    return p;
}
static void PvTri(PvBuf *b, PvV a, PvV c, PvV d)
{
    if (b->n + 3 > b->max) {
        ++b->lost;
        return;
    }
    b->v[b->n++] = a, b->v[b->n++] = c, b->v[b->n++] = d;
}
static void PvQuad(PvBuf *b, PvV p0, PvV p1, PvV p2, PvV p3)  // p0 p1 on top (left, right), p2 p3 below (left, right)
{
    PvTri(b, p0, p1, p2);
    PvTri(b, p1, p3, p2);
}
static float PvPx(const PvScene *s, float X) { return 0.5f * s->w + X * s->h; }
static float PvPy(const PvScene *s, float Y) { return Y * s->h; }
static float PvCanvasXu(const PvScene *s, float cx)  // canvas x -> height units
{
    const float S = s->S > 0.0f ? s->S : 1.0f;
    return (cx - 256.0f) * (s->w / (512.0f * S)) / s->h;
}
// the room's light at (X, Y): a warm lamp, the window's cold light, a vignette (the corners dark)
#define PV_LAMP_X (-0.10f)
#define PV_LAMP_Y 0.175f
static void PvShade(const float base[3], float X, float Y, float o[3])
{
    const float dx = X - PV_LAMP_X, dy = (Y - PV_LAMP_Y - 0.10f) * 1.25f, d2 = dx * dx + dy * dy;
    const float wx = X + 0.22f, wy = (Y - 0.68f) * 2.2f, w2 = wx * wx + wy * wy;
    const float lamp = 0.28f + 1.05f / (1.0f + d2 / 0.055f), win = 0.30f / (1.0f + w2 / 0.020f);
    const float ax = fabsf(X), vx = SfSat((ax - 0.30f) / 0.75f), vy = SfSat((Y - 0.86f) / 0.30f) + SfSat((0.05f - Y) / 0.30f);
    const float vig = (1.0f - 0.60f * vx * vx) * (1.0f - 0.45f * vy);
    static const float kWarm[3] = { 1.00f, 0.86f, 0.66f }, kCold[3] = { 0.55f, 0.68f, 0.90f };
    int k;
    for (k = 0; k < 3; ++k) o[k] = SfSat(base[k] * (lamp * kWarm[k] + win * kCold[k]) * vig);
}
static PvV PvLit(const PvScene *s, const float base[3], float X, float Y, float k)
{
    float c[3], kb[3];
    kb[0] = base[0] * k, kb[1] = base[1] * k, kb[2] = base[2] * k;
    PvShade(kb, X, Y, c);
    return PvPt(PvPx(s, X), PvPy(s, Y), PvArgb3(c, 1.0f));
}
// a quad (X, Y corners: 0 top left, 1 top right, 2 bottom left, 3 bottom right) cut nx x ny, each vertex lit; band:
// every other column x 0.9 (wallpaper stripes; each cell has its own vertices)
static void PvQuadLit(PvBuf *b, const PvScene *s, const float P[4][2], int nx, int ny, const float top[3], const float bot[3], int band)
{
    int i, j, k;
    for (j = 0; j < ny; ++j)
        for (i = 0; i < nx; ++i) {
            PvV q[4];
            for (k = 0; k < 4; ++k) {
                const float u = (float)(i + (k & 1)) / (float)nx, v = (float)(j + (k >> 1)) / (float)ny;
                const float X = (1 - v) * ((1 - u) * P[0][0] + u * P[1][0]) + v * ((1 - u) * P[2][0] + u * P[3][0]);
                const float Y = (1 - v) * ((1 - u) * P[0][1] + u * P[1][1]) + v * ((1 - u) * P[2][1] + u * P[3][1]);
                float c[3];
                c[0] = top[0] + (bot[0] - top[0]) * v, c[1] = top[1] + (bot[1] - top[1]) * v, c[2] = top[2] + (bot[2] - top[2]) * v;
                q[k] = PvLit(s, c, X, Y, band && (i & 1) ? 0.90f : 1.0f);
            }
            PvQuad(b, q[0], q[1], q[2], q[3]);
        }
}
static void PvQuadFlat(PvBuf *b, const PvScene *s, const float P[4][2], const float base[3], float k)
{
    PvQuad(b, PvLit(s, base, P[0][0], P[0][1], k), PvLit(s, base, P[1][0], P[1][1], k), PvLit(s, base, P[2][0], P[2][1], k),
           PvLit(s, base, P[3][0], P[3][1], k));
}
// an ellipse fan in pixels: the centre colour, the rim colour
static void PvFan(PvBuf *b, float cx, float cy, float rx, float ry, int n, PvU32 cIn, PvU32 cOut)
{
    int k;
    for (k = 0; k < n; ++k) {
        const float a0 = 6.2831853f * (float)k / (float)n, a1 = 6.2831853f * (float)(k + 1) / (float)n;
        PvTri(b, PvPt(cx, cy, cIn), PvPt(cx + rx * cosf(a0), cy + ry * sinf(a0), cOut), PvPt(cx + rx * cosf(a1), cy + ry * sinf(a1), cOut));
    }
}
// an ellipse fan lit like the room (opaque): the centre x kIn, the rim x kOut of base
static void PvFanLit(PvBuf *b, const PvScene *s, float X, float Y, float rx, float ry, int n, const float base[3], float kIn, float kOut)
{
    int k;
    for (k = 0; k < n; ++k) {
        const float a0 = 6.2831853f * (float)k / (float)n, a1 = 6.2831853f * (float)(k + 1) / (float)n;
        PvTri(b, PvLit(s, base, X, Y, kIn), PvLit(s, base, X + rx * cosf(a0), Y + ry * sinf(a0), kOut),
              PvLit(s, base, X + rx * cosf(a1), Y + ry * sinf(a1), kOut));
    }
}
// Thermal's body heat of a colour: kPsHeat's ramp, V = luma x DETAIL + BODY x heat (screenfx.inc TH_*)
static void PvHeatRamp(const float rgb[3], float heat, float o[3])
{
    const float L = 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2], V = L * 0.45f + 0.62f * heat;
    o[0] = SfSat((V - 0.15f) * 2.2f), o[1] = SfSat((V - 0.50f) * 2.6f), o[2] = SfSat((V - 0.82f) * 4.0f);
}

// The figure (a person seen from the front) and the ghost (a gaunt shape, its body tattered below the waist): parts in
// local units of the height u from (cx, top) in pixels. Each vertex carries its colour; the figure's heat colours are
// its own colours through the heat ramp (as the game's redraw of a body).
typedef struct { float x, y; } PvP2;
static void PvPart(PvBuf *b, float cx, float top, float u, const PvP2 p[4], const float c0[3], const float c1[3], float a0, float a1,
                   float lit)
{   // a quad part: c0 / a0 at the top edge, c1 / a1 at the bottom; lit: the light from the left (a vertex at local x
    // -0.2 x (1 + lit), at +0.2 x (1 - lit))
    PvV q[4];
    int k;
    for (k = 0; k < 4; ++k) {
        const float *c = k < 2 ? c0 : c1, side = 1.0f - lit * p[k].x * 5.0f;
        q[k] = PvPt(cx + p[k].x * u, top + p[k].y * u, PvArgb(c[0] * side, c[1] * side, c[2] * side, k < 2 ? a0 : a1));
    }
    PvQuad(b, q[0], q[1], q[2], q[3]);
}
static void PvMirror(const PvP2 in[4], PvP2 out[4])  // the right side's part: x mirrored, left / right swapped
{
    out[0].x = -in[1].x, out[0].y = in[1].y, out[1].x = -in[0].x, out[1].y = in[0].y;
    out[2].x = -in[3].x, out[2].y = in[3].y, out[3].x = -in[2].x, out[3].y = in[2].y;
}
static void PvHead(PvBuf *b, float cx, float cy, float rx, float ry, const float skin[3], const float hair[3], float a)
{
    int k;
    const int n = 18;
    for (k = 0; k < n; ++k) {
        const float a0 = 6.2831853f * (float)k / (float)n, a1 = 6.2831853f * (float)(k + 1) / (float)n;
        const float s0 = sinf(a0), s1 = sinf(a1);  // y down: s < 0 = the upper half (hair)
        PvTri(b, PvPt(cx, cy, PvArgb3(skin, a)), PvPt(cx + rx * cosf(a0), cy + ry * s0, PvArgb3(s0 < -0.15f ? hair : skin, a)),
              PvPt(cx + rx * cosf(a1), cy + ry * s1, PvArgb3(s1 < -0.15f ? hair : skin, a)));
    }
}
static void PvFigure(PvBuf *b, float cx, float top, float u)
{
    static const float kJeans0[3] = { 0.20f, 0.23f, 0.33f }, kJeans1[3] = { 0.14f, 0.16f, 0.24f }, kShoe[3] = { 0.08f, 0.07f, 0.06f };
    static const float kCoat0[3] = { 0.40f, 0.38f, 0.36f }, kCoat1[3] = { 0.30f, 0.29f, 0.28f }, kSkin[3] = { 0.70f, 0.55f, 0.46f };
    static const float kHair[3] = { 0.13f, 0.10f, 0.08f }, kShirt[3] = { 0.62f, 0.60f, 0.56f };
    static const PvP2 kLeg[4] = { { -0.095f, 0.50f }, { -0.010f, 0.50f }, { -0.085f, 0.955f }, { -0.035f, 0.955f } };
    static const PvP2 kShoeL[4] = { { -0.088f, 0.955f }, { -0.032f, 0.955f }, { -0.098f, 1.0f }, { -0.026f, 1.0f } };
    static const PvP2 kTorsoL[4] = { { -0.135f, 0.155f }, { 0.0f, 0.150f }, { -0.105f, 0.53f }, { 0.0f, 0.53f } };
    static const PvP2 kShirtV[4] = { { -0.030f, 0.152f }, { 0.030f, 0.152f }, { 0.0f, 0.30f }, { 0.0f, 0.30f } };
    static const PvP2 kArm[4] = { { -0.168f, 0.168f }, { -0.122f, 0.172f }, { -0.198f, 0.49f }, { -0.152f, 0.50f } };
    static const PvP2 kHand[4] = { { -0.198f, 0.49f }, { -0.152f, 0.50f }, { -0.194f, 0.565f }, { -0.156f, 0.57f } };
    static const PvP2 kNeck[4] = { { -0.028f, 0.115f }, { 0.028f, 0.115f }, { -0.032f, 0.162f }, { 0.032f, 0.162f } };
    PvP2 m[4];
    const float L = 0.25f;  // the lamp is on her left
    PvPart(b, cx, top, u, kLeg, kJeans0, kJeans1, 1, 1, L), PvMirror(kLeg, m), PvPart(b, cx, top, u, m, kJeans0, kJeans1, 1, 1, L);
    PvPart(b, cx, top, u, kShoeL, kShoe, kShoe, 1, 1, L), PvMirror(kShoeL, m), PvPart(b, cx, top, u, m, kShoe, kShoe, 1, 1, L);
    PvPart(b, cx, top, u, kTorsoL, kCoat0, kCoat1, 1, 1, L), PvMirror(kTorsoL, m), PvPart(b, cx, top, u, m, kCoat0, kCoat1, 1, 1, L);
    PvPart(b, cx, top, u, kShirtV, kShirt, kShirt, 1, 1, L);
    PvPart(b, cx, top, u, kArm, kCoat0, kCoat1, 1, 1, L), PvMirror(kArm, m), PvPart(b, cx, top, u, m, kCoat0, kCoat1, 1, 1, L);
    PvPart(b, cx, top, u, kHand, kSkin, kSkin, 1, 1, L), PvMirror(kHand, m), PvPart(b, cx, top, u, m, kSkin, kSkin, 1, 1, L);
    PvPart(b, cx, top, u, kNeck, kSkin, kSkin, 1, 1, L);
    PvHead(b, cx, top + 0.068f * u, 0.060f * u, 0.070f * u, kSkin, kHair, 1.0f);
}
// the ghost: c = its colour, a0 its alpha at the shoulders (fading to 0 at the hem); grow x the shape about its chest
static void PvGhost(PvBuf *b, float cx, float top, float u, const float c[3], float a0, float grow)
{
    static const PvP2 kTorsoL[4] = { { -0.110f, 0.165f }, { 0.0f, 0.160f }, { -0.085f, 0.50f }, { 0.0f, 0.50f } };
    static const PvP2 kArm[4] = { { -0.140f, 0.180f }, { -0.102f, 0.188f }, { -0.185f, 0.62f }, { -0.150f, 0.635f } };
    static const PvP2 kNeck[4] = { { -0.022f, 0.118f }, { 0.026f, 0.118f }, { -0.026f, 0.168f }, { 0.030f, 0.168f } };
    static const PvP2 kSkirtL[4] = { { -0.085f, 0.50f }, { 0.0f, 0.50f }, { -0.125f, 0.80f }, { 0.0f, 0.80f } };
    static const PvP2 kTatter[3][3] = { { { -0.125f, 0.80f }, { -0.040f, 0.80f }, { -0.095f, 0.97f } },
                                        { { -0.045f, 0.80f }, { 0.050f, 0.80f }, { 0.010f, 0.93f } },
                                        { { 0.045f, 0.80f }, { 0.125f, 0.80f }, { 0.085f, 1.00f } } };
    const float cy = top + 0.36f * u, ug = u * grow, tg = cy - (cy - top) * grow;  // grown about the chest
    const float aHem = 0.0f, aMid = a0 * 0.55f;
    PvP2 m[4];
    int k;
    PvPart(b, cx, tg, ug, kArm, c, c, a0, a0 * 0.35f, 0.0f), PvMirror(kArm, m), PvPart(b, cx, tg, ug, m, c, c, a0, a0 * 0.35f, 0.0f);
    PvPart(b, cx, tg, ug, kTorsoL, c, c, a0, aMid, 0.0f), PvMirror(kTorsoL, m), PvPart(b, cx, tg, ug, m, c, c, a0, aMid, 0.0f);
    PvPart(b, cx, tg, ug, kSkirtL, c, c, aMid, a0 * 0.2f, 0.0f), PvMirror(kSkirtL, m), PvPart(b, cx, tg, ug, m, c, c, aMid, a0 * 0.2f, 0.0f);
    for (k = 0; k < 3; ++k)
        PvTri(b, PvPt(cx + kTatter[k][0].x * ug, tg + kTatter[k][0].y * ug, PvArgb3(c, a0 * 0.2f)),
              PvPt(cx + kTatter[k][1].x * ug, tg + kTatter[k][1].y * ug, PvArgb3(c, a0 * 0.2f)),
              PvPt(cx + kTatter[k][2].x * ug, tg + kTatter[k][2].y * ug, PvArgb3(c, aHem)));
    PvPart(b, cx, tg, ug, kNeck, c, c, a0, a0, 0.0f);
    PvHead(b, cx + 0.018f * ug, tg + 0.070f * ug, 0.054f * ug, 0.068f * ug, c, c, a0);  // tilted a little
}
// a point of the right wall: sv 0 at the far wall .. 1 at the screen's edge E, fv 0 at its top .. 1 at its foot
// (wall: the far wall's X, top, bottom, then the wall's top and bottom at E)
static void PvWall(const float wall[5], float E, float sv, float fv, float o[2])
{
    const float tt = wall[1] + sv * (wall[3] - wall[1]), tb = wall[2] + sv * (wall[4] - wall[2]);
    o[0] = wall[0] + sv * (E - wall[0]), o[1] = tt + fv * (tb - tt);
}
// the room: the far wall, the walls, the floor and the ceiling in one-point perspective, a window, a lamp, a door, a
// rug, floor boards, a stain; the shadows under the figure and the cabinet
static void PvRoom(PvScene *s, PvBuf *b, PvBuf *g)
{
    static const float kPaperT[3] = { 0.33f, 0.34f, 0.29f }, kPaperB[3] = { 0.38f, 0.37f, 0.32f }, kWains[3] = { 0.26f, 0.18f, 0.12f };
    static const float kSideT[3] = { 0.27f, 0.28f, 0.24f }, kSideB[3] = { 0.31f, 0.30f, 0.26f }, kCeil[3] = { 0.20f, 0.19f, 0.18f };
    static const float kFloorF[3] = { 0.26f, 0.18f, 0.12f }, kFloorN[3] = { 0.34f, 0.24f, 0.16f }, kBoard[3] = { 0.16f, 0.11f, 0.08f };
    static const float kRug[3] = { 0.40f, 0.09f, 0.08f }, kRugB[3] = { 0.52f, 0.38f, 0.16f }, kFrame[3] = { 0.14f, 0.11f, 0.09f };
    static const float kGlass[3] = { 0.90f, 0.94f, 1.00f }, kGlassB[3] = { 0.62f, 0.72f, 0.86f }, kDoor[3] = { 0.26f, 0.15f, 0.10f };
    static const float kDoorF[3] = { 0.15f, 0.10f, 0.08f }, kStain[3] = { 0.22f, 0.06f, 0.04f }, kKnob[3] = { 0.85f, 0.70f, 0.36f };
    const float E = 0.5f * s->w / s->h + 0.04f, BX = 0.36f, BT = 0.14f, BB = 0.60f, VY = 0.40f;
    const float t = (E - BX) / BX, topE = VY - (VY - BT) * (1.0f + t), botE = VY + (BB - VY) * (1.0f + t);  // the walls at the edge
    float P[4][2];
    int i;
    // the ceiling, the far wall (wallpaper, a wainscot), the side walls, the floor
    P[0][0] = -E, P[0][1] = topE, P[1][0] = E, P[1][1] = topE, P[2][0] = -BX, P[2][1] = BT, P[3][0] = BX, P[3][1] = BT;
    PvQuadLit(b, s, P, 8, 3, kCeil, kCeil, 0);
    P[0][0] = -BX, P[0][1] = BT, P[1][0] = BX, P[1][1] = BT, P[2][0] = -BX, P[2][1] = 0.50f, P[3][0] = BX, P[3][1] = 0.50f;
    PvQuadLit(b, s, P, 14, 6, kPaperT, kPaperB, 1);
    P[0][1] = P[1][1] = 0.50f, P[2][1] = P[3][1] = BB;
    PvQuadLit(b, s, P, 8, 1, kWains, kWains, 0);
    P[0][0] = -E, P[0][1] = topE, P[1][0] = -BX, P[1][1] = BT, P[2][0] = -E, P[2][1] = botE, P[3][0] = -BX, P[3][1] = BB;
    PvQuadLit(b, s, P, 5, 6, kSideT, kSideB, 0);
    P[0][0] = BX, P[0][1] = BT, P[1][0] = E, P[1][1] = topE, P[2][0] = BX, P[2][1] = BB, P[3][0] = E, P[3][1] = botE;
    PvQuadLit(b, s, P, 5, 6, kSideT, kSideB, 0);
    {   // the floor: its sides on the walls' bottom lines (through the vanishing point), down past the screen's bottom
        const float kF = (1.06f - VY) / (BB - VY);
        P[0][0] = -BX, P[0][1] = BB, P[1][0] = BX, P[1][1] = BB, P[2][0] = -BX * kF, P[2][1] = 1.06f, P[3][0] = BX * kF, P[3][1] = 1.06f;
        PvQuadLit(b, s, P, 10, 6, kFloorF, kFloorN, 0);
    }
    for (i = -6; i <= 6; ++i) {  // floor boards: thin dark lines toward the vanishing point
        const float xb = 0.06f * (float)i, k = (1.06f - VY) / (BB - VY);
        P[0][0] = xb - 0.0012f, P[0][1] = BB, P[1][0] = xb + 0.0012f, P[1][1] = BB;
        P[2][0] = xb * k - 0.004f, P[2][1] = 1.06f, P[3][0] = xb * k + 0.004f, P[3][1] = 1.06f;
        PvQuadFlat(b, s, P, kBoard, 1.0f);
    }
    // the rug (a border, then the field) under the figure
    P[0][0] = -0.205f, P[0][1] = 0.635f, P[1][0] = 0.205f, P[1][1] = 0.635f, P[2][0] = -0.33f, P[2][1] = 0.84f, P[3][0] = 0.33f, P[3][1] = 0.84f;
    PvQuadLit(b, s, P, 4, 3, kRugB, kRugB, 0);
    P[0][0] = -0.19f, P[0][1] = 0.645f, P[1][0] = 0.19f, P[1][1] = 0.645f, P[2][0] = -0.305f, P[2][1] = 0.825f, P[3][0] = 0.305f, P[3][1] = 0.825f;
    PvQuadLit(b, s, P, 4, 3, kRug, kRug, 0);
    // the window on the far wall (left): a frame, cold bright glass, mullions
    P[0][0] = -0.315f, P[0][1] = 0.195f, P[1][0] = -0.145f, P[1][1] = 0.195f, P[2][0] = -0.315f, P[2][1] = 0.415f, P[3][0] = -0.145f, P[3][1] = 0.415f;
    PvQuadFlat(b, s, P, kFrame, 1.0f);
    {
        const float x0 = -0.302f, x1 = -0.158f, y0 = 0.208f, y1 = 0.402f;
        PvQuad(b, PvPt(PvPx(s, x0), PvPy(s, y0), PvArgb3(kGlass, 1)), PvPt(PvPx(s, x1), PvPy(s, y0), PvArgb3(kGlass, 1)),
               PvPt(PvPx(s, x0), PvPy(s, y1), PvArgb3(kGlassB, 1)), PvPt(PvPx(s, x1), PvPy(s, y1), PvArgb3(kGlassB, 1)));
        P[0][0] = -0.2335f, P[1][0] = -0.2265f, P[0][1] = P[1][1] = y0, P[2][0] = -0.2335f, P[3][0] = -0.2265f, P[2][1] = P[3][1] = y1;
        PvQuadFlat(b, s, P, kFrame, 1.0f);
        P[0][0] = P[2][0] = x0, P[1][0] = P[3][0] = x1, P[0][1] = P[1][1] = 0.300f, P[2][1] = P[3][1] = 0.307f;
        PvQuadFlat(b, s, P, kFrame, 1.0f);
        PvFan(g, PvPx(s, -0.23f), PvPy(s, 0.305f), 0.17f * s->h, 0.16f * s->h, 20, PvArgb(0.10f, 0.13f, 0.17f, 1), 0);  // its haze
    }
    {   // the door on the right wall: a frame, the door, a knob (wall points at s along the wall, f down from its top)
        const float s0 = 0.20f, s1 = 0.48f, f0 = 0.30f, wall[5] = { BX, BT, BB, topE, botE };
        float kn[2];
        PvWall(wall, E, s0 - 0.02f, f0 - 0.03f, P[0]), PvWall(wall, E, s1 + 0.02f, f0 - 0.03f, P[1]);
        PvWall(wall, E, s0 - 0.02f, 1.0f, P[2]), PvWall(wall, E, s1 + 0.02f, 1.0f, P[3]);
        PvQuadFlat(b, s, P, kDoorF, 1.0f);
        PvWall(wall, E, s0, f0, P[0]), PvWall(wall, E, s1, f0, P[1]), PvWall(wall, E, s0, 1.0f, P[2]), PvWall(wall, E, s1, 1.0f, P[3]);
        PvQuadLit(b, s, P, 2, 4, kDoor, kDoor, 0);
        PvWall(wall, E, s0 + 0.03f, 0.66f, kn);
        PvFanLit(b, s, kn[0], kn[1], 0.006f, 0.008f, 8, kKnob, 1.2f, 0.7f);
    }
    {   // a dark stain on the far wall, above the cabinet (an uneven fan)
        const float X = s->ghostXu + 0.115f, Y = 0.300f;
        int k;
        for (k = 0; k < 14; ++k) {
            const float a0 = 6.2831853f * (float)k / 14.0f, a1 = 6.2831853f * (float)(k + 1) / 14.0f;
            const float r0 = 0.020f * (1.0f + 0.45f * sinf(3.0f * a0 + 1.0f) + 0.25f * sinf(5.0f * a0)), r1 = 0.020f * (1.0f + 0.45f * sinf(3.0f * a1 + 1.0f) + 0.25f * sinf(5.0f * a1));
            PvTri(b, PvLit(s, kStain, X, Y, 1.0f), PvLit(s, kStain, X + r0 * cosf(a0), Y + 1.4f * r0 * sinf(a0) + (sinf(a0) > 0.5f ? 0.03f : 0.0f), 1.4f),
                  PvLit(s, kStain, X + r1 * cosf(a1), Y + 1.4f * r1 * sinf(a1) + (sinf(a1) > 0.5f ? 0.03f : 0.0f), 1.4f));
        }
    }
    // the figure's shadow on the rug
    PvFanLit(b, s, s->figXu, PV_FIG_FEET - 0.002f, 0.085f, 0.016f, 16, kRug, 0.30f, 1.0f);
}
// the cabinet (opaque, in front of the ghost's lower half): its front, its top, a drawer line, two knobs, its shadow
static void PvCabinet(PvScene *s, PvBuf *b)
{
    static const float kWood[3] = { 0.24f, 0.15f, 0.09f }, kTop[3] = { 0.34f, 0.23f, 0.15f }, kLine[3] = { 0.10f, 0.06f, 0.04f };
    static const float kKnob[3] = { 0.80f, 0.66f, 0.34f };
    const float x0 = s->ghostXu - 0.090f, x1 = s->ghostXu + 0.085f, yt = 0.500f, yb = PV_GHOST_FEET + 0.018f;
    float P[4][2];
    P[0][0] = x0 + 0.018f, P[0][1] = yt - 0.016f, P[1][0] = x1 + 0.006f, P[1][1] = yt - 0.016f, P[2][0] = x0, P[2][1] = yt, P[3][0] = x1, P[3][1] = yt;
    PvQuadFlat(b, s, P, kTop, 1.0f);
    P[0][0] = x0, P[0][1] = yt, P[1][0] = x1, P[1][1] = yt, P[2][0] = x0, P[2][1] = yb, P[3][0] = x1, P[3][1] = yb;
    PvQuadLit(b, s, P, 3, 3, kWood, kWood, 0);
    P[0][0] = x0 + 0.008f, P[1][0] = x1 - 0.008f, P[0][1] = P[1][1] = yt + 0.050f, P[2][0] = x0 + 0.008f, P[3][0] = x1 - 0.008f, P[2][1] = P[3][1] = yt + 0.054f;
    PvQuadFlat(b, s, P, kLine, 1.0f);
    PvFanLit(b, s, x0 + 0.045f, yt + 0.030f, 0.005f, 0.006f, 8, kKnob, 1.2f, 0.8f);
    PvFanLit(b, s, x1 - 0.045f, yt + 0.030f, 0.005f, 0.006f, 8, kKnob, 1.2f, 0.8f);
}
// Thermal's parts (after its palette): the footprints (fading behind the figure), then the figure in heat colours
static void PvHeatParts(PvScene *s, PvBuf *b)
{
    int i, k;
    for (i = 0; i < 4; ++i) {  // she came in from the left, back of the room: the older prints cooler and fainter
        const float X = s->figXu - 0.070f - 0.055f * (float)i, Y = PV_FIG_FEET - 0.006f - 0.012f * (float)i + ((i & 1) ? 0.008f : 0.0f);
        const float a = 0.90f - 0.17f * (float)i, k1 = 1.0f - 0.08f * (float)i;
        PvFan(b, PvPx(s, X), PvPy(s, Y), 0.020f * s->h * (1.0f - 0.06f * (float)i), 0.0080f * s->h, 10, PvArgb(1.0f, 0.42f * k1, 0.08f, a),
              PvArgb(1.0f, 0.30f * k1, 0.05f, 0.0f));
    }
    for (k = 0; k < s->nFig && b->n < b->max; ++k) {  // the figure: its own colours through the heat ramp, opaque
        float c[3], o[3];
        c[0] = PvChan(s->fig[k].c, 16), c[1] = PvChan(s->fig[k].c, 8), c[2] = PvChan(s->fig[k].c, 0);
        PvHeatRamp(c, 1.0f, o);
        b->v[b->n] = s->fig[k];
        b->v[b->n++].c = PvArgb3(o, 1.0f);
    }
}
// Spirit Vision's ghost glow (after its edges; additive, the whole ghost seen through the cabinet): two crimson / pale
// halos and the body in the glow of the live redraw (blood: lrp(L, deep, pale); white: GV_GLOW x (TEX x L + FLOOR)),
// x pulse (FxPulse)
static void PvSpiritParts(PvScene *s, PvBuf *b, int blood, float pulse)
{
    static const float kPale[3] = { 1.00f, 0.86f, 0.84f }, kDeep[3] = { 0.42f, 0.02f, 0.06f }, kWhite[3] = { 0.95f, 0.97f, 1.00f };
    static const float kGhost[3] = { 0.70f, 0.74f, 0.78f };
    const float L = (0.299f * kGhost[0] + 0.587f * kGhost[1] + 0.114f * kGhost[2]) * pulse;
    float body[3], halo[3], h1[3], h2[3];
    int k;
    for (k = 0; k < 3; ++k) {
        body[k] = blood ? SfSat(kDeep[k] + L * (kPale[k] - kDeep[k])) : SfSat(kWhite[k] * (0.90f * L + 0.25f * pulse));
        halo[k] = blood ? kDeep[k] * 1.6f : kWhite[k] * 0.55f;
        h1[k] = halo[k] * 0.30f * pulse, h2[k] = halo[k] * 0.16f * pulse;
    }
    PvFan(b, s->ghostX, s->ghostY - 0.62f * s->ghostH, 0.42f * s->ghostH, 0.62f * s->ghostH, 24, PvArgb3(h2, 1), 0);  // the aura
    PvGhost(b, s->ghostX, s->ghostY - s->ghostH, s->ghostH, h2, 1.0f, 1.24f);
    PvGhost(b, s->ghostX, s->ghostY - s->ghostH, s->ghostH, h1, 1.0f, 1.10f);
    PvGhost(b, s->ghostX, s->ghostY - s->ghostH, s->ghostH, body, 1.0f, 1.0f);
    for (k = 0; k < b->n; ++k) {  // additive: the alpha of PvGhost's fade to the hem goes into the colour (premultiplied)
        const float a = PvChan(b->v[k].c, 24);
        b->v[k].c = PvArgb(PvChan(b->v[k].c, 16) * a, PvChan(b->v[k].c, 8) * a, PvChan(b->v[k].c, 0) * a, 1.0f);
    }
}
// the whole scene for a w x h target (S = SH4WM_Scale2D, the 4:3 interface's width factor); 1 = built, nothing lost
static int PvSceneBuild(PvScene *s, float w, float h, float S)
{
    static const float kGhost[3] = { 0.66f, 0.72f, 0.78f };
    PvBuf b;
    int lost = 0;
    if (!(w >= 64.0f) || !(h >= 64.0f) || !(w < 20000.0f) || !(h < 20000.0f)) return 0;
    memset(s, 0, sizeof *s);
    s->w = w, s->h = h, s->S = S > 0.0f ? S : 1.0f;
    s->figXu = PvCanvasXu(s, PV_FIG_CX), s->ghostXu = PvCanvasXu(s, PV_GHOST_CX);
    s->figX = PvPx(s, s->figXu), s->figH = PV_FIG_H * h, s->figTop = PvPy(s, PV_FIG_FEET - PV_FIG_H);
    s->chestX = s->figX, s->chestY = s->figTop + PV_FIRE_CHEST * s->figH;
    s->ghostX = PvPx(s, s->ghostXu), s->ghostY = PvPy(s, PV_GHOST_FEET), s->ghostH = PV_GHOST_H * h;
    s->lampX = PvPx(s, PV_LAMP_X), s->lampY = PvPy(s, PV_LAMP_Y);
    {
        PvBuf g;
        b.v = s->room, b.n = 0, b.max = PV_ROOM_MAX, b.lost = 0;
        g.v = s->glow, g.n = 0, g.max = PV_GLOW_MAX, g.lost = 0;
        PvRoom(s, &b, &g);
        {   // the lamp: a cord, a shade, the bulb (opaque), its glow (additive)
            static const float kCord[3] = { 0.05f, 0.05f, 0.05f }, kShade[3] = { 0.42f, 0.34f, 0.22f };
            const float X = PV_LAMP_X, Y = PV_LAMP_Y;
            float P[4][2];
            P[0][0] = X - 0.0015f, P[1][0] = X + 0.0015f, P[0][1] = P[1][1] = -0.02f, P[2][0] = X - 0.0015f, P[3][0] = X + 0.0015f, P[2][1] = P[3][1] = Y - 0.03f;
            PvQuadFlat(&b, s, P, kCord, 1.0f);
            P[0][0] = X - 0.016f, P[1][0] = X + 0.016f, P[0][1] = P[1][1] = Y - 0.032f, P[2][0] = X - 0.042f, P[3][0] = X + 0.042f, P[2][1] = P[3][1] = Y;
            PvQuadFlat(&b, s, P, kShade, 1.0f);
            PvFan(&b, s->lampX, PvPy(s, Y + 0.004f), 0.014f * h, 0.011f * h, 14, PvArgb(1.0f, 1.0f, 0.94f, 1), PvArgb(1.0f, 0.90f, 0.68f, 1));
            PvFan(&g, s->lampX, PvPy(s, Y + 0.01f), 0.16f * h, 0.13f * h, 24, PvArgb(0.36f, 0.29f, 0.18f, 1), 0);
            PvFan(&g, s->lampX, PvPy(s, Y + 0.006f), 0.05f * h, 0.04f * h, 16, PvArgb(0.45f, 0.38f, 0.24f, 1), 0);
        }
        s->nRoom = b.n, s->nGlow = g.n, lost += b.lost + g.lost;
    }
    b.v = s->fig, b.n = 0, b.max = PV_FIG_MAX, b.lost = 0;
    PvFigure(&b, s->figX, s->figTop, s->figH);
    s->nFig = b.n, lost += b.lost;
    b.v = s->ghost, b.n = 0, b.max = PV_FIG_MAX, b.lost = 0;
    PvGhost(&b, s->ghostX, s->ghostY - s->ghostH, s->ghostH, kGhost, 0.40f, 1.0f);
    s->nGhost = b.n, lost += b.lost;
    b.v = s->front, b.n = 0, b.max = PV_FRONT_MAX, b.lost = 0;
    PvCabinet(s, &b);
    s->nFront = b.n, lost += b.lost;
    b.v = s->heat, b.n = 0, b.max = PV_HEAT_MAX, b.lost = 0;
    PvHeatParts(s, &b);
    s->nHeat = b.n, lost += b.lost + (b.n < 3 * 40 + s->nFig);
    b.v = s->spirit, b.n = 0, b.max = PV_SPIRIT_MAX, b.lost = 0;
    PvSpiritParts(s, &b, 1, 1.0f);
    s->nSpirit = b.n, lost += b.lost;
    s->built = !lost;
    return s->built;
}
// the spirit glow again (the pulse and the ghost colour change while the page is open): the same vertex count
static int PvSceneSpirit(PvScene *s, int blood, float pulse)
{
    PvBuf b;
    b.v = s->spirit, b.n = 0, b.max = PV_SPIRIT_MAX, b.lost = 0;
    PvSpiritParts(s, &b, blood, pulse);
    s->nSpirit = b.n;
    return !b.lost;
}

// ---------------------------------------------------------------- 5. the title scene's soul fire (spiritfire_sim.h)
// One person's fire at the figure's chest, stepped once a frame while the page is open (its own SfWorld), drawn as
// play's SvFlames draws: the flames 70 % above the anchor (the top edge sheared by lean), the round sprites rotated; world
// units -> pixels x k = the figure's height / PV_PERSON_U (world y is down, as the screen's). out: 6 vertices a sprite,
// premultiplied ones (smoke) first, then the additive; *nPre = the premultiplied vertex count.
static void PvFireIn(SfIn *in)
{
    memset(in, 0, sizeof *in);
    in->key = 0x50563356u, in->kind = SFK_PERSON, in->size = PV_FIRE_SIZE, in->dieS = 6.0f, in->hp = -1.0f, in->deadAge = -1.0f;
}
static int PvFireQuads(const SfSpr *spr, int n, float cx, float cy, float k, float unit, PvTV *out, int max, int *nPre)
{
    int pass, i, m, o = 0;
    *nPre = 0;
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < n && o + 6 <= max; ++i) {
            const SfSpr *q = &spr[i];
            float uv[4], rx = q->rx * k, ry = q->ry * k, x = cx + q->p[0] * k, y = cy + q->p[1] * k, mm, cs, sn;
            PvTV c[4];
            PvU32 col;
            if (q->add != pass) continue;
            mm = q->cell < SF_FRAMES ? ry : rx > ry ? rx : ry;
            if (!(mm > 0.0f) || !_finite(mm + x + y)) continue;
            if (mm < q->minPx * unit) rx *= q->minPx * unit / mm, ry *= q->minPx * unit / mm;
            SfCellUv(q->cell, uv);
            if (q->mirror) {
                const float t = uv[0];
                uv[0] = uv[2], uv[2] = t;
            }
            if (pass) {
                const float a = q->a;
                col = PvArgb(q->rgb[0] * a, q->rgb[1] * a, q->rgb[2] * a, 1.0f);
            } else {
                const float a = SfSat(q->a);
                col = PvArgb(q->rgb[0] * a, q->rgb[1] * a, q->rgb[2] * a, a);
            }
            cs = cosf(q->rot), sn = sinf(q->rot);
            for (m = 0; m < 4; ++m) {
                const float ux = (m & 1) ? 1.0f : -1.0f, uy = (m & 2) ? 1.0f : -1.0f;
                if (q->cell < SF_FRAMES) {
                    c[m].x = x + ux * rx + ((m & 2) ? 0.0f : q->lean * ry);
                    c[m].y = y + ((m & 2) ? 0.30f : -0.70f) * ry;
                } else {
                    c[m].x = x + (ux * cs - uy * sn) * rx;
                    c[m].y = y + (ux * sn + uy * cs) * ry;
                }
                c[m].u = (m & 1) ? uv[2] : uv[0];
                c[m].v = (m & 2) ? uv[3] : uv[1];
                c[m].c = col;
            }
            out[o++] = c[0], out[o++] = c[1], out[o++] = c[2];
            out[o++] = c[1], out[o++] = c[3], out[o++] = c[2];
        }
        if (!pass) *nPre = o;
    }
    return o;
}
#endif
