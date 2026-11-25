// v3.31 DEPTH331: the pure part of the player depth restore (depthfix.inc; staging/v331_feedback/w3_depth/PLAN.md).
// No Windows, no device: the GS TEST decode of the port's 0x404070, the 2D depth of a GS Z, the view depth a stored
// depth stands for, which collected model is restored, and the once-a-frame gate. Native test: test\depthfix_sim_test.c.
#ifndef DEPTHFIX_SIM_H
#define DEPTHFIX_SIM_H

// what 0x404070 (RenderSetGsTest, matched decomp src/render/render_403870.c) makes of a GS TEST word's depth part:
// ZTE (bit 16) -> ZENABLE 1 and ZWRITEENABLE 1 (the GS ZMSK is ignored); ZTST (bits 17-19 & 7) 0..4 -> D3D ZFUNC
// NEVER (1) / ALWAYS (8) / GREATEREQUAL (7) / GREATER (5) / LESSEQUAL (4); ZTE off -> Z off and no write
typedef struct { int zEnable, zWrite, zFunc; } DfxZState;
static DfxZState DfxGsTestZ(unsigned long long test)
{
    static const int kFunc[5] = { 1, 8, 7, 5, 4 };
    DfxZState s;
    const int t = (int)((test >> 17) & 7);
    s.zEnable = (test & 0x10000) != 0;
    s.zWrite = s.zEnable;
    s.zFunc = s.zEnable ? (t < 5 ? kFunc[t] : 8) : 0;
    return s;
}
// a 2D batch that overwrites the depth of everything under it: Z on, written, compared ALWAYS
static int DfxGsTestOverwrites(unsigned long long test)
{
    const DfxZState s = DfxGsTestZ(test);
    return s.zEnable && s.zWrite && s.zFunc == 8;
}
// the depth a 2D vertex stores: the GS canvas shaders (0x5D56C0 / 0x5D57F8) give z = GS Z x c2.z + c3.z (c2.z = 2^-24)
static float DfxGsDepth(float gsZ, float c2z, float c3z) { return gsZ * c2z + c3z; }
// the 3D pass stores z = P22 + P32 / w (reversed: near 1, far 0; KB 147); the view depth w a stored z stands for
// (0 = none: z at or below the far plane's value)
static float DfxViewDepthOf(float z, float p22, float p32)
{
    const float d = z - p22;
    return d > 0.0f && p32 > 0.0f ? p32 / d : 0.0f;
}
static float DfxDepthAt(float w, float p22, float p32) { return w > 0.0f ? p22 + p32 / w : 0.0f; }

// which body a model drawn at 0x52AEB5 is: the player ([0x10A0F20 + 0x1B0]), her weapon ([+0x340]), the escort
// Eileen (task [0xFCE6B4]) or Cynthia (task [0xFD5A50]); -1 = none of them
enum { DFK_PLAYER, DFK_WEAPON, DFK_EILEEN, DFK_CYNTHIA, DFK_N };
static int DfxKind(const void *model, const void *task, const void *her, const void *wep, const void *eil, const void *cyn)
{
    if (!model) return -1;
    if (model == her) return DFK_PLAYER;
    if (wep && model == wep) return DFK_WEAPON;
    if (task && eil && task == eil) return DFK_EILEEN;
    if (task && cyn && task == cyn) return DFK_CYNTHIA;
    return -1;
}

// the once-a-frame gate. who: 0 Death Presence, 1 Echo; look: 0 normal, 1 Thermal, 2 night vision, 3 Spirit Vision.
// Echo's Spirit Vision draw comes before Spirit Vision's own sprites (calibrated x 1/128, screenfx.inc GvPass): it does
// not restore there (her soul fire would dim over her body); Death Presence's Spirit Vision draw comes after them.
enum { DFW_DP, DFW_ECHO };
typedef struct { long done; int on, ready; } DfxGate;
static void DfxGateInit(DfxGate *g, int on, int ready) { g->done = -1; g->on = on; g->ready = ready; }
static int DfxGateRun(DfxGate *g, long frame, int who, int look)  // 1 = restore now (and not again this frame)
{
    if (!g->on || !g->ready) return 0;
    if (who == DFW_ECHO && look == 3) return 0;
    if (g->done == frame) return 0;
    g->done = frame;
    return 1;
}
// the model collector runs only while a restore was asked lately (Death Presence and Echo off: nothing at all)
#define DFX_ASK_FRAMES 8
static int DfxCollectWanted(long frame, long lastAsk) { return lastAsk >= 0 && frame - lastAsk >= 0 && frame - lastAsk <= DFX_ASK_FRAMES; }

#endif
