// ================================================================ v3.25.0: Blood Colour values, Option 5 Player / Enemies
// Plan and review: staging/feedback_v325/PLAN.md (claudex-loop, Codex); RE: staging/feedback_v325/re_blood.md,
// re_gameplay.md, re_cheats2.md. Included by sh4windowmod.c before InstallPages. All addresses are GOG v7 VAs.
//
// Blood Colour: the stock Option 3 row (cfg + 0x13 = 0x1091AE3, 0 Red / 1 Green / 2 Purple, saved in the cfg block of
// every save) gets 10 values. Its three readers take the colour from tables here: the floor splash switch (0x43008E
// `call 5230C0` -> jmp hkBldSplash), the burst switch (0x430AC3 -> jmp hkBldBurst) and the pool's two colour tables
// (the `lea` disp32 at 0x4777C4 / 0x4777EE -> g_bldPool1 / 2). Entries 0..2 are the stock colours.
//
// Option 5 (Gameplay): Item Shine, Player >, Enemies >, Exit. The two hidden pages (OP_PLAYER, OP_ENEMY) open and close
// with the stock sub-screen fade (as Subtitle Style / Screen Effects) through the generalised requests REQ_OPEN | lp /
// REQ_BACK (SubScreen). Values apply at once, are kept in [Gameplay] (Regeneration: Playable Eileen's own ini keys).
// v3.37 MN337 (staging/v337_feedback/w1_menu/PLAN.md; the user's requests 3, 8, 9, 10, 11): Option 5 = No Clip, Curse
// Meter, Extended Toolbelt, Stackable Items (inventory.inc), Item Shine, Player >, Enemies >, Special >, Exit; One-Hit
// Kill is the Player folder's top row (it left Enemies), No Clip left Special, Curse Meter left Player. Every row
// position is recorded when the tables are built (g_gpTop, g_gpPlayerRow, g_gpEnemyRow, g_gpSpecialRow, g_playShineRow,
// g_lpParentRow): nothing asks for a fixed row number any more.

// ---------------------------------------------------------------- Blood Colour
#define BLD_N 10
enum { GP_MOVE, GP_MED, GP_REGEN, GP_OHK, GP_DP, GP_AI, GP_ECHO, GP_ECHON, GP_COPIES, GP_FOLLOW, GP_DROP, GP_REVIVE, GP_N };  // the Option 5 settings (v3.27.0: + Death Presence, Special page; v3.29 AFTERIMAGE: + After-image; v3.30 ECHO: + Echo Double)  // v3.31 ENEMIES: + Enemy Copies, Enemies Follow (Enemies page)  // SUPPLY DROPS: + GP_DROP  // v3.30 ECHO330: + GP_ECHON (Echo Count)  // v3.30 EN330: + GP_REVIVE (Revive Enemies, Enemies page)  // v3.30 MERGE: both (GP_ECHON 7 .. GP_REVIVE 11, GP_N 12)
#define GP_VMAX 5
// PLAYER PACK: Ammo Saver, Dark Pact, Grab Breaker, Quick Charge (v3.43 QC343: was Tireless), Life Steal (playercheats.inc, included at the end of this
// file; the pure rules are pc_rules.h). Their rows follow this file's on the Player page; their texts are MSG_PC0..
#include "pc_rules.h"
#include "enmreturn_sim.h"  // v3.56 ER356: Enemies Return's values (ErParse: the old On), enemies.inc applies them
static const BYTE *PcText(int id);
static void PcFillRow(DWORD *r, int k);
static void PcSnapshot(void);
static int PcChanged(void);
static int PcRowChanged(int k);
static void PcDiscard(void);
static void PcIniReread(void);
static void PcReadIni(void);
static void PcTexts(void);
static void PcInstall(void);
static DWORD __cdecl PcHitDamage(BYTE *task, const float *hp, DWORD dmgBits, const void *atk);
// v3.32 NC332: No Clip (noclip.inc, included at the end of this file): its row on the Special page, its [Gameplay] key
static void NcSnapshot(void);
static int NcChanged(void);
static int NcRowChanged(void);
static void NcDiscard(void);
static void NcIniReread(void);
static void NcReadIni(void);
static void NcTexts(void);
static void NcFillRow(DWORD *r);
static void NcInstall(void);
// v3.37 MN337: Extended Toolbelt / Stackable Items (inventory.inc, W2 INV337; included after noclip.inc): their Option 5 rows
static void InvToolbeltFillRow(DWORD *r);
static void InvStackFillRow(DWORD *r);
static void InvInstall(void);
// v3.43 GT343: Ghost Teleport (teleport.inc, included in sh4windowmod.c after echo.inc): its Special-page row, [Gameplay] keys
static void GtSnapshot(void);
static int GtChanged(void);
static int GtRowChanged(void);
static void GtDiscard(void);
static void GtIniReread(void);
static void GtReadIni(void);
static void GtTexts(void);
static void GtFillRow(DWORD *r);
static void GtInstall(void);
static void InvSnapshot(void);  // v3.37 MERGE: W2 INV337's other entry points
static int InvChanged(void);
static int InvRowChanged(int which);
static void InvDiscard(void);
static void InvIniReread(void);
static int InvMedSlotP2(void);
// v3.46 OM346: Stick Movement (omni.inc, included in sh4windowmod.c after this file): [Input] keys, snapshot / discard / ini
// poll through this file's lock (v3.47 CT347: its row is on Option 2 (Control), not the Player page)
static void OmSnapshot(void);
static int OmChanged(void);
static int OmRowChanged(void);
static void OmDiscard(void);
static void OmIniReread(void);
static void OmReadIni(void);
static void OmTexts(void);
static void OmFillRow(DWORD *r);
static void OmInstall(void);
// v3.47 RG347: Refined's own Regeneration (regen.inc, included in sh4windowmod.c after omni.inc): the Regeneration row
// answers by its mode (regen_sim.h RGR_*): Refined's own value ([Gameplay] Regen), grey Off while Playable Eileen's own
// regeneration heals (SH4PE_RegenActive() == 1), or v3.46's front end for PE's ini keys (a PE without that export)
#include "regen_sim.h"
static int RgRowModeNow(void);
static int RgRowIn(void);
static int RgRowGet(void);
static int RgRowDesc(void);
static int RgRowDescPe(void);
static void RgSnapshot(void);
static int RgChanged(void);
static int RgRowChanged(void);
static void RgDiscard(void);
static void RgIniReread(void);
static void RgInit(void);
static void RgPeLook(int force);
static int RgStealViaGameNow(void);  // playercheats.inc PcHeal: Life Steal's route
// v3.48 RZ348: the Randomizer (randomizer.inc, included in sh4windowmod.c after regen.inc): its folder row, page, [Randomizer]
static void RzFolderRow(DWORD *r, const DWORD *bcRow);
static void RzBuildPage(const DWORD *stockRows, const DWORD *bcRow, DWORD *t, int parentRow);
static int RzRowChanged(int row);
static void RzSnapshot(void);
static int RzChanged(void);
static void RzDiscard(void);
static void RzIniReread(void);
static void RzInit(void);
// v3.50 EF350: Enemy Pursuit (enmfollow.inc, included at the end of enemies.inc): its Enemies-page row, [Gameplay] EnemyPursuit
static void EfPursuitFillRow(DWORD *r);
static void EfSnapshot(void);
static int EfChanged(void);
static int EfRowChanged(void);
static void EfDiscard(void);
static void EfIniReread(void);
static void EfReadIni(void);
static void EfTexts(void);
static int g_efPursuitRow;
// v3.50 FI350: Fade In (fadein.inc, included in sh4windowmod.c after rzmon.inc): its Enemies-page row, [Gameplay] FadeIn
static int FiEnemyRow(DWORD *t, int n);
static void FiSnapshot(void);
static int FiChanged(void);
static int FiRowChanged(int page, int row);
static void FiDiscard(void);
static void FiIniReread(void);
static void FiInit(void);
// texts: one block of ids from MSG_GP0 (GPM_* offsets)
enum { GPM_T_PLAYER, GPM_T_ENEMY, GPM_R_PLAYER, GPM_R_ENEMY, GPM_D_PLAYER, GPM_D_ENEMY, GPM_BACK, GPM_D_BACK,
       GPM_T_SPECIAL, GPM_R_SPECIAL, GPM_D_SPECIAL, GPM_DPD1, GPM_DPD2, GPM_DPNA,  // v3.27.0: the Special page, Death Presence's value lines
       GPM_AID1,  // v3.29 AFTERIMAGE: After-image's On line (v3.30 AS330: After Shock's Short line)
       GPM_ECD1, GPM_ECD2, GPM_ECD3,  // v3.30 ECHO: the Echo Double's value lines (Shadow, Twin, Mirror; "not available" = GPM_DPNA)  // v3.30 ECHO330: Shadow Copy, Shadow Cast, Echo Count while Off
       GPM_DRD1, GPM_DRD2, GPM_DRNA,  // SUPPLY DROPS: its value lines
       GPM_L0, GPM_D0 = GPM_L0 + GP_N, GPM_V0 = GPM_D0 + GP_N, GPM_BLDV = GPM_V0 + GP_N * GP_VMAX,
       GPM_BLDD = GPM_BLDV + BLD_N - 3,
       GPM_AID2 = GPM_BLDD + BLD_N - 3, GPM_AID3,  // v3.30 AS330: After Shock's Medium / Long lines (new ids at the end)
       GPM_NCL, GPM_NCD, GPM_NCD1,  // v3.32 NC332: No Clip's label and its Off / On lines (new ids at the end; values reuse One-Hit Kill's Off / On)
       GPM_NCV2, GPM_NCD2,  // v3.33 NC333: No Clip's value Fly and its line (new ids at the end: GPM_N 126 of 128)
       GPM_DRD3,  // v3.38 DR338: Supply Drops' Rare line (new id at the end: GPM_N 127 of 128; GPM_DRD1 = Normal, GPM_DRD2 = Abundant)
       GPM_N };
static const char *const kBldName[BLD_N] = { "Red", "Green", "Purple", "Yellow", "Orange", "Pink", "Blue", "Cyan",
                                             "White", "Black" };
// splash (re_blood A): R, G, B dwords at [esp+18h] / [esp+48h] / [esp+4Ch]; 0x80 = full, the texture caps brightness
static const BYTE kBldA[BLD_N][3] = { { 0x14, 0x00, 0x00 }, { 0x00, 0x14, 0x00 }, { 0x0E, 0x00, 0x14 },  // stock
                                      { 0x24, 0x20, 0x00 }, { 0x24, 0x0E, 0x00 }, { 0x30, 0x10, 0x1C }, { 0x00, 0x04, 0x16 },
                                      { 0x00, 0x1C, 0x20 }, { 0x50, 0x50, 0x50 }, { 0x00, 0x00, 0x00 } };
// burst (re_blood B): eax R, ecx G, ebp B
static const BYTE kBldB[BLD_N][3] = { { 0x19, 0x04, 0x04 }, { 0x00, 0x19, 0x00 }, { 0x10, 0x00, 0x16 },  // stock
                                      { 0x2C, 0x26, 0x02 }, { 0x2C, 0x12, 0x02 }, { 0x38, 0x14, 0x22 }, { 0x02, 0x06, 0x1C },
                                      { 0x02, 0x22, 0x26 }, { 0x60, 0x60, 0x60 }, { 0x01, 0x01, 0x01 } };
// pool (re_blood C), AABBGGRR: pass 1 subtractive (0x6155F8), pass 2 overlay (0x615604)
static const DWORD kBldC1[BLD_N] = { 0x805D6220, 0x805D1E5A, 0x800A621E,  // stock
                                     0x805D2020, 0x80604018, 0x80304010, 0x80105060, 0x80202060, 0x80000000, 0x80505050 };
static const DWORD kBldC2[BLD_N] = { 0x400F232C, 0x400F410A, 0x80460F2D,  // stock
                                     0x4008282C, 0x40041C38, 0x60281C40, 0x60401008, 0x50343006, 0x60606060, 0x60040404 };
// The pool indexes `[movsx byte * 4 + table]`: the disp32 points at entry 128 so every byte value -128..127 stays inside;
// values outside 0..9 (only from an edited save) get Red.
static DWORD g_bldPool1[256], g_bldPool2[256];
static DWORD g_bldIds[BLD_N];
static int g_bldOn;  // the three readers take our tables
static int g_bldRowOn;  // review REV-325-009: Option 3's row has the 10 values (InstallPages)
// a saved colour 3..9 is safe only when both are in: otherwise BldSanitize keeps the byte at 0..2
static int BldNeedSanitize(void) { return !(g_bldOn && g_bldRowOn); }
static const DWORD g_bldBackA = 0x4300D5, g_bldBackB = 0x430B0A;

