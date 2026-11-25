/* test_pair.c -- tests for interp_pair.inc.  Build: cl /W4 /nologo test_pair.c */
#include <stdio.h>

/* ---- host shim (as given by the spec) ---- */
#include <windows.h>
#include <math.h>
enum { ST_NONE, ST_SAME, ST_MOVE, ST_REJ };
typedef struct { DWORD off, prevOff, reg, vs; WORD n; BYTE st, pad; } Up;
#define U_MAX 8192
static Up g_up[U_MAX];
static int g_nu;
static int g_np;
static int g_pend[256];
static int g_cur;
static int g_prevOk;
typedef struct { BYTE *p; DWORD n, cap; } Arena;
static Arena g_arena[2];

#include "interp_pair.inc"

/* compile-time: table holds U_MAX entries at <= 50 % load */
typedef char t_capCheck[(PM_LIMIT >= U_MAX && PM_LIMIT * 2 <= PM_CAP) ? 1 : -1];

/* ---- test harness ---- */
static int t_fail, t_pass;
#define CHECK(c) do { if (c) t_pass++; else { t_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define ARENA_BYTES (1u << 20)
static BYTE t_mem[2][ARENA_BYTES];

static void T_Frame(int slot, int prevOk)
{
    g_cur = slot;
    g_prevOk = prevOk;
    g_nu = 0;
    g_np = 0;
    g_arena[slot].p = t_mem[slot];
    g_arena[slot].n = 0;
    g_arena[slot].cap = ARENA_BYTES;
    PairBeginFrame(slot);
}

/* Record one upload of n registers; every lane gets value v (+ lane*0, kept simple). */
static int T_Rec(DWORD vs, DWORD reg, WORD n, float v)
{
    Arena *a = &g_arena[g_cur];
    float *f = (float *)(a->p + a->n);
    int i, idx = g_nu++;
    for (i = 0; i < n * 4; i++) f[i] = v;
    g_up[idx].off = a->n;
    g_up[idx].prevOff = 0xFFFFFFFFu;
    g_up[idx].reg = reg;
    g_up[idx].vs = vs;
    g_up[idx].n = n;
    g_up[idx].st = ST_NONE;
    a->n += (DWORD)n * 16u;
    g_pend[g_np++] = idx;
    return idx;
}

static void T_Pad(DWORD bytes) { g_arena[g_cur].n += bytes; }

static float t_vz = 0.0f, t_vbig = 1e38f;   /* non-constant so no compile-time folding */
static float T_NaN(void) { volatile float z = t_vz; return z / z; }
static float T_Inf(void) { volatile float b = t_vbig; return b * b; }

static void Fill4(float *f, float a, float b, float c, float d) { f[0] = a; f[1] = b; f[2] = c; f[3] = d; }

/* ---------------------------------------------------------------- */

static void TestHash(void)
{
    DWORD a = HashCombine(0, 0), b = HashCombine(0, 1), c = HashCombine(1, 0);
    int bits, i, total = 0;
    DWORD x;
    CHECK(a != b && a != c && b != c);
    CHECK(HashCombine(123, 456) == HashCombine(123, 456));
    /* rough avalanche: flipping one input bit should flip ~16 output bits on average */
    for (i = 0; i < 32; i++) {
        x = HashCombine(0x12345678u, 0x9ABCDEF0u) ^ HashCombine(0x12345678u, 0x9ABCDEF0u ^ (1u << i));
        for (bits = 0; x; x &= x - 1) bits++;
        total += bits;
    }
    CHECK(total > 32 * 12 && total < 32 * 20);
}

static void TestKeys(void)
{
    DWORD keys[64];
    int i, j, distinct = 1;
    for (i = 0; i < 64; i++) keys[i] = pm_Key(7, 0x100, 4, 2, (DWORD)i);
    for (i = 0; i < 64; i++)
        for (j = i + 1; j < 64; j++)
            if (keys[i] == keys[j]) distinct = 0;
    CHECK(distinct);
    for (i = 0; i < 64; i++) CHECK(keys[i] == pm_Key(7, 0x100, 4, 2, (DWORD)i));
    CHECK(pm_Key(7, 0x100, 4, 2, 0) != pm_Key(7, 0x100, 4, 3, 0));
    CHECK(pm_Key(7, 0x100, 4, 2, 0) != pm_Key(8, 0x100, 4, 2, 0));
    CHECK(pm_Key(7, 0x100, 4, 2, 0) != pm_Key(7, 0x101, 4, 2, 0));
    CHECK(pm_Key(7, 0x100, 4, 2, 0) != pm_Key(7, 0x100, 5, 2, 0));
}

