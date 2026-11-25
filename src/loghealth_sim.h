// v3.33 LOG333: the pure part of the gameplay health log (loghealth.inc): no Windows, no game reads, so the native test
// test\loghealth_test.c builds it alone. MD5 (the session header names this dll and the exe by their md5), the frame-time
// statistics of a room (mean, percentile from a 0.25 ms histogram, worst), the frame classes (normal / spike / stall:
// thresholds that hold at 30, 60, 120 and 180 fps), a per-window rate limit, the "row on but never acted" streak, the
// Supply Drops zero-drop bound, the flicker counter, the address-space margin steps and the ini default compare.
#ifndef LOGHEALTH_SIM_H
#define LOGHEALTH_SIM_H

#include <string.h>

typedef unsigned int LhU32;

// ---------------------------------------------------------------- MD5 (RFC 1321)
typedef struct { LhU32 s[4]; LhU32 lo, hi; unsigned char buf[64]; } LhMd5;

#define LH_F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define LH_G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define LH_H(x, y, z) ((x) ^ (y) ^ (z))
#define LH_I(x, y, z) ((y) ^ ((x) | ~(z)))
#define LH_STEP(f, a, b, c, d, x, t, s) \
    (a) += f((b), (c), (d)) + (x) + (t); \
    (a) = (((a) << (s)) | (((a) & 0xffffffffu) >> (32 - (s)))); \
    (a) += (b);