static __declspec(naked) void hkBldSplash(void)  // jmp from 0x43008E; eax / ecx / edx free; ebx = 0 must stay
{
    __asm {
        movsx eax, byte ptr ds:[1091AE3h]
        cmp eax, BLD_N
        jb ok
        xor eax, eax
    ok:
        lea eax, [eax + eax * 2]
        movzx ecx, byte ptr kBldA[eax]
        mov [esp + 18h], ecx
        movzx ecx, byte ptr kBldA[eax + 1]
        mov [esp + 48h], ecx
        movzx ecx, byte ptr kBldA[eax + 2]
        mov [esp + 4Ch], ecx
        jmp dword ptr [g_bldBackA]
    }
}
static __declspec(naked) void hkBldBurst(void)  // jmp from 0x430AC3; out: eax R, ecx G, ebp B (0x430B0A zeroes edx)
{
    __asm {
        movsx edx, byte ptr ds:[1091AE3h]
        cmp edx, BLD_N
        jb ok
        xor edx, edx
    ok:
        lea edx, [edx + edx * 2]
        movzx eax, byte ptr kBldB[edx]
        movzx ecx, byte ptr kBldB[edx + 1]
        movzx ebp, byte ptr kBldB[edx + 2]
        jmp dword ptr [g_bldBackB]
    }
}
// review REV-005: the row's getter and input are ours, so a byte outside 0..9 (an edited save) is shown as Red and
// repaired to Red as soon as the row's input runs (the cursor is on it), before the stock input (0x524DA0) steps it.
static int __cdecl BldGet(void)
{
    const int v = *(const signed char *)0x1091AE3;
    return v >= 0 && v < BLD_N ? v : 0;
}
static int __cdecl BldIn(BYTE *cfg, int count)
{
    if ((signed char)cfg[0x13] < 0 || (signed char)cfg[0x13] >= BLD_N) cfg[0x13] = 0;
    return ((int(__cdecl *)(BYTE *, int))0x524DA0)(cfg, count);
}
static int __cdecl BldDesc(void)
{
    const int v = *(const signed char *)0x1091AE3;
    if (v >= 3 && v < BLD_N) return MSG_GP0 + GPM_BLDD + v - 3;
    return 0x64 + (v >= 0 && v < 3 ? v : 0);  // the stock "Makes blood colour red / green / purple." lines
}
// review REV-325-004: every multi-site feature is installed as one set: each write is checked, and on any failure the
// sites already written get their stock bytes back, so a feature is either fully in or fully out.
typedef struct { DWORD va; int len; BYTE b[8], stock[8]; } GpPatch;
static void GpBranch(GpPatch *p, DWORD va, BYTE op, const void *to, int len)
{
    const DWORD rel = (DWORD)(ULONG_PTR)to - (va + 5);
    p->va = va;
    p->len = len;
    memcpy(p->stock, (const void *)(ULONG_PTR)va, len);
    memset(p->b, 0x90, sizeof p->b);
    p->b[0] = op;
    memcpy(p->b + 1, &rel, 4);
}
static void GpData(GpPatch *p, DWORD va, const void *data, int len)
{
    p->va = va;
    p->len = len;
    memcpy(p->stock, (const void *)(ULONG_PTR)va, len);
    memcpy(p->b, data, len);
}
static int GpApplyAll(const GpPatch *p, int n, const char *what)
{
    int i, j;
    for (i = 0; i < n; ++i)
        if (!Poke(p[i].va, p[i].b, p[i].len) || memcmp((const void *)(ULONG_PTR)p[i].va, p[i].b, p[i].len)) {
            for (j = i; j >= 0; --j) Poke(p[j].va, p[j].stock, p[j].len);  // (j = i: a partial write)
            Log("%s: %08lX could not be written: the set is rolled back, the feature is off", what, p[i].va);
            return 0;
        }
    return 1;
}

