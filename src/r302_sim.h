// v3.50 R302: Room 302's look and walk settings for Henry and Eileen alike (room302.inc), pure parts. Native test:
// test\r302_sim_test.c (test\build_r302_test.cmd), which also compares the look step with a transcription of Playable
// Eileen v1.24.4's FpViewAngles / FpClampView / FpLookAfter. The hand-over contract: staging\v350_feedback\w6_room302\
// R302_CONTRACT.md.
#ifndef R302_SIM_H
#define R302_SIM_H
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------- the feature groups (bits of the hand-over)
#define R302F_LOOK 1u  // Look Down / Look Up limits + the exact pitch clamp + Look Feel: 0x52DF74, 0x52DCC6, 0x52DD14
#define R302F_WALK 2u  // Walk Speed: Room 302's walk speed (Refined's own Movement Speed operands 0x52D89C / 0x52D8E8)
#define R302F_ALL (R302F_LOOK | R302F_WALK)

// SH4 Refined's export SH4R_Room302 (winmm.dll, DATA, by name; ordinal 200). Written by the game thread only.
#define R3X_MAGIC 0x46523352u  // 'R3RF'
#define R3X_VERSION 1u
typedef struct {
    unsigned magic;               // R3X_MAGIC
    unsigned version;             // R3X_VERSION
    unsigned size;                // sizeof (R3Export)
    unsigned offer;               // the groups this build can take (R302F_*), fixed
    volatile long decided;        // 0 until Refined decided who owns each group (Init or the first game tick), then 1
    volatile long taken;          // the groups Refined runs now (hooks in / factor applied); final once decided
    volatile long lookTicks;      // Room 302 look ticks Refined handled (advances while its look runs)
    volatile float yawRate;       // the last look tick's yaw delta after Refined (radians a tick)
    volatile float pitchRate;     // its pitch delta after the clamp (radians a tick, + = up)
    volatile float pitch;         // the view's pitch after that tick (radians, + = up)
    volatile float lookDown;      // the limits in effect, degrees (50 / 25 = the game's)
    volatile float lookUp;
    volatile float lookFeel;      // 0 = no easing
    volatile float walkSpeed;     // 1 = the game's
} R3Export;
typedef char R3ExportSize[sizeof(R3Export) == 56 ? 1 : -1];

// Playable Eileen's export SH4PE_Room302Yield (dsound.dll, DATA, by name), as the contract asks for it
#define R3Y_MAGIC 0x32303352u  // 'R302'
typedef struct {
    unsigned magic;               // R3Y_MAGIC
    unsigned version;             // >= 1
    volatile long decided;        // 0 until PE's Init decided (before it writes its patches), then 1
    volatile long yield;          // R302F_* PE does not do and whose sites it left stock (valid once decided)
    unsigned peVersion;           // PE's FILEVERSION, major << 16 | minor (information only)
} R3PeYield;
typedef char R3PeYieldSize[sizeof(R3PeYield) == 20 ? 1 : -1];