/* Repeated identical tuples pair in recording order across frames. */
static void TestRepeatOrder(void)
{
    int a0, a1, a2, b0, b1, b2;
    DWORD o0, o1, o2;

    T_Frame(0, 0);
    a0 = T_Rec(0x10, 8, 1, 1.0f);
    a1 = T_Rec(0x10, 8, 1, 2.0f);
    PairPending(0xAAAA);
    a2 = T_Rec(0x10, 8, 1, 3.0f);
    PairPending(0xAAAA);
    CHECK(g_np == 0);
    CHECK(g_up[a0].st == ST_NONE && g_up[a1].st == ST_NONE && g_up[a2].st == ST_NONE); /* no prev */
    o0 = g_up[a0].off; o1 = g_up[a1].off; o2 = g_up[a2].off;
    CHECK(g_pmPairFill[0] == 3);

    T_Frame(1, 1);
    T_Pad(48);                       /* offsets differ from the previous frame */
    b0 = T_Rec(0x10, 8, 1, 1.0f);
    b1 = T_Rec(0x10, 8, 1, 2.0f);
    b2 = T_Rec(0x10, 8, 1, 3.0f);
    PairPending(0xAAAA);
    CHECK(g_up[b0].prevOff == o0 && g_up[b0].st == ST_SAME);
    CHECK(g_up[b1].prevOff == o1 && g_up[b1].st == ST_SAME);
    CHECK(g_up[b2].prevOff == o2 && g_up[b2].st == ST_SAME);
    CHECK(g_pmMatched == 3 && g_pmMoved == 0);
    CHECK(Decide() == W_STILL);

    /* a fourth occurrence has no partner */
    b0 = T_Rec(0x10, 8, 1, 3.0f);
    PairPending(0xAAAA);
    CHECK(g_up[b0].st == ST_NONE);
    CHECK(g_pmMatched == 3);
    /* same tuple under a different draw signature does not pair */
    b0 = T_Rec(0x10, 8, 1, 1.0f);
    PairPending(0xBBBB);
    CHECK(g_up[b0].st == ST_NONE);
}

/* n mismatch never pairs. */
static void TestNMismatch(void)
{
    int a, b, c;
    T_Frame(0, 0);
    T_Rec(0x20, 0, 2, 1.0f);
    PairPending(1);
    T_Frame(1, 1);
    a = T_Rec(0x20, 0, 3, 1.0f);
    b = T_Rec(0x20, 0, 1, 1.0f);
    PairPending(1);
    CHECK(g_up[a].st == ST_NONE && g_up[b].st == ST_NONE);
    CHECK(g_pmMatched == 0);
    c = T_Rec(0x20, 0, 2, 1.0f);    /* the right n still pairs */
    PairPending(1);
    CHECK(g_up[c].st == ST_SAME && g_pmMatched == 1);
}

/* Occurrence index is capped at 63. */
static void TestKCap(void)
{
    int i, idx[70], paired = 0, firstUnpaired = -1;
    T_Frame(0, 0);
    for (i = 0; i < 70; i++) T_Rec(0x30, 1, 1, (float)i);
    PairPending(5);
    CHECK(g_pmPairFill[0] == 64);
    T_Frame(1, 1);
    for (i = 0; i < 70; i++) idx[i] = T_Rec(0x30, 1, 1, (float)i);
    PairPending(5);
    for (i = 0; i < 70; i++) {
        if (g_up[idx[i]].st != ST_NONE) paired++;
        else if (firstUnpaired < 0) firstUnpaired = i;
    }
    CHECK(paired == 64 && firstUnpaired == 64);
}

/* Load cap: the table accepts exactly PM_LIMIT (= U_MAX) entries per frame. */
static void LoadDraw(DWORD sig, int count, int *nonePaired, int *paired)
{
    int i;
    g_nu = 0;                         /* reuse g_up slots; results are read right after */
    for (i = 0; i < count; i++) T_Rec(0x40, (DWORD)i, 1, 1.0f);
    PairPending(sig);
    for (i = 0; i < count; i++) {
        if (g_up[i].st == ST_NONE) (*nonePaired)++;
        else (*paired)++;
    }
}

