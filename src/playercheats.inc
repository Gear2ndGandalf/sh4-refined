// ================================================================ PLAYER PACK (special mods, player round 1)
// Ammo Saver, Dark Pact, Grab Breaker, Quick Charge and Life Steal: five rows on Option 5 > Player, values kept in
// [Gameplay] and applied at once (as Movement Speed). Plan: staging/special_mods/player_cheats/p1_combat/PLAN.md; RE:
// staging/special_mods/player_cheats/PROPOSALS.md sections 3, 4, 5, 8, 10 (Life Steal = "Blood Drinker"). GOG v7 VAs.
// Included at the end of gameplay.inc: it reuses GpPatch / GpBranch / GpApplyAll (all-or-nothing installs with byte
// checks and rollback; Poke lists every range in SH4WM_Patches), GpLock, OhkDamage and FillRow / StepValue / Encode.
// gameplay.inc's Player page, snapshot, change test, discard and ini poll call the Pc* functions declared there.
// The pure rules (the ammo rule, the factors, the grab start, the heal amount) are pc_rules.h (native test
// test/pc_rules_test.c).
//
// Sites (each byte-checked, free of cheat-table AOBs / writes (ct_conflicts --span) and of Playable Eileen patches):
//   Ammo Saver    `call 516EB0` (HudBeltRemoveItem, the round) at 0x53DFAA (pistol) and 0x53DFC6 (the other guns) in
//                 PlayerGunFire 0x53DF50 -> PcAmmoTake (cdecl, the item pushed; the caller ignores the result)
//   Dark Pact     taken: `fmul [5B7460] / fadd [5B7520]` (the 0.2 rnd + 0.9 factor) at 0x52D0E8 in PlayerDamage
//                 0x52D0B0 -> call hkPactTake (slot 0 only: x k); dealt: Refined's own hit-record stub 0x44F605
//                 (hkOhk) now calls PcHitDamage, which scales the player's damage x k before OhkDamage
//   Grab Breaker  `mov [esi+230h], ecx` (the struggle meter's start) at 0x53EE36 in the grab entry 0x53ED20 (ghost touch
//                 0x484260, Sniffer Dog 0x4906D0) -> call hkGrab (start x 1 / x 0.5 / at most 0.1 s; slot 0 only)
//   Quick Charge  `fmul [5B7E0C]` (100.0) at 0x544D6E in PlayerItemGetChargeRate 0x544D40 -> call hkQc (x 100 k; Off:
//                 the stock fmul). v3.43 QC343: replaces v3.42's Tireless, whose site 0x5386BC (`call 55AE60`, the
//                 fatigue gain) is stock again; an old [Gameplay] Tireless line is ignored and removed from the ini (logged at the start; v3.43 MERGE)
//   Life Steal    no site of its own: PcHitDamage heals her after a hit on a monster, through the game heal at the
//                 ampoule call 0x534B98 (Playable Eileen's RegenHealAmt when PE wrapped it, so its BlockOtherRegen
//                 guard counts the heal as a game heal; else 0x52D1D0 PlayerHealAmount itself). v3.47 RG347: that
//                 path only while PE's own regeneration is in charge (or an older PE); otherwise 0x52D1D0 directly

// v3.43 QC343: the value texts stay 3 a row (PC_TXTV) as in v3.42; Quick Charge's 4th value (3x) has its own two ids
// at PCM_X0 (name, description), so PCM_N is 42 of the block's 48 (PC_VMAX x 5 x 2 would need 50)
#define PC_TXTV 3
enum { PCM_L0, PCM_D0 = PCM_L0 + PC_COUNT, PCM_V0 = PCM_D0 + PC_COUNT, PCM_VD0 = PCM_V0 + PC_COUNT * PC_TXTV,
       PCM_X0 = PCM_VD0 + PC_COUNT * PC_TXTV, PCM_N = PCM_X0 + 2 };
static int PcValTxt(int k, int v) { return v < PC_TXTV ? PCM_V0 + k * PC_TXTV + v : PCM_X0; }        // v3.43 QC343
static int PcValDescTxt(int k, int v) { return v < PC_TXTV ? PCM_VD0 + k * PC_TXTV + v : PCM_X0 + 1; }  // v3.43 QC343
typedef char PcTextsFit[PCM_N <= MSG_HB0 - MSG_PC0 ? 1 : -1];  // sh4windowmod.c's id block MSG_PC0..MSG_HB0 (v3.29 merge: HUD BARS after it)
typedef char PcGpFit[GPM_N <= MSG_PC0 - MSG_GP0 ? 1 : -1];     // gameplay.inc's block ends before ours

