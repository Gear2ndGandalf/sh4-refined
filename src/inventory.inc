// ================================================================ v3.37 INV337: Extended Toolbelt and Stackable Items
// The cheat table's Extend Toolbelt v6 (CT 7310) and Stackable Items v3 (CT 7360) as two Options rows (W1 places them on
// Option 5 above Item Shine): [Gameplay] ExtendedToolbelt and [Gameplay] StackableItems, Off / On (default Off; 0 / 1
// read too). Plan, sites and the live checklist: staging/v337_feedback/w2_inventory/PLAN.md and BUILD_NOTES.md.
//
// The hooks are the table's own code: tools/inv337/pockets_inv337.cea and stackable_inv337.cea (Refined's copies of
// the two scripts, diffable against the table's ct7310_ / ct7360_ copies there) translated to MSVC inline asm by
// tools/inv337_gen.py into src/inventory_cave.inc, together with every exe site and its stock bytes (46 sites:
// 33 Extended Toolbelt, 13 Stackable Items; each byte-checked against the GOG v7 exe by the generator and again here).
// Refined's changes to the table's code: (1) the attack bug - pk_gate lets the weapon in hand and Playable Eileen's
// weapons 80h..84h count on page 2 (R1 condition / R2 wear / R5 spawn); before, her weapon on the hidden page read as
// broken in ItemGetCondition 0x4F69E0 (PlayerWeaponIsBroken 0x544D80) and the ready gates 0x5387EB / 0x53DACE refused
// the attack; (2) Stackable's "is Extend Toolbelt on" test (cmp byte [4F57B0],E9) is Refined's own flag g_invTbLive;
// (3) with Stackable Items off its broken successor never joins a stack. The table's defaults stay: pk_pin 0 (no pins),
// pk_mel 1, the page marker, flips at the belt ends, overflow to page 2, key items seen on page 2 (pk_optc), st_max 99.
//
// Switching (live): the rows set what the player wants; the change is made at the start of a game tick (QsTick,
// 0x4EF982) while no inventory / chest / menu sub-mode is up and the item bar is idle (the table's own swap guards:
// [0x107BF54] = [0x107BF58] = 0, [0x108EC8C] = [0x108EC98] = 0); until the dispatcher has run once (the title, or no
// dispatcher) the row applies it at once. Extended Toolbelt Off with items on page 2: the weapon in hand (and the
// Saint Medallion in use, while there is room) goes to page 1, everything else into the Storage Chest (the user's
// choice), then the stock bytes come back and the game's own chest sort / merge / recount 0x4F6610 runs. If the chest
// cannot take them all (126 places) or the weapon in hand finds no room on page 1, the toolbelt stays on (buzzer, a
// LogV line), nothing moves. The [DISABLE] Lua of the table (refused while page 2 holds items) is replaced by that.
// Stackable Items: the thief cap 0x463607 (st_stw: a stack above 10 of a stealable item crashed EnemyStealItem
// 0x4635C0's 100-place list), the stacked break 0x4F6A30 and the use-one guard 0x4F5DB2 stay in whenever Refined runs
// (they act only on counts above 1, which the game never makes there); Off removes the other ten, stacks stay as counts.
// Its [ENABLE] Lua (the count bit on slot records with count > 1) is StMarkCounts at the switch.
// The cheat table: its 7310 / 7360 scripts patch the same sites. Refined refuses (all or nothing, a Log line naming the
// site) when the table's version is on, and the table's aobscans fail while Refined's bytes are in (the generator lists
// which), so the table refuses Refined's. Turn CT 7310 / 7360 off when using these rows.

#include "inventory_sim.h"

static volatile LONG g_invTbLive;  // the Extended Toolbelt's 33 sites are in (the cave's inv_tblive)
static volatile LONG g_invStOn;    // Stackable Items' 10 switched sites are in (the cave's inv_ston)
#include "inventory_cave.inc"      // generated: the two caves, their data blocks and the site lists

