// v3.56 RE356: the Randomizer's Spawn Site, the pure part (no game memory, no Windows): the site of a monster (a cell of
// the room's floor map, Ghost Teleport's GtMap, flooded by the player's walk rules from the room's doors and the arrival
// point) and the site of a ghost (anywhere inside the room's collision box in x, y and z: a floor under the point, the
// ghost's own height above it). src/spawnsite.inc runs it on the game (with rzmon.inc for the monsters and rzlooks.inc for
// the victims); test/spawnsite_sim_test.c runs it on made-up rooms. Plan: staging/v356_feedback/w3_rando_enemies/PLAN.md
// (the user's v3.55 improvement 15: "Replace Ghosts in randomizer with mod named Spawn Site which affects every enemy in the
// game that is not a wall monster, boss or wheel chair ... For Ghosts it should affect X Y Z and stay within room
// boundaries"). y grows downward (the game's frame): a floor is below a point when its y is larger.
#ifndef SPAWNSITE_SIM_H
#define SPAWNSITE_SIM_H
#include "rando_sim.h"
#include "teleport_sim.h"

#define SS_MIN_ENTRY   800.0f  // a monster's site: at least this far (x / z) from where you arrive
#define SS_MIN_DOOR    400.0f  // ... and from every door's point on the room's side
#define SS_MIN_OTHER   250.0f  // ... and from another site of this room load
#define SS_TRIES       48      // map cells tried for one monster
#define SS_JITTER      0.3f    // a cell's point moved up to this part of a cell (so sites are not on the grid)
#define SS_FLOOR_DY    60.0f   // the game's floor ray must find the cell's floor within this
#define SS_BOX_IN      150.0f  // a ghost's site: this far inside the box's sides (x / z)
#define SS_GHOST_ENTRY 600.0f  // ... at least this far (x / z) from where you arrive (never inside the player)
#define SS_GHOST_GHOST 600.0f  // ... and from another ghost's placement (rzlooks_sim.h RZG_MIN_GHOST)
#define SS_GHOST_OTHER 400.0f  // ... and from another site of this room load (rzlooks_sim.h RZG_MIN_OTHER)
#define SS_GHOST_TRIES 64
#define SS_TAKEN_MAX   64      // sites of one room load (monsters then victims)

// why a candidate failed (the log counts them)
enum { SSW_OK, SSW_ENTRY, SSW_DOOR, SSW_OTHER, SSW_GHOST, SSW_BOX, SSW_WALL, SSW_FLOOR, SSW_ROOM, SSW_BODY, SSW_N };
static const char *const kSsWhy[SSW_N] = { "", "near where you arrive", "near a door", "near another site", "near a ghost",
                                           "outside the room's box", "in a wall", "no floor", "no room to stand", "body blocked" };

// the key of a placement's site: the seed, the stage and half, the room, its object number, the room's visit salt (0 =
// fixed; the Randomizer's Reroll rule) and its kind (0 a monster placement, 1 a victim)
static unsigned SsKey(unsigned seed, int stage, int half, int room, int n, unsigned visit, int kind)
{
    return RzHash4(seed ^ 0x53495445u, (unsigned)stage | (unsigned)(half & 1) << 8 | (unsigned)(kind & 1) << 9,
                   (unsigned)room << 16 | (unsigned)(n & 0xFFFF), visit);
}
static float SsU01(unsigned h) { return (float)(h >> 8) * (1.0f / 16777216.0f); }  // [0, 1)
static float SsD2(const float *a, const float *b)  // x / z
{
    const float dx = a[0] - b[0], dz = a[2] - b[2];
    return dx * dx + dz * dz;
}
static int SsFinite3(const float *p)
{
    int i;
    for (i = 0; i < 3; ++i)
        if (!(p[i] == p[i]) || p[i] > 1e7f || p[i] < -1e7f) return 0;
    return 1;
}

