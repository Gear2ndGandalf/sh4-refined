// ================================================================ HUD BARS: the pure state code (p2_hud candidate)
// Enemy Health Bars and the Eileen Curse Meter (hudbars.inc). No Windows, no game memory: test\hudbars_sim_test.c runs
// it at several frame rates. Every step takes dt in seconds and moves linearly, so a run at 30, 60, 144 or 240 frames a
// second gives the same values at the same moments (the trail's hold carries its overshoot into the fall).
#ifndef HUDBARS_SIM_H
#define HUDBARS_SIM_H
#include <math.h>

#define HB_FADE_IN_S    0.20f  // a bar fades in over this long ...
#define HB_FADE_OUT_S   0.80f  // ... and out over this long
#define HB_HIT_S        4.0f   // a bar stays up this long after the monster lost health
#define HB_TRAIL_HOLD_S 0.45f  // the pale "lost health" trail waits this long after a hit ...
#define HB_TRAIL_RATE   0.60f  // ... then drains at this share of the bar a second
#define HB_NEAR_R       1500.0f  // All Near: monsters this close (game units; Eileen is about 340 tall) ...
#define HB_NEAR_BAND    300.0f   // ... fading out over the last 300
#define HB_DT_MAX       0.10f  // a longer frame (load, hitch, gate closed) counts as 0.1 s

enum { HB_MODE_OFF, HB_MODE_LOCKED, HB_MODE_NEAR };

typedef struct {
    float alpha;     // 0..1, what is drawn
    float shown;     // the health share now (0..1)
    float trail;     // the pale part: the share before the last hits, draining to shown
    float hold;      // seconds the trail still waits
    float sinceHit;  // seconds since the share last dropped
    float downMax;   // ghosts: the largest down countdown seen in this down (the timer's full length)
    int init;
} HbBar;

static float HbClamp01(float x) { return x > 0.0f ? (x < 1.0f ? x : 1.0f) : 0.0f; }  // (NaN -> 0)

// How much a bar wants to show (0..1): locked or recently hit = 1; All Near = by distance; else 0.
static float HbWant(int mode, int locked, float sinceHit, float dist)
{
    float near_ = 0.0f;
    if (mode != HB_MODE_LOCKED && mode != HB_MODE_NEAR) return 0.0f;
    if (locked || sinceHit < HB_HIT_S) return 1.0f;
    if (mode == HB_MODE_NEAR && dist == dist) near_ = HbClamp01((HB_NEAR_R - dist) / HB_NEAR_BAND);
    return near_;
}

// One frame of a bar. frac = health share now (or the down timer's share), want = HbWant (0..1), seen = drawn this frame
// (on screen, not behind a wall). A drop of frac starts the trail's hold and the hit clock; a rise (healing, a ghost
// getting up) moves the trail up with it.
static void HbBarStep(HbBar *b, float frac, float want, int seen, float dt)
{
    float target, rest;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    frac = HbClamp01(frac);
    if (!b->init) {
        b->init = 1;
        b->alpha = 0.0f;
        b->shown = b->trail = frac;
        b->hold = 0.0f;
        b->sinceHit = 1e6f;
    }
    // 1. the frame's time passes with the last frame's state (the hold first, its overshoot drains), so an event lands
    //    at the same moment at every frame rate
    rest = dt;
    if (b->hold > 0.0f) {
        if (b->hold >= rest) {
            b->hold -= rest;
            rest = 0.0f;
        } else {
            rest -= b->hold;
            b->hold = 0.0f;
        }
    }
    b->trail -= HB_TRAIL_RATE * rest;
    if (b->trail < b->shown) b->trail = b->shown;
    b->sinceHit += dt;
    if (b->sinceHit > 1e6f) b->sinceHit = 1e6f;
    // 2. this frame's value: a drop (a hit, a drain) holds the trail where it is (a new hit while it drains: from there)
    if (frac < b->shown - 0.0005f) {
        b->hold = HB_TRAIL_HOLD_S;
        b->sinceHit = 0.0f;
    }
    b->shown = frac;
    if (b->trail < b->shown) b->trail = b->shown;
    if (b->trail > 1.0f) b->trail = 1.0f;
    target = seen ? HbClamp01(want) : 0.0f;
    if (b->alpha < target) {
        b->alpha += dt / HB_FADE_IN_S;
        if (b->alpha > target) b->alpha = target;
    } else if (b->alpha > target) {
        b->alpha -= dt / HB_FADE_OUT_S;
        if (b->alpha < target) b->alpha = target;
    }
}

// Ghosts: the share of the down countdown left. newDown = the ghost went down this frame (its full length is learnt
// again). The countdown only falls while it runs, so the largest value seen is its length.
static float HbDownFrac(float *downMax, float countdown, int newDown)
{
    if (!(countdown == countdown) || countdown < 0.0f) countdown = 0.0f;
    if (newDown || !(*downMax > 0.0f)) *downMax = countdown;
    if (countdown > *downMax) *downMax = countdown;
    return *downMax > 0.0f ? HbClamp01(countdown / *downMax) : 0.0f;
}

