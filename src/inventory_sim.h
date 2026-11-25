// ================================================================ v3.37 INV337: the pure rules of Extended Toolbelt and
// Stackable Items (src/inventory.inc; the hooks themselves are the cheat table's code, src/inventory_cave.inc).
// No Windows, no game memory: everything works on an InvBlock (the layout of the game's inventory at 0x13D7B60, 520h
// bytes, saved whole) and on copies of the exe's tables. test/inventory_test.c checks these against the generated asm
// (the cave's helpers run on a block at the real address) and on their own.
//
// The model of the cheat table's Extend Toolbelt v6 (CT 7310) and Stackable Items v3 (CT 7360), with Refined's
// changes (v3.37): InvGate lets the weapon in hand and Playable Eileen's weapons 80h..84h count on page 2 (the attack
// bug), StFind takes Refined's own "toolbelt on" flag, and InvOffPlan / InvOffApply are the switch-off move (page 2
// into the Storage Chest; the user's choice) that replaces the table's refusal.
#ifndef INVENTORY_SIM_H
#define INVENTORY_SIM_H

#define INV_P1 10          // page 1 = the belt, entries 0..9
#define INV_P2_FIRST 0x88  // page 2 = chest entries 126..135 = "pocket index" 88h..91h (0x13D7B60 + 8 x idx)
#define INV_P2_END 0x92
#define INV_ENTRIES 146    // 10 pockets + 136 chest entries
#define INV_CHEST_CAP 126  // the chest's logical size while the toolbelt is on (and the game's 7Eh limit)
#define INV_CHEST_PHYS 136 // entries in memory (the stock sort / merge / recount run over 88h)
#define INV_RANK_N 0x78    // ids in the game's sort table 5BF220
#define INV_MEDALLION 8

typedef struct { float cond; unsigned short count; unsigned char id, flag; } InvEnt;  // +0 +4 +6 +7
typedef struct {
    InvEnt e[INV_ENTRIES];         // 0x000: pockets[10], then chest[136] (e[10 + k] = chest k)
    unsigned short chestCount;     // 0x490
    unsigned char ammoType, pad0;  // 0x492: the pistol's loaded ammo type (1 / 3)
    unsigned char medFlag, pad1[3];// 0x494: a Saint Medallion is in use
    unsigned char obtained[0x88];  // 0x498
} InvBlock;
typedef char InvBlockSize[sizeof(InvBlock) == 0x520 ? 1 : -1];

static int InvIsP2Index(int idx) { return idx >= INV_P2_FIRST && idx < INV_P2_END; }
static int InvP2Index(int i) { return INV_P2_FIRST + i; }             // page-2 entry i -> its pocket index
static int InvChestOfIndex(int idx) { return idx - INV_P1; }           // pocket index -> chest entry (88h+i -> 126+i)
static InvEnt *InvP2(InvBlock *b) { return &b->e[INV_P2_FIRST]; }
static const InvEnt *InvP2c(const InvBlock *b) { return &b->e[INV_P2_FIRST]; }

