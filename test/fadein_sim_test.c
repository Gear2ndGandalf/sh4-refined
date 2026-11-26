// v3.50 FI350: native test of src\fadein_sim.h (test\build_fadein_test.cmd): the [Gameplay] FadeIn / FadeInSeconds texts;
// who fades (types, bosses, spawn modes, pre-draw callbacks), the source and skip rules and their order; the fade, shadow
// and portal curves (ends, monotony, the same look at 30 / 60 / 120 / 180 fps on the draw clock, a pause adds 1/30 s);
// the room visit's table (find, a re-used slot, a full table keeps the fading ones), Revive Enemies' notes, the room line's
// text (fixed order for tools\logcheck.py, a short buffer stays terminated); and the stock exe (SH4Decomp orig\sh4.exe):
// 0x45C880 has exactly the bytes fadein.inc checks, with its eight calls, and the pre-draw callbacks are where the rules say.
// Usage: out\fadein_sim_test.exe [path of the stock sh4.exe]
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/fadein_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- the exe as mapped (sections at their VAs)
static unsigned char *g_img;
static unsigned g_imgSize;
static int LoadExe(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *d;
    long n;
    unsigned pe, nsec, so, i, size;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = (unsigned char *)malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) return fclose(f), 0;
    fclose(f);
    memcpy(&pe, d + 0x3C, 4);
    nsec = *(unsigned short *)(d + pe + 6);
    so = pe + 24 + *(unsigned short *)(d + pe + 20);
    memcpy(&size, d + pe + 24 + 56, 4);  // SizeOfImage
    g_img = (unsigned char *)calloc(size, 1);
    g_imgSize = size;
    for (i = 0; i < nsec; ++i) {
        unsigned vs, va, rs, rp;
        memcpy(&vs, d + so + 40 * i + 8, 4);
        memcpy(&va, d + so + 40 * i + 12, 4);
        memcpy(&rs, d + so + 40 * i + 16, 4);
        memcpy(&rp, d + so + 40 * i + 20, 4);
        if (va + (rs < vs || !vs ? rs : vs) <= size) memcpy(g_img + va, d + rp, rs < vs || !vs ? rs : vs);
    }
    free(d);
    return 1;
}
static const unsigned char *Img(unsigned va) { return va >= 0x400000u && va - 0x400000u < g_imgSize ? g_img + (va - 0x400000u) : NULL; }

