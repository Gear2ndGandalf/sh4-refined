// ================================================================ v3.59 FR359: cut-scenes and Room 302's views at the gameplay rate
// USER_FEEDBACK_v358: "Expand 60fps to the window panes in room 302. Expand 60fps to the cut-scenes. Add both 120fps and
// 180fps to the aforementioned areas if you could." Since v2.1 the Governor held every real-time cut-scene (game phase
// [0xFD5C20] == 1) at 30, at the user's wish then; Room 302's window look-out, the peepholes, the door scope and the
// wall hole are phase 1 too (0x4CCBF0 from their event handlers), so they ran at 30 as well. Research (FrameMode 0, the
// true rate; staging\v359_feedback\w8_rate\research_cutscenes.md / research_window_view.md, static):
// * A cut-scene's stream holds ONE block per 30 Hz frame; the stream frame advances by [0x12E7C50] = 30 / fps a tick
//   (v3.2: live, 0x560A6E) and DemoTrackPlayerUpdate 0x560860 poses the actors, the camera and the lights by blending
//   two blocks with t = the fractional frame: at 30 t is always 0, at N the game interpolates by itself (models, the
//   camera, lip-sync: the mouths are float channels of the stream). Speech, subtitles and the scene's effects are keyed
//   to stream frames by crossing tests (0x5070F0 / 0x507140); the skip fade and the fades count against fps.
// * What breaks at N: (1) a camera cut between two blocks is blended over the in-between pictures (1 at 60, 5 at 180):
//   here a pair of blocks whose camera jumps (or an actor put elsewhere) is shown as its first block until the integer
//   frame moves on, the stock moment of the cut (hook 0x5609FB, after t is computed; cutrate_sim.h CrJudgeBlocks);
//   (2) nothing ties the stream to the voice: a late frame at a high rate (the user's logs: 25-67 % of frames late at
//   180) would put the pictures behind the speech, and 30 / 180 in floats drifts too: in phase 1 the stream frame now
//   follows real time at 30 frames a second (CrClockStep, at the v3.2 hook 0x560A6E); (3) the window view's own tasks
//   (cars, birds, two rare models, lamps, the mouse) count ticks: 48 per-tick constants (wv359_table.h) and 6 caves.
// * Hidden keys [Window] CutsceneRate (1: story cut-scenes at the gameplay rate; 0: at 30 as v2.1 - v3.58) and ViewRate
//   (1: Room 302's window, peephole, door-scope and hole views at the gameplay rate; 0: at 30); diagnostics
//   CutsceneCutSnap / CutsceneClock (1; 0 turns that part off, and the scenes that need it stay at 30).
// * FMV (pre-rendered movies) are a blocking loop with their own clock: unchanged. FrameMode 1 (interpolation) is
//   unchanged (its own SmoothCutscenes). Not done (shared with gameplay, where they already run at N today): the film
//   grain's chase / flicker / scratches (0x432FF4.., 0x565A1B, 0x566D74.., 0x5680BC..), the candle flipbook 0x5461E1.
// Installed from TailInstall (the first frame end, FrameMode 0 only), every site byte-checked, all or none per part.
#include "cutrate_sim.h"
#include "wv359_table.h"
static int g_crCutIni = 1, g_crViewIni = 1, g_crSnapIni = 1, g_crClockIni = 1;  // the hidden keys
static int g_crInstalled, g_crSnapOk, g_crClockOk, g_wvOk, g_crStreamHooked;
static int g_crKind;                    // the phase-1 kind the Governor last judged (CR_SCENE / CR_WINDOW / CR_PEEP)
static CrClock g_crClock;
static LARGE_INTEGER g_crQf, g_crLast;
static const BYTE *g_crCur, *g_crNext;  // the block pair last judged
static short g_crCurF, g_crNextF;
static int g_crMask;
static float g_crPrevCam;
static volatile LONG g_crCutFrame;      // a held cut began since the last frame end (the rate guard skips that frame)
static int g_crCuts, g_crCutsCam, g_crCutsObj;
static volatile LONG g_crLogQ, g_crLogVQ;
static float g_wvSlot[NWV359], g_wvSlot30[NWV359];
static float g_wvK = 1.0f;              // 30 / N while the view's table holds the N values, else 1 (the caves)
static int g_wvHoldN, g_wvHold;         // the window frame's click bump: ticks it is held after the first
static const char *const kCrKindName[3] = { "cut-scene", "Room 302 window view", "Room 302 peephole / hole view" };

