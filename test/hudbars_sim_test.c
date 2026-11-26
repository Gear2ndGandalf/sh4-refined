// HUD BARS: native test of src\hudbars_sim.h (test\build_hb_test.cmd). Pure logic only: bar fade / trail timing at any
// frame rate, the want rule, the ghost down timer, the curse stage mapping (game tiers 0x43E930 and Playable Eileen's
// meter marks) and the meter's wake / label timing.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/hudbars_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int Near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

// A scripted fight: full health; locked on at 0.5 s; hits at 1.0 s (-30 %) and 2.0 s (-20 %); lock lost at 3.0 s; the
// bar stays up 4 s after the last hit (to 6.0 s) and then fades. Samples at fixed moments (whole frames at every rate).
typedef struct { float alpha, shown, trail, since; } Snap;
static const float kT[] = { 0.25f, 0.50f, 0.60f, 1.00f, 1.30f, 1.60f, 2.00f, 2.50f, 3.00f, 5.90f, 6.00f, 6.40f, 7.00f };
#define NT (int)(sizeof kT / sizeof kT[0])
static void RunFight(int fps, Snap out[NT])
{
    HbBar b = { 0 };
    const int frames = 8 * fps;
    int f, k = 0;
    for (f = 1; f <= frames && k < NT; ++f) {
        const float t = (float)f / (float)fps, dt = 1.0f / (float)fps;
        const float hp = t >= 2.0f - 1e-6f ? 0.5f : t >= 1.0f - 1e-6f ? 0.7f : 1.0f;
        const int locked = t >= 0.5f - 1e-6f && t < 3.0f - 1e-6f;
        HbBarStep(&b, hp, HbWant(HB_MODE_LOCKED, locked, b.sinceHit, 300.0f), 1, dt);
        while (k < NT && fabsf(t - kT[k]) < 0.5f / (float)fps) {
            out[k].alpha = b.alpha, out[k].shown = b.shown, out[k].trail = b.trail, out[k].since = b.sinceHit;
            ++k;
        }
    }
}

static void TestFrameRates(void)
{
    static const int kFps[] = { 30, 60, 120, 144, 240 };
    Snap ref[NT], s[NT];
    int i, k;
    RunFight(30, ref);
    // the 30 fps run itself, against hand-computed values
    CHECK(Near(ref[0].alpha, 0.0f, 1e-4f), "not shown before the lock (%.3f)", ref[0].alpha);
    // (an event is seen in the frame that ends at its moment: within one frame, 1 / (30 x 0.2) of the fade at 30 fps)
    CHECK(Near(ref[2].alpha, 0.5f, 0.17f), "0.1 s after the lock: about half faded in (%.3f)", ref[2].alpha);
    CHECK(Near(ref[3].alpha, 1.0f, 1e-4f), "fully in (%.3f)", ref[3].alpha);
    CHECK(Near(ref[3].shown, 0.7f, 1e-5f) && Near(ref[3].trail, 1.0f, 1e-5f), "hit: trail held at 1 (%.3f %.3f)", ref[3].shown, ref[3].trail);
    CHECK(Near(ref[4].trail, 1.0f, 1e-4f), "0.3 s after: still held (%.3f)", ref[4].trail);
    CHECK(Near(ref[5].trail, 1.0f - 0.6f * 0.15f, 2e-3f), "0.6 s after: 0.15 s of drain (%.3f)", ref[5].trail);
    CHECK(Near(ref[6].trail, 0.7f, 2e-3f), "second hit: the trail had reached 0.7 at 1.95 s and holds there (%.3f)", ref[6].trail);
    CHECK(Near(ref[7].trail, 0.7f - 0.6f * 0.05f, 2e-3f), "held 0.45 s again, then drains (%.3f)", ref[7].trail);
    CHECK(Near(ref[8].trail, 0.5f, 2e-3f), "drained to the new share (%.3f)", ref[8].trail);
    CHECK(Near(ref[8].alpha, 1.0f, 1e-4f), "lock lost at 3 s, still up (recent hit)");
    CHECK(Near(ref[9].alpha, 1.0f, 1e-4f), "5.9 s: still up (hit 3.9 s ago)");
    CHECK(Near(ref[11].alpha, 0.5f, 0.05f), "6.4 s: half faded out (%.3f)", ref[11].alpha);
    CHECK(Near(ref[12].alpha, 0.0f, 1e-4f), "7.0 s: gone (%.3f)", ref[12].alpha);
    for (i = 1; i < (int)(sizeof kFps / sizeof kFps[0]); ++i) {
        RunFight(kFps[i], s);
        for (k = 0; k < NT; ++k) {
            const float tolA = 1.0f / (30.0f * HB_FADE_IN_S) + 1e-3f;  // the fade target can flip inside one 30 fps frame
            CHECK(Near(s[k].trail, ref[k].trail, 2e-3f), "%d fps t %.2f: trail %.4f vs %.4f", kFps[i], kT[k], s[k].trail, ref[k].trail);
            CHECK(Near(s[k].shown, ref[k].shown, 1e-6f), "%d fps t %.2f: shown", kFps[i], kT[k]);
            CHECK(Near(s[k].alpha, ref[k].alpha, tolA), "%d fps t %.2f: alpha %.4f vs %.4f", kFps[i], kT[k], s[k].alpha, ref[k].alpha);
        }
    }
}

static void TestWant(void)
{
    CHECK(HbWant(HB_MODE_OFF, 1, 0.0f, 0.0f) == 0.0f, "Off never shows");
    CHECK(HbWant(HB_MODE_LOCKED, 1, 1e6f, 9999.0f) == 1.0f, "locked shows at any distance");
    CHECK(HbWant(HB_MODE_LOCKED, 0, 1.0f, 9999.0f) == 1.0f, "recently hit shows");
    CHECK(HbWant(HB_MODE_LOCKED, 0, HB_HIT_S + 0.01f, 100.0f) == 0.0f, "Locked Target: near alone does not show");
    CHECK(HbWant(HB_MODE_NEAR, 0, 1e6f, 100.0f) == 1.0f, "All Near: near shows");
    CHECK(Near(HbWant(HB_MODE_NEAR, 0, 1e6f, HB_NEAR_R - 150.0f), 0.5f, 1e-4f), "All Near: edge band");
    CHECK(HbWant(HB_MODE_NEAR, 0, 1e6f, HB_NEAR_R + 1.0f) == 0.0f, "All Near: far is hidden");
    {
        const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
        CHECK(HbWant(HB_MODE_NEAR, 0, 1e6f, nan) == 0.0f, "NaN distance hidden");
    }
}

static void TestSeenAndHeal(void)
{
    HbBar b = { 0 };
    int i;
    for (i = 0; i < 30; ++i) HbBarStep(&b, 1.0f, 1.0f, 1, 1.0f / 30.0f);
    CHECK(Near(b.alpha, 1.0f, 1e-4f), "in");
    for (i = 0; i < 12; ++i) HbBarStep(&b, 1.0f, 1.0f, 0, 1.0f / 30.0f);  // behind a wall 0.4 s
    CHECK(Near(b.alpha, 0.5f, 0.01f), "behind a wall: fading out (%.3f)", b.alpha);
    HbBarStep(&b, 0.4f, 1.0f, 1, 1.0f / 30.0f);
    HbBarStep(&b, 0.9f, 1.0f, 1, 1.0f / 30.0f);  // a ghost getting up / healing
    CHECK(Near(b.trail, 1.0f, 1e-4f) && Near(b.shown, 0.9f, 1e-6f), "a rise keeps the trail >= shown");
    HbBarStep(&b, 0.9f, 1.0f, 1, 5.0f);  // a 5 s hitch counts as 0.1 s
    CHECK(b.alpha <= 1.0f && b.trail >= b.shown, "long frame clamped");
    {
        const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
        HbBarStep(&b, nan, nan, 1, nan);
        CHECK(b.shown == 0.0f && b.alpha == b.alpha && b.trail == b.trail, "NaN input stays finite");
    }
}

static void TestDown(void)
{
    float m = 0.0f, f;
    f = HbDownFrac(&m, 30.0f, 1);
    CHECK(Near(f, 1.0f, 1e-6f) && Near(m, 30.0f, 1e-6f), "down: full");
    f = HbDownFrac(&m, 15.0f, 0);
    CHECK(Near(f, 0.5f, 1e-6f), "down: half");
    f = HbDownFrac(&m, 31.0f, 0);  // held up by Death Presence / a later larger value
    CHECK(Near(f, 1.0f, 1e-6f) && Near(m, 31.0f, 1e-6f), "down: a larger value is the new length");
    f = HbDownFrac(&m, 8.0f, 1);  // a new down (stun gun: 3-10 s) learns its own length
    CHECK(Near(f, 1.0f, 1e-6f) && Near(m, 8.0f, 1e-6f), "new down");
    f = HbDownFrac(&m, -1.0f, 0);
    CHECK(f == 0.0f, "negative countdown = 0");
}

static void TestStages(void)
{
    static const float kGame[3] = { 0.25f, 0.50f, 0.70f };  // EileenCurseTier (0x43E930 / 0x43E980)
    static const float kPe[3] = { 0.35f, 0.50f, 1.00f };    // PE defaults: FitsFrom 35, CursedIdleFrom 50, full 100
    static const float kPeNoFits[3] = { -1.0f, 0.50f, 1.00f };
    const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
    CHECK(HbCurseStage(0.0f, kGame) == 0, "0");
    CHECK(HbCurseStage(0.2499f, kGame) == 0, "just under 0.25");
    CHECK(HbCurseStage(0.25f, kGame) == 1, "0.25 = tier 1 (>=)");
    CHECK(HbCurseStage(0.4999f, kGame) == 1, "under 0.5");
    CHECK(HbCurseStage(0.5f, kGame) == 2, "0.5 = tier 2");
    CHECK(HbCurseStage(0.6999f, kGame) == 2, "under 0.7");
    CHECK(HbCurseStage(0.7f, kGame) == 3, "0.7 = tier 3");
    CHECK(HbCurseStage(0.95f, kGame) == 3, "the target's cap 0.95");
    CHECK(HbCurseStage(1.0f, kGame) == 3, "1");
    CHECK(HbCurseStage(nan, kGame) == 0, "NaN (a broken save, curse_mechanics 6) = 0");
    CHECK(HbCurseStage(0.34f, kPe) == 0 && HbCurseStage(0.35f, kPe) == 1, "PE fits from 35");
    CHECK(HbCurseStage(0.99f, kPe) == 2 && HbCurseStage(1.0f, kPe) == 3, "PE possessed only when full");
    CHECK(HbCurseStage(0.40f, kPeNoFits) == 0 && HbCurseStage(0.6f, kPeNoFits) == 2, "a skipped stage stays skipped");
    {   // the game's tier function as the decomp has it, against the mapping, on a fine grid
        int i, bad = 0;
        for (i = 0; i <= 10000; ++i) {
            const float c = (float)i / 10000.0f;
            const int tier = c >= 0.7f ? 3 : c >= 0.5f ? 2 : c >= 0.25f ? 1 : 0;
            if (HbCurseStage(c, kGame) != tier) ++bad;
        }
        CHECK(bad == 0, "the mapping equals EileenCurseTier on 10001 values (%d differ)", bad);
    }
}

static void TestMeter(void)
{
    static const int kFps[] = { 30, 60, 144, 240 };
    float ref[4][3];
    int r;
    for (r = 0; r < 4; ++r) {
        HbMeter m = { 0 };
        const int fps = kFps[r];
        int f;
        for (f = 1; f <= 12 * fps; ++f) {
            const float t = (float)f / (float)fps;
            const float v = t < 5.0f - 1e-6f ? 0.20f : 0.30f;  // stage 1 from 5 s
            HbMeterStep(&m, v, v >= 0.25f, 1, 1.0f / (float)fps);
            if (f == 2 * fps) ref[r][0] = HbMeterStrength(&m);   // 2 s: still awake
            if (f == 5 * fps) ref[r][1] = HbLabelAlpha(&m);      // the stage changed now: label starts
            if (f == 10 * fps) ref[r][2] = HbMeterStrength(&m);  // rest
        }
        if (r == 0) {
            CHECK(Near(ref[0][0], 1.0f, 1e-4f), "awake at 2 s (%.3f)", ref[0][0]);
            CHECK(Near(ref[0][2], HB_REST_A, 1e-4f), "rests at 10 s (%.3f)", ref[0][2]);
        } else {
            CHECK(Near(ref[r][0], ref[0][0], 1e-3f) && Near(ref[r][2], ref[0][2], 1e-3f), "%d fps: meter timing", fps);
            CHECK(Near(ref[r][1], ref[0][1], 1.0f / (30.0f * 0.25f) + 1e-3f), "%d fps: label start", fps);
        }
    }
    {
        HbMeter m = { 0 };
        int f;
        for (f = 0; f < 30; ++f) HbMeterStep(&m, 0.6f, 2, 1, 1.0f / 30.0f);
        for (f = 0; f < 30; ++f) HbMeterStep(&m, 0.6f, 2, 0, 1.0f / 30.0f);  // she leaves the party
        CHECK(m.alpha == 0.0f && !m.init, "gone after 0.4 s away");
        HbMeterStep(&m, 0.6f, 2, 1, 1.0f / 30.0f);
        CHECK(Near(m.label, HB_LABEL_S, 1e-6f), "back: the stage is named again");
    }
}

