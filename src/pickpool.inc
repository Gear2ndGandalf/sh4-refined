// ================================================================ v3.35 PK335: the dynamic pickup pool, larger and crash-safe
// The user (2026-11-20, v3.35 round): "drops capacities need to increase so we do not crash". RE: staging/v335_feedback/
// research_pickup_pool/REPORT.md (sections 5-7) and w4_pool/PLAN.md; every byte below re-checked against SH4Decomp
// src/item/pickup_4f3a90.c and re_kit/sh4.asm. Included by sh4windowmod.c before drops.inc; PickPoolInstall runs at Init
// (DllMain, before the exe's own init: the pool is still zero); PpTick from quicksave.inc's 0x4EF982 dispatcher.
//
// The game's pickups made at run time (enemy drops, Gum Head returns, the pinned ghosts' Swords, Supply Drops) live in
// 32 records of 0x80 bytes at 0x1077610 (gPickupRecords), all of them in the byte-matched module 0x4F3A90-0x4F42E1.
// Stock, a full pool crashes every caller: PickupSpawnDynamic 0x4F3E90 still makes the object with a NULL record, then
// 0x4F4120 writes through it (0x4F4127) and 0x4F4240 writes [eax+1] (0x4F42A3).
//
// Stage 1, the guard (always, when its bytes are stock; no switch): "pool full" becomes "no pickup" for every caller.
//   0x4F3F74 (5) `mov [13D8210],eax` -> jmp hkPpGuard: the record stored as before; NULL -> the stock failure exit of
//                0x4F3E90 without the object (add esp,4 = the alloc's argument pushed at 0x4F3F6A; pop edi as 0x4F3F9E;
//                jmp 0x4F3FB2 = xor eax,eax / pop ebx / add esp,94h / ret), so the callers' own NULL checks work
//                (0x463150 / 0x463240 return 0, 0x463340 keeps the item in the steal table). Only eax and the flags
//                change, as the stock path after 0x4F3F74 (it reads no flag before its own cmp at 0x4F3F95).
//   0x4F429C (1) `74 06` je 4F42A3 -> `74 0A` je 4F42A7: 0x4F4240 skips `mov byte [eax+1],1` when there is no record.
// Stage 2, the move ([Gameplay] PickupPool=128, hidden; 0 = the stock pool, the guard only): the pool becomes N records
// in this dll (g_ppBuf, 16-aligned, pinned dll), 13 dword writes:
//   0x4F3F70 call 4F3BA0 -> hkPpAlloc (PpAllocC: the matched alloc over N, the record zeroed first; Supply Drops' own
//            spawns, flagged by drops.inc through PpPreferHigh, search slots 32..N-1 first);
//   the loop bounds 0x4F4018 / 0x4F409B (FindNear, base / end + 0x18), 0x4F40B4 / 0x4F4112 (SpawnRoom, + 6),
//   0x4F4132 / 0x4F415C (the room-exit steal list), 0x4F419D / 0x4F41F2 (the pins' room move, + 6);
//   PickupInit 0x4F42D0 (size 0x1000 -> N * 0x80) / 0x4F42D5 (base);
//   0x4FC1CD call 4F3D50 (save) -> PpSave: the image keeps its 32 records (the save format and old saves unchanged):
//            the game's pin Swords, its other records, then Supply Drops' newest first; the rest zero;
//   0x4FC6B9 call 4F3D30 (load) -> PpLoad: the image's 32 into slots 0..31, slots 32..N-1 cleared.
// Left alone: gPickupZone 0x1078610 (= the old pool's end: 0x4F3ED0, 0x4F427B, 0x4F4289) and gPickupTaken 0x1078618
// (0x4F3B91, 0x4F420C, 0x4F422C, 0x4F42C6). 0x4F3D3E / 0x4F3D5E (the stock load / save copies) are no longer reached.
// No code outside the module indexes the pool (the exe's code and data hold no other address in 0x1077600..0x1078640
// but 0x4F2355's D_01077600, checked 2026-11-20); Refined reads it only through drops.inc's DROP_REC_VA / DROP_REC_N,
// which point at g_ppRecVA / g_ppRecN after the merge (BUILD_NOTES).

#include "pickpool_sim.h"

