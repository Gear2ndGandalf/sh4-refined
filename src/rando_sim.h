// v3.48 RZ348: the Randomizer's pure part (no game memory): the [Randomizer] ini texts, the seed, the room-script scan
// (0x52C4C0 / 0x52CA00 / 0x52C530's rules over a window of memory), the plain-pickup and weapon-spot rules, the stock
// tables, the plans (Shuffle / Random) and the hashes. src/randomizer.inc runs it on the game's .data;
// test/rando_sim_test.c runs it on the stock exe (SH4Decomp orig/sh4.exe) and on small made-up scripts.
// Plan: staging/v348_feedback/w3_rando/PLAN.md (round 2: weapons, the look, Change Each Visit).
//
// Facts (checked against the stock exe by tools/rz_scan.py and the native test, 2026-11-25):
// - A script is a row of commands {u16 tag, u16 argc, argc words}; tag 0 starts a room (arg 0 = the room), tag 1 the
//   next object (arg 0 = its kind; 4 = an item pickup, with the event-object functions 0x4CF330 / 0x4CF4B0 / 0x4CF0C0 /
//   0x4CF350), 0xF0F0F0F0 ends the stage's script. The room runner steps the room's first command before its loop, so a
//   tag 1 there flushes nothing; the room's end flushes the last object. The object number n counts every flush.
// - A pickup's item (ObjInfo +0x1C) comes from tag 0x0F or 0x29 (arg 0) or 0x0A (arg 2); +0x14 / +0x18 its model /
//   texture (0x0A args 0-1, 0x0B / 0x0D / 0x24, 0x0C / 0x0E); +0x54 its own event record (0x18; 0x20 arg 0); +0x40 the
//   taken bit (0x1B / 0x25, read as a short). Tags 2-7 replace the object's functions.
// - EventTaskInit 0x4D0500 (ev_4d0500.c): a kind-4 object without its own record gets the default record 0x6184E4 with
//   the item's own pickup script ItemGetPickupScript(item) (table 0x62DD18); with a record, the record's script runs.
//   A pickup script (the twin's event_scripts report 5) is BANK 1; ASK a; CALL_ON_SHOW 50ABE0; CASE 0; NEXT; CASE 1;
//   STOP; SAY b; GIVE item; STOP: the prompt (a, b) and the GIVE are the script's. So the item word AND the script decide
//   what is offered and given: a spot with a record of its own needs its record word patched with the item.
// - The look (round 2, from the code): the model is ItemGetModelFile / ItemGetTextureFile(item) when the object names
//   none (EventObjectInit 0x4CF4B0), its scale gItemFloatTable(item) (EventObjectDraw 0x4CF350, item = obj->item); the
//   zone's size is the object's own (cmd 0x14) or, with zone flag 8, the constants 0x62D5B4.. (ActorZoneInit 0x52B8D0):
//   never the item's. So the item word decides the model on the ground, the record / default script the prompt and the
//   GIVE (the inventory): nothing else to patch.
// - EventObjectIsEnabled 0x4CF120 hides an obtained item only outside 1..0x0B (ItemIsObtained 0x4F69C0: the byte
//   0x13D7FF8 + item): a weapon spot whose weapon is obtained is not shown. The taken bit is (stage, room, bit).
// - 273 kind-4 objects in the 17 distinct scripts; 137 plain (RzIsPlain): pistol bullets 46, Nutrition Drink 40, Holy
//   Candle 16, Medical Kit 12, Saint Medallion 8, revolver bullets 5, Ampoule 4, Sword of Obedience 4, silver bullets 2;
//   18 of them in Room 302. 27 weapon spots (RzIsWeaponSpot: a simple record of their own, the weapon in wp_model.bin).
//   The 2nd-half scripts of stages 1..5 repeat the 1st half's pickups on the same (room, taken bit) with the same item.
// - Pickup models: item_model.bin 0xF000F003 (items 1-4, 8-0x0B), wp_model.bin 0xF000F057 (the Sword and every weapon
//   but the Steel Pipe / Spade (file 4) and the Paper-Cutting Knife / Stun Gun (file 0x1C), which are listed in their own
//   rooms only): a weapon moves only when its model is in wp_model.bin (in the world list of stages 1..8).
// - Story checks on weapons (every constant-item call of ItemIsObtained / ItemPocketCount / ItemChestCount in .text):
//   0x4DB050 Knife && Stun Gun (a Room 302 message only), 0x4E0B70 Chainsaw (the condition of the Forest's New Game+
//   Chainsaw, a record with a condition: never touched). No story event needs a weapon; the story weapons (the Pistol
//   and the Steel Pipe of Room 302, the Torch) have keyed records and stay.
#ifndef RANDO_SIM_H
#define RANDO_SIM_H
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------- settings
enum { RZI_OFF, RZI_SHUFFLE, RZI_RANDOM, RZI_N };
// the user's answers (2026-11-25): Items Random, Keep Specials Off (candles, medallions and swords are mixed in), Room 302
// In; round 2: Change Each Visit On
// v3.56 RI356 (the user's QA of v3.55, improvements 12, 13, 16; the session's decisions): the same answers in the new rows:
//   [Randomizer] Specials On / Off (was KeepSpecials, the meaning turned: Specials On = they are drawn too = KeepSpecials
//   Off), Room302 Off / Shuffle / Random (was Out / In: In = drawn like Items), Reroll By Room / By World (was PerVisit
//   On / Off: On = By Room; Off, one fixed draw a seed, is folded into By World). Each old value is read once and migrated.
#define RZ_ITEMS_DEFAULT RZI_RANDOM
#define RZ_SPECIALS_DEFAULT 1        // v3.56 RI356: drawn too (v3.55's Keep Specials Off)
#define RZ_R302_DEFAULT RZI_RANDOM   // v3.56 RI356: Random (v3.55's In with Items Random)
enum { RZR_ROOM, RZR_WORLD, RZR_N };  // v3.56 RI356: Reroll Visits By Room / By World
#define RZ_REROLL_DEFAULT RZR_ROOM   // v3.56 RI356 (v3.55's PerVisit On)
static const char *const kRzItemsName[RZI_N] = { "Off", "Shuffle", "Random" };
static const char *const kRzKeepName[2] = { "Off", "On" };  // (every On / Off value: Specials; Monsters' and the looks' logs)
static const char *const kRzR302Name[RZI_N] = { "Off", "Shuffle", "Random" };  // v3.56 RI356 (was Out / In)
static const char *const kRzRerollName[RZR_N] = { "By Room", "By World" };    // v3.56 RI356

