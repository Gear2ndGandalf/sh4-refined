// ================================================================ v3.47 RG347: Refined's own Regeneration
// Plan: staging/v347_feedback/w4_regen/PLAN.md; the user's answer: staging/QUEUE.md item 6; the "who heals" contract with
// Playable Eileen: SH4PlayableEileen/staging/s59_regen/REGEN_CONTRACT.md (PE v1.23.2). Pure rules: regen_sim.h
// (test\regen_sim_test.c). Included by sh4windowmod.c after omni.inc (gameplay.inc's Player page, GpLock and HasExport
// are defined before it). All addresses are GOG v7 VAs, checked against the stock exe (md5 15a85581).
//
// No exe patch. The tick runs in Refined's 0x4EF982 dispatcher (quicksave.inc QsTick: every frame of the game loop
// 0x4EF980, in the worlds, in Room 302 and while paused). It heals through the game's PlayerHealAmount 0x52D1D0
// (cdecl `void (int slot, float amount)`, HP += amount capped at hpMax; byte-checked: 59 bytes 0x52D1D0..0x52D20A).
// Who heals (regen_sim.h RgWho, looked up by name in dsound.dll, every tick from cached pointers):
//   - no Playable Eileen, or PE with SH4PE_RegenActive() == 0 (Henry: SH4PE_Enabled() == 0; or Eileen with PE's own
//     regeneration off): SH4 Refined, at [Gameplay] Regen (Off / x1 / x1.25 / x2) after [Gameplay] RegenDelay s unhurt;
//   - SH4PE_RegenActive() == 1: Playable Eileen's own; Refined does not heal; its row is grey and shows Off;
//   - PE without SH4PE_RegenActive (before v1.23.2): v3.46's behaviour (the row is PE's ini front end; Refined does not
//     heal: an older PE's BlockOtherRegen would put Refined's rises back inside its wait).
// Its own aura check: the game's aura sums of this player frame (PlayerFrameUpdate 0x539030 -> HauntingSumAuraStrength
// 0x539920, the worlds and Room 302): [0x10A3320] (the fall-off sum, > 0 inside an aura's radius) or [0x10A2EE0] (this
// frame's aura damage) above 0 = near a ghost / haunting aura: no healing. Room 302: as PE, it heals only while the
// game's own Room 302 heal (0x52D400: stage 1 and story flag 0x51F, bit 7 of 0x13D8143, clear) does not run.
#include "regen_sim.h"

static int g_qsLoading;  // quicksave.inc (a tentative definition: the same object)

// texts: the MSG_RG0 block (16 ids): a line per value (Off: what it is), Playable Eileen's own, not available
enum { RGT_D0, RGT_PE = RGT_D0 + RG_LEVELS, RGT_NA, RGT_N };
typedef char RgTextsFit[RGT_N <= MSG_SB0 - MSG_RG0 ? 1 : -1];
static BYTE g_txtRg[RGT_N][64];
static const BYTE *RgText(int id)
{
    const int i = id - MSG_RG0;
    return g_txtRg[i >= 0 && i < RGT_N ? i : RGT_NA];
}

static volatile LONG g_rgLevel;               // [Gameplay] Regen 0..3 (Refined's own value; read by the tick)
static LONG g_rgSnap;                         // as Options opened
static int g_rgUnsaved;                       // the menu's value could not be written to the ini
static volatile LONG g_rgDelay = RG_DELAY_DEF;  // [Gameplay] RegenDelay (10..120 s)
static int g_rgReady;                         // installed: the dispatcher and a stock 0x52D1D0
// Playable Eileen (dsound.dll with SH4PE_Patches): found once, never released (PE stays loaded)
typedef int(__cdecl *RgPeFn)(void);
static HMODULE g_rgPe;
static RgPeFn g_rgPeActive, g_rgPeEnabled;    // SH4PE_RegenActive (v1.23.2), SH4PE_Enabled (v1.23.0); NULL = not exported
static DWORD g_rgPeLookAt;
// the tick (game thread)
static RgState g_rg;
static RgRun g_rgRun;
static int g_rgWho = -1, g_rgStealVia = -1;
static DWORD g_rgLastMs;
static float g_rgHealed;                      // HP Refined healed this session
static volatile LONG g_rgQWho, g_rgQRun, g_rgQAnom, g_rgQSteal;  // LogQuota counters

