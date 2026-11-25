"""v3.30 ECHO330: offline preview sheets of the Echo Doubles' shadow-soldier look (w1_echo/DESIGN.md).
v3.31 ECHO331 (staging/v331_feedback/w1_echo/DESIGN.md): one large black flame per shadow (with a faint violet edge), the
edge shell off (EchoRim=0, the game's default now), the eyes on the head bone's axes (echo_eyes.png: v3.30's facing frame
beside v3.31's), the busy moments (echo_busy.png). The sprite dump passes the head bone's axes (rows 0-2 of bone 11).

The bodies: the real player models (Playable Eileen's casual body on the henry01 rig; vanilla Henry from the stock bank,
entry 170, 30 bones) posed from the banks with the runtime skinning rule (vertex x bone world matrix,
staging/casual_port/skin.py); each shadow is that pose x its own transform D, drawn as echo.inc's passes do: EP_SOLID
(opaque near-black ink, V = (0.012 + 0.07 L) x tint, its own depth), the edge shell (each bone's 3 x 3 part x 1.035, its
back faces, additive violet x 0.55). Thermal: the cold silhouette at alpha 0.75 after the heat palette; Spirit Vision:
the additive violet glow after the filter (fog_preview's filter_nv / filter_th / filter_sv stand in for the game's).
The sprites are NOT ported: test\\echo_sim_test.exe --look IN OUT runs echo_sim.h's EchoLookBuild on the shadows' 16 key
bones, facing and smoke events written here, and this tool only rasterises the list as DpSlDraw does: camera-facing quads
(flames stretched base -> tip, their foot 20 % up), premultiplied over (far first) then additive, depth tested against
the scene and the bodies, the ground disc flat on the floor. The flame cells are the real spirit fire atlas (test\\
sf_sim_test --dump); the FXX_SMOKE / SV atlas cells are procedural stand-ins (soft disc, billow puff, crest, orb).
The room is a stand-in (a lit wall, a dark room); the enemy is a grey capsule stand-in. No game art.

v3.32 ECHO332 (staging/v332_feedback/w1_echo/DESIGN.md): the flame coat (black tongues along the body and limbs), fine
purple wisps, softer eyes; calm (1.5 x the slot distance, 45 % opacity: the body through its depth prepass, the sprites
by EchoCalmOp) vs striking (solid, 1 x): echo_calm.png from over her shoulder. The DPS_SOFT cell is the game's profile now
((1 - smoothstep(0.2, 1, r)) (1 - smoothstep(0.8, 1, r)), deathpresence_draw.inc DpSmokeTexel); the wisps get no floor
fade (pad bit 0). v3.31 tiles: set ECHO331_EXE to a v3.31 echo_sim_test.exe (its --look format: 16 fields in, 15 out).

v3.33 ECHO333 (staging/v333_feedback/w1_echo/DESIGN.md): the 1-of-3 case (echo_1of3.png: v3.32 at budget level 2: the 2nd /
3rd calm body without its prepass blends every surface of her model, drawn here as every triangle in front of the scene
blended in order (her model two-sided, as Playable Eileen's materials); v3.33: all three with the prepass) and the vision
looks (echo_vision_eileen.png / echo_vision_henry.png: Thermal, Spirit Vision, Night Vision, Night Vision Mono, striking and
calm; v3.32 beside v3.33 when ECHO332_EXE names a v3.32 echo_sim_test.exe). Thermal's world is the game's cold ramp
(screenfx.inc THERM_*: navy, blue, azure, pale cyan) and her body the heat ramp (TH_*), no longer fog_preview's hot
palette; night vision mono a grey stand-in.

v3.34 FX334 (staging/v334_feedback/w2_fx/DESIGN.md; the default ver '334'): Spirit Vision's dark specters (a near-black cold
ink body over its prepass, 62 % striking .. 45 % calm, dark smoke coat and wisps, a faint cold rim): echo_specter_eileen.png /
echo_specter_henry.png, v3.33's pale look beside them when ECHO333_EXE names a v3.33 echo_sim_test.exe (the same --look
format).

v3.35 FX335 (staging/v335_feedback/w2_fx/DESIGN.md): the effect budget's Echo steps (echo_budget.png): a shadow's body fades
into its smoke (the shadow dict's `govk`: the body's alpha x govk, the sprites unchanged), the light look on the 2nd / 3rd
(`lite`), the ground discs gone at level 6 (render's `noground`); Eileen striking, Henry calm.

usage: py -3.10 tools/echo_preview.py OUTDIR ATLAS_PGM [sheets]   (writes OUTDIR/echo_*.png; sheets: idle,eyes,calm,copy,
       cast,teleport,filters,henry,busy,1of3,vision,specter,budget)
"""
import math
import os
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SANDBOX = os.path.dirname(HERE)
CASUAL = os.environ.get('SH4PE_CASUAL_PORT', 'casual_port')  # Playable Eileen's casual_port tools (not in this repository)
sys.path.insert(0, CASUAL)
sys.path.insert(0, os.environ.get('SH4_ASSET_TOOLS', 'AssetTools'))  # sh4bin / sh4anm / sh4mdl (not in this repository)
sys.path.insert(0, HERE)
import sh4bin, sh4anm, sh4mdl, skin  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location('casual_pose_render', os.path.join(CASUAL, 'pose_render.py'))
_pr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_pr)
pose = _pr.pose
import fog_preview as FP  # noqa: E402  (the Death Presence preview's NV / Thermal / Spirit Vision filters)

EXE = os.path.join(SANDBOX, 'test', 'out', 'echo_sim_test.exe')
EXE331 = os.environ.get('ECHO331_EXE', '')  # v3.32: the v3.31 look for the comparison tiles
EXE332 = os.environ.get('ECHO332_EXE', '')  # v3.33: the v3.32 look (the same --look format) for the comparison tiles
EXE333 = os.environ.get('ECHO333_EXE', '')  # v3.34 FX334: the v3.33 look (the same --look format) for the comparison tiles
SV_SPECTER = np.array([0.55, 0.62, 1.0])   # v3.34 FX334: echo.inc kSpecter (x (0.03 + 0.05 luma))
MDL = os.path.join(CASUAL, 'eink_casual_30.mdl')
BANK = os.environ.get('SH4PE_BANK', 'henry01_eileen.bin')  # Playable Eileen's henry01.bin (not in this repository)
STOCK = os.environ.get('SH4_STOCK_BANK', 'henry01.bin')  # the game's own henry01.bin from your install (not in this repository)
IDLE, SWING = 0, 83
W, H, SS = 420, 500, 2
TW, TH = W * SS, H * SS
LIGHT = np.array([0.55, 0.20, 1.0])        # kEchoLight[Violet]
INK = np.array([0.583, 0.417, 1.0])        # kEchoInk[Violet]
RIM = LIGHT * 0.55
KIND_L = {2: 0.35, 3: 0.65, 4: 0.30}       # a flat texture luma per part kind (cloth / skin / other)
EB = [11, 10, 9, 8, 18, 13, 19, 14, 20, 15, 5, 2, 6, 3, 7, 4]  # kEchoBone
ESK_SOFT, ESK_ORB, ESK_FLAME, ESK_SMOKE0, ESK_SMOKE1, ESK_CREST, ESK_GROUND, ESK_WISP = range(8)  # v3.32: + ESK_WISP


def M4(l):
    return np.array(l, dtype=np.float64).reshape(4, 4)


def shift(o):
    m = np.eye(4)
    m[3, :3] = o
    return m


def rot_y(a):  # echo_sim.h EchoRotY (row vectors); turning by a takes facing angle phi to phi - a
    c, s = math.cos(a), math.sin(a)
    m = np.eye(4)
    m[0, 0], m[0, 2], m[2, 0], m[2, 2] = c, s, -s, c
    return m


def place(mats, root, yaw, dest):
    """EchoXformShadow: her root -> dest, her facing turned by yaw (about her root)."""
    D = shift(-np.asarray(root)) @ rot_y(yaw) @ shift(np.asarray(dest))
    return [M @ D for M in mats], D


