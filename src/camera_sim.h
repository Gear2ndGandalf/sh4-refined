// v3.33 CAM333: the over-the-shoulder camera's pure code (camera.inc runs it in the game; test/camera_sim_test.c checks it
// against a transcription of Playable Eileen v1.18.2's [Camera] code, SH4EileenWeapons.cpp OtsFilter / OtsRay /
// LoadCameraSettings, lines 22389-22640). Same maths, same per-30 Hz-tick constants scaled by the tick length (RateK /
// RateExp). No game memory here: the caller passes the player's position, facing, look input, the camera's rotation rows
// (0x4FEEF0) and a collision query (0x561CD0) as a callback.
#ifndef CAMERA_SIM_H
#define CAMERA_SIM_H
#include <math.h>
#include <float.h>
#include <string.h>

#define CAM_WALL_MIN 40.0f     // the eye closer than this to the shoulder point: first person
#define CAM_WALL_HYST 25.0f    // ... and back out above 65
#define CAM_FRONT 10.0f        // first person: the eye this far in front of the head pivot
#define CAM_PIVOT_MAX 280.0f   // the wall rays start at min(Height, 280) above the feet
#define CAM_TELE2 78400.0f     // a jump of 280 or more in one frame (squared): snap, no easing
#define CAM_MAX_LAG 100.0f     // the stair spring trails the height by about this much (hard 2.5 x)
#define CAM_PITCH_MAX 1.2f     // radians up / down
#define CAM_DEG 0.017453293f

// [Camera] keys, in Playable Eileen's order, with its defaults (Enable: SH4 Refined's default is 0, see camera.inc)
enum { CAMK_ENABLE, CAMK_DISTANCE, CAMK_SIDE, CAMK_HEIGHT, CAMK_TILTDOWN, CAMK_SMOOTHING, CAMK_TURNSPEED, CAMK_TILTSPEED,
       CAMK_INVERTX, CAMK_INVERTY, CAMK_FOLLOW, CAMK_WALLZOOM, CAMK_WALLMARGIN, CAMK_ZOOMOUT, CAMK_ZOOMIN, CAMK_CEILING,
       CAMK_STAIRS, CAMK_FOV, CAMK_FIXED, CAMK_N };
static const char *const kCamKey[CAMK_N] = { "Enable", "Distance", "Side", "Height", "TiltDown", "Smoothing", "TurnSpeed",
                                             "TiltSpeed", "InvertX", "InvertY", "Follow", "WallZoom", "WallMargin", "ZoomOut",
                                             "ZoomIn", "CeilingLimit", "StairSmoothing", "Fov", "FixedCameras" };
static const float kCamDefault[CAMK_N] = { 1.0f, 800.0f, 0.0f, 330.0f, 17.0f, 0.2f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f,
                                           50.0f, 0.06f, 0.25f, 450.0f, 0.25f, 63.0f, 0.0f };

typedef struct {
    int on, invX, invY, follow, wall, fixedCams;
    float dist, side, height, tiltDeg, smooth, turnSpeed, tiltSpeed, margin, zoomOut, zoomIn, ceil, hSmooth;
    float fov;  // degrees, 0 = the game's
} CamCfg;

typedef struct {
    int init;                  // 0: the next frame starts behind the player
    float yaw, yawOfs, cur, pitch, curP, cfgTilt;
    int wReset, wFp;           // wall zoom: snap next frame / first person now
    float curS, curB, wLast[3];
    unsigned wColl;
    int hOk;                   // height spring
    float hy, hv, hLast[3];
    unsigned hColl;
} CamState;

// 1 = a plane was hit: seg = from x y z 1, to x y z 1; plane out = nx ny nz d (the game's 0x561CD0)
typedef int (*CamRayFn)(void *ctx, const float *seg, float *plane);

static int CamFinite(float v) { return v == v && v - v == 0.0f; }  // (also without _finite)
static float CamClampF(float v, float lo, float hi, float def) { return v != v ? def : v < lo ? lo : (v > hi ? hi : v); }

