"""afterimage_preview.py: offline look preview of the v3.29 AFTERIMAGE "Volt Trail" (src/afterimage.inc).

Stand-in only: fog_preview.py's room, camera and filters; her body is a capsule stick figure (the game redraws her real
textured model, so the glow there carries her clothes / face detail through the texture term). Everything else follows
the C code: echo lags and count (kAiTrail), the glow = colour x gain x (0.35 tex + 0.45) added (kPsGvGlow, ONE / ONE),
depth-tested against the scene and her real body (GREATEREQUAL), gain = 0.55 x fall x movement gate x near fade x
flicker; arcs = jagged additive ribbons (core + 3 x glow), motes, the stun zap / burst / blue shock; Thermal: heat-coloured
translucent echoes after the palette; Spirit Vision: pale spirit light after the edge pass; night vision: x 0.7 before
its filter.

v3.30 AS330: After Shock: the row's values are the trail lengths (Short 3 echoes over 0.2 s, Medium 5 over 0.35 s = v3.29's
On, Long 8 over 0.6 s); `lengths` renders the three side by side (ai_lengths_sheet.png).

v3.32 AS332 (`v332`, the default): the copies evenly along her path (AiEchoAt), the path gate with the overlap fade
(AiGate), no lens gate but the lens fan (AiFanShift) and the near fade relative to her depth (AiNearK), the streaks (head,
hands, feet), the aura (arcs pushed in front of her body, tendrils off her silhouette, the corona, scaled by the charge);
`ver='331'` renders v3.31's rules for the side-by-side sheets. Sheets: ai332_behind_sheet, ai332_broken_sheet,
ai332_lengths_sheet, ai332_aura_sheet.

v3.33 AS333 (the default): the copies in line (no lens fan), the cloak (ai_sim.h AiCloakBuild ported), the arcs on the cloak,
Thermal hot points on the outline; Playable Eileen and vanilla Henry at their REAL size (bind skeletons, measured girth).
Sheets: ai333_cloak_sheet, ai333_looks_sheet, ai333_inline_sheet, ai333_orbs_vs_cloak_sheet (`v332`: the v3.32 sheets).

v3.34 FX334 (the default; staging/v334_feedback/w2_fx/DESIGN.md): the cloak with soft edges (ai_sim.h v3.34: no edge lines,
the fill fades out at her silhouette, unlit chain ends), v3.33 beside it. Sheets: ai334_cloak_before_after,
ai334_cloak_close, ai334_cloak_looks (`v333`: the v3.33 sheets). Note: this preview's strips have round caps (the distance to
the segment), the game's are square: v3.34's unlit chain ends matter more in game than here.

v3.35 FX335 (the default; staging/v335_feedback/w2_fx/DESIGN.md; the user: "remove the blue glow from the player on After
Shock"): no cloak (ver '335': nothing drawn from the cloak but Thermal's hot points), v3.34 beside it; what stays on her
(the arcs, tendrils, strikes, sparks; the trail's copies and streaks); the effect budget's copies (every other copy fades,
the trail keeps its length: `keep`). Sheets: ai335_glow_before_after, ai335_close, ai335_looks, ai335_budget_trail (`v334`:
the v3.34 sheets).

usage: python afterimage_preview.py [outdir] [lengths|v331|v332|v333|v334|only=<sheet word>]   (Python with numpy + Pillow; default outdir ../../renders)
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fog_preview as fp  # noqa: E402

ARGS = [a for a in sys.argv[1:] if a not in ('lengths', 'v331', 'v332', 'v333', 'v334') and not a.startswith('only=')]
V334_SHEETS_ONLY = 'v334' in sys.argv[1:]  # v3.35 FX335: the v3.34 sheets
V332_SHEETS_ONLY = 'v332' in sys.argv[1:]  # v3.33 AS333: the v3.32 sheets
V333_SHEETS_ONLY = 'v333' in sys.argv[1:]  # v3.34 FX334: the v3.33 sheets
ONLY333 = next((a[5:] for a in sys.argv[1:] if a.startswith('only=')), None)  # v3.33 AS333: one sheet, e.g. only=cloak
ONLY_LENGTHS = 'lengths' in sys.argv[1:]
OLD_SHEETS = 'v331' in sys.argv[1:]
OUT = ARGS[0] if ARGS else os.path.normpath(os.path.join(HERE, '..', '..', 'renders'))
fp.W, fp.H = 960, 540
SS = 1

# ---- afterimage.inc constants
G0, TEX_K, FILL_K = 0.55, 0.35, 0.45
NEAR0, NEAR1 = 120.0, 300.0
NV_K = 0.7
COL_NEAR, COL_TAIL = (0.805, 0.9025, 1.0), (0.25, 0.35, 1.0)
COL_SV, ARC, HOT, SHOCK = (0.55, 0.75, 1.0), (0.85, 0.95, 1.0), (1.0, 0.95, 0.80), (0.45, 0.65, 1.0)
TRAIL = {'Short': (3, 0.20), 'Medium': (5, 0.35), 'Long': (8, 0.60)}  # v3.30 AS330 (ai_sim.h kAiTrail)
M32 = 0xFFFFFFFF


def ai_hash(a):
    a &= M32
    a ^= a >> 16
    a = (a * 0x7FEB352D) & M32
    a ^= a >> 15
    a = (a * 0x846CA68B) & M32
    a ^= a >> 16
    return a


def rnd01(a):
    return (ai_hash(a) >> 8) / 16777216.0


def smooth(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def flicker(t, k):
    u = t * 24.0 + 17.3 * k
    fl = math.floor(u)
    fr = u - fl
    kk = (k * 0x9E3779B9) & M32
    a, b = rnd01((int(fl) & M32) ^ kk), rnd01(((int(fl) + 1) & M32) ^ kk)
    v = 0.8 + 0.2 * (a + (b - a) * fr * fr * (3 - 2 * fr))
    if rnd01(((int(math.floor(t / 0.05)) * 0x85EBCA6B) & M32) ^ ((kk + 0x51) & M32)) < 0.06:
        v *= 1.5
    return v


def fall(k, n):
    return (1.0 - (k - 1) / n) ** 1.4


# ---- her motion and a stand-in gait
def pose(px, pz, yaw, ph, amp, lean):
    s = math.sin(ph)
    up, dn = max(0.0, s), max(0.0, -s)
    L = dict(pelvis=(0, 94 - 5 * abs(s) * amp, 0), chest=(0, 124, 9 * lean), neck=(0, 144, 14 * lean), head=(0, 158, 17 * lean),
             lSh=(-19, 140, 11 * lean), rSh=(19, 140, 11 * lean),
             lElb=(-23, 114, 11 * lean - 16 * s * amp), rElb=(23, 114, 11 * lean + 16 * s * amp),
             lHand=(-21, 96 + 10 * amp, 11 * lean - 30 * s * amp + 8 * amp), rHand=(21, 96 + 10 * amp, 11 * lean + 30 * s * amp + 8 * amp),
             lHip=(-10, 91, 0), rHip=(10, 91, 0),
             lKnee=(-11, 50 + 10 * amp * dn, 24 * s * amp + 8 * amp), rKnee=(11, 50 + 10 * amp * up, -24 * s * amp + 8 * amp),
             lFoot=(-11, 4 + 30 * amp * dn, 40 * s * amp - 10 * amp * dn), rFoot=(11, 4 + 30 * amp * up, -40 * s * amp - 10 * amp * up))
    fx, fz, rx, rz = math.sin(yaw), math.cos(yaw), math.cos(yaw), -math.sin(yaw)
    return {k: np.array((px + x * rx + z * fx, y, pz + x * rz + z * fz)) for k, (x, y, z) in L.items()}


class Motion:
    """pel(t) -> (x, z), yaw, speed; the gait phase is integrated over the path"""

    def __init__(self, fn, amp, lean):
        self.fn, self.amp, self.lean = fn, amp, lean

    def bones(self, t):
        x, z, yaw, sp = self.fn(t)
        dist, tt, step = 0.0, -2.0, 1.0 / 240
        while tt < t:  # the distance walked up to t (gait phase)
            a, b = self.fn(tt)[:2], self.fn(tt + step)[:2]
            dist += math.hypot(b[0] - a[0], b[1] - a[1])
            tt += step
        stride = 184.0 + (457.0 - 184.0) * min(1.0, sp / 663.0)
        return pose(x, z, yaw, 2 * math.pi * dist / stride, self.amp(sp), self.lean(sp))


RUN_AMP = lambda sp: min(1.0, sp / 400.0)  # noqa: E731
RUN_LEAN = lambda sp: min(1.0, sp / 663.0)  # noqa: E731
MOTIONS = {
    'run_x': Motion(lambda t: (150 + 663 * t, 0.0, math.pi / 2, 663.0), RUN_AMP, RUN_LEAN),
    'walk_x': Motion(lambda t: (60 + 175 * t, 0.0, math.pi / 2, 175.0), RUN_AMP, RUN_LEAN),
    'run_away': Motion(lambda t: (0.0, -150 - 663 * t, math.pi, 663.0), RUN_AMP, RUN_LEAN),
    'dodge': Motion(lambda t: (-90 + 850 * min(0.2, max(0.0, t + 0.2)), -40.0, math.pi, 850.0 if t > -0.2 else 0.0),
                    lambda sp: 0.25, lambda sp: 0.2),
    'stand': Motion(lambda t: (40.0, 0.0, math.pi / 2, 0.0), RUN_AMP, RUN_LEAN),
}
CAMS = {
    'side': dict(pos=(30.0, 125.0, 520.0), target=(30.0, 95.0, 0.0), fov=50.0),
    'high': dict(pos=(0.0, 300.0, 450.0), target=(0.0, 50.0, 0.0), fov=60.0),
    'ots': dict(pos=(0.0, 170.0, 110.0), target=(0.0, 125.0, -150.0), fov=55.0),
    'close': dict(pos=(40.0, 120.0, 330.0), target=(40.0, 100.0, 0.0), fov=45.0),
}


# ---- raster helpers (supersampled-free; 1 px = 1 output px)
def body_mask(cam, bones, edges=None, head=13.0):
    """capsule figure: coverage mask (0..1) and view depth per pixel (painter's order)"""
    edges = edges or [(bones[a], bones[b], r) for a, b, r in fp.PEDGES]
    segs = list(edges) + [(bones['head'], bones['head'], head)] if isinstance(bones, dict) else list(edges)
    zi = Image.new('F', (cam.w, cam.h), 1e9)
    mi = Image.new('L', (cam.w, cam.h), 0)
    dz, dm = ImageDraw.Draw(zi), ImageDraw.Draw(mi)
    items = []
    for pa, pb, rad in segs:
        a, b = cam.project(pa), cam.project(pb)
        if a and b:
            items.append(((a[2] + b[2]) / 2, a, b, rad))
    items.sort(key=lambda s: -s[0])
    for z, a, b, rad in items:
        wpx = max(2.0, 2 * rad * cam.pix_per_unit(z))
        dz.line([a[0], a[1], b[0], b[1]], fill=z, width=int(wpx))
        dm.line([a[0], a[1], b[0], b[1]], fill=255, width=int(wpx))
        for p in (a, b):
            rr = wpx / 2
            dz.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=z)
            dm.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=255)
    m = np.asarray(mi.filter(ImageFilter.GaussianBlur(0.8)), dtype=np.float32) / 255.0
    return m, np.asarray(zi, dtype=np.float32)


FAKE_TEX = None


def fake_tex(cam):
    """a stand-in for her texture's luminance (clothes / skin detail), screen-space noise"""
    global FAKE_TEX
    if FAKE_TEX is None or FAKE_TEX.shape != (cam.h, cam.w):
        gy, gx = np.mgrid[0:cam.h, 0:cam.w].astype(np.float32)
        FAKE_TEX = 0.25 + 0.5 * fp.tex_sample(fp.GRIME_TEX, gx / 90.0, gy / 90.0).reshape(cam.h, cam.w)
    return FAKE_TEX


def splat(img, zb, cam, p, rad, col, a):
    pr = cam.project(p)
    if not pr or a < 0.004:
        return
    r = max(1.2, rad * cam.pix_per_unit(pr[2]))
    x0, x1 = int(max(0, pr[0] - 3 * r)), int(min(cam.w, pr[0] + 3 * r + 1))
    y0, y1 = int(max(0, pr[1] - 3 * r)), int(min(cam.h, pr[1] + 3 * r + 1))
    if x0 >= x1 or y0 >= y1:
        return
    gy, gx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    v = a * np.exp(-((gx + 0.5 - pr[0]) ** 2 + (gy + 0.5 - pr[1]) ** 2) / (r * r))
    v = np.where(pr[2] < zb[y0:y1, x0:x1] + 1.0, v, 0)
    img[y0:y1, x0:x1] += v[..., None] * np.array(col, dtype=np.float32)


