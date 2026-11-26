"""v3.43 PP343: mock-ups of Bloom, Colour Grading and the Otherworld Lens from the shader math of src/post_sim.h.

    py -3.10 post_mockup.py [out_dir]

The test picture is generated here (no game art): a dark room with a shaded table lamp, a cold window, three candles, a
lit hole in the wall, a figure, film-like noise, and a strip of grey / colour ramps along the bottom (the grading curves).
Emulates what the GPU draws (D3D9 sampling: pixel centres at integers, texel k at (k + 0.5) / n, bilinear, CLAMP; every
target 8 bits a channel): the bloom chain of PpMakePlan (bright pass + down in 2x steps, two Kawase passes at 1/16, up in
2x steps blended SRCALPHA with the weight `up`, the screen composite from the half size), the grading shader per pixel
(PpGradePixel) and the lens (PpLensK / PpLensUv per channel, exact instead of the 16 x 10 mesh). The constants below are
post_sim.h's (keep them in step).
Writes: post_bloom.png, post_grading.png, post_lens.png (with a 4x corner zoom), post_combined.png.
v3.52 FX352: Bloom Off / Low / Medium / High / Ultra (post_sim.h kPpBloom; the composite's k may exceed 1); also writes
post_bloom_v352.png (v3.51's three levels against v3.52's four, the test room and a darker copy of it, with the glow
added as a number) and post_lens_v352.png (the lens at v3.51's base 40 %, v3.52's 70 % and full, corner zooms).
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'mockups')
W, H = 1280, 720

# ---- post_sim.h
# th, gain, k, up. v3.52 FX352: five levels (post_sim.h kPpBloom); BLOOM_V351: v3.44 .. v3.51's, for the comparison
BLOOM = {'Low': (0.46, 3.2, 0.95, 0.86), 'Medium': (0.38, 3.6, 1.00, 0.89), 'High': (0.32, 4.0, 1.20, 0.92),
         'Ultra': (0.26, 4.0, 1.60, 0.94)}
BLOOM_V351 = {'v3.51 Low': (0.62, 3.0, 0.90, 0.80), 'v3.51 High': (0.52, 2.6, 1.00, 0.85), 'v3.51 Very High': (0.41, 3.5, 1.00, 0.90)}
BLOOM.update(BLOOM_V351)
LEVELS, RT_MIN, KAWASE_A, KAWASE_B, UP_TAP, COMP_TAP = 4, 8, 1.5, 2.5, 0.75, 0.5
GRADE = {  # sat, gain, off, sh, hi
    'PS2': (0.85, (0.92, 0.97, 0.90), (0.030, 0.042, 0.030), (0.000, 0.012, 0.002), (-0.015, 0.008, -0.025)),
    'Otherworld': (0.55, (0.92, 0.98, 1.05), (0.000, 0.004, 0.014), (-0.008, 0.000, 0.022), (-0.030, 0.000, 0.025)),
    'Apartment': (1.06, (1.08, 1.00, 0.86), (0.010, 0.004, 0.000), (0.014, 0.004, -0.008), (0.050, 0.018, -0.035)),
    'Bleach': (0.55, (1.22, 1.22, 1.20), (-0.050, -0.050, -0.045), (0.000, 0.000, 0.000), (0.000, 0.000, 0.000)),
}
LUMA = np.array([0.299, 0.587, 0.114], np.float32)
LENS_WARP, LENS_FRINGE, LENS_BREATHE = 0.040, 0.0065, 0.30


def q8(a):
    return np.round(np.clip(a, 0.0, 1.0) * 255.0) / 255.0


# ---------------------------------------------------------------- the test picture
def scene():
    rng = np.random.default_rng(43)
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    img = np.zeros((H, W, 3), np.float32)
    wall = 0.10 + 0.05 * (1.0 - y / H)
    img[...] = np.stack([wall * 0.85, wall * 0.95, wall * 0.80], -1)
    floor = y > 0.68 * H
    img[floor] = (np.stack([0.07 + 0.08 * (y / H - 0.68), 0.06 + 0.06 * (y / H - 0.68), 0.05 + 0.04 * (y / H - 0.68)], -1))[floor]
    for k in range(9):  # wallpaper stripes
        xs = 60 + k * 130
        img[(x > xs) & (x < xs + 8) & ~floor] *= 0.75
    # the window (cold light) with its frame
    win = (x > 860) & (x < 1080) & (y > 90) & (y < 360)
    img[win] = [0.62, 0.74, 0.86]
    img[win & (np.abs(x - 970) < 5)] = [0.08, 0.08, 0.08]
    img[win & (np.abs(y - 225) < 5)] = [0.08, 0.08, 0.08]
    sky = win & (y < 160)
    img[sky] = [0.86, 0.92, 0.98]
    # the table and the lamp (a warm bulb under a shade)
    table = (x > 180) & (x < 470) & (y > 410) & (y < 430)
    img[table] = [0.16, 0.10, 0.06]
    img[(x > 210) & (x < 225) & (y > 430) & (y < 530)] = [0.12, 0.08, 0.05]
    img[(x > 425) & (x < 440) & (y > 430) & (y < 530)] = [0.12, 0.08, 0.05]
    img[(x > 300) & (x < 312) & (y > 330) & (y < 410)] = [0.25, 0.20, 0.10]
    shade = (y > 270) & (y < 335) & (np.abs(x - 306) < 30 + (y - 270) * 0.55)
    img[shade] = [0.95, 0.78, 0.48]
    bulb = (x - 306) ** 2 + (y - 345) ** 2 < 14 ** 2
    img[bulb] = [1.0, 0.96, 0.80]
    # candles on the floor (small flames)
    for cx in (560, 600, 645):
        img[(np.abs(x - cx) < 5) & (y > 560) & (y < 600)] = [0.80, 0.76, 0.66]
        fl = ((x - cx) / 4.0) ** 2 + ((y - 550) / 9.0) ** 2 < 1.0
        img[fl] = [1.0, 0.70, 0.25]
        img[((x - cx) / 2.0) ** 2 + ((y - 552) / 4.0) ** 2 < 1.0] = [1.0, 0.95, 0.75]
    # the hole in the wall (Room 302's light through it)
    r = np.hypot(x - 700, y - 250)
    img[r < 34] = [0.0, 0.0, 0.0]
    img[(r < 20)] = [0.96, 0.92, 0.80]
    img[(r >= 34) & (r < 40)] = img[(r >= 34) & (r < 40)] * 0.5
    # a figure (dark silhouette, a pale face)
    body = (((x - 760) / 55.0) ** 2 + ((y - 470) / 150.0) ** 2) < 1.0
    img[body] = [0.05, 0.05, 0.06]
    head = (x - 760) ** 2 + (y - 300) ** 2 < 28 ** 2
    img[head] = [0.42, 0.34, 0.30]
    # the ramps strip
    sy = y > H - 70
    t = x / (W - 1)
    img[sy & (y < H - 52)] = np.stack([t, t, t], -1)[sy & (y < H - 52)]
    cols = [(1, 0.15, 0.1), (0.15, 0.8, 0.2), (0.15, 0.3, 1.0), (0.9, 0.75, 0.55)]
    for i, c in enumerate(cols):
        band = sy & (y >= H - 52 + 13 * i) & (y < H - 52 + 13 * (i + 1))
        img[band] = (t[..., None] * np.array(c, np.float32))[band]
    img += (rng.standard_normal((H, W, 1)).astype(np.float32) * 0.012)
    return q8(img)


# ---------------------------------------------------------------- sampling
def sample(img, u, v):
    """bilinear, CLAMP; u, v arrays of texture coordinates"""
    h, w = img.shape[:2]
    fx = u * w - 0.5
    fy = v * h - 0.5
    x0 = np.floor(fx).astype(np.int32)
    y0 = np.floor(fy).astype(np.int32)
    ax = (fx - x0)[..., None]
    ay = (fy - y0)[..., None]
    x0c, x1c = np.clip(x0, 0, w - 1), np.clip(x0 + 1, 0, w - 1)
    y0c, y1c = np.clip(y0, 0, h - 1), np.clip(y0 + 1, 0, h - 1)
    a = img[y0c, x0c] * (1 - ax) + img[y0c, x1c] * ax
    b = img[y1c, x0c] * (1 - ax) + img[y1c, x1c] * ax
    return a * (1 - ay) + b * ay


def mean4(src, dw, dh, du, dv):
    y, x = np.mgrid[0:dh, 0:dw].astype(np.float32)
    u, v = (x + 0.5) / dw, (y + 0.5) / dh
    acc = 0
    for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        acc = acc + sample(src, u + sx * du, v + sy * dv)
    return acc * 0.25


def half(n):
    return (n + 1) // 2 if n // 2 > RT_MIN else RT_MIN


def bloom(img, look, base=None):
    th, gain, k, up = BLOOM[look]
    ws, hs = [W], [H]
    for _ in range(LEVELS):
        ws.append(half(ws[-1]))
        hs.append(half(hs[-1]))
    lv = [img]
    for i in range(1, LEVELS + 1):  # bright + down
        m = mean4(lv[-1], ws[i], hs[i], 1.0 / ws[i - 1], 1.0 / hs[i - 1])
        if i == 1:
            v2 = (m - th) * (gain * 0.25) * 2.0
            m = np.clip(v2 + v2, 0, 1)
        lv.append(q8(m))
    L = LEVELS
    x = q8(mean4(lv[L], ws[L], hs[L], KAWASE_A / ws[L], KAWASE_A / hs[L]))
    lv[L] = q8(mean4(x, ws[L], hs[L], KAWASE_B / ws[L], KAWASE_B / hs[L]))
    for i in range(L - 1, 0, -1):  # up: SRCALPHA blend with alpha = up
        t = mean4(lv[i + 1], ws[i], hs[i], UP_TAP / ws[i + 1], UP_TAP / hs[i + 1])
        lv[i] = q8(lv[i] * (1 - up) + t * up)
    b = mean4(lv[1], W, H, COMP_TAP / ws[1], COMP_TAP / hs[1]) * (k * 0.5) * 2.0  # v3.52 FX352: c1 = k / 2, mul_x2
    b = q8(b)
    base = img if base is None else base
    return q8(base + b - base * b), lv[1]


def grade(img, name):
    s, g, o, sh, hi = GRADE[name]
    L = (img * LUMA).sum(-1, keepdims=True)
    f = 1.0 - s
    r = f * L + (1 - f) * img
    r = r * (np.array(g) * 0.5) * 2.0
    r = r + np.array(o)
    r = (1 - L) * np.array(sh) + r
    return q8(r + L * L * np.array(hi))


def lens(img, s, t=0.0):
    a = W / H
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    u, v = (x + 0.5) / W, (y + 0.5) / H
    out = np.zeros_like(img)
    br = 1.0 + LENS_BREATHE * math.sin(2 * math.pi * t / 6.0)
    for ch in range(3):
        k = s * (LENS_WARP * br + (1 - ch) * LENS_FRINGE)
        dx, dy = u - 0.5, v - 0.5
        r2 = (dx * dx * a * a + dy * dy) / (0.25 * a * a + 0.25)
        sc = np.clip(1.0 - k * r2, 0.5, 1.0)
        out[..., ch] = sample(img, 0.5 + dx * sc, 0.5 + dy * sc)[..., ch]
    return q8(out)


# ---------------------------------------------------------------- sheets
def to_im(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))


def sheet(tiles, cols, scale, title):
    tw, th = int(W * scale), int(H * scale)
    rows = (len(tiles) + cols - 1) // cols
    im = Image.new('RGB', (cols * tw + (cols + 1) * 8, rows * (th + 22) + 8 + 26), (24, 24, 24))
    d = ImageDraw.Draw(im)
    d.text((8, 6), title, fill=(230, 230, 230))
    for i, (name, a) in enumerate(tiles):
        cx, cy = 8 + (i % cols) * (tw + 8), 30 + (i // cols) * (th + 22)
        im.paste(to_im(a).resize((tw, th), Image.LANCZOS), (cx, cy))
        d.text((cx, cy + th + 4), name, fill=(220, 220, 220))
    return im


def main():
    os.makedirs(OUT, exist_ok=True)
    img = scene()
    lo, lo1 = bloom(img, 'Low')
    hi, hi1 = bloom(img, 'High')
    sheet([('Bloom Off (the game)', img), ('Bloom Low', lo), ('Bloom High', hi), ('High: the half-size glow (B, x1)', hi1)],
          2, 0.5, 'SH4 Refined v3.52 Bloom (generated test room, 1280x720; the chain 640x360 .. 80x45)').save(
        os.path.join(OUT, 'post_bloom.png'))
    v352(img)  # v3.52 FX352
    tiles = [('Colour Grading Off', img)] + [(n, grade(img, n)) for n in ('PS2', 'Otherworld', 'Apartment', 'Bleach')]
    sheet(tiles, 3, 0.5, 'SH4 Refined v3.43 Colour Grading (per-channel curves, the ps.1.1 shader math)').save(
        os.path.join(OUT, 'post_grading.png'))
    ln = lens(img, 1.0)
    z0 = img[0:180, 960:1280]
    z1 = ln[0:180, 960:1280]
    zoom = [('corner, Lens Off (4x)', np.kron(z0, np.ones((4, 4, 1)))[:H, :W]),
            ('corner, Lens On (4x)', np.kron(z1, np.ones((4, 4, 1)))[:H, :W])]
    sheet([('Otherworld Lens Off', img), ('Otherworld Lens On (full strength, near a ghost)', ln)] + zoom, 2, 0.5,
          'SH4 Refined v3.43 Otherworld Lens (edge warp + chromatic fringe; 40 % while a haunting is registered, full near it)').save(
        os.path.join(OUT, 'post_lens.png'))
    c1 = bloom(img, 'Low', grade(img, 'Apartment'))[0]  # as drawn: the bloom of the frame, over the graded frame
    c2 = lens(bloom(img, 'High', grade(img, 'Otherworld'))[0], 0.7)
    c3 = bloom(img, 'Low', grade(img, 'PS2'))[0]
    sheet([('the game', img), ('Apartment + Bloom Low', c1), ('PS2 + Bloom Low', c3), ('Otherworld + Bloom High + Lens 70 %', c2)],
          2, 0.5, 'SH4 Refined v3.43 combinations (as drawn: the bloom of the frame laid over the graded frame)').save(
        os.path.join(OUT, 'post_combined.png'))
    for n in ('post_bloom.png', 'post_grading.png', 'post_lens.png', 'post_combined.png', 'post_bloom_v352.png', 'post_lens_v352.png'):
        print(os.path.join(OUT, n))


# ---------------------------------------------------------------- v3.52 FX352: the comparisons for the merge
def dark_room(img):
    """the test room as dark as SH4's rooms: everything below mid grey at 45 %, the lights at 85 %"""
    L = (img * LUMA).sum(-1, keepdims=True)
    d = np.where(L < 0.55, img * 0.45, img * 0.85)
    d[H - 70:] = img[H - 70:] * 0.45
    return q8(d)


