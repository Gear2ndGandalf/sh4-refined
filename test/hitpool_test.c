// v3.36 HE336: native test of src\hitpool_sim.h (test\build_hitpool_test.cmd): the pool size from the ini, the block's
// chain after the game's 100 (as 0x561420 + hkHpChain build it), the alloc / free of the matched 0x561480 / 0x5614B0 over
// both, a room change's relink without a clear, the free-list walk (and its refusal of a broken list), the shape test.
#include <stdio.h>
#include <string.h>
#include "../src/hitpool_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static __declspec(align(16)) unsigned char g_stock[HP_STOCK_N * HP_SHAPE_SIZE];
static __declspec(align(16)) unsigned char g_blk[HP_EXTRA_MAX * HP_SHAPE_SIZE];
static unsigned char *g_head;  // D_012F1C20
static int g_extra;

// 0x561420 (matched C, hit_5613e0.c) on the test's stock array
static void StockChain(void)
{
    unsigned char *s = g_stock;
    int i;
    for (i = 0; i < 99; ++i) {
        HpLinkPut(s, s + HP_SHAPE_SIZE);
        s += HP_SHAPE_SIZE;
    }
    HpLinkPut(s, NULL);
    g_head = g_stock;
}
// hitpool.inc HpChainC after it (clear = HitInit's path)
static void Build(int clear)
{
    if (clear) memset(g_stock, 0, sizeof g_stock);
    StockChain();
    if (g_extra > 0) {
        if (clear) memset(g_blk, 0, (size_t)g_extra * HP_SHAPE_SIZE);
        HpChain(g_blk, g_extra);
        HpLinkPut(g_stock + (HP_STOCK_N - 1) * HP_SHAPE_SIZE, g_blk);
    }
}
// 0x561480 / 0x5614B0 (matched C)
static unsigned char *Alloc(void)
{
    unsigned char *s = g_head;
    if (s) {
        unsigned v = HP_USED;
        g_head = HpLinkGet(s);
        memcpy(s + HP_LINK, &v, 4);
    }
    return s;
}
static void Free(unsigned char *s)
{
    unsigned v;
    memcpy(&v, s + HP_LINK, 4);
    if (v == HP_USED) {
        HpLinkPut(s, g_head);
        g_head = s;
    }
}
static int Count(void) { return HpCountFree(g_head, (size_t)g_stock, HP_STOCK_N, (size_t)g_blk, g_extra); }
static int InStock(const unsigned char *s) { return s >= g_stock && s < g_stock + sizeof g_stock; }

static void TestPoolN(void)
{
    CHECK(HpPoolN(0) == 100, "0 = the stock pool");
    CHECK(HpPoolN(-1) == 100, "negative = the stock pool");
    CHECK(HpPoolN(100) == 100, "100 = the stock pool");
    CHECK(HpPoolN(101) == 101, "101");
    CHECK(HpPoolN(228) == 228, "228");
    CHECK(HpPoolN(HP_DEFAULT_N) == 356, "the default: 356");
    CHECK(HpPoolN(612) == 612, "612 = the largest");
    CHECK(HpPoolN(100000) == 612, "clamped to the block");
    CHECK(HP_STOCK_BASE + 100 * HP_SHAPE_SIZE == 0x12F6D70u, "the stock pool ends at 0x12F6D70 (the next variable)");
    CHECK(HP_STOCK_BASE + 99 * HP_SHAPE_SIZE + HP_LINK == 0x12F6D60u, "the last stock link at 0x12F6D60");
    CHECK(HP_SHAPE_SIZE % 16 == 0 && HP_STOCK_BASE % 16 == 0, "every shape stays 16-aligned");
}

static void TestStock(void)
{
    int i;
    g_extra = 0;
    Build(1);
    CHECK(Count() == 100, "stock: 100 free after the build (%d)", Count());
    for (i = 0; i < 100; ++i) CHECK(Alloc() != NULL, "stock alloc %d", i);
    CHECK(Alloc() == NULL, "stock: the 101st is NULL (the empty pool)");
    CHECK(Count() == 0, "stock: 0 free");
    CHECK(HpChain(g_blk, 0) == NULL, "no block: nothing chained");
}

