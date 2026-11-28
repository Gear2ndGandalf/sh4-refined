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
#include <math.h>  // v3.51 PT351: the tar portal's layout

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
    if (FiSameNoSpace(s, "Fade In")) return FI_MODE_FADE;  // v3.58 MN358: the Options word (typed by hand) reads as Fade
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

// ================================================================ v3.51 PT351: the tar portal and the hold (pure rules)
// USER_FEEDBACK_v350 item 7: "they lack a sticky bloody detail like the real ghost portals ... It is dark, ominous, slows
// down the ghosts with thin sticky tar-like substance." Plan: staging/v351_feedback/w3_portal/PLAN.md. The game's own wall
// portal (WallGhostCreate 0x487840) is three parts, each with a draw that only reads a block: the 9 x 9 grid (record draw
// 0x4868B0, WallGhost 0x140), the blots and drips (record draw 0x486E80, WallGhostBody 0x3F0) and the strands (the ghost's
// own bone trail, GhostWork +0x4C, started by 0x478440, drawn by its own record draw 0x482D60). The grab / damage is only in
// the portal task's update 0x487650, which Refined never runs: Refined fills blocks of its own from the timeline below and
// calls the two draws after the game's pass 2; the strands are the ghost's own (victims and Cynthia have them).
// FadeInPortalDraw (hidden): Auto = the game's own draw when its textures are loaded, else Refined's own tar portal;
// Refined = always Refined's own; Vortex = v3.50's crimson vortex (no hold).
enum { FI_TDM_AUTO, FI_TDM_REFINED, FI_TDM_VORTEX, FI_TDM_N };
static const char *const kFiTdmName[FI_TDM_N] = { "Auto", "Refined", "Vortex" };
static int FiTdmParse(const char *s, int *known)  // [Gameplay] FadeInPortalDraw; unknown / empty = Auto
{
    int v;
    if (known) *known = 1;
    if (s && *s)
        for (v = 0; v < FI_TDM_N; ++v)
            if (FiSameNoSpace(s, kFiTdmName[v])) return v;
    if (s && (FiSameNoSpace(s, "Game") || FiSameNoSpace(s, "1"))) return FI_TDM_AUTO;
    if (s && (FiSameNoSpace(s, "Own") || FiSameNoSpace(s, "2"))) return FI_TDM_REFINED;
    if (s && (FiSameNoSpace(s, "0") || FiSameNoSpace(s, "Blood"))) return FI_TDM_VORTEX;
    if (known) *known = !s || !*s;
    return FI_TDM_AUTO;
}
// a portal's draw: v3.50's vortex, the game's own wall-portal draw, Refined's own tar portal
enum { FI_TD_VORTEX, FI_TD_GAME, FI_TD_OWN, FI_TD_N };
static const char *const kFiTdName[FI_TD_N] = { "vortex", "the game's own draw", "Refined's draw" };
// its strands: none, the ghost's own bone trail, Refined's own
enum { FI_TS_NONE, FI_TS_GAME, FI_TS_OWN, FI_TS_N };
static const char *const kFiTsName[FI_TS_N] = { "none", "the ghost's own", "Refined's" };

#define FI_HOLD_SLOW    0.4f   // the held ghost moves at this part of its speed ...
#define FI_HOLD_SLOW_S  1.5f   // ... for this long after the hold starts (it is real and seen) ...
#define FI_HOLD_END_S   2.5f   // ... easing to its full speed here; the strands are all snapped by then; no attack before
#define FI_HOLD_WARP    200.0f // a tick's move longer than this (a script, a warp) is taken as it is
#define FI_TAR_OPEN_S   0.6f   // the blot opens (eased) ...
#define FI_TAR_SPREAD_S 0.8f   // ... its 25 blots appear one by one over this ...
#define FI_TAR_CLOSE_S  1.6f   // ... and it fades after the release (each blot and drip on its own clock)
#define FI_TAR_FADE_S   1.1f   // (the game's own per-blot fade time, WallGhostBody +0x3B4)
#define FI_TAR_WAIT_S   6.0f   // a ghost that is not real (the arrival's placeholder) this long after its portal opened:
                               // the portal closes without a hold
#define FI_TAR_WALL_R   180.0f // a collision wall this near behind the ghost (from the camera) carries the portal ...
#define FI_TAR_AIR_BACK 70.0f  // ... else it hangs in the air this far behind it, facing the camera (its strands show)
#define FI_TAR_SPLATS   25     // blots (the game's WallGhostBody holds 25)
#define FI_TAR_DRIPS    15     // drips at most (slots 1 .. 15 of the game's 16: slot 0 is never drawn by 0x486E80)
#define FI_DRIP_SPEED   90.0f  // a drip runs down this fast (world units a second), slowing near its length
#define FI_STR_OWN      12     // Refined's own strands (the game's victims have 32 of their own)
#define FI_STR_MAX      64     // strands looked at in a bone trail at most
#define FI_SNAP_STRAIN_S 0.15f // Refined's strand thins this long before it snaps ...
#define FI_SNAP_RECOIL_S 0.35f // ... and its two ends whip back over this

// the game code the tar portal calls (never patches), each checked by its first 16 bytes (fadein.inc at the install; the
// native test against SH4Decomp orig\sh4.exe); group: 0 the draw (all three needed), 1 the strands (both), 2 the hold,
// 3 the floor under the portal (v3.59 FI359)
#define FI_PHASE_VA 0x62BC98u  // phase 0's step list 0x62BC78, entry 8: 0x4EF900 (ActorDrawPass01, ActorDrawModels, ActorDrawPass2)
#define FI_PHASE_FN 0x4EF900u
typedef struct { unsigned va; unsigned char b[16]; int group; const char *name; } FiTarFn;
static const FiTarFn kFiTarFns[] = {
    { 0x4868B0u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0xB4, 0x00, 0x00, 0x00, 0x8B, 0x45, 0x08, 0x53 }, 0,
      "the wall portal's grid draw" },                                                     // EffectWallGhostDraw (matched)
    { 0x486E80u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0x94, 0x00, 0x00, 0x00, 0x8B, 0x45, 0x08, 0x53 }, 0,
      "its blots and drips draw" },                                                         // func_00486E80 (matched)
    { 0x5736D0u, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0xC7, 0x25, 0x00, 0xF0, 0x00, 0xF0, 0x3D, 0x00, 0xF0 }, 0,
      "the resident file lookup" },                                                         // FileLookupResident (matched)
    { 0x478440u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x44, 0x53, 0x8B, 0x5D, 0x08, 0x56, 0x33, 0xF6 }, 1,
      "the bone trail's start" },                                                           // func_00478440 (97.6 %, same behaviour)
    { 0x482CF0u, { 0x8B, 0x44, 0x24, 0x04, 0x56, 0x50, 0xE8, 0xA5, 0x89, 0xFD, 0xFF, 0x8B, 0xF0, 0x8A, 0x46, 0x7D }, 1,
      "the bone trail's release" },                                                         // func_00482CF0 (matched)
    { 0x45CF20u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x18, 0x56, 0x8B, 0x75, 0x0C, 0x8B, 0x0E, 0x57 }, 2,
      "the enemy's place setter" },                                                         // func_0045CF20 (matched)
    { 0x459490u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x34, 0x8B, 0x0D, 0x40, 0xEE, 0xFC, 0x00, 0x53 }, 3,
      "the enemies' floor ray" },                                                           // v3.59 FI359: EnemyFloorRayCheck (the portal on the floor)
};
#define FI_TAR_NFN ((int)(sizeof kFiTarFns / sizeof kFiTarFns[0]))
// the textures of the game's wall portal and strands: entries 1 and 0 of the resident file F000F020 (enemy effect slots 1
// and 0, en_4c4f80.c / en_4c1ec0.c)
#define FI_TEX_PORTAL 0xF001F020u
#define FI_TEX_STRAND 0xF000F020u