// Playable Eileen's LoadCameraSettings clamps (raw = atof of each key's text). A NaN text ("nan") reads as the default.
static void CamCfgFrom(const float *raw, CamCfg *c)
{
    memset(c, 0, sizeof *c);
    c->on = raw[CAMK_ENABLE] != 0.0f;
    c->dist = CamClampF(raw[CAMK_DISTANCE], 0.0f, 2000.0f, 800.0f);
    c->side = CamClampF(raw[CAMK_SIDE], -500.0f, 500.0f, 0.0f);
    c->height = CamClampF(raw[CAMK_HEIGHT], 0.0f, 600.0f, 330.0f);
    c->fov = raw[CAMK_FOV];
    if (!(c->fov == 0.0f || (c->fov >= 20.0f && c->fov <= 120.0f))) c->fov = 63.0f;
    c->tiltDeg = -CamClampF(raw[CAMK_TILTDOWN], -68.0f, 68.0f, 17.0f);
    c->smooth = CamClampF(raw[CAMK_SMOOTHING], 0.01f, 1.0f, 0.2f);
    c->turnSpeed = fabsf(CamClampF(raw[CAMK_TURNSPEED], -1.0f, 1.0f, 0.1f));
    c->tiltSpeed = fabsf(CamClampF(raw[CAMK_TILTSPEED], -1.0f, 1.0f, 0.1f));
    c->invX = raw[CAMK_INVERTX] != 0.0f;
    c->invY = raw[CAMK_INVERTY] != 0.0f;
    c->follow = raw[CAMK_FOLLOW] != 0.0f;
    c->wall = raw[CAMK_WALLZOOM] != 0.0f;
    c->margin = CamClampF(raw[CAMK_WALLMARGIN], 0.0f, 200.0f, 50.0f);
    c->zoomOut = CamClampF(raw[CAMK_ZOOMOUT], 0.001f, 1.0f, 0.06f);
    c->zoomIn = CamClampF(raw[CAMK_ZOOMIN], 0.001f, 1.0f, 0.25f);
    c->ceil = CamClampF(raw[CAMK_CEILING], 0.0f, 2000.0f, 450.0f);
    c->hSmooth = CamClampF(raw[CAMK_STAIRS], 0.0f, 1.0f, 0.25f);
    c->fixedCams = raw[CAMK_FIXED] != 0.0f;
}

// The tick rate's helpers (Playable Eileen's RateK / RateExp): per-30 Hz-tick amounts, unchanged at 30 ticks a second.
static int CamSaneRate(int r) { return r >= 10 && r <= 240 ? r : 30; }
static float CamRateK(int rate) { rate = CamSaneRate(rate); return rate == 30 ? 1.0f : 30.0f / (float)rate; }
static float CamRateExp(float a, float k)
{
    if (k == 1.0f || !(a > 0.0f)) return a;
    if (!(a < 1.0f)) return a;  // 1 = at once
    return 1.0f - powf(1.0f - a, k);
}
static float CamDist2(const float *a, const float *b)
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

// The look: own yaw turned by the look input (or her facing with Follow), tilt, both eased. 0 = the angles went
// non-finite: the state restarts behind the player next frame and this frame shows the game's camera.
static int CamLook(CamState *s, const CamCfg *c, float facing, float lx, float ly, float rk)
{
    const float tilt = c->tiltDeg * CAM_DEG;
    float turn, tl, smooth, dy;
    if (!s->init) {
        s->yaw = facing;
        s->yawOfs = 0.0f;
        s->cur = s->yaw;
        s->pitch = s->curP = s->cfgTilt = tilt;
        s->init = 1;
        s->wReset = 1;
        s->hOk = 0;
    }
    if (tilt != s->cfgTilt) s->pitch = s->cfgTilt = tilt;  // a new TiltDown from the menu or the ini
    if (c->follow) s->yaw = facing;
    if (!CamFinite(lx)) lx = 0.0f;
    if (!CamFinite(ly)) ly = 0.0f;
    turn = c->turnSpeed * lx * rk;
    if (c->invX) turn = -turn;
    if (c->follow) s->yawOfs += turn;
    else s->yaw += turn;
    tl = c->tiltSpeed * ly * rk;
    if (!c->invY) tl = -tl;  // off: the game's way
    s->pitch += tl;
    if (!(fabsf(s->pitch) <= CAM_PITCH_MAX)) s->pitch = s->pitch < 0.0f ? -CAM_PITCH_MAX : CAM_PITCH_MAX;  // NaN too
    smooth = CamRateExp(c->smooth, rk);
    dy = s->yaw + s->yawOfs - s->cur;
    dy -= floorf(dy * 0.15915494f + 0.5f) * 6.2831855f;
    s->cur += dy * smooth;
    s->curP += (s->pitch - s->curP) * smooth;
    if (!CamFinite(s->cur + s->curP + s->yaw + s->yawOfs)) {
        s->init = 0;
        return 0;
    }
    return 1;
}

