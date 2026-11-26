// v3.49 RZ349: native test of src\rzmon_sim.h (test\build_rzmon_test.cmd): the Randomizer's Monsters. v3.50 RM350: the game's own
// records adopted (a save-based game), All Worlds, a save made in a randomized room and loaded back. The [Randomizer]
// Monsters texts; the type table and the variant counts against the stock exe (SH4Decomp orig\sh4.exe); the scan of the
// enemy placements on the stock exe (783 kind-2 placements, 235 sources, the per-visit table, every word a slot names holds
// the value the scan read and sits behind the command it belongs to) and on made-up scripts (each exclusion); the plans
// (the pools, the seed, Change Each Visit, the Gum Heads, the record rule, the floor, the bone budget, the mix); a plan's
// words written into a copy of the stock scripts give exactly the planned types when scanned again, and the restore gives
// the stock scripts back; the descriptor transform; the records and the save filter; the word stack.
// Usage: out\rzmon_sim_test.exe [path of the stock sh4.exe]
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../src/rzmon_sim.h"

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
static unsigned *ImgPtr(unsigned va) { return (unsigned *)(g_img + (va - 0x400000u)); }
static RzWin ExeWin(void)  // as randomizer.inc RzWindow
{
    RzWin w;
    w.base = g_img + (RZ_DATA_LO - 0x400000u), w.lo = RZ_DATA_LO, w.hi = RZ_SCRIPT_HI;
    return w;
}

// ---------------------------------------------------------------- texts, types, mode words
static void TestTexts(void)
{
    CHECK(RmMonOfText("Off") == RMM_OFF && RmMonOfText("this room") == RMM_ROOM && RmMonOfText(" THIS WORLD ") == RMM_WORLD,
          "Monsters names");
    CHECK(RmMonOfText("Room") == RMM_ROOM && RmMonOfText("world") == RMM_WORLD && RmMonOfText("0") == 0 && RmMonOfText("1") == 1 &&
          RmMonOfText("2") == 2 && RmMonOfText("4") == -1 && RmMonOfText("") == -1 && RmMonOfText("Everywhere") == -1 &&
          RmMonOfText(NULL) == -1, "Monsters short names, numbers and junk");
    CHECK(RmMonOfText("All Worlds") == RMM_ALL && RmMonOfText(" all worlds ") == RMM_ALL && RmMonOfText("All") == RMM_ALL &&
          RmMonOfText("3") == RMM_ALL && RMM_N == 4 && !strcmp(kRmMonName[RMM_ALL], "All Worlds") &&
          !strcmp(kRmMonName[RMM_WORLD], "This World") && RMM_OFF == 0 && RMM_ROOM == 1 && RMM_WORLD == 2,
          "v3.50 RM350: All Worlds (3 / All), the v3.49 values and their numbers unchanged");
    CHECK(RM_MON_DEFAULT == RMM_WORLD, "the host's default: This World");
    CHECK(RmIsSourceType(2) && RmIsSourceType(3) && RmIsSourceType(7) && RmIsSourceType(8) && RmIsSourceType(9) &&
          RmIsSourceType(0x0A) && !RmIsSourceType(1) && !RmIsSourceType(4) && !RmIsSourceType(5) && !RmIsSourceType(6) &&
          !RmIsSourceType(0x0B) && !RmIsSourceType(0x0F) && !RmIsSourceType(0x10) && !RmIsSourceType(0x11),
          "source types: never mm, Wall Man, Wheelchair, ghosts, Walter, the Patient");
    CHECK(RmTargetIndex(7, 0) == 2 && RmTargetIndex(7, 1) == 3 && (RM_GUM_TARGETS == (1u << 2 | 1u << 3)) &&
          RmTargetIndex(9, 1) < 0 && RmTargetIndex(3, 1) < 0 && RmTargetIndex(6, 0) < 0 && RmTargetIndex(0x0B, 0) < 0, "targets");
    CHECK(RmBitCount(0) == 0 && RmBitCount(0x1FF) == 9 && RmNthBit(0x14, 0) == 2 && RmNthBit(0x14, 1) == 4 && RmNthBit(0x14, 2) == -1,
          "bit helpers");
    CHECK(RmModeOk(2, 0) && RmModeOk(9, 0x0010) && RmModeOk(9, 0x3150) == 0 && RmModeOk(9, 0x3110) && RmModeOk(0x0A, 0x0100) &&
          !RmModeOk(0x0A, 0x0106) && !RmModeOk(0x0A, 0x0103) && !RmModeOk(3, 0x0120) && !RmModeOk(7, 0x1000) && !RmModeOk(3, 0x2000) &&
          !RmModeOk(2, 0x10000), "mode words: spawn mode 0, upright, bits 12-15 only for a Tremer");
    CHECK(RmModeWord(9, 0x3110, 9, 0) == 0x3010 && RmModeWord(9, 0x3110, 0x0A, 1) == 0x0100 && RmModeWord(2, 0, 8, 1) == 0x100 &&
          RmModeWord(0x0A, 0x100, 0x0A, 0) == 0, "mode words given: the same type keeps its bits, another gets its variant only");
    CHECK(RmSourceVariantOk(9, 2) && !RmSourceVariantOk(9, 3) && RmSourceVariantOk(8, 1) && !RmSourceVariantOk(8, 2) &&
          !RmSourceVariantOk(3, 1) && !RmSourceVariantOk(2, 1) && RmSourceVariantOk(0x0A, 1), "source variants");
    CHECK(RmPlaceCost(8) == 185 + 111 && RmPlaceCost(9) == 20 + 12 && RmPlaceCost(2) == 18 + 10, "bone estimates (enemies_sim.h)");
}

