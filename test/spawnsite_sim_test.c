// v3.56 RE356: native test of src\spawnsite_sim.h (test\build_spawnsite_test.cmd): the Randomizer's Spawn Site. On made-up
// rooms (a box world like the game's, y grows downward, as test\gt_sim_test.c): a monster's site is a cell of the room's
// floor map (flooded from its door and the arrival point), never in a closed space, never within 800 of the arrival point
// or 400 of a door, 250 apart; a ghost's site is anywhere in the room's box in x / y / z (both storeys), at its own height
// above the floor under it, never within 600 of the arrival point or of another ghost; the keys (seed, visit), the
// placements kept as placed, the record states, the Ghosts -> Spawn Site values (Spots reads as On).
// Usage: out\spawnsite_sim_test.exe
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/spawnsite_sim.h"
#include "../src/rzlooks_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- a box world (the ground's top at y 0, solid boxes)
typedef struct { float x0, x1, y0, y1, z0, z1; } TBox;  // solid where x0 < x < x1, y0 < y < y1 (y0 the top), z0 < z < z1
typedef struct { TBox b[16]; int nb; } TWorld;
static int TSolidAt(const TWorld *t, float x, float y, float z)
{
    int i;
    if (y > 0.0f) return 1;
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        if (x > b->x0 && x < b->x1 && y > b->y0 && y < b->y1 && z > b->z0 && z < b->z1) return 1;
    }
    return 0;
}
static int TSolid(void *c, float x, float y, float z) { return TSolidAt((const TWorld *)c, x, y, z); }
static int TFloor(void *c, float x, float yTop, float yBot, float z, float *fy)  // the first surface below yTop
{
    const TWorld *t = (const TWorld *)c;
    float best = 0.0f;
    int i;
    if (TSolidAt(t, x, yTop, z)) return 0;
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        if (x > b->x0 && x < b->x1 && z > b->z0 && z < b->z1 && b->y0 > yTop && b->y0 < best) best = b->y0;
    }
    if (!(best > yTop && best <= yBot)) return 0;
    *fy = best;
    return 1;
}
static float TClamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static int TSphere(void *c, float x, float y, float z, float r)
{
    const TWorld *t = (const TWorld *)c;
    int i, n = 0;
    if (y + r > 0.0f) ++n;
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        const float dx = x - TClamp(x, b->x0, b->x1), dy = y - TClamp(y, b->y0, b->y1), dz = z - TClamp(z, b->z0, b->z1);
        n += dx * dx + dy * dy + dz * dz < r * r;
    }
    return n;
}
static int TLine(void *c, const float a[3], const float b[3])
{
    const TWorld *t = (const TWorld *)c;
    const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2], l = sqrtf(dx * dx + dy * dy + dz * dz);
    const int n = (int)(l / 4.0f) + 1;
    int i;
    for (i = 0; i <= n; ++i) {
        const float u = (float)i / (float)n;
        if (TSolidAt(t, a[0] + dx * u, a[1] + dy * u, a[2] + dz * u)) return 1;
    }
    return 0;
}
static void TWall(TWorld *t, float x0, float x1, float z0, float z1, float h)
{
    TBox *b = &t->b[t->nb++];
    b->x0 = x0, b->x1 = x1, b->z0 = z0, b->z1 = z1, b->y0 = -h, b->y1 = 0.0f;
}
static TWorld g_w;
static GtWorld g_gw;
static GtMap g_map;
static unsigned short g_cells[GT_MAP_DIM * GT_MAP_DIM];
// the game's checks (spawnsite.inc SsMonTest) on the box world: a floor within SS_FLOOR_DY, not inside solid, the body free
static int TMonTest(void *c, float *cand)
{
    float fy;
    (void)c;
    if (TSolidAt(&g_w, cand[0], cand[1] - SS_FLOOR_DY, cand[2])) return SSW_WALL;
    if (!TFloor(&g_w, cand[0], cand[1] - SS_FLOOR_DY, cand[1] + SS_FLOOR_DY, cand[2], &fy)) return SSW_FLOOR;
    if (!GtBodyFree(&g_gw, cand[0], fy, cand[2])) return SSW_BODY;
    cand[1] = fy;
    return SSW_OK;
}