static void TestExe(const char *path)
{
    static const unsigned kCalls[8][2] = { { 0x19, 0x55C650 }, { 0x78, 0x404B80 }, { 0xA3, 0x429ED0 }, { 0xAF, 0x429F00 },
                                           { 0xB9, 0x55C650 }, { 0xBE, 0x429EF0 }, { 0xC3, 0x429F20 }, { 0xCA, 0x404B80 } };
    const unsigned char *p;
    int i, calls = 0;
    if (!LoadExe(path)) {
        CHECK(0, "the stock exe %s could not be read", path);
        return;
    }
    p = Img(FI_SEE_VA);
    CHECK(p && !memcmp(p, kFiSeeThrough, sizeof kFiSeeThrough), "0x45C880 holds the 214 bytes fadein.inc checks");
    CHECK(p && p[213] == 0xC3 && p[214] == 0x90, "0x45C880 ends with its ret at +0xD5 (then padding)");
    for (i = 0; i < 8 && p; ++i) {
        const unsigned off = kCalls[i][0];
        int rel;
        memcpy(&rel, p + off + 1, 4);
        CHECK(p[off] == 0xE8 && FI_SEE_VA + off + 5 + (unsigned)rel == kCalls[i][1], "0x45C880 +%02X calls %08X", off, kCalls[i][1]);
        calls += p[off] == 0xE8;
    }
    CHECK(calls == 8, "the eight calls of 0x45C880 (%d)", calls);
    // fadein_sim.h FiPreDrawKind's callbacks (enemy_common.c): 0x45C6B0 `mov [esp+4], 1; jmp 0x429F60` (ghost, opaque),
    // 0x45C6C0 `mov [esp+4], 0x53001 ...` (no depth write), 0x45C710 / 0x45C7A0 read [[task+0x408]+0x80] (the own fade)
    p = Img(0x45C6B0);
    CHECK(p && p[0] == 0xC7 && p[1] == 0x44 && p[2] == 0x24 && p[3] == 0x04 && p[4] == 1 && p[8] == 0xE9, "0x45C6B0 is the opaque ghost pre-draw");
    p = Img(0x45C710);
    CHECK(p && !memcmp(p, "\x83\xEC\x14\x8B\x44\x24\x18\x8B\x88\x08\x04\x00\x00\xD9\x81\x80\x00\x00\x00", 19),
          "0x45C710 reads [[task+0x408]+0x80] (the own fade)");
    p = Img(0x45C7A0);
    CHECK(p && !memcmp(p, "\x83\xEC\x14\x8B\x44\x24\x18\x8B\x88\x08\x04\x00\x00\xD9\x81\x80\x00\x00\x00", 19),
          "0x45C7A0 reads [[task+0x408]+0x80] too");
    p = Img(0x52AEB4);  // the hook site screenfx.inc owns: push ebx; call 55C650
    CHECK(p && !memcmp(p, "\x53\xE8\x96\x17\x03\x00\x83\xC4\x04\x8B\x76\x28", 12), "0x52AEB5 is the registry loop's ModelDraw call");
    p = Img(0x52AF94);
    CHECK(p && !memcmp(p, "\x50\xE8\xC6\x18\x03\x00", 6), "0x52AF95 is the shadow loop's ModelDrawShadow call");
}

static void TestTexts(void)
{
    int k = 7, v;
    for (v = 0; v < FI_MODE_N; ++v) CHECK(FiModeParse(kFiModeName[v], &k) == v && k, "%s reads", kFiModeName[v]);
    CHECK(FiModeParse("fade + PORTAL", &k) == FI_MODE_PORTAL && k, "case ignored");
    CHECK(FiModeParse("Fade+Portal", &k) == FI_MODE_PORTAL && k, "spaces ignored");
    CHECK(FiModeParse(" off ", &k) == FI_MODE_OFF && k, "outer spaces ignored");
    CHECK(FiModeParse("On", &k) == FI_MODE_PORTAL && k && FiModeParse("1", &k) == FI_MODE_PORTAL && k, "On / 1 = the default value");
    CHECK(FiModeParse("0", &k) == FI_MODE_OFF && k, "0 = Off");
    CHECK(FiModeParse("Dissolve", &k) == FI_MODE_DEFAULT && !k, "unknown text = the default, not known");
    CHECK(FiModeParse("", &k) == FI_MODE_DEFAULT && !k && FiModeParse(NULL, NULL) == FI_MODE_DEFAULT, "empty / NULL = the default");
    CHECK(FiModeParse("Fade +", &k) == FI_MODE_DEFAULT && !k, "a part of a name is not a name");
    CHECK(FI_MODE_DEFAULT == FI_MODE_PORTAL && !strcmp(kFiModeName[FI_MODE_DEFAULT], "Fade + Portal"), "default Fade + Portal");
    CHECK(strlen(kFiModeName[FI_MODE_PORTAL]) <= 14, "Fade + Portal fits the value column (LONGVAL_CHARS 14)");
    CHECK(FiSecondsClamp(0.0f) == FI_SECONDS_DEFAULT && FiSecondsClamp(-1.0f) == FI_SECONDS_DEFAULT, "0 / negative seconds = default");
    CHECK(FiSecondsClamp((float)sqrt(-1.0)) == FI_SECONDS_DEFAULT, "NaN seconds = default");
    CHECK(FiSecondsClamp(0.05f) == FI_SECONDS_MIN && FiSecondsClamp(9.0f) == FI_SECONDS_MAX && FiSecondsClamp(1.25f) == 1.25f,
          "seconds clamped to 0.2 .. 3");
}

