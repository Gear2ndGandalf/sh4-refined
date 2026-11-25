// ================================================================ SUPPLY DROPS (Option 5 > Enemies; [Gameplay] SupplyDrops)
// Plan: staging/special_mods/player_cheats/p3_drops/PLAN.md (static RE, 2026-11-17 / 18); proposal: PROPOSALS.md
// section 6. Included by sh4windowmod.c after deathpresence_draw.inc; DropTick runs from Refined's 0x4EF982 dispatcher
// (quicksave.inc QsTick, game thread). GOG v7 VAs. No exe byte is patched: the feature only calls game functions and
// writes data (its own pickup records and an 8-byte ledger in the enemy manager's saved block).
//
// A monster the player kills sometimes leaves an item where it fell, through the game's own enemy-drop helper 0x463150
// (the Gum Head's: floor ray, room test, slope test, then PickupSpawnDynamic 0x4F3E90; it refuses a wall / slope / void,
// which is the safe failure). Tiers come from a gauge of the game's own stats (Normal row of the stat table 0x5B8D80:
// max HP / 100 + the strongest attack's damage / 10): weak monsters give common items now and then, strong ones the same
// plus a much lower chance of an uncommon item. The top level (the four named ghosts Cynthia, Jasper, Andrew, Richard
// and the stalker Walter) never die: every full down (class 9 / 0x0A with HP 0) rolls a rare chance of a rare item,
// one roll per down (the user, 2026-11-18: no once-per-world limit).
//
// No duplicate: every kill has an identity key, a pure hash of it (stage, the placement's character id, type, variant;
// the top level: identity, stage, world visit and the saved count of top-level downs); v3.29 rolled on it alone, so a
// reload replayed the same result (v3.30 AS330: the roll mixes in a session seed, below); the drop is a pickup record of the
// game's own pool (0x1077610, 32 x 0x80, saved whole by the game), the ledger lives in the enemy manager's saved block
// (0xFD0B98, 8 bytes no game code reads or writes: 0x463950 clears and 0x465410 / 0x464740 copy them with the kill bits),
// and a kill is only seen as a transition from a live / down class to dead (0x0B) on consecutive gameplay ticks of the
// same task: a monster restored dead, a load or a room change never fires. The pickup pool is shared with the game's
// Sword of Obedience and Gum Head returns: at least 16 records stay free, at most 6 / 8 of ours lie anywhere (the oldest
// common one in another room makes way) and 3 in one room.
//
// v3.30 AS330 (the user: "a random seed for every save load or game restart"): a kill's roll now mixes its identity key
// (the hash above, unchanged: still the record's key and the "never a second" rule) with a session seed, re-rolled at the
// game start, at every world reset (New Game / load: enemies.inc's 0x419830 hook calls DropWorldReset), at a quick load,
// and whenever the game time 0xFD5BD0 goes back or jumps ahead (any load, whichever hook is in): a reload re-rolls.
// [Gameplay] SupplyDropsSeed=Fixed (hidden) keeps v3.29's fixed rolls. The ledger's byte 0 (the top-level down count)
// keeps its meaning: it still numbers the downs inside their identity keys.
//
// v3.32 DHS332 (staging/v332_feedback/w3_drops_hud_shine/PLAN.md; the user: "players should be rewarded better for taking
// down mobs of Cynthia ghosts"): Enemy Copies' copies (ids 0x1E00nnnn, enemies.inc) drop like their type. A copy of a
// mortal monster rolls its type's common / uncommon roll on its death, a copy of a named ghost its rare roll on every
// full down, each with its own identity key (the copy id plus the world visit, never the original's). Copies never
// touch the saved ledger: their world budget (the level's budget x the copies per original) and their top-level down
// count are session-only (DropCopyLedger, reset at every load / world reset and when the world is left: a copy's own
// life). The pool rules (16 records free, 6 / 8 lying, 3 a room) hold for every drop. [Gameplay] CopyDrops=0 (hidden)
// keeps v3.31's rule (copies never drop).
//
// v3.33 EN333 (staging/v333_feedback/w6_enemies/PLAN.md; the user: "Supply Drops were not working on the Sniffers on save
// 02 bank 2"): the saves were fine (both ledgers empty); a revived monster (Revive Enemies) or a copy back in the world
// repeated its first roll for the whole session (one key per placement), and the log showed only the first 3 rolls of a
// session. Every death now has a life number (DropLives: life 0 = the old key), every roll and its chance is a verbose
// line ("no drop: roll 0.62 >= 0.25"), each failure says "no drop: <reason>", each room left gets a tally line;
// LogAnomaly for a full kill tracker, a second-visit ledger count in a first-half save, a ledger at its top.
//
// v3.35 GH335 (staging/v335_feedback/w1_ghosts/PLAN.md; the user's v3.34 QA and choices of 2026-11-20): the ghosts drop
// (every full down of a victim's ghost, the Patient and their copies rolls a lower ghost chance, halved for the same
// ghost's later downs in a world visit; the named ghosts keep their rare roll and roll the ghost chance when it misses);
// anyone's kills roll (Eileen, Echo shadows, After Shock, monsters: [Gameplay] AnyKills=0 = the player only); no world-visit
// budget any more: every drop fades away 2 minutes of gameplay after it lands, in any room (its age in its own record, so
// a save keeps it; it blinks its last 15 s), and the lying caps are 10 anywhere / 4 a room (16 records still kept free).
// [Gameplay] GhostDrops=0 = no ghost rolls, DropAge=0 = drops never fade (v3.34's budget is NOT restored by it).
//
// v3.37 MN337 (staging/v337_feedback/w1_menu/PLAN.md; the user: "raise the amount of dropped items per world to the
// highest safe limits ... it is okay to farm infinitely"): the lying caps are 64 anywhere / 10 a room for Normal and
// Generous (16 records still kept free). A pickup or a fade frees its place at once (the caps count the drops lying now).
// The hard limit is the moved pool's 128 records - 16 kept free - the game's own (about 32 at worst) = 80; with the stock
// pool ([Gameplay] PickupPool=0: 32 records) the 16 free records keep ours to 16 as before. A save still keeps 32 records
// (pickpool.inc PpSave: the game's, then our newest), so a reload brings back the newest drops only. The 12-bit drop
// number (marker word & 0xFFF: the oldest-first recycle and the save's newest-first) is renumbered 1..n in its order
// before it reaches DROP_SEQ_RENUM (DropSeqRenumber), so endless farming never wraps it. Other tables checked: the kill
// tracker (96 beings watched at once, not drops), Item Shine (64 items seen a room, 48 drawn a frame: 10 drops a room fit).
//
// v3.38 DR338 (staging/v338_feedback/w3_drops/PLAN.md; the user's v3.37 feedback 18 / 19 and answers of 2026-11-21:
// "Rare/Normal/Abundant. Replace Generous with Abundant", "all the drops under same enemy types should be equal"): four
// levels Off / Rare / Normal / Abundant (Rare = half of Normal's chances; Abundant = v3.37's Generous; a saved "Generous"
// reads as Abundant: DropIniAlias), and every item of a group has an equal share (common: Nutrition Drink / Pistol Bullets
// / Revolver Bullets, uncommon: Portable Medical Kit / Holy Candle / Silver Bullets, rare: Saint Medallion / Ampoule /
// Sword of Obedience, a third each; a gun not owned gives its bullets' share to the others). Roll keys, salts, the
// session seed and every cap (64 lying / 10 a room / 16 records free, the 2-minute fade) are as v3.37, for every level.
//
// v3.38 DR338b (the user, later the same day: "the ghosts are harder to fight than all of the monsters ... include some
// better rare drops for them but give those drops the lower rate"; "Sword of Obedience, Silver bullets, Ampoules, Saint
// Medallions are some of the rarest items ... followed by holy candles and first aid kits"; the host's design,
// USER_FEEDBACK_v337.md): groups by rarity, equal shares inside each: common Nutrition Drink / Pistol Bullets / Revolver
// Bullets (a third each), uncommon Portable Medical Kit / Holy Candle (a half each), rare Saint Medallion / Ampoule /
// Silver Bullets / Sword of Obedience (a quarter each; Silver Bullets moved from uncommon, still needing the pistol). A
// strong monster rolls a small rare chance first (Rare 0.5 %, Normal 1 %, Abundant 2 %: key draw 10), then the uncommon,
// then the common roll. A ghost's hit (the ghost chance as before) picks its group by a new key draw (11): common 70 %,
// uncommon 20 %, rare 10 %. The top level's rare roll is as before, from the 4-item rare group. A drop's marker ranks it
// by its item's group too (DropMarkTier): a rare item from a ghost or a strong monster never makes way at the lying cap.
//
// v3.38 DR338c (the user's FINAL drop design, 2026-11-21, USER_FEEDBACK_v337.md; replaces DR338b's numbers): one roll a
// kill / full down, by class, on Normal: weak c 11.45 / u 0.5 / r 0.05 %, strong 9.9 / 2 / 0.1 %, ghost (victims' ghosts,
// the Patient, their copies) 7.75 / 4 / 0.25 %, named ghost (Cynthia, Jasper, Andrew, Richard, their copies) 5 / 4 / 3 %,
// Walter 7 / 4 / 1 % (every class 12 % in all); Rare x0.5, Abundant x2. One uniform draw (key draw 12): below r rare,
// below r + u uncommon, below r + u + c common; the item by the group's own draw (common 6, uncommon 2, rare 5, as
// before). Shares: common Drink / Pistol / Revolver 1:1:1, uncommon Portable Medical Kit 2 : Holy Candle 1, rare
// Medallion / Ampoule / Silver Bullets / Sword 1:1:1:1 (the owned-gun rule as before). No falloff: every full down of a
// ghost rolls the full chance (the DR338b halving / quarter is gone; the downs are still counted for the log). The keys,
// the seed, the caps, the fade, DropMarkTier and the hidden GhostDrops / CopyDrops / AnyKills keys are as before.
//
// The pure part (gauge, tiers, rolls, items, ledger, tracker, pool plan) builds alone with DROP_PURE_ONLY defined
// (test\drops_test.c); the rest needs sh4windowmod.c.

// ---------------------------------------------------------------- pure part (no game memory)
#include "enemies_sim.h"  // v3.32 DHS332: EnmIsCopyId (the copy ids; include-guarded, already in when enemies.inc is)
// v3.38 DR338: 0 Off, 1 Rare, 2 Normal, 3 Abundant (v3.37: 0 Off, 1 Normal, 2 Generous; the ini keeps the name, never
// the number, so the new order changes no saved setting). The Options row's values (gameplay.inc kGpName[GP_DROP]) are
// these names in this order (DropsInstall checks it).
#define DROP_OFF 0
#define DROP_RARE 1
#define DROP_NORMAL 2
#define DROP_ABUNDANT 3
#define DROP_LEVELS 4          // v3.38 DR338 (was 3)
static const char *const kDropLevelName[DROP_LEVELS] = { "Off", "Rare", "Normal", "Abundant" };  // v3.38 DR338
#define DROP_TRACK_MAX 96
#ifdef DROP_PURE_ONLY  // v3.35 PK335: the native test sees the stock pool
// v3.37 MN337: the test sets the pool size (32 = the stock pool, 128 = the moved one; the 64 lying cap needs 128)
#define DROP_REC_MAX 128
static int g_dropPureRecN = 32;
#define DROP_REC_N g_dropPureRecN
#else
#define DROP_REC_N g_ppRecN    // v3.35 PK335: the records in use (pickpool.inc: 32 stock, 128 when moved)
#define DROP_REC_MAX PP_REC_MAX  // v3.35 PK335: arrays over the pool
#endif
#define DROP_IDENT_N 5         // the top level
#define DROP_STRONG_G 3.0f     // gauge at or above: strong
#define DROP_MAGIC 0x5344u     // 'SD' in the high half of the marker word (record + 0x74)
#define DROP_SEED 0x5D2F1B07u

enum { DRT_NONE, DRT_WEAK, DRT_STRONG, DRT_TOP };
#define DRT_GHOST 4           // v3.38 DR338c: moved up from the ghosts' section (DropClassOfTier needs it)
enum { DRE_NONE, DRE_KILL, DRE_DOWN };
enum { DI_CYNTHIA, DI_JASPER, DI_ANDREW, DI_RICHARD, DI_WALTER };