static int RzTextIs(const char *s, const char *name)  // whole text, no case, spaces around ignored
{
    size_t n = strlen(name), i;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    for (i = 0; i < n; ++i) {
        char a = s[i], b = name[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    for (s += n; *s == ' ' || *s == '\t'; ++s) {}
    return *s == 0;
}
// Items: Off / Shuffle / Random or 0 / 1 / 2; -1 = anything else (the caller keeps the default and logs it once)
static int RzItemsOfText(const char *s)
{
    int v;
    for (v = 0; v < RZI_N; ++v)
        if (RzTextIs(s, kRzItemsName[v])) return v;
    if (RzTextIs(s, "0")) return RZI_OFF;
    if (RzTextIs(s, "1")) return RZI_SHUFFLE;
    if (RzTextIs(s, "2")) return RZI_RANDOM;
    return -1;
}
// Specials (v3.56 RI356; also v3.55's KeepSpecials / PerVisit): On / Off, 1 / 0, Yes / No; -1 = anything else
static int RzOnOffOfText(const char *s)
{
    if (RzTextIs(s, "On") || RzTextIs(s, "1") || RzTextIs(s, "Yes")) return 1;
    if (RzTextIs(s, "Off") || RzTextIs(s, "0") || RzTextIs(s, "No")) return 0;
    return -1;
}
// v3.55's Room302: In / Out (and On / Off, 1 / 0); -1 = anything else (v3.56 RI356: read only to migrate it)
static int RzR302OfText(const char *s)
{
    if (RzTextIs(s, "In")) return 1;
    if (RzTextIs(s, "Out")) return 0;
    return RzOnOffOfText(s);
}
// v3.56 RI356: [Randomizer] Room302 now: Off / Shuffle / Random (and 0 / 2); v3.55's values In / On / Yes / 1 (= drawn like
// Items: items, the Items value in force) and Out / No (= Off) are read too, *old = 1 (the caller writes the new word once).
// -1 = anything else.
static int RzR302Parse(const char *s, int items, int *old)
{
    int v = RzItemsOfText(s);
    *old = 0;
    if (v >= 0 && !RzTextIs(s, "1")) return v;  // Off, Shuffle, Random, 0, 2
    v = RzR302OfText(s);
    if (v < 0) return -1;
    *old = 1;
    return v ? (items >= RZI_OFF && items < RZI_N ? items : RZ_ITEMS_DEFAULT) : RZI_OFF;
}
// v3.56 RI356: [Randomizer] Reroll: By Room / By World (also Room / World, ByRoom / ByWorld); -1 = anything else
static int RzRerollOfText(const char *s)
{
    if (RzTextIs(s, "By Room") || RzTextIs(s, "ByRoom") || RzTextIs(s, "Room")) return RZR_ROOM;
    if (RzTextIs(s, "By World") || RzTextIs(s, "ByWorld") || RzTextIs(s, "World")) return RZR_WORLD;
    return -1;
}
// v3.56 RI356: the migrations of v3.55's keys (old value -> new value)
static int RzSpecialsOfKeep(int keep) { return keep ? 0 : 1; }                // KeepSpecials On = they stay = Specials Off
static int RzRerollOfPerVisit(int perVisit) { return perVisit ? RZR_ROOM : RZR_WORLD; }  // On = every visit; Off -> By World
// v3.56 RI356: the Items mode a stage draws with: Room 302 (stages 1 / 10) has its own row now
static int RzStageMode(int stage, int items, int r302) { return stage == 1 || stage == 0x0A ? r302 : items; }
// Seed: a decimal 1..4294967295 (spaces around allowed); 0 = missing, 0, or not a number (the caller rolls one)
static unsigned RzSeedOfText(const char *s)
{
    unsigned long long v = 0;
    int digits = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    for (; *s >= '0' && *s <= '9'; ++s) {
        v = v * 10u + (unsigned)(*s - '0');
        if (v > 0xFFFFFFFFull || ++digits > 10) return 0;
    }
    while (*s == ' ' || *s == '\t') ++s;
    return *s || !digits ? 0u : (unsigned)v;
}

// ---------------------------------------------------------------- hashes (lowbias32, as drops.inc; another constant)
#define RZ_SEED_SALT 0x52A3D348u
static unsigned RzMix(unsigned h)
{
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}
static unsigned RzHash4(unsigned a, unsigned b, unsigned c, unsigned d)
{
    unsigned h = RzMix(RZ_SEED_SALT ^ a);
    h = RzMix(h + 0x9E3779B9u + b);
    h = RzMix(h ^ (c * 0x85EBCA6Bu));
    return RzMix(h + d);
}
// a new seed from some entropy (never 0: 0 = "roll one")
static unsigned RzSeedRoll(unsigned a, unsigned b, unsigned c)
{
    const unsigned s = RzHash4(a, b, c, 0x5EEDu);
    return s ? s : 0x13579BDu;
}

// ---------------------------------------------------------------- items
#define RZ_ITEM_CANDLE 4
#define RZ_ITEM_SWORD 5
#define RZ_ITEM_MEDALLION 8
#define RZ_ITEM_N 0x36  // item ids 0..0x35 (the consumables and Henry's weapons)
static int RzIsSpecial(int item) { return item == RZ_ITEM_CANDLE || item == RZ_ITEM_SWORD || item == RZ_ITEM_MEDALLION; }
static int RzIsRoom302(int stage) { return stage == 1 || stage == 0x0A; }
static int RzIsConsumable(int item) { return item >= 1 && item <= 0x0B && item != 6 && item != 7; }
// Henry's usable weapons: 0x10 .. 0x35 without the broken golf clubs (0x16 .. 0x2C even) and the Broken Wine Bottle
// 0x2E (Eileen's 0x80 .. 0x84 are never in the pool)
static int RzIsHenryWeapon(int item)
{
    if (item < 0x10 || item > 0x35 || item == 0x2E) return 0;
    return !(item >= 0x16 && item <= 0x2C && !(item & 1));
}
static const char *RzItemName(int item)  // as drops.inc DropItemName; + the weapons (the cheat table's item list)
{
    static const char *const kW[0x26] = { "Pistol", "Richard's Revolver", "Steel Pipe", "Paper-Cutting Knife", "Aluminium Bat",
                                          "Driver", "Broken Driver", "3-Wood", "Broken 3-Wood", "3-Iron", "Broken 3-Iron",
                                          "4-Iron", "Broken 4-Iron", "5-Iron", "Broken 5-Iron", "6-Iron", "Broken 6-Iron",
                                          "7-Iron", "Broken 7-Iron", "8-Iron", "Broken 8-Iron", "9-Iron", "Broken 9-Iron",
                                          "Pitching Wedge", "Broken Pitching Wedge", "Sand Wedge", "Broken Sand Wedge",
                                          "Putter", "Broken Putter", "Wine Bottle", "Broken Wine Bottle", "Spade",
                                          "Rusty Axe", "Pickaxe of Despair", "Stun Gun", "Bug Spray", "Torch", "Chainsaw" };
    switch (item) {
    case 0x01: return "Pistol Bullets";
    case 0x02: return "Revolver Bullets";
    case 0x03: return "Silver Bullets";
    case 0x04: return "Holy Candle";
    case 0x05: return "Sword of Obedience";
    case 0x08: return "Saint Medallion";
    case 0x09: return "Nutrition Drink";
    case 0x0A: return "Ampoule";
    case 0x0B: return "Portable Medical Kit";
    }
    return item >= 0x10 && item <= 0x35 ? kW[item - 0x10] : "?";
}
// the plain-pickup rule (tools/rz_scan.py is_plain): kind 4 with the stock event-object functions, no event record of its
// own, no model / texture override, exactly one item word, an item 1..0x0B but not 6 / 7 (no model, no pickup script)
static int RzIsPlain(int kind, int funcsSet, unsigned record, unsigned model, unsigned tex, int itemWrites, unsigned item)
{
    return kind == 4 && !funcsSet && !record && !model && !tex && itemWrites == 1 && RzIsConsumable((int)item);
}
// a consumable spot in the randomized pool under the settings (keep = Keep Specials On, r302 = Room 302 In)
static int RzInPool(int stage, int item, int keep, int r302)
{
    if (RzIsRoom302(stage) && !r302) return 0;
    if (keep && RzIsSpecial(item)) return 0;
    return 1;
}
// Random's consumable draw: Supply Drops' three groups and their item weights (drops.inc kDropCommon / kDropUncommon /
// kDropRare, v3.38 DR338c: common Drink / Pistol / Revolver 1 : 1 : 1, uncommon Kit 2 : Candle 1, rare Medallion / Ampoule /
// Silver / Sword 1 each; test/rando_sim_test.c compares them with drops.inc), the groups weighted as the stock pool mixes
// them (91 common, 28 uncommon, 18 rare of 137). Keep Specials On leaves out Candle, Medallion and Sword.
typedef struct { unsigned char item, weight; } RzItemW;
static const RzItemW kRzCommon[3] = { { 0x09, 1 }, { 0x01, 1 }, { 0x02, 1 } };
static const RzItemW kRzUncommon[2] = { { 0x0B, 2 }, { 0x04, 1 } };
static const RzItemW kRzRare[4] = { { 0x08, 1 }, { 0x0A, 1 }, { 0x03, 1 }, { 0x05, 1 } };
static const int kRzGroupW[3] = { 91, 28, 18 };
// round 2: the weapons group. A plain spot draws a weapon 6 times in 143 (about 4 %: some 5-6 over the game's 137), a
// weapon spot every other time (its other half the consumable mix): each weapon of the allowed ones alike.
#define RZ_W_PLAIN 6
#define RZ_W_WSPOT 137
static int RzPickW(const RzItemW *t, int n, unsigned h, int keep)
{
    int i, total = 0, x;
    for (i = 0; i < n; ++i)
        if (!(keep && RzIsSpecial(t[i].item))) total += t[i].weight;
    if (total <= 0) return 0;
    x = (int)(h % (unsigned)total);
    for (i = 0; i < n; ++i) {
        if (keep && RzIsSpecial(t[i].item)) continue;
        if (x < t[i].weight) return t[i].item;
        x -= t[i].weight;
    }
    return 0;
}
static int RzRandomItem(unsigned key, int keep)  // a consumable (v1's draw: the same key gives v1's item)
{
    const unsigned g = RzHash4(key, 0x47525055u, 0, 0) % (unsigned)(kRzGroupW[0] + kRzGroupW[1] + kRzGroupW[2]);
    const unsigned h = RzHash4(key, 0x4954454Du, 1, 0);
    if (g < (unsigned)kRzGroupW[0]) return RzPickW(kRzCommon, 3, h, keep);
    if (g < (unsigned)(kRzGroupW[0] + kRzGroupW[1])) return RzPickW(kRzUncommon, 2, h, keep);
    return RzPickW(kRzRare, 4, h, keep);
}
// a slot's key: the seed, the stage, the room and its taken bit (the slot the save knows; both halves share it)
static unsigned RzSlotKey(unsigned seed, int stage, int room, int taken)
{
    return RzHash4(seed, (unsigned)stage, (unsigned)room, 0x534C0000u ^ (unsigned)(taken & 0xFFFF));
}
// round 2 (Change Each Visit): the key of a slot at the room's k-th visit (k = 0: v1's key)
static unsigned RzVisitKey(unsigned slotKey, unsigned k) { return k ? RzHash4(slotKey, k, 0x56495354u, 0) : slotKey; }

// ---------------------------------------------------------------- memory window, scripts, records, item tables
// [lo, hi) of memory at base (the game: .data from the pickup records to the end of the room scripts at their own
// address; the test: the exe's bytes)
typedef struct { const unsigned char *base; unsigned lo, hi; } RzWin;
#define RZ_DATA_LO 0x00618000u    // the pickup scripts / records (0x618C60..), the item tables 0x62D5E0 / 0x62DD18
#define RZ_SCRIPT_LO 0x0065540Cu  // the room scripts' object (data_map: 0x0065540C .. 0x00694B78)
#define RZ_SCRIPT_HI 0x00694B78u
static int RzRd(const RzWin *w, unsigned va, unsigned *out)
{
    if (va < w->lo || va > w->hi - 4u || (va & 3u)) return 0;
    memcpy(out, w->base + (va - w->lo), 4);
    return 1;
}
// a pickup message script of the stock shape giving `item` (see the facts above)
static int RzStdScript(const RzWin *w, unsigned p, int item)
{
    static const unsigned kShape[15] = { 0xFFFF0401u, 1, 0xFFFF0102u, 0, 0xFFFF0404u, 0x50ABE0u, 0xFFFF0201u, 0xFFFF0302u,
                                         0xFFFF0202u, 0xFFFF0301u, 0xFFFF0101u, 0, 0xFFFF0403u, 0, 0xFFFF0301u };
    unsigned v;
    int i;
    for (i = 0; i < 15; ++i) {
        if (!RzRd(w, p + 4u * (unsigned)i, &v)) return 0;
        if (i == 13 ? v != (unsigned)item : (i != 3 && i != 11 && v != kShape[i])) return 0;
    }
    return 1;
}
// an event record that adds nothing to the default record 0x6184E4 but its script: key 0, no condition, init 0x4CD5D0
// (RUN_MSG args[0]), update 0x4CD680, no late / finish / files, args[1..3] 0, args[0] a stock-shaped script giving item
static int RzSimpleRecord(const RzWin *w, unsigned rec, int item)
{
    static const unsigned kRec[11] = { 0, 0, 0x4CD5D0u, 0x4CD680u, 0, 0, 0, 0, 0, 0, 0 };
    unsigned v[11];
    int i;
    for (i = 0; i < 11; ++i) {
        if (!RzRd(w, rec + 4u * (unsigned)i, &v[i])) return 0;
        if (i != 7 && v[i] != kRec[i]) return 0;
    }
    return RzStdScript(w, v[7], item);
}
// ItemGetPickupScript's table 0x62DD18 (40 x {script, item}): the item has a stock-shaped default script
static int RzDefaultScript(const RzWin *w, int item)
{
    unsigned s, it;
    int i;
    for (i = 0; i < 40; ++i)
        if (RzRd(w, 0x62DD18u + 8u * (unsigned)i + 4u, &it) && it == (unsigned)item)
            return RzRd(w, 0x62DD18u + 8u * (unsigned)i, &s) && RzStdScript(w, s, item);
    return 0;
}
// ItemGetModelFile's table 0x62D5E0 (153 x {model, texture, item}): the file of the item's model (-1 = none)
static int RzModelFile(const RzWin *w, int item)
{
    unsigned m, it;
    int i;
    for (i = 0; i < 153; ++i)
        if (RzRd(w, 0x62D5E0u + 12u * (unsigned)i + 8u, &it) && it == (unsigned)item)
            return RzRd(w, 0x62D5E0u + 12u * (unsigned)i, &m) ? (int)(m & 0xFFFu) : -1;
    return -1;
}
// the weapons a spot may take: m57 = its model is in wp_model.bin (resident in every world with pickups), def = a
// stock-shaped default script (a spot without a record of its own can take it), rec = a simple record giving it (a
// weapon spot can take it with that record word; 0 = none found)
typedef struct { unsigned char def[RZ_ITEM_N], m57[RZ_ITEM_N]; unsigned rec[RZ_ITEM_N]; } RzWeapons;
static void RzWeaponsInit(const RzWin *w, RzWeapons *t)
{
    int i;
    memset(t, 0, sizeof *t);
    for (i = 0x10; i <= 0x35; ++i) {
        if (!RzIsHenryWeapon(i)) continue;
        t->m57[i] = RzModelFile(w, i) == 0x57;
        t->def[i] = (unsigned char)RzDefaultScript(w, i);
    }
}
// a weapon may lie at a spot: plain (no record word) needs its default script, a weapon spot a default script or a record
static int RzWeaponFits(const RzWeapons *t, int item, int weaponSpot)
{
    return RzIsHenryWeapon(item) && t->m57[item] && (t->def[item] || (weaponSpot && t->rec[item]));
}

// ---------------------------------------------------------------- the script scan
#define RZ_VMAX 64    // pickups in the pool of one script (stock: 20 plain + 7 weapon spots at most)
#define RZ_ROOMS 256
#define RZ_GUARD 400000
enum { RZK_PLAIN, RZK_WEAPON };
// va = the item word; recVa / rec = the record word and its stock value (weapon spots; 0 for plain)
typedef struct { unsigned va, recVa, rec; unsigned short room, n, taken; unsigned char item, kind; } RzSlot;
typedef struct {
    int n, nPlain, nWeapon, overflow, kind4, rooms, bad;  // bad: a read outside the window, a room >= 256, the guard
    unsigned roomStart[RZ_ROOMS];         // gRoomScripts' value for the room (0 = none): the word after its tag 0
    RzSlot s[RZ_VMAX];
} RzScan;
typedef struct {
    int kind, funcs, writes, recWrites, rec20;
    unsigned item, itemVa, model, tex, record, recVa, taken;
} RzObj;
static void RzObjFlush(const RzWin *w, const RzObj *o, int room, int n, RzScan *out)
{
    int kind = -1;
    if (o->kind == 4) ++out->kind4;
    if (RzIsPlain(o->kind, o->funcs, o->record, o->model, o->tex, o->writes, o->item)) kind = RZK_PLAIN;
    // round 2: a weapon spot: kind 4, stock functions, no model / texture override, one item word, one record word (tag
    // 0x18, not 0x20), one of Henry's weapons in wp_model.bin, its record simple (it gives that weapon)
    else if (o->kind == 4 && !o->funcs && !o->model && !o->tex && o->writes == 1 && o->recWrites == 1 && !o->rec20 &&
             RzIsHenryWeapon((int)o->item) && RzModelFile(w, (int)o->item) == 0x57 && RzSimpleRecord(w, o->record, (int)o->item))
        kind = RZK_WEAPON;
    if (kind < 0) return;
    if (out->n >= RZ_VMAX) {
        out->overflow = 1;
        return;
    }
    {
        RzSlot *s = &out->s[out->n++];
        s->va = o->itemVa, s->room = (unsigned short)room, s->n = (unsigned short)n, s->taken = (unsigned short)o->taken;
        s->item = (unsigned char)o->item, s->kind = (unsigned char)kind;
        s->recVa = kind == RZK_WEAPON ? o->recVa : 0, s->rec = kind == RZK_WEAPON ? o->record : 0;
        if (kind == RZK_WEAPON) ++out->nWeapon;
        else ++out->nPlain;
    }
}
// one room: the runner's rule from the room's start (gRoomScripts[room])
static void RzScanRoom(const RzWin *w, unsigned p, int room, RzScan *out)
{
    unsigned wd, a;
    int first = 1, guard = 0, n = 0;
    RzObj o;
    memset(&o, 0, sizeof o);
    for (;;) {
        unsigned tag, num;
        if (++guard > RZ_GUARD || !RzRd(w, p, &wd)) {
            out->bad = 1;
            return;
        }
        tag = wd & 0xFFFFu, num = wd >> 16;
        if (!first && (wd == 0xF0F0F0F0u || tag == 0)) break;
        if (tag == 1 && !first) {
            RzObjFlush(w, &o, room, n, out);
            ++n;
            memset(&o, 0, sizeof o);
        }
        switch (tag) {
        case 1: if (num >= 1 && RzRd(w, p + 4, &a)) o.kind = (int)a; break;
        case 2: case 3: case 4: case 5: case 6: case 7: o.funcs = 1; break;
        case 0x0A:
            if (num >= 4) {
                if (!RzRd(w, p + 4, &o.model) || !RzRd(w, p + 8, &o.tex) || !RzRd(w, p + 12, &o.item)) { out->bad = 1; return; }
                o.itemVa = p + 12, ++o.writes;
            }
            break;
        case 0x0B: case 0x0D: case 0x24: if (num >= 1 && !RzRd(w, p + 4, &o.model)) { out->bad = 1; return; } break;
        case 0x0C: case 0x0E: if (num >= 1 && !RzRd(w, p + 4, &o.tex)) { out->bad = 1; return; } break;
        case 0x0F: case 0x29:
            if (num >= 1) {
                if (!RzRd(w, p + 4, &o.item)) { out->bad = 1; return; }
                o.itemVa = p + 4, ++o.writes;
            }
            break;
        case 0x18:
            if (num >= 1) {
                if (!RzRd(w, p + 4, &o.record)) { out->bad = 1; return; }
                o.recVa = p + 4, ++o.recWrites;
            }
            break;
        case 0x20: if (num >= 1 && !RzRd(w, p + 4, &o.record)) { out->bad = 1; return; } o.rec20 = 1; break;
        case 0x1B: case 0x25: if (num >= 1 && RzRd(w, p + 4, &a)) o.taken = a & 0xFFFFu; break;
        }
        first = 0;
        p += 4u * (num + 1u);
    }
    RzObjFlush(w, &o, room, n, out);
}
// a stage's script from its start (0x6553A4[stage] / 0x6553D8[stage]): 0x52C4C0's room list, then every room. 1 = read
static int RzScanScript(const RzWin *w, unsigned script, RzScan *out)
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
            out->roomStart[room] = p + 8;  // (a room listed twice: the last one wins, as in 0x52C4C0)
        }
        p += 4u * ((wd >> 16) + 1u);
    }
    for (r = 0; r < RZ_ROOMS; ++r)
        if (out->roomStart[r]) RzScanRoom(w, out->roomStart[r], r, out);
    return !out->bad;
}
// round 2: the simple records the weapon spots of a scan carry (the first one of each weapon)
static void RzWeaponsAdd(RzWeapons *t, const RzScan *sc)
{
    int i;
    for (i = 0; i < sc->n; ++i)
        if (sc->s[i].kind == RZK_WEAPON && sc->s[i].item < RZ_ITEM_N && !t->rec[sc->s[i].item]) t->rec[sc->s[i].item] = sc->s[i].rec;
}