static void LhMd5Block(LhU32 st[4], const unsigned char *p)
{
    LhU32 a = st[0], b = st[1], c = st[2], d = st[3], x[16];
    int i;
    for (i = 0; i < 16; ++i)
        x[i] = (LhU32)p[i * 4] | (LhU32)p[i * 4 + 1] << 8 | (LhU32)p[i * 4 + 2] << 16 | (LhU32)p[i * 4 + 3] << 24;
    LH_STEP(LH_F, a, b, c, d, x[0], 0xd76aa478, 7) LH_STEP(LH_F, d, a, b, c, x[1], 0xe8c7b756, 12)
    LH_STEP(LH_F, c, d, a, b, x[2], 0x242070db, 17) LH_STEP(LH_F, b, c, d, a, x[3], 0xc1bdceee, 22)
    LH_STEP(LH_F, a, b, c, d, x[4], 0xf57c0faf, 7) LH_STEP(LH_F, d, a, b, c, x[5], 0x4787c62a, 12)
    LH_STEP(LH_F, c, d, a, b, x[6], 0xa8304613, 17) LH_STEP(LH_F, b, c, d, a, x[7], 0xfd469501, 22)
    LH_STEP(LH_F, a, b, c, d, x[8], 0x698098d8, 7) LH_STEP(LH_F, d, a, b, c, x[9], 0x8b44f7af, 12)
    LH_STEP(LH_F, c, d, a, b, x[10], 0xffff5bb1, 17) LH_STEP(LH_F, b, c, d, a, x[11], 0x895cd7be, 22)
    LH_STEP(LH_F, a, b, c, d, x[12], 0x6b901122, 7) LH_STEP(LH_F, d, a, b, c, x[13], 0xfd987193, 12)
    LH_STEP(LH_F, c, d, a, b, x[14], 0xa679438e, 17) LH_STEP(LH_F, b, c, d, a, x[15], 0x49b40821, 22)
    LH_STEP(LH_G, a, b, c, d, x[1], 0xf61e2562, 5) LH_STEP(LH_G, d, a, b, c, x[6], 0xc040b340, 9)
    LH_STEP(LH_G, c, d, a, b, x[11], 0x265e5a51, 14) LH_STEP(LH_G, b, c, d, a, x[0], 0xe9b6c7aa, 20)
    LH_STEP(LH_G, a, b, c, d, x[5], 0xd62f105d, 5) LH_STEP(LH_G, d, a, b, c, x[10], 0x02441453, 9)
    LH_STEP(LH_G, c, d, a, b, x[15], 0xd8a1e681, 14) LH_STEP(LH_G, b, c, d, a, x[4], 0xe7d3fbc8, 20)
    LH_STEP(LH_G, a, b, c, d, x[9], 0x21e1cde6, 5) LH_STEP(LH_G, d, a, b, c, x[14], 0xc33707d6, 9)
    LH_STEP(LH_G, c, d, a, b, x[3], 0xf4d50d87, 14) LH_STEP(LH_G, b, c, d, a, x[8], 0x455a14ed, 20)
    LH_STEP(LH_G, a, b, c, d, x[13], 0xa9e3e905, 5) LH_STEP(LH_G, d, a, b, c, x[2], 0xfcefa3f8, 9)
    LH_STEP(LH_G, c, d, a, b, x[7], 0x676f02d9, 14) LH_STEP(LH_G, b, c, d, a, x[12], 0x8d2a4c8a, 20)
    LH_STEP(LH_H, a, b, c, d, x[5], 0xfffa3942, 4) LH_STEP(LH_H, d, a, b, c, x[8], 0x8771f681, 11)
    LH_STEP(LH_H, c, d, a, b, x[11], 0x6d9d6122, 16) LH_STEP(LH_H, b, c, d, a, x[14], 0xfde5380c, 23)
    LH_STEP(LH_H, a, b, c, d, x[1], 0xa4beea44, 4) LH_STEP(LH_H, d, a, b, c, x[4], 0x4bdecfa9, 11)
    LH_STEP(LH_H, c, d, a, b, x[7], 0xf6bb4b60, 16) LH_STEP(LH_H, b, c, d, a, x[10], 0xbebfbc70, 23)
    LH_STEP(LH_H, a, b, c, d, x[13], 0x289b7ec6, 4) LH_STEP(LH_H, d, a, b, c, x[0], 0xeaa127fa, 11)
    LH_STEP(LH_H, c, d, a, b, x[3], 0xd4ef3085, 16) LH_STEP(LH_H, b, c, d, a, x[6], 0x04881d05, 23)
    LH_STEP(LH_H, a, b, c, d, x[9], 0xd9d4d039, 4) LH_STEP(LH_H, d, a, b, c, x[12], 0xe6db99e5, 11)
    LH_STEP(LH_H, c, d, a, b, x[15], 0x1fa27cf8, 16) LH_STEP(LH_H, b, c, d, a, x[2], 0xc4ac5665, 23)
    LH_STEP(LH_I, a, b, c, d, x[0], 0xf4292244, 6) LH_STEP(LH_I, d, a, b, c, x[7], 0x432aff97, 10)
    LH_STEP(LH_I, c, d, a, b, x[14], 0xab9423a7, 15) LH_STEP(LH_I, b, c, d, a, x[5], 0xfc93a039, 21)
    LH_STEP(LH_I, a, b, c, d, x[12], 0x655b59c3, 6) LH_STEP(LH_I, d, a, b, c, x[3], 0x8f0ccc92, 10)
    LH_STEP(LH_I, c, d, a, b, x[10], 0xffeff47d, 15) LH_STEP(LH_I, b, c, d, a, x[1], 0x85845dd1, 21)
    LH_STEP(LH_I, a, b, c, d, x[8], 0x6fa87e4f, 6) LH_STEP(LH_I, d, a, b, c, x[15], 0xfe2ce6e0, 10)
    LH_STEP(LH_I, c, d, a, b, x[6], 0xa3014314, 15) LH_STEP(LH_I, b, c, d, a, x[13], 0x4e0811a1, 21)
    LH_STEP(LH_I, a, b, c, d, x[4], 0xf7537e82, 6) LH_STEP(LH_I, d, a, b, c, x[11], 0xbd3af235, 10)
    LH_STEP(LH_I, c, d, a, b, x[2], 0x2ad7d2bb, 15) LH_STEP(LH_I, b, c, d, a, x[9], 0xeb86d391, 21)
    st[0] += a, st[1] += b, st[2] += c, st[3] += d;
}