enum { INVM_TBL, INVM_TBD0, INVM_TBD1, INVM_STL, INVM_STD0, INVM_STD1, INVM_NA, INVM_N };
static BYTE g_txtInv[INVM_N][64];
typedef char InvTextsFit[INVM_N <= MSG_SHD0 - MSG_INV0 ? 1 : -1];  // v3.43 MERGE: ends at the next block (was MSG_END)
static volatile LONG g_invTbSet, g_invStSet;  // the rows / ini: what the player wants (0 Off, 1 On)
static LONG g_invTbSnap, g_invStSnap;
static int g_invTbUnsaved, g_invStUnsaved;
static int g_invReady, g_invTbOk, g_invStOk, g_invGuardOk, g_invFault, g_invTickSeen, g_invWaitSaid;
static BYTE g_invListedPk[INV_PK_SITES], g_invListedSt[INV_ST_SITES];
static DWORD g_invIds[2];
static const char *const kInvName[2] = { "Off", "On" };
#define INV_BLOCK ((InvBlock *)0x13D7B60)
#define INV_IN_HAND (*(volatile int *)0x10A1164)
#define INV_RANK ((const unsigned char *)0x5BF220)
#define INV_TMPL ((const InvEnt *)0x107A088)
enum { INV_ALL, INV_GUARDS, INV_SWITCHED };  // which of Stackable's sites

// the three Stackable Items sites that stay in whenever Refined runs
static int InvStGuard(DWORD va) { return va == 0x463607 || va == 0x4F6A30 || va == 0x4F5DB2; }
static int InvPick(DWORD va, int which) { return which == INV_ALL || (which == INV_GUARDS) == InvStGuard(va); }

