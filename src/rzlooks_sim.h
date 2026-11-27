// v3.49 RZ349L: the Randomizer's looks, the pure part (no game memory): [Randomizer] EileenOutfit Off / Random and
// Ghosts Off / Spots (their ini texts), the outfit draw and its override rules, the room-script scan of the victim
// ghosts and the spot pick. src/rzlooks.inc runs it on the game; test/rzlooks_sim_test.c runs it on the stock exe
// (SH4Decomp orig/sh4.exe) and on made-up inputs. Plan: staging/v349_feedback/w4_looks/PLAN.md.
// v3.50 RL350 (staging/v350_feedback/w3_looks/PLAN.md; the user's v3.49 QA B3 / B4): the outfit is drawn among EVERY
// companion outfit (no unlock check: RZO_ALL) and, with Change Each Visit On, again at each room load of her world
// (RzoDrawRoom; at most RZO_RUN_MAX room loads in a row in the same outfit). Ghosts: the spot is never within 600 of
// another ghost's placement (v3.49 let two victims swap spots: the room looked the same), at least 500 from its own and
// 800 + when the room allows (RZG_WANT_*), and a victim's enemy record in this room (written when the room was last left;
// the create restores it at its record spot half the time, EnemyGhostReconcile 0x482590) is moved to the new spot too
// (RzgRecRule: standing records only; pinned / down ones keep their place).
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
static const char *const kRzgName[RZG_N] = { "Off", "On" };  // v3.56 RE356: Spawn Site Off / On (was Ghosts Off / Spots)
static int RzoOfText(const char *s)  // Off / Random (and 0 / 1, On / Off, Yes / No); -1 = anything else
{
    if (RzTextIs(s, "Random")) return RZO_RANDOM;
    return RzOnOffOfText(s);
}
static int RzgOfText(const char *s)  // Off / Spots (and 0 / 1, On / Off, Yes / No); -1 = anything else  // v3.56 RE356: Spawn Site: On / Off; the old Spots reads as On
{
    if (RzTextIs(s, "Spots")) return RZG_SPOTS;
    return RzOnOffOfText(s);
}

