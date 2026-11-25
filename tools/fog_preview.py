"""fog_preview.py - offline preview of the "Death Presence" fog (v3.27.0 PLAN section 8.1-8.4).

Python/numpy port of the art of 8.1 (three stacked ground-parallel layers on a 12 ring x 32 sector polar disc, world
anchored tileable fBm texture with domain warp, scroll + rotation per layer in opposite senses, radial mask 1 inside
0.55 R -> 0 at 1.15 R with a breathing angular rim, near-camera fade, per-vertex alpha interpolated over the mesh),
8.2 (additive violet crest highlights on the top two layers), 8.3 (24 rising curling world puffs, per-victim slab disc,
bone-bound dark violet puffs, violet motes at ramp r > 0.5) and 8.4 (32 radial wall distances, fade beyond the hit).
Blending is the game's: bodies premultiplied ONE / INVSRCALPHA, highlights / motes ONE / ONE, depth GREATEREQUAL
(reversed Z; here: the fog shows only where it is nearer than the scene), no z write.

Approximations: the scene (room, capsule player, stick-figure victims) is a stand-in with NO game art; the downed-ghost
ring (8.3) is not drawn; sprite cells are a procedural stand-in for FXX_SVATLAS 5 / 6; the near-camera fade is the
SV_NEAR0 / SV_NEAR1 rule of aura_preview.py.

Usage:  py -3.10 tools/fog_preview.py [--out DIR] [--only a,b] [NAME=value ...]
        (default out: staging/v327_death_presence/renders; NAME=value overrides a constant below, for tuning runs)
Writes fog_<frame>.png for every frame, scene_nofog.png and fog_sheet.png (a labelled contact sheet).
"""
import math
import os
import sys
from functools import partial

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, 'staging', 'v327_death_presence', 'renders', 'v3')
W, H, SS = 1280, 720, 2          # output size, supersampling

# ======================================================================= TUNABLE CONSTANTS
R_WEAK, R_STRONG = 300.0, 400.0
RINGS, SECTORS = 12, 32
MASK_IN, MASK_OUT = 0.55, 1.15           # radial mask: 1 inside MASK_IN * R, 0 at MASK_OUT * R
RIM_AMP = 0.10                           # breathing rim: the mask edge shrinks by up to this fraction
RIM_SPEED = 1.0                          # time scale of the angular rim noise
BODY = (0.018, 0.006, 0.030)             # premultiplied near-black violet body colour
# per layer: height above the floor, density, texture scale (world units per tile), scroll (u/s), rotation (rad/s)
LAYERS = (
    dict(y=4.0,   dens=0.85, scale=380.0, scroll=(14.0, 6.0),   rot=+0.045),
    dict(y=30.0,  dens=0.60, scale=260.0, scroll=(-9.0, 12.0),  rot=-0.060),
    dict(y=70.0,  dens=0.40, scale=520.0, scroll=(7.0, -15.0),  rot=+0.035),
    dict(y=110.0, dens=0.22, scale=440.0, scroll=(-12.0, -6.0), rot=-0.050),
)
TEX_LO, TEX_HI = 0.12, 0.65              # coverage curve over the normalised fBm: smoothstep(TEX_LO, TEX_HI, n)
TEX_WARP = 0.10                          # domain warp amplitude, texture units
HI_COL = (0.16, 0.05, 0.30)              # 8.2 additive highlight colour
HI_GAIN = 0.15                           # 8.2 gain
HI_LO, HI_HI = 0.62, 0.92                # crest curve over the texture: smoothstep(HI_LO, HI_HI, n)
HI_LAYERS = (1, 2, 3)                       # layers that get highlights
NEAR0, NEAR1 = 60.0, 250.0               # near-camera fade (SvSprAdd)
WALL_LEN = 40.0                          # 8.4 fade length beyond the wall hit
WALL_START = -40.0                         # offset of the fade start from the hit (negative = starts before the wall)
WALL_SECTORS = 32
# 8.3 world puffs
PUFF_N, PUFF_RISE, PUFF_ALPHA = 24, 15.0, 0.32
PUFF_LIFE = (2.5, 5.0)
PUFF_R0, PUFF_R1 = 0.3, 0.9              # spawn radius, fraction of R
PUFF_SIZE = (60.0, 100.0)                 # world radius at spawn
PUFF_COL = (0.060, 0.018, 0.100)         # dark violet puff colour (alpha 0.18 multiplies it)
# victims
VSLAB_R = (90.0, 140.0)                  # slab radius, scaled by (0.5 + 0.5 r)
VSLAB_Y, VSLAB_DENS = 12.0, 0.70
VBOOST, VBOOST_R = 0.9, 170.0            # extra density of the main layers around a victim: 1 + VBOOST * r * falloff
VPUFF_COL = (0.100, 0.028, 0.175)
# v3: violet variation inside the fog body (low-frequency, slow): deep violet <-> blue-violet / magenta tints
TINT_BLUE, TINT_MAG = (0.010, 0.014, 0.062), (0.052, 0.008, 0.046)
TINT_SCALE, TINT_SPEED, TINT_DENSE = 900.0, 0.004, 0.4   # world units per tile, tile/s, extra brightness in the dense parts
# v3: raised wisps (camera-facing vertical smoke tendrils) around the lower body
WISP_N = 48
WISP_R0, WISP_R1 = 0.15, 0.60            # spawn radius, fraction of R
WISP_Y0 = (20.0, 36.0)                   # bottom height of a tendril at spawn
WISP_RISE = 7.0                          # u/s
WISP_HALF_H = (34.0, 52.0)               # half height (so a tendril spans 70-105 u)
WISP_HALF_W = (13.0, 22.0)               # half width of the strand
WISP_LIFE = (4.0, 7.0)
WISP_ALPHA = 0.34
WISP_OMEGA = (0.12, 0.26)                # swirl rad/s around the player, alternating sense
WISP_CURL = 0.55                         # S-curl amplitude, fraction of the strand half width
WISP_COLS = ((0.110, 0.030, 0.190), (0.060, 0.050, 0.210), (0.170, 0.030, 0.170))   # violet / blue-violet / magenta
# v3: victim body shimmer
VPUFF_N = (10, 12)                       # bone-bound puffs = a + round(b * r)
VPUFF_SIZE_R = (18.0, 20.0)
VGLOW_COL, VGLOW_A = (0.50, 0.16, 0.95), 0.30   # additive glints at the bones: alpha = VGLOW_A * (0.10 + 0.9 r^2)
VRIM_COL, VRIM_GAIN, VRIM_PX = (0.42, 0.13, 0.80), 0.9, 3   # silhouette rim glow (additive), amplitude (0.12 + 0.88 r^2)
MOTE_N = (6, 18)                         # motes = a + round(b * (r - 0.5) / 0.5) for r > 0.5
VPUFF_ALPHA = 0.18                       # grows with r: x (0.6 + 1.4 r)
VPUFF_SIZE = (16.0, 14.0)                # world radius = a + b * r
MOTE_COL, MOTE_ALPHA = (0.55, 0.18, 0.95), 0.55
# camera / scene
CAM = (0.0, 300.0, 450.0)
CAM_TARGET = (0.0, 50.0, 0.0)
FOV_V = 60.0
ROOM_BACK, ROOM_SIDE, ROOM_RIGHT, ROOM_FRONT, ROOM_H = 420.0, 380.0, 640.0, 560.0, 330.0
LAMP, LAMP_I, LAMP_D = (0.0, 290.0, -40.0), 1.5, 380.0
PLAYER_H = 170.0

M32 = 0xFFFFFFFF
# ======================================================================= helpers (screenfx.inc style)
def FxHash(a):
    a &= M32
    a ^= a >> 16
    a = (a * 0x7FEB352D) & M32
    a ^= a >> 15
    a = (a * 0x846CA68B) & M32
    a ^= a >> 16
    return a

def FxRnd(a):
    return (FxHash(a) & 0xFFFFFF) / 16777216.0

def FxSat(v):
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v

def smooth(e0, e1, x):
    """smoothstep for scalars and arrays"""
    t = np.clip((np.asarray(x, dtype=np.float32) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)

def lerp(a, b, t):
    return a + (b - a) * t

# ======================================================================= tileable fBm texture (FXX_FOG)
def _pnoise(u, v, per, lat):
    x, y = u * per, v * per
    xi, yi = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = (x - xi).astype(np.float32), (y - yi).astype(np.float32)
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x0, x1, y0, y1 = xi % per, (xi + 1) % per, yi % per, (yi + 1) % per
    a = lat[y0, x0] + (lat[y0, x1] - lat[y0, x0]) * sx
    b = lat[y1, x0] + (lat[y1, x1] - lat[y1, x0]) * sx
    return a + (b - a) * sy

def make_tex(size=128, seed=1, octaves=4, warp=TEX_WARP, base=4):
    rng = np.random.RandomState(seed)
    lats = [rng.rand(base << o, base << o).astype(np.float32) for o in range(octaves)]
    gw = [rng.rand(4, 4).astype(np.float32), rng.rand(4, 4).astype(np.float32)]
    g = (np.arange(size, dtype=np.float32) + 0.5) / size
    u, v = np.meshgrid(g, g)
    if warp > 0:
        u = u + warp * (_pnoise(u, v, 4, gw[0]) - 0.5) * 2
        v = v + warp * (_pnoise(u, v, 4, gw[1]) - 0.5) * 2
    n, amp, tot = np.zeros_like(u), 1.0, 0.0
    for o in range(octaves):
        n += amp * _pnoise(u, v, base << o, lats[o])
        tot += amp
        amp *= 0.5
    n /= tot
    lo, hi = np.percentile(n, 2), np.percentile(n, 98)
    return np.clip((n - lo) / (hi - lo), 0, 1).astype(np.float32)

FOG_TEX = None
GRIME_TEX = None

def tex_sample(T, u, v):
    """bilinear, wrapping"""
    n = T.shape[0]
    x, y = u * n - 0.5, v * n - 0.5
    xi, yi = np.floor(x), np.floor(y)
    fx, fy = (x - xi).astype(np.float32), (y - yi).astype(np.float32)
    xi, yi = xi.astype(np.int64), yi.astype(np.int64)
    x0, x1, y0, y1 = xi % n, (xi + 1) % n, yi % n, (yi + 1) % n
    a = T[y0, x0] + (T[y0, x1] - T[y0, x0]) * fx
    b = T[y1, x0] + (T[y1, x1] - T[y1, x0]) * fx
    return a + (b - a) * fy

# ======================================================================= camera and scene
class Cam:
    def __init__(self, ss, spec=None):
        self.ss = ss
        self.w, self.h = W * ss, H * ss
        spec = spec or dict(pos=CAM, target=CAM_TARGET, fov=FOV_V)
        c = np.array(spec['pos'], dtype=np.float64)
        f = np.array(spec['target'], dtype=np.float64) - c
        f /= np.linalg.norm(f)
        r = np.cross(f, (0, 1, 0))
        r /= np.linalg.norm(r)
        u = np.cross(r, f)
        self.c, self.f, self.r, self.u = c, f, r, u
        self.th = math.tan(math.radians(spec['fov']) / 2)
        self.asp = W / H
        px = (np.arange(self.w, dtype=np.float32) + 0.5) / self.w * 2 - 1
        py = 1 - (np.arange(self.h, dtype=np.float32) + 0.5) / self.h * 2
        sx, sy = np.meshgrid(px * self.asp * self.th, py * self.th)
        # d = f + sx r + sy u : its f-component is 1, so the ray parameter t IS the view depth
        self.dx = (f[0] + sx * r[0] + sy * u[0]).astype(np.float32).ravel()
        self.dy = (f[1] + sx * r[1] + sy * u[1]).astype(np.float32).ravel()
        self.dz = (f[2] + sx * r[2] + sy * u[2]).astype(np.float32).ravel()

    def project(self, p):
        """world point -> (px, py, view depth) in supersampled pixels"""
        rel = np.asarray(p, dtype=np.float64) - self.c
        z = float(rel @ self.f)
        if z < 1e-3:
            return None
        x = float(rel @ self.r) / (z * self.asp * self.th)
        y = float(rel @ self.u) / (z * self.th)
        return self.w * (x + 1) / 2, self.h * (1 - y) / 2, z

    def pix_per_unit(self, z):
        return self.h / 2 / (self.th * z)

def hashf(x, y):
    return np.mod(np.sin(x * 127.1 + y * 311.7) * 43758.5453, 1.0).astype(np.float32)

def render_scene(cam, zback, bright, player=True):
    """Stand-in room: floor (tiles + grime), back / side / right walls, capsule player. Returns (rgb N x 3, zbuf N)."""
    N = cam.w * cam.h
    C = cam.c
    inf = np.float32(1e9)
    dx, dy, dz = cam.dx, cam.dy, cam.dz
    t = np.full(N, inf, dtype=np.float32)
    kind = np.zeros(N, dtype=np.int8)               # 1 floor, 2 back wall, 3 left wall, 4 right wall, 5 player
    m = dy < -1e-6
    tf = np.full(N, inf, dtype=np.float32)
    tf[m] = (0.0 - C[1]) / dy[m]
    t, kind = tf, np.where(m, 1, 0).astype(np.int8)
    walls = ((2, 2, -zback), (3, 0, -ROOM_SIDE), (4, 0, ROOM_RIGHT))
    for k, ax, pos in walls:
        d = (dz, dx)[0 if ax == 2 else 1]
        ok = (d < -1e-6) if pos < 0 else (d > 1e-6)
        tw = np.full(N, inf, dtype=np.float32)
        o = C[ax]
        tw[ok] = (pos - o) / d[ok]
        yh = C[1] + tw * dy
        good = ok & (tw < t) & (yh < ROOM_H) & (yh > 0)
        if ax == 2:                                  # back wall extent in x
            xh = C[0] + tw * dx
            good &= (xh > -ROOM_SIDE) & (xh < ROOM_RIGHT)
        else:
            zh = C[2] + tw * dz
            good &= (zh > -zback) & (zh < ROOM_FRONT)
        t = np.where(good, tw, t)
        kind = np.where(good, k, kind).astype(np.int8)
    # player: legs/torso capsule + head sphere (vertical axis at the origin)
    def sphere(c, r):
        ox, oy, oz = C[0] - c[0], C[1] - c[1], C[2] - c[2]
        a = dx * dx + dy * dy + dz * dz
        b = ox * dx + oy * dy + oz * dz
        cc = ox * ox + oy * oy + oz * oz - r * r
        disc = b * b - a * cc
        ok = disc > 0
        ts = np.where(ok, (-b - np.sqrt(np.maximum(disc, 0))) / a, inf)
        return np.where(ts > 0, ts, inf)
    def cyl(r, y0, y1):
        a = dx * dx + dz * dz
        b = C[0] * dx + C[2] * dz
        cc = C[0] ** 2 + C[2] ** 2 - r * r
        disc = b * b - a * cc
        ok = disc > 0
        ts = np.where(ok, (-b - np.sqrt(np.maximum(disc, 0))) / a, inf)
        yh = C[1] + ts * dy
        return np.where((ts > 0) & (yh >= y0) & (yh <= y1), ts, inf)
    BR, HR = 22.0, 17.0
    tp = np.minimum.reduce([cyl(BR, BR, 118.0), sphere((0, BR, 0), BR), sphere((0, 118.0, 0), BR),
                            sphere((0, PLAYER_H - HR, 0), HR)])
    hitp = (tp < t) if player else np.zeros(N, dtype=bool)
    t = np.where(hitp, tp, t)
    kind = np.where(hitp, 5, kind).astype(np.int8)
    hit = t < inf
    tt = np.where(hit, t, 0).astype(np.float32)
    PX, PY, PZ = C[0] + tt * dx, C[1] + tt * dy, C[2] + tt * dz
    # normals
    nx, ny, nz = np.zeros(N, np.float32), np.zeros(N, np.float32), np.zeros(N, np.float32)
    ny = np.where(kind == 1, 1.0, ny)
    nz = np.where(kind == 2, 1.0, nz)
    nx = np.where(kind == 3, 1.0, nx)
    nx = np.where(kind == 4, -1.0, nx)
    pm = kind == 5
    cy = np.clip(PY, BR, 118.0)
    cyh = np.where(PY > 100.0 + 40.0, PLAYER_H - HR, cy)         # head sphere centre for the upper part
    cyy = np.where(PY > 118.0 + 5, PLAYER_H - HR, cy)
    nx = np.where(pm, PX, nx)
    ny = np.where(pm, PY - cyy, ny)
    nz = np.where(pm, PZ, nz)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz) + 1e-6
    nx, ny, nz = nx / ln, ny / ln, nz / ln
    # albedo
    alb = np.zeros((N, 3), dtype=np.float32)
    g1 = tex_sample(GRIME_TEX, PX / 260.0 + 0.31, PZ / 260.0)
    g2 = tex_sample(GRIME_TEX, PX / 70.0, PZ / 70.0 + 0.17)
    # floor: 50 unit tiles, grout, per-tile tone, grime
    tx, tz = PX / 50.0, PZ / 50.0
    fx, fz = tx - np.floor(tx), tz - np.floor(tz)
    grout = ((fx < 0.04) | (fz < 0.04)).astype(np.float32)
    tone = 0.82 + 0.3 * hashf(np.floor(tx), np.floor(tz))
    fl = (0.26 + 0.10 * g1 - 0.07 * g2) * tone * (1 - 0.55 * grout)
    floor_c = np.stack([fl * 1.00, fl * 0.92, fl * 0.80], axis=1)
    # walls: plaster with grime, darker wainscot, baseboard, streaks
    ws = np.where(kind == 2, PX, PZ)
    gw = tex_sample(GRIME_TEX, ws / 300.0 + 0.5, PY / 900.0)
    streak = tex_sample(GRIME_TEX, ws / 90.0, PY / 1100.0 + 0.3)
    wal = 0.30 + 0.12 * gw - 0.10 * streak
    wal = wal * np.where(PY < 100, 0.62, 1.0) * np.where(PY < 10, 0.5, 1.0)
    wall_c = np.stack([wal * 1.00, wal * 0.90, wal * 0.76], axis=1)
    pc = np.stack([np.full(N, 0.16), np.full(N, 0.14), np.full(N, 0.15)], axis=1).astype(np.float32)
    alb = np.where((kind == 1)[:, None], floor_c, alb)
    alb = np.where(((kind >= 2) & (kind <= 4))[:, None], wall_c, alb)
    alb = np.where(pm[:, None], pc, alb)
    if bright:                                        # mid-grey variant: neutral, light surfaces
        grey = bright * (0.7 + 0.3 * g1 - 0.15 * g2 * (kind == 1))
        gt = np.where(kind == 1, tone, 1.0) * np.where(kind == 1, 1 - 0.4 * grout, 1.0)
        alb = np.where(pm[:, None], pc * 1.3, np.stack([grey * gt] * 3, axis=1).astype(np.float32))
    # light: dim lamp + ambient
    lx, ly, lz = LAMP[0] - PX, LAMP[1] - PY, LAMP[2] - PZ
    ld = np.sqrt(lx * lx + ly * ly + lz * lz) + 1e-3
    ndl = np.maximum(0, (nx * lx + ny * ly + nz * lz) / ld)
    lit = 0.045 + LAMP_I * ndl / (1 + (ld / LAMP_D) ** 2)
    if bright:
        lit = 0.30 + 1.5 * lit
    rgb = alb * lit[:, None] * np.array([1.0, 0.93, 0.82], dtype=np.float32)
    # slight desaturated ambient so black voids are not pure
    rgb = np.where(hit[:, None], rgb, 0).astype(np.float32)
    zb = np.where(hit, t, inf).astype(np.float32)
    return rgb, zb

# ======================================================================= victims (stick figures)
BONES = ('head', 'neck', 'chest', 'pelvis', 'lSh', 'rSh', 'lHand', 'rHand', 'lKnee', 'rKnee', 'lFoot', 'rFoot')
EDGES = ((0, 1, 8), (1, 2, 10), (2, 3, 11), (1, 4, 7), (1, 5, 7), (4, 6, 5), (5, 7, 5),
         (3, 8, 8), (3, 9, 8), (8, 10, 6), (9, 11, 6))

def victim_bones(pos, t, seed):
    """12 world-space bone points of a hunched stick figure, facing the player, swaying."""
    yaw = math.atan2(-pos[0], -pos[2])
    c, s = math.cos(yaw), math.sin(yaw)
    sw = math.sin(t * 1.3 + seed) * 5
    lean = 12 + math.sin(t * 0.9 + seed * 2) * 4
    loc = {'head': (0, 160, lean * 1.6), 'neck': (0, 146, lean * 1.2), 'chest': (0, 122, lean * 0.8),
           'pelvis': (0, 92, 0), 'lSh': (-21, 142, lean), 'rSh': (21, 142, lean),
           'lHand': (-28 + sw, 82, lean + 18), 'rHand': (28 - sw, 80, lean + 22),
           'lKnee': (-13, 46, 6), 'rKnee': (13, 46, -4), 'lFoot': (-14, 3, 2), 'rFoot': (14, 3, 8)}
    out = []
    for k in BONES:
        x, y, z = loc[k]
        out.append((pos[0] + x * c + z * s, y, pos[2] - x * s + z * c))
    return out

