// v3.31 DEPTH331: depthfix_sim.h native test (staging/v331_feedback/w3_depth/PLAN.md): the GS TEST decode of 0x404070
// (the light grid's 0x3000D overwrites depth, the rooms' 0x5000D and the ambient tint's 0x53003 do not), the light
// grid's stored depth and the view depth it stands for, the restore rule (a GREATEREQUAL depth-only redraw) on sprites
// behind / in front of her at the camera distances of the game, in an overwritten room and in a true-depth room, the
// body classification, and the once-a-frame gate at 30 / 60 / 120 / 180 fps.
// Build and run: test\build_depthfix_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <math.h>
#include "../src/depthfix_sim.h"

static int g_fail, g_n;
static void Check(int ok, const char *what)
{
    ++g_n;
    if (!ok) {
        printf("FAIL  %s\n", what);
        ++g_fail;
    }
}
static int Near(float a, float b, float eps) { return fabsf(a - b) <= eps; }

// the camera projection the user's logs show in every room (KB 147): n 50, f 40000
static const float kP22 = -50.0f / 39950.0f, kP32 = 40000.0f * 50.0f / 39950.0f;

static void TestTest(void)
{
    DfxZState s = DfxGsTestZ(0x3000D);  // the light grid (0x433C70)
    Check(s.zEnable && s.zWrite && s.zFunc == 8, "0x3000D (light grid): Z on, written, ALWAYS");
    Check(DfxGsTestOverwrites(0x3000D), "0x3000D overwrites the depth");
    s = DfxGsTestZ(0x5000D);  // the rooms (0x422600)
    Check(s.zEnable && s.zFunc == 7 && !DfxGsTestOverwrites(0x5000D), "0x5000D (rooms): GREATEREQUAL, not an overwrite");
    s = DfxGsTestZ(0x53003);  // the ambient motes / tint (0x437D40, with 0x137F978 = 1)
    Check(s.zEnable && s.zFunc == 7 && !DfxGsTestOverwrites(0x53003), "0x53003 (ambient tint): GREATEREQUAL");
    s = DfxGsTestZ(0x0000D);
    Check(!s.zEnable && !s.zWrite && !DfxGsTestOverwrites(0xD), "no ZTE: Z off, no write");
    Check(DfxGsTestZ(0x10000).zFunc == 1 && DfxGsTestZ(0x70000).zFunc == 5 && DfxGsTestZ(0x90000).zFunc == 4,
          "ZTST 0 / 3 / 4: NEVER / GREATER / LESSEQUAL (the 0x4042A4 jump table)");
    Check(DfxGsTestZ(0xB0000).zFunc == 8, "ZTST 5 (no GS meaning): ALWAYS (the switch default)");
}
static void TestGridDepth(void)
{
    const float z = DfxGsDepth(10000.0f, 1.0f / 16777216.0f, 0.0f), w = DfxViewDepthOf(z, kP22, kP32);
    char b[128];
    Check(Near(z, 5.9605e-4f, 1e-7f), "the light grid's GS z 10000 stores 0.000596");
    _snprintf_s(b, sizeof b, _TRUNCATE, "it stands for a wall at view depth ~27 100 (got %.0f)", w);
    Check(w > 26900.0f && w < 27300.0f, b);
    {
        int k, ok = 1;
        for (k = 0; k < 40; ++k) {
            const float wv = 60.0f * powf(1.15f, (float)k);  // 60 .. ~14 000
            if (!Near(DfxViewDepthOf(DfxDepthAt(wv, kP22, kP32), kP22, kP32), wv, wv * 1e-3f)) ok = 0;
        }
        Check(ok, "view depth <-> stored depth round trip, 60 .. 14 000 units");
    }
    Check(DfxViewDepthOf(kP22, kP22, kP32) == 0.0f && DfxViewDepthOf(-1.0f, kP22, kP32) == 0.0f, "no view depth at / beyond far");
}
// The buffer at 0x4EFBFD at her pixel, a sprite's test (reversed Z: GREATEREQUAL passes nearer-or-equal), and the
// restore (her depth-only redraw, GREATEREQUAL with a z write).
static float Restore(float stored, float zHer) { return zHer >= stored ? zHer : stored; }
static int Passes(float zSprite, float stored) { return zSprite >= stored; }
static void TestRestoreRule(void)
{
    static const float kDist[] = { 150.0f, 300.0f, 500.0f, 800.0f, 1200.0f, 2000.0f, 3500.0f };  // camera to her
    const float zGrid = DfxGsDepth(10000.0f, 1.0f / 16777216.0f, 0.0f);
    int i, bugBefore = 1, hiddenAfter = 1, frontAfter = 1, trueRoom = 1, shadowAfter = 1;
    for (i = 0; i < (int)(sizeof kDist / sizeof kDist[0]); ++i) {
        const float w = kDist[i], zHer = DfxDepthAt(w, kP22, kP32);
        const float zBehind = DfxDepthAt(w + 20.0f, kP22, kP32);   // a cloak sprite pushed 18..25 u behind a bone (DpAway)
        const float zFront = DfxDepthAt(w - 30.0f, kP22, kP32);    // a wisp between her and the camera
        const float zShadow = DfxDepthAt(w + 120.0f, kP22, kP32);  // an Echo shadow standing behind her
        const float zSky = 0.0f;                                     // a cleared pixel (no surface)
        // an interior: the grid overwrote everything after the 3D pass
        if (!Passes(zBehind, zGrid)) bugBefore = 0;                  // v3.30: the sprite behind her passes (the bug)
        if (Passes(zBehind, Restore(zGrid, zHer))) hiddenAfter = 0;  // restored: hidden behind her
        if (Passes(zShadow, Restore(zGrid, zHer))) shadowAfter = 0;
        if (!Passes(zFront, Restore(zGrid, zHer))) frontAfter = 0;   // in front: still drawn
        // stage 5 room 4: the buffer already holds her own depth: the redraw ties and nothing changes
        if (Restore(zHer, zHer) != zHer || Passes(zBehind, Restore(zHer, zHer)) || !Passes(zFront, Restore(zHer, zHer))) trueRoom = 0;
        (void)zSky;
    }
    Check(bugBefore, "an overwritten room: a sprite behind her passed over her body at every distance (v3.30, the report)");
    Check(hiddenAfter, "restored: the sprite behind her is hidden by her body (150 .. 3 500 units)");
    Check(shadowAfter, "restored: an Echo shadow behind her is hidden by her body");
    Check(frontAfter, "restored: a sprite in front of her still draws over her");
    Check(trueRoom, "a true-depth room (5:4): the redraw ties, nothing changes");
    {   // far beyond any room: her z drops below the grid's and the redraw writes nothing (the v3.30 state stays)
        const float zFar = DfxDepthAt(30000.0f, kP22, kP32);
        Check(Restore(zGrid, zFar) == zGrid, "beyond ~27 100 units the restore does nothing (no harm)");
    }
}
static void TestKind(void)
{
    char her, wep, eil, cyn, other, mE, mC, t;
    Check(DfxKind(&her, &t, &her, &wep, &eil, &cyn) == DFK_PLAYER, "the player's model");
    Check(DfxKind(&wep, &t, &her, &wep, &eil, &cyn) == DFK_WEAPON, "her weapon's model");
    Check(DfxKind(&mE, &eil, &her, &wep, &eil, &cyn) == DFK_EILEEN, "the escort Eileen (by task)");
    Check(DfxKind(&mC, &cyn, &her, &wep, &eil, &cyn) == DFK_CYNTHIA, "Cynthia (by task)");
    Check(DfxKind(&other, &t, &her, &wep, &eil, &cyn) == -1, "an enemy / prop: none");
    Check(DfxKind(NULL, &t, &her, &wep, &eil, &cyn) == -1, "no model: none");
    Check(DfxKind(&mE, &t, &her, NULL, NULL, NULL) == -1, "no weapon / no companions: only the player");
    Check(DfxKind(&her, &t, &other, &wep, &eil, &cyn) == -1, "Playable Eileen switched the player model: the old one is not hers");
}
// once a frame at any frame rate, whatever the order and the number of callers; Echo's Spirit Vision draw never
static void GateAt(int fps)
{
    DfxGate g;
    long f;
    int runs = 0, echoSv = 0, dpSv = 0, extra = 0;
    char b[96];
    DfxGateInit(&g, 1, 1);
    for (f = 0; f < 2 * fps; ++f) {
        const int look = (int)(f / (fps / 2)) & 3;  // 0, 1, 2, 3 for half a second each
        int got = 0;
        if (look == 3) {  // GvPass: Echo first (before Spirit Vision's own sprites), then Death Presence (after them)
            if (DfxGateRun(&g, f, DFW_ECHO, 3)) ++echoSv;
            if (DfxGateRun(&g, f, DFW_DP, 3)) ++dpSv, ++got;
        } else {          // SparkleFrame / FxPass: Echo, then Death Presence, then a second Death Presence call
            got += DfxGateRun(&g, f, DFW_ECHO, look);
            got += DfxGateRun(&g, f, DFW_DP, look);
            got += DfxGateRun(&g, f, DFW_DP, look);
        }
        if (got > 1) ++extra;
        runs += got;
    }
    _snprintf_s(b, sizeof b, _TRUNCATE, "%d fps: one restore a frame (%d in %d frames), never twice", fps, runs, 2 * fps);
    Check(runs == 2 * fps && !extra, b);
    _snprintf_s(b, sizeof b, _TRUNCATE, "%d fps: Echo's Spirit Vision draw never restores; Death Presence's does", fps);
    Check(!echoSv && dpSv == fps / 2, b);
}
static void TestGate(void)
{
    DfxGate g;
    DfxGateInit(&g, 0, 1);
    Check(!DfxGateRun(&g, 5, DFW_DP, 0), "PlayerDepthRestore=0: never");
    DfxGateInit(&g, 1, 0);
    Check(!DfxGateRun(&g, 5, DFW_ECHO, 0), "not installed (no draw-call override / model hook): never");
    GateAt(30), GateAt(60), GateAt(120), GateAt(180);
    Check(!DfxCollectWanted(10, -100), "the collector is idle before any restore was asked (features off: v3.30)");
    Check(DfxCollectWanted(10, 9) && DfxCollectWanted(10, 10) && DfxCollectWanted(10 + DFX_ASK_FRAMES, 10),
          "the collector runs for a few frames after an ask");
    Check(!DfxCollectWanted(11 + DFX_ASK_FRAMES, 10), "and stops when Death Presence / Echo stopped drawing");
}
int main(void)
{
    TestTest();
    TestGridDepth();
    TestRestoreRule();
    TestKind();
    TestGate();
    printf("depthfix_sim: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