static const int kPcCount[PC_COUNT] = { 3, 3, 3, 4, 3 };  // v3.43 QC343: Quick Charge 4 (Tireless had 2)
typedef char PcQcIs3[PC_QC == 3 && PC_VMAX == 4 ? 1 : -1];  // v3.43 QC343 (PcTexts checks that only this row has a 4th)
static const char *const kPcIniKey[PC_COUNT] = { "AmmoSaver", "DarkPact", "GrabBreaker", "QuickCharge", "LifeSteal" };  // v3.43 QC343
static const char *const kPcIni[PC_COUNT][PC_VMAX] = {  // the ini's texts
    { "Normal", "Saver", "Endless" }, { "Off", "Pact", "Blood" }, { "Stock", "Quick", "Instant" },
    { "Off", "1.5x", "2x", "3x" },  // v3.43 QC343
    { "Off", "5", "10" } };
// v3.53 OL1: the plan's words (Grab Breaker -> Grab Escape; Quick Charge / Life Steal: the row line for every value)
static const char *const kPcName[PC_COUNT][PC_VMAX] = {  // the row's values (8 characters at most: no arrow shift)
    { OLW_COMBAT_AMMO_SAVER_V_NORMAL, OLW_COMBAT_AMMO_SAVER_V_SAVER, OLW_COMBAT_AMMO_SAVER_V_ENDLESS },
    { OLW_COMBAT_DARK_PACT_V_OFF, OLW_COMBAT_DARK_PACT_V_PACT, OLW_COMBAT_DARK_PACT_V_BLOOD },
    { OLW_PLAYER_GRAB_ESCAPE_V_STOCK, OLW_PLAYER_GRAB_ESCAPE_V_QUICK, OLW_PLAYER_GRAB_ESCAPE_V_INSTANT },
    { OLW_COMBAT_QUICK_CHARGE_V_OFF, OLW_COMBAT_QUICK_CHARGE_V_1_5X, OLW_COMBAT_QUICK_CHARGE_V_2X, OLW_COMBAT_QUICK_CHARGE_V_3X },  // v3.43 QC343
    { OLW_COMBAT_LIFE_STEAL_V_OFF, OLW_COMBAT_LIFE_STEAL_V_5, OLW_COMBAT_LIFE_STEAL_V_10 } };
static const char *const kPcLabel[PC_COUNT] = { OLW_COMBAT_AMMO_SAVER_L, OLW_COMBAT_DARK_PACT_L, OLW_PLAYER_GRAB_ESCAPE_L,
                                                OLW_COMBAT_QUICK_CHARGE_L, OLW_COMBAT_LIFE_STEAL_L };  // v3.43 QC343
static const char *const kPcDescTxt[PC_COUNT] = {  // every value without its own line
    OLW_COMBAT_AMMO_SAVER_D, OLW_COMBAT_DARK_PACT_D, OLW_PLAYER_GRAB_ESCAPE_D, OLW_COMBAT_QUICK_CHARGE_D,  // v3.43 QC343
    OLW_COMBAT_LIFE_STEAL_D };
static const char *const kPcValDesc[PC_COUNT][PC_VMAX] = {  // v3.53 OL1: value 0 may have its own line too (Grab Escape's Stock)
    { NULL, OLW_COMBAT_AMMO_SAVER_VD_SAVER, OLW_COMBAT_AMMO_SAVER_VD_ENDLESS },
    { NULL, OLW_COMBAT_DARK_PACT_VD_PACT, OLW_COMBAT_DARK_PACT_VD_BLOOD },
    { OLW_PLAYER_GRAB_ESCAPE_VD_STOCK, OLW_PLAYER_GRAB_ESCAPE_VD_QUICK, OLW_PLAYER_GRAB_ESCAPE_VD_INSTANT },
    { NULL, NULL, NULL, NULL },  // v3.43 QC343
    { NULL, NULL, NULL } };