def skinned(m, anim):
    return [t for p in m.parts for t in skin.skin_part(m, p, [list(M.reshape(16)) for M in anim])]


class Cam:
    def __init__(self, eye, at, fov=40.0):
        self.eye = np.array(eye, float)
        f = np.array(at, float) - self.eye
        f /= np.linalg.norm(f)
        up = np.array([0.0, -1.0, 0.0])  # -Y is up
        r = np.cross(f, up)
        r /= np.linalg.norm(r)
        u = np.cross(r, f)
        self.f, self.r, self.u = f, r, u
        self.k = 0.5 * TH / math.tan(math.radians(fov) / 2)

    def proj(self, p):
        d = np.asarray(p, float) - self.eye
        z = d @ self.f
        return np.stack([TW / 2 + self.k * (d @ self.r) / z, TH / 2 - self.k * (d @ self.u) / z, z], -1)

    def rays(self):
        ys, xs = np.mgrid[0:TH, 0:TW].astype(np.float64) + 0.5
        d = (self.f[None, None, :] + ((xs - TW / 2) / self.k)[..., None] * self.r + (-(ys - TH / 2) / self.k)[..., None] * self.u)
        return d  # z along f = 1 per unit of d


def raster(cam, tris, fn):
    for a, b, c, kind in tris:
        P = cam.proj(np.array([a, b, c]))
        if (P[:, 2] < 5).any():
            continue
        x0, x1 = int(max(0, P[:, 0].min())), int(min(TW - 1, P[:, 0].max() + 1))
        y0, y1 = int(max(0, P[:, 1].min())), int(min(TH - 1, P[:, 1].max() + 1))
        if x1 < x0 or y1 < y0:
            continue
        (ax, ay, az), (bx, by, bz), (qx, qy, qz) = P
        den = (by - qy) * (ax - qx) + (qx - bx) * (ay - qy)
        if abs(den) < 1e-9:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l1 = ((by - qy) * (xs - qx) + (qx - bx) * (ys - qy)) / den
        l2 = ((qy - ay) * (xs - qx) + (ax - qx) * (ys - qy)) / den
        l3 = 1 - l1 - l2
        m = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not m.any():
            continue
        z = l1 * az + l2 * bz + l3 * qz
        rr, cc = np.nonzero(m)
        fn(rr + y0, cc + x0, z[m], (np.array(a), np.array(b), np.array(c), kind))


def lambert(tri):
    a, b, c, _ = tri
    n = np.cross(b - a, c - a)
    n /= np.linalg.norm(n) + 1e-9
    return 0.35 + 0.65 * abs(n @ np.array([0.3, -0.8, 0.5]))


def back_faces(cam, tris, mats, parents):
    """The shell triangles facing away from the camera, judged by their outward direction from the nearest bone segment
    (what a correct per-material cull leaves)."""
    pos = np.array([M[3, :3] for M in mats])
    seg = [(pos[parents[b]], pos[b]) for b in range(len(parents)) if 0 <= parents[b] < len(parents) and b < 25]
    A = np.array([a for a, _ in seg])
    B = np.array([b for _, b in seg])
    AB = B - A
    L2 = np.maximum((AB * AB).sum(1), 1e-6)
    out = []
    for t in tris:
        c = (np.array(t[0]) + np.array(t[1]) + np.array(t[2])) / 3.0
        u = np.clip(((c - A) * AB).sum(1) / L2, 0, 1)
        q = A + AB * u[:, None]
        o = c - q[int(np.argmin(((c - q) ** 2).sum(1)))]
        if np.dot(o, cam.eye - c) < 0:
            out.append(t)
    return out


# ---------------------------------------------------------------- the stand-in room
def room(cam, kind, floor_y, wall_z):
    d = cam.rays()
    e = cam.eye
    img = np.zeros((TH, TW, 3))
    zb = np.full((TH, TW), 1e9)
    with np.errstate(divide='ignore', invalid='ignore'):
        tf = (floor_y - e[1]) / d[..., 1]
        tw = (wall_z - e[2]) / d[..., 2]
    tf = np.where(tf > 0, tf, np.inf)
    tw = np.where(tw > 0, tw, np.inf)
    floor = tf < tw
    t = np.where(floor, tf, tw)
    P = e + d * t[..., None]
    zb = np.where(np.isfinite(t), t, 1e9)
    lx = P[..., 0] / 400.0
    if kind == 'lit':  # a lit plaster wall under a lamp, a worn floor
        lamp = np.exp(-((P[..., 0] - 60) ** 2 + (P[..., 1] + 260) ** 2) / (2 * 380.0 ** 2))
        wall = np.array([0.50, 0.46, 0.40]) * (0.55 + 0.75 * lamp)[..., None]
        wall *= (0.94 + 0.06 * np.sin(lx * 9 + np.sin(P[..., 1] / 37.0)))[..., None]
        fl = np.exp(-((P[..., 0] - 60) ** 2 + (P[..., 2] - wall_z + 300) ** 2) / (2 * 520.0 ** 2))
        flc = np.array([0.30, 0.27, 0.24]) * (0.45 + 0.85 * fl)[..., None]
        flc *= (0.93 + 0.07 * np.sign(np.sin(P[..., 0] / 45.0)))[..., None]
    else:  # a dark room: a faint cold light from one side
        g = np.clip(0.5 + P[..., 0] / 1400.0, 0, 1)
        wall = np.array([0.040, 0.042, 0.055]) * (0.7 + 0.6 * g)[..., None]
        flc = np.array([0.028, 0.028, 0.034]) * (0.7 + 0.6 * g)[..., None]
    img = np.where(floor[..., None], flc, wall)
    return np.clip(img, 0, 1), zb


# ---------------------------------------------------------------- bodies
def draw_her(F, cam, tris, look):
    img, zb = F

    def px(r, c, z, t):
        keep = z < zb[r, c]
        r, c, z = r[keep], c[keep], z[keep]
        zb[r, c] = z
        s = lambert(t)
        base = {2: (0.42, 0.45, 0.58), 3: (0.80, 0.66, 0.58)}.get(t[3], (0.55, 0.50, 0.48))
        img[r, c] = np.array(base) * s * (0.55 if look == 'dark' else 1.0)
    raster(cam, tris, px)


def draw_enemy(F, cam, base, h=260.0, lean=0.0, dark=False):
    """a grey capsule stand-in (torso, head, arms): no game art"""
    img, zb = F
    parts = [((0, -60, 0), (0 + lean * 30, -h + 30, 0), 26), ((lean * 40, -h + 10, 0), (lean * 40, -h - 18, 4), 17),
             ((-24, -h + 40, 0), (-40, -h + 120, 18), 9), ((24, -h + 40, 0), (40, -h + 120, 18), 9),
             ((-12, 0, 0), (-12, -70, 0), 11), ((12, 0, 0), (12, -70, 0), 11)]
    col = np.array([0.30, 0.27, 0.25]) * (0.45 if dark else 1.0)
    for a, b, rad in parts:
        a = np.array(a, float) + base
        b = np.array(b, float) + base
        P = cam.proj(np.array([a, b]))
        if (P[:, 2] < 5).any():
            continue
        rp = cam.k * rad / P[:, 2].mean()
        x0, x1 = int(max(0, P[:, 0].min() - rp)), int(min(TW - 1, P[:, 0].max() + rp))
        y0, y1 = int(max(0, P[:, 1].min() - rp)), int(min(TH - 1, P[:, 1].max() + rp))
        if x1 <= x0 or y1 <= y0:
            continue
        ys, xs = np.mgrid[y0:y1, x0:x1] + 0.5
        ab = P[1, :2] - P[0, :2]
        L2 = max(ab @ ab, 1e-6)
        u = np.clip(((xs - P[0, 0]) * ab[0] + (ys - P[0, 1]) * ab[1]) / L2, 0, 1)
        dx, dy = xs - (P[0, 0] + ab[0] * u), ys - (P[0, 1] + ab[1] * u)
        dd = np.sqrt(dx * dx + dy * dy) / rp
        m = dd < 1
        z = P[0, 2] + (P[1, 2] - P[0, 2]) * u - rad * np.sqrt(np.clip(1 - dd * dd, 0, 1))
        keep = m & (z < zb[y0:y1, x0:x1])
        shade = (0.55 + 0.45 * np.sqrt(np.clip(1 - dd * dd, 0, 1)) * (1 - 0.4 * dx / rp))[..., None] * col
        img[y0:y1, x0:x1][keep] = shade[keep]
        zb[y0:y1, x0:x1][keep] = z[keep]


