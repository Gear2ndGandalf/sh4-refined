// v3.50 EF350 (staging/v350_feedback/w5_follow/PLAN.md; the user's v3.49 QA C4, C5): the pure parts of Enemy Pursuit (enemies
// lose track of you) and of the followers' side file (Refined's follower state kept per save, next to the game's saves).
// Included by enemies.inc after enemies_sim.h; native test: test/pursuit_sim_test.c (test/build_pursuit_test.cmd).
#ifndef PURSUIT_SIM_H
#define PURSUIT_SIM_H
#include <string.h>

// ---------------------------------------------------------------- Enemy Pursuit (Option 5 > Enemies, [Gameplay] EnemyPursuit)
// Relentless = v3.38 - v3.49: a monster that spotted you in a room visit follows through your next door however far away
// it is then, and every pursuer pursues for the whole world visit (FollowKeepDoors, default Always).
// Normal / Forgetful: a monster (or a monster copy) "senses" you while it is near you (FollowRange, the vertical rule),
// its EnemySense record sees you, or it is hit. It follows through a door only when it sensed you within the last
// EF_MEMORY_NORMAL (10) / EF_MEMORY_FORGETFUL (4) seconds of play before you went through; a pursuer that had not
// loses track there. A pursuer (monster or ghost) that lies down in the room when you leave loses track (you downed it
// and ran). A pursuer away (not in the room you left: on its way, or left behind) loses track after 2 / 1 door(s)
// without catching up. One that lost track stays where it is and lives by the game's own AI (it can spot you again).
// The ghosts sense you through walls (every ghost in the room pursues, the user's v3.38 answer): no memory rule for them.
enum { EF_PURSUIT_RELENTLESS, EF_PURSUIT_NORMAL, EF_PURSUIT_FORGETFUL, EF_PURSUIT_N };
#define EF_PURSUIT_DEFAULT EF_PURSUIT_NORMAL
#define EF_MEMORY_NORMAL 10.0f
#define EF_MEMORY_FORGETFUL 4.0f
static const char *const kEfPursuitName[EF_PURSUIT_N] = { "Relentless", "Normal", "Forgetful" };
static int EfStrIeq(const char *a, const char *b)
{
    for (;; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
        if (!x) return 1;
    }
}
// the ini text: a value's name (any case), else the default (Normal; also a missing or empty key)
static int EfPursuitParse(const char *s)
{
    int v;
    if (!s) return EF_PURSUIT_DEFAULT;
    for (v = 0; v < EF_PURSUIT_N; ++v)
        if (EfStrIeq(s, kEfPursuitName[v])) return v;
    return EF_PURSUIT_DEFAULT;
}
static int EfPursuitOk(int m) { return m >= 0 && m < EF_PURSUIT_N; }
static const char *EfPursuitNameOf(int m) { return EfPursuitOk(m) ? kEfPursuitName[m] : "?"; }
static int EfLoses(int m) { return EfPursuitOk(m) && m != EF_PURSUIT_RELENTLESS; }  // the lose-track rules apply
// seconds a monster keeps knowing where you are once it last sensed you (< 0: forever)
static float EfMemorySec(int m)
{
    return m == EF_PURSUIT_NORMAL ? EF_MEMORY_NORMAL : m == EF_PURSUIT_FORGETFUL ? EF_MEMORY_FORGETFUL : -1.0f;
}
// the doors away a pursuer may miss (EnmPursueAtDoor2's maxMisses: <= 0 = never lost by doors). Relentless keeps
// FollowKeepDoors (iniKeep: -1 Always, 0 = lost at the first door away, 1..32), as v3.38 - v3.49 did.
static int EfMaxMisses(int m, int iniKeep)
{
    if (m == EF_PURSUIT_NORMAL) return 2;
    if (m == EF_PURSUIT_FORGETFUL) return 1;
    return iniKeep == 0 ? 1 : iniKeep;
}
// at a door, one of the room you left that would follow: 1 = it lost track of you (ago = seconds since it last sensed
// you, < 0 = not in this room visit); ghosts never by this rule
static int EfLostAtDoor(int m, int ghost, float ago)
{
    if (!EfLoses(m) || ghost) return 0;
    return ago < 0.0f || ago > EfMemorySec(m);
}
// at a door, a pursuer that was in the room you left (inLeftRoom) and lay down there (!standing): 1 = it lost track
static int EfDownLoses(int m, int inLeftRoom, int standing) { return EfLoses(m) && inLeftRoom && !standing; }