static void TestWho(void)
{
    static const int kFade[0x14] = { 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 };
    int t;
    for (t = 0; t < 0x14; ++t) CHECK(FiTypeFades(t) == kFade[t], "type %02X fades %d", t, kFade[t]);
    for (t = 0; t < 0x14; ++t) CHECK(FiGhostType(t) == (t >= 0x0B && t <= 0x0F), "type %02X ghost", t);
    CHECK(FiBoss(0x10, 0) && FiBoss(0x05, 0x08) && FiBoss(0x05, 0x108) && !FiBoss(0x05, 0) && !FiBoss(0x0B, 8), "bosses: Walter, the Wall boss");
    CHECK(FiModeOk(0x02, 0) && FiModeOk(0x02, 0x100) && !FiModeOk(0x02, 6) && !FiModeOk(0x07, 6), "monsters: mode 0 only (Gum Head 6 = event)");
    CHECK(FiModeOk(0x0A, 3) && FiModeOk(0x0A, 6) && FiModeOk(0x0A, 7) && !FiModeOk(0x0A, 5), "Sniffer Dogs 3 / 6 / 7 are bodies");
    CHECK(FiModeOk(0x03, 1) && !FiModeOk(0x03, 2), "a Hummer on the floor (1) is a body");
    CHECK(FiModeOk(0x0B, 4) && FiModeOk(0x0B, 0x104) && FiModeOk(0x0C, 9) && FiModeOk(0x0F, 6), "ghosts: any mode");
    CHECK(FiPreDrawKind(0) == FI_PD_PLAIN && FiPreDrawKind(0x45C6B0) == FI_PD_PLAIN && FiPreDrawKind(0x45C6C0) == FI_PD_PLAIN,
          "plain pre-draws: none, opaque ghost, no depth write");
    CHECK(FiPreDrawKind(0x45C710) == FI_PD_OWNFADE && FiPreDrawKind(0x45C7A0) == FI_PD_OWNFADE, "own fade pre-draws");
    CHECK(FiPreDrawKind(0x45C840) == FI_PD_FOREIGN && FiPreDrawKind(0x45C860) == FI_PD_FOREIGN && FiPreDrawKind(0x5473A0) == FI_PD_FOREIGN,
          "additive / other / haunting pre-draws: their own draw state");
}

