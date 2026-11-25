// v3.49 RZ349L: the Randomizer's looks, the pure part (no game memory): [Randomizer] EileenOutfit Off / Random and
// Ghosts Off / Spots (their ini texts), the outfit draw and its override rules, the room-script scan of the victim
// ghosts and the spot pick. src/rzlooks.inc runs it on the game; test/rzlooks_sim_test.c runs it on the stock exe
// (SH4Decomp orig/sh4.exe) and on made-up inputs. Plan: staging/v349_feedback/w4_looks/PLAN.md.
//
// Facts (checked against the stock exe md5 15a85581 by tools/rzl_scan.py and the native test, 2026-11-25):
// - Outfits of the companion Eileen: Dress (eil.bin 0xF000F058) and Nurse (eol.bin 0xF000F059); there is no other
//   companion outfit. The Nurse is unlocked per save: gSaveNurseUnlocked 0x13D1A50 (SaveGetNurseUnlocked 0x56F590 =
//   A1 50 1A 3D 01 C3), set by SaveAwardNurseUnlock 0x4CEFB0 at a clear save (ending 1 or 2 with item 0x7B), loaded
//   from the slot by SaveSnapshotApply 0x4FC5B0 at step 0 of the world start 0x4FA4B0, before the stage's file list
//   (StageFileListLoad 0x4F3460, step 3), reset by a New Game (0x56F610). The Dress needs no unlock.
// - Enemy placements (kind 2) in the room scripts: cmd 7 arg 0 = the type (its functions from 0x615348 + 16 * type),
//   cmd 8 = the position (args 0..2, floats x / y / z, desc +0x14 ..), the mode word (desc +0x24: cmd 0x0A arg 0, 0x0B,
//   0x0D, 0x24; low nibble the spawn mode, bits 8-11 the variant), the id override (desc +0x2C: 0x0A arg 2, 0x0F, 0x29;
//   a ghost's own id FFFF00nn). The victims: 0B (any variant), 0C v1, 0D v1 with spawn mode 0 and an FFFF00nn id; the
//   story ghosts (0C v0 Cynthia, 0D v0 Jasper, 0E Andrew, 0F Richard), the Patient 11 and the scene modes 6 / 7 never
//   move. In the 17 distinct stock scripts: 45 victim placements, all made by cmd 7 with one cmd 8, so all 45 may move
//   (RZG_STOCK_MOVERS), in 32 rooms, at most 3 in a room; a room has at most 30 kind-2 placements.
// - eil.bin is in the world file list of stages 7, 8, 9, 11 (1st half) and 2, 3, 4, 5, 7, 8, 9, 11 (2nd half) and in
//   the room lists of 6:43, 7:10 and 2:55 (2nd half).
#ifndef RZLOOKS_SIM_H
#define RZLOOKS_SIM_H
#include <math.h>
#include "rando_sim.h"

// ---------------------------------------------------------------- settings
enum { RZO_OFF, RZO_RANDOM, RZO_N };  // [Randomizer] EileenOutfit
enum { RZG_OFF, RZG_SPOTS, RZG_N };   // [Randomizer] Ghosts
// the user's spec (USER_FEEDBACK_v348 D): EileenOutfit Random, Ghosts Spots
#define RZO_DEFAULT RZO_RANDOM
#define RZG_DEFAULT RZG_SPOTS
static const char *const kRzoName[RZO_N] = { "Off", "Random" };
static const char *const kRzgName[RZG_N] = { "Off", "Spots" };
static int RzoOfText(const char *s)  // Off / Random (and 0 / 1, On / Off, Yes / No); -1 = anything else
{
    if (RzTextIs(s, "Random")) return RZO_RANDOM;
    return RzOnOffOfText(s);
}
static int RzgOfText(const char *s)  // Off / Spots (and 0 / 1, On / Off, Yes / No); -1 = anything else
{
    if (RzTextIs(s, "Spots")) return RZG_SPOTS;
    return RzOnOffOfText(s);
}

