#!/usr/bin/env python
"""Offline preview of the v3.26.0 "rain on glass" screen effect (src/rain.inc RnWalk + FxRainDraw).

Run:  py -3.10 tools/rain_preview.py [--no-gif]
Reads the JSON-lines state dumps produced by `test/out/rain_sim_test.exe dump <file> <secs> <strength>`
(dumped here on demand) and renders what rain.inc draws: refraction pass (SRCALPHA/INVSRCALPHA) for drops,
beads, mist, then trail segments; then the 2x-modulate pass.  2x supersampled, bilinear, 1280x720 output.
No constant of src/ is changed; the numbers below are copies of the ones in rain.inc / rain_sim.h / screenfx.inc.
"""
import json, math, os, re, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "staging", "v326_sv_rain", "renders")
EXE = os.path.join(ROOT, "test", "out", "rain_sim_test.exe")
SCREENS = os.path.join(ROOT, "staging")

# ---- constants copied from rain.inc / rain_sim.h
RN_M, RN_LOOK_UP, RN_REFR_DIFF = 2.4, 0.3, 0.95
RN_TRAIL_SHIFT, RN_TRAIL_REFR = 6.0, 0.80
RN_GLINT = 0.24
RN_HZ, RN_GROW_S, RN_LIFE_FADE = 120, 0.08, 0.6
RN_MIST_FILL, RN_MIST_CLEAR = 6.0, 0.2
SS = 2  # supersample

# ------------------------------------------------------------------ texel formulas (screenfx.inc)
M32 = 0xFFFFFFFF


def fx_hash_s(a):
    a &= M32
    a ^= a >> 16
    a = (a * 0x7FEB352D) & M32
    a ^= a >> 15
    a = (a * 0x846CA68B) & M32
    a ^= a >> 16
    return a


def fx_rnd_s(a):
    return (fx_hash_s(a) & 0xFFFFFF) / 16777216.0


def sat(v):
    return np.clip(v, 0.0, 1.0)


def smooth(e0, e1, x):
    t = sat((x - e0) / (e1 - e0))
    return t * t * (3.0 - 2.0 * t)


def fx_byte(f):
    return np.floor(sat(np.asarray(f, np.float32) * 0.5) * 255.0 + 0.5)  # byte 0..255


def drop_atlas():
    """256x256: returns alpha (0..1) and grey (0..1, 0x80 = neutral), quantised like the 8-bit texture."""
    y, x = np.mgrid[0:256, 0:256]
    cell = (x >= 128).astype(np.int64) + 2 * (y >= 128).astype(np.int64)
    a = ((x & 127) + 0.5) / 64.0 - 1.0
    b = ((y & 127) + 0.5) / 64.0 - 1.0
    a = a.astype(np.float32)
    b = b.astype(np.float32)
    hc = np.array([fx_hash_s(c * 0x9E3779B9 + 0xD20B) for c in range(4)], np.int64)
    r0 = np.array([fx_rnd_s(int(h)) for h in hc])[cell]
    r1 = np.array([fx_rnd_s(int(h) ^ 1) for h in hc])[cell]
    r2 = np.array([fx_rnd_s(int(h) ^ 2) for h in hc])[cell]
    bb = np.where(b < 0, b * 1.12, b * 0.96)
    ba = bb - 0.04
    th = np.arctan2(ba, a)
    rr = np.sqrt(a * a + ba * ba)
    edge = 0.86 * (1 + 0.05 * np.sin(2 * th + 6.2831853 * r0) + 0.035 * np.sin(3 * th + 6.2831853 * r1)
                   + 0.02 * np.sin(5 * th + 6.2831853 * r2))
    q = rr / edge
    al = 1.0 - smooth(1.0 - 1.5 / 55.0, 1.0, q)
    rim = np.exp(-((q - 0.86) / 0.10) ** 2) * (0.75 + 0.25 * sat(0.5 - 0.5 * a + 0.5 * b))
    gx, gy = a + 0.34, b + 0.40
    glint = np.exp(-(gx * gx + gy * gy) / (0.13 * 0.13))
    glint2 = np.exp(-((a - 0.22) ** 2 + (b + 0.30) ** 2) / (0.06 * 0.06))
    caustic = smooth(0.15, 0.55, b) * smooth(0.95, 0.55, q) * (1.0 - smooth(0.5, 0.9, np.abs(a)))
    g = 1.0 - 0.62 * rim + 0.95 * glint + 0.35 * glint2 + 0.30 * caustic
    g = 1.0 + (g - 1.0) * al
    g = np.where(q < 1.0, g, 1.0)
    alpha = np.floor(sat(al) * 255.0 + 0.5) / 255.0
    grey = fx_byte(g) / 255.0
    return alpha.astype(np.float32), grey.astype(np.float32)


