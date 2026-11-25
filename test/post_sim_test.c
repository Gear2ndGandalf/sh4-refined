// v3.43 PP343: native test of src\post_sim.h (test\build_post_test.cmd): the row / ini values, the pause-copy key, which
// filters each effect runs under, the bloom chain's plan for 480 .. 2160 lines and 4:3 .. 32:9 (sizes, order, no draw
// stretched more than 2x, the targets' memory), the bright pass, the grading presets (constants inside ps.1.1's -1..1, the
// looks' directions: PS2 greener with lifted blacks, Otherworld cold and drained, Apartment warm, Bleach harder; no
// value leaves 0..1), the lens' strength from the haunting sums and its fades, the warp (centre fixed, samples inside
// the frame, red further in than blue, Off = identity). v3.44 CS344: Bloom Very High (1.6x High), the grade's proven-forms
// shader (PsLint, the same formula as v3.43's), the pixel check's verdicts.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/post_sim.h"
#include "px_model.h"  // v3.47 PX347

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestParse(void)
{
    CHECK(PpParse(PPE_BLOOM, "Off") == PPB_OFF && PpParse(PPE_BLOOM, "low") == PPB_LOW && PpParse(PPE_BLOOM, " HIGH ") == PPB_HIGH,
          "bloom names, any case, blanks");
    CHECK(PpParse(PPE_GRADE, "PS2") == PPG_PS2 && PpParse(PPE_GRADE, "otherworld") == PPG_OTHER &&
          PpParse(PPE_GRADE, "Apartment") == PPG_APT && PpParse(PPE_GRADE, "bleach") == PPG_BLEACH, "grading names");
    CHECK(PpParse(PPE_LENS, "On") == PPL_ON && PpParse(PPE_LENS, "off") == PPL_OFF, "lens names");
    CHECK(PpParse(PPE_BLOOM, "2") == PPB_HIGH && PpParse(PPE_GRADE, "4") == PPG_BLEACH && PpParse(PPE_LENS, "1") == PPL_ON,
          "digits");
    CHECK(PpParse(PPE_BLOOM, "Very High") == PPB_VHIGH && PpParse(PPE_BLOOM, "veryhigh") == PPB_VHIGH &&
          PpParse(PPE_BLOOM, " VERY  HIGH ") == PPB_VHIGH && PpParse(PPE_BLOOM, "3") == PPB_VHIGH && PpParse(PPE_BLOOM, "Very") == -1,
          "v3.44: Bloom Very High (blanks inside ignored)");
    CHECK(PpParse(PPE_BLOOM, "4") == -1 && PpParse(PPE_LENS, "2") == -1 && PpParse(PPE_GRADE, "") == -1 &&
          PpParse(PPE_GRADE, NULL) == -1 && PpParse(PPE_GRADE, "PS") == -1 && PpParse(PPE_GRADE, "PS22") == -1 &&
          PpParse(PPE_BLOOM, "Medium") == -1 && PpParse(3, "Off") == -1 && PpParse(-1, "Off") == -1, "not values");
    CHECK(kPpCount[PPE_BLOOM] == 4 && kPpCount[PPE_GRADE] == 5 && kPpCount[PPE_LENS] == 2 && PP_VMAX >= 5, "the value counts");
    {
        int k, v;
        for (k = 0; k < PPE_N; ++k)
            for (v = 0; v < kPpCount[k]; ++v) {
                CHECK(kPpName[k][v] && strlen(kPpName[k][v]) <= 12, "a short value name %d %d", k, v);
                CHECK(PpParse(k, kPpName[k][v]) == v, "each name reads back %d %d", k, v);
            }
        {   // the pause-copy keys are distinct
            int seen[64], b, g, l, dup = 0;
            memset(seen, 0, sizeof seen);
            for (b = 0; b < PPB_N; ++b)
                for (g = 0; g < PPG_N; ++g)
                    for (l = 0; l < PPL_N; ++l) {
                        const int key = PpKey(b, g, l);
                        if (key < 0 || key >= 64 || seen[key]++) dup = 1;
                    }
            CHECK(!dup, "every combination its own pause key");
        }
        CHECK(PpKey(0, 0, 0) == 0 && PpKey(2, 4, 1) > 0 && PpKey(3, 0, 0) != PpKey(0, 0, 0), "all Off = key 0");
    }
}

static void TestFilters(void)
{
    enum { NONE, CRT, VIVID, SEPIA, OLDF, NV, NVM, THERMAL, SPIRIT, VIG };
    CHECK(PpWith(kPpGradeWith, NONE) && PpWith(kPpGradeWith, CRT) && PpWith(kPpGradeWith, VIVID) && PpWith(kPpGradeWith, VIG),
          "grading with the filters that keep the colours");
    CHECK(!PpWith(kPpGradeWith, SEPIA) && !PpWith(kPpGradeWith, OLDF) && !PpWith(kPpGradeWith, NV) && !PpWith(kPpGradeWith, NVM) &&
          !PpWith(kPpGradeWith, THERMAL) && !PpWith(kPpGradeWith, SPIRIT), "no grading under the colour filters");
    CHECK(PpWith(kPpBloomWith, NONE) && PpWith(kPpBloomWith, SEPIA) && PpWith(kPpBloomWith, OLDF) && !PpWith(kPpBloomWith, NV) &&
          !PpWith(kPpBloomWith, NVM) && !PpWith(kPpBloomWith, THERMAL) && !PpWith(kPpBloomWith, SPIRIT), "bloom's filters");
    CHECK(PpWith(kPpLensWith, THERMAL) && PpWith(kPpLensWith, NV) && !PpWith(kPpLensWith, SPIRIT) && PpWith(kPpLensWith, NONE),
          "the lens' filters");
    CHECK(!PpWith(kPpLensWith, -1) && !PpWith(kPpLensWith, PP_FILTERS), "out of range: not drawn");
}