// ---------------------------------------------------------------- monsters
// the reached cells of a map (the flood marks a cell reached when it is queued: its floor is proven from a reached
// neighbour by the walk rules, so an unfinished map's cells count too); n written (at most max)
static int SsMapCells(const GtMap *m, unsigned short *out, int max)
{
    int c, n = 0;
    if (!m || !m->on) return 0;
    for (c = 0; c < m->nx * m->ny && n < max; ++c)
        if (m->st[c]) out[n++] = (unsigned short)c;
    return n;
}
// the distance rules of a monster's site (x / z): SSW_OK or why not
static int SsMonFree(const float *c, const float *arrive, const GtExit *x, int nx, const float (*taken)[3], int nTaken)
{
    int i;
    if (arrive && SsD2(c, arrive) < SS_MIN_ENTRY * SS_MIN_ENTRY) return SSW_ENTRY;
    for (i = 0; i < nx; ++i) {
        const float d[3] = { x[i].px, x[i].y, x[i].pz };
        if (SsD2(c, d) < SS_MIN_DOOR * SS_MIN_DOOR) return SSW_DOOR;
    }
    for (i = 0; i < nTaken; ++i)
        if (SsD2(c, taken[i]) < SS_MIN_OTHER * SS_MIN_OTHER) return SSW_OTHER;
    return SSW_OK;
}
// test(ctx, cand): the game's own checks at a candidate (on its floor: cand[1] may be set to the floor the game finds);
// SSW_OK or why not
typedef int (*SsTestFn)(void *ctx, float *cand);
typedef struct {
    unsigned key;               // SsKey(..., 0)
    const float *arrive;        // where you arrive (NULL = not known)
    const GtExit *exits;        // the room's doors (their points px / pz on the room's side)
    int nExits;
    const float (*taken)[3];    // this room load's sites so far
    int nTaken;
} SsMonIn;
// a site among the map's reached cells (cells[0..nCells)), tried in the key's order; out = its floor point. 1 = found;
// why[SSW_N] (may be NULL) counts the failures
static int SsMonPick(const SsMonIn *in, const GtMap *m, const unsigned short *cells, int nCells, SsTestFn test, void *ctx,
                     float *out, int *why)
{
    int t;
    if (!m || !m->on || nCells <= 0) return 0;
    for (t = 0; t < SS_TRIES; ++t) {
        const unsigned h = RzHash4(in->key, 0x43454C4Cu, (unsigned)t, (unsigned)nCells);
        const int c = (int)cells[h % (unsigned)nCells];
        float p[3];
        int r;
        if (c < 0 || c >= m->nx * m->ny || !m->st[c]) continue;
        GtMapXZ(m, c, &p[0], &p[2]);
        p[0] += (SsU01(RzMix(h ^ 0x58u)) * 2.0f - 1.0f) * SS_JITTER * m->cell;
        p[2] += (SsU01(RzMix(h ^ 0x5Au)) * 2.0f - 1.0f) * SS_JITTER * m->cell;
        if (p[0] < m->lo[0]) p[0] = m->lo[0];  // (an edge cell's point stays in the room's box)
        if (p[0] > m->hi[0]) p[0] = m->hi[0];
        if (p[2] < m->lo[2]) p[2] = m->lo[2];
        if (p[2] > m->hi[2]) p[2] = m->hi[2];
        p[1] = (float)m->fy[c];
        if ((r = SsMonFree(p, in->arrive, in->exits, in->nExits, in->taken, in->nTaken)) != SSW_OK ||
            (r = test ? test(ctx, p) : SSW_OK) != SSW_OK || !SsFinite3(p) ||
            (r = SsMonFree(p, in->arrive, in->exits, in->nExits, in->taken, in->nTaken)) != SSW_OK) {  // (the floor moved it)
            if (why && r > 0 && r < SSW_N) ++why[r];
            continue;
        }
        out[0] = p[0], out[1] = p[1], out[2] = p[2];
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- ghosts
// floorAt(ctx, x, y0, z, fy): the first floor at or below y0 (y grows down); 1 = found
typedef int (*SsFloorFn)(void *ctx, float x, float y0, float z, float *fy);
typedef struct {
    unsigned key;               // SsKey(..., 1) (a victim) or (..., 0) (a monster made a ghost)
    float lo[3], hi[3];         // the room's collision box (lo < hi on every axis)
    float hover;                // the ghost's own height above its floor (its stock floor y - its stock y; >= 0)
    const float *base;          // its stock spot (a ghost placement equal to it is its own: skipped)
    const float *arrive;        // where you arrive (NULL = not known)
    const float (*ghosts)[3];   // the room's ghost placements (and pinned / down records' spots)
    int nGhosts;
    const float (*taken)[3];    // this room load's sites so far
    int nTaken;
} SsGhostIn;
static int SsInBox(const float *lo, const float *hi, const float *p)
{
    return p[0] >= lo[0] && p[0] <= hi[0] && p[1] >= lo[1] && p[1] <= hi[1] && p[2] >= lo[2] && p[2] <= hi[2];
}
static int SsGhostFree(const SsGhostIn *in, const float *c)
{
    int i;
    if (in->arrive && SsD2(c, in->arrive) < SS_GHOST_ENTRY * SS_GHOST_ENTRY) return SSW_ENTRY;
    for (i = 0; i < in->nGhosts; ++i)
        if (!(in->base && in->ghosts[i][0] == in->base[0] && in->ghosts[i][2] == in->base[2]) &&
            SsD2(c, in->ghosts[i]) < SS_GHOST_GHOST * SS_GHOST_GHOST)
            return SSW_GHOST;
    for (i = 0; i < in->nTaken; ++i)
        if (SsD2(c, in->taken[i]) < SS_GHOST_OTHER * SS_GHOST_OTHER) return SSW_OTHER;
    return SSW_OK;
}
// the candidate t: x / z uniform inside the box (SS_BOX_IN from its sides; its middle when it is narrower), the height to
// look down from uniform over the box's height
static void SsGhostCand(const SsGhostIn *in, int t, float *x, float *y0, float *z)
{
    const unsigned h = RzHash4(in->key, 0x47485354u, (unsigned)t, 0x424F5800u);
    float a[3], b[3];
    int k;
    for (k = 0; k < 3; ++k) {
        const float pad = k == 1 ? 0.0f : SS_BOX_IN;
        a[k] = in->lo[k] + pad, b[k] = in->hi[k] - pad;
        if (b[k] < a[k]) a[k] = b[k] = (in->lo[k] + in->hi[k]) * 0.5f;
    }
    *x = a[0] + (b[0] - a[0]) * SsU01(RzMix(h ^ 0x11u));
    *y0 = a[1] + (b[1] - a[1]) * SsU01(RzMix(h ^ 0x22u));
    *z = a[2] + (b[2] - a[2]) * SsU01(RzMix(h ^ 0x33u));
}
// a site inside the box: a floor under the candidate (any storey), the ghost at its own height above it, inside the box,
// the distance rules, then test (not inside geometry). out = the ghost's point. 1 = found; why[SSW_N] may be NULL
static int SsGhostPick(const SsGhostIn *in, SsFloorFn floorAt, SsTestFn test, void *ctx, float *out, int *why)
{
    int t, k;
    for (k = 0; k < 3; ++k)
        if (!(in->hi[k] - in->lo[k] > 1.0f) || !(in->hi[k] - in->lo[k] < 1e6f)) return 0;
    for (t = 0; t < SS_GHOST_TRIES; ++t) {
        float p[3], y0, fy;
        int r = SSW_OK;
        SsGhostCand(in, t, &p[0], &y0, &p[2]);
        if (!floorAt || !floorAt(ctx, p[0], y0, p[2], &fy) || !(fy == fy) || fy < y0 - 1.0f) {
            r = SSW_FLOOR;
        } else {
            p[1] = fy - (in->hover > 0.0f && in->hover < 2000.0f ? in->hover : 0.0f);
            if (!SsInBox(in->lo, in->hi, p) || !SsFinite3(p)) r = SSW_BOX;
            else if ((r = SsGhostFree(in, p)) == SSW_OK && test) r = test(ctx, p);
        }
        if (r != SSW_OK) {
            if (why && r > 0 && r < SSW_N) ++why[r];
            continue;
        }
        out[0] = p[0], out[1] = p[1], out[2] = p[2];
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- which placements take a site
// a monster source keeps its place: an event dog before its release (its scene reads it), a Tremer with its room box
// (mode bits 12-15: its clamp 0x488DE0 would pull it back into the box) that stays a Tremer, a boss room, a placement
// whose record is in another room (not made here: the spawn gate's 5). 0 = it may move
enum { SSK_MOVE, SSK_EVENT, SSK_TREMERBOX, SSK_BOSS, SSK_AWAY, SSK_N };
static const char *const kSsKeep[SSK_N] = { "", "asleep for its event", "a Tremer kept in its room box", "a boss room",
                                            "recorded in another room" };
static int SsMonKeep(int stockType, unsigned stockMw, int event, int released, int pickType, int bossRoom, int away)
{
    if (bossRoom) return SSK_BOSS;
    if (away) return SSK_AWAY;
    if (event && !released) return SSK_EVENT;
    if (stockType == 0x09 && pickType == 0x09 && ((stockMw >> 12) & 0xFu)) return SSK_TREMERBOX;
    return SSK_MOVE;
}
// a monster record's state the site may move it in (a standing one: not down 9, not HP 0 0x0A, not dead 0x0B, not removed
// or scripted 0x10 +); 0 = free
static int SsRecStanding(int state) { return state > 0 && state != 9 && state != 0x0A && state != 0x0B && state < 0x10; }
#endif
