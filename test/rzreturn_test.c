// v3.58 RZ358: native test (test\build_rzreturn_test.cmd) of the rule "a reroll redraws WHAT stands where, never WHETHER
// the dead are back" (staging/v358_feedback/USER_FEEDBACK_v357.md; the user: "Randomizer Reroll ignores Enemies Return
// setting and forces killed enemies to always respawn on room change when set to By Room"). It plays src\rzmon_sim.h's
// plan (RmPlanRoom with its dead rules, RmSaveKeepDead + RmSaveClean, RmCopyDraw) with src\enmreturn_sim.h (ErAction) and
// src\worldvisit_sim.h (the one world-visit rule) against a model of the game's enemy state (en_463950.c, matched): body
// records (state 0x0B) and standing records written at the room exit and by a save, kill bits, the spawn gate (a kill bit
// = never made; a record here = restored, its type must be the type made; else made fresh), the hole deleting a world's
// records on the way out, saves as whole copies and loads as the game applies them (gEnemyManager.stage -2).
// The oracle: two games run the same events in lockstep, one with Enemies Off (the stock types, no plan) and one with
// Enemies All Worlds; after every room load each placement (and each copy of Extra Enemies x2) stands in both or in
// neither. One difference is allowed and counted: Enemies Return Off, a randomized body kept dead in a save as its kill bit,
// that world left through its hole and entered again: the stock game would bring it back there, Refined keeps it dead
// (RmSaveKeepDead's note). The matrix: Enemies Return Off / By Room / By World x Reroll By Room / By World x kills, room
// changes, world entries (Room 302 and back), Quick Saves and Quick Loads, a restart and a load (Refined's memory empty).
// The model leaves out the types' own death rules (the Twin Victim's kill bit, the Tremer's coin flip: they are the same
// with or without the reroll) by drawing only from the monsters without one (no Twin Victim, no Tremer) and no ghost (a
// pin is no death).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rzmon_sim.h"
#include "../src/enmreturn_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; if (g_fail < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

#define STAGE 2
#define ROOMS 5     // rooms 1..4 of world 2
#define PER 4       // source placements a room
#define NREC 64
enum { T_STATE_NONE = 0, T_ALIVE = 1, T_BODY = 0x0B };

typedef struct { int state, type, variant, stage, room; short kill; unsigned id; } MRec;
typedef struct { int made, dead, type, variant; } Task;   // the current room's enemies (made = the gate made it)
typedef struct { MRec rec[NREC]; unsigned bits[32]; } Image;  // what a save holds
typedef struct {
    MRec rec[NREC];
    unsigned bits[32];
    Task task[PER], copy[PER];         // the room's placements and their copies (x2: one each)
    unsigned ring[ROOMS][PER];         // killed copies (the dead ring: stage / room / n)
    int stage, room, mgr, rz;          // rz = Enemies All Worlds (else Off)
    WvState wv;
    WvVisits vis;
    RmProv prov;
    Image save;
    int hasSave, saveStage, saveRoom;
    int saveBit[ROOMS][PER];           // (rz) a kill bit that came from RmSaveKeepDead and was loaded
} Game;

static RmScan g_sc;
static Game A, B;                    // A: Enemies Off, B: Enemies All Worlds
static int g_ret, g_reroll;          // Enemies Return, Reroll (RZR: 0 By Room, 1 By World)
static unsigned g_seed = 1398963150u;
static unsigned g_rng = 7;
static int g_deviation, g_mismatch, g_bodiesKept, g_copiesDrawn, g_copiesOther, g_loadsChecked, g_roomLoads;
static int Rand(int n) { g_rng = g_rng * 1103515245u + 12345u; return (int)((g_rng >> 8) % (unsigned)n); }