// review REV-008: when the reader patches are not in (another mod changed those bytes), a save made with colour 3..9
// must not reach the stock readers (random colours, reads past their tables): the value goes back to Red at every
// game tick (the 0x4EF982 dispatcher) and when Options opens. The row then stays the stock three values.
static void __cdecl BldSanitize(void)
{
    volatile char *const b = (volatile char *)0x1091AE3;
    const char seen = *b;
    // review REV-011: replace only the bad value seen (the menu may write a valid one in between: kept)
    if (((signed char)seen < 0 || (signed char)seen > 2) && _InterlockedCompareExchange8(b, 0, seen) == seen) {
        static int s_logged;
        if (!s_logged++) Log("blood colour: a saved colour %d without the colour patches: set to Red", (int)(signed char)seen);
    }
}
// review REV-009: the tick path needs Refined's 0x4EF982 dispatcher (QuickSaveInstall, which also needs the HUD hook);
// when it is not in place, a small thread does the same every 250 ms (a one-byte store of the game's own value range)
static DWORD WINAPI BldSanitizeThread(LPVOID unused)
{
    (void)unused;
    while (BldNeedSanitize()) {  // (fixed after Init: the process's lifetime)
        Sleep(250);
        __try {
            BldSanitize();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return 0;
}
static void GpIniPoll(void);
// reviews REV-325-011 / 014: GpIniPoll's own thread when neither game-thread caller exists (then it is its only caller;
// its writes are the hooks' volatile globals and Interlocked values, as the menu's)
static DWORD WINAPI GpIniThread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        Sleep(1000);
        __try {
            GpIniPoll();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }
}
static int g_qsTickOn;  // quicksave.inc (a tentative definition: the same object)
static int g_spInstalled;  // items.inc (the same)
static void BldFallbackStart(void)  // Init, after the Options tables and QuickSaveInstall
{
    HANDLE th;
    if (!g_qsTickOn && !g_spInstalled) {  // reviews REV-325-011 / 014: neither poll path: a 1 s watcher polls instead
        th = CreateThread(NULL, 0, GpIniThread, NULL, 0, NULL);
        if (th) {
            CloseHandle(th);
            Log("gameplay: no tick dispatcher and no item-shine frame: a 1 s watcher reads [Gameplay] edits");
        } else {
            Log("gameplay: no tick dispatcher, no item-shine frame and no watcher (error %lu): [Gameplay] edits in the ini "
                "apply at the next start (the menu still applies them at once)", GetLastError());
        }
    }
    if (!BldNeedSanitize()) {  // review REV-325-009: patches and the 10-value row are both in
        g_bldTick = NULL;
        return;
    }
    if (g_qsTickOn) {  // the dispatcher runs it every tick
        g_bldTick = BldSanitize;
        Log("blood colour: %s: a saved colour outside Red / Green / Purple is set back to Red", g_bldOn ? "the 10-value row is not in" : "no colour patches");
        return;
    }
    g_bldTick = NULL;
    th = CreateThread(NULL, 0, BldSanitizeThread, NULL, 0, NULL);
    if (th) {
        CloseHandle(th);
        Log("blood colour: no tick dispatcher: a saved colour outside Red / Green / Purple is reset by a 250 ms watcher");
    } else {  // review REV-011: only the Options-opening path is left
        Log("blood colour: no tick dispatcher and the watcher could not start (error %lu): only Options openings reset a "
            "saved colour outside Red / Green / Purple", GetLastError());
    }
}
static void BloodInstall(void)
{
    static const Site kSites[4] = {
        { 0x43008E, { 0xE8, 0x2D, 0x30, 0x0F, 0x00, 0x0F, 0xBE, 0x40, 0x13 }, 9 },        // call 5230C0; movsx eax,[eax+13h]
        { 0x430AC3, { 0xE8, 0xF8, 0x25, 0x0F, 0x00, 0x0F, 0xBE, 0x40, 0x13 }, 9 },
        { 0x4777C1, { 0x8D, 0x1C, 0x85, 0xF8, 0x55, 0x61, 0x00 }, 7 },                    // lea ebx,[eax*4+6155F8]
        { 0x4777EB, { 0x8D, 0x1C, 0x9D, 0x04, 0x56, 0x61, 0x00 }, 7 },                    // lea ebx,[ebx*4+615604]
    };
    DWORD p1, p2;
    int i, bad = 0;
    for (i = 0; i < 4; ++i)
        if (!SiteOk(&kSites[i])) {
            Log("blood colour: unexpected bytes at %08lX (another mod?): the row stays Red / Green / Purple", kSites[i].va);
            ++bad;
        }
    for (i = 0; i < 3 && !bad; ++i)
        if (((const DWORD *)0x6155F8)[i] != kBldC1[i] || ((const DWORD *)0x615604)[i] != kBldC2[i]) {
            Log("blood colour: the pool colour tables are not the stock ones: the row stays Red / Green / Purple");
            ++bad;
        }
    if (bad) {
        g_bldTick = BldSanitize;
        return;
    }
    for (i = 0; i < 256; ++i) {
        const int v = i - 128;
        g_bldPool1[i] = kBldC1[v >= 0 && v < BLD_N ? v : 0];
        g_bldPool2[i] = kBldC2[v >= 0 && v < BLD_N ? v : 0];
    }
    p1 = (DWORD)(ULONG_PTR)&g_bldPool1[128];
    p2 = (DWORD)(ULONG_PTR)&g_bldPool2[128];
    {
        GpPatch set[4];
        GpBranch(&set[0], 0x43008E, 0xE9, hkBldSplash, 5);
        GpBranch(&set[1], 0x430AC3, 0xE9, hkBldBurst, 5);
        GpData(&set[2], 0x4777C4, &p1, 4);
        GpData(&set[3], 0x4777EE, &p2, 4);
        if (!GpApplyAll(set, 4, "blood colour")) {
            g_bldTick = BldSanitize;  // review REV-325-004: the stock readers are back: keep the saved value stock
            return;
        }
    }
    for (i = 0; i < BLD_N; ++i) g_bldIds[i] = i < 3 ? 0x3B + i : MSG_GP0 + GPM_BLDV + i - 3;
    g_bldOn = 1;
    Log("blood colour: %d colours (splash 0043008E, burst 00430AC3, pool 004777C4 / 004777EE)", BLD_N);
}

// ---------------------------------------------------------------- Option 5 pages: settings
static const int kGpCount[GP_N] = { 4, 4, 4, 2, 3, 4, 3, 3, 4, 4, 4, 3 };  // v3.56 ER356: Enemies Return 2 -> 3 (Off / By Room / By World)  // v3.49 EF349: Enemies Follow 3 -> 4 (+ Victims Only)  // v3.38 DR338: Supply Drops 3 -> 4 (+ Rare)  // v3.30 MERGE: AS330 After Shock 2 -> 4; ECHO330 Echo 4 -> 3, + Echo Count 3; EN330 + Revive 2  // the values a press steps through (v3.29 AFTERIMAGE: + 2; v3.30 ECHO: + 4)  // v3.31 ENEMIES: + 4, 3  // SUPPLY DROPS: + 3
static const char *const kGpIniKey[GP_N] = { "MoveSpeed", "MedallionDurability", NULL, "OneHitKill", "DeathPresence",
                                             "AfterImage",  // v3.29 AFTERIMAGE
                                             "Echo",  // v3.30 ECHO: + Echo
                                             "EchoCount",  // v3.30 ECHO330
                                             "EnemyCopies", "EnemyFollow",  // v3.31 ENEMIES
                                             "SupplyDrops",  // SUPPLY DROPS
                                             "EnemyRevive" };  // v3.30 EN330
static const char *const kGpName[GP_N][GP_VMAX] = {
    { "Normal", "x1.25", "x1.5", "x2" },
    { "Normal", "x2", "x3", "x4" },
    { "Off", "x1", "x1.25", "x2", "Custom" },  // "Custom": Playable Eileen's RegenRate is none of these (never stepped to)
    { "Off", "On" },
    { "Off", "Weak", "Strong" },
    { "Off", "Short", "Medium", "Long" },  // v3.29 AFTERIMAGE (v3.30 AS330: the trail lengths; v3.29's "On" reads as Medium)
    { "Off", "Shadow Copy", "Shadow Cast" },  // v3.30 ECHO330 (echo_sim.h ECHO_*; v3.29's Shadow / Twin / Mirror read as Shadow Copy)
    { "1", "2", "3" },  // v3.30 ECHO330: Echo Count
    { "Off", "x2", "x3", "x4" },                  // v3.31 ENEMIES: Enemy Copies
    { "Off", "Monsters", "Victims", "Monsters + Victims" },  // v3.31 ENEMIES: Enemies Follow (a long value: GpLongValW)  // v3.49 EF349: + Victims Only (enemies_sim.h ENM_FOLLOW_*, the same order)  // v3.50 EF350: "Victims Only" -> "Victims" (the old text reads: GpLegacyValue)
    { "Off", "Rare", "Normal", "Abundant" },  // SUPPLY DROPS (v3.38 DR338: + Rare, Generous -> Abundant; drops.inc kDropLevelName, same order)
    { "Off", "By Room", "By World" },  // v3.30 EN330: Revive Enemies  // v3.56 ER356: Off / On -> Off / By Room / By World (enmreturn_sim.h kErName, the same order; v3.55's On reads as By Room)
};
// v3.53 OL1: the plan's names (Movement Speed -> Move Speed, Medallion Durability -> Medallion, Enemy Copies -> Extra
// Enemies, Enemies Follow -> Door Chase, Revive Enemies -> Enemies Return); the ini keys stay kGpIniKey
static const char *const kGpLabel[GP_N] = { OLW_PLAYER_MOVE_SPEED_L, OLW_PLAYER_MEDALLION_L, OLW_PLAYER_REGENERATION_L,
                                            OLW_COMBAT_ONE_HIT_KILL_L, OLW_POWERS_DEATH_PRESENCE_L, OLW_POWERS_AFTER_SHOCK_L,
                                            OLW_POWERS_ECHO_DOUBLES_L, OLW_POWERS_ECHO_COUNT_L, OLW_ENEMIES_EXTRA_ENEMIES_L,
                                            OLW_ENEMIES_DOOR_CHASE_L, OLW_ITEMS_SUPPLY_DROPS_L, OLW_ENEMIES_ENEMIES_RETURN_L };
static const char *const kGpDescTxt[GP_N] = { OLW_PLAYER_MOVE_SPEED_D, OLW_PLAYER_MEDALLION_D,  // v3.53 OL1: the plan's lines
                                              "Playable Eileen's healing after time unhurt.",  // review REV-010: GpSnapshot puts in PE's RegenDelay
                                              OLW_COMBAT_ONE_HIT_KILL_D, OLW_POWERS_DEATH_PRESENCE_D, OLW_POWERS_AFTER_SHOCK_D,
                                              OLW_POWERS_ECHO_DOUBLES_D, OLW_POWERS_ECHO_COUNT_D,
                                              OLW_ENEMIES_EXTRA_ENEMIES_D,  // v3.58 MN358: no "Next room." (the user; was OLW_EXTRA_ENEMIES_R1_D)
                                              OLW_ENEMIES_DOOR_CHASE_D, OLW_ITEMS_SUPPLY_DROPS_D, OLW_ENEMIES_ENEMIES_RETURN_D };
static const float kMoveK[4] = { 1.0f, 1.25f, 1.5f, 2.0f };
static const float kMedK[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
static const char *const kRegenRate[4] = { NULL, "15", "18.75", "30" };  // Playable Eileen [Defense] RegenRate
static volatile LONG g_gp[GP_N];
static LONG g_snapGp[GP_N];
static volatile float g_moveK = 1.0f, g_medK = 1.0f;  // read by the hooks
static float g_r302Walk = 10.5f;                      // Room 302 walk (the two fmul operands read it): 10.5 x Movement Speed
static volatile LONG g_ohk;
static volatile LONG g_dpLevel;  // v3.27.0: Death Presence 0 Off / 1 Weak / 2 Strong (deathpresence.inc reads it)
static volatile LONG g_dropLevel;  // SUPPLY DROPS: 0 Off / 1 Rare / 2 Normal / 3 Abundant (drops.inc reads it; v3.38 DR338: was 0 Off / 1 Normal / 2 Generous)
static void DpLevelChanged(int v);  // deathpresence.inc: the follow distances follow the level
static volatile LONG g_aiLevel;  // v3.29 AFTERIMAGE: After-image 0 Off / 1 On (afterimage.inc reads it); v3.30 AS330: 1 Short / 2 Medium / 3 Long
static volatile LONG g_echoMode;  // v3.30 ECHO: Echo Double 0 Off / 1 Shadow / 2 Twin / 3 Mirror (echo.inc reads it)
static int g_echoReady;           // v3.30 ECHO: echo.inc is installed (else the row says it is unavailable)
static volatile LONG g_echoCountV;  // v3.30 ECHO330: Echo Count 0..2 = 1..3 shadows (echo.inc reads it)
static void EchoIniAlias(char *buf, size_t n);  // v3.30 ECHO330: echo.inc: v3.29's Echo values read as Shadow Copy
static void EnmSettingChanged(int k, int v);  // v3.31 ENEMIES: enemies.inc (Enemy Copies / Enemies Follow values)
static int g_gpUnsaved[GP_N];  // review REV-325-013: the menu's value could not be written: the ini's is not an outside edit
// review REV-325-015: one lock around every [Gameplay] value change (the menu's GpSet with its ini write, and the
// ini poll's GpIniReread), so the fallback watcher thread never sees a half-done menu change
static CRITICAL_SECTION g_gpLock;
static int g_gpLockOn;
static void GpLock(void) { if (g_gpLockOn) EnterCriticalSection(&g_gpLock); }
static void GpUnlock(void) { if (g_gpLockOn) LeaveCriticalSection(&g_gpLock); }
static int g_moveOn, g_medOn, g_ohkOn;                // their hooks are installed
static int g_gpMenu;                                  // Option 5 has the Player / Enemies rows
static int g_peLoaded;                                // Playable Eileen (dsound.dll with SH4PE_Patches) is loaded: the
                                                      // Player page has Regeneration (checked as Options opens; REV-003/6)
static const DWORD *g_gpExit;                         // Option 1's stock Exit record (the Back rows are copies)
static DWORD *g_gpPlayerTab;                          // the Player page's table (rebuilt when g_peLoaded changes)
static int HasExport(HMODULE mod, const char *name);  // sh4windowmod.c
static char g_peIni[MAX_PATH];
static char g_snapPeRegen[256], g_snapPeRate[256];    // PE's two keys as Options opened ("\x01" = missing)
static int g_regenDiff;                               // review REV-325-002: PE's keys now differ from that snapshot
static int g_regenLock;                               // review REV-325-005: a key too long to restore exactly: row inert
static int g_gpPlayerRow[GP_N + PC_COUNT], g_gpPlayerN;  // the Player page: row -> GP index (PLAYER PACK: GP_N + k = its row k)
// v3.37 MN337: Option 5's top level, row by row as GpBuildPages lays it out (-1 = not there), and the folders' row -> GP
// index tables (-1 = a row that answers for itself: HUD BARS, Back)
enum { GPT_NC, GPT_CURSE, GPT_TB, GPT_STACK, GPT_SHINE, GPT_PLAYER, GPT_ENEMY, GPT_SPECIAL, GPT_RANDO, GPT_EXIT, GPT_N };  // v3.48 RZ348: + GPT_RANDO
static int g_gpTop[GPT_N] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };  // v3.48 RZ348: 10 entries
static int g_gpEnemyRow[8], g_gpEnemyN, g_gpEnemyFollow = -1;  // Enemies Follow's row (GpLongValW)  // v3.50 MERGE: 7 of 8 used (rows 0 .. 6; Back has none)
static int g_gpSpecialRow[8], g_gpSpecialN;
static int g_gpSpecialGt = -1;  // v3.43 GT343: Ghost Teleport's row on the Special page (GpBuildPages)
// g_lpParent / g_lpParentRow (sh4windowmod.c): hidden page -> the page / row that opens it (-1 = none)

static BYTE g_txtGp[GPM_N][56];
static DWORD g_gpIds[GP_N][GP_VMAX];
// HUD BARS (hudbars.inc, included later): its two rows share this file's pages, snapshot, ini poll and lock
static void HbInit(void);
static void HbSnapshot(void);
static int HbChanged(void);
static int HbRowChanged(int page, int row);
static void HbDiscard(void);
static void HbIniReread(void);
static int HbPlayerRow(DWORD *t, int n);
static int HbEnemyRow(DWORD *t, int n);
static const BYTE *GpText(int id)
{
    if (id >= MSG_PC0) return PcText(id);  // PLAYER PACK (MsgText asks HbText first for the HUD BARS block after it)
    return id >= MSG_GP0 && id < MSG_GP0 + GPM_N ? g_txtGp[id - MSG_GP0] : g_txtGp[0];
}

static float R3WalkK(void);  // v3.50 R302: room302.inc (Room 302's Walk Speed while Refined owns the walk, in stage 1; else 1)
static void GpApply(int k, int v)  // the hooks' globals (any thread)
{
    if (k == GP_MOVE) {
        g_moveK = kMoveK[v];
        g_r302Walk = 10.5f * kMoveK[v] * R3WalkK();  // v3.50 R302: x Room 302's Walk Speed
    }
    else if (k == GP_MED) g_medK = kMedK[v];
    else if (k == GP_OHK) InterlockedExchange(&g_ohk, v);
    else if (k == GP_DP) {
        InterlockedExchange(&g_dpLevel, v);
        DpLevelChanged(v);
    }
    else if (k == GP_AI) InterlockedExchange(&g_aiLevel, v);  // v3.29 AFTERIMAGE
    else if (k == GP_ECHO) InterlockedExchange(&g_echoMode, v);  // v3.30 ECHO
    else if (k == GP_ECHON) InterlockedExchange(&g_echoCountV, v);  // v3.30 ECHO330
    else if (k == GP_COPIES || k == GP_FOLLOW || k == GP_REVIVE) EnmSettingChanged(k, v);  // v3.31 ENEMIES  // v3.30 EN330: + GP_REVIVE
    else if (k == GP_DROP) InterlockedExchange(&g_dropLevel, v);  // SUPPLY DROPS
}

// Playable Eileen's regeneration as the row shows it: 0 Off, 1..3 the standard rates, 4 Custom
static int GpRegenRead(void)
{
    char on[16], rate[16];
    double r;
    int v;
    GetPrivateProfileStringA("Defense", "Regen", "1", on, sizeof on, g_peIni);  // PE's defaults when missing
    GetPrivateProfileStringA("Defense", "RegenRate", "15", rate, sizeof rate, g_peIni);
    if (atof(on) == 0.0) return 0;
    r = atof(rate);
    for (v = 1; v < 4; ++v)
        if (fabs(r - atof(kRegenRate[v])) < 0.01) return v;
    return 4;
}

static void GpSetLocked(int k, int v);
static void GpSet(int k, int v)
{
    GpLock();
    GpSetLocked(k, v);
    GpUnlock();
}
static void GpSetLocked(int k, int v)
{
    if (k < 0 || k >= GP_N || v < 0 || v >= (k == GP_REGEN ? 5 : kGpCount[k])) return;
    InterlockedExchange(&g_gp[k], v);
    if (k == GP_REGEN) {
        if (v == 4) return;  // Custom is only shown, never written
        // review REV-325-008: each write is checked; on a failure the row shows what the file really holds
        if (!WritePrivateProfileStringA("Defense", "Regen", v ? "1" : "0", g_peIni) ||
            (v && !WritePrivateProfileStringA("Defense", "RegenRate", kRegenRate[v], g_peIni))) {
            InterlockedExchange(&g_gp[k], GpRegenRead());
            Log("gameplay: SH4PlayableEileen.ini could not be written (error %lu): regeneration stays %s", GetLastError(),
                kGpName[k][g_gp[k]]);
        }
        {   // review REV-325-002: changed = the key texts differ from the snapshot (not only the shown value)
            char a[256], b[256];
            GetPrivateProfileStringA("Defense", "Regen", "\x01", a, sizeof a, g_peIni);
            GetPrivateProfileStringA("Defense", "RegenRate", "\x01", b, sizeof b, g_peIni);
            g_regenDiff = strcmp(a, g_snapPeRegen) || strcmp(b, g_snapPeRate);
        }
        Log("gameplay: Playable Eileen's regeneration %s (SH4PlayableEileen.ini [Defense] Regen%s%s)", kGpName[k][v],
            v ? ", RegenRate " : "", v ? kRegenRate[v] : "");
        return;
    }
    GpApply(k, v);
    g_gpUnsaved[k] = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", kGpIniKey[k], kGpName[k][v], g_iniPath)) {  // REV-325-010
        g_gpUnsaved[k] = 1;  // REV-325-013: GpIniReread leaves this key alone until it is saved or set again
        Log("gameplay: %s %s applies now but could not be saved in SH4Refined.ini (error %lu): it is lost at the next start",
            kGpIniKey[k], kGpName[k][v], GetLastError());
    } else {
        Log("gameplay: %s %s", kGpIniKey[k], kGpName[k][v]);
    }
}
static int GpInput(int k)
{
    int v;
    GpLock();  // review REV-325-016: the whole read-modify-write under the lock (the critical section is re-entrant)
    v = StepValue((int)g_gp[k], kGpCount[k]);  // from Custom (4): right -> x1, left -> x2
    if (v >= 0) GpSetLocked(k, v);
    GpUnlock();
    return 0;
}
static int __cdecl GpIn0(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(0); }
static int __cdecl GpIn1(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(1); }
static int __cdecl GpIn2(BYTE *cfg, int count)  // v3.47 RG347: by the row's mode (RGR_PE: inert, grey Off)
{
    const int m = RgRowModeNow();
    (void)cfg;
    (void)count;
    if (m == RGR_OWN) return RgRowIn();
    return m == RGR_LEGACY && g_peLoaded && !g_regenLock ? GpInput(2) : 0;
}
static int __cdecl GpIn3(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(3); }
static int __cdecl GpIn4(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(4); }
static int __cdecl GpIn5(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(5); }  // v3.29 AFTERIMAGE
static int __cdecl GpIn6(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_ECHO); }  // v3.30 ECHO (v3.29 merge: 5 -> 6)
static int __cdecl GpInEchoN(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_ECHON); }  // v3.30 ECHO330
static int __cdecl GpInCopies(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_COPIES); }  // v3.31 ENEMIES
static int __cdecl GpInFollow(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_FOLLOW); }  // v3.31 ENEMIES
static int __cdecl GpInDrop(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_DROP); }  // SUPPLY DROPS (v3.29 merge: GpIn5 -> GpInDrop)
static int __cdecl GpInRevive(BYTE *cfg, int count) { (void)cfg; (void)count; return GpInput(GP_REVIVE); }  // v3.30 EN330
static int __cdecl GpGet0(void) { return (int)g_gp[0]; }
static int __cdecl GpGet1(void) { return (int)g_gp[1]; }
static int __cdecl GpGet2(void)  // v3.47 RG347: Refined's value, Off while PE's own heals, else PE's ini value (v3.46)
{
    const int m = RgRowModeNow();
    return m == RGR_OWN ? RgRowGet() : m == RGR_PE ? 0 : (int)g_gp[2];
}
static int __cdecl GpGet3(void) { return (int)g_gp[3]; }
static int __cdecl GpGet4(void) { return (int)g_gp[4]; }
static int __cdecl GpGet5(void) { return (int)g_gp[5]; }  // v3.29 AFTERIMAGE
static int __cdecl GpGet6(void) { return (int)g_gp[GP_ECHO]; }  // v3.30 ECHO (v3.29 merge: 5 -> 6)
static int __cdecl GpGetEchoN(void) { return (int)g_gp[GP_ECHON]; }  // v3.30 ECHO330
static int __cdecl GpGetCopies(void) { return (int)g_gp[GP_COPIES]; }  // v3.31 ENEMIES
static int __cdecl GpGetFollow(void) { return (int)g_gp[GP_FOLLOW]; }  // v3.31 ENEMIES
static int __cdecl GpGetDrop(void) { return (int)g_gp[GP_DROP]; }  // SUPPLY DROPS (v3.29 merge: GpGet5 -> GpGetDrop)
static int __cdecl GpGetRevive(void) { return (int)g_gp[GP_REVIVE]; }  // v3.30 EN330
static int __cdecl GpDesc0(void) { return g_gp[GP_MOVE] == 3 ? MSG_V53 + OLT_MS_VD_X2 : MSG_GP0 + GPM_D0 + 0; }  // v3.53 OL1: x2's own line
static int __cdecl GpDesc1(void) { return MSG_GP0 + GPM_D0 + 1; }
static int __cdecl GpDesc2(void)  // v3.47 RG347: by the row's mode
{
    const int m = RgRowModeNow();
    return m == RGR_OWN ? RgRowDesc() : m == RGR_PE ? RgRowDescPe() : MSG_GP0 + GPM_D0 + 2;
}
static int __cdecl GpDesc3(void) { return g_gp[GP_OHK] ? MSG_V53 + OLT_OHK_VD_ON : MSG_GP0 + GPM_D0 + 3; }  // v3.53 OL1: On's own line
static int g_dpReady;  // deathpresence.inc: the feature is installed (else the row says it is unavailable)
static int __cdecl GpDesc4(void)  // v3.27.0: one line per value (Off: what it is)
{
    const int v = (int)g_gp[4];
    if (!g_dpReady) return MSG_GP0 + GPM_DPNA;
    return v == 1 ? MSG_GP0 + GPM_DPD1 : v == 2 ? MSG_GP0 + GPM_DPD2 : MSG_GP0 + GPM_D0 + 4;
}
static int g_aiReady;  // v3.29 AFTERIMAGE: afterimage.inc: the feature is installed (else the row says it is unavailable)
static int __cdecl GpDesc5(void)  // v3.29 AFTERIMAGE: Off: what it is; On: what it does (v3.30 AS330: one line per length)
{
    const int v = (int)g_gp[5];
    if (!g_aiReady) return MSG_GP0 + GPM_DPNA;
    return v == 1 ? MSG_GP0 + GPM_AID1 : v == 2 ? MSG_GP0 + GPM_AID2 : v == 3 ? MSG_GP0 + GPM_AID3 : MSG_GP0 + GPM_D0 + 5;
}
static int __cdecl GpDesc6(void)  // v3.30 ECHO: one line per value (Off: what it is) (v3.29 merge: 5 -> 6)
{
    const int v = (int)g_gp[GP_ECHO];
    if (!g_echoReady) return MSG_GP0 + GPM_DPNA;
    return v >= 1 && v <= 2 ? MSG_GP0 + GPM_ECD1 + v - 1 : MSG_GP0 + GPM_D0 + GP_ECHO;  // v3.30 ECHO330: Shadow Copy, Shadow Cast
}
static int __cdecl GpDescEchoN(void)  // v3.30 ECHO330: Echo Count (while Echo Doubles is Off: say so)
{
    if (!g_echoReady) return MSG_GP0 + GPM_DPNA;
    return g_gp[GP_ECHO] ? MSG_GP0 + GPM_D0 + GP_ECHON : MSG_GP0 + GPM_ECD3;
}
static int g_enmCopiesReady, g_enmFollowReady;  // v3.31 ENEMIES: enemies.inc (installed; else the rows say unavailable)
static int __cdecl GpDescCopies(void) { return g_enmCopiesReady ? MSG_GP0 + GPM_D0 + GP_COPIES : MSG_GP0 + GPM_DPNA; }  // v3.31 ENEMIES
static int __cdecl GpDescFollow(void) { return g_enmFollowReady ? MSG_GP0 + GPM_D0 + GP_FOLLOW : MSG_GP0 + GPM_DPNA; }  // v3.31 ENEMIES
static int g_enmReviveReady;  // v3.30 EN330: enemies.inc (its room load hook is in)
static int __cdecl GpDescRevive(void)  // v3.30 EN330  // v3.56 ER356: one line per value (the layout's pool: OLT_ER_VD_*)
{
    const int v = (int)g_gp[GP_REVIVE];
    if (!g_enmReviveReady) return MSG_GP0 + GPM_DPNA;
    return v == 1 ? MSG_V53 + OLT_ER_VD_ROOM : v == 2 ? MSG_V53 + OLT_ER_VD_WORLD : MSG_V53 + OLT_ER_VD_OFF;
}
static int g_dropReady;  // SUPPLY DROPS: drops.inc is installed (else the row says it is unavailable)
static int __cdecl GpDescDrop(void)  // SUPPLY DROPS: one line per value (Off: what it is) (v3.29 merge: GpDesc5 -> GpDescDrop)
{
    const int v = (int)g_gp[GP_DROP];
    if (!g_dropReady) return MSG_GP0 + GPM_DRNA;
    return v == 1 ? MSG_GP0 + GPM_DRD3 : v == 2 ? MSG_GP0 + GPM_DRD1 : v == 3 ? MSG_GP0 + GPM_DRD2 : MSG_GP0 + GPM_D0 + GP_DROP;  // v3.38 DR338: Rare, Normal, Abundant
}
static int __cdecl GpDescSpecial(void) { return MSG_GP0 + GPM_D_SPECIAL; }
static int __cdecl GpDescBack(void) { return MSG_GP0 + GPM_D_BACK; }
static int __cdecl GpDescPlayer(void) { return MSG_GP0 + GPM_D_PLAYER; }
static int __cdecl GpDescEnemy(void) { return MSG_GP0 + GPM_D_ENEMY; }
// Back: Enter fades back to Option 5, cursor on the row that opened the page
static int __cdecl GpBackIn(BYTE *cfg, int count)
{
    (void)cfg;
    (void)count;
    if (!FcClickOk()) return 0;  // v3.56 MN356: Enter, or a click on the row (was Enter only)
    InterlockedExchange(&g_subReq, REQ_BACK);
    ((Fn1)0x56CC40)(0x9C42);  // the stock confirm sound (0x5261CB)
    return SubPageExit();
}
// Option 5's Player / Enemies rows: Enter opens the page (fade route only: the rows exist only with g_subScreenOn)
static int GpOpen(int lp)
{
    if (!g_subScreenOn || !FcClickOk()) return 0;  // v3.56 MN356: Enter, or a click on the row (was Enter only: issue 1)
    InterlockedExchange(&g_subReq, REQ_OPEN | lp);
    ((Fn1)0x56CC40)(0xA411);  // the stock sub-screen sound (Button Config, 52495B)
    return SubPageExit();
}
static int __cdecl GpPlayerIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_PLAYER); }  // v3.54 OL34: the new ids (layout.inc opens the folders itself)
static int __cdecl GpEnemyIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_ENEMIES); }  // v3.54 OL34
static int __cdecl GpSpecialIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_POWERS); }  // v3.27.0  // v3.54 OL34