static void TestRules(void)
{
    int g, c, r, t, a;
    // the source: a follower first, then a wanderer, revive, copy, the room load, then ghost / monster
    CHECK(FiSource(1, 0, 0, 1, 1, 0) == FI_SRC_FOLLOW && FiSource(0, 2, 0, 0, 1, 0) == FI_SRC_FOLLOW, "watch list 0 / 2 = follower");
    CHECK(FiSource(0, -1, 1, 0, 0, 1) == FI_SRC_FOLLOW, "a tagged victim = follower");
    CHECK(FiSource(1, 1, 0, 0, 0, 0) == FI_SRC_WANDER && FiSource(1, 3, 0, 0, 1, 0) == FI_SRC_WANDER, "watch list 1 / 3 = wanderer");
    CHECK(FiSource(1, -1, 0, 1, 0, 0) == FI_SRC_REVIVE, "a revive note = revive");
    CHECK(FiSource(1, -1, 0, 0, 1, 0) == FI_SRC_COPY && FiSource(0, -1, 0, 0, 1, 1) == FI_SRC_COPY, "copy ids = copy (any time)");
    CHECK(FiSource(1, -1, 0, 0, 0, 1) == FI_SRC_ROOM && FiSource(1, -1, 0, 0, 0, 0) == FI_SRC_ROOM, "the room's first moments = room load");
    CHECK(FiSource(0, -1, 0, 0, 0, 1) == FI_SRC_GHOST && FiSource(0, -1, 0, 0, 0, 0) == FI_SRC_MONSTER, "later: ghost / monster");
    for (g = 0; g < 2; ++g)
        for (c = 0; c < 2; ++c)
            for (r = 0; r < 2; ++r)
                for (t = 0; t < 2; ++t)
                    for (a = -1; a < 4; ++a) {
                        const int s = FiSource(g, a, t, r, c, 1);
                        CHECK(s >= 0 && s < FI_SRC_N, "a source in range");
                    }
    // the skip order: off, stage, cut-scene, boss, type, event, dead, own draw, own portal, own fade
    CHECK(FiSkipWhy(0, 0, 1, 0, 1, 0, 1, 0, 1, 1) == FI_SKIP_OFF, "off first");
    CHECK(FiSkipWhy(1, 0, 1, 0, 1, 0, 1, 0, 1, 1) == FI_SKIP_STAGE, "then the stage");
    CHECK(FiSkipWhy(1, 1, 1, 0, 1, 0, 1, 0, 1, 1) == FI_SKIP_SCENE, "then a cut-scene");
    CHECK(FiSkipWhy(1, 1, 0, 0, 1, 0, 1, 0, 1, 1) == FI_SKIP_BOSS, "then a boss (before its type)");
    CHECK(FiSkipWhy(1, 1, 0, 0, 0, 0, 1, 0, 1, 1) == FI_SKIP_TYPE, "then the type");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 0, 1, 0, 1, 1) == FI_SKIP_EVENT, "then an event spawn");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 1, 1, 0, 1, 1) == FI_SKIP_DEAD, "then a dead body");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 1, 0, 0, 1, 1) == FI_SKIP_DRAW, "then a draw state of its own");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 1, 0, 1, 1, 1) == FI_SKIP_OWNPORTAL, "then its own portal");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 1, 0, 1, 0, 1) == FI_SKIP_OWNFADE, "then its own fade");
    CHECK(FiSkipWhy(1, 1, 0, 1, 0, 1, 0, 1, 0, 0) == FI_SKIP_NONE, "else it fades");
    CHECK(FiPortalFor(FI_MODE_PORTAL, 0x0B, 0) && FiPortalFor(FI_MODE_PORTAL, 0x0F, 0), "a later ghost gets a portal");
    CHECK(!FiPortalFor(FI_MODE_PORTAL, 0x0B, 1), "not at the room load");
    CHECK(!FiPortalFor(FI_MODE_FADE, 0x0B, 0) && !FiPortalFor(FI_MODE_OFF, 0x0B, 0), "only with Fade + Portal");
    CHECK(!FiPortalFor(FI_MODE_PORTAL, 0x0A, 0) && !FiPortalFor(FI_MODE_PORTAL, 0x03, 0), "never a monster");
}