static const unsigned char kStock[PER] = { 0x0A, 0x02, 0x0A, 0x03 };  // Sniffer Dog, Whitestool, Sniffer Dog, Hummer
static void BuildScan(void)
{
    int r, i;
    memset(&g_sc, 0, sizeof g_sc);
    for (r = 1; r < ROOMS; ++r)
        for (i = 0; i < PER; ++i) {
            RmSlot *s = &g_sc.s[g_sc.n++];
            s->room = (unsigned short)r, s->n = (unsigned short)(i + 1), s->type = kStock[i], s->variant = 0, s->mw = 0;
            s->kill = (short)(r * 8 + i + 1);
            s->pos[0] = 100.0f * (float)i, s->pos[1] = 0.0f, s->pos[2] = 50.0f * (float)r;
            g_sc.roomCost[r] += RmPlaceCost(s->type);
            ++g_sc.roomN[r];
        }
    g_sc.placed = (1u << RmTargetIndex(0x0A, 0)) | (1u << RmTargetIndex(0x02, 0)) | (1u << RmTargetIndex(0x03, 0));
}
static int SlotOf(int r, int i) { return (r - 1) * PER + i; }
static unsigned IdOf(int r, int i) { return EnmScriptId(STAGE, 0, r, i + 1); }
static MRec *Find(Game *g, unsigned id)
{
    int k;
    for (k = 0; k < NREC; ++k)
        if (g->rec[k].state && g->rec[k].id == id) return &g->rec[k];
    return NULL;
}
static MRec *Alloc(Game *g)
{
    int k;
    for (k = 0; k < NREC; ++k)
        if (!g->rec[k].state) return &g->rec[k];
    return NULL;  // (64 is far more than the model's 16)
}
static int BitOf(int r, int i, int *w, unsigned *m) { return EnmKillBit(STAGE, g_sc.s[SlotOf(r, i)].kill, w, m); }
static int KillTest(Game *g, int r, int i) { int w; unsigned m; return BitOf(r, i, &w, &m) && (g->bits[w] & m); }
static void KillPut(Game *g, int r, int i, int on)
{
    int w;
    unsigned m;
    if (!BitOf(r, i, &w, &m)) return;
    if (on) g->bits[w] |= m; else g->bits[w] &= ~m;
}
// the game's records as rzmon_sim.h reads them (RmProvOurs / RmSaveKeepDead / RmSaveClean read raw records)
static void ToRaw(const MRec *m, unsigned char *r)
{
    short t = (short)m->type, st = (short)m->state;
    memset(r, 0, RM_REC_SIZE);
    if (!m->state) return;
    memcpy(r + 0x20, &m->id, 4), memcpy(r + 0x24, &t, 2), memcpy(r + 0x26, &st, 2), memcpy(r + 0x30, &m->kill, 2);
    r[0x33] = (unsigned char)m->variant, r[0x36] = (unsigned char)m->stage, r[0x37] = (unsigned char)m->room;
}
static void FromRaw(const unsigned char *r, MRec *m)
{
    short t, st, k;
    memcpy(&st, r + 0x26, 2);
    memset(m, 0, sizeof *m);
    if (!st) return;
    memcpy(&m->id, r + 0x20, 4), memcpy(&t, r + 0x24, 2), memcpy(&k, r + 0x30, 2);
    m->state = st, m->type = t, m->kill = k, m->variant = (signed char)r[0x33], m->stage = (signed char)r[0x36], m->room = (signed char)r[0x37];
}
static int Ours(Game *g, const MRec *m)
{
    unsigned char raw[RM_REC_SIZE];
    if (!m) return 0;
    ToRaw(m, raw);
    return RmProvOurs(&g->prov, raw);
}

