"""aura_preview.py - offline preview of the Spirit Vision haunting veins (src/svveins.inc; v3.22.0 PLAN section 8,
v3.23.0 PLAN 5: the crimson halo, roots on the ghost's bones, retract / regrow while down, 4 latched strands).

A faithful port of svveins.inc's tendril maths: the same constants, hashes (FxHash / FxRnd), anchor ray families,
cubic path + sag + travelling waves, widths, feeding pulses, sucker splats, capillaries, nodules, side veins, strain /
snap / recoil, latching onto the player's bones and the medallion let-go. The state machine runs at 60 fps on the
real-time clock exactly as SvVeinsFrame does; the strips are rasterised with the game's blending (bodies
premultiplied ONE / INVSRCALPHA, then the light ONE / ONE with BLENDOP MAX). Not ported (drawn approximately): the atlas
sprites of the haunting core (dark mist puffs, the dim base glow, ash / embers), the ghost's model glow and the Spirit
Vision edge world (blue lines), the depth passes (hidden parts dimmed in game) and the game's collision (a box room).

v3.26.0 (blood-rooted ghost, PLAN 2): also writes blood_t0..3.png, blood_down_regrow.png, blood_white_vs_blood.png,
blood_close.png and blood_sheet.png: the body in the blood ramp (a stand-in texture luminance; kPsGvBlood), the crimson
bone aura (SvbAura), the root seep / drips / floor blots (SvbRootBlood, SvBloodFrame). The blood clock is the render time.
Approximations: the body is the stand-in line figure, SVC_SOFT / SVC_BLOB are guessed profiles, drips / blots read the
CURRENT root (the game records the detach point once), the near fade of sprites and the depth passes are not ported.

Usage:  py -3.10 tools/aura_preview.py [outdir] [blood]     (default staging/feedback_v323/renders)
Writes veins_t0..t3.png (one haunting at 4 times), veins_far.png (gameplay distance), veins_shot02.png (over the
screenshot 02 backdrop, its old red smudges removed), latch_0..3.png (the symbiote latching onto a stick figure),
medallion_0..1.png (the medallion makes them let go) and veins_sheet.png (a contact sheet).
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

V3272 = '--v3272' in sys.argv  # v3.27.3: the root blood spots over the tube's flat start; --v3272 renders the old look (no spots)
V3273 = V3272 or '--v3273' in sys.argv  # v3.28 CORE: the black heart; --v3273 renders the v3.27.3 roots (bones, clots, nodules)
GLINT = False  # v3.28 CORE: a wet glint on the heart (tried in the renders; off)
sys.argv = [a for a in sys.argv if a not in ('--v3272', '--v3273')]
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'staging', 'feedback_v323', 'renders')
SHOT = os.path.join(ROOT, 'staging', 'feedback_v322', 'screens', '02_spirit_hauntings_floating_victims.png')
W, H, SS = 1280, 720, 2          # output size, supersampling
M32 = 0xFFFFFFFF

# ---------------------------------------------------------------- screenfx.inc helpers
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

def FxSmooth(e0, e1, x):
    t = FxSat((x - e0) / (e1 - e0))
    return t * t * (3.0 - 2.0 * t)

def SvEase(s):
    s = FxSat(s)
    return 1.0 - (1.0 - s) * (1.0 - s)

def SVV_R(h, n):
    return FxRnd((h + n * 0x27D4EB2F) & M32)

SV_NEAR0, SV_NEAR1 = 60.0, 250.0
GV_NEAR_W = 10.0
SV_REPEL_K = 0.50
SVP_CRIMSON, SVP_GOLD, SVP_BILE = 0, 1, 2
kSvGold = (1.0, 0.78, 0.35)

# ---------------------------------------------------------------- svveins.inc constants (keep in step)
SVV_H, SVV_T, SVV_N = 4, 7, 28
SVV_CASTS, SVV_CASTS_H = 16, 4
SVV_RAY, SVV_FAR, SVV_OFF, SVV_HUG, SVV_AIR = 650.0, 900.0, 4.0, 2.0, 0.5
SVV_CRAWL0, SVV_CRAWL1 = 40.0, 170.0
SVV_LIFE0, SVV_LIFE1 = 6.0, 4.0
SVV_GROW_S, SVV_STRAIN_S, SVV_RECOIL_S, SVV_SPLAT_FADE = 0.5, 0.25, 0.35, 0.9
SVV_W0, SVV_W1, SVV_MINPX, SVV_MAXH = 20.0, 3.8, 1.5, 0.02  # v3.23.0: MINPX 1.1 -> 1.5
SVV_SAG, SVV_PULSE_S, SVV_PULSE_P, SVV_PULSE_D, SVV_BODY_A = 0.18, 1.4, 0.75, 600.0, 0.90  # v3.23.0: PULSE_P 0.55 -> 0.75
SVV_HALO_X, SVV_HALO_MINPX, SVV_HALO_PULSE, SVV_ROOT_OFF = 3.2, 3.0, 0.6, 8.0  # v3.23.0
SVV_SPLAT_R, SVV_TWITCH_P = 30.0, 0.35
SVV_LATCH_R, SVV_LATCH_GAP, SVV_LATCH_MAX, SVV_LATCH_ALL = 700.0, 0.45, 3, 4  # v3.23.0: ALL 3 -> 4
SVV_SNAP_X, SVV_OUT_S, SVV_WARD_HOLD = 1.4, 1.8, 1.5
SVV_VA, SVV_TURNS = 0.55, 1.5
# v3.28 CORE: the black heart (svveins.inc SVV_CORE_*)
SVV_CORE_R, SVV_CORE_MAXH, SVV_CORE_PULL, SVV_CORE_GROW_S = 18.0, 0.15, 34.0, 0.45
SVV_ROOT_IN, SVV_BEAT_S, SVV_DUB_S, SVV_PUMP = 0.45, 1.3, 0.24, 0.30
SVV_CORE_DRIPS = 2
kSvvCat = (0, 0, 0, 0, 1, 1, 2)
kSvvLimb = ((9, 10), (13, 14), (18, 19), (2, 3), (5, 6))
kSvvWrapR = (24.0, 12.0, 12.0, 15.0, 15.0)
kSvvLimbOrder = (1, 4, 0, 2, 3)
kSvvPal = (
    dict(body=(0.035, 0.002, 0.012), rim=(0.57, 0.030, 0.090), wet=(0.55, 0.180, 0.195), core=(1.00, 0.05, 0.06), hot=(1.00, 0.40, 0.28),
         halo=(0.30, 0.012, 0.035)),
    dict(body=(0.025, 0.028, 0.003), rim=(0.09, 0.11, 0.012), wet=(0.15, 0.17, 0.06), core=(0.48, 0.58, 0.04), hot=(0.95, 1.00, 0.50),
         halo=(0.10, 0.12, 0.010)),
    dict(body=(0.040, 0.025, 0.004), rim=(0.30, 0.19, 0.04), wet=(0.45, 0.38, 0.20), core=(1.00, 0.72, 0.25), hot=(1.00, 0.95, 0.75),
         halo=(0.30, 0.20, 0.05)))

def v3(a): return np.array(a, dtype=np.float64)
def vlen(v): return float(math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]))
def vnorm(v):
    l = vlen(v)
    if l > 1e-6:
        v[:] = v / l
    return l
def SvvPerp(ax):
    up, side = v3((0, 1, 0)), v3((1, 0, 0))
    e1 = np.cross(ax, up if abs(ax[1]) < 0.9 else side)
    vnorm(e1)
    e2 = np.cross(ax, e1)
    return e1, e2
def SvvBack(x, c):
    x = FxSat(x) - 1.0
    return 1.0 + (c + 1.0) * x * x * x + c * x * x
def SvvBump(f):
    d = abs(f - math.floor(f) - 0.5)
    if d >= 0.12:
        return 0.0
    c = math.cos(math.pi * d / 0.24)
    return c * c
def SvvSlotEnv(seed, now, per, p):
    """A hashed event (twitch / clench): fast attack, slow release. Returns (envelope, sign)."""
    tt = now + per * SVV_R(seed, 1)
    sl = math.floor(tt / per)
    tau = tt - sl * per
    hs = FxHash(seed ^ ((int(sl) * 0x2C1B3C6D) & M32))
    if FxRnd(hs) >= p:
        return 0.0, 0.0
    r = 1.0 - FxSat((tau - 0.06) / 0.45)
    return FxSat(tau / 0.06) * r * r, (1.0 if hs & 0x100 else -1.0)
def SvvThump(x):  # v3.28 CORE: one heart sound: 0.05 s attack, 0.11 s exponential decay
    if x < 0.0:
        return 0.0
    return x / 0.05 if x < 0.05 else math.exp(-(x - 0.05) / 0.11)
def SvvBeat(t):  # v3.28 CORE: lub-dub on SVV_BEAT_S (the dub SVV_DUB_S after the lub, at 60 %), 0..1
    x = t - math.floor(t / SVV_BEAT_S) * SVV_BEAT_S
    return min(SvvThump(x) + 0.6 * SvvThump(x - SVV_DUB_S), 1.0)
def SvvHeartKey(key):  # v3.28 CORE: the heart's hash (beat phase, wobble, drips)
    return FxHash(((key * 0x9E3779B9) & M32) ^ 0xC0DE)
def SvvHeart(s, now):
    """v3.28 CORE: the heart: a matched skeleton's chest bone 9 (KB 113; fallback the centroid = s.c), else the
    source centre; a slow 1.5-unit drift."""
    c = s.c.copy()
    if s.bones is not None and len(s.bones) > 9 and vlen(s.bones[9] - s.c) < 160.0:
        c = v3(s.bones[9]).copy()
    hh = SvvHeartKey(s.key)
    c[0] += 1.5 * math.sin(0.50 * now + 6.2831853 * SVV_R(hh, 1))
    c[1] += 1.5 * math.sin(0.37 * now + 6.2831853 * SVV_R(hh, 2))
    c[2] += 1.5 * math.cos(0.43 * now + 6.2831853 * SVV_R(hh, 3))
    return c


# ---------------------------------------------------------------- the world of the preview
class Cam:
    """A pinhole camera in the game's convention (y down). xform -> (x, y, w); px = pixels per unit at w = 1."""
    def __init__(self, eye, at, fov_v=50.0, w=W * SS, h=H * SS):
        self.eye, self.w, self.h = v3(eye), w, h
        f = v3(at) - self.eye
        vnorm(f)
        r = np.cross(v3((0, 1, 0)), f)  # y down: (0,1,0) x f = screen right
        vnorm(r)
        d = np.cross(f, r)
        self.f, self.r, self.d = f, r, d
        self.px = 0.5 * h / math.tan(math.radians(fov_v) * 0.5)
    def proj(self, p):
        q = v3(p) - self.eye
        wv = float(q @ self.f)
        if not (wv >= GV_NEAR_W * 0.999):
            return None
        return (0.5 * self.w + self.px * float(q @ self.r) / wv, 0.5 * self.h + self.px * float(q @ self.d) / wv, wv)

class Room:
    """The fake collision: an open-fronted box room (floor y 0, ceiling -420, walls x -520 / 620, back z 520)."""
    planes = [(v3((0, 1, 0)), 0.0), (v3((0, -1, 0)), -420.0), (v3((-1, 0, 0)), -520.0), (v3((1, 0, 0)), 620.0),
              (v3((0, 0, 1)), 520.0)]  # (axis normal, value): p . n == value
    def cast(self, p0, p1):  # nearest hit -> (hit + n x SVV_OFF, n toward p0) or None (as SvvCast)
        best = None
        for n, val in self.planes:
            a, b = float(p0 @ n) - val, float(p1 @ n) - val
            if (a < 0.0) == (b < 0.0) or a == b:
                continue
            t = a / (a - b)
            if not (0.02 < t <= 1.0):
                continue
            if best is None or t < best[0]:
                best = (t, n * (1.0 if a > 0.0 else -1.0))
        if best is None:
            return None
        t, nrm = best
        return p0 + (p1 - p0) * t + nrm * SVV_OFF, nrm

class Src:
    def __init__(self, key, c, R=450.0, H=400.0, pal=SVP_CRIMSON, k=1.0):
        self.key, self.c, self.R, self.pal, self.k = key, v3(c), R, pal, k
        self.kv, self.reach, self.reachT, self.bones = k, 1.0, 1.0, None  # v3.23.0 (svveins: s->kv, reach, reachT, bone[])
        self.y1 = c[1] + 80.0 + 0.5 * (H - 400.0) + 120.0
        self.y0 = self.y1 - H
        self.Rs = min(R * 0.6, 500.0)
        self.Pm = min(R, 350.0)

# ---------------------------------------------------------------- the module state (svveins.inc globals)
class Ten:
    def __init__(self):
        self.st = self.kind = self.bi = 0
        self.ser = 0
        self.a, self.n = v3((0, 0, 0)), v3((0, 1, 0))
        self.t0 = self.life = self.wantT = self.stT = self.lu = self.lth = self.llen = 0.0
        self.outT = -1.0
        self.rcN = self.rcGold = 0
        self.rcT, self.rcC, self.rcP, self.rcW = 0.0, v3((0, 0, 0)), None, None
        self.osOn, self.osSer, self.osT, self.osA, self.osN = 0, 0, 0.0, None, None

