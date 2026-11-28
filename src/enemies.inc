// ================================================================ v3.31 ENEMIES: Enemy Copies and Enemies Follow (special-mod round 5)
// Plan: staging/special_mods/r5_enemies/PLAN.md (from PLAN_DRAFT.md and the user's decisions of 2026-11-17); RE:
// re_enemies.md (this round) and ../re_special.md B. Included by sh4windowmod.c after deathpresence_draw.inc. GOG v7 VAs.
// The pure rules (ids, types, the bone budget, the round robin, the dead ring, pursuers, the door, the queue,
// wanderers) are enemies_sim.h (native test: test/enemies_sim_test.c).
//
// Enemy Copies (Option 5 > Enemies, Off / x2 / x3 / x4): at every room load (EnmWrap, the re-aimed `call 52CA00` at
// 0x4F0AF8) every eligible original of the room's script that stands in the room gets 1..3 copies: its own descriptor
// rebuilt with the game's script command function (0x52C530) and replayed through SceneObjectCreate 0x52C900 with a
// copy id 0x1E00nnnn, no kill index, no id override, spawn mode 0, at a tested spot 150-250 units from the original.
// The bone budget (enemies_sim.h) limits them. Copies are never saved (their records are purged at every load and tick), do
// not count for the ending's kill rank (entry hook 0x450DB0), and a killed copy stays dead until a world reset (New
// Game / load) or the world is left. The bone pool grows to 4096 records at the next world reset (entry hook 0x419830,
// BonePool=Auto; the larger of this and the table's Bone Pool Extender is kept).
//
// Enemies Follow (Off / Monsters / Monsters + Victims): every normal-play tick the up to FollowMax (3) nearest standing
// monsters within FollowRange (800) horizontally and 400 vertically are noted. When the player leaves the room through a
// real door (same stage 2..8, door > 0, not the Building elevator) those monsters follow: each one's own persistence
// record (written by its destroy as the old room closed) is moved to the new room at the door spot FollowDelay (3) s +
// 1.5 s x its rank later, once the player has left the door (>= 300 units), and its own placement is replayed from its
// home room's script with its own id: the stock spawn gate finds the record here (4) and the type's own restore puts it
// there with its HP and state. It stays that enemy (its kill index: a kill is a kill everywhere). Left behind in a
// foreign room, it is restored there at every load (a wanderer) until it follows you again; Follow Off sends every
// wanderer home at the next room load. Victims (0B, 0C v1, 0D v1) follow by the ghosts' own arrival system: the room
// rule call at 0x486199 answers yes for a victim that was near you when you left.
//
// v3.30 EN330 (staging/v330_feedback/w3_enemies/PLAN.md): the bone pool also grows at the next room change (0x4F0A5A,
// the `call 4198B0` in RoomLoadSetMark: every bone is free there), the copy log reads the real free count and says why
// each asked copy was not made, spots are tried on five rings; Follow takes the released event dogs and the risen
// Hummers too (their mode is cleared in the replay), keeps a follower's home placement empty while its record is
// elsewhere (most monster creates spawn on gate 5: a temporary kill bit for the script run), and logs every room change;
// Revive Enemies (Off / On): before a room's script its killed monsters of the revive types lose their dead record and
// their kill bit, and its killed copies leave the dead ring (never in story rooms; enemies_sim.h EnmRevive*).
//
// v3.31 EN331 (staging/v331_feedback/w2_enemies/PLAN.md): ghosts never die, and their copies no longer vanish. A ghost
// copy's record (written by its destroy at room exit: down with its countdown, pinned, or standing) goes from the purge
// into Refined's own store; the next visit puts it back for the copy's own create (the gate's 4 = the type's restore),
// its countdown and HP aged as the game ages a ghost record (0x4853F0). Copies are made beside a downed or pinned ghost
// too, and a pinned copy never enters the dead ring. The four story ghosts (Cynthia's ghost 0C v0, Jasper 0D v0, Andrew
// 0E, Richard 0F) are copied as the victims are (not Andrew in stage 4 before flag 0x2DC, not Richard in room 5:3; ini
// [Gameplay] CopyStoryGhosts=0 turns it off). Copies stay out of the save (0x465438: the save's record write skips copy
// ids) and out of Supply Drops' top level (drops.inc). A flying Hummer's copy keeps its height above the floor.
//
// v3.33 EN333 (staging/v333_feedback/w6_enemies/PLAN.md; the user's v3.32 QA): the event dogs (Sniffer spawn modes 3 / 6 /
// 7, e.g. the first dogs of Subway World room 2:2) are copied in their own mode, so the copies sleep and wake with them
// (ini [Gameplay] CopyEventDogs=0: v3.32). Enemies Follow carries its followers' files: a follower (monster or victim)
// whose files the next room does not list gets them loaded at the room load (FileRequestAsync, the queue run, the
// set-up 0x417D40) while 30 MB of the file arena stay free, else it stays behind with the reason in the log (ini
// FollowLoadFiles=0: v3.32); the motion containers count too; a flying Hummer is noted up to 1100 above / below her; an
// empty room keeps its pursuer list fresh (v3.32 said "too old" at every door of an empty room). Verbose lines (W0 LogV)
// for copy / no-copy reasons, follow waits and outcomes; LogAnomaly for a follower lost between rooms, a record moved
// twice, files gone after the door.
//
// v3.34 EN334 (staging/v334_feedback/w1_enemies/PLAN.md; the user's v3.33 QA items 18 / 19): Revive Enemies brings back the
// released event dogs: a room whose only event spawns are Sniffer Dogs whose story flag 0x130 is set (their events are
// over) is no story room any more; its killed dogs stand again awake (spawn mode cleared, made after the room's script)
// and their killed copies come back awake (ini [Gameplay] ReviveEventDogs=0: v3.33). Enemies Follow takes Enemy Copies'
// monster copies too, each on its own (FollowCopies=0: v3.33): a copy's record, written by its destroy at the door, goes
// through with it; a copy that came in lives in that room for the rest of the world visit (Refined's copies' ledger,
// never saved; cleared at a world reset, a save load, leaving the world, Copies Off or Follow Off) and is not made again
// in its own room. FollowMax 6 by default (ini 1..8). Wanderers: v3.33 logged the type of a record the type's restore had
// already deleted ("type 00"); a wanderer's record spot now gets the copies' floor test (put onto the floor, or moved to a
// ring spot near it); a LogV line says where it was put and its state 3 s later, a LogAnomaly when it is gone or not drawn.
//
// v3.35 GH335 (staging/v335_feedback/w1_ghosts/PLAN.md; the user's v3.34 QA on ghosts): victims follow better and keep
// following. A victim that turned real after following (EnemyGhostRealFromPlaceholder 0x4852B0 creates it with mode word
// variant << 8 | 4) was read as "an event spawn" and never followed again: mode 4 is a victim's ordinary mode now. Victims
// are taken from FollowRangeVictims (1500) away, and a victim that followed PURSUES you: it is tagged again at every later
// door (in the room at any distance, or still on its way) until it is downed / pinned, you load or leave the world, or it
// fails to catch up for FollowKeepDoors (3) doors in a row; FollowMax counts it; a story flag's refusal (0x464880 sets
// gEnemyManager.denied 0xFD0BC8) is never overridden. Victims' copies follow as the monsters' copies do (FollowCopies; the
// room rule decides at the arrival; a pinned one stays pinned in its room). A victim original that comes into the room
// after the room load (the game's arrival, or a follower) gets its copies beside it then (LateCopies). A hit on a downed
// ghost never shortens its down countdown (the longer of the time left and the game's new roll; DownKeep).
//
// v3.38 EN338 (staging/v338_feedback/w2_ghosts/PLAN.md; the user's v3.37 feedback items 4-10): no timing rule keeps a
// follower out (no 6 s list age, no 20 s give-up: a follower that does not fit yet waits, here or through a later door of
// the same world); every follower pursues for the whole world visit (until you leave the world, load, it is pinned, killed,
// or the game's story rule 0x464880 refuses it); FollowMax 32 (was 6), the copies' ledger 128, and every arrival passes the
// space gates (list, tasks, actors, bones, hit shapes, IK, a record slot for a copy's kept state; files at the door); a
// monster that came near you, was hit or saw you (its EnemySense record) in this room visit pursues from any distance, every
// ghost in the room pursues; the story ghosts (Cynthia's ghost 0C v0, Jasper 0D v0, Andrew 0E, Richard 0F) and their
// copies follow (never Andrew in stage 4 before flag 2DC, never Richard or the Patient into or out of room 5:3 before flag
// 3BF; hidden FollowStoryGhosts=0 = v3.37); a tagged ghost is admitted by the game's arrival one a tick while the room has
// space; no Wheelchair in Copies or Follow (a v3.37 stray's record is deleted at the room load); DownKeep defaults to 0
// (the game's own down countdown).
//
// v3.49 EF349 (staging/v349_feedback/w5_follow/PLAN.md; the user, 2026-11-25: "For enemy's follow allow a Victims Only
// parameter"): Enemies Follow is Off / Monsters / Victims Only / Monsters + Victims (enemies_sim.h ENM_FOLLOW_*; the ini
// keeps the name, the old three read as before). Victims Only = the ghosts' path of Monsters + Victims (the victims, the
// story ghosts, their copies) and none of the monsters' (EnmPursuerWhy4): the monsters' and their copies' pursuits end at
// once (EnmPursueDropMonsters), and at the next room load the monsters left in other rooms go home and the monsters'
// copies leave the copies' ledger (EnmMonstersHomeAtLoad, as Follow Off does for them). Each change of the value is logged
// once with what follows.
//
// v3.50 EF350 (staging/v350_feedback/w5_follow/PLAN.md; the user's v3.49 QA C1, C4, C5): the value "Victims Only" is
// "Victims" (the old ini text still reads); followers survive a save and its load (enmfollow.inc: Refined's follower
// state is kept per save in save\SH4Refined_follow.dat, keyed by the save's own enemy block, and put back at the load's
// room load); Enemy Pursuit Relentless / Normal / Forgetful (pursuit_sim.h): enemies lose track of you (out of their
// senses for 10 / 4 s when you leave, down when you leave, 2 / 1 doors without catching up); Relentless = v3.49. A load
// empties the arrival watch (v3.49 logged a follower that came in after the save as "lost between the rooms").
#include "enemies_sim.h"
#include "pursuit_sim.h"  // v3.50 EF350
#include "pursuit_room_sim.h"  // v3.51 EP351: Enemy Pursuit inside the room (the pure rules)
#include "enmreturn_sim.h"  // v3.56 ER356: Enemies Return Off / By Room / By World (the pure rules)
// v3.56 MERGE: worldvisit.inc (the one world-visit rule) is included by sh4windowmod.c before randomizer.inc

#define ENM_LIST_MAX 40      // at most 40 enemies in the room list (Death Presence tracks 64)
#define ENM_FREE_TASKS 40    // tasks (0x11B6020, 300 x 0x430) kept free (v3.36 TP336: of the first 300: TpGateFree)
#define ENM_FREE_ACTORS 20   // actor records (0x1093AF0, 300 x 0xB0) kept free
#define ENM_ORIG_MAX 40
#define ENM_WANDER_MAX 32    // v3.38 EN338: was 8 (followers stay for the whole world visit)

static volatile LONG g_enmCopies, g_enmFollow;  // the rows (gameplay.inc GpApply): 0 Off / 1 x2 / 2 x3 / 3 x4; ENM_FOLLOW_* (v3.49 EF349: 0 Off / 1 Monsters / 2 Victims Only / 3 Monsters + Victims)
static int g_enmInstalled;                     // the core set (0x4F0AF8, 0x450DB0, 0x419830) is in
static int g_enmGhostOn;                       // 0x486199 is in (the victims' value)
static volatile int g_enmFail;                 // fail-closed: a fault turns both features off for the session
static int g_enmPoolSetting = ENM_POOL_AUTO;    // [Gameplay] BonePool (ini only): Auto, 0, 1408..16384
static int g_enmFollowMax = ENM_FOLLOW_DEFAULT; // [Gameplay] FollowMax (ini only) 1..32 (v3.34 EN334: 6, was 3 of 1..6; v3.38 EN338: 32 of 1..32)
static float g_enmFollowDelay = 3.0f;          // [Gameplay] FollowDelay (ini only) 0..30 s
static float g_enmFollowRange = 800.0f;        // [Gameplay] FollowRange (ini only) 200..3000
static int g_enmPoolNow;                       // the pool size the last world reset asked for (log)
static EnmRing g_enmDead;                       // killed copies (EnmCopyKey)
static int g_enmLastStage = -1;                // the stage of the last room load (leaving the world clears the ring)
static int g_enmLogged[16];                    // one-shot log lines
static DWORD g_enmFaultCode;
static void *g_enmFaultAt;

// the pursuers of the room the player is in (the last normal-play tick)
// v3.38 EN338: every eligible one of the room (ENM_PUR_MAX, was FollowMax), pursuers first (prio); how = its spotted marks
typedef struct { unsigned id; int type, variant, ghost, copy, prio, how; float ago; int lost; } EnmPur;  // v3.34 EN334: + copy  // v3.50 EF350: + ago, lost (Enemy Pursuit)
static EnmPur g_enmPur[ENM_PUR_MAX];
static int g_enmPurN, g_enmPurStage, g_enmPurRoom;
static DWORD g_enmPurTime;
// the followers on their way into this room
// v3.38 EN338: + clock (a copy's record ages from then), said (the wait reasons already logged), ghost (a ghost copy)
typedef struct { unsigned id; int type, variant, fromRoom; float due; int done, copy; BYTE rec[0x50]; float clock; int said, ghost; } EnmFollower;  // v3.34 EN334: + copy, its record
static EnmFollower g_enmQ[ENM_FOLLOW_MAX];
static int g_enmQN, g_enmQStage, g_enmQRoom;
static float g_enmDoor[3];                     // the door spot: the player's position on the first normal tick in the room
static int g_enmDoorSet;
static unsigned g_enmGhostTag[ENM_FOLLOW_MAX];  // victims that follow (FFFF00nn ids) into this room, this stage
static int g_enmGhostTagN, g_enmGhostTagStage;
// v3.30 EN330
static volatile LONG g_enmRevive;              // the row (gameplay.inc GpApply): 0 Off / 1 On  // v3.56 ER356: ER_OFF / ER_ROOM / ER_WORLD
static int g_enmBoneSite;                      // 0x4F0A5A is in (the pool grows at a room change)
static BYTE *g_enmPoolBuf;                     // our bone buffer (VirtualAlloc, kept for the session)
static int g_enmPoolBufN;                      // its size in records
typedef struct { int word; DWORD mask; unsigned id; } EnmHide;
static EnmHide g_enmHide[64];                  // kill bits set for one room script run (followers' home placements); v3.30 REV330-002: one per scanned record (64)
static int g_enmHideN;
typedef struct { int seen, distant, high, down, event, nokill, other, forgot; } EnmPurStats;  // v3.50 EF350: + forgot
static EnmPurStats g_enmPurStats;              // why the monsters of the last list were left out
static int g_enmReviveSkipLog[16];             // one log line per story room (stage x room bits) and session
// v3.31 EN331
static EnmSnapStore g_enmSnap;                  // the ghost copies' own records, between their visits
static int g_enmSnapAny;                        // the store holds something (cleared when Copies goes Off)
static float g_enmClock, g_enmClockPrev;        // seconds the enemy records aged (the room timer's steps)
static int g_enmStoryOn = 1;                    // [Gameplay] CopyStoryGhosts (ini only): the story ghosts are copied
static int g_enmSaveSite;                       // 0x465438 is in (the save skips the copies' records)
// v3.33 EN333 (staging/v333_feedback/w6_enemies/PLAN.md)
static int g_enmCopyDogs = 1;                   // [Gameplay] CopyEventDogs (ini only): the event dogs (mode 3 / 6 / 7) are copied
static int g_enmLoadFiles = 1;                  // [Gameplay] FollowLoadFiles (ini only): a follower's missing files are loaded
static int g_enmFileFns;                        // the file functions it calls are the stock ones (checked at install)
#define ENM_EXTRA_MAX 16
static unsigned g_enmExtra[ENM_EXTRA_MAX];      // whole-file keys Refined loaded for this room (followers, wanderers)
static int g_enmExtraN, g_enmExtraStage = -1, g_enmExtraRoom = -1;
static volatile LONG g_enmQuotaCopy, g_enmQuotaFollow, g_enmQuotaWait, g_enmQuotaFiles, g_enmQuotaAnom;
typedef struct { unsigned id; int type, room; DWORD at; int seen, kind; float put[3]; } EnmArrived;  // v3.34 EN334: + kind, put
#define ENM_WATCH_MAX 64  // v3.34 EN334: followers, wanderers and copies (was ENM_FOLLOW_MAX); v3.38 EN338: 64 (was 16)
static EnmArrived g_enmArrived[ENM_WATCH_MAX];  // followers that came in: watched for a few seconds (lost = an anomaly)
static int g_enmArrivedN;
// v3.34 EN334 (staging/v334_feedback/w1_enemies/PLAN.md)
static int g_enmReviveDogs = 1;                 // [Gameplay] ReviveEventDogs (ini only): released event dogs revive, awake
static int g_enmFollowCopies = 1;               // [Gameplay] FollowCopies (ini only): copies follow too
static EnmCwLedger g_enmCw;                     // copies that followed you: their room and last record (never saved)
static BYTE g_enmPurRec[ENM_PUR_MAX][0x50];     // the records the pursuing copies wrote at the door (v3.38 EN338: ENM_PUR_MAX)
static int g_enmPurHave[ENM_PUR_MAX];
typedef struct { unsigned id; int kill; } EnmAwake;
static EnmAwake g_enmAwakeQ[ENM_ORIG_MAX];      // released event dogs made awake after this room's script
static int g_enmAwakeQN;
static unsigned g_enmAwakeAway[16];             // released event dogs whose body was taken away in another room
static int g_enmAwakeAwayN;
static volatile LONG g_enmQuotaWander, g_enmQuotaCopyF;
static volatile LONG g_enmQuotaFresh;           // v3.41 EN341: ghost copies made without their record in another room
#define ENM_HPGET(t)       ((float(__cdecl *)(const BYTE *))0x44FD30)(t)           // EnemyHpGet (v3.35.1: unused, NULL-unsafe)
// v3.35.1 HF3351: EnemyHpGet without its NULL fault: the HP block of the task's work, else the fallback
#define ENM_HPSAFE(t, fb)  EnmHpFromBlock(EnmWork(t) ? *(const float *const *)(EnmWork(t) + 0x34) : NULL, (fb))
#define ENM_PINS ((const BYTE *)0x10A4A60)                                          // gPlayerPins: 18 x 0x40
#define ENM_FLAG_2DC ((*(volatile const BYTE *)(0x13D80A0 + 0x2DC / 8) & (1 << (0x2DC & 7))) != 0)  // gStoryFlags
#define ENM_MAXHP(t, v)    ((float(__cdecl *)(int, int))0x468E00)(t, v)            // EnemyStatGetMaxHp
#define ENM_DIFFICULTY()   ((int(__cdecl *)(void))0x46C3B0)()                      // DifficultyGet
// v3.35 GH335 (staging/v335_feedback/w1_ghosts/PLAN.md)
static float g_enmVictimRange = ENM_VICTIM_RANGE;  // [Gameplay] FollowRangeVictims (ini only) 200..3000
static int g_enmKeepDoors = ENM_KEEP_ALWAYS;       // [Gameplay] FollowKeepDoors (ini only) 0 (v3.34: no pursuit) .. 32; v3.38 EN338: missing = Always
static int g_enmLateCopies = 1;                    // [Gameplay] LateCopies (ini only): victims that come in later get copies
static int g_enmCdKeepOn = ENM_DOWNKEEP_DEFAULT;   // [Gameplay] DownKeep (ini only): a hit never shortens a down countdown (v3.38 EN338: default 0, vanilla)
static EnmPursuers g_enmPursue;                    // the victims that pursue you (this world visit, never saved); v3.38 EN338: every kind
static int g_enmDoorSerial = 1;                    // + 1 at every room load: the room visit a pursuer was seen in
// v3.38 EN338: the tags already logged at the victims' room rule (reset at the door), one bit per tag (32): let in, kept out
// by a story flag, waiting for space, the story guard
static unsigned g_enmTagSaidIn, g_enmTagSaidDenied, g_enmTagSaidWait, g_enmTagSaidStory;
// v3.58 EN358: what became of each tag in its room visit (enemies_sim.h ENM_TAG_*), its nearest distance to you (XZ) once
// in, the clock at the door; the room it was tagged for (the line at the next door names it)
static unsigned char g_enmTagSeen[ENM_FOLLOW_MAX];
static float g_enmTagNear[ENM_FOLLOW_MAX];
static float g_enmTagT0;
static int g_enmTagRoom = -1;
static volatile LONG g_enmQuotaTagIn, g_enmQuotaTagSum;
// v3.38 EN338 (staging/v338_feedback/w2_ghosts/PLAN.md)
static int g_enmStoryFollow = 1;                   // [Gameplay] FollowStoryGhosts (ini only): the story ghosts and their copies follow
static EnmSeenSet g_enmSeen;                       // this room visit's spotted marks (near, hit, saw you)
static volatile LONG g_enmHitFree = -1;            // free hit shapes on the last tick (hitpool.inc HpTick writes it; -1 = not known)
static unsigned g_enmGhostAdmitTick;               // the tick a tagged ghost was last let in (one a tick)
static unsigned g_enmSpaceTick;                    // the tick g_enmSpace was read
static EnmSpace g_enmSpace;                        // (its bones test is per type: g_enmSpaceHead / Free / Total)
static int g_enmSpaceHead;
static volatile LONG g_enmQuotaSeen, g_enmQuotaDoor, g_enmQuotaStory, g_enmQuotaSpace, g_enmQuotaWith;
#define ENM_GHOSTTIMER() ((float(__cdecl *)(void))0x46F9C0)()  // the room timer a ghost needs here (1.7e38 = never)
static int g_enmTimerFn;                           // 0x46F9C0 is the stock function (checked at install); else no ghost copy waits for it
#define ENM_FLAG(n) ((*(volatile const BYTE *)(0x13D80A0 + (n) / 8) & (1 << ((n) & 7))) != 0)  // gStoryFlags
static EnmLateSet g_enmLate;                       // the victim originals of this room visit already handled
typedef struct { const BYTE *t, *w; unsigned id; EnmCdState s; unsigned tick; } EnmCdTrack;
#define ENM_CD_TRACK_N 64
static EnmCdTrack g_enmCdT[ENM_CD_TRACK_N];        // the ghosts' class and down countdown on the last gameplay tick
static unsigned g_enmCdTick;
static DWORD g_enmCdRoomKey = 0xFFFFFFFFu;
static int g_enmCdKept;                            // countdowns kept this session (the log)
static volatile LONG g_enmQuotaPursue, g_enmQuotaLate, g_enmQuotaCd;
#define ENM_DENIED        (*(volatile const int *)0xFD0BC8)                         // gEnemyManager.denied (0x464880: a story flag refused)
#define ENM_MAYENTER(r, room) ((int(__cdecl *)(BYTE *, int))0x464880)(r, room)      // EnemyGhostMayEnterRoom (the stock rule)
#define ENM_FRAMETIME     (*(volatile const float *)0x614FF0)                       // EnemyGetFrameTime's value
// v3.50 EF350: Enemy Pursuit (the row: enmfollow.inc; the rules: pursuit_sim.h) and the followers' side file
static volatile LONG g_enmPursuit = EF_PURSUIT_DEFAULT;  // [Gameplay] EnemyPursuit: 0 Relentless / 1 Normal / 2 Forgetful
static EfTrack g_efTrack;                                // this room visit: when each enemy last sensed you
static void EfRestoreAtLoad(int room);                   // enmfollow.inc: a load's followers come back (EnmWrap)
static void EfInstall(void);                             // enmfollow.inc (EnemiesInstall)
static int EpIgnores(unsigned id);                       // v3.51 EP351 pursuit_room.inc: it gave up the chase in this room
static void EpRoomEnd(const char *why);                  // v3.51 EP351: the room line (a room load, a save load)
static void EpTick(const float *pp);                     // v3.51 EP351 (EnmTickC)
static void EpInstall(void);                             // v3.51 EP351 (EnemiesInstall)
// a copy whose pursuit ends while it is on its way (the ledger's room 0) is made in its own room again
static void EnmTransitHome(unsigned cid, int stage)
{
    EnmCw *c = EnmCwFind(&g_enmCw, EnmCopyKey(stage, ((int(__cdecl *)(void))0x4FC830)() ? 1 : 0, cid));  // (ENM_HALF)
    if (c && c->room == 0) c->used = 0;
}
// v3.38 REV338 R3: the queue is dropped (Follow off, a fault): every queued copy not in yet goes home (its ledger entry was
// room 0: neither made at home nor replayed anywhere until the next room load cleared it). Returns how many.
static int EnmQueueHome(void)
{
    int i, n = 0;
    for (i = 0; i < g_enmQN && i < ENM_FOLLOW_MAX; ++i)
        if (!g_enmQ[i].done && g_enmQ[i].copy) EnmTransitHome(g_enmQ[i].id, g_enmQStage), ++n;
    return n;
}
static void EnmPursueDrop(const char *why)  // every pursuit ends (a load, leaving the world, Follow off, a fault)
{
    const int n = EnmPursueCount(&g_enmPursue);
    int i;
    for (i = 0; i < ENM_PURSUERS_N; ++i)  // v3.38 REV338 R3: a copy on its way goes home with its pursuit
        if (g_enmPursue.e[i].used && g_enmPursue.e[i].copy) EnmTransitHome(g_enmPursue.e[i].id, g_enmPursue.e[i].stage);
    EnmPursueClear(&g_enmPursue);
    if (n && LogQuota(&g_enmQuotaPursue, 300))
        LogV("enemies: %d pursuer(s) stop pursuing you: %s", n, why);  // v3.38 EN338: every kind (was victims)
}
// v3.38 EN338: Enemies Follow = Monsters: the ghosts' pursuits end (the monsters' and their copies' go on)
static void EnmPursueDropGhosts(void)
{
    int i, n = 0;
    for (i = 0; i < ENM_PURSUERS_N; ++i)
        if (g_enmPursue.e[i].used && g_enmPursue.e[i].ghost) {
            g_enmPursue.e[i].used = 0, ++n;
            if (g_enmPursue.e[i].copy) EnmTransitHome(g_enmPursue.e[i].id, g_enmPursue.e[i].stage);
        }
    for (i = 0; i < g_enmQN; ++i)  // v3.38 REV338 EN338-001: and the ghost copies already on their way into this room
        if (!g_enmQ[i].done && g_enmQ[i].ghost) {
            g_enmQ[i].done = 1, ++n;
            if (g_enmQ[i].copy) EnmTransitHome(g_enmQ[i].id, g_enmQStage);
        }
    n += g_enmGhostTagN, g_enmGhostTagN = 0;  // v3.38 REV338 (inspection 2): the victims tagged for this room too
    if (n && LogQuota(&g_enmQuotaPursue, 300))
        LogV("enemies: %d ghost(s) stop pursuing you: Enemies Follow takes no ghosts now (Monsters)", n);
}
// v3.49 EF349: Enemies Follow = Victims Only: the monsters' and their copies' pursuits end (the ghosts' and the ghost copies'
// go on); a monster copy on its way goes home (its ledger entry), a queued monster's record stays where it is (it goes home
// at the next room load: EnmMonstersHomeAtLoad)
static void EnmPursueDropMonsters(void)
{
    int i, n = 0;
    for (i = 0; i < ENM_PURSUERS_N; ++i)
        if (g_enmPursue.e[i].used && !g_enmPursue.e[i].ghost) {
            g_enmPursue.e[i].used = 0, ++n;
            if (g_enmPursue.e[i].copy) EnmTransitHome(g_enmPursue.e[i].id, g_enmPursue.e[i].stage);
        }
    for (i = 0; i < g_enmQN && i < ENM_FOLLOW_MAX; ++i)  // the monsters and copies already on their way into this room
        if (!g_enmQ[i].done && !g_enmQ[i].ghost) {
            g_enmQ[i].done = 1, ++n;
            if (g_enmQ[i].copy) EnmTransitHome(g_enmQ[i].id, g_enmQStage);
        }
    if (n && LogQuota(&g_enmQuotaPursue, 300))
        LogV("enemies: %d monster(s) and monster cop(ies) stop pursuing you: Enemies Follow takes no monsters now (Victims)", n);  // v3.50 EF350: (was Victims Only)
}

