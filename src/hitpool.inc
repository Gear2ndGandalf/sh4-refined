// ================================================================ v3.36 HE336: the hit-shape pool, larger, and the stomp guard
// The user (2026-11-20, v3.36 round): the pool audit's rank 2 (staging/pool_audit/REPORT.md sections 1, 4, 7; its
// notes_sound_file_hit.md section 1). Plan: staging/v336_feedback/w2_hits_evict/PLAN.md. Every byte below re-checked
// against SH4Decomp src/hit/hit_5613e0.c, src/player/pl_weapon.c (PlayerStompHitTest, matched) and re_kit/sh4.asm.
// Included by sh4windowmod.c after pickpool.inc; HitPoolInstall runs at Init (DllMain, before the exe's own init builds
// the pool at HitInit 0x561164); HpTick from quicksave.inc's 0x4EF982 dispatcher.
//
// The game's collision shapes (HitShape, 0xD0 bytes) come from 100 shapes at 0x12F1C30 on a free list (head 0x12F1C20,
// link +0xC0). Every enemy takes its model's shapes + a root sphere at its create and up to 3 attack volumes while it
// attacks; the player, Eileen, weapons, the room camera, lights and room models take theirs. When the list is empty,
// 0x561480 returns NULL: the body gets fewer or no shapes, silently (an enemy made then cannot be hit, push or attack;
// a weapon has no hit box). Enemy Copies x4 + Enemies Follow can need 80-160 (the audit's estimate) and more.
//
// Who touches the pool (all checked 2026-11-20): only 0x561420 (chain: base 0x561421, count 0x561426, head 0x56144B),
// 0x561460 (clear 0x1450 dwords, then jmp 0x561420), 0x561480 (alloc: pop, tag +0xC0 = 0x50765315), 0x5614B0 (free:
// push back only a tagged shape) refer to 0x12F1C20..0x12F6D70 (sh4.asm immediates and data_map: 5 + 3 code refs, no
// data refs). Callers: 0x561420 from 0x4F0A55 (RoomLoadSetMark, every room change: all shapes free again, re-chained
// WITHOUT a clear) and 0x561470 (from 0x561460, whose only caller is HitInit 0x561164); 0x561480 from 0x560F20 only,
// 0x5614B0 from 0x560EEF only. Nothing turns a shape pointer into an index or range-checks it (no sub / div by 0xD0 on
// a shape); bodies hold plain pointers (HitBody.shape[6]). Shapes are never saved (SaveSnapshotBuild 0x4FC0E0 copies
// story bits, inventory, enemy manager, pickups, player stats / candles / pins, Eileen, map, haunting, doors, return
// place: no body or shape). Outside the exe only Refined's echo.inc EchoHurtOk range-checked a shape (now
// HpShapeInPool); Playable Eileen and the cheat table touch none of the sites (ct_conflicts --span clean).
//
// Stage 1, the guard (always when its bytes are stock; no switch):
//   0x544AEF (6) `cmp byte [10A4F3Ch],bl` -> jmp hkHpStomp: PlayerStompHitTest 0x5448C0 reads sStompBody.shape[0]
//            (0x10A4F24, count 0x10A4F3C) and writes the stomp box through it at 0x544B11 (`xor eax,eax` at 0x544B00 when
//            the body has no shape: a write to address 0). hkHpStomp: a shape -> 0x544B02 (stock); none -> eax = 0 and
//            0x544B68 (sHitTested = 1, the stock epilogue): the stomp tests nothing that tick and hits nothing. The
//            bytes 0x544AF5..0x544B01 become unreachable (their only jump-ins, 0x544AF5's jbe and 0x544AFE's jne, are in
//            them); the x87 stack is empty at 0x544AEF on every path in.
//   0x5614A2 (5) `xor eax,eax; ret` (the alloc's empty exit) -> jmp hkHpEmpty: counts the refusal, a rate-limited
//            ANOMALY line, then NULL as before.
// Stage 2, the larger pool ([Gameplay] HitPool=356, hidden; 0 or 100 = the stock pool, 101 .. 612 shapes in all):
//   0x561455 (5) the chain's `ret` (+ 4 nops of padding) -> jmp hkHpChain: after the stock 100 are chained, the last
//            stock shape's link (0x12F6D60) points at this dll's block of HitPool - 100 shapes, chained the same way,
//            its last link 0. Both callers get it (every room change, and HitInit, which also clears our block as its
//            0x561460 cleared the game's 100). The free list is LIFO and starts with the stock 100, so the block is used
//            only after the game's own shapes are all in use: stock play is unchanged.
#include "hitpool_sim.h"

