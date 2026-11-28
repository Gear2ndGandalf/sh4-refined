"""v3.59 FT359: offline renders of the enemy bars under the special filters (hudbars.inc HbBarQuads + HbThemeFill +
HbThemeOver with hbtheme_sim.h's palettes, read from the header itself).

The bars are drawn as hudbars.inc draws them (the primitive layer of tools/hudbars_mockup.py: untextured triangles,
alpha-blended). The backdrops are procedural (the corridor of hudbars_mockup.py) passed through a rough model of each
filter (Thermal Vision's cold ramp with a hot body, Spirit Vision's edge world with a glowing ghost, Night Vision's
green amplifier, Night Mono's grey), mixed with the plain picture by the Filter Strength as PvFsMix does. Only there to
judge the bars against: no game picture.
Run: py -3.10 tools/ft359_preview.py [outdir]   (default ../../renders)
"""
import math
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hudbars_mockup as hm  # noqa: E402  (Raster, Draw, C, lerpc, backdrop, silhouettes)

OUT = os.path.normpath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'renders'))
HDR = os.path.join(HERE, '..', 'src', 'hbtheme_sim.h')
LOOKS = ['stock', 'Thermal Vision', 'Spirit Vision', 'Night Vision', 'Night Mono']
FS = [('Low', 0.35), ('Medium', 0.65), ('High', 1.0), ('Ultra', 1.35)]


# ---------------------------------------------------------------- the palettes, read from hbtheme_sim.h
def read_header():
    src = open(HDR, encoding='utf-8').read()
    stock = re.search(r'#define HBT_STOCK_PAL(.*?)\n\n', src, re.S).group(1)
    table = re.search(r'static const HbtPal kHbtPal\[HBT_N\] = \{(.*?)\n\};', src, re.S).group(1)
    pals = [[int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{6})u', stock)]]
    body = table.split('HBT_STOCK_PAL,', 1)[1]
    nums = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{6})u', body)]
    assert len(pals[0]) == 18 and len(nums) == 18 * 4, (len(pals[0]), len(nums))
    for i in range(4):
        pals.append(nums[18 * i:18 * (i + 1)])
    hu = [float(x) for x in re.search(r'kHbtHeatU\[HBT_HEAT_N\] = \{([^}]*)\}', src).group(1).replace('f', '').split(',')]
    hc = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{6})u', re.search(r'kHbtHeatC\[HBT_HEAT_N\] = \{([^}]*)\}', src).group(1))]
    return pals, hu, hc


PALS, HEAT_U, HEAT_C = read_header()
NAMES = ['rim', 'back0', 'back1', 'chipHot', 'trail', 'sheen', 'guard', 'lock', 'f00', 'f01', 'f10', 'f11', 'f20', 'f21',
         'rev0', 'rev1', 'steel0', 'steel1']


def heat(u):
    u = min(max(u, 0.0), 1.0)
    i = 1
    while i < len(HEAT_U) - 1 and u > HEAT_U[i]:
        i += 1
    return hm.lerpc(HEAT_C[i - 1], HEAT_C[i], (u - HEAT_U[i - 1]) / (HEAT_U[i] - HEAT_U[i - 1]))


def theme(look, fsk):
    """HbtBuild"""
    li = LOOKS.index(look)
    if li == 0:
        k = over = 0.0
    else:
        k = min(max(fsk, 0.0), 1.0)
        over = min(max(fsk, 0.0), 1.35) if look in ('Night Vision', 'Night Mono') else k
    c = {n: hm.lerpc(PALS[0][i], PALS[li][i], k) for i, n in enumerate(NAMES)}
    t = dict(look=look, k=k, over=over, c=c, heat=look == 'Thermal Vision' and k > 0, flames=look == 'Spirit Vision' and over > 0,
             scan=look in ('Night Vision', 'Night Mono') and over > 0, halo=look == 'Night Vision' and over > 0,
             scanA=0.16 if look == 'Night Mono' else 0.22, grainA=0.25 if look == 'Night Mono' else 0.35, haloA=0.10, flameA=0.55,
             haloRgb=0x50FF50, grainRgb=0xFFFFFF if look == 'Night Mono' else 0xC8FFC0)
    t['grain'] = t['scan']
    return t


