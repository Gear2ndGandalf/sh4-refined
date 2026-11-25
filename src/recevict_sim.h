// v3.36 HE336: the pure part of the copies-first record eviction (recevict.inc; native test test\recevict_test.c).
// No game memory: the record table (64 x 0x50 bytes), the kill bits (32 words) and the +0x1B90 float are passed in.
// The rule is EnemyRecordAlloc 0x464060 (SH4Decomp src/enemy/en_463950.c 384-478, matched; re_kit/sh4.asm 0x464060-
// 0x46432C) with one change (copyFirst): when no slot is free, a record of an Enemy Copies copy (id 0x1E00nnnn, as
// enemies_sim.h EnmIsCopyId) is taken before any of the game's classes.
// Record fields read (include/enemy_spawn.h EnemyRecord): +0x20 id, +0x24 type (short), +0x26 state (short, 0 = free,
// 0x0B = downed), +0x30 kill index (short), +0x36 stage, +0x37 room (signed chars), +0x38 age (int).
#ifndef RECEVICT_SIM_H
#define RECEVICT_SIM_H
#include <string.h>

#define EV_REC_N 64
#define EV_REC_SIZE 0x50
#define EV_CLS_FREE 0
#define EV_CLS_RUNOFF 14    // stock: no class has a candidate (64 used, all of age 0): best_index[14] = best_age[0] = 0
#define EV_CLS_COPY 20      // ours: a copy's record, taken first

static int EvI32(const unsigned char *r, int off) { int v; memcpy(&v, r + off, 4); return v; }
static int EvI16(const unsigned char *r, int off) { short v; memcpy(&v, r + off, 2); return v; }
static int EvI8(const unsigned char *r, int off) { return (signed char)r[off]; }

static int EvIsCopyId(unsigned id) { return (id >> 24) == 0x1Eu && (id >> 16 & 0xFFu) == 0; }       // = EnmIsCopyId
static int EvIsGhost(int type) { return type >= 0x0B && type <= 0x0F; }                              // 0x463910
static int EvIsScript(unsigned id) { return (id >> 24 & 0x1Fu) > 0 && (id >> 16 & 0xFFu) > 0; }      // 0x52C8D0 (id & 0xFF >= 0)

// the stock class of a used record r (0x464060's scan): 13 any ghost; another stage: 1 not a script id, 6 downed, 2;
// another room / this room: 3 / 4 not a script id, 5 type 4, 7 type 2, 8 type 9, 11 / 12 downed, 9 / 10
static int EvClass(const unsigned char *r, int stage, int room)
{
    const int type = EvI16(r, 0x24), state = EvI16(r, 0x26);
    const unsigned id = (unsigned)EvI32(r, 0x20);
    if (EvIsGhost(type)) return 13;
    if (EvI8(r, 0x36) != stage) return !EvIsScript(id) ? 1 : state == 0x0B ? 6 : 2;
    if (EvI8(r, 0x37) != room) {
        if (!EvIsScript(id)) return 3;
        if (type == 4) return 5;
        if (type == 2) return 7;
        if (type == 9) return 8;
        return state == 0x0B ? 11 : 9;
    }
    if (!EvIsScript(id)) return 4;
    if (type == 4) return 5;
    if (type == 2) return 7;
    if (type == 9) return 8;
    return state == 0x0B ? 12 : 10;
}

// The scan and the choice (no clear yet): every used record's age + 1 (the stock `age = rec->age++`, the old value
// compared), then the slot: the first free one (class 0); else with copyFirst the copy's record of the largest old age
// (the first of equals; age 0 counts) (EV_CLS_COPY); else the lowest class with a candidate, inside it the largest old
// age (the first of equals; the stock `age > best_age[c]` from 0: age 0 never a candidate); none = slot 0
// (EV_CLS_RUNOFF, as the stock read past its table). *copies = the copies' records in use, *used = the records in use.
static int EvChoose(unsigned char *recs, int stage, int room, int copyFirst, int *cls, int *copies, int *used)
{
    int bestAge[14], bestIdx[14], i, c, freeIdx = -1, copyIdx = -1, copyAge = -1, nCopy = 0, nUsed = 0;
    for (c = 0; c < 14; ++c) bestAge[c] = 0, bestIdx[c] = -1;
    for (i = 0; i < EV_REC_N; ++i) {
        unsigned char *r = recs + i * EV_REC_SIZE;
        int age;
        if (EvI16(r, 0x26) == 0) {
            if (freeIdx < 0) freeIdx = i;
            continue;
        }
        ++nUsed;
        age = EvI32(r, 0x38);
        {
            const int up = age + 1;
            memcpy(r + 0x38, &up, 4);
        }
        c = EvClass(r, stage, room);
        if (age > bestAge[c]) bestAge[c] = age, bestIdx[c] = i;
        if (EvIsCopyId((unsigned)EvI32(r, 0x20))) {
            ++nCopy;
            if (age > copyAge) copyAge = age, copyIdx = i;
        }
    }
    if (copies) *copies = nCopy;
    if (used) *used = nUsed;
    if (freeIdx >= 0) {
        *cls = EV_CLS_FREE;
        return freeIdx;
    }
    if (copyFirst && copyIdx >= 0) {
        *cls = EV_CLS_COPY;
        return copyIdx;
    }
    for (c = 1; c < 14; ++c)
        if (bestIdx[c] >= 0) {
            *cls = c;
            return bestIdx[c];
        }
    *cls = EV_CLS_RUNOFF;
    return 0;
}

