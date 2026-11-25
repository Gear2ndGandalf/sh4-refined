/* interp_pair.inc -- per-frame vertex-shader constant-upload pairing.
 *
 * Included by the host C file after it has defined: ST_* verdicts, Up, U_MAX, g_up, g_nu, g_np,
 * g_pend, g_cur, g_prevOk, Arena, g_arena. Everything here is static; own globals use g_pm / pm_.
 *
 * Design
 *  - HashCombine: one MurmurHash3-style block step (multiply / rotate / multiply, then fold into h
 *    with rotate + multiply-add) followed by the MurmurHash3 32-bit finaliser for full avalanche.
 *  - Occurrence table (one, per frame): open addressing over the exact tuple (sig, vs, reg, n),
 *    holding how many uploads with that tuple were seen so far this frame. That count is the
 *    occurrence index k of the next such upload (0, 1, 2 ...; capped at 63).
 *  - Pair tables (one per slot): open addressing keyed by the exact tuple (sig, vs, reg, n, k);
 *    the 32-bit key HashCombine(tupleHash, k) picks the start bucket and is compared first as a
 *    fast filter. Value = (off, n). Because the identity is stored in full, two different tuples
 *    can never be mistaken for each other even if their 32-bit keys collide.
 *  - All tables are linear-probed, power-of-two sized (2 * U_MAX buckets) and accept at most
 *    U_MAX live entries (load <= 50 %).
 *  - Reset is O(1): every bucket carries the generation it was written in; bumping a table's
 *    generation makes all its buckets empty. On generation wrap-around the buckets are wiped once.
 */

enum { W_OK, W_STILL, W_PAIRS, W_CUT };

#define PM_CAP      (2 * U_MAX)          /* buckets per table (power of two) */
#define PM_LIMIT    (PM_CAP / 2)         /* max live entries per table (50 % load) */
#define PM_KMAX     63                   /* highest occurrence index that is still paired */
#define PM_SEED     0x2F6B1D35u
#define PM_FLTMAX   3.402823466e+38f

typedef struct {
    DWORD gen;                /* generation this bucket was written in */
    DWORD key;                /* 32-bit key (tuple hash combined with k) */
    DWORD sig, vs, reg, off;
    WORD  n, k;
} PmPair;

typedef struct {
    DWORD gen;
    DWORD hash;
    DWORD sig, vs, reg;
    WORD  n, cnt;             /* cnt = uploads with this tuple seen this frame (<= PM_KMAX + 1) */
} PmOcc;

static PmPair g_pmPair[2][PM_CAP];
static DWORD  g_pmPairGen[2];
static int    g_pmPairFill[2];

static PmOcc  g_pmOcc[PM_CAP];
static DWORD  g_pmOccGen;
static int    g_pmOccFill;

/* per-frame counters (reset by PairBeginFrame) */
static int g_pmMatched;       /* uploads that found a partner */
static int g_pmMoved;         /* uploads classified ST_MOVE */
static int g_pmChg;           /* changed registers over all matched uploads */
static int g_pmRej;           /* jumped registers over all matched uploads */

static DWORD pm_rotl(DWORD x, int r)
{
    return (x << r) | (x >> (32 - r));
}

