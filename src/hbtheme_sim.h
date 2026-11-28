// ================================================================ v3.59 FT359: the enemy bars' looks under the special filters
// Plan: staging/v359_feedback/w4_themes/PLAN.md (the user, 2026-11-27: "remake the health/revive bar's themes to fit both
// visions just like you did with Item Shine", "Include both Night Visions", "For Thermal Vision remove the victim's health
// bars"). The pure part: the palettes, the heat scale, the strength mix, the hide rule. hudbars.inc draws with it
// (HbThemeFrame / HbThemeFill / HbThemeOver); test\hbtheme_sim_test.c (build_hbt_test.cmd) checks every look at every
// Filter Strength: the fill reads against the empty part, the heat scale rises, the stock look is v3.58's exactly.
//
// Item Shine is the model: it is drawn before the filter, so the filter itself paints it. The bars are drawn after the
// filter (the HUD stays clean), so each special filter gets its own palette here, mixed from the stock palette by the
// Filter Strength (Low 35 %, Medium 65 %, High / Ultra all of it; the overlays follow the strength, Ultra x 1.35 for the
// two Night Visions, as their filters):
//   Thermal Vision: a cold navy empty part (the world's cold ramp), the fill a heat scale along the bar (red at the empty
//                   end, orange, yellow, white-hot at full: a monster that loses health cools), the lost part cold azure
//                   (its heat gone), white when it lands; the victims (Thermal's hidden ghosts) get no bar at all.
//   Spirit Vision:  an ink-blue empty part outlined in the edge world's blue-grey line, the fill a pale flame with tongues
//                   that flicker up from its top: monsters a pale soul-fire gold, the victims spirit blue, the bosses
//                   Walter's red fire; the revive bar spirit violet, the seal pale glowing silver.
//   Night Vision:   phosphor green (the victims paler, nearer white), a soft phosphor bloom round the fill, the amplifier's
//                   scan lines and grain.
//   Night Mono:     the same in greys, lighter scan lines and grain, no bloom.
#ifndef HBTHEME_SIM_H
#define HBTHEME_SIM_H

enum { HBT_STOCK, HBT_THERMAL, HBT_SPIRIT, HBT_NV, HBT_MONO, HBT_N };
static const char *const kHbtName[HBT_N] = { "the stock look", "Thermal Vision", "Spirit Vision", "Night Vision", "Night Mono" };

// 0xRRGGBB colours of one look; fill / rev / steel: [kind][top, bottom], kind 0 monster, 1 ghost, 2 boss
typedef struct {
    unsigned rim, back0, back1, chipHot, trail, sheen, guard, lock;
    unsigned fill[3][2], rev[2], steel[2];
} HbtPal;

// the stock palette: v3.58's hudbars.inc constants exactly (HB_RIM .. kHbSteel)
#define HBT_STOCK_PAL                                                                          \
    { 0x000000u, 0x17110Fu, 0x090706u, 0xFFF0D8u, 0xC2B094u, 0xFFFFFFu, 0xDDF4F0u, 0xE2D8C8u,  \
      { { 0xB0362Cu, 0x5C0E0Bu }, { 0x9CC0C0u, 0x3A5A60u }, { 0xC0441Eu, 0x5A0C06u } },        \
      { 0xD2C4FFu, 0x6A54B0u }, { 0xB4BCC8u, 0x4C5460u } }

