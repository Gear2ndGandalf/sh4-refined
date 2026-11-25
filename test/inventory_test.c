// v3.37 INV337: native test of Extended Toolbelt / Stackable Items (src/inventory_sim.h) and of the generated cave
// (src/inventory_cave.inc, the cheat table's code translated to MSVC inline asm by tools/inv337_gen.py).
// The cave's helpers run for real here: the test maps the game's inventory (0x13D7B60), the empty template (0x107A088),
// the bar / sub-mode words and the weapon in hand (0x10A1164) at their real addresses, and the exe's item tables
// (0x5BF100.., test/inventory_rdata.h) inside its own image (linked at 0x400000, fixed, with a 4 MB bss that covers
// them), then compares every helper's answer and writes with the model. Helpers that call into the game are not run.
// build_inventory_test.cmd builds and runs it (x86 /O2, no ASLR).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../src/inventory_sim.h"
#include "inventory_rdata.h"

static volatile LONG g_invTbLive, g_invStOn;
#include "../src/inventory_cave.inc"

static int g_fails, g_checks;
#define CHECK(c, ...) do { ++g_checks; if (!(c)) { if (++g_fails <= 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static BYTE g_low[0x400000];  // covers 0x5BF000..0x5C0000 (the image is fixed at 0x400000)
#define BLK ((InvBlock *)0x13D7B60)
#define IN_HAND (*(volatile int *)0x10A1164)
static const unsigned char *g_rank, *g_brk;
static const float *g_newc;
static const InvEnt kTmpl;

static unsigned g_rng = 12345;
static unsigned Rnd(unsigned n) { g_rng = g_rng * 1103515245u + 12345u; return (g_rng >> 8) % n; }

// ---------------------------------------------------------------- calling the cave's register helpers
static DWORD g_tA, g_tEax, g_tEdx, g_tEsi, g_tOut, g_tEbx, g_tEcx, g_tEdi, g_tEbp, g_tOutEdx, g_tOutEsi;
static __declspec(naked) void TRun(void)
{
    __asm {
        pushad
        mov eax, g_tEax
        mov edx, g_tEdx
        mov esi, g_tEsi
        mov ebx, 0x11111111
        mov ecx, 0x22222222
        mov edi, 0x33333333
        mov ebp, 0x44444444
        call dword ptr [g_tA]
        mov g_tOut, eax
        mov g_tEbx, ebx
        mov g_tEcx, ecx
        mov g_tEdi, edi
        mov g_tEbp, ebp
        mov g_tOutEdx, edx
        mov g_tOutEsi, esi
        popad
        ret
    }
}
static DWORD Reg(DWORD addr, DWORD eax, DWORD edx, DWORD esi)
{
    g_tA = addr, g_tEax = eax, g_tEdx = edx, g_tEsi = esi;
    TRun();
    return g_tOut;
}
static int Kept(int edxToo, int esiToo)  // the helper kept ebx, ecx, edi, ebp (and edx / esi when asked)
{
    return g_tEbx == 0x11111111 && g_tEcx == 0x22222222 && g_tEdi == 0x33333333 && g_tEbp == 0x44444444 &&
           (!edxToo || g_tOutEdx == g_tEdx) && (!esiToo || g_tOutEsi == g_tEsi);
}
#define PK(l) g_pkLbl[PKL_##l]
#define ST(l) g_stLbl[STL_##l]

// ---------------------------------------------------------------- random inventories
static const unsigned char kIds[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x11, 0x12, 0x15, 0x16,
                                      0x17, 0x2D, 0x2E, 0x33, 0x35, 0x3C, 0x40, 0x4D, 0x57, 0x7D, 0x80, 0x81, 0x82, 0x83, 0x84 };
