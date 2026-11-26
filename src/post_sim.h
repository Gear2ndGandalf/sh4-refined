// v3.43 PP343: the post effects' pure code (Bloom, Colour Grading, Otherworld Lens on the Screen Effects page): the ini /
// row values, the looks (bloom thresholds and weights, the grading presets as the ps.1.1 shader computes them), the bloom
// chain's plan (down in 2x steps, Kawase passes at the smallest level, up in 2x steps, the composite from the half size:
// no draw stretches more than 2x, as shadows_sim.h's chain), the lens' strength from the game's haunting signals and its
// fade, the lens' warped texture coordinates, which filters each effect runs under. No device code: post.inc draws,
// test\post_sim_test.c checks, tools\post_mockup.py mirrors the numbers for the mock-ups.
#ifndef POST_SIM_H
#define POST_SIM_H
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>  // v3.47 PX347: abs (the pixel checks)

// ---------------------------------------------------------------- values (rows and [ScreenEffects] keys)
// v3.52 FX352 (USER_FEEDBACK_v351 D7): Off / Low / Medium / High / Ultra (was Off / Low / High / Very High = 0..3)
enum { PPB_OFF, PPB_LOW, PPB_MED, PPB_HIGH, PPB_ULTRA, PPB_N };   // Bloom (v3.44 CS344: + Very High; v3.52 FX352: 5 levels)
enum { PPG_OFF, PPG_PS2, PPG_OTHER, PPG_APT, PPG_BLEACH, PPG_N };  // Colour Grading
enum { PPL_OFF, PPL_ON, PPL_N };                                  // Otherworld Lens
enum { PPE_BLOOM, PPE_GRADE, PPE_LENS, PPE_N };                   // the three rows (Screen Effects rows FX_N + k)
#define PP_VMAX 5
static const int kPpCount[PPE_N] = { PPB_N, PPG_N, PPL_N };
static const char *const kPpIniKey[PPE_N] = { "Bloom", "Grading", "Lens" };
static const char *const kPpName[PPE_N][PP_VMAX] = {
    { "Off", "Low", "Medium", "High", "Ultra" },  // v3.52 FX352 (v3.44 .. v3.51: Off, Low, High, Very High)
    { "Off", "PS2", "Otherworld", "Apartment", "Bleach" },
    { "Off", "On" },
};

// v3.44 CS344: a and b equal ignoring case and blanks ("Very High", "VeryHigh", "very high")
static int PpSameName(const char *a, const char *b)
{
    for (;;) {
        while (*a == ' ') ++a;
        while (*b == ' ' || *b == '\t') ++b;
        if (!*a || !*b) return !*a && !*b;
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        ++a, ++b;
    }
}
// A value name (any case, surrounding blanks ignored) or its digit: the index, -1 when it is not one
static int PpParse(int row, const char *s)
{
    char b[32];
    int n = 0, v;
    if (row < 0 || row >= PPE_N || !s) return -1;
    while (*s == ' ' || *s == '\t') ++s;
    while (s[n] && n < (int)sizeof b - 1) b[n] = s[n], ++n;
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t')) --n;
    b[n] = 0;
    if (!n) return -1;
    if (n == 1 && b[0] >= '0' && b[0] <= '9') return b[0] - '0' < kPpCount[row] ? b[0] - '0' : -1;
    for (v = 0; v < kPpCount[row]; ++v)
        if (PpSameName(kPpName[row][v], b)) return v;  // v3.44 CS344: blanks inside ignored too
    return -1;
}
// v3.43 REV343 PP343-001: a pass's put-back runs once: from its tail or from the fault handler, whichever comes first
static int PpOnceTake(int *armed)
{
    if (!armed || !*armed) return 0;
    *armed = 0;
    return 1;
}
// what a pause copy was made with (the frozen copy is drawn again when this changes during the pause)
// v3.52 FX352: bloom 3 bits (5 levels; was 2)
static int PpKey(int bloom, int grade, int lens) { return (bloom & 7) | (grade & 7) << 3 | (lens & 1) << 6; }

// ---------------------------------------------------------------- which filters each effect runs under (Filter ids)
// None, CRT, Vivid, Sepia, Old Film, Night Vision, Night Vision Mono, Thermal, Spirit Vision, Vignette (FXF_*).
// Grading only under the filters that keep the colours; bloom not under the night visions (their own highlight flare),
// Thermal (a lamp is not hot) or Spirit Vision (a black world of edges); the lens everywhere but Spirit Vision.
#define PP_FILTERS 10
static const unsigned char kPpGradeWith[PP_FILTERS] = { 1, 1, 1, 0, 0, 0, 0, 0, 0, 1 };
static const unsigned char kPpBloomWith[PP_FILTERS] = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 };
static const unsigned char kPpLensWith[PP_FILTERS] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1 };
static int PpWith(const unsigned char *t, int filter) { return filter >= 0 && filter < PP_FILTERS ? t[filter] : 0; }