// ---------------------------------------------------------------- the stock tables (tools/rz_scan.py --header)
// cnt[i] = plain pickups of item i + 1; wn = weapon spots
typedef struct { int stage, visit; unsigned script; int n, wn; unsigned char cnt[11]; } RzVisitCount;
// stock exe md5 15a85581eca3d0664089c440b0acada1: per world visit (stage, visit; visit 1 only where the second half has
// its own script)
static const RzVisitCount kRzStock[] = {
    { 0x0, 0, 0x00655420,   0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0x1, 0, 0x0065A930,   9, 0, { 6, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 } },
    { 0x1, 1, 0x0065D214,   9, 0, { 6, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 } },
    { 0x2, 0, 0x0067EF60,   5, 7, { 4, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 } },
    { 0x2, 1, 0x00683AA0,  18, 7, { 5, 0, 1, 3, 1, 0, 0, 1, 6, 0, 1 } },
    { 0x3, 0, 0x00667D30,   3, 1, { 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1 } },
    { 0x3, 1, 0x0066C558,  17, 1, { 2, 1, 1, 3, 0, 0, 0, 1, 8, 0, 1 } },
    { 0x4, 0, 0x006888C0,   3, 0, { 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 } },
    { 0x4, 1, 0x0068E578,  16, 0, { 6, 0, 0, 3, 0, 0, 0, 2, 4, 0, 1 } },
    { 0x5, 0, 0x0065FCC0,   4, 3, { 2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 } },
    { 0x5, 1, 0x006632C8,  20, 5, { 4, 2, 0, 4, 1, 0, 0, 1, 6, 0, 2 } },
    { 0x6, 0, 0x00672090,   3, 1, { 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
    { 0x7, 0, 0x006763E0,  10, 1, { 2, 1, 0, 1, 0, 0, 0, 1, 3, 1, 1 } },
    { 0x8, 0, 0x00679FF0,  20, 1, { 4, 1, 0, 2, 1, 0, 0, 1, 8, 1, 2 } },
    { 0x9, 0, 0x00679DB0,   0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0xA, 0, 0x00688410,   0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0xB, 0, 0x0067E270,   0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};
#define RZ_STOCK_N ((int)(sizeof kRzStock / sizeof kRzStock[0]))
#define RZ_STOCK_TOTAL 137
#define RZ_STOCK_WEAPON 27
#define RZ_STAGES 13
// 1 = the scan of (stage, visit) gives the stock counts (and the stock script start); 0 = not; -1 = not in the table
static int RzStockMatch(int stage, int visit, unsigned script, const RzScan *sc)
{
    int i, k;
    for (i = 0; i < RZ_STOCK_N; ++i) {
        const RzVisitCount *v = &kRzStock[i];
        unsigned char c[11];
        if (v->stage != stage || v->visit != visit) continue;
        if (v->script != script || sc->bad || sc->overflow || sc->nPlain != v->n || sc->nWeapon != v->wn) return 0;
        memset(c, 0, sizeof c);
        for (k = 0; k < sc->n; ++k)
            if (sc->s[k].kind == RZK_PLAIN && sc->s[k].item >= 1 && sc->s[k].item <= 11) ++c[sc->s[k].item - 1];
        return memcmp(c, v->cnt, sizeof c) == 0;
    }
    return -1;
}

// ---------------------------------------------------------------- the plans
typedef struct {
    int mode, keep, r302;
    int share;                      // visit 1's shared slots take visit 0's item (Change Each Visit Off: v1's rule)
    unsigned seed, worldSalt;       // Shuffle: worldSalt = the world visit's number (0 = fixed)
    const unsigned *roomSalt;       // Random: the room's visit number by room (NULL = 0: fixed); v3.48 fix1 RZ-003: 32 bit
    unsigned roomAll;               // v3.56 RI356: Random with roomSalt NULL: every room's visit number (By World: the world visit)
    unsigned long long owned;       // bit (item - 0x10): the weapon was obtained (Random never draws it: it would not show)
    const RzWeapons *wt;            // NULL = no weapons (the weapon spots keep their weapon)
    const unsigned char *taken;     // v3.48 fix1 RZ-001: the stage's taken bits (gPickupTaken[stage]: 50 rooms x 8 bytes),
                                    // NULL = none taken
} RzPlanIn;
// v3.48 fix1 RZ-001: the game's taken bit of a slot (PickupTakenIsClear 0x4F3B30: room < 50, bit < 64; outside = taken,
// as the game answers)
#define RZ_TAKEN_ROOMS 50
#define RZ_TAKEN_SLOTS 64
static int RzSlotTaken(const RzPlanIn *in, const RzSlot *s)
{
    if (!in->taken) return 0;
    if (s->room >= RZ_TAKEN_ROOMS || s->taken >= RZ_TAKEN_SLOTS) return 1;
    return (in->taken[s->room * (RZ_TAKEN_SLOTS / 8) + s->taken / 8] >> (s->taken % 8)) & 1;
}
// v3.48 fix1 RZ-003: a visit count that never returns to an earlier value (32 bit, stops at the top)
static unsigned RzVisitNext(unsigned k) { return k == 0xFFFFFFFFu ? k : k + 1; }
typedef struct { int pooled, changed, weapons, weaponsStock, unplaced; int cnt[RZ_ITEM_N], stock[RZ_ITEM_N]; } RzPlanStat;
static void RzOrder(const RzScan *v, const int *idx, int m, int *ord)  // idx sorted by (room, n): insertion sort
{
    int i, j;
    for (i = 0; i < m; ++i) {
        const int x = idx[i];
        for (j = i; j > 0 && (v->s[ord[j - 1]].room > v->s[x].room ||
                              (v->s[ord[j - 1]].room == v->s[x].room && v->s[ord[j - 1]].n > v->s[x].n)); --j)
            ord[j] = ord[j - 1];
        ord[j] = x;
    }
}
static void RzPermute(int *a, int m, unsigned seed, unsigned what, unsigned salt)  // a seeded Fisher-Yates
{
    int k;
    for (k = m - 1; k > 0; --k) {
        const int j = (int)(RzHash4(seed, what, salt, (unsigned)k) % (unsigned)(k + 1));
        const int t = a[k];
        a[k] = a[j], a[j] = t;
    }
}
// the same save slot (room, taken bit) with the same stock item and kind in another visit's scan: its index, -1 = none
static int RzSameSlot(const RzScan *o, const RzSlot *s)
{
    int i;
    for (i = 0; o && i < o->n; ++i)
        if (o->s[i].room == s->room && o->s[i].taken == s->taken && o->s[i].item == s->item && o->s[i].kind == s->kind) return i;
    return -1;
}
// visit 1's slot is the same save slot (room, taken bit) as a pooled slot of visit 0 with the same stock item: its index
static int RzSharedSlot(const RzScan *v0, const RzSlot *s, const unsigned char *pool0)
{
    int i;
    if (!v0) return -1;
    for (i = 0; i < v0->n; ++i)
        if (pool0[i] && v0->s[i].room == s->room && v0->s[i].taken == s->taken && v0->s[i].item == s->item) return i;
    return -1;
}
// Shuffle over the slots ord[0..m): their stock items, the weapons that need a record of their own first (to weapon
// spots), then the ones with a default script, then the consumables, each in a seeded order onto the first free spot of a
// seeded spot order that may take it. Every slot gets an item it may hold (a record-needing weapon came from a weapon
// spot, so there are as many weapon spots as such weapons); the visit keeps its counts.
static int RzShufClass(const RzPlanIn *in, int it)  // 0 a weapon that needs a record, 1 a weapon with a default script, 2 else
{
    if (!RzIsHenryWeapon(it)) return 2;
    return in->wt && in->wt->def[it] ? 1 : 0;
}
static int RzShuffleSlots(const RzPlanIn *in, int stage, int visit, const RzScan *v, const int *ord, int m, unsigned char *out,
                          unsigned group)
{
    int spots[RZ_VMAX], items[RZ_VMAX], taken[RZ_VMAX], pass, k, j, unplaced = 0;
    const unsigned what = (unsigned)stage | (unsigned)visit << 8 | group << 16 | 0x53000000u;
    for (k = 0; k < m; ++k) spots[k] = ord[k], items[k] = v->s[ord[k]].item, taken[k] = 0;
    RzPermute(spots, m, in->seed, what, in->worldSalt);
    RzPermute(items, m, in->seed, what ^ 0x49540000u, in->worldSalt);
    for (pass = 0; pass < 3; ++pass)
        for (k = 0; k < m; ++k) {
            const int it = items[k];
            if (RzShufClass(in, it) != pass) continue;
            for (j = 0; j < m; ++j) {
                const RzSlot *s = &v->s[spots[j]];
                if (taken[j]) continue;
                if (RzIsHenryWeapon(it) && !(in->wt && RzWeaponFits(in->wt, it, s->kind == RZK_WEAPON))) continue;
                taken[j] = 1;
                out[spots[j]] = (unsigned char)it;
                break;
            }
            if (j == m) ++unplaced;  // (never with the stock scripts: the test checks it; the spot keeps its stock item)
        }
    return unplaced;
}
static int RzPickWeapon(const RzPlanIn *in, unsigned h, int weaponSpot, unsigned long long used)
{
    int i, n = 0, x;
    if (!in->wt) return 0;
    for (i = 0x10; i <= 0x35; ++i)
        if (RzWeaponFits(in->wt, i, weaponSpot) && !(in->owned >> (i - 0x10) & 1) && !(used >> (i - 0x10) & 1)) ++n;
    if (!n) return 0;
    x = (int)(h % (unsigned)n);
    for (i = 0x10; i <= 0x35; ++i)
        if (RzWeaponFits(in->wt, i, weaponSpot) && !(in->owned >> (i - 0x10) & 1) && !(used >> (i - 0x10) & 1) && !x--) return i;
    return 0;
}
// out[i] = the item slot i gets (its stock item when it is not changed); outRec[i] = the record word a weapon spot gets
// (its stock record when the item stays; 0 = the default record for a consumable or a weapon with a default script)
// vOther (visit 0 only, fixed Shuffle): visit 1's scan: the slots both halves share are shuffled among themselves, so
// visit 1 can take them over and still keep its own counts
static void RzPlanVisit(const RzPlanIn *in, int stage, int visit, const RzScan *v, const RzScan *v0, const unsigned char *out0,
                        const unsigned char *pool0, const RzScan *vOther, unsigned char *out, unsigned char *pool,
                        unsigned *outRec, RzPlanStat *st)
{
    int idx[RZ_VMAX], ord[RZ_VMAX], m = 0, i;
    memset(st, 0, sizeof *st);
    for (i = 0; i < v->n; ++i) {
        const RzSlot *s = &v->s[i];
        out[i] = s->item;
        outRec[i] = s->rec;
        if (in->mode == RZI_OFF) pool[i] = 0;
        else if (s->kind == RZK_WEAPON) pool[i] = in->wt != NULL && !(RzIsRoom302(stage) && !in->r302);
        else pool[i] = (unsigned char)RzInPool(stage, s->item, in->keep, in->r302);
    }
    for (i = 0; i < v->n; ++i) {
        int sh;
        if (!pool[i]) continue;
        sh = visit && in->share && in->mode == RZI_SHUFFLE ? RzSharedSlot(v0, &v->s[i], pool0) : -1;
        if (sh >= 0) out[i] = out0[sh];  // the same save slot: the item visit 0 gave it
        else idx[m++] = i;
    }
    RzOrder(v, idx, m, ord);
    if (in->mode == RZI_SHUFFLE) {
        int a[RZ_VMAX], b[RZ_VMAX], na = 0, nb = 0, k;
        for (k = 0; k < m; ++k) {
            if (vOther && RzSameSlot(vOther, &v->s[ord[k]]) >= 0) a[na++] = ord[k];
            else b[nb++] = ord[k];
        }
        st->unplaced = RzShuffleSlots(in, stage, visit, v, a, na, out, 1) + RzShuffleSlots(in, stage, visit, v, b, nb, out, 2);
    } else if (in->mode == RZI_RANDOM) {
        unsigned long long used = 0;
        int room = -1;
        for (i = 0; i < m; ++i) {
            const RzSlot *s = &v->s[ord[i]];
            const unsigned k = RzVisitKey(RzSlotKey(in->seed, stage, s->room, s->taken), in->roomSalt ? in->roomSalt[s->room] :
                                          in->roomAll);  // v3.56 RI356: + roomAll (was 0)
            const int wspot = s->kind == RZK_WEAPON;
            int it = 0;
            if (s->room != room) used = 0, room = s->room;
            // v3.48 fix1 RZ-001: a taken slot is hidden: it keeps its item and neither redraws nor reserves a weapon
            if (RzSlotTaken(in, s)) continue;
            if (in->wt && RzHash4(k, 0x57504E21u, 0, 0) % (unsigned)(137 + (wspot ? RZ_W_WSPOT : RZ_W_PLAIN)) >= 137u)
                it = RzPickWeapon(in, RzHash4(k, 0x57504E22u, 0, 0), wspot, used);  // (0: every allowed weapon owned / used)
            if (!it) it = RzRandomItem(k, in->keep);
            if (it) out[ord[i]] = (unsigned char)it;
            if (RzIsHenryWeapon(out[ord[i]])) used |= 1ull << (out[ord[i]] - 0x10);
        }
    }
    for (i = 0; i < v->n; ++i) {
        const RzSlot *s = &v->s[i];
        if (s->kind == RZK_WEAPON && out[i] != s->item)
            outRec[i] = RzIsHenryWeapon(out[i]) && in->wt && !in->wt->def[out[i]] ? in->wt->rec[out[i]] : 0;
        if (!pool[i]) continue;
        ++st->pooled;
        if (out[i] != s->item) ++st->changed;
        if (out[i] < RZ_ITEM_N) ++st->cnt[out[i]];
        if (s->item < RZ_ITEM_N) ++st->stock[s->item];
        st->weapons += RzIsHenryWeapon(out[i]);
        st->weaponsStock += RzIsHenryWeapon(s->item);
    }
}
static void RzPlanStage(const RzPlanIn *in, int stage, const RzScan *v0, const RzScan *v1, unsigned char *out0, unsigned char *pool0,
                        unsigned *rec0, RzPlanStat *st0, unsigned char *out1, unsigned char *pool1, unsigned *rec1, RzPlanStat *st1)
{
    RzPlanVisit(in, stage, 0, v0, NULL, NULL, NULL, in->share ? v1 : NULL, out0, pool0, rec0, st0);
    if (v1) RzPlanVisit(in, stage, 1, v1, v0, out0, pool0, NULL, out1, pool1, rec1, st1);
}
// patch-and-restore of one word: written only over the stock value, put back only over our own value
typedef struct { unsigned *word; unsigned stock, now; } RzPatchRec;
static int RzPatchOne(RzPatchRec *r)  // 1 = written, 0 = the word does not hold the stock value (another writer): left
{
    if (*(volatile unsigned *)r->word != r->stock) return 0;
    *(volatile unsigned *)r->word = r->now;
    return 1;
}
static int RzRestoreAll(RzPatchRec *r, int n)  // every word back, the last written first (v3.48 fix1 RZ-002: reverse order);
{                                              // returns how many no longer held our value (left as found)
    int i, other = 0;
    for (i = n - 1; i >= 0; --i) {
        if (*(volatile unsigned *)r[i].word == r[i].now) *(volatile unsigned *)r[i].word = r[i].stock;
        else ++other;
    }
    return other;
}
// v3.48 fix1 RZ-002: one list of written words per room-script run, so a run inside a run (a room built while another is
// being built) never loses the outer one's list. Push at RzBeforeScript, pop (restore) at RzAfterScript, which the
// caller runs from a __finally around the stock runner. Deeper than RZ_DEPTH: that run writes nothing.
#define RZ_DEPTH 4
#define RZ_PATCH_MAX (2 * RZ_VMAX)  // an item word and a record word a pickup at most
typedef struct { RzPatchRec rec[RZ_DEPTH][RZ_PATCH_MAX]; int n[RZ_DEPTH]; int depth; } RzPatchStack;
static int RzStackPush(RzPatchStack *s)  // the level of this run (>= RZ_DEPTH: no list, nothing written)
{
    const int d = s->depth++;
    if (d >= 0 && d < RZ_DEPTH) s->n[d] = 0;
    return d;
}
static int RzStackAdd(RzPatchStack *s, int d, unsigned *word, unsigned stock, unsigned now)  // 1 = written
{
    RzPatchRec *r;
    if (d < 0 || d >= RZ_DEPTH || s->n[d] >= RZ_PATCH_MAX) return 0;
    r = &s->rec[d][s->n[d]];
    r->word = word, r->stock = stock, r->now = now;
    if (!RzPatchOne(r)) return 0;
    ++s->n[d];
    return 1;
}
static int RzStackTrim(RzPatchStack *s, int d, int keep)  // level d's words after the first `keep` back (reverse order)
{
    int other;
    if (d < 0 || d >= RZ_DEPTH || s->n[d] <= keep) return 0;
    other = RzRestoreAll(&s->rec[d][keep], s->n[d] - keep);
    s->n[d] = keep;
    return other;
}
static int RzStackPop(RzPatchStack *s, int d)  // level d and every deeper one still open back (the deepest first)
{
    int k, other = 0;
    if (d < 0) d = 0;
    for (k = (s->depth < RZ_DEPTH ? s->depth : RZ_DEPTH) - 1; k >= d; --k) other += RzStackTrim(s, k, 0);
    s->depth = d;
    return other;
}
// which visit's script a room start belongs to (gRoomScripts[room] against the scans): 0 / 1, -1 = neither
static int RzVisitOfRoom(const RzScan *v0, const RzScan *v1, int room, unsigned start)
{
    if (room < 0 || room >= RZ_ROOMS || !start) return -1;
    if (v0 && v0->roomStart[room] == start) return 0;
    if (v1 && v1->roomStart[room] == start) return 1;
    return -1;
}

#endif