static volatile LONG g_pc[PC_COUNT];
static LONG g_snapPc[PC_COUNT];
static int g_pcUnsaved[PC_COUNT];  // as g_gpUnsaved (review REV-325-013)
static int g_pcOn[PC_COUNT];       // the row's hooks are in (else its description says it is unavailable)
static BYTE g_txtPc[PCM_N][56];
static DWORD g_pcIds[PC_COUNT][PC_VMAX];
// the hooks' globals (written by PcApply on any thread; 4-byte stores)
static volatile LONG g_pcAmmo, g_pcGrab, g_pcQc, g_pcSteal, g_pcPact;  // v3.43 QC343: g_pcQc (was g_pcTire)
static volatile float g_pcQcMul = 100.0f;  // v3.43 QC343: hkQc's fmul operand, 100 x k (stock 100.0 at 0x5B7E0C)
static unsigned g_pcQcSeen[8];             // v3.43 QC343: items whose first charge was logged, (item - 0x10) as a bit
static volatile float g_pcDealtK = 1.0f, g_pcTakeA = 0.2f, g_pcTakeB = 0.9f;  // 0.2 / 0.9 = 0x5B7460 / 0x5B7520
static unsigned g_pcShots[2];  // Ammo Saver: shots per gun under Saver (game thread)

static const BYTE *PcText(int id)
{
    return id >= MSG_PC0 && id < MSG_PC0 + PCM_N ? g_txtPc[id - MSG_PC0] : g_txtPc[0];
}

static void PcApply(int k, int v)
{
    if (k == PC_AMMO) InterlockedExchange(&g_pcAmmo, v);
    else if (k == PC_PACT) {
        const float f = PcPactK(v);
        g_pcDealtK = f;
        g_pcTakeA = 0.2f * f;  // (at x1 the same bits as the game's own constants)
        g_pcTakeB = 0.9f * f;
        InterlockedExchange(&g_pcPact, v);
    }
    else if (k == PC_GRAB) InterlockedExchange(&g_pcGrab, v);
    else if (k == PC_QC) {  // v3.43 QC343: the factor first, then the switch (hkQc reads g_pcQc, then g_pcQcMul)
        g_pcQcMul = (float)PcQcMul100(v);
        InterlockedExchange(&g_pcQc, v);
        memset(g_pcQcSeen, 0, sizeof g_pcQcSeen);  // each item's first charge is logged again at the new factor
    }
    else if (k == PC_STEAL) InterlockedExchange(&g_pcSteal, v);
}

static void PcSetLocked(int k, int v)
{
    if (k < 0 || k >= PC_COUNT || v < 0 || v >= kPcCount[k]) return;
    InterlockedExchange(&g_pc[k], v);
    PcApply(k, v);
    g_pcUnsaved[k] = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", kPcIniKey[k], kPcIni[k][v], g_iniPath)) {
        g_pcUnsaved[k] = 1;
        Log("player pack: %s %s applies now but could not be saved in SH4Refined.ini (error %lu): it is lost at the next "
            "start", kPcIniKey[k], kPcIni[k][v], GetLastError());
    } else {
        Log("player pack: %s %s%s", kPcIniKey[k], kPcIni[k][v], g_pcOn[k] ? "" : " (its hooks are not in: no effect)");
    }
}
static int PcInput(int k)
{
    int v;
    GpLock();  // the whole read-modify-write (as GpInput)
    v = StepValue((int)g_pc[k], kPcCount[k]);
    if (v >= 0) PcSetLocked(k, v);
    GpUnlock();
    return 0;
}
static int PcDesc(int k)
{
    const int v = (int)g_pc[k];
    if (!g_pcOn[k]) return MSG_GP0 + GPM_DPNA;  // "Not available in this game (see SH4Refined.log)."
    if (v >= 0 && v < kPcCount[k] && kPcValDesc[k][v]) return MSG_PC0 + PcValDescTxt(k, v);  // v3.43 QC343  // v3.53 OL1: >= 0 (Stock's line)
    return MSG_PC0 + PCM_D0 + k;
}
#define PC_ROWFNS(i)                                                                              \
    static int __cdecl PcIn##i(BYTE *cfg, int count) { (void)cfg; (void)count; return PcInput(i); } \
    static int __cdecl PcGet##i(void) { return (int)g_pc[i]; }                                       \
    static int __cdecl PcDesc##i(void) { return PcDesc(i); }
