// ================================================================ v3.56 FR356: the rate guard (USER_FEEDBACK_v355 issue 6)
// "A few gameplay sessions ago the game was running slow on 180fps, but fast on 120 and lower." In FrameMode 0 (the true
// frame rate) the game advances one tick a frame, so a PC that cannot draw 180 frames a second runs the whole game in slow
// motion. The guard (rateguard_sim.h, test\rateguard_test.c) measures the play frames at the frame end (0x415202, FrameEndC:
// QueryPerformanceCounter between frame ends) and, after 2 windows of 3 s whose mean frame is over 1.15 x the target, holds
// the gameplay rate one step lower for this run (180 -> 120 -> 60): g_frgCap, read by the Governor (the same path the
// V-Sync cap and the Frame Rate row use: the rate switches at that frame's end, the per-tick tables with it) and written
// to 0x40316F's imm (Playable Eileen's GameRate() converts its seconds with it, as for the V-Sync cap). The ini's
// FrameRate and the row's value never change. Back up: after 10 minutes of clean play at the lower rate, at the next room
// change; a rate stepped down from twice in a run stays down. The effect budget (budget.inc; w5 PW356's power shedding)
// acts first: a window that misses while it is still stepping down is held (at most 3 in a row). Included after
// perfhud.inc (reads budget.inc's BgLevel, quicksave.inc's g_qsLoading, InFront). Hidden [Window] RateGuard=0: off.
// v3.58 GX358 (USER_FEEDBACK_v357 item 13): while the effect budget has nothing left to shed (its quick go-ahead,
// BgRateQuickNow: 0.5 s far late or 1 s late with nothing to take), the last second of play frames decides (the quick
// window, rateguard_sim.h FrgFrameEx): over 1.15 x the frame steps down at once. Never across a load, a pause, a capture,
// a door or the game out of front (they break the window as before). Off with [Graphics] EffectBudgetQuick=0.
#include "rateguard_sim.h"

static int g_frgOn = 1;                  // hidden [Window] RateGuard (1 = on, the default; 0 = off)
static int g_frgReady;                   // FrameMode 0 with the frame-rate hooks in (FrgInit)
static FrgState g_frg;
static LARGE_INTEGER g_frgQf, g_frgPrev;
static int g_frgPrevPlay;                // the previous frame end was a play frame
static DWORD g_frgRoomKey = 0xFFFFFFFFu;
static int g_frgPrevBg;                  // the effect budget's level at the last frame end
static DWORD g_frgShedTick;              // GetTickCount of its last step down (0 = none yet)
static int g_frgShedSeen;
static volatile LONG g_frgLogQ, g_frgLogVQ;

// The effect budget stepped down within the last window: it is still shedding, the guard waits for it. w5 PW356's spark
// levels and blind steps are budget levels (BgLevel), so the stamp sees them too. v3.56 MERGE: not while budget.inc's
// BgRateMayStepNow() says the budget has nothing left to shed (frames have missed at a high rate for 1 s with no useful
// level, no power drawing or the blind tries on hold; or the budget is off): then the guard steps without held windows.
static int FrgShedding(void)
{
    return g_frgShedSeen && GetTickCount() - g_frgShedTick < (DWORD)FRG_WIN_MS && !BgRateMayStepNow();  // v3.56 MERGE (PW356 x FR356)
}

// the effective gameplay rate for Playable Eileen's GameRate() (0x40316F's imm): the choice, the V-Sync cap, the guard
static void FrgEffWrite(void)
{
    int eff = (int)g_fpsUser;
    if (g_fpsCap && eff > g_fpsCap) eff = g_fpsCap;
    if (g_frgCap && eff > (int)g_frgCap) eff = (int)g_frgCap;
    if (g_rateHooks) RateWrite(0x40316F, &eff, 4);
}

// ApplyFrameRate (DllMain, FrameMode 0, the frame-rate hooks in): the hidden key and the start line
static void FrgInit(void)
{
    const int top = g_fpsCap && (int)g_fpsUser > g_fpsCap ? g_fpsCap : (int)g_fpsUser;
    g_frgOn = GetPrivateProfileIntA("Window", "RateGuard", 1, g_iniPath) != 0;
    if (g_frameMode || !g_rateHooks || g_rateProbe) {
        Log("rate guard: not used (%s)", g_frameMode ? "FrameMode 1: the game runs at 30" : g_rateProbe ? "RateProbe=1" : "no frame-rate hooks");
        return;
    }
    QueryPerformanceFrequency(&g_frgQf);
    FrgReset(&g_frg, top);
    g_frgReady = g_frgOn && g_frgQf.QuadPart != 0;
    if (!g_frgOn)
        Log("rate guard: off (hidden [Window] RateGuard=0): a rate this PC cannot hold runs the game in slow motion");
    else
        Log("rate guard: on (hidden [Window] RateGuard=0 turns it off): above 60 fps, when the play frames' mean over %.0f s is over "
            "%.2f x the frame in %d windows in a row (the effect budget sheds first: up to %d windows held), the game steps down "
            "180 -> 120 -> 60 for this run; back up after %.0f minutes clean at the next room change, a rate stepped down from %d "
            "times stays down; the ini's FrameRate never changes; v3.58: while the effect budget has nothing left to shed, its last "
            "%.0f s decides (top now %d)", FRG_WIN_MS / 1000.0f, FRG_MISS_K, FRG_MISS_WINS,
            FRG_HOLD_MAX, FRG_RETRY_MS / 60000.0f, FRG_LOCK_DOWNS, FRG_FAST_MS / 1000.0f, top);  // v3.58 GX358: + the quick window
}

