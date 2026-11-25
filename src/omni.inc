// ================================================================ v3.46 OM346: Stick Movement (360-degree / analog)
// Plan: staging/v346_feedback/w1_omni/PLAN.md (spec SPEC_v346.md, research staging/research_360_movement/REPORT.md).
// Included by sh4windowmod.c after gameplay.inc (its GpPatch / GpApplyAll / GpLock and the Player page). Pure logic:
// omni_sim.h (test\omni_sim_test.c). All addresses are GOG v7 VAs, checked against the stock exe (md5 15a85581).
//
// The engine walks at any angle; only the PC port's DirectInput layer makes the stick 8-way (omni_sim.h). One hook:
//   H1  0x5543C0 `E8 5B F9 FF FF` (InputPollPad 0x554390: `push 0; call 0x553D20`, the caller pops at 0x5543CD) ->
//       call OmniSticks: the stock 0x553D20 first, then, while the mode is On (v3.48 OM348; was Direction / Analog), in gameplay
//       ([0x1236D4C] 6, [0x1236D50] 2, [0x1236D54] 1, phase [0xFD5C20] 0), with movement bound to a stick and the raw
//       stick outside the dead zone, the real stick vector into gPad.stick[0].x / y (0x1236C44 / 0x1236C48). Strength /
//       angle (0x1236C4C / 0x1236C50, the menus' direction bits) are never written; nothing between 0x5543C5 and the
//       ret 0x55441D reads x / y. Every reader of the stick floats (the third-person builder 0x52F260, Room 302's 0x52E0B0,
//       the camera-cut latch 0x502040, the getters 0x4F8610 / 0x4F8660 with their script override and locks) sees one
//       vector. Off: OmniSticks only calls 0x553D20 (no float written, no device call).
//   H1b 0x407B15 / 0x407B28 `E8 36 F4 FF FF` / `E8 23 F4 FF FF` (the Poll's AxisEvent calls for lX / lY) -> a stub that
//       keeps the raw value ([esp+4]) and jumps on to whatever 0x406F50 holds (stock, or Playable Eileen's PadAxis jmp).
// The three sites are one all-or-nothing set (GpApplyAll: byte-checked, rolled back on a failed write), listed in
// SH4WM_Patches by Poke. The raw source (GetDeviceState on the game's joystick, or the call sites) is chosen by the
// self-check (omni_sim.h OmCheckFeed); until it has a verdict the game's own floats stay.
// v3.47 CT347 (staging/v347_feedback/w1_control/PLAN.md; the user: "Move this to Option 2 (Control)", "I'd like Analog to
// work with Playable Eileen too"): the row is on Option 2 (Control) above Exit (OmRowInstall; it left Option 5 > Player);
// the export SH4R_StickAmount (winmm.def @199) tells Playable Eileen this frame's analog push (CONTRACT_analog_pe.md),
// and its export SH4PE_StickAmountOn tells the row whether she follows it; a GetDeviceState failure while the game
// window is not in front is a verbose line (a focus loss), the anomaly stays for failures in front (omni_sim.h OmFail).
// v3.48 OM348 (staging/v348_feedback/w1_omni/PLAN.md; the user: "Rename Stick Movement to Omni-Directional Movement.
// Parameters should be Off/On. On = Analog ... So remove Direction please."): the row is "Omni-Directional Movement",
// Off / On; On = v3.47's Analog in every respect (the hook, the self-check, the dead zone, SH4R_StickAmount 0.6..1, Room
// 302 / Playable Eileen's rules). [Input] OmniMove reads Off / On, 0 / 1; v3.46 / v3.47's Direction / Analog / 2 read as
// On and the first start that finds one writes it back once as On (OmReadIni).
// v3.49 LK349 (staging/v349_feedback/w7_look302/PLAN.md; the user: "Room 302 sometimes the right stick does not turn the 1st
// person camera and can get sticky. It's worse for Henry than Eileen"): the game's look is 8-way (+-300 on / off, full
// speed or nothing) and its reversal latch freezes an axis after a direct flick (Playable Eileen v1.24.2 fixes the latch
// only while she is On). [Input] OmniLook (hidden; Off / Room302 (default) / All): OmniSticks also writes the raw look
// stick (GetDeviceState's lRx / lRy, the confirmed source, its own look self-check) into gPad.stick[1] 0x1236C54 / 0x1236C58
// (no new patch site). Room 302 with X turns: the left stick's small x no longer silences the right stick's yaw
// (OmRoomTurnX). One verbose count line per place (the latch, the 8-way misses, who wrote the look).
#include "omni_sim.h"

static volatile LONG g_omMode;       // OM_* ([Input] OmniMove, the row); read by the hook every frame
static LONG g_omSnap;                // as Options opened
static volatile float g_omDz = OM_DZ_DEF;  // [Input] OmniDeadZone (clamped)
static int g_omReady;                // the three sites are in
static int g_omUnsaved;              // the menu's value could not be written to the ini
static volatile LONG g_omFault;      // the hook faulted: off for this session
static DWORD g_omIds[OM_MODES];
// H1b: the call sites' captures (game thread: the Poll and OmniSticks run on it, one after the other)
static volatile long g_omCapX, g_omCapY;
static volatile LONG g_omCapSeq;     // + 1 at every lY capture (one poll with a joystick state)
static LONG g_omCapSeen;
static const DWORD g_omAxisFn = 0x406F50;  // AxisEvent (stock, or PE's jmp to its PadAxis)
// the self-check and the per-session diagnostics (game thread)
static OmCheck g_omChk;
static int g_omBindLogged = -1;      // the bindings verdict last logged (-1 none)
static unsigned g_omBindPads[4];
static volatile LONG g_omBind = OMB_LEFT;  // the last bindings verdict (the row's description reads it)
static DWORD g_omWrites, g_omZeroes, g_omFrom1;  // since the last mode change: frames written, stopped, from the call sites
static DWORD g_omTraceT;
static volatile LONG g_omQDev, g_omQRaw, g_omQGate, g_omQState, g_omQSwap;  // LogQuota counters
// v3.47 CT347: SH4R_StickAmount's value (float bits; written by the game thread once a poll, read by any thread), the
// GetDeviceState failure episodes, Playable Eileen's answer, the row on Option 2
static volatile LONG g_omAmountBits;    // 0 = 0.0f until the first poll with Stick Movement on
static OmFail g_omFail;
static HRESULT g_omFailHr;              // the episode's first failure
static volatile LONG g_omQBg;           // LogQuota: focus-loss episodes logged (verbose)
static volatile LONG g_omPeReads = -1;  // -1 no Playable Eileen or no SH4PE_StickAmountOn, 0 it says no, 1 she follows the amount
static int g_omRow = -1;                // Stick Movement's row on Option 2 (-1 = no row)
static DWORD g_omRowsControl[(OM_CTL_ROWS + 1) * 7 + 1];  // Option 2: its 6 rows, Stick Movement, Exit, the end mark
// v3.49 LK349: the analog look ([Input] OmniLook; omni_sim.h OmLook*), this frame's GetDeviceState axes, the look's
// self-check and the per-place counts (game thread; flushed as one verbose line per place / 5 minutes / mode change)
static volatile LONG g_omLook = OML_ROOM302;
static long g_omJs[4];                  // this frame's GetDeviceState lX, lY, lRx, lRy (g_omJsFresh = 1)
static int g_omJsFresh;
static OmCheck g_omLookChk;
static int g_omLookBindLogged = -1;
static unsigned g_omLookBindPads[4];
typedef struct {
    DWORD pushed, analog, here, key, noRead, check, bind, latch, latchX, latchY, band, turnKept;
    DWORD t0;  // GetTickCount of the first counted frame
} OmLookStats;
static OmLookStats g_olSt;
static int g_olStage = -1;              // the stage the counts belong to
static volatile LONG g_omQLook, g_omQLookBind;  // LogQuota counters
static DWORD g_omLookTraceT;

