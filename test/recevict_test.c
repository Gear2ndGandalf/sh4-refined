// v3.36 HE336: native test of src\recevict_sim.h (test\build_recevict_test.cmd). (1) With copyFirst 0 the rule is the
// game's EnemyRecordAlloc 0x464060: compared, table byte for byte (ages, the clear, stage / room), kill bits and +0x1B90,
// with a transcription of its matched C (SH4Decomp src/enemy/en_463950.c 384-478) over random tables and chains of allocs.
// (2) copyFirst 1: a free slot first, then the oldest copy's record, then the stock order. (3) A room exit's writes on a
// full table: copies cost at most one real record more than the same exit without copies (the claim in recevict.inc),
// and the purge's report (a real eviction's slot holding a copy's record) counts exactly that one.
#include <stdio.h>
#include <string.h>
#include "../src/enemies_sim.h"
#include "../src/recevict_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- the reference: en_463950.c's types and 0x464060
typedef struct { float x, y, z, w; } Vec4;
typedef struct EnemyRecord {
    Vec4 pos;
    Vec4 rot;
    int id;
    short type;
    short state;
    float unk_28;
    float hp;
    short kill_index;
    signed char ai_state, variant, unk_34, unk_35, stage, room;
    int age;
    float unk_3C;
    int unk_40[4];
} EnemyRecord;
typedef struct { int kill_bits[32]; EnemyRecord rec[64]; float unk_1B90; } Mgr;
typedef char kSizeCheck[sizeof(EnemyRecord) == 0x50 ? 1 : -1];

static int RefStageBase(int stage)
{
    int result = 0;
    switch (stage) {
    case 2: result = 0x80; break;
    case 3: result = 0x100; break;
    case 4: result = 0x180; break;
    case 5: result = 0x300; break;
    case 7: result = 0x3c0; break;
    case 6: result = 0x20; break;
    case 8: result = 0x2c0; break;
    }
    return result;
}
static void RefKillBitSet(Mgr *m, int stage, int index)
{
    int bit;
    if (index > 0) {
        bit = RefStageBase(stage) + index;
        m->kill_bits[bit / 32] |= 1 << (bit & 31);
    }
}
static int RefSceneIdIsScript(int id)
{
    int room;
    if ((((unsigned int)id >> 24) & 0x1F) <= 0) return 0;
    room = ((unsigned int)id >> 16) & 0xFF;
    if (room <= 0) return 0;
    if ((id & 0xFF) < 0) return 0;
    return 1;
}
static int RefIsGhost(int type) { return type >= 0xb && type <= 0xf; }
// EnemyRecordAlloc, as the matched C; best_index[14] is best_age[0] in the exe's frame (the run-off case)
static int RefAlloc(Mgr *m, int stage, int room)
{
    int best_age[14], best_index[14];
    EnemyRecord *rec;
    int state, type, prio, age, index, bit, i;
    memset(best_age, 0, sizeof(best_age));
    memset(best_index, -1, sizeof(best_index));
    for (i = 0, rec = m->rec; i < 64; i++, rec++) {
        state = rec->state;
        type = rec->type;
        if (state == 0) {
            if (best_index[0] == -1) best_index[0] = i;
            continue;
        }
        age = rec->age++;
        if (RefIsGhost(type)) {
            prio = 13;
        } else if (rec->stage != stage) {
            if (!RefSceneIdIsScript(rec->id)) prio = 1;
            else prio = (state == 0xb) ? 6 : 2;
        } else if (rec->room != room) {
            if (!RefSceneIdIsScript(rec->id)) prio = 3;
            else if (type == 4) prio = 5;
            else if (type == 2) prio = 7;
            else if (type == 9) prio = 8;
            else prio = (state == 0xb) ? 11 : 9;
        } else {
            if (!RefSceneIdIsScript(rec->id)) prio = 4;
            else if (type == 4) prio = 5;
            else if (type == 2) prio = 7;
            else if (type == 9) prio = 8;
            else prio = (state == 0xb) ? 12 : 10;
        }
        if (age > best_age[prio]) {
            best_age[prio] = age;
            best_index[prio] = i;
        }
    }
    prio = 0;
    while (prio < 14 && best_index[prio] == -1) prio++;
    i = prio < 14 ? best_index[prio] : best_age[0];
    rec = &m->rec[i];
    if (prio == 6 || prio == 11 || prio == 12) {
        if (m->rec[i].type != 5 || m->rec[i].stage != 0xb) RefKillBitSet(m, rec->stage, rec->kill_index);
    } else if (prio == 8) {
        if (rec->state == 0xb) {
            index = rec->kill_index;
            if (index > 0) {
                bit = RefStageBase(rec->stage) + index;
                m->kill_bits[bit / 32] |= 1 << (bit & 31);
            }
        } else {
            m->unk_1B90 += 0.005f;
            if (m->unk_1B90 > 2.0f) m->unk_1B90 = 2.0f;
        }
    }
    memset(rec, 0, sizeof(EnemyRecord));
    rec->stage = (signed char)stage;
    rec->room = (signed char)room;
    rec->age = 0;
    return i;
}
// the dll's path: EvChoose then EvEvict over the same bytes
static int OurAlloc(Mgr *m, int stage, int room, int copyFirst, int *cls)
{
    int c, copies, used, i = EvChoose((unsigned char *)m->rec, stage, room, copyFirst, &c, &copies, &used);
    EvEvict((unsigned char *)&m->rec[i], c, (unsigned *)m->kill_bits, &m->unk_1B90, stage, room);
    if (cls) *cls = c;
    return i;
}

