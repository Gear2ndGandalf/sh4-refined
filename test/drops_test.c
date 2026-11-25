// SUPPLY DROPS: native test of the pure part of src\drops.inc (PLAN 9): the gauge and tiers from the exe's own stat
// numbers, the top-level identities, the hashed rolls and their rates, Rare within Normal within Abundant (v3.38 DR338; was Generous), the gun rule, the ledger
// (one drop per top-level being, the world budget), the tracker (kills only as live -> dead on consecutive ticks, by the
// player; full downs only from a standing / staggered state; restored beings and gaps never fire), the pool plan and the
// marker. Build and run: test\build_drops_test.cmd (cl /O2 x86, as build.cmd).
// v3.38 DR338: the levels are 0 Off, 1 Rare, 2 Normal, 3 Abundant (DROP_RARE / DROP_NORMAL / DROP_ABUNDANT; v3.37's 1 Normal,
// 2 Generous); TestLevels338 covers each level's chances, the equal shares with and without each gun, the ini names.
#define _CRT_SECURE_NO_WARNINGS  // (sprintf into fixed buffers of the test)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define DROP_PURE_ONLY
#include "../src/drops.inc"

static int g_fail;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}

// the Normal rows of the stat table 0x5B8D80 (max HP, +0x14 / +0x18 / +0x1C) and the attack table 0x63D530, as read
// from SH4Decomp/orig/sh4.exe (PLAN 3); the damage is the strongest attack the type's file emits
typedef struct { int type, variant; float hp, dmg; int tier; const char *name; } GaugeRow;
static const GaugeRow kRows[] = {
    { 0x09, 0, 50.0f, 5.0f, DRT_WEAK, "Tremer (0x5A 5, 0x59 3)" },
    { 0x03, 0, 100.0f, 5.0f, DRT_WEAK, "Hummer (0x4A 5, 0x4B 3, 0x4C 2)" },
    { 0x06, 0, 80.0f, 10.0f, DRT_WEAK, "Wheelchair v0 (0x51 10)" },
    { 0x06, 1, 100.0f, 10.0f, DRT_WEAK, "Wheelchair v1" },
    { 0x05, 0, 100.0f, 10.0f, DRT_WEAK, "Wall Man v0 (0x4D-0x50 10)" },
    { 0x0A, 0, 120.0f, 10.0f, DRT_WEAK, "Sniffer Dog (0x5B 10)" },
    { 0x0A, 1, 120.0f, 10.0f, DRT_WEAK, "Sniffer Dog v1" },
    { 0x05, 1, 240.0f, 10.0f, DRT_STRONG, "Wall Man v1 (stage 8)" },
    { 0x05, 2, 300.0f, 10.0f, DRT_STRONG, "Wall Man v2" },
    { 0x07, 0, 200.0f, 20.0f, DRT_STRONG, "Gum Head v0 (0x52-0x55 10-20)" },
    { 0x07, 1, 300.0f, 20.0f, DRT_STRONG, "Gum Head v1 (steals)" },
    { 0x08, 0, 200.0f, 15.0f, DRT_STRONG, "Twin Victim (0x56-0x58 10-15)" },
    { 0x08, 1, 350.0f, 15.0f, DRT_STRONG, "Bottom (Twin Victim v1)" },
};

static int RunKills(int level, int tier, int owned, unsigned salt, int n, int *counts)
{
    int i, drops = 0;
    memset(counts, 0, sizeof(int) * 16);
    for (i = 0; i < n; ++i) {
        const int item = DropDecide(level, tier, DropKeyKill(2 + i % 7, 0x02000000u + (unsigned)i * 7u + salt, 7, 0), owned);
        if (item) ++drops, ++counts[item & 15];
    }
    return drops;
}

// ---- v3.32 DHS332: Enemy Copies' copies drop like their type (DropKindOf, the copy keys, the copies' session ledger,
// the rates and the farming at x4, the pool reserve under a mob of copies)
static unsigned XorShift(unsigned *s)
{
    *s ^= *s << 13, *s ^= *s >> 17, *s ^= *s << 5;
    return *s;
}
static void TestCopies(void)
{
    char b[320];
    int id_, i;
    // -- which beings give what
    {
        const unsigned orig = EnmScriptId(5, 0, 3, 4), cp = EnmCopyId(3, 4, 2), cyn = 0xFFFF0002u, cynCp = EnmCopyId(7, 1, 3);
        Check(DropKindOf(7, 0, 0, orig, 5, 1, &id_) == DRK_KILL && id_ < 0, "copies: an original Gum Head of this world: a kill");
        Check(DropKindOf(7, 0, 0, cp, 5, 1, &id_) == DRK_KILL_COPY && id_ < 0, "copies: its copy: a copy kill");
        Check(DropKindOf(7, 0, 0, cp, 5, 0, &id_) == DRK_NONE, "copies: CopyDrops=0: the copy never drops (v3.31)");
        Check(DropKindOf(7, 0, 0, orig, 6, 1, &id_) == DRK_NONE, "copies: (a placement of another world: never, as v3.31)");
        Check(DropKindOf(7, 0, 0, 0u, 5, 1, &id_) == DRK_NONE, "copies: a nest spawn (id 0): never");
        Check(DropKindOf(0x0A, 0, 0, 0x60000001u, 5, 1, &id_) == DRK_NONE, "copies: the cheat table's extras (60xxxxxx): never");
        Check(DropKindOf(0x0C, 0, 0, cyn, 5, 1, &id_) == DRK_TOP && id_ == DI_CYNTHIA, "copies: Cynthia's ghost: the top level");
        Check(DropKindOf(0x0C, 0, 0, cynCp, 5, 1, &id_) == DRK_TOP_COPY && id_ == DI_CYNTHIA, "copies: her copy: the top level, as a copy");
        Check(DropKindOf(0x0C, 0, 0, cynCp, 5, 0, &id_) == DRK_NONE, "copies: CopyDrops=0: her copy never gives the top level (v3.31 EN331)");
        Check(DropKindOf(0x0D, 0, 0, EnmCopyId(9, 2, 1), 5, 1, &id_) == DRK_TOP_COPY && id_ == DI_JASPER &&
                  DropKindOf(0x0E, 0, 0, EnmCopyId(9, 3, 1), 4, 1, &id_) == DRK_TOP_COPY && id_ == DI_ANDREW &&
                  DropKindOf(0x0F, 0, 0, EnmCopyId(9, 4, 3), 5, 1, &id_) == DRK_TOP_COPY && id_ == DI_RICHARD,
              "copies: Jasper's, Andrew's and Richard's copies: the top level");
        Check(DropKindOf(0x0B, 2, 0, EnmCopyId(4, 5, 1), 5, 1, &id_) == DRK_NONE && DropKindOf(0x0C, 1, 0, EnmCopyId(4, 6, 1), 5, 1, &id_) == DRK_NONE &&
                  DropKindOf(0x0D, 1, 0, EnmCopyId(4, 7, 1), 5, 1, &id_) == DRK_NONE && DropKindOf(0x11, 0, 0, EnmCopyId(4, 8, 1), 5, 1, &id_) == DRK_NONE,
              "copies: the victims' and the Patient's copies give nothing, as their originals");
        Check(DropKindOf(0x02, 0, 0, EnmCopyId(4, 9, 1), 5, 1, &id_) == DRK_NONE, "copies: Whitestool copies give nothing, as their originals");
        Check(DropKindOf(7, 0, 0, 0x1E000010u, 5, 1, &id_) == DRK_NONE, "copies: a 0x1E id with k = 0 is no copy id: never");
    }
    // -- every copy has its own key, never its original's
    {
        int okId = 1, okKey = 1, okSib = 1, okVisit = 1, okTop = 1, stage, room, n, k, half;
        for (stage = 2; stage <= 8; ++stage)
            for (half = 0; half < 2; ++half)
                for (room = 1; room < 43; ++room)
                    for (n = 0; n < 31; ++n) {
                        const unsigned orig = EnmScriptId(stage, half, room, n);
                        unsigned ks[4];
                        ks[0] = DropKeyKill(stage, orig, 7, 0);
                        for (k = 1; k <= 3; ++k) {
                            const unsigned cid = DropCopyKillId(EnmCopyId(room, n, k), half);
                            const unsigned st = cid >> 24 & 0x7F;
                            if (st >= 2 && st <= 8) okId = 0;  // a copy's identity can never be a placement's
                            if (cid == orig) okId = 0;
                            ks[k] = DropKeyKill(stage, cid, 7, 0);
                            if (ks[k] == ks[0]) okKey = 0;
                            if (DropCopyKillId(EnmCopyId(room, n, k), half) == DropCopyKillId(EnmCopyId(room, n, k), !half)) okVisit = 0;
                        }
                        if (ks[1] == ks[2] || ks[1] == ks[3] || ks[2] == ks[3]) okSib = 0;
                    }
        for (i = 0; i < 64; ++i) {
            const unsigned c1 = EnmCopyId(7, 1, 1 + i % 3);
            if (DropKeyTopCopy(DI_CYNTHIA, 5, 0, c1, i) == DropKeyTop(DI_CYNTHIA, 5, 0, i)) okTop = 0;
            if (DropKeyTopCopy(DI_CYNTHIA, 5, 0, c1, i) == DropKeyTopCopy(DI_CYNTHIA, 5, 0, c1, i + 1)) okTop = 0;
            if (DropKeyTopCopy(DI_CYNTHIA, 5, 0, EnmCopyId(7, 1, 1), i) == DropKeyTopCopy(DI_CYNTHIA, 5, 0, EnmCopyId(7, 1, 2), i)) okTop = 0;
            if (DropKeyTopCopy(DI_CYNTHIA, 5, 0, c1, i) == DropKeyTopCopy(DI_CYNTHIA, 5, 1, c1, i)) okTop = 0;
        }
        Check(okId, "copies: a copy's identity (copy id + visit bit) is never a placement id of any world (stage bits 0x1E)");
        Check(okKey, "copies: no copy's kill key equals its original's (7 worlds x 2 visits x 42 rooms x 31 objects x 3 copies)");
        Check(okSib && okVisit, "copies: the three copies of one original have three keys; the second visit gives other keys");
        Check(okTop, "copies: a copy's full down key: never the original's, new for every down, per copy and per visit");
    }
    // -- the copies' ledger: session only, the level's budget x the copies per original, top level unlimited
    {
        DropCopyLedger c;
        DropLedger L;
        int per, lvl, given, okBudget = 1, j;
        memset(&L, 0, sizeof L);
        for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl)  // v3.38 DR338: + Rare
            for (per = 1; per <= 3; ++per) {
                memset(&c, 0, sizeof c);
                DropCopyLedgerReset(&c);
                for (given = 0, j = 0; j < 200; ++j)
                    if (DropCopyAllowed(&c, lvl, DRT_WEAK, 4, 0, per)) DropCopyCommit(&c, DRT_WEAK), ++given;
                if (given != kDropRates[lvl].worldBudget * per) okBudget = 0;
            }
        sprintf(b, "copies: the copies' budget a world visit = the level's x copies per original (Rare 4 / 8 / 12, Normal 8 / 16 / 24, Abundant 15 / 30 / 45 at x2 / x3 / x4)");
        Check(okBudget, b);
        Check(DropCopyBudget(DROP_NORMAL, 0) == 8 && DropCopyBudget(DROP_NORMAL, 9) == 24 && DropCopyBudget(0, 3) == 0, "copies: per clamped to 1..3; Off: none");
        memset(&c, 0, sizeof c);
        for (j = 0; j < 24; ++j) DropCopyAllowed(&c, DROP_NORMAL, DRT_WEAK, 4, 0, 3), DropCopyCommit(&c, DRT_WEAK);
        Check(!DropCopyAllowed(&c, DROP_NORMAL, DRT_STRONG, 4, 0, 3), "copies: x4, Normal: the 25th copy drop of a world visit is refused");
        Check(DropCopyAllowed(&c, DROP_NORMAL, DRT_TOP, 4, 0, 3), "copies: the top level has no budget (as the originals)");
        Check(DropCopyAllowed(&c, DROP_NORMAL, DRT_WEAK, 4, 1, 3), "copies: the second visit of the world starts afresh (copies come back then)");
        for (j = 0; j < 24; ++j) DropCopyCommit(&c, DRT_WEAK);
        Check(DropCopyAllowed(&c, DROP_NORMAL, DRT_WEAK, 5, 1, 3), "copies: another world starts afresh");
        for (j = 0; j < 24; ++j) DropCopyCommit(&c, DRT_WEAK);
        c.downs = 7;
        DropCopyLedgerReset(&c);
        Check(DropCopyAllowed(&c, DROP_NORMAL, DRT_WEAK, 5, 1, 3) && c.downs == 7, "copies: a load / world reset starts the budget afresh; the down count keeps growing");
        Check(!DropCopyAllowed(&c, 0, DRT_WEAK, 5, 1, 3) && !DropCopyAllowed(&c, DROP_NORMAL, DRT_WEAK, 9, 1, 3), "copies: Off or outside the worlds: never");
        {
            static const unsigned char zero[8] = { 0 };
            Check(!memcmp(L.b, zero, 8), "copies: the saved ledger is never touched by a copy (only DropCopyLedger moves)");
        }
    }
    // -- the rates: a copy rolls exactly like its original (per kill / per down), and the farming at x4
    {
        static const int kTier[2] = { DRT_WEAK, DRT_STRONG };
        unsigned rs = 0x9E3779B9u;
        int lvl, ti;
        for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl) {  // v3.38 DR338: + Rare
            for (ti = 0; ti < 2; ++ti) {
                const int N = 60000;
                int dO = 0, dC = 0, uO = 0, uC = 0;
                for (i = 0; i < N; ++i) {
                    const unsigned seed = XorShift(&rs) | 1u, orig = EnmScriptId(2 + i % 7, i & 1, 1 + i % 40, i % 31);
                    const unsigned cid = DropCopyKillId(EnmCopyId(1 + i % 40, i % 31, 1 + i % 3), i & 1);
                    const int io = DropDecide(lvl, kTier[ti], DropRollKey(DropKeyKill(2 + i % 7, orig, 7, 0), seed), 3);
                    const int ic = DropDecide(lvl, kTier[ti], DropRollKey(DropKeyKill(2 + i % 7, cid, 7, 0), seed), 3);
                    dO += io != 0, dC += ic != 0;
                    uO += io == 0x0B || io == 0x04, uC += ic == 0x0B || ic == 0x04;  // v3.38 DR338b: Silver Bullets are rare now
                }
                {
                    // v3.38 DR338c: one roll by class: any item r + u + c, uncommon u
                    const int cls = kTier[ti] == DRT_STRONG ? DRC_STRONG : DRC_WEAK;
                    const float pU = DropClassChance(lvl, cls, DRG_UNCOMMON), pAny = DropClassAny(lvl, cls);
                    const float fo = (float)dO / N, fc = (float)dC / N;
                    sprintf(b, "copies: %s %s: a copy's drop rate %.3f = its original's %.3f (expected %.3f; uncommon %.3f / %.3f)",
                            kDropLevelName[lvl], kDropTierName[kTier[ti]], fc, fo, pAny, (float)uC / N, (float)uO / N);
                    Check(fabsf(fc - pAny) < 0.008f && fabsf(fo - pAny) < 0.008f && fabsf((float)uC / N - pU) < 0.006f, b);
                    printf("      x4 farming, %s %s: each placement gives %.2f drops per kill round (x1: %.2f)\n",
                           kDropLevelName[lvl], kDropTierName[kTier[ti]], 4.0f * pAny, pAny);
                }
            }
            {   // a named ghost's full downs: the original and three copies
                const int N = 60000;
                int rO = 0, rC = 0;
                for (i = 0; i < N; ++i) {
                    const unsigned seed = XorShift(&rs) | 1u;
                    rO += DropDecide(lvl, DRT_TOP, DropRollKey(DropKeyTop(DI_CYNTHIA, 5, i & 1, i), seed), 3) != 0;
                    rC += DropDecide(lvl, DRT_TOP, DropRollKey(DropKeyTopCopy(DI_CYNTHIA, 5, i & 1, EnmCopyId(7, 1, 1 + i % 3), i), seed), 3) != 0;
                }
                const float pN = DropClassAny(lvl, DRC_NAMED);  // v3.38 DR338c: the named ghost class (copies roll like it)
                sprintf(b, "copies: %s: a copy's full down gives an item %.3f of the time, the original %.3f (expected %.2f, the named ghost "
                           "class); x4 = %.2f items per round of four full downs (x1: %.2f)", kDropLevelName[lvl],
                        (float)rC / N, (float)rO / N, pN, 4.0f * pN, pN);
                Check(fabsf((float)rC / N - pN) < 0.008f && fabsf((float)rO / N - pN) < 0.008f, b);
            }
        }
    }
    // -- a mob of copies cannot fill the pickup pool: at least 16 of 32 stay free, 3 a room, 6 / 8 lying in all
    {
        int lvl, game, room;
        for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl)  // v3.38 DR338: + Rare (the same caps)
            for (game = 0; game <= 16; game += 4) {
                DropRecView v[DROP_REC_MAX];  // v3.37 MN337: DROP_REC_N is the test pool size now
                int j, rc, freeN, ours, minFree = 32, maxRoom = 0, given = 0, r2;
                memset(v, 0, sizeof v);
                for (j = 0; j < game; ++j) v[j].state = 1, v[j].active = 1;  // the game's own records (Swords, returns)
                for (room = 1; room <= 10; ++room)
                    for (j = 0; j < 12; ++j) {  // 12 copy kills a room, all dropping
                        const unsigned key = 0xC0000000u | (unsigned)room << 8 | (unsigned)j;
                        if (!DropPoolPlan(v, 5, room, lvl, key, &rc)) continue;
                        if (rc >= 0) v[rc].state = 0, v[rc].active = 0, v[rc].ours = 0;
                        for (r2 = 0; r2 < DROP_REC_N && v[r2].state; ++r2) {}
                        if (r2 == DROP_REC_N) break;
                        v[r2].state = 1, v[r2].active = 1, v[r2].ours = 1, v[r2].stage = 5, v[r2].room = room, v[r2].tier = DRT_WEAK;
                        v[r2].key = key, v[r2].seq = ++given;
                        for (freeN = 0, ours = 0, rc = 0; rc < DROP_REC_N; ++rc) {
                            freeN += !v[rc].state;
                            ours += v[rc].ours && v[rc].stage == 5 && v[rc].room == room;
                        }
                        if (freeN < minFree) minFree = freeN;
                        if (ours > maxRoom) maxRoom = ours;
                    }
                sprintf(b, "copies: %s, %d game records, 120 copy kills in 10 rooms all dropping: %d drops given, never fewer than %d "
                           "free records (>= 16), at most %d of ours in one room", kDropLevelName[lvl], game, given,
                        minFree, maxRoom);
                Check((given == 0 || minFree >= 16) && maxRoom <= kDropRates[lvl].roomCap, b);  // v3.35 GH335: 4 a room
            }
    }
}

