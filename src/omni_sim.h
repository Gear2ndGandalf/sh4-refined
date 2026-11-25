// v3.46 OM346: the pure logic of Stick Movement (omni.inc): 360-degree / analog left-stick movement. No game memory,
// no Windows calls: test\omni_sim_test.c (test\build_omni_sim_test.cmd).
//
// The PC port reads the stick 8-way: InputDI::AxisEvent 0x406F50 turns each raw axis (+-1000) into a press beyond +-300,
// the press sets a direction bit ([InputDI+0x38], bits 0 x+, 1 x-, 2 y- (up), 3 y+), the bits become +-0x7FFF shorts and
// InputNormalizeSticks 0x553D20 makes the floats gPad.stick[0].x / y (0x1236C44 / 0x1236C48) from them. omni.inc calls
// 0x553D20 first, then (mode on, gameplay, a stick bound to movement, the raw stick outside the dead zone) writes the
// real stick vector into x / y. Signs: raw lY < 0 (up) gives stick.y = -1 in stock, so stick = (+lX, +lY) / 1000.
//
// The raw axes come from one of two sources, picked by a self-check against the game's own direction bits:
//   0 = GetDeviceState: a second IDirectInputDevice8::GetDeviceState on the game's joystick ([[0x6E8EA4]+0xC]);
//   1 = call sites: the lX / lY values the game passes to 0x406F50 at 0x407B15 / 0x407B28 (captured, then passed on).
// v3.47 CT347: + the Option 2 table check (the row moved there), SH4R_StickAmount's value, the focus-loss episodes.
// v3.48 OM348 (USER_FEEDBACK_v347.md A; the user: "Rename Stick Movement to Omni-Directional Movement. Parameters should be
// Off/On. On = Analog ... So remove Direction please."): two modes, Off / On; On = v3.46 / v3.47's Analog in every respect
// (the map, the dead zone, the 0.6 floor, SH4R_StickAmount). Direction is gone; an old Direction / Analog / 2 reads as On.
#ifndef OMNI_SIM_H
#define OMNI_SIM_H

#include <math.h>
#include <stdlib.h>  // v3.46 OM346 inspection 2: strtod (OmDzNumber)

enum { OM_OFF, OM_ON, OM_MODES };  // [Input] OmniMove (also 0 / 1)  // v3.48 OM348: was Off / Direction / Analog (0 / 1 / 2)
static const char *const kOmModeName[OM_MODES] = { "Off", "On" };  // v3.48 OM348

#define OM_DZ_DEF 0.30f   // [Input] OmniDeadZone: radial, of the full push (0.30 = the game's own +-300 of +-1000)
#define OM_DZ_MIN 0.05f
#define OM_DZ_MAX 0.90f
#define OM_ANALOG_FLOOR 0.6f  // Analog's smallest push: the walk clip's own floor (clamp(amount, 0.6, 1)): no foot slide
#define OM_RAW_MAX 1100   // a raw axis beyond this is not the game's +-1000 range: not used (a wrapper's other range)

// v3.48 OM348: [Input] OmniMove's text. "Off" / "On" or 0 / 1 (any case, spaces / tabs around it) = this version's value;
// v3.46 / v3.47's "Direction" / "Analog" / 2 = On with *legacy = 1 (the caller writes it back once as On); "1" is On in
// both versions (v3.47's Direction): no write-back. Anything else = Off with *known = 0. (v3.46 OM346: Off / Direction /
// Analog, 0 / 1 / 2.)
static int OmWordIs(const char *s, int n, const char *w)  // s[0..n) == w, any case
{
    int i;
    for (i = 0; i < n && w[i]; ++i) {
        const char ca = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i], cb = w[i] >= 'A' && w[i] <= 'Z' ? (char)(w[i] + 32) : w[i];
        if (ca != cb) return 0;
    }
    return i == n && !w[i];
}
static int OmParseMode(const char *s, int *known, int *legacy)
{
    int n;
    if (known) *known = 1;
    if (legacy) *legacy = 0;
    if (!s) s = "";
    while (*s == ' ' || *s == '\t') ++s;
    for (n = 0; s[n]; ++n) {}
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
    if (n == 1 && s[0] == '0') return OM_OFF;
    if (n == 1 && s[0] == '1') return OM_ON;  // (v3.47's Direction too: the same value either way)
    if (OmWordIs(s, n, "off")) return OM_OFF;
    if (OmWordIs(s, n, "on")) return OM_ON;
    if ((n == 1 && s[0] == '2') || OmWordIs(s, n, "analog") || OmWordIs(s, n, "direction")) {
        if (legacy) *legacy = 1;
        return OM_ON;
    }
    if (known) *known = 0;
    return OM_OFF;
}