// The Governor (and the phase setters): 1 = this phase-1 frame runs at the gameplay rate
static int CrPhase1FastNow(void)
{
    if (!g_crInstalled || g_frameMode) return 0;
    g_crKind = CrViewKind(*(volatile DWORD *)0xFD5A90, FovViewNow(), *(volatile int *)0xFD5BE8 != 0);
    return CrPhase1Fast(g_crKind, g_crCutIni, g_crViewIni, g_crSnapOk, g_crClockOk, g_wvOk);
}

// ---- the camera cut: 0x5609FB (esi = the track player, edi = the next block, [ebp-4] = the current one, [ebp+8] = t)
static void __cdecl CrSnapC(const BYTE *player, const BYTE *cur, const BYTE *next, float *t)
{
    if (!g_crSnapOk || *(volatile int *)0x5D5248 <= 30 || !cur || !next || cur == next || !(*t > 0.0f)) return;
    __try {
        const short cf = *(const short *)(cur + 2), nf = *(const short *)(next + 2);
        if (cur != g_crCur || next != g_crNext || cf != g_crCurF || nf != g_crNextF) {
            CrJudge j;
            CrJudgeBlocks(player, cur, next, *(const int *)0x12E7C70, g_crPrevCam, &j);
            g_crCur = cur, g_crNext = next, g_crCurF = cf, g_crNextF = nf;
            g_crMask = j.mask;
            g_crPrevCam = j.camMove;
            if (j.mask) {
                ++g_crCuts;
                if (j.mask & (CR_CUT_CAM_MOVE | CR_CUT_CAM_TURN | CR_CUT_CAM_FOV)) ++g_crCutsCam;
                else ++g_crCutsObj;
                InterlockedExchange(&g_crCutFrame, 1);
                if (LogQuota(&g_crLogVQ, 120))
                    LogV("frame rate: %s frame %d -> %d: %s (the camera moved %.0f u, turned %.1f deg, its view angle %.0f %%; an actor "
                         "moved %.0f u): no in-between pictures across it", kCrKindName[g_crKind], cf, nf,
                         j.mask & 7 ? "a camera cut" : "an actor put elsewhere", j.camMove, j.camTurn, j.camFov * 100.0f, j.objMove);
            }
        }
        if (g_crMask) *t = 0.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_crMask = 0;
    }
}
static const DWORD g_crSnapBack = 0x560A01;
static __declspec(naked) void hkCrSnap(void)  // 5609FB: was `mov eax, dword ptr [esi+0D8h]` (6 bytes)
{
    __asm {
        pushad
        pushfd
        lea eax, [ebp + 8]
        push eax                  // &t
        push edi                  // next
        push dword ptr [ebp - 4]  // cur
        push esi                  // the player
        call CrSnapC
        add esp, 16
        popfd
        popad
        mov eax, dword ptr [esi + 0D8h]
        jmp dword ptr [g_crSnapBack]
    }
}