def arc(S, cam, a, b, n, amp, seed, w, k, col):
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    ln = float(np.linalg.norm(d))
    if ln < 0.5 or k < 0.004:
        return
    e = (a + b) / 2 - cam.c
    u = np.cross(d, e)
    u /= (np.linalg.norm(u) + 1e-9)
    v = np.cross(d, u) / ln
    pts, hw = [], []
    for i in range(n):
        s = i / (n - 1)
        env = math.sin(math.pi * s)
        r1, r2 = (rnd01(seed + 2 * i) - 0.5) * 2, rnd01(seed + 2 * i + 1) - 0.5
        pts.append(a + d * s + (u * r1 + v * r2) * amp * ln * env)
        hw.append(w * (0.55 + 0.45 * env))
    fp.draw_strip(S, cam, pts, hw, [k] * n, col, minpx=0.75)
    fp.draw_strip(S, cam, pts, [x * 3 for x in hw], [0.3 * k] * n, col, minpx=1.5)


# ---- one frame
def render(name, motion, cam_name, look='normal', trail='Medium', t=0.0, foe=None, stun_age=None, idle=False):
    if fp.FOG_TEX is None:
        fp.FOG_TEX = fp.make_tex(128, 1, 4, fp.TEX_WARP)
        fp.GRIME_TEX = fp.make_tex(128, 7, 5, 0.0, base=3)
    cam = fp.Cam(SS, CAMS[cam_name])
    fp.NEAR0, fp.NEAR1 = 20.0, 90.0
    rgb, zb = fp.render_scene(cam, fp.ROOM_BACK, 0.0, player=False)
    mo = MOTIONS[motion]
    her = mo.bones(t)
    if look == 'th':
        rgb = (np.clip(fp._lum(rgb) * 1.8, 0, 1) ** 0.8 * fp.TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
    fp.draw_player(cam, rgb, zb, her, (fp.TH_PLAYER,) * 3 if look == 'th' else (0.30, 0.26, 0.27))
    fb = None
    if foe is not None:  # a Gum Head stand-in
        fb = fp.victim_bones(foe, 1.0, 0.3)
        segs = [(fb[a], fb[b], r) for a, b, r in fp.EDGES] + [(fb[0], fb[0], 13.0)]
        fp.draw_bones(cam, rgb, zb, segs, (0.85,) * 3 if look == 'th' else (0.17, 0.13, 0.12))
    img = rgb.reshape(cam.h, cam.w, 3).copy()
    zb2 = zb.reshape(cam.h, cam.w)
    if look == 'th':
        img = fp.filter_th(np.clip(img, 0, 1))
    if look == 'sv':
        img = fp.filter_sv(np.clip(img, 0, 1))
    n, span = TRAIL[trail]
    tex = fake_tex(cam)
    echoes = []
    last = None  # v3.30 AS330: the stack fade (AiStackK): the pelvis of the last echo drawn
    for k in range(1, n + 1):
        lag = span * k / n
        eb = mo.bones(t - lag)
        mv = smooth(20, 70, math.hypot(eb['pelvis'][0] - her['pelvis'][0], eb['pelvis'][2] - her['pelvis'][2]))
        de, dh = float(np.linalg.norm(eb['pelvis'] - cam.c)), float(np.linalg.norm(her['pelvis'] - cam.c))
        nr = smooth(NEAR0, NEAR1, de) * smooth(-90.0, -10.0, de - dh)  # near fade x the lens gate (AiLensK)
        fl = flicker(t + 10.0, k)
        if last is not None:
            mv *= smooth(2.0, 10.0, float(np.linalg.norm(eb['pelvis'] - last)))
        g = G0 * fall(k, n) * mv * nr * fl
        if g >= 0.004:
            last = eb['pelvis']
        u = (k - 1) / (n - 1)
        col = np.array(COL_NEAR) + (np.array(COL_TAIL) - np.array(COL_NEAR)) * u
        echoes.append(dict(k=k, b=eb, g=g, col=col, heat=0.8 - 0.55 * u, alpha=0.6 * fall(k, n) * mv * nr * (0.85 + 0.15 * fl)))
    for E in echoes:  # the echo bodies (in-game: her model on the stored pose, additive, depth-tested, no z write)
        if E['g'] < 0.004:
            continue
        m, z = body_mask(cam, E['b'])
        vis = (z < zb2 + 0.5).astype(np.float32) * m
        if look == 'th':
            hcol = np.array([np.interp(min(1.0, E['heat'] * 0.9 + 0.1), [h[0] for h in fp.HEAT], [h[1][c] for h in fp.HEAT]) for c in range(3)])
            a = (vis * E['alpha'])[..., None]
            img = img * (1 - a) + hcol * a
        else:
            col = np.array(COL_SV) if look == 'sv' else E['col']
            gk = E['g'] * (NV_K if look == 'nv' else 0.8 if look == 'sv' else 1.0)
            img += (vis * (TEX_K * tex + FILL_K) * gk)[..., None] * col
    S = dict(imgimg=img, zbimg=zb2)
    now = t + 10.0
    e60, e80, e40 = int(math.floor(now / 0.06)), int(math.floor(now / 0.08)), int(math.floor(now / 0.04))
    arc_col = HOT if look == 'th' else ARC
    vis_e = [E for E in echoes if E['g'] / G0 >= 0.05]
    if look != 'th':
        names = fp.PBONES
        limb = [e for e in fp.PEDGES[3:]]
        for E in vis_e:  # crackle along the limbs
            for j in range(2):
                h = ai_hash(e60 * 977 + E['k'] * 131 + j * 7)
                if (h >> 8 & 255) > 178:
                    continue
                a_, b_, _ = limb[h % len(limb)]
                pa, pb = E['b'][a_], E['b'][b_]
                s0 = 0.6 * rnd01(h ^ 0x55)
                s1 = min(1.0, s0 + 0.35 + 0.4 * rnd01(h ^ 0x77))
                arc(S, cam, pa + (pb - pa) * s0, pa + (pb - pa) * s1, 5, 0.28, h, 0.75, 0.95 * E['g'] / G0, arc_col)
        if (now / 0.08) % 1.0 < 0.75:  # the chain
            for E, F in zip(vis_e, vis_e[1:]):
                h = ai_hash(e80 * 2654435761 + E['k'] * 40503)
                bn = names[h % len(names)]
                arc(S, cam, E['b'][bn], F['b'][bn], 7, 0.15, h, 0.6, 0.75 * min(E['g'], F['g']) / G0, arc_col)
        if idle:  # a faint crackle at her right hand
            d = her['rHand'] - her['rElb']
            tip = her['rHand'] + d / np.linalg.norm(d) * 14 + np.array((3.0, -4.0, 5.0))
            arc(S, cam, her['rHand'], tip, 5, 0.3, 77, 0.6, 0.5, ARC)
    # motes from the two newest echoes' hands / feet (a snapshot of a 40 a second stream)
    seed = 1234
    for E in vis_e[:2]:
        for q in range(7):
            bn = ('lHand', 'rHand', 'lFoot', 'rFoot')[ai_hash(seed) & 3]
            seed += 1
            age, life = 0.3 * rnd01(seed), 0.18 + 0.17 * rnd01(seed + 1)
            seed += 2
            if age >= life:
                continue
            ang, sp = 2 * math.pi * rnd01(seed), 60 + 80 * rnd01(seed + 1)
            seed += 2
            p = E['b'][bn] + np.array((math.cos(ang) * sp, 20 + 40 * rnd01(seed), math.sin(ang) * sp)) * age
            a = 1 - age / life
            splat(img, zb2, cam, p, 2.4, HOT if look == 'th' else ARC, (0.7 if look == 'th' else 0.9) * a * a)
    if foe is not None and stun_age is not None:  # the stun: zap, burst, shock
        E = echoes[min(2, len(echoes) - 1)]
        kz = math.sqrt(max(0.0, 1 - stun_age / 0.25))
        if look != 'th':
            arc(S, cam, E['b']['chest'], fb[2], 10, 0.16, ai_hash(e40 * 0x9E3779B9), 1.5, kz, ARC)
            for f in range(2):
                h = ai_hash(e40 * 0x85EBCA6B ^ f * 0x27D4EB2F)
                s = 0.25 + 0.4 * rnd01(h)
                mid = E['b']['chest'] + (np.array(fb[2]) - E['b']['chest']) * s
                tip = np.array(fb[2]) + np.array([(rnd01(h + 11 + q) - 0.5) * 70 for q in range(3)])
                arc(S, cam, mid, tip, 6, 0.25, h, 0.9, 0.7 * kz, ARC)
            splat(img, zb2, cam, fb[2], 26 * 0.45, SHOCK, 0.45 * kz)
            segs = [(np.array(fb[a]), np.array(fb[b]), r) for a, b, r in fp.EDGES] + [(np.array(fb[0]), np.array(fb[0]), 13.0)]
            m, z = body_mask(cam, segs, edges=segs)
            vis = (z < zb2 + 1.5).astype(np.float32) * m
            gs = 0.35 * (1 - stun_age / 1.2) * (NV_K if look == 'nv' else 1.0)
            if look != 'sv':
                img += (vis * (TEX_K * tex + FILL_K) * gs)[..., None] * np.array(SHOCK)
        for q in range(12):
            h = ai_hash(900 + q)
            age, life = stun_age, 0.25 + 0.2 * rnd01(h)
            ang, sp = 2 * math.pi * rnd01(h + 1), 80 + 120 * rnd01(h + 2)
            p = np.array(fb[2]) + np.array((math.cos(ang) * sp, (rnd01(h + 3) - 0.5) * 2 * sp, math.sin(ang) * sp)) * age
            splat(img, zb2, cam, p, 3.0, HOT if look == 'th' else ARC, (0.7 if look == 'th' else 0.9) * (1 - age / life) ** 2)
    if look == 'nv':
        img = fp.filter_nv(np.clip(img, 0, 1))
    im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
    os.makedirs(OUT, exist_ok=True)
    im.save(os.path.join(OUT, 'ai_%s.png' % name))
    return name


FRAMES = [
    ('run_side', 'Run (663 u/s), Medium: 5 echoes over 0.35 s, side camera', dict(motion='run_x', cam_name='side')),
    ('run_side_long', 'Run, Long: 8 echoes over 0.6 s', dict(motion='run_x', cam_name='side', trail='Long')),
    ('dodge_high', 'Side leap (850 u/s, 0.2 s), high camera', dict(motion='dodge', cam_name='high')),
    ('run_ots', 'Running away, OTS camera: near fade 120-300 u x the lens gate', dict(motion='run_away', cam_name='ots')),
    ('walk_side', 'Walk (175 u/s): the movement gate leaves a faint tail', dict(motion='walk_x', cam_name='side')),
    ('stand_idle', 'Standing: no echoes; the idle crackle at a hand', dict(motion='stand', cam_name='close', idle=True)),
    ('stun_side', 'Stun: a Gum Head stand-in 0.08 s after the touch (zap, burst, blue shock)',
     dict(motion='run_x', cam_name='side', foe=(-20.0, 0.0, 60.0), stun_age=0.08)),
    ('stun_nv', 'Night vision (green): the same stun, x 0.7 before the filter',
     dict(motion='run_x', cam_name='side', foe=(-20.0, 0.0, 60.0), stun_age=0.08, look='nv')),
    ('stun_th', 'Thermal: residual heat (0.8 -> 0.25), white-hot points, no arcs',
     dict(motion='run_x', cam_name='side', foe=(-20.0, 0.0, 60.0), stun_age=0.08, look='th')),
    ('stun_sv', 'Spirit Vision: pale spirit light after the edge pass',
     dict(motion='run_x', cam_name='side', foe=(-20.0, 0.0, 60.0), stun_age=0.08, look='sv')),
]


# v3.30 AS330: the three lengths, one row each: a run (side camera), a side leap (high camera), a walk
LENGTH_FRAMES = []
for _tr, _txt in (('Short', '3 echoes over 0.2 s'), ('Medium', '5 over 0.35 s (v3.29 On)'), ('Long', '8 over 0.6 s')):
    LENGTH_FRAMES += [
        ('len_%s_run' % _tr.lower(), '%s, %s: run, side camera' % (_tr, _txt), dict(motion='run_x', cam_name='side', trail=_tr)),
        ('len_%s_dodge' % _tr.lower(), '%s: side leap, high camera' % _tr, dict(motion='dodge', cam_name='high', trail=_tr)),
        ('len_%s_walk' % _tr.lower(), '%s: walk (the movement gate)' % _tr, dict(motion='walk_x', cam_name='side', trail=_tr)),
    ]


def sheet(names, labels, cols=2, tw=640, out='ai_sheet.png'):
    th = tw * fp.H // fp.W
    bar = 28
    rows = (len(names) + cols - 1) // cols
    sh = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sh)
    for i, (n, lab) in enumerate(zip(names, labels)):
        im = Image.open(os.path.join(OUT, 'ai_%s.png' % n)).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sh.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), lab, fill=(225, 225, 235), font=font)
    sh.save(os.path.join(OUT, out))


