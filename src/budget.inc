// ================================================================ v3.35 FX335: the effect budget and the frame ledger
// (staging/v335_feedback/w2_fx/PLAN.md, DESIGN.md). Included by sh4windowmod.c before items.inc (the scene pass's parts
// are timed there), so every per-frame part of Refined can name itself in the ledger. Game thread only (the HUD call
// hook 0x4EFBFD and the tick dispatcher 0x4EF982 both run on it).
//  * The ledger: BgPush / BgPop around each part (exclusive times: a nested part is not counted in its parent), taken
//    once a frame by loghealth.inc (BgFrameTake) for the room lines' worst-frame context and the freeze lines.
//  * The governor (budget_sim.h BgGovStep): one level for Echo Doubles, After Shock and Death Presence, fed with the
//    frame's length and the effects' own draw work; the effects read BgLevel() and fade what a level thins.
//  * Hidden ini keys: [Graphics] EffectBudget=1 (0 = v3.34: no governor, Echo's per-room budget level and After Shock's
//    copy ladder as before), [Graphics] EffectBudgetMs=6 (the effects' share of a 60 fps frame, 2 .. 12; scaled by the
//    frame target: 12 ms at 30 fps).
// v3.56 PW356 (staging/v356_feedback/w5_powers/PLAN.md): the high frame-rate fallback (budget_sim.h: at 120 fps and up
// the late rule 1.06 x, the spark levels BGL_SPARK1 / 2, blind steps judged by the frame time) and the go-ahead for the
// frame-rate guard (BgRateMayStepNow). Hidden [Graphics] EffectBudgetHighRate=1 (0 = v3.55's rules at every rate).
#include "budget_sim.h"

static LARGE_INTEGER g_bgQf;
static LONGLONG g_bgT0, g_bgAcc[BGC_N];
static int g_bgStk[8], g_bgSp;
static void BgPush(int slot)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (g_bgSp > 0 && g_bgSp <= 8) g_bgAcc[g_bgStk[g_bgSp - 1]] += now.QuadPart - g_bgT0;
    if (g_bgSp < 8) g_bgStk[g_bgSp] = slot < 0 || slot >= BGC_N ? BGC_T_OTHER : slot;
    ++g_bgSp;
    g_bgT0 = now.QuadPart;
}
static void BgPop(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (g_bgSp > 0 && g_bgSp <= 8) g_bgAcc[g_bgStk[g_bgSp - 1]] += now.QuadPart - g_bgT0;
    if (g_bgSp > 0) --g_bgSp;
    g_bgT0 = now.QuadPart;
}
#define BG_RUN(slot, call) do { BgPush(slot); call; BgPop(); } while (0)
// this frame's parts in ms (and the ledger starts again); called once a frame, outside every part
static void BgFrameTake(float ms[BGC_N])
{
    int i;
    if (!g_bgQf.QuadPart) QueryPerformanceFrequency(&g_bgQf);
    for (i = 0; i < BGC_N; ++i) {
        ms[i] = g_bgQf.QuadPart ? (float)((double)g_bgAcc[i] * 1000.0 / (double)g_bgQf.QuadPart) : 0.0f;
        g_bgAcc[i] = 0;
    }
    if (g_bgSp) {  // a part left open (a fault skipped its pop): the stack starts clean
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("budget", "%d frame part(s) left open at the frame's end (the innermost %s): the ledger starts clean",
                                          g_bgSp, kBgName[g_bgStk[(g_bgSp <= 8 ? g_bgSp : 8) - 1]]);
        g_bgSp = 0;
    }
}