static int EnmFaultNote(EXCEPTION_POINTERS *e)
{
    g_enmFaultCode = e->ExceptionRecord->ExceptionCode;
    g_enmFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static void EnmFailNow(const char *where)
{
    if (g_enmFail) return;
    g_enmFail = 1;
    __try {  // v3.38 REV338 R3: the copies on their way go home (the ledger is not cleared by a fault)
        EnmQueueHome();
        EnmPursueDrop("a fault");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_enmQN = g_enmGhostTagN = g_enmPurN = 0;
    EnmPursueClear(&g_enmPursue), g_enmLate.on = 0;  // v3.35 GH335
    Log("enemies: %s faulted (code %08lX at %p): Enemy Copies and Enemies Follow are off for this session (the room "
        "loads, the kill count and the record clean-up stay stock-safe)", where, g_enmFaultCode, g_enmFaultAt);
}
static int EnmCopiesLive(void) { return g_enmInstalled && !g_enmFail && g_enmCopies > 0 && g_enmCopies <= ENM_COPY_MAXK; }
static int EnmFollowLive(void) { return g_enmInstalled && !g_enmFail && EnmFollowValueOn(g_enmFollow); }  // v3.49 EF349: (was 1..2)
// v3.56 ER356: Enemies Return's live value (enmreturn_sim.h ER_*; Off while the core set is out or after a fault)
static int EnmReturnMode(void) { const int v = (int)g_enmRevive; return g_enmInstalled && !g_enmFail && v > ER_OFF && v < ER_N ? v : ER_OFF; }
static int EnmReviveLive(void) { return EnmReturnMode() == ER_ROOM; }  // v3.30 EN330  // v3.56 ER356: By Room (v3.30 - v3.55's On)

// ---------------------------------------------------------------- game calls (all cdecl, matched in SH4Decomp)
#define ENM_NEXT(t)        ((BYTE *(__cdecl *)(const BYTE *))0x45E760)(t)          // EnemyListNext
#define ENM_FIND(id)       ((BYTE *(__cdecl *)(unsigned))0x45E7F0)(id)             // EnemyListFindBySpawnId
#define ENM_ID(t)          ((unsigned(__cdecl *)(const BYTE *))0x45B6D0)(t)        // the spawn id (info+0)
#define ENM_DEAD(t)        ((int(__cdecl *)(const BYTE *))0x45BB90)(t)             // EnemyIsDead
#define ENM_RECFIND(id)    ((BYTE *(__cdecl *)(unsigned))0x464350)(id)             // EnemyRecordFind
#define ENM_RECDEL(r)      ((void(__cdecl *)(BYTE *))0x464380)(r)                  // EnemyRecordDelete
#define ENM_CREATE(d, r, id) ((BYTE *(__cdecl *)(BYTE *, int, unsigned))0x52C900)(d, r, id)  // SceneObjectCreate
#define ENM_HALF()         ((int(__cdecl *)(void))0x4FC830)()                      // FlagsGetApartmentWorldCompleted
#define ENM_MGR_ON()       ((int(__cdecl *)(void))0x463F40)()                      // EnemyManagerIsOn
#define ENM_TO_REC(o, i)   ((void(__cdecl *)(float *, const float *))0x46FE30)(o, i)  // world -> record position
#define ENM_FROM_REC(o, i) ((void(__cdecl *)(float *, const float *))0x46FE80)(o, i)  // record -> world position
#define ENM_RECORDS        ((BYTE *)0xFCF090)                                      // 64 x 0x50 (gEnemyManager.rec)
#define ENM_ROOM_TIMER     (*(volatile float *)0xFD0BA0)                           // gEnemyManager.room_timer
#define ENM_STAGE          ((int)*(volatile DWORD *)0xFD5A90)
#define ENM_ROOM           ((int)*(volatile DWORD *)0xFD5A94)
// v3.30 EN330
#define ENM_KILLIDX(t)     ((int(__cdecl *)(const BYTE *))0x45B6F0)(t)             // the kill index (info+0x20)
#define ENM_AISTATE(t)     ((int)*(const WORD *)((const BYTE *)(t) + 0x410))       // EnemyGetAiState 0x45B5E0
#define ENM_KILLBITS       ((volatile DWORD *)0xFCF010)                            // gEnemyManager.kill_bits[32]
#define ENM_PREVROOM       ((int)*(volatile DWORD *)0xFD5A98)                      // gGame.prevRoom (0x4F7180 / 0x4F7220)
#define ENM_DOGS_AWAKE     ((*(volatile const BYTE *)0x13D80C6 & 1) != 0)          // the event dogs' release (0x48EA70)
static int EnmKillTest(int stage, int k)
{
    int w;
    unsigned m;
    return EnmKillBit(stage, k, &w, &m) && (ENM_KILLBITS[w] & m) != 0;
}
static void EnmKillPut(int stage, int k, int on)
{
    int w;
    unsigned m;
    if (!EnmKillBit(stage, k, &w, &m)) return;
    if (on) ENM_KILLBITS[w] |= m;
    else ENM_KILLBITS[w] &= ~m;
}

static int EnmFinite3(const float *p) { return _finite(p[0]) && _finite(p[1]) && _finite(p[2]); }
static BYTE *EnmWork(const BYTE *t) { return *(BYTE *const *)(t + 0x408); }
static unsigned EnmModeWord(const BYTE *t)  // info+0x14 (0x45B4B0): [task+0x41C] + 0x2C + 0x14
{
    const BYTE *r = *(const BYTE *const *)(t + 0x41C);
    return (ULONG_PTR)r + 0x2C >= 0x10000 ? *(const unsigned *)(r + 0x40) : 0xFFFFFFFFu;
}
static int EnmModelBones(const BYTE *w)  // [[model+0xB4]+0x0C] (SgAnimeInit's bone count), 0 = unknown
{
    const BYTE *m = *(const BYTE *const *)(w + 0x30), *inst;
    if ((ULONG_PTR)m < 0x10000) return 0;
    inst = *(const BYTE *const *)(m + 0xB4);
    if ((ULONG_PTR)inst < 0x10000) return 0;
    return *(const int *)(inst + 0x0C);
}
static int EnmTypeOfDesc(const BYTE *desc)  // the type whose create function the placement has (script command 6 / 7)
{
    const DWORD create = *(const DWORD *)(desc + 8);
    int t;
    for (t = 1; t < 0x17; ++t)
        if (*(const DWORD *)(0x615348 + 16 * t + 4) == create) return t;
    return -1;
}
static int EnmIsBoss(const BYTE *t, const BYTE *w)
{
    const int type = *(const int *)(w + 0x20);
    return type == 0x10 || (type == 5 && (EnmModeWord(t) & 0xF) == 8);
}
static int EnmListCount(int *boss)
{
    const BYTE *t = NULL;
    int n = 0;
    if (boss) *boss = 0;
    while ((t = ENM_NEXT(t)) != NULL && n < 96) {
        const BYTE *w = EnmWork(t);
        ++n;
        if (boss && w && EnmIsBoss(t, w)) *boss = 1;
    }
    return n;
}
static int EnmFreeTasks(void)
{
    const BYTE *p = (const BYTE *)0x11B6020;
    int i, n = 0;
    for (i = 0; i < g_tpN; ++i, p += 0x430)  // v3.36 TP336: every slot of the pool (300, or [Gameplay] TaskPool)
        if (*(const DWORD *)(p + 0x404) == 0) ++n;
    return n;
}
static int EnmFreeActors(void)  // never used (beyond [0x10A0930]) + freed (as the table's er_room)
{
    const BYTE *p = (const BYTE *)0x1093AF0;
    int c = *(const int *)0x10A0930, i, n;
    if (c > 300) c = 300;
    if (c < 0) c = 0;
    n = 300 - c;
    for (i = 0; i < c; ++i, p += 0xB0)
        if (*(const DWORD *)(p + 0x10) == 0) ++n;
    return n;
}
// v3.36.1 TL3361: may Enemy Copies / Follow use the task slots past 300? Yes when no Playable Eileen is loaded or it is
// v1.20.1+ (it reads the live count 0x553A72); an older PE assumes 300 and keeps the reserve. Decided once, at the first
// call (gameplay: dsound.dll is loaded long before); [Gameplay] CopiesAllTasks=0 (hidden) keeps the reserve always.
static int g_enmTaskAll = -1, g_enmTaskAllIni = 1;
static int EnmTaskAll(void)
{
    if (g_enmTaskAll < 0) {
        const HMODULE ds = GetModuleHandleA("dsound.dll");
        unsigned long ms = 0, ls = 0;
        int pe = ds && HasExport(ds, "SH4PE_Patches"), found = 0;
        if (pe) {
            __try {
                const HRSRC r = FindResourceA(ds, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);
                const HGLOBAL g = r ? LoadResource(ds, r) : NULL;
                const BYTE *p = g ? (const BYTE *)LockResource(g) : NULL;
                const DWORD sz = r ? SizeofResource(ds, r) : 0;
                DWORD i;
                for (i = 0; p && i + 52 <= sz && !found; i += 4)
                    if (*(const DWORD *)(p + i) == 0xFEEF04BD)
                        ms = *(const DWORD *)(p + i + 8), ls = *(const DWORD *)(p + i + 12), found = 1;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                found = 0;
            }
        }
        g_enmTaskAll = g_enmTaskAllIni && (!pe || (found && TpPeReadsCount(ms, ls)));
        Log("enemies: task slots: Enemy Copies / Follow keep %d of %s %d free (%s)", ENM_FREE_TASKS,
            g_enmTaskAll ? "all" : "the first 300 of", g_tpN,
            !g_enmTaskAllIni ? "[Gameplay] CopiesAllTasks=0" : !pe ? "no Playable Eileen loaded"
            : !found ? "Playable Eileen's version is unreadable: it may assume 300 slots"
            : g_enmTaskAll ? "Playable Eileen v1.20.1+ reads the live slot count" : "Playable Eileen before v1.20.1 assumes 300 slots");
    }
    return g_enmTaskAll;
}
static int EnmRoomForOne(void) { return EnmListCount(NULL) < ENM_LIST_MAX && TpGateFreeAll(EnmFreeTasks(), g_tpN, ENM_FREE_TASKS, EnmTaskAll()) && EnmFreeActors() >= ENM_FREE_ACTORS; }  // v3.36 TP336; v3.36.1 TL3361
// the headroom the room's enemies still need for their blends (1.5 x bones each)
static int EnmHeadroom(void)
{
    const BYTE *t = NULL;
    int n = 0, h = 0;
    while ((t = ENM_NEXT(t)) != NULL && n++ < 96) {
        const BYTE *w = EnmWork(t);
        if (w) h += EnmHead(*(const int *)(w + 0x20), EnmModelBones(w));
    }
    return h;
}
static int EnmBudgetFits(int type, int bones, int head, int *freeOut)
{
    const int freeRec = *(volatile const int *)0xED73E4, total = *(volatile const int *)0xED73EC;
    if (freeOut) *freeOut = freeRec;
    return EnmFits(freeRec, head, EnmCost(type, bones), total);
}
static void EnmPlayerPos(float *pp, int atLoad)
{
    // at a room load through a door the player is placed at 0x10A1300 (0x52CEE0 from 0x4F0620) [I]; else her position
    if (atLoad && *(volatile int *)0xFD5AA8 > 0) memcpy(pp, (const void *)0x10A1300, 12);
    else memcpy(pp, (const void *)0x10A0F60, 12);
}

// ---------------------------------------------------------------- the room script walk (0x52CA00's rule, PLAN 2)
// 0x52C530 SceneScriptCommand is a static function with a register convention: ecx = tag, eax = args. It writes only
// gSceneDesc (0x10A09E0, 0x94 bytes) and sSceneTaskType (0x10A0E7C, a short); it touches eax / ecx / edx only. Both are
// saved and put back around the walk.
static const DWORD g_enmCmdFn = 0x52C530;
static __declspec(naked) void __cdecl EnmCmd(int tag, const DWORD *args)
{
    __asm {
        mov ecx, [esp + 4]
        mov eax, [esp + 8]
        push ebx
        push esi
        push edi
        push ebp
        call dword ptr [g_enmCmdFn]
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
typedef int (*EnmWalkFn)(void *ctx, int n, const BYTE *desc);  // 1 = stop
static int EnmWalkC(int room, EnmWalkFn fn, void *ctx)
{
    BYTE *const desc = (BYTE *)0x10A09E0;
    const BYTE *const clear = (const BYTE *)0x10A0E80;
    volatile short *const ttype = (volatile short *)0x10A0E7C;
    const DWORD *p = ((const DWORD *const *)0x10A0A78)[room];
    int n = 0, guard = 0, stop = 0;
    DWORD w;
    if (!p) return -1;
    memcpy(desc, clear, 0x94);
    *ttype = 0;
    w = *p;
    EnmCmd((int)(w & 0xFFFF), p + 1);
    p += (w >> 16) + 1;
    while (*p != 0xF0F0F0F0u && (*p & 0xFFFF) != 0 && guard++ < 8192) {
        w = *p;
        if ((w & 0xFFFF) == 1) {
            RmDescFix(room, n, desc);  // v3.49 RZ349: a randomized placement is rebuilt as the type it lives as (rzmon.inc)
            stop = fn(ctx, n++, desc);
            memcpy(desc, clear, 0x94);
            *ttype = 0;
            if (stop) return n;
        }
        EnmCmd((int)(w & 0xFFFF), p + 1);
        p += (w >> 16) + 1;
    }
    RmDescFix(room, n, desc);  // v3.49 RZ349
    fn(ctx, n, desc);
    return n + 1;
}
static int EnmWalk(int room, EnmWalkFn fn, void *ctx)
{
    BYTE save[0x94];
    short saveType;
    int r = -1;
    if (room <= 0 || room >= 0x100) return -1;
    memcpy(save, (const void *)0x10A09E0, sizeof save);
    saveType = *(volatile short *)0x10A0E7C;
    __try {
        r = EnmWalkC(room, fn, ctx);
    } __finally {
        memcpy((void *)0x10A09E0, save, sizeof save);
        *(volatile short *)0x10A0E7C = saveType;
    }
    return r;
}
typedef struct { int want, found; BYTE *out; } EnmFindCtx;
static int EnmFindCb(void *c, int n, const BYTE *desc)
{
    EnmFindCtx *x = (EnmFindCtx *)c;
    if (n != x->want) return 0;
    memcpy(x->out, desc, 0x94);
    x->found = 1;
    return 1;
}
// the descriptor of a placement from its script id (its home room's script; this stage and half only)
static int EnmDescFor(unsigned id, BYTE *out)
{
    EnmFindCtx x;
    if (!EnmIsScriptId(id) || EnmScriptStage(id) != ENM_STAGE || EnmScriptHalf(id) != (ENM_HALF() ? 1 : 0)) return 0;
    x.want = EnmScriptN(id);
    x.found = 0;
    x.out = out;
    EnmWalk(EnmScriptRoom(id), EnmFindCb, &x);
    return x.found && *(const int *)out == 2;
}

// ---------------------------------------------------------------- spots (PLAN 2: 0x463150's drop recipe)
typedef struct { __declspec(align(16)) float pos[4]; float normal[4]; float dist; int hit; void *poly; int unk; } EnmFloorHit;
// v3.30 EN330: returns ENM_SPOT_OK or why the candidate failed (enemies_sim.h ENM_SPOT_*)
// v3.31 EN331: ghost 2 = a flyer (floor0 = the original's floor y): the copy keeps the original's height above its floor
static int EnmSpotTest(const float *base, float *cand, int ghost, float floor0)
{
    __declspec(align(16)) float p[4], a[4], b[4];
    __declspec(align(16)) EnmFloorHit hit;
    memset(&hit, 0, sizeof hit);
    if (ghost == 2) {  // v3.31 EN331
        p[0] = cand[0], p[1] = cand[1] - 4.0f, p[2] = cand[2], p[3] = 1.0f;
        if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) != 1) return ENM_SPOT_FLOOR;
        if (!_finite(hit.pos[0]) || !_finite(hit.pos[1]) || !_finite(hit.pos[2])) return ENM_SPOT_FLOOR;
        if (!EnmFlyerFloorOk(floor0, hit.pos[1])) return ENM_SPOT_DROP;
        if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hit.pos, hit.normal, 60.0f, 1.0f) != 2) return ENM_SPOT_ROOM;
        p[0] = hit.pos[0], p[1] = EnmFlyerY(base[1], floor0, hit.pos[1]), p[2] = hit.pos[2], p[3] = 1.0f;
        if (!((int(__cdecl *)(float *))0x46F700)(p)) return ENM_SPOT_WALL;  // its flying point inside geometry
        a[0] = base[0], a[1] = base[1], a[2] = base[2], a[3] = 1.0f;
        if (((int(__cdecl *)(void *, float *, float *))0x454000)(NULL, a, p) != 0) return ENM_SPOT_SIGHT;
        cand[0] = p[0], cand[1] = p[1], cand[2] = p[2];
        return ENM_SPOT_OK;
    }
    if (ghost) {  // victims pass walls: a floor within 500 of the height (the table's er_ray, from 300 above)
        p[0] = cand[0], p[1] = cand[1] - 300.0f, p[2] = cand[2], p[3] = 1.0f;
        if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) != 1) return ENM_SPOT_FLOOR;
        if (!(fabsf(hit.pos[1] - cand[1]) <= 500.0f) || !_finite(hit.pos[1])) return ENM_SPOT_DROP;
        cand[1] = hit.pos[1];
        return ENM_SPOT_OK;
    }
    p[0] = cand[0], p[1] = cand[1] - 4.0f, p[2] = cand[2], p[3] = 1.0f;
    if (!((int(__cdecl *)(float *))0x46F700)(p)) return ENM_SPOT_WALL;                          // inside geometry
    if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) != 1) return ENM_SPOT_FLOOR;  // EnemyFloorRayCheck
    if (!_finite(hit.pos[0]) || !_finite(hit.pos[1]) || !_finite(hit.pos[2])) return ENM_SPOT_FLOOR;
    if (cand[1] + 400.0f < hit.pos[1]) return ENM_SPOT_DROP;                                    // a floor at most 400 below
    if (((int(__cdecl *)(float *, float *, float, float))0x458F00)(hit.pos, hit.normal, 60.0f, 1.0f) != 2) return ENM_SPOT_ROOM;
    a[0] = base[0], a[1] = base[1] - 100.0f, a[2] = base[2], a[3] = 1.0f;          // line of sight at chest height
    b[0] = hit.pos[0], b[1] = hit.pos[1] - 100.0f, b[2] = hit.pos[2], b[3] = 1.0f;
    if (((int(__cdecl *)(void *, float *, float *))0x454000)(NULL, a, b) != 0) return ENM_SPOT_SIGHT;
    cand[0] = hit.pos[0], cand[1] = hit.pos[1], cand[2] = hit.pos[2];
    return ENM_SPOT_OK;
}
static int EnmClearOf(const float *c, const float *pp, float minPlayer, float minEnemy)  // ENM_SPOT_OK / _PLAYER / _ENEMY
{
    const BYTE *t = NULL;
    float dx = c[0] - pp[0], dz = c[2] - pp[2];
    int n = 0;
    if (dx * dx + dz * dz < minPlayer * minPlayer) return ENM_SPOT_PLAYER;
    while ((t = ENM_NEXT(t)) != NULL && n++ < 96) {
        const float *w = (const float *)EnmWork(t);
        if (!w) continue;
        dx = c[0] - w[0], dz = c[2] - w[2];
        if (dx * dx + dz * dz < minEnemy * minEnemy) return ENM_SPOT_ENEMY;
    }
    return ENM_SPOT_OK;
}
// v3.30 EN330: 8 candidates on each ring of `tiers` around base; the first that passes. why[ENM_SPOT_N] (may be NULL)
// counts the failures.
static int EnmFindSpot(const float *base, unsigned seed, int ghost, float floor0, const float *pp, const EnmSpotTier *tiers,
                      int nTiers, float *out, int *why)  // v3.31 EN331: + floor0 (ghost 2 = a flyer)
{
    int ring, c;
    for (ring = 0; ring < nTiers; ++ring)
        for (c = 0; c < 8; ++c) {
            const EnmSpotTier *T = &tiers[ring];
            float dx, dz, cand[3];
            int r;
            EnmRingSpot(seed ^ (0xA5A5u * (unsigned)ring), c, T->rMin, T->rMax, &dx, &dz);
            cand[0] = base[0] + dx, cand[1] = base[1], cand[2] = base[2] + dz;
            if ((r = EnmClearOf(cand, pp, T->minPlayer, T->minEnemy)) == ENM_SPOT_OK &&
                (r = EnmSpotTest(base, cand, ghost, floor0)) == ENM_SPOT_OK)
                r = EnmClearOf(cand, pp, T->minPlayer, T->minEnemy);  // (the floor point moved it)
            if (r == ENM_SPOT_OK) {
                memcpy(out, cand, 12);
                return 1;
            }
            if (why && r > 0 && r < ENM_SPOT_N) ++why[r];
        }
    return 0;
}
static void EnmSpotWhyText(const int *why, char *b, size_t n)  // "wall 3, floor 2, ..." (the non-zero ones)
{
    static const char *const kName[ENM_SPOT_N] = { "", "in a wall", "no floor", "floor too low", "no room to stand",
                                                   "out of sight", "too near you", "too near another enemy" };
    int i;
    size_t at = 0;
    b[0] = 0;
    for (i = 1; i < ENM_SPOT_N && at + 1 < n; ++i)
        if (why[i]) {
            const int w = _snprintf_s(b + at, n - at, _TRUNCATE, "%s%s %d", at ? ", " : "", kName[i], why[i]);
            if (w < 0) break;
            at += (size_t)w;
        }
}

// ---------------------------------------------------------------- record clean-up (PLAN 3.3, 4.5)
// v3.34 EN334: a copy's record (its destroy at a room exit; copies keep none in the game's table): a copy that pursues
// you takes it through the door; a copy in the ledger (it followed you before) keeps its room and state there (it leaves
// the ledger when it died: the dead ring keeps it dead)
static void EnmCopyRecordSeen(const BYTE *r, int st)
{
    const unsigned id = *(const unsigned *)(r + 0x20);
    const int ghost = EnmGhostType(*(const short *)(r + 0x24));  // v3.35 GH335: a victim's copy (its 0x0B is a pin)
    int i, w;
    EnmCw *e;
    if (!EnmFollowLive() || !g_enmFollowCopies) return;
    for (i = 0; i < g_enmPurN && i < ENM_PUR_MAX; ++i)  // v3.38 EN338: ENM_PUR_MAX (was ENM_FOLLOW_MAX)
        if (g_enmPur[i].copy && g_enmPur[i].id == id && g_enmPurStage == st) {
            memcpy(g_enmPurRec[i], r, 0x50);
            g_enmPurHave[i] = 1;
        }
    w = EnmCwOnRecord2(&g_enmCw, EnmCopyKey(st, ENM_HALF() ? 1 : 0, id), r, ghost);  // v3.35 GH335: + ghost
    if (w == 0 && (e = EnmCwFind(&g_enmCw, EnmCopyKey(st, ENM_HALF() ? 1 : 0, id))) != NULL) e->clock = g_enmClock;
    if (w == 1 && LogQuota(&g_enmQuotaCopyF, 200))
        LogV("enemies: copy %08X died in room %d: it leaves the copies' ledger (dead until you leave the world%s)", id,
             *(const signed char *)(r + 0x37), EnmReviveLive() ? " or its own room revives it" : "");
}
// v3.36 HE336: what Refined keeps of a copy's record before it is deleted: the purge below, and recevict.inc when the
// game's full table takes a copy's record for a new one (the same keeping, a few frames early)
static void EvAtPurge(int slot, const BYTE *r);  // v3.36 HE336 (recevict.inc): a copy's record that took a real one's slot
static void EvAtPurgeEnd(int purged);            // v3.36 HE336 (recevict.inc)
static void EnmCopyRecordGone(const BYTE *r)
{
    // v3.31 EN331: a ghost copy's record is kept in Refined's store for its next visit (never in the game's table)
    const int st = *(const signed char *)(r + 0x36);
    if (EnmCopiesLive() && EnmGhostType(*(const short *)(r + 0x24)) && st >= 2 && st <= 8) {
        if (EnmSnapPut(&g_enmSnap, EnmCopyKey(st, ENM_HALF() ? 1 : 0, *(const unsigned *)(r + 0x20)),
                       *(const signed char *)(r + 0x37), g_enmClock, r)) {
            static int s_evict;  // v3.31 EN331-002: the store was full (not expected below 1024)
            if (s_evict++ < 10)
                Log("enemies: the ghost-copy store is full (%d): the oldest copy's state was dropped (it comes back "
                    "fresh)", ENM_SNAP_N);
        }
        g_enmSnapAny = 1;
        if (!g_enmLogged[8]++)
            Log("enemies: a ghost copy left room %d:%d (state %02X, countdown %.1f s): it comes back as it was there "
                "(kept by Refined, never saved)", st, *(const signed char *)(r + 0x37), *(const short *)(r + 0x26),
                *(const float *)(r + 0x3C));
        EnmCopyRecordSeen(r, st);  // v3.35 GH335: a victim's copy that pursues you or lives in another room (the ledger)
    } else if (st >= 2 && st <= 8 && !EnmGhostType(*(const short *)(r + 0x24))) {
        EnmCopyRecordSeen(r, st);  // v3.34 EN334
    }
}
static int EnmPurgeCopyRecords(void)  // records of copies (ids 0x1E00nnnn): at every room load and every tick
{
    BYTE *r = ENM_RECORDS;
    int i, n = 0;
    for (i = 0; i < 64; ++i, r += 0x50)
        if (*(const short *)(r + 0x26) != 0 && EnmIsCopyId(*(const unsigned *)(r + 0x20))) {
            EvAtPurge(i, r);         // v3.36 HE336
            EnmCopyRecordGone(r);    // v3.36 HE336 (was inline here)
            ENM_RECDEL(r);
            ++n;
        }
    EvAtPurgeEnd(n);  // v3.36 HE336
    return n;
}
// Follow Off: a monster recorded away from its home room goes home (healed). v3.30 EN330: one killed away from home stays
// dead (its kill bit, as the game's own reuse of a dead record 0x464060 does), unless Revive Enemies is on.
static int EnmSendWanderersHome(int *keptDead)
{
    BYTE *r = ENM_RECORDS;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0, revive = EnmReviveLive();
    int i, n = 0;
    *keptDead = 0;
    for (i = 0; i < 64; ++i, r += 0x50)
        if (*(const short *)(r + 0x26) != 0 &&
            EnmIsWanderer(*(const unsigned *)(r + 0x20), *(const short *)(r + 0x24), *(const signed char *)(r + 0x36),
                         *(const signed char *)(r + 0x37), stage, half)) {
            if (*(const short *)(r + 0x26) == 0x0B && !revive && *(const short *)(r + 0x30) > 0) {
                EnmKillPut(stage, *(const short *)(r + 0x30), 1);
                ++*keptDead;
            }
            ENM_RECDEL(r);
            ++n;
        }
    return n;
}
// v3.49 EF349: a room load while Enemies Follow takes no monsters (Off, or Victims Only): the monsters left in other rooms
// go home; Off: every copy that followed you is made in its own room again (v3.34 EN334), Victims Only: the monsters' copies
// (the ghost copies stay where they are). (v3.30 - v3.48: inline in EnmWrap, Off only; the Off lines are unchanged.)
static void EnmMonstersHomeAtLoad(void)
{
    const int off = g_enmFollow == ENM_FOLLOW_OFF;
    const char *const what = off ? "off" : "Victims";  // v3.50 EF350: (was "Victims Only")
    int n, dead = 0;
    if ((n = EnmSendWanderersHome(&dead)) != 0)
        Log("enemies: Enemies Follow is %s: %d monster(s) left in other rooms went home (%d of them killed there: they stay "
            "dead)", what, n, dead);
    if (off) {
        if ((n = EnmCwCount(&g_enmCw)) != 0) {  // v3.34 EN334
            EnmCwClear(&g_enmCw);
            Log("enemies: Enemies Follow is off: %d cop%s that followed you are made in their own rooms again", n, n == 1 ? "y" : "ies");
        }
    } else if ((n = EnmCwDropMonsters(&g_enmCw)) != 0) {
        Log("enemies: Enemies Follow is Victims: %d monster %s that followed you %s again (ghost copies stay)", n,  // v3.50 EF350
            n == 1 ? "copy" : "copies", n == 1 ? "is made in its own room" : "are made in their own rooms");
    }
}
// v3.38 EN338: a real Wheelchair that followed in v3.37 (its record in a foreign room, also out of a save) goes home: its
// record is deleted (killed there: its kill bit is set, it stays dead; a Wheelchair is no Revive type). Every room load
// (the 64 records; after the first, only a v3.37 save can bring one back).
static void EnmWheelchairHome(void)
{
    BYTE *r = ENM_RECORDS;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    int i;
    for (i = 0; i < 64; ++i, r += 0x50) {
        const unsigned id = *(const unsigned *)(r + 0x20);
        const int state = *(const short *)(r + 0x26), k = *(const short *)(r + 0x30);
        if (state == 0 || !EnmStrayWheelchair(id, *(const short *)(r + 0x24), *(const signed char *)(r + 0x36),
                                              *(const signed char *)(r + 0x37), stage, half))
            continue;
        if (state == 0x0B && k > 0) EnmKillPut(stage, k, 1);
        Log("enemies: Wheelchair %08X (home room %d) was recorded in room %d (state %02X; it followed in v3.37): it goes home%s "
            "(Wheelchairs stay vanilla)", id, EnmScriptRoom(id), *(const signed char *)(r + 0x37), state,
            state == 0x0B ? " dead (its kill bit is set)" : "");
        ENM_RECDEL(r);
    }
}
// v3.30 EN330: most monster creates spawn at their placement when the gate says 5 (a record of this id in another room:
// Hummer 0x4742F0, Dog 0x48E690, Twin 0x4C9610, Wheelchair 0x4CC430 end only on 1 / 2 / 3, the Gum Head 0x49A3D0 on 2 / 3). So
// while a follower's record is in another room, its kill bit is set for this room's script run and cleared right after
// (gate 3: the create ends itself through 0x45CD50 without a record); the follower stays where it is.
static void EnmHideAwayHome(int room)
{
    BYTE *r = ENM_RECORDS;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    int i, w;
    unsigned m;
    // v3.34 EN334: g_enmHideN is reset by EnmWrap (Revive's awake event dogs are in the list already)
    for (i = 0; i < 64; ++i, r += 0x50) {
        const unsigned id = *(const unsigned *)(r + 0x20);
        const int k = *(const short *)(r + 0x30);
        if (*(const short *)(r + 0x26) == 0 || EnmScriptRoom(id) != room ||
            !EnmIsWanderer(id, *(const short *)(r + 0x24), *(const signed char *)(r + 0x36), *(const signed char *)(r + 0x37),
                           stage, half))
            continue;
        if (!EnmKillBit(stage, k, &w, &m)) {
            Log("enemies: %08X (type %02X) is recorded in room %d but has no kill index: its home placement spawns it too",
                id, *(const short *)(r + 0x24), *(const signed char *)(r + 0x37));
            continue;
        }
        if (ENM_KILLBITS[w] & m) continue;  // dead for good already
        if (g_enmHideN >= (int)(sizeof g_enmHide / sizeof g_enmHide[0])) break;  // v3.30 REV330-002: = the 64 records scanned (never hit)
        ENM_KILLBITS[w] |= m;
        g_enmHide[g_enmHideN].word = w, g_enmHide[g_enmHideN].mask = m, g_enmHide[g_enmHideN].id = id;
        ++g_enmHideN;
        Log("enemies: %08X (type %02X) is in room %d: its home placement here stays empty", id, *(const short *)(r + 0x24),
            *(const signed char *)(r + 0x37));
    }
}
static void EnmHideRestore(void)
{
    int i;
    for (i = 0; i < g_enmHideN; ++i) ENM_KILLBITS[g_enmHide[i].word] &= ~g_enmHide[i].mask;
    g_enmHideN = 0;
}