static void TestPlan(void)
{
    static const int kH[] = { 480, 600, 720, 768, 900, 1080, 1200, 1440, 1600, 2160 };
    static const float kA[] = { 4.0f / 3.0f, 16.0f / 10.0f, 16.0f / 9.0f, 21.0f / 9.0f, 32.0f / 9.0f };
    PpPlan p;
    int i, j, k;
    CHECK(!PpMakePlan(32, 32, &p) && !PpMakePlan(0, 720, &p) && !PpMakePlan(20000, 720, &p), "no plan for a silly size");
    for (i = 0; i < (int)(sizeof kH / sizeof kH[0]); ++i)
        for (j = 0; j < (int)(sizeof kA / sizeof kA[0]); ++j) {
            const int h = kH[i], w = (int)(h * kA[j] + 0.5f);
            int bright = 0, comp = 0, blur = 0, up = 0, down = 0;
            CHECK(PpMakePlan(w, h, &p), "plan %dx%d", w, h);
            CHECK(p.levels == PP_LEVELS && p.nsteps == 1 + (PP_LEVELS - 1) + 2 + (PP_LEVELS - 1) + 1, "draw count %dx%d: %d", w, h, p.nsteps);
            for (k = PPR_L1; k <= PPR_L4; ++k) {
                CHECK(p.w[k] >= PP_RT_MIN && p.h[k] >= PP_RT_MIN, "level %d at least the minimum %dx%d", k, w, h);
                CHECK(p.w[k] * 2 >= p.w[k - 1] && p.w[k] * 2 <= p.w[k - 1] + 1 && p.h[k] * 2 >= p.h[k - 1] && p.h[k] * 2 <= p.h[k - 1] + 1,
                      "each level half the one above (%dx%d level %d: %dx%d of %dx%d)", w, h, k, p.w[k], p.h[k], p.w[k - 1], p.h[k - 1]);
            }
            CHECK(p.w[PPR_X] == p.w[PPR_L4] && p.h[PPR_X] == p.h[PPR_L4], "the blur's second target at the smallest size");
            for (k = 0; k < p.nsteps; ++k) {
                const PpStep *s = &p.step[k];
                const float sx = (float)s->dw / (float)p.w[s->src], sy = (float)s->dh / (float)p.h[s->src];
                CHECK(s->dw == p.w[s->dst] && s->dh == p.h[s->dst], "step %d draws its whole target", k);
                CHECK(sx <= 2.01f && sy <= 2.01f && sx >= 0.49f && sy >= 0.49f, "step %d at most a 2x stretch (%.2f x %.2f) at %dx%d", k, sx, sy, w, h);
                CHECK(s->du > 0.0f && s->dv > 0.0f && s->du < 0.5f && s->dv < 0.5f, "step %d tap offsets", k);
                CHECK(s->src != s->dst, "step %d does not read its own target", k);
                if (s->kind == PPS_BRIGHT) {
                    ++bright;
                    CHECK(k == 0 && s->src == PPR_FRAME && s->dst == PPR_L1, "the bright pass first");
                }
                if (s->kind == PPS_DOWN) {
                    ++down;
                    CHECK(s->dst == s->src + 1, "down one level");
                }
                if (s->kind == PPS_BLUR) {
                    ++blur;
                    CHECK(s->dw == p.w[PPR_L4], "blur at the smallest level");
                }
                if (s->kind == PPS_UP) {
                    ++up;
                    CHECK(s->dst == s->src - 1, "up one level");
                }
                if (s->kind == PPS_COMP) {
                    ++comp;
                    CHECK(k == p.nsteps - 1 && s->src == PPR_L1 && s->dst == PPR_BACK, "the composite last, from the half size");
                }
            }
            CHECK(bright == 1 && down == PP_LEVELS - 1 && blur == 2 && up == PP_LEVELS - 1 && comp == 1, "the steps' kinds %dx%d", w, h);
            CHECK(p.step[1 + (PP_LEVELS - 1)].dst == PPR_X && p.step[2 + (PP_LEVELS - 1)].dst == PPR_L4, "the blur goes there and back");
            CHECK(PpPlanBytes(&p) <= (unsigned)w * (unsigned)h * 4u * 36u / 100u, "the targets cost at most 36 %% of one frame (%u)", PpPlanBytes(&p));
            if (h == 1440 && j == 2) printf("  2560x1440: half %dx%d .. 1/16 %dx%d, %.1f MB\n", p.w[PPR_L1], p.h[PPR_L1], p.w[PPR_L4],
                                            p.h[PPR_L4], PpPlanBytes(&p) / 1048576.0f);
        }
}

