// ================================================================ v3.59 CT359: the control-direction latch and Lock-On Walk
// Plan: staging/v359_feedback/w9_controls/PLAN.md (the user, 2026-11-27: "In OTS mode and maybe even in vanilla cam we
// sometimes loose control of the player character as if something is trying to take over ... Affects analogue controller
// movement only it seems"; "Allow player to move while locked onto an enemy with all weapons"). Included by omni.inc
// (after its state: g_omMode, g_omReady, g_omFault; camera.inc's CamOtsDriving / g_cam; gameplay.inc's GpPatch set and
// GpLock). Pure rules: omnilatch_sim.h (test\omnilatch_sim_test.c). GOG v7 VAs, checked against the stock exe (md5 15a85581).
//
// L1  0x502794 `E8 A7 F8 FF FF` (CameraWorldUpdate 0x5025D0: `push [cutFrames]; mov edi, gTargetState; mov esi, drawCamera;
//     call 0x502040`, the caller pops) -> call hkLtLatch: the game's CameraControlDirectionTimer runs as it is, with two
//     changes while [Input] OmniLatch=1 (default) and Omni-Directional Movement is On or an over-the-shoulder camera draws
//     the view (omnilatch_sim.h LtActive):
//     - a running latch is let go as soon as the stick's angle moves more than 15 degrees from the latched one, or the stick
//       is pushed from centre / let go to centre (LtRelease): before the call the latched stick (gCamWorld.stickX / Y,
//       0x1083664 / 0x1083668) is set 1.0 away from the current one, so the game's own 0.7 test lets it go through its own
//       release path (the camera's forward, PlayerControlDirSet, the stick copied back at the end): no state of ours stays;
//     - under the over-the-shoulder view the stock rig's look rate (gCamRig.lookRate 0x10844B0, read only through 0x504A50
//       inside this call) reads 0 for the call (saved and put back right after): Refined's view does not turn with the rig
//       the player cannot see. Room starts / cuts and real jumps of the drawn camera (0xFD5AC0: Refined's matrix) still
//       latch as in the game.
//     OmniLatch=0: the game's latch unchanged; the episodes are still logged (the verbose lines below).
// L2  0x534C23 `E8 F8 F9 FF FF` (PlayerRoomTaskUpdate 0x534AE0, mode 2 and not hurt: `lea edx, [esp+20]; mov esi, ebx;
//     call 0x534620`, the ready-stance step; esi = the input block 0x10A1A00, edx = the wanted position) -> call hkLwStep:
//     [Gameplay] LockOnWalk On (default; the Options row Gameplay > Combat > Lock-On Walk): in the ready stance (the weapon
//     raised, locked on or not, every weapon: the step code is one for all of them) any push steps. The game steps only
//     while the move amount (flags+0) passes 0.9 at 0x53462E and picks the step clip by the same 0.9 at 0x53CA17 (Playable
//     Eileen points both at 0.9 x 30 / rate): an analog push below about 0.9, a latch blend (the control vector shorter
//     than 1) or 60+ fps without Playable Eileen gave none: the player only turned to the target. The amount is raised to
//     a full push's for this frame (LwFull: 30 / rate with Playable Eileen's gates, 1.0 with the game's own), so the step
//     and the clip chooser both pass; with the game's own 9.1 step operand (0x534678 / 0x534697, Playable Eileen's holds
//     the rate) the step is scaled by 30 / rate after the call. Playable Eileen's ready-stance code (StrafeState,
//     NoStanceStep, ReadyStraight, FlagGates) keeps working on the same state: it picks her clips; nothing of it is patched.
// Both sites are byte-checked (with their surroundings), each its own all-or-nothing set (GpApplyAll), listed in
// SH4WM_Patches by Poke, free of Playable Eileen's sites and of the cheat table's (ct_conflicts.py --span: none).
#include "omnilatch_sim.h"