static const HbtPal kHbtPal[HBT_N] = {
    HBT_STOCK_PAL,
    // Thermal Vision: cold navy back, cold azure lost part, white-hot chip; the fill is HbtHeat's scale (these fills are
    // its fallback colours only); Walter's revive countdown cold azure (his down timer: the heat coming back)
    { 0x000000u, 0x0E0C3Au, 0x05041Eu, 0xFFFFFFu, 0x5CC8FFu, 0xFFFFFFu, 0xBFEFFFu, 0xFFF2C8u,
      { { 0xFF9A28u, 0xB02008u }, { 0xFF9A28u, 0xB02008u }, { 0xFFB040u, 0xC01808u } },
      { 0xA8ECFFu, 0x2858C8u }, { 0xC4D2E2u, 0x4A5A78u } },
    // Spirit Vision: the rim is the edge world's line colour (GV_LINE 0.30 / 0.50 / 0.62, a little lighter), ink-blue back
    { 0x5A7E98u, 0x0C1622u, 0x04080Eu, 0xFFFFFFu, 0x86AEE0u, 0xFFFFFFu, 0xF2FAFFu, 0xD4E6FFu,
      { { 0xFFE8C0u, 0xC0782Eu }, { 0xEEF6FFu, 0x5C88D0u }, { 0xFFCCA0u, 0xB8301Cu } },
      { 0xCCD8FFu, 0x4C5CC4u }, { 0xE4EEFFu, 0x6C7E9Cu } },
    // Night Vision: phosphor green (NV_TINT 0.30 / 1.00 / 0.40)
    { 0x000000u, 0x07200Cu, 0x020C04u, 0xF0FFE8u, 0x7CC070u, 0xFFFFFFu, 0xE6FFE6u, 0xD8FFC8u,
      { { 0xA8FF84u, 0x2A8A2Eu }, { 0xDCFFE6u, 0x5AA27Au }, { 0x90F070u, 0x1C701Cu } },
      { 0xBCFFE0u, 0x2E8A62u }, { 0xCCDCCCu, 0x566656u } },
    // Night Mono: greys
    { 0x000000u, 0x181818u, 0x080808u, 0xFFFFFFu, 0xA0A0A0u, 0xFFFFFFu, 0xF0F0F0u, 0xE4E4E4u,
      { { 0xE6E6E6u, 0x6A6A6Au }, { 0xFFFFFFu, 0x9C9C9Cu }, { 0xD4D4D4u, 0x545454u } },
      { 0xDCDCDCu, 0x5E5E5Eu }, { 0xBEBEBEu, 0x4E4E4Eu } },
};

typedef struct {
    HbtPal c;        // this frame's colours: the stock palette mixed toward the look by k
    int look;        // HBT_*
    float k;         // the colours' share of the look (0 = stock .. 1)
    float over;      // the overlays' strength (0 = none; Ultra 1.35 for the Night Visions)
    int heat;        // the fill is the heat scale (Thermal Vision)
    int flames;      // flame tongues over the fill (Spirit Vision)
    int scan;        // scan lines over the bar (the Night Visions)
    int grain;       // amplifier grain specks (the Night Visions)
    int halo;        // a phosphor bloom round the fill (Night Vision)
    float scanA, grainA, haloA, flameA;  // the overlays' alpha at over 1
    unsigned haloRgb, grainRgb;
} HbTheme;

static float HbtClamp(float v, float lo, float hi) { return v > lo ? (v < hi ? v : hi) : lo; }  // (NaN -> lo)
static unsigned HbtLerp(unsigned a, unsigned b, float t)  // 0xRRGGBB, as hudbars.inc HbLerpRgb
{
    unsigned r = 0;
    int sh;
    t = HbtClamp(t, 0.0f, 1.0f);
    for (sh = 0; sh <= 16; sh += 8) {
        const float ca = (float)((a >> sh) & 0xFF), cb = (float)((b >> sh) & 0xFF);
        r |= (unsigned)(ca + (cb - ca) * t + 0.5f) << sh;
    }
    return r;
}
static unsigned HbtScale(unsigned c, float k)  // each channel x k (0..1)
{
    return HbtLerp(0x000000u, c, k);
}
// the brightness used for the readability rule (Rec. 709 weights on the 0..1 values)
static float HbtLuma(unsigned c)
{
    return (0.2126f * (float)((c >> 16) & 0xFF) + 0.7152f * (float)((c >> 8) & 0xFF) + 0.0722f * (float)(c & 0xFF)) / 255.0f;
}
// Thermal Vision's heat scale, u = 0 (the bar's empty end) .. 1 (full): red, orange, yellow, white-hot (the body heat
// shader's order, ThHeat: dark red - red - orange - yellow - white; its dark red end left out so a low bar still reads)
#define HBT_HEAT_N 5
static const float kHbtHeatU[HBT_HEAT_N] = { 0.0f, 0.30f, 0.55f, 0.80f, 1.0f };
static const unsigned kHbtHeatC[HBT_HEAT_N] = { 0xC8241Cu, 0xE8401Au, 0xFF7A1Cu, 0xFFD040u, 0xFFFFE6u };
static unsigned HbtHeat(float u)
{
    int i;
    u = HbtClamp(u, 0.0f, 1.0f);
    for (i = 1; i < HBT_HEAT_N - 1 && u > kHbtHeatU[i]; ++i) {}
    return HbtLerp(kHbtHeatC[i - 1], kHbtHeatC[i], (u - kHbtHeatU[i - 1]) / (kHbtHeatU[i] - kHbtHeatU[i - 1]));
}
#define HBT_HEAT_BOTTOM 0.62f  // the heat scale's bottom row: the top colour x this (the stock fill's top-to-bottom shade)