// ---------------------------------------------------------------- the stock exe
static RmScan g_sc[RZ_STAGES][2];
static int g_has1[RZ_STAGES];
static unsigned Script(int stage, int visit) { return ImgU32((visit ? 0x6553D8u : 0x6553A4u) + 4u * (unsigned)stage); }
static void TestExe(const char *path)
{
    static const unsigned kCreate[RM_TYPE_MAX] = { 0, 0x475FC0, 0x4ACEF0, 0x4742F0, 0x4AB680, 0x49CF30, 0x4CC430, 0x49A3D0, 0x4C9610,
                                                  0x48B940, 0x48E690, 0x4AFA90, 0x4B58C0, 0x47FB50, 0x47B4E0, 0x4BBAF0, 0x4A2130,
                                                  0x4BF9F0, 0x476590, 0x477010, 0x477FD0, 0x4767C0, 0x477B00 };
    static const int kVariants[0x13] = { 0, 1, 1, 1, 1, 3, 2, 2, 2, 1, 2, 4, 2, 2, 1, 2, 2, 1, 1 };
    RzWin w;
    int stage, v, t, scripts = 0, total = 0, kind2 = 0, roomMax = 0, words = 0, wordsOk = 0, cmdOk = 0, boss = 0, i;
    unsigned seen[RZ_STAGES * 2];
    if (!LoadExe(path)) {
        CHECK(0, "the stock exe %s could not be read", path);
        return;
    }
    w = ExeWin();
    for (t = 1; t < RM_TYPE_MAX; ++t)
        CHECK(ImgU32(RM_TYPE_TAB + 16u * (unsigned)t + 4u) == kCreate[t], "type %02X's create at 0x615348 + 16t + 4: %08X", t,
              ImgU32(RM_TYPE_TAB + 16u * (unsigned)t + 4u));
    for (t = 1; t < 0x13; ++t) CHECK(ImgU32(0x5B8BA0u + 4u * (unsigned)t) == (unsigned)kVariants[t], "variant count of type %02X", t);
    for (i = 0; i < RM_TARGET_N; ++i)
        CHECK(kRmTargets[i].type == 9 ? kRmTargets[i].variant == 0 : (int)kRmTargets[i].variant < kVariants[kRmTargets[i].type],
              "target %s is below its type's stat rows", kRmTargets[i].name);
    // the code sites the game side checks or patches (byte for byte)
    CHECK(!memcmp(g_img + (0x52CAA9 - 0x400000), "\xE8\x52\xFE\xFF\xFF", 5) && !memcmp(g_img + (0x52CB2B - 0x400000), "\xE8\xD0\xFD\xFF\xFF", 5),
          "the runner's two create calls (the cheat table's 7700 sites) are stock");
    CHECK(!memcmp(g_img + (0x46545D - 0x400000), "\x68\x90\x1B\x00\x00\x68\x10\xF0\xFC\x00\x50\xE8\x43\x67\xFC\xFF", 16),
          "EnemyManagerSave's copy call 0x465468 (call 42BBB0 with 1B90h bytes from 0FCF010h)");
    CHECK(ImgU32(0x465469) + 0x46546Du == 0x42BBB0u, "0x465468 calls 0x42BBB0");
    CHECK(!memcmp(g_img + (0x49B70A - 0x400000), "\x89\x83\xA4\x00\x00\x00", 6) && !memcmp(g_img + (0x45C3C0 - 0x400000),
          "\x8B\x44\x24\x04\x8B\x80\x08\x04\x00\x00", 10), "the Gum Head guard's two sites are stock in the stock exe");
    CHECK(!memcmp(g_img + (0x4F0AF1 - 0x400000), "\x8B\x0D\x94\x5A\xFD\x00\x51\xE8\x03\xBF\x03\x00", 12), "the room load 0x4F0AF8");
    for (stage = 0; stage < RZ_STAGES; ++stage) {
        const unsigned p0 = Script(stage, 0), p1 = Script(stage, 1);
        g_has1[stage] = p0 != p1;
        for (v = 0; v < 1 + g_has1[stage]; ++v) {
            const unsigned p = v ? p1 : p0;
            int k, dup = 0, m;
            RmScan *sc = &g_sc[stage][v];
            CHECK(RmScanScript(&w, p, stage, sc), "stage %d visit %d: scanned", stage, v);
            CHECK(!sc->overflow && !sc->bad, "stage %d visit %d: no overflow", stage, v);
            m = RmStockMatch(stage, v, p, sc);
            CHECK(m == 1 || (m < 0 && !sc->n), "stage %d visit %d: the stock table (%d; %d sources of %d)", stage, v, m,
                  sc->n, sc->kind2);
            if (sc->roomMax > roomMax) roomMax = sc->roomMax;
            for (k = 0; k < sc->n; ++k) {
                const RmSlot *s = &sc->s[k];
                unsigned y;
                memcpy(&y, &s->pos[1], 4);
                words += 3;
                wordsOk += (ImgU32(s->typeVa) == s->type) + (ImgU32(s->modeVa) == s->mw) + (ImgU32(s->yVa) == y);
                cmdOk += ImgU32(s->typeVa - 4) == (7u | 1u << 16);
                cmdOk += (ImgU32(s->modeVa - 4) & 0xFFFFu) == 0x0B || (ImgU32(s->modeVa - 4) & 0xFFFFu) == 0x0D ||
                         (ImgU32(s->modeVa - 4) & 0xFFFFu) == 0x24;
                cmdOk += (ImgU32(s->yVa - 8) & 0xFFFFu) == 8 && ImgU32(s->yVa - 8) >> 16 >= 3;  // (x, y, z, 1.0)
                CHECK(s->typeVa >= RZ_SCRIPT_LO && s->yVa < RZ_SCRIPT_HI, "slot words inside the room scripts");
                CHECK(RmIsSourceType(s->type) && RmModeOk(s->type, s->mw) && RmSourceVariantOk(s->type, s->variant) &&
                      !(s->mw & 0xF) && s->room > 0 && s->room < 64 && stage >= 2 && stage <= 8, "slot rules");
                CHECK(RmSlotOf(sc, s->room, s->n) == k, "RmSlotOf finds it");
            }
            for (k = 0; k < RZ_ROOMS; ++k) boss += sc->roomBoss[k];
            for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
            if (dup) continue;
            seen[scripts++] = p;
            total += sc->n, kind2 += sc->kind2;
        }
    }
    CHECK(total == RM_STOCK_TOTAL && total == 235 && kind2 == RM_STOCK_KIND2 && kind2 == 783,
          "the stock exe: %d source placements of %d kind-2 (tools/rzmon_scan.py: 235 of 783)", total, kind2);
    CHECK(roomMax == 20 && roomMax <= RM_ROOM_MAX, "at most %d sources in one room (20)", roomMax);
    CHECK(words == wordsOk && words == 3 * 235, "every slot word holds the scanned "
          "value (%d of %d)", wordsOk, words);
    CHECK(cmdOk == words, "every slot word sits right behind its command (7 / 0x0B, 0x0D, 0x24 / 8 with 3+ words): %d of %d", cmdOk, words);
    CHECK(boss > 0, "boss rooms flagged (%d)", boss);
    {   // known placements (tools/rzmon_scan.py --list)
        const RmScan *s21 = &g_sc[2][1];
        const int a = RmSlotOf(s21, 3, 1), b = RmSlotOf(s21, 3, 4);
        CHECK(a >= 0 && s21->s[a].type == 9 && s21->s[a].mw == 0x0010 && b >= 0 && s21->s[b].mw == 0x0110,
              "Subway 2nd half room 3: its floor Tremers are sources (mode 0010 / 0110)");
        CHECK(g_sc[7][0].n == 0 && g_sc[7][0].kind2 == 53, "stage 7: no source (Whitestool v1, Hummer v2 / v3: left alone)");
        CHECK(g_sc[4][1].n == 71 && (g_sc[4][1].placed >> RmTargetIndex(2, 0) & 1u) && (g_sc[4][1].placed >> RmTargetIndex(8, 0) & 1u),
              "Water Prison 2nd half: 71 sources; it places Whitestools and Twin Victims");
        CHECK(!(g_sc[2][0].placed >> RmTargetIndex(8, 0) & 1u) && (g_sc[2][0].placed >> RmTargetIndex(0x0A, 0) & 1u),
              "Subway 1st half: no Twin Victim of its own, Sniffer Dogs yes (This World's pool)");
        CHECK((g_sc[2][0].placed | g_sc[2][1].placed) == 0x1C4u && (g_sc[5][0].placed | g_sc[5][1].placed) == 0x1CFu &&
              g_sc[8][0].placed == 0x030u, "This World's targets: Subway 1C4 (Gum Head, Tremer, both dogs), Building 1CF, stage 8 "
              "030 (both Twin Victims)");
        for (stage = 2; stage <= 8; ++stage)
            printf("  stage %d: This World's targets %03X / %03X (halves), %d / %d sources\n", stage, g_sc[stage][0].placed,
                   g_has1[stage] ? g_sc[stage][1].placed : g_sc[stage][0].placed, g_sc[stage][0].n, g_has1[stage] ? g_sc[stage][1].n : -1);
    }
}