static void TestBlock(int extra)
{
    unsigned char *got[HP_STOCK_N + HP_EXTRA_MAX + 1];
    int i, total = HP_STOCK_N + extra, stockFirst = 1, blockAfter = 1;
    g_extra = extra;
    Build(1);
    CHECK(Count() == total, "%d: all free after the build (%d)", total, Count());
    for (i = 0; i < total; ++i) {
        got[i] = Alloc();
        if (!got[i]) break;
        if (i < HP_STOCK_N && !InStock(got[i])) stockFirst = 0;
        if (i >= HP_STOCK_N && InStock(got[i])) blockAfter = 0;
    }
    CHECK(i == total, "%d: %d allocs before NULL", total, i);
    CHECK(stockFirst, "%d: the game's 100 go first (stock play unchanged)", total);
    CHECK(blockAfter, "%d: then the block", total);
    CHECK(Alloc() == NULL && Count() == 0, "%d: then empty", total);
    Free(got[total - 1]);
    Free(got[7]);
    CHECK(Count() == 2, "%d: two given back", total);
    CHECK(Alloc() == got[7] && Alloc() == got[total - 1], "%d: LIFO over both ranges", total);
    Free(got[5]);
    Free(got[5]);  // a second free of the same shape: not tagged any more, ignored (0x5614B0)
    CHECK(Count() == 1, "%d: a double free is ignored", total);
    // a room change: every shape free again, re-chained without a clear (0x4F0A55)
    memset(got[HP_STOCK_N] + 0x10, 0x5A, 16);
    Build(0);
    CHECK(Count() == total, "%d: a room change frees all (%d)", total, Count());
    CHECK(got[HP_STOCK_N][0x10] == 0x5A, "%d: a room change does not clear the block (as the game's 100)", total);
    Build(1);
    CHECK(got[HP_STOCK_N][0x10] == 0, "%d: HitInit's clear clears the block too", total);
    for (i = 0; i < total; ++i)
        if (!HpIsShape((size_t)got[i], (size_t)g_stock, HP_STOCK_N, (size_t)g_blk, g_extra)) break;
    CHECK(i == total, "%d: every shape handed out passes HpIsShape", total);
}

static void TestIsShape(void)
{
    const size_t s = (size_t)g_stock, b = (size_t)g_blk;
    g_extra = 256;
    CHECK(HpIsShape(s, s, 100, b, 256), "the first stock shape");
    CHECK(HpIsShape(s + 99 * HP_SHAPE_SIZE, s, 100, b, 256), "the last stock shape");
    CHECK(!HpIsShape(s + 100 * HP_SHAPE_SIZE, s, 100, 0, 0), "past the stock pool (no block)");
    CHECK(!HpIsShape(s + 8, s, 100, b, 256), "inside a shape");
    CHECK(!HpIsShape(s - HP_SHAPE_SIZE, s, 100, b, 256), "before the pool");
    CHECK(HpIsShape(b, s, 100, b, 256) && HpIsShape(b + 255 * HP_SHAPE_SIZE, s, 100, b, 256), "the block's first and last");
    CHECK(!HpIsShape(b + 256 * HP_SHAPE_SIZE, s, 100, b, 256), "past the block");
    CHECK(!HpIsShape(b, s, 100, b, 0), "the block when it is not linked (HitPool=0)");
    CHECK(!HpIsShape(b + 0x68, s, 100, b, 256), "inside a block shape");
    CHECK(HpIsShape(0x12F1C30u + 5 * 0xD0, 0x12F1C30u, 100, 0, 0) && !HpIsShape(0x12F6D70u, 0x12F1C30u, 100, 0, 0),
          "the game's addresses (echo.inc's old range test)");
}

static void TestWalk(void)
{
    unsigned char *a, *b;
    g_extra = 10;
    Build(1);
    CHECK(Count() == 110, "110 free");
    a = Alloc(), b = Alloc();
    (void)a;
    HpLinkPut(b, g_head);  // a loop: a free shape pointing back into the list
    g_head = b;
    HpLinkPut(g_stock + 50 * HP_SHAPE_SIZE, b);
    CHECK(Count() == -1, "a loop is refused (the walk stops at the pool's size)");
    Build(1);
    HpLinkPut(g_stock + 20 * HP_SHAPE_SIZE, g_stock + 3);  // a link into the middle of a shape
    CHECK(Count() == -1, "a link that is no shape is refused");
    Build(1);
    HpLinkPut(g_blk + 9 * HP_SHAPE_SIZE, g_blk + 10 * HP_SHAPE_SIZE);  // past the linked block
    CHECK(Count() == -1, "a link past the block is refused");
    g_head = NULL;
    CHECK(Count() == 0, "an empty list: 0");
}

static void TestRoom(void)
{
    HpRoom r;
    HpRoomStart(&r);
    CHECK(r.lowest == -1 && r.ticks == 0, "a room starts with no tick");
    HpRoomSee(&r, -1);
    CHECK(r.lowest == -1 && r.ticks == 0, "a broken walk is not a tick");
    HpRoomSee(&r, 80), HpRoomSee(&r, 12), HpRoomSee(&r, 40);
    CHECK(r.lowest == 12 && r.ticks == 3, "the lowest free of the room");
}

int main(void)
{
    TestPoolN();
    TestStock();
    TestBlock(1);
    TestBlock(128);
    TestBlock(256);
    TestBlock(HP_EXTRA_MAX);
    TestIsShape();
    TestWalk();
    TestRoom();
    printf("hitpool_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