def pale(L, k, m):
    """echo_sim.h EchoPale: the light colour toward white by k, x m"""
    return np.clip((np.asarray(L) + (1 - np.asarray(L)) * k) * m, 0, 1)


def body_op(look, k, ver='334'):
    """echo_sim.h EchoBodyOp (v3.33): the normal look calm .. solid (45 % .. 1); a vision look 50 % striking .. 35 % calm;
    v3.32: EchoCalmOp in every look"""
    k = min(1.0, max(0.0, k))
    e = k * k * (3 - 2 * k)
    if look in ('normal', 'dark') or ver == '332':
        return 1.0 - 0.55 * e
    if look == 'sv' and ver == '334':  # v3.34 FX334: the dark specters 62 % .. 45 %
        return 0.62 - 0.17 * e
    return 0.50 - 0.15 * e


def draw_shadow(F, cam, tris, shell, look, alpha=1.0, ver='334', prepass=True):
    """echo.inc EchoBodies: normal / nv: EP_SOLID (or EP_DEPTH + EP_INK when fading), then EP_RIM; thermal: EP_DEPTH +
    EP_THERM; sv: EP_DEPTH + EP_GLOW. v3.33: prepass False = v3.32's budget level 2 on the 2nd / 3rd shadow: every
    surface in front of the scene blended in order (no culling: her two-sided model)"""
    img, zb = F
    zd = np.full(zb.shape, 1e9)
    if not prepass and alpha < 0.995:
        def blend(r, c, z, t):
            keep = z <= zb[r, c]
            r, c = r[keep], c[keep]
            L = KIND_L.get(t[3], 0.4)
            col = np.clip(INK * (0.012 + 0.07 * L) * (1 + 0.35 * (lambert(t) - 0.6)), 0, 1)
            img[r, c] = img[r, c] * (1 - alpha) + col * alpha
        raster(cam, tris, blend)
        return

    def pre(r, c, z, t):  # nearest surface of the shadow, where it is in front of the scene
        keep = (z <= zb[r, c]) & (z < zd[r, c])
        zd[r[keep], c[keep]] = z[keep]
    raster(cam, tris, pre)
    lay = np.zeros(img.shape)
    hit = np.zeros(zb.shape, bool)

    def col(r, c, z, t):
        keep = np.abs(z - zd[r, c]) < 1e-6
        r, c = r[keep], c[keep]
        L = KIND_L.get(t[3], 0.4)
        if look == 'sv' and ver == '332':
            gc = 0.45 + 0.55 * LIGHT
            lay[r, c] = np.clip(2 * L * gc * 0.45 * 0.5 + gc * 0.08, 0, 1)
        elif look == 'sv' and ver == '334':  # v3.34 FX334: EP_SPECTER, near-black cold ink (0.03 + 0.05 L) x kSpecter, blended
            lay[r, c] = np.clip(SV_SPECTER * (0.03 + 0.05 * L), 0, 1)
        elif look == 'sv':  # v3.33: pale spectral (EchoPale(light, 0.55, 1), tex 0.40, floor 0.10)
            gc = pale(LIGHT, 0.55, 1.0)
            lay[r, c] = np.clip(2 * L * gc * 0.40 * 0.5 + gc * 0.10, 0, 1)
        elif look == 'thermal' and ver == '332':
            lay[r, c] = np.clip(np.array([0.25, 0.35, 1.0]) * (0.06 + 0.10 * L), 0, 1)
        elif look == 'thermal':  # v3.33: deep indigo, colder than the navy floor: (0.15, 0.25, 1) x (0.05 + 0.10 L)
            lay[r, c] = np.clip(np.array([0.15, 0.25, 1.0]) * (0.05 + 0.10 * L), 0, 1)
        else:
            lay[r, c] = np.clip(INK * (0.012 + 0.07 * L) * (1 + 0.35 * (lambert(t) - 0.6)), 0, 1)
        hit[r, c] = True
    raster(cam, tris, col)
    a = alpha * (0.75 if (look == 'thermal' and ver == '332') else 1.0)
    if look == 'sv' and ver != '334':  # (v3.33 / v3.32: additive glow; v3.34: blended ink)
        img[hit] = np.clip(img[hit] + lay[hit] * alpha, 0, 1)
    else:
        img[hit] = img[hit] * (1 - a) + lay[hit] * a
    if alpha >= 0.995 or look in ('thermal', 'sv') or True:
        zb[hit] = np.minimum(zb[hit], zd[hit])  # its own depth (EP_SOLID / the prepass writes it)
    if shell is not None and look in ('normal', 'nv'):
        add = np.zeros(img.shape)

        def rimpx(r, c, z, t):
            keep = z <= zb[r, c] + 1e-6
            add[r[keep], c[keep]] = RIM * alpha * (0.5 if look == 'nv' else 1.0)
        raster(cam, shell, rimpx)
        img[:] = np.clip(img + add, 0, 1)


# ---------------------------------------------------------------- sprites (echo_sim_test --look)
def value_noise(u, v, seed, f=4.0):
    rng = np.random.RandomState(seed)
    g = rng.rand(17, 17)
    x, y = np.clip(u, 0, 1) * f, np.clip(v, 0, 1) * f
    ix, iy = np.floor(x).astype(int) % 16, np.floor(y).astype(int) % 16
    fx, fy = x - np.floor(x), y - np.floor(y)
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = g[iy, ix] * (1 - fx) + g[iy, ix + 1] * fx
    b = g[iy + 1, ix] * (1 - fx) + g[iy + 1, ix + 1] * fx
    return a * (1 - fy) + b * fy


def tex_value(kind, u, v, atlas, cell):
    du, dv = u * 2 - 1, v * 2 - 1
    r2 = du * du + dv * dv
    soft = np.clip(1 - r2, 0, 1) ** 2
    if kind in (ESK_SOFT, ESK_GROUND, ESK_WISP):  # v3.32: DPS_SOFT as the game bakes it (a flat core to 0.2, smoothstep out)
        r = np.sqrt(r2)
        return (1 - FP.smooth(0.2, 1.0, r)) * (1 - FP.smooth(0.8, 1.0, r)) * (r < 1)
    if kind == ESK_ORB:
        return np.clip(np.exp(-r2 * 7.0) + 0.6 * np.exp(-r2 * 40.0), 0, 1) * (r2 < 1)
    if kind in (ESK_SMOKE0, ESK_SMOKE1):
        n = 0.6 * value_noise(u, v, 11 + kind, 4) + 0.4 * value_noise(u, v, 21 + kind, 9)
        return np.clip(soft ** 0.7 * (0.35 + 1.1 * (n - 0.35)), 0, 1)
    if kind == ESK_CREST:
        n = value_noise(u, v, 31, 5)
        return soft * FP.smooth(0.55, 0.85, n)
    if kind == ESK_FLAME:
        c = cell % 28
        x0, y0 = (c & 7) * 64, (c >> 3) * 128
        fx = np.clip(u, 0, 1) * 63  # v3.31: bilinear, as the game's sampler (v3.30: nearest)
        fy = np.clip(v, 0, 1) * 127
        ix, iy = np.minimum(fx.astype(int), 62), np.minimum(fy.astype(int), 126)
        ax, ay = fx - ix, fy - iy
        t00, t10 = atlas[y0 + iy, x0 + ix], atlas[y0 + iy, x0 + ix + 1]
        t01, t11 = atlas[y0 + iy + 1, x0 + ix], atlas[y0 + iy + 1, x0 + ix + 1]
        val = (t00 * (1 - ax) + t10 * ax) * (1 - ay) + (t01 * (1 - ax) + t11 * ax) * ay
        return val * (u >= 0) * (u <= 1) * (v >= 0) * (v <= 1)
    return soft