def fill_at(t, kind, u):
    """HbtFillAt"""
    c = t['c']
    if not t['heat']:
        return c['f%d0' % kind], c['f%d1' % kind]
    h = heat(u)
    return (hm.lerpc(PALS[0][8 + 2 * kind], h, t['k']), hm.lerpc(PALS[0][9 + 2 * kind], hm.lerpc(0, h, 0.62), t['k']))


def vnoise(x, y, seed):
    def rnd(a):
        a = (a ^ (a >> 16)) * 0x7FEB352D & 0xFFFFFFFF
        a = (a ^ (a >> 15)) * 0x846CA68B & 0xFFFFFFFF
        a ^= a >> 16
        return (a & 0xFFFFFF) / 16777216.0
    ix, iy = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - ix, y - iy
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x0, x1 = (ix * 73856093) & 0xFFFFFFFF, ((ix + 1) * 73856093) & 0xFFFFFFFF
    y0, y1 = (iy * 19349663) & 0xFFFFFFFF, ((iy + 1) * 19349663) & 0xFFFFFFFF
    a, b, c, d = rnd(seed ^ x0 ^ y0), rnd(seed ^ x1 ^ y0), rnd(seed ^ x0 ^ y1), rnd(seed ^ x1 ^ y1)
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy


# ---------------------------------------------------------------- HbBarQuads with the theme
class B:
    def __init__(self, x, y, shown, trail=None, kind=0, locked=False, boss=False, guard=None, rev=None, rev_a=0.0, seal=0.0,
                 a=1.0, heat_=0.0):
        self.__dict__.update(locals())
        self.trail = shown if trail is None else trail


