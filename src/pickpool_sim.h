// v3.35 PK335: the pure part of the larger dynamic pickup pool (pickpool.inc; native test test\pickpool_test.c).
// No game memory: record buffers and views are passed in. GOG v7 layout of a record (SH4Decomp include/pickup.h,
// PickupRecord, 0x80 bytes): +0 state (0 free, 1 lying, 2 set by 0x4F4120), +1 unk_01 (1 = a pinned ghost's Sword,
// 0x4F4240), +2 active (short), +4 unk_04 (short, the alloc's argument), +6 stage, +8 room, +0x0A type (shorts),
// +0x10 pos (Vec4A), +0x20 rot (quaternion), +0x30 model, +0x34 texture, +0x38 item, +0x3C difficulty (short),
// +0x40 pos2, +0x50 zoneShape, +0x54 zoneFlags, +0x58 zoneHeading, +0x60 zoneSize[3], +0x70 unk_70[0] (the pin's
// owner id), +0x74..+0x7F free (Supply Drops' marker, drops.inc DropRecOurs).
#ifndef PICKPOOL_SIM_H
#define PICKPOOL_SIM_H
#include <string.h>

#define PP_REC_SIZE 0x80
#define PP_STOCK_N 32       // the game's pool (0x1077610) and its save image (+0x36C0, 0x1000 bytes)
#define PP_REC_MAX 128      // the largest pool (16 KB)
#define PP_DEFAULT_N 128    // [Gameplay] PickupPool (hidden) when the key is missing
#define PP_NULL_MASK 0x4FFFFFFFu  // EventObjectIsEnabled 0x4CF252 `test eax,4FFFFFFFh` (a PS2 NULL test of the record)

// the ini value -> the records in use: 0 .. 32 = the stock pool (no move), 33 .. 128 as given, above = 128
static int PpPoolN(int ini) { return ini <= PP_STOCK_N ? PP_STOCK_N : ini > PP_REC_MAX ? PP_REC_MAX : ini; }

// The record the alloc takes, -1 = the pool is full. The game's callers: the first free of 0 .. n-1 (as 0x4F3BA0, so
// the game's own records keep to the 32 a save holds); preferHigh (Refined's Supply Drops around its 0x463150 call):
// the first free of 32 .. n-1, then of 0 .. 31.
static int PpAllocPick(const unsigned char *recs, int n, int preferHigh)
{
    int i;
    if (preferHigh && n > PP_STOCK_N) {
        for (i = PP_STOCK_N; i < n; ++i)
            if (recs[i * PP_REC_SIZE] == 0) return i;
        n = PP_STOCK_N;
    }
    for (i = 0; i < n; ++i)
        if (recs[i * PP_REC_SIZE] == 0) return i;
    return -1;
}

// PickupRecordAlloc 0x4F3BA0's record from its ObjDesc (def; the alloc's EDI) as the GOG v7 bytes write it, the whole
// record zeroed first (the stock alloc leaves +0x01, +0x0C, +0x1C, +0x5C, +0x6C, +0x70.. of the slot's last user: a stale
// unk_01 = 1 with an old owner id lets 0x4F4190 carry an unrelated pickup along with a pin). The rotation (+0x20) is the
// caller's: 0x421B30(rec + 0x20, 0x6112D0, yaw = def + 0x20). stage / room = the low words of 0xFD5A90 / 0xFD5A94.
static void PpRecFill(unsigned char *r, const unsigned char *def, int arg, int stage, int room)
{
    const short one = 1, a = (short)arg, st = (short)stage, rm = (short)room;
    int shape, flags;
    memset(r, 0, PP_REC_SIZE);
    r[0] = 1;                            // state
    memcpy(r + 0x02, &one, 2);           // active
    memcpy(r + 0x04, &a, 2);             // unk_04 = the argument (0 from 0x4F3F6A)
    memcpy(r + 0x06, &st, 2);
    memcpy(r + 0x08, &rm, 2);
    memcpy(r + 0x0A, def + 0x00, 2);     // type = def->kind
    memcpy(r + 0x10, def + 0x14, 12);    // pos = def->info.pos
    memcpy(r + 0x30, def + 0x24, 4);     // model
    memcpy(r + 0x34, def + 0x28, 4);     // texture
    memcpy(r + 0x38, def + 0x2C, 4);     // item
    memcpy(r + 0x3C, def + 0x30, 2);     // difficulty
    memcpy(r + 0x40, r + 0x10, 12);      // pos2 = pos
    memcpy(r + 0x50, def + 0x38, 4);     // zoneShape
    memcpy(r + 0x54, def + 0x4C, 4);     // zoneFlags
    memcpy(r + 0x58, def + 0x48, 4);     // zoneHeading
    memcpy(&shape, def + 0x38, 4);
    memcpy(&flags, def + 0x4C, 4);
    if (!(flags & 8)) {
        if (shape == 1) memcpy(r + 0x60, def + 0x3C, 8);
        else if (shape == 2) memcpy(r + 0x60, def + 0x3C, 12);
        else if (shape == 3) memcpy(r + 0x60, def + 0x3C, 4);
    }
}