// ---------------------------------------------------------------- the plans
static RmPlanIn In(int mode, unsigned seed, int stage, int half, int room, unsigned visit, unsigned resident, unsigned allowed)
{
    RmPlanIn in;
    memset(&in, 0, sizeof in);
    in.mode = mode, in.seed = seed, in.stage = stage, in.half = half, in.room = room, in.visit = visit;
    in.world = g_sc[stage][0].placed | (g_has1[stage] ? g_sc[stage][1].placed : 0u);  // (as rzmon.inc: both halves)
    in.resident = resident, in.allowed = allowed, in.freeBones = 4096, in.totalBones = 4096;
    return in;
}
static int RoomIdx(const RmScan *sc, int room, int *idx)
{
    int i, m = 0;
    for (i = 0; i < sc->n; ++i)
        if (sc->s[i].room == room) idx[m++] = i;
    return m;
}
static void NoRecs(RmRec *r, int m)
{
    int k;
    for (k = 0; k < m; ++k) r[k].type = -1, r[k].variant = 0, r[k].here = 0, r[k].ours = 0;
}
// every room of a scan planned with in (room set per room); visit = salt
typedef struct { int picks, changed, byTarget[RM_TARGET_N], outside, gum, flyerWalker, costOver; } Sum;
static void PlanAll(const RmScan *sc, RmPlanIn in, const unsigned char *floorOk, Sum *sum, RmPick (*keep)[RM_ROOM_MAX])
{
    int room;
    memset(sum, 0, sizeof *sum);
    for (room = 1; room < RZ_ROOMS; ++room) {
        int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), k;
        RmRec rec[RM_ROOM_MAX];
        RmPick p[RM_ROOM_MAX];
        RmPlanStat st;
        if (!m) continue;
        NoRecs(rec, m);
        in.room = room;
        RmPlanRoom(&in, sc, idx, m, rec, floorOk, p, &st);
        if (keep) memcpy(keep[room], p, sizeof p[0] * (size_t)m);
        if (st.cost > st.limit && st.cost > st.cost0) ++sum->costOver;
        for (k = 0; k < m; ++k) {
            const RmSlot *s = &sc->s[idx[k]];
            const int t = RmTargetIndex(p[k].type, p[k].variant);
            ++sum->picks;
            if (p[k].type != s->type || p[k].variant != s->variant) {
                ++sum->changed;
                if (t < 0 || !(RmPool(&in) >> t & 1u)) ++sum->outside;
                if (p[k].type == 7) ++sum->gum;
                if (RmIsFlyer(s->type) && !RmIsFlyer(p[k].type)) ++sum->flyerWalker;
            }
            if (t >= 0) ++sum->byTarget[t];
        }
    }
}
static int SamePlans(RmPick (*a)[RM_ROOM_MAX], RmPick (*b)[RM_ROOM_MAX], const RmScan *sc, int *diff)
{
    int room, k, same = 1;
    *diff = 0;
    for (room = 1; room < RZ_ROOMS; ++room) {
        int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx);
        for (k = 0; k < m; ++k)
            if (a[room][k].type != b[room][k].type || a[room][k].variant != b[room][k].variant) same = 0, ++*diff;
    }
    return same;
}
static RmPick g_pa[RZ_ROOMS][RM_ROOM_MAX], g_pb[RZ_ROOMS][RM_ROOM_MAX];
static void TestPlans(void)
{
    const RmScan *sc = &g_sc[4][1];  // Water Prison 2nd half: 71 sources
    const unsigned stockT = 1u << RmTargetIndex(2, 0) | 1u << RmTargetIndex(8, 0) | 1u << RmTargetIndex(9, 0);
    unsigned char yes[RM_ROOM_MAX], no[RM_ROOM_MAX];
    Sum s1, s2;
    int diff, t, k;
    memset(yes, 1, sizeof yes), memset(no, 0, sizeof no);
    // This Room: only resident targets; Gum Heads not allowed
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 0, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s1, g_pa);
    CHECK(s1.picks == 71 && s1.changed > 20 && !s1.outside && !s1.gum, "This Room: %d of %d changed, all in the pool", s1.changed, s1.picks);
    for (t = 0; t < RM_TARGET_N; ++t)
        CHECK((stockT >> t & 1u) ? s1.byTarget[t] > 10 : !s1.byTarget[t], "This Room: target %s x%d", kRmTargets[t].name, s1.byTarget[t]);
    // the same input = the same plan; another seed = another plan
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 0, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(SamePlans(g_pa, g_pb, sc, &diff), "the same seed: the same monsters");
    PlanAll(sc, In(RMM_ROOM, 1235, 4, 1, 0, 0, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff) && diff > 30, "another seed: %d of 71 differ", diff);
    // Change Each Visit: visit 1 against visit 0, visit 7 against visit 7 (same)
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 1, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff) && diff > 30, "Change Each Visit: the next visit differs (%d of 71)", diff);
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 7, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s1, g_pa);
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 7, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(SamePlans(g_pa, g_pb, sc, &diff), "the same visit count: the same monsters");
    // the halves are keyed apart
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 0, 0, 7, stockT, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff), "the half is part of the key (%d differ)", diff);
    // This World: resident | placed; every target appears (Gum Heads allowed: placed elsewhere only, so not here)
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, 0, RM_ALL_TARGETS), yes, &s1, NULL);
    CHECK(!s1.outside && s1.byTarget[RmTargetIndex(2, 0)] && s1.byTarget[RmTargetIndex(8, 0)] && s1.byTarget[RmTargetIndex(9, 0)] &&
          !s1.byTarget[RmTargetIndex(0x0A, 0)], "This World with nothing resident: this world's own types only");
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS), yes, &s1, NULL);
    for (t = 0; t < RM_TARGET_N; ++t)
        CHECK(s1.byTarget[t] >= 3, "all targets resident and allowed: %s x%d (a mix)", kRmTargets[t].name, s1.byTarget[t]);
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS & ~RM_GUM_TARGETS), yes, &s1, NULL);
    CHECK(!s1.gum && !s1.byTarget[2] && !s1.byTarget[3], "Gum Heads out without a guard");
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, 0, 0), yes, &s1, NULL);
    CHECK(!s1.changed, "an empty pool changes nothing");
    {   // the distribution over many seeds: each of 9 targets near 1/9
        int cnt[RM_TARGET_N], seed, total = 0, ok = 1;
        memset(cnt, 0, sizeof cnt);
        for (seed = 1; seed <= 200; ++seed) {
            RmPlanIn in = In(RMM_WORLD, (unsigned)seed * 7919u, 4, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS);
            in.freeBones = 1 << 24;  // (no bone limit: the roll alone)
            PlanAll(sc, in, yes, &s1, NULL);
            for (t = 0; t < RM_TARGET_N; ++t) cnt[t] += s1.byTarget[t], total += s1.byTarget[t];
        }
        for (t = 0; t < RM_TARGET_N; ++t) ok &= cnt[t] * 9 > total * 8 / 10 && cnt[t] * 9 < total * 12 / 10;
        CHECK(ok && total == 200 * 71, "the mix is even over 200 seeds (each target within 20 %% of 1/9)");
    }
    // Hummers: a walker only where a floor was found
    {
        const RmScan *h = &g_sc[5][1];  // Building 2nd half: 14 Hummer sources
        PlanAll(h, In(RMM_WORLD, 5, 5, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS), no, &s1, NULL);
        CHECK(!s1.flyerWalker, "no floor: a Hummer's spot never gets a walker");
        PlanAll(h, In(RMM_WORLD, 5, 5, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS), yes, &s2, NULL);
        CHECK(s2.flyerWalker > 3, "with a floor: walkers at Hummer spots (%d)", s2.flyerWalker);
    }
    // the bone budget
    {
        int room;
        for (room = 1; room < RZ_ROOMS; ++room) {
            int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), cost;
            RmRec rec[RM_ROOM_MAX];
            RmPick p[RM_ROOM_MAX];
            RmPlanStat st;
            RmPlanIn in = In(RMM_WORLD, 3, 4, 1, room, 0, RM_ALL_TARGETS, RM_ALL_TARGETS);
            if (m < 5) continue;
            NoRecs(rec, m);
            in.freeBones = 1408, in.totalBones = 1408;  // the stock pool
            RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
            for (k = 0, cost = sc->roomCost[room]; k < m; ++k) cost += RmPlaceCost(p[k].type) - RmPlaceCost(sc->s[idx[k]].type);
            CHECK(cost == st.cost && st.cost <= st.limit && st.limit == (sc->roomCost[room] > 1408 - EnmReserve(1408) ?
                  sc->roomCost[room] : 1408 - EnmReserve(1408)), "room 4:%d: the stock pool: cost %d <= limit %d (stock %d), %d kept",
                  room, st.cost, st.limit, sc->roomCost[room], st.bones);
            in.freeBones = 0;  // nothing free: only changes that cost no more
            RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
            CHECK(st.cost <= sc->roomCost[room] && st.limit == sc->roomCost[room], "room 4:%d: no free bones: never dearer than the "
                  "stock room (%d <= %d)", room, st.cost, sc->roomCost[room]);
        }
    }
    // the record rule
    {
        int idx[RM_ROOM_MAX], m = 0, room, sticky;
        RmRec rec[RM_ROOM_MAX];
        RmPick p[RM_ROOM_MAX], q[RM_ROOM_MAX];
        RmPlanStat st;
        RmPlanIn in;
        for (room = 1; room < RZ_ROOMS && (m = RoomIdx(sc, room, idx)) < 4; ++room) {}
        in = In(RMM_ROOM, 77, 4, 1, room, 0, stockT, RM_ALL_TARGETS);
        CHECK(m >= 4, "a room with 4+ sources (room %d: %d)", room, m);
        sticky = sc->s[idx[0]].type == 8 ? 2 : 8;
        NoRecs(rec, m);
        RmPlanRoom(&in, sc, idx, m, rec, yes, q, &st);
        rec[0].type = sticky, rec[0].variant = 0, rec[0].here = 1;   // a record of another type here, in the pool: sticky
        rec[1].type = sc->s[idx[1]].type, rec[1].variant = sc->s[idx[1]].variant, rec[1].here = 1;  // its stock type: stock
        rec[2].type = 0x0A, rec[2].variant = 1, rec[2].here = 1;  // a dog record here, dogs not in the pool: deleted, rolled
        rec[3].type = 0x0A, rec[3].variant = 0, rec[3].here = 0;  // elsewhere, not in the pool: the stock type, kept
        rec[0].ours = rec[1].ours = rec[2].ours = rec[3].ours = 1;  // (v3.49 fix2: Refined's own records)
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
        CHECK(p[0].type == sticky && p[0].variant == 0 && p[0].why == RMW_STICKY && !p[0].drop, "a record in the pool: its type");
        CHECK(p[1].type == sc->s[idx[1]].type && p[1].variant == sc->s[idx[1]].variant && !p[1].drop, "a record of the stock type: stock");
        CHECK(p[2].drop && p[2].type == q[2].type && p[2].variant == q[2].variant, "a record of a type not in the pool: deleted, "
              "the roll as without it");
        CHECK(p[3].why == RMW_AWAY && p[3].type == sc->s[idx[3]].type && !p[3].drop, "a record elsewhere: the stock type, kept");
        CHECK(st.sticky == 1 && st.away == 1 && st.drops == 1, "the record counts");
        in.reroll = 1;  // Change Each Visit: a record here keeps no type; one elsewhere does
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
        CHECK(p[0].type == q[0].type && p[0].variant == q[0].variant && p[0].why != RMW_STICKY &&
              p[0].drop == !(q[0].type == sticky && q[0].variant == 0), "reroll: a record here is rolled over (deleted unless the roll "
              "gives its type)");
        CHECK(p[1].type == q[1].type && p[1].variant == q[1].variant && p[1].drop == !(q[1].type == rec[1].type && q[1].variant ==
              rec[1].variant), "reroll: a record of the stock type is rolled over too");
        CHECK(p[3].why == RMW_AWAY && !p[3].drop, "reroll: a record elsewhere still keeps its placement stock");
        rec[3].type = sc->s[idx[3]].type == 8 ? 2 : 8, rec[3].variant = 0;
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
        CHECK(p[3].why == RMW_STICKY && p[3].type == rec[3].type && !p[3].drop, "reroll: a record elsewhere in the pool keeps its type");
        rec[3].type = 0x0A, rec[3].variant = 0, in.reroll = 0;
        {   // v3.49 fix2 RZ349-SAVE-001: records Refined did not make: as placed, never deleted (with and without reroll)
            RmRec o[RM_ROOM_MAX];
            RmPick po[RM_ROOM_MAX];
            int j;
            memcpy(o, rec, sizeof o[0] * (size_t)m);
            for (j = 0; j < 4; ++j) o[j].ours = 0, o[j].here = 1;
            o[1].type = sc->s[idx[1]].type, o[1].variant = sc->s[idx[1]].variant;  // (a stock record: the game's own)
            for (j = 0; j < 2; ++j) {
                in.reroll = j;
                RmPlanRoom(&in, sc, idx, m, o, yes, po, &st);
                CHECK(po[0].why == RMW_OTHER && po[0].type == sc->s[idx[0]].type && !po[0].drop && po[2].why == RMW_OTHER &&
                      !po[2].drop && po[3].why == RMW_OTHER && !po[3].drop && st.other == 3, "another writer's records (reroll %d): "
                      "the placements as placed, the records kept", j);
                CHECK(po[1].adopt && po[1].why == RMW_ROLL && po[1].type == q[1].type && po[1].variant == q[1].variant &&
                      po[1].drop == !(q[1].type == sc->s[idx[1]].type && q[1].variant == sc->s[idx[1]].variant) && st.adopted == 1 &&
                      !po[0].adopt && !po[2].adopt && !po[3].adopt, "v3.50 RM350: the game's own stock record here (reroll %d): "
                      "adopted, rolled as without it, deleted only for another type", j);
            }
            in.reroll = 0;
            {
                RmPick r2 = po[2];
                RmPlanStat s2 = st;
                RmRevertPick(&r2, &sc->s[idx[2]], &o[2], &s2);
                CHECK(!r2.drop, "files: another writer's record is never deleted");
            }
        }
        in.freeBones = 0, in.totalBones = 4096;  // a sticky Twin Victim over the budget: the stock type, its record deleted
        rec[0].type = 8;
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
        CHECK(sc->s[idx[0]].type == 8 || (p[0].why == RMW_BONES && p[0].drop && p[0].type == sc->s[idx[0]].type),
              "a sticky pick over the budget: stock, the record deleted");
        {
            RmPick r = p[2];
            RmPlanStat s0 = st;
            RmRevertPick(&r, &sc->s[idx[2]], &rec[2], &s0);
            CHECK(r.why == RMW_FILES && r.type == sc->s[idx[2]].type && r.drop && s0.files == st.files + 1, "files: back to stock");
        }
    }
}