// ---------------------------------------------------------------- v3.30 HB330: what a ghost's bars show
// RE: staging/v330_feedback/w4_hud/RE_GHOST_HEALTH.md (matched decomp). The HP bar is always hp / hp_max (v3.29 drew the
// down countdown instead: empty on a stumble, full on a real down). Classes [work+0x3C]: 8 rising, 9 down with a zero
// countdown (rises at once), 0x0A down with a running countdown [[work+0x28]+0x98], 0x0B pinned by the Sword of Obedience
// (ghost types 0x0B..0x0F; the Patient 0x11 cannot be pinned, its 0x0B is death).
enum { HB_GS_STAND, HB_GS_RISING, HB_GS_STUMBLE, HB_GS_DOWN, HB_GS_PINNED, HB_GS_GONE };
static int HbIsGhostType(int type) { return (type >= 0x0B && type <= 0x0F) || type == 0x11; }
static int HbGhostPinnable(int type) { return type >= 0x0B && type <= 0x0F; }  // EnemyTypeIsGhost (0x464E00's test)
// pinReq = the Sword's "pin this ghost" mailbox [0xFD0BC0] names this task (the frame before the ghost reacts)
static int HbGhostState(int type, int cls, float countdown, int pinReq)
{
    if (HbGhostPinnable(type) && (cls == 0x0B || pinReq)) return HB_GS_PINNED;
    if (cls == 0x0B || cls >= 0x10) return HB_GS_GONE;
    if (cls == 0x0A) return countdown > 0.0f ? HB_GS_DOWN : HB_GS_STUMBLE;  // (0 for the tick before it rises)
    if (cls == 9) return HB_GS_STUMBLE;
    if (cls == 8) return HB_GS_RISING;
    return HB_GS_STAND;
}
// v3.32 DHS332: a ghost lying on the floor (Item Shine: no upright body to hide an item behind): down with a zero or a
// running countdown (9 / 0x0A) or pinned by the Sword (0x0B; the Patient's 0x0B is its death, also lying). Standing,
// staggering upright (classes below 8), rising (8) and scripted (>= 0x10) ghosts are not lying.
static int HbGhostLying(int type, int cls) { return HbIsGhostType(type) && (cls == 9 || cls == 0x0A || cls == 0x0B); }
// The health a downed ghost has when it rises (the get-up functions 0x4AF9E0 / 0x4B5720 / 0x47F990 / 0x47AD70 /
// 0x4BB960): only with no haunting aura on it (aura > 0: no heal, -1); at HP 0: 50 % of max (Andrew 0x0E 40 %); else
// hp + max / (count + 4), count = the get-up count before this rise (countOk 0 = unknown: -1). Returns -1 for none.
static float HbGhostRiseHp(int type, float hp, float hpMax, int count, int countOk, float aura)
{
    float r;
    if (!(hpMax > 0.0f) || !(hp == hp) || aura > 0.0f) return -1.0f;
    if (hp <= 0.0f) r = hpMax * (type == 0x0E ? 0.4f : 0.5f);
    else if (!countOk || count < 0 || count > 1000) return -1.0f;
    else r = hp + hpMax / ((float)count + 4.0f);
    return r < hpMax ? r : hpMax;
}
// the get-up count's offset in the type block [work+0x24] (-1 = none known)
static int HbGhostCountOff(int type)
{
    switch (type) {
    case 0x0B: return 0x4C;          // VictimWork.unk_4C
    case 0x0C: case 0x0D: return 0x94;  // PickgirlWork / JasperWork .unk_94
    case 0x0E: return 0x60;          // FatWork.down_count
    case 0x0F: case 0x11: return 0x14;  // ScratchWork.down_count
    default: return -1;
    }
}

#define HB_REV_IN_S    0.20f  // the revive bar fades in ...
#define HB_REV_OUT_S   0.50f  // ... and out
#define HB_SEAL_IN_S   0.25f  // the seal closes this fast when the Sword lands ...
#define HB_SEAL_OUT_S  0.40f  // ... and opens this fast when it is pulled out
#define HB_FLASH_S     0.60f  // the white flash of a pin landing
#define HB_PIN_S       4.0f   // v3.30 MERGE: Locked Target shows a pinned ghost's sealed bar this long after the pin (the user's choice)
typedef struct {
    float alpha;   // 0..1 the second bar is there
    float frac;    // countdown share left (0..1)
    float max;     // the countdown's full length in this down (learnt; grows when a hit adds to it)
    float seal;    // 0..1 the sealed (pinned) look
    float flash;   // 0..1 the pin's flash
    float sincePin;  // v3.30 MERGE: seconds since the pin landed (large when none was seen landing)
    int state, init;
} HbRevive;
// One frame of the revive bar: state = HbGhostState, countdown = [[work+0x28]+0x98]
static void HbReviveStep(HbRevive *r, int state, float countdown, float dt)
{
    const int down = state == HB_GS_DOWN, pinned = state == HB_GS_PINNED;
    float target;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    if (!(countdown == countdown) || countdown < 0.0f) countdown = 0.0f;
    if (!r->init) r->init = 1, r->state = -1, r->sincePin = 1e6f;
    if (r->sincePin < 1e6f) r->sincePin += dt;  // v3.30 MERGE
    // the flash and the fades first (an event lands at the same moment at every frame rate)
    r->flash = r->flash > dt / HB_FLASH_S ? r->flash - dt / HB_FLASH_S : 0.0f;
    if (down) {
        if (r->state != HB_GS_DOWN || !(r->max > 0.0f)) r->max = countdown;  // a new down: its length is learnt
        if (countdown > r->max) r->max = countdown;
        r->frac = r->max > 0.0f ? HbClamp01(countdown / r->max) : 0.0f;
    }
    if (pinned && r->state != HB_GS_PINNED && r->state != -1) r->flash = 1.0f, r->sincePin = 0.0f;  // v3.30 MERGE: + the pin clock
    if (pinned) {
        r->seal += dt / HB_SEAL_IN_S;
        if (r->seal > 1.0f) r->seal = 1.0f;
    } else {
        r->seal -= dt / HB_SEAL_OUT_S;
        if (r->seal < 0.0f) r->seal = 0.0f;
    }
    if (pinned && r->state == -1) r->seal = 1.0f;  // first seen already pinned: no animation
    target = down || pinned ? 1.0f : 0.0f;
    if (r->alpha < target) {
        r->alpha += dt / HB_REV_IN_S;
        if (r->alpha > target) r->alpha = target;
    } else if (r->alpha > target) {
        r->alpha -= dt / HB_REV_OUT_S;
        if (r->alpha < target) r->alpha = target;
    }
    if (!down && !pinned && r->state == HB_GS_DOWN) r->frac = 0.0f;  // it rose: the countdown is spent
    r->state = state;
}
// v3.30 MERGE: a pinned ghost's bar wants to show for HB_PIN_S after the pin (Locked Target; All Near shows it by distance
// anyway). A ghost first seen already pinned (a room entered with it) does not count: its pin was earlier.
static int HbPinShow(const HbRevive *r) { return r->state == HB_GS_PINNED && r->sincePin < HB_PIN_S; }
// the damage chip's heat: 1 when a hit lands, 0 when the hold ends (the chip then drains in bone colour)
static float HbChipHeat(const HbBar *b) { return HbClamp01(b->hold / HB_TRAIL_HOLD_S); }

// ---------------------------------------------------------------- the curse meter
// Stages: 0 Untouched, 1 Haunted, 2 Cursed, 3 Possessed. th[i] = where stage i + 1 starts (0..1 of the meter); a
// threshold <= 0 or > 1 (a stage the source does not have) is skipped. The stage is the highest threshold passed, as the
// game's tier 0x43E930 (>= 0.7 -> 3, >= 0.5 -> 2, >= 0.25 -> 1) for th = { 0.25, 0.5, 0.7 }.
#define HB_STAGES 4
static int HbCurseStage(float v, const float th[3])
{
    int s, i;
    if (!(v == v)) return 0;
    for (s = 0, i = 0; i < 3; ++i)
        if (th[i] > 0.0f && th[i] <= 1.0f && v >= th[i]) s = i + 1;
    return s;
}