static unsigned FiHash(unsigned a)  // integer hash (deterministic layouts, no rand())
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float FiRnd01(unsigned a) { return (float)(FiHash(a) & 0xFFFFFF) / 16777216.0f; }  // 0 .. 1

// the held ghost's speed factor at `age` seconds after its hold started (< 0: not held yet)
static float FiHoldFactor(float age)
{
    float s;
    if (!(age >= 0.0f) || age >= FI_HOLD_END_S) return 1.0f;
    if (age <= FI_HOLD_SLOW_S) return FI_HOLD_SLOW;
    s = (age - FI_HOLD_SLOW_S) / (FI_HOLD_END_S - FI_HOLD_SLOW_S);
    return FI_HOLD_SLOW + (1.0f - FI_HOLD_SLOW) * s * s * (3.0f - 2.0f * s);
}
// one tick of the hold: the ghost went from prev to cur; out = where it stays (only its level move x f; its height as it
// is). 1 = a warp or a bad value (out = cur, taken as it is)
static int FiHoldStep(const float prev[3], const float cur[3], float f, float out[3])
{
    const float dx = cur[0] - prev[0], dz = cur[2] - prev[2];
    const float d2 = dx * dx + dz * dz;
    if (!(d2 <= FI_HOLD_WARP * FI_HOLD_WARP) || !(f == f)) {
        out[0] = cur[0], out[1] = cur[1], out[2] = cur[2];
        return 1;
    }
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    out[0] = prev[0] + f * dx;
    out[1] = cur[1];
    out[2] = prev[2] + f * dz;
    return 0;
}

// the portal's timeline (seconds after it opened): rel = when it lets go (the hold's end, an early release, or
// FI_TAR_WAIT_S for a ghost that never became real); then it closes over FI_TAR_CLOSE_S
static float FiTarRelease(float holdAt) { return holdAt >= 0.0f ? holdAt + FI_HOLD_END_S : FI_TAR_WAIT_S; }
static float FiTarOpenAt(float age)  // the opening alone (0 .. 1, eased out)
{
    float s;
    if (!(age > 0.0f)) return 0.0f;
    if (age >= FI_TAR_OPEN_S) return 1.0f;
    s = age / FI_TAR_OPEN_S;
    return 1.0f - (1.0f - s) * (1.0f - s);
}
static float FiTarAmount(float age, float rel)  // the blot's strength 0 .. 1
{
    float c;
    if (!(age > 0.0f)) return 0.0f;
    if (age < rel) return FiTarOpenAt(age);
    c = (age - rel) / FI_TAR_CLOSE_S;
    if (c >= 1.0f) return 0.0f;
    return FiTarOpenAt(rel) * (1.0f - c) * (1.0f - c);
}
static int FiTarDone(float age, float rel) { return age >= rel + FI_TAR_CLOSE_S; }
static int FiTarShown(float age)  // blots placed so far (they appear one by one, as 0x487460 places the game's)
{
    int n;
    if (!(age > 0.0f)) return 0;
    n = 1 + (int)(age / FI_TAR_SPREAD_S * (float)FI_TAR_SPLATS);
    return n > FI_TAR_SPLATS ? FI_TAR_SPLATS : n;
}
static float FiTarAppear(int i) { return (float)i * FI_TAR_SPREAD_S / (float)FI_TAR_SPLATS; }  // blot i's first second
// what the game's draw 0x486E80 reads as a blot's life (short x 1/1024: >= 0 whole, then a fade over FI_TAR_FADE_S): open =
// a whole second ahead; closing = its own offset (0 .. 0.5 s) minus the seconds since the release
static float FiTarLife(float age, float rel, float offset)
{
    if (age < rel) return 1.0f;
    return offset - (age - rel);
}
static short FiTarLifeShort(float life)  // x 1024, clamped to a short
{
    float v = life * 1024.0f;
    if (!(v == v)) v = 0.0f;
    if (v > 32000.0f) v = 32000.0f;
    if (v < -32000.0f) v = -32000.0f;
    return (short)(v < 0.0f ? v - 0.5f : v + 0.5f);
}
// Refined's own blot, drip and strand strengths from the same lives (1 whole .. 0 gone; the game's curve 1 - q^2)
static float FiTarLifeAlpha(float life)
{
    float q;
    if (life >= 0.0f) return 1.0f;
    q = -life / FI_TAR_FADE_S;
    return q >= 1.0f ? 0.0f : 1.0f - q * q;
}

