// v3.33 CAM333: native test of src/camera_sim.h (staging/v333_feedback/w4_camera/PLAN.md, Tests).
// 1. Same inputs, same outputs as Playable Eileen v1.18.2: its [Camera] code (SH4EileenWeapons.cpp LoadCameraSettings
//    22413-22432, OtsRay 22452-22487, OtsFilter 22490-22640) transcribed below as Pe*, run side by side with Cam* over
//    long random walks (stick, player motion, stairs, a wall, room changes, teleports) at 30 / 60 / 120 / 180 ticks/s.
// 2. Frame-rate independence at 30 / 60 / 120 / 180: the turn, the smoothing, the stair spring, the wall zoom in / out.
// 3. Limits: pitch, margin, never past the wall plane, ceiling, first person with its hysteresis, the stair lag cap,
//    snaps on a teleport / a new room, non-finite input.
// 4. The defer table (who owns the view) and every load order: never two hooks.
// 5. v3.37 CP337: the pause Camera page's value steps (ranges clamp / wrap, lists wrap, off-grid values) and its
//    Left / Right repeat (the game's menu rhythm on its own counter).
// Build and run: test\build_cam_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/camera_sim.h"

static int g_fail, g_ok;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
    else ++g_ok;
}
static const int kHz[4] = { 30, 60, 120, 180 };

