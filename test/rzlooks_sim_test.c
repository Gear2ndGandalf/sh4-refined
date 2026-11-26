// v3.49 RZ349L: native test of src\rzlooks_sim.h (test\build_rzlooks_test.cmd): the [Randomizer] EileenOutfit / Ghosts
// texts; the outfit draw (only the unlocked outfits, both of them over seeds and visits, fixed without Change Each Visit)
// and the override's rules (a draw per world visit, the set outfit back for Off or a world without her, an Extra Menu
// choice kept until the next visit, nothing to draw with one outfit); the victim-ghost scan on the stock exe (SH4Decomp
// orig\sh4.exe: 45 victim placements in 32 rooms, all may move, their position words, the story ghosts and the scene
// modes left out) and on made-up scripts (the exclusions, reads outside the window); the spot pick (the distance rules,
// the test decides, the key decides, nothing found); patch-and-restore of the position words with the Items' words.
// v3.50 RL350: every outfit (no unlock), the room draws (Change Each Visit On; at most RZO_RUN_MAX in a row alike; the
// final fight keeps its outfit), the ghosts' spots never near another ghost's placement (v3.49's 2:7 swap), 800 + when
// the room allows (short otherwise), the record rule, and on the stock exe: the victim creates run EnemyGhostReconcile
// then EnemySpawnGate (the record restore that hid v3.49's moves), the record position functions.
// Usage: out\rzlooks_sim_test.exe [path of the stock sh4.exe]
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rzlooks_sim.h"

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
static RzWin ExeWin(void)  // as randomizer.inc RzWindow
{
    RzWin w;
    w.base = g_img + (RZ_DATA_LO - 0x400000u), w.lo = RZ_DATA_LO, w.hi = RZ_SCRIPT_HI;
    return w;
}

// ---------------------------------------------------------------- texts
static void TestTexts(void)
{
    CHECK(RzoOfText("Off") == RZO_OFF && RzoOfText(" random ") == RZO_RANDOM && RzoOfText("1") == 1 && RzoOfText("0") == 0 &&
          RzoOfText("On") == 1 && RzoOfText("yes") == 1 && RzoOfText("Nurse") == -1 && RzoOfText("") == -1 && RzoOfText(NULL) == -1,
          "EileenOutfit texts");
    CHECK(RzgOfText("Off") == RZG_OFF && RzgOfText("SPOTS") == RZG_SPOTS && RzgOfText("1") == 1 && RzgOfText("no") == 0 &&
          RzgOfText("Rooms") == -1 && RzgOfText("Spot") == -1 && RzgOfText(NULL) == -1, "Ghosts texts");
    CHECK(RZO_DEFAULT == RZO_RANDOM && RZG_DEFAULT == RZG_SPOTS, "the spec's defaults: EileenOutfit Random, Ghosts Spots");
    CHECK(!strcmp(kRzoName[RzoOfText(kRzoName[1])], "Random") && !strcmp(kRzgName[RzgOfText(kRzgName[1])], "Spots") &&
          RzoOfText(kRzoName[0]) == 0 && RzgOfText(kRzgName[0]) == 0, "the names read back");
    CHECK(!strcmp(RzoOutfitName(-1), "Default") && !strcmp(RzoOutfitName(0), "Dress") && !strcmp(RzoOutfitName(1), "Nurse"),
          "outfit names (eo_outfit -1 / 0 / 1)");
}

// ---------------------------------------------------------------- the outfit draw
static void TestDraw(void)
{
    int s, v, h[2] = { 0, 0 }, changed = 0, n = 0;
    CHECK(RZO_ALL == 3u, "v3.50 RL350: every outfit, the Dress and the Nurse, no unlock asked");
    for (s = 1; s <= 2000; ++s) {
        const int a = RzoDraw((unsigned)s, 3, 1, 0, RZO_ALL), b = RzoDraw((unsigned)s, 3, 1, 0, RZO_ALL);
        CHECK(a == 0 || a == 1, "a draw is the Dress or the Nurse (%d)", a);
        CHECK(a == b, "the same inputs give the same outfit");
        CHECK(RzoDraw((unsigned)s, 3, 1, 5, 1u) == -1 && RzoDraw((unsigned)s, 3, 1, 5, 0u) == -1 && RzoDraw((unsigned)s, 3, 1, 5, 2u) == -1,
              "one outfit (or none): nothing to draw");
        CHECK(RzoDrawRoom((unsigned)s, 3, 1, 2, 4, 7, 1u, 0, 0) == -1 && RzoDrawRoom((unsigned)s, 3, 1, 2, 4, 7, 2u, 1, 5) == -1,
              "a room draw with one outfit: nothing");
        CHECK(RzoDrawRoom((unsigned)s, 3, 1, 2, 4, 7, RZO_ALL, 1, RZO_RUN_MAX) == 0 &&
              RzoDrawRoom((unsigned)s, 3, 1, 2, 4, 7, RZO_ALL, 0, RZO_RUN_MAX + 3) == 1, "a run of RZO_RUN_MAX: the other outfit");
        if (a >= 0) ++h[a];
        for (v = 1; v <= 8; ++v) {
            changed += RzoDraw((unsigned)s, 3, 1, (unsigned)v, RZO_ALL) != RzoDraw((unsigned)s, 3, 1, (unsigned)(v - 1), RZO_ALL);
            ++n;
        }
    }
    CHECK(h[0] > 900 && h[1] > 900, "both outfits come out over seeds (Dress %d, Nurse %d of 2000)", h[0], h[1]);
    CHECK(changed > n / 3 && changed < 2 * n / 3, "the next visit draws anew (%d of %d visits changed)", changed, n);
    {
        int st, diff = 0;
        for (st = 2; st <= 11; ++st) diff += RzoDraw(99, st, 1, 0, RZO_ALL) != RzoDraw(99, 2, 1, 0, RZO_ALL);
        CHECK(diff >= 2, "the stage takes part in the draw (%d of 9 differ from stage 2)", diff);
    }
    {   // the room draw: fair below the run cap, the seed / room / number take part
        int r1 = 0, r0 = 0, dRoom = 0, dSeq = 0, dSeed = 0;
        for (s = 1; s <= 2000; ++s) {
            const int a = RzoDrawRoom((unsigned)s, 5, 1, 3, 4, 7, RZO_ALL, 0, 1);
            r1 += a == 1, r0 += a == 0;
            dRoom += a != RzoDrawRoom((unsigned)s, 5, 1, 3, 4, 8, RZO_ALL, 0, 1);
            dSeq += a != RzoDrawRoom((unsigned)s, 5, 1, 3, 5, 7, RZO_ALL, 0, 1);
            dSeed += a != RzoDrawRoom((unsigned)s + 7777u, 5, 1, 3, 4, 7, RZO_ALL, 0, 1);
        }
        CHECK(r0 > 850 && r1 > 850 && r0 + r1 == 2000, "room draws: both outfits (Dress %d, Nurse %d of 2000)", r0, r1);
        CHECK(dRoom > 700 && dSeq > 700 && dSeed > 700, "the room, the room draw's number and the seed take part (%d %d %d)", dRoom,
              dSeq, dSeed);
    }
}