PC_ROWFNS(0)
PC_ROWFNS(1)
PC_ROWFNS(2)
PC_ROWFNS(3)
PC_ROWFNS(4)
static const void *const kPcIn[PC_COUNT] = { PcIn0, PcIn1, PcIn2, PcIn3, PcIn4 };
static const void *const kPcGet[PC_COUNT] = { PcGet0, PcGet1, PcGet2, PcGet3, PcGet4 };
static const void *const kPcDesc[PC_COUNT] = { PcDesc0, PcDesc1, PcDesc2, PcDesc3, PcDesc4 };
static void PcFillRow(DWORD *r, int k)  // GpBuildPlayer
{
    FillRow(r, kPcIn[k], kPcGet[k], g_pcIds[k], kPcCount[k], MSG_PC0 + PCM_L0 + k, kPcDesc[k]);
}

// snapshot / changed / discard / ini poll (called from GpSnapshot, GpChanged, GpRowChanged, GpDiscard, GpIniReread)
static void PcSnapshot(void)
{
    int k;
    GpLock();
    for (k = 0; k < PC_COUNT; ++k) g_snapPc[k] = g_pc[k];
    GpUnlock();
}
static int PcChanged(void)
{
    int k;
    for (k = 0; k < PC_COUNT; ++k)
        if (g_pc[k] != g_snapPc[k]) return 1;
    return 0;
}
static int PcRowChanged(int k) { return k >= 0 && k < PC_COUNT && g_snapOn && g_pc[k] != g_snapPc[k]; }
static void PcDiscard(void)
{
    int k;
    GpLock();
    for (k = 0; k < PC_COUNT; ++k)
        if (g_pc[k] != g_snapPc[k]) PcSetLocked(k, g_snapPc[k]);
    GpUnlock();
}
static int PcParse(int k, const char *buf)  // unknown text = the first value
{
    int v;
    if (k == PC_QC) return (v = PcQcAlias(buf)) > 0 ? v : 0;  // v3.43 QC343: Off / 1.5x / 2x / 3x (and x1.5 / 150 ...)
    for (v = kPcCount[k] - 1; v > 0 && _stricmp(buf, kPcIni[k][v]); --v) {}
    return v;
}
static void PcIniReread(void)  // GpIniReread (the file changed): an outside edit also becomes the snapshot
{
    char buf[32];
    int k, v;
    if (g_iniFallback) return;
    GpLock();
    for (k = 0; k < PC_COUNT; ++k) {
        if (g_pcUnsaved[k]) continue;
        GetPrivateProfileStringA("Gameplay", kPcIniKey[k], kPcIni[k][g_pc[k]], buf, sizeof buf, g_iniPath);
        v = PcParse(k, buf);
        if (v == g_pc[k]) continue;
        InterlockedExchange(&g_pc[k], v);
        g_snapPc[k] = v;
        PcApply(k, v);
        Log("player pack: [Gameplay] %s read again: %s", kPcIniKey[k], kPcIni[k][v]);
    }
    GpUnlock();
}
static void PcReadIni(void)  // GpReadIni (Init)
{
    char buf[32];
    int k;
    for (k = 0; k < PC_COUNT; ++k) {
        IniStr("Gameplay", kPcIniKey[k], buf, sizeof buf);
        g_pc[k] = PcParse(k, buf);
        if (k == PC_QC && buf[0] && PcQcAlias(buf) < 0)  // v3.43 QC343
            Log("quick charge: [Gameplay] QuickCharge=%s is not a value (Off, 1.5x, 2x or 3x): Off", buf);
        PcApply(k, (int)g_pc[k]);
    }
    Log("player pack: AmmoSaver %s, DarkPact %s, GrabBreaker %s, QuickCharge %s, LifeSteal %s", kPcIni[0][g_pc[0]],
        kPcIni[1][g_pc[1]], kPcIni[2][g_pc[2]], kPcIni[3][g_pc[3]], kPcIni[4][g_pc[4]]);  // v3.43 QC343
    if (!g_iniFallback) {  // v3.43 QC343: v3.42's Tireless is gone (Quick Charge took its row)
        // v3.43 MERGE (the host's decision): the stale line is removed from the ini once, here, and logged
        GetPrivateProfileStringA("Gameplay", "Tireless", "\x01", buf, sizeof buf, g_iniPath);
        if (strcmp(buf, "\x01")) {
            const BOOL gone = WritePrivateProfileStringA("Gameplay", "Tireless", NULL, g_iniPath);
            Log("quick charge: [Gameplay] Tireless=%s (v3.42 and older) is ignored: Quick Charge replaced Tireless, and "
                "the fatigue site 005386BC stays the game's own; the line %s", buf,
                gone ? "was removed from SH4Refined.ini" : "could not be removed from SH4Refined.ini");
        }
    }
}
static void PcTexts(void)  // GpTexts
{
    int k, v;
    for (k = 0; k < PC_COUNT; ++k) {
        Encode(g_txtPc[PCM_L0 + k], 56, kPcLabel[k]);
        Encode(g_txtPc[PCM_D0 + k], 56, kPcDescTxt[k]);
        for (v = 0; v < PC_VMAX; ++v) {
            if (v >= PC_TXTV && (k != PC_QC || v >= kPcCount[k])) {  // v3.43 QC343: only Quick Charge has a 4th value
                if (v < kPcCount[k]) LogAnomaly("player pack", "row %d has %d values: its 4th has no text id", k, kPcCount[k]);
                g_pcIds[k][v] = MSG_PC0 + PcValTxt(k, 0);
                continue;
            }
            Encode(g_txtPc[PcValTxt(k, v)], 56, v < kPcCount[k] ? kPcName[k][v] : "");  // v3.43 QC343: PcValTxt
            Encode(g_txtPc[PcValDescTxt(k, v)], 56, kPcValDesc[k][v] ? kPcValDesc[k][v] : "");
            g_pcIds[k][v] = MSG_PC0 + PcValTxt(k, v);
        }
    }
}

