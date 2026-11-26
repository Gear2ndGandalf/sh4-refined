// ================================================================ v3.50 FI350: Fade In, the pure rules (no game memory)
// Plan: staging/v350_feedback/w4_fadein/PLAN.md (USER_FEEDBACK_v349 C2 / C3). Included by fadein.inc; native test:
// test/fadein_sim_test.c (test/build_fadein_test.cmd). Everything here is plain C on values the caller read.
//
// Fade In (Option 5 > Enemies, [Gameplay] FadeIn = Off / Fade / Fade + Portal): a monster or a victim that appears in a
// room is drawn see-through for FadeInSeconds (0.8 s) by the game's own see-through enemy draw (0x45C880, the one its
// ghosts and enemy echoes use), from invisible to full; with Fade + Portal a ghost that comes in after the room's first
// moments (not one standing there at the room load) also gets a blood-red vortex of Refined's own behind it.
#ifndef FADEIN_SIM_H
#define FADEIN_SIM_H
#include <string.h>
#include <stdio.h>

enum { FI_MODE_OFF, FI_MODE_FADE, FI_MODE_PORTAL, FI_MODE_N };
static const char *const kFiModeName[FI_MODE_N] = { "Off", "Fade", "Fade + Portal" };
#define FI_MODE_DEFAULT FI_MODE_PORTAL
#define FI_SECONDS_DEFAULT 0.8f   // [Gameplay] FadeInSeconds (ini only) 0.2 .. 3.0
#define FI_SECONDS_MIN 0.2f
#define FI_SECONDS_MAX 3.0f
#define FI_GRACE_S 1.5f           // first seen within this long after the room's first frame = "room load"
#define FI_PORTAL_OPEN_S 0.35f    // the vortex opens (eased) ...
#define FI_PORTAL_CLOSE_S 0.6f    // ... stays open while the body fades in, then closes
#define FI_MAX 256                // enemies remembered per room visit (v3.50 FI350-001: was 64; the task pool is 400 in all)
#define FI_STALE_S 2.0            // an entry not drawn this long may make room for a new enemy (FiAdd)
#define FI_PORTAL_MAX 8           // vortices at once
#define FI_PORTAL_NEAR 400.0f     // a game portal this near a ghost's spot at its first sight = its own entrance