// ---------------------------------------------------------------- bloom
// Bright pass (per channel, so a lamp keeps its colour): B = sat((m - th) x gain), m = the mean of 16 frame texels (four
// bilinear taps); down to 1/2, 1/4, 1/8, 1/16 of the frame; two Kawase passes there; up: each level = (1 - up) x its own
// + up x the tent of the level below (SRCALPHA blend: never above 1); the composite: frame' = frame + k B - frame k B
// (screen: ONE / INVSRCCOLOR, highlights never clip harder than the frame did).
typedef struct { float th, gain, k, up; } PpBloomLook;
// v3.52 FX352 (USER_FEEDBACK_v351 D7: "hardly any bloom on low or high"; the user plays Very High): recalibrated so each
// step is clearly visible, measured with tools\post_mockup.py's chain on its test room and a darker copy of it (SH4's
// rooms are dark: the threshold decides how much of a frame can glow at all). The glow added (mean, 0..1), dark room /
// test room: v3.51 Low 0.006 / 0.013, High 0.013 / 0.023, Very High 0.028 / 0.039; v3.52 Low 0.020 / 0.030, Medium
// 0.030 / 0.041, High 0.045 / 0.059, Ultra 0.067 / 0.086 (each step about 1.5x the one below; High 1.6x v3.51's Very
// High, Ultra 2.4x). k may now exceed 1: the composite shader multiplies by c1 = k / 2 with _x2 (screenfx.inc kPsPpComp),
// so a constant stays inside ps.1.1's -1..1 up to k = 2; gain / 4 <= 1 as before (the bright pass's c2).
static const PpBloomLook kPpBloom[PPB_N] = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.46f, 3.2f, 0.95f, 0.86f },   // Low: lamps, windows and flames glow softly (about v3.51's High and a half)
    { 0.38f, 3.6f, 1.00f, 0.89f },   // Medium: a clear glow (about v3.51's Very High; v3.51's High is migrated here)
    { 0.32f, 4.0f, 1.20f, 0.92f },   // High: strong and wide (v3.51's Very High is migrated here: 1.6x its glow)
    { 0.26f, 4.0f, 1.60f, 0.94f },   // Ultra: a dreamy haze around every light (the dark quarter of the picture never glows)
};
// v3.52 FX352: the one-time migration of a v3.43 .. v3.51 Bloom value. Without the hidden [ScreenEffects] BloomV marker
// (absent or lower than PP_BLOOM_VER) the value is read with its old meaning: Off / Low the same, High -> Medium, Very
// High -> High. With the marker: the new names (a hand-written "Very High" still reads as High). "Medium" and "Ultra"
// never existed before: the same either way. The digits mean the same either way too (old 2 = High -> Medium = new 2,
// old 3 = Very High -> High = new 3; 4 = Ultra). Only the word "High" differs between the two meanings.
#define PP_BLOOM_VER 352
#define PP_BLOOM_VER_S "352"
static int PpBloomParse(const char *s, int legacy)
{
    char b[32];
    int n = 0;
    if (!s) return -1;
    while (*s == ' ' || *s == '\t') ++s;
    while (s[n] && n < (int)sizeof b - 1) b[n] = s[n], ++n;
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '\r' || b[n - 1] == '\n')) --n;
    b[n] = 0;
    if (!n) return -1;
    if (n == 1 && b[0] >= '0' && b[0] <= '4') return b[0] - '0';  // PPB_OFF .. PPB_ULTRA, either meaning
    if (PpSameName("Off", b)) return PPB_OFF;
    if (PpSameName("Low", b)) return PPB_LOW;
    if (PpSameName("Medium", b)) return PPB_MED;
    if (PpSameName("High", b)) return legacy ? PPB_MED : PPB_HIGH;
    if (PpSameName("Very High", b)) return PPB_HIGH;
    if (PpSameName("Ultra", b)) return PPB_ULTRA;
    return -1;
}
// s = the ini text, marker = [ScreenEffects] BloomV (0 = absent). The level (-1 = not a value); *rewrite = 1 when the
// line must be written as kPpName[PPE_BLOOM][level] (its text is not already that name); *mark = 1: the marker must be
// written (a legacy value, the line read with its old meaning).
static int PpBloomMigrate(const char *s, int marker, int *rewrite, int *mark)
{
    const int legacy = marker < PP_BLOOM_VER, v = PpBloomParse(s, legacy);
    *rewrite = legacy && v >= 0 && !PpSameName(kPpName[PPE_BLOOM][v], s);
    *mark = legacy && v >= 0;
    return v;
}
// How a chosen level is written (DoF's DofStrWriteOrder, v3.45 QS345-03): the marker on disk: the value only; else an
// unambiguous value first, then the marker (a failure of either leaves a same-meaning ini); "High" (legacy = Medium)
// the marker first, then the value, the marker taken back if the value fails (the old line keeps its old meaning).
enum { PPBW_VALUE, PPBW_VALUE_MARK, PPBW_MARK_VALUE };
static int PpBloomWriteOrder(int v, int markedOnDisk)
{
    return markedOnDisk ? PPBW_VALUE : v == PPB_HIGH ? PPBW_MARK_VALUE : PPBW_VALUE_MARK;
}
#define PP_LEVELS   4      // 1/2 .. 1/16 of the frame
#define PP_RT_MIN   8      // no level smaller than this (a tiny window)
#define PP_KAWASE_A 1.5f   // the two blur passes' tap offsets (texels of the smallest level)
#define PP_KAWASE_B 2.5f
#define PP_UP_TAP   0.75f  // the up passes' tent taps (texels of the level read)
#define PP_COMP_TAP 0.5f   // the composite's taps (half-size texels)
enum { PPS_BRIGHT, PPS_DOWN, PPS_BLUR, PPS_UP, PPS_COMP };
enum { PPR_FRAME = 0, PPR_L1, PPR_L2, PPR_L3, PPR_L4, PPR_X, PPR_BACK, PPR_N };  // buffers (L1 = half size; X = the blur's second)
typedef struct { int kind, src, dst, dw, dh; float du, dv; } PpStep;  // dw x dh = the draw's size; du / dv = the tap offset (uv)
typedef struct { int w[PPR_N], h[PPR_N], levels, nsteps; PpStep step[16]; } PpPlan;
static int PpHalf(int n) { return n / 2 > PP_RT_MIN ? (n + 1) / 2 : PP_RT_MIN; }
// The draws for a w x h frame: 1 if made
static int PpMakePlan(int w, int h, PpPlan *p)
{
    int k, n = 0, last;
    memset(p, 0, sizeof *p);
    if (w < 64 || h < 64 || w > 16384 || h > 16384) return 0;
    p->w[PPR_FRAME] = p->w[PPR_BACK] = w;
    p->h[PPR_FRAME] = p->h[PPR_BACK] = h;
    for (k = PPR_L1; k <= PPR_L4; ++k) p->w[k] = PpHalf(p->w[k - 1]), p->h[k] = PpHalf(p->h[k - 1]);
    p->levels = PP_LEVELS;
    last = PPR_L1 + p->levels - 1;
    p->w[PPR_X] = p->w[last], p->h[PPR_X] = p->h[last];
    for (k = PPR_L1; k <= last; ++k) {  // down: taps 1 source texel off the destination pixel's centre (a 4 x 4 mean)
        PpStep *s = &p->step[n++];
        s->kind = k == PPR_L1 ? PPS_BRIGHT : PPS_DOWN;
        s->src = k == PPR_L1 ? PPR_FRAME : k - 1;
        s->dst = k;
        s->dw = p->w[k], s->dh = p->h[k];
        s->du = 1.0f / (float)p->w[s->src], s->dv = 1.0f / (float)p->h[s->src];
    }
    {   // Kawase at the smallest level: there and back
        PpStep *a = &p->step[n++], *b = &p->step[n++];
        a->kind = b->kind = PPS_BLUR;
        a->src = last, a->dst = PPR_X, b->src = PPR_X, b->dst = last;
        a->dw = b->dw = p->w[last], a->dh = b->dh = p->h[last];
        a->du = PP_KAWASE_A / (float)p->w[last], a->dv = PP_KAWASE_A / (float)p->h[last];
        b->du = PP_KAWASE_B / (float)p->w[last], b->dv = PP_KAWASE_B / (float)p->h[last];
    }
    for (k = last - 1; k >= PPR_L1; --k) {  // up: into the level above, blended over its own content
        PpStep *s = &p->step[n++];
        s->kind = PPS_UP;
        s->src = k + 1, s->dst = k;
        s->dw = p->w[k], s->dh = p->h[k];
        s->du = PP_UP_TAP / (float)p->w[k + 1], s->dv = PP_UP_TAP / (float)p->h[k + 1];
    }
    {
        PpStep *s = &p->step[n++];
        s->kind = PPS_COMP;
        s->src = PPR_L1, s->dst = PPR_BACK;
        s->dw = w, s->dh = h;
        s->du = PP_COMP_TAP / (float)p->w[PPR_L1], s->dv = PP_COMP_TAP / (float)p->h[PPR_L1];
    }
    p->nsteps = n;
    return 1;
}
static unsigned PpPlanBytes(const PpPlan *p)  // our render targets (A8R8G8B8)
{
    unsigned b = 0;
    int k;
    for (k = PPR_L1; k <= PPR_X; ++k)
        if (k < PPR_L1 + p->levels || k == PPR_X) b += (unsigned)p->w[k] * (unsigned)p->h[k] * 4u;
    return b;
}
// the bright pass on one colour (the shader: lrp x3 = the mean, sub th, mul_x2 by gain / 4, add_sat x2)
static float PpBright(const PpBloomLook *L, float m)
{
    float v = (m - L->th) * (L->gain * 0.25f) * 2.0f;
    v = v + v;
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}
static float PpScreen(float a, float b) { return a + b - a * b; }  // ONE / INVSRCCOLOR
// v3.44 CS344: a look's glow strength: k x the mean bright pass over frame values 0..1 (High 0.287; Very High 1.6x that)
static float PpBloomEnergy(const PpBloomLook *L)
{
    float s = 0.0f;
    int i;
    for (i = 0; i < 1000; ++i) s += PpBright(L, ((float)i + 0.5f) / 1000.0f);
    return L->k * s / 1000.0f;
}

