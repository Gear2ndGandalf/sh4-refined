// v3.47 RG347: Refined's own Regeneration, the pure rules (regen.inc runs them on the game thread from the 0x4EF982
// tick; native test test\regen_sim_test.c, test\build_regen_test.cmd). Plan: staging/v347_feedback/w4_regen/PLAN.md.
// Contract with Playable Eileen: SH4PlayableEileen/staging/s59_regen/REGEN_CONTRACT.md.
//
// "Like Playable Eileen's" (its RegenFrame / RegenGameFrame, v1.23.2): Off / x1 / x1.25 / x2 = 0 / 15 / 18.75 / 30 % of
// max HP a minute (PE's [Defense] RegenRate for those values, the same at any max HP); no healing for RegenDelay seconds
// (10..120) after any HP loss (a drop of more than 0.01 HP between two ticks); up to full HP (RegenUpTo 100); never at
// 0 HP, while paused (the clock stands still and nothing is sampled), in an event / door / scripted input / dying /
// load / movie (the clock runs, no heal), while the game's own Room 302 heal runs (no stacking), or while she stands
// in a ghost or haunting aura (Refined's own check: the game's aura sums of this frame are above 0). A door, a load, a
// stall of the game loop (> 1 s) or a new max HP restarts the HP sample (no loss or rise is counted across it; the
// wait keeps running). dt = 1 / the game's tick rate, clamped to 0.1 s.
#ifndef REGEN_SIM_H
#define REGEN_SIM_H

#include <string.h>
#include <stdlib.h>

#define RG_LEVELS 4          // Off, x1, x1.25, x2 (the row's values 0..3; the v3.46 row's "Custom" is PE's only)
#define RG_DELAY_MIN 10
#define RG_DELAY_MAX 120
#define RG_DELAY_DEF 30      // Playable Eileen's own default
#define RG_LOSS_EPS 0.01f    // PE: a drop bigger than this is a hit, a grab, an aura, a drain
#define RG_DT_MAX 0.1f
static const char *const kRgLevelName[RG_LEVELS] = { "Off", "x1", "x1.25", "x2" };
static const float kRgRatePct[RG_LEVELS] = { 0.0f, 15.0f, 18.75f, 30.0f };  // % of max HP a minute (PE's RegenRate)

static int RgStrEqI(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return *a == *b;
}
// [Gameplay] Regen: Off / x1 / x1.25 / x2 (any case), also On (= x1). *known = 0 for any other text (-> Off).
static int RgParseLevel(const char *s, int *known)
{
    int v;
    if (known) *known = 0;
    if (!s) return 0;
    for (v = 0; v < RG_LEVELS; ++v)
        if (RgStrEqI(s, kRgLevelName[v])) {
            if (known) *known = 1;
            return v;
        }
    if (RgStrEqI(s, "On")) {
        if (known) *known = 1;
        return 1;
    }
    return 0;
}
static int RgClampDelay(int s) { return s < RG_DELAY_MIN ? RG_DELAY_MIN : s > RG_DELAY_MAX ? RG_DELAY_MAX : s; }
// A seed from Playable Eileen's [Defense] Regen / RegenRate texts (NULL = the key is missing: PE's defaults 1 / 15):
// 0 Off, 1..3 a standard rate, -1 another rate (no seed)
static int RgLevelFromPe(const char *regen, const char *rate)
{
    const double on = regen ? atof(regen) : 1.0, r = rate ? atof(rate) : 15.0;
    int v;
    if (on == 0.0 || r <= 0.0) return 0;
    for (v = 1; v < RG_LEVELS; ++v) {
        const double d = r - (double)kRgRatePct[v];
        if (d > -0.01 && d < 0.01) return v;
    }
    return -1;
}
// HP a second at this level and max HP
static float RgPerSec(int level, float mx)
{
    if (level <= 0 || level >= RG_LEVELS || !(mx > 0.0f)) return 0.0f;
    return mx * kRgRatePct[level] / 100.0f / 60.0f;
}
// dt from the game's tick rate [0x5D5248] (PE: 10..240, else 1/30), clamped to RG_DT_MAX
static float RgDt(int rate)
{
    const float dt = rate >= 10 && rate <= 240 ? 1.0f / (float)rate : 1.0f / 30.0f;
    return dt < RG_DT_MAX ? dt : RG_DT_MAX;
}

// ---------------------------------------------------------------- who heals (regen.inc RgWho)
enum { RGW_NA, RGW_OFF, RGW_HENRY, RGW_EILEEN, RGW_PE, RGW_LEGACY, RGW_N };
static const char *const kRgWhoName[RGW_N] = {
    "nobody (Refined's regeneration is not installed)", "nobody (Regeneration Off)", "SH4 Refined (Henry)",
    "SH4 Refined (Eileen: Playable Eileen's own regeneration is off)",
    "Playable Eileen's own regeneration (Refined's row grey, Off)",
    "Playable Eileen (before v1.23.2: no SH4PE_RegenActive; the row is its ini front end, Refined does not heal)" };
// pe: dsound.dll with SH4PE_Patches is loaded; hasActive / active: SH4PE_RegenActive exported / its value; hasEnabled /
// enabled: SH4PE_Enabled exported / its value (1 = Eileen); ready: Refined's regeneration is installed
static int RgWho(int ready, int level, int pe, int hasActive, int active, int hasEnabled, int enabled)
{
    if (pe && !hasActive) return RGW_LEGACY;
    if (pe && active) return RGW_PE;
    if (!ready) return RGW_NA;
    if (level <= 0) return RGW_OFF;
    return pe && hasEnabled && enabled ? RGW_EILEEN : RGW_HENRY;
}
// the row's mode: 0 Refined's own (its value, its ini key), 1 grey Off (PE's own heals), 2 PE's ini front end (v3.46)
enum { RGR_OWN, RGR_PE, RGR_LEGACY };
static int RgRowMode(int pe, int hasActive, int active)
{
    if (pe && !hasActive) return RGR_LEGACY;
    return pe && active ? RGR_PE : RGR_OWN;
}
// Life Steal's heal: 1 = through the game's own heal path (0x534B98's target: Playable Eileen's RegenHealAmt), because
// PE's BlockOtherRegen acts now and would put a direct rise back inside its wait; 0 = 0x52D1D0 directly
static int RgStealViaGame(int pe, int hasActive, int active) { return pe && (!hasActive || active); }

