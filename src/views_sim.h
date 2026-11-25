// v3.41 VW341: pure rules of the black sides beside a centred 4:3 picture (BlackSides: the Room 302 hole / door scope /
// window views, the Water Prison peeps, MenuPillars) and of the Borderless edge-cover check. Native test:
// test\views_sim_test.c (test\build_views_test.cmd). No game, no Windows headers.
//
// The picture: Menus43's interface set squeezes the 512-wide canvas by S = 3w / 4h, so a full-canvas 2D picture spans
// x in [L, w - L) with L = (w - 4h/3) / 2 = (3w - 4h) / 6 (clip -1/S .. 1/S, viewport x = (clip + 1) * w / 2). Direct3D 9
// (GOG's wrapper; dgVoodoo2 keeps its rule) samples pixel i at x = i and keeps the top-left rule: a pixel is the
// picture's when L <= i < w - L. The sides are cleared AFTER the picture (a Clear, never blended), so a pillar that
// overlaps the picture hides that column and a pillar that stops short leaves the world showing in a 1 px gap.
// v3.41's rule (v3.42 replaced it, below; the test keeps it for comparison):
//   left  = floor(L) + 1: covers pixel floor(L) even when the GPU's float edge lands a hair right of a whole L
//           (2560x1440: L = 320, the float edge 320.00004); hides one picture column only when L is whole.
//   right = floor(L): the picture's last pixel is ceil(w - L) - 1 = w - floor(L) - 1 for every L, so the right pillar
//           starts right after it (no overlap) and a hair of float error either way leaves no gap.
// v3.40 and before used ceil(float L) on both sides: no gap at the sizes checked (its float L rounds up), one picture
// column hidden on each side at whole L; the test compares both rules under the GPU's float error.
//
// v3.42 EG342 (the user's v3.41 QA item 6: a 1 px lit line in column 0 / row 0 of menus, memos, peepholes, the window
// view and the Options page, 16:9 and faintly 4:3). The cause: the game's 2D canvases map their edges to clip -1 / +1
// exactly (404D80: c2.x = 1/256, c3.x = -8, sprite origin 0x800 - 256), and Direct3D 9 puts clip -1 on the CENTRE of
// pixel 0 (pixel i is centred on x = i). A full-canvas quad then spans x in [0, w): pixel 0 is in only by the tie of
// the top-left rule (zero margin), pixel w - 1 with a whole pixel to spare. Any backend that lands that edge a hair to
// the right / below (dgVoodoo2 on Direct3D 11, whose pixel centres sit at i + 0.5) drops column 0 and row 0, and the
// world or the frozen pause picture underneath shows there; the right / bottom edges stay covered. The BlackSides Clear
// of [0, left) lost column 0 / row 0 the same way in the user's shots (a rect Clear drawn as such a quad). The game's own
// screen-copy quads (post pass 55A260, pause picture 55A8C0) already use the Direct3D 9 rule: corners at -0.5.
// v3.42: (1) every 2D canvas set is uploaded half a pixel left / up (VwAlignNdc: the canvas edges land on pixel EDGES,
//            half a pixel of margin on all four sides, and a texel of a 1:1 screen copy on its pixel centre);
//        (2) the pillars are drawn as quads that run 1 px past the screen edges (VwPillarQuads) after the Clear, and
//            their inner edges keep at least half a pixel over the aligned picture: left = right = ceil(L + 0.5)
//            (never a gap; at most one picture column hidden per side, two at some backend offsets).
// test\views_sim_test.c sweeps every size from 240 to 2400 lines, 4:3 to 32:9, with the backend's edge offset e from
// -0.49 to +0.49 px (and the quads' own offset up to 0.49 px apart from the picture's): the union of the pillars, the
// picture and the full-canvas fills covers every pixel centre, column 0 and row 0 included; v3.41's mapping loses
// column 0 / row 0 as soon as e > 0.
#ifndef VIEWS_SIM_H
#define VIEWS_SIM_H