static int FiLower(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
static int FiSameNoSpace(const char *a, const char *b)  // case and spaces ignored ("Fade+Portal" = "Fade + Portal")
{
    for (;;) {
        while (*a == ' ' || *a == '\t') ++a;
        while (*b == ' ' || *b == '\t') ++b;
        if (!*a || !*b) return !*a && !*b;
        if (FiLower((unsigned char)*a) != FiLower((unsigned char)*b)) return 0;
        ++a, ++b;
    }
}
// the ini text -> a value; *known = 0 for a text that is none of them (the default is used)
static int FiModeParse(const char *s, int *known)
{
    int v;
    if (known) *known = 1;
    if (!s || !*s) {
        if (known) *known = 0;
        return FI_MODE_DEFAULT;
    }
    for (v = 0; v < FI_MODE_N; ++v)
        if (FiSameNoSpace(s, kFiModeName[v])) return v;
    if (FiSameNoSpace(s, "0")) return FI_MODE_OFF;
    if (FiSameNoSpace(s, "On") || FiSameNoSpace(s, "1") || FiSameNoSpace(s, "Portal")) return FI_MODE_PORTAL;
    if (known) *known = 0;
    return FI_MODE_DEFAULT;
}
static float FiSecondsClamp(float v)  // NaN / missing -> the default
{
    if (!(v == v) || v <= 0.0f) return FI_SECONDS_DEFAULT;
    return v < FI_SECONDS_MIN ? FI_SECONDS_MIN : v > FI_SECONDS_MAX ? FI_SECONDS_MAX : v;
}

// 0x45C880 as the stock exe holds it (SH4Decomp enemy_common.c func_0045C880, matched; 214 bytes, calls ModelDraw 0x55C650,
// RenderSetPixelShader 0x404B80, 0x429ED0 / 0x429F00 (GS ALPHA / TEST), 0x429EF0 / 0x429F20 (back)): fadein.inc calls it
// only when the running exe has exactly these bytes (the native test checks them against SH4Decomp orig\sh4.exe)
#define FI_SEE_VA 0x45C880u
static const unsigned char kFiSeeThrough[214] = {  // 0x45C880 (enemy_common.c func_0045C880, matched), the whole function
    0xD9, 0x44, 0x24, 0x08, 0x83, 0xEC, 0x14, 0xD8, 0x1D, 0x64, 0xE0, 0x5C, 0x00, 0xDF, 0xE0, 0xF6, 0xC4, 0x41, 0x7A,
    0x11, 0x8B, 0x44, 0x24, 0x18, 0x50, 0xE8, 0xB2, 0xFD, 0x0F, 0x00, 0x83, 0xC4, 0x04, 0x83, 0xC4, 0x14, 0xC3, 0xD9,
    0x44, 0x24, 0x1C, 0xD8, 0x1D, 0x9C, 0x72, 0x5B, 0x00, 0xDF, 0xE0, 0xF6, 0xC4, 0x05, 0x0F, 0x8A, 0x98, 0x00, 0x00,
    0x00, 0xD9, 0x05, 0x9C, 0x72, 0x5B, 0x00, 0xD8, 0x64, 0x24, 0x1C, 0xD8, 0x0D, 0x18, 0x75, 0x5B, 0x00, 0xD9, 0x5C,
    0x24, 0x00, 0xD9, 0x44, 0x24, 0x00, 0xD8, 0x25, 0xB8, 0x4F, 0x61, 0x00, 0xDB, 0x5C, 0x24, 0x1C, 0xDB, 0x44, 0x24,
    0x1C, 0x6A, 0x04, 0xD8, 0x0D, 0xFC, 0x7A, 0x5B, 0x00, 0xD9, 0x54, 0x24, 0x14, 0xD9, 0x54, 0x24, 0x10, 0xD9, 0x54,
    0x24, 0x0C, 0xD9, 0x5C, 0x24, 0x08, 0xE8, 0x83, 0x82, 0xFA, 0xFF, 0xA1, 0x94, 0x7D, 0x6E, 0x00, 0x8B, 0x08, 0x83,
    0xC4, 0x04, 0x6A, 0x01, 0x8D, 0x54, 0x24, 0x08, 0x52, 0x6A, 0x00, 0x50, 0xFF, 0x91, 0x6C, 0x01, 0x00, 0x00, 0x8B,
    0x44, 0x24, 0x1C, 0x99, 0x33, 0xC9, 0x83, 0xC9, 0x64, 0x50, 0x51, 0xE8, 0xA8, 0xD5, 0xFC, 0xFF, 0x6A, 0x00, 0x68,
    0x01, 0x30, 0x05, 0x00, 0xE8, 0xCC, 0xD5, 0xFC, 0xFF, 0x8B, 0x54, 0x24, 0x28, 0x52, 0xE8, 0x12, 0xFD, 0x0F, 0x00,
    0xE8, 0xAD, 0xD5, 0xFC, 0xFF, 0xE8, 0xD8, 0xD5, 0xFC, 0xFF, 0x6A, 0x08, 0xE8, 0x31, 0x82, 0xFA, 0xFF, 0x83, 0xC4,
    0x18, 0x83, 0xC4, 0x14, 0xC3 };

// ---------------------------------------------------------------- who fades
// 02 Whitestool, 03 Hummer, 07 Gum Head, 08 Twin Victim, 09 Tremer, 0A Sniffer Dog, 0B victims, 0C Cynthia's ghost, 0D
// Jasper, 0E Andrew, 0F Richard. Never 01, 04 mm, 05 Wall Man (and the Wall boss: its own wall effect), 06 Wheelchair, 10
// Walter / the Bodies, 11 the Patient, 12+.
static int FiTypeFades(int type) { return (type >= 0x02 && type <= 0x03) || (type >= 0x07 && type <= 0x0F); }
static int FiGhostType(int type) { return type >= 0x0B && type <= 0x0F; }
static int FiBoss(int type, unsigned modeWord) { return type == 0x10 || (type == 0x05 && (modeWord & 0xF) == 8); }
// a monster's spawn mode (mode word & 0xF): 0 ordinary; the Sniffer Dog's 3 / 6 / 7 (asleep by a body, waiting) and the
// Hummer's 1 (on the floor) are ordinary bodies too; every other mode is an event spawn (enemies_sim.h EnmFollowModeOk).
// Ghosts come by many modes of their own (4 = a victim made real by the arrival): always ok.
static int FiModeOk(int type, unsigned modeWord)
{
    const int m = (int)(modeWord & 0xF);
    if (FiGhostType(type) || m == 0) return 1;
    if (type == 0x0A && (m == 3 || m == 6 || m == 7)) return 1;
    return type == 0x03 && m == 1;
}
// the actor record's pre-draw callback (record +0x24) at the model draw
enum { FI_PD_PLAIN, FI_PD_OWNFADE, FI_PD_FOREIGN };
static int FiPreDrawKind(unsigned long fn)
{
    // NULL: a plain enemy; 0x45C6B0 (a ghost, opaque) / 0x45C6C0 (no depth write): 0x45C880 draws them the game's way;
    // 0x45C710 / 0x45C7A0: the game's own see-through draw with work +0x80 (a ghost in or out of a wall); anything else
    // (0x45C840 additive, 0x45C860, event looks): its own draw state, left alone
    if (fn == 0 || fn == 0x45C6B0ul || fn == 0x45C6C0ul) return FI_PD_PLAIN;
    if (fn == 0x45C710ul || fn == 0x45C7A0ul) return FI_PD_OWNFADE;
    return FI_PD_FOREIGN;
}

// ---------------------------------------------------------------- where it came from
enum { FI_SRC_ROOM, FI_SRC_FOLLOW, FI_SRC_WANDER, FI_SRC_COPY, FI_SRC_REVIVE, FI_SRC_GHOST, FI_SRC_MONSTER, FI_SRC_N };
static const char *const kFiSrcName[FI_SRC_N] = { "room load", "follower", "wanderer", "copy", "revive", "ghost", "monster" };
// arrived = the kind enemies.inc's watch list gives the spawn id (-1 none, 0 follower, 1 wanderer, 2 a copy that followed, 3
// a wandering copy); tagged = a victim let in as a follower by the ghosts' room rule; revived = Revive Enemies stood it up
// at this room's load; copy = an Enemy Copies id; inGrace = first seen in the room's first FI_GRACE_S
static int FiSource(int inGrace, int arrived, int tagged, int revived, int copy, int ghost)
{
    if (arrived == 0 || arrived == 2 || tagged) return FI_SRC_FOLLOW;
    if (arrived == 1 || arrived == 3) return FI_SRC_WANDER;
    if (revived) return FI_SRC_REVIVE;
    if (copy) return FI_SRC_COPY;
    if (inGrace) return FI_SRC_ROOM;
    return ghost ? FI_SRC_GHOST : FI_SRC_MONSTER;
}
// why one is drawn as the game draws it (FI_SKIP_NONE = it fades)
enum { FI_SKIP_NONE, FI_SKIP_OFF, FI_SKIP_STAGE, FI_SKIP_SCENE, FI_SKIP_TYPE, FI_SKIP_BOSS, FI_SKIP_EVENT, FI_SKIP_DEAD,
       FI_SKIP_DRAW, FI_SKIP_OWNPORTAL, FI_SKIP_OWNFADE, FI_SKIP_N };
static const char *const kFiSkipName[FI_SKIP_N] = { "-", "Fade In off", "stage", "cut-scene", "type", "boss", "event", "dead",
                                                    "own draw", "own portal", "own fade" };
static int FiSkipWhy(int on, int stageOk, int cutscene, int typeOk, int boss, int modeOk, int dead, int drawOk, int ownPortal,
                     int ownFade)
{
    if (!on) return FI_SKIP_OFF;
    if (!stageOk) return FI_SKIP_STAGE;
    if (cutscene) return FI_SKIP_SCENE;
    if (boss) return FI_SKIP_BOSS;
    if (!typeOk) return FI_SKIP_TYPE;
    if (!modeOk) return FI_SKIP_EVENT;
    if (dead) return FI_SKIP_DEAD;
    if (!drawOk) return FI_SKIP_DRAW;
    if (ownPortal) return FI_SKIP_OWNPORTAL;
    if (ownFade) return FI_SKIP_OWNFADE;
    return FI_SKIP_NONE;
}
// a vortex for: Fade + Portal, a ghost type (victims, the story ghosts, their copies), not at the room load
static int FiPortalFor(int mode, int type, int inGrace) { return mode == FI_MODE_PORTAL && FiGhostType(type) && !inGrace; }

// ---------------------------------------------------------------- the curves
static float FiFrac(float age, float T)  // 0 at first sight .. 1 at T (smoothstep)
{
    float s;
    if (!(age > 0.0f)) return 0.0f;
    if (!(T > 0.0f) || age >= T) return 1.0f;
    s = age / T;
    return s * s * (3.0f - 2.0f * s);
}
// 0x45C880's argument: 0 = opaque, >= 1 = not drawn. own = the game's own see-through amount (work +0x80, 0 .. 1)
static float FiTransparency(float frac, float own)
{
    float t;
    if (!(own > 0.0f)) own = 0.0f;
    if (own > 1.0f) own = 1.0f;
    if (!(frac > 0.0f)) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    t = 1.0f - (1.0f - own) * frac;
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}
static int FiShadowShown(float frac) { return frac >= 0.5f; }  // its shadow joins when the body is half in
static float FiPortalOpen(float age, float T)  // the vortex's strength 0 .. 1
{
    float to = FI_PORTAL_OPEN_S, s;
    if (!(T > 0.0f)) T = FI_SECONDS_DEFAULT;
    if (to > 0.5f * T) to = 0.5f * T;
    if (!(age > 0.0f)) return 0.0f;
    if (age < to) {
        s = age / to;
        return 1.0f - (1.0f - s) * (1.0f - s);
    }
    if (age <= T) return 1.0f;
    if (age < T + FI_PORTAL_CLOSE_S) {
        s = (age - T) / FI_PORTAL_CLOSE_S;
        return (1.0f - s) * (1.0f - s);
    }
    return 0.0f;
}
static float FiPortalLife(float T) { return (T > 0.0f ? T : FI_SECONDS_DEFAULT) + FI_PORTAL_CLOSE_S; }
// the draw clock: real seconds between consecutive drawn frames (at most 0.1 s), 1/30 s across a gap (a pause, a load)
static double FiClockStep(double dt, int continuous)
{
    const double cap = continuous ? 0.1 : 1.0 / 30.0;
    if (!(dt > 0.0)) return 0.0;
    return dt < cap ? dt : cap;
}

// ---------------------------------------------------------------- the room visit's table
enum { FI_E_FADE, FI_E_DONE };
typedef struct { const void *task, *work; unsigned id; double t0, seen; int state, src, skip, type, portal; } FiEnt;  // seen: the clock at its last draw
typedef struct { FiEnt e[FI_MAX]; int n; } FiTable;
static void FiTableClear(FiTable *t) { memset(t, 0, sizeof *t); }
static int FiFind(const FiTable *t, const void *task, const void *work, unsigned id)
{
    int i;
    for (i = 0; i < t->n; ++i)
        if (t->e[i].task == task && t->e[i].work == work && t->e[i].id == id) return i;
    return -1;
}
// a new entry: a free one, else the one not drawn for the longest time if that is over FI_STALE_S ago (gone from view or
// from the room); NULL = the table is full of enemies drawn just now: the caller draws this one as the game does (no fade,
// no entry). v3.50 FI350-001 (Codex inspection 2): an entry still being drawn is never taken (that enemy would be "first
// seen" again and fade again), so an enemy on screen never restarts its fade; an overflow enemy simply does not fade.
static FiEnt *FiAdd(FiTable *t, const void *task, const void *work, unsigned id, double now)
{
    FiEnt *e;
    int i, k = -1;
    if (t->n < FI_MAX) {
        k = t->n++;
    } else {
        for (i = 0; i < FI_MAX; ++i)
            if (t->e[i].seen < now - FI_STALE_S && (k < 0 || t->e[i].seen < t->e[k].seen)) k = i;
        if (k < 0) return NULL;
    }
    e = &t->e[k];
    memset(e, 0, sizeof *e);
    e->task = task, e->work = work, e->id = id, e->t0 = e->seen = now, e->state = FI_E_DONE;
    return e;
}
// at every draw of a known enemy: a fading one that is hidden (its model away or flagged not drawn: a ghost in a wall, a
// type that hides itself) does not use up its fade meanwhile (its start moves on by the time hidden)
static void FiAge(FiEnt *e, double now, int hidden)
{
    if (hidden && e->state == FI_E_FADE && now > e->seen) e->t0 += now - e->seen;
    e->seen = now;
}
// a known spawn id in a new task / work block: the same enemy made again (the game's ghost arrival turns its placeholder,
// task 0x486030, into the real ghost with the placeholder's id at its spot, 0x4852B0; a pinned ghost made again, 0x4853A0):
// its entry follows it, a fade running goes on and nothing new starts (an entry of another id in that slot goes). Id 0 is
// no one's own: never taken over. NULL = not known.
static FiEnt *FiTakeOver(FiTable *t, const void *task, const void *work, unsigned id, double now)
{
    int i, k = -1;
    if (!id) return NULL;
    for (i = 0; i < t->n && k < 0; ++i)
        if (t->e[i].id == id) k = i;
    if (k < 0) return NULL;
    t->e[k].task = task, t->e[k].work = work, t->e[k].seen = now;
    for (i = 0; i < t->n; ++i)
        if (i != k && t->e[i].task == task) {
            t->e[i] = t->e[--t->n];
            if (k == t->n) k = i;  // (the kept entry was the last one: it moved here)
            else --i;
        }
    return &t->e[k];
}
// the same slot re-used for another enemy (another work block or spawn id): its entry goes
static void FiDropTask(FiTable *t, const void *task)
{
    int i;
    for (i = 0; i < t->n; ++i)
        if (t->e[i].task == task) {
            t->e[i] = t->e[--t->n];
            --i;
        }
}

// ---------------------------------------------------------------- Revive Enemies' notes (ids stood up at a room load)
#define FI_NOTE_MAX 32
typedef struct { unsigned id[FI_NOTE_MAX]; int stage[FI_NOTE_MAX], room[FI_NOTE_MAX], n; } FiNotes;
static void FiNotePut(FiNotes *s, int stage, int room, unsigned id)
{
    int i;
    for (i = 0; i < s->n; ++i)
        if (s->id[i] == id && s->stage[i] == stage && s->room[i] == room) return;
    if (s->n >= FI_NOTE_MAX) {  // the oldest goes
        memmove(s->id, s->id + 1, sizeof s->id[0] * (FI_NOTE_MAX - 1));
        memmove(s->stage, s->stage + 1, sizeof s->stage[0] * (FI_NOTE_MAX - 1));
        memmove(s->room, s->room + 1, sizeof s->room[0] * (FI_NOTE_MAX - 1));
        --s->n;
    }
    s->id[s->n] = id, s->stage[s->n] = stage, s->room[s->n] = room, ++s->n;
}
static int FiNoteHas(const FiNotes *s, int stage, int room, unsigned id)
{
    int i;
    for (i = 0; i < s->n; ++i)
        if (s->id[i] == id && s->stage[i] == stage && s->room[i] == room) return 1;
    return 0;
}
static void FiNoteKeepRoom(FiNotes *s, int stage, int room)  // a new room visit: only this room's notes stay
{
    int i, k = 0;
    for (i = 0; i < s->n; ++i)
        if (s->stage[i] == stage && s->room[i] == room) s->id[k] = s->id[i], s->stage[k] = stage, s->room[k] = room, ++k;
    s->n = k;
}

// ---------------------------------------------------------------- a room visit's counts and its log line
typedef struct { int fades[FI_SRC_N], skips[FI_SKIP_N], portals; } FiStats;
static int FiStatsAny(const FiStats *s)
{
    int i, n = s->portals;
    for (i = 0; i < FI_SRC_N; ++i) n += s->fades[i];
    for (i = FI_SKIP_STAGE; i < FI_SKIP_N; ++i) n += s->skips[i];  // (not Off: nothing to say while it is off)
    return n != 0;
}
// "N fade(s) (room load a, follower b, wanderer c, copy d, revive e, ghost f, monster g), P portal(s); K not faded (stage ..,
// cut-scene .., type .., boss .., event .., dead .., own draw .., own portal .., own fade ..)" (fixed order: tools\logcheck.py)
static int FiStatsText(const FiStats *s, char *out, size_t n)
{
    size_t o = 0;
    int i, f = 0, k = 0, w;
    if (!out || n < 2) return 0;
    for (i = 0; i < FI_SRC_N; ++i) f += s->fades[i];
    for (i = FI_SKIP_STAGE; i < FI_SKIP_N; ++i) k += s->skips[i];
#define FI_PUT(...) do { if (o + 1 < n) { w = _snprintf_s(out + o, n - o, _TRUNCATE, __VA_ARGS__); if (w < 0) { o = n; } else o += (size_t)w; } } while (0)
    FI_PUT("%d fade(s) (", f);
    for (i = 0; i < FI_SRC_N; ++i) FI_PUT("%s%s %d", i ? ", " : "", kFiSrcName[i], s->fades[i]);
    FI_PUT("), %d portal(s); %d not faded (", s->portals, k);
    for (i = FI_SKIP_STAGE; i < FI_SKIP_N; ++i) FI_PUT("%s%s %d", i > FI_SKIP_STAGE ? ", " : "", kFiSkipName[i], s->skips[i]);
    FI_PUT(")");
#undef FI_PUT
    if (o >= n) {
        out[n - 1] = 0;
        return (int)n - 1;
    }
    return (int)o;
}
#endif
