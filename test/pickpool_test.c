// v3.35 PK335: native test of src\pickpool_sim.h (test\build_pickpool_test.cmd): the larger pickup pool's alloc order,
// the record fill (as 0x4F3BA0's bytes), the save's 32-record priority plan, the load's clear and the buffer rules.
#include <stdio.h>
#include <string.h>
#include "../src/pickpool_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned char g_pool[PP_REC_MAX * PP_REC_SIZE], g_img[PP_STOCK_N * PP_REC_SIZE];
static unsigned char *Rec(int i) { return g_pool + i * PP_REC_SIZE; }

static void TestPoolN(void)
{
    CHECK(PpPoolN(0) == 32, "0 = the stock pool");
    CHECK(PpPoolN(-5) == 32, "negative = the stock pool");
    CHECK(PpPoolN(32) == 32, "32 = the stock pool");
    CHECK(PpPoolN(33) == 33, "33");
    CHECK(PpPoolN(96) == 96, "96");
    CHECK(PpPoolN(128) == 128, "128");
    CHECK(PpPoolN(100000) == PP_REC_MAX, "clamped to the buffer");
    CHECK(PpPoolN(PP_DEFAULT_N) == 128, "the default moves the pool to 128");
}

static void TestAlloc(void)
{
    int i;
    memset(g_pool, 0, sizeof g_pool);
    CHECK(PpAllocPick(g_pool, 128, 0) == 0, "empty: the game takes slot 0");
    CHECK(PpAllocPick(g_pool, 128, 1) == 32, "empty: Supply Drops take slot 32");
    CHECK(PpAllocPick(g_pool, 32, 1) == 0, "stock pool: Supply Drops take slot 0 (no high slots)");
    for (i = 0; i < 32; ++i) Rec(i)[0] = 1;
    CHECK(PpAllocPick(g_pool, 128, 0) == 32, "low 32 used: the game goes on at 32");
    CHECK(PpAllocPick(g_pool, 32, 0) == -1, "stock pool full: -1");
    for (i = 32; i < 128; ++i) Rec(i)[0] = 2;
    Rec(5)[0] = 0;
    CHECK(PpAllocPick(g_pool, 128, 1) == 5, "high slots full: Supply Drops fall back to a low one");
    CHECK(PpAllocPick(g_pool, 128, 0) == 5, "the game: the first free");
    Rec(5)[0] = 1;
    CHECK(PpAllocPick(g_pool, 128, 0) == -1 && PpAllocPick(g_pool, 128, 1) == -1, "all 128 used: -1 (the guard's case)");
    Rec(127)[0] = 0;
    Rec(40)[0] = 0;
    CHECK(PpAllocPick(g_pool, 128, 1) == 40 && PpAllocPick(g_pool, 128, 0) == 40, "the first free of the high slots");
    CHECK(PpAllocPick(g_pool, 96, 1) == 40 && PpAllocPick(g_pool, 40, 0) == -1, "n bounds the search");
    memset(g_pool, 0, sizeof g_pool);
    Rec(0)[1] = 1;  // stale bytes in a free slot do not count: only the state byte
    Rec(0)[2] = 1;
    CHECK(PpAllocPick(g_pool, 128, 0) == 0, "a free slot with stale bytes is free");
}

static void PutI(unsigned char *p, int v) { memcpy(p, &v, 4); }
static void PutS(unsigned char *p, short v) { memcpy(p, &v, 2); }
static int GetI(const unsigned char *p) { int v; memcpy(&v, p, 4); return v; }
static short GetS(const unsigned char *p) { short v; memcpy(&v, p, 2); return v; }

