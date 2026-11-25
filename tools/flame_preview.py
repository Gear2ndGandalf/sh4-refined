"""flame_preview.py - offline preview of Spirit Vision's spirit fires (v3.28 FIRE: src/spiritfire_sim.h, src/spiritfire.inc).

The fire logic is NOT ported: test\\sf_sim_test.exe --dump DIR runs src/spiritfire_sim.h itself (a 60 Hz run of each
scripted state) and writes the atlas (atlas.pgm) and the sprite lists of every 2nd frame; this tool only rasterises
them the way spiritfire.inc draws them: world -> screen (a pinhole camera here), flame quads anchored 30 % below / 70 %
above with the top edge sheared by lean, round sprites rotated, the colour = rgb x a x the pass strength clamped per
channel (GvColour), additive ONE / ONE, smoke premultiplied ONE / INVSRCALPHA, the same minimum sizes (720-line px).
The depth passes are approximated with the body's silhouette: the fire behind the body's front (inside the outline)
at SF_HIDDEN 0.8, outside it at 1.0. The backdrop is a stand-in Spirit Vision edge world (blue lines on black).

Usage:  py -3.10 tools/flame_preview.py DUMPDIR OUTDIR [--nogif] [--zoom]
Writes OUTDIR/fire_sheet.png (monster, person, player: idle, low health, a hit, going out; Walter idle / hit),
sheet_<kind>.png, and <kind>_<state>.gif (30 fps).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SF_FRAMES = 28
SF_HIDDEN = 0.80  # spiritfire.inc: the fire's hidden-pass strength (inside its own body)
LINE = np.array((0.30, 0.50, 0.62))
ZOOM = 2.0 if '--zoom' in sys.argv else 1.0  # --zoom: tiles x2 (a closer camera: 1440-line pixels)
TW, TH, SS = int(200 * ZOOM), int(280 * ZOOM), 2  # tile size, supersampling
UNIT = ZOOM                           # tile pixels per 720-line pixel (the tile is a crop of a 720-line screen)
OUT_W, OUT_H = TW * SS, TH * SS


def load_atlas(path):
    with open(path, 'rb') as f:
        data = f.read()
    parts = data.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return np.frombuffer(parts[3][:w * h], dtype=np.uint8).reshape(h, w).astype(np.float64) / 255.0


def cell_rect(cell):
    if cell < SF_FRAMES:
        return (cell & 7) * 64, (cell >> 3) * 128, 64, 128
    s = cell - SF_FRAMES
    c = SF_FRAMES + s // 2
    return (c & 7) * 64, (c >> 3) * 128 + (s & 1) * 64, 64, 64


def sample(atlas, cell, u, v):
    """bilinear, clamped to the cell, u / v 0..1 (half a texel inside, as SfCellUv)."""
    x0, y0, w, h = cell_rect(cell)
    fx = np.clip(u, 0, 1) * (w - 1)
    fy = np.clip(v, 0, 1) * (h - 1)
    ix, iy = np.floor(fx).astype(int), np.floor(fy).astype(int)
    ix1, iy1 = np.minimum(ix + 1, w - 1), np.minimum(iy + 1, h - 1)
    ax, ay = fx - ix, fy - iy
    t = atlas[y0:y0 + h, x0:x0 + w]
    a = t[iy, ix] * (1 - ax) + t[iy, ix1] * ax
    b = t[iy1, ix] * (1 - ax) + t[iy1, ix1] * ax
    return a * (1 - ay) + b * ay


class Cam:
    def __init__(self, target, dist, f):
        self.t = np.array(target, dtype=np.float64)
        self.d, self.f = dist, f

    def proj(self, p):
        z = p[2] - self.t[2] + self.d
        if z < 10.0:
            return None
        s = self.f / z
        return OUT_W * 0.5 + (p[0] - self.t[0]) * s, OUT_H * 0.56 + (p[1] - self.t[1]) * s, s


def body_outline(cam, size):
    """a stand-in figure around the chest at (0, -130, 0) (y down), scaled: head, shoulders, arms, torso, legs."""
    k = size / 1.0
    pts = [(-10, -245), (-12, -215), (-30, -205), (-52, -195), (-60, -150), (-66, -95), (-56, -92), (-46, -145), (-40, -100),
           (-34, -40), (-30, 0), (-12, 0), (-6, -60), (6, -60), (12, 0), (30, 0), (34, -40), (40, -100), (46, -145), (56, -92),
           (66, -95), (60, -150), (52, -195), (30, -205), (12, -215), (10, -245)]
    out = []
    for x, y in pts:
        pr = cam.proj((x * k, -130 + (y + 130) * k, 0.0))
        out.append((pr[0], pr[1]))
    return out


def backdrop(cam, size):
    img = Image.new('L', (OUT_W, OUT_H), 0)
    d = ImageDraw.Draw(img)
    for z in (300.0,):  # back wall / floor line, a door frame and a few scribbles
        pa, pb = cam.proj((-600, 0, z)), cam.proj((600, 0, z))
        d.line([(pa[0], pa[1]), (pb[0], pb[1])], fill=200, width=2 * SS)
    for x in (-150.0, 120.0):
        pa, pb = cam.proj((x, 0, 300.0)), cam.proj((x, -330, 300.0))
        d.line([(pa[0], pa[1]), (pb[0], pb[1])], fill=150, width=SS)
    pa, pb = cam.proj((-150, -330, 300.0)), cam.proj((120, -330, 300.0))
    d.line([(pa[0], pa[1]), (pb[0], pb[1])], fill=150, width=SS)
    out = body_outline(cam, size)
    d.line(out + [out[0]], fill=235, width=2 * SS)
    d.ellipse([out[0][0] - 2 * SS * ZOOM, out[0][1] - 26 * SS * size * ZOOM, out[-1][0] + 2 * SS * ZOOM, out[0][1] + 2 * SS * ZOOM], outline=235, width=2 * SS)
    m = np.asarray(img, dtype=np.float64) / 255.0
    halo = np.asarray(img.filter(ImageFilter.GaussianBlur(3 * SS)), dtype=np.float64) / 255.0
    v = np.clip(m * 0.9 + halo * 0.6, 0, 1)
    mask = Image.new('L', (OUT_W, OUT_H), 0)
    ImageDraw.Draw(mask).polygon(out, fill=255)
    return v[..., None] * LINE[None, None, :], np.asarray(mask, dtype=np.float64) / 255.0


def draw_sprite(img, mul, atlas, cam, sp):
    cell, add, mirror = int(sp[0]), int(sp[1]), int(sp[2])
    p = (sp[3], sp[4], sp[5])
    rx, ry, lean, rot = sp[6], sp[7], sp[8], sp[9]
    rgb = np.array(sp[10:13])
    a, minpx = sp[13], sp[14]
    pr = cam.proj(p)
    if pr is None:
        return
    x, y, s = pr
    rxp, ryp = rx * s, ry * s
    mn = minpx * UNIT * SS
    if cell < SF_FRAMES:
        if ryp < mn:
            rxp, ryp = rxp * mn / max(ryp, 1e-6), mn
        top, bot = y - 0.7 * ryp, y + 0.3 * ryp
        sh = lean * ryp
        x0 = int(max(min(x - rxp, x - rxp + sh) - 1, 0))
        x1 = int(min(max(x + rxp, x + rxp + sh) + 2, OUT_W))
        y0, y1 = int(max(top - 1, 0)), int(min(bot + 2, OUT_H))
        if x0 >= x1 or y0 >= y1:
            return
        Y, X = np.mgrid[y0:y1, x0:x1] + 0.5
        vl = (Y - top) / (bot - top)
        u = (X - (x - rxp + sh * (1 - vl))) / (2 * rxp)
        inside = (vl >= 0) & (vl <= 1) & (u >= 0) & (u <= 1)
        if mirror:
            u = 1 - u
        t = sample(atlas, cell, u, vl) * inside
    else:
        r = max(rxp, mn)
        x0, x1 = int(max(x - 1.415 * r - 1, 0)), int(min(x + 1.415 * r + 2, OUT_W))
        y0, y1 = int(max(y - 1.415 * r - 1, 0)), int(min(y + 1.415 * r + 2, OUT_H))
        if x0 >= x1 or y0 >= y1:
            return
        Y, X = np.mgrid[y0:y1, x0:x1] + 0.5
        dx, dy = (X - x) / r, (Y - y) / r
        cs, sn = np.cos(rot), np.sin(rot)
        lu, lv = dx * cs + dy * sn, -dx * sn + dy * cs  # the inverse rotation
        inside = (np.abs(lu) <= 1) & (np.abs(lv) <= 1)
        t = sample(atlas, cell, (lu + 1) * 0.5, (lv + 1) * 0.5) * inside
    m = mul[y0:y1, x0:x1]
    reg = img[y0:y1, x0:x1]
    if add:
        col = np.clip(rgb[None, None, :] * (a * m)[..., None], 0, 1)  # GvColour: the strength in the colour, clamped
        reg += t[..., None] * col
    else:
        al = np.clip(a * m, 0, 1)
        reg[:] = reg * (1 - (t * al)[..., None]) + (t * al)[..., None] * np.clip(rgb, 0, 1)[None, None, :]


def read_run(path):
    frames = []
    with open(path) as f:
        lines = f.read().split('\n')
    i = 0
    while i < len(lines):
        ln = lines[i].split()
        if not ln or ln[0] != 'F':
            i += 1
            continue
        t, n = float(ln[1]), int(ln[2])
        info = (float(ln[3]), float(ln[4]), int(ln[5]))
        sprs = [list(map(float, lines[i + 1 + k].split())) for k in range(n)]
        frames.append((t, info, sprs))
        i += 1 + n
    return frames


def render(atlas, cam, bg, mask, sprs):
    img = bg.copy()
    mul = np.where(mask > 0.5, SF_HIDDEN, 1.0)
    for sp in sprs:  # premultiplied smoke first, then the additive batch (spiritfire.inc's order)
        if int(sp[1]) == 0:
            draw_sprite(img, mul, atlas, cam, sp)
    for sp in sprs:
        if int(sp[1]) == 1:
            draw_sprite(img, mul, atlas, cam, sp)
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    return im.resize((TW, TH), Image.LANCZOS)


KINDS = [('monster', 'monster (orange)', 1.0), ('person', 'person / companion (white-gold)', 0.8),
         ('player', 'the player (white-gold)', 0.8), ('walter', 'Walter (blood red, x2.2)', 2.2)]
STATES = [('idle', 'idle', [1.0 + i / 15.0 for i in range(9)]),
          ('low', 'health 15 %', [1.0 + i / 15.0 for i in range(9)]),
          ('hit', 'a hit at 1.0 s (20 %; the player 10 %)', [0.9667, 1.0, 1.0333, 1.0667, 1.1333, 1.2, 1.3333, 1.6, 2.0]),
          ('die', 'dies at 0.5 s (D 10)', [0.5, 3.0, 5.5, 7.6, 8.4, 8.6, 8.73, 9.0, 9.7])]


def cam_for(size):
    return Cam((0.0, -150.0 - 20.0 * (size - 1.0), 0.0), 330.0 * max(size, 1.0) ** 0.85, 330.0 * SS * ZOOM)


def label(im, text, sub=None):
    d = ImageDraw.Draw(im)
    d.text((4, 3), text, fill=(220, 220, 220))
    if sub:
        d.text((4, TH - 14), sub, fill=(160, 160, 160))


def main():
    gifs = '--nogif' not in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dump, out = args[0], args[1]
    os.makedirs(out, exist_ok=True)
    atlas = load_atlas(os.path.join(dump, 'atlas.pgm'))
    Image.fromarray((atlas * 255).astype(np.uint8)).save(os.path.join(out, 'atlas.png'))
    rows = []
    for kname, ktext, size in KINDS:
        cam = cam_for(size)
        bg, mask = backdrop(cam, size)
        krows = []
        for sname, stext, times in STATES:
            if kname == 'walter' and sname in ('low', 'die'):
                continue
            run = read_run(os.path.join(dump, '%s_%s.txt' % (kname, sname)))
            tiles = []
            for T in times:
                t, info, sprs = min(run, key=lambda fr: abs(fr[0] - T))
                im = render(atlas, cam, bg, mask, sprs)
                sub = 't %.2f  hp %.2f  ag %.2f%s' % (t, info[0], info[1], '  dying' if info[2] else '')
                label(im, '%s: %s' % (kname, stext) if T == times[0] else '', sub)
                tiles.append(im)
            row = Image.new('RGB', (TW * len(tiles), TH))
            for i, im in enumerate(tiles):
                row.paste(im, (i * TW, 0))
            krows.append(row)
            if gifs:
                gif = [render(atlas, cam, bg, mask, fr[2]) for fr in run]
                gif[0].save(os.path.join(out, '%s_%s.gif' % (kname, sname)), save_all=True, append_images=gif[1:], duration=33, loop=0)
                print('rendered %s %s (%d gif frames)' % (kname, sname, len(gif)))
        sheet = Image.new('RGB', (krows[0].width, TH * len(krows)))
        for i, r in enumerate(krows):
            sheet.paste(r, (0, i * TH))
        sheet.save(os.path.join(out, 'sheet_%s.png' % kname))
        rows += krows
    full = Image.new('RGB', (max(r.width for r in rows), TH * len(rows)))
    for i, r in enumerate(rows):
        full.paste(r, (0, i * TH))
    full.save(os.path.join(out, 'fire_sheet.png'))
    print('wrote', os.path.join(out, 'fire_sheet.png'))


if __name__ == '__main__':
    main()
