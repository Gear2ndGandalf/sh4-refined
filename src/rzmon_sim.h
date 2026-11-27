// v3.49 RZ349: the Randomizer's Monsters, the pure part (no game memory): the [Randomizer] Monsters texts, the source /
// target rules, the room-script scan of the enemy placements (0x52C4C0 / 0x52CA00 / 0x52C530's rules over a window of
// memory, as rando_sim.h does for the pickups), the stock table, the per-enemy roll, the bone estimate, the record rule,
// the descriptor transform, the save filter and a word stack for patch-and-restore. src/rzmon.inc runs it on the game's
// .data; test/rzmon_sim_test.c runs it on the stock exe (SH4Decomp orig/sh4.exe) and on made-up scripts.
// Plan: staging/v349_feedback/w3_monsters/PLAN.md; research: staging/research_randomizer/REPORT.md sections 1, 2, 6, 7.
// v3.50 RM350 (staging/v350_feedback/w2_monsters/PLAN.md): the game's own records of stock monsters are adopted (a save-based
// game rolls), All Worlds, the reasons counted.
// v3.51 SN351 (staging/v351_feedback/w4_sniffers/PLAN.md; the user's v3.50 QA item 4: "the first sniffers I encounter do not
// randomize"): the Subway's first Sniffer Dogs (room 2:2, spawn mode 7 / 6 / 6: hidden and asleep until story flag 0x130, set
// by the room's cut-scene event 00A) were no source; they are now (RmEventDogMode), rolled only once that flag is set (their
// events are over: no event reads them then), else kept as placed (RMW_EVENT). A roll that gives the placement's own type
// rolls once more while another type is in the pool (RmPlanIn.rollAgain; hidden [Randomizer] MonsterRollAgain=0: v3.50).
//
// Facts (checked against the stock exe by tools/rzmon_scan.py and the native test, 2026-11-25):
// - An enemy placement is a kind-2 object of a room script. Its type is command 7's argument (the type index: 0x52C530
//   case 7 takes the 4 functions update / create / destroy / late from the row 0x615348 + 16 * type through 0x470050);
//   command 6 names a row directly (no source uses it). Its mode word ObjInfo +0x14 (desc+0x24; commands 0x0B / 0x0D /
//   0x24, or 0x0A arg 0): bits 0-3 the spawn mode (3 / 6 / 7 / 8 = events and bosses), bits 4-7 the body's turn
//   (0x45DBC0: 0x20..0x60 = on a wall or a ceiling; 0 / 0x10 upright), bits 8-11 the variant (0x45B520), bits 12-15 the
//   type's own (0x45B550: the Tremer's room box for its clamp 0x488DE0, 0 = no clamp; the Gum Head's carried item, a golf
//   club, EnemyGumHeadCreate 0x49A3D0). Position: command 8 (desc+0x14). Id override +0x1C (0x0F / 0x29 / 0x0A arg 2),
//   kill index +0x20 (0x10 / 0x2A), story flags +0x44 / +0x48 (0x19 / 0x26), difficulty mask +0x18 (0x0C / 0x0E: 0x1F).
// - 783 kind-2 placements in the 14 scripts that hold any; 235 sources (RmIsSource): Whitestool 02 v0 x100, Hummer 03
//   v0 x22, Gum Head 07 v0 x14 / v1 x4, Twin Victim 08 v0 x19 / v1 x5, Tremer 09 v0 x12 / v1 x8 / v2 x3, Sniffer Dog 0A
//   v0 x29 / v1 x19; at most 20 in one room. Not sources: other types (224), on a wall or ceiling (192), event spawn
//   modes (39), variants past the type's stat rows (37: Hummer v2 / v3, Whitestool v1, Twin Victim v2), id overrides
//   (28), story flags (23), Gum Heads with an item (5).
//   v3.51 SN351: + the 3 event dogs of Subway room 2:2 (0A v0, modes 7 / 6 / 6, kill indexes 2 / 3 / 4): 238 sources, 36
//   event spawn modes left (Water Prison room 0x23's 15 Whitestools in mode 7, Building room 0x12's 5 Gum Heads in mode 6,
//   stage 6's 8 dogs in mode 3 in rooms 1 / 0x18 (no kill index: a retyped Tremer or Twin Victim would share kill bit 0),
//   stage 7 room 4's 8 Hummers in mode 1 (a forbid flag)).
// - The event dogs (decomp en_48bdb0.c, checked in the stock exe): SnifferDogTask 0x492C00 does nothing while the whole
//   mode word is 6 or 7 and story byte 0x13D80C6 bit 0 (flag 0x130) is clear (`test byte [13D80C6h], 1` at 0x492C5D);
//   SnifferDogCreate 0x48E690 gives mode 3 / 7 AI 0x11 (lies at a body), mode 6 AI 0x15 (waits). Flag 0x130 is set by room
//   2:2's cut-scene event 00A (KB 208); its events 00A / 04A run only while it is clear, and no event reads the dogs then.
//   No other path makes a Sniffer Dog: the room-script runner 0x52CA00 has one caller (the room load 0x4F0AF8, Refined's
//   EnmWrap); the other enemy creates (EnemySpawnObject 0x4623A0: objects 0x13..0x17, the mm, the Hummer nest, the wall
//   boss parts, ghost placeholders) make no dog.
// - gEnemyVariantCounts 0x5B8BA0: 02:1 03:1 07:2 08:2 09:1 0A:2 (the stat rows; the Tremer's variants 1 / 2 are sizes
//   of its one model and share row 0).
// - The game's enemy records (gEnemyManager 0xFCF010: kill bits 0x80 bytes, then 64 records x 0x50 at 0xFCF090; the
//   save copies 0x1B90 bytes from 0xFCF010 at 0x465468): +0x20 id, +0x24 type (short), +0x26 state (short, 0 = free),
//   +0x33 variant, +0x36 stage, +0x37 room. A record is written when a monster's room is left (its destroy) and read by
//   the next create of that id (the spawn gate's 4 = the type's restore): so a retyped monster's record holds its new
//   type, and the placement must be made as that type again while the record lives (RmPlanRoom's record rule).
#ifndef RZMON_SIM_H
#define RZMON_SIM_H
#include "rando_sim.h"
#include "enemies_sim.h"