// v3.33 EN333: lives (a revived monster rolls anew), the ledger check, the logged chance, the kill without credit
static void TestLives(void)
{
    static DropLives L;
    const unsigned k = DropKeyKill(2, 0x02040001u, 0x0A, 0), seed = 0x0A23E80Cu;
    int lives[6], i, okLife = 1, distinct = 1, j;
    unsigned keys[6];
    char b_lives[256];
    DropLivesReset(&L);
    for (i = 0; i < 6; ++i) lives[i] = DropLifeNext(&L, k), keys[i] = DropLifeKey(k, lives[i]);
    for (i = 0; i < 6; ++i) okLife &= lives[i] == i;
    for (i = 0; i < 6; ++i)
        for (j = i + 1; j < 6; ++j) distinct &= keys[i] != keys[j];
    Check(okLife && keys[0] == k && distinct, "lives: the 2:4 Sniffer revived 5 times: lives 0..5, life 0 keeps the v3.29 key, every life its own key");
    {   // the user's save 02: with one key per placement every revived death repeated the first roll; now about 1 in 4
        int n = 0, any = 0;
        for (i = 0; i < 4000; ++i) {
            const unsigned kk = DropKeyKill(2, 0x02040001u + (unsigned)(i % 7), 0x0A, 0);
            DropLives M;
            int l, wins = 0;
            DropLivesReset(&M);
            for (l = 0; l < 4; ++l) wins += DropDecide(DROP_ABUNDANT, DRT_WEAK, DropRollKey(DropLifeKey(kk, DropLifeNext(&M, kk)), seed + (unsigned)i), 3) != 0;
            n += wins, any += wins > 0;
        }
        sprintf(b_lives, "lives: Abundant, a weak monster killed 4 times a session: %.3f drops a death (0.24 expected), %.2f of the sessions see one (0.67 expected)",
                n / 16000.0, any / 4000.0);
        Check(fabs(n / 16000.0 - 0.24) < 0.02 && fabs(any / 4000.0 - 0.666) < 0.03, b_lives);  // v3.38 DR338c: 24 %
    }
    DropLivesReset(&L);
    Check(DropLifeNext(&L, k) == 0, "lives: a load / world reset counts from 0 again (a drop lying from before the reload still blocks its twin)");
    {   // v3.33 REV333-DROP-001: the table full: nothing is evicted; an identity not in it never gets life 0 (no repeat)
        int okFull = 1, l1, l2;
        DropLivesReset(&L);
        for (i = 0; i < DROP_LIFE_N; ++i) okFull &= DropLifeNext(&L, 0x1000u + (unsigned)i) == 0;
        okFull &= L.n == DROP_LIFE_N;
        okFull &= DropLifeNext(&L, 0x1000u) == 1;  /* the first one is still known: its second life */
        l1 = DropLifeNext(&L, 0x9000u);            /* new after the table filled */
        l2 = DropLifeNext(&L, 0x9000u);            /* the same monster again */
        okFull &= l1 >= DROP_LIFE_N && l2 >= DROP_LIFE_N && l1 != l2 && L.spilled == 2;
        okFull &= DropLifeKey(0x9000u, l1) != 0x9000u && DropLifeKey(0x9000u, l1) != DropLifeKey(0x9000u, l2);
        Check(okFull, "lives: 258 monsters killed since the load: the table keeps 256, a later monster gets a fresh key every death (never its first roll again)");
    }
    {
        DropLedger led;
        memset(&led, 0, sizeof led);
        led.b[1] = 0x04;  // bank 2 save 03: world 2 first visit 4 drops
        Check(DropLedgerSecondVisitBad(&led, 0) == 0 && DropLedgerSecondVisitBad(&led, 1) == 0, "ledger check: first-visit counts are fine");
        led.b[4] = 0x20;
        Check(DropLedgerSecondVisitBad(&led, 0) == (1 << 5) && DropLedgerSecondVisitBad(&led, 1) == 0,
              "ledger check: a second-visit count in a first-half save is flagged (world 5)");
    }
    Check(fabs(DropChance(DROP_ABUNDANT, DRT_WEAK, 0) - 0.229f) < 1e-6 && fabs(DropChance(DROP_NORMAL, DRT_STRONG, 1) - 0.02f) < 1e-6 &&
              fabs(DropChance(DROP_ABUNDANT, DRT_TOP, 2) - 0.06f) < 1e-6 && DropChance(0, DRT_WEAK, 0) == 0.0f &&
              fabs(DropChance(DROP_RARE, DRT_GHOST, 1) - 0.02f) < 1e-6 && DropChance(DROP_LEVELS, DRT_WEAK, 0) == 0.0f,
          "chance: the logged chances are the class table's (v3.38 DR338c: Abundant weak common 22.9 %, Normal strong uncommon 2 %, "
          "Abundant named ghost rare 6 %, Rare ghost uncommon 2 %; Off / a 5th level 0)");
    {   // a death without the player's credit: no roll, said once
        static DropTable t;
        DropTrack *e;
        int other = 0, ev, ev2, other2 = 0;
        DropTableReset(&t);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
        DropObserve2(&t, e, 1, 50.0f, 0x45, 0, &other);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
        ev = DropObserve2(&t, e, 0x0B, 0.0f, 0x5B, 0, &other);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
        ev2 = DropObserve2(&t, e, 0x0B, 0.0f, 0x5B, 0, &other2);
        Check(ev == DRE_NONE && other == 1 && ev2 == DRE_NONE && other2 == 0, "credit: a dog killed by attack 5B (a monster's) gives no roll and is said once");
        DropTableReset(&t);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 7u);
        DropObserve2(&t, e, 1, 50.0f, 0x45, 0, &other);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 7u);
        ev = DropObserve2(&t, e, 0x0B, 0.0f, 0x45, 0, &other);
        Check(ev == DRE_KILL && other == 0, "credit: the same death by Echo's attack 45 rolls (DropObserve2 = DropObserve)");
    }
}