static void Rec(unsigned char *r, unsigned id, int type, int state, int variant, int stage, int room);
// ---------------------------------------------------------------- v3.50 RM350: a save-based game (the game's own records)
// one room load over a records array as rzmon.inc RmBeforeScriptC does it (no files / words / floor test: all fit):
// the records read, the plan, the drops, the provenance (RmProvAfterBuild)
static void RoomLoad(unsigned char *recs, RmProv *pv, const RmScan *sc, int stage, int half, int room, RmPlanIn in, RmPick *p,
                     RmPlanStat *st)
{
    int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), k;
    RmRec rec[RM_ROOM_MAX];
    unsigned char yes[RM_ROOM_MAX];
    memset(yes, 1, sizeof yes);
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &sc->s[idx[k]];
        const unsigned char *r = RmRecFind(recs, RM_REC_N, EnmScriptId(stage, half, room, s->n));
        rec[k].type = r && RmRecStage(r) == stage ? RmRecType(r) : -1;
        rec[k].variant = r ? RmRecVariant(r) : 0;
        rec[k].here = r && RmRecRoom(r) == room;
        rec[k].ours = RmProvOurs(pv, r);
    }
    in.room = room;
    RmPlanRoom(&in, sc, idx, m, rec, yes, p, st);
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &sc->s[idx[k]];
        const unsigned id = EnmScriptId(stage, half, room, s->n);
        unsigned char *r = (unsigned char *)RmRecFind(recs, RM_REC_N, id);
        if (p[k].drop && r && RmRecRoom(r) == room) memset(r, 0, RM_REC_SIZE), r = NULL;  // (EnemyRecordDelete)
        if (r && RmProvOurs(pv, r) && RmRecRoom(r) != room) continue;
        RmProvPut(pv, id, p[k].type, p[k].variant, s->type, s->variant);
    }
}
// the room is left: each monster made there writes its record (the type it was made as), as its destroy does
static void RoomLeave(unsigned char *recs, const RmScan *sc, int stage, int half, int room, const RmPick *p)
{
    int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), k, i;
    for (k = 0; k < m; ++k) {
        const unsigned id = EnmScriptId(stage, half, room, sc->s[idx[k]].n);
        unsigned char *r = (unsigned char *)RmRecFind(recs, RM_REC_N, id);
        for (i = 0; !r && i < RM_REC_N; ++i)
            if (!RmRecState(recs + i * RM_REC_SIZE)) r = recs + i * RM_REC_SIZE;
        if (r) Rec(r, id, p[k].type, 1, p[k].variant, stage, room);
    }
}
// every placement of the room gets the game's own record of its stock monster (what a save that visited the room holds)
static void StockRecs(unsigned char *recs, const RmScan *sc, int stage, int half, int room, int here)
{
    RmPick p[RM_ROOM_MAX];
    int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), k;
    for (k = 0; k < m; ++k) p[k].type = sc->s[idx[k]].type, p[k].variant = sc->s[idx[k]].variant;
    RoomLeave(recs, sc, stage, half, room, p);
    if (!here)
        for (k = 0; k < RM_REC_N; ++k)
            if (RmRecState(recs + k * RM_REC_SIZE)) recs[k * RM_REC_SIZE + 0x37] = (unsigned char)(room + 1);  // (moved away)
}
static void TestAdopt(void)
{
    const RmScan *sc = &g_sc[4][1];  // Water Prison 2nd half: 71 sources
    unsigned char yes[RM_ROOM_MAX], no[RM_ROOM_MAX];
    int room, k, tot = 0, totCh = 0, totAd = 0, okSame = 1, okDrop = 1, okAway = 1, okOther = 1, okOurs = 1, okReroll = 1, j;
    memset(yes, 1, sizeof yes), memset(no, 0, sizeof no);
    for (room = 1; room < RZ_ROOMS; ++room) {
        int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx);
        RmRec rec[RM_ROOM_MAX];
        RmPick a[RM_ROOM_MAX], b[RM_ROOM_MAX];
        RmPlanStat sa, sb;
        RmPlanIn in = In(RMM_WORLD, 794655925u, 4, 1, room, 0, 0, RM_ALL_TARGETS);
        if (!m) continue;
        NoRecs(rec, m);
        RmPlanRoom(&in, sc, idx, m, rec, yes, a, &sa);
        for (k = 0; k < m; ++k) rec[k].type = sc->s[idx[k]].type, rec[k].variant = sc->s[idx[k]].variant, rec[k].here = 1;
        RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);  // the game's own stock records here: adopted
        for (k = 0; k < m; ++k) {
            const int ch = b[k].type != sc->s[idx[k]].type || b[k].variant != sc->s[idx[k]].variant;
            okSame &= b[k].type == a[k].type && b[k].variant == a[k].variant && b[k].adopt;
            okDrop &= b[k].drop == ch;
        }
        okSame &= sb.adopted == m && sb.adoptChanged == sa.changed && sb.changed == sa.changed && sb.drops == sa.changed &&
                  sb.rollStock == m - sa.changed - sa.bones - sa.floor;
        tot += m, totCh += sb.changed, totAd += sb.adopted;
        for (j = 0; j < 2; ++j) {  // Change Each Visit On, visit 3: still the roll as without the records
            RmPlanIn r = in;
            r.reroll = 1, r.visit = 3;
            NoRecs(rec, m);
            RmPlanRoom(&r, sc, idx, m, rec, yes, a, &sa);
            for (k = 0; k < m; ++k) rec[k].type = sc->s[idx[k]].type, rec[k].variant = sc->s[idx[k]].variant, rec[k].here = 1;
            RmPlanRoom(&r, sc, idx, m, rec, yes, b, &sb);
            for (k = 0; k < m; ++k) okReroll &= b[k].type == a[k].type && b[k].variant == a[k].variant && b[k].adopt;
        }
        for (k = 0; k < m; ++k) rec[k].here = 0;  // the stock records elsewhere (followers of a save): as placed, kept
        RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
        for (k = 0; k < m; ++k) okAway &= b[k].why == RMW_AWAY && !b[k].drop && !b[k].adopt;
        okAway &= !sb.changed && sb.away == m && !sb.adopted;
        for (k = 0; k < m; ++k)  // records of another type that Refined did not make, here: RZ349-SAVE-001's protection
            rec[k].here = 1, rec[k].type = sc->s[idx[k]].type == 8 ? 2 : 8, rec[k].variant = 0;
        RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
        for (k = 0; k < m; ++k) okOther &= b[k].why == RMW_OTHER && !b[k].drop && !b[k].adopt && b[k].type == sc->s[idx[k]].type;
        okOther &= !sb.changed && sb.other == m;
        for (k = 0; k < m; ++k)  // Refined's own stock records (Change Each Visit Off): kept as v3.49 did
            rec[k].type = sc->s[idx[k]].type, rec[k].variant = sc->s[idx[k]].variant, rec[k].ours = 1;
        RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
        for (k = 0; k < m; ++k) okOurs &= b[k].why == RMW_SAME && !b[k].drop && !b[k].adopt;
        okOurs &= !sb.changed && sb.same == m;
    }
    CHECK(okSame && okDrop && tot == 71 && totAd == 71 && totCh > 30, "v3.50 RM350: the game's own stock records here are adopted: "
          "the roll as in a fresh game, deleted exactly when the type changes (%d of %d changed; v3.49: 0)", totCh, tot);
    CHECK(okReroll, "v3.50 RM350: adopted with Change Each Visit On: the roll of the visit");
    CHECK(okAway, "v3.50 RM350: the game's stock records elsewhere: as placed, never deleted, not adopted");
    CHECK(okOther, "v3.50 RM350: records of another type that Refined did not make: as placed, never deleted (RZ349-SAVE-001)");
    CHECK(okOurs, "v3.50 RM350: Refined's own stock records, Change Each Visit Off: kept stock (v3.49's rule)");
    {   // an adopted pick put back (files, words): the stock type, the record kept, the counts follow
        int idx[RM_ROOM_MAX], m = 0, found = 0;
        RmRec rec[RM_ROOM_MAX];
        RmPick b[RM_ROOM_MAX];
        RmPlanStat sb;
        for (room = 1; room < RZ_ROOMS && !found; ++room) {
            RmPlanIn in = In(RMM_WORLD, 5, 4, 1, room, 0, 0, RM_ALL_TARGETS);
            if (!(m = RoomIdx(sc, room, idx))) continue;
            for (k = 0; k < m; ++k)
                rec[k].type = sc->s[idx[k]].type, rec[k].variant = sc->s[idx[k]].variant, rec[k].here = 1, rec[k].ours = 0;
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            for (k = 0; k < m && !found; ++k)
                if (b[k].drop) {
                    const RmPlanStat s0 = sb;
                    RmPick f = b[k], w = b[k];
                    RmPlanStat s1 = sb, s2 = sb;
                    found = 1;
                    RmRevertPick(&f, &sc->s[idx[k]], &rec[k], &s1);
                    RmPickBack(&w, &sc->s[idx[k]], &rec[k], &s2, RMW_WORDS);
                    CHECK(f.type == sc->s[idx[k]].type && !f.drop && f.adopt && f.why == RMW_FILES && s1.changed == s0.changed - 1 &&
                          s1.adoptChanged == s0.adoptChanged - 1 && s1.drops == s0.drops - 1 && s1.files == s0.files + 1,
                          "v3.50 RM350: an adopted pick whose files fail: stock, its record kept, the counts follow");
                    CHECK(w.type == sc->s[idx[k]].type && !w.drop && w.why == RMW_WORDS && s2.changed == s0.changed - 1 &&
                          s2.drops == s0.drops - 1 && s2.files == s0.files, "v3.50 RM350: an adopted pick whose words were not stock: "
                          "stock, its record kept");
                }
        }
        CHECK(found, "v3.50 RM350: an adopted pick that changes (for the put-back checks)");
    }
    {   // the bone budget and the Hummers' floor: an adopted placement kept stock keeps its record
        const RmScan *h = &g_sc[5][1];
        int okB = 1, okF = 1, n = 0;
        for (room = 1; room < RZ_ROOMS; ++room) {
            int idx[RM_ROOM_MAX], m = RoomIdx(h, room, idx);
            RmRec rec[RM_ROOM_MAX];
            RmPick b[RM_ROOM_MAX];
            RmPlanStat sb;
            RmPlanIn in = In(RMM_WORLD, 11, 5, 1, room, 0, RM_ALL_TARGETS, RM_ALL_TARGETS);
            if (!m) continue;
            for (k = 0; k < m; ++k) rec[k].type = h->s[idx[k]].type, rec[k].variant = h->s[idx[k]].variant, rec[k].here = 1, rec[k].ours = 0;
            RmPlanRoom(&in, h, idx, m, rec, no, b, &sb);
            for (k = 0; k < m; ++k)
                if (RmIsFlyer(h->s[idx[k]].type)) {
                    okF &= RmIsFlyer(b[k].type) && (b[k].why != RMW_FLOOR || !b[k].drop);
                    n += b[k].why == RMW_FLOOR;
                }
            in.freeBones = 0;
            RmPlanRoom(&in, h, idx, m, rec, yes, b, &sb);
            for (k = 0; k < m; ++k) okB &= b[k].why != RMW_BONES || (!b[k].drop && b[k].type == h->s[idx[k]].type);
            okB &= sb.cost <= sb.limit || sb.cost <= sb.cost0;
        }
        CHECK(okF && n > 0, "v3.50 RM350: an adopted Hummer without a floor stays a Hummer, its record kept (%d)", n);
        CHECK(okB, "v3.50 RM350: an adopted placement over the bone budget stays stock, its record kept");
    }
    {   // the user's v3.49 room 5:11 (visit 1, #1; seed 794655925, This World, Change Each Visit On): 8 Whitestools, all kept
        const RmScan *b5 = &g_sc[5][1];
        int idx[RM_ROOM_MAX], m = RoomIdx(b5, 11, idx);
        RmRec rec[RM_ROOM_MAX];
        RmPick b[RM_ROOM_MAX];
        RmPlanStat sb;
        RmPlanIn in = In(RMM_WORLD, 794655925u, 5, 1, 11, 1, 1u << RmTargetIndex(2, 0), RM_ALL_TARGETS);
        in.reroll = 1;
        for (k = 0; k < m; ++k) rec[k].type = b5->s[idx[k]].type, rec[k].variant = b5->s[idx[k]].variant, rec[k].here = 1, rec[k].ours = 0;
        RmPlanRoom(&in, b5, idx, m, rec, yes, b, &sb);
        printf("  the user's room 5:11 with v3.50: %d of %d Whitestools change (all 8 adopted)\n", sb.changed, m);
        CHECK(m == 8 && sb.adopted == 8 && sb.changed >= 4 && sb.drops == sb.changed, "v3.50 RM350: the user's room 5:11 from a save: "
              "%d of 8 change (v3.49: 0)", sb.changed);
    }
}
static void TestAllWorlds(void)
{
    unsigned char yes[RM_ROOM_MAX];
    Sum s1;
    int stage, h, t, by[RM_TARGET_N], outsideWorld = 0, gum = 0;
    RmPlanIn in = In(RMM_ALL, 1, 4, 1, 0, 0, 0, RM_ALL_TARGETS);
    memset(yes, 1, sizeof yes), memset(by, 0, sizeof by);
    CHECK(RmPool(&in) == RM_ALL_TARGETS, "v3.50 RM350 All Worlds: every target, nothing resident (%03X)", RmPool(&in));
    in.allowed = RM_ALL_TARGETS & ~RM_GUM_TARGETS;
    CHECK(RmPool(&in) == (RM_ALL_TARGETS & ~RM_GUM_TARGETS), "v3.50 RM350 All Worlds: Gum Heads only with the guard");
    in.mode = RMM_WORLD, in.allowed = RM_ALL_TARGETS;
    CHECK(RmPool(&in) == in.world && in.world != RM_ALL_TARGETS, "This World stays this world's (%03X)", RmPool(&in));
    in.mode = RMM_ROOM, in.resident = 1u << RmTargetIndex(2, 0);
    CHECK(RmPool(&in) == in.resident, "This Room stays the resident ones");
    for (stage = 2; stage <= 8; ++stage)
        for (h = 0; h < 1 + g_has1[stage]; ++h) {
            const RmScan *sc = &g_sc[stage][h];
            const unsigned world = g_sc[stage][0].placed | (g_has1[stage] ? g_sc[stage][1].placed : 0u);
            Sum sw;
            if (!sc->n) continue;
            in = In(RMM_ALL, 777, stage, h, 0, 0, 0, RM_ALL_TARGETS);
            in.freeBones = 1 << 24;
            PlanAll(sc, in, yes, &s1, NULL);
            CHECK(!s1.outside && s1.picks == sc->n, "All Worlds stage %d half %d: every pick in the pool", stage, h);
            for (t = 0; t < RM_TARGET_N; ++t) {
                by[t] += s1.byTarget[t];
                if (!(world >> t & 1u)) outsideWorld += s1.byTarget[t];
            }
            gum += s1.gum;
            in.mode = RMM_WORLD;
            PlanAll(sc, in, yes, &sw, NULL);
            for (t = 0; t < RM_TARGET_N; ++t)
                CHECK((world >> t & 1u) || !sw.byTarget[t], "This World stage %d half %d: no %s from another world", stage, h,
                      kRmTargets[t].name);
            in.mode = RMM_ALL, in.allowed = RM_ALL_TARGETS & ~RM_GUM_TARGETS;
            PlanAll(sc, in, yes, &sw, NULL);
            CHECK(!sw.gum, "All Worlds stage %d half %d: no Gum Head without the guard", stage, h);
        }
    for (t = 0; t < RM_TARGET_N; ++t) CHECK(by[t] > 5, "All Worlds: %s placed (%d)", kRmTargets[t].name, by[t]);
    CHECK(outsideWorld > 50 && gum > 0, "All Worlds: %d placement(s) of another world's types (silent ones allowed)", outsideWorld);
}
// v3.50 RM350 (B2): a save made in a randomized room and loaded back gives a working room
static void TestSaveRoundTrip(void)
{
    static unsigned char recs[RM_REC_N * RM_REC_SIZE], save[RM_REC_N * RM_REC_SIZE], old[RM_REC_N * RM_REC_SIZE];
    static RmProv pv, pv2;
    const RmScan *sc = &g_sc[4][1];
    const int stage = 4, half = 1;
    int room = 0, k, idx[RM_ROOM_MAX], m = 0, ch = 0, seed = 0, n, okSame, okOurs, found = 0;
    RmPick p1[RM_ROOM_MAX], p2[RM_ROOM_MAX];
    RmPlanStat st;
    RmPlanIn in;
    // a room and seed where the adopted placements both change and keep the stock type
    for (seed = 1; seed < 400 && !found; ++seed)
        for (room = 1; room < RZ_ROOMS && !found; ++room) {
            if ((m = RoomIdx(sc, room, idx)) < 3) continue;
            memset(recs, 0, sizeof recs), memset(&pv, 0, sizeof pv);
            StockRecs(recs, sc, stage, half, room, 1);
            RoomLoad(recs, &pv, sc, stage, half, room, In(RMM_WORLD, (unsigned)seed, stage, half, room, 0, 0, RM_ALL_TARGETS), p1, &st);
            ch = st.changed;
            found = ch > 0 && ch < m;
        }
    --seed, --room;  // (the loops stepped past the one found)
    if (!found) {
        CHECK(0, "save round trip: no room / seed with changed and kept placements");
        return;
    }
    in = In(RMM_WORLD, (unsigned)seed, stage, half, room, 0, 0, RM_ALL_TARGETS);
    CHECK(ch > 0 && ch < m && st.adopted == m && st.drops == ch, "save round trip: room 4:%d seed %d: the save's %d stock records "
          "adopted, %d changed (their records deleted)", room, seed, m, ch);
    for (k = 0, okOurs = 1, n = 0; k < m; ++k) {
        const unsigned char *r = RmRecFind(recs, RM_REC_N, EnmScriptId(stage, half, room, sc->s[idx[k]].n));
        const int changed = p1[k].type != sc->s[idx[k]].type || p1[k].variant != sc->s[idx[k]].variant;
        okOurs &= changed ? !r : r && RmProvOurs(&pv, r) && !RmProvRetyped(&pv, r);
        n += r != NULL;
    }
    CHECK(okOurs && n == m - ch, "save round trip: the kept stock records are Refined's now (provenance), the changed ones gone");
    memcpy(old, recs, sizeof recs);  // (an older save: before these monsters were made; the changed ones' records gone)
    StockRecs(old, sc, stage, half, room, 1);  // (... and an even older one: every stock record)
    RoomLeave(recs, sc, stage, half, room, p1);  // the room is left: the new types write their records
    for (k = 0, n = 0; k < RM_REC_N; ++k) n += RmProvRetyped(&pv, recs + k * RM_REC_SIZE);
    CHECK(n == ch, "save round trip: %d record(s) of the new types (Refined's retypes)", n);
    memcpy(save, recs, sizeof recs);
    CHECK(RmSaveClean(save, &pv) == ch, "save round trip: the save filter leaves the %d retyped record(s) out", ch);
    for (k = 0, n = 0; k < RM_REC_N; ++k) n += RmProvRetyped(&pv, save + k * RM_REC_SIZE);
    for (k = 0, okSame = 1; k < m; ++k)
        okSame &= (RmRecFind(save, RM_REC_N, EnmScriptId(stage, half, room, sc->s[idx[k]].n)) != NULL) ==
                  (p1[k].type == sc->s[idx[k]].type && p1[k].variant == sc->s[idx[k]].variant);
    CHECK(!n && okSame, "save round trip: the save holds no randomized monster; the stock ones' records stay in it");
    // load that save in the same session: the room comes back the same, nothing deleted, nothing adopted
    memcpy(recs, save, sizeof recs);
    RoomLoad(recs, &pv, sc, stage, half, room, in, p2, &st);
    for (k = 0, okSame = 1; k < m; ++k) okSame &= p2[k].type == p1[k].type && p2[k].variant == p1[k].variant && !p2[k].drop;
    CHECK(okSame && st.changed == ch && !st.adopted && !st.drops, "save round trip: loaded back (same session): the same monsters, "
          "no record deleted");
    // load it in a new session (no provenance): the stock records are adopted again: the same monsters
    memcpy(recs, save, sizeof recs), memset(&pv2, 0, sizeof pv2);
    RoomLoad(recs, &pv2, sc, stage, half, room, in, p2, &st);
    for (k = 0, okSame = 1; k < m; ++k) okSame &= p2[k].type == p1[k].type && p2[k].variant == p1[k].variant && !p2[k].drop;
    CHECK(okSame && st.changed == ch && st.adopted == m - ch, "save round trip: loaded in a new session: the same monsters (%d "
          "adopted)", st.adopted);
    // an older save with every stock record, loaded in the same session (the provenance says the new types): adopted, the same
    memcpy(recs, old, sizeof recs);
    RoomLoad(recs, &pv, sc, stage, half, room, in, p2, &st);
    for (k = 0, okSame = 1; k < m; ++k) okSame &= p2[k].type == p1[k].type && p2[k].variant == p1[k].variant;
    CHECK(okSame && st.adopted == ch && st.drops == ch, "save round trip: an older save (stock records): the same monsters, the %d "
          "stock record(s) of changed placements deleted", ch);
    // another writer's record of another type in the save: kept by the plan and by the save filter
    {
        unsigned char *r;
        const unsigned id = EnmScriptId(stage, half, room, sc->s[idx[0]].n);
        const int other = sc->s[idx[0]].type == 8 ? 2 : 8;
        memset(recs, 0, sizeof recs), memset(&pv2, 0, sizeof pv2);
        Rec(recs, id, other, 1, 0, stage, room);
        RoomLoad(recs, &pv2, sc, stage, half, room, in, p2, &st);
        r = (unsigned char *)RmRecFind(recs, RM_REC_N, id);
        CHECK(r && RmRecType(r) == other && p2[0].why == RMW_OTHER && p2[0].type == sc->s[idx[0]].type && st.other == 1,
              "save round trip: another writer's record: the placement as placed, the record kept");
        memcpy(save, recs, sizeof recs);
        CHECK(RmSaveClean(save, &pv2) == 0 && RmRecFind(save, RM_REC_N, id), "save round trip: another writer's record stays in a save");
    }
    // Monsters Off after the randomized visit: only Refined's retypes in the room are deleted
    memset(recs, 0, sizeof recs), memset(&pv, 0, sizeof pv);
    StockRecs(recs, sc, stage, half, room, 1);
    RoomLoad(recs, &pv, sc, stage, half, room, in, p1, &st);
    RoomLeave(recs, sc, stage, half, room, p1);
    for (k = 0, n = 0, okSame = 1; k < m; ++k) {
        const unsigned char *r = RmRecFind(recs, RM_REC_N, EnmScriptId(stage, half, room, sc->s[idx[k]].n));
        const int changed = p1[k].type != sc->s[idx[k]].type || p1[k].variant != sc->s[idx[k]].variant;
        n += RmOffDrops(&pv, r, stage, room);
        okSame &= RmOffDrops(&pv, r, stage, room) == changed;
    }
    CHECK(okSame && n == ch, "save round trip: Monsters Off next visit: the %d retyped record(s) deleted, the adopted stock ones kept", n);
}