// ---------------------------------------------------------------- the setting
enum { RMM_OFF, RMM_ROOM, RMM_WORLD, RMM_ALL, RMM_N };  // v3.50 RM350: + RMM_ALL (All Worlds)
// the host's default (the user's answers, 2026-11-25: a mix per enemy, any strength): This World
// v3.56 RE356: the row is Enemies, Off / Same World / All Worlds (the user's v3.55 improvement 14: "This Room" goes). RMM_ROOM
// stays only inside the planner (its tests); no row value and no ini text reaches it: an old "This Room" reads as Same World.
#define RM_MON_DEFAULT RMM_WORLD
static const char *const kRmMonName[RMM_N] = { "Off", "This Room", "Same World", "All Worlds" };  // v3.50 RM350: + All Worlds  // v3.56 RE356: This World -> Same World
#define RM_ROW_N 3  // v3.56 RE356: the row's values: Off / Same World / All Worlds
static const int kRmRowMode[RM_ROW_N] = { RMM_OFF, RMM_WORLD, RMM_ALL };
static int RmRowOfMode(int mode) { return mode == RMM_ALL ? 2 : mode == RMM_ROOM || mode == RMM_WORLD ? 1 : 0; }  // v3.56 RE356
// Enemies (v3.56 RE356; [Randomizer] Monsters before): Off / Same World / All Worlds (also World / All, 0 / 2 / 3); the old
// values This Room / This World / Room / 1 read as Same World; -1 = anything else (the default, logged once)
static int RmMonOfText(const char *s)
{
    int v;
    if (RzTextIs(s, "This Room") || RzTextIs(s, "Room") || RzTextIs(s, "1")) return RMM_WORLD;  // v3.56 RE356: This Room is gone
    if (RzTextIs(s, "This World")) return RMM_WORLD;                                              // v3.56 RE356: the old name
    for (v = 0; v < RMM_N; ++v)
        if (v != RMM_ROOM && RzTextIs(s, kRmMonName[v])) return v;
    if (RzTextIs(s, "World") || RzTextIs(s, "2")) return RMM_WORLD;
    if (RzTextIs(s, "All") || RzTextIs(s, "3")) return RMM_ALL;  // v3.50 RM350
    if (RzTextIs(s, "0")) return RMM_OFF;
    return -1;
}
// v3.56 RE356: a renamed ini key ([Randomizer] Monsters -> Enemies, Ghosts -> SpawnSite): which text decides. The new key's
// text when it holds one (RZK_NEW, or RZK_BAD: not a value, the default); else the old key's (RZK_OLD: migrated once: the
// caller writes the new key and removes the old one; RZK_OLDBAD: not a value, the default, migrated too); else nothing
// (RZK_NONE: the default). *v = the value in force.
enum { RZK_NONE, RZK_NEW, RZK_BAD, RZK_OLD, RZK_OLDBAD };
static int RzKeyPick(const char *newText, const char *oldText, int (*parse)(const char *), int def, int *v)
{
    int x;
    *v = def;
    if (newText && newText[0]) {
        x = parse(newText);
        if (x < 0) return RZK_BAD;
        *v = x;
        return RZK_NEW;
    }
    if (oldText && oldText[0]) {
        x = parse(oldText);
        if (x < 0) return RZK_OLDBAD;
        *v = x;
        return RZK_OLD;
    }
    return RZK_NONE;
}

