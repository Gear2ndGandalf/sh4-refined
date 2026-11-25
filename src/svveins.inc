// ================================================================ v3.22.0 haunting aura: veins (user item 9)
// staging/feedback_v322/PLAN.md section 8. Included by screenfx.inc in place of the v3.21 SvHauntSprites (after the
// sprite list / particle helpers, before SvHolySprites / SvAuraFrame). Interface used by screenfx.inc:
//   SvHauntSprites(src, now)  per haunting source, once a frame from SvAuraFrame (adds camera-facing sprites)
//   SvVeinsFrame(now, dt)     once a frame from SvAuraFrame, after the sources / particles / sprite scale
//   SvVeinsDraw(dev)          in each depth pass, right after SvAuraDraw (leaves SvUntextured's states)
//   SvVeinsPaused()           1 while the aura block runs from a paused frame (no ray casts, no new anchors)
// The offline preview tools/aura_preview.py is a port of this file (same constants, hashes and maths): change both.
//
// ---- a haunting's core (v3.22.0): a dim base glow and the turning dark mist (the old red veins, the black vein and
// the blood blobs are gone: the tendrils below replace them); ash / embers come from the particle pool as before
#define SVV_MIST_K     0.80f   // the mist and the base glow, x the v3.21 strength
#define SVV_GLOW_K     0.30f
static void SvHauntSprites(const SvSrc *s, float now)
{
    const SvAuraPal *P = &kSvAura[s->pal == SVP_BILE];
    const DWORD key = FxHash(s->key * 0x9E3779B9u + 0x1234u);
    const float k = s->k, Pm = s->Pm, Rs = s->Rs, H = s->y1 - s->y0, cy = s->c[1], dir = (key & 1u) ? 1.0f : -1.0f;
    const int nm = 4 + (int)(key % 3u);
    float p[3];
    int i;
    SvSprAdd(s->c, 0.8f * Pm, 0.0f, SVC_SOFT, 1, P->glow,
             SVV_GLOW_K * SV_GLOW_K * k * (0.85f + 0.15f * sinf(1.7f * now + 6.2831853f * FxRnd(key))), 0.0f);
    for (i = 0; i < nm; ++i) {  // mist puffs: slowly turning, slowly orbiting, breathing
        const DWORD hi = FxHash(key + (DWORD)i * 0x9E37u);
        const float ang = 6.2831853f * ((float)i / (float)nm + 0.15f * FxRnd(hi)) + dir * 0.05f * now;
        const float rad = Rs * (0.15f + 0.5f * FxRnd(hi ^ 0x11u));
        const float rot = 6.2831853f * FxRnd(hi ^ 0x22u) + ((i & 1) ? -dir : dir) * (0.08f + 0.08f * FxRnd(hi ^ 0x33u)) * now;
        p[0] = s->c[0] + rad * cosf(ang);
        p[1] = cy + 0.5f * H * (FxRnd(hi ^ 0x44u) - 0.5f) + 0.04f * H * sinf(0.35f * now + (float)i);
        p[2] = s->c[2] + rad * sinf(ang);
        SvSprAdd(p, Pm * (0.6f + 0.3f * FxRnd(hi ^ 0x55u)), rot, SVC_MIST0 + (i & 1), 0, P->mist[i & 1],
                 SVV_MIST_K * SV_MIST_K * k * (0.8f + 0.2f * sinf(0.5f * now + 1.3f * (float)i)), 0.0f);
    }
    if (s->pal == SVP_GOLD)  // the medallion repels it: gold sparks jumping around it every 70 ms
        for (i = 0; i < 6; ++i) {
            const DWORD slot = (DWORD)(now / 0.07f + 0.37f * (float)i), hs = FxHash(key ^ slot * 0x2C1B3C6Du ^ (DWORD)i * 0x297A2D39u);
            p[0] = s->c[0] + Rs * 0.6f * (FxRnd(hs) - 0.5f);
            p[1] = cy + 0.5f * H * (FxRnd(hs ^ 1u) - 0.5f);
            p[2] = s->c[2] + Rs * 0.6f * (FxRnd(hs ^ 2u) - 0.5f);
            SvSprAdd(p, 10.0f + 10.0f * FxRnd(hs ^ 3u), 6.2831853f * FxRnd(hs ^ 4u), SVC_SPARK, 1, kSvGold,
                     (0.4f + 0.6f * FxRnd(hs ^ 5u)) * k / SV_REPEL_K, 2.0f * g_svUnit);
        }
}

// ---- the tendrils (V1-V5). Per haunting (the SVV_H nearest) SVV_T thin veins grow from the ghost's body to anchors
// found by the game's collision segment query: each comes down onto the surface and crawls along it to a dark sucker
// blot with short capillaries (no hit: it ends in the air, curling). Near the player at most SVV_LATCH_ALL of them
// re-target to separate limbs by bowed, separate routes and coil round them. Geometry is built once a frame in screen
// space (rows of a few vertices across: soft untextured strips, z / rhw like the sprites) and drawn in each depth
// pass: a near-black body (premultiplied, ONE / INVSRCALPHA), then the light with BLENDOP MAX (crossing strands do not
// add up): a dark maroon flesh glow, a wet highlight, a thin crimson inner line and sparse feeding pulses that run from
// the anchor to the ghost. Every motion uses real seconds and every random choice a hash of (source key, tendril,
// anchor serial, time slot): the same at 60 or 180 fps; each strand sways, breathes and twitches on its own clock.
#define SVV_H          4        // hauntings with tendrils (the nearest to the player); the others keep the core only
#define SVV_T          7        // tendrils per haunting
#define SVV_N          28       // points along a tendril (latched: the wrap uses the last 45 %)
#define SVV_CASTS      16       // ray casts a frame, all hauntings
#define SVV_CASTS_H    4        // ... per haunting
#define SVV_RAY        650.0f   // ray length from the ghost centre
#define SVV_FAR        900.0f   // an anchor this far from the ghost (it moved) is let go
#define SVV_OFF        4.0f     // the anchor is pulled this far off the surface (along the plane normal) ...
#define SVV_HUG        2.0f     // ... the crawling part lies this far off it
#define SVV_CRAWL0     40.0f    // the crawl along the surface: 0.3 x the length, within CRAWL0 .. CRAWL1
#define SVV_CRAWL1     170.0f
#define SVV_AIR        0.5f     // no hit: the tendril ends in the air at this x the ray, curling
#define SVV_LIFE0      6.0f     // a surface anchor lives LIFE0 .. LIFE0 + LIFE1 s (in the air: 2.5 .. 4 s)
#define SVV_LIFE1      4.0f
#define SVV_GROW_S     0.5f     // a new tendril shoots out over this (ease-out with a slight overshoot)
#define SVV_STRAIN_S   0.25f    // before a snap the tendril strains: pulled 15 % back into the ghost, taut, thin
#define SVV_RECOIL_S   0.35f    // the snapped piece whips back into the ghost over this
#define SVV_SPLAT_FADE 0.9f     // a released sucker blot shrinks and fades over this
#define SVV_W0         20.0f    // full width at the ghost (world units) ...
#define SVV_W1         3.8f     // ... at the surface (the crawl; the coil x 1.5)
#define SVV_MINPX      1.5f     // a half width at least this (720-line pixels) ... (v3.23.0 item 2: 1.1 -> 1.5)
#define SVV_MAXH       0.02f    // ... at most this x the viewport height (the camera close to a tendril)
#define SVV_SAG        0.18f    // sag toward the floor at the middle, x the horizontal length
#define SVV_PULSE_S    1.4f     // pulse slots every 1.1 .. 2.2 s per strand (latched: x 0.45) ...
#define SVV_PULSE_P    0.75f    // ... each slot carries a pulse with this chance (latched: 0.95); v3.23.0: 0.55 -> 0.75
#define SVV_PULSE_D    600.0f   // world units between two pulse slots along a strand
#define SVV_BODY_A     0.90f    // the body's opacity
// v3.23.0 (user item 2: hard to see on Spirit Vision's black world, keep the dark aura): a crimson halo under each strand
// (additive, before the near-black body, which then darkens its middle: a black vein outlined by a red glow)
#define SVV_HALO_X     3.2f     // halo half width, x the body's half width ...
#define SVV_HALO_MINPX 3.0f     // ... at least this (720-line pixels)
#define SVV_HALO_PULSE 0.6f     // brighter by this x where a feeding pulse passes
#define SVV_ROOT_OFF   8.0f     // (v3.23.0 item 15: a root sat this far off its bone; v3.28 CORE: every root is in the heart)
// v3.28 CORE (user, v3.27.3 item 1: "all tendrils come from the same round black core at the heart of the ghost"; the
// nodules, air drips and root clots are gone): one black heart per haunting, a round camera-facing disc at the chest
// bone 9 (else the source centre: wheelchairs, portals ...), drawn after every tendril layer (layers 3 / 4), its depth
// taken nearer the camera so the ghost's own body does not hide it (walls still do); a lub-dub heartbeat; the
// tendrils start under it; in Blood mode it is the only wound (SVV_CORE_DRIPS drip channels at its lower rim).
// staging/v328_spirit/a_tendrils DESIGN.md / NOTES.md; tools/aura_preview.py Veins.Core.
#define SVV_CORE_R      18.0f   // the heart's radius (world units)
#define SVV_CORE_MAXH   0.15f   // its screen radius at most this x the viewport height
#define SVV_CORE_PULL   34.0f   // its depth: this (+ its radius) nearer the camera, the same screen spot
#define SVV_CORE_GROW_S 0.45f   // it swells in over this (ease-out, overshoot) when the haunting first shows
#define SVV_ROOT_IN     0.45f   // a tendril starts this x SVV_CORE_R from the heart's centre, toward its end
#define SVV_BEAT_S      1.3f    // the heartbeat: a lub every this many seconds ...
#define SVV_DUB_S       0.24f   // ... a dub this much later at 60 %
#define SVV_PUMP        0.30f   // each beat swells a tendril's first quarter by this, travelling out
#define SVV_CORE_DRIPS  2       // drip channels at the heart (SpiritGhost=Blood)
#define SVV_SPLAT_R    30.0f    // sucker blot radius (20 .. 28)
#define SVV_TWITCH_P   0.35f    // a twitch / clench in a 1.7 .. 3.1 s slot with this chance
#define SVV_LATCH_R    700.0f   // latching range: the source's damage radius or this, the larger
#define SVV_LATCH_GAP  0.45f    // one more tendril latches every this many seconds ...
#define SVV_LATCH_MAX  3        // ... up to 1 + 2 x closeness per haunting ...
#define SVV_LATCH_ALL  4        // ... and this many on her in all (v3.23.0, V322-R1: the v3.22 plan's 4)
#define SVV_SNAP_X     1.4f     // a latched tendril snaps when stretched to this x its latch length
#define SVV_OUT_S      1.8f     // ... or when she has been out of range this long
#define SVV_WARD_HOLD  1.5f     // after the medallion repelled the ghost, no latching for this long
#define SVV_VA         0.55f    // latched: the approach takes v 0 .. VA, the wrap VA .. 1 ...
#define SVV_TURNS      1.5f     // ... this many turns round the limb
#define SVV_PV         40960    // pre-built vertices a frame (about 840 a tendril at full detail + ~180 halo; v3.23.0:
                                // 32 768 -> 40 960, worst case 28 tendrils = 28 560 + recoils, review SH4-323-02)
#define SVV_CORE_EST   900      // vertices kept free for each tendril still to build (a halo never starves a core)
#define SVV_STR        2048     // strips a frame
static const signed char kSvvCat[SVV_T] = { 0, 0, 0, 0, 1, 1, 2 };  // ray family per tendril: around, down, up
static const int kSvvLimb[5][2] = { { 9, 10 }, { 13, 14 }, { 18, 19 }, { 2, 3 }, { 5, 6 } };  // chest, arms, thighs
static const float kSvvWrapR[5] = { 24.0f, 12.0f, 12.0f, 15.0f, 15.0f };  // coil radius round each bone axis
static const int kSvvLimbOrder[5] = { 1, 4, 0, 2, 3 };  // R arm, L thigh, chest, L arm, R thigh: alternating sides
typedef struct { float body[3], rim[3], wet[3], core[3], hot[3], halo[3]; } SvvPal;
static const SvvPal kSvvPal[3] = {  // v3.23.0 (item 2): crimson rim x 1.5, wet x 1.3, line 0.80 -> 1.00; the halo colours
    { { 0.035f, 0.002f, 0.012f }, { 0.57f, 0.030f, 0.090f }, { 0.55f, 0.180f, 0.195f }, { 1.00f, 0.05f, 0.06f },
      { 1.00f, 0.40f, 0.28f }, { 0.30f, 0.012f, 0.035f } },  // crimson: near-black maroon flesh, a wet sheen, a crimson line
    { { 0.025f, 0.028f, 0.003f }, { 0.09f, 0.11f, 0.012f }, { 0.15f, 0.17f, 0.06f }, { 0.48f, 0.58f, 0.04f },
      { 0.95f, 1.00f, 0.50f }, { 0.10f, 0.12f, 0.010f } },  // bile (Eileen's fit)
    { { 0.040f, 0.025f, 0.004f }, { 0.30f, 0.19f, 0.04f }, { 0.45f, 0.38f, 0.20f }, { 1.00f, 0.72f, 0.25f },
      { 1.00f, 0.95f, 0.75f }, { 0.30f, 0.20f, 0.05f } } };  // gold (the medallion's let-go)
typedef struct {
    int st, kind, bi;        // st: 0 waits for an anchor, 1 live, 2 straining (snaps after SVV_STRAIN_S); kind: 0 surface,
    DWORD ser;               // 1 air, 2 latched to the player's limb kSvvLimb[bi]; ser: the anchor serial (hash input)
    float a[3], n[3];        // anchor and its surface normal (toward the ghost; air: the ray direction)
    float t0, life, wantT, stT, lu, lth, llen, outT;  // grow start, anchor life, next cast, strain start, latch: along /
    int rcN, rcGold;                                  // angle / length, out of range since; the recoiling piece:
    float rcT, rcC[3], rcP[SVV_N][3], rcW[SVV_N];     // the snap time, ghost centre, its points / widths
    int osOn;                                          // the released blot (fading)
    DWORD osSer;
    float osT, osA[3], osN[3];
} SvvTen;
typedef struct { DWORD key; float t0, lastT, latchT, goldT; int los; SvvTen t[SVV_T];
                 float gulp, gulpAcc; } SvvSlot;  // los: V322-R2 round robin; gulp: v3.28 CORE (a feeding pulse at a root)