// the fields as the GOG v7 alloc writes them (0x4F3BC7..0x4F3D0A)
static void TestFill(void)
{
    unsigned char def[0x94], r[PP_REC_SIZE];
    int shape, flags8, i;
    for (shape = 0; shape <= 4; ++shape)
        for (flags8 = 0; flags8 <= 1; ++flags8) {
            for (i = 0; i < (int)sizeof def; ++i) def[i] = (unsigned char)(0x40 + i);
            PutS(def + 0x00, 4);                                   // kind
            PutI(def + 0x38, shape);                               // zoneShape
            PutI(def + 0x4C, flags8 ? 0x0D : 0x05);                // zoneFlags (0x0D: bit 8 set)
            memset(r, 0xCC, sizeof r);                             // the slot's last user
            PpRecFill(r, def, 0, 3, 0x1234);
            CHECK(r[0] == 1 && GetS(r + 2) == 1, "state 1, active 1");
            CHECK(r[1] == 0, "unk_01 zeroed (no stale pin Sword)");
            CHECK(GetS(r + 4) == 0 && GetS(r + 6) == 3 && GetS(r + 8) == 0x1234, "arg, stage, room");
            CHECK(GetS(r + 0x0A) == 4, "type = kind");
            CHECK(!memcmp(r + 0x10, def + 0x14, 12), "pos");
            CHECK(GetI(r + 0x1C) == 0, "pos.w zero (stock: stale)");
            CHECK(!memcmp(r + 0x30, def + 0x24, 12), "model, texture, item");
            CHECK(!memcmp(r + 0x3C, def + 0x30, 2) && GetS(r + 0x3E) == 0, "difficulty (a short)");
            CHECK(!memcmp(r + 0x40, def + 0x14, 12) && GetI(r + 0x4C) == 0, "pos2 = pos");
            CHECK(GetI(r + 0x50) == shape && GetI(r + 0x54) == GetI(def + 0x4C), "zone shape and flags");
            CHECK(!memcmp(r + 0x58, def + 0x48, 4), "zone heading");
            CHECK(GetI(r + 0x5C) == 0 && GetI(r + 0x6C) == 0, "the gaps zeroed");
            {
                const int want = flags8 ? 0 : shape == 1 ? 8 : shape == 2 ? 12 : shape == 3 ? 4 : 0;
                unsigned char zero[12];
                memset(zero, 0, sizeof zero);
                CHECK(!memcmp(r + 0x60, def + 0x3C, (size_t)want) && !memcmp(r + 0x60 + want, zero, (size_t)(12 - want)),
                      "zone size: shape %d flags8 %d copies %d bytes", shape, flags8, want);
            }
            for (i = 0x70; i < PP_REC_SIZE; ++i) CHECK(r[i] == 0, "+%X zeroed (owner id, Supply Drops' marker)", i);
            CHECK(r[0x20] == 0, "rot left to 0x421B30 (zero here)");
        }
    PpRecFill(r, def, 7, 0x10002, 0x30004);
    CHECK(GetS(r + 4) == 7 && GetS(r + 6) == 2 && GetS(r + 8) == 4, "the low words of arg / stage / room");
}

// a record of the given kind: state, sword (unk_01), ours (the view says so), seq; its slot index in +0x38
static void MakeViews(PpView *v, int n, const int *kind, const int *seq)
{
    int i;
    for (i = 0; i < n; ++i) {
        v[i].state = kind[i] != PPC_FREE;
        v[i].sword = kind[i] == PPC_SWORD;
        v[i].ours = kind[i] == PPC_OURS;
        v[i].seq = seq ? seq[i] : 0;
    }
}
static int PlanValid(const int *plan, int kept, int n)
{
    int used[PP_REC_MAX], k, c = 0;
    memset(used, 0, sizeof used);
    for (k = 0; k < PP_STOCK_N; ++k) {
        if (plan[k] < 0) continue;
        if (plan[k] >= n || used[plan[k]]) return 0;
        used[plan[k]] = 1;
        ++c;
    }
    return c == kept;
}