// The look's share by Filter Strength: fsK = preview_sim.h kPvFsK of the strength (0.35, 0.65, 1.0, 1.35). Colours take
// at most all of the look; the overlays follow the strength, Ultra above 1 only for the Night Visions (Thermal and Spirit
// Vision are High at Ultra: PvFsLookApply, the user's v3.58 rule).
static void HbtStrength(int look, float fsK, float *k, float *over)
{
    if (!(fsK > 0.0f)) fsK = 0.0f;  // (NaN -> 0)
    *k = HbtClamp(fsK, 0.0f, 1.0f);
    *over = (look == HBT_NV || look == HBT_MONO) ? HbtClamp(fsK, 0.0f, 1.35f) : *k;
}

static void HbtBuild(int look, float fsK, HbTheme *t)
{
    static const HbtPal kStock = HBT_STOCK_PAL;
    const HbtPal *s = &kStock, *p;
    int i, j;
    if (look <= HBT_STOCK || look >= HBT_N) look = HBT_STOCK;
    p = &kHbtPal[look];
    t->look = look;
    if (look == HBT_STOCK) t->k = t->over = 0.0f;
    else HbtStrength(look, fsK, &t->k, &t->over);
#define HBT_MIX(f) t->c.f = HbtLerp(s->f, p->f, t->k)
    HBT_MIX(rim); HBT_MIX(back0); HBT_MIX(back1); HBT_MIX(chipHot); HBT_MIX(trail); HBT_MIX(sheen); HBT_MIX(guard); HBT_MIX(lock);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 2; ++j) HBT_MIX(fill[i][j]);
    for (j = 0; j < 2; ++j) HBT_MIX(rev[j]), HBT_MIX(steel[j]);
#undef HBT_MIX
    t->heat = look == HBT_THERMAL && t->k > 0.0f;
    t->flames = look == HBT_SPIRIT && t->over > 0.0f;
    t->scan = (look == HBT_NV || look == HBT_MONO) && t->over > 0.0f;
    t->grain = t->scan;
    t->halo = look == HBT_NV && t->over > 0.0f;
    t->scanA = look == HBT_MONO ? 0.16f : 0.22f;
    t->grainA = look == HBT_MONO ? 0.25f : 0.35f;
    t->haloA = 0.10f;
    t->flameA = 0.55f;
    t->haloRgb = 0x50FF50u;
    t->grainRgb = look == HBT_MONO ? 0xFFFFFFu : 0xC8FFC0u;
}
// The fill's colours at u (0..1 along the full bar) for a kind (0 monster, 1 ghost, 2 boss): the look's fill, or under
// Thermal Vision the heat scale mixed from the stock fill by k. *top / *bot: 0xRRGGBB.
static void HbtFillAt(const HbTheme *t, int kind, float u, unsigned *top, unsigned *bot)
{
    static const HbtPal kStock = HBT_STOCK_PAL;
    if (kind < 0 || kind > 2) kind = 0;
    if (!t->heat) {
        *top = t->c.fill[kind][0], *bot = t->c.fill[kind][1];
        return;
    }
    {
        const unsigned h = HbtHeat(u);
        *top = HbtLerp(kStock.fill[kind][0], h, t->k);
        *bot = HbtLerp(kStock.fill[kind][1], HbtScale(h, HBT_HEAT_BOTTOM), t->k);
    }
}
// v3.59 FT359 (the user: "For Thermal Vision remove the victim's health bars"): 1 = no bar for this enemy. on = the
// filter themes are on ([ScreenEffects] FilterThemes); thermalLive = Thermal Vision hides the ghosts this frame
// (screenfx.inc ThermalLive); cls = its Thermal class (ThermClassify: 2 = hidden, a ghost or a victim: no heat).
static int HbtHides(int on, int thermalLive, int cls) { return on && thermalLive && cls == 2; }

// Ghost Teleport's fade under Spirit Vision (spiritfire.inc): her soul fire turns spirit blue (After Shock's kAiColSv
// 0.55 / 0.75 / 1.0) as she fades, fully by 60 % of the fade (gk = the fire's alpha factor, 1 = not fading)
static void HbtSpiritTint(float rgb[3], float gk)
{
    static const float kBlue[3] = { 0.55f, 0.75f, 1.0f };
    const float t = HbtClamp((1.0f - gk) / 0.6f, 0.0f, 1.0f);
    int i;
    for (i = 0; i < 3; ++i) rgb[i] += (kBlue[i] - rgb[i]) * t;
}

#endif
