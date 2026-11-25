// ================================================================ v3.36 HE336: copies' records are evicted first
// The user (2026-11-20, v3.36 round): the pool audit's rank 3 (staging/pool_audit/REPORT.md section 3, notes_enemies.md
// 3a). Plan: staging/v336_feedback/w2_hits_evict/PLAN.md. Included by sh4windowmod.c after hitpool.inc (enemies.inc is
// before it: EnmIsCopyId, EnmCopyRecordGone); RecEvictInstall runs at Init; EvTick from quicksave.inc's 0x4EF982
// dispatcher; EvAtPurge / EvAtPurgeEnd from enemies.inc's EnmPurgeCopyRecords.
//
// The enemy records rec[64] (gEnemyManager +0x90 = 0xFCF090, 0x50 each; saved in the 0x1B90-byte manager block) hold
// the enemies left alive or downed in rooms the player left. A room's destroys at its exit and EnemyManagerSave write
// them through EnemyRecordAlloc 0x464060, which never refuses: on a full table it evicts the record of the lowest class
// (1 / 2 / 6 another stage, 3 / 5 / 7 / 8 / 9 / 11 another room, 4 / 10 / 12 this room, 13 any ghost; the oldest inside
// a class). An evicted downed monster (6 / 11 / 12, 8 downed) gets its kill bit: dead for good; an evicted standing one
// comes back at full health; that is in the next save. Copies' records (ids 0x1E00nnnn: no placement id, so classes
// 3 / 4, a ghost copy's 13) join the table at every exit until enemies.inc purges them at the next tick or room load: on
// a full table they evicted real records of other stages (1 / 2 / 6) and, for ghost copies (13, the last class), of
// every class.
//
// The change ([Gameplay] CopyEvictFirst=1, hidden; 0 = the game's own function): 0x464060's entry (6 bytes `sub
// esp,80h`) -> jmp EvAllocC, the matched C (en_463950.c 384-478, recevict_sim.h EvChoose / EvEvict, tested against a
// transcription of it) with one change: when no slot is free, a copy's record (the oldest) is taken before any class.
// Real records go only when no copy's record is left, in the stock order. Why a copy's record may go: (1) the purge
// deletes it anyway at the next tick or room load (copies are made again from their placements; enemies.inc); (2) what
// Refined keeps of it is kept first, by the purge's own code (EnmCopyRecordGone: a ghost copy's state into the
// ghost-copy store, a pursuing copy's record for the door, the copies' ledger), so taking it is the purge a few frames
// early; (3) copies are never in a save (0x465438) and no copy has a kill index, so its eviction sets no kill bit and
// changes no save. The save format is untouched (the same 64 records in the same block).
// What is left: a copy's own write on a full table with no copy's record in it still takes a real record (the game
// cannot tell the writer at the alloc). That happens at most once per room exit more than the same exit without copies
// would lose (after it the table holds a copy's record, which every later write takes first); the purge reports each
// one (EvAtPurge: the slot of a real eviction holds a copy's record).
#include "recevict_sim.h"

typedef struct { unsigned id; int type, state, stage, room, cls, on; } EvGone;
static int g_evOn;                       // 0x464060 is EvAllocC
static int g_evCopyFirst = 1;            // [Gameplay] CopyEvictFirst (ini only)
static EvGone g_evPend[EV_REC_N];        // per slot: the real record the last eviction there took, until the next purge
static int g_evPendAny;
static LONG g_evCopyN, g_evRealN, g_evForCopyN, g_evPeak;  // this session (game thread)
static int g_evRoomCopy, g_evRoomReal, g_evRoomForCopy, g_evRoomPurged, g_evRoomPeak, g_evRoomAllocs;
static DWORD g_evRoomKey = 0xFFFFFFFFu;
static volatile LONG g_evQuotaEvict, g_evQuotaCopy, g_evQuotaForCopy, g_evQuotaAnom, g_evQuotaRoom, g_evQuotaFault;