static void TestCurves(void)
{
    float T, prev, x;
    int i, ok;
    CHECK(FiFrac(0.0f, 0.8f) == 0.0f && FiFrac(-1.0f, 0.8f) == 0.0f, "frac 0 at first sight");
    CHECK(FiFrac(0.8f, 0.8f) == 1.0f && FiFrac(5.0f, 0.8f) == 1.0f, "frac 1 at T and after");
    CHECK(fabsf(FiFrac(0.4f, 0.8f) - 0.5f) < 1e-6f, "frac 0.5 at T / 2 (smoothstep)");
    CHECK(FiFrac(0.1f, 0.0f) == 1.0f && FiFrac((float)sqrt(-1.0), 0.8f) == 0.0f, "T 0: at once; NaN age: 0");
    for (T = 0.2f; T <= 3.0f; T += 0.4f) {
        prev = -1.0f, ok = 1;
        for (i = 0; i <= 200; ++i) {
            x = FiFrac(T * 1.2f * (float)i / 200.0f, T);
            if (x < prev - 1e-7f || x < 0.0f || x > 1.0f) ok = 0;
            prev = x;
        }
        CHECK(ok, "frac rises 0 .. 1 without a step back (T %.1f)", T);
    }
    CHECK(FiTransparency(0.0f, 0.0f) == 1.0f, "first sight: transparency 1 (0x45C880 draws nothing)");
    CHECK(FiTransparency(1.0f, 0.0f) == 0.0f, "in: 0 (opaque)");
    CHECK(fabsf(FiTransparency(0.5f, 0.0f) - 0.5f) < 1e-6f, "half in: 0.5");
    CHECK(fabsf(FiTransparency(1.0f, 0.3f) - 0.3f) < 1e-6f && fabsf(FiTransparency(0.5f, 0.3f) - 0.65f) < 1e-6f,
          "the game's own see-through amount stays under ours");
    CHECK(FiTransparency(0.5f, 1.0f) == 1.0f && FiTransparency(0.5f, 7.0f) == 1.0f, "own 1 (or more): still invisible");
    CHECK(FiTransparency(0.5f, (float)sqrt(-1.0)) == 0.5f && FiTransparency((float)sqrt(-1.0), 0.0f) == 1.0f, "NaN inputs: safe");
    CHECK(FiTransparency(2.0f, -1.0f) == 0.0f, "out-of-range inputs clamped");
    CHECK(!FiShadowShown(0.0f) && !FiShadowShown(0.49f) && FiShadowShown(0.5f) && FiShadowShown(1.0f), "the shadow from half in");
    CHECK(FiPortalOpen(0.0f, 0.8f) == 0.0f && FiPortalOpen(-0.1f, 0.8f) == 0.0f, "the vortex starts closed");
    CHECK(FiPortalOpen(FI_PORTAL_OPEN_S, 0.8f) == 1.0f && FiPortalOpen(0.8f, 0.8f) == 1.0f, "open from 0.35 s through T");
    CHECK(FiPortalOpen(0.8f + FI_PORTAL_CLOSE_S, 0.8f) == 0.0f && FiPortalOpen(10.0f, 0.8f) == 0.0f, "closed 0.6 s after T");
    CHECK(fabsf(FiPortalLife(0.8f) - 1.4f) < 1e-6f && FiPortalLife(0.0f) == FI_SECONDS_DEFAULT + FI_PORTAL_CLOSE_S, "its life");
    CHECK(FiPortalOpen(0.1f, 0.2f) == 1.0f && FiPortalOpen(0.05f, 0.2f) > 0.0f && FiPortalOpen(0.05f, 0.2f) < 1.0f,
          "a short fade opens in half its time");
    for (T = 0.2f; T <= 3.0f; T += 0.7f) {
        float last = 0.0f, jump = 0.0f, y;
        for (i = 1; i <= 2000; ++i) {
            y = FiPortalOpen(FiPortalLife(T) * 1.1f * (float)i / 2000.0f, T);
            if (fabsf(y - last) > jump) jump = fabsf(y - last);
            last = y;
        }
        CHECK(jump < 0.05f, "the vortex has no jump (T %.1f: %.3f)", T, jump);
    }
}