// ---------------------------------------------------------------- the hooks
// Ammo Saver: replaces `call 516EB0` (push item) at 0x53DFAA / 0x53DFC6. A free shot skips the call (the gun's slot
// count, its rounds loaded, stays); the empty test 0x4F6020 and the reloads stay stock.
static int __cdecl PcAmmoTake(int item)
{
    const int gun = PcAmmoGun(item), mode = (int)g_pcAmmo;
    if (gun >= 0 && mode && !PcAmmoRule(mode, &g_pcShots[gun])) return 0;
    return ((int(__cdecl *)(int))0x516EB0)(item);
}

// Dark Pact, damage taken: call from 0x52D0E8 (12 bytes: fmul [5B7460] / fadd [5B7520]; the call + 7 nops); st0 = the random 0..1 from
// 0x4215D0; out st0 = rnd x 0.2 k + 0.9 k for slot 0 (esi = slot x 420h), stock for any other slot. The flags are dead
// here (0x52D0E1's je has run; 0x52D0FD's fcom sets them again).
static __declspec(naked) void hkPactTake(void)
{
    __asm {
        test esi, esi
        jnz stock
        fmul dword ptr g_pcTakeA
        fadd dword ptr g_pcTakeB
        ret
    stock:
        fmul dword ptr ds:[5B7460h]
        fadd dword ptr ds:[5B7520h]
        ret
    }
}

// Grab Breaker: call from 0x53EE36 (6 bytes: mov [esi+230h], ecx; ecx = the hold length, a float the hold loop 0x53C250
// lowers by dt and by stick shake, releasing her below 0 (0x53C571 / 0x53C725..0x53C73C)). The flags of 0x53EE22's
// cmp edi, eax are live (jne at 0x53EE46): kept with pushfd / popfd; every register is kept.
static DWORD __cdecl PcGrabHold(DWORD bits, const BYTE *P)
{
    const int v = (int)g_pcGrab;
    float h;
    if (!v || P != (const BYTE *)0x10A0F20) return bits;  // slot 0 (the player) only
    memcpy(&h, &bits, 4);
    {
        const float h0 = h;
        static int s_logged;
        h = PcGrabStart(h, v);
        if (!s_logged++) Log("grab breaker: first grab: hold %.2f -> %.2f", h0, h);
    }
    memcpy(&bits, &h, 4);
    return bits;
}
static __declspec(naked) void hkGrab(void)
{
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push esi                     // P
        push ecx                     // the hold length (float bits)
        call PcGrabHold
        add esp, 8
        mov [esi + 230h], eax        // the replaced store, with the scaled hold
        pop edx
        pop ecx
        pop eax
        popfd
        ret
    }
}