static const BYTE *OmText(int id);
static void OmPeProbe(void);  // v3.47 CT347 (the row part, below)
static void OmLogCounts(const char *why)
{
    LogV("omni: %s: since the last change %lu frame(s) written (%lu of them from the call sites), %lu stopped inside the "
         "dead zone", why, g_omWrites, g_omFrom1, g_omZeroes);
    g_omWrites = g_omZeroes = g_omFrom1 = 0;
}

// ---------------------------------------------------------------- the hook
static int OmGate(void)  // gameplay: the game running, not an overlay / pause / menu, not a cut-scene
{
    return *(volatile const int *)0x1236D4C == 6 && *(volatile const int *)0x1236D50 == 2 &&
           *(volatile const int *)0x1236D54 == 1 && *(volatile const int *)0xFD5C20 == 0;
}
// A keyboard key of the four movement bits is held (gInputButtonKeys[21..24] 0x5FF334: DIK codes; keyState at
// [InputDI+0x50], 0x80 = down at the last poll). Keys set the same bits as the stick.
static int OmKeysHeld(const BYTE *in)
{
    const volatile int *const keys = (const volatile int *)0x5FF334;
    int i;
    for (i = 0; i < 4; ++i) {
        const int k = keys[i];
        if (k > 0 && k < 0x100 && (in[0x50 + k] & 0x80)) return 1;
    }
    return 0;
}
// H1: GetDeviceState (IDirectInputDevice8 vtable 9) on the game's joystick [InputDI+0xC] into a DIJOYSTATE2 (0x110
// bytes: lX +0, lY +4, lRx +0xC, lRy +0x10). 1 = read, 0 = no joystick, -1 = the call failed (*hr).
static int OmReadDevice(const BYTE *in, int bind, long *x, long *y, HRESULT *hr)
{
    LONG js[0x110 / 4];
    void **const dev = *(void **const *)(in + 0xC);
    *hr = 0;
    if (!dev) return 0;
    memset(js, 0, sizeof js);
    *hr = ((HRESULT(__stdcall *)(void *, DWORD, void *))(*(void ***)dev)[9])(dev, sizeof js, js);
    if (FAILED(*hr)) return -1;
    g_omJs[0] = js[0], g_omJs[1] = js[1], g_omJs[2] = js[3], g_omJs[3] = js[4];  // v3.49 LK349: lX, lY, lRx, lRy for the look
    g_omJsFresh = 1;
    *x = bind == OMB_SWAPPED ? js[3] : js[0];
    *y = bind == OMB_SWAPPED ? js[4] : js[1];
    return 1;
}
static void OmBindNow(int bind, const unsigned p[4])
{
    InterlockedExchange(&g_omBind, bind);
    if (bind == g_omBindLogged && !memcmp(p, g_omBindPads, sizeof g_omBindPads)) return;
    g_omBindLogged = bind;
    memcpy(g_omBindPads, p, sizeof g_omBindPads);
    Log("omni: bindings: TurnRight %04X, TurnLeft %04X, MoveForward %04X, MoveBackward %04X = %s%s", p[0] & 0xFFFF,
        p[1] & 0xFFFF, p[2] & 0xFFFF, p[3] & 0xFFFF, kOmBindName[bind],
        bind == OMB_OTHER ? ": Omni-Directional Movement does nothing until movement is on a stick (the left stick, or the sticks swapped)" :  // v3.48 OM348: the name
        bind == OMB_SWAPPED ? " (the raw axes from GetDeviceState only)" : "");
}
static void OmCheckLog(int r)
{
    const OmCheck *const c = &g_omChk;
    if (r == OMC_VERDICT) {
        OmPeProbe();  // v3.47 CT347: Playable Eileen is loaded by now (logged once, before the verdict line)
        Log("omni: self-check (check %d): GetDeviceState %d of %d agree, call sites %d of %d agree: source %s%s", c->checks,
            c->lastOk[0], c->lastN[0], c->lastOk[1], c->lastN[1], kOmSrcName[c->verdict],
            c->verdict == OMS_NONE ? " (neither matches the game's own direction bits: the game's 8 directions stay)" : "");
        if (c->verdict == OMS_NONE)
            LogAnomaly("omni", "no raw stick source matches the game's direction bits (GetDeviceState %d of %d, call sites %d "
                       "of %d agree): Omni-Directional Movement does nothing this session", c->lastOk[0], c->lastN[0], c->lastOk[1], c->lastN[1]);  // v3.48 OM348: the name
    } else if (r == OMC_RECHECK) {
        const int s = c->lastSrc;
        LogAnomaly("omni", "%s agreed with the game's direction bits in only %d of %d frames: %s", kOmSrcName[1 + s],
                   c->lastOk[s], c->lastN[s], c->verdict == OMS_NONE ? "no source left: the game's 8 directions stay this session"
                                                                       : "self-check again with the other source");
    }
}
static void OmGateLog(void)  // the stick is pushed (bits) but the gate is shut: new game states, once each (verbose)
{
    static int s_seen[12][4], s_n;
    const int st[4] = { *(volatile const int *)0x1236D4C, *(volatile const int *)0x1236D50, *(volatile const int *)0x1236D54,
                        *(volatile const int *)0xFD5C20 };
    int i;
    if (st[0] != 6) return;  // the front end: not ours to judge
    if (st[1] == 2 && st[3] == 0 && st[2] != 1 && st[2] != 2 && LogQuota(&g_omQState, 3))
        LogAnomaly("omni", "a game state never seen: mode 6 / 2, overlay %d, phase 0 (gameplay is overlay 1, menus 2): the "
                   "stick stays 8-way there", st[2]);
    for (i = 0; i < s_n && memcmp(s_seen[i], st, sizeof st); ++i) {}
    if (i < s_n || s_n >= 12) return;
    memcpy(s_seen[s_n++], st, sizeof st);
    if (LogQuota(&g_omQGate, 12))
        LogV("omni: the stick is pushed in game state %d / %d, overlay %d, phase %d: not gameplay, the game's own input stays",
             st[0], st[1], st[2], st[3]);
}
// v3.47 CT347: the game window is in front (no window known yet: counted as in front, so a failure stays an anomaly)
static int OmFront(void) { return !g_hwnd || GetForegroundWindow() == g_hwnd; }
// v3.47 CT347: one tick of the GetDeviceState failure episodes (omni_sim.h OmFail). judged: 1 read, -1 failed, 0 not
// judged (no read, no joystick, or the self-check still pending); cap = a call-site capture stood in this frame.
static void OmFailTick(int judged, HRESULT hr, int front, DWORD now, int cap)
{
    static int s_line;  // this episode's first line: 0 none, 1 the anomaly written, 2 the focus-loss line written, 3 / 4 not written (quota)
    int r;
    if (judged < 0 && !g_omFail.open) g_omFailHr = hr;
    r = OmFailFeed(&g_omFail, judged, front, now);
    switch (r) {
    case OMF_ANOMALY:  // failing with the game window in front: a real failure (the pad unplugged, the wrapper)
        s_line = LogQuota(&g_omQDev, 3) ? 1 : 4;
        if (s_line == 1)
            LogAnomaly("omni", "GetDeviceState on the game's pad failed (hr %08lX) after the game's own read, %lu frame(s) with the "
                       "game window in front%s: those frames %s", (DWORD)g_omFailHr, g_omFail.frames,
                       g_omFail.open ? " so far" : "", cap ? "from the call sites" : "the game's own input");
        if (!g_omFail.open) s_line = 0;  // (a short failure judged after it ended: nothing follows)
        break;
    case OMF_BG_START:
        s_line = LogQuota(&g_omQBg, 12) ? 2 : 3;
        if (s_line == 2)
            LogV("omni: GetDeviceState on the game's pad failed (hr %08lX) while the game window is not in front (focus lost): "
                 "the game's own input until it reads the pad again", (DWORD)g_omFailHr);
        break;
    case OMF_BG_END:
        if (!g_omFail.told) {  // the focus went away only after the failures: one line for the whole episode
            if (LogQuota(&g_omQBg, 12))
                LogV("omni: GetDeviceState on the game's pad failed (hr %08lX) for %lu frame(s) around a focus loss (the game window "
                     "not in front): it reads the pad again", (DWORD)g_omFailHr, g_omFail.frames);
        } else if (s_line == 2) {
            LogV("omni: GetDeviceState reads the game's pad again after %lu failed frame(s) (the game window was not in front)",
                 g_omFail.frames);
        } else if (s_line == 1) {
            Log("omni: GetDeviceState reads the game's pad again after %lu failed frame(s)", g_omFail.frames);
        }
        s_line = 0;
        break;
    case OMF_END:
        if (s_line == 1) Log("omni: GetDeviceState reads the game's pad again after %lu failed frame(s)", g_omFail.frames);
        s_line = 0;
        break;
    default:
        break;
    }
}
// v3.47 CT347: returns 1 = the stick vector was written this frame (*len its length), 0 = not this frame (the game's own
// floats), -1 = Omni-Directional Movement cannot act (movement not on a stick, no source, the swapped sticks without GetDeviceState).  // v3.48 OM348: the name
// *judged / *hr / *cap: this frame's GetDeviceState result for OmFailTick.
static int OmTickBody(int fresh, int front, DWORD now, float *len, int *judged, HRESULT *hrOut, int *cap)  // fresh: the call sites captured lX / lY in this frame's poll
{
    BYTE *const in = *(BYTE *volatile *)0x6E8EA4;
    const volatile unsigned *const pads = (const volatile unsigned *)0x5FF3AC;  // gInputButtonPads[21..24]
    const int mode = (int)g_omMode;
    const float dz = g_omDz;
    unsigned p[4], bits;
    int bind, keys, have0 = 0, have1 = 0, have0e, inUse, e0 = OME_SKIP, e1 = OME_SKIP, r, from1 = 0;
    long x0 = 0, y0 = 0, x1 = 0, y1 = 0, rx, ry;
    HRESULT hr = 0;
    float gx, gy, fx, fy;
    if (!in) return 0;
    p[0] = pads[0], p[1] = pads[1], p[2] = pads[2], p[3] = pads[3];
    bind = OmBindings(p);
    OmBindNow(bind, p);
    if (bind == OMB_OTHER) return -1;
    if (g_omChk.verdict == OMS_NONE) return -1;  // v3.47 CT347 (nothing below acts with no source)
    bits = *(volatile const DWORD *)(in + 0x38) & 0xF;
    if (!OmGate()) {
        if (bits) OmGateLog();
        return 0;
    }
    keys = OmKeysHeld(in);
    if (fresh && bind == OMB_LEFT) {
        x1 = g_omCapX, y1 = g_omCapY;
        have1 = 1;
    }
    *cap = have1;
    if (g_omChk.verdict == OMS_DEVICE || (g_omChk.verdict == OMS_PENDING && !g_omChk.rejected[0])) {
        have0 = OmReadDevice(in, bind, &x0, &y0, &hr);
        *hrOut = hr;
        if (g_omChk.verdict == OMS_DEVICE) *judged = have0 > 0 ? 1 : have0 < 0 ? -1 : 0;  // v3.47 CT347: (was the anomaly here)
    }
    if (bind == OMB_SWAPPED && g_omChk.rejected[0]) {  // the call sites carry lX / lY only
        if (LogQuota(&g_omQSwap, 1))
            Log("omni: the swapped-stick bindings need GetDeviceState, which is not used this session: the game's 8 directions stay");
        return -1;
    }
    // v3.47 CT347: a failed read while the game window is (or just was) not in front says nothing about the source
    have0e = have0 < 0 && (!front || OmFailRecentBg(&g_omFail, now)) ? 0 : have0;
    inUse = bits || (have0 > 0 && OmPushed(x0, y0, dz)) || (have1 && OmPushed(x1, y1, dz));
    if (inUse && g_omChk.verdict != OMS_NONE) {
        e0 = OmEvalDevice(have0e, x0, y0, bits, keys);  // v3.46 OM346 inspection 3: a failed read with a key held = skipped
        if (have1) e1 = OmEval(x1, y1, bits, keys);
        r = OmCheckFeed(&g_omChk, e0, e1);
        if (r != OMC_NOTHING) OmCheckLog(r);
    }
    switch (OmPickSource(g_omChk.verdict, have0, have1, keys, g_omChk.rejected[1])) {  // v3.46 OM346 inspection 5: + keys  // v3.46 OM346 inspection 1: the rule in omni_sim.h (tested)
    case 0: rx = x0, ry = y0; break;
    case 1: rx = x1, ry = y1, from1 = 1; break;  // the call sites, or this frame's capture after a failed GetDeviceState
    default: return g_omChk.verdict == OMS_NONE ? -1 : 0;  // no verdict yet, none, or nothing read this frame: the game's own floats
    }
    if (!OmRawOk(rx, ry)) {
        if (LogQuota(&g_omQRaw, 3))
            LogAnomaly("omni", "raw stick axes %ld, %ld from %s are outside the game's +-1000: not used", rx, ry,
                       from1 ? "the call sites" : "GetDeviceState");
        return 0;
    }
    gx = *(volatile const float *)0x1236C44, gy = *(volatile const float *)0x1236C48;
    if (OmMap(rx, ry, mode, dz, &fx, &fy)) {
        // v3.49 LK349: Room 302 with X turns: a small sideways push no longer blocks the right stick's turn (omni_sim.h OmRoomTurnX)
        const int room = *(volatile const int *)0xFD5A90 == OM_LOOK_STAGE_FP;
        if (OmRoomTurnX(room, room && *(volatile const BYTE *)0x1091AE7 == 0, gx, &fx)) {
            ++g_olSt.turnKept;
            if (fy == 0.0f) return 0;  // (nothing left to write: the game's own 0, 0)
        }
        *(volatile float *)0x1236C44 = fx;
        *(volatile float *)0x1236C48 = fy;
        *len = sqrtf(fx * fx + fy * fy);  // v3.47 CT347: SH4R_StickAmount's value in Analog  // v3.48 OM348: = On
        ++g_omWrites;
        g_omFrom1 += from1;
    } else if (bits && !keys) {  // inside the dead zone while the game reads the stick's own push (OmniDeadZone > 0.30)
        fx = fy = 0.0f;
        *(volatile float *)0x1236C44 = 0.0f;
        *(volatile float *)0x1236C48 = 0.0f;
        ++g_omZeroes;
    } else {
        return 0;
    }
    if (g_logLevel >= 3 && GetTickCount() - g_omTraceT >= 500) {
        g_omTraceT = GetTickCount();
        LogT("omni: raw %ld, %ld (%s) -> stick %.3f, %.3f (the game's %.3f, %.3f; bits %X%s)", rx, ry,
             from1 ? "call sites" : "GetDeviceState", fx, fy, gx, gy, bits, keys ? ", a key held" : "");
    }
    return fx != 0.0f || fy != 0.0f ? 1 : 0;  // (the dead-zone stop writes 0, 0: no input, the export says 1)
}
// ---------------------------------------------------------------- v3.49 LK349: the analog look (omni_sim.h OmLook*)
// One verbose line for a place (Room 302, or a stage under OmniLook=All) with the look stick in use: what wrote the look
// (Refined's analog vector or the game's own 8-way floats, and why), how often the game's reversal latch held an axis
// (Playable Eileen Off before v1.24.3 = vanilla Henry), how often the game's 8-way look missed a push, and how often Room
// 302's X-turns rule kept the right stick's turn. At most 40 a session.
static void OmLookFlush(const char *why)
{
    OmLookStats *const s = &g_olSt;
    char place[24];
    if (!s->pushed && !s->turnKept) return;
    if (g_olStage == OM_LOOK_STAGE_FP) strcpy_s(place, sizeof place, "Room 302");
    else sprintf_s(place, sizeof place, "stage %d", g_olStage);
    if (LogQuota(&g_omQLook, 40))
        LogV("omni: look: %s, %lu s (%s): the look stick pushed %lu frame(s): %lu analog (OmniLook %s), the game's own %lu "
             "(OmniLook not here %lu, a look key %lu, no pad read %lu, look self-check %s %lu, look not on a stick %lu); the game's "
             "reversal latch %lu frame(s) (x %lu, y %lu); a push the game's 8-way look missed %lu frame(s); the right stick's "
             "turn kept from a small sideways push %lu frame(s) (X turns)", place, (GetTickCount() - s->t0) / 1000, why,
             s->pushed, s->analog, kOmLookName[g_omLook >= 0 && g_omLook < OML_MODES ? g_omLook : 0],
             s->here + s->key + s->noRead + s->check + s->bind, s->here, s->key, s->noRead, kOmSrcName[g_omLookChk.verdict & 3],
             s->check, s->bind, s->latch, s->latchX, s->latchY, s->band, s->turnKept);
    memset(s, 0, sizeof *s);
}
// The place (stage) of this frame, before OmTickBody counts into it: a new stage flushes the last one's counts; 5 minutes
// in one place flush too (a long Room 302 visit still leaves a line).
static void OmLookPlace(DWORD now)
{
    const int stage = *(volatile const int *)0xFD5A90;
    if (stage != g_olStage) {
        OmLookFlush("left");
        g_olStage = stage;
    } else if ((g_olSt.pushed || g_olSt.turnKept) && now - g_olSt.t0 >= 300000) {
        OmLookFlush("5 minutes");
    }
    if (!g_olSt.pushed && !g_olSt.turnKept) g_olSt.t0 = now;
}
static int OmLookKeysHeld(const BYTE *in)  // a key of the four look bits (gInputButtonKeys[25..28] 0x5FF344) is down
{
    const volatile int *const keys = (const volatile int *)0x5FF344;
    int i;
    for (i = 0; i < 4; ++i) {
        const int k = keys[i];
        if (k > 0 && k < 0x100 && (in[0x50 + k] & 0x80)) return 1;
    }
    return 0;
}
static void OmLookBindNow(int bind, const unsigned p[4])
{
    if (bind == g_omLookBindLogged && !memcmp(p, g_omLookBindPads, sizeof g_omLookBindPads)) return;
    g_omLookBindLogged = bind;
    memcpy(g_omLookBindPads, p, sizeof g_omLookBindPads);
    if (LogQuota(&g_omQLookBind, 8))
        Log("omni: look bindings: LookRight %04X, LookLeft %04X, LookUp %04X, LookDown %04X = %s%s", p[0] & 0xFFFF, p[1] & 0xFFFF,
            p[2] & 0xFFFF, p[3] & 0xFFFF, kOmLookBindName[bind], bind == OMLB_OTHER ? ": the game's own look (no analog look)" : "");
}
static void OmLookCheckLog(int r)
{
    const OmCheck *const c = &g_omLookChk;
    if (r == OMC_VERDICT && c->verdict == OMS_DEVICE) {
        Log("omni: look self-check: GetDeviceState %d of %d agree with the game's look bits: the analog look is on (OmniLook %s)",
            c->lastOk[0], c->lastN[0], kOmLookName[g_omLook >= 0 && g_omLook < OML_MODES ? g_omLook : 0]);
    } else if (r == OMC_VERDICT || r == OMC_RECHECK) {
        const int ok = r == OMC_VERDICT ? c->lastOk[0] : c->lastOk[c->lastSrc], n = r == OMC_VERDICT ? c->lastN[0] : c->lastN[c->lastSrc];
        Log("omni: look self-check: GetDeviceState %d of %d agree with the game's look bits: the game's own 8-way look stays this session", ok, n);
        LogAnomaly("omni", "the look stick's GetDeviceState axes agreed with the game's look bits in only %d of %d frames: no analog "
                   "look this session", ok, n);
    }
}
// After OmTickBody (which read GetDeviceState into g_omJs when it is the confirmed source): the counts, the look's
// self-check, and the analog look vector into stick[1] (0x1236C54 / 0x1236C58) where [Input] OmniLook applies.
static void OmLookTick(void)
{
    BYTE *const in = *(BYTE *volatile *)0x6E8EA4;
    const volatile unsigned *const pads = (const volatile unsigned *)0x5FF3BC;  // gInputButtonPads[25..28]
    const int stage = g_olStage, look = (int)g_omLook, where = OmLookWhere(look, stage);
    OmLookStats *const s = &g_olSt;
    unsigned p[4], bits;
    int bind, keys, read, pushed, latch, r;
    long rx = 0, ry = 0;
    float fx, fy;
    if (!in || (!where && stage != OM_LOOK_STAGE_FP) || !OmGate()) return;  // counted: Room 302 always, other stages under All
    p[0] = pads[0], p[1] = pads[1], p[2] = pads[2], p[3] = pads[3];
    bind = OmLookBindings(p);
    OmLookBindNow(bind, p);
    bits = (*(volatile const DWORD *)(in + 0x38) >> 4) & 0xF;
    read = g_omJsFresh && g_omChk.verdict == OMS_DEVICE;
    if (read) {
        rx = g_omJs[bind == OMLB_LEFT ? 0 : 2];
        ry = g_omJs[bind == OMLB_LEFT ? 1 : 3];
        if (!OmRawOk(rx, ry)) read = 0;
    }
    pushed = bits || (read && OmPushed(rx, ry, OM_LOOK_DZ));
    if (!pushed) return;
    keys = OmLookKeysHeld(in);
    ++s->pushed;
    latch = OmLatch(bits);
    if (latch) {
        ++s->latch;
        s->latchX += latch & 1;
        s->latchY += latch >> 1;
    }
    if (read && !bits && OmBandMiss(rx, ry, OM_LOOK_DZ)) ++s->band;
    if (where && read && bind != OMLB_OTHER && g_omLookChk.verdict != OMS_NONE) {
        r = OmCheckFeed(&g_omLookChk, OmEval(rx, ry, bits, keys), OME_SKIP);
        if (r != OMC_NOTHING) OmLookCheckLog(r);
    }
    if (!where) ++s->here;
    else if (bind == OMLB_OTHER) ++s->bind;
    else if (keys) ++s->key;
    else if (!read) ++s->noRead;
    else if (g_omLookChk.verdict != OMS_DEVICE) ++s->check;
    else if (OmLookMap(rx, ry, OM_LOOK_DZ, &fx, &fy)) {
        *(volatile float *)0x1236C54 = fx;
        *(volatile float *)0x1236C58 = fy;
        ++s->analog;
        if (g_logLevel >= 3 && GetTickCount() - g_omLookTraceT >= 500) {
            g_omLookTraceT = GetTickCount();
            LogT("omni: look: raw %ld, %ld -> stick[1] %.3f, %.3f (look bits %X)", rx, ry, fx, fy, bits);
        }
    } else {
        ++s->noRead;  // (inside the dead zone with a look bit: not reached without a key; counted with the unread frames)
    }
}
static float OmTick(int fresh, int *active)  // v3.47 CT347: OmTickBody + the failure episodes; returns the written length (-1 none)
{
    const int front = OmFront();
    const DWORD now = GetTickCount();
    int judged = 0, cap = 0, r;
    HRESULT hr = 0;
    float len = -1.0f;
    g_omJsFresh = 0;        // v3.49 LK349: OmReadDevice sets it
    OmLookPlace(now);       // v3.49 LK349: the counts' place before OmTickBody counts into it
    r = OmTickBody(fresh, front, now, &len, &judged, &hr, &cap);
    OmLookTick();           // v3.49 LK349: the analog look (after the stick's own vector)
    OmFailTick(judged, hr, front, now, cap);
    *active = r >= 0;
    return r > 0 ? len : -1.0f;
}
static void OmAmountSet(float a)  // v3.47 CT347
{
    LONG b;
    memcpy(&b, &a, sizeof b);
    InterlockedExchange(&g_omAmountBits, b);
}
// v3.47 CT347: SH4R_StickAmount (export @199, by name; CONTRACT_analog_pe.md): this frame's Stick Movement amount for
// Playable Eileen (omni_sim.h OmExportAmount): 0 = Off / cannot act, 1 = full (Direction, or not Refined's vector this
// frame), 0.6..1 = Analog's push written this frame. v3.48 OM348: the same meaning with On = Analog (Direction is gone:
// 1 = On without Refined's vector this frame). Set once per InputPollPad (the main loop, before the game's logic in
// the same frame; game thread); any thread may read it (one aligned 4-byte load, no side effects).
float __cdecl SH4R_StickAmount(void)
{
    const LONG b = g_omAmountBits;
    float a;
    memcpy(&a, &b, sizeof a);
    return a;
}
static void __cdecl OmniSticks(int pad)  // replaces `call 0x553D20` at 0x5543C0 (cdecl, the caller pops its push 0)
{
    const LONG seq = g_omCapSeq;
    const int fresh = seq != g_omCapSeen;
    const int mode = (int)g_omMode;
    ((void(__cdecl *)(int))0x553D20)(pad);  // stock: the stick shorts -> gPad 0x1236C44..0x1236C60
    g_omCapSeen = seq;                      // (also while Off: a capture older than this frame is never used)
    if (!mode || g_omFault) {               // Off: nothing written, no device call
        if (g_omAmountBits) OmAmountSet(0.0f);  // v3.47 CT347
        if (g_olSt.pushed || g_olSt.turnKept) OmLookFlush(g_omFault ? "the hook faulted" : "Omni-Directional Movement Off");  // v3.49 LK349
        return;
    }
    __try {
        int active = 0;
        const float len = OmTick(fresh, &active);
        OmAmountSet(OmExportAmount(mode, active, len >= 0.0f, len));  // v3.47 CT347
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_omFault, 1);
        OmAmountSet(0.0f);  // v3.47 CT347
        Log("omni: faulted (code %08lX): Omni-Directional Movement is off for this session", GetExceptionCode());  // v3.48 OM348: the name
        LogAnomaly("omni", "the stick hook faulted: Omni-Directional Movement is off for this session");
    }
}
static __declspec(naked) void hkOmCapX(void)  // call from 0x407B15: [esp+4] = lX; on to 0x406F50 with the caller's frame
{
    __asm {
        mov eax, [esp + 4]
        mov g_omCapX, eax
        jmp dword ptr [g_omAxisFn]
    }
}
static __declspec(naked) void hkOmCapY(void)  // call from 0x407B28: [esp+4] = lY (eax / edx are AxisEvent's scratch)
{
    __asm {
        mov eax, [esp + 4]
        mov g_omCapY, eax
        inc dword ptr [g_omCapSeq]
        jmp dword ptr [g_omAxisFn]
    }
}