// ---------------------------------------------------------------- Playable Eileen's code (transcription, C)
typedef struct {
    int on, invX, invY, follow, wall;
    float dist, side, height, tiltDeg, smooth, turnSpeed, tiltSpeed, margin, zoomOut, zoomIn, ceil, hSmooth;
    float fov;
    int fixedCams;
} PeCfg;
typedef struct {
    int init;
    float yaw, yawOfs, cur, pitch, curP, cfgTilt;
    int wReset, wFp;
    float curS, curB, wLast[3];
    unsigned wColl;
    int hOk;
    float hy, hv, hLast[3];
    unsigned hColl;
} PeState;
static float pe_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void PeLoad(const float *raw, PeCfg *c)  // IniFloatIn(key) = raw[k]
{
    memset(c, 0, sizeof *c);
    c->on = raw[CAMK_ENABLE] != 0.0f;
    c->dist = pe_clampf(raw[CAMK_DISTANCE], 0.0f, 2000.0f);
    c->side = pe_clampf(raw[CAMK_SIDE], -500.0f, 500.0f);
    c->height = pe_clampf(raw[CAMK_HEIGHT], 0.0f, 600.0f);
    c->fov = raw[CAMK_FOV];
    if (!(c->fov == 0.0f || (c->fov >= 20.0f && c->fov <= 120.0f))) c->fov = 63.0f;
    c->tiltDeg = -pe_clampf(raw[CAMK_TILTDOWN], -68.0f, 68.0f);
    c->smooth = pe_clampf(raw[CAMK_SMOOTHING], 0.01f, 1.0f);
    c->turnSpeed = fabsf(pe_clampf(raw[CAMK_TURNSPEED], -1.0f, 1.0f));
    c->tiltSpeed = fabsf(pe_clampf(raw[CAMK_TILTSPEED], -1.0f, 1.0f));
    c->invX = raw[CAMK_INVERTX] != 0.0f;
    c->invY = raw[CAMK_INVERTY] != 0.0f;
    c->follow = raw[CAMK_FOLLOW] != 0.0f;
    c->wall = raw[CAMK_WALLZOOM] != 0.0f;
    c->margin = pe_clampf(raw[CAMK_WALLMARGIN], 0.0f, 200.0f);
    c->zoomOut = pe_clampf(raw[CAMK_ZOOMOUT], 0.001f, 1.0f);
    c->zoomIn = pe_clampf(raw[CAMK_ZOOMIN], 0.001f, 1.0f);
    c->ceil = pe_clampf(raw[CAMK_CEILING], 0.0f, 2000.0f);
    c->hSmooth = pe_clampf(raw[CAMK_STAIRS], 0.0f, 1.0f);
    c->fixedCams = raw[CAMK_FIXED] != 0.0f;
}
static float PeRateK(int r) { return r == 30 ? 1.0f : 30.0f / (float)r; }
static float PeRateExp(float a, float k)
{
    if (k == 1.0f || !(a > 0.0f)) return a;
    if (!(a < 1.0f)) return a;
    return 1.0f - powf(1.0f - a, k);
}
static float PeDist2(const float *a, const float *b)
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}
static PeCfg g_ots;
static CamRayFn g_peRay;
static void *g_peCtx;
static void PeRay(const float *start, const float *d, float wl, float *res, float *hit)
{
    float seg[8], n[4], a, b, mn, am, s;
    const float wlm = wl + g_ots.margin;
    int k;
    for (k = 0; k < 3; ++k) {
        seg[k] = start[k];
        seg[4 + k] = start[k] + d[k] * wlm;
    }
    seg[3] = seg[7] = 1.0f;
    *res = *hit = wl;
    if (!g_peRay(g_peCtx, seg, n)) return;
    a = n[0] * seg[0] + n[1] * seg[1] + n[2] * seg[2] + n[3];
    b = n[0] * seg[4] + n[1] * seg[5] + n[2] * seg[6] + n[3];
    if (a < 0.0f) { a = -a; b = -b; }
    if (b < 0.0f) {
        float x = a / (a - b) * wlm - 2.0f;
        if (!(0.0f < x)) x = 0.0f;
        if (!(wl >= x)) x = wl;
        *hit = x;
    }
    mn = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * g_ots.margin;
    if (!(mn > b)) return;
    am = a - mn;
    if (!(am > 0.0f)) { *res = 0.0f; return; }
    s = am / (a - b) * wlm;
    if (!(wl < s)) *res = s;
}
// The body of OtsFilter after its gates, with the game reads as arguments. rot(out, pitch, yaw) = 0x4FEEF0.
typedef void (*RotFn)(float *out, float pitch, float yaw);
static int PeFrame(PeState *s, const float *pos, float facing, float lx, float ly, int rate, unsigned coll, int collLoaded,
                   RotFn rot, float *mat)
{
    const float tilt = g_ots.tiltDeg * 0.017453293f;
    const float rk = PeRateK(rate);
    float turn, tl, smooth, dy, *R, *F, *eye, y, hy, hp, P[3], sg, Sd[3], sl, res, hit, S[3], D[3], wl;
    int snapH, k;
    if (!s->init) {
        s->yaw = facing; s->yawOfs = 0.0f; s->cur = s->yaw; s->pitch = s->curP = s->cfgTilt = tilt;
        s->init = 1; s->wReset = 1; s->hOk = 0;
    }
    if (tilt != s->cfgTilt) s->pitch = s->cfgTilt = tilt;
    if (g_ots.follow) s->yaw = facing;
    if (!_finite(lx)) lx = 0.0f;
    if (!_finite(ly)) ly = 0.0f;
    turn = g_ots.turnSpeed * lx * rk;
    if (g_ots.invX) turn = -turn;
    if (g_ots.follow) s->yawOfs += turn; else s->yaw += turn;
    tl = g_ots.tiltSpeed * ly * rk;
    if (!g_ots.invY) tl = -tl;
    s->pitch += tl;
    if (!(fabsf(s->pitch) <= 1.2f)) s->pitch = s->pitch < 0.0f ? -1.2f : 1.2f;
    smooth = PeRateExp(g_ots.smooth, rk);
    dy = s->yaw + s->yawOfs - s->cur;
    dy -= floorf(dy * 0.15915494f + 0.5f) * 6.2831855f;
    s->cur += dy * smooth;
    s->curP += (s->pitch - s->curP) * smooth;
    if (!_finite(s->cur + s->curP + s->yaw + s->yawOfs)) { s->init = 0; return 0; }
    rot(mat, s->curP, s->cur);
    R = mat; F = mat + 8; eye = mat + 12;
    y = pos[1];
    snapH = !(g_ots.hSmooth > 0.0f) || !s->hOk || coll != s->hColl || !_finite(s->hv) || !_finite(s->hy) || !(PeDist2(pos, s->hLast) < 78400.0f);
    if (snapH) {
        s->hy = y; s->hv = 0.0f; s->hColl = coll; s->hOk = 1;
    } else {
        const float w = g_ots.hSmooth, x = w * rk;
        const float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        const float c = s->hy - y, t = (s->hv + w * c) * rk;
        float lag, al;
        s->hv = (s->hv - w * t) * e;
        lag = (c + t) * e;
        al = fabsf(lag);
        if (al > 100.0f) {
            float nl = 100.0f + (al - 100.0f) * (rk == 1.0f ? 0.7f : powf(0.7f, rk));
            if (nl > 100.0f * 2.5f) nl = 100.0f * 2.5f;
            lag = lag < 0.0f ? -nl : nl;
        }
        s->hy = y + lag;
    }
    memcpy(s->hLast, pos, sizeof s->hLast);
    hy = s->hy;
    if (!g_ots.wall || !(g_ots.dist > 0.0f) || !collLoaded) {
        s->wReset = 1; s->wFp = 0;
        for (k = 0; k < 3; ++k) eye[k] = (k == 1 ? hy - g_ots.height : pos[k]) - F[k] * g_ots.dist + R[k] * g_ots.side;
        return 1;
    }
    if (coll != s->wColl) { s->wColl = coll; s->wReset = 1; }
    if (!(PeDist2(pos, s->wLast) < 78400.0f)) s->wReset = 1;
    memcpy(s->wLast, pos, sizeof s->wLast);
    hp = g_ots.height < 280.0f ? g_ots.height : 280.0f;
    P[0] = pos[0]; P[1] = hy - hp; P[2] = pos[2];
    sg = g_ots.side < 0.0f ? -1.0f : 1.0f;
    Sd[0] = R[0] * sg; Sd[1] = R[1] * sg; Sd[2] = R[2] * sg;
    sl = fabsf(g_ots.side);
    res = sl;
    if (!(sl < 1.0f)) PeRay(P, Sd, sl, &res, &hit);
    if (s->wReset) s->curS = res;
    else { const float eased = s->curS + (sl - s->curS) * PeRateExp(g_ots.zoomOut, rk); s->curS = eased < res ? eased : res; }
    for (k = 0; k < 3; ++k) S[k] = P[k] + Sd[k] * s->curS;
    D[0] = -F[0] * g_ots.dist; D[1] = -F[1] * g_ots.dist - g_ots.height + hp; D[2] = -F[2] * g_ots.dist;
    wl = sqrtf(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]);
    for (k = 0; k < 3; ++k) D[k] /= wl;
    PeRay(S, D, wl, &res, &hit);
    if (g_ots.ceil > 0.0f && D[1] < 0.0f) {
        float lim = -(g_ots.ceil - hp) / D[1];
        if (!(0.0f < lim)) lim = 0.0f;
        if (!(res < lim)) res = lim;
        if (!(hit < lim)) hit = lim;
    }
    if (s->wReset) s->curB = res;
    else if (s->curB < res || !(g_ots.zoomIn < 1.0f)) { const float eased = s->curB + (wl - s->curB) * PeRateExp(g_ots.zoomOut, rk); s->curB = eased < res ? eased : res; }
    else { const float eased = s->curB - (s->curB - res) * PeRateExp(g_ots.zoomIn, rk); s->curB = hit >= eased || !_finite(hit) ? eased : hit; }
    s->wReset = 0;
    if (!s->wFp ? s->curB < 40.0f : !(40.0f + 25.0f < s->curB)) {
        s->wFp = 1;
        for (k = 0; k < 3; ++k) eye[k] = P[k] + F[k] * 10.0f;
    } else {
        s->wFp = 0;
        for (k = 0; k < 3; ++k) eye[k] = S[k] + D[k] * s->curB;
    }
    if (!_finite(eye[0] + eye[1] + eye[2])) { s->init = 0; return 0; }
    return 1;
}