static void InvSiteBytes(const InvSite *s, const DWORD *lbl, BYTE *b)
{
    if (s->kind == INV_IMM32) {
        memcpy(b, &s->imm, 4);
    } else {
        const DWORD rel = lbl[s->label] - (s->va + 5);
        memset(b, 0x90, s->len);
        b[0] = s->kind == INV_JMP ? 0xE9 : 0xE8;
        memcpy(b + 1, &rel, 4);
    }
}
// code bytes written live (game thread, at a tick start: none of these functions is running); listed once in SH4WM_Patches
static int InvPoke(DWORD va, const BYTE *b, int len, BYTE *listed)
{
    BYTE *p = (BYTE *)(ULONG_PTR)va;
    DWORD old;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(p, b, len);
    VirtualProtect(p, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    if (!*listed) ListPatch(va, len), *listed = 1;
    return !memcmp(p, b, len);
}
// v3.37 REV337 INV337-001: may the set come out? 1 = every selected site holds the game's bytes or Refined's (nothing
// else wrote there); else 0 and one rate-limited LogAnomaly names the first other site. Nothing is written.
static int InvSetCheckOff(const InvSite *s, int n, const DWORD *lbl, int which, const char *what)
{
    BYTE b[16];
    int i;
    for (i = 0; i < n; ++i) {
        if (!InvPick(s[i].va, which)) continue;
        InvSiteBytes(&s[i], lbl, b);
        if (!memcmp((const void *)(ULONG_PTR)s[i].va, s[i].stock, s[i].len) || !memcmp((const void *)(ULONG_PTR)s[i].va, b, s[i].len))
            continue;
        {
            static volatile LONG s_n;
            if (LogQuota(&s_n, 20))
                LogAnomaly("inventory", "%s off: %08lX (%s) holds neither Refined's bytes nor the game's (another script wrote there): "
                           "nothing restored, it stays on", what, s[i].va, s[i].ref);
        }
        return 0;
    }
    return 1;
}
// All or nothing. on: every site must hold the game's bytes or already ours, else nothing is written (a Log line names
// the first such site); a failed write rolls the set back. off (v3.37 REV337 INV337-001): every site is checked first
// (InvSetCheckOff): one holding anything else leaves the whole set as it is; a failed write puts Refined's bytes back on
// the sites already restored. Returns 1 when the set is in (on) / out (off).
static int InvApplySet(const InvSite *s, int n, const DWORD *lbl, BYTE *listed, int which, int on, const char *what)
{
    BYTE b[16];
    int i, j, bad = 0;
    if (on) {
        for (i = 0; i < n; ++i) {
            if (!InvPick(s[i].va, which)) continue;
            InvSiteBytes(&s[i], lbl, b);
            if (!memcmp((const void *)(ULONG_PTR)s[i].va, b, s[i].len) || !memcmp((const void *)(ULONG_PTR)s[i].va, s[i].stock, s[i].len))
                continue;
            Log("%s: %08lX (%s) holds neither the game's bytes nor Refined's (%s): nothing written, the option stays off", what,
                s[i].va, s[i].ref, *(const BYTE *)(ULONG_PTR)s[i].va == 0xE9 ?
                "a jump: the cheat table's script (7310 Extend Toolbelt / 7360 Stackable Items) is on? turn it off" : "another mod?");
            return 0;
        }
        for (i = 0; i < n; ++i) {
            if (!InvPick(s[i].va, which)) continue;
            InvSiteBytes(&s[i], lbl, b);
            if (!memcmp((const void *)(ULONG_PTR)s[i].va, b, s[i].len)) continue;
            if (!InvPoke(s[i].va, b, s[i].len, &listed[i])) {
                for (j = i; j >= 0; --j)
                    if (InvPick(s[j].va, which)) InvPoke(s[j].va, s[j].stock, s[j].len, &listed[j]);
                Log("%s: %08lX could not be written: the set is rolled back, the option stays off", what, s[i].va);
                return 0;
            }
        }
        return 1;
    }
    if (!InvSetCheckOff(s, n, lbl, which, what)) return 0;  // v3.37 REV337 INV337-001: nothing restored
    for (i = 0; i < n; ++i) {
        if (!InvPick(s[i].va, which)) continue;
        if (!memcmp((const void *)(ULONG_PTR)s[i].va, s[i].stock, s[i].len)) continue;
        if (!InvPoke(s[i].va, s[i].stock, s[i].len, &listed[i])) {
            for (j = i - 1; j >= 0; --j)  // v3.37 REV337 INV337-001: the set goes back in whole
                if (InvPick(s[j].va, which)) {
                    InvSiteBytes(&s[j], lbl, b);
                    InvPoke(s[j].va, b, s[j].len, &listed[j]);
                }
            ++bad;
            LogAnomaly("inventory", "%s off: %08lX could not be written: Refined's bytes are back on every site, it stays on", what, s[i].va);
            break;
        }
    }
    return !bad;
}

static int g_invBoot;  // InvInstall runs (Init, before the exe's own init): no game function may be called
static void InvBuzz(void) { if (!g_invBoot) ((Fn1)0x56CC40)(0x9C44); }

// ---------------------------------------------------------------- Extended Toolbelt on / off
static void InvSaveIni(const char *key, int v, int *unsaved)
{
    *unsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", key, kInvName[v], g_iniPath)) {
        *unsaved = 1;
        Log("gameplay: %s %s applies now but could not be saved in SH4Refined.ini (error %lu)", key, kInvName[v], GetLastError());
    }
}
static void InvTbOn(void)
{
    if (!InvApplySet(kPkSites, INV_PK_SITES, g_pkLbl, g_invListedPk, INV_ALL, 1, "extended toolbelt")) {
        InterlockedExchange(&g_invTbSet, 0);  // the row shows Off again (the ini keeps the player's On for the next start)
        InvBuzz();
        return;
    }
    InterlockedExchange(&g_invTbLive, 1);
    LogV("extended toolbelt: on (33 exe sites); page 2 (chest entries 126..135) holds %d item(s), the chest %u entr%s; "
         "Left / Right past the belt's ends flips the page", InvCnt10(InvP2(INV_BLOCK)), INV_BLOCK->chestCount,
         INV_BLOCK->chestCount == 1 ? "y" : "ies");
}
// why the switch-off is refused (0 = it may go ahead); fromRow: the row's own check (buzzer, nothing moved)
static const char *InvOffRefusal(const InvOffResult *r, char *buf, size_t n)
{
    if (r->why == INV_OFF_CHEST_FULL)
        sprintf_s(buf, n, "page 2 holds %d item(s) for the Storage Chest and it has room for %d (126 places): store or use some, "
                  "then turn it off", r->toChest, r->chestFree);
    else if (r->why == INV_OFF_HAND_NO_ROOM)
        sprintf_s(buf, n, "the weapon in your hand (%02X) is on page 2 and page 1 is full: free a belt slot, then turn it off",
                  INV_IN_HAND);
    else
        return NULL;
    return buf;
}
static int InvOffAllowed(void)  // the row: 1 = Off may be asked for now
{
    InvOffResult r;
    char why[200];
    if (!g_invTbLive) return 1;
    InvOffPlan(INV_BLOCK, INV_IN_HAND, &r);
    if (!InvOffRefusal(&r, why, sizeof why)) return 1;
    InvBuzz();
    LogV("extended toolbelt: stays on: %s", why);
    return 0;
}
static void InvTbOff(void)
{
    InvOffResult r;
    char why[200];
    const int hand = INV_IN_HAND;
    int moved = 0;
    InvOffPlan(INV_BLOCK, hand, &r);
    if (InvOffRefusal(&r, why, sizeof why)) {
        InterlockedExchange(&g_invTbSet, 1);  // the switch stays on; so does the ini
        InvSaveIni("ExtendedToolbelt", 1, &g_invTbUnsaved);
        InvBuzz();
        LogV("extended toolbelt: stays on: %s", why);
        return;
    }
    if (!InvSetCheckOff(kPkSites, INV_PK_SITES, g_pkLbl, INV_ALL, "extended toolbelt")) {  // v3.37 REV337 INV337-001: before
        InterlockedExchange(&g_invTbSet, 1);  // any item moves: another script holds a site, nothing changes, it stays on
        InvSaveIni("ExtendedToolbelt", 1, &g_invTbUnsaved);
        InvBuzz();
        return;
    }
    if (r.why == INV_OFF_OK) moved = InvOffApply(INV_BLOCK, hand, INV_RANK, INV_TMPL, &r);
    if (!InvApplySet(kPkSites, INV_PK_SITES, g_pkLbl, g_invListedPk, INV_ALL, 0, "extended toolbelt")) {
        InterlockedExchange(&g_invTbSet, 1);  // (the items moved already: page 2 is empty; a later Off tries again)
        LogAnomaly("inventory", "extended toolbelt: its sites could not all be restored: it stays on");
        return;
    }
    InterlockedExchange(&g_invTbLive, 0);
    if (r.toChest) ((void(__cdecl *)(void))0x4F6610)();  // the game's own chest sort / merge / recount (stock again)
    if (moved)
        LogV("extended toolbelt: off: page 2's %d item(s) moved: %d to page 1 (the weapon in hand / the medallion in use), %d into the "
             "Storage Chest (now %u entries)%s", moved, r.toPage1, r.toChest, INV_BLOCK->chestCount,
             r.medOff ? "; the Saint Medallion in use went into the chest and is taken off" : "");
    else
        LogV("extended toolbelt: off (page 2 was empty); the game's own code again");
}