// StairSmoothing: the player's height through a critically damped spring (omega per 30 Hz tick), soft-limited to trail
// by about CAM_MAX_LAG (hard 2.5 x); snaps on a new room (coll), a jump, the first frame or StairSmoothing 0. Returns the
// height the camera uses (world y grows downward).
static float CamHeight(CamState *s, const CamCfg *c, const float *pos, unsigned coll, float rk)
{
    const float y = pos[1];
    const int snap = !(c->hSmooth > 0.0f) || !s->hOk || coll != s->hColl || !CamFinite(s->hv) || !CamFinite(s->hy) ||
                     !(CamDist2(pos, s->hLast) < CAM_TELE2);
    if (snap) {
        s->hy = y;
        s->hv = 0.0f;
        s->hColl = coll;
        s->hOk = 1;
    } else {
        const float w = c->hSmooth, x = w * rk;
        const float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        const float cc = s->hy - y, t = (s->hv + w * cc) * rk;
        float lag, al;
        s->hv = (s->hv - w * t) * e;
        lag = (cc + t) * e;
        al = fabsf(lag);
        if (al > CAM_MAX_LAG) {
            float nl = CAM_MAX_LAG + (al - CAM_MAX_LAG) * (rk == 1.0f ? 0.7f : powf(0.7f, rk));
            if (nl > CAM_MAX_LAG * 2.5f) nl = CAM_MAX_LAG * 2.5f;
            lag = lag < 0.0f ? -nl : nl;
        }
        s->hy = y + lag;
    }
    memcpy(s->hLast, pos, sizeof s->hLast);
    return s->hy;
}

// From start, unit direction d, wanted length wl: how far the eye may go and keep the margin from the first wall plane
// (*res, 0..wl), and where the segment crosses that plane minus 2 (*hit, 0..wl; wl when it does not cross).
static void CamRay(const float *start, const float *d, float wl, float margin, CamRayFn ray, void *ctx, float *res, float *hit)
{
    float seg[8], n[4], a, b, mn, am, sx;
    const float wlm = wl + margin;
    int k;
    for (k = 0; k < 3; ++k) {
        seg[k] = start[k];
        seg[4 + k] = start[k] + d[k] * wlm;
    }
    seg[3] = seg[7] = 1.0f;
    *res = *hit = wl;
    if (!ray || !ray(ctx, seg, n)) return;
    a = n[0] * seg[0] + n[1] * seg[1] + n[2] * seg[2] + n[3];
    b = n[0] * seg[4] + n[1] * seg[5] + n[2] * seg[6] + n[3];
    if (a < 0.0f) {  // seen from the start's side
        a = -a;
        b = -b;
    }
    if (b < 0.0f) {  // the segment crosses the plane
        float x = a / (a - b) * wlm - 2.0f;
        if (!(0.0f < x)) x = 0.0f;
        if (!(wl >= x)) x = wl;
        *hit = x;
    }
    mn = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * margin;
    if (!(mn > b)) return;  // the end is still the margin in front of the plane (or NaN): no limit
    am = a - mn;
    if (!(am > 0.0f)) {
        *res = 0.0f;  // the start itself is within the margin
        return;
    }
    sx = am / (a - b) * wlm;
    if (!(wl < sx)) *res = sx;
}