// ---------------------------------------------------------------- types
#define RM_TYPE_TAB 0x00615348u  // gEnemyTypeDescriptors: 16 bytes a type (update, create, destroy, late)
#define RM_TYPE_MAX 0x17
static int RmIsSourceType(int t) { return t == 0x02 || t == 0x03 || t == 0x07 || t == 0x08 || t == 0x09 || t == 0x0A; }
static int RmIsFlyer(int t) { return t == 0x03; }  // the Hummer: a walker at its spot needs a floor (RmPlanRoom)
// the variants a source may have (gEnemyVariantCounts 0x5B8BA0), the Tremer's three sizes too
static int RmSourceVariantOk(int t, int v)
{
    static const signed char kN[RM_TYPE_MAX] = { 0, 1, 1, 1, 1, 3, 2, 2, 2, 1, 2, 4, 2, 2, 1, 2, 2, 1, 1, 0, 0, 0, 0 };
    if (t == 0x09) return v >= 0 && v <= 2;
    return t >= 0 && t < RM_TYPE_MAX && v >= 0 && v < kN[t];
}
// the targets (a type and variant a placement may become): the source types at variants below their stat rows. Gum Heads
// only with a crash guard (RmPlanIn.allowed: rzmon.inc RmGumGuard), the Tremer at its first size.
typedef struct { unsigned char type, variant; const char *name; } RmTarget;
// v3.56 RE356 (the user's improvement 14: "include all victims and monsters from the whole game"; PLAN.md 2.1): + the
// victims' ghosts: 0B v0..v3 (the male victims), 0C v1, 0D v1 (the old man). Never the story ghosts (Cynthia 0C v0, Jasper
// 0D v0, Andrew 0E, Richard 0F), the Patient 11, Walter 10, Wall Men 05, Wheelchairs 06, mm 04. Two creates write a story
// flag: 0B in stage 4 (EventKeyApplyEffect(0xC19): flag 0x2D3, Water Prison's "Secret Number Memo 1") and every 0D v1
// (flag 0x23B, the Forest's "Forest Ghost"): RmStoryAllowed keeps them out while their flag is clear (the write a no-op then).
static const RmTarget kRmTargets[] = {
    { 0x02, 0, "Whitestool" }, { 0x03, 0, "Hummer" }, { 0x07, 0, "Gum Head" }, { 0x07, 1, "Gum Head (v1)" },
    { 0x08, 0, "Twin Victim" }, { 0x08, 1, "Twin Victim (v1)" }, { 0x09, 0, "Tremer" }, { 0x0A, 0, "Sniffer Dog" },
    { 0x0A, 1, "Sniffer Dog (v1)" },
    { 0x0B, 0, "Victim 0B v0" }, { 0x0B, 1, "Victim 0B v1" }, { 0x0B, 2, "Victim 0B v2" }, { 0x0B, 3, "Victim 0B v3" },  // v3.56 RE356
    { 0x0C, 1, "Victim 0C v1" }, { 0x0D, 1, "Victim 0D v1" },                                                            // v3.56 RE356
};
#define RM_TARGET_N ((int)(sizeof kRmTargets / sizeof kRmTargets[0]))
#define RM_ALL_TARGETS ((1u << RM_TARGET_N) - 1u)
#define RM_GUM_TARGETS ((1u << 2) | (1u << 3))
#define RM_MON_TARGETS 0x01FFu    // v3.56 RE356: the nine monster targets (bits 0-8)
#define RM_GHOST_TARGETS 0x7E00u  // v3.56 RE356: the six victim ghosts (bits 9-14)
#define RM_G0B_TARGETS 0x1E00u    // v3.56 RE356: 0B v0..v3
#define RM_G0D1_TARGET 0x4000u    // v3.56 RE356: 0D v1
typedef char RmTargetsFit[RM_TARGET_N == 15 && (RM_MON_TARGETS | RM_GHOST_TARGETS) == RM_ALL_TARGETS ? 1 : -1];
static int RmIsGhostTarget(int type) { return type == 0x0B || type == 0x0C || type == 0x0D; }  // v3.56 RE356 (a target's type)
// v3.56 RE356: the targets the story allows now (stage: gGame.stage; flag2D3 / flag23B: those story flags are set)
static unsigned RmStoryAllowed(int stage, int flag2D3, int flag23B)
{
    unsigned m = RM_ALL_TARGETS;
    if (stage == 4 && !flag2D3) m &= ~RM_G0B_TARGETS;
    if (!flag23B) m &= ~RM_G0D1_TARGET;
    return m;
}
static int RmTargetIndex(int type, int variant)
{
    int i;
    for (i = 0; i < RM_TARGET_N; ++i)
        if (kRmTargets[i].type == type && kRmTargets[i].variant == variant) return i;
    return -1;
}
static const char *RmTypeName(int t)
{
    static const char *const kN[0x13] = { "type 00", "type 01", "Whitestool", "Hummer", "mm", "Wall Man", "Wheelchair",
                                          "Gum Head", "Twin Victim", "Tremer", "Sniffer Dog", "victim ghost", "ghost 0C",
                                          "ghost 0D", "Andrew", "Richard", "Walter", "the Patient", "type 12" };
    return t >= 0 && t < 0x13 ? kN[t] : "?";
}
static int RmBitCount(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1u) ++n;
    return n;
}
static int RmNthBit(unsigned m, int k)  // the index of the k-th set bit (0-based); -1 = none
{
    int i;
    for (i = 0; i < 32; ++i)
        if ((m >> i & 1u) && !k--) return i;
    return -1;
}
// a placement's bone estimate: its cost and its blends' headroom (enemies_sim.h EnmTypeCost / EnmHead with no bone count)
static int RmPlaceCost(int type) { return EnmTypeCost(type) + EnmHead(type, 0); }

// ---------------------------------------------------------------- the mode word
// a source's mode word: spawn mode 0, upright (bits 4-7 0 or 1), bits 12-15 only the Tremer's room box
static int RmModeOk(int type, unsigned mw)
{
    if (mw > 0xFFFFu || (mw & 0xFu)) return 0;
    if ((mw & 0xF0u) > 0x10u) return 0;
    if ((mw >> 12) & 0xFu) return type == 0x09;
    return 1;
}
// v3.51 SN351: an event dog's mode word: a Sniffer Dog asleep until story flag 0x130 (spawn mode 6 waits, 7 lies at a body
// and waits), upright, no bits 12-15. Mode 3 alone (stage 6's dogs: no kill index) stays out
static int RmEventDogMode(int type, unsigned mw)
{
    if (type != 0x0A || mw > 0xFFFFu || ((mw & 0xFu) != 6 && (mw & 0xFu) != 7)) return 0;
    return (mw & 0xF0u) <= 0x10u && !((mw >> 12) & 0xFu);
}
// the mode word a placement gets: the same type keeps its own bits (another variant only); another type gets spawn mode 0,
// upright, its variant and nothing of the old type's own bits (the twin's Enemy Randomizer does the same)
static unsigned RmModeWord(int stockType, unsigned stockMw, int type, int variant)
{
    if (type == stockType) return (stockMw & ~0xF00u) | ((unsigned)variant & 0xFu) << 8;
    return ((unsigned)variant & 0xFu) << 8;
}