// ---------------------------------------------------------------- the outfit
#define RZO_DRESS 0
#define RZO_NURSE 1
static const char *RzoOutfitName(int o) { return o < 0 ? "Default" : o ? "Nurse" : "Dress"; }
static unsigned RzoUnlocked(int nurseUnlocked) { return 1u | (nurseUnlocked ? 2u : 0u); }  // bit 0 Dress, bit 1 Nurse
// the outfit of a world visit: one of the unlocked ones, from the seed, the stage, the half and the visit's number
// (0 = Change Each Visit Off); -1 = fewer than two unlocked (nothing to draw)
static int RzoDraw(unsigned seed, int stage, int half, unsigned visit, unsigned unlocked)
{
    int n = 0, i, x;
    for (i = 0; i < 2; ++i) n += (unlocked >> i) & 1;
    if (n < 2) return -1;
    x = (int)(RzHash4(seed, (unsigned)stage | (unsigned)(half & 1) << 8 | 0x4F550000u, 0x4F555446u, visit) % (unsigned)n);
    for (i = 0; i < 2; ++i)
        if (((unlocked >> i) & 1) && !x--) return i;
    return -1;
}
// The override (the engine's eo_outfit holds our draw while `active`). A world visit starts when the stage differs from
// the last one seen (Items' rule in randomizer.inc). Each step (the world list load of an Eileen world, every room
// load) is given what the engine shows now: cur = eo_outfit, saved = g_eoSaved (the outfit last set by the player: the
// ini value, or a pause-menu choice). A pause-menu choice while active (cur or saved changed) is the player's until
// the next visit; Off, or a world without her models, puts `saved` back.
typedef struct {
    int stage;        // the stage last seen (-1 none)
    unsigned visit;   // world visits seen (memory only)
    int drawn;        // this visit is decided (drawn, nothing to draw, or taken over)
    int active;       // eo_outfit holds `wrote`
    int wrote;        // what we put there
    int savedAt;      // g_eoSaved when we put it
    unsigned draws, drawn2[2], restores, takeovers, single;
} RzoState;
enum { RZO_EV_NONE, RZO_EV_DRAW, RZO_EV_SINGLE, RZO_EV_RESTORE, RZO_EV_TAKEOVER };
static void RzoInit(RzoState *s)
{
    memset(s, 0, sizeof *s);
    s->stage = -1;
}
// *put = the value for eo_outfit (-2 = leave it); returns RZO_EV_*
static int RzoStep(RzoState *s, int mode, unsigned seed, int perVisit, int stage, int half, int eileenHere, unsigned unlocked,
                   int cur, int saved, int *put)
{
    int o, ev = RZO_EV_NONE;
    *put = -2;
    if (s->active && (cur != s->wrote || saved != s->savedAt)) {  // the player chose meanwhile (in the visit that is ending,
        s->active = 0;                                            // if the stage changed): theirs, never put back
        s->drawn = 1;
        ++s->takeovers;
        ev = RZO_EV_TAKEOVER;
    }
    if (stage != s->stage) {
        s->stage = stage;
        s->visit = RzVisitNext(s->visit);
        s->drawn = 0;
    } else if (ev == RZO_EV_TAKEOVER) {
        return ev;  // kept for the rest of this visit
    }
    if (s->active && (mode != RZO_RANDOM || !eileenHere)) {
        s->active = 0;
        *put = saved;
        ++s->restores;
        return RZO_EV_RESTORE;
    }
    if (mode != RZO_RANDOM || !eileenHere || s->drawn) return ev;
    s->drawn = 1;
    o = RzoDraw(seed, stage, half, perVisit ? s->visit : 0, unlocked);
    if (o < 0) {
        ++s->single;
        return RZO_EV_SINGLE;
    }
    s->active = 1, s->wrote = o, s->savedAt = saved;
    *put = o;
    ++s->draws, ++s->drawn2[o];
    return RZO_EV_DRAW;
}

