// v3.59 CT359: the pure rules of omnilatch.inc (the control-direction latch's release, the Shoulder camera's ignored
// look-rate trigger, Lock-On Walk's step gate). No game memory, no Windows calls: test\omnilatch_sim_test.c
// (test\build_omnilatch_test.cmd). Plan: staging/v359_feedback/w9_controls/PLAN.md.
//
// The game's control-direction latch 0x502040 (CameraControlDirectionTimer, called once a frame at 0x502794) keeps the
// player's control frame on the old camera after a room start (1.6 s), a drawn-camera jump (row dots < 0.71: 1.8 s) or a
// fast turn of the stock rig ((|look rate| - pi/2) x 0.716 s, at most 1.8 s), blends it toward the camera in its last
// second, and lets go early only when the stick moved more than 0.7 (vector distance) from where it was at the latch.
// The 8-way stick moves 0.77 at every 45-degree step; an analog stick (Omni-Directional Movement On) turns smoothly and
// rarely gets that far, so the latch holds its full time and the blend turns the player without any input.
#ifndef OMNILATCH_SIM_H
#define OMNILATCH_SIM_H

#include <math.h>
#include <string.h>

#define LT_PI 3.14159265f
#define LT_HALF_PI 1.57079633f
#define LT_RATE_K 0.71619719f      // the game's (rate - pi/2) x this (0x5BF7C0)
#define LT_RATE_MAX 1.8f           // its clamp (0x5B7EB4)
#define LT_COS_DEF 0.96592583f     // cos 15 degrees: Refined's release angle
#define LT_MIN_PUSH 0.25f          // a stick vector shorter than this counts as centred (Omni writes 0 or >= 0.6; 8-way ~1)
#define LT_JUMP_DOT 0.71f          // the game's drawn-camera jump test (0x5BF7C4)

// [Input] OmniLatch: 0 = the game's own latch (the episodes are still logged), 1 = Refined's release rule (default)
enum { LT_STOCK, LT_REFINED, LT_MODES };
static const char *const kLtModeName[LT_MODES] = { "0", "1" };

// The text of an ini value, trimmed (spaces, tabs, line ends) and in lower case, into w (at most n - 1 characters).
// Returns its length, or -1 when it is longer.
static int LtWord(const char *s, char *w, int n)
{
    int len, i;
    if (!s) s = "";
    while (*s == ' ' || *s == '\t') ++s;
    for (len = 0; s[len]; ++len) {}
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) --len;
    if (len >= n) return -1;
    for (i = 0; i < len; ++i) w[i] = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];
    w[len] = 0;
    return len;
}
// [Input] OmniLatch: "0" / "Off" / "Stock" = 0, "1" / "On" = 1 (any case, spaces around it), empty = 1; anything else = 1
// with *known = 0 (the caller logs it once).
static int LtParse(const char *s, int *known)
{
    char w[8];
    const int n = LtWord(s, w, (int)sizeof w);
    if (known) *known = 1;
    if (n == 0) return LT_REFINED;
    if (n > 0 && (!strcmp(w, "0") || !strcmp(w, "off") || !strcmp(w, "stock"))) return LT_STOCK;
    if (n > 0 && (!strcmp(w, "1") || !strcmp(w, "on"))) return LT_REFINED;
    if (known) *known = 0;
    return LT_REFINED;
}

// Refined's rule acts with Omni-Directional Movement On, or while an over-the-shoulder camera draws the view.
static int LtActive(int mode, int omniOn, int otsView) { return mode == LT_REFINED && (omniOn || otsView); }

// Why a running latch is let go (Refined's rule, on top of the game's 0.7): the stick's angle moved more than the release
// angle (cosMax = its cosine) from the latched one, the stick was centred at the latch and is pushed now, or it was
// pushed and is centred now. lx, ly = the stick at the latch (gCamWorld.stickX / Y), cx, cy = now (the same getters
// 0x530610 / 0x530630). A NaN anywhere holds (the game's own rule decides).
enum { LTR_HOLD, LTR_ANGLE, LTR_PUSHED, LTR_LETGO, LTR_N };
static int LtRelease(float lx, float ly, float cx, float cy, float minLen, float cosMax)
{
    const float l2 = lx * lx + ly * ly, c2 = cx * cx + cy * cy, m2 = minLen * minLen;
    int lp, cp;
    if (!(l2 == l2) || !(c2 == c2) || !(cosMax == cosMax)) return LTR_HOLD;
    lp = l2 >= m2;
    cp = c2 >= m2;
    if (!lp && !cp) return LTR_HOLD;
    if (!lp) return LTR_PUSHED;
    if (!cp) return LTR_LETGO;
    return lx * cx + ly * cy < cosMax * sqrtf(l2 * c2) ? LTR_ANGLE : LTR_HOLD;
}
// The angle between two stick vectors in degrees (0 when either is centred or not finite).
static float LtStickDeg(float lx, float ly, float cx, float cy)
{
    const float l = sqrtf(lx * lx + ly * ly), c = sqrtf(cx * cx + cy * cy);
    float d;
    if (!(l > 1e-6f) || !(c > 1e-6f)) return 0.0f;
    d = (lx * cx + ly * cy) / (l * c);
    if (!(d == d)) return 0.0f;
    d = d > 1.0f ? 1.0f : d < -1.0f ? -1.0f : d;
    return acosf(d) * (180.0f / LT_PI);
}
// The game's stick distance (its 0.7 rule) between the latched and the current stick.
static float LtStickDist(float lx, float ly, float cx, float cy)
{
    const float dx = cx - lx, dy = cy - ly;
    return sqrtf(dx * dx + dy * dy);
}

