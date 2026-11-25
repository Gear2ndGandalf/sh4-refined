"""v3.47 DF347: before / after renders of where the depth of field starts (v3.46's whole-level steps, v3.47's ramps of
quarter-level steps), made offline from src/dof_sim.h's tables (kDofRamp is read from the header; v3.46's are below).

    py -3.10 dof_ease_preview.py [path/to/dof_sim.h] [out_dir]

The test picture is generated here (no game art): a floor of tiles running from the camera to a far wall, striped posts at
0.45 .. 4.5 x the player's depth, the player's post in focus. The blur levels are drawn as dof.inc does: each level the
screen / 2^L (a box mean of 2 x 2 per step), read back with the 4-tap tent (+-0.5 texel, bilinear); v3.46 shows each code's
whole level, v3.47 lerp(level q / 4, level q / 4 + 1, q % 4 / 4) for a code of q quarters (the composite's blend).
Writes dof_ease_Low.png, dof_ease_Medium.png, dof_ease_High.png (v3.46 above, v3.47 below, each with a 2x zoom of where the
far blur starts), dof_ease_profile.png (the blur level against the distance, both versions, the three strengths) and
dof_ease_card.png (the Screen Effects page's sample depth, Medium, both versions).
"""
import math
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'src', 'dof_sim.h')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '..', 'renders')
W, H = 512, 288
STR = ('Low', 'Medium', 'High')

# ---- v3.46's tables (dof_sim.h before v3.47): far ratios, near ratios, a whole level per code
V346_FAR = (1.25, 1.6, 2.2, 3.2, 5.0)
V346_NEAR = (0.7, 0.5)
V346_LV = ((0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1), (0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 1), (0, 1, 2, 3, 3, 3, 0, 0, 0, 1, 2))
V346_CARD = ((0.12, 0.22, 0.32, 0.42, 0.52, 0.78, 0.90), (5, 4, 3, 2, 1, 0, 9, 10))


def read_ramps(path):
    """kDofRamp[strength][side] = (n, grid steps) from the header: {Low far, Low near}, {Medium ...}, {High ...}"""
    text = open(path, encoding='utf-8').read()
    body = text[text.index('kDofRamp[DOFS_N][DOF_SIDES] = {'):]
    body = body[:body.index('};')]
    sides = re.findall(r'\{\s*(\d+)\s*,\s*\{([-\d,\s]+)\}\s*\}', body)
    assert len(sides) == 6, sides
    out = []
    for k in range(3):
        pair = []
        for n, g in sides[2 * k:2 * k + 2]:
            vals = [int(v) for v in g.split(',') if v.strip()]
            assert len(vals) == int(n)
            pair.append(vals)
        out.append(pair)
    grid = int(re.search(r'#define DOF_GRID (\d+)', text).group(1))
    return out, grid


RAMPS, GRID = read_ramps(SIM)


def blur_new(s, r):
    """v3.47: quarters of blur at w / F = r (array)"""
    far, near = RAMPS[s]
    q = np.zeros_like(r)
    for g in far:
        q += r > 2.0 ** (g / GRID)
    for g in near:
        q += r < 2.0 ** (g / GRID)
    return q / 4.0


def blur_old(s, r):
    code = np.zeros(r.shape, np.int32)
    for k, t in enumerate(V346_FAR):
        code = np.where(r > t, k + 1, code)
    for k, t in enumerate(V346_NEAR):
        code = np.where(r < t, 9 + k, code)
    return np.array(V346_LV[s], np.float32)[code]


# ---- the test picture: colour and view depth (a camera above and behind the player looking down, as SH4's)
CAM_H, FOC, YH, FDEPTH = 420.0, 320.0, -0.12 * H, 800.0


def row_of(r):
    """the screen row where the floor lies at r x the player's depth"""
    return YH + CAM_H * FOC / (r * FDEPTH)


