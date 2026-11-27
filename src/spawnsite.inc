// ================================================================ v3.56 RE356: the Randomizer's Spawn Site (Option 5 > Randomizer)
// Plan: staging/v356_feedback/w3_rando_enemies/PLAN.md (the user's v3.55 improvement 15). The row is rzlooks.inc's (it was
// Ghosts Off / Spots: [Randomizer] SpawnSite Off / On, the old Ghosts value read once); the pure part is spawnsite_sim.h.
// Included by sh4windowmod.c after teleport.inc (it uses Ghost Teleport's floor map GtMap, its collision queries g_gtWorld
// and their byte checks) and before rzmon.inc (which calls it for the monster placements); rzlooks.inc calls it for the
// victims (declared there).
//
// At a room load of stages 2..8 (randomizer.inc RzBeforeScript, before the stock runner reads the room's script), with
// Spawn Site On:
// - every monster source of the room (rzmon.inc: the same placements Enemies rolls, after its roll) starts at a site of
//   the room: a cell of this room load's floor map (flooded from every door's point and the arrival point by the player's
//   walk rules, teleport_sim.h GtMap; its own map, not Ghost Teleport's), not inside geometry (0x46F700), on the floor the
//   game's floor ray finds there (0x459490, within 60), with room to stand (0x458F00), the player's body spheres free,
//   at least 800 from where you arrive, 400 from every door's point and 250 from the room's other sites. A walker stands
//   on that floor, a Hummer at its own height above it, a monster Enemies made a ghost takes a ghost's site;
// - every victim (rzlooks.inc: 0B, 0C v1, 0D v1 in spawn mode 0 with its FFFF00nn id) takes a ghost's site: anywhere in
//   the room's collision box (0x561A70) in x / z (150 in from its sides) and y (a height in the box, the first floor under
//   it, the victim's own height above that floor), inside the box, not inside geometry, 600 from where you arrive, 600
//   from another ghost's placement, 400 from another site. Without a box: v3.50's rings (rzlooks_sim.h RzgPick2).
// The x / y / z words of each placement are written with its type words (patch-and-restore around the runner); a standing
// record of it in this room is moved to the site too (else the game's record puts it back). Nothing is saved. Kept as
// placed: event dogs before story flag 0x130, a Tremer with its room box, boss rooms, no map / no site (the reasons are
// logged). The site's key: the seed, the stage and half, the room, the object number and the room's visit salt (the
// Randomizer's Reroll rule).
#include "spawnsite_sim.h"

static int g_ssFnState;             // 0 not checked, 1 the game functions are stock, -1 not (Spawn Site off for the session)
static int g_ssFail;                // faulted: off for this session
static int g_ssBegun;               // this room load's room state is read (SsRoomRead)
static int g_ssBoxOk, g_ssExitN, g_ssMapState;  // map state: 0 not built, 1 built, -1 none (no tree2 / no box / not stock)
static float g_ssBoxLo[3], g_ssBoxHi[3], g_ssArrive[3];
static GtExit g_ssExit[GT_EXIT_MAX];
static GtMap g_ssMap;
static unsigned short g_ssCells[GT_MAP_DIM * GT_MAP_DIM];
static int g_ssNCells;
static double g_ssMapMs;
static float g_ssTaken[SS_TAKEN_MAX][3];
static int g_ssTakenN;
static RzgRoom g_ssGhostsHere;      // the room's ghost placements (rzlooks_sim.h RzgScanRoom, read before any word is ours)
static int g_ssGhostsOk;
// this room load's monster counts (SsMonRoomSay) and the session's
static int g_ssRmN, g_ssRmMoved, g_ssRmKept, g_ssRmRec, g_ssRmWhy[SSW_N], g_ssRmKeep[SSK_N];
static int g_ssRooms, g_ssMoved, g_ssKept, g_ssRecMoved, g_ssMaps, g_ssMapCells, g_ssNoMap, g_ssGhostMoved, g_ssGhostBox;
static double g_ssMapMsAll;
static volatile LONG g_ssQuotaRoom, g_ssQuotaAnom;
static int g_ssNoMapSaid;