// ---------------------------------------------------------------- the same frame through camera_sim.h
static int CamFrameT(CamState *s, const CamCfg *c, const float *pos, float facing, float lx, float ly, int rate, unsigned coll,
                     int collLoaded, RotFn rot, float *mat, CamRayFn ray, void *ctx, int *flags)
{
    const float rk = CamRateK(rate);
    float hy;
    if (!CamLook(s, c, facing, lx, ly, rk)) return 0;
    rot(mat, s->curP, s->cur);
    hy = CamHeight(s, c, pos, coll, rk);
    return CamEye(s, c, mat, mat + 8, pos, hy, coll, collLoaded, rk, ray, ctx, mat + 12, flags);
}

// A rotation like 0x4FEEF0's rows right, down, forward (world y grows downward; a negative pitch looks down).
static void Rot(float *m, float pitch, float yaw)
{
    const float cp = cosf(pitch), sp = sinf(pitch), cy = cosf(yaw), sy = sinf(yaw);
    float f[3], r[3], d[3];
    f[0] = sy * cp; f[1] = -sp; f[2] = cy * cp;
    r[0] = cy; r[1] = 0.0f; r[2] = -sy;
    d[0] = f[1] * r[2] - f[2] * r[1]; d[1] = f[2] * r[0] - f[0] * r[2]; d[2] = f[0] * r[1] - f[1] * r[0];
    memset(m, 0, 16 * sizeof(float));
    m[0] = r[0]; m[1] = r[1]; m[2] = r[2];
    m[4] = d[0]; m[5] = d[1]; m[6] = d[2];
    m[8] = f[0]; m[9] = f[1]; m[10] = f[2];
    m[15] = 1.0f;
}
// The collision query: walls as planes n.x + d = 0 (n unit). Returns the nearest plane the segment moves toward and
// comes within 300 of at its end (enough for OtsRay's margin logic), as 0x561CD0 returns the closest plane.
typedef struct { int n; float p[4][4]; } Walls;
static int RayWalls(void *ctx, const float *seg, float *plane)
{
    const Walls *w = (const Walls *)ctx;
    int i, best = -1;
    float bestA = 1e30f;
    for (i = 0; i < w->n; ++i) {
        const float *n = w->p[i];
        const float a = n[0] * seg[0] + n[1] * seg[1] + n[2] * seg[2] + n[3];
        const float b = n[0] * seg[4] + n[1] * seg[5] + n[2] * seg[6] + n[3];
        if (a > 0.0f && b < a && b < 300.0f && a < bestA) best = i, bestA = a;
    }
    if (best < 0) return 0;
    memcpy(plane, w->p[best], 4 * sizeof(float));
    return 1;
}

static unsigned g_rng = 12345u;
static float Rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return (float)((g_rng >> 8) & 0xFFFF) / 65535.0f; }
static float Rel(float a, float b) { const float m = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b); return m < 1.0f ? fabsf(a - b) : fabsf(a - b) / m; }