static void RandEnt(InvEnt *e)
{
    e->id = kIds[Rnd(sizeof kIds)];
    e->count = (unsigned short)(Rnd(5) == 0 ? 0 : Rnd(4) == 0 ? 1 + Rnd(120) : 1 + Rnd(3));
    e->cond = Rnd(4) ? 1.0f : (float)Rnd(101);
    e->flag = 0;
}
static void RandPage(InvEnt *p, int holes)
{
    int n = (int)Rnd(11), i;
    memset(p, 0, 10 * sizeof *p);
    for (i = 0; i < n; ++i) RandEnt(&p[i]);
    if (holes && Rnd(4) == 0) memset(&p[Rnd(10)], 0, sizeof *p);
}
static void RandBlock(InvBlock *b, int holes)
{
    int k, n, i;
    memset(b, 0, sizeof *b);
    RandPage(b->e, 0);
    RandPage(InvP2(b), holes);
    n = Rnd(4) == 0 ? (int)Rnd(127) : (int)Rnd(20);
    for (k = 0; k < n; ++k) RandEnt(&b->e[INV_P1 + k]);
    b->chestCount = (unsigned short)n;
    if (Rnd(3) == 0) {  // one medallion in use, on a used entry of either page
        InvEnt *p = Rnd(2) ? b->e : InvP2(b);
        const int n1 = InvCnt10(p);
        if (n1) {
            InvEnt *e = &p[Rnd(n1)];
            if (e->id) e->id = INV_MEDALLION, e->flag = 1, e->count = 1, b->medFlag = 1;
        }
    }
    for (i = 0; i < 10; ++i)  // page 1 stays packed (the game's rule)
        if (!b->e[i].id) {
            int j;
            for (j = i + 1; j < 10 && !b->e[j].id; ++j) {}
            if (j < 10) b->e[i] = b->e[j], memset(&b->e[j], 0, sizeof b->e[j]);
        }
}
static int RandHand(const InvBlock *b)
{
    const int r = Rnd(6);
    if (r == 0) return 0;
    if (r == 1) return 0x80 + Rnd(5);
    if (r <= 3) {  // something carried
        const int idx = Rnd(2) ? (int)Rnd(10) : InvP2Index(Rnd(10));
        return b->e[idx].id;
    }
    return 0x10 + Rnd(0x26);
}

