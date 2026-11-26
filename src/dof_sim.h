// v3.43 DF343: depth of field by depth slices, the pure logic (no Windows, no game memory; test\dof_sim_test.c).
// dof.inc does the device work. staging/v343_feedback/w4_dof/PLAN.md.
//
// The idea (USER_FEEDBACK_v342 / v341: "Refined native depth slices"): right after the 3D scene and the game's own
// cut-scene depth of field (0x566AE0) and before the light grid (0x433F00 at 0x4EFBDC) writes its 2D depth over the
// screen, the stored depth is still the scene's: z = P22 + P32 / w (reversed: near 1, far 0; P = cam+0xA0 = 0x12E3ED0,
// w = the view depth). Full-screen quads at the z of chosen view depths (the slices), depth-tested, colour off, write a
// code into the top four stencil bits where the scene is farther (far slices, ZFUNC GREATER, drawn near to far: the
// farthest passed slice wins) or nearer (near slices, ZFUNC LESS, drawn far to near). Then a blurred copy of the scene
// (half / quarter / eighth size) is drawn where each code is (stencil EQUAL): more blur farther from the focus.
// The focus F is the player's view depth (cut-scenes: the scene's own focus distance when its camera track has one, else
// the player); the slices sit at F x a ratio, so the fall-off follows the focus.
// v3.47 DF347: the blur eases in: thin slices a quarter level apart, the next level blended over the one below (below,
// "slices and codes"); far and near are marked and composed one after the other. staging/v347_feedback/w2_dof/PLAN.md.
#ifndef DOF_SIM_H
#define DOF_SIM_H
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------- settings
enum { DOF_OFF, DOF_CUT, DOF_ALWAYS, DOF_MODE_N };
static const char *const kDofModeName[DOF_MODE_N] = { "Off", "Cut-scenes", "Always" };
// v3.45 QS345: Low is new and gentler; Medium = v3.43 / v3.44's Low (the same levels); High unchanged
enum { DOFS_LOW, DOFS_MEDIUM, DOFS_HIGH, DOFS_N };
static const char *const kDofStrName[DOFS_N] = { "Low", "Medium", "High" };
#define DOF_STR_VER 345  // v3.45 QS345: the hidden [ScreenEffects] DofStrengthV marker (absent / lower: a v3.43 / v3.44 value)
#define DOF_STR_VER_S "345"

static int DofEqI(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return *a == *b;
}
// [ScreenEffects] DepthOfField: Off / Cut-scenes / Always (also 0 / 1 / 2, "Cutscenes", "Cut scenes", "On" = Always);
// -1 = not a value (the caller keeps Off and logs it)
static int DofParseMode(const char *s)
{
    if (!s || !s[0]) return -1;
    if (DofEqI(s, "Off") || DofEqI(s, "0") || DofEqI(s, "None")) return DOF_OFF;
    if (DofEqI(s, "Cut-scenes") || DofEqI(s, "Cutscenes") || DofEqI(s, "Cut scenes") || DofEqI(s, "1")) return DOF_CUT;
    if (DofEqI(s, "Always") || DofEqI(s, "On") || DofEqI(s, "2")) return DOF_ALWAYS;
    return -1;
}
// [ScreenEffects] DofStrength: Low / Medium / High (0 / 1 / 2). v3.45 QS345: legacy = 1 (no DofStrengthV marker: a
// value v3.43 / v3.44 wrote, when there were only Low / High = 0 / 1): their Low (0) is v3.45's Medium, their 1 High.
static int DofParseStrength(const char *s, int legacy)
{
    if (!s || !s[0]) return -1;
    if (DofEqI(s, "Low") || DofEqI(s, "0")) return legacy ? DOFS_MEDIUM : DOFS_LOW;
    if (DofEqI(s, "Medium") || DofEqI(s, "Med")) return DOFS_MEDIUM;
    if (DofEqI(s, "1")) return legacy ? DOFS_HIGH : DOFS_MEDIUM;
    if (DofEqI(s, "High") || DofEqI(s, "2")) return DOFS_HIGH;
    return -1;
}
// v3.45 QS345: the one-time migration. s = the ini text, marker = [ScreenEffects] DofStrengthV (0 = absent). Returns the
// strength (-1 = not a value) and *rewrite = 1 when the line must be written as kDofStrName[result] (a legacy value whose
// text is not already that name: "Low", "0" -> "Medium", "1" -> "High"), *mark = 1 when the marker must be written.
// v3.45 QS345-03 (Codex inspection 3): how a chosen strength is written. Medium and High read the same with or without
// the DofStrengthV marker; only "Low" differs (legacy = Medium, v3.45 = the new Low). DOFW_VALUE: the marker is on disk,
// write the value only. DOFW_VALUE_MARK: an unambiguous value first, then the marker (a failure of either leaves a valid,
// same-meaning ini). DOFW_MARK_VALUE: a new Low, the marker first, then the value; if the value fails the marker is taken
// back, so the value still on disk keeps the meaning it had.
enum { DOFW_VALUE, DOFW_VALUE_MARK, DOFW_MARK_VALUE };
static int DofStrWriteOrder(int v, int markedOnDisk)
{
    return markedOnDisk ? DOFW_VALUE : v == DOFS_LOW ? DOFW_MARK_VALUE : DOFW_VALUE_MARK;
}
static int DofStrMigrate(const char *s, int marker, int *rewrite, int *mark)
{
    const int legacy = marker < DOF_STR_VER, v = DofParseStrength(s, legacy);
    *rewrite = legacy && v >= 0 && !DofEqI(s, kDofStrName[v]);
    *mark = legacy;
    return v;
}