// The dead zone: clamped to OM_DZ_MIN..OM_DZ_MAX; not a number (or none) = the default.
static float OmClampDz(float dz)
{
    if (!(dz == dz)) return OM_DZ_DEF;  // NaN
    if (dz < OM_DZ_MIN) return OM_DZ_MIN;
    if (dz > OM_DZ_MAX) return OM_DZ_MAX;
    return dz;
}

// v3.46 OM346 (inspection 2, OM346-002): the OmniDeadZone text as a number. 1 = a whole number (optional spaces around
// it), *out set; 0 = not a number ("0.4junk", "abc", "."): the caller uses the default. Empty = 0 too (the caller treats
// empty as "the default, nothing to log").
static int OmDzNumber(const char *s, double *out)
{
    char *end;
    double d;
    if (!s) return 0;
    d = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end) return 0;
    if (out) *out = d;
    return 1;
}

// Analog's length for a push m (0..1, already clamped) outside the dead zone dz: 0.6 at the edge, 1 at a full push,
// linear between (monotone, continuous at m = dz from outside).
static float OmAnalogLen(float m, float dz)
{
    float t;
    dz = OmClampDz(dz);
    t = (m - dz) / (1.0f - dz);
    if (!(t > 0.0f)) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return OM_ANALOG_FLOOR + (1.0f - OM_ANALOG_FLOOR) * t;
}

// The raw axes are in the game's range (+-OM_RAW_MAX).
static int OmRawOk(long rx, long ry)
{
    return rx >= -OM_RAW_MAX && rx <= OM_RAW_MAX && ry >= -OM_RAW_MAX && ry <= OM_RAW_MAX;
}

// The raw stick is outside the dead zone (radial; raw axes out of range never count).
static int OmPushed(long rx, long ry, float dz)
{
    const float x = (float)rx / 1000.0f, y = (float)ry / 1000.0f;
    return OmRawOk(rx, ry) && sqrtf(x * x + y * y) > OmClampDz(dz);
}

// The vector to write for raw axes (rx, ry) in +-1000. Returns 1 and the floats when the push is outside the dead zone
// (|v| clamped to 1, so the stock normalize quirk never runs: the same speed every way); 0 inside it (the game's floats
// stay) or when the mode is Off or the raw axes are out of range.
// v3.48 OM348: On = v3.47's Analog (the length OmAnalogLen: 0.6 at the dead zone's edge, 1 at a full push); Direction's
// fixed length 1 is gone.
static int OmMap(long rx, long ry, int mode, float dz, float *ox, float *oy)
{
    float x, y, m, mm, out;
    if (mode != OM_ON) return 0;  // v3.48 OM348 (was Direction / Analog)
    if (!OmRawOk(rx, ry)) return 0;
    dz = OmClampDz(dz);
    x = (float)rx / 1000.0f;
    y = (float)ry / 1000.0f;
    m = sqrtf(x * x + y * y);
    if (!(m > dz)) return 0;
    mm = m > 1.0f ? 1.0f : m;
    out = OmAnalogLen(mm, dz);  // v3.48 OM348 (was: Direction 1)
    *ox = x / m * out;
    *oy = y / m * out;
    return 1;
}