static void TestBright(void)
{
    int b;
    for (b = PPB_LOW; b < PPB_N; ++b) {
        const PpBloomLook *L = &kPpBloom[b];
        CHECK(PpBright(L, 0.0f) == 0.0f && PpBright(L, L->th) == 0.0f && PpBright(L, L->th - 0.1f) == 0.0f, "dark and mid tones never bloom (%d)", b);
        CHECK(PpBright(L, 1.0f) > 0.9f, "white blooms fully (%d: %.2f)", b, PpBright(L, 1.0f));
        CHECK(PpBright(L, 0.9f) >= PpBright(L, 0.8f) && PpBright(L, 0.8f) >= PpBright(L, 0.7f) && PpBright(L, 0.7f) > PpBright(L, 0.6f),
              "rises with brightness (%d; v3.44: Very High saturates from 0.70)", b);
        CHECK(L->th > 0.4f && L->th < 0.8f && L->gain * 0.25f <= 1.0f && L->k > 0.0f && L->k <= 1.0f && L->up > 0.0f && L->up < 1.0f,
              "constants inside ps.1.1's -1..1 (%d)", b);
        CHECK(PpScreen(0.5f, 0.5f) == 0.75f && PpScreen(1.0f, 1.0f) == 1.0f && PpScreen(0.2f, 0.0f) == 0.2f, "screen blend");
    }
    CHECK(kPpBloom[PPB_HIGH].th < kPpBloom[PPB_LOW].th && kPpBloom[PPB_HIGH].k >= kPpBloom[PPB_LOW].k &&
          kPpBloom[PPB_HIGH].up >= kPpBloom[PPB_LOW].up, "High: more light, more glow, wider");
    {   // v3.44 CS344: Very High about 1.6x High (USER_FEEDBACK_v343 item 5)
        const float lo = PpBloomEnergy(&kPpBloom[PPB_LOW]), hi = PpBloomEnergy(&kPpBloom[PPB_HIGH]), vh = PpBloomEnergy(&kPpBloom[PPB_VHIGH]);
        CHECK(lo < hi && vh / hi >= 1.45f && vh / hi <= 1.75f, "Very High's glow %.3f = %.2fx High's %.3f (Low %.3f)", vh, vh / hi, hi, lo);
        CHECK(kPpBloom[PPB_VHIGH].th < kPpBloom[PPB_HIGH].th && kPpBloom[PPB_VHIGH].up >= kPpBloom[PPB_HIGH].up &&
              kPpBloom[PPB_VHIGH].k >= kPpBloom[PPB_HIGH].k, "Very High: more light, wider");
        CHECK(PpBright(&kPpBloom[PPB_VHIGH], 0.35f) == 0.0f, "Very High: the dark third of the picture never blooms");
        printf("bloom (v3.44): glow Low %.3f, High %.3f, Very High %.3f (%.2fx High)\n", lo, hi, vh, vh / hi);
    }
}

static float Luma(const float c[3]) { return PP_LUMA_R * c[0] + PP_LUMA_G * c[1] + PP_LUMA_B * c[2]; }
static float Chroma(const float c[3])
{
    float mx = c[0], mn = c[0];
    int i;
    for (i = 1; i < 3; ++i) mx = c[i] > mx ? c[i] : mx, mn = c[i] < mn ? c[i] : mn;
    return mx - mn;
}
static void TestGrade(void)
{
    float c[6][4], in[3], out[3];
    int g, r, gg, b;
    for (g = 0; g < PPG_N; ++g) CHECK(PpGradeConsts(&kPpGrade[g], c), "preset %d: every constant inside -1..1", g);
    for (g = 0; g < PPG_N; ++g)  // a 9^3 cube: inside 0..1, finite
        for (r = 0; r <= 8; ++r)
            for (gg = 0; gg <= 8; ++gg)
                for (b = 0; b <= 8; ++b) {
                    in[0] = r / 8.0f, in[1] = gg / 8.0f, in[2] = b / 8.0f;
                    PpGradePixel(&kPpGrade[g], in, out);
                    CHECK(out[0] >= 0.0f && out[0] <= 1.0f && out[1] >= 0.0f && out[1] <= 1.0f && out[2] >= 0.0f && out[2] <= 1.0f,
                          "preset %d in range", g);
                }
    for (r = 0; r <= 8; ++r) {  // Off is the identity
        in[0] = r / 8.0f, in[1] = 1.0f - r / 8.0f, in[2] = 0.3f;
        PpGradePixel(&kPpGrade[PPG_OFF], in, out);
        CHECK(fabsf(out[0] - in[0]) < 1e-5f && fabsf(out[1] - in[1]) < 1e-5f && fabsf(out[2] - in[2]) < 1e-5f, "Off = identity");
    }
    {   // the looks' directions on a neutral grey, black and a colour
        const float grey[3] = { 0.4f, 0.4f, 0.4f }, black[3] = { 0, 0, 0 }, red[3] = { 0.8f, 0.2f, 0.15f };
        float o[3], o2[3];
        PpGradePixel(&kPpGrade[PPG_PS2], grey, o);
        CHECK(o[1] > o[0] && o[1] > o[2], "PS2: a grey turns green (%.3f %.3f %.3f)", o[0], o[1], o[2]);
        PpGradePixel(&kPpGrade[PPG_PS2], black, o);
        CHECK(o[1] > 0.03f && o[0] > 0.02f && o[1] < 0.1f, "PS2: the blacks lifted (fog) (%.3f %.3f %.3f)", o[0], o[1], o[2]);
        PpGradePixel(&kPpGrade[PPG_OTHER], grey, o);
        CHECK(o[2] > o[1] && o[1] > o[0], "Otherworld: a grey turns cold (%.3f %.3f %.3f)", o[0], o[1], o[2]);
        PpGradePixel(&kPpGrade[PPG_OTHER], red, o);
        CHECK(Chroma(o) < 0.7f * Chroma(red), "Otherworld: drained (chroma %.3f of %.3f)", Chroma(o), Chroma(red));
        PpGradePixel(&kPpGrade[PPG_APT], grey, o);
        CHECK(o[0] > o[1] && o[1] > o[2], "Apartment: a grey turns warm (%.3f %.3f %.3f)", o[0], o[1], o[2]);
        PpGradePixel(&kPpGrade[PPG_APT], red, o);
        CHECK(Chroma(o) >= Chroma(red) * 0.98f, "Apartment: the colour kept");
        {
            const float dark[3] = { 0.15f, 0.15f, 0.15f }, light[3] = { 0.7f, 0.7f, 0.7f };
            PpGradePixel(&kPpGrade[PPG_BLEACH], dark, o);
            PpGradePixel(&kPpGrade[PPG_BLEACH], light, o2);
            CHECK(Luma(o2) - Luma(o) > (0.7f - 0.15f) * 1.1f, "Bleach: harder contrast (%.3f)", Luma(o2) - Luma(o));
            CHECK(Luma(o) > 0.08f, "Bleach: the dark greys of a dark game stay visible (%.3f)", Luma(o));
            PpGradePixel(&kPpGrade[PPG_BLEACH], red, o);
            CHECK(Chroma(o) < 0.8f * Chroma(red), "Bleach: less colour");
        }
        for (g = 1; g < PPG_N; ++g) {  // no preset blows a mid grey out or crushes it (still readable in a dark game)
            PpGradePixel(&kPpGrade[g], grey, o);
            CHECK(Luma(o) > 0.3f && Luma(o) < 0.52f, "preset %d keeps a mid grey near its level (%.3f)", g, Luma(o));
            PpGradePixel(&kPpGrade[g], black, o);
            CHECK(Luma(o) < 0.07f, "preset %d: black stays dark (%.3f)", g, Luma(o));
        }
    }
}