// ---------------------------------------------------------------- who owns a group
enum { R3_OWN_NONE, R3_OWN_REFINED, R3_OWN_PE, R3_OWN_WAIT };
static const char *const kR3OwnName[4] = { "nobody", "SH4 Refined", "Playable Eileen", "waiting for Playable Eileen" };
enum { R3S_STOCK, R3S_OURS, R3S_OTHER };  // a group's sites: the game's bytes, Refined's, anything else (PE's, another mod's)
typedef struct {
    int peMapped;        // dsound.dll with the SH4PE_Patches export is in the process
    int yieldExp;        // it also exports SH4PE_Room302Yield with the magic and version >= 1
    int yieldDecided;    // ... whose decided field is 1
    unsigned yieldMask;  // ... its yield field
    int peListsSite;     // its SH4PE_Patches list covers one of the group's sites
    int sites;           // R3S_*
    int avail;           // Refined can do the group in this game (WALK: Movement Speed's operands are in)
    int atTick;          // the first-game-tick decision (every DllMain has run)
} R3OwnIn;
// The rule: never two owners, never a site Playable Eileen lists. *anomaly = 1 for a state that should not happen.
static int R3DecideOwner(const R3OwnIn *in, unsigned bit, int *anomaly)
{
    *anomaly = 0;
    if (in->peMapped) {
        if (!in->yieldExp) return R3_OWN_PE;  // v1.24.4 and older: PE keeps its Room 302 features (Eileen only)
        if (!in->yieldDecided) {
            if (!in->atTick) return R3_OWN_WAIT;
            *anomaly = 1;                      // its Init never decided: it stays its own, Refined stays out
            return R3_OWN_PE;
        }
        if (!(in->yieldMask & bit)) return R3_OWN_PE;
        if (in->peListsSite) {                 // it yields the group but lists one of its sites: nobody's
            *anomaly = 1;
            return R3_OWN_NONE;
        }
    }
    if (!in->avail) return R3_OWN_NONE;
    if (in->sites == R3S_OTHER) {
        *anomaly = 1;
        return R3_OWN_NONE;
    }
    return R3_OWN_REFINED;
}

// ---------------------------------------------------------------- the rows' values
// Look Down / Look Up: index 0 = the game's own (50 / 25 degrees; ini 0); Look Feel 0 = Off; Walk Speed index 8 = 1.
#define R3_DOWN_N 7
#define R3_UP_N 7
#define R3_FEEL_N 5
#define R3_WALK_N 11
#define R3_WALK_GAME 8
static const int kR3Down[R3_DOWN_N] = { 50, 60, 65, 70, 75, 80, 85 };
static const int kR3Up[R3_UP_N] = { 25, 35, 45, 60, 70, 80, 85 };
static const float kR3Feel[R3_FEEL_N] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f };
static const float kR3Walk[R3_WALK_N] = { 0.5f, 0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f };
enum { R3K_DOWN, R3K_UP, R3K_FEEL, R3K_WALK, R3K_N };
static const char *const kR3Key[R3K_N] = { "LookDownLimit", "LookUpLimit", "LookFeel", "WalkSpeed" };
static const char *const kR3Def[R3K_N] = { "85", "85", "1", "0.65" };
static const int kR3Count[R3K_N] = { R3_DOWN_N, R3_UP_N, R3_FEEL_N, R3_WALK_N };
static const unsigned kR3Group[R3K_N] = { R302F_LOOK, R302F_LOOK, R302F_LOOK, R302F_WALK };

static float R3Val(int k, int i)  // the value of row k's index i (degrees / factor)
{
    if (i < 0 || i >= kR3Count[k]) i = 0;
    return k == R3K_DOWN ? (float)kR3Down[i] : k == R3K_UP ? (float)kR3Up[i] : k == R3K_FEEL ? kR3Feel[i] : kR3Walk[i];
}
// A number of the ini (spaces around it allowed). 0 = not a number.
static int R3ParseNum(const char *s, float *out)
{
    char *end;
    double v;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    if (!*s) return 0;
    v = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end || !(v == v) || v > 1e6 || v < -1e6) return 0;
    *out = (float)v;
    return 1;
}
// The row index of an ini value: 0 or less = the game's (limits, Feel Off; Walk Speed: the game's 1), else the nearest
// row value (the lower one on a tie). *exact = it is a row value (or 0 for the game's).
static int R3IndexOf(int k, float v, int *exact)
{
    int i, best = 0;
    float bd = 1e9f;
    if (exact) *exact = 1;
    if (!(v > 0.0f)) {
        if (exact) *exact = v == 0.0f && k != R3K_WALK;
        return k == R3K_WALK ? R3_WALK_GAME : 0;
    }
    for (i = 0; i < kR3Count[k]; ++i) {
        const float d = fabsf(R3Val(k, i) - v);
        if (d < bd - 1e-4f) bd = d, best = i;
    }
    if (exact) *exact = bd < 1e-3f;
    return best;
}
// The ini text of row k's index i (the limits' game value is 0, as Playable Eileen writes it)
static void R3IniText(int k, int i, char *out, size_t n)
{
    char b[16];
    const float v = R3Val(k, i);
    if ((k == R3K_DOWN || k == R3K_UP) && i == 0) {
        strcpy_s(out, n, "0");
        return;
    }
    sprintf_s(b, sizeof b, "%g", (double)v);
    strcpy_s(out, n, b);
}
// The first-start seed of a missing key from Playable Eileen's [Room302]: peHas = its ini is there; body = its
// FirstPersonBody (PE applies the limits, the look easing and the walk speed only with her body: 0 -> the game's);
// peText = its key's text (LookDownLimit, LookUpLimit, Feel, WalkSpeed), "" = missing. Returns 1 = seeded from PE.
static int R3Seed(int k, int peHas, int body, const char *peText, char *out, size_t n)
{
    float v;
    strcpy_s(out, n, kR3Def[k]);
    if (!peHas) return 0;
    if (!body) {
        strcpy_s(out, n, k == R3K_WALK ? "1" : "0");
        return 1;
    }
    if (!peText || !peText[0] || !R3ParseNum(peText, &v)) return 0;  // PE's own default stands (= kR3Def)
    R3IniText(k, R3IndexOf(k, v, NULL), out, n);
    return 1;
}