static __declspec(align(16)) BYTE g_ppBuf[PP_REC_MAX * PP_REC_SIZE];
static unsigned g_ppRecVA = 0x1077610u;  // the pool in use: 0x1077610 (stock) or g_ppBuf (drops.inc DROP_REC_VA)
static int g_ppRecN = PP_STOCK_N;        // its records (drops.inc DROP_REC_N)
static int g_ppMoved, g_ppGuardOn, g_ppPeak, g_ppAllocFail, g_ppTickOff;
static volatile LONG g_ppPreferHigh;     // drops.inc around its 0x463150 call (PpPreferHigh)
static volatile LONG g_ppGuardN, g_ppQuotaGuard, g_ppQuotaSave, g_ppQuotaLoad, g_ppQuotaRoom, g_ppQuotaFault;
static DWORD g_ppRoomKey = 0xFFFFFFFFu;
static int g_ppLastUse[3] = { -1, -1, -1 };
static const DWORD g_ppBack = 0x4F3F79, g_ppFail = 0x4F3FB2;
static int DropRecOurs(const BYTE *r);  // drops.inc (later in this translation unit): Supply Drops' marker at +0x74

// drops.inc: 1 around its own 0x463150 call (the drop takes a slot of 32..N-1 first, the game keeps the 32 a save holds)
static void PpPreferHigh(int on) { InterlockedExchange(&g_ppPreferHigh, on ? 1 : 0); }

static void PpViews(PpView *v, const BYTE *recs, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        const BYTE *r = recs + i * PP_REC_SIZE;
        v[i].state = r[0];
        v[i].sword = r[1] != 0;
        v[i].ours = r[0] && DropRecOurs(r);
        v[i].seq = v[i].ours ? (int)(*(const unsigned *)(r + 0x74) & 0xFFFu) : 0;
    }
}