// ---------------------------------------------------------------- the victim ghosts of a room
static int RzgIsVictim(int type, int variant) { return type == 0x0B || ((type == 0x0C || type == 0x0D) && variant == 1); }
static int RzgIsStoryGhost(int type, int variant)
{
    return ((type == 0x0C || type == 0x0D) && variant == 0) || type == 0x0E || type == 0x0F;
}
static int RzgGhostId(unsigned id) { return (id & 0xFFFFFF00u) == 0xFFFF0000u; }
#define RZG_MOVERS_MAX 8   // victims of one room (stock: 3 at most)
#define RZG_SPOTS_MAX 32   // placements of one room kept as candidate spots (stock: 30 at most)
typedef struct { unsigned posVa, pos[3], id, mode; int type, n; } RzgMover;  // pos = the three words (float bits)
typedef struct {
    int objects, kind2, victims, nm, nspot, notMoved, bad;  // notMoved: victims that may not move (no / two cmd 8, own functions)
    RzgMover m[RZG_MOVERS_MAX];
    float spot[RZG_SPOTS_MAX][3];  // every other enemy placement's position (x, y, z)
} RzgRoom;
typedef struct { int kind, funcs, type, posWrites; unsigned mode, id, posVa, pos[3]; } RzgObj;
static float RzgF(unsigned u)
{
    float f;
    memcpy(&f, &u, 4);
    return f;
}
static unsigned RzgU(float f)
{
    unsigned u;
    memcpy(&u, &f, 4);
    return u;
}
static int RzgFinite3(const float *p)
{
    int i;
    for (i = 0; i < 3; ++i)
        if (!(p[i] == p[i]) || p[i] > 1e7f || p[i] < -1e7f) return 0;
    return 1;
}
// a mover: a victim placement of spawn mode 0 with its own ghost id, made by cmd 7 alone, exactly one cmd 8
static int RzgMayMove(const RzgObj *o)
{
    const int variant = (int)(o->mode >> 8 & 0xF);
    if (o->kind != 2 || o->type < 0 || !RzgIsVictim(o->type, variant) || RzgIsStoryGhost(o->type, variant)) return 0;
    return (o->mode & 0xF) == 0 && RzgGhostId(o->id) && !o->funcs && o->posWrites == 1;
}
static void RzgFlush(const RzgObj *o, int n, RzgRoom *out)
{
    float p[3];
    int i;
    ++out->objects;
    if (o->kind != 2) return;
    ++out->kind2;
    for (i = 0; i < 3; ++i) p[i] = RzgF(o->pos[i]);
    if (o->type >= 0 && RzgIsVictim(o->type, (int)(o->mode >> 8 & 0xF)) && !RzgIsStoryGhost(o->type, (int)(o->mode >> 8 & 0xF)) &&
        (o->mode & 0xF) == 0 && RzgGhostId(o->id)) {
        ++out->victims;
        if (!RzgMayMove(o) || !RzgFinite3(p) || out->nm >= RZG_MOVERS_MAX) {
            ++out->notMoved;
        } else {  // (its spot stays a candidate for the room's other victims; RZG_MIN_SELF keeps it from its own)
            RzgMover *m = &out->m[out->nm++];
            m->posVa = o->posVa, m->pos[0] = o->pos[0], m->pos[1] = o->pos[1], m->pos[2] = o->pos[2];
            m->id = o->id, m->mode = o->mode, m->type = o->type, m->n = n;
        }
    }
    if (o->posWrites >= 1 && RzgFinite3(p) && out->nspot < RZG_SPOTS_MAX) {
        out->spot[out->nspot][0] = p[0], out->spot[out->nspot][1] = p[1], out->spot[out->nspot][2] = p[2];
        ++out->nspot;
    }
}
// one room from its start (gRoomScripts[room]): the runner's rule (rando_sim.h RzScanRoom). 1 = read
static int RzgScanRoom(const RzWin *w, unsigned p, RzgRoom *out)
{
    unsigned wd, a;
    int first = 1, guard = 0, n = 0;
    RzgObj o;
    memset(out, 0, sizeof *out);
    memset(&o, 0, sizeof o);
    o.type = -1;
    for (;;) {
        unsigned tag, num;
        if (++guard > RZ_GUARD || !RzRd(w, p, &wd)) return out->bad = 1, 0;
        tag = wd & 0xFFFFu, num = wd >> 16;
        if (!first && (wd == 0xF0F0F0F0u || tag == 0)) break;
        if (tag == 1 && !first) {
            RzgFlush(&o, n++, out);
            memset(&o, 0, sizeof o);
            o.type = -1;
        }
        switch (tag) {
        case 1: if (num >= 1 && RzRd(w, p + 4, &a)) o.kind = (int)a; break;
        case 2: case 3: case 4: case 5: case 6: o.funcs = 1; break;
        case 7: if (num >= 1 && RzRd(w, p + 4, &a)) o.type = a < 0x40u ? (int)a : -1; break;
        case 8:
            if (num >= 3) {
                if (!RzRd(w, p + 4, &o.pos[0]) || !RzRd(w, p + 8, &o.pos[1]) || !RzRd(w, p + 12, &o.pos[2])) return out->bad = 1, 0;
                o.posVa = p + 4, ++o.posWrites;
            }
            break;
        case 0x0A:
            if (num >= 3 && (!RzRd(w, p + 4, &o.mode) || !RzRd(w, p + 12, &o.id))) return out->bad = 1, 0;
            break;
        case 0x0B: case 0x0D: case 0x24: if (num >= 1 && !RzRd(w, p + 4, &o.mode)) return out->bad = 1, 0; break;
        case 0x0F: case 0x29: if (num >= 1 && !RzRd(w, p + 4, &o.id)) return out->bad = 1, 0; break;
        }
        first = 0;
        p += 4u * (num + 1u);
    }
    RzgFlush(&o, n, out);
    return 1;
}

