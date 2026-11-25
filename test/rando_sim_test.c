// v3.48 RZ348: native test of src\rando_sim.h (test\build_rando_test.cmd): the [Randomizer] texts and the seed, the script
// scan on the stock exe (SH4Decomp orig\sh4.exe: 137 plain pickups, the per-visit table, Room 302's 18; round 2: the 27
// weapon spots, their records and scripts, the weapons a spot may take) and on made-up scripts (the runner's flush rule,
// the exclusions, reads outside the window), Random's groups against drops.inc, the plans (Off changes nothing; Shuffle
// keeps each visit's counts, puts every weapon where it may lie and, fixed, the shared slots of both halves; Random draws
// the specials only with Keep Specials Off, weapons at the right share, never an owned one or one twice in a room, and
// with Change Each Visit a new item at the room's next visit; Room 302 Out; the seed decides), the record words a weapon
// spot gets, patch-and-restore.
// Usage: out\rando_sim_test.exe [path of the stock sh4.exe]
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define DROP_PURE_ONLY
#include "../src/drops.inc"
#include "../src/rando_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- the exe as mapped (sections at their VAs)
static unsigned char *g_img;
static unsigned g_imgSize;
static int LoadExe(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *d;
    long n;
    unsigned pe, nsec, so, i, size;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = (unsigned char *)malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) return fclose(f), 0;
    fclose(f);
    memcpy(&pe, d + 0x3C, 4);
    nsec = *(unsigned short *)(d + pe + 6);
    so = pe + 24 + *(unsigned short *)(d + pe + 20);
    memcpy(&size, d + pe + 24 + 56, 4);  // SizeOfImage
    g_img = (unsigned char *)calloc(size, 1);
    g_imgSize = size;
    for (i = 0; i < nsec; ++i) {
        unsigned vs, va, rs, rp;
        memcpy(&vs, d + so + 40 * i + 8, 4);
        memcpy(&va, d + so + 40 * i + 12, 4);
        memcpy(&rs, d + so + 40 * i + 16, 4);
        memcpy(&rp, d + so + 40 * i + 20, 4);
        if (va + (rs < vs || !vs ? rs : vs) <= size) memcpy(g_img + va, d + rp, rs < vs || !vs ? rs : vs);
    }
    free(d);
    return 1;
}
static unsigned ImgU32(unsigned va)
{
    unsigned v = 0;
    if (va >= 0x400000u && va - 0x400000u + 4 <= g_imgSize) memcpy(&v, g_img + (va - 0x400000u), 4);
    return v;
}
static RzWin ExeWin(void)  // as randomizer.inc RzWindow: the pickup records / scripts / item tables .. the room scripts
{
    RzWin w;
    w.base = g_img + (RZ_DATA_LO - 0x400000u), w.lo = RZ_DATA_LO, w.hi = RZ_SCRIPT_HI;
    return w;
}

// ---------------------------------------------------------------- texts
static void TestTexts(void)
{
    CHECK(RzItemsOfText("Off") == RZI_OFF && RzItemsOfText("shuffle") == RZI_SHUFFLE && RzItemsOfText(" RANDOM ") == RZI_RANDOM,
          "Items names");
    CHECK(RzItemsOfText("0") == 0 && RzItemsOfText("1") == 1 && RzItemsOfText("2") == 2 && RzItemsOfText("3") == -1 &&
          RzItemsOfText("") == -1 && RzItemsOfText("Randomize") == -1 && RzItemsOfText(NULL) == -1, "Items numbers and junk");
    CHECK(RzOnOffOfText("On") == 1 && RzOnOffOfText("off") == 0 && RzOnOffOfText("1") == 1 && RzOnOffOfText("0") == 0 &&
          RzOnOffOfText("yes") == 1 && RzOnOffOfText("maybe") == -1 && RzOnOffOfText("") == -1, "KeepSpecials / PerVisit texts");
    CHECK(RzR302OfText("In") == 1 && RzR302OfText("out") == 0 && RzR302OfText("On") == 1 && RzR302OfText("0") == 0 &&
          RzR302OfText("Inside") == -1, "Room302 texts");
    CHECK(RzSeedOfText("1") == 1u && RzSeedOfText(" 4294967295 ") == 4294967295u && RzSeedOfText("4294967296") == 0 &&
          RzSeedOfText("0") == 0 && RzSeedOfText("") == 0 && RzSeedOfText("12a") == 0 && RzSeedOfText("-5") == 0 &&
          RzSeedOfText("00000000123") == 0 && RzSeedOfText("0000000123") == 123u && RzSeedOfText(NULL) == 0, "Seed texts");
    CHECK(RZ_ITEMS_DEFAULT == RZI_RANDOM && RZ_KEEP_DEFAULT == 0 && RZ_R302_DEFAULT == 1 && RZ_PERVISIT_DEFAULT == 1,
          "the user's defaults: Random, Keep Specials Off, Room 302 In, Change Each Visit On");
    {
        unsigned a = RzSeedRoll(1, 2, 3), b = RzSeedRoll(1, 2, 4);
        CHECK(a && b && a != b && RzSeedRoll(0, 0, 0) != 0, "seed rolls (never 0)");
    }
    CHECK(RzIsHenryWeapon(0x10) && RzIsHenryWeapon(0x15) && !RzIsHenryWeapon(0x16) && RzIsHenryWeapon(0x2B) &&
          !RzIsHenryWeapon(0x2C) && RzIsHenryWeapon(0x2D) && !RzIsHenryWeapon(0x2E) && RzIsHenryWeapon(0x35) &&
          !RzIsHenryWeapon(0x36) && !RzIsHenryWeapon(0x80) && !RzIsHenryWeapon(0x0B), "Henry's weapons (no broken ones, no Eileen's)");
    CHECK(!strcmp(RzItemName(0x35), "Chainsaw") && !strcmp(RzItemName(0x10), "Pistol") && !strcmp(RzItemName(0x2B), "Putter") &&
          !strcmp(RzItemName(0x31), "Pickaxe of Despair"), "weapon names");
    CHECK(RzVisitKey(1234, 0) == 1234 && RzVisitKey(1234, 1) != 1234 && RzVisitKey(1234, 1) != RzVisitKey(1234, 2), "visit keys");
}