// ---------------------------------------------------------------- Enemy Copies (PLAN 3)
typedef struct { int n, type, variant, bones, flyer, beside; unsigned id; float pos[3], floor0; BYTE desc[0x94]; } EnmOrig;  // v3.31 EN331: + flyer, beside, floor0
static EnmOrig g_enmOrig[ENM_ORIG_MAX];
static int g_enmOrigN;
typedef struct { unsigned clearOver; int kind2, eligible, notType, event, idOver, stage, room, story, storyRule, dogs; } EnmScanCtx;  // v3.30 EN330: + the reasons; v3.31 EN331: + the story rule; v3.33 EN333: + dogs
static int EnmScanCb(void *c, int n, const BYTE *desc)
{
    EnmScanCtx *x = (EnmScanCtx *)c;
    EnmOrig *o;
    unsigned mw;
    int type;
    if (*(const int *)desc != 2) return 0;
    ++x->kind2;
    type = EnmTypeOfDesc(desc);
    mw = *(const unsigned *)(desc + 0x24);
    // v3.33 EN333: + the event dogs (EnmCopyEligible3: spawn mode 3 / 6 / 7 kept in the copy)
    if (type < 0 || !EnmCopyEligible3(type, (int)(mw >> 8 & 0xF), mw, *(const unsigned *)(desc + 0x2C), x->clearOver,
                                      g_enmStoryOn, g_enmCopyDogs)) {  // v3.31 EN331: + the story ghosts
        const char *why;  // v3.33 EN333: the reason, for the verbose log
        if (type < 0 || !(EnmCopyMonster(type) || EnmIsVictim(type, (int)(mw >> 8 & 0xF)) ||
                          (g_enmStoryOn && EnmIsStoryGhost(type, (int)(mw >> 8 & 0xF))))) {
            ++x->notType, why = "a type never copied (01, 04, 05, 09, 10, 12+; story ghosts with CopyStoryGhosts=0)";  // v3.30 EN330
        } else if (mw & 0xF) {
            ++x->event, why = "an event / boss spawn mode (only the Sniffers' 3 / 6 / 7 are copied)";
        } else {
            ++x->idOver, why = "a story id override on the placement";
        }
        if (LogQuota(&g_enmQuotaCopy, 400))  // v3.33 EN333
            LogV("enemies: copies: room %d:%d placement %d (type %02X, mode word %03X): not copied: %s", x->stage, x->room, n,
                 type, mw & 0xFFF, why);
        return 0;
    }
    if (EnmEventDog(type, mw)) ++x->dogs;  // v3.33 EN333
    if (EnmStoryCopyRoom(type, x->stage, x->room, ENM_FLAG_2DC) != ENM_STORY_OK) {  // v3.31 EN331
        ++x->story;
        x->storyRule = EnmStoryCopyRoom(type, x->stage, x->room, ENM_FLAG_2DC);
        return 0;
    }
    ++x->eligible;
    if (g_enmOrigN >= ENM_ORIG_MAX) return 0;
    o = &g_enmOrig[g_enmOrigN++];
    o->n = n;
    o->type = type;
    o->variant = (int)(mw >> 8 & 0xF);
    memcpy(o->desc, desc, 0x94);
    return 0;
}
// v3.30 EN330: the log says why each asked copy was not made (one summary line per room, one line per original that did
// not get all of its copies); the free count is read at the start and at the end (v3.29 printed the last budget test,
// 0 when none ran); five spot rings; a room without eligible or standing originals is logged too.
// v3.31 EN331: + back (restored from the store), its down / pinned counts, and the ghost copies beside a downed original.
typedef struct { int made, cut, nospot, dead, exists, limit, failed, badId, back, away, recs, why[ENM_SPOT_N]; } EnmCopyTally;  // v3.34 EN334: + away  // v3.58 RZ358: + recs
// v3.31 EN331: a pin of the Sword with this id lies in this room (gPlayerPins 0x10A4A60: +0 stage, +4 room, +0x30 id)
static int EnmPinHeld(unsigned id, int stage, int room)
{
    const BYTE *p = ENM_PINS;
    int i;
    for (i = 0; i < 18; ++i, p += 0x40)
        if (*(const unsigned *)(p + 0x30) == id && *(const int *)p == stage && *(const int *)(p + 4) == room) return 1;
    return 0;
}
// v3.31 EN331: a ghost copy comes back from its own record (Refined's store): the record goes into a free slot of the
// game's table, aged as the game ages ghost records (0x4853F0: the countdown, then the HP), the copy's own create finds
// it (gate 4) and the type's restore puts the copy there as it was; every record of the id is deleted again at once.
// 1 = it is back (*how: its state 0x0B pinned / 0x0A down / 0), -1 = no free record slot now: the snapshot is KEPT and no
// copy is made this visit (v3.31 EN331-001, review: a full table must not lose the stored state), 0 = make a fresh one (a pinned copy whose
// Sword is gone (a load), or the create refused: the room rule of the gate, the stock half-chance of Reconcile 0x482590
// ends in a fresh ghost too).
static int EnmCopyFromStore(const EnmOrig *o, int room, unsigned cid, EnmSnap *e, BYTE *d, int *how)
{
    __declspec(align(16)) float in[4], at[4];
    BYTE rec[ENM_REC_SIZE], *slot = NULL, *r;
    const int stage = ENM_STAGE;
    float cd, hp, mx;
    int i, st, ok;
    BYTE *nt;
    memcpy(rec, e->rec, sizeof rec);
    st = *(const short *)(rec + 0x26);
    if (e->room != room || *(const short *)(rec + 0x24) != o->type || *(const signed char *)(rec + 0x36) != stage ||
        *(const unsigned *)(rec + 0x20) != cid || st == 0) {
        e->used = 0;  // not this copy's record any more: dropped
        return 0;
    }
    if (st == 0x0B && !EnmPinHeld(cid, stage, room)) {
        e->used = 0;  // its Sword is gone: a fresh copy
        return 0;
    }
    for (i = 0, r = ENM_RECORDS; i < 64; ++i, r += 0x50)
        if (*(const short *)(r + 0x26) == 0) {
            slot = r;
            break;
        }
    if (!slot) return -1;  // v3.31 EN331-001: kept for a later visit
    e->used = 0;  // taken (the copy writes a new one when it leaves)
    cd = *(const float *)(rec + 0x3C), hp = *(const float *)(rec + 0x28);
    mx = ENM_MAXHP(o->type, *(const signed char *)(rec + 0x33));
    if (!_finite(cd) || !_finite(hp) || !_finite(mx) || !EnmFinite3((const float *)rec)) return 0;
    EnmGhostDecay(&cd, &hp, mx, ENM_DIFFICULTY(), g_enmClock - e->clock);
    *(float *)(rec + 0x3C) = cd, *(float *)(rec + 0x28) = hp;
    *(int *)(rec + 0x38) = 0;  // age
    memcpy(in, rec, 16);
    ENM_FROM_REC(at, in);
    if (!EnmFinite3(at)) return 0;
    memcpy(d + 0x14, at, 12);  // its own place (the restore puts it there too)
    memcpy(slot, rec, sizeof rec);
    nt = ENM_CREATE(d, room, cid);
    for (i = 0; i < 64 && (r = ENM_RECFIND(cid)) != NULL; ++i) ENM_RECDEL(r);  // (the restore deleted it already)
    nt = nt ? ENM_FIND(cid) : NULL;
    ok = nt && EnmWork(nt) && *(const int *)(EnmWork(nt) + 0x3C) < 0x10;
    if (ok) *how = *(const int *)(EnmWork(nt) + 0x3C);
    return ok;
}
// v3.35 GH335: one copy k of original o (its placement in room `home`, made in `room`): EnmMakeCopies' loop body, shared
// with the late copies (EnmLateCopies). ENM_ONE_* = what happened (the callers count it).
enum { ENM_ONE_MADE, ENM_ONE_BACK, ENM_ONE_KEPT, ENM_ONE_BADID, ENM_ONE_DEAD, ENM_ONE_AWAY, ENM_ONE_EXISTS, ENM_ONE_LIMIT,
       ENM_ONE_CUT, ENM_ONE_NOSPOT, ENM_ONE_FAILED, ENM_ONE_RECS };  // v3.58 RZ358: + RECS (the enemy records are full)
// v3.58 RZ358: the enemy records at this room's exit (rzmon_sim.h RmCopyRecordsOk): may a copy be made without its record
// write taking a real record of this world? The free slots, the other stages' monster records (the game's own first
// choice), the real writes to come (the room's real enemies, the followers on their way). [Gameplay] CopyRecordGuard
// (hidden): 1 = this world's records (default), 2 = no real record at all, 0 = v3.57 (no test)
static int g_enmCopyRecGuard = RM_CRG_WORLD;
static LONG g_enmCopyRecRefused;
static int EnmCopyRecOk(void)
{
    const BYTE *r = ENM_RECORDS, *t = NULL;
    const int stage = ENM_STAGE;
    int i, fr = 0, other = 0, real = 0;
    if (g_enmCopyRecGuard == RM_CRG_OFF) return 1;
    for (i = 0; i < 64; ++i, r += 0x50) {
        if (*(const short *)(r + 0x26) == 0) ++fr;
        else if (!EnmIsCopyId(*(const unsigned *)(r + 0x20)) && !EnmGhostType(*(const short *)(r + 0x24)) &&
                 *(const signed char *)(r + 0x36) != stage)
            ++other;
    }
    while ((t = ENM_NEXT(t)) != NULL && real < 96)
        if (!EnmIsCopyId(ENM_ID(t))) ++real;
    real += g_enmQN + g_enmGhostTagN;
    if (RmCopyRecordsOk(g_enmCopyRecGuard, fr, other, real)) return 1;
    if (InterlockedIncrement(&g_enmCopyRecRefused) <= 20)
        LogV("enemies: copies: the enemy records are full for this room's exit (%d free, %d of other worlds, %d real enemies to "
             "write): no copy is made (a copy never takes a real record of this world%s)", fr, other, real,
             g_enmCopyRecGuard == RM_CRG_STRICT ? "; CopyRecordGuard=2: of any world" : "");
    return 0;
}
static int RmCopyPick(int home, int room, int n, int k, int *type, int *variant, float *site, int *sited);  // v3.58 RZ358: rzmon.inc
static int RmCopyRetype(unsigned cid, int type, int variant, BYTE *d);                                    // v3.58 RZ358: rzmon.inc
static void RmCopyResult(int type, int made);                                                             // v3.58 RZ358: rzmon.inc
static void RmCopiesSay(int stage, int room);                                                             // v3.58 RZ358: rzmon.inc
// v3.58 RZ358: site = a site of its own (Spawn Site, rzmon.inc RmCopyPick), NULL = a ring spot near the original (as v3.57)
static int EnmMakeOneC(const EnmOrig *o, int home, int room, int k, unsigned clearOver, int released, int *head, const float *pp,
                       int *how, int *why, const float *site)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    __declspec(align(16)) BYTE d[0x94];
    float spot[3];
    unsigned cid;
    BYTE *nt;
    EnmSnap *e;
    if (!EnmCopyIdOk(home, o->n, k)) return ENM_ONE_BADID;
    cid = EnmCopyId(home, o->n, k);
    if (EnmRingHas(&g_enmDead, EnmCopyKey(stage, half, cid))) return ENM_ONE_DEAD;
    if (EnmCwFind(&g_enmCw, EnmCopyKey(stage, half, cid))) return ENM_ONE_AWAY;  // v3.34 EN334: it lives in another room
    if (ENM_FIND(cid)) return ENM_ONE_EXISTS;
    if (!EnmRoomForOne()) return ENM_ONE_LIMIT;
    if (!EnmCopyRecOk()) return ENM_ONE_RECS;  // v3.58 RZ358: a copy never takes a real record of this world
    if (!EnmBudgetFits(o->type, o->bones, *head, NULL)) return ENM_ONE_CUT;
    memcpy(d, o->desc, 0x94);
    *(unsigned *)(d + 0x2C) = clearOver;  // no id override (a ghost copy is an ordinary ghost)
    *(int *)(d + 0x30) = 0;               // no kill index: no kill bit is set or tested
    *(unsigned *)(d + 0x24) = EnmCopyModeWord2(o->type, *(const unsigned *)(o->desc + 0x24), g_enmCopyDogs, released);  // spawn mode 0, the same variant (v3.33 EN333: an event dog keeps its 3 / 6 / 7; v3.34 EN334: awake once released)
    // v3.31 EN331: a ghost copy that was here before comes back from its own record (down, pinned or standing)
    if (EnmGhostType(o->type) && (e = EnmSnapFind(&g_enmSnap, EnmCopyKey(stage, half, cid))) != NULL) {
        const int back1 = EnmCopyFromStore(o, room, cid, e, d, how);
        if (back1 < 0) {  // v3.31 EN331-001: no free enemy record: its stored state waits for the next visit
            static int s_full;
            if (s_full++ < 20) Log("enemies: copy %08X: no free enemy record to bring back its stored state: kept for the next visit", cid);
            return ENM_ONE_KEPT;
        }
        if (back1 > 0) {
            nt = ENM_FIND(cid);
            *head += EnmHead(o->type, nt ? EnmModelBones(EnmWork(nt)) : 0);
            return ENM_ONE_BACK;
        }
        memcpy(d, o->desc, 0x94);  // (a fresh one: the descriptor again)
        *(unsigned *)(d + 0x2C) = clearOver;
        *(int *)(d + 0x30) = 0;
        *(unsigned *)(d + 0x24) = EnmCopyModeWord2(o->type, *(const unsigned *)(o->desc + 0x24), g_enmCopyDogs, released);  // v3.33 EN333; v3.34 EN334
    }
    if (site) {  // v3.58 RZ358: Spawn Site's own site for this copy
        memcpy(spot, site, 12);
    } else if (!EnmFindSpot(o->pos, cid, o->flyer ? 2 : EnmGhostType(o->type) ? 1 : 0, o->floor0, pp, kEnmCopyTiers, ENM_COPY_TIERS,
                            spot, why)) {
        return ENM_ONE_NOSPOT;
    }
    memcpy(d + 0x14, spot, 12);
    nt = ENM_CREATE(d, room, cid);
    nt = nt ? ENM_FIND(cid) : NULL;
    if (!nt || !EnmWork(nt) || ENM_DEAD(nt)) return ENM_ONE_FAILED;
    *head += EnmHead(o->type, EnmModelBones(EnmWork(nt)));
    return ENM_ONE_MADE;
}
// v3.58 RZ358: copy k of original o: the type the Randomizer drew for it (rzmon.inc RmCopyPick: Enemies Same World / All
// Worlds, a randomized room's source placement; else its original's type, as v3.57) and, with Spawn Site, its own site
static int EnmMakeOne(const EnmOrig *o, int home, int room, int k, unsigned clearOver, int released, int *head, const float *pp,
                      int *how, int *why)
{
    static EnmOrig oc;  // (game thread only)
    float site[3];
    int type = -1, variant = 0, sited = 0, planned, r;
    const EnmOrig *use = o;
    planned = RmCopyPick(home, room, o->n, k, &type, &variant, site, &sited);
    if (planned && (type != o->type || variant != o->variant)) {
        oc = *o;
        oc.type = type, oc.variant = variant, oc.bones = 0;  // (its model's bones are not known before it stands)
        oc.flyer = o->flyer && type == 0x03, oc.floor0 = oc.flyer ? o->floor0 : 0.0f;
        if (!RmCopyRetype(EnmCopyId(home, o->n, k), type, variant, oc.desc)) oc = *o, planned = 0;  // (no draw after all: its own type)
        else use = &oc;
    }
    r = EnmMakeOneC(use, home, room, k, clearOver, released, head, pp, how, why, sited ? site : NULL);
    if (planned && r != ENM_ONE_BADID && r != ENM_ONE_DEAD && r != ENM_ONE_AWAY && r != ENM_ONE_EXISTS)
        RmCopyResult(use->type, r == ENM_ONE_MADE || r == ENM_ONE_BACK);
    return r;
}
static void EnmMakeCopies(int room)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0, per = (int)g_enmCopies;
    const unsigned clearOver = *(const unsigned *)0x10A0EAC;
    const int total = *(volatile const int *)0xED73EC, free0 = *(volatile const int *)0xED73E4;
    EnmScanCtx x;
    EnmStep s = { -1, 1 };
    static EnmCopyTally tal[ENM_ORIG_MAX];
    float pp[3];
    int i, boss, head, made = 0, cut = 0, nospot = 0, dead = 0, failed = 0, limit = 0, present = 0, gone = 0, retyped = 0,
        down = 0, evmode = 0, beside = 0, back = 0, backDown = 0, backPin = 0, flyers = 0, away = 0;  // v3.31 EN331: + beside .. flyers; v3.34 EN334: + away
    int recs = 0;  // v3.58 RZ358
    const int released = g_enmReviveDogs && ENM_DOGS_AWAKE;  // v3.34 EN334: released event dogs' copies are made awake
    if (stage < 2 || stage > 8) return;
    if (!ENM_MGR_ON()) {
        Log("enemies: room %d:%d copies: the enemy manager is off (an event room): no copies", stage, room);
        return;
    }
    EnmListCount(&boss);
    if (boss) {
        Log("enemies: room %d:%d copies: a boss is in the room (type 10 / the Wall boss): no copies there", stage, room);
        return;
    }
    g_enmOrigN = 0;
    memset(&x, 0, sizeof x);
    x.clearOver = clearOver;
    x.stage = stage, x.room = room;  // v3.31 EN331
    if (EnmWalk(room, EnmScanCb, &x) < 0) return;
    if (x.story)  // v3.31 EN331
        Log("enemies: room %d:%d copies: %d story ghost(s) not copied here (%s)", stage, room, x.story,
            x.storyRule == ENM_STORY_ANDREW ? "Andrew in stage 4 before story flag 2DC: a pinned copy would play his pin "
                                              "message and set it" : "Richard in room 5:3: his 30 s there set story flag 3BF");
    // the originals in the room now (a restored enemy is not at its placement: its live position counts). v3.31 EN331: a
    // ghost counts down or pinned too (EnmCopyBeside; EnemyIsDead 0x45BB90 calls a ghost at HP 0 in class 0x0A dead)
    for (i = 0; i < g_enmOrigN;) {
        EnmOrig *o = &g_enmOrig[i];
        const unsigned over = *(const unsigned *)(o->desc + 0x2C);
        const BYTE *t, *w;
        int cls = 0;
        o->id = EnmIsGhostId(over) ? over : EnmScriptId(stage, half, room, o->n);
        t = ENM_FIND(o->id);
        w = t ? EnmWork(t) : NULL;
        if (w) cls = *(const int *)(w + 0x3C);
        if (!w || *(const int *)(w + 0x20) != o->type || !EnmCopyBeside(o->type, cls) ||
            (!EnmGhostType(o->type) && ENM_DEAD(t)) || !EnmFinite3((const float *)w) ||
            !EnmCopyModeOk(o->type, EnmModeWord(t), g_enmCopyDogs)) {  // (another type: the table's Randomizer retyped it); v3.33 EN333: + the event dogs' modes
            if (!w) ++gone;
            else if (*(const int *)(w + 0x20) != o->type) ++retyped;
            else if (!EnmCopyModeOk(o->type, EnmModeWord(t), g_enmCopyDogs)) ++evmode;  // v3.33 EN333
            else ++down;
            if (LogQuota(&g_enmQuotaCopy, 400))  // v3.33 EN333: the reason per original
                LogV("enemies: copies: room %d:%d %08X (type %02X): no copies: %s", stage, room, o->id, o->type,
                     !w ? "not in the room (killed, elsewhere, or the story has not spawned it)"
                        : *(const int *)(w + 0x20) != o->type ? "retyped by the cheat table's Randomizer"
                        : !EnmCopyModeOk(o->type, EnmModeWord(t), g_enmCopyDogs) ? "in an event mode now"
                        : "down or dead");
            g_enmOrig[i] = g_enmOrig[--g_enmOrigN];
            continue;
        }
        o->beside = !EnmStanding(cls);
        beside += o->beside;
        memcpy(o->pos, w, 12);
        o->bones = EnmModelBones(w);
        o->flyer = 0, o->floor0 = 0.0f;
        if (o->type == 0x03) {  // v3.31 EN331: a flying Hummer keeps its height above the floor
            __declspec(align(16)) float p[4];
            __declspec(align(16)) EnmFloorHit hit;
            memset(&hit, 0, sizeof hit);
            p[0] = o->pos[0], p[1] = o->pos[1] - 4.0f, p[2] = o->pos[2], p[3] = 1.0f;
            if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) == 1 && _finite(hit.pos[1]) &&
                EnmIsFlyer(o->type, o->pos[1], hit.pos[1])) {
                o->flyer = 1, o->floor0 = hit.pos[1];
                ++flyers;
            }
        }
        ++i;
    }
    present = g_enmOrigN;
    if (!g_enmOrigN) {
        Log("enemies: room %d:%d copies x%d: %d placements, %d eligible (%d of types never copied, %d event spawns, %d with "
            "a story id), none in the room (%d not in the room: killed, elsewhere or not spawned by the story, %d down or "
            "dead, %d retyped, %d in an event mode): no copies", stage, room, per + 1, x.kind2, x.eligible, x.notType,
            x.event, x.idOver, gone, down, retyped, evmode);
        return;
    }
    memset(tal, 0, sizeof tal);
    EnmPlayerPos(pp, 1);
    head = EnmHeadroom();
    while (EnmPlanNext(&s, g_enmOrigN, per)) {  // v3.35 GH335: the body is EnmMakeOne (shared with the late copies)
        EnmOrig *o = &g_enmOrig[s.i];
        EnmCopyTally *T = &tal[s.i];
        int how = 0;
        const int r = EnmMakeOne(o, room, room, s.k, clearOver, released, &head, pp, &how, T->why);
        if (r == ENM_ONE_LIMIT) {
            ++limit;
            for (i = 0; i < g_enmOrigN; ++i) tal[i].limit = 1;
            break;
        }
        switch (r) {
        case ENM_ONE_BADID: ++T->badId; break;
        case ENM_ONE_DEAD: ++dead, ++T->dead; break;
        case ENM_ONE_AWAY: ++away, ++T->away; break;
        case ENM_ONE_EXISTS: ++T->exists; break;
        case ENM_ONE_CUT: ++cut, ++T->cut; break;
        case ENM_ONE_BACK:
            ++made, ++T->made, ++back, ++T->back;
            if (how == 0x0B) ++backPin;
            else if (how == 9 || how == 0x0A) ++backDown;
            break;
        case ENM_ONE_NOSPOT: ++nospot, ++T->nospot; break;
        case ENM_ONE_FAILED: ++failed, ++T->failed; break;
        case ENM_ONE_MADE: ++made, ++T->made; break;
        case ENM_ONE_RECS: ++recs, ++T->recs; break;  // v3.58 RZ358
        }
    }
    Log("enemies: room %d:%d copies x%d: %d placements (%d eligible, %d in the room%s), %d of %d copies made (%d back as they "
        "were: %d down, %d pinned); not made: %d over the bone budget, %d without a spot, %d killed before (back when you "
        "leave the world%s), %d failed%s; bone pool free %d -> %d of %d (reserve %d)", stage, room, per + 1, x.kind2,
        x.eligible, present, beside ? ", some down or pinned: their copies stand beside them" : "", made, present * per,
        back, backDown, backPin, cut, nospot, dead, EnmReviveLive() ? "" : " or with Enemies Return By Room", failed,  // v3.56 ER356: (was Revive Enemies)
        limit ? ", the room's enemy / task limit reached" : recs ? ", the enemy records are full (a copy never takes a real record)" : "",  // v3.58 RZ358: + recs
        free0, *(volatile const int *)0xED73E4, total, EnmReserve(total));
    if (recs && LogQuota(&g_enmQuotaCopy, 400))  // v3.58 RZ358
        Log("enemies: room %d:%d copies: %d not made: the enemy records would be full at this room's exit (a copy never takes a "
            "real record of this world; fewer copies instead)", stage, room, recs);
    RmCopiesSay(stage, room);  // v3.58 RZ358: the Randomizer's line: what the copies were drawn as and made
    if (flyers && !g_enmLogged[9]++)
        Log("enemies: %d flying original(s) here: their copies keep the height above the floor", flyers);
    if (away)  // v3.34 EN334
        LogV("enemies: copies: room %d:%d: %d cop%s followed you into other rooms and live%s there: not made here", stage, room,
             away, away == 1 ? "y" : "ies", away == 1 ? "s" : "");
    if (x.dogs)  // v3.33 EN333
        LogV("enemies: copies: room %d:%d: %d event dog placement(s) (spawn mode 3 / 6 / 7): their copies keep the mode and "
             "sleep, wake and fight with them (story flag 130 %s)", stage, room, x.dogs,
             released ? "set: awake now, the copies are made awake" : ENM_DOGS_AWAKE ? "set: awake now"
                                                                    : "clear: asleep and unseen until the story releases them");
    for (i = 0; i < g_enmOrigN; ++i) {
        const EnmOrig *o = &g_enmOrig[i];
        const EnmCopyTally *T = &tal[i];
        char w[160];
        if (T->made + T->exists + T->away + T->recs >= per) continue;  // v3.34 EN334: + away  // v3.58 RZ358: + recs (said in one line)
        EnmSpotWhyText(T->why, w, sizeof w);
        Log("enemies:   %08X (type %02X v%d at %.0f %.0f %.0f, %d bones%s): %d of %d made; %d over the bone budget (a "
            "copy needs %d + the room's headroom), %d without a spot%s%s%s, %d killed before, %d failed%s", o->id, o->type,
            o->variant, o->pos[0], o->pos[1], o->pos[2], o->bones, o->flyer ? ", flying" : "", T->made, per, T->cut,
            EnmCost(o->type, o->bones), T->nospot, w[0] ? " (" : "", w, w[0] ? ")" : "", T->dead, T->failed,
            T->limit ? ", the room's limit stopped the rest" : T->badId ? ", its object number is outside the copy ids" : "");
    }
}
// a copy that died stays dead (the ring; the list is read every tick). v3.31 EN331: not a ghost (its 0x0B is a pin)
static void EnmDeadScan(void)
{
    const BYTE *t = NULL;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    int n = 0;
    while ((t = ENM_NEXT(t)) != NULL && n++ < 96) {
        const BYTE *w = EnmWork(t);
        unsigned id;
        if (!w || !EnmCopyKilled(*(const int *)(w + 0x20), *(const int *)(w + 0x3C))) continue;
        id = ENM_ID(t);
        if (EnmIsCopyId(id)) EnmRingAdd(&g_enmDead, EnmCopyKey(stage, half, id));
    }
}