// the room exit: every enemy of the room writes its record (dead: its body; standing: its record), copies keep none
static void Exit(Game *g)
{
    int i;
    if (g->stage != STAGE || g->room <= 0) return;
    for (i = 0; i < PER; ++i) {
        const Task *t = &g->task[i];
        MRec *m;
        if (!t->made) continue;
        m = Find(g, IdOf(g->room, i));
        if (!m) m = Alloc(g);
        m->state = t->dead ? T_BODY : T_ALIVE, m->type = t->type, m->variant = t->variant, m->stage = STAGE, m->room = g->room;
        m->id = IdOf(g->room, i), m->kill = g_sc.s[SlotOf(g->room, i)].kill;
        if (g->copy[i].made && g->copy[i].dead) g->ring[g->room][i] = 1;
    }
    memset(g->task, 0, sizeof g->task), memset(g->copy, 0, sizeof g->copy);
}
// Enemies Return's revive of room r (enemies.inc EnmReviveRoom: every placement here is of a revive type)
static void Revive(Game *g, int r)
{
    int i;
    for (i = 0; i < PER; ++i) {
        MRec *m = Find(g, IdOf(r, i));
        if (m && m->state == T_BODY && m->stage == STAGE) m->state = 0;
        KillPut(g, r, i, 0);
        g->saveBit[r][i] = 0;
        g->ring[r][i] = 0;  // (its killed copies come back)
    }
}
// a room load (EnmWrap): the world-visit rule first, then Enemies Return, then the plan, then the room's script (the gate)
static void Load(Game *g, int stage, int room, int isLoad)
{
    int kind, act, f, i;
    if (!isLoad) Exit(g);
    else memset(g->task, 0, sizeof g->task), memset(g->copy, 0, sizeof g->copy);
    if (stage != g->stage && g->stage == STAGE && !isLoad) {  // the hole on the way out: the world's records go, the bits stay
        int k;
        for (k = 0; k < NREC; ++k)
            if (g->rec[k].state && g->rec[k].stage == STAGE) g->rec[k].state = 0;
        memset(g->ring, 0, sizeof g->ring);  // (enemies.inc: leaving the world clears the dead ring)
    }
    if (stage != g->stage) memset(g->ring, 0, sizeof g->ring);
    kind = WvRoomLoadBoth(&g->wv, &g->vis, g->mgr, stage, room, &f);
    g->stage = stage, g->room = room, g->mgr = stage;
    if (stage != STAGE) return;
    act = ErAction(g_ret, kind);
    if (act == ERA_ROOM) Revive(g, room);
    else if (act == ERA_WORLD) {
        int r;
        for (r = 1; r < ROOMS; ++r) Revive(g, r);
    }
    {
        int idx[PER], m = 0;
        RmRec rec[PER];
        RmPick pick[PER];
        RmPlanStat st;
        RmPlanIn in;
        unsigned char yes[PER];
        memset(yes, 1, sizeof yes), memset(&in, 0, sizeof in);
        for (i = 0; i < PER; ++i) idx[m++] = SlotOf(room, i);
        for (i = 0; i < PER; ++i) {
            const MRec *r = Find(g, IdOf(room, i));
            rec[i].type = r && r->stage == STAGE ? r->type : -1, rec[i].variant = r ? r->variant : 0;
            rec[i].here = r && r->room == room, rec[i].ours = Ours(g, r);
            if (r && r->state == T_BODY && r->stage == STAGE && r->room == room) in.dead |= 1u << i;
            if (KillTest(g, room, i)) in.killed |= 1u << i;
        }
        in.mode = g->rz ? RMM_ALL : RMM_OFF, in.seed = g_seed, in.stage = STAGE, in.half = 0, in.room = room;
        in.visit = g_reroll == RZR_ROOM ? WvRoomOf(&g->vis, stage, room) : g->vis.world;
        in.reroll = g_reroll == RZR_ROOM;
        in.resident = RM_ALL_TARGETS;
        in.world = g_sc.placed;
        in.allowed = RM_MON_TARGETS & ~((1u << RmTargetIndex(8, 0)) | (1u << RmTargetIndex(8, 1)) | RM_TREMER_TARGET);  // (see the top)
        in.freeBones = 100000, in.totalBones = 100000, in.released = 1, in.rollAgain = 1;
        if (g->rz) {
            RmPlanRoom(&in, &g_sc, idx, m, rec, yes, pick, &st);
            g_bodiesKept += st.dead;
        } else {
            for (i = 0; i < PER; ++i) pick[i].type = kStock[i], pick[i].variant = 0, pick[i].why = RMW_SAME, pick[i].drop = 0;
        }
        for (i = 0; i < PER; ++i) {
            const RmSlot *s = &g_sc.s[idx[i]];
            MRec *r = Find(g, IdOf(room, i));
            Task *t = &g->task[i];
            if (g->rz && pick[i].drop && r && r->room == room) r->state = 0, r = NULL;
            if (g->rz && pick[i].why != RMW_KILLED) RmProvPut(&g->prov, IdOf(room, i), pick[i].type, pick[i].variant, s->type, s->variant);
            // the gate (0x464F20)
            memset(t, 0, sizeof *t);
            t->type = pick[i].type, t->variant = pick[i].variant;
            if (KillTest(g, room, i)) continue;                // 3: never made
            if (r && r->stage == STAGE && r->room == room) {   // 4: the type's restore reads its record
                if (r->type != t->type || r->variant != t->variant) ++g_mismatch;
                CHECK(r->type == t->type && r->variant == t->variant, "room %d n%d: the create of %02X v%d reads a record of %02X v%d",
                      room, i + 1, t->type, t->variant, r->type, r->variant);
                t->made = 1, t->dead = r->state == T_BODY;
                r->state = 0;                                  // (the restore deletes it; the exit writes it again)
            } else {
                t->made = 1, t->dead = 0;                      // 0 / 5: made fresh at its placement
            }
            // its copy (Extra Enemies x2): beside a standing original, unless killed before (the ring)
            if (t->made && !t->dead && !g->ring[room][i]) {
                Task *c = &g->copy[i];
                c->made = 1, c->dead = 0, c->type = t->type, c->variant = t->variant;
                if (g->rz && RmCopiesDrawn(t->type, pick[i].why)) {
                    const int ct = RmCopyDraw(&in, s, 1);
                    if (ct >= 0) {
                        c->type = kRmTargets[ct].type, c->variant = kRmTargets[ct].variant;
                        ++g_copiesDrawn, g_copiesOther += c->type != t->type;
                        CHECK(RM_COPY_TARGETS >> ct & 1u, "a copy's type is a copy target");
                    }
                }
            }
        }
    }
    ++g_roomLoads;
}
static int Standing(const Game *g, int i) { return g->stage == STAGE && g->task[i].made && !g->task[i].dead; }
static int CopyStanding(const Game *g, int i) { return g->stage == STAGE && g->copy[i].made && !g->copy[i].dead; }
static void Compare(const char *what)
{
    int i;
    if (A.stage != STAGE) return;
    for (i = 0; i < PER; ++i) {
        const int a = Standing(&A, i), b = Standing(&B, i);
        if (a != b && !b && B.saveBit[B.room][i] && g_ret == ER_OFF) {  // the one allowed difference (see the top)
            ++g_deviation;
            continue;
        }
        CHECK(a == b, "Return %s, Reroll %s, %s, room %d n%d: Enemies Off %s, All Worlds %s", kErName[g_ret], g_reroll == RZR_ROOM ?
              "By Room" : "By World", what, A.room, i + 1, a ? "stands" : "dead / not made", b ? "stands" : "dead / not made");
        if (a == b) CHECK(CopyStanding(&A, i) == CopyStanding(&B, i), "Return %s, %s, room %d n%d: its copy", kErName[g_ret], what, A.room, i + 1);
    }
}
static void Kill(int i)
{
    if (A.stage != STAGE) return;
    if (Standing(&A, i) && Standing(&B, i)) A.task[i].dead = B.task[i].dead = 1;
    if (CopyStanding(&A, i) && CopyStanding(&B, i) && Rand(2)) A.copy[i].dead = B.copy[i].dead = 1;
}
// a save (EnemyManagerSave 0x465410: a record for every enemy of the room, the 0x1B90 bytes copied (Refined's filter in
// the copy), those records deleted again)
static void Save(Game *g)
{
    static unsigned char block[0x1B90];
    int i, k;
    MRec keep[NREC];
    memcpy(keep, g->rec, sizeof keep);
    if (g->stage == STAGE)
        for (i = 0; i < PER; ++i)
            if (g->task[i].made) {
                MRec *m = Alloc(g);
                m->state = g->task[i].dead ? T_BODY : T_ALIVE, m->type = g->task[i].type, m->variant = g->task[i].variant;
                m->stage = STAGE, m->room = g->room, m->id = IdOf(g->room, i), m->kill = g_sc.s[SlotOf(g->room, i)].kill;
            }
    memset(block, 0, sizeof block);
    memcpy(block, g->bits, sizeof g->bits);
    for (k = 0; k < NREC; ++k) ToRaw(&g->rec[k], block + RM_SAVE_RECS + RM_REC_SIZE * k);
    if (g->rz) {
        int lost;
        RmSaveKeepDead(block, block + RM_SAVE_RECS, &g->prov, &lost);
        CHECK(lost == 0, "every model placement has a kill index");
        RmSaveClean(block + RM_SAVE_RECS, &g->prov);
    }
    memcpy(g->save.bits, block, sizeof g->save.bits);
    for (k = 0; k < NREC; ++k) FromRaw(block + RM_SAVE_RECS + RM_REC_SIZE * k, &g->save.rec[k]);
    memcpy(g->rec, keep, sizeof keep);
    g->hasSave = 1, g->saveStage = g->stage, g->saveRoom = g->room;
}
static void QuickLoad(Game *g)
{
    int r, i;
    if (!g->hasSave) return;
    memcpy(g->rec, g->save.rec, sizeof g->rec), memcpy(g->bits, g->save.bits, sizeof g->bits);
    g->mgr = WV_MGR_LOADED;
    memset(g->saveBit, 0, sizeof g->saveBit);
    for (r = 1; r < ROOMS; ++r)
        for (i = 0; i < PER; ++i)
            if (g->rz && KillTest(g, r, i)) g->saveBit[r][i] = 1;  // (the model's only kill bits are RmSaveKeepDead's)
    Load(g, g->saveStage, g->saveRoom, 1);
    ++g_loadsChecked;
}
static void Restart(Game *g, unsigned salt)  // the game closed and started again: Refined's memory is empty; then a load
{
    memset(&g->prov, 0, sizeof g->prov);
    WvReset(&g->wv), WvVisitsInit(&g->vis, salt);
    memset(g->ring, 0, sizeof g->ring);
    g->mgr = WV_MGR_RESET;
}
static void Both(int ev, int arg)
{
    switch (ev) {
    case 0: Load(&A, STAGE, arg, 0), Load(&B, STAGE, arg, 0), Compare("a room change"); break;
    case 1: Load(&A, 1, 1, 0), Load(&B, 1, 1, 0), Load(&A, STAGE, arg, 0), Load(&B, STAGE, arg, 0), Compare("a world entry"); break;
    case 2: Save(&A), Save(&B); break;
    case 3: if (A.hasSave) QuickLoad(&A), QuickLoad(&B), Compare("a Quick Load"); break;
    case 4: Kill(arg); break;
    case 5:
        if (A.hasSave) {
            const unsigned salt = (unsigned)Rand(1 << 30) | 1u;
            Restart(&A, salt), Restart(&B, salt), QuickLoad(&A), QuickLoad(&B), Compare("a restart and a load");
        }
        break;
    }
}
static void Fresh(Game *g, int rz)
{
    memset(g, 0, sizeof *g);
    g->rz = rz, g->stage = -1, g->room = -1, g->mgr = WV_MGR_RESET;
    WvReset(&g->wv), WvVisitsInit(&g->vis, 0x1234567u);
}
// one run of a combination: random events (kills weigh most; doors; Room 302 and back; saves and loads)
static void Run(int ret, int reroll, unsigned seed, int steps)
{
    int s;
    g_ret = ret, g_reroll = reroll, g_rng = seed;
    Fresh(&A, 0), Fresh(&B, 1);
    Both(0, 1);
    for (s = 0; s < steps; ++s) {
        const int e = Rand(100);
        if (e < 40) Both(4, Rand(PER));
        else if (e < 70) Both(0, 1 + Rand(ROOMS - 1));
        else if (e < 78) Both(1, 1 + Rand(ROOMS - 1));
        else if (e < 88) Both(2, 0);
        else if (e < 97) Both(3, 0);
        else Both(5, 0);
    }
}
// the user's case, step by step: Return Off, Reroll By Room: kill all four in room 1, leave and come back
static void TestUserCase(void)
{
    int i, ret, rr;
    for (ret = 0; ret < ER_N; ++ret)
        for (rr = 0; rr < 2; ++rr) {
            g_ret = ret, g_reroll = rr ? RZR_WORLD : RZR_ROOM, g_rng = 99;
            Fresh(&A, 0), Fresh(&B, 1);
            Both(0, 1);
            for (i = 0; i < PER; ++i) Both(4, i);
            Both(0, 2), Both(0, 1);
            for (i = 0; i < PER; ++i)
                CHECK(Standing(&B, i) == (ret == ER_ROOM), "Return %s, Reroll %s: room 1 entered again: n%d %s", kErName[ret],
                      rr ? "By World" : "By Room", i + 1, Standing(&B, i) ? "stands" : "stays dead");
            Both(2, 0), Both(0, 2), Both(3, 0);  // a Quick Save there, a door, a Quick Load: the dead stay as the save has them
            for (i = 0; i < PER; ++i)
                CHECK(Standing(&B, i) == (ret == ER_ROOM), "Return %s: after a Quick Load in room 1: n%d %s", kErName[ret], i + 1,
                      Standing(&B, i) ? "stands" : "stays dead");
            Both(1, 1);  // Room 302 and back into room 1: a world entry
            for (i = 0; i < PER; ++i)
                CHECK(Standing(&B, i) == (ret != ER_OFF || !B.saveBit[1][i]), "Return %s: a world entry: n%d %s", kErName[ret], i + 1,
                      Standing(&B, i) ? "stands" : "stays dead");
        }
}
int main(void)
{
    int ret, rr, seed, combos = 0;
    BuildScan();
    TestUserCase();
    for (ret = 0; ret < ER_N; ++ret)
        for (rr = 0; rr < 2; ++rr)
            for (seed = 1; seed <= 40; ++seed, ++combos) Run(ret, rr ? RZR_WORLD : RZR_ROOM, (unsigned)seed * 2654435761u, 600);
    printf("rzreturn_test: %d run(s) of 600 events over Return x Reroll; %d room load(s) (%d after a load), %d bod(ies) kept as they "
           "died by the plan, %d copies drawn (%d of another type than their original); %d type mismatch(es) at the gate; %d "
           "allowed difference(s) (Return Off: a save's kill bit after the hole)\n", combos, g_roomLoads, g_loadsChecked,
           g_bodiesKept, g_copiesDrawn, g_copiesOther, g_mismatch, g_deviation);
    CHECK(g_mismatch == 0, "no create ever reads a record of another type");
    CHECK(g_bodiesKept > 100 && g_copiesOther > 100 && g_loadsChecked > 100, "the matrix exercised the rules");
    printf("rzreturn_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