def draw_victims(cam, rgb, zb, victims, t, cols=None, rim=True):
    if not victims:
        return None
    zi = Image.new('F', (cam.w, cam.h), 1e9)
    ci = Image.new('RGB', (cam.w, cam.h), (0, 0, 0))
    ii = Image.new('L', (cam.w, cam.h), 0)
    di = ImageDraw.Draw(ii)
    dz_, dc = ImageDraw.Draw(zi), ImageDraw.Draw(ci)
    segs = []
    for vi, (pos, r) in enumerate(victims):
        b = victim_bones(pos, t, vi * 1.7)
        for a, c, rad in EDGES:
            pa, pc_ = cam.project(b[a]), cam.project(b[c])
            if pa and pc_:
                segs.append(((pa[2] + pc_[2]) / 2, pa, pc_, rad, vi + 1))
        ph = cam.project(b[0])
        if ph:
            segs.append((ph[2], ph, ph, 13, vi + 1))
    segs.sort(key=lambda s: -s[0])
    for z, pa, pc_, rad, vid in segs:
        wpx = max(2.0, 2 * rad * cam.pix_per_unit(z))
        col = (int(255 * 0.17), int(255 * 0.12), int(255 * 0.11))
        if cols is not None:
            col = tuple(int(255 * q) for q in cols[vid - 1])
        di.line([pa[0], pa[1], pc_[0], pc_[1]], fill=vid, width=int(wpx))
        dz_.line([pa[0], pa[1], pc_[0], pc_[1]], fill=z, width=int(wpx))
        dc.line([pa[0], pa[1], pc_[0], pc_[1]], fill=col, width=int(wpx))
        for p in (pa, pc_):
            rr = wpx / 2
            di.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=vid)
            dz_.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=z)
            dc.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=col)
    za = np.asarray(zi, dtype=np.float32).ravel()
    ca = np.asarray(ci, dtype=np.float32).reshape(-1, 3) / 255.0
    m = (za < 1e8) & (za < zb)
    # a little of the room's light: darker toward the feet
    rgb[m] = ca[m]
    zb[m] = za[m]
    if not rim:
        return None
    # v3 silhouette rim: edge ring of VRIM_PX px, amplitude (0.12 + 0.88 r^2) x moving glint noise, blurred 1.5 px
    idm = np.asarray(ii, dtype=np.int32)
    m2 = m.reshape(cam.h, cam.w) & (idm > 0)
    er = m2.copy()
    for _ in range(VRIM_PX * cam.ss // 2):
        er[1:-1, 1:-1] = er[1:-1, 1:-1] & m2[:-2, 1:-1] & m2[2:, 1:-1] & m2[1:-1, :-2] & m2[1:-1, 2:]
        m2 = er.copy()
    edge = (m.reshape(cam.h, cam.w) & (idm > 0)) & ~er
    ramp = np.array([0.0] + [0.12 + 0.88 * v[1] * v[1] for v in victims], dtype=np.float32)
    gy, gx = np.mgrid[0:cam.h, 0:cam.w].astype(np.float32)
    glint = 0.25 + 0.75 * smooth(0.35, 0.75, tex_sample(FOG_TEX, gx / (260.0 * cam.ss) + t * 0.18, gy / (260.0 * cam.ss) - t * 0.11))
    amt = np.where(edge, ramp[idm] * glint, 0).astype(np.float32)
    rim = Image.fromarray((np.clip(amt, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5 * cam.ss / 2))
    return np.asarray(rim, dtype=np.float32).ravel() / 255.0 * VRIM_GAIN

# ======================================================================= fog
def wall_distances(zback):
    """8.4: 32 radial wall distances from the player's centre (a box room)."""
    out = []
    for j in range(WALL_SECTORS):
        a = 2 * math.pi * j / WALL_SECTORS
        c, s = math.cos(a), math.sin(a)
        best = 1e9
        for pos, d in ((-ROOM_SIDE, c), (ROOM_RIGHT, c)):
            if (pos < 0 and d < -1e-6) or (pos > 0 and d > 1e-6):
                best = min(best, pos / d)
        for pos, d in ((-zback, s), (ROOM_FRONT, s)):
            if (pos < 0 and d < -1e-6) or (pos > 0 and d > 1e-6):
                best = min(best, pos / d)
        out.append(best)
    return out

def rim_noise(th, t, k):
    t = t * RIM_SPEED
    return 0.55 * np.sin(3 * th + 0.35 * t + k) + 0.30 * np.sin(5 * th - 0.5 * t + 2 * k + 1) + 0.15 * np.sin(8 * th + 0.8 * t + 3 * k)

def layer_mask_table(cam, R, t, k, wd, victims, cx=0.0, cz=0.0, y=0.0, rmax=None, rings=RINGS, sectors=SECTORS,
                     m_in=MASK_IN, m_out=MASK_OUT, walls=True, boost=True, rim=True):
    """Per-vertex alpha mask (without the layer density): radial x rim x wall x near-camera x victim boost."""
    rmax = rmax if rmax is not None else MASK_OUT * R
    ri = np.arange(rings + 1, dtype=np.float32)[:, None] / rings * rmax
    j = np.arange(sectors + 1)
    th = (2 * math.pi * (j % sectors) / sectors).astype(np.float32)[None, :]
    X, Z = cx + ri * np.cos(th), cz + ri * np.sin(th)
    if rim:
        shrink = 1 - RIM_AMP * (0.5 + 0.5 * rim_noise(th, t, k))
    else:
        shrink = 1.0
    e0, e1 = m_in / m_out * rmax * shrink, rmax * shrink
    M = 1 - smooth(e0, e1, ri * np.ones_like(th))
    if walls:
        hit = np.array([wd[(int(q) * WALL_SECTORS // sectors) % WALL_SECTORS] for q in j % sectors],
                       dtype=np.float32)[None, :]
        M = M * (1 - smooth(0.0, 1.0, (ri - hit - WALL_START) / WALL_LEN))
    dist = np.sqrt((X - cam.c[0]) ** 2 + (y - cam.c[1]) ** 2 + (Z - cam.c[2]) ** 2)
    M = M * smooth(NEAR0, NEAR1, dist)
    if boost and victims:
        b = np.zeros_like(M)
        for pos, r in victims:
            d2 = (X - pos[0]) ** 2 + (Z - pos[2]) ** 2
            b += r * np.exp(-d2 / VBOOST_R ** 2)
        M = np.minimum(M * (1 + VBOOST * b), 1.0)
    return M.astype(np.float32)

def disc_pass(S, cam, cx, cz, rmax, y, rings, sectors, M, lay_tex, dens, t, mode, rgb_body=BODY, extra=None,
              amul=1.0, cmul=1.0, hcol=None, hgain=None):
    """Rasterise one polar disc mesh at height y as an analytic plane hit: bilinear per-vertex mask x texture."""
    zb, img = S['zb'], S['img']
    dy = cam.dy
    N = dy.shape[0]
    tf = np.full(N, np.inf, dtype=np.float32)
    m = dy < -1e-6
    tf[m] = (y - cam.c[1]) / dy[m]
    idx = np.nonzero(tf < zb)[0]
    X = cam.c[0] + tf[idx] * cam.dx[idx] - cx
    Z = cam.c[2] + tf[idx] * cam.dz[idx] - cz
    r = np.sqrt(X * X + Z * Z)
    sel = r < rmax
    idx, X, Z, r = idx[sel], X[sel], Z[sel], r[sel]
    if idx.size == 0:
        return
    fr = r / rmax * rings
    i0 = np.minimum(fr.astype(np.int64), rings - 1)
    f1 = (fr - i0).astype(np.float32)
    th = np.mod(np.arctan2(Z, X), 2 * np.pi)
    fs = th / (2 * np.pi) * sectors
    j0 = np.minimum(fs.astype(np.int64), sectors - 1)
    f2 = (fs - j0).astype(np.float32)
    a = M[i0, j0] + (M[i0, j0 + 1] - M[i0, j0]) * f2
    b = M[i0 + 1, j0] + (M[i0 + 1, j0 + 1] - M[i0 + 1, j0]) * f2
    Mi = a + (b - a) * f1
    ang = lay_tex['rot'] * t
    c, s = math.cos(ang), math.sin(ang)
    wx, wz = X + cx, Z + cz                            # world XZ: the texture stays in the world
    u = ((wx * c - wz * s) + lay_tex['scroll'][0] * t) / lay_tex['scale']
    v = ((wx * s + wz * c) + lay_tex['scroll'][1] * t) / lay_tex['scale']
    n = tex_sample(FOG_TEX, u, v)
    if mode == 'body':
        cov = smooth(TEX_LO, TEX_HI, n)
        al = np.clip(dens * amul * Mi * cov, 0, 1)[:, None]
        if rgb_body is BODY:      # v3: slow low-frequency tint, denser parts a little brighter
            tn = tex_sample(FOG_TEX, wx / TINT_SCALE + 0.37 + t * TINT_SPEED, wz / TINT_SCALE + 0.11 - t * TINT_SPEED * 0.7)
            tn = smooth(0.25, 0.75, tn)
            wb = np.clip(1 - 2 * tn, 0, 1)[:, None]
            wm = np.clip(2 * tn - 1, 0, 1)[:, None]
            col = (1 - wb - wm) * np.array(BODY, dtype=np.float32) + wb * np.array(TINT_BLUE, dtype=np.float32) \
                + wm * np.array(TINT_MAG, dtype=np.float32)
            col = col * (1 + TINT_DENSE * cov)[:, None]
        else:
            col = np.array(rgb_body, dtype=np.float32)
        img[idx] = col * cmul * al + img[idx] * (1 - al)
    else:
        cr = smooth(HI_LO, HI_HI, n) * Mi * (HI_GAIN if hgain is None else hgain)
        img[idx] += cr[:, None] * np.array(HI_COL if hcol is None else hcol, dtype=np.float32)

# ---- sprites
def draw_sprite(S, cam, pos, radius, color, alpha, rot, additive=False, soft=1.0):
    p = cam.project(pos)
    if p is None:
        return
    px, py, z = p
    pr = radius * cam.pix_per_unit(z)
    if pr < 0.7:
        return
    # near-camera fade
    dist = float(np.linalg.norm(np.asarray(pos) - cam.c))
    alpha *= float(smooth(NEAR0, NEAR1, dist))
    x0, x1 = max(0, int(px - pr)), min(cam.w, int(px + pr) + 1)
    y0, y1 = max(0, int(py - pr)), min(cam.h, int(py + pr) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    gx, gy = np.meshgrid((np.arange(x0, x1, dtype=np.float32) + 0.5 - px) / pr,
                         (np.arange(y0, y1, dtype=np.float32) + 0.5 - py) / pr)
    rr = np.sqrt(gx * gx + gy * gy)
    c, s = math.cos(rot), math.sin(rot)
    u, v = gx * c - gy * s, gx * s + gy * c
    nz = tex_sample(FOG_TEX, 0.5 + 0.22 * u + rot * 0.13, 0.5 + 0.22 * v)
    prof = (1 - smooth(0.05, 1.0, rr * (0.8 + 0.5 * (nz - 0.5))))
    a = alpha * prof * (0.55 + 0.9 * nz) ** soft
    zb = S['zbimg'][y0:y1, x0:x1]
    a = np.where(z < zb, np.clip(a, 0, 1), 0).astype(np.float32)[..., None]
    reg = S['imgimg'][y0:y1, x0:x1]
    col = np.array(color, dtype=np.float32)
    if additive:
        reg += col * a
    else:
        reg[:] = col * a + reg * (1 - a)

def draw_wisp(S, cam, pos, hw, hh, color, alpha, phase, t):
    """v3 camera-facing vertical tendril: quad of half size (2 hw, hh) around pos; strand width tapers (1 - y^2)^0.8,
    S-curls (sin(2.4 y + phase + 0.6 t) * WISP_CURL), soft edges, vertical fade, noise cell stretched along the strand."""
    p = cam.project(pos)
    if p is None:
        return
    px, py, z = p
    ppu = cam.pix_per_unit(z)
    qx, qy = 2 * hw * ppu, hh * ppu
    if qy < 1.5:
        return
    dist = float(np.linalg.norm(np.asarray(pos) - cam.c))
    alpha *= float(smooth(NEAR0, NEAR1, dist))
    x0, x1 = max(0, int(px - qx)), min(cam.w, int(px + qx) + 1)
    y0, y1 = max(0, int(py - qy)), min(cam.h, int(py + qy) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    gx, gy = np.meshgrid((np.arange(x0, x1, dtype=np.float32) + 0.5 - px) / qx,
                         (np.arange(y0, y1, dtype=np.float32) + 0.5 - py) / qy)
    yy = -gy                                   # +1 top, -1 bottom
    cen = WISP_CURL * 0.5 * np.sin(2.4 * yy + phase + 0.6 * t)
    wid = 0.5 * np.maximum(1 - yy * yy, 0) ** 0.8 + 1e-3
    dxs = np.abs(gx - cen) / wid
    nz = tex_sample(FOG_TEX, 0.5 + 0.35 * gx + phase * 0.1, 0.5 + 0.08 * yy - 0.03 * t + phase * 0.07)
    prof = (1 - smooth(0.15, 1.0, dxs * (0.85 + 0.5 * (nz - 0.5)))) * smooth(-1.0, -0.55, yy) * (1 - smooth(0.3, 1.0, yy))
    a = alpha * prof * (0.55 + 0.9 * nz)
    zb = S['zbimg'][y0:y1, x0:x1]
    a = np.where(z < zb, np.clip(a, 0, 1), 0).astype(np.float32)[..., None]
    reg = S['imgimg'][y0:y1, x0:x1]
    reg[:] = np.array(color, dtype=np.float32) * a + reg * (1 - a)

def wisp_list(R, t):
    out = []
    for i in range(WISP_N):
        life = lerp(WISP_LIFE[0], WISP_LIFE[1], FxRnd(i * 6007 + 5))
        tt = t + FxRnd(i * 15485 + 9) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 197 + n * 1031) & M32
        rr = R * lerp(WISP_R0, WISP_R1, FxRnd(k + 21))
        th0 = 2 * math.pi * FxRnd(k + 22)
        om = lerp(WISP_OMEGA[0], WISP_OMEGA[1], FxRnd(k + 23)) * (1 if i % 2 else -1)
        th = th0 + om * age
        hh = lerp(WISP_HALF_H[0], WISP_HALF_H[1], FxRnd(k + 24))
        hw = lerp(WISP_HALF_W[0], WISP_HALF_W[1], FxRnd(k + 25))
        y = lerp(WISP_Y0[0], WISP_Y0[1], FxRnd(k + 26)) + WISP_RISE * age + hh
        env = float(smooth(0.0, 0.25, f) * (1 - smooth(0.6, 1.0, f)))
        col = WISP_COLS[int(FxRnd(k + 27) * 3) % 3]
        out.append(((rr * math.cos(th), y, rr * math.sin(th)), hw, hh, col, WISP_ALPHA * env, FxRnd(k + 28) * 6.28))
    return out

def world_puffs(R, t):
    out = []
    for i in range(PUFF_N):
        life = lerp(PUFF_LIFE[0], PUFF_LIFE[1], FxRnd(i * 7919 + 1))
        off = FxRnd(i * 104729 + 3) * life
        tt = t + off
        n = int(tt // life)
        age = tt - n * life
        k = (i * 131 + n * 977) & M32
        rr = R * lerp(PUFF_R0, PUFF_R1, FxRnd(k + 11))
        th0 = 2 * math.pi * FxRnd(k + 12)
        om = (0.28 + 0.3 * FxRnd(k + 13)) * (1 if FxRnd(k + 14) > 0.5 else -1)
        th = th0 + om * age
        rr *= 1 - 0.10 * age / life
        f = age / life
        env = float(smooth(0.0, 0.2, f) * (1 - smooth(0.55, 1.0, f)))
        size = lerp(PUFF_SIZE[0], PUFF_SIZE[1], FxRnd(k + 15)) * (0.7 + 0.6 * f)
        out.append(((rr * math.cos(th), 10 + PUFF_RISE * age, rr * math.sin(th)), size, PUFF_ALPHA * env, FxRnd(k + 16) * 6.28))
    return out

def render_fog(S, cam, R, t, victims, zback):
    global FOG_TEX
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS)]
    # 8.1 body layers (far to near: the camera is above all of them), the victim slabs after the lowest
    disc_pass(S, cam, 0, 0, rmax, LAYERS[0]['y'], RINGS, SECTORS, tables[0], LAYERS[0], LAYERS[0]['dens'], t, 'body')
    for vi, (pos, r) in enumerate(victims):
        rad = lerp(VSLAB_R[0], VSLAB_R[1], r) * (0.5 + 0.5 * r)
        Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                              sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
        # (the slab has no angular-rim modulation shrink beyond the mask: the mesh ends at rad)
        disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS[1], VSLAB_DENS * (0.5 + 0.5 * r), t, 'body')
    for k in range(1, len(LAYERS)):
        disc_pass(S, cam, 0, 0, rmax, LAYERS[k]['y'], RINGS, SECTORS, tables[k], LAYERS[k], LAYERS[k]['dens'], t, 'body')
    # 8.2 violet highlights, additive
    for k in HI_LAYERS:
        disc_pass(S, cam, 0, 0, rmax, LAYERS[k]['y'], RINGS, SECTORS, tables[k], LAYERS[k], 1.0, t, 'add')
    # 8.3 world puffs (far to near)
    pf = world_puffs(R, t)
    pf.sort(key=lambda p: -cam.project(p[0])[2] if cam.project(p[0]) else 0)
    for pos, size, al, rot in pf:
        draw_sprite(S, cam, pos, size, PUFF_COL, al, rot)
    # v3 raised wisps (far to near)
    wl = wisp_list(R, t)
    wl.sort(key=lambda w: -cam.project(w[0])[2] if cam.project(w[0]) else 0)
    for pos, hw, hh, col, al, ph in wl:
        draw_wisp(S, cam, pos, hw, hh, col, al, ph, t)
    # victims: bone-bound puffs and motes
    for vi, (pos, r) in enumerate(victims):
        b = victim_bones(pos, t, vi * 1.7)
        npuff = VPUFF_N[0] + int(round(VPUFF_N[1] * r))
        items = []
        for q in range(npuff):
            bi = (q * 5 + vi) % 12
            k = (vi * 977 + q * 31) & M32
            ph = t * (0.35 + 0.3 * FxRnd(k + 1)) + 6.28 * FxRnd(k + 2)
            rise = 14 * ((t * 0.25 + FxRnd(k + 3)) % 1.0)
            off = (10 * math.cos(ph), rise + 4 * math.sin(ph * 0.7), 10 * math.sin(ph))
            p = (b[bi][0] + off[0], b[bi][1] + off[1], b[bi][2] + off[2])
            z = cam.project(p)
            if z:
                items.append((z[2], p, k))
        items.sort(key=lambda it: -it[0])
        for z, p, k in items:
            size = VPUFF_SIZE_R[0] + VPUFF_SIZE_R[1] * r
            al = VPUFF_ALPHA * (0.6 + 1.4 * r) * (0.75 + 0.25 * math.sin(t * 1.7 + k))
            draw_sprite(S, cam, p, size, VPUFF_COL, min(al, 0.6), FxRnd(k + 5) * 6.28)
        for bi in range(12):                     # v3 additive glints at the bones, flickering, ~r^2
            fl = 0.5 + 0.5 * math.sin(t * (2.1 + 0.37 * bi) + 1.9 * bi + vi)
            draw_sprite(S, cam, b[bi], 8.0 + 7.0 * r, VGLOW_COL, VGLOW_A * (0.10 + 0.9 * r * r) * (0.4 + 0.6 * fl), 0,
                        additive=True, soft=0.3)
        if r > 0.5:
            nm = MOTE_N[0] + int(round(MOTE_N[1] * (r - 0.5) / 0.5))
            for q in range(nm):
                k = (vi * 6151 + q * 53) & M32
                life = 1.4 + 0.8 * FxRnd(k + 1)
                tt = t + FxRnd(k + 2) * life
                n = int(tt // life)
                age = tt - n * life
                kk = (k + n * 977) & M32
                bi = int(FxRnd(kk + 3) * 12) % 12
                f = age / life
                p = (b[bi][0] + 8 * math.sin(age * 2 + kk), b[bi][1] + 50 * age, b[bi][2] + 8 * math.cos(age * 2.3 + kk))
                env = float(smooth(0, 0.15, f) * (1 - smooth(0.4, 1.0, f)))
                draw_sprite(S, cam, p, 4.0 + 3.0 * FxRnd(kk + 4), MOTE_COL, MOTE_ALPHA * env * (r - 0.1), 0, additive=True,
                            soft=0.3)

# ======================================================================= frames
def render_frame(R=None, t=0.0, victims=(), zback=ROOM_BACK, bright=0.0, fog=True, ss=SS):
    global FOG_TEX, GRIME_TEX
    if FOG_TEX is None:
        FOG_TEX = make_tex(128, 1, 4, TEX_WARP)
        GRIME_TEX = make_tex(128, 7, 5, 0.0, base=3)
    cam = Cam(ss)
    rgb, zb = render_scene(cam, zback, bright)
    vs = [(p, r) for p, r in victims]
    rim = draw_victims(cam, rgb, zb, vs, t)
    S = dict(img=rgb, zb=zb)
    if fog:
        render_fog(S, cam, R, t, vs, zback)
    if rim is not None:
        S['img'] += rim[:, None] * np.array(VRIM_COL, dtype=np.float32)
    img = np.clip(S['img'].reshape(cam.h, cam.w, 3), 0, 1)
    im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    return im.resize((W, H), Image.BOX) if ss > 1 else im

V1 = (220 * math.sin(math.radians(-35)), 0.0, -220 * math.cos(math.radians(-35)))
V2 = (330 * math.sin(math.radians(30)), 0.0, -330 * math.cos(math.radians(30)))

FRAMES = [   # name, label, kwargs
    ('weak_t2', 'Weak R=300  t=2 s', dict(R=R_WEAK, t=2.0)),
    ('strong_t0', 'Strong R=400  t=0 s', dict(R=R_STRONG, t=0.0)),
    ('strong_t1', 'Strong R=400  t=1 s', dict(R=R_STRONG, t=1.0)),
    ('strong_t2', 'Strong R=400  t=2 s', dict(R=R_STRONG, t=2.0)),
    ('strong_t3', 'Strong R=400  t=3 s', dict(R=R_STRONG, t=3.0)),
    ('strong_victims', 'Strong  victims 220 u (r=0.3), 330 u (r=0.9)', dict(R=R_STRONG, t=2.0, victims=((V1, 0.3), (V2, 0.9)))),
    ('strong_wall', 'Strong  player 120 u from the back wall', dict(R=R_STRONG, t=2.0, zback=120.0)),
    ('strong_grey', 'Strong  mid-grey scene  t=2 s', dict(R=R_STRONG, t=2.0, bright=0.62)),
    ('scene_nofog', 'Reference: scene without fog', dict(fog=False)),
]

def make_sheet(names, labels, outdir, cols=3, tw=640):
    th = tw * H // W
    bar = 30
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, (n, lab) in enumerate(zip(names, labels)):
        fn = os.path.join(outdir, ('%s.png' % n) if n == 'scene_nofog' else ('fog_%s.png' % n))
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), lab, fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_sheet.png'))

def main_v3():
    global OUT
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    os.makedirs(OUT, exist_ok=True)
    for name, label, kw in FRAMES:
        if only and name not in only:
            continue
        im = render_frame(**kw)
        im.save(os.path.join(OUT, ('%s.png' % name) if name == 'scene_nofog' else ('fog_%s.png' % name)))
        print('wrote', name, flush=True)
    sn = [os.path.join(OUT, 'fog_strong_t%d.png' % k) for k in range(4)]
    if all(os.path.exists(f) for f in sn):       # 2 x 2 motion strip, t = 0, 1, 2, 3 s
        strip = Image.new('RGB', (W, H))
        for k, f in enumerate(sn):
            strip.paste(Image.open(f).convert('RGB').resize((W // 2, H // 2), Image.LANCZOS), ((k % 2) * W // 2, (k // 2) * H // 2))
        strip.save(os.path.join(OUT, 'fog_strong_strip.png'))
    make_sheet([f[0] for f in FRAMES], [f[1] for f in FRAMES], OUT)
    print('sheet done')

# ======================================================================= v4 (v3.27.1 art iteration, DESIGN_DRAFT sections A-D)
# Run without --v3 to render v4 (default out: staging/v3271_dp_art/renders). `--v3` runs the unchanged v3 frames.
OUT4 = os.path.join(ROOT, 'staging', 'v3271_dp_art', 'renders')
CAMS = {
    'high': dict(pos=(0.0, 300.0, 450.0), target=(0.0, 50.0, 0.0), fov=60.0),       # the v3 camera
    'ots':  dict(pos=(0.0, 170.0, 260.0), target=(0.0, 125.0, 0.0), fov=55.0),      # over the shoulder: 260 behind, 170 up
    'side': dict(pos=(450.0, 90.0, 0.0), target=(0.0, 90.0, 0.0), fov=55.0),        # hip height, 450 to the side
}
V4_LAMP_I = 2.8                              # brighter lamp than v3 (1.5) so the smoke has something to read against
V4_NEAR = (20.0, 90.0)                       # near-camera fade of every fog sprite / vertex (was 60 / 250)
SIZE_FADE = (0.4, 1.0)                       # a sprite fades out while camera distance / radius < 1.0 (zero at 0.4)
# A. volume
VOL_N, VOL_RAD, VOL_H, VOL_MINY = 150, (70.0, 150.0), 110.0, 0.6
VOL_LIFE = (9.0, 16.0)
VOL_OMEGA = (0.04, 0.10)                     # rad/s orbit round her, sense alternates by slot parity
VOL_BOB = 10.0                               # u, bob amplitude (0.5 rad/s)
VOL_ALPHA = 0.30
VOL_TEX = (0.35, 1.3)                         # alpha texture: a * (VOL_TEX[0] + VOL_TEX[1] * smoothstep(0.15, 0.75, fbm))
VOL_GLOW = 0.35                              # violet crest light inside each volume sprite (additive, x HI_COL, x alpha / VOL_ALPHA)
VOL_BRIGHT = 1.5                             # body colour gain of the volume sprites (tint colours of v3 x this)
VOL_FALL_R = (0.25, 1.0)                     # density falls  1 -> 0 over these fractions of R
VOL_FALL_H = (40.0, 190.0)                   # and -45 % with centre height over these u
LAYERS4 = (dict(y=4.0, dens=0.85, scale=380.0, scroll=(14.0, 6.0), rot=+0.045),
           dict(y=30.0, dens=0.35, scale=260.0, scroll=(-9.0, 12.0), rot=-0.060))
WISP4_HALF_H = (40.0, 62.0)                  # v3: 34-52
# B. her aura
HALO_COL = (0.003, 0.001, 0.007)
HALO_A = (0.45, 0.60)                        # Weak, Strong
HALO_PUSH = (18.0, 25.0)
HALO_R = dict(head=26, neck=22, chest=34, pelvis=32, lSh=24, rSh=24, lElb=22, rElb=22, lHand=22, rHand=22,
              lHip=26, rHip=26, lKnee=24, rKnee=24, lFoot=22, rFoot=22)
AURA_COL = (0.42, 0.13, 0.80)
AURA_N = (10, 16)                            # tendrils Weak, Strong
AURA_PTS, AURA_TAIL = 28, 8
AURA_RAD, AURA_OM, AURA_A, AURA_LIFE = (18.0, 30.0), (0.6, 1.2), (0.20, 0.35), (3.0, 5.5)
AURA_TRAIL = (40.0, 80.0)
AURA_PT_GAIN = 0.8
AURA_SORT_BIAS = 80.0                        # tendrils sort this many u nearer than their true depth (they stay readable through fog)
# C. victims
V_CLING_N = (12, 22)                         # a + round(b s)
V_CLING_R = (20.0, 22.0)
V_CLING_A = 0.40
V_CLING_COL = (0.060, 0.017, 0.105)
V_HALO_A = 0.55
V_GLINT_A = 0.45
V7 = False                                   # v7 switches (set by main7)
V6 = False                                   # v6 switches (victim_items etc.); set by main6
V_HALO_CORE, V_HALO_MUL, V_HALO_MID, V_FRINGE = 0.35, 1.0, False, 0.0   # v5 overrides these (v4 values here)
V_TEND_N = (3, 8)
# D. filters
SV_COL, SV_VOL_A, SV_VEIN_COL, SV_VEIN_N, SV_VEIN_PTS = (0.55, 0.15, 1.0), 0.035, (0.80, 0.40, 1.0), 40, 34
SV_VEIN_STEP, SV_VEIN_R, SV_VEIN_A = 5.5, 1.7, 0.45
SV_POOL = (0.12, 0.08)                        # additive gain of the 4 u / 30 u floor layers in SV
SV_LIFT, SV_GAMMA, SV_DESAT = 1.1, 0.9, 0.6
NV_CMUL, NV_AMUL, NV_GAIN = 0.3, 1.3, 3.6
TH_ENV, TH_PLAYER = 0.9, 0.90
V4A, V4B, V4C = (-100.0, 0.0, -170.0), (120.0, 0.0, -310.0), (-270.0, 0.0, -380.0)
VICT4 = ((V4A, 1.0), (V4B, 0.6), (V4C, 0.25))

class params:
    """temporarily override module constants (so v3 stays reproducible)"""
    def __init__(self, **kw):
        self.kw = kw
    def __enter__(self):
        g = globals()
        self.old = {k: g[k] for k in self.kw}
        g.update(self.kw)
    def __exit__(self, *a):
        globals().update(self.old)

# ---- player skeleton
PBONES = ('head', 'neck', 'chest', 'pelvis', 'lSh', 'rSh', 'lElb', 'rElb', 'lHand', 'rHand', 'lHip', 'rHip', 'lKnee', 'rKnee',
          'lFoot', 'rFoot')
PEDGES = (('head', 'neck', 11), ('neck', 'chest', 13), ('chest', 'pelvis', 16), ('neck', 'lSh', 7), ('neck', 'rSh', 7),
          ('lSh', 'lElb', 5.5), ('lElb', 'lHand', 4.5), ('rSh', 'rElb', 5.5), ('rElb', 'rHand', 4.5),
          ('pelvis', 'lHip', 9), ('pelvis', 'rHip', 9), ('lHip', 'lKnee', 8.5), ('lKnee', 'lFoot', 6.5),
          ('rHip', 'rKnee', 8.5), ('rKnee', 'rFoot', 6.5))
PAXES = (('pelvis', 'chest'), ('chest', 'neck'), ('lSh', 'lElb'), ('lElb', 'lHand'), ('rSh', 'rElb'), ('rElb', 'rHand'),
         ('lHip', 'lKnee'), ('lKnee', 'lFoot'), ('rHip', 'rKnee'), ('rKnee', 'rFoot'), ('neck', 'head'))

def player_bones(t, walk=False):
    """16 world-space bones of a standing figure facing -z (away from the OTS camera) in a gentle walk cycle (v6: walk=True strides
    faster and twice as wide)."""
    s = math.sin(t * (5.5 if walk else 2.2)) * (2.0 if walk else 1.0)
    b = dict(head=(0, 158, -2), neck=(0, 144, 0), chest=(0, 124, 1), pelvis=(0, 94, 0),
             lSh=(-19, 140, 0), rSh=(19, 140, 0), lElb=(-23, 113, 3 + 7 * s), rElb=(23, 113, 3 - 7 * s),
             lHand=(-22, 88, 8 + 13 * s), rHand=(22, 88, 8 - 13 * s),
             lHip=(-10, 91, 0), rHip=(10, 91, 0), lKnee=(-11, 50, 8 * s), rKnee=(11, 50, -8 * s),
             lFoot=(-11, 4, 15 * s), rFoot=(11, 4, -15 * s))
    return {k: np.array(v, dtype=np.float64) for k, v in b.items()}

def draw_bones(cam, rgb, zb, segs, col):
    """flat-shaded thick capsules (painter's order by depth); writes colour and z where nearer than the scene"""
    zi = Image.new('F', (cam.w, cam.h), 1e9)
    ci = Image.new('RGB', (cam.w, cam.h), (0, 0, 0))
    dz_, dc = ImageDraw.Draw(zi), ImageDraw.Draw(ci)
    items = []
    for pa, pb, rad in segs:
        a, b = cam.project(pa), cam.project(pb)
        if a and b:
            items.append(((a[2] + b[2]) / 2, a, b, rad))
    items.sort(key=lambda s: -s[0])
    c8 = tuple(int(255 * q) for q in col)
    for z, a, b, rad in items:
        wpx = max(2.0, 2 * rad * cam.pix_per_unit(z))
        dc.line([a[0], a[1], b[0], b[1]], fill=c8, width=int(wpx))
        dz_.line([a[0], a[1], b[0], b[1]], fill=z, width=int(wpx))
        for p in (a, b):
            rr = wpx / 2
            dc.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=c8)
            dz_.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=z)
    za = np.asarray(zi, dtype=np.float32).ravel()
    ca = np.asarray(ci, dtype=np.float32).reshape(-1, 3) / 255.0
    m = (za < 1e8) & (za < zb)
    rgb[m] = ca[m]
    zb[m] = za[m]

def draw_player(cam, rgb, zb, pb, col):
    segs = [(pb[a], pb[b], r) for a, b, r in PEDGES]
    segs.append((pb['head'], pb['head'], 13.0))
    draw_bones(cam, rgb, zb, segs, col)

# ---- sprites
def tint_col(wx, wz, t):
    """v3 low-frequency tint of the fog body at a world XZ point"""
    u = np.array([wx / TINT_SCALE + 0.37 + t * TINT_SPEED], dtype=np.float32)
    v = np.array([wz / TINT_SCALE + 0.11 - t * TINT_SPEED * 0.7], dtype=np.float32)
    tn = float(smooth(0.25, 0.75, tex_sample(FOG_TEX, u, v))[0])
    wb, wm = max(0.0, 1 - 2 * tn), max(0.0, 2 * tn - 1)
    return (1 - wb - wm) * np.array(BODY) + wb * np.array(TINT_BLUE) + wm * np.array(TINT_MAG)

def draw_smoke(S, cam, pos, radius, color, alpha, rot, additive=False, size_fade=True, core=0.0, glow=0.0, clip_top=None):
    """FXX_SMOKE: soft round puff with two-octave internal fBm, camera facing; near fade NEAR0..NEAR1, size fade, z test"""
    p = cam.project(pos)
    if p is None:
        return
    px, py, z = p
    pr = radius * cam.pix_per_unit(z)
    if pr < 1.2:
        return
    dist = float(np.linalg.norm(np.asarray(pos) - cam.c))
    alpha *= float(smooth(NEAR0, NEAR1, dist))
    if size_fade:
        alpha *= float(smooth(SIZE_FADE[0], SIZE_FADE[1], dist / radius))
    if alpha < 0.004:
        return
    x0, x1 = max(0, int(px - pr)), min(cam.w, int(px + pr) + 1)
    y0, y1 = max(0, int(py - pr)), min(cam.h, int(py + pr) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    zb = S['zbimg'][y0:y1, x0:x1]
    gx, gy = np.meshgrid((np.arange(x0, x1, dtype=np.float32) + 0.5 - px) / pr,
                         (np.arange(y0, y1, dtype=np.float32) + 0.5 - py) / pr)
    rr = np.sqrt(gx * gx + gy * gy)
    c, s = math.cos(rot), math.sin(rot)
    u, v = gx * c - gy * s, gx * s + gy * c
    n1 = tex_sample(FOG_TEX, 0.5 + 0.20 * u + rot * 0.13, 0.5 + 0.20 * v)
    n2 = tex_sample(FOG_TEX, 0.5 + 0.55 * u + rot * 0.31 + 0.37, 0.5 + 0.55 * v + 0.21)
    n = 0.65 * n1 + 0.35 * n2
    prof = (1 - smooth(core, 1.0, rr * (0.82 + 0.6 * (n - 0.5)))) * (1 - smooth(0.75, 1.0, rr))
    a = alpha * prof * (VOL_TEX[0] + VOL_TEX[1] * smooth(0.15, 0.75, n))
    if clip_top is not None:      # v7 shoulder clip: alpha x clamp((top - h) / 20, 0, 1), h = world height of the pixel on the sprite plane
        a = a * np.clip((clip_top - (pos[1] - gy * radius * cam.u[1])) / SHCLIP_FADE, 0, 1)
    vis = z < zb
    a = np.where(vis, np.clip(a, 0, 1), 0).astype(np.float32)[..., None]
    reg = S['imgimg'][y0:y1, x0:x1]
    col = np.array(color, dtype=np.float32)
    if additive:
        reg += col * a
    else:
        reg[:] = col * a + reg * (1 - a)
        if glow > 0:                                  # faint violet crest light inside the smoke
            g = (glow * alpha / max(VOL_ALPHA, 1e-3) * prof * smooth(0.6, 0.95, n) * vis).astype(np.float32)[..., None]
            reg += np.array(HI_COL, dtype=np.float32) * g

def away(cam, p, d):
    v = np.asarray(p, dtype=np.float64) - cam.c
    return np.asarray(p, dtype=np.float64) + v / (np.linalg.norm(v) + 1e-6) * d

# ---- A. volume
def volume_list(R, t):
    out = []
    for i in range(VOL_N):
        life = lerp(VOL_LIFE[0], VOL_LIFE[1], FxRnd(i * 8191 + 7))
        tt = t + FxRnd(i * 92821 + 13) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 271 + n * 1237) & M32
        rad = lerp(VOL_RAD[0], VOL_RAD[1], FxRnd(k + 31))
        r0 = R * 0.97 * math.sqrt(FxRnd(k + 32))
        th = 2 * math.pi * FxRnd(k + 34) + lerp(VOL_OMEGA[0], VOL_OMEGA[1], FxRnd(k + 35)) * (1 if i % 2 else -1) * age
        ph = 6.28 * FxRnd(k + 36)
        rr = r0 * (1 + 0.05 * math.sin(0.3 * t + ph))
        hmax = VOL_H * math.sqrt(max(0.0, 1 - (r0 / R) ** 2))
        y = max(VOL_MINY * rad, hmax * FxRnd(k + 33) ** 1.2) + VOL_BOB * math.sin(0.5 * t + ph)
        y = max(y, VOL_MINY * rad)
        env = float(smooth(0.0, 0.2, f) * (1 - smooth(0.7, 1.0, f)))
        dens = (1 - float(smooth(VOL_FALL_R[0], VOL_FALL_R[1], rr / R))) * (1 - 0.45 * float(smooth(VOL_FALL_H[0], VOL_FALL_H[1], y)))
        size = rad * (0.9 + 0.2 * f)
        out.append(((rr * math.cos(th), y, rr * math.sin(th)), size, VOL_ALPHA * env * dens, FxRnd(k + 37) * 6.28))
    return out

# ---- B. her aura
def halo_items(cam, S, pb, t, strong, col, amul=1.0):
    items = []
    for bi, name in enumerate(PBONES):
        push = lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(bi * 31 + 7))
        p = away(cam, pb[name], push)
        al = HALO_A[1 if strong else 0] * (0.85 + 0.15 * math.sin(0.9 * t + bi * 1.3)) * amul
        z = cam.project(p)
        if z:
            items.append((z[2], partial(draw_smoke, S, cam, p, HALO_R[name], col, al, bi * 0.7, size_fade=False, core=0.35)))
    return items

def tendril_items(cam, S, pb, t, strong, gain=1.0):
    items = []
    n = AURA_N[1 if strong else 0]
    for i in range(n):
        life = lerp(AURA_LIFE[0], AURA_LIFE[1], FxRnd(i * 4973 + 3))
        tt = t + FxRnd(i * 8221 + 5) * life
        cyc = int(tt // life)
        age = tt - cyc * life
        f = age / life
        k = (i * 389 + cyc * 1723) & M32
        a_, b_ = PAXES[int(FxRnd(k + 1) * len(PAXES)) % len(PAXES)]
        A, B = pb[a_], pb[b_]
        d = B - A
        dn = d / np.linalg.norm(d)
        e1 = np.cross(dn, (1, 0, 0) if abs(dn[0]) < 0.9 else (0, 0, 1))
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(dn, e1)
        rad = lerp(AURA_RAD[0], AURA_RAD[1], FxRnd(k + 2))
        sgn = 1 if FxRnd(k + 4) > 0.5 else -1
        om = lerp(AURA_OM[0], AURA_OM[1], FxRnd(k + 3)) * sgn
        ph = 6.28 * FxRnd(k + 5) + om * age
        s0, span, turns = 0.40 * FxRnd(k + 6), 0.60, 0.9
        env = float(smooth(0.0, 0.15, f) * (1 - smooth(0.7, 1.0, f)))
        al = lerp(AURA_A[0], AURA_A[1], FxRnd(k + 7)) * env * gain * AURA_PT_GAIN
        trail = (i % 3 == 0)
        last = None
        for q in range(AURA_PTS):
            u = q / (AURA_PTS - 1)
            th = ph + sgn * 2 * math.pi * turns * u
            rr = rad * (1 + 0.12 * math.sin(3 * u + 0.8 * t + i))
            p = A + d * (s0 + span * u) + rr * (math.cos(th) * e1 + math.sin(th) * e2)
            tap = math.sin(math.pi * u) ** 0.6
            last = (p, th, rr, tap)
            z = cam.project(p)
            if z:
                items.append((z[2] - AURA_SORT_BIAS, partial(draw_sprite, S, cam, p, 3.4 + 3.0 * tap, AURA_COL, al * tap, 0.0,
                                            additive=True, soft=0.3)))
        if trail:
            p0, th0, rr0, _ = last
            tl = lerp(AURA_TRAIL[0], AURA_TRAIL[1], FxRnd(k + 8)) * float(smooth(0.2, 0.9, f))
            for q in range(AURA_TAIL):
                v = (q + 1) / AURA_TAIL
                th = th0 + sgn * 1.2 * v
                rr = rr0 * (1 - 0.6 * v)
                p = p0 + np.array((0.0, tl * v, 0.0)) + (rr - rr0) * (math.cos(th0) * e1 + math.sin(th0) * e2) \
                    + 6 * v * np.array((math.cos(th), 0, math.sin(th)))
                z = cam.project(p)
                if z:
                    items.append((z[2] - AURA_SORT_BIAS, partial(draw_sprite, S, cam, p, 4.0 * (1 - 0.4 * v), AURA_COL, al * (1 - v) ** 1.2, 0.0,
                                                additive=True, soft=0.3)))
    return items

# ---- C. victims
HALO_VR = dict(head=20, neck=14, chest=30, pelvis=28, lSh=20, rSh=20, lHand=15, rHand=15, lKnee=20, rKnee=20, lFoot=16, rFoot=16)

def victim_items(cam, S, vi, pos, s, t, mode):
    items = []
    b = victim_bones(pos, t, vi * 1.7)
    dark = mode in ('normal', 'nv', 'th')
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (1.0, 1.0)
    if V6 and mode == 'th':
        cm = TH6_CM
    gmul = V6_VGLOW if (V6 and mode == 'sv') else 1.0
    if mode == 'sv':                                 # violet spirit glow in proportion to s
        for bi in range(12):
            q = 0.5 + 0.5 * math.sin(t * 1.3 + bi * 1.7 + vi)
            z = cam.project(b[bi])
            if z:
                items.append((z[2], partial(draw_smoke, S, cam, b[bi], 22.0 + 10 * s, SV_COL, 0.10 * gmul * s * (0.6 + 0.4 * q), bi * .7,
                                            additive=True, size_fade=False, core=0.1)))
    if dark:
        n = V_CLING_N[0] + int(round(V_CLING_N[1] * s))
        for q in range(n):                          # dark smoke clinging to the bones
            bi = (q * 5 + vi) % 12
            k = (vi * 977 + q * 31) & M32
            ph = t * (0.35 + 0.3 * FxRnd(k + 1)) + 6.28 * FxRnd(k + 2)
            rise = 16 * ((t * 0.25 + FxRnd(k + 3)) % 1.0)
            p = (b[bi][0] + 11 * math.cos(ph), b[bi][1] + rise + 4 * math.sin(ph * 0.7), b[bi][2] + 11 * math.sin(ph))
            z = cam.project(p)
            if z:
                al = V_CLING_A * (0.25 + 0.95 * s) * (0.75 + 0.25 * math.sin(t * 1.7 + k)) * am
                items.append((z[2], partial(draw_smoke, S, cam, p, V_CLING_R[0] + V_CLING_R[1] * s, tuple(c * cm for c in V_CLING_COL),
                                            min(al, 0.6), FxRnd(k + 5) * 6.28, size_fade=False)))
        for bi, name in enumerate(BONES):           # the black halo trick, pushed behind the bone
            if name not in HALO_VR:
                continue
            p = away(cam, b[bi], lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(bi * 17 + vi)))
            z = cam.project(p)
            if z:
                al = V_HALO_A * (0.2 + 0.8 * s) * (0.85 + 0.15 * math.sin(0.9 * t + bi * 1.3 + vi)) * am
                hr = HALO_VR[name] * (0.8 + 0.3 * s) * V_HALO_MUL
                if V_FRINGE > 0:
                    items.append((z[2] + 4, partial(draw_smoke5, S, cam, p, hr * 1.55, tuple(c * (cm if V6 else 1.0) for c in FRINGE_COL), V_FRINGE * al, bi * .5,
                                                    size_fade=False, core=0.2, plain=True)))
                items.append((z[2], partial(draw_smoke, S, cam, p, hr, HALO_COL, al, bi * .5,
                                            size_fade=False, core=V_HALO_CORE)))
        if V_HALO_MID:                              # v5: sprites on the bone segment midpoints, rim continuous
            for ei, (a, c, rad) in enumerate(EDGES):
                p = away(cam, (np.array(b[a]) + np.array(b[c])) * 0.5, lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(ei * 29 + vi)))
                z = cam.project(p)
                if z:
                    al = V_HALO_A * (0.2 + 0.8 * s) * (0.85 + 0.15 * math.sin(0.9 * t + ei * 1.1 + vi)) * am
                    items.append((z[2], partial(draw_smoke, S, cam, p, (10 + 1.4 * rad) * (0.8 + 0.3 * s), HALO_COL, al * 0.9, ei * .5,
                                                size_fade=False, core=V_HALO_CORE)))
    if mode in ('normal', 'sv') or (V6 and mode == 'nv'):
        gm = 1.5 * gmul if mode == 'sv' else (V6_NVGL if mode == 'nv' else 1.0)
        gcol = (NV7_COL if V7 else tuple(c * 0.8 for c in NV6_GLINT)) if mode == 'nv' else VGLOW_COL
        for ei, (a, c, rad) in enumerate(EDGES):    # violet glints running along the bone segments
            for q in range(2):
                k = (vi * 613 + ei * 17 + q) & M32
                u = (t * 0.35 + FxRnd(k)) % 1.0
                p = np.array(b[a]) * (1 - u) + np.array(b[c]) * u
                fl = math.sin(math.pi * u)
                z = cam.project(p)
                if z and V7:       # v7: short filament along the bone, strength = max(old, 0.35 smoothstep(0.02, 0.15, s))
                    strength = max(0.08 + 0.92 * s * s, V7_GMIN * float(smooth(0.02, 0.15, s)))
                    dv = np.array(b[c]) - np.array(b[a])
                    dv = dv / (np.linalg.norm(dv) + 1e-9)
                    N = 7
                    tc = cam.c - p
                    pf = p + tc / (np.linalg.norm(tc) + 1e-9) * V7_GLIFT       # lifted off the bone toward the camera, so the body capsule does not hide it
                    pts = [pf + dv * (V7_GLEN * (q / (N - 1) - 0.5)) for q in range(N)]
                    al = V_GLINT_A * gm * strength * fl * V7_GALPHA
                    prof_ = [math.sin(math.pi * (0.04 + 0.92 * q / (N - 1))) ** 0.8 for q in range(N)]
                    zz = z[2] - (AURA_SORT_BIAS if mode == 'nv' else 0.0)
                    hc = tuple(c_ * 0.5 for c_ in gcol)
                    items.append((zz + 0.1, partial(draw_strip, S, cam, pts, [V7_GHW * 3.0] * N, [al * 0.30 * q_ for q_ in prof_], hc, 2.0)))
                    items.append((zz, partial(draw_strip, S, cam, pts, [V7_GHW] * N, [al * q_ for q_ in prof_], gcol, 1.0)))
                elif z:
                    items.append((z[2] - (AURA_SORT_BIAS if (V6 and mode == 'nv') else 0.0), partial(draw_sprite, S, cam, p, 4.5 + 4 * s, gcol,
                                                V_GLINT_A * gm * (0.08 + 0.92 * s * s) * fl, 0, additive=True, soft=0.3)))
        if s > 0.3 and mode != 'nv':                # motes
            nm = MOTE_N[0] + int(round(MOTE_N[1] * (s - 0.3) / 0.7))
            for q in range(nm):
                k = (vi * 6151 + q * 53) & M32
                life = 1.4 + 0.8 * FxRnd(k + 1)
                tt = t + FxRnd(k + 2) * life
                n = int(tt // life)
                age = tt - n * life
                kk = (k + n * 977) & M32
                bi = int(FxRnd(kk + 3) * 12) % 12
                f = age / life
                p = (b[bi][0] + 8 * math.sin(age * 2 + kk), b[bi][1] + 50 * age, b[bi][2] + 8 * math.cos(age * 2.3 + kk))
                env = float(smooth(0, 0.15, f) * (1 - smooth(0.4, 1.0, f)))
                z = cam.project(p)
                if z:
                    items.append((z[2], partial(draw_sprite, S, cam, p, 4.0 + 3.0 * FxRnd(kk + 4), MOTE_COL,
                                                MOTE_ALPHA * env * (s - 0.1), 0, additive=True, soft=0.3)))
    if dark and s > 0.5:                            # dark tendrils rising off shoulders / hands / head
        nt = V_TEND_N[0] + int(round(V_TEND_N[1] * (s - 0.5) / 0.5))
        for q in range(nt):
            k = (vi * 733 + q * 41) & M32
            life = 2.4 + 1.0 * FxRnd(k + 1)
            tt = t + FxRnd(k + 2) * life
            n = int(tt // life)
            age = tt - n * life
            f = age / life
            bi = (0, 4, 5, 6, 7, 1)[int(FxRnd(k + n * 31 + 3) * 6) % 6]
            for j in range(7):
                v = j / 6.0
                p = (b[bi][0] + 7 * math.sin(3 * v + age * 2 + q), b[bi][1] + 10 + 70 * f * (0.3 + 0.7 * v), b[bi][2] + 7 * math.cos(3 * v + age * 2 + q))
                z = cam.project(p)
                if z:
                    al = 0.22 * s * float(smooth(0, 0.2, f) * (1 - smooth(0.6, 1, f))) * (1 - 0.5 * v) * am
                    items.append((z[2], partial(draw_smoke, S, cam, p, 7 + 6 * v, tuple(c * cm for c in VPUFF_COL), al, q + j,
                                                size_fade=False)))
    return items

# ---- D. SV veins
def vein_items(cam, S, R, t):
    items = []
    for i in range(SV_VEIN_N):
        life = lerp(3.0, 6.0, FxRnd(i * 3571 + 1))
        tt = t + FxRnd(i * 7127 + 2) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 211 + n * 1511) & M32
        rr = R * 0.8 * math.sqrt(FxRnd(k + 1))
        th = 2 * math.pi * FxRnd(k + 2)
        p = np.array((rr * math.cos(th), lerp(15.0, 90.0, FxRnd(k + 3)), rr * math.sin(th)))
        d = np.array((FxRnd(k + 4) - 0.5, 0.35 * (FxRnd(k + 5) - 0.3), FxRnd(k + 6) - 0.5))
        d /= np.linalg.norm(d)
        env = float(smooth(0.0, 0.2, f) * (1 - smooth(0.6, 1.0, f)))
        for q in range(SV_VEIN_PTS):
            z = cam.project(p)
            if z:
                pulse = 0.65 + 0.35 * math.sin(0.9 * q - 4.0 * t + i)
                items.append((z[2], partial(draw_sprite, S, cam, p, SV_VEIN_R, SV_VEIN_COL, SV_VEIN_A * env * pulse, 0, additive=True, soft=0.3)))
            d = d + 0.55 * np.array((FxRnd(k + 10 + q) - 0.5, 0.5 * (FxRnd(k + 40 + q) - 0.5), FxRnd(k + 70 + q) - 0.5))
            d /= np.linalg.norm(d)
            p = p + d * SV_VEIN_STEP
            if p[1] < 8:
                p[1] = 8
                d[1] = abs(d[1])
    return items

# ---- the v4 fog
def render_fog4(S, cam, R, t, victims, zback, mode, strong, pb):
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (1.0, 1.0)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS4)]
    for k in (0, 1):                                  # the floor pool (4 u) and the weaker 30 u layer
        L = LAYERS4[k]
        if mode == 'sv':
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tables[k], L, 1.0, t, 'add', hcol=SV_COL, hgain=SV_POOL[k])
        else:
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tables[k], L, L['dens'], t, 'body', amul=am, cmul=cm)
            if k == 1 and mode == 'normal':           # v3 violet crest highlights, on the 30 u layer only
                disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tables[k], L, 1.0, t, 'add')
    if mode != 'sv':
        for vi, (pos, s) in enumerate(victims):       # victim floor slab, scaled by the stain
            rad = lerp(VSLAB_R[0], VSLAB_R[1], s) * (0.5 + 0.5 * s)
            Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                                  sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
            disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS4[1], VSLAB_DENS * (0.5 + 0.5 * s), t, 'body', amul=am, cmul=cm)
    items = []
    for pos, size, al, rot in volume_list(R, t):      # A. the volume
        z = cam.project(pos)
        if not z:
            continue
        if mode == 'sv':
            items.append((z[2], partial(draw_smoke, S, cam, pos, size, SV_COL, SV_VOL_A * al / VOL_ALPHA, rot, additive=True, core=0.0)))
        else:
            col = tint_col(pos[0], pos[2], t) * VOL_BRIGHT * cm
            items.append((z[2], partial(draw_smoke, S, cam, pos, size, col, min(al * am, 0.9), rot, glow=VOL_GLOW if mode == 'normal' else 0.0)))
    if mode != 'sv':                                  # wisps (a little taller)
        for pos, hw, hh, col, al, ph in wisp_list(R, t):
            z = cam.project(pos)
            if z:
                items.append((z[2], partial(draw_wisp, S, cam, pos, hw, hh, tuple(c * cm for c in col), al * am, ph, t)))
    else:
        items += vein_items(cam, S, R, t)
    if pb is not None:                                # B. her aura
        if mode in ('normal', 'nv'):
            items += halo_items(cam, S, pb, t, strong, tuple(c * cm for c in HALO_COL), am if mode == 'nv' else 1.0)
        if mode in ('normal', 'sv'):
            items += tendril_items(cam, S, pb, t, strong, 1.2 if mode == 'sv' else 1.0)
        if mode == 'sv':                              # spirit glow round the torso
            for name, rad in (('chest', 40), ('pelvis', 36), ('head', 26)):
                z = cam.project(pb[name])
                if z:
                    items.append((z[2], partial(draw_smoke, S, cam, away(cam, pb[name], 10), rad, SV_COL, 0.10, 1.0, additive=True,
                                                size_fade=False, core=0.1)))
    for vi, (pos, s) in enumerate(victims):           # C. victims
        if s > 0.02:
            items += victim_items(cam, S, vi, pos, s, t, mode)
    items.sort(key=lambda it: -it[0])
    for z, fn in items:
        fn()