// snapshot / changed / discard (ResetPages, PadChanged, RowChangedDraw, DiscardOwn)
static void GpBuildPlayer(void);
static void GpSnapshot(void)
{
    int k;
    if (BldNeedSanitize()) BldSanitize();  // review REV-008 / REV-325-009 (also without the 0x4EF982 dispatcher)
    {   // review REV-003 / REV-006: the row follows the loaded plugin (its ini may outlive it, or not exist yet at Init)
        const HMODULE ds = GetModuleHandleA("dsound.dll");
        const int was = g_peLoaded, now = ds && HasExport(ds, "SH4PE_Patches");
        g_peLoaded = now;
        if (now != was && g_gpPlayerTab) GpBuildPlayer();  // safe here: Options is opening, no hidden page is loaded
        if (now != was) Log("gameplay: Playable Eileen %s", now ? "is loaded" : "is not loaded");  // v3.47 RG347: the Regeneration row is always there
    }
    RgPeLook(1);  // v3.47 RG347: the row's mode follows PE's exports
    if (g_peLoaded && RgRowModeNow() == RGR_LEGACY) {  // v3.47 RG347: PE's ini keys only while the row is their front end
        char d[56];
        const int delay = (int)GetPrivateProfileIntA("Defense", "RegenDelay", 30, g_peIni);  // review REV-010
        sprintf_s(d, sizeof d, OLW_REGEN_PE_LEGACY, delay < 10 ? 10 : delay > 120 ? 120 : delay);  // v3.57 OLP1: the generated line ("Playable Eileen heals after %d seconds unhurt."; was a literal of v3.47)
        Encode(g_txtGp[GPM_D0 + GP_REGEN], 56, d);
        InterlockedExchange(&g_gp[GP_REGEN], GpRegenRead());  // PE's ini may have been edited since
        g_regenLock = GetPrivateProfileStringA("Defense", "Regen", "\x01", g_snapPeRegen, sizeof g_snapPeRegen, g_peIni) >=
                          sizeof g_snapPeRegen - 1 ||
                      GetPrivateProfileStringA("Defense", "RegenRate", "\x01", g_snapPeRate, sizeof g_snapPeRate, g_peIni) >=
                          sizeof g_snapPeRate - 1;
        if (g_regenLock) Log("gameplay: Playable Eileen's [Defense] Regen / RegenRate text is too long to restore exactly: the row is inert");
    }
    GpLock();  // REV-325-015: the poll writes the snapshot too
    for (k = 0; k < GP_N; ++k) g_snapGp[k] = g_gp[k];
    g_regenDiff = 0;
    HbSnapshot();  // HUD BARS
    NcSnapshot();  // v3.32 NC332
    InvSnapshot();  // v3.37 INV337
    GtSnapshot();  // v3.43 GT343
    OmSnapshot();  // v3.46 OM346
    RgSnapshot();  // v3.47 RG347
    RzSnapshot();  // v3.48 RZ348
    EfSnapshot();  // v3.50 EF350
    FiSnapshot();  // v3.50 FI350
    GpUnlock();
    PcSnapshot();  // PLAYER PACK
}
static int GpChanged(void)
{
    int k;
    if (!g_snapOn || !g_gpMenu) return 0;
    for (k = 0; k < GP_N; ++k)
        if (g_gp[k] != g_snapGp[k]) return 1;
    return g_regenDiff || PcChanged() || HbChanged() || NcChanged() || InvChanged() || GtChanged() ||  // PLAYER PACK; HUD BARS  // v3.32 NC332  // v3.37 INV337  // v3.43 GT343  // v3.47 CT347: OmChanged moved to PadChanged (the row is on Option 2)
           RgChanged() || RzChanged() || EfChanged() || FiChanged();  // v3.47 RG347  // v3.48 RZ348  // v3.50 EF350  // v3.50 FI350  // v3.50 MERGE: both
}
static int GpRowChanged(int page, int row)  // -1 = not one of these pages
{
    int k;
    if ((k = FiRowChanged(page, row)) >= 0) return k;  // v3.50 FI350: Fade In (Enemies)
    if ((k = HbRowChanged(page, row)) >= 0) return k;  // HUD BARS: Enemy Health Bars (Enemies), Curse Meter (v3.37 MN337: Option 5)
    if (page == 4) {  // v3.37 MN337: Option 5's top level: No Clip; Item Shine, the folders and Exit answer elsewhere (-1)
        if (!g_gpMenu || row < 0) return -1;
        if (row == g_gpTop[GPT_NC]) return NcRowChanged();
        if (row == g_gpTop[GPT_TB]) return InvRowChanged(0);  // v3.37 MERGE: W2 INV337 (0 = Extended Toolbelt)
        if (row == g_gpTop[GPT_STACK]) return InvRowChanged(1);  // v3.37 MERGE: 1 = Stackable Items
        return -1;
    }
    if (page == OP_RANDO) return RzRowChanged(row);  // v3.48 RZ348
    if (page == OP_PLAYER) {
        if (row < 0 || row >= g_gpPlayerN) return 0;
        k = g_gpPlayerRow[row];
        if (k < 0) return 0;  // v3.46 OM346 (v3.47 CT347: no mark today; Stick Movement left this page)
        if (k >= GP_N) return PcRowChanged(k - GP_N);  // PLAYER PACK
    } else if (page == OP_ENEMY) {  // v3.37 MN337: by the table GpBuildPages recorded (was a fixed kRow[6] with One-Hit Kill at row 0)
        if (row >= 0 && row == g_efPursuitRow) return EfRowChanged();  // v3.50 EF350
        if (row < 0 || row >= g_gpEnemyN || g_gpEnemyRow[row] < 0) return 0;
        k = g_gpEnemyRow[row];
    } else if (page == OP_SPECIAL) {  // v3.27.0; v3.37 MN337: by the recorded table (No Clip left this page)
        if (row >= 0 && row == g_gpSpecialGt) return GtRowChanged();  // v3.43 GT343
        if (row < 0 || row >= g_gpSpecialN || g_gpSpecialRow[row] < 0) return 0;
        k = g_gpSpecialRow[row];
    } else {
        return -1;
    }
    if (k == GP_REGEN && RgRowModeNow() != RGR_LEGACY) return RgRowChanged();  // v3.47 RG347: Refined's own value
    return g_snapOn && (g_gp[k] != g_snapGp[k] || (k == GP_REGEN && g_regenDiff));
}
static void GpDiscard(void)
{
    int k;
    if (!g_snapOn) return;
    GpLock();  // review REV-325-016: every compare with the snapshot and every restore under the lock
    for (k = 0; k < GP_N; ++k) {
        if (g_gp[k] == g_snapGp[k] && !(k == GP_REGEN && g_regenDiff)) continue;
        if (k == GP_REGEN) {  // PE's keys go back to the exact text they had (or away, if they were missing)
            if (!g_peLoaded) continue;
            g_regenDiff = 0;
            if (!WritePrivateProfileStringA("Defense", "Regen", strcmp(g_snapPeRegen, "\x01") ? g_snapPeRegen : NULL, g_peIni) ||
                !WritePrivateProfileStringA("Defense", "RegenRate", strcmp(g_snapPeRate, "\x01") ? g_snapPeRate : NULL, g_peIni))
                Log("gameplay: SH4PlayableEileen.ini could not be restored (error %lu): check its [Defense] Regen / RegenRate",
                    GetLastError());  // review REV-325-008
            InterlockedExchange(&g_gp[k], GpRegenRead());
        } else {
            GpSetLocked(k, g_snapGp[k]);
        }
    }
    HbDiscard();  // HUD BARS
    NcDiscard();  // v3.32 NC332
    InvDiscard();  // v3.37 INV337
    GtDiscard();  // v3.43 GT343
    OmDiscard();  // v3.46 OM346
    RgDiscard();  // v3.47 RG347
    RzDiscard();  // v3.48 RZ348
    EfDiscard();  // v3.50 EF350
    FiDiscard();  // v3.50 FI350
    GpUnlock();
    PcDiscard();  // PLAYER PACK
}
static int GpSubmenuRow(int page, int row)  // v3.27.0: + Special; v3.37 MN337: the folder rows where GpBuildPages put them
{
    return g_gpMenu && page == 4 && row >= 0 && (row == g_gpTop[GPT_PLAYER] || row == g_gpTop[GPT_ENEMY] || row == g_gpTop[GPT_SPECIAL] ||
                                                 row == g_gpTop[GPT_RANDO]);  // v3.48 RZ348
}