// the blots and drips: positions in the portal's plane (u across, v down, both -1 .. 1 of its half sizes), turn, size
// factor, the game's piece kind of that index (0x615E44) and the drip slot it carries (0 = none)
static const signed char kFiSplatKind[FI_TAR_SPLATS] = { 1, 1, 1, 4, 5, 1, 1, 6, 4, 7, 5, 1, 7, 4, 6, 1, 1, 6, 4, 7, 5, 1, 7, 4, 6 };
typedef struct { float u, v, ang, sc, off; signed char kind, drip; } FiSplat;
typedef struct { float len, off; int kind; float w; } FiDrip;  // target length (world units at size 1), life offset, piece 2 / 3
typedef struct { FiSplat s[FI_TAR_SPLATS]; FiDrip d[FI_TAR_DRIPS + 1]; int nd; unsigned seed; } FiTarLayout;
static void FiTarLayoutMake(FiTarLayout *L, unsigned seed)
{
    int i;
    memset(L, 0, sizeof *L);
    L->seed = seed;
    for (i = 0; i < FI_TAR_SPLATS; ++i) {
        const unsigned h = FiHash(seed * 0x9E3779B9u + (unsigned)i * 0x85EBCA6Bu);
        FiSplat *s = &L->s[i];
        const float r = (i == 0 ? 0.15f : 0.92f) * (float)sqrt(FiRnd01(h ^ 0x11u)), th = 6.2831853f * FiRnd01(h ^ 0x22u);
        s->u = r * (float)cos(th);
        s->v = r * (float)sin(th);
        s->kind = kFiSplatKind[i];
        s->sc = s->kind == 1 ? (FiRnd01(h ^ 0x33u) * 1.5f + 1.0f) * 0.65f : ((FiRnd01(h ^ 0x33u) - 0.5f) * 0.2f + 1.0f) * 0.65f;
        s->ang = (FiRnd01(h ^ 0x44u) * 2.0f - 1.0f) * 3.1415927f;
        s->off = 0.5f * FiRnd01(h ^ 0x55u);
        s->drip = 0;
    }
    // drips hang from blots in the middle and lower part (v down), 8 .. 12 of them, never from blot 0 (the centre)
    L->nd = 0;
    for (i = 1; i < FI_TAR_SPLATS && L->nd < 8 + (int)(seed % 5u) && L->nd < FI_TAR_DRIPS; ++i) {
        const unsigned h = FiHash(seed ^ (unsigned)i * 0x27D4EB2Fu);
        if (L->s[i].v < -0.25f) continue;
        ++L->nd;
        L->s[i].drip = (signed char)L->nd;
        L->d[L->nd].kind = (h & 1u) ? 2 : 3;  // the two drip pieces of the game's slot 1 (32 x 128, 32 x 96)
        L->d[L->nd].len = (FiRnd01(h ^ 0x66u) * 0.4f + 0.8f) * 200.0f;
        L->d[L->nd].off = 0.5f * FiRnd01(h ^ 0x77u);
        L->d[L->nd].w = 0.7f + 0.6f * FiRnd01(h ^ 0x88u);
    }
}
// a drip's length `age` seconds after its blot appeared: runs at FI_DRIP_SPEED, slows over its last 40 units
static float FiDripLen(float age, float len)
{
    float l, k;
    if (!(age > 0.0f) || !(len > 0.0f)) return 0.0f;
    l = FI_DRIP_SPEED * age;
    k = len > 40.0f ? len - 40.0f : 0.0f;
    if (l <= k) return l;
    return len - (len - k) * (float)exp(-(l - k) / (len - k > 1.0f ? len - k : 1.0f));
}

// the strands snap one by one between FI_HOLD_SLOW_S and FI_HOLD_END_S after the hold started: strand i of n at its own
// moment (a fixed permutation of the order, so each moment is taken once)
static int FiGcd(int a, int b)
{
    while (b) {
        const int t = a % b;
        a = b, b = t;
    }
    return a;
}
// v3.58 FI358: the window [a, b] is the caller's (FiSnapAt: v3.51's 1.5 .. 2.5 s; FiStickWindow: the sticky strands' end)
static float FiSnapAtW(int i, int n, unsigned seed, float a, float b)
{
    static const int kP[] = { 7, 11, 13, 17, 19, 23, 29, 31 };
    int p = 1, k, rank;
    if (n <= 0) return a;
    for (k = 0; k < (int)(sizeof kP / sizeof kP[0]); ++k)
        if (FiGcd(kP[k], n) == 1) {
            p = kP[k];
            break;
        }
    rank = (int)(((unsigned)i * (unsigned)p + seed % (unsigned)n) % (unsigned)n);
    return a + (b - a) * ((float)rank + 0.5f) / (float)n;
}
static float FiSnapAt(int i, int n, unsigned seed) { return FiSnapAtW(i, n, seed, FI_HOLD_SLOW_S, FI_HOLD_END_S); }
// Refined's strand at `age` (seconds after the hold started), snapping at `at`: 0 whole (*k = 1), 1 straining (*k = the
// width left, 1 .. 0.4), 2 snapped (*k = how far its ends whipped back, 0 .. 1), 3 gone
static int FiStrandPhase(float age, float at, float *k)
{
    if (age < at - FI_SNAP_STRAIN_S) {
        *k = 1.0f;
        return 0;
    }
    if (age < at) {
        *k = 1.0f - 0.6f * (age - (at - FI_SNAP_STRAIN_S)) / FI_SNAP_STRAIN_S;
        return 1;
    }
    if (age < at + FI_SNAP_RECOIL_S) {
        const float s = (age - at) / FI_SNAP_RECOIL_S;
        *k = 1.0f - (1.0f - s) * (1.0f - s);
        return 2;
    }
    *k = 1.0f;
    return 3;
}
// the portal's plane: a wall hit (normal toward the ghost, mostly level) or the air behind the ghost, facing the camera
static int FiWallOk(const float n[3]) { return n[1] > -0.5f && n[1] < 0.5f; }
// axes of a plane with normal n (unit): across (level) and down (+y, the game's y points down)
static int FiPlaneAxes(const float n[3], float ax[3], float ay[3])
{
    float l = (float)sqrt(n[0] * n[0] + n[2] * n[2]), d;
    if (!(l > 1e-4f)) return 0;
    ax[0] = -n[2] / l, ax[1] = 0.0f, ax[2] = n[0] / l;
    ay[0] = -n[1] * n[0], ay[1] = 1.0f - n[1] * n[1], ay[2] = -n[1] * n[2];  // +y less its part along n
    d = (float)sqrt(ay[0] * ay[0] + ay[1] * ay[1] + ay[2] * ay[2]);
    if (!(d > 1e-4f)) return 0;
    ay[0] /= d, ay[1] /= d, ay[2] /= d;
    return 1;
}