// v3.35 GH335: + ghost (a ghost's full down); the world budget no longer limits (kept for the log and loghealth.inc), the
// lying caps 6 / 8 anywhere and 3 a room became 10 / 4 (the drops fade after 2 minutes; 16 records stay free as before)
// v3.37 MN337: 10 / 4 -> 64 anywhere / 10 a room (both levels; 16 records still kept free)
#define DROP_LYING_CAP 64      // v3.37 MN337
#define DROP_ROOM_CAP 10       // v3.37 MN337
#define DROP_MIN_FREE 16
// v3.38 DR338c: per level only the factor on the Normal class table (mult: Rare x0.5, Normal x1, Abundant x2) and `common`
// = a roll's chance of any item on this level (every class's c + u + r: 6 / 12 / 24 %; loghealth.inc's no-drop check reads
// it); the caps are the same for every level. (DR338 / DR338b: common, uncommon, rare, ghost, strongRare per level.)
typedef struct { float common; int worldBudget, lyingCap, roomCap, minFree; float mult; } DropRates;
static const DropRates kDropRates[DROP_LEVELS] = {
    { 0.0f, 0, 0, 0, 32, 0.0f },
    { 0.06f, 4, DROP_LYING_CAP, DROP_ROOM_CAP, DROP_MIN_FREE, 0.5f },   // v3.38 DR338c Rare: x0.5 (1 roll in 16.7 gives an item)
    { 0.12f, 8, DROP_LYING_CAP, DROP_ROOM_CAP, DROP_MIN_FREE, 1.0f },   // Normal: x1 (1 in 8.3)
    { 0.24f, 15, DROP_LYING_CAP, DROP_ROOM_CAP, DROP_MIN_FREE, 2.0f },  // Abundant (v3.37 Generous): x2 (1 in 4.2)
};
// v3.38 DR338c: the classes and their Normal chances of the three groups (the user's final table)
enum { DRC_WEAK, DRC_STRONG, DRC_GHOST, DRC_NAMED, DRC_WALTER, DRC_N };
enum { DRG_COMMON, DRG_UNCOMMON, DRG_RARE, DRG_N };
static const float kDropClassNormal[DRC_N][DRG_N] = {
    { 0.1145f, 0.005f, 0.0005f },  // weak monster: common 11.45 %, uncommon 0.5 %, rare 0.05 %
    { 0.099f, 0.02f, 0.001f },     // strong monster: 9.9 / 2 / 0.1 %
    { 0.0775f, 0.04f, 0.0025f },   // ghost (victims' ghosts, the Patient, their copies): 7.75 / 4 / 0.25 %
    { 0.05f, 0.04f, 0.03f },       // named ghost (Cynthia, Jasper, Andrew, Richard, their copies): 5 / 4 / 3 %
    { 0.07f, 0.04f, 0.01f },       // Walter (the stalker): 7 / 4 / 1 %
};
static const char *const kDropClassName[DRC_N] = { "weak", "strong", "ghost", "named ghost", "Walter" };
static const char *const kDropGroupName[DRG_N] = { "common", "uncommon", "rare" };
static float DropClassChance(int level, int cls, int group)
{
    if (level <= 0 || level >= DROP_LEVELS || cls < 0 || cls >= DRC_N || group < 0 || group >= DRG_N) return 0.0f;
    return kDropClassNormal[cls][group] * kDropRates[level].mult;
}
static float DropClassAny(int level, int cls)
{
    return DropClassChance(level, cls, DRG_COMMON) + DropClassChance(level, cls, DRG_UNCOMMON) + DropClassChance(level, cls, DRG_RARE);
}
// the class a roll's tier gives (ident = the top-level identity: Walter has his own class; a top tier without one = named)
static int DropClassOfTier(int tier, int ident)
{
    if (tier == DRT_WEAK) return DRC_WEAK;
    if (tier == DRT_STRONG) return DRC_STRONG;
    if (tier == DRT_GHOST) return DRC_GHOST;
    if (tier == DRT_TOP) return ident == DI_WALTER ? DRC_WALTER : DRC_NAMED;
    return -1;
}
// v3.38 DR338: an old [Gameplay] SupplyDrops value rewritten to today's name before the lookup (gameplay.inc
// GpLegacyValue): v3.29 - v3.37's "Generous" = Abundant. Returns 1 when buf was rewritten (the caller logs it once).
static int DropIniAlias(char *buf, size_t n)
{
    if (!buf || n <= strlen(kDropLevelName[DROP_ABUNDANT]) || _stricmp(buf, "Generous")) return 0;
    strcpy_s(buf, n, kDropLevelName[DROP_ABUNDANT]);
    return 1;
}
// v3.38 DR338: the level an ini text gives, as GpReadIni / GpIniReread find it (the alias, then the names without case;
// any other text, a number included, is the first value: Off, as in every build before)
static int DropLevelOfText(const char *s)
{
    char buf[32];
    int v;
    for (v = 0; s && s[v] && v < (int)sizeof buf - 1; ++v) buf[v] = s[v];  // (as the menu's 32-byte read: cut, never overrun)
    buf[v] = 0;
    DropIniAlias(buf, sizeof buf);
    for (v = DROP_LEVELS - 1; v > 0 && _stricmp(buf, kDropLevelName[v]); --v) {}
    return v;
}
// v3.37 MN337: the caps against the tables that hold drops (compile-time): the moved pool keeps 16 free and the game's own
// (about 32 at worst) beside all of ours; a room's drops fit Item Shine's room track (items.inc SP_TRACK, 64) and its
// frame (SP_MAX, 48); the drop numbers (12 bits) are renumbered long before they could wrap (DropSeqRenumber)
typedef char DropCapsFit[DROP_LYING_CAP + DROP_MIN_FREE + 32 <= DROP_REC_MAX && DROP_ROOM_CAP <= 48 ? 1 : -1];
#ifdef SP_TRACK
typedef char DropRoomCapFitsShine[DROP_ROOM_CAP < SP_TRACK && DROP_ROOM_CAP <= SP_MAX ? 1 : -1];
#endif

// items (ids from the decomp: item.h, the pickup script and model tables 0x62DD18 / 0x62D5E0; 0x5402A1 `push 4` = the
// Holy Candle; 0x543570 / 0x4F4240 item 5 = the Sword of Obedience). All in 1..0xB, which the static pickups never hide
// once obtained (EventObjectIsEnabled), all have a pickup script and an item_model.bin / wp_model.bin model.
#define DROP_NEED_PISTOL 1
#define DROP_NEED_REVOLVER 2
typedef struct { unsigned char item, weight, need; } DropItem;
// v3.38 DR338 (the user: "all the drops under same enemy types should be equal. Getting too many health items would make
// the game unbalanced"): every item of a group weighs the same (v3.37: common 50 / 30 / 20, uncommon 40 / 35 / 25, rare
// 45 / 25 / 30). DropPick leaves out a gun not owned and shares among the rest: with one weight each, that is an equal
// share of the others (common without the revolver: Drink / Pistol Bullets a half each).
#define DROP_W 1  // v3.38 DR338: the one weight
static const DropItem kDropCommon[3] = { { 0x09, DROP_W, 0 }, { 0x01, DROP_W, DROP_NEED_PISTOL }, { 0x02, DROP_W, DROP_NEED_REVOLVER } };
// the user (2026-11-18): the Ampoule heals more than the Medical Kit, so the Kit is uncommon and the Ampoule rare
// v3.38 DR338b: groups by rarity: uncommon = Kit / Candle (a half each), rare = Medallion / Ampoule / Silver Bullets
// (the pistol) / Sword (a quarter each); Silver Bullets moved from uncommon to rare
#define DROP_N_COMMON 3
#define DROP_N_UNCOMMON 2
#define DROP_N_RARE 4
// v3.38 DR338c: the uncommon shares are Medical Kit 2 : Holy Candle 1 (the user's final design)
#define DROP_W_KIT 2
static const DropItem kDropUncommon[DROP_N_UNCOMMON] = { { 0x0B, DROP_W_KIT, 0 }, { 0x04, DROP_W, 0 } };
static const DropItem kDropRare[DROP_N_RARE] = { { 0x08, DROP_W, 0 }, { 0x0A, DROP_W, 0 }, { 0x03, DROP_W, DROP_NEED_PISTOL }, { 0x05, DROP_W, 0 } };
static const char *DropItemName(int item)
{
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
    return "?";
}
static const char *const kDropTierName[5] = { "none", "weak", "strong", "top", "ghost" };  // v3.35 GH335: + ghost
static const char *const kDropIdentName[DROP_IDENT_N] = { "Cynthia", "Jasper", "Andrew", "Richard", "Walter (stalker)" };

static unsigned DropMix(unsigned h)  // a 32-bit avalanche (lowbias32)
{
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}
static unsigned DropHash4(unsigned a, unsigned b, unsigned c, unsigned d)
{
    unsigned h = DropMix(DROP_SEED ^ a);
    h = DropMix(h + 0x9E3779B9u + b);
    h = DropMix(h ^ (c * 0x85EBCA6Bu));
    return DropMix(h + d);
}
static float DropU01(unsigned key, unsigned salt) { return (float)(DropHash4(key, salt, 0xA511E9B3u, 0) >> 8) * (1.0f / 16777216.0f); }
// the key of a monster kill: the world, the placement's character id (bit 31 = the world's second visit), type, variant
static unsigned DropKeyKill(int stage, unsigned id, int type, int variant) { return DropHash4((unsigned)stage, id, (unsigned)type, (unsigned)variant); }
// the key of a top-level being's full down: identity, world, visit and the saved count of top-level downs so far (the
// ledger's byte 0), so every down rolls anew and a reload replays the same rolls in the same order
static unsigned DropKeyTop(int ident, int stage, int visit, int down) { return DropHash4(0x70000000u | (unsigned)ident, (unsigned)stage, (unsigned)visit, 0x544F5000u ^ (unsigned)down); }

// v3.30 AS330: the roll key = the identity key mixed with this session's seed (seed 0 = Fixed: the identity key itself,
// v3.29's rolls). The identity key stays the pickup record's key (+0x78), its marker and the pool's duplicate rule.
static unsigned DropRollKey(unsigned idKey, unsigned seed) { return seed ? DropHash4(idKey, seed, 0x5EED0330u, seed >> 13) : idKey; }
// a new session seed from the old one and some entropy (never 0: 0 means Fixed)
static unsigned DropSeedMix(unsigned prev, unsigned a, unsigned b, unsigned c)
{
    const unsigned s = DropHash4(prev ^ 0xA5A5F00Du, a, b, c);
    return s ? s : 0x1234567u;
}
// what re-rolls the seed, stepped on every tick of the dispatcher: a world reset (the hook's flag), a quick load starting,
// the game time going back or jumping ahead more than DROP_SEED_JUMP ticks (a load of another save; v3.30 REV330-003, was 10 s). The re-roll waits for the
// first gameplay tick after them (`playing`), so one load gives one new seed before any kill of it is rolled. Returns
// the reasons (DSR_* bits), 0 = keep the seed. The game time only grows by one a tick while the dispatcher runs (menus,
// pauses and cut-scenes included), so play never re-rolls.
enum { DSR_RESET = 1, DSR_QUICK = 2, DSR_TIME = 4 };
#define DROP_SEED_JUMP 2  // v3.30 REV330-003: ticks the game time may advance between two dispatcher ticks without a re-roll
// v3.32 REV332-001 (inspection 1): the copies' ledger starts afresh only on a world reset or a save load, never on a
// bare game-time jump (that may only re-roll the seed), so the per-visit copy budget holds through play
static int DropCopyResetFor(int why) { return (why & (DSR_RESET | DSR_QUICK)) != 0; }
typedef struct { int have, lastTime, wasQs, pending; } DropSeedWatch;
static int DropSeedStep(DropSeedWatch *w, int time, int rate, int qsLoading, int worldReset, int playing)
{
    int why;
    if (worldReset) w->pending |= DSR_RESET;
    if (qsLoading && !w->wasQs) w->pending |= DSR_QUICK;
    w->wasQs = qsLoading != 0;
    // v3.30 REV330-003: any forward jump beyond DROP_SEED_JUMP ticks (was 10 s): the time grows by one a tick, so a load of
    // a save less than 10 s ahead re-rolls too; a false alarm only re-rolls the seed (no other effect)
    (void)rate;
    if (w->have && (time < w->lastTime || time - w->lastTime > DROP_SEED_JUMP)) w->pending |= DSR_TIME;
    w->have = 1;
    w->lastTime = time;
    if (!w->pending || !playing) return 0;
    why = w->pending;
    w->pending = 0;
    return why;
}

// the gauge: Normal max HP / 100 + the strongest attack's damage (vs the player, Normal) / 10
static float DropGauge(float hp, float dmg) { return hp / 100.0f + dmg / 10.0f; }
// monsters that die (and are worth a drop): Hummer 3, Wall Man 5, Wheelchair 6, Gum Head 7, Twin Victim 8, Tremer 9,
// Sniffer Dog 0x0A. Not 2 (Whitestool / Toadstool: 1 HP hazards the game respawns), 4 (the hanging figure: no attack),
// 1 / 0x12+ (helpers), the ghosts 0x0B-0x0F / 0x11 (they never die) and 0x10 (Walter: never killed as a stalker)
static int DropIsMortalType(int type) { return type == 3 || (type >= 5 && type <= 0x0A); }
static int DropTierOfGauge(float g) { return g >= DROP_STRONG_G ? DRT_STRONG : DRT_WEAK; }
// the top level (PLAN 4; the user, 2026-11-18): the four named victims' ghosts (unique ghost ids FFFF0002..05) and the
// stalker Walter. kindNibble = the placement's mode nibble (ObjInfo +0x14 & 0xF: 7 the Conjoined Bodies, 8 the final
// Walter: never). The other ghosts (Victims 01, 04, 10, 12, 13, 14; the Patient 0x11) and every other being: -1.
static int DropTopIdent(int type, int variant, int kindNibble)
{
    if (type == 0x0C && variant == 0) return DI_CYNTHIA;  // variant 1 = Victim 13
    if (type == 0x0D && variant == 0) return DI_JASPER;   // variant 1 = Victim 04
    if (type == 0x0E) return DI_ANDREW;
    if (type == 0x0F) return DI_RICHARD;                  // (the Patient is type 0x11: not here)
    if (type == 0x10 && kindNibble != 7 && kindNibble != 8) return DI_WALTER;
    return -1;
}
// v3.32 DHS332: what a being of the enemy list can give. DRK_KILL / DRK_KILL_COPY = a mortal monster's death (a placement
// of this world, or an Enemy Copies copy of one), DRK_TOP / DRK_TOP_COPY = a named ghost's (or Walter's) full down;
// *ident = the top-level identity (-1 for the monsters). copyDrops 0 = v3.31 (copies never drop). Never: the cheat
// table's extra copies (ids 60xxxxxx), nest spawns (id 0), any id that is neither a placement of this world nor a copy.
enum { DRK_NONE, DRK_KILL, DRK_KILL_COPY, DRK_TOP, DRK_TOP_COPY };
static int DropKindOf(int type, int variant, int kindNibble, unsigned id, int stage, int copyDrops, int *ident)
{
    const int copy = EnmIsCopyId(id) && (id & 3u) != 0;  // (k = 1..3: enemies_sim.h EnmCopyId)
    *ident = -1;
    if (!DropIsMortalType(type)) {
        const int k = DropTopIdent(type, variant, kindNibble);
        if (k < 0 || !id || (id >> 24) == 0x60) return DRK_NONE;
        if (EnmIsCopyId(id) && !(copy && copyDrops)) return DRK_NONE;  // (v3.31 EN331: copies never gave the top level)
        *ident = k;
        return copy ? DRK_TOP_COPY : DRK_TOP;
    }
    if (copy) return copyDrops ? DRK_KILL_COPY : DRK_NONE;
    if (((id >> 24) & 0x7F) != (unsigned)stage || !(id & 0xFFFFFF)) return DRK_NONE;
    return DRK_KILL;
}
// a copy's kill identity: its copy id with bit 31 = the world's second visit (as a script id's half bit). An original's
// id has its stage (2..8) in bits 24..30, a copy's 0x1E there: the two keys never meet.
static unsigned DropCopyKillId(unsigned copyId, int visit) { return (copyId & 0x7FFFFFFFu) | (visit ? 0x80000000u : 0u); }
// a copy's top-level down: identity, world, visit, its copy id and the session's count of copy downs (never saved)
static unsigned DropKeyTopCopy(int ident, int stage, int visit, unsigned copyId, int down)
{
    return DropHash4(0x71000000u | (unsigned)ident, (unsigned)stage | (unsigned)visit << 8, copyId, 0x43505900u ^ (unsigned)down);
}