static volatile LONG g_ltMode = LT_REFINED;  // [Input] OmniLatch
static int g_ltReady;                        // L1 in
static volatile LONG g_ltFault;              // the latch hook faulted: the game's latch for this session
static volatile LONG g_lwMode = LW_ON;       // [Gameplay] LockOnWalk (the row)
static LONG g_lwSnap;
static int g_lwReady, g_lwUnsaved;           // L2 in; the menu's value could not be written
static volatile LONG g_lwFault;
static DWORD g_lwIds[LW_MODES];
// the latch's current episode (game thread)
typedef struct {
    int on;               // a latch runs (the timer > 0 after the call)
    int trig;             // LTT_* (LTT_NONE: found running)
    float trigVal;        // a jump's angle (degrees), a look rate (deg/s)
    DWORD t0;             // GetTickCount at its start
    int ots, omni, follow, warp, act;
    float lx, ly;         // the stick at the latch
    float maxOff;         // the control frame's largest angle from the drawn camera (degrees)
    DWORD blend;          // frames in its blend (timer < 1)
} LtEp;
static LtEp g_ltEp;
static DWORD g_ltWarpT;                      // GetTickCount of the last frame the player moved > 280 u (a warp)
static int g_ltWarpSeen;
typedef struct {
    DWORD n, trig[LTT_N], why[LTR_N], game, timeout, recut, supp, unlogged;  // why[LTR_HOLD] unused
    DWORD lwFrames, lwLocked, lwFirstItem;   // Lock-On Walk: frames raised, of them with a lock-on target, the first item
    int lwItems[4], lwItemN;
} LtRoom;
static LtRoom g_ltRoom;
static int g_ltStage = -1000, g_ltRoomId = -1000;
static volatile LONG g_ltQEp, g_ltQEpAll, g_ltQSum, g_ltQBad;  // LogQuota counters (g_ltQEp: per room)
static int g_ltSuppOn;                       // the last frame's call had the look rate ignored
static DWORD g_ltTraceT;
static int g_lwFirstSaid;
static float g_lwScaleNow = 1.0f;            // L2: this call's step scale (game thread)
static const char *const kLtTrigName[LTT_N] = { "a latch already running", "a room start or camera cut",
                                                "a drawn-camera jump", "the stock rig's look rate" };
static const char *const kLtWhyName[LTR_N] = { "", "the stick's angle (15 degrees)", "a push from centre", "the stick let go" };

// ---------------------------------------------------------------- L1: the latch
typedef float(__cdecl *LtStickFn)(int);
static float LtStick(int y) { return ((LtStickFn)(ULONG_PTR)(y ? 0x530630 : 0x530610))(0); }  // the getters 0x502040 reads
// The game's 0x502040 with its register arguments (esi = the drawn camera, edi = gTargetState[0], the cut count pushed).
static __declspec(naked) void __cdecl LtCallStock(int cut, const float *cam, const void *who)
{
    __asm {
        push esi
        push edi
        mov esi, [esp + 0x10]      // cam
        mov edi, [esp + 0x14]      // who
        push dword ptr [esp + 0x0C]  // cut
        mov eax, 0x502040
        call eax
        add esp, 4
        pop edi
        pop esi
        ret
    }
}
static float LtRowDot(const float *a, const float *b) { return a[2] * b[2] + a[1] * b[1] + a[0] * b[0]; }
static float LtDeg(float rad) { return rad * (180.0f / LT_PI); }