// ---------------------------------------------------------------- monsters
static void TestMonsters(void)
{
    const float lo[3] = { -3000.0f, -2000.0f, -3000.0f }, hi[3] = { 3000.0f, 2000.0f, 3000.0f };
    const float arrive[3] = { 0.0f, 0.0f, -2750.0f };
    GtExit door;
    SsMonIn in;
    float out[3], first[3], taken[SS_TAKEN_MAX][3];
    int why[SSW_N], k, n, picks = 0, okIn = 1, okEntry = 1, okDoor = 1, okClosed = 1, okFloor = 1, okCell = 1, okOther = 1, nt = 0;
    memset(&g_w, 0, sizeof g_w);
    // a closed room (4 walls, 1000 high) at x 1500 .. 2500, z 1500 .. 2500: floor nobody can walk to
    TWall(&g_w, 1500.0f, 2500.0f, 1500.0f, 1540.0f, 1000.0f), TWall(&g_w, 1500.0f, 2500.0f, 2460.0f, 2500.0f, 1000.0f);
    TWall(&g_w, 1500.0f, 1540.0f, 1500.0f, 2500.0f, 1000.0f), TWall(&g_w, 2460.0f, 2500.0f, 1500.0f, 2500.0f, 1000.0f);
    // a pillar in the middle (no site inside it)
    TWall(&g_w, -200.0f, 200.0f, -200.0f, 200.0f, 1000.0f);
    g_gw.ctx = &g_w, g_gw.floorAt = TFloor, g_gw.sphere = TSphere, g_gw.line = TLine, g_gw.solid = TSolid;
    memset(&door, 0, sizeof door);
    door.x0 = -100.0f, door.x1 = 100.0f, door.z0 = door.z1 = -3000.0f, door.y = 0.0f, door.px = 0.0f, door.pz = -2850.0f;
    GtMapBegin(&g_map, lo, hi, &door, 1);
    GtMapSeed(&g_map, &g_gw, door.px, door.y, door.pz, GT_MAP_SEED_DY);
    GtMapSeed(&g_map, &g_gw, arrive[0], arrive[1], arrive[2], GT_REACH_UP);
    for (k = 0; k < 200 && !GtMapFinished(&g_map) && !g_map.overflow; ++k) GtMapRun(&g_map, &g_gw, 4096);
    n = SsMapCells(&g_map, g_cells, (int)(sizeof g_cells / sizeof g_cells[0]));
    CHECK(GtMapFinished(&g_map) && n > 5000, "the room's floor map: %d reached cell(s), finished", n);
    {   // the closed room's floor is never reached
        int c, inClosed = 0;
        for (c = 0; c < n; ++c) {
            float x, z;
            GtMapXZ(&g_map, g_cells[c], &x, &z);
            inClosed += x > 1540.0f && x < 2460.0f && z > 1540.0f && z < 2460.0f;
        }
        CHECK(!inClosed, "no reached cell inside the closed room (%d)", inClosed);
    }
    memset(why, 0, sizeof why);
    for (k = 0; k < 400; ++k) {
        memset(&in, 0, sizeof in);
        in.key = SsKey(12345u, 4, 1, 21, k, 0, 0);
        in.arrive = arrive, in.exits = &door, in.nExits = 1;
        if (!SsMonPick(&in, &g_map, g_cells, n, TMonTest, NULL, out, why)) continue;
        ++picks;
        okIn &= out[0] >= lo[0] && out[0] <= hi[0] && out[2] >= lo[2] && out[2] <= hi[2];
        okEntry &= SsD2(out, arrive) >= SS_MIN_ENTRY * SS_MIN_ENTRY;
        okDoor &= (out[0] - door.px) * (out[0] - door.px) + (out[2] - door.pz) * (out[2] - door.pz) >= SS_MIN_DOOR * SS_MIN_DOOR;
        okClosed &= !(out[0] > 1500.0f && out[0] < 2500.0f && out[2] > 1500.0f && out[2] < 2500.0f) &&
                    !(out[0] > -200.0f && out[0] < 200.0f && out[2] > -200.0f && out[2] < 200.0f);
        okFloor &= out[1] == 0.0f && GtBodyFree(&g_gw, out[0], out[1], out[2]);
        okCell &= GtMapHas(&g_map, &g_gw, out) == GTM_HAS;
    }
    CHECK(picks == 400, "every monster found a site (%d of 400)", picks);
    CHECK(okIn && okFloor && okCell, "a site is in the room, on its floor (the body free), on the floor map (%d %d %d)", okIn, okFloor, okCell);
    CHECK(okEntry && okDoor, "never within %.0f of where you arrive or %.0f of the door", SS_MIN_ENTRY, SS_MIN_DOOR);
    CHECK(okClosed, "never in the closed room or the pillar");
    CHECK(why[SSW_ENTRY] > 0, "near the arrival refused (%d)", why[SSW_ENTRY]);
    // the key decides: the same key the same site, another seed / visit / object another one
    memset(&in, 0, sizeof in);
    in.arrive = arrive, in.exits = &door, in.nExits = 1;
    in.key = SsKey(777u, 4, 1, 21, 3, 0, 0);
    CHECK(SsMonPick(&in, &g_map, g_cells, n, TMonTest, NULL, first, NULL), "a site for the key");
    CHECK(SsMonPick(&in, &g_map, g_cells, n, TMonTest, NULL, out, NULL) && !memcmp(out, first, 12), "the same key: the same site");
    {
        int diff = 0;
        for (k = 1; k <= 50; ++k) {
            in.key = SsKey(777u, 4, 1, 21, 3, (unsigned)k, 0);  // Reroll: the room's visit salt
            diff += SsMonPick(&in, &g_map, g_cells, n, TMonTest, NULL, out, NULL) && SsD2(out, first) > 100.0f * 100.0f;
        }
        CHECK(diff >= 45, "another visit: another site (%d of 50)", diff);
        CHECK(SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(2, 4, 1, 21, 3, 0, 0) && SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(1, 4, 0, 21, 3, 0, 0) &&
              SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(1, 4, 1, 22, 3, 0, 0) && SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(1, 4, 1, 21, 4, 0, 0) &&
              SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(1, 4, 1, 21, 3, 0, 1) && SsKey(1, 4, 1, 21, 3, 0, 0) != SsKey(1, 5, 1, 21, 3, 0, 0),
              "the key: seed, half, room, object, kind and stage each change it");
    }
    // the room's other sites: 250 apart
    for (k = 0; k < 20; ++k) {
        memset(&in, 0, sizeof in);
        in.key = SsKey(99u, 2, 0, 5, k, 0, 0);
        in.arrive = arrive, in.exits = &door, in.nExits = 1, in.taken = (const float(*)[3])taken, in.nTaken = nt;
        if (SsMonPick(&in, &g_map, g_cells, n, TMonTest, NULL, out, NULL)) {
            int j;
            for (j = 0; j < nt; ++j) okOther &= SsD2(out, taken[j]) >= SS_MIN_OTHER * SS_MIN_OTHER;
            memcpy(taken[nt++], out, 12);
        }
    }
    CHECK(nt == 20 && okOther, "20 sites in one room, each %.0f from the others (%d)", SS_MIN_OTHER, nt);
    // no cells / no map: no site
    memset(&in, 0, sizeof in);
    in.key = 1;
    CHECK(!SsMonPick(&in, &g_map, g_cells, 0, TMonTest, NULL, out, NULL), "no reached cell: no site");
    {
        static GtMap off;
        memset(&off, 0, sizeof off);
        CHECK(!SsMonPick(&in, &off, g_cells, n, TMonTest, NULL, out, NULL) && SsMapCells(&off, g_cells, 10) == 0, "no map: no site");
    }
    {   // a room where every cell is near the arrival point: no site (the placement keeps its spot)
        const float lo2[3] = { -300.0f, -2000.0f, -300.0f }, hi2[3] = { 300.0f, 2000.0f, 300.0f }, at[3] = { 0.0f, 0.0f, 0.0f };
        static GtMap small;
        static unsigned short c2[GT_MAP_DIM * GT_MAP_DIM];
        int n2, w2[SSW_N];
        memset(&g_w, 0, sizeof g_w);
        GtMapBegin(&small, lo2, hi2, NULL, 0);
        GtMapSeed(&small, &g_gw, at[0], at[1], at[2], GT_REACH_UP);
        GtMapRun(&small, &g_gw, 1 << 20);
        n2 = SsMapCells(&small, c2, (int)(sizeof c2 / sizeof c2[0]));
        memset(&in, 0, sizeof in), memset(w2, 0, sizeof w2);
        in.key = 5, in.arrive = at;
        CHECK(n2 > 10 && !SsMonPick(&in, &small, c2, n2, TMonTest, NULL, out, w2) && w2[SSW_ENTRY] == SS_TRIES,
              "a small room, all of it near you: no site, every try refused for the arrival (%d)", w2[SSW_ENTRY]);
    }
    // the distance rule alone
    {
        const float c1[3] = { 0.0f, 0.0f, 0.0f }, a1[3] = { 0.0f, 0.0f, 799.0f }, a2[3] = { 0.0f, 0.0f, 801.0f };
        GtExit d1;
        float tk[1][3] = { { 0.0f, 0.0f, 249.0f } };
        memset(&d1, 0, sizeof d1);
        d1.px = 399.0f, d1.pz = 0.0f;
        CHECK(SsMonFree(c1, a1, NULL, 0, NULL, 0) == SSW_ENTRY && SsMonFree(c1, a2, NULL, 0, NULL, 0) == SSW_OK &&
              SsMonFree(c1, NULL, &d1, 1, NULL, 0) == SSW_DOOR && SsMonFree(c1, NULL, NULL, 0, (const float(*)[3])tk, 1) == SSW_OTHER &&
              SsMonFree(c1, NULL, NULL, 0, NULL, 0) == SSW_OK, "the monsters' distances: entry 800, door 400, others 250");
    }
}