def draw_bar(dw, d, H, t, now):
    C = hm.C
    c = t['c']
    W = H * 0.075 * (1.6 if d.boss else 1.0)
    hh = max(H * 0.0065, 4.0) * (1.25 if d.boss else 1.0)
    x0, x1, y1 = d.x - 0.5 * W, d.x + 0.5 * W, d.y
    y0 = y1 - hh
    px = max(H / 1080.0, 1.0)
    a = d.a * (1.0 if d.locked else 0.85)
    k = d.kind
    ghost = k == 1
    nq = 10 if d.boss else 4
    gl = max(math.floor(hh * 0.22 + 0.5), px) if ghost else 0.0
    hy1 = y1 - (gl + px if ghost else 0.0)
    xe = x0 + W * d.shown
    dw.rect(x0 - 2 * px, y0 - 2 * px, x1 + 2 * px, y1 + 2 * px, C(c['rim'], 0.22 * a))
    dw.rect(x0 - px, y0 - px, x1 + px, y1 + px, C(c['rim'], 0.85 * a))
    dw.rect(x0, y0, x1, y1, C(c['back0'], 0.80 * a), C(c['back1'], 0.80 * a))
    if d.trail > d.shown + 0.002:
        ch = hm.lerpc(c['trail'], c['chipHot'], d.heat_)
        dw.rect(xe, y0, x0 + W * d.trail, hy1, C(ch, 0.85 * a), C(ch, 0.62 * a))
    # HbThemeFill
    if not t['heat']:
        dw.rect(x0, y0, xe, hy1, C(c['f%d0' % k], 0.96 * a), C(c['f%d1' % k], 0.96 * a))
    else:
        xs = x0
        for s in range(8):
            xn = min(x0 + W * (s + 1) / 8.0, xe)
            if xn > xs:
                t0, b0 = fill_at(t, k, (xs - x0) / W)
                t1, b1 = fill_at(t, k, (xn - x0) / W)
                dw.quad([(xs, y0), (xn, y0), (xs, hy1), (xn, hy1)], [C(t0, 0.96 * a), C(t1, 0.96 * a), C(b0, 0.96 * a), C(b1, 0.96 * a)])
            xs = xn
            if xs >= xe:
                break
    dw.rect(x0, y0, xe, y0 + px, C(c['sheen'], 0.24 * a))
    if hy1 - y0 > 3 * px:
        dw.rect(x0, hy1 - px, xe, hy1, C(0, 0.25 * a))
    for i in range(1, nq):
        xq = x0 + W * i / nq
        dw.rect(xq - 0.5 * px, y0, xq + 0.5 * px, hy1, C(0, (0.45 if i * 2 == nq else 0.30) * a))
    dw.rect(x0, y0, x1, y0 + px, C(c['sheen'], 0.07 * a))
    if ghost and d.guard is not None:
        dw.rect(x0, y1 - gl, x0 + W * d.guard, y1, C(c['guard'], 0.85 * a), C(c['guard'], 0.55 * a))
    if d.locked:
        my, sz = 0.5 * (y0 + y1), 2.5 * px + 0.25 * hh
        for cx in (x0 - 3 * px - sz, x1 + 3 * px + sz):
            o = sz + 1.5 * px
            dw.quad([(cx, my - o), (cx + o, my), (cx - o, my), (cx, my + o)], [C(c['rim'], 0.8 * a)] * 4)
            dw.quad([(cx, my - sz), (cx + sz, my), (cx - sz, my), (cx, my + sz)], [C(c['lock'], 0.95 * a)] * 4)
    yb = y1 + px
    if d.rev_a > 0.004:  # HbReviveQuads (countdown / seal)
        rh = max(H * 0.0030, 2.0)
        ry0, ry1 = y1 + 3 * px, y1 + 3 * px + rh
        yb = ry1 + px
        ka = d.a * d.rev_a
        if d.seal < 1.0:
            kk = (1 - d.seal) * ka
            dw.rect(x0 - px, ry0 - px, x1 + px, ry1 + px, C(c['rim'], 0.80 * kk))
            dw.rect(x0, ry0, x1, ry1, C(0x100C18, 0.75 * kk))
            xr = x0 + W * (d.rev or 0.0)
            dw.rect(x0, ry0, xr, ry1, C(c['rev0'], 0.92 * kk), C(c['rev1'], 0.92 * kk))
            if d.rev and d.rev > 0.01:
                dw.rect(xr - px, ry0 - px, xr + px, ry1 + px, C(0xF4EEFF, 0.75 * kk))
        if d.seal > 0.0:
            kk = d.seal * ka
            g = 3 * px
            xc = 0.5 * (x0 + x1)
            for hx0, hx1 in ((x0, xc - g), (xc + g, x1)):
                dw.rect(hx0 - px, ry0 - px, hx1 + px, ry1 + px, C(c['rim'], 0.85 * kk))
                dw.rect(hx0, ry0, hx1, ry1, C(c['steel0'], 0.80 * kk), C(c['steel1'], 0.80 * kk))
            dw.rect(xc - 0.5 * px, ry0 - 3 * px, xc + 0.5 * px, ry1 + 2 * px, C(0xF2F4F8, 0.95 * kk), C(0x9AA2AE, 0.95 * kk))
    # HbThemeOver
    if t['look'] == 'stock':
        return
    if t['halo'] and xe > x0 + 0.5:
        cc, c0 = C(t['haloRgb'], t['haloA'] * t['over'] * a), C(t['haloRgb'], 0.0)
        dw.rect(x0 - 2 * px, y0 - 4 * px, xe + 2 * px, y0 - px, c0, cc)
        dw.rect(x0 - 2 * px, y1 + px, xe + 2 * px, y1 + 3 * px, cc, c0)
        dw.rect(x0 - 4 * px, y0 - px, x0 - px, y1 + px, cc, cc)
    if t['flames'] and xe > x0 + 2 * px:
        fw = xe - x0
        sd = ((int(d.shown * 977.0) ^ (k * 131)) & 1023) * 0.37
        ka = t['flameA'] * t['over'] * a
        n = max(1, min(14, int(fw / (5 * px))))
        dw.rect(x0, y0 - 1.5 * px, xe, y0, C(c['f%d0' % k], 0.0), C(c['f%d0' % k], 0.30 * t['over'] * a))
        for i in range(n):
            cx, hw = x0 + fw * (i + 0.5) / n, 0.425 * fw / n
            nz = vnoise(i * 1.7 + sd, now * 3.2, 0x5F1A3E)
            ht = hh * (0.35 + 1.05 * nz)
            lean = hw * 0.8 * (vnoise(i * 2.3 + sd, now * 2.1, 0x2C77) - 0.5)
            top, _ = fill_at(t, k, (i + 0.5) / n * fw / W)
            dw.tri([(cx - hw, y0), (cx + hw, y0), (cx + lean, y0 - ht)], [C(top, ka), C(top, ka), C(hm.lerpc(top, 0xFFFFFF, 0.5), 0.0)])
    if t['scan']:
        step, th = math.floor(3 * px + 0.5), math.floor(px + 0.5)
        ka = t['scanA'] * t['over'] * a
        y = math.ceil((y0 - px) / step) * step
        while y < yb:
            dw.rect(x0 - px, y, x1 + px, y + th, C(0, ka))
            y += step
    if t['grain']:
        rng = np.random.default_rng(int(now * 30) * 7919 + int(d.x))
        ka = t['grainA'] * t['over'] * a
        for _ in range(3):
            sx, sy = x0 + W * rng.random(), y0 + (yb - px - y0) * rng.random()
            dw.rect(sx, sy, sx + max(px, 1.0), sy + max(px, 1.0), C(t['grainRgb'], ka))