// ---------------------------------------------------------------- Stackable Items on / off
static void InvStSwitch(int on)
{
    if (!InvApplySet(kStSites, INV_ST_SITES, g_stLbl, g_invListedSt, INV_SWITCHED, on, "stackable items")) {
        InterlockedExchange(&g_invStSet, g_invStOn);
        if (on) InvBuzz();
        else LogAnomaly("inventory", "stackable items: its sites could not all be restored: it stays on");
        return;
    }
    InterlockedExchange(&g_invStOn, on);
    if (on) {
        const int n = StMarkCounts((unsigned char *)0x108ECD8);  // the table's [ENABLE] Lua: counts drawn at once
        LogV("stackable items: on (10 exe sites; up to %ld a stack); %d belt slot(s) show their count now", INV_ST_MAX, n);
    } else {
        LogV("stackable items: off; stacks stay as they are (the thief cap 00463607, the stacked break 004F6A30 and the use-one "
             "guard 004F5DB2 stay in)");
    }
}

static int InvIdle(void)
{
    return !*(volatile const int *)0x107BF54 && !*(volatile const int *)0x107BF58 && !*(volatile const int *)0x108EC8C &&
           !*(volatile const int *)0x108EC98;
}
static int InvPending(void)
{
    return (g_invTbOk && g_invTbSet != g_invTbLive) || (g_invStOk && g_invStSet != g_invStOn);
}
static void InvReconcile(void)
{
    if (g_invTbOk && g_invTbSet != g_invTbLive) {
        if (g_invTbSet) InvTbOn();
        else InvTbOff();
    }
    if (g_invStOk && g_invStSet != g_invStOn) InvStSwitch((int)g_invStSet);
}

// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread)
static LONG g_invSeen[12], g_invFlipsSeen;
static DWORD g_invSumAt, g_invLineAt;
static int g_invP2Said, g_invCtSaid;
static int InvPkStock(void)  // every Extended Toolbelt site holds the game's bytes (no toolbelt of anyone's is on)
{
    int i;
    for (i = 0; i < INV_PK_SITES; ++i)
        if (memcmp((const void *)(ULONG_PTR)kPkSites[i].va, kPkSites[i].stock, kPkSites[i].len)) return 0;
    return 1;
}
// The toolbelt is off (nobody's is on) and the game holds items in chest entries 126..135 with a chest count of 126 or
// less: page 2 of a save made with it on. The same move as the switch-off (what the game's next chest sort would do,
// without its merge quirk at 127+ entries); when it does not fit, the game's own way (it pulls them in later).
static void InvOrphans(int p2)
{
    InvOffResult r;
    char why[200];
    const int hand = INV_IN_HAND;
    int moved;
    InvOffPlan(INV_BLOCK, hand, &r);
    if (r.why != INV_OFF_OK) {
        if (!g_invP2Said) {
            g_invP2Said = 1;
            LogV("extended toolbelt off: this game has %d item(s) on page 2 (a save made with it on) and they do not fit (%s); the "
                 "game moves them into the Storage Chest when it next sorts it (it may then list up to 136 entries; a put-in waits "
                 "until there is room); turning the toolbelt on keeps them as page 2", p2,
                 InvOffRefusal(&r, why, sizeof why) ? why : "?");
        }
        return;
    }
    moved = InvOffApply(INV_BLOCK, hand, INV_RANK, INV_TMPL, &r);
    if (r.toChest) ((void(__cdecl *)(void))0x4F6610)();  // the game's chest sort / merge / recount
    g_invP2Said = 0;
    LogV("extended toolbelt off: this game had %d item(s) on page 2 (a save made with it on): %d to page 1, %d into the Storage "
         "Chest (now %u entries)", moved, r.toPage1, r.toChest, INV_BLOCK->chestCount);
}
static void InvWatch(void)
{
    static volatile LONG s_nMed, s_nCnt;
    const InvBlock *b = INV_BLOCK;
    const DWORD now = GetTickCount();
    const LONG c[12] = { INV_PK_COUNT, INV_PK_OVFN, INV_PK_C3N, INV_PK_R5N, INV_PK_GN, INV_PK_RLN, INV_PK_MDN,
                         INV_ST_MGN, INV_ST_TKN, INV_ST_BKN, INV_ST_LOST, INV_ST_PFN };
    int i, p2, flagged;
    if (c[0] != g_invFlipsSeen) {
        g_invFlipsSeen = c[0];
        LogT("extended toolbelt: flip %ld (the belt shows page %ld)", c[0], INV_PK_PAGE + 1);
    }
    if (now - g_invSumAt < 2000) return;  // the rest every 2 s
    g_invSumAt = now;
    p2 = InvCnt10(InvP2c(b));
    if (memcmp(c, g_invSeen, sizeof c) && (!g_invLineAt || now - g_invLineAt >= 30000)) {  // a summary every 30 s at most
        g_invLineAt = now ? now : 1;
        LogV("inventory: toolbelt flips %ld, overflows to page 2 %ld, page-2 story removals %ld, re-equips from page 2 %ld, rounds "
             "fired from page 2 %ld, cross-page reloads %ld, broken page-2 medallions %ld; stacks: merges %ld, chest take-outs %ld, "
             "stacked breaks %ld, broken ones dropped (no room) %ld, full-belt pickups %ld",
             c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10], c[11]);
        if (c[10] != g_invSeen[10])
            LogV("stackable items: %ld broken item(s) had no room on the belt%s: dropped", c[10] - g_invSeen[10],
                 g_invTbLive ? " or page 2" : "");
        memcpy(g_invSeen, c, sizeof c);
    }
    if (!g_invTbLive) {
        if (p2 && b->chestCount <= INV_CHEST_CAP && InvIdle() && !InvPending() && InvPkStock()) InvOrphans(p2);
        else if (!p2) g_invP2Said = 0;
        if (*(volatile const BYTE *)0x4F57B0 == 0xE9 && !g_invCtSaid) {
            g_invCtSaid = 1;
            LogV("extended toolbelt: another script (the cheat table's Extend Toolbelt, CT 7310?) is on at 004F57B0: Refined's row "
                 "refuses to switch on until it is off");
        }
        return;
    }
    // impossible states while the toolbelt is on (rate-limited)
    if (b->chestCount > INV_CHEST_PHYS && LogQuota(&s_nCnt, 5))
        LogAnomaly("inventory", "the chest count reads %u (more than its 136 entries)", b->chestCount);
    for (i = 0, flagged = 0; i < 10; ++i) {
        flagged += b->e[i].id == INV_MEDALLION && b->e[i].flag;
        flagged += InvP2c(b)[i].id == INV_MEDALLION && InvP2c(b)[i].flag;
    }
    if ((flagged > 1 || (b->medFlag && !flagged)) && LogQuota(&s_nMed, 5))
        LogAnomaly("inventory", "Saint Medallion state: the in-use flag %u, %d entr%s marked in use on the two pages", b->medFlag,
                   flagged, flagged == 1 ? "y" : "ies");
}
static void InvTickBody(void)
{
    if (InvPending()) {
        if (InvIdle()) {
            g_invWaitSaid = 0;
            InvReconcile();
        } else if (!g_invWaitSaid) {
            g_invWaitSaid = 1;
            LogV("inventory: a switch waits until the item bar, the chest and the menus are closed");
        }
    }
    InvWatch();
}
static void InvTick(void)
{
    if (!g_invReady || g_invFault) return;
    g_invTickSeen = 1;
    __try {
        InvTickBody();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_invFault = 1;
        LogAnomaly("inventory", "the switch / watch faulted (code %08lX): no more switching this session (the hooks stay as they are)",
                   GetExceptionCode());
    }
}