// a player-side attack id (HP block +0x20, the kill credit): Henry 0x02-0x33, Eileen / Playable Eileen 0x34-0x48
// (Death Presence's last blow uses 0x08); monsters' and Walter's attacks are 0x49 and up
static int DropPlayerAttack(int id) { return id >= 0x02 && id <= 0x48; }

static int DropPick(const DropItem *t, int n, float u, int owned)
{
    int i, total = 0, acc = 0;
    for (i = 0; i < n; ++i)
        if (!(t[i].need & ~owned)) total += t[i].weight;
    if (total <= 0) return 0;
    {
        int x = (int)(u * (float)total);
        if (x >= total) x = total - 1;
        for (i = 0; i < n; ++i) {
            if (t[i].need & ~owned) continue;
            acc += t[i].weight;
            if (x < acc) return t[i].item;
        }
    }
    return 0;
}
// the item of a kill / a down, 0 = none. owned: DROP_NEED_* bits of the guns obtained. Every threshold grows with the
// level, so whatever drops on Rare also drops on Normal, and on Abundant (v3.38 DR338).
static int DropDecideClass(int level, int cls, unsigned key, int owned)  // v3.38 DR338c: one roll (draw 12) by class
{
    float u, r, n, c;
    if (level <= 0 || level >= DROP_LEVELS || cls < 0 || cls >= DRC_N) return 0;
    u = DropU01(key, 12);
    r = DropClassChance(level, cls, DRG_RARE);
    n = DropClassChance(level, cls, DRG_UNCOMMON);
    c = DropClassChance(level, cls, DRG_COMMON);
    if (u < r) return DropPick(kDropRare, DROP_N_RARE, DropU01(key, 5), owned);
    if (u < r + n) return DropPick(kDropUncommon, DROP_N_UNCOMMON, DropU01(key, 2), owned);
    if (u < r + n + c) return DropPick(kDropCommon, DROP_N_COMMON, DropU01(key, 6), owned);
    return 0;
}
// a tier's roll (DRT_TOP = a named ghost; Walter's: DropDecideClass with DRC_WALTER, DropOnEvent's DropClassOfTier)
static int DropDecide(int level, int tier, unsigned key, int owned)
{
    return DropDecideClass(level, DropClassOfTier(tier, -1), key, owned);
}

// The ledger (8 bytes, saved and loaded with the game): byte 0 = the count of top-level full downs so far (wraps; it
// only feeds their roll keys), bytes 1..7 = drops given in the worlds 2..8, a nibble per visit (low = first, high = second; at most 15).
// New game: zero (0x463950). It bounds what a world can give when its monsters come back (leaving through the hole
// wipes their records: 0x4651C0) and nothing else.
typedef struct { unsigned char b[8]; } DropLedger;
static int DropLedgerCount(const DropLedger *L, int stage, int visit)
{
    if (stage < 2 || stage > 8) return 15;
    return visit ? L->b[stage - 1] >> 4 : L->b[stage - 1] & 0xF;
}
static int DropLedgerAllowed(const DropLedger *L, int level, int tier, int stage, int visit, int ident)
{
    if (level <= 0 || level >= DROP_LEVELS || stage < 2 || stage > 8) return 0;
    if (tier == DRT_TOP) return ident >= 0 && ident < DROP_IDENT_N;  // no limit (the user): the pool caps still hold
    return DropLedgerCount(L, stage, visit) < kDropRates[level].worldBudget;
}
static void DropLedgerCommit(DropLedger *L, int tier, int stage, int visit, int ident)
{
    const int n = DropLedgerCount(L, stage, visit);
    if (stage < 2 || stage > 8) return;
    (void)ident;
    if (tier == DRT_TOP) return;  // nothing to count (DropLedgerNextDown counted the down)
    if (n < 15) {
        L->b[stage - 1] = (unsigned char)(visit ? (L->b[stage - 1] & 0x0F) | (n + 1) << 4 : (L->b[stage - 1] & 0xF0) | (n + 1));
    }
}

// the index of this top-level down (its roll key), counted in the ledger: one per down, whatever its outcome
static int DropLedgerNextDown(DropLedger *L)
{
    const int n = L->b[0];
    L->b[0] = (unsigned char)(n + 1);
    return n;
}

// v3.32 DHS332: the copies' ledger, session only (copies are never saved: enemies.inc). Their world budget = the level's
// budget x the copies made per original (per 1..3: x2 .. x4), so every copy brings its type's share, as its original
// does; counted per world visit and forgotten at a load / world reset (DropCopyLedgerReset) or when the world changes,
// which is when the copies themselves come back (their dead ring is cleared then). The top level has no budget (as the
// originals); `downs` numbers the copies' full downs for their keys and only grows (a reload keeps it, so a drop still
// lying from before the load never blocks a new down's).
typedef struct { int stage, visit, n, downs; } DropCopyLedger;
static void DropCopyLedgerReset(DropCopyLedger *c) { c->stage = -1, c->visit = 0, c->n = 0; }
static int DropCopyBudget(int level, int per)
{
    if (level <= 0 || level >= DROP_LEVELS) return 0;
    if (per < 1) per = 1;
    if (per > 3) per = 3;
    return kDropRates[level].worldBudget * per;
}
static int DropCopyAllowed(DropCopyLedger *c, int level, int tier, int stage, int visit, int per)
{
    if (level <= 0 || level >= DROP_LEVELS || stage < 2 || stage > 8) return 0;
    if (c->stage != stage || c->visit != visit) c->stage = stage, c->visit = visit, c->n = 0;  // another world visit
    if (tier == DRT_TOP) return 1;
    return c->n < DropCopyBudget(level, per);
}
static void DropCopyCommit(DropCopyLedger *c, int tier)
{
    if (tier != DRT_TOP) ++c->n;
}

// v3.33 EN333 (staging/v333_feedback/w6_enemies/PLAN.md): a monster's lives. Revive Enemies (and a world left and entered
// again, for the copies) brings a killed monster back with the same placement id, so v3.30 - v3.32 gave its every death
// the same identity key and, within one session seed, the same roll: a monster that rolled "no drop" once never dropped
// again until a load (the user's bank 2 save 02, 2026-11-20: the Sniffer in room 2:4 and its copies, revived on every
// visit). Each death now has a life number (0 for the first death after a load or a world reset, then 1, 2, ...): life 0
// keeps the v3.29 identity key (a drop lying from before a reload still blocks its twin), later lives get keys of their
// own. Session only, like the seed.
#define DROP_LIFE_N 256
// v3.33 REV333-DROP-001 (inspection 1): a full table no longer evicts (an evicted monster met again would start at life
// 0 and repeat its first roll): an identity not in the full table gets a spill life (DROP_LIFE_N + a session counter
// never reset), a fresh key every death; `spilled` counts them for the log.
typedef struct { unsigned key[DROP_LIFE_N]; unsigned char life[DROP_LIFE_N]; int n, next; unsigned spill, spilled; } DropLives;
static void DropLivesReset(DropLives *L) { L->n = 0, L->next = 0, L->spilled = 0; }
static int DropLifeNext(DropLives *L, unsigned key)  // this death's life number; counts it
{
    int i;
    for (i = 0; i < L->n; ++i)
        if (L->key[i] == key) {
            const int r = L->life[i];
            if (L->life[i] < 255) ++L->life[i];
            return r;
        }
    if (L->n >= DROP_LIFE_N) {  // full (256 monsters killed since the load): a fresh life, never life 0 again
        ++L->spilled;
        return DROP_LIFE_N + (int)(L->spill++ & 0x3FFFFFFFu);
    }
    i = L->n++;
    L->key[i] = key;
    L->life[i] = 1;
    return 0;
}
static unsigned DropLifeKey(unsigned key, int life) { return life > 0 ? DropHash4(key, (unsigned)life, 0x11FE0333u, 0x5EEDu) : key; }

// v3.33 EN333: the saved ledger's impossible state: a drop counted for a world's second visit (high nibble) while the
// save has not reached the second half (the visit bit 0x13D812D bit 2 clear). Returns a bit per world 2..8 (bit stage).
static int DropLedgerSecondVisitBad(const DropLedger *L, int visit)
{
    int s, bad = 0;
    if (visit) return 0;
    for (s = 2; s <= 8; ++s)
        if (L->b[s - 1] >> 4) bad |= 1 << s;
    return bad;
}
// v3.33 EN333: the chance the roll of a kill was measured against (the log says "roll 0.62 >= 0.25")
static float DropChance(int level, int tier, int which)  // which: 0 common, 1 uncommon, 2 rare (v3.38 DR338c: the class's)
{
    return DropClassChance(level, DropClassOfTier(tier, -1), which);
}

// The tracker: one entry per being (task, work, character id), stepped once per gameplay tick. An event needs the
// entry seen on the previous tick too (a gap, a load or a room change resets the table), so a being restored dead or
// down never fires. A monster's entry fires once (its death); a top-level being's fires on every new full down (a
// down lasts from HP 0 in class 9 / 0x0A until the class leaves 9 / 0x0A, so it is never rolled twice).
typedef struct { const void *task, *work; unsigned id; int lastCls, lastFull; unsigned lastTick, touch; int fired, used; } DropTrack;
typedef struct { DropTrack e[DROP_TRACK_MAX]; unsigned tick; } DropTable;
static void DropTableReset(DropTable *t)
{
    const unsigned tick = t->tick;
    memset(t, 0, sizeof *t);
    t->tick = tick + 2;  // nothing of before counts as "the previous tick"
}
static DropTrack *DropTrackGet(DropTable *t, const void *task, const void *work, unsigned id)
{
    int i, freeI = -1;
    for (i = 0; i < DROP_TRACK_MAX; ++i) {
        DropTrack *e = &t->e[i];
        if (e->used && e->task == task && e->work == work && e->id == id) {
            e->touch = t->tick;
            return e;
        }
        if (freeI < 0 && (!e->used || t->tick - e->touch > 1)) freeI = i;  // free, or not met since before the last tick
    }
    if (freeI < 0) return NULL;
    memset(&t->e[freeI], 0, sizeof t->e[freeI]);
    t->e[freeI].task = task;
    t->e[freeI].work = work;
    t->e[freeI].id = id;
    t->e[freeI].used = 1;
    t->e[freeI].lastTick = t->tick - 2;  // new: not seen on the previous tick
    t->e[freeI].touch = t->tick;
    return &t->e[freeI];
}
// top = a top-level being (an event on a full down), else a mortal monster (an event on its death by the player)
// v3.33 EN333: *other (may be NULL) = 1 when a mortal monster died on this tick without the player's credit (its last
// attack id is a monster's / none: a fall, another monster): no roll, but the log says why
static int DropObserve2(DropTable *t, DropTrack *e, int cls, float hp, int lastAttack, int top, int *other)
{
    const int downCls = cls == 9 || cls == 0x0A, prev = e->lastTick + 1 == t->tick;
    const int full = downCls && (!(hp > 0.0f) || (prev && e->lastFull));  // the down episode holds while it lies
    int ev = DRE_NONE;
    if (other) *other = 0;
    if (prev && !(e->fired && !top)) {
        if (!top) {
            if (e->lastCls >= 1 && e->lastCls <= 0x0A && cls == 0x0B && !(hp > 0.0f)) {
                if (DropPlayerAttack(lastAttack)) ev = DRE_KILL;
                else if (other) *other = 1, e->fired = 1;  // v3.33 EN333 (fired: said once)
            }
        } else if (full && !e->lastFull && e->lastCls >= 1 && e->lastCls <= 0x0A) {
            ev = DRE_DOWN;
        }
    }
    if (ev == DRE_KILL) e->fired = 1;
    e->lastCls = cls;
    e->lastFull = full;
    e->lastTick = t->tick;
    return ev;
}
static int DropObserve(DropTable *t, DropTrack *e, int cls, float hp, int lastAttack, int top)
{
    return DropObserve2(t, e, cls, hp, lastAttack, top, NULL);
}

