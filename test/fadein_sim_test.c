// v3.50 FI350: native test of src\fadein_sim.h (test\build_fadein_test.cmd): the [Gameplay] FadeIn / FadeInSeconds texts;
// who fades (types, bosses, spawn modes, pre-draw callbacks), the source and skip rules and their order; the fade, shadow
// and portal curves (ends, monotony, the same look at 30 / 60 / 120 / 180 fps on the draw clock, a pause adds 1/30 s);
// the room visit's table (find, a re-used slot, a full table keeps the fading ones), Revive Enemies' notes, the room line's
// text (fixed order for tools\logcheck.py, a short buffer stays terminated); and the stock exe (SH4Decomp orig\sh4.exe):
// 0x45C880 has exactly the bytes fadein.inc checks, with its eight calls, and the pre-draw callbacks are where the rules say.
// v3.51 PT351: the tar portal and the hold: FadeInPortalDraw, the hold's factor and step, the portal's timeline, the blots /
// drips layout, the drips' run, the strands' snap order and phases, the plane's axes, the room line's new part; and in the
// stock exe: the game code it calls (kFiTarFns), the step list's entry 8, and the offsets it reads / writes there.
// v3.58 FI358: the bigger, stickier portal: the hidden keys, the strands' end and snap window, the body's box from its
// bones / its type, the whole portal's size, the pieces' layout (never one bigger than the first, all inside, the target
// spanned), their amounts, the budget's share and weights, the strands' width / sag / drop, the timeline, the close line.
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
    s.byDraw[FI_TD_GAME] = 1, s.byDraw[FI_TD_OWN] = 1, s.held = 2, s.drawFrames = 4, s.drawMs = 0.5, s.drawMax = 0.25f;  // v3.51 PT351
    len = FiStatsText(&s, b, sizeof b);
    CHECK(!strcmp(b, "6 fade(s) (room load 3, follower 2, wanderer 0, copy 0, revive 0, ghost 1, monster 0), 2 portal(s) (the game's "
                     "own draw 1, Refined's 1, vortex 0; 2 held; portal draw 0.125 ms a frame, max 0.250); 2 not "
                     "faded (stage 0, cut-scene 0, type 0, boss 1, event 0, dead 0, own draw 0, own portal 1, own fade 0)") &&
          len == (int)strlen(b), "the room line's text: %s", b);
    s.portals = 0;  // v3.51 PT351
    FiStatsText(&s, b, sizeof b);
    CHECK(strstr(b, "), 0 portal(s); 2 not faded (") != NULL, "no portal: no portal part (v3.50's form): %s", b);
    s.portals = 2;
    len = FiStatsText(&s, small, sizeof small);
    CHECK(len == (int)strlen(small) && len < (int)sizeof small && !strncmp(small, "6 fade(s) (room load 3,", 22), "a short buffer stays terminated (%s)", small);
    CHECK(FiStatsText(&s, NULL, 10) == 0 && FiStatsText(&s, small, 1) == 0, "no buffer: nothing");
}

