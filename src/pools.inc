// ================================================================ v3.36 TP336: a larger task pool and IK pool, and their live use
// The user (2026-11-20, v3.36 round) chose ranks 1 and 4 of the pool audit (staging/pool_audit/REPORT.md sections 2, 4,
// 7; notes_enemies.md 2). Every site below re-checked against SH4Decomp (src/system/task_553820.c, sys_553ad0.c,
// src/anim/sg_anime_pool.c, sg_anime.c) and re_kit/sh4.asm: staging/v336_feedback/w1_tasks_ik/PLAN.md has the proofs.
// Included by sh4windowmod.c after pickpool.inc (enemies.inc's EnmFreeActors is used); PoolsInstall runs at Init (DllMain,
// before the exe's own init) after EnemiesInstall (its 0x4F0A55 check holds 0x4F0A5F); PoolsTick from quicksave.inc's
// 0x4EF982 dispatcher. The pure part (sizes, the ten writes, the range helper, the IK list walk) is pools_sim.h.
//
// 1. Tasks ([Gameplay] TaskPool=400, hidden; 0..300 = the game's 300). gTasks 0x11B6020 holds 300 slots of 0x430; a full
//    pool returns NULL from SystemTaskAlloc 0x553A50 and 56 of its 94 callers write through it (a crash). The pool grows
//    in place into the draw packet buffer right behind it (0x1204860, 0x30000 bytes, reset every frame; on PC only
//    0x468CD0 (two register writes), 0x468060 (0 bytes: its node lists D_00FD0C1C are only ever cleared) and 0x565890
//    (one item-box sprite) take from it: well under 1 KB). The buffer keeps its end (= its top pointer 0x1234860) and
//    starts (N - 300) slots later. Ten imm32 (pools_sim.h TpPlan), all or nothing: the pool's end 0x553846, the three
//    counts 0x55385F / 0x553A40 / 0x553A72, the buffer's base 0x553BC9 / 0x553C04 / 0x553C5B / 0x553C88 and its two
//    limits 0x553BCE (Get: size - 0x1000) / 0x553C0A (Fix: size). Only 4 instructions hold the pool's base (0x553821,
//    0x553859, 0x553A26, 0x553A50: unchanged), nothing in the exe computes a slot index (the only `add r,430h` are the
//    four loops), no data pointer points into it, nothing saves it. New slots are taken only when the first 300 are in
//    use (first free), so stock play never reaches them. Refined's own walkers use TaskSlotOk / TaskSlotAt / g_tpN.
// 2. IK handles ([Gameplay] IkPool=512, hidden; 0..160 = the game's 160). gSgAnimeWorkIks 0xED73F4: 160 handles of 0x50
//    from the world arena (`push 0A0h` at 0x414A29 world reset, 0x414E2F boot -> GetSize 0x419A20 -> arena -> SetBuffer
//    0x419A30); the only alloc 0x419B00 (caller 0x418334, SgAnimeApplyRelation) returns NULL when empty and the limb is
//    just not bent to its target (silent). The two pushes ask for N (2 imm32, all or nothing); as the bone pool
//    (enemies.inc EnmBoneReset at 0x4F0A5A), the room change's `call 419AB0` at 0x4F0A5F also runs IkReset: the stock
//    reset (every handle freed: nothing holds one across 0x4F0A50), then a pool smaller than N gets a buffer of this dll
//    through the game's own SetBuffer (covers any path the two pushes miss).
// 3. The live use (audit section 7): every dispatcher tick counts the task slots in use (and those of the grown part),
//    walks the IK free list, reads the free actor records and the system heap's used units; one verbose line per room
//    left; an ANOMALY when fewer than TP_MARGIN task slots are free or the IK pool is empty.