// ================================================================ v3.35 GH335 (staging/v335_feedback/w1_ghosts/PLAN.md)
// ---------------------------------------------------------------- ghosts drop too
// The user (2026-11-20): "Downed ghosts should always have a chance to drop an item. The chances are lower than monster
// drops which acts as a buffer to prevent heavy farming. Copies should have drop chances too." Every full down (HP 0 in
// class 9 / 0x0A, one per down: DropObserve's down episode) of a victim's ghost (0x0B, 0x0C / 0x0D v1, FFFF00nn ids),
// the Patient 0x11 and their Enemy Copies copies rolls the ghost chance (kDropRates.ghost: Normal 5 %, Generous 10 %
// (v3.38 DR338: Rare 2.5 %, Normal 5 %, Abundant 10 %):
// about 40 % of the monsters' common chance, as a ghost can be downed again every minute or two while a monster dies
// once) for a common item. The same ghost's later downs in the same world visit roll half the chance (the 2nd) and a
// quarter (the 3rd on, the floor: it always has a chance). The four named ghosts keep their rare roll; when it gives
// nothing they roll the ghost chance too (their downs count the same way). Every pool cap holds.
// v3.38 DR338c: superseded by the final design: a ghost rolls its class (7.75 / 4 / 0.25 % on Normal) on every down
// with no falloff, a named ghost its own class (5 / 4 / 3 %) once; there is no second roll.
// DRT_GHOST 4 (after DRT_TOP: stored in the drop's marker; ranks as a weak drop when one must make way): defined with the tiers
#define DROP_GHOST_N 64
static int DropTierRank(int tier) { return tier == DRT_GHOST ? DRT_WEAK : tier; }
static int DropIsGhostKind(int type, int variant) { return EnmIsVictim(type, variant) || type == 0x11; }
static int DropTopIsGhost(int ident) { return ident >= 0 && ident != DI_WALTER; }
// DropKindOf plus the ghosts (ghostDrops 0 = v3.34: they never roll)
enum { DRK_GHOST = DRK_TOP_COPY + 1, DRK_GHOST_COPY };
static int DropKindOf2(int type, int variant, int kindNibble, unsigned id, int stage, int copyDrops, int ghostDrops, int *ident)
{
    const int k = DropKindOf(type, variant, kindNibble, id, stage, copyDrops, ident);
    const int copy = EnmIsCopyId(id) && (id & 3u) != 0;
    if (k != DRK_NONE || !ghostDrops || !DropIsGhostKind(type, variant)) return k;
    if (copy) return copyDrops ? DRK_GHOST_COPY : DRK_NONE;
    if (EnmIsGhostId(id) || (((id >> 24) & 0x7F) == (unsigned)stage && (id & 0xFFFFFF))) return DRK_GHOST;
    return DRK_NONE;  // (nest spawns, the cheat table's extras)
}
// a ghost's downs this world visit (its being key: the id, or 0x70000000 | the named identity); a new visit starts afresh
typedef struct { unsigned key; int downs, used; } DropGhostDown;
typedef struct { DropGhostDown e[DROP_GHOST_N]; int stage, visit, spill; } DropGhostDowns;
static void DropGhostDownsReset(DropGhostDowns *g)
{
    int i;
    for (i = 0; i < DROP_GHOST_N; ++i) g->e[i].used = 0;
    g->stage = -1, g->visit = 0;
}
static unsigned DropGhostBeingKey(int copy, int ident, unsigned id) { return !copy && ident >= 0 ? 0x70000000u | (unsigned)ident : id; }
static int DropGhostDownNext(DropGhostDowns *g, int stage, int visit, unsigned key)  // this down's number (0 = the first)
{
    int i, f = -1;
    if (g->stage != stage || g->visit != visit) {
        for (i = 0; i < DROP_GHOST_N; ++i) g->e[i].used = 0;
        g->stage = stage, g->visit = visit;
    }
    for (i = 0; i < DROP_GHOST_N; ++i) {
        if (g->e[i].used && g->e[i].key == key) return g->e[i].downs < 255 ? g->e[i].downs++ : 255;
        if (f < 0 && !g->e[i].used) f = i;
    }
    if (f < 0) {  // 64 ghosts downed this visit: a new one rolls the floor chance (never a full chance from a full table)
        ++g->spill;
        return 2;
    }
    g->e[f].key = key, g->e[f].downs = 1, g->e[f].used = 1;
    return 0;
}
// v3.38 DR338c: no falloff: every full down rolls the ghost class's full chance (down is kept for the callers and the log)
static float DropGhostChance(int level, int down)
{
    (void)down;
    return DropClassAny(level, DRC_GHOST);
}
static int DropDecideGhost(int level, int down, unsigned key, int owned)
{
    (void)down;
    return DropDecideClass(level, DRC_GHOST, key, owned);
}
// v3.38 DR338c: the exact chance of one item from one roll of a class (the start-up line, DROP_LIST_v338.md and the test)
static double DropGroupShare(const DropItem *t, int n, int item, int owned)
{
    int i, total = 0, w = 0;
    for (i = 0; i < n; ++i)
        if (!(t[i].need & ~owned)) total += t[i].weight, w += t[i].item == item ? t[i].weight : 0;
    return total > 0 ? (double)w / total : 0.0;
}
static double DropItemChance(int level, int cls, int item, int owned)
{
    if (level <= 0 || level >= DROP_LEVELS || cls < 0 || cls >= DRC_N) return 0.0;
    return DropClassChance(level, cls, DRG_COMMON) * DropGroupShare(kDropCommon, DROP_N_COMMON, item, owned) +
           DropClassChance(level, cls, DRG_UNCOMMON) * DropGroupShare(kDropUncommon, DROP_N_UNCOMMON, item, owned) +
           DropClassChance(level, cls, DRG_RARE) * DropGroupShare(kDropRare, DROP_N_RARE, item, owned);
}
static int DropGroupOf(int item)  // v3.38 DR338c: an item's group (-1 = none)
{
    int i;
    for (i = 0; i < DROP_N_RARE; ++i)
        if (kDropRare[i].item == item) return DRG_RARE;
    for (i = 0; i < DROP_N_UNCOMMON; ++i)
        if (kDropUncommon[i].item == item) return DRG_UNCOMMON;
    for (i = 0; i < DROP_N_COMMON; ++i)
        if (kDropCommon[i].item == item) return DRG_COMMON;
    return -1;
}
// v3.38 DR338b: the tier kept in a drop's marker (its rank when the lying cap must make way, DropPoolPlan): the higher of
// the roll's tier and its item's group (a rare-group item = DRT_TOP: never taken away; an uncommon one at least strong)
static int DropMarkTier(int tier, int item)
{
    int i;
    for (i = 0; i < DROP_N_RARE; ++i)
        if (kDropRare[i].item == item) return DRT_TOP;
    for (i = 0; i < DROP_N_UNCOMMON; ++i)
        if (kDropUncommon[i].item == item) return tier == DRT_TOP ? DRT_TOP : DRT_STRONG;
    return tier;
}

// ---------------------------------------------------------------- anyone's kills
// The user (2026-11-20): "Make it for anyone's kills even our shadows or Eileen". A mortal monster's live -> dead
// transition on two consecutive gameplay ticks rolls whoever killed it (anyone = 1): the player, companion Eileen /
// Cynthia, Echo Doubles' shadows, After Shock, another monster, a fall. What never rolls stays as before: a removal
// (the task leaves the list: a cutscene, a story despawn, the room teardown), a scripted class (>= 0x10), the restore of
// a dead record (no previous tick), the Whitestool hazards (not a mortal type), anything outside gameplay ticks.
// *other = 1 when the last attack was not the player's (the log names the source). anyone = 0: v3.34 (the player only).
static int DropObserve3(DropTable *t, DropTrack *e, int cls, float hp, int lastAttack, int top, int anyone, int *other)
{
    const int downCls = cls == 9 || cls == 0x0A, prev = e->lastTick + 1 == t->tick;
    const int full = downCls && (!(hp > 0.0f) || (prev && e->lastFull));
    int ev = DRE_NONE;
    if (other) *other = 0;
    if (!anyone) return DropObserve2(t, e, cls, hp, lastAttack, top, other);
    if (prev && !(e->fired && !top)) {
        if (!top) {
            if (e->lastCls >= 1 && e->lastCls <= 0x0A && cls == 0x0B && !(hp > 0.0f)) {
                ev = DRE_KILL;
                if (other) *other = !DropPlayerAttack(lastAttack);
            }
        } else if (full && !e->lastFull && e->lastCls >= 1 && e->lastCls <= 0x0A) {
            ev = DRE_DOWN;
        }
    }
    if (ev == DRE_KILL) e->fired = 1;
    e->lastCls = cls;
    e->lastFull = full;
    e->lastTick = t->tick;
    return ev;
}
static const char *DropCreditName(int a)  // who dealt the last blow (the HP block's attack id)
{
    if (a >= 0x02 && a <= 0x33) return "you";
    if (a >= 0x34 && a <= 0x48) return "Eileen, an Echo shadow or another player-side attack";
    if (a >= 0x49) return "a monster (or Walter)";
    return "no attack (a fall, a stun or an effect)";
}

// ---------------------------------------------------------------- every drop fades away 2 minutes after it lands
// The user (2026-11-20): no world-visit budget any more (v3.29 - v3.34: Normal 8 / Generous 15 a world visit, the copies
// that x their count); instead every drop of ours disappears DROP_AGE_S after it landed, in the loaded room and in rooms
// that are not loaded, so drops never pile up. The age runs on gameplay ticks only (the dispatcher's gameplay test: no
// menu, pause, event, movie or load), 1 / rate seconds a tick (rate = [0x5D5248], the ticks a second: frame-rate
// independent), and lives in the drop's own pickup record (+0x70, a word no game code reads for our records: only the
// Sword's returns use it, through +0x01 = 1, which ours keep 0), so a save keeps it. A record without our age tag (a
// v3.34 drop, an old save) starts a fresh 2 minutes. Its last DROP_FADE_S it blinks (its record's `active` off in part of
// each beat: the game's own EventObjectIsEnabled 0x4CF120 then disables the object for those ticks; off-phase marked by
// bit 29 so it stays ours), only in the loaded room and only while the player is not next to it.
#define DROP_AGE_S 120.0f
#define DROP_FADE_S 15.0f
#define DROP_AGE_TAG 0x40000000u  // bits 31..30 = 01: an age of ours
#define DROP_AGE_OFF 0x20000000u  // bit 29: blinking, shown off now (we cleared `active`)
#define DROP_AGE_US 0x1FFFFFFFu   // bits 0..28: the age in microseconds (up to 536 s)
static int DropAgeTagged(unsigned w70) { return (w70 & 0xC0000000u) == DROP_AGE_TAG; }
static unsigned DropAgeUs(unsigned w70) { return DropAgeTagged(w70) ? w70 & DROP_AGE_US : 0u; }  // (untagged: a fresh drop)
static unsigned DropAgeWord(unsigned us, int off) { return DROP_AGE_TAG | (off ? DROP_AGE_OFF : 0u) | (us & DROP_AGE_US); }
// one dispatcher tick: playing = a gameplay tick (else the age stands still), rate = ticks a second (1..240)
static unsigned DropAgeStep(unsigned us, int playing, int rate)
{
    const unsigned cap = (unsigned)(DROP_AGE_S * 1000000.0f) + 1000000u;
    if (!playing || rate < 1 || rate > 240) return us;
    us += 1000000u / (unsigned)rate;
    return us < cap ? us : cap;
}
static int DropAgeExpired(unsigned us) { return us >= (unsigned)(DROP_AGE_S * 1000000.0f); }
// the blink of the last DROP_FADE_S: beats of 0.5 s (0.25 s in the last 5 s), off for the last 40 % of each beat
static int DropAgeBlinkOff(unsigned us)
{
    const unsigned start = (unsigned)((DROP_AGE_S - DROP_FADE_S) * 1000000.0f), fast = (unsigned)((DROP_AGE_S - 5.0f) * 1000000.0f);
    unsigned period, t;
    if (us < start || DropAgeExpired(us)) return 0;
    period = us >= fast ? 250000u : 500000u;
    t = (us - start) % period;
    return t >= period * 6u / 10u;
}

static int DropTierRank(int tier);  // v3.35 GH335 (below)
// The pool plan over a view of the 32 records: 1 = spawn (recycle = one of ours to free after it, -1 none), 0 = not.
typedef struct { int state, active, stage, room, ours, tier, seq; unsigned key; } DropRecView;
static int DropPoolPlan(const DropRecView *v, int stage, int room, int level, unsigned key, int *recycle)
{
    const DropRates *R = &kDropRates[level > 0 && level < DROP_LEVELS ? level : 0];
    int i, freeN = 0, ours = 0, inRoom = 0, best = -1;
    *recycle = -1;
    for (i = 0; i < DROP_REC_N; ++i) {
        if (v[i].state == 0) {
            ++freeN;
            continue;
        }
        if (!v[i].ours || !v[i].active) continue;
        if (v[i].key == key) return 0;  // the same kill's drop still lies somewhere: never a second
        ++ours;
        if (v[i].stage == stage && v[i].room == room) {
            ++inRoom;
        } else if (v[i].tier != DRT_TOP && (best < 0 || DropTierRank(v[i].tier) < DropTierRank(v[best].tier) ||
                                            (DropTierRank(v[i].tier) == DropTierRank(v[best].tier) && v[i].seq < v[best].seq))) {  // v3.35 GH335: a ghost drop ranks as a weak one
            best = i;  // the least valuable, then the oldest, outside this room (no room object of its own now)
        }
    }
    if (level <= 0 || level >= DROP_LEVELS || inRoom >= R->roomCap) return 0;
    if (ours >= R->lyingCap) {  // a recycled record of ours takes the new drop: the free count stays as it is
        if (best < 0 || freeN < R->minFree) return 0;
        *recycle = best;
        return 1;
    }
    // review REV329-DROPS-001: a new drop takes one free record, so at least minFree + 1 must be free before it (with
    // exactly minFree free the spawn would leave minFree - 1)
    return freeN >= R->minFree + 1;
}
// v3.33 MERGE: why DropPoolPlan said no (the log names the real cause): the same checks in the same order. DPW_FITS = it
// would spawn. *freeN / *inRoom / *ours: the counts for the line (records free of 32, our drops in this room / lying).
enum { DPW_FITS, DPW_SAME_KILL, DPW_LEVEL, DPW_ROOM_CAP, DPW_LYING_CAP, DPW_RECORDS };
static const char *const kDropPoolWhy[] = { "it fits", "this kill's drop still lies somewhere", "Supply Drops is off",
                                            "the room's cap of drops lying here", "the cap of drops lying (none may make way)",
                                            "too few free pickup records (the game's own items keep 16 free)" };