// ---------------------------------------------------------------- a room visit's counts and its log line
// v3.51 PT351: + the portals by draw, the held ghosts, the portal draw's cost (the frames it drew, ms in all, the worst)
typedef struct { int fades[FI_SRC_N], skips[FI_SKIP_N], portals, byDraw[FI_TD_N], held, drawFrames; double drawMs; float drawMax; } FiStats;
static int FiStatsAny(const FiStats *s)
{
    int i, n = s->portals;
    for (i = 0; i < FI_SRC_N; ++i) n += s->fades[i];
    for (i = FI_SKIP_STAGE; i < FI_SKIP_N; ++i) n += s->skips[i];  // (not Off: nothing to say while it is off)
    return n != 0;
}
// "N fade(s) (room load a, follower b, wanderer c, copy d, revive e, ghost f, monster g), P portal(s)[ (by draw ...)]; K not faded (stage ..,
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
    FI_PUT("), %d portal(s)", s->portals);
    if (s->portals)  // v3.51 PT351: by draw, the held ghosts, the portal draw's cost (tools\logcheck.py reads it)
        FI_PUT(" (the game's own draw %d, Refined's %d, vortex %d; %d held; portal draw %.3f ms a frame, max %.3f)",
               s->byDraw[FI_TD_GAME], s->byDraw[FI_TD_OWN], s->byDraw[FI_TD_VORTEX], s->held,
               s->drawFrames ? s->drawMs / s->drawFrames : 0.0, s->drawMax);
    FI_PUT("; %d not faded (", k);
    for (i = FI_SKIP_STAGE; i < FI_SKIP_N; ++i) FI_PUT("%s%s %d", i > FI_SKIP_STAGE ? ", " : "", kFiSkipName[i], s->skips[i]);
    FI_PUT(")");
#undef FI_PUT
    if (o >= n) {
        out[n - 1] = 0;
        return (int)n - 1;
    }
    return (int)o;
}

// ================================================================ v3.58 FI358: Reveal Method's portal, bigger and stickier
// USER_FEEDBACK_v357 ("The portal parameter design looks nice, but it is smaller than the victims. Make it larger, not by
// stretching it, but by add to the effect with more similar effects on and around it. Make stretchy tar-like substance
// stick to the victims for a few seconds and let the portal stay open for that long too."). Plan:
// staging/v358_feedback/w5_portal/PLAN.md. The first portal keeps v3.57's size; up to FI_PIECES_MAX more portals of the
// same kind (the game's own grid / Refined's blot, each at most the first one's size: never one stretched piece) and two
// more blot sets (25 blots, up to 15 drips each) are laid around it so that the whole is 1.24 x the arriving body's height;
// Refined's sticky strands hold on to the body (its bones, else its box) for FadeInStrandSeconds after it is in, thin as
// they stretch and age, snap one by one over the last 1.3 s, and their body ends drop off; the portal closes when they are
// gone (the pieces around first: it shrinks to its centre). FadeInStrandSeconds 0 + FadeInPortalPieces 0 = v3.57.
#define FI_PIECES_DEFAULT  9      // [Gameplay] FadeInPortalPieces (hidden) 0 .. 9: portals laid around the first one
#define FI_PIECES_MAX      9
#define FI_STICK_DEFAULT   3.0f   // [Gameplay] FadeInStrandSeconds (hidden) 0 .. 6: how long the strands hold after it is in
#define FI_STICK_MAX       6.0f
#define FI_STRANDS_DEFAULT 16     // [Gameplay] FadeInStrands (hidden) 0 .. 24: Refined's sticky strands (half that many
#define FI_STRANDS_MAX     24     //   beside a ghost's own 32)
#define FI_STICK_SNAP_S    1.3f   // the strands snap one by one over the last 1.3 s of their time
#define FI_COVER_Y         0.62f  // the whole portal's half height = this x the body's height (1.24 x the body) ...
#define FI_COVER_X         0.42f  // ... its half width at least this x the body's height (and 1.6 x the body's half width)
#define FI_COVER_MAXK      3.0f   // ... never more than 3 x the first portal's half sizes
#define FI_BODY_MIN        80.0f  // a body's height is taken within 80 .. 600 u (bones; else its type's)
#define FI_BODY_MAX        600.0f
#define FI_MEASURE_WAIT_S  0.35f  // no usable bones this long after the portal opened: the type's box
#define FI_SETS_MAX        3      // blot sets: the first (around the first portal) + 2 over the whole portal
#define FI_PIECE_CLOSE_S   1.0f   // the pieces around close over 1.0 s (the first over FI_TAR_CLOSE_S): it shrinks inward
#define FI_STR_LOOSE       900.0f // a sticky strand stretched past this snaps at once ("stretched loose")
#define FI_DROP_S          0.6f   // a snapped strand's body end falls off over this ...
#define FI_DROP_G          900.0f // ... under this gravity (u / s^2, y down)

static float FiStickClamp(float v)  // [Gameplay] FadeInStrandSeconds: NaN / below 0 -> the default; 0 = v3.57's timeline
{
    if (!(v == v) || v < 0.0f) return FI_STICK_DEFAULT;
    return v > FI_STICK_MAX ? FI_STICK_MAX : v;
}
static int FiCountClamp(int v, int def, int max) { return v < 0 ? def : v > max ? max : v; }  // the two hidden counts
// the strands' end, seconds after the hold started (it starts with the portal): FadeInSeconds (the body is in) + stick, never
// before the hold's end; stick 0 = v3.57 (the hold's end)
static float FiStickEnd(float fadeSec, float stick)
{
    const float e = fadeSec + stick;
    if (!(stick > 0.0f) || !(e > FI_HOLD_END_S)) return FI_HOLD_END_S;
    return e;
}
// where the strands snap one by one: v3.57's 1.5 .. 2.5 s, or the last FI_STICK_SNAP_S of their time
static void FiStickWindow(float sEnd, float *a, float *b)
{
    float s = sEnd - FI_STICK_SNAP_S;
    if (!(sEnd > FI_HOLD_END_S)) {
        *a = FI_HOLD_SLOW_S, *b = FI_HOLD_END_S;
        return;
    }
    if (s < FI_HOLD_SLOW_S) s = FI_HOLD_SLOW_S;
    *a = s, *b = sEnd;
}

