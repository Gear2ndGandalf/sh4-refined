// v3.50 EF350: native test of src\pursuit_sim.h (test\build_pursuit_test.cmd): Enemy Pursuit's values and lose-track rules,
// the per-room-visit memory, the followers' side file (blob pack / unpack, the file walk, damage, the store's slot choice).
#include <stdio.h>
#include <stdlib.h>
#include "../src/pursuit_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestValues(void)
{
    CHECK(EfPursuitParse("Relentless") == EF_PURSUIT_RELENTLESS, "Relentless");
    CHECK(EfPursuitParse("normal") == EF_PURSUIT_NORMAL, "normal (any case)");
    CHECK(EfPursuitParse("FORGETFUL") == EF_PURSUIT_FORGETFUL, "FORGETFUL");
    CHECK(EfPursuitParse("") == EF_PURSUIT_NORMAL, "empty = Normal");
    CHECK(EfPursuitParse(NULL) == EF_PURSUIT_NORMAL, "NULL = Normal");
    CHECK(EfPursuitParse("On") == EF_PURSUIT_NORMAL, "unknown = Normal");
    CHECK(EfPursuitParse("Relentles") == EF_PURSUIT_NORMAL, "a prefix is unknown");
    CHECK(EF_PURSUIT_DEFAULT == EF_PURSUIT_NORMAL, "default Normal");
    CHECK(!EfLoses(EF_PURSUIT_RELENTLESS) && EfLoses(EF_PURSUIT_NORMAL) && EfLoses(EF_PURSUIT_FORGETFUL), "who loses");
    CHECK(!EfLoses(-1) && !EfLoses(EF_PURSUIT_N), "out of range: no rule");
    CHECK(EfMemorySec(EF_PURSUIT_NORMAL) == 10.0f && EfMemorySec(EF_PURSUIT_FORGETFUL) == 4.0f, "memory 10 / 4 s");
    CHECK(EfMemorySec(EF_PURSUIT_RELENTLESS) < 0.0f, "Relentless: forever");
    // doors away: Relentless = v3.38's FollowKeepDoors rule (0 -> 1, -1 Always, n), Normal 2, Forgetful 1
    CHECK(EfMaxMisses(EF_PURSUIT_RELENTLESS, -1) == -1, "Relentless Always");
    CHECK(EfMaxMisses(EF_PURSUIT_RELENTLESS, 0) == 1, "Relentless 0 -> 1");
    CHECK(EfMaxMisses(EF_PURSUIT_RELENTLESS, 5) == 5, "Relentless 5");
    CHECK(EfMaxMisses(EF_PURSUIT_NORMAL, -1) == 2 && EfMaxMisses(EF_PURSUIT_NORMAL, 7) == 2, "Normal 2 whatever the ini");
    CHECK(EfMaxMisses(EF_PURSUIT_FORGETFUL, -1) == 1, "Forgetful 1");
}

static void TestDoorRules(void)
{
    int m;
    for (m = 0; m < EF_PURSUIT_N; ++m) {
        const float mem = EfMemorySec(m);
        CHECK(!EfLostAtDoor(m, 1, -1.0f) && !EfLostAtDoor(m, 1, 1000.0f), "ghosts never lose by memory (%d)", m);
        CHECK(!EfLostAtDoor(m, 0, 0.0f), "sensed now: follows (%d)", m);
        if (m == EF_PURSUIT_RELENTLESS) {
            CHECK(!EfLostAtDoor(m, 0, -1.0f) && !EfLostAtDoor(m, 0, 1.0e6f), "Relentless never loses");
            CHECK(!EfDownLoses(m, 1, 0), "Relentless: down keeps it");
            continue;
        }
        CHECK(!EfLostAtDoor(m, 0, mem), "at the memory's edge it still follows (%d)", m);
        CHECK(EfLostAtDoor(m, 0, mem + 0.01f), "past the memory it lost you (%d)", m);
        CHECK(EfLostAtDoor(m, 0, -1.0f), "never sensed in this visit: lost (%d)", m);
        CHECK(EfDownLoses(m, 1, 0), "down in the room you left: lost (%d)", m);
        CHECK(!EfDownLoses(m, 1, 1), "standing: kept (%d)", m);
        CHECK(!EfDownLoses(m, 0, 0), "down elsewhere: the door count decides (%d)", m);
    }
}