// ---------------------------------------------------------------- v3.30 HB330: ghost health (RE_GHOST_HEALTH.md)
static void TestGhostState(void)
{
    int t;
    CHECK(HbGhostState(0x0D, 3, 0.0f, 0) == HB_GS_STAND, "standing");
    CHECK(HbGhostState(0x0D, 8, 0.0f, 0) == HB_GS_RISING, "rising");
    CHECK(HbGhostState(0x0D, 9, 0.0f, 0) == HB_GS_STUMBLE, "class 9: a stumble, rises at once");
    CHECK(HbGhostState(0x0D, 0x0A, 42.0f, 0) == HB_GS_DOWN, "class 0x0A with a countdown: down");
    CHECK(HbGhostState(0x0D, 0x0A, 0.0f, 0) == HB_GS_STUMBLE, "class 0x0A at 0: its last tick");
    for (t = 0x0B; t <= 0x0F; ++t) {
        CHECK(HbGhostState(t, 0x0B, 0.0f, 0) == HB_GS_PINNED, "type %X class 0x0B = pinned", t);
        CHECK(HbGhostState(t, 0x0A, 30.0f, 1) == HB_GS_PINNED, "type %X pin mailbox = pinned", t);
    }
    CHECK(HbGhostState(0x11, 0x0B, 0.0f, 0) == HB_GS_GONE, "the Patient cannot be pinned: 0x0B = dead");
    CHECK(HbGhostState(0x11, 0x0A, 5.0f, 1) == HB_GS_DOWN, "the Patient ignores the mailbox");
    CHECK(HbGhostState(0x0D, 0x10, 0.0f, 0) == HB_GS_GONE, "scripted");
    CHECK(HbIsGhostType(0x11) && !HbIsGhostType(0x10) && !HbIsGhostType(0x0A), "ghost types");
    // the rise heal
    CHECK(Near(HbGhostRiseHp(0x0D, 0.0f, 800.0f, 3, 1, 0.0f), 400.0f, 1e-3f), "Jasper at HP 0: 50 %%");
    CHECK(Near(HbGhostRiseHp(0x0E, 0.0f, 800.0f, 3, 0, 0.0f), 320.0f, 1e-3f), "Andrew at HP 0: 40 %% (count not needed)");
    CHECK(Near(HbGhostRiseHp(0x0B, 300.0f, 400.0f, 0, 1, 0.0f), 400.0f, 1e-3f), "count 0: + max / 4, capped at max");
    CHECK(Near(HbGhostRiseHp(0x0B, 100.0f, 400.0f, 1, 1, 0.0f), 180.0f, 1e-3f), "count 1: + max / 5");
    CHECK(HbGhostRiseHp(0x0B, 100.0f, 400.0f, 1, 0, 0.0f) < 0.0f, "count unknown: no preview");
    CHECK(HbGhostRiseHp(0x0B, 0.0f, 400.0f, 1, 1, 0.5f) < 0.0f, "a haunting aura on it: no heal");
    CHECK(HbGhostRiseHp(0x0B, 0.0f, 0.0f, 1, 1, 0.0f) < 0.0f, "no max");
    CHECK(HbGhostCountOff(0x0B) == 0x4C && HbGhostCountOff(0x0C) == 0x94 && HbGhostCountOff(0x0D) == 0x94 &&
          HbGhostCountOff(0x0E) == 0x60 && HbGhostCountOff(0x0F) == 0x14 && HbGhostCountOff(0x11) == 0x14 &&
          HbGhostCountOff(0x05) == -1, "get-up count offsets");
}

// A scripted ghost: stands to 1 s; down (class 0x0A) with a 40 s countdown from 1 s; a hit at 11 s halves what is left
// (30 -> 15); pinned at 16 s; released at 20 s with a 3 s countdown; rises at 23 s. Same values at every frame rate.
typedef struct { float alpha, frac, seal, flash; } RevSnap;
static const float kRT[] = { 0.5f, 1.1f, 1.2f, 6.0f, 11.0f, 16.0f, 16.1f, 16.3f, 17.0f, 20.0f, 20.2f, 21.5f, 23.0f, 23.25f, 24.0f };
#define NRT (int)(sizeof kRT / sizeof kRT[0])
static void RunGhost(int fps, RevSnap out[NRT])
{
    HbRevive r = { 0 };
    const int frames = 25 * fps;
    float cd = 0.0f;
    int f, k = 0;
    for (f = 1; f <= frames && k < NRT; ++f) {
        const float t = (float)f / (float)fps, dt = 1.0f / (float)fps, e = 0.5f / (float)fps;
        int cls = 3;
        if (t >= 1.0f - e && t < 16.0f - e) {
            if (fabsf(t - 1.0f) < e) cd = 40.0f;
            if (fabsf(t - 11.0f) < e) cd *= 0.5f;
            cls = 0x0A;
        } else if (t >= 16.0f - e && t < 20.0f - e) {
            cls = 0x0B;
        } else if (t >= 20.0f - e && t < 23.0f - e) {
            if (fabsf(t - 20.0f) < e) cd = 3.0f;
            cls = 0x0A;
        } else if (t >= 23.0f - e && t < 23.5f - e) {
            cls = 8;
        }
        HbReviveStep(&r, HbGhostState(0x0D, cls, cd, 0), cd, dt);
        cd -= dt;  // GhostUpdate after the draw (the order inside one frame does not matter for the test)
        if (cd < 0.0f) cd = 0.0f;
        while (k < NRT && fabsf(t - kRT[k]) < e) {
            out[k].alpha = r.alpha, out[k].frac = r.frac, out[k].seal = r.seal, out[k].flash = r.flash;
            ++k;
        }
    }
}
static void TestRevive(void)
{
    static const int kFps[] = { 60, 120, 180 };
    RevSnap ref[NRT], s[NRT];
    int i, k;
    RunGhost(30, ref);
    CHECK(ref[0].alpha == 0.0f, "standing: no revive bar");
    CHECK(Near(ref[1].alpha, 0.5f, 0.17f), "0.1 s into the down: fading in (%.3f)", ref[1].alpha);
    CHECK(Near(ref[2].frac, 1.0f - 0.2f / 40.0f, 2e-3f), "the countdown share (%.4f)", ref[2].frac);
    CHECK(Near(ref[3].frac, 35.0f / 40.0f, 2e-3f), "6 s: 35 of 40 left (%.4f)", ref[3].frac);
    CHECK(Near(ref[4].frac, 15.0f / 40.0f, 2e-3f), "a hit halves it: 15 of 40 (%.4f)", ref[4].frac);
    CHECK(ref[5].seal > 0.0f && ref[5].flash > 0.9f, "pinned: the flash and the seal start (%.3f %.3f)", ref[5].seal, ref[5].flash);
    CHECK(Near(ref[6].seal, 0.1f / HB_SEAL_IN_S, 0.14f), "0.1 s into the pin (%.3f)", ref[6].seal);
    CHECK(Near(ref[7].seal, 1.0f, 1e-4f) && Near(ref[7].alpha, 1.0f, 1e-4f), "sealed");
    CHECK(Near(ref[8].flash, 0.0f, 1e-4f), "the flash is over in 0.6 s");
    CHECK(Near(ref[10].frac, 1.0f - 0.2f / 3.0f, 0.02f), "released: a new 3 s countdown (%.3f)", ref[10].frac);
    CHECK(Near(ref[10].seal, 1.0f - 0.2f / HB_SEAL_OUT_S, 0.1f), "the seal opens (%.3f)", ref[10].seal);
    CHECK(Near(ref[11].frac, 0.5f, 0.02f), "half of the 3 s (%.3f)", ref[11].frac);
    CHECK(Near(ref[13].alpha, 0.5f, 0.1f), "rising: fading out (%.3f)", ref[13].alpha);
    CHECK(ref[14].alpha == 0.0f && ref[14].seal == 0.0f, "standing again: gone");
    for (i = 0; i < 3; ++i) {
        RunGhost(kFps[i], s);
        for (k = 0; k < NRT; ++k) {
            const float tol = 1.0f / (30.0f * HB_REV_IN_S) + 1e-3f;
            CHECK(Near(s[k].alpha, ref[k].alpha, tol), "%d fps t %.2f: alpha %.3f vs %.3f", kFps[i], kRT[k], s[k].alpha, ref[k].alpha);
            CHECK(Near(s[k].frac, ref[k].frac, 0.012f), "%d fps t %.2f: frac %.4f vs %.4f", kFps[i], kRT[k], s[k].frac, ref[k].frac);
            CHECK(Near(s[k].seal, ref[k].seal, 1.0f / (30.0f * HB_SEAL_IN_S) + 1e-3f), "%d fps t %.2f: seal", kFps[i], kRT[k]);
            CHECK(Near(s[k].flash, ref[k].flash, 1.0f / (30.0f * HB_FLASH_S) + 1e-3f), "%d fps t %.2f: flash", kFps[i], kRT[k]);
        }
    }
    {   // first seen already pinned (a room entered with a pinned ghost): sealed at once, no flash
        HbRevive r = { 0 };
        HbReviveStep(&r, HB_GS_PINNED, 0.0f, 1.0f / 60.0f);
        CHECK(r.seal == 1.0f && r.flash == 0.0f, "already pinned");
        HbReviveStep(&r, HB_GS_DOWN, (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0)), 1.0f / 60.0f);
        CHECK(r.frac == r.frac && r.max == r.max, "NaN countdown stays finite");
    }
    {   // v3.30 MERGE: the sealed bar shows HB_PIN_S (4 s) after the pin at 30 / 60 / 120 / 180 fps; not for a ghost
        // first seen pinned; a want of 1 keeps a Locked Target bar up (HbWant alone would not: no lock, no health lost)
        static const int kF[] = { 30, 60, 120, 180 };
        int j, f;
        for (j = 0; j < 4; ++j) {
            HbRevive r = { 0 };
            HbBar b = { 0 };
            const float dt = 1.0f / (float)kF[j];
            float lastShow = -1.0f, alphaAt45 = -1.0f;
            for (f = 1; f <= 6 * kF[j]; ++f) {
                const float t = (float)f * dt;  // down (HP 0) until 1 s, pinned from then on
                const int st = t < 1.0f - 1e-6f ? HB_GS_DOWN : HB_GS_PINNED;
                float want;
                HbReviveStep(&r, st, st == HB_GS_DOWN ? 60.0f - t : 0.0f, dt);
                want = HbWant(HB_MODE_LOCKED, 0, b.sinceHit, 300.0f);
                if (HbPinShow(&r)) want = 1.0f, lastShow = t;
                HbBarStep(&b, 0.0f, want, 1, dt);
                if (fabsf(t - 4.5f) < 0.5f * dt) alphaAt45 = b.alpha;
            }
            CHECK(Near(lastShow, 5.0f, dt + 1e-3f), "%d fps: the pinned bar wants to show until 4 s after the pin (%.3f)", kF[j], lastShow);
            CHECK(Near(alphaAt45, 1.0f, 1e-4f), "%d fps: shown 3.5 s after the pin (%.3f)", kF[j], alphaAt45);
            CHECK(b.alpha == 0.0f, "%d fps: faded out 1 s after the hold (%.3f)", kF[j], b.alpha);
        }
        {
            HbRevive r = { 0 };
            HbReviveStep(&r, HB_GS_PINNED, 0.0f, 1.0f / 60.0f);
            CHECK(!HbPinShow(&r), "first seen already pinned: no pin hold");
        }
    }
}

static void TestPulse(void)
{
    static const int kFps[] = { 30, 60, 120, 180 };
    float ref[3] = { 0 };
    int r, f;
    for (r = 0; r < 4; ++r) {
        HbMeter m = { 0 };
        const int fps = kFps[r];
        float got[3] = { 0 };
        for (f = 1; f <= 4 * fps; ++f) {
            const float t = (float)f / (float)fps;
            const int st = t < 1.0f - 1e-6f ? 1 : t < 3.0f - 1e-6f ? 2 : 1;  // up at 1 s, down at 3 s
            HbMeterStep(&m, st == 2 ? 0.55f : 0.30f, st, 1, 1.0f / (float)fps);
            if (f == fps) { got[0] = m.pulse; CHECK(m.pulseUp == 1, "%d fps: up", fps); }
            if (f == fps + fps / 2) got[1] = m.pulse;
            if (f == 3 * fps) { got[2] = m.pulse; CHECK(m.pulseUp == 0, "%d fps: down", fps); }
        }
        if (r == 0) {
            memcpy(ref, got, sizeof ref);
            CHECK(Near(got[0], 1.0f, 1e-6f) && Near(got[1], 1.0f - 0.5f / HB_PULSE_S, 1e-3f) && Near(got[2], 1.0f, 1e-6f), "pulse timing");
        } else {
            CHECK(Near(got[1], ref[1], 1e-3f), "%d fps: pulse %.4f vs %.4f", fps, got[1], ref[1]);
        }
    }
    {
        HbMeter m = { 0 };
        HbMeterStep(&m, 0.8f, 3, 1, 1.0f / 30.0f);
        CHECK(m.pulse == 0.0f, "no pulse when the meter appears (the label names the stage)");
    }
    {
        int i, ok = 1;
        float mx = 0.0f;
        for (i = 0; i < 2000; ++i) {
            const float b = HbBeat((float)i * 0.001f);
            if (!(b >= 0.0f && b <= 1.0f)) ok = 0;
            if (b > mx) mx = b;
        }
        CHECK(ok && mx > 0.95f, "heartbeat 0..1 (max %.3f)", mx);
        CHECK(Near(HbBeat(0.08f / 1.1f), 1.0f, 1e-3f), "the first beat");
    }
    {
        HbBar b = { 0 };
        HbBarStep(&b, 1.0f, 1.0f, 1, 1.0f / 60.0f);
        HbBarStep(&b, 0.5f, 1.0f, 1, 1.0f / 60.0f);
        CHECK(Near(HbChipHeat(&b), 1.0f, 1e-6f), "the chip is hot when the hit lands");
        for (f = 0; f < 27; ++f) HbBarStep(&b, 0.5f, 1.0f, 1, 1.0f / 60.0f);
        CHECK(HbChipHeat(&b) == 0.0f, "cold when the hold ends (%.3f)", HbChipHeat(&b));
    }
}

