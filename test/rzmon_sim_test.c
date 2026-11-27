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
    // v3.56 RE356: the row is Enemies, Off / Same World / All Worlds; the old This Room / This World read as Same World
    CHECK(RmMonOfText("Off") == RMM_OFF && RmMonOfText("this room") == RMM_WORLD && RmMonOfText(" THIS WORLD ") == RMM_WORLD &&
          RmMonOfText("Same World") == RMM_WORLD && RmMonOfText(" same world ") == RMM_WORLD, "v3.56 RE356: Enemies names; the old "
          "This Room and This World migrate to Same World");
    CHECK(RmMonOfText("Room") == RMM_WORLD && RmMonOfText("world") == RMM_WORLD && RmMonOfText("0") == 0 && RmMonOfText("1") == RMM_WORLD &&
          RmMonOfText("2") == RMM_WORLD && RmMonOfText("4") == -1 && RmMonOfText("") == -1 && RmMonOfText("Everywhere") == -1 &&
          RmMonOfText(NULL) == -1, "v3.56 RE356: short names and numbers (1 = the old This Room: Same World), junk");
    CHECK(RmMonOfText("All Worlds") == RMM_ALL && RmMonOfText(" all worlds ") == RMM_ALL && RmMonOfText("All") == RMM_ALL &&
          RmMonOfText("3") == RMM_ALL && RMM_N == 4 && !strcmp(kRmMonName[RMM_ALL], "All Worlds") &&
          !strcmp(kRmMonName[RMM_WORLD], "Same World") && RMM_OFF == 0 && RMM_ROOM == 1 && RMM_WORLD == 2,
          "v3.50 RM350: All Worlds (3 / All); v3.56 RE356: This World is named Same World, the numbers unchanged");
    CHECK(RM_MON_DEFAULT == RMM_WORLD, "the host's default: Same World (was This World)");
    CHECK(RM_ROW_N == 3 && kRmRowMode[0] == RMM_OFF && kRmRowMode[1] == RMM_WORLD && kRmRowMode[2] == RMM_ALL && RmRowOfMode(RMM_OFF) == 0 &&
          RmRowOfMode(RMM_ROOM) == 1 && RmRowOfMode(RMM_WORLD) == 1 && RmRowOfMode(RMM_ALL) == 2 && RmRowOfMode(9) == 0,
          "v3.56 RE356: the row's three values and their modes (This Room never on the row)");
    {   // v3.56 RE356: the renamed keys (Monsters -> Enemies): the new key wins; missing, the old key once; junk = the default
        int v = -9;
        CHECK(RzKeyPick("All Worlds", "Off", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_NEW && v == RMM_ALL, "key pick: the new key wins");
        CHECK(RzKeyPick("", "This Room", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_OLD && v == RMM_WORLD, "key pick: Monsters=This Room -> "
              "Enemies Same World");
        CHECK(RzKeyPick(NULL, "This World", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_OLD && v == RMM_WORLD, "key pick: This World -> "
              "Same World");
        CHECK(RzKeyPick("", "Off", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_OLD && v == RMM_OFF &&
              RzKeyPick("", "All Worlds", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_OLD && v == RMM_ALL, "key pick: Off and All Worlds stay");
        CHECK(RzKeyPick("", "Everywhere", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_OLDBAD && v == RM_MON_DEFAULT, "key pick: an old junk "
              "value: the default (migrated too)");
        CHECK(RzKeyPick("Nowhere", "Off", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_BAD && v == RM_MON_DEFAULT, "key pick: a new junk "
              "value: the default, the old key not read");
        CHECK(RzKeyPick("", "", RmMonOfText, RM_MON_DEFAULT, &v) == RZK_NONE && v == RM_MON_DEFAULT &&
              RzKeyPick(NULL, NULL, RmMonOfText, RM_MON_DEFAULT, &v) == RZK_NONE, "key pick: neither key: the default");
    }
    CHECK(RmIsSourceType(2) && RmIsSourceType(3) && RmIsSourceType(7) && RmIsSourceType(8) && RmIsSourceType(9) &&
          RmIsSourceType(0x0A) && !RmIsSourceType(1) && !RmIsSourceType(4) && !RmIsSourceType(5) && !RmIsSourceType(6) &&
          !RmIsSourceType(0x0B) && !RmIsSourceType(0x0F) && !RmIsSourceType(0x10) && !RmIsSourceType(0x11),
          "source types: never mm, Wall Man, Wheelchair, ghosts, Walter, the Patient (v3.56 RE356: the victims keep their ghosts)");
    CHECK(RmTargetIndex(7, 0) == 2 && RmTargetIndex(7, 1) == 3 && (RM_GUM_TARGETS == (1u << 2 | 1u << 3)) &&
          RmTargetIndex(9, 1) < 0 && RmTargetIndex(3, 1) < 0 && RmTargetIndex(6, 0) < 0, "targets");
    {   // v3.56 RE356: the pool filter: + the six victims' ghosts, never a story ghost, Walter, Wall Man, Wheelchair, mm, the Patient
        int t, v, okIn = 1, okOut = 1;
        for (v = 0; v < 4; ++v) okIn &= RmTargetIndex(0x0B, v) == 9 + v;
        okIn &= RmTargetIndex(0x0C, 1) == 13 && RmTargetIndex(0x0D, 1) == 14 && RM_TARGET_N == 15;
        okOut &= RmTargetIndex(0x0C, 0) < 0 && RmTargetIndex(0x0D, 0) < 0 && RmTargetIndex(0x0E, 0) < 0 && RmTargetIndex(0x0F, 0) < 0 &&
                 RmTargetIndex(0x0F, 1) < 0 && RmTargetIndex(0x10, 0) < 0 && RmTargetIndex(0x11, 0) < 0 && RmTargetIndex(5, 0) < 0 &&
                 RmTargetIndex(5, 1) < 0 && RmTargetIndex(6, 1) < 0 && RmTargetIndex(4, 0) < 0 && RmTargetIndex(1, 0) < 0 &&
                 RmTargetIndex(0x0C, 2) < 0;
        for (t = 0; t < RM_TARGET_N; ++t)
            okIn &= (RmIsGhostTarget(kRmTargets[t].type) != 0) == ((RM_GHOST_TARGETS >> t & 1u) != 0);
        CHECK(okIn && okOut && RM_MON_TARGETS == 0x1FF && RM_GHOST_TARGETS == 0x7E00 && (RM_MON_TARGETS | RM_GHOST_TARGETS) == RM_ALL_TARGETS &&
              !(RM_MON_TARGETS & RM_GHOST_TARGETS), "v3.56 RE356: the pool: 9 monsters + 0B v0..v3, 0C v1, 0D v1; never a story ghost "
              "(0C v0, 0D v0, 0E, 0F), Walter, a Wall Man, a Wheelchair, mm or the Patient");
        CHECK(RmStoryAllowed(2, 0, 0) == (RM_ALL_TARGETS & ~RM_G0D1_TARGET) && RmStoryAllowed(4, 0, 0) == (RM_ALL_TARGETS & ~RM_G0D1_TARGET &
              ~RM_G0B_TARGETS) && RmStoryAllowed(4, 1, 0) == (RM_ALL_TARGETS & ~RM_G0D1_TARGET) && RmStoryAllowed(4, 0, 1) ==
              (RM_ALL_TARGETS & ~RM_G0B_TARGETS) && RmStoryAllowed(4, 1, 1) == RM_ALL_TARGETS && RmStoryAllowed(5, 0, 1) == RM_ALL_TARGETS &&
              RmStoryAllowed(3, 1, 0) == (RM_ALL_TARGETS & ~RM_G0D1_TARGET), "v3.56 RE356: the story rule: no 0B in stage 4 before flag "
              "2D3 (its create sets it there), no 0D v1 anywhere before flag 23B (its create sets it); the monsters always");
    }
    CHECK(RmBitCount(0) == 0 && RmBitCount(0x1FF) == 9 && RmNthBit(0x14, 0) == 2 && RmNthBit(0x14, 1) == 4 && RmNthBit(0x14, 2) == -1,
          "bit helpers");
    CHECK(RmModeOk(2, 0) && RmModeOk(9, 0x0010) && RmModeOk(9, 0x3150) == 0 && RmModeOk(9, 0x3110) && RmModeOk(0x0A, 0x0100) &&
          !RmModeOk(0x0A, 0x0106) && !RmModeOk(0x0A, 0x0103) && !RmModeOk(3, 0x0120) && !RmModeOk(7, 0x1000) && !RmModeOk(3, 0x2000) &&
          !RmModeOk(2, 0x10000), "mode words: spawn mode 0, upright, bits 12-15 only for a Tremer");
    CHECK(RmModeWord(9, 0x3110, 9, 0) == 0x3010 && RmModeWord(9, 0x3110, 0x0A, 1) == 0x0100 && RmModeWord(2, 0, 8, 1) == 0x100 &&
          RmModeWord(0x0A, 0x100, 0x0A, 0) == 0, "mode words given: the same type keeps its bits, another gets its variant only");
    CHECK(RmEventDogMode(0x0A, 7) && RmEventDogMode(0x0A, 6) && RmEventDogMode(0x0A, 0x106) && RmEventDogMode(0x0A, 0x117) &&
          !RmEventDogMode(0x0A, 3) && !RmEventDogMode(0x0A, 0x103) && !RmEventDogMode(0x0A, 0) && !RmEventDogMode(0x0A, 0x0026) &&
          !RmEventDogMode(0x0A, 0x1007) && !RmEventDogMode(0x0A, 0x10007) && !RmEventDogMode(7, 6) && !RmEventDogMode(2, 7) &&
          !RmEventDogMode(3, 6), "v3.51 SN351: event dogs: a Sniffer Dog in spawn mode 6 / 7, upright; never mode 3 alone, never "
          "another type");
    CHECK(RmModeWord(0x0A, 7, 0x0A, 1) == 0x107 && RmModeWord(0x0A, 6, 2, 0) == 0 && RmModeWord(0x0A, 7, 8, 1) == 0x100,
          "v3.51 SN351: an event dog rolled a dog keeps its spawn mode; another type gets spawn mode 0");
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
    int stage, v, t, scripts = 0, total = 0, kind2 = 0, roomMax = 0, words = 0, wordsOk = 0, cmdOk = 0, boss = 0, i, events = 0;
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
                CHECK(RmIsSourceType(s->type) && (s->event ? RmEventDogMode(s->type, s->mw) && (s->mw & 0xF) : RmModeOk(s->type,
                      s->mw) && !(s->mw & 0xF)) && RmSourceVariantOk(s->type, s->variant) && s->room > 0 && s->room < 64 &&
                      stage >= 2 && stage <= 8, "slot rules");  // v3.51 SN351: + the event dogs
                if (s->event) {  // v3.51 SN351: only Subway room 2:2's three, all Sniffer Dogs v0
                    ++events;
                    CHECK(stage == 2 && v == 0 && s->room == 2 && s->n >= 1 && s->n <= 3 && s->type == 0x0A && !s->variant &&
                          s->mw == (s->n == 1 ? 7u : 6u), "event dog %d:%d n%d mode %04X", stage, s->room, s->n, s->mw);
                }
                CHECK(RmSlotOf(sc, s->room, s->n) == k, "RmSlotOf finds it");
            }
            for (k = 0; k < RZ_ROOMS; ++k) boss += sc->roomBoss[k];
            for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
            if (dup) continue;
            seen[scripts++] = p;
            total += sc->n, kind2 += sc->kind2;
        }
    }
    CHECK(total == RM_STOCK_TOTAL && total == 238 && kind2 == RM_STOCK_KIND2 && kind2 == 783,
          "the stock exe: %d source placements of %d kind-2 (tools/rzmon_scan.py: 238 of 783; v3.50: 235)", total, kind2);
    CHECK(events == RM_STOCK_EVENTS && events == 3 && g_sc[2][0].events == 3, "v3.51 SN351: the 3 event dogs of Subway room 2:2 "
          "are sources (%d)", events);
    CHECK(roomMax == 20 && roomMax <= RM_ROOM_MAX, "at most %d sources in one room (20)", roomMax);
    CHECK(words == wordsOk && words == 3 * 238, "every slot word holds the scanned "
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
        CHECK(((g_sc[2][0].placed | g_sc[2][1].placed) & RM_MON_TARGETS) == 0x1C4u && ((g_sc[5][0].placed | g_sc[5][1].placed) &
              RM_MON_TARGETS) == 0x1CFu && (g_sc[8][0].placed & RM_MON_TARGETS) == 0x030u, "This World's targets: Subway 1C4 (Gum Head, Tremer, both dogs), Building 1CF, stage 8 "
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
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 0, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s1, g_pa);
    CHECK(s1.picks == 71 && s1.changed > 20 && !s1.outside && !s1.gum, "This Room: %d of %d changed, all in the pool", s1.changed, s1.picks);
    for (t = 0; t < RM_TARGET_N; ++t)
        CHECK((stockT >> t & 1u) ? s1.byTarget[t] > 10 : !s1.byTarget[t], "This Room: target %s x%d", kRmTargets[t].name, s1.byTarget[t]);
    // the same input = the same plan; another seed = another plan
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 0, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(SamePlans(g_pa, g_pb, sc, &diff), "the same seed: the same monsters");
    PlanAll(sc, In(RMM_ROOM, 1235, 4, 1, 0, 0, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff) && diff > 30, "another seed: %d of 71 differ", diff);
    // Change Each Visit: visit 1 against visit 0, visit 7 against visit 7 (same)
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 1, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff) && diff > 30, "Change Each Visit: the next visit differs (%d of 71)", diff);
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 7, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s1, g_pa);
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 1, 0, 7, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(SamePlans(g_pa, g_pb, sc, &diff), "the same visit count: the same monsters");
    // the halves are keyed apart
    PlanAll(sc, In(RMM_ROOM, 1234, 4, 0, 0, 7, stockT, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s2, g_pb);
    CHECK(!SamePlans(g_pa, g_pb, sc, &diff), "the half is part of the key (%d differ)", diff);
    // This World: resident | placed; every target appears (Gum Heads allowed: placed elsewhere only, so not here)
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, 0, RM_MON_TARGETS), yes, &s1, NULL);
    CHECK(!s1.outside && s1.byTarget[RmTargetIndex(2, 0)] && s1.byTarget[RmTargetIndex(8, 0)] && s1.byTarget[RmTargetIndex(9, 0)] &&
          !s1.byTarget[RmTargetIndex(0x0A, 0)], "This World with nothing resident: this world's own types only");
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, RM_MON_TARGETS, RM_MON_TARGETS), yes, &s1, NULL);
    for (t = 0; t < 9; ++t)  // v3.56 RE356: the nine monster targets (the victims' ghosts: TestPlans' RE356 part)
        CHECK(s1.byTarget[t] >= 3, "all targets resident and allowed: %s x%d (a mix)", kRmTargets[t].name, s1.byTarget[t]);
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, RM_MON_TARGETS, RM_MON_TARGETS & ~RM_GUM_TARGETS), yes, &s1, NULL);
    CHECK(!s1.gum && !s1.byTarget[2] && !s1.byTarget[3], "Gum Heads out without a guard");
    PlanAll(sc, In(RMM_WORLD, 99, 4, 1, 0, 0, 0, 0), yes, &s1, NULL);
    CHECK(!s1.changed, "an empty pool changes nothing");
    {   // the distribution over many seeds: each of 9 targets near 1/9
        int cnt[RM_TARGET_N], seed, total = 0, ok = 1;
        memset(cnt, 0, sizeof cnt);
        for (seed = 1; seed <= 200; ++seed) {
            RmPlanIn in = In(RMM_WORLD, (unsigned)seed * 7919u, 4, 1, 0, 0, RM_MON_TARGETS, RM_MON_TARGETS);
            in.freeBones = 1 << 24;  // (no bone limit: the roll alone)
            PlanAll(sc, in, yes, &s1, NULL);
            for (t = 0; t < RM_TARGET_N; ++t) cnt[t] += s1.byTarget[t], total += s1.byTarget[t];
        }
        for (t = 0; t < 9; ++t) ok &= cnt[t] * 9 > total * 8 / 10 && cnt[t] * 9 < total * 12 / 10;  // v3.56 RE356: the monsters
        CHECK(ok && total == 200 * 71, "the mix is even over 200 seeds (each target within 20 %% of 1/9)");
    }
    {   // v3.56 RE356: Same World and All Worlds with the victims' ghosts (the story rule applied as rzmon.inc does)
        int seed, ghosts = 0, mon = 0, g0B = 0, g0D1 = 0, outside = 0, total = 0, room, okSave = 1;
        for (seed = 1; seed <= 200; ++seed) {
            RmPlanIn in = In(RMM_ALL, (unsigned)seed * 104729u, 4, 1, 0, 0, RM_ALL_TARGETS, RM_ALL_TARGETS & RmStoryAllowed(4, 0, 1));
            in.freeBones = 1 << 24;
            PlanAll(sc, in, yes, &s1, NULL);
            for (t = 0; t < RM_TARGET_N; ++t) {
                total += s1.byTarget[t];
                if (RM_GHOST_TARGETS >> t & 1u) ghosts += s1.byTarget[t];
                else mon += s1.byTarget[t];
                if (RM_G0B_TARGETS >> t & 1u) g0B += s1.byTarget[t];
            }
            in.allowed = RM_ALL_TARGETS & RmStoryAllowed(4, 1, 0);
            PlanAll(sc, in, yes, &s2, NULL);
            g0D1 += s2.byTarget[RmTargetIndex(0x0D, 1)];
        }
        CHECK(total == 200 * 71 && !g0B && ghosts * 100 > total * 8 && mon > ghosts, "v3.56 RE356: All Worlds in the Water Prison before "
              "flag 2D3: no 0B; the other victims' ghosts in the mix (%d of %d, monsters %d)", ghosts, total, mon);
        CHECK(!g0D1, "v3.56 RE356: no old man's ghost (0D v1) before flag 23B (%d)", g0D1);
        // Same World: this world's victims only (the Subway places 0B v0 / v3 and 0C v1; the Water Prison none)
        {
            const unsigned w2 = g_sc[2][0].placed | g_sc[2][1].placed, w4 = g_sc[4][0].placed | g_sc[4][1].placed;
            const unsigned w3 = g_sc[3][0].placed | g_sc[3][1].placed, w6 = g_sc[6][0].placed;
            CHECK((w2 & RM_GHOST_TARGETS) == (1u << RmTargetIndex(0x0B, 0) | 1u << RmTargetIndex(0x0B, 3) | 1u << RmTargetIndex(0x0C, 1)) &&
                  !(w4 & RM_GHOST_TARGETS) && (w3 & RM_GHOST_TARGETS) == (1u << RmTargetIndex(0x0B, 0) | 1u << RmTargetIndex(0x0B, 2) |
                  1u << RmTargetIndex(0x0B, 3) | 1u << RmTargetIndex(0x0D, 1)) && (w6 & RM_GHOST_TARGETS) == (1u << RmTargetIndex(0x0B, 0) |
                  1u << RmTargetIndex(0x0C, 1) | 1u << RmTargetIndex(0x0D, 1)), "v3.56 RE356: Same World's victims: Subway %04X, Forest "
                  "%04X, Water Prison %04X, Apartment %04X (the stock scripts)", w2 & RM_GHOST_TARGETS, w3 & RM_GHOST_TARGETS,
                  w4 & RM_GHOST_TARGETS, w6 & RM_GHOST_TARGETS);
        }
        for (seed = 1, ghosts = 0, outside = 0; seed <= 100; ++seed) {
            RmPlanIn in = In(RMM_WORLD, (unsigned)seed * 31u, 2, 0, 0, 0, 0, RM_ALL_TARGETS & RmStoryAllowed(2, 0, 1));
            in.released = 1, in.freeBones = 1 << 24;
            PlanAll(&g_sc[2][0], in, yes, &s1, NULL);
            for (t = 0; t < RM_TARGET_N; ++t) {
                if (RM_GHOST_TARGETS >> t & 1u) ghosts += s1.byTarget[t];
                if ((RM_GHOST_TARGETS >> t & 1u) && !(in.world >> t & 1u)) outside += s1.byTarget[t];
            }
        }
        CHECK(ghosts > 100 && !outside, "v3.56 RE356: Same World in the Subway: its own victims' ghosts only (%d, %d of another world)",
              ghosts, outside);
        // a monster rolled a ghost is retyped like any target: its record is Refined's and stays out of a save
        for (room = 1; room < RZ_ROOMS && okSave; ++room) {
            static unsigned char recs[RM_REC_N * RM_REC_SIZE];
            static RmProv pv;
            int idx[RM_ROOM_MAX], m = RoomIdx(sc, room, idx), k2;
            RmRec rec[RM_ROOM_MAX];
            RmPick pk[RM_ROOM_MAX];
            RmPlanStat ps;
            RmPlanIn in = In(RMM_ALL, 77, 4, 1, room, 0, RM_ALL_TARGETS, RM_GHOST_TARGETS);  // (ghosts only: every placement a ghost)
            if (!m) continue;
            in.freeBones = 1 << 24;
            memset(recs, 0, sizeof recs), memset(&pv, 0, sizeof pv);
            NoRecs(rec, m);
            RmPlanRoom(&in, sc, idx, m, rec, yes, pk, &ps);
            for (k2 = 0; k2 < m && k2 < RM_REC_N; ++k2) {
                unsigned char *r = recs + RM_REC_SIZE * k2;
                const unsigned id = EnmScriptId(4, 1, room, sc->s[idx[k2]].n);
                const short ty = (short)pk[k2].type, stt = 1;
                okSave &= RmIsGhostTarget(pk[k2].type);
                RmProvPut(&pv, id, pk[k2].type, pk[k2].variant, sc->s[idx[k2]].type, sc->s[idx[k2]].variant);
                memcpy(r + 0x20, &id, 4), memcpy(r + 0x24, &ty, 2), memcpy(r + 0x26, &stt, 2);
                r[0x33] = pk[k2].variant, r[0x36] = 4, r[0x37] = (unsigned char)room;
            }
            okSave &= RmSaveClean(recs, &pv) == (m < RM_REC_N ? m : RM_REC_N);
        }
        CHECK(okSave, "v3.56 RE356: monsters rolled victims' ghosts: their records are Refined's, every one left out of a save");
    }
    // Hummers: a walker only where a floor was found
    {
        const RmScan *h = &g_sc[5][1];  // Building 2nd half: 14 Hummer sources
        PlanAll(h, In(RMM_WORLD, 5, 5, 1, 0, 0, RM_MON_TARGETS, RM_MON_TARGETS), no, &s1, NULL);
        CHECK(!s1.flyerWalker, "no floor: a Hummer's spot never gets a walker");
        PlanAll(h, In(RMM_WORLD, 5, 5, 1, 0, 0, RM_MON_TARGETS, RM_MON_TARGETS), yes, &s2, NULL);
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
            RmPlanIn in = In(RMM_WORLD, 3, 4, 1, room, 0, RM_MON_TARGETS, RM_MON_TARGETS);
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
        in = In(RMM_ROOM, 77, 4, 1, room, 0, stockT, RM_MON_TARGETS);
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
        if (p[k].why == RMW_EVENT) continue;  // v3.51 SN351: as rzmon.inc (an event dog left asleep stays the game's own)
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
        RmPlanIn in = In(RMM_WORLD, 794655925u, 4, 1, room, 0, 0, RM_MON_TARGETS);
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
            RmPlanIn in = In(RMM_WORLD, 5, 4, 1, room, 0, 0, RM_MON_TARGETS);
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
            RmPlanIn in = In(RMM_WORLD, 11, 5, 1, room, 0, RM_MON_TARGETS, RM_MON_TARGETS);
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
        RmPlanIn in = In(RMM_WORLD, 794655925u, 5, 1, 11, 1, 1u << RmTargetIndex(2, 0), RM_MON_TARGETS);
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
    RmPlanIn in = In(RMM_ALL, 1, 4, 1, 0, 0, 0, RM_MON_TARGETS);
    memset(yes, 1, sizeof yes), memset(by, 0, sizeof by);
    CHECK(RmPool(&in) == RM_MON_TARGETS, "v3.50 RM350 All Worlds: every target, nothing resident (%03X)", RmPool(&in));
    in.allowed = RM_MON_TARGETS & ~RM_GUM_TARGETS;
    CHECK(RmPool(&in) == (RM_MON_TARGETS & ~RM_GUM_TARGETS), "v3.50 RM350 All Worlds: Gum Heads only with the guard");
    in.mode = RMM_WORLD, in.allowed = RM_MON_TARGETS;
    CHECK(RmPool(&in) == in.world && in.world != RM_MON_TARGETS, "This World stays this world's (%03X)", RmPool(&in));
    in.mode = RMM_ROOM, in.resident = 1u << RmTargetIndex(2, 0);
    CHECK(RmPool(&in) == in.resident, "This Room stays the resident ones");
    for (stage = 2; stage <= 8; ++stage)
        for (h = 0; h < 1 + g_has1[stage]; ++h) {
            const RmScan *sc = &g_sc[stage][h];
            const unsigned world = g_sc[stage][0].placed | (g_has1[stage] ? g_sc[stage][1].placed : 0u);
            Sum sw;
            if (!sc->n) continue;
            in = In(RMM_ALL, 777, stage, h, 0, 0, 0, RM_MON_TARGETS);
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
            in.mode = RMM_ALL, in.allowed = RM_MON_TARGETS & ~RM_GUM_TARGETS;
            PlanAll(sc, in, yes, &sw, NULL);
            CHECK(!sw.gum, "All Worlds stage %d half %d: no Gum Head without the guard", stage, h);
        }
    for (t = 0; t < 9; ++t) CHECK(by[t] > 5, "All Worlds: %s placed (%d)", kRmTargets[t].name, by[t]);  // v3.56 RE356: the monsters
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
            RoomLoad(recs, &pv, sc, stage, half, room, In(RMM_WORLD, (unsigned)seed, stage, half, room, 0, 0, RM_MON_TARGETS), p1, &st);
            ch = st.changed;
            found = ch > 0 && ch < m;
        }
    --seed, --room;  // (the loops stepped past the one found)
    if (!found) {
        CHECK(0, "save round trip: no room / seed with changed and kept placements");
        return;
    }
    in = In(RMM_WORLD, (unsigned)seed, stage, half, room, 0, 0, RM_MON_TARGETS);
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
        RmPlanIn in = In(RMM_WORLD, 4242, stage, half, room, 0, RM_ALL_TARGETS, RM_ALL_TARGETS & RmStoryAllowed(stage, 1, 1));  // v3.56 RE356: + ghosts
        if (!m) continue;
        in.freeBones = 1 << 24;
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
        for (k = 0; k < m; ++k) {  // v3.56 RE356: the runner's view: the type word and the mode word's variant (a ghost is no source)
            const RmSlot *s = &sc->s[idx[k]];
            ++total;
            if (*ImgPtr(s->typeVa) == p[k].type && (*ImgPtr(s->modeVa) >> 8 & 0xFu) == p[k].variant) ++match;
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
    Enemy(0x0A, 0x0006);                                       // n 1: an event dog (mode 6): v3.51 SN351 a source (event)
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
    Enemy(0x0A, 0x0003);                                       // n 14: v3.51 SN351: a dog in mode 3 alone: no source
    Enemy(0x0A, 0x0027);                                       // n 15: v3.51 SN351: an event dog on a wall: no source
    Enemy(0x07, 0x0006);                                       // n 16: v3.51 SN351: a Gum Head in mode 6: no source
    Put(1, 1, 4), Put(0x0F, 1, 9);                             // n 17: a pickup
    Put(0, 1, 6);                                              // room 6
    Put(1, 1, 3);                                              // n 0: an NPC
    Enemy(0x02, 0);                                            // n 1: a Whitestool (the end of the room flushes the last)
    g_buf[g_bn++] = 0xF0F0F0F0u;
    w = BufWin();
    CHECK(RmScanScript(&w, 0x10000u, 4, &sc) && sc.rooms == 2 && sc.kind2 == 18, "made up: scanned (rooms %d, kind2 %d)", sc.rooms,
          sc.kind2);
    CHECK(sc.n == 4 && sc.s[0].room == 5 && sc.s[0].n == 0 && sc.s[0].type == 0x0A && !sc.s[0].event && sc.s[1].n == 1 &&
          sc.s[1].type == 0x0A && sc.s[1].mw == 6 && sc.s[1].event && sc.s[2].n == 4 && sc.s[2].type == 9 && sc.s[2].mw == 0x3110 &&
          !sc.s[2].event && sc.s[3].room == 6 && sc.s[3].n == 1 && sc.s[3].type == 2 && sc.events == 1, "made up: the sources (%d): "
          "n 0 dog, n 1 event dog (v3.51 SN351), n 4 Tremer, room 6 n 1 Whitestool; mode 3 alone, a dog on a wall, a Gum Head in "
          "mode 6: no source", sc.n);
    CHECK(sc.s[0].typeVa == 0x10000u + 4 * 5 && sc.s[0].modeVa == sc.s[0].typeVa + 4 * 6 && sc.s[0].yVa == sc.s[0].typeVa + 4 * 3,
          "made up: the words of n 0");
    CHECK(sc.roomBoss[5] && !sc.roomBoss[6] && sc.roomN[5] == 3 && sc.roomN[6] == 1, "made up: room 5 has a boss");
    CHECK(sc.roomCost[6] == RmPlaceCost(2) && sc.roomCost[5] > 10 * RmPlaceCost(0x0A), "made up: room costs");
    CHECK((sc.placed >> RmTargetIndex(0x0A, 0) & 1u) && !(sc.placed >> RmTargetIndex(9, 0) & 1u) && !(sc.placed >> RmTargetIndex(8, 1) & 1u) &&
          (sc.types >> 0x10 & 1u) && (sc.types >> 6 & 1u), "made up: the placed masks");
    CHECK(RmScanScript(&w, 0x10000u, 1, &sc) && sc.n == 0 && sc.kind2 == 18 && !sc.events, "made up: stage 1 (Room 302): no source");
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

// ---------------------------------------------------------------- v3.51 SN351: the event dogs, the second roll
static void TestEventDogs(void)
{
    static RmPatchStack ps;
    const RmScan *sc = &g_sc[2][0];  // Subway 1st half
    int idx[RM_ROOM_MAX], m = RoomIdx(sc, 2, idx), k, seed, changed = 0, plans = 0, okAsleep = 1, okSame = 1, byType[0x20];
    RmRec rec[RM_ROOM_MAX];
    RmPick p[RM_ROOM_MAX], q[RM_ROOM_MAX];
    RmPlanStat st;
    unsigned char yes[RM_ROOM_MAX];
    memset(yes, 1, sizeof yes), memset(byType, 0, sizeof byType);
    CHECK(m == 3 && sc->s[idx[0]].event && sc->s[idx[1]].event && sc->s[idx[2]].event, "Subway room 2:2: 3 event dog sources (%d)", m);
    if (m != 3) return;
    for (seed = 1; seed <= 300; ++seed) {
        RmPlanIn in = In(RMM_ALL, (unsigned)seed * 2654435761u, 2, 0, 2, 0, RM_MON_TARGETS, RM_MON_TARGETS);
        NoRecs(rec, m);
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);  // story flag 0x130 clear
        for (k = 0; k < m; ++k) okAsleep &= p[k].why == RMW_EVENT && p[k].type == 0x0A && !p[k].variant && !p[k].drop && !p[k].adopt;
        okAsleep &= st.event == 3 && !st.changed && !st.eventPlanned;
        in.released = 1;  // the flag set: their cut-scene is over
        RmPlanRoom(&in, sc, idx, m, rec, yes, q, &st);
        okSame &= st.eventPlanned == 3 && !st.event;
        for (k = 0; k < m; ++k) {
            ++plans;
            changed += q[k].type != 0x0A || q[k].variant;
            ++byType[q[k].type & 0x1F];
            okSame &= q[k].why != RMW_EVENT;
        }
    }
    CHECK(okAsleep, "v3.51 SN351: story flag 130 clear: the event dogs stay as placed (asleep for their cut-scene), nothing deleted");
    CHECK(okSame && changed * 10 > plans * 7 && byType[2] && byType[3] && byType[7] && byType[8] && byType[9], "v3.51 SN351: flag "
          "130 set: the event dogs roll as the others (%d of %d changed; every type)", changed, plans);
    {   // records: the flag clear keeps the game's own stock record (not adopted) and deletes Refined's of another type here;
        // the flag set adopts the game's own ones (deleted when the roll gives another type)
        RmPlanIn in = In(RMM_ALL, 5, 2, 0, 2, 3, RM_MON_TARGETS, RM_MON_TARGETS);
        in.reroll = 1;
        for (k = 0; k < m; ++k) rec[k].type = 0x0A, rec[k].variant = 0, rec[k].here = 1, rec[k].ours = 0;
        rec[1].type = 8, rec[1].ours = 1;
        RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
        CHECK(p[0].why == RMW_EVENT && p[1].why == RMW_EVENT && p[2].why == RMW_EVENT && !p[0].drop && p[1].drop && !p[2].drop &&
              !p[0].adopt && !st.adopted && st.drops == 1 && !st.changed, "v3.51 SN351: flag clear: the game's own record kept, "
              "Refined's Twin Victim record here deleted (the dog comes back as placed)");
        rec[1].type = 0x0A, rec[1].ours = 0;
        in.released = 1;
        RmPlanRoom(&in, sc, idx, m, rec, yes, q, &st);
        for (k = 0, okSame = 1; k < m; ++k)
            okSame &= q[k].adopt && q[k].drop == (q[k].type != 0x0A || q[k].variant != 0);
        CHECK(okSame && st.adopted == 3, "v3.51 SN351: flag set: the game's own records of the dogs adopted, deleted for another type");
    }
    {   // the planned words in a copy of the stock script: the retyped dogs are no event dogs any more; the restore is stock
        RzWin w = ExeWin();
        static RmScan after;
        RmPlanIn in = In(RMM_ALL, 0, 2, 0, 2, 0, RM_MON_TARGETS, RM_MON_TARGETS);
        int d, ok = 0, okRe = 1, tries;
        in.released = 1;
        for (tries = 0, seed = 1; tries < 1 && seed < 2000; ++seed) {  // a plan with a dog kept (any variant) and a dog retyped
            int dog = 0, other = 0;
            in.seed = (unsigned)seed;
            NoRecs(rec, m);
            RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
            for (k = 0; k < m; ++k) dog += p[k].type == 0x0A, other += p[k].type != 0x0A;
            if (dog && other) ++tries;
        }
        memset(&ps, 0, sizeof ps);
        d = RmStackPush(&ps);
        for (k = 0; k < m; ++k) {
            const RmSlot *s = &sc->s[idx[k]];
            const unsigned mw = RmModeWord(s->type, s->mw, p[k].type, p[k].variant);
            if (p[k].type == s->type && p[k].variant == s->variant) continue;
            if (mw != s->mw) RmStackAdd(&ps, d, ImgPtr(s->modeVa), s->mw, mw);
            RmStackAdd(&ps, d, ImgPtr(s->typeVa), s->type, p[k].type);
        }
        RmScanScript(&w, Script(2, 0), 2, &after);
        for (k = 0; k < m; ++k) {
            const int j = RmSlotOf(&after, 2, sc->s[idx[k]].n);
            ok += j >= 0 && after.s[j].type == p[k].type && after.s[j].variant == p[k].variant;
            okRe &= j >= 0 && after.s[j].event == (p[k].type == 0x0A);
        }
        CHECK(tries && ok == 3 && okRe, "v3.51 SN351: room 2:2 patched (seed %d): the planned types, a retyped dog in spawn mode 0 "
              "(no event dog), a dog kept keeps its mode 6 / 7", seed - 1);
        CHECK(RmStackPop(&ps, d) == 0 && ps.depth == 0, "v3.51 SN351: room 2:2's words restored");
        RmScanScript(&w, Script(2, 0), 2, &after);
        CHECK(RmStockMatch(2, 0, Script(2, 0), &after) == 1 && after.events == 3, "v3.51 SN351: the restored Subway script is stock");
    }
    {   // a new game, Change Each Visit Off: the first visit before the cut-scene leaves the dogs asleep (no provenance), they
        // write the game's own records when the room is left; the next visit (the flag set) adopts and rolls them as fresh
        static unsigned char recs[RM_REC_N * RM_REC_SIZE];
        static RmProv pv;
        RmPick a[RM_ROOM_MAX], b[RM_ROOM_MAX], f[RM_ROOM_MAX];
        RmPlanStat sa, sb;
        int okSeq = 1, n = 0;
        for (seed = 1; seed <= 40; ++seed) {
            RmPlanIn in = In(RMM_ALL, (unsigned)seed * 977u, 2, 0, 2, 0, RM_MON_TARGETS, RM_MON_TARGETS);
            memset(recs, 0, sizeof recs), memset(&pv, 0, sizeof pv);
            RoomLoad(recs, &pv, sc, 2, 0, 2, in, a, &sa);
            okSeq &= sa.event == 3 && pv.n == 0;
            RoomLeave(recs, sc, 2, 0, 2, a);
            in.released = 1;
            NoRecs(rec, m);
            RmPlanRoom(&in, sc, idx, m, rec, yes, f, &st);  // a fresh plan (no records)
            RoomLoad(recs, &pv, sc, 2, 0, 2, in, b, &sb);
            for (k = 0; k < m; ++k) okSeq &= b[k].type == f[k].type && b[k].variant == f[k].variant && b[k].adopt;
            okSeq &= sb.adopted == 3 && pv.n == 3;
            n += sb.changed;
        }
        CHECK(okSeq && n > 60, "v3.51 SN351: Change Each Visit Off: asleep first (no provenance), then adopted and rolled as fresh (%d of "
              "120 changed)", n);
    }
    {   // what the user will see: the seed of the v3.50 log, All Worlds, Change Each Visit On, the flag set (its save)
        int vis;
        printf("  v3.51 SN351: the user's Subway room 2:2 (seed 1398963150, All Worlds, Change Each Visit On, roll again On):");
        for (vis = 1; vis <= 4; ++vis) {
            RmPlanIn in = In(RMM_ALL, 1398963150u, 2, 0, 2, (unsigned)vis, RM_MON_TARGETS, RM_MON_TARGETS);
            in.released = 1, in.reroll = 1, in.rollAgain = 1;
            NoRecs(rec, m);
            RmPlanRoom(&in, sc, idx, m, rec, yes, p, &st);
            printf(" #%d", vis);
            for (k = 0; k < m; ++k) printf(" %s v%d", RmTypeName(p[k].type), p[k].variant);
            printf(";");
        }
        printf("\n");
    }
}
static void TestRollAgain(void)
{
    // the user's v3.50 log (21:07 - 21:33; seed 1398963150, All Worlds, Change Each Visit On, Gum Heads in): every roll of
    // Subway rooms 2:4 and 2:5, by visit (#); three kept their own type (2:4 #12, 2:5 n3 #2 and #4: a dog v1)
    static const struct { int room, n; unsigned visit; unsigned char type, variant; } kLog[] = {
        { 4, 1, 2, 7, 1 }, { 4, 1, 3, 7, 1 }, { 4, 1, 4, 7, 0 }, { 4, 1, 5, 7, 1 }, { 4, 1, 6, 8, 1 }, { 4, 1, 7, 2, 0 },
        { 4, 1, 8, 8, 0 }, { 4, 1, 9, 9, 0 }, { 4, 1, 10, 9, 0 }, { 4, 1, 11, 7, 1 }, { 4, 1, 12, 0x0A, 0 }, { 4, 1, 13, 2, 0 },
        { 4, 1, 14, 7, 0 }, { 4, 1, 15, 9, 0 }, { 4, 1, 16, 7, 0 }, { 5, 2, 1, 8, 1 }, { 5, 3, 1, 9, 0 }, { 5, 2, 2, 3, 0 },
        { 5, 3, 2, 0x0A, 0 }, { 5, 2, 3, 8, 0 }, { 5, 3, 3, 2, 0 }, { 5, 2, 4, 7, 0 }, { 5, 3, 4, 0x0A, 1 } };
    const int nLog = (int)(sizeof kLog / sizeof kLog[0]);
    const RmScan *sc = &g_sc[2][0];
    unsigned char yes[RM_ROOM_MAX];
    int i, k, match = 0, again = 0, same = 0, own = 0, t, other = 0;
    memset(yes, 1, sizeof yes);
    for (i = 0; i < nLog; ++i) {
        int idx[RM_ROOM_MAX], m = RoomIdx(sc, kLog[i].room, idx), j = -1;
        RmRec rec[RM_ROOM_MAX];
        RmPick a[RM_ROOM_MAX], b[RM_ROOM_MAX];
        RmPlanStat sa, sb;
        RmPlanIn in = In(RMM_ALL, 1398963150u, 2, 0, kLog[i].room, kLog[i].visit, 0, RM_MON_TARGETS);
        in.reroll = 1;
        NoRecs(rec, m);
        RmPlanRoom(&in, sc, idx, m, rec, yes, a, &sa);
        in.rollAgain = 1;
        RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
        for (k = 0; k < m; ++k)
            if (sc->s[idx[k]].n == kLog[i].n) j = k;
        if (j < 0) continue;
        match += a[j].type == kLog[i].type && a[j].variant == kLog[i].variant && !a[j].again;
        if (kLog[i].type == 0x0A) {
            ++own;
            again += b[j].again && sb.again >= 1;
            other += b[j].type != 0x0A;
            printf("  v3.51 SN351: the log's room 2:%d n%d #%u: %s v%d kept; rolled again: %s v%d\n", kLog[i].room, kLog[i].n,
                   kLog[i].visit, RmTypeName(a[j].type), a[j].variant, RmTypeName(b[j].type), b[j].variant);
        } else {
            same += b[j].type == a[j].type && b[j].variant == a[j].variant && !b[j].again;
        }
    }
    CHECK(match == nLog, "v3.51 SN351: the sim replays the user's v3.50 log: %d of %d rolls of Subway 2:4 / 2:5 as logged", match, nLog);
    CHECK(own == 3 && again == 3 && other >= 2 && same == nLog - 3, "v3.51 SN351: roll again: the %d logged dogs that stayed dogs "
          "roll once more (%d), %d of them another type; the %d other rolls unchanged", own, again, other, same);
    {   // Water Prison 2nd half, All Worlds, 200 seeds: the own type from about 1 in 9 to 1 in 81, never excluded
        const RmScan *w4 = &g_sc[4][1];
        int seed, ownOff = 0, ownOn = 0, total = 0, room, okAdopt = 1;
        for (seed = 1; seed <= 200; ++seed)
            for (room = 1; room < RZ_ROOMS; ++room) {
                int idx[RM_ROOM_MAX], m = RoomIdx(w4, room, idx);
                RmRec rec[RM_ROOM_MAX];
                RmPick a[RM_ROOM_MAX], b[RM_ROOM_MAX];
                RmPlanStat sa, sb;
                RmPlanIn in = In(RMM_ALL, (unsigned)seed * 7919u, 4, 1, room, 0, RM_MON_TARGETS, RM_MON_TARGETS);
                if (!m) continue;
                in.freeBones = 1 << 24;
                NoRecs(rec, m);
                RmPlanRoom(&in, w4, idx, m, rec, yes, a, &sa);
                in.rollAgain = 1;
                RmPlanRoom(&in, w4, idx, m, rec, yes, b, &sb);
                for (k = 0; k < m; ++k) {
                    ++total;
                    ownOff += a[k].type == w4->s[idx[k]].type;
                    ownOn += b[k].type == w4->s[idx[k]].type;
                }
                if (seed <= 20) {  // the game's own stock records adopted: the same as a fresh game with the second roll too
                    RmPick c[RM_ROOM_MAX];
                    RmPlanStat sc2;
                    for (k = 0; k < m; ++k) rec[k].type = w4->s[idx[k]].type, rec[k].variant = w4->s[idx[k]].variant, rec[k].here = 1;
                    RmPlanRoom(&in, w4, idx, m, rec, yes, c, &sc2);
                    for (k = 0; k < m; ++k) okAdopt &= c[k].type == b[k].type && c[k].variant == b[k].variant && c[k].adopt;
                }
            }
        CHECK(total == 200 * 71 && ownOff * 100 > total * 8 && ownOff * 100 < total * 16 && ownOn > 0 && ownOn * 100 < total * 3,
              "v3.51 SN351: roll again (All Worlds, 71 Water Prison sources x 200 seeds): their own type %d -> %d of %d", ownOff, ownOn,
              total);
        CHECK(okAdopt, "v3.51 SN351: roll again: adopted records roll as a fresh game");
    }
    {   // This Room with only the stock type resident: no other type fits, the plan is v3.50's
        const RmScan *w4 = &g_sc[4][1];
        Sum s1;
        int diff;
        RmPlanIn in = In(RMM_ROOM, 1234, 4, 1, 0, 0, 1u << RmTargetIndex(2, 0), RM_MON_TARGETS);
        PlanAll(w4, in, yes, &s1, g_pa);
        in.rollAgain = 1;
        PlanAll(w4, in, yes, &s1, g_pb);
        CHECK(SamePlans(g_pa, g_pb, w4, &diff), "v3.51 SN351: roll again: a pool of the own type alone changes nothing (%d differ)", diff);
    }
    {   // Subway 1st half, This World without the Gum Head guard (Tremer, both dogs): dogs stay possible (about 4 in 9)
        Sum s1, s2;
        int seed, dogsOff = 0, dogsOn = 0, picks = 0;
        for (seed = 1; seed <= 200; ++seed) {
            RmPlanIn in = In(RMM_WORLD, (unsigned)seed * 31u, 2, 0, 0, 0, 0, RM_MON_TARGETS & ~RM_GUM_TARGETS);
            in.released = 1, in.freeBones = 1 << 24;
            PlanAll(sc, in, yes, &s1, NULL);
            in.rollAgain = 1;
            PlanAll(sc, in, yes, &s2, NULL);
            for (t = 0; t < RM_TARGET_N; ++t)
                if (kRmTargets[t].type == 0x0A) dogsOff += s1.byTarget[t], dogsOn += s2.byTarget[t];
            picks += s2.picks;
        }
        CHECK(picks == 200 * 11 && dogsOff * 100 > picks * 58 && dogsOff * 100 < picks * 75 && dogsOn * 100 > picks * 36 &&
              dogsOn * 100 < picks * 53, "v3.51 SN351: Subway This World (no Gum Heads): dogs %d -> %d of %d (never none)", dogsOff,
              dogsOn, picks);
    }
}