static int DropPoolWhy(const DropRecView *v, int stage, int room, int level, unsigned key, int *freeN, int *inRoom, int *ours)
{
    const DropRates *R = &kDropRates[level > 0 && level < DROP_LEVELS ? level : 0];
    int i, best = -1, same = 0;
    *freeN = *inRoom = *ours = 0;
    for (i = 0; i < DROP_REC_N; ++i) {
        if (v[i].state == 0) {
            ++*freeN;
            continue;
        }
        if (!v[i].ours || !v[i].active) continue;
        if (v[i].key == key) same = 1;
        ++*ours;
        if (v[i].stage == stage && v[i].room == room) {
            ++*inRoom;
        } else if (v[i].tier != DRT_TOP) {
            best = i;
        }
    }
    if (same) return DPW_SAME_KILL;
    if (level <= 0 || level >= DROP_LEVELS) return DPW_LEVEL;
    if (*inRoom >= R->roomCap) return DPW_ROOM_CAP;
    if (*ours >= R->lyingCap) return best < 0 || *freeN < R->minFree ? DPW_LYING_CAP : DPW_FITS;
    return *freeN >= R->minFree + 1 ? DPW_FITS : DPW_RECORDS;
}
// v3.37 MN337: the drop number is 12 bits (marker word & 0xFFF; a new drop takes the highest lying + 1). With 64 lying and
// endless farming it would pass 4095 and wrap to 0, which reads as the oldest (the recycle would take the newest drop, the
// save would drop it first). Before the highest reaches DROP_SEQ_RENUM the lying drops are numbered 1..n again in their
// order (ties: the lower record first). newSeq[i] = the new number of record i (0 = not ours). Returns how many of ours
// were renumbered (0 = none needed).
#define DROP_SEQ_RENUM 0xF00
static int DropSeqRenumber(const DropRecView *v, int n, int *newSeq)
{
    int i, j, maxSeq = 0, ours = 0;
    for (i = 0; i < n; ++i)
        if (v[i].ours && v[i].seq > maxSeq) maxSeq = v[i].seq;
    if (maxSeq < DROP_SEQ_RENUM) return 0;
    for (i = 0; i < n; ++i) {
        int rank = 1;
        newSeq[i] = 0;
        if (!v[i].ours) continue;
        for (j = 0; j < n; ++j)
            if (j != i && v[j].ours && (v[j].seq < v[i].seq || (v[j].seq == v[i].seq && j < i))) ++rank;
        newSeq[i] = rank;
        ++ours;
    }
    return ours;
}
// the marker check word: ties the marker to the record's own content, so a slot the game reused (its alloc clears only
// the state byte) never passes for ours
static unsigned DropMarkCheck(unsigned word74, unsigned key, int item, int stage, int room, unsigned px, unsigned pz)
{
    return DropHash4(word74 ^ (unsigned)item, key, ((unsigned)stage << 16) ^ (unsigned)room, px ^ (pz * 0x27D4EB2Fu));
}

#ifndef DROP_PURE_ONLY
// ---------------------------------------------------------------- runtime (game thread)
#define DROP_LEDGER_VA 0xFD0B98u   // EnemyManager +0x1B98 (unk_1B98[8]), inside the 0x1B90 bytes saved from 0xFCF010
#define DROP_REC_VA g_ppRecVA      // v3.35 PK335: the pool in use (0x1077610 stock, else pickpool.inc's g_ppBuf)
static DropTable g_dropTab;
static int g_dropInstalled;
static volatile int g_dropFail;
static int g_dropWasOn;
static DWORD g_dropRoomKey = 0xFFFFFFFFu;
static unsigned char g_dropTier[0x13][8];   // tier by type and stat variant (from the exe's tables at install)
static float g_dropG[0x13][8];
static int g_dropLogged[16];
static DWORD g_dropFaultCode;
static void *g_dropFaultAt;
static unsigned g_dropSeed;           // v3.30 AS330: this session's roll seed (0 = [Gameplay] SupplyDropsSeed=Fixed: v3.29)
static volatile LONG g_dropWorldReset;  // set by enemies.inc's world-reset hook (0x419830, return 0x414A1A)
static DropSeedWatch g_dropSeedW;
static int g_dropSeedN;               // re-rolls this session (the log)
static int g_dropCopyDrops = 1;       // v3.32 DHS332: [Gameplay] CopyDrops (hidden, default 1; 0 = v3.31: copies never drop)
static DropCopyLedger g_dropCopyL;    // v3.32 DHS332: the copies' session ledger
static int g_dropCopyN;               // v3.32 DHS332: copy drops given this session (the log)
// v3.33 EN333: lives (Revive Enemies), the verbose roll log and the per-room tally
static DropLives g_dropLives;
static volatile LONG g_dropQuotaRoll, g_dropQuotaWhy, g_dropQuotaAnom, g_dropQuotaRoom;
typedef struct { int kills, drops, chance, budget, pool, model, place, other, copies; } DropRoomTally;
static DropRoomTally g_dropRoomT;
static DWORD g_dropTallyKey = 0xFFFFFFFFu;
// v3.35 GH335
static int g_dropGhostDrops = 1;      // [Gameplay] GhostDrops (hidden, default 1; 0 = v3.34: ghosts never roll)
static int g_dropAnyKills = 1;        // [Gameplay] AnyKills (hidden, default 1; 0 = v3.34: only the player's kills roll)
static int g_dropAgeOn = 1;           // [Gameplay] DropAge (hidden, default 1; 0 = drops never fade)
static DropGhostDowns g_dropGhostD;   // the ghosts' downs this world visit (session only)
static int g_dropFaded;               // drops faded away this session (the log)
static volatile LONG g_dropQuotaAge;
static void DropWorldReset(void) { InterlockedExchange(&g_dropWorldReset, 1); }  // v3.30 AS330 (any thread)
static unsigned DropEntropy(unsigned prev)
{
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return DropSeedMix(prev, (unsigned)q.LowPart ^ (unsigned)__rdtsc(), (unsigned)q.HighPart ^ GetTickCount(),
                       GetCurrentProcessId() ^ (unsigned)(ULONG_PTR)&q);
}
static void DropSeedTick(int rate, int playing)  // v3.30 AS330: every dispatcher tick, before the level check
{
    // v3.30 DROPS-001 (inspection 3): a load-specific signal: the game's load menu (and Refined's quick load) set
    // [0xFD5A74] = 1 and the world start clears it once the save image is applied (quicksave.inc), so its rising edge is
    // every save load, whatever the saved game time
    const int why = DropSeedStep(&g_dropSeedW, *(volatile int *)0xFD5BD0, rate, g_qsLoading || *(volatile int *)0xFD5A74 != 0,
                                 InterlockedExchange(&g_dropWorldReset, 0) != 0, playing);
    if (DropCopyResetFor(why)) DropCopyLedgerReset(&g_dropCopyL), DropGhostDownsReset(&g_dropGhostD);  // v3.32 DHS332 / REV332-001: a load / world reset only; v3.35 GH335: + the ghosts' downs
    if (why) {  // v3.33 EN333: a load or a world reset: lives count from 0 again; the loaded ledger is checked
        const int visit = (*(volatile const BYTE *)0x13D812D >> 2) & 1, bad = DropLedgerSecondVisitBad((const DropLedger *)(ULONG_PTR)DROP_LEDGER_VA, visit);
        const BYTE *b = (const BYTE *)(ULONG_PTR)DROP_LEDGER_VA;
        DropLivesReset(&g_dropLives);
        LogV("supply drops: the saved ledger after the load: %02X %02X %02X %02X %02X %02X %02X %02X (top-level downs %d; world 2..8 "
             "drops first / second visit: %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d, counted only: no world budget since v3.35, "
             "drops fade %.0f s after they land)", b[0], b[1],
             b[2], b[3], b[4], b[5], b[6], b[7], b[0], b[1] & 15, b[1] >> 4, b[2] & 15, b[2] >> 4, b[3] & 15, b[3] >> 4, b[4] & 15,
             b[4] >> 4, b[5] & 15, b[5] >> 4, b[6] & 15, b[6] >> 4, b[7] & 15, b[7] >> 4, DROP_AGE_S);  // v3.35 GH335
        if (bad && LogQuota(&g_dropQuotaAnom, 20))
            LogAnomaly("supply drops", "the saved ledger counts second-visit drops (worlds mask %03X) in a save of the first half "
                       "(visit bit clear): a save edited or written by another build?", bad);
    }
    if (!why || !g_dropSeed) return;  // (Fixed: never)
    g_dropSeed = DropEntropy(g_dropSeed);
    if (++g_dropSeedN <= 40)
        Log("supply drops: a new roll seed %08X (%s%s%s; game time %d): this session's kills roll anew", g_dropSeed,
            why & DSR_RESET ? "world reset " : "", why & DSR_QUICK ? "load (menu or quick) " : "", why & DSR_TIME ? "game time jump" : "",
            g_dropSeedW.lastTime);
}
static int DropFaultNote(EXCEPTION_POINTERS *e)
{
    g_dropFaultCode = e->ExceptionRecord->ExceptionCode;
    g_dropFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}

// the stat table row of (type, variant, level) as 0x468DC0 indexes it (type 0x11 = 0x0F variant 1); NULL when none
static const float *DropStatRow(int type, int variant, int level)
{
    const int *counts = (const int *)0x5B8BA0;
    int i, row = 0;
    if (type == 0x11) type = 0x0F, variant = 1;
    if (type < 0 || type >= 0x13 || variant < 0 || variant >= counts[type] || level < 0 || level > 4) return NULL;
    for (i = 0; i < type; ++i) row += counts[i] * 5;
    return (const float *)(ULONG_PTR)(0x5B8D80 + 0x28 * (row + variant * 5 + level));
}
// the strongest attack of a mortal type at Normal: its attack ids (table damage 0x63D530) and stat kinds (0x469030:
// kind 1 = +0x14, 2 = +0x18, 3 = +0x1C). Sources: the EnemyEmitAttack calls of each type's file (PLAN 3).
static float DropTypeDamage(int type, const float *row)
{
    static const struct { int type; signed char kind; unsigned char id; } kSrc[] = {
        { 3, 0, 0x4B }, { 3, 1, 0x4A }, { 3, 3, 0x4C },                                   // Hummer
        { 5, 0, 0x4D }, { 5, 0, 0x4E }, { 5, 0, 0x4F }, { 5, 0, 0x50 },                   // Wall Man
        { 6, 1, 0x51 },                                                                   // Wheelchair
        { 7, 0, 0x52 }, { 7, 0, 0x53 }, { 7, 0, 0x54 }, { 7, 0, 0x55 },                   // Gum Head (kind + 0x52)
        { 8, 0, 0x56 }, { 8, 0, 0x57 }, { 8, 0, 0x58 },                                   // Twin Victim
        { 9, 1, 0x5A }, { 9, 2, 0x59 },                                                   // Tremer
        { 0x0A, 1, 0x5B },                                                                // Sniffer Dog (kind 5 = +0x14)
    };
    float best = 0.0f;
    int i;
    for (i = 0; i < (int)(sizeof kSrc / sizeof kSrc[0]); ++i) {
        float d;
        if (kSrc[i].type != type) continue;
        d = kSrc[i].kind ? (row ? row[4 + kSrc[i].kind] : 0.0f) : ((const float *)(ULONG_PTR)0x63D530)[2 * kSrc[i].id];
        if (_finite(d) && d > best) best = d;
    }
    return best;
}
static void DropBuildTiers(void)
{
    char line[512];
    int type, v, n = 0;
    memset(g_dropTier, 0, sizeof g_dropTier);
    line[0] = 0;
    for (type = 0; type < 0x13; ++type) {
        const int count = ((const int *)0x5B8BA0)[type];
        if (!DropIsMortalType(type)) continue;
        for (v = 0; v < count && v < 8; ++v) {
            const float *row = DropStatRow(type, v, 2);  // Normal
            const float hp = row && _finite(row[0]) ? row[0] : 0.0f, dmg = DropTypeDamage(type, row), g = DropGauge(hp, dmg);
            g_dropG[type][v] = g;
            g_dropTier[type][v] = (unsigned char)(hp > 1.0f ? DropTierOfGauge(g) : DRT_NONE);
            n += sprintf_s(line + n, sizeof line - n, "%s%02X/%d %.0f+%.0f=%.1f %s", n ? ", " : "", type, v, hp, dmg, g,
                           kDropTierName[g_dropTier[type][v]]);
            if (n > (int)sizeof line - 48) break;
        }
    }
    Log("supply drops: gauges (type/variant hp+damage=gauge tier, Normal row): %s", line);
}
// the stat variant of a live being: types whose create stores it (0x45B730 -> work+0x40: 5, 7, 8, 0xB-0xD), else the
// variant whose max HP at the current difficulty (0x468E00) is the being's
static int DropVariantOf(int type, const BYTE *w, float hpMax)
{
    const int count = ((const int *)0x5B8BA0)[type < 0x13 ? type : 0];
    int v;
    if (type == 5 || type == 7 || type == 8 || (type >= 0x0B && type <= 0x0D)) {
        v = *(const int *)(w + 0x40);
        return v >= 0 && v < count && v < 8 ? v : 0;
    }
    for (v = 0; v < count && v < 8; ++v)
        if (((float(__cdecl *)(int, int))0x468E00)(type, v) == hpMax) return v;
    return 0;
}