def away(cam, p, d):
    e = np.asarray(p) - cam.eye
    return np.asarray(p) + e / (np.linalg.norm(e) + 1e-9) * d


def draw_sprites(F, cam, sprs, atlas, floor_y):
    img, zb = F
    items = []
    for s in sprs:
        kind, add, cell = int(s[0]), int(s[1]), int(s[2])
        p, tip = np.array(s[3:6]), np.array(s[6:9])
        r, a, rgb, push = s[9], s[10], np.array(s[11:14]), s[14]
        pad = int(s[15]) if len(s) > 15 else 1  # v3.32: the floor fade bit (v3.31's output: every flame faded)
        if push > 0:
            p, tip = away(cam, p, push), away(cam, tip, push)
        items.append((kind, add, cell, p, tip, r, a, rgb, pad))
    pre = [it for it in items if not it[1]]
    adds = [it for it in items if it[1]]
    pre.sort(key=lambda it: -cam.proj(it[3])[2])
    gr = [it for it in pre if it[0] == ESK_GROUND]
    pre = [it for it in pre if it[0] != ESK_GROUND]
    zb0 = zb.copy()
    for kind, add, cell, p, tip, r, a, rgb, pad in gr:  # flat on the floor (after the bodies: their depth hides the middle)
        d = cam.rays()
        with np.errstate(divide='ignore', invalid='ignore'):
            t = (p[1] - cam.eye[1]) / d[..., 1]
        P = cam.eye + d * t[..., None]
        q = np.sqrt((P[..., 0] - p[0]) ** 2 + (P[..., 2] - p[2]) ** 2) / r
        al = np.clip(1 - q * q, 0, 1) ** 2 * a * (t > 0) * (t <= zb0 + 0.5)
        img[:] = img * (1 - al[..., None])
    for kind, add, cell, p, tip, r, a, rgb, pad in pre + adds:
        if kind == ESK_WISP:  # v3.32: echo.inc EchoStreak: the soft cell centred half way, stretched along p -> tip
            P0, P1 = cam.proj(p), cam.proj(tip)
            if P0[2] < 5:
                continue
            c3 = 0.5 * (p + tip)
            C = cam.proj(c3)
            ex, ey = (P1[0] - P0[0]), (P1[1] - P0[1])
            el = math.hypot(ex, ey)
            if el < 1e-3:
                ex, ey, el = 0.0, -1.0, 0.0
            else:
                ex, ey = ex / el, ey / el
            hw = r * cam.k / C[2]
            hl = max(hw, 0.5 * el + 0.5 * hw)
            ext = max(hw, hl) * 1.5
        elif kind == ESK_FLAME:
            foot = 0.2
            P0, P1 = cam.proj(p), cam.proj(tip)
            if P0[2] < 5:
                continue
            ex, ey = (P1[0] - P0[0]), (P1[1] - P0[1])
            el = math.hypot(ex, ey)
            wl = np.linalg.norm(tip - p) * cam.k / P0[2]
            if el < 0.05 * wl:
                ex, ey, el = 0.0, -1.0, 1.0
            ex, ey = ex / el, ey / el
            el = max(el, 0.45 * wl)
            c3 = p + (tip - p) * ((0.5 - foot) / (1 - foot))
            C = cam.proj(c3)
            hw = r * cam.k / C[2]
            K = max(1.0, el / (1 - foot) * 0.5 / hw)
            hl = hw * K
            ext = max(hw, hl) * 1.5
        else:
            C = cam.proj(p)
            if C[2] < 5:
                continue
            hw = r * cam.k / C[2]
            if kind == ESK_ORB:
                hw = max(hw, 1.3 * SS * (TH / SS) / 720.0)
            ext = hw * 1.5
            ex, ey, hl = 0.0, -1.0, hw
        x0, x1 = int(max(0, C[0] - ext)), int(min(TW, C[0] + ext + 1))
        y0, y1 = int(max(0, C[1] - ext)), int(min(TH, C[1] + ext + 1))
        if x1 <= x0 or y1 <= y0:
            continue
        ys, xs = np.mgrid[y0:y1, x0:x1] + 0.5
        dx, dy = xs - C[0], ys - C[1]
        along = dx * ex + dy * ey
        across = -dx * ey + dy * ex
        u = 0.5 + across / (2 * hw)
        v = 0.5 - along / (2 * hl)
        inside = (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
        T = tex_value(kind, u, v, ATLAS, cell) * inside
        if kind == ESK_FLAME and pad:  # v3.31: faded out on its floor in its own plane (0..35 u above it), as DPF_FLOOR | DPF_CEIL
            dd = cam.f[None, None, :] + ((xs - TW / 2) / cam.k)[..., None] * cam.r + (-(ys - TH / 2) / cam.k)[..., None] * cam.u
            wy = cam.eye[1] + dd[..., 1] * C[2]
            T = T * np.clip((floor_y - wy) / 35.0, 0, 1)
        vis = C[2] <= zb[y0:y1, x0:x1] + 0.5
        al = np.clip(T * a, 0, 1) * vis
        sub = img[y0:y1, x0:x1]
        if add:
            sub[:] = np.clip(sub + al[..., None] * rgb, 0, 1)
        else:
            sub[:] = sub * (1 - al[..., None]) + al[..., None] * np.clip(rgb, 0, 1)


def look_sprites(shadows, look, t, v331=False, ver='334'):
    """shadows: dicts {mats, face, idx, vis, flare, floor, calm, events:[(t0, kind, seed, mats)]}: the C side's sprite list
    (v331: the v3.31 exe and format, no calm)"""
    code = {'normal': 0, 'dark': 0, 'thermal': 1, 'nv': 2, 'sv': 3}[look]
    lines = []
    for s in shadows:
        b = [s['mats'][k][3, :3] for k in EB]
        ev = s.get('events', [])
        f = s['face']
        lines.append('S %.4f %.4f %d %.3f %.3f %d %d %.5f %.5f 0 0 0 %.4f %.4f %.4f %d' % (
            t, s.get('vis', 1.0), code, s.get('flare', 0.0), s['floor'], s['idx'], s.get('lite', 0), f[0], f[1], LIGHT[0], LIGHT[1],
            LIGHT[2], len(ev)) + ('' if v331 else ' %.4f' % s.get('calm', 0.0)))
        lines += ['%.3f %.3f %.3f' % tuple(p) for p in b]
        H = s['mats'][11]
        for k in range(3):  # v3.31: the head bone's axes (unit rows), or zeros (v3.30's facing frame)
            r = H[k, :3] / (np.linalg.norm(H[k, :3]) + 1e-9) if not s.get('v330eyes') else np.zeros(3)
            lines.append('%.5f %.5f %.5f' % tuple(r))
        for t0, kind, seed, mats in ev:
            lines.append('E %.4f %d %d' % (t0, kind, seed))
            lines += ['%.3f %.3f %.3f' % tuple(mats[k][3, :3]) for k in EB]
    tmp = os.path.join(os.environ.get('TEMP', '.'), 'echo_look_in.txt')
    out = tmp.replace('_in.', '_out.')
    with open(tmp, 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    exe = EXE331 if v331 else EXE332 if ver == '332' else EXE333 if ver == '333' else EXE
    subprocess.run([exe, '--look', tmp, out], check=True, stdout=subprocess.DEVNULL)
    return [list(map(float, ln.split())) for ln in open(out) if ln.strip()]


def facing_of(mats, yaw0):
    return (math.sin(yaw0), math.cos(yaw0))


def finish(img, look):
    if look == 'nv':
        img = FP.filter_nv(img.astype(np.float32))
    if look == 'nvmono':  # v3.33: night vision mono, a grey stand-in (the same gain and vignette)
        g = FP.filter_nv(img.astype(np.float32))[..., 1:2]
        img = np.repeat(g, 3, 2) * np.array([0.92, 0.95, 0.92])
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    return im.resize((W, H), Image.LANCZOS)


def label(im, text):
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, im.width, 20], fill=(0, 0, 0))
    d.text((6, 4), text, fill=(235, 235, 235))
    return im


