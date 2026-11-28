// ================================================================ v3.59 PV359: the F7 picture's look layers as files (pure)
// staging/v359_feedback/w1_preview/PLAN.md (USER_FEEDBACK_v358 QA 3: "Thermal fails to show heat of Eileen on our stock
// screenshot and Spirit Vision fails to show her flame"). A picture holds no bodies, so F7 now keeps, beside its picture,
// the two layers a pause keeps for the Effects page (v3.56 PV356): Thermal's heat bodies / footprints and Spirit Vision's
// ghost glow / auras / soul fires, each drawn over black at the picture's own size. Pure C (no D3D, no files): included
// through preview_sim.h by preview.inc and by test/preview_sim_test.c.
//
// The files: <picture without .dds>.heat and .spirit (preview.dds -> preview.heat / preview.spirit; the stock picture
// SH4Refined_preview_stock.dds -> SH4Refined_preview_stock.heat / .spirit). Lossless and small: the layers are black where
// nothing is hot or spiritual, so the pixels are stored as runs of black and literal BGRX pixels.
//   header (32 bytes, little-endian): "PVLY", version 1, kind (1 heat, 2 spirit), width, height, the picture's stamp,
//   payload bytes, CRC-32 of the payload;
//   payload: pairs (u32 black run, u32 literal count n, n x u32 pixel), in row order, covering width x height exactly.
// The picture's stamp: F7 (FxPrevWrite) writes "PV59" and a fresh 32-bit stamp into the DDS header's dwReserved1[0..1]
// (bytes 32..39, ignored by every DDS reader); a layer is used only with the picture whose stamp and size it carries, so a
// layer left from an older picture is never laid over a new one. Copying the set (the next install's promotion to the
// stock picture) keeps the stamp.
#ifndef PVLAYER_SIM_H
#define PVLAYER_SIM_H
#include <string.h>
#include <stddef.h>

typedef unsigned int PvlU32;
#define PVL_MAGIC 0x594C5650u      // "PVLY"
#define PVL_VERSION 1u
#define PVL_HDR 32u
#define PVL_PIC_TAG 0x39355650u    // "PV59": the picture's DDS header, dwReserved1[0] (byte 32); dwReserved1[1] (byte 36) the stamp
#define PVL_MAX_PIX (1920u * 1080u)  // a layer's pixels at most (F7 writes at most 1280 x 720): 8.3 MB decoded
#define PVL_MIN_SIDE 16u
enum { PVLK_HEAT = 1, PVLK_SPIRIT = 2 };
static const char *const kPvlKind[3] = { "?", "heat", "spirit" };
// the load's result (PVLR_OK = the layer is used; PVLR_MISSING = no file: nothing to say)
enum { PVLR_OK, PVLR_MISSING, PVLR_READ, PVLR_HEADER, PVLR_KIND, PVLR_SIZE, PVLR_NOSTAMP, PVLR_STAMP, PVLR_CRC, PVLR_DATA, PVLR_TEX,
       PVLR_N };
static const char *const kPvlWhy[PVLR_N] = {
    "used", "no file", "could not be read", "not a layer file (header)", "the other layer's file", "not the picture's size",
    "the picture has no stamp (taken before v3.59: its layers cannot be matched)", "not this picture's (another F7 shot)",
    "damaged (CRC)", "damaged (runs)", "the texture could not be made" };