// ---------------------------------------------------------------- the override's rules
// atWorld 1 = her world's file list (eo_stage), 0 = a room load (eo_room)
static int Step(RzoState *s, int mode, int stage, int here, unsigned unlocked, int *cur, int saved, int perVisit, int atWorld)
{
    int put, ev = RzoStep(s, mode, 1234u, perVisit, stage, 1, here, unlocked, *cur, saved, atWorld, 3, &put);
    if (put >= -1) *cur = put;
    return ev;
}
static void TestOverride(void)
{
    RzoState s;
    int cur = -1, saved = -1, ev, first, k, run, last, maxRun, changes;
    // a world visit: the draw at her world's file list; its first room load keeps it; every later room load draws again
    RzoInit(&s);
    ev = Step(&s, RZO_RANDOM, 7, 1, RZO_ALL, &cur, saved, 1, 1);
    CHECK(ev == RZO_EV_DRAW && (cur == 0 || cur == 1) && s.active && s.visit == 1, "a new Eileen world: drawn (%d -> %d)", ev, cur);
    first = cur;
    CHECK(Step(&s, RZO_RANDOM, 7, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == first,
          "the room load right after the world's list keeps that draw");
    last = cur, run = 1, maxRun = 1, changes = 0;
    for (k = 0; k < 40; ++k) {
        ev = Step(&s, RZO_RANDOM, 7, 1, RZO_ALL, &cur, saved, 1, 0);
        CHECK(ev == RZO_EV_ROOM && (cur == 0 || cur == 1) && s.active, "room load %d: a room draw (%d %d)", k, ev, cur);
        run = cur == last ? run + 1 : 1, changes += cur != last, last = cur;
        if (run > maxRun) maxRun = run;
    }
    CHECK(maxRun <= 2 && RZO_RUN_MAX == 2 && changes >= 13 && s.roomDraws == 40 && s.roomChanged == (unsigned)changes && s.seq == 40,
          "40 room loads: at most %d in a row alike (%d), %d changes", RZO_RUN_MAX, maxRun, changes);
    CHECK(Step(&s, RZO_RANDOM, 7, 1, RZO_ALL, &cur, saved, 1, 1) == RZO_EV_NONE, "another list load of the same world: no draw there");
    // Room 302 (no Eileen models, another stage): the set outfit back
    ev = Step(&s, RZO_RANDOM, 1, 0, RZO_ALL, &cur, saved, 1, 0);
    CHECK(ev == RZO_EV_RESTORE && cur == -1 && !s.active && s.restores == 1, "a world without her: the set outfit back (%d %d)", ev, cur);
    CHECK(Step(&s, RZO_RANDOM, 1, 0, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == -1, "and nothing more there");
    // the next Eileen world: a new visit, a new draw (Change Each Visit On); its room draws count from 1 again
    ev = Step(&s, RZO_RANDOM, 8, 1, RZO_ALL, &cur, saved, 1, 1);
    CHECK(ev == RZO_EV_DRAW && s.visit == 3 && s.draws == 2 && s.seq == 0, "the next Eileen world draws again (%d, visit %u)", ev, s.visit);
    // Off in the middle of a visit: the set outfit back at the next step, no draw while Off
    saved = 0;  // (the set outfit was Dress all along in this part)
    RzoInit(&s), cur = 0;
    CHECK(Step(&s, RZO_RANDOM, 3, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_DRAW, "drawn");
    k = cur;
    ev = Step(&s, RZO_OFF, 3, 1, RZO_ALL, &cur, saved, 1, 0);
    CHECK(ev == RZO_EV_RESTORE && cur == 0, "Eileen Outfit Off: the set outfit (Dress) back (%d %d; drawn %d)", ev, cur, k);
    CHECK(Step(&s, RZO_OFF, 4, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == 0, "Off: no draw in the next world");
    CHECK(Step(&s, RZO_OFF, 4, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == 0, "Off: no room draws");
    // switched on in the middle of a visit: drawn at the next room load; the room load after it draws again
    CHECK(Step(&s, RZO_RANDOM, 4, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_DRAW && s.active, "switched on mid-visit: drawn at once");
    CHECK(Step(&s, RZO_RANDOM, 4, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_ROOM, "and the room load after it: a room draw");
    // the player chooses in the Extra Menu: kept until the next world visit, never put back, no room draws meanwhile
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 1);
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 0);
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 0);
    cur = cur ^ 1, saved = cur;  // eo_menu_go: the other outfit, saved to the ini
    ev = Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 0);
    CHECK(ev == RZO_EV_TAKEOVER && !s.active && s.takeovers == 1, "an Extra Menu choice: taken over (%d)", ev);
    k = cur;
    for (ev = 0; ev < 5; ++ev)
        CHECK(Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == k, "kept for the rest of the visit (no room draw)");
    CHECK(Step(&s, RZO_RANDOM, 1, 0, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == k, "Room 302: the player's choice stays (no restore)");
    CHECK(Step(&s, RZO_RANDOM, 6, 1, RZO_ALL, &cur, saved, 1, 1) == RZO_EV_DRAW && s.active, "the next world visit draws again");
    // a choice just before leaving the world (no room load between): the next Eileen world still draws; a world without
    // her keeps the player's choice
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 1);
    cur ^= 1, saved = cur;
    ev = Step(&s, RZO_RANDOM, 6, 1, RZO_ALL, &cur, saved, 1, 1);
    CHECK(ev == RZO_EV_DRAW && s.takeovers == 1 && s.active && s.visit == 2, "a choice, then straight into the next world: drawn there (%d)", ev);
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 1);
    cur ^= 1, saved = cur, k = cur;
    ev = Step(&s, RZO_RANDOM, 1, 0, RZO_ALL, &cur, saved, 1, 0);
    CHECK(ev == RZO_EV_TAKEOVER && cur == k && !s.active && !s.restores, "a choice, then Room 302: the choice stays (%d %d)", ev, cur);
    // an Extra Menu choice of the value we drew (only the saved outfit changes): taken over too
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 1);
    saved = cur;
    CHECK(Step(&s, RZO_RANDOM, 5, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_TAKEOVER, "the drawn outfit chosen in the Extra Menu: the player's");
    // only one outfit given (never with RZO_ALL): nothing drawn, her outfit as set
    RzoInit(&s), cur = 1, saved = 1;
    ev = Step(&s, RZO_RANDOM, 7, 1, 1u, &cur, saved, 1, 1);
    CHECK(ev == RZO_EV_SINGLE && cur == 1 && !s.active && s.single == 1, "one outfit: left as set (%d %d)", ev, cur);
    CHECK(Step(&s, RZO_RANDOM, 7, 1, 1u, &cur, saved, 1, 0) == RZO_EV_NONE && cur == 1, "said once a visit, no room draws");
    // no Eileen models in the world: no draw at room loads
    RzoInit(&s), cur = -1, saved = -1;
    CHECK(Step(&s, RZO_RANDOM, 2, 0, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == -1 && !s.drawn, "a world without her: nothing");
    CHECK(Step(&s, RZO_RANDOM, 2, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_DRAW, "her models come with a room: drawn then (6:43 / 7:10 / 2:55)");
    CHECK(Step(&s, RZO_RANDOM, 2, 0, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_RESTORE && cur == -1, "a room of that world without them: the set outfit");
    // the final fight (stage 9): its world draw stays; said once
    RzoInit(&s), cur = -1, saved = -1;
    CHECK(Step(&s, RZO_RANDOM, RZO_FINAL_STAGE, 1, RZO_ALL, &cur, saved, 1, 1) == RZO_EV_DRAW, "the final fight's world: drawn");
    first = cur;
    CHECK(Step(&s, RZO_RANDOM, RZO_FINAL_STAGE, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == first, "its first room keeps it");
    CHECK(Step(&s, RZO_RANDOM, RZO_FINAL_STAGE, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_HELD && cur == first, "no room draw there: said");
    for (k = 0; k < 6; ++k)
        CHECK(Step(&s, RZO_RANDOM, RZO_FINAL_STAGE, 1, RZO_ALL, &cur, saved, 1, 0) == RZO_EV_NONE && cur == first && !s.roomDraws,
              "the final fight: the outfit stays (%d)", k);
    CHECK(Step(&s, RZO_RANDOM, 8, 1, RZO_ALL, &cur, saved, 1, 1) == RZO_EV_DRAW && !s.heldSaid, "the next world: drawn, said anew");
    // Change Each Visit Off: one draw per world visit (no room draws), the same outfit at every visit of a stage
    {
        int a, b, same = 1, st, none = 1;
        for (st = 2; st <= 9; ++st) {
            RzoInit(&s), cur = -1;
            Step(&s, RZO_RANDOM, st, 1, RZO_ALL, &cur, -1, 0, 1);
            a = cur;
            for (k = 0; k < 6; ++k) none &= Step(&s, RZO_RANDOM, st, 1, RZO_ALL, &cur, -1, 0, 0) == RZO_EV_NONE && cur == a;
            Step(&s, RZO_RANDOM, 1, 0, RZO_ALL, &cur, -1, 0, 0);
            Step(&s, RZO_RANDOM, st, 1, RZO_ALL, &cur, -1, 0, 1);
            b = cur;
            same &= a == b && a >= 0;
        }
        CHECK(same, "Change Each Visit Off: a stage gets the same outfit at every visit");
        CHECK(none, "Change Each Visit Off: no room draws (one draw per world visit, as v3.49)");
    }
    // the same seed gives the same room sequence
    {
        RzoState a, b;
        int ca = -1, cb = -1, same = 1;
        RzoInit(&a), RzoInit(&b);
        Step(&a, RZO_RANDOM, 5, 1, RZO_ALL, &ca, -1, 1, 1), Step(&b, RZO_RANDOM, 5, 1, RZO_ALL, &cb, -1, 1, 1);
        for (k = 0; k < 20; ++k) {
            Step(&a, RZO_RANDOM, 5, 1, RZO_ALL, &ca, -1, 1, 0), Step(&b, RZO_RANDOM, 5, 1, RZO_ALL, &cb, -1, 1, 0);
            same &= ca == cb;
        }
        CHECK(same, "the same seed and visit: the same room draws");
    }
    // the visit count saturates (rando_sim.h RzVisitNext)
    RzoInit(&s), s.visit = 0xFFFFFFFFu;
    Step(&s, RZO_RANDOM, 3, 1, RZO_ALL, &cur, -1, 1, 1);
    CHECK(s.visit == 0xFFFFFFFFu, "the visit count stops at the top");
}

// ---------------------------------------------------------------- the stock exe
typedef struct { int stage, visit, room, n; unsigned id, posVa; float x, y, z; } KnownMover;
static const KnownMover kKnown[] = {  // tools/rzl_scan.py --list
    { 2, 0, 7, 1, 0xFFFF000Au, 0x006809BCu, -1000.0f, 900.0f, -4400.0f },
    { 2, 1, 14, 3, 0xFFFF0008u, 0x00686384u, 10465.0f, -300.0f, 736.0f },
    { 2, 1, 14, 4, 0xFFFF0007u, 0x006863C8u, 10140.0f, -200.0f, 736.0f },
    { 5, 0, 13, 1, 0xFFFF0006u, 0x00661D10u, -1200.0f, 0.0f, -1100.0f },
    { 5, 0, 13, 2, 0xFFFF0001u, 0x00661D54u, -3400.0f, 0.0f, 1900.0f },
    { 6, 0, 22, 1, 0xFFFF0008u, 0x00675874u, 1000.0f, -200.0f, 1500.0f },
    { 8, 0, 5, 1, 0xFFFF000Du, 0x0067AEC0u, 292.0f, -200.0f, 1315.0f },
};
// v3.50 RL350: the calls (E8 rel32) in the first `len` bytes of a function, in order; the index of `target` among them
// from `from` on (-1 = none)
static int CallIndex(unsigned fn, unsigned len, unsigned target, int from)
{
    unsigned a;
    int n = 0;
    for (a = fn; a + 5 <= fn + len; ++a) {
        unsigned rel;
        if (g_img[a - 0x400000u] != 0xE8) continue;
        memcpy(&rel, g_img + (a + 1 - 0x400000u), 4);
        if (a + 5u + rel == target) {
            if (n >= from) return n;
        }
        if (a + 5u + rel >= 0x401000u && a + 5u + rel < 0x5B0000u) ++n;
    }
    return -1;
}
static void TestExeRecords(void)
{
    static const struct { unsigned fn, len; int type; } kCreate[3] = {
        { 0x4AFA90, 880, 0x0B }, { 0x4B58C0, 1184, 0x0C }, { 0x47FB50, 899, 0x0D } };  // EnemyVictimGhostCreate and 0C / 0D
    int i;
    CHECK(ImgU32(0x615348 + 16 * 0x0B + 4) == 0x4AFA90 && ImgU32(0x615348 + 16 * 0x0C + 4) == 0x4B58C0 &&
          ImgU32(0x615348 + 16 * 0x0D + 4) == 0x47FB50, "the type table 0x615348: the creates of 0B / 0C / 0D");
    for (i = 0; i < 3; ++i) {
        const int rc = CallIndex(kCreate[i].fn, kCreate[i].len, 0x482590, 0), gt = CallIndex(kCreate[i].fn, kCreate[i].len, 0x464F20, 0);
        CHECK(rc >= 0 && gt > rc, "type %02X's create %06X: EnemyGhostReconcile 0x482590 (call %d) before EnemySpawnGate 0x464F20 (call %d)",
              kCreate[i].type, kCreate[i].fn, rc, gt);
    }
    CHECK(CallIndex(0x4AD330, 189, 0x464470, 0) >= 0, "0x4AD330 (0B's record restore) calls EnemyRecordRestore 0x464470");
    CHECK(CallIndex(0x464470, 180, 0x46FE80, 0) >= 0, "EnemyRecordRestore puts the model at the record's spot (0x46FE80)");
    CHECK(CallIndex(0x482590, 220, 0x46FE30, 0) >= 0 && CallIndex(0x482590, 220, 0x464380, 0) >= 0,
          "EnemyGhostReconcile writes a record spot (0x46FE30) or deletes it (0x464380)");
    CHECK(!memcmp(g_img + (0x46FE30 - 0x400000), "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x10\x8D\x44\x24\x00\x50\xE8", 15) &&
          !memcmp(g_img + (0x46FE80 - 0x400000), "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x10\x8D\x44\x24\x00\x50\xE8", 15),
          "0x46FE30 / 0x46FE80 (world <-> record, enemies.inc ENM_TO_REC / ENM_FROM_REC) as RzlGhostRec calls them");
    {   // 0x482590: the half chance (MathRand01 < 0.5) is read against the float 0.5
        unsigned a;
        int seen = 0;
        for (a = 0x482590; a < 0x482590 + 220 - 6; ++a)
            if (g_img[a - 0x400000] == 0xD8 || g_img[a - 0x400000] == 0xD9 || g_img[a - 0x400000] == 0xDC) {
                unsigned v;
                float f;
                memcpy(&v, g_img + (a + 2 - 0x400000), 4);
                if (v < 0x5B0000u || v >= 0x700000u) continue;
                memcpy(&f, g_img + (v - 0x400000), 4);
                seen |= f == 0.5f;
            }
        CHECK(seen, "EnemyGhostReconcile compares with 0.5 (the half chance that kept v3.49's moved victims at their record)");
    }
}
static void TestExe(const char *path)
{
    unsigned seen[32];
    int scripts = 0, stage, v, victims = 0, movers = 0, rooms = 0, most = 0, mostSpots = 0, notMoved = 0, bad = 0, i, k, known = 0;
    int known27 = 0;  // v3.50 RL350
    RzWin w;
    if (!LoadExe(path)) {
        CHECK(0, "the stock exe %s could not be read", path);
        return;
    }
    TestExeRecords();  // v3.50 RL350 (the Nurse's unlock check 0x56F590 is no longer used)
    CHECK(!memcmp(g_img + (0x52CAA9 - 0x400000), "\xE8\x52\xFE\xFF\xFF", 5) && !memcmp(g_img + (0x52CB2B - 0x400000), "\xE8\xD0\xFD\xFF\xFF", 5),
          "0x52CAA9 / 0x52CB2B: the runner's stock `call 52C900` (RzlCt7700)");
    CHECK(!memcmp(g_img + (0x4FC830 - 0x400000), "\x33\xC0\xA0\x2D\x81\x3D\x01\xC1\xE8\x02\x83\xE0\x01\xC3", 14),
          "0x4FC830: the half is bit 2 of the byte 0x13D812D (read directly)");
    CHECK(!memcmp(g_img + (0x459490 - 0x400000), "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x34", 9) &&
          !memcmp(g_img + (0x458F00 - 0x400000), "\x55\x8B\xEC\x83\xE4\xF0\x81\xEC\x84\x00\x00\x00", 12) &&
          !memcmp(g_img + (0x46F700 - 0x400000), "\x55\x8B\xEC\x53\x8B\x1D\x90\x5A\xFD\x00", 10) &&
          !memcmp(g_img + (0x454000 - 0x400000), "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x28", 9),
          "the spot test's game functions as rzlooks.inc kRzlFns expects them");
    CHECK(!memcmp(g_img + (0x4F3490 - 0x400000), "\x50\xE8\xDA\xFF\x07\x00", 6) &&
          !memcmp(g_img + (0x4F3514 - 0x400000), "\xC7\x05\x24\xB3\xEC\x00\x00\x00\x00\x00", 10),
          "the outfit engine's eo_stage 0x4F3490 / eo_room 0x4F3514 sites (where its stubs call RzlOutfitWorld / RzlOutfitRoom)");
    w = ExeWin();
    for (stage = 0; stage < RZ_STAGES; ++stage)
        for (v = 0; v < 2; ++v) {
            const unsigned p = ImgU32((v ? 0x6553D8u : 0x6553A4u) + 4u * (unsigned)stage);
            static RzScan sc;
            int dup = 0, r;
            for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
            if (dup) continue;
            seen[scripts++] = p;
            CHECK(RzScanScript(&w, p, &sc), "stage %d visit %d: the script reads", stage, v);
            for (r = 0; r < RZ_ROOMS; ++r) {
                RzgRoom R;
                if (!sc.roomStart[r]) continue;
                if (!RzgScanRoom(&w, sc.roomStart[r], &R)) {
                    ++bad;
                    continue;
                }
                victims += R.victims, movers += R.nm, notMoved += R.notMoved, rooms += R.nm > 0;
                if (R.nm > most) most = R.nm;
                if (R.kind2 > mostSpots) mostSpots = R.kind2;
                CHECK(R.nspot <= RZG_SPOTS_MAX && R.kind2 <= RZG_SPOTS_MAX, "room %d:%d: every placement fits the spot list (%d)", stage, r,
                      R.kind2);
                for (i = 0; i < R.nm; ++i) {
                    const RzgMover *m = &R.m[i];
                    const int var = (int)(m->mode >> 8 & 0xF);
                    CHECK(RzgIsVictim(m->type, var) && !RzgIsStoryGhost(m->type, var) && (m->mode & 0xF) == 0 && RzgGhostId(m->id),
                          "room %d:%d #%d: a victim of mode 0 with its ghost id", stage, r, m->n);
                    CHECK(ImgU32(m->posVa - 4) == 0x00040008u && ImgU32(m->posVa) == m->pos[0] && ImgU32(m->posVa + 4) == m->pos[1] &&
                          ImgU32(m->posVa + 8) == m->pos[2], "room %d:%d #%d: posVa %08X holds cmd 8 (4 args: x y z w)'s first three words", stage, r, m->n, m->posVa);
                    CHECK(m->posVa >= RZ_SCRIPT_LO && m->posVa + 12 <= RZ_SCRIPT_HI, "its words are in the room scripts' object");
                    for (k = 0; k < (int)(sizeof kKnown / sizeof kKnown[0]); ++k) {
                        const KnownMover *K = &kKnown[k];
                        if (K->stage != stage || K->visit != v || K->room != r || K->n != m->n) continue;
                        ++known;
                        CHECK(m->id == K->id && m->posVa == K->posVa && RzgF(m->pos[0]) == K->x && RzgF(m->pos[1]) == K->y &&
                              RzgF(m->pos[2]) == K->z, "room %d:%d #%d: as rzl_scan.py lists it", stage, r, m->n);
                    }
                }
                if (stage == 8 && r == 13) CHECK(R.nm == 0 && R.kind2 >= 1, "8:13: Cynthia's ghost (0C v0) never moves");
                if (stage == 8 && r == 8) CHECK(R.nm == 0, "8:8: Andrew (0E) never moves");
                if (stage == 8 && r == 17) CHECK(R.nm == 0, "8:17: Richard (0F) never moves");
                if (stage == 5 && v == 0 && r == 7) CHECK(R.nm == 0, "5:7 (1st half): the scene victim (0B v2 mode 6) never moves");
                if (stage == 2 && v == 1 && r == 14) CHECK(R.nm == 2 && R.victims == 2, "2:14 (2nd half): two victims move, Cynthia's train ghost (0C v0 mode 7) stays");
                if (stage == 5 && v == 0 && r == 13)
                    CHECK(R.nm == 2 && R.nspot == 0 && R.nghost == 2, "5:13 (1st half): two victims, no spot of theirs is a candidate, two ghost places (%d %d)",
                          R.nspot, R.nghost);  // v3.50 RL350 (was: their two spots)
                CHECK(R.nghost >= R.nm && R.nspot + R.nghost <= R.kind2, "room %d:%d: every mover's place is a ghost place (%d %d)", stage, r,
                      R.nghost, R.nm);  // v3.50 RL350
                if (stage == 2 && v == 0 && r == 7) {  // v3.50 RL350: the user's v3.49 room: the two victims 1982 apart
                    ++known27;
                    CHECK(R.nm == 2 && R.nghost == 2 && RzgDist2(R.ghost[0], R.ghost[1]) > 1900.0f * 1900.0f,
                          "2:7: two victims, two ghost places (%d %d)", R.nm, R.nghost);
                }
            }
        }
    CHECK(scripts == 17, "17 distinct scripts (%d)", scripts);
    CHECK(!bad, "every room reads (%d bad)", bad);
    CHECK(victims == RZG_STOCK_VICTIMS && movers == RZG_STOCK_MOVERS && rooms == RZG_STOCK_ROOMS && !notMoved,
          "the stock scripts: %d victim placements, %d may move, in %d rooms, %d may not (want %d / %d / %d / 0)", victims, movers,
          rooms, notMoved, RZG_STOCK_VICTIMS, RZG_STOCK_MOVERS, RZG_STOCK_ROOMS);
    CHECK(most == 3 && most <= RZG_MOVERS_MAX && mostSpots == 30, "at most %d victims and %d placements in a room", most, mostSpots);
    CHECK(known == (int)(sizeof kKnown / sizeof kKnown[0]), "every known victim found (%d)", known);
    CHECK(known27 == 1, "2:7 (1st half) seen (%d)", known27);
}

// ---------------------------------------------------------------- made-up scripts
static unsigned g_buf[256];
static int g_bn;
static void W(unsigned x) { g_buf[g_bn++] = x; }
static unsigned FB(float f) { return RzgU(f); }
static void Obj(int kind, int type, int nPos, unsigned mode, unsigned id, int funcs)
{
    int k;
    W(0x00010001u), W((unsigned)kind);
    if (funcs) W(0x00010003u), W(0x00470000u);
    if (type >= 0) W(0x00010007u), W((unsigned)type);
    for (k = 0; k < nPos; ++k) W(0x00040008u), W(FB(100.0f * (float)(g_bn + k))), W(FB(0.0f)), W(FB(-50.0f)), W(FB(1.0f));
    W(0x0001000Bu), W(mode);
    W(0x0001000Fu), W(id);
}
static void TestMadeUp(void)
{
    RzWin w;
    RzgRoom R;
    g_bn = 0;
    W(0x00010000u), W(3);                           // the room's cmd 0 (the runner steps it before its loop)
    Obj(2, 0x0B, 1, 0x200, 0xFFFF0001u, 0);         // 0: a victim (0B v2): moves
    Obj(2, 0x0C, 1, 0x000, 0xFFFF0002u, 0);         // 1: Cynthia's ghost (0C v0): story, a spot only
    Obj(2, 0x0D, 1, 0x100, 0xFFFF0009u, 0);         // 2: the old man (0D v1): moves
    Obj(2, 0x0B, 2, 0x000, 0xFFFF0003u, 0);         // 3: two cmd 8: may not move
    Obj(2, 0x0B, 1, 0x006, 0xFFFF0004u, 0);         // 4: scene mode 6: not a victim placement
    Obj(2, 0x0B, 1, 0x000, 0x00000000u, 0);         // 5: no ghost id: not a victim placement
    Obj(2, 0x0B, 1, 0x000, 0xFFFF0005u, 1);         // 6: its own create function: may not move
    Obj(4, -1, 1, 0, 0x09, 0);                      // 7: a pickup: not a spot
    Obj(2, 0x0E, 1, 0x000, 0xFFFF0004u, 0);         // 8: Andrew: story
    Obj(2, 0x03, 0, 0x000, 0, 0);                   // 9: a Hummer without a position: no spot
    W(0x00010000u), W(4);                           // the next room ends this one
    W(0xF0F0F0F0u);
    w.base = (const unsigned char *)g_buf, w.lo = 0x1000u, w.hi = 0x1000u + 4u * (unsigned)g_bn;
    CHECK(RzgScanRoom(&w, 0x1000u + 8u, &R), "a made-up room reads");
    CHECK(R.objects == 10 && R.kind2 == 9, "10 objects, 9 placements (%d %d)", R.objects, R.kind2);
    CHECK(R.nm == 2 && R.m[0].n == 0 && R.m[0].id == 0xFFFF0001u && R.m[1].n == 2 && R.m[1].type == 0x0D,
          "the victims 0 and 2 move (%d)", R.nm);
    CHECK(R.victims == 4 && R.notMoved == 2, "4 victim placements, 2 may not move (two cmd 8, own functions) (%d %d)", R.victims,
          R.notMoved);
    CHECK(R.nspot == 0 && R.nghost == 8, "v3.50 RL350: spots: no ghost's place (8 ghost places: the movers, the story ghosts, the "
          "others), no pickup, no Hummer without one (%d %d)", R.nspot, R.nghost);
    CHECK(R.m[0].posVa == 0x1000u + 4u * 7u, "the first mover's position words (%08X)", R.m[0].posVa);  // cmd 0 (2), cmd 1 (2), cmd 7 (2), cmd 8 tag
    CHECK(g_buf[(R.m[0].posVa - 0x1000u) / 4u - 1u] == 0x00040008u, "posVa follows its cmd 8 word");
    // a read outside the window
    w.hi = 0x1000u + 4u * 20u;
    CHECK(!RzgScanRoom(&w, 0x1000u + 8u, &R) && R.bad, "a script running out of the window is not read");
    // a victim made by cmd 6 is never a mover; the type from cmd 7 only below 0x40
    g_bn = 0;
    W(0x00010000u), W(1);
    W(0x00010001u), W(2), W(0x00010007u), W(0x41u), W(0x00030008u), W(FB(1.0f)), W(FB(2.0f)), W(FB(3.0f)), W(0x0001000Fu), W(0xFFFF0001u);
    W(0xF0F0F0F0u);
    w.base = (const unsigned char *)g_buf, w.lo = 0x1000u, w.hi = 0x1000u + 4u * (unsigned)g_bn;
    CHECK(RzgScanRoom(&w, 0x1000u + 8u, &R) && R.nm == 0 && R.victims == 0 && R.nspot == 1, "an unknown type: a spot, not a victim");
    // a NaN position is never a mover
    g_bn = 0;
    W(0x00010000u), W(1);
    W(0x00010001u), W(2), W(0x00010007u), W(0x0Bu), W(0x00030008u), W(0x7FC00000u), W(FB(0.0f)), W(FB(0.0f)), W(0x0001000Fu),
        W(0xFFFF0001u);
    W(0xF0F0F0F0u);
    w.hi = 0x1000u + 4u * (unsigned)g_bn;
    CHECK(RzgScanRoom(&w, 0x1000u + 8u, &R) && R.nm == 0 && R.notMoved == 1 && R.nspot == 0, "a NaN position: left alone");
}

// ---------------------------------------------------------------- the spot pick
typedef struct { int calls, mode; float floorY; const float (*only)[3]; int nOnly; } FakeGeo;
static int FakeTest(void *ctx, const float *base, float *cand)
{
    FakeGeo *g = (FakeGeo *)ctx;
    int i;
    (void)base;
    ++g->calls;
    if (g->mode == 0) return 0;              // nothing fits
    if (g->mode == 2) {                      // only the given spots
        for (i = 0; i < g->nOnly; ++i)
            if (cand[0] == g->only[i][0] && cand[2] == g->only[i][2]) return 1;
        return 0;
    }
    if (g->mode == 3) {                      // only within 1000, onto its floor
        if (RzgDist2(cand, base) > 1000.0f * 1000.0f) return 0;
        cand[1] = g->floorY;
        return 1;
    }
    if (g->mode == 4) return RzgDist2(cand, base) <= 750.0f * 750.0f;  // v3.50 RL350: a small room (nothing at 800 +)
    return 1;                                // everything fits
}
static void TestPick(void)
{
    static const float stock[4][3] = { { 0, 0, 0 }, { 2000, 0, 0 }, { 0, 0, 1500 }, { -800, 0, -800 } };
    const float base[3] = { 100, -400, 100 };
    float out[3], player[3] = { 900, 0, 100 }, taken[1][3] = { { 2000, 0, 0 } };
    FakeGeo g;
    RzgPickIn in;
    int nv, nt, k, distinct = 0, ok, sh, allWant;
    float seen[64][2];
    memset(&g, 0, sizeof g);
    memset(&in, 0, sizeof in);
    in.base = base, in.key = RzgKey(1, 5, 0, 13, 1, 0), in.stock = stock, in.nStock = 4, in.player = NULL, in.taken = NULL, in.nTaken = 0;
    g.mode = 0;
    CHECK(!RzgPick(&in, FakeTest, &g, out, &nv, &nt) && nv == 0 && nt > 30, "nothing fits: no spot (%d tried)", nt);
    g.mode = 1;
    CHECK(RzgPick2(&in, FakeTest, &g, out, &nv, &nt, &sh) && nv == nt && nv >= 3 + RZG_TIERS * RZG_RING - 2 && nv <= 3 + RZG_TIERS * RZG_RING && !sh,
          "everything fits: the other placements but the one at the ghost's own spot, and the rings (%d of %d)", nv, nt);
    CHECK(RzgDist2(out, base) >= RZG_WANT_SELF * RZG_WANT_SELF && out[1] == base[1], "the spot is 800 + from its stock one, at its height");
    allWant = 1;
    for (k = 0; k < 64; ++k) {  // the key decides; the far ones are taken while there are any
        int j, dup = 0;
        in.key = RzgKey(1, 5, 0, 13, 1, (unsigned)k);
        RzgPick2(&in, FakeTest, &g, out, &nv, &nt, &sh);
        allWant &= !sh && RzgDist2(out, base) >= RZG_WANT_SELF * RZG_WANT_SELF;
        CHECK(RzgDist2(out, base) <= 3001.0f * 3001.0f + 2100.0f * 2100.0f, "visit %d: the spot is in the rules' range", k);
        for (j = 0; j < distinct; ++j) dup |= seen[j][0] == out[0] && seen[j][1] == out[2];
        if (!dup) seen[distinct][0] = out[0], seen[distinct][1] = out[2], ++distinct;
    }
    CHECK(allWant, "v3.50 RL350: with far spots there, every pick is 800 + from its stock spot");
    CHECK(distinct > 20, "the visits give different spots (%d of 64)", distinct);
    {
        float a[3], b[3];
        in.key = 777u;
        RzgPick(&in, FakeTest, &g, a, NULL, NULL), RzgPick(&in, FakeTest, &g, b, NULL, NULL);
        CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2], "the same key gives the same spot");
    }
    // a small room: nothing at 800 +: the nearer ones (500 +), said short
    g.mode = 4;
    ok = 1;
    for (k = 0; k < 32; ++k) {
        in.key = RzgKey(9, 5, 0, 13, 1, (unsigned)k);
        ok &= RzgPick2(&in, FakeTest, &g, out, &nv, &nt, &sh) && sh && RzgDist2(out, base) >= RZG_MIN_SELF * RZG_MIN_SELF - 1.0f &&
              RzgDist2(out, base) <= 750.0f * 750.0f;
    }
    CHECK(ok, "v3.50 RL350: a small room: a spot 500 .. 750 away, said short");
    g.mode = 1;
    // where you arrive: at least 600 away (or as far as the stock spot is); 1000 + while the room allows it
    in.player = player;  // 800 from the stock spot
    ok = 1;
    for (k = 0; k < 64; ++k) {
        in.key = RzgKey(2, 5, 0, 13, 1, (unsigned)k);
        if (RzgPick2(&in, FakeTest, &g, out, NULL, NULL, &sh)) ok &= RzgDist2(out, player) >= RZG_WANT_PLAYER * RZG_WANT_PLAYER && !sh;
    }
    CHECK(ok, "never within 1000 of where you arrive while the room allows it");
    player[0] = 300, player[2] = 100;  // 200 from the stock spot: at least 200
    ok = 1;
    for (k = 0; k < 64; ++k) {
        in.key = RzgKey(3, 5, 0, 13, 1, (unsigned)k);
        if (RzgPick(&in, FakeTest, &g, out, NULL, NULL)) ok &= RzgDist2(out, player) >= 200.0f * 200.0f - 1.0f;
    }
    CHECK(ok, "a stock spot near the door: never nearer to you than it");
    in.player = NULL;
    // another victim's spot
    in.taken = (const float(*)[3])taken, in.nTaken = 1;
    ok = 1;
    for (k = 0; k < 64; ++k) {
        in.key = RzgKey(4, 5, 0, 13, 2, (unsigned)k);
        if (RzgPick(&in, FakeTest, &g, out, NULL, NULL)) ok &= RzgDist2(out, taken[0]) >= RZG_MIN_OTHER * RZG_MIN_OTHER;
    }
    CHECK(ok, "never within 400 of a spot another victim took");
    in.taken = NULL, in.nTaken = 0;
    // the test decides: only the stock spots
    g.mode = 2, g.only = stock, g.nOnly = 4;
    ok = 1;
    for (k = 0; k < 32; ++k) {
        in.key = RzgKey(5, 2, 1, 7, 1, (unsigned)k);
        ok &= RzgPick(&in, FakeTest, &g, out, &nv, NULL) && nv == 3 &&
              ((out[0] == 2000 && out[2] == 0) || (out[0] == 0 && out[2] == 1500) || (out[0] == -800 && out[2] == -800));
    }
    CHECK(ok, "only the room's other placements fit: one of them (the ghost's own spot never)");
    // the test moves the candidate onto its floor; far rings are refused
    g.mode = 3, g.floorY = -50.0f;
    in.key = 4242u;
    CHECK(RzgPick(&in, FakeTest, &g, out, &nv, &nt) && out[1] == -50.0f && RzgDist2(out, base) <= 1000.0f * 1000.0f && nv < nt,
          "the test's floor height is kept; refused candidates are left out (%d of %d)", nv, nt);
    CHECK(RzgKey(1, 5, 0, 13, 1, 0) != RzgKey(1, 5, 0, 13, 1, 1) && RzgKey(1, 5, 0, 13, 1, 0) != RzgKey(1, 5, 1, 13, 1, 0) &&
          RzgKey(1, 5, 0, 13, 1, 0) != RzgKey(1, 5, 0, 13, 2, 0) && RzgKey(1, 5, 0, 13, 1, 0) != RzgKey(2, 5, 0, 13, 1, 0),
          "the key: seed, half, object and visit take part");
    // v3.50 RL350: the user's 2:7 (v3.49 #2 / #3: FFFF000A went to FFFF0007's spot, FFFF0007 near FFFF000A's): with both
    // ghost places given, neither victim lands within 600 of the other's place, both 800 + from their own
    {
        static const float gh[2][3] = { { -1000, 900, -4400 }, { 982, 900, -4395 } };
        float A[3], B[3], tk[1][3];
        int okA = 1, okB = 1, kk, old49 = 0;
        RzgPickIn a;
        g.mode = 1;
        for (kk = 0; kk < 200; ++kk) {  // v3.49's rule: the other victim's place was a candidate (the swap)
            memset(&a, 0, sizeof a);
            a.base = gh[0], a.key = RzgKey((unsigned)kk, 2, 0, 7, 1, 2), a.stock = gh, a.nStock = 2;
            if (RzgPick(&a, FakeTest, &g, A, NULL, NULL)) old49 += RzgDist2(A, gh[1]) < 600.0f * 600.0f;
        }
        CHECK(old49 > 0, "without the ghost places (v3.49): the other victim's place is taken now and then (%d of 200)", old49);
        for (kk = 0; kk < 200; ++kk) {
            memset(&a, 0, sizeof a);
            a.base = gh[0], a.key = RzgKey((unsigned)kk, 2, 0, 7, 1, 2), a.stock = gh, a.nStock = 2, a.ghosts = gh, a.nGhosts = 2;
            okA &= RzgPick2(&a, FakeTest, &g, A, NULL, NULL, &sh) && !sh && RzgDist2(A, gh[1]) >= 600.0f * 600.0f &&
                   RzgDist2(A, gh[0]) >= 800.0f * 800.0f;
            memcpy(tk[0], A, 12);
            a.base = gh[1], a.key = RzgKey((unsigned)kk, 2, 0, 7, 2, 2), a.taken = (const float(*)[3])tk, a.nTaken = 1;
            okB &= RzgPick2(&a, FakeTest, &g, B, NULL, NULL, &sh) && RzgDist2(B, gh[0]) >= 600.0f * 600.0f &&
                   RzgDist2(B, A) >= RZG_MIN_OTHER * RZG_MIN_OTHER && RzgDist2(B, gh[1]) >= 800.0f * 800.0f;
        }
        CHECK(RZG_MIN_GHOST == 600.0f && RZG_WANT_SELF == 800.0f, "the ghost-place and wanted distances (600 / 800)");
        CHECK(okA && okB, "2:7: no swap: each victim 600 + from the other's place, 800 + from its own (%d %d)", okA, okB);
        // only the other ghost's place fits: refused (v3.49 took it)
        g.mode = 2, g.only = (const float(*)[3])gh, g.nOnly = 2;
        memset(&a, 0, sizeof a);
        a.base = gh[0], a.key = 5u, a.stock = gh, a.nStock = 2, a.ghosts = gh, a.nGhosts = 2;
        CHECK(!RzgPick(&a, FakeTest, &g, A, &nv, &nt) && nv == 0, "the other victim's place is never a spot (%d of %d)", nv, nt);
        g.mode = 1;
    }
}

// ---------------------------------------------------------------- v3.50 RL350: the record rule
static void TestRecords(void)
{
    CHECK(RzgRecRule(0, 0.0f, 2, 7, 2, 7) == RZG_REC_NONE, "a free record: none");
    CHECK(RzgRecRule(1, 0.0f, 2, 7, 2, 7) == RZG_REC_MOVE && RzgRecRule(0x0D, -1.0f, 2, 7, 2, 7) == RZG_REC_MOVE,
          "a standing record in this room: moved with its ghost");
    CHECK(RzgRecRule(0x0B, 0.0f, 2, 7, 2, 7) == RZG_REC_PINNED, "pinned (0x0B) here: kept where it lies");
    CHECK(RzgRecRule(0x0A, 0.0f, 2, 7, 2, 7) == RZG_REC_DOWN && RzgRecRule(1, 3.5f, 2, 7, 2, 7) == RZG_REC_DOWN &&
          RzgRecRule(0x12, 0.0f, 2, 7, 2, 7) == RZG_REC_DOWN, "down (0x0A, a countdown, a scene state): kept");
    CHECK(RzgRecRule(1, 0.0f, 2, 9, 2, 7) == RZG_REC_ELSEWHERE && RzgRecRule(0x0B, 0.0f, 2, 9, 2, 7) == RZG_REC_ELSEWHERE,
          "a record in another room of the world: left alone");
    CHECK(RzgRecRule(1, 0.0f, 5, 7, 2, 7) == RZG_REC_OTHERWORLD, "another world's record (the game deletes it): left alone");
    CHECK(RzgRecWorse(RZG_REC_MOVE, RZG_REC_PINNED) == RZG_REC_PINNED && RzgRecWorse(RZG_REC_PINNED, RZG_REC_MOVE) == RZG_REC_PINNED &&
          RzgRecWorse(RZG_REC_NONE, RZG_REC_MOVE) == RZG_REC_MOVE && RzgRecWorse(RZG_REC_ELSEWHERE, RZG_REC_MOVE) == RZG_REC_MOVE &&
          RzgRecWorse(RZG_REC_DOWN, RZG_REC_PINNED) == RZG_REC_PINNED, "several records of one id: a kept one wins over a move");
    CHECK(!strcmp(kRzgRecName[RZG_REC_MOVE], "moved") && !strcmp(kRzgRecName[RZG_REC_NONE], "none") &&
          !strcmp(kRzgRecName[RZG_REC_PINNED], "pinned here"), "the record names the log uses");
}

// ---------------------------------------------------------------- patch-and-restore (rando_sim.h's stack)
static void TestPatch(void)
{
    static RzPatchStack st;
    unsigned script[8] = { 0x00030008u, 0x11, 0x22, 0x33, 0x0001000Fu, 0x09, 0x44, 0x55 };
    const unsigned stock[8] = { 0x00030008u, 0x11, 0x22, 0x33, 0x0001000Fu, 0x09, 0x44, 0x55 };
    int d, k, n0, other;
    memset(&st, 0, sizeof st);
    d = RzStackPush(&st);
    CHECK(RzStackAdd(&st, d, &script[5], 0x09, 0x0B), "an Items word written first");
    n0 = st.n[d];
    for (k = 0; k < 3; ++k) CHECK(RzStackAdd(&st, d, &script[1 + k], stock[1 + k], 0xA0u + (unsigned)k), "position word %d", k);
    CHECK(script[1] == 0xA0 && script[2] == 0xA1 && script[3] == 0xA2 && script[5] == 0x0B, "all written");
    other = RzStackPop(&st, d);
    CHECK(!other && !memcmp(script, stock, sizeof script) && st.depth == 0, "all back after the runner");
    // a position word that is not the stock value: that ghost's words go back, the Items' word stays written
    d = RzStackPush(&st);
    RzStackAdd(&st, d, &script[5], 0x09, 0x0B);
    n0 = st.n[d];
    script[3] = 0x99;  // another writer
    for (k = 0; k < 3; ++k)
        if (!RzStackAdd(&st, d, &script[1 + k], stock[1 + k], 0xA0u + (unsigned)k)) break;
    CHECK(k == 2, "the third word is not stock: refused");
    RzStackTrim(&st, d, n0);
    CHECK(script[1] == 0x11 && script[2] == 0x22 && script[3] == 0x99 && script[5] == 0x0B && st.n[d] == n0,
          "its first two words back, the other writer's word left, the Items' word kept");
    other = RzStackPop(&st, d);
    CHECK(!other && script[5] == 0x09, "the Items' word back after the runner");
}

int main(int argc, char **argv)
{
    const char *exe = argc > 1 ? argv[1] : "C:\\ClaudeCode\\CModding\\SH4Decomp\\orig\\sh4.exe";
    TestTexts();
    TestDraw();
    TestOverride();
    TestExe(exe);
    TestMadeUp();
    TestPick();
    TestRecords();  // v3.50 RL350
    TestPatch();
    printf("rzlooks_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