// The texts and the [Gameplay] values (before the Options tables are built)
static void GpTexts(void)
{
    int k, v;
    // v3.53 OL1: the plan's words; the titles are the page names (OL2 draws "Gameplay > ..."); Special -> Powers
    Encode(g_txtGp[GPM_T_PLAYER], 56, OLW_T_PLAYER);
    Encode(g_txtGp[GPM_T_ENEMY], 56, OLW_T_ENEMIES);
    Encode(g_txtGp[GPM_R_PLAYER], 56, OLW_GAMEPLAY_PLAYER_L);
    Encode(g_txtGp[GPM_R_ENEMY], 56, OLW_GAMEPLAY_ENEMIES_L);
    Encode(g_txtGp[GPM_D_PLAYER], 56, OLW_GAMEPLAY_PLAYER_D);  // PLAYER PACK (was: Movement, medallion and healing cheats.)  // v3.53 OL1: the plan's Player + Combat  // v3.57 OLP1: the Player folder's own line (Codex OLP1-002: was OLW_PLAYER_FOLDER_R1_D, which named Combat's damage and ammo)
    Encode(g_txtGp[GPM_D_ENEMY], 56, OLW_GAMEPLAY_ENEMIES_D);  // v3.37 MN337 (One-Hit Kill moved to Player)
    Encode(g_txtGp[GPM_BACK], 56, OLW_BACK);
    Encode(g_txtGp[GPM_D_BACK], 56, OLW_BACK_GAMEPLAY);  // v3.53 OL1: was "Back to Option 5 (Gameplay)."
    Encode(g_txtGp[GPM_T_SPECIAL], 56, OLW_T_POWERS);  // v3.27.0
    Encode(g_txtGp[GPM_R_SPECIAL], 56, OLW_GAMEPLAY_POWERS_L);
    Encode(g_txtGp[GPM_D_SPECIAL], 56, OLW_GAMEPLAY_POWERS_D);
    Encode(g_txtGp[GPM_DPD1], 56, OLW_POWERS_DEATH_PRESENCE_VD_WEAK);
    Encode(g_txtGp[GPM_DPD2], 56, OLW_POWERS_DEATH_PRESENCE_VD_STRONG);
    Encode(g_txtGp[GPM_DPNA], 56, OLW_NA_GAME);
    Encode(g_txtGp[GPM_AID1], 56, OLW_POWERS_AFTER_SHOCK_VD_SHORT);  // v3.29 AFTERIMAGE (v3.30 AS330)
    Encode(g_txtGp[GPM_AID2], 56, OLW_POWERS_AFTER_SHOCK_VD_MEDIUM);  // v3.30 AS330
    Encode(g_txtGp[GPM_AID3], 56, OLW_POWERS_AFTER_SHOCK_VD_LONG);  // v3.30 AS330
    Encode(g_txtGp[GPM_ECD1], 56, OLW_POWERS_ECHO_DOUBLES_VD_COPY);  // v3.30 ECHO330 (was the three v3.29 lines)
    Encode(g_txtGp[GPM_ECD2], 56, OLW_POWERS_ECHO_DOUBLES_VD_CAST);
    Encode(g_txtGp[GPM_ECD3], 56, "How many shadows (turn Echo Doubles on first).");  // v3.53 OL1: kept (OL2 owns the grey rows' reason lines)
    Encode(g_txtGp[GPM_DRD1], 56, OLW_ITEMS_SUPPLY_DROPS_VD_NORMAL);  // SUPPLY DROPS
    Encode(g_txtGp[GPM_DRD2], 56, OLW_ITEMS_SUPPLY_DROPS_VD_ABUNDANT);  // v3.38 DR338 (was Generous)
    Encode(g_txtGp[GPM_DRD3], 56, OLW_ITEMS_SUPPLY_DROPS_VD_RARE);      // v3.38 DR338
    Encode(g_txtGp[GPM_DRNA], 56, OLW_NA_GAME);
    for (k = 0; k < GP_N; ++k) {
        const int map = k == GP_ECHO ? OLM_POWERS_ECHO_DOUBLES : k == GP_FOLLOW ? OLM_ENEMIES_DOOR_CHASE : -1;  // v3.53 OL1: Copy / Cast, Both on screen
        Encode(g_txtGp[GPM_L0 + k], 56, kGpLabel[k]);
        Encode(g_txtGp[GPM_D0 + k], 56, kGpDescTxt[k]);
        for (v = 0; v < GP_VMAX; ++v) {
            Encode(g_txtGp[GPM_V0 + k * GP_VMAX + v], 56, kGpName[k][v] ? OlShow(map, kGpName[k][v]) : "");
            g_gpIds[k][v] = MSG_GP0 + GPM_V0 + k * GP_VMAX + v;
        }
    }
    for (v = 3; v < BLD_N; ++v) {
        char d[56], lower[16];
        size_t i;
        strcpy_s(lower, sizeof lower, kBldName[v]);
        for (i = 0; lower[i]; ++i) lower[i] = (char)tolower((unsigned char)lower[i]);
        sprintf_s(d, sizeof d, "%s: %s blood.", kBldName[v], lower);  // v3.56 MN356: house style "Value: ..." (was "Makes blood colour %s."; layout_data.py's lines)
        Encode(g_txtGp[GPM_BLDV + v - 3], 56, kBldName[v]);
        Encode(g_txtGp[GPM_BLDD + v - 3], 56, d);
    }
    PcTexts();  // PLAYER PACK
    NcTexts();  // v3.32 NC332
    GtTexts();  // v3.43 GT343
    OmTexts();  // v3.46 OM346
    EfTexts();  // v3.50 EF350
}
// v3.30 MERGE: the one helper for old [Gameplay] values that still read (AS330 + ECHO330). It rewrites buf to today's
// value name before the lookup, each case logged once; the ini keeps its text until the row is changed.
// AS330: v3.29's After-image "On" = After Shock Medium. ECHO330: v3.29's Echo Shadow / Twin / Mirror = Shadow Copy
// (echo.inc EchoIniAlias, which logs it).
static int DropIniAlias(char *buf, size_t n);  // v3.38 DR338: drops.inc (pure part): "Generous" -> "Abundant"
static void GpLegacyValue(int k, char *buf, size_t n)
{
    static int s_logged, s_dropLogged;  // v3.38 DR338: + s_dropLogged
    if (k == GP_AI && !_stricmp(buf, "On")) {  // v3.30 AS330
        if (!s_logged++) Log("gameplay: AfterImage On (v3.29) reads as Medium (After Shock: Off, Short, Medium or Long)");
        strcpy_s(buf, n, kGpName[GP_AI][2]);
    } else if (k == GP_ECHO) {
        EchoIniAlias(buf, n);  // v3.30 ECHO330
    } else if (k == GP_FOLLOW && !_stricmp(buf, "Victims Only")) {  // v3.50 EF350: v3.49's name of the value
        static int s_folLogged;
        if (!s_folLogged++) Log("gameplay: EnemyFollow Victims Only (v3.49) reads as Victims (the same value, renamed)");
        strcpy_s(buf, n, kGpName[GP_FOLLOW][2]);
    } else if (k == GP_DROP && DropIniAlias(buf, n) && !s_dropLogged++) {  // v3.38 DR338
        Log("gameplay: SupplyDrops Generous (v3.29 - v3.37) reads as Abundant (Supply Drops: Off, Rare, Normal or Abundant; "
            "the same chances)");
    } else if (k == GP_REVIVE) {  // v3.56 ER356: v3.30 - v3.55's On = By Room (GpReadIni writes it back once and logs it)
        int legacy;
        const int v = ErParse(buf, &legacy);
        if (legacy) strcpy_s(buf, n, kGpName[GP_REVIVE][v]);
    }
}
// [Gameplay] read again when SH4Refined.ini changes (items.inc SpIniPoll, once a second, game thread). The menu's own
// writes read back the value already set; a different value is an edit made outside the game, which also becomes the
// Options snapshot (as Item Shine), so "No" at the exit never undoes it.
static void GpIniReread(void)
{
    char buf[32];
    int k, v;
    if (g_iniFallback) return;
    GpLock();
    for (k = 0; k < GP_N; ++k) {
        if (!kGpIniKey[k] || g_gpUnsaved[k]) continue;  // REV-325-013: an unsaved menu value is not undone by the file
        GetPrivateProfileStringA("Gameplay", kGpIniKey[k], kGpName[k][g_gp[k]], buf, sizeof buf, g_iniPath);
        GpLegacyValue(k, buf, sizeof buf);  // v3.30 MERGE: AS330 After-image On, ECHO330 old Echo names
        for (v = kGpCount[k] - 1; v > 0 && _stricmp(buf, kGpName[k][v]); --v) {}
        if (v == g_gp[k]) continue;
        InterlockedExchange(&g_gp[k], v);
        g_snapGp[k] = v;
        GpApply(k, v);
        Log("gameplay: [Gameplay] %s read again: %s", kGpIniKey[k], kGpName[k][v]);
    }
    HbIniReread();  // HUD BARS
    NcIniReread();  // v3.32 NC332
    InvIniReread();  // v3.37 INV337
    GtIniReread();  // v3.43 GT343
    OmIniReread();  // v3.46 OM346: [Input] OmniMove / OmniDeadZone (the same file)
    RgIniReread();  // v3.47 RG347: [Gameplay] Regen / RegenDelay
    RzIniReread();  // v3.48 RZ348: [Randomizer] (the same file)
    EfIniReread();  // v3.50 EF350: [Gameplay] EnemyPursuit
    FiIniReread();  // v3.50 FI350: [Gameplay] FadeIn
    GpUnlock();
    PcIniReread();  // PLAYER PACK
}
// review REV-325-007: [Gameplay]'s own write-time poll (once a second), run from Refined's 0x4EF982 tick dispatcher and
// from the item shine's frame (items.inc SpIniPoll), whichever is in: both on the game thread, neither depends on the other.
static FILETIME g_gpIniTime;
static DWORD g_gpIniCheck;
static void GpIniPoll(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    const DWORD now = GetTickCount();
    if (g_iniFallback || now - g_gpIniCheck < 1000) return;
    g_gpIniCheck = now;
    if (!GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &a)) return;
    if (!CompareFileTime(&a.ftLastWriteTime, &g_gpIniTime)) return;
    g_gpIniTime = a.ftLastWriteTime;
    GpIniReread();
}
static void GpReadIni(void)
{
    char buf[32];
    int k, v;
    for (k = 0; k < GP_N; ++k) {
        int erLegacy = 0;  // v3.56 ER356
        if (!kGpIniKey[k]) continue;
        IniStr("Gameplay", kGpIniKey[k], buf, sizeof buf);
        if (k == GP_REVIVE) ErParse(buf, &erLegacy);  // v3.56 ER356: v3.30 - v3.55's EnemyRevive=On
        GpLegacyValue(k, buf, sizeof buf);  // v3.30 MERGE: AS330 After-image On, ECHO330 old Echo names
        for (v = kGpCount[k] - 1; v > 0 && _stricmp(buf, kGpName[k][v]); --v) {}
        g_gp[k] = v;  // unknown text = the first value (Normal / Off)
        GpApply(k, v);
        if (erLegacy) {  // v3.56 ER356: migrated once (the file says By Room from then on; under the ini fallback read only)
            const int ok = !g_iniFallback && WritePrivateProfileStringA("Gameplay", kGpIniKey[k], kGpName[k][v], g_iniPath);
            Log("gameplay: EnemyRevive On (v3.30 - v3.55) is %s now (Enemies Return: Off, By Room, By World): %s", kGpName[k][v],
                ok ? "written to SH4Refined.ini" : g_iniFallback ? "read only (the ini fallback)"
                                                  : "could not be written to SH4Refined.ini (used for this start; tried again at the next)");
        }
    }
    sprintf_s(g_peIni, sizeof g_peIni, "%sSH4PlayableEileen.ini", g_dir);
    {
        const HMODULE ds = GetModuleHandleA("dsound.dll");
        g_peLoaded = ds && HasExport(ds, "SH4PE_Patches");  // may load after this dll: checked again as Options opens
    }
    if (GetFileAttributesA(g_peIni) != INVALID_FILE_ATTRIBUTES) g_gp[GP_REGEN] = GpRegenRead();
    Log("gameplay: MoveSpeed %s, MedallionDurability %s, OneHitKill %s, DeathPresence %s, AfterImage %s, Echo %s x%s, SupplyDrops %s; "
        "Playable Eileen %s (its regeneration %s)", kGpName[0][g_gp[0]], kGpName[1][g_gp[1]], kGpName[3][g_gp[3]],
        kGpName[4][g_gp[4]], kGpName[5][g_gp[5]], kGpName[GP_ECHO][g_gp[GP_ECHO]], kGpName[GP_ECHON][g_gp[GP_ECHON]], kGpName[GP_DROP][g_gp[GP_DROP]],
        g_peLoaded ? "loaded" : "not loaded yet", kGpName[2][g_gp[2]]);  // v3.29 AFTERIMAGE; v3.30 ECHO: + Echo; SUPPLY DROPS  // v3.30 ECHO330: x EchoCount
    PcReadIni();  // PLAYER PACK
    NcReadIni();  // v3.32 NC332
    GtReadIni();  // v3.43 GT343
    OmReadIni();  // v3.46 OM346
    EfReadIni();  // v3.50 EF350
}