// ---------------------------------------------------------------- bindings
// gInputButtonPads 0x5FF358 entries 21..24 (0x5FF3AC; drc.dll GetPAM loads the user's input.ini over them): TurnRight
// (bit 0, x+), TurnLeft (bit 1, x-), MoveForward (bit 2, up), MoveBackward (bit 3, down); a pad code is (axis << 8) |
// 1 (below -300) or 3 (above 300), axis 0 lX, 4 lY, 8 lRy, 0x14 lRx. Only the low 16 bits are compared (as 0x406C50).
enum { OMB_LEFT, OMB_SWAPPED, OMB_OTHER };
static const char *const kOmBindName[3] = { "the left stick (stock)", "the right stick's axes (swapped sticks)",
                                            "not a stick" };
static int OmBindings(const unsigned pads[4])
{
    static const unsigned kLeft[4] = { 0x0003, 0x0001, 0x0401, 0x0403 }, kSwap[4] = { 0x1403, 0x1401, 0x0801, 0x0803 };
    int i, l = 1, s = 1;
    for (i = 0; i < 4; ++i) {
        l &= (pads[i] & 0xFFFF) == kLeft[i];
        s &= (pads[i] & 0xFFFF) == kSwap[i];
    }
    return l ? OMB_LEFT : s ? OMB_SWAPPED : OMB_OTHER;
}

// ---------------------------------------------------------------- the self-check
// One frame of one source against the game's direction bits (bits 0..3 of [InputDI+0x38], read after the game's poll)
// and whether a keyboard key of those bits is held (keys set the same bits). An axis clearly past the game's +-300 band
// (OM_T_OUT) needs its bit set: keys only add bits, so this holds with any key, and also through the stock reversal
// latch (it never clears the new side). A source clearly centred (OM_T_IN on both axes) while the game sees a push and
// no key is held disagrees (a blank or stale state). Near the band (OM_T_IN..OM_T_OUT): no verdict.
// v3.46 OM346 (inspection 4, OM346-004): no verdict at all while a movement key is held: the key's bits could make a
// stale vector that points the key's way look right (and win over the live pad), so such frames never count.
#define OM_T_OUT 450
#define OM_T_IN 150
enum { OME_SKIP, OME_AGREE, OME_DISAGREE };
static int OmEval(long rx, long ry, unsigned bits, int keys)
{
    unsigned need = 0;
    if (keys) return OME_SKIP;  // v3.46 OM346 inspection 4
    bits &= 0xF;
    if (rx >= OM_T_OUT) need |= 1;
    else if (rx <= -OM_T_OUT) need |= 2;
    if (ry <= -OM_T_OUT) need |= 4;
    else if (ry >= OM_T_OUT) need |= 8;
    if (need) return (bits & need) == need ? OME_AGREE : OME_DISAGREE;
    if (rx > -OM_T_IN && rx < OM_T_IN && ry > -OM_T_IN && ry < OM_T_IN) {
        if (!bits) return OME_AGREE;
        return keys ? OME_SKIP : OME_DISAGREE;
    }
    return OME_SKIP;
}