// v3.44 CS344: the v3.43 shaders GOG's translator dropped (the `1-x` modifier), for PsLint
static const PsTok kOldGrade343[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0), I_LRP, D_R(0), S_C(1), S_R(1), S_T(0), I_MUL, D_R(0) | D_X2, S_R(0), S_C(2),
    I_ADD, D_R(0), S_R(0), S_C(3), I_MAD, D_R(0), S_R(1) | S_INV, S_C(4), S_R(0), I_MUL, D_R(1), S_R(1), S_R(1),
    I_MUL, D_R(1), S_R(1), S_C(5), I_ADD, D_R(0) | D_SAT, S_R(0), S_R(1), PS_END };
static void TestGrade344(void)
{
    const char *why;
    int at, g, k, i, r, gg, b, off;
    float chg, exChg, worst = 0.0f;
    CHECK(PsLint(kPpGradePs, 64, &at, &why) == PSL_OK && at == -1, "the new grade uses proven forms only (%s)", why);
    CHECK(PsLint(kOldGrade343, 64, &at, &why) == PSL_MOD && at == 5, "v3.43's grade: a source modifier at instruction 5 (%d %s)", at, why);
    {   // the lint's other rules
        static const PsTok two[] = { PS_VER, PS_TEX(0), I_MAD, D_R(0), S_T(0), S_C(1), S_C(2), PS_END };
        static const PsTok rep[] = { PS_VER, PS_TEX(0), I_MUL, D_R(0), S_RA(0), S_C(1), PS_END };
        static const PsTok bias[] = { PS_VER, PS_TEX(0), I_MUL, D_R(0), S_T(0) | S_BIAS, S_C(1), PS_END };
        static const PsTok late[] = { PS_VER, PS_TEX(0), I_MUL, D_R(0), S_T(0), S_C(1), PS_TEX(1), PS_END };
        static const PsTok mask[] = { PS_VER, PS_TEX(0), I_MUL, 0x80080000u, S_T(0), S_C(1), PS_END };
        static const PsTok cois[] = { PS_VER, PS_TEX(0), I_MUL | 0x40000000u, D_R(0), S_T(0), S_C(1), PS_END };
        static const PsTok r2[] = { PS_VER, PS_TEX(0), I_MUL, D_R(2), S_T(0), S_C(1), PS_END };
        static const PsTok noend[] = { PS_VER, PS_TEX(0), I_MUL, D_R(0), S_T(0), S_C(1) };
        static const PsTok ver[] = { 0xFFFF0104u, PS_TEX(0), PS_END };
        PsTok longp[2 + 2 + 9 * 4 + 1];
        int n = 0;
        longp[n++] = PS_VER, longp[n++] = 0x42u, longp[n++] = 0xB00F0000u;
        for (k = 0; k < 9; ++k) longp[n++] = I_ADD, longp[n++] = D_R(0), longp[n++] = S_R(0), longp[n++] = S_T(0);
        longp[n++] = PS_END;
        CHECK(PsLint(two, 64, &at, &why) == PSL_TWOC, "two constants (%s)", why);
        CHECK(PsLint(rep, 64, &at, &why) == PSL_SWZ, ".a replicate (%s)", why);
        CHECK(PsLint(bias, 64, &at, &why) == PSL_MOD, "_bias (%s)", why);
        CHECK(PsLint(late, 64, &at, &why) == PSL_TEXLATE, "tex after arithmetic (%s)", why);
        CHECK(PsLint(mask, 64, &at, &why) == PSL_DEST, "a partial write mask (%s)", why);
        CHECK(PsLint(cois, 64, &at, &why) == PSL_OPCODE, "co-issue (%s)", why);
        CHECK(PsLint(r2, 64, &at, &why) == PSL_DEST, "r2 (%s)", why);
        CHECK(PsLint(noend, sizeof noend / sizeof noend[0], &at, &why) == PSL_END, "no end token (%s)", why);
        CHECK(PsLint(ver, 64, &at, &why) == PSL_VERSION, "ps.1.4 (%s)", why);
        CHECK(PsLint(longp, n, &at, &why) == PSL_LONG && at == 9, "9 arithmetic instructions (%s at %d)", why, at);
    }
    for (g = 0; g < PPG_N; ++g)  // the rebuilt shader's sim = v3.43's formula (the same picture once it draws)
        for (r = 0; r <= 8; ++r)
            for (gg = 0; gg <= 8; ++gg)
                for (b = 0; b <= 8; ++b) {
                    const PpGradeLook *G = &kPpGrade[g];
                    float in[3], out[3];
                    in[0] = r / 8.0f, in[1] = gg / 8.0f, in[2] = b / 8.0f;
                    PpGradePixel(G, in, out);
                    for (i = 0; i < 3; ++i) {
                        const float L = PP_LUMA_R * in[0] + PP_LUMA_G * in[1] + PP_LUMA_B * in[2];
                        float v = ((1.0f - G->sat) * L + G->sat * in[i]) * G->gain[i] + G->off[i] + (1.0f - L) * G->sh[i] + L * L * G->hi[i];
                        v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
                        if (fabsf(v - out[i]) > worst) worst = fabsf(v - out[i]);
                    }
                }
    CHECK(worst < 2e-6f, "v3.44's grade = v3.43's formula (worst %.2g)", worst);
    for (g = PPG_PS2; g < PPG_N; ++g) {  // the pixel check tells each preset from "unchanged", "nothing" and wrong pixels
        unsigned char ex[PP_PX_N][3], o[PP_PX_N][3];
        int maxd = 0;
        PpPxExpect(&kPpGrade[g], ex);
        for (k = 0; k < PP_PX_N; ++k)
            for (i = 0; i < 3; ++i) {
                const int d = abs((int)ex[k][i] - (int)kPpPxIn[k][i]);
                if (d > maxd) maxd = d;
            }
        CHECK(maxd > 2 * PP_PX_TOL + 1, "preset %d changes a test colour by %d levels (> 2 x the tolerance)", g, maxd);
        CHECK(PpPxVerdict(ex, ex, &off, &chg, &exChg) == PPX_OK && off == 0 && chg == exChg, "the sim read back: ok (%d)", g);
        memcpy(o, ex, sizeof o);
        o[1][1] = (unsigned char)(o[1][1] + PP_PX_TOL);
        o[3][0] = (unsigned char)(o[3][0] - PP_PX_TOL);
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OK && off == PP_PX_TOL, "within the rounding tolerance: ok (%d)", g);
        CHECK(PpPxVerdict(kPpPxIn, ex, &off, &chg, &exChg) == PPX_SAME && chg == 0.0f && exChg > 1.0f,
              "the input unchanged: SAME (the v3.43 failure) (%d: %d off)", g, off);
        for (k = 0; k < PP_PX_N; ++k) o[k][0] = 0xFF, o[k][1] = 0x00, o[k][2] = 0xFF;
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_NOTHING, "the destination's clear colour: NOTHING (%d)", g);
        for (k = 0; k < PP_PX_N; ++k) o[k][0] = o[k][1] = o[k][2] = 0;
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OFF, "black: OFF (%d)", g);
    }
    {
        unsigned char ex[PP_PX_N][3];
        PpPxExpect(&kPpGrade[PPG_PS2], ex);
        printf("grade (v3.44): PS2's test colours 38 -> %u,%u,%u; 128 -> %u,%u,%u; 217 -> %u,%u,%u; (153,77,51) -> %u,%u,%u\n",
               ex[0][0], ex[0][1], ex[0][2], ex[1][0], ex[1][1], ex[1][2], ex[2][0], ex[2][1], ex[2][2], ex[3][0], ex[3][1], ex[3][2]);
    }
}