static int SsOn(void)  // rzlooks.inc's Spawn Site value (g_rzgMode), not faulted
{
    return g_rzgMode == RZG_SPOTS && !g_ssFail && g_ssFnState >= 0;
}
static int SsFnsOk(void)  // the game functions the sites call (once a session; rzlooks.inc kRzlFns and teleport.inc kFn's)
{
    static const Site kRoom[] = {
        { 0x4FA040, { 0x55, 0x8B, 0xEC, 0xA1, 0x90, 0x5A, 0xFD, 0x00 }, 8 },  // ExitGetNum
        { 0x4FA070, { 0x55, 0x8B, 0xEC, 0xA1, 0x90, 0x5A, 0xFD, 0x00 }, 8 },  // ExitGet
        { 0x561A70, { 0x8B, 0x15, 0x74, 0x6D, 0x2F, 0x01, 0x85, 0xD2 }, 8 } };  // the room's collision box
    int i, ok;
    if (g_ssFnState) return g_ssFnState > 0;
    ok = RzlFnsOk();
    for (i = 0; i < (int)(sizeof kRoom / sizeof kRoom[0]); ++i)
        if (!SiteOk(&kRoom[i])) {
            Log("randomizer: spawn site: %08lX is not the game's code (another mod?): Spawn Site is off for this session", kRoom[i].va);
            ok = 0;
        }
    g_ssFnState = ok ? 1 : -1;
    return ok;
}
static void SsLoadBegin(void)  // rzmon.inc RmBeforeScript: a room-script run begins (the room state is read when first needed)
{
    if (!g_ssFnState) SsFnsOk();  // (once, at the first room load: game thread)
    g_ssBegun = 0, g_ssTakenN = 0, g_ssMapState = 0, g_ssGhostsOk = 0;
    g_ssRmN = g_ssRmMoved = g_ssRmKept = g_ssRmRec = 0;
    memset(g_ssRmWhy, 0, sizeof g_ssRmWhy), memset(g_ssRmKeep, 0, sizeof g_ssRmKeep);
}
// the room's box, its doors and the arrival point (teleport.inc GtRoomRead's reads, into our own copies)
static void SsRoomRead(int room)
{
    __declspec(align(16)) float box[8];
    int i, n, k;
    if (g_ssBegun) return;
    g_ssBegun = 1;
    g_ssExitN = 0, g_ssBoxOk = 0;
    EnmPlayerPos(g_ssArrive, 1);
    n = room >= 1 && room < 0x100 ? ((int(__cdecl *)(unsigned))0x4FA040)((unsigned)room) : 0;
    for (i = 0; i < n && g_ssExitN < GT_EXIT_MAX; ++i) {
        const BYTE *x = ((const BYTE *(__cdecl *)(unsigned, unsigned))0x4FA070)((unsigned)room, (unsigned)i);
        GtExit *e = &g_ssExit[g_ssExitN];
        if (!x) continue;
        e->x0 = *(const float *)(x + 0x00), e->z0 = *(const float *)(x + 0x08);
        e->x1 = *(const float *)(x + 0x10), e->y = *(const float *)(x + 0x14), e->z1 = *(const float *)(x + 0x18);
        e->px = *(const float *)(x + 0x20), e->pz = *(const float *)(x + 0x28);
        if (_finite(e->x0) && _finite(e->z0) && _finite(e->x1) && _finite(e->z1) && _finite(e->y) && _finite(e->px) && _finite(e->pz))
            ++g_ssExitN;
    }
    for (k = 0; k < 8; ++k) box[k] = 1e30f;
    ((void(__cdecl *)(float *))0x561A70)(box);
    g_ssBoxOk = box[0] < 1e29f && box[4] < 1e29f;
    for (k = 0; k < 3 && g_ssBoxOk; ++k) {
        const float a = box[4 + k], b = box[k];
        g_ssBoxLo[k] = a < b ? a : b, g_ssBoxHi[k] = a < b ? b : a;
        if (!(g_ssBoxHi[k] - g_ssBoxLo[k] > 1.0f) || !(g_ssBoxHi[k] - g_ssBoxLo[k] < 1e6f)) g_ssBoxOk = 0;
    }
    {
        const unsigned start = ((const DWORD *)0x10A0A78)[room > 0 && room < RZ_ROOMS ? room : 0];
        g_ssGhostsOk = start && RzgScanRoom(RzWindow(), start, &g_ssGhostsHere);
    }
}
#define SS_MAP_MS 40.0  // a room load's floor map gets at most this long
static int SsMapReady(void)  // this room load's floor map (built once; 1 = cells to pick from)
{
    LARGE_INTEGER a, b, f;
    int k, guard = 0;
    if (g_ssMapState) return g_ssMapState > 0 && g_ssNCells > 0;
    g_ssMapState = -1, g_ssNCells = 0, g_ssMapMs = 0.0;
    if (!g_gtWorldOk || !NwHasTree2() || !g_ssBoxOk) {
        ++g_ssNoMap;
        if (!g_ssNoMapSaid++)
            Log("randomizer: spawn site: no floor map in room %d:%d (%s): its monsters keep their spots (said once)", ENM_STAGE,
                ENM_ROOM, !g_gtWorldOk ? "the collision queries are not stock" : !g_ssBoxOk ? "no collision box" : "no solid map");
        return 0;
    }
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    GtMapBegin(&g_ssMap, g_ssBoxLo, g_ssBoxHi, g_ssExit, g_ssExitN);
    for (k = 0; k < g_ssExitN; ++k) GtMapSeed(&g_ssMap, &g_gtWorld, g_ssExit[k].px, g_ssExit[k].y, g_ssExit[k].pz, GT_MAP_SEED_DY);
    GtMapSeed(&g_ssMap, &g_gtWorld, g_ssArrive[0], g_ssArrive[1], g_ssArrive[2], GT_REACH_UP);  // where you arrive is floor
    while (g_ssMap.on && !GtMapFinished(&g_ssMap) && !g_ssMap.overflow && g_ssMap.head != g_ssMap.tail && ++guard < 4096) {
        GtMapRun(&g_ssMap, &g_gtWorld, 1024);
        QueryPerformanceCounter(&b);
        g_ssMapMs = f.QuadPart ? (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart : 0.0;
        if (g_ssMapMs > SS_MAP_MS) break;  // (an unfinished map's reached cells are still proven floor)
    }
    QueryPerformanceCounter(&b);
    g_ssMapMs = f.QuadPart ? (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart : 0.0;
    g_ssNCells = SsMapCells(&g_ssMap, g_ssCells, (int)(sizeof g_ssCells / sizeof g_ssCells[0]));
    g_ssMapState = g_ssMap.on ? 1 : -1;
    ++g_ssMaps, g_ssMapCells += g_ssNCells, g_ssMapMsAll += g_ssMapMs;
    return g_ssMapState > 0 && g_ssNCells > 0;
}
// the game's checks at a monster's candidate (spawnsite_sim.h SsTestFn): its floor, not inside geometry, room to stand,
// the player's body spheres free; cand[1] = the floor the game finds
static int SsMonTest(void *ctx, float *cand)
{
    __declspec(align(16)) float p[4];
    __declspec(align(16)) EnmFloorHit hit;
    (void)ctx;
    memset(&hit, 0, sizeof hit);
    p[0] = cand[0], p[1] = cand[1] - SS_FLOOR_DY, p[2] = cand[2], p[3] = 1.0f;
    if (!((int(__cdecl *)(float *))0x46F700)(p)) return SSW_WALL;                                     // inside geometry
    if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) != 1 || !_finite(hit.pos[0]) || !_finite(hit.pos[1]) ||
        !_finite(hit.pos[2]) || !(fabsf(hit.pos[1] - cand[1]) <= SS_FLOOR_DY))
        return SSW_FLOOR;
    if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hit.pos, hit.normal, 60.0f, 1.0f) != 2) return SSW_ROOM;
    if (!GtBodyFree(&g_gtWorld, hit.pos[0], hit.pos[1], hit.pos[2])) return SSW_BODY;
    cand[0] = hit.pos[0], cand[1] = hit.pos[1], cand[2] = hit.pos[2];
    return SSW_OK;
}
// a ghost's: the first floor at or below y0 (EnemyFloorRayCheck from just above it) and not inside geometry
static int SsGhostFloor(void *ctx, float x, float y0, float z, float *fy)
{
    const float p[3] = { x, y0, z };
    (void)ctx;
    return RzlFloorAt(p, fy);
}
static int SsGhostTest(void *ctx, float *cand)
{
    __declspec(align(16)) float p[4];
    (void)ctx;
    p[0] = cand[0], p[1] = cand[1], p[2] = cand[2], p[3] = 1.0f;
    return ((int(__cdecl *)(float *))0x46F700)(p) ? SSW_OK : SSW_WALL;
}
static void SsTake(const float *p)
{
    if (g_ssTakenN < SS_TAKEN_MAX) memcpy(g_ssTaken[g_ssTakenN++], p, 12);
}
// a ghost's site in this room (the victims of rzlooks.inc and the monsters Enemies made ghosts). base: its stock spot;
// hover: its own height above its floor. 1 = out holds the site
static int SsGhostSite(int stage, int half, int room, int n, int kind, const float *base, float hover, const float (*ghosts)[3],
                       int nGhosts, float *out, int *why)
{
    SsGhostIn in;
    const unsigned visit = RzVisitSalt(stage, room);  // v3.56 MERGE: the one world-visit rule (was v3.55 per-visit room count)
    SsRoomRead(room);
    if (!g_ssBoxOk) return 0;
    memset(&in, 0, sizeof in);
    in.key = SsKey((unsigned)g_rzSeed, stage, half, room, n, visit, kind);
    memcpy(in.lo, g_ssBoxLo, 12), memcpy(in.hi, g_ssBoxHi, 12);
    in.hover = hover, in.base = base, in.arrive = g_ssArrive;
    in.ghosts = ghosts, in.nGhosts = nGhosts;
    in.taken = (const float(*)[3])g_ssTaken, in.nTaken = g_ssTakenN;
    if (!SsGhostPick(&in, SsGhostFloor, SsGhostTest, NULL, out, why)) return 0;
    SsTake(out);
    ++g_ssGhostBox;
    return 1;
}
// a monster placement's site (rzmon.inc RmBeforeScriptC), made as (type, variant). hover: a Hummer's own height above its
// floor (0 = on the floor). 1 = site holds it; else *keep (SSK_*) or *noSite = 1
static int SsMonSite(int stage, int half, int room, const RmSlot *s, int type, int bossRoom, int released, int away, float hover,
                     float *site, int *keep)
{
    const unsigned visit = RzVisitSalt(stage, room);  // v3.56 MERGE: the one world-visit rule (was v3.55 per-visit room count)
    int ok = 0;
    *keep = SsMonKeep(s->type, s->mw, s->event, released, type, bossRoom, away);
    ++g_ssRmN;
    if (*keep != SSK_MOVE) {
        ++g_ssRmKept, ++g_ssRmKeep[*keep];
        return 0;
    }
    SsRoomRead(room);
    if (RmIsGhostTarget(type)) {  // a monster made a ghost: a ghost's site (the room's ghosts kept apart)
        ok = SsGhostSite(stage, half, room, s->n, 0, s->pos, 0.0f, g_ssGhostsOk ? (const float(*)[3])g_ssGhostsHere.ghost : NULL,
                         g_ssGhostsOk ? g_ssGhostsHere.nghost : 0, site, g_ssRmWhy);
    } else if (SsMapReady()) {
        SsMonIn in;
        memset(&in, 0, sizeof in);
        in.key = SsKey((unsigned)g_rzSeed, stage, half, room, s->n, visit, 0);
        in.arrive = g_ssArrive, in.exits = g_ssExit, in.nExits = g_ssExitN;
        in.taken = (const float(*)[3])g_ssTaken, in.nTaken = g_ssTakenN;
        ok = SsMonPick(&in, &g_ssMap, g_ssCells, g_ssNCells, SsMonTest, NULL, site, g_ssRmWhy);
        if (ok && RmIsFlyer(type) && hover > 0.0f && hover < 2000.0f) {  // a Hummer at its own height (its flying point free)
            __declspec(align(16)) float p[4];
            p[0] = site[0], p[1] = site[1] - hover, p[2] = site[2], p[3] = 1.0f;
            if (((int(__cdecl *)(float *))0x46F700)(p)) site[1] = p[1];
        }
        if (ok) SsTake(site);
    }
    if (ok) ++g_ssRmMoved;
    else ++g_ssRmKept;
    return ok;
}
// a standing record of this placement in this room, of the type it is made as: moved to the site (world -> record,
// 0x46FE30; its w kept). 1 = moved
static int SsMonRecMove(unsigned id, int stage, int room, int type, int variant, const float *site)
{
    BYTE *r = ENM_RECORDS;
    int i, n = 0;
    for (i = 0; i < 64; ++i, r += 0x50) {
        __declspec(align(16)) float in[4], out[4];
        if (RmRecState(r) == 0 || RmRecId(r) != id || RmRecStage(r) != stage || RmRecRoom(r) != room) continue;
        if (RmRecType(r) != type || RmRecVariant(r) != variant || !SsRecStanding(RmRecState(r))) continue;
        in[0] = site[0], in[1] = site[1], in[2] = site[2], in[3] = *(const float *)(r + 0x0C);
        ENM_TO_REC(out, in);
        if (SsFinite3(out)) memcpy(r, out, 12), ++n;
    }
    g_ssRmRec += n;
    return n > 0;
}
static void SsMonRoomSay(int stage, int room)  // after rzmon.inc's words of this room load
{
    char why[200];
    int k, o = 0;
    if (!g_ssRmN) return;
    ++g_ssRooms;
    g_ssMoved += g_ssRmMoved, g_ssKept += g_ssRmKept, g_ssRecMoved += g_ssRmRec;
    why[0] = 0;
    for (k = 1; k < SSK_N + SSW_N - 1 && o < (int)sizeof why - 1; ++k) {  // the kept reasons, then the refused candidates
        const int n = k < SSK_N ? g_ssRmKeep[k] : g_ssRmWhy[k - SSK_N + 1];
        const char *name = k < SSK_N ? kSsKeep[k] : kSsWhy[k - SSK_N + 1];
        int w;
        if (!n) continue;
        w = _snprintf_s(why + o, sizeof why - o, _TRUNCATE, "%s%s %d", o ? ", " : "", name, n);
        o = w < 0 ? (int)sizeof why - 1 : o + w;  // (_TRUNCATE: a full list ends there)
    }
    if (LogQuota(&g_ssQuotaRoom, 4000))
        Log("randomizer: spawn site: room %d:%d (#%u): %d of %d monster(s) at a new site, %d record(s) moved with them; floor map %d "
            "cell(s) (%.1f ms)%s%s%s", stage, room, RzVisitSalt(stage, room) /* v3.56 MERGE */, g_ssRmMoved, g_ssRmN, g_ssRmRec,
            g_ssMapState > 0 ? g_ssNCells : 0, g_ssMapState > 0 ? g_ssMapMs : 0.0, o ? " (kept or refused: " : "", why, o ? ")" : "");
}
static void SsFault(DWORD code)
{
    g_ssFail = 1;
    if (LogQuota(&g_ssQuotaAnom, 20))
        LogAnomaly("randomizer", "spawn site: faulted at a room load (code %08lX): Spawn Site is off for this session", code);
}
static void SsSessionLine(void)  // rzlooks.inc RzlSessionLine
{
    Log("randomizer: spawn site session: %s: %d room load(s) with monsters, %d monster(s) at a new site, %d kept, %d record(s) moved; "
        "%d floor map(s) (%d cell(s), %.1f ms in all), %d room load(s) without a map; %d ghost(s) at a site in the room's box%s",
        kRzgName[g_rzgMode ? 1 : 0], g_ssRooms, g_ssMoved, g_ssKept, g_ssRecMoved, g_ssMaps, g_ssMapCells, g_ssMapMsAll, g_ssNoMap,
        g_ssGhostBox, g_ssFail ? "; FAULTED (off)" : g_ssFnState < 0 ? "; the game functions are not stock (off)" : "");
}