// ---------------------------------------------------------------- the model alone
static void TestModel(void)
{
    InvBlock b;
    InvOffResult r;
    int join, rest, made, n, i;
    unsigned char rec[10 * 0xB0];
    CHECK(sizeof(InvBlock) == 0x520, "block size");
    CHECK(InvP2Index(0) == 0x88 && InvP2Index(9) == 0x91 && InvIsP2Index(0x88) && !InvIsP2Index(0x92) && !InvIsP2Index(9),
          "page-2 indexes");
    CHECK((BYTE *)&BLK->e[InvP2Index(0)] == (BYTE *)0x13D7FA0 && InvChestOfIndex(0x88) == 126, "88h+i = 13D7FA0 + 8 x i = chest 126+i");
    // the gate: the table refused Playable Eileen's weapons on page 2 (the attack bug); v3.37 lets them and the weapon in hand
    for (i = 0x80; i <= 0x84; ++i) {
        CHECK(InvGateCt(0x89, i, 1) == -1, "CT gate refuses %02X on page 2", i);
        CHECK(InvGate(0x89, i, 0, 1) == 0x89, "v3.37 gate passes %02X on page 2", i);
        CHECK(InvGate(3, i, 0, 1) == 3, "page 1 always passes");
    }
    CHECK(InvGate(0x8A, 0x10, 0, 1) == 0x8A && InvGate(0x8A, 0x35, 0, 1) == 0x8A && InvGate(0x8A, 0x35, 0, 0) == -1, "Henry's weapons");
    CHECK(InvGate(0x8A, 0x05, 0, 1) == -1 && InvGate(0x8A, 0x05, 0x05, 1) == 0x8A, "the weapon in hand counts on page 2 whatever its id");
    CHECK(InvGate(-1, 0x80, 0x80, 1) == -1, "not found stays not found");
    CHECK(StThiefCap(0) == 0 && StThiefCap(10) == 10 && StThiefCap(99) == 10, "thief cap");
    n = StGiveJoin(95, 12, 99, &rest);
    CHECK(n == 4 && rest == 8, "give 12 into 95/99: 4 join, 8 new");
    n = StGiveJoin(10, 12, 99, &rest);
    CHECK(n == 12 && rest == 0, "give joins whole");
    n = StPlan(93, 6, 99, 1, -1, 0, &join, &rest, &made);
    CHECK(n == 93 && join == 0 && rest == 93 && made, "Revolver x93 out as one entry");
    n = StPlan(93, 6, 99, 1, 90, 0, &join, &rest, &made);
    CHECK(n == 93 && join == 9 && rest == 84 && made, "9 join the stack, 84 new");
    n = StPlan(93, 6, 99, 1, 90, 1, &join, &rest, &made);
    CHECK(n == 9 && join == 9 && rest == 0 && !made, "belt full: only the join leaves");
    n = StPlan(0, 6, 99, 1, -1, 0, &join, &rest, &made);
    CHECK(n == 0 && made, "an empty gun moves as a new entry");
    n = StPlan(5, 1, 99, 0, -1, 0, &join, &rest, &made);
    CHECK(n == 1 && rest == 1, "take-out off: one unit");
    memset(rec, 0, sizeof rec);
    *(int *)(rec + 0x88) = 5, rec[0xA8] = 1;
    *(int *)(rec + 0xB0 + 0x88) = 1, rec[0xB0 + 0xA8] = 1;
    *(int *)(rec + 0x160 + 0x88) = 7, rec[0x160 + 0xA8] = 0;
    CHECK(StMarkCounts(rec) == 1 && rec[0xA8] == 5 && rec[0xB0 + 0xA8] == 1 && rec[0x160 + 0xA8] == 0, "count bits at enable");
    // the switch-off move
    memset(&b, 0, sizeof b);
    b.e[0].id = 0x10, b.e[0].count = 5;
    InvP2(&b)[0].id = 0x15, InvP2(&b)[0].count = 1, InvP2(&b)[0].cond = 100.0f;
    InvP2(&b)[1].id = 0x01, InvP2(&b)[1].count = 12;
    for (i = 0; i < 120; ++i) b.e[INV_P1 + i].id = 0x04, b.e[INV_P1 + i].count = 1;
    b.chestCount = 120;
    CHECK(InvOffPlan(&b, 0x15, &r) == INV_OFF_OK && r.hand && r.toPage1 == 1 && r.toChest == 1, "plan: hand to page 1");
    n = InvOffApply(&b, 0x15, g_rank, &kTmpl, &r);
    CHECK(n == 2 && InvCnt10(InvP2(&b)) == 0 && b.chestCount == 121 && b.e[INV_P1 + 120].id == 1 && InvFind(&b, 0x15) < 10,
          "apply: page 2 empty, the club on page 1, the bullets in the chest");
    CHECK(b.e[0].id == 0x15 && b.e[1].id == 0x10, "the club sorts before the pistol (rank)");
    memset(&b, 0, sizeof b);
    for (i = 0; i < 10; ++i) b.e[i].id = 0x09, InvP2(&b)[i].id = 0x0A;
    CHECK(InvOffPlan(&b, 0, &r) == INV_OFF_OK && r.toChest == 10, "10 into an empty chest");
    for (i = 0; i < 120; ++i) b.e[INV_P1 + i].id = 0x04;
    CHECK(InvOffPlan(&b, 0, &r) == INV_OFF_CHEST_FULL && r.chestFree == 6, "chest has room for 6: refused");
    memset(&b, 0, sizeof b);
    for (i = 0; i < 10; ++i) b.e[i].id = 0x09;
    InvP2(&b)[0].id = 0x82;
    CHECK(InvOffPlan(&b, 0x82, &r) == INV_OFF_HAND_NO_ROOM, "the night stick in hand, page 1 full: refused");
    InvP2(&b)[1].id = INV_MEDALLION, InvP2(&b)[1].flag = 1, b.medFlag = 1;
    CHECK(InvOffPlan(&b, 0, &r) == INV_OFF_OK && r.medOff && r.toChest == 2, "medallion in use, page 1 full: into the chest, off");
    InvOffApply(&b, 0, g_rank, &kTmpl, &r);
    CHECK(!b.medFlag && b.e[INV_P1 + 1].id == INV_MEDALLION && !b.e[INV_P1 + 1].flag, "taken off in the chest");
    memset(&b, 0, sizeof b);
    CHECK(InvOffPlan(&b, 0, &r) == INV_OFF_NOTHING, "nothing on page 2");
}