// ---------------------------------------------------------------- ghosts
static int g_mezz;  // a second storey: a floor at y -1000 over x > 0
static int GFloor(void *c, float x, float y0, float z, float *fy)
{
    (void)c, (void)z;
    if (g_mezz && x > 0.0f && y0 <= -1000.0f) return *fy = -1000.0f, 1;
    if (y0 <= 0.0f) return *fy = 0.0f, 1;
    return 0;
}
static int GNoFloor(void *c, float x, float y0, float z, float *fy) { (void)c, (void)x, (void)y0, (void)z, (void)fy; return 0; }
static int GWall(void *c, float *p) { (void)c; return p[0] > 2000.0f ? SSW_WALL : SSW_OK; }  // "inside geometry" past x 2000
static void TestGhosts(void)
{
    const float arrive[3] = { 0.0f, 0.0f, -2750.0f }, base[3] = { -1000.0f, -150.0f, 1000.0f };
    float ghosts[2][3] = { { -1000.0f, -150.0f, 1000.0f }, { 1500.0f, -150.0f, -1500.0f } };  // its own and another ghost
    float out[3], first[3], taken[8][3];
    SsGhostIn in;
    int k, picks = 0, upper = 0, lower = 0, okBox = 1, okY = 1, okEntry = 1, okGhost = 1, okWall = 1, okIn = 1, nt = 0, okOther = 1;
    int why[SSW_N], ownNear = 0;
    g_mezz = 1;
    memset(why, 0, sizeof why);
    for (k = 0; k < 400; ++k) {
        memset(&in, 0, sizeof in);
        in.key = SsKey(4242u, 2, 0, 7, k, 0, 1);
        in.lo[0] = -3000.0f, in.lo[1] = -2000.0f, in.lo[2] = -3000.0f, in.hi[0] = 3000.0f, in.hi[1] = 500.0f, in.hi[2] = 3000.0f;
        in.hover = 150.0f, in.base = base, in.arrive = arrive, in.ghosts = (const float(*)[3])ghosts, in.nGhosts = 2;
        if (!SsGhostPick(&in, GFloor, GWall, NULL, out, why)) continue;
        ++picks;
        okBox &= SsInBox(in.lo, in.hi, out);
        okIn &= out[0] >= -3000.0f + SS_BOX_IN && out[0] <= 3000.0f - SS_BOX_IN && out[2] >= -3000.0f + SS_BOX_IN &&
                out[2] <= 3000.0f - SS_BOX_IN;
        okY &= out[1] == -150.0f || (out[1] == -1150.0f && out[0] > 0.0f);
        upper += out[1] == -1150.0f, lower += out[1] == -150.0f;
        okEntry &= SsD2(out, arrive) >= SS_GHOST_ENTRY * SS_GHOST_ENTRY;
        okGhost &= SsD2(out, ghosts[1]) >= SS_GHOST_GHOST * SS_GHOST_GHOST;
        ownNear += SsD2(out, base) < SS_GHOST_GHOST * SS_GHOST_GHOST;
        okWall &= out[0] <= 2000.0f;
    }
    CHECK(picks == 400, "every ghost found a site (%d of 400)", picks);
    CHECK(okBox && okIn, "a ghost's site is inside the room's box, %.0f in from its sides", SS_BOX_IN);
    CHECK(okY && upper > 40 && lower > 40, "x / y / z: both storeys, at its own height above the floor under it (upper %d, lower %d)",
          upper, lower);
    CHECK(okEntry && okGhost, "never within %.0f of where you arrive or of another ghost", SS_GHOST_ENTRY);
    CHECK(ownNear > 0, "its own stock spot is no other ghost's (%d near it)", ownNear);
    CHECK(okWall && why[SSW_WALL] > 0, "inside geometry refused (%d)", why[SSW_WALL]);
    // the same key the same site; another visit another one
    memset(&in, 0, sizeof in);
    in.lo[0] = -3000.0f, in.lo[1] = -2000.0f, in.lo[2] = -3000.0f, in.hi[0] = 3000.0f, in.hi[1] = 500.0f, in.hi[2] = 3000.0f;
    in.hover = 150.0f, in.arrive = arrive;
    in.key = SsKey(1u, 2, 0, 7, 1, 0, 1);
    CHECK(SsGhostPick(&in, GFloor, NULL, NULL, first, NULL) && SsGhostPick(&in, GFloor, NULL, NULL, out, NULL) && !memcmp(out, first, 12),
          "the same key: the same ghost site");
    in.key = SsKey(1u, 2, 0, 7, 1, 1, 1);
    CHECK(SsGhostPick(&in, GFloor, NULL, NULL, out, NULL) && SsD2(out, first) > 1.0f, "another visit: another ghost site");
    // other sites of the room load: 400 apart
    for (k = 0; k < 8; ++k) {
        in.key = SsKey(3u, 5, 1, 13, k, 0, 1), in.taken = (const float(*)[3])taken, in.nTaken = nt;
        if (SsGhostPick(&in, GFloor, NULL, NULL, out, NULL)) {
            int j;
            for (j = 0; j < nt; ++j) okOther &= SsD2(out, taken[j]) >= SS_GHOST_OTHER * SS_GHOST_OTHER;
            memcpy(taken[nt++], out, 12);
        }
    }
    CHECK(nt == 8 && okOther, "8 ghosts in one room, %.0f apart", SS_GHOST_OTHER);
    // no floor anywhere, a flat box, a box narrower than the inset, a hover too high for the box
    in.taken = NULL, in.nTaken = 0;
    CHECK(!SsGhostPick(&in, GNoFloor, NULL, NULL, out, NULL), "no floor under any point: no site");
    in.hi[1] = in.lo[1];
    CHECK(!SsGhostPick(&in, GFloor, NULL, NULL, out, NULL), "a flat box: no site");
    in.lo[1] = -2000.0f, in.hi[1] = 500.0f, in.lo[0] = -100.0f, in.hi[0] = 100.0f, in.arrive = NULL;
    CHECK(SsGhostPick(&in, GFloor, NULL, NULL, out, NULL) && out[0] == 0.0f, "a box narrower than the inset: its middle in x");
    in.lo[0] = -3000.0f, in.hi[0] = 3000.0f, in.lo[1] = -100.0f, in.hover = 150.0f, g_mezz = 0;
    CHECK(!SsGhostPick(&in, GFloor, NULL, NULL, out, NULL), "its height above the floor would leave the box: no site");
}