// FpsUserChanged (the Frame Rate row, the discard, the V-Sync cap refresh): n = the choice, eff = after the V-Sync cap
static void FrgUserChanged(int n, int eff)
{
    const int wasCap = (int)g_frgCap;
    if (!g_frgReady) return;
    if (n != (int)g_fpsUser) {  // a new choice: the guard starts again
        FrgReset(&g_frg, eff);
        InterlockedExchange(&g_frgCap, 0);
        if (wasCap) Log("rate guard: the Frame Rate row chose %d: the guard starts again (it held the game at %d)", n, wasCap);
    } else {
        FrgSetTop(&g_frg, eff);
        InterlockedExchange(&g_frgCap, g_frg.cap);
        if (wasCap && !g_frg.cap) Log("rate guard: the V-Sync cap is now %d: the guard's %d is lifted", eff, wasCap);
    }
}

// FrameEndC, first (before the Governor: the frame that just ended ran at the rate in force)
static void FrgFrameEnd(void)
{
    LARGE_INTEGER now;
    float ms;
    int play, ev, lv, rate;
    DWORD key;
    if (!g_frgReady) return;
    QueryPerformanceCounter(&now);
    ms = g_frgPrev.QuadPart ? (float)((double)(now.QuadPart - g_frgPrev.QuadPart) * 1000.0 / (double)g_frgQf.QuadPart) : 0.0f;
    g_frgPrev = now;
    lv = BgLevel();
    if (lv > g_frgPrevBg) {
        g_frgShedTick = GetTickCount();
        g_frgShedSeen = 1;
    }
    g_frgPrevBg = lv;
    rate = *(volatile int *)0x5D5248;
    key = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (key != g_frgRoomKey) {  // a room change (or the first frame): a waiting step up is taken here
        const int had = g_frgRoomKey != 0xFFFFFFFFu;
        g_frgRoomKey = key;
        g_frgPrevPlay = 0;
        if (had && FrgRoomChange(&g_frg)) {
            InterlockedExchange(&g_frgCap, g_frg.cap);
            FrgEffWrite();
            if (LogQuota(&g_frgLogQ, 60))
                Log("rate guard: %d -> %d at the room change to %lX:%02lX (a try after %.0f minutes clean at %d)", g_frg.lastFrom,
                    FrgRate(&g_frg), key >> 16, key & 0xFFFF, FRG_RETRY_MS / 60000.0f, g_frg.lastFrom);
        }
        return;
    }
    {
        const int mode = *(volatile int *)0x1236D4C, phase = *(volatile int *)0xFD5C20;
        const int menu = *(volatile int *)0x1236D50 == 2 && *(volatile int *)0x1236D54 == 2;  // the Governor's overlay test
        static LONG s_pvSeen;  // v3.56 MERGE: preview.inc's pause capture drew since the last frame end: not a play interval
        const int pvCap = PvCapNew(&s_pvSeen);
        play = mode == 6 && phase == 0 && !menu && !g_qsLoading && rate == g_rateNow && !g_inGate && InFront() && !pvCap;
    }
    if (!play || !g_frgPrevPlay) {  // the interval since the last frame end is play only when both ends are
        if (!play) FrgBreak(&g_frg);
        g_frgPrevPlay = play;
        return;
    }
    ev = FrgFrameEx(&g_frg, ms, rate, FrgShedding(), BgRateQuickNow());  // v3.58 GX358: + the quick window (the budget's go-ahead)
    switch (ev) {
    case FRG_EV_DOWN:
        InterlockedExchange(&g_frgCap, g_frg.cap);
        FrgEffWrite();
        if (LogQuota(&g_frgLogQ, 60)) {
            char how[96];  // v3.58 GX358: the quick window says so
            if (g_frg.lastFast) sprintf_s(how, sizeof how, "in the last %.0f s, the effect budget with nothing left to shed", FRG_FAST_MS / 1000.0f);
            else sprintf_s(how, sizeof how, "in %d windows of %.0f s in a row", FRG_MISS_WINS, FRG_WIN_MS / 1000.0f);
            Log("rate guard: %d -> %d: the play frames took %.2f ms on average against %.2f (%.2f x) %s (stage %lX room %lX; effect "
                "budget level %d)%s; [Window] FrameRate stays %d", g_frg.lastFrom, g_frg.cap,
                g_frg.lastMean, g_frg.lastTarget, g_frg.lastTarget > 0.0f ? g_frg.lastMean / g_frg.lastTarget : 0.0f, how,
                key >> 16, key & 0xFFFF, lv,
                FrgLocked(&g_frg, g_frg.lastFrom) ? "; stepped down from it twice: not tried again this run" : "", (int)g_fpsUser);
        }
        break;
    case FRG_EV_RETRY:
        if (LogQuota(&g_frgLogQ, 60))
            Log("rate guard: %.0f minutes clean at %d (%.2f ms against %.2f): %d is tried again at the next room change",
                FRG_RETRY_MS / 60000.0f, g_frg.cap, g_frg.lastMean, g_frg.lastTarget, FrgHigher(g_frg.cap));
        break;
    case FRG_EV_MISS:
    case FRG_EV_HELD:
        if (LogQuota(&g_frgLogVQ, 200))
            LogV("rate guard: a %.0f s window at %d missed: %.2f ms on average against %.2f (%.2f x)%s", FRG_WIN_MS / 1000.0f, rate,
                 g_frg.lastMean, g_frg.lastTarget, g_frg.lastMean / g_frg.lastTarget,
                 ev == FRG_EV_HELD ? "; held: the effect budget is still shedding" : "");
        break;
    default:
        break;
    }
}