// the draw clock at different frame rates: the transparency at the same real time agrees
static void TestClock(void)
{
    static const int kFps[4] = { 30, 60, 120, 180 };
    float at[4];
    int k, i;
    double c;
    CHECK(FiClockStep(0.016, 1) == 0.016 && FiClockStep(0.5, 1) == 0.1, "continuous: real time, at most 0.1 s");
    CHECK(fabs(FiClockStep(5.0, 0) - 1.0 / 30.0) < 1e-12 && FiClockStep(0.01, 0) == 0.01, "a gap (pause, load): at most 1/30 s");
    CHECK(FiClockStep(-1.0, 1) == 0.0 && FiClockStep(sqrt(-1.0), 1) == 0.0, "negative / NaN: 0");
    for (k = 0; k < 4; ++k) {
        c = 0.0;
        for (i = 0; i < kFps[k] * 6 / 10; ++i) c += FiClockStep(1.0 / kFps[k], 1);  // 0.6 s of frames
        at[k] = FiTransparency(FiFrac((float)c, 0.8f), 0.0f);
    }
    for (k = 1; k < 4; ++k) CHECK(fabsf(at[k] - at[0]) < 1e-3f, "%d fps looks as 30 fps at 0.6 s (%.4f / %.4f)", kFps[k], at[k], at[0]);
    c = 0.0;
    for (i = 0; i < 30; ++i) c += FiClockStep(1.0 / 60.0, 1);
    c += FiClockStep(12.0, 0);  // a 12 s pause in the middle of a fade
    for (i = 0; i < 30; ++i) c += FiClockStep(1.0 / 60.0, 1);
    CHECK(fabs(c - (1.0 + 1.0 / 30.0)) < 1e-9, "a pause adds 1/30 s to the fade (%.4f)", c);
}

