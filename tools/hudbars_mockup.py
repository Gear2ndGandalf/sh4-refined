"""HUD BARS v3.30 (W4 / HB330): offline mock-ups of the Enemy Health Bars (with the ghost revive bar) and the Curse Meter.

The drawing below mirrors src/hudbars.inc call by call: HbRect / HbQuad / HbTri (untextured triangles with a colour and
an alpha per vertex, alpha-blended, D3D's top-left fill rule, pixel centres at i + 0.5), so a render here is what the
dll draws. The backdrops are procedural (no game picture), only there to judge the art against (dark corridor, bright
fog). Run: py -3.10 tools/hudbars_mockup.py [outdir]   (default ../../renders: w4_hud/renders in the sandbox)
"""
import math
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "renders"))  # (sandbox layout)
FONT = r"C:\Windows\Fonts\arial.ttf"
RNG = np.random.default_rng(330)


def rgb(c):
    return np.array([(c >> 16) & 255, (c >> 8) & 255, c & 255], np.float32) / 255.0


def lerpc(a, b, t):
    """0xRRGGBB lerp (as HbLerpRgb)"""
    t = min(max(t, 0.0), 1.0)
    r = [int(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t + 0.5) for s in (16, 8, 0)]
    return r[0] << 16 | r[1] << 8 | r[2]


# ====================================================================== the primitive layer (HbTri / HbQuad / HbRect)
class Raster:
    """a float RGB image; tri() = one D3D triangle with Gouraud colour + alpha, SRCALPHA / INVSRCALPHA"""

    def __init__(self, img):
        self.a = img.astype(np.float32)
        self.ntri = 0

    def tri(self, p, c):
        (x0, y0), (x1, y1), (x2, y2) = p
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            return
        if area < 0:
            p = [p[0], p[2], p[1]]
            c = [c[0], c[2], c[1]]
            (x0, y0), (x1, y1), (x2, y2) = p
            area = -area
        self.ntri += 1
        H, W = self.a.shape[:2]
        X0, X1 = max(int(math.floor(min(x0, x1, x2))), 0), min(int(math.ceil(max(x0, x1, x2))), W)
        Y0, Y1 = max(int(math.floor(min(y0, y1, y2))), 0), min(int(math.ceil(max(y0, y1, y2))), H)
        if X1 <= X0 or Y1 <= Y0:
            return
        px = np.arange(X0, X1, dtype=np.float64)[None, :] + 0.5
        py = np.arange(Y0, Y1, dtype=np.float64)[:, None] + 0.5

        def edge(ax, ay, bx, by):
            e = (bx - ax) * (py - ay) - (by - ay) * (px - ax)
            topleft = (ay == by and bx > ax) or (by < ay)
            return e, (e > 0) | ((e == 0) & topleft)

        e0, m0 = edge(x1, y1, x2, y2)  # weight of vertex 0
        e1, m1 = edge(x2, y2, x0, y0)
        e2, m2 = edge(x0, y0, x1, y1)
        m = m0 & m1 & m2
        if not m.any():
            return
        w0, w1, w2 = e0 / area, e1 / area, e2 / area
        cc = [np.array(ci, np.float64) for ci in c]
        col = w0[..., None] * cc[0] + w1[..., None] * cc[1] + w2[..., None] * cc[2]
        al = np.clip(col[..., 3:4], 0, 1)
        reg = self.a[Y0:Y1, X0:X1]
        out = reg * (1 - al) + col[..., :3] * al
        self.a[Y0:Y1, X0:X1] = np.where(m[..., None], out, reg)

    def img(self):
        return Image.fromarray((self.a.clip(0, 1) * 255 + 0.5).astype(np.uint8))


GREY_K = 0.0  # v3.32 MERGE: hudbars.inc g_hbGreyK (> 0 while the locked small ring is drawn)


def lock_rgb(c, k):
    """v3.32 MERGE: hudbars_sim.h HbLockRgb: k of the way to the colour's own grey, that grey 10 % darker at k = 1"""
    r, g, b_ = (c >> 16) & 255, (c >> 8) & 255, c & 255
    t = min(max(k, 0.0), 1.0)
    y = (0.299 * r + 0.587 * g + 0.114 * b_) * (1.0 - 0.10 * t)
    o = [int(min(max(x + (y - x) * t, 0.0), 255.0) + 0.5) for x in (r, g, b_)]
    return o[0] << 16 | o[1] << 8 | o[2]


def C(rgbc, a):
    """(0xRRGGBB, alpha) -> vertex colour"""
    if GREY_K > 0.0:
        rgbc = lock_rgb(rgbc, GREY_K)  # v3.32 MERGE (HbArgb)
    v = rgb(rgbc)
    return (float(v[0]), float(v[1]), float(v[2]), float(min(max(a, 0.0), 1.0)))


class Draw:
    """HbRect / HbQuad / HbTri / HbRing over a Raster; skew = the 'blade' look (x shifts by skew x height above base)"""

    def __init__(self, r):
        self.r = r
        self.skew = 0.0
        self.base = 0.0

    def _x(self, x, y):
        return x + self.skew * (self.base - y)

    def quad(self, pts, cols):  # corners TL, TR, BL, BR (HbRect's order)
        q = [(self._x(x, y), y) for x, y in pts]
        self.r.tri([q[0], q[1], q[2]], [cols[0], cols[1], cols[2]])
        self.r.tri([q[1], q[3], q[2]], [cols[1], cols[3], cols[2]])

    def rect(self, x0, y0, x1, y1, ct, cb=None):
        cb = ct if cb is None else cb
        if not (x1 > x0 and y1 > y0):
            return
        self.quad([(x0, y0), (x1, y0), (x0, y1), (x1, y1)], [ct, ct, cb, cb])

    def rect_h(self, x0, y0, x1, y1, cl, cr):  # horizontal gradient
        if not (x1 > x0 and y1 > y0):
            return
        self.quad([(x0, y0), (x1, y0), (x0, y1), (x1, y1)], [cl, cr, cl, cr])

    def tri(self, p, c):
        self.r.tri([(self._x(x, y), y) for x, y in p], c)

    def ring(self, cx, cy, r0, r1, a0, a1, cin, cout, seg_deg=6.0):
        """an annulus band from angle a0 to a1 (radians, 0 = up, clockwise on screen)"""
        n = max(1, int(math.ceil(abs(a1 - a0) / math.radians(seg_deg))))
        for i in range(n):
            t0, t1 = a0 + (a1 - a0) * i / n, a0 + (a1 - a0) * (i + 1) / n
            s0, c0, s1, c1 = math.sin(t0), -math.cos(t0), math.sin(t1), -math.cos(t1)
            self.quad([(cx + r1 * s0, cy + r1 * c0), (cx + r1 * s1, cy + r1 * c1), (cx + r0 * s0, cy + r0 * c0),
                       (cx + r0 * s1, cy + r0 * c1)], [cout, cout, cin, cin])

    def disc(self, cx, cy, r, cc, ce, seg_deg=15.0):
        n = max(6, int(math.ceil(360.0 / seg_deg)))
        for i in range(n):
            t0, t1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            self.tri([(cx, cy), (cx + r * math.sin(t0), cy - r * math.cos(t0)), (cx + r * math.sin(t1), cy - r * math.cos(t1))],
                     [cc, ce, ce])


# ====================================================================== Enemy Health Bars v3.30
# colours (0xRRGGBB): dried blood for monsters, ectoplasm for ghosts, spectral violet for the revive time, steel for a seal
RIM, BACK0, BACK1 = 0x000000, 0x17110F, 0x090706
FILL = {"monster": (0xB0362C, 0x5C0E0B), "boss": (0xC0441E, 0x5A0C06), "ghost": (0x9CC0C0, 0x3A5A60)}
CHIP_HOT, CHIP_COLD = 0xFFF0D8, 0xC2B094       # the damage chip: hot when it lands, bone while it waits / drains
GUARD = 0xDDF4F0                                 # the stagger line (ghosts)
REV = (0xD2C4FF, 0x6A54B0)                       # the revive countdown
STEEL = (0xB4BCC8, 0x4C5460)                     # sealed by the Sword
LOCK = 0xE2D8C8


class Bar:
    def __init__(self, x, y, shown, trail=None, a=1.0, kind="monster", locked=False, boss=False, hold=0.0,
                 guard=None, rise=None, rev=None, rev_a=0.0, seal=0.0, flash=0.0, t=0.0):
        self.__dict__.update(locals())
        self.trail = shown if trail is None else trail


def bar_geom(H, boss):
    W = H * 0.075 * (1.6 if boss else 1.0)
    hh = max(H * 0.0065, 4.0) * (1.25 if boss else 1.0)
    px = max(H / 1080.0, 1.0)
    return W, hh, px


def draw_bar(dw, b, H, look="bevel"):
    """HbBarQuads v3.30. look: 'bevel' (implemented), 'blade' (bevel + a 0.8 skew), 'v329' (the old one)"""
    W, hh, px = bar_geom(H, b.boss)
    if look == "v329":
        hh = max(H * 0.0056, 3.0) * (1.25 if b.boss else 1.0)
    x0, x1, y1 = b.x - 0.5 * W, b.x + 0.5 * W, b.y
    y0 = y1 - hh
    a = b.a * (1.0 if b.locked else 0.85)
    ghost = b.kind == "ghost"
    fk = "boss" if b.boss and not ghost else b.kind
    if look == "v329":
        old = {"monster": (0x9C2A22, 0x560C0A), "boss": (0xA0301E, 0x4E0A08), "ghost": (0x8C9AA6, 0x3E4852)}[fk]
        dw.rect(x0 - px, y0 - px, x1 + px, y1 + px, C(RIM, 0.75 * a))
        dw.rect(x0, y0, x1, y1, C(0x16110E, 0.6 * a))
        if b.trail > b.shown + 0.002:
            dw.rect(x0 + W * b.shown, y0, x0 + W * b.trail, y1, C(CHIP_COLD, 0.7 * a), C(CHIP_COLD, 0.55 * a))
        dw.rect(x0, y0, x0 + W * b.shown, y1, C(old[0], 0.95 * a), C(old[1], 0.95 * a))
        dw.rect(x0, y0, x0 + W * b.shown, y0 + px, C(0xFFFFFF, 0.16 * a))
        return
    dw.skew, dw.base = (0.8 if look == "blade" else 0.0), y1
    # the HP area; a ghost keeps its bottom rows for the stagger line
    gl = max(px, round(hh * 0.22)) if ghost else 0.0
    hy1 = y1 - (gl + px if ghost else 0.0)
    # frame: soft shadow, rim, dark back with a bevel (light top row, dark bottom row)
    dw.rect(x0 - 2 * px, y0 - 2 * px, x1 + 2 * px, y1 + 2 * px, C(RIM, 0.22 * a))
    dw.rect(x0 - px, y0 - px, x1 + px, y1 + px, C(RIM, 0.85 * a))
    dw.rect(x0, y0, x1, y1, C(BACK0, 0.80 * a), C(BACK1, 0.80 * a))
    # the rise preview: the health it gets back when it rises (pulses slowly)
    if b.rise is not None and b.rise > b.shown + 0.004:
        k = 0.40 + 0.12 * math.sin(b.t * 2 * math.pi * 0.8)
        f = FILL[fk]
        dw.rect(x0 + W * b.shown, y0, x0 + W * b.rise, hy1, C(lerpc(f[0], 0xFFFFFF, 0.25), k * a), C(f[1], k * a))
        dw.rect(x0 + W * b.rise - px, y0, x0 + W * b.rise, hy1, C(0xE8FFFF, 0.70 * a))
    # the damage chip: hot when it lands, bone while it drains
    if b.trail > b.shown + 0.002:
        ch = lerpc(CHIP_COLD, CHIP_HOT, b.hold / 0.45)
        dw.rect(x0 + W * b.shown, y0, x0 + W * b.trail, hy1, C(ch, 0.85 * a), C(ch, 0.62 * a))
    # the fill: gradient, a light top row, a darker bottom row
    f = FILL[fk]
    xe = x0 + W * b.shown
    dw.rect(x0, y0, xe, hy1, C(f[0], 0.96 * a), C(f[1], 0.96 * a))
    dw.rect(x0, y0, xe, y0 + px, C(0xFFFFFF, 0.24 * a))
    if hy1 - y0 > 3 * px:
        dw.rect(x0, hy1 - px, xe, hy1, C(0x000000, 0.25 * a))
    # quarter notches (bosses: tenths)
    nq = 10 if b.boss else 4
    for i in range(1, nq):
        xq = x0 + W * i / nq
        dw.rect(xq - 0.5 * px, y0, xq + 0.5 * px, hy1, C(0x000000, (0.45 if i * 2 == nq else 0.30) * a))
    dw.rect(x0, y0, x1, y0 + px, C(0xFFFFFF, 0.07 * a))  # bevel over everything
    # ghosts: the stagger line (the gauge: empty = it falls)
    if ghost and b.guard is not None:
        dw.rect(x0, y1 - gl, x0 + W * b.guard, y1, C(GUARD, 0.85 * a), C(GUARD, 0.55 * a))
    # the locked target: small diamonds at both ends
    if b.locked:
        my, s = 0.5 * (y0 + y1), 2.5 * px + 0.25 * hh
        for cx in (x0 - 3 * px - s, x1 + 3 * px + s):
            o = s + 1.5 * px
            dw.quad([(cx, my - o), (cx + o, my), (cx - o, my), (cx, my + o)], [C(RIM, 0.8 * a)] * 4)
            dw.quad([(cx, my - s), (cx + s, my), (cx - s, my), (cx, my + s)], [C(LOCK, 0.95 * a)] * 4)
    # ghosts: the revive bar under it
    if ghost and (b.rev_a > 0.004):
        revive_bar(dw, b, x0, x1, y1, W, H, px, b.a * b.rev_a)
    dw.skew = 0.0