// Pillar widths for a w x h target (pixels). 0 = no pillars (4:3 or narrower, or a bad size).
// v3.42 EG342: the picture is aligned (VwAlignNdc), so it covers the pixel area [L, w - L) symmetrically: both pillars
// are ceil(L + 0.5) wide (6 L = num: ceil((num + 3) / 6)).
static int VwPillars(int w, int h, int *left, int *right)
{
    long long num;
    *left = *right = 0;
    if (w <= 0 || h <= 0) return 0;
    num = 3LL * w - 4LL * h;  // 6 L
    if (num <= 0) return 0;
    *left = (int)((num + 3 + 5) / 6);  // v3.42 EG342: ceil((num + 3) / 6) = ceil(L + 0.5)
    *right = *left;
    if (*left > w / 2) *left = w / 2;
    if (*right > w / 2) *right = w / 2;
    return 1;
}

// v3.42 EG342: the offsets added to the 2D canvas constants (c3.xy, c95.xy) so the canvases sit on pixel edges: half a
// pixel left (clip x - 1 / w) and half a pixel up (clip y + 1 / h; screen y = (1 - clip y) * h / 2). 0 for a bad size.
static void VwAlignNdc(int w, int h, float *dx, float *dy)
{
    *dx = w > 0 ? -1.0f / (float)w : 0.0f;
    *dy = h > 0 ? 1.0f / (float)h : 0.0f;
}

// v3.42 EG342: the two pillar quads in Direct3D 9 screen coordinates (pixel i centred on x = i): q[k] = x0, y0, x1, y1.
// Pixel-edge spans [-1, left) and [w - right, w + 1) by [-1, h + 1): one pixel past every screen edge, so no edge
// offset of the backend below a pixel can open column 0, row 0 or the last ones; the inner edges sit on the pillar
// widths. Returns how many quads (0..2).
static int VwPillarQuads(int w, int h, int left, int right, float q[2][4])
{
    int n = 0;
    if (w <= 0 || h <= 0) return 0;
    if (left > 0) {
        q[n][0] = -1.5f; q[n][1] = -1.5f; q[n][2] = (float)left - 0.5f; q[n][3] = (float)h + 0.5f;
        ++n;
    }
    if (right > 0) {
        q[n][0] = (float)(w - right) - 0.5f; q[n][1] = -1.5f; q[n][2] = (float)w + 0.5f; q[n][3] = (float)h + 0.5f;
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------- Borderless: what covers the game's edges
// A window above the game whose rectangle meets the game window only in a thin strip along one of its edges (at most
// maxThick px): Windows draws that strip over the game picture (another program's window edge on the neighbouring
// monitor, an auto-hidden taskbar's sliver). Returns the edges it touches (VW_LEFT ...), 0 = none; *cut = the strip.
typedef struct { long l, t, r, b; } VwRect;
enum { VW_LEFT = 1, VW_TOP = 2, VW_RIGHT = 4, VW_BOTTOM = 8 };

static int VwEdgeSliver(VwRect win, VwRect game, int maxThick, VwRect *cut)
{
    VwRect c;
    long cw, ch;
    int bits = 0;
    c.l = win.l > game.l ? win.l : game.l;
    c.t = win.t > game.t ? win.t : game.t;
    c.r = win.r < game.r ? win.r : game.r;
    c.b = win.b < game.b ? win.b : game.b;
    if (cut) *cut = c;
    if (c.r <= c.l || c.b <= c.t) return 0;
    cw = c.r - c.l;
    ch = c.b - c.t;
    if (cw <= maxThick && c.l == game.l) bits |= VW_LEFT;
    if (cw <= maxThick && c.r == game.r) bits |= VW_RIGHT;
    if (ch <= maxThick && c.t == game.t) bits |= VW_TOP;
    if (ch <= maxThick && c.b == game.b) bits |= VW_BOTTOM;
    return bits;
}

static const char *VwEdgeName(int bits)
{
    return bits & VW_RIGHT ? "right" : bits & VW_LEFT ? "left" : bits & VW_BOTTOM ? "bottom" : bits & VW_TOP ? "top" : "none";
}

#endif