// ---------------------------------------------------------------- Random's groups = Supply Drops'
static int SameW(const RzItemW *a, const DropItem *b, int n)
{
    int i;
    for (i = 0; i < n; ++i)
        if (a[i].item != b[i].item || a[i].weight != b[i].weight) return 0;
    return 1;
}
static void TestGroups(void)
{
    int i, k, hist[12], keepHist[12], n = 200000;
    CHECK(SameW(kRzCommon, kDropCommon, 3) && SameW(kRzUncommon, kDropUncommon, 2) && SameW(kRzRare, kDropRare, 4) &&
          DROP_N_COMMON == 3 && DROP_N_UNCOMMON == 2 && DROP_N_RARE == 4, "Random's groups are drops.inc's");
    for (i = 1; i <= 11; ++i) CHECK(i == 6 || i == 7 || !strcmp(RzItemName(i), DropItemName(i)), "item name %d", i);
    memset(hist, 0, sizeof hist), memset(keepHist, 0, sizeof keepHist);
    for (k = 0; k < n; ++k) {
        const int a = RzRandomItem(RzHash4(k, 7, 0, 0), 0), b = RzRandomItem(RzHash4(k, 7, 0, 0), 1);
        CHECK(RzIsConsumable(a) && RzIsConsumable(b), "a drawn item is a pickup item (%d %d)", a, b);
        ++hist[a], ++keepHist[b];
    }
    {   // shares: common 91/137 a third each, uncommon 28/137 (Kit 2 : Candle 1), rare 18/137 a quarter each
        const double c = 91.0 / 137 / 3, kit = 28.0 / 137 * 2 / 3, cand = 28.0 / 137 / 3, r = 18.0 / 137 / 4;
        const double e[12] = { 0, c, c, r, cand, r, 0, 0, r, c, r, kit };
        for (i = 1; i <= 11; ++i) {
            const double got = (double)hist[i] / n;
            CHECK(got > e[i] - 0.006 && got < e[i] + 0.006, "Random share of %s %.4f (want %.4f)", RzItemName(i), got, e[i]);
        }
        CHECK(!keepHist[4] && !keepHist[5] && !keepHist[8], "Keep Specials On: Random never draws a Candle, Sword or Medallion");
        CHECK(keepHist[0x0B] > n * 0.19 && keepHist[0x0A] > n * 0.06 && keepHist[3] > n * 0.06,
              "Keep Specials On: their groups keep the others (Kit %d, Ampoule %d, Silver %d)", keepHist[0x0B], keepHist[0x0A], keepHist[3]);
    }
}