// ---------------------------------------------------------------- random tables
static unsigned g_seed = 12345;
static unsigned Rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return g_seed >> 8; }
static int Pick(const int *v, int n) { return v[Rnd() % (unsigned)n]; }
static const int kTypes[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x14 };
static const int kStates[] = { 1, 3, 4, 0x0B, 0x0B, 0x0F, 0x10 };
static unsigned RandId(int stage, int room)
{
    switch (Rnd() % 6) {
    case 0: return Rnd() % 40 + 1;                                                   // a run-time id (stage 0)
    case 1: return 0x1E000000u | (Rnd() & 0xFFFFu);                                  // a copy's id
    case 2: return (unsigned)(stage & 0x1F) << 24 | (Rnd() % 3) << 16 | (Rnd() & 0xFF);  // a placement, maybe room 0
    case 3: return 0x80000000u | (unsigned)(stage & 0x1F) << 24 | (unsigned)(room & 0xFF) << 16 | (Rnd() & 0xFF);  // 2nd half
    default: return (unsigned)((Rnd() % 9) & 0x1F) << 24 | (unsigned)(Rnd() % 40) << 16 | (Rnd() & 0xFF);
    }
}
static void RandRec(EnemyRecord *r, int stage, int room, int freeChance)
{
    memset(r, 0, sizeof *r);
    if (freeChance && Rnd() % 100 < (unsigned)freeChance) return;
    r->stage = (signed char)(Rnd() % 3 ? stage : (int)(Rnd() % 9));
    r->room = (signed char)(Rnd() % 2 ? room : (int)(Rnd() % 40));
    r->id = (int)RandId(r->stage, r->room);
    r->type = (short)Pick(kTypes, sizeof kTypes / sizeof kTypes[0]);
    r->state = (short)Pick(kStates, sizeof kStates / sizeof kStates[0]);
    r->kill_index = (short)(Rnd() % 4 ? Rnd() % 60 : 0);
    r->age = (int)(Rnd() % 4 ? Rnd() % 30 : 0);
    r->hp = (float)(Rnd() % 1000);
    r->pos.x = (float)(Rnd() % 5000);
    if (Rnd() % 50 == 0) r->type = 5, r->stage = 0x0B;  // the 0x464278 exception
}
static void RandMgr(Mgr *m, int stage, int room, int freeChance)
{
    int i;
    memset(m, 0, sizeof *m);
    for (i = 0; i < 32; ++i) m->kill_bits[i] = (int)(Rnd() & Rnd());
    for (i = 0; i < 64; ++i) RandRec(&m->rec[i], stage, room, freeChance);
    m->unk_1B90 = 1.0f + (float)(Rnd() % 200) / 199.0f;
}