// ---------------------------------------------------------------- v3.58 RZ358: the dead stay dead; the copies' draws
static void TestRz358(void)
{
    int stage, v, k, none = 0, total = 0, dup = 0, outside = 0;
    // the kill indexes of the stock sources (commands 0x10 / 0x2A): every source has one, and no two of a stage share a bit
    for (stage = 2; stage <= 8; ++stage) {
        static unsigned char used[2][1024];
        memset(used, 0, sizeof used);
        for (v = 0; v < 1 + g_has1[stage]; ++v) {
            const RmScan *sc = &g_sc[stage][v];
            for (k = 0; k < sc->n; ++k) {
                int w;
                unsigned m;
                ++total;
                if (sc->s[k].kill <= 0) {
                    ++none;
                    continue;
                }
                if (!EnmKillBit(stage, sc->s[k].kill, &w, &m)) {
                    ++outside;
                    continue;
                }
                if (used[v][w * 32 + RmNthBit(m, 0)]++) ++dup;
            }
        }
    }
    printf("  v3.58 RZ358: %d source placement(s): %d without a kill index, %d outside the kill bits, %d sharing a bit in a half\n",
           total, none, outside, dup);
    CHECK(total == 238 && none == 0 && outside == 0, "every stock source has a kill index inside the kill bits (%d none, %d outside)",
          none, outside);
    CHECK(kRmTargets[6].type == 9 && kRmTargets[6].variant == 0 && RM_TREMER_TARGET == 1u << 6 && !(RM_COPY_TARGETS & RM_TREMER_TARGET) &&
          RmBitCount(RM_COPY_TARGETS) == RM_TARGET_N - 1, "the copies' targets: every target but the Tremer");
    {   // the plan's dead rules over a real room (Water Prison 2nd half room with the most sources)
        const RmScan *sc = &g_sc[4][1];
        int room, best = 0, bestRoom = 0, idx[RM_ROOM_MAX], m, j, again;
        RmRec rec[RM_ROOM_MAX];
        RmPick a[RM_ROOM_MAX], b[RM_ROOM_MAX];
        RmPlanStat sa, sb;
        unsigned char yes[RM_ROOM_MAX];
        memset(yes, 1, sizeof yes);
        for (room = 1; room < RZ_ROOMS; ++room)
            if ((m = RoomIdx(sc, room, idx)) > best) best = m, bestRoom = room;
        m = RoomIdx(sc, bestRoom, idx);
        for (again = 0; again < 2; ++again) {  // reroll On (By Room) and Off (By World)
            RmPlanIn in = In(RMM_ALL, 1398963150u, 4, 1, bestRoom, 7, RM_ALL_TARGETS, RM_ALL_TARGETS);
            in.reroll = !again;
            for (k = 0; k < m; ++k) {  // Refined made each one as another type, and each lies dead here
                const RmSlot *s = &sc->s[idx[k]];
                rec[k].type = s->type == 8 ? 0x0A : 8, rec[k].variant = 0, rec[k].here = 1, rec[k].ours = 1;
            }
            in.dead = (1u << m) - 1u;
            RmPlanRoom(&in, sc, idx, m, rec, yes, a, &sa);
            for (k = 0, j = 0; k < m; ++k) j += a[k].type == rec[k].type && a[k].variant == rec[k].variant && a[k].why == RMW_DEAD && !a[k].drop;
            CHECK(j == m && sa.dead == m && sa.drops == 0, "reroll %s: every body keeps the type it died as, no record deleted (%d of %d)",
                  in.reroll ? "By Room" : "By World", j, m);
            for (j = 1; j < 40; ++j) {  // whatever the visit: the same
                in.visit = (unsigned)j * 7919u;
                RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
                if (memcmp(a, b, sizeof a[0] * (size_t)m)) break;
            }
            CHECK(j == 40, "a body is never rolled again, whatever the visit salt");
            in.dead = 0;  // the same records standing (not dead): reroll By Room rolls them over and drops the old records
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            if (in.reroll) CHECK(sb.drops > 0 && sb.dead == 0, "standing records here: rolled again, some dropped (%d)", sb.drops);
            else CHECK(sb.dead == 0 && sb.sticky > 0, "By World: standing records keep their type where they may (sticky %d of %d)", sb.sticky, m);
            in.killed = 1u;  // slot 0's kill bit: stock words, its record kept
            in.dead = 0;
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            CHECK(b[0].why == RMW_KILLED && b[0].type == sc->s[idx[0]].type && b[0].variant == sc->s[idx[0]].variant && !b[0].drop &&
                  sb.killed == 1, "a kill bit: as placed, its record kept");
            in.killed = 0;
            in.dead = 1u;  // a body of another writer (not ours, not the stock type): rule 0 (as placed, kept)
            rec[0].ours = 0;
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            CHECK(b[0].why == RMW_OTHER && !b[0].drop, "another writer's body: as placed, its record kept");
            rec[0].type = sc->s[idx[0]].type, rec[0].variant = sc->s[idx[0]].variant;  // the game's own stock body: never adopted
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            CHECK(b[0].why == RMW_DEAD && !b[0].adopt && !b[0].drop && b[0].type == sc->s[idx[0]].type, "the game's own body: not adopted, kept");
            in.dead = 0;
            RmPlanRoom(&in, sc, idx, m, rec, yes, b, &sb);
            CHECK(b[0].adopt, "(the same record standing: adopted, as v3.50)");
        }
        CHECK(RmDeadFallback(&sc->s[idx[0]]) == RMD_HIDE, "a stock source's body can always be kept with its placement empty");
        {
            RmSlot z = sc->s[idx[0]];
            z.kill = 0;
            CHECK(RmDeadFallback(&z) == RMD_LOST && RmDeadFallback(NULL) == RMD_LOST, "no kill index: lost");
        }
        {   // the copies' draws: in the pool, deterministic, salted by k, different across visits; Tremers never
            RmPlanIn in = In(RMM_ALL, 1398963150u, 4, 1, bestRoom, 3, RM_ALL_TARGETS, RM_ALL_TARGETS);
            int byT[RM_TARGET_N], c, n = 0, diffK = 0, diffV = 0, sameOrig = 0;
            memset(byT, 0, sizeof byT);
            for (k = 0; k < m; ++k)
                for (c = 1; c <= RM_COPY_K; ++c) {
                    const int t = RmCopyDraw(&in, &sc->s[idx[k]], c);
                    RmPlanIn in2 = in;
                    CHECK(t >= 0 && (RM_COPY_TARGETS >> t & 1u), "a copy's draw is a copy target");
                    if (t < 0) continue;
                    ++byT[t], ++n;
                    CHECK(RmCopyDraw(&in, &sc->s[idx[k]], c) == t, "the same draw again (deterministic)");
                    if (c > 1 && RmCopyDraw(&in, &sc->s[idx[k]], c - 1) != t) ++diffK;
                    in2.visit = 4;
                    if (RmCopyDraw(&in2, &sc->s[idx[k]], c) != t) ++diffV;
                }
            CHECK(n == m * RM_COPY_K && !byT[6], "every copy drew, no Tremer (%d)", byT[6]);
            CHECK(diffK > 0 && diffV > 0, "copies differ by k (%d) and by visit (%d)", diffK, diffV);
            (void)sameOrig;
            in.allowed = 1u << RmTargetIndex(2, 0);
            for (k = 0; k < m; ++k) CHECK(RmCopyDraw(&in, &sc->s[idx[k]], 1) == RmTargetIndex(2, 0), "a pool of one: that one");
            in.allowed = RM_TREMER_TARGET;
            CHECK(RmCopyDraw(&in, &sc->s[idx[0]], 1) < 0, "only the Tremer allowed: no draw (the copy keeps its original's type)");
            in.allowed = RM_ALL_TARGETS;
            CHECK(RmCopyDraw(&in, &sc->s[idx[0]], 0) < 0 && RmCopyDraw(&in, &sc->s[idx[0]], 4) < 0 && RmCopyDraw(&in, NULL, 1) < 0,
                  "k outside 1..3: none");
            {   // Same World: the world's pool only
                RmPlanIn w = In(RMM_WORLD, 77, 2, 0, 2, 0, 0, RM_ALL_TARGETS);
                w.world = g_sc[2][0].placed | g_sc[2][1].placed;
                for (k = 0; k < g_sc[2][0].n; ++k)
                    for (c = 1; c <= 3; ++c) {
                        const int t = RmCopyDraw(&w, &g_sc[2][0].s[k], c);
                        CHECK(t >= 0 && (w.world >> t & 1u) && t != 6, "Same World: a copy of the world's own types, never a Tremer");
                    }
            }
        }
        CHECK(RmCopiesDrawn(2, RMW_ROLL) && RmCopiesDrawn(0x0A, RMW_SAME) && RmCopiesDrawn(3, RMW_STICKY) && RmCopiesDrawn(8, RMW_BONES),
              "copies of standing originals draw");
        CHECK(!RmCopiesDrawn(2, RMW_DEAD) && !RmCopiesDrawn(2, RMW_KILLED) && !RmCopiesDrawn(0x0A, RMW_EVENT) && !RmCopiesDrawn(2, RMW_OTHER)
              && !RmCopiesDrawn(2, RMW_AWAY), "no draw for a body, a kill bit, an event dog asleep, another writer's, one elsewhere");
        CHECK(!RmCopiesDrawn(9, RMW_ROLL) && !RmCopiesDrawn(0x0B, RMW_ROLL) && !RmCopiesDrawn(0x11, RMW_ROLL),
              "an original made a Tremer or a ghost gets no copies of drawn types (Enemy Copies' own rules)");
    }
    {   // the save keeps a randomized body dead as its kill bit
        static unsigned char block[0x1B90];
        static RmProv pv;
        unsigned char *recs = block + RM_SAVE_RECS;
        const RmScan *sc = &g_sc[4][1];
        const RmSlot *a = &sc->s[0], *b = &sc->s[1], *c = &sc->s[2];
        const unsigned ida = EnmScriptId(4, 1, a->room, a->n), idb = EnmScriptId(4, 1, b->room, b->n), idc = EnmScriptId(4, 1, c->room, c->n);
        short ka = a->kill, kz = 0;
        int lost, n, w;
        unsigned msk, word;
        memset(block, 0, sizeof block), memset(&pv, 0, sizeof pv);
        Rec(recs, ida, a->type == 8 ? 2 : 8, 0x0B, 0, 4, a->room), memcpy(recs + 0x30, &ka, 2);                  // dead, retyped
        Rec(recs + RM_REC_SIZE, idb, b->type == 8 ? 2 : 8, 1, 0, 4, b->room);                                    // alive, retyped
        Rec(recs + 2 * RM_REC_SIZE, idc, 0x0B, 0x0B, 1, 4, c->room), memcpy(recs + 2 * RM_REC_SIZE + 0x30, &ka, 2);  // a pinned ghost
        Rec(recs + 3 * RM_REC_SIZE, idc + 0x40, 8, 0x0B, 0, 4, c->room), memcpy(recs + 3 * RM_REC_SIZE + 0x30, &kz, 2);  // no kill index
        CHECK(RmSaveKeepDead(block, recs, &pv, &lost) == 0 && lost == 0, "nothing of Refined's: nothing");
        RmProvPut(&pv, ida, a->type == 8 ? 2 : 8, 0, a->type, a->variant);
        RmProvPut(&pv, idb, b->type == 8 ? 2 : 8, 0, b->type, b->variant);
        RmProvPut(&pv, idc, 0x0B, 1, c->type, c->variant);
        RmProvPut(&pv, idc + 0x40, 8, 0, c->type, c->variant);
        n = RmSaveKeepDead(block, recs, &pv, &lost);
        CHECK(n == 1 && lost == 1, "one randomized body kept dead (%d), one without a kill index lost (%d)", n, lost);
        CHECK(EnmKillBit(4, a->kill, &w, &msk) && (memcpy(&word, block + 4 * w, 4), word & msk), "its kill bit is set in the save's copy");
        CHECK(RmSaveClean(recs, &pv) == 4, "then the records go as before (4)");
        {
            unsigned char zero[0x80];
            int bits = 0, i;
            memset(zero, 0, sizeof zero);
            for (i = 0; i < 0x80; ++i) bits += block[i] != 0;
            CHECK(bits == 1, "only that bit (%d bytes changed)", bits);
        }
    }
    // the copies' record guard
    CHECK(RmCopyRecordsOk(RM_CRG_OFF, 0, 0, 30), "Off: always");
    CHECK(RmCopyRecordsOk(RM_CRG_WORLD, 20, 0, 19) && !RmCopyRecordsOk(RM_CRG_WORLD, 19, 0, 19), "this world: a free slot left after the real writes");
    CHECK(RmCopyRecordsOk(RM_CRG_WORLD, 0, 40, 19) && !RmCopyRecordsOk(RM_CRG_WORLD, 0, 19, 19) && RmCopyRecordsOk(RM_CRG_WORLD, 5, 15, 19),
          "this world: or another world's record left (the log's 5:11: 64 of 64 in use, most of them stage 4's)");
    CHECK(!RmCopyRecordsOk(RM_CRG_STRICT, 0, 40, 19) && RmCopyRecordsOk(RM_CRG_STRICT, 20, 0, 19), "strict: free slots only");
    {   // a descriptor made as another type from the slot's stock words, also back to the stock type
        const RmSlot *s = &g_sc[4][1].s[0];
        unsigned char d[0x94];
        unsigned row[4] = { 1, 2, 3, 4 }, mw;
        memset(d, 0xEE, sizeof d);
        RmFixDescTo(d, s, s->type == 8 ? 2 : 8, 0, row);
        memcpy(&mw, d + 0x24, 4);
        CHECK(!memcmp(d + 4, row, 16) && mw == 0 && d[0] == 0xEE && d[0x14] == 0xEE, "another type: its row, mode word 0 (variant 0)");
        RmFixDescTo(d, s, s->type, s->variant, row);
        memcpy(&mw, d + 0x24, 4);
        CHECK(mw == s->mw, "back to the stock type: the stock mode word (%04X)", mw);
    }
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
        TestEventDogs();      // v3.51 SN351
        TestRollAgain();      // v3.51 SN351
        TestRz358();          // v3.58 RZ358
    }
    TestMadeUp();
    TestDesc();
    TestStack();
    printf("rzmon_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