static BYTE *DropRec(int i) { return (BYTE *)(ULONG_PTR)(DROP_REC_VA + 0x80u * (unsigned)i); }
static int DropRecOurs(const BYTE *r)
{
    const unsigned w74 = *(const unsigned *)(r + 0x74);
    // v3.35 GH335: a blinking drop shown off (`active` cleared by DropAgeTick, bit 29 of its age word) is still ours
    if (!r[0] || (!*(const short *)(r + 2) && !(DropAgeTagged(*(const unsigned *)(r + 0x70)) &&
                                                (*(const unsigned *)(r + 0x70) & DROP_AGE_OFF))) || (w74 >> 16) != DROP_MAGIC) return 0;
    return *(const unsigned *)(r + 0x7C) == DropMarkCheck(w74, *(const unsigned *)(r + 0x78), *(const int *)(r + 0x38),
                                                          *(const short *)(r + 6), *(const short *)(r + 8),
                                                          *(const unsigned *)(r + 0x10), *(const unsigned *)(r + 0x18));
}
static void DropRecViews(DropRecView *v, int *maxSeq)
{
    int i;
    *maxSeq = 0;
    for (i = 0; i < DROP_REC_N; ++i) {
        const BYTE *r = DropRec(i);
        const unsigned w74 = *(const unsigned *)(r + 0x74);
        v[i].state = r[0];
        v[i].active = *(const short *)(r + 2);
        v[i].stage = *(const short *)(r + 6);
        v[i].room = *(const short *)(r + 8);
        v[i].ours = DropRecOurs(r);
        if (v[i].ours) v[i].active = 1;  // v3.35 GH335: (blinking off: still lying)
        v[i].tier = v[i].ours ? (int)(w74 >> 12 & 0xF) : 0;
        v[i].seq = v[i].ours ? (int)(w74 & 0xFFF) : 0;
        v[i].key = v[i].ours ? *(const unsigned *)(r + 0x78) : 0;
        if (v[i].ours && v[i].seq > *maxSeq) *maxSeq = v[i].seq;
    }
}
// v3.37 MN337: renumber the lying drops 1..n (DropSeqRenumber) when the highest number nears the 12-bit top: each record's
// marker word gets its new number and its check word is made again over it (DropRecOurs still knows it); the views are
// read again. Game thread (DropOnEvent).
static void DropSeqCompact(DropRecView *v, int *maxSeq)
{
    static volatile LONG s_q;
    int newSeq[DROP_REC_MAX], i, n, bad = 0;
    const int before = *maxSeq;
    if ((n = DropSeqRenumber(v, DROP_REC_N, newSeq)) <= 0) return;
    for (i = 0; i < DROP_REC_N; ++i) {
        BYTE *r;
        unsigned w74;
        if (!v[i].ours || newSeq[i] <= 0) continue;
        r = DropRec(i);
        w74 = (*(const unsigned *)(r + 0x74) & ~0xFFFu) | ((unsigned)newSeq[i] & 0xFFFu);
        *(unsigned *)(r + 0x74) = w74;
        *(unsigned *)(r + 0x7C) = DropMarkCheck(w74, *(const unsigned *)(r + 0x78), *(const int *)(r + 0x38),
                                                *(const short *)(r + 6), *(const short *)(r + 8),
                                                *(const unsigned *)(r + 0x10), *(const unsigned *)(r + 0x18));
    }
    DropRecViews(v, maxSeq);
    for (i = 0; i < DROP_REC_N; ++i) bad += newSeq[i] > 0 && !v[i].ours;
    LogV("supply drops: drop numbers renumbered 1..%d (the highest was %d of 4095; their order is kept)", n, before);
    if ((bad || *maxSeq != n) && LogQuota(&s_q, 5))
        LogAnomaly("supply drops", "the renumbered drops do not read back (%d lost their marker, highest %d of %d)", bad, *maxSeq, n);
}
// the floor under p (EnemyFloorRayCheck 0x459490 from 60 units above, as the Gum Head's drop 0x495510), within 400 below
static void DropFloor(const float *p, float *out)
{
    __declspec(align(16)) float f[12];  // MoverFloor: pos, normal, dist, hit, poly, pad
    __declspec(align(16)) float d[4];
    d[0] = p[0], d[1] = p[1] - 60.0f, d[2] = p[2], d[3] = 1.0f;
    out[0] = p[0], out[1] = p[1], out[2] = p[2], out[3] = 1.0f;
    memset(f, 0, sizeof f);
    if (((int(__cdecl *)(float *, float *))0x459490)(f, d) && _finite(f[1]) && f[1] < p[1] + 400.0f && f[1] > p[1] - 400.0f) {
        out[0] = f[0], out[1] = f[1], out[2] = f[2];
    }
}
// spawn through 0x463150 at p (on its floor); 1 and the record index on success
static int DropSpawnAt(int item, const float *p, float yaw, int *recI)
{
    __declspec(align(16)) float at[4];
    __declspec(align(16)) float q[4];
    BYTE before[DROP_REC_MAX];  // v3.35 PK335
    int i, ok;
    DropFloor(p, at);
    q[0] = 0.0f, q[1] = sinf(yaw * 0.5f), q[2] = 0.0f, q[3] = cosf(yaw * 0.5f);  // a turn about y
    for (i = 0; i < DROP_REC_N; ++i) before[i] = DropRec(i)[0];
    PpPreferHigh(1);  // v3.35 PK335: slots 32 and up first (the game's own records keep the 32 a save holds)
    ok = ((int(__cdecl *)(int, float *, float *))0x463150)(item, at, q);
    PpPreferHigh(0);  // v3.35 PK335
    if (!ok) return 0;
    for (i = 0; i < DROP_REC_N; ++i)
        if (!before[i] && DropRec(i)[0] && *(const int *)(DropRec(i) + 0x38) == item) {
            *recI = i;
            return 1;
        }
    if (!g_dropLogged[5]++) Log("supply drops: 00463150 reported a drop but no new record was found");
    return 0;
}