// The verdict over the frames with the stick in use (omni.inc feeds only those, in gameplay, with a stick bound).
// PENDING: each source not rejected collects OM_CHECK_N verdicts (a failed read counts as a disagreement); it passes
// with at least 90 % agreement. GetDeviceState first: it wins when it passes; the call sites are used when it was
// rejected (or had no verdict of its own while the call sites passed and then saw 2 x OM_CHECK_N more); both rejected
// = NONE. No float is written while PENDING. After a verdict the chosen source keeps being checked in windows of OM_WIN verdicts: more than
// 10 % disagreement rejects it and the check runs again with the other source (at most OM_CHECKS verdicts a session).
#define OM_CHECK_N 40
#define OM_WIN 600
#define OM_CHECKS 4
enum { OMS_PENDING, OMS_DEVICE, OMS_CALLS, OMS_NONE };
static const char *const kOmSrcName[4] = { "pending", "GetDeviceState", "call sites", "none" };
enum { OMC_NOTHING, OMC_VERDICT, OMC_RECHECK };  // what OmCheckFeed has to say
typedef struct {
    int verdict;       // OMS_*
    int n[2], ok[2];   // verdicts collected / agreeing, per source (0 GetDeviceState, 1 call sites), this check
    int passed[2];     // passed in this check
    int extra[2];      // verdicts seen after it passed (the call sites wait for GetDeviceState: at most 2 x OM_CHECK_N)
    int rejected[2];   // rejected this session
    int checks;        // verdicts made
    int wn, wbad;      // the chosen source's window
    int lastN[2], lastOk[2];  // the numbers of the last verdict or rejection (for its log line)
    int lastSrc;       // the source the last re-check rejected (0 / 1)
} OmCheck;

static void OmCheckInit(OmCheck *c)
{
    int s;
    c->verdict = OMS_PENDING;
    for (s = 0; s < 2; ++s) c->n[s] = c->ok[s] = c->passed[s] = c->extra[s] = c->rejected[s] = c->lastN[s] = c->lastOk[s] = 0;
    c->checks = c->wn = c->wbad = c->lastSrc = 0;
}

static int OmCheckDecide(OmCheck *c)
{
    int v = OMS_PENDING;
    if (c->passed[0]) v = OMS_DEVICE;
    else if (c->passed[1] && (c->rejected[0] || c->extra[1] >= 2 * OM_CHECK_N)) v = OMS_CALLS;
    else if (c->rejected[0] && c->rejected[1]) v = OMS_NONE;
    if (v == OMS_PENDING) return OMC_NOTHING;
    if (v == OMS_CALLS && !c->rejected[0]) c->rejected[0] = 1;  // never reached its verdict: not used this session
    c->verdict = v;
    ++c->checks;
    c->lastN[0] = c->n[0], c->lastN[1] = c->n[1], c->lastOk[0] = c->ok[0], c->lastOk[1] = c->ok[1];
    c->wn = c->wbad = 0;
    return OMC_VERDICT;
}

// v3.46 OM346 (inspection 3, OM346-003): GetDeviceState's verdict this frame. have0 > 0 = read (judged by OmEval);
// have0 < 0 = the read failed: a disagreement only while no movement key is held (with a key held the bits may be the
// keyboard's alone, so a failed read of an idle or lost pad says nothing: skipped; otherwise keyboard walking with a lost pad
// could reject GetDeviceState, the only source of the swapped-stick binding, for the session); have0 == 0 = not asked.
static int OmEvalDevice(int have0, long rx, long ry, unsigned bits, int keys)
{
    if (have0 > 0) return OmEval(rx, ry, bits, keys);
    if (have0 < 0 && !keys) return OME_DISAGREE;
    return OME_SKIP;
}

// v3.46 OM346 (inspection 1, OM346-001): which raw source this frame writes from. have0 > 0 = GetDeviceState read this
// frame (< 0 failed, 0 not asked); have1 = a fresh call-site capture (this frame's poll). GetDeviceState chosen: its read,
// or this frame's call-site capture when the read failed; call sites chosen: the capture. 0 / 1 = the source, -1 = none
// (the game's own floats stay).
// v3.46 OM346 (inspection 5, OM346-005): keys = a movement key held this frame, callsRejected = the self-check rejected
// the call sites. While a key is held the self-check gives no verdict (inspection 4), so a stale GetDeviceState vector
// could not be caught there: this frame's own call-site capture (the values the game itself just read) is used instead
// whenever there is one and the call sites were not rejected.
static int OmPickSource(int verdict, int have0, int have1, int keys, int callsRejected)
{
    if (verdict == OMS_DEVICE && keys && have1 && !callsRejected) return 1;
    if (verdict == OMS_DEVICE && have0 > 0) return 0;
    if ((verdict == OMS_DEVICE || verdict == OMS_CALLS) && have1) return 1;
    return -1;
}