class Slot:
    def __init__(self, key):
        self.key, self.t0, self.lastT, self.latchT, self.goldT, self.los = key, 0.0, 0.0, -100.0, -100.0, 0
        self.gulp = self.gulpAcc = 0.0  # v3.28 CORE: the heart's gulp (a feeding pulse at a root, last frame)
        self.t = [Ten() for _ in range(SVV_T)]

class Geo:
    pass

class Veins:
    def __init__(self, room):
        self.room = room
        self.slots = []
        self.B = {}        # the player's bones (index -> world position)
        self.cam = None
        self.unit = 1.0
        self.now = 0.0
        self.strips = []   # (layer, rows: list of rows of (x, y, rgba) or None)
        self.paused = 0
        self.roots = []    # v3.26.0: g_sbRoot of this frame

    def LimbOk(self, bi):
        return 0 <= bi < 5 and kSvvLimb[bi][0] in self.B and kSvvLimb[bi][1] in self.B

    def Setup(self, S, s, ti, now):
        t = S.t[ti]
        hk = FxHash(((s.key * 0x9E3779B9) & M32) ^ 0x5EED)
        ht = FxHash((hk + ti * 0x632BE5AB) & M32)
        Hh = s.y1 - s.y0
        rb = FxSat((0.12 * s.R - 25.0) / 30.0) * 30.0 + 25.0
        hb = FxSat((0.22 * Hh - 30.0) / 50.0) * 50.0 + 30.0
        if t.st == 0:
            return None
        G = Geo()
        G.kind, G.seed, G.t0, G.C = t.kind, ht, t.t0, s.c.copy()
        G.crawl = 0.0
        if t.kind == 2:
            if not self.LimbOk(t.bi):
                return None
            b0, b1 = self.B[kSvvLimb[t.bi][0]], self.B[kSvvLimb[t.bi][1]]
            G.ax = b1 - b0
            G.L0 = b0 + t.lu * (b1 - b0)
            if vnorm(G.ax) < 1.0:
                return None
            G.e1, G.e2 = SvvPerp(G.ax)
            G.rw, G.th0 = kSvvWrapR[t.bi], t.lth
            G.wl = 46.0 if t.bi else 50.0
            G.E = G.L0 - 0.5 * G.wl * G.ax + G.rw * (math.cos(G.th0) * G.e1 + math.sin(G.th0) * G.e2)
        else:
            G.E = t.a.copy()
        d = G.E - s.c
        G.Hc = None
        if not V3273:  # v3.28 CORE: every tendril starts under the heart's disc, toward its end, breathing a little
            G.Hc = SvvHeart(s, now)
            d = G.E - G.Hc
            if vnorm(d) < 1e-3:
                d = v3((1, 0, 0))
            G.R0 = v3((G.Hc[0] + d[0] * SVV_ROOT_IN * SVV_CORE_R + 1.5 * math.sin(0.9 * now + 6.2831853 * SVV_R(ht, 2)),
                       G.Hc[1] + d[1] * SVV_ROOT_IN * SVV_CORE_R + 1.5 * math.sin(0.7 * now + 6.2831853 * SVV_R(ht, 4)),
                       G.Hc[2] + d[2] * SVV_ROOT_IN * SVV_CORE_R + 1.5 * math.cos(0.8 * now + 6.2831853 * SVV_R(ht, 2))))
        elif s.bones is not None and len(s.bones) >= 3:  # v3.23.0 (item 15): on one of the ghost's bones
            b = s.bones[FxHash(ht ^ ((t.ser * 0x9E3779B1) & M32)) % len(s.bones)]
            if vnorm(d) < 1e-3:
                d = v3((1, 0, 0))
            G.R0 = v3((b[0] + d[0] * SVV_ROOT_OFF + 2.0 * math.sin(0.9 * now + 6.2831853 * SVV_R(ht, 2)),
                       b[1] + d[1] * SVV_ROOT_OFF + 2.0 * math.sin(0.7 * now + 6.2831853 * SVV_R(ht, 4)),
                       b[2] + d[2] * SVV_ROOT_OFF + 2.0 * math.cos(0.8 * now + 6.2831853 * SVV_R(ht, 2))))
        else:
          d[1] *= 0.3
          if vnorm(d) < 1e-3:
            d = v3((1, 0, 0))
          G.R0 = v3((s.c[0] + d[0] * rb * 0.55 + 0.35 * rb * (SVV_R(ht, 1) - 0.5) + 5.0 * math.sin(0.9 * now + 6.2831853 * SVV_R(ht, 2)),
                   s.c[1] + hb * (2.0 * SVV_R(ht, 3) - 1.0) + 7.0 * math.sin(0.7 * now + 6.2831853 * SVV_R(ht, 4)),
                   s.c[2] + d[2] * rb * 0.55 + 0.35 * rb * (SVV_R(ht, 5) - 0.5) + 5.0 * math.cos(0.8 * now + 6.2831853 * SVV_R(ht, 2))))
        if t.kind == 0:
            G.n = t.n.copy()
            G.A = t.a - G.n * (SVV_OFF - SVV_HUG)
            q = G.R0 - G.A
            dn = float(q @ G.n)
            G.u = q - dn * G.n
            if vnorm(G.u) < 1.0:
                G.u, _ = SvvPerp(G.n)
            G.wd = np.cross(G.n, G.u)
            G.crawl = min(max(0.3 * vlen(q), SVV_CRAWL0), SVV_CRAWL1)
            G.E = G.A + G.u * G.crawl
        G.taut = FxSat((now - t.stT) / SVV_STRAIN_S) if t.st == 2 else 0.0
        G.D = G.E - G.R0
        G.La = vnorm(G.D)
        if not (G.La > 1.0):
            return None
        G.L = G.La + G.crawl
        G.va = G.La / G.L if t.kind == 0 else SVV_VA if t.kind == 2 else 1.0
        if t.kind == 2:
            G.taut = FxSat((G.La / (t.llen if t.llen > 1.0 else 1.0) - 1.0) / (SVV_SNAP_X - 1.0))
        if t.st == 2 and V3273:  # (v3.28 CORE: the root stays in the heart; the strand thins and goes taut)
            G.R0 = G.R0 - G.D * 0.15 * G.La * G.taut
        out = G.R0 - (s.c if G.Hc is None else G.Hc)
        if vnorm(out) < 1e-3:
            out = G.D.copy()
        v = 0.5 * out + 0.5 * G.D
        vnorm(v)
        ext = min(G.La * 0.3, 70.0)
        G.p1 = G.R0 + v * 0.33 * G.La
        if t.kind == 0:
            G.p2 = G.E + G.n * ext + G.u * 0.6 * ext
        elif t.kind == 2:
            G.p2 = G.E + (math.cos(G.th0) * G.e1 + math.sin(G.th0) * G.e2) * min(ext, 60.0)
        else:
            G.p2 = G.E - G.D * 0.3 * G.La
        m = 0.5 * (G.R0 + G.E)
        vd = m - self.cam.eye
        if vnorm(vd) < 1.0:
            vd = v3((0, 0, 1))
        G.pa = np.cross(G.D, vd)
        if vnorm(G.pa) < 0.05:
            G.pa, _ = SvvPerp(G.D)
        G.pb = np.cross(G.D, G.pa)
        if t.kind == 2:
            bow = (0.22 if t.bi & 1 else -0.22) * G.La
            G.p1 = G.p1 + G.pa * bow
            G.p2 = G.p2 + G.pa * bow * 0.35
        Lh = math.hypot(G.E[0] - G.R0[0], G.E[2] - G.R0[2])
        G.ph, G.ph2 = 6.2831853 * SVV_R(ht, 6), 6.2831853 * SVV_R(ht, 7)
        G.tw, G.twd = SvvSlotEnv(ht ^ 0x7E1C, now, 1.7 + 1.4 * SVV_R(ht, 12), SVV_TWITCH_P)
        G.sag = (0.08 if t.kind == 2 else SVV_SAG) * Lh * (1.0 - G.taut) * (1.0 + 0.12 * math.sin((0.3 + 0.3 * SVV_R(ht, 13)) * now + G.ph)) * (1.0 - 0.5 * G.tw)
        G.amp = (3.0 + 5.0 * SVV_R(ht, 8)) * min(max(G.La / 450.0, 0.7), 1.5) * (1.8 if t.kind == 1 else 1.0) * (1.0 - G.taut)
        G.nw = 0.8 + 1.0 * SVV_R(ht, 9)
        G.fq = 0.5 + 0.9 * SVV_R(ht, 10)
        G.sw = 0.05 * G.La * (1.0 - G.taut) * (0.5 if t.kind == 2 else 1.0)
        G.swf = 0.12 + 0.18 * SVV_R(ht, 14)
        G.per = SVV_PULSE_S * (0.8 + 0.8 * SVV_R(ht, 11)) * (0.45 if t.kind == 2 else 1.0)
        G.pp = 0.95 if t.kind == 2 else SVV_PULSE_P
        G.cyc = min(max(G.L / SVV_PULSE_D, 0.5), 1.6)
        G.g = SvvBack((now - t.t0) / (0.4 if t.kind == 2 else SVV_GROW_S), 1.1) * FxSat(s.reach)  # v3.23.0: x the reach
        G.live = FxSat((now - t.t0 - SVV_GROW_S) / 0.3)
        G.k = s.kv
        G.bph = SVV_BEAT_S * SVV_R(SvvHeartKey(s.key), 4)  # v3.28 CORE: the heart's beat phase (the pump bulge)
        return G

    @staticmethod
    def Pulse(G, v, now):
        f = now / G.per + v * G.cyc + G.ph
        if FxRnd(G.seed ^ ((int(math.floor(f)) * 0x9E3779B1) & M32)) >= G.pp:
            return 0.0
        return G.live * SvvBump(f)

    def At(self, G, v, now):
        va = G.va
        if G.kind == 2 and v > va:
            kk = (v - va) / (1.0 - va)
            th, al = G.th0 + 6.2831853 * SVV_TURNS * kk, G.wl * (kk - 0.5)
            r = G.rw * (1.0 - 0.18 * self.Pulse(G, 1.0, now) - 0.12 * G.tw)
            return G.L0 + al * G.ax + r * (math.cos(th) * G.e1 + math.sin(th) * G.e2)
        if G.kind == 0 and v > va:
            s_ = (v - va) / (1.0 - va)
            env2 = math.sin(math.pi * FxSat(s_))
            m = G.crawl * (0.10 * math.sin(5.0 * s_ + G.ph) + 0.03 * math.sin(0.3 * now + G.ph2)) * env2
            return G.E + (G.A - G.E) * s_ + G.wd * m
        u = v / va
        if G.kind == 1 and u > 0.6:
            u = 0.6
        a, b, c, d = (1 - u) ** 3, 3 * (1 - u) ** 2 * u, 3 * (1 - u) * u * u, u ** 3
        env = math.sin(math.pi * FxSat(v / va))
        w1 = G.amp * env * math.sin(6.2831853 * (G.nw * v + G.fq * now) + G.ph) + G.twd * G.tw * 0.06 * G.La * env
        w2 = 0.5 * G.amp * env * math.sin(6.2831853 * (0.7 * G.nw * v + 0.6 * G.fq * now) + G.ph2) + G.sw * env * math.sin(6.2831853 * G.swf * now + G.ph2)
        if G.g < 1.0:
            gg = G.g if G.g > 0.05 else 0.05
            w1 += 20.0 * (1.0 - G.g) * (v / gg) * (v / gg) * math.sin(11.0 * now + G.ph)
        o = a * G.R0 + b * G.p1 + c * G.p2 + d * G.E + w1 * G.pa + w2 * G.pb
        o[1] += G.sag * 4.0 * u * (1.0 - u)
        if G.kind == 1 and v > 0.6:
            q = (v - 0.6) / 0.4
            ph = 1.25 * math.pi * q ** 1.3 * (1.0 + 0.25 * math.sin(1.4 * now + G.ph))
            r, be = 0.12 * G.La * (1.0 - 0.5 * q), 0.6 * math.sin(0.9 * now + G.ph2)
            pc = math.cos(be) * G.pa + math.sin(be) * G.pb
            o = o + r * (math.sin(ph) * G.D + (1.0 - math.cos(ph)) * pc)
        return o

    def Width(self, G, v, now):
        va = G.va
        w = SVV_W1 * (1.5 if G.kind == 2 else 1.0) if v > va else SVV_W0 + (SVV_W1 * 1.3 - SVV_W0) * FxSat(v / va) ** 0.6
        if v < 0.1:
            w *= 1.0 + 0.3 * (1.0 - v / 0.1)
        w *= 1.0 + 0.08 * math.sin((1.0 + 0.8 * SVV_R(G.seed, 15)) * now + G.ph + 4.0 * v)
        if v <= va:
            w *= 1.0 + 0.3 * math.sin(19.0 * v + G.ph) * math.sin(7.3 * v + G.ph2) - 0.25 * G.tw
        w *= 1.0 - 0.45 * G.taut
        w *= 1.0 + 0.5 * self.Pulse(G, v, now)
        if not V3273 and v < 0.25:  # v3.28 CORE: each heartbeat pumps a bulge a quarter of the way out (delayed with v)
            w *= 1.0 + SVV_PUMP * SvvBeat(now + G.bph - 0.9 * v) * (1.0 - v / 0.25)
        if G.g < 1.0:
            w *= 0.35 + 0.65 * FxSat((G.g - v) / 0.1)
        return w

    # ---- strips
    def Proj(self, p):
        return self.cam.proj(p)

    def Fade(self, p, w, r):
        return FxSmooth(SV_NEAR0, SV_NEAR1, vlen(v3(p) - self.cam.eye) - r)

    @staticmethod
    def Pre(c, a):
        a = FxSat(a)
        return (FxSat(c[0]) * a, FxSat(c[1]) * a, FxSat(c[2]) * a, a)

    @staticmethod
    def Add(c, k):
        k = FxSat(k)
        return (FxSat(c[0] * k), FxSat(c[1] * k), FxSat(c[2] * k), 1.0)

    def Tube(self, P, w, pu, k, detail, pal):
        kBo, kBa = (-1.0, -0.55, 0.0, 0.55, 1.0), (0.0, 1.0, 1.0, 1.0, 0.0)
        kLo = (-1.0, -0.68, -0.48, -0.28, 0.0, 0.45, 1.0)
        kBo0, kBa0 = (-1.0, 0.0, 1.0), (0.0, 1.0, 0.0)
        lo, hi = SVV_MINPX * self.unit, SVV_MAXH * self.cam.h
        n = len(P)
        pr = [self.Proj(p) for p in P]
        nx, ny, hw, fd, lk = [0.0] * n, [0.0] * n, [0.0] * n, [0.0] * n, [1.0] * n
        lx, ly = 1.0, 0.0
        for i in range(n):
            if pr[i] is None:
                continue
            a, b = i - 1, i + 1
            while a >= 0 and pr[a] is None:
                a -= 1
            while b < n and pr[b] is None:
                b += 1
            if a < 0:
                a = i
            if b >= n:
                b = i
            tx, ty = pr[b][0] - pr[a][0], pr[b][1] - pr[a][1]
            tl = math.hypot(tx, ty)
            if tl > 1e-3:
                lx, ly = -ty / tl, tx / tl
            nx[i], ny[i] = lx, ly
            h = 0.5 * w[i] * self.cam.px / pr[i][2]
            hw[i] = min(max(h, lo), hi)
            lk[i] = 0.35 + 0.65 * h / lo if h < lo else 1.0
            fd[i] = k[i] * self.Fade(P[i], pr[i][2], w[i])
        KB, KL = (5, 7) if detail else (3, 3)
        rows = []
        for i in range(n):
            if pr[i] is None:
                rows.append(None)
                continue
            row = []
            for c in range(KB):
                o = kBo[c] if detail else kBo0[c]
                row.append((pr[i][0] + nx[i] * o * hw[i], pr[i][1] + ny[i] * o * hw[i],
                            self.Pre(pal['body'], SVV_BODY_A * fd[i] * (kBa[c] if detail else kBa0[c]))))
            rows.append(row)
        self.strips.append((0, rows))
        rows = []
        for i in range(n):
            if pr[i] is None:
                rows.append(None)
                continue
            p = pu[i]
            rim = [pal['rim'][j] * (0.75 + 0.5 * p) for j in range(3)]
            wet = [pal['wet'][j] * (0.8 + 0.4 * p) for j in range(3)]
            core = [pal['core'][j] * (0.60 + 0.8 * p) + pal['hot'][j] * 0.45 * p * p for j in range(3)]
            if not detail:
                core = [0.8 * rim[j] + 0.35 * core[j] for j in range(3)]
            row = []
            for c in range(KL):
                o = kLo[c] if detail else kBo0[c]
                col = (core if c == 4 else wet if c == 2 else rim) if detail else core
                kk = 0.0 if ((c in (0, 6)) if detail else (c != 1)) else fd[i] * lk[i]
                row.append((pr[i][0] + nx[i] * o * hw[i], pr[i][1] + ny[i] * o * hw[i], self.Add(col, kk)))
            rows.append(row)
        self.strips.append((1, rows))
        if detail:  # v3.23.0 (item 2): the crimson halo (layer 2, additive, drawn before the bodies)
            kHo, kHa = (-1.0, -0.45, 0.0, 0.45, 1.0), (0.0, 0.45, 1.0, 0.45, 0.0)
            hlo = SVV_HALO_MINPX * self.unit
            rows = []
            for i in range(n):
                if pr[i] is None:
                    rows.append(None)
                    continue
                hh = max(SVV_HALO_X * hw[i], hlo)
                col = [pal['halo'][j] * (1.0 + SVV_HALO_PULSE * pu[i]) for j in range(3)]
                rows.append([(pr[i][0] + nx[i] * kHo[c] * hh, pr[i][1] + ny[i] * kHo[c] * hh, self.Add(col, kHa[c] * fd[i])) for c in range(5)])
            self.strips.append((2, rows))

    def Blot(self, c, t1, t2, r, sp, seed, lump, star, layer, rr, rc, minPx):
        pc = self.Proj(c)
        if pc is None:
            return
        fd = self.Fade(c, pc[2], r)
        if not (fd > 0.004):
            return
        if t1 is None:
            rpx = min(max(r * self.cam.px / pc[2], minPx * self.unit), 0.08 * self.cam.h)
        rows = []
        for j in range(sp + 1):
            jj = j % sp
            f = 1.0 - lump + 2.0 * lump * SVV_R(seed, jj) + 0.06 * math.sin(2.3 * self.now + 1.7 * jj)
            f *= (1.0 - star * (0.55 - 0.15 * SVV_R(seed, jj + 40))) if (jj & 1) else (1.0 + star * (0.4 * SVV_R(seed, jj + 40) - 0.15))
            ang = 6.2831853 * (jj + 0.3 * (SVV_R(seed, jj + 80) - 0.5)) / sp + 6.2831853 * SVV_R(seed, 99)
            row = []
            for m in range(len(rr)):
                rad, a = rr[m] * f, rc[m][3] * fd
                col = self.Pre(rc[m], a) if layer in (0, 3) else self.Add(rc[m], a)  # v3.28 CORE: layer 3 premultiplied
                if t1 is not None:
                    pv = self.Proj(c + r * rad * (math.cos(ang) * t1 + math.sin(ang) * t2))
                    row.append(None if pv is None else (pv[0], pv[1], col))
                else:
                    row.append((pc[0] + rpx * rad * math.cos(ang), pc[1] + rpx * rad * math.sin(ang), col))
            rows.append(None if any(x is None for x in row) else row)
        self.strips.append((layer, rows))

    def Splat(self, a, n, seed, scale, k, throb, pal):
        r = SVV_SPLAT_R * (0.85 + 0.3 * SVV_R(seed, 1)) * scale * (1.0 - 0.12 * throb)
        nc = 3 + (SVV_R(seed, 2) < 0.5)
        if not (k > 0.004) or not (r > 0.5):
            return
        t1, t2 = SvvPerp(n)
        c = a - n * (SVV_OFF - SVV_HUG)
        for i in range(nc):
            ph = 6.2831853 * (i + 0.6 * SVV_R(seed, 10 + i)) / nc
            ln = r * (1.5 + 0.9 * SVV_R(seed, 20 + i))
            P, w, pu, kk = [], [], [], []
            for q in range(4):
                s_ = (0.6 + (ln / r - 0.6) * q / 3.0) * r
                bend = 0.2 * ln * math.sin(1.7 * q + 6.2831853 * SVV_R(seed, 30 + i)) * q / 3.0
                P.append(c + s_ * (math.cos(ph) * t1 + math.sin(ph) * t2) + bend * (-math.sin(ph) * t1 + math.cos(ph) * t2))
                w.append(2.2 - 0.45 * q)
                pu.append(0.3 * throb)
                kk.append(k * 0.8)
            self.Tube(P, w, pu, kk, 0, pal)
        b = pal['body']
        sp, st = (12, 0.35) if V3273 else (16, 0.15)  # v3.28 CORE: a rounder sucker (a 12-point star read as a polygon)
        self.Blot(c, t1, t2, r, sp, seed, 0.1, st, 0, (0.0, 1.0, 1.25), ((*b, 0.95 * k), (*b, 0.92 * k), (*b, 0.0)), 1.5)
        h = [pal['halo'][j] * (1.0 + SVV_HALO_PULSE * throb) for j in range(3)]  # v3.23.0: the halo ring
        self.Blot(c, t1, t2, 1.4 * r, sp, seed, 0.1, 0.2 if V3273 else 0.1, 2, (0.0, 0.7, 1.0), ((*h, 0.8 * k), (*h, 0.6 * k), (*h, 0.0)), 2.0)
        rc = ([0.0, 0.0, 0.0, k],
              [pal['rim'][j] * (1.8 + 1.0 * throb) + pal['core'][j] * (0.20 + 0.3 * throb) for j in range(3)] + [k],
              [pal['rim'][j] * 0.6 for j in range(3)] + [k], [0.0, 0.0, 0.0, 0.0])
        self.Blot(c, t1, t2, r, sp, seed, 0.1, st, 1, (0.0, 0.3, 0.55, 1.0), rc, 1.5)

    def Knot(self, p, r, seed, k, pulse, pal, infect):
        sp, lump = (12, 0.28) if V3273 else (18, 0.12)  # v3.28 CORE: round (only the latched bite uses it now)
        b = pal['body']
        self.Blot(p, None, None, r, sp, seed, lump, 0.0, 0, (0.0, 0.75, 1.0), ((*b, 0.95 * k), (*b, 0.9 * k), (*b, 0.0)), 1.5)
        rc = ([pal['core'][j] * ((0.3 + 0.6 * pulse) if infect else (0.1 + 0.5 * pulse)) for j in range(3)] + [k],
              [pal['rim'][j] * (0.7 + 0.5 * pulse) + pal['wet'][j] * 0.15 for j in range(3)] + [k],
              [pal['rim'][j] * 0.45 for j in range(3)] + [k], [0.0, 0.0, 0.0, 0.0])
        self.Blot(p, None, None, r, sp, seed, lump, 0.0, 1, (0.0, 0.5, 0.85, 1.15), rc, 1.5)

    def RootSpot(self, p, r, seed, k, pulse, pal):  # v3.27.3 SvvRootSpot: 18 spokes, lump 0.14, dark body only (v4: no light layer)
        c0 = [pal['body'][j] + 0.32 * pal['rim'][j] for j in range(3)]
        c1 = [pal['body'][j] + 0.22 * pal['rim'][j] for j in range(3)]
        self.Blot(p, None, None, r, 18, seed, 0.14, 0.0, 0, (0.0, 0.82, 1.0), ((*c0, 0.95 * k), (*c1, 0.90 * k), (*c1, 0.0)), 1.5)

    def Flash(self, p, r, k, rgb, minPx):
        self.Blot(p, None, None, r, 8, 0x51, 0.0, 1.0, 1, (0.0, 1.0), ((*rgb, k), (0.0, 0.0, 0.0, 0.0)), minPx)

    # ---- v3.28 CORE: the black heart
    def Disc(self, pc, rpx, hh, layer, rr, rc, ox=0.0, oy=0.0):
        """SvvDisc: a camera-facing round disc (24 spokes, a slow 1.5-2 % wobble) of rings rr (x rpx), colours rc (rgb + a)."""
        sp = 24
        ph, ph2 = 6.2831853 * SVV_R(hh, 5), 6.2831853 * SVV_R(hh, 6)
        rows = []
        for j in range(sp + 1):
            ang = 6.2831853 * (j % sp) / sp
            f = 1.0 + 0.02 * math.sin(3.0 * ang + 0.8 * self.now + ph) + 0.015 * math.sin(5.0 * ang - 1.3 * self.now + ph2)
            row = []
            for m in range(len(rr)):
                col = self.Pre(rc[m], rc[m][3]) if layer in (0, 3) else self.Add(rc[m], rc[m][3])
                row.append((pc[0] + ox + rpx * rr[m] * f * math.cos(ang), pc[1] + oy + rpx * rr[m] * f * math.sin(ang), col))
            rows.append(row)
        self.strips.append((layer, rows))

    def Core(self, S, s, now):
        """SvvCore: the heart (drawn before its tendrils; layers 2 / 3 / 4: the corona first, the black disc and its rim
        light after every tendril layer); its blood records (SVV_CORE_DRIPS drip channels at its lower rim)."""
        hh = SvvHeartKey(s.key)
        pal = kSvvPal[1 if s.pal == SVP_BILE else 0]
        if not (s.kv > 0.004):
            return
        c = SvvHeart(s, now)
        g = SvvBack((now - S.t0) / SVV_CORE_GROW_S, 1.6)
        beat = SvvBeat(now + SVV_BEAT_S * SVV_R(hh, 4))
        gulp = S.gulp
        r = SVV_CORE_R * g * (0.55 + 0.45 * FxSat(s.reach)) * (1.0 + 0.12 * beat + 0.04 * gulp)
        k = s.kv * FxSat((now - S.t0) / 0.15)
        for m in range(SVV_CORE_DRIPS):  # the heart is the only wound: drips from its lower rim, in front of the body
            a = 6.2831853 * SVV_R(hh, 10 + m)
            dv = self.cam.eye - c
            vnorm(dv)
            r0 = c + dv * SVV_CORE_PULL + v3((0.4 * r * math.cos(a), 0.85 * r, 0.4 * r * math.sin(a)))
            self.roots.append(dict(r0=r0, g=g, k=k, pulse=beat, seed=FxHash((hh + m * 0x632BE5AB) & M32),
                                   pal=1 if s.pal == SVP_BILE else 2 if s.pal == SVP_GOLD else 0, core=1))
        pc = self.Proj(c)
        if pc is None or not (r > 0.5):
            return
        fd = self.Fade(c, pc[2], r) * k
        if not (fd > 0.004):
            return
        rpx = min(max(r * self.cam.px / pc[2], 2.5 * self.unit), SVV_CORE_MAXH * self.cam.h)
        hc = [pal['halo'][j] * (1.6 + 2.2 * beat + 1.2 * gulp) for j in range(3)]
        self.Disc(pc, rpx, hh, 2, (0.0, 1.0, 2.1), ((*hc, 0.6 * fd), (*hc, fd), (*hc, 0.0)))  # the corona (additive)
        e = [pal['body'][j] + 0.30 * pal['rim'][j] for j in range(3)]
        self.Disc(pc, rpx, hh, 3, (0.0, 0.78, 0.93, 1.0), ((0.0, 0.0, 0.0, fd), (*pal['body'], fd), (*e, 0.92 * fd), (*e, 0.0)))
        rim = [pal['rim'][j] * (0.9 + 1.0 * beat + 0.6 * gulp) + pal['core'][j] * 0.12 * beat for j in range(3)]
        rin = [pal['rim'][j] * (0.30 + 0.5 * beat) for j in range(3)]
        self.Disc(pc, rpx, hh, 4, (0.74, 0.88, 0.96, 1.02), ((0, 0, 0, 0.0), (*rin, fd), (*rim, fd), (0, 0, 0, 0.0)))
        if GLINT:
            wg = [pal['wet'][j] * (0.30 + 0.15 * beat) for j in range(3)]
            self.Disc(pc, 0.16 * rpx, hh ^ 0x61, 4, (0.0, 1.0), ((*wg, fd), (0, 0, 0, 0.0)), -0.36 * rpx, -0.40 * rpx)

    # ---- state
    def Path(self, G, now):
        P, w, pu = [], [], []
        for i in range(SVV_N):
            v = G.g * i / (SVV_N - 1)
            p, q = self.Pulse(G, v, now), 0.35 * G.tw * G.live
            P.append(self.At(G, v, now))
            w.append(self.Width(G, v, now))
            pu.append(max(p, q))
        return P, w, pu

    def Snap(self, S, s, ti, now, gold):
        t = S.t[ti]
        G = self.Setup(S, s, ti, now)
        if G is not None:
            t.rcP, t.rcW, _ = self.Path(G, now)
            t.rcN, t.rcT, t.rcGold, t.rcC = SVV_N, now, gold, s.c.copy()
        if t.kind == 0 and t.st:
            t.osOn, t.osSer, t.osT, t.osA, t.osN = 1, t.ser, now, t.a.copy(), t.n.copy()
        t.st = t.kind = 0
        t.ser += 1
        t.wantT, t.outT = now, -1.0

    def Anchor(self, S, s, ti, now, pp):
        t = S.t[ti]
        hk = FxHash(((s.key * 0x9E3779B9) & M32) ^ 0x5EED)
        ht = FxHash((hk + ti * 0x632BE5AB) & M32)
        ha = FxHash(ht ^ ((t.ser * 0x85EBCA6B) & M32))
        az = 6.2831853 * (SVV_R(ha, 1) if kSvvCat[ti] else (ti + 0.8 * SVV_R(ha, 1)) / 4.0 + SVV_R(hk, 9))
        if kSvvCat[ti] == 0:
            d = v3((math.cos(az), -0.15 + 0.6 * SVV_R(ha, 2), math.sin(az)))
        elif kSvvCat[ti] == 1:
            d = v3((0.55 * SVV_R(ha, 3) * math.cos(az), 1.0, 0.55 * SVV_R(ha, 3) * math.sin(az)))
        else:
            d = v3((0.5 * math.cos(az), -1.0, 0.5 * math.sin(az)))
        vnorm(d)
        p1 = s.c + d * SVV_RAY
        got = self.room.cast(s.c, p1)
        if got is None and pp is not None and d[1] > 0.3 and s.c[1] < pp[1] and p1[1] >= pp[1]:
            u = (pp[1] - s.c[1]) / (p1[1] - s.c[1])
            hit = s.c + d * SVV_RAY * u
            hit[1] -= SVV_OFF
            got = (hit, v3((0, -1, 0)))
        if got is not None:
            t.a, t.n = got[0].copy(), got[1].copy()
            t.kind = 0
            t.life = SVV_LIFE0 + SVV_LIFE1 * SVV_R(ha, 4)
        else:
            t.a, t.n = s.c + d * SVV_RAY * SVV_AIR, d.copy()
            t.kind = 1
            t.life = 2.5 + 1.5 * SVV_R(ha, 4)
        t.st, t.t0 = 1, now

    def SlotStep(self, S, s, now, pp, castsLeft, latchFree):
        hk = FxHash(((s.key * 0x9E3779B9) & M32) ^ 0x5EED)
        gold = s.pal == SVP_GOLD
        rng = max(s.R, SVV_LATCH_R)
        dist, want, latched, casts, used = 1e9, 0, 0, 0, 0
        if gold:
            S.goldT = now
        if s.reach <= 0.001:  # v3.23.0 (item 17): retracted: nothing left out; the regrow starts clean
            for t in S.t:
                if t.st and t.kind == 2:
                    latchFree[0] += 1
                t.st, t.kind, t.wantT, t.rcN, t.osOn, t.outT = 0, 0, now, 0, 0, -1.0
                t.ser += 1
            return
        if 9 in self.B:
            dist = vlen(self.B[9] - s.c)
            if dist < rng and not gold and now - S.goldT > SVV_WARD_HOLD and s.reach >= 1.0:
                want = min(1 + int(2.0 * (1.0 - dist / rng) + 0.5), SVV_LATCH_MAX)
        for ti in range(SVV_T):
            t = S.t[ti]
            if t.st == 0:
                if s.reachT > 0.0 and now >= t.wantT and casts < SVV_CASTS_H and castsLeft[0] > 0:
                    self.Anchor(S, s, ti, now, pp)
                    casts += 1
                    castsLeft[0] -= 1
                continue
            if t.kind == 2:
                if gold:
                    self.Snap(S, s, ti, now, 1)
                    latchFree[0] += 1
                    continue
                G = self.Setup(S, s, ti, now)
                if G is None:
                    t.st, t.kind, t.wantT = 0, 0, now
                    t.ser += 1
                    latchFree[0] += 1
                    continue
                if dist > rng * 1.1:
                    if t.outT < 0.0:
                        t.outT = now
                else:
                    t.outT = -1.0
                if G.La > SVV_SNAP_X * t.llen or (t.outT >= 0.0 and now - t.outT > SVV_OUT_S):
                    self.Snap(S, s, ti, now, 0)
                    latchFree[0] += 1
                    continue
                latched += 1
                used |= 1 << t.bi
                continue
            if t.st == 1:
                if now - t.t0 > t.life or vlen(t.a - s.c) > SVV_FAR:
                    t.st, t.stT = 2, now
                elif t.kind == 0 and ti == S.los % SVV_T and casts < SVV_CASTS_H and castsLeft[0] > 0 and now - t.t0 > SVV_GROW_S:
                    casts += 1  # v3.23.0 (V322-R2): a wall now between the body and the anchor
                    castsLeft[0] -= 1
                    hit = self.room.cast(s.c, t.a)
                    same = hit is not None and float(hit[1] @ t.n) > 0.95 and abs(float((hit[0] - t.a) @ t.n)) < 8.0
                    if hit is not None and not same and vlen(hit[0] - s.c) < 0.9 * vlen(t.a - s.c):
                        t.st, t.stT = 2, now
            elif t.st == 2 and now - t.stT >= SVV_STRAIN_S:
                self.Snap(S, s, ti, now, 0)
        S.los += 1
        if latched < want and latchFree[0] > 0 and now - S.latchT >= SVV_LATCH_GAP:
            start, b0 = FxHash(hk ^ 0x1A7C) % SVV_T, FxHash(hk ^ 0xB0E5) % 5
            best, bi = -1, -1
            for q in range(SVV_T):
                t = S.t[(start + q) % SVV_T]
                if t.st == 1 and t.kind != 2 and now - t.t0 > SVV_GROW_S:
                    best = (start + q) % SVV_T
                    break
            for q in range(5):
                b = kSvvLimbOrder[(b0 + q) % 5]
                if not (used >> b & 1) and self.LimbOk(b):
                    bi = b
                    break
            if best >= 0 and bi >= 0:
                t = S.t[best]
                self.Snap(S, s, best, now, 0)
                ha = FxHash(FxHash((hk + best * 0x632BE5AB) & M32) ^ ((t.ser * 0x85EBCA6B) & M32))
                t.kind, t.st, t.t0, t.bi = 2, 1, now, bi
                t.lu = 0.3 + 0.35 * SVV_R(ha, 5) if bi else 0.15 + 0.3 * SVV_R(ha, 5)
                t.lth = 6.2831853 * SVV_R(ha, 6)
                t.llen, t.outT = 1.0, -1.0
                G = self.Setup(S, s, best, now)
                if G is not None:
                    t.llen = G.La
                    latchFree[0] -= 1
                else:
                    t.st = t.kind = 0
                S.latchT = now

    def Build(self, S, s, ti, now):
        t = S.t[ti]
        hk = FxHash(((s.key * 0x9E3779B9) & M32) ^ 0x5EED)
        ht = FxHash((hk + ti * 0x632BE5AB) & M32)
        pal = kSvvPal[1 if s.pal == SVP_BILE else 0]
        if t.osOn:
            u = (now - t.osT) / SVV_SPLAT_FADE
            if u >= 1.0 or u < 0.0:
                t.osOn = 0
            else:
                self.Splat(t.osA, t.osN, FxHash(ht ^ ((t.osSer * 0x85EBCA6B) & M32)) ^ 0x5B1A, 1.0 - 0.3 * u,
                           s.k * (1.0 - u) * (1.0 - u), 0.0, pal)
        if t.rcN and 0.0 <= now - t.rcT < SVV_RECOIL_S:
            u = (now - t.rcT) / SVV_RECOIL_S
            e = SvEase(u)
            fr = 1.0 - e
            rp = kSvvPal[2] if t.rcGold else pal
            sh = s.c - t.rcC
            R, rw, rpu, rk = [], [], [], []
            for i in range(SVV_N):
                x = i / (SVV_N - 1) * fr * (SVV_N - 1)
                a = min(int(x), SVV_N - 2)
                f = x - a
                wh = 18.0 * math.sin(math.pi * u) * (i / (SVV_N - 1)) ** 1.5 * math.sin(16.0 * now + 0.6 * i)
                p = t.rcP[a] + (t.rcP[a + 1] - t.rcP[a]) * f + sh
                p[1] += wh
                p[0] += 0.6 * wh
                R.append(p)
                rw.append((t.rcW[a] + (t.rcW[a + 1] - t.rcW[a]) * f) * (1.0 - 0.4 * e))
                rpu.append(0.6 if t.rcGold else 0.0)
                rk.append(s.k * (1.0 - 0.5 * e * e))
            self.Tube(R, rw, rpu, rk, 1, rp)
            if u < 0.4:
                self.Flash(R[-1], 12.0 * (1.0 + 1.5 * u), s.k * (1.0 - u / 0.4) * 0.8, kSvvPal[2]['hot'] if t.rcGold else pal['core'], 2.0)
            if t.rcGold:
                for q in range(8):
                    sv = SVV_R(ht ^ t.ser, 50 + q) - 2.5 * (now - t.rcT)
                    x = (sv - math.floor(sv)) * (SVV_N - 1)
                    a = min(int(x), SVV_N - 2)
                    p = R[a] + (R[a + 1] - R[a]) * (x - a)
                    self.Flash(p, 8.0 + 6.0 * SVV_R(ht, 60 + q), s.k / SV_REPEL_K * (0.6 + 0.4 * math.sin(30.0 * now + q)), kSvGold, 2.0)
        else:
            t.rcN = 0
        G = self.Setup(S, s, ti, now)
        if G is None:
            return
        if V3273:
            self.roots.append(dict(r0=G.R0.copy(), g=G.g, k=G.k * (1.0 - 0.6 * G.taut), pulse=self.Pulse(G, 0.0, now), seed=ht,
                                   pal=1 if s.pal == SVP_BILE else 0))
        else:  # v3.28 CORE: a feeding pulse reaching the heart makes it gulp (next frame)
            S.gulpAcc = max(S.gulpAcc, self.Pulse(G, 0.02, now))
        P, w, pu = self.Path(G, now)
        kk = [G.k * FxSmooth(0.0, 0.09, G.g * i / (SVV_N - 1)) for i in range(SVV_N)]
        if t.kind == 0 and G.g >= 0.85:
            ts = now - t.t0 - 0.85 * SVV_GROW_S
            self.Splat(t.a, t.n, FxHash(ht ^ ((t.ser * 0x85EBCA6B) & M32)) ^ 0x5B1A, SvvBack(ts / 0.2, 2.2) if ts > 0 else 0.0,
                       G.k * (1.0 - 0.6 * G.taut), max(self.Pulse(G, 1.0, now), 0.5 * G.tw), pal)
        self.Tube(P, w, pu, kk, 1, pal)
        if V3273 and not V3272 and s.pal != SVP_GOLD and G.g > 0.05:  # v3.27.3 (src: the budget guard is not ported): blood covers the root's flat end
            gk = G.k * (1.0 - 0.6 * G.taut) * FxSat(G.g / 0.15)
            th = self.Pulse(G, 0.0, now)
            self.RootSpot(G.R0, SVV_W0 * (0.55 + 0.25 * SVV_R(ht, 70)) * (1.0 + 0.06 * th), ht ^ 0xB0B0, gk, th, pal)
            for m in range(2):
                a = 6.2831853 * SVV_R(ht, 72 + m)
                rr = SVV_W0 * (0.25 + 0.2 * SVV_R(ht, 74 + m))
                q = G.R0.copy()
                q[0] += 0.40 * SVV_W0 * math.cos(a)
                q[1] += 0.40 * SVV_W0 * math.sin(a) * 0.8
                q[2] += 0.40 * SVV_W0 * math.sin(a) * 0.6
                self.RootSpot(q, rr, ht ^ ((0xB1B1 + m) & M32), gk * 0.9, th, pal)
        if t.kind == 0:
            for b in range(2):
                sb = 0.15 + 0.6 * SVV_R(ht, 31 + b)
                vb = G.va + (1.0 - G.va) * sb
                gb = SvEase((G.g - vb) / 0.2)
                side = 1.0 if b else -1.0
                ln = (20.0 + 35.0 * SVV_R(ht, 37 + b)) * gb
                if not (gb > 0.05):
                    continue
                base = self.At(G, vb, now)
                Q, bw, bp, bk = [], [], [], []
                for q in range(4):
                    f = q / 3.0
                    kink = 0.25 * ln * math.sin(2.1 * q + 6.2831853 * SVV_R(ht, 40 + b))
                    Q.append(base + (G.wd * side * 0.9 - G.u * 0.45) * ln * f + (G.u + G.wd * 0.3) * kink * f)
                    bw.append(2.0 - 1.2 * f)
                    bp.append(self.Pulse(G, vb + 0.06 * f, now))
                    bk.append(G.k * (1.0 - 0.6 * G.taut))
                self.Tube(Q, bw, bp, bk, 0, pal)
        vd = (0.3 + 0.3 * SVV_R(ht, 33)) * G.va
        gd = FxSat((G.g - vd) / 0.2)
        dl = (14.0 + 26.0 * SVV_R(ht, 34)) * (0.65 + 0.35 * math.sin((0.5 + 0.4 * SVV_R(ht, 35)) * now + G.ph)) * gd
        if V3273 and SVV_R(ht, 36) < 0.7 and gd > 0.05 and G.taut < 0.5:  # (v3.28 CORE: no drips on the air spans)
            base = self.At(G, vd, now)
            Q, bw, bp, bk = [], [], [], []
            for q in range(4):
                f = q / 3.0
                sw = 4.0 * math.sin(1.3 * now + G.ph2 + 0.5 * q) * f * f
                p = base + G.pa * sw
                p[1] += dl * f
                Q.append(p)
                bw.append(1.6 - 0.7 * f)
                bp.append(0.0)
                bk.append(G.k)
            self.Tube(Q, bw, bp, bk, 0, pal)
            self.Knot(Q[3], 2.4 + 0.6 * math.sin(1.1 * now + G.ph), ht ^ 0xD21A, G.k, 0.0, pal, 0)
        for i in range(2 if V3273 else 0):  # (v3.28 CORE: no nodules: the vein centres stay clean)
            vn = (0.15 + 0.6 * SVV_R(ht, 20 + 3 * i)) * G.va
            gn = FxSat((G.g - vn) / 0.08)
            if not (gn > 0.0) or (t.kind == 2 and vn > 0.45):
                continue
            self.Knot(self.At(G, vn, now), 0.75 * self.Width(G, vn, now) * gn * (0.9 + 0.3 * SVV_R(ht, 21 + 3 * i)),
                      ht ^ (0x4B0D + i), G.k, self.Pulse(G, vn, now), pal, 0)
        if t.kind == 2 and G.g >= 1.0:
            tl = now - t.t0
            pe = self.Pulse(G, 1.0, now)
            sp = 36.0 * FxSat((tl - 0.4) / 1.5)
            dv = self.cam.eye - G.L0
            vnorm(dv)
            self.Knot(G.L0 + dv * G.rw, (0.3 * G.rw + 3.0) * (1.0 + 0.25 * pe) * FxSat(tl / 0.3), ht ^ 0x1F3C, G.k, pe, pal, 1)
            if sp > 2.0:
                for sgn in (-1, 1):
                    Q, bw, bp, bk = [], [], [], []
                    for q in range(4):
                        f = q / 3.0
                        th = G.th0 + 0.5 * math.sin(q + 6.2831853 * SVV_R(ht, (70 + sgn) & M32))
                        Q.append(G.L0 + G.ax * sgn * sp * f + 0.95 * G.rw * (math.cos(th) * G.e1 + math.sin(th) * G.e2))
                        bw.append(2.2 - 1.4 * f)
                        bp.append(self.Pulse(G, 1.0 - 0.1 * f, now))
                        bk.append(G.k)
                    self.Tube(Q, bw, bp, bk, 0, pal)

    def Frame(self, now, srcs, pp, build=True):
        """SvVeinsFrame: srcs sorted nearest first; pp = the player's feet (None: no player)."""
        self.now = now
        self.strips = []
        self.roots = []
        castsLeft = [SVV_CASTS]
        latchFree = [SVV_LATCH_ALL - sum(1 for S in self.slots for t in S.t if t.st and t.kind == 2)]
        nh = 0
        for s in srcs:
            if nh >= SVV_H:
                break
            nh += 1
            S = next((x for x in self.slots if x.key == s.key), None)
            if S is not None and now - S.lastT > 1.0:
                self.slots.remove(S)
                S = None
            if S is None:
                S = Slot(s.key)
                self.slots.append(S)
            if not S.t0:
                hk = FxHash(((s.key * 0x9E3779B9) & M32) ^ 0x5EED)
                S.t0 = now
                for j in range(SVV_T):
                    S.t[j].wantT = now + 0.15 * j + 0.35 * SVV_R(FxHash((hk + j * 0x632BE5AB) & M32), 41)
            S.lastT = now
            if not self.paused:
                self.SlotStep(S, s, now, pp, castsLeft, latchFree)
            if build:
                if not V3273:  # v3.28 CORE: the heart first (its layers draw after the tendrils' anyway)
                    self.Core(S, s, now)
                S.gulpAcc = 0.0
                for j in range(SVV_T):
                    self.Build(S, s, j, now)
                S.gulp = S.gulpAcc