static void DropOnEvent(const BYTE *t, const BYTE *w, int type, int variant, int ident, const float *hp, int stage,
                        int room, unsigned id, int level, int copy, int ghost)  // v3.32 DHS332: + copy (an Enemy Copies copy); v3.35 GH335: + ghost
{
    DropLedger *const L = (DropLedger *)(ULONG_PTR)DROP_LEDGER_VA;
    int tier = ident >= 0 ? DRT_TOP : ghost ? DRT_GHOST : (int)g_dropTier[type][variant];  // v3.35 GH335: + ghost
    const int visit = (*(volatile const BYTE *)0x13D812D >> 2) & 1;  // FlagsGetApartmentWorldCompleted (0x4FC830)
    const int per = (int)g_enmCopies;  // v3.32 DHS332: copies per original (1..3 = x2..x4) for the copies' budget
    // v3.32 DHS332: a copy's key is its own (copy id + visit; its downs counted in the session ledger, never the saved one)
    const unsigned key0 = copy ? (ident >= 0 ? DropKeyTopCopy(ident, stage, visit, id, g_dropCopyL.downs++)
                                             : DropKeyKill(stage, DropCopyKillId(id, visit), type, variant))
                               : ident >= 0 ? DropKeyTop(ident, stage, visit, DropLedgerNextDown(L)) : DropKeyKill(stage, id, type, variant);
    // v3.33 EN333: a mortal monster's every life rolls anew (Revive Enemies; copies back after the world was left)
    const int life = ident < 0 ? DropLifeNext(&g_dropLives, key0) : 0;
    if (life >= DROP_LIFE_N && g_dropLives.spilled == 1)  // v3.33 REV333-DROP-001: once per load
        LogV("supply drops: 256 monsters killed since the load: later monsters roll with a fresh key per death (no first-roll repeat)");
    const unsigned key = DropLifeKey(key0, life);
    const unsigned roll = DropRollKey(key, g_dropSeed);  // v3.30 AS330: the identity key x this session's seed
    const BYTE *obtained = (const BYTE *)0x13D7FF8;  // gInventory.obtained (0x13D7B60 + 0x498)
    const int owned = (obtained[0x10] ? DROP_NEED_PISTOL : 0) | (obtained[0x11] ? DROP_NEED_REVOLVER : 0);
    // v3.35 GH335: a ghost's down number this world visit (v3.38 DR338c: for the log only; every down rolls the full chance)
    const int gdown = tier == DRT_GHOST || (g_dropGhostDrops && DropTopIsGhost(ident))
                          ? DropGhostDownNext(&g_dropGhostD, stage, visit, DropGhostBeingKey(copy, ident, id))
                          : -1;
    const int cls = DropClassOfTier(tier, ident);  // v3.38 DR338c: one roll by class
    const int item = DropDecideClass(level, cls, roll, owned);
    DropRecView v[DROP_REC_MAX];  // v3.35 PK335
    int recycle, maxSeq, recI = -1, model, tex, ok;
    float pos[3], yaw;
    (void)t;
    (void)hp;
    if (tier == DRT_NONE || cls < 0) return;
    ++g_dropRoomT.kills, g_dropRoomT.copies += copy != 0;  // v3.33 EN333: the room's tally
    if (!item) {
        // v3.33 EN333: every roll is in the verbose log with its chance (v3.32 logged only the first 3 of a session, so the
        // user's save 02 kills were silent: "no drop" looked like "not working"); v3.38 DR338c: the class's one roll
        ++g_dropRoomT.chance;
        if (g_dropLogged[0]++ < 3 || LogQuota(&g_dropQuotaRoll, 400)) {
            char dn[48];
            if (gdown >= 0) sprintf_s(dn, sizeof dn, "down %d of it this world visit", gdown + 1);
            else sprintf_s(dn, sizeof dn, "life %d", life);
            LogV("supply drops: no drop: %s%s %02X/%d (%s) in room %d:%d, %s: roll %.4f >= %.4f (the %s chance at %s: rare %.4f, "
                 "uncommon %.4f, common %.4f); key %08X", copy ? "a copy of " : "", ident >= 0 ? kDropIdentName[ident] : gdown >= 0 ? "ghost" : "type",
                 type, variant, kDropTierName[tier], stage, room, dn, DropU01(roll, 12), DropClassAny(level, cls), kDropClassName[cls],
                 kGpName[GP_DROP][level], DropClassChance(level, cls, DRG_RARE), DropClassChance(level, cls, DRG_UNCOMMON),
                 DropClassChance(level, cls, DRG_COMMON), key);
        }
        return;
    }
    // v3.35 GH335: no world-visit budget any more (the drops fade after 2 minutes): only Off / outside the worlds refuse
    if (g_dropAgeOn ? level <= 0 || level >= DROP_LEVELS || stage < 2 || stage > 8
                    : copy ? !DropCopyAllowed(&g_dropCopyL, level, tier, stage, visit, per) : !DropLedgerAllowed(L, level, tier, stage, visit, ident)) {
        ++g_dropRoomT.budget;  // v3.33 EN333
        if (g_dropLogged[copy ? 6 : 1]++ < 4 || LogQuota(&g_dropQuotaWhy, 100))  // v3.32 DHS332: the copies' own budget
            Log(copy ? "supply drops: no drop: %s won but this world visit's copy drops are used up (%d)"
                     : "supply drops: no drop: %s won but this world visit's drops are used up (%d, the saved ledger)",
                DropItemName(item), copy ? DropCopyBudget(level, per) : kDropRates[level].worldBudget);
        return;
    }
    DropRecViews(v, &maxSeq);
    if (maxSeq >= DROP_SEQ_RENUM) DropSeqCompact(v, &maxSeq);  // v3.37 MN337: the 12-bit drop number never wraps
    if (!DropPoolPlan(v, stage, room, level, key, &recycle)) {
        int fr, inR, ours;  // v3.33 MERGE: the real cause and the counts (was "too full (or 3 lie in this room)")
        const int why = DropPoolWhy(v, stage, room, level, key, &fr, &inR, &ours);
        ++g_dropRoomT.pool;  // v3.33 EN333
        if (g_dropLogged[2]++ < 4 || LogQuota(&g_dropQuotaWhy, 100))
            Log("supply drops: no drop: %s won but %s (room %d:%d: %d of ours lie here, cap %d; %d lying, cap %d; pickup records "
                "free %d of %d, a new drop needs %d)", DropItemName(item), kDropPoolWhy[why > 0 && why <= DPW_RECORDS ? why : 0],
                stage, room, inR, kDropRates[level].roomCap, ours, kDropRates[level].lyingCap, fr, DROP_REC_N,
                kDropRates[level].minFree + 1);
        if (why == DPW_FITS) {
            static volatile LONG s_qa;
            if (LogQuota(&s_qa, 5))
                LogAnomaly("supply drops", "the pool plan refused a drop the reason check allows (room %d:%d, free %d, here %d, "
                           "lying %d)", stage, room, fr, inR, ours);
        }
        return;
    }
    model = ((int(__cdecl *)(int))0x4F4710)(item);
    tex = ((int(__cdecl *)(int))0x4F4750)(item);
    if (!model || !tex || model == -1 || tex == -1 || !((void *(__cdecl *)(unsigned))0x5736D0)((unsigned)model) ||
        !((void *(__cdecl *)(unsigned))0x5736D0)((unsigned)tex)) {
        ++g_dropRoomT.model;  // v3.33 EN333
        if (g_dropLogged[3]++ < 4 || LogQuota(&g_dropQuotaWhy, 100))
            Log("supply drops: no drop: %s won but its model is not loaded here (%08X / %08X)", DropItemName(item), model, tex);
        return;
    }
    memcpy(pos, w, sizeof pos);  // the work position (KB 138)
    if (!_finite(pos[0]) || !_finite(pos[1]) || !_finite(pos[2])) return;
    yaw = DropU01(roll, 7) * 6.2831853f;
    ok = DropSpawnAt(item, pos, yaw, &recI);
    if (!ok) {  // a wall / slope / void under the body: next to the player instead, on the body's side
        const float *pp = (const float *)0x10A0F60;
        float dx = pos[0] - pp[0], dz = pos[2] - pp[2], d = sqrtf(dx * dx + dz * dz), p2[3];
        if (d > 1.0f) dx /= d, dz /= d;
        else dx = 0.0f, dz = 1.0f;
        p2[0] = pp[0] + dx * 60.0f, p2[1] = pp[1], p2[2] = pp[2] + dz * 60.0f;
        if (_finite(pp[0]) && _finite(pp[1]) && _finite(pp[2])) ok = DropSpawnAt(item, p2, yaw, &recI);
    }
    if (!ok) {
        ++g_dropRoomT.place;  // v3.33 EN333
        if (g_dropLogged[4]++ < 4 || LogQuota(&g_dropQuotaWhy, 100))
            Log("supply drops: no drop: %s won but 00463150 refused the place (wall, slope or no floor)", DropItemName(item));
        return;
    }
    ++g_dropRoomT.drops;  // v3.33 EN333
    {
        BYTE *r = DropRec(recI);
        const unsigned w74 = DROP_MAGIC << 16 | (unsigned)(DropMarkTier(tier, item) & 0xF) << 12 | (unsigned)((maxSeq + 1) & 0xFFF);  // v3.38 DR338b: DropMarkTier
        r[0] = 1;                      // lies in its room (0x463150's 0x4F4120 made it 2: moved to the return list at exit)
        r[1] = 0;                      // not a pinned ghost's Sword (0x4F4190 moves those; the alloc leaves old values)
        *(unsigned *)(r + 0x70) = DropAgeWord(0, 0);  // v3.35 GH335: its age (0 s; DropAgeTick)
        *(unsigned *)(r + 0x74) = w74;
        *(unsigned *)(r + 0x78) = key;
        *(unsigned *)(r + 0x7C) = DropMarkCheck(w74, key, item, *(const short *)(r + 6), *(const short *)(r + 8),
                                                *(const unsigned *)(r + 0x10), *(const unsigned *)(r + 0x18));
    }
    if (recycle >= 0 && recycle != recI) {  // the oldest least valuable of ours elsewhere makes way (PickupRecordClear)
        BYTE *r = DropRec(recycle);
        Log("supply drops: the cap of %d lying drops: %s in stage %d room %d is taken away", kDropRates[level].lyingCap,
            DropItemName(*(const int *)(r + 0x38)), *(const short *)(r + 6), *(const short *)(r + 8));
        r[0] = 0;
        *(short *)(r + 2) = 0;
        *(unsigned *)(r + 0x74) = 0;
    }
    if (copy) {  // v3.32 DHS332: the session ledger only (nothing saved changes for a copy)
        DropCopyCommit(&g_dropCopyL, tier);
        ++g_dropCopyN;
        if (tier == DRT_GHOST)  // v3.35 GH335
            Log("supply drops: %s from a copy of %s %02X/%d (copy %08X, its down %d this world visit, %s of the ghost chance %.4f) in "
                "stage %d room %d at %.0f %.0f %.0f (record %d; it fades in %.0f s)", DropItemName(item),
                ident >= 0 ? kDropIdentName[ident] : "ghost", type, variant, id, gdown + 1, kDropGroupName[DropGroupOf(item) >= 0 ? DropGroupOf(item) : 0],
                DropClassAny(level, cls), stage, room, pos[0], pos[1], pos[2], recI, DROP_AGE_S);  // v3.38 DR338c
        else if (ident >= 0)
            Log("supply drops: %s from a copy of %s (copy %08X, stage %d, visit %d, copy down %d; %s, the %s class) at %.0f %.0f %.0f (record %d)",
                DropItemName(item), kDropIdentName[ident], id, stage, visit, g_dropCopyL.downs - 1,
                kDropGroupName[DropGroupOf(item) >= 0 ? DropGroupOf(item) : 0], kDropClassName[cls], pos[0], pos[1], pos[2], recI);  // v3.38 DR338c
        else
            Log("supply drops: %s from a copy of type %02X/%d (%s, copy %08X, life %d) in stage %d room %d (record %d; copies this "
                "world visit: %d / %d; %d copy drops this session)", DropItemName(item), type, variant, kDropTierName[tier], id,
                life, stage, room, recI, g_dropCopyL.n, DropCopyBudget(level, per), g_dropCopyN);  // v3.33 EN333: + life
        return;
    }
    if (!g_dropAgeOn && ident < 0 && DropLedgerCount(L, stage, visit) >= 15 && LogQuota(&g_dropQuotaAnom, 20))  // v3.33 EN333 (v3.35 GH335: only with the budget)
        LogAnomaly("supply drops", "world %d visit %d: the ledger is at 15 (its nibble's top) yet a drop was allowed (budget %d)",
                   stage, visit, kDropRates[level].worldBudget);
    DropLedgerCommit(L, tier, stage, visit, ident);  // (v3.35 GH335: counted for the log; it saturates at 15, nothing reads it as a limit)
    if (tier == DRT_GHOST)  // v3.35 GH335
        Log("supply drops: %s from %s %02X/%d (char %08X, its down %d this world visit, %s of the ghost chance %.4f) in stage %d room "
            "%d at %.0f %.0f %.0f (record %d; it fades in %.0f s)", DropItemName(item), ident >= 0 ? kDropIdentName[ident] : "ghost",
            type, variant, id, gdown + 1, kDropGroupName[DropGroupOf(item) >= 0 ? DropGroupOf(item) : 0], DropClassAny(level, cls),
            stage, room, pos[0], pos[1], pos[2], recI, DROP_AGE_S);  // v3.38 DR338c
    else if (ident >= 0)
        Log("supply drops: %s from %s (stage %d, visit %d, top-level down %d; %s, the %s class) at %.0f %.0f %.0f (record %d)", DropItemName(item),
            kDropIdentName[ident], stage, visit, (L->b[0] + 255) & 0xFF, kDropGroupName[DropGroupOf(item) >= 0 ? DropGroupOf(item) : 0],
            kDropClassName[cls], pos[0], pos[1], pos[2], recI);  // v3.38 DR338c
    else
        Log("supply drops: %s from type %02X/%d (%s, gauge %.1f, char %08X, life %d) in stage %d room %d (record %d; world visit "
            "%d: %d / %d)", DropItemName(item), type, variant, kDropTierName[tier], g_dropG[type][variant], id, life, stage, room,
            recI, visit, DropLedgerCount(L, stage, visit), kDropRates[level].worldBudget);  // v3.33 EN333: + life
}

// v3.35 GH335: a drop's end: as the game's own take of a run-time pickup (ItemPickupEventTake 0x4D1180: PickupRecordClear
// 0x4F3D10 = state 0 and active 0, then ActorDestroy 0x52B020 of its room object's task when the room is loaded). Nothing
// else holds a record: only the loaded room's objects point at theirs (EventObj +0x48, the event's +0x98), the Gum Head's
// return list takes state-2 records only (0x4F4130; ours are state 1), the Sword's moves (0x4F4190) +0x01 = 1 ones only,
// and the record scans (0x4F4000, 0x4F40B0, the allocation 0x4F3BA0) read the state. DropAgeTick never ends one in the
// loaded room while the player is within 300 of it (its pickup event could be running) or outside gameplay.
static void DropFade(BYTE *r, int here, unsigned us)
{
    const int item = *(const int *)(r + 0x38), st = *(const short *)(r + 6), rm = *(const short *)(r + 8);
    void *task = NULL;
    if (here) {  // its room object: gEventObjList (0x13D8200: +4 task, +0x0C next, +0x48 the record)
        const BYTE *o = *(const BYTE *const volatile *)0x13D8200;
        int guard = 0;
        for (; (ULONG_PTR)o >= 0x10000 && guard < 512; o = *(const BYTE *const *)(o + 0x0C), ++guard)
            if (*(BYTE *const *)(o + 0x48) == r) {
                task = *(void *const *)(o + 4);
                break;
            }
    }
    r[0] = 0;                // PickupRecordClear
    *(short *)(r + 2) = 0;
    *(unsigned *)(r + 0x70) = 0;
    *(unsigned *)(r + 0x74) = 0;
    if ((ULONG_PTR)task >= 0x10000 && ((int(__cdecl *)(const void *))0x52B230)(task) == 4)  // ActorGetType: an item pickup
        ((void(__cdecl *)(void *))0x52B020)(task);                                            // ActorDestroy
    ++g_dropFaded;
    if (LogQuota(&g_dropQuotaAge, 300))
        LogV("supply drops: %s faded after %.0f s (room %d:%d, %s; %d faded this session)", DropItemName(item), us / 1000000.0f,
             st, rm, here ? (task ? "the room you are in: its object removed" : "the room you are in, no object") : "not loaded",
             g_dropFaded);
}
// every dispatcher tick, whatever the level (a drop lying when Supply Drops goes Off fades too)
static void DropAgeTick(int playing, int rate, int stage, int room)
{
    const float *pp = (const float *)0x10A0F60;
    int i;
    for (i = 0; i < DROP_REC_N; ++i) {
        BYTE *r = DropRec(i);
        const unsigned w70 = *(const unsigned *)(r + 0x70);
        unsigned us;
        int here, nearYou = 0, off;
        if (!DropRecOurs(r)) continue;
        if (!DropAgeTagged(w70) && LogQuota(&g_dropQuotaAge, 300))
            LogV("supply drops: %s in room %d:%d has no age (a drop of an older build or save): it fades %.0f s from now",
                 DropItemName(*(const int *)(r + 0x38)), *(const short *)(r + 6), *(const short *)(r + 8), DROP_AGE_S);
        us = DropAgeStep(DropAgeUs(w70), playing, rate);
        here = *(const short *)(r + 6) == stage && *(const short *)(r + 8) == room;
        if (here) {
            const float dx = *(const float *)(r + 0x10) - pp[0], dz = *(const float *)(r + 0x18) - pp[2];
            nearYou = !_finite(dx) || !_finite(dz) || dx * dx + dz * dz < 300.0f * 300.0f;
        }
        if (DropAgeExpired(us) && playing && !nearYou) {
            DropFade(r, here, us);
            continue;
        }
        off = here && playing && !nearYou && DropAgeBlinkOff(us);
        *(short *)(r + 2) = (short)(off ? 0 : 1);
        *(unsigned *)(r + 0x70) = DropAgeWord(us, off);
    }
}
static void DropTickC(void)
{
    const int level = (int)g_dropLevel;
    const int rate = *(volatile int *)0x5D5248;
    const int stage = *(volatile int *)0xFD5A90, room = *(volatile int *)0xFD5A94 & 0xFFFF;
    const DWORD roomKey = (DWORD)stage << 16 | (DWORD)room;
    const BYTE *t = NULL;
    int guard = 0;
    // v3.30 AS330: the session seed follows loads whatever the level (a load while Off re-rolls too)
    DropSeedTick(rate, *(volatile int *)0xFD5C20 == 0 && *(volatile int *)0xFD5A74 == 0 && *(volatile int *)0xFD5AA0 == 0 &&
                           !g_qsLoading && rate >= 1 && rate <= 240 && *(void *volatile *)0xFD5BA0 != NULL);
    if (roomKey != g_dropTallyKey) {  // v3.33 EN333: the room just left, in one line (only when something was killed there)
        const DropRoomTally *T = &g_dropRoomT;
        if (T->kills + T->other && LogQuota(&g_dropQuotaRoom, 300))
            LogV("supply drops: room %d:%d: %d kill(s) / ghost down(s) rolled (%d of copies), %d drop(s); no drop: %d lost the roll, "
                 "%d budget used up, %d pool full, %d model not here, %d no place; %d killed by someone else than you", (int)(g_dropTallyKey >> 16),
                 (int)(g_dropTallyKey & 0xFFFF), T->kills, T->copies, T->drops, T->chance, T->budget, T->pool, T->model, T->place,
                 T->other);
        memset(&g_dropRoomT, 0, sizeof g_dropRoomT);
        g_dropTallyKey = roomKey;
    }
    if (g_dropAgeOn)  // v3.35 GH335: the drops' age (gameplay ticks only), the blink and the fade
        DropAgeTick(*(volatile int *)0xFD5C20 == 0 && *(volatile int *)0xECB1B4 == 0 && *(volatile int *)0xFD5A74 == 0 &&
                        *(volatile int *)0xFD5AA0 == 0 && !g_qsLoading && rate >= 1 && rate <= 240 && *(void *volatile *)0xFD5BA0,
                    rate, stage, room);
    if (level <= 0 || level >= DROP_LEVELS) {
        if (g_dropWasOn) DropTableReset(&g_dropTab), g_dropWasOn = 0;
        return;
    }
    // gameplay only: no menu / event / movie, no load in progress (the load menu's world start, Refined's quick load)
    if (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0 || *(volatile int *)0xFD5A74 != 0 ||
        *(volatile int *)0xFD5AA0 != 0 || g_qsLoading || rate < 1 || rate > 240 || !*(void *volatile *)0xFD5BA0 ||
        stage < 2 || stage > 8 || roomKey != g_dropRoomKey) {
        DropTableReset(&g_dropTab);  // a gap: nothing seen before it counts as the previous tick
        g_dropRoomKey = roomKey;
        return;
    }
    g_dropWasOn = 1;
    ++g_dropTab.tick;
    while ((t = DpNext(t)) != NULL && guard++ < 96) {
        const BYTE *w = *(const BYTE *const *)(t + 0x408), *rec = *(const BYTE *const *)(t + 0x41C);
        const float *hp;
        int type, cls, ident = -1, kind = 0, variant = 0, ev, dk;
        unsigned id;
        DropTrack *e;
        if (!w || (ULONG_PTR)(rec + 0x2C) < 0x10000) continue;
        type = *(const int *)(w + 0x20);
        hp = *(const float *const *)(w + 0x34);
        if (!hp || !(hp[1] > 0.0f)) continue;
        id = *(const unsigned *)(rec + 0x2C);  // ObjInfo +0x00: the character id given to EnemyCreate (0x45B6D0)
        kind = *(const int *)(rec + 0x2C + 0x14) & 0xF;
        if (type == 0x0C || type == 0x0D) variant = *(const int *)(w + 0x40);
        // v3.32 DHS332 (DropKindOf): placements of this world and the top level as v3.31, + Enemy Copies' copies (CopyDrops);
        // never nest spawns (id 0) or the cheat table's extra copies (ids 60xxxxxx)
        dk = DropKindOf2(type, variant, kind, id, stage, g_dropCopyDrops, g_dropGhostDrops, &ident);  // v3.35 GH335: + the ghosts
        if (dk == DRK_NONE) continue;
        cls = *(const int *)(w + 0x3C);
        e = DropTrackGet(&g_dropTab, t, w, id);
        if (!e) {
            if (LogQuota(&g_dropQuotaAnom, 20))  // v3.33 EN333
                LogAnomaly("supply drops", "the kill tracker is full (%d beings in room %d:%d): %08X (type %02X) is not watched, "
                           "its kill would not roll", DROP_TRACK_MAX, stage, room, id, type);
            continue;
        }
        {   // v3.33 EN333: a death without the player's credit is said (no roll); v3.35 GH335: anyone's kills roll (AnyKills)
            int other = 0;
            const int la = *(const int *)((const BYTE *)hp + 0x20);
            ev = DropObserve3(&g_dropTab, e, cls, hp[0], la, ident >= 0 || dk == DRK_GHOST || dk == DRK_GHOST_COPY, g_dropAnyKills,
                              &other);
            if (other) {
                ++g_dropRoomT.other;
                if (LogQuota(&g_dropQuotaWhy, 100))
                    LogV(g_dropAnyKills ? "supply drops: %08X (type %02X) in room %d:%d was killed by %s (last attack id %02X): it rolls "
                                          "(anyone's kills roll)"
                                        : "supply drops: no drop: %08X (type %02X) in room %d:%d died without your credit (%s, last "
                                          "attack id %02X: not the player's 02..48)", id, type, stage, room, DropCreditName(la), la);
            }
        }
        if (!ev) continue;
        if (ident < 0) variant = DropVariantOf(type, w, hp[1]);
        DropOnEvent(t, w, type, variant, ident, hp, stage, room, id, level,
                    dk == DRK_KILL_COPY || dk == DRK_TOP_COPY || dk == DRK_GHOST_COPY, dk == DRK_GHOST || dk == DRK_GHOST_COPY);  // v3.35 GH335
    }
}
static void DropTick(void)  // QsTick (0x4EF982), game thread
{
    if (!g_dropInstalled || g_dropFail) return;
    __try {
        DropTickC();
    } __except (DropFaultNote(GetExceptionInformation())) {
        g_dropFail = 1;
        g_dropReady = 0;  // the menu shows "not available"
        Log("supply drops: faulted (code %08lX at %p): the feature is off for this session", g_dropFaultCode, g_dropFaultAt);
    }
}