// ---------------------------------------------------------------- 1. Playable Eileen's outputs
static void RawDefaults(float *raw) { memcpy(raw, kCamDefault, sizeof kCamDefault); }
static void TestLoadSame(void)
{
    int i, k, bad = 0;
    char msg[160];
    for (i = 0; i < 20000; ++i) {
        float raw[CAMK_N];
        CamCfg c;
        PeCfg p;
        for (k = 0; k < CAMK_N; ++k) raw[k] = (Rnd() - 0.4f) * (Rnd() < 0.5f ? 3.0f : 3000.0f);
        if (i == 0) RawDefaults(raw);
        CamCfgFrom(raw, &c);
        PeLoad(raw, &p);
        if (c.on != p.on || c.invX != p.invX || c.invY != p.invY || c.follow != p.follow || c.wall != p.wall || c.fixedCams != p.fixedCams ||
            c.dist != p.dist || c.side != p.side || c.height != p.height || c.tiltDeg != p.tiltDeg || c.smooth != p.smooth ||
            c.turnSpeed != p.turnSpeed || c.tiltSpeed != p.tiltSpeed || c.margin != p.margin || c.zoomOut != p.zoomOut ||
            c.zoomIn != p.zoomIn || c.ceil != p.ceil || c.hSmooth != p.hSmooth || c.fov != p.fov)
            ++bad;
    }
    sprintf_s(msg, sizeof msg, "settings: CamCfgFrom == Playable Eileen's LoadCameraSettings clamps for 20000 random ini values (%d differ)", bad);
    Check(bad == 0, msg);
    {   // a "nan" text: Playable Eileen passes NaN through its clamps; Refined reads the default (documented difference)
        float raw[CAMK_N];
        CamCfg c;
        RawDefaults(raw);
        raw[CAMK_DISTANCE] = raw[CAMK_HEIGHT] = (float)atof("nan");
        CamCfgFrom(raw, &c);
        Check(c.dist == 800.0f && c.height == 330.0f, "settings: a \"nan\" Distance / Height reads as the default (PE would pass NaN on)");
    }
}
static void TestFramesSame(void)
{
    int h, scen;
    for (scen = 0; scen < 4; ++scen)
        for (h = 0; h < 4; ++h) {
            const int hz = kHz[h];
            float raw[CAMK_N], pos[3] = { 0.0f, 0.0f, 0.0f }, facing = 0.3f, matC[16], matP[16], worst = 0.0f;
            CamCfg c;
            CamState sc;
            PeState sp;
            Walls w;
            unsigned coll = 7;
            long t, n = (long)(hz * 30.0f);  // 30 s
            int fc, fp, okC, okP, mism = 0, fpFrames = 0, wallFrames = 0;
            char msg[200];
            RawDefaults(raw);
            if (scen == 1) { raw[CAMK_SIDE] = 100.0f; raw[CAMK_DISTANCE] = 600.0f; raw[CAMK_HEIGHT] = 270.0f; raw[CAMK_TILTDOWN] = 15.0f; raw[CAMK_WALLMARGIN] = 30.0f; }  // the user's
            if (scen == 2) { raw[CAMK_FOLLOW] = 1.0f; raw[CAMK_INVERTX] = 1.0f; raw[CAMK_INVERTY] = 1.0f; raw[CAMK_SIDE] = -150.0f; raw[CAMK_ZOOMIN] = 1.0f; }
            if (scen == 3) { raw[CAMK_WALLZOOM] = 0.0f; raw[CAMK_STAIRS] = 0.0f; }
            CamCfgFrom(raw, &c);
            PeLoad(raw, &g_ots);
            memset(&sc, 0, sizeof sc);
            memset(&sp, 0, sizeof sp);
            w.n = 2;
            w.p[0][0] = 0.0f; w.p[0][1] = 0.0f; w.p[0][2] = 1.0f; w.p[0][3] = 400.0f;    // a wall at z = -400 (behind at the start)
            w.p[1][0] = -1.0f; w.p[1][1] = 0.0f; w.p[1][2] = 0.0f; w.p[1][3] = 350.0f;   // a wall at x = 350
            g_peRay = RayWalls;
            g_peCtx = &w;
            g_rng = 777u + scen * 13u + h;
            for (t = 0; t < n; ++t) {
                const float sec = (float)t / hz;
                const float lx = sinf(sec * 0.7f) * (Rnd() < 0.9f ? 1.0f : 0.0f), ly = cosf(sec * 0.31f) * 0.6f;
                pos[0] = 300.0f * sinf(sec * 0.2f);
                pos[2] = 250.0f * sinf(sec * 0.13f) - 100.0f;
                pos[1] = (float)((int)(sec * 2.0f) % 8) * -25.0f;  // stairs: 25 per half second, then down at once
                facing += 0.4f / hz;
                if (t == n / 3) coll = 8;                          // a new room
                if (t == n / 2) pos[0] += 900.0f;                  // a teleport (load, scripted move)
                okC = CamFrameT(&sc, &c, pos, facing, lx, ly, hz, coll, 1, Rot, matC, RayWalls, &w, &fc);
                fp = 0;
                okP = PeFrame(&sp, pos, facing, lx, ly, hz, coll, 1, Rot, matP);
                if (okC != okP) { ++mism; continue; }
                if (!okC) continue;
                {
                    int k;
                    for (k = 12; k < 15; ++k) {
                        const float d = Rel(matC[k], matP[k]);
                        if (d > worst) worst = d;
                    }
                }
                if (fc & CAMF_FP) ++fpFrames;
                if (fc & CAMF_WALL) ++wallFrames;
                (void)fp;
            }
            sprintf_s(msg, sizeof msg, "Playable Eileen's frames at %3d Hz, case %d: eye the same over %ld frames (worst rel. diff %.2g, %d mismatched, %d wall / %d first-person frames)",
                    hz, scen, n, worst, mism, wallFrames, fpFrames);
            Check(mism == 0 && worst < 1e-4f, msg);  // float rounding of two code shapes (a few 1e-5 at most)
        }
}