// pk_cnt10: used entries of a 10-entry page
static int InvCnt10(const InvEnt *page)
{
    int i, n = 0;
    for (i = 0; i < 10; ++i) n += page[i].id != 0;
    return n;
}
// pk_find: the first entry with this id, page 1 (0..9) then page 2 (88h+i), -1 (id 0 finds the first empty one)
static int InvFind(const InvBlock *b, int id)
{
    int idx = 0;
    for (;;) {
        if (b->e[idx].id == (unsigned)id) return idx;
        if (++idx == INV_P1) idx = INV_P2_FIRST;
        if (idx >= INV_P2_END) return -1;
    }
}
// pk_gate (v3.37): may this weapon be used from page 2? idx from InvFind (a page-1 index or -1 passes unchanged).
// The table: guns 10h / 11h always, melee 12h..35h with pk_mel (1), else never. v3.37: also the weapon in hand and
// Playable Eileen's 80h..84h.
static int InvGate(int idx, int id, int inHand, int mel)
{
    if (!InvIsP2Index(idx)) return idx;
    if (id == inHand) return idx;
    if (id >= 0x80 && id <= 0x84) return idx;
    if (id < 0x10) return -1;
    if (id <= 0x11) return idx;
    if (id > 0x35) return -1;
    return mel ? idx : -1;
}
// the cheat table's gate (v6), for the tests: what v3.37 changes
static int InvGateCt(int idx, int id, int mel)
{
    if (!InvIsP2Index(idx)) return idx;
    if (id < 0x10) return -1;
    if (id <= 0x11) return idx;
    if (id > 0x35) return -1;
    return mel ? idx : -1;
}
// pk_p2sum: the counts of the page-2 entries with this id
static int InvP2Sum(const InvBlock *b, int id)
{
    int i, s = 0;
    for (i = 0; i < 10; ++i)
        if (InvP2c(b)[i].id == (unsigned)id) s += InvP2c(b)[i].count;
    return s;
}
// pk_p2del: page-2 entry i goes, the rest moves up, the empty template (zero) at the end
static void InvP2Del(InvBlock *b, int i, const InvEnt *tmpl)
{
    InvEnt *p = InvP2(b);
    for (; i + 1 < 10; ++i) p[i] = p[i + 1];
    p[9] = *tmpl;
}
// pk_medslot: the Saint Medallion in use (id 08 with +7), page 1 then page 2, -1
static int InvMedSlot(const InvBlock *b)
{
    int idx = 0;
    for (;;) {
        if (b->e[idx].id == INV_MEDALLION && b->e[idx].flag) return idx;
        if (++idx == INV_P1) idx = INV_P2_FIRST;
        if (idx >= INV_P2_END) return -1;
    }
}
// 4F5500 / pk_rank: the sort rank of an id (0 -> 7FFFFFFF, unknown -> -1)
static int InvRank(const unsigned char *rank, int id)
{
    int i;
    if (!id) return 0x7FFFFFFF;
    for (i = 0; i < INV_RANK_N; ++i)
        if (rank[i] == (unsigned)(id & 0xFF)) return i;
    return -1;
}
// pk_sortn: stable insertion sort by rank
static void InvSortN(InvEnt *v, int n, const unsigned char *rank)
{
    int k, j;
    for (k = 1; k < n; ++k) {
        const InvEnt key = v[k];
        const int kr = InvRank(rank, key.id);
        for (j = k - 1; j >= 0 && InvRank(rank, v[j].id) > kr; --j) v[j + 1] = v[j];
        v[j + 1] = key;
    }
}
// pk_getpins: pin0 = the weapon id in hand that stays visible (0 none), *pinMed = the medallion in use stays
static int InvPins(int inHand, int pin, int mel, int *pinMed)
{
    *pinMed = pin != 0;
    if (pin && (inHand == 0x10 || inHand == 0x11)) return inHand;
    if (!mel && inHand >= 0x12 && inHand <= 0x35) return inHand;
    return 0;
}
static int InvPinned(const InvEnt *e, int pin0, int pinMed) { return e->id == (unsigned)pin0 || (e->flag && pinMed); }
// pk_do_swap: the rotation of a flip. New page 1 = pins + the first hidden items up to 10 (sorted by rank), new page 2 =
// the hidden items left, then the old visible ones (queue order). Returns the items that changed page (0 = nothing),
// -1 = refused (nothing written; cannot happen with 10 + 10).
static int InvSwap(InvBlock *b, int pin0, int pinMed, const unsigned char *rank, const InvEnt *tmpl)
{
    InvEnt vis[10], hid[10], q[10];
    int nv = 0, nh = 0, nq = 0, mv = 0, i;
    for (i = 0; i < 10; ++i) {
        const InvEnt *e = &b->e[i];
        if (!e->id) continue;
        if (InvPinned(e, pin0, pinMed)) vis[nv++] = *e;
        else q[nq++] = *e, ++mv;
    }
    for (i = 0; i < 10; ++i) {
        const InvEnt *e = &InvP2c(b)[i];
        if (!e->id) continue;
        if (nv < 10) {
            vis[nv++] = *e;
            ++mv;
        } else {
            if (nh >= 10) return -1;
            hid[nh++] = *e;
        }
    }
    for (i = 0; i < nq; ++i) {
        if (nh >= 10) return -1;
        hid[nh++] = q[i];
    }
    if (!mv) return 0;
    InvSortN(vis, nv, rank);
    for (i = 0; i < 10; ++i) b->e[i] = i < nv ? vis[i] : *tmpl;
    for (i = 0; i < 10; ++i) InvP2(b)[i] = i < nh ? hid[i] : *tmpl;
    return mv;
}
// pk_class: what moves to page 2 first when page 1 overflows (0 broken weapon, 1 weapon, 2 Eileen weapon, 3 key / story,
// 4 other, 5 the weapon in hand)
static int InvClass(int id, int inHand)
{
    if (id == inHand) return 5;
    if (id == 0x2E || (id >= 0x16 && id <= 0x2C && !(id & 1))) return 0;
    if (id >= 0x10 && id <= 0x35) return 1;
    if (id >= 0x80 && id <= 0x84) return 2;
    if (id >= 0x3C && id <= 0x7D) return 3;
    return 4;
}
// pk_victim: the visible item that moves (lowest class, the last among equals; the medallion in use counts 5), -1
static int InvVictim(const InvBlock *b, int pin0, int pinMed, int inHand)
{
    int i, best = -1, bc = 6;
    for (i = 0; i < 10; ++i) {
        const InvEnt *e = &b->e[i];
        int c;
        if (!e->id || InvPinned(e, pin0, pinMed)) continue;
        c = e->flag ? 5 : InvClass(e->id, inHand);
        if (c <= bc) bc = c, best = i;
    }
    return best;
}
// pk_full (4F57B0 replaced): 1 only when page 1 is full and nothing can overflow
static int InvFull(const InvBlock *b, int ovf, int pin0, int pinMed, int inHand)
{
    if (InvCnt10(b->e) < 10) return 0;
    if (!ovf) return 1;
    if (InvCnt10(InvP2c(b)) >= 10) return 1;
    return InvVictim(b, pin0, pinMed, inHand) < 0;
}
// pk_empty (4F6020 replaced): a gun / ammo id carried with count 0 (01..03, 10h..35h), page 1 then page 2
static int InvEmpty(const InvBlock *b, int id)
{
    int idx;
    if ((unsigned)id <= 3) {
        if (!id) return 0;
    } else if (id < 0x10 || id > 0x35) {
        return 0;
    }
    idx = InvFind(b, id);
    return idx >= 0 && b->e[idx].count == 0;
}
// pk_has (5171E0 replaced): page 1 as the game, page 2 for key / story items 3Ch..7Dh (pk_optc)
static int InvHas(const InvBlock *b, int id, int optc)
{
    int i;
    for (i = 0; i < 10; ++i)
        if (b->e[i].id == (unsigned)id) return 1;
    if (!optc || id < 0x3C || id > 0x7D) return 0;
    for (i = 0; i < 10; ++i)
        if (InvP2c(b)[i].id == (unsigned)id) return 1;
    return 0;
}
// pk_moff (4F5D20 replaced): no medallion in use, on either page
static void InvMedOff(InvBlock *b)
{
    int i;
    b->medFlag = 0;
    for (i = 0; i < 10; ++i) {
        if (b->e[i].id == INV_MEDALLION) b->e[i].flag = 0;
        if (InvP2(b)[i].id == INV_MEDALLION) InvP2(b)[i].flag = 0;
    }
}