# ---------------------------------------------------------------- rasteriser (the game's blending)
def tri(img, p0, p1, p2, c0, c1, c2, mode):
    """mode 0: premultiplied (ONE / INVSRCALPHA), 1: the light (ONE / ONE with BLENDOP MAX), 2: the halo (ONE / ONE, ADD)."""
    xs, ys = (p0[0], p1[0], p2[0]), (p0[1], p1[1], p2[1])
    x0, x1 = max(int(math.floor(min(xs))), 0), min(int(math.ceil(max(xs))), img.shape[1] - 1)
    y0, y1 = max(int(math.floor(min(ys))), 0), min(int(math.ceil(max(ys))), img.shape[0] - 1)
    if x0 > x1 or y0 > y1:
        return
    den = (p1[1] - p2[1]) * (p0[0] - p2[0]) + (p2[0] - p1[0]) * (p0[1] - p2[1])
    if abs(den) < 1e-9:
        return
    X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    a = ((p1[1] - p2[1]) * (X - p2[0]) + (p2[0] - p1[0]) * (Y - p2[1])) / den
    b = ((p2[1] - p0[1]) * (X - p2[0]) + (p0[0] - p2[0]) * (Y - p2[1])) / den
    c = 1.0 - a - b
    m = (a >= -1e-6) & (b >= -1e-6) & (c >= -1e-6)
    if not m.any():
        return
    col = a[..., None] * np.array(c0) + b[..., None] * np.array(c1) + c[..., None] * np.array(c2)
    reg = img[y0:y1 + 1, x0:x1 + 1]
    if mode == 2:
        reg[m] = reg[m] + col[m][:, :3]
    elif mode:
        reg[m] = np.maximum(reg[m], col[m][:, :3])
    else:
        reg[m] = col[m][:, :3] + reg[m] * (1.0 - col[m][:, 3:4])