// ---------------------------------------------------------------- the row and [Input] OmniMove
static void OmSetLocked(int v, const char *from)  // under GpLock
{
    const int was = (int)g_omMode;
    if (v < 0 || v >= OM_MODES) return;
    InterlockedExchange(&g_omMode, v);
    g_omUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Input", "OmniMove", kOmModeName[v], g_iniPath)) {
        g_omUnsaved = 1;
        Log("omni: OmniMove %s (was %s; %s) applies now but could not be saved in SH4Refined.ini (error %lu)", kOmModeName[v],
            kOmModeName[was >= 0 && was < OM_MODES ? was : 0], from, GetLastError());
    } else {
        Log("omni: OmniMove %s (was %s; %s)%s", kOmModeName[v], kOmModeName[was >= 0 && was < OM_MODES ? was : 0], from,
            v != OM_ON || !g_peLoaded ? "" : g_omPeReads == 1 ? "; Playable Eileen follows the push (SH4PE_StickAmountOn)"  // v3.47 CT347  // v3.48 OM348: OM_ON (was OM_ANALOG)
                                                                   : "; with Playable Eileen her walk / run speed stays its own (WalkInput / RunInput)");
    }
    if (v != was) OmLogCounts(kOmModeName[was >= 0 && was < OM_MODES ? was : 0]);
}
static int __cdecl OmIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    if (!g_omReady) return 0;
    GpLock();
    v = StepValue((int)g_omMode, OM_MODES);
    if (v >= 0) OmSetLocked(v, "Options");
    GpUnlock();
    return 0;
}
static int __cdecl OmGet(void)
{
    const int v = (int)g_omMode;
    return v >= 0 && v < OM_MODES ? v : 0;
}
// MSG_OM0 block (OMM_*): the label, the three values, a line per value (Analog: another with Playable Eileen), and the
// reasons the row does nothing
// v3.48 OM348: two values (Off / On), 9 of the 16 ids; On's line with Playable Eileen when she keeps her own speed
enum { OMM_L, OMM_V0, OMM_D0 = OMM_V0 + OM_MODES, OMM_DPE = OMM_D0 + OM_MODES, OMM_NA, OMM_BIND, OMM_NOSRC, OMM_N };
typedef char OmTextsFit[OMM_N <= MSG_RG0 - MSG_OM0 ? 1 : -1];  // the MSG_OM0 block (16 ids)  // v3.47 CT347: ends at MSG_RG0 (the v3.47 text-id chain)
static BYTE g_txtOm[OMM_N][64];  // (the longest text, On's line, is 49 characters + 4 bytes)
static const BYTE *OmText(int id)
{
    const int i = id - MSG_OM0;
    return g_txtOm[i >= 0 && i < OMM_N ? i : OMM_L];
}
static int __cdecl OmDesc(void)
{
    const int v = OmGet();
    if (!g_omReady) return MSG_OM0 + OMM_NA;
    if (v != OM_OFF && g_omBind == OMB_OTHER) return MSG_OM0 + OMM_BIND;
    if (v != OM_OFF && g_omChk.verdict == OMS_NONE) return MSG_OM0 + OMM_NOSRC;
    if (v == OM_ON && g_peLoaded && g_omPeReads != 1) return MSG_OM0 + OMM_DPE;  // v3.47 CT347: until she reads SH4R_StickAmount  // v3.48 OM348: On (was Analog)
    return MSG_OM0 + OMM_D0 + v;
}
static void OmFillRow(DWORD *r)  // v3.47 CT347: Option 2 (Control), above Exit (OmRowInstall; was the Player page)
{
    int v;
    for (v = 0; v < OM_MODES; ++v) g_omIds[v] = MSG_OM0 + OMM_V0 + v;
    FillRow(r, OmIn, OmGet, g_omIds, OM_MODES, MSG_OM0 + OMM_L, OmDesc);
}
static void OmTexts(void)
{
    int v;
    // v3.48 OM348: the label (font 0: 286.4 canvas px from x 40 = 326.4, inside the left value arrow at 344; the stock
    // Spanish "Movimiento Primera persona" on this page is 288.4) and On's lines (font 1: 416.0 / 384.8 px of the 448)
    Encode(g_txtOm[OMM_L], sizeof g_txtOm[0], "Omni-Directional Movement");  // v3.48 OM348 (was "Stick Movement")
    for (v = 0; v < OM_MODES; ++v) Encode(g_txtOm[OMM_V0 + v], sizeof g_txtOm[0], kOmModeName[v]);
    Encode(g_txtOm[OMM_D0 + OM_OFF], sizeof g_txtOm[0], "Off: the left stick walks in 8 directions.");
    Encode(g_txtOm[OMM_D0 + OM_ON], sizeof g_txtOm[0], "On: walk at any angle; a light push walks slower.");  // v3.48 OM348 (Direction's line gone)
    Encode(g_txtOm[OMM_DPE], sizeof g_txtOm[0], "On: any angle (Eileen keeps her own speed).");  // v3.48 OM348 (was "Analog: ...")
    Encode(g_txtOm[OMM_NA], sizeof g_txtOm[0], "Not available in this game (see SH4Refined.log).");
    Encode(g_txtOm[OMM_BIND], sizeof g_txtOm[0], "Movement is not on a stick: no effect.");
    Encode(g_txtOm[OMM_NOSRC], sizeof g_txtOm[0], "The stick could not be read: no effect.");
}
// v3.47 CT347 (the user: "Move this to Option 2 (Control)"): ApplyAspectMenu, after InstallPages and FnRowInstall: Option 2
// becomes a copy of its table (the stock 0x654D88, or Refined's Vibration copy) with Stick Movement above Exit (row 7 of 8,
// the stock 32-pixel rows as Option 3's 8). Only when the table is the stock shape (omni_sim.h OmControlTableOk). Without
// Refined's Options pages ([Window] AspectMenu=0, or a site differs) there is no row; [Input] OmniMove still applies
// (also edited while the game runs).
static void OmRowInstall(void)
{
    const DWORD *const s = g_lpTable[1];
    if (!g_pagesOn) {
        Log("omni: no Options pages (see above): no Omni-Directional Movement row; [Input] OmniMove still applies (also edited while "
            "the game runs)");  // v3.48 OM348: the name
        return;
    }
    if (!OmControlTableOk(s, g_lpCount[1])) {
        Log("omni: Option 2's table is not the stock one (%p, %d rows): no Omni-Directional Movement row; [Input] OmniMove still applies",  // v3.48 OM348: the name
            (const void *)s, g_lpCount[1]);
        return;
    }
    memcpy(g_omRowsControl, s, OM_CTL_ROW * 7 * 4);  // Button Config, Default Control, Vibration, Head Motion, First Person Movement / Camera
    OmFillRow(g_omRowsControl + OM_CTL_ROW * 7);
    memcpy(g_omRowsControl + (OM_CTL_ROW + 1) * 7, s + OM_CTL_ROW * 7, 7 * 4);  // Exit (Option 2's own record)
    g_omRowsControl[(OM_CTL_ROW + 2) * 7] = 0;
    g_lpTable[1] = g_omRowsControl;
    g_lpCount[1] = OM_CTL_ROWS + 1;
    g_omRow = OM_CTL_ROW;
    Log("omni: the Omni-Directional Movement row is on Option 2 (Control), row %d of %d above Exit%s: %s", OM_CTL_ROW + 1, OM_CTL_ROWS + 1,  // v3.48 OM348: the name
        s == (const DWORD *)0x654D88 ? "" : " (with Refined's Vibration levels)", kOmModeName[OmGet()]);
}
// v3.47 CT347: RowChangedDraw (sh4windowmod.c) for the slot holding Option 2: -1 = not the Stick Movement row
static int OmPageRowChanged(int page, int row) { return page != 1 || g_omRow < 0 || row != g_omRow ? -1 : OmRowChanged(); }
// v3.47 CT347: Playable Eileen's answer (CONTRACT_analog_pe.md): her export `int __cdecl SH4PE_StickAmountOn(void)` = 1 while
// she scales her walk / run by SH4R_StickAmount, 0 while not (her own switch); no export (older PE) = she keeps her speed.
// Asked as Options opens (OmSnapshot) and at a self-check verdict (game thread); a change is logged.
static void OmPeProbe(void)
{
    static int s_told;  // the answer was logged once with Playable Eileen loaded
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    const int pe = ds && HasExport(ds, "SH4PE_Patches");
    int (__cdecl *f)(void) = NULL;
    LONG r = -1;
    if (pe) f = (int(__cdecl *)(void))GetProcAddress(ds, "SH4PE_StickAmountOn");
    if (f) {
        __try {
            r = f() ? 1 : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            r = -1;
        }
    }
    if (r == g_omPeReads && (s_told || !pe)) return;
    InterlockedExchange(&g_omPeReads, r);
    if (pe) {
        s_told = 1;
        Log("omni: Playable Eileen %s", r == 1 ? "follows the stick's push (SH4PE_StickAmountOn 1): her walk / run speed scales with it"  // v3.48 OM348: was "Analog's push"
                                       : r == 0 ? "reads SH4R_StickAmount but has it off (SH4PE_StickAmountOn 0): her speed stays her own"
                                                : "does not read SH4R_StickAmount (no SH4PE_StickAmountOn): her speed stays her own");
    }
}
// The ini texts: an unknown or clamped value is logged once per text (the ini poll reads them again at every write).
// v3.48 OM348: Off / On (0 / 1); *legacy = 1 for v3.46 / v3.47's Direction / Analog / 2 (read as On). start = the first
// read (OmReadIni logs and writes an old value itself); later reads (the ini edited while the game runs) log an old value
// once per text and leave the file alone: the next start writes it back.
static int OmParseLogged(const char *buf, int start, int *legacy)
{
    static char s_last[32] = "\x01";
    int known, old;
    const int v = OmParseMode(buf, &known, &old);
    if (strcmp(buf, s_last)) {
        if (!known) Log("omni: [Input] OmniMove=%s is not Off or On (0, 1): Off", buf);  // v3.48 OM348 (was Off, Direction or Analog (0, 1, 2))
        else if (old && !start)
            Log("omni: [Input] OmniMove=%s (v3.46 / v3.47) is read as On (Omni-Directional Movement); the next start writes it as On", buf);
    }
    strcpy_s(s_last, sizeof s_last, buf);
    if (legacy) *legacy = old;
    return v;
}
static float OmReadDz(const char *buf)  // empty = the default; not a number = the default; else clamped 0.05..0.90
{
    static char s_last[32] = "\x01";
    double d = OM_DZ_DEF;
    const int number = !buf[0] || OmDzNumber(buf, &d);  // v3.46 OM346 inspection 2: the whole text must be a number
    const float c = number ? OmClampDz((float)d) : OM_DZ_DEF;
    if (buf[0] && (!number || c != (float)d) && strcmp(buf, s_last))
        Log("omni: [Input] OmniDeadZone=%s is read as %.2f (a number from 0.05 to 0.90)", buf, c);
    strcpy_s(s_last, sizeof s_last, buf);
    return c;
}
// v3.49 LK349: [Input] OmniLook's text (omni_sim.h OmLookParse); an unknown text is logged once per text and reads as Room302
static int OmReadLook(const char *buf)
{
    static char s_last[32] = "\x01";
    int known;
    const int v = OmLookParse(buf, &known);
    if (!known && strcmp(buf, s_last)) Log("omni: [Input] OmniLook=%s is not Off, Room302 or All (0, 1, 2): Room302", buf);
    strcpy_s(s_last, sizeof s_last, buf);
    return v;
}
static void OmReadIni(void)
{
    char buf[32];
    int old;
    IniStr("Input", "OmniMove", buf, sizeof buf);
    g_omMode = OmParseLogged(buf, 1, &old);
    if (old) {  // v3.48 OM348: v3.46 / v3.47's Direction / Analog / 2 = On, written back once (the next start reads On)
        const int ok = !g_iniFallback && WritePrivateProfileStringA("Input", "OmniMove", kOmModeName[OM_ON], g_iniPath);
        const DWORD err = ok || g_iniFallback ? 0 : GetLastError();
        if (ok)
            Log("omni: [Input] OmniMove=%s (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): "
                "written as On", buf);
        else if (g_iniFallback)
            Log("omni: [Input] OmniMove=%s (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): "
                "not written (SH4Refined.ini could not be made this start: nothing is written)", buf);
        else
            Log("omni: [Input] OmniMove=%s (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): "
                "could not be written to SH4Refined.ini (error %lu): On for this start, the next start tries again", buf, err);
    }
    IniStr("Input", "OmniDeadZone", buf, sizeof buf);
    g_omDz = OmReadDz(buf);
    IniStr("Input", "OmniLook", buf, sizeof buf);  // v3.49 LK349
    g_omLook = OmReadLook(buf);
}
static void OmIniReread(void)  // under GpLock (GpIniReread, when SH4Refined.ini changed)
{
    char buf[32];
    int v;
    float dz;
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Input", "OmniDeadZone", "", buf, sizeof buf, g_iniPath);
    dz = OmReadDz(buf);
    if (dz != g_omDz) {
        Log("omni: [Input] OmniDeadZone read again: %.2f (was %.2f)", dz, g_omDz);
        g_omDz = dz;
    }
    GetPrivateProfileStringA("Input", "OmniLook", kOmLookName[OML_ROOM302], buf, sizeof buf, g_iniPath);  // v3.49 LK349
    v = OmReadLook(buf);
    if (v != (int)g_omLook) {
        Log("omni: [Input] OmniLook read again: %s (was %s)", kOmLookName[v], kOmLookName[(int)g_omLook]);
        InterlockedExchange(&g_omLook, v);
    }
    if (g_omUnsaved) return;  // the menu's value stands until it is saved or set again
    GetPrivateProfileStringA("Input", "OmniMove", kOmModeName[OmGet()], buf, sizeof buf, g_iniPath);
    v = OmParseLogged(buf, 0, NULL);  // v3.48 OM348: an old value reads as On (logged once; written back at the next start)
    if (v == OmGet()) return;
    Log("omni: [Input] OmniMove read again: %s (was %s)", kOmModeName[v], kOmModeName[OmGet()]);
    OmLogCounts(kOmModeName[OmGet()]);
    InterlockedExchange(&g_omMode, v);
    g_omSnap = v;  // an outside edit is the new snapshot (as [Gameplay]): "No" at the exit never undoes it
}
static void OmSnapshot(void)
{
    g_omSnap = g_omMode;
    OmPeProbe();  // v3.47 CT347
}
static int OmChanged(void) { return g_omRow >= 0 && g_omMode != g_omSnap; }  // v3.47 CT347: asked by PadChanged (Option 2's row; was GpChanged)
static int OmRowChanged(void) { return g_snapOn && g_omMode != g_omSnap; }
static void OmDiscard(void)  // under GpLock (GpDiscard)
{
    if (g_omMode != g_omSnap) OmSetLocked((int)g_omSnap, "Options: No at the exit");
}
static void OmInstall(void)
{
    static const Site kPoll = { 0x5543B9, { 0x6A, 0x00, 0xA3, 0x00, 0x6D, 0x23, 0x01, 0xE8, 0x5B, 0xF9, 0xFF, 0xFF, 0x8B, 0x35,
                                            0x30, 0x6C, 0x23, 0x01, 0x8B, 0xCE, 0x83, 0xC4, 0x04 }, 23 };  // push 0; mov; call 553D20; ..; add esp, 4
    static const Site kCapX = { 0x407B07, { 0x8B, 0x44, 0x24, 0x28, 0x6A, 0x00, 0x68, 0x9C, 0x8F, 0x6E, 0x00, 0x50, 0x8B, 0xCB,
                                            0xE8, 0x36, 0xF4, 0xFF, 0xFF }, 19 };  // eax = lX; push 0; push 6E8F9C; push eax; call 406F50
    static const Site kCapY = { 0x407B1A, { 0x8B, 0x4C, 0x24, 0x2C, 0x6A, 0x04, 0x68, 0xA0, 0x8F, 0x6E, 0x00, 0x51, 0x8B, 0xCB,
                                            0xE8, 0x23, 0xF4, 0xFF, 0xFF }, 19 };  // ecx = lY; push 4; push 6E8FA0; push ecx; call 406F50
    GpPatch set[3];
    OmCheckInit(&g_omChk);
    OmLookCheckInit(&g_omLookChk);  // v3.49 LK349
    if (!SiteOk(&kPoll) || !SiteOk(&kCapX) || !SiteOk(&kCapY)) {
        Log("omni: %s is not the stock code (another mod?): Omni-Directional Movement does nothing",  // v3.48 OM348: the name
            !SiteOk(&kPoll) ? "005543C0 (call 00553D20)" : !SiteOk(&kCapX) ? "00407B15 (the lX call of 00406F50)"
                                                                           : "00407B28 (the lY call of 00406F50)");
    } else {
        GpBranch(&set[0], 0x407B15, 0xE8, hkOmCapX, 5);
        GpBranch(&set[1], 0x407B28, 0xE8, hkOmCapY, 5);
        GpBranch(&set[2], 0x5543C0, 0xE8, OmniSticks, 5);
        g_omReady = GpApplyAll(set, 3, "omni");
    }
    Log("omni: %s (005543C0 after 00553D20: the stick floats; raw axes from GetDeviceState on the game's pad or the call sites "
        "00407B15 / 00407B28, picked by a self-check); [Input] OmniMove %s, OmniDeadZone %.2f; gameplay only", g_omReady ? "in" : "OFF",
        kOmModeName[OmGet()], g_omDz);
    if (!g_omReady && g_omMode) LogAnomaly("omni", "[Input] OmniMove is %s but the hook is not in: the stick stays 8-way", kOmModeName[OmGet()]);
    Log("omni: look: [Input] OmniLook %s (%s; while Omni-Directional Movement is On, with GetDeviceState the confirmed source and "  // v3.49 LK349
        "the look on a stick: the raw stick into stick[1] 01236C54 / 01236C58, dead zone %.2f); Room 302's X-turns rule for the "
        "left stick's x: on", kOmLookName[(int)g_omLook], g_omLook == OML_OFF ? "the game's own 8-way look" : g_omLook == OML_ALL
        ? "the analog look in every stage" : "the analog look in Room 302's first-person view", OM_LOOK_DZ);
}