static void LhMd5Init(LhMd5 *m)
{
    m->s[0] = 0x67452301u, m->s[1] = 0xefcdab89u, m->s[2] = 0x98badcfeu, m->s[3] = 0x10325476u;
    m->lo = m->hi = 0;
}

static void LhMd5Update(LhMd5 *m, const void *data, size_t n)
{
    const unsigned char *p = (const unsigned char *)data;
    LhU32 used = m->lo & 63u;
    const LhU32 lo = m->lo;
    m->lo = (LhU32)(lo + (LhU32)n);
    if (m->lo < lo) ++m->hi;
    m->hi += (LhU32)((unsigned long long)n >> 32);
    if (used) {
        const LhU32 room = 64u - used;
        if (n < room) {
            memcpy(m->buf + used, p, n);
            return;
        }
        memcpy(m->buf + used, p, room);
        LhMd5Block(m->s, m->buf);
        p += room, n -= room;
    }
    while (n >= 64) {
        LhMd5Block(m->s, p);
        p += 64, n -= 64;
    }
    memcpy(m->buf, p, n);
}

static void LhMd5Final(LhMd5 *m, unsigned char out[16])
{
    static const unsigned char kPad[64] = { 0x80 };
    unsigned char len[8];
    const LhU32 lo = m->lo << 3, hi = m->hi << 3 | m->lo >> 29;
    const LhU32 used = m->lo & 63u;
    int i;
    for (i = 0; i < 4; ++i) len[i] = (unsigned char)(lo >> (8 * i)), len[4 + i] = (unsigned char)(hi >> (8 * i));
    LhMd5Update(m, kPad, used < 56 ? 56 - used : 120 - used);
    LhMd5Update(m, len, 8);
    for (i = 0; i < 4; ++i) {
        out[i * 4] = (unsigned char)m->s[i], out[i * 4 + 1] = (unsigned char)(m->s[i] >> 8);
        out[i * 4 + 2] = (unsigned char)(m->s[i] >> 16), out[i * 4 + 3] = (unsigned char)(m->s[i] >> 24);
    }
}

static void LhHex(const unsigned char *b, int n, char *out)  // out: 2n + 1 chars
{
    static const char kHex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < n; ++i) out[2 * i] = kHex[b[i] >> 4], out[2 * i + 1] = kHex[b[i] & 15];
    out[2 * n] = 0;
}

// ---------------------------------------------------------------- frame-time statistics (one room, or the session)
#define LH_BIN_MS 0.25f  // histogram step
#define LH_BINS 400      // 0..100 ms, then one overflow bin (the percentile there is the worst frame)
#define LH_STALL_MS 1000.0f  // a frame this long is a stall (loading, Alt+Tab, a breakpoint): not in the mean / p99
typedef struct {
    unsigned n;        // frames counted (normal + spike)
    double sum;        // their milliseconds
    float worst;       // the longest counted frame
    unsigned spikes;   // frames over LhSpikeMs
    unsigned stalls;   // frames of LH_STALL_MS or more (not counted in n)
    unsigned bin[LH_BINS + 1];
} LhStats;

static void LhStatsReset(LhStats *s) { memset(s, 0, sizeof *s); }

// A spike: three frames' time at the target rate, never under 50 ms (one 30 fps frame plus a half is not worth a line).
// 30 fps: 100 ms; 60 / 120 / 180 fps: 50 ms. A target of 0 or less (rate unknown) counts as 30 fps.
static float LhSpikeMs(float targetMs)
{
    float t = targetMs > 0.0f ? 3.0f * targetMs : 100.0f;
    return t < 50.0f ? 50.0f : t;
}