static void TestStockSame(void)
{
    static Mgr a, b;
    int t, k, same = 1, sameIdx = 1, n = 0, cls, full = 0, killed = 0, raised = 0;
    for (t = 0; t < 20000; ++t) {
        const int stage = 2 + (int)(Rnd() % 7), room = (int)(Rnd() % 40);
        RandMgr(&a, stage, room, t % 3 == 0 ? 0 : t % 3 == 1 ? 3 : 30);
        b = a;
        for (k = 0; k < 6; ++k) {  // a chain of allocs (ages grow, slots are reused); each new record gets a random body
            const int ia = RefAlloc(&a, stage, room), ib = OurAlloc(&b, stage, room, 0, &cls);
            ++n;
            if (ia != ib) sameIdx = 0;
            if (cls != EV_CLS_FREE) ++full;
            if (memcmp(&a, &b, sizeof a)) same = 0;
            if (!same || !sameIdx) {
                printf("  differs at try %d alloc %d: index %d vs %d (class %d)\n", t, k, ia, ib, cls);
                break;
            }
            RandRec(&a.rec[ia], stage, room, 0);
            if (a.rec[ia].state == 0) a.rec[ia].state = 1;
            b.rec[ib] = a.rec[ia];
        }
        if (!same || !sameIdx) break;
    }
    printf("  %d allocs compared with the matched C, %d of them on a full table\n", n, full);
    CHECK(same && sameIdx, "copyFirst 0 = the matched C of 0x464060 (%d allocs, %d on a full table)", n, full);
    // the side effects: full tables of placement ids of this stage (classes 7 .. 13 only), still compared with the C
    same = 1;
    for (t = 0; t < 8000; ++t) {
        static const int kHigh[] = { 1, 3, 9, 0x0A, 0x0B, 5 };
        const int stage = 2 + (int)(Rnd() % 7), room = (int)(Rnd() % 40) + 1, nt = 3 + (int)(Rnd() % 4);
        Mgr before;
        int i;
        RandMgr(&a, stage, room, 0);
        for (i = 0; i < 64; ++i) {
            EnemyRecord *e = &a.rec[i];
            e->stage = (signed char)stage;
            e->id = (int)((unsigned)stage << 24 | (unsigned)(e->room > 0 ? e->room : 1) << 16 | (unsigned)i);
            e->type = (short)kHigh[Rnd() % (unsigned)nt];
            if (Rnd() % 20 == 0) e->stage = 0x0B, e->type = 5;  // the 0x464278 exception (another stage: class 6)
        }
        before = a;
        b = a;
        RefAlloc(&b, stage, room);
        OurAlloc(&a, stage, room, 0, &cls);
        if (memcmp(&a, &b, sizeof a)) same = 0;
        if (memcmp(a.kill_bits, before.kill_bits, sizeof a.kill_bits)) ++killed;
        if (a.unk_1B90 != before.unk_1B90) ++raised;
    }
    printf("  side effects: %d kill bits set, %d raises of +0x1B90 in 8000 evictions, all as the matched C\n", killed, raised);
    CHECK(same, "the eviction's side effects = the matched C");
    CHECK(killed > 50 && raised > 50, "the kill-bit and +0x1B90 paths were taken (%d / %d)", killed, raised);
}

static void TestRunoff(void)
{
    static Mgr a, b;
    int i, cls;
    memset(&a, 0, sizeof a);
    for (i = 0; i < 64; ++i) a.rec[i].state = 1, a.rec[i].type = 1, a.rec[i].id = 0x02030000 + i, a.rec[i].stage = 2,
                             a.rec[i].room = 3, a.rec[i].age = 0;
    b = a;
    CHECK(RefAlloc(&a, 2, 3) == 0 && OurAlloc(&b, 2, 3, 0, &cls) == 0 && cls == EV_CLS_RUNOFF && !memcmp(&a, &b, sizeof a),
          "64 used, all age 0: slot 0 as the stock read past its table (class 14)");
}

