"""ots_fade_preview.py: v3.56 PW356 offline sheet of After Shock's over-the-shoulder see-her fade (src/ai_sim.h AiOtsBox /
AiOtsK, used by src/afterimage.inc AiFrame).

Stand-in only: her body is a capsule stick figure at REAL size (the sh4-player-effects skill's bind measures: Playable
Eileen, hips 24 u apart; vanilla Henry, hips 36 u, thicker limbs), the copies are the same figure drawn additively in the
trail's blue (gain = 0.55 x AiFall x AiGate, as afterimage.inc) and depth-tested against her (reversed Z, GREATEREQUAL:
a copy behind her is hidden by her body, a copy in front of her is added over her). The Long trail of a run (8 copies,
30 u apart). AiOtsBox / AiOtsK are ported line by line; test/ai_sim_test.c section 10 checks the C functions themselves.

Columns: the over-the-shoulder camera (camera.inc's defaults: 800 u back, 330 up, no side offset) as v3.55 drew it; the same with v3.56's fade (AfterImageOtsFade 30 %); a stock side
camera (the rule is not applied there; the numbers show AiOtsK would not dim a copy there anyway). Rows: Eileen, Henry.

    <python 3.10 with numpy + PIL> tools/ots_fade_preview.py [out.png]
"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw

W_PX, H_PX, FOCAL = 360, 480, 1100.0
MIN_K = 0.30


def smooth(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def ots_box(head, lf, rf, lh, rh):  # ai_sim.h AiOtsBox
    ank = (lf + rf) * 0.5
    c = (head + ank) * 0.5
    span = float(np.linalg.norm(head - ank))
    hips = float(np.linalg.norm(lh - rh))
    if not (40.0 < span < 600.0) or not hips < 120.0:
        return None
    return c, min(max(hips + 22.0, 30.0), 80.0), min(max(0.5 * span + 30.0, 60.0), 240.0)


def ots_k(eye, c, W, H, p, min_k):  # ai_sim.h AiOtsK (Y down there; this sheet is Y up: only lengths and dots are used)
    f = c - eye
    dH = float(np.linalg.norm(f))
    if dH <= 1.0:
        return 1.0
    f = f / dH
    v = p - eye
    along = float(v @ f)
    if along <= 1.0:
        return 1.0
    front = smooth(8.0, 40.0, dH - along)
    if front <= 0.0:
        return 1.0
    r = np.array([-f[2], 0.0, f[0]])
    l = math.hypot(r[0], r[2])
    r = r / l if l > 1e-3 else np.array([1.0, 0.0, 0.0])
    u = np.cross(f, r)
    x, y = float(v @ r) / along, float(v @ u) / along
    e = max(abs(x) / (W / dH + W / along), abs(y) / (H / dH + H / along))
    k = min_k + (1.0 - min_k) * smooth(0.45, 1.0, e)
    return 1.0 - front * (1.0 - k)


def figure(henry, at, fwd):  # the stick figure: (segments [(a, b, radius)], bones dict), Y up, floor 0
    right = np.array([fwd[2], 0.0, -fwd[0]])
    def P(h, side=0.0, ahead=0.0):
        return at + np.array([0.0, h, 0.0]) + right * side + fwd * ahead
    if henry:
        b = dict(head=P(317), neck=P(303), chest=P(260), pelvis=P(219), lsh=P(284, -34), rsh=P(284, 34), lel=P(228, -38), rel=P(228, 38),
                 lwr=P(174, -36), rwr=P(174, 36), lhip=P(186, -18), rhip=P(186, 18), lkn=P(108, -17, 6), rkn=P(108, 17, -6),
                 lft=P(24, -16, 10), rft=P(24, 16, -10))
        rl, rt, rh = 15.0, 36.0, 21.0
    else:
        b = dict(head=P(307.3), neck=P(290.3), chest=P(239.1), pelvis=P(210.9), lsh=P(276.5, -28.4), rsh=P(276.5, 28.4), lel=P(232, -31),
                 rel=P(232, 31), lwr=P(178, -29), rwr=P(178, 29), lhip=P(178.2, -12.05), rhip=P(178.2, 12.05), lkn=P(102.5, -11, 6),
                 rkn=P(102.5, 11, -6), lft=P(26.3, -10, 10), rft=P(26.3, 10, -10))
        rl, rt, rh = 8.0, 24.0, 19.0
    seg = [(b['pelvis'], b['chest'], rt), (b['chest'], b['neck'], rt * 0.8), (b['neck'], b['head'], rh * 0.6), (b['head'] + np.array([0, 18.0, 0]), b['head'] + np.array([0, 24.0, 0]), rh),
           (b['lsh'], b['rsh'], rl), (b['lsh'], b['lel'], rl), (b['lel'], b['lwr'], rl * 0.85), (b['rsh'], b['rel'], rl), (b['rel'], b['rwr'], rl * 0.85),
           (b['lhip'], b['rhip'], rt * 0.9), (b['lhip'], b['lkn'], rl * 1.4), (b['lkn'], b['lft'], rl * 1.1), (b['rhip'], b['rkn'], rl * 1.4),
           (b['rkn'], b['rft'], rl * 1.1)]
    return seg, b


def camera(eye, target):
    f = target - eye
    f = f / np.linalg.norm(f)
    r = np.cross(np.array([0.0, 1.0, 0.0]), f)
    r = r / np.linalg.norm(r)
    u = np.cross(f, r)
    return eye, r, u, f


def raster(cam, segs):  # per pixel: the nearest depth of this figure (inf = not covered)
    eye, r, u, f = cam
    yy, xx = np.mgrid[0:H_PX, 0:W_PX].astype(np.float32)
    depth = np.full((H_PX, W_PX), np.inf, np.float32)
    for a, b, rad in segs:
        pa, pb = [np.array([(p - eye) @ r, (p - eye) @ u, (p - eye) @ f]) for p in (a, b)]
        if pa[2] < 5.0 or pb[2] < 5.0:
            continue
        sa = np.array([W_PX / 2 + FOCAL * pa[0] / pa[2], H_PX / 2 - FOCAL * pa[1] / pa[2]])
        sb = np.array([W_PX / 2 + FOCAL * pb[0] / pb[2], H_PX / 2 - FOCAL * pb[1] / pb[2]])
        d = sb - sa
        L2 = float(d @ d) or 1e-6
        t = np.clip(((xx - sa[0]) * d[0] + (yy - sa[1]) * d[1]) / L2, 0.0, 1.0)
        z = pa[2] + (pb[2] - pa[2]) * t
        dist = np.hypot(xx - (sa[0] + d[0] * t), yy - (sa[1] + d[1] * t))
        cov = dist < FOCAL * rad / z
        depth = np.where(cov & (z < depth), z, depth)
    return depth


def panel(henry, view, fade):
    fwd = np.array([0.0, 0.0, 1.0])
    her_segs, hb = figure(henry, np.zeros(3), fwd)
    if view == 'ots':
        eye = np.array([0.0, 330.0 + 30.0, -800.0])  # camera.inc's defaults: Distance 800, Side 0, Height 330 (+ the tilt's aim)
        cam = camera(eye, np.array([0.0, 150.0, 0.0]))
    else:
        eye = np.array([900.0, 300.0, -100.0])
        cam = camera(eye, np.array([0.0, 160.0, -100.0]))
    box = ots_box(hb['head'], hb['lft'], hb['rft'], hb['lhip'], hb['rhip'])
    img = np.zeros((H_PX, W_PX, 3), np.float32) + np.array([0.07, 0.07, 0.08], np.float32)
    her_d = raster(cam, her_segs)
    shade = np.clip(1.4 - her_d / 400.0, 0.35, 1.0)[..., None]
    img = np.where(np.isfinite(her_d)[..., None], np.array([0.55, 0.42, 0.40], np.float32) * shade, img)
    ks = []
    n = 8
    for k in range(1, n + 1):
        back = 30.0 * k
        segs, cb = figure(henry, -fwd * back, fwd)
        gain = 0.55 * (1.0 - (k - 1) / n) ** 1.4 * smooth(20.0, 70.0, back) * smooth(10.0, 30.0, back)
        kb = ots_box(cb['head'], cb['lft'], cb['rft'], cb['lhip'], cb['rhip'])
        ok = ots_k(eye, box[0], box[1], box[2], kb[0], MIN_K) if (fade and box and kb) else 1.0
        okd = ots_k(eye, box[0], box[1], box[2], kb[0], MIN_K) if (box and kb) else 1.0
        ks.append(okd)
        d = raster(cam, segs)
        vis = np.isfinite(d) & (d <= her_d)  # reversed Z GREATEREQUAL: drawn where nearer than her (or no her)
        col = np.array([0.805, 0.9025, 1.0]) * (1 - (k - 1) / (n - 1)) + np.array([0.25, 0.35, 1.0]) * ((k - 1) / (n - 1))
        img += vis[..., None] * (col * gain * ok * 0.8).astype(np.float32)
    out = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    return out, ks


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'ots_fade_sheet.png'
    cols = [('ots', False, 'over the shoulder, v3.55'), ('ots', True, 'over the shoulder, v3.56 (30 %)'), ('side', False, 'stock side camera (unchanged)')]
    sheet = Image.new('RGB', (W_PX * 3, (H_PX + 54) * 2 + 30), (20, 20, 22))
    dr = ImageDraw.Draw(sheet)
    dr.text((8, 8), 'After Shock, Long trail at a run: the copies between the camera and her (AiOtsK) - v3.56 PW356', fill=(230, 230, 230))
    for row, henry in enumerate((False, True)):
        for col, (view, fade, cap) in enumerate(cols):
            p, ks = panel(henry, view, fade)
            x, y = col * W_PX, 30 + row * (H_PX + 54)
            sheet.paste(p, (x, y))
            dr.text((x + 6, y + H_PX + 4), '%s: %s' % ('Henry' if henry else 'Eileen', cap), fill=(230, 230, 230))
            dr.text((x + 6, y + H_PX + 20), 'kept 1..8: ' + ' '.join('%.2f' % k for k in ks), fill=(180, 200, 255))
            dr.text((x + 6, y + H_PX + 36), 'drawn: %s' % ('these values' if fade else 'full light (AiOtsK would give these)'),
                    fill=(150, 150, 150))
    sheet.save(out)
    print('wrote', out)


if __name__ == '__main__':
    main()