typedef struct { int first, rows, K, layer; } SvvStr;
typedef struct {  // a tendril's shape this frame
    int kind;
    DWORD seed;
    float R0[3], E[3], D[3], p1[3], p2[3], pa[3], pb[3], C[3];
    float L, La, va, g, taut, sag, amp, nw, fq, ph, ph2, per, pp, cyc, live, k, t0, sw, swf, tw, twd;
    float A[3], n[3], u[3], wd[3], crawl;           // surface: the anchor on the surface, its normal, crawl direction
    float L0[3], ax[3], e1[3], e2[3], rw, th0, wl;  // latched: the latch point, bone axis, its basis, coil radius / angle / length
    float Hc[3], bph;                               // v3.28 CORE: the heart, its beat phase (s)
} SvvGeo;
static SvvSlot g_svv[SVV_H];
static GvVtx g_svvPv[SVV_PV];
static SvvStr g_svvStr[SVV_STR];
static int g_svvPvN, g_svvStrN, g_svvDrop, g_svvCasts, g_svvLatched, g_svvTen, g_svvLeft, g_svvHaloSkip, g_svvLosCut, g_svvNoSkel;
static int g_svvCores, g_svvCoresBone;  // v3.28 CORE: hearts this frame (on a chest bone)
static float g_svvPull;                 // v3.28 CORE: SvvProj takes the depth this much nearer the camera (the heart only)
static LONG g_svvStamp = -1;
static DWORD g_svvRoom = 0xFFFFFFFFu, g_svvBOk;
static float g_svvB[25][3], g_svvPx, g_svvNow;
static __declspec(align(16)) float g_svvSeg[8], g_svvPl[4];
static int g_svVeinPaused;  // set by GvPass around a paused-frame aura block (PLAN G3)
static int g_svvCastsLeft;  // v3.26.0: the frame's vein cast budget left after the veins (the blood's share)
// v3.26.0 (round 1, PLAN 2.3): each tendril's root this frame, for the blood (SvBloodFrame)
typedef struct { LONG stamp; float r0[3], g, k, pulse; DWORD seed; int pal;
                 LONG bFrame; DWORD bSeed; float bT; int bOk, core; } SvbRoot;  // b*: the blood's last pass (INSP326-015)
// v3.28 CORE: only the hearts fill it now ([slot][0 .. SVV_CORE_DRIPS - 1], core = 1: drips, no seep)
static SvbRoot g_sbRoot[SVV_H][SVV_T];
static int SvVeinsPaused(void) { return g_svVeinPaused; }

#define SVV_R(h, n) FxRnd((h) + (DWORD)(n) * 0x27D4EB2Fu)
static float SvvLen(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
static float SvvNorm(float v[3])
{
    const float l = SvvLen(v);
    if (l > 1e-6f) v[0] /= l, v[1] /= l, v[2] /= l;
    return l;
}
static void SvvCross(float o[3], const float a[3], const float b[3])
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}
static float SvvBack(float x, float c)  // ease-out with an overshoot of about c x 0.1 (0 -> 1)
{
    x = FxSat(x) - 1.0f;
    return 1.0f + (c + 1.0f) * x * x * x + c * x * x;
}
static float SvvBump(float f)  // one feeding pulse on a 0..1 cycle: a cos^2 bump at 0.5, 0.24 of the cycle wide
{
    const float d = fabsf(f - floorf(f) - 0.5f);
    float c;
    if (d >= 0.12f) return 0.0f;
    c = cosf(3.14159265f * d / 0.24f);
    return c * c;
}
static void SvvPerp(float e1[3], float e2[3], const float ax[3])  // an orthonormal basis around ax
{
    const float up[3] = { 0.0f, 1.0f, 0.0f }, side[3] = { 1.0f, 0.0f, 0.0f };
    SvvCross(e1, ax, fabsf(ax[1]) < 0.9f ? up : side);
    SvvNorm(e1);
    SvvCross(e2, ax, e1);
}
static float SvvSlotEnv(DWORD seed, float now, float per, float p, float *sgn)  // a hashed event: fast attack, slow release
{
    const float tt = now + per * SVV_R(seed, 1), sl = floorf(tt / per), tau = tt - sl * per;
    const DWORD hs = FxHash(seed ^ (DWORD)(int)sl * 0x2C1B3C6Du);
    float r;
    if (FxRnd(hs) >= p) return 0.0f;
    if (sgn) *sgn = (hs & 0x100u) ? 1.0f : -1.0f;
    r = 1.0f - FxSat((tau - 0.06f) / 0.45f);
    return FxSat(tau / 0.06f) * r * r;
}
// v3.28 CORE: the heartbeat (closed form on real seconds) and the heart's place
static float SvvThump(float x)  // one heart sound: 0.05 s attack, 0.11 s exponential decay
{
    if (x < 0.0f) return 0.0f;
    return x < 0.05f ? x / 0.05f : expf(-(x - 0.05f) / 0.11f);
}
static float SvvBeat(float t)  // lub-dub, 0..1
{
    const float x = t - floorf(t / SVV_BEAT_S) * SVV_BEAT_S, b = SvvThump(x) + 0.6f * SvvThump(x - SVV_DUB_S);
    return b < 1.0f ? b : 1.0f;
}
static DWORD SvvHeartKey(DWORD key) { return FxHash(key * 0x9E3779B9u ^ 0xC0DEu); }
static void SvvHeart(const SvSrc *s, float now, float o[3])  // the chest bone 9 (KB 113), else the source centre (for a
{                                                            // skeleton: its mean); a slow 1.5-unit drift
    const DWORD hh = SvvHeartKey(s->key);
    memcpy(o, s->c, 12);
    if (s->nb > 9) {
        const float *b = s->bone[9];
        const float dv[3] = { b[0] - s->c[0], b[1] - s->c[1], b[2] - s->c[2] };
        if (SvvLen(dv) < 160.0f) memcpy(o, b, 12);
    }
    o[0] += 1.5f * sinf(0.50f * now + 6.2831853f * SVV_R(hh, 1));
    o[1] += 1.5f * sinf(0.37f * now + 6.2831853f * SVV_R(hh, 2));
    o[2] += 1.5f * cosf(0.43f * now + 6.2831853f * SVV_R(hh, 3));
}