def sheet(tiles, cols, path, title):
    rows = (len(tiles) + cols - 1) // cols
    tw, th = tiles[0].size
    out = Image.new('RGB', (tw * cols, th * rows + 24), (12, 12, 14))
    ImageDraw.Draw(out).text((8, 6), title, fill=(220, 220, 230))
    for i, t in enumerate(tiles):
        out.paste(t, ((i % cols) * tw, 24 + (i // cols) * th))
    out.save(path)
    print('wrote', path)


# ---------------------------------------------------------------- the scenes
class Body:
    def __init__(self, mdl_bytes, bank_path, stock=False):
        self.m = sh4mdl.Model(mdl_bytes)
        type(next(b for p in self.m.parts for b in p.batches)).triangles = _strip_tris
        _, self.ents = sh4bin.read_container(open(bank_path, 'rb').read())

    def pose(self, slot, frame):
        mats = [M4(x) for x in pose(self.m, sh4anm.Clip(self.ents[slot]), frame)]
        root = mats[0][3, :3].copy()
        foot = max(mats[7][3, 1], mats[4][3, 1])
        r = mats[12][3, :3] - mats[17][3, :3]
        r[1] = 0
        r /= np.linalg.norm(r)
        fwd = np.array([-r[2], 0.0, r[0]])  # her facing (xz) from her right: right = (cos, -sin) of yaw, facing = (sin, cos)
        yaw = math.atan2(fwd[0], fwd[2])
        base = np.array([root[0], foot, root[2]])
        mats = [M @ shift(-base) for M in mats]  # her feet at the origin
        return mats, yaw

    def tris(self, mats):
        return skinned(self.m, mats)

    def shell(self, cam, mats):
        rm = []
        for M in mats:
            M2 = M.copy()
            M2[:3, :3] *= 1.035
            rm.append(M2)
        return back_faces(cam, skinned(self.m, rm), rm, self.m.parents)


def _strip_tris(self):
    idx, out = self.indices, []
    for k in range(len(idx) - 2):
        a, b, c = idx[k], idx[k + 1], idx[k + 2]
        if a != b and b != c and a != c:
            out.append((b, a, c) if k & 1 else (a, b, c))
    return out


def thermal_world(img):
    """screenfx.inc THERM_*: luma x 1.35 = L; R sat((L - 0.92) 1.2), G sat((L - 0.30) 1.4), B sat((L - 0.05) 1.6); below
    0.35 the navy floor (0.04, 0.03, 0.22) fades in (x 3): navy, blue, azure, pale cyan (a lamp is never hot)"""
    L = np.clip(FP._lum(img.astype(np.float32)) * 1.35, 0, 1)
    rgb = np.stack([np.clip((L - 0.92) * 1.2, 0, 1), np.clip((L - 0.30) * 1.4, 0, 1), np.clip((L - 0.05) * 1.6, 0, 1)], -1)
    f = np.clip((0.35 - L) * 3.0, 0, 1)[..., None]
    return np.maximum(rgb, np.array([0.04, 0.03, 0.22]) * f)


def thermal_heat(img, heat=1.0):
    """screenfx.inc TH_*: V = luma x 0.45 + 0.62 heat; R sat((V - 0.15) 2.2), G sat((V - 0.50) 2.6), B sat((V - 0.82) 4)"""
    V = FP._lum(img.astype(np.float32)) * 0.45 + 0.62 * heat
    return np.stack([np.clip((V - 0.15) * 2.2, 0, 1), np.clip((V - 0.50) * 2.6, 0, 1), np.clip((V - 0.82) * 4.0, 0, 1)], -1)


def render(body, cam, kind, her=None, shadows=(), enemies=(), look='normal', t=2.0, shell=False, v331=False, wall_z=-420.0, ver='334',
           noground=False):
    """her: (mats) or None; shadows: list of dict(mats, yaw(world facing angle), idx, vis, flare, events[, prepass])"""
    bg = 'lit' if kind == 'lit' else 'dark'
    img, zb = room(cam, bg, 0.0, wall_z)
    if look == 'thermal':
        img = thermal_world(np.clip(img * 1.2 + 0.10, 0, 1)).astype(np.float64)  # v3.33: the game's cold world ramp
    F = (img, zb)
    if her is not None:
        if look == 'thermal':
            hot = (img.copy(), zb.copy())
            draw_her(hot, cam, body.tris(her), 'lit')
            tmp = thermal_heat(np.clip(hot[0], 0, 1))
            m = hot[1] < zb + 0
            img[m] = tmp[m]
            zb[:] = np.minimum(zb, hot[1])
        else:
            draw_her(F, cam, body.tris(her), 'dark' if (kind == 'dark' and look == 'normal') else 'lit')
    for e in enemies:
        draw_enemy(F, cam, np.array(e), dark=kind == 'dark')
    if look == 'sv':
        img[:] = FP.filter_sv(img.astype(np.float32))
    blook = 'nv' if look == 'nvmono' else look
    for s in shadows:
        tr = body.tris(s['mats'])
        sh = body.shell(cam, s['mats']) if (shell and blook in ('normal', 'nv') and s.get('vis', 1.0) > 0.01) else None
        if s.get('vis', 1.0) * s.get('govk', 1.0) > 0.02:  # v3.32: x the calm opacity (the prepass: its nearest surface only); v3.33: the look's (EchoBodyOp)
            a = s.get('vis', 1.0) * (1.0 if v331 else body_op(blook, s.get('calm', 0.0), ver)) * s.get('govk', 1.0)  # v3.35 FX335: x the budget's body fade
            draw_shadow(F, cam, tr, sh, blook, a, ver, s.get('prepass', True))
    if shadows and (look != 'thermal' or (ver in ('333', '334') and not v331)):  # v3.33: Thermal draws its coat as heat (v3.31 / v3.32: none)
        sprs = look_sprites(shadows, 'normal' if blook in ('normal', 'dark') else blook, t, v331, ver)
        if noground:  # v3.35 FX335: the budget's level 6 (the ground discs faded out)
            sprs = [q for q in sprs if int(q[0]) != ESK_GROUND]
        draw_sprites(F, cam, sprs, ATLAS, 0.0)
    return finish(img, look)


def shadow_at(body, slot, frame, dest, face, idx, **kw):
    mats, yaw0 = body.pose(slot, frame)
    turn = yaw0 - face  # rotation a takes phi to phi - a
    m2, D = place(mats, np.zeros(3), turn, np.asarray(dest, float))
    d = dict(mats=m2, face=(math.sin(face), math.cos(face)), idx=idx, floor=float(dest[1]))
    d.update(kw)
    return d


def turn_head(mats, parents, yaw, pitch):
    """bone 11's subtree turned about the head bone's origin: yaw about the world up, then pitch about its side axis"""
    sub = {11}
    changed = True
    while changed:
        changed = False
        for b, p in enumerate(parents):
            if p in sub and b not in sub:
                sub.add(b)
                changed = True
    o = mats[11][3, :3].copy()
    side = mats[11][0, :3] / np.linalg.norm(mats[11][0, :3])

    def axis_rot(ax, a):
        ax = ax / np.linalg.norm(ax)
        c, s_ = math.cos(a), math.sin(a)
        K = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]])
        R3 = np.eye(3) + s_ * K + (1 - c) * K @ K
        m = np.eye(4)
        m[:3, :3] = R3.T  # row vectors
        return m
    T = shift(-o) @ axis_rot(np.array([0.0, -1.0, 0.0]), yaw) @ axis_rot(side, pitch) @ shift(o)
    return [M @ T if b in sub else M for b, M in enumerate(mats)]


def her_at(body, slot, frame, dest, face):
    mats, yaw0 = body.pose(slot, frame)
    m2, _ = place(mats, np.zeros(3), yaw0 - face, np.asarray(dest, float))
    return m2