// the per-room-visit memory: when each enemy last sensed you (the game's play seconds: enemies.inc g_enmClock) and its
// last HP (a drop = a hit)
#define EF_TRACK_N 96  // = the enemy list walk's guard
typedef struct { unsigned id; float last, hp; int known; } EfTrackE;
typedef struct { int serial, n; EfTrackE e[EF_TRACK_N]; } EfTrack;
static void EfTrackReset(EfTrack *t, int serial) { t->serial = serial, t->n = 0; }
// one tick's look at an enemy: sensed = near you or its sense saw you; hp < 0 = unknown; pursuer = it pursues you already
// (it gets the memory's time from its first look in this room visit: it came after you). Returns the seconds since it
// last sensed you (0 = now); -1 = it has not sensed you in this room visit (and pursues nobody); -2 = the table is full
// (counted as sensing you: nobody loses track for the table). *hit = this look saw a hit.
static float EfTrackSee(EfTrack *t, unsigned id, float now, int sensed, float hp, int pursuer, int *hit)
{
    EfTrackE *e = 0;
    int i, h = 0;
    for (i = 0; i < t->n; ++i)
        if (t->e[i].id == id) {
            e = &t->e[i];
            break;
        }
    if (hit) *hit = 0;
    if (!e) {
        if (t->n >= EF_TRACK_N) return -2.0f;
        e = &t->e[t->n++];
        e->id = id, e->known = 0, e->last = now, e->hp = hp >= 0.0f && hp < 1.0e9f ? hp : -1.0f;
        if (sensed || pursuer) e->known = 1;
        return e->known ? 0.0f : -1.0f;
    }
    if (hp >= 0.0f && hp < 1.0e9f) {
        if (e->hp >= 0.0f && hp < e->hp - 0.01f) h = 1;
        e->hp = hp;
    }
    if (hit) *hit = h;
    if (sensed || h) e->known = 1, e->last = now;
    if (!e->known) return -1.0f;
    return now >= e->last ? now - e->last : 0.0f;
}

// ---------------------------------------------------------------- the followers' side file
// The game's save holds the enemy records (a follower in the room you saved in is recorded there; one left in another room
// is recorded there), not Refined's own state: who pursues you, who is on the way through the door you came in by, which
// ghosts are let into this room, the copies that followed you (their rooms and records). EnemyManagerSave's copy of the
// records (0x1B90 bytes, after the Randomizer's save filter) is the key: its CRC-32 with the slot's game time, stage and
// room, read again from the image at the load. A load whose image has no entry = v3.49 (nothing restored).
#define EF_PUR_N 256  // = ENM_PURSUERS_N
#define EF_Q_N 32     // = ENM_FOLLOW_MAX
#define EF_TAG_N 32   // = ENM_FOLLOW_MAX
#define EF_CW_N 128   // = ENM_CW_N
#define EF_REC 0x50   // = ENM_REC_SIZE
#define EF_STORE_N 64 // entries kept (the oldest goes)
enum { EFP_COPY = 1, EFP_GHOST = 2, EFP_HERE = 4, EFP_STANDING = 8 };
#pragma pack(push, 1)
typedef struct { unsigned crc, time; int stage, room; } EfKey;
typedef struct { unsigned id; unsigned char type, variant, flags, misses; } EfPurS;                 // a pursuer
typedef struct { unsigned id; unsigned char type, variant, flags; signed char fromRoom; float age;
                 unsigned char rec[EF_REC]; } EfQS;                                                 // one on its way here