#define HB_WAKE_S   3.5f   // the meter is bright (and its stage named) this long after a change ...
#define HB_REST_A   0.55f  // ... then rests at this strength
#define HB_LABEL_S  3.5f   // the stage name shows this long after the stage changed (or the meter appeared)
#define HB_MFADE_S  0.40f  // the meter itself fades in / out over this long
typedef struct {
    float alpha;   // 0..1 the meter is there (fades when it appears / goes)
    float wake;    // seconds of brightness left
    float label;   // seconds of the stage name left
    float last;    // the value at the last change counted
    int stage, init;
    float pulse;   // v3.30 HB330: 1 -> 0 over HB_PULSE_S after a stage was crossed (the ring swells out)
    int pulseUp;   // the crossing went up (red) or down (cold)
    int woke;      // v3.38 HB338: 1 in the frame it woke (appeared, a stage crossed; v3.38.1 HF3381: not a move): Hide Gauge's own timer
} HbMeter;
#define HB_PULSE_S 0.90f

// present = a source exists (companion Eileen in the party, or Playable Eileen's own meter); v = 0..1
static void HbMeterStep(HbMeter *m, float v, int stage, int present, float dt)
{
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    v = HbClamp01(v);
    m->woke = 0;  // v3.38 HB338
    if (!present) {
        m->alpha -= dt / HB_MFADE_S;
        if (m->alpha < 0.0f) m->alpha = 0.0f;
        if (m->alpha == 0.0f) m->init = 0;  // gone: the next appearance wakes it and names the stage again
        m->wake = m->wake > dt ? m->wake - dt : 0.0f;
        m->label = m->label > dt ? m->label - dt : 0.0f;
        m->pulse = 0.0f;
        return;
    }
    m->pulse = m->pulse > dt / HB_PULSE_S ? m->pulse - dt / HB_PULSE_S : 0.0f;
    if (!m->init) {
        m->init = 1;
        m->stage = stage;
        m->last = v;
        m->wake = HB_WAKE_S;
        m->label = HB_LABEL_S;
        m->woke = 1;  // v3.38 HB338
    } else {
        m->wake = m->wake > dt ? m->wake - dt : 0.0f;
        m->label = m->label > dt ? m->label - dt : 0.0f;
        if (stage != m->stage) {
            m->pulse = 1.0f;
            m->pulseUp = stage > m->stage;
            m->stage = stage;
            m->wake = HB_WAKE_S;
            m->label = HB_LABEL_S;
            m->last = v;
            m->woke = 1;  // v3.38 HB338
        } else if (v > m->last + 0.004f || v < m->last - 0.004f) {  // a visible move (0.4 % of the bar)
            m->wake = HB_WAKE_S;
            m->last = v;  // v3.38.1 HF3381: brightens it, but no Hide Gauge wake (the curse drifts all the time)
        }
    }
    m->alpha += dt / HB_MFADE_S;
    if (m->alpha > 1.0f) m->alpha = 1.0f;
}
// how strong the meter draws: its fade x (rest .. 1 by the wake)
static float HbMeterStrengthRest(const HbMeter *m, float rest)  // v3.38 HB338: the same with its own rest strength
{
    const float w = m->wake >= 1.0f ? 1.0f : m->wake;  // the last second of the wake eases down to rest
    return m->alpha * (rest + (1.0f - rest) * HbClamp01(w));
}
static float HbMeterStrength(const HbMeter *m) { return HbMeterStrengthRest(m, HB_REST_A); }
// the Possessed heartbeat, 0..1: two quick beats every 0.91 s (lub-dub), from the meter's clock in seconds
static float HbBeat(float t)
{
    float p = t * 1.1f, a, b;
    p -= (float)(int)p;
    if (p < 0.0f) p += 1.0f;
    a = (p - 0.08f) / 0.05f, b = (p - 0.30f) / 0.06f;
    a = expf(-a * a), b = 0.7f * expf(-b * b);
    return a > b ? a : b;
}
// the stage name's alpha: in over 0.25 s, out over the last 0.8 s
static float HbLabelAlpha(const HbMeter *m)
{
    const float a = m->label >= 0.8f ? 1.0f : m->label / 0.8f;
    const float b = (HB_LABEL_S - m->label) / 0.25f;
    return m->alpha * HbClamp01(a < b ? a : b);
}

// ---------------------------------------------------------------- v3.32 DHS332: Curse Meter = Both
// The row's values (ini [Gameplay] CurseMeter): Off / On / Companion / Both (added at the end). Both = with Playable
// Eileen loaded the player's own ring (PE's meter) plus a second, smaller ring for companion Eileen (the game's curse
// w+0x0C, tiers 0.25 / 0.5 / 0.7); without Playable Eileen, Both = On (one ring, the companion's curse).
enum { HB_CM_OFF, HB_CM_ON, HB_CM_COMPANION, HB_CM_BOTH, HB_CM_N };
// v3.41 CM341 (staging/v341_feedback/w5_curse/PLAN.md; the contract SH4PlayableEileen/staging/s58_offsound/
// REFINED_HANDOVER.md): Playable Eileen v1.23.1+ exports SH4PE_DrawsPlayerCurse() = 1 and draws the player's curse ring
// itself; Refined then draws companion Eileen's ring only. peRing (HbPeRingState): HB_PR_NONE = no such export (PE up to
// v1.23.0, or no PE): everything as before; HB_PR_HIDDEN = PE owns the player's ring but it cannot show now (PE Off, or
// its [Curse] Meter=0); HB_PR_SHOWN = PE owns it and it can show (or its state is unknown: an SH4PE_Curse below version 2)
enum { HB_PR_NONE, HB_PR_HIDDEN, HB_PR_SHOWN };
// what the meter draws for value cm: returns 1 = the main ring (top right, 452 / 42) is drawn, 0 = no main ring;
// *mainPe = the main ring is Playable Eileen's own meter (else the companion's), *second = the companion's small ring is
// drawn. v3.41 CM341: with PE owning the player's ring (peRing != HB_PR_NONE) *mainPe is never 1: Player draws nothing,
// Both only the small ring, Companion her main ring while PE's ring cannot show, else her small ring (never on top of PE's)
static int HbCurseRings(int cm, int peLoaded, int peRing, int *mainPe, int *second)
{
    *mainPe = *second = 0;
    if (cm <= HB_CM_OFF || cm >= HB_CM_N) return 0;
    if (peRing == HB_PR_HIDDEN || peRing == HB_PR_SHOWN) {  // v3.41 CM341
        if (cm == HB_CM_ON) return 0;
        if (cm == HB_CM_BOTH || peRing == HB_PR_SHOWN) {
            *second = 1;
            return 0;
        }
        return 1;
    }
    *mainPe = (cm == HB_CM_ON || cm == HB_CM_BOTH) && peLoaded;
    *second = cm == HB_CM_BOTH && peLoaded;
    return 1;
}
// v3.41 CM341: which way PE's player ring is: owns = SH4PE_DrawsPlayerCurse() returned 1; haveCurse = SH4PE_Curse was
// found (magic 'SHCM'), version its version; mode / shown its ringMode / ringShown (read only from version 2); peOn =
// SH4PE_Enabled() (1 when that export is missing). A ring drawn last frame counts as shown, unless PE is Off now (it
// never draws while Off; v3.41 REV341 CM341-001: a stale ringShown from before the switch must not win).
static int HbPeRingState(int owns, int haveCurse, unsigned version, long mode, long shown, int peOn)
{
    if (!owns) return HB_PR_NONE;
    if (!peOn) return HB_PR_HIDDEN;  // v3.41 REV341 CM341-001
    if (!haveCurse || version < 2) return HB_PR_SHOWN;  // its settings unknown: as if it shows at its default place
    if (shown == 1) return HB_PR_SHOWN;
    return mode != 0 && peOn ? HB_PR_SHOWN : HB_PR_HIDDEN;
}