// ---------------------------------------------------------------- colour grading (per-channel curves, no LUT)
// GOG's d3d8 wrapper drew nothing for a dependent texture read (texreg2gb, v3.16.0 live), so the grade is computed:
// L = luma; c' = lrp(f, L, c) (f = 1 - saturation); c' x gain + offset + (1 - L) shadow tint + L^2 highlight tint.
typedef struct { float sat, gain[3], off[3], sh[3], hi[3]; } PpGradeLook;
static const PpGradeLook kPpGrade[PPG_N] = {
    { 1.00f, { 1.00f, 1.00f, 1.00f }, { 0.000f, 0.000f, 0.000f }, { 0.000f, 0.000f, 0.000f }, { 0.000f, 0.000f, 0.000f } },
    // PS2: greener, slightly foggy (the blacks lifted green-grey, a little less contrast and colour)
    { 0.85f, { 0.92f, 0.97f, 0.90f }, { 0.030f, 0.042f, 0.030f }, { 0.000f, 0.012f, 0.002f }, { -0.015f, 0.008f, -0.025f } },
    // Otherworld: cold and drained (blue-grey shadows, the highlights a pale cyan)
    { 0.55f, { 0.92f, 0.98f, 1.05f }, { 0.000f, 0.004f, 0.014f }, { -0.008f, 0.000f, 0.022f }, { -0.030f, 0.000f, 0.025f } },
    // Apartment: warm (amber light, brown shadows)
    { 1.06f, { 1.08f, 1.00f, 0.86f }, { 0.010f, 0.004f, 0.000f }, { 0.014f, 0.004f, -0.008f }, { 0.050f, 0.018f, -0.035f } },
    // Bleach: the bleach-bypass film look (half the colour, harder contrast)
    { 0.55f, { 1.22f, 1.22f, 1.20f }, { -0.050f, -0.050f, -0.045f }, { 0.000f, 0.000f, 0.000f }, { 0.000f, 0.000f, 0.000f } },
};
#define PP_LUMA_R 0.299f
#define PP_LUMA_G 0.587f
#define PP_LUMA_B 0.114f
// The shader's constants c0..c5 (each of the 8 instructions reads at most one): c0 luma, c1 f = 1 - sat, c2 gain / 2,
// c3 offset + shadow tint, c4 -shadow tint (v3.44 CS344: (1 - L) sh = sh - L sh, no `1-x` modifier), c5 highlight tint.
// 0 = a constant outside ps.1.1's -1..1 (the preset is not drawn).
static int PpGradeConsts(const PpGradeLook *g, float c[6][4])
{
    int i, k, ok = 1;
    memset(c, 0, sizeof(float) * 24);
    c[0][0] = PP_LUMA_R, c[0][1] = PP_LUMA_G, c[0][2] = PP_LUMA_B;
    for (i = 0; i < 3; ++i) {
        c[1][i] = 1.0f - g->sat;
        c[2][i] = g->gain[i] * 0.5f;
        c[3][i] = g->off[i] + g->sh[i];  // v3.44 CS344
        c[4][i] = -g->sh[i];             // v3.44 CS344
        c[5][i] = g->hi[i];
    }
    c[1][3] = 1.0f - g->sat;  // (alpha: not written, COLORWRITEENABLE RGB)
    for (k = 0; k < 6; ++k)
        for (i = 0; i < 4; ++i)
            if (!(c[k][i] >= -1.0f && c[k][i] <= 1.0f)) ok = 0;
    return ok;
}
static float PpSat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
// one pixel as the shader computes it (rgb 0..1 in, out); intermediate results clamped to -8..8 as ps.1.1's range
static void PpGradePixel(const PpGradeLook *g, const float in[3], float out[3])
{
    float c[6][4], L, L2;
    int i;
    PpGradeConsts(g, c);
    L = in[0] * c[0][0] + in[1] * c[0][1] + in[2] * c[0][2];
    L2 = L * L;
    for (i = 0; i < 3; ++i) {
        float r = c[1][i] * L + (1.0f - c[1][i]) * in[i];  // lrp r0, c1, r1, t0
        r = r * c[2][i] * 2.0f;                               // mul_x2
        r = r + c[3][i];                                      // add (offset + shadow tint)
        r = L * c[4][i] + r;                                  // mad r0, r1, c4, r0 (- L shadow tint; v3.44 CS344)
        out[i] = PpSat(r + L2 * c[5][i]);                     // mul r1, r1, r1; mul r1, r1, c5; add_sat
    }
}