// ---------------------------------------------------------------- v3.51 PT351: the tar portal and the hold
static void TestTar(void)
{
    FiTarLayout L, L2;
    float out[3], ax[3], ay[3], prevT, prevL, last, f;
    int i, j, k, n, known, okAll, seen[64];
    // FadeInPortalDraw
    for (i = 0; i < FI_TDM_N; ++i) CHECK(FiTdmParse(kFiTdmName[i], &known) == i && known, "FadeInPortalDraw %s reads", kFiTdmName[i]);
    CHECK(FiTdmParse("", &known) == FI_TDM_AUTO && known, "empty = Auto (known: the key is simply not there)");
    CHECK(FiTdmParse("refined", &known) == FI_TDM_REFINED && known, "case ignored");
    CHECK(FiTdmParse("Game", &known) == FI_TDM_AUTO && FiTdmParse("1", &known) == FI_TDM_AUTO, "Game / 1 = Auto");
    CHECK(FiTdmParse("Own", &known) == FI_TDM_REFINED && FiTdmParse("0", &known) == FI_TDM_VORTEX, "Own = Refined, 0 = Vortex");
    CHECK(FiTdmParse("Splash", &known) == FI_TDM_AUTO && !known, "unknown = Auto, not known");
    // the hold's factor: 40 % to 1.5 s, eased to 100 % at 2.5 s; 1 before it starts and after
    CHECK(FiHoldFactor(-0.1f) == 1.0f && FiHoldFactor(0.0f) == FI_HOLD_SLOW && FiHoldFactor(1.5f) == FI_HOLD_SLOW, "40 %% from 0 to 1.5 s");
    CHECK(FiHoldFactor(2.5f) == 1.0f && FiHoldFactor(9.0f) == 1.0f, "full speed at 2.5 s");
    CHECK(fabsf(FiHoldFactor(2.0f) - 0.7f) < 1e-4f, "half way at 2.0 s (%.4f)", FiHoldFactor(2.0f));
    okAll = 1, last = 0.0f;
    for (i = 0; i <= 300; ++i) {
        f = FiHoldFactor((float)i / 100.0f);
        if (f < last - 1e-6f || f < FI_HOLD_SLOW - 1e-6f || f > 1.0f) okAll = 0;
        last = f;
    }
    CHECK(okAll, "the factor never falls and stays in 0.4 .. 1");
    CHECK(FiHoldFactor((float)NAN) == 1.0f, "NaN age: not held");
    // one tick of the hold
    {
        const float p0[3] = { 100.0f, -50.0f, 200.0f }, c0[3] = { 110.0f, -40.0f, 180.0f };
        CHECK(!FiHoldStep(p0, c0, 0.4f, out) && fabsf(out[0] - 104.0f) < 1e-4f && fabsf(out[2] - 192.0f) < 1e-4f && out[1] == -40.0f,
              "40 %% of the level move, its height as the game left it (%.2f %.2f %.2f)", out[0], out[1], out[2]);
        CHECK(!FiHoldStep(p0, c0, 1.0f, out) && out[0] == c0[0] && out[2] == c0[2], "factor 1: where the game put it");
        CHECK(!FiHoldStep(p0, c0, 3.0f, out) && out[0] == c0[0] && !FiHoldStep(p0, c0, -1.0f, out) && out[0] == p0[0],
              "a factor outside 0 .. 1 is clamped");
    }
    {
        const float p0[3] = { 0.0f, 0.0f, 0.0f }, warp[3] = { 150.0f, 0.0f, 150.0f }, bad[3] = { 1.0f, 0.0f, (float)NAN };
        CHECK(FiHoldStep(p0, warp, 0.4f, out) && out[0] == 150.0f && out[2] == 150.0f, "a warp (212 u in a tick) is taken as it is");
        CHECK(FiHoldStep(p0, bad, 0.4f, out), "a bad value is taken as it is (no NaN written)");
    }
    {   // a ghost walking 3 u a tick for 2.5 s at 60 ticks: about 0.4 x for 1.5 s, then easing (the release line's numbers)
        float prev[3] = { 0.0f, 0.0f, 0.0f }, cur[3], moved = 0.0f, unheld = 0.0f;
        for (i = 0; i < 150; ++i) {
            const float age = (float)i / 60.0f;
            cur[0] = prev[0] + 3.0f, cur[1] = 0.0f, cur[2] = prev[2];
            FiHoldStep(prev, cur, FiHoldFactor(age), out);
            moved += out[0] - prev[0], unheld += 3.0f;
            memcpy(prev, out, sizeof out);
        }
        CHECK(unheld == 450.0f && moved > 0.5f * unheld && moved < 0.62f * unheld, "2.5 s held: %.0f of %.0f u", moved, unheld);
    }
    // the portal's timeline
    CHECK(FiTarRelease(0.0f) == FI_HOLD_END_S && FiTarRelease(1.0f) == 1.0f + FI_HOLD_END_S && FiTarRelease(-1.0f) == FI_TAR_WAIT_S,
          "it lets go at the hold's end (or after the wait for a ghost never real)");
    CHECK(FiTarAmount(0.0f, 2.5f) == 0.0f && FiTarAmount(-1.0f, 2.5f) == 0.0f && FiTarAmount(FI_TAR_OPEN_S, 2.5f) == 1.0f &&
          FiTarAmount(2.0f, 2.5f) == 1.0f, "opens over 0.6 s, stays open");
    CHECK(fabsf(FiTarAmount(2.5f + 0.5f * FI_TAR_CLOSE_S, 2.5f) - 0.25f) < 1e-4f && FiTarAmount(2.5f + FI_TAR_CLOSE_S, 2.5f) < 1e-6f && FiTarAmount(2.5f + FI_TAR_CLOSE_S + 0.01f, 2.5f) == 0.0f,
          "closes over 1.6 s ((1 - s)^2)");
    CHECK(fabsf(FiTarAmount(0.3f + 0.4f, 0.3f) - FiTarOpenAt(0.3f) * (1.0f - 0.25f) * (1.0f - 0.25f)) < 1e-4f,
          "a release before it is open closes from where it was");
    CHECK(!FiTarDone(4.09f, 2.5f) && FiTarDone(4.1f, 2.5f), "done at the release + 1.6 s");
    okAll = 1, prevT = 0.0f;
    for (i = 1; i <= 410; ++i) {
        const float a = FiTarAmount((float)i / 100.0f, 2.5f);
        if (a < 0.0f || a > 1.0f || (i <= 250 && a < prevT - 1e-6f) || (i > 251 && a > prevT + 1e-6f)) okAll = 0;
        prevT = a;
    }
    CHECK(okAll, "the amount rises, holds, falls, within 0 .. 1");
    CHECK(FiTarShown(0.0f) == 0 && FiTarShown(0.001f) == 1 && FiTarShown(FI_TAR_SPREAD_S) == FI_TAR_SPLATS &&
          FiTarShown(9.0f) == FI_TAR_SPLATS, "the 25 blots appear over 0.8 s");
    for (i = 0; i < FI_TAR_SPLATS; ++i)
        if (FiTarShown(FiTarAppear(i) + 1e-4f) < i + 1) break;
    CHECK(i == FI_TAR_SPLATS, "blot i is shown from its first second on (%d)", i);
    CHECK(FiTarLife(1.0f, 2.5f, 0.3f) == 1.0f && fabsf(FiTarLife(3.0f, 2.5f, 0.3f) + 0.2f) < 1e-5f, "a blot's life: whole, then its offset less the close time");
    CHECK(FiTarLifeShort(1.0f) == 1024 && FiTarLifeShort(-0.5f) == -512 && FiTarLifeShort(100.0f) == 32000 &&
          FiTarLifeShort(-100.0f) == -32000 && FiTarLifeShort((float)NAN) == 0, "x 1024, clamped to a short");
    CHECK(FiTarLifeAlpha(0.2f) == 1.0f && FiTarLifeAlpha(-FI_TAR_FADE_S) == 0.0f && fabsf(FiTarLifeAlpha(-0.5f * FI_TAR_FADE_S) - 0.75f) < 1e-5f,
          "the game's fade 1 - q^2 over 1.1 s");
    CHECK(0.5f - (FI_TAR_CLOSE_S - 0.0f) <= -FI_TAR_FADE_S, "every blot (offset <= 0.5 s) is gone by the close's end");
    // the layout: deterministic, inside the plane, the game's piece of each index, drips below, unique slots
    FiTarLayoutMake(&L, 12345u);
    FiTarLayoutMake(&L2, 12345u);
    CHECK(!memcmp(&L, &L2, sizeof L), "the same seed, the same layout");
    FiTarLayoutMake(&L2, 54321u);
    CHECK(memcmp(&L, &L2, sizeof L) != 0, "another seed, another layout");
    for (k = 0; k < 200; ++k) {
        int slots = 0, bad = 0;
        FiTarLayoutMake(&L, FiHash((unsigned)k));
        for (i = 0; i < FI_TAR_SPLATS; ++i) {
            const FiSplat *s = &L.s[i];
            if (!(s->u * s->u + s->v * s->v <= 0.93f * 0.93f) || s->kind != kFiSplatKind[i] || !(s->sc > 0.5f && s->sc < 1.7f) ||
                !(s->ang >= -3.1416f && s->ang <= 3.1416f) || !(s->off >= 0.0f && s->off <= 0.5f))
                bad = 1;
            if (s->drip) {
                if (s->drip < 1 || s->drip > L.nd || s->v < -0.25f || i == 0 || (slots >> s->drip & 1)) bad = 1;
                slots |= 1 << s->drip;
            }
        }
        for (j = 1; j <= L.nd; ++j)
            if ((L.d[j].kind != 2 && L.d[j].kind != 3) || !(L.d[j].len >= 160.0f && L.d[j].len <= 240.0f) || !(L.d[j].w >= 0.7f && L.d[j].w <= 1.3f))
                bad = 1;
        if (bad || L.nd > 12 || L.nd > FI_TAR_DRIPS || fabsf(L.s[0].u) > 0.15f || fabsf(L.s[0].v) > 0.15f) break;
    }
    CHECK(k == 200, "200 layouts: blots inside the plane, the game's pieces, drips 2 / 3 below the middle, slots 1 .. nd once (%d)", k);
    for (n = 0, k = 0; k < 200; ++k) {
        FiTarLayoutMake(&L, FiHash((unsigned)k + 7u));
        n += L.nd >= 5;
    }
    CHECK(n >= 190, "nearly every layout has 5 drips or more (%d of 200)", n);
    // a drip runs down at 90 u/s and slows to its length
    CHECK(FiDripLen(0.0f, 200.0f) == 0.0f && FiDripLen(-1.0f, 200.0f) == 0.0f && fabsf(FiDripLen(1.0f, 200.0f) - 90.0f) < 1e-3f,
          "90 u in the first second");
    okAll = 1, prevL = 0.0f;
    for (i = 1; i < 2000; ++i) {
        const float l = FiDripLen((float)i / 100.0f, 200.0f);
        if (l < prevL - 1e-3f || l > 200.0f) okAll = 0;
        prevL = l;
    }
    CHECK(okAll && prevL > 195.0f, "it never shrinks, gets near its length, never past it (%.1f)", prevL);
    CHECK(FiDripLen(5.0f, 30.0f) < 30.0f && FiDripLen(5.0f, 30.0f) > 25.0f, "a short drip too (%.2f)", FiDripLen(5.0f, 30.0f));
    // the strands snap one by one in 1.5 .. 2.5 s: each moment once
    for (n = 1; n <= FI_STR_MAX; ++n) {
        int bad = 0;
        memset(seen, 0, sizeof seen);
        for (i = 0; i < n; ++i) {
            const float at = FiSnapAt(i, n, 0xBEEFu), r = (at - FI_HOLD_SLOW_S) / (FI_HOLD_END_S - FI_HOLD_SLOW_S) * (float)n - 0.5f;
            const int rk = (int)(r + 0.5f);
            if (!(at > FI_HOLD_SLOW_S && at < FI_HOLD_END_S) || rk < 0 || rk >= n || seen[rk]++) bad = 1;
        }
        if (bad) break;
    }
    CHECK(n == FI_STR_MAX + 1, "1 .. 64 strands: each snaps at its own moment inside 1.5 .. 2.5 s (%d)", n);
    CHECK(FiSnapAt(0, 32, 1u) != FiSnapAt(1, 32, 1u) && FiSnapAt(5, 0, 1u) == FI_HOLD_SLOW_S, "neighbours apart; no strands: no moment");
    {
        float kk;
        CHECK(FiStrandPhase(1.0f, 2.0f, &kk) == 0 && kk == 1.0f, "whole before its moment");
        CHECK(FiStrandPhase(2.0f - 0.5f * FI_SNAP_STRAIN_S, 2.0f, &kk) == 1 && fabsf(kk - 0.7f) < 1e-4f, "straining: thinner");
        CHECK(FiStrandPhase(2.0f + 0.5f * FI_SNAP_RECOIL_S, 2.0f, &kk) == 2 && fabsf(kk - 0.75f) < 1e-4f, "snapped: whipping back");
        CHECK(FiStrandPhase(2.0f + FI_SNAP_RECOIL_S, 2.0f, &kk) == 3, "then gone");
    }
    // the plane
    {
        const float n1[3] = { 0.6f, 0.0f, 0.8f }, n2[3] = { 0.0f, 0.3f, 0.953939f }, up[3] = { 0.0f, -1.0f, 0.0f };
        const float *ns[2] = { n1, n2 };
        for (k = 0; k < 2; ++k) {
            const float *nn = ns[k];
            const int ok = FiPlaneAxes(nn, ax, ay);
            const float d1 = ax[0] * nn[0] + ax[1] * nn[1] + ax[2] * nn[2], d2 = ay[0] * nn[0] + ay[1] * nn[1] + ay[2] * nn[2];
            const float d3 = ax[0] * ay[0] + ax[1] * ay[1] + ax[2] * ay[2];
            const float l1 = ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2], l2 = ay[0] * ay[0] + ay[1] * ay[1] + ay[2] * ay[2];
            CHECK(ok && fabsf(d1) < 1e-5f && fabsf(d2) < 1e-5f && fabsf(d3) < 1e-5f && fabsf(l1 - 1.0f) < 1e-5f && fabsf(l2 - 1.0f) < 1e-5f &&
                  ax[1] == 0.0f && ay[1] > 0.9f, "plane %d: across level, down along +y, both unit and square to the normal", k);
        }
        CHECK(!FiPlaneAxes(up, ax, ay), "a floor / ceiling normal has no across: refused");
        CHECK(FiWallOk(n1) && FiWallOk(n2) && !FiWallOk(up), "walls only (a level-ish normal)");
    }
    CHECK(FI_TAR_DRIPS < 16 && FI_TAR_SPLATS == 25 && FI_STR_OWN >= 8 && FI_STR_OWN <= 16, "the game's 25 blots / 16 drip slots; 8 .. 16 own strands");
    CHECK(FI_HOLD_END_S + FI_SNAP_RECOIL_S < FiTarRelease(0.0f) + FI_TAR_CLOSE_S, "the last strand is back before the portal is gone");
}
// the stock exe: the code the tar portal calls, the step list entry it wraps, the offsets it relies on
static void TestTarExe(void)
{
    const unsigned char *p;
    int i, rel;
    unsigned v;
    for (i = 0; i < FI_TAR_NFN; ++i) {
        p = Img(kFiTarFns[i].va);
        CHECK(p && !memcmp(p, kFiTarFns[i].b, sizeof kFiTarFns[i].b), "%08X (%s) holds the 16 bytes fadein.inc checks", kFiTarFns[i].va,
              kFiTarFns[i].name);
    }
    p = Img(FI_PHASE_VA);
    memcpy(&v, p ? p : (const unsigned char *)"\0\0\0\0", 4);
    CHECK(p && v == FI_PHASE_FN, "0x62BC98 (phase 0's step 8) is 0x4EF900 (%08X)", v);
    p = Img(0x62BC94);
    memcpy(&v, p ? p : (const unsigned char *)"\0\0\0\0", 4);
    CHECK(p && v == 0x4EF670, "its neighbour 0x62BC94 is 0x4EF670 (EntityGate's entry 7)");
    p = Img(FI_PHASE_FN);  // 0x4EF900: call ActorDrawPass01; call ActorDrawModels; jmp ActorDrawPass2
    CHECK(p && p[0] == 0xE8 && p[5] == 0xE8 && p[10] == 0xE9, "0x4EF900 is two calls and a jump (pass 0/1, the models, pass 2)");
    if (p) {
        memcpy(&rel, p + 6, 4);
        CHECK(0x4EF905u + 5u + (unsigned)rel == 0x52AE50u, "its second call is ActorDrawModels 0x52AE50");
        memcpy(&rel, p + 11, 4);
        CHECK(0x4EF90Au + 5u + (unsigned)rel == 0x52AD70u, "its jump is ActorDrawPass2 0x52AD70");
    }
    // 0x482CC0 (the game's own start of the strands): push [esi+4C]; lea ecx,[esi+20]; lea edx,[esi+10]; call 0x478440;
    // mov byte [esi+7D],1 (GhostWork: the trail +0x4C, dir +0x20, pos +0x10, its state +0x7D)
    p = Img(0x482CC0);
    CHECK(p && !memcmp(p + 13, "\x8B\x46\x4C\x8D\x4E\x20\x51\x8D\x56\x10\x52\x50\xE8", 13) && !memcmp(p + 33, "\xC6\x46\x7D\x01", 4),
          "0x482CC0 starts the trail at GhostWork +0x4C with +0x10 / +0x20 and sets +0x7D = 1");
    if (p) {
        memcpy(&rel, p + 26, 4);
        CHECK(0x482CD9u + 5u + (unsigned)rel == 0x478440u, "0x482CC0 calls 0x478440");
    }
    p = Img(0x482CF0);
    CHECK(p && p[13] == 0x8A && p[14] == 0x46 && p[15] == 0x7D && !memcmp(p + 35, "\xC6\x46\x7D\x02", 4),
          "0x482CF0 reads GhostWork +0x7D and sets it to 2");
    p = Img(0x45BBF0);  // the once-a-tick latch: [[task+0x408]+0x5C] & 1
    CHECK(p && !memcmp(p, "\x8B\x44\x24\x04\x8B\x88\x08\x04\x00\x00\x8B\x41\x5C\xA8\x01", 15), "0x45BBF0 tests EnemyWork +0x5C bit 0");
    p = Img(0x4868B0);  // the grid draw reads [task+0x408], then the block's mode +0x124 (3 = nothing)
    CHECK(p && !memcmp(p + 17, "\x8B\xB0\x08\x04\x00\x00\x83\xBE\x24\x01\x00\x00\x03", 13), "0x4868B0: task+0x408, mode +0x124 == 3");
    p = Img(0x486E80);  // the blot draw reads [task+0x408], the fade time +0x3B4 and the count +0x3BC
    CHECK(p && !memcmp(p + 17, "\x8B\xB0\x08\x04\x00\x00", 6) && !memcmp(p + 35, "\xD8\xB6\xB4\x03\x00\x00\x0F\xBF\x86\xBC\x03\x00\x00", 13),
          "0x486E80: task+0x408, 1 / +0x3B4, the count +0x3BC");
    p = Img(0x4AD280);  // the victims' record draw: if active, 0x482D60 (which draws the trail when +0x7D != 0)
    if (p) memcpy(&rel, p + 20, 4);
    CHECK(p && p[19] == 0xE8 && 0x4AD293u + 5u + (unsigned)rel == 0x482D60u, "the victims' draw 0x4AD280 calls 0x482D60");
}