// ---------------------------------------------------------------- 2. frame-rate independence
static void Cfg(CamCfg *c, int follow)
{
    float raw[CAMK_N];
    RawDefaults(raw);
    raw[CAMK_FOLLOW] = (float)follow;
    CamCfgFrom(raw, c);
}
static void TestRates(void)
{
    float yaw[4], cur[4], curStep[4], pitchMax = 0.0f;
    int h;
    char msg[200];
    for (h = 0; h < 4; ++h) {  // the turn: full stick for 1 s, then let go for 0.5 s
        const int hz = kHz[h];
        CamCfg c;
        CamState s;
        long t;
        Cfg(&c, 0);
        memset(&s, 0, sizeof s);
        for (t = 0; t < hz; ++t) CamLook(&s, &c, 0.0f, 1.0f, 0.0f, CamRateK(hz));
        yaw[h] = s.yaw;
        for (t = 0; t < hz / 2; ++t) CamLook(&s, &c, 0.0f, 0.0f, 0.0f, CamRateK(hz));
        cur[h] = s.cur;
        for (t = 0; t < hz * 10; ++t) CamLook(&s, &c, 0.0f, 0.0f, 1.0f, CamRateK(hz));
        if (fabsf(s.pitch) > pitchMax) pitchMax = fabsf(s.pitch);
    }
    sprintf_s(msg, sizeof msg, "turn: 1 s of full stick turns %.4f / %.4f / %.4f / %.4f rad at 30 / 60 / 120 / 180 (TurnSpeed 0.1 x 30 = 3)", yaw[0], yaw[1], yaw[2], yaw[3]);
    Check(fabsf(yaw[0] - 3.0f) < 1e-3f && fabsf(yaw[1] - 3.0f) < 1e-3f && fabsf(yaw[2] - 3.0f) < 2e-3f && fabsf(yaw[3] - 3.0f) < 2e-3f, msg);
    sprintf_s(msg, sizeof msg, "turn: the shown yaw 0.5 s after letting go %.3f / %.3f / %.3f / %.3f (within 0.06 of 30 Hz)", cur[0], cur[1], cur[2], cur[3]);
    Check(fabsf(cur[1] - cur[0]) < 0.06f && fabsf(cur[2] - cur[0]) < 0.06f && fabsf(cur[3] - cur[0]) < 0.06f, msg);
    sprintf_s(msg, sizeof msg, "tilt: 10 s of full stick stops at %.3f rad (limit 1.2)", pitchMax);
    Check(pitchMax <= 1.2f + 1e-6f, msg);
    for (h = 0; h < 4; ++h) {  // the smoothing: Follow, she turns 1 rad at once; 0.2 s later
        const int hz = kHz[h];
        CamCfg c;
        CamState s;
        long t;
        Cfg(&c, 1);
        memset(&s, 0, sizeof s);
        CamLook(&s, &c, 0.0f, 0.0f, 0.0f, CamRateK(hz));
        for (t = 0; t < (hz / 5); ++t) CamLook(&s, &c, 1.0f, 0.0f, 0.0f, CamRateK(hz));
        curStep[h] = s.cur;
    }
    sprintf_s(msg, sizeof msg, "smoothing: 0.2 s after a 1 rad turn the view is at %.5f / %.5f / %.5f / %.5f (1 - 0.8^6 = %.5f)", curStep[0], curStep[1],
            curStep[2], curStep[3], 1.0f - powf(0.8f, 6.0f));
    Check(fabsf(curStep[0] - (1.0f - powf(0.8f, 6.0f))) < 1e-4f && fabsf(curStep[1] - curStep[0]) < 1e-4f && fabsf(curStep[2] - curStep[0]) < 1e-4f &&
              fabsf(curStep[3] - curStep[0]) < 1e-4f, msg);
    {   // the stair spring: down a flight (200 in 1 s), 0.5 s and 1.5 s after the start
        float a[4], b[4], maxLag[4];
        for (h = 0; h < 4; ++h) {
            const int hz = kHz[h];
            CamCfg c;
            CamState s;
            float pos[3] = { 0.0f, 0.0f, 0.0f };
            long t;
            Cfg(&c, 0);
            memset(&s, 0, sizeof s);
            maxLag[h] = 0.0f;
            CamHeight(&s, &c, pos, 1u, CamRateK(hz));
            for (t = 1; t <= hz * 3 / 2; ++t) {
                const float sec = (float)t / hz;
                float hy;
                pos[1] = sec < 1.0f ? 200.0f * sec : 200.0f;
                hy = CamHeight(&s, &c, pos, 1u, CamRateK(hz));
                if (fabsf(hy - pos[1]) > maxLag[h]) maxLag[h] = fabsf(hy - pos[1]);
                if (t == hz / 2) a[h] = hy;
                if (t == hz * 3 / 2) b[h] = hy;
            }
        }
        sprintf_s(msg, sizeof msg, "stairs: the camera height 0.5 s down a flight %.1f / %.1f / %.1f / %.1f (player at 100; within 3 of 30 Hz)", a[0], a[1], a[2], a[3]);
        Check(fabsf(a[1] - a[0]) < 3.0f && fabsf(a[2] - a[0]) < 3.0f && fabsf(a[3] - a[0]) < 3.0f && a[0] < 100.0f, msg);
        sprintf_s(msg, sizeof msg, "stairs: 0.5 s after the flight %.1f / %.1f / %.1f / %.1f (player at 200; within 2)", b[0], b[1], b[2], b[3]);
        Check(fabsf(b[1] - b[0]) < 2.0f && fabsf(b[2] - b[0]) < 2.0f && fabsf(b[3] - b[0]) < 2.0f, msg);
        sprintf_s(msg, sizeof msg, "stairs: the most it trails %.1f / %.1f / %.1f / %.1f (soft limit 100, hard 250)", maxLag[0], maxLag[1], maxLag[2], maxLag[3]);
        Check(maxLag[0] <= 250.0f && maxLag[3] <= 250.0f && fabsf(maxLag[3] - maxLag[0]) < 3.0f, msg);
    }
    {   // the wall zoom: walking back toward a wall, then away from it
        float inB[4], outB[4];
        for (h = 0; h < 4; ++h) {
            const int hz = kHz[h];
            CamCfg c;
            CamState s;
            Walls w;
            float pos[3] = { 0.0f, 0.0f, 0.0f }, m[16];
            long t;
            int f;
            Cfg(&c, 0);
            memset(&s, 0, sizeof s);
            w.n = 1;
            w.p[0][0] = 0.0f; w.p[0][1] = 0.0f; w.p[0][2] = 1.0f; w.p[0][3] = 2000.0f;  // a wall at z = -2000: out of reach
            for (t = 0; t < hz; ++t) CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, hz, 1u, 1, Rot, m, RayWalls, &w, &f);
            w.p[0][3] = 400.0f;  // the wall comes to z = -400 (a door closes behind): ease in for 0.1 s
            for (t = 0; t < hz / 10; ++t) CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, hz, 1u, 1, Rot, m, RayWalls, &w, &f);
            inB[h] = s.curB;
            w.p[0][3] = 2000.0f;  // gone again: ease out for 1 s
            for (t = 0; t < hz; ++t) CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, hz, 1u, 1, Rot, m, RayWalls, &w, &f);
            outB[h] = s.curB;
        }
        sprintf_s(msg, sizeof msg, "wall zoom: the eye 0.1 s after a wall comes close %.1f / %.1f / %.1f / %.1f (never past the plane)", inB[0], inB[1], inB[2], inB[3]);
        Check(fabsf(inB[1] - inB[0]) < 15.0f && fabsf(inB[2] - inB[0]) < 15.0f && fabsf(inB[3] - inB[0]) < 15.0f, msg);
        sprintf_s(msg, sizeof msg, "wall zoom: 1 s after it is gone %.1f / %.1f / %.1f / %.1f (ZoomOut 0.06 a 30 Hz tick; within 5)", outB[0], outB[1], outB[2], outB[3]);
        Check(fabsf(outB[1] - outB[0]) < 5.0f && fabsf(outB[2] - outB[0]) < 5.0f && fabsf(outB[3] - outB[0]) < 5.0f, msg);
    }
}

