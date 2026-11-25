// v3.36 TP336: the pure part of the larger task pool and IK pool (pools.inc; native test test\pools_test.c). No game
// memory is read here: addresses are numbers, the IK free list is walked through the pointers it is given (the test
// builds its own). Included near the top of sh4windowmod.c (every Refined walker of the task pool uses TaskSlotOk /
// TaskSlotAt / g_tpN; pools.inc sets g_tpN once at Init, before the game's own init, and never again).
//
// GOG v7 (SH4Decomp src/system/task_553820.c, sys_553ad0.c, src/anim/sg_anime_pool.c; re_kit/sh4.asm):
//   gTasks 0x11B6020, 300 slots of 0x430 (+0x404 update, 0 = free), ends at 0x1204860 where gSfOtPacketBuffer starts
//   (0x30000 bytes, its top pointer gSfOtPacketTop at 0x1234860 = the buffer's end). The pool grows in place: the
//   packet buffer starts N - 300 slots later and keeps its end, so it shrinks by (N - 300) * 0x430.
//   IK handles gSgAnimeWorkIks 0xED73F4 (free head, num 0xED73F8, num_free 0xED73FC (never counted down), base
//   0xED7400), 0x50 bytes each, next link at +0x40; 160 stock (`push 0A0h` at 0x414A29 world reset, 0x414E2F boot).
#ifndef POOLS_SIM_H
#define POOLS_SIM_H

#define TP_BASE 0x011B6020u       // gTasks
#define TP_REC 0x430u             // one task slot
#define TP_STOCK_N 300
#define TP_DEFAULT_N 400          // [Gameplay] TaskPool (hidden) when the key is missing
#define TP_MAX_N 448              // the packet buffer keeps 0x93C0 bytes (>= 0x8000; its peak on PC is under 1 KB)
#define TP_PK_END 0x01234860u     // the packet buffer's end = gSfOtPacketTop (stays)
#define TP_PK_GET_SLACK 0x1000u   // sfOtPacketBufferGet refuses once fewer than this many bytes are left (stock 0x2F000)
#define TP_MARGIN 16              // fewer free task slots than this: an ANOMALY line (a full pool crashes the game)

#define IK_REC 0x50u              // SgIkHandle
#define IK_NEXT 0x40u             // its free-list link
#define IK_STOCK_N 160
#define IK_DEFAULT_N 512          // [Gameplay] IkPool (hidden) when the key is missing
#define IK_MAX_N 2048

// the ini value -> the slots in use: 0 .. 300 = the stock pool, 301 .. 448 as given, above = 448
static int TpPoolN(int ini) { return ini <= TP_STOCK_N ? TP_STOCK_N : ini > TP_MAX_N ? TP_MAX_N : ini; }
static int IkPoolN(int ini) { return ini <= IK_STOCK_N ? IK_STOCK_N : ini > IK_MAX_N ? IK_MAX_N : ini; }

// the pool's end = the packet buffer's new start (16-aligned for every n: 0x11B6020 and 0x430 are), and its new size
static unsigned TpEnd(int n) { return TP_BASE + (unsigned)n * TP_REC; }
static unsigned TpPacketSize(int n) { return TP_PK_END - TpEnd(n); }

// The ten imm32 the growth writes (all or nothing), with their stock values (re_kit/sh4.asm, checked 2026-11-20):
//   0x553846 InitAll `cmp esi,1204860h` (end), 0x55385F RunGroup `mov edi,12Ch`, 0x553A40 FindByFunc `cmp ecx,12Ch`,
//   0x553A72 Alloc `cmp eax,12Ch`; the packet buffer: 0x553BC9 Get `sub eax,1204860h`, 0x553BCE Get `cmp eax,2F000h`,
//   0x553C04 Fix `sub edx,1204860h`, 0x553C0A Fix `cmp edx,30000h`, 0x553C5B / 0x553C88 the two resets
//   `mov [1234860h],1204860h`. The pool's base (0x553822, 0x55385A = base + 0x404, 0x553A27, 0x553A51) and the top
//   pointer 0x1234860 stay.
typedef struct { unsigned va, stock, val; } TpDword;
#define TP_NSITES 10
static void TpPlan(int n, TpDword *w)
{
    static const unsigned kVa[TP_NSITES] = { 0x553846, 0x55385F, 0x553A40, 0x553A72, 0x553BC9, 0x553BCE, 0x553C04, 0x553C0A,
                                             0x553C5B, 0x553C88 };
    static const unsigned kStock[TP_NSITES] = { 0x01204860, 0x12C, 0x12C, 0x12C, 0x01204860, 0x2F000, 0x01204860, 0x30000,
                                                0x01204860, 0x01204860 };
    const unsigned e = TpEnd(n), s = TpPacketSize(n);
    const unsigned val[TP_NSITES] = { e, (unsigned)n, (unsigned)n, (unsigned)n, e, s - TP_PK_GET_SLACK, e, s, e, e };
    int i;
    for (i = 0; i < TP_NSITES; ++i) w[i].va = kVa[i], w[i].stock = kStock[i], w[i].val = val[i];
}