# ---- post filters (on the finished frame, float rgb H x W x 3)
def _lum(a):
    return a @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)

def _blur(a, px):
    im = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(px)), dtype=np.float32) / 255.0

def filter_sv(img):
    """Spirit Vision: desaturate (less where the pixel is violet), lift, pale edge glow of the luminance"""
    lum = _lum(img)
    vio = np.clip((img[..., 0] + img[..., 2]) * 0.5 - img[..., 1], 0, 1)
    w = (SV_DESAT * (1 - smooth(0.04, 0.30, vio)))[..., None]
    grey = lum[..., None] * np.array([0.80, 0.95, 1.15], dtype=np.float32)
    out = img * (1 - w) + grey * w
    out = np.clip(out * SV_LIFT, 0, 1) ** SV_GAMMA
    l2 = _lum(_blur(out, 1.0))
    gx = np.zeros_like(l2)
    gy = np.zeros_like(l2)
    gx[:, 1:-1] = l2[:, 2:] - l2[:, :-2]
    gy[1:-1] = l2[2:] - l2[:-2]
    e = np.clip(np.sqrt(gx * gx + gy * gy) * 5.0, 0, 1)
    glow = _blur(np.repeat(e[..., None], 3, 2), 2.0)[..., 0]
    out = out + (np.clip(e * 0.7 + glow * 0.8, 0, 1))[..., None] * np.array([0.40, 0.55, 0.95], dtype=np.float32)
    return np.clip(out, 0, 1)