// ---------------------------------------------------------------- 3. limits
static void TestLimits(void)
{
    char msg[200];
    int h;
    for (h = 0; h < 4; ++h) {  // the margin and the plane: a wall behind at z = -300, distance 800, margin 50
        const int hz = kHz[h];
        CamCfg c;
        CamState s;
        Walls w;
        float pos[3] = { 0.0f, 0.0f, 0.0f }, m[16], minGap = 1e9f;
        long t;
        int f;
        Cfg(&c, 0);
        memset(&s, 0, sizeof s);
        w.n = 1;
        w.p[0][0] = 0.0f; w.p[0][1] = 0.0f; w.p[0][2] = 1.0f; w.p[0][3] = 300.0f;
        for (t = 0; t < hz * 3; ++t) {
            CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, hz, 1u, 1, Rot, m, RayWalls, &w, &f);
            if (m[14] + 300.0f < minGap) minGap = m[14] + 300.0f;
        }
        sprintf_s(msg, sizeof msg, "margin at %3d Hz: the eye stays %.1f in front of a wall plane (WallMargin 50; flags %d)", hz, minGap, f);
        Check(minGap >= 50.0f - 0.5f && (f & CAMF_WALL), msg);
    }
    {   // first person when the wall is right behind (closer than 40), back out above 65
        CamCfg c;
        CamState s;
        Walls w;
        float pos[3] = { 0.0f, 0.0f, 0.0f }, m[16];
        int f = 0, t, fpIn = 0, fpOut = 1;
        Cfg(&c, 0);
        memset(&s, 0, sizeof s);
        w.n = 1;
        w.p[0][0] = 0.0f; w.p[0][1] = 0.0f; w.p[0][2] = 1.0f; w.p[0][3] = 60.0f;  // z = -60: margin 50 leaves ~10
        for (t = 0; t < 60; ++t) CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, 30, 1u, 1, Rot, m, RayWalls, &w, &f);
        fpIn = (f & CAMF_FP) != 0;
        w.p[0][3] = 2000.0f;
        for (t = 0; t < 120; ++t) CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, 30, 1u, 1, Rot, m, RayWalls, &w, &f);
        fpOut = (f & CAMF_FP) != 0;
        sprintf_s(msg, sizeof msg, "first person: a wall 60 behind -> first person %d; the wall gone -> third person again %d", fpIn, !fpOut);
        Check(fpIn && !fpOut, msg);
    }
    {   // the hysteresis: an eye distance between 40 and 65 keeps the current mode
        CamState s;
        memset(&s, 0, sizeof s);
        s.wFp = 1;
        s.curB = 50.0f;
        Check(!(CAM_WALL_MIN + CAM_WALL_HYST < s.curB), "first person: at 50 it stays first person (back above 65)");
        s.wFp = 0;
        Check(!(s.curB < CAM_WALL_MIN), "first person: at 50 from third person it stays third person (in below 40)");
    }
    {   // the ceiling: looking down steeply the eye rises; near a wall the cap holds it at CeilingLimit above the feet
        CamCfg c;
        CamState s;
        Walls w;
        float raw[CAMK_N], pos[3] = { 0.0f, 0.0f, 0.0f }, m[16], top = 0.0f;
        int f, t;
        RawDefaults(raw);
        raw[CAMK_TILTDOWN] = 68.0f;
        raw[CAMK_DISTANCE] = 2000.0f;
        CamCfgFrom(raw, &c);
        memset(&s, 0, sizeof s);
        w.n = 1;
        w.p[0][0] = 0.0f; w.p[0][1] = 0.0f; w.p[0][2] = 1.0f; w.p[0][3] = 5000.0f;
        for (t = 0; t < 60; ++t) {
            CamFrameT(&s, &c, pos, 0.0f, 0.0f, 0.0f, 30, 1u, 1, Rot, m, RayWalls, &w, &f);
            if (-m[13] > top) top = -m[13];
        }
        sprintf_s(msg, sizeof msg, "ceiling: looking down 68 deg from 2000 away the eye stays %.1f above the feet (CeilingLimit 450)", top);
        Check(top <= 450.0f + 0.5f, msg);
    }
    {   // snaps: a teleport and a new room snap the spring and the wall zoom; non-finite input restarts
        CamCfg c;
        CamState s;
        float pos[3] = { 0.0f, 0.0f, 0.0f }, hy;
        Cfg(&c, 0);
        memset(&s, 0, sizeof s);
        CamHeight(&s, &c, pos, 1u, 1.0f);
        pos[1] = 150.0f;
        hy = CamHeight(&s, &c, pos, 1u, 1.0f);
        Check(hy < 150.0f, "stairs: a 150 step down trails (spring)");
        pos[0] = 400.0f;
        hy = CamHeight(&s, &c, pos, 1u, 1.0f);
        Check(hy == 150.0f, "stairs: a jump of 400 snaps the height");
        pos[1] = 300.0f;
        pos[0] = 410.0f;
        hy = CamHeight(&s, &c, pos, 2u, 1.0f);
        Check(hy == 300.0f, "stairs: a new room (collision file) snaps the height");
        memset(&s, 0, sizeof s);
        Check(CamLook(&s, &c, (float)atof("nan"), 0.0f, 0.0f, 1.0f) == 0 && s.init == 0, "look: a NaN facing restarts (the game's camera that frame)");
        memset(&s, 0, sizeof s);
        Check(CamLook(&s, &c, 0.5f, (float)atof("inf"), (float)atof("nan"), 1.0f) == 1 && s.yaw == 0.5f, "look: a non-finite stick reads as 0");
    }
    {   // RateK / RateExp
        Check(CamRateK(30) == 1.0f && CamRateK(60) == 0.5f && CamRateK(5) == 1.0f && CamRateK(999) == 1.0f, "rate: RateK 30 / 60, out of range = 30");
        Check(CamRateExp(0.2f, 1.0f) == 0.2f && CamRateExp(1.0f, 0.25f) == 1.0f && fabsf(CamRateExp(0.2f, 0.5f) - (1.0f - sqrtf(0.8f))) < 1e-6f,
              "rate: RateExp keeps 30 Hz values, 1 = at once, 2 ticks of k 0.5 = 1 tick");
    }
}