# ======================================================================= v3.32 AS332
# ai_sim.h v3.32 constants
PATH0, PATH1, OVER0, OVER1 = 20.0, 70.0, 10.0, 30.0
FAN_T, FAN_CLEAR = 0.5, 0.35
NEARR0, NEARR1, NEARA0, NEARA1 = 0.35, 0.60, 45.0, 100.0
CHARGE_IDLE, CHARGE_V0, CHARGE_V1 = 0.3, 40.0, 300.0
STREAK_PTS, STREAK_K = 17, 0.55
STREAK_BONES = ('head', 'lHand', 'rHand', 'lFoot', 'rFoot')
EDGE_R = [r for _, _, r in fp.PEDGES]


def _xz(p):
    return np.array((p[0], p[2]), dtype=np.float64)


def path_ring(mo, t, hz=60, back=1.0):
    """the ring at 60 Hz (newest = her at t) and the cumulative XZ path back from her"""
    ts = [t - j / hz for j in range(int(back * hz) + 1)]
    pts = [np.array(mo.fn(tt)[:2], dtype=np.float64) for tt in ts]
    cum = [0.0]
    for j in range(1, len(pts)):
        cum.append(cum[-1] + float(np.linalg.norm(pts[j] - pts[j - 1])))
    return ts, pts, cum


def path_at(ts, cum, tq):
    """the path back to time tq (linear between samples)"""
    if tq >= ts[0]:
        return 0.0
    for j in range(1, len(ts)):
        if ts[j] <= tq:
            f = (ts[j - 1] - tq) / (ts[j - 1] - ts[j])
            return cum[j - 1] + (cum[j] - cum[j - 1]) * f
    return cum[-1]


def echo_at(ts, cum, t, span, k, n):
    """AiEchoAt: the moment of copy k of n evenly along the path of the last `span` seconds; returns (time, path)"""
    L = path_at(ts, cum, t - span)
    want = L * k / n
    if want <= cum[0]:
        return ts[0], cum[0]
    for j in range(1, len(ts)):
        if cum[j] >= want:
            seg = cum[j] - cum[j - 1]
            f = (want - cum[j - 1]) / seg if seg > 1e-4 else 0.0
            return ts[j - 1] + (ts[j] - ts[j - 1]) * f, want
    return ts[-1], cum[-1]


def gate(path, chord):
    return smooth(PATH0, PATH1, path) * smooth(OVER0, OVER1, chord)


def near_k(de, dh):
    return smooth(NEARR0 * dh, NEARR1 * dh, de) * smooth(NEARA0, NEARA1, de)


def fan_shift(along, lat, side):
    m = FAN_T * abs(along)
    x = (1.0 if side >= 0 else -1.0) * lat
    if m <= 1e-3 or x >= m:
        return 0.0
    return side * (m - x) * smooth(-1.5 * m, -0.5 * m, x)


class Fan:
    def __init__(self, cam, her_pel):
        f = _xz(her_pel) - _xz(cam.c)
        self.ok = np.linalg.norm(f) > 30.0
        self.F = f / (np.linalg.norm(f) + 1e-9)
        self.R = np.array((-self.F[1], self.F[0]))
        self.her = her_pel
        self.side = 1.0
        d = np.asarray(her_pel, float) - cam.c
        self.hd = float(np.linalg.norm(d))
        self.dir = d / (self.hd + 1e-9)
        self.c = cam.c

    def set_side(self, tail_pel):
        v = _xz(tail_pel) - _xz(self.her)
        lat, along = float(v @ self.R), float(v @ self.F)
        if abs(lat) > 4.0 and abs(lat) > FAN_CLEAR * FAN_T * abs(along):
            self.side = 1.0 if lat > 0 else -1.0

    def vec(self, pel):
        if not self.ok:
            return np.zeros(3)
        v = _xz(pel) - _xz(self.her)
        d = fan_shift(float(v @ self.F), float(v @ self.R), self.side)
        return np.array((self.R[0] * d, 0.0, self.R[1] * d))

    def near(self, p):
        return near_k(float((np.asarray(p, float) - self.c) @ self.dir), self.hd)


def to_eye(cam, p, d):
    e = cam.c - p
    ln = float(np.linalg.norm(e))
    return p + e / ln * d if ln > d + 20 else p


def across(cam, p, d):
    e = np.asarray(p, float) - cam.c
    o = np.cross(d, e)
    ln = float(np.linalg.norm(o))
    return o / ln if ln > 1e-4 else np.array((1.0, 0, 0))


def shift_bones(b, sh):
    return {k: v + sh for k, v in b.items()}


def trail_v331(mo, cam, her, t, n_total, span, ncap):
    """v3.31: time lags (a capped trail spread: AiSpreadEcho), the straight-distance gate, near 120-300 x the lens gate, the
    stack fade"""
    n = min(ncap, n_total)
    out, last = [], None
    for k in range(1, n + 1):
        kk = k if n >= n_total else (k * n_total + n - 1) // n
        eb = mo.bones(t - span * kk / n_total)
        mv = smooth(20, 70, math.hypot(eb['pelvis'][0] - her['pelvis'][0], eb['pelvis'][2] - her['pelvis'][2]))
        de, dh = float(np.linalg.norm(eb['pelvis'] - cam.c)), float(np.linalg.norm(her['pelvis'] - cam.c))
        nr = smooth(120.0, 300.0, de) * smooth(-90.0, -10.0, de - dh)
        if last is not None:
            mv *= smooth(2.0, 10.0, float(np.linalg.norm(eb['pelvis'] - last)))
        g = G0 * fall(kk, n_total) * mv * nr * flicker(t + 10.0, kk)
        if g >= 0.004:
            last = eb['pelvis']
        u = (kk - 1) / (n_total - 1)
        out.append(dict(k=kk, b=eb, g=g, u=u, heat=0.8 - 0.55 * u, alpha=0.6 * fall(kk, n_total) * mv * nr))
    return out, [], None


def trail_v332(mo, cam, her, t, n_total, span, ncap):
    n = min(ncap, n_total)
    ts, pts, cum = path_ring(mo, t)
    fan = Fan(cam, her['pelvis'])
    moments = [echo_at(ts, cum, t, span, k, n) for k in range(1, n + 1)]
    tail_x, tail_z = mo.fn(moments[-1][0])[:2]
    fan.set_side(np.array((tail_x, 0.0, tail_z)))
    out = []
    for k, (tk, pk) in enumerate(moments, 1):
        eb = mo.bones(tk)
        chord = math.hypot(eb['pelvis'][0] - her['pelvis'][0], eb['pelvis'][2] - her['pelvis'][2])
        sh = fan.vec(eb['pelvis'])
        nr = fan.near(eb['pelvis'] + sh)
        gt = gate(pk, chord)
        u = (k - 1) / (n - 1) if n > 1 else 0.0
        out.append(dict(k=k, b=shift_bones(eb, sh), g=G0 * fall(k, n) * gt * nr * flicker(t + 10.0, k), u=u, heat=0.8 - 0.55 * u,
                        alpha=0.6 * fall(k, n) * gt * nr))
    streaks = []  # (points per bone, k per point)
    J = STREAK_PTS - 1
    P = {b: [her[b]] for b in STREAK_BONES}
    K = [0.0]
    for j in range(1, J + 1):
        tj, pj = echo_at(ts, cum, t, span, j, J)
        bj = mo.bones(tj)
        chord = math.hypot(bj['pelvis'][0] - her['pelvis'][0], bj['pelvis'][2] - her['pelvis'][2])
        sh = fan.vec(bj['pelvis'])
        K.append(STREAK_K * gate(pj, chord) * (1 - j / (J + 1)) ** 1.2 * fan.near(bj['pelvis'] + sh))
        for b in STREAK_BONES:
            P[b].append(bj[b] + sh)
    streaks = (P, K)
    sp = path_at(ts, cum, t - 0.1) / 0.1
    charge = CHARGE_IDLE + (1 - CHARGE_IDLE) * smooth(CHARGE_V0, CHARGE_V1, sp)
    return out, streaks, charge


def corona_splat(img, zb, cam, p, rad, col, a):
    splat(img, zb, cam, p, rad, col, a)


def draw_aura(S, img, zb, cam, her, charge, now, look):
    c = charge
    e50 = int(math.floor(now / 0.05))
    arc_col = HOT if look == 'th' else ARC
    glow = COL_SV if look == 'sv' else SHOCK
    nA, nT = 2 + int(8 * c + 0.5), int(4 * c + 0.2)
    nvk = NV_K if look == 'nv' else 1.0
    E = fp.PEDGES
    for i in range(nA):
        h = ai_hash(e50 * 0x2545F491 + i * 977 + 0x3B)
        e = h % 15
        pa, pb, R = her[E[e][0]], her[E[e][1]], EDGE_R[e]
        s0 = 0.5 * rnd01(h ^ 0x51)
        s1 = min(1.0, s0 + 0.4 + 0.45 * rnd01(h ^ 0x73))
        k = (0.45 + 0.55 * c) * (0.75 + 0.25 * rnd01(h ^ 0x99)) * nvk
        if rnd01(h ^ 0x1F) > 0.55 + 0.4 * c:
            continue
        a = to_eye(cam, pa + (pb - pa) * s0, R + 3)
        b = to_eye(cam, pa + (pb - pa) * s1, R + 3)
        if look == 'th':
            for q in range(3):
                ss = 0.2 + 0.3 * q + 0.1 * rnd01(h + q)
                splat(img, zb, cam, a + (b - a) * ss, 1.6, HOT, 0.6 * k)
        else:
            arc(S, cam, a, b, 6, 0.32, h, 0.9, k, arc_col)
    if look != 'th':
        for i in range(nT):
            h = ai_hash(e50 * 0x9E3779B1 + i * 0x85EBCA77 + 0x5D)
            e = h % 15
            pa, pb, R = her[E[e][0]], her[E[e][1]], EDGE_R[e]
            s = rnd01(h ^ 0x21)
            side = 1.0 if (h >> 9) & 1 else -1.0
            ln = 14 + 26 * rnd01(h ^ 0x43)
            p = pa + (pb - pa) * s
            w = across(cam, p, pb - pa)
            a = p + side * w * R
            tip = a + side * w * ln + np.array((0, 0.6 * rnd01(h ^ 0x65) * ln, 0))
            arc(S, cam, a, tip, 5, 0.35, h, 0.7, 0.9 * c * nvk, arc_col)
            if (h >> 13) % 3 == 0:
                mid = (a + tip) / 2
                t2 = mid + np.array([(rnd01(h + 7 + q) - 0.5) * 0.8 * ln for q in range(3)])
                arc(S, cam, mid, t2, 4, 0.3, h ^ 0xAB, 0.5, 0.6 * c * nvk, arc_col)
        for i in range(15):
            pa, pb, R = her[E[i][0]], her[E[i][1]], EDGE_R[i]
            for q in range(4):
                s = 0.75 if q & 2 else 0.3
                side = 1.0 if q & 1 else -1.0
                fl = flicker(now * 1.4, 60 + i * 4 + q)
                p = pa + (pb - pa) * s
                p = p + side * across(cam, p, pb - pa) * (R + 2)
                corona_splat(img, zb, cam, p, (8 + 1.0 * R) * 0.45, glow, (0.10 + 0.18 * c) * fl * nvk)
        for i, (bn, br) in enumerate((('head', 30.0), ('chest', 38.0), ('pelvis', 34.0))):  # the bloom over her
            splat(img, zb, cam, to_eye(cam, her[bn], 25.0), br * 0.45, glow, (0.04 + 0.08 * c) * flicker(now, 90 + i) * nvk)
    # a ground strike while she moves (lit for the first 60 % of its 0.12 s)
    e12 = int(math.floor(now / 0.12))
    h = ai_hash(e12 * 0x7F4A7C15 + 0x77)
    if c > 0.45 and (now / 0.12) % 1.0 < 0.6 and rnd01(h) < 0.6 * c:
        src = ('lFoot', 'rFoot', 'lHand', 'rHand')[(h >> 5) & 3]
        fy = min(her['lFoot'][1], her['rFoot'][1])
        ang, dist = 2 * math.pi * rnd01(h ^ 0x3C), 25 + 35 * rnd01(h ^ 0x5A)
        tip = np.array((her[src][0] + math.cos(ang) * dist, fy, her[src][2] + math.sin(ang) * dist))
        if look == 'th':
            for q in range(3):
                splat(img, zb, cam, her[src] + (tip - her[src]) * (0.2 + 0.3 * q), 1.6, HOT, 0.6 * c)
        else:
            arc(S, cam, her[src], tip, 7, 0.3, h ^ 0x11, 1.0, 0.9 * c * nvk, arc_col)
            splat(img, zb, cam, tip + np.array((0, 3.0, 0)), 10 * 0.45, glow, 0.35 * c * nvk)
    # shed sparks (a snapshot of the 4..22 a second stream)
    rate = 22 * smooth(CHARGE_IDLE - 0.12, 1.0, c)
    for q in range(int(rate * 0.25 + 0.5)):
        h = ai_hash(5000 + q)
        e = h % 15
        p0 = her[E[e][0]] + (her[E[e][1]] - her[E[e][0]]) * rnd01(h + 1)
        age, life = 0.25 * rnd01(h + 2), 0.15 + 0.15 * rnd01(h + 3)
        if age >= life:
            continue
        ang, sp = 2 * math.pi * rnd01(h + 4), 30 + 60 * rnd01(h + 5)
        p = p0 + np.array((math.cos(ang) * sp, (rnd01(h + 6) - 0.5) * 2 * sp, math.sin(ang) * sp)) * age
        splat(img, zb, cam, p, 1.8, HOT if look == 'th' else ARC, (0.7 if look == 'th' else 0.9) * (1 - age / life) ** 2)