// ---------------------------------------------------------------- slices and codes (v3.47 DF347: the blur eases in)
// v3.43 .. v3.46 gave each stencil code a whole blur level, so the blur began with an edge where the first blurred slice
// lay (Medium: sharp to half size at once at F x 1.6) and stepped again at each next level. v3.47 DF347 (USER_FEEDBACK_v346
// 2: "the DoF should be eased into, but currently has a hard starting point"): every level is reached through four thin
// slices a quarter level apart; the composite draws the level below opaque and blends the next one over it at 25 / 50 /
// 75 % (bloom's proven up-step shader kPsPpUp, SRCALPHA blend), so no step anywhere is more than a quarter level, near
// and far, at Low / Medium / High. Each ramp is centred (in log distance) on v3.46's step: about the same blur, eased in.
// The distances lie on a grid of 1/24 octave: a slice at grid g sits at the view depth F x 2^(g / 24) (far g > 0, near
// g < 0). The far side and the near side are marked and composed one after the other (each up to 15 codes in the top
// four stencil bits, cleared in between). Code c (1..n) of a side: the pixel lies beyond that side's c-th slice (far:
// farther, near: nearer) and not beyond the next; its blur is c quarter levels. Code 0: in focus (no slice passed). The
// game's own low stencil bits (shadow volumes) are never touched.
#define DOF_GRID 24              // grid steps an octave
#define DOF_Q 4                  // codes a level (quarter levels)
#define DOF_SIDE_N 15            // codes a side (the top four stencil bits: 1..15)
#define DOF_LEVELS 3             // 1 half size, 2 quarter size, 3 eighth size (drawn back up from a quarter-size step)
#define DOF_STENCIL_SHIFT 4
#define DOF_STENCIL_MASK 0xF0u
enum { DOF_FAR, DOF_NEAR, DOF_SIDES };
static const char *const kDofSideName[DOF_SIDES] = { "far", "near" };
typedef struct { int n; signed char g[DOF_SIDE_N]; } DofRampT;  // a side's slices (grid units), from the focus outwards
static const DofRampT kDofRamp[DOFS_N][DOF_SIDES] = {
    /* Low, half size only (v3.46: at once at F x 2.2 far, 0.5 near) */
    { { 4, { 22, 25, 28, 31 } }, { 4, { -20, -23, -26, -29 } } },
    /* Medium, half then quarter size (v3.46: 1.6 and 3.2 far, 0.5 near) */
    { { 8, { 12, 15, 18, 21, 35, 38, 41, 44 } }, { 4, { -20, -23, -26, -29 } } },
    /* High, up to an eighth (v3.46: 1.25, 1.6, 2.2 far; 0.7, 0.5 near) */
    { { 12, { 4, 7, 10, 13, 14, 16, 18, 20, 24, 26, 28, 30 } }, { 8, { -8, -11, -14, -17, -20, -23, -26, -29 } } },
};
static float DofGridR(int g) { return powf(2.0f, (float)g / (float)DOF_GRID); }  // the ratio w / F of grid g
static int DofRampN(int strength, int side)
{
    return strength >= 0 && strength < DOFS_N && side >= 0 && side < DOF_SIDES ? kDofRamp[strength][side].n : 0;
}
// a code's blur in quarter levels (0: in focus, or not a code of that side)
static int DofCodeQ(int strength, int side, int code) { return code >= 1 && code <= DofRampN(strength, side) ? code : 0; }
// the whole level drawn opaque under a blur of q quarters: blend 1 = the level below (the next one is blended over it);
// blend 0 (the blend shader could not be made) = the nearest whole level (v3.46-like steps, centred on the ramps)
static int DofLevelLo(int q, int blend)
{
    const int L = q <= 0 ? 0 : blend ? q / DOF_Q : (q + DOF_Q / 2) / DOF_Q;
    return L > DOF_LEVELS ? DOF_LEVELS : L;
}
// the view depth of a side's code (0 = not a code)
static float DofSliceW(float F, int strength, int side, int code)
{
    if (!(F > 0.0f) || !DofCodeQ(strength, side, code)) return 0.0f;
    return F * DofGridR(kDofRamp[strength][side].g[code - 1]);
}
// the stored depth of view depth w (KB 147: z = P22 + P32 / w)
static float DofDepthAt(float w, float p22, float p32) { return w > 0.0f ? p22 + p32 / w : 0.0f; }
// the projection's z column is usable (0x41CB60: P22 = -n / (f - n) < 0, P32 = f n / (f - n) > 0)
static int DofProjOk(float p22, float p32)
{
    return p22 == p22 && p32 == p32 && p22 < 0.0f && p22 > -1.0f && p32 > 0.0f && p32 < 1.0e6f;
}