def scene():
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32) + 0.5
    depth = CAM_H * FOC / (ys - YH)
    # floor tiles 50 units (world x, z) with dark grout, a little colour per tile
    X = (xs - W / 2) * depth / FOC
    tx, tz = np.floor(X / 50.0), np.floor(depth / 50.0)
    tiles = ((tx + tz) % 2).astype(np.float32)
    tint = 0.85 + 0.15 * np.sin(tx * 1.7 + tz * 2.3)
    grout = (np.abs((X / 50.0) % 1.0 - 0.5) > 0.44) | (np.abs((depth / 50.0) % 1.0 - 0.5) > 0.44)
    img = (0.28 + 0.42 * tiles[..., None] * np.array([1.0, 0.93, 0.82])) * tint[..., None]
    img = np.where(grout[..., None], 0.10, img)
    # posts: (w / F, x on screen as a share, colour), drawn far to near; horizontal stripes for detail
    posts = [(4.5, 0.12, (0.3, 0.6, 0.9)), (3.2, 0.82, (0.9, 0.8, 0.3)), (2.2, 0.30, (0.5, 0.9, 0.5)), (1.6, 0.68, (0.9, 0.5, 0.8)),
             (1.3, 0.38, (0.6, 0.7, 1.0)), (1.0, 0.53, (1.0, 1.0, 1.0)), (0.7, 0.20, (1.0, 0.6, 0.4)), (0.5, 0.86, (0.7, 1.0, 0.9))]
    for r, xs_, col in posts:
        w = r * FDEPTH
        yb = row_of(r)
        hp = 170.0 * FOC / w
        wp = 30.0 * FOC / w
        x0, x1 = xs_ * W - wp / 2, xs_ * W + wp / 2
        m = (xs >= x0) & (xs < x1) & (ys >= yb - hp) & (ys < yb) & (depth > w)
        hh = (yb - ys) * w / FOC
        stripe = (((hh / 14.0) % 1.0) < 0.5).astype(np.float32)
        c = np.array(col)[None, None, :] * (0.40 + 0.60 * stripe[..., None])
        img = np.where(m[..., None], c, img)
        depth = np.where(m, w, depth)
    return np.clip(img, 0.0, 1.0), depth, FDEPTH


def box_down(a):
    h, w = a.shape[0] // 2, a.shape[1] // 2
    return a[:2 * h, :2 * w].reshape(h, 2, w, 2, -1).mean(axis=(1, 3))


def bilinear(a, H_, W_):
    """sample a (h, w, c) at the centres of an H_ x W_ grid over the same area (D3D: texel k at (k + 0.5) / n, CLAMP)"""
    h, w = a.shape[:2]
    y = (np.arange(H_) + 0.5) * h / H_ - 0.5
    x = (np.arange(W_) + 0.5) * w / W_ - 0.5
    y0 = np.clip(np.floor(y).astype(int), 0, h - 1)
    x0 = np.clip(np.floor(x).astype(int), 0, w - 1)
    y1, x1 = np.clip(y0 + 1, 0, h - 1), np.clip(x0 + 1, 0, w - 1)
    fy = np.clip(y - y0, 0, 1)[:, None, None]
    fx = np.clip(x - x0, 0, 1)[None, :, None]
    top = a[y0][:, x0] * (1 - fx) + a[y0][:, x1] * fx
    bot = a[y1][:, x0] * (1 - fx) + a[y1][:, x1] * fx
    return top * (1 - fy) + bot * fy


def levels(img):
    """level 0 = the scene; 1..3 = half / quarter / eighth (box 2 x 2 per step, as the 4 x 4 box of the 4-tap mean at +-1
    texel roughly), read back over the screen with the tent (bilinear at +-0.5 texel of the level ~ one more 2 x 2 mean)"""
    out = [img]
    a = img
    for L in range(1, 4):
        a = box_down(a)
        t = np.pad(a, ((1, 1), (1, 1), (0, 0)), mode='edge')
        tent = 0.25 * (t[:-2, :-2] + t[:-2, 1:-1] + t[1:-1, :-2] + t[1:-1, 1:-1]) * 0.5 + 0.5 * a
        out.append(bilinear(tent, H, W))
    return out


def compose(lv, b):
    lo = np.clip(np.floor(b + 1e-6), 0, 3).astype(int)
    fr = np.clip(b - lo, 0, 1)
    hi = np.clip(lo + 1, 0, 3)
    stack = np.stack(lv, 0)
    yy, xx = np.mgrid[0:H, 0:W]
    a = stack[lo, yy, xx]
    c = stack[hi, yy, xx]
    return a * (1 - fr[..., None]) + c * fr[..., None]


def to_img(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255.0 + 0.5).astype(np.uint8))


def label(im, text, xy=(6, 4)):
    d = ImageDraw.Draw(im)
    x, y = xy
    d.rectangle((x - 3, y - 2, x + 7 * len(text) + 3, y + 12), fill=(0, 0, 0))
    d.text((x, y), text, fill=(255, 255, 255))