// ---- v3.32 DHS332: Curse Meter = Both (which rings, the small ring's layout and label), Item Shine's lying ghosts
static void TestBoth(void)
{
    int pe, sec, cm, i, hk;
    // which rings: Off nothing; On PE's ring (else the companion's); Companion the companion's; Both PE's + the small ring
    for (pe = 0; pe < 2; ++pe) {
        int mainPe, second;
        CHECK(!HbCurseRings(HB_CM_OFF, pe, HB_PR_NONE, &mainPe, &second) && !mainPe && !second, "Off draws nothing (pe %d)", pe);
        CHECK(HbCurseRings(HB_CM_ON, pe, HB_PR_NONE, &mainPe, &second) && mainPe == pe && !second, "On: PE's ring when loaded (pe %d)", pe);
        CHECK(HbCurseRings(HB_CM_COMPANION, pe, HB_PR_NONE, &mainPe, &second) && !mainPe && !second, "Companion: one ring, hers (pe %d)", pe);
        CHECK(HbCurseRings(HB_CM_BOTH, pe, HB_PR_NONE, &mainPe, &second) && mainPe == pe && second == pe, "Both: PE's + hers; without PE = On (pe %d)", pe);
        CHECK(!HbCurseRings(HB_CM_N, pe, HB_PR_NONE, &mainPe, &second) && !HbCurseRings(-1, pe, HB_PR_NONE, &mainPe, &second), "out of range: nothing");
    }
    {   // Both without PE draws exactly what On draws
        int m1, s1, m2, s2;
        HbCurseRings(HB_CM_ON, 0, HB_PR_NONE, &m1, &s1), HbCurseRings(HB_CM_BOTH, 0, HB_PR_NONE, &m2, &s2);
        CHECK(m1 == m2 && s1 == s2, "Both = On without Playable Eileen");
    }
    (void)cm;
    // the label: E I L N only, a unit between letters
    CHECK(HbTinyWidth("EILEEN") == 22.0f, "EILEEN is 22 units wide (%.1f)", HbTinyWidth("EILEEN"));
    CHECK(HbTinyWidth("") == 0.0f && HbTinyWidth("E") == 3.0f && HbTinyWidth("x") == 0.0f, "empty / one letter / unknown");
    {
        int okG = 1;
        const char *t = "EILN";
        for (i = 0; t[i]; ++i) {
            const HbGlyphDef *g = HbGlyphOf(t[i]);
            int s;
            if (!g || g->n < 1 || g->n > 4) { okG = 0; continue; }
            for (s = 0; s < g->n; ++s) {
                const HbStroke *k = &g->s[s];
                if (!(k->x1 > k->x0) || !(k->y1 > k->y0) || k->x0 < 0.0f || k->x1 + k->slant > (float)g->adv + 1e-4f || k->y1 > 5.0f) okG = 0;
            }
        }
        CHECK(okG, "every stroke inside its letter's cell (5 units tall, its advance wide)");
    }
    // the layout: the small ring (with its thorns) never touches the main ring (with its thorns), and its label ends
    // above the main stage name, at every height and squeeze (sx / sy: 1.094 at 5:4, 1.167 in the 4:3 box, more stretched)
    for (hk = 0; hk < 8; ++hk) {
        static const float kH[8] = { 480, 600, 720, 768, 900, 1080, 1440, 2160 };
        const float h = kH[hk], sy = h / 448.0f, px = h / 1080.0f > 1.0f ? h / 1080.0f : 1.0f;  // (hudbars.inc HbPx)
        float sr;
        for (sr = 1.09375f; sr < 2.0f; sr += 0.15f) {  // (5:4 = 1.094, the 4:3 box 1.167, stretched wider: more)
            const float sx = sy * sr;
            const float mainL = 452.0f * sx - HbRingReach(17.0f * sy, px, 1.0f);
            const float smallR = HB_RING2_CX * sx + HbRingReach(HB_RING2_R * sy, px, HB_RING2_K);
            const float tagBot = (HB_RING2_CY + HB_RING2_R + HB_TAG_GAP + HB_TAG_H) * sy + px;
            const float tagHalf = 0.5f * HbTinyWidth("EILEEN") * HB_TAG_H / 5.0f * sy;
            CHECK(mainL - smallR > 2.0f, "h %.0f sx/sy %.3f: the rings stay %.1f px apart", h, sr, mainL - smallR);
            CHECK(tagBot < HB_LABEL_TOP * sy, "h %.0f: the label ends at %.0f, above the stage name at %.0f", h, tagBot, HB_LABEL_TOP * sy);  // v3.35 CM335
            CHECK(HB_RING2_CX * sx - tagHalf > 0.0f && HB_RING2_CX * sx - tagHalf > (HB_RING2_CX - 30.0f) * sx, "h %.0f: the label stays near its ring", h);
        }
    }
    CHECK(HB_RING2_CY - HB_RING2_R > 25.0f - 1.0f && HB_RING2_CY - HB_RING2_R < 25.0f + 1.0f, "the small ring is top-aligned with the main one");
    // the small ring fades in and out with her at any frame rate (its own meter: 0.4 s), and names nothing (no label stage)
    {
        static const int kF[4] = { 30, 60, 120, 180 };
        float ref = -1.0f, refOut = -1.0f;
        for (i = 0; i < 4; ++i) {
            HbMeter m;
            int f;
            float at02 = 0.0f, out02 = 0.0f;
            memset(&m, 0, sizeof m);
            for (f = 1; f <= 3 * kF[i]; ++f) {
                const float t = (float)f / (float)kF[i];
                const int present = t <= 2.0f + 1e-6f;
                HbMeterStep(&m, 0.33f, 1, present, 1.0f / (float)kF[i]);
                if (fabsf(t - 0.2f) < 0.5f / kF[i]) at02 = m.alpha;
                if (fabsf(t - 2.2f) < 0.5f / kF[i]) out02 = m.alpha;
            }
            if (ref < 0.0f) ref = at02, refOut = out02;
            CHECK(Near(at02, 0.5f, 1e-3f) && Near(at02, ref, 1e-3f), "%d fps: half in 0.2 s after she appears (%.3f)", kF[i], at02);
            CHECK(Near(out02, 0.5f, 1e-3f) && Near(out02, refOut, 1e-3f), "%d fps: half out 0.2 s after she goes (%.3f)", kF[i], out02);
            CHECK(m.alpha == 0.0f && !m.init, "%d fps: gone, and wakes again when she comes back", kF[i]);
        }
    }
    // Item Shine (items.inc): lying ghosts let the shine through; standing ones, monsters and the rest still hide it
    {
        static const int kGhost[6] = { 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x11 };
        int okL = 1, c;
        for (i = 0; i < 6; ++i)
            for (c = 0; c < 0x14; ++c) {
                const int want = c == 9 || c == 0x0A || c == 0x0B;
                if (HbGhostLying(kGhost[i], c) != want) okL = 0;
            }
        CHECK(okL, "ghosts 0B-0F / 11: lying in class 9 / 0A / 0B only (not standing, staggering upright, rising 8, scripted)");
        for (i = 0; i < 0x14; ++i)
            if (!HbIsGhostType(i))
                for (c = 0; c < 0x14; ++c)
                    if (HbGhostLying(i, c)) okL = 0;
        CHECK(okL, "monsters (dead or down), Walter and the helpers never count as lying ghosts");
    }
    (void)pe; (void)sec;
}

// v3.32 MERGE: Both's small ring while Playable Eileen locks the companion's curse ([Ghost] LockCurse=1, curse held at 0)
static void TestLocked(void)
{
    static const int kF[4] = { 30, 60, 120, 180 };
    int i, hk, okG = 1;
    // when: the small ring drawn (Both + PE), she is present, PE's LockCurse=1 and her curse exactly 0
    CHECK(HbRing2Locked(1, 1, 1, 0.0f), "Both, present, LockCurse=1, curse 0: locked");
    CHECK(!HbRing2Locked(1, 1, 0, 0.0f), "LockCurse=0: normal even at curse 0");
    CHECK(!HbRing2Locked(1, 1, 1, 0.001f) && !HbRing2Locked(1, 1, 1, 0.5f), "a curse above 0 (not held by PE): normal");
    CHECK(!HbRing2Locked(0, 1, 1, 0.0f), "no small ring (Off / On / Companion / no PE): never locked");
    CHECK(!HbRing2Locked(1, 0, 1, 0.0f), "she is not present: not locked");
    {
        const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
        CHECK(!HbRing2Locked(1, 1, 1, nan), "NaN curse: not locked");
    }
    {   // the curse meter rows still say which rings: Both with PE has the small ring (the only place LOCKED can show)
        int cm, pe;
        for (cm = 0; cm < HB_CM_N; ++cm)
            for (pe = 0; pe < 2; ++pe) {
                int m, s;
                HbCurseRings(cm, pe, HB_PR_NONE, &m, &s);
                if (HbRing2Locked(s, 1, 1, 0.0f) != (cm == HB_CM_BOTH && pe)) okG = 0;
            }
        CHECK(okG, "LOCKED only in Both with Playable Eileen loaded");
    }
    // the grey eases in and out over HB_LOCK_S at every frame rate (the same value at the same moment)
    {
        float ref[3] = { -1, -1, -1 };
        for (i = 0; i < 4; ++i) {
            float k = 0.0f, at010 = -1, at030 = -1, back010 = -1;
            int f;
            for (f = 1; f <= 2 * kF[i]; ++f) {
                const float t = (float)f / (float)kF[i];
                k = HbLockStep(k, t <= 1.0f + 1e-6f, 1.0f / (float)kF[i]);
                if (fabsf(t - 0.10f) < 0.5f / kF[i]) at010 = k;
                if (fabsf(t - 0.30f) < 0.5f / kF[i]) at030 = k;
                if (fabsf(t - 1.10f) < 0.5f / kF[i]) back010 = k;
            }
            CHECK(Near(at010, 1.0f / 3.0f, 1e-3f) && Near(at030, 1.0f, 1e-4f), "%d fps: a third grey at 0.1 s, grey at 0.3 s (%.3f %.3f)", kF[i], at010, at030);
            CHECK(Near(back010, 2.0f / 3.0f, 1e-3f) && k == 0.0f, "%d fps: a third back 0.1 s after the unlock, then normal (%.3f %.3f)", kF[i], back010, k);
            if (ref[0] < 0.0f) ref[0] = at010, ref[1] = at030, ref[2] = back010;
            CHECK(Near(at010, ref[0], 1e-3f) && Near(at030, ref[1], 1e-3f) && Near(back010, ref[2], 1e-3f), "%d fps: same path as 30 fps", kF[i]);
        }
        CHECK(HbLockStep(1.0f, 1, 0.5f) == 1.0f && HbLockStep(0.0f, 0, 0.5f) == 0.0f, "held at its ends");
        CHECK(Near(HbLockStep(0.0f, 1, 5.0f), HB_DT_MAX / HB_LOCK_S, 1e-5f), "a long hitch counts as one 0.1 s step");
        {
            const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
            const float a = HbLockStep(nan, 1, 0.01f), b = HbLockStep(0.5f, 1, nan), c = HbLockStep(0.5f, 0, -1.0f);
            CHECK(a >= 0.0f && a <= 1.0f && b == 0.5f && c == 0.5f, "NaN / negative inputs: finite, no step (%f %f %f)", a, b, c);
        }
    }
    // the colours: k 0 = the colour itself; k 1 = a darker grey (r = g = b); the dim factor
    {
        static const unsigned int kCol[6] = { 0x7A5864u, 0xF06A8Cu, 0xC8A8B4u, 0xFF3040u, 0xCDBFC6u, 0x000000u };
        int ok0 = 1, ok1 = 1, okD = 1;
        for (i = 0; i < 6; ++i) {
            const unsigned int c = kCol[i], g = HbLockRgb(c, 1.0f), h = HbLockRgb(c, 0.5f);
            const float y = 0.299f * (c >> 16 & 0xFF) + 0.587f * (c >> 8 & 0xFF) + 0.114f * (c & 0xFF);
            if (HbLockRgb(c, 0.0f) != c) ok0 = 0;
            if ((g >> 16 & 0xFF) != (g & 0xFF) || (g >> 8 & 0xFF) != (g & 0xFF)) ok1 = 0;
            if (fabsf((float)(g & 0xFF) - y * (1.0f - HB_LOCK_DARK)) > 1.0f) ok1 = 0;
            if ((c >> 16 & 0xFF) != (c & 0xFF) && (h == c || h == g || (h >> 16 & 0xFF) == (h & 0xFF))) okD = 0;
        }
        CHECK(ok0, "k 0: every colour unchanged (normal ring = W3's drawing)");
        CHECK(ok1, "k 1: grey (r = g = b) at %.0f %% of the colour's brightness", 100.0f * (1.0f - HB_LOCK_DARK));
        CHECK(okD, "k 0.5: on the way (still tinted)");
        CHECK(HbLockAlpha(0.0f) == 1.0f && Near(HbLockAlpha(1.0f), 1.0f - HB_LOCK_DIM, 1e-6f), "strength x1 normal, x%.2f locked", 1.0f - HB_LOCK_DIM);
    }
    // the tag: L O C K E D in the stroke font, every stroke inside its cell (a negative slant stays inside too)
    CHECK(HbTinyWidth("LOCKED") == 25.0f, "LOCKED is 25 units wide (%.1f)", HbTinyWidth("LOCKED"));
    {
        const char *t = "LOCKEDIN";
        okG = 1;
        for (i = 0; t[i]; ++i) {
            const HbGlyphDef *g = HbGlyphOf(t[i]);
            int s;
            if (!g || g->n < 1 || g->n > 4) { okG = 0; continue; }
            for (s = 0; s < g->n; ++s) {
                const HbStroke *k = &g->s[s];
                const float lo = k->slant < 0.0f ? k->x0 + k->slant : k->x0, hi = k->slant > 0.0f ? k->x1 + k->slant : k->x1;
                if (!(k->x1 > k->x0) || !(k->y1 > k->y0) || lo < 0.0f || hi > (float)g->adv + 1e-4f || k->y0 < 0.0f || k->y1 > 5.0f) okG = 0;
            }
        }
        CHECK(okG, "C D K O (and E I L N): every stroke inside its letter's cell");
        CHECK(HbGlyphOf('O') != HbGlyphOf('D') && HbGlyphOf('C') && HbGlyphOf('K'), "the new letters exist");
    }
    // the layout: the tag under EILEEN ends above the main stage name and stays left of the main ring (with its thorns),
    // at every height and squeeze (as TestBoth's)
    for (hk = 0; hk < 8; ++hk) {
        static const float kH[8] = { 480, 600, 720, 768, 900, 1080, 1440, 2160 };
        const float h = kH[hk], sy = h / 448.0f, px = h / 1080.0f > 1.0f ? h / 1080.0f : 1.0f;
        float sr;
        for (sr = 1.09375f; sr < 2.0f; sr += 0.15f) {
            const float sx = sy * sr;
            const float labelBot = (HB_RING2_CY + HB_RING2_R + HB_TAG_GAP + HB_TAG_H) * sy;
            const float top = (HB_RING2_CY + HB_RING2_R + HB_TAG_GAP + HB_TAG_H + HB_LOCK_GAP) * sy;
            const float bot = top + HB_LOCK_H * sy + px;  // (+ its shadow)
            const float half = 0.5f * HbTinyWidth("LOCKED") * HB_LOCK_H / 5.0f * sy;
            const float mainL = 452.0f * sx - HbRingReach(17.0f * sy, px, 1.0f);
            CHECK(top > labelBot + px, "h %.0f sx/sy %.3f: LOCKED starts below EILEEN (%.1f > %.1f)", h, sr, top, labelBot);
            CHECK(bot < HB_LABEL_TOP * sy, "h %.0f: LOCKED ends at %.1f, above the stage name at %.1f", h, bot, HB_LABEL_TOP * sy);  // v3.35 CM335
            CHECK(HB_RING2_CX * sx + half < mainL - 2.0f, "h %.0f sx/sy %.3f: LOCKED stays left of the main ring (%.1f < %.1f)", h, sr,
                  HB_RING2_CX * sx + half, mainL);
            CHECK(half < 0.5f * HbTinyWidth("EILEEN") * HB_TAG_H / 5.0f * sy + 1e-3f, "h %.0f: the tag is no wider than EILEEN", h);
        }
    }
}