// ---- v3.35 GH335: ghosts drop, anyone's kills roll, drops fade after 2 minutes (no world budget)
static void TestGhosts335(void)
{
    char b[320];
    int i, id;
    // -- which beings roll the ghost chance
    Check(DropKindOf2(0x0B, 3, 0, 0xFFFF0007u, 2, 1, 1, &id) == DRK_GHOST && id < 0 &&
              DropKindOf2(0x0C, 1, 0, 0xFFFF0011u, 6, 1, 1, &id) == DRK_GHOST && DropKindOf2(0x0D, 1, 0, 0xFFFF0012u, 3, 1, 1, &id) == DRK_GHOST,
          "ghosts: the victims' ghosts (0B any, 0C v1, 0D v1, FFFF00nn ids) roll on their downs");
    Check(DropKindOf2(0x11, 0, 0, 0x05040003u, 5, 1, 1, &id) == DRK_GHOST && DropKindOf2(0x11, 0, 0, 0x05040003u, 4, 1, 1, &id) == DRK_NONE,
          "ghosts: the Patient (11) with a placement id of this world rolls; another world's id never");
    Check(DropKindOf2(0x0B, 0, 0, EnmCopyId(9, 2, 1), 2, 1, 1, &id) == DRK_GHOST_COPY && DropKindOf2(0x0B, 0, 0, EnmCopyId(9, 2, 1), 2, 0, 1, &id) == DRK_NONE,
          "ghosts: a victim's copy rolls too (CopyDrops=0: never)");
    Check(DropKindOf2(0x0C, 0, 0, 0xFFFF0002u, 2, 1, 1, &id) == DRK_TOP && id == DI_CYNTHIA &&
              DropKindOf2(0x0E, 0, 0, 0xFFFF0004u, 4, 1, 1, &id) == DRK_TOP && id == DI_ANDREW,
          "ghosts: the named ghosts stay top level (their rare roll first)");
    Check(DropKindOf2(0x0B, 0, 0, 0xFFFF0007u, 2, 1, 0, &id) == DRK_NONE && DropKindOf2(0x0B, 0, 0, 0x60000001u, 2, 1, 1, &id) == DRK_NONE &&
              DropKindOf2(0x0B, 0, 0, 0u, 2, 1, 1, &id) == DRK_NONE && DropKindOf2(0x0A, 0, 0, 0x02020001u, 2, 1, 1, &id) == DRK_KILL,
          "ghosts: GhostDrops=0 never; the cheat table's extras and nest spawns never; monsters as before");
    // -- v3.38 DR338c: the ghost class: c 7.75 / u 4 / r 0.25 % on Normal, 12 % in all, the same on every down (no falloff)
    Check(fabsf(DropGhostChance(DROP_NORMAL, 0) - 0.12f) < 1e-6f && DropGhostChance(DROP_NORMAL, 1) == DropGhostChance(DROP_NORMAL, 0) &&
              DropGhostChance(DROP_NORMAL, 7) == DropGhostChance(DROP_NORMAL, 0) && fabsf(DropGhostChance(DROP_RARE, 3) - 0.06f) < 1e-6f &&
              fabsf(DropGhostChance(DROP_ABUNDANT, 2) - 0.24f) < 1e-6f,
          "ghosts: every down rolls the full ghost chance (Rare 6 %, Normal 12 %, Abundant 24 %; no halving since v3.38 DR338c)");
    {
        static const struct { int level, down; double rate; } kR[] = {  // v3.38 DR338: + Rare; Generous = Abundant
            { DROP_NORMAL, 0, 0.12 }, { DROP_NORMAL, 1, 0.12 }, { DROP_NORMAL, 2, 0.12 }, { DROP_NORMAL, 9, 0.12 },  // v3.38 DR338c: no falloff
            { DROP_ABUNDANT, 0, 0.24 }, { DROP_ABUNDANT, 1, 0.24 }, { DROP_ABUNDANT, 2, 0.24 },
            { DROP_RARE, 0, 0.06 }, { DROP_RARE, 1, 0.06 }, { DROP_RARE, 2, 0.06 } };
        int k, counts[16], mono = 1;
        for (k = 0; k < (int)(sizeof kR / sizeof kR[0]); ++k) {
            const int n = 200000;
            int d = 0, other = 0;
            memset(counts, 0, sizeof counts);
            for (i = 0; i < n; ++i) {
                const int it = DropDecideGhost(kR[k].level, kR[k].down, DropRollKey(DropKeyKill(2, 0xFFFF0000u + (unsigned)i, 0x0B, 0), 0x5EEDu + (unsigned)k), 3);
                if (it) ++d, ++counts[it & 15];
                other += it == 0x06;  // v3.38 DR338b: any group now (common 70 / uncommon 20 / rare 10 %); never a non-drop item
            }
            sprintf(b, "ghosts: %s, down %d of a ghost this world visit: %.2f %% drop (expected %.2f %%)",
                    kDropLevelName[kR[k].level], kR[k].down + 1, 100.0 * d / n, 100.0 * kR[k].rate);
            Check(fabs((double)d / n - kR[k].rate) < 0.004 && !other, b);
        }
        for (i = 0; i < 100000; ++i) {
            const unsigned key = DropKeyKill(3, 0xFFFF0100u + (unsigned)i, 0x0B, 1);
            int dn;
            for (dn = 0; dn < 3; ++dn)
                mono &= (!DropDecideGhost(DROP_NORMAL, dn, key, 3) || DropDecideGhost(DROP_ABUNDANT, dn, key, 3)) &&
                        (!DropDecideGhost(DROP_RARE, dn, key, 3) || DropDecideGhost(DROP_NORMAL, dn, key, 3));  // v3.38 DR338
            mono &= DropDecideGhost(DROP_NORMAL, 1, key, 3) == DropDecideGhost(DROP_NORMAL, 0, key, 3);  // v3.38 DR338c: no falloff
        }
        Check(mono, "ghosts: every Rare ghost drop is a Normal one, every Normal one an Abundant one; a later down rolls as the first");
        Check(DropDecideGhost(0, 0, 123u, 3) == 0 && DropGhostChance(0, 0) == 0.0f, "ghosts: Off never");
    }
    {   // the downs of one ghost, counted per world visit
        static DropGhostDowns g;
        int okSeq = 1, k;
        DropGhostDownsReset(&g);
        for (k = 0; k < 5; ++k) okSeq &= DropGhostDownNext(&g, 2, 0, 0xFFFF0007u) == k;
        okSeq &= DropGhostDownNext(&g, 2, 0, 0xFFFF0008u) == 0;           // another ghost: its own count
        okSeq &= DropGhostDownNext(&g, 2, 0, EnmCopyId(9, 2, 1)) == 0;    // its copy: its own count
        Check(okSeq, "ghosts: a ghost's downs count 1, 2, 3 ... in a world visit (each ghost and each copy on its own)");
        Check(DropGhostDownNext(&g, 2, 1, 0xFFFF0007u) == 0 && DropGhostDownNext(&g, 3, 1, 0xFFFF0007u) == 0,
              "ghosts: the second visit and another world start afresh");
        DropGhostDownsReset(&g);
        Check(DropGhostDownNext(&g, 3, 1, 0xFFFF0007u) == 0, "ghosts: a load / world reset starts afresh");
        for (k = 0; k < DROP_GHOST_N; ++k) DropGhostDownNext(&g, 4, 0, 0x1000u + (unsigned)k);
        Check(DropGhostDownNext(&g, 4, 0, 0x9999u) == 2 && g.spill == 1, "ghosts: 64 ghosts downed: a 65th is counted as a 3rd down (the log only: no falloff since v3.38 DR338c)");
        Check(DropGhostBeingKey(0, DI_JASPER, 0xFFFF0003u) == (0x70000000u | DI_JASPER) && DropGhostBeingKey(1, DI_JASPER, 0x1E001C05u) == 0x1E001C05u &&
                  DropGhostBeingKey(0, -1, 0xFFFF0007u) == 0xFFFF0007u, "ghosts: a named ghost counts by its identity, a copy by its id");
        Check(DropTopIsGhost(DI_CYNTHIA) && DropTopIsGhost(DI_RICHARD) && !DropTopIsGhost(DI_WALTER) && !DropTopIsGhost(-1),
              "ghosts: Walter (the stalker) is not a ghost (his own class, v3.38 DR338c)");
    }
    {   // v3.38 DR338c: a named ghost: one roll of its class (5 / 4 / 3 % on Normal), no second roll
        const int n = 200000;
        int d = 0, rare = 0;
        for (i = 0; i < n; ++i) {
            const unsigned key = DropRollKey(DropKeyTop(DI_CYNTHIA, 5, 1, i), 0xABCDu);
            const int it = DropDecide(DROP_NORMAL, DRT_TOP, key, 3);
            rare += it != 0 && DropGroupOf(it) == DRG_RARE;
            d += it != 0;
        }
        sprintf(b, "ghosts: Cynthia's down at Normal: %.2f %% rare, %.2f %% any (expected 3 %% and 12 %%)", 100.0 * rare / n, 100.0 * d / n);
        Check(fabs((double)rare / n - 0.03) < 0.002 && fabs((double)d / n - 0.12) < 0.004, b);
    }
    Check(DropTierRank(DRT_GHOST) == DropTierRank(DRT_WEAK) && DropTierRank(DRT_STRONG) > DropTierRank(DRT_GHOST) &&
              !strcmp(kDropTierName[DRT_GHOST], "ghost"), "ghosts: a ghost's drop ranks as a weak one (named 'ghost')");
    {   // one roll per down: a re-down / stumble inside the same down never rolls again
        static DropTable t;
        DropTrack *e;
        static const struct { int cls; float hp; } kSeq[] = { { 1, 300.0f }, { 0x0A, 120.0f }, { 0x0A, 0.0f }, { 0x0A, 0.0f }, { 9, 0.0f },
                                                              { 0x0A, 0.0f }, { 8, 40.0f }, { 1, 150.0f }, { 0x0A, 0.0f } };
        int ev[9], k;
        DropTableReset(&t);
        for (k = 0; k < 9; ++k) {
            ++t.tick;
            e = DropTrackGet(&t, (void *)5, (void *)6, 0xFFFF0007u);
            ev[k] = DropObserve3(&t, e, kSeq[k].cls, kSeq[k].hp, 0x10, 1, 1, NULL);
        }
        Check(ev[0] == DRE_NONE && ev[1] == DRE_NONE && ev[2] == DRE_DOWN && ev[3] == DRE_NONE && ev[4] == DRE_NONE &&
                  ev[5] == DRE_NONE && ev[6] == DRE_NONE && ev[7] == DRE_NONE && ev[8] == DRE_DOWN,
              "ghosts: a stumble (HP > 0) does not roll; the down at HP 0 rolls once; hits while it lies never; up and down again: a new roll");
    }
    // -- anyone's kills
    {
        static DropTable t;
        DropTrack *e;
        int other = 0, ev, k;
        static const int kBy[] = { 0x05, 0x40, 0x45, 0x5B, 0, -1 };
        for (k = 0; k < 6; ++k) {
            DropTableReset(&t);
            ++t.tick;
            e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
            DropObserve3(&t, e, 1, 50.0f, kBy[k], 0, 1, &other);
            ++t.tick;
            e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
            ev = DropObserve3(&t, e, 0x0B, 0.0f, kBy[k], 0, 1, &other);
            sprintf(b, "anyone: a dog killed with attack %02X (%s) rolls once (said as not yours: %d)", kBy[k] & 0xFF, DropCreditName(kBy[k]), other);
            Check(ev == DRE_KILL && other == !DropPlayerAttack(kBy[k]), b);
            ++t.tick;
            e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
            Check(DropObserve3(&t, e, 0x0B, 0.0f, kBy[k], 0, 1, &other) == DRE_NONE, "anyone: the body lying on: no second roll");
        }
        DropTableReset(&t);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 7u);
        Check(DropObserve3(&t, e, 0x0B, 0.0f, 0x5B, 0, 1, &other) == DRE_NONE, "anyone: a dead record restored (first seen dead) never rolls");
        ++t.tick;
        e = DropTrackGet(&t, (void *)3, (void *)4, 8u);
        DropObserve3(&t, e, 1, 50.0f, 0x5B, 0, 1, &other);
        ++t.tick;
        e = DropTrackGet(&t, (void *)3, (void *)4, 8u);
        Check(DropObserve3(&t, e, 0x10, 50.0f, 0x5B, 0, 1, &other) == DRE_NONE, "anyone: a scripted / story class (>= 0x10) never rolls");
        ++t.tick;
        e = DropTrackGet(&t, (void *)9, (void *)9, 9u);
        DropObserve3(&t, e, 1, 50.0f, 0x5B, 0, 1, &other);
        t.tick += 2;  // a gap (a menu, an event, a room change: the tracker resets or the tick is not consecutive)
        e = DropTrackGet(&t, (void *)9, (void *)9, 9u);
        Check(DropObserve3(&t, e, 0x0B, 0.0f, 0x5B, 0, 1, &other) == DRE_NONE, "anyone: a death across a gap (not gameplay ticks) never rolls");
        DropTableReset(&t);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
        DropObserve3(&t, e, 1, 50.0f, 0x5B, 0, 0, &other);
        ++t.tick;
        e = DropTrackGet(&t, (void *)1, (void *)2, 0x02020002u);
        Check(DropObserve3(&t, e, 0x0B, 0.0f, 0x5B, 0, 0, &other) == DRE_NONE && other == 1, "anyone: AnyKills=0 = v3.34 (the player only)");
    }
    // -- the drops' age: 120 s of gameplay, at any frame rate; pauses do not count; the record keeps it
    {
        static const int kHz[4] = { 30, 60, 120, 180 };
        int h, ok = 1, okPause = 1, okBlink = 1;
        for (h = 0; h < 4; ++h) {
            unsigned us = DropAgeUs(0u);  // a fresh drop (no tag yet)
            int tick, at = -1, off = 0, beats = 0, last = 0;
            for (tick = 0; tick < kHz[h] * 200 && at < 0; ++tick) {
                const int playing = !(tick >= kHz[h] * 30 && tick < kHz[h] * 50);  // a 20 s pause / menu at 30 s
                const unsigned before = us;
                us = DropAgeWord(DropAgeStep(us, playing, kHz[h]), 0) & DROP_AGE_US;  // through the record's word (a save)
                if (!playing) okPause &= us == before;
                if (DropAgeExpired(us)) at = tick;
                if (DropAgeBlinkOff(us)) ++off;
                if (DropAgeBlinkOff(us) != last) beats += !last, last = DropAgeBlinkOff(us);
                if (us < 105000000u) okBlink &= !DropAgeBlinkOff(us);
            }
            sprintf(b, "age: at %d Hz a drop fades at %.3f s of play + 20 s paused (expected 140, within a tick); it blinked %d times, "
                       "off %.1f of its last 15 s", kHz[h], (double)at / kHz[h], beats, (double)off / kHz[h]);
            ok = fabs((double)at / kHz[h] - 140.0) <= 1.5 / kHz[h] + 0.01 && beats >= 39 && beats <= 41 && fabs((double)off / kHz[h] - 6.0) < 0.2;
            Check(ok && okPause && okBlink, b);
        }
        Check(DropAgeUs(0u) == 0 && DropAgeUs(0x00001234u) == 0 && DropAgeUs(0x80000000u | 5000000u) == 0,
              "age: a record without our tag (a v3.34 drop, an old save, a Sword record's owner word) starts a fresh 2 minutes");
        Check(DropAgeUs(DropAgeWord(61000000u, 1)) == 61000000u && DropAgeTagged(DropAgeWord(61000000u, 1)) &&
                  (DropAgeWord(61000000u, 1) & DROP_AGE_OFF) && !(DropAgeWord(61000000u, 0) & DROP_AGE_OFF),
              "age: the word in the record (+0x70) keeps 61 s through a save and a load; the blink's off bit apart");
        Check(DropAgeStep(DropAgeStep(0, 1, 0), 1, 999) == 0 && DropAgeStep(130000000u, 1, 60) == 121000000u && !DropAgeBlinkOff(120000000u),
              "age: a junk rate does not age; the age stops at 121 s (a drop the player stands on waits); no blink once expired");
    }
}