// ---------------------------------------------------------------- the scan
#define RM_VMAX 128      // source placements of one script (stock: 71 at most)
#define RM_ROOM_MAX 32   // source placements of one room (stock: 20 at most)
typedef struct {
    unsigned typeVa, modeVa, yVa;  // the script words: command 7's type, the mode word, command 8's y
    unsigned mw;                   // the stock mode word
    float pos[3];                  // the stock position
    unsigned short room, n;        // the room and the object number (the low word of the script id)
    unsigned char type, variant;   // the stock type and variant
    unsigned char event;           // v3.51 SN351: an event dog (RmEventDogMode): rolled only once story flag 0x130 is set
} RmSlot;
typedef struct {
    int n, kind2, overflow, bad, rooms, roomMax;
    int events;                           // v3.51 SN351: source placements that are event dogs (RmSlot.event)
    unsigned roomStart[RZ_ROOMS];         // gRoomScripts' value for the room (0 = none)
    int roomCost[RZ_ROOMS];               // the stock bone estimate of the room's kind-2 placements (RmPlaceCost)
    unsigned char roomBoss[RZ_ROOMS];     // a boss placement: Walter 0x10, the Wall Man boss (type 5, spawn mode 8)
    unsigned char roomN[RZ_ROOMS];        // source placements of the room
    unsigned placed;                      // bit i: target i is placed by the script (any mode: "this world's types")
    unsigned types;                       // bit t: type t is placed by the script (any kind-2 object)
    RmSlot s[RM_VMAX];
} RmScan;
typedef struct {
    int kind, tcmds, cmd6, cmd0a, funcs, modes, overs, flags, diffs, posN, type;
    unsigned typeVa, mw, modeVa, diff, yVa, pos[3];
} RmObj;
static int RmTypeOfRow(unsigned p)  // command 6's row pointer -> the type (-1 = not a row of the table)
{
    if (p < RM_TYPE_TAB || (p - RM_TYPE_TAB) % 16u || (p - RM_TYPE_TAB) / 16u >= RM_TYPE_MAX) return -1;
    return (int)((p - RM_TYPE_TAB) / 16u);
}
// a source placement (tools/rzmon_scan.py why_not ''): kind 2; one command 7 and no 2-6 / 0x0A; a source type at a source
// variant; one mode word with RmModeOk (a Gum Head with an item excluded); no id override; no story flag; difficulty mask
// 0x1F or none; one position; stage 2..8. v3.51 SN351: + an event dog's mode word (RmEventDogMode)
static int RmIsSource(const RmObj *o, int stage)
{
    const int v = (int)(o->mw >> 8 & 0xFu);
    if (o->kind != 2 || o->tcmds != 1 || o->cmd6 || o->cmd0a || o->funcs) return 0;
    if (!RmIsSourceType(o->type) || stage < 2 || stage > 8) return 0;
    if (o->modes != 1 || !(RmModeOk(o->type, o->mw) || RmEventDogMode(o->type, o->mw)) || !RmSourceVariantOk(o->type, v)) return 0;
    if (o->overs || o->flags || o->posN != 1) return 0;
    return o->diffs == 0 || (o->diffs == 1 && o->diff == 0x1Fu);
}
static void RmObjFlush(const RmObj *o, int stage, int room, int n, RmScan *out)
{
    int t;
    if (o->kind != 2) return;
    ++out->kind2;
    out->roomCost[room] += RmPlaceCost(o->type >= 0 ? o->type : 0x1F);
    if (o->type == 0x10 || (o->type == 0x05 && (o->mw & 0xFu) == 8)) out->roomBoss[room] = 1;
    if (o->type >= 0 && o->type < 32) out->types |= 1u << o->type;
    if ((t = RmTargetIndex(o->type, (int)(o->mw >> 8 & 0xFu))) >= 0) out->placed |= 1u << t;
    if (!RmIsSource(o, stage)) return;
    if (out->n >= RM_VMAX || out->roomN[room] >= RM_ROOM_MAX) {
        out->overflow = 1;
        return;
    }
    {
        RmSlot *s = &out->s[out->n++];
        s->typeVa = o->typeVa, s->modeVa = o->modeVa, s->yVa = o->yVa, s->mw = o->mw;
        memcpy(s->pos, o->pos, sizeof s->pos);
        s->room = (unsigned short)room, s->n = (unsigned short)n;
        s->type = (unsigned char)o->type, s->variant = (unsigned char)(o->mw >> 8 & 0xFu);
        s->event = (unsigned char)RmEventDogMode(o->type, o->mw);  // v3.51 SN351
        out->events += s->event;
        if (++out->roomN[room] > out->roomMax) out->roomMax = out->roomN[room];
    }
}
// one room: the runner's rule from the room's start (rando_sim.h RzScanRoom's walk)
static void RmScanRoom(const RzWin *w, unsigned p, int stage, int room, RmScan *out)
{
    unsigned wd, a;
    int first = 1, guard = 0, n = 0;
    RmObj o;
    memset(&o, 0, sizeof o);
    o.type = -1;
    for (;;) {
        unsigned tag, num;
        if (++guard > RZ_GUARD || !RzRd(w, p, &wd)) {
            out->bad = 1;
            return;
        }
        tag = wd & 0xFFFFu, num = wd >> 16;
        if (!first && (wd == 0xF0F0F0F0u || tag == 0)) break;
        if (tag == 1 && !first) {
            RmObjFlush(&o, stage, room, n, out);
            ++n;
            memset(&o, 0, sizeof o);
            o.type = -1;
        }
        switch (tag) {
        case 1: if (num >= 1 && RzRd(w, p + 4, &a)) o.kind = (int)a; break;
        case 2: case 3: case 4: case 5: ++o.funcs; break;
        case 6:
            if (num >= 1 && RzRd(w, p + 4, &a)) o.type = RmTypeOfRow(a), o.typeVa = p + 4;
            ++o.cmd6, ++o.tcmds;
            break;
        case 7:
            if (num >= 1 && RzRd(w, p + 4, &a)) o.type = a < 256u ? (int)a : -1, o.typeVa = p + 4;
            ++o.tcmds;
            break;
        case 8:
            if (num >= 3 && RzRd(w, p + 4, &o.pos[0]) && RzRd(w, p + 8, &o.pos[1]) && RzRd(w, p + 12, &o.pos[2])) o.yVa = p + 8;
            ++o.posN;
            break;
        case 0x0A:
            ++o.cmd0a;
            if (num >= 1 && RzRd(w, p + 4, &o.mw)) o.modeVa = p + 4, ++o.modes;
            ++o.overs, ++o.diffs, o.diff = 0;
            break;
        case 0x0B: case 0x0D: case 0x24:
            if (num >= 1 && RzRd(w, p + 4, &o.mw)) o.modeVa = p + 4;
            ++o.modes;
            break;
        case 0x0C: case 0x0E:
            if (num >= 1 && RzRd(w, p + 4, &o.diff)) ++o.diffs;
            break;
        case 0x0F: case 0x29: ++o.overs; break;
        case 0x19: case 0x26: if (num >= 1 && RzRd(w, p + 4, &a) && a) o.flags = 1; break;
        }
        first = 0;
        p += 4u * (num + 1u);
    }
    RmObjFlush(&o, stage, room, n, out);
}
// a stage's script from its start (0x6553A4[stage] / 0x6553D8[stage]). 1 = read
static int RmScanScript(const RzWin *w, unsigned script, int stage, RmScan *out)
{
    unsigned p = script, wd, room;
    int guard = 0, r;
    memset(out, 0, sizeof *out);
    for (;;) {
        if (++guard > RZ_GUARD || !RzRd(w, p, &wd)) return out->bad = 1, 0;
        if (wd == 0xF0F0F0F0u) break;
        if ((wd & 0xFFFFu) == 0) {
            if (!RzRd(w, p + 4, &room) || room >= RZ_ROOMS) return out->bad = 1, 0;
            if (!out->roomStart[room]) ++out->rooms;
            out->roomStart[room] = p + 8;
        }
        p += 4u * ((wd >> 16) + 1u);
    }
    for (r = 0; r < RZ_ROOMS; ++r)
        if (out->roomStart[r]) RmScanRoom(w, out->roomStart[r], stage, r, out);
    return !out->bad;
}
// the slot of (room, n) in a scan, -1 = not a source placement
static int RmSlotOf(const RmScan *sc, int room, int n)
{
    int i;
    for (i = 0; sc && i < sc->n; ++i)
        if (sc->s[i].room == room && sc->s[i].n == n) return i;
    return -1;
}

