// v3.49 RZ349L: native test of src\rzlooks_sim.h (test\build_rzlooks_test.cmd): the [Randomizer] EileenOutfit / Ghosts
// texts; the outfit draw (only the unlocked outfits, both of them over seeds and visits, fixed without Change Each Visit)
// and the override's rules (a draw per world visit, the set outfit back for Off or a world without her, an Extra Menu
// choice kept until the next visit, nothing to draw with one outfit); the victim-ghost scan on the stock exe (SH4Decomp
// orig\sh4.exe: 45 victim placements in 32 rooms, all may move, their position words, the story ghosts and the scene
// modes left out) and on made-up scripts (the exclusions, reads outside the window); the spot pick (the distance rules,
// the test decides, the key decides, nothing found); patch-and-restore of the position words with the Items' words.
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
    CHECK(RzoUnlocked(0) == 1u && RzoUnlocked(1) == 3u && RzoUnlocked(7) == 3u, "unlocked: the Dress always, the Nurse by the flag");
    for (s = 1; s <= 2000; ++s) {
        const int a = RzoDraw((unsigned)s, 3, 1, 0, 3u), b = RzoDraw((unsigned)s, 3, 1, 0, 3u);
        CHECK(a == 0 || a == 1, "a draw is the Dress or the Nurse (%d)", a);
        CHECK(a == b, "the same inputs give the same outfit");
        CHECK(RzoDraw((unsigned)s, 3, 1, 5, 1u) == -1 && RzoDraw((unsigned)s, 3, 1, 5, 0u) == -1 && RzoDraw((unsigned)s, 3, 1, 5, 2u) == -1,
              "one outfit (or none) unlocked: nothing to draw");
        if (a >= 0) ++h[a];
        for (v = 1; v <= 8; ++v) {
            changed += RzoDraw((unsigned)s, 3, 1, (unsigned)v, 3u) != RzoDraw((unsigned)s, 3, 1, (unsigned)(v - 1), 3u);
            ++n;
        }
    }
    CHECK(h[0] > 900 && h[1] > 900, "both outfits come out over seeds (Dress %d, Nurse %d of 2000)", h[0], h[1]);
    CHECK(changed > n / 3 && changed < 2 * n / 3, "the next visit draws anew (%d of %d visits changed)", changed, n);
    {
        int st, diff = 0;
        for (st = 2; st <= 11; ++st) diff += RzoDraw(99, st, 1, 0, 3u) != RzoDraw(99, 2, 1, 0, 3u);
        CHECK(diff >= 2, "the stage takes part in the draw (%d of 9 differ from stage 2)", diff);
    }
}