typedef struct { int code, g; float w, z; } DofSlice;
typedef struct {
    int n;                       // slices to mark, in drawing order (from the focus outwards: the last one passed wins)
    DofSlice s[DOF_SIDE_N];
    unsigned codes;              // bit c: code c can be in the stencil after the marking (bit 0: code 0)
    int maxQ;                    // the largest blur (quarters) a kept code has
} DofSideT;
typedef struct {
    DofSideT side[DOF_SIDES];    // far, then near
    int n;                       // slices of both sides
    int maxLevel;                // the deepest level drawn (1..3)
    unsigned levels;             // bit L: level L is drawn (opaque or blended)
} DofPlanT;
static void DofPlanSum(int strength, DofPlanT *pl)
{
    int sd, c;
    pl->n = pl->maxLevel = 0, pl->levels = 0;
    for (sd = 0; sd < DOF_SIDES; ++sd) {
        DofSideT *s = &pl->side[sd];
        s->maxQ = 0;
        pl->n += s->n;
        for (c = 1; c <= DOF_SIDE_N; ++c)
            if (s->codes >> c & 1u) {
                const int q = DofCodeQ(strength, sd, c), lo = DofLevelLo(q, 1), hi = q % DOF_Q && lo < DOF_LEVELS ? lo + 1 : lo;
                if (q > s->maxQ) s->maxQ = q;
                if (lo) pl->levels |= 1u << lo;
                if (hi) pl->levels |= 1u << hi;
                if (hi > pl->maxLevel) pl->maxLevel = hi;
            }
    }
}
// The slices for focus F (view units), strength, the projection's z column. A slice is kept only when its z lies strictly
// inside (0, 1) (between the far and the near plane: one past the far plane has nothing behind it, one in front of the
// near plane nothing in front of it). 0 = no plan (nothing to mark).
static int DofMakePlan(float F, float p22, float p32, int strength, DofPlanT *pl)
{
    int sd, c;
    memset(pl, 0, sizeof *pl);
    if (!(F > 0.0f) || !DofProjOk(p22, p32) || strength < 0 || strength >= DOFS_N) return 0;
    for (sd = 0; sd < DOF_SIDES; ++sd) {
        DofSideT *s = &pl->side[sd];
        s->codes = 1u;  // code 0 (nothing marked)
        for (c = 1; c <= DofRampN(strength, sd); ++c) {
            const float w = DofSliceW(F, strength, sd, c), z = DofDepthAt(w, p22, p32);
            if (z > 0.0f && z < 1.0f) {
                DofSlice *t = &s->s[s->n++];
                t->code = c, t->g = kDofRamp[strength][sd].g[c - 1], t->w = w, t->z = z;
                s->codes |= 1u << c;
            }
        }
    }
    DofPlanSum(strength, pl);
    return pl->n > 0;
}
// Every code of a strength, no depths (the page's preview: the depth map already holds the depths, DofBandTest).
static void DofPlanCodes(int strength, DofPlanT *pl)
{
    int sd, c;
    memset(pl, 0, sizeof *pl);
    if (strength < 0 || strength >= DOFS_N) return;
    for (sd = 0; sd < DOF_SIDES; ++sd) {
        DofSideT *s = &pl->side[sd];
        s->codes = 1u;
        for (c = 1; c <= DofRampN(strength, sd); ++c) {
            DofSlice *t = &s->s[s->n++];
            t->code = c, t->g = kDofRamp[strength][sd].g[c - 1];
            s->codes |= 1u << c;
        }
    }
    DofPlanSum(strength, pl);
}
// The code a pixel ends with after one side's marking (the emulation of the stencil, for the test): its slices in order
// (far: ZFUNC GREATER, slice z > stored z = the scene is farther; near: ZFUNC LESS), REPLACE: the last one passed.
static int DofEmulateCode(const DofSideT *s, int side, float storedZ)
{
    int i, c = 0;
    for (i = 0; i < s->n; ++i)
        if (side == DOF_FAR ? s->s[i].z > storedZ : s->s[i].z < storedZ) c = s->s[i].code;
    return c;
}

// The composite of one side, as draws (dof.inc DofCompose runs them in order): each whole level L opaque over every code
// from the first one whose blur reaches it (stencil LESSEQUAL: ref <= stored; the codes rise with the blur), then, for
// each code between two levels, the next level blended over at the code's fraction (stencil EQUAL). A pixel of blur q
// quarters ends as lerp(level q / 4, level q / 4 + 1, (q % 4) / 4), level 0 = the sharp scene. blend 0: the opaque draws
// only, each at the nearest whole level.
enum { DOFC_EQUAL, DOFC_FROM };
typedef struct { int level, func, code; float a; } DofOp;  // a: the blend weight (1 = opaque, no blend)
#define DOF_OPS_MAX (DOF_LEVELS + DOF_SIDE_N)
static int DofComposeOps(const DofSideT *s, int strength, int side, int blend, DofOp *op)
{
    int n = 0, L, c;
    for (L = 1; L <= DOF_LEVELS; ++L) {
        for (c = 1; c <= DOF_SIDE_N; ++c)
            if ((s->codes >> c & 1u) && DofLevelLo(DofCodeQ(strength, side, c), blend) >= L) break;
        if (c <= DOF_SIDE_N) op[n].level = L, op[n].func = DOFC_FROM, op[n].code = c, op[n].a = 1.0f, ++n;
    }
    if (blend)
        for (c = 1; c <= DOF_SIDE_N; ++c) {
            const int q = DofCodeQ(strength, side, c);
            if (!(s->codes >> c & 1u) || !(q % DOF_Q) || q / DOF_Q >= DOF_LEVELS) continue;
            op[n].level = q / DOF_Q + 1, op[n].func = DOFC_EQUAL, op[n].code = c, op[n].a = (float)(q % DOF_Q) / (float)DOF_Q, ++n;
        }
    return n;
}
// A frame's draws for a plan (after the copy and the blur chain's 4 passes): the stencil clears (one at the start, one
// between the sides, one at the end), the slices, the composite's draws. *comp = the composite's draws.
static int DofFrameDraws(const DofPlanT *pl, int strength, int blend, int *comp)
{
    DofOp op[DOF_OPS_MAX];
    int sd, n = 1, c = 0, sides = 0;
    for (sd = 0; sd < DOF_SIDES; ++sd)
        if (pl->side[sd].n) {
            n += pl->side[sd].n + (sides++ ? 1 : 0);
            c += DofComposeOps(&pl->side[sd], strength, sd, blend, op);
        }
    if (comp) *comp = c;
    return n + c + 1;
}