HEAT = ((0.0, (0, 0, 0)), (0.12, (0.02, 0.0, 0.25)), (0.30, (0.20, 0.0, 0.55)), (0.50, (0.75, 0.10, 0.35)),
        (0.70, (1.0, 0.45, 0.05)), (0.88, (1.0, 0.90, 0.20)), (1.0, (1, 1, 1)))

def filter_th(img):
    """Thermal: luminance (= heat) through the heat palette; dark = cold blue / black"""
    L = np.clip(_lum(img), 0, 1)
    xs = np.array([h[0] for h in HEAT])
    return np.stack([np.interp(L, xs, [h[1][c] for h in HEAT]) for c in range(3)], axis=-1).astype(np.float32)

def filter_nv(img):
    """Night vision: luminance gain + lift, green phosphor, grain, vignette"""
    L = np.clip(_lum(img) * NV_GAIN, 0, 1) ** 0.6
    rng = np.random.RandomState(5)
    L = np.clip(L * (1 + 0.10 * (rng.rand(*L.shape).astype(np.float32) - 0.5)), 0, 1)
    gy, gx = np.mgrid[0:L.shape[0], 0:L.shape[1]].astype(np.float32)
    vg = 1 - 0.27 * (((gx / L.shape[1] - 0.5) * 2) ** 2 + ((gy / L.shape[0] - 0.5) * 2) ** 2)
    L = L * vg
    return np.stack([L * 0.30, L * 1.0, L * 0.38], axis=-1).astype(np.float32)

def render_frame4(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True, v5=False):
    global FOG_TEX, GRIME_TEX
    ss = ss or SS
    if FOG_TEX is None:
        FOG_TEX = make_tex(128, 1, 4, TEX_WARP)
        GRIME_TEX = make_tex(128, 7, 5, 0.0, base=3)
    with params(NEAR0=V4_NEAR[0], NEAR1=V4_NEAR[1], WISP_HALF_H=WISP4_HALF_H, LAMP_I=V4_LAMP_I):
        c = Cam(ss, CAMS[cam])
        rgb, zb = render_scene(c, zback, 0.0, player=False)
        if mode == 'th':                              # environment heat: a dim function of the lit surface
            rgb = (np.clip(_lum(rgb) * 1.8, 0, 1) ** 0.8 * TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
        vs = [(p, s) for p, s in victims]
        pb = player_bones(t) if player else None
        if player:
            pc = (TH_PLAYER,) * 3 if mode == 'th' else (0.30, 0.26, 0.27)
            draw_player(c, rgb, zb, pb, pc)
        cols = [(0.85 * (1 - 0.7 * s),) * 3 for p, s in vs] if mode == 'th' else None
        rim = draw_victims(c, rgb, zb, vs, t, cols=cols, rim=(mode in ('normal', 'sv')))
        S = dict(img=rgb, zb=zb)
        (render_fog5 if v5 else render_fog4)(S, c, R, t, vs, zback, mode, R >= 350, pb)
        if rim is not None:
            S['img'] += rim[:, None] * np.array(SV_COL if mode == 'sv' else VRIM_COL, dtype=np.float32)
    img = np.clip(S['img'].reshape(c.h, c.w, 3), 0, 1)
    im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    im = im.resize((W, H), Image.BOX) if ss > 1 else im
    f = np.asarray(im, dtype=np.float32) / 255.0
    f = {'sv': filter_sv, 'th': filter_th, 'nv': filter_nv}.get(mode, lambda a: a)(f)
    return Image.fromarray((np.clip(f, 0, 1) * 255 + 0.5).astype(np.uint8))

FRAMES4 = [   # name, label, kwargs
    ('weak_ots', 'Weak R=300  OTS  t=2 s', dict(R=R_WEAK, t=2.0, cam='ots')),
    ('strong_ots', 'Strong R=400  OTS  t=2 s', dict(R=R_STRONG, t=2.0, cam='ots')),
    ('weak_side', 'Weak R=300  side (hip height)', dict(R=R_WEAK, t=2.0, cam='side')),
    ('strong_side', 'Strong R=400  side (hip height)', dict(R=R_STRONG, t=2.0, cam='side')),
    ('weak_high', 'Weak R=300  high camera', dict(R=R_WEAK, t=2.0, cam='high')),
    ('strong_high', 'Strong R=400  high camera', dict(R=R_STRONG, t=2.0, cam='high')),
    ('strong_ots_t0', 'Strong OTS t=0 s', dict(R=R_STRONG, t=0.0, cam='ots')),
    ('strong_ots_t1', 'Strong OTS t=1 s', dict(R=R_STRONG, t=1.0, cam='ots')),
    ('strong_ots_t2', 'Strong OTS t=2 s', dict(R=R_STRONG, t=2.0, cam='ots')),
    ('strong_ots_t3', 'Strong OTS t=3 s', dict(R=R_STRONG, t=3.0, cam='ots')),
    ('victims', 'Victims: s=1 inside, s=0.6 (3 s after), s=0.25 (8 s after)  Weak OTS',
     dict(R=R_WEAK, t=2.0, cam='ots', victims=VICT4, zback=560.0)),
    ('sv_ots', 'Spirit Vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='sv', zback=560.0)),
    ('th_ots', 'Thermal  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='th', zback=560.0)),
    ('nv_ots', 'Night vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='nv', zback=560.0)),
]
SHEET4 = ['weak_ots', 'strong_ots', 'strong_ots_strip', 'weak_side', 'strong_side', 'victims', 'weak_high', 'strong_high',
          'sv_ots', 'th_ots', 'nv_ots']
SHEET4_LAB = {n: l for n, l, _ in FRAMES4}
SHEET4_LAB['strong_ots_strip'] = 'Motion strip: Strong OTS t = 0, 1, 2, 3 s'

def make_sheet4(outdir, cols=3, tw=640):
    th = tw * H // W
    bar = 30
    rows = (len(SHEET4) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(SHEET4):
        fn = os.path.join(outdir, 'v4_%s.png' % n)
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), SHEET4_LAB[n], fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_v4_sheet.png'))

def main4():
    global OUT, OUT4
    if '--v3' in sys.argv:
        sys.argv.remove('--v3')
        return main_v3()
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT4 = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    os.makedirs(OUT4, exist_ok=True)
    import time
    for name, label, kw in FRAMES4:
        if only and name not in only:
            continue
        t0 = time.time()
        render_frame4(**kw).save(os.path.join(OUT4, 'v4_%s.png' % name))
        print('wrote', name, '%.0fs' % (time.time() - t0), flush=True)
    sn = [os.path.join(OUT4, 'v4_strong_ots_t%d.png' % k) for k in range(4)]
    if all(os.path.exists(f) for f in sn):
        strip = Image.new('RGB', (W, H))
        for k, f in enumerate(sn):
            strip.paste(Image.open(f).convert('RGB').resize((W // 2, H // 2), Image.LANCZOS), ((k % 2) * W // 2, (k // 2) * H // 2))
        strip.save(os.path.join(OUT4, 'v4_strong_ots_strip.png'))
    make_sheet4(OUT4)
    print('sheet done')

# ======================================================================= v5 (v4 + lead's critique 1-6; default; `--v4` / `--v3` run the older frames)
OUT5 = os.path.join(ROOT, 'staging', 'v3271_dp_art', 'renders')
CAMS['ots_low'] = dict(pos=(0.0, 140.0, 200.0), target=(0.0, 105.0, 0.0), fov=55.0)   # closer and lower: 200 behind, 140 up
# per intensity: Weak is low / thin / gappy, Strong tall / dense / churning
P5 = {
    False: dict(N=66, RAD=(90.0, 160.0), H=50.0, MINY=0.42, DENS=1.15, THIN=0.35, DENSE=0.04, CHURN=0.7, BOB=7.0,
                T_N=7, T_H=(60.0, 90.0), T_A=0.70, HALO_A=0.70, FLOOR=(0.55, 0.18)),
    True:  dict(N=96, RAD=(110.0, 210.0), H=190.0, MINY=0.50, DENS=1.35, THIN=0.22, DENSE=0.18, CHURN=1.8, BOB=16.0,
                T_N=18, T_H=(60.0, 100.0), T_A=1.00, HALO_A=1.00, FLOOR=(0.85, 0.30)),
}
VOL5_ALPHA = 0.62                              # base peak alpha of a puff (x its hashed density d, x env, x falloffs)
VOL5_TEX = (0.36, 0.56, 0.04, 1.9)            # alpha = a * (V3 + V4 * smoothstep(V1, V2, n)): holes inside each puff
VOL5_LOBE = (0.62, 1.00)                       # profile radius rr x (V1 + V2 (n - 0.5)): lobed outline
VOL5_EDGE = (0.45, 1.0)                        # profile = 1 - smoothstep(0.25, 1.0, rr') times (1 - smoothstep(0.8, 1, rr))
VOL5_SHADE = (0.55, 1.00)                      # colour x (V1 + V2 n): billow shading
TUN = dict(r0=(0.10, 15.0), r1=(0.60, 45.0), core=(40.0, 170.0), core_min=0.45)   # clear tunnel
TUN_AIM = ((0.0, 150.0, 0.0), (0.0, 92.0, 0.0), (0.0, 12.0, 0.0))                 # eye rays: head, pelvis, feet
HOLE5 = (25.0, 85.0, 0.85)                     # floor discs: density x (1 - 0.85 (1 - smoothstep(25, 85, r))) round her
HALO5_RMUL, HALO5_CORE, HALO5_MID = 1.35, 0.55, (14.0, 1.6)      # black glow: radius x1.35, flat core 55 %, midpoint r = 14 + 1.6 rad
FRINGE_COL, FRINGE_A5 = (0.085, 0.025, 0.200), (0.20, 0.30)       # faint violet outer fringe (premultiplied), Weak / Strong
HALO5_BIAS = 60.0
SHCLIP_FADE = 20.0                              # v7 shoulder clip fade length (u)
TEND5 = dict(PTS=38, TURNS=(0.8, 1.5), R0=(12.0, 20.0), RGROW=1.4, LIFE=(3.5, 6.5), HW=(0.9, 1.4), MINPX=1.2, ALPHA=(0.8, 1.3),
             DASH=(2.5, 4.5), COL=(0.52, 0.17, 0.98), HALO_COL=(0.30, 0.09, 0.62), HALO_X=3.0, HALO_A=0.30)
# SV: palette to violet-blue, floor pool cut, veins as continuous tapered strips
SV5 = dict(COL=(0.30, 0.30, 0.95), VEIN=(0.55, 0.65, 1.0), VEIN_HALO=(0.25, 0.28, 0.90), POOL=(0.030, 0.018), DESAT=0.75,
           PTS=30, STEP=7.0, TURN=0.30, HW=0.55, A=0.55)
ATT5 = PAXES + (('neck', 'lSh'), ('neck', 'rSh'))

def draw_smoke5(S, cam, pos, radius, color, alpha, rot, additive=False, size_fade=True, core=0.0, glow=0.0, plain=False, floor=False, cell6=False, clip_top=None, stretch=None):
    """FXX_SMOKE5 billow cell: higher contrast than FXX_SMOKE. n = 0.55 fbm(0.20 uv) + 0.45 fbm(0.70 uv); outline radius
    rr' = rr (0.70 + 0.75 (n-0.5)); profile (1 - smoothstep(core, 1, rr')) (1 - smoothstep(0.8, 1, rr)); alpha x (0.15 + 1.55
    smoothstep(0.22, 0.62, n)); colour x (0.70 + 0.60 n). plain=True: soft round (no holes, for the violet fringe)."""
    p = cam.project(pos)
    if p is None:
        return
    px, py, z = p
    pr = radius * cam.pix_per_unit(z)
    if pr < 1.2:
        return
    dist = float(np.linalg.norm(np.asarray(pos) - cam.c))
    alpha *= float(smooth(NEAR0, NEAR1, dist))
    if size_fade:
        alpha *= float(smooth(SIZE_FADE[0], SIZE_FADE[1], dist / radius))
    if alpha < 0.004:
        return
    pb_ = pr * (stretch[2] if stretch else 1.0)       # v7: bounding box of a sprite stretched along the screen direction (dx, dy) by k
    x0, x1 = max(0, int(px - pb_)), min(cam.w, int(px + pb_) + 1)
    y0, y1 = max(0, int(py - pb_)), min(cam.h, int(py + pb_) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    zb = S['zbimg'][y0:y1, x0:x1]
    gx, gy = np.meshgrid((np.arange(x0, x1, dtype=np.float32) + 0.5 - px) / pr,
                         (np.arange(y0, y1, dtype=np.float32) + 0.5 - py) / pr)
    gy0 = gy
    if stretch:
        sdx, sdy, sk = stretch
        al_ = (gx * sdx + gy * sdy) / sk
        pe_ = -gx * sdy + gy * sdx
        gx, gy = al_, pe_
    rr = np.sqrt(gx * gx + gy * gy)
    c, s = math.cos(rot), math.sin(rot)
    u, v = gx * c - gy * s, gx * s + gy * c
    n1 = tex_sample(FOG_TEX, 0.5 + 0.34 * u + rot * 0.13, 0.5 + 0.34 * v)
    n2 = tex_sample(FOG_TEX, 0.5 + 1.00 * u + rot * 0.31 + 0.37, 0.5 + 0.70 * v + 0.21)
    n = 0.55 * n1 + 0.45 * n2
    vis = z < zb
    if plain:
        prof = (1 - smooth(core, 1.0, rr)) * (1 - smooth(0.8, 1.0, rr))
        a = alpha * prof
    else:
        prof = (1 - smooth(VOL5_EDGE[0] if core == 0.0 else core, 1.0, rr * (VOL5_LOBE[0] + VOL5_LOBE[1] * (n - 0.5)))) * (1 - smooth(0.8, 1.0, rr))
        if cell6:      # v6 halo cell: no holes, alpha = lobed profile x (0.85 + 0.15 n)
            a = alpha * prof * (V6_CELL[0] + V6_CELL[1] * n)
        else:
            a = alpha * prof * (VOL5_TEX[2] + VOL5_TEX[3] * smooth(VOL5_TEX[0], VOL5_TEX[1], n))
        if floor:      # no hard cut where the billboard dips below the floor: alpha x smoothstep(0, 35, world height of the pixel)
            a = a * smooth(0.0, 35.0, pos[1] - gy * radius * cam.u[1])
    if clip_top is not None:        # v7 shoulder clip: alpha x clamp((top - h) / 20, 0, 1), h = world height of the pixel on the sprite plane
        a = a * np.clip((clip_top - (pos[1] - gy0 * radius * cam.u[1])) / SHCLIP_FADE, 0, 1)
    a = np.where(vis, np.clip(a, 0, 1), 0).astype(np.float32)[..., None]
    reg = S['imgimg'][y0:y1, x0:x1]
    col = np.array(color, dtype=np.float32)
    if not plain and core == 0.0 and not additive:
        col = col * (VOL5_SHADE[0] + VOL5_SHADE[1] * n)[..., None]
    if additive:
        reg += col * a
    else:
        reg[:] = col * a + reg * (1 - a)
        if glow > 0 and not plain:
            g = (glow * alpha / max(VOL5_ALPHA, 1e-3) * prof * smooth(0.6, 0.95, n) * vis).astype(np.float32)[..., None]
            reg += np.array(HI_COL, dtype=np.float32) * g

def draw_strip(S, cam, pts, hw, alpha, color, minpx=0.9):
    """Camera-facing strand: a polyline of world points (pts), world half width hw[i], alpha[i] per vertex. In game: one quad per
    segment, 2 vertices per point offset +-w along the screen-space perpendicular (like svveins.inc strips), vertex alpha, ONE/ONE.
    Cross profile 1 - smoothstep(0.15, 1, |d|/w); half width = max(minpx px at 720 lines, hw x pixels per unit); max-merged inside
    one strand (joints do not double up); z tested per pixel; near fade NEAR0..NEAR1 per vertex."""
    P = []
    for i, p in enumerate(pts):
        pr = cam.project(p)
        if pr is None:
            P.append(None)
            continue
        ppu = cam.pix_per_unit(pr[2])
        d = float(np.linalg.norm(np.asarray(p) - cam.c))
        P.append((pr[0], pr[1], pr[2], max(minpx * cam.ss, hw[i] * ppu), alpha[i] * float(smooth(NEAR0, NEAR1, d))))
    ok = [q for q in P if q]
    if len(ok) < 2:
        return
    reg_all = S['imgimg']
    pad = max(q[3] for q in ok) + 2
    bx0, bx1 = int(max(0, min(q[0] for q in ok) - pad)), int(min(cam.w, max(q[0] for q in ok) + pad + 1))
    by0, by1 = int(max(0, min(q[1] for q in ok) - pad)), int(min(cam.h, max(q[1] for q in ok) + pad + 1))
    if bx0 >= bx1 or by0 >= by1:
        return
    buf = np.zeros((by1 - by0, bx1 - bx0), dtype=np.float32)
    zbimg = S['zbimg']
    for i in range(len(P) - 1):
        a, b = P[i], P[i + 1]
        if not a or not b or (a[4] < 0.003 and b[4] < 0.003):
            continue
        hmax = max(a[3], b[3]) + 1
        x0, x1 = int(max(bx0, min(a[0], b[0]) - hmax)), int(min(bx1, max(a[0], b[0]) + hmax + 1))
        y0, y1 = int(max(by0, min(a[1], b[1]) - hmax)), int(min(by1, max(a[1], b[1]) + hmax + 1))
        if x0 >= x1 or y0 >= y1:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1, dtype=np.float32) + 0.5, np.arange(y0, y1, dtype=np.float32) + 0.5)
        ex, ey = b[0] - a[0], b[1] - a[1]
        L2 = ex * ex + ey * ey + 1e-6
        tt = np.clip(((gx - a[0]) * ex + (gy - a[1]) * ey) / L2, 0, 1)
        d = np.sqrt((gx - a[0] - tt * ex) ** 2 + (gy - a[1] - tt * ey) ** 2)
        wl = a[3] + (b[3] - a[3]) * tt
        al = a[4] + (b[4] - a[4]) * tt
        v = al * (1 - smooth(0.15, 1.0, d / wl))
        zz = a[2] + (b[2] - a[2]) * tt
        v = np.where(zz < zbimg[y0:y1, x0:x1], v, 0)
        sub = buf[y0 - by0:y1 - by0, x0 - bx0:x1 - bx0]
        np.maximum(sub, v, out=sub)
    reg_all[by0:by1, bx0:bx1] += np.clip(buf, 0, 1)[..., None] * np.array(color, dtype=np.float32)

# ---- A. volume (v5)
def volume_list5(R, t, strong):
    Q = P5[strong]
    out = []
    for i in range(Q['N']):
        life = lerp(VOL_LIFE[0], VOL_LIFE[1], FxRnd(i * 8191 + 7)) / Q['CHURN']
        tt = t + FxRnd(i * 92821 + 13) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 271 + n * 1237) & M32
        rad = lerp(Q['RAD'][0], Q['RAD'][1], FxRnd(k + 31))
        r0 = R * Q.get('RMAX', 0.97) * math.sqrt(FxRnd(k + 32))
        om = lerp(VOL_OMEGA[0], VOL_OMEGA[1], FxRnd(k + 35)) * Q['CHURN'] * (1 if i % 2 else -1)
        th = 2 * math.pi * FxRnd(k + 34) + om * age * Q['CHURN'] ** 0 * 1.0 * (1.0)
        ph = 6.28 * FxRnd(k + 36)
        rr = r0 * (1 + 0.05 * Q['CHURN'] * math.sin(0.3 * Q['CHURN'] * t + ph))
        hmax = Q['H'] * math.sqrt(max(0.0, 1 - (r0 / R) ** 2))
        y = max(Q['MINY'] * rad, hmax * FxRnd(k + 33) ** 1.2) + Q['BOB'] * math.sin(0.5 * Q['CHURN'] * t + ph)
        y = max(y, Q['MINY'] * rad)
        env = float(smooth(0.0, 0.2, f) * (1 - smooth(0.7, 1.0, f)))
        h = FxRnd(k + 41)                            # hashed density: thin / normal / dense
        if h < Q['THIN']:
            d = lerp(0.10, 0.38, FxRnd(k + 42))
        elif h > 1 - Q['DENSE']:
            d = lerp(1.5, 2.0, FxRnd(k + 42))
        else:
            d = lerp(0.70, 1.10, FxRnd(k + 42))
        dens = (1 - float(smooth(VOL_FALL_R[0], VOL_FALL_R[1], rr / R))) * (1 - 0.45 * float(smooth(VOL_FALL_H[0], VOL_FALL_H[1], y)))
        size = rad * (0.9 + 0.2 * f)
        if 'TOPFADE' in Q:     # v5b: thin out puffs whose top is high, keeps the wall above ~150 u readable
            dens *= 1 - Q['TOPFADE'][2] * float(smooth(Q['TOPFADE'][0], Q['TOPFADE'][1], y + 0.6 * size))
        out.append(((rr * math.cos(th), y, rr * math.sin(th)), size, VOL5_ALPHA * Q['DENS'] * env * dens * d, FxRnd(k + 37) * 6.28))
    return out

def seg_dist(p, a, b):
    ab = b - a
    t = float(np.clip(np.dot(p - a, ab) / (np.dot(ab, ab) + 1e-9), 0, 1))
    return float(np.linalg.norm(p - (a + ab * t)))

def tunnel(cam, pos, size):
    """clear tunnel: min over 3 eye rays (eye -> head / pelvis / feet) of the puff's distance to the segment; alpha x
    smoothstep(0.25 size + 25, 0.90 size + 50, d); plus a lighter core: alpha x (core_min + (1 - core_min) smoothstep(50, 200, horiz. d to her axis))"""
    p = np.asarray(pos, dtype=np.float64)
    d = min(seg_dist(p, cam.c, np.array(a)) for a in TUN_AIM)
    f = float(smooth(TUN['r0'][0] * size + TUN['r0'][1], TUN['r1'][0] * size + TUN['r1'][1], d))
    hd = math.hypot(p[0], p[2])
    c = TUN['core_min'] + (1 - TUN['core_min']) * float(smooth(TUN['core'][0], TUN['core'][1], hd))
    return f * c

# ---- B. her aura (v5)
def halo_items5(cam, S, pb, t, strong, col, amul=1.0):
    items = []
    Ha = P5[strong]['HALO_A'] * 0.62 * 1.0     # base 0.62 -> Weak 0.43, Strong 0.62... (see NOTES_v5: HALO5_A)
    Ha = HALO5_A[1 if strong else 0]
    fr = FRINGE_A5[1 if strong else 0]
    def add(p, rad, bi, al):
        z = cam.project(p)
        if not z:
            return
        items.append((z[2] - HALO5_BIAS + 4, partial(draw_smoke5, S, cam, p, rad * 1.55, FRINGE_COL, fr * al / Ha * amul, bi * 0.7 + 3,
                                                     size_fade=False, core=0.2, plain=True)))
        items.append((z[2] - HALO5_BIAS, partial(draw_smoke5, S, cam, p, rad, col, al * amul, bi * 0.7, size_fade=False,
                                                 core=HALO5_CORE)))
    for bi, name in enumerate(PBONES):
        push = lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(bi * 31 + 7))
        hp = away(cam, pb[name], push)
        hp[1] = max(hp[1], 0.75 * HALO_R[name] * HALO5_RMUL)
        add(hp, HALO_R[name] * HALO5_RMUL, bi, Ha * (0.85 + 0.15 * math.sin(0.9 * t + bi * 1.3)))
    for ei, (a, b, rad) in enumerate(PEDGES):
        mid = (pb[a] + pb[b]) * 0.5
        push = lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(ei * 53 + 11))
        add(away(cam, mid, push), HALO5_MID[0] + HALO5_MID[1] * rad, ei + 20, Ha * 0.9 * (0.85 + 0.15 * math.sin(0.9 * t + ei * 1.7)))
    return items
HALO5_A = (0.55, 0.80)                           # black glow alpha, Weak / Strong (v4: 0.45 / 0.60)

def tendril_items5(cam, S, pb, t, strong, gain=1.0, colset=None):
    Q, T = P5[strong], TEND5
    col = colset[0] if colset else T['COL']
    hcol = colset[1] if colset else T['HALO_COL']
    items = []
    for i in range(Q['T_N']):
        life = lerp(T['LIFE'][0], T['LIFE'][1], FxRnd(i * 4973 + 3))
        tt = t + FxRnd(i * 8221 + 5) * life
        cyc = int(tt // life)
        age = tt - cyc * life
        f = age / life
        k = (i * 389 + cyc * 1723) & M32
        a_, b_ = ATT5[int(FxRnd(k + 1) * len(ATT5)) % len(ATT5)]
        A, B = pb[a_], pb[b_]
        d = B - A
        dn = d / np.linalg.norm(d)
        e1 = np.cross(dn, (1, 0, 0) if abs(dn[0]) < 0.9 else (0, 0, 1))
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(dn, e1)
        r0 = lerp(T['R0'][0], T['R0'][1], FxRnd(k + 2))
        sgn = 1 if FxRnd(k + 4) > 0.5 else -1
        om = 0.5 + 0.5 * FxRnd(k + 3)
        ph = 6.28 * FxRnd(k + 5) + sgn * om * age
        turns = lerp(T['TURNS'][0], T['TURNS'][1], FxRnd(k + 6))
        s0 = 0.15 + 0.7 * FxRnd(k + 7)
        H = lerp(Q['T_H'][0], Q['T_H'][1], FxRnd(k + 8))
        nd = lerp(T['DASH'][0], T['DASH'][1], FxRnd(k + 9))
        dph = FxRnd(k + 10) + (0.35 * t if FxRnd(k + 11) > 0.5 else -0.35 * t)
        env = float(smooth(0.0, 0.15, f) * (1 - smooth(0.65, 1.0, f)))
        base = lerp(T['ALPHA'][0], T['ALPHA'][1], FxRnd(k + 12)) * Q['T_A'] * gain * env
        grow = float(smooth(0.0, 0.45, f)) * 0.6 + 0.4                      # the tip extends while it is young
        pts, hws, als = [], [], []
        N = T['PTS']
        for q in range(N):
            u = q / (N - 1)
            ug = u * grow
            th = ph + sgn * 2 * math.pi * turns * ug
            rr = r0 * (1 + T['RGROW'] * float(smooth(0.2, 1.0, ug))) * (1 + 0.12 * math.sin(3 * u + 0.8 * t + i))
            rise = H * ug ** 1.6
            p = A + d * (s0 + 0.25 * ug) + rr * (math.cos(th) * e1 + math.sin(th) * e2) + np.array((0.0, rise, 0.0))
            dash = float(smooth(0.30, 0.55, 0.5 + 0.5 * math.sin(2 * math.pi * (ug * nd + dph))))
            bri = 0.55 + 0.45 * math.sin(7 * ug + 1.3 * t + i * 2.1)
            tip = (1 - ug) ** 1.3 * float(smooth(0.0, 0.08, ug))
            pts.append(p)
            hws.append(T['HW'][0] + (T['HW'][1] - T['HW'][0]) * math.sin(math.pi * ug) ** 0.5 * (1 - 0.5 * ug))
            als.append(base * dash * bri * tip)
        z = cam.project(pts[N // 2])
        if z:
            zz = z[2] - AURA_SORT_BIAS
            items.append((zz + 0.1, partial(draw_strip, S, cam, pts, [h * T['HALO_X'] for h in hws], [a * T['HALO_A'] for a in als], hcol, 1.0 * 2.2)))
            items.append((zz, partial(draw_strip, S, cam, pts, hws, als, col, T['MINPX'])))
    return items

# ---- D. SV veins as continuous strips
def vein_items5(cam, S, R, t):
    V = SV5
    items = []
    for i in range(SV_VEIN_N):
        life = lerp(3.0, 6.0, FxRnd(i * 3571 + 1))
        tt = t + FxRnd(i * 7127 + 2) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 211 + n * 1511) & M32
        rr = R * 0.8 * math.sqrt(FxRnd(k + 1))
        th = 2 * math.pi * FxRnd(k + 2)
        p = np.array((rr * math.cos(th), lerp(15.0, 90.0, FxRnd(k + 3)), rr * math.sin(th)))
        d = np.array((FxRnd(k + 4) - 0.5, 0.35 * (FxRnd(k + 5) - 0.3), FxRnd(k + 6) - 0.5))
        d /= np.linalg.norm(d)
        env = float(smooth(0.0, 0.2, f) * (1 - smooth(0.6, 1.0, f)))
        pts, hws, als = [], [], []
        for q in range(V['PTS']):
            u = q / (V['PTS'] - 1)
            pulse = 0.65 + 0.35 * math.sin(0.9 * q - 4.0 * t + i)
            pts.append(p.copy())
            hws.append(V['HW'] * math.sin(math.pi * (0.05 + 0.95 * u)) ** 0.7)
            als.append(V['A'] * env * pulse * (1 - u) ** 0.5)
            d = d + V['TURN'] * np.array((FxRnd(k + 10 + q) - 0.5, 0.5 * (FxRnd(k + 40 + q) - 0.5), FxRnd(k + 70 + q) - 0.5))
            d /= np.linalg.norm(d)
            p = p + d * V['STEP']
            if p[1] < 8:
                p[1] = 8
                d[1] = abs(d[1])
        z = cam.project(pts[len(pts) // 2])
        if z:
            items.append((z[2] + 0.1, partial(draw_strip, S, cam, pts, [h * 3.2 for h in hws], [a * 0.22 for a in als], V['VEIN_HALO'], 1.8)))
            items.append((z[2], partial(draw_strip, S, cam, pts, hws, als, V['VEIN'], 1.0)))
    return items

def render_fog5(S, cam, R, t, victims, zback, mode, strong, pb):
    Q = P5[strong]
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (1.0, 1.0)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS4)]
    ri = np.linspace(0, rmax, RINGS + 1, dtype=np.float32)[:, None]
    hole = 1 - HOLE5[2] * (1 - smooth(HOLE5[0], HOLE5[1], ri))
    for k in (0, 1):
        L = LAYERS4[k]
        tab = tables[k] * hole
        if mode == 'sv':
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add', hcol=SV_COL, hgain=SV_POOL[k])
        else:
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, Q['FLOOR'][k], t, 'body', amul=am, cmul=cm)
            if k == 1 and mode == 'normal':
                disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add')
    if mode != 'sv':
        for vi, (pos, s) in enumerate(victims):
            rad = lerp(VSLAB_R[0], VSLAB_R[1], s) * (0.5 + 0.5 * s)
            Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                                  sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
            disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS4[1], VSLAB_DENS * (0.5 + 0.5 * s), t, 'body', amul=am, cmul=cm)
    items = []
    for pos, size, al, rot in volume_list5(R, t, strong):
        z = cam.project(pos)
        if not z:
            continue
        if mode == 'sv':
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, SV_COL, SV_VOL_A * al / VOL5_ALPHA * 0.6, rot, additive=True)))
        else:
            al = al * tunnel(cam, pos, size)
            col = tint_col(pos[0], pos[2], t) * VOL_BRIGHT * cm
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, col, min(al * am, 0.9), rot, glow=VOL_GLOW if mode == 'normal' else 0.0, floor=True)))
    if mode != 'sv':
        for pos, hw, hh, col, al, ph in wisp_list(R, t)[: (24 if not strong else 48)]:
            z = cam.project(pos)
            if z:
                items.append((z[2], partial(draw_wisp, S, cam, pos, hw, hh, tuple(c * cm for c in col), al * am * (0.6 if not strong else 1.0), ph, t)))
    else:
        items += vein_items5(cam, S, R, t)
    if pb is not None:
        if mode in ('normal', 'nv'):
            items += halo_items5(cam, S, pb, t, strong, tuple(c * cm for c in HALO_COL), am if mode == 'nv' else 1.0)
        if mode in ('normal', 'sv'):
            cs = (SV5['VEIN'], SV5['VEIN_HALO']) if mode == 'sv' else None
            items += tendril_items5(cam, S, pb, t, strong, 1.2 if mode == 'sv' else 1.0, cs)
        if mode == 'sv':
            for name, rad in (('chest', 40), ('pelvis', 36), ('head', 26)):
                z = cam.project(pb[name])
                if z:
                    items.append((z[2], partial(draw_smoke, S, cam, away(cam, pb[name], 10), rad, SV_COL, 0.10, 1.0, additive=True,
                                                size_fade=False, core=0.1)))
    for vi, (pos, s) in enumerate(victims):
        if s > 0.02:
            items += victim_items(cam, S, vi, pos, s, t, mode)
    items.sort(key=lambda it: -it[0])
    for z, fn in items:
        fn()