// ---------------------------------------------------------------- v3.58 FI358: the bigger, stickier portal
// a standing victim (sizes.md's idle Eileen: feet at y 0, y down) at x / z, the work position 200 u above the feet
static int MakeBody(float (*b)[3], float x, float z, float k)
{
    static const float kY[25] = { -184, -184, -178, -75, -26, -178, -75, -26, -184, -239, -263, -280, -258, -247, -200,
                                  -154, -140, -258, -247, -200, -154, -140, -8, -4, -8 };  // (bone 11 the head: 280)
    static const float kX[25] = { 0, 0, -12, -13, -14, 12, 13, 14, 0, 0, 0, 0, -10, -28, -33, -36, -38, 10, 28, 33, 36, 38,
                                  -14, 0, 14 };
    int i;
    for (i = 0; i < 25; ++i) b[i][0] = x + kX[i] * k, b[i][1] = kY[i] * k, b[i][2] = z + 4.0f * (float)(i % 3) * k;
    return 25;
}
static void TestBig(void)
{
    float bp[40][3], a, b, prev, tx, ty;
    const float work[3] = { 0.0f, -200.0f, 0.0f };
    FiBody B;
    FiBig G, G2;
    int i, k, n, okAll, seed, mx;
    // the hidden keys
    CHECK(FiStickClamp((float)NAN) == FI_STICK_DEFAULT && FiStickClamp(-1.0f) == FI_STICK_DEFAULT && FiStickClamp(0.0f) == 0.0f &&
          FiStickClamp(2.5f) == 2.5f && FiStickClamp(9.0f) == FI_STICK_MAX, "FadeInStrandSeconds: NaN / below 0 the default, 0 kept, at most 6");
    CHECK(FiCountClamp(-1, 9, 9) == 9 && FiCountClamp(0, 9, 9) == 0 && FiCountClamp(40, 16, 24) == 24 && FiCountClamp(5, 9, 9) == 5,
          "the counts: below 0 the default, 0 kept, at most the top");
    // the strands' end and their snap window
    CHECK(FiStickEnd(0.8f, 0.0f) == FI_HOLD_END_S && FiStickEnd(0.8f, (float)NAN) == FI_HOLD_END_S, "stick 0 / NaN: v3.57's 2.5 s");
    CHECK(fabsf(FiStickEnd(0.8f, 3.0f) - 3.8f) < 1e-5f, "defaults: the strands hold 3 s after the body is in (3.8 s)");
    CHECK(FiStickEnd(0.2f, 1.0f) == FI_HOLD_END_S, "never before the hold's end");
    FiStickWindow(FI_HOLD_END_S, &a, &b);
    CHECK(a == FI_HOLD_SLOW_S && b == FI_HOLD_END_S, "v3.57's window 1.5 .. 2.5 s");
    FiStickWindow(3.8f, &a, &b);
    CHECK(fabsf(a - 2.5f) < 1e-5f && b == 3.8f, "the last 1.3 s (2.5 .. 3.8)");
    FiStickWindow(2.6f, &a, &b);
    CHECK(a == FI_HOLD_SLOW_S && b == 2.6f, "the window never starts before 1.5 s");
    for (n = 1, okAll = 1; n <= FI_STRANDS_MAX; ++n) {
        int seen[FI_STRANDS_MAX];
        memset(seen, 0, sizeof seen);
        for (i = 0; i < n; ++i) {
            const float at = FiSnapAtW(i, n, 0x1234u, 2.5f, 3.8f);
            const int r = (int)floorf((at - 2.5f) / 1.3f * (float)n);
            if (!(at > 2.5f && at < 3.8f) || r < 0 || r >= n || seen[r]++) okAll = 0;
        }
    }
    CHECK(okAll, "FiSnapAtW: every strand at its own moment inside the window (1 .. 24 strands)");
    for (i = 0, okAll = 1; i < 32; ++i)
        if (FiSnapAt(i, 32, 77u) != FiSnapAtW(i, 32, 77u, FI_HOLD_SLOW_S, FI_HOLD_END_S)) okAll = 0;
    CHECK(okAll, "FiSnapAt is v3.57's window");
    // the body's box from its bones
    n = MakeBody(bp, 0.0f, 0.0f, 1.0f);
    CHECK(FiBodyBones((const float(*)[3])bp, n, work, &B) && B.bones == 25, "a standing victim's 25 bones");
    CHECK(FiBodyH(&B) > 300.0f && FiBodyH(&B) < 345.0f, "its height %.0f (the mesh top ~314 above the feet)", FiBodyH(&B));
    CHECK(B.bottom > -5.0f && B.bottom < 25.0f && B.top < -280.0f, "its box from the soles (%.0f) to above the head (%.0f)", B.bottom, B.top);
    CHECK(B.half > 30.0f && B.half < 90.0f && fabsf(B.c[0]) < 5.0f, "its half width %.0f, centred", B.half);
    bp[3][0] = 5000.0f, bp[4][1] = (float)NAN;
    CHECK(FiBodyBones((const float(*)[3])bp, n, work, &B) && B.bones == 23, "a bone far away or not finite is left out");
    CHECK(!FiBodyBones((const float(*)[3])bp, 2, work, &B) && !B.ok, "fewer than 3 bones: not usable");
    for (i = 0; i < n; ++i) bp[i][0] = bp[i][1] = bp[i][2] = 0.0f;  // (a ghost made this frame: no bones drawn yet)
    {   // (the user's Jasper at -62 -1000 404, 2026-11-27: its first frame's bones at the origin)
        const float jw[3] = { -62.0f, -1000.0f, 404.0f };
        CHECK(!FiBodyBones((const float(*)[3])bp, n, jw, &B), "bones at the origin, 1000 u off its position: not usable");
    }
    FiBodyType(0x0B, work, &B);
    CHECK(B.ok && fabsf(FiBodyH(&B) - 340.0f) < 0.01f && B.bottom == 0.0f && !B.bones, "a victim by its type: 340 u, feet 200 below");
    FiBodyType(0x0E, work, &B);
    CHECK(fabsf(FiBodyH(&B) - 420.0f) < 0.01f, "Andrew by his type: 420 u");
    // the whole portal's size for a body
    n = MakeBody(bp, 0.0f, 0.0f, 1.0f);
    FiBodyBones((const float(*)[3])bp, n, work, &B);
    FiBigTarget(&B, 100.0f, 150.0f, &tx, &ty);
    CHECK(fabsf(ty - FI_COVER_Y * FiBodyH(&B)) < 0.01f && 2.0f * ty > FiBodyH(&B) * 1.2f, "on a wall: %.0f u tall for a body of %.0f (1.24 x)",
          2.0f * ty, FiBodyH(&B));
    CHECK(tx >= FI_COVER_X * FiBodyH(&B) - 0.01f && tx >= 100.0f, "and %.0f u wide", 2.0f * tx);
    FiBigTarget(&B, 60.0f, 95.0f, &tx, &ty);
    CHECK(2.0f * ty > 1.2f * FiBodyH(&B) && ty <= FI_COVER_MAXK * 95.0f && tx <= FI_COVER_MAXK * 60.0f, "in the air: 1.24 x too, at most 3 x the first");
    n = MakeBody(bp, 0.0f, 0.0f, 0.3f);  // a small body (a crawling one): never smaller than the first portal
    FiBodyBones((const float(*)[3])bp, n, work, &B);
    FiBigTarget(&B, 100.0f, 150.0f, &tx, &ty);
    CHECK(tx == 100.0f && ty == 150.0f, "a small body: the first portal alone (%.0f x %.0f)", tx, ty);
    FiBigMake(&G, 5u, 100.0f, 150.0f, tx, ty, 0.0f, 0.0f, FI_PIECES_MAX);
    CHECK(G.n == 1 && G.sets == 1, "nothing to add around it (%d pieces)", G.n);
    // the layout: never a piece bigger than the first, all inside the target, the target covered, the counts
    okAll = 1;
    for (seed = 0; seed < 400; ++seed)
        for (mx = 0; mx <= FI_PIECES_MAX; ++mx) {
            const float sx = seed & 1 ? 60.0f : 100.0f, sy = seed & 1 ? 95.0f : 150.0f, h = 150.0f + (float)(seed % 9) * 55.0f;
            const float du = (float)(seed % 7 - 3) * 20.0f, dv = (float)(seed % 5 - 2) * 15.0f;
            float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f }, ttx = FI_COVER_X * h, tty = FI_COVER_Y * h;
            if (ttx < sx) ttx = sx;
            if (tty < sy) tty = sy;
            if (ttx > 3.0f * sx) ttx = 3.0f * sx;
            if (tty > 3.0f * sy) tty = 3.0f * sy;
            FiBigMake(&G, (unsigned)seed, sx, sy, ttx, tty, du, dv, mx);
            if (G.n < 1 || G.n > mx + 1 || G.g[0].s != 1.0f || G.nOn != G.n) okAll = 0;
            if (G.sets != (G.n == 1 ? 1 : G.n - 1 >= 5 ? 3 : 2)) okAll = 0;
            for (k = 0; k < G.n; ++k) {
                const FiPiece *g = &G.g[k];
                if (k && !(g->s >= 0.5f && g->s <= 0.8f)) okAll = 0;  // never stretched past the first
                if (g->u - g->s * sx < G.du - G.tx - 0.01f || g->u + g->s * sx > G.du + G.tx + 0.01f) okAll = 0;
                if (g->v - g->s * sy < G.dv - G.ty - 0.01f || g->v + g->s * sy > G.dv + G.ty + 0.01f) okAll = 0;
                if (k && !(g->open > 0.0f && g->open < 0.6f)) okAll = 0;
                if (g->u - g->s * sx < lo[0]) lo[0] = g->u - g->s * sx;
                if (g->u + g->s * sx > hi[0]) hi[0] = g->u + g->s * sx;
                if (g->v - g->s * sy < lo[1]) lo[1] = g->v - g->s * sy;
                if (g->v + g->s * sy > hi[1]) hi[1] = g->v + g->s * sy;
            }
            if (mx == FI_PIECES_MAX && (hi[0] - lo[0] < 1.8f * G.tx || hi[1] - lo[1] < 1.8f * G.ty)) okAll = 0;  // 90 % covered
            if (mx == 0 && G.n != 1) okAll = 0;
            FiBigMake(&G2, (unsigned)seed, sx, sy, ttx, tty, du, dv, mx);
            if (memcmp(&G, &G2, sizeof G)) okAll = 0;
        }
    CHECK(okAll, "FiBigMake (400 seeds x 0 .. 9 pieces, wall / air, bodies 150 .. 590): the first at its size, the rest 0.5 .. 0.8 "
                 "of it, all inside the target, 90 %% of it spanned with 9, the counts, the same twice");
    FiBigMake(&G, 7u, 100.0f, 150.0f, 141.0f, 209.0f, 0.0f, -20.0f, FI_PIECES_MAX);
    CHECK(G.n == 10 && G.sets == 3, "a victim on a wall: the first + 9 (%d), 3 blot sets", G.n);
    CHECK(G.g[1].v > G.g[0].v && G.g[2].v < G.g[0].v && G.g[3].u < 0.0f && G.g[4].u > 0.0f, "below, above, the sides first");
    CHECK(FiBigCovers(&G, G.g[0].u, G.g[0].v, 100.0f, 150.0f) && FiBigCovers(&G, G.g[5].u, G.g[5].v, 100.0f, 150.0f) &&
          !FiBigCovers(&G, 400.0f, 0.0f, 100.0f, 150.0f), "FiBigCovers: the pieces' centres in, far away out");
    for (k = 1; k < G.n; ++k) G.g[k].on = 0;  // (a wall that ends: every piece around the first left out)
    CHECK(!FiBigCovers(&G, G.g[1].u, G.g[1].v + 0.9f * G.g[1].s * 150.0f, 100.0f, 150.0f) &&
          FiBigCovers(&G, G.g[0].u, G.g[0].v, 100.0f, 150.0f), "a piece left out covers nothing (the first still does)");
    FiBigMake(&G, 7u, 100.0f, 150.0f, 141.0f, 150.0f, 0.0f, 0.0f, FI_PIECES_MAX);
    CHECK(G.n >= 3 && G.g[1].slot == 2 && G.g[2].slot == 3, "wider only: the sides (no piece above / below)");
    // the pieces' amounts: they open one after another, close faster than the first (it shrinks to its centre)
    FiBigMake(&G, 7u, 100.0f, 150.0f, 141.0f, 209.0f, 0.0f, 0.0f, FI_PIECES_MAX);
    CHECK(FiPieceAmount(0.05f, 3.8f, &G.g[3], 0) == 0.0f && FiPieceAmount(1.0f, 3.8f, &G.g[3], 0) == 1.0f, "a piece opens after its delay");
    CHECK(FiPieceAmount(2.0f, 3.8f, &G.g[0], 1) == FiTarAmount(2.0f, 3.8f), "the first: v3.57's curve");
    CHECK(FiPieceAmount(3.8f + FI_PIECE_CLOSE_S, 3.8f, &G.g[3], 0) == 0.0f && FiTarAmount(3.8f + FI_PIECE_CLOSE_S, 3.8f) > 0.0f,
          "when the pieces around are gone the first is still closing");
    for (i = 0, okAll = 1, prev = 2.0f; i <= 200; ++i) {
        const float t = 3.8f + (float)i * 0.01f, v = FiPieceAmount(t, 3.8f, &G.g[4], 0);
        if (v > prev + 1e-6f || v < 0.0f || v > 1.0f) okAll = 0;
        prev = v;
    }
    CHECK(okAll, "a piece's close never grows");
    // the budget
    CHECK(FiPieceKeep(1.0f, 0, 9) == 9.0f && fabsf(FiPieceKeep(0.6f, 0, 9) - 5.4f) < 1e-4f && fabsf(FiPieceKeep(0.35f, 0, 9) - 3.15f) < 1e-4f,
          "the spark levels keep 100 / 60 / 35 %% of the pieces");
    CHECK(fabsf(FiPieceKeep(1.0f, 1, 9) - 3.15f) < 1e-4f && FiPieceKeep(0.1f, 0, 9) == FiPieceKeep(0.35f, 0, 9) &&
          FiPieceKeep((float)NAN, 0, 4) == 4.0f, "a deep level 35 %%, never below 35 %%, NaN = all");
    for (i = 0, a = 0.0f; i < 9; ++i) a += FiPieceWeight(5.4f, i + 1);
    CHECK(fabsf(a - 5.4f) < 1e-4f && FiPieceWeight(5.4f, 1) == 1.0f && fabsf(FiPieceWeight(5.4f, 6) - 0.4f) < 1e-4f &&
          FiPieceWeight(5.4f, 7) == 0.0f, "the weights add up to the share: the last kept one fades (continuous)");
    CHECK(FiSetWeight(1.0f, 0, 0) == 1.0f && FiSetWeight(1.0f, 0, 1) == 1.0f && FiSetWeight(1.0f, 0, 2) == 1.0f, "all three sets at full");
    CHECK(FiSetWeight(0.6f, 0, 1) > 0.5f && FiSetWeight(0.6f, 0, 2) == 0.0f && FiSetWeight(0.35f, 0, 1) == 0.0f &&
          FiSetWeight(0.35f, 1, 0) == 1.0f, "60 %%: the outer set goes; 35 %%: the second too; the first always");
    {   // the portals' own limit: 8 %% of the frame (180 fps: 0.44 ms; 60 fps: 1.33 ms)
        float kk = 1.0f;
        for (i = 0; i < 180; ++i) kk = FiSelfKeepStep(kk, 0.6f, 1000.0f / 180.0f, 1.0f / 180.0f);
        CHECK(kk == 0.35f, "180 fps, the portals at 0.6 ms a frame: the pieces around thin to 35 %% within a second (%.2f)", kk);
        for (i = 0; i < 180; ++i) kk = FiSelfKeepStep(kk, 0.2f, 1000.0f / 180.0f, 1.0f / 180.0f);
        CHECK(fabsf(kk - 0.65f) < 0.01f, "under 60 %% of the limit they come back at 0.3 a second (%.2f)", kk);
        kk = 1.0f;
        for (i = 0; i < 60; ++i) kk = FiSelfKeepStep(kk, 0.6f, 1000.0f / 60.0f, 1.0f / 60.0f);
        CHECK(kk == 1.0f && FiSelfKeepStep(0.5f, 9.0f, 5.0f, 0.0f) == 0.5f && FiSelfKeepStep((float)NAN, 0.0f, 5.0f, 0.01f) == 1.0f,
              "60 fps: 0.6 ms is under its 1.33 ms; no time step: no change; NaN: all");
        kk = 0.6f;
        CHECK(FiSelfKeepStep(kk, 0.30f, 1000.0f / 180.0f, 0.01f) == 0.6f, "between 60 and 100 %% of the limit it holds");
    }
    CHECK(FiLifeAtWeight(1.0f, 1.0f) == 1.0f && FiLifeAtWeight(0.3f, 0.0f) < -FI_TAR_FADE_S && fabsf(FiTarLifeAlpha(FiLifeAtWeight(1.0f, 0.5f)) - 0.5f) < 1e-3f &&
          FiLifeAtWeight(-1.0f, 0.9f) == -1.0f, "a blot's life at a weight: whole -> that alpha, a fading one stays as faded");
    // the strands' look
    CHECK(fabsf(FiStickWidth(0.0f, 3.8f, 200.0f, 200.0f) - 1.0f) < 1e-5f && fabsf(FiStickWidth(3.8f, 3.8f, 200.0f, 200.0f) - 0.5f) < 1e-5f,
          "a strand thins to half over its time");
    CHECK(fabsf(FiStickWidth(0.0f, 3.8f, 800.0f, 200.0f) - 0.5f) < 1e-5f && fabsf(FiStickWidth(0.0f, 3.8f, 9000.0f, 200.0f) - 0.35f) < 1e-5f &&
          FiStickWidth(0.0f, 3.8f, 100.0f, 200.0f) == 1.0f, "and as it stretches (4 x: half; never below 35 %%; shorter: as it was)");
    for (i = 0, okAll = 1, prev = 2.0f; i <= 100; ++i) {
        const float v = FiStickWidth((float)i * 0.05f, 3.8f, 300.0f, 200.0f);
        if (v > prev + 1e-6f || !(v > 0.0f)) okAll = 0;
        prev = v;
    }
    CHECK(okAll, "it never gets wider as it ages");
    CHECK(FiStickSag(3.8f, 3.8f, 5000.0f) == 160.0f && FiStickSag(0.0f, 3.8f, 200.0f) < FiStickSag(3.0f, 3.8f, 200.0f) &&
          fabsf(FiStickSag(0.0f, 3.8f, 200.0f) - 10.0f) < 1e-4f, "it sags more as it ages, at most 160 u");
    CHECK(FiDropFall(0.0f) == 0.0f && FiDropLeft(0.0f) == 1.0f && FiDropLeft(FI_DROP_S) == 0.0f && fabsf(FiDropFall(0.5f) - 112.5f) < 1e-3f,
          "a dropped end falls (112 u in 0.5 s) and is gone after 0.6 s");
    // the timeline with the defaults: the hold as v3.57, the strands to 3.8 s, the portal open as long, closed by 5.4 s
    {
        const float sEnd = FiStickEnd(FI_SECONDS_DEFAULT, FI_STICK_DEFAULT);
        CHECK(FiHoldFactor(FI_HOLD_END_S) == 1.0f && FiTarAmount(sEnd - 0.01f, sEnd) == 1.0f && FiTarAmount(sEnd + 0.8f, sEnd) > 0.0f &&
              FiTarDone(sEnd + FI_TAR_CLOSE_S, sEnd) && !FiTarDone(sEnd + 1.0f, sEnd),
              "the hold ends at 2.5 s; the portal stays open to %.1f s and has closed at %.1f s", sEnd, sEnd + FI_TAR_CLOSE_S);
    }
    // the close line (tools\logcheck.py reads it)
    {
        FiClose c;
        char buf[480], tiny[12];
        memset(&c, 0, sizeof c);
        c.id = 0xFFFF0002u, c.type = 0x0C, c.at = 5.4f, c.grids = 10, c.gridsOf = 10, c.sets = 3, c.drips = 31;
        c.w = 282.0f, c.h = 418.0f, c.body = 337.0f, c.bones = 1, c.strGame = 32, c.strOwn = 8, c.snapped = 7, c.loose = 1;
        c.stick = 3.0f, c.stickTo = 3.8f, c.ms = 30.0, c.msMax = 0.41f, c.frames = 100;
        FiCloseText(&c, buf, sizeof buf);
        CHECK(!strcmp(buf, "FFFF0002 (type 0C): its tar portal closed at 5.4 s: 13 piece(s) (grids 10 of 10, blot sets 3, drips 31), "
                           "282 x 418 u for a body 337 u tall (1.24 x, its bones); strands: the ghost's own 32, Refined's 8 (its bones), "
                           "held 3.0 s after it was in (to 3.8 s; 7 snapped, 1 stretched loose); portal draw 0.300 ms a frame (max 0.410, "
                           "100 frames)"), "the close line: %s", buf);
        c.bones = -1, c.body = 0.0f, c.ownBox = 1;
        FiCloseText(&c, buf, sizeof buf);
        CHECK(strstr(buf, "for a body 0 u tall (0.00 x, not seen)") && strstr(buf, "Refined's 8 (its box)"), "not seen / its box: %s", buf);
        CHECK(FiCloseText(&c, tiny, sizeof tiny) == (int)sizeof tiny - 1 && tiny[sizeof tiny - 1] == 0, "a short buffer stays terminated");
    }
}