// the switch-off move keeps every item (random blocks)
static void TestOffRandom(void)
{
    int t;
    for (t = 0; t < 20000; ++t) {
        InvBlock b, b0;
        InvOffResult r;
        int hand, why, id, k, ok = 1;
        RandBlock(&b, 1);
        hand = RandHand(&b);
        b0 = b;
        why = InvOffPlan(&b, hand, &r);
        if (why != INV_OFF_OK) {
            CHECK(!memcmp(&b, &b0, sizeof b), "a refused plan writes nothing");
            continue;
        }
        InvOffApply(&b, hand, g_rank, &kTmpl, &r);
        CHECK(InvCnt10(InvP2(&b)) == 0, "page 2 empty after the move");
        for (id = 1; id < 256 && ok; ++id) {
            long n0 = 0, n1 = 0;
            for (k = 0; k < INV_ENTRIES; ++k) {
                if (b0.e[k].id == id) n0 += b0.e[k].count + 1000;
                if (b.e[k].id == id) n1 += b.e[k].count + 1000;
            }
            if (n0 != n1) ok = 0;
        }
        CHECK(ok, "every item kept (counts and entries)");
        CHECK(!hand || !r.hand || InvFind(&b, hand) < INV_P1, "the weapon in hand is on page 1");
        CHECK(b.chestCount == InvChestUsed(&b) && b.chestCount <= INV_CHEST_CAP, "chest count %d", b.chestCount);
        for (k = 0; k < 9; ++k) CHECK(b.e[k].id || !b.e[k + 1].id, "page 1 packed");
    }
}

// ---------------------------------------------------------------- the generated asm against the model
static void TestAsm(void)
{
    int t, i;
    INV_PK_PIN = 0, INV_PK_MEL = 1, INV_PK_OVF = 1, INV_PK_OPTC = 1;  // the table's defaults (pk_pin 0, pk_mel 1)
    CHECK(INV_PK_FLIP == 1 && INV_PK_IND == 1 && INV_PK_IND_X == 0x1F1 && INV_ST_MAX == 99 && INV_ST_MERGE == 1 &&
          INV_ST_BREAK == 1, "the data block's defaults");
    for (t = 0; t < 30000; ++t) {
        InvBlock m;
        int hand, id, idx, a, s, pinMed, pin0;
        RandBlock(BLK, 1);
        hand = RandHand(BLK);
        IN_HAND = hand;
        m = *BLK;
        id = Rnd(4) ? kIds[Rnd(sizeof kIds)] : (int)Rnd(0x90);
        // pk_find / pk_gate / pk_p2sum / pk_medslot / pk_cnt10
        a = (int)Reg(PK(pk_find), 0, id, 0);
        idx = InvFind(&m, id);
        CHECK(a == idx && Kept(1, 1), "pk_find %02X: asm %d model %d", id, a, idx);
        a = (int)Reg(PK(pk_gate), (DWORD)idx, id, 0);
        CHECK(a == InvGate(idx, id, hand, 1) && Kept(1, 1), "pk_gate %X %02X hand %02X", idx, id, hand);
        a = (int)Reg(PK(pk_p2sum), 0, id, 0);
        CHECK(a == InvP2Sum(&m, id) && Kept(1, 1), "pk_p2sum");
        a = (int)Reg(PK(pk_medslot), 0, 0, 0);
        CHECK(a == InvMedSlot(&m) && Kept(1, 1), "pk_medslot");
        a = (int)Reg(PK(pk_cnt10), 0, 0, 0x13D7FA0);
        CHECK(a == InvCnt10(InvP2(&m)) && Kept(1, 1), "pk_cnt10");
        a = (int)Reg(PK(pk_class), (DWORD)id, 0, 0);
        CHECK(a == InvClass(id, hand), "pk_class %02X hand %02X", id, hand);
        pin0 = InvPins(hand, 0, 1, &pinMed);
        a = (int)Reg(PK(pk_victim), 0, 0, 0);
        CHECK(a == InvVictim(&m, pin0, pinMed, hand) && Kept(1, 1), "pk_victim");
        a = ((int(__cdecl *)(void))PK(pk_full))();
        CHECK(a == InvFull(&m, 1, pin0, pinMed, hand), "pk_full");
        a = ((int(__cdecl *)(int))PK(pk_empty))(id);
        CHECK(a == InvEmpty(&m, id), "pk_empty %02X", id);
        a = ((int(__cdecl *)(int))PK(pk_has))(id);
        CHECK(a == InvHas(&m, id, 1), "pk_has %02X", id);
        // stackable: st_find with the toolbelt off / on, st_brki, st_newc
        g_invTbLive = (LONG)Rnd(2);
        a = (int)Reg(ST(st_find), 0, id, 0);
        CHECK(a == StFind(&m, id, 99, g_invTbLive) && Kept(1, 1), "st_find %02X tb %ld", id, g_invTbLive);
        a = (int)Reg(ST(st_brki), 0, id, 0);
        CHECK(a == StBrk(g_brk, id), "st_brki %02X", id);
        {
            const DWORD bits = Reg(ST(st_newc), 0, id, 0);
            const float want = StNewCond(g_brk, g_newc, id);
            CHECK(!memcmp(&bits, &want, 4), "st_newc %02X", id);
        }
        // pk_moff
        ((void(__cdecl *)(void))PK(pk_moff))();
        InvMedOff(&m);
        CHECK(!memcmp(BLK, &m, sizeof m), "pk_moff");
        // pk_do_swap (the flip)
        a = (int)Reg(PK(pk_do_swap), 0, 0, 0);
        s = InvSwap(&m, pin0, pinMed, g_rank, &kTmpl);
        CHECK(a == s && Kept(1, 1) && !memcmp(BLK, &m, sizeof m), "pk_do_swap: asm %d model %d", a, s);
        // pk_p2del on a random page-2 entry
        i = Rnd(10);
        Reg(PK(pk_p2del), 0x13D7FA0 + 8 * i, 0, 0);
        InvP2Del(&m, i, &kTmpl);
        CHECK(!memcmp(BLK, &m, sizeof m) && Kept(1, 1), "pk_p2del %d", i);
    }
    // a flip and back restores the pages exactly (the table's own claim: two swaps restore it) when page 2 holds items
    for (t = 0; t < 5000; ++t) {
        InvBlock b0;
        int pinMed, pin0;
        RandBlock(BLK, 0);
        IN_HAND = 0;
        pin0 = InvPins(0, 0, 1, &pinMed);
        (void)pin0;
        b0 = *BLK;
        if (InvCnt10(InvP2(BLK)) == 0 || InvCnt10(BLK->e) == 0) continue;
        Reg(PK(pk_do_swap), 0, 0, 0);
        CHECK(InvCnt10(BLK->e) + InvCnt10(InvP2(BLK)) == InvCnt10(b0.e) + InvCnt10(InvP2(&b0)), "a flip keeps every item");
    }
}