# v5b: Strong retuned (Weak unchanged); selected by default, `--v5` runs the v5 Strong numbers and writes v5_*
P5B_STRONG = dict(N=80, RAD=(70.0, 130.0), H=125.0, MINY=0.45, DENS=0.90, THIN=0.30, DENSE=0.05, RMAX=0.70,
                  TOPFADE=(100.0, 230.0, 0.70), FLOOR=(0.55, 0.18))
PFX = 'v5b'
V5_OVERRIDES = dict(V_HALO_CORE=0.55, V_HALO_MUL=1.35, V_HALO_MID=True, V_FRINGE=0.35, V_HALO_A=0.75)

def render_frame5(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True):
    ov = dict(V5_OVERRIDES)
    if mode == 'sv':
        ov.update(SV_COL=SV5['COL'], SV_POOL=SV5['POOL'], SV_DESAT=SV5['DESAT'], VGLOW_COL=SV5['COL'], VRIM_COL=SV5['COL'])
    with params(**ov):
        return render_frame4(R, t, cam, victims, mode, zback, ss, player, v5=True)

FRAMES5 = [
    ('weak_ots', 'Weak R=300  OTS  t=2 s', dict(R=R_WEAK, t=2.0, cam='ots')),
    ('strong_ots', 'Strong R=400  OTS  t=2 s', dict(R=R_STRONG, t=2.0, cam='ots')),
    ('weak_side', 'Weak R=300  side (hip height)', dict(R=R_WEAK, t=2.0, cam='side')),
    ('strong_side', 'Strong R=400  side (hip height)', dict(R=R_STRONG, t=2.0, cam='side')),
    ('weak_high', 'Weak R=300  high camera', dict(R=R_WEAK, t=2.0, cam='high')),
    ('strong_high', 'Strong R=400  high camera', dict(R=R_STRONG, t=2.0, cam='high')),
    ('strong_ots_t0', 'Strong OTS t=0 s', dict(R=R_STRONG, t=0.0, cam='ots')),
    ('strong_ots_t1', 'Strong OTS t=1 s', dict(R=R_STRONG, t=1.0, cam='ots')),
    ('strong_ots_t2', 'Strong OTS t=2 s', dict(R=R_STRONG, t=2.0, cam='ots')),
    ('strong_ots_t3', 'Strong OTS t=3 s', dict(R=R_STRONG, t=3.0, cam='ots')),
    ('victims', 'Victims: s=1 inside, s=0.6 (3 s after), s=0.25 (8 s after)  Weak OTS',
     dict(R=R_WEAK, t=2.0, cam='ots', victims=VICT4, zback=560.0)),
    ('sv_ots', 'Spirit Vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='sv', zback=560.0)),
    ('th_ots', 'Thermal  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='th', zback=560.0)),
    ('nv_ots', 'Night vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='nv', zback=560.0)),
    ('strong_ots_low', 'Strong  low OTS (200 behind, 140 up)', dict(R=R_STRONG, t=2.0, cam='ots_low')),
    ('weak_ots_low', 'Weak  low OTS (200 behind, 140 up)', dict(R=R_WEAK, t=2.0, cam='ots_low')),
    ('strong_victims', 'Strong OTS + victims s = 1, 0.6, 0.25', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, zback=560.0)),
]
SHEET5 = ['weak_ots', 'strong_ots', 'strong_ots_strip', 'weak_side', 'strong_side', 'victims', 'weak_high', 'strong_high',
          'sv_ots', 'th_ots', 'nv_ots', 'strong_ots_low', 'weak_ots_low', 'strong_victims']
SHEET5_LAB = {n: l for n, l, _ in FRAMES5}
SHEET5_LAB['strong_ots_strip'] = 'Motion strip: Strong OTS t = 0, 1, 2, 3 s'

def make_sheet5(outdir, cols=3, tw=640):
    th = tw * H // W
    bar = 30
    rows = (len(SHEET5) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(SHEET5):
        fn = os.path.join(outdir, PFX + '_%s.png' % n)
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), SHEET5_LAB[n], fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_%s_sheet.png' % PFX))

def main():
    """v5 is the default; --v4 and --v3 run the older versions unchanged"""
    global OUT5, PFX
    if '--v5' in sys.argv:
        sys.argv.remove('--v5')
        PFX = 'v5'
    else:
        P5[True] = dict(P5[True], **P5B_STRONG)
    if '--v3' in sys.argv or '--v4' in sys.argv:
        if '--v4' in sys.argv:
            sys.argv.remove('--v4')
        return main4()
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT5 = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    os.makedirs(OUT5, exist_ok=True)
    import time
    for name, label, kw in FRAMES5:
        if only and name not in only:
            continue
        t0 = time.time()
        render_frame5(**kw).save(os.path.join(OUT5, '%s_%s.png' % (PFX, name)))
        print('wrote', name, '%.0fs' % (time.time() - t0), flush=True)
    sn = [os.path.join(OUT5, PFX + '_strong_ots_t%d.png' % k) for k in range(4)]
    if all(os.path.exists(f) for f in sn):
        strip = Image.new('RGB', (W, H))
        for k, f in enumerate(sn):
            strip.paste(Image.open(f).convert('RGB').resize((W // 2, H // 2), Image.LANCZOS), ((k % 2) * W // 2, (k // 2) * H // 2))
        strip.save(os.path.join(OUT5, PFX + '_strong_ots_strip.png'))
    make_sheet5(OUT5)
    print('sheet done')


# ======================================================================= v6 (v3.27.2 fix round: PLAN sections 3-5; default; --v5b / --v5 / --v4 / --v3 run the older frames)
OUT6 = os.path.join(ROOT, 'staging', 'v3272_dp_fix', 'renders')
CAMS['close'] = dict(pos=(75.0, 152.0, 105.0), target=(0.0, 136.0, 0.0), fov=40.0)     # upper-body close-up, from behind-right
CAMS['feet'] = dict(pos=(70.0, 55.0, 150.0), target=(0.0, 24.0, 0.0), fov=50.0)        # the hem
V6_CELL = (0.85, 0.15)                          # halo cell: alpha = lobed profile x (0.85 + 0.15 n)
CLOAK_BONES = ('pelvis', 'lSh', 'rSh', 'lElb', 'rElb', 'lHand', 'rHand', 'lHip', 'rHip', 'lKnee', 'rKnee', 'lFoot', 'rFoot')
CLOAK_EDGES = (('chest', 'pelvis', 16), ('lSh', 'lElb', 5.5), ('lElb', 'lHand', 4.5), ('rSh', 'rElb', 5.5), ('rElb', 'rHand', 4.5),
               ('lHip', 'lKnee', 8.5), ('lKnee', 'lFoot', 6.5), ('rHip', 'rKnee', 8.5), ('rKnee', 'rFoot', 6.5))
SH_RMUL, SH_DROP = 0.8, 6.0                      # shoulder halo radius x0.8, centre 6 u lower
HEM_N, HEM_RING, HEM_H, HEM_R = 12, (18.0, 34.0), (6.0, 34.0), (12.0, 19.0)   # sprites, ring radius, centre height, sprite radius (mine)
HEM_A, HEM_TRAIL, HEM_CAP = (0.55, 0.90), 0.30, 60.0   # alpha hash x halo alpha; offset -v x 0.30 s x (0.4 + 0.6 hf), capped at 60 u
FRAY_N, FRAY_R, FRAY_LIFE, FRAY_A, FRAY_CAP = 8, (10.0, 18.0), (1.2, 2.0), 0.50, 90.0
TH6_CM = 0.08                                    # thermal: fog colours x 0.08, drawn after the heat palette
NV6_CMUL, NV6_AMUL = 0.0, 1.3                    # night vision dark parts: pitch black, alpha x 1.3 (cap 0.9)
NV6_GLINT, NV6_GAIN6 = (0.55, 0.85, 0.60), 0.8   # her tendrils / victims' glints, additive, x 0.8, before the NV post
V6_NVGL = 2.5                                    # NV victims' glints: extra alpha gain (mine: at x1 they vanish in the black)
V6_VGLOW = 1.5                                   # SV victims' glow x 1.5 (glow puffs and glints)
SV6 = dict(SV5, A=0.80, POOL=(0.060, 0.036))
SV6_VOL, SV6_TEND, SV6_TORSO = 2.5, 1.6, 0.16    # SV volume additive gain, her tendrils, torso glow puffs alpha
WALK_V = (0.0, 0.0, -150.0)

def cloak_hem_items(cam, S, pb, t, Ha, amul, col, vel):
    """12 hem sprites round her feet + 8 fray puffs. vel = her horizontal velocity (u/s); the hem trails by -vel x 0.30 s x (0.4 + 0.6 hf)."""
    items = []
    fc = (pb['lFoot'] + pb['rFoot']) * 0.5
    vel = np.asarray(vel, dtype=np.float64)
    for i in range(HEM_N):
        a = 2 * math.pi * (i + 0.6 * (FxRnd(i * 71 + 1) - 0.5)) / HEM_N + (0.10 if i % 2 else -0.10) * t
        r = lerp(HEM_RING[0], HEM_RING[1], FxRnd(i * 71 + 2))
        h = lerp(HEM_H[0], HEM_H[1], FxRnd(i * 71 + 3))
        hf = (h - HEM_H[0]) / (HEM_H[1] - HEM_H[0])
        rad = lerp(HEM_R[0], HEM_R[1], FxRnd(i * 71 + 4))
        off = -vel * HEM_TRAIL * (0.4 + 0.6 * hf)
        nrm = float(np.linalg.norm(off))
        if nrm > HEM_CAP:
            off *= HEM_CAP / nrm
        flut = 0.80 + 0.20 * math.sin(0.8 * t + 6.28 * FxRnd(i * 71 + 5))
        al = Ha * lerp(HEM_A[0], HEM_A[1], FxRnd(i * 71 + 6)) * flut * amul
        p = np.array((fc[0] + r * math.cos(a) + off[0], max(h, 0.75 * rad), fc[2] + r * math.sin(a) + off[2]))   # clamp: no flat cut at the floor
        z = cam.project(p)
        if z:
            items.append((z[2] - HALO5_BIAS, partial(draw_smoke5, S, cam, p, rad, col, al, i * 0.9 + 1.0, size_fade=False,
                                                     core=HALO5_CORE, cell6=True)))
    for i in range(FRAY_N):
        life = lerp(FRAY_LIFE[0], FRAY_LIFE[1], FxRnd(i * 53 + 1))
        tt = t + FxRnd(i * 53 + 2) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 131 + n * 977) & M32
        a = 2 * math.pi * FxRnd(k + 1)
        r = lerp(HEM_RING[0], HEM_RING[1], FxRnd(k + 2))
        h0 = lerp(10.0, HEM_H[1], FxRnd(k + 3))
        rad = lerp(FRAY_R[0], FRAY_R[1], FxRnd(k + 4))
        off = -vel * age                                   # it stays where it detached: she walks away from it
        nrm = float(np.linalg.norm(off))
        if nrm > FRAY_CAP:
            off *= FRAY_CAP / nrm
        sink = float(smooth(0.0, 0.85, f))
        y = lerp(h0, 0.65 * rad, sink)
        p = np.array((fc[0] + (r + 14.0 * f) * math.cos(a) + off[0], y, fc[2] + (r + 14.0 * f) * math.sin(a) + off[2]))
        env = float(smooth(0.0, 0.12, f) * (1 - smooth(0.30, 1.0, f)))
        z = cam.project(p)
        if z:
            items.append((z[2] - HALO5_BIAS, partial(draw_smoke, S, cam, p, rad, col, FRAY_A * Ha / 0.80 * env * amul, 1.7 * i, size_fade=False)))
    return items

def halo_items6(cam, S, pb, t, strong, cm, am, vel):
    """her cloak: black halo only on pelvis, shoulders (r x0.8, centre 6 u lower), elbows, hands, hips, knees, feet; midpoints on chest-pelvis,
    arms, legs; smooth cell; plus the hem. Not on head / neck / chest and not on head-neck, neck-chest, neck-shoulder midpoints."""
    items = []
    col = tuple(c * cm for c in HALO_COL)
    fcol = tuple(c * cm for c in FRINGE_COL)
    Ha = HALO5_A[1 if strong else 0]
    fr = FRINGE_A5[1 if strong else 0]
    def add(p, rad, bi, al):
        z = cam.project(p)
        if not z:
            return
        items.append((z[2] - HALO5_BIAS + 4, partial(draw_smoke5, S, cam, p, rad * 1.55, fcol, fr * al / Ha * am, bi * 0.7 + 3,
                                                     size_fade=False, core=0.2, plain=True)))
        items.append((z[2] - HALO5_BIAS, partial(draw_smoke5, S, cam, p, rad, col, min(al * am, 0.9), bi * 0.7, size_fade=False,
                                                 core=HALO5_CORE, cell6=True)))
    for bi, name in enumerate(PBONES):
        if name not in CLOAK_BONES:
            continue
        hp = away(cam, pb[name], lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(bi * 31 + 7)))
        rad = HALO_R[name] * HALO5_RMUL
        if name in ('lSh', 'rSh'):
            rad *= SH_RMUL
            hp[1] -= SH_DROP
        hp[1] = max(hp[1], 0.75 * rad)
        add(hp, rad, bi, Ha * (0.85 + 0.15 * math.sin(0.9 * t + bi * 1.3)))
    for ei, (a, b, rad) in enumerate(CLOAK_EDGES):
        mid = (pb[a] + pb[b]) * 0.5
        add(away(cam, mid, lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(ei * 53 + 11))), HALO5_MID[0] + HALO5_MID[1] * rad, ei + 20,
            Ha * 0.9 * (0.85 + 0.15 * math.sin(0.9 * t + ei * 1.7)))
    items += cloak_hem_items(cam, S, pb, t, Ha, am, col, vel)
    return items

def render_fog6(S, cam, R, t, victims, zback, mode, strong, pb, vel):
    Q = P5[strong]
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (TH6_CM, 1.0) if mode == 'th' else (1.0, 1.0)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS4)]
    for k in (0, 1):                                  # floor pool: no shin hole any more (HOLE5 removed)
        L = LAYERS4[k]
        tab = tables[k]
        if mode == 'sv':
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add', hcol=SV_COL, hgain=SV_POOL[k])
        else:
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, Q['FLOOR'][k], t, 'body', amul=am, cmul=cm)
            if k == 1 and mode == 'normal':
                disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add')
    if mode != 'sv':
        for vi, (pos, s) in enumerate(victims):
            rad = lerp(VSLAB_R[0], VSLAB_R[1], s) * (0.5 + 0.5 * s)
            Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                                  sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
            disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS4[1], VSLAB_DENS * (0.5 + 0.5 * s), t, 'body', amul=am, cmul=cm)
    items = []
    for pos, size, al, rot in volume_list5(R, t, strong):
        z = cam.project(pos)
        if not z:
            continue
        if mode == 'sv':
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, SV_COL, SV_VOL_A * al / VOL5_ALPHA * 0.6 * SV6_VOL, rot, additive=True)))
        else:
            al = al * tunnel(cam, pos, size)
            col = tint_col(pos[0], pos[2], t) * VOL_BRIGHT * cm
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, col, min(al * am, 0.9), rot, glow=VOL_GLOW if mode == 'normal' else 0.0, floor=True)))
    if mode != 'sv':
        for pos, hw, hh, col, al, ph in wisp_list(R, t)[: (24 if not strong else 48)]:
            z = cam.project(pos)
            if z:
                items.append((z[2], partial(draw_wisp, S, cam, pos, hw, hh, tuple(c * cm for c in col), al * am * (0.6 if not strong else 1.0), ph, t)))
    else:
        items += vein_items5(cam, S, R, t)
    if pb is not None:
        if mode in ('normal', 'nv', 'th'):
            items += halo_items6(cam, S, pb, t, strong, cm, am, vel)
        if mode in ('normal', 'sv'):
            cs = (SV5['VEIN'], SV5['VEIN_HALO']) if mode == 'sv' else None
            items += tendril_items5(cam, S, pb, t, strong, SV6_TEND if mode == 'sv' else 1.0, cs)
        if mode == 'nv':                              # green-white filaments, additive, before the NV post
            g = tuple(c * NV6_GAIN6 for c in NV6_GLINT)
            items += tendril_items5(cam, S, pb, t, strong, 1.0, (g, tuple(c * 0.55 for c in g)))
        if mode == 'sv':
            for name, rad in (('chest', 40), ('pelvis', 36), ('head', 26)):
                z = cam.project(pb[name])
                if z:
                    items.append((z[2], partial(draw_smoke, S, cam, away(cam, pb[name], 10), rad, SV_COL, SV6_TORSO, 1.0, additive=True,
                                                size_fade=False, core=0.1)))
    for vi, (pos, s) in enumerate(victims):
        if s > 0.02:
            items += victim_items(cam, S, vi, pos, s, t, mode)
    items.sort(key=lambda it: -it[0])
    for z, fn in items:
        fn()