static void TestTrack(void)
{
    EfTrack t;
    int hit, i;
    float a;
    EfTrackReset(&t, 7);
    CHECK(t.serial == 7 && t.n == 0, "reset");
    a = EfTrackSee(&t, 0x02020001u, 1.0f, 0, 100.0f, 0, &hit);
    CHECK(a == -1.0f && !hit, "a new one that did not sense you: -1");
    a = EfTrackSee(&t, 0x02020001u, 2.0f, 0, 100.0f, 0, &hit);
    CHECK(a == -1.0f && !hit, "still unknown");
    a = EfTrackSee(&t, 0x02020001u, 3.0f, 1, 100.0f, 0, &hit);
    CHECK(a == 0.0f, "sensed: 0");
    a = EfTrackSee(&t, 0x02020001u, 9.5f, 0, 100.0f, 0, &hit);
    CHECK(a == 6.5f, "6.5 s later (%.2f)", a);
    a = EfTrackSee(&t, 0x02020001u, 20.0f, 0, 80.0f, 0, &hit);
    CHECK(a == 0.0f && hit, "a hit (HP 100 -> 80) is sensing you");
    a = EfTrackSee(&t, 0x02020001u, 25.0f, 0, 90.0f, 0, &hit);
    CHECK(a == 5.0f && !hit, "HP up (regen) is no hit");
    a = EfTrackSee(&t, 0x02020001u, 26.0f, 0, -1.0f, 0, &hit);
    CHECK(a == 6.0f && !hit, "unknown HP: no hit, the HP kept");
    a = EfTrackSee(&t, 0x02020001u, 27.0f, 0, 85.0f, 0, &hit);
    CHECK(a == 0.0f && hit, "a hit after an unknown look");
    // a pursuer that came after you: the memory's time from its first look
    a = EfTrackSee(&t, 0x02040001u, 30.0f, 0, 50.0f, 1, &hit);
    CHECK(a == 0.0f, "a pursuer's first look: 0");
    a = EfTrackSee(&t, 0x02040001u, 41.0f, 0, 50.0f, 1, &hit);
    CHECK(a == 11.0f && EfLostAtDoor(EF_PURSUIT_NORMAL, 0, a), "11 s without sensing you: Normal loses it");
    // the clock going back (a reset): never negative
    a = EfTrackSee(&t, 0x02040001u, 5.0f, 0, 50.0f, 1, &hit);
    CHECK(a == 0.0f, "clock back: 0");
    // the table full: -2, counted as sensing
    EfTrackReset(&t, 8);
    for (i = 0; i < EF_TRACK_N; ++i) EfTrackSee(&t, 0x1000u + (unsigned)i, 1.0f, 0, -1.0f, 0, NULL);
    a = EfTrackSee(&t, 0x9999u, 1.0f, 0, -1.0f, 0, &hit);
    CHECK(a == -2.0f && t.n == EF_TRACK_N, "full: -2");
    CHECK(!EfLostAtDoor(EF_PURSUIT_NORMAL, 0, a < -1.5f ? 0.0f : a), "the caller treats -2 as sensed");
}

static void Fill(EfState *s, int nPur, int nQ, int nTag, int nCw)
{
    int i, k;
    memset(s, 0, sizeof *s);
    s->key.crc = 0xDEADBEEFu, s->key.time = 44565, s->key.stage = 2, s->key.room = 2;
    s->nPur = nPur, s->nQ = nQ, s->nTag = nTag, s->nCw = nCw;
    s->doorSet = nQ ? 1 : 0;
    s->door[0] = -1000.0f, s->door[1] = 900.0f, s->door[2] = -4400.0f;
    for (i = 0; i < nPur; ++i) {
        s->pur[i].id = 0x02040001u + (unsigned)i, s->pur[i].type = 0x0A, s->pur[i].variant = (unsigned char)(i & 1);
        s->pur[i].flags = (unsigned char)(i % 16), s->pur[i].misses = (unsigned char)(i % 3);
    }
    for (i = 0; i < nQ; ++i) {
        s->q[i].id = 0x02040001u + (unsigned)i, s->q[i].type = 0x0A, s->q[i].fromRoom = 4, s->q[i].age = 1.5f * (float)i;
        for (k = 0; k < EF_REC; ++k) s->q[i].rec[k] = (unsigned char)(i + k);
    }
    for (i = 0; i < nTag; ++i) s->tag[i] = 0xFFFF0007u + (unsigned)i;
    for (i = 0; i < nCw; ++i) {
        s->cw[i].key = 0x00050000u + (unsigned)i, s->cw[i].cid = 0x1E000000u + (unsigned)i, s->cw[i].room = (signed char)(i % 40);
        s->cw[i].type = 0x0A, s->cw[i].age = (float)i;
        for (k = 0; k < EF_REC; ++k) s->cw[i].rec[k] = (unsigned char)(3 * i + k);
    }
}