// v3.43 QC343: Quick Charge: call from 0x544D6E (6 bytes: fmul [5B7E0C], 100.0; the call + 1 nop) in
// PlayerItemGetChargeRate 0x544D40: st0 = dt (0x55AE60), [esp+8] = the item's charge seconds (the function's own
// argument slot, written by its fstp [esp+4]), eax = (item - 0x10) x 3; next: fdiv [esp+4]; ret. Off: the stock fmul.
// On: x 100 k, and PcQcNote logs each item's first charge (registers and the result kept; flags are dead here: the six
// callers `fadd` / `fstp` the result first). Playable Eileen's items reach this through its guard 0x544D44
// (ChargeGuardStub) with her [Melee] charge seconds (its table copy, 0x544D5E): her charges scale too.
static float g_pcQcSt0;           // (game thread)
static volatile LONG g_pcQcQ;     // LogQuota of the per-item lines (g_pcQcSeen: 0x10..0x10F)
static void __cdecl PcQcNote(DWORD timeBits, DWORD idx3)
{
    const unsigned i = idx3 / 3u;
    float t;
    memcpy(&t, &timeBits, 4);
    if (!(t > 0.0f) || idx3 % 3u) {  // (every caller tests "can charge" 0x544D10 first: time > 0)
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5))
            LogAnomaly("quick charge", "the charge rate 00544D40 ran with time %g, index %lu: the callers test 00544D10 "
                       "(time > 0) first", t, idx3);
        return;
    }
    if (i >= 256u || (g_pcQcSeen[i >> 5] & (1u << (i & 31)))) return;
    g_pcQcSeen[i >> 5] |= 1u << (i & 31);
    if (LogQuota(&g_pcQcQ, 40))
        LogV("quick charge: item %02X charges in %.2f s (game %.2f s, x%.1f; full at 100)", i + 0x10,
             t * 100.0f / g_pcQcMul, t, g_pcQcMul / 100.0f);
}
static __declspec(naked) void hkQc(void)
{
    __asm {
        cmp dword ptr g_pcQc, 0
        jne scaled
        fmul dword ptr ds:[5B7E0Ch]
        ret
    scaled:
        fmul dword ptr g_pcQcMul
        fstp dword ptr g_pcQcSt0
        push eax
        push ecx
        push edx
        push eax                     // (item - 0x10) x 3
        push dword ptr [esp + 24]    // the charge seconds ([esp + 8] at entry)
        call PcQcNote
        add esp, 8
        pop edx
        pop ecx
        pop eax
        fld dword ptr g_pcQcSt0
        ret
    }
}
// SH4R_QuickCharge (export 198, v3.43 QC343): Quick Charge's factor x 100 for other mods (Playable Eileen:
// CONTRACT_quickcharge.md): 100 Off (or the hook not in), 150, 200, 300. Any thread; reads two 4-byte values.
int __cdecl SH4R_QuickCharge(void)
{
    return g_pcOn[PC_QC] ? PcQcMul100((int)g_pcQc) : 100;
}