// ---------------------------------------------------------------- v3.42 SM342: Curse Meter = Off / On (companion Eileen only)
// USER_FEEDBACK_v341 item 8: "only show Off / On for Curse Meter as it should only affect NPC Eileen; the Playable Eileen
// mod affects Player Eileen's meter". The row's values are Off / On; On = companion Eileen's ring: her main ring place
// while Playable Eileen's own ring cannot show, else the small ring beside PE's (v3.41's Companion / Both with PE owning
// the player's ring). Refined never draws the player's ring again, whatever Playable Eileen version (or none).
// The ini ([Gameplay] CurseMeter) keeps its key. Its old values read: Off -> Off; On (v3.32..v3.41: the player's ring,
// shown as "Player" since v3.37) and Player -> Off; Companion and Both -> On. The new On is written as "Companion", the
// name every older build reads as companion Eileen's ring (v3.41 with PE v1.23.1: the same rings), and Off as "Off".
// Anything else reads as Off. Greyed values are gone. HbCurseRings is asked with the v3.41 mode the row value stands for.
enum { HB_CMV_OFF, HB_CMV_ON, HB_CMV_N };
static const char *const kHbCmvIni[HB_CMV_N] = { "Off", "Companion" };  // what the ini is written with
static const char *const kHbCmvShown[HB_CMV_N] = { "Off", "On" };       // what the menu and the log show
static int HbCmvLower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int HbCmvEq(const char *a, const char *b)
{
    while (*a && *b && HbCmvLower(*a) == HbCmvLower(*b)) ++a, ++b;
    return !*a && !*b;
}
// the row value of an ini text; *legacy = 1 when it is a v3.41-or-older value read another way (On / Player / Both)
static int HbCmvParse(const char *s, int *legacy)
{
    int v = HB_CMV_OFF, old = 0;
    if (s && (HbCmvEq(s, "Companion") || HbCmvEq(s, "Both"))) v = HB_CMV_ON, old = HbCmvEq(s, "Both");
    else if (s && (HbCmvEq(s, "On") || HbCmvEq(s, "Player"))) old = 1;
    if (legacy) *legacy = old;
    return v;
}
static int HbCmvMode(int v) { return v == HB_CMV_ON ? HB_CM_COMPANION : HB_CM_OFF; }  // the v3.41 mode for HbCurseRings