// Medallion Durability (gameplay.inc MedWearK): the Saint Medallion in use on page 2 (88h+i), or -1
static int InvMedSlotP2(void)
{
    int i;
    if (!g_invTbLive) return -1;
    for (i = 0; i < 10; ++i)
        if (InvP2c(INV_BLOCK)[i].id == INV_MEDALLION && InvP2c(INV_BLOCK)[i].flag) return InvP2Index(i);
    return -1;
}

// ---------------------------------------------------------------- the rows and [Gameplay] ExtendedToolbelt / StackableItems
static void InvSetLocked(int which, int v)
{
    if (v < 0 || v > 1) return;
    if (which) {
        InterlockedExchange(&g_invStSet, v);
        InvSaveIni("StackableItems", v, &g_invStUnsaved);
    } else {
        InterlockedExchange(&g_invTbSet, v);
        InvSaveIni("ExtendedToolbelt", v, &g_invTbUnsaved);
    }
    Log("gameplay: %s %s%s", which ? "StackableItems" : "ExtendedToolbelt", kInvName[v],
        InvPending() ? (g_invTickSeen ? " (applied at the next game tick with the bar, chest and menus closed)" : "") : "");
}
static int InvInput(int which)
{
    int v;
    if (!g_invReady || !(which ? g_invStOk : g_invTbOk)) {
        ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue((int)(which ? g_invStSet : g_invTbSet), 2);
    if (v == 0 && !which && g_invTbSet && !InvOffAllowed()) v = -1;  // the move would not fit: refused here, with the buzzer
    if (v >= 0) InvSetLocked(which, v);
    GpUnlock();
    if (v >= 0 && !g_invTickSeen && !*(volatile const int *)0x108EC8C && !*(volatile const int *)0x108EC98) InvReconcile();
    return 0;
}
static int __cdecl InvTbIn(BYTE *cfg, int count) { (void)cfg; (void)count; return InvInput(0); }
static int __cdecl InvStIn(BYTE *cfg, int count) { (void)cfg; (void)count; return InvInput(1); }
static int __cdecl InvTbGet(void) { return g_invTbOk ? (int)g_invTbSet : 0; }  // unavailable: shown Off
static int __cdecl InvStGet(void) { return g_invStOk ? (int)g_invStSet : 0; }
static int __cdecl InvTbDesc(void) { return MSG_INV0 + (!g_invTbOk ? INVM_NA : g_invTbSet ? INVM_TBD1 : INVM_TBD0); }
static int __cdecl InvStDesc(void) { return MSG_INV0 + (!g_invStOk ? INVM_NA : g_invStSet ? INVM_STD1 : INVM_STD0); }
static void InvIds(void)  // Off / On: One-Hit Kill's value texts (as No Clip's row)
{
    g_invIds[0] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 0;
    g_invIds[1] = MSG_GP0 + GPM_V0 + GP_OHK * GP_VMAX + 1;
}
static void InvToolbeltFillRow(DWORD *r)  // a complete 7-dword row (W1 places it on Option 5 above Item Shine)
{
    InvIds();
    FillRow(r, InvTbIn, InvTbGet, g_invIds, 2, MSG_INV0 + INVM_TBL, InvTbDesc);
}
static void InvStackFillRow(DWORD *r)
{
    InvIds();
    FillRow(r, InvStIn, InvStGet, g_invIds, 2, MSG_INV0 + INVM_STL, InvStDesc);
}
static const BYTE *InvText(int id)
{
    return id >= MSG_INV0 && id < MSG_INV0 + INVM_N ? g_txtInv[id - MSG_INV0] : g_txtInv[INVM_NA];
}
static void InvTexts(void)
{
    Encode(g_txtInv[INVM_TBL], 64, "Extended Toolbelt");
    Encode(g_txtInv[INVM_TBD0], 64, "A second belt page: 20 items on two pages.");
    Encode(g_txtInv[INVM_TBD1], 64, "On: Left / Right past the belt's ends flips the page.");
    Encode(g_txtInv[INVM_STL], 64, "Stackable Items");
    Encode(g_txtInv[INVM_STD0], 64, "The same items stack, up to 99 an entry.");
    Encode(g_txtInv[INVM_STD1], 64, "On: pickups, gifts and the chest join a stack.");
    Encode(g_txtInv[INVM_NA], 64, "Not available in this game (see SH4Refined.log).");
}
static int InvParse(const char *s) { return !_stricmp(s, "On") || !strcmp(s, "1"); }
static void InvReadIni(void)
{
    char buf[32];
    IniStr("Gameplay", "ExtendedToolbelt", buf, sizeof buf);
    g_invTbSet = InvParse(buf);
    IniStr("Gameplay", "StackableItems", buf, sizeof buf);
    g_invStSet = InvParse(buf);
}
static void InvIniReread(void)  // under GpLock (GpIniReread)
{
    static const char *const kKey[2] = { "ExtendedToolbelt", "StackableItems" };
    char buf[32];
    int k, v;
    if (g_iniFallback) return;
    for (k = 0; k < 2; ++k) {
        volatile LONG *set = k ? &g_invStSet : &g_invTbSet;
        if (k ? g_invStUnsaved : g_invTbUnsaved) continue;
        GetPrivateProfileStringA("Gameplay", kKey[k], kInvName[*set ? 1 : 0], buf, sizeof buf, g_iniPath);
        v = InvParse(buf);
        if (v == (int)*set) continue;
        InterlockedExchange(set, v);  // applied at the next idle tick (Off: the same move as the row; refused = stays on)
        if (k) g_invStSnap = v;
        else g_invTbSnap = v;
        Log("gameplay: [Gameplay] %s read again: %s", kKey[k], kInvName[v]);
    }
}
static void InvSnapshot(void) { g_invTbSnap = g_invTbSet, g_invStSnap = g_invStSet; }
static int InvChanged(void) { return g_invTbSet != g_invTbSnap || g_invStSet != g_invStSnap; }
static int InvRowChanged(int which) { return g_snapOn && (which ? g_invStSet != g_invStSnap : g_invTbSet != g_invTbSnap); }
static void InvDiscard(void)  // under GpLock (GpDiscard)
{
    if (g_invTbSet != g_invTbSnap) InvSetLocked(0, (int)g_invTbSnap);
    if (g_invStSet != g_invStSnap) InvSetLocked(1, (int)g_invStSnap);
}

// ---------------------------------------------------------------- install (GameplayInstall, Init: after the ini read)
static void InvInstall(void)
{
    int i, tb = 1, st = 1, gd = 1, off = 0;
    g_invBoot = 1;
    InvPkCave();  // the label addresses
    InvStCave();
    InvTexts();
    InvIds();
    InvReadIni();
    for (i = 0; i < INV_PK_SITES; ++i) {
        Site s;
        s.va = kPkSites[i].va, s.len = kPkSites[i].len;
        memcpy(s.stock, kPkSites[i].stock, s.len);
        if (!SiteOk(&s)) {
            if (tb) Log("extended toolbelt: %08lX (%s) is not the game's code (another mod?): the option does nothing", s.va, kPkSites[i].ref);
            tb = 0;
        }
    }
    for (i = 0; i < INV_ST_SITES; ++i) {
        Site s;
        s.va = kStSites[i].va, s.len = kStSites[i].len;
        memcpy(s.stock, kStSites[i].stock, s.len);
        if (!SiteOk(&s)) {
            int *f = InvStGuard(s.va) ? &gd : &st;
            if (*f) Log("stackable items: %08lX (%s) is not the game's code (another mod?): %s", s.va, kStSites[i].ref,
                        InvStGuard(s.va) ? "the stack guards are not in" : "the option does nothing");
            *f = 0;
        }
    }
    g_invTbOk = tb;
    // hidden key [Gameplay] StackGuards=0: the three guards stay out (the game's code; Stackable Items then unavailable)
    if (gd && !g_iniFallback && !GetPrivateProfileIntA("Gameplay", "StackGuards", 1, g_iniPath)) {
        gd = 0, off = 1;
        Log("stackable items: [Gameplay] StackGuards=0 (hidden key): the stack guards stay out and Stackable Items is unavailable");
    }
    if (gd) g_invGuardOk = InvApplySet(kStSites, INV_ST_SITES, g_stLbl, g_invListedSt, INV_GUARDS, 1, "stack guards");
    g_invStOk = st && g_invGuardOk;  // Stackable Items needs its guards (the stacked break) too
    g_invReady = 1;
    if (g_invTbSet && g_invTbOk) InvTbOn();
    if (g_invStSet && g_invStOk) InvStSwitch(1);
    g_invBoot = 0;
    Log("inventory: Extended Toolbelt %s ([Gameplay] ExtendedToolbelt %s; 33 sites from 004F4A09 to 0053DED0), Stackable Items %s "
        "(StackableItems %s; 10 sites), stack guards %s (00463607 thief cap, 004F6A30 stacked break, 004F5DB2 use one: always)",
        !g_invTbOk ? "UNAVAILABLE" : g_invTbLive ? "on" : "off", kInvName[g_invTbSet ? 1 : 0],
        !g_invStOk ? "UNAVAILABLE" : g_invStOn ? "on" : "off", kInvName[g_invStSet ? 1 : 0], g_invGuardOk ? "in" : "OFF");
    if (!g_invGuardOk && !off) LogAnomaly("inventory", "the stack guards are not in: a saved stack above 10 of a stealable item can crash a Gum Head's steal");
}