// ---------------------------------------------------------------- the stock exe
static RzScan g_sc[RZ_STAGES][2];
static int g_has1[RZ_STAGES];
static RzWeapons g_wt;
static void TestExe(const char *path)
{
    int stage, v, total = 0, wtotal = 0, cnt[RZ_ITEM_N], wcnt[RZ_ITEM_N], r302 = 0, kind4 = 0, scripts = 0, i;
    unsigned seen[32];
    RzWin w;
    if (!LoadExe(path)) {
        CHECK(0, "the stock exe %s could not be read", path);
        return;
    }
    w = ExeWin();
    RzWeaponsInit(&w, &g_wt);
    memset(cnt, 0, sizeof cnt), memset(wcnt, 0, sizeof wcnt);
    for (stage = 0; stage < RZ_STAGES; ++stage) {
        const unsigned p0 = ImgU32(0x6553A4u + 4 * stage), p1 = ImgU32(0x6553D8u + 4 * stage);
        g_has1[stage] = p1 != p0;
        for (v = 0; v < 1 + g_has1[stage]; ++v) {
            const unsigned p = v ? p1 : p0;
            int dup = 0, k;
            CHECK(RzScanScript(&w, p, &g_sc[stage][v]) && !g_sc[stage][v].overflow, "stage %d visit %d scans", stage, v);
            RzWeaponsAdd(&g_wt, &g_sc[stage][v]);
            for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
            if (dup) continue;
            seen[scripts++] = p;
            CHECK(RzStockMatch(stage, v, p, &g_sc[stage][v]) == 1, "stage %d visit %d: the stock table (%d plain, %d weapon spots)",
                  stage, v, g_sc[stage][v].nPlain, g_sc[stage][v].nWeapon);
            total += g_sc[stage][v].nPlain;
            wtotal += g_sc[stage][v].nWeapon;
            kind4 += g_sc[stage][v].kind4;
            for (i = 0; i < g_sc[stage][v].n; ++i) {
                const RzSlot *s = &g_sc[stage][v].s[i];
                CHECK(s->va >= RZ_SCRIPT_LO && s->va < RZ_SCRIPT_HI && ImgU32(s->va) == s->item, "the item word of a slot holds its item");
                CHECK(g_sc[stage][v].roomStart[s->room] != 0, "a slot's room has a start");
                if (s->kind == RZK_PLAIN) {
                    ++cnt[s->item];
                    if (RzIsRoom302(stage)) ++r302;
                    CHECK(!s->recVa && !s->rec, "a plain slot has no record word");
                } else {
                    ++wcnt[s->item];
                    CHECK(s->recVa >= RZ_SCRIPT_LO && s->recVa < RZ_SCRIPT_HI && ImgU32(s->recVa) == s->rec &&
                          RzSimpleRecord(&w, s->rec, s->item), "a weapon spot's record word holds its simple record");
                }
            }
        }
    }
    CHECK(scripts == 17 && total == RZ_STOCK_TOTAL && kind4 == 273, "17 scripts, 137 plain of 273 kind-4 (got %d, %d, %d)", scripts, total, kind4);
    CHECK(wtotal == RZ_STOCK_WEAPON, "27 weapon spots (%d)", wtotal);
    CHECK(cnt[1] == 46 && cnt[9] == 40 && cnt[4] == 16 && cnt[0x0B] == 12 && cnt[8] == 8 && cnt[2] == 5 && cnt[0x0A] == 4 &&
          cnt[5] == 4 && cnt[3] == 2 && !cnt[6] && !cnt[7] && !cnt[0], "the report's item counts (46 40 16 12 8 5 4 4 2)");
    CHECK(r302 == 18, "18 in Room 302 (%d)", r302);
    CHECK(wcnt[0x11] == 2 && wcnt[0x14] == 4 && wcnt[0x2D] == 2 && wcnt[0x30] == 4 && wcnt[0x31] == 3 && wcnt[0x33] == 2 &&
          wcnt[0x35] == 2 && wcnt[0x1F] == 1 && wcnt[0x1D] == 2 && wcnt[0x15] == 1 && wcnt[0x17] == 1 && wcnt[0x2B] == 2 &&
          wcnt[0x1B] == 1 && !wcnt[0x10] && !wcnt[0x12] && !wcnt[0x13] && !wcnt[0x2F] && !wcnt[0x32] && !wcnt[0x34],
          "the weapon spots' weapons (distinct scripts; never the Pistol, Steel Pipe, Knife, Spade, Stun Gun, Torch)");
    {   // the weapon table: models, default scripts, simple records
        CHECK(!g_wt.m57[0x12] && !g_wt.m57[0x13] && !g_wt.m57[0x2F] && !g_wt.m57[0x32] && g_wt.m57[0x10] && g_wt.m57[0x35] &&
              g_wt.m57[0x14], "model files: Steel Pipe / Spade (file 4), Knife / Stun Gun (0x1C) not in wp_model.bin");
        CHECK(g_wt.def[0x14] && g_wt.def[0x2D] && g_wt.def[0x33] && g_wt.def[0x19] && !g_wt.def[0x10] && !g_wt.def[0x11] &&
              !g_wt.def[0x30] && !g_wt.def[0x31] && !g_wt.def[0x34] && !g_wt.def[0x35], "default scripts");
        CHECK(g_wt.rec[0x11] == 0x62ADA8u && g_wt.rec[0x30] && g_wt.rec[0x31] && g_wt.rec[0x35] == 0x62AEDCu && !g_wt.rec[0x10] &&
              !g_wt.rec[0x34], "simple records (the Pistol and the Torch have none: keyed)");
        CHECK(RzWeaponFits(&g_wt, 0x14, 0) && !RzWeaponFits(&g_wt, 0x35, 0) && RzWeaponFits(&g_wt, 0x35, 1) &&
              !RzWeaponFits(&g_wt, 0x10, 1) && !RzWeaponFits(&g_wt, 0x12, 1) && !RzWeaponFits(&g_wt, 0x34, 1) &&
              !RzWeaponFits(&g_wt, 0x16, 1), "which weapon fits where");
        CHECK(RzDefaultScript(&w, 1) && RzDefaultScript(&w, 5) && RzDefaultScript(&w, 0x0B) && RzModelFile(&w, 1) == 3 &&
              RzModelFile(&w, 5) == 0x57 && RzModelFile(&w, 6) == -1, "consumables: default scripts, models");
        CHECK(!RzSimpleRecord(&w, 0x61AFFCu, 0x10) && !RzSimpleRecord(&w, 0x622620u, 0x35) && !RzSimpleRecord(&w, 0x62244Cu, 0x34) &&
              RzSimpleRecord(&w, 0x62AEDCu, 0x35) && !RzSimpleRecord(&w, 0x62AEDCu, 0x34),
              "keyed / conditioned records are not simple (Room 302's Pistol, the Forest's Chainsaw, the Torch)");
    }
    for (stage = 1; stage <= 5; ++stage) {  // every 1st-half plain slot is in the 2nd half too, same bit, same item
        int ok = g_has1[stage];
        for (i = 0; ok && i < g_sc[stage][0].n; ++i) {
            int j, f = 0;
            if (g_sc[stage][0].s[i].kind != RZK_PLAIN) continue;
            for (j = 0; j < g_sc[stage][1].n; ++j)
                f |= g_sc[stage][1].s[j].room == g_sc[stage][0].s[i].room && g_sc[stage][1].s[j].taken == g_sc[stage][0].s[i].taken &&
                     g_sc[stage][1].s[j].item == g_sc[stage][0].s[i].item;
            ok = f;
        }
        CHECK(ok, "stage %d: the 1st half's plain slots are the 2nd half's too", stage);
    }
    CHECK(g_sc[1][0].n == 9 && g_sc[1][1].n == 9, "Room 302: 9 + 9");
    {
        const RzScan *a = &g_sc[2][0], *b = &g_sc[2][1];
        CHECK(RzVisitOfRoom(a, b, 5, a->roomStart[5]) == 0 && RzVisitOfRoom(a, b, 5, b->roomStart[5]) == 1 &&
              RzVisitOfRoom(a, b, 5, 0x12345678u) == -1 && RzVisitOfRoom(a, NULL, 5, b->roomStart[5]) == -1 &&
              RzVisitOfRoom(a, b, 300, a->roomStart[5]) == -1, "the visit of a room start");
    }
    {
        RzScan t = g_sc[2][1];
        for (i = 0; i < t.n && t.s[i].kind != RZK_PLAIN; ++i) {}
        t.s[i].item = t.s[i].item == 1 ? 9 : 1;
        CHECK(RzStockMatch(2, 1, ImgU32(0x6553D8u + 8), &t) == 0 && RzStockMatch(2, 1, 0x655420u, &g_sc[2][1]) == 0 &&
              RzStockMatch(13, 0, 0, &t) == -1, "a changed scan or another script start is refused");
    }
}