static void TestSave(void)
{
    PpView v[PP_REC_MAX];
    int kind[PP_REC_MAX], seq[PP_REC_MAX], plan[PP_STOCK_N], act, kept, i, k, ours, swords, game, freeN;
    // a: everything in the low 32: the image = the same records in the same slots (as the stock save)
    for (i = 0; i < 128; ++i) kind[i] = PPC_FREE, seq[i] = 0;
    kind[0] = PPC_GAME, kind[3] = PPC_SWORD, kind[7] = PPC_OURS, kind[31] = PPC_GAME;
    MakeViews(v, 128, kind, seq);
    kept = PpSavePlan(v, 128, plan, &act);
    CHECK(kept == 4 && act == 4, "a: 4 kept of 4 (%d / %d)", kept, act);
    for (k = 0; k < 32; ++k) CHECK(plan[k] == (kind[k] != PPC_FREE ? k : -1), "a: slot %d keeps its place (%d)", k, plan[k]);
    // b: 45 in use: 10 Swords (4 above 31), 15 game (5 above), 20 ours (seq = slot); 32 kept: all 25 game, the 7 newest ours
    for (i = 0; i < 128; ++i) kind[i] = PPC_FREE, seq[i] = 0;
    for (i = 0; i < 6; ++i) kind[i * 2] = PPC_SWORD;            // 0 2 4 6 8 10
    for (i = 0; i < 4; ++i) kind[40 + i] = PPC_SWORD;           // 40..43
    for (i = 0; i < 10; ++i) kind[12 + i] = PPC_GAME;           // 12..21
    for (i = 0; i < 5; ++i) kind[50 + i] = PPC_GAME;            // 50..54
    for (i = 0; i < 8; ++i) kind[22 + i] = PPC_OURS, seq[22 + i] = 100 + i;  // 22..29, seq 100..107 (old)
    for (i = 0; i < 12; ++i) kind[60 + i] = PPC_OURS, seq[60 + i] = 200 + i; // 60..71, seq 200..211 (newer)
    MakeViews(v, 128, kind, seq);
    kept = PpSavePlan(v, 128, plan, &act);
    CHECK(kept == 32 && act == 45, "b: 32 kept of 45 (%d / %d)", kept, act);
    CHECK(PlanValid(plan, kept, 128), "b: every record at most once");
    {
        int has[PP_REC_MAX];
        memset(has, 0, sizeof has);
        for (k = 0; k < 32; ++k) if (plan[k] >= 0) has[plan[k]] = 1;
        for (i = 0; i < 128; ++i) {
            if (kind[i] == PPC_SWORD || kind[i] == PPC_GAME) CHECK(has[i], "b: the game's record %d is saved", i);
            if (kind[i] == PPC_OURS) CHECK(has[i] == (seq[i] >= 205), "b: drop %d (seq %d) saved %d (the 7 newest only)", i, seq[i], has[i]);
            if (kind[i] == PPC_FREE) CHECK(!has[i], "b: free %d not saved", i);
        }
        for (k = 0; k < 32; ++k)
            if (plan[k] >= 0 && plan[k] < 32) CHECK(plan[k] == k, "b: a low record keeps its slot (%d at %d)", plan[k], k);
        // the low slots not kept (free, or our old drops 22..29) take the high ones: Swords first, then game, then ours
        CHECK(plan[1] == 40 && plan[3] == 41 && plan[5] == 42 && plan[7] == 43, "b: the high Swords take the first free slots");
        CHECK(plan[9] == 50 && plan[11] == 51, "b: then the high game records");
        CHECK(plan[22] == 52 && plan[23] == 53 && plan[24] == 54, "b: freed slots of old drops reused (%d %d %d)", plan[22],
              plan[23], plan[24]);
        CHECK(plan[25] == 71 && plan[26] == 70 && plan[31] == 65, "b: our newest first (%d %d %d)", plan[25], plan[26], plan[31]);
    }
    // c: the game alone holds 40 (vanilla would have crashed): Swords, then the game's by slot; no drop of ours
    for (i = 0; i < 128; ++i) kind[i] = i < 30 ? PPC_GAME : i < 40 ? PPC_SWORD : i < 45 ? PPC_OURS : PPC_FREE, seq[i] = i;
    MakeViews(v, 128, kind, seq);
    kept = PpSavePlan(v, 128, plan, &act);
    CHECK(kept == 32 && act == 45 && PlanValid(plan, kept, 128), "c: 32 of 45");
    for (k = 0; k < 22; ++k) CHECK(plan[k] == k, "c: game %d (slot %d)", plan[k], k);
    for (k = 22; k < 30; ++k) CHECK(plan[k] == k + 10, "c: slot %d holds Sword %d (want %d)", k, plan[k], k + 10);
    CHECK(plan[30] == 30 && plan[31] == 31, "c: the low Swords keep their slots");
    {
        int sw = 0, ou = 0;
        for (k = 0; k < 32; ++k) sw += plan[k] >= 30 && plan[k] < 40, ou += plan[k] >= 40;
        CHECK(sw == 10 && ou == 0, "c: all 10 Swords, no drop (%d %d)", sw, ou);
    }
    // d: the stock pool (n = 32): the plan is the identity
    for (i = 0; i < 32; ++i) kind[i] = (i % 3) == 0 ? PPC_FREE : (i % 3) == 1 ? PPC_GAME : PPC_OURS, seq[i] = 31 - i;
    MakeViews(v, 32, kind, seq);
    kept = PpSavePlan(v, 32, plan, &act);
    for (k = 0; k < 32; ++k) CHECK(plan[k] == (kind[k] == PPC_FREE ? -1 : k), "d: identity at %d", k);
    CHECK(kept == act, "d: nothing left out");
    // e: the image bytes
    for (i = 0; i < 128; ++i) {
        memset(Rec(i), i + 1, PP_REC_SIZE);
        kind[i] = i >= 30 && i < 36 ? PPC_GAME : PPC_FREE;
    }
    MakeViews(v, 128, kind, NULL);
    kept = PpSavePlan(v, 128, plan, &act);
    memset(g_img, 0xEE, sizeof g_img);
    PpSaveApply(g_img, g_pool, plan);
    CHECK(kept == 6 && g_img[30 * PP_REC_SIZE] == 31 && g_img[31 * PP_REC_SIZE + 0x7F] == 32, "e: slots 30 / 31 copied");
    CHECK(g_img[0] == 33 && g_img[PP_REC_SIZE] == 34 && g_img[3 * PP_REC_SIZE + 5] == 36, "e: 32..35 into 0..3");
    CHECK(g_img[4 * PP_REC_SIZE] == 0 && g_img[29 * PP_REC_SIZE + 0x7F] == 0, "e: the rest zero");
    // usage
    for (i = 0; i < 128; ++i) kind[i] = i < 3 ? PPC_SWORD : i < 10 ? PPC_GAME : i < 14 ? PPC_OURS : PPC_FREE;
    MakeViews(v, 128, kind, NULL);
    PpUsage(v, 128, &game, &swords, &ours, &freeN);
    CHECK(game == 10 && swords == 3 && ours == 4 && freeN == 114, "usage %d %d %d %d", game, swords, ours, freeN);
}