// Life Steal's heal: the game heal through whatever the ampoule's over-time call 0x534B98 calls. Stock: 0x52D1D0
// (PlayerHealAmount(slot, amount), capped at max). With Playable Eileen it is PE's RegenHealAmt, which calls 0x52D1D0
// and counts the rise as a game heal, so [Defense] BlockOtherRegen never puts it back. Any other target (another mod)
// is not called: the heal goes to 0x52D1D0 directly.
typedef void(__cdecl *PcHealFn)(int slot, float amount);
static int PcPeHealOk(DWORD to)
{
    HMODULE m = NULL;
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    return ds && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    (LPCSTR)(ULONG_PTR)to, &m) && m == ds && HasExport(ds, "SH4PE_Patches");
}
// v3.47 RG347 (REGEN_CONTRACT.md 3 / 5): 0x52D1D0 directly unless Playable Eileen's BlockOtherRegen acts now (its own
// regeneration in charge, SH4PE_RegenActive() == 1, or a PE before v1.23.2): only then the game's own heal path, so PE
// counts the rise as a game heal and does not put it back inside its wait (regen_sim.h RgStealViaGame)
static void PcHeal(float amount)
{
    static DWORD s_seen, s_ok;  // the last target looked at (game thread)
    static int s_via = -1;      // v3.47 RG347: the route last logged
    static volatile LONG s_q;
    DWORD to = 0x52D1D0;
    const BYTE *const site = (const BYTE *)0x534B98;
    const int via = RgStealViaGameNow();  // v3.47 RG347
    if (via != s_via) {
        s_via = via;
        if (LogQuota(&s_q, 20))
            Log("life steal: %s", via ? "heals through the game's heal 00534B98's target (Playable Eileen's regeneration is in charge: "
                "its guard counts them)" : "heals go to 0052D1D0 directly (Playable Eileen's regeneration is not in charge)");
    }
    if (via && site[0] == 0xE8) {  // v3.47 RG347: + via
        const DWORD t = 0x534B9D + *(const DWORD *)(site + 1);
        if (t != 0x52D1D0) {
            if (t != s_seen) {
                s_seen = t;
                s_ok = PcPeHealOk(t);
                Log("life steal: the game heal 00534B98 calls %08lX: %s", t, s_ok ? "Playable Eileen's heal wrapper (its "
                    "regen guard counts the heal)" : "not Playable Eileen's: heals go to 0052D1D0 directly");
            }
            if (s_ok) to = t;
        }
    }
    ((PcHealFn)(ULONG_PTR)to)(0, amount);
}

