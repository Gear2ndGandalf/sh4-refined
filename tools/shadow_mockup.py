"""v3.40 SH340 / v3.41 SH341: before / after mock-ups of the shadow edge from the mask math of src/shadows_sim.h.

    py -3.10 shadow_mockup.py [out_dir] [backdrop.dds]

Emulates what the GPU draws, pixel by pixel (D3D9 sampling rules: pixel centres at integers, texel k at (k + 0.5) / n,
bilinear, CLAMP; every target 8 bits a channel): the game's mask (hard-edged triangles of a figure's shadow cast along
the floor, alpha = TEXTUREFACTOR), the stock composite (the mask twice, offsets 0 and -2 px, SRCALPHA / INVSRCALPHA),
v3.40's Softer (two 4 x 4 box passes to the quarter size, ONE 4x composite with taps at +-0.82 quarter texels) and
v3.41's Soft (ShMakePlan's plan, ported here: down in 2x steps (the first one to coverage), Kawase passes at the small
level, up in 2x steps, the composite from the half size, A = K (T + T (1 - T))). The backdrop is the game folder's
preview.dds (read only; a 1280 x 720 BGRX picture Refined saved) or a plain floor. Darkness: stage 1's 0x32 x Playable
Eileen's ShadowDarkness 150 % = 0x4B (the user's setting).
Writes (at 2560 x 1440, the user's resolution):
  shadow_edges_1440.png    the figure's shadow on a plain floor: Stock | v3.40 Softer | v3.41 Soft (shown at half size)
  shadow_zoom_1440.png     a 4x zoom (nearest) of a slanted leg edge, as drawn and with its contrast raised x8 (the grid)
  shadow_profiles_1440.png the darkening across a straight edge and across a 6 px / 20 px bar
  shadow_split_1440.png    the Shadows page's split view over the backdrop, Stock | Soft (shown at 1280 x 720)
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'mockups')
DDS = sys.argv[2] if len(sys.argv) > 2 else 'preview.dds'
SOFT_PX = 12.0                       # SH_SOFT_PX720
LEVELS_MAX, KAWASE_MAX, RT_MIN = 4, 4, 8
OFF_LO, LAST_LO, OFF_HI, OFF_STEPS = 0.5, 0.0, 1.5, 12
TENT, NORM_GAIN, WIDTH_TOL = 0.5, 32.0, 0.03
COMP_X0, STOCK_OFF = -1.0, 2.0
STRIP = 512
TF_ALPHA = round(0x32 * 1.5) / 255.0  # 0x4B
NORM, DOWN, KAWASE, UP, COMP = range(5)
MASK, L1, X = 0, 1, 5


def half(n):
    return (n + 1) // 2


def core(t):
    return t + t * (1 - t)


def q8(a):
    """an 8-bit render target"""
    return np.floor(np.clip(a, 0, 1) * 255 + 0.5) / 255


# ---------------------------------------------------------------- the plan (as shadows_sim.h ShMakePlan)
def build_steps(levels, kawase, off, w, h):
    W, H = [w], [h]
    for _ in range(levels):
        W.append(half(W[-1])), H.append(half(H[-1]))

    def size(b):
        return (W[levels], H[levels]) if b == X else (W[b], H[b])
    steps = []

    def add(kind, src, dst, tap):
        sw, sh = size(src)
        dw, dh = (w, h) if kind == COMP else size(dst)
        steps.append((kind, src, dst, sw, sh, dw, dh, tap / sw, tap / sh))
    for k in range(1, levels + 1):
        add(NORM if k == 1 else DOWN, k - 1, k, 1.0)
    cur = levels
    for k in range(kawase):
        dst = levels if cur == X else X
        add(KAWASE, cur, dst, off[k])
        cur = dst
    for k in range(levels, 1, -1):
        add(UP, cur, k - 1, TENT)
        cur = k - 1
    add(COMP, cur, -1, TENT)
    return steps, W, H


def tap1(t, u):
    n = len(t)
    s = u * n - 0.5
    i0 = np.floor(s)
    f = s - i0
    i0 = i0.astype(int)
    return t[np.clip(i0, 0, n - 1)] * (1 - f) + t[np.clip(i0 + 1, 0, n - 1)] * f


def run1d(levels, kawase, off, a):
    steps, _, _ = build_steps(levels, kawase, off, len(a), len(a))
    buf = {MASK: a}
    out = None
    for kind, src, dst, sw, sh, dw, dh, du, dv in steps:
        j = np.arange(dw, dtype=np.float64)
        u = (j - COMP_X0) / dw if kind == COMP else (j + 0.5) / dw
        r = 0.5 * (tap1(buf[src], u - du) + tap1(buf[src], u + du))
        if kind == COMP:
            out = r
        else:
            buf[dst] = r
    return core(out)


def width1090(p):
    lo, hi = p.min(), p.max()
    t10, t90 = lo + 0.1 * (hi - lo), lo + 0.9 * (hi - lo)

    def cross(t):
        i = np.nonzero((p[:-1] < t) & (p[1:] >= t))[0][0]
        return i + (t - p[i]) / (p[i + 1] - p[i])
    return cross(t90) - cross(t10)


def offsets(n, last):
    return [last if k == n - 1 else OFF_LO for k in range(n)]


def chain_width(levels, kawase, off):
    period = 1 << levels
    step, n = (period // 4, 4) if period >= 4 else (1, period)
    s = 0.0
    for k in range(n):
        e = STRIP // 2 + k * step
        s += width1090(run1d(levels, kawase, off, (np.arange(STRIP) >= e).astype(np.float64)))
    return s / n


def off_at(i):
    return LAST_LO + (OFF_HI - LAST_LO) * i / OFF_STEPS


TAB = {}


def table():
    if not TAB:
        for L in range(1, LEVELS_MAX + 1):
            for n in range(KAWASE_MAX + 1):
                w0 = chain_width(L, 0, [])
                TAB[L, n] = [w0] * (OFF_STEPS + 1) if n == 0 else [chain_width(L, n, offsets(n, off_at(i))) for i in range(OFF_STEPS + 1)]
    return TAB


def make_plan(w, h, soft_px=SOFT_PX):
    t = table()
    W, H = [w], [h]
    lavail = 0
    for k in range(1, LEVELS_MAX + 1):
        W.append(half(W[-1])), H.append(half(H[-1]))
        if W[k] >= RT_MIN and H[k] >= RT_MIN and lavail == k - 1:
            lavail = k
    target = min(max(soft_px, 2.0), 24.0) * h / 720.0
    half_step = int((OFF_LO - LAST_LO) * OFF_STEPS / (OFF_HI - LAST_LO) + 0.5)
    pick = None
    for tier in (0, 1):
        for L in range(lavail, 0, -1):
            for n in range(KAWASE_MAX, -1, -1):
                tab = t[L, n]
                o = -1.0
                if n == 0:
                    o = OFF_LO if abs(tab[0] - target) <= WIDTH_TOL * target else -1.0
                else:
                    for i in range(0 if tier else half_step, OFF_STEPS):
                        a, b = tab[i], tab[i + 1]
                        if a <= target <= b and b > a:
                            o = off_at(i) + (off_at(i + 1) - off_at(i)) * (target - a) / (b - a)
                            break
                if o >= 0 and pick is None:
                    pick = (L, n, o)
    if pick is None:
        best = 1e9
        for L in range(lavail, 0, -1):
            for n in range(KAWASE_MAX + 1):
                for e in (0, OFF_STEPS):
                    err = abs(t[L, n][e] - target)
                    if err < best - 0.01:
                        best, pick = err, (L, n, off_at(e))
    L, n, o = pick
    off = offsets(n, o)
    width = chain_width(L, n, off)
    steps, Ws, Hs = build_steps(L, n, off, w, h)
    return {'levels': L, 'kawase': n, 'off': off, 'target': target, 'width': width, 'steps': steps, 'w': Ws, 'h': Hs}


# ---------------------------------------------------------------- 2D (what the GPU draws)
def tap2(t, u, v):
    """bilinear sample of t (h x w) at normalised u (columns) / v (rows), CLAMP; u, v broadcastable"""
    h, w = t.shape
    sx, sy = u * w - 0.5, v * h - 0.5
    x0, y0 = np.floor(sx), np.floor(sy)
    fx, fy = sx - x0, sy - y0
    x0, y0 = x0.astype(int), y0.astype(int)
    x1, y1 = np.clip(x0 + 1, 0, w - 1), np.clip(y0 + 1, 0, h - 1)
    x0, y0 = np.clip(x0, 0, w - 1), np.clip(y0, 0, h - 1)
    a = t[y0, x0] * (1 - fx) + t[y0, x1] * fx
    b = t[y1, x0] * (1 - fx) + t[y1, x1] * fx
    return a * (1 - fy) + b * fy


def draw4(src, dw, dh, du, dv, comp=False):
    if comp:
        u = ((np.arange(dw) - COMP_X0) / dw)[None, :]
        v = ((np.arange(dh) - COMP_X0) / dh)[:, None]
    else:
        u = ((np.arange(dw) + 0.5) / dw)[None, :]
        v = ((np.arange(dh) + 0.5) / dh)[:, None]
    return 0.25 * (tap2(src, u - du, v - dv) + tap2(src, u + du, v - dv) + tap2(src, u - du, v + dv) + tap2(src, u + du, v + dv))


def stock_alpha(mask):
    """the share of the picture the two stock copies take (dst' = dst (1 - S) for a black shadow); 8-bit between"""
    h, w = mask.shape
    u = (np.arange(w) / w)[None, :]
    v = (np.arange(h) / h)[:, None]
    c1 = tap2(mask, u, v)                                       # copy 1: the game's quad at 0 (pixel i reads u = i / w)
    c2 = tap2(mask, u + STOCK_OFF / w, v + STOCK_OFF / h)       # copy 2 at -2
    return 1 - (1 - c1) * (1 - c2)


def old_alpha(mask):
    """v3.40 Softer at 1440 lines: 2 box passes (8-bit targets of a x coverage), one 4x composite, taps 0.82, x + x (1 - x)"""
    h, w = mask.shape
    low = mask
    for _ in range(2):
        lh, lw = low.shape
        low = q8(draw4(low, half(lw), half(lh), 1.0 / lw, 1.0 / lh))
    lh, lw = low.shape
    x = draw4(low, w, h, 0.82 / lw, 0.82 / lh, comp=True)
    return x + x * (1 - x)


def soft_alpha(mask, plan, a=TF_ALPHA):
    """v3.41 Soft: the plan's draws; the first pass turns the alpha into coverage (sat(32 c x), c = 1.01 / (32 a))"""
    c = min(1.0, 1.01 / (NORM_GAIN * a))
    tin = min(1.0, NORM_GAIN * c * a)
    K = (1 - (1 - a) ** 2) / core(tin)
    buf = {MASK: mask}
    for kind, src, dst, sw, sh, dw, dh, du, dv in plan['steps']:
        r = draw4(buf[src], dw, dh, du, dv, comp=kind == COMP)
        if kind == NORM:
            r = np.clip(NORM_GAIN * c * r, 0, 1)
        if kind == COMP:
            return K * core(r)
        buf[dst] = q8(r)


def figure_tris(cx, top, hgt, shear=0.0):
    """ShFigure (shadows_sim.h), sheared along the floor (x += shear (feet - y)): a shadow cast by a low light"""
    u = hgt
    feet = top + u
    tris = []
    hx, hy, r = cx, top + 0.065 * u, 0.065 * u
    for k in range(16):
        a0, a1 = 2 * math.pi * k / 16, 2 * math.pi * (k + 1) / 16
        tris.append([(hx, hy), (hx + r * math.cos(a0), hy + r * math.sin(a0)), (hx + r * math.cos(a1), hy + r * math.sin(a1))])

    def quad(p):
        tris.append([p[0], p[1], p[2]])
        tris.append([p[0], p[2], p[3]])
    quad([(cx - 0.025 * u, top + 0.12 * u), (cx + 0.025 * u, top + 0.12 * u), (cx + 0.03 * u, top + 0.16 * u), (cx - 0.03 * u, top + 0.16 * u)])
    quad([(cx - 0.13 * u, top + 0.16 * u), (cx + 0.13 * u, top + 0.16 * u), (cx + 0.10 * u, top + 0.54 * u), (cx - 0.10 * u, top + 0.54 * u)])
    quad([(cx - 0.155 * u, top + 0.17 * u), (cx - 0.115 * u, top + 0.18 * u), (cx - 0.14 * u, top + 0.52 * u), (cx - 0.18 * u, top + 0.51 * u)])
    quad([(cx + 0.115 * u, top + 0.18 * u), (cx + 0.155 * u, top + 0.17 * u), (cx + 0.18 * u, top + 0.51 * u), (cx + 0.14 * u, top + 0.52 * u)])
    quad([(cx - 0.10 * u, top + 0.53 * u), (cx - 0.012 * u, top + 0.53 * u), (cx - 0.03 * u, top + 1.0 * u), (cx - 0.085 * u, top + 1.0 * u)])
    quad([(cx + 0.012 * u, top + 0.53 * u), (cx + 0.10 * u, top + 0.53 * u), (cx + 0.085 * u, top + 1.0 * u), (cx + 0.03 * u, top + 1.0 * u)])
    return [[(x + shear * (feet - y), y) for x, y in t] for t in tris]


def figure_mask(w, h, shear=0.45):
    """hard-edged (not anti-aliased) like the stencil mask: a pixel is in when its centre is; alpha = TEXTUREFACTOR's"""
    img = Image.new('L', (w, h), 0)
    dr = ImageDraw.Draw(img)
    top, hgt = h * 0.14, h * 0.78
    cx = w * 0.42
    for t in figure_tris(cx, top, hgt, shear):
        dr.polygon([(x - 0.5, y - 0.5) for x, y in t], fill=255)
    return (np.asarray(img, dtype=np.float64) > 127).astype(np.float64) * TF_ALPHA, (cx, top, hgt, shear)


def floor(w, h):
    """a plain lit floor with a faint grain (easier to judge an edge on than a dark room)"""
    rng = np.random.default_rng(341)
    y = np.linspace(0.66, 0.52, h)[:, None, None]
    g = 1 + 0.02 * rng.standard_normal((h, w, 1))
    return np.clip(y * g * np.array([0.95, 0.90, 0.82]), 0, 1)


def backdrop(w, h):
    if os.path.exists(DDS):
        with open(DDS, 'rb') as f:
            b = f.read()
        hh, ww = int.from_bytes(b[12:16], 'little'), int.from_bytes(b[16:20], 'little')
        if len(b) >= 128 + ww * hh * 4 and int.from_bytes(b[88:92], 'little') == 32:
            a = np.frombuffer(b[128:128 + ww * hh * 4], np.uint8).reshape(hh, ww, 4)[:, :, [2, 1, 0]]
            return np.asarray(Image.fromarray(a).resize((w, h), Image.BILINEAR), np.float64) / 255.0
    y = np.linspace(0.30, 0.55, h)[:, None, None]
    return np.broadcast_to(y * np.array([0.9, 0.85, 0.8]), (h, w, 3)).copy()


def gauss(pic, sigma):
    """a separable Gaussian blur (CLAMP) of an h x w x 3 picture"""
    r = int(math.ceil(3 * sigma))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    for axis in (0, 1):
        pad = [(0, 0)] * 3
        pad[axis] = (r, r)
        q = np.pad(pic, pad, mode='edge')
        pic = sum(k[i] * np.take(q, range(i, i + pic.shape[axis]), axis=axis) for i in range(2 * r + 1))
    return pic


def shade(bg, share):
    return q8(bg * (1 - share[:, :, None]))


def to_img(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))