int main(void)
{
    void *hi;
    if ((DWORD)(ULONG_PTR)g_low > 0x5BF000 || (DWORD)(ULONG_PTR)(g_low + sizeof g_low) < 0x5C0000) {
        printf("FAIL: the bss does not cover 0x5BF000 (image at %p): link with /BASE:0x400000 /FIXED\n", (void *)g_low);
        return 1;
    }
    memcpy((void *)0x5BF100, kInvRdata5BF100, sizeof kInvRdata5BF100);
    g_rank = (const unsigned char *)0x5BF220;
    g_brk = (const unsigned char *)0x5BF188;
    g_newc = (const float *)0x5BF198;
    hi = VirtualAlloc((void *)0x1070000, 0x370000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);  // 107A088 .. 13D8080
    if (hi != (void *)0x1070000) {
        printf("FAIL: 0x1070000..0x13E0000 is not free in this process (%p)\n", hi);
        return 1;
    }
    InvPkCave();
    InvStCave();
    CHECK(g_pkLbl[PKL_pk_frame] && g_stLbl[STL_st_give], "label addresses");
    CHECK(g_rank[0] == 0x12 && g_brk[0] == 0x15, "the exe's tables (rank starts with 12h, breakables with 15h)");
    TestModel();
    TestOffRandom();
    TestAsm();
    printf("%s: %d checks, %d failed (inventory: model, switch-off move, generated asm vs model)\n", g_fails ? "FAILED" : "ALL OK",
           g_checks, g_fails);
    return g_fails != 0;
}