// ---------------------------------------------------------------- plans on the stock scripts
static RzPlanIn In(int mode, int keep, int r302, int share, unsigned seed, unsigned world, const unsigned *rooms,
                   unsigned long long owned, const RzWeapons *wt)
{
    RzPlanIn in;
    in.mode = mode, in.keep = keep, in.r302 = r302, in.share = share, in.seed = seed, in.worldSalt = world;
    in.roomSalt = rooms, in.owned = owned, in.wt = wt, in.taken = NULL;
    return in;
}
static int Counts(const RzScan *v, const unsigned char *items, const unsigned char *pool, int *c)
{
    int i, n = 0;
    memset(c, 0, RZ_ITEM_N * sizeof(int));
    for (i = 0; i < v->n; ++i)
        if (pool[i]) ++c[items[i]], ++n;
    return n;
}
typedef struct { unsigned char o[RZ_VMAX], p[RZ_VMAX]; unsigned r[RZ_VMAX]; RzPlanStat s; } VPlan;
static void Plan(const RzPlanIn *in, int stage, VPlan *a, VPlan *b)
{
    RzPlanStage(in, stage, &g_sc[stage][0], g_has1[stage] ? &g_sc[stage][1] : NULL, a->o, a->p, a->r, &a->s, b->o, b->p, b->r, &b->s);
}
// every planned item may lie at its spot, the record word fits it
static void CheckFit(const RzPlanIn *in, const RzScan *sc, const VPlan *pl, int stage, int v)
{
    int i;
    for (i = 0; i < sc->n; ++i) {
        const RzSlot *s = &sc->s[i];
        const int it = pl->o[i];
        CHECK(RzIsConsumable(it) || (RzIsHenryWeapon(it) && in->wt && RzWeaponFits(in->wt, it, s->kind == RZK_WEAPON)),
              "stage %d visit %d slot %d: %02X may lie at a %s spot", stage, v, i, it, s->kind == RZK_WEAPON ? "weapon" : "plain");
        if (s->kind == RZK_PLAIN) continue;
        if (it == s->item) CHECK(pl->r[i] == s->rec, "an unchanged weapon spot keeps its record");
        else if (RzIsConsumable(it) || in->wt->def[it]) CHECK(pl->r[i] == 0, "a consumable / default-script weapon: the default record");
        else CHECK(pl->r[i] == in->wt->rec[it] && pl->r[i], "a weapon without a default script: its simple record");
    }
}
static void TestPlans(void)
{
    static const unsigned kSeeds[] = { 1, 2, 12345, 0xDEADBEEFu, 4294967295u };
    int si, stage, mode, keep, r302, share, i;
    for (si = 0; si < (int)(sizeof kSeeds / sizeof kSeeds[0]); ++si)
        for (mode = 0; mode < RZI_N; ++mode)
            for (keep = 0; keep < 2; ++keep)
                for (r302 = 0; r302 < 2; ++r302)
                    for (share = 0; share < 2; ++share)
                        for (stage = 0; stage < RZ_STAGES; ++stage) {
                            const RzScan *v0 = &g_sc[stage][0], *v1 = g_has1[stage] ? &g_sc[stage][1] : NULL;
                            const RzPlanIn in = In(mode, keep, r302, share, kSeeds[si], share ? 0 : 3, NULL, 0, &g_wt);
                            VPlan a, b, c, d;
                            int v;
                            Plan(&in, stage, &a, &b);
                            Plan(&in, stage, &c, &d);
                            CHECK(!memcmp(a.o, c.o, v0->n) && (!v1 || !memcmp(b.o, d.o, v1->n)), "a plan is the same twice");
                            for (v = 0; v < 1 + (v1 != NULL); ++v) {
                                const RzScan *sc = v ? v1 : v0;
                                const VPlan *pl = v ? &b : &a;
                                int stock[RZ_ITEM_N], got[RZ_ITEM_N];
                                CheckFit(&in, sc, pl, stage, v);
                                for (i = 0; i < sc->n; ++i) {
                                    const int it = sc->s[i].item;
                                    if (!pl->p[i]) CHECK(pl->o[i] == it, "a slot out of the pool keeps its item");
                                    if (mode == RZI_OFF) CHECK(!pl->p[i] && pl->o[i] == it, "Off: nothing in the pool");
                                    if (keep && RzIsSpecial(it)) CHECK(!pl->p[i] && pl->o[i] == it, "Keep Specials On: a special stays");
                                    if (keep && pl->p[i]) CHECK(!RzIsSpecial(pl->o[i]), "Keep Specials On: no special lands elsewhere");
                                    if (RzIsRoom302(stage) && !r302) CHECK(!pl->p[i] && pl->o[i] == it, "Room 302 Out: left alone");
                                    if (mode != RZI_OFF && !keep && (!RzIsRoom302(stage) || r302)) CHECK(pl->p[i], "in the pool");
                                }
                                if (mode == RZI_SHUFFLE) {
                                    unsigned char st[RZ_VMAX];
                                    Counts(sc, pl->o, pl->p, got);
                                    for (i = 0; i < sc->n; ++i) st[i] = sc->s[i].item;
                                    Counts(sc, st, pl->p, stock);
                                    CHECK(!memcmp(got, stock, sizeof got) && !pl->s.unplaced,
                                          "Shuffle: stage %d visit %d keeps its counts, weapons included (unplaced %d)", stage, v, pl->s.unplaced);
                                }
                            }
                            if (v1 && mode == RZI_SHUFFLE && share)  // fixed: the same save slot has one item in both halves
                                for (i = 0; i < v1->n; ++i) {
                                    const int j = RzSharedSlot(v0, &v1->s[i], a.p);
                                    if (j >= 0 && b.p[i]) CHECK(b.o[i] == a.o[j], "stage %d: a shared slot has one item in both halves", stage);
                                }
                        }
    {   // the seed decides; Shuffle moves weapons among the visit's spots (weapons onto plain spots too)
        VPlan a, b;
        RzPlanIn in = In(RZI_SHUFFLE, 0, 1, 1, 1, 0, NULL, 0, &g_wt);
        int moved = 0, onPlain = 0, k;
        Plan(&in, 8, &a, &b);
        in.seed = 2;
        Plan(&in, 8, &b, &b);
        CHECK(memcmp(a.o, b.o, g_sc[8][0].n) != 0 && a.s.changed > 0, "Shuffle: another seed, another order (changed %d)", a.s.changed);
        for (k = 1; k <= 40; ++k) {
            VPlan c, d;
            in.seed = (unsigned)k;
            Plan(&in, 2, &c, &d);
            for (i = 0; i < g_sc[2][1].n; ++i) {
                moved += g_sc[2][1].s[i].kind == RZK_WEAPON && d.o[i] != g_sc[2][1].s[i].item;
                onPlain += g_sc[2][1].s[i].kind == RZK_PLAIN && RzIsHenryWeapon(d.o[i]);
            }
            CHECK(d.s.weapons == 7 && d.s.weaponsStock == 7, "Shuffle: the Subway's 2nd visit keeps its 7 weapons (%d)", d.s.weapons);
        }
        CHECK(moved > 0 && onPlain > 0, "Shuffle: weapons move, some onto plain spots (%d, %d)", moved, onPlain);
    }
    {   // Change Each Visit: Shuffle by world visit, Random by room visit
        VPlan a, b, c, d;
        static unsigned rooms0[RZ_ROOMS], rooms1[RZ_ROOMS];
        RzPlanIn in = In(RZI_SHUFFLE, 0, 1, 0, 77, 1, NULL, 0, &g_wt);
        int diff = 0;
        Plan(&in, 5, &a, &b);
        in.worldSalt = 2;
        Plan(&in, 5, &c, &d);
        CHECK(memcmp(b.o, d.o, g_sc[5][1].n) != 0, "Shuffle: a new world visit, a new permutation");
        for (i = 0; i < RZ_ROOMS; ++i) rooms0[i] = 1, rooms1[i] = 1;
        rooms1[11] = 2;  // only room 11 is visited again
        in = In(RZI_RANDOM, 0, 1, 0, 77, 0, rooms0, 0, &g_wt);
        Plan(&in, 5, &a, &b);
        in.roomSalt = rooms1;
        Plan(&in, 5, &c, &d);
        for (i = 0; i < g_sc[5][1].n; ++i) {
            if (g_sc[5][1].s[i].room != 11) CHECK(b.o[i] == d.o[i], "Random: the other rooms keep their items");
            else diff += b.o[i] != d.o[i];
        }
        {   // over many visits the room's spots change (each spot most of the time)
            int k, changes = 0, spots = 0;
            unsigned char last[RZ_VMAX];
            memcpy(last, b.o, sizeof last);
            for (k = 2; k < 60; ++k) {
                rooms1[11] = (unsigned)k;
                Plan(&in, 5, &c, &d);
                for (i = 0; i < g_sc[5][1].n; ++i)
                    if (g_sc[5][1].s[i].room == 11) ++spots, changes += d.o[i] != last[i], last[i] = d.o[i];
            }
            CHECK(spots > 0 && changes > spots / 2, "Random: room 5:11's spots change between visits (%d of %d)", changes, spots);
        }
        (void)diff;
    }
    {   // Random's weapons: share, owned weapons never, a weapon once per room, only where it fits
        int k, plainW = 0, plainN = 0, wspotW = 0, wspotN = 0, owned35 = 0, dup = 0;
        static unsigned rooms[RZ_ROOMS];
        for (k = 1; k <= 400; ++k)
            for (stage = 1; stage <= 8; ++stage) {
                VPlan a, b;
                const unsigned long long own = 1ull << (0x35 - 0x10) | 1ull << (0x14 - 0x10);  // the Chainsaw and the Bat owned
                RzPlanIn in;
                int v;
                for (i = 0; i < RZ_ROOMS; ++i) rooms[i] = (unsigned)k;
                in = In(RZI_RANDOM, 0, 1, 0, 9, 0, rooms, own, &g_wt);
                Plan(&in, stage, &a, &b);
                for (v = 0; v < 1 + g_has1[stage]; ++v) {
                    const RzScan *sc = &g_sc[stage][v];
                    const VPlan *pl = v ? &b : &a;
                    int j;
                    CheckFit(&in, sc, pl, stage, v);
                    for (i = 0; i < sc->n; ++i) {
                        const int wpn = RzIsHenryWeapon(pl->o[i]);
                        if (sc->s[i].kind == RZK_PLAIN) ++plainN, plainW += wpn;
                        else ++wspotN, wspotW += wpn;
                        owned35 += pl->o[i] == 0x35 || pl->o[i] == 0x14;
                        for (j = 0; j < i; ++j) dup += wpn && sc->s[j].room == sc->s[i].room && pl->o[j] == pl->o[i];
                    }
                }
            }
        CHECK(plainW > plainN * 0.025 && plainW < plainN * 0.06, "Random: weapons at plain spots %.3f (want ~0.042)", (double)plainW / plainN);
        CHECK(wspotW > wspotN * 0.42 && wspotW < wspotN * 0.58, "Random: weapons at weapon spots %.3f (want ~0.5)", (double)wspotW / wspotN);
        CHECK(!owned35 && !dup, "Random: never an owned weapon (%d), never one weapon twice in a room (%d)", owned35, dup);
    }
    {   // without the weapon table (wt NULL) the weapon spots stay, plain spots never draw weapons
        VPlan a, b;
        const RzPlanIn in = In(RZI_RANDOM, 0, 1, 1, 5, 0, NULL, 0, NULL);
        Plan(&in, 2, &a, &b);
        for (i = 0; i < g_sc[2][1].n; ++i) {
            if (g_sc[2][1].s[i].kind == RZK_WEAPON) CHECK(!b.p[i] && b.o[i] == g_sc[2][1].s[i].item && b.r[i] == g_sc[2][1].s[i].rec,
                                                          "no weapon table: a weapon spot stays");
            else CHECK(!RzIsHenryWeapon(b.o[i]), "no weapon table: no weapon drawn");
        }
    }
    {   // Change Each Visit Off: plain spots get v1's items (the same consumable draw)
        VPlan a, b;
        const RzPlanIn in = In(RZI_RANDOM, 0, 1, 1, 12345, 0, NULL, 0, &g_wt);
        int same = 0, n = 0;
        Plan(&in, 8, &a, &b);
        for (i = 0; i < g_sc[8][0].n; ++i) {
            const RzSlot *s = &g_sc[8][0].s[i];
            if (s->kind != RZK_PLAIN || RzIsHenryWeapon(a.o[i])) continue;
            ++n, same += a.o[i] == RzRandomItem(RzSlotKey(12345, 8, s->room, s->taken), 0);
        }
        CHECK(n > 0 && same == n, "fixed Random: a plain spot without a weapon roll gets v1's item (%d of %d)", same, n);
    }
}