enum { LHF_SKIP, LHF_NORMAL, LHF_SPIKE, LHF_STALL };
static int LhClassify(float ms, float targetMs)
{
    if (!(ms > 0.0f) || ms != ms) return LHF_SKIP;  // zero, negative, NaN
    if (ms >= LH_STALL_MS) return LHF_STALL;
    return ms > LhSpikeMs(targetMs) ? LHF_SPIKE : LHF_NORMAL;
}

// Adds one frame; returns its class.
static int LhStatsAdd(LhStats *s, float ms, float targetMs)
{
    const int c = LhClassify(ms, targetMs);
    int b;
    if (c == LHF_SKIP) return c;
    if (c == LHF_STALL) {
        ++s->stalls;
        return c;
    }
    if (c == LHF_SPIKE) ++s->spikes;
    ++s->n;
    s->sum += ms;
    if (ms > s->worst) s->worst = ms;
    b = (int)(ms / LH_BIN_MS);
    if (b > LH_BINS) b = LH_BINS;
    if (b < 0) b = 0;
    ++s->bin[b];
    return c;
}

static float LhStatsMean(const LhStats *s) { return s->n ? (float)(s->sum / s->n) : 0.0f; }

// The pct-th percentile (0..100): the upper edge of the bin that holds that frame (0.25 ms resolution), the worst
// frame when it falls in the overflow bin or is shorter than that edge.
static float LhStatsPct(const LhStats *s, float pct)
{
    unsigned need, acc = 0;
    int b;
    if (!s->n) return 0.0f;
    if (pct <= 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    need = (unsigned)((double)pct / 100.0 * s->n + 0.999999);
    if (need < 1) need = 1;
    for (b = 0; b <= LH_BINS; ++b) {
        acc += s->bin[b];
        if (acc >= need) {
            float edge;
            if (b == LH_BINS) return s->worst;
            edge = (float)(b + 1) * LH_BIN_MS;
            return edge < s->worst ? edge : s->worst;
        }
    }
    return s->worst;
}

// Merges a room into the session.
static void LhStatsMerge(LhStats *into, const LhStats *from)
{
    int b;
    into->n += from->n;
    into->sum += from->sum;
    if (from->worst > into->worst) into->worst = from->worst;
    into->spikes += from->spikes;
    into->stalls += from->stalls;
    for (b = 0; b <= LH_BINS; ++b) into->bin[b] += from->bin[b];
}

// A single spike is an ANOMALY when it is long (250 ms: a visible freeze) and not the room's first 3 s (its files load).
#define LH_FREEZE_MS 250.0f
#define LH_ENTRY_MS 3000.0f
static int LhFreezeIsAnomaly(float ms, float sinceRoomMs) { return ms >= LH_FREEZE_MS && ms < LH_STALL_MS && sinceRoomMs >= LH_ENTRY_MS; }

// Stutter in a room: at least 10 spikes and at least 6 a minute of play.
static int LhStutter(unsigned spikes, float playSeconds)
{
    return spikes >= 10 && playSeconds > 0.0f && (float)spikes * 60.0f / playSeconds >= 6.0f;
}

// ---------------------------------------------------------------- a rate limit per window (e.g. 2 lines per room)
typedef struct { unsigned long start; int n; } LhRate;
// 1 while fewer than max lines were let through in the window of windowMs that began at the first line of it.
static int LhRateOk(LhRate *r, unsigned long nowMs, unsigned long windowMs, int max)
{
    if (r->n == 0 || nowMs - r->start >= windowMs) {
        r->start = nowMs;
        r->n = 0;
    }
    if (r->n >= max) return 0;
    ++r->n;
    return 1;
}

// ---------------------------------------------------------------- "on, but never acted" over several rooms
// Fed once per room. An eligible room (the row was on and the room was one where the feature must act: the caller
// decides) without an action adds to the streak; an action, or the row off, ends it; an ineligible room changes nothing.
// Returns 1 once when the streak reaches need (then again only after an action or the row off re-arms it).
typedef struct { int streak, flagged; } LhIdle;
static int LhIdleStep(LhIdle *w, int on, int eligible, int acted, int need)
{
    if (!on || acted) {
        w->streak = 0;
        w->flagged = 0;
        return 0;
    }
    if (!eligible) return 0;
    ++w->streak;
    if (w->streak >= need && !w->flagged) {
        w->flagged = 1;
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- Supply Drops: kills without a single drop
// 1 when no drop came from `kills` rolls although each had at least probability p: (1 - p)^kills under 1 %.
static int LhDropsSuspicious(int kills, int drops, float p)
{
    double q = 1.0;
    int i;
    if (drops > 0 || kills <= 0 || !(p > 0.0f) || p >= 1.0f) return 0;
    for (i = 0; i < kills && q >= 0.01; ++i) q *= 1.0 - p;
    return q < 0.01;
}

// ---------------------------------------------------------------- flicker: a value that should hold still
typedef struct { int have, last, flips; } LhFlip;
static void LhFlipReset(LhFlip *f) { f->have = 0, f->flips = 0; }
// Returns 1 when v differs from the last value (a flip, counted).
static int LhFlipStep(LhFlip *f, int v)
{
    if (!f->have) {
        f->have = 1;
        f->last = v;
        return 0;
    }
    if (v == f->last) return 0;
    f->last = v;
    ++f->flips;
    return 1;
}

// ---------------------------------------------------------------- address space (the exe is not large-address aware)
// The margin steps: 1 the first time the free margin falls under 384 MB, then at every further 128 MB less. `*low`
// holds the lowest margin step already reported (start it at 0 = none).
#define LH_VA_WARN_MB 384
#define LH_VA_STEP_MB 128
static int LhVaStep(int *low, int usedMb, int totalMb)
{
    const int margin = totalMb - usedMb;
    int step;
    if (totalMb <= 0 || margin >= LH_VA_WARN_MB) return 0;
    step = 1 + (LH_VA_WARN_MB - (margin < 0 ? 0 : margin)) / LH_VA_STEP_MB;  // 1: < 384, 2: < 256, 3: < 128, 4: none
    if (step <= *low) return 0;
    *low = step;
    return 1;
}

// ---------------------------------------------------------------- ini values: is this the default?
static int LhLower(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
// Same text (case and outer spaces ignored), or the same number ("2" and "2.0", "0x10" is text).
static int LhIniSame(const char *v, const char *def)
{
    const char *a = v, *b = def, *ae, *be;
    while (*a == ' ' || *a == '\t') ++a;
    while (*b == ' ' || *b == '\t') ++b;
    ae = a + strlen(a);
    be = b + strlen(b);
    while (ae > a && (ae[-1] == ' ' || ae[-1] == '\t')) --ae;
    while (be > b && (be[-1] == ' ' || be[-1] == '\t')) --be;
    if (ae - a == be - b) {
        const char *p = a, *q = b;
        while (p < ae && LhLower(*p) == LhLower(*q)) ++p, ++q;
        if (p == ae) return 1;
    }
    {   // numbers: digits, one optional sign and dot
        double x = 0.0, y = 0.0;
        int okA = 0, okB = 0, k;
        for (k = 0; k < 2; ++k) {
            const char *p = k ? b : a, *e = k ? be : ae;
            double val = 0.0, scale = 0.0, sign = 1.0;
            int digits = 0, bad = 0;
            if (p < e && (*p == '-' || *p == '+')) sign = *p++ == '-' ? -1.0 : 1.0;
            for (; p < e; ++p) {
                if (*p >= '0' && *p <= '9') {
                    ++digits;
                    if (scale > 0.0) val += (*p - '0') * scale, scale *= 0.1;
                    else val = val * 10.0 + (*p - '0');
                } else if (*p == '.' && scale == 0.0) {
                    scale = 0.1;
                } else {
                    bad = 1;
                    break;
                }
            }
            if (k) okB = digits && !bad, y = sign * val;
            else okA = digits && !bad, x = sign * val;
        }
        return okA && okB && x == y;
    }
}

#endif