// e0 / e1: OME_* of GetDeviceState / the call sites this frame (OME_SKIP when that source had nothing to say).
static int OmCheckFeed(OmCheck *c, int e0, int e1)
{
    const int e[2] = { e0, e1 };
    int s;
    if (c->verdict == OMS_NONE) return OMC_NOTHING;
    if (c->verdict == OMS_PENDING) {
        for (s = 0; s < 2; ++s) {
            if (c->rejected[s] || e[s] == OME_SKIP) continue;
            if (c->passed[s]) {
                ++c->extra[s];
                continue;
            }
            ++c->n[s];
            c->ok[s] += e[s] == OME_AGREE;
            if (c->n[s] >= OM_CHECK_N) {
                if (c->ok[s] * 10 >= c->n[s] * 9) c->passed[s] = 1;
                else c->rejected[s] = 1;
            }
        }
        return OmCheckDecide(c);
    }
    s = c->verdict == OMS_DEVICE ? 0 : 1;
    if (e[s] == OME_SKIP) return OMC_NOTHING;
    ++c->wn;
    c->wbad += e[s] == OME_DISAGREE;
    if (c->wn < OM_WIN) return OMC_NOTHING;
    if (c->wbad * 10 <= c->wn) {
        c->wn = c->wbad = 0;
        return OMC_NOTHING;
    }
    c->lastN[s] = c->wn, c->lastOk[s] = c->wn - c->wbad;
    c->lastSrc = s;
    c->rejected[s] = 1;
    c->wn = c->wbad = 0;
    for (s = 0; s < 2; ++s) c->n[s] = c->ok[s] = c->passed[s] = c->extra[s] = 0;
    c->verdict = c->checks >= OM_CHECKS || (c->rejected[0] && c->rejected[1]) ? OMS_NONE : OMS_PENDING;
    if (c->verdict == OMS_NONE) ++c->checks;
    return OMC_RECHECK;
}

// ---------------------------------------------------------------- v3.47 CT347: the row on Option 2 (Control)
// Option 2's table (0x654D88 in the exe, 7 rows of 7 dwords, no terminator; Refined's Vibration copy has the same shape)
// is the stock one when row 0 is Button Config (input 0x524940, label 0x27) and row 6 is Exit (input 0x524F90, label
// 0x30): Omni-Directional Movement then goes in above Exit (row 6 of 8; Exit row 7), as Option 1's Menu Font row (font.inc).  // v3.48 OM348: the name
#define OM_CTL_ROWS 7
#define OM_CTL_ROW 6
static int OmControlTableOk(const unsigned long *t, int count)
{
    return t && count == OM_CTL_ROWS && t[0] == 0x524940ul && t[5] == 0x27ul && t[7 * 6 + 0] == 0x524F90ul && t[7 * 6 + 5] == 0x30ul;
}

// ---------------------------------------------------------------- v3.47 CT347: SH4R_StickAmount (the export)
// What Refined's export says for this frame (staging/v347_feedback/w1_control/CONTRACT_analog_pe.md):
//   0      Omni-Directional Movement does nothing: Off, or it cannot act this session (the hook not in or faulted,
//          movement not on a stick, no raw source, the swapped sticks without GetDeviceState): use your own behaviour;
//   1      On in a frame whose stick vector is not Refined's (the self-check pending, the stick inside the dead zone, a
//          menu / pause / cut-scene, the keyboard alone): the full amount, as today;
//   0.6..1 On wrote the stick vector this frame: its length (OmAnalogLen: 0.6 at the dead zone's edge, 1 at a full push).
// active = the mode can act this session (see 0); wrote = this frame's poll wrote the vector, len = its length.
// v3.48 OM348: the meaning is v3.47's with On = Analog (Direction's "always 1" case is gone with Direction).
static float OmExportAmount(int mode, int active, int wrote, float len)
{
    if (mode != OM_ON || !active) return 0.0f;  // v3.48 OM348 (was Direction / Analog)
    if (!wrote) return 1.0f;                    // v3.48 OM348 (was: Direction, or Analog not written)
    if (!(len >= OM_ANALOG_FLOOR)) return OM_ANALOG_FLOOR;  // (also NaN)
    return len > 1.0f ? 1.0f : len;
}