static int EvFaultNote(EXCEPTION_POINTERS *e, const char *where)
{
    if (LogQuota(&g_evQuotaFault, 5))
        LogAnomaly("enemy records", "%s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
                   e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// 0x464060 (jmp; cdecl, no arguments, returns the record; game thread: a type's record save at a room exit, or
// EnemyManagerSave). The caller writes the record through the pointer.
static BYTE *__cdecl EvAllocC(void)
{
    BYTE *const recs = (BYTE *)0xFCF090;
    const int stage = *(volatile const int *)0xFD5A90, room = *(volatile const int *)0xFD5A94;  // 0x46F520 / 0x46F530
    int cls = EV_CLS_FREE, copies = 0, used = 0, i, se;
    EvGone g;
    BYTE *r;
    i = EvChoose(recs, stage, room, g_evCopyFirst, &cls, &copies, &used);
    r = recs + i * EV_REC_SIZE;
    ++g_evRoomAllocs;
    if (used > g_evRoomPeak) g_evRoomPeak = used;
    if (used > g_evPeak) g_evPeak = used;
    g.id = (unsigned)EvI32(r, 0x20), g.type = EvI16(r, 0x24), g.state = EvI16(r, 0x26), g.stage = EvI8(r, 0x36);
    g.room = EvI8(r, 0x37), g.cls = cls, g.on = 1;
    if (cls == EV_CLS_COPY) {
        __try {
            EnmCopyRecordGone(r);  // enemies.inc: what the purge keeps of it (ghost-copy store, door, ledger)
        } __except (EvFaultNote(GetExceptionInformation(), "keeping a copy's record")) {
        }
        ++g_evCopyN, ++g_evRoomCopy;
    } else if (cls != EV_CLS_FREE) {
        g_evPend[i] = g, g_evPendAny = 1;
        ++g_evRealN, ++g_evRoomReal;
    }
    se = EvEvict(r, cls, (unsigned *)0xFCF010, (float *)0xFD0B90, stage, room);
    if (cls == EV_CLS_COPY) {
        if (LogQuota(&g_evQuotaCopy, 300))
            LogV("enemy records: the table is full (64 in use): copy %08X's record (type %02X, room %d:%d, state %02X) makes "
                 "room for a new one (copies' records go first; Refined keeps what it needs of it); %d copies' record(s) "
                 "were in the table", g.id, g.type, g.stage, g.room, g.state, copies);
    } else if (cls != EV_CLS_FREE) {
        if (copies > 0 && g_evCopyFirst && LogQuota(&g_evQuotaAnom, 10))
            LogAnomaly("enemy records", "a real record (%08X, type %02X) was evicted while %d copies' record(s) were in the "
                       "table (copies go first)", g.id, g.type, copies);
        if (LogQuota(&g_evQuotaEvict, 300))
            LogV("enemy records: the table is full (64 in use, %d copies' record(s)): %08X's record (type %02X, stage %d room "
                 "%d, state %02X) is evicted for a new one: %s%s", copies, g.id, g.type, g.stage, g.room, g.state,
                 EvClassName(cls), se == 1 ? " (its kill bit set)" : se == -1 ? " (its kill bit is outside the table: not "
                 "set)" : "");
    }
    return r;
}

// enemies.inc EnmPurgeCopyRecords (game thread), for every copy's record it deletes: was the slot taken from a real one?
static void EvAtPurge(int slot, const BYTE *r)
{
    EvGone *g;
    if (!g_evPendAny || slot < 0 || slot >= EV_REC_N || !g_evPend[slot].on) return;
    g = &g_evPend[slot];
    ++g_evForCopyN, ++g_evRoomForCopy;
    if (LogQuota(&g_evQuotaForCopy, 100))
        LogV("enemy records: copy %08X's record had taken slot %d from %08X's (type %02X, stage %d room %d, state %02X): that "
             "record is gone (%s); at most one real record per room exit goes for the copies this way", *(const unsigned *)(r + 0x20),
             slot, g->id, g->type, g->stage, g->room, g->state, EvClassName(g->cls));
    g->on = 0;
}
static void EvAtPurgeEnd(int purged)
{
    if (g_evPendAny) memset(g_evPend, 0, sizeof g_evPend), g_evPendAny = 0;
    g_evRoomPurged += purged;
}

// QsTick (0x4EF982, game thread; after EnmTick's purge): one verbose line per room left with what its exit did
static void EvTick(void)
{
    DWORD key = (DWORD)*(volatile int *)0xFD5A90 << 16 | (DWORD)(*(volatile int *)0xFD5A94 & 0xFFFF);
    int i, used = 0, copies = 0;
    if (!g_enmInstalled && g_evPendAny) EvAtPurgeEnd(0);  // (no purge runs without enemies.inc: nothing to report)
    if (key == g_evRoomKey) return;
    __try {
        const BYTE *r = (const BYTE *)0xFCF090;
        for (i = 0; i < EV_REC_N; ++i, r += EV_REC_SIZE)
            if (EvI16(r, 0x26)) ++used, copies += EvIsCopyId((unsigned)EvI32(r, 0x20));
    } __except (EvFaultNote(GetExceptionInformation(), "the room count")) {
        g_evRoomKey = key;
        return;
    }
    if (g_evRoomKey != 0xFFFFFFFFu && LogQuota(&g_evQuotaRoom, 1500))
        LogV("enemy records: room %d:%d left: %d of 64 in use now (%d copies'), %d at most at its exit's %d write(s); %d copies' "
             "record(s) purged; evictions: %d copies' record(s), %d real (%d of them for a copy's write); session %ld / %ld / %ld, "
             "most in use %ld (%s)", (int)(g_evRoomKey >> 16), (int)(g_evRoomKey & 0xFFFF), used, copies, g_evRoomPeak,
             g_evRoomAllocs, g_evRoomPurged, g_evRoomCopy, g_evRoomReal, g_evRoomForCopy, g_evCopyN, g_evRealN, g_evForCopyN,
             g_evPeak, g_evOn ? "copies first" : "the game's own eviction: no eviction counts");
    g_evRoomCopy = g_evRoomReal = g_evRoomForCopy = g_evRoomPurged = g_evRoomPeak = g_evRoomAllocs = 0;
    g_evRoomKey = key;
}

static void RecEvictInstall(void)
{
    static const Site k[] = {
        // sub esp,80h / push ebx / push esi / push edi / call 46F520 / mov [esp+10h],eax / call 46F530
        { 0x464060, { 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00, 0x53, 0x56, 0x57, 0xE8, 0xB2, 0xB4, 0x00, 0x00, 0x89, 0x44, 0x24, 0x10,
                      0xE8, 0xB9, 0xB4, 0x00, 0x00 }, 23 },
        { 0x46409C, { 0xBB, 0xC8, 0xF0, 0xFC, 0x00 }, 5 },                 // mov ebx,0FCF0C8h (rec[0] + 0x38)
        { 0x4641D1, { 0x83, 0xF8, 0x40 }, 3 },                              // cmp eax,40h (64 records)
        { 0x464206, { 0x8D, 0xB1, 0x90, 0xF0, 0xFC, 0x00 }, 6 },            // lea esi,[ecx+0FCF090h]
        { 0x464246, { 0xD9, 0x05, 0x90, 0x0B, 0xFD, 0x00, 0xD8, 0x05, 0x7C, 0x74, 0x5B, 0x00 }, 12 },  // +0x1B90 += [5B747C]
        { 0x464301, { 0x6A, 0x50, 0x56, 0xE8, 0x87, 0x78, 0xFC, 0xFF }, 8 },  // push 50h / push esi / call 42BB90
        { 0x464326, { 0x81, 0xC4, 0x80, 0x00, 0x00, 0x00, 0xC3 }, 7 },      // add esp,80h / ret
        { 0x46F520, { 0xA1, 0x90, 0x5A, 0xFD, 0x00, 0xC3 }, 6 },            // gGame.stage = [0xFD5A90]
        { 0x46F530, { 0xA1, 0x94, 0x5A, 0xFD, 0x00, 0xC3 }, 6 },            // gGame.room = [0xFD5A94]
    };
    GpPatch set[1];
    int i;
    g_evCopyFirst = GetPrivateProfileIntA("Gameplay", "CopyEvictFirst", 1, g_iniPath) != 0;
    if (!g_evCopyFirst) {
        Log("enemy records: the game's own eviction ([Gameplay] CopyEvictFirst=0): a full table may evict real records while "
            "copies' records are in it");
        return;
    }
    for (i = 0; i < (int)(sizeof k / sizeof k[0]); ++i)
        if (!SiteOk(&k[i])) {
            Log("enemy records: %08lX is not stock (another mod?): the game's own eviction stays", k[i].va);
            return;
        }
    GpBranch(&set[0], 0x464060, 0xE9, EvAllocC, 6);
    g_evOn = GpApplyAll(set, 1, "enemy records (copies first)");
    if (g_evOn)
        Log("enemy records: copies first (00464060 -> Refined's copy of EnemyRecordAlloc: on a full table a copy's record goes "
            "before any real one; real ones in the game's own order); [Gameplay] CopyEvictFirst=1");
}