// ---------------------------------------------------------------- the governor (runtime)
static int g_bgOn = 1, g_bgIniRead;
static float g_bgT60 = BG_T60_MS;
static BgGov g_bgGov;
static volatile LONG g_bgLevel;          // what the effects read (0 with the governor off)
static LONG g_bgFrameNo;                 // +1 a frame (BgFrame): the effects' once-a-frame fades
static float g_bgDt;                     // this frame's length (s), for the effects' fades (0 on a skipped frame)
// the room's numbers (BgRoomLog)
static int g_bgRoomMax, g_bgRoomDowns, g_bgRoomUps;
static unsigned g_bgRoomN, g_bgRoomThin, g_bgRoomLate;
static double g_bgRoomCost;
static float g_bgRoomCostMax;
static int BgLevel(void) { return g_bgOn ? (int)g_bgLevel : 0; }
// v3.56 PW356: the high-rate fallback's state for the effects and the log
static int g_bgHrOn = 1;                 // hidden [Graphics] EffectBudgetHighRate (0 = v3.55's rules at every rate)
static float g_bgSparkK = 1.0f;          // the sparks, arcs and smoke kept (BgSparkK of the level, faded over 0.5 s)
static int g_bgRoomHr, g_bgRoomBlind, g_bgRoomKept, g_bgRoomBack;  // the room's high-rate frames seen, blind steps, verdicts
static float BgSparkNow(void) { return g_bgOn ? g_bgSparkK : 1.0f; }
// Echo's light look by the spark levels: the 2nd / 3rd shadow from 60 %, every shadow at 35 % (v3.31's light look)
static int BgSparkLite(int shadow) { const float k = BgSparkNow(); return k < 0.5f || (k < 0.8f && shadow > 0); }
// the frame-rate guard (w7 FR356) may lower the logic rate: frames have missed at a high rate for 1 s with nothing left
// for the budget to shed (or the budget is off)
static int BgRateMayStepNow(void) { return !g_bgOn || BgRateMayStep(&g_bgGov); }
static void BgIni(void)
{
    char buf[32];
    if (g_bgIniRead) return;
    g_bgIniRead = 1;
    BgGovReset(&g_bgGov);
    g_bgOn = GetPrivateProfileIntA("Graphics", "EffectBudget", 1, g_iniPath) != 0;
    g_bgHrOn = GetPrivateProfileIntA("Graphics", "EffectBudgetHighRate", 1, g_iniPath) != 0;  // v3.56 PW356
    GetPrivateProfileStringA("Graphics", "EffectBudgetMs", "", buf, sizeof buf, g_iniPath);
    if (buf[0]) {
        const float v = (float)atof(buf);
        g_bgT60 = v >= 2.0f && v <= 12.0f ? v : v > 12.0f ? 12.0f : v > 0.0f ? 2.0f : BG_T60_MS;
    }
    if (g_bgOn)
        Log("budget: in (v3.35: one effect budget for Echo Doubles, After Shock and Death Presence; their draw work's target %.1f ms of a "
            "60 fps frame, %.1f at 30 fps; a level down after %.1f s of late frames (%.0f %% over %.2f x the frame) while they cost %.0f %% "
            "of it or more, back up after %.0f s calm (to %.0f s when a step up is taken back); %d levels, each fades what it thins; "
            "[Graphics] EffectBudget=0 = v3.34's own guards)", g_bgT60, BgTargetMs(g_bgT60, 1000.0f / 30.0f), BG_DOWN_S,
            100.0f * BG_LATE_ON, BG_LATE_K, 100.0f * BG_COST_ON, BG_UP_S, BG_UP_MAX, BG_MAX);
    if (g_bgOn && g_bgHrOn)  // v3.56 PW356
        Log("budget: high rate fallback in (v3.56: at %.0f fps and up a frame over %.2f x the target is late; the effects' work counts from "
            "%.0f %% of their target; first the sparks, arcs and smoke at %.0f / %.0f %%; a late run while the powers draw but cost little "
            "CPU takes a blind step, kept only if the frames get %.0f %% shorter in %.1f s, else taken back and not tried for %.0f s (to %.0f); "
            "[Graphics] EffectBudgetHighRate=0 = v3.55's rules)", 1000.0f / BG_HR_FRAME_MS, BG_HR_LATE_K, 100.0f * BG_HR_COST_ON,
            100.0f * BgSparkK(BGL_SPARK1, 1), 100.0f * BgSparkK(BGL_SPARK2, 1), 100.0f * BG_HR_GAIN, BG_SETTLE_S, BG_HR_RETRY, BG_HR_RETRY_MAX);
    else if (g_bgOn)
        Log("budget: high rate fallback off ([Graphics] EffectBudgetHighRate=0): v3.55's rules at every frame rate");
    if (!g_bgOn)
        Log("budget: off ([Graphics] EffectBudget=0): v3.34's behaviour (Echo's budget level for the room, After Shock's copy ladder)");
}
// the levels that change something with the current settings (bit L); v3.56 PW356: + the spark levels at a high rate
// (After Shock at any trail length, Death Presence, Echo: each has sparks or smoke to thin)
static unsigned BgUseful(int hr)
{
    const int echo = g_echoReady && g_echoMode > 0, n = (int)g_echoCountV + 1;
    const int ai = g_aiReady && g_aiLevel >= 2, dp = g_dpReady && g_dpLevel > 0;  // (After Shock Short has 3 copies: nothing to thin)
    unsigned u = 1u;
    if (hr && (dp || (g_aiReady && g_aiLevel >= 1) || (echo && n >= 2))) u |= 1u << BGL_SPARK1;  // v3.56 PW356 (Echo: the 2nd / 3rd's light look)
    if (hr && (dp || (g_aiReady && g_aiLevel >= 1) || echo)) u |= 1u << BGL_SPARK2;  // v3.56 PW356 (Echo: every shadow's light look)
    if (echo && n >= 3) u |= 1u << BGL_ECHO3;
    if (echo && n >= 2) u |= 1u << BGL_ECHOWEP | 1u << BGL_ECHOLITE | 1u << BGL_ECHO2;
    if (ai) u |= 1u << BGL_AI5;
    if (echo) u |= 1u << BGL_ECHOALL | 1u << BGL_ECHO1;
    if (ai || dp) u |= 1u << BGL_AI3DP;
    if (dp) u |= 1u << BGL_DP50;
    return u;
}
// One frame (loghealth.inc LhFrameEndC): its length, the game's frame target, the effects' draw work, a frame that must
// not count (skip), and whether it is a play frame at all (not paused / cut-scene: nothing is stepped then).
static void BgFrame(float frameMs, float targetMs, float effMs, int skip, int play, DWORD room)
{
    static volatile LONG s_q, s_qHr, s_qV, s_qY;  // v3.56 PW356: + the high-rate lines' quotas
    static int s_hrSaid = -1, s_yieldSaid;
    int step, before, hr;
    BgIni();
    ++g_bgFrameNo;
    g_bgDt = 0.0f;
    if (!g_bgOn) {
        InterlockedExchange(&g_bgLevel, 0);
        return;
    }
    if (!play) return;
    before = g_bgGov.level;
    // v3.56 PW356: the high-rate rules (budget_sim.h) only with EffectBudgetHighRate=1 (g_bgGov.hrOff: every rule v3.55's)
    hr = g_bgHrOn && BgHighRate(targetMs);
    g_bgGov.hrOff = !g_bgHrOn;
    step = BgGovStep(&g_bgGov, frameMs / 1000.0f, frameMs, effMs, targetMs, g_bgT60, BgUseful(hr), skip);
    if (!skip && targetMs > 1.0f && hr != s_hrSaid) {  // v3.56 PW356: the rules in force (a rate change)
        if ((s_hrSaid >= 0 || hr) && LogQuota(&s_qHr, 40))
            LogV("budget: %.0f fps: %s", 1000.0f / targetMs, hr ? "the high-rate rules (late from 1.06 x the frame, the spark levels, blind steps)"
                                                               : "v3.55's rules (late from 1.25 x the frame, no spark level, no blind step)");
        s_hrSaid = hr;
    }
    if (!skip && frameMs > 0.0f && frameMs < BG_SPIKE_MS) g_bgRoomHr += hr;  // v3.56 PW356
    if (step > 0 && g_bgGov.lastBlind) ++g_bgRoomBlind;
    if (g_bgGov.verdict == 1) {  // v3.56 PW356: a blind step's verdict
        ++g_bgRoomKept;
        if (LogQuota(&s_qV, 300))
            LogV("budget: the blind step to level %d is kept: the frames %.0f %% shorter (%.2f ms a frame now, target %.2f; stage %lX room %lX)",
                 g_bgGov.level, 100.0f * g_bgGov.blindGain, g_bgGov.frameE, targetMs, room >> 16, room & 0xFFFF);
    }
    if (g_bgGov.verdict == 2) ++g_bgRoomBack;
    if (hr) {  // v3.56 PW356: the frame-rate guard's go-ahead, said each time it turns on (bounded)
        const int y = BgRateMayStep(&g_bgGov);
        if (y && !s_yieldSaid && LogQuota(&s_qY, 100))
            LogV("budget: nothing left to shed at %.0f fps (level %d, %s; %.0f %% of frames late, the effects %.2f ms a frame; blind "
                 "tries %s): the frame-rate guard may step the rate down", 1000.0f / targetMs, g_bgGov.level, kBgLevelText[g_bgGov.level],
                 100.0f * g_bgGov.lateE, g_bgGov.costE, g_bgGov.blindHold > 0.0f ? "on hold" : "possible");
        if (y) s_yieldSaid = 1;
    }
    if (!skip && frameMs > 0.0f && frameMs < BG_SPIKE_MS) {
        g_bgDt = frameMs / 1000.0f;
        ++g_bgRoomN;
        g_bgRoomThin += g_bgGov.level > 0;
        g_bgRoomLate += frameMs > BG_LATE_K * targetMs;
        g_bgRoomCost += effMs;
        if (effMs > g_bgRoomCostMax) g_bgRoomCostMax = effMs;
    }
    InterlockedExchange(&g_bgLevel, g_bgGov.level);
    if (g_bgGov.level > g_bgRoomMax) g_bgRoomMax = g_bgGov.level;
    if (step > 0) ++g_bgRoomDowns;
    if (step < 0) ++g_bgRoomUps;
    {   // v3.56 PW356: the sparks follow the level (0.5 s fades; back to full below a high rate)
        g_bgSparkK = BgRamp(g_bgSparkK, BgSparkK(g_bgGov.level, hr), g_bgDt, 2.0f);
        if (!BgRateMayStep(&g_bgGov)) s_yieldSaid = 0;
    }
    if (step && LogQuota(&s_q, 600)) {
        char why[96];  // v3.56 PW356: a blind step and a take-back say so
        if (step > 0 && g_bgGov.lastBlind) strcpy_s(why, sizeof why, "frames run over, no CPU evidence: a blind try");
        else if (step < 0 && g_bgGov.verdict == 2)
            sprintf_s(why, sizeof why, "a blind try taken back: the frames %.1f %% shorter, %.0f %% needed", 100.0f * g_bgGov.blindGain, 100.0f * BG_HR_GAIN);
        else strcpy_s(why, sizeof why, step > 0 ? "frames run over" : "calm again");
        LogV("budget: level %d -> %d (%s; %.0f %% of frames late, the effects %.1f ms a frame against %.1f; stage %lX room %lX): %s%s",
             before, g_bgGov.level, why, 100.0f * g_bgGov.lateE, g_bgGov.costE,
             BgTargetMs(g_bgT60, targetMs), room >> 16, room & 0xFFFF, kBgLevelText[step > 0 ? g_bgGov.level : before],
             step > 0 ? "" : " comes back");
    }
}
// v3.56 PW356: the room line's high-rate part ("" when the room had no high-rate frame; no parentheses: logcheck.py)
static const char *BgRoomHrText(void)
{
    static char s[128];
    s[0] = 0;
    if (g_bgRoomHr)
        sprintf_s(s, sizeof s, "; high rate: %u frame(s), %d blind step(s), %d kept, %d taken back; the sparks at %.0f %%", (unsigned)g_bgRoomHr,
                  g_bgRoomBlind, g_bgRoomKept, g_bgRoomBack, 100.0f * g_bgSparkK);
    return s;
}
// the room's line (loghealth.inc LhRoomEnd), then the room's numbers start again (the level itself carries on)
static void BgRoomLog(int stage, int room, float targetMs)
{
    if (g_bgOn && g_bgRoomN)
        LogV("budget: room %X:%02X: level %d now (%s), at most %d this room; %d step(s) down, %d up; %.0f %% of its play frames thinned; the "
             "effects %.1f ms a frame on average, %.1f at most (target %.1f); %.1f %% of the frames late%s", stage, room, g_bgGov.level,
             kBgLevelText[g_bgGov.level], g_bgRoomMax, g_bgRoomDowns, g_bgRoomUps, 100.0 * g_bgRoomThin / g_bgRoomN,
             g_bgRoomCost / g_bgRoomN, g_bgRoomCostMax, BgTargetMs(g_bgT60, targetMs), 100.0 * g_bgRoomLate / g_bgRoomN,
             BgRoomHrText());  // v3.56 PW356: + the high-rate part
    g_bgRoomHr = g_bgRoomBlind = g_bgRoomKept = g_bgRoomBack = 0;  // v3.56 PW356
    BgGovRoomNew(&g_bgGov);  // v3.56 PW356: a new scene: a blind step may be tried again at once
    g_bgRoomMax = g_bgGov.level;
    g_bgRoomDowns = g_bgRoomUps = 0;
    g_bgRoomN = g_bgRoomThin = g_bgRoomLate = 0;
    g_bgRoomCost = 0.0;
    g_bgRoomCostMax = 0.0f;
}