// ---- the stream clock: hkStreamStep (sh4windowmod.c, 0x560A6E; esi = the track player) asks for this tick's step
static float __cdecl CrStreamStepC(const BYTE *player)
{
    const int rate = *(volatile int *)0x5D5248;
    LARGE_INTEGER now;
    double dt;
    float frame;
    if (rate < 10 || rate > 1000) return *(volatile float *)0x12E7C50;
    if (!g_crClockOk || rate <= 30 || *(volatile int *)0x1236D4C != 6 || *(volatile int *)0xFD5C20 != 1 || !g_crQf.QuadPart) {
        CrClockBreak(&g_crClock);  // v3.59 FR359-001 (was valid = 0)
        g_crLast.QuadPart = 0;
        return 30.0f / (float)rate;  // v3.2: 30 / the rate in force (the stock value at any steady rate)
    }
    QueryPerformanceCounter(&now);
    dt = g_crLast.QuadPart ? (double)(now.QuadPart - g_crLast.QuadPart) / (double)g_crQf.QuadPart : 0.0;
    g_crLast = now;
    __try {
        frame = *(const float *)(player + 0xE0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 30.0f / (float)rate;
    }
    return CrClockStep(&g_crClock, rate, dt, frame);
}

// ---- the window view's caves (g_wvK = 30 / N, 1 at 30)
static const DWORD g_wvBack1 = 0x54D209, g_wvBack2 = 0x54D4A8, g_wvBack3 = 0x54D4F3, g_wvBack4 = 0x54DA71, g_wvBack5 = 0x54C5C2,
                   g_wvBack6 = 0x54C5DE;
static __declspec(naked) void hkWvBrake(void)  // 54D203: was `fsubr dword ptr [edi+1F8h]` (st0 = f)
{
    __asm {
        fmul dword ptr [g_wvK]
        fsubr dword ptr [edi + 1F8h]
        jmp dword ptr [g_wvBack1]
    }
}
static __declspec(naked) void hkWvSpeed(void)  // 54D4A2: was `fadd dword ptr [edi+1F4h]` (st0 = accel)
{
    __asm {
        fmul dword ptr [g_wvK]
        fadd dword ptr [edi + 1F4h]
        jmp dword ptr [g_wvBack2]
    }
}
static __declspec(naked) void hkWvMove(void)  // 54D4ED: was `fld dword ptr [edi+1F4h]` (speed, then x dir)
{
    __asm {
        fld dword ptr [edi + 1F4h]
        fmul dword ptr [g_wvK]
        jmp dword ptr [g_wvBack3]
    }
}
static __declspec(naked) void hkWvCell(void)  // 54DA6A: was `add esp,10h; inc ecx; and ecx,7`
{
    __asm {
        add esp, 10h
        add ecx, dword ptr [g_compTick]  // 1 on the 30 Hz ticks while the table holds the N values, else 0; 1 at 30
        and ecx, 7
        jmp dword ptr [g_wvBack4]
    }
}
static __declspec(naked) void hkWvBumpEnd(void)  // 54C5BD: was `mov [10A57D0h], eax` (eax = 0: no click this tick)
{
    __asm {
        cmp dword ptr [g_wvHold], 0
        jle clear
        dec dword ptr [g_wvHold]
        jmp dword ptr [g_wvBack5]
    clear:
        mov dword ptr ds:[010A57D0h], eax
        jmp dword ptr [g_wvBack5]
    }
}
static __declspec(naked) void hkWvBump(void)  // 54C5D8: was `mov [10A57D0h], esi` (esi = 1: a click)
{
    __asm {
        mov dword ptr ds:[010A57D0h], esi
        push eax
        mov eax, dword ptr [g_wvHoldN]
        mov dword ptr [g_wvHold], eax
        pop eax
        jmp dword ptr [g_wvBack6]
    }
}
static const void *const kWvCaveTo[NWV359CAVE] = { hkWvBrake, hkWvSpeed, hkWvMove, hkWvCell, hkWvBumpEnd, hkWvBump };

// TailApply / TailWrite: ri = RateIndex (0 / 1 / 2 = 60 / 120 / 180; -1 = the stock values)
static void CrWvRate(int ri)
{
    int i;
    if (!g_wvOk) return;
    for (i = 0; i < NWV359; ++i) {
        const RateTail *e = &kWv359[i];
        BYTE *p = (BYTE *)(ULONG_PTR)(e->va + e->off);
        if (e->type == RT_PTR) {
            g_wvSlot[i] = ri < 0 ? g_wvSlot30[i] : (float)e->v[ri];
        } else if (ri < 0) {
            memcpy(p, e->orig + e->off, e->type == RT_I8 ? 1 : 4);
        } else if (e->type == RT_F32) {
            const float f = (float)e->v[ri];
            memcpy(p, &f, 4);
        } else if (e->type == RT_I32) {
            const int n = (int)e->v[ri];
            memcpy(p, &n, 4);
        } else {
            *p = (BYTE)(signed char)(int)e->v[ri];
        }
    }
    g_wvK = ri < 0 ? 1.0f : ri == 0 ? 0.5f : ri == 1 ? 0.25f : 1.0f / 6.0f;
    g_wvHoldN = ri < 0 ? 0 : ri == 0 ? 1 : ri == 1 ? 3 : 5;
}

static int CrWvInstall(void)
{
    int i, bad = 0, done = 0, caves = 0;
    DWORD old;
    for (i = 0; i < NWV359; ++i)
        if (memcmp((const void *)(ULONG_PTR)kWv359[i].va, kWv359[i].orig, kWv359[i].len)) {
            ++bad;
            Log("frame rate: window view site %08lX is not stock", kWv359[i].va);
        }
    for (i = 0; i < NWV359CAVE; ++i)
        if (memcmp((const void *)(ULONG_PTR)kWv359Cave[i].va, kWv359Cave[i].orig, kWv359Cave[i].len)) {
            ++bad;
            Log("frame rate: window view site %08lX (%s) is not stock", kWv359Cave[i].va, kWv359Cave[i].what);
        }
    if (bad) return 0;
    for (i = 0; i < NWV359; ++i, ++done) {
        const RateTail *e = &kWv359[i];
        if (e->type == RT_PTR) {
            const DWORD to = (DWORD)(ULONG_PTR)&g_wvSlot[i];
            g_wvSlot30[i] = g_wvSlot[i] = **(const float *const *)(ULONG_PTR)(e->va + e->off);
            if (!Poke(e->va + e->off, &to, 4)) break;
        } else {  // written again at every switch: the page stays writable
            if (!VirtualProtect((void *)(ULONG_PTR)e->va, e->len, PAGE_EXECUTE_READWRITE, &old)) break;
            if (e->va < 0x5B7000) ListPatch(e->va, e->len);
        }
    }
    if (done == NWV359)
        for (caves = 0; caves < NWV359CAVE; ++caves)
            if (!PokeBranch(kWv359Cave[caves].va, 0xE9, kWvCaveTo[caves], kWv359Cave[caves].len)) break;
    if (done < NWV359 || caves < NWV359CAVE) {  // all or none: the written ones back to their stock bytes
        for (i = 0; i < caves; ++i) RateWrite(kWv359Cave[i].va, kWv359Cave[i].orig, kWv359Cave[i].len);
        for (i = 0; i < done; ++i)
            if (kWv359[i].type == RT_PTR) RateWrite(kWv359[i].va, kWv359[i].orig, kWv359[i].len);
        Log("frame rate: a window view site could not be written (%d of %d, %d of %d caves): all put back; Room 302's window view stays at 30",
            done, NWV359, caves, NWV359CAVE);
        return 0;
    }
    return 1;
}

// TailInstall (the first frame end, FrameMode 0), after RopeStreamInstall (the stream step hook)
static void CrInstall(void)
{
    static const Site kSnap = { 0x5609FB, { 0x8B, 0x86, 0xD8, 0x00, 0x00, 0x00 }, 6 };
    g_crCutIni = GetPrivateProfileIntA("Window", "CutsceneRate", 1, g_iniPath) != 0;
    g_crViewIni = GetPrivateProfileIntA("Window", "ViewRate", 1, g_iniPath) != 0;
    g_crSnapIni = GetPrivateProfileIntA("Window", "CutsceneCutSnap", 1, g_iniPath) != 0;
    g_crClockIni = GetPrivateProfileIntA("Window", "CutsceneClock", 1, g_iniPath) != 0;
    QueryPerformanceFrequency(&g_crQf);
    if (!g_crCutIni && !g_crViewIni) {
        Log("frame rate: cut-scenes and Room 302's views at 30 (hidden [Window] CutsceneRate=0, ViewRate=0)");
        return;
    }
    if (g_crSnapIni) {
        if (SiteOk(&kSnap) && PokeBranch(0x5609FB, 0xE9, hkCrSnap, 6)) g_crSnapOk = 1;
        else Log("frame rate: 005609FB is not stock: cut-scenes stay at 30 (their camera cuts would be blended)");
    }
    g_crClockOk = g_crClockIni && g_crStreamHooked && g_crQf.QuadPart != 0;
    if (g_crClockIni && !g_crClockOk) Log("frame rate: the stream step hook is not in: cut-scenes stay at 30 (their pictures could fall behind the voice)");
    if (g_crViewIni) g_wvOk = CrWvInstall();
    g_crInstalled = 1;
    Log("frame rate: cut-scenes %s; Room 302's window view %s, its peephole / door-scope / hole views %s (hidden [Window] CutsceneRate / "
        "ViewRate; v3.59: the stream frame follows real time%s, camera cuts are shown at their stock moment%s; %d window view sites "
        "and %d caves)",
        CrPhase1Fast(CR_SCENE, g_crCutIni, g_crViewIni, g_crSnapOk, g_crClockOk, g_wvOk) ? "at the gameplay rate" : "at 30",
        CrPhase1Fast(CR_WINDOW, g_crCutIni, g_crViewIni, g_crSnapOk, g_crClockOk, g_wvOk) ? "at the gameplay rate" : "at 30",
        CrPhase1Fast(CR_PEEP, g_crCutIni, g_crViewIni, g_crSnapOk, g_crClockOk, g_wvOk) ? "at the gameplay rate" : "at 30",
        g_crClockOk ? "" : " (CutsceneClock=0: off)", g_crSnapOk ? "" : " (CutsceneCutSnap=0: off)", g_wvOk ? NWV359 : 0,
        g_wvOk ? NWV359CAVE : 0);
}

// FrameEndC, after the Governor: one line when a phase-1 stretch starts at the gameplay rate and one when it ends
static void CrFrameEnd(void)
{
    static int s_in, s_fast, s_kind;
    static DWORD s_t0, s_id;
    const int in1 = *(volatile int *)0x1236D4C == 6 && *(volatile int *)0xFD5C20 == 1;
    const DWORD id = *(volatile DWORD *)0x10774B8;  // gCutscene.id (v3.59 FR359-001)
    if (!g_crInstalled) return;
    // v3.59 FR359-001: the known breaks of a playing scene restart the stream clock (no catch-up of that time): the game
    // out of front (Alt+Tab), a quick load, an in-game menu over it, a scene that is not playing (state 3) or another
    // scene id. The pause and the other overlays are phase 2 and end the stretch (the reset below at its next start).
    // Any other slow frame or hitch is caught up by CrClockStep (bounded per tick).
    if (in1 && (!InFront() || g_qsLoading || *(volatile int *)0x10774B4 != 3 || (s_in && id != s_id) ||
                (*(volatile int *)0x1236D50 == 2 && *(volatile int *)0x1236D54 == 2)))
        CrClockBreak(&g_crClock);
    s_id = id;
    if (in1 && !s_in) {
        s_in = 1;
        s_kind = g_crKind;
        s_fast = g_rateNow > 30;
        s_t0 = GetTickCount();
        g_crCuts = g_crCutsCam = g_crCutsObj = 0;
        g_crPrevCam = 0.0f;
        g_crCur = g_crNext = NULL;
        CrClockReset(&g_crClock);
        if (s_fast && LogQuota(&g_crLogQ, 80))
            Log("frame rate: %d in a %s (scene %X; v3.59 [Window] %s=1)", g_rateNow, kCrKindName[s_kind],
                *(volatile DWORD *)0x10774B8, s_kind == CR_SCENE ? "CutsceneRate" : "ViewRate");
    } else if (!in1 && s_in) {
        s_in = 0;
        if (s_fast && LogQuota(&g_crLogQ, 80))
            Log("frame rate: the %s at %d ended after %.1f s: %d camera cut(s) and %d actor jump(s) shown at their stock moment; "
                "the stream %+.0f ms against real time", kCrKindName[s_kind], g_rateNow, (GetTickCount() - s_t0) / 1000.0f,
                g_crCutsCam, g_crCutsObj, CrClockDriftMs(&g_crClock));
    }
}