static void TestBlob(void)
{
    static EfState a, b;
    static unsigned char buf[EF_BLOB_MAX + 16];
    int n, m;
    Fill(&a, 0, 0, 0, 0);
    CHECK(EfStateEmpty(&a), "empty");
    n = EfPack(&a, buf, sizeof buf);
    CHECK(n == EF_BLOB_HEAD && EfUnpack(buf, n, &b) && EfStateEmpty(&b) && EfKeySame(&a.key, &b.key), "empty round trip");
    Fill(&a, 3, 1, 2, 4);
    CHECK(!EfStateEmpty(&a), "not empty");
    n = EfPack(&a, buf, sizeof buf);
    CHECK(n == EfBlobSize(&a) && n > EF_BLOB_HEAD, "packed %d", n);
    CHECK(EfUnpack(buf, n, &b), "unpacks");
    CHECK(!memcmp(&a, &b, sizeof a), "round trip exact");
    CHECK(!EfUnpack(buf, n - 1, &b) && EfStateEmpty(&b), "one byte short: refused, zeroed");
    CHECK(!EfUnpack(buf, n + 1, &b), "one byte long: refused");
    buf[0] = 'X';
    CHECK(!EfUnpack(buf, n, &b), "bad magic: refused");
    buf[0] = 'E';
    {   // a count out of range
        int bad = EF_PUR_N + 1;
        memcpy(buf + 4 + sizeof(EfKey), &bad, 4);
        CHECK(!EfUnpack(buf, n, &b), "count out of range: refused");
        bad = -1;
        memcpy(buf + 4 + sizeof(EfKey), &bad, 4);
        CHECK(!EfUnpack(buf, n, &b), "negative count: refused");
    }
    Fill(&a, EF_PUR_N, EF_Q_N, EF_TAG_N, EF_CW_N);
    n = EfPack(&a, buf, sizeof buf);
    CHECK(n == EF_BLOB_MAX, "the largest blob %d = EF_BLOB_MAX %d", n, EF_BLOB_MAX);
    CHECK(EfUnpack(buf, n, &b) && !memcmp(&a, &b, sizeof a), "the largest round trip");
    CHECK(EfPack(&a, buf, n - 1) == 0, "does not fit: 0");
    a.nPur = EF_PUR_N + 1;
    CHECK(EfPack(&a, buf, sizeof buf) == 0, "bad count: not packed");
    a.nPur = 1, a.doorSet = 2;
    CHECK(EfPack(&a, buf, sizeof buf) == 0, "bad door flag: not packed");
    m = (int)sizeof(EfPurS) + (int)sizeof(EfQS) + (int)sizeof(EfCwS);
    CHECK(sizeof(EfKey) == 16 && sizeof(EfPurS) == 8 && sizeof(EfQS) == 0x5C && sizeof(EfCwS) == 0x60, "packed sizes (%d)", m);
    CHECK(EF_BLOB_MAX < 32768, "an entry stays under 32 KB (%d)", EF_BLOB_MAX);
}