// ---------------------------------------------------------------- Otherworld Lens
// Strength: from the game's haunting step 0x539920 (src/haunting/haunt_539840.c): [0x10A3318] the strength sum of the
// auras registered this frame (ghosts' and Room 302's hauntings; 0 = none), [0x10A3320] the same weighted by
// (1 - distance / radius) at the player (0 outside every aura). nearness = falloff / sum (0..1, no unit); target = BASE
// while a haunting is registered, up to 1 as she nears it; hidden LensOtherworld=1: at least OW in every Otherworld
// stage (not Room 302, stages 1 / 0x0A). Fades: in over IN_S, out over OUT_S (real time).
// v3.52 FX352 (USER_FEEDBACK_v351 D6: "not sure it is working outside our preview"): who registers a haunting this
// frame (func_0053A8D0, summed by HauntingSumAuraStrength 0x539920; none while the player is frozen, none in the first
// 10 frames with auras): the ghosts' EnemyHauntingAuraRegister 0x4509E0 (Cynthia and Victim 13 0xC, the victim ghosts
// 0xB, Jasper 0xD, Andrew DeSalvo 0xE, the Wheelchair 6, the scratch wall 0x4B9810), the companion Eileen's curse aura
// (radius 800, strength curse x 2.5: the second half, once her curse shows) and Room 302's hauntings (0x547010). The
// v3.51 log: 23 fades in, every one at "nearness 0.00: 40 %" (the Subway World's ghosts registered all over the room;
// the player rarely inside an aura's radius): the lens ran at its base of 40 %: red and blue about 7 px apart at the
// corners of 2560 x 1440 (17 at full strength), a 1.6 % pull, hard to see in SH4's dark corners and invisible in the
// middle of the screen. The base is now 70 % (hidden [ScreenEffects] LensBase, 10..100 %,
// 40 = v3.51); full strength halfway into an aura as before; the page's preview stays at full strength (unchanged).
#define PP_LENS_BASE   0.70f   // v3.52 FX352: was 0.40
#define PP_LENS_BASE_V351 0.40f
#define PP_LENS_NEAR_K 2.0f    // nearness x this, clamped: full strength halfway into an aura
#define PP_LENS_OW     0.35f
#define PP_LENS_IN_S   1.2f
#define PP_LENS_OUT_S  2.5f
#define PP_LENS_MIN    0.01f   // below this nothing is drawn
// v3.52 FX352: the base as a parameter (LensBase); out of 0.10 .. 1.00 (or NaN): PP_LENS_BASE
static float PpLensTargetB(float sum, float falloff, int stage, int otherworld, int always, float base)
{
    float t = 0.0f;
    if (!(base >= 0.10f && base <= 1.0f)) base = PP_LENS_BASE;
    if (always) return 1.0f;
    if (sum > 0.0f && sum < 1e6f && falloff == falloff) {
        const float n = PpSat(falloff / sum);
        t = base + (1.0f - base) * PpSat(n * PP_LENS_NEAR_K);
    }
    if (otherworld && stage != 1 && stage != 0x0A && stage > 0 && t < PP_LENS_OW) t = PP_LENS_OW;
    return t;
}
static float PpLensTarget(float sum, float falloff, int stage, int otherworld, int always)
{
    return PpLensTargetB(sum, falloff, stage, otherworld, always, PP_LENS_BASE);
}
// v3.52 FX352: [ScreenEffects] LensBase (percent, hidden): 10..100; anything else = the default (*ok 0: the caller logs it)
static float PpLensBaseParse(int pct, int *ok)
{
    *ok = pct >= 10 && pct <= 100;
    return *ok ? (float)pct / 100.0f : PP_LENS_BASE;
}
// v3.52 FX352: a room's lens time (post.inc's room line: when the lens shows, how strong): fed every gameplay frame
typedef struct { float secs, shown, maxS, maxSum, maxNear; } PpLensRoom;
static void PpLensRoomAdd(PpLensRoom *r, float dt, float target, float s, float sum, float nearness)
{
    if (!(dt > 0.0f)) return;
    if (dt > 0.25f) dt = 0.25f;  // a hitch or a load (as PpLensStep)
    r->secs += dt;
    if (s >= PP_LENS_MIN || target > 0.0f) r->shown += dt;
    if (s > r->maxS) r->maxS = s;
    if (sum == sum && sum > r->maxSum && sum < 1e6f) r->maxSum = sum;
    if (nearness == nearness && nearness > r->maxNear) r->maxNear = nearness > 1.0f ? 1.0f : nearness;
}
static float PpLensStep(float cur, float target, float dt)
{
    if (!(dt > 0.0f)) return cur == cur ? cur : 0.0f;
    if (dt > 0.25f) dt = 0.25f;  // a hitch or a load: no jump
    if (!(cur == cur)) cur = 0.0f;
    if (target > cur) {
        cur += dt / PP_LENS_IN_S;
        if (cur > target) cur = target;
    } else if (target < cur) {
        cur -= dt / PP_LENS_OUT_S;
        if (cur < target) cur = target;
    }
    return PpSat(cur);
}
// The warp: channel ch (0 R, 1 G, 2 B) of the point (u, v) samples the frame at a point nearer the centre by
// k r^2 of its distance (r^2 = 1 at the corners, aspect-correct): a gentle bulge toward the edges that breathes slowly,
// red a little further in than blue (a fringe outward of light edges). Every sample stays inside the frame.
#define PP_LENS_WARP      0.040f   // the corners' pull at full strength
#define PP_LENS_FRINGE    0.0065f  // red / blue apart at the corners (a share of the distance; about 0.3 % of the width)
#define PP_LENS_BREATHE   0.30f    // the warp breathes +-30 % ...
#define PP_LENS_BREATHE_S 6.0f     // ... once in this many seconds
#define PP_LENS_GX 16              // the mesh drawn (the warp is smooth: a 16 x 10 grid of quads)
#define PP_LENS_GY 10
static float PpLensK(float s, float t, int ch)
{
    const float br = 1.0f + PP_LENS_BREATHE * sinf(6.2831853f * t / PP_LENS_BREATHE_S);
    return s * (PP_LENS_WARP * br + (float)(1 - ch) * PP_LENS_FRINGE);
}
static void PpLensUv(float u, float v, float aspect, float k, float *ou, float *ov)
{
    const float dx = u - 0.5f, dy = v - 0.5f, a2 = aspect * aspect;
    const float r2 = (dx * dx * a2 + dy * dy) / (0.25f * a2 + 0.25f);
    float sc = 1.0f - k * r2;
    if (sc > 1.0f) sc = 1.0f;
    if (sc < 0.5f) sc = 0.5f;
    *ou = 0.5f + dx * sc;
    *ov = 0.5f + dy * sc;
}