// ---------------------------------------------------------------- the words of a plan in the scripts (a copy of the image)
static void TestWords(void)
{
    static RmPatchStack st;
    const int stage = 4, half = 1;
    RmScan *sc = &g_sc[stage][half], after;
    RzWin w = ExeWin();
    unsigned char yes[RM_ROOM_MAX];
    int room, total = 0, changed = 0, match = 0, restored = 1, k;
    memset(yes, 1, sizeof yes);
    memset(&st, 0, sizeof st);
    for (room = 1; room < RZ_ROOMS; ++room) {
        int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), d, mismatch = 0;
        RmRec rec[RM_ROOM_MAX];
        RmPick p[RM_ROOM_MAX];
        RmPlanStat ps;
        RmPlanIn in = In(RMM_WORLD, 4242, stage, half, room, 0, RM_ALL_TARGETS, RM_ALL_TARGETS);
        if (!m) continue;
        NoRecs(rec, m);
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &ps);
        d = RmStackPush(&st);
        for (k = 0; k < m; ++k) {
            const RmSlot *s = &sc->s[idx[k]];
            const unsigned mw = RmModeWord(s->type, s->mw, p[k].type, p[k].variant);
            if (p[k].type == s->type && p[k].variant == s->variant) continue;
            if (mw != s->mw) RmStackAdd(&st, d, ImgPtr(s->modeVa), s->mw, mw);
            RmStackAdd(&st, d, ImgPtr(s->typeVa), s->type, p[k].type);
            ++changed;
        }
        RmScanScript(&w, Script(stage, half), stage, &after);  // the runner's view of the patched script
        for (k = 0; k < m; ++k) {
            const int j = RmSlotOf(&after, room, sc->s[idx[k]].n);
            ++total;
            if (j >= 0 && after.s[j].type == p[k].type && after.s[j].variant == p[k].variant) ++match;
            else ++mismatch;
        }
        CHECK(!mismatch, "room 4:%d: the patched script holds the planned types", room);
        CHECK(RmStackPop(&st, d) == 0 && st.depth == 0, "room 4:%d: every word restored", room);
        RmScanScript(&w, Script(stage, half), stage, &after);
        restored &= after.n == sc->n && !memcmp(after.s, sc->s, sizeof sc->s[0] * (size_t)sc->n);
    }
    CHECK(total == 71 && match == 71 && changed > 30, "Water Prison 2nd half: %d of %d planned types in the scripts (%d changed)", match,
          total, changed);
    CHECK(restored && RmStockMatch(stage, half, Script(stage, half), &after) == 1, "the restored scripts are the stock ones");
}