// ---- the collision segment query (as TpBlocked / Item Shine): the hit point, pulled SVV_OFF off the surface
static int SvvCastT(const float p0[3], const float p1[3], float hit[3], float nrm[3], float tmin)
{
    float a, b, t, l;
    int r, j;
    __try {
        if (!*(volatile DWORD *)0x12F6D74) return 0;  // no collision loaded
        for (j = 0; j < 3; ++j) g_svvSeg[j] = p0[j], g_svvSeg[4 + j] = p1[j];
        g_svvSeg[3] = g_svvSeg[7] = 1.0f;
        r = ((int(__cdecl *)(const float *, float *))0x561CD0)(g_svvSeg, g_svvPl);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (!r) return 0;
    a = g_svvPl[0] * p0[0] + g_svvPl[1] * p0[1] + g_svvPl[2] * p0[2] + g_svvPl[3];
    b = g_svvPl[0] * p1[0] + g_svvPl[1] * p1[1] + g_svvPl[2] * p1[2] + g_svvPl[3];
    if (!((a < 0.0f) != (b < 0.0f)) || a == b) return 0;
    t = a / (a - b);
    l = SvvLen(g_svvPl);
    if (!(t > tmin && t <= 1.0f) || !(l > 1e-6f)) return 0;
    for (j = 0; j < 3; ++j) {
        nrm[j] = g_svvPl[j] / l * (a > 0.0f ? 1.0f : -1.0f);  // toward the ghost
        hit[j] = p0[j] + (p1[j] - p0[j]) * t + nrm[j] * SVV_OFF;
    }
    return _finite(hit[0] + hit[1] + hit[2]);
}
static int SvvCast(const float p0[3], const float p1[3], float hit[3], float nrm[3])  // the veins: t > 0.02
{
    return SvvCastT(p0, p1, hit, nrm, 0.02f);
}
// the player's bones this frame (her model: [0x10A0F20 + 0x1B0]; bone b = [model + 0xC0] + 16 b + 12)
static void SvvBones(const float pp[3])
{
    g_svvBOk = 0;
    __try {
        const BYTE *model = *(const BYTE *const *)(0x10A0F20 + 0x1B0), *skel;
        const float *mats;
        int b, nb = 25;
        if (!model) return;
        mats = *(const float *const *)(model + 0xC0);
        skel = *(const BYTE *const *)(model + 0xE0);
        if (!mats) return;
        if (skel) nb = *(const int *)(skel + 0x14);
        for (b = 0; b < 25 && b < nb; ++b) {
            const float *m = mats + 16 * b + 12;
            if (_finite(m[0] + m[1] + m[2]) && fabsf(m[0] - pp[0]) < 400.0f && fabsf(m[1] - pp[1]) < 400.0f && fabsf(m[2] - pp[2]) < 400.0f) {
                memcpy(g_svvB[b], m, 12);
                g_svvBOk |= 1u << b;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_svvBOk = 0;
    }
}
static int SvvLimbOk(int bi) { return bi >= 0 && bi < 5 && (g_svvBOk >> kSvvLimb[bi][0] & 1u) && (g_svvBOk >> kSvvLimb[bi][1] & 1u); }

// ---- a tendril's shape: root on the ghost's body; an air span (cubic + sag + slow sway + peristalsis + twitches) down
// onto the surface, then a crawl along it to the anchor (or a coil round her limb, or a curl in the air)
static int SvvSetup(const SvvSlot *S, const SvSrc *s, int ti, float now, SvvGeo *G)
{
    const SvvTen *t = &S->t[ti];
    const DWORD hk = FxHash(s->key * 0x9E3779B9u ^ 0x5EEDu), ht = FxHash(hk + (DWORD)ti * 0x632BE5ABu);
    float out[3], d[3], v[3], Lh, ext;
    int j;
    if (t->st == 0) return 0;
    memset(G, 0, sizeof *G);
    G->kind = t->kind;
    G->seed = ht;
    G->t0 = t->t0;
    memcpy(G->C, s->c, 12);
    if (t->kind == 2) {  // latched: the coil starts on a circle round the bone axis at the latch point
        const float *b0 = g_svvB[kSvvLimb[t->bi][0]], *b1 = g_svvB[kSvvLimb[t->bi][1]];
        if (!SvvLimbOk(t->bi)) return 0;
        for (j = 0; j < 3; ++j) G->ax[j] = b1[j] - b0[j], G->L0[j] = b0[j] + t->lu * (b1[j] - b0[j]);
        if (SvvNorm(G->ax) < 1.0f) return 0;
        SvvPerp(G->e1, G->e2, G->ax);
        G->rw = kSvvWrapR[t->bi];
        G->th0 = t->lth;
        G->wl = t->bi ? 46.0f : 50.0f;
        for (j = 0; j < 3; ++j)
            G->E[j] = G->L0[j] - 0.5f * G->wl * G->ax[j] + G->rw * (cosf(G->th0) * G->e1[j] + sinf(G->th0) * G->e2[j]);
    } else {
        memcpy(G->E, t->a, 12);
    }
    // v3.28 CORE: every root under the heart's disc, toward the strand's end, breathing a little (v3.23.0 rooted them on
    // the ghost's bones, an unmatched source in a box round its centre: flat, scattered origins, user v3.27.3 item 1)
    SvvHeart(s, now, G->Hc);
    for (j = 0; j < 3; ++j) d[j] = G->E[j] - G->Hc[j];
    if (SvvNorm(d) < 1e-3f) d[0] = 1.0f, d[1] = d[2] = 0.0f;
    G->R0[0] = G->Hc[0] + d[0] * SVV_ROOT_IN * SVV_CORE_R + 1.5f * sinf(0.9f * now + 6.2831853f * SVV_R(ht, 2));
    G->R0[1] = G->Hc[1] + d[1] * SVV_ROOT_IN * SVV_CORE_R + 1.5f * sinf(0.7f * now + 6.2831853f * SVV_R(ht, 4));
    G->R0[2] = G->Hc[2] + d[2] * SVV_ROOT_IN * SVV_CORE_R + 1.5f * cosf(0.8f * now + 6.2831853f * SVV_R(ht, 2));
    if (t->kind == 0) {  // the crawl: the anchor on the surface, the direction back toward the root within the plane
        float q[3], dn;
        memcpy(G->n, t->n, 12);
        for (j = 0; j < 3; ++j) G->A[j] = t->a[j] - G->n[j] * (SVV_OFF - SVV_HUG), q[j] = G->R0[j] - G->A[j];
        dn = q[0] * G->n[0] + q[1] * G->n[1] + q[2] * G->n[2];
        for (j = 0; j < 3; ++j) G->u[j] = q[j] - dn * G->n[j];
        if (SvvNorm(G->u) < 1.0f) SvvPerp(G->u, G->wd, G->n);
        SvvCross(G->wd, G->n, G->u);
        G->crawl = 0.3f * SvvLen(q);
        G->crawl = G->crawl < SVV_CRAWL0 ? SVV_CRAWL0 : G->crawl > SVV_CRAWL1 ? SVV_CRAWL1 : G->crawl;
        for (j = 0; j < 3; ++j) G->E[j] = G->A[j] + G->u[j] * G->crawl;  // where it touches down
    }
    G->taut = t->st == 2 ? FxSat((now - t->stT) / SVV_STRAIN_S) : 0.0f;
    for (j = 0; j < 3; ++j) G->D[j] = G->E[j] - G->R0[j];
    G->La = SvvNorm(G->D);
    if (!(G->La > 1.0f)) return 0;
    G->L = G->La + G->crawl;
    G->va = t->kind == 0 ? G->La / G->L : t->kind == 2 ? SVV_VA : 1.0f;
    if (t->kind == 2) G->taut = FxSat((G->La / (t->llen > 1.0f ? t->llen : 1.0f) - 1.0f) / (SVV_SNAP_X - 1.0f));
    // (straining: v3.28 CORE keeps the root in the heart; the strand thins and goes taut, SvvWidth / sag / amp)
    for (j = 0; j < 3; ++j) out[j] = G->R0[j] - G->Hc[j];
    if (SvvNorm(out) < 1e-3f) memcpy(out, G->D, 12);
    for (j = 0; j < 3; ++j) v[j] = 0.5f * out[j] + 0.5f * G->D[j];
    SvvNorm(v);
    ext = G->La * 0.3f < 70.0f ? G->La * 0.3f : 70.0f;
    for (j = 0; j < 3; ++j) {
        G->p1[j] = G->R0[j] + v[j] * 0.33f * G->La;
        if (t->kind == 0) G->p2[j] = G->E[j] + G->n[j] * ext + G->u[j] * 0.6f * ext;  // comes down onto the surface
        else if (t->kind == 2) G->p2[j] = G->E[j] + (cosf(G->th0) * G->e1[j] + sinf(G->th0) * G->e2[j]) * (ext < 60.0f ? ext : 60.0f);
        else G->p2[j] = G->E[j] - G->D[j] * 0.3f * G->La;
    }
    {   // wave axes: across the view (always visible on screen) and the third axis
        float m[3], vd[3];
        for (j = 0; j < 3; ++j) m[j] = 0.5f * (G->R0[j] + G->E[j]), vd[j] = m[j] - g_svEye[j];
        if (!g_svEyeOk || SvvNorm(vd) < 1.0f) vd[0] = 0.0f, vd[1] = 0.0f, vd[2] = 1.0f;
        SvvCross(G->pa, G->D, vd);
        if (SvvNorm(G->pa) < 0.05f) SvvPerp(G->pa, vd, G->D);
        SvvCross(G->pb, G->D, G->pa);
    }
    if (t->kind == 2) {  // separate routes to separate limbs: each latched strand bows to its own side
        const float bow = ((t->bi & 1) ? 0.22f : -0.22f) * G->La;
        for (j = 0; j < 3; ++j) G->p1[j] += G->pa[j] * bow, G->p2[j] += G->pa[j] * bow * 0.35f;
    }
    Lh = sqrtf((G->E[0] - G->R0[0]) * (G->E[0] - G->R0[0]) + (G->E[2] - G->R0[2]) * (G->E[2] - G->R0[2]));
    G->ph = 6.2831853f * SVV_R(ht, 6);
    G->ph2 = 6.2831853f * SVV_R(ht, 7);
    G->tw = SvvSlotEnv(ht ^ 0x7E1Cu, now, 1.7f + 1.4f * SVV_R(ht, 12), SVV_TWITCH_P, &G->twd);  // a twitch / clench
    G->sag = (t->kind == 2 ? 0.08f : SVV_SAG) * Lh * (1.0f - G->taut) * (1.0f + 0.12f * sinf((0.3f + 0.3f * SVV_R(ht, 13)) * now + G->ph)) *
             (1.0f - 0.5f * G->tw);
    G->amp = (3.0f + 5.0f * SVV_R(ht, 8)) * (G->La / 450.0f < 0.7f ? 0.7f : G->La / 450.0f > 1.5f ? 1.5f : G->La / 450.0f) *
             (t->kind == 1 ? 1.8f : 1.0f) * (1.0f - G->taut);
    G->nw = 0.8f + 1.0f * SVV_R(ht, 9);
    G->fq = 0.5f + 0.9f * SVV_R(ht, 10);
    G->sw = 0.05f * G->La * (1.0f - G->taut) * (t->kind == 2 ? 0.5f : 1.0f);  // the slow breathing sway of the whole span
    G->swf = 0.12f + 0.18f * SVV_R(ht, 14);
    G->per = SVV_PULSE_S * (0.8f + 0.8f * SVV_R(ht, 11)) * (t->kind == 2 ? 0.45f : 1.0f);
    G->pp = t->kind == 2 ? 0.95f : SVV_PULSE_P;
    G->cyc = G->L / SVV_PULSE_D < 0.5f ? 0.5f : G->L / SVV_PULSE_D > 1.6f ? 1.6f : G->L / SVV_PULSE_D;
    G->g = SvvBack((now - t->t0) / (t->kind == 2 ? 0.4f : SVV_GROW_S), 1.1f) * FxSat(s->reach);  // v3.23.0: x the reach
    G->live = FxSat((now - t->t0 - SVV_GROW_S) / 0.3f);                                       // (item 17: retract / regrow)
    G->k = s->kv;
    G->bph = SVV_BEAT_S * SVV_R(SvvHeartKey(s->key), 4);  // v3.28 CORE: the heart's beat phase (the pump bulge)
    return 1;
}
static float SvvPulse(const SvvGeo *G, float v, float now)  // sparse: each pulse slot carries one with chance pp
{
    const float f = now / G->per + v * G->cyc + G->ph;
    if (FxRnd(G->seed ^ (DWORD)(int)floorf(f) * 0x9E3779B1u) >= G->pp) return 0.0f;
    return G->live * SvvBump(f);
}
static void SvvAt(const SvvGeo *G, float v, float now, float o[3])
{
    const float va = G->va;
    float u, a, b, c, d, env, w1, w2;
    int j;
    if (G->kind == 2 && v > va) {  // the coil: SVV_TURNS turns along the bone axis, squeezing with each pulse
        const float kk = (v - va) / (1.0f - va), th = G->th0 + 6.2831853f * SVV_TURNS * kk, al = G->wl * (kk - 0.5f);
        const float r = G->rw * (1.0f - 0.18f * SvvPulse(G, 1.0f, now) - 0.12f * G->tw);
        for (j = 0; j < 3; ++j) o[j] = G->L0[j] + al * G->ax[j] + r * (cosf(th) * G->e1[j] + sinf(th) * G->e2[j]);
        return;
    }
    if (G->kind == 0 && v > va) {  // the crawl: along the surface to the anchor, meandering, creeping slowly
        const float s = (v - va) / (1.0f - va), env2 = sinf(3.14159265f * FxSat(s));
        const float m = G->crawl * (0.10f * sinf(5.0f * s + G->ph) + 0.03f * sinf(0.3f * now + G->ph2)) * env2;
        for (j = 0; j < 3; ++j) o[j] = G->E[j] + (G->A[j] - G->E[j]) * s + G->wd[j] * m;
        return;
    }
    u = v / va;
    if (G->kind == 1 && u > 0.6f) u = 0.6f;  // in the air: the last 40 % curls (below)
    a = (1.0f - u) * (1.0f - u) * (1.0f - u), b = 3.0f * (1.0f - u) * (1.0f - u) * u, c = 3.0f * (1.0f - u) * u * u, d = u * u * u;
    env = sinf(3.14159265f * FxSat(v / va));
    w1 = G->amp * env * sinf(6.2831853f * (G->nw * v + G->fq * now) + G->ph) + G->twd * G->tw * 0.06f * G->La * env;
    w2 = 0.5f * G->amp * env * sinf(6.2831853f * (0.7f * G->nw * v + 0.6f * G->fq * now) + G->ph2) +
         G->sw * env * sinf(6.2831853f * G->swf * now + G->ph2);
    if (G->g < 1.0f)  // a growing tip whips about, searching
        w1 += 20.0f * (1.0f - G->g) * (v / (G->g > 0.05f ? G->g : 0.05f)) * (v / (G->g > 0.05f ? G->g : 0.05f)) * sinf(11.0f * now + G->ph);
    for (j = 0; j < 3; ++j) o[j] = a * G->R0[j] + b * G->p1[j] + c * G->p2[j] + d * G->E[j] + w1 * G->pa[j] + w2 * G->pb[j];
    o[1] += G->sag * 4.0f * u * (1.0f - u);
    if (G->kind == 1 && v > 0.6f) {  // the curl: a tightening spiral starting along the path, writhing
        const float q = (v - 0.6f) / 0.4f, ph = 1.25f * 3.14159265f * powf(q, 1.3f) * (1.0f + 0.25f * sinf(1.4f * now + G->ph));
        const float r = 0.12f * G->La * (1.0f - 0.5f * q), be = 0.6f * sinf(0.9f * now + G->ph2);
        float pc[3];
        for (j = 0; j < 3; ++j) pc[j] = cosf(be) * G->pa[j] + sinf(be) * G->pb[j];
        for (j = 0; j < 3; ++j) o[j] += r * (sinf(ph) * G->D[j] + (1.0f - cosf(ph)) * pc[j]);
    }
}
static float SvvWidth(const SvvGeo *G, float v, float now)
{
    const float va = G->va;
    float w = v > va ? SVV_W1 * (G->kind == 2 ? 1.5f : 1.0f) : SVV_W0 + (SVV_W1 * 1.3f - SVV_W0) * powf(FxSat(v / va), 0.6f);
    if (v < 0.1f) w *= 1.0f + 0.3f * (1.0f - v / 0.1f);  // the root swells a little into the ghost's body
    w *= 1.0f + 0.08f * sinf((1.0f + 0.8f * SVV_R(G->seed, 15)) * now + G->ph + 4.0f * v);  // breathing, own rate
    if (v <= va) w *= 1.0f + 0.3f * sinf(19.0f * v + G->ph) * sinf(7.3f * v + G->ph2) - 0.25f * G->tw;  // lumpy; clenching
    w *= 1.0f - 0.45f * G->taut;
    w *= 1.0f + 0.5f * SvvPulse(G, v, now);
    if (v < 0.25f)  // v3.28 CORE: each heartbeat pumps a bulge a quarter of the way out (delayed with v)
        w *= 1.0f + SVV_PUMP * SvvBeat(now + G->bph - 0.9f * v) * (1.0f - v / 0.25f);
    if (G->g < 1.0f) w *= 0.35f + 0.65f * FxSat((G->g - v) / 0.1f);
    return w;
}

// ---- the strips: rows of K vertices (screen x, y, z, rhw, colour); z < 0 marks a row clipped at the near plane
static int SvvBegin(int K, int layer, int rows)
{
    SvvStr *s;
    if (g_svvStrN >= SVV_STR || g_svvPvN + K * rows > SVV_PV) {
        ++g_svvDrop;
        return 0;
    }
    s = &g_svvStr[g_svvStrN];
    s->first = g_svvPvN, s->K = K, s->layer = layer, s->rows = 0;
    return 1;
}
static void SvvEnd(void)
{
    SvvStr *s = &g_svvStr[g_svvStrN];
    s->rows = (g_svvPvN - s->first) / s->K;
    if (s->rows >= 2) ++g_svvStrN;
    else g_svvPvN = s->first;
}
static void SvvV(float x, float y, const float pr[4], DWORD c)
{
    GvVtx *v = &g_svvPv[g_svvPvN++];
    v->x = x - 0.5f, v->y = y - 0.5f;
    v->z = pr ? pr[2] : -1.0f;
    v->rhw = pr ? 1.0f / pr[3] : 1.0f;
    v->c = c;
}
static DWORD SvvPre(const float c[3], float a)  // premultiplied
{
    a = FxSat(a);
    return (DWORD)(a * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(c[0]) * a * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(c[1]) * a * 255.0f + 0.5f) << 8 |
           (DWORD)(FxSat(c[2]) * a * 255.0f + 0.5f);
}
static DWORD SvvAdd(const float c[3], float k) { return GvColour(c, k); }
static int SvvProj(const float p[3], float pr[4])  // x, y, z, w
{
    float h[4];
    GvXform(p, h);
    if (!GvScreen(h, g_svVp, &pr[0], &pr[1])) return 0;
    pr[2] = SvZH(h);
    pr[3] = h[3];
    if (g_svvPull > 0.0f && g_svEyeOk) {  // v3.28 CORE: the depth of a point this much nearer the camera (same x / y / w)
        float q[3], e[3], l;
        int j;
        for (j = 0; j < 3; ++j) e[j] = g_svEye[j] - p[j];
        l = SvvLen(e);
        if (l > 2.0f * g_svvPull) {
            for (j = 0; j < 3; ++j) q[j] = p[j] + e[j] * g_svvPull / l;
            GvXform(q, h);
            if (h[3] > 0.0f) pr[2] = SvZH(h);
        }
    }
    return 1;
}
static float SvvFade(const float p[3], float w, float r)  // the near-camera fade (as SvSprAdd)
{
    float d = w;
    if (g_svEyeOk) {
        const float dv[3] = { p[0] - g_svEye[0], p[1] - g_svEye[1], p[2] - g_svEye[2] };
        d = SvvLen(dv);
    }
    return FxSmooth(SV_NEAR0, SV_NEAR1, d - r);
}
// a tube along n world points: widths w (world), pulse pu (0..1), strength k. detail 1: body 5 across, light 7 across
// (edge, maroon, wet sheen, maroon, crimson line, maroon, edge); 0: 3 and 3 (side veins, capillaries, drips).
// Thinner than the minimum on screen (far away): drawn at the minimum with the light dimmed by the ratio
static void SvvTube(const float (*P)[3], const float *w, const float *pu, const float *k, int n, int detail, const SvvPal *pal)
{
    static const float kBo[5] = { -1.0f, -0.55f, 0.0f, 0.55f, 1.0f }, kBa[5] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    static const float kLo[7] = { -1.0f, -0.68f, -0.48f, -0.28f, 0.0f, 0.45f, 1.0f };
    static const float kBo0[3] = { -1.0f, 0.0f, 1.0f }, kBa0[3] = { 0.0f, 1.0f, 0.0f };
    const float lo = SVV_MINPX * g_svUnit, hi = SVV_MAXH * (float)g_svVp->h;
    float pr[SVV_N][4], nx[SVV_N], ny[SVV_N], hw[SVV_N], fd[SVV_N], lk[SVV_N];
    int ok[SVV_N], i, c, j, KB = detail ? 5 : 3, KL = detail ? 7 : 3;
    float lx = 1.0f, ly = 0.0f;
    if (n < 2 || n > SVV_N) return;
    for (i = 0; i < n; ++i) ok[i] = SvvProj(P[i], pr[i]);
    for (i = 0; i < n; ++i) {
        int a = i - 1, b = i + 1;
        float tx, ty, tl, h;
        if (!ok[i]) continue;
        while (a >= 0 && !ok[a]) --a;
        while (b < n && !ok[b]) ++b;
        if (a < 0) a = i;
        if (b >= n) b = i;
        tx = pr[b][0] - pr[a][0], ty = pr[b][1] - pr[a][1], tl = sqrtf(tx * tx + ty * ty);
        if (tl > 1e-3f) lx = -ty / tl, ly = tx / tl;
        nx[i] = lx, ny[i] = ly;
        h = 0.5f * w[i] * g_svvPx / pr[i][3];
        hw[i] = h < lo ? lo : h > hi ? hi : h;
        lk[i] = h < lo ? 0.35f + 0.65f * h / lo : 1.0f;  // far: the light dims rather than the vein growing fat
        fd[i] = k[i] * SvvFade(P[i], pr[i][3], w[i]);
    }
    if (SvvBegin(KB, 0, n)) {  // the body: near-black maroon
        for (i = 0; i < n; ++i)
            for (c = 0; c < KB; ++c) {
                const float o = detail ? kBo[c] : kBo0[c];
                if (!ok[i]) SvvV(0.0f, 0.0f, NULL, 0);
                else SvvV(pr[i][0] + nx[i] * o * hw[i], pr[i][1] + ny[i] * o * hw[i], pr[i], SvvPre(pal->body, SVV_BODY_A * fd[i] * (detail ? kBa[c] : kBa0[c])));
            }
        SvvEnd();
    }
    if (SvvBegin(KL, 1, n)) {  // the light (BLENDOP MAX): maroon flesh, a wet sheen, a thin crimson line, hot pulses
        for (i = 0; i < n; ++i) {
            const float p = pu[i];
            float rim[3], core[3], wet[3];
            for (j = 0; j < 3; ++j) {
                rim[j] = pal->rim[j] * (0.75f + 0.5f * p);
                wet[j] = pal->wet[j] * (0.8f + 0.4f * p);
                core[j] = pal->core[j] * (0.60f + 0.8f * p) + pal->hot[j] * 0.45f * p * p;
            }
            if (!detail)
                for (j = 0; j < 3; ++j) core[j] = 0.8f * rim[j] + 0.35f * core[j];
            for (c = 0; c < KL; ++c) {
                const float o = detail ? kLo[c] : kBo0[c];
                const float *col = detail ? (c == 4 ? core : c == 2 ? wet : rim) : core;
                const float kk = (detail ? (c == 0 || c == 6) : (c != 1)) ? 0.0f : fd[i] * lk[i];
                if (!ok[i]) SvvV(0.0f, 0.0f, NULL, 0);
                else SvvV(pr[i][0] + nx[i] * o * hw[i], pr[i][1] + ny[i] * o * hw[i], pr[i], SvvAdd(col, kk));
            }
        }
        SvvEnd();
    }
    if (detail && pal->halo[0] + pal->halo[1] + pal->halo[2] > 0.0f) {  // v3.23.0 (item 2): the halo (layer 2, additive,
        static const float kHo[5] = { -1.0f, -0.45f, 0.0f, 0.45f, 1.0f }, kHa[5] = { 0.0f, 0.45f, 1.0f, 0.45f, 0.0f };
        const float hlo = SVV_HALO_MINPX * g_svUnit;  // drawn before the bodies); only with room for the cores still to come
        if (g_svvPvN + 5 * n + SVV_CORE_EST * g_svvLeft > SVV_PV) {
            ++g_svvHaloSkip;
        } else if (SvvBegin(5, 2, n)) {
            for (i = 0; i < n; ++i) {
                const float hh = SVV_HALO_X * hw[i] > hlo ? SVV_HALO_X * hw[i] : hlo;
                float col[3];
                for (j = 0; j < 3; ++j) col[j] = pal->halo[j] * (1.0f + SVV_HALO_PULSE * pu[i]);
                for (c = 0; c < 5; ++c) {
                    if (!ok[i]) SvvV(0.0f, 0.0f, NULL, 0);
                    else SvvV(pr[i][0] + nx[i] * kHo[c] * hh, pr[i][1] + ny[i] * kHo[c] * hh, pr[i], SvvAdd(col, kHa[c] * fd[i]));
                }
            }
            SvvEnd();
        }
    }
}
#define SVV_PREMUL(layer) ((layer) == 0 || (layer) == 3)  // v3.28 CORE: layers 0 / 3 premultiplied, 1 / 2 / 4 additive
// a radial blot: spokes round a centre, K rings (radius fraction rr[], colour rc[][4]: rgb + strength); in the world
// plane (t1, t2) or camera-facing (t1 NULL); spoke radius x (1 - lump + 2 lump rnd); star 0..1: odd spokes short
static void SvvBlot(const float c[3], const float *t1, const float *t2, float r, int sp, DWORD seed, float lump, float star,
                    int layer, int K, const float *rr, const float (*rc)[4], float minPx)
{
    float pc[4], rpx = 0.0f, fd, ang;
    int j, m;
    if (K < 2 || sp < 3 || sp > 24 || !SvvProj(c, pc)) return;
    fd = SvvFade(c, pc[3], r);
    if (!(fd > 0.004f)) return;
    if (!t1) {
        rpx = r * g_svvPx / pc[3];
        if (rpx < minPx * g_svUnit) rpx = minPx * g_svUnit;
        if (rpx > 0.08f * (float)g_svVp->h) rpx = 0.08f * (float)g_svVp->h;
    }
    if (!SvvBegin(K, layer, sp + 1)) return;
    for (j = 0; j <= sp; ++j) {
        const int jj = j % sp;
        float f = 1.0f - lump + 2.0f * lump * SVV_R(seed, jj) + 0.06f * sinf(2.3f * g_svvNow + 1.7f * (float)jj);
        f *= (jj & 1) ? 1.0f - star * (0.55f - 0.15f * SVV_R(seed, jj + 40)) : 1.0f + star * (0.4f * SVV_R(seed, jj + 40) - 0.15f);
        ang = 6.2831853f * ((float)jj + 0.3f * (SVV_R(seed, jj + 80) - 0.5f)) / (float)sp + 6.2831853f * SVV_R(seed, 99);
        for (m = 0; m < K; ++m) {
            const float rad = rr[m] * f, a = rc[m][3] * fd;
            const DWORD col = SVV_PREMUL(layer) ? SvvPre(rc[m], a) : SvvAdd(rc[m], a);
            if (t1) {
                float p[3], pv[4];
                int q;
                for (q = 0; q < 3; ++q) p[q] = c[q] + r * rad * (cosf(ang) * t1[q] + sinf(ang) * t2[q]);
                if (SvvProj(p, pv)) SvvV(pv[0], pv[1], pv, col);
                else SvvV(0.0f, 0.0f, NULL, 0);
            } else {
                SvvV(pc[0] + rpx * rad * cosf(ang), pc[1] + rpx * rad * sinf(ang), pc, col);
            }
        }
    }
    SvvEnd();
}
// a sucker blot on the surface: a dark, slightly lobed disc with a dim maroon lip round a black mouth, clenching with
// each gulp, and 3-4 short capillaries over the surface
static void SvvSplat(const float a[3], const float n[3], DWORD seed, float scale, float k, float throb, const SvvPal *pal)
{
    static const float kR3[3] = { 0.0f, 1.0f, 1.25f }, kR4[4] = { 0.0f, 0.3f, 0.55f, 1.0f };
    const float r = SVV_SPLAT_R * (0.85f + 0.3f * SVV_R(seed, 1)) * scale * (1.0f - 0.12f * throb);
    float t1[3], t2[3], rc[4][4], c[3];
    int i, j, nc = 3 + (SVV_R(seed, 2) < 0.5f);
    if (!(k > 0.004f) || !(r > 0.5f)) return;
    SvvPerp(t1, t2, n);
    for (j = 0; j < 3; ++j) c[j] = a[j] - n[j] * (SVV_OFF - SVV_HUG);  // on the surface, as the crawl
    for (i = 0; i < nc; ++i) {  // capillaries first: the blot lies over their roots
        const float ph = 6.2831853f * ((float)i + 0.6f * SVV_R(seed, 10 + i)) / (float)nc, len = r * (1.5f + 0.9f * SVV_R(seed, 20 + i));
        float P[4][3], w[4], pu[4], kk[4];
        int q;
        for (q = 0; q < 4; ++q) {  // from under the blot's edge out over the surface, kinked
            const float s = (0.6f + (len / r - 0.6f) * (float)q / 3.0f) * r;
            const float bend = 0.2f * len * sinf(1.7f * (float)q + 6.2831853f * SVV_R(seed, 30 + i)) * (float)q / 3.0f;
            for (j = 0; j < 3; ++j) P[q][j] = c[j] + s * (cosf(ph) * t1[j] + sinf(ph) * t2[j]) + bend * (-sinf(ph) * t1[j] + cosf(ph) * t2[j]);
            w[q] = 2.2f - 0.45f * (float)q;
            pu[q] = 0.3f * throb;
            kk[q] = k * 0.8f;
        }
        SvvTube((const float(*)[3])P, w, pu, kk, 4, 0, pal);
    }
    for (j = 0; j < 3; ++j) rc[0][j] = rc[1][j] = rc[2][j] = pal->body[j];
    rc[0][3] = 0.95f * k, rc[1][3] = 0.92f * k, rc[2][3] = 0.0f;
    SvvBlot(c, t1, t2, r, 16, seed, 0.1f, 0.15f, 0, 3, kR3, (const float(*)[4])rc, 1.5f);  // v3.28 CORE: 16 spokes, star
    // 0.35 -> 0.15 (round, not a 12-point polygon; also the halo ring and the light below)
    if (g_svvPvN + 13 * 3 + SVV_CORE_EST * g_svvLeft <= SVV_PV) {  // v3.23.0 (item 2): a crimson halo ring round the blot
        static const float kRH[3] = { 0.0f, 0.7f, 1.0f };
        for (j = 0; j < 3; ++j) rc[0][j] = rc[1][j] = rc[2][j] = pal->halo[j] * (1.0f + SVV_HALO_PULSE * throb);
        rc[0][3] = 0.8f * k, rc[1][3] = 0.6f * k, rc[2][3] = 0.0f;
        SvvBlot(c, t1, t2, 1.4f * r, 16, seed, 0.1f, 0.1f, 2, 3, kRH, (const float(*)[4])rc, 2.0f);
    } else {
        ++g_svvHaloSkip;
    }
    for (j = 0; j < 3; ++j) {  // the light: a black mouth, a maroon lip that flushes with each gulp, a dim rim
        rc[0][j] = 0.0f;
        rc[1][j] = pal->rim[j] * (1.8f + 1.0f * throb) + pal->core[j] * (0.20f + 0.3f * throb);
        rc[2][j] = pal->rim[j] * 0.6f;
        rc[3][j] = 0.0f;
    }
    rc[0][3] = rc[1][3] = rc[2][3] = k, rc[3][3] = 0.0f;
    SvvBlot(c, t1, t2, r, 16, seed, 0.1f, 0.15f, 1, 4, kR4, (const float(*)[4])rc, 1.5f);
}
// the latched bite (camera-facing): a dark lump with a dim glowing rim (v3.28 CORE: 18 spokes, lump 0.12: round; the
// nodules and drip beads that also used it are gone)
static void SvvKnot(const float p[3], float r, DWORD seed, float k, float pulse, const SvvPal *pal, int infect)
{
    static const float kR3[3] = { 0.0f, 0.75f, 1.0f }, kR4[4] = { 0.0f, 0.5f, 0.85f, 1.15f };
    float rc[4][4];
    int j;
    for (j = 0; j < 3; ++j) rc[0][j] = rc[1][j] = rc[2][j] = pal->body[j];
    rc[0][3] = 0.95f * k, rc[1][3] = 0.9f * k, rc[2][3] = 0.0f;
    SvvBlot(p, NULL, NULL, r, 18, seed, 0.12f, 0.0f, 0, 3, kR3, (const float(*)[4])rc, 1.5f);
    for (j = 0; j < 3; ++j) {
        rc[0][j] = pal->core[j] * (infect ? 0.3f + 0.6f * pulse : 0.1f + 0.5f * pulse);
        rc[1][j] = pal->rim[j] * (0.7f + 0.5f * pulse) + pal->wet[j] * 0.15f;
        rc[2][j] = pal->rim[j] * 0.45f;
        rc[3][j] = 0.0f;
    }
    rc[0][3] = rc[1][3] = rc[2][3] = k, rc[3][3] = 0.0f;
    SvvBlot(p, NULL, NULL, r, 18, seed, 0.12f, 0.0f, 1, 4, kR4, (const float(*)[4])rc, 1.5f);
}
static void SvvFlash(const float p[3], float r, float k, const float rgb[3], float minPx)  // additive glint
{
    static const float kR[2] = { 0.0f, 1.0f };
    float rc[2][4];
    int j;
    for (j = 0; j < 3; ++j) rc[0][j] = rgb[j], rc[1][j] = 0.0f;
    rc[0][3] = k, rc[1][3] = 0.0f;
    SvvBlot(p, NULL, NULL, r, 8, 0x51u, 0.0f, 1.0f, 1, 2, kR, (const float(*)[4])rc, minPx);
}
// ---- v3.28 CORE: the black heart
// a camera-facing round disc at the projected pc: 24 spokes with a slow 1.5-2 % wobble (round, never a polygon), K rings
// (radius rr[] x rpx, colour rc[][4]: rgb + strength), offset (ox, oy) pixels
static void SvvDisc(const float pc[4], float rpx, DWORD hh, int layer, int K, const float *rr, const float (*rc)[4], float ox, float oy)
{
    const float ph = 6.2831853f * SVV_R(hh, 5), ph2 = 6.2831853f * SVV_R(hh, 6);
    int j, m;
    if (!SvvBegin(K, layer, 25)) return;
    for (j = 0; j <= 24; ++j) {
        const float ang = 6.2831853f * (float)(j % 24) / 24.0f;
        const float f = 1.0f + 0.02f * sinf(3.0f * ang + 0.8f * g_svvNow + ph) + 0.015f * sinf(5.0f * ang - 1.3f * g_svvNow + ph2);
        for (m = 0; m < K; ++m)
            SvvV(pc[0] + ox + rpx * rr[m] * f * cosf(ang), pc[1] + oy + rpx * rr[m] * f * sinf(ang), pc,
                 SVV_PREMUL(layer) ? SvvPre(rc[m], rc[m][3]) : SvvAdd(rc[m], rc[m][3]));
    }
    SvvEnd();
}
// the heart of a haunting (built before its tendrils; drawn after all of them: layer 2 its crimson corona, layer 3 the
// black disc, layer 4 its rim light): swells in, beats, gulps a feeding pulse (S->gulp, last frame), shrinks with the
// reach, fades with the veins' strength; its blood records (SpiritGhost=Blood: drips at its lower rim, in front)
static void SvvCore(SvvSlot *S, const SvSrc *s, float now)
{
    static const float kRc[3] = { 0.0f, 1.0f, 2.1f }, kRb[4] = { 0.0f, 0.78f, 0.93f, 1.0f }, kRr[4] = { 0.74f, 0.88f, 0.96f, 1.02f };
    const DWORD hh = SvvHeartKey(s->key);
    const SvvPal *pal = &kSvvPal[s->pal == SVP_BILE];
    const float g = SvvBack((now - S->t0) / SVV_CORE_GROW_S, 1.6f), beat = SvvBeat(now + SVV_BEAT_S * SVV_R(hh, 4)), gulp = S->gulp;
    const float r = SVV_CORE_R * g * (0.55f + 0.45f * FxSat(s->reach)) * (1.0f + 0.12f * beat + 0.04f * gulp);
    const float k = s->kv * FxSat((now - S->t0) / 0.15f);
    float c[3], pc[4], fd, rpx, rc[4][4];
    int j, m;
    if (!(s->kv > 0.004f)) return;
    SvvHeart(s, now, c);
    for (m = 0; m < SVV_CORE_DRIPS; ++m) {  // the heart is the only wound: drips from its lower rim, in front of the body
        SvbRoot *rt = &g_sbRoot[S - g_svv][m];
        const float a = 6.2831853f * SVV_R(hh, 10 + m);
        float dv[3] = { 0.0f, 0.0f, 0.0f };
        if (g_svEyeOk) {
            for (j = 0; j < 3; ++j) dv[j] = g_svEye[j] - c[j];
            SvvNorm(dv);
        }
        for (j = 0; j < 3; ++j) rt->r0[j] = c[j] + dv[j] * SVV_CORE_PULL;
        rt->r0[0] += 0.4f * r * cosf(a), rt->r0[1] += 0.85f * r, rt->r0[2] += 0.4f * r * sinf(a);  // (y down)
        rt->stamp = g_fxFrame;
        rt->g = g;
        rt->k = k;
        rt->pulse = beat;
        rt->seed = FxHash(hh + (DWORD)m * 0x632BE5ABu);
        rt->pal = s->pal == SVP_BILE ? 1 : s->pal == SVP_GOLD ? 2 : 0;
        rt->core = 1;
    }
    if (!(r > 0.5f)) return;
    g_svvPull = SVV_CORE_PULL + r;  // the ghost's own chest must not hide it
    j = SvvProj(c, pc);
    g_svvPull = 0.0f;
    if (!j) return;
    fd = SvvFade(c, pc[3], r) * k;
    if (!(fd > 0.004f)) return;
    rpx = r * g_svvPx / pc[3];
    if (rpx < 2.5f * g_svUnit) rpx = 2.5f * g_svUnit;
    if (rpx > SVV_CORE_MAXH * (float)g_svVp->h) rpx = SVV_CORE_MAXH * (float)g_svVp->h;
    ++g_svvCores;
    g_svvCoresBone += s->nb > 9;
    for (j = 0; j < 3; ++j) rc[0][j] = rc[1][j] = rc[2][j] = pal->halo[j] * (1.6f + 2.2f * beat + 1.2f * gulp);  // the corona
    rc[0][3] = 0.6f * fd, rc[1][3] = fd, rc[2][3] = 0.0f;
    SvvDisc(pc, rpx, hh, 2, 3, kRc, (const float(*)[4])rc, 0.0f, 0.0f);
    for (j = 0; j < 3; ++j) {  // the disc: black, a near-black maroon ring, a dark crimson feathered edge
        rc[0][j] = 0.0f;
        rc[1][j] = pal->body[j];
        rc[2][j] = rc[3][j] = pal->body[j] + 0.30f * pal->rim[j];
    }
    // v3.29.1 (user, v3.28 test item 4: "fill in the black tendril orbs ... so that circle is like a black hole"): the
    // disc's fill is opaque black as soon as the heart shows (x 2.5, saturating), not the veins' (often low) strength: it
    // had read as a hollow ring; the crimson rim light stays as the glowing edge round a black hole
    {
        const float fo = FxSat(2.5f * fd);
        rc[0][3] = rc[1][3] = fo, rc[2][3] = 0.92f * fo, rc[3][3] = 0.0f;
    }
    SvvDisc(pc, rpx, hh, 3, 4, kRb, (const float(*)[4])rc, 0.0f, 0.0f);
    for (j = 0; j < 3; ++j) {  // the rim light: a thin crimson ring just inside the edge, flaring with each beat / gulp
        rc[0][j] = rc[3][j] = 0.0f;
        rc[1][j] = pal->rim[j] * (0.30f + 0.5f * beat);
        rc[2][j] = pal->rim[j] * (0.9f + 1.0f * beat + 0.6f * gulp) + pal->core[j] * 0.12f * beat;
    }
    rc[0][3] = rc[3][3] = 0.0f, rc[1][3] = rc[2][3] = fd;
    SvvDisc(pc, rpx, hh, 4, 4, kRr, (const float(*)[4])rc, 0.0f, 0.0f);
}

// ---- state: anchors, strain / snap / recoil, latching
static void SvvPath(const SvvGeo *G, float now, float (*P)[3], float *w, float *pu)
{
    int i;
    for (i = 0; i < SVV_N; ++i) {
        const float v = G->g * (float)i / (float)(SVV_N - 1), p = SvvPulse(G, v, now), q = 0.35f * G->tw * G->live;
        SvvAt(G, v, now, P[i]);
        w[i] = SvvWidth(G, v, now);
        pu[i] = p > q ? p : q;  // a clench flushes the whole strand
    }
}
static void SvvSnap(SvvSlot *S, const SvSrc *s, int ti, float now, int gold)  // the tendril tears: its piece recoils
{
    SvvTen *t = &S->t[ti];
    SvvGeo G;
    float pu[SVV_N];
    if (SvvSetup(S, s, ti, now, &G)) {
        SvvPath(&G, now, t->rcP, t->rcW, pu);
        t->rcN = SVV_N;
        t->rcT = now;
        memcpy(t->rcC, s->c, 12);
        t->rcGold = gold;
    }
    if (t->kind == 0 && t->st) {  // the sucker lets go of the wall (shrinks and fades)
        t->osOn = 1, t->osSer = t->ser, t->osT = now;
        memcpy(t->osA, t->a, 12), memcpy(t->osN, t->n, 12);
    }
    t->st = 0;
    t->kind = 0;
    ++t->ser;
    t->wantT = now;
    t->outT = -1.0f;
}
static void SvvAnchor(SvvSlot *S, const SvSrc *s, int ti, float now, const float pp[3], int pOk)  // one ray cast
{
    SvvTen *t = &S->t[ti];
    const DWORD hk = FxHash(s->key * 0x9E3779B9u ^ 0x5EEDu), ht = FxHash(hk + (DWORD)ti * 0x632BE5ABu);
    const DWORD ha = FxHash(ht ^ t->ser * 0x85EBCA6Bu);
    const float az = 6.2831853f * (kSvvCat[ti] ? SVV_R(ha, 1) : ((float)ti + 0.8f * SVV_R(ha, 1)) / 4.0f + SVV_R(hk, 9));
    float d[3], p1[3], hit[3], nrm[3];
    int j, got;
    if (kSvvCat[ti] == 0) d[0] = cosf(az), d[1] = -0.15f + 0.6f * SVV_R(ha, 2), d[2] = sinf(az);
    else if (kSvvCat[ti] == 1) d[0] = 0.55f * SVV_R(ha, 3) * cosf(az), d[1] = 1.0f, d[2] = 0.55f * SVV_R(ha, 3) * sinf(az);
    else d[0] = 0.5f * cosf(az), d[1] = -1.0f, d[2] = 0.5f * sinf(az);
    SvvNorm(d);
    for (j = 0; j < 3; ++j) p1[j] = s->c[j] + d[j] * SVV_RAY;
    ++g_svvCasts;
    got = SvvCast(s->c, p1, hit, nrm);
    if (!got && pOk && d[1] > 0.3f && s->c[1] < pp[1] && p1[1] >= pp[1]) {  // no floor in the query: her floor level
        const float u = (pp[1] - s->c[1]) / (p1[1] - s->c[1]);
        for (j = 0; j < 3; ++j) hit[j] = s->c[j] + d[j] * SVV_RAY * u;
        hit[1] -= SVV_OFF;
        nrm[0] = nrm[2] = 0.0f, nrm[1] = -1.0f;
        got = 1;
    }
    if (got) {
        memcpy(t->a, hit, 12), memcpy(t->n, nrm, 12);
        t->kind = 0;
        t->life = SVV_LIFE0 + SVV_LIFE1 * SVV_R(ha, 4);
    } else {
        for (j = 0; j < 3; ++j) t->a[j] = s->c[j] + d[j] * SVV_RAY * SVV_AIR;
        memcpy(t->n, d, 12);
        t->kind = 1;
        t->life = 2.5f + 1.5f * SVV_R(ha, 4);
    }
    t->st = 1;
    t->t0 = now;
}
// latchFree: how many more may latch onto her this frame (SVV_LATCH_ALL less those already on her, all hauntings)
static void SvvSlotStep(SvvSlot *S, const SvSrc *s, float now, const float pp[3], int pOk, int *castsLeft, int *latchFree)
{
    const DWORD hk = FxHash(s->key * 0x9E3779B9u ^ 0x5EEDu);
    const int gold = s->pal == SVP_GOLD, chestOk = (g_svvBOk >> 9 & 1u) != 0;
    const float range = s->R > SVV_LATCH_R ? s->R : SVV_LATCH_R;
    float dist = 1e9f;
    int ti, want = 0, latched = 0, casts = 0, used = 0;
    if (gold) S->goldT = now;
    if (s->reach <= 0.001f) {  // v3.23.0 (item 17): retracted into the lying ghost: nothing left out; the regrow starts clean
        for (ti = 0; ti < SVV_T; ++ti) {
            SvvTen *t = &S->t[ti];
            if (t->st && t->kind == 2) ++*latchFree;
            t->st = 0, t->kind = 0, ++t->ser, t->wantT = now, t->rcN = 0, t->osOn = 0, t->outT = -1.0f;
        }
        return;
    }
    if (chestOk) {
        const float dv[3] = { g_svvB[9][0] - s->c[0], g_svvB[9][1] - s->c[1], g_svvB[9][2] - s->c[2] };
        dist = SvvLen(dv);
        if (dist < range && !gold && now - S->goldT > SVV_WARD_HOLD && s->reach >= 1.0f) {  // v3.23.0: none while retracting
            want = 1 + (int)(2.0f * (1.0f - dist / range) + 0.5f);
            if (want > SVV_LATCH_MAX) want = SVV_LATCH_MAX;
        }
    }
    for (ti = 0; ti < SVV_T; ++ti) {
        SvvTen *t = &S->t[ti];
        if (t->st == 0) {
            if (s->reachT > 0.0f && now >= t->wantT && casts < SVV_CASTS_H && *castsLeft > 0) {  // (none while retracting)
                SvvAnchor(S, s, ti, now, pp, pOk);
                ++casts, --*castsLeft;
            }
            continue;
        }
        if (t->kind == 2) {
            SvvGeo G;
            if (gold) {  // the medallion: it lets go at once (gold sparks run along the piece)
                SvvSnap(S, s, ti, now, 1);
                ++*latchFree;
                continue;
            }
            if (!SvvSetup(S, s, ti, now, &G)) {  // her bones are gone
                t->st = 0, t->kind = 0, ++t->ser, t->wantT = now;
                ++*latchFree;
                continue;
            }
            if (dist > range * 1.1f) {
                if (t->outT < 0.0f) t->outT = now;
            } else {
                t->outT = -1.0f;
            }
            if (G.La > SVV_SNAP_X * t->llen || (t->outT >= 0.0f && now - t->outT > SVV_OUT_S)) {
                SvvSnap(S, s, ti, now, 0);
                ++*latchFree;
                continue;
            }
            ++latched;
            used |= 1 << t->bi;
            continue;
        }
        if (t->st == 1) {
            const float dv[3] = { t->a[0] - s->c[0], t->a[1] - s->c[1], t->a[2] - s->c[2] };
            if (now - t->t0 > t->life || SvvLen(dv) > SVV_FAR) t->st = 2, t->stT = now;
            else if (t->kind == 0 && ti == S->los % SVV_T && casts < SVV_CASTS_H && *castsLeft > 0 && now - t->t0 > SVV_GROW_S) {
                float hit[3], nrm[3];  // v3.23.0 (V322-R2): a wall now between the body and its anchor: strain, snap, re-anchor
                const float dl = SvvLen(dv);
                ++casts, --*castsLeft, ++g_svvCasts;
                if (SvvCast(s->c, t->a, hit, nrm)) {  // (a hit on the anchor's own surface, e.g. a lying ghost's ray
                    const float dh[3] = { hit[0] - s->c[0], hit[1] - s->c[1], hit[2] - s->c[2] };  // grazing its floor,
                    const float da[3] = { hit[0] - t->a[0], hit[1] - t->a[1], hit[2] - t->a[2] };  // does not count)
                    const int same = nrm[0] * t->n[0] + nrm[1] * t->n[1] + nrm[2] * t->n[2] > 0.95f &&
                                     fabsf(da[0] * t->n[0] + da[1] * t->n[1] + da[2] * t->n[2]) < 8.0f;
                    if (!same && SvvLen(dh) < 0.9f * dl) t->st = 2, t->stT = now, ++g_svvLosCut;
                }
            }
        } else if (t->st == 2 && now - t->stT >= SVV_STRAIN_S) {
            SvvSnap(S, s, ti, now, 0);
        }
    }
    ++S->los;  // V322-R2: the next tendril's line-of-sight check next frame
    if (latched < want && *latchFree > 0 && now - S->latchT >= SVV_LATCH_GAP) {  // one more re-targets to her
        const int start = (int)(FxHash(hk ^ 0x1A7Cu) % SVV_T), b0 = (int)(FxHash(hk ^ 0xB0E5u) % 5u);
        int q, best = -1, bi = -1;
        for (q = 0; q < SVV_T && best < 0; ++q) {
            const SvvTen *t = &S->t[(start + q) % SVV_T];
            if (t->st == 1 && t->kind != 2 && now - t->t0 > SVV_GROW_S) best = (start + q) % SVV_T;
        }
        for (q = 0; q < 5 && bi < 0; ++q) {  // a limb no other strand of this ghost holds, alternating sides
            const int b = kSvvLimbOrder[(b0 + q) % 5];
            if (!(used >> b & 1) && SvvLimbOk(b)) bi = b;
        }
        if (best >= 0 && bi >= 0) {
            SvvTen *t = &S->t[best];
            SvvGeo G;
            DWORD ha;
            SvvSnap(S, s, best, now, 0);  // tears from the wall ...
            ha = FxHash(FxHash(hk + (DWORD)best * 0x632BE5ABu) ^ t->ser * 0x85EBCA6Bu);
            t->kind = 2, t->st = 1, t->t0 = now, t->bi = bi;  // ... and shoots at her
            t->lu = bi ? 0.3f + 0.35f * SVV_R(ha, 5) : 0.15f + 0.3f * SVV_R(ha, 5);
            t->lth = 6.2831853f * SVV_R(ha, 6);
            t->llen = 1.0f;
            t->outT = -1.0f;
            if (SvvSetup(S, s, best, now, &G)) t->llen = G.La, --*latchFree;
            else t->st = 0, t->kind = 0;
            S->latchT = now;
        }
    }
}

// ---- building one tendril's strips (once a frame)
static void SvvBuild(SvvSlot *S, const SvSrc *s, int ti, float now)
{
    SvvTen *t = &S->t[ti];
    const DWORD hk = FxHash(s->key * 0x9E3779B9u ^ 0x5EEDu), ht = FxHash(hk + (DWORD)ti * 0x632BE5ABu);
    const SvvPal *pal = &kSvvPal[s->pal == SVP_BILE];
    float P[SVV_N][3], w[SVV_N], pu[SVV_N], kk[SVV_N];
    SvvGeo G;
    int i, j;
    if (t->osOn) {  // a released blot
        const float u = (now - t->osT) / SVV_SPLAT_FADE;
        if (u >= 1.0f || u < 0.0f) t->osOn = 0;
        else SvvSplat(t->osA, t->osN, FxHash(ht ^ t->osSer * 0x85EBCA6Bu) ^ 0x5B1Au, 1.0f - 0.3f * u, s->kv * (1.0f - u) * (1.0f - u), 0.0f, pal);
    }
    if (t->rcN && now - t->rcT >= 0.0f && now - t->rcT < SVV_RECOIL_S) {  // the snapped piece whips back into the ghost
        const float u = (now - t->rcT) / SVV_RECOIL_S, e = SvEase(u), fr = 1.0f - e;
        const SvvPal *rp = t->rcGold ? &kSvvPal[2] : pal;
        float sh[3], R[SVV_N][3], rw[SVV_N], rpu[SVV_N], rk[SVV_N], tip[3];
        int q;
        for (j = 0; j < 3; ++j) sh[j] = s->c[j] - t->rcC[j];  // follows the ghost
        for (i = 0; i < SVV_N; ++i) {
            const float x = (float)i / (float)(SVV_N - 1) * fr * (float)(SVV_N - 1);
            const int a = (int)x < SVV_N - 1 ? (int)x : SVV_N - 2;
            const float f = x - (float)a, wh = 18.0f * sinf(3.14159265f * u) * powf((float)i / (float)(SVV_N - 1), 1.5f) * sinf(16.0f * now + 0.6f * (float)i);
            for (j = 0; j < 3; ++j) R[i][j] = t->rcP[a][j] + (t->rcP[a + 1][j] - t->rcP[a][j]) * f + sh[j];
            R[i][1] += wh;
            R[i][0] += 0.6f * wh;
            rw[i] = (t->rcW[a] + (t->rcW[a + 1] - t->rcW[a]) * f) * (1.0f - 0.4f * e);
            rpu[i] = t->rcGold ? 0.6f : 0.0f;
            rk[i] = s->kv * (1.0f - 0.5f * e * e);
        }
        SvvTube((const float(*)[3])R, rw, rpu, rk, SVV_N, 1, rp);
        memcpy(tip, R[SVV_N - 1], 12);
        if (u < 0.4f) SvvFlash(tip, 12.0f * (1.0f + 1.5f * u), s->kv * (1.0f - u / 0.4f) * 0.8f, t->rcGold ? kSvvPal[2].hot : pal->core, 2.0f);
        if (t->rcGold)  // gold sparks running along the piece toward the ghost
            for (q = 0; q < 8; ++q) {
                const float sv = (SVV_R(ht ^ t->ser, 50 + q) - 2.5f * (now - t->rcT));
                const float x = (sv - floorf(sv)) * (float)(SVV_N - 1);
                const int a = (int)x < SVV_N - 1 ? (int)x : SVV_N - 2;
                float p[3];
                for (j = 0; j < 3; ++j) p[j] = R[a][j] + (R[a + 1][j] - R[a][j]) * (x - (float)a);
                SvvFlash(p, 8.0f + 6.0f * SVV_R(ht, 60 + q), s->kv / SV_REPEL_K * (0.6f + 0.4f * sinf(30.0f * now + (float)q)), kSvGold, 2.0f);
            }
    } else {
        t->rcN = 0;
    }
    if (!SvvSetup(S, s, ti, now, &G)) return;
    {   // v3.28 CORE: a feeding pulse reaching the heart makes it gulp (next frame; v3.26.0's per-root blood is the heart's)
        const float pg = SvvPulse(&G, 0.02f, now);
        if (pg > S->gulpAcc) S->gulpAcc = pg;
    }
    ++g_svvTen;
    g_svvLatched += t->kind == 2;
    SvvPath(&G, now, P, w, pu);
    for (i = 0; i < SVV_N; ++i) kk[i] = G.k * FxSmooth(0.0f, 0.09f, G.g * (float)i / (float)(SVV_N - 1));  // sinks into the body
    if (t->kind == 0 && G.g >= 0.85f) {  // the sucker blot: slaps on as the tip arrives, then clenches with each gulp
        const float ts = now - t->t0 - 0.85f * SVV_GROW_S;
        SvvSplat(t->a, t->n, FxHash(ht ^ t->ser * 0x85EBCA6Bu) ^ 0x5B1Au, ts > 0.0f ? SvvBack(ts / 0.2f, 2.2f) : 0.0f,
                 G.k * (1.0f - 0.6f * G.taut), SvvPulse(&G, 1.0f, now) > 0.5f * G.tw ? SvvPulse(&G, 1.0f, now) : 0.5f * G.tw, pal);
    }
    SvvTube((const float(*)[3])P, w, pu, kk, SVV_N, 1, pal);
    if (t->kind == 0) {  // side veins on the surface: 2, branching off the crawl, creeping over the wall / floor
        int b;
        for (b = 0; b < 2; ++b) {
            const float sb = 0.15f + 0.6f * SVV_R(ht, 31 + b), vb = G.va + (1.0f - G.va) * sb, gb = SvEase((G.g - vb) / 0.2f);
            const float side = (b ? 1.0f : -1.0f), len = (20.0f + 35.0f * SVV_R(ht, 37 + b)) * gb;
            float base[3], Q[4][3], bw[4], bp[4], bk[4];
            int q;
            if (!(gb > 0.05f)) continue;
            SvvAt(&G, vb, now, base);
            for (q = 0; q < 4; ++q) {
                const float f = (float)q / 3.0f, kink = 0.25f * len * sinf(2.1f * (float)q + 6.2831853f * SVV_R(ht, 40 + b));
                for (j = 0; j < 3; ++j)
                    Q[q][j] = base[j] + (G.wd[j] * side * 0.9f - G.u[j] * 0.45f) * len * f + (G.u[j] + G.wd[j] * 0.3f) * kink * f;
                bw[q] = 2.0f - 1.2f * f;
                bp[q] = SvvPulse(&G, vb + 0.06f * f, now);
                bk[q] = G.k * (1.0f - 0.6f * G.taut);
            }
            SvvTube((const float(*)[3])Q, bw, bp, bk, 4, 0, pal);
        }
    }
    // (v3.28 CORE: no slime drips and no knotted nodules on the air spans any more: the vein centres stay clean, user
    // v3.27.3 item 1; the v3.27.3 root clots are gone too: every root is under the heart's disc)
    if (t->kind == 2 && G.g >= 1.0f) {  // latched: a dark bite at the coil, black veins creeping along her limb
        const float tl = now - t->t0, pe = SvvPulse(&G, 1.0f, now), sp = 36.0f * FxSat((tl - 0.4f) / 1.5f);
        float p[3], dv[3];
        int sgn, q;
        for (j = 0; j < 3; ++j) dv[j] = g_svEye[j] - G.L0[j];
        SvvNorm(dv);
        for (j = 0; j < 3; ++j) p[j] = G.L0[j] + dv[j] * G.rw;
        SvvKnot(p, (0.3f * G.rw + 3.0f) * (1.0f + 0.25f * pe) * FxSat(tl / 0.3f), ht ^ 0x1F3Cu, G.k, pe, pal, 1);
        if (sp > 2.0f)
            for (sgn = -1; sgn <= 1; sgn += 2) {
                float Q[4][3], bw[4], bp[4], bk[4];
                for (q = 0; q < 4; ++q) {
                    const float f = (float)q / 3.0f, th = G.th0 + 0.5f * sinf((float)q + 6.2831853f * SVV_R(ht, 70 + sgn));
                    for (j = 0; j < 3; ++j)
                        Q[q][j] = G.L0[j] + G.ax[j] * (float)sgn * sp * f + 0.95f * G.rw * (cosf(th) * G.e1[j] + sinf(th) * G.e2[j]);
                    bw[q] = 2.2f - 1.4f * f;
                    bp[q] = SvvPulse(&G, 1.0f - 0.1f * f, now);
                    bk[q] = G.k;
                }
                SvvTube((const float(*)[3])Q, bw, bp, bk, 4, 0, pal);
            }
    }
}

static void SvVeinsFrame(float now, float dt)
{
    static int s_logged;
    static float s_t0 = -1.0f;
    const int paused = SvVeinsPaused();
    DWORD rk = g_svvRoom;
    float pp[3];
    int i, j, nh = 0, castsLeft = SVV_CASTS, pOk = 0, latchFree = SVV_LATCH_ALL;
    (void)dt;  // closed-form motion: real seconds only
    __try {
        rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_svvPvN = g_svvStrN = g_svvDrop = g_svvCasts = g_svvLatched = g_svvTen = g_svvHaloSkip = g_svvCores = g_svvCoresBone = 0;
    g_svvLeft = 0;  // v3.23.0: tendrils still to be built this frame (the halo reserve): the hauntings the loop below will
    for (i = 0; i < g_svSrcN && g_svvLeft < SVV_H * SVV_T; ++i)  // take (same filters), SVV_T each (inspection SH4-323-R1)
        if (g_svSrc[i].kind == SVS_HAUNT && g_svSrc[i].key && !g_svSrc[i].noVeins) g_svvLeft += SVV_T;
    g_svvStamp = g_fxFrame;
    g_svvNow = now;
    if (rk != g_svvRoom) {  // a new stage / room: every tendril starts clean
        g_svvRoom = rk;
        memset(g_svv, 0, sizeof g_svv);
        s_logged = 0;
        s_t0 = -1.0f;
    }
    if (!g_svVp) return;
    g_svvPx = 0.5f * (g_svPxX + g_svPxY);
    __try {
        memcpy(pp, (const void *)0x10A0F60, 12);
        pOk = _finite(pp[0] + pp[1] + pp[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        pOk = 0;
    }
    if (pOk) SvvBones(pp);
    else g_svvBOk = 0;
    for (i = 0; i < SVV_H; ++i)  // strands on her now (all hauntings): at most SVV_LATCH_ALL
        for (j = 0; j < SVV_T; ++j) latchFree -= g_svv[i].key && g_svv[i].t[j].st && g_svv[i].t[j].kind == 2;
    for (i = 0; i < g_svSrcN && nh < SVV_H; ++i) {  // the nearest hauntings (g_svSrc is sorted nearest first)
        const SvSrc *s = &g_svSrc[i];
        SvvSlot *S = NULL;
        if (s->kind != SVS_HAUNT || !s->key) continue;
        if (s->noVeins) {  // v3.23.0 (review SH4-323-01 / 05): a matched ghost without this frame's skeleton: no tendrils
            ++g_svvNoSkel;
            continue;
        }
        ++nh;
        for (j = 0; j < SVV_H && !S; ++j)
            if (g_svv[j].key == s->key) S = &g_svv[j];
        if (S && now - S->lastT > 1.0f) {  // away a while: grows anew
            memset(S, 0, sizeof *S);
            S->key = s->key;
        }
        if (!S) {  // a free slot, else the one unseen longest
            float old = 3.0e38f;
            for (j = 0; j < SVV_H; ++j) {
                if (!g_svv[j].key) {
                    S = &g_svv[j];
                    break;
                }
                if (g_svv[j].lastT < now && g_svv[j].lastT < old) old = g_svv[j].lastT, S = &g_svv[j];
            }
            if (!S) continue;
            memset(S, 0, sizeof *S);
            S->key = s->key;
        }
        if (!S->t0) {
            const DWORD hk = FxHash(s->key * 0x9E3779B9u ^ 0x5EEDu);
            S->t0 = now;
            S->latchT = S->goldT = -100.0f;
            for (j = 0; j < SVV_T; ++j) S->t[j].wantT = now + 0.15f * (float)j + 0.35f * SVV_R(FxHash(hk + (DWORD)j * 0x632BE5ABu), 41);
        }
        S->lastT = now;
        if (!paused) SvvSlotStep(S, s, now, pp, pOk, &castsLeft, &latchFree);  // paused (G3): anchors and casts frozen
        SvvCore(S, s, now);  // v3.28 CORE: the heart first (its layers draw after the tendrils' anyway: never starved)
        S->gulpAcc = 0.0f;
        for (j = 0; j < SVV_T; ++j) {
            --g_svvLeft;
            SvvBuild(S, s, j, now);
        }
        S->gulp = S->gulpAcc;
    }
    g_svvCastsLeft = castsLeft;  // v3.26.0 (INSP326-003): what the falling blood drops may still cast this frame
    if (!s_logged && g_svvTen) {  // once a room, 3 s after the first tendril
        if (s_t0 < 0.0f) {
            s_t0 = now;
        } else if (now - s_t0 >= 3.0f) {
            s_logged = 1;
            Log("screen effects: Spirit Vision veins: %d haunting(s), %d heart(s) (%d on a chest bone), %d tendril(s) (%d latched), "
                "%d strip(s), %d of %d vertices%s; %d halo(s) skipped for room; so far %d anchor(s) let go behind a new wall, "
                "%d ghost frame(s) without a skeleton",
                nh, g_svvCores, g_svvCoresBone, g_svvTen, g_svvLatched, g_svvStrN, g_svvPvN, SVV_PV,
                g_svvDrop ? " (over the budget: some dropped)" : "", g_svvHaloSkip, g_svvLosCut, g_svvNoSkel);
        }
    }
}

// ================================================================ v3.26.0 (round 1): the blood-rooted ghost (PLAN 2.2 / 2.3)
// Spirit Vision with [ScreenEffects] SpiritGhost=Blood: a crimson aura on every ghost's bones (the edge the ramp in the
// glow shader cannot see), and at each tendril root a wound that seeps, drips down the body and drops blood on the
// floor. The aura uses real seconds (g_svvNow); the blood its own clock g_sbNow (reviews SVR-006 / 102 / 106): it
// advances by the full real time between two CONSECUTIVE frames that run the aura block (g_fxFrame + 1: any frame rate,
// hitches included) and by nothing across frames that did not run it (pause, menus, cut-scenes, loads; the aura block
// runs only in gameplay phase 0), so the blood stops while paused and resumes where it was. Drips are closed-form in
// their slot time; a falling drop casts once.
#define SVB_AURA_K     1.60f    // the aura: kSvvPal halo x this
#define SVB_AURA_R0    14.0f    // a bone's sprite radius: 0.75 x the nearest other bone, within R0 .. R1
#define SVB_AURA_R1    38.0f    // (renders: 46 made the torso haze too uniform)
#define SVB_AURA_N     8        // ghosts with an aura: the nearest to the player (INSP326-012: a bounded budget)
#define SVB_ENDS       6        // the bones farthest from the centroid (hands, feet, head) ...
#define SVB_END_R      1.25f    // ... bigger ...
#define SVB_END_K      1.70f    // ... and brighter: "crimson toward the limbs" (renders: 1.4 hardly showed)
#define SVB_GROWN      0.30f    // a root bleeds once its tendril is this far grown
#define SVB_DRIP_P     0.80f    // a drip slot (1.5 .. 4 s per root) carries a drip with this chance (renders: 0.6 too rare)
#define SVB_FALL_G     980.0f   // world units / s^2 (+y is down)
#define SVB_FALL_MAX   600.0f   // no floor within this: the drop fades out
#define SVB_FALLS      128      // drip records (a fall, then a 'done' marker kept 4.5 s so its slot never re-starts it)
#define SVB_FLOOR      32       // floor blots (oldest replaced)
#define SVB_FLOOR_S    4.0f     // a floor blot fades over this
#define SVB_CASTS      4        // ray casts a frame for falling drops
static const float kSvbBlood[3] = { 0.19f, 0.006f, 0.024f };  // a dark crimson (the mock-up: near-black read as ink)
static const float kSvbFloor[3] = { 0.22f, 0.010f, 0.030f };  // floor blots: a little lighter (renders: lost on the dark floor)
typedef struct { DWORD key; float p[3], t0, fy, hit[3], n[3], k; int st, pal; } SvbFall;  // st: 0 needs a cast, 1 floor,
// 3 done (landed / no floor / timed out: kept as a marker until its drip slot is over, INSP326-006)
// at the surface fy (hit = the blot's point, SVV_OFF off it), 2 no floor; key 0 = a free record (INSP326-001: falls live
// on their own, apart from the root and its drip slot)
typedef struct { DWORD key; float p[3], n[3], t0; int pal; } SvbBlot;
static SvbFall g_sbFall[SVB_FALLS];
static SvbBlot g_sbFloor[SVB_FLOOR];
static int g_sbFallN, g_sbFloorN;
static float g_sbNow, g_sbLast = -1.0f;
static LONG g_sbFrame = -100;
static DWORD g_sbRoom = 0xFFFFFFFFu;
static void SvbAura(float now)
{
    int i, b, c, n, pi, pick[SVB_AURA_N], np = 0;
    float pd[SVB_AURA_N], pp[3] = { 0, 0, 0 };
    __try {
        memcpy(pp, (const void *)0x10A0F60, 12);  // the player (the nearest ghosts get an aura)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    for (i = 0; g_gvStamp == g_fxFrame && i < g_gvN; ++i) {  // choose the SVB_AURA_N nearest live actor ghosts
        const GvEnt *e = &g_gv[i];
        float wp[3] = { 0, 0, 0 }, d;
        int ok = 0, j;
        if (e->kind != GVK_ACTOR || !GvValid(e)) continue;
        __try {
            const BYTE *w = *(const BYTE *const *)(e->task + 0x408);
            if (w) memcpy(wp, w, 12), ok = _finite(wp[0] + wp[1] + wp[2]);  // INSP326-009: the work position at +0 (SvWorkPos)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok = 0;
        }
        if (!ok) continue;
        d = (wp[0] - pp[0]) * (wp[0] - pp[0]) + (wp[2] - pp[2]) * (wp[2] - pp[2]);
        if (np < SVB_AURA_N) {
            pick[np] = i, pd[np++] = d;
        } else {
            for (j = 0, c = 1; c < SVB_AURA_N; ++c)
                if (pd[c] > pd[j]) j = c;
            if (d < pd[j]) pick[j] = i, pd[j] = d;
        }
    }
    for (pi = 0; pi < np; ++pi) {
        const GvEnt *e = &g_gv[pick[pi]];
        float B[SV_BONES][3], cen[3] = { 0, 0, 0 }, dc[SV_BONES], k = 1.0f, wp[3];
        int endRank[SV_BONES], q;
        n = 0;
        __try {
            const BYTE *m = e->model, *skel = *(const BYTE *const *)(m + 0xE0), *w = *(const BYTE *const *)(e->task + 0x408);
            const float *mats = *(const float *const *)(m + 0xC0);
            int nb = SV_BONES;
            if (mats && w) memcpy(wp, w, 12);  // the work position at +0, as SvWorkPos / SvTrackBones (INSP326-009)
            if (!mats || !w || !_finite(wp[0] + wp[1] + wp[2])) nb = 0;
            else if (skel) nb = *(const int *)(skel + 0x14);
            if (nb > SV_BONES) nb = SV_BONES;
            for (b = 0; b < nb; ++b) {
                const float *qq = mats + 16 * b + 12;
                if (!_finite(qq[0] + qq[1] + qq[2]) || fabsf(qq[0] - wp[0]) > 400.0f || fabsf(qq[2] - wp[2]) > 400.0f ||
                    fabsf(qq[1] - wp[1]) > 600.0f)
                    continue;
                memcpy(B[n++], qq, 12);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            n = 0;
        }
        if (n < 3) continue;  // no bones this frame: no aura (never a floating centre)
        for (b = 0; b < n; ++b) cen[0] += B[b][0], cen[1] += B[b][1], cen[2] += B[b][2];
        cen[0] /= (float)n, cen[1] /= (float)n, cen[2] /= (float)n;
        for (q = 0; q < g_svSrcN; ++q)  // the veins' envelope of ITS haunting (INSP326-011: matched by the ghost task)
            if (g_svSrc[q].kind == SVS_HAUNT && g_svSrc[q].ghost == e->task) {
                k = g_svSrc[q].kv;
                break;
            }
        if (!(k > 0.01f)) continue;
        for (b = 0; b < n; ++b) {
            const float dx = B[b][0] - cen[0], dy = B[b][1] - cen[1], dz = B[b][2] - cen[2];
            dc[b] = dx * dx + dy * dy + dz * dz;
        }
        for (b = 0; b < n; ++b) {  // rank by distance from the centroid: the SVB_ENDS farthest are the limb ends
            endRank[b] = 0;
            for (c = 0; c < n; ++c) endRank[b] += dc[c] > dc[b];
        }
        for (b = 0; b < n; ++b) {
            const DWORD hb = FxHash((DWORD)(ULONG_PTR)e->task * 0x9E3779B9u + (DWORD)b * 0x632BE5ABu);
            float nn = 1.0e9f, r, a, rgb[3];
            const int end = endRank[b] < SVB_ENDS;
            int j;
            for (c = 0; c < n; ++c) {
                float d;
                if (c == b) continue;
                d = (B[c][0] - B[b][0]) * (B[c][0] - B[b][0]) + (B[c][1] - B[b][1]) * (B[c][1] - B[b][1]) + (B[c][2] - B[b][2]) * (B[c][2] - B[b][2]);
                if (d < nn) nn = d;
            }
            r = 0.75f * sqrtf(nn);
            r = !(r >= SVB_AURA_R0) ? SVB_AURA_R0 : r > SVB_AURA_R1 ? SVB_AURA_R1 : r;
            if (end) r *= SVB_END_R;
            a = SVB_AURA_K * k * (end ? SVB_END_K : 1.0f) * (0.85f + 0.15f * sinf((0.6f + 0.5f * FxRnd(hb)) * now + 6.2831853f * FxRnd(hb ^ 1u)));
            for (j = 0; j < 3; ++j) rgb[j] = kSvvPal[0].halo[j];
            SvSprAdd(B[b], r, 0.0f, SVC_SOFT, 1, rgb, a, 2.0f * g_svUnit);
        }
    }
}
static void SvbFloorAdd(DWORD key, const float p[3], const float n[3], float t0, int pal)
{
    int i;
    for (i = 0; i < g_sbFloorN; ++i)
        if (g_sbFloor[i].key == key) return;
    if (g_sbFloorN < SVB_FLOOR) {
        i = g_sbFloorN++;
    } else {  // the oldest goes
        int j;
        for (i = 0, j = 1; j < SVB_FLOOR; ++j)
            if (g_sbFloor[j].t0 < g_sbFloor[i].t0) i = j;
    }
    g_sbFloor[i].key = key;
    memcpy(g_sbFloor[i].p, p, 12);
    memcpy(g_sbFloor[i].n, n, 12);
    g_sbFloor[i].t0 = t0;
    g_sbFloor[i].pal = pal;
}
#define SVB_DONE_KEEP  4.5f     // a done record is kept this long after its detach (a drip slot is at most 4 s)
static void SvbFallStart(DWORD key, const float p[3], float t0, float k, int pal, float T)  // a detach: recorded once, then
{                                                                                    // advanced by SvbFalls
    int i, f = -1;
    if (T - t0 > 1.2f) return;  // detached longer ago than any fall lasts (a long hitch): nothing to show, nothing cast
    for (i = 0; i < SVB_FALLS; ++i) {
        if (g_sbFall[i].key == key) return;  // already falling, or done (one cast per drip)
        if (f < 0 && (!g_sbFall[i].key || (g_sbFall[i].st == 3 && T - g_sbFall[i].t0 > SVB_DONE_KEEP))) f = i;
    }
    if (f < 0) return;  // all records busy: no new fall (none is cut short)
    memset(&g_sbFall[f], 0, sizeof g_sbFall[f]);
    g_sbFall[f].key = key;
    memcpy(g_sbFall[f].p, p, 12);
    g_sbFall[f].t0 = t0;
    g_sbFall[f].k = k;
    g_sbFall[f].pal = pal;
}
// every live fall, each frame: one cast (from the veins' remaining budget, at most SVB_CASTS), the free fall, the
// impact (a floor blot) or the end (no floor: fades over 0.7 s; still no cast after 1.2 s: gone)
static void SvbFalls(float T, int *casts, int paused)
{
    int i;
    for (i = 0; i < SVB_FALLS; ++i) {
        SvbFall *f = &g_sbFall[i];
        const SvvPal *pal;
        float tf, y, p[3];
        if (!f->key || f->st == 3) continue;
        pal = &kSvvPal[f->pal];
        tf = T - f->t0;
        if (tf < 0.0f) tf = 0.0f;
        if (f->st == 0 && !paused && *casts > 0) {  // one cast straight down (+y)
            float p1[3], hit[3], nrm[3];
            --*casts;
            memcpy(p1, f->p, 12);
            p1[1] += SVB_FALL_MAX;
            if (SvvCastT(f->p, p1, hit, nrm, 0.0005f) && nrm[1] < -0.5f) {  // SVR-202: near floors too
                f->st = 1;
                memcpy(f->hit, hit, 12);
                memcpy(f->n, nrm, 12);
                f->fy = hit[1] - nrm[1] * SVV_OFF;  // INSP326-002: the surface itself (the hit is SVV_OFF toward the drop)
            } else {
                f->st = 2;
            }
        }
        y = f->p[1] + 0.5f * SVB_FALL_G * tf * tf;
        if (f->st == 1 && y >= f->fy) {  // landed (a floor at or above the detach point: at once)
            const float d = f->fy - f->p[1];
            SvbFloorAdd(f->key, f->hit, f->n, f->t0 + (d > 0.0f ? sqrtf(2.0f * d / SVB_FALL_G) : 0.0f), f->pal);
            f->st = 3;  // done: the marker stays (its slot must not start it again)
            continue;
        }
        if ((f->st == 2 && tf > 0.7f) || tf > 1.2f) {
            f->st = 3;
            continue;
        }
        memcpy(p, f->p, 12);
        p[1] = y;
        SvSprAdd(p, 2.6f, 0.0f, SVC_BLOB, 0, f->pal ? pal->body : kSvbBlood, 0.8f * f->k * (f->st == 2 ? 1.0f - tf / 0.7f : 1.0f),
                 1.0f * g_svUnit);
    }
}
static void SvbRootBlood(SvbRoot *rt, float T)
{
    const SvvPal *pal = &kSvvPal[rt->pal];
    const float k = rt->k * FxSat((rt->g - SVB_GROWN) / 0.2f), th = rt->pulse;
    const DWORD hr = rt->seed ^ 0xB100Du;
    // INSP326-015: a drip starts its fall only if its detach time was crossed while this same root was bleeding in the
    // previous frame too (cont), so a root that just appeared, came back or regrew never drops blood that never ran
    const int cont = rt->bFrame == g_fxFrame - 1 && rt->bSeed == rt->seed && rt->bOk;
    const float prevT = rt->bT;
    float p[3], core[3], soft[3];
    int j;
    rt->bFrame = g_fxFrame, rt->bSeed = rt->seed, rt->bT = T, rt->bOk = k > 0.01f;
    if (!(k > 0.01f)) return;
    for (j = 0; j < 3; ++j) core[j] = pal->rim[j] * 0.45f * (1.0f + 0.6f * th), soft[j] = pal->rim[j] * 0.6f;
    if (!rt->core) {  // the seep: a dark crimson wound with a red core, a soft crimson glow under it, a wet glint
        SvSprAdd(rt->r0, 1.6f * SVV_W0, 0.0f, SVC_SOFT, 1, soft, k, 0.0f);  // (renders: 2.2 drowned the wound)
        SvSprAdd(rt->r0, 1.1f * SVV_W0 * (1.0f + 0.12f * th), 6.2831853f * SVV_R(hr, 1), SVC_BLOB, 0, rt->pal ? pal->body : kSvbBlood, 0.80f * k, 1.5f * g_svUnit);
        SvSprAdd(rt->r0, 0.66f * SVV_W0, 6.2831853f * SVV_R(hr, 2), SVC_BLOB, 1, core, k, 1.0f * g_svUnit);
        memcpy(p, rt->r0, 12);
        p[1] -= 0.35f * SVV_W0;  // up is -y
        SvSprAdd(p, 3.0f, 0.0f, SVC_SOFT, 1, pal->wet, 0.8f * k, 1.0f * g_svUnit);
    }  // (v3.28 CORE: a heart's record: the heart is the wound, only its drips)
    {   // the drips: hashed slots of 1.5 .. 4 s on the blood clock; closed form in the slot time
        const float per = 1.5f + 2.5f * SVV_R(hr, 3), off = per * SVV_R(hr, 4), tt = T + off;
        const float sl = floorf(tt / per), tau = tt - sl * per;
        const DWORD hs = FxHash(hr ^ (DWORD)(int)sl * 0x2C1B3C6Du);
        const float dr = 0.6f + 0.5f * FxRnd(hs ^ 1u), L = 20.0f + 40.0f * FxRnd(hs ^ 2u);  // INSP326-010: dr <= 1.1 < the
        // shortest slot 1.5 s, so a drip always detaches inside its slot (a >= 0.4 s window; a frame gap longer than that is a stall)
        float u, head[3];
        int q;
        for (q = 0; q <= 2; ++q) {  // INSP326-013 / 015: this slot and the two before: a detach crossed since the last pass
            const DWORD hp = FxHash(hr ^ (DWORD)((int)sl - q) * 0x2C1B3C6Du);  // starts its fall (from the detach time;
            const float drp = 0.6f + 0.5f * FxRnd(hp ^ 1u), Lp = 20.0f + 40.0f * FxRnd(hp ^ 2u);  // the point is today's
            const float td = (sl - (float)q) * per - off + drp;                                   // root + its run)
            if (cont && FxRnd(hp) < SVB_DRIP_P && td > prevT && td <= T) {
                for (j = 0; j < 3; ++j) head[j] = rt->r0[j];
                head[1] += Lp;
                SvbFallStart(hp | 1u, head, td, k, rt->pal, T);
            }
        }
        if (FxRnd(hs) >= SVB_DRIP_P) return;
        u = tau / dr;
        if (u < 1.0f) {  // running down the body: a chain of 5 shrinking blobs, tail fading
            const float e = u * u;
            for (q = 0; q < 5; ++q) {
                const float f = (float)q / 4.0f, sz = (9.0f - 5.0f * f) * (0.8f + 0.4f * FxRnd(hs ^ (10u + (DWORD)q)));
                float c[3], cc[3];
                for (j = 0; j < 3; ++j) c[j] = rt->r0[j];
                c[1] += L * e * f;
                c[0] += 2.5f * (FxRnd(hs ^ (20u + (DWORD)q)) - 0.5f) * 2.0f;  // sideways wobble (not a straight bar)
                c[2] += 2.5f * (FxRnd(hs ^ (30u + (DWORD)q)) - 0.5f) * 2.0f;
                SvSprAdd(c, sz, 6.2831853f * FxRnd(hs ^ (40u + (DWORD)q)), SVC_BLOB, 0, rt->pal ? pal->body : kSvbBlood,
                         0.75f * k * (0.4f + 0.6f * f), 1.0f * g_svUnit);
                for (j = 0; j < 3; ++j) cc[j] = pal->rim[j] * 0.35f;
                SvSprAdd(c, 0.5f * sz, 0.0f, SVC_BLOB, 1, cc, k, 0.0f);
            }
            for (j = 0; j < 3; ++j) head[j] = rt->r0[j];
            head[1] += L * e;
            SvSprAdd(head, 2.0f, 0.0f, SVC_SOFT, 1, pal->wet, 0.9f * k, 1.0f * g_svUnit);
            return;
        }
        (void)L;  // detached: the loop above started its fall when the detach was crossed; SvbFalls has it (INSP326-001)
    }
}
static void SvBloodFrame(float now)
{
    const int paused = SvVeinsPaused();  // (v3.22's flag: the aura block never runs from a paused frame, so it stays 0)
    const int cont = g_fxFrame == g_sbFrame + 1;  // the previous frame ran the aura block too
    float d = cont && g_sbLast >= 0.0f ? now - g_sbLast : 0.0f;
    int i, j, casts = g_svvCastsLeft < SVB_CASTS ? g_svvCastsLeft : SVB_CASTS;  // INSP326-003: the veins' remainder
    g_sbLast = now;
    g_sbFrame = g_fxFrame;
    if (!g_gvBloodOn) return;  // review SVR-105: White = the v3.23.0 look, none of the blood-rooted extras
    if (!(d >= 0.0f) || !_finite(d)) d = 0.0f;
    if (d > 2.0f) d = 2.0f;  // a single frame this long is a stall, not play: at most 2 s of blood time
    g_sbNow += d;
    if (g_svvRoom != g_sbRoom) {  // a new room: no blood carried over
        g_sbRoom = g_svvRoom;
        g_sbFallN = g_sbFloorN = 0;
        memset(g_sbFall, 0, sizeof g_sbFall);
    }
    SvbAura(now);
    for (i = 0; i < SVV_H; ++i)
        for (j = 0; j < SVV_T; ++j)
            if (g_sbRoot[i][j].stamp == g_fxFrame) SvbRootBlood(&g_sbRoot[i][j], g_sbNow);
    SvbFalls(g_sbNow, &casts, paused);
    for (i = 0; i < g_sbFloorN; ++i) {  // floor blots: a flattened dark crimson splash with a crimson rim, fading
        const SvbBlot *b = &g_sbFloor[i];
        const float age = g_sbNow - b->t0, f = 1.0f - age / SVB_FLOOR_S;
        float t1[3], t2[3], rc[3][4];
        static const float kR[3] = { 0.0f, 0.75f, 1.0f }, kRH[3] = { 0.0f, 0.6f, 1.0f };
        const SvvPal *pal = &kSvvPal[b->pal];
        int c;
        if (age < 0.0f || !(f > 0.0f)) continue;
        SvvPerp(t1, t2, b->n);  // review SVR-104: in the floor's plane (the cast's normal), SVV_OFF above it
        for (c = 0; c < 3; ++c) rc[0][c] = rc[1][c] = rc[2][c] = b->pal ? pal->body[c] : kSvbFloor[c];
        rc[0][3] = 0.8f * f, rc[1][3] = 0.7f * f, rc[2][3] = 0.0f;
        SvvBlot(b->p, t1, t2, (9.0f + 5.0f * FxRnd(b->key)) * (0.5f + 0.5f * FxSat(age / 0.15f)), 10, b->key, 0.25f, 0.3f, 0, 3, kR, (const float(*)[4])rc, 1.0f);
        for (c = 0; c < 3; ++c) rc[0][c] = rc[1][c] = rc[2][c] = pal->halo[c] * (2.0f + 2.0f * (1.0f - FxSat(age / 0.4f)));
        rc[0][3] = 0.6f * f, rc[1][3] = 0.4f * f, rc[2][3] = 0.0f;
        SvvBlot(b->p, t1, t2, 13.0f, 10, b->key ^ 7u, 0.2f, 0.2f, 2, 3, kRH, (const float(*)[4])rc, 1.5f);
    }
}

// each depth pass, after SvAuraDraw (SvUntextured's states, FVF XYZRHW | DIFFUSE): the bodies (premultiplied), then the
// light (additive, BLENDOP MAX: where strands cross the brightest wins, they do not add up); indexed from the pre-built
// rows. Leaves SvUntextured's states (and the blend op as found).
typedef HRESULT(STDMETHODCALLTYPE *SvvDrawIUP_t)(void *, DWORD, UINT, UINT, UINT, const void *, DWORD, const void *, UINT);
#define SVV_DV 4096
#define SVV_DI 12288
static GvVtx g_svvDv[SVV_DV];
static WORD g_svvDi[SVV_DI];
static int g_svvDn, g_svvIn;
static int SvvFlush(void *dev)
{
    const int tri = g_svvIn / 3;
    if (tri) {  // host (v3.22.0 integration): DrawIndexedPrimitiveUP clears the bound index buffer (and stream 0, which
        void *ib = NULL;  // FxSave / FxRestore of the pass put back): the game's next indexed draw must find its own
        UINT base = 0;
        const int haveIb = SUCCEEDED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base));  // AddRef'd on success
        ((SvvDrawIUP_t)VT(dev, 73 /*DrawIndexedPrimitiveUP*/))(dev, 4 /*TRIANGLELIST*/, 0, (UINT)g_svvDn, (UINT)tri, g_svvDi,
                                                                101 /*D3DFMT_INDEX16*/, g_svvDv, sizeof(GvVtx));
        if (haveIb) {
            ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
            if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        }
    }
    g_svvDn = g_svvIn = 0;
    return tri;
}
static int SvVeinsDraw(void *dev)
{
    const int m = (int)(FxSat(g_svMul) * 256.0f + 0.5f);
    DWORD op = 1 /*ADD*/;
    int layer, lp, i, r, c, drawn = 0, haveOp;
    if (!g_svvStrN || g_svvStamp != g_fxFrame || !g_svVp) return 0;
    haveOp = SUCCEEDED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 171 /*BLENDOP*/, &op));
    g_svvDn = g_svvIn = 0;
    for (lp = 0; lp < 5; ++lp) {  // v3.23.0: the halo (layer 2, additive) first, then the bodies, then the light; v3.28 CORE:
        static const int kLayer[5] = { 2, 0, 1, 3, 4 };  // then the hearts (3 premultiplied, 4 their rim light, MAX)
        layer = kLayer[lp];
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, SVV_PREMUL(layer) ? 6 /*INVSRCALPHA*/ : 2 /*ONE*/);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 171 /*BLENDOP*/, layer == 1 || layer == 4 ? 4 /*MAX*/ : 1 /*ADD*/);
        for (i = 0; i < g_svvStrN; ++i) {
            const SvvStr *s = &g_svvStr[i];
            const int cnt = s->rows * s->K, base = g_svvDn;
            if (s->layer != layer) continue;
            if (g_svvDn + cnt > SVV_DV || g_svvIn + (s->rows - 1) * (s->K - 1) * 6 > SVV_DI) {
                drawn += SvvFlush(dev);
                if (cnt > SVV_DV) continue;
            }
            for (r = 0; r < cnt; ++r) {
                const GvVtx *a = &g_svvPv[s->first + r];
                GvVtx *o = &g_svvDv[g_svvDn + r];
                const DWORD col = a->c;
                *o = *a;
                if (!g_svZ) o->z = 0.0f, o->rhw = 1.0f;
                if (m < 256)
                    o->c = (DWORD)((((col >> 24) & 0xFF) * m) >> 8) << 24 | (DWORD)((((col >> 16) & 0xFF) * m) >> 8) << 16 |
                           (DWORD)((((col >> 8) & 0xFF) * m) >> 8) << 8 | (DWORD)(((col & 0xFF) * m) >> 8);
            }
            g_svvDn += cnt;
            for (r = 0; r + 1 < s->rows; ++r)
                for (c = 0; c + 1 < s->K; ++c) {
                    const int i0 = r * s->K + c, i1 = i0 + 1, i2 = i0 + s->K, i3 = i2 + 1;
                    const GvVtx *v = &g_svvPv[s->first];
                    if (v[i0].z < 0.0f || v[i1].z < 0.0f || v[i2].z < 0.0f || v[i3].z < 0.0f) continue;
                    if (!(v[i0].c | v[i1].c | v[i2].c | v[i3].c) || (!SVV_PREMUL(layer) && !((v[i0].c | v[i1].c | v[i2].c | v[i3].c) & 0xFFFFFF))) continue;
                    g_svvDi[g_svvIn++] = (WORD)(base + i0), g_svvDi[g_svvIn++] = (WORD)(base + i1), g_svvDi[g_svvIn++] = (WORD)(base + i2);
                    g_svvDi[g_svvIn++] = (WORD)(base + i1), g_svvDi[g_svvIn++] = (WORD)(base + i3), g_svvDi[g_svvIn++] = (WORD)(base + i2);
                }
        }
        drawn += SvvFlush(dev);
    }
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 2 /*ONE*/);  // SvUntextured's
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 171 /*BLENDOP*/, haveOp ? op : 1 /*ADD*/);
    return drawn;
}
#undef SVV_R