#define CAMF_WALL 1   // the wall zoom holds the eye in (more than 1 unit short of the wanted distance)
#define CAMF_FP 2     // first person (the eye closer than CAM_WALL_MIN to the shoulder point)
#define CAMF_FIXED 4  // no wall zoom this frame (WallZoom off, Distance 0, or no room collision loaded)
// The eye. R / F = the camera's right and forward rows (0x4FEEF0: rows right, down, forward), pos = the player's
// position, hy = CamHeight's height, coll = the room collision file (a change snaps), collLoaded = it is loaded.
// 0 = the eye went non-finite: the caller shows the game's camera and the state restarts behind the player.
static int CamEye(CamState *s, const CamCfg *c, const float *R, const float *F, const float *pos, float hy, unsigned coll,
                  int collLoaded, float rk, CamRayFn ray, void *ctx, float *eye, int *flags)
{
    float P[3], Sd[3], S[3], D[3], hp, sg, sl, res, hit = 0.0f, wl;
    int k;
    *flags = 0;
    if (!c->wall || !(c->dist > 0.0f) || !collLoaded) {
        s->wReset = 1;
        s->wFp = 0;
        for (k = 0; k < 3; ++k) eye[k] = (k == 1 ? hy - c->height : pos[k]) - F[k] * c->dist + R[k] * c->side;
        *flags = CAMF_FIXED;
        if (!CamFinite(eye[0] + eye[1] + eye[2])) {
            s->init = 0;
            return 0;
        }
        return 1;
    }
    if (coll != s->wColl) {
        s->wColl = coll;
        s->wReset = 1;
    }
    if (!(CamDist2(pos, s->wLast) < CAM_TELE2)) s->wReset = 1;
    memcpy(s->wLast, pos, sizeof s->wLast);
    hp = c->height < CAM_PIVOT_MAX ? c->height : CAM_PIVOT_MAX;
    P[0] = pos[0];
    P[1] = hy - hp;
    P[2] = pos[2];
    sg = c->side < 0.0f ? -1.0f : 1.0f;
    for (k = 0; k < 3; ++k) Sd[k] = R[k] * sg;
    sl = fabsf(c->side);
    res = sl;
    if (!(sl < 1.0f)) CamRay(P, Sd, sl, c->margin, ray, ctx, &res, &hit);
    if (s->wReset) {
        s->curS = res;
    } else {
        const float eased = s->curS + (sl - s->curS) * CamRateExp(c->zoomOut, rk);
        s->curS = eased < res ? eased : res;
    }
    for (k = 0; k < 3; ++k) S[k] = P[k] + Sd[k] * s->curS;
    D[0] = -F[0] * c->dist;
    D[1] = -F[1] * c->dist - c->height + hp;
    D[2] = -F[2] * c->dist;
    wl = sqrtf(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]);
    for (k = 0; k < 3; ++k) D[k] /= wl;
    CamRay(S, D, wl, c->margin, ray, ctx, &res, &hit);
    if (c->ceil > 0.0f && D[1] < 0.0f) {  // the collision has no ceilings: stay within CeilingLimit above the feet
        float lim = -(c->ceil - hp) / D[1];
        if (!(0.0f < lim)) lim = 0.0f;
        if (!(res < lim)) res = lim;
        if (!(hit < lim)) hit = lim;
    }
    if (s->wReset) {
        s->curB = res;
    } else if (s->curB < res || !(c->zoomIn < 1.0f)) {  // growing (or zoom in at once)
        const float eased = s->curB + (wl - s->curB) * CamRateExp(c->zoomOut, rk);
        s->curB = eased < res ? eased : res;
    } else {  // shrinking: ease in, but never past the wall plane
        const float eased = s->curB - (s->curB - res) * CamRateExp(c->zoomIn, rk);
        s->curB = hit >= eased || !CamFinite(hit) ? eased : hit;
    }
    s->wReset = 0;
    if (!s->wFp ? s->curB < CAM_WALL_MIN : !(CAM_WALL_MIN + CAM_WALL_HYST < s->curB)) {  // first person
        s->wFp = 1;
        for (k = 0; k < 3; ++k) eye[k] = P[k] + F[k] * CAM_FRONT;
        *flags |= CAMF_FP | CAMF_WALL;
    } else {
        s->wFp = 0;
        for (k = 0; k < 3; ++k) eye[k] = S[k] + D[k] * s->curB;
        if (s->curB < wl - 1.0f) *flags |= CAMF_WALL;
    }
    if (!CamFinite(eye[0] + eye[1] + eye[2])) {
        s->init = 0;
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------- who owns the view (the defer rule)
// The hook site 0x4F7080 as found: stock bytes, a jmp into Playable Eileen's dll, into this dll, elsewhere, or other bytes.
enum { CAMS_STOCK, CAMS_JMP_PE, CAMS_JMP_US, CAMS_JMP_OTHER, CAMS_OTHER };
// The owners: none (another mod holds the site, or nothing can hook), SH4 Refined, Playable Eileen, or "wait" (Playable
// Eileen's dll has its camera code but has not run its patches yet: Refined installs nothing and looks again on the
// first game tick, when every DllMain has run).
enum { CAM_OWN_NONE, CAM_OWN_REFINED, CAM_OWN_PE, CAM_OWN_WAIT };
// peMapped: dsound.dll with the SH4PE_Patches export is in the process. peCamCode: its image has the camera (version up to
// 1.18, or the camera's log text). peRan: its patch list has entries (its DllMain patched the exe). peListsSite: that list
// covers 0x4F7080. atTick: the first-tick re-check (every DllMain has run). *anomaly: 1 = an inconsistent state.
static int CamDecideOwner(int site, int peMapped, int peCamCode, int peRan, int peListsSite, int atTick, int *anomaly)
{
    *anomaly = 0;
    switch (site) {
    case CAMS_JMP_PE: return CAM_OWN_PE;
    case CAMS_JMP_US: return CAM_OWN_REFINED;
    case CAMS_JMP_OTHER:
    case CAMS_OTHER: *anomaly = 1; return CAM_OWN_NONE;  // the cheat table's Over The Shoulder, another camera mod
    default: break;
    }
    if (!peMapped) return CAM_OWN_REFINED;
    if (peRan && peListsSite) {  // it lists the site but the bytes are stock: it took the hook back out (or failed)
        *anomaly = 1;
        return CAM_OWN_PE;  // never two owners: Refined stays out
    }
    if (peRan) return CAM_OWN_REFINED;          // it patched the exe without the camera (a build without it, or its
                                                 // byte check failed): Refined's camera
    if (!peCamCode) return CAM_OWN_REFINED;     // a build without the camera that has not run yet
    return atTick ? CAM_OWN_REFINED : CAM_OWN_WAIT;  // at the tick: it never patched anything (inactive): Refined's camera
}

// ---------------------------------------------------------------- v3.37 CP337: one setting's next value
// The pause menu's Camera page (extramenu.inc) steps the settings with these (v3.33's Options page did the same inline).
// A range row (lo .. hi by st): Left / Right (wrap 0) clamp at the ends, Enter / a click (wrap 1) steps up and wraps to
// lo; a value off the grid (an ini edit) goes to its neighbour on the grid in that direction.
static float CamStepRange(float raw, float lo, float hi, float st, int dir, int wrap)
{
    float t, v;
    if (!(st > 0.0f) || !CamFinite(raw)) return lo;
    t = (raw - lo) / st;
    v = lo + (dir > 0 ? floorf(t + 1e-3f) + 1.0f : ceilf(t - 1e-3f) - 1.0f) * st;
    if (v > hi + 1e-4f) v = wrap ? lo : hi;
    if (v < lo - 1e-4f) v = wrap ? hi : lo;
    return v;
}
// A list row's next option of n (lists always wrap); idx outside 0 .. n-1 (Custom, unknown) starts at the first / last.
static int CamStepList(int idx, int n, int dir)
{
    if (n <= 0) return 0;
    if (idx < 0 || idx >= n) return dir > 0 ? 0 : n - 1;
    return ((idx + (dir > 0 ? 1 : -1)) % n + n) % n;
}
// Left / Right with the game's menu repeat (InputMenuKeyRepeat 0x4F85A0: a new press at once, then while held every 2nd
// tick from the 16th), on a counter of its own: 0x4F85A0's timer belongs to the pause's own Up / Down (two calls a tick
// would run it twice as fast). held / pressed = the direction bits (0x5542A0 / 0x5542C0: 4 Left, 8 Right). t < 0 waits for
// a new press (a key still held when the page opened does not run on). Returns -1 Left, +1 Right, 0 nothing (or both).
typedef struct { int t; } CamRepeat;
static void CamRepeatReset(CamRepeat *r) { r->t = -1; }
static int CamRepeatStep(CamRepeat *r, int held, int pressed)
{
    int bits = 0;
    held &= 0x0C;
    pressed &= 0x0C;
    if (pressed) r->t = 0;
    if (r->t < 0) return 0;
    if (!held && !pressed) {
        r->t = 0;
        return 0;
    }
    if (r->t == 0) bits = held | pressed;
    else if (r->t >= 16 && (r->t & 1) == 0) bits = held;
    if (++r->t > 1000) r->t -= 2;  // (held for minutes: keeps the every-2nd-tick rhythm)
    return bits == 4 ? -1 : bits == 8 ? 1 : 0;
}

#endif