// ---------------------------------------------------------------- Stackable Items
#define ST_MAX 99     // st_max
#define ST_THIEF 10   // the item thief's places per entry (10 x 10 = its 100-place list)
// st_find: the first entry with this id and count < max, page 1, then page 2 while the toolbelt is on, -1
static int StFind(const InvBlock *b, int id, int max, int tbLive)
{
    int idx = 0;
    for (;;) {
        if (b->e[idx].id == (unsigned)id && b->e[idx].count < (unsigned)max) return idx;
        if (++idx == INV_P1) {
            if (!tbLive) return -1;
            idx = INV_P2_FIRST;
        }
        if (idx >= INV_P2_END) return -1;
    }
}
// st_brki: the place of an id in the game's breakable list 5BF188 (zero-ended), -1
static int StBrk(const unsigned char *brk, int id)
{
    int i;
    for (i = 0; brk[i]; ++i)
        if (brk[i] == (unsigned)(id & 0xFF)) return i;
    return -1;
}
// st_newc: the condition of a new one (5BF198 for breakables, 1.0 else)
static float StNewCond(const unsigned char *brk, const float *newCond, int id)
{
    const int i = StBrk(brk, id);
    return i < 0 ? 1.0f : newCond[i];
}
// st_stw (463607, always in): the item thief lists an entry once per count, at most 10 times
static int StThiefCap(int n) { return n > ST_THIEF ? ST_THIEF : n; }
// st_give: a stack with room takes the unit; *rest = what becomes a new entry (0 = all joined)
static int StGiveJoin(int have, int unit, int max, int *rest)
{
    const int room = max - have;
    if (unit <= room) {
        *rest = 0;
        return unit;
    }
    *rest = unit - room;
    return room;
}
// st_plan: a chest take-out. want = min(chest count, max) (takeout 0: the game's unit); *join of it joins the stack
// (stackCount < 0: none), *rest makes one new entry when the belt has room (full: it stays in the chest); *made = a
// new entry is made. Returns how many leave the chest.
static int StPlan(int chestCount, int unit, int max, int takeout, int stackCount, int full, int *join, int *rest, int *made)
{
    int want = takeout ? max : unit, j = 0, r;
    if (chestCount < want) want = chestCount;
    if (stackCount >= 0) {
        j = max - stackCount;
        if (j > want) j = want;
    }
    r = want - j;
    *made = 0;
    if (r == 0 && (chestCount != 0 || stackCount >= 0)) {
        *join = j;
        *rest = 0;
        return j;
    }
    if (full) r = 0;
    else *made = 1;
    *join = j;
    *rest = r;
    return j + r;
}