// ---------------------------------------------------------------- made-up scripts
static unsigned g_buf[512];
static int g_bn;
static void Put(unsigned tag, int argc, ...)
{
    va_list ap;
    int i;
    va_start(ap, argc);
    g_buf[g_bn++] = tag | (unsigned)argc << 16;
    for (i = 0; i < argc; ++i) g_buf[g_bn++] = va_arg(ap, unsigned);
    va_end(ap);
}
static RzWin BufWin(void)
{
    RzWin w;
    w.base = (const unsigned char *)g_buf, w.lo = 0x10000u, w.hi = 0x10000u + 4u * (unsigned)g_bn;
    return w;
}
static void TestMadeUp(void)
{
    RzScan sc;
    RzWin w;
    g_bn = 0;
    Put(0, 1, 3);                        // room 3
    Put(1, 1, 4), Put(0x0F, 1, 9), Put(0x1B, 1, 7);  // n 0: plain (the room's first command is a tag 1: no flush)
    Put(1, 1, 4), Put(0x0F, 1, 0x30);    // n 1: a weapon without a record
    Put(1, 1, 4), Put(0x0F, 1, 1), Put(0x18, 1, 0x61AFFCu);  // n 2: its own record
    Put(1, 1, 4), Put(0x0F, 1, 1), Put(0x0B, 1, 0xF000F001u);  // n 3: a model
    Put(1, 1, 4), Put(0x0F, 1, 1), Put(0x29, 1, 2);  // n 4: two item words
    Put(1, 1, 4), Put(0x0F, 1, 6);       // n 5: item 6
    Put(1, 1, 4), Put(0x0F, 1, 1), Put(3, 1, 0x401000u);  // n 6: its own create
    Put(1, 1, 2), Put(0x0F, 1, 1);       // n 7: an enemy
    Put(1, 1, 4), Put(0x0A, 4, 0, 0, 0x0B, 0xF), Put(0x25, 1, 0x12345);  // n 8: tag 0x0A's item, tag 0x25's bit
    Put(1, 1, 4), Put(0x0F, 1, 0x14), Put(0x18, 1, 0x10000u);  // n 9: a weapon with a "record" that is no record
    Put(0, 1, 4);                        // room 4
    Put(1, 1, 4), Put(0x29, 1, 8), Put(0x1B, 1, 3);  // n 0 (the end of the room flushes the last)
    g_buf[g_bn++] = 0xF0F0F0F0u;
    w = BufWin();
    CHECK(RzScanScript(&w, 0x10000u, &sc) && sc.rooms == 2 && sc.kind4 == 10, "made up: scanned (rooms %d, kind4 %d)", sc.rooms, sc.kind4);
    CHECK(sc.n == 3 && sc.nPlain == 3 && !sc.nWeapon && sc.s[0].room == 3 && sc.s[0].n == 0 && sc.s[0].item == 9 && sc.s[0].taken == 7 &&
          sc.s[0].va == 0x10000u + 4 * 5, "made up: the first object of a room (n 0, its item word); no weapon spot");
    CHECK(sc.s[1].room == 3 && sc.s[1].n == 8 && sc.s[1].item == 0x0B && sc.s[1].taken == 0x2345 && sc.s[2].room == 4 &&
          sc.s[2].n == 0 && sc.s[2].item == 8 && sc.s[2].taken == 3, "made up: tag 0x0A / 0x29 / 0x25, the last object of a room");
    CHECK(sc.roomStart[3] == 0x10000u + 8 && sc.roomStart[4] != 0 && !sc.roomStart[5], "made up: room starts");
    {
        RzWin s = w;
        s.hi -= 8;
        CHECK(!RzScanScript(&s, 0x10000u, &sc) && sc.bad, "made up: a script running past the window is refused");
        CHECK(!RzScanScript(&w, 0x10002u, &sc), "made up: a misaligned start is refused");
    }
    g_bn = 0;
    Put(0, 1, 300);
    g_buf[g_bn++] = 0xF0F0F0F0u;
    w = BufWin();
    CHECK(!RzScanScript(&w, 0x10000u, &sc), "made up: a room >= 256 is refused");
}