def trail_tex():
    """64x8 trail texture; uniform along y, so 1-D across x. Returns alpha, grey (64,)."""
    x = np.arange(64)
    u = (x + 0.5) / 32.0 - 1.0
    a = np.exp(-np.power(np.abs(u) / 0.62, 4.0))
    g = 1.0 + 0.55 * np.exp(-(u / 0.18) ** 2) - 0.18 * np.exp(-((np.abs(u) - 0.72) / 0.12) ** 2)
    return (np.floor(sat(a) * 255 + 0.5) / 255.0).astype(np.float32), (fx_byte(g) / 255.0).astype(np.float32)


ATL_A, ATL_G = drop_atlas()
TR_A, TR_G = trail_tex()
NEUTRAL = 128.0 / 255.0  # 0x80

# ------------------------------------------------------------------ sampling
def bilin(img, cx, cy):
    """img (H,W[,C]); cx,cy continuous pixel coordinates (texel centres at k+0.5), clamp addressing."""
    H, W = img.shape[:2]
    fx = np.clip(cx - 0.5, 0, W - 1)
    fy = np.clip(cy - 0.5, 0, H - 1)
    x0 = np.minimum(fx.astype(np.int32), W - 2)
    y0 = np.minimum(fy.astype(np.int32), H - 2)
    tx, ty = fx - x0, fy - y0
    if img.ndim == 3:
        tx, ty = tx[..., None], ty[..., None]
    a = img[y0, x0]
    b = img[y0, x0 + 1]
    c = img[y0 + 1, x0]
    d = img[y0 + 1, x0 + 1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def bilin1(vec, cx):
    n = vec.shape[0]
    fx = np.clip(cx - 0.5, 0, n - 1)
    x0 = np.minimum(fx.astype(np.int32), n - 2)
    t = fx - x0
    return vec[x0] * (1 - t) + vec[x0 + 1] * t


def qa(v):  # 8-bit vertex alpha / grey
    return math.floor(min(max(v, 0.0), 1.0) * 255.0 + 0.5) / 255.0


def smooth_s(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ quads
class Canvas:
    def __init__(self, backdrop, unit):
        self.src = backdrop.astype(np.float32)  # the frame copy (never modified)
        self.dst = backdrop.astype(np.float32).copy()
        self.H, self.W = backdrop.shape[:2]
        self.U = unit

    def _bbox(self, xs, ys):
        x0 = max(int(math.floor(min(xs) - 0.5)), 0)
        x1 = min(int(math.ceil(max(xs) + 0.5)), self.W)
        y0 = max(int(math.floor(min(ys) - 0.5)), 0)
        y1 = min(int(math.ceil(max(ys) + 0.5)), self.H)
        return x0, x1, y0, y1

    def blend(self, x0, x1, y0, y1, mask, col, alpha):
        reg = self.dst[y0:y1, x0:x1]
        a = (alpha * mask)[..., None]
        reg[:] = col * a + reg * (1 - a)

    def modulate(self, x0, x1, y0, y1, mask, grey, va):
        reg = self.dst[y0:y1, x0:x1]
        src = NEUTRAL + (grey - NEUTRAL) * va
        src = np.where(mask, src, 0.5)  # outside the quad: untouched
        reg[:] = np.clip(reg * 2.0 * src[..., None], 0, 255)

    # ---- drop quad (axis aligned); cell uv, optional refraction uv
    def drop(self, X, Y, R, Ry, cell, mode, va, kd=None):
        xs = (X - R, X + R)
        ys = (Y - Ry, Y + Ry)
        x0, x1, y0, y1 = self._bbox(xs, ys)
        if x1 <= x0 or y1 <= y0:
            return
        py, px = np.mgrid[y0:y1, x0:x1]
        px = px + 0.5
        py = py + 0.5
        inside = (px >= X - R) & (px < X + R) & (py >= Y - Ry) & (py < Y + Ry)
        s = (px - (X - R)) / (2 * R)
        t = (py - (Y - Ry)) / (2 * Ry)
        u0 = 0.5 * (cell & 1) + 0.5 / 256.0
        v0 = 0.5 * ((cell >> 1) & 1) + 0.5 / 256.0
        u1 = u0 + 0.5 - 1.0 / 256.0
        v1 = v0 + 0.5 - 1.0 / 256.0
        au = (u0 + s * (u1 - u0)) * 256.0
        av = (v0 + t * (v1 - v0)) * 256.0
        if mode == 0:
            Yc = Y - RN_LOOK_UP * R
            H = RN_M * R
            fu0, fu1 = (X + H), (X - H)       # pixels (already x Tw)
            fv0, fv1 = (Yc + H), (Yc - H)
            fx = fu0 + s * (fu1 - fu0)
            fy = fv0 + t * (fv1 - fv0)
            col = bilin(self.src, fx, fy) * qa(RN_REFR_DIFF)
            ta = bilin(ATL_A, au, av)
            self.blend(x0, x1, y0, y1, inside, col, ta * va)
        else:
            g = bilin(ATL_G, au, av)
            self.modulate(x0, x1, y0, y1, inside, g, va)

    def trail(self, x, y, nx, ny, mode, va):
        x0, x1, y0, y1 = self._bbox(x, y)
        if x1 <= x0 or y1 <= y0:
            return
        py, px = np.mgrid[y0:y1, x0:x1]
        px = px + 0.5
        py = py + 0.5
        ax, ay = x[1] - x[0], y[1] - y[0]  # across
        bx, by = x[2] - x[0], y[2] - y[0]  # along
        la, lb = ax * ax + ay * ay, bx * bx + by * by
        s = ((px - x[0]) * ax + (py - y[0]) * ay) / la
        t = ((px - x[0]) * bx + (py - y[0]) * by) / lb
        inside = (s >= 0) & (s < 1) & (t >= 0) & (t < 1)
        cx = s * 64.0
        if mode == 1:
            sh = RN_TRAIL_SHIFT * self.U
            col = bilin(self.src, px + nx * sh, py + ny * sh)
            ta = bilin1(TR_A, cx)
            self.blend(x0, x1, y0, y1, inside, col, ta * va)
        else:
            g = bilin1(TR_G, cx)
            self.modulate(x0, x1, y0, y1, inside, g, va)


# ------------------------------------------------------------------ RnWalk
def items(fr):
    """Yield (cx, cy, r, cell, fade, sy) for drops, beads, mist in RnWalk order."""
    out = []
    for d in fr["drops"]:
        x, y, r, st, age, shape, spd, bead = d
        st = int(st)
        if not st:
            continue
        grow = 0.4 + 0.6 * smooth_s(0.0, RN_GROW_S * RN_HZ, float(age))
        rr = r * (grow if st == 1 and not int(bead) else 1.0)  # = RnWalk (INSP326-014 / 016)
        fade = 1.0 - age / (RN_LIFE_FADE * RN_HZ) if st == 3 else 1.0
        sy = 1.0 + 0.15 * min(max(spd / 120.0, 0), 1) if st == 2 else 1.0
        out.append((x, y, rr, int(shape), fade, sy))
    for b in fr["beads"]:
        x, y, r, age, life, cell = b
        if life <= 0 or age < 0 or age >= life:
            continue
        fade = smooth_s(0, 0.15 * RN_HZ, age) * (1 - smooth_s(life - 0.5 * RN_HZ, life, age))
        out.append((x, y, r, int(cell), fade, 1.0))
    for j, m in enumerate(fr["mist"]):
        x, y, r, cl, ap = m
        if cl >= 0:
            fade = 1.0 - cl / (RN_MIST_CLEAR * RN_HZ)
        else:
            fade = smooth_s(0, RN_MIST_FILL * RN_HZ, ap)
        out.append((x, y, r, j & 3, fade * 0.8, 1.0))
    return out


def trail_quads(fr, U):
    out = []
    for sg in fr["segs"]:
        x0_, y0_, x1_, y1_, w, f = sg
        hw = 0.5 * w * (f ** 0.7) * U
        if not hw > 0.15:
            continue
        dx, dy = (x1_ - x0_) * U, (y1_ - y0_) * U
        ln = math.hypot(dx, dy)
        if not ln > 0.01:
            continue
        nx, ny = -dy / ln, dx / ln
        ex, ey = dx / ln * 0.5, dy / ln * 0.5
        x = [x0_ * U - ex + nx * hw, x0_ * U - ex - nx * hw, x1_ * U + ex + nx * hw, x1_ * U + ex - nx * hw]
        y = [y0_ * U - ey + ny * hw, y0_ * U - ey - ny * hw, y1_ * U + ey + ny * hw, y1_ * U + ey - ny * hw]
        out.append((x, y, nx, ny, f))
    return out


def render_frame(backdrop, fr, kd):
    H, W = backdrop.shape[:2]
    U = H / 720.0
    cv = Canvas(backdrop, U)
    its = items(fr)
    tq = trail_quads(fr, U)
    for (cx, cy, r, cell, fade, sy) in its:  # 1a refraction: drops, beads, mist
        if not (fade > 0.01) or not (r > 0.3):
            continue
        R = r * U
        cv.drop(cx * U, cy * U, R, R * sy, cell, 0, qa((0.55 + 0.45 * kd) * fade))
    for (x, y, nx, ny, f) in tq:  # 1b refraction: trails
        cv.trail(x, y, nx, ny, 1, qa(RN_TRAIL_REFR * f * (0.6 + 0.4 * kd)))
    for (cx, cy, r, cell, fade, sy) in its:  # 2a modulate: drops
        if not (fade > 0.01) or not (r > 0.3):
            continue
        R = r * U
        cv.drop(cx * U, cy * U, R, R * sy, cell, 2, qa(kd * fade))
    for (x, y, nx, ny, f) in tq:  # 2b modulate: trails
        cv.trail(x, y, nx, ny, 3, qa(kd * 0.8 * f))
    nm = len(fr["mist"])
    for (cx, cy, r, cell, fade, sy) in its[:len(its) - nm]:  # 3b additive glints (FXX_GLOW: soft round, (1 - smooth(r))^2)
        if not (fade > 0.01) or not (r > 0.3):
            continue
        R = r * U
        gx, gy, gr = cx * U - 0.34 * R, cy * U - 0.40 * R * sy, 0.30 * R + 0.6 * U
        gk = RN_GLINT * (0.5 + 0.5 * kd) * fade
        x0, x1 = max(int(gx - gr), 0), min(int(gx + gr) + 1, cv.W)
        y0, y1 = max(int(gy - gr), 0), min(int(gy + gr) + 1, cv.H)
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        rr = np.sqrt((xx + 0.5 - gx) ** 2 + (yy + 0.5 - gy) ** 2) / gr
        t = np.clip(rr, 0, 1)
        a = (1 - t * t * (3 - 2 * t)) ** 2
        cv.dst[y0:y1, x0:x1] = np.clip(cv.dst[y0:y1, x0:x1] + (255.0 * gk * a)[..., None], 0, 255)
    return cv.dst


# ------------------------------------------------------------------ I/O
def load_dump(path):
    frames = []
    with open(path) as fh:
        for line in fh:
            line = re.sub(r"\[,", "[", line)  # the harness prints a leading comma when drop slot 0 is free
            frames.append(json.loads(line))
    return frames


def ensure_dump(name, secs, strength):
    p = os.path.join(OUT, name)
    if not os.path.exists(p):
        subprocess.run([EXE, "dump", p, str(secs), str(strength)], check=True)
    return load_dump(p)


def frame_at(frames, t):
    return frames[min(max(int(round(t * 30)) - 1, 0), len(frames) - 1)]


def load_backdrop(path, titlebar):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if titlebar:  # windowed screenshot: cut the title bar and borders (about 31 px at 1282x752)
        k = w / 1282.0
        im = im.crop((int(round(1 * k)), int(round(31 * k)), w - int(round(1 * k)), h - int(round(1 * k))))
    w, h = im.size
    tw, th = (w, int(round(w * 9 / 16))) if w * 9 <= h * 16 * 1.0 + 1e9 else (w, h)
    th = int(round(w * 9 / 16))
    if th > h:
        tw, th = int(round(h * 16 / 9)), h
    else:
        tw = w
    left, top = (w - tw) // 2, (h - th) // 2
    im = im.crop((left, top, left + tw, top + th)).resize((1280, 720), Image.LANCZOS)
    return im


def synth_backdrop():
    """A bright, detailed night-city-through-a-window stand-in (the real captures are dark / spirit vision)."""
    rng = np.random.default_rng(7)
    W, H = 1280, 720
    y = np.linspace(0, 1, H)[:, None, None]
    sky = (np.array([40, 60, 110]) * (1 - y) + np.array([235, 170, 120]) * y) * np.ones((H, W, 3))
    img = Image.fromarray(sky.astype(np.uint8))
    d = ImageDraw.Draw(img)
    for layer, (base, col) in enumerate([(430, (60, 62, 84)), (500, (36, 38, 52)), (570, (18, 20, 30))]):
        x = 0
        while x < W:
            bw = int(rng.integers(50, 140))
            bh = int(rng.integers(60, 260)) + (2 - layer) * 40
            d.rectangle([x, base - bh + 140, x + bw, H], fill=col)
            for wy in range(base - bh + 150, base + 130, 16):
                for wx in range(x + 6, x + bw - 8, 14):
                    if rng.random() < 0.45:
                        c = (255, int(190 + rng.integers(0, 60)), int(90 + rng.integers(0, 90)))
                        d.rectangle([wx, wy, wx + 6, wy + 9], fill=c)
            x += bw + int(rng.integers(0, 12))
    for _ in range(40):
        cx, cy, r = int(rng.integers(0, W)), int(rng.integers(380, 700)), int(rng.integers(3, 9))
        c = [(255, 250, 220), (255, 120, 90), (120, 220, 255), (255, 220, 120)][int(rng.integers(0, 4))]
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c)
    for gx in range(0, W, 64):
        d.line([gx, 0, gx, H], fill=(25, 25, 30), width=2)
    d.line([0, 360, W, 360], fill=(25, 25, 30), width=3)
    return img


def save(arr, name, size=None):
    im = Image.fromarray(np.clip(arr + 0.5, 0, 255).astype(np.uint8))
    if size:
        im = im.resize(size, Image.LANCZOS)
    p = os.path.join(OUT, name)
    im.save(p)
    return im


def render_still(bd_img, frames, t, kd, name):
    big = np.asarray(bd_img.resize((1280 * SS, 720 * SS), Image.BILINEAR), np.float32)
    fr = frame_at(frames, t)
    img = render_frame(big, fr, kd)
    # downsample 2x (box)
    img = img.reshape(720, SS, 1280, SS, 3).mean(axis=(1, 3))
    save(img, name)
    return Image.open(os.path.join(OUT, name)), fr


def main():
    os.makedirs(OUT, exist_ok=True)
    gif = "--no-gif" not in sys.argv
    f06 = ensure_dump("dump06.jsonl", 10, 0.6)
    f10 = ensure_dump("dump10.jsonl", 10, 1.0)
    p1 = os.path.join(SCREENS, "feedback_v322", "screens", "02_spirit_hauntings_floating_victims.png")
    p2 = os.path.join(SCREENS, "feedback_v323", "screens", "02_thermal_train_cynthia.webp")
    bd1 = load_backdrop(p1, True)
    bd2 = load_backdrop(p2, True)
    bd3 = synth_backdrop()
    bd1.save(os.path.join(OUT, "_backdrop1.png"))
    stills = []
    for t, nm in [(1, "rain_t1.png"), (2.5, "rain_t2_5.png"), (4, "rain_t4.png"), (6, "rain_t6.png"), (9, "rain_t9.png")]:
        stills.append(render_still(bd1, f06, t, 0.6, nm)[0])
        print(nm)
    stills.append(render_still(bd1, f10, 6, 1.0, "rain_strong_t6.png")[0])
    # close-up: largest sliding drop near t 6 (strength 0.6)
    best = None
    for k in range(int(5.0 * 30), int(7.0 * 30)):
        for d in f06[k]["drops"]:
            if int(d[3]) == 2 and (best is None or d[2] > best[0]):
                best = (d[2], k, d)
    r, k, d = best
    print("closeup: frame", k, "t", f06[k]["t"], "drop", d)
    big = np.asarray(bd1.resize((2560, 1440), Image.BILINEAR), np.float32)
    img = render_frame(big, f06[k], 0.6).reshape(720, 2, 1280, 2, 3).mean(axis=(1, 3))
    cx = int(min(max(d[0] - 160, 0), 1280 - 320))
    cy = int(min(max(d[1] - 110, 0), 720 - 180))  # trail is mostly above a sliding drop
    crop = Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).crop((cx, cy, cx + 320, cy + 180)).resize((960, 540), Image.NEAREST)
    crop.save(os.path.join(OUT, "rain_closeup.png"))
    stills.append(crop.resize((1280, 720)))
    stills.append(render_still(bd2, f06, 6, 0.6, "rain_alt_t6.png")[0])
    stills.append(render_still(bd3, f06, 6, 0.6, "rain_synth_t6.png")[0])
    render_still(bd3, f10, 6, 1.0, "rain_synth_strong_t6.png")
    big3 = np.asarray(bd3.resize((2560, 1440), Image.BILINEAR), np.float32)
    img3 = render_frame(big3, f06[k], 0.6).reshape(720, 2, 1280, 2, 3).mean(axis=(1, 3))
    Image.fromarray(np.clip(img3 + 0.5, 0, 255).astype(np.uint8)).crop((cx, cy, cx + 320, cy + 180)).resize(
        (960, 540), Image.NEAREST).save(os.path.join(OUT, "rain_synth_closeup.png"))
    # contact sheet 3 x 3
    names = ["t1", "t2.5", "t4", "t6", "t9", "strong t6 (1.0)", "closeup 3x", "alt backdrop t6", "synthetic bright t6"]
    sheet = Image.new("RGB", (1920, 1080))
    dr = ImageDraw.Draw(sheet)
    for i, (im, nm) in enumerate(zip(stills, names)):
        tile = im.convert("RGB").resize((640, 360), Image.LANCZOS)
        sheet.paste(tile, ((i % 3) * 640, (i // 3) * 360))
        dr.text(((i % 3) * 640 + 6, (i // 3) * 360 + 4), nm, fill=(255, 255, 0))
    sheet.save(os.path.join(OUT, "rain_sheet.png"))
    if gif:
        big = np.asarray(bd1.resize((2560, 1440), Image.BILINEAR), np.float32)
        ims = []
        for n in range(120):
            fr = f06[min(int(round(n * 2)) , len(f06) - 1)]
            img = render_frame(big, fr, 0.6).reshape(720, 2, 1280, 2, 3).mean(axis=(1, 3))
            ims.append(Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).resize((640, 360), Image.LANCZOS))
        ims[0].save(os.path.join(OUT, "rain_anim.gif"), save_all=True, append_images=ims[1:], duration=66, loop=0)
        print("gif done")


if __name__ == "__main__":
    main()