// ---------------------------------------------------------------- v3.44 CS344: ps.1.1 tokens, the proven forms, the pixel check
// USER_FEEDBACK_v343 items 1 / 6: the shadow Darkness and Colour Grading reached the device (c1 read back right) but not
// the pixels. Refined's working shaders (the filters, bloom, the lens) take runtime constants the same way, so constants
// are not the cause. The two broken shaders were the only ones in use with the `1-x` source modifier (v3.41's shadow
// composite `mad r1, r0, 1-r0, r0`, v3.43's grade `mad r0, 1-r1, c4, r0`): GOG's translator accepts it and then draws
// as if no shader were set (stage 0 = texture x diffuse, alpha = the texture's): the grade showed the frame unchanged and
// the soft shadow its coverage, pitch black inside at any Darkness (the user's v3.41 "pitch black"). Both are rebuilt
// from the proven forms only; PsLint checks every Refined shader against that list (test and log), and a fixed test
// draw read back proves the pixels (post.inc PxCheck).
#ifndef PS_VER  // screenfx.inc defines the same tokens before it includes this file; the native tests need them here
#define PS_VER   0xFFFF0101u
#define PS_END   0x0000FFFFu
#define PS_TEX(n) 0x00000042u, (0xB00F0000u | (n))
#define D_R(n)   (0x800F0000u | (n))
#define D_SAT    0x00100000u
#define D_X2     0x01000000u
#define D_X4     0x02000000u
#define S_R(n)   (0x80E40000u | (n))
#define S_C(n)   (0xA0E40000u | (n))
#define S_T(n)   (0xB0E40000u | (n))
#define S_RA(n)  (0x80FF0000u | (n))
#define S_BIAS   0x02000000u
#define S_INV    0x06000000u
#define I_ADD 2u
#define I_SUB 3u
#define I_MAD 4u
#define I_MUL 5u
#define I_DP3 8u
#define I_LRP 0x12u
#endif
typedef unsigned long PsTok;  // = DWORD (CreatePixelShader's token type)