static void TestCopyFirst(void)
{
    static Mgr a, b;
    int t, ok = 1, freeOk = 1, stockOk = 1, cls, nCopy = 0;
    for (t = 0; t < 20000; ++t) {
        const int stage = 2 + (int)(Rnd() % 7), room = (int)(Rnd() % 40);
        int i, copies = 0, bestAge = -1, best = -1, hasFree = 0, got;
        RandMgr(&a, stage, room, t % 2 ? 0 : 2);
        for (i = 0; i < 64; ++i) {
            if (!a.rec[i].state) { hasFree = 1; continue; }
            if (EnmIsCopyId((unsigned)a.rec[i].id)) {
                ++copies;
                if (a.rec[i].age > bestAge) bestAge = a.rec[i].age, best = i;
            }
        }
        b = a;
        got = OurAlloc(&a, stage, room, 1, &cls);
        if (hasFree) {
            const int ref = RefAlloc(&b, stage, room);
            if (got != ref || cls != EV_CLS_FREE || memcmp(&a, &b, sizeof a)) freeOk = 0;
        } else if (copies) {
            ++nCopy;
            if (got != best || cls != EV_CLS_COPY) ok = 0;
        } else {
            const int ref = RefAlloc(&b, stage, room);
            if (got != ref || memcmp(&a, &b, sizeof a)) stockOk = 0;
        }
    }
    CHECK(freeOk, "copyFirst 1, a free slot: as the stock (the first free)");
    CHECK(ok && nCopy > 1000, "copyFirst 1, full with copies' records: the oldest copy's record (%d cases)", nCopy);
    CHECK(stockOk, "copyFirst 1, full without copies' records: as the stock");
    {   // no side effect for a copy's record; a ghost copy goes first too (stock: class 13, the last)
        Mgr before;
        int i;
        memset(&a, 0, sizeof a);
        for (i = 0; i < 64; ++i) a.rec[i].state = 0x0B, a.rec[i].type = 1, a.rec[i].id = 0x05010000 + i, a.rec[i].stage = 3,
                                 a.rec[i].room = 1, a.rec[i].kill_index = (short)(i + 1), a.rec[i].age = 5;
        a.rec[40].id = 0x1E000123, a.rec[40].type = 0x0C, a.rec[40].age = 1;  // a ghost copy, young
        before = a;
        CHECK(OurAlloc(&a, 5, 2, 1, &cls) == 40 && cls == EV_CLS_COPY, "a ghost copy's record goes before 63 downed real ones");
        CHECK(!memcmp(a.kill_bits, before.kill_bits, sizeof a.kill_bits), "no kill bit for a copy's record");
        a = before;
        CHECK(OurAlloc(&a, 5, 2, 0, &cls) == 0 && cls == 6 && a.kill_bits[(0x100 + 1) / 32] != before.kill_bits[(0x100 + 1) / 32],
              "stock: a downed real record of another stage goes (dead for good: its kill bit)");
    }
}