// ---------------------------------------------------------------- the depth map (the page's preview, v3.47 DF347)
// The pause's depth map holds, as alpha, a depth index independent of the strength: the count of the band grid's slices
// (every strength's slices, near and far, sorted) the scene is farther than, x 8. Any strength's code is then marked from
// it by an alpha test (far slice u: alpha >= (u + 1) x 8; near slice u: alpha <= u x 8), drawn in the side's order, so a
// strength changed on the page while paused still shows right.
#define DOF_BAND_MAX 31          // band slices the map can hold (alpha up to 248)
#define DOF_BAND_UNIT 8u
static int DofBandGrid(int *g)   // the sorted, distinct grid of every strength's slices (both sides); its count
{
    int s, sd, c, n = 0, i, j;
    for (s = 0; s < DOFS_N; ++s)
        for (sd = 0; sd < DOF_SIDES; ++sd)
            for (c = 0; c < kDofRamp[s][sd].n; ++c) {
                const int v = kDofRamp[s][sd].g[c];
                for (i = 0; i < n && g[i] < v; ++i) {}
                if (i < n && g[i] == v) continue;
                if (n >= DOF_BAND_MAX) return -1;  // (the test keeps this from happening)
                for (j = n; j > i; --j) g[j] = g[j - 1];
                g[i] = v, ++n;
            }
    return n;
}
static int DofBandIndex(int g)   // a slice's place in the band grid (-1: not in it)
{
    int b[DOF_BAND_MAX], n = DofBandGrid(b), i;
    for (i = 0; i < n; ++i)
        if (b[i] == g) return i;
    return -1;
}
static unsigned DofBandAlpha(int k) { return k <= 0 ? 0u : (unsigned)k * DOF_BAND_UNIT; }  // k slices passed
typedef struct { int n; DofSlice s[DOF_BAND_MAX]; } DofSliceList;
// The band grid's slices for focus F, near to far, each drawn (ZFUNC GREATER) with alpha (u + 1) x 8: the farthest one
// the scene lies behind wins. One past the far plane: not drawn (nothing is farther, as DofMakePlan drops it); one in
// front of the near plane: drawn at z 1 (everything is farther, as DofMakePlan's near side never marks in front of it).
static int DofMakeBandList(float F, float p22, float p32, DofSliceList *bl)
{
    int b[DOF_BAND_MAX], n, u;
    memset(bl, 0, sizeof *bl);
    n = DofBandGrid(b);
    if (!(F > 0.0f) || !DofProjOk(p22, p32) || n <= 0) return 0;
    for (u = 0; u < n; ++u) {
        const float w = F * DofGridR(b[u]);
        float z = DofDepthAt(w, p22, p32);
        if (!(z > 0.0f)) continue;
        if (z > 1.0f) z = 1.0f;
        bl->s[bl->n].code = u + 1, bl->s[bl->n].g = b[u], bl->s[bl->n].w = w, bl->s[bl->n].z = z;
        ++bl->n;
    }
    return bl->n > 0;
}
// the alpha test marking a slice of `side` from the map: D3DCMP 7 GREATEREQUAL (far) or 4 LESSEQUAL (near), *ref. The
// map's alphas are multiples of 8: the reference sits half-way between two of them (far (u + 1) x 8 - 4, near u x 8 + 4),
// so a backend that rounds the sampled alpha by a few levels still marks the same pixels.
enum { DOFA_LESSEQUAL = 4, DOFA_GREATEREQUAL = 7 };
static int DofBandTest(int side, int g, unsigned *ref)
{
    const int u = DofBandIndex(g);
    if (u < 0) return *ref = 0, 0;
    if (side == DOF_FAR) return *ref = DofBandAlpha(u + 1) - DOF_BAND_UNIT / 2u, DOFA_GREATEREQUAL;
    return *ref = DofBandAlpha(u) + DOF_BAND_UNIT / 2u, DOFA_LESSEQUAL;
}
static int DofBandEmulate(const DofSideT *s, int side, unsigned alpha)  // the code the map marks (the test)
{
    int i, c = 0;
    for (i = 0; i < s->n; ++i) {
        unsigned ref;
        const int f = DofBandTest(side, s->s[i].g, &ref);
        if ((f == DOFA_GREATEREQUAL && alpha >= ref) || (f == DOFA_LESSEQUAL && alpha <= ref)) c = s->s[i].code;
    }
    return c;
}
// the card's sample depth (preview.dds: the floor plane, far at the top, the focus in the lower middle, near at the
// bottom): the grid g at a height y / h (0 top .. 1 bottom), and the map's index there
static float DofCardG(float y)
{
    static const float kY[4] = { 0.0f, 0.52f, 0.70f, 1.0f }, kG[4] = { 52.0f, 2.0f, -3.0f, -34.0f };
    int i;
    if (!(y > 0.0f)) return kG[0];
    for (i = 1; i < 4; ++i)
        if (y <= kY[i]) return kG[i - 1] + (kG[i] - kG[i - 1]) * (y - kY[i - 1]) / (kY[i] - kY[i - 1]);
    return kG[3];
}
static int DofCardBand(float y)
{
    int b[DOF_BAND_MAX], n = DofBandGrid(b), i, k = 0;
    const float g = DofCardG(y);
    for (i = 0; i < n; ++i) k += (float)b[i] < g;
    return k;
}