// v3.35 CM335: companion Eileen's curse target (0x43E790 in C), the next-spawn record, the left-alone rule, the notch
// timing, the glyph and the stroke-font stage name. The cases are computed by hand from curse_mechanics.md 2.5:
// A = (rooms - spawned + dangerTotal) / rooms, B = clamp(near / frames - 0.5, 0, 1), C = 0.05 min(collapses, 10) - 0.05,
// D = clamp(damage x 0.0003, 0, 0.2), E = clamp(flinches x 0.02, 0, 0.5), S = A - 0.2 B + C + D + E - 0.2,
// target = min(1.25 clamp(S, 0, 0.8), 0.95); 0 below 10 rooms.
static HbCurseRec Rec(int rooms, int spawned, int dangerTotal, int danger, int frames, int nearF, int coll, float dmg, int fl)
{
    HbCurseRec r;
    r.rooms = rooms, r.spawned = spawned, r.dangerTotal = dangerTotal, r.danger = danger, r.frames = frames;
    r.nearFrames = nearF, r.collapses = coll, r.damage = dmg, r.flinches = fl;
    return r;
}
static void TestTarget(void)
{
    static const struct { int rooms, spawned, dt, d, frames, nearF, coll; float dmg; int fl; float want; const char *what; } kC[] = {
        { 9, 0, 50, 2, 100, 0, 10, 9999.0f, 99, 0.0f, "fewer than 10 room exits: 0 whatever the rest" },
        { 10, 10, 0, 0, 1000, 1000, 0, 0.0f, 0, 0.0f, "always with the player, never hurt: A 0, S -0.35: 0" },
        { 20, 10, 6, 0, 36000, 18000, 2, 300.0f, 5, 0.95f, "A 0.8, B 0, C 0.05, D 0.09, E 0.1: S 0.84 -> 0.8 -> 1.0 -> 0.95" },
        { 40, 30, 4, 0, 10000, 8000, 1, 100.0f, 3, 0.225f, "A 0.35, B 0.3 (-0.06), C 0, D 0.03, E 0.06: S 0.18 -> 0.225" },
        { 15, 12, 2, 2, 9000, 4500, 0, 0.0f, 0, 0.104167f, "A 1/3, B 0, C -0.05: S 0.0833 -> 0.1042" },
        { 12, 20, 0, 0, 100, 100, 0, 0.0f, 0, 0.0f, "spawned > rooms (the old Cure Curse bug): A < 0: 0" },
        { 10, 10, 0, 0, 100, 0, 50, 1e6f, 100, 0.95f, "caps: C 0.45, D 0.2, E 0.5: S 0.95 -> 0.8 -> 0.95" },
        { 10, 10, 0, 0, 100, 0, 10, 0.0f, 0, 0.3125f, "10 collapses: C 0.45: S 0.25 -> 0.3125" },
        { 10, 10, 0, 0, 100, 0, 11, 0.0f, 0, 0.3125f, "11 collapses count as 10" },
        { 10, 0, 0, 0, 0, 5, 0, 0.0f, 0, 0.6875f, "frames 0, near 5 (x / 0 = inf): B 1: A 1 - 0.2 - 0.05 - 0.2 = 0.55 -> 0.6875" },
        { 20, 10, 0, 0, 100, 50, 3, 0.0f, 0, 0.5f, "A 0.5, C 0.1: S 0.4 -> 0.5" },
        { 20, 4, 6, 0, 100, 100, 0, 0.0f, 0, 0.9375f, "A 1.1, B 0.5 (-0.1), C -0.05: S 0.75 -> 0.9375" },
    };
    int i;
    for (i = 0; i < (int)(sizeof kC / sizeof kC[0]); ++i) {
        const HbCurseRec r = Rec(kC[i].rooms, kC[i].spawned, kC[i].dt, kC[i].d, kC[i].frames, kC[i].nearF, kC[i].coll, kC[i].dmg, kC[i].fl);
        const float t = HbCurseTarget(&r);
        CHECK(Near(t, kC[i].want, 1e-5f), "target %s: %.6f (want %.6f)", kC[i].what, t, kC[i].want);
    }
    {   // the game's NaN: 10+ rooms with frames 0 and near frames 0 (0 / 0); 9 rooms: still 0
        const HbCurseRec r = Rec(10, 0, 0, 0, 0, 0, 0, 0.0f, 0), r9 = Rec(9, 0, 0, 0, 0, 0, 0, 0.0f, 0);
        const float t = HbCurseTarget(&r);
        CHECK(t != t, "rooms 10, frames 0, near 0: NaN as the game's (%f)", t);
        CHECK(HbCurseTarget(&r9) == 0.0f, "rooms 9, frames 0: 0 (the early return comes first)");
    }
    {   // left behind with danger 2 (snowball): every door adds a room and the danger (spawned and the danger stay)
        HbCurseRec r = Rec(15, 12, 2, 2, 9000, 4500, 0, 0.0f, 0);
        HbCurseRec n = HbCurseAfterExit(r);
        CHECK(n.rooms == 16 && n.dangerTotal == 4 && n.spawned == 12 && n.danger == 2 && n.frames == 9000, "one more door: rooms 16, total 4");
        CHECK(Near(HbCurseTarget(&n), 0.3125f, 1e-5f), "next spawn: A (4 + 4) / 16 = 0.5: S 0.25 -> 0.3125 (%.6f)", HbCurseTarget(&n));
        n = HbCurseAfterExit(HbCurseAfterExit(n));
        CHECK(n.rooms == 18 && n.dangerTotal == 8, "three doors: rooms 18, total 8");
        CHECK(Near(HbCurseTarget(&n), 0.659722f, 1e-5f), "three doors: A 14 / 18: S 0.5278 -> 0.6597 (%.6f)", HbCurseTarget(&n));
        r = Rec(9, 3, 2, 1, 1000, 500, 0, 0.0f, 0);
        n = HbCurseAfterExit(r);
        CHECK(HbCurseTarget(&r) == 0.0f && Near(HbCurseTarget(&n), 0.9375f, 1e-5f), "9 rooms now: 0; the 10th door: A 1.0: 0.9375 (%.6f)",
              HbCurseTarget(&n));
        r = Rec(20, 10, 0, 0, 100, 50, 3, 0.0f, 0);
        n = HbCurseAfterExit(r);
        CHECK(n.dangerTotal == 0 && Near(HbCurseTarget(&n), 0.529762f, 1e-5f), "danger 0: the room still counts as away: A 11 / 21 (%.6f)",
              HbCurseTarget(&n));
        r = Rec(20, 4, 6, 0, 100, 100, 0, 0.0f, 0);
        n = HbCurseAfterExit(r);
        CHECK(Near(HbCurseTarget(&n), 0.931548f, 1e-5f) && HbCurseTarget(&n) < HbCurseTarget(&r),
              "a total above spawned: the next door lowers it (A 23 / 21: %.6f)", HbCurseTarget(&n));
        r = Rec(0xFFFF, 0, 0xFFFF, 2, 100, 100, 0, 0.0f, 0);
        n = HbCurseAfterExit(r);
        CHECK(n.rooms == 0 && n.dangerTotal == 1 && HbCurseTarget(&n) == 0.0f, "the words wrap as the game's (rooms 0: target 0)");
    }
    // the left-alone glyph: party mode 2, not with the player, a room with monsters; the danger it shows
    CHECK(HbLeftAlone(2, 1, 1) == 1 && HbLeftAlone(2, 1, 2) == 2 && HbLeftAlone(2, 1, 7) == 2, "left alone: danger 1 / 2 (a bad byte: 2)");
    CHECK(!HbLeftAlone(2, 1, 0) && !HbLeftAlone(2, 0, 2) && !HbLeftAlone(1, 1, 2) && !HbLeftAlone(0, 1, 2) && !HbLeftAlone(2, 1, -1),
          "not alone: no monsters, with the player (back in her room), in the party with him, not in the party");
    CHECK(HbShowNext(2, 0) && !HbShowNext(2, 1) && !HbShowNext(1, 0) && !HbShowNext(0, 0), "the next mark: left behind, no task only");
}
static void TestNotch(void)
{
    static const int kF[4] = { 30, 60, 120, 180 };
    const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
    int i;
    CHECK(HbNotchWant(0.3f, 0.3f) == 0.0f && HbNotchWant(0.306f, 0.3f) == 0.0f, "target = shown (within 0.006): hidden");
    CHECK(Near(HbNotchWant(0.311f, 0.3f), 0.5f, 1e-3f) && HbNotchWant(0.4f, 0.3f) == 1.0f && HbNotchWant(0.2f, 0.3f) == 1.0f,
          "half at 0.011, full from 0.016, either side");
    CHECK(HbNotchWant(nan, 0.3f) == 0.0f && HbNotchWant(-1.0f, 0.3f) == 0.0f && HbNotchWant(1.2f, 0.3f) == 0.0f && HbNotchWant(0.5f, nan) == 0.0f,
          "unknown / out of range: hidden");
    // a scripted visit at 30 / 60 / 120 / 180 fps: shown 0.30; target 0.30 (hidden) to 0.5 s, 0.50 (next 0.60, left alone
    // with danger 2 from 1.0 s) to 2.0 s, 0.80 to 3.0 s, then unknown. Events land at the start of the frame after them.
    for (i = 0; i < 4; ++i) {
        HbNotch n;
        int f;
        float s[12];
        memset(&n, 0, sizeof n);
        memset(s, 0, sizeof s);
        for (f = 1; f <= 4 * kF[i]; ++f) {
            const float t = (float)f / (float)kF[i], e = 1e-4f;
            const float target = t > 3.0f + e ? -1.0f : t > 2.0f + e ? 0.80f : t > 0.5f + e ? 0.50f : 0.30f;
            const int haveNext = t > 0.5f + e && t <= 2.0f + e, alone = t > 1.0f + e && t <= 2.0f + e ? 2 : 0;
            HbNotchStep(&n, target, 0.30f, 0.60f, haveNext, alone, 1.0f / (float)kF[i]);
#define AT(T, k, v) if (fabsf(t - (T)) < 0.5f / kF[i]) s[k] = (v)
            AT(0.4f, 0, n.a); AT(0.7f, 1, n.a); AT(0.7f, 2, n.pos); AT(0.9f, 3, n.a); AT(0.7f, 4, n.nextA);
            AT(1.3f, 5, n.alone); AT(1.6f, 6, n.alone); AT(2.2f, 7, n.pos); AT(2.6f, 8, n.pos); AT(3.3f, 9, n.a);
            AT(2.4f, 10, n.alone); AT(2.3f, 11, n.nextA);
#undef AT
        }
        CHECK(s[0] == 0.0f, "%d fps: target = shown: no notch (%.3f)", kF[i], s[0]);
        CHECK(Near(s[1], 0.5f, 1e-3f) && Near(s[2], 0.5f, 1e-5f) && Near(s[3], 1.0f, 1e-5f), "%d fps: fades in over 0.4 s at the target (%.3f %.3f %.3f)",
              kF[i], s[1], s[2], s[3]);
        CHECK(Near(s[4], 0.5f, 1e-3f), "%d fps: the next mark fades in with it (%.3f)", kF[i], s[4]);
        CHECK(Near(s[5], 0.6f, 1e-3f) && Near(s[6], 1.0f, 1e-5f) && Near(s[10], 0.5f, 1e-3f), "%d fps: the glyph in over 0.5 s, out over 0.8 s (%.3f %.3f %.3f)",
              kF[i], s[5], s[6], s[10]);
        CHECK(Near(s[7], 0.62f, 1e-3f) && Near(s[8], 0.80f, 1e-5f), "%d fps: a new target slides at 0.6 a second (%.3f %.3f)", kF[i], s[7], s[8]);
        CHECK(Near(s[9], 0.5f, 1e-3f), "%d fps: unknown target: fades out over 0.6 s (%.3f)", kF[i], s[9]);
        CHECK(Near(s[11], 0.5f, 1e-3f), "%d fps: back with the player: the next mark fades out (%.3f)", kF[i], s[11]);
        CHECK(n.a == 0.0f && n.nextA == 0.0f && n.alone == 0.0f && Near(n.pos, 0.8f, 1e-5f) && n.danger == 2,
              "%d fps: all gone at the end, the notch kept its place (%.3f), the glyph its danger", kF[i], n.pos);
    }
    {   // a hitch counts as 0.1 s; NaN / negative dt: no step; a hidden notch jumps to a new target
        HbNotch n;
        memset(&n, 0, sizeof n);
        HbNotchStep(&n, 0.9f, 0.1f, -1.0f, 0, 0, 5.0f);
        CHECK(Near(n.a, HB_DT_MAX / HB_NOTCH_IN_S, 1e-5f) && n.pos == 0.9f, "a 5 s hitch: one 0.1 s step (%.3f), at the target", n.a);
        HbNotchStep(&n, 0.2f, 0.1f, -1.0f, 0, 0, nan);
        HbNotchStep(&n, 0.2f, 0.1f, -1.0f, 0, 0, -1.0f);
        CHECK(Near(n.a, HB_DT_MAX / HB_NOTCH_IN_S, 1e-5f) && n.pos == 0.9f, "NaN / negative dt: nothing moves");
        HbNotchStep(&n, nan, 0.1f, nan, 1, 0, 0.05f);
        CHECK(n.pos == 0.9f && n.nextA == 0.0f && n.a < HB_DT_MAX / HB_NOTCH_IN_S, "a NaN target: the notch fades, keeps its place");
        memset(&n, 0, sizeof n);
        HbNotchStep(&n, 0.3f, 0.3f, 0.7f, 1, 0, 0.05f);
        CHECK(n.a == 0.0f && n.nextA > 0.0f && n.next == 0.7f, "target = shown but the next spawn differs: only the next mark");
    }
}
static void TestStageLabel(void)
{
    static const char *const kTag[4] = { "CLEAR", "HAUNTED", "CURSED", "POSSESSED" };  // v3.37 MN337: Untouched -> Clear
    static const float kH[8] = { 480, 600, 720, 768, 900, 1080, 1440, 2160 };
    int i, j, hk, okG = 1;
    for (i = 0; i < 4; ++i)
        for (j = 0; kTag[i][j]; ++j)
            if (!HbGlyphOf(kTag[i][j])) okG = 0;
    CHECK(okG, "every letter of the four stage names is in the stroke font");
    {
        const char *t = "UTHAPRS";
        okG = 1;
        for (i = 0; t[i]; ++i) {
            const HbGlyphDef *g = HbGlyphOf(t[i]);
            int s;
            if (!g || g->n < 1 || g->n > HB_GLYPH_MAXS) { okG = 0; continue; }
            for (s = 0; s < g->n; ++s) {
                const HbStroke *k = &g->s[s];
                const float lo = k->slant < 0.0f ? k->x0 + k->slant : k->x0, hi = k->slant > 0.0f ? k->x1 + k->slant : k->x1;
                if (!(k->x1 > k->x0) || !(k->y1 > k->y0) || lo < 0.0f || hi > (float)g->adv + 1e-4f || k->y0 < 0.0f || k->y1 > 5.0f) okG = 0;
            }
        }
        CHECK(okG, "U T H A P R S: every stroke inside its letter's cell");
    }
    CHECK(HbTinyWidth("UNTOUCHED") == 41.0f && HbTinyWidth("POSSESSED") == 38.0f, "widths %.0f / %.0f units",
          HbTinyWidth("UNTOUCHED"), HbTinyWidth("POSSESSED"));
    CHECK(HB_LABEL_H < 17.0f / 2.0f && HB_LABEL_GAP == HB_TAG_GAP, "compact (%.1f, the game's font about 17) and as close as EILEEN's", HB_LABEL_H);
    for (hk = 0; hk < 8; ++hk) {
        const float h = kH[hk], sy = h / 448.0f, px = h / 1080.0f > 1.0f ? h / 1080.0f : 1.0f;
        float sr;
        for (sr = 1.09375f; sr < 2.0f; sr += 0.15f) {
            const float sx = sy * sr, half = 0.5f * HbTinyWidth("UNTOUCHED") * HB_LABEL_H / 5.0f * sy;
            const float ringBot = (42.0f + 17.0f) * sy, top = HB_LABEL_TOP * sy;
            CHECK(top - ringBot > 2.0f * px && top - ringBot < 5.0f * sy, "h %.0f: the name starts %.1f px under the ring", h, top - ringBot);
            CHECK((452.0f - 256.0f) * sx + half < 256.0f * sx, "h %.0f sx/sy %.3f: the widest name stays inside the canvas", h, sr);
            {   // the glyph (figure and claws) left of each ring: clear of the ring and its thorns, above the labels
                static const float kR[2][3] = { { 452.0f, 42.0f, 17.0f }, { 414.0f, 36.0f, 10.5f } };  // main / small ring
                static const float kK[2] = { 1.0f, 0.62f };
                int r;
                for (r = 0; r < 2; ++r) {
                    const float cx = kR[r][0] * sx, cy = kR[r][1] * sy, R = kR[r][2] * sy, s = HbAloneSize(kR[r][2]) * sy;
                    const float gx = cx - R - HbAloneGap(kR[r][2]) * sy, w = 0.07f * s + 0.25f;
                    const float bx0 = gx - 0.62f * s - 2.0f * 0.34f * s - 0.20f * s - w, bx1 = gx + 0.51f * s;
                    const float by0 = cy - 0.82f * s, by1 = cy + 0.85f * s;
                    const float L = HbThornLen(R, px, kK[r]), base = R + 0.5f * px;
                    int m;
                    CHECK(bx1 < cx - R - 3.0f * kK[r] * px - px, "h %.0f ring %d: the glyph stays off the ring (%.1f < %.1f)", h, r, bx1, cx - R);
                    for (m = 0; m < 3; ++m) {  // the thorns at 0.25 / 0.5 / 0.7 (angle from 7:30 clockwise over 270 degrees)
                        static const float kM[3] = { 0.25f, 0.5f, 0.7f };
                        const float ang = -2.3561945f + 4.712389f * kM[m];
                        const float tx = cx + sinf(ang) * (base + L), ty = cy - cosf(ang) * (base + L);
                        const float dx = tx < bx0 ? bx0 - tx : tx > bx1 ? tx - bx1 : 0.0f, dy = ty < by0 ? by0 - ty : ty > by1 ? ty - by1 : 0.0f;
                        CHECK(dx > px || dy > px, "h %.0f ring %d: the thorn at %.2f stays off the glyph", h, r, kM[m]);
                    }
                    CHECK(by1 < (r ? HB_RING2_CY + HB_RING2_R + HB_TAG_GAP : HB_LABEL_TOP) * sy, "h %.0f ring %d: the glyph ends above the label", h, r);
                    CHECK(bx0 > 0.0f, "h %.0f ring %d: on screen", h, r);
                }
            }
        }
    }
}

