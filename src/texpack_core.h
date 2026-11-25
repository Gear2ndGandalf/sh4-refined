// texpack_core.h: the pure half of [Textures] Pack (texpack.inc): MD5 of a texture's level-0 bytes (the replacement
// key) and the DDS reader. No game addresses here; CModding\SH4HDTextures\texpack\test_texpack.c compiles it alone and checks it against the
// Python extractor (CModding\SH4HDTextures\texpack\tools\texpack.py) on real bank textures and DDS files.
//
// Key: the first 8 bytes of MD5(level 0 as stored in the bank), as 16 lowercase hex digits; the same as Python's
// hashlib.md5(level0).hexdigest()[:16]. The game uploads the stored bytes verbatim (memcpy after LockRect, no
// conversion), so the key computed offline from data\*.bin is the key seen at run time.
#ifndef TEXPACK_CORE_H
#define TEXPACK_CORE_H
#include <string.h>

typedef unsigned __int64 TpKey;

// ---------------------------------------------------------------- MD5 (RFC 1321)
typedef struct { unsigned int a, b, c, d; unsigned __int64 len; unsigned char buf[64]; unsigned int fill; } TpMd5;

static const unsigned int kTpMd5K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
static const unsigned char kTpMd5R[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

static void TpMd5Block(TpMd5 *m, const unsigned char *p)
{
    unsigned int w[16], a = m->a, b = m->b, c = m->c, d = m->d, f, t;
    int i, g;
    for (i = 0; i < 16; ++i)
        w[i] = (unsigned int)p[i * 4] | (unsigned int)p[i * 4 + 1] << 8 | (unsigned int)p[i * 4 + 2] << 16 |
               (unsigned int)p[i * 4 + 3] << 24;
    for (i = 0; i < 64; ++i) {
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        t = d;
        d = c;
        c = b;
        f += a + kTpMd5K[i] + w[g];
        b += (f << kTpMd5R[i]) | (f >> (32 - kTpMd5R[i]));
        a = t;
    }
    m->a += a; m->b += b; m->c += c; m->d += d;
}

static void TpMd5Init(TpMd5 *m)
{
    m->a = 0x67452301; m->b = 0xefcdab89; m->c = 0x98badcfe; m->d = 0x10325476;
    m->len = 0;
    m->fill = 0;
}

static void TpMd5Add(TpMd5 *m, const void *data, size_t n)
{
    const unsigned char *p = (const unsigned char *)data;
    m->len += n;
    if (m->fill) {
        size_t k = 64 - m->fill < n ? 64 - m->fill : n;
        memcpy(m->buf + m->fill, p, k);
        m->fill += (unsigned int)k;
        p += k;
        n -= k;
        if (m->fill < 64) return;
        TpMd5Block(m, m->buf);
        m->fill = 0;
    }
    for (; n >= 64; p += 64, n -= 64) TpMd5Block(m, p);
    if (n) { memcpy(m->buf, p, n); m->fill = (unsigned int)n; }
}

static void TpMd5End(TpMd5 *m, unsigned char out[16])
{
    static const unsigned char pad[64] = { 0x80 };
    unsigned char lenb[8];
    unsigned __int64 bits = m->len * 8;
    int i;
    for (i = 0; i < 8; ++i) lenb[i] = (unsigned char)(bits >> (8 * i));
    TpMd5Add(m, pad, m->fill < 56 ? 56 - m->fill : 120 - m->fill);
    TpMd5Add(m, lenb, 8);
    for (i = 0; i < 4; ++i) {
        out[i] = (unsigned char)(m->a >> (8 * i));
        out[4 + i] = (unsigned char)(m->b >> (8 * i));
        out[8 + i] = (unsigned char)(m->c >> (8 * i));
        out[12 + i] = (unsigned char)(m->d >> (8 * i));
    }
}

// The key: digest bytes 0..7 read big-endian, so "%016llx" prints the first 16 hex digits of the md5 hex string.
static TpKey TpKeyOf(const void *data, size_t n)
{
    TpMd5 m;
    unsigned char d[16];
    TpKey k = 0;
    int i;
    TpMd5Init(&m);
    TpMd5Add(&m, data, n);
    TpMd5End(&m, d);
    for (i = 0; i < 8; ++i) k = k << 8 | d[i];
    return k;
}

static int TpHexKey(const char *s, TpKey *out)  // exactly 16 hex digits at s (case ignored); the rest is not read
{
    TpKey k = 0;
    int i;
    for (i = 0; i < 16; ++i) {
        const char c = s[i];
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return 0;
        k = k << 4 | (TpKey)v;
    }
    *out = k;
    return 1;
}

// ---------------------------------------------------------------- DDS
// D3DFORMAT values the game and GOG's D3D8-to-D3D9 wrapper take (BC7 and the other DX10-only formats are not there).
#define TP_FMT_DXT1 0x31545844u
#define TP_FMT_DXT3 0x33545844u
#define TP_FMT_DXT5 0x35545844u
#define TP_FMT_A8R8G8B8 21u
#define TP_FMT_X8R8G8B8 22u
#define TP_MAX_LEVELS 14
#define TP_MAX_DIM 8192u

typedef struct {
    unsigned int w, h, levels, fmt;
    unsigned int block;                      // bytes per 4x4 block (8 / 16), 0 for 32-bit pixels
    unsigned int off[TP_MAX_LEVELS], size[TP_MAX_LEVELS], rows[TP_MAX_LEVELS], rowBytes[TP_MAX_LEVELS];
} TpDds;

static unsigned int TpRd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

static const char *TpFmtName(unsigned int f)
{
    switch (f) {
    case TP_FMT_DXT1: return "DXT1";
    case TP_FMT_DXT3: return "DXT3";
    case TP_FMT_DXT5: return "DXT5";
    case TP_FMT_A8R8G8B8: return "A8R8G8B8";
    case TP_FMT_X8R8G8B8: return "X8R8G8B8";
    }
    return "?";
}

// Parses a whole DDS file in memory. Returns NULL when it is usable (t filled), else the reason. Accepts the legacy
// header (DXT1/3/5 FourCC, 32-bit BGRA / BGRX masks) and the DX10 header texconv writes (BC1, BC2, BC3, B8G8R8A8,
// B8G8R8X8, UNORM or SRGB: sRGB is only a sampling flag, the bytes are the same). 2D textures only, powers of two not
// required, every level's data must be in the file.
static const char *TpDdsParse(const unsigned char *d, size_t n, TpDds *t)
{
    unsigned int flags, pfFlags, fourcc, at, l, w, h, caps2;
    memset(t, 0, sizeof *t);
    if (n < 128 || memcmp(d, "DDS ", 4) || TpRd32(d + 4) != 124) return "not a DDS file";
    flags = TpRd32(d + 8);
    t->h = TpRd32(d + 12);
    t->w = TpRd32(d + 16);
    t->levels = (flags & 0x20000) ? TpRd32(d + 28) : 1;  // DDSD_MIPMAPCOUNT
    if (!t->levels) t->levels = 1;
    caps2 = TpRd32(d + 112);
    if (caps2 & 0x200) return "a cube map";
    if ((flags & 0x800000) && TpRd32(d + 24) > 1) return "a volume texture";
    pfFlags = TpRd32(d + 80);
    fourcc = TpRd32(d + 84);
    at = 128;
    if (pfFlags & 0x4) {  // DDPF_FOURCC
        if (fourcc == TP_FMT_DXT1 || fourcc == TP_FMT_DXT3 || fourcc == TP_FMT_DXT5) t->fmt = fourcc;
        else if (fourcc == 0x30315844u) {  // "DX10"
            unsigned int dxgi, dim, arr;
            if (n < 148) return "truncated DX10 header";
            dxgi = TpRd32(d + 128);
            dim = TpRd32(d + 132);
            arr = TpRd32(d + 140);
            at = 148;
            if (dim != 3) return "not a 2D texture";
            if (arr > 1) return "a texture array";
            switch (dxgi) {
            case 71: case 72: t->fmt = TP_FMT_DXT1; break;
            case 74: case 75: t->fmt = TP_FMT_DXT3; break;
            case 77: case 78: t->fmt = TP_FMT_DXT5; break;
            case 87: case 91: t->fmt = TP_FMT_A8R8G8B8; break;
            case 88: case 93: t->fmt = TP_FMT_X8R8G8B8; break;
            default: return "DXGI format not supported here (use BC1, BC2, BC3 or B8G8R8A8)";
            }
        } else
            return "FourCC not supported (DXT1, DXT3, DXT5 only)";
    } else if ((pfFlags & 0x40) && TpRd32(d + 88) == 32 && TpRd32(d + 92) == 0xff0000 && TpRd32(d + 96) == 0xff00 &&
               TpRd32(d + 100) == 0xff) {  // DDPF_RGB, 32 bits, BGRA in memory
        t->fmt = ((pfFlags & 0x1) && TpRd32(d + 104) == 0xff000000u) ? TP_FMT_A8R8G8B8 : TP_FMT_X8R8G8B8;
    } else
        return "pixel format not supported (DXT1, DXT3, DXT5 or 32-bit BGRA)";
    if (!t->w || !t->h || t->w > TP_MAX_DIM || t->h > TP_MAX_DIM) return "size out of range";
    t->block = t->fmt == TP_FMT_DXT1 ? 8 : (t->fmt == TP_FMT_DXT3 || t->fmt == TP_FMT_DXT5) ? 16 : 0;
    if (t->levels > TP_MAX_LEVELS) t->levels = TP_MAX_LEVELS;
    for (l = 0, w = t->w, h = t->h; l < t->levels; ++l) {
        if (t->block) {
            t->rowBytes[l] = ((w + 3) / 4) * t->block;
            t->rows[l] = (h + 3) / 4;
        } else {
            t->rowBytes[l] = w * 4;
            t->rows[l] = h;
        }
        t->size[l] = t->rowBytes[l] * t->rows[l];
        t->off[l] = at;
        if ((size_t)at + t->size[l] > n) {
            if (!l) return "truncated level 0";
            t->levels = l;  // keep the complete levels
            break;
        }
        at += t->size[l];
        if (w == 1 && h == 1) { t->levels = l + 1; break; }
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }
    return NULL;
}

#endif