// v3.58 FI358: "--dump": the layouts of four arrivals for tools\portal_sheet.py (an SVG sheet: the pieces, the body, the target)
static void DumpBig(const char *name, float sx, float sy, const FiBody *B, float cy, unsigned seed)
{
    FiBig G;
    float tx, ty;
    int k;
    FiBigTarget(B, sx, sy, &tx, &ty);
    FiBigMake(&G, seed, sx, sy, tx, ty, B->c[0], B->c[1] - cy, FI_PIECES_MAX);
    printf("CASE %s %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %d\n", name, sx, sy, G.tx, G.ty, G.du, G.dv, B->top - cy,
           B->bottom - cy, B->half, G.sets);
    for (k = 0; k < G.n; ++k) printf("PIECE %d %.1f %.1f %.3f %.2f\n", k, G.g[k].u, G.g[k].v, G.g[k].s, G.g[k].open);
}
static int Dump(void)
{
    float bp[40][3];
    const float work[3] = { 0.0f, -200.0f, 0.0f };
    FiBody B;
    int n = MakeBody(bp, 0.0f, 0.0f, 1.0f);
    FiBodyBones((const float(*)[3])bp, n, work, &B);
    DumpBig("victim_on_a_wall", 100.0f, 150.0f, &B, -200.0f, 11u);
    DumpBig("victim_in_the_air", 60.0f, 95.0f, &B, -200.0f, 12u);
    FiBodyType(0x0E, work, &B);
    DumpBig("Andrew_by_type_on_a_wall", 100.0f, 150.0f, &B, -200.0f, 13u);
    n = MakeBody(bp, 0.0f, 0.0f, 0.55f);
    FiBodyBones((const float(*)[3])bp, n, work, &B);
    DumpBig("crouching_in_the_air", 60.0f, 95.0f, &B, -200.0f, 14u);
    return 0;
}

int main(int argc, char **argv)
{
    const char *exe = argc > 1 ? argv[1] : "C:\\ClaudeCode\\CModding\\SH4Decomp\\orig\\sh4.exe";
    if (argc > 1 && !strcmp(argv[1], "--dump")) return Dump();  // v3.58 FI358
    TestTexts();
    TestWho();
    TestRules();
    TestCurves();
    TestClock();
    TestTable();
    TestNotes();
    TestText();
    TestExe(exe);
    TestTar();      // v3.51 PT351
    TestTarExe();   // v3.51 PT351 (after TestExe: the image is loaded)
    TestBig();      // v3.58 FI358
    printf("%s: %d checks, %d failed\n", g_fail ? "FAIL" : "ok", g_n, g_fail);
    return g_fail ? 1 : 0;
}