// ---- v3.37 MN337: Curse Meter's Player / Both greyed and stepped over without Playable Eileen; PE's stage marks from
// FitsFrom / CursedIdleFrom whatever its switches; the first stage is Clear
static void TestCurse337(void)
{
    // the next value without PE (Off 0, Player 1, Companion 2, Both 3): Right / Left from every value
    static const int kRight[4] = { HB_CM_COMPANION, HB_CM_COMPANION, HB_CM_OFF, HB_CM_OFF };
    static const int kLeft[4] = { HB_CM_COMPANION, HB_CM_OFF, HB_CM_OFF, HB_CM_COMPANION };
    static const char *const kNm[4] = { "Off", "Player", "Companion", "Both" };
    float th[3];
    int v, pe, n, ok;
    CHECK(!HbCmGreyed(HB_CM_OFF, 0, 0) && HbCmGreyed(HB_CM_ON, 0, 0) && !HbCmGreyed(HB_CM_COMPANION, 0, 0) && HbCmGreyed(HB_CM_BOTH, 0, 0),
          "greyed without PE: Player and Both only");
    for (v = 0; v < HB_CM_N; ++v) CHECK(!HbCmGreyed(v, 1, 0), "nothing greyed with PE (%s)", kNm[v]);
    for (v = 0; v < HB_CM_N; ++v) {
        CHECK(HbCmStepFrom(v, 0, 0, 0) == kRight[v], "no PE: Right from %s -> %s (got %s)", kNm[v], kNm[kRight[v]], kNm[HbCmStepFrom(v, 0, 0, 0)]);
        CHECK(HbCmStepFrom(v, 1, 0, 0) == kLeft[v], "no PE: Left from %s -> %s (got %s)", kNm[v], kNm[kLeft[v]], kNm[HbCmStepFrom(v, 1, 0, 0)]);
        CHECK(HbCmStepFrom(v, 0, 1, 0) == (v + 1) % 4 && HbCmStepFrom(v, 1, 1, 0) == (v + 3) % 4, "with PE: every value in turn from %s", kNm[v]);
    }
    for (pe = 0, ok = 1; pe < 2; ++pe)  // any number of presses from any value never lands on a greyed one
        for (v = 0; v < HB_CM_N; ++v) {
            int c = v;
            for (n = 0; n < 40; ++n) {
                c = HbCmStepFrom(c, (n * 7 + v) % 3 == 0, pe, 0);
                if (HbCmGreyed(c, pe, 0) || c < 0 || c >= HB_CM_N) ok = 0;
            }
        }
    CHECK(ok, "40 presses either way from every value: never on a greyed value");
    CHECK(HbCmStepFrom(-3, 0, 0, 0) == HB_CM_COMPANION && HbCmStepFrom(9, 1, 0, 0) == HB_CM_COMPANION, "an out-of-range value steps as Off");
    {   // a saved Player / Both without PE draws what Companion draws (the companion's one ring), itself again with PE
        int m1, s1, m2, s2, m3, s3;
        HbCurseRings(HB_CM_ON, 0, HB_PR_NONE, &m1, &s1), HbCurseRings(HB_CM_BOTH, 0, HB_PR_NONE, &m2, &s2), HbCurseRings(HB_CM_COMPANION, 0, HB_PR_NONE, &m3, &s3);
        CHECK(m1 == m3 && s1 == s3 && m2 == m3 && s2 == s3, "no PE: Player and Both act as Companion");
        HbCurseRings(HB_CM_ON, 1, HB_PR_NONE, &m1, &s1), HbCurseRings(HB_CM_BOTH, 1, HB_PR_NONE, &m2, &s2);
        CHECK(m1 == 1 && !s1 && m2 == 1 && s2 == 1, "PE back: Player = PE's ring, Both = PE's + the small ring");
    }
    // PE's marks: Haunted from FitsFrom and Cursed from CursedIdleFrom (no switch can skip them any more)
    HbPeStageMarks(35, 50, th);
    CHECK(th[0] == 0.35f && th[1] == 0.50f && th[2] == 1.0f, "PE defaults: Haunted 35, Cursed 50, Possessed 100");
    CHECK(HbCurseStage(0.34f, th) == 0 && HbCurseStage(0.35f, th) == 1 && HbCurseStage(0.49f, th) == 1 && HbCurseStage(0.5f, th) == 2 &&
              HbCurseStage(0.99f, th) == 2 && HbCurseStage(1.0f, th) == 3,
          "PE meter: Clear < 35 <= Haunted < 50 <= Cursed < 100 = Possessed (the user's log: Fits=0 skipped Haunted in v3.36)");
    HbPeStageMarks(20, 70, th);
    CHECK(th[0] == 0.20f && th[1] == 0.70f && HbCurseStage(0.25f, th) == 1 && HbCurseStage(0.69f, th) == 1, "PE edited: 20 / 70");
    HbPeStageMarks(-5, 150, th);
    CHECK(th[0] == 0.0f && th[1] == 1.0f && th[2] == 1.0f, "PE marks clamped to 0 .. 100");
    {
        int i, okG = 1;
        const char *t = "CLEAR";
        for (i = 0; t[i]; ++i)
            if (!HbGlyphOf(t[i])) okG = 0;
        CHECK(okG && HbTinyWidth("CLEAR") > 0.0f && HbTinyWidth("CLEAR") < HbTinyWidth("POSSESSED"),
              "CLEAR: every letter in the stroke font, %.0f units (narrower than POSSESSED %.0f)", HbTinyWidth("CLEAR"), HbTinyWidth("POSSESSED"));
    }
}