static PvlU32 PvlRd32(const unsigned char *p) { return (PvlU32)p[0] | (PvlU32)p[1] << 8 | (PvlU32)p[2] << 16 | (PvlU32)p[3] << 24; }
static void PvlWr32(unsigned char *p, PvlU32 v)
{
    p[0] = (unsigned char)v, p[1] = (unsigned char)(v >> 8), p[2] = (unsigned char)(v >> 16), p[3] = (unsigned char)(v >> 24);
}
// CRC-32 (IEEE 802.3, reflected 0xEDB88320, as zlib's crc32)
static PvlU32 PvlCrc32(const unsigned char *p, size_t n)
{
    static PvlU32 s_t[256];
    static int s_made;
    PvlU32 c = 0xFFFFFFFFu;
    size_t i;
    if (!s_made) {
        PvlU32 k, v;
        int b;
        for (k = 0; k < 256; ++k) {
            v = k;
            for (b = 0; b < 8; ++b) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            s_t[k] = v;
        }
        s_made = 1;
    }
    for (i = 0; i < n; ++i) c = s_t[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static int PvlBlack(PvlU32 p) { return (p & 0x00FFFFFFu) == 0; }
static int PvlSizeOk(PvlU32 w, PvlU32 h)
{
    return w >= PVL_MIN_SIDE && h >= PVL_MIN_SIDE && w <= 8192u && h <= 8192u && (unsigned long long)w * h <= PVL_MAX_PIX;
}
// the encoder's buffer: never more than the raw pixels + 8 bytes (a literal is cut only at two or more black pixels)
static size_t PvlMaxBytes(PvlU32 w, PvlU32 h) { return PVL_HDR + (size_t)w * h * 4u + 16u; }
// w x h BGRX pixels (rows packed) -> the file in out (header + payload). Returns its bytes (0: refused / no room).
// *lit (may be NULL): the pixels stored as literals (not black).
static size_t PvlEncode(const PvlU32 *px, PvlU32 w, PvlU32 h, PvlU32 kind, PvlU32 stamp, unsigned char *out, size_t cap, PvlU32 *lit)
{
    const size_t n = (size_t)w * h;
    size_t i = 0, at = PVL_HDR, lits = 0;
    if (!px || !out || !PvlSizeOk(w, h) || (kind != PVLK_HEAT && kind != PVLK_SPIRIT) || cap < PvlMaxBytes(w, h)) return 0;
    while (i < n) {
        size_t r = 0, l = 0, k;
        while (i + r < n && PvlBlack(px[i + r])) ++r;
        k = i + r;
        while (k + l < n && !(PvlBlack(px[k + l]) && (k + l + 1 >= n || PvlBlack(px[k + l + 1])))) ++l;  // up to 2 blacks / the end
        if (at + 8 + l * 4 > cap) return 0;
        PvlWr32(out + at, (PvlU32)r), PvlWr32(out + at + 4, (PvlU32)l);
        at += 8;
        for (; l; --l, ++k, at += 4) PvlWr32(out + at, px[k] | 0xFF000000u), ++lits;
        i = k;
    }
    PvlWr32(out + 0, PVL_MAGIC), PvlWr32(out + 4, PVL_VERSION), PvlWr32(out + 8, kind), PvlWr32(out + 12, w), PvlWr32(out + 16, h);
    PvlWr32(out + 20, stamp), PvlWr32(out + 24, (PvlU32)(at - PVL_HDR)), PvlWr32(out + 28, PvlCrc32(out + PVL_HDR, at - PVL_HDR));
    if (lit) *lit = (PvlU32)lits;
    return at;
}
// a layer file f (n bytes) for the picture (picW x picH, its stamp; 0 = none): PVLR_OK or why not. The runs are walked
// (bounds) here, so PvlDecode of a checked file cannot run out of the buffer.
static int PvlCheck(const unsigned char *f, size_t n, PvlU32 kind, PvlU32 picW, PvlU32 picH, PvlU32 picStamp)
{
    PvlU32 w, h, len, pix = 0;
    size_t at;
    if (!f || n < PVL_HDR || PvlRd32(f) != PVL_MAGIC || PvlRd32(f + 4) != PVL_VERSION) return PVLR_HEADER;
    if (PvlRd32(f + 8) != kind) return PvlRd32(f + 8) == PVLK_HEAT || PvlRd32(f + 8) == PVLK_SPIRIT ? PVLR_KIND : PVLR_HEADER;
    w = PvlRd32(f + 12), h = PvlRd32(f + 16), len = PvlRd32(f + 24);
    if (!PvlSizeOk(w, h)) return PVLR_HEADER;
    if (w != picW || h != picH) return PVLR_SIZE;
    if (!picStamp) return PVLR_NOSTAMP;
    if (PvlRd32(f + 20) != picStamp) return PVLR_STAMP;
    if ((size_t)len != n - PVL_HDR) return PVLR_DATA;
    if (PvlCrc32(f + PVL_HDR, len) != PvlRd32(f + 28)) return PVLR_CRC;
    for (at = PVL_HDR; pix < w * h;) {
        PvlU32 r, l;
        if (at + 8 > n) return PVLR_DATA;
        r = PvlRd32(f + at), l = PvlRd32(f + at + 4);
        at += 8;
        if (r > w * h - pix || l > w * h - pix - r || (size_t)l * 4 > n - at) return PVLR_DATA;
        pix += r + l;
        at += (size_t)l * 4;
        if (!r && !l) return PVLR_DATA;  // (an empty pair: never written; refuse rather than loop)
    }
    return at == n ? PVLR_OK : PVLR_DATA;
}
// a checked file -> w x h pixels at out (pitch bytes a row), black as 0xFF000000. 1 = done
static int PvlDecode(const unsigned char *f, size_t n, unsigned char *out, int pitch)
{
    const PvlU32 w = PvlRd32(f + 12), h = PvlRd32(f + 16), total = w * h;
    PvlU32 pix = 0, x = 0, y = 0;
    size_t at = PVL_HDR;
    if (!out || pitch < (int)(w * 4)) return 0;
    while (pix < total && at + 8 <= n) {
        PvlU32 r = PvlRd32(f + at), l = PvlRd32(f + at + 4), k;
        at += 8;
        if (r > total - pix || l > total - pix - r || (size_t)l * 4 > n - at) return 0;
        for (k = 0; k < r + l; ++k) {
            const PvlU32 v = k < r ? 0xFF000000u : PvlRd32(f + at + (size_t)(k - r) * 4) | 0xFF000000u;
            memcpy(out + (size_t)y * (size_t)pitch + (size_t)x * 4, &v, 4);
            if (++x == w) x = 0, ++y;
        }
        at += (size_t)l * 4;
        pix += r + l;
    }
    return pix == total;
}
// the picture's DDS header (its first 128 bytes): its stamp (0 = none: not written by v3.59's F7) and size
static PvlU32 PvlPicStamp(const unsigned char *d, size_t n, PvlU32 *w, PvlU32 *h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    if (!d || n < 128 || memcmp(d, "DDS ", 4) || PvlRd32(d + 4) != 124) return 0;
    if (h) *h = PvlRd32(d + 12);
    if (w) *w = PvlRd32(d + 16);
    return PvlRd32(d + 32) == PVL_PIC_TAG ? PvlRd32(d + 36) : 0;
}
// the stamp F7 writes into its picture's 128-byte header (dwReserved1[0..1], bytes 32..39)
static void PvlPicTag(unsigned char *hdr, PvlU32 stamp)
{
    PvlWr32(hdr + 32, PVL_PIC_TAG);
    PvlWr32(hdr + 36, stamp);
}
// the layer's path beside the picture: its name without a .dds extension (any case) + ".heat" / ".spirit". 1 = fits
static int PvlPath(const char *pic, int kind, char *out, size_t cap)
{
    const char *ext = kind == PVLK_HEAT ? ".heat" : kind == PVLK_SPIRIT ? ".spirit" : NULL;
    size_t n, i, base;
    if (!pic || !pic[0] || !ext || !out || !cap) return 0;
    n = strlen(pic);
    base = n;
    for (i = n; i > 0; --i) {
        const char c = pic[i - 1];
        if (c == '\\' || c == '/' || c == ':') break;
        if (c == '.') {
            if (n - (i - 1) == 4 && (pic[i] == 'd' || pic[i] == 'D') && (pic[i + 1] == 'd' || pic[i + 1] == 'D') &&
                (pic[i + 2] == 's' || pic[i + 2] == 'S'))
                base = i - 1;
            break;
        }
    }
    if (base + strlen(ext) + 1 > cap) return 0;
    memcpy(out, pic, base);
    memcpy(out + base, ext, strlen(ext) + 1);
    return 1;
}
#endif