// ---------------------------------------------------------------- the stock table (tools/rzmon_scan.py --header)
typedef struct { int stage, visit; unsigned script; int kind2, n; unsigned char cnt[6]; } RmVisitCount;
static const unsigned char kRmCntType[6] = { 0x02, 0x03, 0x07, 0x08, 0x09, 0x0A };
// stock exe md5 15a85581eca3d0664089c440b0acada1: the scripts that hold kind-2 objects (visit 1 only where the second
// half has its own script); kind-2 placements, source placements, by source type 02 03 07 08 09 0A
static const RmVisitCount kRmStock[] = {
    { 0x0, 0, 0x00655420,   2,   0, { 0, 0, 0, 0, 0, 0 } },
    { 0x2, 0, 0x0067EF60,  46,  11, { 0, 0, 0, 0, 0, 11 } },  // v3.51 SN351: + room 2's 3 event dogs (was 8)
    { 0x2, 1, 0x00683AA0,  53,  21, { 0, 0, 6, 0, 10, 5 } },
    { 0x3, 0, 0x00667D30,  41,  11, { 0, 1, 0, 0, 0, 10 } },
    { 0x3, 1, 0x0066C558,  37,  12, { 0, 5, 1, 3, 0, 3 } },
    { 0x4, 0, 0x006888C0, 142,  29, { 23, 0, 0, 4, 2, 0 } },
    { 0x4, 1, 0x0068E578, 162,  71, { 61, 0, 0, 9, 1, 0 } },
    { 0x5, 0, 0x0065FCC0,  81,  22, { 8, 0, 8, 0, 0, 6 } },
    { 0x5, 1, 0x006632C8,  74,  34, { 8, 14, 3, 0, 0, 9 } },
    { 0x6, 0, 0x00672090,  44,  19, { 0, 2, 0, 0, 10, 7 } },
    { 0x7, 0, 0x006763E0,  53,   0, { 0, 0, 0, 0, 0, 0 } },
    { 0x8, 0, 0x00679FF0,  34,   8, { 0, 0, 0, 8, 0, 0 } },
    { 0x9, 0, 0x00679DB0,   2,   0, { 0, 0, 0, 0, 0, 0 } },
    { 0xB, 0, 0x0067E270,  12,   0, { 0, 0, 0, 0, 0, 0 } },
};
#define RM_STOCK_N ((int)(sizeof kRmStock / sizeof kRmStock[0]))
#define RM_STOCK_KIND2 783
#define RM_STOCK_TOTAL 238   // v3.51 SN351: + the 3 event dogs (v3.50: 235)
#define RM_STOCK_EVENTS 3    // v3.51 SN351: of them event dogs (Subway room 2:2)
// 1 = the scan of (stage, visit) gives the stock counts and script start; 0 = not; -1 = not in the table
static int RmStockMatch(int stage, int visit, unsigned script, const RmScan *sc)
{
    int i, k, j;
    for (i = 0; i < RM_STOCK_N; ++i) {
        const RmVisitCount *v = &kRmStock[i];
        unsigned char c[6];
        if (v->stage != stage || v->visit != visit) continue;
        if (v->script != script || sc->bad || sc->overflow || sc->kind2 != v->kind2 || sc->n != v->n) return 0;
        if (sc->events != (stage == 2 && visit == 0 ? RM_STOCK_EVENTS : 0)) return 0;  // v3.51 SN351: only room 2:2's event dogs
        memset(c, 0, sizeof c);
        for (k = 0; k < sc->n; ++k)
            for (j = 0; j < 6; ++j)
                if (sc->s[k].type == kRmCntType[j]) ++c[j];
        return memcmp(c, v->cnt, sizeof c) == 0;
    }
    return -1;
}