// ---------------------------------------------------------------- 4. the defer table and the load orders
static void TestDefer(void)
{
    static const struct { int site, mapped, code, ran, lists, tick, own, anom; const char *what; } kT[] = {
        { CAMS_JMP_PE, 1, 1, 1, 1, 0, CAM_OWN_PE, 0, "Playable Eileen hooked first" },
        { CAMS_JMP_PE, 1, 1, 0, 0, 1, CAM_OWN_PE, 0, "Playable Eileen hooked after the wait" },
        { CAMS_JMP_US, 1, 0, 1, 0, 1, CAM_OWN_REFINED, 0, "ours already" },
        { CAMS_JMP_OTHER, 0, 0, 0, 0, 0, CAM_OWN_NONE, 1, "the cheat table's Over The Shoulder" },
        { CAMS_OTHER, 1, 1, 1, 1, 1, CAM_OWN_NONE, 1, "other bytes" },
        { CAMS_STOCK, 0, 0, 0, 0, 0, CAM_OWN_REFINED, 0, "no Playable Eileen" },
        { CAMS_STOCK, 1, 1, 0, 0, 0, CAM_OWN_WAIT, 0, "Playable Eileen with its camera, not patched yet" },
        { CAMS_STOCK, 1, 1, 0, 0, 1, CAM_OWN_REFINED, 0, "... and never patched (inactive) at the tick" },
        { CAMS_STOCK, 1, 0, 0, 0, 0, CAM_OWN_REFINED, 0, "Playable Eileen without a camera, not patched yet" },
        { CAMS_STOCK, 1, 0, 1, 0, 0, CAM_OWN_REFINED, 0, "Playable Eileen without a camera, patched" },
        { CAMS_STOCK, 1, 1, 1, 0, 0, CAM_OWN_REFINED, 0, "Playable Eileen with its camera code but its site check failed" },
        { CAMS_STOCK, 1, 1, 1, 1, 0, CAM_OWN_PE, 1, "it lists the site, bytes stock: stay out" },
    };
    int i;
    char msg[200];
    for (i = 0; i < (int)(sizeof kT / sizeof kT[0]); ++i) {
        int an;
        const int own = CamDecideOwner(kT[i].site, kT[i].mapped, kT[i].code, kT[i].ran, kT[i].lists, kT[i].tick, &an);
        sprintf_s(msg, sizeof msg, "defer: %-62s -> owner %d (want %d), anomaly %d", kT[i].what, own, kT[i].own, an);
        Check(own == kT[i].own && an == kT[i].anom, msg);
    }
    {   // load orders: a model of the two DllMains and the first tick; count the hooks at 0x4F7080
        static const char *const kName[6] = { "PE (camera) first", "Refined first, PE (camera) later", "Refined first, PE (no camera) later",
                                              "PE (no camera) first", "PE (camera) inactive", "no PE" };
        int order;
        for (order = 0; order < 6; ++order) {
            const int peCam = order == 0 || order == 1 || order == 4, peMapped = order != 5, peActive = order != 4;
            const int peFirst = order == 0 || order == 3 || order == 4;
            int site = CAMS_STOCK, peRan = 0, peLists = 0, hooks = 0, an, own;
            const int peTries = peMapped && peActive;
            // DllMain 1
            if (peFirst && peTries) {
                peRan = 1;
                if (peCam && site == CAMS_STOCK) site = CAMS_JMP_PE, peLists = 1, ++hooks;
            }
            own = CamDecideOwner(site, peMapped, peCam, peRan, peLists, 0, &an);  // Refined's Init
            if (own == CAM_OWN_REFINED && site == CAMS_STOCK) site = CAMS_JMP_US, ++hooks;
            if (!peFirst && peTries) {  // Playable Eileen's DllMain second: its byte check needs the stock entry
                peRan = 1;
                if (peCam && site == CAMS_STOCK) site = CAMS_JMP_PE, peLists = 1, ++hooks;
            }
            own = CamDecideOwner(site, peMapped, peCam, peRan, peLists, 1, &an);  // the first game tick
            if (own == CAM_OWN_REFINED && site == CAMS_STOCK) site = CAMS_JMP_US, ++hooks;
            sprintf_s(msg, sizeof msg, "load order %-36s: %d hook(s), owner %s", kName[order], hooks,
                    own == CAM_OWN_PE ? "Playable Eileen" : own == CAM_OWN_REFINED ? "SH4 Refined" : "none");
            Check(hooks == 1 && own == (peCam && peActive ? CAM_OWN_PE : CAM_OWN_REFINED) &&
                      (own != CAM_OWN_PE || site == CAMS_JMP_PE) && (own != CAM_OWN_REFINED || site == CAMS_JMP_US), msg);
        }
    }
}