static void RgPeLook(int force)
{
    HMODULE ds;
    const DWORD now = GetTickCount();
    if (g_rgPe || (!force && now - g_rgPeLookAt < 1000)) return;
    g_rgPeLookAt = now;
    ds = GetModuleHandleA("dsound.dll");
    if (!ds || !HasExport(ds, "SH4PE_Patches")) return;
    g_rgPeActive = (RgPeFn)(void *)GetProcAddress(ds, "SH4PE_RegenActive");
    g_rgPeEnabled = (RgPeFn)(void *)GetProcAddress(ds, "SH4PE_Enabled");
    g_rgPe = ds;
    if (g_rgPeActive)
        Log("regeneration: Playable Eileen loaded with SH4PE_RegenActive (v1.23.2+): Refined heals Henry (SH4PE_Enabled 0) "
            "and Eileen while her own regeneration is off; while hers is in charge Refined's row is grey Off%s",
            g_rgPeEnabled ? "" : " (no SH4PE_Enabled: the player is taken as Henry)");
    else
        Log("regeneration: Playable Eileen loaded without SH4PE_RegenActive (before v1.23.2): the Regeneration row stays its "
            "ini front end ([Defense] Regen / RegenRate in SH4PlayableEileen.ini) and Refined does not heal");
}
// Playable Eileen's state now (cheap: two calls into PE reading statics)
static void RgPeState(int *pe, int *hasA, int *act, int *hasE, int *en)
{
    *pe = g_rgPe != NULL;
    *hasA = g_rgPeActive != NULL;
    *hasE = g_rgPeEnabled != NULL;
    *act = *en = 0;
    if (!*pe) return;
    __try {
        if (g_rgPeActive) *act = g_rgPeActive() != 0;
        if (g_rgPeEnabled) *en = g_rgPeEnabled() != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rgPeActive = g_rgPeEnabled = NULL;  // (never seen; the row falls back to v3.46's front end)
        if (LogQuota(&g_rgQAnom, 5)) LogAnomaly("regeneration", "calling Playable Eileen's SH4PE_RegenActive / SH4PE_Enabled faulted: not called again");
        *hasA = *hasE = 0;
    }
}
static int RgRowModeNow(void)  // RGR_OWN / RGR_PE / RGR_LEGACY (regen_sim.h)
{
    int pe, hasA, act, hasE, en;
    RgPeState(&pe, &hasA, &act, &hasE, &en);
    return RgRowMode(pe, hasA, act);
}
static int RgStealViaGameNow(void)  // playercheats.inc PcHeal: 1 = Life Steal through the game's heal path (PE's guard acts)
{
    int pe, hasA, act, hasE, en;
    RgPeLook(0);
    RgPeState(&pe, &hasA, &act, &hasE, &en);
    return RgStealViaGame(pe, hasA, act);
}

