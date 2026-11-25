// v3.36 HE336: the pure part of the larger hit-shape pool (hitpool.inc; native test test\hitpool_test.c).
// No game memory: shape blocks are passed in. GOG v7 layout (SH4Decomp include/hit.h, src/hit/hit_5613e0.c, matched):
// a HitShape is 0xD0 bytes; +0xC0 is its link word: the next free shape while it is free, HP_USED while a body holds it
// (0x561480 writes it, 0x5614B0 gives a shape back only when it holds HP_USED). The stock pool is 100 shapes at 0x12F1C30
// (D_012F1C30, to 0x12F6D70), its free head D_012F1C20; 0x561420 chains the 100 (the last link 0) and sets the head.
#ifndef HITPOOL_SIM_H
#define HITPOOL_SIM_H
#include <stddef.h>
#include <string.h>

#define HP_SHAPE_SIZE 0xD0
#define HP_LINK 0xC0
#define HP_USED 0x50765315u
#define HP_STOCK_BASE 0x012F1C30u
#define HP_STOCK_HEAD 0x012F1C20u
#define HP_STOCK_N 100
#define HP_EXTRA_MAX 512    // the largest block (512 x 0xD0 = 104 KB of bss)
#define HP_DEFAULT_N 356    // [Gameplay] HitPool (hidden) when the key is missing: 100 + 256

// the ini value -> the shapes in all: 0 .. 100 = the stock pool, 101 .. 612 as given, above = 612
static int HpPoolN(int ini)
{
    return ini <= HP_STOCK_N ? HP_STOCK_N : ini > HP_STOCK_N + HP_EXTRA_MAX ? HP_STOCK_N + HP_EXTRA_MAX : ini;
}

static unsigned char *HpLinkGet(const unsigned char *s)
{
    unsigned char *p;
    memcpy(&p, s + HP_LINK, sizeof p);
    return p;
}
static void HpLinkPut(unsigned char *s, const void *p) { memcpy(s + HP_LINK, &p, sizeof p); }

// Chains the n shapes of blk into one free list in address order, the last link NULL (what 0x561420 does with the
// game's 100). Returns blk, NULL when n < 1.
static unsigned char *HpChain(unsigned char *blk, int n)
{
    int i;
    if (!blk || n < 1) return NULL;
    for (i = 0; i + 1 < n; ++i) HpLinkPut(blk + i * HP_SHAPE_SIZE, blk + (i + 1) * HP_SHAPE_SIZE);
    HpLinkPut(blk + (n - 1) * HP_SHAPE_SIZE, NULL);
    return blk;
}

// 1 = p is the start of a shape of the stock pool (stock, stockN) or of the block (blk, blkN)
static int HpIsShape(size_t p, size_t stock, int stockN, size_t blk, int blkN)
{
    if (stockN > 0 && p >= stock && p < stock + (size_t)stockN * HP_SHAPE_SIZE) return (p - stock) % HP_SHAPE_SIZE == 0;
    if (blkN > 0 && p >= blk && p < blk + (size_t)blkN * HP_SHAPE_SIZE) return (p - blk) % HP_SHAPE_SIZE == 0;
    return 0;
}

// The free shapes: the list from head through +0xC0. -1 = the list is broken: a link that is no shape of the two
// ranges, or more nodes than the pool has (a cycle). The walk reads at most stockN + blkN + 1 nodes.
static int HpCountFree(const unsigned char *head, size_t stock, int stockN, size_t blk, int blkN)
{
    const int limit = stockN + (blkN > 0 ? blkN : 0);
    const unsigned char *p = head;
    int n = 0;
    while (p) {
        if (!HpIsShape((size_t)p, stock, stockN, blk, blkN) || n >= limit) return -1;
        ++n;
        p = HpLinkGet(p);
    }
    return n;
}

// The per-room numbers of the log line: the lowest free count seen on a tick of the room (-1 = no tick seen yet)
typedef struct { int lowest, refused, stomps, ticks; } HpRoom;
static void HpRoomStart(HpRoom *r) { r->lowest = -1, r->refused = r->stomps = r->ticks = 0; }
static void HpRoomSee(HpRoom *r, int freeN)
{
    if (freeN < 0) return;
    if (r->lowest < 0 || freeN < r->lowest) r->lowest = freeN;
    ++r->ticks;
}
#endif
