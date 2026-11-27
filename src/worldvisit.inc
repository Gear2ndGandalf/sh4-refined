// ================================================================ v3.56 MERGE: the world-visit rule (game side), ONE copy
// For Enemies Return (w4 ER356) and the Randomizer's Reroll Visits (w2 RI356; w3 RE356's Enemies / Spawn Site through
// RzVisitSalt). THE CONTRACT and the proof of the load signal are at the top of worldvisit_sim.h. Included by
// sh4windowmod.c before randomizer.inc (its reader; enemies.inc, which calls it, comes later). enemies.inc EnmWrap calls
// WvOnRoomLoad first at every room load (one call: its kind for Enemies Return, its visit for the Randomizer);
// EnmPoolAtReset calls WvWorldReset. No exe site of its own, nothing saved, no ini key. GOG v7 VAs, checked against the
// stock exe.
// Logs: one line per stage change into a new visit and per load's room (RI356's "visits:" lines, LogQuota 400 each; the
// load line names the game's signal), ER356's level-2 lines for a room-0 hold and leaving a world (400), one session line
// (loghealth.inc via randomizer.inc RzSessionLine).
#include "worldvisit_sim.h"

#define WV_MGR_STAGE (*(volatile const int *)0xFD0BA4)  // gEnemyManager.stage (worldvisit_sim.h: its writers)
static WvState g_wv;      // part 1: the kinds (ER356)
static WvVisits g_wvVis;  // part 2: the visits and room counts (RI356)
static int g_wvOn;        // initialised (else WvKind() is WV_FIRST)
static volatile LONG g_wvQuotaWorld, g_wvQuotaLoad, g_wvQuota;

static void WvInitOnce(void)
{
    LARGE_INTEGER q;
    if (g_wvOn) return;
    QueryPerformanceCounter(&q);
    WvReset(&g_wv);
    WvVisitsInit(&g_wvVis, WvMix(q.LowPart ^ GetTickCount() ^ (GetCurrentProcessId() << 16)));
    g_wvOn = 1;
}
// readers (game thread, after this room load's WvOnRoomLoad)
static int WvKind(void) { return g_wvOn ? g_wv.lastKind : WV_FIRST; }
static unsigned WvEntries(void) { return g_wv.entries; }
static unsigned WvEpoch(void) { return g_wv.epoch; }
static unsigned WvWorld(void) { return g_wvVis.world; }
static int WvStageNow(void) { return g_wvVis.stage; }
static unsigned WvRoomCount(int stage, int room) { return WvRoomOf(&g_wvVis, stage, room); }
static int WvLast(void) { return g_wvVis.lastFlags; }
static void WvWorldReset(void)  // enemies.inc EnmPoolAtReset (New Game, the title's Load)
{
    WvInitOnce();
    WvVisitsReset(&g_wvVis);
}
// enemies.inc EnmWrap, first (game thread; the caller's __try guards it). Returns this room load's kind (WV_*).
static int WvOnRoomLoad(int room)
{
    int stage, mgr, kind, f, prev;
    WvInitOnce();
    stage = (int)*(volatile const DWORD *)0xFD5A90;  // gGame.stage (the room being loaded)
    mgr = WV_MGR_STAGE;                              // read before 0x52CA00 writes it (a fixed address: always readable)
    prev = g_wv.lastStage;                           // (the stage of Refined's last room load: the log's "from")
    kind = WvRoomLoadBoth(&g_wv, &g_wvVis, mgr, stage, room, &f);
    if (room <= 0) {
        if (kind != WV_ROOM && LogQuota(&g_wvQuota, 400))
            LogV("world visit: a room load without a room (stage %d, %s): held for the next room", stage, kWvKindName[kind]);
        return kind;
    }
    if (f & WVF_IGNORED) return kind;
    if (f & WVF_WORLD_ENTRY) {
        if (LogQuota(&g_wvQuotaWorld, 400))
            Log("visits: world visit %08X begins: stage %d entered (room %d; world entry %u of this session%s)", g_wvVis.world, stage,
                room, g_wvVis.worldEntries, (f & WVF_RESET_SEEN) ? ", after a New Game / title load" : "");
    } else if (f & WVF_LOAD) {
        if (LogQuota(&g_wvQuotaLoad, 400))
            Log("visits: room %d:%d after a load (%s): no room or world entry; world visit %08X %s (room entries %u)", stage, room,
                "a save applied: the enemy manager's stage -2", g_wvVis.world, (f & WVF_RESUMED) ? "resumed (this stage's last visit this session)" :
                (f & WVF_UNKNOWN) ? "= the seed's own (no visit of this stage this session)" : "goes on", WvRoomOf(&g_wvVis, stage, room));
    }
    if (kind == WV_OUT && WvIsWorld(prev) && LogQuota(&g_wvQuota, 400))
        LogV("world visit: left world %d for stage %d room %d", prev, stage, room);
    return kind;
}
static void WvSessionLine(void)  // randomizer.inc RzSessionLine
{
    Log("visits: session: %u room entr%s, %u world visit(s), %u load(s) (%u resumed a stage's visit, %u the seed's own), %u world "
        "reset(s), %u room load(s) ignored (room 0)", g_wvVis.entries, g_wvVis.entries == 1 ? "y" : "ies", g_wvVis.worldEntries,
        g_wvVis.loads, g_wvVis.resumes, g_wvVis.unknowns, g_wvVis.resets, g_wvVis.ignored);
}
