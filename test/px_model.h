// v3.47 PX347: a model of the pixel checks' GPU side for the native tests (post_sim_test.c, shadows_sim_test.c; needs
// ../src/post_sim.h first). The source: cells of cw texels, each one Clear (a "lost" cell keeps the fresh texture's 0);
// each destination pixel samples the source bilinearly at its centre moved by (ox, oy) texels, outside the texture
// CLAMP or black (the v3.46 log: black left of the first cell); then the shader (a callback, rgba 0..1), written as is
// (the grade: COLORWRITEENABLE RGB, the destination's alpha kept) or blended SRCALPHA / INVSRCALPHA over the clear (the
// shadow composite). The v3.46 backend (dgVoodoo2 2.87.6 Direct3D 11 through GOG's d3d8 wrapper) = ox = oy = -0.5, black.
#ifndef PX_MODEL_H
#define PX_MODEL_H
#define PXM_MAXW 32
#define PXM_MAXH 4
typedef void (*PxmShade_t)(const float in[4], float out[4], const void *ctx);
typedef struct { float t[PXM_MAXH][PXM_MAXW][4]; int w, h; } PxmTex;  // rgba 0..1
static void PxmUnpack(unsigned long v, float o[4])
{
    o[0] = (float)((v >> 16) & 0xFFu) / 255.0f, o[1] = (float)((v >> 8) & 0xFFu) / 255.0f, o[2] = (float)(v & 0xFFu) / 255.0f;
    o[3] = (float)((v >> 24) & 0xFFu) / 255.0f;
}
static unsigned long PxmPack(const float c[4])
{
    unsigned long v = 0;
    int i;
    for (i = 0; i < 4; ++i) {
        const float x = c[i] < 0.0f ? 0.0f : c[i] > 1.0f ? 1.0f : c[i];
        const unsigned long b = (unsigned long)(x * 255.0f + 0.5f);
        v |= b << (i == 3 ? 24 : 16 - 8 * i);
    }
    return v;
}
// lost: the cell whose Clear does not land (-1: none)
static void PxmFill(PxmTex *s, int w, int h, int cw, const unsigned long *cells, int n, int lost)
{
    int x, y;
    memset(s, 0, sizeof *s);
    s->w = w, s->h = h;
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x)
            if (x / cw < n && x / cw != lost) PxmUnpack(cells[x / cw], s->t[y][x]);
}
static void PxmTexel(const PxmTex *s, int x, int y, int black, float o[4])
{
    int i;
    if (black && (x < 0 || y < 0 || x >= s->w || y >= s->h)) {
        for (i = 0; i < 4; ++i) o[i] = 0.0f;
        return;
    }
    x = x < 0 ? 0 : x >= s->w ? s->w - 1 : x;
    y = y < 0 ? 0 : y >= s->h ? s->h - 1 : y;
    for (i = 0; i < 4; ++i) o[i] = s->t[y][x][i];
}
// the bilinear sample at texel-space point (u, v) (texel centres at i + 0.5)
static void PxmSample(const PxmTex *s, float u, float v, int black, float o[4])
{
    const float fu = u - 0.5f, fv = v - 0.5f;
    const int x0 = (int)floorf(fu), y0 = (int)floorf(fv);
    const float ax = fu - (float)x0, ay = fv - (float)y0;
    float a[4], b[4], c[4], d[4];
    int i;
    PxmTexel(s, x0, y0, black, a), PxmTexel(s, x0 + 1, y0, black, b);
    PxmTexel(s, x0, y0 + 1, black, c), PxmTexel(s, x0 + 1, y0 + 1, black, d);
    for (i = 0; i < 4; ++i) o[i] = (a[i] * (1.0f - ax) + b[i] * ax) * (1.0f - ay) + (c[i] * (1.0f - ax) + d[i] * ax) * ay;
}
// the destination pixel (x, y) after the test draw over clear (blend 0: rgb written, the clear's alpha kept)
static unsigned long PxmPixel(const PxmTex *s, int x, int y, float ox, float oy, int black, PxmShade_t sh, const void *ctx,
                              unsigned long clear, int blend)
{
    float in[4], out[4], d[4], r[4];
    int i;
    PxmSample(s, (float)x + 0.5f + ox, (float)y + 0.5f + oy, black, in);
    sh(in, out, ctx);
    PxmUnpack(clear, d);
    for (i = 0; i < 3; ++i) r[i] = blend ? out[i] * out[3] + d[i] * (1.0f - out[3]) : out[i];
    r[3] = d[3];
    return PxmPack(r);
}
// what post.inc PxBody reads: the destination at each cell's places, the source at each centre (the v3.47 pattern)
static void PxmProbe(const PxmTex *s, float ox, float oy, int black, PxmShade_t sh, const void *ctx, unsigned long clear, int blend,
                     PxProbe *p)
{
    int k, at;
    memset(p, 0, sizeof *p);
    for (k = 0; k < PX_CELLS; ++k) {
        for (at = 0; at < PXC_N; ++at) p->dst[k][at] = PxmPixel(s, PxCol(k, at), PxRow(at), ox, oy, black, sh, ctx, clear, blend);
        p->src[k] = PxmPack(s->t[PX_ROW][PxCol(k, PXC_MID)]);
    }
    p->srcOk = 1;
}
// what v3.44 .. v3.46 read: cells 2 texels wide in an 8 x 2 target, the pixel (2k, 1)
static void PxmOld(const unsigned long *cells, int lost, float ox, float oy, int black, PxmShade_t sh, const void *ctx,
                   unsigned long clear, int blend, unsigned long out[PX_CELLS])
{
    static PxmTex s;
    int k;
    PxmFill(&s, 2 * PX_CELLS, 2, 2, cells, PX_CELLS, lost);
    for (k = 0; k < PX_CELLS; ++k) out[k] = PxmPixel(&s, 2 * k, 1, ox, oy, black, sh, ctx, clear, blend);
}
#endif