// ---------------------------------------------------------------- Enemies Follow (PLAN 4)
// "Loaded here" (the table's Spawn Enemies / er_varok + er_res, live-verified there): a type / variant's model, motion
// and shadow files are listed for this world (index 0) or this room, and resident in the file cache.
static const BYTE kEnmRoomN[13] = { 0x00, 0x08, 0x1A, 0x18, 0x2C, 0x1D, 0x19, 0x23, 0x1A, 0x03, 0x0C, 0x08, 0x00 };
static int EnmInList(const DWORD *l, DWORD fid)
{
    int i;
    if (!l) return 0;
    for (i = 0; i < 80 && l[i] != 0x0FFFFFFF; ++i)
        if (l[i] == fid) return 1;
    return 0;
}
// v3.33 EN333: the keys Refined loaded for this room count as listed here (forgotten at the next room: the game parks them)
static int EnmExtraHas(DWORD fid)
{
    int i;
    if (g_enmExtraStage != ENM_STAGE || g_enmExtraRoom != ENM_ROOM) return 0;
    for (i = 0; i < g_enmExtraN; ++i)
        if (g_enmExtra[i] == fid) return 1;
    return 0;
}
static const BYTE *EnmCacheEntry(DWORD fid)  // FileCacheLookup's probe (0x13D1BF0, 128 x 0x14, open addressing)
{
    const BYTE *e;
    int i, k;
    for (i = 0, k = (int)(fid & 0x7F); i < 128; ++i, k = (k + 1) & 0x7F) {
        e = (const BYTE *)(0x13D1BF0 + 0x14 * k);
        if (*(const DWORD *)e == 0) return NULL;
        if (*(const DWORD *)e == fid) return e;
    }
    return NULL;
}
static int EnmResident(DWORD h)
{
    const int stage = ENM_STAGE, room = ENM_ROOM;
    const DWORD fid = h & 0xF800FFFF;
    const DWORD *const *tab;
    const BYTE *e;
    int k;
    if ((h & 0xF000F000) != 0xF000F000 || stage < 0 || stage > 12) return 0;
    k = stage + ((*(const BYTE *)0x13D812D & 4) ? 13 : 0);
    tab = *(const DWORD *const *const *)(0x62D514 + 4 * k);
    if (!tab) return 0;
    if (!EnmInList(tab[0], fid) && !(room >= 0 && room < kEnmRoomN[stage] && EnmInList(tab[room], fid)) &&
        !EnmExtraHas(fid))  // v3.33 EN333
        return 0;
    e = EnmCacheEntry(fid);  // v3.33 EN333 (was inline)
    return e && *(const DWORD *)(e + 0x0C) == 0 && *(const DWORD *)(e + 4) != 0;  // loaded, not parked
}
// v3.33 EN333: every whole file a type / variant's create reads: the model, its second file, the shadow file (none in
// Building room 10, 0x467F80) and every motion container (EnemyGetMotionHandle 0x467470; e.g. the Gum Head's 0x11F, the
// victims' 0x0BB / 0x121, Richard's and the Patient's 0x0E7 / 0x122). v3.32 checked the first three only.
#define ENM_TYPE_KEYS (3 + 128)  // v3.33 REV333-ENM-001 (inspection 1): the model, 2nd, shadow + 128 motion ids
static int EnmTypeKeys(int type, int variant, unsigned *keys, int max)
{
    int n = 0, id;
    if (type < 1 || type > 0x12 || variant < 0 || variant >= *(const int *)(0x5B8BA0 + 4 * type)) return 0;
    n = EnmKeyAdd(keys, n, max, ((DWORD(__cdecl *)(int, int))0x466C60)(type, variant));
    if (!n) return 0;  // no model: never loadable
    n = EnmKeyAdd(keys, n, max, ((DWORD(__cdecl *)(int, int))0x466F10)(type, variant));
    if (!(ENM_STAGE == 5 && ENM_ROOM == 0x0A))  // Building room 10: the game passes 0 for the 3rd handle (467F80)
        n = EnmKeyAdd(keys, n, max, ((DWORD(__cdecl *)(int, int))0x4671C0)(type, variant));
    for (id = 0; id < 128; ++id) n = EnmKeyAdd(keys, n, max, ((DWORD(__cdecl *)(int, int, int))0x467470)(type, variant, id));
    return n;
}
static int EnmLoadedHere(int type, int variant)
{
    unsigned keys[ENM_TYPE_KEYS];  // v3.33 REV333-ENM-001 (was 8: later motion files were dropped silently)
    const int n = EnmTypeKeys(type, variant, keys, ENM_TYPE_KEYS);
    int i;
    if (!n) return 0;
    for (i = 0; i < n; ++i)
        if (!EnmResident(keys[i])) return 0;
    return 1;
}
// v3.33 EN333: makes a type's files resident in this room: the missing whole files are requested, read to the end and set
// up as the room loader does. Only from EnmWrap (the room load, before the room's script: no event holds an arena mark,
// events are off until RoomInitLoad enables them). 1 = all resident; 0 = not (why: the reason for the log).
static const char *g_enmFilesWhy;
static int EnmEnsureFiles(int type, int variant, int *loadedKb)
{
    unsigned keys[ENM_TYPE_KEYS], miss[ENM_TYPE_KEYS];  // v3.33 REV333-ENM-001 (was 8)
    const BYTE *const arena = (const BYTE *)0x11B6000;  // gArena: base, cur, end (heap_551e40.c)
    unsigned need = 0, freeB;
    int n, m = 0, i;
    *loadedKb = 0;
    g_enmFilesWhy = "";
    if (g_enmExtraStage != ENM_STAGE || g_enmExtraRoom != ENM_ROOM)
        g_enmExtraN = 0, g_enmExtraStage = ENM_STAGE, g_enmExtraRoom = ENM_ROOM;  // a new room: the old list is parked
    n = EnmTypeKeys(type, variant, keys, ENM_TYPE_KEYS);
    if (!n) return g_enmFilesWhy = "the type has no model file", 0;
    for (i = 0; i < n; ++i)
        if (!EnmResident(keys[i])) {
            const BYTE *ent = ((const BYTE *(__cdecl *)(unsigned))0x573440)(keys[i]);  // FileGetTableEntry
            if (!ent) return g_enmFilesWhy = "a file id outside the file table", 0;
            need += EnmFileBlock(*(const int *)(ent + 4));
            miss[m++] = keys[i];
        }
    if (!m) return 1;
    if (!g_enmLoadFiles) return g_enmFilesWhy = "its files are not in this room's list (FollowLoadFiles=0)", 0;
    if (!g_enmFileFns) return g_enmFilesWhy = "the file loader is not the stock one (another mod?)", 0;
    if (g_enmExtraN + m > ENM_EXTRA_MAX) return g_enmFilesWhy = "16 extra files in this room already", 0;
    freeB = (unsigned)(*(BYTE *const *)(arena + 8) - *(BYTE *const *)(arena + 4));
    if (!EnmArenaFits(freeB, need, ENM_ARENA_KEEP)) {
        static char why[96];
        _snprintf_s(why, sizeof why, _TRUNCATE, "file memory: %u KB free, %u KB needed + 30 MB kept for the room", freeB >> 10,
                    need >> 10);
        return g_enmFilesWhy = why, 0;
    }
    for (i = 0; i < m; ++i) {
        const int r = ((int(__cdecl *)(unsigned))0x573470)(miss[i]);  // FileRequestAsync: 0 queued / already, 1 bad, 2 full
        if (r != 0) {
            if (LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "follower files: FileRequestAsync(%08X) refused (%d) with %u KB free", miss[i], r, freeB >> 10);
            ((int(__cdecl *)(int))0x573370)(0);  // (what was queued is read: the cache stays consistent)
            return g_enmFilesWhy = "the file loader refused a file", 0;
        }
    }
    ((int(__cdecl *)(int))0x573370)(0);  // FileQueueIsIdle(0): runs the read queue to the end (as the events do)
    for (i = 0; i < m; ++i) {
        ((int(__cdecl *)(unsigned))0x417D40)(miss[i]);  // the set-up RoomFilesSetup does (textures, buffers; idempotent)
        g_enmExtra[g_enmExtraN++] = miss[i];
    }
    *loadedKb = (int)(need >> 10);
    for (i = 0; i < n; ++i)
        if (!EnmResident(keys[i])) {
            if (LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "follower files: %08X of type %02X v%d is not resident after its read", keys[i], type, variant);
            return g_enmFilesWhy = "a file did not arrive", 0;
        }
    return 1;
}
// ---------------------------------------------------------------- v3.38 EN338: the room's space, what saw you
// the free slots of the game's 64 enemy records
static int EnmRecFree(void)
{
    const BYTE *r = ENM_RECORDS;
    int i, n = 0;
    for (i = 0; i < 64; ++i, r += 0x50) n += *(const short *)(r + 0x26) == 0;
    return n;
}
// the room's space (read at most once a tick, again after an arrival); the bones test is the caller's (per type:
// EnmBudgetFits with g_enmSpaceHead)
static const EnmSpace *EnmSpaceNow(void)
{
    if (!g_enmSpaceTick || g_enmSpaceTick != g_enmCdTick) {
        EnmSpace *s = &g_enmSpace;
        const int ikNum = *(volatile const int *)0xED73F8;
        s->listOk = EnmListCount(NULL) < ENM_LIST_MAX;
        s->tasksOk = TpGateFreeAll(EnmFreeTasks(), g_tpN, ENM_FREE_TASKS, EnmTaskAll());
        s->actorsOk = EnmFreeActors() >= ENM_FREE_ACTORS;
        s->bonesOk = 1;
        s->hitFree = (int)g_enmHitFree;
        s->ikFree = ikNum > 0 ? IkCountFree(*(const BYTE *volatile const *)0xED73F4, *(const BYTE *volatile const *)0xED7400, ikNum) : -1;
        s->recFree = EnmRecFree();
        g_enmSpaceHead = EnmHeadroom();
        g_enmSpaceTick = g_enmCdTick ? g_enmCdTick : 1;
    }
    return &g_enmSpace;
}
// why `type` does not fit now (ENM_SPACE_OK = it does); needRec: a copy whose record keeps a state
static int EnmSpaceFor(int type, int needRec)
{
    EnmSpace s = *EnmSpaceNow();
    s.bonesOk = EnmBudgetFits(type, 0, g_enmSpaceHead, NULL);
    return EnmSpaceWhy(&s, needRec);
}
// its EnemySense record found the player on its last test (Hummer, Gum Head, Twin Victim, Sniffer Dog; enemies_sim.h)
static int EnmSawYou(const BYTE *w, int type)
{
    const int off = EnmSenseOff(type);
    int r = 0;
    if (!off) return 0;
    __try {
        const BYTE *blk = *(const BYTE *const *)(w + 0x24), *s = NULL;
        if ((ULONG_PTR)blk >= 0x10000) s = *(const BYTE *const *)(blk + off);
        if ((ULONG_PTR)s >= 0x10000)
            r = EnmSenseSawPlayer(*(const int *)(s + 0x10), *(const unsigned *)(s + 0x18), *(const int *)(s + 0x3C));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 0;
    }
    return r;
}
static const char *EnmPurKind(int copy, int ghost) { return copy ? (ghost ? "ghost copy" : "copy") : ghost ? "ghost" : "monster"; }
// v3.58 EN358: a tagged ghost seen in this room's list (normal-play tick): its first sight as the real ghost is logged once
// (quota), the line at the next door sums them (EnmTagRoomSay)
static void EnmTagSee(unsigned id, int type, int cls, float d2)
{
    int i;
    if (g_enmTagRoom != ENM_ROOM) return;
    for (i = 0; i < g_enmGhostTagN; ++i) {
        if (!g_enmGhostTag[i] || g_enmGhostTag[i] != id) continue;
        {
            const int bits = EnmTagSeenBits(cls), first = (bits & ENM_TAG_IN) && !(g_enmTagSeen[i] & ENM_TAG_IN);
            const float d = d2 >= 0.0f ? sqrtf(d2) : -1.0f;
            g_enmTagSeen[i] |= (unsigned char)bits;
            if ((bits & ENM_TAG_IN) && d >= 0.0f && (g_enmTagNear[i] < 0.0f || d < g_enmTagNear[i])) g_enmTagNear[i] = d;
            if (first && LogQuota(&g_enmQuotaTagIn, 300))
                LogV("enemies: ghost %08X (type %02X) that followed you is in room %d now (class %02X, %.1f s after the door, %.0f units "
                     "from you)%s", id, type, ENM_ROOM, cls, g_enmClock - g_enmTagT0, d,
                     EnmIsGhostId(id) ? "" : " (a victim the randomizer drew)");
        }
        return;
    }
}
// v3.58 EN358: at the next door (or a load): the ghosts that followed you into the room you leave, one line
static void EnmTagRoomSay(const char *why)
{
    EnmTagSum s;
    float nearest = -1.0f;
    int i;
    if (!g_enmGhostTagN || g_enmTagRoom < 0) return;
    s = EnmTagSummary(g_enmGhostTag, g_enmTagSeen, g_enmGhostTagN);
    for (i = 0; i < g_enmGhostTagN; ++i)
        if (g_enmGhostTag[i] && g_enmTagNear[i] >= 0.0f && (nearest < 0.0f || g_enmTagNear[i] < nearest)) nearest = g_enmTagNear[i];
    if (s.tagged && LogQuota(&g_enmQuotaTagSum, 400))
        Log("enemies: ghosts that followed you into room %d: %d of %d came in (the real ghost in the room; nearest %.0f units from you), "
            "%d still arriving through the walls, %d never in the room (waiting outside: space or the room timer) (%s)", g_enmTagRoom,
            s.in, s.tagged, nearest, s.arriving, s.outside, why);
}

// the pursuers of this room (normal-play tick)
// v3.35 GH335: a pursuing victim seen in the room: downed (HP 0) or pinned ends its pursuit, else it is seen here
// v3.38 EN338: every pursuer; a ghost's pin or a monster's / copy's death ends it, down keeps it (the game's down)
static void EnmPursueSee(unsigned id, int type, int cls, int dead)
{
    EnmPursuer *e = EnmPursueFind(&g_enmPursue, id);
    int first, misses, v;
    if (!e) return;
    first = e->seen != g_enmDoorSerial, misses = e->misses;
    v = EnmPursueOnSight2(e, g_enmDoorSerial, cls, dead);
    if (v != ENM_PURSUE_KEEP) {
        e->used = 0;
        if (LogQuota(&g_enmQuotaPursue, 300))
            LogV("enemies: %s %08X (type %02X v%d) stops pursuing you in room %d: %s", EnmPurKind(e->copy, e->ghost), id, type,
                 e->variant, ENM_ROOM, kEnmPursueWhy[v]);
        return;
    }
    if (first && e->ghost && !e->copy && LogQuota(&g_enmQuotaWith, 300))  // (monsters and copies log their own arrival)
        LogV("enemies: pursuing ghost %08X (type %02X v%d) is in room %d with you%s", id, type, e->variant, ENM_ROOM,
             misses ? " (it caught up)" : "");
}
// v3.38 EN338: every eligible enemy of the room, pursuers first, then nearest: a monster (or its copy) once it spotted you in
// this room visit (near, hit or saw you: EnmSeenSee), a ghost (original or copy, down included, not pinned) always
static void EnmTakePursuers(const float *pp)
{
    EnmCand4 c[ENM_PUR_MAX];
    EnmPur p[ENM_PUR_MAX];
    EnmPurStats st;
    int idx[ENM_PUR_MAX], n = 0, m, i, guard = 0;
    const BYTE *t = NULL;
    const int mode = g_enmFollow;  // v3.49 EF349: read once
    const int victims = EnmFollowTakesVictims(mode) && g_enmGhostOn, monsters = EnmFollowTakesMonsters(mode);  // v3.49 EF349 (was == 2)
    const float r2 = g_enmFollowRange * g_enmFollowRange;
    const int pmode = (int)g_enmPursuit;  // v3.50 EF350: read once (Relentless = v3.49)
    if (g_enmSeen.serial != g_enmDoorSerial) EnmSeenReset(&g_enmSeen, g_enmDoorSerial);
    if (g_efTrack.serial != g_enmDoorSerial) EfTrackReset(&g_efTrack, g_enmDoorSerial);  // v3.50 EF350
    // an empty enemy list in the room the list was taken in is the room's exit teardown (0x462A20 clears the list
    // after the destroys wrote the records) [I]: keep the last list for the door. v3.33 EN333: only while it holds
    // someone (EnmPurKeep): an empty room keeps its empty list fresh (v3.32 called it "too old" at every door)
    if (EnmPurKeep(!ENM_NEXT(NULL), g_enmPurStage == ENM_STAGE && g_enmPurRoom == ENM_ROOM, g_enmPurN)) return;
    memset(&st, 0, sizeof st);
    while ((t = ENM_NEXT(t)) != NULL && guard++ < 96 && n < ENM_PUR_MAX) {
        const BYTE *w = EnmWork(t);
        const float *q = (const float *)w;
        unsigned id, mw;
        int type, variant, gkind, copy, why, cls, pur, how, fresh = 0;
        float d2, dy, ago = 0.0f;  // v3.50 EF350: + ago
        int lost = 0;              // v3.50 EF350
        if (!w) continue;
        type = *(const int *)(w + 0x20);
        id = ENM_ID(t);
        copy = EnmIsCopyId(id);  // v3.34 EN334: copies follow too, each on its own (FollowCopies=0: never, as v3.33)
        cls = *(const int *)(w + 0x3C);
        mw = EnmModeWord(t);
        variant = (int)(mw >> 8 & 0xF);
        gkind = EnmGhostPath(type, variant, copy, id);  // a ghost or a ghost's copy: its 0x0B is a pin, never death (v3.38 REV338 R1)  // v3.58 EN358: + a victim the randomizer drew (EnmDrawnGhost; was EnmGhostKind)
        if (gkind && !copy && g_enmGhostTagN && EnmFinite3(q))  // v3.58 EN358: a ghost that followed you: in the room yet?
            EnmTagSee(id, type, cls, (q[0] - pp[0]) * (q[0] - pp[0]) + (q[2] - pp[2]) * (q[2] - pp[2]));
        if (g_enmStoryFollow && EnmStoryNoGhostId(type, variant, copy, id) && LogQuota(&g_enmQuotaAnom, 20))  // v3.38 REV338 R1
            LogAnomaly("enemies", "story ghost %08X (type %02X v%d) in room %d has no ghost id (every stock placement has one): it "
                       "does not follow", id, type, variant, ENM_ROOM);
        EnmPursueSee(id, type, cls, gkind ? 0 : copy ? EnmCopyKilled(type, cls) : ENM_DEAD(t));
        if (copy && !g_enmFollowCopies) continue;
        if (copy) {  // a copy on its way (the ledger's room 0) cannot be in a room list
            EnmCw *e = EnmCwFind(&g_enmCw, EnmCopyKey(ENM_STAGE, ENM_HALF() ? 1 : 0, id));
            if (e && e->room == 0) {
                if (LogQuota(&g_enmQuotaAnom, 20))
                    LogAnomaly("enemies", "copy %08X (type %02X) is in room %d's enemy list while the copies' ledger has it on its "
                               "way: the ledger is set to this room", id, type, ENM_ROOM);
                e->room = ENM_ROOM;
            }
        }
        ++st.seen;
        if (!EnmFinite3(q) || (gkind ? (cls < 0 || cls == 0x0B || cls >= 0x10) : (!EnmStanding(cls) || ENM_DEAD(t)))) {
            ++st.down;
            continue;
        }
        why = EnmPursuerWhy4(monsters, copy, EnmIsGhostId(id) || EnmDrawnGhost(type, variant, copy, id), victims, g_enmStoryFollow, type, variant, mw, ENM_DOGS_AWAKE,  // v3.49 EF349: Why4  // v3.58 EN358: + drawn victims
                             ENM_AISTATE(t), EnmIsScriptId(id), copy || gkind ? 0 : ENM_KILLIDX(t));
        if (why == ENM_PUR_OTHER) {
            ++st.other;
            continue;
        }
        if (why == ENM_PUR_EVENT) {
            ++st.event;
            continue;
        }
        if (why == ENM_PUR_NOKILL) {
            ++st.nokill;
            continue;
        }
        d2 = (q[0] - pp[0]) * (q[0] - pp[0]) + (q[2] - pp[2]) * (q[2] - pp[2]);
        dy = q[1] - pp[1];
        pur = EnmPursueFind(&g_enmPursue, id) != NULL;
        if (gkind) {
            how = ENM_SEEN_NEAR;  // every ghost in the room pursues (the user's answer: "victims' ghosts: all in the room")
            if (EfDownLoses(pmode, 1, EnmStanding(cls))) {  // v3.50 EF350: a ghost lying down does not see where you go
                lost = 2;
                if (!pur) {
                    ++st.forgot;
                    continue;
                }
            }
        } else {
            // v3.50 EF350: near / saw / HP once (they were inline arguments)
            const int isNear = d2 < r2 && EnmCandDyOk(dy, EnmFollowDyMax(type)), saw = EnmSawYou(w, type);
            const float hp = ENM_HPSAFE(t, -1.0f);
            how = EnmSeenSee(&g_enmSeen, id, hp, isNear, saw, &fresh);
            if (how < 0) {
                if (LogQuota(&g_enmQuotaAnom, 20))
                    LogAnomaly("enemies", "the spotted marks of room %d are full (%d): %08X is not marked", ENM_ROOM, ENM_SEEN_N, id);
                how = 0;
            }
            if (fresh && !(how & ~fresh) && LogQuota(&g_enmQuotaSeen, 600))
                LogV("enemies: %s %08X (type %02X) spotted you in room %d (%s%s%s; %.0f units away): it follows you through your next "
                     "door, however far away it is then", copy ? "copy" : "monster", id, type, ENM_ROOM,
                     how & ENM_SEEN_NEAR ? "came near you" : "", how & ENM_SEEN_HIT ? (how & ENM_SEEN_NEAR ? ", was hit" : "was hit") : "",
                     how & ENM_SEEN_SAW ? (how & (ENM_SEEN_NEAR | ENM_SEEN_HIT) ? ", saw you" : "saw you") : "", sqrtf(d2));
            if (!how && !pur) {
                ++st.distant;  // (not spotted yet)
                continue;
            }
            if (EfLoses(pmode)) {  // v3.50 EF350: Enemy Pursuit: how long ago it last sensed you (near, saw you, a hit)
                ago = EfTrackSee(&g_efTrack, id, g_enmClock, isNear || saw || (fresh & ENM_SEEN_HIT) != 0, hp, pur, NULL);
                if (ago < -1.5f) ago = 0.0f;  // (the memory table is full: nobody loses track because of it)
                lost = EfLostAtDoor(pmode, 0, ago);
                if (lost && !pur) {  // it spotted you in this room visit but has lost you since: it would not follow
                    ++st.forgot;
                    continue;
                }
            }
        }
        if (!lost && EpIgnores(id)) {  // v3.51 EP351: it gave up the chase in this room and has not taken it up again
            lost = 3;
            if (!pur) {
                ++st.forgot;
                continue;
            }
        }
        c[n].d2 = d2, c[n].ok = 1, c[n].prio = pur;
        p[n].id = id, p[n].type = type, p[n].variant = variant, p[n].ghost = gkind, p[n].copy = copy, p[n].prio = pur, p[n].how = how;
        p[n].ago = ago, p[n].lost = lost;  // v3.50 EF350
        ++n;
    }
    m = EnmPickPursuers4(c, n, ENM_PUR_MAX, idx);
    g_enmPurStats = st;
    for (i = 0; i < m; ++i) g_enmPur[i] = p[idx[i]];
    memset(g_enmPurHave, 0, sizeof g_enmPurHave);  // v3.34 EN334: a new list: no copy record taken yet
    g_enmPurN = m;
    g_enmPurStage = ENM_STAGE;
    g_enmPurRoom = ENM_ROOM;
    g_enmPurTime = GetTickCount();
}

// ---------------------------------------------------------------- v3.38 EN338: the door (EnmWrap, before the room script)
// Who comes through: the pursuers in the room you left (nearest first), the pursuers away (on their way or left in other
// rooms of this world: real monsters by their record, copies by the copies' ledger, ghosts by their record), then the newly
// spotted ones of the room you left (EnmDoorSlots). FollowMax (32) of them come; the rest wait for a later door. A follower
// waits too when the story guard refuses this room change (EnmStoryFollowWhy), its files cannot be loaded here, or (a monster
// away) it lies down in its room. Nothing is ever dropped by time.
typedef struct { int stage, half, from, room, f2DC, f3BF, slots, nm, nc, ng, fromList, fromAway, wFull, wFiles, wStory, wDown,
                 wNoRec, here, lost, lostSight, lostDown, lostBehind; char who[1600]; size_t at; } EnmDoorCtx;  // v3.50 EF350: + lostSight / Down / Behind