// ---------------------------------------------------------------- the range helper (every Refined walker)
static int g_tpN = TP_STOCK_N;  // the task slots in use: 300, or [Gameplay] TaskPool once pools.inc grew the pool

// p is the start of one of the n slots
static int TpIsSlot(unsigned p, int n) { const unsigned ofs = p - TP_BASE; return ofs < (unsigned)n * TP_REC && ofs % TP_REC == 0; }
static int TaskSlotOk(const void *p) { return TpIsSlot((unsigned)(size_t)p, g_tpN); }
static unsigned char *TaskSlotAt(int i) { return (unsigned char *)(size_t)(TP_BASE + (unsigned)i * TP_REC); }

// Refined's copy / follower gate keeps its v3.35 meaning: copies stop while fewer than `keep` of the STOCK 300 would be
// free, so the grown part stays a reserve for the game's own tasks (effects, flames, events; 56 of 94 callers of the
// alloc crash on a full pool) and Playable Eileen's 300-slot animation watchdog never meets her nodes in it because
// of Refined. free counts all n slots.
static int TpGateFree(int freeN, int n, int keep) { return freeN >= keep + (n > TP_STOCK_N ? n - TP_STOCK_N : 0); }
// v3.36.1 TL3361 (the user: "lift it now"): with every other walker of the pool reading the live count, Enemy Copies /
// Follow may use the whole pool: keep `keep` free of all n slots (else of the first 300, TpGateFree)
static int TpGateFreeAll(int freeN, int n, int keep, int all) { return all ? freeN >= keep : TpGateFree(freeN, n, keep); }
// v3.36.1 TL3361: Playable Eileen reads the live slot count (0x553A72) from v1.20.1 (its KB section 179): the file
// version's ms / ls dwords (major.minor, build.revision)
static int TpPeReadsCount(unsigned long ms, unsigned long ls)
{
    return ms > 0x00010014ul || (ms == 0x00010014ul && ls >= 0x00010000ul);
}

// ---------------------------------------------------------------- the IK free list
// Free handles from head through +0x40, -1 when a link leaves [base, base + num * 0x50) or is not on a record start, or
// the list is longer than num (a cycle). head 0 = none free.
static int IkCountFree(const unsigned char *head, const unsigned char *base, int num)
{
    int n = 0;
    const unsigned char *p = head;
    if (num < 0 || num > 0x100000) return -1;
    while (p) {
        const size_t ofs = (size_t)(p - base);
        if (p < base || ofs >= (size_t)num * IK_REC || ofs % IK_REC || ++n > num) return -1;
        p = *(const unsigned char *const *)(p + IK_NEXT);
    }
    return n;
}

// ---------------------------------------------------------------- per-room use (the tick samples, a room line reports)
typedef struct {
    int samples;
    int tpUsedMax, tpFreeMin, tpHighMax;  // tasks: most in use, fewest free, most in the grown part (slots 300+)
    int ikUsedMax, ikFreeMin, ikBad, ikNum;  // IK: most in use, fewest free, unreadable lists, the pool's size
    int actFreeMin;                       // actor records: fewest free
    int heapMax;                          // system heap units used (of 0x10000)
} TpRoom;
static void TpRoomClear(TpRoom *r)
{
    r->samples = 0;
    r->tpUsedMax = r->tpHighMax = r->ikUsedMax = r->heapMax = 0;
    r->tpFreeMin = r->ikFreeMin = r->actFreeMin = 0x7FFFFFFF;
    r->ikBad = r->ikNum = 0;
}
// one sample: tpUsed / tpHigh over the n task slots, ikFree of ikNum (-1 = the list could not be read), actFree,
// heap units
static void TpRoomAdd(TpRoom *r, int n, int tpUsed, int tpHigh, int ikNum, int ikFree, int actFree, int heap)
{
    ++r->samples;
    if (tpUsed > r->tpUsedMax) r->tpUsedMax = tpUsed;
    if (n - tpUsed < r->tpFreeMin) r->tpFreeMin = n - tpUsed;
    if (tpHigh > r->tpHighMax) r->tpHighMax = tpHigh;
    if (ikFree < 0) ++r->ikBad;
    else {
        r->ikNum = ikNum;
        if (ikNum - ikFree > r->ikUsedMax) r->ikUsedMax = ikNum - ikFree;
        if (ikFree < r->ikFreeMin) r->ikFreeMin = ikFree;
    }
    if (actFree < r->actFreeMin) r->actFreeMin = actFree;
    if (heap > r->heapMax) r->heapMax = heap;
}

#endif