// ---------------------------------------------------------------- the tick
enum { RGS_OFF, RGS_HEAL, RGS_FULL, RGS_WAIT, RGS_AURA, RGS_R302, RGS_BUSY, RGS_DEAD, RGS_PAUSED, RGS_RESET, RGS_N };
static const char *const kRgStateName[RGS_N] = { "off", "healing", "full", "hurt", "near a ghost aura",
                                                 "Room 302's own heal", "busy", "no health", "paused", "a door or load" };
typedef struct {
    double clock;      // gameplay seconds (unpaused ticks with a valid sample)
    double hurtAt;     // clock at the last HP loss
    float lastHp, lastMax;
    unsigned room;     // stage << 16 | room of the sample
    int have;          // lastHp / lastMax / room are a valid sample
} RgState;
typedef struct {
    float hp, mx;      // gPlayer[0] hp / hpMax (0x10A0F28 / 0x10A0F4C)
    unsigned room;
    int gap;           // the game loop stalled (> 1 s since the last tick): a load
    int paused;        // game phase 2 (pause menu, inventory, map, Options, ...)
    int busy;          // not free for healing: event, door, scripted input, interaction, dying state, load, movie
    int aura;          // she stands in a ghost / haunting aura this frame
    int r302;          // the game's own Room 302 heal runs (stage 1, story flag 0x51F clear)
    float dt;          // this tick's seconds (RgDt)
    float perSec;      // HP a second Refined may heal now (0 = Refined does not heal: Off, PE's own, older PE)
    float delay;       // RegenDelay seconds
} RgIn;
static void RgReset(RgState *s)
{
    memset(s, 0, sizeof *s);
    s->hurtAt = -1.0e9;
    s->lastHp = -1.0f;
}
// One tick. Returns the HP to heal now (0 = none); *state = why (RGS_*); *lost = the HP lost this tick (0 = none).
// The caller heals by the result, then gives the HP it reads back to RgNoteHp (the next tick's sample).
static float RgStep(RgState *s, const RgIn *in, int *state, float *lost)
{
    const float hp = in->hp, mx = in->mx;
    float dt = in->dt, add = 0.0f, delta;
    int waiting;
    *lost = 0.0f;
    if (in->paused) {  // the clock stands still; the sample stays (a scripted loss in the pause counts afterwards)
        *state = RGS_PAUSED;
        return 0.0f;
    }
    if (!(mx > 0.0f) || !(hp == hp) || !(mx == mx) || in->gap || !s->have || in->room != s->room || mx != s->lastMax) {
        s->room = in->room;
        s->lastMax = mx;
        s->have = mx > 0.0f && hp == hp && mx == mx;
        s->lastHp = s->have ? hp : -1.0f;
        *state = RGS_RESET;
        return 0.0f;
    }
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > RG_DT_MAX) dt = RG_DT_MAX;
    s->clock += dt;
    delta = hp - s->lastHp;
    if (delta < -RG_LOSS_EPS) {
        s->hurtAt = s->clock;
        *lost = -delta;
    }
    waiting = s->clock - s->hurtAt < (double)in->delay;
    if (!(in->perSec > 0.0f)) *state = RGS_OFF;
    else if (!(hp > 0.0f)) *state = RGS_DEAD;
    else if (waiting) *state = RGS_WAIT;
    else if (in->aura) *state = RGS_AURA;
    else if (in->r302) *state = RGS_R302;
    else if (in->busy) *state = RGS_BUSY;
    else if (!(hp < mx)) *state = RGS_FULL;
    else {
        add = in->perSec * dt;
        if (hp + add > mx) add = mx - hp;
        *state = add > 0.0f ? RGS_HEAL : RGS_FULL;
        if (!(add > 0.0f)) add = 0.0f;
    }
    s->lastHp = hp + add;
    return add;
}
static void RgNoteHp(RgState *s, float hp)  // the HP after this tick's heal (read back)
{
    if (s->have && hp == hp) s->lastHp = hp;
}
// seconds left of the wait after a loss (0 = none)
static double RgWaitLeft(const RgState *s, float delay)
{
    const double left = (double)delay - (s->clock - s->hurtAt);
    return left > 0.0 ? left : 0.0;
}

// ---------------------------------------------------------------- a healing run, for the log (start / stop lines)
// A run starts at the first healing tick and ends at the first tick that is neither healing nor paused nor a door / load
// (those only suspend it: the sample restarts, healing goes on). RgRunStep returns 1 at a start, 2 at a stop (run->why =
// the stop's state), else 0.
typedef struct { int on, why; float gained, from; double start, at; } RgRun;
static int RgRunStep(RgRun *r, int state, float add, float hp, double clock)
{
    if (state == RGS_HEAL) {
        if (!r->on) {
            r->on = 1;
            r->gained = 0.0f;
            r->from = hp;
            r->start = clock;
            r->gained += add;
            r->at = clock;
            return 1;
        }
        r->gained += add;
        r->at = clock;
        return 0;
    }
    if (state == RGS_PAUSED || state == RGS_RESET || !r->on) return 0;
    r->on = 0;
    r->why = state;
    return 2;
}

#endif