static int EnmQueueAdd(unsigned id, int type, int variant, int fromRoom, int copy, int ghost, const BYTE *rec, float clock)
{
    EnmFollower *f;
    int j;
    if (g_enmQN >= ENM_FOLLOW_MAX) return 0;
    for (j = 0; j < g_enmQN; ++j)
        if (g_enmQ[j].id == id) {
            if (LogQuota(&g_enmQuotaAnom, 20)) LogAnomaly("enemies", "follower %08X (type %02X) is queued twice at one door", id, type);
            return 0;
        }
    f = &g_enmQ[g_enmQN];
    f->id = id, f->type = type, f->variant = variant, f->fromRoom = fromRoom;
    f->due = EnmFollowDue(g_enmQN, g_enmFollowDelay);
    f->done = 0, f->copy = copy, f->ghost = ghost, f->said = 0, f->clock = clock;
    if (rec) memcpy(f->rec, rec, sizeof f->rec);
    else memset(f->rec, 0, sizeof f->rec);
    ++g_enmQN;
    return 1;
}
static void EnmPursueEnd(EnmPursuer *e, int v, int room)
{
    e->used = 0;
    if (e->copy) EnmTransitHome(e->id, e->stage);
    if (LogQuota(&g_enmQuotaPursue, 300))
        LogV("enemies: %s %08X (type %02X v%d) stops pursuing you at the door to room %d: %s", EnmPurKind(e->copy, e->ghost), e->id,
             e->type, e->variant, room, kEnmPursueWhy[v]);
}
// one follower: 1 = it comes through this door, 0 = it waits (it pursues you either way). listed = it stood in the room you
// left (a copy then brings the record its destroy wrote: rec); else it is away (rec = its ledger record, clock its time)
static int EnmDoorOne(EnmDoorCtx *x, unsigned id, int type, int variant, int copy, int ghost, int fromRoom, const BYTE *rec,
                      float clock, int listed)
{
    const unsigned key = EnmCopyKey(x->stage, x->half, id);
    const int sf = EnmStoryFollowWhy(type, x->stage, fromRoom, x->room, x->f2DC, x->f3BF);
    const char *wait = NULL;
    EnmPursuer *e;
    int kb = 0, a;
    if (sf != ENM_SF_OK) {
        ++x->wStory, wait = kEnmStoryFollowWhy[sf];
        if (LogQuota(&g_enmQuotaStory, 300))
            LogV("enemies: %s %08X (type %02X v%d) does not come from room %d into room %d: %s (it keeps pursuing you)",
                 EnmPurKind(copy, ghost), id, type, variant, fromRoom, x->room, wait);
    } else if (x->slots <= 0 || (ghost && !copy && g_enmGhostTagN >= ENM_FOLLOW_MAX) || ((!ghost || copy) && g_enmQN >= ENM_FOLLOW_MAX)) {
        ++x->wFull, wait = "FollowMax is full";
    } else if (!EnmEnsureFiles(type, variant, &kb)) {
        ++x->wFiles, wait = "its files cannot be loaded here";
        if (LogQuota(&g_enmQuotaPursue, 300))
            LogV("enemies: %s %08X (type %02X v%d) waits: its files cannot be loaded in room %d (%s)", EnmPurKind(copy, ghost), id,
                 type, variant, x->room, g_enmFilesWhy);
    }
    if (listed) {  // it follows from now on (or waits for a later door): a pursuer for the rest of this world visit
        a = EnmPursueAdd2(&g_enmPursue, id, type, variant, x->stage, g_enmDoorSerial - 1, copy, ghost);
        if (a < 0 && LogQuota(&g_enmQuotaAnom, 20))
            LogAnomaly("enemies", "the pursuers' table is full (%d): %s %08X follows through this door only", ENM_PURSUERS_N,
                       EnmPurKind(copy, ghost), id);
        else if (a == 1 && LogQuota(&g_enmQuotaPursue, 300))
            LogV("enemies: %s %08X (type %02X v%d) now pursues you for the rest of this world visit (until it is %s, a load or you "
                 "leave the world)", EnmPurKind(copy, ghost), id, type, variant, ghost ? "pinned" : "killed");
    }
    if ((e = EnmPursueFind(&g_enmPursue, id)) != NULL) e->door = g_enmDoorSerial;
    if (wait) {
        if (copy && listed && rec && !EnmCwFind(&g_enmCw, key)) {  // it lives in the room it stood in (not made at home again)
            EnmCw *c;
            if (EnmCwPut(&g_enmCw, key, id, fromRoom, type, variant, rec))
                Log("enemies: the copies' ledger is full (%d): the copy that followed you longest ago is made in its own room again",
                    ENM_CW_N);
            if ((c = EnmCwFind(&g_enmCw, key)) != NULL) c->clock = clock;
        }
        return 0;
    }
    if (kb)
        Log("enemies: %s %08X (type %02X v%d): its files were loaded into room %d for it (%d KB; parked with the room when you "
            "leave)", EnmPurKind(copy, ghost), id, type, variant, x->room, kb);
    --x->slots;
    if (ghost && !copy) {
        g_enmGhostTag[g_enmGhostTagN++] = id;
        ++x->ng;
    } else {
        EnmQueueAdd(id, type, variant, fromRoom, copy, ghost, rec, clock);
        ++x->nm;
        if (copy) {  // on its way (the ledger's room 0): its home and the room it left do not make it meanwhile
            EnmCw *c = EnmCwFind(&g_enmCw, key);
            ++x->nc;
            if (!c) {
                if (EnmCwPut(&g_enmCw, key, id, 0, type, variant, rec))
                    Log("enemies: the copies' ledger is full (%d): the copy that followed you longest ago is made in its own room "
                        "again", ENM_CW_N);
                if ((c = EnmCwFind(&g_enmCw, key)) != NULL) c->clock = clock;
            } else {
                if (listed) {
                    int i;
                    for (i = 0; i < ENM_REC_SIZE; ++i) c->rec[i] = rec[i];
                    c->clock = clock;
                }
                c->room = 0;
            }
        }
    }
    if (listed) ++x->fromList;
    else ++x->fromAway;
    if (x->at + 1 < sizeof x->who) {
        const int w = _snprintf_s(x->who + x->at, sizeof x->who - x->at, _TRUNCATE, "%s%08X %s %02X%s", x->at ? ", " : "", id,
                                  EnmPurKind(copy, ghost), type, listed ? "" : " (away)");
        if (w > 0) x->at += (size_t)w;
    }
    return 1;
}
// a pursuer away at this door (not in the room you left)
static void EnmDoorAway(EnmDoorCtx *x, EnmPursuer *e)
{
    // (FollowKeepDoors 0: lost at the first door away); v3.50 EF350: by Enemy Pursuit (Relentless: FollowKeepDoors as v3.49)
    const int pmode = (int)g_enmPursuit, maxMisses = EfMaxMisses(pmode, g_enmKeepDoors);
    const BYTE *r;
    int v;
    e->door = g_enmDoorSerial;
    if (e->stage != x->stage) {
        EnmPursueEnd(e, ENM_PURSUE_GONE, x->room), ++x->lost;
        return;
    }
    if (EfDownLoses(pmode, e->seen == g_enmDoorSerial - 1, e->standing)) {  // v3.50 EF350: down in the room you left
        e->used = 0;
        if (e->copy) EnmTransitHome(e->id, e->stage);
        ++x->lost, ++x->lostDown;
        if (LogQuota(&g_enmQuotaPursue, 300))
            LogV("enemies: %s %08X (type %02X v%d) lost track of you at the door to room %d: it lay down when you left (Enemy "
                 "Pursuit %s; it stays in room %d)", EnmPurKind(e->copy, e->ghost), e->id, e->type, e->variant, x->room,
                 EfPursuitNameOf(pmode), x->from);
        return;
    }
    if (!e->copy) {  // a real monster or a ghost: its record
        r = ENM_RECFIND(e->id);
        v = EnmPursueAtDoor2(&e->misses, maxMisses, x->stage, e->ghost, r ? *(const short *)(r + 0x26) : -1,
                             r ? *(const signed char *)(r + 0x36) : -1, r ? *(const signed char *)(r + 0x37) : -1);
        if (v != ENM_PURSUE_KEEP) {
            EnmPursueEnd(e, v, x->room), ++x->lost;
            x->lostBehind += v == ENM_PURSUE_BEHIND;  // v3.50 EF350
            return;
        }
        if (!e->ghost) {
            if (*(const signed char *)(r + 0x37) == x->room) {  // its record is here: the room makes it (its home / a wanderer)
                ++x->here;
                return;
            }
            if (!EnmStanding(*(const short *)(r + 0x26))) {  // down in its room: it comes once it stands
                ++x->wDown;
                return;
            }
        }
        EnmDoorOne(x, e->id, e->type, e->variant, 0, e->ghost, *(const signed char *)(r + 0x37), NULL, 0.0f, 0);
        return;
    }
    {   // a copy: the copies' ledger keeps its room and its record
        const unsigned key = EnmCopyKey(x->stage, x->half, e->id);
        EnmCw *c = EnmCwFind(&g_enmCw, key);
        if (!c) {
            EnmPursueEnd(e, ENM_PURSUE_HOME, x->room), ++x->lost;
            return;
        }
        if (EnmRingHas(&g_enmDead, key)) {
            c->used = 0;
            EnmPursueEnd(e, ENM_PURSUE_DIED, x->room), ++x->lost;
            return;
        }
        if (c->room == x->room) {  // it lives here (EnmReplayCopyWanderers makes it after the script)
            ++x->here;
            return;
        }
        if (maxMisses > 0 && ++e->misses >= maxMisses) {
            EnmPursueEnd(e, ENM_PURSUE_BEHIND, x->room), ++x->lost, ++x->lostBehind;  // v3.50 EF350: + lostBehind
            return;
        }
        if (e->ghost && *(const short *)(c->rec + 0x26) == 0x0B) {  // pinned in its room: it stays with its Sword
            EnmPursueEnd(e, ENM_PURSUE_PINNED, x->room), ++x->lost;
            return;
        }
        if (!e->ghost && *(const short *)(c->rec + 0x26) != 0 && !EnmStanding(*(const short *)(c->rec + 0x26))) {
            ++x->wDown;
            return;
        }
        EnmDoorOne(x, e->id, e->type, e->variant, 1, e->ghost, c->room > 0 ? c->room : x->from, c->rec, c->clock, 0);
    }
}
// v3.50 EF350: one line at a door where enemies lost track of you (Enemy Pursuit; logcheck sums them by reason)
static volatile LONG g_efQuotaLost;
static void EfDoorLostSay(int door, int from, int room, int sight, int down, int behind, int forgot)
{
    if ((!sight && !down && !behind && !forgot) || !LogQuota(&g_efQuotaLost, 400)) return;
    Log("enemies: through door %d from room %d to %d: Enemy Pursuit %s: %d lost track of you (%d out of their senses, %d down "
        "when you left, %d did not catch up); %d more had spotted you but forgot", door, from, room,
        EfPursuitNameOf((int)g_enmPursuit), sight + down + behind, sight, down, behind, forgot);
}
static void EnmDoorPass(int room)
{
    const int stage = ENM_STAGE, door = *(volatile const int *)0xFD5AA8;
    const int elevator = (*(volatile const BYTE *)0x13D8113 & 0x20) != 0;
    static EnmDoorCtx x;  // (1.6 KB of names: not on the game thread's stack)
    int i, why, pend = 0, pass;
    EpRoomEnd("you left the room");  // v3.51 EP351: the room visit's in-room give-ups (before the serial moves on)
    for (i = 0; i < g_enmQN; ++i) {
        pend += !g_enmQ[i].done;
        // v3.38 REV338 R3: one without a pursuit (the pursuers' table was full) would stay on its way: it goes home
        if (!g_enmQ[i].done && g_enmQ[i].copy && !EnmPursueFind(&g_enmPursue, g_enmQ[i].id)) EnmTransitHome(g_enmQ[i].id, g_enmQStage);
    }
    if (pend && LogQuota(&g_enmQuotaFollow, 300))  // v3.38 EN338: they wait for a later door (v3.37: left in the old room)
        LogV("enemies: %d follower(s) had not come into room %d when you left it: they keep pursuing you (a later door)", pend,
             g_enmQRoom);
    EnmTagRoomSay("you left the room");  // v3.58 EN358: the ghosts that followed you into the room you leave
    g_enmTagRoom = -1;
    g_enmQN = g_enmGhostTagN = 0;
    g_enmTagSaidIn = g_enmTagSaidDenied = g_enmTagSaidWait = g_enmTagSaidStory = 0;  // v3.38 EN338
    g_enmDoorSet = 0;
    ++g_enmDoorSerial;  // v3.35 GH335: a new room visit (every room load)
    g_enmSpaceTick = 0;  // v3.38 REV338 EN338-002: the new room's space is read before any arrival is checked
    InterlockedExchange(&g_enmHitFree, -1);  // v3.38 REV338 EN338-004: not the old room's count (HpTick gives the new one)
    if (!EnmFollowLive()) return;
    // v3.30 EN330: every room change of a monster world is logged with its decision; v3.38 EN338: no list age
    why = EnmDoorVerdict2(stage, g_enmPurStage, g_enmPurRoom, room, door, elevator, ENM_PREVROOM);
    if (why == ENM_DOOR_STAGE || why == ENM_DOOR_OTHERSTAGE) EnmPursueDrop("you left the world");  // v3.35 GH335
    if (why == ENM_DOOR_STAGE) {
        g_enmPurN = 0;
        return;
    }
    if (why != ENM_DOOR_OK) {  // (a warp, a lift or the same room: the pursuers wait for the next real door)
        if (why != ENM_DOOR_OTHERSTAGE || g_enmPurN)
            Log("enemies: room %d -> %d (door %d): %s: nobody comes through (%d eligible in that room; %d pursuer(s) wait for the "
                "next real door)", g_enmPurRoom, room, door, kEnmDoorWhy[why], g_enmPurN, EnmPursueCount(&g_enmPursue));
        g_enmPurN = 0;
        return;
    }
    if (!g_enmPurN && !EnmPursueCount(&g_enmPursue)) {
        const EnmPurStats *s = &g_enmPurStats;
        Log("enemies: through door %d from room %d to %d: nobody follows (%d enem%s there: %d had not spotted you, %d down or "
            "dead, %d event spawns not awake, %d without a kill index, %d of no following kind)", door, g_enmPurRoom, room, s->seen,
            s->seen == 1 ? "y" : "ies", s->distant, s->down, s->event, s->nokill, s->other);
        EfDoorLostSay(door, g_enmPurRoom, room, 0, 0, 0, s->forgot);  // v3.50 EF350
        return;
    }
    memset(&x, 0, sizeof x);
    x.stage = stage, x.half = ENM_HALF() ? 1 : 0, x.from = g_enmPurRoom, x.room = room;
    x.f2DC = ENM_FLAG(0x2DC), x.f3BF = ENM_FLAG(0x3BF);
    x.slots = g_enmFollowMax;
    for (pass = 0; pass < 3; ++pass) {
        if (pass == 1) {  // the pursuers away, the oldest pursuit first (table order)
            for (i = 0; i < ENM_PURSUERS_N; ++i)
                if (g_enmPursue.e[i].used && g_enmPursue.e[i].door != g_enmDoorSerial) {
                    int j, listed = 0;
                    for (j = 0; j < g_enmPurN; ++j) listed |= g_enmPur[j].id == g_enmPursue.e[i].id;
                    if (!listed) EnmDoorAway(&x, &g_enmPursue.e[i]);
                }
            continue;
        }
        for (i = 0; i < g_enmPurN; ++i) {
            const EnmPur *p = &g_enmPur[i];
            if ((pass == 0) != (p->prio != 0)) continue;
            if (p->lost) {  // v3.50 EF350: Enemy Pursuit: a pursuer that lost track of you (out of its senses, or down) stays
                EnmPursuer *e = EnmPursueFind(&g_enmPursue, p->id);
                if (e) {
                    e->used = 0;
                    if (e->copy) EnmTransitHome(e->id, e->stage);
                }
                ++x.lost;
                if (p->lost == 2) ++x.lostDown;
                else ++x.lostSight;
                if (LogQuota(&g_enmQuotaPursue, 300)) {
                    char why[64];
                    if (p->lost == 2) strcpy_s(why, sizeof why, "it lay down when you left");
                    else if (p->lost == 3) strcpy_s(why, sizeof why, "it gave up the chase in that room");  // v3.51 EP351
                    else if (p->ago < 0.0f) strcpy_s(why, sizeof why, "it had not sensed you in that room");
                    else sprintf_s(why, sizeof why, "it had not sensed you for %.0f s", p->ago);
                    LogV("enemies: %s %08X (type %02X v%d) lost track of you at the door to room %d: %s (Enemy Pursuit %s; it "
                         "stays in room %d)", EnmPurKind(p->copy, p->ghost), p->id, p->type, p->variant, room, why,
                         EfPursuitNameOf((int)g_enmPursuit), g_enmPurRoom);
                }
                continue;
            }
            if (p->copy && !g_enmPurHave[i]) {  // v3.34 EN334: a copy goes with the record its destroy wrote at the door
                ++x.wNoRec;
                Log("enemies: copy %08X (type %02X v%d) stays in room %d: it wrote no record at the door (its state is not kept)",
                    p->id, p->type, p->variant, g_enmPurRoom);
                continue;
            }
            EnmDoorOne(&x, p->id, p->type, p->variant, p->copy, p->ghost, g_enmPurRoom, p->copy ? g_enmPurRec[i] : NULL, g_enmClock, 1);
        }
    }
    g_enmGhostTagStage = g_enmQStage = stage;
    g_enmQRoom = room;
    if (x.ng) {  // v3.58 EN358: the tags of this door: what becomes of them is summed at the next door (EnmTagRoomSay)
        memset(g_enmTagSeen, 0, sizeof g_enmTagSeen);
        for (i = 0; i < ENM_FOLLOW_MAX; ++i) g_enmTagNear[i] = -1.0f;
        g_enmTagT0 = g_enmClock, g_enmTagRoom = room;
    }
    Log("enemies: through door %d from room %d to %d: %d follow (%d monster(s), %d of them copies, %d ghost(s); %d from that room, "
        "%d pursuing from other rooms or on their way): %s; %d wait for a later door (%d FollowMax %d full, %d files, %d story "
        "rule, %d down in their room), %d already here, %d stopped, %d in that room had not spotted you", door, g_enmPurRoom, room,
        x.nm + x.ng, x.nm, x.nc, x.ng, x.fromList, x.fromAway, x.at ? x.who : "nobody", x.wFull + x.wFiles + x.wStory + x.wDown,
        x.wFull, g_enmFollowMax, x.wFiles, x.wStory, x.wDown, x.here, x.lost, g_enmPurStats.distant);
    EfDoorLostSay(door, g_enmPurRoom, room, x.lostSight, x.lostDown, x.lostBehind, g_enmPurStats.forgot);  // v3.50 EF350
    if (x.nm && LogQuota(&g_enmQuotaDoor, 400))
        LogV("enemies: follow: the monsters and copies come through the door %.1f s after you arrive (+1.5 s each), once you are "
             "300 units from the door (else after 10 s at a spot away from you); one the room has no space for waits (never given "
             "up); the ghosts come by the game's own arrival, one a tick while the room has space", g_enmFollowDelay);
    g_enmPurN = 0;
}
// a record moved to this room, its placement replayed from its home room's script (gate 4 = the type's restore)
static int EnmReplay(unsigned id, int room, const float *spot, BYTE *rec, const char *why)
{
    __declspec(align(16)) BYTE d[0x94];
    __declspec(align(16)) float in[4], st[4];
    BYTE saved[0x50];
    BYTE *t;
    if (!EnmDescFor(id, d)) {
        if (!g_enmLogged[2]++) Log("enemies: %s %08X: its placement could not be rebuilt: it stays where it was", why, id);
        return 0;
    }
    *(unsigned *)(d + 0x24) &= ~0xFu;  // v3.30 EN330: spawn mode 0 (a dog's 3 / 7 would put it back to sleep at create)
    memcpy(saved, rec, sizeof saved);
    if (spot) {
        in[0] = spot[0], in[1] = spot[1], in[2] = spot[2], in[3] = ((const float *)rec)[3];
        ENM_TO_REC(st, in);
        memcpy(rec, st, 16);
        *(signed char *)(rec + 0x37) = (signed char)room;
        memcpy(d + 0x14, spot, 12);
    } else {
        ENM_FROM_REC(st, (const float *)rec);
        memcpy(d + 0x14, st, 12);
    }
    t = ENM_CREATE(d, room, id);
    t = t ? ENM_FIND(id) : NULL;
    if (t && EnmWork(t)) return 1;
    if (ENM_RECFIND(id) == rec && *(const unsigned *)(rec + 0x20) == id) memcpy(rec, saved, sizeof saved);  // undo the move
    if (!g_enmLogged[3]++) Log("enemies: %s %08X: the game did not create it here: its record is put back", why, id);
    return 0;
}
// v3.34 EN334: a copy's descriptor away from its room: its original's placement (the copy id holds its room and object
// number) rebuilt from that room's script, made as EnmMakeCopies makes it (no id override, no kill index), awake (spawn
// mode 0: a follower or a wanderer only stands in an awake mode)
static int EnmCopyDescFor(unsigned cid, int type, int variant, BYTE *d)  // v3.58 RZ358: + variant (the copy's record's)
{
    EnmFindCtx x;
    if (!EnmIsCopyId(cid) || EnmCopyRoomOf(cid) <= 0) return 0;
    x.want = EnmCopyNOf(cid), x.found = 0, x.out = d;
    EnmWalk(EnmCopyRoomOf(cid), EnmFindCb, &x);
    if (!x.found || *(const int *)d != 2) return 0;
    if (EnmTypeOfDesc(d) != type) {  // v3.58 RZ358: + a copy of the type it drew (Enemies Same World / All Worlds)
        if (!RmCopyRetype(cid, type, variant, d)) return 0;
    } else if (variant >= 0 && variant != (int)(*(const unsigned *)(d + 0x24) >> 8 & 0xFu)) {
        RmCopyRetype(cid, type, variant, d);  // (its own variant; else as v3.57: the original's)
    }
    *(unsigned *)(d + 0x2C) = *(const unsigned *)0x10A0EAC;  // no id override
    *(int *)(d + 0x30) = 0;                                  // no kill index
    *(unsigned *)(d + 0x24) &= 0xF00u;                       // its variant, spawn mode 0
    return 1;
}
// v3.34 EN334: makes copy cid of `type` in this room from a record of it (gate 4 = the type's restore: its HP and state)
// at `spot` (a follower) or at the record's own place (spot NULL). out = the record as it was put (for the ledger).
// The record goes into a free slot of the game's table only for the create and is deleted at once (copies keep none).
// v3.41 EN341: fresh = a ghost copy made with no record of its id in the table (EnmGhostCopyMake FRESH: the game's create
// refuses a ghost whose record is in a room the rule keeps ghosts out of), at full HP; *story = 1 when a refusal is the
// placement's own story flags (the game's rule, not an anomaly)
static int EnmCopyReplay(unsigned cid, int type, int room, const float *spot, const BYTE *srec, BYTE *out, int fresh, int *story)
{
    __declspec(align(16)) BYTE d[0x94];
    __declspec(align(16)) float in[4], st[4];
    BYTE *slot = NULL, *r, *t;
    int i, ok;
    if (story) *story = 0;  // v3.41 EN341
    if (!EnmCopyDescFor(cid, type, srec ? *(const signed char *)(srec + 0x33) : -1, d)) {  // v3.58 RZ358: + its variant
        if (LogQuota(&g_enmQuotaCopyF, 200))
            LogV("enemies: copy %08X: its placement (room %d object %d) could not be rebuilt as type %02X: not made in room %d",
                 cid, EnmCopyRoomOf(cid), EnmCopyNOf(cid), type, room);
        return 0;
    }
    memcpy(out, srec, 0x50);
    if (!fresh && *(const short *)(out + 0x26) != 0 && *(const short *)(out + 0x24) == type && EnmFinite3((const float *)out))  // v3.41 EN341: + !fresh
        for (i = 0, r = ENM_RECORDS; i < 64; ++i, r += 0x50)
            if (*(const short *)(r + 0x26) == 0) {
                slot = r;
                break;
            }
    *(unsigned *)(out + 0x20) = cid;
    *(signed char *)(out + 0x36) = (signed char)ENM_STAGE;
    *(signed char *)(out + 0x37) = (signed char)room;
    *(int *)(out + 0x38) = 0;  // age
    if (spot) {
        in[0] = spot[0], in[1] = spot[1], in[2] = spot[2], in[3] = ((const float *)out)[3];
        ENM_TO_REC(st, in);
        memcpy(out, st, 16);
        memcpy(d + 0x14, spot, 12);
    } else {
        memcpy(in, out, 16);
        ENM_FROM_REC(st, in);
        if (!EnmFinite3(st)) return 0;
        memcpy(d + 0x14, st, 12);
    }
    if (fresh) {  // v3.41 EN341: no record of its id for the gate (0x464F20) to find; the ledger keeps it as made: full HP, up
        *(float *)(out + 0x28) = ENM_MAXHP(type, *(const signed char *)(out + 0x33));
        *(float *)(out + 0x3C) = 0.0f;
        for (i = 0; i < 64 && (r = ENM_RECFIND(cid)) != NULL; ++i) ENM_RECDEL(r);
    }
    if (slot) memcpy(slot, out, 0x50);  // (no free slot or no state kept: a fresh copy there, full HP)
    t = ENM_CREATE(d, room, cid);
    for (i = 0; i < 64 && (r = ENM_RECFIND(cid)) != NULL; ++i) ENM_RECDEL(r);  // (the restore deleted it already)
    t = t ? ENM_FIND(cid) : NULL;
    ok = t && EnmWork(t) && !ENM_DEAD(t);
    if (!ok && story) {  // v3.41 EN341: the placement's story flags (info+0x44 / +0x48 = d+0x54 / d+0x58) refuse it now?
        const int need = *(const int *)(d + 0x54), forbid = *(const int *)(d + 0x58);
        const int fl = EnmPlacementFlagsOk(need, need > 0 && need < ENM_STORY_FLAGS_N && ENM_FLAG(need), forbid,
                                           forbid > 0 && forbid < ENM_STORY_FLAGS_N && ENM_FLAG(forbid));
        *story = fl == 0;
    }
    return ok;
}
static void EnmWatchAdd(unsigned id, int type, int room, int kind, const float *put)  // v3.34 EN334 (was inline)
{
    EnmArrived *a;
    if (g_enmArrivedN >= ENM_WATCH_MAX) return;
    a = &g_enmArrived[g_enmArrivedN++];
    a->id = id, a->type = type, a->room = room, a->at = GetTickCount(), a->seen = 0, a->kind = kind;
    if (put) memcpy(a->put, put, sizeof a->put);
    else memset(a->put, 0, sizeof a->put);
}
// v3.34 EN334: a copy follower comes in from the record it wrote at the door and lives here (the copies' ledger)
// v3.38 EN338: rec = its record as it comes (a ghost copy's aged); the room rule is tested by the caller (EnmFollowArrivals)
// v3.41 EN341: + fresh, *story (EnmCopyReplay)
static int EnmCopyArrive(const EnmFollower *f, int room, const float *spot, const BYTE *rec, int fresh, int *story)
{
    BYTE out[0x50];
    EnmCw *e;
    if (!EnmCopyReplay(f->id, f->type, room, spot, rec, out, fresh, story)) return 0;
    if (EnmCwPut(&g_enmCw, EnmCopyKey(ENM_STAGE, ENM_HALF() ? 1 : 0, f->id), f->id, room, f->type, f->variant, out))
        Log("enemies: the copies' ledger is full (%d): the copy that followed you longest ago is made in its own room again",
            ENM_CW_N);
    if ((e = EnmCwFind(&g_enmCw, EnmCopyKey(ENM_STAGE, ENM_HALF() ? 1 : 0, f->id))) != NULL) e->clock = g_enmClock;  // v3.35 GH335
    return 1;
}
// v3.38 EN338: a follower that waits says why once per reason (bit = 1 << reason; spot / down / room gates above the space ones)
#define ENM_SAID_SPOT (1 << 16)
#define ENM_SAID_DOWN (1 << 17)
#define ENM_SAID_NEVER (1 << 18)
#define ENM_SAID_TIMER (1 << 19)
static int EnmWaitSay(EnmFollower *f, int bit)
{
    if (f->said & bit) return 0;
    f->said |= bit;
    return LogQuota(&g_enmQuotaSpace, 600);
}
// a copy follower whose pursuit ends at the arrival (the game's story refusal, the create refused): it goes home
static void EnmCopyGoHome(EnmFollower *f, int v)
{
    EnmCw *c = EnmCwFind(&g_enmCw, EnmCopyKey(ENM_STAGE, ENM_HALF() ? 1 : 0, f->id));
    EnmPursuer *e = EnmPursueFind(&g_enmPursue, f->id);
    f->done = 1;
    if (c) c->used = 0;
    if (e) e->used = 0;
    if (LogQuota(&g_enmQuotaPursue, 300))
        LogV("enemies: copy %08X (type %02X) stops pursuing you at room %d: %s: it is made in its own room %d again", f->id, f->type,
             ENM_ROOM, kEnmPursueWhy[v], EnmCopyRoomOf(f->id));
}
// v3.41 REV341 EN341-R1: its pursuit ends but its ledger entry stays (a pinned / downed ghost copy the game cannot
// bring back in this room: it stays where the ledger has it, as EnmReplayCopyWanderers' ENM_GCM_LATER keeps it)
static void EnmCopyStay(EnmFollower *f)
{
    EnmPursuer *e = EnmPursueFind(&g_enmPursue, f->id);
    f->done = 1;
    if (e) e->used = 0;
}
static void EnmFollowArrivals(const float *pp)
{
    const int stage = ENM_STAGE, room = ENM_ROOM;
    const float T = ENM_ROOM_TIMER;
    int i;
    if (g_enmQStage != stage || g_enmQRoom != room) {
        g_enmQN = 0;
        return;
    }
    for (i = 0; i < g_enmQN; ++i) {
        EnmFollower *f = &g_enmQ[i];
        float dx, dz, away, spot[3];
        int step, ok, why, needRec = 0, gcm = ENM_GCM_RECORD, story = 0;  // v3.41 EN341: + gcm, story
        BYTE *rec = NULL, arr[0x50];
        if (f->done || !(T >= f->due)) continue;
        dx = pp[0] - g_enmDoor[0], dz = pp[2] - g_enmDoor[2];
        away = sqrtf(dx * dx + dz * dz);
        step = EnmArriveStep2(T - f->due, away);  // v3.38 EN338: never given up (v3.37: after 20 s)
        if (!step) continue;
        if (ENM_FIND(f->id)) {
            f->done = 1;
            continue;
        }
        rec = f->copy ? NULL : ENM_RECFIND(f->id);  // v3.34 EN334: a copy has its own record (f->rec)
        if (!f->copy && (!rec || *(const signed char *)(rec + 0x36) != stage || *(const signed char *)(rec + 0x37) != f->fromRoom ||
            *(const short *)(rec + 0x26) == 0x0B)) {
            f->done = 1;  // no record (this type or state does not persist; the table's Randomizer may have deleted it)
            // v3.33 EN333: its record already in this room (moved by someone else) is a record moved twice
            if (rec && *(const signed char *)(rec + 0x36) == stage && *(const signed char *)(rec + 0x37) == room &&
                LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "follower %08X (type %02X): its record is already in room %d before Refined moved it "
                           "(a record moved twice?)", f->id, f->type, room);
            else
                Log("enemies: follower %08X (type %02X) has no record of room %d (%s): it stays there", f->id, f->type, f->fromRoom,
                    !rec ? "none: its type or state is not kept, or the cheat table deleted it"
                         : *(const short *)(rec + 0x26) == 0x0B ? "it died there" : "the record is in another room");
            continue;
        }
        if (!EnmLoadedHere(f->type, f->variant)) {
            f->done = 1;
            if (LogQuota(&g_enmQuotaAnom, 20))  // v3.33 EN333: EnmDoorPass made them resident: they cannot be gone
                LogAnomaly("enemies", "follower %08X (type %02X v%d): its files are not resident in room %d any more (they were "
                           "at the door): it comes through a later door", f->id, f->type, f->variant, room);
            continue;
        }
        if (f->copy) {
            memcpy(arr, f->rec, sizeof arr);
            if (f->ghost) {  // v3.38 EN338: a ghost copy: its record ages as the game ages a ghost record (0x4853F0)
                float cd = *(const float *)(arr + 0x3C), hp = *(const float *)(arr + 0x28);
                const float mx = ENM_MAXHP(f->type, *(const signed char *)(arr + 0x33));
                if (*(const short *)(arr + 0x26) != 0 && _finite(cd) && _finite(hp) && _finite(mx)) {
                    EnmGhostDecay(&cd, &hp, mx, ENM_DIFFICULTY(), g_enmClock - f->clock);
                    *(float *)(arr + 0x3C) = cd, *(float *)(arr + 0x28) = hp;
                    if (cd > 0.0f) {  // down: the game decides when it gets up (its countdown), then it comes
                        if (EnmWaitSay(f, ENM_SAID_DOWN))
                            LogV("enemies: ghost copy %08X waits: it lies down (%.0f s left on its countdown): it comes once it is up",
                                 f->id, cd);
                        continue;
                    }
                }
                {   // the game's room rule (0x464880 on its record here): a story flag's refusal ends its pursuit
                    // v3.41 EN341: and it decides how the copy is made (EnmGhostCopyMake): on a plain "no" with no record of
                    // its id in the table (the game's create refuses a ghost whose record is here: 0x464F20 gate 1)
                    BYTE r[0x50];
                    int yes, den;
                    memcpy(r, arr, sizeof r);
                    *(signed char *)(r + 0x36) = (signed char)stage, *(signed char *)(r + 0x37) = (signed char)room;
                    yes = ENM_MAYENTER(r, room) != 0;
                    den = ENM_DENIED;  // (read at once: the call sets it)
                    gcm = EnmGhostCopyMake(yes, den, *(const short *)(arr + 0x26), *(const float *)(arr + 0x3C));
                    if (gcm == ENM_GCM_DENIED) {
                        EnmCopyGoHome(f, ENM_PURSUE_DENIED);
                        continue;
                    }
                    if (gcm == ENM_GCM_LATER) {  // (its record pinned, or a countdown that is not a number: never queued so)
                        if (LogQuota(&g_enmQuotaAnom, 20))
                            LogAnomaly("enemies", "ghost copy %08X (type %02X) follows with a record the game cannot restore in room "
                                       "%d (state %02X): its pursuit ends, it stays in the ledger where it was", f->id, f->type,
                                       room, *(const short *)(arr + 0x26));
                        EnmCopyStay(f);  // v3.41 REV341 EN341-R1 (was EnmCopyGoHome: that dropped its ledger entry)
                        continue;
                    }
                }
                if (g_enmTimerFn) {  // the game's room gate of ghosts (0x46F9C0: never, or a delay in some rooms)
                    const int g = EnmRoomTimerGate(T, ENM_GHOSTTIMER());
                    if (g != ENM_RT_OK) {
                        if (EnmWaitSay(f, g == ENM_RT_NEVER ? ENM_SAID_NEVER : ENM_SAID_TIMER))
                            LogV("enemies: ghost copy %08X waits: %s", f->id, g == ENM_RT_NEVER
                                     ? "the game lets no ghost into this room now (it comes through a later door)"
                                     : "the game's room timer for ghosts here has not run out yet");
                        continue;
                    }
                }
            }
            needRec = gcm == ENM_GCM_FRESH ? 0  // v3.41 EN341: (made with no record: no record slot needed)
                    : EnmCopyNeedsRecord(*(const short *)(arr + 0x26), *(const float *)(arr + 0x28),
                                         ENM_MAXHP(f->type, *(const signed char *)(arr + 0x33)));
        }
        why = EnmSpaceFor(f->type, needRec);  // v3.38 EN338: the space gates (list, tasks, actors, bones, hit shapes, IK, record)
        if (why != ENM_SPACE_OK) {
            if (EnmWaitSay(f, 1 << why))
                LogV("enemies: %s %08X (type %02X) waits in front of room %d: %s (it comes once there is space, or through a later "
                     "door)", EnmPurKind(f->copy, f->ghost), f->id, f->type, room, kEnmSpaceWhy[why]);
            continue;
        }
        ok = EnmFindSpot(g_enmDoor, step == 1 ? f->id : f->id ^ 0x77u, 0, 0.0f, pp, kEnmDoorTiers[step == 1 ? 0 : 1],
                         ENM_DOOR_TIERS, spot, NULL);  // v3.30 EN330: three rings each
        if (!ok) {
            if (EnmWaitSay(f, ENM_SAID_SPOT))  // v3.38 EN338: once per follower (was a session quota)
                LogV("enemies: follower %08X waits: no free spot %s (door at %.0f %.0f %.0f, you %.0f units away)", f->id,
                     step == 1 ? "near the door" : "on the outer rings", g_enmDoor[0], g_enmDoor[1], g_enmDoor[2], away);
            continue;
        }
        f->done = 1;
        {   // v3.33 EN333: one record of an id only (a second one = a record moved twice)
            BYTE *r = ENM_RECORDS;
            int k, same = 0;
            for (k = 0; k < 64; ++k, r += 0x50)
                same += *(const short *)(r + 0x26) != 0 && *(const unsigned *)(r + 0x20) == f->id;
            if (same > 1 && LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "follower %08X (type %02X): %d records of its id in the table (a record moved twice?)",
                           f->id, f->type, same);
        }
        ok = f->copy ? EnmCopyArrive(f, room, spot, arr, gcm == ENM_GCM_FRESH, &story)  // v3.41 EN341: + fresh, story
                     : EnmReplay(f->id, room, spot, rec, "follower");  // v3.34 EN334: + copies
        g_enmSpaceTick = 0;  // v3.38 EN338: (the room changed: read its space again)
        if (ok) {
            Log("enemies: %s %08X (type %02X) came through the door into room %d after %.1f s", f->copy ? "copy" : "follower",
                f->id, f->type, room, T);
            if (gcm == ENM_GCM_FRESH && LogQuota(&g_enmQuotaFresh, 300))  // v3.41 EN341
                LogV("enemies: ghost copy %08X (type %02X v%d) came into room %d as the game's own ghosts arrive: made without its "
                     "record, standing at full HP (the room rule keeps ghosts out of room %d, and the game does not create a ghost "
                     "whose record is there)", f->id, f->type, f->variant, room, room);
            EnmWatchAdd(f->id, f->type, room, f->copy ? 2 : 0, spot);  // v3.33 EN333: watched for 3 s
        } else if (f->copy && story) {  // v3.41 EN341: its placement's story flags keep it out now: the game's rule, it goes home
            if (LogQuota(&g_enmQuotaPursue, 300))
                LogV("enemies: copy %08X (type %02X) is not made in room %d: the story flags of its placement keep it out now (the "
                     "game's rule, as in its own room)", f->id, f->type, room);
            EnmCopyGoHome(f, ENM_PURSUE_HOME);
        } else if (f->copy) {  // v3.34 EN334; v3.38 EN338: its pursuit ends, it is made in its own room again
            if (LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "copy %08X (type %02X): the game did not create it in room %d with its files resident (it "
                           "is made in its own room %d again; %s)", f->id, f->type, room, EnmCopyRoomOf(f->id),
                           !f->ghost ? "a monster copy" : gcm == ENM_GCM_FRESH ? "a ghost copy made without its record"
                                                                               : "a ghost copy made from its record: the room rule said yes");  // v3.41 EN341
            EnmCopyGoHome(f, ENM_PURSUE_HOME);
        } else if (LogQuota(&g_enmQuotaAnom, 20)) {  // v3.33 EN333
            LogAnomaly("enemies", "follower %08X (type %02X): the game did not create it in room %d with its files resident "
                       "(its record is put back in room %d)", f->id, f->type, room, f->fromRoom);
        }
        return;  // one arrival per tick
    }
}
// v3.33 EN333: a follower that came in must still be in the list (or lie dead / down) a few seconds later; one that
// vanished while you stay in the room was lost between the rooms. v3.34 EN334: wanderers and copies too (kind), and the
// 3 s line says where it is (from where it was put), its class / AI state / HP and whether the game draws it
static const char *const kEnmWatchKind[4] = { "follower", "wanderer", "copy", "wandering copy" };
static void EnmWatchArrived(void)
{
    int i;
    for (i = 0; i < g_enmArrivedN;) {
        EnmArrived *a = &g_enmArrived[i];
        const char *kind = kEnmWatchKind[a->kind & 3];
        const BYTE *t;
        if (a->room != ENM_ROOM) {  // you left: nothing to watch
            g_enmArrived[i] = g_enmArrived[--g_enmArrivedN];
            continue;
        }
        t = ENM_FIND(a->id);
        if (!t && !a->seen) {  // v3.34 EN334: missing for 0.5 s (an exit teardown empties the list before the room changes)
            a->seen = (int)(GetTickCount() | 1);
            ++i;
            continue;
        }
        if (!t && GetTickCount() - (DWORD)a->seen < 500) {
            ++i;
            continue;
        }
        if (t) a->seen = 0;
        if (!t) {
            if (LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "%s %08X (type %02X) came into room %d and is gone %.1f s later (not in the enemy list: %s)",
                           kind, a->id, a->type, a->room, (GetTickCount() - a->at) / 1000.0f,
                           a->kind == 1 || a->kind == 3 ? "its replay from its record did not last" : "lost between the rooms");
            g_enmArrived[i] = g_enmArrived[--g_enmArrivedN];
            continue;
        }
        if (GetTickCount() - a->at > 3000) {
            const BYTE *w = EnmWork(t);
            if (w) {
                const float *q = (const float *)w;
                const BYTE *ar = *(const BYTE *const *)(t + 0x41C);  // the actor record (+0x0C bit 0 enabled: 0x52B1C0)
                const int cls = *(const int *)(w + 0x3C), on = (ULONG_PTR)ar >= 0x10000 && (*(const int *)(ar + 0x0C) & 1);
                const float px = q[0] - *(const float *)0x10A0F60, pz = q[2] - *(const float *)0x10A0F68;
                const float mx = q[0] - a->put[0], my = q[1] - a->put[1], mz = q[2] - a->put[2];
                if (LogQuota(&g_enmQuotaFollow, 200))
                    LogV("enemies: %s %08X (type %02X) is in room %d 3 s after it came in (class %02X, AI state %02X, HP %.0f, "
                         "at %.0f %.0f %.0f: %.0f from where it was put, %.0f units from you; %s)", kind, a->id, a->type,
                         a->room, cls, ENM_AISTATE(t), ENM_HPSAFE(t, -1.0f), q[0], q[1], q[2], sqrtf(mx * mx + my * my + mz * mz),  // v3.35.1 HF3351
                         sqrtf(px * px + pz * pz), on && EnmClassDrawn(cls) ? "drawn" : on ? "not drawn in this class" : "disabled: not drawn, no update");
                if (!on && !ENM_DEAD(t) && LogQuota(&g_enmQuotaAnom, 20))  // (a type may disable its dead body)
                    LogAnomaly("enemies", "%s %08X (type %02X) in room %d is in the enemy list but disabled (class %02X): it "
                               "cannot be seen", kind, a->id, a->type, a->room, cls);
            }
            g_enmArrived[i] = g_enmArrived[--g_enmArrivedN];
            continue;
        }
        ++i;
    }
}
// v3.34 EN334: where a wanderer is put (ENM_WSPOT_*): its record's spot when the copies' floor test passes there (tested
// from 100 above it, then at its own height; the spot put onto that floor), else the first ring spot near it that
// passes, else the record's spot as v3.33 did. A Hummer keeps its spot (it may fly). v3.33 replayed the record's spot untested.
static int EnmWanderSpot(unsigned id, int type, const float *at, float *out)
{
    float base[3], cand[3], pp[3];
    memcpy(out, at, 12);
    if (type == 0x03) return ENM_WSPOT_FLYER;
    if (EnmGhostType(type)) return ENM_WSPOT_GHOST;  // v3.35 GH335: a victim's copy passes walls and floats: its record's spot
    base[0] = at[0], base[1] = at[1] - 100.0f, base[2] = at[2];  // (y grows downwards)
    memcpy(cand, base, 12);
    if (EnmSpotTest(base, cand, 0, 0.0f) == ENM_SPOT_OK) {
        memcpy(out, cand, 12);
        return EnmSnapKind(at[1], cand[1]);
    }
    memcpy(cand, at, 12);  // (something low over it: tested at its own height, as the copies are)
    if (EnmSpotTest(at, cand, 0, 0.0f) == ENM_SPOT_OK) {
        memcpy(out, cand, 12);
        return EnmSnapKind(at[1], cand[1]);
    }
    EnmPlayerPos(pp, 1);
    if (EnmFindSpot(base, id, 0, 0.0f, pp, kEnmCopyTiers, ENM_COPY_TIERS, cand, NULL)) {
        memcpy(out, cand, 12);
        return ENM_WSPOT_RING;
    }
    return ENM_WSPOT_KEPT;
}
// EnmWrap, after the room script and the copies: wanderers recorded in this room come back
// v3.34 EN334: the type, variant, state and HP are read BEFORE the replay: the type's restore deletes the record
// (SnifferDogRestoreRecord 0x48DB70 -> EnemyRecordDelete 0x464380), so v3.33's "is back" line read type 00 from the
// cleared slot. The spot is tested (EnmWanderSpot) and the wanderer is watched for 3 s (EnmWatchArrived).
static void EnmReplayWanderers(int room)
{
    BYTE *r = ENM_RECORDS;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    int i, n = 0;
    for (i = 0; i < 64 && n < ENM_WANDER_MAX; ++i, r += 0x50) {
        const unsigned id = *(const unsigned *)(r + 0x20);
        const int type = *(const short *)(r + 0x24), variant = *(const signed char *)(r + 0x33), state = *(const short *)(r + 0x26);  // v3.34 EN334
        const float hp = *(const float *)(r + 0x28);
        __declspec(align(16)) float at[4];
        float spot[3];
        int how;
        if (state == 0 || *(const signed char *)(r + 0x37) != room ||
            !EnmIsWanderer(id, type, *(const signed char *)(r + 0x36), room, stage, half))
            continue;
        if (ENM_FIND(id)) continue;
        ++n;
        {   // v3.33 EN333: its files are loaded here when missing (EnmEnsureFiles), and the log says which test failed
            int kb = 0;
            const int files = EnmEnsureFiles(type, variant, &kb);
            if (!files || !EnmRoomForOne() || !EnmBudgetFits(type, 0, EnmHeadroom(), NULL)) {
                Log("enemies: wanderer %08X (type %02X) does not fit in room %d now (%s%s): its record stays for a later visit",
                    id, type, room, !files ? "files: " : !EnmRoomForOne() ? "the room's enemy / task limit" : "the bone budget",
                    !files ? g_enmFilesWhy : "");
                continue;
            }
            if (kb) LogV("enemies: wanderer %08X: its files were loaded into room %d (%d KB)", id, room, kb);
        }
        ENM_FROM_REC(at, (const float *)r);  // v3.34 EN334: its record's spot, tested
        if (!EnmFinite3(at)) continue;
        how = EnmWanderSpot(id, type, at, spot);
        if (EnmReplay(id, room, spot, r, "wanderer")) {
            Log("enemies: wanderer %08X (type %02X v%d, home room %d) is back in room %d", id, type, variant, EnmScriptRoom(id), room);
            if (LogQuota(&g_enmQuotaWander, 200))
                LogV("enemies: wanderer %08X put at %.0f %.0f %.0f: %s (record at %.0f %.0f %.0f, state %02X, HP %.0f when you "
                     "left)", id, spot[0], spot[1], spot[2], kEnmWspot[how], at[0], at[1], at[2], state, hp);
            EnmWatchAdd(id, type, room, 1, spot);
        }
    }
}
// v3.34 EN334: the copies that followed you into this room before (the copies' ledger) come back from their last record
static void EnmReplayCopyWanderers(int room)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    int i, n = 0;
    for (i = 0; i < ENM_CW_N && n < ENM_WANDER_MAX; ++i) {
        EnmCw *e = &g_enmCw.e[i];
        BYTE out[0x50];
        __declspec(align(16)) float in[4], at[4];
        float spot[3];
        int kb = 0, files, how, gcm = ENM_GCM_RECORD, story = 0;  // v3.41 EN341: + gcm, story
        if (!e->used || e->room != room || (int)(e->key >> 17 & 0x1F) != stage || (int)(e->key >> 16 & 1) != half) continue;
        if (ENM_FIND(e->cid)) continue;
        if (EnmRingHas(&g_enmDead, e->key)) {  // killed (its record was not seen): dead until you leave the world
            e->used = 0;
            continue;
        }
        if (EnmGhostType(e->type)) {  // v3.35 GH335: a victim's copy: its pin, its record aged, the room rule
            const int st = *(const short *)(e->rec + 0x26);
            float cd = *(const float *)(e->rec + 0x3C), hp = *(const float *)(e->rec + 0x28);
            const float mx = ENM_MAXHP(e->type, *(const signed char *)(e->rec + 0x33));
            const char *why = NULL;
            if (st == 0x0B && !EnmPinHeld(e->cid, stage, room)) why = "its Sword pin is gone";
            else if (!_finite(cd) || !_finite(hp) || !_finite(mx)) why = "its record is not readable";
            if (!why) {
                EnmGhostDecay(&cd, &hp, mx, ENM_DIFFICULTY(), g_enmClock - e->clock);
                *(float *)(e->rec + 0x3C) = cd, *(float *)(e->rec + 0x28) = hp;
                e->clock = g_enmClock;
                *(signed char *)(e->rec + 0x37) = (signed char)room;
                // v3.38 EN338: only a story flag's refusal keeps it out (a plain "no" is overridden, as for the originals)
                // v3.41 EN341: on a plain "no" it is made without its record (EnmGhostCopyMake), or not now when pinned / down
                {
                    const int yes = ENM_MAYENTER(e->rec, room) != 0, den = ENM_DENIED;
                    gcm = EnmGhostCopyMake(yes, den, st, cd);
                }
                if (gcm == ENM_GCM_DENIED) why = "a story flag keeps that ghost out (the game's room rule)";
                else if (gcm == ENM_GCM_LATER) {  // (it stays in the ledger here: a later visit, or a later door while it pursues)
                    if (LogQuota(&g_enmQuotaCopyF, 200))
                        LogV("enemies: copy %08X (type %02X) is not made in room %d now: it %s, and the game cannot bring a ghost "
                             "back so in a room its rule keeps ghosts out of (it stays here in the ledger: %s)", e->cid, e->type,
                             room, st == 0x0B ? "is pinned by the Sword" : "lies down",
                             st == 0x0B ? "its Sword stays where it was; once the Sword is pulled out it is made in its own room"
                                        : "it comes once it is up, or after you through a later door");
                    continue;
                }
            }
            if (why) {
                EnmPursuer *pu = EnmPursueFind(&g_enmPursue, e->cid);  // v3.38 EN338: (it goes home: its pursuit ends)
                if (pu) pu->used = 0;
                e->used = 0;
                if (LogQuota(&g_enmQuotaCopyF, 200))
                    LogV("enemies: copy %08X (type %02X) is not made in room %d (%s): it is made in its own room %d again",
                         e->cid, e->type, room, why, EnmCopyRoomOf(e->cid));
                continue;
            }
        }
        ++n;
        files = EnmEnsureFiles(e->type, e->variant, &kb);
        if (!files || !EnmRoomForOne() || !EnmBudgetFits(e->type, 0, EnmHeadroom(), NULL)) {
            Log("enemies: copy %08X (type %02X) does not fit in room %d now (%s%s): it stays in the ledger for a later visit",
                e->cid, e->type, room, !files ? "files: " : !EnmRoomForOne() ? "the room's enemy / task limit" : "the bone budget",
                !files ? g_enmFilesWhy : "");
            continue;
        }
        memcpy(in, e->rec, 16);
        ENM_FROM_REC(at, in);
        if (!EnmFinite3(at)) continue;
        how = EnmWanderSpot(e->cid, e->type, at, spot);
        if (EnmCopyReplay(e->cid, e->type, room, spot, e->rec, out, gcm == ENM_GCM_FRESH, &story)) {  // v3.41 EN341: + fresh, story
            memcpy(e->rec, out, sizeof out);
            Log("enemies: copy %08X (type %02X, of room %d) is back in room %d (it followed you here before)", e->cid, e->type,
                EnmCopyRoomOf(e->cid), room);
            if (gcm == ENM_GCM_FRESH && LogQuota(&g_enmQuotaFresh, 300))  // v3.41 EN341
                LogV("enemies: ghost copy %08X (type %02X v%d) is back in room %d as the game's own ghosts arrive: made without its "
                     "record, standing at full HP (the room rule keeps ghosts out of room %d, and the game does not create a ghost "
                     "whose record is there)", e->cid, e->type, e->variant, room, room);
            if (LogQuota(&g_enmQuotaWander, 200))
                LogV("enemies: copy %08X put at %.0f %.0f %.0f: %s (state %02X, HP %.0f when you left)", e->cid, spot[0], spot[1],
                     spot[2], kEnmWspot[how], *(const short *)(e->rec + 0x26), *(const float *)(e->rec + 0x28));
            EnmWatchAdd(e->cid, e->type, room, 3, spot);
        } else if (EnmGhostType(e->type)) {  // v3.35 GH335: (a victim's copy refused by the gate goes home)
            EnmPursuer *pu = EnmPursueFind(&g_enmPursue, e->cid);  // v3.38 EN338: (its pursuit ends)
            if (pu) pu->used = 0;
            e->used = 0;
            Log("enemies: copy %08X (type %02X) could not be made in room %d%s: it is made in its own room %d again", e->cid,
                e->type, room, story ? " (the story flags of its placement keep it out now: the game's rule)"  // v3.41 EN341
                                     : gcm == ENM_GCM_FRESH ? " (made without its record)" : "", EnmCopyRoomOf(e->cid));
        } else {
            Log("enemies: copy %08X (type %02X) could not be made in room %d: it stays in the ledger", e->cid, e->type, room);
        }
    }
}
static int EnmGhostTagged(const BYTE *rec)  // v3.35 GH335: the tag's index + 1 (0 = not tagged)
{
    unsigned id;
    int i;
    if (!g_enmGhostTagN || !EnmFollowTakesVictims(g_enmFollow) || g_enmFail) return 0;  // v3.49 EF349: + Victims Only (was != 2)
    id = *(const unsigned *)(rec + 0x20);
    if (*(const signed char *)(rec + 0x36) != g_enmGhostTagStage || g_enmGhostTagStage != ENM_STAGE) return 0;
    for (i = 0; i < g_enmGhostTagN; ++i)
        if (g_enmGhostTag[i] == id) return i + 1;
    return 0;
}
// 0x486199: was `call 464880` (EnemyGhostMayEnterRoom) in EnemyGhostArrivalCheck 0x486150: a tagged victim may enter
// v3.38 EN338: a tagged ghost (victim or story ghost) is let in while the game's own gates allow it (a story flag's refusal
// ends its pursuit; the story guard; the room timer 0x46F9C0, tested by the caller right after), one a tick, and only while
// the room has space (EnmSpaceFor); else it waits (the game asks again every frame). The stock "yes" is never touched.
// v3.38 REV338 R2: a tagged ghost passes the same gates on the stock "yes" too (EnmGhostAdmit); untagged ones stay stock.
static int __cdecl EnmGhostRoom(BYTE *rec, int room)
{
    const int r = ((int(__cdecl *)(BYTE *, int))0x464880)(rec, room);
    const int denied = ENM_DENIED;  // (read at once: the stock call sets it)
    if (!rec) return r;
    __try {
        const int k = EnmGhostTagged(rec);
        if (k) {
            const unsigned id = *(const unsigned *)(rec + 0x20), bit = 1u << ((k - 1) & 31);
            const int type = *(const short *)(rec + 0x24);
            int sf, why = ENM_SPACE_OK, timerOk, tickFree, ga;
            sf = EnmStoryFollowWhy(type, ENM_STAGE, *(const signed char *)(rec + 0x37), room, ENM_FLAG(0x2DC), ENM_FLAG(0x3BF));
            timerOk = !g_enmTimerFn || EnmRoomTimerGate(ENM_ROOM_TIMER, ENM_GHOSTTIMER()) == ENM_RT_OK;
            tickFree = g_enmGhostAdmitTick != g_enmCdTick;
            if (!(!r && denied) && sf == ENM_SF_OK && timerOk && tickFree) why = EnmSpaceFor(type, 0);
            ga = EnmGhostAdmit(r, denied, 1, sf == ENM_SF_OK, timerOk, tickFree, why == ENM_SPACE_OK);
            if (ga == ENM_GA_STOCK) return r;  // (the room timer has not run out: the caller's own test keeps it out)
            // v3.35 GH335: a story flag's refusal (0x464880 sets gEnemyManager.denied) is never overridden
            if (ga == ENM_GA_DENIED) {
                EnmPursuer *e = EnmPursueFind(&g_enmPursue, id);
                if (!(g_enmTagSaidDenied & bit) && LogQuota(&g_enmQuotaPursue, 300))
                    LogV("enemies: ghost %08X may not come into room %d: a story flag keeps it out (kept as the game says); its "
                         "pursuit ends", id, room);
                g_enmTagSaidDenied |= bit;
                if (e) e->used = 0;          // v3.38 EN338: a story refusal ends the pursuit
                g_enmGhostTag[k - 1] = 0;    // (no longer tagged for this room)
                return 0;
            }
            if (sf != ENM_SF_OK) {  // v3.38 EN338: its record moved (the game's own roaming) into a guarded room since the door
                if (!(g_enmTagSaidStory & bit) && LogQuota(&g_enmQuotaStory, 300))
                    LogV("enemies: ghost %08X does not come from room %d into room %d: %s (it keeps pursuing you)", id,
                         *(const signed char *)(rec + 0x37), room, kEnmStoryFollowWhy[sf]);
                g_enmTagSaidStory |= bit;
                return 0;
            }
            if (!tickFree) return 0;  // one a tick
            if (why != ENM_SPACE_OK) {
                if (!(g_enmTagSaidWait & bit) && LogQuota(&g_enmQuotaSpace, 600))
                    LogV("enemies: ghost %08X waits outside room %d: %s (it comes once there is space)", id, room, kEnmSpaceWhy[why]);
                g_enmTagSaidWait |= bit;
                return 0;
            }
            g_enmGhostAdmitTick = g_enmCdTick, g_enmSpaceTick = 0;
            if (!r && !g_enmLogged[5]++) Log("enemies: victim %08X follows into room %d (the room rule said no)", id, room);
            if (!(g_enmTagSaidIn & bit) && LogQuota(&g_enmQuotaPursue, 300))  // v3.35 GH335: once per victim and door
                LogV("enemies: ghost %08X (type %02X) may come into room %d (it follows you; the room rule said %s)", id, type, room,
                     r ? "yes: let in with space" : "no");
            g_enmTagSaidIn |= bit;
            return 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_enmGhostTagN = 0;
    }
    return r;
}
// v3.59 EN359: 0x465123, the `call 464880` (EnemyGhostMayEnterRoom) in EnemySpawnGate 0x464F20: a ghost's own pinned
// record in this room, with its Sword's pin here, is let in (enemies_sim.h EnmPinStays); every other answer is the
// stock call's ([Gameplay] PinnedGhostsStay=0, hidden: the site stays stock)
static int g_enmPinGateOn, g_enmPinGateIni = 1;
static LONG g_enmPinKept;
static volatile LONG g_enmQuotaPinGate;
static int __cdecl EnmGatePin(BYTE *rec, int room)
{
    const int r = ((int(__cdecl *)(BYTE *, int))0x464880)(rec, room);
    const int denied = ENM_DENIED;
    if (r || !rec) return r;
    __try {
        const unsigned id = *(const unsigned *)(rec + 0x20);
        const int stage = ENM_STAGE, st = *(const short *)(rec + 0x26), type = *(const short *)(rec + 0x24);
        if (EnmPinStays(r, denied, st, type, *(const signed char *)(rec + 0x36), *(const signed char *)(rec + 0x37), stage, room,
                        ENM_ROOM, st == 0x0B && EnmPinHeld(id, stage, room))) {
            InterlockedIncrement(&g_enmPinKept);
            if (LogQuota(&g_enmQuotaPinGate, 100))
                Log("enemies: ghost %08X (type %02X v%d) lies pinned by its Sword in room %d:%d: it is made pinned there again (the game's "
                    "ghost room rule says no to its record in this room: a victim the Randomizer drew or one that followed you here)",
                    id, type, *(const signed char *)(rec + 0x33), stage, room);
            return 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return r;
}

// ---------------------------------------------------------------- v3.35 GH335: the down countdown never gets shorter from a hit
// Every gameplay tick each ghost's class and down countdown [[work+0x28]+0x98] (the value hudbars.inc draws) is compared
// with the last dispatcher tick's (enemies_sim.h EnmCdKeep): a re-roll that lowered it is put back to the time left. Only
// consecutive ticks compare (g_enmCdTick counts every dispatcher tick, so an event, a pause or a menu between them never
// gives back time the game drained), a room change or a load starts afresh. The Sword's pin (class 0x0B), the short down
// after the Sword is pulled out (EnmCdStep) and the drain itself are never touched; ghost copies are in the list too.
static void EnmCdWatch(void)
{
    const DWORD key = (DWORD)ENM_STAGE << 16 | (DWORD)(ENM_ROOM & 0xFFFF);
    const BYTE *t = NULL;
    float dt = ENM_FRAMETIME;
    int guard = 0, i;
    if (key != g_enmCdRoomKey) {
        memset(g_enmCdT, 0, sizeof g_enmCdT);
        g_enmCdRoomKey = key;
    }
    if (!_finite(dt) || dt < 0.0f) dt = 1.0f / 30.0f;
    while ((t = ENM_NEXT(t)) != NULL && guard++ < 96) {
        const BYTE *w = EnmWork(t), *gw;
        EnmCdTrack *e = NULL, *fr = NULL;
        unsigned id;
        int type, cls;
        float cd = 0.0f, keep;
        if (!w) continue;
        type = *(const int *)(w + 0x20);
        if (!EnmCdGhostType(type)) continue;
        gw = *(const BYTE *const *)(w + 0x28);
        if ((ULONG_PTR)gw < 0x10000) continue;
        cls = *(const int *)(w + 0x3C);
        id = ENM_ID(t);
        if (cls == 0x0A) {  // (only a down ghost's block is read: the follower placeholder 0x0F has another one)
            cd = *(const float *)(gw + 0x98);
            if (!_finite(cd)) {
                if (LogQuota(&g_enmQuotaAnom, 20))
                    LogAnomaly("enemies", "ghost %08X (type %02X) lies down (class 0A) with a countdown that is no number: left "
                               "as it is", id, type);
                continue;
            }
        }
        for (i = 0; i < ENM_CD_TRACK_N; ++i) {
            EnmCdTrack *x = &g_enmCdT[i];
            if (x->tick && x->t == t && x->w == w && x->id == id) {
                e = x;
                break;
            }
            if (!fr && (!x->tick || g_enmCdTick - x->tick > 1)) fr = x;
        }
        if (!e && fr) memset(fr, 0, sizeof *fr);  // (a new ghost here: no pin, nothing to compare)
        if (!e) e = fr;
        if (!e) continue;
        if ((keep = EnmCdStep(&e->s, e->tick && e->tick + 1 == g_enmCdTick, cls, cd, dt)) > 0.0f) {
            *(float *)(gw + 0x98) = keep;
            ++g_enmCdKept;
            if (LogQuota(&g_enmQuotaCd, 200))
                LogV("enemies: ghost %08X (type %02X) down: a hit re-rolled its countdown to %.1f s: %.1f s kept (a hit never "
                     "shortens it; %d kept this session)", id, type, cd, keep, g_enmCdKept);
        }
        e->t = t, e->w = w, e->id = id, e->tick = g_enmCdTick;
    }
}

// ---------------------------------------------------------------- v3.35 GH335: copies for victims that come in later
// A victim original that comes into the room after the room load (the game's own arrival 0x486150, or a follower) gets
// its copies beside it then: the same rules as at the load (EnmMakeOne: the bone budget, the room's limits, the spot
// finder, the ghost-copy store, the dead ring and the copies' ledger; never saved). Its placement comes from its FFFF00nn
// id (EnmGhostHome); its copies' ids are that placement's, as its own room makes them.
typedef struct { unsigned id; int stage, half, room, n; } EnmHome;
static EnmHome g_enmHome[32];  // the victims' placements found (room 0: none), this world visit
static int g_enmHomeN;
typedef struct { unsigned want; int n, found; BYTE *out; } EnmHomeCtx;
static int EnmHomeCb(void *c, int n, const BYTE *desc)
{
    EnmHomeCtx *x = (EnmHomeCtx *)c;
    if (*(const int *)desc != 2 || *(const unsigned *)(desc + 0x2C) != x->want) return 0;
    memcpy(x->out, desc, 0x94);
    x->n = n, x->found = 1;
    return 1;
}
// the current room's script first (then its copies' ids are the ones its room load makes), else the first room of this
// stage that places it (the rooms are walked once per id and world visit)
static int EnmGhostHome(unsigned id, int room, int *home, int *n, BYTE *desc)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    const int rooms = stage >= 0 && stage < 13 ? kEnmRoomN[stage] : 0, lim = rooms < ENM_COPY_ROOMS ? rooms : ENM_COPY_ROOMS;
    const DWORD *const *tab = (const DWORD *const *)0x10A0A78;  // the room scripts of this stage (EnmWalkC)
    EnmHomeCtx x;
    int i, r;
    x.want = id, x.found = 0, x.out = desc, x.n = -1;
    if (room > 0 && room < lim) EnmWalk(room, EnmHomeCb, &x);
    if (x.found) {
        *home = room, *n = x.n;
        return 1;
    }
    for (i = 0; i < g_enmHomeN; ++i)
        if (g_enmHome[i].id == id && g_enmHome[i].stage == stage && g_enmHome[i].half == half) {
            if (g_enmHome[i].room <= 0) return 0;
            EnmWalk(g_enmHome[i].room, EnmHomeCb, &x);
            if (!x.found) return 0;
            *home = g_enmHome[i].room, *n = x.n;
            return 1;
        }
    for (r = 1; r < lim; ++r) {
        if (r == room || !tab[r]) continue;
        EnmWalk(r, EnmHomeCb, &x);
        if (x.found) break;
    }
    if (g_enmHomeN < (int)(sizeof g_enmHome / sizeof g_enmHome[0])) {
        EnmHome *h = &g_enmHome[g_enmHomeN++];
        h->id = id, h->stage = stage, h->half = half, h->room = x.found ? r : 0, h->n = x.n;
    }
    if (!x.found) return 0;
    *home = r, *n = x.n;
    return 1;
}
// EnmWrap after the script: the victim originals in the room now are this visit's (EnmMakeCopies makes their copies)
static void EnmLateAtLoad(int room)
{
    const BYTE *t = NULL;
    const int stage = ENM_STAGE;
    int guard = 0, boss = 0;
    EnmListCount(&boss);
    EnmLateReset(&g_enmLate, stage, room, g_enmLateCopies && EnmCopiesLive() && stage >= 2 && stage <= 8 && ENM_MGR_ON() && !boss);
    if (!g_enmLate.on) return;
    while ((t = ENM_NEXT(t)) != NULL && guard++ < 96)
        if (EnmIsGhostId(ENM_ID(t)) && EnmLateAdd(&g_enmLate, ENM_ID(t)) < 0) break;
}
static void EnmLateCopies(const BYTE *w, unsigned id, int type, int cls)
{
    const int stage = ENM_STAGE, room = ENM_ROOM, per = (int)g_enmCopies;
    const unsigned clearOver = *(const unsigned *)0x10A0EAC;
    EnmOrig o;
    int home = 0, k, j, boss = 0, head, made = 0, back = 0, cut = 0, nospot = 0, failed = 0, away = 0, here = 0, how = 0,
        limit = 0, why[ENM_SPOT_N];
    unsigned mw;
    float pp[3];
    char wt[160];
    memset(&o, 0, sizeof o);
    memset(why, 0, sizeof why);
    if (!EnmGhostHome(id, room, &home, &o.n, o.desc)) {
        if (LogQuota(&g_enmQuotaLate, 200))
            LogV("enemies: late copies: victim %08X (type %02X) came into room %d:%d, but no room of this world places its id: "
                 "no copies", id, type, stage, room);
        return;
    }
    mw = *(const unsigned *)(o.desc + 0x24);
    if (EnmTypeOfDesc(o.desc) != type || !EnmCopyEligible3(type, (int)(mw >> 8 & 0xF), mw, *(const unsigned *)(o.desc + 0x2C),
                                                          clearOver, g_enmStoryOn, g_enmCopyDogs)) {
        if (LogQuota(&g_enmQuotaLate, 200))
            LogV("enemies: late copies: victim %08X (type %02X) in room %d:%d: its placement (room %d object %d, mode word %03X) "
                 "is not copied", id, type, stage, room, home, o.n, mw & 0xFFF);
        return;
    }
    EnmListCount(&boss);
    if (boss) return;
    o.type = type, o.variant = (int)(mw >> 8 & 0xF), o.id = id, o.beside = !EnmStanding(cls);
    memcpy(o.pos, w, 12);
    o.bones = EnmModelBones(w);
    head = EnmHeadroom();
    EnmPlayerPos(pp, 0);
    for (k = 1; k <= per && k <= ENM_COPY_MAXK; ++k) {
        int r, queued = 0;
        if (EnmCopyIdOk(home, o.n, k))  // a copy of it that follows you through the door now comes as itself
            for (j = 0; j < g_enmQN; ++j) queued |= g_enmQ[j].copy && !g_enmQ[j].done && g_enmQ[j].id == EnmCopyId(home, o.n, k);
        if (queued) {
            ++here;
            continue;
        }
        r = EnmMakeOne(&o, home, room, k, clearOver, 0, &head, pp, &how, why);
        if (r == ENM_ONE_LIMIT) {
            limit = 1;
            break;
        }
        made += r == ENM_ONE_MADE || r == ENM_ONE_BACK, back += r == ENM_ONE_BACK, cut += r == ENM_ONE_CUT;
        nospot += r == ENM_ONE_NOSPOT, failed += r == ENM_ONE_FAILED, away += r == ENM_ONE_AWAY, here += r == ENM_ONE_EXISTS;
    }
    EnmSpotWhyText(why, wt, sizeof wt);
    if (LogQuota(&g_enmQuotaLate, 200))
        LogV("enemies: late copies: victim %08X (type %02X v%d, placement room %d object %d) came into room %d:%d after the room "
             "load: %d of %d copies made beside it (%d back as they were; not made: %d over the bone budget, %d without a "
             "spot%s%s%s, %d failed, %d here already, %d living in other rooms%s)", id, type, o.variant, home, o.n, stage, room,
             made, per, back, cut, nospot, wt[0] ? " (" : "", wt, wt[0] ? ")" : "", failed, here, away,
             limit ? "; the room's enemy / task limit stopped the rest" : "");
}
// every gameplay tick: one victim original not handled yet this visit, once it is the real ghost (not the placeholder)
static void EnmLateTick(void)
{
    const BYTE *t = NULL;
    int guard = 0;
    if (g_enmLate.stage != ENM_STAGE || g_enmLate.room != ENM_ROOM || !EnmCopiesLive()) return;
    while ((t = ENM_NEXT(t)) != NULL && guard++ < 96) {
        const BYTE *w = EnmWork(t);
        unsigned id, mw;
        int type, variant, cls, a;
        if (!w) continue;
        id = ENM_ID(t);
        if (!EnmIsGhostId(id) || EnmLateHas(&g_enmLate, id)) continue;
        type = *(const int *)(w + 0x20), cls = *(const int *)(w + 0x3C);
        mw = EnmModeWord(t), variant = (int)(mw >> 8 & 0xF);
        if (EnmIsVictim(type, variant) && (!EnmLateReady(cls) || !EnmFinite3((const float *)w))) continue;  // (the placeholder)
        a = EnmLateAdd(&g_enmLate, id);
        if (a < 0) {
            if (LogQuota(&g_enmQuotaAnom, 20))
                LogAnomaly("enemies", "late copies: %d ghost originals already handled in room %d:%d (the set is full): no more "
                           "late copies this visit", ENM_LATE_N, ENM_STAGE, ENM_ROOM);
            g_enmLate.on = 0;
            return;
        }
        if (!EnmIsVictim(type, variant)) continue;  // (the story ghosts: never)
        EnmLateCopies(w, id, type, cls);
        return;  // one original a tick
    }
}

// ---------------------------------------------------------------- v3.30 EN330: Revive Enemies (EnmWrap, before the script)
// A killed monster of a revive type (enemies_sim.h EnmReviveType) whose placement is in this room, or whose body lies in
// this room, stands again: its dead record (state 0x0B) is deleted and its kill bit cleared, so the room's own script
// spawns it fresh (gate 0). Killed copies of this room leave the dead ring. Never in a story room (EnmReviveRoomOk).
typedef struct { unsigned clearOver; int events, bosses, n, num[ENM_ORIG_MAX], kill[ENM_ORIG_MAX], type[ENM_ORIG_MAX],
                 released, relDogs, dog[ENM_ORIG_MAX]; } EnmRevCtx;  // v3.34 EN334: + the released event dogs
static void FiNoteRevive(int stage, int room, unsigned id);  // v3.50 FI350: fadein.inc (a revived id fades in as "revive")
static int EnmRevCb(void *c, int n, const BYTE *desc)
{
    EnmRevCtx *x = (EnmRevCtx *)c;
    const unsigned mw = *(const unsigned *)(desc + 0x24);
    const int mode = (int)(mw & 0xF);
    int type, dog;
    if (*(const int *)desc != 2) return 0;
    type = EnmTypeOfDesc(desc);
    dog = type >= 0 && EnmReviveEventDog(type, mw, *(const unsigned *)(desc + 0x2C), x->clearOver, x->released);  // v3.34 EN334
    if (mode == 6 || mode == 7) ++x->events, x->relDogs += dog;
    if (type == 0x10 || (type == 5 && mode == 8)) ++x->bosses;
    if (type < 0 || !(EnmRevivePlacement(type, mode, *(const unsigned *)(desc + 0x2C), x->clearOver) || dog) || x->n >= ENM_ORIG_MAX)
        return 0;
    x->num[x->n] = n;
    x->kill[x->n] = *(const int *)(desc + 0x30);
    x->type[x->n] = type;
    x->dog[x->n] = dog;
    ++x->n;
    return 0;
}
// v3.30 REV-330-001 (inspection 2): a body lying here of a monster from another room revives only if its HOME placement
// passes the same rules as this room's own (type / mode / id override through EnmRevCb, and its home room not a story room)
// v3.34 EN334: + *dog (a released event dog: its home room makes it awake, EnmAwakeAway*)
static int EnmReviveAwayOk(int stage, int half, int home, unsigned id, int *dog)
{
    static EnmRevCtx y;
    int k;
    *dog = 0;
    if (home <= 0 || home >= 0x100) return 0;
    memset(&y, 0, sizeof y);
    y.clearOver = *(const unsigned *)0x10A0EAC;
    y.released = g_enmReviveDogs && ENM_DOGS_AWAKE;  // v3.34 EN334
    if (EnmWalk(home, EnmRevCb, &y) < 0 || !EnmReviveRoomOk2(stage, y.events, y.relDogs, y.bosses)) return 0;
    for (k = 0; k < y.n; ++k)
        if (EnmScriptId(stage, half, home, y.num[k]) == id) return *dog = y.dog[k], 1;
    return 0;
}
// v3.34 EN334: released event dogs whose body was taken away in another room (their home room makes them awake)
static void EnmAwakeAwayPut(unsigned id)
{
    int i;
    for (i = 0; i < g_enmAwakeAwayN; ++i)
        if (g_enmAwakeAway[i] == id) return;
    if (g_enmAwakeAwayN < (int)(sizeof g_enmAwakeAway / sizeof g_enmAwakeAway[0])) g_enmAwakeAway[g_enmAwakeAwayN++] = id;
}
static int EnmAwakeAwayHas(unsigned id)  // v3.56 ER356
{
    int i;
    for (i = 0; i < g_enmAwakeAwayN; ++i)
        if (g_enmAwakeAway[i] == id) return 1;
    return 0;
}
static int EnmAwakeAwayTake(unsigned id)
{
    int i;
    for (i = 0; i < g_enmAwakeAwayN; ++i)
        if (g_enmAwakeAway[i] == id) {
            g_enmAwakeAway[i] = g_enmAwakeAway[--g_enmAwakeAwayN];
            return 1;
        }
    return 0;
}
// v3.34 EN334: a released event dog of this room that was killed (its body record, its kill bit, or its body taken away
// in another room) stands again awake: the record and the bit go, its placement is kept empty for the room's script (a
// kill bit for the script run, as EnmHideAwayHome) and EnmReviveAwake makes it after the script with its spawn mode
// cleared. One alive, or recorded alive in another room (a wanderer), is left to the game.
static void EnmReviveDog(int stage, unsigned id, int kill, BYTE *rec, int *recs, int *bits, int *dogs)
{
    const int body = rec && EnmBodyRecord(*(const short *)(rec + 0x26), *(const short *)(rec + 0x24), *(const signed char *)(rec + 0x36),
                                          stage);  // v3.59 EN359: not a ghost's pin (was: state 0x0B of this stage)
    const int bit = EnmKillTest(stage, kill), away = EnmAwakeAwayTake(id);
    int w;
    unsigned m;
    if (rec && !body) return;  // alive here or elsewhere: as the game does
    if (!body && !bit && !away) return;  // never killed
    if (body) ENM_RECDEL(rec), ++*recs;
    if (bit) EnmKillPut(stage, kill, 0), ++*bits;
    if (!EnmKillBit(stage, kill, &w, &m) || g_enmAwakeQN >= ENM_ORIG_MAX ||
        g_enmHideN >= (int)(sizeof g_enmHide / sizeof g_enmHide[0])) {
        Log("enemies: revive: event dog %08X has no kill index: the room's script makes it in its own spawn mode", id);
        return;
    }
    ENM_KILLBITS[w] |= m;  // the script leaves its placement empty (gate 3); EnmHideRestore clears it right after
    g_enmHide[g_enmHideN].word = w, g_enmHide[g_enmHideN].mask = m, g_enmHide[g_enmHideN].id = id;
    ++g_enmHideN;
    g_enmAwakeQ[g_enmAwakeQN].id = id, g_enmAwakeQ[g_enmAwakeQN].kill = kill;
    ++g_enmAwakeQN;
    ++*dogs;
}
// v3.34 EN334: EnmWrap, after the room's script (the kill bits are back): the revived event dogs are made awake at their
// placement (gate 0: no record, no kill bit; spawn mode 0: no wait for the flag, no AI 0x11 body pose)
static void EnmReviveAwake(int room)
{
    const int stage = ENM_STAGE;
    int i, made = 0;
    for (i = 0; i < g_enmAwakeQN; ++i) {
        const unsigned id = g_enmAwakeQ[i].id;
        __declspec(align(16)) BYTE d[0x94];
        BYTE *t;
        if (ENM_FIND(id)) continue;
        if (!EnmDescFor(id, d)) {
            Log("enemies: revive: event dog %08X: its placement could not be rebuilt (it comes back in its event mode later)", id);
            continue;
        }
        if (!EnmRoomForOne()) {
            Log("enemies: revive: the room's enemy / task limit: %d released event dog(s) not made now", g_enmAwakeQN - i);
            break;
        }
        *(unsigned *)(d + 0x24) &= ~0xFu;  // awake: spawn mode 0
        t = ENM_CREATE(d, room, id);
        t = t ? ENM_FIND(id) : NULL;
        if (t && EnmWork(t)) {
            ++made;
            FiNoteRevive(stage, room, id);  // v3.50 FI350
            LogV("enemies: revive: event dog %08X (type %02X) stands again awake in room %d:%d at its placement (%.0f %.0f %.0f)",
                 id, *(const int *)(EnmWork(t) + 0x20), stage, room, ((const float *)EnmWork(t))[0], ((const float *)EnmWork(t))[1],
                 ((const float *)EnmWork(t))[2]);
        } else {
            Log("enemies: revive: event dog %08X: the game did not create it (it comes back in its event mode later)", id);
        }
    }
    if (g_enmAwakeQN)
        Log("enemies: revive: room %d:%d: %d of %d released event dog(s) stand again, awake (story flag 130 is set: their "
            "events are over)", stage, room, made, g_enmAwakeQN);
    g_enmAwakeQN = 0;
}
// v3.56 ER356: dogsOnly = By World inside the visit: only this room's released event dogs that the world entry marked
// (g_enmAwakeAway) stand again, awake; nothing else of the room returns (its killed monsters stay dead this visit)
static void EnmReviveRoom(int room, int dogsOnly)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    static EnmRevCtx x;
    BYTE *r;
    int i, recs = 0, bits = 0, away = 0, copies = 0, keptAway = 0, dogs = 0;  // v3.34 EN334: + dogs
    if (stage < 2 || stage > 8 || room <= 0 || room >= 0x100) return;
    memset(&x, 0, sizeof x);
    x.clearOver = *(const unsigned *)0x10A0EAC;
    x.released = g_enmReviveDogs && ENM_DOGS_AWAKE;  // v3.34 EN334
    if (EnmWalk(room, EnmRevCb, &x) < 0) return;
    if (!EnmReviveRoomOk2(stage, x.events, x.relDogs, x.bosses)) {  // v3.34 EN334: released event dogs are no story
        const int bit = (stage & 7) * 64 + (room & 63);
        if (!(g_enmReviveSkipLog[bit >> 5] & (1 << (bit & 31)))) {
            g_enmReviveSkipLog[bit >> 5] |= 1 << (bit & 31);
            Log("enemies: revive: room %d:%d is a story room (%d event spawn(s), %d boss placement(s)): nothing revives there",
                stage, room, x.events, x.bosses);
        }
        return;
    }
    for (i = 0; i < x.n; ++i) {
        const unsigned id = EnmScriptId(stage, half, room, x.num[i]);
        BYTE *rec;
        if (dogsOnly && (!x.dog[i] || !EnmAwakeAwayHas(id))) continue;  // v3.56 ER356
        rec = ENM_RECFIND(id);
        if (x.dog[i]) {  // v3.34 EN334: replayed awake after the script
            EnmReviveDog(stage, id, x.kill[i], rec, &recs, &bits, &dogs);
            continue;
        }
        if (rec && EnmBodyRecord(*(const short *)(rec + 0x26), *(const short *)(rec + 0x24), *(const signed char *)(rec + 0x36), stage)) {
            ENM_RECDEL(rec);  // v3.59 EN359: a ghost's pin (a victim drawn on this placement) is no body: it stays with its Sword
            ++recs;
            FiNoteRevive(stage, room, id);  // v3.50 FI350
        }
        if (EnmKillTest(stage, x.kill[i])) {
            EnmKillPut(stage, x.kill[i], 0);
            ++bits;
            FiNoteRevive(stage, room, id);  // v3.50 FI350
        }
    }
    if (dogsOnly) {  // v3.56 ER356: (By World: the bodies lying here and the killed copies wait for the next world entry)
        if (dogs)
            LogV("enemies: return: room %d:%d (By World): %d released event dog(s) marked at the world entry are made awake after "
                 "the room's script", stage, room, dogs);
        return;
    }
    // bodies lying here of monsters from other rooms (a follower killed here): its home room spawns it again
    for (i = 0, r = ENM_RECORDS; i < 64; ++i, r += 0x50) {
        const unsigned id = *(const unsigned *)(r + 0x20);
        int dog;  // v3.34 EN334
        if (*(const short *)(r + 0x26) != 0x0B || *(const signed char *)(r + 0x36) != stage ||
            *(const signed char *)(r + 0x37) != room || !EnmReviveType(*(const short *)(r + 0x24)) || !EnmIsScriptId(id) ||
            EnmScriptStage(id) != stage || EnmScriptHalf(id) != half || EnmScriptRoom(id) == room)
            continue;
        if (!EnmReviveAwayOk(stage, half, EnmScriptRoom(id), id, &dog)) {  // v3.30 REV-330-001: e.g. an event dog not released
            ++keptAway;
            continue;
        }
        if (*(const short *)(r + 0x30) > 0) EnmKillPut(stage, *(const short *)(r + 0x30), 0);
        ENM_RECDEL(r);
        ++away;
        if (dog) EnmAwakeAwayPut(id);  // v3.34 EN334: its home room makes it awake
    }
    copies = EnmRingRemoveRoom(&g_enmDead, stage, half, room);
    if (keptAway)
        Log("enemies: revive: room %d:%d: %d bod%s of other rooms' monsters stay (their home placement is a story / event "
            "spawn or not revivable)", stage, room, keptAway, keptAway == 1 ? "y" : "ies");
    if (dogs)  // v3.34 EN334
        LogV("enemies: revive: room %d:%d: %d released event dog(s) (story flag 130 set) are made awake after the room's "
             "script", stage, room, dogs);
    if (recs + bits + away + copies)
        Log("enemies: revive: room %d:%d: killed monsters stand again (%d bod%s taken away, %d kill bit(s) cleared, %d bod%s "
            "of other rooms' monsters: they stand at home), %d killed cop%s come back", stage, room, recs,
            recs == 1 ? "y" : "ies", bits, away, away == 1 ? "y" : "ies", copies, copies == 1 ? "y" : "ies");
}

// ---------------------------------------------------------------- v3.56 ER356: Enemies Return By Room / By World
// The rule is enmreturn_sim.h (ErAction by worldvisit_sim.h's kind of this room load). By Room = EnmReviveRoom at every
// room change (a load's room load is none: v3.55 revived there). By World = at a world entry every killed monster of
// the revive types of the world stands again (ErWorldPass: every room of the stage that is no story room, then
// EnmReviveRoom for the entry room); inside the visit the game's own bodies keep them dead (Tremers included: the
// 0x48ACE5 site below), the saves hold them as they are, a load is no entry.
static volatile LONG g_erQuota;
static int g_erTremerIni = 1;          // [Gameplay] ReturnTremers (ini only): 0 = the Tremer site stays stock
static int g_erTremerSite;             // 0x48ACE5 is in
static volatile LONG g_erTremerKept;   // dead Tremers whose room exit kept their body under By World (this session)
static LONG g_erTremerSaid;
static int g_erLastMode = -1;          // the mode of the last room load (a change is logged once)
// By World at a world entry: every room of this stage but the entry room (EnmReviveRoom takes that one after this)
static void ErWorldPass(int entryRoom)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    const int rooms = stage >= 0 && stage < 13 ? kEnmRoomN[stage] : 0;
    const DWORD *const *tab = (const DWORD *const *)0x10A0A78;  // the room scripts of this stage (EnmWalkC)
    static EnmRevCtx x;
    int room, i, walked = 0, story = 0, recs = 0, bits = 0, dogs = 0, alive = 0, back = 0;
    if (stage < 2 || stage > 8) return;
    for (room = 1; room < rooms && room < 0x100; ++room) {
        if (!tab[room]) continue;
        memset(&x, 0, sizeof x);
        x.clearOver = *(const unsigned *)0x10A0EAC;
        x.released = g_enmReviveDogs && ENM_DOGS_AWAKE;
        if (EnmWalk(room, EnmRevCb, &x) < 0) continue;
        if (!EnmReviveRoomOk2(stage, x.events, x.relDogs, x.bosses)) {
            ++story;
            continue;
        }
        ++walked;
        for (i = 0; i < x.n; ++i) {
            const unsigned id = EnmScriptId(stage, half, room, x.num[i]);
            BYTE *rec = ENM_RECFIND(id);
            const int body = rec && EnmBodyRecord(*(const short *)(rec + 0x26), *(const short *)(rec + 0x24),
                                                  *(const signed char *)(rec + 0x36), stage);  // v3.59 EN359: not a ghost's pin
            if (rec && !body) {  // recorded alive (a story move without the hole: a wanderer, a downed one): the game's
                ++alive;
                continue;
            }
            if (x.dog[i]) {  // a released event dog: its own room makes it awake at its first load of this visit
                EnmAwakeAwayPut(id);
                ++dogs;
                continue;
            }
            if (room == entryRoom) continue;  // (EnmReviveRoom: its notes for Fade In too)
            {
                const int bit = EnmKillTest(stage, x.kill[i]);
                if (body) ENM_RECDEL(rec), ++recs;
                if (bit) EnmKillPut(stage, x.kill[i], 0), ++bits;
                back += body || bit;
            }
        }
    }
    if (LogQuota(&g_erQuota, 300))
        Log("enemies: return: entered world %d (By World): %d room(s) walked, %d killed monster(s) stand again (%d bod%s taken "
            "away, %d kill bit(s) cleared), %d released event dog(s) awake at their room's next load, %d recorded alive left "
            "as they are, %d story room(s) left as they are; the entry room %d next", stage, walked, back, recs,
            recs == 1 ? "y" : "ies", bits, dogs, alive, story, entryRoom);
}
// EnmWrap, before the room's script (after the door pass's set-up: g_enmHideN / g_enmAwakeQN are 0)
static void ErAtRoomLoad(int room, int kind)
{
    const int mode = EnmReturnMode(), act = ErAction(mode, kind), stage = ENM_STAGE;
    const LONG kept = g_erTremerKept;
    if (mode != g_erLastMode) {
        if (g_erLastMode >= 0 && LogQuota(&g_erQuota, 300))
            Log("enemies: return: Enemies Return %s from this room load on (%s)", kErName[mode],
                mode == ER_ROOM ? "killed monsters stand again each time you enter their room" :
                mode == ER_WORLD ? "killed monsters stand again when you enter the world again; a load is no entry" :
                "the game's own rule");
        g_erLastMode = mode;
    }
    if (kept != g_erTremerSaid && room > 0) {
        if (LogQuota(&g_erQuota, 300))
            LogV("enemies: return: %ld killed Tremer(s) kept their body at a room exit this session (By World: dead until you "
                 "enter the world again)", kept);
        g_erTremerSaid = kept;
    }
    if (act == ERA_ROOM) {
        EnmReviveRoom(room, 0);
    } else if (act == ERA_WORLD) {
        ErWorldPass(room);
        EnmReviveRoom(room, 0);
    } else if (act == ERA_DOGS) {
        if (g_enmAwakeAwayN) EnmReviveRoom(room, 1);
    } else if (kind == WV_LOAD && room > 0 && mode != ER_OFF && stage >= 2 && stage <= 8 && LogQuota(&g_erQuota, 300)) {
        Log("enemies: return: room %d:%d after a load: nothing returns (%s: a load is no room change and no world entry; the "
            "save's dead stay dead)", stage, room, kErName[mode]);
    }
}
// 0x48ACE5: the first `call 4215D0` (MathRand01) in TremerDestroy 0x48ACB0, reached only for a dead Tremer (state 0x0B)
// at its room exit: the stock coin flip (< 0.5 keeps its body record, else 1 in 5 sets its kill bit, else nothing: it
// stands again at the room's next visit). By World answers 0.0: the body is always kept (TremerSaveRecord 0x4889F0, the
// game's own record), so it stays dead through this world visit and its saves; any other value: the stock call.
static int __cdecl ErTremerKeepNow(void)
{
    if (!g_erTremerSite || !ErTremerKeep(EnmReturnMode(), 1, ENM_STAGE)) return 0;
    InterlockedIncrement(&g_erTremerKept);
    return 1;
}
static const DWORD g_erRandFn = 0x4215D0;
static __declspec(naked) void hkErTremer(void)
{
    __asm {
        call ErTremerKeepNow
        test eax, eax
        jz stock
        fldz                         // MathRand01's result: 0.0 (< 0.5: TremerSaveRecord)
        ret
    stock:
        jmp dword ptr [g_erRandFn]   // MathRand01 (returns to 0x48ACEA)
    }
}