// ---------------------------------------------------------------- the texts, the ini
static void RgTexts(void)
{
    char d[64];
    const int delay = (int)g_rgDelay;
    int v;
    sprintf_s(d, sizeof d, "Heals you slowly after %d s unhurt, up to full.", delay);
    Encode(g_txtRg[RGT_D0], sizeof g_txtRg[0], d);
    for (v = 1; v < RG_LEVELS; ++v) {
        sprintf_s(d, sizeof d, "%s: %g %% of max health a minute, %d s unhurt.", kRgLevelName[v], (double)kRgRatePct[v], delay);
        Encode(g_txtRg[RGT_D0 + v], sizeof g_txtRg[0], d);
    }
    Encode(g_txtRg[RGT_PE], sizeof g_txtRg[0], "Playable Eileen's own regeneration heals her now.");
    Encode(g_txtRg[RGT_NA], sizeof g_txtRg[0], "Not available in this game (see SH4Refined.log).");
}
static int RgSeed(const char *key, const char *val, const char *why)  // v3.47 RG347 inspection 2: 1 = written
{
    if (WritePrivateProfileStringA("Gameplay", key, val, g_iniPath)) {
        Log("settings: [Gameplay] %s was missing, added as \"%s\" (%s)", key, val, why);
        return 1;
    }
    Log("settings: [Gameplay] %s was missing and could not be added (error %lu): %s for this start", key, GetLastError(), val);
    return 0;
}
// v3.47 RG347: the first start of v3.47 takes the two keys from Playable Eileen's ini when it is there (the v3.46 row
// showed PE's value as "Regeneration"); later starts read SH4Refined.ini only
static void RgReadIni(void)
{
    char pe[MAX_PATH], buf[64], a[64], b[64];
    int known, v, d, havePe;
    int keepV = -1, keepD = -1;  // v3.47 RG347 inspection 2: a seeded value whose write failed stands for this start
    sprintf_s(pe, sizeof pe, "%sSH4PlayableEileen.ini", g_dir);
    havePe = GetFileAttributesA(pe) != INVALID_FILE_ATTRIBUTES;
    if (!g_iniFallback) {
        GetPrivateProfileStringA("Gameplay", "Regen", "\x01", buf, sizeof buf, g_iniPath);
        if (!strcmp(buf, "\x01")) {
            char why[160];
            v = 0;
            if (havePe) {
                GetPrivateProfileStringA("Defense", "Regen", "\x01", a, sizeof a, pe);
                GetPrivateProfileStringA("Defense", "RegenRate", "\x01", b, sizeof b, pe);
                v = RgLevelFromPe(strcmp(a, "\x01") ? a : NULL, strcmp(b, "\x01") ? b : NULL);
                sprintf_s(why, sizeof why, "from Playable Eileen's [Defense] Regen %s / RegenRate %s%s", strcmp(a, "\x01") ? a : "(default 1)",
                          strcmp(b, "\x01") ? b : "(default 15)", v < 0 ? ": not one of x1 / x1.25 / x2, so Off" : ", as the v3.46 row showed it");
                if (v < 0) v = 0;
            } else {
                strcpy_s(why, sizeof why, "the default: no SH4PlayableEileen.ini");
            }
            if (!RgSeed("Regen", kRgLevelName[v], why)) keepV = v;
        }
        GetPrivateProfileStringA("Gameplay", "RegenDelay", "\x01", buf, sizeof buf, g_iniPath);
        if (!strcmp(buf, "\x01")) {
            char val[16], why[96];
            d = RG_DELAY_DEF;
            if (havePe && GetPrivateProfileStringA("Defense", "RegenDelay", "", a, sizeof a, pe) && a[0]) {
                d = RgClampDelay(atoi(a));
                sprintf_s(why, sizeof why, "from Playable Eileen's [Defense] RegenDelay %s", a);
            } else {
                strcpy_s(why, sizeof why, "the default, Playable Eileen's own");
            }
            sprintf_s(val, sizeof val, "%d", d);
            if (!RgSeed("RegenDelay", val, why)) keepD = d;
        }
    }
    if (keepV >= 0) {
        g_rgLevel = keepV;  // v3.47 RG347 inspection 2: not written, so the ini (or IniStr's default) would lose it
    } else {
        IniStr("Gameplay", "Regen", buf, sizeof buf);
        v = RgParseLevel(buf, &known);
        if (!known) Log("regeneration: [Gameplay] Regen=%s is not Off, x1, x1.25 or x2: Off", buf);
        g_rgLevel = v;
    }
    if (keepD >= 0) {
        g_rgDelay = keepD;  // v3.47 RG347 inspection 2: as above
    } else {
        IniStr("Gameplay", "RegenDelay", buf, sizeof buf);
        d = RgClampDelay(atoi(buf));
        if (d != atoi(buf)) Log("regeneration: [Gameplay] RegenDelay=%s is kept within 10-120 s: %d", buf, d);
        g_rgDelay = d;
    }
}
static void RgInit(void)  // GameplayInstall (GpReadIni / GpTexts): before the Options tables
{
    RgReadIni();
    RgTexts();
}