def draw_strips(img, strips):
    for layer in (2, 0, 1, 3, 4):  # v3.23.0: the halo first; v3.28 CORE: the heart's disc (3) and rim light (4) last
        for lay, rows in strips:
            if lay != layer:
                continue
            for r in range(len(rows) - 1):
                A, B = rows[r], rows[r + 1]
                if A is None or B is None:
                    continue
                for c in range(len(A) - 1):
                    q = (A[c], A[c + 1], B[c], B[c + 1])
                    if all(sum(v[2]) == 0.0 for v in q):
                        continue
                    if layer in (1, 2, 4) and all(sum(v[2][:3]) == 0.0 for v in q):
                        continue
                    md = (0, 1, 2, 0, 1)[layer]
                    tri(img, q[0], q[1], q[2], q[0][2], q[1][2], q[2][2], md)
                    tri(img, q[1], q[3], q[2], q[1][2], q[3][2], q[2][2], md)


# ---------------------------------------------------------------- backdrop: the Spirit Vision edge world (approximate)
LINE = np.array((0.30, 0.50, 0.62))
GLOW = np.array((0.95, 0.97, 1.00))  # v3.23.0 (item 3): white ghosts (was 0.35 / 1.00 / 0.75)

def seg3(cam, a, b):  # clip a world segment at the near plane, return screen points or None
    a, b = v3(a), v3(b)
    wa, wb = float((a - cam.eye) @ cam.f), float((b - cam.eye) @ cam.f)
    nw = GV_NEAR_W * 1.01
    if wa < nw and wb < nw:
        return None
    if wa < nw:
        a = a + (b - a) * (nw - wa) / (wb - wa)
    elif wb < nw:
        b = b + (a - b) * (nw - wb) / (wa - wb)
    pa, pb = cam.proj(a), cam.proj(b)
    if pa is None or pb is None:
        return None
    return (pa[0], pa[1]), (pb[0], pb[1])