// ---------------------------------------------------------------- v3.37 MN337: the Curse Meter row and PE's stage marks
// The value HB_CM_ON is shown as "Player" (the ini keeps writing "On": older builds read it; both read). Without Playable
// Eileen loaded Player and Both are drawn grey and the row's Left / Right step over them: from Off or Companion, and from
// a greyed value saved earlier, the next press lands on Off or Companion. A saved Player / Both acts as Companion while PE
// is absent (HbCurseRings with peLoaded 0) and as itself when PE is back; the ini value is not rewritten by that.
// v3.41 CM341: + peOwns (SH4PE_DrawsPlayerCurse() = 1): Player is grey too (PE draws the player's ring; its own pause menu
// sets it), Both is not (it means companion Eileen's ring only). The ini value stays as saved either way.
static int HbCmGreyed(int v, int peLoaded, int peOwns)
{
    return (!peLoaded && (v == HB_CM_ON || v == HB_CM_BOTH)) || (peLoaded && peOwns && v == HB_CM_ON);
}
// the next value from cur, one press left (left != 0) or right, skipping the greyed ones (Off and Companion never are)
static int HbCmStepFrom(int cur, int left, int peLoaded, int peOwns)
{
    int v = cur < 0 || cur >= HB_CM_N ? 0 : cur, n;
    for (n = 0; n < HB_CM_N; ++n) {
        v = left ? (v + HB_CM_N - 1) % HB_CM_N : (v + 1) % HB_CM_N;
        if (!HbCmGreyed(v, peLoaded, peOwns)) return v;
    }
    return HB_CM_OFF;
}
// v3.41 CM341: which description the row shows for value v: HB_CD_OWN (its own text), HB_CD_NOPE (Player / Both without
// PE: "this shows Companion"), HB_CD_PE_PLAYER (Player while PE owns the ring), HB_CD_PE_BOTH (Both while PE owns it)
enum { HB_CD_OWN, HB_CD_NOPE, HB_CD_PE_PLAYER, HB_CD_PE_BOTH };
static int HbCmDescKind(int v, int peLoaded, int peOwns)
{
    if (!peLoaded) return v == HB_CM_ON || v == HB_CM_BOTH ? HB_CD_NOPE : HB_CD_OWN;
    if (peOwns && v == HB_CM_ON) return HB_CD_PE_PLAYER;
    if (peOwns && v == HB_CM_BOTH) return HB_CD_PE_BOTH;
    return HB_CD_OWN;
}
// Playable Eileen's stage marks (0..1 of its 0..100 meter) from its [Curse] FitsFrom (Haunted) and CursedIdleFrom
// (Cursed), whether its Fits / CursedIdle switches are on or not (v3.36 skipped Haunted with Fits=0: "it goes straight
// to Cursed half way through"); Possessed = full (100, latched). Values outside 0..100 are clamped.
static void HbPeStageMarks(int fitsFrom, int idleFrom, float th[3])
{
    fitsFrom = fitsFrom < 0 ? 0 : fitsFrom > 100 ? 100 : fitsFrom;
    idleFrom = idleFrom < 0 ? 0 : idleFrom > 100 ? 100 : idleFrom;
    th[0] = (float)fitsFrom / 100.0f;
    th[1] = (float)idleFrom / 100.0f;
    th[2] = 1.0f;
}
// the ring's geometry (canvas 512 x 448; radii in canvas heights, so circles at any squeeze): the main ring at (452, 42)
// R 17 (hudbars.inc HB_RING_*), the companion's to its left, top-aligned with it, 0.62 of its pixel-sized parts
#define HB_RING2_CX 414.0f
#define HB_RING2_CY 36.0f
#define HB_RING2_R  10.5f
#define HB_RING2_K  0.62f
#define HB_TAG_H    4.6f      // the label's letter height (canvas heights; 5 grid units)
#define HB_TAG_GAP  3.4f      // from the small ring's outer edge down to the label's top
// a thorn's length (pixels) at a stage mark: k = the ring's size scale (1 = the main ring), px = HbPx
static float HbThornLen(float R, float px, float k) { return k * 4.2f * px + 0.10f * R; }
// how far a ring reaches out from its centre with its thorns (pixels): HbRingDraw's thorn base + length
static float HbRingReach(float R, float px, float k) { return R + 0.5f * px + HbThornLen(R, px, k); }
// v3.41 CM341: the small ring's centre (canvas) while Playable Eileen draws the player's ring. follow = its SH4PE_Curse is
// version 2+ (px, py = its ring's centre, pr = its outer radius, canvas). PE's default ring (452, 42, R 17) gives the usual
// place (414, 36); a larger ring pushes it out along the same line (the contract: x = px - 38 pr / 17, y = py - 6 pr / 17);
// a smaller one keeps the default gap between the two rings (38.47 - 17 - 10.5 = 10.97 canvas heights), so they never
// touch. Where it would leave the canvas on the left it goes right of PE's ring; never above the canvas top. Returns 0 =
// the default place (not followed, or PE's values out of range / NaN), 1 = left of PE's ring, 2 = right of it.
#define HB_RING2_DIST 38.470768f  // |(452 - 414, 42 - 36)|: the default centre-to-centre distance
#define HB_RING2_EDGE 4.0f        // room kept for the small ring's thorns at the canvas edge
static int HbRing2Place(int follow, float px, float py, float pr, float *cx, float *cy)
{
    float d, dx, dy;
    *cx = HB_RING2_CX, *cy = HB_RING2_CY;
    if (!follow || !(px >= 0.0f && px <= 512.0f) || !(py >= 0.0f && py <= 448.0f) || !(pr >= 1.0f && pr <= 120.0f)) return 0;
    d = HB_RING2_DIST * pr / 17.0f;
    if (d < pr + HB_RING2_DIST - 17.0f) d = pr + HB_RING2_DIST - 17.0f;  // (pr below 17: the default gap)
    dx = d * (38.0f / HB_RING2_DIST), dy = d * (6.0f / HB_RING2_DIST);
    *cy = py - dy < HB_RING2_R ? HB_RING2_R : py - dy;
    if (px - dx - HB_RING2_R - HB_RING2_EDGE >= 0.0f) {
        *cx = px - dx;
        return 1;
    }
    *cx = px + dx;
    return 2;
}

// The small ring's label: a stroke font of the letters it needs (E I L N), boxes on a grid 5 units tall (y down); a
// box's bottom edge is shifted right by `slant` units (N's diagonal). One unit between letters. No game font: the
// game's text is as tall as the small ring.
// v3.32 MERGE: + C D K O for the LOCKED tag (a negative slant leans the box's bottom left: K's upper arm).
typedef struct { float x0, y0, x1, y1, slant; } HbStroke;
#define HB_GLYPH_MAXS 5  // v3.35 CM335: was 4 (R and S need 5)
typedef struct { char c; int adv, n; HbStroke s[HB_GLYPH_MAXS]; } HbGlyphDef;
static const HbGlyphDef kHbGlyph[] = {
    { 'E', 3, 4, { { 0, 0, 1, 5, 0 }, { 1, 0, 3, 1, 0 }, { 1, 2, 2.6f, 3, 0 }, { 1, 4, 3, 5, 0 } } },
    { 'I', 1, 1, { { 0, 0, 1, 5, 0 } } },
    { 'L', 3, 2, { { 0, 0, 1, 5, 0 }, { 1, 4, 3, 5, 0 } } },
    { 'N', 4, 3, { { 0, 0, 1, 5, 0 }, { 3, 0, 4, 5, 0 }, { 0.5f, 0, 1.5f, 5, 2 } } },
    { 'O', 4, 4, { { 0, 1, 1, 4, 0 }, { 3, 1, 4, 4, 0 }, { 1, 0, 3, 1, 0 }, { 1, 4, 3, 5, 0 } } },          // v3.32 MERGE
    { 'C', 3, 3, { { 0, 1, 1, 4, 0 }, { 1, 0, 3, 1, 0 }, { 1, 4, 3, 5, 0 } } },                              // v3.32 MERGE
    { 'K', 3, 3, { { 0, 0, 1, 5, 0 }, { 2, 0, 3, 2.5f, -1 }, { 1, 2.5f, 2, 5, 1 } } },                       // v3.32 MERGE
    { 'D', 4, 4, { { 0, 0, 1, 5, 0 }, { 1, 0, 3, 1, 0 }, { 1, 4, 3, 5, 0 }, { 3, 1, 4, 4, 0 } } },          // v3.32 MERGE
    // v3.35 CM335: the main ring's stage names in the same font (UNTOUCHED HAUNTED CURSED POSSESSED)
    { 'U', 4, 3, { { 0, 0, 1, 4, 0 }, { 3, 0, 4, 4, 0 }, { 1, 4, 3, 5, 0 } } },
    { 'T', 3, 2, { { 0, 0, 3, 1, 0 }, { 1, 1, 2, 5, 0 } } },
    { 'H', 4, 3, { { 0, 0, 1, 5, 0 }, { 3, 0, 4, 5, 0 }, { 1, 2, 3, 3, 0 } } },
    { 'A', 4, 4, { { 0, 1, 1, 5, 0 }, { 3, 1, 4, 5, 0 }, { 1, 0, 3, 1, 0 }, { 1, 2.5f, 3, 3.5f, 0 } } },
    { 'P', 4, 4, { { 0, 0, 1, 5, 0 }, { 1, 0, 3, 1, 0 }, { 3, 1, 4, 2, 0 }, { 1, 2, 3, 3, 0 } } },
    { 'R', 4, 5, { { 0, 0, 1, 5, 0 }, { 1, 0, 3, 1, 0 }, { 3, 1, 4, 2, 0 }, { 1, 2, 3, 3, 0 }, { 2, 3, 3, 5, 1 } } },
    { 'S', 3, 5, { { 0, 0, 3, 1, 0 }, { 0, 1, 1, 2, 0 }, { 0, 2, 3, 3, 0 }, { 2, 3, 3, 4, 0 }, { 0, 4, 3, 5, 0 } } },
    // v3.38 HB338: PLAYER under the player's ring (the font had no Y: "PLAER"): two arms leaning in, a stem
    { 'Y', 4, 3, { { 0, 0, 1, 2.5f, 1 }, { 3, 0, 4, 2.5f, -1 }, { 1.5f, 2.5f, 2.5f, 5, 0 } } },
};
static const HbGlyphDef *HbGlyphOf(char c)
{
    int i;
    for (i = 0; i < (int)(sizeof kHbGlyph / sizeof kHbGlyph[0]); ++i)
        if (kHbGlyph[i].c == c) return &kHbGlyph[i];
    return 0;  // (not drawn, no advance)
}
static float HbTinyWidth(const char *t)  // grid units
{
    int w = 0, n = 0;
    for (; *t; ++t) {
        const HbGlyphDef *g = HbGlyphOf(*t);
        if (g) w += g->adv, ++n;
    }
    return (float)(w + (n > 1 ? n - 1 : 0));
}