typedef struct { unsigned key, cid; signed char room; unsigned char type, variant, pad; float age;
                 unsigned char rec[EF_REC]; } EfCwS;                                                // a copy that followed
#pragma pack(pop)
typedef struct {
    EfKey key;
    int nPur, nQ, nTag, nCw, doorSet;
    float door[3];
    EfPurS pur[EF_PUR_N];
    EfQS q[EF_Q_N];
    unsigned tag[EF_TAG_N];
    EfCwS cw[EF_CW_N];
} EfState;
static int EfStateEmpty(const EfState *s) { return !s->nPur && !s->nQ && !s->nTag && !s->nCw; }
static int EfKeySame(const EfKey *a, const EfKey *b)
{
    return a->crc == b->crc && a->time == b->time && a->stage == b->stage && a->room == b->room;
}
static unsigned EfCrc32(const void *p, unsigned n, unsigned crc)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned i;
    int k;
    crc = ~crc;
    for (i = 0; i < n; ++i) {
        crc ^= b[i];
        for (k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
// the blob of one entry: "EFS1", the key, the five counts, the door spot, then the arrays (counts entries each)
#define EF_BLOB_HEAD (4 + (int)sizeof(EfKey) + 5 * 4 + 12)
#define EF_BLOB_MAX (EF_BLOB_HEAD + EF_PUR_N * (int)sizeof(EfPurS) + EF_Q_N * (int)sizeof(EfQS) + EF_TAG_N * 4 + \
                     EF_CW_N * (int)sizeof(EfCwS))
static int EfBlobSize(const EfState *s)
{
    return EF_BLOB_HEAD + s->nPur * (int)sizeof(EfPurS) + s->nQ * (int)sizeof(EfQS) + s->nTag * 4 + s->nCw * (int)sizeof(EfCwS);
}
static int EfCountsOk(const EfState *s)
{
    return s->nPur >= 0 && s->nPur <= EF_PUR_N && s->nQ >= 0 && s->nQ <= EF_Q_N && s->nTag >= 0 && s->nTag <= EF_TAG_N &&
           s->nCw >= 0 && s->nCw <= EF_CW_N && (s->doorSet == 0 || s->doorSet == 1);
}
// returns the bytes written, 0 = it does not fit (or the counts are out of range)
static int EfPack(const EfState *s, unsigned char *out, int cap)
{
    int n, at = 0;
    if (!EfCountsOk(s)) return 0;
    n = EfBlobSize(s);
    if (n > cap) return 0;
    memcpy(out + at, "EFS1", 4), at += 4;
    memcpy(out + at, &s->key, sizeof s->key), at += (int)sizeof s->key;
    memcpy(out + at, &s->nPur, 4), at += 4;
    memcpy(out + at, &s->nQ, 4), at += 4;
    memcpy(out + at, &s->nTag, 4), at += 4;
    memcpy(out + at, &s->nCw, 4), at += 4;
    memcpy(out + at, &s->doorSet, 4), at += 4;
    memcpy(out + at, s->door, 12), at += 12;
    memcpy(out + at, s->pur, s->nPur * sizeof(EfPurS)), at += s->nPur * (int)sizeof(EfPurS);
    memcpy(out + at, s->q, s->nQ * sizeof(EfQS)), at += s->nQ * (int)sizeof(EfQS);
    memcpy(out + at, s->tag, s->nTag * 4), at += s->nTag * 4;
    memcpy(out + at, s->cw, s->nCw * sizeof(EfCwS)), at += s->nCw * (int)sizeof(EfCwS);
    return at == n ? n : 0;
}
// 1 = a whole, consistent blob (its counts in range, its size exact); s is left zeroed otherwise
static int EfUnpack(const unsigned char *in, int n, EfState *s)
{
    int at = 0;
    memset(s, 0, sizeof *s);
    if (!in || n < EF_BLOB_HEAD || n > EF_BLOB_MAX || memcmp(in, "EFS1", 4)) return 0;
    at = 4;
    memcpy(&s->key, in + at, sizeof s->key), at += (int)sizeof s->key;
    memcpy(&s->nPur, in + at, 4), at += 4;
    memcpy(&s->nQ, in + at, 4), at += 4;
    memcpy(&s->nTag, in + at, 4), at += 4;
    memcpy(&s->nCw, in + at, 4), at += 4;
    memcpy(&s->doorSet, in + at, 4), at += 4;
    memcpy(s->door, in + at, 12), at += 12;
    if (!EfCountsOk(s) || EfBlobSize(s) != n) {
        memset(s, 0, sizeof *s);
        return 0;
    }
    memcpy(s->pur, in + at, s->nPur * sizeof(EfPurS)), at += s->nPur * (int)sizeof(EfPurS);
    memcpy(s->q, in + at, s->nQ * sizeof(EfQS)), at += s->nQ * (int)sizeof(EfQS);
    memcpy(s->tag, in + at, s->nTag * 4), at += s->nTag * 4;
    memcpy(s->cw, in + at, s->nCw * sizeof(EfCwS));
    return 1;
}
// the file: "SH4RFOL1", the entry count, then each entry: its size, its stamp (the newest is the largest), the blob's
// CRC-32, the blob. EfFileNext walks it: 1 = the next whole entry (*blob, *n, *stamp), 0 = the end, -1 = damaged.
#define EF_FILE_MAGIC "SH4RFOL1"
#define EF_FILE_HEAD 12
#define EF_ENTRY_HEAD 12
static int EfFileHeadOk(const unsigned char *buf, int size, int *count)
{
    int c;
    if (!buf || size < EF_FILE_HEAD || memcmp(buf, EF_FILE_MAGIC, 8)) return 0;
    memcpy(&c, buf + 8, 4);
    if (c < 0 || c > EF_STORE_N) return 0;
    *count = c;
    return 1;
}
static int EfFileNext(const unsigned char *buf, int size, int *pos, const unsigned char **blob, int *n, unsigned *stamp)
{
    int len;
    unsigned crc;
    if (*pos == size) return 0;
    if (*pos < EF_FILE_HEAD || *pos > size || size - *pos < EF_ENTRY_HEAD) return -1;
    memcpy(&len, buf + *pos, 4);
    memcpy(stamp, buf + *pos + 4, 4);
    memcpy(&crc, buf + *pos + 8, 4);
    if (len < EF_BLOB_HEAD || len > EF_BLOB_MAX || len > size - *pos - EF_ENTRY_HEAD) return -1;
    if (EfCrc32(buf + *pos + EF_ENTRY_HEAD, (unsigned)len, 0) != crc) return -1;
    *blob = buf + *pos + EF_ENTRY_HEAD;
    *n = len;
    *pos += EF_ENTRY_HEAD + len;
    return 1;
}
static int EfFilePutHead(unsigned char *out, int count)
{
    memcpy(out, EF_FILE_MAGIC, 8);
    memcpy(out + 8, &count, 4);
    return EF_FILE_HEAD;
}
static int EfFilePutEntry(unsigned char *out, const unsigned char *blob, int n, unsigned stamp)
{
    const unsigned crc = EfCrc32(blob, (unsigned)n, 0);
    memcpy(out, &n, 4);
    memcpy(out + 4, &stamp, 4);
    memcpy(out + 8, &crc, 4);
    memcpy(out + EF_ENTRY_HEAD, blob, n);
    return EF_ENTRY_HEAD + n;
}
// the slot for an entry of key k among n slots: the one with the same key, else a free one, else the oldest stamp
static int EfStoreSlot(const EfKey *keys, const unsigned *stamps, const int *used, int n, const EfKey *k)
{
    int i, b = -1;
    for (i = 0; i < n; ++i)
        if (used[i] && EfKeySame(&keys[i], k)) return i;
    for (i = 0; i < n; ++i)
        if (!used[i]) return i;
    for (i = 0; i < n; ++i)
        if (b < 0 || stamps[i] < stamps[b]) b = i;
    return b;
}
#endif