// ---------------------------------------------------------------- made-up scripts (each exclusion)
static unsigned g_buf[1024];
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
static void Enemy(unsigned type, unsigned mw)  // a plain enemy: kind 2, its type, its mode word, a position, a mask
{
    Put(1, 1, 2), Put(7, 1, type), Put(8, 3, 0x42C80000u, 0, 0x43480000u), Put(0x0B, 1, mw), Put(0x0C, 1, 0x1F);
}
static RzWin BufWin(void)
{
    RzWin w;
    w.base = (const unsigned char *)g_buf, w.lo = 0x10000u, w.hi = 0x10000u + 4u * (unsigned)g_bn;
    return w;
}
static void TestMadeUp(void)
{
    static RmScan sc;
    RzWin w;
    g_bn = 0;
    Put(0, 1, 5);                                              // room 5
    Enemy(0x0A, 0);                                            // n 0: a dog (the room's first command is a tag 1: no flush)
    Enemy(0x0A, 0x0006);                                       // n 1: an event dog (mode 6)
    Enemy(0x03, 0x0120);                                       // n 2: a Hummer on a wall
    Enemy(0x07, 0x1100);                                       // n 3: a Gum Head with an item
    Enemy(0x09, 0x3110);                                       // n 4: a Tremer with its room box: a source
    Enemy(0x08, 0x0200);                                       // n 5: Twin Victim v2: left alone
    Enemy(0x0A, 0), Put(0x0F, 1, 0xFFFF0003u);                 // n 6: an id override
    Enemy(0x0A, 0), Put(0x19, 1, 0x130);                       // n 7: a story flag
    Enemy(0x0A, 0), Put(0x0D, 1, 0);                           // n 8: two mode words
    Put(1, 1, 2), Put(6, 1, RM_TYPE_TAB + 16 * 0x0A), Put(8, 3, 0, 0, 0), Put(0x0B, 1, 0);  // n 9: command 6
    Enemy(0x06, 0);                                            // n 10: a Wheelchair
    Enemy(0x0A, 0), Put(3, 1, 0x401000u);                      // n 11: its own create
    Put(1, 1, 2), Put(7, 1, 0x0A), Put(8, 3, 0, 0, 0), Put(0x0B, 1, 0), Put(0x0E, 1, 0x0F);  // n 12: difficulty mask 0F
    Enemy(0x10, 0);                                            // n 13: Walter (a boss room)
    Put(1, 1, 4), Put(0x0F, 1, 9);                             // n 14: a pickup
    Put(0, 1, 6);                                              // room 6
    Put(1, 1, 3);                                              // n 0: an NPC
    Enemy(0x02, 0);                                            // n 1: a Whitestool (the end of the room flushes the last)
    g_buf[g_bn++] = 0xF0F0F0F0u;
    w = BufWin();
    CHECK(RmScanScript(&w, 0x10000u, 4, &sc) && sc.rooms == 2 && sc.kind2 == 15, "made up: scanned (rooms %d, kind2 %d)", sc.rooms,
          sc.kind2);
    CHECK(sc.n == 3 && sc.s[0].room == 5 && sc.s[0].n == 0 && sc.s[0].type == 0x0A && sc.s[1].n == 4 && sc.s[1].type == 9 &&
          sc.s[1].mw == 0x3110 && sc.s[2].room == 6 && sc.s[2].n == 1 && sc.s[2].type == 2, "made up: the sources (%d): n 0 dog, n 4 "
          "Tremer, room 6 n 1 Whitestool", sc.n);
    CHECK(sc.s[0].typeVa == 0x10000u + 4 * 5 && sc.s[0].modeVa == sc.s[0].typeVa + 4 * 6 && sc.s[0].yVa == sc.s[0].typeVa + 4 * 3,
          "made up: the words of n 0");
    CHECK(sc.roomBoss[5] && !sc.roomBoss[6] && sc.roomN[5] == 2 && sc.roomN[6] == 1, "made up: room 5 has a boss");
    CHECK(sc.roomCost[6] == RmPlaceCost(2) && sc.roomCost[5] > 10 * RmPlaceCost(0x0A), "made up: room costs");
    CHECK((sc.placed >> RmTargetIndex(0x0A, 0) & 1u) && !(sc.placed >> RmTargetIndex(9, 0) & 1u) && !(sc.placed >> RmTargetIndex(8, 1) & 1u) &&
          (sc.types >> 0x10 & 1u) && (sc.types >> 6 & 1u), "made up: the placed masks");
    CHECK(RmScanScript(&w, 0x10000u, 1, &sc) && sc.n == 0 && sc.kind2 == 15, "made up: stage 1 (Room 302): no source");
    CHECK(RmScanScript(&w, 0x10000u, 9, &sc) && sc.n == 0, "made up: stage 9: no source");
    {
        RzWin s = w;
        s.hi -= 8;
        CHECK(!RmScanScript(&s, 0x10000u, 4, &sc) && sc.bad, "made up: a script running past the window is refused");
    }
}