// ---------------------------------------------------------------- the row (gameplay.inc GpIn2 / GpGet2 / GpDesc2 call these)
static void RgSetLocked(int v, const char *from)  // under GpLock
{
    const int was = (int)g_rgLevel;
    if (v < 0 || v >= RG_LEVELS) return;
    InterlockedExchange(&g_rgLevel, v);
    g_rgUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "Regen", kRgLevelName[v], g_iniPath)) {
        g_rgUnsaved = 1;
        Log("regeneration: Regen %s (was %s; %s) applies now but could not be saved in SH4Refined.ini (error %lu)",
            kRgLevelName[v], kRgLevelName[was >= 0 && was < RG_LEVELS ? was : 0], from, GetLastError());
    } else {
        Log("regeneration: Regen %s (was %s; %s)", kRgLevelName[v], kRgLevelName[was >= 0 && was < RG_LEVELS ? was : 0], from);
    }
}
static int RgRowIn(void)  // Refined's own value: Left / Right step Off, x1, x1.25, x2
{
    int v;
    GpLock();
    v = StepValue((int)g_rgLevel, RG_LEVELS);
    if (v >= 0) RgSetLocked(v, "Options");
    GpUnlock();
    return 0;
}
static int RgRowGet(void)
{
    const int v = (int)g_rgLevel;
    return v >= 0 && v < RG_LEVELS ? v : 0;
}
static int RgRowDesc(void)
{
    if (!g_rgReady) return MSG_RG0 + RGT_NA;
    return MSG_RG0 + RGT_D0 + RgRowGet();
}
static int RgRowDescPe(void) { return MSG_RG0 + RGT_PE; }
// sh4windowmod.c RowLabelC / RowValueC: the row and its value grey while Playable Eileen's own regeneration heals
static int RgRowGrey(int slot, int row)
{
    return (unsigned)slot < 3 && g_slotPage[slot] == LP_PLAYER && row >= 0 && row < g_gpPlayerN && g_gpPlayerRow[row] == GP_REGEN &&
           RgRowModeNow() == RGR_PE;
}
static int RgValueIdGreyed(int id)
{
    return id >= (int)g_gpIds[GP_REGEN][0] && id <= (int)g_gpIds[GP_REGEN][GP_VMAX - 1] && RgRowModeNow() == RGR_PE;
}
// snapshot / changed / discard / ini re-read (gameplay.inc, under GpLock)
static void RgSnapshot(void) { g_rgSnap = g_rgLevel; }
static int RgChanged(void) { return g_rgLevel != g_rgSnap; }
static int RgRowChanged(void) { return g_snapOn && g_rgLevel != g_rgSnap; }
static void RgDiscard(void)
{
    if (g_rgLevel != g_rgSnap) RgSetLocked((int)g_rgSnap, "Options: No at the exit");
}
static void RgIniReread(void)  // GpIniReread: SH4Refined.ini changed (an edit outside the game)
{
    char buf[64];
    int v, known, d;
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Gameplay", "RegenDelay", "", buf, sizeof buf, g_iniPath);
    d = buf[0] ? RgClampDelay(atoi(buf)) : RG_DELAY_DEF;  // v3.47 RG347 inspection 1: a deleted / empty key = the default
    if (d != g_rgDelay) {
        Log("regeneration: [Gameplay] RegenDelay read again: %d s (was %ld s)", d, g_rgDelay);
        InterlockedExchange(&g_rgDelay, d);
        RgTexts();
    }
    if (g_rgUnsaved) return;  // the menu's value stands until it is saved or set again
    GetPrivateProfileStringA("Gameplay", "Regen", kRgLevelName[0], buf, sizeof buf, g_iniPath);  // v3.47 RG347 inspection 1: deleted = Off (the default)
    v = RgParseLevel(buf, &known);
    if (v == RgRowGet()) return;
    Log("regeneration: [Gameplay] Regen read again: %s (was %s)", kRgLevelName[v], kRgLevelName[RgRowGet()]);
    InterlockedExchange(&g_rgLevel, v);
    g_rgSnap = v;  // an outside edit is the new snapshot (as [Gameplay]): "No" at the exit never undoes it
}

// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread)
#define RG_P 0x10A0F20u  // gPlayer[0]
// PE's RgFree, inverted (+ a load, a movie, a quick load): not free for healing now
static int RgBusy(void)
{
    static const DWORD kFlags[] = { 0xFD5AA0, 0xFD5AA4, 0xFD5BB0, 0xFD5BB4, 0xFD5BB8, 0xFD5C28, 0x10A1914 };
    int i;
    if (*(volatile const DWORD *)0xFD5BA0 != RG_P || *(volatile const DWORD *)(RG_P + 0x1B0) != RG_P + 0xC0) return 1;  // not built
    for (i = 0; i < (int)(sizeof kFlags / sizeof kFlags[0]); ++i)
        if (*(volatile const DWORD *)kFlags[i]) return 1;  // next stage, door, world exit / game over / ending, event, scripted input
    if (*(volatile const DWORD *)(RG_P + 0x214) >= 0x64) return 1;  // the ampoule's own test (0x534B7D): a dying state
    if (*(volatile const int *)0x10A1C34 == 0x0A) return 1;          // dead (PlayerDamage 0x52D0B0)
    return *(volatile const int *)0xECB1B4 != 0 || *(volatile const int *)0xFD5A74 != 0 || g_qsLoading;  // a movie, a load
}
static const char *RgBody(int who) { return who == RGW_EILEEN ? "Eileen" : "Henry"; }
static void RgTickC(void)
{
    const DWORD now = GetTickCount();
    const int phase = *(volatile const int *)0xFD5C20, stage = *(volatile const int *)0xFD5A90;
    int pe, hasA, act, hasE, en, who, level, st, ev;
    float add, lost, hp, mx;
    RgIn in;
    RgPeLook(0);
    RgPeState(&pe, &hasA, &act, &hasE, &en);
    level = RgRowGet();
    who = RgWho(1, level, pe, hasA, act, hasE, en);
    if (who != g_rgWho) {
        if (LogQuota(&g_rgQWho, 100))
            Log("regeneration: who heals: %s (Refined's Regen %s, RegenDelay %ld s)", kRgWhoName[who], kRgLevelName[level], g_rgDelay);
        g_rgWho = who;
    }
    memset(&in, 0, sizeof in);
    hp = *(volatile const float *)(RG_P + 0x08);
    mx = *(volatile const float *)(RG_P + 0x2C);
    in.hp = hp;
    in.mx = mx;
    in.room = (unsigned)stage << 16 | ((unsigned)*(volatile const int *)0xFD5A94 & 0xFFFFu);
    in.gap = g_rgLastMs && now - g_rgLastMs > 1000;
    g_rgLastMs = now;
    in.paused = phase != 0 && phase != 1;  // 2 = paused (pause menu, inventory, map, files, Options): as PE's PauseGuard
    in.busy = RgBusy();
    in.aura = *(volatile const float *)0x10A3320 > 0.0f || *(volatile const float *)0x10A2EE0 > 0.0f;
    in.r302 = stage == 1 && !(*(volatile const BYTE *)0x13D8143 & 0x80);
    in.dt = RgDt(*(volatile const int *)0x5D5248);
    in.perSec = who == RGW_HENRY || who == RGW_EILEEN ? RgPerSec(level, mx) : 0.0f;
    in.delay = (float)g_rgDelay;
    add = RgStep(&g_rg, &in, &st, &lost);
    if (add > 0.0f) {
        float after;
        ((void(__cdecl *)(int, float))0x52D1D0)(0, add);  // PlayerHealAmount(slot 0, amount): HP += amount, capped at max
        after = *(volatile const float *)(RG_P + 0x08);
        RgNoteHp(&g_rg, after);
        if (after > hp) g_rgHealed += after - hp;
        else if (LogQuota(&g_rgQAnom, 3))
            LogAnomaly("regeneration", "0052D1D0 did not raise HP (%.3f -> %.3f of %.0f, +%.4f asked): another mod on PlayerHealAmount?",
                       hp, after, mx, add);
    }
    ev = RgRunStep(&g_rgRun, st, add, hp, g_rg.clock);
    if (ev == 1) {
        if (LogQuota(&g_rgQRun, 400))
            LogV("regeneration: heals %s from %.1f / %.0f HP (%s: %.3f HP a second)", RgBody(who), hp, mx, kRgLevelName[level],
                 in.perSec);
    } else if (ev == 2) {
        if (LogQuota(&g_rgQRun, 400)) {
            if (g_rgRun.why == RGS_WAIT && lost > 0.0f)
                LogV("regeneration: stops: hurt (-%.1f HP; %.1f / %.0f HP, +%.1f HP in %.1f s): no healing for %ld s", lost, hp, mx,
                     g_rgRun.gained, g_rgRun.at - g_rgRun.start, g_rgDelay);
            else
                LogV("regeneration: stops: %s (%.1f / %.0f HP, +%.1f HP in %.1f s)", kRgStateName[g_rgRun.why], hp, mx, g_rgRun.gained,
                     g_rgRun.at - g_rgRun.start);
        }
    }
}
static int RgFaultNote(DWORD code)
{
    Log("regeneration: faulted (code %08lX): Refined's regeneration is off for this session (%.1f HP healed)", code, g_rgHealed);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void RgTick(void)  // quicksave.inc QsTick (its own __try: fail closed)
{
    if (!g_rgReady) return;
    __try {
        RgTickC();
    } __except (RgFaultNote(GetExceptionCode())) {
        g_rgReady = 0;
    }
}

static void RegenInstall(void)  // Init, after QuickSaveInstall (the 0x4EF982 dispatcher)
{
    static const Site kHeal[2] = {  // PlayerHealAmount 0x52D1D0 (both paths, through the second ret 0x52D20A)
        { 0x52D1D0, { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08, 0x69, 0xC9, 0x20, 0x04, 0x00, 0x00, 0xD9, 0x81, 0x4C, 0x0F, 0x0A, 0x01,
                      0xD9, 0x45, 0x0C, 0xD8, 0x81, 0x28, 0x0F, 0x0A, 0x01, 0xD9, 0x55, 0x08, 0xD8, 0xD9 }, 32 },
        { 0x52D1F0, { 0xDF, 0xE0, 0xF6, 0xC4, 0x41, 0x75, 0x08, 0xD9, 0x99, 0x28, 0x0F, 0x0A, 0x01, 0x5D, 0xC3, 0x8B, 0x45, 0x08,
                      0xDD, 0xD8, 0x89, 0x81, 0x28, 0x0F, 0x0A, 0x01, 0x5D }, 27 },
    };
    const int heal = SiteOk(&kHeal[0]) && SiteOk(&kHeal[1]);
    int pe, hasA, act, hasE, en;
    RgReset(&g_rg);
    memset(&g_rgRun, 0, sizeof g_rgRun);
    if (!heal) Log("regeneration: 0052D1D0 (PlayerHealAmount) is not the stock code (another mod?): Refined's regeneration does nothing");
    if (!g_qsTickOn) Log("regeneration: no 0x4EF982 tick dispatcher: Refined's regeneration does nothing");
    g_rgReady = heal && g_qsTickOn;
    RgPeLook(1);
    RgPeState(&pe, &hasA, &act, &hasE, &en);
    Log("regeneration: %s (no exe patch; tick 004EF982, heals through 0052D1D0 PlayerHealAmount, aura check [010A3320] / "
        "[010A2EE0], Room 302 after its own heal ends); [Gameplay] Regen %s (%g %% of max health a minute), RegenDelay %ld s; "
        "who heals now: %s", g_rgReady ? "in" : "OFF", kRgLevelName[RgRowGet()], (double)kRgRatePct[RgRowGet()], g_rgDelay,
        kRgWhoName[RgWho(g_rgReady, RgRowGet(), pe, hasA, act, hasE, en)]);
}