static int PpFaultNote(EXCEPTION_POINTERS *e, const char *where)
{
    if (LogQuota(&g_ppQuotaFault, 5))
        LogAnomaly("pickup pool", "%s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
                   e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// PickupRecordAlloc 0x4F3BA0 over the moved pool (hkPpAlloc; game thread). NULL = full (the guard then makes no object).
static BYTE *__cdecl PpAllocC(const BYTE *def, int arg)
{
    BYTE *r = NULL;
    int i = -1, used = 0, k;
    __try {
        i = PpAllocPick(g_ppBuf, g_ppRecN, g_ppPreferHigh != 0);
        if (i >= 0) {
            r = g_ppBuf + i * PP_REC_SIZE;
            PpRecFill(r, def, arg, *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94);
            ((void(__cdecl *)(void *, const void *, float))0x421B30)(r + 0x20, (const void *)0x6112D0,
                                                                     *(const float *)(def + 0x20));  // MathQuatFromAxisAngle
            for (k = 0; k < g_ppRecN; ++k) used += g_ppBuf[k * PP_REC_SIZE] != 0;
            if (used > g_ppPeak) g_ppPeak = used;
        }
    } __except (PpFaultNote(GetExceptionInformation(), "the record alloc")) {
        if (r) r[0] = 0, *(short *)(r + 2) = 0;
        r = NULL;
        ++g_ppAllocFail;
    }
    if (r) LogT("pickup pool: record %d for item %d%s (%d of %d in use)", i, *(const int *)(r + 0x38),
                g_ppPreferHigh ? " (Supply Drops: slots 32 and up first)" : "", used, g_ppRecN);
    return r;
}

// 0x4F3F6F's call (was 4F3BA0: the ObjDesc in EDI, the argument on the stack; ebx / esi / edi / ebp kept, as cdecl)
static __declspec(naked) void hkPpAlloc(void)
{
    __asm {
        push dword ptr [esp + 4]    // arg
        push edi                    // def
        call PpAllocC
        add esp, 8
        ret
    }
}

static const char *PpCallerName(DWORD ret)
{
    switch (ret) {
    case 0x463211: return g_ppPreferHigh ? "Supply Drops (through 00463150)" : "an enemy drop (00463150, a Gum Head's)";
    case 0x4632CC: return "an enemy drop (00463240, a Gum Head's)";
    case 0x46344E: return "a Gum Head's stolen item put back (00463340)";
    case 0x4F4284: return "a pinned ghost's Sword (004F4240)";
    }
    return "an unknown caller";
}
// the guard fired (hkPpGuard; game thread): the pool was full, the pickup is not made
static void __cdecl PpGuardFired(DWORD caller, int item)
{
    const LONG n = InterlockedIncrement(&g_ppGuardN);
    if (LogQuota(&g_ppQuotaGuard, 20))
        LogAnomaly("pickup pool", "the pool was full (all %d records in use): %s (return %08lX) asked for item %d; no pickup "
                   "was made (the stock game crashes here at 004F4127 / 004F42A3); %ld time(s) this session", g_ppRecN,
                   PpCallerName(caller), caller, item, n);
}

// 0x4F3F74 (was `mov [13D8210],eax`; next `mov eax,[FD5A94]` at 0x4F3F79). Stack here: [esp] the alloc's argument
// (push ebx, 0x4F3F6A), [esp+4] the saved edi (0x4F3F69), [esp+0A0h] 0x4F3E90's return, [esp+0B4h] its item argument.
static __declspec(naked) void hkPpGuard(void)
{
    __asm {
        mov dword ptr ds:[13D8210h], eax   // D_013D8210 = the record (NULL too: the stock failure path clears it)
        test eax, eax
        jz pp_full
        jmp dword ptr [g_ppBack]           // 4F3F79: the stock path
    pp_full:
        pushad
        cld
        push dword ptr [esp + 20h + 0B4h]  // item
        push dword ptr [esp + 24h + 0A0h]  // 0x4F3E90's caller
        call PpGuardFired
        add esp, 8
        popad
        add esp, 4                         // the alloc's argument
        pop edi                            // as 0x4F3F9E
        jmp dword ptr [g_ppFail]           // 4F3FB2: xor eax,eax / pop ebx / add esp,94h / ret: NULL, no object
    }
}

// 0x4FC1CD (SaveSnapshotBuild 0x4FC0E0: the image's records at +0x36C0, 0x1000 bytes). Copy only: the live pool stays.
static void __cdecl PpSave(void *dst)
{
    static PpView v[PP_REC_MAX];  // game thread only
    int plan[PP_STOCK_N], act = 0, kept, game, swords, ours, freeN;
    PpViews(v, g_ppBuf, g_ppRecN);
    kept = PpSavePlan(v, g_ppRecN, plan, &act);
    PpSaveApply((BYTE *)dst, g_ppBuf, plan);
    if (act <= kept) return;
    PpUsage(v, g_ppRecN, &game, &swords, &ours, &freeN);
    if (game > PP_STOCK_N && LogQuota(&g_ppQuotaSave, 10))
        LogAnomaly("pickup pool", "the game itself holds %d pickup records (%d pin Swords): a save keeps %d; %d of the game's and "
                   "all %d Supply Drops are not saved", game, swords, PP_STOCK_N, game - PP_STOCK_N, ours);
    else if (LogQuota(&g_ppQuotaSave, 40))
        LogV("pickup pool: a snapshot keeps %d of the %d records in use (%d the game's, %d Supply Drops): the %d oldest Supply "
             "Drops are not in it", kept, act, game, ours, act - kept);
}

// 0x4FC6B9 (SaveSnapshotApply 0x4FC5B0: a Load, a Continue, Refined's quick load). The world start rebuilds the room's
// objects from the records (RoomInitLoad -> 0x4F40B0).
static void __cdecl PpLoad(const void *src)
{
    int i, extra = 0;
    for (i = PP_STOCK_N; i < g_ppRecN; ++i) extra += g_ppBuf[i * PP_REC_SIZE] != 0;
    PpLoadApply(g_ppBuf, g_ppRecN, (const BYTE *)src);
    if (LogQuota(&g_ppQuotaLoad, 60)) {
        int in = 0;
        for (i = 0; i < PP_STOCK_N; ++i) in += g_ppBuf[i * PP_REC_SIZE] != 0;
        LogV("pickup pool: a snapshot applied: %d record(s) in; slots 32..%d cleared (%d were in use)", in, g_ppRecN - 1, extra);
    }
}

// QsTick (0x4EF982, game thread): one verbose line per room left, only when the pool's use changed since the last line
static void PpTick(void)
{
    static PpView v[PP_REC_MAX];
    DWORD key;
    int game, swords, ours, freeN;
    if (!g_ppGuardOn || g_ppTickOff) return;
    key = (DWORD)*(volatile int *)0xFD5A90 << 16 | (DWORD)(*(volatile int *)0xFD5A94 & 0xFFFF);
    if (key == g_ppRoomKey) return;
    __try {
        PpViews(v, (const BYTE *)(ULONG_PTR)g_ppRecVA, g_ppRecN);
        PpUsage(v, g_ppRecN, &game, &swords, &ours, &freeN);
    } __except (PpFaultNote(GetExceptionInformation(), "the room count")) {
        g_ppTickOff = 1;  // (the patches stay; only this log stops)
        return;
    }
    if (g_ppRoomKey != 0xFFFFFFFFu && (game != g_ppLastUse[0] || swords != g_ppLastUse[1] || ours != g_ppLastUse[2]) &&
        LogQuota(&g_ppQuotaRoom, 400))
        LogV("pickup pool: room %d:%d left: %d game record(s) (%d pin Sword(s)), %d Supply Drop(s), %d free of %d (%s; most in "
             "use this session %d; full-pool refusals %ld)", (int)(g_ppRoomKey >> 16), (int)(g_ppRoomKey & 0xFFFF), game, swords,
             ours, freeN, g_ppRecN, g_ppMoved ? "moved" : "the stock pool", g_ppPeak, g_ppGuardN);
    if (g_ppRoomKey != 0xFFFFFFFFu) g_ppLastUse[0] = game, g_ppLastUse[1] = swords, g_ppLastUse[2] = ours;
    g_ppRoomKey = key;
}

typedef struct { DWORD va, stock, val; } PpDword;
static int PpPoke4(DWORD va, DWORD v) { return Poke(va, &v, 4); }

static void PickPoolInstall(void)
{
    static const Site kGuard[] = {
        // push edi / push ebx / lea edi,[esp+0Ch] / call 4F3BA0 / mov [13D8210],eax / mov eax,[FD5A94]
        { 0x4F3F69, { 0x57, 0x53, 0x8D, 0x7C, 0x24, 0x0C, 0xE8, 0x2C, 0xFC, 0xFF, 0xFF, 0xA3, 0x10, 0x82, 0x3D, 0x01, 0xA1, 0x94,
                      0x5A, 0xFD, 0x00 }, 21 },
        // pop edi / jne 4F3FBC / the object-failure clear / 4F3FB2: xor eax,eax / pop ebx / add esp,94h / ret
        { 0x4F3F9E, { 0x5F, 0x75, 0x1B, 0xA1, 0x10, 0x82, 0x3D, 0x01, 0x88, 0x18, 0x66, 0x89, 0x58, 0x02, 0x89, 0x1D, 0x10, 0x82,
                      0x3D, 0x01, 0x33, 0xC0, 0x5B, 0x81, 0xC4, 0x94, 0x00, 0x00, 0x00, 0xC3 }, 30 },
        // add esp,18h / test eax,eax / je 4F42A3 / mov edx,[ebp+0Ch] / mov [eax+70h],edx / mov byte [eax+1],1 / xor eax,eax..
        { 0x4F4296, { 0x83, 0xC4, 0x18, 0x85, 0xC0, 0x74, 0x06, 0x8B, 0x55, 0x0C, 0x89, 0x50, 0x70, 0xC6, 0x40, 0x01, 0x01, 0x33,
                      0xC0, 0x85, 0xF6, 0x0F, 0x95, 0xC0 }, 24 },
    };
    static const Site kMove[] = {
        { 0x4F4017, { 0xB9, 0x28, 0x76, 0x07, 0x01 }, 5 },                  // mov ecx,1077628h (FindNear)
        { 0x4F4099, { 0x81, 0xF9, 0x28, 0x86, 0x07, 0x01 }, 6 },            // cmp ecx,1078628h
        { 0x4F40B3, { 0xBF, 0x16, 0x76, 0x07, 0x01 }, 5 },                  // mov edi,1077616h (SpawnRoom)
        { 0x4F4110, { 0x81, 0xFF, 0x16, 0x86, 0x07, 0x01 }, 6 },            // cmp edi,1078616h
        { 0x4F4131, { 0xBE, 0x10, 0x76, 0x07, 0x01 }, 5 },                  // mov esi,1077610h (room exit)
        { 0x4F415A, { 0x81, 0xFE, 0x10, 0x86, 0x07, 0x01 }, 6 },            // cmp esi,1078610h
        { 0x4F419C, { 0xBE, 0x16, 0x76, 0x07, 0x01 }, 5 },                  // mov esi,1077616h (pins' room move)
        { 0x4F41F0, { 0x81, 0xFE, 0x16, 0x86, 0x07, 0x01 }, 6 },            // cmp esi,1078616h
        // PickupInit: push 1450h / push 1078618h / call 42BB90 / push 1000h / push 1077610h / call 42BB90
        { 0x4F42C0, { 0x68, 0x50, 0x14, 0x00, 0x00, 0x68, 0x18, 0x86, 0x07, 0x01, 0xE8, 0xC1, 0x78, 0xF3, 0xFF, 0x68, 0x00, 0x10,
                      0x00, 0x00, 0x68, 0x10, 0x76, 0x07, 0x01, 0xE8, 0xB2, 0x78, 0xF3, 0xFF }, 30 },
        // push 1080430h / mov [107CDAC],36C0h / call 4F3D50
        { 0x4FC1BD, { 0x68, 0x30, 0x04, 0x08, 0x01, 0xC7, 0x05, 0xAC, 0xCD, 0x07, 0x01, 0xC0, 0x36, 0x00, 0x00, 0xE8, 0x7F, 0x7B,
                      0xFF, 0xFF }, 20 },
        { 0x4FC6B1, { 0x8D, 0x88, 0x70, 0xCD, 0x07, 0x01, 0x51, 0xE8, 0x73, 0x76, 0xFF, 0xFF }, 12 },  // lea ecx,[eax+107CD70h] / push / call 4F3D30
        { 0x4F3BA0, { 0x55, 0x8B, 0xEC, 0x56, 0x33, 0xF6, 0xB8, 0x10, 0x76, 0x07, 0x01, 0xEB }, 12 },  // PickupRecordAlloc (replaced)
        { 0x4F3D30, { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x08, 0x57, 0xB9, 0x00, 0x04, 0x00, 0x00, 0xBF, 0x10, 0x76 }, 16 },  // Load
        { 0x4F3D50, { 0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B, 0x7D, 0x08, 0xB9, 0x00, 0x04, 0x00, 0x00, 0xBE, 0x10, 0x76 }, 16 },  // Save
        { 0x421B30, { 0xD9, 0x44, 0x24, 0x0C, 0x83, 0xEC, 0x08, 0xD8, 0x0D, 0xE0, 0x79, 0x5B }, 12 },  // MathQuatFromAxisAngle (called)
    };
    static const BYTE kGuardStock[5] = { 0xA3, 0x10, 0x82, 0x3D, 0x01 };
    PpDword w[13];
    const int ini = GetPrivateProfileIntA("Gameplay", "PickupPool", PP_DEFAULT_N, g_iniPath);
    const int n = PpPoolN(ini);
    const unsigned B = (unsigned)(ULONG_PTR)g_ppBuf, E = B + (unsigned)n * PP_REC_SIZE;
    int i, moveOk = n > PP_STOCK_N;
    BYTE j0A = 0x0A;
    for (i = 0; i < (int)(sizeof kGuard / sizeof kGuard[0]); ++i)
        if (!SiteOk(&kGuard[i])) {
            Log("pickup pool: %08lX is not stock (another mod?): no full-pool guard and no larger pool (the game's 32 records; "
                "a full pool crashes as in the stock game)", kGuard[i].va);
            return;
        }
    if (moveOk)
        for (i = 0; i < (int)(sizeof kMove / sizeof kMove[0]); ++i)
            if (!SiteOk(&kMove[i])) {
                Log("pickup pool: %08lX is not stock (another mod?): the pool stays the game's 32 records (the guard goes in)",
                    kMove[i].va);
                moveOk = 0;
                break;
            }
    if (moveOk && !PpBufferOk(B, n)) {
        Log("pickup pool: the buffer %08X (%d records) fails the stock code's address rules (16-aligned, the 4FFFFFFFh NULL "
            "mask, no signed wrap): the pool stays the game's 32 records", B, n);
        moveOk = 0;
    }
    // stage 1: the guard (two writes; the second's failure takes the first back)
    if (!PokeBranch(0x4F3F74, 0xE9, hkPpGuard, 5)) {
        Log("pickup pool: 004F3F74 could not be written: no guard, no larger pool");
        return;
    }
    if (!Poke(0x4F429C, &j0A, 1)) {
        if (!Poke(0x4F3F74, kGuardStock, 5) && LogQuota(&g_ppQuotaFault, 5))
            LogAnomaly("pickup pool", "004F3F74 could not be put back after 004F429C failed: the guard's jump stays (it is "
                       "complete on its own for 0x4F3E90's callers)");
        Log("pickup pool: 004F429C could not be written: no guard, no larger pool");
        return;
    }
    g_ppGuardOn = 1;
    // stage 2: the move (13 dword writes, all or nothing)
    if (moveOk) {
        int k;
        w[0].va = 0x4F3F70, w[0].stock = 0xFFFFFC2Cu, w[0].val = (DWORD)(ULONG_PTR)hkPpAlloc - 0x4F3F74u;
        w[1].va = 0x4F4018, w[1].stock = 0x01077628u, w[1].val = B + 0x18u;
        w[2].va = 0x4F409B, w[2].stock = 0x01078628u, w[2].val = E + 0x18u;
        w[3].va = 0x4F40B4, w[3].stock = 0x01077616u, w[3].val = B + 6u;
        w[4].va = 0x4F4112, w[4].stock = 0x01078616u, w[4].val = E + 6u;
        w[5].va = 0x4F4132, w[5].stock = 0x01077610u, w[5].val = B;
        w[6].va = 0x4F415C, w[6].stock = 0x01078610u, w[6].val = E;
        w[7].va = 0x4F419D, w[7].stock = 0x01077616u, w[7].val = B + 6u;
        w[8].va = 0x4F41F2, w[8].stock = 0x01078616u, w[8].val = E + 6u;
        w[9].va = 0x4F42D0, w[9].stock = 0x00001000u, w[9].val = (DWORD)n * PP_REC_SIZE;
        w[10].va = 0x4F42D5, w[10].stock = 0x01077610u, w[10].val = B;
        w[11].va = 0x4FC1CD, w[11].stock = 0xFFFF7B7Fu, w[11].val = (DWORD)(ULONG_PTR)PpSave - 0x4FC1D1u;
        w[12].va = 0x4FC6B9, w[12].stock = 0xFFFF7673u, w[12].val = (DWORD)(ULONG_PTR)PpLoad - 0x4FC6BDu;
        for (i = 0; i < 13 && *(const DWORD *)(ULONG_PTR)w[i].va == w[i].stock; ++i) {}
        if (i < 13) {  // (the site checks above cover every one; a second look costs nothing)
            Log("pickup pool: %08lX is not %08lX: the pool stays the game's 32 records", w[i].va, w[i].stock);
            moveOk = 0;
        }
        for (i = 0; moveOk && i < 13; ++i)
            if (!PpPoke4(w[i].va, w[i].val)) break;
        if (moveOk && i < 13) {
            int bad = 0;
            for (k = i - 1; k >= 0; --k) bad += !PpPoke4(w[k].va, w[k].stock);
            Log("pickup pool: %08lX could not be written: the %d write(s) before it were taken back%s; the pool stays the game's "
                "32 records", w[i].va, i, bad ? " (some failed)" : "");
            if (bad && LogQuota(&g_ppQuotaFault, 5))
                LogAnomaly("pickup pool", "%d of the move's writes could not be taken back: the pool's code is mixed", bad);
            moveOk = 0;
        }
    }
    if (moveOk) {
        memset(g_ppBuf, 0, sizeof g_ppBuf);  // (static: zero already; PickupInit zeroes it again with the new size)
        g_ppRecVA = B;
        g_ppRecN = n;
        g_ppMoved = 1;
        Log("pickup pool: %d records at %08X (%u KB in this dll; was 32 at 01077610) and the full-pool guard: no pickup instead "
            "of a crash when all are in use. Saves keep their 32 records (pin Swords, the game's own, then Supply Drops newest "
            "first)", n, B, (unsigned)(n * PP_REC_SIZE / 1024));
        LogV("pickup pool: sites 004F3F74 (jmp, the guard) 004F429C (06 -> 0A) 004F3F70 (call PpAlloc) 004F4018 004F409B 004F40B4 "
             "004F4112 004F4132 004F415C 004F419D 004F41F2 (loop bounds) 004F42D0 004F42D5 (PickupInit %08lX bytes at %08X) "
             "004FC1CD (call PpSave) 004FC6B9 (call PpLoad); every slot passes the 4FFFFFFFh mask; [Gameplay] PickupPool=%d",
             w[9].val, B, ini);
    } else {
        Log("pickup pool: the game's 32 records at 01077610%s, with the full-pool guard (004F3F74 jmp, 004F429C 06 -> 0A): no "
            "pickup instead of a crash when all are in use", n > PP_STOCK_N ? "" : " ([Gameplay] PickupPool=0)");
    }
}