// ---------------------------------------------------------------- v3.32 MERGE: the small ring while her curse is locked
// The user (2026-11-19, after W3): with Playable Eileen's [Ghost] LockCurse=1 (its default) PE holds the companion's game
// curse at 0 (it writes her target w+0x10 = 0 every frame while it ghosts her), so the small ring of Both would always
// read Untouched. Locked = the small ring is drawn (Both, PE loaded, she is present) AND PE's ini says LockCurse=1 AND
// her curse reads exactly 0 (so a curse still easing down after the lock was set, or one PE does not hold, reads normal).
// Then the ring turns grey and dim and a small LOCKED tag shows under its EILEEN label; the change eases over
// HB_LOCK_S both ways (frame-rate independent), so the ring never flickers.
#define HB_LOCK_S    0.30f    // the grey eases in / out over this long
#define HB_LOCK_DIM  0.30f    // locked: the ring's strength x (1 - this) ...
#define HB_LOCK_DARK 0.10f    // ... and its grey this much darker than the colours' own brightness
#define HB_LOCK_H    3.6f     // the LOCKED tag's letter height (canvas heights; smaller than the EILEEN label's 4.6)
#define HB_LOCK_GAP  1.6f     // from the EILEEN label's bottom down to the tag's top
static int HbRing2Locked(int second, int present2, int peLockCurse, float v2)
{
    return second && present2 && peLockCurse && v2 == 0.0f;  // (NaN: not locked; hudbars.inc clamps v2 first)
}
static float HbLockStep(float k, int locked, float dt)  // k 0 = normal .. 1 = locked
{
    if (!(dt > 0.0f)) dt = 0.0f;  // (NaN / negative: no step)
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    if (!(k >= 0.0f && k <= 1.0f)) k = 0.0f;
    k += (locked ? 1.0f : -1.0f) * dt / HB_LOCK_S;
    return HbClamp01(k);
}
// a colour (0xRRGGBB) k of the way to its own grey (Rec. 601 luma), that grey HB_LOCK_DARK darker at k = 1
static unsigned int HbLockRgb(unsigned int rgb, float k)
{
    const float r = (float)(rgb >> 16 & 0xFF), g = (float)(rgb >> 8 & 0xFF), b = (float)(rgb & 0xFF);
    const float y = (0.299f * r + 0.587f * g + 0.114f * b) * (1.0f - HB_LOCK_DARK * HbClamp01(k));
    const float t = HbClamp01(k);
    const float c[3] = { r + (y - r) * t, g + (y - g) * t, b + (y - b) * t };
    unsigned int o = 0;
    int i;
    for (i = 0; i < 3; ++i) {
        const float x = c[i] < 0.0f ? 0.0f : c[i] > 255.0f ? 255.0f : c[i];
        o = o << 8 | (unsigned int)(x + 0.5f);
    }
    return o;
}
static float HbLockAlpha(float k) { return 1.0f - HB_LOCK_DIM * HbClamp01(k); }  // the small ring's strength factor

// ---------------------------------------------------------------- v3.35 CM335: the main ring's stage name
// The user (2026-11-20): the small ring's EILEEN / LOCKED tags are compact and close to their ring; the main ring's stage
// name (the game's font 1, about 17 canvas units tall, 4 below the ring) should look the same. It is now drawn in the same
// stroke font (kHbGlyph), upper case, its top HB_LABEL_GAP below the ring's outer edge (EILEEN's gap), letters
// HB_LABEL_H tall (EILEEN 4.6 under a ring of 10.5; this ring is 17), with the same one-pixel shadow.
#define HB_LABEL_H   5.4f    // letter height (canvas heights; 5 grid units)
#define HB_LABEL_GAP 3.4f    // from the main ring's outer edge (42 + 17) down to the name's top (HB_TAG_GAP)
#define HB_LABEL_TOP (42.0f + 17.0f + HB_LABEL_GAP)  // (hudbars.inc HB_RING_CY + HB_RING_R)