def lines_layer(cam, segs, width):
    im = Image.new('L', (cam.w, cam.h), 0)
    d = ImageDraw.Draw(im)
    for a, b in segs:
        s = seg3(cam, a, b)
        if s:
            d.line([s[0], s[1]], fill=255, width=width)
    return im

def room_backdrop(cam, seed=7):
    """Black, with the room's edges and wall / floor detail as blue lines (the edge filter look)."""
    segs = []
    X0, X1, Y0, Y1, Z1, Z0 = -520.0, 620.0, -420.0, 0.0, 520.0, -1400.0
    for x in (X0, X1):
        segs += [((x, Y1, Z0), (x, Y1, Z1)), ((x, Y0, Z0), (x, Y0, Z1)), ((x, Y0, Z1), (x, Y1, Z1))]
    segs += [((X0, Y1, Z1), (X1, Y1, Z1)), ((X0, Y0, Z1), (X1, Y0, Z1))]
    for i in range(1, 9):  # floor boards (faint)
        x = X0 + (X1 - X0) * i / 9.0
        segs.append(((x, Y1, Z0), (x, Y1, Z1)))
    detail = []
    for i in range(140):  # scribbles: wall paper, cracks, stains
        h = FxHash(seed * 7919 + i)
        wall = h % 3
        u, v, l, ang = FxRnd(h ^ 1), FxRnd(h ^ 2), 15 + 60 * FxRnd(h ^ 3), 6.283 * FxRnd(h ^ 4)
        if wall == 0:
            p = (X0, Y0 + (Y1 - Y0) * v, Z0 * 0.6 + (Z1 - Z0 * 0.6) * u)
            q = (X0, p[1] + l * math.sin(ang), p[2] + l * math.cos(ang))
        elif wall == 1:
            p = (X0 + (X1 - X0) * u, Y0 + (Y1 - Y0) * v, Z1)
            q = (p[0] + l * math.cos(ang), p[1] + l * math.sin(ang), Z1)
        else:
            p = (X1, Y0 + (Y1 - Y0) * v, Z0 * 0.6 + (Z1 - Z0 * 0.6) * u)
            q = (X1, p[1] + l * math.sin(ang), p[2] + l * math.cos(ang))
        detail.append((p, q))
    # a doorway on the back wall and a cabinet on the right
    segs += [((-120, 0, Z1), (-120, -260, Z1)), ((-120, -260, Z1), (40, -260, Z1)), ((40, -260, Z1), (40, 0, Z1))]
    cab = [(X1, 0, 100), (X1 - 120, 0, 100), (X1 - 120, -190, 100), (X1, -190, 100), (X1 - 120, -190, 300), (X1 - 120, 0, 300)]
    segs += [(cab[0], cab[1]), (cab[1], cab[2]), (cab[2], cab[3]), (cab[2], cab[4]), (cab[4], cab[5]), (cab[5], cab[1])]
    a = np.asarray(lines_layer(cam, segs, 2 * SS), dtype=np.float64) / 255.0
    b = np.asarray(lines_layer(cam, detail, SS), dtype=np.float64) / 255.0 * 0.55
    m = np.maximum(a, b)
    halo = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3 * SS)), dtype=np.float64) / 255.0
    v = np.clip(m * 0.9 + halo * 0.6, 0, 1)
    return v[..., None] * LINE[None, None, :]