// ---------------------------------------------------------------- install (Init, after DeathPresenceInstall)
// No exe byte is written: the game functions it calls are checked (first bytes as in the GOG v7 exe), and the tables
// it reads are sanity-checked. Fail closed: the row then says "not available".
static void DropsInstall(void)
{
    static const Site kFns[] = {
        { 0x463150, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x44 }, 9 },  // the enemy drop helper
        { 0x459490, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x34 }, 9 },  // EnemyFloorRayCheck
        { 0x4F4710, { 0x8B, 0x54, 0x24, 0x04, 0x33, 0xC9, 0x33, 0xC0, 0x56 }, 9 },  // ItemGetModelFile
        { 0x4F4750, { 0x8B, 0x54, 0x24, 0x04, 0x33, 0xC9, 0x33, 0xC0, 0x56 }, 9 },  // ItemGetTextureFile
        { 0x5736D0, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0xC7 }, 8 },        // FileLookupResident
        { 0x45E760, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x75, 0x06 }, 8 },        // EnemyListNext
        { 0x468E00, { 0x51, 0xC7, 0x44, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 }, 9 },  // EnemyStatGetMaxHp
    };
    int i;
    if (!g_qsTickOn) {
        Log("supply drops: no 0x4EF982 tick dispatcher: the feature is off");
        return;
    }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i)
        if (!SiteOk(&kFns[i]) && !PeHookedCallee(kFns[i].va, "supply drops")) {  // v3.34 PEC334: PE's hook is fine
            Log("supply drops: %08lX is not the stock function (another mod?): the feature is off", kFns[i].va);
            return;
        }
    for (i = 0; i < 0x13; ++i) {
        const int c = ((const int *)0x5B8BA0)[i];
        if (c < 1 || c > 8) {
            Log("supply drops: the stat table's variant counts (005B8BA0) are not the stock ones: the feature is off");
            return;
        }
    }
    DropBuildTiers();
    DropTableReset(&g_dropTab);
    DropCopyLedgerReset(&g_dropCopyL);  // v3.32 DHS332
    DropLivesReset(&g_dropLives);       // v3.33 EN333
    DropGhostDownsReset(&g_dropGhostD);  // v3.35 GH335
    g_dropGhostDrops = GetPrivateProfileIntA("Gameplay", "GhostDrops", 1, g_iniPath) ? 1 : 0;  // (hidden)
    g_dropAnyKills = GetPrivateProfileIntA("Gameplay", "AnyKills", 1, g_iniPath) ? 1 : 0;      // (hidden)
    g_dropAgeOn = GetPrivateProfileIntA("Gameplay", "DropAge", 1, g_iniPath) ? 1 : 0;          // (hidden)
    Log("supply drops: v3.38: ghosts %s; %s kills roll; drops %s (lying caps %d anywhere / %d a room, %d records kept free; "
        "a pickup or a fade frees its place at once; the same for every level)",  // v3.37 MN337: the caps 10 / 4 -> 64 / 10; v3.38 DR338: v3.38, the levels
        g_dropGhostDrops ? "drop (every full down rolls the ghost class's full chance, no falloff)" : "never drop (GhostDrops=0)",  // v3.38 DR338c
        g_dropAnyKills ? "anyone's" : "only the player's (AnyKills=0)",
        g_dropAgeOn ? "fade 120 s of gameplay after they land, in any room (no world budget)" : "never fade (DropAge=0: the v3.34 world budget)",
        kDropRates[DROP_NORMAL].lyingCap, kDropRates[DROP_NORMAL].roomCap, kDropRates[DROP_NORMAL].minFree);
    {   // v3.38 DR338: the four levels' chances and the equal shares, from the tables themselves (logcheck.py reads both lines)
        const DropRates *r = &kDropRates[DROP_RARE], *n = &kDropRates[DROP_NORMAL], *a = &kDropRates[DROP_ABUNDANT];
        int i, ok = kGpCount[GP_DROP] == DROP_LEVELS;
        // v3.38 DR338c: the final design: the class table, the shares, each item's exact chance on Normal (all guns owned)
        char line[900];
        int c, n2 = 0;
        (void)r, (void)n, (void)a;
        for (c = 0; c < DRC_N; ++c)
            n2 += sprintf_s(line + n2, sizeof line - n2, "%s%s %g / %g / %g %%", c ? ", " : "", kDropClassName[c],
                            kDropClassNormal[c][DRG_COMMON] * 100.0f, kDropClassNormal[c][DRG_UNCOMMON] * 100.0f,
                            kDropClassNormal[c][DRG_RARE] * 100.0f);
        Log("supply drops: levels Off / Rare / Normal / Abundant (Normal's chances x%g / x%g / x%g): one roll a kill or full down, "
            "common / uncommon / rare on Normal: %s (any item %g %% for every class; every ghost down rolls the full chance)",
            kDropRates[DROP_RARE].mult, kDropRates[DROP_NORMAL].mult, kDropRates[DROP_ABUNDANT].mult, line,
            DropClassAny(DROP_NORMAL, DRC_WEAK) * 100.0f);
        Log("supply drops: shares in each group: common %s %d / %s %d / %s %d, uncommon %s %d / %s %d, rare %s %d / %s %d / %s %d / "
            "%s %d; Pistol and Silver Bullets need the pistol, Revolver Bullets the revolver: a gun not owned gives its share to "
            "the others of its group", DropItemName(kDropCommon[0].item), kDropCommon[0].weight, DropItemName(kDropCommon[1].item),
            kDropCommon[1].weight, DropItemName(kDropCommon[2].item), kDropCommon[2].weight, DropItemName(kDropUncommon[0].item),
            kDropUncommon[0].weight, DropItemName(kDropUncommon[1].item), kDropUncommon[1].weight, DropItemName(kDropRare[0].item),
            kDropRare[0].weight, DropItemName(kDropRare[1].item), kDropRare[1].weight, DropItemName(kDropRare[2].item),
            kDropRare[2].weight, DropItemName(kDropRare[3].item), kDropRare[3].weight);
        for (c = 0, n2 = 0; c < DRC_N; ++c)
            n2 += sprintf_s(line + n2, sizeof line - n2, "%s%s: Drink %.3f, Kit %.3f, Candle %.3f, each rare %.4f %%", c ? "; " : "",
                            kDropClassName[c], 100.0 * DropItemChance(DROP_NORMAL, c, 0x09, 3), 100.0 * DropItemChance(DROP_NORMAL, c, 0x0B, 3),
                            100.0 * DropItemChance(DROP_NORMAL, c, 0x04, 3), 100.0 * DropItemChance(DROP_NORMAL, c, 0x08, 3));
        Log("supply drops: on Normal, all guns owned, each item's chance a roll (Pistol / Revolver Bullets as the Drink): %s", line);
        ok = ok && kDropCommon[0].weight == 1 && kDropCommon[1].weight == 1 && kDropCommon[2].weight == 1 && kDropUncommon[0].weight == 2 &&
             kDropUncommon[1].weight == 1 && kDropRare[0].weight == 1 && kDropRare[1].weight == 1 && kDropRare[2].weight == 1 &&
             kDropRare[3].weight == 1;  // v3.38 DR338c: the final shares
        for (i = 0; i < DROP_LEVELS && ok; ++i) ok = kGpName[GP_DROP][i] && !strcmp(kGpName[GP_DROP][i], kDropLevelName[i]);
        if (!ok)
            LogAnomaly("supply drops", "the Options row's values (%d) are not the levels Off / Rare / Normal / Abundant, or a share "
                       "is not the design's: the row and the chances disagree", kGpCount[GP_DROP]);  // v3.38 DR338c
    }
    if (DROP_REC_N < kDropRates[DROP_NORMAL].lyingCap + kDropRates[DROP_NORMAL].minFree + 1)  // v3.37 MN337: the stock pool ([Gameplay] PickupPool=0)
        Log("supply drops: the pickup pool holds %d records: at most %d drops lie at once (the 16 kept free come first; "
            "[Gameplay] PickupPool=128 allows %d)", DROP_REC_N, DROP_REC_N - kDropRates[DROP_NORMAL].minFree > 0 ? DROP_REC_N - kDropRates[DROP_NORMAL].minFree : 0,
            kDropRates[DROP_NORMAL].lyingCap);
    g_dropCopyDrops = GetPrivateProfileIntA("Gameplay", "CopyDrops", 1, g_iniPath) ? 1 : 0;  // (hidden; 0 = v3.31's rule)
    Log("supply drops: Enemy Copies' copies %s", g_dropCopyDrops ? "drop like their type (own keys, a session budget of the "
                                                                    "level's x the copies per original; nothing saved)"
                                                                  : "never drop (CopyDrops=0: v3.31)");
    {   // v3.30 AS330: the session seed (hidden [Gameplay] SupplyDropsSeed: Session, the default, or Fixed = v3.29's rolls)
        char sm[16];
        GetPrivateProfileStringA("Gameplay", "SupplyDropsSeed", "Session", sm, sizeof sm, g_iniPath);
        g_dropSeed = _stricmp(sm, "Fixed") ? DropEntropy(0x5D2F1B07u) : 0;
        Log("supply drops: rolls %s", g_dropSeed ? "seeded per session (a new seed at the game start, every New Game, load "
                                                   "and quick load)" : "fixed per kill (SupplyDropsSeed=Fixed: a reload gives the same)");
    }
    g_dropInstalled = 1;
    g_dropReady = 1;
    Log("supply drops: in (calls 00463150 / 00459490 from the tick; ledger %08X, pickup records %08X); level %s", DROP_LEDGER_VA,
        DROP_REC_VA, kGpName[GP_DROP][g_gp[GP_DROP]]);
}
#endif  // DROP_PURE_ONLY