// ---------------------------------------------------------------- the stencil guard (DF343-001)
// Once the first stencil write of a frame (or of the preview) may have happened, our top bits must be cleared on every
// exit: a failed mark / composite and a fault included (restoring the device state does not restore stencil contents).
// Begin before the first write; End says whether the cleanup must run now (once: a second End, or one after a fault in
// the cleanup itself, does nothing).
typedef struct { int dirty, begun, cleaned; } DofStGuard;
static void DofStBegin(DofStGuard *g) { g->dirty = 1, ++g->begun; }
static int DofStEnd(DofStGuard *g)
{
    if (!g->dirty) return 0;
    g->dirty = 0;
    ++g->cleaned;
    return 1;
}

// ---------------------------------------------------------------- the restore plan (DF343-001b)
// A path that changed the render target, viewport or device states puts them back on every exit, a structured exception
// included, exactly once, in this order: the saved target (and its depth-stencil) set back; on it our stencil top bits
// cleared if a write may have happened; the saved states (FxRestore + the stencil / alpha-test states); the saved
// viewport; the target references released. The flags are cleared by the plan: a second call does nothing.
enum { DOFR_TARGET, DOFR_CLEAN, DOFR_STATE, DOFR_VIEWPORT, DOFR_RELEASE, DOFR_N };
typedef struct { int saved, rsSaved, held, dirty; } DofRestoreIn;  // held = a target reference is kept
static int DofRestorePlan(DofRestoreIn *f, int steps[DOFR_N])
{
    int n = 0;
    if (f->held) steps[n++] = DOFR_TARGET;
    if (f->held && f->dirty) steps[n++] = DOFR_CLEAN;
    if (f->saved || f->rsSaved) steps[n++] = DOFR_STATE;
    if (f->saved) steps[n++] = DOFR_VIEWPORT;
    if (f->held) steps[n++] = DOFR_RELEASE;
    f->saved = f->rsSaved = f->held = f->dirty = 0;
    return n;
}

// ---------------------------------------------------------------- the focus
#define DOF_F_MIN 200.0f        // the focus is never nearer (the slices would crowd the camera)
#define DOF_F_MAX 20000.0f
// v3.43 .. v3.51: the player nearer than DOF_F_NOFOCUS was taken for a first-person view: no focus, no blur. v3.52 FX352
// (USER_FEEDBACK_v351 D3: 10639 frames of "nearer than 150" were close third-person cameras, e.g. the Subway World's,
// the companion Eileen filling the frame): first person is the game's own state only (the gate: Room 302's camera
// CameraFp, stages 1 / 0x0A by the camera dispatch 0x4FF900; a view of another place: the room's geometry off). A close
// player (0 < w <= DOF_F_NOFOCUS) is DOFF_CLOSE: the focus on her (DofFocusStep clamps it to DOF_F_MIN), the far side
// only (nothing between her and the near plane 50 is blurred: DofPlanFarOnly). A cut-scene's own focus still needs
// more than DOF_F_NOFOCUS (its small values are not proven to be distances).
#define DOF_F_NOFOCUS 150.0f    // v3.52 FX352: the player nearer than this: close to the camera (DOFF_CLOSE), not first person
#define DOF_F_TAU 0.25f         // s: the focus follows in log space (no pumping on a camera cut: a 3x jump snaps)
static float DofClampF(float f) { return f < DOF_F_MIN ? DOF_F_MIN : f > DOF_F_MAX ? DOF_F_MAX : f; }
static float DofFocusStep(float cur, float target, float dt, int snap)
{
    float a;
    target = DofClampF(target);
    if (snap || !(cur > 0.0f) || !(dt >= 0.0f) || target > 3.0f * cur || cur > 3.0f * target) return target;
    if (dt > 1.0f) dt = 1.0f;
    a = 1.0f - expf(-dt / DOF_F_TAU);
    if (!(a > 0.0f)) return cur;
    return expf(logf(cur) + (logf(target) - logf(cur)) * a);
}