def ghost_glow(img, cam, c, yaw=0.3, lying=True, blood=False):
    """A floating victim's model glow: a green-white body outline (approximate)."""
    cx, cy, cz = c
    ca, sa = math.cos(yaw), math.sin(yaw)
    def P(u, h, s):  # u along the body, h up, s sideways
        if lying:
            return (cx + u * ca - s * sa, cy - h, cz + u * sa + s * ca)
        return (cx + s * ca, cy - u, cz + s * sa)
    body = [((-80, 6, 0), (40, 8, 0)), ((40, 8, 0), (70, 8, 0)), ((-80, 6, -10), (-170, 2, -12)), ((-80, 6, 10), (-170, 2, 12)),
            ((30, 8, -18), (-30, 4, -32)), ((30, 8, 18), (-30, 4, 32))]
    im = Image.new('L', (cam.w, cam.h), 0)
    d = ImageDraw.Draw(im)
    for a, b in body:
        s = seg3(cam, P(*a), P(*b))
        if s:
            d.line([s[0], s[1]], fill=255, width=int(14 * SS * 400.0 / max(200.0, float((v3(c) - cam.eye) @ cam.f))))
    hp = cam.proj(P(84, 8, 0))
    if hp:
        r = 13 * cam.px / hp[2]
        d.ellipse([hp[0] - r, hp[1] - r, hp[0] + r, hp[1] + r], fill=255)
    a = np.asarray(im, dtype=np.float64) / 255.0
    halo = np.asarray(im.filter(ImageFilter.GaussianBlur(6 * SS)), dtype=np.float64) / 255.0
    if blood:  # v3.26.0 PLAN 2.1 (kPsGvBlood): ramp = deep * pulse + L (pale - deep), L = luma(texel) * GAIN * pulse
        al = np.clip(a * 0.85 + halo * 0.9, 0, 1)
        core = np.asarray(im.filter(ImageFilter.GaussianBlur(2.5 * SS)), dtype=np.float64) / 255.0  # the model's own texel
        L = cloth_tex(img.shape[1], img.shape[0]) * np.clip(core * 1.15, 0, 1) * GV_BLOOD_GAIN * BLOOD_PULSE
        ramp = BL_DEEP[None, None, :] * BLOOD_PULSE + L[..., None] * (BL_PALE - BL_DEEP)[None, None, :]
        img += al[..., None] * ramp
        return
    img += (np.clip(a * 0.85 + halo * 0.9, 0, 1))[..., None] * GLOW[None, None, :]

def ghost_bones(c, yaw=0.3, lying=True):
    """v3.23.0: a ghost skeleton in the same frame as ghost_glow (pelvis, spine, chest, head, shoulders, hands, knees,
    feet), for the bone roots (item 15); as the game's SvTrackBones does, the source centre becomes their mean."""
    cx, cy, cz = c
    ca, sa = math.cos(yaw), math.sin(yaw)
    def P(u, h, s_):
        if lying:
            return v3((cx + u * ca - s_ * sa, cy - h, cz + u * sa + s_ * ca))
        return v3((cx + s_ * ca, cy - u, cz + s_ * sa))
    pts = [(-80, 6, 0), (-25, 7, 0), (30, 8, 0), (70, 8, 0), (84, 8, 0), (30, 8, -18), (30, 8, 18), (-30, 4, -32), (-30, 4, 32),
           (-125, 4, -11), (-125, 4, 11), (-170, 2, -12), (-170, 2, 12)]
    return [P(*q) for q in pts]

def with_bones(src, c, yaw=0.3, lying=True):
    src.bones = ghost_bones(c, yaw, lying)
    src.c = sum(src.bones) / len(src.bones)
    return src

def stick_figure(img, cam, B):
    """The player drawn as Spirit Vision draws her: a blue edge outline (approximate)."""
    segs = [(3, 2), (2, 1), (6, 5), (5, 1), (3, 4), (6, 7), (1, 9), (9, 10), (10, 11), (9, 13), (13, 14), (14, 16), (9, 18), (18, 19), (19, 21)]
    im = Image.new('L', (cam.w, cam.h), 0)
    d = ImageDraw.Draw(im)
    for a, b in segs:
        s = seg3(cam, B[a], B[b])
        if s:
            dw = float((B[a] - cam.eye) @ cam.f)
            d.line([s[0], s[1]], fill=255, width=max(2, int(9 * cam.px / dw)))
    hp = cam.proj(B[11] - v3((0, 6, 0)))
    if hp:
        r = 11 * cam.px / hp[2]
        d.ellipse([hp[0] - r, hp[1] - r, hp[0] + r, hp[1] + r], outline=255, width=max(2, int(3 * cam.px / hp[2])))
    edge = np.asarray(im.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.MaxFilter(3)), dtype=np.float64) / 255.0
    img[:] = np.maximum(img, np.clip(edge * 1.2, 0, 1)[..., None] * LINE[None, None, :] * 1.4)

def player_bones(feet, yaw, t=0.0):
    """A henry01-like skeleton (bones 0-24 subset), standing, slight breathing; y down."""
    fx, fy, fz = feet
    fw = v3((math.sin(yaw), 0, math.cos(yaw)))
    rt = v3((math.cos(yaw), 0, -math.sin(yaw)))
    base = v3(feet)
    def P(h, side, fwd):
        return base + v3((0, -h, 0)) + rt * side + fw * fwd
    br = 1.5 * math.sin(2.0 * t)
    B = {0: P(0, 0, 0), 1: P(95, 0, 0), 2: P(92, 10, 0), 3: P(50, 11, 3), 4: P(6, 11, 0), 5: P(92, -10, 0), 6: P(50, -11, 3),
         7: P(6, -11, 0), 8: P(115, 0, 0), 9: P(135 + br, 0, 0), 10: P(152 + br, 0, 0), 11: P(162 + br, 0, 1),
         12: P(145, 8, 0), 13: P(145, 19, -1), 14: P(117, 23, 2), 16: P(92, 24, 8), 17: P(145, -8, 0), 18: P(145, -19, -1),
         19: P(117, -23, 2), 21: P(92, -24, 8)}
    B[15], B[20] = B[14], B[19]
    return B

def core_sprites(img, cam, s, now):
    """The haunting's core (approximate: soft blobs instead of the atlas): dark mist puffs (premultiplied), a dim base glow,
    rising ash / embers. Same hashes and timing as SvHauntSprites / the particle pool, simplified shapes."""
    key = FxHash((s.key * 0x9E3779B9 + 0x1234) & M32)
    Hh = s.y1 - s.y0
    hgt, wid = img.shape[:2]
    Y, X = np.mgrid[0:hgt, 0:wid]
    def blob(p, wr, rgb, a, add):
        pr = cam.proj(p)
        if pr is None:
            return
        r = wr * cam.px / pr[2]
        x0, x1, y0, y1 = int(max(pr[0] - 2 * r, 0)), int(min(pr[0] + 2 * r, wid - 1)), int(max(pr[1] - 2 * r, 0)), int(min(pr[1] + 2 * r, hgt - 1))
        if x0 >= x1 or y0 >= y1:
            return
        xx, yy = X[y0:y1, x0:x1], Y[y0:y1, x0:x1]
        f = np.exp(-((xx - pr[0]) ** 2 + (yy - pr[1]) ** 2) / (0.5 * r * r)) * a
        reg = img[y0:y1, x0:x1]
        if add:
            reg += f[..., None] * np.array(rgb)
        else:
            reg[:] = reg * (1 - f[..., None]) + f[..., None] * np.array(rgb)
    blob(s.c, 0.8 * s.Pm * 0.5, (0.40, 0.02, 0.05), 0.30 * 0.30 * s.k, True)
    nm = 4 + key % 3
    for i in range(nm):
        hi = FxHash((key + i * 0x9E37) & M32)
        ang = 6.2831853 * (i / nm + 0.15 * FxRnd(hi)) + 0.05 * now
        rad = s.Rs * (0.15 + 0.5 * FxRnd(hi ^ 0x11))
        p = (s.c[0] + rad * math.cos(ang), s.c[1] + 0.5 * Hh * (FxRnd(hi ^ 0x44) - 0.5), s.c[2] + rad * math.sin(ang))
        blob(p, s.Pm * (0.6 + 0.3 * FxRnd(hi ^ 0x55)) * 0.35, (0.16 if i & 1 else 0.02, 0.0, 0.02 if i & 1 else 0.0), 0.8 * 0.5 * s.k * 0.6, False)
    for i in range(16):  # ash / embers
        h = FxHash((key ^ (i * 0x85EBCA6B)) & M32)
        life = 2.5 + 1.5 * FxRnd(h ^ 6)
        age = (now + life * FxRnd(h ^ 9)) % life
        a, r = 6.2831853 * FxRnd(h ^ 3), 0.7 * s.Rs * math.sqrt(FxRnd(h ^ 4))
        ember = FxRnd(h ^ 7) < 0.2
        p = (s.c[0] + r * math.cos(a) + 10 * math.sin(3 * age), s.c[1] + (s.y1 - s.c[1]) - Hh * (0.05 + 0.6 * FxRnd(h ^ 5)) - (60 if ember else 35) * age,
             s.c[2] + r * math.sin(a))
        fade = FxSmooth(0, 0.4, age) * (1 - FxSmooth(life - 0.8, life, age))
        if ember:
            blob(p, 4.0, (1.0, 0.38, 0.08), 0.9 * fade * s.k, True)
        else:
            blob(p, 7.0, (0.05, 0.035, 0.03), 0.85 * fade * s.k, False)


# ---------------------------------------------------------------- v3.26.0: the blood-rooted ghost
GV_BLOOD_GAIN, BLOOD_PULSE = 1.0, 0.93
BL_PALE, BL_DEEP = np.array((1.00, 0.86, 0.84)), np.array((0.42, 0.02, 0.06))
SVB_AURA_K, SVB_AURA_R0, SVB_AURA_R1, SVB_ENDS, SVB_END_R, SVB_END_K = 1.60, 14.0, 38.0, 6, 1.25, 1.70  # = svveins.inc v3.26.0 (tuned)
SVB_GROWN, SVB_DRIP_P, SVB_FALL_G, SVB_FALL_MAX, SVB_FLOOR_S = 0.30, 0.80, 980.0, 600.0, 4.0
kSvbBlood = (0.19, 0.006, 0.024)
kSvbFloor = (0.22, 0.010, 0.030)
_TEX = {}