// ---------------------------------------------------------------- the switch-off move (v3.37, Refined's own)
// Extended Toolbelt off with items on page 2: the weapon in hand goes to page 1 (it must stay carried: off the belt it
// would read as broken / out of ammo), the Saint Medallion in use too while page 1 has room (else into the chest, taken
// off as the game's chest put-in does), everything else into the Storage Chest's free entries (0..125). Refused (nothing
// written) when the weapon in hand finds no room on page 1 or the chest cannot take the rest: the toolbelt stays on.
enum { INV_OFF_OK, INV_OFF_NOTHING, INV_OFF_CHEST_FULL, INV_OFF_HAND_NO_ROOM };
typedef struct {
    int why;          // INV_OFF_*
    int p2;           // items on page 2
    int toPage1;      // of them to page 1 (the weapon in hand, the medallion in use)
    int toChest;      // of them into the chest
    int chestUsed;    // used chest entries 0..125 before
    int chestFree;    // free chest entries 0..125 before
    int medOff;       // the medallion in use went into the chest: taken off
    int hand;         // the weapon in hand was on page 2 (only there)
} InvOffResult;
static int InvChestUsed(const InvBlock *b)
{
    int k, n = 0;
    for (k = 0; k < INV_CHEST_CAP; ++k) n += b->e[INV_P1 + k].id != 0;
    return n;
}
static int InvOffPlan(const InvBlock *b, int inHand, InvOffResult *r)
{
    int i, free1 = 10 - InvCnt10(b->e), med = -1;
    r->p2 = InvCnt10(InvP2c(b));
    r->toPage1 = r->toChest = r->medOff = r->hand = 0;
    r->chestUsed = InvChestUsed(b);
    r->chestFree = INV_CHEST_CAP - r->chestUsed;
    if (!r->p2) return r->why = INV_OFF_NOTHING;
    if (inHand && InvFind(b, inHand) >= INV_P2_FIRST) {  // only on page 2 (page 1 is searched first)
        r->hand = 1;
        if (free1 < 1) return r->why = INV_OFF_HAND_NO_ROOM;
        --free1;
        ++r->toPage1;
    }
    for (i = 0; i < 10 && med < 0; ++i)
        if (InvP2c(b)[i].id == INV_MEDALLION && InvP2c(b)[i].flag) med = i;
    if (med >= 0) {
        if (free1 >= 1) --free1, ++r->toPage1;
        else r->medOff = 1;
    }
    r->toChest = r->p2 - r->toPage1;
    if (r->toChest > r->chestFree) return r->why = INV_OFF_CHEST_FULL;
    return r->why = INV_OFF_OK;
}
// 4F5860's place for an id in a packed page 1 (after every entry of the same or a lower rank)
static int InvInsertPos(const InvBlock *b, int id, const unsigned char *rank)
{
    int i, pos = 0;
    const int r = InvRank(rank, id);
    for (i = 0; i < 10 && b->e[i].id; ++i)
        if (r >= InvRank(rank, b->e[i].id)) pos = i + 1;
    return i < 10 ? pos : -1;
}
static void InvPage1Insert(InvBlock *b, const InvEnt *e, const unsigned char *rank)
{
    const int pos = InvInsertPos(b, e->id, rank);
    int i;
    if (pos < 0) return;  // (InvOffPlan checked the room)
    for (i = 9; i > pos; --i) b->e[i] = b->e[i - 1];
    b->e[pos] = *e;
}
// Does what InvOffPlan planned (call it right after a plan with why == INV_OFF_OK). The chest count becomes the used
// entries of 0..125; the caller then runs the game's own sort / merge / recount (4F6610, stock again). Returns the
// items moved.
static int InvOffApply(InvBlock *b, int inHand, const unsigned char *rank, const InvEnt *tmpl, const InvOffResult *plan)
{
    InvEnt *p2 = InvP2(b);
    int i, k = 0, moved = 0, handDone = !plan->hand, medDone = 0;
    for (i = 0; i < 10; ++i) {
        InvEnt e = p2[i];
        int page1 = 0;
        if (!e.id) continue;
        if (!handDone && e.id == (unsigned)inHand) {
            page1 = handDone = 1;
        } else if (!medDone && e.id == INV_MEDALLION && e.flag) {
            medDone = 1;
            if (plan->medOff) e.flag = 0, b->medFlag = 0;  // into the chest: taken off (as the chest put-in 5121B0)
            else page1 = 1;
        }
        if (page1) {
            InvPage1Insert(b, &e, rank);
        } else {
            while (k < INV_CHEST_CAP && b->e[INV_P1 + k].id) ++k;
            if (k >= INV_CHEST_CAP) return moved;  // (cannot happen after the plan)
            b->e[INV_P1 + k] = e;
        }
        p2[i] = *tmpl;
        ++moved;
    }
    b->chestCount = (unsigned short)InvChestUsed(b);
    return moved;
}

// ---------------------------------------------------------------- bar slot records (Stackable Items' enable step)
// The C form of the table's [ENABLE] Lua: a used slot record (bit 0 of +A8) with a count above 1 gets bit 2 ("draw the
// count") at once; the game sets it only when it rebuilds the belt. rec = 10 records of B0h bytes (0x108ECD8).
static int StMarkCounts(unsigned char *rec)
{
    int i, n = 0;
    for (i = 0; i < 10; ++i) {
        unsigned char *f = rec + i * 0xB0 + 0xA8;
        const int count = *(const int *)(rec + i * 0xB0 + 0x88);
        if ((*f & 1) && count > 1 && !(*f & 4)) *f |= 4, ++n;
    }
    return n;
}

#endif