// ---------------------------------------------------------------- the arriving body's box
typedef struct { float top, bottom, half, c[3]; int bones, ok; } FiBody;  // y down: top < bottom; half: its half width
static float FiBodyH(const FiBody *b)
{
    const float h = b->bottom - b->top;
    return h < FI_BODY_MIN ? FI_BODY_MIN : h > FI_BODY_MAX ? FI_BODY_MAX : h;
}
// from this frame's bone points (world, y down) near the work position: 0 = fewer than 3 usable ones
static int FiBodyBones(const float (*bp)[3], int n, const float work[3], FiBody *o)
{
    float lo = 1.0e30f, hi = -1.0e30f, sx = 0.0f, sz = 0.0f, r = 0.0f, span;
    int i, k = 0;
    memset(o, 0, sizeof *o);
    for (i = 0; i < n; ++i) {
        const float *q = bp[i];
        if (!(q[0] == q[0] && q[1] == q[1] && q[2] == q[2])) continue;
        if (!(fabsf(q[0] - work[0]) < 400.0f && fabsf(q[2] - work[2]) < 400.0f && fabsf(q[1] - work[1]) < 600.0f)) continue;
        if (q[1] < lo) lo = q[1];
        if (q[1] > hi) hi = q[1];
        sx += q[0], sz += q[2], ++k;
    }
    if (k < 3) return 0;
    sx /= (float)k, sz /= (float)k;
    for (i = 0; i < n; ++i) {
        const float *q = bp[i];
        float d;
        if (!(q[0] == q[0] && q[1] == q[1] && q[2] == q[2])) continue;
        if (!(fabsf(q[0] - work[0]) < 400.0f && fabsf(q[2] - work[2]) < 400.0f && fabsf(q[1] - work[1]) < 600.0f)) continue;
        d = sqrtf((q[0] - sx) * (q[0] - sx) + (q[2] - sz) * (q[2] - sz));
        if (d > r) r = d;
    }
    span = hi - lo;
    o->top = lo - 0.12f * span;     // the crown above the head bone (34 of 307 u, sizes.md)
    o->bottom = hi + 0.04f * span;  // the soles below the lowest bone (the toes / ankles)
    o->half = r + 0.10f * span;     // the limbs' own girth
    if (o->half < 0.12f * FiBodyH(o)) o->half = 0.12f * FiBodyH(o);
    o->c[0] = sx, o->c[1] = 0.5f * (o->top + o->bottom), o->c[2] = sz;
    o->bones = k, o->ok = 1;
    return 1;
}
// the box by its type when the bones are not usable: victims and the story ghosts ~340 u, the work position 200 u above
// the feet (KB 113); Andrew (0x0E) is bigger
static void FiBodyType(int type, const float work[3], FiBody *o)
{
    const float h = type == 0x0E ? 420.0f : 340.0f;
    memset(o, 0, sizeof *o);
    o->bottom = work[1] + 200.0f * h / 340.0f;
    o->top = o->bottom - h;
    o->half = 0.18f * h;
    o->c[0] = work[0], o->c[1] = 0.5f * (o->top + o->bottom), o->c[2] = work[2];
    o->ok = 1;
}
// the whole portal's half sizes for a body (sx, sy: the first portal's): taller than the body, never smaller than the
// first portal, never more than FI_COVER_MAXK x it
static void FiBigTarget(const FiBody *b, float sx, float sy, float *tx, float *ty)
{
    const float h = FiBodyH(b);
    float x = FI_COVER_X * h, y = FI_COVER_Y * h;
    if (x < 1.6f * b->half) x = 1.6f * b->half;
    if (x < sx) x = sx;
    if (y < sy) y = sy;
    if (x > FI_COVER_MAXK * sx) x = FI_COVER_MAXK * sx;
    if (y > FI_COVER_MAXK * sy) y = FI_COVER_MAXK * sy;
    *tx = x, *ty = y;
}

// ---------------------------------------------------------------- the pieces: the first portal and more around it
// a piece: its centre in the portal's plane (u across, v down, world units from the portal's point), its size (a factor
// of the first one's half sizes: 1 for the first, 0.5 .. 0.8 around it), when it starts to open (s), on = laid out (a
// piece on a wall that is not on the wall is dropped by the caller)
typedef struct { float u, v, s, open, phase; int on, slot; } FiPiece;
typedef struct { FiPiece g[1 + FI_PIECES_MAX]; int n, nOn, sets; float tx, ty, du, dv; } FiBig;
// the slots around the first, in the order they are taken: below (the feet), above (the head), the sides, the four
// corners, then one more on the first itself; need: 1 = taller than the first, 2 = wider, 3 = the corners stick out, 0 = always
static const float kFiSlot[FI_PIECES_MAX][3] = {
    { 0.0f, 1.0f, 1 }, { 0.0f, -1.0f, 1 }, { -1.0f, 0.0f, 2 }, { 1.0f, 0.0f, 2 }, { -0.7071f, -0.7071f, 3 },
    { 0.7071f, -0.7071f, 3 }, { -0.7071f, 0.7071f, 3 }, { 0.7071f, 0.7071f, 3 }, { 0.22f, -0.18f, 0 } };