// ---------------------------------------------------------------- the descriptor transform, the records, the save filter
static void TestDesc(void)
{
    unsigned char d[0x94];
    unsigned row[4] = { 0x48DAC0u, 0x48E690u, 0x48DAC0u, 0x401000u }, v;
    RmSlot s;
    memset(&s, 0, sizeof s);
    s.type = 9, s.variant = 1, s.mw = 0x3110;
    memset(d, 0xAB, sizeof d);
    CHECK(!RmFixDesc(d, &s, 9, 1, row) && d[4] == 0xAB, "the stock type and variant: the descriptor stays");
    CHECK(RmFixDesc(d, &s, 0x0A, 1, row) && !memcmp(d + 4, row, 16), "another type: its 4 functions");
    memcpy(&v, d + 0x24, 4);
    CHECK(v == 0x0100 && d[0x23] == 0xAB && d[0x28] == 0xAB, "another type: its mode word only");
    CHECK(RmFixDesc(d, &s, 9, 0, row), "the same type at another variant");
    memcpy(&v, d + 0x24, 4);
    CHECK(v == 0x3010, "the same type keeps its own bits (the Tremer's room box and turn)");
}
static void Rec(unsigned char *r, unsigned id, int type, int state, int variant, int stage, int room)
{
    short t = (short)type, st = (short)state;
    memset(r, 0, RM_REC_SIZE);
    memcpy(r + 0x20, &id, 4), memcpy(r + 0x24, &t, 2), memcpy(r + 0x26, &st, 2);
    r[0x33] = (unsigned char)variant, r[0x36] = (unsigned char)stage, r[0x37] = (unsigned char)room;
    r[0x00] = 0x11;  // (a position byte: cleared with the record)
}
static void TestRecords(void)  // v3.49 fix2 RZ349-SAVE-001: provenance (only Refined's own records are filtered / dropped)
{
    static unsigned char recs[RM_REC_N * RM_REC_SIZE], zero[RM_REC_SIZE], keep[RM_REC_N * RM_REC_SIZE];
    static RmProv pv;
    const RmScan *sc = &g_sc[4][1];
    const RmSlot *a = &sc->s[0], *b = &sc->s[1], *c = &sc->s[2], *e = &sc->s[3];
    const unsigned ida = EnmScriptId(4, 1, a->room, a->n), idb = EnmScriptId(4, 1, b->room, b->n);
    const unsigned idc = EnmScriptId(4, 1, c->room, c->n), ide = EnmScriptId(4, 1, e->room, e->n);
    const int ta = a->type == 8 ? 0x0A : 8, tc = c->type == 9 ? 2 : 9, te = e->type == 8 ? 0x0A : 8;
    memset(recs, 0, sizeof recs), memset(&pv, 0, sizeof pv);
    CHECK(RmSaveClean(recs, &pv) == 0, "nothing randomized this session: the save filter changes nothing");
    Rec(recs + 0 * RM_REC_SIZE, ida, ta, 1, 0, 4, a->room);                              // Refined's retype: filtered
    Rec(recs + 1 * RM_REC_SIZE, idb, b->type, 1, b->variant, 4, b->room);               // Refined made it stock: kept
    Rec(recs + 2 * RM_REC_SIZE, 0xFFFF0005u, 0x0B, 1, 0, 4, 3);                         // a ghost: kept
    Rec(recs + 3 * RM_REC_SIZE, idc, tc, 1, 0, 4, c->room);                             // another writer's retype (no entry): kept
    Rec(recs + 4 * RM_REC_SIZE, ida, ta, 0, 0, 4, a->room);                             // a free slot (state 0): ignored
    Rec(recs + 5 * RM_REC_SIZE, ide, te ^ 0x02, 1, 0, 4, e->room);                      // Refined's retype changed later by another writer: kept
    CHECK(RmRecFind(recs, RM_REC_N, ida) == recs && RmRecFind(recs, RM_REC_N, idb) == recs + RM_REC_SIZE &&
          !RmRecFind(recs, RM_REC_N, 0x12345678u), "RmRecFind (live records only)");
    memcpy(keep, recs, sizeof recs);
    CHECK(RmSaveClean(recs, &pv) == 0 && !memcmp(recs, keep, sizeof recs), "an empty provenance table: every record kept "
          "(another writer's retypes too)");
    RmProvPut(&pv, ida, ta, 0, a->type, a->variant);
    RmProvPut(&pv, idb, b->type, b->variant, b->type, b->variant);
    RmProvPut(&pv, ide, te, 0, e->type, e->variant);  // (Refined made it te; the record now holds another type)
    CHECK(pv.n == 3 && RmProvFind(&pv, ida) && !RmProvFind(&pv, idc), "provenance entries");
    RmProvPut(&pv, ida, ta, 0, a->type, a->variant);
    CHECK(pv.n == 3, "an entry is overwritten, not added twice");
    CHECK(RmProvOurs(&pv, recs) && RmProvRetyped(&pv, recs), "Refined's retype: ours, retyped");
    CHECK(RmProvOurs(&pv, recs + RM_REC_SIZE) && !RmProvRetyped(&pv, recs + RM_REC_SIZE), "Refined's stock build: ours, not retyped");
    CHECK(!RmProvOurs(&pv, recs + 3 * RM_REC_SIZE) && !RmProvRetyped(&pv, recs + 3 * RM_REC_SIZE), "another writer's retype: not ours");
    CHECK(!RmProvOurs(&pv, recs + 5 * RM_REC_SIZE), "Refined's retype changed by another writer: not ours any more");
    CHECK(!RmProvOurs(&pv, recs + 4 * RM_REC_SIZE) && !RmProvOurs(&pv, recs + 2 * RM_REC_SIZE) && !RmProvOurs(&pv, NULL),
          "a free slot, a ghost, no record: not ours");
    CHECK(RmOffDrops(&pv, recs, 4, a->room) && !RmOffDrops(&pv, recs, 4, a->room + 1) && !RmOffDrops(&pv, recs + RM_REC_SIZE, 4, b->room)
          && !RmOffDrops(&pv, recs + 3 * RM_REC_SIZE, 4, c->room) && !RmOffDrops(&pv, recs + 5 * RM_REC_SIZE, 4, e->room),
          "Monsters Off: only Refined's retype in this room is deleted (another writer's retypes kept)");
    CHECK(RmSaveClean(recs, &pv) == 1 && !memcmp(recs, zero, RM_REC_SIZE) && !memcmp(recs + RM_REC_SIZE, keep + RM_REC_SIZE,
          5 * RM_REC_SIZE), "the save filter clears Refined's retype only (stock build, ghost, another writer's, a changed one kept)");
    CHECK(RmRecVariant(recs + RM_REC_SIZE) == b->variant && RmRecStage(recs + RM_REC_SIZE) == 4 && RmRecRoom(recs + RM_REC_SIZE) == b->room,
          "record fields");
    {   // the table never overflows silently
        static RmProv big;
        int i;
        memset(&big, 0, sizeof big);
        for (i = 0; i < RM_PROV_N + 3; ++i) RmProvPut(&big, 0x02010000u + (unsigned)i, 8, 0, 2, 0);
        CHECK(big.n == RM_PROV_N && big.full == 3 && !RmProvFind(&big, 0x02010000u + RM_PROV_N), "a full table counts, adds nothing");
    }
}
static void TestStack(void)
{
    static RmPatchStack st;
    unsigned w[RM_PATCH_MAX + 4];
    int i, d0, d1, ok = 1;
    memset(&st, 0, sizeof st);
    for (i = 0; i < RM_PATCH_MAX + 4; ++i) w[i] = (unsigned)i;
    d0 = RmStackPush(&st);
    CHECK(d0 == 0 && RmStackAdd(&st, d0, &w[0], 0, 100) && w[0] == 100, "a word written");
    CHECK(!RmStackAdd(&st, d0, &w[1], 5, 101) && w[1] == 1, "a word that is not the stock value is left");
    d1 = RmStackPush(&st);
    CHECK(d1 == 1 && RmStackAdd(&st, d1, &w[2], 2, 102), "a nested run's word");
    w[2] = 77;  // another writer
    CHECK(RmStackPop(&st, d1) == 1 && w[2] == 77 && st.depth == 1, "the nested run: left as found");
    CHECK(RmStackPop(&st, d0) == 0 && w[0] == 0 && st.depth == 0, "the outer run restored");
    for (i = 0; i < RM_PATCH_MAX + 4; ++i) w[i] = (unsigned)i;
    d0 = RmStackPush(&st);
    for (i = 0; i < RM_PATCH_MAX + 4; ++i) ok &= RmStackAdd(&st, d0, &w[i], (unsigned)i, 1000u + (unsigned)i) == (i < RM_PATCH_MAX);
    CHECK(ok && w[RM_PATCH_MAX] == (unsigned)RM_PATCH_MAX, "the list holds RM_PATCH_MAX words, the next is not written");
    CHECK(RmStackTrim(&st, d0, 2) == 0 && w[2] == 2 && w[1] == 1001 && st.n[d0] == 2, "trim back to 2");
    RmStackPop(&st, d0);
    for (i = 0, ok = 1; i < RM_PATCH_MAX; ++i) ok &= w[i] == (unsigned)i;
    CHECK(ok && st.depth == 0, "all restored");
    for (i = 0; i < RZ_DEPTH + 2; ++i) RmStackPush(&st);
    CHECK(!RmStackAdd(&st, RZ_DEPTH, &w[0], 0, 1) && w[0] == 0, "deeper than RZ_DEPTH: nothing written");
    RmStackPop(&st, 0);
    CHECK(st.depth == 0, "popped to 0");
    CHECK(RmSlotKey(1, 4, 0, 3, 2) != RmSlotKey(1, 4, 1, 3, 2) && RmSlotKey(1, 4, 0, 3, 2) != RmSlotKey(2, 4, 0, 3, 2) &&
          RmSlotKey(1, 4, 0, 3, 2) != RmSlotKey(1, 4, 0, 3, 3) && RmSlotKey(1, 4, 0, 3, 2) == RmSlotKey(1, 4, 0, 3, 2), "the key");
}

int main(int argc, char **argv)
{
    const char *exe = argc > 1 ? argv[1] : "C:\\ClaudeCode\\CModding\\SH4Decomp\\orig\\sh4.exe";
    TestTexts();
    TestExe(exe);
    if (g_img) {
        TestPlans();
        TestWords();
        TestRecords();
        TestAdopt();          // v3.50 RM350
        TestAllWorlds();      // v3.50 RM350
        TestSaveRoundTrip();  // v3.50 RM350
    }
    TestMadeUp();
    TestDesc();
    TestStack();
    printf("rzmon_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