static void TestFile(void)
{
    static EfState s[3], r;
    static unsigned char blob[3][EF_BLOB_MAX], file[3 * (EF_BLOB_MAX + EF_ENTRY_HEAD) + EF_FILE_HEAD];
    int n[3], i, at, pos, count, len, got;
    const unsigned char *b;
    unsigned stamp;
    for (i = 0; i < 3; ++i) {
        Fill(&s[i], i + 1, i, i, 2 * i);
        s[i].key.crc ^= (unsigned)i;
        n[i] = EfPack(&s[i], blob[i], EF_BLOB_MAX);
    }
    at = EfFilePutHead(file, 3);
    for (i = 0; i < 3; ++i) at += EfFilePutEntry(file + at, blob[i], n[i], 10u + (unsigned)i);
    CHECK(EfFileHeadOk(file, at, &count) && count == 3, "head");
    pos = EF_FILE_HEAD, got = 0;
    while (EfFileNext(file, at, &pos, &b, &len, &stamp) == 1) {
        CHECK(len == n[got] && stamp == 10u + (unsigned)got && EfUnpack(b, len, &r) && !memcmp(&r, &s[got], sizeof r),
              "entry %d", got);
        ++got;
    }
    CHECK(got == 3 && pos == at, "three entries, to the end");
    CHECK(EfFileNext(file, at, &pos, &b, &len, &stamp) == 0, "the end: 0");
    // damage: a flipped byte in the second blob
    file[EF_FILE_HEAD + EF_ENTRY_HEAD + n[0] + EF_ENTRY_HEAD + 5] ^= 0x40;
    pos = EF_FILE_HEAD;
    CHECK(EfFileNext(file, at, &pos, &b, &len, &stamp) == 1, "the first still reads");
    CHECK(EfFileNext(file, at, &pos, &b, &len, &stamp) == -1, "the second is damaged: -1");
    file[EF_FILE_HEAD + EF_ENTRY_HEAD + n[0] + EF_ENTRY_HEAD + 5] ^= 0x40;
    // a cut file
    pos = EF_FILE_HEAD;
    CHECK(EfFileNext(file, at - 3, &pos, &b, &len, &stamp) == 1 && EfFileNext(file, at - 3, &pos, &b, &len, &stamp) == 1 &&
          EfFileNext(file, at - 3, &pos, &b, &len, &stamp) == -1, "a cut last entry: -1");
    CHECK(!EfFileHeadOk(file, 8, &count), "too short");
    file[0] = 'x';
    CHECK(!EfFileHeadOk(file, at, &count), "bad magic");
    file[0] = 'S';
    count = EF_STORE_N + 1;
    memcpy(file + 8, &count, 4);
    CHECK(!EfFileHeadOk(file, at, &count), "too many entries");
    {   // a huge length field
        int huge = 0x7FFFFFFF;
        memcpy(file + EF_FILE_HEAD, &huge, 4);
        pos = EF_FILE_HEAD;
        CHECK(EfFileNext(file, at, &pos, &b, &len, &stamp) == -1, "a huge length: -1");
    }
    CHECK(EfCrc32("123456789", 9, 0) == 0xCBF43926u, "CRC-32 check value");
}

static void TestStoreSlot(void)
{
    EfKey keys[4], k;
    unsigned stamps[4] = { 5, 2, 9, 7 };
    int used[4] = { 1, 1, 1, 1 }, i;
    for (i = 0; i < 4; ++i) keys[i].crc = 100u + (unsigned)i, keys[i].time = 1, keys[i].stage = 2, keys[i].room = 3;
    k = keys[2];
    CHECK(EfStoreSlot(keys, stamps, used, 4, &k) == 2, "the same key: its slot");
    k.crc = 999;
    CHECK(EfStoreSlot(keys, stamps, used, 4, &k) == 1, "full: the oldest (stamp 2)");
    used[3] = 0;
    CHECK(EfStoreSlot(keys, stamps, used, 4, &k) == 3, "a free slot first");
    k = keys[0], k.time = 2;
    CHECK(EfStoreSlot(keys, stamps, used, 4, &k) == 3, "another game time is another key");
    k = keys[3];
    CHECK(EfStoreSlot(keys, stamps, used, 4, &k) == 3, "a free slot's old key does not count");
}

int main(void)
{
    TestValues();
    TestDoorRules();
    TestTrack();
    TestBlob();
    TestFile();
    TestStoreSlot();
    printf("pursuit_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
