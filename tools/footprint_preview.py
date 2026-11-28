r"""v3.59 TV359: Thermal Vision's footprints, a preview sheet (Henry's boots, Eileen's heels, the old oval).

The shapes are the C code's own triangles: test\thermal_sim_test.exe --dump <file> writes every point of TpShapeTris
(src\thermal_sim.h) and TpShapeSide; the print colour and life come from src\screenfx.inc (TP_HEAT_*, TP_LIFE_S, TP_K,
TP_STEP). Drawn as TpDraw draws them: each triangle's heat interpolated from its corners, x the print's fade
(1 - age / life)^1.5, added in TP_HEAT colours over the cold world's navy floor (THERM_COLD_*). Rows: a walk of Henry
(boots), of Eileen (heels), of a monster (the v3.17.1 oval), the oldest prints on the left; right: one pair x4 with the
walking line. Seen from straight above (the game's camera looks down at an angle).
usage: test\build_thermal_test.cmd, then
       test\out\thermal_sim_test.exe --dump shapes.txt
       py -3.10 tools\footprint_preview.py shapes.txt [out.png]   (needs numpy + PIL)
"""
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')


def load(path):
    tris, side = {}, {}
    pts = {}
    for line in open(path):
        v = line.split()
        if v[0] == 'side':
            for i in range(1, len(v), 2):
                side[int(v[i])] = float(v[i + 1])
            continue
        pts.setdefault(int(v[0]), []).append((float(v[1]), float(v[2]), float(v[3])))
    for sh, p in pts.items():
        tris[sh] = [p[i:i + 3] for i in range(0, len(p), 3)]
    return tris, side


def consts():
    txt = open(os.path.join(SRC, 'screenfx.inc'), encoding='utf-8', errors='replace').read()
    return {m.group(1): float(m.group(2)) for m in re.finditer(r'#define\s+((?:TP|THERM)_\w+)\s+([-0-9.]+)f?', txt)}


def raster(buf, tri, xf):
    P = [xf(a, b) for a, b, _ in tri]
    K = [k for _, _, k in tri]
    xs = [q[0] for q in P]
    ys = [q[1] for q in P]
    x0, x1 = max(0, int(min(xs))), min(buf.shape[1] - 1, int(max(xs)) + 1)
    y0, y1 = max(0, int(min(ys))), min(buf.shape[0] - 1, int(max(ys)) + 1)
    (ax, ay), (bx, by), (cx, cy) = P
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-9 or x1 < x0 or y1 < y0:
        return
    yy, xx = np.mgrid[y0:y1 + 1, x0:x1 + 1].astype(np.float32) + 0.5
    l0 = ((by - cy) * (xx - cx) + (cx - bx) * (yy - cy)) / den
    l1 = ((cy - ay) * (xx - cx) + (ax - cx) * (yy - cy)) / den
    l2 = 1 - l0 - l1
    inside = (l0 >= -1e-3) & (l1 >= -1e-3) & (l2 >= -1e-3)  # (a pixel on a shared edge belongs to one of them)
    v = np.where(inside, l0 * K[0] + l1 * K[1] + l2 * K[2], 0)
    # one print's triangles never overlap (the GPU draws a shared edge once): max, not sum, inside one print
    np.maximum(buf[y0:y1 + 1, x0:x1 + 1], v, out=buf[y0:y1 + 1, x0:x1 + 1])


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    tris, sides = load(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(HERE), 'footprint_preview.png')
    C = consts()
    heat = np.array([C['TP_HEAT_R'], C['TP_HEAT_G'], C['TP_HEAT_B']], np.float32)
    navy = np.array([C['THERM_COLD_R'], C['THERM_COLD_G'], C['THERM_COLD_B']], np.float32)
    life, step = C['TP_LIFE_S'], C['TP_STEP']
    W, H = 1500, 660
    img = np.ones((H, W, 3), np.float32) * navy
    rows = [(1, "Henry: boots (one sole: heel, outer arch, ball)"), (2, "Eileen: heels (narrow sole, heel dot)"),
            (0, "monsters: the v3.17.1 oval (unchanged)")]
    for r, (shape, name) in enumerate(rows):
        cy, sd = 120 + 200 * r, sides[shape]
        acc = np.zeros((H, W), np.float32)
        scale = 1.6  # px per world unit
        for i in range(10):  # every TP_STEP a print, alternating sides, the oldest on the left (walking +x)
            age = (9 - i) * 0.55
            k = C['TP_K'] * (1 - age / life) ** 1.5 if age < life else 0.0
            side = 1 if i % 2 == 0 else -1
            cx = 60 + i * step * scale
            one = np.zeros((H, W), np.float32)
            for t in tris[shape]:  # walking +x: out = side x (0, 1) -> up on screen for side +1
                raster(one, t, lambda a, b, cx=cx, side=side: (cx + a * scale, cy - (b + sd) * side * scale))
            acc += one * k
        for side in (1, -1):  # the close-up pair (x4)
            one = np.zeros((H, W), np.float32)
            for t in tris[shape]:
                raster(one, t, lambda a, b, side=side: (1180 + a * 4.0, cy - (b + sd) * side * 4.0))
            acc += one
        img += np.clip(acc, 0, 1)[..., None] * heat
    img = np.clip(img, 0, 1)
    pil = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    d = ImageDraw.Draw(pil)
    try:
        f = ImageFont.truetype('arial.ttf', 20)
    except OSError:
        f = ImageFont.load_default()
    for r, (shape, name) in enumerate(rows):
        cy = 120 + 200 * r
        d.text((20, cy - 92), name, fill=(235, 235, 235), font=f)
        d.line([(1060, cy), (1300, cy)], fill=(120, 120, 160), width=1)  # the walking line of the pair
    d.text((20, H - 30), 'Thermal Vision footprints (v3.59 TV359): walking right, oldest prints on the left; right: one pair x4',
           fill=(200, 200, 200), font=f)
    pil.save(out)
    print('wrote', out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