static DWORD HashCombine(DWORD h, DWORD v)
{
    v *= 0xCC9E2D51u;
    v = pm_rotl(v, 15);
    v *= 0x1B873593u;
    h ^= v;
    h = pm_rotl(h, 13);
    h = h * 5u + 0xE6546B64u;
    /* finaliser */
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

static DWORD pm_TupleHash(DWORD sig, DWORD vs, DWORD reg, DWORD n)
{
    DWORD h = HashCombine(PM_SEED, sig);
    h = HashCombine(h, vs);
    h = HashCombine(h, reg);
    return HashCombine(h, n);
}

/* Key of the k-th upload with tuple (sig, vs, reg, n) in a frame. */
static DWORD pm_Key(DWORD sig, DWORD vs, DWORD reg, DWORD n, DWORD k)
{
    return HashCombine(pm_TupleHash(sig, vs, reg, n), k);
}

static void PairBeginFrame(int slot)
{
    int i;
    slot &= 1;

    if (++g_pmPairGen[slot] == 0) {          /* wrapped: wipe once so stale gens cannot revive */
        for (i = 0; i < PM_CAP; i++) g_pmPair[slot][i].gen = 0;
        g_pmPairGen[slot] = 1;
    }
    g_pmPairFill[slot] = 0;

    if (++g_pmOccGen == 0) {
        for (i = 0; i < PM_CAP; i++) g_pmOcc[i].gen = 0;
        g_pmOccGen = 1;
    }
    g_pmOccFill = 0;

    g_pmMatched = 0;
    g_pmMoved = 0;
    g_pmChg = 0;
    g_pmRej = 0;
}

/* Returns the occurrence index for this tuple and advances the count, or -1 when the tuple is
   past the k cap or the occurrence table is full. */
static int pm_NextOcc(DWORD sig, DWORD vs, DWORD reg, WORD n)
{
    DWORD h = pm_TupleHash(sig, vs, reg, n);
    DWORD i = h & (PM_CAP - 1);
    int probes;
    PmOcc *e;

    for (probes = 0; probes < PM_CAP; probes++, i = (i + 1) & (PM_CAP - 1)) {
        e = &g_pmOcc[i];
        if (e->gen != g_pmOccGen) {
            if (g_pmOccFill >= PM_LIMIT) return -1;
            e->gen = g_pmOccGen;
            e->hash = h;
            e->sig = sig; e->vs = vs; e->reg = reg; e->n = n;
            e->cnt = 1;
            g_pmOccFill++;
            return 0;
        }
        if (e->hash == h && e->sig == sig && e->vs == vs && e->reg == reg && e->n == n) {
            if (e->cnt > PM_KMAX) return -1;
            return (int)e->cnt++;
        }
    }
    return -1;
}

static int pm_Insert(int slot, DWORD key, const Up *u, WORD k, DWORD sig)
{
    DWORD i = key & (PM_CAP - 1);
    DWORD gen = g_pmPairGen[slot];
    int probes;
    PmPair *e;

    if (g_pmPairFill[slot] >= PM_LIMIT) return 0;
    for (probes = 0; probes < PM_CAP; probes++, i = (i + 1) & (PM_CAP - 1)) {
        e = &g_pmPair[slot][i];
        if (e->gen != gen) {
            e->gen = gen;
            e->key = key;
            e->sig = sig; e->vs = u->vs; e->reg = u->reg; e->off = u->off;
            e->n = u->n; e->k = k;
            g_pmPairFill[slot]++;
            return 1;
        }
    }
    return 0;
}

static const PmPair *pm_Find(int slot, DWORD key, DWORD sig, DWORD vs, DWORD reg, WORD n, WORD k)
{
    DWORD i = key & (PM_CAP - 1);
    DWORD gen = g_pmPairGen[slot];
    int probes;
    const PmPair *e;

    for (probes = 0; probes < PM_CAP; probes++, i = (i + 1) & (PM_CAP - 1)) {
        e = &g_pmPair[slot][i];
        if (e->gen != gen) return 0;
        if (e->key == key && e->sig == sig && e->vs == vs && e->reg == reg && e->n == n && e->k == k)
            return e;
    }
    return 0;
}

static int Classify(const float *c, const float *p, int n, int *chg, int *rej)
{
    int r, l, nChg = 0, nRej = 0, bad;
    float a, b, d, diff, mag, t;

    for (r = 0; r < n; r++, c += 4, p += 4) {
        diff = 0.0f;
        mag = 0.0f;
        bad = 0;
        for (l = 0; l < 4; l++) {
            a = c[l];
            b = p[l];
            d = fabsf(a - b);
            /* !(x <= MAX) is true for NaN and for +/-infinity */
            if (!(fabsf(a) <= PM_FLTMAX) || !(fabsf(b) <= PM_FLTMAX) || !(d <= PM_FLTMAX)) {
                bad = 1;
                continue;
            }
            if (d > diff) diff = d;
            t = fabsf(a); if (t > mag) mag = t;
            t = fabsf(b); if (t > mag) mag = t;
        }
        if (bad) {
            nChg++;
            nRej++;
        } else if (diff > 0.0f) {
            nChg++;
            if (diff > 0.35f * mag + 1e-4f) nRej++;
        }
    }
    *chg += nChg;
    *rej += nRej;
    if (nRej) return ST_REJ;
    if (nChg) return ST_MOVE;
    return ST_SAME;
}

static void PairPending(DWORD sig)
{
    int i, k, cur = g_cur & 1, prev = cur ^ 1;
    DWORD key;
    Up *u;
    const PmPair *e;

    for (i = 0; i < g_np; i++) {
        if (g_pend[i] < 0 || g_pend[i] >= U_MAX) continue;
        u = &g_up[g_pend[i]];
        u->st = ST_NONE;

        k = pm_NextOcc(sig, u->vs, u->reg, u->n);
        if (k < 0) continue;

        key = pm_Key(sig, u->vs, u->reg, u->n, (DWORD)k);
        if (!pm_Insert(cur, key, u, (WORD)k, sig)) continue;

        if (!g_prevOk) continue;
        e = pm_Find(prev, key, sig, u->vs, u->reg, u->n, (WORD)k);
        if (!e || e->n != u->n) continue;

        g_pmMatched++;
        u->prevOff = e->off;
        u->st = (BYTE)Classify((const float *)(g_arena[cur].p + u->off),
                               (const float *)(g_arena[prev].p + e->off),
                               u->n, &g_pmChg, &g_pmRej);
        if (u->st == ST_MOVE) g_pmMoved++;
    }
    g_np = 0;
}

static int Decide(void)
{
    if (g_pmMoved == 0) return W_STILL;
    if (g_pmMatched * 2 < g_nu) return W_PAIRS;
    if (g_pmRej * 3 > g_pmChg) return W_CUT;
    return W_OK;
}