def _vnoise(w, h, cx, cy, seed):
    rng = np.random.RandomState(seed)
    g = rng.rand(h // cy + 4, w // cx + 4).astype(np.float32)
    im = Image.fromarray(g, 'F').resize(((w // cx + 4) * cx, (h // cy + 4) * cy), Image.BICUBIC)
    return np.clip(np.asarray(im)[:h, :w], 0, 1).astype(np.float64)

def cloth_tex(w, h):
    """The stand-in texel luminance 0.25 .. 1.0: cloth folds (stretched ridged value noise) + mottling."""
    if (w, h) not in _TEX:
        n1 = _vnoise(w, h, 90 * SS, 26 * SS, 3)
        n2 = _vnoise(w, h, 46 * SS, 16 * SS, 5)
        n3 = _vnoise(w, h, 14 * SS, 14 * SS, 9)
        v = 0.40 * n1 + 0.35 * (1.0 - np.abs(2.0 * n2 - 1.0)) + 0.25 * n3
        lo, hi = np.percentile(v, 2), np.percentile(v, 98)
        _TEX[(w, h)] = 0.25 + 0.75 * np.clip((v - lo) / (hi - lo), 0, 1)
    return _TEX[(w, h)]

def sprite(img, cam, p, r, rgb, a, add, minpx, unit, rot=0.0, blob=False, stretch=1.0):
    """SvSprAdd: one soft (radial falloff) or blob (lumpy disc) sprite, additive or premultiplied."""
    pr = cam.proj(p)
    if pr is None or a <= 0.002:
        return
    rpx = max(r * cam.px / pr[2], minpx * unit)
    hh, ww = img.shape[:2]
    ry = rpx * stretch
    x0, x1, y0, y1 = int(max(pr[0] - rpx - 1, 0)), int(min(pr[0] + rpx + 2, ww)), int(max(pr[1] - ry - 1, 0)), int(min(pr[1] + ry + 2, hh))
    if x0 >= x1 or y0 >= y1:
        return
    Y, X = np.mgrid[y0:y1, x0:x1]
    dx, dy = (X + 0.5 - pr[0]) / rpx, (Y + 0.5 - pr[1]) / ry
    d = np.sqrt(dx * dx + dy * dy)
    if blob:
        th = np.arctan2(dy, dx)
        d = d / (1.0 + 0.12 * np.sin(3.0 * th + rot) + 0.07 * np.sin(5.0 * th + 2.0 * rot))
        t = np.clip((d - 0.55) / 0.45, 0, 1)
        f = 1.0 - t * t * (3.0 - 2.0 * t)
    else:
        t = np.clip(d, 0, 1)
        f = (1.0 - t * t * (3.0 - 2.0 * t)) ** 1.3
    reg = img[y0:y1, x0:x1]
    rgb = np.array(rgb, dtype=np.float64)
    if add:
        reg += (f * a)[..., None] * rgb
    else:
        reg[:] = reg * (1.0 - np.clip(f * a, 0, 1))[..., None] + (f * a)[..., None] * rgb

def svb_aura(img, cam, s, now, unit):
    """SvbAura: one additive soft sprite per ghost bone."""
    B = s.bones
    if B is None or len(B) < 3 or not (s.kv > 0.01):
        return
    n = len(B)
    cen = sum(B) / n
    dc = [float((b - cen) @ (b - cen)) for b in B]
    for b in range(n):
        rank = sum(1 for c in range(n) if dc[c] > dc[b])
        end = rank < SVB_ENDS
        nn = min(float((B[c] - B[b]) @ (B[c] - B[b])) for c in range(n) if c != b)
        r = min(max(0.75 * math.sqrt(nn), SVB_AURA_R0), SVB_AURA_R1) * (SVB_END_R if end else 1.0)
        hb = FxHash((s.key * 0x9E3779B9 + b * 0x632BE5AB) & M32)
        a = SVB_AURA_K * s.kv * (SVB_END_K if end else 1.0) * (0.85 + 0.15 * math.sin((0.6 + 0.5 * FxRnd(hb)) * now + 6.2831853 * FxRnd(hb ^ 1)))
        sprite(img, cam, B[b], r, kSvvPal[0]['halo'], a, True, 2.0, unit)

def _drip_slot(rt, T, sl, per, off):
    hr = rt['seed'] ^ 0xB100D
    hs = FxHash(hr ^ ((int(sl) * 0x2C1B3C6D) & M32))
    dr, L = 0.6 + 0.5 * FxRnd(hs ^ 1), 20.0 + 40.0 * FxRnd(hs ^ 2)
    return hs, dr, L, FxRnd(hs) < SVB_DRIP_P

def _floor_of(room, head):
    got = room.cast(head, head + v3((0, SVB_FALL_MAX, 0)))
    if got is not None and got[1][1] < -0.5:
        return float(got[0][1]), got[1]
    return None, None

def svb_root_blood(img, cam, rt, T, room, unit, veins):
    """SvbRootBlood: the seep, the drip (chain / free fall); returns the floor blots (landed within SVB_FLOOR_S)."""
    pal = kSvvPal[rt['pal']]
    k = rt['k'] * FxSat((rt['g'] - SVB_GROWN) / 0.2)
    th = rt['pulse']
    hr = rt['seed'] ^ 0xB100D
    blots = []
    if not (k > 0.01):
        return blots
    r0 = rt['r0']
    bl = pal['body'] if rt['pal'] else kSvbBlood
    core = [pal['rim'][j] * 0.45 * (1.0 + 0.6 * th) for j in range(3)]
    soft = [pal['rim'][j] * 0.6 for j in range(3)]
    if not rt.get('core'):  # v3.28 CORE: the heart is the wound: no seep, only its drips
        sprite(img, cam, r0, 1.6 * SVV_W0, soft, k, True, 0.0, unit)
        sprite(img, cam, r0, 1.1 * SVV_W0 * (1.0 + 0.12 * th), bl, 0.80 * k, False, 1.5, unit, rot=6.2831853 * SVV_R(hr, 1), blob=True)
        sprite(img, cam, r0, 0.66 * SVV_W0, core, k, True, 1.0, unit, rot=6.2831853 * SVV_R(hr, 2), blob=True)
        gp = r0.copy()
        gp[1] -= 0.35 * SVV_W0
        sprite(img, cam, gp, 3.0, pal['wet'], 0.8 * k, True, 1.0, unit)
    per = 1.5 + 2.5 * SVV_R(hr, 3)
    off = per * SVV_R(hr, 4)
    tt = T + off
    sl0 = math.floor(tt / per)
    for sl in range(int(sl0) - 3, int(sl0) + 1):  # earlier slots: only their floor blots can still be alive
        hs, dr, L, has = _drip_slot(rt, T, sl, per, off)
        if not has:
            continue
        tdet = sl * per - off + dr
        head = r0 + v3((0, L, 0))
        fy, nrm = _floor_of(room, head)
        if fy is None:
            continue
        tl = tdet + math.sqrt(2.0 * (fy - head[1]) / SVB_FALL_G)
        age = T - tl
        if 0.0 <= age < SVB_FLOOR_S:
            blots.append((hs | 1, v3((head[0], fy, head[2])), nrm, age))
    tau = tt - sl0 * per
    hs, dr, L, has = _drip_slot(rt, T, sl0, per, off)
    if not has:
        return blots
    u = tau / dr
    if u < 1.0:
        e = u * u
        for q in range(5):
            f = q / 4.0
            sz = (9.0 - 5.0 * f) * (0.8 + 0.4 * FxRnd(hs ^ (10 + q)))
            c = r0.copy()
            c[1] += L * e * f
            c[0] += 2.5 * (FxRnd(hs ^ (20 + q)) - 0.5) * 2.0
            c[2] += 2.5 * (FxRnd(hs ^ (30 + q)) - 0.5) * 2.0
            sprite(img, cam, c, sz, bl, 0.75 * k * (0.4 + 0.6 * f), False, 1.0, unit, rot=6.2831853 * FxRnd(hs ^ (40 + q)), blob=True)
            sprite(img, cam, c, 0.5 * sz, [pal['rim'][j] * 0.35 for j in range(3)], k, True, 0.0, unit, blob=True)
        head = r0.copy()
        head[1] += L * e
        sprite(img, cam, head, 2.0, pal['wet'], 0.9 * k, True, 1.0, unit)
        return blots
    head = r0 + v3((0, L, 0))
    tf = T - (T - (tau - dr))  # = tau - dr
    tf = tau - dr
    fy, nrm = _floor_of(room, head)
    y = head[1] + 0.5 * SVB_FALL_G * tf * tf
    if fy is not None and y >= fy:
        return blots  # landed: its blot is in the list above
    if fy is None and tf > 0.7:
        return blots
    if tf > 1.2:
        return blots
    p = head.copy()
    p[1] = y
    sprite(img, cam, p, 2.6, bl, 0.8 * k * (1.0 - tf / 0.7 if fy is None else 1.0), False, 1.0, unit, blob=True, stretch=1.5)
    return blots

def svb_floor_strips(veins, blots, T):
    """SvBloodFrame's floor blots (SvvBlot: the flattened splash + the crimson ring); returns the strips."""
    save, veins.strips = veins.strips, []
    kR, kRH = (0.0, 0.75, 1.0), (0.0, 0.6, 1.0)
    for key, p, n, age in blots:
        f = 1.0 - age / SVB_FLOOR_S
        t1, t2 = SvvPerp(n)
        rc = [(*kSvbFloor, 0.8 * f), (*kSvbFloor, 0.7 * f), (*kSvbFloor, 0.0)]
        veins.Blot(p, t1, t2, (9.0 + 5.0 * FxRnd(key)) * (0.5 + 0.5 * FxSat(age / 0.15)), 10, key, 0.25, 0.3, 0, kR, rc, 1.0)
        hc = [kSvvPal[0]['halo'][c] * (2.0 + 2.0 * (1.0 - FxSat(age / 0.4))) for c in range(3)]
        rc = [(*hc, 0.6 * f), (*hc, 0.4 * f), (*hc, 0.0)]
        veins.Blot(p, t1, t2, 13.0, 10, key ^ 7, 0.2, 0.2, 2, kRH, rc, 1.5)
    out, veins.strips = veins.strips, save
    return out

def svb_frame(img, cam, veins, srcs, now, room):
    """SvBloodFrame on the preview clock (T = now): the aura, the roots' blood; returns the floor-blot strips."""
    blots = {}
    for s in srcs:
        svb_aura(img, cam, s, now, veins.unit)
    for rt in veins.roots:
        for b in svb_root_blood(img, cam, rt, now, room, veins.unit, veins):
            blots[b[0]] = b
    return svb_floor_strips(veins, list(blots.values()), now), len(blots)

# ---------------------------------------------------------------- scenes
def render(veins, cam, srcs, now, pp=None, bones=None, backdrop=None, ghosts=(), blood=None, info=None):
    """blood: None = the v3.23.0 white ghost; a Room = the v3.26.0 blood-rooted ghost (ramp body, aura, root blood)."""
    img = (backdrop.copy() if backdrop is not None else room_backdrop(cam))
    for g in ghosts:
        if g and g[0] == 'yaw':  # ('yaw', centre, yaw): a ghost at another heading (v3.23.0 downed scene)
            ghost_glow(img, cam, g[1], g[2], blood=blood is not None)
        else:
            ghost_glow(img, cam, g, blood=blood is not None)
    if bones is not None:
        stick_figure(img, cam, bones)
    for s in srcs:
        core_sprites(img, cam, s, now)
    strips = veins.strips
    if blood is not None:
        extra, nb = svb_frame(img, cam, veins, srcs, now, blood)
        strips = strips + extra
        if info is not None:
            info['floor_blots'] = nb
    draw_strips(img, strips)
    out = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
    return out.resize((W, H), Image.LANCZOS)

def label(im, text):
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 10 + 7 * len(text), 20], fill=(0, 0, 0))
    d.text((5, 4), text, fill=(200, 200, 200))
    return im

def run(veins, cam, srcs, t_end, pp_fn=None, bones_fn=None, src_fn=None, frames=(), fps=60.0, shoot=None, variants=None, **kw):
    """Steps SvVeinsFrame at fps on the real-time clock; at each time in frames renders through shoot(name, image)."""
    veins.cam = cam
    veins.unit = cam.h / 720.0
    want = sorted(frames)
    n = int(t_end * fps) + 1
    out = {}
    for i in range(n):
        now = i / fps
        if src_fn:
            src_fn(srcs, now)
        pp = pp_fn(now) if pp_fn else None
        veins.B = bones_fn(now) if bones_fn else {}
        build = bool(want) and now + 0.5 / fps >= want[0]
        veins.Frame(now, srcs, pp, build=build)
        if build:
            if variants:  # several renders of the same frame (name -> kw overrides)
                out[want[0]] = {n: render(veins, cam, srcs, now, pp=pp, bones=veins.B if bones_fn else None, **{**kw, **v}) for n, v in variants.items()}
            else:
                out[want[0]] = render(veins, cam, srcs, now, pp=pp, bones=veins.B if bones_fn else None, **kw)
            print('  t = %.2f s: %d strips' % (now, len(veins.strips)))
            want.pop(0)
            if not want:
                break
    return out

def main():
    os.makedirs(OUT, exist_ok=True)
    saved = []
    def save(im, name, text):
        p = os.path.join(OUT, name)
        label(im, text).save(p)
        saved.append(p)
        print('wrote', p)

    # 1. one haunting (a floating victim) in a room, four moments
    print('haunting over time')
    room = Room()
    ghost = (40.0, -150.0, 180.0)
    src = with_bones(Src(0x2A, ghost, R=450.0, H=400.0), ghost)  # v3.23.0: roots on its bones
    cam = Cam((330.0, -250.0, -560.0), (60.0, -170.0, 180.0), fov_v=55.0)
    times = (0.75, 1.6, 4.3, 7.9)
    shots = run(Veins(room), cam, [src], max(times) + 0.1, frames=times, ghosts=[ghost], backdrop=room_backdrop(cam))
    for i, t in enumerate(times):
        save(shots[t], 'veins_t%d.png' % i, 'veins t = %.2f s' % t)

    # 2. gameplay distance (the camera behind the player, the ghost 1100 away)
    print('gameplay distance')
    camf = Cam((-60.0, -230.0, -1150.0), (40.0, -150.0, 180.0), fov_v=50.0)
    shots = run(Veins(room), camf, [src], 5.1, frames=(5.0,), ghosts=[ghost], backdrop=room_backdrop(camf),
                pp_fn=lambda now: v3((-40.0, 0.0, -820.0)), bones_fn=lambda now: player_bones((-40.0, 0.0, -820.0), 3.14, now))
    save(shots[5.0], 'veins_far.png', 'gameplay distance (ghost ~1150 from the camera), t = 5 s')

    # 3. over screenshot 02 (its red smudges removed), the haunting at the lying victim, a wall / floor guessed
    if os.path.exists(SHOT):
        print('over screenshot 02')
        bg = Image.open(SHOT).convert('RGB')
        bw, bh = bg.size
        bg = bg.crop((1, bh - 721, 1281, bh - 1)) if bh >= 752 else bg
        a = np.asarray(bg, dtype=np.float64) / 255.0
        red = (a[..., 0] > a[..., 1] + 0.06) & (a[..., 0] > a[..., 2] + 0.06)
        a = a.copy()
        a[red] = 0.0
        a = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).resize((W * SS, H * SS), Image.BILINEAR), dtype=np.float64) / 255.0
        cam2 = Cam((0.0, -260.0, -900.0), (0.0, -150.0, 0.0), fov_v=50.0)
        # place the ghost where the lying victim glows (screen 275, 300): unproject at depth 1500
        def unproj(x, y, d):
            q = cam2.f * d + cam2.r * ((x * SS - 0.5 * cam2.w) * d / cam2.px) + cam2.d * ((y * SS - 0.5 * cam2.h) * d / cam2.px)
            return cam2.eye + q
        g2 = unproj(275.0, 290.0, 1500.0)
        room2 = Room()
        room2.planes = [(v3((0, 1, 0)), float(g2[1]) + 130.0), (v3((-1, 0, 0)), -float(g2[0]) + 420.0), (v3((0, 0, 1)), float(g2[2]) + 380.0)]
        src2 = Src(0x77, tuple(g2), R=400.0)
        shots = run(Veins(room2), cam2, [src2], 4.6, frames=(4.5,), backdrop=a)
        save(shots[4.5], 'veins_shot02.png', 'over screenshot 02 (old smudges removed; anchors on a guessed floor / wall)')

    # 4. latching: she walks up to the ghost; then the medallion
    print('latch sequence')
    ghost3 = (60.0, -150.0, 260.0)
    src3 = with_bones(Src(0x3C, ghost3, R=450.0, H=400.0), ghost3)
    cam3 = Cam((-470.0, -200.0, -300.0), (-10.0, -125.0, 80.0), fov_v=50.0)
    def feet(now):  # walks in over 2..4.5 s, stands; the chest ends ~330 from the ghost
        u = SvEase((now - 2.0) / 2.5)
        return v3((-260.0 + 160.0 * u, 0.0, -460.0 + 420.0 * u))
    def src_fn(srcs, now):  # the medallion repels the ghost from 9.0 s to 10.0 s
        srcs[0].pal = SVP_GOLD if 9.0 <= now < 10.0 else SVP_CRIMSON
        srcs[0].k = SV_REPEL_K if srcs[0].pal == SVP_GOLD else 1.0
    lt = (5.15, 5.75, 6.6, 8.4, 9.08, 9.25)
    shots = run(Veins(room), cam3, [src3], 9.4, pp_fn=feet, bones_fn=lambda now: player_bones(feet(now), 0.6, now),
                src_fn=src_fn, frames=lt, ghosts=[ghost3], backdrop=room_backdrop(cam3))
    names = ('latch_0.png', 'latch_1.png', 'latch_2.png', 'latch_3.png', 'medallion_0.png', 'medallion_1.png')
    texts = ('latch: the first tendril tears off the wall and shoots at her', 'latch: wrapping her limbs', 'latch: 3-4 latched, feeding',
             'latch: symbiote, draining her (pulses run to the ghost)', 'medallion: they let go at once, gold sparks',
             'medallion: recoiling into the ghost')
    for t, n_, tx in zip(lt, names, texts):
        save(shots[t], n_, '%s, t = %.2f s' % (tx, t))
    cam4 = Cam((-360.0, -175.0, -260.0), (-90.0, -115.0, -30.0), fov_v=45.0)  # close: the wrap round her limbs
    shots = run(Veins(room), cam4, [src3], 8.5, pp_fn=feet, bones_fn=lambda now: player_bones(feet(now), 0.6, now),
                src_fn=src_fn, frames=(8.4,), ghosts=[ghost3], backdrop=room_backdrop(cam4))
    save(shots[8.4], 'latch_close.png', 'latched, close: coils round her arm / thigh / chest, infection, t = 8.40 s')

    # 5. v3.23.0 (items 15 / 17): a ghost knocked down onto the floor: the veins come out of its lying body, retract when
    # it falls (3.0 s), are gone while it is down, and regrow over the last 2.5 s of its 6 s countdown (rises at 9.0 s)
    print('downed: retract / regrow')
    gd = (-20.0, -12.0, 200.0)
    srcd = with_bones(Src(0x51, gd, R=450.0, H=400.0), gd, yaw=0.9, lying=True)
    camd = Cam((300.0, -330.0, -420.0), (0.0, -40.0, 200.0), fov_v=55.0)
    st = {'reach': 1.0, 'kv': 1.0, 'last': 0.0}
    def src_down(srcs, now):  # SvTrackStep's reach / kv rules (class 9, countdown running from 6 s at 3.0 s)
        dt = now - st['last']
        st['last'] = now
        down = 3.0 <= now < 9.0
        cd = 9.0 - now
        tgt = FxSat(1.0 - cd / 2.5) if down else 1.0
        r = st['reach']
        r = max(tgt, r - dt / 0.6) if r > tgt else min(tgt, r + dt / 0.4)
        st['reach'] = r
        kt = r if down else 1.0
        st['kv'] = kt if kt >= st['kv'] else max(kt, st['kv'] - dt / 0.6)
        srcs[0].reach, srcs[0].reachT, srcs[0].kv = r, tgt, st['kv']
        srcs[0].k = 0.25 * FxSat(1.0 - cd / 4.0) if down else 1.0
    dts = (2.9, 3.3, 5.0, 7.7, 8.6, 9.6)
    shots = run(Veins(room), camd, [srcd], 9.7, src_fn=src_down, frames=dts, ghosts=[('yaw', gd, 0.9)], backdrop=room_backdrop(camd))
    dnames = ('down_0_up.png', 'down_1_retract.png', 'down_2_gone.png', 'down_3_regrow.png', 'down_4_regrow.png', 'down_5_risen.png')
    dtexts = ('downed: veins from the lying body (before the fall counts)', 'downed: retracting into the body',
              'downed: gone while down', 'downed: regrowing (last 2.5 s of the countdown)', 'downed: almost grown back',
              'risen: full veins')
    for t, n_, tx in zip(dts, dnames, dtexts):
        im = shots[t]
        save(im, n_, '%s, t = %.2f s' % (tx, t))

    # contact sheet
    ims = [Image.open(p) for p in saved]
    cols = 3
    tw, th = W // 2, H // 2
    sheet = Image.new('RGB', (cols * tw, ((len(ims) + cols - 1) // cols) * th))
    for i, im in enumerate(ims):
        sheet.paste(im.resize((tw, th), Image.LANCZOS), ((i % cols) * tw, (i // cols) * th))
    p = os.path.join(OUT, 'veins_sheet.png')
    sheet.save(p)
    print('wrote', p)

def main_blood(saved_dir):
    """v3.26.0 outputs: the blood-rooted ghost."""
    print('blood-rooted ghost')
    room = Room()
    sv = []
    def save(im, name, text):
        p = os.path.join(saved_dir, name)
        label(im, text).save(p)
        sv.append(p)
        print('wrote', p)
    ghost = (40.0, -150.0, 180.0)
    src = with_bones(Src(0x2A, ghost, R=450.0, H=400.0), ghost)
    cam = Cam((300.0, -235.0, -400.0), (50.0, -120.0, 180.0), fov_v=55.0)
    times = (3.0, 4.5, 6.0, 7.5)
    info = {}
    shots = run(Veins(room), cam, [src], max(times) + 0.1, frames=times, ghosts=[ghost], backdrop=room_backdrop(cam), blood=room, info=info)
    for i, t in enumerate(times):
        save(shots[t], 'blood_t%d.png' % i, 'blood-rooted ghost, t = %.2f s' % t)
    # white vs blood, same frame (t = 6.0)
    shots = run(Veins(room), cam, [src], 6.1, frames=(6.0,), ghosts=[ghost], backdrop=room_backdrop(cam),
                variants={'white': dict(blood=None), 'blood': dict(blood=room)})
    both = Image.new('RGB', (2 * W, H))
    both.paste(shots[6.0]['white'], (0, 0))
    both.paste(shots[6.0]['blood'], (W, 0))
    save(both, 'blood_white_vs_blood.png', 'left: v3.23.0 white ghost     right: v3.26.0 blood-rooted     (same frame, t = 6.00 s)')
    # a close crop of the blood frame (x2) to judge the roots / drips
    cc = Cam((150.0, -190.0, -60.0), (40.0, -140.0, 180.0), fov_v=50.0)
    shots = run(Veins(room), cc, [src], 6.1, frames=(6.0,), ghosts=[ghost], backdrop=room_backdrop(cc), blood=room)
    save(shots[6.0], 'blood_close.png', 'close: seep / drips at the roots, t = 6.00 s')
    # downed: the veins regrow, everything scaled by kv
    gd = (-20.0, -12.0, 200.0)
    srcd = with_bones(Src(0x51, gd, R=450.0, H=400.0), gd, yaw=0.9, lying=True)
    camd = Cam((300.0, -330.0, -420.0), (0.0, -40.0, 200.0), fov_v=55.0)
    st = {'reach': 1.0, 'kv': 1.0, 'last': 0.0}
    def src_down(srcs, now):
        dt = now - st['last']
        st['last'] = now
        down = 3.0 <= now < 9.0
        cd = 9.0 - now
        tgt = FxSat(1.0 - cd / 2.5) if down else 1.0
        r = st['reach']
        r = max(tgt, r - dt / 0.6) if r > tgt else min(tgt, r + dt / 0.4)
        st['reach'] = r
        kt = r if down else 1.0
        st['kv'] = kt if kt >= st['kv'] else max(kt, st['kv'] - dt / 0.6)
        srcs[0].reach, srcs[0].reachT, srcs[0].kv = r, tgt, st['kv']
        srcs[0].k = 0.25 * FxSat(1.0 - cd / 4.0) if down else 1.0
    shots = run(Veins(room), camd, [srcd], 8.7, src_fn=src_down, frames=(8.6,), ghosts=[('yaw', gd, 0.9)], backdrop=room_backdrop(camd), blood=room)
    save(shots[8.6], 'blood_down_regrow.png', 'downed, veins regrowing (kv = %.2f): aura + root blood scaled by kv, t = 8.60 s' % st['kv'])
    names = ['blood_t0.png', 'blood_t1.png', 'blood_t2.png', 'blood_t3.png', 'blood_close.png', 'blood_down_regrow.png', 'blood_white_vs_blood.png']
    ims = [Image.open(os.path.join(saved_dir, n)).convert('RGB') for n in names]
    tw, th = W // 2, H // 2
    sheet = Image.new('RGB', (3 * tw, 2 * th + int(3 * tw * H / (2 * W))))
    for i, im in enumerate(ims[:6]):
        sheet.paste(im.resize((tw, th), Image.LANCZOS), ((i % 3) * tw, (i // 3) * th))
    sheet.paste(ims[6].resize((3 * tw, int(3 * tw * H / (2 * W))), Image.LANCZOS), (0, 2 * th))
    p = os.path.join(saved_dir, 'blood_sheet.png')
    sheet.save(p)
    print('wrote', p, info)

if __name__ == '__main__':
    if 'blood' not in sys.argv[2:]:  # 'blood' as a second argument: only the v3.26.0 outputs
        main()
    main_blood(OUT)