def v352(img):
    dk = dark_room(img)
    tiles, rows = [], []
    for name in ('v3.51 Low', 'v3.51 High', 'v3.51 Very High', 'Low', 'Medium', 'High', 'Ultra'):
        a, _ = bloom(img, name)
        b, _ = bloom(dk, name)
        ga, gb = float((a - img).mean()), float((b - dk).mean())
        rows.append((name, ga, gb))
        label = name if name.startswith('v3.51') else 'v3.52 ' + name
        tiles.append(('%s: glow %.3f (dark room %.3f)' % (label, ga, gb), b))
    tiles.insert(0, ('the dark room, Bloom Off', dk))
    sheet(tiles, 4, 0.36, 'SH4 Refined v3.52 Bloom levels against v3.51 (the dark test room; glow = the mean added, test room / dark room). '
          'Migration: v3.51 High -> Medium, Very High -> High').save(os.path.join(OUT, 'post_bloom_v352.png'))
    for name, ga, gb in rows:
        print('bloom %-16s glow %.4f (dark room %.4f)' % (name, ga, gb))
    l40, l70, l100 = lens(dk, 0.40, 1.5), lens(dk, 0.70, 1.5), lens(dk, 1.0, 1.5)

    def corner(a):
        return np.kron(a[0:180, 960:1280], np.ones((4, 4, 1)))[:H, :W]
    sheet([('Lens Off', dk), ('v3.51: 40 % (a haunting in the room)', l40), ('v3.52: 70 % (the new base)', l70),
           ('full (halfway into an aura; the preview)', l100), ('corner, Off (4x)', corner(dk)), ('corner, 40 % (4x)', corner(l40)),
           ('corner, 70 % (4x)', corner(l70)), ('corner, full (4x)', corner(l100))], 4, 0.36,
          'SH4 Refined v3.52 Otherworld Lens: the base while a ghost or haunting is in the room, 40 % -> 70 % (the dark test room)').save(
        os.path.join(OUT, 'post_lens_v352.png'))


if __name__ == '__main__':
    main()