static void TestLoad(void)
{
    int i;
    for (i = 0; i < (int)sizeof g_img; ++i) g_img[i] = (unsigned char)(i * 7 + 1);
    memset(g_pool, 0xAA, sizeof g_pool);
    PpLoadApply(g_pool, 128, g_img);
    CHECK(!memcmp(g_pool, g_img, sizeof g_img), "load: the image's 32 records in slots 0..31");
    for (i = 32 * PP_REC_SIZE; i < 128 * PP_REC_SIZE; ++i)
        if (g_pool[i]) break;
    CHECK(i == 128 * PP_REC_SIZE, "load: slots 32..127 cleared (byte %X)", i);
    CHECK(PpAllocPick(g_pool, 128, 1) == 32, "load: a drop then takes slot 32");
    memset(g_pool, 0xAA, sizeof g_pool);
    PpLoadApply(g_pool, 96, g_img);
    CHECK(g_pool[96 * PP_REC_SIZE] == 0xAA && g_pool[95 * PP_REC_SIZE + 0x7F] == 0, "load: n = 96 clears to 95 only");
    // round trip: 40 records, save, load: 32 come back, the 8 oldest drops do not, nothing duplicated
    {
        PpView v[PP_REC_MAX];
        int plan[PP_STOCK_N], act, kept, n = 0;
        memset(g_pool, 0, sizeof g_pool);
        for (i = 0; i < 40; ++i) {
            Rec(i * 3)[0] = 1;
            Rec(i * 3)[0x74] = (unsigned char)i;
            v[i * 3].state = 1, v[i * 3].sword = 0, v[i * 3].ours = 1, v[i * 3].seq = i;
        }
        for (i = 0; i < 128; ++i) if (i % 3 || i >= 120) v[i].state = v[i].sword = v[i].ours = v[i].seq = 0;
        kept = PpSavePlan(v, 128, plan, &act);
        PpSaveApply(g_img, g_pool, plan);
        PpLoadApply(g_pool, 128, g_img);
        for (i = 0; i < 128; ++i) n += Rec(i)[0] != 0;
        CHECK(kept == 32 && act == 40 && n == 32, "round trip: %d kept of %d, %d in the pool", kept, act, n);
        for (i = 0; i < 32; ++i) CHECK(!Rec(i)[0] || Rec(i)[0x74] >= 8, "round trip: slot %d holds drop %d (not one of the 8 oldest)", i, Rec(i)[0x74]);
    }
}

static void TestBuffer(void)
{
    CHECK(PpBufferOk(0x10012340u, 128), "a dll address");
    CHECK(!PpBufferOk(0x10000000u, 128), "0x10000000 (bit 28 alone) masks to 0: the mask is a real limit");
    CHECK(!PpBufferOk(0x0FFFFF80u, 2), "slot 1 = 0x10000000 masks to 0");
    CHECK(PpBufferOk(0x6A400000u, 128), "a high dll address");
    CHECK(!PpBufferOk(0x10000008u, 128), "not 16-aligned");
    CHECK(!PpBufferOk(0x80000000u, 1), "slot 0 masks to 0 (0x4CF252 would see NULL)");
    CHECK(!PpBufferOk(0xB0000000u, 4), "0xB0000000 masks to 0");
    CHECK(!PpBufferOk(0x7FFFF000u, 128), "crosses 0x80000000 (the signed jl)");
    CHECK(PpBufferOk(0x90001000u, 128), "above 0x80000000 without a crossing, mask fine");
    CHECK(!PpBufferOk(0x90000000u, 128), "0x90000000 masks to 0");
    CHECK(!PpBufferOk(0xFFFFF000u, 128), "wraps past 4 GB");
    CHECK(!PpBufferOk(0x10000000u, 0), "no records");
    CHECK(PpBufferOk(0x01077610u, 32), "the stock pool itself passes");
}

int main(void)
{
    TestPoolN();
    TestAlloc();
    TestFill();
    TestSave();
    TestLoad();
    TestBuffer();
    printf("pickpool_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