def revive_bar(dw, b, x0, x1, y1, W, H, px, a):
    rh = max(H * 0.0030, 2.0)
    ry0 = y1 + 3 * px
    ry1 = ry0 + rh
    s = b.seal
    if s < 1.0:  # the countdown: drains to the left, its end glowing
        k = (1.0 - s) * a
        dw.rect(x0 - px, ry0 - px, x1 + px, ry1 + px, C(RIM, 0.80 * k))
        dw.rect(x0, ry0, x1, ry1, C(0x100C18, 0.75 * k))
        xe = x0 + W * (b.rev or 0.0)
        dw.rect(x0, ry0, xe, ry1, C(REV[0], 0.92 * k), C(REV[1], 0.92 * k))
        if b.rev and b.rev > 0.01:
            dw.rect(xe - px, ry0 - px, xe + px, ry1 + px, C(0xF4EEFF, 0.75 * k))
    if s > 0.0:  # sealed: the bar broken in two steel halves, slashed, the Sword's cross in the gap
        k = s * a
        g = 3 * px + 2 * px * (1.0 - s)   # the halves close in as the seal lands
        xc = 0.5 * (x0 + x1)
        for hx0, hx1 in ((x0, xc - g), (xc + g, x1)):
            dw.rect(hx0 - px, ry0 - px, hx1 + px, ry1 + px, C(RIM, 0.85 * k))
            st0, st1 = lerpc(STEEL[0], 0xFFFFFF, b.flash), lerpc(STEEL[1], 0xFFFFFF, 0.7 * b.flash)
            dw.rect(hx0, ry0, hx1, ry1, C(st0, 0.80 * k), C(st1, 0.80 * k))
            n = 3
            for i in range(n):
                sx = hx0 + (hx1 - hx0) * (i + 0.7) / (n + 0.4)
                dw.quad([(sx + rh, ry0), (sx + rh + px, ry0), (sx, ry1), (sx + px, ry1)], [C(0x000000, 0.55 * k)] * 4)
        # the cross: a blade through the gap and a guard
        dw.rect(xc - 1.5 * px, ry0 - 3 * px, xc + 1.5 * px, ry1 + 3 * px, C(RIM, 0.6 * k))
        dw.rect(xc - 0.5 * px, ry0 - 3 * px, xc + 0.5 * px, ry1 + 2 * px, C(0xF2F4F8, 0.95 * k), C(0x9AA2AE, 0.95 * k))
        dw.rect(xc - 2.5 * px, ry0 - 2 * px, xc + 2.5 * px, ry0 - px, C(0xF2F4F8, 0.95 * k))


# ====================================================================== the Curse Meter
STAGES = ["Untouched", "Haunted", "Cursed", "Possessed"]
CSEG = [(0x7A5864, 0x34222A), (0x9A3050, 0x420E20), (0xB01A30, 0x500610), (0xD0182C, 0x640812)]  # ash rose .. blood
CGLOW = [0xC8A8B4, 0xF06A8C, 0xFF5060, 0xFF3040]
STAGE_TEXT = [0x505050, 0x604C58, 0x50345C, 0x1C1C64]  # 0xBBGGRR, 0x80 = full (the game's text colour)
REST = 0.55


def canvas(w, h, S):
    sx, sy = w / (512.0 * S), h / 448.0
    return (lambda cx: 0.5 * w + (cx - 256.0) * sx), (lambda cy: cy * sy), sx, sy


def stage_of(v, th):
    s = 0
    for i in range(3):
        if 0 < th[i] <= 1 and v >= th[i]:
            s = i + 1
    return s


def seg_colour(stage, th):
    return CSEG[stage]


def heartbeat(t):
    """the Possessed beat: two quick pulses a second (0..1)"""
    p = (t * 1.1) % 1.0
    return max(math.exp(-((p - 0.08) / 0.05) ** 2), 0.7 * math.exp(-((p - 0.30) / 0.06) ** 2))


def curse_ring(dw, w, h, S, v, th, minor, a, pulse=0.0, pulse_up=True, t=0.0, full_latched=False, cxy=(452.0, 42.0),
               Rc=17.0, k=1.0):
    """design R (implemented): a 270-degree ring of thorns around a sigil, top right. v3.32 DHS332: cxy / Rc = the
    centre and radius in canvas units, k = the size scale of the pixel-sized parts (1 = the main ring; HbRingDraw)"""
    X, Y, sx, sy = canvas(w, h, S)
    cx, cy = X(cxy[0]), Y(cxy[1])
    R = Rc * sy                         # outer radius of the band (pixels; circular whatever the canvas squeeze)
    bw = 3.6 * k * sy
    r0, r1 = R - bw, R
    px = max(h / 1080.0, 1.0)
    sd = 6.0 / k                        # the ring segments (C: 0.105 rad / k)
    st = stage_of(v, th)
    A0, SPAN = -0.75 * math.pi, 1.5 * math.pi
    beat = heartbeat(t) if st == 3 else 0.0
    # backing disc, shadow ring, rim
    dw.disc(cx, cy, R + 3 * k * px, C(0x000000, 0.35 * a), C(0x000000, 0.0))
    dw.ring(cx, cy, r0 - px, r1 + px, A0 - 0.03, A0 + SPAN + 0.03, C(RIM, 0.85 * a), C(RIM, 0.85 * a), sd)
    dw.ring(cx, cy, r0, r1, A0, A0 + SPAN, C(0x140C10, 0.80 * a), C(0x0A0608, 0.80 * a), sd)
    # the fill: piece by piece in the colour of the stage it belongs to (all blood when a latched full is reached)
    bs, bst = [0.0], [0]
    for i in range(3):
        if bs[-1] < th[i] < 1.0:
            bs.append(th[i])
            bst.append(i + 1)
    bs.append(1.0)
    for j in range(len(bst)):
        e = min(v, bs[j + 1])
        if e <= bs[j]:
            continue
        ss = 3 if (st == 3 and full_latched) else bst[j]
        c0, c1 = CSEG[ss]
        kk = 0.95 * a * (1.0 + 0.25 * beat)
        c0 = lerpc(c0, CGLOW[ss], 0.25 * beat + 0.35 * pulse)
        dw.ring(cx, cy, r0, r1, A0 + SPAN * bs[j], A0 + SPAN * e, C(c1, kk), C(c0, kk), sd)
    # inner light row and the leading edge
    if v > 0.004:
        dw.ring(cx, cy, r1 - px, r1, A0, A0 + SPAN * v, C(0xFFFFFF, 0.18 * a), C(0xFFFFFF, 0.18 * a), sd)
        ang = A0 + SPAN * v
        dw.ring(cx, cy, r0 - px, r1 + px, ang - 0.035, ang + 0.0, C(0xFFE0E8, 0.80 * a), C(0xFFE0E8, 0.80 * a), sd)
    # thorns: one at each stage mark, pointing out; lit (stage colour) when passed
    for i in range(3):
        m = th[i]
        if not (0 < m < 1):
            continue
        ang = A0 + SPAN * m
        lit = v >= m
        sn, cs = math.sin(ang), -math.cos(ang)
        tx, ty = -cs, sn  # tangent
        L = k * 4.2 * px + 0.10 * R
        base = r1 + 0.5 * px
        wv = k * 1.6 * px + 0.04 * R
        p0 = (cx + sn * base + tx * wv, cy + cs * base + ty * wv)
        p1 = (cx + sn * base - tx * wv, cy + cs * base - ty * wv)
        p2 = (cx + sn * (base + L), cy + cs * (base + L))
        col = CGLOW[i + 1] if lit else 0x6A6070
        dw.tri([p0, p1, p2], [C(col, 0.9 * a), C(col, 0.9 * a), C(lerpc(col, 0xFFFFFF, 0.3), 0.9 * a)])
        # the cut through the band
        dw.ring(cx, cy, r0, r1, ang - 0.018, ang + 0.018, C(RIM, 0.9 * a), C(RIM, 0.9 * a), sd)
    if minor > 0:
        ang = A0 + SPAN * minor
        dw.ring(cx, cy, r0 - 2.5 * px, r0 - 1.0 * px, ang - 0.03, ang + 0.03, C(0xCFC4D8, 0.4 * a), C(0xCFC4D8, 0.4 * a), sd)
    # the sigil: a dark well with an eye of the stage colour that opens with the curse
    dw.disc(cx, cy, r0 - 2.0 * k * px, C(0x0C080A, 0.80 * a), C(0x1A1014, 0.80 * a))
    ec = CGLOW[st]
    open_ = [0.15, 0.40, 0.70, 1.0][st]
    ew, eh = (r0 - 4 * k * px) * 0.85, (r0 - 4 * k * px) * 0.85 * (0.18 + 0.42 * open_)
    glow = (0.45 + 0.35 * open_) * a * (1.0 + 0.6 * beat + 0.8 * pulse)
    dw.quad([(cx, cy - eh), (cx + ew, cy), (cx - ew, cy), (cx, cy + eh)],
            [C(lerpc(ec, 0x000000, 0.4), glow), C(ec, 0.0), C(ec, 0.0), C(lerpc(ec, 0x000000, 0.4), glow)])
    dw.disc(cx, cy, max(1.5 * k * px, eh * 0.55), C(lerpc(ec, 0xFFFFFF, 0.35), min(1.0, glow * 1.4)), C(ec, glow * 0.6))
    # the stage-cross pulse: a ring that swells out and fades (red up, cold down)
    if pulse > 0.0:
        q = 1.0 - pulse
        rr = R + (2.0 + 16.0 * q) * k * px
        pc = CGLOW[st] if pulse_up else 0xB8C8E0
        dw.ring(cx, cy, rr - 2.5 * k * px, rr, -math.pi, math.pi, C(pc, 0.0), C(pc, 0.85 * pulse * a), 8.0 / k)
        dw.ring(cx, cy, rr, rr + 2.5 * k * px, -math.pi, math.pi, C(pc, 0.85 * pulse * a), C(pc, 0.0), 8.0 / k)
    return st, (X(452.0), Y(63.0))


def curse_vial(dw, w, h, S, v, th, minor, a, pulse=0.0, pulse_up=True, t=0.0, full_latched=False):
    """design V: a vertical glass vial, top right, filling with dark blood; stage pips on its left"""
    X, Y, sx, sy = canvas(w, h, S)
    px = max(h / 1080.0, 1.0)
    cxm = X(468.0)
    vw = 9.0 * sy
    x0, x1 = cxm - vw / 2, cxm + vw / 2
    yt, yb = Y(24.0), Y(70.0)
    st = stage_of(v, th)
    beat = heartbeat(t) if st == 3 else 0.0
    rr = vw / 2
    # neck and cork
    dw.rect(cxm - rr * 0.55 - px, yt - 5 * px - px, cxm + rr * 0.55 + px, yt + px, C(RIM, 0.85 * a))
    dw.rect(cxm - rr * 0.55, yt - 5 * px, cxm + rr * 0.55, yt, C(0x3A2C24, 0.9 * a), C(0x1A120E, 0.9 * a))
    # body: rim, glass back, round bottom
    dw.rect(x0 - px, yt - px, x1 + px, yb, C(RIM, 0.85 * a))
    dw.ring(cxm, yb, 0, rr + px, 0.5 * math.pi, 1.5 * math.pi, C(RIM, 0.85 * a), C(RIM, 0.85 * a), 10)
    dw.rect(x0, yt, x1, yb, C(0x140C10, 0.78 * a), C(0x0A0608, 0.78 * a))
    dw.ring(cxm, yb, 0, rr, 0.5 * math.pi, 1.5 * math.pi, C(0x0A0608, 0.78 * a), C(0x0A0608, 0.78 * a), 10)
    # liquid: from the bottom of the round end up to the level
    total = (yb - yt) + rr
    lvl = yb + rr - total * v
    c0, c1 = CSEG[3 if (st == 3 and full_latched) else st]
    c0 = lerpc(c0, CGLOW[st], 0.25 * beat + 0.35 * pulse)
    k = 0.95 * a
    if v > 0:
        dw.ring(cxm, yb, 0, rr, 0.5 * math.pi, 1.5 * math.pi, C(c1, k), C(c1, k), 10)
        top = max(lvl, yt)
        if top < yb:
            dw.rect(x0, top, x1, yb, C(c0, k), C(c1, k))
            dw.rect(x0, top, x1, top + px, C(0xFFD0D8, 0.75 * a))  # meniscus
    # glass highlight
    dw.rect(x0 + 1.5 * px, yt + 2 * px, x0 + 2.5 * px, yb - 2 * px, C(0xFFFFFF, 0.16 * a), C(0xFFFFFF, 0.04 * a))
    # stage pips on the left
    for i in range(3):
        m = th[i]
        if not (0 < m < 1):
            continue
        y = yb + rr - total * m
        lit = v >= m
        col = CGLOW[i + 1] if lit else 0x6A6070
        s = 2.2 * px
        xx = x0 - 4 * px
        dw.quad([(xx, y - s), (xx + s, y), (xx - s, y), (xx, y + s)], [C(col, 0.9 * a)] * 4)
        dw.rect(x0, y - 0.5 * px, x0 + 0.35 * vw, y + 0.5 * px, C(RIM, 0.8 * a))
    if pulse > 0.0:
        pc = CGLOW[st] if pulse_up else 0xB8C8E0
        g = (1.0 - pulse) * 10 * px
        dw.rect(x0 - 2 * px - g, yt - 2 * px - g, x1 + 2 * px + g, yb + rr + 2 * px + g, C(pc, 0.0))
        for (ax0, ay0, ax1, ay1) in ((x0 - 2 * px - g, yt - 2 * px - g, x0 - g, yb + rr + g),
                                     (x1 + g, yt - 2 * px - g, x1 + 2 * px + g, yb + rr + g)):
            dw.rect(ax0, ay0, ax1, ay1, C(pc, 0.7 * pulse * a))
    return st, (X(444.0) - vw, Y(36.0))


def curse_thorn(dw, w, h, S, v, th, minor, a, pulse=0.0, pulse_up=True, t=0.0, full_latched=False):
    """design T: the v3.29 horizontal bar made taller, cut into stage segments with diamond pips and thorns"""
    X, Y, sx, sy = canvas(w, h, S)
    px = max(h / 1080.0, 1.0)
    X0, X1, Y0, Y1 = X(392.0), X(476.0), Y(29.0), Y(33.5)
    st = stage_of(v, th)
    beat = heartbeat(t) if st == 3 else 0.0
    dw.rect(X0 - 2 * px, Y0 - 2 * px, X1 + 2 * px, Y1 + 2 * px, C(0, 0.25 * a))
    dw.rect(X0 - px, Y0 - px, X1 + px, Y1 + px, C(RIM, 0.85 * a))
    dw.rect(X0, Y0, X1, Y1, C(0x140C10, 0.78 * a), C(0x0A0608, 0.78 * a))
    bs, bst = [0.0], [0]
    for i in range(3):
        if bs[-1] < th[i] < 1.0:
            bs.append(th[i])
            bst.append(i + 1)
    bs.append(1.0)
    for j in range(len(bst)):
        e = min(v, bs[j + 1])
        if e <= bs[j]:
            continue
        ss = 3 if (st == 3 and full_latched) else bst[j]
        c0, c1 = CSEG[ss]
        c0 = lerpc(c0, CGLOW[ss], 0.25 * beat + 0.35 * pulse)
        dw.rect(X0 + (X1 - X0) * bs[j], Y0, X0 + (X1 - X0) * e, Y1, C(c0, 0.95 * a), C(c1, 0.95 * a))
        # thorns under the filled part (more with the stage)
        n = int((e - bs[j]) * 40)
        for q in range(n):
            tx = X0 + (X1 - X0) * (bs[j] + (q + 0.5) / 40.0)
            L = (1.0 + 0.8 * ss) * px
            dw.tri([(tx - 1.2 * px, Y1), (tx + 1.2 * px, Y1), (tx + 0.3 * px, Y1 + L)], [C(c1, 0.9 * a)] * 3)
    dw.rect(X0, Y0, X0 + (X1 - X0) * v, Y0 + px, C(0xFFFFFF, 0.2 * a))
    for i in range(3):
        m = th[i]
        if not (0 < m < 1):
            continue
        mx = X0 + (X1 - X0) * m
        dw.rect(mx - px, Y0, mx + px, Y1, C(RIM, 0.95 * a))
        lit = v >= m
        col = CGLOW[i + 1] if lit else 0x6A6070
        s = 2.4 * px
        dw.quad([(mx, Y0 - 3 * px - 2 * s), (mx + s, Y0 - 3 * px - s), (mx - s, Y0 - 3 * px - s), (mx, Y0 - 3 * px)],
                [C(col, 0.9 * a)] * 4)
    if pulse > 0.0:
        pc = CGLOW[st] if pulse_up else 0xB8C8E0
        g = (1.0 - pulse) * 8 * px
        dw.rect(X0 - g, Y0 - g, X1 + g, Y1 + g, C(pc, 0.45 * pulse * a))
    return st, (X(434.0), Y(38.0))