// ---------------------------------------------------------------- the outfit
#define RZO_DRESS 0
#define RZO_NURSE 1
static const char *RzoOutfitName(int o) { return o < 0 ? "Default" : o ? "Nurse" : "Dress"; }
// v3.50 RL350: every companion outfit the game has (bit 0 Dress eil.bin, bit 1 Nurse eol.bin; both on the GOG disc, no
// other one exists): the Nurse's unlock (0x13D1A50) is no longer asked (the user: "remove the unlock limiter")
#define RZO_ALL 3u
#define RZO_RUN_MAX 2  // v3.50 RL350: at most this many room loads in a row in the same outfit (then the other one)
// v3.50 RL350: the final fight (stage 9: her walk to Walter decides the ending; Playable Eileen and the game's gates
// 0x44A0A0 treat it apart) keeps the outfit of its world visit: no room draws there
#define RZO_FINAL_STAGE 9
// the outfit of a world visit: one of the given ones (RZO_ALL), from the seed, the stage, the half and the visit's
// number (0 = Change Each Visit Off); -1 = fewer than two (nothing to draw)
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
// v3.50 RL350: the outfit of a room load (Change Each Visit On): from the seed, the stage, the half, the world visit and
// the room load's number in it (seq) and the room; `last` = the outfit of the last draw, `run` = how many draws in a row
// gave it: after RZO_RUN_MAX the other one (so she visibly changes), else a fair draw. -1 = fewer than two outfits
static int RzoDrawRoom(unsigned seed, int stage, int half, unsigned visit, unsigned seq, int room, unsigned unlocked, int last,
                       int run)
{
    int n = 0, i, x;
    for (i = 0; i < 2; ++i) n += (unlocked >> i) & 1;
    if (n < 2) return -1;
    if (last >= 0 && last < 2 && ((unlocked >> last) & 1) && run >= RZO_RUN_MAX) {
        for (i = 0; i < 2; ++i)
            if (i != last && ((unlocked >> i) & 1)) return i;
    }
    x = (int)(RzHash4(seed ^ 0x524F4F4Du, (unsigned)stage | (unsigned)(half & 1) << 8 | (unsigned)(room & 0xFF) << 16, visit, seq) %
              (unsigned)n);
    for (i = 0; i < 2; ++i)
        if (((unlocked >> i) & 1) && !x--) return i;
    return -1;
}
// The override (the engine's eo_outfit holds our draw while `active`). A world visit starts when the stage differs from
// the last one seen (Items' rule in randomizer.inc). Each step (the world list load of an Eileen world, every room
// load) is given what the engine shows now: cur = eo_outfit, saved = g_eoSaved (the outfit last set by the player: the
// ini value, or a pause-menu choice). A pause-menu choice while active (cur or saved changed) is the player's until
// the next visit; Off, or a world without her models, puts `saved` back.
// v3.50 RL350: with perRoom (Change Each Visit On) every room load of the visit after the first draws again
// (RzoDrawRoom); the room load right after a draw at her world's file list keeps that draw (it is the same arrival).
typedef struct {
    int stage;        // the stage last seen (-1 none)
    unsigned visit;   // world visits seen (memory only)
    int drawn;        // this visit is decided (drawn, nothing to draw, or taken over)
    int active;       // eo_outfit holds `wrote`
    int wrote;        // what we put there
    int savedAt;      // g_eoSaved when we put it
    unsigned draws, drawn2[2], restores, takeovers, single;
    unsigned seq;      // v3.50 RL350: room draws in this visit
    int run;           // v3.50 RL350: draws in a row that gave `wrote`
    int skipRoom;      // v3.50 RL350: the next room load keeps the draw just made at her world's file list
    int heldSaid;      // v3.50 RL350: RZO_EV_HELD said in this visit
    unsigned roomDraws, roomChanged;  // v3.50 RL350: room draws (memory only), those that changed her outfit
} RzoState;
enum { RZO_EV_NONE, RZO_EV_DRAW, RZO_EV_SINGLE, RZO_EV_RESTORE, RZO_EV_TAKEOVER, RZO_EV_ROOM, RZO_EV_HELD };  // v3.50 RL350: + ROOM, HELD
static void RzoInit(RzoState *s)
{
    memset(s, 0, sizeof *s);
    s->stage = -1;
}
// *put = the value for eo_outfit (-2 = leave it); returns RZO_EV_*. v3.50 RL350: + atWorld (1 = her world's file list,
// 0 = a room load), room (for the room draw's key)
static int RzoStep(RzoState *s, int mode, unsigned seed, int perVisit, int stage, int half, int eileenHere, unsigned unlocked,
                   int cur, int saved, int atWorld, int room, int *put)
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
        s->seq = 0, s->run = 0, s->skipRoom = 0, s->heldSaid = 0;  // v3.50 RL350
    } else if (ev == RZO_EV_TAKEOVER) {
        return ev;  // kept for the rest of this visit
    }
    if (s->active && (mode != RZO_RANDOM || !eileenHere)) {
        s->active = 0;
        *put = saved;
        ++s->restores;
        return RZO_EV_RESTORE;
    }
    if (mode != RZO_RANDOM || !eileenHere) return ev;
    if (s->drawn) {  // v3.50 RL350: a room load of a visit we dress: a new draw (Change Each Visit On)
        if (!perVisit || !s->active || atWorld || ev != RZO_EV_NONE) return ev;
        if (s->skipRoom) {
            s->skipRoom = 0;
            return ev;
        }
        if (stage == RZO_FINAL_STAGE) {  // the final fight: the visit's outfit stays (said once a visit)
            if (s->heldSaid) return ev;
            s->heldSaid = 1;
            return RZO_EV_HELD;
        }
        s->seq = RzVisitNext(s->seq);
        o = RzoDrawRoom(seed, stage, half, s->visit, s->seq, room, unlocked, s->wrote, s->run);
        if (o < 0) return ev;
        s->run = o == s->wrote ? s->run + 1 : 1;
        s->roomChanged += o != s->wrote;
        s->wrote = o;
        *put = o;
        ++s->roomDraws, ++s->drawn2[o];
        return RZO_EV_ROOM;
    }
    s->drawn = 1;
    o = RzoDraw(seed, stage, half, perVisit ? s->visit : 0, unlocked);
    if (o < 0) {
        ++s->single;
        return RZO_EV_SINGLE;
    }
    s->active = 1, s->wrote = o, s->savedAt = saved;
    s->run = 1, s->skipRoom = atWorld != 0;  // v3.50 RL350
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
    float spot[RZG_SPOTS_MAX][3];  // every enemy placement's position (x, y, z) but the ghosts' (v3.50 RL350: was every one)
    int nghost;                    // v3.50 RL350: the ghosts' placements (types 0B..0F, any mode: victims and story ghosts)
    float ghost[RZG_SPOTS_MAX][3];
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
    if (o->posWrites < 1 || !RzgFinite3(p)) return;
    if (o->type >= 0x0B && o->type <= 0x0F) {  // v3.50 RL350: a ghost's place is no spot for another one (v3.49 swapped them)
        if (out->nghost < RZG_SPOTS_MAX) {
            out->ghost[out->nghost][0] = p[0], out->ghost[out->nghost][1] = p[1], out->ghost[out->nghost][2] = p[2];
            ++out->nghost;
        }
        return;
    }
    if (out->nspot < RZG_SPOTS_MAX) {
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
// v3.50 RL350: four rings, the far ones first (v3.49: 600-1200, 1200-2200, 350-600; most moves were 400-600 units)
#define RZG_TIERS 4
static const RzgTier kRzgTiers[RZG_TIERS] = { { 800.0f, 1400.0f }, { 1400.0f, 2200.0f }, { 2200.0f, 3000.0f }, { 500.0f, 800.0f } };
#define RZG_RING 8            // candidates per ring
#define RZG_MIN_SELF 500.0f   // a new spot is at least this far from the stock one (x / z; v3.50 RL350: was 300)
#define RZG_MIN_OTHER 400.0f  // and from a spot another victim of the room took (v3.50 RL350: was 250)
#define RZG_MIN_GHOST 600.0f  // v3.50 RL350: and from every other ghost's placement in the room (no swapped look-alike)
#define RZG_MIN_PLAYER 600.0f // and from where you arrive (or as far as the stock spot is, when that is nearer)
#define RZG_WANT_SELF 800.0f    // v3.50 RL350: preferred: at least this far from its stock spot (the brief: "e.g. 800")
#define RZG_WANT_PLAYER 1000.0f // v3.50 RL350: preferred: at least this far from where you arrive
#define RZG_VALID_MAX 64
#define RZG_STOCK_VICTIMS 45  // tools/rzl_scan.py --header (stock exe)
#define RZG_STOCK_MOVERS 45
#define RZG_STOCK_ROOMS 32
typedef struct {
    const float *base;         // the stock spot
    unsigned key;              // RzgKey
    const float (*stock)[3];   // the room's other placements (v3.50 RL350: not the ghosts')
    int nStock;
    const float *player;       // where you arrive (NULL = not known)
    const float (*taken)[3];   // spots already given to this room's victims
    int nTaken;
    const float (*ghosts)[3];  // v3.50 RL350: the room's ghost placements (and pinned / down ghosts' record spots); a
    int nGhosts;               // point equal to `base` (its own) is skipped
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
    for (i = 0; i < in->nGhosts; ++i)  // v3.50 RL350
        if (!(in->ghosts[i][0] == in->base[0] && in->ghosts[i][2] == in->base[2]) &&
            RzgDist2(c, in->ghosts[i]) < RZG_MIN_GHOST * RZG_MIN_GHOST)
            return 0;
    return 1;
}
// v3.50 RL350: a spot the room allows far enough: RZG_WANT_SELF from its stock spot and RZG_WANT_PLAYER from you
static int RzgWanted(const RzgPickIn *in, const float *c)
{
    if (RzgDist2(c, in->base) < RZG_WANT_SELF * RZG_WANT_SELF) return 0;
    return !in->player || RzgDist2(c, in->player) >= RZG_WANT_PLAYER * RZG_WANT_PLAYER;
}
// every candidate (the room's other placements, then the rings) that keeps the distance rules and passes the test;
// out = one of them by the key (v3.50 RL350: among the wanted ones when there are any; *shortOut = 1 when none was).
// 1 = found; *nValid / *nTried for the log
static int RzgPick2(const RzgPickIn *in, RzgTestFn test, void *ctx, float *out, int *nValid, int *nTried, int *shortOut)
{
    float valid[RZG_VALID_MAX][3], c[3], minPlayer2 = 0.0f;
    int want[RZG_VALID_MAX], nWant = 0;
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
        want[nv] = RzgWanted(in, c);
        nWant += want[nv];
        ++nv;
    }
    if (nValid) *nValid = nv;
    if (nTried) *nTried = nt;
    if (shortOut) *shortOut = 0;
    if (!nv) return 0;
    if (nWant) {  // v3.50 RL350: the k-th wanted one
        k = (int)(RzHash4(in->key, 0x5049434Bu, (unsigned)nWant, 1) % (unsigned)nWant);
        for (i = 0; i < nv; ++i)
            if (want[i] && !k--) break;
        k = i;
    } else {
        if (shortOut) *shortOut = 1;
        k = (int)(RzHash4(in->key, 0x5049434Bu, (unsigned)nv, 0) % (unsigned)nv);
    }
    out[0] = valid[k][0], out[1] = valid[k][1], out[2] = valid[k][2];
    return 1;
}
static int RzgPick(const RzgPickIn *in, RzgTestFn test, void *ctx, float *out, int *nValid, int *nTried)
{
    return RzgPick2(in, test, ctx, out, nValid, nTried, NULL);
}