// ---------------------------------------------------------------- the plan of one room load
// a placement's key: the seed, the stage and half, the room, the object number (the script id's parts); Change Each Visit:
// rando_sim.h RzVisitKey with the room's visit count (0 = fixed)
static unsigned RmSlotKey(unsigned seed, int stage, int half, int room, int n)
{
    return RzHash4(seed ^ 0x4D4F4E53u, (unsigned)stage | (unsigned)(half & 1) << 8, (unsigned)room, 0x4D000000u ^ (unsigned)(n & 0xFFFF));
}
typedef struct {
    int mode;                   // RMM_ROOM / RMM_WORLD / RMM_ALL (v3.50 RM350)
    unsigned seed;
    int stage, half, room;
    unsigned visit;             // the room's visit count (Change Each Visit On), 0 = fixed
    int reroll;                 // Change Each Visit On: a record in this room keeps no type (the roll decides; another type's
                                // record is deleted: that monster comes back fresh, at its placement, as the new type)
    unsigned resident;          // bit i: target i's files are resident here now (EnmLoadedHere)
    unsigned world;             // bit i: target i is placed by this world's scripts (both halves: RmScan.placed of each)
    unsigned allowed;           // bit i: target i may be used at all (RM_ALL_TARGETS less the Gum Heads without a guard)
    int freeBones, totalBones;  // the bone pool now (0xED73E4 / 0xED73EC)
    int released;               // v3.51 SN351: story flag 0x130 is set (byte 0x13D80C6 bit 0): the event dogs may roll
    int rollAgain;              // v3.51 SN351: a roll that gives the placement's own type rolls once more (another type in the pool)
} RmPlanIn;
// the game's record of a placement (the caller reads the 64 records): type -1 = none; here = in this room; ours = Refined
// made that monster as exactly that type and variant (v3.49 fix2 RZ349-SAVE-001: the provenance table RmProv, below)
typedef struct { int type, variant, here, ours; } RmRec;
enum { RMW_SAME, RMW_ROLL, RMW_STICKY, RMW_AWAY, RMW_BONES, RMW_FLOOR, RMW_FILES, RMW_NONE, RMW_OTHER, RMW_WORDS, RMW_EVENT, RMW_N };  // fix2: + OTHER  // v3.50 RM350: + WORDS (a word not the stock value: put back)  // v3.51 SN351: + EVENT (an event dog before story flag 0x130: as placed)
// drop: the record in this room is deleted first; adopt (v3.50 RM350): the placement's record here was the game's own record
// of its stock monster (not Refined's): adopted, rolled like a fresh placement (RmPlanRoom rule 0); again (v3.51 SN351): the
// first roll gave its own type and it rolled once more
typedef struct { unsigned char type, variant, why, drop, adopt, again; } RmPick;
// v3.50 RM350: + adopted (game records adopted), adoptChanged (of them made as another type: their record deleted), rollStock
// (rolled their own stock type), same (kept the stock type by a record of the stock type); v3.51 SN351: + again (rolled once
// more for their own type), event (event dogs kept as placed: story flag 0x130 clear), eventPlanned (event dogs planned as the
// others: the flag set)
typedef struct { int n, changed, sticky, away, bones, floor, files, drops, none, cost0, cost, limit, other, adopted, adoptChanged,
                 rollStock, same, again, event, eventPlanned; } RmPlanStat;