def label(img, text, xy=(8, 6)):
    d = ImageDraw.Draw(img)
    x, y = xy
    for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        d.text((x + ox, y + oy), text, fill=(0, 0, 0))
    d.text((x, y), text, fill=(255, 255, 255))
    return img


def plan_text(p):
    return '%d level(s) to %dx%d, %d blur pass(es), %d draws: %.1f px' % (p['levels'], p['w'][p['levels']], p['h'][p['levels']], p['kawase'],
                                                                     len(p['steps']), p['width'])


def main():
    os.makedirs(OUT, exist_ok=True)
    w, h = 2560, 1440
    plan = make_plan(w, h)
    print('2560x1440 Soft: %s (asked %.1f; offsets %s)' % (plan_text(plan), plan['target'], ', '.join('%.3f' % o for o in plan['off'])))
    for hh in (480, 720, 1080, 2160):
        p = make_plan(hh * 16 // 9, hh)
        print('%dx%d Soft: %s (asked %.1f)' % (hh * 16 // 9, hh, plan_text(p), p['target']))
    mask, (cx, top, hgt, shear) = figure_mask(w, h)
    shares = [('Stock (the game)', stock_alpha(mask)), ('v3.40 Softer (12 px, one 4x stretch)', old_alpha(mask)),
              ('v3.41 Soft (%s)' % plan_text(plan), soft_alpha(mask, plan))]
    a_in = 1 - (1 - TF_ALPHA) ** 2
    for t, s in shares:
        print('  %s: the darkest %.4f (stock inside %.4f)' % (t, s.max(), a_in))
    # 1. the whole figure on a floor, three panels at half size
    bg = floor(w, h)
    tiles = [label(to_img(shade(bg, s)).resize((w // 2, h // 2), Image.LANCZOS), t) for t, s in shares]
    out = Image.new('RGB', (3 * (w // 2) + 16, h // 2 + 26), (24, 24, 24))
    for k, t in enumerate(tiles):
        out.paste(t, (k * (w // 2 + 8), 26))
    label(out, '2560x1440, a figure\'s shadow cast along a plain floor, darkness 0x4B (stage 1 x 150 %%); shown at half size', (8, 6))
    out.save(os.path.join(OUT, 'shadow_edges_1440.png'))
    # 2. a 4x zoom of a slanted edge (the right leg's outer edge near the knee), as drawn and with the contrast raised x8
    ky = int(top + 0.80 * hgt)
    kx = int(cx + 0.093 * hgt + shear * (top + hgt - ky))
    x0, y0, cw, ch = kx - 60, ky - 50, 120, 100
    rows = []
    for gain in (1, 16):
        tiles = []
        for t, s in shares:
            pic = shade(np.full((h, w, 3), 0.6), s)[y0 - 8:y0 + ch + 8, x0 - 8:x0 + cw + 8]
            if gain > 1:  # a high pass: the picture minus its own 2 px Gaussian blur; a smooth ramp leaves ~0, a grid stays
                pic = np.clip(0.5 + gain * (pic - gauss(pic, 2.0)), 0, 1)
            im = to_img(pic[8:-8, 8:-8]).resize((cw * 4, ch * 4), Image.NEAREST)
            tiles.append(label(im, '%s%s' % (t.split(' (')[0], '' if gain == 1 else ': high pass x16 (a grid / steps show)')))
        rows.append(tiles)
    out = Image.new('RGB', (3 * (cw * 4) + 16, 2 * (ch * 4) + 8 + 26), (24, 24, 24))
    for r, tiles in enumerate(rows):
        for k, t in enumerate(tiles):
            out.paste(t, (k * (cw * 4 + 8), 26 + r * (ch * 4 + 8)))
    label(out, '4x zoom (nearest) of the shadow\'s slanted leg edge at 2560x1440 on a flat grey floor; bottom: the picture minus '
               'its 2 px blur, x16: a smooth edge is flat grey, blocks / stair steps show as a pattern', (8, 6))
    out.save(os.path.join(OUT, 'shadow_zoom_1440.png'))
    # 3. profiles across a straight edge and across thin bars (1D: the same draws)
    im = Image.new('RGB', (1400, 760), (250, 250, 250))
    d = ImageDraw.Draw(im)
    i = np.arange(STRIP)
    a = TF_ALPHA

    def stock1d(m):
        c1 = a * tap1(m, i / STRIP)
        c2 = a * tap1(m, (i + STOCK_OFF) / STRIP)
        return (1 - (1 - c1) * (1 - c2)) / a_in

    def old1d(m):
        x = a * m
        for _ in range(2):
            n = len(x)
            mm = half(n)
            u = (np.arange(mm) + 0.5) / mm
            x = 0.5 * (tap1(x, u - 1.0 / n) + tap1(x, u + 1.0 / n))
        n = len(x)
        u = (i - COMP_X0) / STRIP
        y = 0.5 * (tap1(x, u - 0.82 / n) + tap1(x, u + 0.82 / n))
        return (y + y * (1 - y)) / a_in

    def new1d(m):
        return run1d(plan['levels'], plan['kawase'], plan['off'], m)
    curves = (('Stock', stock1d, (200, 0, 0)), ('v3.40 Softer', old1d, (0, 120, 0)), ('v3.41 Soft', new1d, (0, 0, 200)))
    panels = (('a straight edge', (i >= STRIP // 2).astype(np.float64), 90),
              ('a 20 px bar (a leg)', ((i >= STRIP // 2 - 10) & (i < STRIP // 2 + 10)).astype(np.float64), 90),
              ('a 6 px bar (a pipe, a thin arm)', ((i >= STRIP // 2 - 3) & (i < STRIP // 2 + 3)).astype(np.float64), 90))
    for pk, (name, m, n) in enumerate(panels):
        ox, oy, pw, ph = 50 + pk * 450, 120, 400, 560
        xs = np.arange(STRIP // 2 - n // 2, STRIP // 2 + n // 2)
        d.line([(ox, oy), (ox, oy + ph), (ox + pw, oy + ph)], fill=(0, 0, 0))
        d.text((ox, oy - 40), name, fill=(0, 0, 0))
        for k, (nm, fn, col) in enumerate(curves):
            p = fn(m)
            pts = [(ox + (j - xs[0]) * pw / (n - 1), oy + ph - p[j] * (ph - 20)) for j in xs]
            d.line(pts, fill=col, width=3)
            d.text((ox + 4, oy - 24 + 0 * k), '', fill=col)
            d.text((ox + 10, oy + 10 + 16 * k), '%s: the darkest %.0f %%' % (nm, 100 * p.max()), fill=col)
    d.text((50, 20), 'the darkening at 2560x1440 (1 = a large shadow\'s inside: the same for all three), one pixel a step; '
                     'v3.41 Soft: %s (asked %.1f)' % (plan_text(plan), plan['target']), fill=(0, 0, 0))
    d.text((50, 40), 'Stock: red, v3.40 Softer: green, v3.41 Soft: blue. Thin parts get fainter with a wider blur (the core curve keeps more of them).',
           fill=(0, 0, 0))
    im.save(os.path.join(OUT, 'shadow_profiles_1440.png'))
    # 4. the Shadows page's split view over the backdrop
    bg = backdrop(w, h)
    st, so = shares[0][1], shares[2][1]
    sp = w // 2
    share = np.concatenate([st[:, :sp], so[:, sp:]], axis=1)
    pic = shade(bg, share)
    pic[:, sp - 2:sp + 2] = pic[:, sp - 2:sp + 2] * 0.5
    pic[:, sp:sp + 1] = pic[:, sp:sp + 1] * 0.25 + 0.75 * 0.88
    im = to_img(pic).resize((1280, 720), Image.LANCZOS)
    sx, sy = 1280 / 512 * (720 * 4 / 3) / 1280, 720 / 448
    ox = (1280 - 512 * sx) / 2

    def panel(im, x0, y0, x1, y1):
        ov = Image.new('RGBA', im.size, (0, 0, 0, 0))
        ImageDraw.Draw(ov).rectangle([ox + x0 * sx, y0 * sy, ox + x1 * sx, y1 * sy], fill=(0, 0, 0, 184))
        return Image.alpha_composite(im.convert('RGBA'), ov).convert('RGB')
    im = panel(im, 28, 40, 484, 168)
    im = panel(im, 40, 352, 472, 392)
    d = ImageDraw.Draw(im)
    for txt, x, y, c in (('Shadows', 40, 50, (255, 255, 255)), ('Shadow Edge', 40, 96, (200, 40, 40)), ('Soft', 364, 96, (255, 255, 255)),
                         ('Back', 40, 128, (255, 255, 255)), ('The shadows\' edge. Left of the line: Stock.', 64, 362, (255, 255, 255))):
        d.text((ox + x * sx, y * sy), txt, fill=c)
    label(im, 'mock-up: the Shadows page at 2560x1440, Stock | Soft (%.1f px)' % plan['width'], (8, 700))
    im.save(os.path.join(OUT, 'shadow_split_1440.png'))
    print('written to', os.path.abspath(OUT), '; backdrop', DDS if os.path.exists(DDS) else 'a plain floor')


if __name__ == '__main__':
    main()