// GRADE (screenfx.inc FXT_PPGRADE; PpGradePixel is its sim): c0 luma, c1 1 - saturation, c2 gain / 2, c3 offset + shadow
// tint, c4 -shadow tint, c5 highlight tint. out = sat(gain lrp(f, L, c) + off + (1 - L) sh + L^2 hi), v3.43's formula
static const PsTok kPpGradePs[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_LRP, D_R(0), S_C(1), S_R(1), S_T(0),         // saturation
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(2),          // x gain
    I_ADD, D_R(0), S_R(0), S_C(3),                 // + offset + shadow tint
    I_MAD, D_R(0), S_R(1), S_C(4), S_R(0),         // - L x shadow tint
    I_MUL, D_R(1), S_R(1), S_R(1),                 // L^2
    I_MUL, D_R(1), S_R(1), S_C(5),                 // x highlight tint
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(1),
    PS_END };

// The forms proven through GOG's d3d8 -> d3d9 translator (knowledge base sections 110 / 111 / 116, live v3.15 .. v3.43):
// tex first; then add / sub / mul / mad / dp3 / lrp, each writing r0 / r1 with the full .rgba mask (results _sat, _x2,
// _x4), reading r0 / r1, c0..c7, t0..t3 with the .rgba swizzle and no source modifier (`1-x`: v3.44; `_bias`, `.a`
// replicate, texreg2gb: earlier failures), at most one constant per instruction (v3.15), at most 8 arithmetic
// instructions. 0 = every instruction proven; else the first offence (*why) and its instruction index (*at).
enum { PSL_OK, PSL_VERSION, PSL_OPCODE, PSL_TEXLATE, PSL_DEST, PSL_SRC, PSL_MOD, PSL_SWZ, PSL_TWOC, PSL_LONG, PSL_END, PSL_N };
static const char *const kPsLintWhy[PSL_N] = { "proven", "not ps.1.1", "an opcode not proven (or co-issued)",
                                               "a tex after arithmetic", "a destination not proven (mask, register, modifier)",
                                               "a source register not proven", "a source modifier (1-x, _bias, ...)",
                                               "a swizzle (.a replicate ...)", "two constants in one instruction",
                                               "more than 8 arithmetic instructions", "no end token" };
static int PsLint(const PsTok *p, int maxTok, int *at, const char **why)
{
    int i = 1, ins = 0, arith = 0, e = PSL_OK;
    if (at) *at = -1;
    if (!p || maxTok < 2 || p[0] != PS_VER) e = PSL_VERSION;
    while (!e) {
        unsigned long op, d;
        int ns, k, nc = 0;
        if (i >= maxTok) { e = PSL_END; break; }
        op = p[i];
        if (op == PS_END) break;
        ns = op == 0x42u ? 0 : op == I_ADD || op == I_SUB || op == I_MUL || op == I_DP3 ? 2 : op == I_MAD || op == I_LRP ? 3 : -1;
        if (ns < 0) { e = PSL_OPCODE; break; }
        if (i + 1 + ns >= maxTok) { e = PSL_END; break; }
        d = p[i + 1];
        if (op == 0x42u) {
            if (arith) { e = PSL_TEXLATE; break; }
            if ((d & 0xFFFFF800u) != 0xB00F0000u || (d & 0x7FFu) > 3) { e = PSL_DEST; break; }
        } else {
            const unsigned long rm = (d >> 20) & 0xFu, sh = (d >> 24) & 0xFu;
            if ((d & 0xF00F0000u) != 0x800F0000u || (d & 0x7FFu) > 1 || rm > 1 || sh > 2 || (d & 0x0000F800u)) { e = PSL_DEST; break; }
            if (++arith > 8) { e = PSL_LONG; break; }
        }
        for (k = 0; k < ns && !e; ++k) {
            const unsigned long s = p[i + 2 + k], ty = (s >> 28) & 7u, n = s & 0x7FFu;
            if (!(s & 0x80000000u) || (s & 0x0000F800u) || !((ty == 0 && n <= 1) || (ty == 2 && n <= 7) || (ty == 3 && n <= 3))) e = PSL_SRC;
            else if ((s >> 24) & 0xFu) e = PSL_MOD;
            else if (((s >> 16) & 0xFFu) != 0xE4u) e = PSL_SWZ;
            else if (ty == 2 && ++nc > 1) e = PSL_TWOC;
        }
        if (e) break;
        i += 2 + ns;
        ++ins;
    }
    if (at) *at = e ? ins : -1;
    if (why) *why = kPsLintWhy[e];
    return e;
}