# ---------------------------------------------------------------- the backdrops through a model of each filter
def luma(a):
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def filtered(plain, look, fsk, figs, w, h):
    if look == 'stock':
        return plain
    L = luma(plain)
    rng = np.random.default_rng(359)
    if look == 'Night Vision':
        v = np.clip(L * 1.62 * min(fsk, 1.35) / max(min(fsk, 1.0), 1e-3), 0, 1)[..., None] * np.array([0.30, 1.0, 0.40]) + np.array([0, 0.035, 0])
        v += rng.normal(0, 0.06, (h, w, 1))
    elif look == 'Night Mono':
        v = np.repeat(np.clip(L * 1.27, 0, 1)[..., None], 3, -1) + rng.normal(0, 0.035, (h, w, 1))
    elif look == 'Thermal Vision':
        x = np.clip(L * 1.35, 0, 1)
        v = np.stack([np.clip((x - 0.92) * 1.2, 0, 1), np.clip((x - 0.30) * 1.4, 0, 1), np.clip((x - 0.05) * 1.6, 0, 1)], -1)
        cold = np.clip((0.35 - x) * 3.0, 0, 1)[..., None]
        v = v * (1 - cold) + np.array([0.04, 0.03, 0.22]) * cold
    else:  # Spirit Vision: edges on black
        gy, gx = np.gradient(L)
        e = np.clip((np.hypot(gx, gy) * 18.0 - 0.2), 0, 1)[..., None]
        v = e * np.array([0.30, 0.50, 0.62])
    m = Image.new('L', (w, h), 0)
    dd = ImageDraw.Draw(m)
    for kind, x, fy, ht in figs:
        wd = ht * 0.22
        dd.ellipse([x - wd * 0.28, fy - ht, x + wd * 0.28, fy - ht * 0.84], fill=255)
        dd.polygon([(x - wd * 0.55, fy - ht * 0.82), (x + wd * 0.55, fy - ht * 0.82), (x + wd * 0.30, fy), (x - wd * 0.30, fy)], fill=255)
    mb = np.asarray(m.filter(ImageFilter.GaussianBlur(3)), np.float32)[..., None] / 255.0
    if look == 'Thermal Vision':  # the monster hot, the ghost (second figure) hidden: drawn only for the first
        v = v * (1 - mb) + np.array([1.0, 0.62, 0.15]) * mb
    if look == 'Spirit Vision':
        v = v + mb * np.array([0.85, 0.88, 0.95]) * 0.8
    k = min(fsk, 1.0)
    return np.clip(plain * (1 - k) + v * k, 0, 1)