// ---------------------------------------------------------------- the look (a port of PE v1.24.4's, both characters)
#define R3_PI 3.14159265f
#define R3_DEG 0.017453293f
#define R3_LIM_MAX 1.5533f      // 89 degrees: the view never passes it
#define R3_LOOK_TAU 0.04f       // the look-delta easing, seconds x Look Feel (PE FP_LOOK_TAU)
typedef struct {
    float yawRate, pitchRate;   // the eased deltas of the last tick (radians a tick)
} R3Ease;
typedef struct {
    int eased;                  // the easing changed a delta
    int clamped;                // the pitch delta was cut at a limit
    int up, down;               // ... which one
    int rebuilt;                // the view was rolled / past a limit and was rebuilt
    float pitch;                // the pitch after this tick (radians, + = up)
    float rollDeg, fromDeg;     // a rebuild's roll and its pitch before (for the log)
} R3StepOut;
// The game's limits in radians (the stock constants at 0x5BD6D8 / 0x5C5BC0)
#define R3_STOCK_UP 0.43633232f
#define R3_STOCK_DOWN -0.87266463f
// 1 = the look hook does something with these rows (all Game + Feel Off = the game's own look, untouched)
static int R3LookActive(int downIdx, int upIdx, int feelIdx) { return downIdx != 0 || upIdx != 0 || feelIdx != 0; }
// The operands' values: the stock tests at 0x52DCC6 / 0x52DD14 compare the game's two-tick-old pitch with these
static float R3UpLimit(int upIdx) { return upIdx <= 0 ? R3_STOCK_UP : (float)kR3Up[upIdx < R3_UP_N ? upIdx : R3_UP_N - 1] * R3_DEG; }
static float R3DownLimit(int downIdx)
{
    return downIdx <= 0 ? R3_STOCK_DOWN : -(float)kR3Down[downIdx < R3_DOWN_N ? downIdx : R3_DOWN_N - 1] * R3_DEG;
}
static int R3Finite(float v) { return v == v && v < FLT_MAX && v > -FLT_MAX; }
// The view (rows right, down, forward, eye; y down) as yaw and a continuous pitch (+ = up; past +-90 upside down)
static int R3ViewAngles(const float *m, float *yaw, float *pitch)
{
    const float rl = sqrtf(m[0] * m[0] + m[2] * m[2]);
    float h;
    if (!R3Finite(rl) || rl < 0.5f) return 0;
    *yaw = atan2f(-m[2], m[0]);
    h = m[8] * sinf(*yaw) + m[10] * cosf(*yaw);
    *pitch = atan2f(-m[9], h);
    return R3Finite(*pitch);
}
// The exact clamp on the view m: the pitch delta *pd may take the view at most to lo / hi (radians); a view outside them
// or rolled is rebuilt without roll at the limit. Returns 0 when the view could not be read (nothing changed).
static int R3ClampView(float *m, float *pd, float lo, float hi, R3Ease *e, R3StepOut *o)
{
    float yaw, p0, p1, d;
    int rolled;
    if (!R3ViewAngles(m, &yaw, &p0) || !R3Finite(*pd)) return 0;
    if (lo < -R3_LIM_MAX) lo = -R3_LIM_MAX;
    if (hi > R3_LIM_MAX) hi = R3_LIM_MAX;
    rolled = fabsf(m[1]) > 0.01f;
    if (p0 < lo - 0.004f || p0 > hi + 0.004f || rolled) {
        const float cx = m[1] * m[6] - m[2] * m[5], cy = m[2] * m[4] - m[0] * m[6], cz = m[0] * m[5] - m[1] * m[4];
        if (cx * m[8] + cy * m[9] + cz * m[10] > 0.0f) {  // the game's handedness: right x down = forward
            const float pc = p0 < lo ? lo : (p0 > hi ? hi : p0);
            const float sy = sinf(yaw), cyw = cosf(yaw), sp = sinf(pc), cp = cosf(pc);
            o->rollDeg = asinf(m[1] > 1.0f ? 1.0f : (m[1] < -1.0f ? -1.0f : m[1])) * 57.29578f;
            o->fromDeg = p0 * 57.29578f;
            m[0] = cyw; m[1] = 0.0f; m[2] = -sy;
            m[4] = sy * sp; m[5] = cp; m[6] = cyw * sp;
            m[8] = sy * cp; m[9] = -sp; m[10] = cyw * cp;
            o->rebuilt = 1;
            p0 = pc;
        }
    }
    p1 = p0 + *pd;
    if (p1 < lo) p1 = lo, o->down = 1;
    if (p1 > hi) p1 = hi, o->up = 1;
    d = p1 - p0;
    if (fabsf(d - *pd) > 1e-5f) o->clamped = 1;
    else o->up = o->down = 0;
    *pd = d;
    e->pitchRate = d;
    o->pitch = p1;
    return 1;
}
// One look tick after Room302LookApply (0x52DDA0): pd = P+0x60 (pitch delta), yd = P+0x64 (yaw delta, already added
// to by = P+0x84, the body yaw). Eases both by Look Feel (dt seconds), then clamps the pitch on the view m.
static void R3LookStep(R3Ease *e, float *m, float *pd, float *yd, float *by, float dt, float feel, float lo, float hi,
                       R3StepOut *o)
{
    memset(o, 0, sizeof *o);
    if (!R3Finite(*pd) || !R3Finite(*yd)) return;
    if (feel > 0.0f) {
        const float k = 1.0f - expf(-dt / (R3_LOOK_TAU * feel));
        float y = e->yawRate + k * (*yd - e->yawRate), p = e->pitchRate + k * (*pd - e->pitchRate);
        if (*yd == 0.0f && fabsf(y) < 1e-5f) y = 0.0f;
        if (*pd == 0.0f && fabsf(p) < 1e-5f) p = 0.0f;
        o->eased = y != *yd || p != *pd;
        *by += y - *yd;
        *yd = y;
        *pd = p;
    }
    e->yawRate = *yd;
    R3ClampView(m, pd, lo, hi, e, o);
}
// The tick length the game runs at ([0x5D5248] frames a second; PE FpDt)
static float R3Dt(int fps) { return fps >= 10 && fps <= 240 ? 1.0f / (float)fps : 1.0f / 30.0f; }

// ---------------------------------------------------------------- the walk
// Room302Walk's 10.5 (forward and side) x Movement Speed x Walk Speed while Refined owns the walk, in stage 1 only
// (stage 0x0A, the crawl through the hole, reads the same operands and keeps Movement Speed alone).
static float R3WalkFactor(int owned, int stage, float walk) { return owned && stage == 1 && walk > 0.0f ? walk : 1.0f; }
#endif