static void TestLens(void)
{
    float s = 0.0f, t;
    int i;
    CHECK(PpLensTarget(0.0f, 0.0f, 3, 0, 0) == 0.0f, "no haunting: no lens");
    CHECK(PpLensTarget(0.0f, 0.0f, 3, 0, 1) == 1.0f, "LensAlways: full");
    CHECK(fabsf(PpLensTarget(2.0f, 0.0f, 3, 0, 0) - PP_LENS_BASE) < 1e-6f, "a haunting registered, far: the base");
    CHECK(PpLensTarget(2.0f, 1.0f, 3, 0, 0) > 0.99f, "halfway into the aura: full");
    CHECK(PpLensTarget(2.0f, 0.4f, 3, 0, 0) > PP_LENS_BASE && PpLensTarget(2.0f, 0.4f, 3, 0, 0) < 1.0f, "between: between");
    CHECK(PpLensTarget(2.0f, 5.0f, 3, 0, 0) == 1.0f, "a falloff above the sum: clamped");
    CHECK(PpLensTarget(0.5f, 0.1f, 3, 0, 0) == PpLensTarget(50.0f, 10.0f, 3, 0, 0), "no unit: the ratio counts");
    CHECK(PpLensTarget(-1.0f, 0.5f, 3, 0, 0) == 0.0f && PpLensTarget(1e9f, 1.0f, 3, 0, 0) == 0.0f, "nonsense sums: none");
    {
        volatile float zero = 0.0f;
        const float nan = zero / zero;
        CHECK(PpLensTarget(nan, 0.5f, 3, 0, 0) == 0.0f && PpLensTarget(1.0f, nan, 3, 0, 0) == 0.0f, "NaN: none");
        CHECK(PpLensStep(nan, 1.0f, 0.1f) >= 0.0f && PpLensStep(nan, 1.0f, 0.1f) <= 1.0f, "NaN strength recovers");
    }
    CHECK(PpLensTarget(0.0f, 0.0f, 3, 1, 0) == PP_LENS_OW && PpLensTarget(0.0f, 0.0f, 1, 1, 0) == 0.0f &&
          PpLensTarget(0.0f, 0.0f, 0x0A, 1, 0) == 0.0f, "LensOtherworld: faint in Otherworld stages, not in Room 302");
    CHECK(PpLensTarget(2.0f, 1.0f, 3, 1, 0) > 0.99f, "LensOtherworld does not cap a near haunting");
    for (t = 0.0f, i = 0; i < 200 && s < 1.0f; ++i, t += 1.0f / 60.0f) s = PpLensStep(s, 1.0f, 1.0f / 60.0f);
    CHECK(fabsf(t - PP_LENS_IN_S) < 0.05f, "fades in over %.1f s (%.2f)", PP_LENS_IN_S, t);
    for (t = 0.0f, i = 0; i < 400 && s > 0.0f; ++i, t += 1.0f / 60.0f) s = PpLensStep(s, 0.0f, 1.0f / 60.0f);
    CHECK(fabsf(t - PP_LENS_OUT_S) < 0.05f, "fades out over %.1f s (%.2f)", PP_LENS_OUT_S, t);
    CHECK(PpLensStep(0.0f, 1.0f, 5.0f) <= 0.25f / PP_LENS_IN_S + 1e-6f, "a 5 s hitch: no jump");
    CHECK(PpLensStep(0.3f, 0.3f, 0.1f) == 0.3f && PpLensStep(0.3f, 1.0f, 0.0f) == 0.3f && PpLensStep(0.3f, 1.0f, -1.0f) == 0.3f, "holds");
    CHECK(PpLensStep(0.9f, 0.4f, 1.0f / 60.0f) >= 0.4f && PpLensStep(0.41f, 0.4f, 1.0f) == 0.4f, "fades out to the target, not past it");
    {   // the warp
        const float asp[3] = { 4.0f / 3.0f, 16.0f / 9.0f, 32.0f / 9.0f };
        int a, ch, x, y;
        for (a = 0; a < 3; ++a)
            for (ch = 0; ch < 3; ++ch) {
                const float k = PpLensK(1.0f, 1.5f, ch);  // the breathing's top
                float u, v;
                PpLensUv(0.5f, 0.5f, asp[a], k, &u, &v);
                CHECK(u == 0.5f && v == 0.5f, "the centre stays");
                for (y = 0; y <= PP_LENS_GY; ++y)
                    for (x = 0; x <= PP_LENS_GX; ++x) {
                        const float u0 = (float)x / PP_LENS_GX, v0 = (float)y / PP_LENS_GY;
                        PpLensUv(u0, v0, asp[a], k, &u, &v);
                        CHECK(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f, "every sample inside the frame");
                        CHECK(fabsf(u - 0.5f) <= fabsf(u0 - 0.5f) + 1e-6f && fabsf(v - 0.5f) <= fabsf(v0 - 0.5f) + 1e-6f, "never outward");
                        CHECK(fabsf(u - u0) < 0.04f && fabsf(v - v0) < 0.04f, "gentle: at most 4 %% of the frame (%.3f %.3f)", u - u0, v - v0);
                    }
                PpLensUv(0.0f, 0.0f, asp[a], 0.0f, &u, &v);
                CHECK(u == 0.0f && v == 0.0f, "strength 0: the identity");
            }
        {
            float ur, vr, ub, vb, ug, vg;
            PpLensUv(1.0f, 1.0f, 16.0f / 9.0f, PpLensK(1.0f, 0.0f, 0), &ur, &vr);
            PpLensUv(1.0f, 1.0f, 16.0f / 9.0f, PpLensK(1.0f, 0.0f, 1), &ug, &vg);
            PpLensUv(1.0f, 1.0f, 16.0f / 9.0f, PpLensK(1.0f, 0.0f, 2), &ub, &vb);
            CHECK(ur < ug && ug < ub, "red further in than green than blue at the corner (%.4f %.4f %.4f)", ur, ug, ub);
            CHECK(ub - ur < 0.02f && ub - ur > 0.004f, "the fringe slight: %.4f of the width", ub - ur);
        }
        for (t = 0.0f; t < PP_LENS_BREATHE_S; t += 0.25f)
            CHECK(PpLensK(1.0f, t, 2) >= 0.0f && PpLensK(1.0f, t, 0) <= 0.1f, "the breathing stays gentle and blue never outward");
        CHECK(PpLensK(0.0f, 2.0f, 0) == 0.0f, "strength 0: k 0");
    }
}