// ---------------------------------------------------------------- the override's rules
static int Step(RzoState *s, int mode, int stage, int here, unsigned unlocked, int *cur, int saved, int perVisit)
{
    int put, ev = RzoStep(s, mode, 1234u, perVisit, stage, 1, here, unlocked, *cur, saved, &put);
    if (put >= -1) *cur = put;
    return ev;
}
static void TestOverride(void)
{
    RzoState s;
    int cur = -1, saved = -1, ev, first, k;
    // a world visit: the draw at her world's file list, nothing more at its room loads
    RzoInit(&s);
    ev = Step(&s, RZO_RANDOM, 7, 1, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_DRAW && (cur == 0 || cur == 1) && s.active && s.visit == 1, "a new Eileen world: drawn (%d -> %d)", ev, cur);
    first = cur;
    for (k = 0; k < 5; ++k) CHECK(Step(&s, RZO_RANDOM, 7, 1, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == first,
                                  "its rooms: no new draw (%d)", k);
    // Room 302 (no Eileen models, another stage): the set outfit back
    ev = Step(&s, RZO_RANDOM, 1, 0, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_RESTORE && cur == -1 && !s.active && s.restores == 1, "a world without her: the set outfit back (%d %d)", ev, cur);
    CHECK(Step(&s, RZO_RANDOM, 1, 0, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == -1, "and nothing more there");
    // the next Eileen world: a new visit, a new draw (Change Each Visit On)
    ev = Step(&s, RZO_RANDOM, 8, 1, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_DRAW && s.visit == 3 && s.draws == 2, "the next Eileen world draws again (%d, visit %u)", ev, s.visit);
    // Off in the middle of a visit: the set outfit back at the next step, no draw while Off
    saved = 0;  // (the set outfit was Dress all along in this part)
    RzoInit(&s), cur = 0;
    CHECK(Step(&s, RZO_RANDOM, 3, 1, 3u, &cur, saved, 1) == RZO_EV_DRAW, "drawn");
    k = cur;
    ev = Step(&s, RZO_OFF, 3, 1, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_RESTORE && cur == 0, "Eileen Outfit Off: the set outfit (Dress) back (%d %d; drawn %d)", ev, cur, k);
    CHECK(Step(&s, RZO_OFF, 4, 1, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == 0, "Off: no draw in the next world");
    // switched on in the middle of a visit: drawn at the next room load
    CHECK(Step(&s, RZO_RANDOM, 4, 1, 3u, &cur, saved, 1) == RZO_EV_DRAW && s.active, "switched on mid-visit: drawn at once");
    // the player chooses in the Extra Menu: kept until the next world visit, never put back
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1);
    cur = cur ^ 1, saved = cur;  // eo_menu_go: the other outfit, saved to the ini
    ev = Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_TAKEOVER && !s.active && s.takeovers == 1, "an Extra Menu choice: taken over (%d)", ev);
    k = cur;
    CHECK(Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == k, "kept for the rest of the visit");
    CHECK(Step(&s, RZO_RANDOM, 1, 0, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == k, "Room 302: the player's choice stays (no restore)");
    CHECK(Step(&s, RZO_RANDOM, 6, 1, 3u, &cur, saved, 1) == RZO_EV_DRAW && s.active, "the next world visit draws again");
    // a choice just before leaving the world (no room load between): the next Eileen world still draws; a world without
    // her keeps the player's choice
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1);
    cur ^= 1, saved = cur;
    ev = Step(&s, RZO_RANDOM, 6, 1, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_DRAW && s.takeovers == 1 && s.active && s.visit == 2, "a choice, then straight into the next world: drawn there (%d)", ev);
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1);
    cur ^= 1, saved = cur, k = cur;
    ev = Step(&s, RZO_RANDOM, 1, 0, 3u, &cur, saved, 1);
    CHECK(ev == RZO_EV_TAKEOVER && cur == k && !s.active && !s.restores, "a choice, then Room 302: the choice stays (%d %d)", ev, cur);
    // an Extra Menu choice of the value we drew (only the saved outfit changes): taken over too
    RzoInit(&s), cur = -1, saved = -1;
    Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1);
    saved = cur;
    CHECK(Step(&s, RZO_RANDOM, 5, 1, 3u, &cur, saved, 1) == RZO_EV_TAKEOVER, "the drawn outfit chosen in the Extra Menu: the player's");
    // only the Dress unlocked: nothing drawn, her outfit as set
    RzoInit(&s), cur = 1, saved = 1;
    ev = Step(&s, RZO_RANDOM, 7, 1, 1u, &cur, saved, 1);
    CHECK(ev == RZO_EV_SINGLE && cur == 1 && !s.active && s.single == 1, "one outfit unlocked: left as set (%d %d)", ev, cur);
    CHECK(Step(&s, RZO_RANDOM, 7, 1, 1u, &cur, saved, 1) == RZO_EV_NONE, "said once a visit");
    // no Eileen models in the world: no draw at room loads
    RzoInit(&s), cur = -1, saved = -1;
    CHECK(Step(&s, RZO_RANDOM, 2, 0, 3u, &cur, saved, 1) == RZO_EV_NONE && cur == -1 && !s.drawn, "a world without her: nothing");
    CHECK(Step(&s, RZO_RANDOM, 2, 1, 3u, &cur, saved, 1) == RZO_EV_DRAW, "her models come with a room: drawn then (6:43 / 7:10 / 2:55)");
    // Change Each Visit Off: the same outfit at every visit of a stage
    {
        int a, b, same = 1, st;
        for (st = 2; st <= 9; ++st) {
            RzoInit(&s), cur = -1;
            Step(&s, RZO_RANDOM, st, 1, 3u, &cur, -1, 0);
            a = cur;
            Step(&s, RZO_RANDOM, 1, 0, 3u, &cur, -1, 0);
            Step(&s, RZO_RANDOM, st, 1, 3u, &cur, -1, 0);
            b = cur;
            same &= a == b && a >= 0;
        }
        CHECK(same, "Change Each Visit Off: a stage gets the same outfit at every visit");
    }
    // the visit count saturates (rando_sim.h RzVisitNext)
    RzoInit(&s), s.visit = 0xFFFFFFFFu;
    Step(&s, RZO_RANDOM, 3, 1, 3u, &cur, -1, 1);
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
static void TestExe(const char *path)
{
    unsigned seen[32];
    int scripts = 0, stage, v, victims = 0, movers = 0, rooms = 0, most = 0, mostSpots = 0, notMoved = 0, bad = 0, i, k, known = 0;
    RzWin w;
    if (!LoadExe(path)) {
        CHECK(0, "the stock exe %s could not be read", path);
        return;
    }
    CHECK(!memcmp(g_img + (0x56F590 - 0x400000), "\xA1\x50\x1A\x3D\x01\xC3", 6),
          "0x56F590 SaveGetNurseUnlocked = mov eax,[013D1A50] / ret (rzlooks.inc kRzoNurseFn)");
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
                if (stage == 5 && v == 0 && r == 13) CHECK(R.nm == 2 && R.nspot == 2, "5:13 (1st half): two victims, their two spots (%d)", R.nspot);
            }
        }
    CHECK(scripts == 17, "17 distinct scripts (%d)", scripts);
    CHECK(!bad, "every room reads (%d bad)", bad);
    CHECK(victims == RZG_STOCK_VICTIMS && movers == RZG_STOCK_MOVERS && rooms == RZG_STOCK_ROOMS && !notMoved,
          "the stock scripts: %d victim placements, %d may move, in %d rooms, %d may not (want %d / %d / %d / 0)", victims, movers,
          rooms, notMoved, RZG_STOCK_VICTIMS, RZG_STOCK_MOVERS, RZG_STOCK_ROOMS);
    CHECK(most == 3 && most <= RZG_MOVERS_MAX && mostSpots == 30, "at most %d victims and %d placements in a room", most, mostSpots);
    CHECK(known == (int)(sizeof kKnown / sizeof kKnown[0]), "every known victim found (%d)", known);
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
    CHECK(R.nspot == 8, "spots: every placement with one or more positions, the movers too, no pickup, no Hummer without one (%d)",
          R.nspot);
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
    return 1;                                // everything fits
}
static void TestPick(void)
{
    static const float stock[4][3] = { { 0, 0, 0 }, { 2000, 0, 0 }, { 0, 0, 1500 }, { -800, 0, -800 } };
    const float base[3] = { 100, -400, 100 };
    float out[3], player[3] = { 900, 0, 100 }, taken[1][3] = { { 2000, 0, 0 } };
    FakeGeo g;
    RzgPickIn in;
    int nv, nt, k, distinct = 0, ok;
    float seen[64][2];
    memset(&g, 0, sizeof g);
    in.base = base, in.key = RzgKey(1, 5, 0, 13, 1, 0), in.stock = stock, in.nStock = 4, in.player = NULL, in.taken = NULL, in.nTaken = 0;
    g.mode = 0;
    CHECK(!RzgPick(&in, FakeTest, &g, out, &nv, &nt) && nv == 0 && nt > 20, "nothing fits: no spot (%d tried)", nt);
    g.mode = 1;
    CHECK(RzgPick(&in, FakeTest, &g, out, &nv, &nt) && nv == nt && nv == 4 + RZG_TIERS * RZG_RING - 1,
          "everything fits: the stock spots but the one at the ghost's own spot, and the rings (%d of %d)", nv, nt);
    CHECK(RzgDist2(out, base) >= RZG_MIN_SELF * RZG_MIN_SELF && out[1] == base[1], "the spot is away from its stock one, at its height");
    for (k = 0; k < 64; ++k) {  // the key decides
        int j, dup = 0;
        in.key = RzgKey(1, 5, 0, 13, 1, (unsigned)k);
        RzgPick(&in, FakeTest, &g, out, &nv, &nt);
        CHECK(RzgDist2(out, base) >= RZG_MIN_SELF * RZG_MIN_SELF && RzgDist2(out, base) <= 2201.0f * 2201.0f + 2100.0f * 2100.0f,
              "visit %d: the spot is in the rules' range", k);
        for (j = 0; j < distinct; ++j) dup |= seen[j][0] == out[0] && seen[j][1] == out[2];
        if (!dup) seen[distinct][0] = out[0], seen[distinct][1] = out[2], ++distinct;
    }
    CHECK(distinct > 20, "the visits give different spots (%d of 64)", distinct);
    {
        float a[3], b[3];
        in.key = 777u;
        RzgPick(&in, FakeTest, &g, a, NULL, NULL), RzgPick(&in, FakeTest, &g, b, NULL, NULL);
        CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2], "the same key gives the same spot");
    }
    // where you arrive: at least 600 away (or as far as the stock spot is)
    in.player = player;  // 800 from the stock spot
    ok = 1;
    for (k = 0; k < 64; ++k) {
        in.key = RzgKey(2, 5, 0, 13, 1, (unsigned)k);
        if (RzgPick(&in, FakeTest, &g, out, NULL, NULL)) ok &= RzgDist2(out, player) >= 600.0f * 600.0f;
    }
    CHECK(ok, "never within 600 of where you arrive");
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
    CHECK(ok, "never within 250 of a spot another victim took");
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
    TestPatch();
    printf("rzlooks_sim_test: %d check(s), %d failure(s)\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