// ---------------------------------------------------------------- the placements kept, the records, the values
static void TestRules(void)
{
    CHECK(SsMonKeep(0x02, 0, 0, 0, 0x02, 0, 0) == SSK_MOVE && SsMonKeep(0x0A, 7, 1, 0, 0x0A, 0, 0) == SSK_EVENT &&
          SsMonKeep(0x0A, 7, 1, 1, 0x0A, 0, 0) == SSK_MOVE && SsMonKeep(0x09, 0x3010, 0, 0, 0x09, 0, 0) == SSK_TREMERBOX &&
          SsMonKeep(0x09, 0x3010, 0, 0, 0x08, 0, 0) == SSK_MOVE && SsMonKeep(0x09, 0x0010, 0, 0, 0x09, 0, 0) == SSK_MOVE &&
          SsMonKeep(0x02, 0, 0, 0, 0x0B, 1, 0) == SSK_BOSS && SsMonKeep(0x02, 0, 0, 0, 0x02, 0, 1) == SSK_AWAY,
          "kept as placed: an event dog before its release, a Tremer in its room box that stays a Tremer, a boss room, one recorded "
          "in another room (not made here); else it moves");
    CHECK(SsRecStanding(1) && SsRecStanding(8) && !SsRecStanding(0) && !SsRecStanding(9) && !SsRecStanding(0x0A) && !SsRecStanding(0x0B) &&
          !SsRecStanding(0x10) && !SsRecStanding(0x12), "a record moves with its site only while it stands");
    CHECK(RzgOfText("Spots") == RZG_SPOTS && RzgOfText("On") == RZG_SPOTS && RzgOfText(" on ") == RZG_SPOTS && RzgOfText("Off") == RZG_OFF &&
          RzgOfText("1") == 1 && RzgOfText("0") == 0 && RzgOfText("Rooms") == -1 && !strcmp(kRzgName[1], "On") && !strcmp(kRzgName[0], "Off") &&
          RZG_DEFAULT == RZG_SPOTS, "Spawn Site Off / On (default On); the old Ghosts Spots reads as On");
    CHECK(SSW_N == 10 && !kSsWhy[SSW_OK][0] && kSsWhy[SSW_DOOR][0] && SSK_N == 5 && kSsKeep[SSK_AWAY][0], "the reasons' names");
}

int main(void)
{
    TestRules();
    TestMonsters();
    TestGhosts();
    printf("spawnsite_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