// ---------------------------------------------------------------- the gate
enum { DOFG_ON, DOFG_OFF, DOFG_NOTPLAY, DOFG_PHASE, DOFG_GAMEPLAY, DOFG_VIEW, DOFG_FP, DOFG_N };
static const char *const kDofGateWhy[DOFG_N] = {
    "on", "Off", "not in play (menus, FMV, loading)", "paused or an overlay (phase 2+)",
    "in play (DepthOfField = Cut-scenes)", "a view of another place (peephole, window: the room's geometry is off)",
    "Room 302's first-person view (no focus)" };
// mode, the running game (engine mode 6 / sub-mode 2), the game phase [0xFD5C20] (0 play, 1 cut-scene, 2 paused),
// the room's geometry drawn ([0xFD6C78]), Room 302 (stages 1 / 0x0A: its play is first person, the player is the camera)
// v3.52 FX352: these are the game's only first-person states in play: the camera task 0x4FF900 (and the start 0x4FF8D0)
// runs CameraFpUpdate 0x500C10 exactly when gGame.stage is 1 or 10, CameraWorldUpdate 0x5025D0 otherwise; the peepholes,
// the door scope, the wall hole and the window look-outs turn the room's geometry off (DOFG_VIEW).
static int DofIsRoom302(unsigned stage) { return stage == 1u || stage == 0x0Au; }
static int DofGate(int mode, int gameplay, int phase, int roomDrawn, int room302)
{
    if (mode <= DOF_OFF || mode >= DOF_MODE_N) return DOFG_OFF;
    if (!gameplay) return DOFG_NOTPLAY;
    if (phase != 0 && phase != 1) return DOFG_PHASE;
    if (phase == 0 && mode == DOF_CUT) return DOFG_GAMEPLAY;
    if (!roomDrawn) return DOFG_VIEW;
    if (phase == 0 && room302) return DOFG_FP;
    return DOFG_ON;
}
// the focus source in this frame (v3.52 FX352: + DOFF_CLOSE)
enum { DOFF_NONE, DOFF_PLAYER, DOFF_SCENE, DOFF_CLOSE, DOFF_N };
static const char *const kDofFocusName[DOFF_N] = { "none", "the player", "the cut-scene's own focus",
                                                   "the player close to the camera" };
// cut-scene: the scene's own focus distance (0x566A10's centre, [0x137FA04], when its camera track turns the game's
// depth of field on: [0x137FA1C] sides and [0x137FA10] planes both set), else the player; play: the player. *F out.
// v3.52 FX352: a player in front of the camera but nearer than DOF_F_NOFOCUS is DOFF_CLOSE (was DOFF_NONE: no blur).
static int DofPickFocus(int phase, int sceneOn, float sceneF, int playerOk, float playerW, float *F)
{
    if (phase == 1 && sceneOn && sceneF > DOF_F_NOFOCUS && sceneF == sceneF && sceneF < 1.0e6f) {
        *F = sceneF;
        return DOFF_SCENE;
    }
    if (playerOk && playerW > DOF_F_NOFOCUS && playerW == playerW && playerW < 1.0e6f) {
        *F = playerW;
        return DOFF_PLAYER;
    }
    if (playerOk && playerW > 0.0f && playerW <= DOF_F_NOFOCUS) {  // v3.52 FX352 (NaN fails both tests)
        *F = playerW;
        return DOFF_CLOSE;
    }
    *F = 0.0f;
    return DOFF_NONE;
}
// v3.52 FX352: the same subject (the player, at any distance): no snap of the focus and no "focus" log line between
// DOFF_PLAYER and DOFF_CLOSE (a camera that swings close and back would otherwise snap and fill the log)
static int DofSameSubject(int a, int b)
{
    return a == b || (a == DOFF_PLAYER && b == DOFF_CLOSE) || (a == DOFF_CLOSE && b == DOFF_PLAYER);
}
// v3.52 FX352: a close player's plan: the far side only (the near side's slices lie between her and the near plane, or
// on her; DofMakePlan's near side would blur her). 1 = something is left to draw.
static int DofPlanFarOnly(DofPlanT *pl, int strength)
{
    memset(&pl->side[DOF_NEAR], 0, sizeof pl->side[DOF_NEAR]);
    pl->side[DOF_NEAR].codes = 1u;  // code 0 only (nothing marked)
    DofPlanSum(strength, pl);
    return pl->n > 0;
}

// ---------------------------------------------------------------- v3.44 DF344: why a frame was not drawn
// v3.43 drew in 0 of 16593 asked frames and said why only once ("focus none"): every frame the effect is asked for now
// gets one reason, counted per room (the room line lists them), and the first of each kind in a room is logged. The
// gate's by-design skips (paused, Cut-scenes in play, a view of another place, Room 302) are counted apart.
// v3.52 FX352: DOFN_NEAR ("the player is nearer than 150 (a first-person view)") is gone: a close player is a focus now
enum { DOFN_DRAWN, DOFN_PHASE, DOFN_CUTMODE, DOFN_VIEW, DOFN_FP, DOFN_FAILED, DOFN_NOTBACK, DOFN_NOVP, DOFN_READ, DOFN_BEHIND,
       DOFN_OFFSCREEN, DOFN_PROBEWAIT, DOFN_PROBEOFF, DOFN_NOPLAN, DOFN_NOSTATE, DOFN_NODS, DOFN_NORT, DOFN_N };
