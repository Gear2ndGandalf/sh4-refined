"""2D mock-up of the 'blood-rooted' Spirit Vision ghost (v326 sv_rain).
Paints a ramped body, crimson aura and blood at the tendril roots over a real screenshot.
Run: py -3.10 ghost_mockup.py
"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

SRC = sys.argv[1] if len(sys.argv) > 1 else "shot_spirit_vision_ghost_aura.webp"  # a screenshot of your own
OUT = sys.argv[2] if len(sys.argv) > 2 else "renders"
CROP = (380, 240, 820, 760)
SEED = 326

# thresholds
X_MAX = 700            # menu text lives right of this
L_MIN = 0.50           # ghost: luminance above
S_MAX = 0.40           # and HSV saturation below
MIN_AREA = 25         # keep connected components bigger than this
ROOT_BAND = 6          # px from mask edge for tendril roots
PALE = np.array([1.00, 0.86, 0.84])
DEEP = np.array([0.42, 0.02, 0.06])
AURA_COL = np.array([0.30, 0.012, 0.035]) * 1.6


def blur(a, s):
    return ndi.gaussian_filter(a, s, mode="constant")


def smooth(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def detect(img):
    mx, mn = img.max(2), img.min(2)
    L = img @ np.array([0.299, 0.587, 0.114])
    sat = (mx - mn) / np.maximum(mx, 1e-4)
    m = (L > L_MIN) & (sat < S_MAX)
    m[:, X_MAX:] = False
    m[:200] = False
    m[800:] = False
    m[:, :400] = False
    m = ndi.binary_opening(m, iterations=1)
    lab, n = ndi.label(m)
    if n:
        areas = ndi.sum(m, lab, range(1, n + 1))
        big = int(np.argmax(areas)) + 1
        near = ndi.binary_dilation(lab == big, iterations=70)
        keep = [i + 1 for i, a in enumerate(areas) if a >= MIN_AREA and (near & (lab == i + 1)).any()]
        m = np.isin(lab, keep)
    m = ndi.binary_closing(m, iterations=2)
    m = ndi.binary_fill_holes(m)
    return m, L


def stamp(img, cx, cy, r, col, op, rng, irregular=0.25, add=False, edge=1.6):
    """Soft, irregular-edged blob. add=True: additive glow instead of paint."""
    H, W = img.shape[:2]
    R = int(r * (1 + irregular) + edge + 3)
    x0, x1, y0, y1 = int(cx) - R, int(cx) + R + 1, int(cy) - R, int(cy) + R + 1
    x0c, y0c, x1c, y1c = max(x0, 0), max(y0, 0), min(x1, W), min(y1, H)
    if x1c <= x0c or y1c <= y0c:
        return
    yy, xx = np.mgrid[y0c:y1c, x0c:x1c].astype(np.float32)
    dx, dy = xx - cx, yy - cy
    d = np.hypot(dx, dy)
    th = np.arctan2(dy, dx)
    ph = rng.uniform(0, 6.28, 3)
    k = 1 + irregular * (0.5 * np.sin(2 * th + ph[0]) + 0.35 * np.sin(3 * th + ph[1]) + 0.25 * np.sin(5 * th + ph[2]))
    re = r * k
    if add:
        a = np.clip(1 - d / re, 0, 1) ** 2
        img[y0c:y1c, x0c:x1c] += a[..., None] * np.asarray(col) * op
    else:
        a = smooth((re - d) / edge + 0.5) * op
        sub = img[y0c:y1c, x0c:x1c]
        sub[:] = sub * (1 - a[..., None]) + np.asarray(col) * a[..., None]


def ellipse(img, cx, cy, rx, ry, col, op, rng):
    H, W = img.shape[:2]
    R = int(max(rx, ry)) + 4
    x0c, y0c, x1c, y1c = max(int(cx) - R, 0), max(int(cy) - R, 0), min(int(cx) + R, W), min(int(cy) + R, H)
    yy, xx = np.mgrid[y0c:y1c, x0c:x1c].astype(np.float32)
    d = np.hypot((xx - cx) / rx, (yy - cy) / ry)
    a = smooth((1 - d) / 0.35) * op
    sub = img[y0c:y1c, x0c:x1c]
    sub[:] = sub * (1 - a[..., None]) + np.asarray(col) * a[..., None]


def render(src, aura_scale=1.0):
    rng = np.random.default_rng(SEED)
    img = src.copy()
    mask, L = detect(src)
    ys, xs = np.nonzero(mask)
    cx, cy = xs.mean(), ys.mean()

    # feathered mask
    mf = np.clip(blur(mask.astype(np.float32), 1.5 / 1.0 * 0.8), 0, 1)
    mf = np.maximum(mf, mask * 0.0)

    # 1. body ramp: stretch the ghost's own luminance (p10..p95)
    Lg = L[mask]
    lo, hi = np.percentile(Lg, 10), np.percentile(Lg, 97)
    Lp = np.clip((L - lo) / max(hi - lo, 1e-4), 0, 1)
    # shaded edges go crimson: distance-to-edge term (sub-pixel anti-alias plus ~5 px inner falloff)
    dist = ndi.distance_transform_edt(mask)
    edge_t = smooth(dist / 11.0)
    Lp = Lp ** 0.8 * (0.22 + 0.78 * edge_t)
    Lp = np.clip(Lp * 1.04, 0, 1)
    body = DEEP + Lp[..., None] * (PALE - DEEP)
    # soft pale shoulders: keep interior brightest
    a = mf[..., None]
    # also swallow the original bright rim just outside the mask (anti-alias halo)
    halo = np.clip(blur(mask.astype(np.float32), 1.2) * 1.0, 0, 1)
    halo_a = np.clip(halo * 1.6, 0, 1)[..., None]
    L3 = np.clip((L - 0.15) / 0.6, 0, 1)[..., None]
    outer = DEEP * 1.2 + (PALE - DEEP) * 0.15
    img = img * (1 - a) + body * a
    # halo ring: original pixels bright-ish outside mask -> crimson edge
    ring = np.clip(halo_a - a, 0, 1) * L3
    img = img * (1 - ring) + outer * ring

    # 2. crimson aura (additive, mostly outside)
    m = mask.astype(np.float32)
    inv = 1 - m
    g1 = blur(m, 18)
    g2 = blur(m, 45) * 0.5
    glow = g1 + g2
    # outside + edge weighting (0.35 inside so the edge fades to crimson, not the body)
    glow = glow * (inv * 1.0 + m * 0.07)
    # limb ends: far from the centroid -> stronger
    yy, xx = np.mgrid[0:src.shape[0], 0:src.shape[1]].astype(np.float32)
    dc = np.hypot((xx - cx) / 1.0, (yy - cy) / 1.0)
    dn = dc / max(np.percentile(np.hypot(xs - cx, ys - cy), 98), 1)
    wl = 0.8 + 0.7 * np.clip(dn, 0, 1.2)
    glow = glow * wl
    # keep tendrils legible: aura multiplies darkness a bit lower over tendril pixels
    img = img + glow[..., None] * AURA_COL * aura_scale
    # light screen-style fade: avoid blowing out over blue lines
    img = np.clip(img, 0, 1)

    # 3. blood at tendril roots
    r_, g_, b_ = src[..., 0], src[..., 1], src[..., 2]
    dark_red = (r_ > g_ * 1.5 + 0.01) & (r_ > b_ * 1.2 + 0.005) & (r_ > 0.07) & (r_ < 0.55) & (L < 0.35)
    band = ndi.binary_dilation(mask, iterations=ROOT_BAND) & ~mask
    cand = np.argwhere(dark_red & band)
    roots = []
    if len(cand):
        # lower-priority sorting: spread by greedy farthest-point with min separation 45px
        order = rng.permutation(len(cand))
        for i in order:
            y, x = cand[i]
            if all(np.hypot(x - rx, y - ry) > 45 for rx, ry in roots):
                roots.append((x, y))
            if len(roots) >= 8:
                break
    roots = sorted(roots, key=lambda p: p[1])[:8]
    n_roots = len(roots)

    # glow under blots first, then blots, then highlights
    for x, y in roots:
        stamp(img, x, y, 28, np.array([0.57, 0.03, 0.09]), 0.6, rng, 0.15, add=True)
    for x, y in roots:
        r = rng.uniform(11, 14)
        # nudge blot slightly over the body so the white edge is covered
        stamp(img, x - 1, y, r, np.array([0.035, 0.002, 0.012]), 0.85, rng, 0.42, edge=5.0)
        stamp(img, x + rng.uniform(-4, 4), y + rng.uniform(-4, 4), r * 0.5, np.array([0.035, 0.002, 0.012]), 0.9, rng, 0.45, edge=2.5)
    for x, y in roots:
        hx, hy = x - 3, y - 4
        stamp(img, hx, hy, 1.8, np.array([0.55, 0.18, 0.195]), 0.75, rng, 0.0, edge=1.8)

    dark = np.array([0.035, 0.002, 0.012])
    hl = np.array([0.55, 0.18, 0.195])
    drip_roots = [roots[i] for i in (1, 3, 4, 6) if i < n_roots][: 4 if n_roots >= 7 else 3]
    for x, y in drip_roots:
        length = rng.uniform(20, 60)
        n = int(length // 1.6) + 3
        for j in range(n):
            t = j / max(n - 1, 1)
            r = 6.2 - 3.2 * t
            px, py = x + rng.uniform(-0.6, 0.6), y + 4 + t * length
            stamp(img, px, py, r, dark, 0.55, rng, 0.05, edge=1.8)
        # head blob
        hx, hy = x, y + 4 + length
        stamp(img, hx, hy, 3, dark, 0.92, rng, 0.1, edge=1.4)
        stamp(img, hx - 1, hy - 1, 1.1, hl, 0.7, rng, 0.0, edge=1.2)
        # faint glow beside the drip
        stamp(img, x, y + length * 0.5, 12, np.array([0.57, 0.03, 0.09]), 0.12, rng, 0.1, add=True)

    # floor drops below the ghost
    y_floor = ys.max()
    fx = [cx - 28, cx + 8, cx + 36]
    for i, fxx in enumerate(fx[:3]):
        fy = y_floor + rng.uniform(14, 30)
        stamp(img, fxx, fy, 9, np.array([0.57, 0.03, 0.09]), 0.35, rng, 0.1, add=True)
        ellipse(img, fxx, fy, rng.uniform(8, 12), rng.uniform(3, 4.5), dark, 0.92, rng)
        ellipse(img, fxx - 2, fy - 1, 2.2, 1.0, hl, 0.6, rng)
    # drop mid-fall
    mx_, my_ = cx - 10, y_floor - 8
    stamp(img, mx_, my_, 2.6, dark, 0.95, rng, 0.0, edge=1.4)
    for j in range(1, 8):
        stamp(img, mx_, my_ - j * 1.4, 1.5 * (1 - j / 10), dark, 0.9, rng, 0.0, edge=1.0)
    stamp(img, mx_ - 0.8, my_ - 0.8, 0.9, hl, 0.6, rng, 0.0, edge=1.0)

    info = dict(roots=roots, mask_px=int(mask.sum()), lo=float(lo), hi=float(hi))
    return np.clip(img, 0, 1), info


def to_img(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))


def crop2x(im):
    return im.crop(CROP).resize(((CROP[2] - CROP[0]) * 2, (CROP[3] - CROP[1]) * 2), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    orig = Image.open(SRC).convert("RGB")
    src = np.asarray(orig, dtype=np.float32) / 255
    res, info = render(src, 1.0)
    full = to_img(res)
    full.save(os.path.join(OUT, "ghost_mockup_full.png"))
    after = crop2x(full)
    before = crop2x(orig)
    after.save(os.path.join(OUT, "ghost_mockup_crop.png"))
    before.save(os.path.join(OUT, "ghost_before_crop.png"))
    for s, name in ((0.6, "aura06"), (1.5, "aura15")):
        r2, _ = render(src, s)
        crop2x(to_img(r2)).save(os.path.join(OUT, f"ghost_mockup_crop_{name}.png"))
    # compare
    w, h = before.size
    pad, top = 12, 44
    cmp_ = Image.new("RGB", (w * 2 + pad * 3, h + top + pad), (18, 18, 22))
    cmp_.paste(before, (pad, top))
    cmp_.paste(after, (w + pad * 2, top))
    d = ImageDraw.Draw(cmp_)
    try:
        f = ImageFont.truetype("arial.ttf", 26)
    except Exception:
        f = ImageFont.load_default()
    d.text((pad, 8), "BEFORE (flat white ghost, cut-off aura)", fill=(230, 230, 230), font=f)
    d.text((w + pad * 2, 8), "AFTER (blood-rooted mock-up)", fill=(255, 200, 200), font=f)
    cmp_.save(os.path.join(OUT, "ghost_mockup_compare.png"))
    print(info)


if __name__ == "__main__":
    main()