// ---------------------------------------------------------------- the coexistence probes (once per session)
static void EnmProbeTable(void)
{
    static int s_done;
    if (s_done++) return;
    Log("enemies: the cheat table's Randomizer %s, Spawn Enemies %s, Victims %s, Bone Pool Extender %s",
        memcmp((const void *)0x52CAA9, "\xE8\x52\xFE\xFF\xFF", 5) ? "is on (stacking: copies come after its extras, "
                                                                     "capped by the bone budget)" : "off",
        memcmp((const void *)0x462910, "\xE8\x2B\x16\x00\x00", 5) ? "may be on" : "off",        // call 463F40
        memcmp((const void *)0x464880, "\x55\x8B\xEC\x83\xEC\x10", 6) ? "may be on" : "off",  // 0x464880's entry
        memcmp((const void *)0x414A10, "\x68\x80\x05\x00\x00", 5) ? "is on (the larger pool is kept)" : "off");
}

// v3.31 EN331-003 (inspection 4): every save load (the game's load menu and Refined's quick load set [0xFD5A74] = 1 until
// the world start has applied the image, quicksave.inc) drops the ghost-copy store: a snapshot belongs to the timeline
// the load abandoned, even in the same world. Watched on every tick (rising edge) and at each room load (flag still up).
static int g_enmLoadSeen;
static void EnmLoadWatch(const char *where)
{
    const int loading = *(volatile int *)0xFD5A74 != 0;
    if (loading && !g_enmLoadSeen && g_enmSnapAny) {
        EnmSnapClear(&g_enmSnap), g_enmSnapAny = 0;
        if (!g_enmLogged[11]++) Log("enemies: a save load (%s): the ghost copies' kept states are dropped (that timeline is gone)", where);
    }
    if (loading && !g_enmLoadSeen) {  // v3.34 EN334: the copies that followed you go home too, and the dogs' away marks
        const int n = EnmCwCount(&g_enmCw);
        EnmCwClear(&g_enmCw), g_enmAwakeAwayN = 0;
        if (n) LogV("enemies: a save load (%s): %d cop%s that followed you are made in their own rooms again", where, n,
                    n == 1 ? "y" : "ies");
        EnmPursueDrop("a save load");  // v3.35 GH335
        EnmTagRoomSay("a save load"), g_enmTagRoom = -1;  // v3.58 EN358
        g_enmQN = g_enmGhostTagN = 0;  // v3.38 EN338: the followers on their way belong to the timeline the load abandoned
        g_enmCdRoomKey = 0xFFFFFFFFu;  // (the countdown watch starts afresh)
        g_enmArrivedN = 0;  // v3.50 EF350: and the arrivals watched there (v3.49: "came into room 2 and is gone 4.2 s later")
        EfTrackReset(&g_efTrack, 0);  // v3.50 EF350
        EpRoomEnd("a save load");      // v3.51 EP351: and the in-room give-ups' timers
    }
    g_enmLoadSeen = loading;
}
// ---------------------------------------------------------------- the hooks
// 0x4F0AF8: was `call 52CA00` (SceneRoomScriptRun) in RoomInitLoad, after the arrival position (0x4F0620)
static void __cdecl EnmWrap(int room)
{
    int wv = WV_FIRST;  // v3.56 MERGE (RI356 + ER356): the world-visit rule first, ONE call per room load (worldvisit.inc: every
    __try {             // reader of the visit comes after it); its kind for Enemies Return, read before 0x52CA00 resets its source
        wv = WvOnRoomLoad(room);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        wv = WV_FIRST;
    }
    if (g_enmInstalled) {
        __try {
            const int stage = ENM_STAGE;
            int n;
            EnmLoadWatch("room load");  // v3.31 EN331-003: before the purge and the copies
            n = EnmPurgeCopyRecords();
            if (n && !g_enmLogged[6]++) Log("enemies: %d record(s) of copies deleted at a room load (a save or an exit)", n);
            if (!g_enmFail) {
                if (stage != g_enmLastStage) {  // left the world (or a new one): killed copies come back next time
                    EnmRingClear(&g_enmDead);
                    EnmCwClear(&g_enmCw), g_enmAwakeAwayN = 0;  // v3.34 EN334: and the copies that followed you go home
                    if (g_enmSnapAny) EnmSnapClear(&g_enmSnap), g_enmSnapAny = 0;  // v3.31 EN331: and ghost copies start afresh
                    EnmPursueDrop("you left the world");  // v3.35 GH335
                    g_enmHomeN = 0;                       // v3.35 GH335: the victims' placements of another world
                    g_enmLastStage = stage;
                }
                if (!EnmFollowTakesMonsters(g_enmFollow)) EnmMonstersHomeAtLoad();  // v3.49 EF349: Off or Victims Only (was == 0, inline)
                if (stage >= 2 && stage <= 8) EnmWheelchairHome();  // v3.38 EN338: v3.37's strays go home (any setting)
                if (g_enmCopies > 0 || g_enmFollow > 0 || g_enmRevive > 0) EnmProbeTable();
                g_enmExtraN = 0, g_enmExtraStage = stage, g_enmExtraRoom = room;  // v3.33 EN333: every room load parks the last room's extra files
                g_enmHideN = 0, g_enmAwakeQN = 0;  // v3.34 EN334: (was in EnmHideAwayHome) Revive's event dogs and the followers' homes
                if (wv == WV_LOAD) g_enmAwakeAwayN = 0;  // v3.56 ER356: the dogs' marks belong to the timeline the load left
                ErAtRoomLoad(room, wv);  // v3.30 EN330 (before the hide: a revived follower is home)  // v3.56 ER356: by the mode and this room load's kind (was: By Room / On at every room load)
                if (EnmFollowLive()) EnmHideAwayHome(room);  // v3.30 EN330
                EnmDoorPass(room);
                EfRestoreAtLoad(room);  // v3.50 EF350: after a load, the save's followers (pursuits, the queue, tags, copies)
            }
        } __except (EnmFaultNote(GetExceptionInformation())) {
            EnmFailNow("the room load (before the script)");
        }
    }
    {   // v3.48 RZ348: the Randomizer writes this room's pickup words (randomizer.inc) and puts them back right after the
        // room's build; v3.48 fix1 RZ-002: in a __finally (also when a fault in the runner is handled further out), this
        // run's own word list (a nested run keeps the outer one's)
        const int rzLevel = RzBeforeScript(room);
        __try {
            ((void(__cdecl *)(int))0x52CA00)(room);
        } __finally {
            RzAfterScript(rzLevel);
        }
    }
    if (g_enmHideN) {  // v3.30 EN330: the temporary kill bits go back at once (a follower may come home later)
        __try {
            EnmHideRestore();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_enmHideN = 0;
        }
    }
    if (!g_enmInstalled || g_enmFail) {
        g_enmAwakeQN = 0;  // v3.34 EN334
        return;
    }
    __try {
        int boss = 0;
        if (g_enmAwakeQN) {  // v3.34 EN334: the revived event dogs first (they are originals of this room's copies)
            if (ENM_MGR_ON()) EnmReviveAwake(room);
            g_enmAwakeQN = 0;
        }
        if (EnmFollowLive()) {  // the real enemies first (wanderers are not in this room's script: they get no copies)
            EnmListCount(&boss);
            if (boss) {  // v3.38 EN338: they wait (pursuers: a later door), never into a boss room
                if (g_enmQN + g_enmGhostTagN)
                    Log("enemies: room %d:%d holds a boss: %d follower(s) wait for a later door", ENM_STAGE, room, g_enmQN + g_enmGhostTagN);
                g_enmQN = g_enmGhostTagN = 0;
            } else if (ENM_MGR_ON() && ENM_STAGE >= 2 && ENM_STAGE <= 8) {
                EnmReplayWanderers(room);
                if (g_enmFollowCopies && EnmCopiesLive()) EnmReplayCopyWanderers(room);  // v3.34 EN334
            }
        }
        EnmLateAtLoad(room);  // v3.35 GH335: the victim originals here now (their copies are made below, not again)
        if (EnmCopiesLive()) EnmMakeCopies(room);
    } __except (EnmFaultNote(GetExceptionInformation())) {
        EnmFailNow("the room load (after the script)");
    }
}
// 0x450DB0: EnemyKillCredit's entry (`mov eax,[esp+4]; push eax`): a copy's death never reaches KillCountIncrement
static LONG g_enmKillsKept;
static int __cdecl EnmIsCopyTask(const BYTE *task)
{
    __try {
        if (task && EnmIsCopyId(ENM_ID(task))) {
            if (InterlockedIncrement(&g_enmKillsKept) == 1)
                Log("enemies: a copy was killed: the ending's kill count stays as it is");
            return 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return 0;
}
static const DWORD g_enmKillBack = 0x450DB5;
static __declspec(naked) void hkEmKill(void)
{
    __asm {
        push dword ptr [esp + 4]
        call EnmIsCopyTask
        add esp, 4
        test eax, eax
        jnz skip
        mov eax, [esp + 4]           // the two replaced instructions
        push eax
        jmp dword ptr [g_enmKillBack]
    skip:
        ret
    }
}
// 0x419830: SgAnimeWorkBoneGetSize (`mov eax,[esp+4]; imul eax,eax,70h; ret`); only the world reset's call (return
// 0x414A1A) is enlarged, the boot call (0x414E18) stays stock. Also a world reset: the session state starts afresh.
static int __cdecl EnmPoolAtReset(int count)
{
    int n = count;
    __try {
        const int on = !g_enmFail && (g_enmCopies > 0 || g_enmFollow > 0);
        n = EnmPoolCount(count, g_enmPoolSetting, on);
        EnmRingClear(&g_enmDead);
        EnmSnapClear(&g_enmSnap), g_enmSnapAny = 0;  // v3.31 EN331
        EnmCwClear(&g_enmCw), g_enmAwakeAwayN = 0;   // v3.34 EN334: New Game / title load: the copies' ledger starts empty
        g_enmLastStage = -1;
        WvWorldReset();  // v3.56 RI356: the world visit in force ends (worldvisit.inc)
        g_enmPurN = g_enmQN = g_enmGhostTagN = 0;
        EnmPursueClear(&g_enmPursue), g_enmHomeN = 0, g_enmLate.on = 0;  // v3.35 GH335
        EnmSeenReset(&g_enmSeen, 0);                                      // v3.38 EN338
        DropWorldReset();  // v3.30 AS330: Supply Drops' roll seed is re-rolled (drops.inc; New Game / load)
        if (n != g_enmPoolNow || n != count) Log("enemies: world reset: bone pool %d records (asked %d)", n, count);
        g_enmPoolNow = n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        n = count;
    }
    return n;
}
static __declspec(naked) void hkEmPool(void)
{
    __asm {
        mov eax, [esp + 4]
        cmp dword ptr [esp], 414A1Ah
        jne stock
        push eax
        call EnmPoolAtReset
        add esp, 4
    stock:
        imul eax, eax, 70h
        ret
    }
}

// v3.31 EN331: 0x465438, the `call 4638E0` in EnemyManagerSave 0x465410 (the save writes a record for every enemy of the
// list through the type's table 0x615128, copies the 0x1B90 bytes of the manager into the save and deletes those records
// again): a copy writes none, so no copy is ever in a save (v3.30 purged them at the load: a save read without Refined
// had them; a victim copy's state-0x0F record could then arrive as a follower through 0x486150).
static void __cdecl EnmSaveRecord(BYTE *task, int type)
{
    __try {
        if (task && EnmIsCopyId(ENM_ID(task))) {
            if (!g_enmLogged[10]++) Log("enemies: a save: the copies are left out of it");
            return;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    ((void(__cdecl *)(BYTE *, int))0x4638E0)(task, type);
}

// v3.30 EN330: 0x4F0A5A, the `call 4198B0` (SgAnimeWorkBoneReset) in RoomLoadSetMark 0x4F0A50, before gGame.room =
// nextRoom: the stock reset releases every bone (nothing holds one across a room change), then a pool smaller than Refined
// wants gets our buffer through the game's own SgAnimeWorkBoneSetBuffer 0x419840 (it clears it, aligns it and sets the
// counts at 0xED73E4 / 0xED73EC / 0xED73F0). The old buffer stays allocated (the world arena) until the next world reset
// gives a new one, so a stale pointer still reads valid memory. A save loaded in the game never passes the world reset
// 0x414A00 (the user's log 17:43): before this, Copies switched on after a load kept the stock 1408 records.
static void __cdecl EnmBoneReset(void)
{
    ((void(__cdecl *)(void))0x4198B0)();
    if (!g_enmInstalled || g_enmFail) return;
    __try {
        const int num = *(volatile const int *)0xED73EC;
        const int want = EnmPoolGrowAt(num, g_enmPoolSetting, g_enmCopies > 0 || g_enmFollow > 0);
        if (want > 0) {
            if (g_enmPoolBufN < want) {  // a new one (the old one may be the pool still: never freed)
                BYTE *b = (BYTE *)VirtualAlloc(NULL, (SIZE_T)want * 0x70, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (!b) {
                    if (!g_enmLogged[7]++)
                        Log("enemies: room change: no memory for a bone pool of %d records (error %lu): it stays %d", want,
                            GetLastError(), num);
                    return;
                }
                g_enmPoolBuf = b;
                g_enmPoolBufN = want;
            }
            ((void(__cdecl *)(void *, unsigned))0x419840)(g_enmPoolBuf, (unsigned)want * 0x70);
            g_enmPoolNow = *(volatile const int *)0xED73EC;
            Log("enemies: room change: bone pool %d -> %d records (Enemy Copies / Enemies Follow are on; the game's next "
                "world reset gives its own pool again)", num, g_enmPoolNow);
        }
    } __except (EnmFaultNote(GetExceptionInformation())) {
        EnmFailNow("the bone pool at a room change");
    }
}

// v3.49 EF349: what Enemies Follow takes, one line per change of the value (the first tick says the start value)
static int g_enmFollowSaid = -1;
static volatile LONG g_enmQuotaMode;
static void EnmFollowSay(void)
{
    const int m = g_enmFollow;
    if (m == g_enmFollowSaid) return;
    g_enmFollowSaid = m;
    if (!LogQuota(&g_enmQuotaMode, 100)) return;
    if (m < 0 || m >= ENM_FOLLOW_N) {
        LogAnomaly("enemies", "Enemies Follow has the value %d (0..%d): nobody follows", m, ENM_FOLLOW_N - 1);
        return;
    }
    Log("enemies: Enemies Follow %s: %s%s%s%s", kGpName[GP_FOLLOW][m],
        m == ENM_FOLLOW_OFF ? "nobody follows you through doors" :
        m == ENM_FOLLOW_MONSTERS ? "monsters and their copies follow you through doors; the ghosts do not" :
        m == ENM_FOLLOW_VICTIMS ? "only the victims' ghosts follow you through doors" : "monsters and the victims' ghosts follow you through doors",
        EnmFollowTakesVictims(m) ? (g_enmStoryFollow ? " (the story ghosts too" : " (not the story ghosts") : "",
        EnmFollowTakesVictims(m) ? (g_enmFollowCopies ? ", and their copies)" : "; no copies)") : "",
        m == ENM_FOLLOW_VICTIMS ? (g_enmGhostOn ? "; monsters and their copies stay in their rooms (any left in other rooms go home at "
                                                  "the next room load)"
                                                : "; but 00486199 is not in: no ghost can follow, so nobody does")
                                : EnmFollowTakesVictims(m) && !g_enmGhostOn ? "; 00486199 is not in: the ghosts cannot follow" : "");
}
// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread)
static void EnmTickC(void)
{
    float pp[3];
    const int stage = ENM_STAGE;
    {  // v3.31 EN331: the clock the ghost records age by (before the purge: a record written this tick keeps its time)
        const float now = ENM_ROOM_TIMER;
        if (_finite(now)) {
            g_enmClock += EnmClockStep(g_enmClockPrev, now);
            g_enmClockPrev = now;
        }
    }
    EnmLoadWatch("tick");  // v3.31 EN331-003
    EnmPurgeCopyRecords();
    if (g_enmFail) return;
    if (g_enmCopies > 0) EnmDeadScan();
    else {
        if (g_enmSnapAny) EnmSnapClear(&g_enmSnap), g_enmSnapAny = 0;  // v3.31 EN331: Copies Off: ghost copies start afresh
        if (g_enmCw.age) EnmCwClear(&g_enmCw);                           // v3.34 EN334: and the copies that followed you
    }
    {   // v3.35 GH335: the down countdowns and the late copies (gameplay ticks; any Follow setting)
        const int play = *(volatile int *)0xFD5C20 == 0 && *(volatile int *)0xECB1B4 == 0 && *(void *volatile *)0xFD5BA0 &&
                         *(volatile int *)0xFD5A74 == 0 && stage >= 2 && stage <= 8 && ENM_MGR_ON();
        ++g_enmCdTick;  // (every dispatcher tick: the countdown watch compares consecutive ticks only)
        if (play && g_enmCdKeepOn) EnmCdWatch();
        if (play && g_enmLate.on) EnmLateTick();
    }
    {   // v3.51 EP351: Enemy Pursuit inside the room (normal play, any Enemies Follow value)
        if (*(volatile int *)0xFD5C20 == 0 && *(volatile int *)0xECB1B4 == 0 && *(void *volatile *)0xFD5BA0 &&
            *(volatile int *)0xFD5A74 == 0 && stage >= 2 && stage <= 8 && ENM_MGR_ON()) {
            EnmPlayerPos(pp, 0);
            if (EnmFinite3(pp)) EpTick(pp);
        }
    }
    EnmFollowSay();  // v3.49 EF349: each change of the value, once
    if (!EnmFollowLive()) EnmPursueDrop("Enemies Follow is off");  // v3.35 GH335
    else {  // v3.49 EF349: by the value (was: != 2 -> the ghosts' pursuits end)
        const int mode = g_enmFollow;
        if (!EnmFollowTakesVictims(mode)) EnmPursueDropGhosts();     // v3.38 EN338: Monsters: the ghosts' pursuits end
        if (!EnmFollowTakesMonsters(mode)) EnmPursueDropMonsters();  // v3.49 EF349: Victims Only: the monsters' pursuits end
    }
    if (!EnmFollowLive()) {
        if (g_enmQN) {  // v3.38 REV338 R3: the queued copies not in yet go home before the queue is dropped
            const int n = EnmQueueHome();
            if (n && LogQuota(&g_enmQuotaPursue, 300))
                LogV("enemies: Enemies Follow is off: %d copy follower(s) on their way are made in their own rooms again", n);
        }
        g_enmPurN = g_enmQN = g_enmGhostTagN = 0;
        return;
    }
    if (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0 || !*(void *volatile *)0xFD5BA0) return;
    if (stage < 2 || stage > 8 || !ENM_MGR_ON()) return;
    EnmPlayerPos(pp, 0);
    if (!EnmFinite3(pp)) return;
    if (!g_enmDoorSet && g_enmQStage == stage && g_enmQRoom == ENM_ROOM) {
        memcpy(g_enmDoor, pp, sizeof g_enmDoor);
        g_enmDoorSet = 1;
    }
    EnmTakePursuers(pp);
    if (g_enmQN && g_enmDoorSet) EnmFollowArrivals(pp);
    if (g_enmArrivedN) EnmWatchArrived();  // v3.33 EN333
}
static void EnmTick(void)  // quicksave.inc QsTick
{
    if (!g_enmInstalled) return;
    __try {
        EnmTickC();
    } __except (EnmFaultNote(GetExceptionInformation())) {
        EnmFailNow("the tick");
    }
}

// ---------------------------------------------------------------- settings (gameplay.inc GpApply, any thread)
static void EnmSettingChanged(int k, int v)
{
    if (k == GP_COPIES) InterlockedExchange(&g_enmCopies, v);
    else if (k == GP_FOLLOW) InterlockedExchange(&g_enmFollow, v);
    else if (k == GP_REVIVE) InterlockedExchange(&g_enmRevive, v);  // v3.30 EN330
}
static void EnmReadIni(void)  // the ini-only keys (not written into SH4Refined.ini; documented in the PLAN)
{
    char b[32];
    double f;
    int v;
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Gameplay", "BonePool", "Auto", b, sizeof b, g_iniPath);
    g_enmPoolSetting = !_stricmp(b, "Auto") || !b[0] ? ENM_POOL_AUTO : atoi(b) <= 0 ? 0 : atoi(b);
    v = (int)GetPrivateProfileIntA("Gameplay", "FollowMax", ENM_FOLLOW_DEFAULT, g_iniPath);  // v3.34 EN334: 6 (was 3), 1..8; v3.38 EN338: 32, 1..32
    g_enmFollowMax = EnmFollowMaxIni(v);
    GetPrivateProfileStringA("Gameplay", "FollowDelay", "3", b, sizeof b, g_iniPath);
    f = atof(b);
    g_enmFollowDelay = (float)(f < 0.0 ? 0.0 : f > 30.0 ? 30.0 : f);
    GetPrivateProfileStringA("Gameplay", "FollowRange", "800", b, sizeof b, g_iniPath);
    f = atof(b);
    g_enmFollowRange = (float)(f < 200.0 ? 200.0 : f > 3000.0 ? 3000.0 : f);
    g_enmStoryOn = GetPrivateProfileIntA("Gameplay", "CopyStoryGhosts", 1, g_iniPath) != 0;  // v3.31 EN331
    g_enmCopyDogs = GetPrivateProfileIntA("Gameplay", "CopyEventDogs", 1, g_iniPath) != 0;     // v3.33 EN333
    g_enmLoadFiles = GetPrivateProfileIntA("Gameplay", "FollowLoadFiles", 1, g_iniPath) != 0;  // v3.33 EN333
    g_enmReviveDogs = GetPrivateProfileIntA("Gameplay", "ReviveEventDogs", 1, g_iniPath) != 0;  // v3.34 EN334
    g_enmFollowCopies = GetPrivateProfileIntA("Gameplay", "FollowCopies", 1, g_iniPath) != 0;   // v3.34 EN334
    GetPrivateProfileStringA("Gameplay", "FollowRangeVictims", "1500", b, sizeof b, g_iniPath);  // v3.35 GH335
    g_enmVictimRange = EnmVictimRangeIni(atof(b));
    // v3.38 EN338: FollowKeepDoors missing / "Always" / negative = the whole world visit (was 3); 0 = no pursuit; 1..32 doors
    GetPrivateProfileStringA("Gameplay", "FollowKeepDoors", "Always", b, sizeof b, g_iniPath);
    g_enmKeepDoors = !b[0] || !_stricmp(b, "Always") ? ENM_KEEP_ALWAYS : EnmKeepDoorsIni2(atoi(b));
    g_enmLateCopies = GetPrivateProfileIntA("Gameplay", "LateCopies", 1, g_iniPath) != 0;
    g_enmCdKeepOn = GetPrivateProfileIntA("Gameplay", "DownKeep", ENM_DOWNKEEP_DEFAULT, g_iniPath) != 0;  // v3.38 EN338: default 0 (vanilla)
    g_enmStoryFollow = GetPrivateProfileIntA("Gameplay", "FollowStoryGhosts", 1, g_iniPath) != 0;       // v3.38 EN338 (hidden)
    g_enmTaskAllIni = GetPrivateProfileIntA("Gameplay", "CopiesAllTasks", 1, g_iniPath) != 0;  // v3.36.1 TL3361 (hidden)
    g_erTremerIni = GetPrivateProfileIntA("Gameplay", "ReturnTremers", 1, g_iniPath) != 0;      // v3.56 ER356 (hidden)
    v = (int)GetPrivateProfileIntA("Gameplay", "CopyRecordGuard", RM_CRG_WORLD, g_iniPath);     // v3.58 RZ358 (hidden): 0 / 1 / 2
    g_enmCopyRecGuard = v >= RM_CRG_OFF && v < RM_CRG_N ? v : RM_CRG_WORLD;
    g_enmPinGateIni = GetPrivateProfileIntA("Gameplay", "PinnedGhostsStay", 1, g_iniPath) != 0;  // v3.59 EN359 (hidden): 0 = stock gate
}

// ---------------------------------------------------------------- install (Init, after DeathPresenceInstall)
static void EnemiesInstall(void)
{
    static const Site kCore[3] = {
        { 0x4F0AF1, { 0x8B, 0x0D, 0x94, 0x5A, 0xFD, 0x00, 0x51, 0xE8, 0x03, 0xBF, 0x03, 0x00 }, 12 },  // push [FD5A94]; call 52CA00
        { 0x450DB0, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0xE6, 0xA9, 0x00, 0x00 }, 10 },              // EnemyKillCredit
        { 0x419830, { 0x8B, 0x44, 0x24, 0x04, 0x6B, 0xC0, 0x70, 0xC3 }, 8 },                           // SgAnimeWorkBoneGetSize
    };
    static const Site kGhost = { 0x486192, { 0xE8, 0x99, 0x93, 0xFE, 0xFF, 0x50, 0x56, 0xE8, 0xE2, 0xE6, 0xFD, 0xFF, 0x83,
                                             0xC4, 0x08 }, 15 };  // call 46F530; push eax; push esi; call 464880; add esp,8
    // v3.30 EN330: RoomLoadSetMark 0x4F0A55: call 561420; call 4198B0; call 419AB0
    static const Site kBone = { 0x4F0A55, { 0xE8, 0xC6, 0x09, 0x07, 0x00, 0xE8, 0x51, 0x8E, 0xF2, 0xFF, 0xE8, 0x4C, 0x90, 0xF2,
                                            0xFF }, 15 };
    // v3.31 EN331: EnemyManagerSave 0x465430: push esi; call 45B710; push eax; push esi; call 4638E0
    static const Site kSave = { 0x465430, { 0x56, 0xE8, 0xDA, 0x62, 0xFF, 0xFF, 0x50, 0x56, 0xE8, 0xA3, 0xE4, 0xFF, 0xFF }, 13 };
    GpPatch set[3];
    int i;
    EnmReadIni();
    EnmRingClear(&g_enmDead);
    EnmCwClear(&g_enmCw);  // v3.34 EN334
    EnmPursueClear(&g_enmPursue);  // v3.35 GH335
    if (!g_qsTickOn) {
        Log("enemies: no 0x4EF982 tick dispatcher: Enemy Copies and Enemies Follow are off");
        return;
    }
    for (i = 0; i < 3; ++i)
        if (!SiteOk(&kCore[i])) {
            Log("enemies: unexpected bytes at %08lX (another mod?): Enemy Copies and Enemies Follow are off", kCore[i].va);
            return;
        }
    GpBranch(&set[0], 0x4F0AF8, 0xE8, EnmWrap, 5);
    GpBranch(&set[1], 0x450DB0, 0xE9, hkEmKill, 5);
    GpBranch(&set[2], 0x419830, 0xE9, hkEmPool, 8);
    if (!GpApplyAll(set, 3, "enemies")) return;
    g_enmInstalled = 1;
    g_enmCopiesReady = g_enmFollowReady = 1;
    g_enmReviveReady = 1;  // v3.30 EN330: Revive Enemies needs only the room load hook
    if (SiteOk(&kGhost)) {
        GpBranch(&set[0], 0x486199, 0xE8, EnmGhostRoom, 5);
        g_enmGhostOn = GpApplyAll(set, 1, "enemies (victims follow)");
    } else {
        Log("enemies: 00486199 differs from the GOG v7 exe (another mod?): victims do not follow (monsters do)");
    }
    {   // v3.59 EN359: EnemySpawnGate: movsx ecx,[esi+37h]; push ecx; push esi; call 464880; add esp,8; test eax,eax
        static const Site kPinGate = { 0x46511D, { 0x0F, 0xBE, 0x4E, 0x37, 0x51, 0x56, 0xE8, 0x58, 0xF7, 0xFF, 0xFF, 0x83, 0xC4, 0x08,
                                                   0x85, 0xC0 }, 16 };
        if (!g_enmPinGateIni) {
            Log("enemies: pinned ghosts: the gate site is off ([Gameplay] PinnedGhostsStay=0): a ghost pinned in a room the game's ghost "
                "rule refuses is lost at its next room load (its Sword stays)");
        } else if (!SiteOk(&kPinGate)) {
            Log("enemies: 0046511D differs from the GOG v7 exe (another mod?): pinned ghosts keep the game's own gate");
        } else {
            GpBranch(&set[0], 0x465123, 0xE8, EnmGatePin, 5);
            g_enmPinGateOn = GpApplyAll(set, 1, "enemies (pinned ghosts stay)");
            if (g_enmPinGateOn)
                Log("enemies: pinned ghosts stay (00465123): a ghost pinned by the Sword in a room the game's ghost rule refuses is "
                    "made pinned there again at the room load ([Gameplay] PinnedGhostsStay=1)");
        }
    }
    if (SiteOk(&kBone)) {  // v3.30 EN330 (separate: without it the pool grows at the next world reset only, as in v3.29)
        GpBranch(&set[0], 0x4F0A5A, 0xE8, EnmBoneReset, 5);
        g_enmBoneSite = GpApplyAll(set, 1, "enemies (bone pool at a room change)");
    } else {
        Log("enemies: 004F0A5A differs from the GOG v7 exe (another mod?): the bone pool grows at a New Game / title load only");
    }
    {   // v3.33 EN333: the file functions a follower's files are loaded with (called, never patched); else no loading
        static const Site kFiles[] = {
            { 0x573470, { 0x81, 0xEC, 0x10, 0x02, 0x00, 0x00, 0xA1, 0xB8, 0x2F, 0x6A, 0x00 }, 11 },  // FileRequestAsync
            { 0x573370, { 0xE9, 0xAB, 0x61, 0xFF, 0xFF }, 5 },                                       // FileQueueIsIdle (jmp 569520)
            { 0x417D40, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x04, 0x08, 0x00, 0x00 }, 12 },  // the set-up pass
            { 0x573440, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00 }, 11 },  // FileGetTableEntry
            { 0x467470, { 0x56, 0x57, 0x33, 0xF6, 0xE8, 0xA7, 0x80, 0x00, 0x00 }, 9 },               // EnemyGetMotionHandle
        };
        int k;
        g_enmFileFns = 1;
        for (k = 0; k < (int)(sizeof kFiles / sizeof kFiles[0]); ++k)
            if (!SiteOk(&kFiles[k])) {
                g_enmFileFns = 0;
                Log("enemies: %08lX is not the stock function (another mod?): followers whose files are not in the next room's "
                    "list stay behind", kFiles[k].va);
                break;
            }
    }
    {   // v3.38 EN338: the ghosts' room timer gate (called, never patched): mov eax,[FD5A90]; fld [5CE064]; sub eax,4 ...
        static const Site kTimer = { 0x46F9C0, { 0xA1, 0x90, 0x5A, 0xFD, 0x00, 0xD9, 0x05, 0x64, 0xE0, 0x5C, 0x00 }, 11 };
        g_enmTimerFn = SiteOk(&kTimer);
        if (!g_enmTimerFn)
            Log("enemies: 0046F9C0 is not the stock function (another mod?): ghost copies do not wait for the game's room timer");
    }
    if (SiteOk(&kSave)) {  // v3.31 EN331 (separate: without it a save holds the copies' records, purged at the load as in v3.30)
        GpBranch(&set[0], 0x465438, 0xE8, EnmSaveRecord, 5);
        g_enmSaveSite = GpApplyAll(set, 1, "enemies (saves without copies)");
    } else {
        Log("enemies: 00465430 differs from the GOG v7 exe (another mod?): a save may hold copies' records (purged at the load)");
    }
    {   // v3.56 ER356: By World keeps a dead Tremer's body (separate: without it Tremers keep the game's own coin flip)
        // TremerDestroy: cmp eax,0Bh; jne 48AD2C; call 4215D0; fcomp [5B79E0] (0.5); fnstsw ax; test ah,5; jnp 48AD2C
        static const Site kTremer = { 0x48ACE0, { 0x83, 0xF8, 0x0B, 0x75, 0x47, 0xE8, 0xE6, 0x68, 0xF9, 0xFF, 0xD8, 0x1D, 0xE0, 0x79,
                                                  0x5B, 0x00, 0xDF, 0xE0, 0xF6, 0xC4, 0x05, 0x7B, 0x35 }, 23 };
        static const Site kRand = { 0x4215D0, { 0x51, 0xE8, 0xD0, 0x5B, 0x15, 0x00, 0x8B, 0xC8 }, 8 };  // MathRand01 (called)
        if (!g_erTremerIni) {
            Log("enemies: return: the Tremer site is off ([Gameplay] ReturnTremers=0): By World leaves Tremers the game's own "
                "rule (4 in 10 killed ones stand again at their room's next visit)");
        } else if (!SiteOk(&kTremer) || !SiteOk(&kRand) || *(const float *)0x5B79E0 != 0.5f) {
            Log("enemies: return: 0048ACE0 or 004215D0 differs from the GOG v7 exe (another mod?): By World leaves Tremers the "
                "game's own rule");
        } else {
            GpBranch(&set[0], 0x48ACE5, 0xE8, hkErTremer, 5);
            g_erTremerSite = GpApplyAll(set, 1, "enemies (Tremer bodies, By World)");
        }
    }
    Log("enemies: in (room load 004F0AF8, kill count 00450DB0, bone pool 00419830%s%s%s%s); EnemyCopies %s, EnemyFollow %s, "
        "EnemyRevive %s, BonePool %d (-1 Auto = %d while Copies or Follow is on), FollowMax %d, FollowDelay %.1f s, "
        "FollowRange %.0f, CopyStoryGhosts %d, ReviveEventDogs %d, FollowCopies %d, FollowRangeVictims %.0f (v3.38: every ghost "
        "in the room follows), FollowKeepDoors %d (-1 = Always: the whole world visit), LateCopies %d, DownKeep %d (%s), FollowStoryGhosts %d",
        g_enmGhostOn ? ", victims 00486199" : "",
        g_enmBoneSite ? ", room change 004F0A5A" : "", g_enmSaveSite ? ", save 00465438" : "",
        g_erTremerSite ? ", Tremer bodies 0048ACE5" : "",  // v3.56 ER356
        kGpName[GP_COPIES][g_gp[GP_COPIES]], kGpName[GP_FOLLOW][g_gp[GP_FOLLOW]], kGpName[GP_REVIVE][g_gp[GP_REVIVE]],
        g_enmPoolSetting, ENM_POOL_AUTO_N, g_enmFollowMax, g_enmFollowDelay, g_enmFollowRange, g_enmStoryOn, g_enmReviveDogs,
        g_enmFollowCopies, g_enmVictimRange,
        g_enmKeepDoors,
        g_enmLateCopies, g_enmCdKeepOn, g_enmCdKeepOn ? "a hit never shortens a ghost's down" : "the game's own down countdown",
        g_enmStoryFollow);  // v3.34 EN334; v3.35 GH335; v3.38 EN338
    EfInstall();  // v3.50 EF350: the followers' side file (0x4FC1A4 / 0x4FC6ED) and Enemy Pursuit's line
    EpInstall();  // v3.51 EP351: Enemy Pursuit inside the room (calls only; its line)
}
#include "enmfollow.inc"  // v3.50 EF350: Enemy Pursuit's row, the followers' side file (save / load)
#include "pursuit_room.inc"  // v3.51 EP351: Enemy Pursuit inside the room