// The player's hit records (hkOhk, 0x44F605; edi == the player's attack object 0x10A4FE0): Dark Pact scales the damage
// of a hit on a monster, ghost or boss (types 0x02..0x11) x k, then One-Hit Kill (OhkDamage) as before, then Life
// Steal heals her from a hit on a monster (types 0x02..0x0A; not the Wall boss, not ghosts or Walter, who never die).
static DWORD __cdecl PcHitDamage(BYTE *task, const float *hp, DWORD dmgBits, const void *atk)
{
    const int pact = g_pcOn[PC_PACT] ? (int)g_pcPact : 0, steal = g_pcOn[PC_STEAL] ? (int)g_pcSteal : 0;
    DWORD d = dmgBits;
    int type = -1, wallBoss = 1;
    float before = 0.0f;
    if (pact || steal) {
        __try {
            const BYTE *work = task ? *(BYTE *const *)(task + 0x408) : NULL, *pr = task ? *(BYTE *const *)(task + 0x41C) : NULL;
            if (work) type = *(const int *)(work + 0x20);
            if ((ULONG_PTR)(pr + 0x2C) >= 0x10000) wallBoss = type == 5 && (*(const unsigned *)(pr + 0x40) & 0xF) == 8;
            else wallBoss = type == 5;  // (placement not readable: fail closed, as OhkDamage)
            if (hp) before = hp[0];
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            type = -1;
        }
    }
    if (pact && type >= 0x02 && type <= 0x11) {
        float f;
        memcpy(&f, &d, 4);
        f *= g_pcDealtK;
        if (f == f && f > 0.0f) memcpy(&d, &f, 4);
    }
    d = OhkDamage(task, hp, d, atk);
    if (steal && type >= 0x02 && type <= 0x0A && !wallBoss) {
        __try {
            float dmg, heal;
            memcpy(&dmg, &d, 4);
            heal = PcStealHeal(steal, dmg, before, *(const float *)0x10A0F28, *(const float *)0x10A0F4C);
            if (heal > 0.0f && *(const int *)0x10A1C34 != 0x0A) {  // (0x0A: she is dead, 0x52D130)
                static int s_logged;
                PcHeal(heal);
                if (!s_logged++) Log("life steal: first heal +%.2f HP (hit %.1f on type %02X)", heal, dmg, type);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return d;
}

static void PcInstall(void)  // GameplayInstall, after CheatsInstall (needs g_ohkOn)
{
    static const Site kAmmo[2] = {
        { 0x53DFA9, { 0x52, 0xE8, 0x01, 0x8F, 0xFD, 0xFF, 0x83, 0xC4, 0x04 }, 9 },  // push edx; call 516EB0; add esp,4
        { 0x53DFC5, { 0x52, 0xE8, 0xE5, 0x8E, 0xFD, 0xFF, 0x83, 0xC4, 0x04 }, 9 },
    };
    static const Site kTake = { 0x52D0E3, { 0xE8, 0xE8, 0x44, 0xEF, 0xFF, 0xD8, 0x0D, 0x60, 0x74, 0x5B, 0x00, 0xD8, 0x05,
                                            0x20, 0x75, 0x5B, 0x00 }, 17 };  // call 4215D0; fmul [5B7460]; fadd [5B7520]
    static const Site kGrab = { 0x53EE30, { 0x89, 0xBE, 0x2C, 0x02, 0x00, 0x00, 0x89, 0x8E, 0x30, 0x02, 0x00, 0x00 }, 12 };
    static const Site kQc = { 0x544D6E, { 0xD8, 0x0D, 0x0C, 0x7E, 0x5B, 0x00, 0xD8, 0x74, 0x24, 0x04, 0xC3 }, 11 };  // v3.43 QC343: fmul [5B7E0C]; fdiv [esp+4]; ret
    GpPatch set[2];
    if (SiteOk(&kAmmo[0]) && SiteOk(&kAmmo[1])) {
        GpBranch(&set[0], 0x53DFAA, 0xE8, PcAmmoTake, 5);
        GpBranch(&set[1], 0x53DFC6, 0xE8, PcAmmoTake, 5);
        g_pcOn[PC_AMMO] = GpApplyAll(set, 2, "ammo saver");
    } else {
        Log("ammo saver: 0053DFAA / 0053DFC6 differ from the GOG v7 exe (another mod?): the option does nothing");
    }
    if (!g_ohkOn) {  // the dealt half lives in the hit-record stub: without it the pact (and the steal) stay out
        Log("dark pact / life steal: the hit-record hook 0044F605 is not in: both options do nothing");
    } else if (SiteOk(&kTake)) {  // 12 bytes: the call (5) and 7 nops (GpPatch holds 8 bytes a write: two writes)
        static const BYTE kNop7[7] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
        GpBranch(&set[0], 0x52D0E8, 0xE8, hkPactTake, 5);
        GpData(&set[1], 0x52D0ED, kNop7, 7);
        g_pcOn[PC_PACT] = GpApplyAll(set, 2, "dark pact");
    } else {
        Log("dark pact: 0052D0E8 differs from the GOG v7 exe (another mod?): the option does nothing");
    }
    g_pcOn[PC_STEAL] = g_ohkOn;
    if (SiteOk(&kGrab)) {
        GpBranch(&set[0], 0x53EE36, 0xE8, hkGrab, 6);
        g_pcOn[PC_GRAB] = GpApplyAll(set, 1, "grab breaker");
    } else {
        Log("grab breaker: 0053EE36 differs from the GOG v7 exe (another mod?): the option does nothing");
    }
    if (SiteOk(&kQc)) {  // v3.43 QC343 (Tireless's 0x5386BC is no longer written)
        GpBranch(&set[0], 0x544D6E, 0xE8, hkQc, 6);
        g_pcOn[PC_QC] = GpApplyAll(set, 1, "quick charge");
    } else {
        Log("quick charge: 00544D6E differs from the GOG v7 exe (another mod?): the option does nothing");
    }
    Log("player pack hooks: ammo saver %s (0053DFAA / 0053DFC6), dark pact %s (0052D0E8 + 0044F605), grab breaker %s "
        "(0053EE36), quick charge %s (00544D6E), life steal %s (0044F605)", g_pcOn[PC_AMMO] ? "in" : "OFF",
        g_pcOn[PC_PACT] ? "in" : "OFF", g_pcOn[PC_GRAB] ? "in" : "OFF", g_pcOn[PC_QC] ? "in" : "OFF",
        g_pcOn[PC_STEAL] ? "in" : "OFF");  // v3.43 QC343
    if (g_pcOn[PC_QC])
        LogV("quick charge: %s (x%.1f): the charge rate 00544D40 (all six callers, Playable Eileen's items through its "
             "guard 00544D44 too) is scaled; the fatigue site 005386BC (v3.42's Tireless) is stock", kPcIni[PC_QC][g_pc[PC_QC]],
             PcQcMul100((int)g_pcQc) / 100.0);
}