#define DOFN_DESIGN_END DOFN_FP  // DOFN_PHASE .. DOFN_FP: by design (the gate); after it: a refusal
static const char *const kDofWhyNot[DOFN_N] = {
    "drawn", "paused or an overlay", "in play (DepthOfField = Cut-scenes)", "a view of another place (peephole or window)",
    "Room 302's first-person view", "off this session", "not drawing to the back buffer", "no viewport",
    "the player's position could not be projected", "the player is behind the camera or too far",
    "the player is off screen (a cut-scene without its own focus)",
    "the room's depth probe is pending", "the room's depth probe turned it off", "no slice between the near and far planes",
    "a device state could not be read", "no depth-stencil surface", "our render targets are not ready" };
// the gate's verdict as a reason (-1: Off or not in play: not counted)
static int DofGateWhyNot(int gate)
{
    switch (gate) {
    case DOFG_ON: return DOFN_DRAWN;
    case DOFG_PHASE: return DOFN_PHASE;
    case DOFG_GAMEPLAY: return DOFN_CUTMODE;
    case DOFG_VIEW: return DOFN_VIEW;
    case DOFG_FP: return DOFN_FP;
    default: return -1;
    }
}
// no focus this frame: why (DofPlayer read ok, the player's view depth w, on screen, the phase)
static int DofNoFocusWhy(int readOk, float w, int figOk, int phase)
{
    if (!readOk) return DOFN_READ;
    if (!(w > 0.0f) || !(w < 1.0e6f)) return DOFN_BEHIND;  // (NaN too)
    if (phase == 1 && !figOk) return DOFN_OFFSCREEN;  // v3.52 FX352: also a close player (no first-person guess)
    return DOFN_BEHIND;
}
typedef struct { unsigned n[DOFN_N]; } DofTally;
// one frame's reason into the room's tally: 1 = the first of its kind in this room (not for DOFN_DRAWN)
static int DofTallyAdd(DofTally *t, int why)
{
    if (why < 0 || why >= DOFN_N) return 0;
    return t->n[why]++ == 0 && why != DOFN_DRAWN;
}
static unsigned DofTallyAsked(const DofTally *t)  // the frames the effect was asked for (the gate said on)
{
    unsigned s = t->n[DOFN_DRAWN];
    int k;
    for (k = DOFN_DESIGN_END + 1; k < DOFN_N; ++k) s += t->n[k];
    return s;
}
static unsigned DofTallyDesign(const DofTally *t)
{
    unsigned s = 0;
    int k;
    for (k = DOFN_PHASE; k <= DOFN_DESIGN_END; ++k) s += t->n[k];
    return s;
}
// "reason n, reason n" for the refusals (design 0) or the by-design skips (design 1), in enum order; "none" when empty
static void DofTallyText(const DofTally *t, int design, char *buf, int cap)
{
    const int k0 = design ? DOFN_PHASE : DOFN_DESIGN_END + 1, k1 = design ? DOFN_DESIGN_END : DOFN_N - 1;
    int k, o = 0;
    if (cap <= 0) return;
    buf[0] = 0;
    for (k = k0; k <= k1; ++k) {
        const char *s = kDofWhyNot[k];
        char num[16];
        int i = 0, d = 0;
        unsigned v = t->n[k];
        if (!v) continue;
        do num[d++] = (char)('0' + v % 10), v /= 10; while (v && d < 15);
        if (o && o + 2 < cap) buf[o++] = ',', buf[o++] = ' ';
        while (s[i] && o + 1 < cap) buf[o++] = s[i++];
        if (o + 1 < cap) buf[o++] = ' ';
        while (d > 0 && o + 1 < cap) buf[o++] = num[--d];
        buf[o < cap ? o : cap - 1] = 0;
    }
    if (!o) {
        const char *s = "none";
        int i = 0;
        while (s[i] && o + 1 < cap) buf[o++] = s[i++];
        buf[o] = 0;
    }
}