def render_frame6(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True, vel=(0.0, 0.0, 0.0)):
    global FOG_TEX, GRIME_TEX
    ss = ss or SS
    if FOG_TEX is None:
        FOG_TEX = make_tex(128, 1, 4, TEX_WARP)
        GRIME_TEX = make_tex(128, 7, 5, 0.0, base=3)
    ov = dict(V5_OVERRIDES, NEAR0=V4_NEAR[0], NEAR1=V4_NEAR[1], WISP_HALF_H=WISP4_HALF_H, LAMP_I=V4_LAMP_I, SV5=SV6)
    if mode == 'sv':
        ov.update(SV_COL=SV6['COL'], SV_POOL=SV6['POOL'], SV_DESAT=SV6['DESAT'], VGLOW_COL=SV6['COL'], VRIM_COL=SV6['COL'])
    if mode == 'nv':
        ov.update(NV_CMUL=NV6_CMUL, NV_AMUL=NV6_AMUL)
    with params(**ov):
        c = Cam(ss, CAMS[cam])
        rgb, zb = render_scene(c, zback, 0.0, player=False)
        if mode == 'th':
            rgb = (np.clip(_lum(rgb) * 1.8, 0, 1) ** 0.8 * TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
        vs = [(p, s) for p, s in victims]
        walking = float(np.linalg.norm(vel)) > 0
        pb = player_bones(t, walking) if player else None
        if player:
            pc = (TH_PLAYER,) * 3 if mode == 'th' else (0.30, 0.26, 0.27)
            draw_player(c, rgb, zb, pb, pc)
        cols = [(0.85 * (1 - 0.7 * s),) * 3 for p, s in vs] if mode == 'th' else None
        rim = draw_victims(c, rgb, zb, vs, t, cols=cols, rim=(mode in ('normal', 'sv')))
        if mode == 'th':                              # heat palette FIRST (hot bodies and environment), the fog is drawn after it
            rgb[:] = filter_th(rgb)
        S = dict(img=rgb, zb=zb)
        render_fog6(S, c, R, t, vs, zback, mode, R >= 350, pb, vel)
        if rim is not None:
            S['img'] += rim[:, None] * np.array(SV_COL if mode == 'sv' else VRIM_COL, dtype=np.float32)
        img = np.clip(S['img'].reshape(c.h, c.w, 3), 0, 1)
        im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
        im = im.resize((W, H), Image.BOX) if ss > 1 else im
        f = np.asarray(im, dtype=np.float32) / 255.0
        f = {'sv': filter_sv, 'nv': filter_nv}.get(mode, lambda a: a)(f)
    return Image.fromarray((np.clip(f, 0, 1) * 255 + 0.5).astype(np.uint8))

FRAMES6 = [
    ('weak_ots', 'Weak OTS, standing', dict(R=R_WEAK, t=2.0, cam='ots')),
    ('strong_ots', 'Strong OTS, standing', dict(R=R_STRONG, t=2.0, cam='ots')),
    ('strong_ots_walk', 'Strong OTS, walking 150 u/s (hem trails)', dict(R=R_STRONG, t=2.0, cam='ots', vel=WALK_V)),
    ('strong_side_walk', 'Strong side, walking 150 u/s', dict(R=R_STRONG, t=2.0, cam='side', vel=WALK_V)),
    ('closeup', 'Close-up upper body (Strong): head / neck / chest', dict(R=R_STRONG, t=2.0, cam='close')),
    ('victims', 'Victims s = 1, 0.6, 0.25  Weak OTS', dict(R=R_WEAK, t=2.0, cam='ots', victims=VICT4, zback=560.0)),
    ('sv_ots', 'Spirit Vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='sv', zback=560.0)),
    ('th_ots', 'Thermal  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='th', zback=560.0)),
    ('nv_ots', 'Night vision  Strong OTS', dict(R=R_STRONG, t=2.0, cam='ots', victims=VICT4, mode='nv', zback=560.0)),
    # extras (not on the sheet)
    ('weak_ots_walk', 'Weak OTS, walking', dict(R=R_WEAK, t=2.0, cam='ots', vel=WALK_V)),
    ('strong_side', 'Strong side, standing', dict(R=R_STRONG, t=2.0, cam='side')),
    ('feet', 'Hem close-up, standing', dict(R=R_STRONG, t=2.0, cam='feet')),
    ('feet_walk', 'Hem close-up, walking', dict(R=R_STRONG, t=2.0, cam='feet', vel=WALK_V)),
    ('closeup_weak', 'Close-up upper body (Weak)', dict(R=R_WEAK, t=2.0, cam='close')),
    ('nv_walk', 'Night vision  OTS walking', dict(R=R_STRONG, t=2.0, cam='ots', mode='nv', vel=WALK_V)),
    ('th_walk', 'Thermal  OTS walking', dict(R=R_STRONG, t=2.0, cam='ots', mode='th', vel=WALK_V)),
]
SHEET6 = ['weak_ots', 'strong_ots', 'strong_ots_walk', 'strong_side_walk', 'closeup', 'victims', 'sv_ots', 'th_ots', 'nv_ots']
SHEET6_LAB = {n: l for n, l, _ in FRAMES6}

def make_sheet6(outdir, cols=3, tw=640):
    th = tw * H // W
    bar = 30
    rows = (len(SHEET6) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(SHEET6):
        fn = os.path.join(outdir, 'v6_%s.png' % n)
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), SHEET6_LAB[n], fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_v6_sheet.png'))

def main6():
    """v6 is the default; --v5b (the old default), --v5, --v4, --v3 run the older versions unchanged"""
    global OUT6, V6
    if any(a in sys.argv for a in ('--v5b', '--v5', '--v4', '--v3')):
        if '--v5b' in sys.argv:
            sys.argv.remove('--v5b')
        return main()
    V6 = True
    P5[True] = dict(P5[True], **P5B_STRONG)
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT6 = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    os.makedirs(OUT6, exist_ok=True)
    import time
    for name, label, kw in FRAMES6:
        if only and name not in only:
            continue
        t0 = time.time()
        render_frame6(**kw).save(os.path.join(OUT6, 'v6_%s.png' % name))
        print('wrote', name, '%.0fs' % (time.time() - t0), flush=True)
    make_sheet6(OUT6)
    print('sheet done')


# ======================================================================= v7 (reviewer fixes: cloak legibility, shoulder clip, NV after the post, SV hue; default; --v6 runs v6 unchanged)
# the --v6 / --v5b / --v5 / --v4 / --v3 switches run the older frames
V7_GMIN, V7_GLEN, V7_GALPHA, V7_GHW = 0.35, 22.0, 1.6, 1.8   # victim glints: min strength 0.35 smoothstep(0.02, 0.15, s); filament length u, alpha x, half width u (mine: last three)
V7_GLIFT = 9.0                                  # filament lift toward the camera, u (mine)
NV7_COL = (0.55, 0.90, 0.60)                    # NV filaments / glints: exact colour, drawn after the NV post
NV7_TEND_GAIN = 1.0                             # NV tendril gain (v6: colour x 0.8)
HALO7_A = (0.65, 0.92)                          # halo alpha Weak / Strong (v6 0.55 / 0.80)
HALO7_CAP = 0.95                                # per-sprite alpha cap (v6 0.9); NV keeps 0.9
SH7_RMUL = 0.7                                  # shoulder halo radius x0.7 (v6 x0.8)
HALO7_COL = (0.0, 0.0, 0.0)                     # pure black core (v6 (0.003, 0.001, 0.007))
HEM7_STRETCH = 3.0                              # hem sprite stretched along the trail at full walking speed (v6: round)
HEM7_TRAIL, HEM7_CAP, FRAY7_CAP = 0.45, 80.0, 150.0   # hem trail s / cap u, fray cap u (v6 0.30 / 60 / 90; mine)
HEM7_RIM = 0.0                                  # fringe alpha of the hem (violet rim, premultiplied FRINGE_COL); 0 = none
FRAY7_R, FRAY7_A = (12.0, 22.0), 0.65           # fray radius / alpha at birth (v6 (10, 18) / 0.50)
FRAY7_ENV_IN = 0.06                             # fray fade-in fraction (v6 0.12)
SV7 = dict(SV6, A=0.70)
SV7_TEND = 1.35                                 # SV her tendrils gain (v6 1.6)

def cloak_hem_items7(cam, S, pb, t, Ha, amul, col, vel, top, fcol, fr):
    """v7 hem + fray: pure black, sprites stretched along the trail (screen direction of -vel) when walking, shoulder clip, bigger fray."""
    items = []
    fc = (pb['lFoot'] + pb['rFoot']) * 0.5
    vel = np.asarray(vel, dtype=np.float64)
    speed = float(np.linalg.norm(vel))
    stretch = None
    if speed > 0:
        dw = -vel / speed
        sx, sy = float(dw @ cam.r), -float(dw @ cam.u)            # screen dir, y down
        sl = math.hypot(sx, sy)
        if sl > 0.05:
            stretch = (sx / sl, sy / sl, 1.0 + (HEM7_STRETCH - 1.0) * min(1.0, speed / 150.0) * min(1.0, sl * 1.4))
    for i in range(HEM_N):
        a = 2 * math.pi * (i + 0.6 * (FxRnd(i * 71 + 1) - 0.5)) / HEM_N + (0.10 if i % 2 else -0.10) * t
        r = lerp(HEM_RING[0], HEM_RING[1], FxRnd(i * 71 + 2))
        h = lerp(HEM_H[0], HEM_H[1], FxRnd(i * 71 + 3))
        hf = (h - HEM_H[0]) / (HEM_H[1] - HEM_H[0])
        rad = lerp(HEM_R[0], HEM_R[1], FxRnd(i * 71 + 4))
        off = -vel * HEM7_TRAIL * (0.4 + 0.6 * hf)
        nrm = float(np.linalg.norm(off))
        if nrm > HEM7_CAP:
            off *= HEM7_CAP / nrm
        flut = 0.80 + 0.20 * math.sin(0.8 * t + 6.28 * FxRnd(i * 71 + 5))
        al = Ha * lerp(HEM_A[0], HEM_A[1], FxRnd(i * 71 + 6)) * flut * amul
        p = np.array((fc[0] + r * math.cos(a) + off[0], max(h, 0.75 * rad), fc[2] + r * math.sin(a) + off[2]))
        z = cam.project(p)
        if z:
            if HEM7_RIM > 0:
                items.append((z[2] - HALO5_BIAS + 4, partial(draw_smoke5, S, cam, p, rad * 1.45, fcol, HEM7_RIM * fr * al / Ha, i * 0.9 + 4.0,
                                                             size_fade=False, core=0.2, plain=True, clip_top=top, stretch=stretch)))
            items.append((z[2] - HALO5_BIAS, partial(draw_smoke5, S, cam, p, rad, col, al, i * 0.9 + 1.0, size_fade=False,
                                                     core=HALO5_CORE, cell6=True, clip_top=top, stretch=stretch)))
    for i in range(FRAY_N):
        life = lerp(FRAY_LIFE[0], FRAY_LIFE[1], FxRnd(i * 53 + 1))
        tt = t + FxRnd(i * 53 + 2) * life
        n = int(tt // life)
        age = tt - n * life
        f = age / life
        k = (i * 131 + n * 977) & M32
        a = 2 * math.pi * FxRnd(k + 1)
        r = lerp(HEM_RING[0], HEM_RING[1], FxRnd(k + 2))
        h0 = lerp(10.0, HEM_H[1], FxRnd(k + 3))
        rad = lerp(FRAY7_R[0], FRAY7_R[1], FxRnd(k + 4))
        off = -vel * age
        nrm = float(np.linalg.norm(off))
        if nrm > FRAY7_CAP:
            off *= FRAY7_CAP / nrm
        sink = float(smooth(0.0, 0.85, f))
        y = lerp(h0, 0.65 * rad, sink)
        p = np.array((fc[0] + (r + 14.0 * f) * math.cos(a) + off[0], y, fc[2] + (r + 14.0 * f) * math.sin(a) + off[2]))
        env = float(smooth(0.0, FRAY7_ENV_IN, f) * (1 - smooth(0.30, 1.0, f)))
        z = cam.project(p)
        if z:
            items.append((z[2] - HALO5_BIAS, partial(draw_smoke, S, cam, p, rad, col, FRAY7_A * Ha / 0.92 * env * amul, 1.7 * i, size_fade=False,
                                                     clip_top=top)))
    return items

def halo_items7(cam, S, pb, t, strong, cm, am, vel, cap):
    """v7 cloak: halo alpha 0.65 / 0.92, pure black colour, shoulder discs x0.7, every sprite clipped at her shoulder line (alpha fades to 0 over the 20 u below it)."""
    items = []
    col = HALO7_COL
    fcol = tuple(c * cm for c in FRINGE_COL)
    Ha = HALO7_A[1 if strong else 0]
    fr = FRINGE_A5[1 if strong else 0]
    top = float((pb['lSh'][1] + pb['rSh'][1]) * 0.5)
    def add(p, rad, bi, al):
        z = cam.project(p)
        if not z:
            return
        items.append((z[2] - HALO5_BIAS + 4, partial(draw_smoke5, S, cam, p, rad * 1.55, fcol, fr * al / Ha * am, bi * 0.7 + 3,
                                                     size_fade=False, core=0.2, plain=True, clip_top=top)))
        items.append((z[2] - HALO5_BIAS, partial(draw_smoke5, S, cam, p, rad, col, min(al * am, cap), bi * 0.7, size_fade=False,
                                                 core=HALO5_CORE, cell6=True, clip_top=top)))
    for bi, name in enumerate(PBONES):
        if name not in CLOAK_BONES:
            continue
        hp = away(cam, pb[name], lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(bi * 31 + 7)))
        rad = HALO_R[name] * HALO5_RMUL
        if name in ('lSh', 'rSh'):
            rad *= SH7_RMUL
            hp[1] -= SH_DROP
        hp[1] = max(hp[1], 0.75 * rad)
        add(hp, rad, bi, Ha * (0.85 + 0.15 * math.sin(0.9 * t + bi * 1.3)))
    for ei, (a, b, rad) in enumerate(CLOAK_EDGES):
        mid = (pb[a] + pb[b]) * 0.5
        add(away(cam, mid, lerp(HALO_PUSH[0], HALO_PUSH[1], FxRnd(ei * 53 + 11))), HALO5_MID[0] + HALO5_MID[1] * rad, ei + 20,
            Ha * 0.9 * (0.85 + 0.15 * math.sin(0.9 * t + ei * 1.7)))
    items += cloak_hem_items7(cam, S, pb, t, Ha, am, col, vel, top, fcol, fr)
    return items

def render_fog7(S, cam, R, t, victims, zback, mode, strong, pb, vel):
    """as render_fog6 with the v7 cloak; SV: tendrils and veins go to a separate additive layer S['svlayer'] (hue-preserving sum in render_frame7)"""
    Q = P5[strong]
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    S2 = S
    if mode == 'sv':
        S['svlayer'] = np.zeros_like(S['imgimg'])
        S2 = dict(S, imgimg=S['svlayer'])
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (TH6_CM, 1.0) if mode == 'th' else (1.0, 1.0)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS4)]
    for k in (0, 1):
        L = LAYERS4[k]
        tab = tables[k]
        if mode == 'sv':
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add', hcol=SV_COL, hgain=SV_POOL[k])
        else:
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, Q['FLOOR'][k], t, 'body', amul=am, cmul=cm)
            if k == 1 and mode == 'normal':
                disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add')
    if mode != 'sv':
        for vi, (pos, s) in enumerate(victims):
            rad = lerp(VSLAB_R[0], VSLAB_R[1], s) * (0.5 + 0.5 * s)
            Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                                  sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
            disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS4[1], VSLAB_DENS * (0.5 + 0.5 * s), t, 'body', amul=am, cmul=cm)
    items = []
    for pos, size, al, rot in volume_list5(R, t, strong):
        z = cam.project(pos)
        if not z:
            continue
        if mode == 'sv':
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, SV_COL, SV_VOL_A * al / VOL5_ALPHA * 0.6 * SV6_VOL, rot, additive=True)))
        else:
            al = al * tunnel(cam, pos, size)
            col = tint_col(pos[0], pos[2], t) * VOL_BRIGHT * cm
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, col, min(al * am, 0.9), rot, glow=VOL_GLOW if mode == 'normal' else 0.0, floor=True)))
    if mode != 'sv':
        for pos, hw, hh, col, al, ph in wisp_list(R, t)[: (24 if not strong else 48)]:
            z = cam.project(pos)
            if z:
                items.append((z[2], partial(draw_wisp, S, cam, pos, hw, hh, tuple(c * cm for c in col), al * am * (0.6 if not strong else 1.0), ph, t)))
    else:
        items += vein_items5(cam, S2, R, t)
    if pb is not None:
        if mode in ('normal', 'nv', 'th'):
            items += halo_items7(cam, S, pb, t, strong, cm, am, vel, 0.9 if mode == 'nv' else HALO7_CAP)
        if mode in ('normal', 'sv'):
            cs = (SV5['VEIN'], SV5['VEIN_HALO']) if mode == 'sv' else None
            items += tendril_items5(cam, S2, pb, t, strong, SV7_TEND if mode == 'sv' else 1.0, cs)
        if mode == 'nv':                              # exact green-white filaments, additive, AFTER the NV post
            g = tuple(c * NV7_TEND_GAIN for c in NV7_COL)
            items += tendril_items5(cam, S, pb, t, strong, 1.0, (g, tuple(c * 0.55 for c in g)))
        if mode == 'sv':
            for name, rad in (('chest', 40), ('pelvis', 36), ('head', 26)):
                z = cam.project(pb[name])
                if z:
                    items.append((z[2], partial(draw_smoke, S, cam, away(cam, pb[name], 10), rad, SV_COL, SV6_TORSO, 1.0, additive=True,
                                                size_fade=False, core=0.1)))
    for vi, (pos, s) in enumerate(victims):
        if s > 0.02:
            items += victim_items(cam, S, vi, pos, s, t, mode)
    items.sort(key=lambda it: -it[0])
    for z, fn in items:
        fn()

def nv_post7(rgb):
    """NV phosphor only (luminance gain + lift, green); grain and vignette come after the fog (nv_final7)"""
    L = np.clip(_lum(rgb) * NV_GAIN, 0, 1) ** 0.6
    return np.stack([L * 0.30, L * 1.0, L * 0.38], axis=-1).astype(np.float32)

def nv_final7(f):
    rng = np.random.RandomState(5)
    g = 1 + 0.10 * (rng.rand(f.shape[0], f.shape[1]).astype(np.float32) - 0.5)
    gy, gx = np.mgrid[0:f.shape[0], 0:f.shape[1]].astype(np.float32)
    vg = 1 - 0.27 * (((gx / f.shape[1] - 0.5) * 2) ** 2 + ((gy / f.shape[0] - 0.5) * 2) ** 2)
    return np.clip(f * (g * vg)[..., None], 0, 1)