// The seconds the stock rig's look rate (rad/s, its length) would hold the latch: (rate - pi/2) x 0.716, 0 .. 1.8.
static float LtRateHold(float rate)
{
    float t;
    if (!(rate == rate)) return 0.0f;
    t = (rate - LT_HALF_PI) * LT_RATE_K;
    return t < 0.0f ? 0.0f : t > LT_RATE_MAX ? LT_RATE_MAX : t;
}
// The latch's trigger this frame, as 0x502040 decides it before the release test: a room start / cut (cut != 0), a
// drawn-camera jump (any row dot < 0.71), the rig's look rate raising the timer (hold > the decremented timer), or none.
enum { LTT_NONE, LTT_CUT, LTT_JUMP, LTT_RATE, LTT_N };
static int LtTrigger(int cut, float dot0, float dot1, float dot2, float hold, float timerLeft)
{
    if (cut) return LTT_CUT;
    if (dot0 < LT_JUMP_DOT || dot1 < LT_JUMP_DOT || dot2 < LT_JUMP_DOT) return LTT_JUMP;  // (NaN dots: not a jump, as the game's jnp)
    return hold > timerLeft ? LTT_RATE : LTT_NONE;
}
// The timer after the game's decrement and clamp: max(timer - dt, 0).
static float LtTimerLeft(float timer, float dt)
{
    const float t = timer - dt;
    return t < 0.0f || !(t == t) ? 0.0f : t;
}

// Wrap an angle into -pi .. pi.
static float LtWrapAngle(float a)
{
    if (!(a == a) || a > 1e6f || a < -1e6f) return 0.0f;
    while (a > LT_PI) a -= 2.0f * LT_PI;
    while (a < -LT_PI) a += 2.0f * LT_PI;
    return a;
}
// The yaw of the drawn camera's flattened forward as 0x502040's untimed branch builds it from the matrix rows
// (row 1 = up, row 2 = forward; k = 1 - m21^2; f = k row2 - m21 row1), atan2(x, z) as PlayerControlDirSet. 0 when flat.
static float LtCamYaw(const float *m)
{
    const float k = 1.0f - m[9] * m[9], u = -m[9];
    const float x = k * m[8] + u * m[4], z = k * m[10] + u * m[6];
    if (!(x * x + z * z > 1e-12f)) return 0.0f;
    return atan2f(x, z);
}

// ---------------------------------------------------------------- Lock-On Walk (the ready stance's step gate)
// In the ready stance (player mode 2: the weapon raised, locked on or not) the game moves the player only while the
// per-frame move amount (flags+0 = push x control length x dt x 30) passes 0.9 (0x53462E, the step 0x534620; the same
// gate picks the step clip at 0x53CA17). Playable Eileen points both at 0.9 x 30 / rate. So an analog push below about
// 0.9, a latch blend (control length < 1) or 60+ fps without Playable Eileen gives no step. [Gameplay] LockOnWalk On:
// any push steps: the amount is raised to what a full push gives (the gate's own full value) for this frame.
// rate = the game's tick rate ([0x5D5248]); stockGate = the gate operand is the game's own 0.9 (no Playable Eileen):
// then 1.0 (the 30 Hz full push, above 0.9 at every rate); else 30 / rate (Playable Eileen's gate is 0.9 x that).
static float LwFull(int rate, int stockGate)
{
    if (rate < 30 || rate > 1000) rate = 30;
    return stockGate ? 1.0f : 30.0f / (float)rate;
}
// The amount to use: raised to full when on, in the ready stance, third person, pushed and below full; else unchanged.
static float LwForward(int on, int mode, int thirdPerson, float fwd, float full)
{
    if (!on || mode != 2 || !thirdPerson || !(fwd > 0.0f) || !(full > 0.0f)) return fwd;
    return fwd < full ? full : fwd;
}
// The step's length scale: the game's own 9.1 per call (its operand stock) was tuned for 30 calls a second, so at a
// higher rate the step is scaled by 30 / rate; Playable Eileen's operand already holds its rate factor (1).
static float LwStepScale(int rate, int stockStep)
{
    if (!stockStep || rate <= 30 || rate > 1000) return 1.0f;
    return 30.0f / (float)rate;
}
enum { LW_OFF, LW_ON, LW_MODES };
static const char *const kLwName[LW_MODES] = { "Off", "On" };
// [Gameplay] LockOnWalk: "Off" / "0" = Off, "On" / "1" = On (any case, spaces around it), empty = On (the default);
// anything else = On with *known = 0.
static int LwParse(const char *s, int *known)
{
    char w[8];
    const int n = LtWord(s, w, (int)sizeof w);
    if (known) *known = 1;
    if (n == 0) return LW_ON;
    if (n > 0 && (!strcmp(w, "0") || !strcmp(w, "off"))) return LW_OFF;
    if (n > 0 && (!strcmp(w, "1") || !strcmp(w, "on"))) return LW_ON;
    if (known) *known = 0;
    return LW_ON;
}

#endif