// ---------------------------------------------------------------- v3.35 CM335: where companion Eileen's curse is heading
// The user's choice (2026-11-20): a faint notch on her ring where her curse is heading, and a small "left alone" glyph
// while she is left in a room with monsters. The game (curse_mechanics.md; SH4Decomp src/eileen/eileen_43e790.c and
// eileen_445960.c): her live curse w+0x08 creeps up to the target w+0x10 (0.00015 a tick, 0.0045 a second) and decays
// down to it; the shown curse w+0x0C follows live. The target is EileenCurseComputeTarget 0x43E790, called only at her
// spawn (0x43EC20, at every room load), from her record 0xFCE7F0. Every room exit while she is in the party (0x449EC0,
// [0xFD5BC4] != 0) adds 1 to rooms +0x30 and the danger +0x2D to the danger total +0x2E; with her task alive it also adds
// 1 to spawned +0x32 and sets +0x2D again (0x43F8E0 over the monsters of the room the player leaves: 0 / 1 / 2 when she
// stays there, party mode 2; else 0). Without her task (left behind) it returns after the first two, so every room the
// player walks through raises her next target. HbCurseTarget is 0x43E790 in C, read only, in the game's order of
// operations (FloatClamp lets NaN through: rooms >= 10 with frames 0 and near frames 0 is NaN in the game too).
typedef struct {
    float damage;      // +0x24 damage taken
    int danger;        // +0x2D (byte) the danger of the room she was left in: 0 / 1 / 2
    int dangerTotal;   // +0x2E (word)
    int rooms;         // +0x30 (word) room exits while she is in the party
    int spawned;       // +0x32 (word) room exits while her task existed
    int collapses;     // +0x34 (word)
    int flinches;      // +0x36 (word)
    int frames;        // +0x38 frames her task ran
    int nearFrames;    // +0x3C frames she was within 800 of the player
} HbCurseRec;
static float HbNaN(void)
{
    union { unsigned int u; float f; } q;
    q.u = 0x7FC00000u;
    return q.f;
}
static float HbFClampG(float x, float lo, float hi)  // the game's FloatClamp (a NaN passes)
{
    if (x < lo) x = lo;
    else if (x > hi) x = hi;
    return x;
}
static float HbCurseTarget(const HbCurseRec *r)
{
    const int n = r->rooms, c = r->collapses < 0 ? 0 : r->collapses > 10 ? 10 : r->collapses;
    float v, ratio;
    if (n < 10) return 0.0f;
    v = ((float)(n - r->spawned) + (float)r->dangerTotal) / (float)n;
    if (r->frames) ratio = (float)r->nearFrames / (float)r->frames;
    else if (r->nearFrames) ratio = r->nearFrames > 0 ? 1e30f : -1e30f;  // (x / 0 = +-inf: clamped as the game's)
    else return HbNaN();                                              // 0 / 0: the game's target is NaN
    v -= HbFClampG(ratio - 0.5f, 0.0f, 1.0f) * 0.2f;
    v += (float)c * 0.05f - 0.05f;
    v += HbFClampG(r->damage * 0.0003f, 0.0f, 0.2f);
    v += HbFClampG((float)r->flinches * 0.02f, 0.0f, 0.5f);
    v = HbFClampG(v - 0.2f, 0.0f, 0.8f);
    return HbFClampG(v * 1.25f, 0.0f, 0.95f);
}
// the record after one more room exit while she is left behind without a task (0x449EC0 up to its early return): the
// pending danger joins the total, one more room; spawned and the danger itself stay (the words wrap as the game's)
static HbCurseRec HbCurseAfterExit(HbCurseRec r)
{
    r.rooms = (r.rooms + 1) & 0xFFFF;
    if (r.danger) r.dangerTotal = (r.dangerTotal + r.danger) & 0xFFFF;
    return r;
}
// the left-alone glyph: she stayed behind at the last door (party mode 2), she is not with the player (no task here, or
// her task in a neighbour room) and the room she stayed in had monsters (+0x2D > 0). Returns the danger shown, 0..2.
static int HbLeftAlone(int partyMode, int away, int danger)
{
    if (partyMode != 2 || !away || danger <= 0) return 0;
    return danger >= 2 ? 2 : 1;
}
// the next-spawn mark: left behind (party mode 2) with no task of hers (the next door adds a room and the danger only)
static int HbShowNext(int partyMode, int task) { return partyMode == 2 && !task; }

#define HB_NOTCH_EPS   0.006f  // the notch hides when the target is this close to the shown curse ...
#define HB_NOTCH_RAMP  0.010f  // ... and is at full strength this much further away (no flicker at the edge)
#define HB_NOTCH_IN_S  0.40f   // it fades in ...
#define HB_NOTCH_OUT_S 0.60f   // ... and out
#define HB_NOTCH_SPEED 0.60f   // a new target slides there at this share of the ring a second (a new room's target)
#define HB_ALONE_IN_S  0.50f   // the left-alone glyph fades in ...
#define HB_ALONE_OUT_S 0.80f   // ... and out
typedef struct {
    float a, pos;       // the notch: strength 0..1 and where it is drawn (0..1 of the ring)
    float nextA, next;  // the next-spawn mark (left behind)
    float alone;        // the left-alone glyph 0..1
    int danger;         // the danger the glyph shows (kept while it fades out)
    // v3.38 HB338: what woke the ring last frame (Hide Gauge's own timer): HB_NW_* bits; the places last counted
    int woke, wShown, wNextShown, wAlone;
    float wPos, wNext;
} HbNotch;
enum { HB_NW_NOTCH = 1, HB_NW_NEXT = 2, HB_NW_ALONE = 4 };  // v3.38 HB338
#define HB_NW_MOVE 0.004f  // v3.38 HB338: a visible move of a mark (0.4 % of the ring, as the fill's)
// how strongly a mark at `target` shows against `ref` (0..1): 0 for a target out of 0..1 or NaN, or within HB_NOTCH_EPS
static float HbNotchWant(float target, float ref)
{
    float d;
    if (!(target >= 0.0f && target <= 1.0f) || !(ref == ref)) return 0.0f;
    d = fabsf(target - ref);
    return HbClamp01((d - HB_NOTCH_EPS) / HB_NOTCH_RAMP);
}
static float HbFadeTo(float a, float want, float inS, float outS, float dt)
{
    if (!(a >= 0.0f && a <= 1.0f)) a = 0.0f;
    if (a < want) {
        a += dt / inS;
        if (a > want) a = want;
    } else if (a > want) {
        a -= dt / outS;
        if (a < want) a = want;
    }
    return a;
}
static float HbSlideTo(float p, float to, float dt)
{
    const float s = HB_NOTCH_SPEED * dt;
    if (!(p == p)) return to;
    return p < to - s ? p + s : p > to + s ? p - s : to;
}
// One frame. target = where her curse is heading (-1 / NaN = unknown: the notch fades), shown = the curse the ring shows;
// next / haveNext = the target at her next spawn (left behind); alone = HbLeftAlone. A hidden mark jumps to its value; a
// shown one slides there.
static void HbNotchStep(HbNotch *n, float target, float shown, float next, int haveNext, int alone, float dt)
{
    const int okT = target >= 0.0f && target <= 1.0f, okN = haveNext && next >= 0.0f && next <= 1.0f;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    if (okT) n->pos = n->a > 0.0f ? HbSlideTo(n->pos, target, dt) : target;
    n->a = HbFadeTo(n->a, okT ? HbNotchWant(target, shown) : 0.0f, HB_NOTCH_IN_S, HB_NOTCH_OUT_S, dt);
    if (okN) n->next = n->nextA > 0.0f ? HbSlideTo(n->next, next, dt) : next;
    n->nextA = HbFadeTo(n->nextA, okN ? HbNotchWant(next, okT ? target : shown) : 0.0f, HB_NOTCH_IN_S, HB_NOTCH_OUT_S, dt);
    if (alone > 0) n->danger = alone;
    n->alone = HbFadeTo(n->alone, alone > 0 ? 1.0f : 0.0f, HB_ALONE_IN_S, HB_ALONE_OUT_S, dt);
    {   // v3.38 HB338: the wakes of Hide Gauge's own timer: a mark appearing, the glyph appearing or its danger changing (a
        // mark or the glyph going away wakes nothing; v3.38.1 HF3381: a mark sliding wakes nothing either)
        const int shownN = n->a > 0.0f, shownX = n->nextA > 0.0f, al = alone > 0 ? (alone >= 2 ? 2 : 1) : 0;
        n->woke = 0;
        if (shownN && !n->wShown) n->woke |= HB_NW_NOTCH;  // v3.38.1 HF3381: appearing only
        if (shownX && !n->wNextShown) n->woke |= HB_NW_NEXT;
        if (al && al != n->wAlone) n->woke |= HB_NW_ALONE;
        n->wShown = shownN, n->wNextShown = shownX, n->wAlone = al;
    }
}
// the glyph's place: left of its ring (9 o'clock, clear of the thorns at 0.25 / 0.5 / 0.7 and of the labels under the
// ring), its half height HbAloneSize(R) (canvas units in, canvas units out), its centre HbAloneGap(R) out from the
// ring's outer edge
static float HbAloneSize(float R) { return 2.4f + 0.13f * R; }
static float HbAloneGap(float R) { return 1.6f + HbAloneSize(R); }