// ---------------------------------------------------------------- v3.47 CT347: GetDeviceState failures and focus loss
// The user's v3.46 log: three "GetDeviceState on the game's pad failed (hr 8007001E)" anomalies in the second the taskbar
// came over the game (GOG's wrapper loses the pad with the focus; Refined keeps the game running behind other windows).
// A run of failed reads (an episode) is judged as a whole: it is a focus loss when the game window was not in front at
// any moment from OM_FG_GRACE ms before it started to OM_FG_GRACE ms after it (a verbose line, at its start, and one when
// the reads work again); otherwise it is a real failure with the game in front (the anomaly, once per episode, after
// OM_FG_GRACE ms of failing or when it ends). The caller feeds every tick: read 1 = GetDeviceState read the pad, -1 =
// it failed, 0 = not asked (or not judged: the self-check still pending); front = the game window is in front now.
#define OM_FG_GRACE 1000u
enum { OMF_NOTHING, OMF_ANOMALY, OMF_BG_START, OMF_BG_END, OMF_END };
typedef struct {
    int open;            // an episode runs (or waits for its verdict)
    int failing;         // its last judged read failed
    int bg;              // a focus loss
    int told;            // its first line was asked for
    int haveBg;          // lastBg is set
    unsigned long t0, lastBg;  // the episode's first failure; the last tick the game was not in front (GetTickCount ms)
    unsigned long frames;      // failed reads in the episode
} OmFail;
static void OmFailInit(OmFail *e)
{
    e->open = e->failing = e->bg = e->told = e->haveBg = 0;
    e->t0 = e->lastBg = e->frames = 0;
}
static int OmFailFeed(OmFail *e, int read, int front, unsigned long now)
{
    int recentBg;
    if (!front) e->lastBg = now, e->haveBg = 1;
    recentBg = e->haveBg && now - e->lastBg <= OM_FG_GRACE;
    if (read < 0) {
        if (!e->open) {
            e->open = 1, e->bg = e->told = 0, e->frames = 0, e->t0 = now;
            if (e->haveBg && now - e->lastBg <= OM_FG_GRACE) e->bg = 1;  // the focus came back just now
        }
        ++e->frames;
        e->failing = 1;
        if (recentBg) e->bg = 1;
        if (e->told) return OMF_NOTHING;
        if (e->bg) {
            e->told = 1;
            return OMF_BG_START;
        }
        if (now - e->t0 >= OM_FG_GRACE) {  // failing for a second with the game in front all along
            e->told = 1;
            return OMF_ANOMALY;
        }
        return OMF_NOTHING;
    }
    if (!e->open) return OMF_NOTHING;
    if (read > 0) e->failing = 0;
    if (e->failing) return OMF_NOTHING;  // (no read this tick: the episode stays as it is)
    if (recentBg) e->bg = 1;             // the focus went away just after the failures
    if (!e->told && !e->bg && now - e->t0 < OM_FG_GRACE) return OMF_NOTHING;  // a short failure in front: wait for its verdict
    e->open = 0;
    if (e->bg) return OMF_BG_END;        // (its start line, too, if none was written yet: the caller writes one line)
    return e->told ? OMF_END : OMF_ANOMALY;
}
// The game window was not in front within OM_FG_GRACE ms (after OmFailFeed for this tick): a failed read then says
// nothing about the source, so the self-check skips it (OmEvalDevice gets have0 = 0).
static int OmFailRecentBg(const OmFail *e, unsigned long now)
{
    return e->haveBg && now - e->lastBg <= OM_FG_GRACE;
}

#endif