// v3.37 MN337: the caps 64 anywhere / 10 a room with the moved pool (128 records), the stock pool's limit (32 records:
// the 16 free keep ours at 16), farming at the caps (a pickup / fade frees a place at once) and the 12-bit drop number
// renumbered before it wraps
static void TestCaps337(void)
{
    static DropRecView v[DROP_REC_MAX];
    char b[256];
    int lvl, game, recycle, j, k, rc;
    for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl)  // v3.38 DR338: + Rare (the same caps)
        for (game = 0; game <= 32; game += 16) {  // the game's own records: none, 16, 32 (the worst seen)
            int given = 0, maxRoom = 0, maxOurs = 0, minFree = 999, ours, freeN, here[32];
            g_dropPureRecN = 128;
            memset(v, 0, sizeof v);
            memset(here, 0, sizeof here);
            for (j = 0; j < game; ++j) v[j].state = 1, v[j].active = 1;
            for (k = 0; k < 3000; ++k) {  // kills in 30 rooms (15 in a row in each), every one dropping: no pickups, no fades
                const int rm = (k / 15) % 30;
                int r2;
                if (!DropPoolPlan(v, 4, rm, lvl, 0x40000000u + (unsigned)k, &recycle)) continue;
                if (recycle >= 0) --here[v[recycle].room], v[recycle].state = 0, v[recycle].active = 0, v[recycle].ours = 0;
                for (r2 = 0; r2 < DROP_REC_N && v[r2].state; ++r2) {}
                if (r2 == DROP_REC_N) break;
                v[r2].state = 1, v[r2].active = 1, v[r2].ours = 1, v[r2].stage = 4, v[r2].room = rm, v[r2].tier = DRT_WEAK;
                v[r2].key = 0x40000000u + (unsigned)k, v[r2].seq = ++given;
                if (++here[rm] > maxRoom) maxRoom = here[rm];
                for (ours = 0, freeN = 0, rc = 0; rc < DROP_REC_N; ++rc) ours += v[rc].ours, freeN += !v[rc].state;
                if (ours > maxOurs) maxOurs = ours;
                if (freeN < minFree) minFree = freeN;
            }
            sprintf(b, "caps: %s, %d game records, 3000 kills in 30 rooms all dropping (128-record pool): at most %d lying "
                       "(cap 64), %d in one room (cap 10), never fewer than %d free (>= 16)", kDropLevelName[lvl],
                    game, maxOurs, maxRoom, minFree);
            Check(maxOurs == (game + 64 + 16 <= 128 ? 64 : 128 - game - 16) && maxRoom == 10 && minFree >= 16, b);
        }
    {   // the stock pool (PickupPool=0): 32 records, 16 kept free: at most 16 of ours, as before
        int ours = 0, n;
        g_dropPureRecN = 32;
        memset(v, 0, sizeof v);
        for (n = 0; n < 100; ++n) {
            int r2;
            if (!DropPoolPlan(v, 5, n % 7, DROP_ABUNDANT, 0x50000000u + (unsigned)n, &recycle)) continue;
            for (r2 = 0; r2 < DROP_REC_N && v[r2].state; ++r2) {}
            v[r2].state = 1, v[r2].active = 1, v[r2].ours = 1, v[r2].stage = 5, v[r2].room = n % 7, v[r2].tier = DRT_WEAK;
            v[r2].key = 0x50000000u + (unsigned)n, v[r2].seq = n + 1, ++ours;
        }
        Check(ours == 16, "caps: the stock pool (32 records): 16 drops lie at most (the 16 kept free come first)");
    }
    {   // farming at the cap: every pickup frees a place at once, the next kill drops again
        int ok = 1, n, taken = 0;
        g_dropPureRecN = 128;
        memset(v, 0, sizeof v);
        for (j = 0; j < 64; ++j)
            v[j].state = 1, v[j].active = 1, v[j].ours = 1, v[j].stage = 3, v[j].room = j % 8, v[j].tier = DRT_TOP, v[j].key = 900u + j, v[j].seq = j + 1;
        ok &= !DropPoolPlan(v, 3, 9, DROP_NORMAL, 5000u, &recycle);  // 64 rare drops lying: none may make way
        for (n = 0; n < 200; ++n) {
            const int t = n % 64, rm = 9 + n % 40;  // the player takes one (PickupRecordClear: state / active 0) ...
            ok &= !DropPoolPlan(v, 3, rm, DROP_NORMAL, 6000u + (unsigned)n, &recycle);  // (at the cap: no drop before it)
            v[t].state = 0, v[t].active = 0, v[t].ours = 0, ++taken;
            ok &= DropPoolPlan(v, 3, rm, DROP_NORMAL, 6000u + (unsigned)n, &recycle) && recycle < 0;  // ... and the next kill drops
            v[t].state = 1, v[t].active = 1, v[t].ours = 1, v[t].stage = 3, v[t].room = rm, v[t].tier = DRT_TOP;
            v[t].key = 6000u + (unsigned)n, v[t].seq = 65 + n;
        }
        sprintf(b, "caps: farming at the cap: %d pickups, each frees its place at once for the next drop (no budget)", taken);
        Check(ok, b);
    }
    {   // the drop number: renumbered 1..n in order before it reaches 0xF00, never needed below it
        int newSeq[DROP_REC_MAX], n, okOrder = 1, okKeep, seqTop = 0, maxSeq, renum = 0;
        g_dropPureRecN = 128;
        memset(v, 0, sizeof v);
        for (j = 0; j < 40; ++j) v[j * 3].state = 1, v[j * 3].active = 1, v[j * 3].ours = 1, v[j * 3].seq = 100 + 7 * j;
        v[1].state = 1, v[1].active = 1;  // one of the game's
        Check(DropSeqRenumber(v, DROP_REC_N, newSeq) == 0, "seq: highest 373: nothing to renumber");
        for (j = 0; j < 40; ++j) v[j * 3].seq = 0xF00 + ((j * 17) % 40);  // near the top, shuffled
        v[3].seq = v[6].seq;                                             // a tie: the lower record first
        n = DropSeqRenumber(v, DROP_REC_N, newSeq);
        for (j = 0; j < 128; ++j)
            for (k = 0; k < 128; ++k)
                if (v[j].ours && v[k].ours && j != k && (v[j].seq < v[k].seq || (v[j].seq == v[k].seq && j < k)) && !(newSeq[j] < newSeq[k])) okOrder = 0;
        okKeep = newSeq[1] == 0 && newSeq[2] == 0;
        for (j = 0, k = 0; j < 128; ++j) k = newSeq[j] > k ? newSeq[j] : k;
        Check(n == 40 && okOrder && okKeep && k == 40, "seq: near 0xF00: the 40 lying drops become 1..40 in their order (ties: the lower record), the game's untouched");
        // endless farming: 64 lying, the oldest taken and a new one given 20000 times: the number stays below 4096 and the
        // newest is always the highest (the recycle and the save's newest-first keep working)
        memset(v, 0, sizeof v);
        for (j = 0; j < 64; ++j) v[j].state = 1, v[j].active = 1, v[j].ours = 1, v[j].seq = j + 1;
        for (n = 0; n < 20000; ++n) {
            int oldest = -1, i;
            for (i = 0; i < 64; ++i) if (oldest < 0 || v[i].seq < v[oldest].seq) oldest = i;
            for (i = 0, maxSeq = 0; i < 64; ++i) maxSeq = v[i].seq > maxSeq ? v[i].seq : maxSeq;
            if (maxSeq >= DROP_SEQ_RENUM && DropSeqRenumber(v, DROP_REC_N, newSeq) > 0) {
                for (i = 0; i < 64; ++i) v[i].seq = newSeq[i];
                for (i = 0, maxSeq = 0; i < 64; ++i) maxSeq = v[i].seq > maxSeq ? v[i].seq : maxSeq;
                ++renum;
            }
            v[oldest].seq = (maxSeq + 1) & 0xFFF;  // as DropOnEvent's marker word
            if (v[oldest].seq == 0 || v[oldest].seq <= maxSeq) okOrder = 0;
            if (v[oldest].seq > seqTop) seqTop = v[oldest].seq;
        }
        sprintf(b, "seq: 20000 drops through 64 places: renumbered %d times, the highest number %d (< 4096), every new drop "
                   "the newest", renum, seqTop);
        Check(okOrder && seqTop < 4096 && renum > 0, b);
    }
    g_dropPureRecN = 32;
}