// The rows' handlers by GP index (GpBuildPlayer / GpBuildPages below build Option 5 and its folders from them)
static const void *const kGpIn[GP_N] = { GpIn0, GpIn1, GpIn2, GpIn3, GpIn4, GpIn5, GpIn6,  // v3.29 AFTERIMAGE: + 5; v3.30 ECHO: + 6
                                         GpInEchoN,  // v3.30 ECHO330
                                         GpInCopies, GpInFollow,  // v3.31 ENEMIES: + Copies, Follow (GP_COPIES, GP_FOLLOW order)
                                         GpInDrop,  // SUPPLY DROPS
                                         GpInRevive };  // v3.30 EN330
static const void *const kGpGet[GP_N] = { GpGet0, GpGet1, GpGet2, GpGet3, GpGet4, GpGet5, GpGet6,  // v3.30 ECHO: + 6 (v3.29 merge)
                                          GpGetEchoN,  // v3.30 ECHO330
                                          GpGetCopies, GpGetFollow,  // v3.31 ENEMIES
                                          GpGetDrop,  // SUPPLY DROPS
                                          GpGetRevive };  // v3.30 EN330
static const void *const kGpDesc[GP_N] = { GpDesc0, GpDesc1, GpDesc2, GpDesc3, GpDesc4, GpDesc5, GpDesc6,  // v3.30 ECHO: + 6 (v3.29 merge)
                                           GpDescEchoN,  // v3.30 ECHO330
                                           GpDescCopies, GpDescFollow,  // v3.31 ENEMIES
                                           GpDescDrop,  // SUPPLY DROPS
                                           GpDescRevive };  // v3.30 EN330