def label(img, w, h, S, stage, alpha, pos):
    """the stage name: the game's font 1 approximated (Arial, ~19 canvas units), centred on pos, shadow at +2, +2"""
    _, _, sx, sy = canvas(w, h, S)
    f = ImageFont.truetype(FONT, int(round(17.0 * sy)))
    d = ImageDraw.Draw(img, "RGBA")
    s = STAGES[stage]
    tw = d.textlength(s, font=f)
    x, y = pos[0] - tw / 2, pos[1]
    d.text((x + 2 * sx, y + 2 * sy), s, font=f, fill=(0, 0, 0, int(200 * alpha)))
    b, g, r = (STAGE_TEXT[stage] >> 16) & 255, (STAGE_TEXT[stage] >> 8) & 255, STAGE_TEXT[stage] & 255
    c = [min(255, int(q / 128.0 * 255)) for q in (r, g, b)]
    d.text((x, y), s, font=f, fill=(c[0], c[1], c[2], int(255 * alpha)))


# ====================================================================== backdrops (procedural)
def backdrop(w, h, bright=False):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    u, v = xx / w, yy / h
    base = np.stack([0.075 + 0.02 * v, 0.080 + 0.02 * v, 0.072 + 0.015 * v], -1)
    hz = 0.46
    fl = v > hz
    d = np.where(fl, (v - hz) / (1 - hz), 0)
    z = np.where(fl, 1.0 / np.maximum(d, 1e-3), 0)
    tx = (u - 0.5) * z * 2.2
    tiles = (np.abs(((tx * 2) % 1) - 0.5) < 0.012 * z) | (np.abs(((z * 1.6) % 1) - 0.5) < 0.012 * z)
    floor = np.stack([0.11, 0.115, 0.10], -1) * (0.5 + 0.7 * d[..., None])
    base = np.where(fl[..., None], floor, base)
    base = np.where((fl & tiles)[..., None] & (d[..., None] > 0.02), base * 0.7, base)
    base = np.where((u < 0.16)[..., None], base * 0.55, base)
    base = np.where((u > 0.86)[..., None], base * 0.6, base)
    door = (u > 0.40) & (u < 0.52) & (v > 0.18) & (v < hz + 0.01)
    base = np.where(door[..., None], base * 0.35, base)
    light = np.exp(-(((u - 0.47) / 0.30) ** 2 + ((v - 0.70) / 0.25) ** 2))
    base *= (0.7 + 0.9 * light)[..., None]
    vig = np.clip(1.15 - 0.85 * ((u - 0.5) ** 2 + (v - 0.5) ** 2) ** 0.5 * 1.6, 0.25, 1)
    base *= vig[..., None]
    if bright:  # a white-out: fog and a lit window (the hard case for the bars)
        fog = 0.55 + 0.35 * np.exp(-(((u - 0.55) / 0.5) ** 2 + ((v - 0.35) / 0.4) ** 2))
        base = base * 0.35 + np.stack([fog * 0.86, fog * 0.86, fog * 0.84], -1)
        win = (u > 0.62) & (u < 0.80) & (v > 0.12) & (v < 0.40)
        base = np.where(win[..., None], np.array([0.97, 0.96, 0.93]), base)
    base += RNG.normal(0, 0.018, (h, w, 1)).astype(np.float32)
    return np.clip(base, 0, 1)


def silhouettes(img, figs, tint=(0.02, 0.018, 0.02)):
    m = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(m)
    for kind, x, fy, ht in figs:
        if kind in ("human", "ghost"):
            w = ht * 0.22
            d.ellipse([x - w * 0.28, fy - ht, x + w * 0.28, fy - ht * 0.84], fill=255)
            d.polygon([(x - w * 0.55, fy - ht * 0.82), (x + w * 0.55, fy - ht * 0.82), (x + w * 0.42, fy - ht * 0.42),
                       (x + w * 0.30, fy), (x + w * 0.08, fy), (x, fy - ht * 0.40), (x - w * 0.08, fy), (x - w * 0.30, fy),
                       (x - w * 0.42, fy - ht * 0.42)], fill=255 if kind == "human" else 170)
        elif kind == "dog":
            L = ht * 2.3
            d.ellipse([x - L * 0.45, fy - ht * 0.95, x + L * 0.30, fy - ht * 0.45], fill=255)
            d.ellipse([x + L * 0.22, fy - ht * 1.05, x + L * 0.50, fy - ht * 0.70], fill=255)
            for lx in (-0.38, -0.25, 0.12, 0.24):
                d.rectangle([x + L * lx, fy - ht * 0.6, x + L * lx + L * 0.05, fy], fill=255)
        elif kind == "down":
            d.ellipse([x - ht * 0.9, fy - ht * 0.22, x + ht * 0.9, fy], fill=200)
            d.ellipse([x + ht * 0.8, fy - ht * 0.28, x + ht * 1.05, fy - ht * 0.02], fill=200)
        elif kind == "pinned":
            d.ellipse([x - ht * 0.9, fy - ht * 0.22, x + ht * 0.9, fy], fill=200)
            d.rectangle([x - 3, fy - ht * 1.3, x + 3, fy - ht * 0.1], fill=255)
            d.rectangle([x - 18, fy - ht * 1.12, x + 18, fy - ht * 1.08], fill=255)
    mb = np.asarray(m.filter(ImageFilter.GaussianBlur(1.2)), np.float32)[..., None] / 255.0
    rim = np.asarray(m.filter(ImageFilter.GaussianBlur(5)), np.float32)[..., None] / 255.0
    a = np.asarray(img, np.float32) / 255.0
    a = a * (1 - 0.92 * mb) + np.array(tint) * 0.92 * mb
    a += np.clip(rim - mb, 0, 1) * np.array([0.10, 0.10, 0.11])
    return Image.fromarray((a.clip(0, 1) * 255 + 0.5).astype(np.uint8))


def scene(w, h, bright=False, meter="ring", look="bevel", t=0.4):
    S = (w / h) / (4.0 / 3.0) if w / h > 4.0 / 3.0 + 1e-3 else 1.0
    k = h / 1080.0
    cxs = w / 2.0
    figs = [("human", cxs + 220 * k, 760 * k, 330 * k), ("dog", cxs - 270 * k, 830 * k, 150 * k),
            ("down", cxs + 500 * k, 905 * k, 150 * k), ("ghost", cxs - 55 * k, 640 * k, 200 * k),
            ("pinned", cxs - 560 * k, 940 * k, 120 * k)]
    img = Image.fromarray((backdrop(w, h, bright) * 255 + 0.5).astype(np.uint8))
    img = silhouettes(img, figs)
    r = Raster(np.asarray(img, np.float32) / 255.0)
    dw = Draw(r)
    gap = h * 0.012
    bars = [Bar(cxs + 220 * k, 760 * k - 330 * k - 40 * k - gap, 0.46, 0.71, 1.0, "monster", locked=True, hold=0.3),
            Bar(cxs - 270 * k, 830 * k - 150 * k * 1.05 - 18 * k - gap, 0.80, a=0.85),
            Bar(cxs + 530 * k, 905 * k - 60 * k - gap, 0.0, 0.0, 1.0, "ghost", guard=0.0, rise=0.5, rev=0.62, rev_a=1.0, t=t),
            Bar(cxs - 55 * k, 640 * k - 200 * k - 25 * k - gap, 0.74, a=0.85, kind="ghost", guard=0.55),
            Bar(cxs - 560 * k, 940 * k - 150 * k - gap, 0.0, 0.0, 1.0, "ghost", guard=0.0, rev=0.0, rev_a=1.0, seal=1.0)]
    for b in bars:
        draw_bar(dw, b, h, look)
    fn = {"ring": curse_ring, "vial": curse_vial, "thorn": curse_thorn}[meter]
    st, pos = fn(dw, w, h, S, 0.58, (0.25, 0.5, 0.7), 0.40, 1.0, t=t)
    out = r.img()
    label(out, w, h, S, st, 1.0, pos)
    return out


# ====================================================================== sheets
def bar_sheet(look):
    Ws, Hs = 1500, 1660
    r = Raster(np.full((Hs, Ws, 3), 0.085, np.float32) + RNG.normal(0, 0.010, (Hs, Ws, 1)).astype(np.float32))
    dw = Draw(r)
    Hb = 1080 * 3.0
    rows = [
        ("Monster (All Near, 80 %)", Bar(0, 0, 0.8, a=0.85)),
        ("Monster just hit: the chip lands hot, waits 0.45 s, drains", Bar(0, 0, 0.46, 0.71, 0.85, hold=0.4)),
        ("Locked target (diamonds at both ends)", Bar(0, 0, 0.46, 0.60, 1.0, locked=True, hold=0.0)),
        ("Ghost standing: HP + stagger line (its gauge: empty = it falls)", Bar(0, 0, 0.74, a=0.85, kind="ghost", guard=0.55)),
        ("Ghost stumble (class 9, rises at once): HP stays 62 %, gauge empty, preview +20 %",
         Bar(0, 0, 0.62, a=1.0, kind="ghost", guard=0.0, rise=0.82, t=0.1)),
        ("Ghost down for real (HP 0): revive bar 62 % left, preview: back at 50 %",
         Bar(0, 0, 0.0, a=1.0, kind="ghost", guard=0.0, rise=0.5, rev=0.62, rev_a=1.0, t=0.1)),
        ("Ghost down near a Holy Candle (aura): no heal on rising, no preview",
         Bar(0, 0, 0.0, a=1.0, kind="ghost", guard=0.0, rise=None, rev=0.93, rev_a=1.0)),
        ("Pinned by the Sword of Obedience: the revive bar sealed, broken", Bar(0, 0, 0.0, a=1.0, kind="ghost", guard=0.0,
                                                                              rev=0.4, rev_a=1.0, seal=1.0)),
        ("The pin landing (0.15 s): halves closing, flash", Bar(0, 0, 0.0, 0.3, 1.0, kind="ghost", guard=0.0, rev=0.4,
                                                              rev_a=1.0, seal=0.5, flash=0.7, hold=0.4)),
        ("Boss (Walter, the Wall boss): wider, tenths", Bar(0, 0, 0.64, 0.75, 0.85, boss=True, hold=0.1)),
        ("Fading out (dead, or 4 s after the last hit)", Bar(0, 0, 0.0, 0.22, 0.4)),
    ]
    labels = []
    for i, (name, b) in enumerate(rows):
        b.y = 150 + i * 140
        b.x = 330 + 0.5 * Hb * 0.075 * (1.6 if b.boss else 1.0)
        draw_bar(dw, b, Hb, look)
        labels.append((60, b.y - 92, name))
    out = r.img()
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 20), ImageFont.truetype(FONT, 30)
    d.text((60, 20), "Enemy Health Bars v3.30, look '%s' (x3 of 1080p)" % look, font=ft, fill=(200, 196, 188))
    for x, y, t in labels:
        d.text((x, y), t, font=fs, fill=(150, 146, 140))
    return out, r.ntri


def bar_compare():
    """the same three bars in the three looks, x3, on dark and on bright"""
    Ws, Hs = 1800, 900
    out = Image.new("RGB", (Ws, Hs))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 22)
    for li, look in enumerate(("v329", "bevel", "blade")):
        for bi, bright in enumerate((False, True)):
            x0, y0 = li * 600, bi * 450
            bg = np.full((450, 600, 3), 0.80 if bright else 0.07, np.float32) + RNG.normal(0, 0.01, (450, 600, 1)).astype(np.float32)
            r = Raster(bg)
            dw = Draw(r)
            Hb = 1080 * 3.0
            bl = [Bar(300, 120, 0.46, 0.71, 1.0, locked=True, hold=0.3),
                  Bar(300, 240, 0.0, 0.0, 1.0, "ghost", guard=0.0, rise=0.5, rev=0.62, rev_a=1.0, t=0.1),
                  Bar(300, 360, 0.0, 0.0, 1.0, "ghost", guard=0.0, rev=0.4, rev_a=1.0, seal=1.0)]
            for b in bl:
                if look == "v329" and b.kind == "ghost":
                    b.shown = b.trail = 0.62 if b.seal == 0 else 0.0  # v3.29 showed the countdown
                draw_bar(dw, b, Hb, look)
            out.paste(r.img(), (x0, y0))
            d.text((x0 + 12, y0 + 10), "%s, %s" % (look, "bright" if bright else "dark"), font=fs,
                   fill=(40, 40, 40) if bright else (200, 196, 188))
    return out