// v3.38 DR338 / DR338c (staging/v338_feedback/w3_drops/PLAN.md, BUILD_NOTES "Follow-up c"): the four levels Off / Rare /
// Normal / Abundant, the ini names ("Generous" reads as Abundant), and the user's FINAL design: one roll a kill / full
// down by class (weak, strong, ghost, named ghost, Walter), Rare x0.5, Abundant x2, shares common 1:1:1, uncommon Kit 2 :
// Candle 1, rare 1:1:1:1, the owned-gun rule; every item's exact chance per class and level, with and without each gun;
// no falloff for a ghost's later downs; the keys / salts / seed of v3.37.
static int SharePick(const DropItem *t, int nItems, int owned, int *cnt)  // DropPick over 360000 evenly spaced u
{
    int i, n = 0;
    memset(cnt, 0, sizeof(int) * 16);
    for (i = 0; i < 360000; ++i) {
        const int it = DropPick(t, nItems, ((float)i + 0.5f) / 360000.0f, owned);
        if (it) ++cnt[it & 15], ++n;
    }
    return n;
}
static int ShareIs(const int *cnt, int n, int item, double want) { return fabs((double)cnt[item & 15] / n - want) < 0.0005; }
static const int kAllItems[10] = { 0x09, 0x01, 0x02, 0x0B, 0x04, 0x08, 0x0A, 0x03, 0x05, 0x06 };  // (0x06: never a drop)
static const int kOwnSets[4] = { DROP_NEED_PISTOL | DROP_NEED_REVOLVER, DROP_NEED_PISTOL, DROP_NEED_REVOLVER, 0 };
static const char *const kOwnName[4] = { "both guns", "the pistol only", "the revolver only", "no gun" };
// the expected chance of one item from one roll, written out from the user's table (independent of DropItemChance)
static double ExpectItem(int level, int cls, int item, int owned)
{
    static const double kN[5][3] = { { 0.1145, 0.005, 0.0005 }, { 0.099, 0.02, 0.001 }, { 0.0775, 0.04, 0.0025 },
                                     { 0.05, 0.04, 0.03 }, { 0.07, 0.04, 0.01 } };
    const double m = level == DROP_RARE ? 0.5 : level == DROP_NORMAL ? 1.0 : level == DROP_ABUNDANT ? 2.0 : 0.0;
    const int p = (owned & DROP_NEED_PISTOL) != 0, rv = (owned & DROP_NEED_REVOLVER) != 0;
    const double c = kN[cls][0] * m, u = kN[cls][1] * m, r = kN[cls][2] * m;
    switch (item) {
    case 0x09: return c / (1 + p + rv);
    case 0x01: return p ? c / (1 + p + rv) : 0.0;
    case 0x02: return rv ? c / (1 + p + rv) : 0.0;
    case 0x0B: return u * 2.0 / 3.0;
    case 0x04: return u / 3.0;
    case 0x08: case 0x0A: case 0x05: return r / (3 + p);
    case 0x03: return p ? r / (3 + p) : 0.0;
    }
    return 0.0;
}
static void TestLevels338(void)
{
    char b[400];
    int lvl, i, c;
    // -- the table (the user's final design)
    {
        static const double kN[5][3] = { { 11.45, 0.5, 0.05 }, { 9.9, 2.0, 0.1 }, { 7.75, 4.0, 0.25 }, { 5.0, 4.0, 3.0 }, { 7.0, 4.0, 1.0 } };
        int ok = DROP_LEVELS == 4 && DROP_OFF == 0 && DROP_RARE == 1 && DROP_NORMAL == 2 && DROP_ABUNDANT == 3 && DRC_N == 5 && DRG_N == 3;
        Check(ok, "levels: 0 Off, 1 Rare, 2 Normal, 3 Abundant (the Options row's order); 5 classes, 3 groups");
        for (c = 0, ok = 1; c < DRC_N; ++c)
            for (i = 0; i < DRG_N; ++i) ok &= fabs(DropClassChance(DROP_NORMAL, c, i) * 100.0 - kN[c][i]) < 1e-5;
        Check(ok, "classes on Normal (c / u / r %): weak 11.45 / 0.5 / 0.05, strong 9.9 / 2 / 0.1, ghost 7.75 / 4 / 0.25, named ghost 5 / 4 / 3, Walter 7 / 4 / 1");
        for (c = 0, ok = 1; c < DRC_N; ++c)
            for (i = 0; i < DRG_N; ++i)
                ok &= fabsf(DropClassChance(DROP_RARE, c, i) - 0.5f * DropClassChance(DROP_NORMAL, c, i)) < 1e-8f &&
                      fabsf(DropClassChance(DROP_ABUNDANT, c, i) - 2.0f * DropClassChance(DROP_NORMAL, c, i)) < 1e-8f &&
                      DropClassChance(DROP_OFF, c, i) == 0.0f && DropClassChance(DROP_LEVELS, c, i) == 0.0f;
        Check(ok, "levels: Rare = x0.5 and Abundant = x2 of every class's every group; Off and an out-of-range level: 0");
        for (c = 0, ok = 1; c < DRC_N; ++c)
            ok &= fabsf(DropClassAny(DROP_NORMAL, c) - 0.12f) < 1e-6f && fabsf(DropClassAny(DROP_RARE, c) - 0.06f) < 1e-6f &&
                  fabsf(DropClassAny(DROP_ABUNDANT, c) - 0.24f) < 1e-6f;
        Check(ok && kDropRates[DROP_RARE].common == 0.06f && kDropRates[DROP_NORMAL].common == 0.12f && kDropRates[DROP_ABUNDANT].common == 0.24f,
              "levels: every class gives any item 6 / 12 / 24 % a roll (kDropRates.common, loghealth's no-drop check, says the same)");
        for (lvl = DROP_RARE, ok = 1; lvl <= DROP_ABUNDANT; ++lvl)
            ok &= kDropRates[lvl].lyingCap == 64 && kDropRates[lvl].roomCap == 10 && kDropRates[lvl].minFree == 16;
        Check(ok && DROP_AGE_S == 120.0f && kDropRates[DROP_RARE].worldBudget == 4 && kDropRates[DROP_NORMAL].worldBudget == 8 &&
                  kDropRates[DROP_ABUNDANT].worldBudget == 15, "levels: the same caps (64 / 10 / 16 free) and the 2-minute fade on every level");
        Check(DropClassOfTier(DRT_WEAK, -1) == DRC_WEAK && DropClassOfTier(DRT_STRONG, -1) == DRC_STRONG && DropClassOfTier(DRT_GHOST, -1) == DRC_GHOST &&
                  DropClassOfTier(DRT_TOP, DI_CYNTHIA) == DRC_NAMED && DropClassOfTier(DRT_TOP, DI_RICHARD) == DRC_NAMED &&
                  DropClassOfTier(DRT_TOP, DI_WALTER) == DRC_WALTER && DropClassOfTier(DRT_NONE, -1) < 0,
              "classes: weak / strong monsters, ghosts, the named ghosts (and their copies: same identity), Walter his own");
    }
    // -- the groups and their shares, exact (DropPick over evenly spaced rolls), with and without each gun
    {
        int cnt[16], k, n;
        Check(DROP_N_COMMON == 3 && DROP_N_UNCOMMON == 2 && DROP_N_RARE == 4 && kDropCommon[0].item == 0x09 && kDropCommon[1].item == 0x01 &&
                  kDropCommon[2].item == 0x02 && kDropUncommon[0].item == 0x0B && kDropUncommon[1].item == 0x04 && kDropRare[0].item == 0x08 &&
                  kDropRare[1].item == 0x0A && kDropRare[2].item == 0x03 && kDropRare[3].item == 0x05 &&
                  kDropCommon[1].need == DROP_NEED_PISTOL && kDropCommon[2].need == DROP_NEED_REVOLVER && kDropRare[2].need == DROP_NEED_PISTOL &&
                  !kDropCommon[0].need && !kDropUncommon[0].need && !kDropUncommon[1].need && !kDropRare[0].need && !kDropRare[1].need && !kDropRare[3].need &&
                  kDropUncommon[0].weight == 2 && kDropUncommon[1].weight == 1,
              "groups: common Drink / Pistol / Revolver 1:1:1, uncommon Kit 2 : Candle 1, rare Medallion / Ampoule / Silver / Sword 1:1:1:1; "
              "Pistol and Silver need the pistol, Revolver the revolver");
        for (k = 0; k < 4; ++k) {
            const int ow = kOwnSets[k], p = (ow & DROP_NEED_PISTOL) != 0, rv = (ow & DROP_NEED_REVOLVER) != 0;
            const double cShare = 1.0 / (1 + p + rv), rShare = 1.0 / (3 + p);
            n = SharePick(kDropCommon, DROP_N_COMMON, ow, cnt);
            sprintf(b, "shares: common with %s: Drink %.4f, Pistol %.4f, Revolver %.4f", kOwnName[k], (double)cnt[9] / n, (double)cnt[1] / n, (double)cnt[2] / n);
            Check(n == 360000 && ShareIs(cnt, n, 0x09, cShare) && ShareIs(cnt, n, 0x01, p ? cShare : 0.0) && ShareIs(cnt, n, 0x02, rv ? cShare : 0.0), b);
            n = SharePick(kDropUncommon, DROP_N_UNCOMMON, ow, cnt);
            sprintf(b, "shares: uncommon with %s: Medical Kit %.4f, Holy Candle %.4f (2 : 1)", kOwnName[k], (double)cnt[0xB] / n, (double)cnt[4] / n);
            Check(n == 360000 && ShareIs(cnt, n, 0x0B, 2.0 / 3) && ShareIs(cnt, n, 0x04, 1.0 / 3), b);
            n = SharePick(kDropRare, DROP_N_RARE, ow, cnt);
            sprintf(b, "shares: rare with %s: Medallion %.4f, Ampoule %.4f, Silver %.4f, Sword %.4f", kOwnName[k], (double)cnt[8] / n,
                    (double)cnt[0xA] / n, (double)cnt[3] / n, (double)cnt[5] / n);
            Check(n == 360000 && ShareIs(cnt, n, 0x08, rShare) && ShareIs(cnt, n, 0x0A, rShare) && ShareIs(cnt, n, 0x05, rShare) &&
                      ShareIs(cnt, n, 0x03, p ? rShare : 0.0), b);
        }
    }
    // -- every item's exact chance: DropItemChance = the user's table written out, every class x level x gun set x item
    {
        int ok = 1, nPins = 0, o;
        double worst = 0.0;
        for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl)
            for (c = 0; c < DRC_N; ++c)
                for (o = 0; o < 4; ++o) {
                    double sum = 0.0;
                    for (i = 0; i < 10; ++i) {
                        const double got = DropItemChance(lvl, c, kAllItems[i], kOwnSets[o]), want = ExpectItem(lvl, c, kAllItems[i], kOwnSets[o]);
                        if (fabs(got - want) > worst) worst = fabs(got - want);
                        ok &= fabs(got - want) < 1e-8;
                        sum += got, ++nPins;
                    }
                    ok &= fabs(sum - 0.12 * kDropRates[lvl].mult) < 1e-7;
                }
        sprintf(b, "chances: %d item chances (3 levels x 5 classes x 4 gun sets x 10 items) equal the user's table (worst %.1e); "
                   "each set adds up to 6 / 12 / 24 %%", nPins, worst);
        Check(ok, b);
        // a few by hand (Normal, all guns unless said)
        Check(fabs(DropItemChance(DROP_NORMAL, DRC_WEAK, 0x09, 3) - 0.1145 / 3) < 1e-8 && fabs(DropItemChance(DROP_NORMAL, DRC_WEAK, 0x0B, 3) - 0.005 * 2 / 3) < 1e-8 &&
                  fabs(DropItemChance(DROP_NORMAL, DRC_WEAK, 0x03, 3) - 0.0005 / 4) < 1e-9 && fabs(DropItemChance(DROP_NORMAL, DRC_STRONG, 0x04, 3) - 0.02 / 3) < 1e-8 &&
                  fabs(DropItemChance(DROP_NORMAL, DRC_GHOST, 0x05, 3) - 0.0025 / 4) < 1e-9 && fabs(DropItemChance(DROP_NORMAL, DRC_NAMED, 0x08, 3) - 0.0075) < 1e-8 &&
                  fabs(DropItemChance(DROP_NORMAL, DRC_NAMED, 0x08, 2) - 0.01) < 1e-8 && fabs(DropItemChance(DROP_NORMAL, DRC_WALTER, 0x0A, 3) - 0.0025) < 1e-8 &&
                  fabs(DropItemChance(DROP_ABUNDANT, DRC_NAMED, 0x03, 3) - 0.015) < 1e-8 && DropItemChance(DROP_ABUNDANT, DRC_NAMED, 0x03, 2) == 0.0 &&
                  fabs(DropItemChance(DROP_RARE, DRC_GHOST, 0x09, 0) - 0.03875) < 1e-8 && fabs(DropItemChance(DROP_ABUNDANT, DRC_STRONG, 0x02, 2) - 0.099) < 1e-8,
              "chances by hand: Normal weak Drink 3.817 %, Kit 0.333 %, Silver 0.0125 %; strong Candle 0.667 %; ghost Sword 0.0625 %; "
              "named Medallion 0.75 % (1 % without the pistol); Walter Ampoule 0.25 %; Abundant named Silver 1.5 % (0 without the pistol); "
              "Rare ghost Drink with no gun 3.875 %; Abundant strong Revolver with the revolver only 9.9 %");
    }
    // -- the real rolls (DropDecideClass) give each item at its exact chance: every level x class x gun set, 400000 rolls
    {
        int o, okAll = 1, never = 1;
        double worstZ = 0.0;
        for (lvl = DROP_RARE; lvl <= DROP_ABUNDANT; ++lvl)
            for (c = 0; c < DRC_N; ++c)
                for (o = 0; o < 4; ++o) {
                    const int N = 400000;
                    int cnt[16], j;
                    memset(cnt, 0, sizeof cnt);
                    for (j = 0; j < N; ++j) {
                        const unsigned key = DropRollKey(DropKeyKill(2 + j % 7, 0x02000000u + (unsigned)j, 7, 0), 0xD338C000u + (unsigned)(lvl * 64 + c * 4 + o));
                        const int it = DropDecideClass(lvl, c, key, kOwnSets[o]);
                        if (it) ++cnt[it & 15];
                    }
                    for (i = 0; i < 10; ++i) {
                        const double p = ExpectItem(lvl, c, kAllItems[i], kOwnSets[o]), sd = sqrt(p * (1.0 - p) / N) + 1e-9;
                        const double z = fabs((double)cnt[kAllItems[i] & 15] / N - p) / sd;
                        if (p == 0.0 && cnt[kAllItems[i] & 15]) never = 0;
                        if (p > 0.0 && z > worstZ) worstZ = z;
                        if (p > 0.0 && z > 4.5) okAll = 0;
                    }
                }
        sprintf(b, "rolls: 3 levels x 5 classes x 4 gun sets x 400000 rolls: every item within tolerance of its exact chance (worst %.2f "
                   "standard deviations); never an item a gun not owned excludes", worstZ);
        Check(okAll && never, b);
    }
    // -- no falloff: a ghost's every down rolls the same full chance (the down number changes nothing)
    {
        int same = 1, n = 0, d;
        for (i = 0; i < 200000; ++i) {
            const unsigned key = DropRollKey(DropKeyKill(3, 0xFFFF0000u + (unsigned)i, 0x0B, 0), 0x0FA11u);
            const int it0 = DropDecideGhost(DROP_NORMAL, 0, key, 3);
            for (d = 1; d < 6; ++d) same &= DropDecideGhost(DROP_NORMAL, d, key, 3) == it0;
            same &= DropDecideGhost(DROP_NORMAL, 255, key, 3) == it0 && DropDecideClass(DROP_NORMAL, DRC_GHOST, key, 3) == it0;
            n += it0 != 0;
        }
        for (d = 0, i = 1; d < 300; ++d) i &= DropGhostChance(DROP_NORMAL, d) == DropClassAny(DROP_NORMAL, DRC_GHOST);
        sprintf(b, "no falloff: a ghost's downs 1, 2 .. 6 and 256 roll the same (%.2f %% of 200000 first downs drop; 12 %% expected); "
                   "DropGhostChance is the full chance for every down", 100.0 * n / 200000);
        Check(same && i && fabs(n / 200000.0 - 0.12) < 0.003, b);
    }
    // -- a drop's marker tier: a rare-group item never makes way at the lying cap, whatever rolled it
    Check(DropMarkTier(DRT_GHOST, 0x05) == DRT_TOP && DropMarkTier(DRT_STRONG, 0x03) == DRT_TOP && DropMarkTier(DRT_WEAK, 0x08) == DRT_TOP &&
              DropMarkTier(DRT_GHOST, 0x0B) == DRT_STRONG && DropMarkTier(DRT_WEAK, 0x04) == DRT_STRONG && DropMarkTier(DRT_GHOST, 0x09) == DRT_GHOST &&
              DropMarkTier(DRT_WEAK, 0x01) == DRT_WEAK && DropMarkTier(DRT_STRONG, 0x02) == DRT_STRONG && DropGroupOf(0x03) == DRG_RARE &&
              DropGroupOf(0x04) == DRG_UNCOMMON && DropGroupOf(0x02) == DRG_COMMON && DropGroupOf(0x06) < 0,
          "marker: rare items rank as top (never taken away), uncommon as strong, common keep their tier; DropGroupOf");
    // -- the ini: names, "Generous" = Abundant, anything else Off (a number too, as in every build before)
    {
        static const struct { const char *text; int level; } kIni[] = {
            { "Off", 0 }, { "Rare", 1 }, { "Normal", 2 }, { "Abundant", 3 }, { "abundant", 3 }, { "RARE", 1 }, { "Generous", 3 },
            { "generous", 3 }, { "GENEROUS", 3 }, { "", 0 }, { "0", 0 }, { "1", 0 }, { "2", 0 }, { "3", 0 }, { "Generous ", 0 },
            { "Gen", 0 }, { "x2", 0 }, { "Abundant but this text runs on past the menu's 32-byte read", 0 } };
        char buf[32], small[6];
        int ok = 1, k;
        for (k = 0; k < (int)(sizeof kIni / sizeof kIni[0]); ++k) {
            const int v = DropLevelOfText(kIni[k].text);
            if (v != kIni[k].level) {
                printf("      ini \"%s\" -> %d, expected %d\n", kIni[k].text, v, kIni[k].level);
                ok = 0;
            }
        }
        Check(ok && DropLevelOfText(NULL) == 0, "ini: Off / Rare / Normal / Abundant by name (any case); a saved v3.37 \"Generous\" is Abundant; "
                                              "any other text (numbers included) is Off, as before");
        strcpy(buf, "Generous");
        ok = DropIniAlias(buf, sizeof buf) && !strcmp(buf, "Abundant");
        strcpy(buf, "Normal");
        ok &= !DropIniAlias(buf, sizeof buf) && !strcmp(buf, "Normal");
        strcpy(small, "Gene");
        ok &= !DropIniAlias(small, sizeof small) && !strcmp(small, "Gene");
        strcpy(buf, "Generous");
        ok &= !DropIniAlias(buf, 8) && !strcmp(buf, "Generous");  // (too small for "Abundant": left alone, never overrun)
        Check(ok, "ini: DropIniAlias rewrites only \"Generous\" (to \"Abundant\"), never past the buffer");
        Check(!strcmp(kDropLevelName[0], "Off") && !strcmp(kDropLevelName[1], "Rare") && !strcmp(kDropLevelName[2], "Normal") &&
                  !strcmp(kDropLevelName[3], "Abundant"), "ini: the names in the Options row's order");
    }
    // -- the keys / salts / seed are v3.37's: v3.37's own rules recomputed from today's draws give v3.37's pinned patterns;
    // today's roll is the one draw 12 against the class's chance, kill by kill
    {
        // from the v3.37 source (base_src drops.inc, 2026-11-21): Normal / Generous, weak (draw 3), strong (1, 3), top (4), ghost (8)
        static const struct { int level, t, n; unsigned h; } kGold[] = {
            { DROP_NORMAL, 0, 2451, 0xE15C4076u }, { DROP_NORMAL, 1, 3146, 0x35D653B3u }, { DROP_NORMAL, 2, 3962, 0x5342F507u },
            { DROP_NORMAL, 3, 548, 0x861D33FFu }, { DROP_ABUNDANT, 0, 5047, 0xBEA9AE9Au }, { DROP_ABUNDANT, 1, 6541, 0xD3350A7Au },
            { DROP_ABUNDANT, 2, 7989, 0xF5F7717Cu }, { DROP_ABUNDANT, 3, 1143, 0x96926E00u } };
        static const struct { float common, uncommon, rare, ghost; } kV337[2] = { { 0.12f, 0.04f, 0.20f, 0.05f }, { 0.25f, 0.10f, 0.40f, 0.10f } };
        static const int kCls[4] = { DRC_WEAK, DRC_STRONG, DRC_NAMED, DRC_GHOST };
        int k, ok = 1, okNow = 1;
        for (k = 0; k < 8; ++k) {
            const int g = kGold[k].level == DROP_ABUNDANT, t = kGold[k].t;
            unsigned h = 2166136261u;
            int n = 0;
            for (i = 0; i < 20000; ++i) {
                const unsigned key = DropRollKey(DropKeyKill(2 + i % 7, 0x02000000u + (unsigned)i * 13u, 7, 0), 0x0BADF00Du + (unsigned)(i >> 5));
                const float gc = kV337[g].ghost * (i % 3 == 0 ? 1.0f : i % 3 == 1 ? 0.5f : 0.25f);
                const int d = t == 0 ? DropU01(key, 3) < kV337[g].common
                            : t == 1 ? DropU01(key, 1) < kV337[g].uncommon || DropU01(key, 3) < kV337[g].common
                            : t == 2 ? DropU01(key, 4) < kV337[g].rare : DropU01(key, 8) < gc;
                {   // (the sum in the code's own float order: r + n + c)
                    const float r = DropClassChance(kGold[k].level, kCls[t], DRG_RARE), un = DropClassChance(kGold[k].level, kCls[t], DRG_UNCOMMON);
                    const float cc = DropClassChance(kGold[k].level, kCls[t], DRG_COMMON);
                    okNow &= (DropDecideClass(kGold[k].level, kCls[t], key, 3) != 0) == (DropU01(key, 12) < r + un + cc);
                }
                n += d;
                h = (h ^ (unsigned)d) * 16777619u;
            }
            if (n != kGold[k].n || h != kGold[k].h) {
                printf("      %s tier %d: %d drops, hash %08X (v3.37: %d, %08X)\n", kDropLevelName[kGold[k].level], t, n, h, kGold[k].n, kGold[k].h);
                ok = 0;
            }
        }
        Check(ok, "keys: v3.37's rules on today's draws (keys, salts 1 / 3 / 4 / 8, the seed) give v3.37's pinned patterns exactly");
        Check(okNow, "keys: today's roll drops exactly when draw 12 is below the class's r + u + c");
    }
}