// ---------------------------------------------------------------- v3.38 HB338: PLAYER, the alert floor, Hide Gauge
static void TestHud338(void)
{
    static const int kF[5] = { 30, 60, 120, 144, 240 };
    const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
    float ref[7] = { 0 };
    int i;
    {   // the font's Y and PLAYER
        const HbGlyphDef *y = HbGlyphOf('Y');
        CHECK(y && y->adv == 4 && y->n == 3 && HbTinyWidth("PLAYER") == 27.0f && HbTinyWidth("PLAER") == 22.0f &&
                  HbTinyWidth("EILEEN") < HbTinyWidth("PLAYER"),
              "PLAYER: the font has a Y (4 wide, 3 strokes); PLAYER is 27 units (%.0f)", HbTinyWidth("PLAYER"));
        CHECK(y && y->s[0].x1 + y->s[0].slant >= y->s[2].x0 && y->s[1].x0 + y->s[1].slant <= y->s[2].x1 && y->s[2].y0 == y->s[0].y1,
              "Y: both arms reach the stem where it starts");
    }
    {   // the label rows: PLAYER where the stage name was, the stage name under it (LOCKED's gap), clear of EILEEN / LOCKED
        const float pTop = HB_PLAYER_TOP, pBot = HB_PLAYER_TOP + HB_TAG_H, sTop = HbStageTop(1), sBot = sTop + HB_LABEL_H;
        const float eTop = HB_RING2_CY + HB_RING2_R + HB_TAG_GAP, lBot = eTop + HB_TAG_H + HB_LOCK_GAP + HB_LOCK_H;
        static const float kSq[3] = { 1.0f, 1.2f, 1.3333f };  // 4:3, 16:10, 16:9 (S): canvas x per canvas height = 448/512 / S x W/H ...
        int s;
        CHECK(HbStageTop(0) == HB_LABEL_TOP && pTop == HB_LABEL_TOP && sTop >= pBot + HB_LOCK_GAP - 1e-4f && Near(sBot, 74.0f, 0.01f),
              "rows: PLAYER %.1f..%.1f, the stage name %.1f..%.1f (Companion: %.1f)", pTop, pBot, sTop, sBot, HbStageTop(0));
        for (s = 0; s < 3; ++s) {  // in pixels of a 1440-high picture: x = (canvas x - 256) * W / (512 S), letters by canvas heights
            const float H = 1440.0f, W = H * 4.0f / 3.0f * kSq[s], sx = W / (512.0f * kSq[s]), sy = H / 448.0f;
            const float mainX = (452.0f - 256.0f) * sx, smallX = (HB_RING2_CX - 256.0f) * sx;
            const float wP = HbTinyWidth("POSSESSED") * HB_LABEL_H / 5.0f * sy, wE = HbTinyWidth("LOCKED") * HB_LOCK_H / 5.0f * sy;
            const float wEi = HbTinyWidth("EILEEN") * HB_TAG_H / 5.0f * sy, wPl = HbTinyWidth("PLAYER") * HB_TAG_H / 5.0f * sy;
            const float gap = (mainX - 0.5f * (wP > wPl ? wP : wPl)) - (smallX + 0.5f * (wEi > wE ? wEi : wE));
            CHECK(gap > 4.0f && mainX + 0.5f * wP < 0.5f * W - 1.0f, "S %.2f: the main ring's labels clear of EILEEN / LOCKED by %.0f px and "
                  "inside the picture (rows %.1f..%.1f vs %.1f..%.1f)", kSq[s], gap, pTop, sBot, eTop, lBot);
        }
    }
    {   // the alert floor: never under 0.85 of the ring's fade-in, never above 1, no away factor in it
        CHECK(Near(HbAlertQ(0.55f, 1.0f), 0.85f, 1e-6f) && Near(HbAlertQ(1.0f, 1.0f), 1.0f, 1e-6f) && Near(HbAlertQ(0.3f, 0.5f), 0.425f, 1e-6f) &&
                  HbAlertQ(0.0f, 0.0f) == 0.0f && HbAlertQ(2.0f, 2.0f) == 1.0f && HbAlertQ(nan, nan) == 0.0f,
              "HbAlertQ: max(strength, 0.85 x fade-in), clamped, NaN 0");
        CHECK(HB_AWAY_A == 0.70f && HB_REST2_A == 0.70f && HB_REST_A == 0.55f, "strengths: away 0.7, both rings rest 0.70 (v3.37 0.55 kept)");
    }
    {   // both rings rest at 0.70 after the wake (the user's choice; v3.37 0.55); the same wake curve
        HbMeter m;
        int f;
        memset(&m, 0, sizeof m);
        for (f = 0; f < 600; ++f) HbMeterStep(&m, 0.3f, 1, 1, 1.0f / 60.0f);
        CHECK(Near(HbMeterStrengthRest(&m, HB_REST2_A), 0.70f, 1e-5f) && Near(HbMeterStrength(&m), 0.55f, 1e-5f),
              "rest: 0.70 for both rings (v3.37's 0.55 still HbMeterStrength) (%.3f %.3f)", HbMeterStrengthRest(&m, HB_REST2_A),
              HbMeterStrength(&m));
        m.wake = 1.5f;
        CHECK(Near(HbMeterStrengthRest(&m, HB_REST2_A), 1.0f, 1e-5f), "rest: still 1 while awake");
    }
    {   // HbMeter.woke: the first frame, a stage crossing; not a still meter, a drift or a move (v3.38.1 HF3381)
        HbMeter m;
        int w0, w1, w2, w3, w4;
        memset(&m, 0, sizeof m);
        HbMeterStep(&m, 0.30f, 1, 1, 0.016f), w0 = m.woke;
        HbMeterStep(&m, 0.30f, 1, 1, 0.016f), w1 = m.woke;
        HbMeterStep(&m, 0.303f, 1, 1, 0.016f), w2 = m.woke;
        HbMeterStep(&m, 0.31f, 1, 1, 0.016f), w3 = m.woke;
        HbMeterStep(&m, 0.51f, 2, 1, 0.016f), w4 = m.woke;
        CHECK(w0 && !w1 && !w2 && !w3 && w4, "woke: appear %d, still %d, drift 0.003 %d, move 0.01 %d (none since v3.38.1), stage %d", w0, w1, w2, w3, w4);
        HbMeterStep(&m, 0.51f, 2, 0, 0.016f);
        CHECK(!m.woke, "woke: not while it fades away");
    }
    {   // HbNotch.woke: a notch / next mark appearing or moving, the glyph appearing or its danger changing; not going away
        HbNotch n;
        int f, w[8];
        memset(&n, 0, sizeof n);
        memset(w, 0, sizeof w);
        HbNotchStep(&n, 0.30f, 0.30f, -1.0f, 0, 0, 0.016f), w[0] = n.woke;                // at the target: nothing
        HbNotchStep(&n, 0.50f, 0.30f, -1.0f, 0, 0, 0.016f), w[1] = n.woke;                // the notch appears
        HbNotchStep(&n, 0.50f, 0.30f, -1.0f, 0, 0, 0.016f), w[2] = n.woke;                // still
        for (f = 0; f < 60; ++f) {                                                          // slides to 0.6: no wake (v3.38.1)
            HbNotchStep(&n, 0.60f, 0.30f, -1.0f, 0, 0, 0.016f);
            w[3] |= n.woke & HB_NW_NOTCH;
        }
        HbNotchStep(&n, 0.60f, 0.30f, 0.70f, 1, 0, 0.016f), w[4] = n.woke;                // the next mark appears
        HbNotchStep(&n, 0.60f, 0.30f, 0.70f, 1, 1, 0.016f), w[5] = n.woke;                // the glyph appears (danger 1)
        HbNotchStep(&n, 0.60f, 0.30f, 0.70f, 1, 2, 0.016f), w[6] = n.woke;                // its danger changes
        HbNotchStep(&n, 0.60f, 0.30f, 0.70f, 1, 0, 0.016f), w[7] = n.woke;                // it goes: no wake
        CHECK(!w[0] && w[1] == HB_NW_NOTCH && !w[2] && !w[3] && w[4] == HB_NW_NEXT && w[5] == HB_NW_ALONE && w[6] == HB_NW_ALONE && !w[7],
              "notch wakes: %d %d %d %d %d %d %d %d", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    }
    // Hide Gauge at 30 / 60 / 120 / 144 / 240 fps: on, the gauge faded (0), one wake at 0: shown to 5 s, half at 5.5 s, gone
    // at 6 s; a wake at 7 s: back at once; Possessed from 8 to 10 s holds it, then its 5 s + 1 s: half at 15.5, gone at 16
    for (i = 0; i < 5; ++i) {
        HbHide hd;
        float s[7] = { 0 }, k = 0.0f;
        int f;
        memset(&hd, 0, sizeof hd);
        for (f = 0; f <= 18 * kF[i]; ++f) {  // (the first wake at t = 0)
            const float t = (float)f / (float)kF[i], e = 1e-4f;
            const int wake = f == 0 || fabsf(t - 7.0f) < 0.5f / kF[i], hold = t > 8.0f + e && t <= 10.0f + e;
            k = HbHideStep(&hd, 1, wake, hold, 0.0f, 1.0f / (float)kF[i]);
#define AT(T, j) if (fabsf(t - (T)) < 0.5f / kF[i]) s[j] = k
            AT(4.9f, 0); AT(5.5f, 1); AT(6.2f, 2); AT(7.0f, 3); AT(10.0f, 4); AT(14.9f, 5); AT(15.5f, 6);
#undef AT
        }
        CHECK(s[0] == 1.0f && Near(s[1], 0.5f, 0.02f) && s[2] == 0.0f && s[3] == 1.0f && s[4] == 1.0f && s[5] == 1.0f &&
                  Near(s[6], 0.5f, 0.02f) && k == 0.0f,
              "%d fps: hold 5 s (%.2f), half at 5.5 (%.3f), gone at 6.2 (%.2f), woken at 7 (%.2f), Possessed holds (%.2f), "
              "5 s after it (%.2f), half at 15.5 (%.3f), gone at 18 (%.2f)", kF[i], s[0], s[1], s[2], s[3], s[4], s[5], s[6], k);
        if (i == 0) memcpy(ref, s, sizeof ref);
        CHECK(Near(s[1], ref[1], 0.02f) && Near(s[6], ref[6], 0.02f), "%d fps: the fades land at the same moments as at 30 fps", kF[i]);
    }
    {   // the game's gauge: k follows its fade / 0.7 when that is higher; Hide Gauge off: 1, and on again it starts shown
        HbHide hd;
        memset(&hd, 0, sizeof hd);
        HbHideSleep(&hd);
        CHECK(HbHideStep(&hd, 1, 0, 0, 0.0f, 0.016f) == 0.0f && hd.asleep, "sleep (the HUD was hidden): 0 at once");
        CHECK(Near(HbHideStep(&hd, 1, 0, 0, 0.35f, 0.016f), 0.5f, 1e-5f) && !hd.asleep, "the gauge at 0.35: k 0.5");
        CHECK(HbHideStep(&hd, 1, 0, 0, 0.7f, 0.016f) == 1.0f && HbHideStep(&hd, 1, 0, 0, 0.9f, 0.016f) == 1.0f, "the gauge full: 1");
        CHECK(HbHideStep(&hd, 1, 0, 0, nan, 0.016f) == 0.0f, "a NaN fade: the own timer only (asleep: 0)");
        CHECK(HbHideStep(&hd, 1, 0, 1, 0.0f, 0.016f) == 1.0f, "Possessed after a sleep: up at once (the gauge's low-HP rule)");
        HbHideSleep(&hd);
        CHECK(HbHideStep(&hd, 0, 0, 0, 0.0f, 0.016f) == 1.0f && hd.idle == 0.0f, "Hide Gauge off: always 1");
        CHECK(HbHideStep(&hd, 1, 0, 0, 0.0f, 0.016f) == 1.0f, "turned on again: shown, its 5 s start");
        CHECK(HbHideStep(&hd, 1, 0, 0, 0.0f, 50.0f) == 1.0f && Near(hd.idle, 0.016f + HB_DT_MAX, 1e-5f), "a hitch counts 0.1 s");
        hd.idle = nan;
        CHECK(HbHideStep(&hd, 1, 0, 0, 0.0f, 0.016f) == 1.0f && hd.idle >= 0.0f, "a NaN timer restarts shown");
        CHECK(HbGameK(0.7f) == 1.0f && HbGameK(0.0f) == 0.0f && HbOwnK(nan) == 0.0f && HbOwnK(5.0f) == 1.0f && HbOwnK(6.0f) == 0.0f,
              "HbGameK / HbOwnK ends");
    }
}

// ---- v3.41 CM341: Playable Eileen v1.23.1+ draws the player's curse ring (SH4PE_DrawsPlayerCurse() = 1): which rings
// Refined still draws, PE's ring state, the Options row (grey, step, description), the small ring beside PE's ring
static void TestCurse341(void)
{
    static const char *const kNm[4] = { "Off", "Player", "Companion", "Both" };
    static const char *const kPr[3] = { "no export", "PE owns, hidden", "PE owns, shown" };
    int cm, pe, pr, v, n, ok, owns;
    // 1. every mode x PE loaded x owner state
    for (pr = HB_PR_NONE; pr <= HB_PR_SHOWN; ++pr)
        for (pe = 0; pe < 2; ++pe)
            for (cm = -1; cm <= HB_CM_N; ++cm) {
                int m, s, r, wantR, wantM, wantS;
                const int in = cm > HB_CM_OFF && cm < HB_CM_N;
                if (pr == HB_PR_NONE) {  // exactly the v3.40 rule
                    wantR = in, wantM = in && (cm == HB_CM_ON || cm == HB_CM_BOTH) && pe, wantS = in && cm == HB_CM_BOTH && pe;
                } else {
                    wantM = 0;
                    wantR = cm == HB_CM_COMPANION && pr == HB_PR_HIDDEN;
                    wantS = cm == HB_CM_BOTH || (cm == HB_CM_COMPANION && pr == HB_PR_SHOWN);
                }
                r = HbCurseRings(cm, pe, pr, &m, &s);
                CHECK(r == wantR && m == wantM && s == wantS, "%s, pe %d, cm %d (%s): main %d PE %d small %d (want %d %d %d)", kPr[pr], pe,
                      cm, in ? kNm[cm] : "out of range", r, m, s, wantR, wantM, wantS);
            }
    for (pr = HB_PR_HIDDEN, ok = 1; pr <= HB_PR_SHOWN; ++pr)  // PE owning: never PE's curse here; Player nothing at all
        for (pe = 0; pe < 2; ++pe)
            for (cm = 0; cm < HB_CM_N; ++cm) {
                int m, s, r = HbCurseRings(cm, pe, pr, &m, &s);
                if (m || (cm == HB_CM_ON && (r || s)) || (pr == HB_PR_SHOWN && r)) ok = 0;
            }
    CHECK(ok, "PE owns the ring: never a PE meter from Refined, Player draws nothing, no main ring while PE's can show");
    {   // Both owned = companion only = Companion with PE's ring showing
        int m1, s1, m2, s2, r1, r2;
        r1 = HbCurseRings(HB_CM_BOTH, 1, HB_PR_SHOWN, &m1, &s1), r2 = HbCurseRings(HB_CM_COMPANION, 1, HB_PR_SHOWN, &m2, &s2);
        CHECK(r1 == r2 && m1 == m2 && s1 == s2 && s1 == 1, "PE's ring shows: Both = Companion = her small ring only");
        r1 = HbCurseRings(HB_CM_COMPANION, 1, HB_PR_HIDDEN, &m1, &s1), r2 = HbCurseRings(HB_CM_COMPANION, 1, HB_PR_NONE, &m2, &s2);
        CHECK(r1 == r2 && m1 == m2 && s1 == s2, "PE's ring cannot show: Companion exactly as before (her main ring)");
    }
    // 2. the owner state from the exports
    for (n = 0, ok = 1; n < 64; ++n)  // without the function: always as before, whatever the struct says
        if (HbPeRingState(0, n & 1, (unsigned)(n >> 1 & 3), (n >> 3) & 3, (n >> 5) & 1, n & 1) != HB_PR_NONE) ok = 0;
    CHECK(ok, "no SH4PE_DrawsPlayerCurse (or not 1): HB_PR_NONE in every case");
    CHECK(HbPeRingState(1, 0, 0, 0, 0, 1) == HB_PR_SHOWN, "owns, no usable SH4PE_Curse: as if its ring shows");
    CHECK(HbPeRingState(1, 1, 1, 0, 0, 1) == HB_PR_SHOWN, "owns, SH4PE_Curse version 1: as if its ring shows (no ring fields)");
    CHECK(HbPeRingState(1, 0, 0, 0, 0, 0) == HB_PR_HIDDEN && HbPeRingState(1, 1, 1, 0, 0, 0) == HB_PR_HIDDEN,
          "REV341 CM341-001: PE Off: hidden whatever SH4PE_Curse says");
    CHECK(HbPeRingState(1, 1, 2, 0, 0, 1) == HB_PR_HIDDEN, "v2, [Curse] Meter=0: cannot show");
    CHECK(HbPeRingState(1, 1, 2, 1, 0, 1) == HB_PR_SHOWN && HbPeRingState(1, 1, 2, 2, 0, 1) == HB_PR_SHOWN,
          "v2, Meter 1 / 2 with PE On: can show (also while faded out: the layout does not jump)");
    CHECK(HbPeRingState(1, 1, 2, 1, 0, 0) == HB_PR_HIDDEN, "v2, PE Off: cannot show");
    CHECK(HbPeRingState(1, 1, 2, 0, 1, 1) == HB_PR_SHOWN && HbPeRingState(1, 1, 3, 0, 1, 1) == HB_PR_SHOWN,
          "drawn last frame: shown whatever the mode says (and a later version)");
    CHECK(HbPeRingState(1, 1, 2, 1, 1, 0) == HB_PR_HIDDEN && HbPeRingState(1, 1, 2, 2, 1, 0) == HB_PR_HIDDEN,
          "REV341 CM341-001: a stale ringShown after PE was switched Off: hidden (Companion takes the main ring)");
    // 3. the Options row: without ownership exactly as v3.37 (both loaded states)
    for (pe = 0, ok = 1; pe < 2; ++pe)
        for (v = 0; v < HB_CM_N; ++v) {
            const int old = !pe && (v == HB_CM_ON || v == HB_CM_BOTH);
            if (HbCmGreyed(v, pe, 0) != old) ok = 0;
            if (HbCmDescKind(v, pe, 0) != (old ? HB_CD_NOPE : HB_CD_OWN)) ok = 0;
        }
    CHECK(ok, "no ownership: grey and descriptions as v3.37");
    for (v = 0; v < HB_CM_N; ++v) {
        CHECK(HbCmGreyed(v, 1, 1) == (v == HB_CM_ON), "PE owns: only Player grey (%s)", kNm[v]);
        CHECK(HbCmGreyed(v, 0, 1) == HbCmGreyed(v, 0, 0), "PE not loaded: the ownership flag changes nothing (%s)", kNm[v]);
    }
    CHECK(HbCmDescKind(HB_CM_OFF, 1, 1) == HB_CD_OWN && HbCmDescKind(HB_CM_ON, 1, 1) == HB_CD_PE_PLAYER &&
              HbCmDescKind(HB_CM_COMPANION, 1, 1) == HB_CD_OWN && HbCmDescKind(HB_CM_BOTH, 1, 1) == HB_CD_PE_BOTH,
          "PE owns: Player and Both re-described, Off / Companion their own texts");
    {   // PE owns: Off -> Companion -> Both -> Off (Right), the reverse (Left); a saved Player steps off it
        static const int kRight[4] = { HB_CM_COMPANION, HB_CM_COMPANION, HB_CM_BOTH, HB_CM_OFF };
        static const int kLeft[4] = { HB_CM_BOTH, HB_CM_OFF, HB_CM_OFF, HB_CM_COMPANION };
        for (v = 0; v < HB_CM_N; ++v) {
            CHECK(HbCmStepFrom(v, 0, 1, 1) == kRight[v], "PE owns: Right from %s -> %s (got %s)", kNm[v], kNm[kRight[v]],
                  kNm[HbCmStepFrom(v, 0, 1, 1)]);
            CHECK(HbCmStepFrom(v, 1, 1, 1) == kLeft[v], "PE owns: Left from %s -> %s (got %s)", kNm[v], kNm[kLeft[v]],
                  kNm[HbCmStepFrom(v, 1, 1, 1)]);
        }
    }
    for (pe = 0, ok = 1; pe < 2; ++pe)
        for (owns = 0; owns < 2; ++owns)
            for (v = 0; v < HB_CM_N; ++v) {
                int c = v;
                for (n = 0; n < 40; ++n) {
                    c = HbCmStepFrom(c, (n * 5 + v) % 3 == 0, pe, owns);
                    if (HbCmGreyed(c, pe, owns) || c < 0 || c >= HB_CM_N) ok = 0;
                }
            }
    CHECK(ok, "40 presses either way, every loaded / owner state: never on a greyed value");
    // 4. the small ring beside PE's ring
    {
        float x, y;
        int r;
        r = HbRing2Place(0, 300.0f, 100.0f, 30.0f, &x, &y);
        CHECK(r == 0 && x == HB_RING2_CX && y == HB_RING2_CY, "not followed (SH4PE_Curse below version 2): its own place");
        r = HbRing2Place(1, 452.0f, 42.0f, 17.0f, &x, &y);
        CHECK(r == 1 && Near(x, 414.0f, 1e-3f) && Near(y, 36.0f, 1e-3f), "PE's default ring: the usual place 414, 36 (%.4f %.4f)", x, y);
        r = HbRing2Place(1, 452.0f, 42.0f, 34.0f, &x, &y);
        CHECK(r == 1 && Near(x, 452.0f - 38.0f * 2.0f, 1e-3f) && Near(y, 42.0f - 6.0f * 2.0f, 1e-3f),
              "MeterSize 200: the contract's formula (%.3f %.3f)", x, y);
        r = HbRing2Place(1, 300.0f, 200.0f, 25.5f, &x, &y);
        CHECK(r == 1 && Near(x, 300.0f - 38.0f * 1.5f, 1e-3f) && Near(y, 200.0f - 6.0f * 1.5f, 1e-3f), "moved and 150 %%: the formula");
        for (n = 1, ok = 1; n <= 120; ++n) {  // never touching: the gap between the rings at least the default one
            const float pr = (float)n;
            float dx, dy;
            HbRing2Place(1, 300.0f, 200.0f, pr, &x, &y);
            dx = x - 300.0f, dy = y - 200.0f;
            if (sqrtf(dx * dx + dy * dy) - pr - HB_RING2_R < HB_RING2_DIST - 17.0f - HB_RING2_R - 1e-3f) ok = 0;
            if (n >= 17 && (!Near(x, 300.0f - 38.0f * pr / 17.0f, 2e-3f) || !Near(y, 200.0f - 6.0f * pr / 17.0f, 2e-3f))) ok = 0;
        }
        CHECK(ok, "R 1..120: the default gap at least; the contract's formula from R 17 up");
        r = HbRing2Place(1, 20.0f, 42.0f, 17.0f, &x, &y);
        CHECK(r == 2 && Near(x, 58.0f, 1e-3f) && Near(y, 36.0f, 1e-3f), "PE's ring at the left edge: the small ring right of it (%.2f)", x);
        r = HbRing2Place(1, 452.0f, 4.0f, 17.0f, &x, &y);
        CHECK(r == 1 && y == HB_RING2_R, "never above the canvas top (%.2f)", y);
        {
            const float nan = (float)(0.0 * (1.0 / 0.0 - 1.0 / 0.0));
            CHECK(HbRing2Place(1, nan, 42.0f, 17.0f, &x, &y) == 0 && x == HB_RING2_CX && y == HB_RING2_CY, "NaN x: its own place");
            CHECK(HbRing2Place(1, 452.0f, 42.0f, nan, &x, &y) == 0, "NaN R: its own place");
            CHECK(HbRing2Place(1, 600.0f, 42.0f, 17.0f, &x, &y) == 0 && HbRing2Place(1, 452.0f, -1.0f, 17.0f, &x, &y) == 0 &&
                      HbRing2Place(1, 452.0f, 42.0f, 0.5f, &x, &y) == 0 && HbRing2Place(1, 452.0f, 42.0f, 500.0f, &x, &y) == 0,
                  "off the canvas / a silly radius: its own place");
        }
    }
}

// v3.42 SM342: Curse Meter Off / On (companion Eileen only). The ini's old values, the new names older builds read, and
// the rings: never the player's ring (no PE meter, no PLAYER ring) for any value, PE version or PE state; On = her main
// ring while PE's ring cannot show (or no PE owner), her small ring beside PE's while it can.
static void TestCurse342(void)
{
    static const struct { const char *s; int v, legacy; } kIni[] = {
        { "Off", HB_CMV_OFF, 0 }, { "off", HB_CMV_OFF, 0 }, { "On", HB_CMV_OFF, 1 }, { "ON", HB_CMV_OFF, 1 },
        { "Player", HB_CMV_OFF, 1 }, { "Companion", HB_CMV_ON, 0 }, { "companion", HB_CMV_ON, 0 }, { "Both", HB_CMV_ON, 1 },
        { "BOTH", HB_CMV_ON, 1 }, { "", HB_CMV_OFF, 0 }, { "Yes", HB_CMV_OFF, 0 }, { "Companions", HB_CMV_OFF, 0 },
        { "Bot", HB_CMV_OFF, 0 }, { "1", HB_CMV_OFF, 0 } };
    static const char *const kPr[3] = { "no export", "PE owns, hidden", "PE owns, shown" };
    int i, v, pe, pr, legacy;
    for (i = 0; i < (int)(sizeof kIni / sizeof kIni[0]); ++i) {
        legacy = -1;
        v = HbCmvParse(kIni[i].s, &legacy);
        CHECK(v == kIni[i].v && legacy == kIni[i].legacy, "ini '%s' -> %d legacy %d (want %d %d)", kIni[i].s, v, legacy, kIni[i].v,
              kIni[i].legacy);
    }
    CHECK(HbCmvParse(NULL, &legacy) == HB_CMV_OFF && legacy == 0 && HbCmvParse("Both", NULL) == HB_CMV_ON, "NULL text / NULL legacy");
    // what the ini is written with reads back as the same value, here and in every v3.32 .. v3.41 build (their names)
    for (v = 0; v < HB_CMV_N; ++v) {
        CHECK(HbCmvParse(kHbCmvIni[v], &legacy) == v && !legacy, "'%s' reads back as %s", kHbCmvIni[v], kHbCmvShown[v]);
        CHECK(HbCmvMode(v) == (v ? HB_CM_COMPANION : HB_CM_OFF), "row %d -> the v3.41 mode %d", v, HbCmvMode(v));
    }
    CHECK(!strcmp(kHbCmvIni[HB_CMV_ON], "Companion") && !strcmp(kHbCmvIni[HB_CMV_OFF], "Off") && !strcmp(kHbCmvShown[HB_CMV_ON], "On"),
          "names: On written as Companion (older builds: companion Eileen's ring), Off as Off");
    CHECK(HB_CMV_N == 2, "two values: Off / On");
    // the rings
    for (pr = HB_PR_NONE; pr <= HB_PR_SHOWN; ++pr)
        for (pe = 0; pe < 2; ++pe)
            for (v = 0; v < HB_CMV_N; ++v) {
                int m = -1, s = -1, r = HbCurseRings(HbCmvMode(v), pe, pr, &m, &s);
                const int wantR = v == HB_CMV_ON && pr != HB_PR_SHOWN, wantS = v == HB_CMV_ON && pr == HB_PR_SHOWN;
                CHECK(m == 0, "%s, pe %d, %s: never the player's ring (PE meter %d)", kPr[pr], pe, kHbCmvShown[v], m);
                CHECK(r == wantR && s == wantS, "%s, pe %d, %s: main %d small %d (want %d %d)", kPr[pr], pe, kHbCmvShown[v], r, s, wantR, wantS);
                CHECK(!(r && s), "never both rings of hers at once");
            }
    // the old values: what v3.41 drew with Playable Eileen v1.23.1 owning the ring (peRing HIDDEN / SHOWN) and now
    for (pr = HB_PR_HIDDEN; pr <= HB_PR_SHOWN; ++pr) {
        static const char *const kOld[3] = { "On", "Player", "Companion" };
        for (i = 0; i < 3; ++i) {  // On / Player: v3.41 drew nothing; Companion: the same rings
            int m0, s0, m1, s1, r0, r1;
            const int old = i < 2 ? HB_CM_ON : HB_CM_COMPANION;
            r0 = HbCurseRings(old, 1, pr, &m0, &s0);
            r1 = HbCurseRings(HbCmvMode(HbCmvParse(kOld[i], NULL)), 1, pr, &m1, &s1);
            CHECK(r0 == r1 && m0 == m1 && s0 == s1, "%s, old '%s': as v3.41 drew it (main %d small %d)", kPr[pr], kOld[i], r1, s1);
        }
        {   // Both: v3.41 the small ring always; now her small ring while PE's ring can show, her main ring while it cannot
            int m1, s1, r1 = HbCurseRings(HbCmvMode(HbCmvParse("Both", NULL)), 1, pr, &m1, &s1);
            CHECK(pr == HB_PR_SHOWN ? (s1 && !r1) : (r1 && !s1), "%s, old 'Both': %s", kPr[pr], pr == HB_PR_SHOWN ? "the small ring" : "her main ring");
        }
    }
}

// v3.52 HB352 (D1, D2): which enemy may have a bar, the boss's want, the bar's place, the refusal count
static void TestRefuse352(void)
{
    static const int kFps[] = { 30, 60, 144, 240 };
    int s, i, k;
    for (s = 0; s <= 0x20; ++s) {  // EnemyIsActive 0x45CA40: 1..0x0C or 0x12
        const int active = s > 0 && (s <= 0x0C || s == 0x12);
        CHECK(HbStateDrawn(s) == active, "state %X drawn by the game: %d", s, active);
        CHECK(HbStateHidden(s) == (s == 0 || s == 0x0D || s == 0x0E || s == 0x0F), "state %X hidden (no bar at all)", s);
    }
    {
        const float her[3] = { -1590.0f, -160.0f, 1248.0f }, origin[3] = { 0.0f, 0.0f, 0.0f }, nearO[3] = { 0.4f, -0.3f, 0.2f };
        const float parked[3] = { 120.0f, 70000.0f, -300.0f }, far_[3] = { 2e6f, 0.0f, 0.0f }, deep[3] = { 300.0f, -30000.0f, 9000.0f };
        float nan_[3];
        const unsigned qnan = 0x7FC00000u;
        nan_[0] = 1.0f, nan_[2] = 0.0f;
        memcpy(&nan_[1], &qnan, sizeof nan_[1]);  // NaN
        CHECK(HbRefuseWhy(0x0D, her) == HB_RF_HIDDEN, "an arriving ghost (state 0D) at her position: no bar");
        CHECK(HbRefuseWhy(0x0E, her) == HB_RF_HIDDEN, "an arriving ghost with no wall spot (state 0E): no bar");
        CHECK(HbRefuseWhy(0x0F, her) == HB_RF_HIDDEN, "a follower placeholder (state 0F): no bar");
        CHECK(HbRefuseWhy(0x00, her) == HB_RF_HIDDEN, "a task not set up (state 0): no bar");
        CHECK(HbRefuseWhy(0x02, her) == HB_RF_NONE && HbRefuseWhy(0x0A, her) == HB_RF_NONE && HbRefuseWhy(0x12, her) == HB_RF_NONE,
              "standing / down / the 0x12 state at a place: may have a bar (0x12: v3.29's fade-out)");
        CHECK(HbRefuseWhy(0x10, her) == HB_RF_NONE && HbRefuseWhy(0x11, her) == HB_RF_NONE, "0x10+: not refused (they fade out as before)");
        CHECK(HbRefuseWhy(0x03, origin) == HB_RF_PLACE && HbRefuseWhy(0x03, nearO) == HB_RF_PLACE, "at the origin: no place in the room");
        CHECK(HbRefuseWhy(0x03, parked) == HB_RF_PLACE, "parked at y 70000 (0x4AFA90's hidden spot): no place");
        CHECK(HbRefuseWhy(0x03, far_) == HB_RF_PLACE && HbRefuseWhy(0x03, nan_) == HB_RF_PLACE, "far away / not a number: no place");
        CHECK(HbRefuseWhy(0x03, deep) == HB_RF_NONE, "a tall room's deep floor (y -30000) is a place");
        CHECK(HbRefuseWhy(0x0D, origin) == HB_RF_HIDDEN, "hidden comes first");
    }
    // the boss's want: any distance, both modes; never with the bars Off; the others as HbWant
    CHECK(HbWantBoss(HB_MODE_NEAR, 1, 0, 1e6f, 2500.0f) == 1.0f, "a boss 2500 away in All Near: wanted (v3.51: 0)");
    CHECK(HbWant(HB_MODE_NEAR, 0, 1e6f, 2500.0f) == 0.0f, "(v3.51's rule: nothing at 2500)");
    CHECK(HbWantBoss(HB_MODE_LOCKED, 1, 0, 1e6f, 300.0f) == 1.0f, "a boss in Locked Target, not locked: wanted");
    CHECK(HbWantBoss(HB_MODE_OFF, 1, 1, 0.0f, 100.0f) == 0.0f, "bars Off: no boss bar");
    for (i = 0; i <= 20; ++i) {
        const float d = 100.0f * (float)i;
        CHECK(HbWantBoss(HB_MODE_NEAR, 0, 0, 1e6f, d) == HbWant(HB_MODE_NEAR, 0, 1e6f, d) &&
              HbWantBoss(HB_MODE_LOCKED, 0, i & 1, (float)i, d) == HbWant(HB_MODE_LOCKED, i & 1, (float)i, d), "not a boss: v3.51's want at %.0f", d);
    }
    {   // the bar's place in a 0,0 2560x1440 view
        float x = -1.0f, y = -1.0f;
        CHECK(HbBarPlace(1, 900.0f, 300.0f, 1, 900.0f, 700.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y) && x == 900.0f && y == 300.0f,
              "the top in the view: the top (%.0f, %.0f)", x, y);
        CHECK(HbBarPlace(1, 900.0f, -150.0f, 1, 905.0f, 500.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y) && x == 900.0f && y == 0.0f,
              "the top above the view, the middle in it: the top edge (%.0f, %.0f)", x, y);
        CHECK(HbBarPlace(0, 0.0f, 0.0f, 1, 905.0f, 500.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y) && x == 905.0f && y == 0.0f,
              "the top behind the near plane, the middle in the view: the middle's x at the top edge");
        CHECK(!HbBarPlace(1, 900.0f, -150.0f, 1, 905.0f, -20.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y), "top and middle above: none");
        CHECK(!HbBarPlace(1, 3000.0f, 300.0f, 1, 3000.0f, 600.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y), "right of the view: none");
        CHECK(!HbBarPlace(1, 900.0f, 1500.0f, 1, 900.0f, 1300.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y),
              "the top below the bottom + 40: none (v3.51's rule)");
        CHECK(HbBarPlace(1, -30.0f, 1470.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y), "v3.51's 40 px margins kept");
        CHECK(!HbBarPlace(0, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 2560.0f, 1440.0f, &x, &y), "no projection: none");
    }
    {   // the count: a being once a reason a room visit
        HbRefCount c;
        int said1 = 0, said2 = 0;
        memset(&c, 0, sizeof c);
        CHECK(HbRefNote(&c, &said1, HB_RF_HIDDEN) == 1 && HbRefNote(&c, &said1, HB_RF_HIDDEN) == 0, "once per being and reason");
        CHECK(HbRefNote(&c, &said1, HB_RF_PLACE) == 1 && HbRefNote(&c, &said2, HB_RF_HIDDEN) == 1, "another reason / being counts");
        CHECK(HbRefNote(&c, &said2, HB_RF_NONE) == 0 && HbRefNote(&c, &said2, HB_RF_N) == 0, "no reason: nothing");
        CHECK(c.n[HB_RF_HIDDEN] == 2 && c.n[HB_RF_PLACE] == 1 && c.n[HB_RF_NONE] == 0, "counts %d %d", c.n[HB_RF_HIDDEN], c.n[HB_RF_PLACE]);
    }
    // D2 as a story: a victim's ghost arrives in an elevator (All Near); 8 s in state 0D / 0E sliding onto her (no bar on any
    // frame, the loop forces the alpha to 0), then the game finds a wall spot and it stands (state 02): the bar fades in 0.2 s
    for (k = 0; k < (int)(sizeof kFps / sizeof kFps[0]); ++k) {
        const int fps = kFps[k];
        const float dt = 1.0f / (float)fps;
        HbBar b = { 0 };
        int f, everUp = 0;
        float at02 = -1.0f;
        for (f = 1; f <= 10 * fps; ++f) {
            const float t = (float)f * dt;
            const int cls = t < 8.0f - 1e-6f ? ((f & 1) ? 0x0D : 0x0E) : 0x02;
            const float d = t < 8.0f ? 1500.0f * (1.0f - t / 8.0f) : 600.0f;  // it slides onto her
            const float pos[3] = { 100.0f + d, -160.0f, 50.0f };
            if (HbRefuseWhy(cls, pos) != HB_RF_NONE) {
                HbBarStep(&b, 1.0f, 0.0f, 0, dt);
                b.alpha = 0.0f;
            } else {
                HbBarStep(&b, 1.0f, HbWantBoss(HB_MODE_NEAR, 0, 0, b.sinceHit, d), 1, dt);
                if (at02 < 0.0f) at02 = t;
            }
            if (t < 8.0f - 1e-6f && b.alpha > 0.0f) everUp = 1;
            if (fabsf(t - 8.2f) < 0.5f * dt) CHECK(Near(b.alpha, 1.0f, 1.01f * dt / HB_FADE_IN_S), "%d fps: up 0.2 s after it stood (%.3f)", fps, b.alpha);
        }
        CHECK(!everUp && Near(at02, 8.0f, 1.5f * dt), "%d fps: no bar while it arrived; it stood at %.3f s", fps, at02);
    }
    // D1 as a story: Walter emerges 2500 away (All Near): v3.51 never; now up 0.2 s after his first active frame
    for (k = 0; k < (int)(sizeof kFps / sizeof kFps[0]); ++k) {
        const int fps = kFps[k];
        const float dt = 1.0f / (float)fps;
        HbBar old = { 0 }, now = { 0 };
        int f;
        for (f = 1; f <= fps; ++f) {
            HbBarStep(&old, 1.0f, HbWant(HB_MODE_NEAR, 0, old.sinceHit, 2500.0f), 1, dt);
            HbBarStep(&now, 1.0f, HbWantBoss(HB_MODE_NEAR, 1, 0, now.sinceHit, 2500.0f), 1, dt);
            if (f == (int)(0.2f * (float)fps + 0.5f)) CHECK(Near(now.alpha, 1.0f, 1e-4f), "%d fps: Walter's bar up at 0.2 s", fps);
        }
        CHECK(old.alpha == 0.0f && now.alpha == 1.0f, "%d fps: v3.51 %.2f, now %.2f", fps, old.alpha, now.alpha);
    }
}

// v3.52 MERGE (HB352 x EP352): a calm ghost keeps its bar when the game draws it. EP352 sets a dive back to the idle
// float (AI state 1, class 1) and holds a ghost calm after its rest; a dive at its wall node (class 7) is never set back.
static void TestCalm352(void)
{
    const float pos[3] = { 1200.0f, -50.0f, 800.0f };
    int cls;
    for (cls = 1; cls <= 7; ++cls) {
        CHECK(HbRefuseWhy(cls, pos) == HB_RF_NONE && HbStateDrawn(cls), "calm / idle ghost class %d refused", cls);
        CHECK(HbGhostState(0x0B, cls, 0.0f, 0) == HB_GS_STAND, "calm ghost class %d not standing", cls);
    }
    for (cls = 0x0D; cls <= 0x0F; ++cls) CHECK(HbRefuseWhy(cls, pos) == HB_RF_HIDDEN, "arriving class %X shown", cls);
    CHECK(HbRefuseWhy(0, pos) == HB_RF_HIDDEN, "class 0 shown");
}

int main(void)
{
    TestRefuse352();  // v3.52 HB352
    TestCalm352();  // v3.52 MERGE
    TestFrameRates();
    TestWant();
    TestSeenAndHeal();
    TestDown();
    TestStages();
    TestMeter();
    TestGhostState();
    TestRevive();
    TestPulse();
    TestBoth();  // v3.32 DHS332
    TestLocked();  // v3.32 MERGE
    TestTarget();  // v3.35 CM335
    TestNotch();  // v3.35 CM335
    TestStageLabel();  // v3.35 CM335
    TestCurse337();  // v3.37 MN337
    TestHud338();  // v3.38 HB338
    TestCurse341();  // v3.41 CM341
    TestCurse342();  // v3.42 SM342
    printf("hudbars_sim: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