static __declspec(align(16)) BYTE g_hpBlk[HP_EXTRA_MAX * HP_SHAPE_SIZE];
static int g_hpExtra;                   // shapes of g_hpBlk linked after the game's 100 (0 = the stock pool)
static int g_hpGuardOn, g_hpTickOff, g_hpIni = HP_DEFAULT_N;
static LONG g_hpChains, g_hpLowest = -1;  // (game thread)
static volatile LONG g_hpRefused, g_hpStomps;
static volatile LONG g_hpQuotaDry, g_hpQuotaStomp, g_hpQuotaRoom, g_hpQuotaFault, g_hpQuotaBroken;
static HpRoom g_hpRoom;
static DWORD g_hpRoomKey = 0xFFFFFFFFu;
static const DWORD g_hpStompBack = 0x544B02, g_hpStompSkip = 0x544B68;

static int HpTotal(void) { return HP_STOCK_N + g_hpExtra; }
// echo.inc EchoHurtOk: a shape of the game's pool or of this dll's block
static int HpShapeInPool(const BYTE *hs)
{
    return HpIsShape((size_t)(ULONG_PTR)hs, HP_STOCK_BASE, HP_STOCK_N, (size_t)(ULONG_PTR)g_hpBlk, g_hpExtra);
}

static int HpFaultNote(EXCEPTION_POINTERS *e, const char *where)
{
    if (LogQuota(&g_hpQuotaFault, 5))
        LogAnomaly("hit shapes", "%s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
                   e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// 0x561455 (hkHpChain; game thread): the stock 100 are chained; ours go after them. ret = 0x561420's caller's return:
// 0x561169 = HitInit's clear 0x561460 (ours are cleared too), 0x4F0A5A = a room change (RoomLoadSetMark: no clear).
static void __cdecl HpChainC(DWORD ret)
{
    const int n = g_hpExtra;
    if (n <= 0) return;
    if (ret == 0x561169) memset(g_hpBlk, 0, (size_t)n * HP_SHAPE_SIZE);
    HpChain(g_hpBlk, n);
    HpLinkPut((BYTE *)(ULONG_PTR)(HP_STOCK_BASE + (HP_STOCK_N - 1) * HP_SHAPE_SIZE), g_hpBlk);  // 0x12F6D60
    if (++g_hpChains == 1 || (ret != 0x561169 && ret != 0x4F0A5A && LogQuota(&g_hpQuotaFault, 5)))
        LogV("hit shapes: the pool was built (return %08lX): the game's 100 shapes, then %d in this dll", ret, n);
}
static __declspec(naked) void hkHpChain(void)
{
    __asm {
        push dword ptr [esp]        // 0x561420's return address
        call HpChainC
        add esp, 4
        ret
    }
}

// 0x5614A2 (hkHpEmpty; game thread): the pool is empty, the body asking (0x560F20) gets no shape
static void __cdecl HpEmptyC(void)
{
    const LONG n = InterlockedIncrement(&g_hpRefused);
    ++g_hpRoom.refused;
    if (LogQuota(&g_hpQuotaDry, 20))
        LogAnomaly("hit shapes", "the pool is empty: all %d shapes are in use and a body got none (an enemy made now cannot be "
                   "hit or attack, a weapon has no hit box; stage %d room %d); %ld refusal(s) this session%s", HpTotal(),
                   (int)*(volatile int *)0xFD5A90, (int)*(volatile int *)0xFD5A94, n,
                   g_hpExtra ? "" : " ([Gameplay] HitPool=0: the stock 100)");
}
static __declspec(naked) void hkHpEmpty(void)
{
    __asm {
        call HpEmptyC
        xor eax, eax                // the stock exit: NULL
        ret
    }
}

// 0x544AEF (hkHpStomp; game thread): the stomp body has no shape: no stomp test this tick (the stock game writes the box
// through address 0 at 0x544B11)
static void __cdecl HpStompSkipped(void)
{
    const LONG n = InterlockedIncrement(&g_hpStomps);
    ++g_hpRoom.stomps;
    if (LogQuota(&g_hpQuotaStomp, 10))
        LogAnomaly("hit shapes", "the stomp's body has no hit shape (the pool was empty when the player was made): the stomp hits "
                   "nothing (the stock game crashes here at 00544B11); stage %d room %d, %ld time(s) this session",
                   (int)*(volatile int *)0xFD5A90, (int)*(volatile int *)0xFD5A94, n);
}
static __declspec(naked) void hkHpStomp(void)
{
    __asm {
        cmp byte ptr ds:[10A4F3Ch], bl     // the stock 0x544AEF: sStompBody.shape_count against bl (0)
        jbe hp_none
        mov eax, dword ptr ds:[10A4F24h]   // its shape[0] (the stock 0x544AF7)
        test eax, eax
        jz hp_none
        cmp eax, ebx                       // (the stock test, ebx = 0 there)
        je hp_none
        jmp dword ptr [g_hpStompBack]      // 0x544B02: the stock box write and test
    hp_none:
        call HpStompSkipped
        xor eax, eax                       // the test's result: no hit
        jmp dword ptr [g_hpStompSkip]      // 0x544B68: sHitTested = 1, pop esi / ebx, the stock epilogue
    }
}

// QsTick (0x4EF982, game thread): the free list is walked every tick (at most 613 links); one verbose line per room left
static void HpTick(void)
{
    DWORD key;
    int freeN;
    if (g_hpTickOff) return;
    __try {
        freeN = HpCountFree(*(const BYTE *const *)(ULONG_PTR)HP_STOCK_HEAD, HP_STOCK_BASE, HP_STOCK_N,
                            (size_t)(ULONG_PTR)g_hpBlk, g_hpExtra);
    } __except (HpFaultNote(GetExceptionInformation(), "the free-shape count")) {
        g_hpTickOff = 1;  // (the patches stay; only the count stops)
        InterlockedExchange(&g_enmHitFree, -1);  // v3.38 REV338 EN338-004: Enemies Follow's arrivals wait (count not known)
        return;
    }
    if (freeN < 0 && LogQuota(&g_hpQuotaBroken, 3))
        LogAnomaly("hit shapes", "the free list does not walk (a link outside the %d shapes, or a loop): not counted this tick",
                   HpTotal());
    key = (DWORD)*(volatile int *)0xFD5A90 << 16 | (DWORD)(*(volatile int *)0xFD5A94 & 0xFFFF);
    if (key != g_hpRoomKey) {
        if (g_hpRoomKey != 0xFFFFFFFFu && g_hpRoom.ticks && LogQuota(&g_hpQuotaRoom, 1500))
            LogV("hit shapes: room %d:%d left: lowest free %d of %d (%d in use at most), %d refusal(s) here (%ld this session), "
                 "stomp guard %d here; session lowest free %ld; %s", (int)(g_hpRoomKey >> 16), (int)(g_hpRoomKey & 0xFFFF),
                 g_hpRoom.lowest, HpTotal(), HpTotal() - g_hpRoom.lowest, g_hpRoom.refused, g_hpRefused, g_hpRoom.stomps,
                 g_hpLowest, g_hpExtra ? "the game's 100 + this dll's block" : "the stock pool");
        HpRoomStart(&g_hpRoom);
        g_hpRoomKey = key;
    }
    HpRoomSee(&g_hpRoom, freeN);
    g_enmHitFree = freeN;  // v3.38 EN338: Enemies Follow's hit-shape gate (enemies.inc, keeps 24 free)
    if (freeN >= 0 && (g_hpLowest < 0 || freeN < g_hpLowest)) g_hpLowest = freeN;
}

static void HitPoolInstall(void)
{
    static const Site kGuard[] = {
        // cmp byte [10A4F3C],bl / jbe 544B00 / mov eax,[10A4F24] / cmp eax,ebx / jne 544B02 / xor eax,eax
        { 0x544AEF, { 0x38, 0x1D, 0x3C, 0x4F, 0x0A, 0x01, 0x76, 0x09, 0xA1, 0x24, 0x4F, 0x0A, 0x01, 0x3B, 0xC3, 0x75, 0x02,
                      0x33, 0xC0 }, 19 },
        { 0x544B02, { 0xD9, 0x44, 0x24, 0x10 }, 4 },  // fld [esp+10h] (the way back)
        // mov dword [10A4FC4],1 / pop esi / pop ebx / mov esp,ebp / pop ebp / ret (the skip's exit)
        { 0x544B68, { 0xC7, 0x05, 0xC4, 0x4F, 0x0A, 0x01, 0x01, 0x00, 0x00, 0x00, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC3 }, 16 },
        // 0x561480 alloc: mov eax,[12F1C20] / test / je 5614A2 / pop and tag / ret / 5614A2: xor eax,eax / ret / nop nop
        { 0x561480, { 0xA1, 0x20, 0x1C, 0x2F, 0x01, 0x85, 0xC0, 0x74, 0x19, 0x8B, 0x90, 0xC0, 0x00, 0x00, 0x00, 0x8D, 0x88 }, 17 },
        { 0x561491, { 0xC0, 0x00, 0x00, 0x00, 0x89, 0x15, 0x20, 0x1C, 0x2F, 0x01, 0xC7, 0x01, 0x15, 0x53, 0x76, 0x50, 0xC3 }, 17 },
        { 0x5614A2, { 0x33, 0xC0, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }, 14 },
    };
    static const Site kPool[] = {
        // 0x561420 chain: mov eax,12F1C30h / mov edx,63h / lea ebx,[ebx] / loop / last 0 / head / ret / 10 nops
        { 0x561420, { 0xB8, 0x30, 0x1C, 0x2F, 0x01, 0xBA, 0x63, 0x00, 0x00, 0x00, 0x8D, 0x9B, 0x00, 0x00, 0x00, 0x00 }, 16 },
        { 0x561430, { 0x4A, 0x8D, 0x88, 0xD0, 0x00, 0x00, 0x00, 0x89, 0x88, 0xC0, 0x00, 0x00, 0x00, 0x8B, 0xC1, 0x75, 0xEF }, 17 },
        { 0x561441, { 0xC7, 0x80, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x05, 0x20, 0x1C, 0x2F, 0x01, 0x30, 0x1C,
                      0x2F, 0x01, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }, 31 },
        // 0x561460 clear: push edi / mov ecx,1450h / xor eax,eax / mov edi,12F1C30h / rep stosd / pop edi / jmp 561420
        { 0x561460, { 0x57, 0xB9, 0x50, 0x14, 0x00, 0x00, 0x33, 0xC0, 0xBF, 0x30, 0x1C, 0x2F, 0x01, 0xF3, 0xAB, 0x5F, 0xE9, 0xAB,
                      0xFF, 0xFF, 0xFF }, 21 },
        // 0x5614B0 free: push back only a tagged shape
        { 0x5614B0, { 0x8B, 0x44, 0x24, 0x04, 0x81, 0xB8, 0xC0, 0x00, 0x00, 0x00, 0x15, 0x53, 0x76, 0x50, 0x75, 0x11, 0x8B }, 17 },
        { 0x5614C1, { 0x0D, 0x20, 0x1C, 0x2F, 0x01, 0x89, 0x88, 0xC0, 0x00, 0x00, 0x00, 0xA3, 0x20, 0x1C, 0x2F, 0x01, 0xC3 }, 17 },
        { 0x561164, { 0xE8, 0xF7, 0x02, 0x00, 0x00 }, 5 },  // HitInit's call 561460 (its return 0x561169: the clear)
    };
    GpPatch set[2];
    const int n = HpPoolN(g_hpIni = GetPrivateProfileIntA("Gameplay", "HitPool", HP_DEFAULT_N, g_iniPath));
    int i, guardOk = 1, poolOk = n > HP_STOCK_N;
    HpRoomStart(&g_hpRoom);
    for (i = 0; i < (int)(sizeof kGuard / sizeof kGuard[0]); ++i)
        if (!SiteOk(&kGuard[i])) {
            Log("hit shapes: %08lX is not stock (another mod?): no stomp guard and no empty-pool count (a stomp with no shape "
                "crashes as in the stock game)", kGuard[i].va);
            guardOk = 0;
            break;
        }
    if (guardOk) {  // stage 1: the guard and the empty-pool count (both or neither)
        GpBranch(&set[0], 0x544AEF, 0xE9, hkHpStomp, 6);
        GpBranch(&set[1], 0x5614A2, 0xE9, hkHpEmpty, 5);
        g_hpGuardOn = GpApplyAll(set, 2, "hit shapes (the stomp guard)");
    }
    if (poolOk)
        for (i = 0; i < (int)(sizeof kPool / sizeof kPool[0]); ++i)
            if (!SiteOk(&kPool[i])) {
                Log("hit shapes: %08lX is not stock (another mod?): the pool stays the game's 100 shapes", kPool[i].va);
                poolOk = 0;
                break;
            }
    if (poolOk) {  // stage 2: the block (one write; the count is set first: the hook reads it)
        g_hpExtra = n - HP_STOCK_N;
        GpBranch(&set[0], 0x561455, 0xE9, hkHpChain, 5);
        if (!GpApplyAll(set, 1, "hit shapes (the larger pool)")) g_hpExtra = 0, poolOk = 0;
    }
    if (poolOk)
        Log("hit shapes: %d shapes: the game's 100 at 012F1C30, then %d in this dll at %08X (%u KB), linked after them at "
            "every pool build (00561455: HitInit and every room change); stomp guard %s, empty-pool count %s; [Gameplay] "
            "HitPool=%d", n, g_hpExtra, (unsigned)(ULONG_PTR)g_hpBlk, (unsigned)(g_hpExtra * HP_SHAPE_SIZE / 1024),
            g_hpGuardOn ? "in (00544AEF)" : "OFF", g_hpGuardOn ? "in (005614A2)" : "OFF", g_hpIni);
    else
        Log("hit shapes: the game's 100 shapes at 012F1C30%s; stomp guard %s, empty-pool count %s",
            n > HP_STOCK_N ? "" : " ([Gameplay] HitPool=0)", g_hpGuardOn ? "in (00544AEF: no stomp test instead of a crash "
            "when the stomp has no shape)" : "OFF", g_hpGuardOn ? "in (005614A2)" : "OFF");
}