// The Player page's table (v3.37 MN337: One-Hit Kill first, from Enemies; Curse Meter left for Option 5): One-Hit Kill,
// Movement Speed, Medallion Durability, Regeneration (v3.47 RG347: always; was only while Playable Eileen is loaded),
// PLAYER PACK: Ammo Saver, Dark Pact, Grab Breaker, Quick Charge (v3.43 QC343), Life Steal; Back (g_rowsPlayer holds 11:
// 10 rows used, with or without Playable Eileen)
// (v3.47 CT347: Stick Movement, v3.46's row below Movement Speed, moved to Option 2 (Control); v3.47 merge: with RG347's
// Regeneration always here the page has 10 rows, v3.45's count with Playable Eileen)
static void GpBuildPlayer(void)
{
    DWORD *const t = g_gpPlayerTab;
    int n = 0, k;
    g_gpPlayerRow[n] = GP_OHK;  // v3.37 MN337: the top row
    FillRow(t + 7 * n++, kGpIn[GP_OHK], kGpGet[GP_OHK], g_gpIds[GP_OHK], kGpCount[GP_OHK], MSG_GP0 + GPM_L0 + GP_OHK, kGpDesc[GP_OHK]);
    for (k = 0; k < GP_N; ++k) {
        if (k == GP_OHK || k == GP_DP || k == GP_AI || k == GP_ECHO || k == GP_ECHON || k == GP_COPIES || k == GP_FOLLOW || k == GP_DROP ||
            k == GP_REVIVE) continue;  // v3.47 RG347: Regeneration always (was only while Playable Eileen is loaded)  // v3.29 AFTERIMAGE: + GP_AI; v3.30 ECHO: not here; v3.31 ENEMIES; SUPPLY DROPS: + GP_DROP  // v3.30 ECHO330: + GP_ECHON  // v3.30 EN330: + GP_REVIVE  // v3.37 MN337: GP_OHK above
        g_gpPlayerRow[n] = k;
        FillRow(t + 7 * n++, kGpIn[k], kGpGet[k], g_gpIds[k], k == GP_REGEN ? 5 : kGpCount[k], MSG_GP0 + GPM_L0 + k, kGpDesc[k]);
    }
    for (k = 0; k < PC_COUNT; ++k) {  // PLAYER PACK: Ammo Saver, Dark Pact, Grab Breaker, Quick Charge, Life Steal  // v3.43 QC343
        g_gpPlayerRow[n] = GP_N + k;
        PcFillRow(t + 7 * n++, k);
    }
    // v3.37 MN337: (HUD BARS' Curse Meter is on Option 5's top level now)
    memcpy(t + 7 * n, g_gpExit, 7 * 4);
    t[7 * n + 0] = (DWORD)(ULONG_PTR)GpBackIn;
    t[7 * n + 5] = MSG_GP0 + GPM_BACK;
    t[7 * n + 6] = (DWORD)(ULONG_PTR)GpDescBack;
    t[7 * (n + 1)] = 0;
    g_gpPlayerN = n;
    g_opTable[OP_PLAYER] = t;
    g_opCount[OP_PLAYER] = n + 1;
    g_opTitle[OP_PLAYER] = MSG_GP0 + GPM_T_PLAYER;
}
// v3.37 MN337: the tables hold every row (Player with Regeneration: 10 of 11; Enemies 6 of 7; Special 5 of 6; Option 5: 9)
typedef char GpPlayerFits[(1 + 3 + PC_COUNT + 1) * 7 + 1 <= sizeof g_rowsPlayer / sizeof g_rowsPlayer[0] ? 1 : -1];  // v3.47 CT347: 4 -> 3 again (Stick Movement left)
typedef char GpPlayerRowsFit[1 + 3 + PC_COUNT <= GP_N + PC_COUNT ? 1 : -1];  // v3.46 OM346: g_gpPlayerRow holds every row (v3.47 CT347: 3)
typedef char GpEnemyFits[8 * 7 + 1 <= sizeof g_rowsEnemy / sizeof g_rowsEnemy[0] ? 1 : -1];  // v3.50 EF350: 6 -> 7 (+ Enemy Pursuit)  // v3.50 MERGE: 7 -> 8 (+ FI350 Fade In: Copies, Follow, Pursuit, Health Bars, Drops, Revive, Fade In, Back)
typedef char GpSpecialFits[6 * 7 + 1 <= sizeof g_rowsSpecial / sizeof g_rowsSpecial[0] ? 1 : -1];  // v3.43 GT343: 5 -> 6 (+ Ghost Teleport)
typedef char GpPlayFits[GPT_N * 7 + 1 <= sizeof g_rowsPlay / sizeof g_rowsPlay[0] ? 1 : -1];  // v3.48 RZ348: 10 rows
typedef char GpRandoFits[(9 + 1) * 7 + 1 <= sizeof g_rowsRando / sizeof g_rowsRando[0] ? 1 : -1];  // v3.48 RZ348: 6 rows + Back (r2: + Change Each Visit)  // v3.49 RZ349: 7 rows (+ Monsters)  // v3.49 MERGE: 9 rows (+ RZ349L Eileen Outfit, Ghosts) + Back = 10 of 11
// a folder row on Option 5: a copy of the Button Config record (no value) with our input / label / description
static void GpFolderRow(DWORD *r, const DWORD *bcRow, const void *input, int label, const void *desc)
{
    memcpy(r, bcRow, 7 * 4);
    r[0] = (DWORD)(ULONG_PTR)input;
    r[5] = (DWORD)label;
    r[6] = (DWORD)(ULONG_PTR)desc;
}
// a value row on a folder page, its GP index recorded in map[*n]
static void GpPageRow(DWORD *t, int *map, int *n, int k)
{
    map[*n] = k;
    FillRow(t + 7 * *n, kGpIn[k], kGpGet[k], g_gpIds[k], kGpCount[k], MSG_GP0 + GPM_L0 + k, kGpDesc[k]);
    ++*n;
}
// a folder's Back row (Option 1's Exit record with our input / label / description) and the end mark
static void GpBackRow(DWORD *t, int n, const DWORD *stockRows)
{
    memcpy(t + 7 * n, stockRows + 6 * 7, 7 * 4);
    t[7 * n + 0] = (DWORD)(ULONG_PTR)GpBackIn;
    t[7 * n + 5] = MSG_GP0 + GPM_BACK;
    t[7 * n + 6] = (DWORD)(ULONG_PTR)GpDescBack;
    t[7 * (n + 1)] = 0;
}
// Builds Option 5 and its three folders. stockRows = Option 1's stock table (Exit at row 6), bcRow = Option 2's Button
// Config record (a value-less sub-screen row: getter 0x51EB10, one blank value), shineRow = the Item Shine row
// (sh4windowmod.c). v3.37 MN337: Option 5 = No Clip, Curse Meter, Extended Toolbelt, Stackable Items, Item Shine, Player >,
// Enemies >, Special >, Exit; every position recorded. Returns Option 5's row count.
static int GpBuildPages(const DWORD *stockRows, const DWORD *bcRow, const DWORD *shineRow, DWORD *play, DWORD *player,
                        DWORD *enemy, DWORD *special)
{
    int n = 0, k;
    for (k = 0; k < GPT_N; ++k) g_gpTop[k] = -1;
    g_gpTop[GPT_NC] = n;
    NcFillRow(play + 7 * n++);  // v3.32 NC332: No Clip (v3.37 MN337: from the Special folder)
    g_gpTop[GPT_CURSE] = n;
    n = HbPlayerRow(play, n);  // HUD BARS: Curse Meter (v3.37 MN337: from the Player folder; HbRowChanged asks for page 4)
    g_gpTop[GPT_TB] = n;
    InvToolbeltFillRow(play + 7 * n++);  // v3.37 MN337: Extended Toolbelt (inventory.inc, W2)
    g_gpTop[GPT_STACK] = n;
    InvStackFillRow(play + 7 * n++);  // v3.37 MN337: Stackable Items (inventory.inc, W2)
    g_gpTop[GPT_SHINE] = n;
    memcpy(play + 7 * n++, shineRow, 7 * 4);  // Item Shine (sh4windowmod.c's row; RowChangedDraw asks g_playShineRow)
    g_playShineRow = g_gpTop[GPT_SHINE];
    g_gpTop[GPT_PLAYER] = n;
    GpFolderRow(play + 7 * n++, bcRow, GpPlayerIn, MSG_GP0 + GPM_R_PLAYER, GpDescPlayer);
    g_gpTop[GPT_ENEMY] = n;
    GpFolderRow(play + 7 * n++, bcRow, GpEnemyIn, MSG_GP0 + GPM_R_ENEMY, GpDescEnemy);
    g_gpTop[GPT_SPECIAL] = n;
    GpFolderRow(play + 7 * n++, bcRow, GpSpecialIn, MSG_GP0 + GPM_R_SPECIAL, GpDescSpecial);  // v3.27.0
    g_gpTop[GPT_RANDO] = n;
    RzFolderRow(play + 7 * n++, bcRow);  // v3.48 RZ348: Randomizer (randomizer.inc), under Special
    g_gpTop[GPT_EXIT] = n;
    memcpy(play + 7 * n++, stockRows + 6 * 7, 7 * 4);  // Exit
    play[7 * n] = 0;
    // Player: One-Hit Kill (v3.37 MN337), Movement Speed, Medallion Durability, (Regeneration), the PLAYER PACK, Back
    g_gpExit = stockRows + 6 * 7;
    g_gpPlayerTab = player;
    GpBuildPlayer();
    // Enemies (v3.37 MN337: One-Hit Kill left for Player): Enemy Copies, Enemies Follow (v3.31 ENEMIES), Enemy Health Bars
    // (HUD BARS), Supply Drops, Revive Enemies (v3.30 EN330), Back
    {
        int nE = 0;
        GpPageRow(enemy, g_gpEnemyRow, &nE, GP_COPIES);
        g_gpEnemyFollow = nE;
        GpPageRow(enemy, g_gpEnemyRow, &nE, GP_FOLLOW);
        g_efPursuitRow = nE, g_gpEnemyRow[nE] = -1;  // v3.50 EF350: Enemy Pursuit under Enemies Follow (GpRowChanged asks EfRowChanged)
        EfPursuitFillRow(enemy + 7 * nE++);
        g_gpEnemyRow[nE] = -1;  // HUD BARS: HbRowChanged answers for it
        nE = HbEnemyRow(enemy, nE);
        GpPageRow(enemy, g_gpEnemyRow, &nE, GP_DROP);
        GpPageRow(enemy, g_gpEnemyRow, &nE, GP_REVIVE);
        g_gpEnemyRow[nE] = -1;  // v3.50 FI350: Fade In (FiRowChanged answers for it)
        nE = FiEnemyRow(enemy, nE);
        g_gpEnemyN = nE;
        GpBackRow(enemy, nE, stockRows);
        g_opCount[OP_ENEMY] = nE + 1;  // v3.37 MN337: 6 (was 7 with One-Hit Kill)  // v3.50 EF350: 7  // v3.50 MERGE: 8 with FI350's Fade In (row step 32: <= 8 rows)
    }
    g_opTable[OP_ENEMY] = enemy;
    g_opTitle[OP_ENEMY] = MSG_GP0 + GPM_T_ENEMY;
    // Special: Death Presence (v3.27.0), After Shock (v3.29 AFTERIMAGE), Echo Doubles, Echo Count (v3.30 ECHO330), Back
    // (v3.37 MN337: No Clip left for Option 5)
    {
        int nS = 0;
        GpPageRow(special, g_gpSpecialRow, &nS, GP_DP);
        GpPageRow(special, g_gpSpecialRow, &nS, GP_AI);
        GpPageRow(special, g_gpSpecialRow, &nS, GP_ECHO);
        GpPageRow(special, g_gpSpecialRow, &nS, GP_ECHON);
        g_gpSpecialGt = nS;  // v3.43 GT343: Ghost Teleport under Echo Count (teleport.inc; GpRowChanged asks GtRowChanged)
        g_gpSpecialRow[nS] = -1;
        GtFillRow(special + 7 * nS++);
        g_gpSpecialN = nS;
        GpBackRow(special, nS, stockRows);
        g_opCount[OP_SPECIAL] = nS + 1;  // v3.37 MN337: 5 (was 6 with No Clip)
    }
    g_opTable[OP_SPECIAL] = special;
    g_opTitle[OP_SPECIAL] = MSG_GP0 + GPM_T_SPECIAL;
    g_opParent[OP_PLAYER] = g_opParent[OP_ENEMY] = g_opParent[OP_SPECIAL] = 4;
    g_opParentRow[OP_PLAYER] = g_gpTop[GPT_PLAYER];  // v3.37 MN337: Back lands on the row that opened the folder
    g_opParentRow[OP_ENEMY] = g_gpTop[GPT_ENEMY];
    g_opParentRow[OP_SPECIAL] = g_gpTop[GPT_SPECIAL];
    RzBuildPage(stockRows, bcRow, g_rowsRando, g_gpTop[GPT_RANDO]);  // v3.48 RZ348: the Randomizer page (OP_RANDO, parent Option 5)
    LogV("gameplay: Option 5 = No Clip %d, Curse Meter %d, Extended Toolbelt %d, Stackable Items %d, Item Shine %d, Player %d, "
         "Enemies %d, Special %d, Randomizer %d, Exit %d; Player %d rows, Enemies %d, Special %d, Randomizer %d (with Back)",
         g_gpTop[GPT_NC], g_gpTop[GPT_CURSE], g_gpTop[GPT_TB], g_gpTop[GPT_STACK], g_gpTop[GPT_SHINE], g_gpTop[GPT_PLAYER],
         g_gpTop[GPT_ENEMY], g_gpTop[GPT_SPECIAL], g_gpTop[GPT_RANDO], g_gpTop[GPT_EXIT], g_opCount[OP_PLAYER], g_opCount[OP_ENEMY],
         g_opCount[OP_SPECIAL], g_opCount[OP_RANDO]);  // v3.37 MN337  // v3.48 RZ348: + Randomizer
    return n;
}
// v3.37 MN337: without the sub-screen fade the folders cannot open: Option 5 = Item Shine + Exit (as before); the rows
// moved to Option 5 go with the folders (their settings still apply from the ini). Returns the row count (2).
static int GpNoFadeLayout(const DWORD *stockRows, DWORD *play)
{
    int k;
    if (g_playShineRow > 0) memmove(play, play + 7 * g_playShineRow, 7 * 4);
    memcpy(play + 7, stockRows + 6 * 7, 7 * 4);
    play[14] = 0;
    g_playShineRow = 0;
    for (k = 0; k < GPT_N; ++k) g_gpTop[k] = -1;
    g_gpTop[GPT_SHINE] = 0, g_gpTop[GPT_EXIT] = 1;
    g_opParent[OP_PLAYER] = g_opParent[OP_ENEMY] = g_opParent[OP_SPECIAL] = -1;
    g_opParent[OP_RANDO] = -1;  // v3.48 RZ348 (the Randomizer's settings still apply from the ini)
    g_gpMenu = 0;
    return 2;
}

typedef char GpTextsFit[GPM_N <= MSG_PC0 - MSG_GP0 ? 1 : -1];  // the id block in sh4windowmod.c holds every text (HUD BARS: its block ends at MSG_HB0; v3.29 merge: the gameplay block ends at MSG_PC0)

// v3.31 ENEMIES: Enemies Follow's "Monsters + Victims" is wider than the value column (sh4windowmod.c LongValW)
static int GpLongValW(int page, int row)
{
    int v;
    if (page != OP_ENEMY || row != g_gpEnemyFollow || row < 0 || !g_gpMenu) return 0;  // v3.37 MN337: its recorded row (was 2)
    v = (int)g_gp[GP_FOLLOW];
    if (v < 0 || v >= kGpCount[GP_FOLLOW] || (int)strlen(OlShow(OLM_ENEMIES_DOOR_CHASE, kGpName[GP_FOLLOW][v])) <= LONGVAL_CHARS) return 0;  // v3.53 OL1: the shown word (Both: no move)
    return (int)(LabelW(g_txtGp[GPM_V0 + GP_FOLLOW * GP_VMAX + v]) + 0.5f);
}

// ---------------------------------------------------------------- the cheats' hooks (re_cheats2.md)
// All four pass straight through at the default value. Sites free of cheat-table AOBs and Playable Eileen patches
// (re_cheats2.md: ct_conflicts --span, greps).