// This Room: the resident targets; This World: also every target this world places (the stage's two halves: the same
// world's monsters, its sound tables; their files are loaded at the room load); v3.50 RM350 All Worlds: every target of any
// world (the files of each one not resident loaded at the room load; its sounds may be missing outside its own worlds)
static unsigned RmPool(const RmPlanIn *in)
{
    const unsigned m = in->mode == RMM_ALL ? RM_ALL_TARGETS : in->mode == RMM_WORLD ? in->resident | in->world : in->resident;
    return m & in->allowed & RM_ALL_TARGETS;
}
// the room's slots idx[0..m) (script order), their records rec[k] and floor answers floorOk[k] (1 = a walker may stand at
// a flyer's spot: only asked for Hummer sources); out[k] the picks. The rules, in order:
// 0. (v3.49 fix2 RZ349-SAVE-001) a record Refined did not make (not `ours`: another writer, or the game's own from before
//    Refined planned the room) keeps the placement as placed and is never deleted (RMW_OTHER when its type is not the
//    stock one, RMW_AWAY when it is the stock one in another room); only Refined's own records follow rules 1 and 5.
//    v3.50 RM350 (the user's v3.49 QA: a save-based game never rolled): the game's own record of the stock monster (its
//    type and variant = the placement's stock ones, this stage, in this room: what every room a save visited holds) is
//    adopted: rolled like a fresh placement (rule 2); when the roll gives another type that record is deleted (rule 5:
//    that monster comes back fresh, at its placement, as the new type: its hurt / downed state and spot are lost); when
//    the roll gives the stock type the record stays (the monster as the save left it). The caller then writes the
//    provenance (RmProvPut), so the record is Refined's from then on (save filter, RmDescFix, RmDropForeign);
// 1. a record of the placement holds the type it was made as: a target in the pool keeps it (sticky: the type's restore
//    reads its own record; with reroll, only a record elsewhere: one in this room is rolled over); its stock type keeps
//    the stock (no reroll); another one is deleted when it is in this room (else the stock create would restore a
//    record of another type), and left alone (the stock type) when it is elsewhere;
// 2. else a roll over the pool (RmSlotKey + the visit), each target alike (any strength, a mix per enemy); v3.51 SN351 with
//    rollAgain: a roll that gives the placement's own type (any variant) rolls once more (RmAgainKey) while the pool holds
//    another type (the own type then 1 in n^2 / k^2 instead of k / n; never excluded: a world keeps its own monsters);
// (v3.51 SN351, before rule 0) an event dog while story flag 0x130 is clear: as placed (RMW_EVENT), its record kept unless it
//    is Refined's of another type in this room (rule 5);
// 3. a walker at a Hummer's spot needs a floor under it (else the stock type);
// 4. the bones: the room may never need more than max(the pool's free part, the stock room's estimate): a dearer type
//    that does not fit keeps the stock type;
// 5. whatever the pick, a record of Refined's (or an adopted one, v3.50 RM350) in this room of another type than the pick is
//    deleted (drop).
// v3.50 RM350: is rec the game's own record of slot s's stock monster in this room (rule 0's adoption)?
static int RmRecAdopt(const RmRec *r, const RmSlot *s)
{
    return r->type >= 0 && !r->ours && r->here && r->type == s->type && r->variant == s->variant;
}
// v3.51 SN351: does the pool hold a target of another type than `type`? And the second roll's key
static int RmPoolOther(unsigned pool, int type)
{
    int i;
    for (i = 0; i < RM_TARGET_N; ++i)
        if ((pool >> i & 1u) && kRmTargets[i].type != type) return 1;
    return 0;
}
static unsigned RmAgainKey(unsigned key) { return RzHash4(key, 0x41474149u, 0x534E3531u, 0); }
static void RmPlanRoom(const RmPlanIn *in, const RmScan *sc, const int *idx, int m, const RmRec *rec, const unsigned char *floorOk,
                       RmPick *out, RmPlanStat *st)
{
    const unsigned pool = RmPool(in);
    const int base = sc && in->room >= 0 && in->room < RZ_ROOMS ? sc->roomCost[in->room] : 0;
    const int avail = in->freeBones - EnmReserve(in->totalBones);
    int k, cur = base;
    memset(st, 0, sizeof *st);
    st->n = m, st->cost0 = base, st->limit = avail > base ? avail : base;
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &sc->s[idx[k]];
        const RmRec *r = &rec[k];
        RmPick p;
        int ti, delta;
        p.type = s->type, p.variant = s->variant, p.why = RMW_SAME, p.drop = 0, p.again = 0;  // v3.51 SN351: + again
        const int rStock = r->type == s->type && r->variant == s->variant, rKeeps = r->type >= 0 && !(in->reroll && r->here);
        p.adopt = (unsigned char)RmRecAdopt(r, s);  // v3.50 RM350
        if (s->event && !in->released) {  // v3.51 SN351: an event dog before its release: as placed (its events read it)
            p.adopt = 0, p.why = RMW_EVENT;
        } else if (r->type >= 0 && !r->ours && !p.adopt) {  // v3.49 fix2: not Refined's record: as placed, the record kept
            p.why = rStock ? RMW_AWAY : RMW_OTHER;   // v3.50 RM350: the stock one elsewhere = recorded elsewhere (was SAME)
        } else if (!p.adopt && rKeeps && !rStock && (ti = RmTargetIndex(r->type, r->variant)) >= 0 && (pool >> ti & 1u)) {
            p.type = (unsigned char)r->type, p.variant = (unsigned char)r->variant, p.why = RMW_STICKY;
        } else if (!p.adopt && r->type >= 0 && !rStock && !r->here) {
            p.why = RMW_AWAY;
        } else if (p.adopt || !rKeeps || !rStock) {  // v3.50 RM350: + an adopted record: rolled as a fresh placement
            const int cnt = RmBitCount(pool);
            if (!cnt) {
                p.why = RMW_NONE;
            } else {
                const unsigned key = RzVisitKey(RmSlotKey(in->seed, in->stage, in->half, s->room, s->n), in->visit);
                int t = RmNthBit(pool, (int)(key % (unsigned)cnt));
                if (in->rollAgain && kRmTargets[t].type == s->type && RmPoolOther(pool, s->type)) {  // v3.51 SN351: once more
                    t = RmNthBit(pool, (int)(RmAgainKey(key) % (unsigned)cnt));
                    p.again = 1;
                }
                p.type = kRmTargets[t].type, p.variant = kRmTargets[t].variant, p.why = RMW_ROLL;
            }
        }
        if ((p.type != s->type || p.variant != s->variant) && RmIsFlyer(s->type) && !RmIsFlyer(p.type) && !(floorOk && floorOk[k]))
            p.type = s->type, p.variant = s->variant, p.why = RMW_FLOOR;
        delta = RmPlaceCost(p.type) - RmPlaceCost(s->type);
        if (delta > 0 && cur + delta > st->limit) {
            p.type = s->type, p.variant = s->variant, p.why = RMW_BONES;
            delta = 0;
        }
        cur += delta;
        if (r->type >= 0 && (r->ours || p.adopt) && r->here && !(r->type == p.type && r->variant == p.variant))
            p.drop = 1;  // fix2: + ours  // v3.50 RM350: + adopted
        out[k] = p;
        {
            const int changed = p.type != s->type || p.variant != s->variant;
            st->changed += changed;
            st->adopted += p.adopt, st->adoptChanged += p.adopt && changed;  // v3.50 RM350
            st->rollStock += p.why == RMW_ROLL && !changed, st->same += p.why == RMW_SAME;
            st->again += p.again, st->event += p.why == RMW_EVENT, st->eventPlanned += s->event && p.why != RMW_EVENT;  // v3.51 SN351
        }
        st->sticky += p.why == RMW_STICKY, st->away += p.why == RMW_AWAY, st->bones += p.why == RMW_BONES;
        st->floor += p.why == RMW_FLOOR, st->none += p.why == RMW_NONE, st->drops += p.drop, st->other += p.why == RMW_OTHER;
    }
    st->cost = cur;
}
// v3.50 RM350: a pick put back to the stock type after the plan (its files, its words): the counts follow; a record of
// Refined's (or adopted) in this room of another type than the stock one is dropped; an adopted record (the stock type)
// stays. why: the new reason
static void RmPickBack(RmPick *p, const RmSlot *s, const RmRec *r, RmPlanStat *st, int why)
{
    if (p->type != s->type || p->variant != s->variant) {
        --st->changed;
        if (p->adopt) --st->adoptChanged;
    }
    if (p->drop) --st->drops;
    p->type = s->type, p->variant = s->variant, p->why = (unsigned char)why;
    p->drop = r->type >= 0 && (r->ours || p->adopt) && r->here && !(r->type == s->type && r->variant == s->variant);  // fix2: + ours
    st->drops += p->drop;
}
// a pick whose files could not be loaded goes back to the stock type (its record, if any, of the pick's type: dropped)
static void RmRevertPick(RmPick *p, const RmSlot *s, const RmRec *r, RmPlanStat *st)
{
    RmPickBack(p, s, r, st, RMW_FILES);  // v3.50 RM350 (the same rule, shared with the words' fallback)
    ++st->files;
}

// ---------------------------------------------------------------- the descriptor transform (enemies.inc EnmWalkC)
// A placement rebuilt from its script (Enemy Copies, Enemies Follow's and the wanderers' replays) gets the type it lives
// as: desc+4 .. +0x10 the type's 4 functions (row = 0x615348 + 16 * type, as command 7), desc+0x24 its mode word.
// row: the 4 words of the row (the caller reads them). 1 = changed
static int RmFixDesc(unsigned char *desc, const RmSlot *s, int type, int variant, const unsigned *row)
{
    unsigned mw;
    if (type == s->type && variant == s->variant) return 0;
    memcpy(desc + 4, row, 16);
    mw = RmModeWord(s->type, s->mw, type, variant);
    memcpy(desc + 0x24, &mw, 4);
    return 1;
}