MOTIONS.update({
    'walk_away': Motion(lambda t: (0.0, -150 - 175 * t, math.pi, 175.0), RUN_AMP, RUN_LEAN),
    'run_diag': Motion(lambda t: (40 - 663 * t * math.sin(0.35), -150 - 663 * t * math.cos(0.35), math.pi + 0.35, 663.0), RUN_AMP, RUN_LEAN),
    'reverse': Motion(lambda t: ((-60 + 663 * (t + 0.12), 0.0, math.pi / 2, 663.0) if t < -0.12 else (-60 - 500 * (t + 0.12), 0.0, -math.pi / 2, 500.0)),
                      RUN_AMP, RUN_LEAN),
    'stopstart': Motion(lambda t: ((-150 + 663 * min(0.0, t + 0.5) + 663 * max(0.0, t + 0.1), 0.0, math.pi / 2,
                                    663.0 if (t < -0.5 or t > -0.1) else 0.0)), RUN_AMP, RUN_LEAN),
    'stand_b': Motion(lambda t: (0.0, -150.0, math.pi, 0.0), RUN_AMP, RUN_LEAN),
})
CAMS.update({
    'behind': dict(pos=(55.0, 250.0, 380.0), target=(0.0, 105.0, -250.0), fov=55.0),     # ~540 u behind her, OTS right
    'behind_near': dict(pos=(40.0, 190.0, 150.0), target=(0.0, 110.0, -250.0), fov=55.0),  # ~300 u (a tight follow camera)
    'aura_close': dict(pos=(45.0, 140.0, 140.0), target=(0.0, 100.0, -150.0), fov=50.0),
    'aura_side': dict(pos=(-40.0, 125.0, 260.0), target=(-40.0, 95.0, 0.0), fov=45.0),
})


