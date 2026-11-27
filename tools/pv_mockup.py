"""v3.56 PV356: mock-ups of the Effects page's title-scene preview from src/preview_sim.h's own triangles.

    test\\build_pv_test.cmd --dump DIR      (writes DIR\\pv_scene.txt and DIR\\pv_atlas.raw)
    py -3.10 -I tools\\pv_mockup.py DIR [out_dir]

Nothing of the game: the scene is SH4 Refined's (preview_sim.h PvSceneBuild), the fire atlas spiritfire_sim.h's. Emulates
what the GPU draws: the vertex-coloured triangles (Gouraud), the blend of each list (room / figure / cabinet opaque, the
glow and the spirit glow ONE / ONE, the ghost and the heat list SRCALPHA / INVSRCALPHA, the fire premultiplied then
additive), Thermal's cold-world palette (screenfx.inc kPsThermal with THERM_*) and Spirit Vision's lift / edge / grade
passes (kPsGvLift / kPsGvEdge / kPsGvGrade with GV_*). Writes pv_normal.png, pv_thermal.png, pv_spirit.png and
pv_preview.png (the three side by side, half size).
"""
import math
import os
import sys

import numpy as np
from PIL import Image


def load(d):
    lists, fire, size = {}, None, None
    with open(os.path.join(d, 'pv_scene.txt')) as f:
        lines = f.read().split('\n')
    i = 0
    while i < len(lines):
        t = lines[i].split()
        i += 1
        if not t:
            continue
        if t[0] == 'size':
            size = (int(float(t[1])), int(float(t[2])))
        elif t[0] == 'list':
            n = int(t[2])
            v = []
            for k in range(n):
                x, y, c = lines[i + k].split()
                v.append((float(x), float(y), int(c, 16)))
            lists[t[1]] = v
            i += n
        elif t[0] == 'fire':
            n, pre = int(t[1]), int(t[2])
            v = []
            for k in range(n):
                x, y, u, vv, c = lines[i + k].split()
                v.append((float(x), float(y), float(u), float(vv), int(c, 16)))
            fire = (v, pre)
            i += n
    atlas = np.frombuffer(open(os.path.join(d, 'pv_atlas.raw'), 'rb').read(), dtype=np.uint8).reshape(512, 512) / 255.0
    return size, lists, fire, atlas


def argb(c):
    return np.array([(c >> 16) & 255, (c >> 8) & 255, c & 255, (c >> 24) & 255], dtype=np.float64) / 255.0


def raster(img, tri, mode, tex=None):
    """tri: 3 (x, y, rgba[, u, v]); pixel centres at integers (D3D9's -0.5 is in the vertices' place here)"""
    H, W, _ = img.shape
    xs = [p[0] for p in tri]
    ys = [p[1] for p in tri]
    x0, x1 = max(int(math.floor(min(xs))), 0), min(int(math.ceil(max(xs))), W - 1)
    y0, y1 = max(int(math.floor(min(ys))), 0), min(int(math.ceil(max(ys))), H - 1)
    if x0 > x1 or y0 > y1:
        return
    (ax, ay), (bx, by), (cx, cy) = [(p[0], p[1]) for p in tri]
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-9:
        return
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.0, np.arange(y0, y1 + 1) + 0.0)
    l0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
    l1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
    l2 = 1.0 - l0 - l1
    inside = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
    if not inside.any():
        return
    col = l0[..., None] * tri[0][2] + l1[..., None] * tri[1][2] + l2[..., None] * tri[2][2]
    if tex is not None:
        u = l0 * tri[0][3] + l1 * tri[1][3] + l2 * tri[2][3]
        v = l0 * tri[0][4] + l1 * tri[1][4] + l2 * tri[2][4]
        t = tex[np.clip((v * 512).astype(int), 0, 511), np.clip((u * 512).astype(int), 0, 511)]
        col = col * t[..., None]
    dst = img[y0:y1 + 1, x0:x1 + 1]
    m = inside[..., None]
    rgb, a = col[..., :3], col[..., 3:4]
    if mode == 'opaque':
        out = rgb
    elif mode == 'add':
        out = np.clip(dst + rgb, 0, 1)
    elif mode == 'alpha':
        out = rgb * a + dst * (1 - a)
    elif mode == 'premul':
        out = np.clip(rgb + dst * (1 - a), 0, 1)
    img[y0:y1 + 1, x0:x1 + 1] = np.where(m, out, dst)


def draw(img, verts, mode):
    for k in range(0, len(verts) - 2, 3):
        raster(img, [(v[0], v[1], argb(v[2])) for v in verts[k:k + 3]], mode)


def draw_fire(img, fire, atlas):
    v, pre = fire
    for k in range(0, len(v) - 2, 3):
        tri = [(p[0], p[1], argb(p[4]), p[2], p[3]) for p in v[k:k + 3]]
        raster(img, tri, 'premul' if k < pre else 'add', atlas)


def thermal(img):
    L = (img[..., 0] * 0.299 + img[..., 1] * 0.587 + img[..., 2] * 0.114) * 1.35
    r = np.clip((L - 0.92) * 1.2, 0, 1)
    g = np.clip((L - 0.30) * 1.4, 0, 1)
    b = np.clip((L - 0.05) * 1.6, 0, 1)
    cold = np.clip((0.35 - L) * 3.0, 0, 1)
    return np.stack([r + cold * 0.04, g + cold * 0.03, b + cold * 0.22], -1).clip(0, 1)


def shift(a, dx, dy):
    return np.roll(np.roll(a, int(round(dy)), 0), int(round(dx)), 1)


def spirit(img, h):
    u = h / 720.0
    x = img + 2 * 0.35 * (img - img * img)
    d = 1.5 * u
    dX = shift(x, -d, 0) - shift(x, d, 0)
    dY = shift(x, 0, -d) - shift(x, 0, d)
    e = np.clip(8 * (dX * dX + dY * dY).sum(-1, keepdims=True), 0, 1).repeat(3, -1)
    r = 1.5 * u
    blur = (shift(e, r, 0.5 * r) + shift(e, -r, 0.5 * r) + shift(e, 0, -r)) / 3
    g = np.clip(8 * (0.35 * blur + 0.65 * e - 0.20), 0, 1)
    return g * np.array([0.30, 0.50, 0.62])


def main():
    d = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'mockups')
    os.makedirs(out, exist_ok=True)
    (W, H), L, fire, atlas = load(d)

    def base(ghost):
        img = np.zeros((H, W, 3))
        draw(img, L['room'], 'opaque')
        draw(img, L['glow'], 'add')
        draw(img, L['fig'], 'opaque')
        if ghost:
            draw(img, L['ghost'], 'alpha')
        draw(img, L['front'], 'opaque')
        return img

    normal = base(True)
    th = thermal(base(False))
    draw(th, L['heat'], 'alpha')
    sv = spirit(base(True), H)
    draw(sv, L['spirit'], 'add')
    draw_fire(sv, fire, atlas)
    names = []
    for name, img in (('pv_normal.png', normal), ('pv_thermal.png', th), ('pv_spirit.png', sv)):
        p = os.path.join(out, name)
        Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)).save(p)
        names.append(p)
        print('wrote', p)
    ims = [Image.open(p).resize((W // 2, H // 2), Image.LANCZOS) for p in names]
    sheet = Image.new('RGB', (W // 2 * 3 + 20, H // 2), (40, 40, 40))
    for k, im in enumerate(ims):
        sheet.paste(im, (k * (W // 2 + 10), 0))
    p = os.path.join(out, 'pv_preview.png')
    sheet.save(p)
    print('wrote', p)


if __name__ == '__main__':
    main()
