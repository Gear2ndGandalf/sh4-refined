// ================================================================ v3.28 FIRE: Spirit Vision's spirit fires (the game side)
// staging/v328_spirit/b_fire/DESIGN.md (user items v3.27.3 #2, v3.27.1 #9-10). Replaces v3.17-v3.24's SvFlames (three
// static teardrop quads): an animated flipbook fire with ephemeral embers that shrinks with its being's health, flares
// and goes wild on a hit, and goes out like a real fire (gutter, a last flare, collapse, a coal, falling embers, a smoke
// wisp). The pure logic (state, timeline, particles, the atlas FXX_FLAME) is spiritfire_sim.h; this file reads health
// and hits from game memory (guarded), runs SfFrame once a frame (the first of the two depth passes) and draws.
// Health: enemies EnemyHp [work+0x34] {hp +0, hp_max +4, hit_serial +0x1C} (Walter too: he has one and regenerates);
// the player floats [0x10A0F28] / [0x10A0F4C]; companion Eileen / Cynthia: EileenWork [task+0x408] {float *hp +0,
// float *hp_max +4, damage_taken +0x14} (decomp include/eileen.h). Others (the trapped Cynthia, the peephole Eileen,
// other full-skeleton people) have no HP path: full health, no hit reaction.
#define SF_HIDDEN 0.80f  // the fire where it lies behind scene geometry (inside its own body): SV_HIDDEN 0.45 hid it
#define SF_NEAR0  20.0f  // near-camera fade: none at (eye distance - flame height) <= NEAR0 (only a camera inside the fire)
#define SF_NEAR1  80.0f

static SfWorld g_sf;
static SfSpr g_sfSpr[SF_SPR_MAX];
typedef struct { float x, y, z, rhw, rx, ry, sh, cs, sn, k; int ok; } SfScr;  // a sprite on screen (built once a frame)
static SfScr g_sfScr[SF_SPR_MAX];
static int g_sfSprN, g_sfFires;
static LONG g_sfFrame = -1;
static DWORD g_sfRoom = 0xFFFFFFFFu;