def curse_sheet():
    designs = [("ring", curse_ring), ("vial", curse_vial), ("thorn", curse_thorn)]
    cases = [("Companion 0.12 Untouched", 0.12, (0.25, 0.5, 0.7), 0.4, REST, 0.0),
             ("Companion 0.33 Haunted", 0.33, (0.25, 0.5, 0.7), 0.4, 1.0, 0.0),
             ("Companion 0.58 Cursed", 0.58, (0.25, 0.5, 0.7), 0.4, 1.0, 0.0),
             ("Companion 0.84 Possessed (beat)", 0.84, (0.25, 0.5, 0.7), 0.4, 1.0, 0.0),
             ("Crossing 0.50 (pulse at 0.35 s)", 0.51, (0.25, 0.5, 0.7), 0.4, 1.0, 0.6),
             ("Playable Eileen 42 Haunted", 0.42, (0.35, 0.5, 1.0), -1, 1.0, 0.0),
             ("Playable Eileen 100 (latched)", 1.0, (0.35, 0.5, 1.0), -1, 1.0, 0.0)]
    Wf, Hf = 1920, 1080
    Sf = (16 / 9) / (4 / 3)
    cw, ch = 330, 240
    out = Image.new("RGB", (60 + cw * len(cases), 70 + (ch + 40) * len(designs)), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 18), ImageFont.truetype(FONT, 28)
    d.text((20, 15), "Curse Meter designs (crop of 1080p 16:9, 1:1): R ring of thorns, V vial, T thorned bar", font=ft,
           fill=(200, 196, 188))
    for di, (dn, fn) in enumerate(designs):
        for ci, (name, v, th, minor, strength, pulse) in enumerate(cases):
            r = Raster(np.full((Hf // 4, Wf, 3), 0.085, np.float32))
            dw = Draw(r)
            full = th[2] >= 1.0 and v >= 1.0
            st, pos = fn(dw, Wf, Hf, Sf, v, th, minor, strength, pulse=pulse, t=0.05, full_latched=full)
            im = r.img()
            label(im, Wf, Hf, Sf, st, 1.0 if strength > 0.6 else 0.6, pos)
            X, Y, sx, sy = canvas(Wf, Hf, Sf)
            box = (int(X(452)) - 200, 0, int(X(452)) + 130, 240) if dn != 'ring' else (int(X(452)) - 165, 0, int(X(452)) + 165, 240)
            crop = im.crop(box)
            y = 70 + di * (ch + 40)
            out.paste(crop, (40 + ci * cw, y + 26))
            d.text((40 + ci * cw, y), "%s: %s" % (dn.upper()[0], name), font=fs, fill=(160, 156, 150))
    return out


def ring_steps():
    """the chosen ring through one stage crossing (pulse every 0.1 s) and the Possessed beat"""
    Wf, Hf, Sf = 1920, 1080, (16 / 9) / (4 / 3)
    out = Image.new("RGB", (40 + 8 * 200, 2 * 200 + 80), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 18)
    for row, (v, th, label_) in enumerate(((0.51, (0.25, 0.5, 0.7), "crossing into Cursed"),
                                           (0.86, (0.25, 0.5, 0.7), "Possessed: the beat"))):
        for i in range(8):
            t = i * 0.12
            pulse = max(0.0, 1.0 - t / 0.9) if row == 0 else 0.0
            r = Raster(np.full((Hf // 4, Wf, 3), 0.085, np.float32))
            dw = Draw(r)
            st, pos = curse_ring(dw, Wf, Hf, Sf, v, th, 0.4, 1.0, pulse=pulse, t=t)
            im = r.img()
            label(im, Wf, Hf, Sf, st, 1.0, pos)
            X, Y, sx, sy = canvas(Wf, Hf, Sf)
            crop = im.crop((int(X(452)) - 100, 0, int(X(452)) + 100, 200))
            out.paste(crop, (20 + i * 200, 50 + row * 220))
            d.text((20 + i * 200, 28 + row * 220), "%s t=%.2f" % (label_ if i == 0 else "", t), font=fs, fill=(160, 156, 150))
    return out


# ====================================================================== v3.32 DHS332: Curse Meter "Both"
# With Playable Eileen loaded the main ring is the player's own curse (PE's meter) and a second, smaller ring left of it
# shows companion Eileen's game curse (w+0x0C, tiers 0.25 / 0.5 / 0.7, the 0.4 mark), labelled EILEEN in a small
# stroke font (no game font: the game's text is as tall as the small ring). Mirrors src/hudbars.inc HbRingDraw /
# HbTinyText and hudbars_sim.h HB_RING2_* / kHbGlyph.
RING2 = dict(cx=414.0, cy=36.0, R=10.5, k=0.62)   # HB_RING2_CX / _CY / _R / _K (canvas units)
TAG_H = 4.6                                      # HB_TAG_H: the label's letter height (canvas units), 5 grid units
TAG_GAP = 3.4                                    # HB_TAG_GAP: from the ring's bottom to the label's top
TAG_COL = 0xCDBFC6                               # HB_TAG_RGB: pale bone with a rose cast
GLYPH = {  # kHbGlyph: advance, strokes (x0, y0, x1, y1, slant) on a 5-unit-tall grid, y down; slant shifts the bottom right
    'E': (3, [(0, 0, 1, 5, 0), (1, 0, 3, 1, 0), (1, 2, 2.6, 3, 0), (1, 4, 3, 5, 0)]),
    'I': (1, [(0, 0, 1, 5, 0)]),
    'L': (3, [(0, 0, 1, 5, 0), (1, 4, 3, 5, 0)]),
    'N': (4, [(0, 0, 1, 5, 0), (3, 0, 4, 5, 0), (0.5, 0, 1.5, 5, 2)]),
    # v3.32 MERGE: the LOCKED tag's letters (a negative slant leans the bottom left)
    'O': (4, [(0, 1, 1, 4, 0), (3, 1, 4, 4, 0), (1, 0, 3, 1, 0), (1, 4, 3, 5, 0)]),
    'C': (3, [(0, 1, 1, 4, 0), (1, 0, 3, 1, 0), (1, 4, 3, 5, 0)]),
    'K': (3, [(0, 0, 1, 5, 0), (2, 0, 3, 2.5, -1), (1, 2.5, 2, 5, 1)]),
    'D': (4, [(0, 0, 1, 5, 0), (1, 0, 3, 1, 0), (1, 4, 3, 5, 0), (3, 1, 4, 4, 0)]),
}
LOCK_H, LOCK_GAP, LOCK_COL, LOCK_DIM = 3.6, 1.6, 0xB8C4D0, 0.30  # v3.32 MERGE: HB_LOCK_H / _GAP / HB_LOCK_RGB / HB_LOCK_DIM


def tiny_width(text):
    """HbTinyWidth: grid units, one unit between letters"""
    n = [GLYPH[c][0] for c in text if c in GLYPH]
    return sum(n) + max(0, len(n) - 1)


def tiny_text(dw, cx, top, u, text, col, a, px):
    """HbTinyText: centred on cx, top edge at top, u pixels a grid unit; a dark shadow one pixel down-right first"""
    x0 = cx - 0.5 * tiny_width(text) * u
    for pas in (0, 1):
        x = x0
        for ch in text:
            if ch not in GLYPH:
                continue
            adv, strokes = GLYPH[ch]
            for (a0, b0, a1, b1, sl) in strokes:
                o = px if pas == 0 else 0.0
                c = C(0x000000, 0.70 * a) if pas == 0 else C(col, 0.92 * a)
                X0, X1, Y0, Y1 = x + a0 * u + o, x + a1 * u + o, top + b0 * u + o, top + b1 * u + o
                d = sl * u
                dw.quad([(X0, Y0), (X1, Y0), (X0 + d, Y1), (X1 + d, Y1)], [c] * 4)
            x += (adv + 1) * u


def curse_both(dw, w, h, S, vP, thP, vC, aP, aC, pulseP=0.0, pulseC=0.0, upC=True, t=0.0, latched=False, lockK=0.0):
    """the main ring (the player's curse) and the companion's small ring with its label; v3.32 MERGE: lockK = the
    locked grey (0 normal .. 1 locked: grey, dim, LOCKED under the label)"""
    global GREY_K
    st, pos = curse_ring(dw, w, h, S, vP, thP, -1, aP, pulse=pulseP, t=t, full_latched=latched)
    stc = None
    if aC > 0.004:
        X, Y, sx, sy = canvas(w, h, S)
        px = max(h / 1080.0, 1.0)
        a2 = aC * (1.0 - LOCK_DIM * min(max(lockK, 0.0), 1.0))
        GREY_K = lockK
        stc, _ = curse_ring(dw, w, h, S, vC, (0.25, 0.5, 0.7), 0.40, a2, pulse=pulseC, pulse_up=upC, t=t,
                            cxy=(RING2['cx'], RING2['cy']), Rc=RING2['R'], k=RING2['k'])
        u = TAG_H / 5.0 * sy
        top = Y(RING2['cy'] + RING2['R'] + TAG_GAP)
        tiny_text(dw, X(RING2['cx']), top, u, "EILEEN", TAG_COL, a2 * 0.9, px)
        GREY_K = 0.0
        if lockK > 0.004:
            tiny_text(dw, X(RING2['cx']), top + (TAG_H + LOCK_GAP) * sy, LOCK_H / 5.0 * sy, "LOCKED", LOCK_COL,
                      0.95 * lockK * aC, px)
    return st, pos, stc


def locked_sheet():
    """v3.32 MERGE: the small ring normal, easing (k 0.5) and locked (PE [Ghost] LockCurse=1, curse 0), at 1080p 16:9,
    1024x768 4:3 and 2560x1440, crops 1:1, plus a 3x zoom of 1080p"""
    cases = [("Eileen 0.33", 0.33, 0.0), ("Eileen 0, unlocked", 0.0, 0.0),
             ("easing (0.15 s)", 0.0, 0.5), ("LOCKED", 0.0, 1.0)]
    sizes = [(1920, 1080), (1024, 768), (2560, 1440)]
    cw, ch = 330, 260
    out = Image.new("RGB", (40 + cw * len(cases), 70 + (ch + 34) * len(sizes) + 700), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 17), ImageFont.truetype(FONT, 24)
    d.text((20, 15), "Curse Meter = Both (v3.32 MERGE): companion ring locked by Playable Eileen = grey, dim, LOCKED",
           font=ft, fill=(200, 196, 188))
    zoom = None
    for si, (W, H) in enumerate(sizes):
        S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
        for ci, (name, vC, k) in enumerate(cases):
            r = Raster(np.full((H // 4, W, 3), 0.085, np.float32) + RNG.normal(0, 0.008, (H // 4, W, 1)).astype(np.float32))
            dw = Draw(r)
            st, pos, _ = curse_both(dw, W, H, S, 0.42, (0.35, 0.5, 1.0), vC, 1.0, 1.0, t=0.05, lockK=k)
            im = r.img()
            label(im, W, H, S, st, 1.0, pos)
            X, Y, sx, sy = canvas(W, H, S)
            half = cw // 2
            crop = im.crop((int(X(432)) - half, 0, int(X(432)) + half, min(ch, H // 4)))
            y = 60 + si * (ch + 34)
            out.paste(crop, (20 + ci * cw, y + 24))
            d.text((20 + ci * cw, y), "%dx%d %s" % (W, H, name), font=fs, fill=(160, 156, 150))
            if (W, H) == (1920, 1080) and k == 1.0:
                zoom = im.crop((int(X(414)) - 70, 0, int(X(414)) + 130, 170)).resize((600, 510), Image.LANCZOS)
    y = 60 + len(sizes) * (ch + 34)
    d.text((20, y), "1920x1080 LOCKED, 3x", font=fs, fill=(160, 156, 150))
    out.paste(zoom, (20, y + 24))
    return out


def both_sheet():
    """the cases at 1080p 16:9, crops 1:1"""
    cases = [
        ("PE 12 + Eileen 0.12", 0.12, 0.12, 0.55, 0.55, 0, 0, False),
        ("PE 42 Haunted + Eileen 0.33", 0.42, 0.33, 1.0, 0.55, 0, 0, False),
        ("PE 42 + Eileen crossing 0.50 (pulse)", 0.42, 0.51, 0.55, 1.0, 0, 0.6, False),
        ("PE 64 Cursed + Eileen 0.58", 0.64, 0.58, 1.0, 1.0, 0, 0, False),
        ("PE 100 latched + Eileen 0.84 (beat)", 1.0, 0.84, 1.0, 1.0, 0, 0, True),
        ("PE 30 + Eileen away (saved value, dimmed)", 0.30, 0.46, 1.0, 0.5 * 0.55, 0, 0, False),
        ("PE 55, no companion (one ring, = On)", 0.55, 0.0, 1.0, 0.0, 0, 0, False),
    ]
    Wf, Hf = 1920, 1080
    Sf = (16 / 9) / (4 / 3)
    cw, ch = 300, 230
    out = Image.new("RGB", (40 + cw * 4, 80 + (ch + 40) * 2), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 17), ImageFont.truetype(FONT, 26)
    d.text((20, 15), "Curse Meter = Both (v3.32): the player's ring + companion Eileen's small ring (1080p 16:9, 1:1)",
           font=ft, fill=(200, 196, 188))
    for ci, (name, vP, vC, aP, aC, pP, pC, lat) in enumerate(cases):
        r = Raster(np.full((Hf // 4, Wf, 3), 0.085, np.float32) + RNG.normal(0, 0.008, (Hf // 4, Wf, 1)).astype(np.float32))
        dw = Draw(r)
        st, pos, _ = curse_both(dw, Wf, Hf, Sf, vP, (0.35, 0.5, 1.0), vC, aP, aC, pP, pC, t=0.05, latched=lat)
        im = r.img()
        label(im, Wf, Hf, Sf, st, 1.0 if aP > 0.6 else 0.6, pos)
        X, Y, sx, sy = canvas(Wf, Hf, Sf)
        crop = im.crop((int(X(440)) - 150, 0, int(X(440)) + 150, 230))
        col, row = ci % 4, ci // 4
        y = 70 + row * (ch + 40)
        out.paste(crop, (20 + col * cw, y + 24))
        d.text((20 + col * cw, y), name, font=fs, fill=(160, 156, 150))
    return out


def both_sizes():
    """the same state at 720p, 1080p, 1440p (16:9), 768p 4:3 and 2560x1080, crops 1:1"""
    sizes = [(1280, 720), (1920, 1080), (2560, 1440), (1024, 768), (1280, 1024), (2560, 1080)]
    crops = []
    for (W, H) in sizes:
        S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
        r = Raster(np.full((H // 4, W, 3), 0.085, np.float32))
        dw = Draw(r)
        st, pos, _ = curse_both(dw, W, H, S, 0.64, (0.35, 0.5, 1.0), 0.33, 1.0, 1.0, t=0.05)
        im = r.img()
        label(im, W, H, S, st, 1.0, pos)
        X, Y, sx, sy = canvas(W, H, S)
        half = int(150 * H / 1080.0) + 20
        crops.append(("%dx%d" % (W, H), im.crop((int(X(432)) - half, 0, int(X(432)) + half, int(H * 0.2)))))
    Wt = sum(c.size[0] for _, c in crops) + 20 * (len(crops) + 1)
    Ht = max(c.size[1] for _, c in crops) + 60
    out = Image.new("RGB", (Wt, Ht), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 17)
    x = 20
    for name, c in crops:
        out.paste(c, (x, 40))
        d.text((x, 12), name, font=fs, fill=(160, 156, 150))
        x += c.size[0] + 20
    return out


def both_scene(w, h, bright=False):
    """the v3.30 scene with the Both meter"""
    S = (w / h) / (4.0 / 3.0) if w / h > 4.0 / 3.0 + 1e-3 else 1.0
    img = Image.fromarray((backdrop(w, h, bright) * 255 + 0.5).astype(np.uint8))
    k = h / 1080.0
    cxs = w / 2.0
    img = silhouettes(img, [("human", cxs + 220 * k, 760 * k, 330 * k), ("ghost", cxs - 55 * k, 640 * k, 200 * k),
                            ("down", cxs + 500 * k, 905 * k, 150 * k)])
    r = Raster(np.asarray(img, np.float32) / 255.0)
    dw = Draw(r)
    gap = h * 0.012
    for b in (Bar(cxs - 55 * k, 640 * k - 200 * k - 25 * k - gap, 0.74, a=0.85, kind="ghost", guard=0.55),
              Bar(cxs + 530 * k, 905 * k - 60 * k - gap, 0.0, 0.0, 1.0, "ghost", guard=0.0, rise=0.5, rev=0.62, rev_a=1.0, t=0.4)):
        draw_bar(dw, b, h)
    st, pos, _ = curse_both(dw, w, h, S, 0.64, (0.35, 0.5, 1.0), 0.33, 1.0, 1.0, t=0.4)
    out = r.img()
    label(out, w, h, S, st, 1.0, pos)
    return out


def both_layout_check():
    """pixel boxes of the two rings (with thorns and the pulse) and the labels: printed, and asserted apart"""
    for (W, H) in ((1280, 720), (1920, 1080), (1024, 768), (1280, 1024), (640, 480), (2560, 1080)):
        S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
        X, Y, sx, sy = canvas(W, H, S)
        px = max(H / 1080.0, 1.0)
        R1 = 17.0 * sy
        L1 = 4.2 * px + 0.10 * R1
        R2 = RING2['R'] * sy
        L2 = RING2['k'] * 4.2 * px + 0.10 * R2
        main_left = X(452.0) - (R1 + 0.5 * px + L1)
        small_right = X(RING2['cx']) + R2 + 0.5 * px + L2
        u = TAG_H / 5.0 * sy
        tag_w = tiny_width("EILEEN") * u
        tag_top = Y(RING2['cy'] + RING2['R'] + TAG_GAP)
        print("%dx%d: main ring from x %.0f, small ring (thorns) to x %.0f: gap %.1f px; label %.0f..%.0f x %.0f..%.0f, "
              "the main stage name from y %.0f" % (W, H, main_left, small_right, main_left - small_right,
                                                  X(RING2['cx']) - tag_w / 2, X(RING2['cx']) + tag_w / 2, tag_top,
                                                  tag_top + 5 * u, Y(63.0)))
        assert main_left - small_right > 2.0
        assert tag_top + 5 * u + px < Y(63.0)


# ====================================================================== v3.35 CM335: the target notch, the next mark, the
# left-alone glyph, and the main ring's stage name in the stroke font. Mirrors src/hudbars.inc HbNotchDraw / HbAloneDraw
# and hudbars_sim.h (kHbGlyph + U T H A P R S, HB_LABEL_*, HbAloneSize / HbAloneGap).
GLYPH.update({
    'U': (4, [(0, 0, 1, 4, 0), (3, 0, 4, 4, 0), (1, 4, 3, 5, 0)]),
    'T': (3, [(0, 0, 3, 1, 0), (1, 1, 2, 5, 0)]),
    'H': (4, [(0, 0, 1, 5, 0), (3, 0, 4, 5, 0), (1, 2, 3, 3, 0)]),
    'A': (4, [(0, 1, 1, 5, 0), (3, 1, 4, 5, 0), (1, 0, 3, 1, 0), (1, 2.5, 3, 3.5, 0)]),
    'P': (4, [(0, 0, 1, 5, 0), (1, 0, 3, 1, 0), (3, 1, 4, 2, 0), (1, 2, 3, 3, 0)]),
    'R': (4, [(0, 0, 1, 5, 0), (1, 0, 3, 1, 0), (3, 1, 4, 2, 0), (1, 2, 3, 3, 0), (2, 3, 3, 5, 1)]),
    'S': (3, [(0, 0, 3, 1, 0), (0, 1, 1, 2, 0), (0, 2, 3, 3, 0), (2, 3, 3, 4, 0), (0, 4, 3, 5, 0)]),
})
STAGE_TAG = ["UNTOUCHED", "HAUNTED", "CURSED", "POSSESSED"]
STAGE_RGB = [0xC6BCC2, 0xD0A8C6, 0xDC8EAE, 0xEC7478]   # kHbStageRgb
LABEL_H, LABEL_GAP = 5.4, 3.4                       # HB_LABEL_H / HB_LABEL_GAP
LABEL_TOP = 42.0 + 17.0 + LABEL_GAP                 # HB_LABEL_TOP
NOTCH_RGB, NEXT_RGB, CLAW_RGB = 0xF0E4EC, 0xB8C8E0, 0xD02838


def alone_size(Rc):
    return 2.4 + 0.13 * Rc


def notch_draw(dw, cx, cy, R, bw, k, px, pos, na, nxt, nxa, a):
    """HbNotchDraw: the next mark (arc + cold tick over the outer half) under the notch (pale cut + inner pointer)"""
    A0, SPAN = -0.75 * math.pi, 1.5 * math.pi
    r1, r0, rm = R, R - bw, R - 0.5 * bw
    sd = 6.0 / k
    if nxa * a > 0.004:
        q = nxa * a
        an, ap = A0 + SPAN * nxt, A0 + SPAN * pos
        hw = (0.6 + 0.3 * k) * px / rm
        dw.ring(cx, cy, r1 + px, r1 + 2 * px, min(an, ap), max(an, ap), C(NEXT_RGB, 0.30 * q), C(NEXT_RGB, 0.30 * q), sd)
        dw.ring(cx, cy, rm - px, r1 + 2 * px, an - hw - px / rm, an + hw + px / rm, C(RIM, 0.45 * q), C(RIM, 0.45 * q), sd)
        dw.ring(cx, cy, rm, r1 + 2 * px, an - hw, an + hw, C(NEXT_RGB, 0.55 * q), C(NEXT_RGB, 0.70 * q), sd)
    if na * a > 0.004:
        q = na * a
        ang = A0 + SPAN * pos
        sn, cs = math.sin(ang), -math.cos(ang)
        hw = (0.55 + 0.35 * k) * px / rm
        ho = hw + px / rm
        tl, tw, tb = (1.5 + 2.0 * k) * px, (1.0 + 1.2 * k) * px, r0 - 0.5 * px
        dw.ring(cx, cy, r0 - px, r1 + px, ang - ho, ang + ho, C(RIM, 0.55 * q), C(RIM, 0.55 * q), sd)
        dw.ring(cx, cy, r0, r1, ang - hw, ang + hw, C(NOTCH_RGB, 0.62 * q), C(NOTCH_RGB, 0.72 * q), sd)
        dw.tri([(cx + sn * tb, cy + cs * tb), (cx + sn * (tb - tl) - cs * tw, cy + cs * (tb - tl) + sn * tw),
                (cx + sn * (tb - tl) + cs * tw, cy + cs * (tb - tl) - sn * tw)],
               [C(NOTCH_RGB, 0.65 * q), C(NOTCH_RGB, 0.12 * q), C(NOTCH_RGB, 0.12 * q)])


def alone_draw(dw, cx, cy, R, Rc, sy, danger, alone, a, t=0.0):
    """HbAloneDraw: her figure and claw marks left of the ring"""
    q = alone * a * (0.80 + 0.20 * math.sin(t * 2.618))
    if q <= 0.004:
        return
    s = alone_size(Rc) * sy
    gx, gy = cx - R - (1.6 + alone_size(Rc)) * sy, cy
    fx = gx + 0.15 * s
    dw.disc(gx - 0.45 * s, gy, 1.45 * s, C(RIM, 0.50 * q), C(RIM, 0.0))
    dw.disc(fx, gy - 0.58 * s, 0.24 * s, C(TAG_COL, 0.90 * q), C(TAG_COL, 0.90 * q))
    ct, cb = C(TAG_COL, 0.90 * q), C(TAG_COL, 0.70 * q)
    dw.quad([(fx - 0.22 * s, gy - 0.28 * s), (fx + 0.22 * s, gy - 0.28 * s), (fx - 0.36 * s, gy + 0.85 * s),
             (fx + 0.36 * s, gy + 0.85 * s)], [ct, ct, cb, cb])
    for i in range(3 if danger >= 2 else 2):
        x0 = gx - 0.62 * s - i * 0.34 * s
        w = 0.07 * s + 0.25
        c0, c1 = C(CLAW_RGB, 0.25 * q), C(CLAW_RGB, 0.90 * q)
        dw.quad([(x0 + 0.20 * s - w, gy - 0.62 * s), (x0 + 0.20 * s + w, gy - 0.62 * s), (x0 - 0.20 * s - w, gy + 0.62 * s),
                 (x0 - 0.20 * s + w, gy + 0.62 * s)], [c0, c0, c1, c1])


def ring_v335(dw, w, h, S, v, a, small, notch=None, nxt=None, alone=0, lockK=0.0, t=0.05, label_a=0.0, th=None,
              minor=0.40, latched=False):
    """one ring as HbCurse draws it in v3.35: small = Both's companion ring (EILEEN tag, LOCKED), else the main ring
    (its stage name in the stroke font); notch = the target (None = none), nxt = the next-spawn target"""
    global GREY_K
    X, Y, sx, sy = canvas(w, h, S)
    px = max(h / 1080.0, 1.0)
    th = th or (0.25, 0.5, 0.7)
    if small:
        cxy, Rc, k = (RING2['cx'], RING2['cy']), RING2['R'], RING2['k']
        a2 = a * (1.0 - LOCK_DIM * lockK)
        GREY_K = lockK
    else:
        cxy, Rc, k, a2 = (452.0, 42.0), 17.0, 1.0, a
    st, _ = curse_ring(dw, w, h, S, v, th, minor, a2, t=t, cxy=cxy, Rc=Rc, k=k, full_latched=latched)
    cx, cy, R = X(cxy[0]), Y(cxy[1]), Rc * sy
    na = 0.0 if notch is None else min(max((abs(notch - v) - 0.006) / 0.010, 0.0), 1.0)
    nxa = 0.0 if nxt is None else min(max((abs(nxt - (v if notch is None else notch)) - 0.006) / 0.010, 0.0), 1.0)
    notch_draw(dw, cx, cy, R, 3.6 * k * sy, k, px, notch if notch is not None else v, na, nxt if nxt is not None else 0.0,
               nxa, a2 * (1.0 - lockK))
    alone_draw(dw, cx, cy, R, Rc, sy, alone, 1.0 if alone else 0.0, a2, t)
    if small:
        top = Y(RING2['cy'] + RING2['R'] + TAG_GAP)
        tiny_text(dw, cx, top, TAG_H / 5.0 * sy, "EILEEN", TAG_COL, a2 * 0.9, px)
        GREY_K = 0.0
        if lockK > 0.004:
            tiny_text(dw, cx, top + (TAG_H + LOCK_GAP) * sy, LOCK_H / 5.0 * sy, "LOCKED", LOCK_COL, 0.95 * lockK * a, px)
    elif label_a > 0.02:
        tiny_text(dw, cx, Y(LABEL_TOP), LABEL_H / 5.0 * sy, STAGE_TAG[st], STAGE_RGB[st], label_a, px)
    return st


def strip(w=1920, h=1080):
    S = (w / h) / (4.0 / 3.0) if w / h > 4.0 / 3.0 + 1e-3 else 1.0
    return Raster(np.full((h // 4, w, 3), 0.085, np.float32) + RNG.normal(0, 0.008, (h // 4, w, 1)).astype(np.float32)), S


def v335_target_sheet():
    """the companion's notch, next mark and glyph: the main ring in Companion mode (top row) and Both's small ring
    beside the player's ring (bottom row), 1080p 16:9, crops 1:1, captions with the values"""
    cases = [  # caption, shown v, target, next, danger, away (dimmed), lock
        ("heading up: 0.30 -> 0.55", 0.30, 0.55, None, 0, False, 0.0),
        ("cooling down: 0.62 -> 0.40", 0.62, 0.40, None, 0, False, 0.0),
        ("at its target (0.45): no notch", 0.45, 0.45, None, 0, False, 0.0),
        ("left behind, danger 1: 0.33, target 0.33, next 0.47", 0.33, 0.33, 0.47, 1, True, 0.0),
        ("left alone, danger 2: 0.40, 0.52, next 0.66", 0.40, 0.52, 0.66, 2, True, 0.0),
        ("LockCurse (PE): grey, no notch", 0.0, 0.0, None, 0, False, 1.0),
    ]
    Wf, Hf = 1920, 1080
    cw, ch = 330, 200
    out = Image.new("RGB", (40 + cw * len(cases), 120 + (ch + 46) * 2), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 15), ImageFont.truetype(FONT, 24)
    d.text((20, 12), "v3.35 CM335: companion Eileen's target notch, next-spawn mark and left-alone glyph (1080p 16:9, crops 1:1)",
           font=ft, fill=(200, 196, 188))
    for row in range(2):
        for ci, (name, v, tg, nx, dg, away, lk) in enumerate(cases):
            if row == 0 and lk > 0:
                name = "PE LockCurse, Companion mode: 0, no notch"
            r, S = strip(Wf, Hf)
            dw = Draw(r)
            X, Y, sx, sy = canvas(Wf, Hf, S)
            if row == 0:
                ring_v335(dw, Wf, Hf, S, v, (0.5 if away else 1.0), False, tg, nx, dg, 0.0, label_a=0.6 if away else 1.0)
                cxc = X(440)
            else:
                ring_v335(dw, Wf, Hf, S, 0.42, 1.0, False, None, None, 0, label_a=1.0, th=(0.35, 0.5, 1.0), minor=-1)
                ring_v335(dw, Wf, Hf, S, v, (0.5 if away else 1.0), True, tg, nx, dg, lk)
                cxc = X(420)
            im = r.img()
            crop = im.crop((int(cxc) - cw // 2, 0, int(cxc) + cw // 2, ch))
            y = 60 + row * (ch + 46)
            out.paste(crop, (20 + ci * cw, y + 22))
            d.text((20 + ci * cw, y), ("Companion: " if row == 0 else "Both: ") + name if ci == 0 else name, font=fs,
                   fill=(160, 156, 150))
    return out


def v335_zoom():
    """3x zooms of the two left-alone cases and a notch on the fill, 1080p"""
    Wf, Hf = 1920, 1080
    tiles = []
    for (name, small, v, tg, nx, dg) in (("Companion: left alone, danger 2, 3x", False, 0.40, 0.52, 0.66, 2),
                                         ("Companion: heading up 0.30 -> 0.55, 3x", False, 0.30, 0.55, None, 0),
                                         ("Both: small ring left alone, danger 1, 3x", True, 0.33, 0.40, 0.47, 1),
                                         ("Both: small ring cooling 0.62 -> 0.40, 3x", True, 0.62, 0.40, None, 0)):
        r, S = strip(Wf, Hf)
        dw = Draw(r)
        X, Y, sx, sy = canvas(Wf, Hf, S)
        if small:
            ring_v335(dw, Wf, Hf, S, 0.42, 1.0, False, None, None, 0, label_a=1.0, th=(0.35, 0.5, 1.0), minor=-1)
            ring_v335(dw, Wf, Hf, S, v, 0.5 if dg else 1.0, True, tg, nx, dg)
            c = (int(X(405)) - 75, 40, int(X(405)) + 75, 170)
        else:
            ring_v335(dw, Wf, Hf, S, v, 0.5 if dg else 1.0, False, tg, nx, dg, label_a=0.6 if dg else 1.0)
            c = (int(X(440)) - 100, 40, int(X(440)) + 100, 200)
        tiles.append((name, r.img().crop(c).resize(((c[2] - c[0]) * 3, (c[3] - c[1]) * 3), Image.LANCZOS)))
    Wt = 20 + sum(t.size[0] + 20 for _, t in tiles)
    out = Image.new("RGB", (Wt, max(t.size[1] for _, t in tiles) + 60), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 17)
    x = 20
    for name, t in tiles:
        out.paste(t, (x, 44))
        d.text((x, 14), name, font=fs, fill=(160, 156, 150))
        x += t.size[0] + 20
    return out


def v335_sizes():
    """the left-alone case (Companion mode) and Both at 720p, 1080p, 1440p, 1024x768, 2560x1080, crops 1:1"""
    sizes = [(1280, 720), (1920, 1080), (2560, 1440), (1024, 768), (2560, 1080)]
    crops = []
    for (W, H) in sizes:
        for both in (False, True):
            r, S = strip(W, H)
            dw = Draw(r)
            X, Y, sx, sy = canvas(W, H, S)
            if both:
                ring_v335(dw, W, H, S, 0.64, 1.0, False, None, None, 0, label_a=1.0, th=(0.35, 0.5, 1.0), minor=-1)
                ring_v335(dw, W, H, S, 0.40, 0.5, True, 0.52, 0.66, 2)
            else:
                ring_v335(dw, W, H, S, 0.40, 0.5, False, 0.52, 0.66, 2, label_a=0.6)
            half = int(130 * H / 1080.0) + 20
            crops.append(("%dx%d %s" % (W, H, "Both" if both else "Companion"),
                          r.img().crop((int(X(430)) - half, 0, int(X(430)) + half, min(int(H * 0.2), H // 4)))))
    Wt = sum(c.size[0] for _, c in crops[0::2]) + 20 * (len(sizes) + 1)
    Ht = 2 * (max(c.size[1] for _, c in crops) + 40) + 20
    out = Image.new("RGB", (Wt, Ht), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 15)
    x = 20
    for i in range(0, len(crops), 2):
        for j in range(2):
            name, c = crops[i + j]
            y = 10 + j * (Ht // 2)
            d.text((x, y), name, font=fs, fill=(160, 156, 150))
            out.paste(c, (x, y + 24))
        x += crops[i][1].size[0] + 20
    return out


def v335_label_compare():
    """the user's ask: our stage name like the small ring's tags. Left: the small ring's EILEEN / LOCKED (the reference
    style); middle: v3.34 (the game's font 1, 17 canvas units, at y 63); right: v3.35 (the stroke font, 5.4 units, 3.4
    under the ring). Every stage, 1080p crops 1:1 and a 2x zoom; measurements in the captions."""
    Wf, Hf = 1920, 1080
    S = (16 / 9) / (4 / 3)
    X, Y, sx, sy = canvas(Wf, Hf, S)
    cw, ch = 300, 210
    out = Image.new("RGB", (40 + cw * 5, 90 + (ch + 40) * 3 + 520), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 15), ImageFont.truetype(FONT, 22)
    d.text((20, 12), "v3.35 CM335: the stage name in the small ring's stroke font (1080p 16:9, crops 1:1; bottom: 2x)",
           font=ft, fill=(200, 196, 188))
    vals = [0.12, 0.33, 0.58, 0.84]
    # row 0: the reference (Both, LOCKED on / off); row 1: v3.34 main ring with the game font; row 2: v3.35 stroke font
    for row in range(3):
        for ci in range(5):
            r, _ = strip(Wf, Hf)
            dw = Draw(r)
            if row == 0:
                if ci >= 2:
                    continue
                lk = 1.0 if ci == 1 else 0.0
                ring_v335(dw, Wf, Hf, S, 0.42, 1.0, False, None, None, 0, label_a=0.0, th=(0.35, 0.5, 1.0), minor=-1)
                ring_v335(dw, Wf, Hf, S, 0.0 if lk else 0.33, 1.0, True, None, None, 0, lk)
                im = r.img()
                cap = "reference: EILEEN 4.6 / LOCKED 3.6 units" if ci == 0 else "reference: LOCKED (PE LockCurse)"
                cxc = X(414)
            else:
                if ci >= 4:
                    continue
                v = vals[ci]
                st = stage_of(v, (0.25, 0.5, 0.7))
                if row == 1:
                    curse_ring(dw, Wf, Hf, S, v, (0.25, 0.5, 0.7), 0.40, 1.0, t=0.05)
                    im = r.img()
                    label(im, Wf, Hf, S, st, 1.0, (X(452.0), Y(63.0)))
                    cap = "v3.34: game font, %s" % STAGES[st]
                else:
                    ring_v335(dw, Wf, Hf, S, v, 1.0, False, None, None, 0, label_a=1.0)
                    im = r.img()
                    cap = "v3.35: stroke font, %s" % STAGE_TAG[st]
                cxc = X(452)
            crop = im.crop((int(cxc) - cw // 2, 0, int(cxc) + cw // 2, ch))
            y = 50 + row * (ch + 40)
            out.paste(crop, (20 + ci * cw, y + 22))
            d.text((20 + ci * cw, y), cap, font=fs, fill=(160, 156, 150))
    # 2x zoom: reference | before | after, side by side (Both mode, Cursed)
    tiles = []
    for mode in range(3):
        r, _ = strip(Wf, Hf)
        dw = Draw(r)
        if mode == 0:
            ring_v335(dw, Wf, Hf, S, 0.58, 1.0, False, None, None, 0, label_a=0.0)
            ring_v335(dw, Wf, Hf, S, 0.0, 1.0, True, None, None, 0, 1.0)
            im = r.img()
            name = "the small ring's tags (reference)"
        elif mode == 1:
            curse_ring(dw, Wf, Hf, S, 0.58, (0.25, 0.5, 0.7), 0.40, 1.0, t=0.05)
            ring_v335(dw, Wf, Hf, S, 0.0, 1.0, True, None, None, 0, 1.0)
            im = r.img()
            label(im, Wf, Hf, S, 2, 1.0, (X(452.0), Y(63.0)))
            name = "before (v3.34): game font 1"
        else:
            ring_v335(dw, Wf, Hf, S, 0.58, 1.0, False, None, None, 0, label_a=1.0)
            ring_v335(dw, Wf, Hf, S, 0.0, 1.0, True, None, None, 0, 1.0)
            im = r.img()
            name = "after (v3.35): stroke font"
        c = (int(X(432)) - 120, 30, int(X(432)) + 120, 200)
        tiles.append((name, im.crop(c).resize((480, 340), Image.LANCZOS)))
    y = 50 + 3 * (ch + 40)
    x = 20
    for name, t in tiles:
        d.text((x, y), name + ", 2x", font=fs, fill=(160, 156, 150))
        out.paste(t, (x, y + 22))
        x += t.size[0] + 20
    return out


def v335_measure():
    """pixel sizes of the texts at 720p / 1080p / 1440p (for DESIGN.md)"""
    for H in (720, 1080, 1440):
        sy = H / 448.0
        px = max(H / 1080.0, 1.0)
        print("%dp: EILEEN %.1f px tall, %.1f wide, %.1f under its ring; LOCKED %.1f tall; v3.35 stage name %.1f tall, "
              "UNTOUCHED %.1f wide, %.1f under the ring; v3.34 game font ~%.1f tall at %.1f under the ring; stroke %.1f px"
              % (H, TAG_H * sy, tiny_width("EILEEN") * TAG_H / 5 * sy, TAG_GAP * sy, LOCK_H * sy, LABEL_H * sy,
                 tiny_width("UNTOUCHED") * LABEL_H / 5 * sy, LABEL_GAP * sy, 17.0 * sy, 4.0 * sy, LABEL_H / 5 * sy))


# ====================================================================== v3.38 HB338: readable on black, PLAYER, the
# Performance readout's place. Mirrors src/hudbars.inc HbRingDraw / HbNotchDraw / HbAloneDraw / HbCurse (v3.38) and
# src/perfhud_sim.h PerfPlace (v3.38). "before" = v3.37 (ring_v335 with CLEAR), "after" = v3.38 (ring_v338): both rings
# rest at 0.70 and every ring is named (PLAYER / EILEEN), the user's choices of 2026-11-21.
GLYPH['Y'] = (4, [(0, 0, 1, 2.5, 1), (3, 0, 4, 2.5, -1), (1.5, 2.5, 2.5, 5, 0)])   # v3.38 HB338
STAGE_TAG[0] = "CLEAR"                                                               # v3.37 MN337
HAIR_RGB, HAIR_A = 0xCDBFC6, 0.30          # the bone hairlines outside / inside the band
EMPTY338 = (0x3A2630, 0x241A20)            # the empty band (inner, outer colour): a dusky track
THORN_OFF338 = 0x8E8496                    # an unlit thorn
MINOR338_A = 0.70                          # the 0.4 mark
CLAW338 = 0xFF3A4A
AWAY_A, REST_A, REST2_A, ALERT_FLOOR = 0.70, 0.55, 0.70, 0.85
PLAYER_TOP = LABEL_TOP                     # HB_PLAYER_TOP
STAGE_TOP_P = LABEL_TOP + TAG_H + LOCK_GAP  # HbStageTop(1)


def curse_ring_338(dw, w, h, S, v, th, minor, a, t=0.05, cxy=(452.0, 42.0), Rc=17.0, k=1.0, full_latched=False):
    """HbRingDraw v3.38: a stronger backing plate, bone hairlines outside and inside the band, a dusky empty band, a
    lifted fill (inner colour 35 % toward the outer, x 1.15), brighter unlit thorns, the 0.4 mark over the hairline"""
    X, Y, sx, sy = canvas(w, h, S)
    cx, cy = X(cxy[0]), Y(cxy[1])
    R = Rc * sy
    bw = 3.6 * k * sy
    r0, r1 = R - bw, R
    px = max(h / 1080.0, 1.0)
    sd = 6.0 / k
    st = stage_of(v, th)
    A0, SPAN = -0.75 * math.pi, 1.5 * math.pi
    beat = heartbeat(t) if st == 3 else 0.0
    dw.disc(cx, cy, R + 2 * px, C(0x000000, 0.55 * a), C(0x000000, 0.45 * a))
    dw.ring(cx, cy, R + 2 * px, R + max(5 * k * px, 3 * px), -math.pi, math.pi, C(0x000000, 0.45 * a), C(0x000000, 0.0), sd)
    dw.ring(cx, cy, r0 - px, r1 + px, A0 - 0.03, A0 + SPAN + 0.03, C(RIM, 0.85 * a), C(RIM, 0.85 * a), sd)
    dw.ring(cx, cy, r1 + px, r1 + 2 * px, A0 - 0.03, A0 + SPAN + 0.03, C(HAIR_RGB, HAIR_A * a), C(HAIR_RGB, HAIR_A * a), sd)
    dw.ring(cx, cy, r0, r1, A0, A0 + SPAN, C(EMPTY338[0], 0.85 * a), C(EMPTY338[1], 0.85 * a), sd)
    bs, bst = [0.0], [0]
    for i in range(3):
        if bs[-1] < th[i] < 1.0:
            bs.append(th[i])
            bst.append(i + 1)
    bs.append(1.0)
    for j in range(len(bst)):
        e = min(v, bs[j + 1])
        if e <= bs[j]:
            continue
        ss = 3 if (st == 3 and full_latched) else bst[j]
        c0, c1 = CSEG[ss]
        kk = min(1.0, 1.15 * a * (1.0 + 0.25 * beat))
        c0 = lerpc(c0, CGLOW[ss], 0.25 * beat)
        dw.ring(cx, cy, r0, r1, A0 + SPAN * bs[j], A0 + SPAN * e, C(lerpc(c1, CSEG[ss][0], 0.35), kk), C(c0, kk), sd)
    if v > 0.004:
        dw.ring(cx, cy, r1 - px, r1, A0, A0 + SPAN * v, C(0xFFFFFF, 0.18 * a), C(0xFFFFFF, 0.18 * a), sd)
        ang = A0 + SPAN * v
        dw.ring(cx, cy, r0 - px, r1 + px, ang - 0.035, ang, C(0xFFE0E8, 0.80 * a), C(0xFFE0E8, 0.80 * a), sd)
    for i in range(3):
        m = th[i]
        if not (0 < m < 1):
            continue
        ang = A0 + SPAN * m
        lit = v >= m
        sn, cs = math.sin(ang), -math.cos(ang)
        L = k * 4.2 * px + 0.10 * R
        base = r1 + 0.5 * px
        wv = k * 1.6 * px + 0.04 * R
        col, ta = (CGLOW[i + 1], 0.9 * a) if lit else (THORN_OFF338, min(1.0, a))
        dw.tri([(cx + sn * base - cs * wv, cy + cs * base + sn * wv), (cx + sn * base + cs * wv, cy + cs * base - sn * wv),
                (cx + sn * (base + L), cy + cs * (base + L))], [C(col, ta), C(col, ta), C(lerpc(col, 0xFFFFFF, 0.3), ta)])
        dw.ring(cx, cy, r0, r1, ang - 0.018, ang + 0.018, C(RIM, 0.9 * a), C(RIM, 0.9 * a), sd)
    # the sigil's dark well, then the inner hairline and the 0.4 mark over it, then the eye
    dw.disc(cx, cy, r0 - 2.0 * k * px, C(0x0C080A, 0.80 * a), C(0x1A1014, 0.80 * a))
    dw.ring(cx, cy, r0 - 2 * px, r0 - px, A0 - 0.03, A0 + SPAN + 0.03, C(HAIR_RGB, HAIR_A * a), C(HAIR_RGB, HAIR_A * a), sd)
    if minor > 0:
        ang = A0 + SPAN * minor
        dw.ring(cx, cy, r0 - 3.5 * px, r0 - 1.0 * px, ang - 0.03, ang + 0.03, C(0xCFC4D8, MINOR338_A * a), C(0xCFC4D8, MINOR338_A * a), sd)
    ec = CGLOW[st]
    open_ = [0.15, 0.40, 0.70, 1.0][st]
    ew, eh = (r0 - 4 * k * px) * 0.85, (r0 - 4 * k * px) * 0.85 * (0.18 + 0.42 * open_)
    glow = min(1.0, (0.45 + 0.35 * open_) * a * (1.0 + 0.6 * beat))
    dw.quad([(cx, cy - eh), (cx + ew, cy), (cx - ew, cy), (cx, cy + eh)],
            [C(lerpc(ec, 0x000000, 0.4), glow), C(ec, 0.0), C(ec, 0.0), C(lerpc(ec, 0x000000, 0.4), glow)])
    dw.disc(cx, cy, max(1.5 * k * px, eh * 0.55), C(lerpc(ec, 0xFFFFFF, 0.35), min(1.0, glow * 1.4)), C(ec, glow * 0.6))
    return st


def notch_draw_338(dw, cx, cy, R, bw, k, px, pos, na, nxt, nxa, q):
    """HbNotchDraw v3.38 (q = the alert strength HbAlertQ x k): the next arc outside the hairline, a wider, brighter
    mark; the notch's cut 0.85 / 0.95, its edges 0.75, its pointer 0.90"""
    A0, SPAN = -0.75 * math.pi, 1.5 * math.pi
    r1, r0, rm = R, R - bw, R - 0.5 * bw
    sd = 6.0 / k
    if nxa * q > 0.004:
        qq = nxa * q
        an, ap = A0 + SPAN * nxt, A0 + SPAN * pos
        hw = (0.9 + 0.4 * k) * px / rm
        ro = r1 + 3.5 * px
        dw.ring(cx, cy, r1 + 2 * px, ro, min(an, ap), max(an, ap), C(NEXT_RGB, 0.55 * qq), C(NEXT_RGB, 0.55 * qq), sd)
        dw.ring(cx, cy, rm - px, ro + px, an - hw - px / rm, an + hw + px / rm, C(RIM, 0.65 * qq), C(RIM, 0.65 * qq), sd)
        dw.ring(cx, cy, rm, ro, an - hw, an + hw, C(NEXT_RGB, 0.85 * qq), C(NEXT_RGB, 0.95 * qq), sd)
    if na * q > 0.004:
        qq = na * q
        ang = A0 + SPAN * pos
        sn, cs = math.sin(ang), -math.cos(ang)
        hw = (0.55 + 0.35 * k) * px / rm
        ho = hw + px / rm
        tl, tw, tb = (1.5 + 2.0 * k) * px, (1.0 + 1.2 * k) * px, r0 - 0.5 * px
        dw.ring(cx, cy, r0 - px, r1 + px, ang - ho, ang + ho, C(RIM, 0.75 * qq), C(RIM, 0.75 * qq), sd)
        dw.ring(cx, cy, r0, r1, ang - hw, ang + hw, C(NOTCH_RGB, 0.85 * qq), C(NOTCH_RGB, 0.95 * qq), sd)
        dw.tri([(cx + sn * tb, cy + cs * tb), (cx + sn * (tb - tl) - cs * tw, cy + cs * (tb - tl) + sn * tw),
                (cx + sn * (tb - tl) + cs * tw, cy + cs * (tb - tl) - sn * tw)],
               [C(NOTCH_RGB, 0.90 * qq), C(NOTCH_RGB, 0.12 * qq), C(NOTCH_RGB, 0.12 * qq)])


def alone_draw_338(dw, cx, cy, R, Rc, sy, px, danger, q):
    """HbAloneDraw v3.38 (q = alone x the alert strength x k, breathing folded in): a darker spot, the figure at full
    strength, brighter, wider claws each over a black copy one pixel larger"""
    if q <= 0.004:
        return
    s = alone_size(Rc) * sy
    gx, gy = cx - R - (1.6 + alone_size(Rc)) * sy, cy
    fx = gx + 0.15 * s
    dw.disc(gx - 0.45 * s, gy, 1.45 * s, C(RIM, 0.70 * q), C(RIM, 0.0))
    dw.disc(fx, gy - 0.58 * s, 0.24 * s, C(TAG_COL, 1.0 * q), C(TAG_COL, 1.0 * q))
    ct, cb = C(TAG_COL, 1.0 * q), C(TAG_COL, 0.85 * q)
    dw.quad([(fx - 0.22 * s, gy - 0.28 * s), (fx + 0.22 * s, gy - 0.28 * s), (fx - 0.36 * s, gy + 0.85 * s),
             (fx + 0.36 * s, gy + 0.85 * s)], [ct, ct, cb, cb])
    for i in range(3 if danger >= 2 else 2):
        x0 = gx - 0.62 * s - i * 0.34 * s
        w = 0.09 * s + 0.5 * px
        for pas in (0, 1):
            o = px if pas == 0 else 0.0
            c0, c1 = (C(RIM, 0.75 * q), C(RIM, 0.75 * q)) if pas == 0 else (C(CLAW338, 0.60 * q), C(CLAW338, 1.0 * q))
            dw.quad([(x0 + 0.20 * s - w - o, gy - 0.62 * s - o), (x0 + 0.20 * s + w + o, gy - 0.62 * s - o),
                     (x0 - 0.20 * s - w - o, gy + 0.62 * s + o), (x0 - 0.20 * s + w + o, gy + 0.62 * s + o)], [c0, c0, c1, c1])


def ring_v338(dw, w, h, S, v, a, small, notch=None, nxt=None, alone=0, lockK=0.0, t=0.05, label_a=0.0, th=None,
              minor=0.40, latched=False, player=False, alert=None, k=1.0, tag_a=None):
    """one ring as HbCurse draws it in v3.38 (a = its strength with the away factor and k; alert = HbAlertQ x k, the
    notch / next / glyph strength; player = PLAYER under it and the stage name moved down; tag_a = the PLAYER / EILEEN
    labels' strength (rest 0.70) x k)"""
    global GREY_K
    X, Y, sx, sy = canvas(w, h, S)
    px = max(h / 1080.0, 1.0)
    th = th or (0.25, 0.5, 0.7)
    alert = a if alert is None else alert
    tag_a = a if tag_a is None else tag_a
    if small:
        cxy, Rc, kk = (RING2['cx'], RING2['cy']), RING2['R'], RING2['k']
        a2 = a * (1.0 - LOCK_DIM * lockK)
        GREY_K = lockK
    else:
        cxy, Rc, kk, a2 = (452.0, 42.0), 17.0, 1.0, a
    st = curse_ring_338(dw, w, h, S, v, th, minor, a2, t=t, cxy=cxy, Rc=Rc, k=kk, full_latched=latched)
    cx, cy, R = X(cxy[0]), Y(cxy[1]), Rc * sy
    na = 0.0 if notch is None else min(max((abs(notch - v) - 0.006) / 0.010, 0.0), 1.0)
    nxa = 0.0 if nxt is None else min(max((abs(nxt - (v if notch is None else notch)) - 0.006) / 0.010, 0.0), 1.0)
    notch_draw_338(dw, cx, cy, R, 3.6 * kk * sy, kk, px, notch if notch is not None else v, na,
                   nxt if nxt is not None else 0.0, nxa, alert * (1.0 - lockK))
    alone_draw_338(dw, cx, cy, R, Rc, sy, px, alone, (1.0 if alone else 0.0) * alert)
    if small:
        top = Y(RING2['cy'] + RING2['R'] + TAG_GAP)
        tiny_text(dw, cx, top, TAG_H / 5.0 * sy, "EILEEN", TAG_COL, tag_a * (1.0 - LOCK_DIM * lockK) * 0.9, px)
        GREY_K = 0.0
        if lockK > 0.004:
            tiny_text(dw, cx, top + (TAG_H + LOCK_GAP) * sy, LOCK_H / 5.0 * sy, "LOCKED", LOCK_COL, 0.95 * lockK * tag_a, px)
    else:
        tiny_text(dw, cx, Y(PLAYER_TOP), TAG_H / 5.0 * sy, "PLAYER" if player else "EILEEN", TAG_COL, 0.9 * tag_a, px)
        if label_a > 0.02:
            tiny_text(dw, cx, Y(STAGE_TOP_P), LABEL_H / 5.0 * sy, STAGE_TAG[st], STAGE_RGB[st], label_a, px)
    return st


def plain(w, h, level, rows=None):
    """a flat backdrop (level 0..1) with a little noise (the dark hole: 0.02)"""
    S = (w / h) / (4.0 / 3.0) if w / h > 4.0 / 3.0 + 1e-3 else 1.0
    hh = rows or h // 4
    img = np.full((hh, w, 3), level, np.float32) + RNG.normal(0, 0.006 + 0.01 * level, (hh, w, 1)).astype(np.float32)
    return Raster(np.clip(img, 0, 1)), S


# the cases (the user's screenshot first): (caption, mode, PE value, her value, away, target, next, danger, woken)
V338_CASES = [
    ("Both, at rest: PE 0 + Eileen 0.35 heading to 0.50", "both", 0.0, 0.35, False, 0.50, None, 0, False),
    ("Both, at rest: Eileen left alone (danger 2), next 0.66", "both", 0.12, 0.40, True, 0.52, 0.66, 2, False),
    ("Companion, at rest: left alone (danger 1), next 0.47", "companion", 0.0, 0.33, True, 0.40, 0.47, 1, False),
    ("Both, woken: PE 42 Haunted, Eileen 0.58 Cursed", "both", 0.42, 0.58, False, 0.45, None, 0, True),
]


def v338_frame(dw, W, H, S, case, after, t=0.05):
    """one HbCurse frame of a case, v3.37 (after False) or v3.38: the strengths as the dll computes them"""
    _, mode, vP, vC, away, tg, nx, dg, woken = case
    if mode == "both":
        aP = 1.0 if woken else (REST2_A if after else REST_A)
        if after:
            aC0 = 1.0 if woken else REST2_A
            aC = aC0 * (AWAY_A if away else 1.0)
            al = max(aC0 * (0.8 + 0.2 * math.sin(t * 2.618)), ALERT_FLOOR)
            ring_v338(dw, W, H, S, vP, aP, False, label_a=1.0 if woken else 0.0, th=(0.35, 0.5, 1.0), minor=-1,
                      player=True, tag_a=1.0 if woken else REST2_A)
            ring_v338(dw, W, H, S, vC, aC, True, tg, nx, dg, alert=al, tag_a=aC0 * (AWAY_A if away else 1.0))
        else:
            aC = (1.0 if woken else REST_A) * (0.5 if away else 1.0)
            ring_v335(dw, W, H, S, vP, aP, False, None, None, 0, label_a=1.0 if woken else 0.0, th=(0.35, 0.5, 1.0), minor=-1)
            ring_v335(dw, W, H, S, vC, aC, True, tg, nx, dg, t=t)
    else:
        a0 = 1.0 if woken else (REST2_A if after else REST_A)
        if after:
            al = max(a0 * (0.8 + 0.2 * math.sin(t * 2.618)), ALERT_FLOOR)
            ring_v338(dw, W, H, S, vC, a0 * (AWAY_A if away else 1.0), False, tg, nx, dg, alert=al,
                      label_a=(0.6 if away else 1.0) if woken else 0.0, tag_a=a0 * (AWAY_A if away else 1.0))
        else:
            ring_v335(dw, W, H, S, vC, a0 * (0.5 if away else 1.0), False, tg, nx, dg, t=t,
                      label_a=(0.6 if away else 1.0) if woken else 0.0)


V338_BACKS = [("near-black (a dark hole, 0.02)", 0.02), ("mid-grey (0.45)", 0.45), ("light (0.85)", 0.85)]


def v338_sheet(W=2048, H=1536):
    """before / after for every case on the three backdrops at the user's 4:3 2048x1536, crops 1:1"""
    S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
    X, Y, sx, sy = canvas(W, H, S)
    x0c, x1c = int(X(388)) - 40, int(X(452) + 32 * sy)
    hc = int(Y(80))
    cw = x1c - x0c
    nC = len(V338_CASES)
    out = Image.new("RGB", (40 + (cw + 16) * nC, 110 + (hc + 56) * 2 * len(V338_BACKS)), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 15), ImageFont.truetype(FONT, 24)
    d.text((20, 14), "v3.38 HB338: the curse meters before (v3.37) and after, %dx%d 4:3, crops 1:1 (at rest = 3.5 s after "
                     "the last change)" % (W, H), font=ft, fill=(200, 196, 188))
    for ci, case in enumerate(V338_CASES):
        d.text((20 + ci * (cw + 16), 52), case[0], font=fs, fill=(170, 166, 160))
    y = 80
    for bname, lev in V338_BACKS:
        for after in (False, True):
            d.text((20, y), "%s, %s" % ("AFTER (v3.38)" if after else "before (v3.37)", bname), font=fs,
                   fill=(200, 160, 150) if not after else (150, 200, 160))
            for ci, case in enumerate(V338_CASES):
                r, _ = plain(W, H, lev, hc)
                dw = Draw(r)
                v338_frame(dw, W, H, S, case, after)
                out.paste(r.img().crop((x0c, 0, x1c, hc)), (20 + ci * (cw + 16), y + 22))
            y += hc + 56
    return out


def v338_zoom(W=2048, H=1536):
    """2x zooms on black: the user's screenshot case and the left-alone case, before | after"""
    S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
    X, Y, sx, sy = canvas(W, H, S)
    c = (int(X(388)) - 40, 0, int(X(452) + 32 * sy), int(Y(80)))
    tiles = []
    for ci in (0, 1, 2):
        for after in (False, True):
            r, _ = plain(W, H, 0.02, c[3])
            dw = Draw(r)
            v338_frame(dw, W, H, S, V338_CASES[ci], after)
            tiles.append(("%s: %s" % ("after" if after else "before", V338_CASES[ci][0]),
                          r.img().crop(c).resize(((c[2] - c[0]) * 2, (c[3] - c[1]) * 2), Image.LANCZOS)))
    tw, th_ = tiles[0][1].size
    out = Image.new("RGB", (40 + (tw + 20) * 2, 30 + (th_ + 40) * 3), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 17)
    for i, (name, t) in enumerate(tiles):
        x, y = 20 + (i % 2) * (tw + 20), 16 + (i // 2) * (th_ + 40)
        d.text((x, y), name + " (on black, 2x)", font=fs, fill=(170, 166, 160))
        out.paste(t, (x, y + 22))
    return out


def v338_sizes():
    """after: the Both left-alone case on black at 1280x720, 1920x1080, 2560x1440 (16:9), 1024x768 (4:3), 1920x1200
    (16:10), crops 1:1"""
    sizes = [(1280, 720), (1920, 1080), (2560, 1440), (1024, 768), (1920, 1200)]
    crops = []
    for (W, H) in sizes:
        r, S = plain(W, H, 0.02)
        dw = Draw(r)
        v338_frame(dw, W, H, S, V338_CASES[1], True)
        X, Y, sx, sy = canvas(W, H, S)
        crops.append(("%dx%d" % (W, H), r.img().crop((int(X(388)) - 30, 0, int(X(452) + 32 * sy), min(int(Y(80)), H // 4)))))
    Wt = sum(c.size[0] for _, c in crops) + 20 * (len(crops) + 1)
    out = Image.new("RGB", (Wt, max(c.size[1] for _, c in crops) + 60), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs = ImageFont.truetype(FONT, 17)
    x = 20
    for name, c in crops:
        out.paste(c, (x, 40))
        d.text((x, 12), name, font=fs, fill=(160, 156, 150))
        x += c.size[0] + 20
    return out


def lum(c):
    return 0.299 * ((c >> 16) & 255) + 0.587 * ((c >> 8) & 255) + 0.114 * (c & 255)


def v338_measure():
    """a x luminance over black (0..255) of each element at rest: v3.37 against v3.38 (the target for warnings: >= ~70)"""
    rows = []
    t = 0.05
    br = 0.8 + 0.2 * math.sin(t * 2.618)
    for name, small, away in (("Both small ring", True, False), ("Both small ring, away", True, True),
                              ("Companion main ring, away", False, True)):
        b = REST_A * (0.5 if away else 1.0)                      # v3.37: rest 0.55 for both rings, away x 0.5
        a = REST2_A * (AWAY_A if away else 1.0)
        al = max(REST2_A * br, ALERT_FLOOR)
        bq = b * br
        rows.append((name, [
            ("empty band", lum(0x140C10) * 0.80 * b, lum(EMPTY338[0]) * 0.85 * a),
            ("hairline", 0.0, lum(HAIR_RGB) * HAIR_A * a),
            ("unlit thorn", lum(0x6A6070) * 0.9 * b, lum(THORN_OFF338) * min(1.0, a)),
            ("Haunted fill inner", lum(0x420E20) * 0.95 * b, lum(lerpc(0x420E20, 0x9A3050, 0.35)) * min(1.0, 1.15 * a)),
            ("Haunted fill outer", lum(0x9A3050) * 0.95 * b, lum(0x9A3050) * min(1.0, 1.15 * a)),
            ("notch cut", lum(NOTCH_RGB) * 0.72 * b, lum(NOTCH_RGB) * 0.95 * al),
            ("next mark", lum(NEXT_RGB) * 0.70 * b, lum(NEXT_RGB) * 0.95 * al),
            ("next arc", lum(NEXT_RGB) * 0.30 * b, lum(NEXT_RGB) * 0.55 * al),
            ("glyph figure", lum(TAG_COL) * 0.90 * bq, lum(TAG_COL) * 1.0 * al),
            ("glyph claw (bright end)", lum(CLAW_RGB) * 0.90 * bq, lum(CLAW338) * 1.0 * al),
            ("glyph claw (faint end)", lum(CLAW_RGB) * 0.25 * bq, lum(CLAW338) * 0.60 * al),
        ]))
    lines = []
    for name, els in rows:
        lines.append(name + ":")
        for en, b, a in els:
            lines.append("  %-24s %5.1f -> %5.1f" % (en, b, a))
    return "\n".join(lines)


# ---------------------------------------------------------------- the Performance readout's place (perfhud_sim.h PerfPlace)
def perf_font():
    """kPerfFont from src/perfhud_sim.h (the dll's own table)"""
    src = open(os.path.join(HERE, "..", "src", "perfhud_sim.h"), encoding="ascii").read()
    f = {}
    import re as _re
    for m in _re.finditer(r"\{ '(.)', \{ ([^}]*) \} \}", src):
        f[m.group(1)] = [int(x, 0) for x in m.group(2).split(",")]
    return f


PERF_FIT_CHARS = 20


def perf_scale(h):
    return min(max((h + 240) // 480, 1), 8)


def canvas_x(cx, W, S):
    return 0.5 * W + (cx - 256.0) * W / (512.0 * S)


def pe_hud_box(W, H, S, X=24.0, Yc=24.0, sc=1.0, aspect=0.0):
    """PerfPeHudBox: Playable Eileen's ring (91 x Scale canvas units, x by its aspect A) and 2 units of shake / halo"""
    A = aspect if aspect > 0 else (H / 448.0) / (W / 512.0) * S
    m = 2.0 * sc
    return (canvas_x((X - m) * A, W, S), (Yc - m) * H / 448.0, canvas_x((X + 91.0 * sc + m) * A, W, S),
            (Yc + 91.0 * sc + m) * H / 448.0)


def stock_gauge_box(W, H, S):
    return canvas_x(21.0, W, S), 21.0 * H / 448.0, canvas_x(223.0, W, S), 71.0 * H / 448.0


def readout_box(bx, by, u):
    return bx, by, bx + (PERF_FIT_CHARS * 6 - 1 + 6) * u, by + (4 * 9 - 2 + 6) * u


def meet(a, b, gap):
    return not (a[2] + gap <= b[0] or b[2] + gap <= a[0] or a[3] + gap <= b[1] or b[3] + gap <= a[1])


def perf_place(W, H, hud):
    """PerfPlace: 0 the corner, 1 under the HUD, 2 right of it, 3 the corner over it (nothing else fits)"""
    u = perf_scale(H)
    gap, step = float(u), 8.0 * H / 448.0
    bx, by = 3.0 * u, 3.0 * u
    if hud is None or not meet(readout_box(bx, by, u), hud, gap):
        return 0, bx, by
    b = readout_box(bx, hud[3] + step, u)
    if b[3] <= H - 3.0 * u:
        return 1, bx, hud[3] + step
    b = readout_box(hud[2] + step, by, u)
    if b[2] <= W - 3.0 * u:
        return 2, hud[2] + step, by
    return 3, bx, by


def draw_readout(img, bx, by, u, font):
    lines = [("60.0 FPS  16.7 MS", (124, 224, 140)), ("1% LOW 55.8 FPS", (240, 200, 80)), ("REFINED 0.05 MS", (232, 228, 220)),
             ("MEM 743 / 2047 MB", (124, 224, 140))]
    d = ImageDraw.Draw(img, "RGBA")
    wmax = max(len(t) * 6 - 1 for t, _ in lines)
    d.rectangle([bx, by, bx + (wmax + 6) * u - 1, by + (4 * 9 - 2 + 6) * u - 1], fill=(0, 0, 0, 140))
    for li, (t, col) in enumerate(lines):
        x = bx + 3 * u
        y = by + 3 * u + li * 9 * u
        for ch in t:
            g = font.get(ch)
            if g:
                for r_, bits in enumerate(g):
                    for c_ in range(5):
                        if bits & (0x10 >> c_):
                            d.rectangle([x + c_ * u, y + r_ * u, x + (c_ + 1) * u - 1, y + (r_ + 1) * u - 1], fill=col)
            x += 6 * u


def draw_pe_hud(img, W, H, S, X=24.0, Yc=24.0, sc=1.0):
    """a stand-in of Playable Eileen's chain ring and plus at its place (PE's HudVert mapping)"""
    A = (H / 448.0) / (W / 512.0) * S
    d = ImageDraw.Draw(img, "RGBA")
    ox, oy = (X + 45.5 * sc) * A, Yc + 45.5 * sc

    def P(lx, ly):
        return canvas_x(ox + lx * sc * A, W, S), (oy + ly * sc) * H / 448.0
    for i in range(48):
        a0, a1 = 2 * math.pi * i / 48, 2 * math.pi * (i + 1) / 48
        for rr, col in ((44.0, (150, 150, 150, 255)), (36.0, (90, 90, 90, 255))):
            d.line([P(rr * math.sin(a0), -rr * math.cos(a0)), P(rr * math.sin(a1), -rr * math.cos(a1))], fill=col, width=max(2, int(H / 300)))
    e, hh = 28.0, 9.6
    for (x0, y0, x1, y1) in ((-e, -hh, e, hh), (-hh, -e, hh, e)):
        p0, p1 = P(x0, y0), P(x1, y1)
        d.rectangle([p0[0], p0[1], p1[0], p1[1]], fill=(120, 220, 230, 255))


def perf_layout_sheet(pe=True):
    """the readout before (always the corner) and after (PerfPlace) over Playable Eileen's HUD (or the stock gauge) at 4:3
    2048x1536 and 1024x768, 16:10 1920x1200, 16:9 1280x720 / 1920x1080 / 2560x1440; each frame scaled to 480 px wide"""
    font = perf_font()
    sizes = [(2048, 1536), (1024, 768), (1920, 1200), (1280, 720), (1920, 1080), (2560, 1440)]
    tw = 480
    tiles = []
    report = []
    for (W, H) in sizes:
        S = (W / H) / (4.0 / 3.0) if W / H > 4.0 / 3.0 + 1e-3 else 1.0
        hud = pe_hud_box(W, H, S) if pe else stock_gauge_box(W, H, S)
        u = perf_scale(H)
        for after in (False, True):
            img = Image.fromarray((backdrop(W, H) * 255 + 0.5).astype(np.uint8))
            d = ImageDraw.Draw(img, "RGBA")
            x43 = 0.5 * W - 0.5 * W / S
            d.rectangle([x43, 0, W - x43, H - 1], outline=(70, 70, 90, 255), width=max(1, H // 500))
            if pe:
                draw_pe_hud(img, W, H, S)
            else:
                d.rectangle([hud[0], hud[1] + 0.05 * (hud[3] - hud[1]), hud[2], hud[3]], outline=(160, 170, 150, 255), width=max(2, H // 400))
            d.rectangle([hud[0], hud[1], hud[2], hud[3]], outline=(255, 80, 80, 200), width=max(1, H // 700))
            where, bx, by = (0, 3.0 * u, 3.0 * u) if not after else perf_place(W, H, hud)
            draw_readout(img, bx, by, u, font)
            hit = meet(readout_box(bx, by, u), hud, 0.0)
            if after:
                report.append("%dx%d: %s (HUD box %.0f..%.0f x %.0f..%.0f px; readout box from %.0f, %.0f, u %d)%s" % (
                    W, H, ["corner", "under the HUD", "right of the HUD", "corner (over it)"][where], hud[0], hud[2], hud[1],
                    hud[3], bx, by, u, "" if not hit else " OVERLAP"))
            sc_ = tw / float(W)
            tiles.append(("%dx%d %s%s" % (W, H, "after" if after else "before", " (overlap)" if hit else ""),
                          img.crop((0, 0, W, int(H * 0.45))).resize((tw, int(H * 0.45 * sc_)), Image.LANCZOS)))
    th_ = max(t.size[1] for _, t in tiles)
    out = Image.new("RGB", (20 + (tw + 20) * 4, 50 + (th_ + 40) * 3), (22, 22, 22))
    d = ImageDraw.Draw(out)
    fs, ft = ImageFont.truetype(FONT, 16), ImageFont.truetype(FONT, 22)
    d.text((20, 10), "v3.38 HB338: the Performance readout's place over %s (top 45 %% of each frame; red = the HUD box)" % (
        "Playable Eileen's HUD (X 24, Y 24, Scale 1)" if pe else "the stock gauge (no Playable Eileen HUD)"), font=ft,
           fill=(200, 196, 188))
    for i, (name, t) in enumerate(tiles):
        x, y = 20 + (i % 4) * (tw + 20), 44 + (i // 4) * (th_ + 40)
        d.text((x, y), name, font=fs, fill=(200, 160, 150) if "overlap" in name else (160, 156, 150))
        out.paste(t, (x, y + 22))
    return out, report


if __name__ == "__main__" and "--v338" in sys.argv:  # v3.38 HB338
    OUT338 = os.path.normpath(sys.argv[sys.argv.index("--v338") + 1]) if len(sys.argv) > sys.argv.index("--v338") + 1 \
        else os.path.normpath(os.path.join(HERE, "..", "..", "renders"))
    os.makedirs(OUT338, exist_ok=True)
    assert tiny_width("PLAYER") == 27, tiny_width("PLAYER")
    print(v338_measure())
    v338_sheet().save(os.path.join(OUT338, "v338_curse_before_after_2048x1536.png"))
    v338_zoom().save(os.path.join(OUT338, "v338_curse_black_zoom2x.png"))
    v338_sizes().save(os.path.join(OUT338, "v338_curse_sizes_black.png"))
    for pe in (True, False):
        s, rep = perf_layout_sheet(pe)
        s.save(os.path.join(OUT338, "v338_perf_place_%s.png" % ("pe_hud" if pe else "stock_gauge")))
        print("\n".join(rep))
    print("wrote", OUT338)
    sys.exit(0)


if __name__ == "__main__" and "--v335" in sys.argv:  # v3.35 CM335
    OUT335 = os.path.normpath(sys.argv[sys.argv.index("--v335") + 1]) if len(sys.argv) > sys.argv.index("--v335") + 1 \
        else os.path.normpath(os.path.join(HERE, "..", "..", "renders"))
    os.makedirs(OUT335, exist_ok=True)
    v335_measure()
    v335_target_sheet().save(os.path.join(OUT335, "v335_curse_target_sheet.png"))
    v335_zoom().save(os.path.join(OUT335, "v335_curse_target_zoom3x.png"))
    v335_sizes().save(os.path.join(OUT335, "v335_curse_target_sizes.png"))
    v335_label_compare().save(os.path.join(OUT335, "v335_curse_label_compare.png"))
    print("wrote", OUT335)
    sys.exit(0)


if __name__ == "__main__" and "--v332locked" in sys.argv:  # v3.32 MERGE
    OUTL = os.path.normpath(os.path.join(HERE, "..", "..", "renders"))
    os.makedirs(OUTL, exist_ok=True)
    locked_sheet().save(os.path.join(OUTL, "v332_curse_both_locked.png"))
    print("wrote", OUTL)
    sys.exit(0)


if __name__ == "__main__" and "--v332" in sys.argv:
    OUT332 = os.path.normpath(os.path.join(HERE, "..", "..", "renders"))
    os.makedirs(OUT332, exist_ok=True)
    both_layout_check()
    both_sheet().save(os.path.join(OUT332, "v332_curse_both_sheet.png"))
    both_sizes().save(os.path.join(OUT332, "v332_curse_both_sizes.png"))
    a = both_scene(1920, 1080)
    a.save(os.path.join(OUT332, "v332_scene_both_1080p_dark.png"))
    a.crop((1260, 0, 1660, 220)).resize((1200, 660), Image.LANCZOS).save(os.path.join(OUT332, "v332_curse_both_zoom3x.png"))
    both_scene(1920, 1080, bright=True).save(os.path.join(OUT332, "v332_scene_both_1080p_bright.png"))
    print("wrote", OUT332)
    sys.exit(0)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    a = scene(1920, 1080)
    a.save(os.path.join(OUT, "v330_scene_1080p_dark.png"))
    a.crop((560, 330, 1640, 960)).resize((1620, 945), Image.LANCZOS).save(os.path.join(OUT, "v330_scene_bars_zoom.png"))
    scene(1920, 1080, bright=True).save(os.path.join(OUT, "v330_scene_1080p_bright.png"))
    scene(1024, 768).save(os.path.join(OUT, "v330_scene_768p_4x3.png"))
    scene(2560, 1080).save(os.path.join(OUT, "v330_scene_2560x1080_ultrawide.png"))
    s, nt = bar_sheet("bevel")
    s.save(os.path.join(OUT, "v330_bars_sheet_bevel.png"))
    bar_sheet("blade")[0].save(os.path.join(OUT, "v330_bars_sheet_blade.png"))
    bar_compare().save(os.path.join(OUT, "v330_bars_compare_v329_bevel_blade.png"))
    curse_sheet().save(os.path.join(OUT, "v330_curse_designs.png"))
    ring_steps().save(os.path.join(OUT, "v330_curse_ring_pulse.png"))
    print("wrote", OUT)