def render332(name, motion, cam_name, look='normal', trail='Medium', t=0.0, ver='332', ncap=8, aura=True):
    if fp.FOG_TEX is None:
        fp.FOG_TEX = fp.make_tex(128, 1, 4, fp.TEX_WARP)
        fp.GRIME_TEX = fp.make_tex(128, 7, 5, 0.0, base=3)
    cam = fp.Cam(SS, CAMS[cam_name])
    fp.NEAR0, fp.NEAR1 = 20.0, 90.0
    rgb, zb = fp.render_scene(cam, fp.ROOM_BACK, 0.0, player=False)
    mo = MOTIONS[motion]
    her = mo.bones(t)
    if look == 'th':
        rgb = (np.clip(fp._lum(rgb) * 1.8, 0, 1) ** 0.8 * fp.TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
    fp.draw_player(cam, rgb, zb, her, (fp.TH_PLAYER,) * 3 if look == 'th' else (0.30, 0.26, 0.27))
    img = rgb.reshape(cam.h, cam.w, 3).copy()
    zb2 = zb.reshape(cam.h, cam.w)
    if look == 'th':
        img = fp.filter_th(np.clip(img, 0, 1))
    if look == 'sv':
        img = fp.filter_sv(np.clip(img, 0, 1))
    n, span = TRAIL[trail]
    echoes, streaks, charge = (trail_v332 if ver == '332' else trail_v331)(mo, cam, her, t, n, span, ncap)
    tex = fake_tex(cam)
    for E in echoes:
        if E['g'] < 0.004:
            continue
        m, z = body_mask(cam, E['b'])
        vis = (z < zb2 + 0.5).astype(np.float32) * m
        if look == 'th':
            hcol = np.array([np.interp(min(1.0, E['heat'] * 0.9 + 0.1), [h[0] for h in fp.HEAT], [h[1][c] for h in fp.HEAT]) for c in range(3)])
            a = (vis * E['alpha'])[..., None]
            img = img * (1 - a) + hcol * a
        else:
            col = np.array(COL_SV) if look == 'sv' else np.array(COL_NEAR) + (np.array(COL_TAIL) - np.array(COL_NEAR)) * E['u']
            gk = E['g'] * (NV_K if look == 'nv' else 0.8 if look == 'sv' else 1.0)
            img += (vis * (TEX_K * tex + FILL_K) * gk)[..., None] * col
    S = dict(imgimg=img, zbimg=zb2)
    now = t + 10.0
    e60, e80 = int(math.floor(now / 0.06)), int(math.floor(now / 0.08))
    arc_col = HOT if look == 'th' else ARC
    if ver == '332' and aura and charge is not None:
        draw_aura(S, img, zb2, cam, her, charge, now, look)
    if ver == '332' and streaks and look != 'th':
        P, K = streaks
        e50 = int(math.floor(now / 0.05))
        for si, b in enumerate(STREAK_BONES):
            pts = [p + (np.array([(rnd01(ai_hash(e50 * 0x6A09E667 + si * 131 + j * 7) + q) - 0.5) * 3 for q in range(3)]) if j else 0)
                   for j, p in enumerate(P[b])]
            kk = [k * (0.8 if si == 0 else 1.0 if si < 3 else 0.6) * (NV_K if look == 'nv' else 1.0) for k in K]
            col = COL_SV if look == 'sv' else COL_NEAR
            fp.draw_strip(S, cam, pts, [0.8] * len(pts), kk, col, minpx=0.75)
            fp.draw_strip(S, cam, pts, [3.0] * len(pts), [0.35 * k for k in kk], col, minpx=1.5)
    vis_e = [E for E in echoes if E['g'] / G0 >= 0.05]
    limb = list(fp.PEDGES[3:])
    for E in vis_e:  # crackle along the copies' limbs
        for j in range(2):
            h = ai_hash(e60 * 977 + E['k'] * 131 + j * 7)
            if (h >> 8 & 255) > 178:
                continue
            a_, b_, _ = limb[h % len(limb)]
            pa, pb = E['b'][a_], E['b'][b_]
            s0 = 0.6 * rnd01(h ^ 0x55)
            s1 = min(1.0, s0 + 0.35 + 0.4 * rnd01(h ^ 0x77))
            if look == 'th':
                continue
            arc(S, cam, pa + (pb - pa) * s0, pa + (pb - pa) * s1, 5, 0.28, h, 0.75, 0.95 * E['g'] / G0, arc_col)
    if look != 'th' and (now / 0.08) % 1.0 < 0.75:
        for E, F in zip(vis_e, vis_e[1:]):
            h = ai_hash(e80 * 2654435761 + E['k'] * 40503)
            bn = fp.PBONES[h % len(fp.PBONES)]
            arc(S, cam, E['b'][bn], F['b'][bn], 7, 0.15, h, 0.6, 0.75 * min(E['g'], F['g']) / G0, arc_col)
    if ver == '331' and not vis_e and look != 'th':  # v3.31's idle crackle at a hand
        d = her['rHand'] - her['rElb']
        tip = her['rHand'] + d / np.linalg.norm(d) * 14 + np.array((3.0, -4.0, 5.0))
        arc(S, cam, her['rHand'], tip, 5, 0.3, 77, 0.6, 0.5, ARC)
    seed = 1234
    for E in vis_e[:2]:  # motes off the two newest copies
        for q in range(7):
            bn = ('lHand', 'rHand', 'lFoot', 'rFoot')[ai_hash(seed) & 3]
            seed += 1
            age, life = 0.3 * rnd01(seed), 0.18 + 0.17 * rnd01(seed + 1)
            seed += 2
            if age >= life:
                continue
            ang, sp = 2 * math.pi * rnd01(seed), 60 + 80 * rnd01(seed + 1)
            seed += 2
            p = E['b'][bn] + np.array((math.cos(ang) * sp, 20 + 40 * rnd01(seed), math.sin(ang) * sp)) * age
            splat(img, zb2, cam, p, 2.4, HOT if look == 'th' else ARC, (0.7 if look == 'th' else 0.9) * (1 - age / life) ** 2)
    if look == 'nv':
        img = fp.filter_nv(np.clip(img, 0, 1))
    im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
    os.makedirs(OUT, exist_ok=True)
    im.save(os.path.join(OUT, 'ai_%s.png' % name))
    shown = sum(1 for E in echoes if E['g'] / G0 >= 0.05)
    return shown, len(echoes), charge


V332_SHEETS = [
    ('ai332_behind_sheet.png', 2, [
        ('b_run_331', 'v3.31: running away, camera ~540 u behind (Medium)', dict(motion='run_away', cam_name='behind', ver='331')),
        ('b_run_332', 'v3.32: the same frame: the lens fan, streaks, aura', dict(motion='run_away', cam_name='behind')),
        ('b_near_331', 'v3.31: tight camera ~300 u behind, Long', dict(motion='run_away', cam_name='behind_near', trail='Long', ver='331')),
        ('b_near_332', 'v3.32: tight camera, Long (near fade relative to her)', dict(motion='run_away', cam_name='behind_near', trail='Long')),
        ('b_walk_331', 'v3.31: walking away (175 u/s), Medium', dict(motion='walk_away', cam_name='behind', ver='331')),
        ('b_walk_332', 'v3.32: walking away, Medium', dict(motion='walk_away', cam_name='behind')),
        ('b_diag_331', 'v3.31: running away 20 deg off the view axis, Long', dict(motion='run_diag', cam_name='behind', trail='Long', ver='331')),
        ('b_diag_332', 'v3.32: the same, Long (the fan follows the trail side)', dict(motion='run_diag', cam_name='behind', trail='Long')),
    ]),
    ('ai332_broken_sheet.png', 2, [
        ('k_rev_331', 'v3.31: reversal 0.12 s ago (Long): copies vanish mid-trail', dict(motion='reverse', cam_name='high', trail='Long', ver='331')),
        ('k_rev_332', 'v3.32: the same: copies along her path', dict(motion='reverse', cam_name='high', trail='Long')),
        ('k_stop_331', 'v3.31: running again 0.1 s after a 0.4 s stop (Long)', dict(motion='stopstart', cam_name='side', trail='Long', ver='331')),
        ('k_stop_332', 'v3.32: the same: no copies piled / blanked at the stop', dict(motion='stopstart', cam_name='side', trail='Long')),
        ('k_cap_331', 'v3.31: Long capped to 3 by the guard (copies 3, 6, 8)', dict(motion='run_x', cam_name='side', trail='Long', ncap=3, ver='331')),
        ('k_cap_332', 'v3.32: Long capped to 3: evenly along the path + streaks', dict(motion='run_x', cam_name='side', trail='Long', ncap=3)),
    ]),
    ('ai332_lengths_sheet.png', 3, [
        ('l_short_b', 'Short (3 / 0.2 s): run away, camera behind', dict(motion='run_away', cam_name='behind', trail='Short')),
        ('l_medium_b', 'Medium (5 / 0.35 s): run away, behind', dict(motion='run_away', cam_name='behind', trail='Medium')),
        ('l_long_b', 'Long (8 / 0.6 s): run away, behind', dict(motion='run_away', cam_name='behind', trail='Long')),
        ('l_short_s', 'Short: run, side camera', dict(motion='run_x', cam_name='side', trail='Short')),
        ('l_medium_s', 'Medium: run, side camera', dict(motion='run_x', cam_name='side', trail='Medium')),
        ('l_long_s', 'Long: run, side camera', dict(motion='run_x', cam_name='side', trail='Long')),
        ('l_short_w', 'Short: walk, side camera', dict(motion='walk_x', cam_name='side', trail='Short')),
        ('l_medium_w', 'Medium: walk, side camera', dict(motion='walk_x', cam_name='side', trail='Medium')),
        ('l_long_w', 'Long: walk, side camera', dict(motion='walk_x', cam_name='side', trail='Long')),
    ]),
    ('ai332_aura_sheet.png', 3, [
        ('a_idle_331', 'v3.31 standing: the only aura, a hand crackle every 1.5 s', dict(motion='stand_b', cam_name='aura_close', ver='331')),
        ('a_idle_332', 'v3.32 standing (charge 0.3): faint crackle + corona', dict(motion='stand_b', cam_name='aura_close')),
        ('a_run_332', 'v3.32 running away (charge 1), close behind', dict(motion='run_away', cam_name='aura_close', trail='Short')),
        ('a_walk_side', 'v3.32 walking, side close-up (charge ~0.7)', dict(motion='walk_x', cam_name='aura_side')),
        ('a_run_side', 'v3.32 running, side close-up (charge 1)', dict(motion='run_x', cam_name='aura_side', t=-0.05)),
        ('a_nv', 'Night vision: running away, behind', dict(motion='run_away', cam_name='behind', look='nv')),
        ('a_th', 'Thermal: heat copies, white-hot points (no additive light)', dict(motion='run_away', cam_name='behind', look='th')),
        ('a_sv', 'Spirit Vision: pale spirit light', dict(motion='run_away', cam_name='behind', look='sv')),
        ('a_sv_side', 'Spirit Vision: run, side camera, Long', dict(motion='run_x', cam_name='side', look='sv', trail='Long')),
    ]),
]


def main332():
    for out, cols, frames in V332_SHEETS:
        for n, lab, kw in frames:
            shown, total, ch = render332(n, **kw)
            print('rendered %-12s copies shown %d / %d, charge %s' % (n, shown, total, 'n/a' if ch is None else '%.2f' % ch))
        sheet([f[0] for f in frames], [f[1] for f in frames], cols=cols, tw=640 if cols == 2 else 560, out=out)
        print('sheet ->', os.path.join(OUT, out))


# ======================================================================= v3.33 AS333
# The copies in line (no lens fan: AfterImageFan 0), the cloak (ai_sim.h AiCloakBuild, ported below line by line), the arcs on
# the cloak (AiOnSurface / AiEdgeR), Thermal hot points along the outline. Figures at their REAL size: Playable Eileen's and
# vanilla Henry's bind skeletons (henry01 entry 170, BUILD_NOTES "size facts") posed by a small FK rig, their bodies as capsules
# of the measured mesh girth. Computed in game units (Y down for the cloak, as the C code), drawn into fog_preview's
# half-scale room x 0.5 (the same picture as a double-size room).
RK = 0.5  # real units -> the preview room's units
BODY = {  # bind facts (game units above the floor; x lateral; toe = forward, down from the ankle)
    'eileen': dict(pelvisY=210.9, hipX=12.1, hipY=178.2, thigh=75.7, shin=76.3, ankleY=26.3, toeF=8.7, toeD=17.9, chestY=239.1,
                   neckY=290.3, headY=307.3, shX=28.4, shY=276.5, upper=44.7, fore=53.8, headR=22.0, col=(0.36, 0.24, 0.31)),
    'henry': dict(pelvisY=219.0, hipX=18.0, hipY=186.0, thigh=78.0, shin=84.0, ankleY=24.0, toeF=22.0, toeD=18.0, chestY=260.0,
                  neckY=303.3, headY=317.1, shX=34.0, shY=284.0, upper=56.0, fore=54.0, headR=24.0, col=(0.26, 0.29, 0.23)),
}
HB = ('head', 'neck', 'chest', 'pelvis', 'lSh', 'rSh', 'lElb', 'rElb', 'lHand', 'rHand', 'lHip', 'rHip', 'lKnee', 'rKnee', 'lFoot', 'rFoot',
      'lToe', 'rToe')
DP_EDGES = ((0, 1), (1, 2), (2, 3), (1, 4), (1, 5), (4, 6), (6, 8), (5, 7), (7, 9), (3, 10), (3, 11), (10, 12), (12, 14), (11, 13), (13, 15))
EDGE_CK = (((17, 18), (20, 22), 2), ((18, 28), (16, 22), 2), ((25, 36), (19, 27), 2), ((11, 17), (11, 17), 0), ((11, 17), (11, 17), 0),
           ((8.5, 16), (8.5, 16), 0), ((7, 13.5), (7, 13.5), 0), ((8.5, 16), (8.5, 16), 0), ((7, 13.5), (7, 13.5), 0),
           ((28, 34), (20, 27), 1), ((28, 34), (20, 27), 1), ((15, 20), (15, 20), 0), ((10, 17.5), (10, 17.5), 0),
           ((15, 20), (15, 20), 0), ((10, 17.5), (10, 17.5), 0))
DP_EDGE_R_V332 = (11, 13, 16, 7, 7, 5.5, 4.5, 5.5, 4.5, 9, 9, 8.5, 6.5, 8.5, 6.5)  # kDpHerEdgeR (v3.32's aura radii)
CK_CORE = (0.70, 0.88, 1.0)


def pose_real(body, px, pz, yaw, ph, amp, lean):
    """FK stand-in rig at the real bind proportions; Y up; facing +z before the yaw (as pose())"""
    B = BODY[body]
    s = math.sin(ph)
    bob = -6.0 * abs(s) * amp
    L = {}
    lt = 0.22 * lean  # forward tilt of the upper body about the pelvis

    def tilt(y):
        yr = y - B['pelvisY']
        return B['pelvisY'] + bob + yr * math.cos(lt), yr * math.sin(lt)

    L['pelvis'] = (0.0, B['pelvisY'] + bob, 0.0)
    for k, y in (('chest', B['chestY']), ('neck', B['neckY']), ('head', B['headY'])):
        yy, zz = tilt(y)
        L[k] = (0.0, yy, zz)
    for side, sg in (('l', -1.0), ('r', 1.0)):
        th = 0.55 * amp * s * (-sg)  # the left leg forward while s > 0
        fwd = math.sin(th) > 0
        kap = 0.12 + 0.35 * amp + amp * (0.25 if fwd else 1.05) * abs(math.sin(th)) / 0.55 * 0.9
        hip = np.array((sg * B['hipX'], B['hipY'] + bob, 0.0))
        knee = hip + B['thigh'] * np.array((0.0, -math.cos(th), math.sin(th)))
        a2 = th - kap
        ankle = knee + B['shin'] * np.array((0.0, -math.cos(a2), math.sin(a2)))
        fp_ = 0.6 * a2 if a2 < 0 else 0.0
        toe = ankle + np.array((0.0, -B['toeD'] * math.cos(fp_) - B['toeF'] * math.sin(fp_), B['toeF'] * math.cos(fp_) - B['toeD'] * math.sin(fp_)))
        L[side + 'Hip'], L[side + 'Knee'], L[side + 'Foot'], L[side + 'Toe'] = tuple(hip), tuple(knee), tuple(ankle), tuple(toe)
        al = 0.6 * amp * s * sg + 0.05  # the arm opposite to its leg
        be = 0.25 + 0.85 * amp
        abd = 0.10
        yy, zz = tilt(B['shY'])
        sh = np.array((sg * B['shX'], yy, zz))
        du = np.array((sg * math.sin(abd), -math.cos(al) * math.cos(abd), math.sin(al) * math.cos(abd)))
        elb = sh + B['upper'] * du
        df = np.array((sg * math.sin(abd) * 0.5, -math.cos(al + be), math.sin(al + be)))
        df /= np.linalg.norm(df)
        hand = elb + B['fore'] * df
        L[side + 'Sh'], L[side + 'Elb'], L[side + 'Hand'] = tuple(sh), tuple(elb), tuple(hand)
    fx, fz, rx, rz = math.sin(yaw), math.cos(yaw), math.cos(yaw), -math.sin(yaw)
    return {k: np.array((px + x * rx + z * fx, y, pz + x * rz + z * fz)) for k, (x, y, z) in L.items()}


class MotionR:
    """real-unit motion: fn(t) -> (x, z, yaw, speed); the gait phase over the path"""

    def __init__(self, fn, body):
        self.fn, self.body = fn, body

    def bones(self, t):
        x, z, yaw, sp = self.fn(t)
        dist, tt, step = 0.0, -2.0, 1.0 / 240
        while tt < t:
            a, b = self.fn(tt)[:2], self.fn(tt + step)[:2]
            dist += math.hypot(b[0] - a[0], b[1] - a[1])
            tt += step
        stride = 2.0 * (184.0 + (457.0 - 184.0) * min(1.0, sp / 663.0))
        return pose_real(self.body, x, z, yaw, 2 * math.pi * dist / stride, min(1.0, sp / 400.0), min(1.0, sp / 663.0))


MOTIONS_R = {
    'stand': lambda t: (0.0, 0.0, math.pi / 2, 0.0),
    'walk': lambda t: (175.0 * t, 0.0, math.pi / 2, 175.0),
    'run': lambda t: (663.0 * t, 0.0, math.pi / 2, 663.0),
    'run_away': lambda t: (0.0, -300.0 - 663.0 * t, math.pi, 663.0),
    'run_side': lambda t: (300.0 + 663.0 * t, 0.0, math.pi / 2, 663.0),
}
CAMS_R = {  # real units
    'ck34': dict(pos=(250.0, 250.0, 330.0), target=(0.0, 172.0, 0.0), fov=50.0),         # 3/4 front, close
    'behind': dict(pos=(110.0, 500.0, 760.0), target=(0.0, 210.0, -500.0), fov=55.0),     # ~1080 u behind her
    'side': dict(pos=(60.0, 250.0, 1040.0), target=(60.0, 190.0, 0.0), fov=50.0),
    'close': dict(pos=(215.0, 235.0, 285.0), target=(0.0, 215.0, 0.0), fov=36.0),       # v3.34 FX334: her arms and sides, close
}


def cam_real(name):
    spec = CAMS_R[name]
    return fp.Cam(SS, dict(pos=tuple(np.array(spec['pos']) * RK), target=tuple(np.array(spec['target']) * RK), fov=spec['fov']))


def G(p):  # preview (Y up) <-> game (Y down)
    return np.array((p[0], -p[1], p[2]), dtype=np.float64)


# ---- ai_sim.h v3.33 port (kAiCk: chain = (edges, kMul, controls (a, b, f, ext, X, W, D, lat)))
CK = [
    (2, 0.80, [('lHip', 'rHip', 0.5, 0, (0, 0), (-1, -1), (21, 27), 1), ('pelvis', 'pelvis', 0, 0, (0, 0), (28, 34), (20, 27), 1),
               ('pelvis', 'chest', 0.5, 0, (0, 0), (24, 34), (18, 27), 1), ('chest', 'chest', 0, 0, (0, 0), (25, 40), (21, 26), 2),
               ('chest', 'neck', 0.55, 0, (0, 0), (22, 34), (17, 23), 2), ('neck', 'neck', 0, 0, (0, 0), (10, 15.5), (10.5, 15.5), 2),
               ('neck', 'head', 0.35, 0, (0, 0), (16, 17), (18, 20), 2), ('neck', 'head', 1.0, 0, (0, 0), (19, 19.5), (22, 25), 2),
               ('neck', 'head', 1.0, 1, (25, 34), (12, 13), (14, 15), 2), ('neck', 'head', 1.0, 1, (33, 44), (5, 5.5), (6, 7), 2)]),
    (1, 0.80, [('lSh', 'lSh', 0, 0, (0, 0), (11, 17.5), (11, 17.5), 0), ('chest', 'neck', 0.55, 0, (0, 0), (13, 19), (13, 19), 0),
               ('rSh', 'rSh', 0, 0, (0, 0), (11, 17.5), (11, 17.5), 0)]),
] + [(2, 1.0, [(s + 'Elb', s + 'Sh', 1.0, 1, (5, 9), (8, 14), (8, 14), 0), (s + 'Sh', s + 'Sh', 0, 0, (0, 0), (9.5, 17), (9.5, 17), 0),
               (s + 'Elb', s + 'Elb', 0, 0, (0, 0), (8, 14.5), (8, 14.5), 0), (s + 'Hand', s + 'Hand', 0, 0, (0, 0), (6.5, 12.5), (6.5, 12.5), 0),
               (s + 'Elb', s + 'Hand', 1.0, 1, (14, 14), (7, 10.5), (7, 10.5), 0), (s + 'Elb', s + 'Hand', 1.0, 1, (27, 27), (4, 5.5), (4, 5.5), 0)])
     for s in ('l', 'r')] + [
    (2, 1.0, [(s + 'Hip', s + 'Hip', 0, 0, (0, 0), (17, 21), (17, 21), 0), (s + 'Hip', s + 'Knee', 0.5, 0, (0, 0), (14, 19.5), (14, 19.5), 0),
              (s + 'Knee', s + 'Knee', 0, 0, (0, 0), (11, 18), (11, 18), 0), (s + 'Knee', s + 'Foot', 0.5, 0, (0, 0), (10, 18), (10, 18), 0),
              (s + 'Foot', s + 'Foot', 0, 0, (0, 0), (8.5, 15.5), (8.5, 15.5), 0), (s + 'Toe', s + 'Toe', 0, 0, (0, 0), (8, 11), (8, 11), 0),
              (s + 'Foot', s + 'Toe', 1.0, 2, (24, 22), (5, 7), (5, 7), 0)]) for s in ('l', 'r')]


def lerp2(v, b):
    return v[0] + (v[1] - v[0]) * b


def unit(v, fb=(0.0, 0.0, 1.0)):
    n = float(np.linalg.norm(v))
    return v / n if n > 1e-5 else np.array(fb, dtype=np.float64)


def ck_extent(w, d, X, u, lat):
    if not lat or w == d:
        return w
    x = min(1.0, float(u @ X) ** 2)
    return math.sqrt(w * w * x + d * d * (1 - x))


def body_measure(bn):
    L = lambda a, b: float(np.linalg.norm(bn[a] - bn[b]))  # noqa: E731
    ln = L('pelvis', 'chest') + L('chest', 'neck') + L('neck', 'head') + 0.5 * (L('lHip', 'lKnee') + L('lKnee', 'lFoot') + L('rHip', 'rKnee') + L('rKnee', 'rFoot'))
    hips = L('lHip', 'rHip')
    build = min(1.25, max(-0.25, (hips - 24.1) / (36.0 - 24.1)))
    ls = ln / 249.5
    b01 = min(1.0, max(0.0, build))
    return dict(build=build, lenScale=ls, sizeK=min(2.0, max(0.5, ls / (1 + (261.2 / 249.5 - 1) * b01))), hips=hips)


def cloak_build(bn, eye, c, t, ver='334'):
    """AiCloakBuild (game coordinates, Y down): ribbons [(kind, ch, P[n], W[n], K[n])] and the body. ver '334': v3.34 FX334
    (no edge lines: the outlines carry no light; the fill 1.10 x + 0.3 u wide, 0.03 + 0.33 c^1.5, x (0.75 + 0.6 band), its
    chain ends unlit); '333': v3.33"""
    v4 = ver == '334'
    body = body_measure(bn)
    b, g = body['build'], body['sizeK']
    Xh, Xs = unit(bn['rHip'] - bn['lHip'], (1, 0, 0)), unit(bn['rSh'] - bn['lSh'], (1, 0, 0))
    cc = min(1.0, max(0.0, c)) ** 1.5
    kf, ke = (0.03 + 0.33 * cc, 0.0) if v4 else (0.025 + 0.24 * cc, 0.10 + 0.62 * cc)
    e50 = int(math.floor(t / 0.05)) & M32
    out = []
    for ch, (edges, kmul, ctl) in enumerate(CK):
        cp, cw, cd, cl = [], [], [], []
        for (a, bb, f, ext, X, W, D, lat) in ctl:
            pa, pb = bn[a], bn[bb]
            p = pa + (pb - pa) * f
            if ext == 1:
                p = p + unit(pb - pa, (0, -1, 0)) * lerp2(X, b) * g
            elif ext == 2:
                d = np.array((pb[0] - pa[0], 0.0, pb[2] - pa[2]))
                if np.linalg.norm(d) > 1.0:
                    p = p + d / np.linalg.norm(d) * lerp2(X, b) * g + np.array((0, 4.0 * g, 0))
            cp.append(p)
            cw.append(0.5 * body['hips'] + lerp2((17, 21), b) * g if W[0] < 0 else lerp2(W, b) * g)
            cd.append(lerp2(D, b) * g)
            cl.append(lat)
        P, Wd, Dd, lt = [], [], [], []
        for i in range(len(cp)):
            for s in ((0, 1) if i + 1 < len(cp) else (0,)):
                i1 = i + 1 if s else i
                f = 0.5 * s
                P.append(cp[i] + (cp[i1] - cp[i]) * f)
                Wd.append(cw[i] + (cw[i1] - cw[i]) * f)
                Dd.append(cd[i] + (cd[i1] - cd[i]) * f)
                lt.append(cl[i1] if (s and cl[i1]) else cl[i])
        n = len(P)
        Lc = [0.0]
        for i in range(1, n):
            Lc.append(Lc[-1] + float(np.linalg.norm(P[i] - P[i - 1])))
        A, R, Rv = [], [], []
        for i in range(n):
            d = P[min(n - 1, i + 1)] - P[max(0, i - 1)]
            e = P[i] - eye
            a = np.cross(d, e)
            if np.linalg.norm(a) > 1e-4 * (np.linalg.norm(e) * np.linalg.norm(d) + 1e-6):
                a = a / np.linalg.norm(a)
            else:
                a = unit(np.array((-e[2], 0.0, e[0])), (1, 0, 0))
            X = Xh if lt[i] == 1 else Xs
            A.append(a)
            R.append(ck_extent(Wd[i], Dd[i], X, a, lt[i]))
            Rv.append(ck_extent(Wd[i], Dd[i], X, unit(e), lt[i]))
        fl = flicker(t * 1.3, 120 + ch)
        tm = math.fmod(t, 1000.0)
        PF, WF, KF = [], [], []
        for i in range(n):
            band = (0.5 + 0.5 * math.sin(2 * math.pi * (Lc[i] / (70.0 * g) - 2.2 * tm) + 2.1 * ch + 4.0)) ** 6
            push = Rv[i] + 1.5 * g
            de = eye - P[i]
            dl = float(np.linalg.norm(de))
            if not dl > push + 20:
                push, dl = 0.0, 1.0
            PF.append(P[i] + de / dl * push)
            WF.append(R[i] * 1.10 + 0.3 * g if v4 else R[i] * 1.30 + 2.0 * g)
            KF.append(kf * kmul * fl * ((0.75 + 0.6 * band) if v4 else (0.85 + 0.3 * band)))
        if v4:
            KF[0] = KF[-1] = 0.0
        elif ch == 0:
            KF[0] *= 0.4
        out.append((0, ch, PF, WF, KF))
        if edges == 1 and sum(a[1] for a in A) > 0:
            A = [-a for a in A]
        for s in range(edges):
            side = -1.0 if s else 1.0
            PE, KE = [], []
            for i in range(n):
                h = ai_hash((e50 * 0x2C1B3C6D + ch * 131 + s * 17 + i * 7) & M32)
                jit = (rnd01(h) - 0.5) * 2 * (0.5 + 0.9 * cc) * g
                jit2 = (rnd01(h ^ 0x5B) - 0.5) * 1.2 * g
                band = (0.5 + 0.5 * math.sin(2 * math.pi * (Lc[i] / (70.0 * g) - 2.2 * tm) + 2.1 * ch + 1.3 * side)) ** 6
                tg = unit(P[min(n - 1, i + 1)] - P[max(0, i - 1)], (0, 0, 0))
                push = 0.4 * Rv[i]
                de = eye - P[i]
                dl = float(np.linalg.norm(de))
                if not dl > push + 20:
                    push, dl = 0.0, 1.0
                PE.append(P[i] + A[i] * (side * R[i] + jit) + tg * jit2 + de / dl * push)
                KE.append(ke * kmul * fl * (0.55 + 0.9 * band))
            if v4:  # v3.34: her outline as points only (Thermal's hot points), never drawn
                out.append((1, ch, PE, [0.8 * g] * n, [0.0] * n))
                continue
            if ch == 0:
                KE[0] *= 0.3
            out.append((1, ch, PE, [0.8 * g] * n, KE))
            out.append((2, ch, PE, [2.8 * g] * n, [0.3 * k for k in KE]))
    return out, body


def edge_r(bn, body, e, u):
    W, D, lat = EDGE_CK[e]
    X = unit((bn['rHip'] - bn['lHip']) if lat == 1 else (bn['rSh'] - bn['lSh']), (1, 0, 0))
    return ck_extent(lerp2(W, body['build']) * body['sizeK'], lerp2(D, body['build']) * body['sizeK'], X, u, lat)


def to_eye_g(eye, p, d):
    e = eye - p
    ln = float(np.linalg.norm(e))
    return p + e / ln * d if ln > d + 20 else p


def RP(p):  # game point -> preview room point
    return G(p) * RK


def strip_g(S, cam, pts, hw, k, col, minpx):
    fp.draw_strip(S, cam, [RP(p) for p in pts], [w * RK for w in hw], k, col, minpx=minpx)


def arc_g(S, cam, eye, a, b, n, amp, seed, w, k, col):
    """AiArc in game coordinates (its u axis across the view)"""
    d = b - a
    ln = float(np.linalg.norm(d))
    if ln < 0.5 or k < 0.004:
        return
    u = unit(np.cross(d, (a + b) / 2 - eye), (1, 0, 0))
    v = np.cross(d, u) / ln
    pts, hw = [], []
    for i in range(n):
        s = i / (n - 1)
        env = math.sin(math.pi * s)
        r1, r2 = (rnd01((seed + 2 * i) & M32) - 0.5) * 2, rnd01((seed + 2 * i + 1) & M32) - 0.5
        pts.append(a + d * s + (u * r1 + v * r2) * amp * ln * env)
        hw.append(w * (0.55 + 0.45 * env))
    strip_g(S, cam, pts, hw, [k] * n, col, 0.75)
    strip_g(S, cam, pts, [x * 3 for x in hw], [0.3 * k] * n, col, 1.5)


def splat_g(img, zb, cam, p, rad, col, a):
    splat(img, zb, cam, RP(p), rad * RK, col, a)


def aura333(S, img, zb, cam, eye, bn, body, c, now, look, ver='333'):
    """AiAura v3.33 (and ver '332': v3.32's corona + bloom and arcs at the stand-in radii, for the comparison)"""
    e50 = int(math.floor(now / 0.05))
    arc_col = HOT if look == 'th' else ARC
    glow = COL_SV if look == 'sv' else SHOCK
    nA, nT = 2 + int(8 * c + 0.5), int(4 * c + 0.2)
    nvk = NV_K if look == 'nv' else 1.0
    g = body['sizeK']
    for i in range(nA):
        h = ai_hash((e50 * 0x2545F491 + i * 977 + 0x3B) & M32)
        e = h % 15
        pa, pb = bn[HB[DP_EDGES[e][0]]], bn[HB[DP_EDGES[e][1]]]
        s0 = 0.5 * rnd01(h ^ 0x51)
        s1 = min(1.0, s0 + 0.4 + 0.45 * rnd01(h ^ 0x73))
        k = (0.45 + 0.55 * c) * (0.75 + 0.25 * rnd01(h ^ 0x99)) * nvk
        if rnd01(h ^ 0x1F) > 0.55 + 0.4 * c:
            continue
        a, b = pa + (pb - pa) * s0, pa + (pb - pa) * s1
        if ver == '333':
            a = to_eye_g(eye, a, edge_r(bn, body, e, unit(eye - a)) + 3 * g)
            b = to_eye_g(eye, b, edge_r(bn, body, e, unit(eye - b)) + 3 * g)
        else:
            a, b = to_eye_g(eye, a, DP_EDGE_R_V332[e] + 3), to_eye_g(eye, b, DP_EDGE_R_V332[e] + 3)
        if look == 'th':
            for q in range(3):
                splat_g(img, zb, cam, a + (b - a) * (0.2 + 0.3 * q + 0.1 * rnd01(h + q)), 1.6, HOT, 0.6 * k)
        else:
            arc_g(S, cam, eye, a, b, 6, 0.32, h, 0.9, k, arc_col)
    if look != 'th':
        for i in range(nT):
            h = ai_hash((e50 * 0x9E3779B1 + i * 0x85EBCA77 + 0x5D) & M32)
            e = h % 15
            pa, pb = bn[HB[DP_EDGES[e][0]]], bn[HB[DP_EDGES[e][1]]]
            s = rnd01(h ^ 0x21)
            side = 1.0 if (h >> 9) & 1 else -1.0
            ln = (14 + 26 * rnd01(h ^ 0x43)) * (g if ver == '333' else 1.0)
            p = pa + (pb - pa) * s
            w = unit(np.cross(pb - pa, p - eye), (1, 0, 0))
            R = edge_r(bn, body, e, w) if ver == '333' else DP_EDGE_R_V332[e]
            a = p + side * w * R
            tip = a + side * w * ln + np.array((0, -0.6 * rnd01(h ^ 0x65) * ln, 0))
            arc_g(S, cam, eye, a, tip, 5, 0.35, h, 0.7, 0.9 * c * nvk, arc_col)
            if (h >> 13) % 3 == 0:
                mid = (a + tip) / 2
                t2 = mid + np.array([(rnd01(h + 7 + q) - 0.5) * 0.8 * ln for q in range(3)])
                arc_g(S, cam, eye, mid, t2, 4, 0.3, h ^ 0xAB, 0.5, 0.6 * c * nvk, arc_col)
        if ver == '332':  # v3.32's corona (2 x 2 soft sprites per edge at the stand-in radius + 2) and the bloom
            for i in range(15):
                pa, pb = bn[HB[DP_EDGES[i][0]]], bn[HB[DP_EDGES[i][1]]]
                R = DP_EDGE_R_V332[i]
                for q in range(4):
                    s = 0.75 if q & 2 else 0.3
                    side = 1.0 if q & 1 else -1.0
                    p = pa + (pb - pa) * s
                    p = p + side * unit(np.cross(pb - pa, p - eye)) * (R + 2)
                    splat_g(img, zb, cam, p, (8 + R) * 0.45, glow, (0.10 + 0.18 * c) * flicker(now * 1.4, 60 + i * 4 + q) * nvk)
            for i, (bn_, br) in enumerate((('head', 30.0), ('chest', 38.0), ('pelvis', 34.0))):
                splat_g(img, zb, cam, to_eye_g(eye, bn[bn_], 25.0), br * 0.45, glow, (0.04 + 0.08 * c) * flicker(now, 90 + i) * nvk)
    e12 = int(math.floor(now / 0.12))
    h = ai_hash((e12 * 0x7F4A7C15 + 0x77) & M32)
    if c > 0.45 and (now / 0.12) % 1.0 < 0.6 and rnd01(h) < 0.6 * c:
        src = ('lFoot', 'rFoot', 'lHand', 'rHand')[(h >> 5) & 3]
        fy = max(bn['lFoot'][1], bn['rFoot'][1])
        ang, dist = 2 * math.pi * rnd01(h ^ 0x3C), 25 + 35 * rnd01(h ^ 0x5A)
        tip = np.array((bn[src][0] + math.cos(ang) * dist, fy, bn[src][2] + math.sin(ang) * dist))
        if look == 'th':
            for q in range(3):
                splat_g(img, zb, cam, bn[src] + (tip - bn[src]) * (0.2 + 0.3 * q), 1.6, HOT, 0.6 * c)
        else:
            arc_g(S, cam, eye, bn[src], tip, 7, 0.3, h ^ 0x11, 1.0, 0.9 * c * nvk, arc_col)
            splat_g(img, zb, cam, tip, 10 * 0.45, glow, 0.35 * c * nvk)
    rate = 22 * smooth(CHARGE_IDLE - 0.12, 1.0, c)  # shed sparks (a snapshot)
    for q in range(int(rate * 0.25 + 0.5)):
        h = ai_hash(5000 + q)
        e = h % 15
        p0 = bn[HB[DP_EDGES[e][0]]] + (bn[HB[DP_EDGES[e][1]]] - bn[HB[DP_EDGES[e][0]]]) * rnd01(h + 1)
        age, life = 0.25 * rnd01(h + 2), 0.15 + 0.15 * rnd01(h + 3)
        if age >= life:
            continue
        ang, sp = 2 * math.pi * rnd01(h + 4), 30 + 60 * rnd01(h + 5)
        p = p0 + np.array((math.cos(ang) * sp, (rnd01(h + 6) - 0.5) * 2 * sp, math.sin(ang) * sp)) * age
        splat_g(img, zb, cam, p, 1.8, HOT if look == 'th' else ARC, (0.7 if look == 'th' else 0.9) * (1 - age / life) ** 2)


def cloak_draw(S, img, zb, cam, ribs, c, now, look, body):
    nvk = NV_K if look == 'nv' else 1.0
    if look == 'th':  # white-hot points crawling along her outline
        edges = [r for r in ribs if r[0] == 1]
        e50 = int(math.floor(now / 0.05))
        for i in range(4 + int(10 * c + 0.5)):
            h = ai_hash((e50 * 0x632BE5AB + i * 0x9E3779B9) & M32)
            r = edges[h % len(edges)]
            splat_g(img, zb, cam, r[2][(h >> 8) % len(r[2])], 1.4 * body['sizeK'], HOT, (0.25 + 0.45 * c) * (0.7 + 0.3 * rnd01(h ^ 0x3D)))
        return
    for kind, ch, P, W, K in ribs:
        if max(K) <= 0.0:  # (v3.34: the outlines carry no light)
            continue
        col = COL_SV if look == 'sv' else CK_CORE if kind == 1 else SHOCK
        kk = [k * nvk * (0.8 if look == 'sv' else 1.0) for k in K]
        strip_g(S, cam, P, W, kk, col, 0.75 if kind == 1 else 1.5)


def body_segs(bn, body_name):
    """the stand-in body: capsules of the measured girth (preview room units)"""
    b = 0.0 if body_name == 'eileen' else 1.0
    segs = []
    for e, (i, j) in enumerate(DP_EDGES):
        W, D, lat = EDGE_CK[e]
        r = 0.5 * (lerp2(W, b) + lerp2(D, b)) * 0.92
        segs.append((RP(bn[HB[i]]), RP(bn[HB[j]]), r * RK))
    for s in ('l', 'r'):
        segs.append((RP(bn[s + 'Foot']), RP(bn[s + 'Toe'] + (bn[s + 'Toe'] - bn[s + 'Foot']) * 0.6), (7.0 if b < 0.5 else 9.5) * RK))
        d = unit(bn[s + 'Hand'] - bn[s + 'Elb'])
        segs.append((RP(bn[s + 'Hand']), RP(bn[s + 'Hand'] + d * 22), (5.0 if b < 0.5 else 8.0) * RK))
    up = unit(bn['head'] - bn['neck'])  # the head: an upright capsule (half width 19 / 20, crown 34 / 45 above the head bone)
    hr = 18.5 if b < 0.5 else 19.5
    segs.append((RP(bn['head'] - up * 4), RP(bn['head'] + up * ((34.0 if b < 0.5 else 45.0) - hr)), hr * RK))
    return segs


def render333(name, body='eileen', motion='stand', cam_name='ck34', look='normal', trail=None, t=0.0, ver='334', aura=True, fan=0.0,
              keep=None):
    """keep (v3.35 FX335): the effect budget's fade per copy (1 = drawn, 0 = thinned; None = all drawn)"""
    if fp.FOG_TEX is None:
        fp.FOG_TEX = fp.make_tex(128, 1, 4, fp.TEX_WARP)
        fp.GRIME_TEX = fp.make_tex(128, 7, 5, 0.0, base=3)
    cam = cam_real(cam_name)
    campos = np.array(CAMS_R[cam_name]['pos'])
    eye = G(campos)
    fp.NEAR0, fp.NEAR1 = 10.0, 45.0  # (the room is half scale: the game's 20 -> 90 u)
    rgb, zb = fp.render_scene(cam, fp.ROOM_BACK, 0.0, player=False)
    mo = MotionR(MOTIONS_R[motion], body)
    herY = mo.bones(t)
    bn = {k: G(v) for k, v in herY.items()}
    if look == 'th':
        rgb = (np.clip(fp._lum(rgb) * 1.8, 0, 1) ** 0.8 * fp.TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
    fp.draw_bones(cam, rgb, zb, body_segs(bn, body), (fp.TH_PLAYER,) * 3 if look == 'th' else BODY[body]['col'])
    img = rgb.reshape(cam.h, cam.w, 3).copy()
    zb2 = zb.reshape(cam.h, cam.w)
    if look == 'th':
        img = fp.filter_th(np.clip(img, 0, 1))
    if look == 'sv':
        img = fp.filter_sv(np.clip(img, 0, 1))
    S = dict(imgimg=img, zbimg=zb2)
    now = t + 10.0
    sp = MOTIONS_R[motion](t)[3]
    charge = CHARGE_IDLE + (1 - CHARGE_IDLE) * smooth(CHARGE_V0, CHARGE_V1, sp)  # (the steady charge at this speed)
    shown = 0
    if trail:  # the copies: evenly along her path, the path gate, the relative near fade; v3.33 no fan (fan > 0: v3.32's)
        n, span = TRAIL[trail]

        class _M:  # path_ring wants .fn
            fn = staticmethod(MOTIONS_R[motion])
        ts, pts, cum = path_ring(_M, t)
        tex = fake_tex(cam)
        her_p = herY['pelvis']
        f = _xz(her_p) - _xz(campos)
        F = f / (np.linalg.norm(f) + 1e-9)
        Rr = np.array((-F[1], F[0]))
        dvec = her_p - campos
        hd = float(np.linalg.norm(dvec))
        dirv = dvec / hd
        side = 1.0
        moments = [echo_at(ts, cum, t, span, k, n) for k in range(1, n + 1)]
        if fan > 0:
            tx, tz = MOTIONS_R[motion](moments[-1][0])[:2]
            v = np.array((tx, tz)) - _xz(her_p)
            lat_, along_ = float(v @ Rr), float(v @ F)
            if abs(lat_) > 4.0 and abs(lat_) > FAN_CLEAR * FAN_T * abs(along_):
                side = 1.0 if lat_ > 0 else -1.0

        def shift(p):
            v = _xz(p) - _xz(her_p)
            d = fan * fan_shift(float(v @ F), float(v @ Rr), side) if fan > 0 else 0.0
            return np.array((Rr[0] * d, 0.0, Rr[1] * d))
        for k, (tk, pk) in enumerate(moments, 1):
            eb = mo.bones(tk)
            chord = math.hypot(eb['pelvis'][0] - her_p[0], eb['pelvis'][2] - her_p[2])
            sh = shift(eb['pelvis'])
            ebs = {kk: vv + sh for kk, vv in eb.items()}
            nr = near_k(float((ebs['pelvis'] - campos) @ dirv), hd)
            gt = gate(pk, chord)
            u = (k - 1) / (n - 1) if n > 1 else 0.0
            kk_ = 1.0 if keep is None else keep[k - 1]  # v3.35 FX335: the budget's fade
            gk = G0 * fall(k, n) * gt * nr * flicker(now, k) * kk_
            if gk < 0.004:
                continue
            shown += gk / G0 >= 0.05
            m, z = body_mask(cam, None, edges=body_segs({kk: G(vv) for kk, vv in ebs.items()}, body))
            vis = (z < zb2 + 0.5).astype(np.float32) * m
            if look == 'th':
                hcol = np.array([np.interp(min(1.0, (0.8 - 0.55 * u) * 0.9 + 0.1), [hh[0] for hh in fp.HEAT], [hh[1][q] for hh in fp.HEAT]) for q in range(3)])
                a = (vis * 0.6 * fall(k, n) * gt * nr * kk_)[..., None]
                img *= (1 - a)
                img += hcol * a
            else:
                col = np.array(COL_SV) if look == 'sv' else np.array(COL_NEAR) + (np.array(COL_TAIL) - np.array(COL_NEAR)) * u
                img += (vis * (TEX_K * tex + FILL_K) * gk * (NV_K if look == 'nv' else 0.8 if look == 'sv' else 1.0))[..., None] * col
        if look != 'th':  # the streaks (head, hands, feet) along the same path
            J = STREAK_PTS - 1
            P = {bb: [G(herY[bb])] for bb in STREAK_BONES}
            K = [0.0]
            for j in range(1, J + 1):
                tj, pj = echo_at(ts, cum, t, span, j, J)
                bj = mo.bones(tj)
                chord = math.hypot(bj['pelvis'][0] - her_p[0], bj['pelvis'][2] - her_p[2])
                sh = shift(bj['pelvis'])
                K.append(STREAK_K * gate(pj, chord) * (1 - j / (J + 1)) ** 1.2 * near_k(float((bj['pelvis'] + sh - campos) @ dirv), hd))
                for bb in STREAK_BONES:
                    P[bb].append(G(bj[bb] + sh))
            e50 = int(math.floor(now / 0.05))
            for si, bb in enumerate(STREAK_BONES):
                ptsS = [p + (np.array([(rnd01((ai_hash((e50 * 0x6A09E667 + si * 131 + j * 7) & M32) + q) & M32) - 0.5) * 3 for q in range(3)]) if j else 0)
                        for j, p in enumerate(P[bb])]
                kk = [k * (0.8 if si == 0 else 1.0 if si < 3 else 0.6) * (NV_K if look == 'nv' else 1.0) for k in K]
                col = COL_SV if look == 'sv' else COL_NEAR
                strip_g(S, cam, ptsS, [0.8] * len(ptsS), kk, col, 0.75)
                strip_g(S, cam, ptsS, [3.0] * len(ptsS), [0.35 * k for k in kk], col, 1.5)
    ribs, bod = cloak_build(bn, eye, charge, now, ver)
    if aura:
        # (the stand-in body's depth is flat per capsule: the cloak and arcs are tested with a 24 u margin, as her real
        # per-pixel surface would let them through; most game interiors overwrite the depth anyway)
        S['zbimg'] = zb2 + 12.0
        if ver in ('333', '334') or (ver == '335' and look == 'th'):  # v3.35 FX335: no cloak but Thermal's hot points
            cloak_draw(S, img, S['zbimg'], cam, ribs, charge, now, look, bod)
        aura333(S, img, S['zbimg'], cam, eye, bn, bod, charge, now, look, '333' if ver in ('334', '335') else ver)
        S['zbimg'] = zb2
    if look == 'nv':
        img = fp.filter_nv(np.clip(img, 0, 1))
    im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
    os.makedirs(OUT, exist_ok=True)
    im.save(os.path.join(OUT, 'ai_%s.png' % name))
    return shown, charge, bod, sum(1 for r in ribs if max(r[4]) > 0.0) if ver in ('333', '334') else 0  # (v3.35: 0 strips)


V333_SHEETS = [
    ('ai333_cloak_sheet.png', 3, [
        ('c_e_stand', 'Eileen standing: faint cloak (charge 0.30)', dict(body='eileen', motion='stand', ver='333')),
        ('c_e_walk', 'Eileen walking (175 u/s): stronger (charge 0.67)', dict(body='eileen', motion='walk', ver='333')),
        ('c_e_run', 'Eileen running (663 u/s): strong (charge 1.0)', dict(body='eileen', motion='run', ver='333')),
        ('c_h_stand', 'Henry standing: the cloak sized to his body', dict(body='henry', motion='stand', ver='333')),
        ('c_h_walk', 'Henry walking', dict(body='henry', motion='walk', ver='333')),
        ('c_h_run', 'Henry running', dict(body='henry', motion='run', ver='333')),
    ]),
    ('ai333_looks_sheet.png', 3, [
        ('l_e_th', 'Eileen running, Thermal: hot points on her outline', dict(body='eileen', motion='run', look='th', trail='Medium', ver='333')),
        ('l_e_sv', 'Eileen running, Spirit Vision', dict(body='eileen', motion='run', look='sv', trail='Medium', ver='333')),
        ('l_e_nv', 'Eileen running, night vision (x 0.7)', dict(body='eileen', motion='run', look='nv', trail='Medium', ver='333')),
        ('l_h_th', 'Henry running, Thermal', dict(body='henry', motion='run', look='th', trail='Medium', ver='333')),
        ('l_h_sv', 'Henry running, Spirit Vision', dict(body='henry', motion='run', look='sv', trail='Medium', ver='333')),
        ('l_h_nv', 'Henry running, night vision', dict(body='henry', motion='run', look='nv', trail='Medium', ver='333')),
    ]),
    ('ai333_inline_sheet.png', 2, [
        ('i_fan_m', 'v3.32: running away, camera behind, Medium: the fan', dict(body='eileen', motion='run_away', cam_name='behind', trail='Medium', fan=1.0)),
        ('i_line_m', 'v3.33: the same frame: copies in line', dict(body='eileen', motion='run_away', cam_name='behind', trail='Medium', ver='333')),
        ('i_fan_l', 'v3.32: Long (8 copies), behind: the fan', dict(body='eileen', motion='run_away', cam_name='behind', trail='Long', fan=1.0)),
        ('i_line_l', 'v3.33: Long, behind: in line', dict(body='eileen', motion='run_away', cam_name='behind', trail='Long', ver='333')),
        ('i_line_h', 'v3.33: Henry, Long, behind: in line', dict(body='henry', motion='run_away', cam_name='behind', trail='Long', ver='333')),
        ('i_side_e', 'v3.33: Eileen, Long, side camera (as v3.32)', dict(body='eileen', motion='run_side', cam_name='side', trail='Long', ver='333')),
    ]),
    ('ai333_orbs_vs_cloak_sheet.png', 2, [
        ('o_e_stand_332', 'v3.32 standing: the corona / bloom "orbs"', dict(body='eileen', motion='stand', ver='332')),
        ('o_e_stand_333', 'v3.33 standing: one skin-tight cloak', dict(body='eileen', motion='stand', ver='333')),
        ('o_e_run_332', 'v3.32 running: orbs, arcs inside her body', dict(body='eileen', motion='run', ver='332')),
        ('o_e_run_333', 'v3.33 running: the cloak, arcs on it', dict(body='eileen', motion='run', ver='333')),
        ('o_h_run_332', 'v3.32 Henry running (stand-in radii)', dict(body='henry', motion='run', ver='332')),
        ('o_h_run_333', 'v3.33 Henry running (sized to him)', dict(body='henry', motion='run', ver='333')),
    ]),
]

# v3.34 FX334: v3.33 (edge lines on her outline) beside v3.34 (soft edges), standing / walking / running, both bodies; close
# up; the filters
_MOT = (('stand', 'standing (charge 0.30)'), ('walk', 'walking (0.67)'), ('run', 'running (1.0)'))
V334_SHEETS = [
    ('ai334_cloak_before_after.png', 3,
     [('b_%s_%s_%s' % (bd[0], m, v), '%s %s %s: %s' % (v_, bd.capitalize(), mm, 'edge lines' if v == '333' else 'soft edges'),
       dict(body=bd, motion=m, ver=v))
      for bd in ('eileen', 'henry') for v, v_ in (('333', 'v3.33'), ('334', 'v3.34')) for m, mm in _MOT]),
    ('ai334_cloak_close.png', 2,
     [('k_%s_%s_%s' % (bd[0], m, v), '%s %s %s, close: %s' % (v_, bd.capitalize(), m, 'outline lines' if v == '333' else 'fades out at her edge'),
       dict(body=bd, motion=m, ver=v, cam_name='close'))
      for bd, m in (('eileen', 'stand'), ('eileen', 'run'), ('henry', 'run')) for v, v_ in (('333', 'v3.33'), ('334', 'v3.34'))]),
    ('ai334_cloak_looks.png', 4,
     [('f_%s_%s_%s' % (bd[0], lk, v), '%s %s running, %s' % (v_, bd.capitalize(), nm), dict(body=bd, motion='run', look=lk, ver=v, trail='Medium'))
      for bd in ('eileen', 'henry') for lk, v, v_, nm in (('sv', '333', 'v3.33', 'Spirit Vision'), ('sv', '334', 'v3.34', 'Spirit Vision'),
                                                           ('nv', '334', 'v3.34', 'night vision'), ('th', '334', 'v3.34', 'Thermal (as v3.33)'))]),
]


# v3.35 FX335: v3.34 (the soft blue cloak) beside v3.35 (no glow on her: arcs and sparks only), both bodies; close up; the
# looks with the trail; the effect budget's copies (Long 8 -> 5 -> 3, every other copy fading, the trail keeps its length)
def _keep(n, m, fade=1.0):
    """budget_sim.h BgAiKeep: copy k kept with m of n (the nearest and the tail always); fade: the dropped ones' value"""
    out = []
    for k in range(1, n + 1):
        kept = m >= n or n <= 1 or any(int(math.floor(1 + j * (n - 1) / (m - 1) + 0.5)) == k for j in range(m)) if m > 1 else k == 1
        out.append(1.0 if kept else fade)
    return out


V335_SHEETS = [
    ('ai335_glow_before_after.png', 3,
     [('g_%s_%s_%s' % (bd[0], m, v), '%s %s %s: %s' % (v_, bd.capitalize(), mm, 'soft blue cloak' if v == '334' else 'no glow, arcs only'),
       dict(body=bd, motion=m, ver=v))
      for bd in ('eileen', 'henry') for v, v_ in (('334', 'v3.34'), ('335', 'v3.35')) for m, mm in _MOT]),
    ('ai335_close.png', 2,
     [('c5_%s_%s_%s' % (bd[0], m, v), '%s %s %s, close: %s' % (v_, bd.capitalize(), m, 'blue tint on her' if v == '334' else 'her own colours'),
       dict(body=bd, motion=m, ver=v, cam_name='close'))
      for bd, m in (('eileen', 'stand'), ('eileen', 'run'), ('henry', 'run')) for v, v_ in (('334', 'v3.34'), ('335', 'v3.35'))]),
    ('ai335_looks.png', 4,
     [('l5_%s_%s' % (bd[0], lk), 'v3.35 %s running, %s, Medium trail' % (bd.capitalize(), nm), dict(body=bd, motion='run', look=lk, ver='335', trail='Medium'))
      for bd in ('eileen', 'henry') for lk, nm in (('normal', 'normal'), ('sv', 'Spirit Vision'), ('nv', 'night vision'), ('th', 'Thermal hot points'))]),
    ('ai335_budget_trail.png', 4,
     [('b5_%s_%s' % (bd[0], tag), '%s Long, %s' % (bd.capitalize(), lab), dict(body=bd, motion='run_side', cam_name='side', trail='Long', ver='335', keep=kp))
      for bd in ('eileen', 'henry') for tag, lab, kp in (('l0', 'budget level 0: 8 copies', None),
                                                       ('l5f', 'level 5, mid-fade: 2 4 7 at 50 %', _keep(8, 5, 0.5)),
                                                       ('l5', 'level 5: 5 copies 1 3 5 6 8', _keep(8, 5, 0.0)),
                                                       ('l7', 'level 7: 3 copies 1 5 8', _keep(8, 3, 0.0)))]),
]


def main335(only=None):
    log = []
    for out, cols, frames in V335_SHEETS:
        if only and only not in out:
            continue
        for n, lab, kw in frames:
            shown, ch, bod, nr = render333(n, **kw)
            line = 'rendered %-16s %s: charge %.2f, build %.2f, radii x%.2f, copies shown %d, cloak strips drawn %d' % (
                n, kw.get('ver'), ch, bod['build'], bod['sizeK'], shown, nr)
            print(line)
            log.append(line)
        sheet([f[0] for f in frames], [f[1] for f in frames], cols=cols, tw=640 if cols == 2 else 560 if cols == 3 else 480, out=out)
        print('sheet ->', os.path.join(OUT, out))
    with open(os.path.join(OUT, 'render_log_335.txt'), 'w') as f:
        f.write('\n'.join(log) + '\n')


def main334(only=None):
    log = []
    for out, cols, frames in V334_SHEETS:
        if only and only not in out:
            continue
        for n, lab, kw in frames:
            shown, ch, bod, nr = render333(n, **kw)
            line = 'rendered %-16s %s: charge %.2f, build %.2f, radii x%.2f, cloak strips drawn %d' % (n, kw.get('ver'), ch, bod['build'], bod['sizeK'], nr)
            print(line)
            log.append(line)
        sheet([f[0] for f in frames], [f[1] for f in frames], cols=cols, tw=640 if cols == 2 else 560 if cols == 3 else 480, out=out)
        print('sheet ->', os.path.join(OUT, out))
    with open(os.path.join(OUT, 'render_log_334.txt'), 'w') as f:
        f.write('\n'.join(log) + '\n')


def main333(only=None):
    log = []
    for out, cols, frames in V333_SHEETS:
        if only and only not in out:
            continue
        for n, lab, kw in frames:
            shown, ch, bod, nr = render333(n, **kw)
            line = 'rendered %-14s copies shown %d, charge %.2f, build %.2f, radii x%.2f, cloak ribbons %d' % (n, shown, ch, bod['build'], bod['sizeK'], nr)
            print(line)
            log.append(line)
        sheet([f[0] for f in frames], [f[1] for f in frames], cols=cols, tw=640 if cols == 2 else 560, out=out)
        print('sheet ->', os.path.join(OUT, out))
    with open(os.path.join(OUT, 'render_log_333.txt'), 'a') as f:
        f.write('\n'.join(log) + '\n')


def main():
    if V332_SHEETS_ONLY:
        main332()
        return
    if V333_SHEETS_ONLY:  # v3.34 FX334: the v3.33 sheets
        main333(ONLY333)
        return
    if V334_SHEETS_ONLY:  # v3.35 FX335: the v3.34 sheets
        main334(ONLY333)
        return
    if not ONLY_LENGTHS and not OLD_SHEETS:  # v3.35 FX335: the default
        main335(ONLY333)
        return
    sets = [(LENGTH_FRAMES, 3, 'ai_lengths_sheet.png')] + ([] if ONLY_LENGTHS else [(FRAMES, 2, 'ai_sheet.png')])
    for frames, cols, out in sets:
        for n, lab, kw in frames:
            render(n, **kw)
            print('rendered', n)
        sheet([f[0] for f in frames], [f[1] for f in frames], cols=cols, tw=640 if cols == 2 else 560, out=out)
        print('sheet ->', os.path.join(OUT, out))


if __name__ == '__main__':
    main()