static void TestTable(void)
{
    static FiTable t;
    static char buf[FI_MAX][4];
    FiEnt *e;
    int i, fading = 0;
    FiTableClear(&t);
    e = FiAdd(&t, buf[0], buf[1], 0xFFFF0007u, 1.0);
    CHECK(t.n == 1 && e->state == FI_E_DONE && e->t0 == 1.0, "a new entry starts done (the caller sets the fade)");
    e->state = FI_E_FADE;
    CHECK(FiFind(&t, buf[0], buf[1], 0xFFFF0007u) == 0, "found by task, work, id");
    CHECK(FiFind(&t, buf[0], buf[2], 0xFFFF0007u) < 0 && FiFind(&t, buf[0], buf[1], 0x02040001u) < 0, "another work or id: not found");
    FiAdd(&t, buf[3], buf[4], 1u, 2.0);
    FiDropTask(&t, buf[0]);
    CHECK(t.n == 1 && FiFind(&t, buf[3], buf[4], 1u) == 0, "a re-used slot's old entry goes, the others stay");
    FiTableClear(&t);
    for (i = 0; i < FI_MAX; ++i) {  // full: every other one fading
        e = FiAdd(&t, buf[i], buf[i], (unsigned)i, (double)i);
        e->state = i & 1 ? FI_E_FADE : FI_E_DONE;
    }
    CHECK(t.n == FI_MAX, "the table fills to %d", FI_MAX);
    e = FiAdd(&t, buf[0] + 1, buf[0] + 1, 999u, 100.0);
    CHECK(t.n == FI_MAX && e->id == 999u && FiFind(&t, buf[0], buf[0], 0u) < 0, "full: the oldest done entry makes room");
    for (i = 0; i < FI_MAX; ++i) fading += t.e[i].state == FI_E_FADE;
    CHECK(fading == FI_MAX / 2, "no fading entry was dropped (%d)", fading);
    {   // the game's arrival: the placeholder (task A) becomes the real ghost (task B) with the same id: the entry follows
        static FiTable u;
        FiEnt *p, *q;
        FiTableClear(&u);
        p = FiAdd(&u, buf[10], buf[11], 0xFFFF0009u, 5.0);
        p->state = FI_E_FADE, p->src = FI_SRC_FOLLOW, p->portal = 1;
        FiAdd(&u, buf[12], buf[13], 0x02020002u, 5.0);
        FiAdd(&u, buf[14], buf[15], 0x02020003u, 5.0);  // B's slot held another enemy before (it went)
        CHECK(FiTakeOver(&u, buf[20], buf[21], 0x12345678u, 6.0) == NULL, "an unknown id: no take-over");
        CHECK(FiTakeOver(&u, buf[20], buf[21], 0u, 6.0) == NULL, "id 0: never taken over");
        q = FiTakeOver(&u, buf[14], buf[22], 0xFFFF0009u, 5.5);
        CHECK(q && q->id == 0xFFFF0009u && q->task == buf[14] && q->work == buf[22] && q->state == FI_E_FADE && q->t0 == 5.0 &&
              q->seen == 5.5 && q->src == FI_SRC_FOLLOW && q->portal == 1, "the real ghost takes the placeholder's entry (its fade goes on)");
        CHECK(u.n == 2 && FiFind(&u, buf[14], buf[15], 0x02020003u) < 0 && FiFind(&u, buf[12], buf[13], 0x02020002u) >= 0 &&
              FiFind(&u, buf[14], buf[22], 0xFFFF0009u) >= 0, "the slot's old entry went, the others stay (%d)", u.n);
        FiTableClear(&u);
        FiAdd(&u, buf[30], buf[30], 7u, 1.0);   // the old entry of the new task's slot is first ...
        FiAdd(&u, buf[31], buf[31], 8u, 1.0);   // ... the taken one is last (it moves when the first goes)
        q = FiTakeOver(&u, buf[30], buf[32], 8u, 2.0);
        CHECK(q && u.n == 1 && q == &u.e[0] && q->id == 8u && q->task == buf[30], "the kept entry moved by the removal is returned");
    }
    {   // a fading enemy hidden for 2 s (a ghost in a wall) keeps its fade; a done one is not touched
        FiEnt f;
        memset(&f, 0, sizeof f);
        f.state = FI_E_FADE, f.t0 = f.seen = 10.0;
        FiAge(&f, 10.2, 0);
        FiAge(&f, 10.4, 1);
        FiAge(&f, 12.4, 1);
        FiAge(&f, 12.4, 1);  // (the shadow pass in the same frame)
        FiAge(&f, 12.6, 0);
        CHECK(fabs(f.t0 - 12.2) < 1e-9 && f.seen == 12.6, "hidden 2.2 s: the fade's start moves on 2.2 s (%.3f)", f.t0);
        CHECK(fabs(FiFrac((float)(12.6 - f.t0), 0.8f) - FiFrac(0.4f, 0.8f)) < 1e-6f, "it shows at the fade it had when it hid");
        f.state = FI_E_DONE, f.t0 = 1.0;
        FiAge(&f, 20.0, 1);
        CHECK(f.t0 == 1.0 && f.seen == 20.0, "a done entry keeps its start");
    }
    // v3.50 FI350-001 (Codex inspection 2): an entry drawn within FI_STALE_S is never taken
    for (i = 0; i < FI_MAX; ++i) t.e[i].state = FI_E_FADE, t.e[i].t0 = 50.0 + i, t.e[i].seen = 199.0;
    t.e[17].seen = 3.0;
    e = FiAdd(&t, buf[1] + 2, buf[1] + 2, 1000u, 200.0);
    CHECK(e == &t.e[17] && e->id == 1000u, "full: the one not drawn for the longest time (over 2 s) makes room");
    for (i = 0; i < FI_MAX; ++i) t.e[i].seen = 199.5;
    CHECK(FiAdd(&t, buf[2] + 2, buf[2] + 2, 1001u, 200.0) == NULL && FiFind(&t, buf[1] + 2, buf[1] + 2, 1000u) >= 0,
          "full of enemies drawn just now: no entry (drawn as the game does), nothing evicted");
    {   // saturation: FI_MAX + 20 enemies all on screen from the first frame through a whole fade (0.8 s + 0.4 s at 60 fps):
        // every one that got an entry keeps it (its fade never restarts), the overflow ones never get one meanwhile
        static FiTable s;
        static char ab[FI_MAX + 20];
        int f, j, starts = 0, lost = 0, n = FI_MAX + 20;
        static unsigned char had[FI_MAX + 20];
        FiTableClear(&s);
        memset(had, 0, sizeof had);
        for (f = 0; f < 72; ++f) {
            const double now = 10.0 + f / 60.0;
            for (j = 0; j < n; ++j) {
                const int k = FiFind(&s, &ab[j], &ab[j], (unsigned)j + 1u);
                if (k >= 0) {
                    FiAge(&s.e[k], now, 0);
                    continue;
                }
                if (had[j]) ++lost;
                {
                    FiEnt *x = FiAdd(&s, &ab[j], &ab[j], (unsigned)j + 1u, now);
                    if (x) x->state = FI_E_FADE, had[j] = 1, ++starts;
                }
            }
        }
        CHECK(starts == FI_MAX && lost == 0, "saturation: %d fades started (one per entry), %d restarted", starts, lost);
    }
}