// ---------------------------------------------------------------- the spot pick
// test(ctx, base, cand): 1 = the ghost may start at cand (cand may be moved onto its floor); the game's is enemies.inc's
// EnmSpotTest (flyer rule: the same height above a floor of the same storey, not in a wall, in sight of its stock spot)
typedef int (*RzgTestFn)(void *ctx, const float *base, float *cand);
typedef struct { float rMin, rMax; } RzgTier;
#define RZG_TIERS 3
static const RzgTier kRzgTiers[RZG_TIERS] = { { 600.0f, 1200.0f }, { 1200.0f, 2200.0f }, { 350.0f, 600.0f } };
#define RZG_RING 8           // candidates per ring
#define RZG_MIN_SELF 300.0f  // a new spot is at least this far from the stock one (x / z)
#define RZG_MIN_OTHER 250.0f // and from a spot another victim of the room took
#define RZG_MIN_PLAYER 600.0f // and from where you arrive (or as far as the stock spot is, when that is nearer)
#define RZG_VALID_MAX 64
#define RZG_STOCK_VICTIMS 45  // tools/rzl_scan.py --header (stock exe)
#define RZG_STOCK_MOVERS 45
#define RZG_STOCK_ROOMS 32
typedef struct {
    const float *base;         // the stock spot
    unsigned key;              // RzgKey
    const float (*stock)[3];   // the room's other placements
    int nStock;
    const float *player;       // where you arrive (NULL = not known)
    const float (*taken)[3];   // spots already given to this room's victims
    int nTaken;
} RzgPickIn;
static float RzgDist2(const float *a, const float *b)  // x / z only
{
    const float dx = a[0] - b[0], dz = a[2] - b[2];
    return dx * dx + dz * dz;
}
// the key of a placement: the seed, the stage and half, the room and object number, the room's visit (0 = fixed)
static unsigned RzgKey(unsigned seed, int stage, int half, int room, int n, unsigned visit)
{
    return RzHash4(seed ^ 0x47485354u, (unsigned)stage | (unsigned)(half & 1) << 8, (unsigned)room << 8 | (unsigned)(n & 0xFF), visit);
}
static void RzgRingCand(unsigned key, int tier, int c, const float *base, float *out)
{
    const RzgTier *T = &kRzgTiers[tier];
    const unsigned h1 = RzHash4(key, 0x52494E47u, (unsigned)tier, (unsigned)c), h2 = RzMix(h1 ^ 0x55u);
    const float a = (float)(h1 >> 8) * (6.2831853f / 16777216.0f) + 0.7853982f * (float)c;
    const float d = T->rMin + (T->rMax - T->rMin) * (float)(h2 >> 8) / 16777216.0f;
    out[0] = base[0] + d * (float)sin(a), out[1] = base[1], out[2] = base[2] + d * (float)cos(a);
}
static int RzgFree(const RzgPickIn *in, const float *c, float minPlayer2)  // the distance rules (x / z)
{
    int i;
    if (RzgDist2(c, in->base) < RZG_MIN_SELF * RZG_MIN_SELF) return 0;
    if (in->player && RzgDist2(c, in->player) < minPlayer2) return 0;
    for (i = 0; i < in->nTaken; ++i)
        if (RzgDist2(c, in->taken[i]) < RZG_MIN_OTHER * RZG_MIN_OTHER) return 0;
    return 1;
}
// every candidate (the room's other placements, then three rings) that keeps the distance rules and passes the test;
// out = one of them by the key. 1 = found; *nValid / *nTried for the log
static int RzgPick(const RzgPickIn *in, RzgTestFn test, void *ctx, float *out, int *nValid, int *nTried)
{
    float valid[RZG_VALID_MAX][3], c[3], minPlayer2 = 0.0f;
    int nv = 0, nt = 0, i, t, k;
    if (in->player) {
        const float d2 = RzgDist2(in->base, in->player);
        minPlayer2 = d2 < RZG_MIN_PLAYER * RZG_MIN_PLAYER ? d2 : RZG_MIN_PLAYER * RZG_MIN_PLAYER;
    }
    for (i = 0; i < in->nStock + RZG_TIERS * RZG_RING && nv < RZG_VALID_MAX; ++i) {
        if (i < in->nStock) c[0] = in->stock[i][0], c[1] = in->base[1], c[2] = in->stock[i][2];  // (the ghost's own height)
        else t = (i - in->nStock) / RZG_RING, k = (i - in->nStock) % RZG_RING, RzgRingCand(in->key, t, k, in->base, c);
        if (!RzgFree(in, c, minPlayer2)) continue;
        ++nt;
        if (!test(ctx, in->base, c) || !RzgFinite3(c) || !RzgFree(in, c, minPlayer2)) continue;
        valid[nv][0] = c[0], valid[nv][1] = c[1], valid[nv][2] = c[2];
        ++nv;
    }
    if (nValid) *nValid = nv;
    if (nTried) *nTried = nt;
    if (!nv) return 0;
    k = (int)(RzHash4(in->key, 0x5049434Bu, (unsigned)nv, 0) % (unsigned)nv);
    out[0] = valid[k][0], out[1] = valid[k][1], out[2] = valid[k][2];
    return 1;
}
#endif