def cell(look, fsname, fsk, W=640, Hc=300, H=1080.0, now=0.6):
    plain = hm.backdrop(W, Hc).astype(np.float32)
    figs = [('human', W * 0.30, Hc * 0.92, Hc * 0.55)]
    img = filtered(plain, look, fsk, figs, W, Hc)
    r = hm.Raster(img)
    dw = hm.Draw(r)
    t = theme(look, fsk)
    bars = [B(W * 0.30, Hc * 0.28, 0.62, 0.78, kind=0, locked=True, heat_=0.6),
            B(W * 0.72, Hc * 0.28, 0.40, kind=1, guard=0.7, rev=0.55, rev_a=1.0),
            B(W * 0.50, Hc * 0.62, 0.85, kind=2, boss=True),
            B(W * 0.72, Hc * 0.80, 0.18, kind=0)]
    hidden = []
    for b in bars:
        if look == 'Thermal Vision' and b.kind == 1:
            hidden.append(b)  # HbThemeHides: a victim's bar is not drawn
            continue
        draw_bar(dw, b, H, t, now)
    im = r.img()
    dr = ImageDraw.Draw(im)
    try:
        f = ImageFont.truetype(hm.FONT, 15)
    except OSError:
        f = ImageFont.load_default()
    dr.text((8, 6), '%s, Filter Strength %s' % (look, fsname), fill=(255, 255, 255), font=f)
    for b in hidden:
        dr.text((b.x - 80, b.y - 14), '(victim: no bar)', fill=(200, 200, 255), font=f)
    return im, r.ntri


def main():
    os.makedirs(OUT, exist_ok=True)
    rows = []
    tris = 0
    for look in LOOKS:
        row = []
        for fsname, fsk in (FS if look != 'stock' else FS[2:3]):
            im, n = cell(look, fsname, fsk)
            tris += n
            row.append(im)
        rows.append(row)
    cw, ch = rows[0][0].size
    sheet = Image.new('RGB', (cw * 4, ch * len(rows)), (0, 0, 0))
    for j, row in enumerate(rows):
        for i, im in enumerate(row):
            sheet.paste(im, (i * cw, j * ch))
    p = os.path.join(OUT, 'ft359_bars_sheet.png')
    sheet.save(p)
    # a zoom of the High column, 3x
    zoom = Image.new('RGB', (cw, ch * (len(rows) - 1)))
    for j, row in enumerate(rows[1:]):
        zoom.paste(row[2], (0, j * ch))
    zoom = zoom.crop((cw * 0.12, 0, cw * 0.95, zoom.size[1]))
    zoom = zoom.resize((zoom.size[0] * 2, zoom.size[1] * 2), Image.NEAREST)
    pz = os.path.join(OUT, 'ft359_bars_high_zoom.png')
    zoom.save(pz)
    # Spirit Vision's flame tongues over time (4 moments)
    fr = [cell('Spirit Vision', 'High', 1.0, now=t)[0].crop((100, 40, 560, 140)) for t in (0.0, 0.25, 0.5, 0.75)]
    strip = Image.new('RGB', (fr[0].size[0], fr[0].size[1] * 4))
    for i, im in enumerate(fr):
        strip.paste(im, (0, i * fr[0].size[1]))
    strip = strip.resize((strip.size[0] * 2, strip.size[1] * 2), Image.NEAREST)
    ps = os.path.join(OUT, 'ft359_spirit_flames.png')
    strip.save(ps)
    print('wrote %s, %s, %s (%d triangles)' % (p, pz, ps, tris))


if __name__ == '__main__':
    main()