// ---------------------------------------------------------------- the depth probe (once per room)
// "Rooms whose depth is not the scene's": a map of the depth against fixed view depths (code = how many of them the
// scene is farther than), read back once per room on a sample grid and along the player's figure. Flat = one code
// holds almost every sample (a cleared or overwritten depth: no slice could find anything); her figure farther than her
// own depth everywhere = her body is not in the depth buffer.
#define DOF_PROBE_N 8
static const float kDofProbeW[DOF_PROBE_N] = { 150.0f, 400.0f, 900.0f, 1800.0f, 3500.0f, 7000.0f, 14000.0f, 30000.0f };
#define DOF_PROBE_GX 32
#define DOF_PROBE_GY 18
#define DOF_PROBE_FIG 9          // samples along her figure
#define DOF_PROBE_FLAT 0.97f     // share of one code that counts as flat
#define DOF_PROBE_MIN 64u        // fewer grid samples: no verdict (OK)
#define DOF_PROBE_AFTER 10       // eligible frames in a room before the probe (the room's load settles)
#define DOF_PROBE_TRIES 3        // v3.45 RB345-01: tries a room gets while the read back is not yet given up
static int DofProbeCodeOf(float w)  // the code a scene depth w gets in the probe map
{
    int j, c = 0;
    for (j = 0; j < DOF_PROBE_N; ++j)
        if (w > kDofProbeW[j]) c = j + 1;
    return c;
}
enum { DOFP_OK, DOFP_FLAT, DOFP_PLAYER, DOFP_FEW, DOFP_NONE, DOFP_NOREAD, DOFP_N };  // v3.45 RB345: + NOREAD
// v3.45 RB345-01 (Codex inspection 1): 1 = the room's probe stays pending and is tried again: nothing was read (NOREAD)
// but the session's read back is not off (a path that may still work failed this time) and the room has tries left. The
// terminal "no read back on this backend" (rbOff) and every other verdict end the room's probe as before.
static int DofProbeRetry(int verdict, int rbOff, int triesDone)
{
    return verdict == DOFP_NOREAD && !rbOff && triesDone < DOF_PROBE_TRIES;
}
static const char *const kDofProbeName[DOFP_N] = {
    "the scene's depth: on", "flat (one depth almost everywhere: cleared or overwritten): off in this room",
    "her figure is not in the depth (only farther things where she stands): off in this room",
    "too few samples: on (no verdict)", "not probed: on",
    "not probed: no read back: on" };  // v3.45 RB345: nothing could be read back (post.inc RbBegin): drawn unprobed
typedef struct {
    unsigned hist[DOF_PROBE_N + 1];  // grid samples per code
    unsigned n;
    int figMin;                      // the nearest code along her figure (-1: no figure samples)
    int figN;
} DofProbeIn;
static int DofProbeVerdict(const DofProbeIn *in, float F, float *share)
{
    unsigned best = 0;
    int c, exp;
    *share = 0.0f;
    if (in->n < DOF_PROBE_MIN) return DOFP_FEW;
    for (c = 0; c <= DOF_PROBE_N; ++c)
        if (in->hist[c] > best) best = in->hist[c];
    *share = (float)best / (float)in->n;
    if (*share >= DOF_PROBE_FLAT) return DOFP_FLAT;
    exp = DofProbeCodeOf(F);
    if (in->figN > 0 && in->figMin >= 0 && F > 0.0f && in->figMin > exp + 1) return DOFP_PLAYER;
    return DOFP_OK;
}

// ---------------------------------------------------------------- the page's preview
// The split view: left of the line the scene as it is, right of it with the chosen depth of field. From a gameplay pause:
// the paused scene and its depth map (taken in the frame whose copy became the pause picture); from the title, or
// without a depth map: preview.dds with a sample depth (the floor plane: far at the top, near at the bottom).
enum { DOFV_NONE, DOFV_SCENE, DOFV_CARD };
enum { DOFW_OFF, DOFW_NOHOOK, DOFW_FAILED, DOFW_SCENE, DOFW_NOPAUSE, DOFW_NOCOPY, DOFW_NODEPTH, DOFW_N };
static const char *const kDofWhy[DOFW_N] = {
    "the preview is off ([ScreenEffects] DofPreview=0)", "no page backdrop hook", "depth of field is off this session",
    "the paused scene", "not a gameplay pause: preview.dds with a sample depth", "no copy of this pause: preview.dds with a sample depth",
    "the paused scene had no depth map: preview.dds with a sample depth" };
typedef struct { int previewOn, hook, failed, pausedGame, haveCopy, haveDepth; } DofPrevIn;
static int DofPreviewMode(const DofPrevIn *in, int *why)
{
    int w;
    if (!in->previewOn) w = DOFW_OFF;
    else if (!in->hook) w = DOFW_NOHOOK;
    else if (in->failed) w = DOFW_FAILED;
    else if (!in->pausedGame) w = DOFW_NOPAUSE;
    else if (!in->haveCopy) w = DOFW_NOCOPY;
    else if (!in->haveDepth) w = DOFW_NODEPTH;
    else w = DOFW_SCENE;
    *why = w;
    return w == DOFW_OFF || w == DOFW_NOHOOK || w == DOFW_FAILED ? DOFV_NONE : w == DOFW_SCENE ? DOFV_SCENE : DOFV_CARD;
}
// a probe code as the probe map's alpha (exact 8-bit steps, point-sampled back; v3.47 DF347: the probe's only)
static unsigned DofCodeAlpha(int code) { return (unsigned)(code < 0 ? 0 : code) * 16u; }

// ---------------------------------------------------------------- the chain's sizes
// level L (1..3) is the screen / 2^L (at least 8 x 8); the eighth level is drawn back up into a quarter-size target
static int DofLevelDim(int full, int level)
{
    int v = full;
    while (level-- > 0) v = (v + 1) / 2;
    return v < 8 ? 8 : v;
}
static unsigned DofChainBytes(int w, int h)  // full copy + half + quarter + eighth + the eighth's quarter-size step
{
    return 4u * ((unsigned)w * (unsigned)h + (unsigned)DofLevelDim(w, 1) * (unsigned)DofLevelDim(h, 1) +
                 2u * (unsigned)DofLevelDim(w, 2) * (unsigned)DofLevelDim(h, 2) + (unsigned)DofLevelDim(w, 3) * (unsigned)DofLevelDim(h, 3));
}
#endif