// What the eviction of record r of class cls does besides its clear (0x464060 after the choice): 1 = its kill bit is
// set (classes 6 / 11 / 12, not a type-5 record of stage 0x0B; class 8 when downed): the monster stays dead for good;
// 2 = +0x1B90 rises by 0.005 to 2.0 at most (class 8 not downed); 0 = nothing (free, a copy, every other class)
static int EvSideEffect(int cls, const unsigned char *r)
{
    if (cls == 6 || cls == 11 || cls == 12) return EvI16(r, 0x24) != 5 || EvI8(r, 0x36) != 0x0B ? 1 : 0;
    if (cls == 8) return EvI16(r, 0x26) == 0x0B ? 1 : 2;
    return 0;
}
// the kill bit of (stage, index) as 0x463A20 / 0x4639C0 (index <= 0: none); 0 = none or outside the 1024 bits (the
// stock writes it unbounded; no kill index of the game's data comes near)
static int EvKillBit(int stage, int index, int *word, unsigned *mask)
{
    int base = 0, bit;
    switch (stage) {
    case 2: base = 0x80; break;
    case 3: base = 0x100; break;
    case 4: base = 0x180; break;
    case 5: base = 0x300; break;
    case 6: base = 0x20; break;
    case 7: base = 0x3C0; break;
    case 8: base = 0x2C0; break;
    }
    if (index <= 0) return 0;
    bit = base + index;
    if (bit < 0 || bit >= 32 * 32) return 0;
    *word = bit >> 5;
    *mask = 1u << (bit & 31);
    return 1;
}
// The eviction proper: the side effect, the clear (0x42BB90 of 0x50 bytes), stage / room of now, age 0. Returns the
// side effect done (EvSideEffect; -1 = a kill bit outside the table, not written). The caller has read the record
// before (the log, the copies' rescue).
static int EvEvict(unsigned char *r, int cls, unsigned *killBits, float *f1B90, int stage, int room)
{
    int se = EvSideEffect(cls, r), w;
    unsigned m;
    if (se == 1) {
        if (EvKillBit(EvI8(r, 0x36), EvI16(r, 0x30), &w, &m)) killBits[w] |= m;
        else if (EvI16(r, 0x30) > 0) se = -1;
    } else if (se == 2) {
        const float v = *f1B90 + 0.005f;  // 0x5B747C (0x3BA3D70A); the cap 0x5B79D8 = 2.0
        *f1B90 = v > 2.0f ? 2.0f : v;
    }
    memset(r, 0, EV_REC_SIZE);
    r[0x36] = (unsigned char)(signed char)stage;
    r[0x37] = (unsigned char)(signed char)room;
    return se;
}

// a class's words for the log
static const char *EvClassName(int cls)
{
    static const char *const k[15] = {
        "a free slot", "another stage's (no placement id)", "another stage's (standing)", "another room's (no placement id)",
        "this room's (no placement id)", "a type-4 record", "another stage's (downed: dead for good)", "a type-2 record",
        "a type-9 record", "another room's (standing: back at full health)", "this room's (standing: back at full health)",
        "another room's (downed: dead for good)", "this room's (downed: dead for good)", "a ghost's (its down / pin state lost)",
        "slot 0 (no candidate at all)" };
    if (cls == EV_CLS_COPY) return "a copy's (Refined keeps its state)";
    return cls >= 0 && cls <= 14 ? k[cls] : "?";
}
#endif