// one room exit: a full table (no copies' records unless asked), then writes; returns the real records evicted
static int Exit(Mgr *m, const char *seq, int stage, int room, int copyFirst, int *reported)
{
    int pend[64], i, k, cls, lost = 0, nCopy = 0;
    memset(pend, 0, sizeof pend);
    for (k = 0; seq[k]; ++k) {
        const int copy = seq[k] == 'C' || seq[k] == 'G';
        i = OurAlloc(m, stage, room, copyFirst, &cls);
        if (cls != EV_CLS_FREE && cls != EV_CLS_COPY) ++lost, pend[i] = 1;  // recevict.inc's g_evPend
        m->rec[i].state = 1;
        m->rec[i].type = (short)(seq[k] == 'G' ? 0x0C : 3);
        m->rec[i].id = copy ? (int)(0x1E000000u | (unsigned)(++nCopy * 4)) : (int)(0x02000000u | (unsigned)room << 16 | (unsigned)k);
        m->rec[i].kill_index = copy ? 0 : (short)(k + 1);
    }
    if (reported) {  // the purge: a copy's record in a slot a real eviction emptied
        *reported = 0;
        for (i = 0; i < 64; ++i) *reported += pend[i] && m->rec[i].state && EnmIsCopyId((unsigned)m->rec[i].id);
    }
    return lost;
}
static void TestExitBound(void)
{
    static Mgr base, a, b, c;
    char seq[80], reals[80];
    int t, worst = 0, ok = 1, repOk = 1, stockWorse = 0, stockMax = 0, sumOurs = 0, sumStock = 0;
    for (t = 0; t < 20000; ++t) {
        const int stage = 2 + (int)(Rnd() % 7), room = (int)(Rnd() % 30) + 1, len = 1 + (int)(Rnd() % 60);
        const int freeN = t % 4 == 0 ? (int)(Rnd() % 12) : 0, ghosts = t % 3 == 0;
        int i, r = 0, l0, l1, ls, rep;
        memset(&base, 0, sizeof base);
        for (i = 0; i < 64; ++i) {  // real records of this stage's other rooms and of other stages, standing or downed
            EnemyRecord *e = &base.rec[i];
            e->state = (short)(Rnd() % 3 ? 1 : 0x0B), e->type = (short)Pick(kTypes, 10);  // 1 .. 0x0A: no ghost
            e->stage = (signed char)(Rnd() % 4 ? stage : 2 + (int)(Rnd() % 7)), e->room = (signed char)(1 + Rnd() % 30);
            e->id = (int)((unsigned)(e->stage & 0x1F) << 24 | (unsigned)(e->room & 0xFF) << 16 | (unsigned)i);
            e->kill_index = (short)(i + 1), e->age = (int)(Rnd() % 20 + 1);
        }
        for (i = 0; i < freeN; ++i) base.rec[Rnd() % 64].state = 0;
        for (i = 0; i < len; ++i) {
            seq[i] = Rnd() % 3 ? (ghosts && Rnd() % 2 ? 'G' : 'C') : 'R';
            if (seq[i] == 'R') reals[r++] = 'R';
        }
        seq[len] = 0, reals[r] = 0;
        a = base, b = base, c = base;
        l1 = Exit(&a, seq, stage, room, 1, &rep);      // copies first
        l0 = Exit(&b, reals, stage, room, 0, NULL);    // the same exit without copies (the game's rule)
        ls = Exit(&c, seq, stage, room, 0, NULL);      // the game's rule with the copies
        sumOurs += l1, sumStock += ls;
        if (l1 - l0 > worst) worst = l1 - l0;
        if (l1 - l0 > 1) {
            ok = 0;
            printf("  exit %d: copies first lost %d, without copies %d: %s (free %d)\n", t, l1, l0, seq, freeN);
        }
        if (freeN == 0 && rep != l1 - l0) repOk = 0;
        if (rep > 1) repOk = 0;
        if (ls > l1) ++stockWorse;
        if (ls - l0 > stockMax) stockMax = ls - l0;
    }
    printf("  20000 exits: copies first lose at most %d real record(s) more than without copies; the game's rule with the "
           "copies: %d exits worse than copies first, up to %d more real records than without copies; %d lost in all "
           "against %d copies first\n", worst, stockWorse, stockMax, sumStock, sumOurs);
    CHECK(ok, "copies cost at most one real record per room exit over the same exit without copies (worst %d)", worst);
    CHECK(repOk, "the purge's report counts that one (a full table) and never more than one");
    CHECK(stockWorse > 1000 && stockMax > 10, "the game's rule with copies loses more (%d exits worse, up to %d more real records; "
          "%d lost in all against %d copies first)", stockWorse, stockMax, sumStock, sumOurs);
}

static void TestHelpers(void)
{
    unsigned id;
    int s, k, w1, w2, ok = 1;
    unsigned m1, m2;
    for (id = 0; id < 0x30000000u; id += 0x00010101u + (id & 7u))
        if (EvIsCopyId(id) != EnmIsCopyId(id)) ok = 0;
    CHECK(ok && EvIsCopyId(0x1E000000u) && EvIsCopyId(0x1E00FFFFu) && !EvIsCopyId(0x1E010000u) && !EvIsCopyId(0x9E000001u),
          "EvIsCopyId = enemies_sim.h EnmIsCopyId");
    ok = 1;
    for (s = 0; s < 10; ++s)
        for (k = -2; k < 1100; ++k) {
            const int a = EvKillBit(s, k, &w1, &m1), b = EnmKillBit(s, k, &w2, &m2);
            if (a != b || (a && (w1 != w2 || m1 != m2))) ok = 0;
        }
    CHECK(ok, "EvKillBit = enemies_sim.h EnmKillBit (= 0x463A20 inside the 1024 bits)");
    CHECK(EvIsScript(0x02030001u) && !EvIsScript(0x02000001u) && !EvIsScript(0x00030001u) && !EvIsScript(0x1E000005u) &&
          EvIsScript(0x82030001u), "EvIsScript = 0x52C8D0 (a copy's id is no placement id)");
    for (k = 0; k <= 14; ++k) CHECK(EvClassName(k)[0] != '?', "class %d named", k);
    CHECK(EvClassName(EV_CLS_COPY)[0] == 'a', "the copy class named");
}

int main(void)
{
    TestHelpers();
    TestStockSame();
    TestRunoff();
    TestCopyFirst();
    TestExitBound();
    printf("recevict_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