// v3.43 REV343 PP343-001: the put-back's guard: a normal pass puts back once; a fault after the switch puts back once
// (the handler), the tail never runs; a fault in the handler's own put-back is not repeated; nothing armed = nothing
static int g_putBack;
static void Cleanup(int *armed) { if (PpOnceTake(armed)) ++g_putBack; }
static void TestOnce(void)
{
    int armed = 0, fault;
    for (fault = 0; fault < 3; ++fault) {
        g_putBack = 0;
        armed = 1;                       // FxSave succeeded
        if (fault == 1) Cleanup(&armed); // the handler (a fault in the chain): the tail is skipped
        else Cleanup(&armed);            // the tail
        if (fault == 2) Cleanup(&armed); // a fault after the tail's put-back: the handler finds nothing
        CHECK(g_putBack == 1 && armed == 0, "put back exactly once (case %d: %d)", fault, g_putBack);
    }
    g_putBack = 0;
    Cleanup(&armed);
    CHECK(g_putBack == 0, "nothing armed (FxSave failed): nothing put back");
    CHECK(!PpOnceTake(NULL), "no guard: nothing");
}

// v3.45 RB345: the read back's path choice (post_sim.h RbOrder / RbNote): the lockable target first; the path that worked
// first from then on; a path given up after RB_TRIES failures only if it never worked; off (one event) when every path
// is given up; nothing tried once off; a winner that fails is kept (LOST) and the next path takes over (SWITCH)
static void TestReadBack(void)
{
    RbState s;
    int o[RBP_N], n, i, ev, offs;
    RbInit(&s);
    n = RbOrder(&s, o);
    CHECK(n == 2 && o[0] == RBP_LOCKRT && o[1] == RBP_IMAGE && s.win == -1 && !s.off, "fresh: the lockable target, then the image surface");
    // dgVoodoo2 Direct3D 11 (the v3.43 log): the lockable target works at once
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_OK, 0) == RBE_FIRST && s.win == RBP_LOCKRT, "D3D11: the first success is logged once");
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_OK, 0) == RBE_NONE, "D3D11: the next successes are quiet");
    n = RbOrder(&s, o);
    CHECK(n == 2 && o[0] == RBP_LOCKRT, "D3D11: the lockable target stays first");
    // GOG's own wrapper without a lockable target: the image surface works, then goes first
    RbInit(&s);
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_MAKE, 0x8876086CuL) == RBE_NONE, "stock: a first failure is quiet");
    CHECK(RbNote(&s, RBP_IMAGE, RBS_OK, 0) == RBE_FIRST && s.win == RBP_IMAGE, "stock: the image surface's success is the one line");
    n = RbOrder(&s, o);
    CHECK(n == 2 && o[0] == RBP_IMAGE && o[1] == RBP_LOCKRT, "stock: the image surface first now");
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_COPY, 1) == RBE_NONE && RbGivenUp(&s, RBP_LOCKRT), "stock: the lockable target given up after 2");
    n = RbOrder(&s, o);
    CHECK(n == 1 && o[0] == RBP_IMAGE, "stock: only the image surface is tried");
    // no read back at all: two calls of two tries, then off, once; nothing tried after
    RbInit(&s);
    offs = 0;
    for (i = 0; i < 6; ++i) {
        int k;
        n = RbOrder(&s, o);
        for (k = 0; k < n; ++k) {
            ev = RbNote(&s, o[k], RBS_COPY, 0x8876086CuL);
            CHECK(ev == RBE_NONE || ev == RBE_OFF, "none: only quiet or off (%d)", ev);
            offs += ev == RBE_OFF;
        }
        CHECK(i < 2 ? n == 2 : n == 0, "none: call %d tries %d path(s)", i, n);
    }
    CHECK(offs == 1 && s.off && s.fail[RBP_LOCKRT] == 2 && s.fail[RBP_IMAGE] == 2 && s.step[RBP_IMAGE] == RBS_COPY &&
          s.hr[RBP_IMAGE] == 0x8876086CuL, "none: off logged once, after RB_TRIES each");
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_OK, 0) == RBE_NONE && s.off, "off stays off (a late success is ignored)");
    // a winner that fails later: kept, the other path takes over, then the winner again
    RbInit(&s);
    RbNote(&s, RBP_LOCKRT, RBS_OK, 0);
    CHECK(RbNote(&s, RBP_LOCKRT, RBS_LOCK, 5) == RBE_LOST && !RbGivenUp(&s, RBP_LOCKRT), "a path that worked is never given up");
    CHECK(RbNote(&s, RBP_IMAGE, RBS_OK, 0) == RBE_SWITCH && s.win == RBP_IMAGE, "the other path takes over: one switch line");
    for (i = 0; i < 5; ++i) RbNote(&s, RBP_LOCKRT, RBS_COPY, 5);
    n = RbOrder(&s, o);
    CHECK(!s.off && n == 2 && o[0] == RBP_IMAGE && o[1] == RBP_LOCKRT, "the old winner still a fallback");
    CHECK(RbNote(&s, RBP_IMAGE, RBS_COPY, 5) == RBE_LOST && !s.off, "both have worked: never off");
    // bad input
    RbInit(&s);
    CHECK(RbNote(&s, -1, RBS_OK, 0) == RBE_NONE && RbNote(&s, RBP_N, RBS_COPY, 0) == RBE_NONE && s.win == -1, "no such path: nothing");
    RbNote(&s, RBP_IMAGE, 99, 0);
    CHECK(s.step[RBP_IMAGE] > RBS_OK && s.step[RBP_IMAGE] < RBS_N, "an unknown step stays a name index");
    for (i = 0; i < RBP_N; ++i) CHECK(kRbPathName[i][0] && kRbPathShort[i][0], "path names %d", i);
    for (i = 0; i < RBS_N; ++i) CHECK(kRbStepName[i][0], "step names %d", i);
}