def render_frame7(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True, vel=(0.0, 0.0, 0.0)):
    global FOG_TEX, GRIME_TEX
    ss = ss or SS
    if FOG_TEX is None:
        FOG_TEX = make_tex(128, 1, 4, TEX_WARP)
        GRIME_TEX = make_tex(128, 7, 5, 0.0, base=3)
    ov = dict(V5_OVERRIDES, NEAR0=V4_NEAR[0], NEAR1=V4_NEAR[1], WISP_HALF_H=WISP4_HALF_H, LAMP_I=V4_LAMP_I, SV5=SV7)
    if mode == 'sv':
        ov.update(SV_COL=SV7['COL'], SV_POOL=SV7['POOL'], SV_DESAT=SV7['DESAT'], VGLOW_COL=SV7['COL'], VRIM_COL=SV7['COL'])
    if mode == 'nv':
        ov.update(NV_CMUL=NV6_CMUL, NV_AMUL=NV6_AMUL)
    with params(**ov):
        c = Cam(ss, CAMS[cam])
        rgb, zb = render_scene(c, zback, 0.0, player=False)
        if mode == 'th':
            rgb = (np.clip(_lum(rgb) * 1.8, 0, 1) ** 0.8 * TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
        vs = [(p, s) for p, s in victims]
        walking = float(np.linalg.norm(vel)) > 0
        pb = player_bones(t, walking) if player else None
        if player:
            pc = (TH_PLAYER,) * 3 if mode == 'th' else (0.30, 0.26, 0.27)
            draw_player(c, rgb, zb, pb, pc)
        cols = [(0.85 * (1 - 0.7 * s),) * 3 for p, s in vs] if mode == 'th' else None
        rim = draw_victims(c, rgb, zb, vs, t, cols=cols, rim=(mode in ('normal', 'sv')))
        if mode == 'th':
            rgb[:] = filter_th(rgb)
        if mode == 'nv':                              # v7: NV post on the scene FIRST; the black cloud and the filaments are drawn on top
            rgb[:] = nv_post7(rgb)
        S = dict(img=rgb, zb=zb)
        render_fog7(S, c, R, t, vs, zback, mode, R >= 350, pb, vel)
        if rim is not None:
            S['img'] += rim[:, None] * np.array(SV_COL if mode == 'sv' else VRIM_COL, dtype=np.float32)
        img = np.clip(S['img'].reshape(c.h, c.w, 3), 0, 1)
        if mode == 'sv':                              # tendrils / veins layer: where the sum exceeds 1 scale rgb by 1 / max channel (hue kept)
            tot = img + S['svlayer']
            img = tot / np.maximum(1.0, tot.max(axis=-1, keepdims=True))
        im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
        im = im.resize((W, H), Image.BOX) if ss > 1 else im
        f = np.asarray(im, dtype=np.float32) / 255.0
        if mode == 'sv':
            f = filter_sv(f)
        elif mode == 'nv':
            f = nv_final7(f)
    return Image.fromarray((np.clip(f, 0, 1) * 255 + 0.5).astype(np.uint8))

FRAMES7 = FRAMES6
SHEET7 = SHEET6
HEMCMP = [('strong_ots', 'Strong OTS, standing'), ('strong_ots_walk', 'Strong OTS, walking'), ('strong_side_walk', 'Strong side, walking'),
          ('feet', 'Feet close-up, standing'), ('feet_walk', 'Feet close-up, walking')]

def make_sheet7(outdir, cols=3, tw=640):
    th = tw * H // W
    bar = 30
    rows = (len(SHEET7) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(SHEET7):
        fn = os.path.join(outdir, 'v7_%s.png' % n)
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), SHEET6_LAB[n], fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_v7_sheet.png'))

def make_compare7(outdir, tw=800):
    """fog_v7_hem_compare.png: v6 (left) vs v7 (right), same cameras, standing and walking"""
    th = tw * H // W
    bar = 30
    sheet = Image.new('RGB', (2 * tw, len(HEMCMP) * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for r, (n, lab) in enumerate(HEMCMP):
        for k, pf in enumerate(('v6', 'v7')):
            fn = os.path.join(outdir, '%s_%s.png' % (pf, n))
            if not os.path.exists(fn):
                continue
            sheet.paste(Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS), (k * tw, r * (th + bar) + bar))
            d.text((k * tw + 8, r * (th + bar) + 5), '%s  %s' % (pf, lab), fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, 'fog_v7_hem_compare.png'))

def main7():
    """v7 is the default; --v6 (the old default), --v5b, --v5, --v4, --v3 run the older versions unchanged"""
    global OUT6, V6, V7
    if '--v6' in sys.argv or any(a in sys.argv for a in ('--v5b', '--v5', '--v4', '--v3')):
        if '--v6' in sys.argv:
            sys.argv.remove('--v6')
        return main6()
    V6 = True
    V7 = True
    P5[True] = dict(P5[True], **P5B_STRONG)
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT6 = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    os.makedirs(OUT6, exist_ok=True)
    import time
    for name, label, kw in FRAMES7:
        if only and name not in only:
            continue
        t0 = time.time()
        render_frame7(**kw).save(os.path.join(OUT6, 'v7_%s.png' % name))
        print('wrote', name, '%.0fs' % (time.time() - t0), flush=True)
    make_sheet7(OUT6)
    make_compare7(OUT6)
    print('sheet done')


# ======================================================================= v8 (v3.28 TRAIL: the black-fire trail, user v3.27.3 item 7; default; --v7 runs v7 unchanged)
# staging/v328_spirit/c_trail/DESIGN.md (variant A) / NOTES.md. Mirrors deathpresence_sprites.inc DpTrailStep / DpHerTrail:
# path nodes every S = clamp(0.32 speed, 40, 120) u (alternately 7 u left / right), each a hashed seed (its serial) for black
# flame tongues with an ember-red -> violet rim, a smouldering footprint decal, embers, ash and dark heat wisps. The preview
# has no frame loop: she walks in a straight line at constant speed (the nodes the game lays at any frame rate), optionally
# stopped `stop` s ago. Y is up here (down in the game).
OUT8 = os.path.join(ROOT, 'staging', 'v328_spirit', 'c_trail', 'renders')
CAMS['back34'] = dict(pos=(-170.0, 210.0, 330.0), target=(0.0, 30.0, 90.0), fov=55.0)   # behind-left, high: the trail on the floor
WALK8_V = (0.0, 0.0, -175.5)                    # her real walk / run speeds (KB, at 60: 175.5 / 663 u/s)
RUN8_V = (0.0, 0.0, -663.0)
TR8 = dict(N=32, LIFE=2.6, VMIN=30.0, SIDE=7.0, SPC=(0.32, 40.0, 120.0))
TR8_RIM = ((1.0, 0.24, 0.04), (0.50, 0.08, 0.85))   # tongue rim: ember red (new) -> violet (old)
TR8_SPARK = ((1.0, 0.45, 0.10), (0.75, 0.12, 0.45))
TR8_ASH, TR8_CRACK = (0.025, 0.018, 0.030), (0.95, 0.28, 0.05)
TR8_GLOW = ((0.90, 0.22, 0.04), (0.35, 0.05, 0.60))   # ground glow under the fire: new / old
TR8_SV, TR8_SVHOT = (0.30, 0.30, 0.95), (0.55, 0.65, 1.0)
TR8_BIAS = 40.0                                 # tongues / ash sort this much nearer (over the volume round them), wisps 20
NV8_COL = (0.55, 0.90, 0.60)
VPUFF_COL5 = (0.075, 0.020, 0.130)              # kDpVPuffCol
TRAIL8 = True                                   # False: v8 frames without the trail
TR8B = True                                     # v8b (the OTS pass: DP_TR_V8B 1); False (--v8): the v8 trail
TR8_HOT = (1.0, 0.42, 0.08)                     # v8b: the newest tongues' edge

TR8C = True                                     # v8c (review ART-T1-001: the wake in the OTS frame; DP_TR_V8C 1); False (--v8b): v8b

def TRC(v8b, v8c):
    """DpTrC"""
    return v8c if (TR8B and TR8C) else v8b

def TRV(v8, v8b):
    """DpTrV"""
    return v8b if TR8B else v8

def tr8_spacing(sp):
    return min(max(TR8['SPC'][0] * sp, TR8['SPC'][1]), TRV(TR8['SPC'][2], 70.0))   # (v8b: 70 at a run)

def trail_nodes8(t, vel, stop=0.0):
    """the nodes alive at time t: she walks along vel since long ago and is at the origin now (stopped `stop` s ago); newest first.
    Each: dict(p, t0, sp, dir, seg, id). Mirrors DpTrailStep (constant speed: the crossing points are exact)."""
    vel = np.asarray(vel, dtype=np.float64)
    sp = float(np.linalg.norm(vel))
    if sp < TR8['VMIN']:
        return []
    S = tr8_spacing(sp)
    d = vel / sp
    ux, uz = d[0], d[2]
    T0 = 100.0                                   # she started T0 s before t = 0
    tw = t - stop                                # the time she stopped
    Dw = sp * (tw + T0)                          # path length walked by then
    jmax = int(Dw // S)
    out = []
    for j in range(jmax, 0, -1):
        tb = j * S / sp - T0
        if t - tb > TR8['LIFE']:
            break
        side = TR8['SIDE'] if (j & 1) else -TR8['SIDE']
        back = Dw - j * S                        # behind her stop point along the path
        p = np.array((-ux * back - uz * side, 0.0, -uz * back + ux * side))
        out.append(dict(p=p, t0=tb, sp=sp, dir=(ux, uz), seg=S, id=j))
    return out

def tr8_big(cam, p, hl):
    """DpTrBig: a trail sprite of world half length hl filling more than 45-70 % of the screen height fades out"""
    q = cam.project(p)
    if not q:
        return 0.0
    return 1.0 - float(smooth(TRV(0.45, 0.60), TRV(0.70, 0.90), 2 * hl * cam.pix_per_unit(q[2]) / cam.h))

def _vnoise(x, y):
    """the FxValueNoise stand-in (the fog texture, bilinear)"""
    return tex_sample(FOG_TEX, np.asarray(x, dtype=np.float32) * 0.13, np.asarray(y, dtype=np.float32) * 0.13 + 0.37)

def _blend(S, x0, x1, y0, y1, z, a, color, additive, zz=None):
    zb = S['zbimg'][y0:y1, x0:x1]
    a = np.where((zz if zz is not None else z) < zb, np.clip(a, 0, 1), 0).astype(np.float32)[..., None]
    reg = S['imgimg'][y0:y1, x0:x1]
    col = np.array(color, dtype=np.float32)
    if additive:
        reg += col * a
    else:
        reg[:] = col * a + reg * (1 - a)

def draw_flame8(S, cam, base, tip, hw, color, alpha, additive=False, cell=-1):
    """FXX_FLAME teardrop (screenfx.inc FxTexel FXX_FLAME), camera facing, foot 20 % up the quad at base (the round bottom under the floor), tip at tip, half width hw;
    stretched along the base-tip line on screen (>= 0.45 x world length); depth = its centre's; near fade. cell >= 0: the FXX_SMOKE
    cell 0 / 1 instead (DpSmokeTexel: ragged billow with holes; its RGB x (0.55 + n) / 1.55), its foot 15 % up"""
    foot = 0.20 if cell < 0 else 0.15
    if alpha < 0.004 or hw < 0.5:
        return
    base, tip = np.asarray(base, dtype=np.float64), np.asarray(tip, dtype=np.float64)
    pb, pt = cam.project(base), cam.project(tip)
    if not pb or not pt:
        return
    ppb = cam.pix_per_unit(pb[2])
    ex, ey = (pt[0] - pb[0]) / ppb, (pt[1] - pb[1]) / ppb
    el = math.hypot(ex, ey)
    wl = float(np.linalg.norm(tip - base))
    if not el > 0.05 * wl:
        ex, ey, el = 0.0, -1.0, 1.0
    ex, ey = ex / el, ey / el
    el = max(el, 0.45 * wl)
    alpha *= tr8_big(cam, base, 0.5 * wl / (1 - foot))
    c = base + (tip - base) * ((0.5 - foot) / (1 - foot))
    pc = cam.project(c)
    if not pc:
        return
    px, py, z = pc
    sk = max(1.0, el / (1 - foot) * 0.5 / hw)
    pr = hw * cam.pix_per_unit(z)
    if pr < 0.5:
        return
    alpha *= float(smooth(NEAR0, NEAR1, float(np.linalg.norm(c - cam.c)))) * float(smooth(60.0, 140.0, float(np.linalg.norm(base - cam.c))))   # + DpTrNear
    if alpha < 0.004:
        return
    ext = pr * sk * 1.05 + 2
    x0, x1 = max(0, int(px - ext)), min(cam.w, int(px + ext) + 1)
    y0, y1 = max(0, int(py - ext)), min(cam.h, int(py + ext) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    gx, gy = np.meshgrid(np.arange(x0, x1, dtype=np.float32) + 0.5 - px, np.arange(y0, y1, dtype=np.float32) + 0.5 - py)
    along = (gx * ex + gy * ey) / (pr * sk)      # +1 = the tip
    fx = (-gx * ey + gy * ex) / pr
    fy = np.clip((1 + along) * 0.5, 0, 1)
    inside = (np.abs(along) <= 1) & (np.abs(fx) <= 1)
    if cell >= 0:
        qy = along
        o = 0.31 * cell
        rr = np.sqrt(fx * fx + qy * qy)
        n = 0.55 * _vnoise(3.4 * fx + 5.0 + o, 3.4 * qy + 5.0 - o) + 0.45 * _vnoise(10.0 * fx + 8.7 - o, 7.0 * qy + 7.1 + o)
        prof = (1 - smooth(0.45, 1.0, rr * (0.62 + 1.00 * (n - 0.5)))) * (1 - smooth(0.8, 1.0, rr))
        a = np.clip(prof * (0.04 + 1.90 * smooth(0.36, 0.56, n)), 0, 1) * alpha * inside
        _blend(S, x0, x1, y0, y1, z, a, np.array(color, dtype=np.float32) * ((0.55 + n) / 1.55)[..., None], additive)
        return
    prof = 0.78 * np.sin(math.pi * np.clip(fy * 0.92 + 0.04, 0, 1) ** 0.55) ** 0.9 * (1 - 0.55 * fy)
    wob = 0.10 * (_vnoise(fy * 5.0, 0.5) - 0.5) * fy
    d = np.where(prof > 0.01, np.abs(fx - wob) / np.maximum(prof, 1e-3), 9.0)
    n = 0.75 + 0.25 * _vnoise(fx * 4.0 + 7.0, fy * 9.0)
    a = np.clip((1 - smooth(0.35, 1.0, d)) * smooth(0.0, 0.12, fy) * n, 0, 1) * alpha * inside
    _blend(S, x0, x1, y0, y1, z, a, color, additive)

def draw_decal8(S, cam, cen, ax, hl, hw, cell, color, alpha, additive=False):
    """a floor quad 1.5 u above the floor (per-pixel ray hit: its own depth), FXX_SMOKE cell 4 (halo: smooth lobed), 5 (soft disc: the
    ground glow) or 2 / 3 (crest: the bright billow tops = glowing cracks)"""
    if alpha < 0.004:
        return
    cen = np.asarray(cen, dtype=np.float64)
    a3 = np.array((ax[0], 0.0, ax[1]))
    p3 = np.array((-ax[1], 0.0, ax[0]))
    cs = []
    for sa in (-1, 1):
        for sb in (-1, 1):
            q = cam.project(cen + sa * hl * a3 + sb * hw * p3 + np.array((0, 1.5, 0)))
            if not q:
                return
            cs.append(q)
    x0, x1 = max(0, int(min(q[0] for q in cs))), min(cam.w, int(max(q[0] for q in cs)) + 2)
    y0, y1 = max(0, int(min(q[1] for q in cs))), min(cam.h, int(max(q[1] for q in cs)) + 2)
    if x0 >= x1 or y0 >= y1:
        return
    xx, yy = np.meshgrid(np.arange(x0, x1), np.arange(y0, y1))
    idx = (yy * cam.w + xx).ravel()
    dx, dy, dz = cam.dx[idx], cam.dy[idx], cam.dz[idx]
    with np.errstate(divide='ignore', invalid='ignore'):
        tt = (1.5 - cam.c[1]) / dy
    Px, Pz = cam.c[0] + tt * dx - cen[0], cam.c[2] + tt * dz - cen[2]
    qx = ((Px * a3[0] + Pz * a3[2]) / hl).reshape(xx.shape)
    qy = ((Px * p3[0] + Pz * p3[2]) / hw).reshape(xx.shape)
    tt = tt.reshape(xx.shape)
    rr = np.sqrt(qx * qx + qy * qy)
    edge = 1 - smooth(0.8, 1.0, rr)
    o = 0.53 if cell == 4 else 0.31 * (cell & 1)
    n = 0.55 * _vnoise(3.4 * qx + 5.0 + o, 3.4 * qy + 5.0 - o) + 0.45 * _vnoise(10.0 * qx + 8.7 - o, 7.0 * qy + 7.1 + o)
    core = 0.55 if cell == 4 else 0.45
    prof = (1 - smooth(core, 1.0, rr * (0.62 + 1.00 * (n - 0.5)))) * edge
    tx = prof * (0.85 + 0.15 * n) if cell == 4 else prof * smooth(0.60, 0.95, n)
    if cell == 5:                                # DPS_SOFT: a plain soft disc (the ground glow)
        tx = (1 - smooth(0.2, 1.0, rr)) * edge
    inside = (np.abs(qx) <= 1) & (np.abs(qy) <= 1) & (tt > 0)
    a = np.where(inside, tx * alpha * float(smooth(NEAR0, NEAR1, float(np.linalg.norm(cen - cam.c)))), 0)
    _blend(S, x0, x1, y0, y1, None, a, color, additive, zz=np.where(inside, tt, 1e9))

def draw_atlas8(S, cam, pos, radius, cell, color, alpha, rot, additive, minpx=0.0):
    """FXX_SVATLAS cells 10 (orb: hot core, soft halo) and 7 / 8 (ash flakes), camera facing, near fade, z test"""
    p = cam.project(pos)
    if p is None or alpha < 0.004:
        return
    px, py, z = p
    pr = max(radius * cam.pix_per_unit(z), minpx * cam.ss)
    alpha *= float(smooth(NEAR0, NEAR1, float(np.linalg.norm(np.asarray(pos) - cam.c))))
    if alpha < 0.004 or pr < 0.5:
        return
    x0, x1 = max(0, int(px - pr)), min(cam.w, int(px + pr) + 1)
    y0, y1 = max(0, int(py - pr)), min(cam.h, int(py + pr) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    gx, gy = np.meshgrid((np.arange(x0, x1, dtype=np.float32) + 0.5 - px) / pr * 0.5, (np.arange(y0, y1, dtype=np.float32) + 0.5 - py) / pr * 0.5)
    c, s = math.cos(rot), math.sin(rot)
    du, dv = gx * c - gy * s, gx * s + gy * c
    cr = np.sqrt(du * du + dv * dv)
    edge = 1 - smooth(0.38, 0.5, cr)
    if cell == 10:
        a = np.exp(-(cr / 0.05) ** 2) + 0.45 * np.exp(-(cr / 0.15) ** 2) + 0.15 * np.exp(-(cr / 0.30) ** 2)
    else:
        axx, ayy = (1.0, 1.9) if cell == 7 else (1.3, 1.45)
        nn = _vnoise(du * 9.0 + 3.0, dv * 9.0)
        n2 = _vnoise(du * 5.0 + 9.0, dv * 5.0 + 2.0)
        bend = 0.18 * du * du * 8.0 if cell == 8 else 0.0
        rr = np.sqrt((du * axx) ** 2 + ((dv - bend) * ayy) ** 2) * (1 + 0.55 * (nn - 0.5))
        a = (1 - smooth(0.24, 0.34, rr)) * (0.55 + 0.45 * n2)
    _blend(S, x0, x1, y0, y1, z, np.clip(a * edge, 0, 1) * alpha, color, additive)

def trail_items8(cam, S, pb, t, strong, mode, vel, stop=0.0):
    """DpHerTrail: draws the footprints now (on the pool, under every sprite) and returns the sprite items (key, fn); additive
    items sort after every premultiplied one (as DpSlCmp)"""
    items = []
    look = {'normal': 0, 'th': 1, 'nv': 2, 'sv': 3}[mode]
    sv = look == 3
    lvA = HALO7_A[1 if strong else 0] / 0.92
    nvA = 1.3 if look == 2 else 1.0
    lookc = 0.0 if look == 2 else 0.08 if look == 1 else 1.0
    aims = [np.array((0.0, 12.0, 0.0))] + ([pb['pelvis']] if pb is not None else [])
    def tun(p, size):
        e = min(seg_dist(np.asarray(p, dtype=np.float64), cam.c, a) for a in aims)
        return float(smooth(0.10 * size + 6.0, 0.60 * size + 26.0, e))
    def add_item(p, fn, bias, additive):
        z = cam.project(p)
        if z:
            items.append((z[2] - bias - (1e6 if additive else 0.0), fn))
    def tongue(base, tip, hw, f, al, cell=-1, rim=1.0, hot=0.0):
        if sv:
            add_item(base, partial(draw_flame8, S, cam, base, tip, hw * 1.25, TR8_SV, 0.15 * al, True, cell), 0.0, True)
            add_item(base, partial(draw_flame8, S, cam, base, tip, hw, TR8_SVHOT, 0.20 * al, True, cell), 0.0, True)
            return
        if look == 0 and rim > 0:
            m = float(smooth(0.30, 0.85, f)) * (1 - hot)
            rc = tuple(lerp(lerp(TR8_RIM[0][c], TR8_RIM[1][c], m), TR8_HOT[c], hot) for c in range(3))
            add_item(base, partial(draw_flame8, S, cam, base, base + (tip - base) * 1.08, hw * 1.25, rc, 0.65 * rim * al * (1 + 0.5 * hot), False, cell), TR8_BIAS - 3.0, False)
        add_item(base, partial(draw_flame8, S, cam, base, tip, hw, HALO7_COL, min(al * nvA, 0.95), False, cell), TR8_BIAS, False)
    def carry(q, ta, tau):
        """DpTrCarry: her slipstream (v8b)"""
        c = 0.85 * q['sp'] * tau * (1 - math.exp(-ta / tau)) if TR8B and ta > 0 else 0.0
        return np.array((q['dir'][0] * c, 0.0, q['dir'][1] * c))
    decals = [[], [], []]                       # glow (additive), print (black), cracks (additive)
    for q in trail_nodes8(t, vel, stop):
        age = t - q['t0']
        if age < 0:
            continue
        Iw = float(smooth(TR8['VMIN'], 120.0, q['sp']))
        R = float(smooth(250.0, 600.0, q['sp']))
        dx_, dz_ = q['dir']
        px, pz = -dz_, dx_
        hot = TRV(0.0, 1 - float(smooth(0.25, 0.9, age)))
        P, seg, qid = q['p'], q['seg'], q['id']
        nT = (5 if strong else 3) + int(TRV(2.0, 3.0) * R + 0.5)
        nE = (TRV(4, 6) if strong else TRV(2, 3)) + int(TRV(2.0, 3.0) * R + 0.5)
        nA = (2 if strong else 1) + int(R + 0.5)
        nW = 1 + int(R + 0.5)
        # the footprint
        k = (qid * 911 + 5) & M32
        rot = (FxRnd(k) - 0.5) * 0.35
        ax = (dx_ * math.cos(rot) - dz_ * math.sin(rot), dx_ * math.sin(rot) + dz_ * math.cos(rot))
        hl, hw = 15.0 * (1 + 0.25 * R), 7.5 * (1 + 0.15 * R)
        ab = 0.80 * lvA * Iw * float(smooth(0, 0.10, age) * (1 - smooth(1.0, TR8['LIFE'], age)))
        ag = 0.90 * lvA * Iw * float(smooth(0, 0.08, age) * (1 - smooth(0.15, 1.5, age))) * (0.75 + 0.25 * math.sin(6.1 * t + 6.2831853 * FxRnd(k + 1)))
        aw = 0.70 * lvA * Iw * float(smooth(0.05, 0.20, age) * (1 - smooth(0.6, 1.6, age)))
        if look in (0, 3) and aw > 0.004:
            m = float(smooth(0.4, 1.4, age))
            gc = TR8_SV if sv else tuple(lerp(TR8_GLOW[0][c], TR8_GLOW[1][c], m) for c in range(3))
            decals[0].append((P, (dx_, dz_), 30.0 * (1 + 0.3 * R), 22.0 * (1 + 0.3 * R), 5, gc, aw * (0.5 if sv else 1.0), True))
        if not sv and ab > 0.004:
            decals[1].append((P, ax, hl, hw, 4, TR8_ASH, min(ab * nvA, 0.95), False))
        if look != 1 and ag > 0.004:
            decals[2].append((P, ax, hl * 0.85, hw * 0.8, 2 + (qid & 1), TR8_SV if sv else NV8_COL if look == 2 else TR8_CRACK,
                              ag * (0.35 if sv else 0.45 if look == 2 else 1.0), True))
        for j in range(nT):                          # tongues
            k = (qid * 7919 + j * 131 + 17) & M32
            u, lat0 = FxRnd(k), (-1 if FxRnd(k + 1) < 0.5 else 1) * (TRC(TRV(10.0, 14.0), 18.0) + (TRV(16.0, 14.0) + TRC(TRV(8.0, 10.0), 14.0) * R) * FxRnd(k + 12))   # a wake flanking her path
            ta = age - (TRV(0.06, 0.03) + TRV(0.22, 0.12) * FxRnd(k + 2)) * (1 - 0.6 * R)
            life = lerp(0.9, 1.8, FxRnd(k + 3)) * (1 - TRV(0.25, 0.0) * R)
            H0 = (TRV(30.0, 44.0) + TRV(50.0, 66.0) * FxRnd(k + 4) ** 2) * (1 + TRV(0.45, 0.25) * R) * Iw * (1.0 if strong else 0.85) * (1 + TRC(0.0, 0.4) * hot)
            wr, ph = lerp(0.50, 0.68, FxRnd(k + 5)), 6.2831853 * FxRnd(k + 6)
            if ta < 0 or ta >= life:
                continue
            f = ta / life
            grow, burn = float(smooth(0, 0.10, f)), 1 - float(smooth(0.35, 1.0, f))
            Hh = H0 * grow * (0.35 + 0.65 * burn) * (1 + 0.16 * math.sin(8.7 * t + ph) + 0.09 * math.sin(14.3 * t + 2.1 * ph))
            hwf = 0.5 * H0 * wr * grow * (0.60 + 0.40 * burn)
            lat = lat0 + (-1 if lat0 < 0 else 1) * (TRV(9.0, 55.0) + TRV(9.0, 150.0) * R) * ta
            base = np.array((P[0] - dx_ * u * seg + px * lat, 16.0 * (H0 / 40.0) * float(smooth(0.5, 1.0, f)), P[2] - dz_ * u * seg + pz * lat)) + carry(q, ta, TRC(0.08, 0.20))
            lean, sway = (0.25 + 0.25 * R) * Hh * (1 - f), Hh * ((FxRnd(k + 9) - 0.5) * 0.4 + 0.12 * math.sin(5.3 * t + ph))
            tip = np.array((base[0] - dx_ * lean + px * sway, base[1] + Hh, base[2] - dz_ * lean + pz * sway))
            al = 0.88 * lvA * float(smooth(0, 0.06, f) * (1 - smooth(0.62, 1.0, f)))
            tn = 0.4 + 0.6 * tun(base, hwf * 2)
            bend = Hh * (0.10 + 0.10 * FxRnd(k + 10)) * math.sin(7.1 * t + 1.7 * ph) * (-1 if FxRnd(k + 11) < 0.5 else 1)
            pv = np.array((px, 0.0, pz))
            tongue(base, base + (tip - base) * 0.62, 1.15 * hwf, f, al * tn, k & 1, 1.0, hot)      # the ragged body, glowing edge
            tongue(base + (tip - base) * 0.30 + pv * 0.4 * bend, tip + pv * bend, 0.62 * hwf, f, al * tn, -1, TRV(0.6, 0.35), hot)   # the tongue, bent
            P2 = lerp(0.22, 0.34, FxRnd(k + 7))     # the lick: torn off the top, born again every P2 s
            x = ta / P2 + FxRnd(k + 8)
            g = x - math.floor(x)
            kc = (k * 31 + int(math.floor(x)) * 977) & M32
            Hl, side = 0.45 * Hh * (1 - 0.6 * g), (FxRnd(kc) - 0.5) * 0.5 * Hh
            ga = float(smooth(0, 0.2, g) * (1 - smooth(0.5, 1.0, g)))
            lb = base + (tip - base) * (0.55 + 0.5 * g) + np.array((px * side, 0.0, pz * side))
            lt = np.array((lb[0] - dx_ * 0.3 * lean + px * 0.5 * side, lb[1] + Hl, lb[2] - dz_ * 0.3 * lean + pz * 0.5 * side))
            tongue(lb, lt, 0.6 * hwf * (1 - 0.5 * g), f, 0.9 * ga * al * tn, -1, 0.0)   # black only
        if look != 1:
            for j in range(nE):                      # embers
                k = (qid * 4567 + j * 197 + 29) & M32
                u, lat = FxRnd(k), (2 * FxRnd(k + 1) - 1) * (10.0 + 12.0 * R)
                ta = age - (TRV(0.10, 0.05) + TRV(0.50, 0.35) * FxRnd(k + 2)) * (1 - 0.6 * R)
                life = (TRV(1.0, 1.3) + 0.9 * FxRnd(k + 3)) * (1 - TRV(0.0, 0.4) * R)
                rise, r = lerp(TRC(TRV(60.0, 120.0), 70.0), TRC(TRV(120.0, 220.0), 140.0), FxRnd(k + 4)) * (1 + 0.3 * R), lerp(1.4, 2.6, FxRnd(k + 5))
                ph = 6.2831853 * FxRnd(k + 6)
                if ta < 0 or ta >= life:
                    continue
                f = ta / life
                if TR8B:                             # v8b: the wake
                    lat = (-1 if FxRnd(k + 8) < 0.5 else 1) * (TRC(14.0, 24.0) + TRC(20.0, 26.0) * FxRnd(k + 1) + (33.0 + 90.0 * R) * ta)
                cu = 9.0 * math.sin(3.1 * t + ph) * f + lat
                p = np.array((P[0] - dx_ * (u * seg + 14 * f) + px * cu, TRC(6.0, 30.0 + 40.0 * FxRnd(k + 9)) + rise * (1 - (1 - f) ** 2), P[2] - dz_ * (u * seg + 14 * f) + pz * cu)) + carry(q, ta, TRC(0.30, 0.35))
                b = 1.8 * float(smooth(60.0, 140.0, float(np.linalg.norm(p - cam.c)))) * lvA * float(smooth(0, 0.08, f) * (1 - smooth(0.55, 1.0, f))) * (0.6 + 0.4 * math.sin(19.0 * t + ph))
                col = TR8_SVHOT if sv else NV8_COL if look == 2 else TR8_SPARK[0 if FxRnd(k + 7) < 0.75 else 1]
                add_item(p, partial(draw_atlas8, S, cam, p, 4.0 * r, 10, col, b, ph, True, 1.3), 0.0, True)
        if not sv:
            for j in range(nA):                      # ash
                k = (qid * 2711 + j * 61 + 41) & M32
                u, lat = FxRnd(k), (2 * FxRnd(k + 1) - 1) * (10.0 + 10.0 * R)
                ta = age - (0.20 + 0.60 * FxRnd(k + 2))
                life = 1.4 + 1.0 * FxRnd(k + 3)
                rise, sz, ph = lerp(40.0, 90.0, FxRnd(k + 4)), lerp(2.4, 4.2, FxRnd(k + 5)), 6.2831853 * FxRnd(k + 6)
                spin = (-1 if FxRnd(k + 7) < 0.5 else 1) * (2.0 + 3.0 * FxRnd(k + 8))
                if ta < 0 or ta >= life:
                    continue
                f = ta / life
                cu = lat + 12.0 * math.sin(1.7 * t + ph) * f
                p = np.array((P[0] - dx_ * (u * seg + 10 * f) + px * cu, 8.0 + rise * (1 - (1 - f) ** 2), P[2] - dz_ * (u * seg + 10 * f) + pz * cu)) + carry(q, ta, 0.25)
                al = 0.85 * float(smooth(60.0, 140.0, float(np.linalg.norm(p - cam.c)))) * lvA * nvA * float(smooth(0, 0.10, f) * (1 - smooth(0.60, 1.0, f))) * tun(p, sz)
                add_item(p, partial(draw_atlas8, S, cam, p, 1.6 * sz, 7 + (k & 1), TR8_ASH, min(al, 0.95), ph + spin * t, False, 1.0), TR8_BIAS, False)
        for j in range(nW):                          # dark heat wisps
            k = (qid * 3203 + j * 89 + 53) & M32
            u, lat = FxRnd(k), (2 * FxRnd(k + 1) - 1) * (8.0 + 10.0 * R)
            ta = age - (0.15 + 0.30 * FxRnd(k + 2))
            life = 1.6 + 0.8 * FxRnd(k + 3)
            rise, r0 = lerp(TRC(90.0, 50.0), TRC(150.0, 80.0), FxRnd(k + 4)) * (1 + 0.25 * R), lerp(16.0, 24.0, FxRnd(k + 5))
            ph = 6.2831853 * FxRnd(k + 6)
            if ta < 0 or ta >= life:
                continue
            f = ta / life
            rad = r0 * (1 + 0.8 * f)
            cu = lat + 8.0 * math.sin(1.3 * t + ph)
            p = np.array((P[0] - dx_ * u * seg + px * cu, 28.0 + rise * f ** 0.8, P[2] - dz_ * u * seg + pz * cu))
            al = float(smooth(60.0, 140.0, float(np.linalg.norm(p - cam.c)))) * tr8_big(cam, p, 1.7 * rad) * lvA * float(smooth(0, 0.15, f) * (1 - smooth(0.40, 1.0, f)))
            st = (0.0, 1.0, 1.7)
            if sv:
                add_item(p, partial(draw_smoke5, S, cam, p, rad, TR8_SV, 0.05 * al, ph, additive=True, stretch=st), 0.0, True)
            else:
                col = tuple(c * lookc for c in TR8_ASH)
                add_item(p, partial(draw_smoke5, S, cam, p, rad, col, TRC(0.38, 0.55) * nvA * al * tun(p, rad), ph, stretch=st), 20.0, False)
        if TR8B and age < 1.4:
            for j in range(2 + int(R + 0.5)):         # v8b: black smoke columns off the nearest nodes, both sides of her path
                k = (qid * 5303 + j * 71 + 61) & M32
                side = 1 if (j & 1) else -1
                ta = age - 0.08 * FxRnd(k)
                life = (0.8 + 0.4 * FxRnd(k + 1)) * (1 - 0.5 * R)
                rise, r0, ph = lerp(TRC(150.0, 45.0), TRC(230.0, 75.0), FxRnd(k + 2)), lerp(TRC(18.0, 14.0), TRC(28.0, 22.0), FxRnd(k + 3)), 6.2831853 * FxRnd(k + 4)
                u = FxRnd(k + 6)
                if ta < 0 or ta >= life:
                    continue
                f = ta / life
                rad = r0 * (1 + TRC(1.0, 0.5) * f)
                lat = side * (24.0 + 18.0 * FxRnd(k + 5) + (40.0 + 110.0 * R) * ta) + 6.0 * math.sin(1.9 * t + ph)
                p = np.array((P[0] - dx_ * u * seg + px * lat, TRC(40.0, 25.0) + rise * f ** 0.75, P[2] - dz_ * u * seg + pz * lat)) + carry(q, ta, 0.30)
                al = TRC(0.50, 0.45) * lvA * float(smooth(60.0, 140.0, float(np.linalg.norm(p - cam.c)))) * tr8_big(cam, p, 1.5 * rad) * float(smooth(0, 0.12, f) * (1 - smooth(0.45, 1.0, f)))
                st = (0.0, 1.0, TRC(1.5, 1.3))
                if sv:
                    add_item(p, partial(draw_smoke5, S, cam, p, rad, TR8_SV, 0.06 * al, ph, additive=True, stretch=st), 0.0, True)
                else:
                    col = tuple(c * lookc for c in TR8_ASH)
                    add_item(p, partial(draw_smoke5, S, cam, p, rad, col, min(al * nvA * tun(p, rad), 0.9), ph, stretch=st), 20.0, False)
    for b in (0, 1, 2):
        for P, ax, hl, hw, cell, col, a, addv in decals[b]:
            draw_decal8(S, cam, P, ax, hl, hw, cell, col, a, addv)
    return items

def render_fog8(S, cam, R, t, victims, zback, mode, strong, pb, vel, stop=0.0):
    """render_fog7 + the trail: the footprints after the floor pool, the trail sprites sorted with the rest"""
    Q = P5[strong]
    wd = wall_distances(zback)
    rmax = MASK_OUT * R
    S['zbimg'] = S['zb'].reshape(cam.h, cam.w)
    S['imgimg'] = S['img'].reshape(cam.h, cam.w, 3)
    S2 = S
    if mode == 'sv':
        S['svlayer'] = np.zeros_like(S['imgimg'])
        S2 = dict(S, imgimg=S['svlayer'])
    cm, am = (NV_CMUL, NV_AMUL) if mode == 'nv' else (TH6_CM, 1.0) if mode == 'th' else (1.0, 1.0)
    tables = [layer_mask_table(cam, R, t, k * 2.1, wd, victims, y=L['y']) for k, L in enumerate(LAYERS4)]
    for k in (0, 1):
        L = LAYERS4[k]
        tab = tables[k]
        if mode == 'sv':
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add', hcol=SV_COL, hgain=SV_POOL[k])
        else:
            disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, Q['FLOOR'][k], t, 'body', amul=am, cmul=cm)
            if k == 1 and mode == 'normal':
                disc_pass(S, cam, 0, 0, rmax, L['y'], RINGS, SECTORS, tab, L, 1.0, t, 'add')
    if mode != 'sv':
        for vi, (pos, s) in enumerate(victims):
            rad = lerp(VSLAB_R[0], VSLAB_R[1], s) * (0.5 + 0.5 * s)
            Mv = layer_mask_table(cam, R, t, 3.0 + vi, wd, victims, cx=pos[0], cz=pos[2], y=VSLAB_Y, rmax=rad, rings=8,
                                  sectors=24, m_in=0.5, m_out=1.0, walls=False, boost=False)
            disc_pass(S, cam, pos[0], pos[2], rad, VSLAB_Y, 8, 24, Mv, LAYERS4[1], VSLAB_DENS * (0.5 + 0.5 * s), t, 'body', amul=am, cmul=cm)
    items = trail_items8(cam, S, pb, t, strong, mode, vel, stop) if TRAIL8 else []
    for pos, size, al, rot in volume_list5(R, t, strong):
        z = cam.project(pos)
        if not z:
            continue
        if mode == 'sv':
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, SV_COL, SV_VOL_A * al / VOL5_ALPHA * 0.6 * SV6_VOL, rot, additive=True)))
        else:
            al = al * tunnel(cam, pos, size)
            col = tint_col(pos[0], pos[2], t) * VOL_BRIGHT * cm
            items.append((z[2], partial(draw_smoke5, S, cam, pos, size, col, min(al * am, 0.9), rot, glow=VOL_GLOW if mode == 'normal' else 0.0, floor=True)))
    if mode != 'sv':
        for pos, hw, hh, col, al, ph in wisp_list(R, t)[: (24 if not strong else 48)]:
            z = cam.project(pos)
            if z:
                items.append((z[2], partial(draw_wisp, S, cam, pos, hw, hh, tuple(c * cm for c in col), al * am * (0.6 if not strong else 1.0), ph, t)))
    else:
        items += vein_items5(cam, S2, R, t)
    if pb is not None:
        hv = tuple(vel) if not stop else (0.0, 0.0, 0.0)
        if mode in ('normal', 'nv', 'th'):
            items += halo_items7(cam, S, pb, t, strong, cm, am, hv, 0.9 if mode == 'nv' else HALO7_CAP)
        if mode in ('normal', 'sv'):
            cs = (SV5['VEIN'], SV5['VEIN_HALO']) if mode == 'sv' else None
            items += tendril_items5(cam, S2, pb, t, strong, SV7_TEND if mode == 'sv' else 1.0, cs)
        if mode == 'nv':
            g = tuple(c * NV7_TEND_GAIN for c in NV7_COL)
            items += tendril_items5(cam, S, pb, t, strong, 1.0, (g, tuple(c * 0.55 for c in g)))
        if mode == 'sv':
            for name, rad in (('chest', 40), ('pelvis', 36), ('head', 26)):
                z = cam.project(pb[name])
                if z:
                    items.append((z[2], partial(draw_smoke, S, cam, away(cam, pb[name], 10), rad, SV_COL, SV6_TORSO, 1.0, additive=True,
                                                size_fade=False, core=0.1)))
    for vi, (pos, s) in enumerate(victims):
        if s > 0.02:
            items += victim_items(cam, S, vi, pos, s, t, mode)
    items.sort(key=lambda it: -it[0])
    for z, fn in items:
        fn()