// ---------------------------------------------------------------- v3.50 RL350: a victim's enemy record
// The game's records (gEnemyManager 0xFCF090, 64 x 0x50; rzmon_sim.h has the monsters' accessors): +0x00 the position
// (world + the room origin: 0x46FE30 / 0x46FE80), +0x20 id, +0x24 type, +0x26 state (0 free, 0x0A down with its
// countdown, 0x0B pinned), +0x36 stage, +0x37 room, +0x3C a ghost's countdown (float). A victim create (0x4AFA90) runs
// EnemyGhostReconcile 0x482590 then EnemySpawnGate 0x464F20: a record of this stage that is not pinned is deleted half
// the time (mode 0, countdown <= 0) and else kept; kept in this room it is restored there (gate 4: 0x4AD330 ->
// EnemyRecordRestore 0x464470 puts the model at the record's spot), kept in another room the ghost is not made here
// (gate 5). Another stage's record is deleted (mode 0): the placement (ours) is used.
enum { RZG_REC_NONE, RZG_REC_MOVE, RZG_REC_PINNED, RZG_REC_DOWN, RZG_REC_ELSEWHERE, RZG_REC_OTHERWORLD, RZG_REC_N };
static const char *const kRzgRecName[RZG_REC_N] = { "none", "moved", "pinned here", "down here", "in another room",
                                                    "of another world" };
// what Ghosts does with one live record (state != 0) of a mover's id; stage / room = the room being built
static int RzgRecRule(int state, float countdown, int recStage, int recRoom, int stage, int room)
{
    if (state == 0) return RZG_REC_NONE;
    if (recStage != stage) return RZG_REC_OTHERWORLD;  // (a pinned one waits there; else the game deletes it)
    if (recRoom != room) return RZG_REC_ELSEWHERE;     // (half the time deleted: then our spot; else not made here)
    if (state == 0x0B) return RZG_REC_PINNED;          // the Sword holds it where it lies
    if (state == 0x0A || state == 0x10 || state == 0x12 || countdown > 0.0f) return RZG_REC_DOWN;
    return RZG_REC_MOVE;
}
// the strongest of two answers for one mover (several records of its id): a kept one wins over a move
static int RzgRecWorse(int a, int b)
{
    static const int kRank[RZG_REC_N] = { 0, 2, 4, 3, 1, 1 };
    return kRank[b] > kRank[a] ? b : a;
}
#endif