static void TestLoadCap(void)
{
    int d, none = 0, paired = 0;
    /* capacity relation checked at compile time by t_capCheck */

    T_Frame(0, 0);
    for (d = 0; d < U_MAX / 256; d++) LoadDraw((DWORD)d, 256, &none, &paired);
    CHECK(g_pmPairFill[0] == U_MAX);
    LoadDraw(999, 4, &none, &paired);            /* table is full: rejected silently */
    CHECK(g_pmPairFill[0] == U_MAX);

    T_Frame(1, 1);
    none = paired = 0;
    LoadDraw(999, 1, &none, &paired);            /* new tuple first: uses one bucket, no partner */
    CHECK(none == 1 && paired == 0);
    none = paired = 0;
    for (d = 0; d < U_MAX / 256; d++) LoadDraw((DWORD)d, 256, &none, &paired);
    CHECK(paired == U_MAX - 1 && none == 1);     /* last one did not fit */
    CHECK(g_up[255].st == ST_NONE);
    CHECK(g_pmPairFill[1] == U_MAX);
    CHECK(g_pmMatched == U_MAX - 1);

    /* after a reset the full table is usable again */
    T_Frame(0, 1);
    none = paired = 0;
    LoadDraw(0, 256, &none, &paired);
    CHECK(paired == 256 && none == 0 && g_pmPairFill[0] == 256);
}

/* Generation reset forgets the slot; wrap-around also works. */
static void TestGeneration(void)
{
    int a, i, live;
    DWORD key;

    T_Frame(0, 0);
    T_Rec(0x50, 3, 1, 1.0f);
    PairPending(77);
    key = pm_Key(77, 0x50, 3, 1, 0);
    CHECK(pm_Find(0, key, 77, 0x50, 3, 1, 0) != 0);

    T_Frame(1, 1);                   /* records something else in slot 1 */
    T_Rec(0x51, 3, 1, 1.0f);
    PairPending(78);
    CHECK(pm_Find(0, key, 77, 0x50, 3, 1, 0) != 0);   /* slot 0 untouched by slot 1's reset */

    T_Frame(0, 1);                   /* slot 0 reset */
    CHECK(pm_Find(0, key, 77, 0x50, 3, 1, 0) == 0);
    CHECK(g_pmPairFill[0] == 0 && g_pmOccFill == 0);
    a = T_Rec(0x50, 3, 1, 1.0f);     /* prev (slot 1) never had this tuple */
    PairPending(77);
    CHECK(g_up[a].st == ST_NONE);

    /* counters reset */
    CHECK(g_pmMatched == 0 && g_pmMoved == 0 && g_pmChg == 0 && g_pmRej == 0);

    /* generation wrap-around: stale buckets must not revive */
    g_pmPairGen[0] = 0xFFFFFFFFu;
    g_pmOccGen = 0xFFFFFFFFu;
    T_Frame(0, 0);
    T_Rec(0x60, 0, 1, 1.0f);
    PairPending(1);
    CHECK(g_pmPairGen[0] == 1);
    for (live = 0, i = 0; i < PM_CAP; i++) if (g_pmPair[0][i].gen == g_pmPairGen[0]) live++;
    CHECK(live == 1);
    CHECK(pm_Find(0, key, 77, 0x50, 3, 1, 0) == 0);
}