static int FiSlotNeeded(int need, float ru, float rv)
{
    if (need == 1) return rv > 1.08f;
    if (need == 2) return ru > 1.08f;
    if (need == 3) return 0.5f * (ru * ru + rv * rv) > 1.25f;
    return 1;
}
// lay out the first portal (half sizes sx, sy) and up to maxExtra more within the target (tx, ty) centred at (du, dv)
static void FiBigMake(FiBig *B, unsigned seed, float sx, float sy, float tx, float ty, float du, float dv, int maxExtra)
{
    const float ru = sx > 1.0f ? tx / sx : 1.0f, rv = sy > 1.0f ? ty / sy : 1.0f;
    int k, extra = 0;
    memset(B, 0, sizeof *B);
    if (maxExtra < 0) maxExtra = 0;
    if (maxExtra > FI_PIECES_MAX) maxExtra = FI_PIECES_MAX;
    if (!(tx >= sx)) tx = sx;
    if (!(ty >= sy)) ty = sy;
    if (du > 0.5f * (tx - sx)) du = 0.5f * (tx - sx);
    if (du < -0.5f * (tx - sx)) du = -0.5f * (tx - sx);
    if (dv > 0.5f * (ty - sy)) dv = 0.5f * (ty - sy);
    if (dv < -0.5f * (ty - sy)) dv = -0.5f * (ty - sy);
    B->tx = tx, B->ty = ty, B->du = du, B->dv = dv;
    B->g[0].u = du, B->g[0].v = dv, B->g[0].s = 1.0f, B->g[0].on = 1, B->g[0].slot = -1;
    B->n = 1;
    for (k = 0; k < FI_PIECES_MAX && extra < maxExtra; ++k) {
        const unsigned h = FiHash(seed * 0x9E3779B9u + (unsigned)k * 0x632BE5ABu + 0x5A11u);
        const float cu = kFiSlot[k][0], cv = kFiSlot[k][1];
        const int need = (int)kFiSlot[k][2];
        FiPiece *g;
        float s, ru2, rv2, ju, jv;
        if (!FiSlotNeeded(need, ru, rv)) continue;
        if (need == 0 && !extra) continue;  // (the one on the first only with others around it)
        g = &B->g[B->n];
        s = need == 0 ? 0.5f : 0.55f + 0.25f * FiRnd01(h);
        ru2 = tx - s * sx, rv2 = ty - s * sy;  // its outer edge reaches the target's
        if (ru2 < 0.0f) ru2 = 0.0f;
        if (rv2 < 0.0f) rv2 = 0.0f;
        ju = (FiRnd01(h ^ 0x1u) - 0.5f) * 0.16f * s * sx;
        jv = (FiRnd01(h ^ 0x2u) - 0.5f) * 0.16f * s * sy;
        g->u = need == 0 ? du + cu * sx + ju : du + cu * ru2 + ju;
        g->v = need == 0 ? dv + cv * sy + jv : dv + cv * rv2 + jv;
        if (g->u - s * sx < du - tx) g->u = du - tx + s * sx;  // (the jitter never takes it out of the target)
        if (g->u + s * sx > du + tx) g->u = du + tx - s * sx;
        if (g->v - s * sy < dv - ty) g->v = dv - ty + s * sy;
        if (g->v + s * sy > dv + ty) g->v = dv + ty - s * sy;
        g->s = s;
        g->open = 0.10f + 0.05f * (float)extra;  // they open one after another around the first
        g->phase = 6.2831853f * FiRnd01(h ^ 0x3u);
        g->on = 1, g->slot = k;
        ++B->n, ++extra;
    }
    B->nOn = B->n;
    B->sets = extra ? (extra >= 5 ? 3 : 2) : 1;
}
// how strong piece g is (0 .. 1) at `age` s after the portal opened, closing from `rel`
static float FiPieceAmount(float age, float rel, const FiPiece *g, int first)
{
    float a, c;
    if (first) return FiTarAmount(age, rel);
    a = age - g->open;
    if (!(a > 0.0f)) return 0.0f;
    if (age < rel) return FiTarOpenAt(a);
    c = (age - rel) / FI_PIECE_CLOSE_S;
    if (c >= 1.0f) return 0.0f;
    return FiTarOpenAt(rel - g->open) * (1.0f - c) * (1.0f - c);
}
// the effect budget: how many of n pieces around the first are kept (a fractional count: piece k of them fades with
// FiPieceWeight); spark = budget.inc BgSparkNow() (1 / 0.6 / 0.35), deep = a level that thins bodies already
static float FiPieceKeep(float spark, int deep, int n)
{
    float k = spark == spark ? spark : 1.0f;
    if (k > 1.0f) k = 1.0f;
    if (k < 0.35f) k = 0.35f;
    if (deep && k > 0.35f) k = 0.35f;
    return k * (float)(n > 0 ? n : 0);
}
static float FiPieceWeight(float keep, int k)  // k = 1 .. n (the order they were laid out)
{
    const float w = keep - (float)(k - 1);
    return w <= 0.0f ? 0.0f : w >= 1.0f ? 1.0f : w;
}
// the portals' own limit (the effect budget's governor counts only Echo / After Shock / Death Presence): when their draw
// (its CPU time, smoothed over 0.25 s) takes more than FI_SELF_SHARE of the frame, the pieces around them thin (down 1.5 /
// s, to 35 %), and come back below 60 % of it (0.3 / s); at 60 fps the limit is 1.3 ms, at 180 fps 0.44 ms
#define FI_SELF_SHARE 0.08f
static float FiSelfKeepStep(float keep, float msEma, float frameMs, float dt)
{
    const float lim = FI_SELF_SHARE * (frameMs > 1.0f ? frameMs : 1000.0f / 60.0f);
    if (!(keep == keep)) keep = 1.0f;
    if (!(dt > 0.0f)) return keep;
    if (dt > 0.1f) dt = 0.1f;
    if (msEma > lim) keep -= 1.5f * dt;
    else if (msEma < 0.6f * lim) keep += 0.3f * dt;
    return keep < 0.35f ? 0.35f : keep > 1.0f ? 1.0f : keep;
}
// a blot set j (1 / 2: over the whole portal) is drawn with this weight (the first set always 1)
static float FiSetWeight(float spark, int deep, int j)
{
    const float k = FiPieceKeep(spark, deep, 1), need = j == 1 ? 0.45f : 0.75f;
    const float w = (k - need) / 0.2f;
    if (j <= 0) return 1.0f;
    return w <= 0.0f ? 0.0f : w >= 1.0f ? 1.0f : w;
}
// a blot's life for the game's draw (x 1/1024 there) at a weight: whole blots fade to w as the game's own curve 1 - q^2
static float FiLifeAtWeight(float life, float w)
{
    float cap;
    if (!(w < 1.0f)) return life;
    if (!(w > 0.0f)) return -1000.0f;  // (below -FI_TAR_FADE_S: not drawn)
    cap = -FI_TAR_FADE_S * sqrtf(1.0f - w);
    return life < cap ? life : cap;
}
// 1 = the point (u, v) of the plane lies within a laid-out piece (a blot of a set over the whole portal is kept only there)
static int FiBigCovers(const FiBig *B, float u, float v, float sx, float sy)
{
    int k;
    for (k = 0; k < B->n; ++k) {
        const FiPiece *g = &B->g[k];
        const float a = 1.1f * g->s * sx, b = 1.1f * g->s * sy, x = u - g->u, y = v - g->v;
        if (g->on && a > 0.0f && b > 0.0f && (x * x) / (a * a) + (y * y) / (b * b) <= 1.0f) return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- the sticky strands
// how wide a strand is (a factor of its first width) at `age` s after the hold started: it thins to half over its time,
// and as it stretches (the root of its first length over its length now, at least 0.35)
static float FiStickWidth(float age, float sEnd, float len, float len0)
{
    float s = sEnd > 0.0f ? age / sEnd : 1.0f, st = 1.0f;
    if (!(s > 0.0f)) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (len0 > 1.0f && len > len0) st = sqrtf(len0 / len);
    if (st < 0.35f) st = 0.35f;
    return (1.0f - 0.5f * s * s * (3.0f - 2.0f * s)) * st;
}
static float FiStickSag(float age, float sEnd, float len)  // it sags more as it ages (tar runs down), at most 160 u
{
    float s = sEnd > 0.0f ? age / sEnd : 1.0f, g;
    if (!(s > 0.0f)) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    g = (0.05f + 0.10f * s) * (len > 0.0f ? len : 0.0f);
    return g > 160.0f ? 160.0f : g;
}
// a snapped strand's body end, t s after the snap: how far it fell (y down) and how much of it is left (1 .. 0)
static float FiDropFall(float t) { return t > 0.0f ? 0.5f * FI_DROP_G * t * t : 0.0f; }
static float FiDropLeft(float t)
{
    float q;
    if (!(t > 0.0f)) return 1.0f;
    q = t / FI_DROP_S;
    return q >= 1.0f ? 0.0f : (1.0f - q) * (1.0f - q);
}

// ---------------------------------------------------------------- the portal's close line (tools\logcheck.py reads it)
typedef struct {
    unsigned id;
    int type;
    float at;                   // closed at (s after it opened)
    int grids, gridsOf, sets, drips;   // the most pieces drawn in a frame, laid out; blot sets; their drips
    float w, h, body;           // the whole portal (u) and the body's height
    int bones;                  // 1 = the body from its bones, 0 its type's box, -1 never seen
    int strGame, strOwn, ownBox, snapped, loose;
    float stick, stickTo;       // the strands held this long after it was in; to this second after it opened
    double ms;
    float msMax;
    int frames;
} FiClose;
// "%08X (type %02X): its tar portal closed at A s: P piece(s) (grids g of G, blot sets s, drips d), W x H u for a body B u
// tall (R x, its bones / its type's box / not seen); strands: the ghost's own a, Refined's b (its bones / its box), held S s
// after it was in (to T s; n snapped, m stretched loose); portal draw x ms a frame (max y, f frames)"
static int FiCloseText(const FiClose *c, char *out, size_t n)
{
    int w;
    if (!out || n < 2) return 0;
    w = _snprintf_s(out, n, _TRUNCATE,
                    "%08X (type %02X): its tar portal closed at %.1f s: %d piece(s) (grids %d of %d, blot sets %d, drips %d), "
                    "%.0f x %.0f u for a body %.0f u tall (%.2f x, %s); strands: the ghost's own %d, Refined's %d (%s), held "
                    "%.1f s after it was in (to %.1f s; %d snapped, %d stretched loose); portal draw %.3f ms a frame (max %.3f, "
                    "%d frames)", c->id, c->type & 0xFF, c->at, c->grids + c->sets, c->grids, c->gridsOf, c->sets, c->drips,
                    c->w, c->h, c->body, c->body > 1.0f ? c->h / c->body : 0.0f,
                    c->bones > 0 ? "its bones" : c->bones == 0 ? "its type's box" : "not seen", c->strGame, c->strOwn,
                    c->ownBox ? "its box" : "its bones", c->stick, c->stickTo, c->snapped, c->loose,
                    c->frames ? c->ms / c->frames : 0.0, c->msMax, c->frames);
    if (w < 0) {
        out[n - 1] = 0;
        return (int)n - 1;
    }
    return w;
}

// ================================================================ v3.59 FI359: the portal on the floor, the victim at its own
// height, the special filters' looks
// USER_FEEDBACK_v358 QA 9 ("the portal clips to the floor in the subway restrooms while the victim fly's out of it to a new
// spot with its legs floating above it folded in") and Issues ("Spirit Vision still uses the old portal effect"). Plan:
// staging/v359_feedback/w6_portal/PLAN.md. In the user's log the Subway restrooms' (room 2:3) arrivals were first seen with
// their centre 119 .. 155 u above the floor (a victim floats size x 180 above it): the game's own wall exit (the mover
// 0x485F90) brings a victim out at its wall point's height, between where it travelled and the player's floor, holds no
// height while its appearance plays (height mode 0) and only then rises; v3.58 laid the portal out on that body (1.24 x its
// height, centred on it), so its lower part was under the floor.
#define FI_FLOOR_GAP    6.0f    // the portal's foot (and its drips, its blots) stay this far above the floor under it
#define FI_CEIL_GAP     12.0f   // its top this far under a ceiling above it
#define FI_FIT_MINK     0.45f   // a low ceiling shrinks the first portal (both ways: never squashed) to no less than this
#define FI_FLOOR_FROM   150.0f  // the floor ray starts this far above the work position (a ghost may arrive in the floor)
#define FI_FLOOR_REACH  900.0f  // a floor further below the ray's start is not the arrival spot's
#define FI_CEIL_REACH   900.0f  // a ceiling is looked for this far above the body's centre
#define FI_BLOT_R       40.0f   // a blot's reach (u at size 1) kept off the floor
#define FI_VICTIM_TYPE  0x0B    // the victims (EnemyVictimGhostTask 0x4B2C30) ...
#define FI_VICTIM_REST  180.0f  // ... float size x 180 above their floor (GhostHeightReset(size x -180): 0x4AD9C0, states 1 / 2)
#define FI_LIFT_MIN     25.0f   // a victim this much lower than that at its hold's start is lifted to it, unseen ...
#define FI_LIFT_MAX     420.0f  // ... up to this much (more: it is over another floor, left alone)
#define FI_AI_WALLOUT   9       // the victim's AI state that comes out of a wall by itself (0x4ADD20: the mover 0x485F90)
#define FI_WALLOUT_S0   3       // its steps 3 .. 5: it is out and seen, its appearance (animation 35) plays, not rising yet
#define FI_WALLOUT_S1   5
#define FI_WAIT_MAX_S   3.0f    // such a victim is kept unseen at most this long (then it fades in where it is)
#define FI_WAIT_N       8

static int FiSpiritLookParse(int v) { return v != 0; }  // [Gameplay] FadeInSpiritLook (hidden): 1 default, 0 = v3.58
// a victim seen while it still comes out of a wall by itself (no wall portal of the game's near it): kept unseen until it
// is out (Reveal Method Portal only; never at the room load; its grab is not posted before step 6: 0x4B28C0 state 9)
static int FiWallOutWait(int type, int aiState, int aiStep, int ownPortal)
{
    return type == FI_VICTIM_TYPE && aiState == FI_AI_WALLOUT && aiStep >= FI_WALLOUT_S0 && aiStep <= FI_WALLOUT_S1 &&
           !ownPortal;
}
// where a victim floats over the floor fy (y down): its size x FI_VICTIM_REST above it (size = EnemyWork +0x60, 0x45B970)
static float FiRestY(float fy, float size)
{
    if (!(size >= 0.3f && size <= 3.0f)) size = 1.0f;
    return fy - FI_VICTIM_REST * size;
}
// how far to lift a victim at y up to its rest height ry (y down): 0 = leave it (high enough, or too far below: another floor)
static float FiLiftBy(float y, float ry)
{
    const float d = y - ry;
    if (!(d > FI_LIFT_MIN) || !(d < FI_LIFT_MAX)) return 0.0f;
    return d;
}
// fit the whole portal between the floor and a ceiling along the plane's down axis v (0 = the plane's point, + down).
// vFloor: the floor's v less the gap (>= 1e29: none); vCeil: the ceiling's v plus the gap (<= -1e29: none). In: the whole's
// centre dv (the body's) and half height ty, the first portal's half height sy. Out: the whole's centre *vc, half height
// *ty2, the first portal's scale *k (1, or less under a low ceiling). Returns FI_FIT_* bits (0 = as it was).
enum { FI_FIT_RAISED = 1, FI_FIT_SHRUNK = 2, FI_FIT_LOWERED = 4 };
static int FiFloorFit(float vFloor, float vCeil, float dv, float ty, float sy, float *vc, float *ty2, float *k)
{
    float c = dv, t = ty, kk = 1.0f, span;
    int r = 0;
    if (!(vFloor == vFloor) || vFloor > 1e29f) vFloor = 1e30f;
    if (!(vCeil == vCeil) || vCeil < -1e29f) vCeil = -1e30f;
    if (!(t > 0.0f)) t = sy;
    if (!(sy > 0.0f)) sy = 1.0f;
    if (vFloor < 1e29f && vCeil > -1e29f) {
        span = vFloor - vCeil;
        if (span < 2.0f * t) {  // a low ceiling: the whole as tall as the room allows, the first smaller if it must be
            t = 0.5f * span;
            r |= FI_FIT_SHRUNK;
            if (t < sy) {
                kk = t / sy;
                if (kk < FI_FIT_MINK) kk = FI_FIT_MINK;
                t = kk * sy;  // (the floor wins over the ceiling below)
            }
        }
    }
    if (c + t > vFloor) {
        c = vFloor - t;
        r |= FI_FIT_RAISED;
    } else if (c - t < vCeil) {
        c = vCeil + t;
        if (c + t > vFloor) c = vFloor - t;
        r |= FI_FIT_LOWERED;
    }
    *vc = c, *ty2 = t, *k = kk;
    return r;
}
// a drip run down from a point at world y (y down): at most to the floor's gap (fy: the floor; ok = a floor is known)
static float FiDripToFloor(float len, float y, float fy, int ok)
{
    float m;
    if (!ok || !(len > 0.0f)) return len > 0.0f ? len : 0.0f;
    m = fy - FI_FLOOR_GAP - y;
    if (!(m > 0.0f)) return 0.0f;
    return len < m ? len : m;
}
// 1 = a blot of size sc centred at world y is clear of the floor fy
static int FiBlotClear(float y, float sc, float fy, int ok) { return !ok || y + FI_BLOT_R * sc <= fy - FI_FLOOR_GAP; }

// the colours of Refined's own tar draw (FiOwnDisc / FiOwnDrips / FiOwnStrands / FiBlob / FiOwnCore): the tar (the darkening
// blend: 1 - the colour is what is left of the frame) and Spirit Vision's spirit tar (additive over its black edge world)
typedef struct {
    float core[3], edge[3], drip[3], str[3], blob[3], glow[3];  // disc centre / rim, drips, strands, the tar on the body, the core
    float kFirst, kAround, kStr;                                // strength of the first piece, of the pieces around, of the strands
} FiPal;
static const FiPal kFiPalTar = { { 0.60f, 0.97f, 0.96f }, { 0.93f, 0.95f, 0.95f }, { 0.90f, 0.96f, 0.96f },
                                 { 0.90f, 0.96f, 0.96f }, { 0.84f, 0.97f, 0.97f }, { 0.34f, 0.015f, 0.02f }, 1.0f, 1.0f, 1.0f };
static const FiPal kFiPalSpirit = { { 0.02f, 0.05f, 0.16f }, { 0.20f, 0.46f, 1.00f }, { 0.10f, 0.28f, 0.80f },
                                    { 0.30f, 0.58f, 1.00f }, { 0.50f, 0.74f, 1.00f }, { 0.22f, 0.42f, 0.95f }, 0.80f, 0.45f, 0.85f };

// ---------------------------------------------------------------- the arrive line (tools\logcheck.py reads it)
typedef struct {
    unsigned id;
    int type;
    int floorOk, ceilOk;        // a floor under it / a ceiling over it was found
    float foot, raised, k;      // the portal's foot above the floor (u), moved up (u), the first's scale (1 = full size)
    float above, rest;          // its centre above the floor when it was first seen, its own height (victims; 0 = not a victim)
    float lift, waited;         // lifted (u), kept unseen before (s, coming out of a wall by itself)
    int ai, step, anim;         // its AI state, step and animation when it was first seen
} FiArrive;
// "%08X (type %02X) arrives: its tar portal on the floor (foot F u above it, raised R u, full size / a low ceiling: K %); it
// came out A u above the floor (its own height H u): lifted L u / left as it was; AI state S step T, animation N; unseen W s
// before (it came out of a wall by itself)"   (no floor: "its tar portal: no floor found under it (as laid out)")
static int FiArriveText(const FiArrive *a, char *out, size_t n)
{
    char fl[96], tail[96], lf[32];
    int w;
    if (!out || n < 2) return 0;
    if (a->lift > 0.0f) _snprintf_s(lf, sizeof lf, _TRUNCATE, "lifted %.0f u", a->lift);
    else _snprintf_s(lf, sizeof lf, _TRUNCATE, "left as it was");
    if (a->floorOk)
        _snprintf_s(fl, sizeof fl, _TRUNCATE, "its tar portal on the floor (foot %.0f u above it, raised %.0f u, %s%.0f %%)",
                    a->foot, a->raised, a->k < 0.999f ? "a low ceiling: " : "full size: ", 100.0f * a->k);
    else
        _snprintf_s(fl, sizeof fl, _TRUNCATE, "its tar portal: no floor found under it (as laid out)");
    if (a->waited > 0.0f)
        _snprintf_s(tail, sizeof tail, _TRUNCATE, "; unseen %.2f s before (it came out of a wall by itself)", a->waited);
    else
        tail[0] = 0;
    if (a->rest > 0.0f && a->floorOk)
        w = _snprintf_s(out, n, _TRUNCATE, "%08X (type %02X) arrives: %s; it came out %.0f u above the floor (its own height %.0f u): "
                        "%s; AI state %X step %d, animation %d%s", a->id, a->type & 0xFF, fl, a->above, a->rest, lf,
                        a->ai & 0xFFFF, a->step, a->anim, tail);
    else
        w = _snprintf_s(out, n, _TRUNCATE, "%08X (type %02X) arrives: %s; AI state %X step %d, animation %d%s", a->id, a->type & 0xFF,
                        fl, a->ai & 0xFFFF, a->step, a->anim, tail);
    if (w < 0) {
        out[n - 1] = 0;
        return (int)n - 1;
    }
    return w;
}
#endif