def render_frame8(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True, vel=(0.0, 0.0, 0.0), stop=0.0, v7=False, v8=None, v8b=None):
    """v8=True / False: this frame with the v8 / v8b+ trail (None: as TR8B); v8b=True / False: v8b / v8c (None: as TR8C)"""
    if v8 is not None:
        with params(TR8B=not v8):
            return render_frame8(R, t, cam, victims, mode, zback, ss, player, vel, stop, v7, None, v8b)
    if v8b is not None:
        with params(TR8C=not v8b):
            return render_frame8(R, t, cam, victims, mode, zback, ss, player, vel, stop, v7)
    return _render_frame8(R, t, cam, victims, mode, zback, ss, player, vel, stop, v7)

def _render_frame8(R, t=2.0, cam='ots', victims=(), mode='normal', zback=ROOM_BACK, ss=None, player=True, vel=(0.0, 0.0, 0.0), stop=0.0, v7=False):
    """render_frame7 with the v8 trail (v7=True: the same frame through render_fog7: the before)"""
    global FOG_TEX, GRIME_TEX
    ss = ss or SS
    if FOG_TEX is None:
        FOG_TEX = make_tex(128, 1, 4, TEX_WARP)
        GRIME_TEX = make_tex(128, 7, 5, 0.0, base=3)
    ov = dict(V5_OVERRIDES, NEAR0=V4_NEAR[0], NEAR1=V4_NEAR[1], WISP_HALF_H=WISP4_HALF_H, LAMP_I=V4_LAMP_I, SV5=SV7)
    if mode == 'sv':
        ov.update(SV_COL=SV7['COL'], SV_POOL=SV7['POOL'], SV_DESAT=SV7['DESAT'], VGLOW_COL=SV7['COL'], VRIM_COL=SV7['COL'])
    if mode == 'nv':
        ov.update(NV_CMUL=NV6_CMUL, NV_AMUL=NV6_AMUL)
    with params(**ov):
        c = Cam(ss, CAMS[cam])
        rgb, zb = render_scene(c, zback, 0.0, player=False)
        if mode == 'th':
            rgb = (np.clip(_lum(rgb) * 1.8, 0, 1) ** 0.8 * TH_ENV * 0.5)[:, None].repeat(3, 1).astype(np.float32)
        vs = [(p, s) for p, s in victims]
        walking = float(np.linalg.norm(vel)) > 0 and not stop
        pb = player_bones(t, walking) if player else None
        if player:
            pc = (TH_PLAYER,) * 3 if mode == 'th' else (0.30, 0.26, 0.27)
            draw_player(c, rgb, zb, pb, pc)
        cols = [(0.85 * (1 - 0.7 * s),) * 3 for p, s in vs] if mode == 'th' else None
        rim = draw_victims(c, rgb, zb, vs, t, cols=cols, rim=(mode in ('normal', 'sv')))
        if mode == 'th':
            rgb[:] = filter_th(rgb)
        if mode == 'nv':
            rgb[:] = nv_post7(rgb)
        S = dict(img=rgb, zb=zb)
        if v7:
            render_fog7(S, c, R, t, vs, zback, mode, R >= 350, pb, vel if not stop else (0.0, 0.0, 0.0))
        else:
            render_fog8(S, c, R, t, vs, zback, mode, R >= 350, pb, vel, stop)
        if rim is not None:
            S['img'] += rim[:, None] * np.array(SV_COL if mode == 'sv' else VRIM_COL, dtype=np.float32)
        img = np.clip(S['img'].reshape(c.h, c.w, 3), 0, 1)
        if mode == 'sv':
            tot = img + S['svlayer']
            img = tot / np.maximum(1.0, tot.max(axis=-1, keepdims=True))
        im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
        im = im.resize((W, H), Image.BOX) if ss > 1 else im
        f = np.asarray(im, dtype=np.float32) / 255.0
        if mode == 'sv':
            f = filter_sv(f)
        elif mode == 'nv':
            f = nv_final7(f)
    return Image.fromarray((np.clip(f, 0, 1) * 255 + 0.5).astype(np.uint8))

FRAMES8 = [   # name, label, kwargs (v7=True: the before, the same frame through the v7 code)
    ('v7_ots_walk', 'BEFORE v7  Strong OTS, walking 175 u/s', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v7=True)),
    ('ots_walk', 'v8  Strong OTS, walking 175 u/s', dict(R=R_STRONG, cam='ots', vel=WALK8_V)),
    ('ots_run', 'v8  Strong OTS, running 663 u/s', dict(R=R_STRONG, cam='ots', vel=RUN8_V)),
    ('v7_side_walk', 'BEFORE v7  Strong side, walking', dict(R=R_STRONG, cam='side', vel=WALK8_V, v7=True)),
    ('side_walk', 'v8  Strong side, walking', dict(R=R_STRONG, cam='side', vel=WALK8_V)),
    ('side_run', 'v8  Strong side, running', dict(R=R_STRONG, cam='side', vel=RUN8_V)),
    ('v7_feet_walk', 'BEFORE v7  feet close-up, walking', dict(R=R_STRONG, cam='feet', vel=WALK8_V, v7=True)),
    ('feet_walk', 'v8  feet close-up, walking', dict(R=R_STRONG, cam='feet', vel=WALK8_V)),
    ('feet_run', 'v8  feet close-up, running', dict(R=R_STRONG, cam='feet', vel=RUN8_V)),
    ('back34_walk', 'v8  behind-left high, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V)),
    ('back34_run', 'v8  behind-left high, running', dict(R=R_STRONG, cam='back34', vel=RUN8_V)),
    ('back34_stop', 'v8  Weak, stopped 0.8 s ago (burning out)', dict(R=R_WEAK, cam='back34', vel=WALK8_V, stop=0.8)),
    ('th_walk', 'v8  Thermal, behind-left, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V, mode='th')),
    ('nv_walk', 'v8  Night vision, behind-left, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V, mode='nv')),
    ('sv_walk', 'v8  Spirit Vision, behind-left, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V, mode='sv')),
    # extras (not on the sheet)
    ('ots_stand', 'v8  Strong OTS, standing (long)', dict(R=R_STRONG, cam='ots')),
    ('v7_back34_walk', 'BEFORE v7  behind-left, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V, v7=True)),
    ('th_ots_run', 'v8  Thermal OTS running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, mode='th')),
    ('nv_ots_run', 'v8  Night vision OTS running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, mode='nv')),
    ('sv_ots_run', 'v8  Spirit Vision OTS running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, mode='sv')),
]
SHEET8 = [n for n, _, _ in FRAMES8[:15]]
FRAMES8B = [   # v8b (the OTS pass) is the default run: files tr_<name>.png, sheet trail_sheet.png; OTS first: v7 / v8 / v8b
    ('v7_ots_walk', 'BEFORE v7  Strong OTS, walking 175 u/s', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v7=True)),
    ('v8_ots_walk', 'v8  Strong OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v8=True)),
    ('v8b_ots_walk', 'v8b  Strong OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v8=False)),
    ('v7_ots_run', 'BEFORE v7  Strong OTS, running 663 u/s', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v7=True)),
    ('v8_ots_run', 'v8  Strong OTS, running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v8=True)),
    ('v8b_ots_run', 'v8b  Strong OTS, running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v8=False)),
    ('v8b_side_walk', 'v8b  Strong side, walking', dict(R=R_STRONG, cam='side', vel=WALK8_V, v8=False)),
    ('v8b_side_run', 'v8b  Strong side, running', dict(R=R_STRONG, cam='side', vel=RUN8_V, v8=False)),
    ('v8b_feet_walk', 'v8b  feet close-up, walking', dict(R=R_STRONG, cam='feet', vel=WALK8_V, v8=False)),
    ('v8b_feet_run', 'v8b  feet close-up, running', dict(R=R_STRONG, cam='feet', vel=RUN8_V, v8=False)),
    ('v8b_back34_walk', 'v8b  behind-left high, walking', dict(R=R_STRONG, cam='back34', vel=WALK8_V, v8=False)),
    ('v8b_back34_run', 'v8b  behind-left high, running', dict(R=R_STRONG, cam='back34', vel=RUN8_V, v8=False)),
    ('v8b_weak_ots_walk', 'v8b  Weak OTS, walking', dict(R=R_WEAK, cam='ots', vel=WALK8_V, v8=False)),
    ('v8b_ots_stop', 'v8b  Strong OTS, stopped 0.8 s ago', dict(R=R_STRONG, cam='ots', vel=WALK8_V, stop=0.8, v8=False)),
    ('v8b_ots_stand', 'v8b  Strong OTS, standing (long)', dict(R=R_STRONG, cam='ots', v8=False)),
    ('v8b_th_ots_walk', 'v8b  Thermal OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, mode='th', v8=False)),
    ('v8b_nv_ots_walk', 'v8b  Night vision OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, mode='nv', v8=False)),
    ('v8b_sv_ots_walk', 'v8b  Spirit Vision OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, mode='sv', v8=False)),
]
FRAMES8C = [   # v8c (review ART-T1-001) is the default run: files tc_<name>.png, sheet trail_sheet.png; OTS first: v7 / v8b / v8c
    ('v7_ots_walk', 'BEFORE v7  Strong OTS, walking 175 u/s', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v7=True)),
    ('v8b_ots_walk', 'v8b  Strong OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v8b=True)),
    ('v8c_ots_walk', 'v8c  Strong OTS, walking', dict(R=R_STRONG, cam='ots', vel=WALK8_V, v8b=False)),
    ('v7_ots_run', 'BEFORE v7  Strong OTS, running 663 u/s', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v7=True)),
    ('v8b_ots_run', 'v8b  Strong OTS, running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v8b=True)),
    ('v8c_ots_run', 'v8c  Strong OTS, running', dict(R=R_STRONG, cam='ots', vel=RUN8_V, v8b=False)),
] + [(n.replace('v8b_', 'v8c_'), l.replace('v8b ', 'v8c '), dict(kw, v8b=False)) for n, l, kw in FRAMES8B[6:]]
SHEET8_LAB = {n: l for n, l, _ in FRAMES8}
SHEET8_LAB.update({n: l for n, l, _ in FRAMES8C})
SHEET8_LAB.update({n: l for n, l, _ in FRAMES8B})
T8 = 2.0                                        # the frames' time (s)

def make_sheet8(outdir, names=None, out='trail_sheet.png', cols=3, tw=640, pfx='v8_'):
    names = names or SHEET8
    th = tw * H // W
    bar = 30
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * tw, rows * (th + bar)), (18, 18, 22))
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    except Exception:
        font = ImageFont.load_default()
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        fn = os.path.join(outdir, '%s%s.png' % (pfx, n))
        if not os.path.exists(fn):
            continue
        im = Image.open(fn).convert('RGB').resize((tw, th), Image.LANCZOS)
        x, y = (i % cols) * tw, (i // cols) * (th + bar)
        sheet.paste(im, (x, y + bar))
        d.text((x + 8, y + 5), SHEET8_LAB[n], fill=(225, 220, 235), font=font)
    sheet.save(os.path.join(outdir, out))

def main8():
    """v8b is the default (FRAMES8B: tr_*.png, trail_sheet.png); --v8 renders the v8 frames exactly (v8_*.png, trail_sheet_v8.png); --v8b
    the v8b frames (tr_*.png, trail_sheet_v8b.png); the default is v8c (FRAMES8C: tc_*.png, trail_sheet.png);
    --v7 (the old default), --v6, --v5b, --v5, --v4, --v3 run the older versions unchanged"""
    global OUT8, V6, V7, TR8B, TR8C
    if any(a in sys.argv for a in ('--v7', '--v6', '--v5b', '--v5', '--v4', '--v3')):
        if '--v7' in sys.argv:
            sys.argv.remove('--v7')
        return main7()
    V6 = True
    V7 = True
    P5[True] = dict(P5[True], **P5B_STRONG)
    only = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--out':
            OUT8 = args[i + 1]
            i += 1
        elif a == '--only':
            only = args[i + 1].split(',')
            i += 1
        elif '=' in a:
            k, v = a.split('=', 1)
            if k not in globals():
                sys.exit('unknown constant ' + k)
            globals()[k] = eval(v)
        i += 1
    v8, v8b = '--v8' in args, '--v8b' in args
    TR8B = not v8
    TR8C = not v8b
    frames, pfx = (FRAMES8, 'v8_') if v8 else (FRAMES8B, 'tr_') if v8b else (FRAMES8C, 'tc_')
    os.makedirs(OUT8, exist_ok=True)
    import time
    for name, label, kw in frames:
        if only and name not in only:
            continue
        t0 = time.time()
        render_frame8(t=T8, **kw).save(os.path.join(OUT8, '%s%s.png' % (pfx, name)))
        print('wrote', name, '%.0fs' % (time.time() - t0), flush=True)
    if v8:
        make_sheet8(OUT8, out='trail_sheet_v8.png')
    elif v8b:
        make_sheet8(OUT8, names=[n for n, _, _ in FRAMES8B], pfx='tr_', out='trail_sheet_v8b.png')
    else:
        make_sheet8(OUT8, names=[n for n, _, _ in FRAMES8C], pfx='tc_')
    print('sheet done')


if __name__ == '__main__':
    main8()
