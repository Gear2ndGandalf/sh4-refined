/* test_matrix.c -- v2.2 ClassifyMatrix (interp_pair.inc) with values from the live dump of 2026-11-04.
   Build: test\build_test_matrix.cmd */
#include <stdio.h>
#include <windows.h>
#include <math.h>
enum { ST_NONE, ST_SAME, ST_MOVE, ST_REJ };
typedef struct { DWORD off, prevOff, reg, vs; WORD n; BYTE st, pad; } Up;
#define U_MAX 8192
static Up g_up[U_MAX];
static int g_nu, g_np, g_pend[256], g_cur, g_prevOk;
typedef struct { BYTE *p; DWORD n, cap; } Arena;
static Arena g_arena[2];
#include "..\src\interp_pair.inc"
static int fails, checks;
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static int Run(const float *c, const float *p, int n, int *chg, int *rej) { *chg = *rej = 0; return ClassifyUpload(c, p, n, chg, rej); }
int main(void)
{
    int chg, rej, st;
    /* an item on the floor while the camera turns (dump 1, draw 63): per-row rule called it a jump (0.585) */
    const float itemNow[12] = { -0.6637f, -0.0f, -0.7480f, -771.3574f, -0.2187f, 0.9563f, 0.1940f, 33.6434f, 0.7153f, 0.2924f, -0.6347f, 1361.4419f };
    const float itemPrev[12] = { -0.5926f, 0.0f, -0.8055f, -699.5190f, -0.2355f, 0.9563f, 0.1733f, 13.9746f, 0.7703f, 0.2924f, -0.5667f, 1439.9189f };
    /* Eileen's root bone (draw 84): translation row 0 crosses zero */
    const float boneNow[12] = { 0.4700f, 0.3459f, -0.8120f, -0.3379f, -0.2485f, 0.9347f, 0.2543f, -36.8243f, 0.8469f, 0.0822f, 0.5252f, 561.2671f };
    const float bonePrev[12] = { 0.5481f, 0.3262f, -0.7701f, 0.3350f, -0.2350f, 0.9438f, 0.2325f, -36.8561f, 0.8027f, 0.0535f, 0.5939f, 575.4941f };
    float cut[12], flip[12], tele[12], same[12];
    int i;
    st = Run(itemNow, itemPrev, 3, &chg, &rej); CHECK(st == ST_MOVE && chg == 3 && rej == 0);
    st = Run(boneNow, bonePrev, 3, &chg, &rej); CHECK(st == ST_MOVE && rej == 0);
    g_pmMatrixRule = 0;  /* the v2.1 rule: both were jumps */
    st = Run(itemNow, itemPrev, 3, &chg, &rej); CHECK(st == ST_REJ);
    st = Run(boneNow, bonePrev, 3, &chg, &rej); CHECK(st == ST_REJ);
    g_pmMatrixRule = 1;
    /* a camera cut: the view turns 90 degrees */
    for (i = 0; i < 12; ++i) cut[i] = itemPrev[i];
    cut[0] = -itemPrev[2]; cut[2] = itemPrev[0]; cut[8] = -itemPrev[10]; cut[10] = itemPrev[8];
    st = Run(cut, itemPrev, 3, &chg, &rej); CHECK(st == ST_REJ && rej == chg);
    /* a teleport: same rotation, the object is now 3x as far */
    for (i = 0; i < 12; ++i) tele[i] = itemPrev[i];
    tele[3] *= 3.0f; tele[7] *= 3.0f; tele[11] *= 3.0f;
    st = Run(tele, itemPrev, 3, &chg, &rej); CHECK(st == ST_REJ);
    /* a mirror flip of the rotation (a bone wrapping) */
    for (i = 0; i < 12; ++i) flip[i] = (i % 4 == 3) ? bonePrev[i] : -bonePrev[i];
    st = Run(flip, bonePrev, 3, &chg, &rej); CHECK(st == ST_REJ);
    /* nothing moved */
    for (i = 0; i < 12; ++i) same[i] = itemPrev[i];
    st = Run(same, itemPrev, 3, &chg, &rej); CHECK(st == ST_SAME && chg == 0 && rej == 0);
    /* only one row moved a little */
    same[7] += 1.0f;
    st = Run(same, itemPrev, 3, &chg, &rej); CHECK(st == ST_MOVE && chg == 1 && rej == 0);
    /* NaN: a jump */
    same[5] = (float)NAN;
    st = Run(same, itemPrev, 3, &chg, &rej); CHECK(st == ST_REJ && chg == 3 && rej == 3);
    /* single registers still use the per-register rule: 0 -> 1 is a jump */
    { const float a[4] = { 1, 0, 0, 0 }, b[4] = { 0, 0, 0, 0 }; st = Run(a, b, 1, &chg, &rej); CHECK(st == ST_REJ); }
    /* 4x4 view-projection, small turn: blended */
    { float vp[16] = { 1.2f, 0, -0.1f, 5, 0, 1.6f, 0, -20, 0.1f, 0, 1.0f, 900, 0.1f, 0, 1.0f, 910 }, vq[16]; for (i = 0; i < 16; ++i) vq[i] = vp[i];
      vq[2] = -0.16f; vq[8] = 0.16f; vq[12] = 0.16f; vq[3] = -40; st = Run(vp, vq, 4, &chg, &rej); CHECK(st == ST_MOVE); }
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