// v3.47 PX347: the grade's pixel check. (1) The v3.46 log (SH4Refined.log 2026-11-24 18:45, dgVoodoo2 Direct3D 11): the
// v3.44 pattern under a sampler half a texel left (black past the edge) reproduces every value logged within 1 level
// and the "off" maxima, so the check read across cells; the shader is its sim. (2) The v3.47 pattern passes under any
// offset of up to 1 texel either way, clamp or black, and measures the offset. (3) A Clear that does not land: found in
// the source read back, the shader judged on what the source held.
static void PxmGrade(const float in[4], float out[4], const void *ctx)
{
    PpGradePixel((const PpGradeLook *)ctx, in, out);
    out[3] = 1.0f;
}
static void TestPx347(void)
{
    static const unsigned char kLog[PPG_N][3][3] = {  // the v3.46 log's "test colours" (three cells a line)
        { { 0 } },
        { { 25, 32, 25 }, { 84, 93, 82 }, { 164, 179, 160 } },
        { { 16, 20, 29 }, { 74, 82, 95 }, { 154, 169, 189 } },
        { { 26, 21, 14 }, { 96, 85, 69 }, { 195, 175, 143 } },
        { { 10, 10, 11 }, { 88, 88, 88 }, { 197, 197, 195 } } };
    static const int kLogOff[PPG_N] = { 0, 70, 77, 72, 88 };
    static PxmTex t;
    unsigned long cells[PX_CELLS], old[PX_CELLS];
    unsigned char o[PP_PX_N][3], ex[PP_PX_N][3], in[PP_PX_N][3];
    PxProbe p;
    int g, k, i, off, n, worst = 0, cases = 0, bx, oi, oj;
    float chg, exChg, wo = 0.0f;
    for (k = 0; k < PX_CELLS; ++k) cells[k] = 0xFF000000uL | (unsigned long)kPpPxIn[k][0] << 16 | (unsigned long)kPpPxIn[k][1] << 8 | kPpPxIn[k][2];
    CHECK(PX_W == 32 && PX_H == 4 && PxCol(0, PXC_MID) == 4 && PxCol(3, PXC_FIRST) == 24 && PxCol(3, PXC_LAST) == 31 &&
          PxRow(PXC_MID) == 2 && PxRow(PXC_TOP) == 0, "the pattern: 4 cells of 8 x 4, read at (8k + 4, 2)");
    for (g = PPG_PS2; g < PPG_N; ++g) {  // (1)
        PxmOld(cells, -1, -0.5f, -0.5f, 1, PxmGrade, &kPpGrade[g], PP_PX_DST, 0, old);
        for (k = 0; k < PP_PX_N; ++k)
            o[k][0] = (unsigned char)(old[k] >> 16), o[k][1] = (unsigned char)(old[k] >> 8), o[k][2] = (unsigned char)old[k];
        for (k = 0; k < 3; ++k)
            for (i = 0; i < 3; ++i) {
                const int d = abs((int)o[k][i] - (int)kLog[g][k][i]);
                if (d > worst) worst = d;
            }
        PpPxExpect(&kPpGrade[g], ex);
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OFF && abs(off - kLogOff[g]) <= 1,
              "v3.46's pattern half a texel off: FAILED, %d levels off (logged %d) (%d)", off, kLogOff[g], g);
        PxmOld(cells, -1, 0.0f, 0.0f, 0, PxmGrade, &kPpGrade[g], PP_PX_DST, 0, old);  // GOG's own wrapper: on the centres
        for (k = 0; k < PP_PX_N; ++k)
            o[k][0] = (unsigned char)(old[k] >> 16), o[k][1] = (unsigned char)(old[k] >> 8), o[k][2] = (unsigned char)old[k];
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OK, "v3.46's pattern on the texel centres: ok (%d off) (%d)", off, g);
    }
    CHECK(worst <= 1, "the v3.46 log = the grade's sim on half / half mixed cells, within %d level(s) (12 cells x 3)", worst);
    for (g = PPG_PS2; g < PPG_N; ++g)  // (2)
        for (bx = 0; bx < 2; ++bx)
            for (oi = -4; oi <= 4; ++oi)
                for (oj = -2; oj <= 2; ++oj) {
                    const float ox = 0.25f * (float)oi, oy = 0.25f * (float)oj;
                    float m;
                    PxmFill(&t, PX_W, PX_H, PX_CW, cells, PX_CELLS, -1);
                    PxmProbe(&t, ox, oy, bx, PxmGrade, &kPpGrade[g], PP_PX_DST, 0, &p);
                    for (k = 0; k < PP_PX_N; ++k)
                        o[k][0] = (unsigned char)(p.dst[k][PXC_MID] >> 16), o[k][1] = (unsigned char)(p.dst[k][PXC_MID] >> 8),
                        o[k][2] = (unsigned char)p.dst[k][PXC_MID];
                    PpPxExpect(&kPpGrade[g], ex);
                    CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OK && off <= 1, "v3.47 pattern ok at (%.2f, %.2f) %s (%d off) (%d)",
                          ox, oy, bx ? "black" : "clamp", off, g);
                    for (k = 0; k < PX_CELLS; ++k) CHECK(PxSrcHeld(cells[k], p.src[k], 0x00FFFFFFuL), "the source as asked (%d)", k);
                    m = PxEdgeOffset(&p, 8, &n);
                    CHECK(n >= 4 && fabsf(m - ox) <= 0.12f, "the offset measured %.3f for %.2f (%d edges) (%d)", m, ox, n, g);
                    if (fabsf(m - ox) > wo) wo = fabsf(m - ox);
                    ++cases;
                }
    printf("pixel check (v3.47): the v3.46 log reproduced within %d level(s); the new pattern ok in %d cases, the offset measured "
           "within %.3f texel\n", worst, cases, wo);
    {  // (3) the third cell's Clear lost (the v3.46 shadow check's source): found, then judged on the source as read back
        PxmFill(&t, PX_W, PX_H, PX_CW, cells, PX_CELLS, 2);
        PxmProbe(&t, -0.5f, -0.5f, 1, PxmGrade, &kPpGrade[PPG_BLEACH], PP_PX_DST, 0, &p);
        for (k = 0; k < PP_PX_N; ++k) {
            o[k][0] = (unsigned char)(p.dst[k][PXC_MID] >> 16), o[k][1] = (unsigned char)(p.dst[k][PXC_MID] >> 8), o[k][2] = (unsigned char)p.dst[k][PXC_MID];
            in[k][0] = (unsigned char)(p.src[k] >> 16), in[k][1] = (unsigned char)(p.src[k] >> 8), in[k][2] = (unsigned char)p.src[k];
        }
        CHECK(!PxSrcHeld(cells[2], p.src[2], 0x00FFFFFFuL) && PxSrcHeld(cells[1], p.src[1], 0x00FFFFFFuL), "a lost Clear: found in the source");
        PpPxExpect(&kPpGrade[PPG_BLEACH], ex);
        CHECK(PpPxVerdict(o, ex, &off, &chg, &exChg) == PPX_OFF, "a lost Clear: against the colours asked, FAILED");
        PpPxExpectFrom(&kPpGrade[PPG_BLEACH], in, ex);
        CHECK(PpPxVerdictIn(o, ex, in, &off, &chg, &exChg) == PPX_OK && off <= 1, "a lost Clear: on the source as read back, ok (%d)", off);
        CHECK(PpPxVerdictIn(in, ex, in, &off, &chg, &exChg) == PPX_SAME, "the source itself read back: SAME");
    }
    CHECK(PxSrcHeld(0x80000000uL, 0x81000000uL, 0xFF000000uL) && !PxSrcHeld(0x80000000uL, 0x82000000uL, 0xFF000000uL) &&
          PxSrcHeld(0x80000000uL, 0x80FFFFFFuL, 0xFF000000uL) && !PxSrcHeld(0xFF262626uL, 0x00000000uL, 0x00FFFFFFuL) &&
          PxSrcHeld(0xFF262626uL, 0x00272526uL, 0x00FFFFFFuL), "PxSrcHeld: 1 level, the mask's bytes only");
    memset(&p, 0, sizeof p);
    CHECK(PxEdgeOffset(&p, 8, &n) == 0.0f && n == 0, "a flat probe: no offset measurable");
}

int main(void)
{
    TestPx347();  // v3.47 PX347
    TestReadBack();  // v3.45 RB345
    TestOnce();
    TestParse();
    TestFilters();
    TestPlan();
    TestBright();
    TestGrade();
    TestGrade344();  // v3.44 CS344
    TestLens();
    printf("post_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