static int g_tpWanted, g_tpGrown;           // [Gameplay] TaskPool asks for more than 300 / the ten writes are in
static int g_ikWanted, g_ikGrown, g_ikSite; // IkPool asks for more than 160 / the two pushes are in / 0x4F0A5F is in
static int g_ikN = IK_STOCK_N;              // the IK handles wanted
static BYTE *g_ikBuf;                       // IkReset's buffer (never freed: a stale handle still reads valid memory)
static int g_ikBufN;
static int g_tpTickOff, g_tpHighSaid, g_tpLowSaid, g_ikEmptySaid;
static DWORD g_tpRoomKey = 0xFFFFFFFFu;
static TpRoom g_tpRoom;
static int g_tpPeakUsed, g_tpPeakHigh, g_ikPeakUsed;
static volatile LONG g_tpQuotaRoom, g_tpQuotaLow, g_ikQuotaEmpty, g_tpQuotaFault, g_ikQuotaReset;

static int TpFaultNote(EXCEPTION_POINTERS *e, const char *where)
{
    if (LogQuota(&g_tpQuotaFault, 5))
        LogAnomaly("pools", "%s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
                   e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int TpPoke4(DWORD va, DWORD v) { return Poke(va, &v, 4); }

// all-or-nothing dword writes: every stock value first, then the writes; a failed write takes the earlier ones back
static int TpWriteAll(const TpDword *w, int n, const char *what)
{
    int i, k, bad = 0;
    for (i = 0; i < n; ++i)
        if (*(const DWORD *)(ULONG_PTR)w[i].va != w[i].stock) {
            Log("%s: %08X is not %08X (another mod?): not changed", what, w[i].va, w[i].stock);
            return 0;
        }
    for (i = 0; i < n && TpPoke4(w[i].va, w[i].val); ++i) {}
    if (i == n) return 1;
    for (k = i - 1; k >= 0; --k) bad += !TpPoke4(w[k].va, w[k].stock);
    Log("%s: %08X could not be written: the %d write(s) before it were taken back%s; not changed", what, w[i].va, i,
        bad ? " (some failed)" : "");
    if (bad) LogAnomaly("pools", "%s: %d write(s) could not be taken back: the code is mixed", what, bad);
    return 0;
}

static void TaskPoolInstall(void)
{
    static const Site kTp[] = {
        { 0x553821, { 0xBE, 0x20, 0x60, 0x1B, 0x01 }, 5 },                    // InitAll: mov esi,11B6020h (base, kept)
        { 0x553844, { 0x81, 0xFE, 0x60, 0x48, 0x20, 0x01 }, 6 },              //   cmp esi,1204860h (the end)
        { 0x553859, { 0xBE, 0x24, 0x64, 0x1B, 0x01, 0xBF, 0x2C, 0x01, 0x00, 0x00 }, 10 },  // RunGroup: mov esi,11B6424h; mov edi,12Ch
        { 0x553A26, { 0xB8, 0x20, 0x60, 0x1B, 0x01 }, 5 },                    // FindByFunc: mov eax,11B6020h
        { 0x553A3E, { 0x81, 0xF9, 0x2C, 0x01, 0x00, 0x00 }, 6 },              //   cmp ecx,12Ch
        { 0x553A50, { 0xBA, 0x20, 0x60, 0x1B, 0x01 }, 5 },                    // Alloc: mov edx,11B6020h
        { 0x553A71, { 0x3D, 0x2C, 0x01, 0x00, 0x00 }, 5 },                    //   cmp eax,12Ch
        { 0x553BC8, { 0x2D, 0x60, 0x48, 0x20, 0x01, 0x3D, 0x00, 0xF0, 0x02, 0x00 }, 10 },  // Get: sub eax,1204860h; cmp eax,2F000h
        { 0x553C02, { 0x81, 0xEA, 0x60, 0x48, 0x20, 0x01, 0x81, 0xFA, 0x00, 0x00, 0x03, 0x00 }, 12 },  // Fix: sub; cmp edx,30000h
        { 0x553C55, { 0xC7, 0x05, 0x60, 0x48, 0x23, 0x01, 0x60, 0x48, 0x20, 0x01 }, 10 },  // mov [1234860h],1204860h (0x553C50)
        { 0x553C82, { 0xC7, 0x05, 0x60, 0x48, 0x23, 0x01, 0x60, 0x48, 0x20, 0x01 }, 10 },  // the same (SystemFrameAllocBegin)
    };
    TpDword w[TP_NSITES];
    const int ini = GetPrivateProfileIntA("Gameplay", "TaskPool", TP_DEFAULT_N, g_iniPath);
    const int n = TpPoolN(ini);
    int i;
    g_tpWanted = n > TP_STOCK_N;
    if (!g_tpWanted) {
        Log("task pool: the game's 300 slots ([Gameplay] TaskPool=%d)", ini);
        return;
    }
    for (i = 0; i < (int)(sizeof kTp / sizeof kTp[0]); ++i)
        if (!SiteOk(&kTp[i])) {
            Log("task pool: %08lX is not stock (another mod?): the game's 300 slots", kTp[i].va);
            return;
        }
    // before the game's own init: the packet buffer's top pointer is still 0 and no slot is in use
    if (*(volatile const DWORD *)0x1234860 != 0 || *(volatile const DWORD *)(TP_BASE + 0x404) != 0) {
        Log("task pool: the game's tasks already run (the packet top %08lX): too late to grow; the game's 300 slots",
            *(volatile const DWORD *)0x1234860);
        return;
    }
    TpPlan(n, w);
    if (!TpWriteAll(w, TP_NSITES, "task pool")) return;
    g_tpN = n;
    g_tpGrown = 1;
    Log("task pool: %d slots (was 300) at 011B6020..%08X, in place: the draw packet buffer behind it now starts at %08X with "
        "%u KB (was 192 KB at 01204860; its use on PC is under 1 KB a frame). Slots 300 and up are taken only when the first "
        "300 are all in use (a full pool crashes the game)", n, TpEnd(n), TpEnd(n), TpPacketSize(n) / 1024u);
    LogV("task pool: 10 imm32: 00553846 (end %08X) 0055385F 00553A40 00553A72 (count %d) 00553BC9 00553C04 00553C5B 00553C88 "
         "(packet base) 00553BCE (Get limit %05X) 00553C0A (Fix limit %05X); [Gameplay] TaskPool=%d; Enemy Copies and Follow "
         "still keep 40 of the first 300 free (the grown part is the game's reserve)", w[0].val, n, w[5].val, w[7].val, ini);
}

// v3.36 TP336: 0x4F0A5F, the `call 419AB0` (SgAnimeWorkIkReset) in RoomLoadSetMark 0x4F0A50, right after the bone reset
// (enemies.inc EnmBoneReset): the stock reset frees every handle (nothing holds one across a room change), then a pool
// smaller than IkPool gets this dll's buffer through the game's own SgAnimeWorkIkSetBuffer 0x419A30 (it clears it,
// aligns it to 16 and sets 0xED73F4 .. 0xED7400). The old buffer stays allocated (the world arena) until the next world
// reset gives a new one.
static void __cdecl IkReset(void)
{
    ((void(__cdecl *)(void))0x419AB0)();
    __try {
        const int num = *(volatile const int *)0xED73F8;
        if (num >= g_ikN) return;
        if (g_ikBufN < g_ikN) {
            BYTE *b = (BYTE *)VirtualAlloc(NULL, (SIZE_T)g_ikN * IK_REC, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!b) {
                if (LogQuota(&g_ikQuotaReset, 5))
                    Log("IK pool: room change: no memory for %d handles (error %lu): it stays %d", g_ikN, GetLastError(), num);
                return;
            }
            g_ikBuf = b;  // (the old one may be the pool still: never freed)
            g_ikBufN = g_ikN;
        }
        ((void(__cdecl *)(void *, unsigned))0x419A30)(g_ikBuf, (unsigned)g_ikN * IK_REC);
        if (LogQuota(&g_ikQuotaReset, 20))
            Log("IK pool: room change: %d -> %d handles (this dll's buffer %p; the game's next world reset gives its own "
                "pool of %d again)", num, *(volatile const int *)0xED73F8, g_ikBuf, g_ikGrown ? g_ikN : IK_STOCK_N);
    } __except (TpFaultNote(GetExceptionInformation(), "the IK pool at a room change")) {
    }
}

static void IkPoolInstall(void)
{
    static const Site kIk[] = {
        { 0x414A29, { 0x68, 0xA0, 0x00, 0x00, 0x00, 0xE8, 0xED, 0x4F, 0x00, 0x00 }, 10 },  // world reset: push 0A0h; call 419A20
        { 0x414E2F, { 0x68, 0xA0, 0x00, 0x00, 0x00, 0xE8, 0xE7, 0x4B, 0x00, 0x00 }, 10 },  // boot: push 0A0h; call 419A20
        { 0x419A20, { 0x8B, 0x44, 0x24, 0x04, 0x8D, 0x04, 0x80, 0xC1, 0xE0, 0x04, 0xC3 }, 11 },  // GetSize: n * 0x50
    };
    static const Site kCalled[] = {
        { 0x419A30, { 0x53, 0x56, 0x8B, 0x74, 0x24, 0x10, 0x57, 0x8B, 0x7C, 0x24, 0x10, 0x56 }, 12 },  // SetBuffer (called)
        { 0x419AB0, { 0xA1, 0xF8, 0x73, 0xED, 0x00, 0x56, 0x8B, 0x35, 0x00, 0x74, 0xED, 0x00 }, 12 },  // Reset (called)
    };
    static const Site kRoom = { 0x4F0A5F, { 0xE8, 0x4C, 0x90, 0xF2, 0xFF }, 5 };  // RoomLoadSetMark: call 419AB0
    TpDword w[2];
    const int ini = GetPrivateProfileIntA("Gameplay", "IkPool", IK_DEFAULT_N, g_iniPath);
    int i, ok = 1;
    g_ikN = IkPoolN(ini);
    g_ikWanted = g_ikN > IK_STOCK_N;
    if (!g_ikWanted) {
        Log("IK pool: the game's 160 handles ([Gameplay] IkPool=%d)", ini);
        return;
    }
    for (i = 0; i < (int)(sizeof kIk / sizeof kIk[0]) && ok; ++i)
        if (!SiteOk(&kIk[i])) {
            Log("IK pool: %08lX is not stock (another mod?): the world reset and the boot keep 160 handles", kIk[i].va);
            ok = 0;
        }
    // v3.36 REV336 IK-001 (Codex): the functions the larger count flows into (SetBuffer 0x419A30, Reset 0x419AB0) are
    // checked BEFORE either count is written: a changed one leaves the stock counts and the room change untouched
    for (i = 0; ok && i < (int)(sizeof kCalled / sizeof kCalled[0]); ++i)
        if (!SiteOk(&kCalled[i])) {
            Log("IK pool: %08lX is not the stock function (another mod?): the game keeps 160 handles, no rebuild at a room "
                "change", kCalled[i].va);
            ok = 0;
        }
    if (ok) {
        w[0].va = 0x414A2A, w[0].stock = 0xA0, w[0].val = (unsigned)g_ikN;
        w[1].va = 0x414E30, w[1].stock = 0xA0, w[1].val = (unsigned)g_ikN;
        g_ikGrown = TpWriteAll(w, 2, "IK pool");
    }
    if (!g_ikGrown) {
        // v3.36 REV336 IK-001: no larger pool, no room-change rebuild
    } else if (!SiteOk(&kRoom))
        Log("IK pool: 004F0A5F differs from the GOG v7 exe (another mod?): no rebuild at a room change");
    else if (PokeBranch(0x4F0A5F, 0xE8, IkReset, 5))
        g_ikSite = 1;
    else
        Log("IK pool: 004F0A5F could not be written: no rebuild at a room change");
    if (g_ikGrown || g_ikSite)
        Log("IK pool: %d handles (was 160; %u KB of the world arena)%s%s; [Gameplay] IkPool=%d", g_ikN,
            (unsigned)g_ikN * IK_REC / 1024u, g_ikGrown ? " at the boot and every world reset (00414E30, 00414A2A)" : "",
            g_ikSite ? ", checked at every room change (004F0A5F)" : "", ini);
}

static void PoolsInstall(void)  // Init, after EnemiesInstall
{
    TaskPoolInstall();
    IkPoolInstall();
}

// ---------------------------------------------------------------- the live use (QsTick, 0x4EF982, game thread)
static void TpRoomLine(void)
{
    const TpRoom *r = &g_tpRoom;
    char ik[96];
    if (!r->samples || !LogQuota(&g_tpQuotaRoom, 600)) return;
    if (r->ikFreeMin == 0x7FFFFFFF) sprintf_s(ik, sizeof ik, "IK list unreadable (%d samples)", r->ikBad);
    else sprintf_s(ik, sizeof ik, "IK handles %d in use at most (fewest free %d of %d)", r->ikUsedMax, r->ikFreeMin,
                   r->ikNum);
    LogV("pools: room %X:%02X left: tasks %d of %d in use at most (fewest free %d; slots 300+ held %d at most), %s, actor "
         "records fewest free %d of 300, heap %d of 65536 units at most; this session at most tasks %d (slots 300+ %d), "
         "IK %d", (int)(g_tpRoomKey >> 16), (int)(g_tpRoomKey & 0xFFFF), r->tpUsedMax, g_tpN, r->tpFreeMin, r->tpHighMax, ik,
         r->actFreeMin == 0x7FFFFFFF ? -1 : r->actFreeMin, r->heapMax, g_tpPeakUsed, g_tpPeakHigh, g_ikPeakUsed);
}

static void PoolsTick(void)
{
    DWORD key;
    int i, used = 0, high = 0, ikNum = 0, ikFree = -1, actFree, heap;
    if (g_tpTickOff) return;
    __try {
        for (i = 0; i < g_tpN; ++i)
            if (*(volatile const DWORD *)(TaskSlotAt(i) + 0x404)) {
                ++used;
                if (i >= TP_STOCK_N) ++high;
            }
        ikNum = *(volatile const int *)0xED73F8;
        ikFree = IkCountFree(*(const BYTE *volatile const *)0xED73F4, *(const BYTE *volatile const *)0xED7400, ikNum);
        actFree = EnmFreeActors();
        heap = *(volatile const int *)0x11A5FE0;
    } __except (TpFaultNote(GetExceptionInformation(), "the pool count")) {
        g_tpTickOff = 1;  // (the patches stay; only this log stops)
        return;
    }
    key = (DWORD)*(volatile int *)0xFD5A90 << 16 | (DWORD)(*(volatile int *)0xFD5A94 & 0xFFFF);
    if (key != g_tpRoomKey) {
        if (g_tpRoomKey != 0xFFFFFFFFu) TpRoomLine();
        TpRoomClear(&g_tpRoom);
        g_tpRoomKey = key;
        g_tpLowSaid = g_ikEmptySaid = 0;
    }
    TpRoomAdd(&g_tpRoom, g_tpN, used, high, ikNum, ikFree, actFree, heap);
    if (used > g_tpPeakUsed) g_tpPeakUsed = used;
    if (high > g_tpPeakHigh) g_tpPeakHigh = high;
    if (ikFree >= 0 && ikNum - ikFree > g_ikPeakUsed) g_ikPeakUsed = ikNum - ikFree;
    if (high && !g_tpHighSaid) {
        g_tpHighSaid = 1;
        LogV("task pool: the grown part is in use for the first time this session: %d of slots 300..%d (%d of %d in use) in "
             "room %X:%02X", high, g_tpN - 1, used, g_tpN, (int)(key >> 16), (int)(key & 0xFFFF));
    }
    if (g_tpN - used < TP_MARGIN && !g_tpLowSaid && LogQuota(&g_tpQuotaLow, 20)) {
        g_tpLowSaid = 1;
        LogAnomaly("task pool", "only %d of %d task slots free in room %X:%02X (the game crashes when none is left: 56 of the "
                   "94 callers of 00553A50 use the slot unchecked)%s", g_tpN - used, g_tpN, (int)(key >> 16),
                   (int)(key & 0xFFFF), g_tpGrown ? "" : "; [Gameplay] TaskPool=400 grows it");
    }
    if (ikFree == 0 && ikNum > 0 && !g_ikEmptySaid && LogQuota(&g_ikQuotaEmpty, 20)) {
        g_ikEmptySaid = 1;
        LogAnomaly("IK pool", "all %d IK handles in use in room %X:%02X: limbs set up from now on are not bent to their "
                   "targets (silent in the game); [Gameplay] IkPool (hidden, asked %d) sets the size", ikNum, (int)(key >> 16),
                   (int)(key & 0xFFFF), g_ikN);
    }
}