// The grade's pixel check (post.inc PpGradeCheck): four test colours drawn through the grading shader into a target of
// ours cleared to PP_PX_DST, read back, compared with PpGradePixel. PPX_SAME = the input came through (the v3.43 failure).
#define PP_PX_N   4
#define PP_PX_TOL 3                // 8-bit levels the card may round differently
#define PP_PX_DST 0xFFFF00FFu      // magenta: "nothing drawn" keeps it
static const unsigned char kPpPxIn[PP_PX_N][3] = { { 38, 38, 38 }, { 128, 128, 128 }, { 217, 217, 217 }, { 153, 77, 51 } };
enum { PPX_OK, PPX_SAME, PPX_NOTHING, PPX_OFF, PPX_N };
static const char *const kPpPxWhy[PPX_N] = { "the grade reaches the pixels",
                                             "the input came through unchanged: the grading shader did not reach the pixels",
                                             "nothing was drawn", "the pixels are not the shader's (its sim)" };
// v3.47 PX347: the sim on given source colours (the source as read back; PpPxExpect: the colours asked)
static void PpPxExpectFrom(const PpGradeLook *g, const unsigned char src[PP_PX_N][3], unsigned char ex[PP_PX_N][3])
{
    int k, i;
    for (k = 0; k < PP_PX_N; ++k) {
        float in[3], out[3];
        for (i = 0; i < 3; ++i) in[i] = (float)src[k][i] / 255.0f;
        PpGradePixel(g, in, out);
        for (i = 0; i < 3; ++i) ex[k][i] = (unsigned char)(out[i] * 255.0f + 0.5f);
    }
}
static void PpPxExpect(const PpGradeLook *g, unsigned char ex[PP_PX_N][3]) { PpPxExpectFrom(g, kPpPxIn, ex); }
// out = what was read back; *maxOff the worst |out - sim| (levels), *chg / *exChg the mean |out - in| / |sim - in|
// v3.47 PX347: in = the source the sim ran on (SAME = out is that input unchanged); PpPxVerdict: the colours asked
static int PpPxVerdictIn(const unsigned char out[PP_PX_N][3], const unsigned char ex[PP_PX_N][3], const unsigned char in[PP_PX_N][3],
                         int *maxOff, float *chg, float *exChg)
{
    int k, i, off = 0, same = 1, none = 1;
    float c = 0.0f, x = 0.0f;
    for (k = 0; k < PP_PX_N; ++k)
        for (i = 0; i < 3; ++i) {
            const int o = out[k][i], d = abs(o - (int)ex[k][i]), s = abs(o - (int)in[k][i]);
            const int dst = (int)((PP_PX_DST >> (16 - 8 * i)) & 0xFFu);
            if (d > off) off = d;
            if (s > 1) same = 0;
            if (o != dst) none = 0;
            c += (float)s, x += (float)abs((int)ex[k][i] - (int)in[k][i]);
        }
    if (maxOff) *maxOff = off;
    if (chg) *chg = c / (float)(3 * PP_PX_N);
    if (exChg) *exChg = x / (float)(3 * PP_PX_N);
    return off <= PP_PX_TOL ? PPX_OK : none ? PPX_NOTHING : same ? PPX_SAME : PPX_OFF;
}
static int PpPxVerdict(const unsigned char out[PP_PX_N][3], const unsigned char ex[PP_PX_N][3], int *maxOff, float *chg, float *exChg)
{
    return PpPxVerdictIn(out, ex, kPpPxIn, maxOff, chg, exChg);
}

// ---------------------------------------------------------------- v3.47 PX347: the pixel checks' test pattern
// USER_FEEDBACK_v346: with the first read back that worked (dgVoodoo2 2.87.6 Direct3D 11) every v3.46 pixel check
// FAILED, and every value read is the shader's own result on a half / half mix of the cell asked and the cell on its
// left (black left of the first cell): the grade within 1 level on all 12 cells logged (4 presets x 3), the shadow
// composite within 0.002 alpha on all 8 (2 Darkness values x 4; its third source cell read as empty). So the test draw
// sampled half a texel left of the texel centres there (the half pixel of KB 194's 2D canvases), and v3.44's cells, 2
// texels wide and read at their first column, straddled two cells: the check was wrong, not the shaders
// (staging/v347_feedback/w5_pxcheck/PLAN.md). Now: cells PX_CW texels wide, read at their centre (an offset of less than
// PX_MID - 1 texels still reads the cell itself); the source is read back too and the sim also runs on what it held; the
// cells' first and last columns measure the offset (logged: the backend's rule for every full-screen pass).
#define PX_CELLS 4
#define PX_CW    8                 // a cell's width (texels)
#define PX_W     (PX_CW * PX_CELLS)
#define PX_H     4
#define PX_ROW   (PX_H / 2)        // the row read
#define PX_MID   (PX_CW / 2)       // the column read inside a cell
enum { PXC_MID, PXC_FIRST, PXC_LAST, PXC_TOP, PXC_N };  // the places read in each cell of the destination
static int PxCol(int k, int at) { return PX_CW * k + (at == PXC_FIRST ? 0 : at == PXC_LAST ? PX_CW - 1 : PX_MID); }
static int PxRow(int at) { return at == PXC_TOP ? 0 : PX_ROW; }
typedef struct {
    unsigned long dst[PX_CELLS][PXC_N];  // the destination after the draw (A8R8G8B8) at each cell's places
    unsigned long src[PX_CELLS];         // the source at each cell's centre (srcOk: it could be read back)
    int srcOk;
} PxProbe;
static unsigned PxByte(unsigned long v, int shift) { return (unsigned)(v >> shift) & 0xFFu; }
// 1 = the source cell held the colour asked: every byte of mask within 1 level
static int PxSrcHeld(unsigned long asked, unsigned long got, unsigned long mask)
{
    int s;
    for (s = 0; s < 32; s += 8)
        if (((mask >> s) & 0xFFu) && abs((int)PxByte(asked, s) - (int)PxByte(got, s)) > 1) return 0;
    return 1;
}
// The test draw's sampling offset in texels (negative: left of the texel centres), from one byte (shift) of the
// destination: a cell's first column holds share sL of its left neighbour, its last column share sR of its right one
// (bilinear; on the shader's output, so about): offset = mean sR - mean sL over the neighbours at least PX_EDGE_MIN
// levels apart. *n = the edges used (0: not measurable, 0 returned).
#define PX_EDGE_MIN 16
static float PxEdgeOffset(const PxProbe *p, int shift, int *n)
{
    float sl = 0.0f, sr = 0.0f;
    int k, nl = 0, nr = 0;
    for (k = 0; k < PX_CELLS; ++k) {
        const float m = (float)PxByte(p->dst[k][PXC_MID], shift);
        if (k > 0) {
            const float d = m - (float)PxByte(p->dst[k - 1][PXC_MID], shift);
            if (fabsf(d) >= (float)PX_EDGE_MIN) {
                float s = (m - (float)PxByte(p->dst[k][PXC_FIRST], shift)) / d;
                sl += s < 0.0f ? 0.0f : s > 1.0f ? 1.0f : s, ++nl;
            }
        }
        if (k + 1 < PX_CELLS) {
            const float d = m - (float)PxByte(p->dst[k + 1][PXC_MID], shift);
            if (fabsf(d) >= (float)PX_EDGE_MIN) {
                float s = (m - (float)PxByte(p->dst[k][PXC_LAST], shift)) / d;
                sr += s < 0.0f ? 0.0f : s > 1.0f ? 1.0f : s, ++nr;
            }
        }
    }
    if (n) *n = nl + nr;
    return (nr ? sr / (float)nr : 0.0f) - (nl ? sl / (float)nl : 0.0f);
}