static void TestNotes(void)
{
    static FiNotes n;
    unsigned i;
    memset(&n, 0, sizeof n);
    FiNotePut(&n, 2, 4, 0x02040001u);
    FiNotePut(&n, 2, 4, 0x02040001u);
    FiNotePut(&n, 2, 2, 0x02020001u);
    CHECK(n.n == 2 && FiNoteHas(&n, 2, 4, 0x02040001u) && !FiNoteHas(&n, 2, 2, 0x02040001u) && !FiNoteHas(&n, 3, 4, 0x02040001u),
          "notes by stage, room, id (no doubles)");
    FiNoteKeepRoom(&n, 2, 2);
    CHECK(n.n == 1 && FiNoteHas(&n, 2, 2, 0x02020001u) && !FiNoteHas(&n, 2, 4, 0x02040001u), "a new room visit keeps only its notes");
    for (i = 0; i < FI_NOTE_MAX + 5; ++i) FiNotePut(&n, 5, 1, 0x05010000u + i);
    CHECK(n.n == FI_NOTE_MAX && !FiNoteHas(&n, 2, 2, 0x02020001u) && FiNoteHas(&n, 5, 1, 0x05010000u + FI_NOTE_MAX + 4) &&
          !FiNoteHas(&n, 5, 1, 0x05010000u), "full: the oldest notes go");
}

static void TestText(void)
{
    FiStats s;
    char b[400], small[24];
    int len;
    memset(&s, 0, sizeof s);
    CHECK(!FiStatsAny(&s), "nothing: no line");
    s.skips[FI_SKIP_OFF] = 9;
    CHECK(!FiStatsAny(&s), "only seen while off: no line");
    s.fades[FI_SRC_ROOM] = 3, s.fades[FI_SRC_FOLLOW] = 2, s.fades[FI_SRC_GHOST] = 1, s.portals = 2;
    s.skips[FI_SKIP_BOSS] = 1, s.skips[FI_SKIP_OWNPORTAL] = 1;
    CHECK(FiStatsAny(&s), "fades: a line");
    len = FiStatsText(&s, b, sizeof b);
    CHECK(!strcmp(b, "6 fade(s) (room load 3, follower 2, wanderer 0, copy 0, revive 0, ghost 1, monster 0), 2 portal(s); 2 not "
                     "faded (stage 0, cut-scene 0, type 0, boss 1, event 0, dead 0, own draw 0, own portal 1, own fade 0)") &&
          len == (int)strlen(b), "the room line's text: %s", b);
    len = FiStatsText(&s, small, sizeof small);
    CHECK(len == (int)strlen(small) && len < (int)sizeof small && !strncmp(small, "6 fade(s) (room load 3,", 22), "a short buffer stays terminated (%s)", small);
    CHECK(FiStatsText(&s, NULL, 10) == 0 && FiStatsText(&s, small, 1) == 0, "no buffer: nothing");
}

int main(int argc, char **argv)
{
    const char *exe = argc > 1 ? argv[1] : "C:\\ClaudeCode\\CModding\\SH4Decomp\\orig\\sh4.exe";
    TestTexts();
    TestWho();
    TestRules();
    TestCurves();
    TestClock();
    TestTable();
    TestNotes();
    TestText();
    TestExe(exe);
    printf("%s: %d checks, %d failed\n", g_fail ? "FAIL" : "ok", g_n, g_fail);
    return g_fail ? 1 : 0;
}