static void TestClassify(void)
{
    float c[16], p[16];
    int chg, rej, st;

    /* no change */
    Fill4(c, 1, 2, 3, 4); Fill4(p, 1, 2, 3, 4);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_SAME && chg == 0 && rej == 0);
    /* -0 vs +0 is not a change */
    Fill4(c, -0.0f, 0, 0, 0); Fill4(p, 0, 0, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_SAME && chg == 0);

    /* small move */
    Fill4(c, 1.05f, 2, 3, 4); Fill4(p, 1, 2, 3, 4);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_MOVE && chg == 1 && rej == 0);

    /* 0 -> 1 is a jump */
    Fill4(c, 1, 0, 0, 0); Fill4(p, 0, 0, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);

    /* mag uses all lanes: 0->1 next to a large lane is a move */
    Fill4(c, 1, 10, 0, 0); Fill4(p, 0, 10, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_MOVE && chg == 1 && rej == 0);

    /* 1e-4 floor for tiny values */
    Fill4(c, 5e-5f, 0, 0, 0); Fill4(p, 0, 0, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_MOVE && chg == 1 && rej == 0);
    Fill4(c, 2e-4f, 0, 0, 0); Fill4(p, 0, 0, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);

    /* NaN in current, infinity in previous, overflowing difference */
    Fill4(c, 1, T_NaN(), 3, 4); Fill4(p, 1, 2, 3, 4);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);
    Fill4(c, T_NaN(), 2, 3, 4); Fill4(p, T_NaN(), 2, 3, 4);   /* NaN both sides still a jump */
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);
    Fill4(c, 1, 2, 3, 4); Fill4(p, 1, 2, 3, T_Inf());
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);
    Fill4(c, 3e38f, 0, 0, 0); Fill4(p, -3e38f, 0, 0, 0);
    chg = rej = 0; st = Classify(c, p, 1, &chg, &rej);
    CHECK(st == ST_REJ && chg == 1 && rej == 1);

    /* several registers: same, move, jump; counts accumulate into existing totals */
    Fill4(c + 0, 1, 1, 1, 1); Fill4(p + 0, 1, 1, 1, 1);
    Fill4(c + 4, 1.1f, 1, 1, 1); Fill4(p + 4, 1, 1, 1, 1);
    Fill4(c + 8, 5, 0, 0, 0); Fill4(p + 8, -5, 0, 0, 0);
    Fill4(c + 12, 2, 2, 2, 2); Fill4(p + 12, 2, 2, 2, 2.01f);
    chg = 10; rej = 20;
    st = Classify(c, p, 4, &chg, &rej);
    CHECK(st == ST_REJ && chg == 13 && rej == 21);
    chg = rej = 0;
    st = Classify(c, p, 2, &chg, &rej);
    CHECK(st == ST_MOVE && chg == 1 && rej == 0);
}

/* Builds prev frame (slot 0) then current frame (slot 1); returns Decide(). */
static int DecideCase(int nSame, int nMove, int nJump, int nNew)
{
    int i;
    T_Frame(0, 0);
    for (i = 0; i < nSame; i++) T_Rec(1, (DWORD)i, 1, 1.0f);
    for (i = 0; i < nMove; i++) T_Rec(2, (DWORD)i, 1, 1.0f);
    for (i = 0; i < nJump; i++) T_Rec(3, (DWORD)i, 1, 0.0f);
    PairPending(42);
    T_Frame(1, 1);
    for (i = 0; i < nSame; i++) T_Rec(1, (DWORD)i, 1, 1.0f);
    for (i = 0; i < nMove; i++) T_Rec(2, (DWORD)i, 1, 1.01f);
    for (i = 0; i < nJump; i++) T_Rec(3, (DWORD)i, 1, 1.0f);
    for (i = 0; i < nNew; i++) T_Rec(4, (DWORD)i, 1, 1.0f);
    PairPending(42);
    return Decide();
}

static void TestDecide(void)
{
    /* nothing moved -> STILL (even with jumps and unpaired uploads) */
    CHECK(DecideCase(5, 0, 0, 0) == W_STILL);
    CHECK(DecideCase(5, 0, 3, 20) == W_STILL);
    /* moved, but fewer than half the uploads paired -> PAIRS */
    CHECK(DecideCase(1, 2, 0, 4) == W_PAIRS);
    CHECK(g_pmMatched == 3 && g_nu == 7 && g_pmMoved == 2);
    /* exactly half paired is not PAIRS */
    CHECK(DecideCase(1, 2, 0, 3) == W_OK);
    /* jumped*3 > changed -> CUT  (2 moves + 1 jump: 1*3 > 3 false -> OK; 2 moves + 2 jumps: 6 > 4 -> CUT) */
    CHECK(DecideCase(0, 2, 1, 0) == W_OK);
    CHECK(g_pmChg == 3 && g_pmRej == 1);
    CHECK(DecideCase(0, 2, 2, 0) == W_CUT);
    CHECK(g_pmChg == 4 && g_pmRej == 2);
    /* plain OK */
    CHECK(DecideCase(10, 10, 0, 0) == W_OK);
}

int main(void)
{
    TestHash();
    TestKeys();
    TestRepeatOrder();
    TestNMismatch();
    TestKCap();
    TestLoadCap();
    TestGeneration();
    TestClassify();
    TestDecide();
    printf("%d passed, %d failed\n", t_pass, t_fail);
    return t_fail ? 1 : 0;
}