// A record as the save sees it. sword = unk_01 != 0 (a pinned ghost's Sword, 0x4F4240); ours = Supply Drops' marker
// (drops.inc DropRecOurs); seq = its 12-bit drop number (marker word & 0xFFF, higher = newer).
typedef struct { int state, sword, ours, seq; } PpView;
enum { PPC_SWORD, PPC_GAME, PPC_OURS, PPC_FREE };
static int PpClass(const PpView *v) { return !v->state ? PPC_FREE : v->ours ? PPC_OURS : v->sword ? PPC_SWORD : PPC_GAME; }

// The save image's 32 records (PickupRecordsSave 0x4F3D50 is replaced by this plan): out[k] = the record copied to the
// image's slot k, -1 = a zero slot. Priority: the game's pin Swords, the game's other records (both by slot), Supply
// Drops' records newest first; 32 at most. A chosen record of slots 0 .. 31 keeps its slot (with the pool's first 32
// slots holding everything, the image holds the same records in the same slots as the stock save); the others fill the
// lowest unused slots in priority order. Returns the records kept; *active = the records in use (kept + left out).
static int PpSavePlan(const PpView *v, int n, int *out, int *active)
{
    int chosen[PP_REC_MAX], order[PP_REC_MAX];
    int i, k, c, kept = 0, act = 0;
    if (n > PP_REC_MAX) n = PP_REC_MAX;
    for (k = 0; k < PP_STOCK_N; ++k) out[k] = -1;
    for (i = 0; i < n; ++i) {
        chosen[i] = 0;
        act += v[i].state != 0;
    }
    for (c = PPC_SWORD; c <= PPC_GAME; ++c)
        for (i = 0; i < n && kept < PP_STOCK_N; ++i)
            if (PpClass(&v[i]) == c) chosen[i] = 1, order[kept++] = i;
    while (kept < PP_STOCK_N) {  // ours, newest first (ties: the lower slot)
        int best = -1;
        for (i = 0; i < n; ++i)
            if (!chosen[i] && PpClass(&v[i]) == PPC_OURS && (best < 0 || v[i].seq > v[best].seq)) best = i;
        if (best < 0) break;
        chosen[best] = 1, order[kept++] = best;
    }
    for (k = 0; k < kept; ++k)
        if (order[k] < PP_STOCK_N) out[order[k]] = order[k];
    for (k = 0, c = 0; k < kept; ++k) {
        if (order[k] < PP_STOCK_N) continue;
        while (c < PP_STOCK_N && out[c] >= 0) ++c;
        if (c >= PP_STOCK_N) break;  // (cannot happen: kept <= 32)
        out[c] = order[k];
    }
    if (active) *active = act;
    return kept;
}
static void PpSaveApply(unsigned char *dst, const unsigned char *recs, const int *plan)
{
    int k;
    for (k = 0; k < PP_STOCK_N; ++k)
        if (plan[k] >= 0) memcpy(dst + k * PP_REC_SIZE, recs + plan[k] * PP_REC_SIZE, PP_REC_SIZE);
        else memset(dst + k * PP_REC_SIZE, 0, PP_REC_SIZE);
}
// PickupRecordsLoad 0x4F3D30 replaced: the image's 32 records into slots 0 .. 31, slots 32 .. n-1 cleared (else the
// records of before the load would come back next to the loaded ones)
static void PpLoadApply(unsigned char *recs, int n, const unsigned char *src)
{
    memcpy(recs, src, PP_STOCK_N * PP_REC_SIZE);
    if (n > PP_STOCK_N) memset(recs + PP_STOCK_N * PP_REC_SIZE, 0, (size_t)(n - PP_STOCK_N) * PP_REC_SIZE);
}

// the pool's use: the game's records (*swords of them pin Swords), Supply Drops', free
static void PpUsage(const PpView *v, int n, int *game, int *swords, int *ours, int *freeN)
{
    int i;
    *game = *swords = *ours = *freeN = 0;
    for (i = 0; i < n; ++i) switch (PpClass(&v[i])) {
        case PPC_SWORD: ++*swords; ++*game; break;
        case PPC_GAME: ++*game; break;
        case PPC_OURS: ++*ours; break;
        default: ++*freeN; break;
    }
}

// 1 = a buffer at base for n records works with the stock code: 16-aligned (the records' Vec4A), every record passes
// the NULL mask (0x4CF252), and the moved loops' signed `jl` ends (base .. end + 0x18) do not cross 0x80000000
static int PpBufferOk(unsigned base, int n)
{
    const unsigned long long end = (unsigned long long)base + (unsigned long long)n * PP_REC_SIZE + 0x18u;
    int i;
    if (base & 15u || n < 1 || end > 0xFFFFFFFFull) return 0;
    if ((base ^ (unsigned)end) & 0x80000000u) return 0;
    for (i = 0; i < n; ++i)
        if (!((base + (unsigned)i * PP_REC_SIZE) & PP_NULL_MASK)) return 0;
    return 1;
}
#endif