// The room's summary (one verbose line when anything happened) and the next room's quotas.
static void LtRoomFlush(void)
{
    LtRoom *const r = &g_ltRoom;
    if ((r->n || r->supp || r->lwFrames) && LogQuota(&g_ltQSum, 300)) {
        if (r->n || r->supp)
            LogV("omni: latch: stage %d room %d: %lu episode(s) (%lu room starts / cuts, %lu camera jumps, %lu rig look rates, %lu "
                 "found running); let go by the stick's angle %lu, a push %lu, the stick let go %lu, the game's 0.7 %lu, its time %lu, "
                 "a new cut %lu; %lu rig look-rate trigger(s) ignored under the Shoulder camera; %lu line(s) over the room's quota; "
                 "OmniLatch %s", g_ltStage, g_ltRoomId, r->n, r->trig[LTT_CUT], r->trig[LTT_JUMP], r->trig[LTT_RATE],
                 r->trig[LTT_NONE], r->why[LTR_ANGLE], r->why[LTR_PUSHED], r->why[LTR_LETGO], r->game, r->timeout, r->recut, r->supp,
                 r->unlogged, kLtModeName[g_ltMode == LT_STOCK ? 0 : 1]);
        if (r->lwFrames) {
            char items[48] = "";
            int i;
            for (i = 0; i < r->lwItemN; ++i) {
                char one[12];
                sprintf_s(one, sizeof one, "%s0x%02X", i ? ", " : "", r->lwItems[i] & 0xFF);
                strcat_s(items, sizeof items, one);
            }
            LogV("omni: lock-on walk: stage %d room %d: %lu frame(s) stepped below a full push (%lu of them locked on), weapons %s",
                 g_ltStage, g_ltRoomId, r->lwFrames, r->lwLocked, items[0] ? items : "none");
        }
    }
    memset(r, 0, sizeof *r);
    InterlockedExchange(&g_ltQEp, 0);
}
static void LtRoomTick(void)
{
    const int stage = *(volatile const int *)0xFD5A90, room = *(volatile const int *)0x10835E8;  // gGame.stage, gCamWorld.room
    if (stage == g_ltStage && room == g_ltRoomId) return;
    LtRoomFlush();
    g_ltStage = stage, g_ltRoomId = room;
}
// The end of an episode: one verbose line (12 a room, 600 a session).
static void LtEnd(const char *why, int whyCode, float cx, float cy, DWORD now)
{
    LtEp *const e = &g_ltEp;
    LtRoom *const r = &g_ltRoom;
    char trig[96];
    const float deg = LtStickDeg(e->lx, e->ly, cx, cy), dist = LtStickDist(e->lx, e->ly, cx, cy);
    e->on = 0;
    if (whyCode > 0 && whyCode < LTR_N) ++r->why[whyCode];
    if (e->trig == LTT_JUMP) sprintf_s(trig, sizeof trig, "%s (%.0f deg in a frame)", kLtTrigName[LTT_JUMP], e->trigVal);
    else if (e->trig == LTT_RATE) sprintf_s(trig, sizeof trig, "%s (%.0f deg/s)", kLtTrigName[LTT_RATE], e->trigVal);
    else strcpy_s(trig, sizeof trig, kLtTrigName[e->trig >= 0 && e->trig < LTT_N ? e->trig : 0]);
    if (!LogQuota(&g_ltQEp, 12) || !LogQuota(&g_ltQEpAll, 600)) {
        ++r->unlogged;
        return;
    }
    LogV("omni: latch: %s held the control frame %.2f s (%s, Omni %s%s%s, rule %s): let go by %s; the stick %.0f deg / %.2f from "
         "the latch; the control frame up to %.0f deg from the camera, %lu frame(s) blended", trig, (now - e->t0) / 1000.0f,
         e->ots ? "the Shoulder camera's view" : "the game's camera", e->omni ? "On" : "Off", e->ots && e->follow ? ", Follow on" : "",
         e->warp ? ", a warp of the player in the 2 s before" : "", e->act ? "Refined's" : "the game's", why, deg, dist, e->maxOff,
         e->blend);
}
// After the game's call: the episode's start / end / extent, the warp watch, the room summary, the level-3 trace.
static void LtAfter(int cut, const float *cam, int trig, float trigVal, int supp, int forced, float left, float cx, float cy,
                    int ots, int omni, int act)
{
    LtEp *const e = &g_ltEp;
    LtRoom *const r = &g_ltRoom;
    const DWORD now = GetTickCount();
    const float t1 = *(volatile const float *)0x1083660;
    const float camYaw = LtCamYaw(cam), ctrlYaw = *(volatile const float *)0x10A1918;  // PlayerControlDirSet's reference yaw
    LtRoomTick();
    {   // a warp: the player moved > 280 u since the last frame (the camera's own test; gCamWorld.prevFocus is set after us)
        const float *pos = (const float *)0x10A1B70, *prev = (const float *)0x1083680;
        const float dx = pos[0] - prev[0], dy = pos[1] - prev[1], dz = pos[2] - prev[2];
        if (dx * dx + dy * dy + dz * dz > 280.0f * 280.0f) g_ltWarpT = now, g_ltWarpSeen = 1;
    }
    if (supp && !g_ltSuppOn) ++r->supp;
    g_ltSuppOn = supp;
    if (e->on && cut && e->trig == LTT_CUT && t1 > 0.0f) {  // a room start latches every frame while cutFrames runs: one episode
        e->lx = *(volatile const float *)0x1083664, e->ly = *(volatile const float *)0x1083668;
    } else if (e->on && (cut || !(t1 > 0.0f))) {
        if (cut) {
            ++r->recut;
            LtEnd("a new room start or cut", 0, cx, cy, now);
        } else if (forced) {
            LtEnd(kLtWhyName[forced], forced, cx, cy, now);
        } else if (left > 0.0f) {
            ++r->game;
            LtEnd("the stick moved more than 0.7 (the game's own rule)", 0, cx, cy, now);
        } else {
            ++r->timeout;
            LtEnd("its time ran out", 0, cx, cy, now);
        }
    }
    if (!e->on && t1 > 0.0f) {
        memset(e, 0, sizeof *e);
        e->on = 1;
        e->trig = trig;
        e->trigVal = trigVal;
        e->t0 = now;
        e->ots = ots, e->omni = omni, e->act = act;
        e->follow = ots && g_cam.follow;
        e->warp = g_ltWarpSeen && now - g_ltWarpT <= 2000;
        e->lx = *(volatile const float *)0x1083664, e->ly = *(volatile const float *)0x1083668;
        ++r->n;
        ++r->trig[trig >= 0 && trig < LTT_N ? trig : LTT_NONE];
    }
    if (e->on) {
        const float off = LtDeg(fabsf(LtWrapAngle(ctrlYaw - camYaw)));
        if (off > e->maxOff) e->maxOff = off;
        if (t1 < 1.0f) ++e->blend;
    }
    if (g_logLevel >= 3 && (cx * cx + cy * cy) >= LT_MIN_PUSH * LT_MIN_PUSH && now - g_ltTraceT >= 100) {
        g_ltTraceT = now;
        LogT("omni: latch trace: stick %.3f, %.3f, timer %.2f, facing %.1f, control yaw %.1f, camera yaw %.1f%s", cx, cy, t1,
             LtDeg(*(volatile const float *)0x10A0FC4), LtDeg(ctrlYaw), LtDeg(camYaw), supp ? ", the rig's look rate ignored" : "");
    }
}
static void __cdecl LtWrap(int cut, const float *cam, const void *who)
{
    float *const rate = (float *)0x10844B0;  // gCamRig.lookRate (0x504A50 copies it)
    float saved[4];
    int supp = 0, forced = LTR_HOLD, trig = LTT_NONE, ots = 0, omni = 0, act = 0, pre = 0;
    float left = 0.0f, cx = 0.0f, cy = 0.0f, trigVal = 0.0f;
    if (!g_ltFault) {
        __try {
            const float *ctrl = (const float *)0x1083610;  // gCamWorld.ctrl: the last frame's camera
            const float t0 = *(volatile const float *)0x1083660;
            const float lx = *(volatile const float *)0x1083664, ly = *(volatile const float *)0x1083668;
            const int tick = *(volatile const int *)0x5D5248;
            const float d0 = LtRowDot(ctrl, cam), d1 = LtRowDot(ctrl + 4, cam + 4), d2 = LtRowDot(ctrl + 8, cam + 8);
            const float rl = sqrtf(rate[0] * rate[0] + rate[1] * rate[1] + rate[2] * rate[2]);
            float dmin;
            ots = CamOtsDriving();
            omni = g_omMode == OM_ON && g_omReady && !g_omFault;
            act = LtActive((int)g_ltMode, omni, ots);
            cx = LtStick(0), cy = LtStick(1);
            left = LtTimerLeft(t0, 1.0f / (float)(tick > 0 ? tick : 30));
            trig = LtTrigger(cut, d0, d1, d2, LtRateHold(rl), left);
            dmin = d0 < d1 ? d0 : d1;
            dmin = d2 < dmin ? d2 : dmin;
            trigVal = trig == LTT_JUMP ? LtDeg(acosf(dmin < -1.0f ? -1.0f : dmin > 1.0f ? 1.0f : dmin)) : trig == LTT_RATE ? LtDeg(rl) : 0.0f;
            if (act && ots && trig == LTT_RATE) supp = 1;  // the over-the-shoulder view: the rig's turn is not the player's
            if (act && !cut && (left > 0.0f || trig == LTT_JUMP || (trig == LTT_RATE && !supp)))
                forced = LtRelease(lx, ly, cx, cy, LT_MIN_PUSH, LT_COS_DEF);
            pre = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&g_ltFault, 1);
            Log("omni: latch: faulted (code %08lX): the game's control-direction latch for this session", GetExceptionCode());
            LogAnomaly("omni", "the latch hook faulted: the game's control-direction latch for this session");
        }
    }
    if (pre && supp) {
        memcpy(saved, rate, sizeof saved);
        rate[0] = rate[1] = rate[2] = 0.0f;
    }
    if (pre && forced) {  // the game's own 0.7 test lets go (its release path, its stick copy at the end)
        *(volatile float *)0x1083664 = cx + 1.0f;
        *(volatile float *)0x1083668 = cy;
    }
    LtCallStock(cut, cam, who);
    if (pre && supp) memcpy(rate, saved, sizeof saved);
    if (pre && !g_ltFault) {
        __try {
            LtAfter(cut, cam, supp ? LTT_NONE : trig, trigVal, supp, forced, left, cx, cy, ots, omni, act);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&g_ltFault, 1);
            Log("omni: latch: its log faulted (code %08lX): the game's control-direction latch for this session", GetExceptionCode());
        }
    }
}
static __declspec(naked) void hkLtLatch(void)  // call from 0x502794: [esp+4] = the cut count, esi = the drawn camera, edi = who
{
    __asm {
        push edi
        push esi
        push dword ptr [esp + 0x0C]
        call LtWrap
        add esp, 0x0C
        ret
    }
}