static SfU32 SfKeyOf(const SvSoul *s)  // task + work (pool slots are reused), or the model (no task)
{
    const BYTE *work = NULL;
    if (s->task) {
        __try {
            work = *(const BYTE *const *)(s->task + 0x408);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            work = NULL;
        }
    }
    return (FxHash((DWORD)(ULONG_PTR)(s->task ? (const void *)s->task : (const void *)s->model)) ^
            FxHash((DWORD)(ULONG_PTR)work + 0x51u)) | 1u;
}
static void SfKind(int type, int *kind, float *size)
{
    if (type == 0x10) *kind = SFK_WALTER, *size = SV_FLAME_WALTER;
    else if (type == SV_SOUL_PLAYER || type == SV_SOUL_NPC) *kind = SFK_PERSON, *size = SV_FLAME_PLAYER;
    else *kind = SFK_MONSTER, *size = 1.0f;
}
// the being's health (in->hp, 0..1), what it lost since the last frame (in->drop, a fraction of max HP) and a new hit
// record (in->hit); the last sample lives in the fire (gHp / gDmg / gSer)
static void SfReadHp(const SvSoul *s, SfFire *F, SfIn *in)
{
    static const char *const kSrc[4] = { "", "an enemy's HP block", "the player's HP", "a companion's HP" };
    static int s_srcLogged, s_hitLogged;
    float hp = 0.0f, mx = 0.0f, dmg = -1.0f, frac, drop;
    int ser = -1, src = 0;
    __try {
        if (s->type == SV_SOUL_PLAYER) {
            hp = *(volatile const float *)0x10A0F28, mx = *(volatile const float *)0x10A0F4C, src = 2;
        } else if (s->type == SV_SOUL_NPC) {
            const BYTE *const eil = *(const BYTE *const volatile *)0xFCE6B4, *const cyn = *(const BYTE *const volatile *)0xFD5A50;
            if (s->task && (s->task == eil || s->task == cyn)) {
                const BYTE *const w = *(const BYTE *const *)(s->task + 0x408);
                const float *const ph = w ? *(const float *const *)w : NULL, *const pm = w ? *(const float *const *)(w + 4) : NULL;
                if ((ULONG_PTR)ph >= 0x10000 && (ULONG_PTR)pm >= 0x10000) hp = *ph, mx = *pm, dmg = *(const float *)(w + 0x14), src = 3;
            }
        } else if (s->task) {
            const BYTE *const w = *(const BYTE *const *)(s->task + 0x408);
            const BYTE *const e = w ? *(const BYTE *const *)(w + 0x34) : NULL;
            if ((ULONG_PTR)e >= 0x10000) hp = *(const float *)e, mx = *(const float *)(e + 4), ser = *(const int *)(e + 0x1C), src = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        src = 0;
    }
    if (!src || !_finite(hp) || !_finite(mx) || !(mx > 0.0f) || mx > 100000.0f) {
        F->gHave = 0;
        return;  // in->hp stays -1: full health
    }
    if (!_finite(dmg)) dmg = -1.0f;
    if (!(s_srcLogged >> src & 1)) {
        s_srcLogged |= 1 << src;
        Log("screen effects: Spirit Vision: a spirit fire's health from %s (type %02X): %.1f / %.1f", kSrc[src], s->type, hp, mx);
    }
    frac = hp / mx;
    in->hp = frac;
    if (F->gHave) {
        drop = F->gHp - frac;
        if (dmg >= 0.0f && F->gDmg >= 0.0f && (dmg - F->gDmg) / mx > drop) drop = (dmg - F->gDmg) / mx;  // healed the same frame
        in->drop = drop > 0.0f ? drop : 0.0f;
        in->hit = src == 1 && ser != -1 && ser != F->gSer;
        if ((in->hit || in->drop >= SF_HIT_DROP) && !(s_hitLogged >> src & 1)) {
            s_hitLogged |= 1 << src;
            Log("screen effects: Spirit Vision: a spirit fire flares: %s took %.1f %% of its max HP (type %02X%s)", kSrc[src],
                100.0f * in->drop, s->type, in->hit ? ", a new hit record" : "");
        }
    }
    F->gHp = frac, F->gDmg = dmg, F->gSer = ser, F->gHave = 1;
    if (src == 2 && hp <= 0.0f) in->deadNow = 1;  // the player at 0 HP: her fire goes out
}
#include "hbtheme_sim.h"  // v3.59 FT359: HbtSpiritTint (Ghost Teleport's spirit-blue fade)
static int g_ftOn;  // v3.59 FT359: [ScreenEffects] FilterThemes (hidden, default 1; read by hudbars.inc HbThemeInit)
// once a frame: the beings drawn this frame (g_svSoul), the fire states, the sprites on screen
static void SfBuild(const Viewport8 *vp, float now)
{
    static SfIn ins[SV_SOULS];
    static int s_scaleBad, s_dieLogged, s_logged;
    const DWORD rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    const float unit = (float)vp->h / 720.0f;
    int i, n = 0;
    SfU32 herKey = 0;  // v3.58 GT358: her soul fire's key (Ghost Teleport's fade)
    if (rk != g_sfRoom) {  // a new stage / room: no fire or particle carries over
        g_sfRoom = rk;
        memset(&g_sf, 0, sizeof g_sf);
    }
    for (i = 0; g_gvStamp == g_fxFrame && i < g_svSoulN && n < SV_SOULS; ++i) {
        const SvSoul *s = &g_svSoul[i];
        SfIn *in = &ins[n];
        memset(in, 0, sizeof *in);
        if (!SvChest(s->model, s->type, in->o)) continue;
        in->key = SfKeyOf(s);
        if (s->type == SV_SOUL_PLAYER) herKey = in->key;  // v3.58 GT358
        SfKind(s->type, &in->kind, &in->size);
        in->dieS = g_soulFadeS;
        in->hp = -1.0f;
        in->deadAge = s->age;
        SfReadHp(s, SfAcquire(&g_sf, in, now), in);
        if ((in->deadAge >= 0.0f || in->deadNow) && !s_dieLogged++)
            Log("screen effects: Spirit Vision: a spirit fire goes out (type %02X; SoulFadeSeconds %.0f)", s->type, g_soulFadeS);
        ++n;
    }
    g_sfSprN = SfFrame(&g_sf, ins, n, now, g_sfSpr, SF_SPR_MAX);
    if (herKey) {  // v3.58 GT358: in Ghost Teleport's fade her soul fire fades with her body (teleport.inc GtSoulOp; embers fly on)
        const float gk = GtSoulOp();
        if (gk < 1.0f)
            for (i = 0; i < g_sfSprN; ++i)
                if (g_sfSpr[i].fire >= 0 && g_sfSpr[i].fire < SF_FIRES && g_sf.fire[g_sfSpr[i].fire].key == herKey) {
                    g_sfSpr[i].a *= gk;
                    if (g_ftOn && g_sfSpr[i].add) HbtSpiritTint(g_sfSpr[i].rgb, gk);  // v3.59 FT359: a spectral warp: her fire (not its smoke) turns spirit blue as she fades
                }
    }
    for (i = g_sfFires = 0; i < SF_FIRES; ++i) g_sfFires += g_sf.fire[i].live;
    if (!SvSprScale(vp)) {  // pixels per world unit (as the auras); no camera matrix: nothing this frame
        if (!s_scaleBad++) Log("screen effects: Spirit Vision: spirit fires: no sprite scale from the camera matrix (skipped)");
        g_sfSprN = 0;
    }
    for (i = 0; i < g_sfSprN; ++i) {
        const SfSpr *q = &g_sfSpr[i];
        SfScr *o = &g_sfScr[i];
        float h[4], x, y, rx, ry, m, reach;
        o->ok = 0;
        GvXform(q->p, h);
        if (!GvScreen(h, vp, &x, &y)) continue;  // also the w >= GV_NEAR_W near clip
        rx = q->rx * g_svPxX / h[3];
        ry = q->ry * g_svPxY / h[3];
        m = q->cell < SF_FRAMES ? ry : rx > ry ? rx : ry;
        if (!_finite(m) || !(m > 0.0f)) continue;
        if (m < q->minPx * unit) rx *= q->minPx * unit / m, ry *= q->minPx * unit / m;  // far: still a visible fire
        m = q->cell < SF_FRAMES ? ry : rx > ry ? rx : ry;
        if (q->cell < SF_FRAMES ? m > 0.6f * (float)vp->h : m > SV_SPR_MAXH * (float)vp->h) {
            const float f = (q->cell < SF_FRAMES ? 0.6f : SV_SPR_MAXH) * (float)vp->h / m;
            rx *= f, ry *= f;
        }
        reach = 1.415f * (rx > ry ? rx : ry) + fabsf(q->lean) * ry;
        if (x + reach < (float)vp->x || x - reach > (float)(vp->x + vp->w) || y + reach < (float)vp->y || y - reach > (float)(vp->y + vp->h))
            continue;
        o->k = 1.0f;
        if (g_svEyeOk) {
            const float dx = q->p[0] - g_svEye[0], dy = q->p[1] - g_svEye[1], dz = q->p[2] - g_svEye[2];
            o->k = FxSmooth(SF_NEAR0, SF_NEAR1, sqrtf(dx * dx + dy * dy + dz * dz) - q->ry);
            if (!(o->k > 0.002f)) continue;
        }
        o->x = x, o->y = y, o->z = SvZH(h), o->rhw = 1.0f / h[3];
        o->rx = rx, o->ry = ry, o->sh = q->lean * ry, o->cs = cosf(q->rot), o->sn = sinf(q->rot);
        o->ok = 1;
    }
    if (!s_logged && g_sfFires) {
        s_logged = 1;
        Log("screen effects: Spirit Vision: spirit fires v3.28 (%d fire(s), %d sprite(s); flipbook %d frames, %d particles max)",
            g_sfFires, g_sfSprN, SF_FRAMES, SF_PARTS);
    }
}
// each depth pass: the smoke (premultiplied ONE / INVSRCALPHA), then the fires, glows, embers, sparks (additive ONE / ONE),
// all from the one FXX_FLAME atlas; leaves SvUntextured's states. Returns the fires alive.
static int SvFlames(void *dev, const Viewport8 *vp, float now)
{
    static SpVtx v[6 * SF_SPR_MAX];
    const float mul = g_svZ && g_svMul < 1.0f ? SF_HIDDEN : g_svMul;
    void *tex;
    int b, i, m, n;
    if (g_sfFrame != g_fxFrame) {  // the first pass of the frame steps the fires (the second draws the same sprites)
        g_sfFrame = g_fxFrame;
        SfBuild(vp, now);
    }
    if (!g_sfSprN || (tex = FxTexture(dev, FXX_FLAME)) == NULL) return g_sfFires;
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    FxSampler(dev, tex, 0, 0);                    // clamp, linear (every atlas cell is 0 at its border)
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
    for (b = 0; b < 2; ++b) {  // 0: premultiplied (the smoke), 1: additive
        n = 0;
        for (i = 0; i < g_sfSprN; ++i) {
            const SfSpr *q = &g_sfSpr[i];
            const SfScr *o = &g_sfScr[i];
            const float z = g_svZ ? o->z : 0.0f, rhw = g_svZ ? o->rhw : 1.0f;
            float uv[4];
            SpVtx c[4];
            DWORD col;
            if (!o->ok || q->add != b) continue;
            SfCellUv(q->cell, uv);
            if (q->mirror) {
                const float t = uv[0];
                uv[0] = uv[2], uv[2] = t;
            }
            if (b) {
                const float k = q->a * mul * o->k;
                col = 0xFF000000u | (DWORD)(FxSat(q->rgb[0] * k) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(q->rgb[1] * k) * 255.0f + 0.5f) << 8 |
                      (DWORD)(FxSat(q->rgb[2] * k) * 255.0f + 0.5f);
            } else {
                const float a = FxSat(q->a * mul * o->k);
                col = (DWORD)(a * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(q->rgb[0]) * a * 255.0f + 0.5f) << 16 |
                      (DWORD)(FxSat(q->rgb[1]) * a * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(q->rgb[2]) * a * 255.0f + 0.5f);
            }
            for (m = 0; m < 4; ++m) {
                const float cx = (m & 1) ? 1.0f : -1.0f, cy = (m & 2) ? 1.0f : -1.0f;
                if (q->cell < SF_FRAMES) {  // a flame quad: 70 % above the anchor, 30 % below, the top edge sheared by lean
                    c[m].x = o->x + cx * o->rx + ((m & 2) ? 0.0f : o->sh) - 0.5f;
                    c[m].y = o->y + ((m & 2) ? 0.30f : -0.70f) * o->ry - 0.5f;
                } else {  // a round sprite, rotated
                    c[m].x = o->x + (cx * o->cs - cy * o->sn) * o->rx - 0.5f;
                    c[m].y = o->y + (cx * o->sn + cy * o->cs) * o->ry - 0.5f;
                }
                c[m].z = z;
                c[m].rhw = rhw;
                c[m].c = col;
                c[m].u = (m & 1) ? uv[2] : uv[0];
                c[m].v = (m & 2) ? uv[3] : uv[1];
            }
            v[n++] = c[0], v[n++] = c[1], v[n++] = c[2];
            v[n++] = c[1], v[n++] = c[3], v[n++] = c[2];
        }
        if (!n) continue;
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, b ? 2 /*ONE*/ : 6 /*INVSRCALPHA*/);
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
    }
    SvUntextured(dev);  // the states the following draws expect: no texture, additive diffuse
    return g_sfFires;
}