// ---------------------------------------------------------------- 5. v3.37 CP337: the pause page's steps and key repeat
static void TestSteps(void)
{
    char msg[160];
    int i, t, fires, first, gaps_ok;
    CamRepeat r;
    // ranges: Distance 0..1000 by 25, Turn speed 0.02..0.3 by 0.02
    Check(fabsf(CamStepRange(800.0f, 0.0f, 1000.0f, 25.0f, 1, 0) - 825.0f) < 1e-3f, "step: Distance 800 Right -> 825");
    Check(fabsf(CamStepRange(800.0f, 0.0f, 1000.0f, 25.0f, -1, 0) - 775.0f) < 1e-3f, "step: Distance 800 Left -> 775");
    Check(fabsf(CamStepRange(1000.0f, 0.0f, 1000.0f, 25.0f, 1, 0) - 1000.0f) < 1e-3f, "step: Distance 1000 Right stays (Left / Right clamp)");
    Check(fabsf(CamStepRange(0.0f, 0.0f, 1000.0f, 25.0f, -1, 0) - 0.0f) < 1e-3f, "step: Distance 0 Left stays");
    Check(fabsf(CamStepRange(1000.0f, 0.0f, 1000.0f, 25.0f, 1, 1) - 0.0f) < 1e-3f, "step: Distance 1000 confirm wraps to 0");
    Check(fabsf(CamStepRange(810.0f, 0.0f, 1000.0f, 25.0f, 1, 0) - 825.0f) < 1e-3f, "step: off the grid (810) Right -> 825");
    Check(fabsf(CamStepRange(810.0f, 0.0f, 1000.0f, 25.0f, -1, 0) - 800.0f) < 1e-3f, "step: off the grid (810) Left -> 800");
    Check(fabsf(CamStepRange(1500.0f, 0.0f, 1000.0f, 25.0f, -1, 0) - 1000.0f) < 1e-3f, "step: above the range (an ini 1500) Left -> 1000");
    Check(fabsf(CamStepRange(0.1f, 0.02f, 0.3f, 0.02f, 1, 0) - 0.12f) < 1e-4f, "step: Turn speed 0.1 Right -> 0.12");
    Check(fabsf(CamStepRange(0.3f, 0.02f, 0.3f, 0.02f, 1, 1) - 0.02f) < 1e-4f, "step: Turn speed 0.3 confirm wraps to 0.02");
    {   // every grid point is reached from the bottom in exactly (hi - lo) / st Right presses, none skipped or repeated
        float v = 0.02f;
        int n = 0, ok = 1;
        while (n < 100) {
            const float w = CamStepRange(v, 0.02f, 0.3f, 0.02f, 1, 0);
            if (fabsf(w - v) < 1e-5f) break;
            if (fabsf(w - v - 0.02f) > 1e-4f) ok = 0;
            v = w, ++n;
        }
        sprintf_s(msg, sizeof msg, "step: Turn speed 0.02 -> 0.3 in %d Right presses (want 14), even steps %d", n, ok);
        Check(n == 14 && ok, msg);
    }
    Check(CamStepRange(NAN, 0.0f, 1000.0f, 25.0f, 1, 0) == 0.0f, "step: a non-finite value goes to the bottom");
    // lists (always wrap; Custom / unknown starts at an end)
    Check(CamStepList(0, 2, 1) == 1 && CamStepList(1, 2, 1) == 0 && CamStepList(0, 2, -1) == 1, "list: Off / On wraps both ways");
    Check(CamStepList(12, 13, 1) == 0 && CamStepList(0, 13, -1) == 12, "list: Side Right 150 -> Left 150 and back");
    Check(CamStepList(13, 13, 1) == 0 && CamStepList(13, 13, -1) == 12 && CamStepList(-1, 13, 1) == 0, "list: Custom starts at an end");
    // the repeat: a press fires at once; held, the 16th tick fires and then every 2nd; released, the next press at once
    CamRepeatReset(&r);
    Check(CamRepeatStep(&r, 8, 0) == 0, "repeat: a key held when the page opened does nothing");
    Check(CamRepeatStep(&r, 8, 8) == 1, "repeat: a new Right press fires +1");
    fires = 0, first = -1, gaps_ok = 1;
    {
        int last = 0;
        for (t = 1; t <= 60; ++t) {
            const int d = CamRepeatStep(&r, 8, 0);
            if (d) {
                if (d != 1) gaps_ok = 0;
                if (first < 0) first = t;
                else if (t - last != 2) gaps_ok = 0;
                last = t;
                ++fires;
            }
        }
    }
    sprintf_s(msg, sizeof msg, "repeat: held 60 more ticks: first repeat at tick %d (want 16), %d repeats every 2nd tick (want 23) %d",
              first, fires, gaps_ok);
    Check(first == 16 && fires == 23 && gaps_ok, msg);
    Check(CamRepeatStep(&r, 0, 0) == 0 && CamRepeatStep(&r, 4, 4) == -1, "repeat: released, then Left fires -1 at once");
    Check(CamRepeatStep(&r, 0x0C, 0x0C) == 0, "repeat: Left and Right together: nothing");
    CamRepeatReset(&r);
    Check(CamRepeatStep(&r, 4, 4) == -1, "repeat: after a reset a new press fires");
    for (i = 0, fires = 0; i < 5000; ++i) fires += CamRepeatStep(&r, 4, 0) != 0;
    sprintf_s(msg, sizeof msg, "repeat: held for 5000 ticks: %d repeats (want 2493: every 2nd from the 16th, no run-away)", fires);
    Check(fires == 2493, msg);
}

int main(void)
{
    TestLoadSame();
    TestFramesSame();
    TestRates();
    TestLimits();
    TestDefer();
    TestSteps();
    printf("%s: %d passed, %d failed\n", g_fail ? "FAIL" : "ALL OK", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