int main(void)
{
    char b[256];
    int i;
    TestLevels338();  // v3.38 DR338
    TestLives();  // v3.33 EN333
    TestGhosts335();  // v3.35 GH335
    // ---- gauge and tiers
    for (i = 0; i < (int)(sizeof kRows / sizeof kRows[0]); ++i) {
        const float g = DropGauge(kRows[i].hp, kRows[i].dmg);
        sprintf(b, "gauge: %s %.0f + %.0f -> %.1f %s", kRows[i].name, kRows[i].hp, kRows[i].dmg, g, kDropTierName[kRows[i].tier]);
        Check(DropIsMortalType(kRows[i].type) && DropTierOfGauge(g) == kRows[i].tier, b);
    }
    Check(!DropIsMortalType(2) && !DropIsMortalType(4) && !DropIsMortalType(1) && !DropIsMortalType(0x12),
          "types: Whitestool / Toadstool, the hanging figure and the helpers never drop");
    for (i = 0x0B; i <= 0x11; ++i)
        if (DropIsMortalType(i)) break;
    Check(i == 0x12, "types: ghosts 0x0B-0x0F / 0x11 and Walter 0x10 are not mortal");
    // ---- the top level
    Check(DropTopIdent(0x0C, 0, 0) == DI_CYNTHIA && DropTopIdent(0x0C, 1, 0) < 0, "top: Cynthia = 0x0C v0; Victim 13 (v1) not");
    Check(DropTopIdent(0x0D, 0, 0) == DI_JASPER && DropTopIdent(0x0D, 1, 0) < 0, "top: Jasper 0x0D v0; Victim 04 (v1) not (the user)");
    Check(DropTopIdent(0x0E, 0, 0) == DI_ANDREW && DropTopIdent(0x0F, 0, 0) == DI_RICHARD, "top: Andrew 0x0E, Richard 0x0F");
    Check(DropTopIdent(0x11, 0, 0) < 0 && DropTopIdent(0x0B, 0, 0) < 0 && DropTopIdent(0x0B, 3, 0) < 0,
          "top: the Patient and the other victims' ghosts are not top level");
    Check(DropTopIdent(0x10, 0, 0) == DI_WALTER && DropTopIdent(0x10, 0, 6) == DI_WALTER && DropTopIdent(0x10, 0, 7) < 0 &&
              DropTopIdent(0x10, 1, 8) < 0, "top: the stalker Walter yes; the Conjoined Bodies (7) and the final Walter (8) no");
    Check(DropTopIdent(0x03, 0, 0) < 0 && DropTopIdent(0x08, 1, 0) < 0, "top: monsters are never top level");
    Check(DropPlayerAttack(0x02) && DropPlayerAttack(0x08) && DropPlayerAttack(0x33) && DropPlayerAttack(0x48) &&
              !DropPlayerAttack(0x49) && !DropPlayerAttack(0x56) && !DropPlayerAttack(0x68) && !DropPlayerAttack(0) &&
              !DropPlayerAttack(-1), "credit: Henry / Eileen / Death Presence ids 02-48 yes; monsters' and Walter's no");
    // ---- rolls: determinism and rates
    {
        const unsigned k1 = DropKeyKill(2, 0x02020001u, 0x0A, 0), k2 = DropKeyKill(2, 0x82020001u, 0x0A, 0);
        const unsigned k3 = DropKeyKill(3, 0x02020001u, 0x0A, 0), k4 = DropKeyKill(2, 0x02020002u, 0x0A, 0);
        int same = 1, j;
        for (j = 0; j < 1000; ++j) same &= DropDecide(DROP_NORMAL, DRT_STRONG, k1, 3) == DropDecide(DROP_NORMAL, DRT_STRONG, k1, 3);
        Check(same, "roll: the same kill gives the same item every time (a reload replays it)");
        Check(k1 != k2 && k1 != k3 && k1 != k4, "roll: the second visit (bit 31), another world, another placement: other keys");
        Check(DropKeyTop(DI_CYNTHIA, 2, 1, 0) != DropKeyTop(DI_CYNTHIA, 5, 1, 0) && DropKeyTop(DI_CYNTHIA, 5, 1, 0) != DropKeyTop(DI_JASPER, 5, 1, 0) &&
                  DropKeyTop(DI_CYNTHIA, 5, 1, 0) != DropKeyTop(DI_CYNTHIA, 5, 1, 1) && DropKeyTop(DI_CYNTHIA, 5, 1, 3) == DropKeyTop(DI_CYNTHIA, 5, 1, 3),
              "roll: a top-level down has its own key (identity, world, visit, the saved down count)");
        Check(DropDecide(0, DRT_STRONG, k1, 3) == 0 && DropDecide(0, DRT_TOP, k1, 3) == 0, "roll: Off never drops");
    }
    {
        // v3.38 DR338c: every class gives any item 12 % x the level's factor; the groups' share of the drops by class
        static const struct { int level, tier; double rate, tol; const char *what; } kRate[] = {
            { DROP_NORMAL, DRT_WEAK, 0.12, 0.004, "Normal weak: 12 %" },
            { DROP_NORMAL, DRT_STRONG, 0.12, 0.004, "Normal strong: 12 %" },
            { DROP_NORMAL, DRT_TOP, 0.12, 0.004, "Normal named ghost: 12 %" },
            { DROP_ABUNDANT, DRT_WEAK, 0.24, 0.005, "Abundant weak: 24 %" },
            { DROP_ABUNDANT, DRT_STRONG, 0.24, 0.005, "Abundant strong: 24 %" },
            { DROP_ABUNDANT, DRT_TOP, 0.24, 0.005, "Abundant named ghost: 24 %" },
            { DROP_RARE, DRT_WEAK, 0.06, 0.003, "Rare weak: 6 %" },
            { DROP_RARE, DRT_STRONG, 0.06, 0.003, "Rare strong: 6 %" },
            { DROP_RARE, DRT_TOP, 0.06, 0.003, "Rare named ghost: 6 %" },
        };
        int counts[16], k;
        for (k = 0; k < (int)(sizeof kRate / sizeof kRate[0]); ++k) {
            const int n = 200000, d = RunKills(kRate[k].level, kRate[k].tier, 3, 11u * (unsigned)k, n, counts);
            const int cls = DropClassOfTier(kRate[k].tier, -1);
            const double gr = (double)(counts[8] + counts[0xA] + counts[3] + counts[5]) / d, gu = (double)(counts[0xB] + counts[4]) / d;
            const double wr = DropClassChance(kRate[k].level, cls, DRG_RARE) / DropClassAny(kRate[k].level, cls);
            const double wu = DropClassChance(kRate[k].level, cls, DRG_UNCOMMON) / DropClassAny(kRate[k].level, cls);
            sprintf(b, "rate: %s (%.2f %% of %d kills; of the drops %.2f %% uncommon (expected %.2f), %.2f %% rare (expected %.2f))",
                    kRate[k].what, 100.0 * d / n, n, 100.0 * gu, 100.0 * wu, 100.0 * gr, 100.0 * wr);
            Check(fabs((double)d / n - kRate[k].rate) < kRate[k].tol && fabs(gu - wu) < 0.02 && fabs(gr - wr) < 0.015 &&
                      !counts[6] && !counts[7] && !counts[0xC], b);
        }
    }
    {
        int j, mono = 1, n1 = 0;
        for (j = 0; j < 100000; ++j) {
            const unsigned key = DropKeyKill(4, 0x04000000u + (unsigned)j, 8, 1);
            const int a = DropDecide(DROP_NORMAL, DRT_STRONG, key, 3), c = DropDecide(DROP_ABUNDANT, DRT_STRONG, key, 3);
            if (a) ++n1, mono &= c != 0;
            mono &= !DropDecide(DROP_NORMAL, DRT_TOP, key, 3) || DropDecide(DROP_ABUNDANT, DRT_TOP, key, 3);
            mono &= (!DropDecide(DROP_RARE, DRT_STRONG, key, 3) || a) && (!DropDecide(DROP_RARE, DRT_WEAK, key, 3) || DropDecide(DROP_NORMAL, DRT_WEAK, key, 3)) &&
                    (!DropDecide(DROP_RARE, DRT_TOP, key, 3) || DropDecide(DROP_NORMAL, DRT_TOP, key, 3));  // v3.38 DR338
        }
        Check(mono && n1 > 0, "roll: every Rare drop is a Normal drop, every Normal drop an Abundant one (the thresholds only grow)");
    }
    {
        int counts[16], d;
        d = RunKills(DROP_ABUNDANT, DRT_STRONG, 0, 5u, 100000, counts);
        Check(d > 0 && !counts[1] && !counts[2] && !counts[3], "guns: no bullets at all before a gun is owned");
        d = RunKills(DROP_ABUNDANT, DRT_STRONG, DROP_NEED_PISTOL, 6u, 100000, counts);
        Check(d > 0 && counts[1] && counts[3] && !counts[2], "guns: with the pistol only: pistol and silver bullets, no revolver ones");
        d = RunKills(DROP_ABUNDANT, DRT_STRONG, DROP_NEED_REVOLVER, 7u, 100000, counts);
        Check(d > 0 && counts[2] && !counts[1] && !counts[3], "guns: with the revolver only: revolver bullets only");
        Check(DropPick(kDropCommon, 3, 0.999f, 0) == 9 && DropPick(kDropCommon, 3, 0.0f, 3) == 9 && DropPick(kDropCommon, 3, 0.999f, 3) == 2,
              "items: the pick covers its whole range (ends included)");
    }
    // ---- the ledger
    {
        DropLedger L;
        int j, given = 0;
        memset(&L, 0, sizeof L);
        Check(DropLedgerAllowed(&L, DROP_NORMAL, DRT_TOP, 5, 1, DI_JASPER), "ledger: a top-level being may give a drop");
        DropLedgerCommit(&L, DRT_TOP, 5, 1, DI_JASPER);
        Check(DropLedgerAllowed(&L, DROP_NORMAL, DRT_TOP, 5, 1, DI_JASPER) && DropLedgerAllowed(&L, DROP_ABUNDANT, DRT_TOP, 3, 1, DI_JASPER),
              "ledger: and another at its next full down (no once-per-game / per-world limit)");
        Check(DropLedgerNextDown(&L) == 0 && DropLedgerNextDown(&L) == 1 && L.b[0] == 2, "ledger: byte 0 counts the top-level downs");
        L.b[0] = 0;
        for (j = 0; j < 40; ++j)
            if (DropLedgerAllowed(&L, DROP_NORMAL, DRT_WEAK, 3, 0, -1)) DropLedgerCommit(&L, DRT_WEAK, 3, 0, -1), ++given;
        Check(given == 8 && DropLedgerCount(&L, 3, 0) == 8 && DropLedgerCount(&L, 3, 1) == 0,
              "ledger: Normal gives at most 8 drops in a world visit");
        Check(DropLedgerAllowed(&L, DROP_NORMAL, DRT_WEAK, 3, 1, -1), "ledger: the second visit has its own budget");
        Check(DropLedgerAllowed(&L, DROP_ABUNDANT, DRT_STRONG, 3, 0, -1) && !DropLedgerAllowed(&L, DROP_RARE, DRT_STRONG, 3, 0, -1),
              "ledger: Abundant allows 15 a world visit, Rare 4 (v3.38 DR338; DropAge=0 only)");
        Check(DropLedgerAllowed(&L, DROP_NORMAL, DRT_WEAK, 4, 0, -1), "ledger: the budget is per world");
        Check(!DropLedgerAllowed(&L, DROP_NORMAL, DRT_WEAK, 1, 0, -1) && !DropLedgerAllowed(&L, DROP_NORMAL, DRT_WEAK, 9, 0, -1) &&
                  !DropLedgerAllowed(&L, DROP_NORMAL, DRT_TOP, 10, 0, DI_WALTER) && !DropLedgerAllowed(&L, 0, DRT_WEAK, 3, 1, -1) &&
                  !DropLedgerAllowed(&L, DROP_NORMAL, DRT_TOP, 5, 0, -1),
              "ledger: never in Room 302 (1 / 10), the final stages (9+), or Off");
        for (j = 0; j < 300; ++j) DropLedgerCommit(&L, DRT_WEAK, 8, 1, -1);
        Check(DropLedgerCount(&L, 8, 1) == 15 && DropLedgerCount(&L, 8, 0) == 0 && !(L.b[0] & 0xC0),
              "ledger: the nibble saturates at 15 and touches nothing else");
    }
    // ---- the tracker
    {
        static DropTable T;
        DropTrack *e;
        int ev, j, kills = 0;
        const void *task = (const void *)0x11B6450, *work = (const void *)0x2000000;
        memset(&T, 0, sizeof T);
        DropTableReset(&T);
        // (a) a monster killed by the player
        for (j = 0; j < 10; ++j) {
            ++T.tick;
            e = DropTrackGet(&T, task, work, 0x02020001u);
            ev = DropObserve(&T, e, j < 5 ? 2 : 0x0B, j < 5 ? 50.0f : -3.0f, 0x16, 0);
            kills += ev == DRE_KILL;
        }
        Check(kills == 1, "tracker: a kill by the player fires once (staying dead fires no more)");
        // (b) the same being after a reset (a load / room change): seen dead first: never
        DropTableReset(&T);
        for (j = 0, kills = 0; j < 10; ++j) {
            ++T.tick;
            e = DropTrackGet(&T, task, work, 0x02020001u);
            kills += DropObserve(&T, e, 0x0B, 0.0f, 0x16, 0) != 0;
        }
        Check(kills == 0, "tracker: a monster restored dead never fires");
        // (c) killed by a monster's attack (Walter's bullets, a monster brawl): no credit
        DropTableReset(&T);
        for (j = 0, kills = 0; j < 10; ++j) {
            ++T.tick;
            e = DropTrackGet(&T, task, work, 0x02020002u);
            kills += DropObserve(&T, e, j < 5 ? 3 : 0x0B, j < 5 ? 10.0f : 0.0f, 0x69, 0) != 0;
        }
        Check(kills == 0, "tracker: a death by a non-player attack never fires");
        // (d) a gap between alive and dead (a tick without gameplay: the table is reset)
        DropTableReset(&T);
        ++T.tick;
        e = DropTrackGet(&T, task, work, 0x02020003u);
        DropObserve(&T, e, 2, 30.0f, 0x16, 0);
        DropTableReset(&T);
        ++T.tick;
        e = DropTrackGet(&T, task, work, 0x02020003u);
        ev = DropObserve(&T, e, 0x0B, 0.0f, 0x16, 0);
        ++T.tick;
        e = DropTrackGet(&T, task, work, 0x02020003u);
        ev |= DropObserve(&T, e, 0x0B, 0.0f, 0x16, 0);
        Check(!ev, "tracker: a death across a gap (load, menu, room change) never fires");
        // (e) a downed (class 9) monster finished by a stomp fires
        DropTableReset(&T);
        for (j = 0, kills = 0; j < 6; ++j) {
            ++T.tick;
            e = DropTrackGet(&T, task, work, 0x02020004u);
            kills += DropObserve(&T, e, j < 3 ? 9 : 0x0B, j < 3 ? 20.0f : 0.0f, 0x30, 0) == DRE_KILL;
        }
        Check(kills == 1, "tracker: a downed monster finished by a stomp fires once");
        // (f) a ghost: staggered (HP left) no; HP 0 down yes; up and down again no
        DropTableReset(&T);
        {
            static const struct { int cls; float hp; } kSeq[] = { { 2, 600 }, { 9, 120 }, { 9, 120 }, { 2, 120 }, { 0x0A, 0 },
                                                                   { 9, 0 }, { 9, 0 }, { 2, 150 }, { 0x0A, 0 }, { 9, 0 } };
            int downs = 0, firstAt = -1;
            for (j = 0; j < (int)(sizeof kSeq / sizeof kSeq[0]); ++j) {
                ++T.tick;
                e = DropTrackGet(&T, task, work, 0xFFFF0002u);
                ev = DropObserve(&T, e, kSeq[j].cls, kSeq[j].hp, 0, 1);
                if (ev == DRE_DOWN) ++downs, firstAt = firstAt < 0 ? j : firstAt;
            }
            Check(downs == 2 && firstAt == 4, "tracker: a top-level ghost fires on every full down (HP 0), once each, not on a stagger");
        }
        // (f2) hit / healed while lying: the down episode holds until the class leaves 9 / 0x0A
        DropTableReset(&T);
        {
            static const struct { int cls; float hp; } kSeq[] = { { 2, 300 }, { 0x0A, 0 }, { 9, 5 }, { 9, 0 }, { 9, 12 }, { 0x0A, 0 }, { 2, 100 } };
            int downs = 0;
            for (j = 0; j < (int)(sizeof kSeq / sizeof kSeq[0]); ++j) {
                ++T.tick;
                e = DropTrackGet(&T, task, work, 0xFFFF0004u);
                downs += DropObserve(&T, e, kSeq[j].cls, kSeq[j].hp, 0, 1) == DRE_DOWN;
            }
            Check(downs == 1, "tracker: never more than one roll per down (HP changes while lying do not count)");
        }
        // (g) a ghost restored downed: no; after it rises and is downed again by the player: yes
        DropTableReset(&T);
        {
            static const struct { int cls; float hp; } kSeq[] = { { 9, 0 }, { 9, 0 }, { 9, 0 }, { 1, 300 }, { 0x0A, 0 } };
            int downs = 0, at = -1;
            for (j = 0; j < (int)(sizeof kSeq / sizeof kSeq[0]); ++j) {
                ++T.tick;
                e = DropTrackGet(&T, task, work, 0xFFFF0003u);
                if (DropObserve(&T, e, kSeq[j].cls, kSeq[j].hp, 0, 1) == DRE_DOWN) ++downs, at = j;
            }
            Check(downs == 1 && at == 4, "tracker: a ghost restored downed does not fire until it rises and falls again");
        }
        // (h) 96 beings at once each get their own entry; a 97th has none (no event, no crash)
        DropTableReset(&T);
        ++T.tick;
        for (j = 0, kills = 0; j < DROP_TRACK_MAX; ++j) kills += DropTrackGet(&T, (const char *)task + j, work, 1u) != NULL;
        Check(kills == DROP_TRACK_MAX && DropTrackGet(&T, (const char *)task + 500, work, 1u) == NULL,
              "tracker: 96 entries a tick, a 97th gets none");
    }
    // ---- a reload replays the same drops (pure model of a save / quick load)
    {
        static DropTable T;
        DropLedger L, saved;
        int run, j, items[2][64], n[2];
        memset(&L, 0, sizeof L);
        for (j = 0; j < 4; ++j) DropLedgerCommit(&L, DRT_WEAK, 5, 1, -1);  // drops before the save
        saved = L;
        for (run = 0; run < 2; ++run) {
            L = saved;  // the load restores the ledger with the rest of the enemy manager block
            memset(&T, 0, sizeof T);
            DropTableReset(&T);
            n[run] = 0;
            for (j = 0; j < 64; ++j) {  // 64 kills after the save, in the same order or not: per kill the same
                const unsigned id = 0x05030001u + (unsigned)((j * 37 + run * 11) % 64);
                const int tier = (id & 3) ? DRT_WEAK : DRT_STRONG;
                const int item = DropDecide(DROP_NORMAL, tier, DropKeyKill(5, id, (id & 3) ? 0x0A : 7, 0), 3);
                items[run][id - 0x05030001u] = item && DropLedgerAllowed(&L, DROP_NORMAL, tier, 5, 1, -1) ? item : 0;
                if (items[run][id - 0x05030001u]) DropLedgerCommit(&L, tier, 5, 1, -1), ++n[run];
            }
        }
        Check(n[0] > 0 && n[0] <= 4 && n[0] == n[1], "reload: the drops after a load follow the saved world budget");
        {
            int same = 1, k = 0;
            for (j = 0; j < 64; ++j) {
                const unsigned id = 0x05030001u + (unsigned)j;
                const int tier = (id & 3) ? DRT_WEAK : DRT_STRONG;
                const int item = DropDecide(DROP_NORMAL, tier, DropKeyKill(5, id, (id & 3) ? 0x0A : 7, 0), 3);
                if (items[0][j]) same &= items[0][j] == item, ++k;
                if (items[1][j]) same &= items[1][j] == item;
            }
            Check(same && k > 0, "reload: every kill's item is the kill's own (never re-rolled)");
        }
    }
    // ---- the pool plan
    {
        DropRecView v[DROP_REC_MAX];  // v3.37 MN337: DROP_REC_N is the test pool size now
        int recycle, j;
        memset(v, 0, sizeof v);
        Check(DropPoolPlan(v, 3, 5, DROP_NORMAL, 77u, &recycle) && recycle < 0, "pool: an empty pool: spawn, nothing recycled");
        for (j = 0; j < 17; ++j) v[j].state = 1, v[j].active = 1;  // 17 of the game's: 15 free
        Check(!DropPoolPlan(v, 3, 5, DROP_NORMAL, 77u, &recycle), "pool: fewer than 16 free records: no drop (the game's Swords / returns first)");
        memset(v, 0, sizeof v);
        for (j = 0; j < 16; ++j) v[j].state = 1, v[j].active = 1;  // REV329-DROPS-001: exactly 16 free: a drop would leave 15
        Check(!DropPoolPlan(v, 3, 5, DROP_NORMAL, 77u, &recycle), "pool: exactly 16 free records: no new drop (16 always stay free)");
        memset(v, 0, sizeof v);
        for (j = 0; j < 15; ++j) v[j].state = 1, v[j].active = 1;  // 17 free: the drop leaves 16
        Check(DropPoolPlan(v, 3, 5, DROP_NORMAL, 77u, &recycle) && recycle < 0, "pool: 17 free records: a drop (leaves 16 free)");
        memset(v, 0, sizeof v);
        for (j = 0; j < kDropRates[DROP_NORMAL].roomCap; ++j) v[j].state = 1, v[j].active = 1, v[j].ours = 1, v[j].stage = 3, v[j].room = 5, v[j].tier = DRT_WEAK, v[j].key = 10u + j;
        Check(kDropRates[DROP_RARE].roomCap == 10 && kDropRates[DROP_NORMAL].roomCap == 10 && kDropRates[DROP_ABUNDANT].roomCap == 10 &&
                  !DropPoolPlan(v, 3, 5, DROP_NORMAL, 77u, &recycle) && !DropPoolPlan(v, 3, 5, DROP_RARE, 77u, &recycle),
              "pool: 10 of ours in this room (v3.37 MN337: was 4): no more here");
        Check(DropPoolPlan(v, 3, 6, DROP_NORMAL, 77u, &recycle) && recycle < 0, "pool: the next room may get one");
        Check(!DropPoolPlan(v, 3, 6, DROP_NORMAL, 11u, &recycle), "pool: never a second drop of the same kill while the first lies");
        memset(v, 0, sizeof v);
        g_dropPureRecN = 128;  // v3.37 MN337: the cap of 64 lying needs the moved pool (128 records)
        memset(v, 0, sizeof v);
        for (j = 0; j < 64; ++j) {  // v3.37 MN337: the cap is 64 (v3.35: 10)
            v[j].state = 1, v[j].active = 1, v[j].ours = 1, v[j].stage = 2, v[j].room = j % 9, v[j].key = 200u + j, v[j].seq = 10 + j;
            v[j].tier = j == 0 ? DRT_TOP : j == 1 ? DRT_STRONG : DRT_WEAK;
        }
        v[4].seq = 3;  // the oldest weak one
        Check(kDropRates[DROP_RARE].lyingCap == 64 && kDropRates[DROP_NORMAL].lyingCap == 64 && kDropRates[DROP_ABUNDANT].lyingCap == 64 &&
                  DropPoolPlan(v, 2, 20, DROP_NORMAL, 77u, &recycle) && recycle == 4,
              "pool: at the cap (64 lying, v3.37 MN337): the oldest common drop elsewhere makes way");
        v[5].tier = DRT_GHOST, v[5].seq = 1;  // v3.35 GH335: a ghost drop (a common item) is as cheap as a weak one
        Check(DropPoolPlan(v, 2, 20, DROP_NORMAL, 77u, &recycle) && recycle == 5, "pool: a ghost's drop ranks as a weak one (the oldest of them makes way)");
        for (j = 2; j < 64; ++j) v[j].tier = DRT_TOP;
        Check(DropPoolPlan(v, 2, 20, DROP_NORMAL, 77u, &recycle) && recycle == 1, "pool: then the uncommon one; a rare drop never");
        v[1].tier = DRT_TOP;
        Check(!DropPoolPlan(v, 2, 20, DROP_NORMAL, 77u, &recycle), "pool: only rare drops lying at the cap: no new drop");
        Check(!DropPoolPlan(v, 2, 20, DROP_ABUNDANT, 77u, &recycle) && !DropPoolPlan(v, 2, 20, DROP_RARE, 77u, &recycle),
              "pool: Abundant and Rare have the same cap (64)");
        v[63].state = 0, v[63].active = 0, v[63].ours = 0;
        Check(DropPoolPlan(v, 2, 20, DROP_ABUNDANT, 77u, &recycle) && recycle < 0, "pool: one fewer lying (faded away): a new drop fits");
        v[0].active = 0;  // taken by the player (PickupRecordClear: state / active 0): not counted
        v[0].state = 0;
        Check(DropPoolPlan(v, 2, 9, DROP_NORMAL, 200u, &recycle), "pool: a taken drop no longer blocks its key");
        g_dropPureRecN = 32;
    }
    TestCaps337();  // v3.37 MN337
    // ---- v3.33 MERGE: the refusal's reason (DropPoolWhy) agrees with DropPoolPlan and names the cause
    {
        DropRecView v[DROP_REC_MAX];  // v3.37 MN337: DROP_REC_N is the test pool size now
        int recycle, j, n, fr, inR, ours, bad = 0, seen[6] = { 0 };
        unsigned s = 12345u;
        memset(v, 0, sizeof v);
        for (j = 0; j < 10; ++j) v[j].state = 1, v[j].active = 1, v[j].ours = 1, v[j].stage = 3, v[j].room = 2, v[j].tier = DRT_WEAK, v[j].key = 10u + j;
        Check(DropPoolWhy(v, 3, 2, DROP_ABUNDANT, 77u, &fr, &inR, &ours) == DPW_ROOM_CAP && fr == 22 && inR == 10,
              "pool why: 10 copy drops in stage 3 room 2: the room cap (v3.37 MN337: 10), 22 records free");
        Check(DropPoolWhy(v, 3, 2, DROP_ABUNDANT, 11u, &fr, &inR, &ours) == DPW_SAME_KILL, "pool why: the same kill's drop lies: said so");
        memset(v, 0, sizeof v);
        for (j = 0; j < 16; ++j) v[j].state = 1, v[j].active = 1;
        Check(DropPoolWhy(v, 3, 5, DROP_ABUNDANT, 77u, &fr, &inR, &ours) == DPW_RECORDS && fr == 16, "pool why: 16 game records in use: too few free (16 of 32)");
        memset(v, 0, sizeof v);
        for (j = 0; j < 5; ++j) v[j].state = 1, v[j].active = 1;  // the most any of the user's saves holds (5, bank 2 slot 4..18)
        Check(DropPoolWhy(v, 2, 4, DROP_ABUNDANT, 77u, &fr, &inR, &ours) == DPW_FITS && DropPoolPlan(v, 2, 4, DROP_ABUNDANT, 77u, &recycle),
              "pool why: 5 records in use (the fullest of the user's saves): a drop fits");
        for (n = 0; n < 20000; ++n) {
            int lvl, st, rm, w, p;
            unsigned key;
            memset(v, 0, sizeof v);
            g_dropPureRecN = n & 1 ? 128 : 32;  // v3.37 MN337: every other pool the moved one (its room cap of 10 is reached)
            for (j = 0; j < DROP_REC_N; ++j) {
                s = s * 1664525u + 1013904223u;
                if ((s >> 28) < 7) continue;  // about 56 % in use
                v[j].state = 1, v[j].active = (s >> 8 & 7) != 0, v[j].ours = (s >> 12 & 1);
                v[j].stage = 2 + (int)(s >> 13 & 1), v[j].room = 1 + (int)(s >> 14 & 3), v[j].tier = 1 + (int)(s >> 16 & 3) % 3;
                if (n & 1) v[j].stage = 2, v[j].room = 1 + (int)(s >> 14 & 1);  // v3.37 MN337
                v[j].key = 100u + (s >> 18 & 15), v[j].seq = j;
            }
            s = s * 1664525u + 1013904223u;
            lvl = (int)(s >> 20) % DROP_LEVELS,  /* v3.38 DR338: 0..3 */ st = 2 + (int)(s >> 22 & 1), rm = 1 + (int)(s >> 23 & 3), key = 100u + (s >> 25 & 31);
            p = DropPoolPlan(v, st, rm, lvl, key, &recycle);
            w = DropPoolWhy(v, st, rm, lvl, key, &fr, &inR, &ours);
            if ((p != 0) != (w == DPW_FITS)) ++bad;
            if (w >= 0 && w < 6) seen[w] = 1;
        }
        g_dropPureRecN = 32;  // v3.37 MN337
        Check(!bad, "pool why: 20000 random pools (32 and 128 records): the reason says 'fits' exactly when the plan spawns");
        Check(seen[DPW_FITS] && seen[DPW_SAME_KILL] && seen[DPW_LEVEL] && seen[DPW_ROOM_CAP] && seen[DPW_RECORDS],
              "pool why: the random pools reach every reason (fits, same kill, off, room cap, records)");
    }
    // ---- the marker
    {
        const unsigned w = DROP_MAGIC << 16 | 1u << 12 | 5u, c = DropMarkCheck(w, 1234u, 9, 3, 7, 0x44000000u, 0x45000000u);
        Check(c != DropMarkCheck(w, 1234u, 8, 3, 7, 0x44000000u, 0x45000000u) && c != DropMarkCheck(w, 1234u, 9, 3, 8, 0x44000000u, 0x45000000u) &&
                  c != DropMarkCheck(w, 1234u, 9, 3, 7, 0x44000001u, 0x45000000u) && c != DropMarkCheck(w + 1, 1234u, 9, 3, 7, 0x44000000u, 0x45000000u),
              "marker: a reused slot (other item, room, place or word) never passes for ours");
    }
    // ---- v3.30 AS330: the session seed
    {
        const unsigned id = DropKeyKill(2, 0x02040001u, 0x0A, 0);  // the user's dog of the 17:44 / 17:49 log (char 02040001)
        unsigned seeds[64], s = 0x5D2F1B07u;
        int k, differ = 0, drops = 0, n = 0, sameSeed = 1;
        Check(DropRollKey(id, 0) == id, "seed: Fixed (0) = v3.29's roll (the identity key itself)");
        for (k = 0; k < 64; ++k) s = seeds[k] = DropSeedMix(s, (unsigned)k * 2654435761u, 77u, 0u);
        for (k = 0; k < 64; ++k) {
            sameSeed &= DropDecide(DROP_NORMAL, DRT_WEAK, DropRollKey(id, seeds[k]), 3) == DropDecide(DROP_NORMAL, DRT_WEAK, DropRollKey(id, seeds[k]), 3);
            differ += DropRollKey(id, seeds[k]) != DropRollKey(id, seeds[(k + 1) & 63]);
        }
        Check(sameSeed, "seed: within one session the same kill rolls the same (no re-roll by killing it again)");
        Check(differ == 64, "seed: every new seed gives the same kill another roll key");
        for (k = 0, n = 1; k < 64; ++k) n &= seeds[k] != 0;
        Check(n && DropSeedMix(0x5D2F1B07u, 0, 0, 0) != 0, "seed: a new seed is never 0 (0 means Fixed)");
        n = 0;
        for (k = 0; k < 200000; ++k) {  // 1 kill under 200 000 seeds: the Normal weak rate holds per kill
            s = DropSeedMix(s, (unsigned)k, 5u, 9u);
            drops += DropDecide(DROP_NORMAL, DRT_WEAK, DropRollKey(id, s), 3) != 0;
            ++n;
        }
        sprintf(b, "seed: one kill over %d sessions drops in %.2f %% (Normal weak 12 %%): a reload can re-roll it", n, 100.0 * drops / n);
        Check(fabs((double)drops / n - 0.12) < 0.004, b);
        {   // across many kills and two seeds: about 12 % each, and they differ kill by kill
            int a = 0, c = 0, both = 0, j;
            for (j = 0; j < 100000; ++j) {
                const unsigned kk = DropKeyKill(2 + j % 7, 0x02000000u + (unsigned)j, 7, 0);
                const int x = DropDecide(DROP_NORMAL, DRT_WEAK, DropRollKey(kk, seeds[3]), 3) != 0, y = DropDecide(DROP_NORMAL, DRT_WEAK, DropRollKey(kk, seeds[9]), 3) != 0;
                a += x, c += y, both += x && y;
            }
            sprintf(b, "seed: two sessions over 100 000 kills: %.2f %% / %.2f %% drop, %.2f %% in both (independent: 1.44 %%)",
                    a / 1000.0, c / 1000.0, both / 1000.0);
            Check(fabs(a / 100000.0 - 0.12) < 0.005 && fabs(c / 100000.0 - 0.12) < 0.005 && fabs(both / 100000.0 - 0.0144) < 0.002, b);
        }
        {   // the pool's duplicate rule and the record key use the identity key: a reload never makes a second drop
            DropRecView w[DROP_REC_MAX];  // v3.37 MN337
            int rc;
            memset(w, 0, sizeof w);
            w[0].state = 1, w[0].active = 1, w[0].ours = 1, w[0].stage = 2, w[0].room = 4, w[0].tier = DRT_WEAK, w[0].key = id;
            Check(!DropPoolPlan(w, 2, 4, DROP_NORMAL, id, &rc), "seed: the record keeps the identity key: its drop still lying blocks a second one");
            Check(DropRollKey(id, seeds[1]) != id, "seed: (the roll key is not the record key)");
        }
    }
    {   // the re-roll points (DropSeedStep), at 30 / 60 / 120 / 180 ticks a second
        static const int kRates[4] = { 30, 60, 120, 180 };
        int r;
        for (r = 0; r < 4; ++r) {
            DropSeedWatch w;
            const int hz = kRates[r];
            int t, tick, fires = 0, why, okPlay, okLoad, okFwd, okQuick, okReset, okCut;
            memset(&w, 0, sizeof w);
            for (t = 1000, tick = 0; tick < 600 * hz; ++tick, ++t) fires += DropSeedStep(&w, t, hz, 0, 0, 1) != 0;  // 10 min of play
            for (tick = 0; tick < 5 * hz; ++tick) fires += DropSeedStep(&w, t, hz, 0, 0, 0) != 0;  // a pause: the time stands
            for (tick = 0; tick < 90 * hz; ++tick, ++t) fires += DropSeedStep(&w, t, hz, 0, 0, 0) != 0;  // a 90 s cut-scene: it runs on
            okPlay = fires == 0;
            okCut = DropSeedStep(&w, t++, hz, 0, 0, 1) == 0;
            // a reload: the menu (no ticks), the world reset hook, the time back to the save's, a few non-gameplay ticks
            why = DropSeedStep(&w, t, hz, 0, 1, 0);
            why |= DropSeedStep(&w, 2000, hz, 0, 0, 0);
            okLoad = !why && DropSeedStep(&w, 2001, hz, 0, 0, 1) == (DSR_RESET | DSR_TIME) && DropSeedStep(&w, 2002, hz, 0, 0, 1) == 0;
            // loading a later save: the time jumps ahead (no world-reset hook: Enemy Copies not installed)
            okFwd = DropSeedStep(&w, 2003 + 11 * hz, hz, 0, 0, 1) == DSR_TIME && DropSeedStep(&w, 2004 + 11 * hz, hz, 0, 0, 1) == 0;
            // a quick load: the flag rises, the load runs (not playing), the time comes back: one re-roll at the first play tick
            t = 2005 + 11 * hz;
            why = DropSeedStep(&w, t, hz, 1, 0, 0) | DropSeedStep(&w, t + 1, hz, 1, 0, 0) | DropSeedStep(&w, 1500, hz, 0, 0, 0);
            okQuick = !why && DropSeedStep(&w, 1501, hz, 0, 0, 1) == (DSR_QUICK | DSR_TIME) && DropSeedStep(&w, 1502, hz, 0, 0, 1) == 0;
            // New Game: the reset hook, the time from 0
            okReset = DropSeedStep(&w, 0, hz, 0, 1, 1) == (DSR_RESET | DSR_TIME) && DropSeedStep(&w, 1, hz, 0, 0, 1) == 0;
            sprintf(b, "%3d Hz: play 10 min, a pause and a 90 s cut-scene: %d re-rolls; a reload, a later save, a quick load, New Game: "
                       "one each, before the first gameplay tick's kills", hz, fires);
            Check(okPlay && okCut && okLoad && okFwd && okQuick && okReset, b);
            {   // v3.30 REV330-003: a load of a save 3 ticks .. 10 s ahead re-rolls once; a 2-tick step does not
                int okSmall = 1, j;
                const int jumps[6] = { 3, 4, hz / 2, hz, 5 * hz, 10 * hz };
                for (j = 0; j < 6; ++j) {
                    DropSeedWatch v;
                    memset(&v, 0, sizeof v);
                    okSmall &= DropSeedStep(&v, 5000, hz, 0, 0, 1) == 0;
                    okSmall &= DropSeedStep(&v, 5000 + jumps[j], hz, 0, 0, 1) == DSR_TIME;
                    okSmall &= DropSeedStep(&v, 5001 + jumps[j], hz, 0, 0, 1) == 0;
                    okSmall &= DropSeedStep(&v, 5003 + jumps[j], hz, 0, 0, 1) == 0;  // a 2-tick step: kept
                }
                {   // v3.30 DROPS-001: a menu load of a save at the same game time (0, +1, +2 ticks): the load flag's edge re-rolls once
                    int d;
                    for (d = 0; d <= 2; ++d) {
                        DropSeedWatch v;
                        memset(&v, 0, sizeof v);
                        okSmall &= DropSeedStep(&v, 7000, hz, 0, 0, 1) == 0;
                        okSmall &= DropSeedStep(&v, 7000, hz, 1, 0, 0) == 0;  // the menu confirms: the flag rises (no play)
                        okSmall &= DropSeedStep(&v, 7000 + d, hz, 0, 0, 1) == DSR_QUICK;  // applied: the first gameplay tick
                        okSmall &= DropSeedStep(&v, 7001 + d, hz, 0, 0, 1) == 0;
                    }
                }
                sprintf(b, "%3d Hz: loads of a save 3 ticks, 4 ticks, 0.5 s, 1 s, 5 s and 10 s ahead re-roll once each; a 2-tick step does not; a menu load at the same time (+0..2 ticks) re-rolls once", hz);
                Check(okSmall, b);
            }
        }
    }
    TestCopies();  // v3.32 DHS332
    // v3.32 REV332-001: the copies' ledger resets on a world reset or a load, not on a bare game-time jump
    Check(DropCopyResetFor(DSR_RESET) && DropCopyResetFor(DSR_QUICK) && DropCopyResetFor(DSR_RESET | DSR_TIME) &&
          DropCopyResetFor(DSR_QUICK | DSR_TIME) && !DropCopyResetFor(DSR_TIME) && !DropCopyResetFor(0),
          "copy ledger: reset on a world reset / load (with or without a time jump), kept on a bare game-time jump");
    printf(g_fail ? "%d FAILED\n" : "all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