def calm_op(k, lo=0.45):
    """echo_sim.h EchoCalmOp: 1 - (1 - lo) x smoothstep(k)"""
    k = min(1.0, max(0.0, k))
    return 1.0 - (1.0 - lo) * k * k * (3 - 2 * k)


SLOTS = [(120.0, 0.0), (-120.0, -20.0), (70.0, -125.0)]  # echo.inc kEchoSlot (right, forward)


def slot_dest(base, face, i, side, calm):
    """a shadow's place beside her: kEchoSlot x the shared side x EchoCalmDist (1.5 calm)"""
    k = min(1.0, max(0.0, calm))
    sc = 1.0 + 0.5 * k * k * (3 - 2 * k)
    lr, lf = SLOTS[i]
    f = np.array([math.sin(face), 0, math.cos(face)])
    r = np.array([math.cos(face), 0, -math.sin(face)])
    return np.asarray(base, float) + r * lr * side * sc + f * lf * sc


def main():
    global ATLAS
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SANDBOX, '..', 'renders')
    ATLAS = FP_load_atlas(sys.argv[2])
    os.makedirs(out, exist_ok=True)
    eil = Body(open(MDL, 'rb').read(), BANK)
    _, stock = sh4bin.read_container(open(STOCK, 'rb').read())
    hen = Body(stock[170], STOCK)
    face0 = math.radians(5)  # three-quarter front view (the camera is at about 34 degrees)
    cam = Cam((470, -330, 760), (-40, -165, -60), fov=38)
    camc = Cam((70, -305, 150), (-20, -275, -40), fov=28)
    camc2 = Cam((80, -300, 170), (-20, -278, -40), fov=15)  # the eyes, closer
    only = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    has331 = bool(EXE331) and os.path.exists(EXE331)

    def want(name):
        return not only or name in only

    # 1. idle: v3.31's column vs v3.32's coat, lit wall / dark room; its back; the calm (faded) idle
    if want('idle'):
        tiles = []
        for kind in ('lit', 'dark'):
            s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0)]
            if has331:
                tiles.append(label(render(eil, cam, kind, shadows=s, t=2.3, v331=True), 'v3.31 idle, %s: one tall column' % kind))
            tiles.append(label(render(eil, cam, kind, shadows=s, t=2.3), 'v3.32 idle, %s: the flame coat' % kind))
        s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0 + math.pi, 0)]
        tiles.append(label(render(eil, cam, 'lit', shadows=s, t=2.9), 'v3.32 lit, its back (eyes hidden)'))
        s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0)]
        tiles.append(label(render(eil, cam, 'dark', shadows=s, t=3.7), 'v3.32 dark, another instant (wisps)'))
        for kind in ('lit', 'dark'):
            s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0, calm=1.0)]
            tiles.append(label(render(eil, cam, kind, shadows=s, t=2.3), 'v3.32 calm (not attacking), %s: 45 %%' % kind))
        sheet(tiles, 4, os.path.join(out, 'echo_idle.png'),
              'Echo v3.32: the black flame tight to the figure (tongues along body and limbs), fine purple wisps; v3.31 for comparison')

    # 2. the eyes, close: v3.31 (10 u glow) vs v3.32 (orb + 15 u glow + 30 u haze: a wider, softer gradient)
    if want('eyes'):
        tiles = []
        for kind in ('dark', 'lit'):
            s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0)]
            if has331:
                tiles.append(label(render(eil, camc2, kind, shadows=s, t=2.3, v331=True), 'v3.31 eyes, %s' % kind))
            tiles.append(label(render(eil, camc2, kind, shadows=s, t=2.3), 'v3.32 eyes, %s' % kind))
        s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0)]
        s[0]['mats'] = turn_head(s[0]['mats'], eil.m.parents, math.radians(40), 0.0)
        tiles.append(label(render(eil, camc, 'dark', shadows=s, t=2.3), 'v3.32 head turned 40 (eyes on the head bone)'))
        s = [shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0, calm=1.0)]
        tiles.append(label(render(eil, camc, 'dark', shadows=s, t=2.3), 'v3.32 calm: eyes 70 %, body 45 %, coat 30 %'))
        sheet(tiles, 3 if has331 else 2, os.path.join(out, 'echo_eyes.png'),
              'The eyes: a hot orb, a soft 15 u glow and a faint 30 u haze (v3.31: one 10 u glow at 0.55)')

    # 3. calm vs striking from the over-the-shoulder camera (the user's item 3): v3.31 solid at 1 x, v3.32 calm at 1.5 x and
    # 45 %, v3.32 striking (solid, 1 x); 3 Shadow Copy shadows, her in front
    if want('calm'):
        tiles = []
        base = np.array([0.0, 0.0, 0.0])
        fz = face0
        fwd = np.array([math.sin(fz), 0, math.cos(fz)])
        rgt = np.array([math.cos(fz), 0, -math.sin(fz)])
        ots = Cam(tuple(base - fwd * 560 + rgt * 60 + np.array([0, -250, 0])), tuple(base + fwd * 300 + np.array([0, -100, 0])), fov=52)
        for kind in ('lit', 'dark'):
            herI = her_at(eil, IDLE, 12.0, base, fz)
            herS = her_at(eil, SWING, 19.0, base, fz)
            for nm, calm, swing, v331 in (('v3.31: idle, solid, 1 x', 0.0, False, True), ('v3.32 calm: 1.5 x, 45 %', 1.0, False, False),
                                          ('v3.32 striking: solid, 1 x', 0.0, True, False)):
                if v331 and not has331:
                    continue
                s = []
                for i in range(3):
                    d = slot_dest(base, fz, i, -1.0, calm)
                    if swing:
                        s.append(shadow_at(eil, SWING, max(0.0, 19.0 - [6.0, 11.5, 16.0][i]), d, fz, i, calm=calm))
                    else:
                        s.append(shadow_at(eil, IDLE, 12.0 + 3 * i, d, fz, i, calm=calm))
                tiles.append(label(render(eil, ots, kind, her=herS if swing else herI, shadows=s, t=3.1, v331=v331, wall_z=700.0), '%s, %s' % (nm, kind)))
        sheet(tiles, 3 if has331 else 2, os.path.join(out, 'echo_calm.png'),
              'Calm vs striking (over her shoulder, 3 shadows): not attacking they stand 1.5 x farther at 45 %; striking: solid, v3.31 distance')

    # 4. Shadow Copy beside her: 1 vs 3, striking (solid), lit / dark
    if want('copy'):
        tiles = []
        her = her_at(eil, SWING, 19.0, (60, 0, 40), face0)
        for kind, n in (('lit', 1), ('dark', 1), ('lit', 3), ('dark', 3)):
            s = []
            for i in range(n):
                d = slot_dest((60, 0, 40), face0, i, -1.0, 0.0)
                s.append(shadow_at(eil, SWING, max(0.0, 19.0 - [6.0, 11.5, 16.0][i]), d, face0, i))
            tiles.append(label(render(eil, cam, kind, her=her, shadows=s, t=3.1), 'Shadow Copy, %d striking, %s' % (n, kind)))
        sheet(tiles, 4, os.path.join(out, 'echo_copy.png'), 'Shadow Copy v3.32: 1 vs 3 shadows striking (solid), lit / dark')

    # 5. Shadow Cast: round an enemy, striking
    if want('cast'):
        tiles = []
        en = np.array([-140.0, 0.0, -180.0])
        for kind, n, two, nm in (('dark', 3, False, 'Shadow Cast, 3 on one enemy, dark'), ('lit', 1, False, 'Shadow Cast, 1, lit'),
                                 ('lit', 3, False, 'Shadow Cast, 3 on one enemy, lit'), ('dark', 3, True, 'Shadow Cast, 2 enemies (2 + 1), dark')):
            enemies = [en] if not two else [en, en + np.array([260.0, 0, -60.0])]
            her2 = her_at(eil, SWING, 21.0, (300, 0, 120), math.radians(250))
            s = []
            for i in range(n):
                tgt = enemies[0] if (not two or i != 1) else enemies[1]
                j = i if not two else (0 if i == 0 else 1 if i == 2 else 0)
                a = math.radians(35) + j * 2 * math.pi / 3
                dest = tgt + np.array([110 * math.sin(a), 0, 110 * math.cos(a)])
                dd = tgt - dest
                s.append(shadow_at(eil, SWING, 15.0 + 2.0 * i, dest, math.atan2(dd[0], dd[2]), i))
            tiles.append(label(render(eil, cam, kind, her=her2, shadows=s, enemies=enemies, t=4.2), nm))
        sheet(tiles, 4, os.path.join(out, 'echo_cast.png'), 'Shadow Cast v3.32: each shadow at an enemy, striking (solid), dark / lit, 1 vs 3')

    # 6. teleport: out (dissolve into smoke) and in (reform from smoke); the wisps / edges wait while the smoke is thick
    if want('teleport'):
        tiles = []
        mats0 = shadow_at(eil, IDLE, 12.0, (-20, 0, -40), face0, 0)['mats']
        for kind in ('dark', 'lit'):
            for age, nm in ((0.0, 'out 0.00 s'), (0.08, 'out 0.08'), (0.16, 'out 0.16 (gone)'), (0.35, 'out 0.35 (smoke)')):
                vis = 1.0 - FP.smooth(0.0, 0.16, age)
                sw = 1 + 0.08 * (1 - vis)
                m2 = [M @ shift(-np.array([-20, -95, -40])) @ np.diag([sw, sw, sw, 1]) @ shift(np.array([-20, -95, -40])) for M in mats0]
                s = [dict(mats=m2, face=(math.sin(face0), math.cos(face0)), idx=0, vis=vis, floor=0.0, events=[(2.0, 0, 77, mats0)])]
                tiles.append(label(render(eil, cam, kind, shadows=s, t=2.0 + age), kind + ': ' + nm))
            for age, nm in ((0.08, 'in 0.08'), (0.24, 'in 0.24 (formed)'), (0.45, 'in 0.45'), (0.75, 'in 0.75 (wisps back)')):
                vis = FP.smooth(0.0, 0.24, age)
                sw = 0.88 + 0.12 * vis
                m2 = [M @ shift(-np.array([-20, -95, -40])) @ np.diag([sw, sw, sw, 1]) @ shift(np.array([-20, -95, -40])) for M in mats0]
                s = [dict(mats=m2, face=(math.sin(face0), math.cos(face0)), idx=0, vis=vis, floor=0.0,
                          flare=1 - min(1, max(0, age - 0.24) / 0.4) if age >= 0.24 else 0.0, events=[(2.0, 1, 78, mats0)])]
                tiles.append(label(render(eil, cam, kind, shadows=s, t=2.0 + age), kind + ': ' + nm))
        sheet(tiles, 4, os.path.join(out, 'echo_teleport.png'), 'Teleport: dissolve into black smoke, reform from smoke (the wisps and edges wait while it is thick)')

    # 7. the filters
    if want('filters'):
        tiles = []
        s = [shadow_at(eil, SWING, 12.0, (-20, 0, -40), face0, 0), shadow_at(eil, IDLE, 12.0, (-150, 0, 60), face0, 1, calm=1.0)]
        her = her_at(eil, SWING, 19.0, (110, 0, 80), face0)
        for kind, look, nm in (('lit', 'normal', 'normal, lit (striking + calm)'), ('dark', 'normal', 'normal, dark'), ('dark', 'nv', 'night vision (before its filter)'),
                               ('dark', 'thermal', 'Thermal (v3.33): cold body, coat as heat'), ('dark', 'sv', 'Spirit Vision (v3.33): pale spectral')):
            tiles.append(label(render(eil, cam, kind, her=her, shadows=s, look=look, t=2.6), nm))
        sheet(tiles, 5, os.path.join(out, 'echo_filters.png'), 'The looks under the filters (2 shadows: one striking, one calm; Playable Eileen; v3.33: see-through under every vision filter)')

    # 8. vanilla Henry as the player: his own 30-bone body
    if want('henry'):
        tiles = []
        herH = her_at(hen, SWING, 19.0, (60, 0, 40), face0)
        herHI = her_at(hen, IDLE, 12.0, (60, 0, 40), face0)
        en = np.array([-140.0, 0.0, -180.0])
        for kind in ('lit', 'dark'):
            s = [shadow_at(hen, SWING, 19.0 - 7.0 * (i + 1), slot_dest((60, 0, 40), face0, i, -1.0, 0.0), face0, i) for i in range(2)]
            tiles.append(label(render(hen, cam, kind, her=herH, shadows=s, t=3.3), 'Henry, Shadow Copy x2 striking, %s' % kind))
            s = [shadow_at(hen, IDLE, 12.0 + 4 * i, slot_dest((60, 0, 40), face0, i, -1.0, 1.0), face0, i, calm=1.0) for i in range(2)]
            tiles.append(label(render(hen, cam, kind, her=herHI, shadows=s, t=3.3), 'Henry, Shadow Copy x2 calm, %s' % kind))
        s = []
        for i in range(3):
            a = math.radians(35) + i * 2 * math.pi / 3
            dest = en + np.array([110 * math.sin(a), 0, 110 * math.cos(a)])
            dd = en - dest
            s.append(shadow_at(hen, SWING, 15.0 + 2.0 * i, dest, math.atan2(dd[0], dd[2]), i))
        tiles.append(label(render(hen, cam, 'dark', her=her_at(hen, SWING, 21.0, (260, 0, 160), math.radians(225)), shadows=s,
                                  enemies=[en], t=3.8), 'Henry, Shadow Cast x3, dark'))
        s = [shadow_at(hen, IDLE, 12.0, (-20, 0, -40), face0, 0)]
        tiles.append(label(render(hen, camc, 'dark', shadows=s, t=2.3), 'Henry, close (eyes, coat)'))
        sheet(tiles, 3, os.path.join(out, 'echo_henry.png'), 'Vanilla Henry as the player: the same coat on his own 30-bone body')

    # 9. busy (a pickup / a candle): an enemy near: her last swing replayed at it (Copy: a lunge, solid); none: her idle
    # pose, calm (never her pickup pose)
    if want('busy'):
        tiles = []
        herP = her_at(eil, IDLE, 12.0, (60, 0, 40), face0)
        f = np.array([math.sin(face0), 0, math.cos(face0)])
        r = np.array([math.cos(face0), 0, -math.sin(face0)])
        home = np.array([60, 0, 40]) + r * -120
        enb = home + np.array([-190.0, 0.0, -150.0])
        d = enb - home
        lunge = home + d / np.linalg.norm(d) * (np.linalg.norm(d) - 110)
        fc = math.atan2(d[0], d[2])
        for kind in ('lit', 'dark'):
            s = [shadow_at(eil, SWING, 19.0, lunge, fc, 0)]
            tiles.append(label(render(eil, cam, kind, her=herP, shadows=s, enemies=[enb], t=2.9), 'busy, an enemy near (%s): her last swing (solid)' % kind))
            s = [shadow_at(eil, IDLE, 12.0, slot_dest((60, 0, 40), face0, 0, -1.0, 1.0), face0, 0, calm=1.0)]
            tiles.append(label(render(eil, cam, kind, her=herP, shadows=s, t=2.9), 'busy, no enemy (%s): idle pose, calm' % kind))
        sheet(tiles, 4, os.path.join(out, 'echo_busy.png'), 'Her busy moments (a pickup, a candle): her last attack at an enemy, else her idle pose (never the pickup)')

    # 10. v3.33: the 1-of-3 case (over her shoulder, 3 calm shadows, budget level 2): v3.32 (2nd / 3rd without the prepass:
    # every surface blended) vs v3.33 (all three with it); Eileen and Henry
    if want('1of3'):
        tiles = []
        base = np.array([0.0, 0.0, 0.0])
        fz = face0
        fwd = np.array([math.sin(fz), 0, math.cos(fz)])
        rgt = np.array([math.cos(fz), 0, -math.sin(fz)])
        ots = Cam(tuple(base - fwd * 560 + rgt * 60 + np.array([0, -250, 0])), tuple(base + fwd * 300 + np.array([0, -100, 0])), fov=52)
        for bd, bn in ((eil, 'Eileen'), (hen, 'Henry')):
            herI = her_at(bd, IDLE, 12.0, base, fz)
            for kind in ('lit', 'dark'):
                for nm, pre in (('v3.32 budget level 2: 1 see-through, 2 / 3 read solid', (True, False, False)),
                                ('v3.33: all three see-through (prepass kept)', (True, True, True))):
                    s = [shadow_at(bd, IDLE, 12.0 + 3 * i, slot_dest(base, fz, i, -1.0, 1.0), fz, i, calm=1.0, lite=int(i > 0), prepass=pre[i])
                         for i in range(3)]
                    tiles.append(label(render(bd, ots, kind, her=herI, shadows=s, t=3.1, wall_z=700.0), '%s %s, %s' % (bn, kind, nm)))
        sheet(tiles, 4, os.path.join(out, 'echo_1of3.png'),
              'Out of combat, 3 calm shadows at budget level 2 (weapon in hand): v3.32 (1 of 3 see-through) vs v3.33 (all see-through)')

    # 11. v3.33: the vision looks, Eileen- and Henry-sized: Thermal, Spirit Vision, Night Vision, Night Vision Mono; a striking
    # and a calm shadow each (v3.32 beside v3.33 when ECHO332_EXE is set), a close tile of each look
    if want('vision'):
        has332 = bool(EXE332) and os.path.exists(EXE332)
        for bd, bn in ((eil, 'eileen'), (hen, 'henry')):
            tiles = []
            s = [shadow_at(bd, SWING, 12.0, (-20, 0, -40), face0, 0), shadow_at(bd, IDLE, 12.0, (-150, 0, 60), face0, 1, calm=1.0)]
            her = her_at(bd, SWING, 19.0, (110, 0, 80), face0)
            for look, nm in (('thermal', 'Thermal'), ('sv', 'Spirit Vision'), ('nv', 'Night Vision'), ('nvmono', 'Night Vision Mono')):
                if has332:
                    tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look=look, t=2.6, ver='332'), 'v3.32 %s (left striking, right calm)' % nm))
                tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look=look, t=2.6), 'v3.33 %s: striking 50 %%, calm 35 %%' % nm))
                sc = [shadow_at(bd, IDLE, 12.0, (-20, 0, -40), face0, 0)]
                tiles.append(label(render(bd, camc, 'dark', shadows=sc, look=look, t=2.9), 'v3.33 %s, close (striking)' % nm))
            tiles.append(label(render(bd, cam, 'lit', her=her, shadows=s, look='normal', t=2.6), 'normal look, unchanged (reference)'))
            sheet(tiles, 3 if has332 else 2, os.path.join(out, 'echo_vision_%s.png' % bn),
                  'Vision filters, %s-sized: every shadow see-through at all times, in the filter\'s theme (v3.33)' % bn.capitalize())

    # 12. v3.34 FX334: Spirit Vision's dark specters (the user's v3.33 QA item 3: "they look too much like the player"), v3.33's
    # pale look beside them (ECHO333_EXE), Eileen- and Henry-sized; Thermal and night vision for reference (unchanged)
    if want('specter'):
        has333 = bool(EXE333) and os.path.exists(EXE333)
        for bd, bn in ((eil, 'eileen'), (hen, 'henry')):
            tiles = []
            s = [shadow_at(bd, SWING, 12.0, (-20, 0, -40), face0, 0), shadow_at(bd, IDLE, 12.0, (-150, 0, 60), face0, 1, calm=1.0)]
            her = her_at(bd, SWING, 19.0, (110, 0, 80), face0)
            sc = [shadow_at(bd, IDLE, 12.0, (-20, 0, -40), face0, 0)]
            scc = [shadow_at(bd, IDLE, 12.0, (-20, 0, -40), face0, 0, calm=1.0)]
            if has333:
                tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look='sv', t=2.6, ver='333'),
                                   'v3.33 Spirit Vision: pale, like her (left striking, right calm)'))
            tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look='sv', t=2.6), 'v3.34 Spirit Vision: dark specters (62 / 45 %)'))
            if has333:
                tiles.append(label(render(bd, camc, 'dark', shadows=sc, look='sv', t=2.9, ver='333'), 'v3.33 Spirit Vision, close (striking)'))
            tiles.append(label(render(bd, camc, 'dark', shadows=sc, look='sv', t=2.9), 'v3.34 Spirit Vision, close, striking'))
            tiles.append(label(render(bd, camc, 'dark', shadows=scc, look='sv', t=2.9), 'v3.34 Spirit Vision, close, calm'))
            tiles.append(label(render(bd, cam, 'lit', her=her, shadows=s, look='sv', t=3.4), 'v3.34 Spirit Vision, a lit room'))
            tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look='thermal', t=2.6), 'v3.34 Thermal (unchanged)'))
            tiles.append(label(render(bd, cam, 'dark', her=her, shadows=s, look='nv', t=2.6), 'v3.34 Night Vision (unchanged)'))
            sheet(tiles, 4, os.path.join(out, 'echo_specter_%s.png' % bn),
                  'Spirit Vision, %s-sized: v3.33 pale shadows (read like her) vs v3.34 dark specters; Thermal / Night Vision unchanged' % bn.capitalize())


    # 13. v3.35 FX335: the effect budget's Echo steps, three shadows beside her: full; the 3rd body mid-fade (50 %, blended as
    # the game's fading path, no prepass while striking); the 3rd body gone (its flame coat, eyes and strikes stay); + the light
    # look on the 2nd / 3rd; the 2nd body gone too; the last resort (every body gone, no ground discs)
    if want('budget'):
        tiles = []
        steps = (('level 0: three shadows, full', (1.0, 1.0, 1.0), (0, 0, 0), False),
                 ('level 1, mid-fade: the 3rd body at 50 %', (1.0, 1.0, 0.5), (0, 0, 0), False),
                 ('level 1: the 3rd body gone, its coat and eyes stay', (1.0, 1.0, 0.0), (0, 0, 0), False),
                 ('level 3: + the light look on the 2nd / 3rd', (1.0, 1.0, 0.0), (0, 1, 1), False),
                 ('level 4: the 2nd body gone too', (1.0, 0.0, 0.0), (0, 1, 1), False),
                 ('level 9, the last resort: every body gone', (0.0, 0.0, 0.0), (1, 1, 1), True))
        for bd, bn, kind, calm in ((eil, 'Eileen', 'lit', 0.0), (hen, 'Henry', 'dark', 1.0)):
            her = her_at(bd, SWING if calm == 0.0 else IDLE, 19.0 if calm == 0.0 else 12.0, (60, 0, 40), face0)
            for nm, gk, lite, nog in steps:
                s = []
                for i in range(3):
                    d = slot_dest((60, 0, 40), face0, i, -1.0, calm)
                    if calm == 0.0:
                        s.append(shadow_at(bd, SWING, max(0.0, 19.0 - [6.0, 11.5, 16.0][i]), d, face0, i, govk=gk[i], lite=lite[i],
                                           prepass=gk[i] >= 1.0))
                    else:
                        s.append(shadow_at(bd, IDLE, 12.0 + 3 * i, d, face0, i, calm=calm, govk=gk[i], lite=lite[i]))
                tiles.append(label(render(bd, cam, kind, her=her, shadows=s, t=3.1, noground=nog),
                                   '%s %s: %s' % (bn, 'striking' if calm == 0.0 else 'calm', nm)))
        sheet(tiles, 3, os.path.join(out, 'echo_budget.png'),
              'v3.35 effect budget, Echo: bodies fade into their smoke (coat, eyes and strikes stay), the light look, the last resort')


def FP_load_atlas(path):
    with open(path, 'rb') as f:
        data = f.read()
    parts = data.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return np.frombuffer(parts[3][:w * h], dtype=np.uint8).reshape(h, w).astype(np.float64) / 255.0


ATLAS = None
if __name__ == '__main__':
    main()