// ---------------------------------------------------------------- L2: Lock-On Walk
static void __cdecl LwBefore(float *in)
{
    g_lwScaleNow = 1.0f;
    if (!g_lwReady || g_lwFault || g_lwMode == LW_OFF) return;
    __try {
        const BYTE *const p = *(const BYTE *volatile *)0xFD5BA0;
        const int rate = *(volatile const int *)0x5D5248;
        const int stockGate = *(volatile const DWORD *)0x534630 == 0x5B7520 || *(volatile const DWORD *)0x53CA19 == 0x5B7520;
        const int stockStep = *(volatile const DWORD *)0x53467A == 0x5C5BF4;
        const float f = in[0], full = LwFull(rate, stockGate);
        float nf;
        if (!p) return;
        nf = LwForward(1, *(const int *)(p + 0x214), *(volatile const int *)0xFD5BA4 != 0, f, full);
        if (nf == f) return;
        in[0] = nf;
        g_lwScaleNow = LwStepScale(rate, stockStep);
        {
            LtRoom *const r = &g_ltRoom;
            const int item = *(const int *)(p + 0x244), locked = *(const DWORD *)(p + 0x23C) != 0;
            int i;
            ++r->lwFrames;
            r->lwLocked += locked;
            for (i = 0; i < r->lwItemN && r->lwItems[i] != item; ++i) {}
            if (i == r->lwItemN && r->lwItemN < 4) r->lwItems[r->lwItemN++] = item;
            if (!g_lwFirstSaid) {
                g_lwFirstSaid = 1;
                Log("omni: lock-on walk: the first ready-stance step below a full push this session (weapon 0x%02X, %s, move amount "
                    "%.2f raised to %.2f at %d Hz, %s gates, step x%.2f)", item & 0xFF, locked ? "locked on" : "no target", f, nf, rate,
                    stockGate ? "the game's" : "Playable Eileen's", g_lwScaleNow);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_lwFault, 1);
        g_lwScaleNow = 1.0f;
        Log("omni: lock-on walk: faulted (code %08lX): the game's ready-stance steps for this session", GetExceptionCode());
        LogAnomaly("omni", "the Lock-On Walk hook faulted: the game's ready-stance steps for this session");
    }
}
static void __cdecl LwAfter(float *out)
{
    const float s = g_lwScaleNow;
    const float *p;
    if (s == 1.0f) return;
    g_lwScaleNow = 1.0f;
    p = *(const float *volatile *)0xFD5BA0;  // the player: pos x +0x40, z +0x48 (what 0x534620 added the step to)
    if (!p) return;
    out[0] = p[0x40 / 4] + (out[0] - p[0x40 / 4]) * s;
    out[2] = p[0x48 / 4] + (out[2] - p[0x48 / 4]) * s;
}
static __declspec(naked) void hkLwStep(void)  // call from 0x534C23: esi = the input block, edx = the wanted position
{
    __asm {
        push edx
        push esi
        call LwBefore
        add esp, 4
        mov edx, [esp]
        mov eax, 0x534620
        call eax
        mov edx, [esp]
        push edx
        call LwAfter
        add esp, 4
        pop edx
        ret
    }
}

// ---------------------------------------------------------------- the Lock-On Walk row (Gameplay > Combat) and the ini
enum { LWM_L, LWM_D0, LWM_D1, LWM_NA, LWM_N };
typedef char LwTextsFit[LWM_N <= 16 ? 1 : -1];  // MSG_V59 + 128 .. 143 (BRIEF_COMMON: w9's ids)
static BYTE g_txtLw[LWM_N][64];
static const BYTE *LwText(int id)
{
    const int i = id - (MSG_V59 + 128);
    return g_txtLw[i >= 0 && i < LWM_N ? i : LWM_NA];
}
static void LwTexts(void)
{
    Encode(g_txtLw[LWM_L], sizeof g_txtLw[0], OLW_COMBAT_LOCK_ON_WALK_L);
    Encode(g_txtLw[LWM_D0], sizeof g_txtLw[0], OLW_COMBAT_LOCK_ON_WALK_VD_OFF);
    Encode(g_txtLw[LWM_D1], sizeof g_txtLw[0], OLW_COMBAT_LOCK_ON_WALK_VD_ON);
    Encode(g_txtLw[LWM_NA], sizeof g_txtLw[0], OLW_NA_GAME);
}
static void LwSetLocked(int v, const char *from)  // under GpLock
{
    const int was = (int)g_lwMode;
    if (v < 0 || v >= LW_MODES) return;
    InterlockedExchange(&g_lwMode, v);
    g_lwUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "LockOnWalk", kLwName[v], g_iniPath)) {
        g_lwUnsaved = 1;
        Log("omni: LockOnWalk %s (was %s; %s) applies now but could not be saved in SH4Refined.ini (error %lu)", kLwName[v],
            kLwName[was == LW_OFF ? 0 : 1], from, GetLastError());
    } else {
        Log("omni: LockOnWalk %s (was %s; %s)%s", kLwName[v], kLwName[was == LW_OFF ? 0 : 1], from,
            g_lwReady ? "" : " (its hook is not in: no effect)");
    }
}
static int __cdecl LwIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    if (!g_lwReady) {
        ((Fn1)0x56CC40)(0x9C44);  // the buzzer: nothing to change
        return 0;
    }
    GpLock();
    v = StepValue((int)g_lwMode == LW_OFF ? 0 : 1, LW_MODES);
    if (v >= 0) LwSetLocked(v, "Options");
    GpUnlock();
    return 0;
}
static int __cdecl LwGet(void) { return g_lwReady && g_lwMode != LW_OFF ? 1 : 0; }
static int __cdecl LwDesc(void) { return MSG_V59 + 128 + (!g_lwReady ? LWM_NA : g_lwMode != LW_OFF ? LWM_D1 : LWM_D0); }
static void LwFillRow(DWORD *r)  // gameplay.inc GpBuildPlayer: the Player origin page (layout.inc copies it to Gameplay > Combat)
{
    g_lwIds[0] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 0;  // Off / On: One-Hit Kill's value texts (as Item Stacks' row)
    g_lwIds[1] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 1;
    FillRow(r, LwIn, LwGet, g_lwIds, LW_MODES, MSG_V59 + 128 + LWM_L, LwDesc);
}
static void LwSnapshot(void) { g_lwSnap = g_lwMode; }                          // OmSnapshot (under GpLock)
static int LwChanged(void) { return g_lwMode != g_lwSnap; }                    // gameplay.inc GpChanged
static int LwRowChanged(void) { return g_snapOn && g_lwMode != g_lwSnap; }     // gameplay.inc GpRowChanged (the Player page)
static void LwDiscard(void)                                                    // OmDiscard (under GpLock)
{
    if (g_lwMode != g_lwSnap) LwSetLocked((int)g_lwSnap, "Options: No at the exit");
}
// The ini texts (an unknown text is logged once per text; [Input] OmniLatch is hidden, [Gameplay] LockOnWalk is the row's)
static int LtReadLatch(const char *buf)
{
    static char s_last[32] = "\x01";
    int known;
    const int v = LtParse(buf, &known);
    if (!known && strcmp(buf, s_last) && LogQuota(&g_ltQBad, 8)) Log("omni: [Input] OmniLatch=%s is not 0 or 1: 1 (Refined's latch rule)", buf);
    strcpy_s(s_last, sizeof s_last, buf);
    return v;
}
static int LwReadKey(const char *buf)
{
    static char s_last[32] = "\x01";
    int known;
    const int v = LwParse(buf, &known);
    if (!known && strcmp(buf, s_last) && LogQuota(&g_ltQBad, 8)) Log("omni: [Gameplay] LockOnWalk=%s is not Off or On: On", buf);
    strcpy_s(s_last, sizeof s_last, buf);
    return v;
}
static void LtReadIni(void)  // OmReadIni
{
    char buf[32];
    GetPrivateProfileStringA("Input", "OmniLatch", "", buf, sizeof buf, g_iniPath);  // v3.59 MERGE: hidden, not in kIniKeys (IniStr gave "")
    g_ltMode = LtReadLatch(buf);
    IniStr("Gameplay", "LockOnWalk", buf, sizeof buf);
    g_lwMode = LwReadKey(buf);
    g_lwSnap = g_lwMode;
}
static void LtIniReread(void)  // OmIniReread (under GpLock)
{
    char buf[32];
    int v;
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Input", "OmniLatch", "", buf, sizeof buf, g_iniPath);
    v = LtReadLatch(buf);
    if (v != (int)g_ltMode) {
        Log("omni: [Input] OmniLatch read again: %s (was %s)", kLtModeName[v], kLtModeName[g_ltMode == LT_STOCK ? 0 : 1]);
        InterlockedExchange(&g_ltMode, v);
    }
    if (g_lwUnsaved) return;  // the menu's value stands until it is saved or set again
    GetPrivateProfileStringA("Gameplay", "LockOnWalk", kLwName[g_lwMode == LW_OFF ? 0 : 1], buf, sizeof buf, g_iniPath);
    v = LwReadKey(buf);
    if (v == (g_lwMode == LW_OFF ? LW_OFF : LW_ON)) return;
    Log("omni: [Gameplay] LockOnWalk read again: %s (was %s)", kLwName[v], kLwName[g_lwMode == LW_OFF ? 0 : 1]);
    InterlockedExchange(&g_lwMode, v);
    g_lwSnap = v;  // an outside edit is the new snapshot: "No" at the exit never undoes it
}
static void LtInstall(void)  // OmInstall
{
    static const Site kLatch = { 0x502789, { 0x50, 0xBF, 0x70, 0x1B, 0x0A, 0x01, 0xBE, 0xC0, 0x5A, 0xFD, 0x00, 0xE8, 0xA7, 0xF8,
                                             0xFF, 0xFF }, 16 };  // push eax; mov edi, 10A1B70; mov esi, FD5AC0; call 502040
    static const Site kStep = { 0x534C1D, { 0x8D, 0x54, 0x24, 0x20, 0x8B, 0xF3, 0xE8, 0xF8, 0xF9, 0xFF, 0xFF }, 11 };  // lea edx, [esp+20]; mov esi, ebx; call 534620
    GpPatch one;
    LwTexts();
    if (!SiteOk(&kLatch)) {
        Log("omni: latch: 00502794 (call 00502040) is not the stock code (another mod?): the game's control-direction latch");
    } else {
        GpBranch(&one, 0x502794, 0xE8, hkLtLatch, 5);
        g_ltReady = GpApplyAll(&one, 1, "omni: latch");
    }
    Log("omni: latch: %s (00502794: the control-direction latch 00502040); [Input] OmniLatch %s (%s)", g_ltReady ? "in" : "OFF",
        kLtModeName[g_ltMode == LT_STOCK ? 0 : 1], g_ltMode == LT_STOCK ? "the game's latch; its episodes logged"
        : "with Omni-Directional Movement On or the Shoulder camera's view: let go at 15 degrees of the stick, the rig's look rate "
          "ignored under the Shoulder camera");
    if (!SiteOk(&kStep)) {
        Log("omni: lock-on walk: 00534C23 (call 00534620) is not the stock code (another mod?): the game's ready-stance steps");
    } else {
        GpBranch(&one, 0x534C23, 0xE8, hkLwStep, 5);
        g_lwReady = GpApplyAll(&one, 1, "omni: lock-on walk");
    }
    Log("omni: lock-on walk: %s (00534C23: the ready-stance step 00534620); [Gameplay] LockOnWalk %s (gates %s, step operand %s)",
        g_lwReady ? "in" : "OFF", kLwName[g_lwMode == LW_OFF ? 0 : 1],
        *(volatile const DWORD *)0x534630 == 0x5B7520 ? "the game's 0.9" : "another's (Playable Eileen's rate gate)",
        *(volatile const DWORD *)0x53467A == 0x5C5BF4 ? "the game's 9.1" : "another's");
    if (!g_lwReady && g_lwMode != LW_OFF)
        LogAnomaly("omni", "[Gameplay] LockOnWalk is On but its hook is not in: the game's ready-stance steps");
}