// ---------------------------------------------------------------- v3.38 HB338: readable on black, PLAYER, Hide Gauge
// staging/v338_feedback/w1_hud/PLAN.md (USER_FEEDBACK_v337 items 1-3; RESEARCH_hud.md sections 1-3).
// 1. Strengths. Companion Eileen away (saved value, or her task in a neighbour room) dims her ring to HB_AWAY_A (was 0.5);
//    both rings rest at HB_REST2_A (the user's choice, 2026-11-21; v3.37 HB_REST_A 0.55, kept for HbMeterStrength). Her warnings (the target notch, the next-spawn
//    mark: the user's "limiter bars", and the left-alone glyph) are not dimmed by away and never drop under HB_ALERT_FLOOR
//    of the ring's fade-in (HbAlertQ); Hide Gauge's k still multiplies them (the glyph wakes the ring, it does not hold it).
#define HB_AWAY_A      0.70f   // her ring while she is away (v3.37: 0.5)
#define HB_REST2_A     0.70f   // both rings and their PLAYER / EILEEN names at rest (v3.37: HB_REST_A 0.55)
#define HB_ALERT_FLOOR 0.85f   // the notch / next mark / glyph strength floor (x the ring's own fade-in alpha)
static float HbAlertQ(float aNoAway, float meterAlpha)
{
    const float f = HB_ALERT_FLOOR * HbClamp01(meterAlpha), a = HbClamp01(aNoAway);
    return a > f ? a : f;
}
// 2. Every ring named: PLAYER under the player's ring (Player, or Both with Playable Eileen), EILEEN under the main ring
//    when it shows companion Eileen (the user's choice, 2026-11-21): the small ring's letter height HB_TAG_H at the stage
//    name's old top; the stage name moves under it by LOCKED's gap (HB_LOCK_GAP), still HB_LABEL_H tall (HbStageTop(1)).
//    HbStageTop(0) is the v3.37 place (no name).
#define HB_PLAYER_TOP HB_LABEL_TOP
static float HbStageTop(int player) { return player ? HB_PLAYER_TOP + HB_TAG_H + HB_LOCK_GAP : HB_LABEL_TOP; }
// 3. Hide Gauge (Option 3, byte [0x1091AD9]; not Hide Icon [0x1091ADA]). Off: k = 1 (always shown, as v3.37). On: k =
//    max(the game's gauge fade [0x653B8C] / 0.7, the ring's own timer): 1 for HB_HIDE_HOLD_S after its last wake, then
//    down to 0 over HB_HIDE_FADE_S (the gauge's 5 s + 1 s, 0x50E090 / 0x50E910). Wakes: the meter appeared, a stage was
//    crossed (HbMeter.woke; v3.38.1 HF3381: not a move of the fill), a notch / next mark appearing (not sliding), the glyph appearing or
//    its danger changing (HbNotch.woke), the LOCKED state changing. Held up (idle 0) while its owner is Possessed (the
//    gauge's low-HP rule, 0x50E0F0). HbHideSleep: the HUD was hidden (HudDisable 0x4F5090 -> 0x50ED90: pause, events,
//    pickups; with Hide Gauge on the gauge comes back faded out), so does the ring. Real seconds, frame-rate independent.
#define HB_HIDE_HOLD_S 5.0f
#define HB_HIDE_FADE_S 1.0f
#define HB_GAUGE_FULL  0.70f   // [0x653B8C] when the gauge is fully shown (0x50E910 clamps to 0.7)
typedef struct { float idle; int asleep; } HbHide;  // seconds since the ring's last own wake (asleep: put to sleep)
static float HbGameK(float fade) { return fade == fade ? HbClamp01(fade / HB_GAUGE_FULL) : 0.0f; }
static float HbOwnK(float idle)
{
    if (!(idle == idle)) return 0.0f;
    return idle <= HB_HIDE_HOLD_S ? 1.0f : HbClamp01(1.0f - (idle - HB_HIDE_HOLD_S) / HB_HIDE_FADE_S);
}
static void HbHideSleep(HbHide *h) { h->idle = HB_HIDE_HOLD_S + HB_HIDE_FADE_S, h->asleep = 1; }
// One frame: hideOn = [0x1091AD9] != 0; wake = an own wake this frame; hold = its owner is Possessed; fade = [0x653B8C].
// Returns k (0..1), every element's alpha factor.
static float HbHideStep(HbHide *h, int hideOn, int wake, int hold, float fade, float dt)
{
    float g, o;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > HB_DT_MAX) dt = HB_DT_MAX;
    if (!hideOn) {  // always shown; turned on later, it starts from a fresh wake
        h->idle = 0.0f, h->asleep = 0;
        return 1.0f;
    }
    if (!(h->idle >= 0.0f)) h->idle = 0.0f;  // (NaN)
    if (wake || hold) {
        h->idle = 0.0f, h->asleep = 0;
    } else {
        h->idle += dt;
        if (h->idle > 1e6f) h->idle = 1e6f;
    }
    g = HbGameK(fade), o = HbOwnK(h->idle);
    if (g > o) o = g;
    if (o > 0.0f) h->asleep = 0;  // (shown again: a later fade-out is the timer's, not the sleep's)
    return o;
}
#endif