// ---------------------------------------------------------------- records
#define RM_REC_SIZE 0x50
#define RM_REC_N 64
#define RM_SAVE_RECS 0x80  // the records' offset in the 0x1B90 bytes the save copies (from the kill bits 0xFCF010)
static unsigned RmRecId(const unsigned char *r) { unsigned v; memcpy(&v, r + 0x20, 4); return v; }
static int RmRecType(const unsigned char *r) { short v; memcpy(&v, r + 0x24, 2); return v; }
static int RmRecState(const unsigned char *r) { short v; memcpy(&v, r + 0x26, 2); return v; }
static int RmRecVariant(const unsigned char *r) { return (signed char)r[0x33]; }
static int RmRecStage(const unsigned char *r) { return (signed char)r[0x36]; }
static int RmRecRoom(const unsigned char *r) { return (signed char)r[0x37]; }
// the record of script id `id` among n records (state != 0), NULL = none
static const unsigned char *RmRecFind(const unsigned char *recs, int n, unsigned id)
{
    int i;
    for (i = 0; i < n; ++i, recs += RM_REC_SIZE)
        if (RmRecState(recs) != 0 && RmRecId(recs) == id) return recs;
    return NULL;
}
// v3.49 fix2 RZ349-SAVE-001 (Codex inspection 2): provenance. For every source placement whose room Refined built (its
// plan, or Monsters Off / left as placed) the table holds the script id, the type and variant Refined made it as and the
// stock ones (this session, memory only; an entry is kept while a record of Refined's with that type lives in another
// room: a follower, a wanderer). A record is Refined's ("ours") only while it still holds exactly that type and variant;
// only such a record is ever deleted, filtered from a save or used for a rebuilt placement. Every other mismatch (another
// mod, the cheat table, a record of Refined's that another writer changed later) is left alone.
#define RM_PROV_N 512  // (the stock scripts: 235 sources, + 27 of the single-script stages under either half's id)
typedef struct { unsigned id; unsigned char type, variant, stockType, stockVariant; } RmProvE;
typedef struct { int n, full; RmProvE e[RM_PROV_N]; } RmProv;
static const RmProvE *RmProvFind(const RmProv *p, unsigned id)
{
    int i;
    for (i = 0; p && i < p->n; ++i)
        if (p->e[i].id == id) return &p->e[i];
    return NULL;
}
static void RmProvPut(RmProv *p, unsigned id, int type, int variant, int stockType, int stockVariant)  // add or overwrite
{
    RmProvE *e = (RmProvE *)RmProvFind(p, id);
    if (!e) {
        if (p->n >= RM_PROV_N) {
            ++p->full;  // (not with the stock scripts: such a placement's records are then nobody's: left alone)
            return;
        }
        e = &p->e[p->n++];
        e->id = id;
    }
    e->type = (unsigned char)type, e->variant = (unsigned char)variant;
    e->stockType = (unsigned char)stockType, e->stockVariant = (unsigned char)stockVariant;
}
// a live record that Refined made: its id is in the table and it holds exactly the type and variant Refined made it as
static int RmProvOurs(const RmProv *p, const unsigned char *r)
{
    const RmProvE *e;
    if (!r || RmRecState(r) == 0 || !(e = RmProvFind(p, RmRecId(r)))) return 0;
    return RmRecType(r) == e->type && RmRecVariant(r) == e->variant;
}
// ... and that type is not its placement's stock one (a randomized monster)
static int RmProvRetyped(const RmProv *p, const unsigned char *r)
{
    const RmProvE *e = RmProvOurs(p, r) ? RmProvFind(p, RmRecId(r)) : NULL;
    return e && !(e->type == e->stockType && e->variant == e->stockVariant);
}
// Monsters Off / a room left as placed: a record in this room (stage, room) is deleted only when it is Refined's randomized one
static int RmOffDrops(const RmProv *p, const unsigned char *r, int stage, int room)
{
    return r && RmRecRoom(r) == room && RmRecStage(r) == stage && RmProvRetyped(p, r);
}
// the save filter: in the copy the save made (64 records), every record of a monster Refined randomized (RmProvRetyped) is
// cleared as EnemyRecordDelete 0x464380 clears one (0x50 zero bytes): a save never holds a monster Refined randomized (it
// comes back as placed). Every other record stays, also one of another type that Refined did not make. The live records
// are not touched. n cleared (nothing while Refined randomized nothing)
static int RmSaveClean(unsigned char *recs, const RmProv *p)
{
    int i, n = 0;
    if (!p || !p->n) return 0;
    for (i = 0; i < RM_REC_N; ++i, recs += RM_REC_SIZE)
        if (RmProvRetyped(p, recs)) {
            memset(recs, 0, RM_REC_SIZE);
            ++n;
        }
    return n;
}

// ---------------------------------------------------------------- patch-and-restore: one word list per room-script run
// (rando_sim.h's RzPatchOne / RzRestoreAll; the same levels as the Items' list: push at the run's start, pop in the
// __finally after the runner, the deepest first)
#define RM_PATCH_MAX (5 * RM_ROOM_MAX)  // a type word, a mode word and a y word a placement at most  // v3.56 RE356: + x / z (Spawn Site: 5)
typedef struct { RzPatchRec rec[RZ_DEPTH][RM_PATCH_MAX]; int n[RZ_DEPTH]; int depth; } RmPatchStack;
static int RmStackPush(RmPatchStack *s)
{
    const int d = s->depth++;
    if (d >= 0 && d < RZ_DEPTH) s->n[d] = 0;
    return d;
}
static int RmStackAdd(RmPatchStack *s, int d, unsigned *word, unsigned stock, unsigned now)  // 1 = written
{
    RzPatchRec *r;
    if (d < 0 || d >= RZ_DEPTH || s->n[d] >= RM_PATCH_MAX) return 0;
    r = &s->rec[d][s->n[d]];
    r->word = word, r->stock = stock, r->now = now;
    if (!RzPatchOne(r)) return 0;
    ++s->n[d];
    return 1;
}
static int RmStackTrim(RmPatchStack *s, int d, int keep)
{
    int other;
    if (d < 0 || d >= RZ_DEPTH || s->n[d] <= keep) return 0;
    other = RzRestoreAll(&s->rec[d][keep], s->n[d] - keep);
    s->n[d] = keep;
    return other;
}
static int RmStackPop(RmPatchStack *s, int d)
{
    int k, other = 0;
    if (d < 0) d = 0;
    for (k = (s->depth < RZ_DEPTH ? s->depth : RZ_DEPTH) - 1; k >= d; --k) other += RmStackTrim(s, k, 0);
    s->depth = d;
    return other;
}

#endif