// One-Hit Kill (re_cheats2.md 1-2): 0x44F605 in EnemyReceiveHit 0x44F410, the only place a hit record is made, right
// before the damage is stored (rec+0x2C). A record whose attack is the player's attack object 0x10A4FE0 (swings, stomps,
// guns; Playable Eileen's weapons use it too) gets at least (hp + hp_max) x 4 + 1 (handlers scale damage by 0.25 at
// most), so the game's own death path runs. Ghosts (0x0B-0x0F, 0x11) then go down by their stock HP <= 0 rule (~30 s);
// Walter / the Conjoined Bodies (type 0x10) and the Wall boss (type 5, placement mode 8) keep the stock damage.
static const DWORD g_ohkBack = 0x44F60D;
static DWORD __cdecl OhkDamage(BYTE *task, const float *hp, DWORD dmgBits, const void *atk)
{
    static DWORD s_logged;  // bit per type: logged once
    DWORD out = dmgBits;
    (void)atk;
    if (!g_ohk || !task || !hp) return dmgBits;
    __try {
        const BYTE *work = *(BYTE *const *)(task + 0x408), *pr = *(BYTE *const *)(task + 0x41C);
        unsigned mode = 0;
        int type, modeKnown = 0;
        float dmg, kill;
        if (!work) return dmgBits;
        type = *(const int *)(work + 0x20);
        // the stock lookup (0x45B4B0 via 0x52B290): p = [task+41Ch] + 2Ch; valid when the ADDRESS p >= 10000h; mode =
        // [p+14h] & 0xF. review REV-325-006: a type 5 whose placement is not readable keeps the stock damage (fail closed)
        if ((ULONG_PTR)(pr + 0x2C) >= 0x10000) {
            mode = *(const unsigned *)(pr + 0x40) & 0xF;
            modeKnown = 1;
        }
        if (type == 0x10 || (type == 5 && (!modeKnown || mode == 8))) return dmgBits;  // Walter, the Bodies, the Wall boss
        if (!((type >= 0x02 && type <= 0x0F) || type == 0x11)) return dmgBits;  // monsters and ghosts only
        memcpy(&dmg, &dmgBits, 4);
        kill = (hp[0] + hp[1]) * 4.0f + 1.0f;
        if (!(dmg >= kill)) dmg = kill;  // (a NaN damage also becomes lethal; a NaN hp keeps the damage)
        if (kill == kill) memcpy(&out, &dmg, 4);
        if (type < 32 && !(s_logged >> type & 1)) {
            s_logged |= 1u << type;
            Log("one-hit kill: first lethal hit on enemy type %02X (hp %.0f / %.0f)", type, hp[0], hp[1]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return dmgBits;
    }
    return out;
}
static __declspec(naked) void hkOhk(void)  // jmp from 0x44F605; esi record, edi attack, [esp+1Ch] damage, [esp+20h] HP block
{
    __asm {
        mov ecx, [esp + 1Ch]         // the replaced instruction
        cmp edi, 10A4FE0h
        jne back
        push edi                     // attack
        push ecx                     // damage bits
        push dword ptr [esp + 28h]   // the HP block ([esp+20h] at the site, 8 bytes pushed)
        push dword ptr [ebp + 8]     // the victim task
        call PcHitDamage             // PLAYER PACK: Dark Pact x k, OhkDamage, Life Steal (was: call OhkDamage)
        add esp, 10h
        mov ecx, eax
    back:
        push edi                     // replaced
        mov [esi + 44h], edi         // replaced
        jmp dword ptr [g_ohkBack]    // 0x44F60D: mov [esi+2Ch],ecx
    }
}

// Movement Speed (re_cheats2.md 3): `call 534590` at 0x534BEB (the third-person travel step: ecx flags, edx out; no
// stack arguments; it does not write the position) -> hkMove scales the step around P+0x40 / P+0x48 (P = [0xFD5BA0]).
// `call 52E980` at 0x5352C2 (once per tick, after the step) -> hkAnim: when the step was scaled and she walks / runs
// (mode 0, state 1 / 2 / 0x2D / 0x30 / 0x33), the one-shot playback multiplier P+0x3D0 (zeroed by 0x52E980 every
// call) becomes (value > 0 ? value : 1) x k, so her feet follow. Room 302: the two `fmul [5C5BA8]` (10.5) operands at
// 0x52D89C / 0x52D8E8 read g_r302Walk = 10.5 x k.
static const DWORD g_tankMove = 0x534590, g_animTick = 0x52E980;
static volatile LONG g_moveScaled;
static void __cdecl MoveScale(float *out)
{
    const float k = g_moveK;
    const BYTE *P;
    if (k == 1.0f || !out) return;
    P = *(BYTE *const *)0xFD5BA0;
    if (!P) return;
    out[0] = *(const float *)(P + 0x40) + (out[0] - *(const float *)(P + 0x40)) * k;
    out[2] = *(const float *)(P + 0x48) + (out[2] - *(const float *)(P + 0x48)) * k;
    InterlockedExchange(&g_moveScaled, 1);
}
static __declspec(naked) void hkMove(void)
{
    __asm {
        push edx                     // out (0x534590 takes no stack arguments)
        call dword ptr [g_tankMove]
        call MoveScale               // its argument is the pushed out
        pop edx
        ret
    }
}
static void __cdecl AnimPre(BYTE *P)
{
    const float k = g_moveK;
    int st;
    if (!InterlockedExchange(&g_moveScaled, 0) || k == 1.0f || !P) return;
    if (*(const int *)(P + 0x214) != 0) return;
    st = *(const int *)(P + 0x3B8);
    if (st == 1 || st == 2 || st == 0x2D || st == 0x30 || st == 0x33) {
        float *m = (float *)(P + 0x3D0);
        *m = (*m > 0.0f ? *m : 1.0f) * k;
    }
}
static __declspec(naked) void hkAnim(void)  // call from 0x5352C2: [esp+4] = P (the caller cleans)
{
    __asm {
        push dword ptr [esp + 4]
        call AnimPre
        add esp, 4
        jmp dword ptr [g_animTick]   // 0x52E980 with the caller's own frame
    }
}

// Medallion Durability (re_cheats2.md 4): the ghost-contact wear `call 516F60` (push 8) at 0x53AB76 and the Room 302
// loop's at 0x53AB92 -> MedWearK: before the stock call, the active medallion slot (0x4F5D60) gets dt x (1 - 1/k)
// back, so the net wear is dt / k; the stock call still breaks it at 0, plays the effect and sets the sound flag. If the
// game wore another slot (the cheat table's Extend Toolbelt page 2), the bonus is taken back.
static float __cdecl MedWearK(int item)
{
    float *c = NULL, before = 0.0f, bonus = 0.0f, r;
    const float k = g_medK;
    if (k > 1.0f && item == 8) {
        int slot = ((int(__cdecl *)(void))0x4F5D60)();
        const int rate = *(volatile const int *)0x5D5248;
        if (slot < 0) slot = InvMedSlotP2();  // v3.37 INV337: the medallion in use on Extended Toolbelt's page 2 (88h+i)
        if (((slot >= 0 && slot < 10) || (slot >= 0x88 && slot < 0x92)) && rate > 0) {
            c = (float *)(ULONG_PTR)(0x13D7B60 + 8 * slot);
            before = *c;
            if (before > 0.0f) {
                bonus = (1.0f / (float)rate) * (1.0f - 1.0f / k);
                *c = before + bonus;
            } else {
                c = NULL;
            }
        }
    }
    r = ((float(__cdecl *)(int))0x516F60)(item);
    if (c && *c == before + bonus) *c = before;  // not this slot's wear: undo the bonus
    return r;
}

static void CheatsInstall(void)
{
    static const Site kOhk = { 0x44F605, { 0x8B, 0x4C, 0x24, 0x1C, 0x57, 0x89, 0x7E, 0x44, 0x89, 0x4E, 0x2C, 0xE8, 0x8B, 0xEB,
                                           0x0A, 0x00 }, 16 };
    static const Site kMove[4] = {
        { 0x534BE5, { 0x8D, 0x54, 0x24, 0x20, 0x8B, 0xCB, 0xE8, 0xA0, 0xF9, 0xFF, 0xFF, 0xEB, 0x54 }, 13 },
        { 0x5352C1, { 0x57, 0xE8, 0xB9, 0x96, 0xFF, 0xFF, 0x57, 0xE8, 0x93, 0x04, 0x00, 0x00 }, 12 },
        { 0x52D89A, { 0xD8, 0x0D, 0xA8, 0x5B, 0x5C, 0x00 }, 6 },
        { 0x52D8E6, { 0xD8, 0x0D, 0xA8, 0x5B, 0x5C, 0x00 }, 6 },
    };
    static const Site kMed[2] = {
        { 0x53AB74, { 0x6A, 0x08, 0xE8, 0xE5, 0xC3, 0xFD, 0xFF, 0xDD, 0xD8 }, 9 },
        { 0x53AB90, { 0x6A, 0x08, 0xE8, 0xC9, 0xC3, 0xFD, 0xFF, 0xDD, 0xD8, 0x83, 0xC4, 0x04, 0x4F, 0x75, 0xF1 }, 15 },
    };
    int i, ok;
    GpPatch set[4];
    if (SiteOk(&kOhk)) {
        GpBranch(&set[0], 0x44F605, 0xE9, hkOhk, 8);
        g_ohkOn = GpApplyAll(set, 1, "one-hit kill");
    } else {
        Log("one-hit kill: 0044F605 differs from the GOG v7 exe (another mod?): the option does nothing");
    }
    for (i = 0, ok = 1; i < 4; ++i) ok &= SiteOk(&kMove[i]);
    if (ok) {
        const DWORD w = (DWORD)(ULONG_PTR)&g_r302Walk;
        g_r302Walk = 10.5f * g_moveK * R3WalkK();  // v3.50 R302: x Room 302's Walk Speed (1 until room302.inc decides)
        GpBranch(&set[0], 0x534BEB, 0xE8, hkMove, 5);
        GpBranch(&set[1], 0x5352C2, 0xE8, hkAnim, 5);
        GpData(&set[2], 0x52D89C, &w, 4);
        GpData(&set[3], 0x52D8E8, &w, 4);
        g_moveOn = GpApplyAll(set, 4, "movement speed");
    } else {
        Log("movement speed: the travel / animation / Room 302 sites differ from the GOG v7 exe (another mod?): the option does nothing");
    }
    for (i = 0, ok = 1; i < 2; ++i) ok &= SiteOk(&kMed[i]);
    if (ok) {
        GpBranch(&set[0], 0x53AB76, 0xE8, MedWearK, 5);
        GpBranch(&set[1], 0x53AB92, 0xE8, MedWearK, 5);
        g_medOn = GpApplyAll(set, 2, "medallion durability");
    } else {
        Log("medallion durability: the wear sites differ from the GOG v7 exe (another mod?): the option does nothing");
    }
    Log("gameplay hooks: one-hit kill %s (0044F605), movement %s (00534BEB, 005352C2, 0052D89C / 0052D8E8), medallion %s "
        "(0053AB76 / 0053AB92)", g_ohkOn ? "in" : "OFF", g_moveOn ? "in" : "OFF", g_medOn ? "in" : "OFF");
}

static void GameplayInstall(void)  // Init, before the Options tables
{
    InitializeCriticalSection(&g_gpLock);  // (before any menu or poll can run)
    g_gpLockOn = 1;
    GpReadIni();
    GpTexts();
    RgInit();  // v3.47 RG347: [Gameplay] Regen / RegenDelay (seeded once from Playable Eileen's ini) and the row's texts
    HbInit();  // HUD BARS: its [Gameplay] keys and texts (before the Options tables)
    FiInit();  // v3.50 FI350: Fade In's [Gameplay] FadeIn and texts (before the Options tables)
    BloodInstall();
    CheatsInstall();
    PcInstall();  // PLAYER PACK (after CheatsInstall: needs g_ohkOn)
    NcInstall();  // v3.32 NC332: No Clip (one site, 0x534D29)
    InvInstall();  // v3.37 MN337 / INV337: Extended Toolbelt / Stackable Items (after the ini read; 46 sites, live switching)
    GtInstall();  // v3.43 GT343: Ghost Teleport (after the ini read; no exe patch, its game functions byte-checked)
    OmInstall();  // v3.46 OM346: Stick Movement (after the ini read; 3 sites, one set: 0x5543C0, 0x407B15, 0x407B28)
    RzInit();  // v3.48 RZ348: the Randomizer's [Randomizer] keys, texts and the scripts' self-check (no exe patch)
}

#include "playercheats.inc"  // PLAYER PACK
#include "noclip.inc"  // v3.32 NC332: No Clip
#include "inventory.inc"  // v3.37 MN337: Extended Toolbelt / Stackable Items rows (W2 INV337's file; W1 ships a stub)