def main():
    os.makedirs(OUT, exist_ok=True)
    img, depth, F = scene()
    lv = levels(img)
    r = depth / F
    zw, zh = 128, 72
    for s, name in enumerate(STR):
        # the zoom: where v3.46's far blur started (its first blurred code), on the floor left of the middle
        r0 = next(V346_FAR[k] for k in range(5) if V346_LV[s][k + 1])
        zx, zy = 150, int(max(0, min(H - zh, row_of(r0) - zh / 2)))
        old = compose(lv, blur_old(s, r))
        new = compose(lv, blur_new(s, r))
        sheet = Image.new('RGB', (W + 2 * zw + 6, 2 * H + 6), (40, 40, 40))
        for k, (pic, tag) in enumerate(((old, 'v3.46 %s: hard start' % name), (new, 'v3.47 %s: eased in' % name))):
            im = to_img(pic)
            zoom = im.crop((zx, zy, zx + zw, zy + zh)).resize((2 * zw, 2 * zh), Image.NEAREST)
            d = ImageDraw.Draw(im)
            d.rectangle((zx, zy, zx + zw - 1, zy + zh - 1), outline=(255, 64, 64))
            label(im, tag)
            sheet.paste(im, (0, k * (H + 6)))
            sheet.paste(zoom, (W + 6, k * (H + 6) + (H - 2 * zh) // 2))
        sheet.save(os.path.join(OUT, 'dof_ease_%s.png' % name), optimize=True)
    # the profile: blur level against log2(w / F)
    PW, PH, ml, mb = 720, 340, 46, 34
    prof = Image.new('RGB', (PW, PH), (24, 24, 28))
    d = ImageDraw.Draw(prof)
    x0, x1 = -1.6, 2.4
    gx = lambda o: ml + (o - x0) / (x1 - x0) * (PW - ml - 10)
    gy = lambda b: PH - mb - b / 3.2 * (PH - mb - 40)
    for b in range(4):
        d.line((ml, gy(b), PW - 10, gy(b)), fill=(60, 60, 66))
        d.text((10, gy(b) - 6), 'L%d' % b, fill=(200, 200, 200))
    for o, t in ((-1.0, '0.5 F'), (0.0, 'F (player)'), (1.0, '2 F'), (2.0, '4 F')):
        d.line((gx(o), 26, gx(o), PH - mb), fill=(60, 60, 66))
        d.text((gx(o) - 14, PH - mb + 6), t, fill=(200, 200, 200))
    cols = ((120, 200, 255), (255, 200, 90), (255, 110, 110))
    o = np.linspace(x0, x1, 1600)
    rr = 2.0 ** o
    for s in range(3):
        for which, fn in ((0, blur_old), (1, blur_new)):
            b = fn(s, rr)
            pts = [(gx(o[i]), gy(b[i]) - (s - 1) * 2) for i in range(len(o))]
            if which:
                d.line(pts, fill=cols[s], width=2)
            else:
                for i in range(0, len(pts) - 1, 8):
                    d.line((pts[i], pts[min(i + 4, len(pts) - 1)]), fill=tuple(int(c * 0.7) for c in cols[s]), width=2)
    d.text((ml, 6), 'blur level by distance from the player: v3.46 dashed (hard steps), v3.47 solid (quarter steps)',
           fill=(230, 230, 230))
    for s in range(3):
        d.rectangle((ml + 80 * s, 20, ml + 8 + 80 * s, 28), fill=cols[s])
        d.text((ml + 12 + 80 * s, 18), STR[s], fill=(230, 230, 230))
    prof.save(os.path.join(OUT, 'dof_ease_profile.png'), optimize=True)
    # the page's card (preview.dds stand-in: the test picture's colours over a sample depth that only follows the height)
    yv = (np.arange(H) + 0.5) / H
    kY, kG = (0.0, 0.52, 0.70, 1.0), (52.0, 2.0, -3.0, -34.0)
    gcard = np.interp(yv, kY, kG)
    r_new = np.repeat((2.0 ** (gcard / GRID))[:, None], W, 1)
    t, c = V346_CARD
    code = np.array([next((c[i] for i in range(7) if y < t[i]), c[7]) for y in ((np.floor(yv * 64) + 0.5) / 64)])
    lvl_old = np.array(V346_LV[1], np.float32)[code]
    old = compose(lv, np.repeat(lvl_old[:, None], W, 1))
    new = compose(lv, blur_new(1, r_new))
    card = Image.new('RGB', (2 * W + 6, H), (40, 40, 40))
    for k, (pic, tag) in enumerate(((old, 'page card v3.46 Medium'), (new, 'page card v3.47 Medium'))):
        im = to_img(pic)
        label(im, tag)
        card.paste(im, (k * (W + 6), 0))
    card.save(os.path.join(OUT, 'dof_ease_card.png'), optimize=True)
    for fn in sorted(os.listdir(OUT)):
        if fn.startswith('dof_ease_'):
            print('%s %d KB' % (fn, os.path.getsize(os.path.join(OUT, fn)) // 1024))


if __name__ == '__main__':
    main()