// ---------------------------------------------------------------- patch and restore
static void TestPatch(void)
{
    unsigned words[3] = { 9, 1, 4 };
    RzPatchRec r[3] = { { &words[0], 9, 4 }, { &words[1], 2, 8 }, { &words[2], 4, 5 } };
    int n = 0;
    n += RzPatchOne(&r[0]);
    CHECK(n == 1 && words[0] == 4, "patch: written over the stock item");
    CHECK(!RzPatchOne(&r[1]) && words[1] == 1, "patch: a word without the stock item is left");
    n += RzPatchOne(&r[2]);
    CHECK(n == 2 && words[2] == 5, "patch: the second word written");
    words[2] = 7;  // another writer during the build
    r[1] = r[2];   // (the list holds only the written ones, as RzBeforeScript keeps it)
    CHECK(RzRestoreAll(r, 2) == 1 && words[0] == 9 && words[2] == 7, "restore: ours back to stock, another writer's value left (and counted)");
}

// ---------------------------------------------------------------- v3.48 fix1 (Codex inspection 1)
static void SetTaken(unsigned char *bits, int room, int bit) { bits[room * 8 + bit / 8] |= (unsigned char)(1u << (bit % 8)); }
// RZ-001: a revisit with taken slots holding weapons: taken slots keep their item and change nothing else. The plan of the
// untaken slots equals the plan of the same scan with the taken slots removed (they neither redraw nor reserve a weapon).
static void TestTakenSlots(void)
{
    static unsigned rooms[RZ_ROOMS];
    static unsigned char bits[RZ_TAKEN_ROOMS * RZ_TAKEN_SLOTS / 8];
    const RzScan *sc = &g_sc[2][1];
    int k, i, blockedBefore = 0, takenWeapons = 0, checked = 0;
    for (k = 1; k <= 300; ++k) {
        RzScan cut = *sc;
        VPlan a, b, c, d, e, f;
        RzPlanIn in, inCut;
        int m = 0, j;
        memset(bits, 0, sizeof bits);
        for (i = 0; i < sc->n; ++i)  // the weapon row of room 1 (bits 11..21) partly taken, and one plain spot elsewhere
            if ((sc->s[i].room == 1 && sc->s[i].kind == RZK_WEAPON && (sc->s[i].taken + k) % 3 == 0) ||
                (sc->s[i].kind == RZK_PLAIN && i == k % sc->n)) {
                SetTaken(bits, sc->s[i].room, sc->s[i].taken);
                takenWeapons += sc->s[i].kind == RZK_WEAPON;
            }
        for (i = 0; i < RZ_ROOMS; ++i) rooms[i] = (unsigned)k;
        in = In(RZI_RANDOM, 0, 1, 0, 4242, 0, rooms, 0, &g_wt);
        in.taken = bits;
        RzPlanStage(&in, 2, &g_sc[2][0], sc, a.o, a.p, a.r, &a.s, b.o, b.p, b.r, &b.s);
        // without the taken state (round 2): count the cases where a taken slot's weapon was missing from an untaken one
        inCut = in;
        inCut.taken = NULL;
        RzPlanStage(&inCut, 2, &g_sc[2][0], sc, c.o, c.p, c.r, &c.s, d.o, d.p, d.r, &d.s);
        // the scan without the taken slots
        cut.n = 0;
        for (i = 0; i < sc->n; ++i) {
            RzPlanIn t = in;
            t.taken = bits;
            if (!RzSlotTaken(&t, &sc->s[i])) cut.s[cut.n++] = sc->s[i];
        }
        inCut.taken = NULL;
        RzPlanStage(&inCut, 2, &g_sc[2][0], &cut, e.o, e.p, e.r, &e.s, f.o, f.p, f.r, &f.s);
        for (i = 0; i < sc->n; ++i) {
            RzPlanIn t = in;
            if (RzSlotTaken(&t, &sc->s[i])) {
                CHECK(b.o[i] == sc->s[i].item && b.r[i] == sc->s[i].rec, "RZ-001: a taken slot keeps its item and record");
                continue;
            }
            CHECK(b.o[i] == f.o[m] && b.r[i] == f.r[m], "RZ-001: an untaken slot's draw ignores the taken slots (k %d slot %d)", k, i);
            ++m, ++checked;
            for (j = 0; j < i; ++j)
                if (sc->s[j].room == sc->s[i].room && RzIsHenryWeapon(b.o[i]) && b.o[j] == b.o[i] && !RzSlotTaken(&t, &sc->s[j]))
                    CHECK(0, "RZ-001: one weapon twice among a room's untaken slots");
            if (d.o[i] != b.o[i]) ++blockedBefore;
        }
    }
    CHECK(checked > 1000 && takenWeapons > 300, "RZ-001: enough cases (%d untaken slots, %d taken weapon spots)", checked, takenWeapons);
    CHECK(blockedBefore > 0, "RZ-001: without the taken state some untaken slot drew otherwise (%d: the bug the fix removes)", blockedBefore);
    {   // Shuffle keeps planning taken slots (the world's arrangement must not change as items are taken)
        VPlan a, b, c, d;
        RzPlanIn in = In(RZI_SHUFFLE, 0, 1, 0, 7, 2, NULL, 0, &g_wt);
        Plan(&in, 2, &a, &b);
        memset(bits, 0xFF, sizeof bits);
        in.taken = bits;
        Plan(&in, 2, &c, &d);
        CHECK(!memcmp(b.o, d.o, sc->n) && !memcmp(a.o, c.o, g_sc[2][0].n), "RZ-001: Shuffle's arrangement does not move as items are taken");
    }
    {   // the game's rule outside its array: a room >= 50 or a bit >= 64 reads as taken
        RzPlanIn t = In(RZI_RANDOM, 0, 1, 0, 1, 0, NULL, 0, NULL);
        RzSlot s;
        memset(&s, 0, sizeof s);
        memset(bits, 0, sizeof bits);
        t.taken = bits;
        s.room = 49, s.taken = 63;
        CHECK(!RzSlotTaken(&t, &s), "taken: room 49 bit 63 clear");
        SetTaken(bits, 49, 63);
        CHECK(RzSlotTaken(&t, &s), "taken: room 49 bit 63 set");
        s.room = 50;
        CHECK(RzSlotTaken(&t, &s), "taken: room 50 = taken (outside the array)");
        t.taken = NULL;
        CHECK(!RzSlotTaken(&t, &s), "taken: no bits = nothing taken");
    }
}
// RZ-003: the room visit count is 32 bit and stops at the top (never back to an earlier value)
static void TestVisitCount(void)
{
    unsigned k = 0, prev = 0;
    int i, up = 1;
    for (i = 0; i < 70000; ++i) {
        k = RzVisitNext(k);
        up &= k > prev;
        prev = k;
    }
    CHECK(up && k == 70000u, "RZ-003: 70000 visits count up without wrapping (%u)", k);
    CHECK(RzVisitNext(0xFFFFu) == 0x10000u && RzVisitNext(0xFFFFFFFEu) == 0xFFFFFFFFu && RzVisitNext(0xFFFFFFFFu) == 0xFFFFFFFFu,
          "RZ-003: past 65535, and it stops at 4294967295");
    CHECK(RzVisitKey(99, 0x10001u) != RzVisitKey(99, 1u), "RZ-003: visit 65537 is not visit 1");
}
// RZ-002: the patch stack: a run inside a run keeps the outer list; reverse order; the depth limit; a pop of an outer level
// also restores an inner one that was never popped
static void TestPatchStack(void)
{
    static RzPatchStack st;
    unsigned w[6] = { 1, 2, 3, 4, 5, 6 };
    int d0, d1, d2, k, deep[RZ_DEPTH + 2];
    memset(&st, 0, sizeof st);
    d0 = RzStackPush(&st);
    CHECK(d0 == 0 && RzStackAdd(&st, d0, &w[0], 1, 10) && RzStackAdd(&st, d0, &w[1], 2, 20), "stack: level 0 writes");
    CHECK(RzStackAdd(&st, d0, &w[0], 10, 11) && w[0] == 11, "stack: the same word twice in a level");
    d1 = RzStackPush(&st);  // a nested room build
    CHECK(d1 == 1 && RzStackAdd(&st, d1, &w[2], 3, 30) && st.n[0] == 3 && st.n[1] == 1, "stack: the nested run has its own list");
    CHECK(RzStackPop(&st, d1) == 0 && w[2] == 3 && w[0] == 11 && w[1] == 20 && st.depth == 1, "stack: the inner pop restores only its words");
    CHECK(RzStackPop(&st, d0) == 0 && w[0] == 1 && w[1] == 2 && st.depth == 0, "stack: the outer pop restores its words, the last first");
    // a word changed by someone else: left as found, counted
    d0 = RzStackPush(&st);
    RzStackAdd(&st, d0, &w[3], 4, 40);
    w[3] = 41;
    CHECK(RzStackPop(&st, d0) == 1 && w[3] == 41, "stack: another writer's value is left and counted");
    // an inner level never popped (its frame was skipped): the outer pop restores it too
    d0 = RzStackPush(&st);
    RzStackAdd(&st, d0, &w[4], 5, 50);
    d1 = RzStackPush(&st);
    RzStackAdd(&st, d1, &w[5], 6, 60);
    CHECK(RzStackPop(&st, d0) == 0 && w[4] == 5 && w[5] == 6 && st.depth == 0, "stack: an outer pop restores an inner level left open");
    // the depth limit: the levels past RZ_DEPTH write nothing; popping them changes no list below
    for (k = 0; k < RZ_DEPTH + 2; ++k) deep[k] = RzStackPush(&st);
    CHECK(deep[RZ_DEPTH] == RZ_DEPTH && !RzStackAdd(&st, deep[RZ_DEPTH], &w[0], 1, 99) && w[0] == 1, "stack: no list past the depth limit");
    RzStackAdd(&st, deep[0], &w[1], 2, 22);
    for (k = RZ_DEPTH + 1; k >= 1; --k) RzStackPop(&st, deep[k]);
    CHECK(w[1] == 22 && st.depth == 1 && st.n[0] == 1, "stack: popping the deep levels keeps level 0's list");
    RzStackPop(&st, deep[0]);
    CHECK(w[1] == 2 && st.depth == 0, "stack: level 0 restored last");
    // the trim (a weapon spot whose item word failed: its record word back)
    d0 = RzStackPush(&st);
    RzStackAdd(&st, d0, &w[0], 1, 7);
    RzStackAdd(&st, d0, &w[1], 2, 8);
    CHECK(RzStackTrim(&st, d0, 1) == 0 && w[1] == 2 && w[0] == 7 && st.n[0] == 1, "stack: trim back to a mark");
    RzStackPop(&st, d0);
    CHECK(w[0] == 1, "stack: then the rest");
    d2 = RzStackPush(&st);
    for (k = 0; k < RZ_PATCH_MAX; ++k) w[0] = 1, RzStackAdd(&st, d2, &w[0], 1, 2), w[0] = 1;
    CHECK(st.n[0] == RZ_PATCH_MAX && !RzStackAdd(&st, d2, &w[0], 1, 2), "stack: a full level refuses (bounded)");
    RzStackPop(&st, d2);
}

int main(int argc, char **argv)
{
    const char *exe = argc > 1 ? argv[1] : "sh4.exe";
    TestTexts();
    TestGroups();
    TestExe(exe);
    TestPlans();
    TestMadeUp();
    TestPatch();
    TestTakenSlots();  // v3.48 fix1 RZ-001
    TestVisitCount();  // v3.48 fix1 RZ-003
    TestPatchStack();  // v3.48 fix1 RZ-002
    printf("rando_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