// ---------------------------------------------------------------- v3.45 RB345: the GPU -> CPU read back's path
// USER_FEEDBACK_v344 Q8: through GOG's d3d8 -> dgVoodoo2 2.87.6 Direct3D 11, CopyRects from a render target of ours into
// a system-memory image surface (GOG's translator: GetRenderTargetData) fails: every v3.44 pixel check and depth probe
// read nothing. The v3.43 log of 2026-11-22 21:46 (the same backend) shows screenfx.inc FxGrabClean's route 2 working
// there twice: CopyRects into a lockable render target (GOG: StretchRect), then LockRect (2560x1440, read back 9.7 /
// 11.3 ms). Under GOG's own wrapper the image surface works (RE_capture, KB 116) and the lockable target is its MSAA
// fallback. So the lockable target is tried first, the image surface next; the path that worked is tried first from
// then on; a path that failed RB_TRIES times and never worked is given up; every path given up = no read back on this
// backend for the session: one log line (RBE_OFF), the callers skip the work and stay quiet.
enum { RBP_LOCKRT, RBP_IMAGE, RBP_N };               // in the order tried
enum { RBS_OK, RBS_MAKE, RBS_COPY, RBS_LOCK, RBS_N };  // the step a try failed at
enum { RBE_NONE, RBE_FIRST, RBE_SWITCH, RBE_LOST, RBE_OFF };  // what a try's result asks to log (FIRST / OFF once a session)
#define RB_TRIES 2                                   // failures (none ever worked) before a path is given up
static const char *const kRbPathName[RBP_N] = { "a lockable render target", "a system-memory image surface" };
static const char *const kRbPathShort[RBP_N] = { "lockable render target", "image surface" };
static const char *const kRbStepName[RBS_N] = { "worked", "the surface could not be made", "the copy (CopyRects) failed",
                                                "the lock (LockRect) failed" };
typedef struct {
    unsigned ok[RBP_N], fail[RBP_N];
    int step[RBP_N];        // the last failure's step
    unsigned long hr[RBP_N];  // and its HRESULT
    int win;                // the path that worked last (-1: none yet)
    int off;                // every path given up: no read back this session
} RbState;
static void RbInit(RbState *s)
{
    memset(s, 0, sizeof *s);
    s->win = -1;
}
static int RbGivenUp(const RbState *s, int p) { return p >= 0 && p < RBP_N && !s->ok[p] && s->fail[p] >= RB_TRIES; }
// the paths to try now (the last that worked first, then the others not given up, in RBP order); 0 = none (off)
static int RbOrder(const RbState *s, int order[RBP_N])
{
    int n = 0, p;
    if (s->off) return 0;
    if (s->win >= 0 && s->win < RBP_N) order[n++] = s->win;
    for (p = 0; p < RBP_N; ++p)
        if (p != s->win && !RbGivenUp(s, p)) order[n++] = p;
    return n;
}
// one try's result (step RBS_OK = it worked); returns what to log
static int RbNote(RbState *s, int p, int step, unsigned long hr)
{
    int q, all = 1;
    if (p < 0 || p >= RBP_N || s->off) return RBE_NONE;
    if (step == RBS_OK) {
        const int was = s->win;
        ++s->ok[p];
        s->win = p;
        return was < 0 ? RBE_FIRST : was != p ? RBE_SWITCH : RBE_NONE;
    }
    ++s->fail[p];
    s->step[p] = step > RBS_OK && step < RBS_N ? step : RBS_COPY;
    s->hr[p] = hr;
    if (s->ok[p]) return RBE_LOST;  // a path that worked before failed this time (kept: tried again next time)
    for (q = 0; q < RBP_N; ++q)
        if (!RbGivenUp(s, q)) all = 0;
    if (!all) return RBE_NONE;
    s->off = 1;
    return RBE_OFF;
}
#endif
