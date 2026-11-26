// v3.51 EP351 (staging/v351_feedback/w2_pursuit/PLAN.md; the user's v3.50 QA item 9: "monsters and especially the victims
// are relentless even with Forgetful"): the pure parts of Enemy Pursuit INSIDE the room. v3.50's rules (pursuit_sim.h) acted
// only at doors; here a chasing monster or victim's ghost gives up in the room and goes back to its own routine, by the
// game's own AI state (pursuit_room.inc applies it). Included by enemies.inc after pursuit_sim.h; native test:
// test/pursuit_room_test.c (test/build_pursuit_room_test.cmd).
#ifndef PURSUIT_ROOM_SIM_H
#define PURSUIT_ROOM_SIM_H

// ---------------------------------------------------------------- the families (SH4Decomp, GOG v7; P = matched code)
// Hummer 03 (en_4700c0.c): states 6 chase / 7 circle / 8 dive give up into 4 (flying wander) by their own sense (P);
//   9 latch-drain / 0xA sting are chasing but never interrupted (the latch bone is parented to her).
// Gum Head 07 (en_493f30.c): standing 4 / 5 / 6 / 8 -> 3 (standing idle), crawling 0xA..0xE / 0x10 / 0x11 -> 9 (crawling
//   wander), the stock exits (P); 7 / 0xF bites are never interrupted; variants >= 2 (wall perch, state 0x14) and the
//   event kinds 6 / 7 are left as they are.
// Sniffer Dog 0A (en_48bdb0.c): state 4 with a player target (target_kind 0 / 1) gives up by its own rule when timer[0]
//   (+0x66) is 0 at its next animation wrap: timer[3] = fps x 5 (its notice cooldown) and state 2 (wander) (R, 98.3 %);
//   Refined only zeroes timer[0]. 7 bite, 8 / 9 grab, 0x0A / 0x13 / 0x14 / 0x17 are chasing, never interrupted.
// Twin Victim 08 (en_4c58a0.c): 2 (not step 6: asleep), 3 steps 0..1, 4, 5, 6, 10 -> 1 (wander), the stock exit (P / R);
//   7 / 8 / 9 attacks and 3 steps >= 2 (holding her) are never interrupted; the event kind 6 is left alone.
// Wall Man 05 (en_49bc40.c): 6 (out of the wall) -> 7 (back into the wall), the stock exit (P); 3 / 4 / 5 / 8 are
//   chasing, never interrupted; the floor variant and the Wall boss (kind 8) are left alone.
// Victim's ghost 0B (en_4ad140.c): 2 / 3 / 6 / 0xC / 0xD -> 1 (idle float) (P); 4 / 5 grab, 7 / 8 / 9 in the wall,
//   0xA bump are chasing, never interrupted. They home on you through walls: no sight rule (distance and time).
// Tremer 09 never chases (it bursts when you are near): nothing to do. Story ghosts (0C..0F, Walter 10) and the victims
// of 0C v1 / 0D v1 (Cynthia's and Jasper's code) are not touched.
enum { EP_FAM_NONE, EP_FAM_HUMMER, EP_FAM_GUMHEAD, EP_FAM_DOG, EP_FAM_TWIN, EP_FAM_WALLMAN, EP_FAM_GHOST, EP_FAM_N };
static const char *const kEpFamName[EP_FAM_N] = { "-", "Hummer", "Gum Head", "Sniffer Dog", "Twin Victim", "Wall Man",
                                                  "victim's ghost" };
// type = work+0x20; variant / kind = the mode word's bits 8..11 / 0..3 (0x45B520 / 0x45B4B0); wallFloor = a Wall Man's
// EnemyWork+0x40 == 1 (its floor variant)
static int EpFamily(int type, int variant, int kind, int wallFloor)
{
    switch (type) {
    case 0x03: return EP_FAM_HUMMER;
    case 0x07: return variant >= 2 || kind == 6 || kind == 7 ? EP_FAM_NONE : EP_FAM_GUMHEAD;
    case 0x0A: return EP_FAM_DOG;
    case 0x08: return kind == 6 ? EP_FAM_NONE : EP_FAM_TWIN;
    case 0x05: return kind == 8 || wallFloor ? EP_FAM_NONE : EP_FAM_WALLMAN;
    case 0x0B: return EP_FAM_GHOST;
    }
    return EP_FAM_NONE;
}
// what its AI state (task+0x410) and step (+0x412) say: EP_NOT = not chasing you, EP_CHASE = chasing but not to be
// interrupted now, EP_CHASE_OK = chasing and the give-up may be set now. extra: the dog's target_kind (+0x24)
enum { EP_NOT, EP_CHASE, EP_CHASE_OK };
static int EpChase(int fam, int st, int step, int extra)
{
    switch (fam) {
    case EP_FAM_HUMMER:
        if (st >= 6 && st <= 8) return EP_CHASE_OK;
        return st == 9 || st == 0x0A ? EP_CHASE : EP_NOT;
    case EP_FAM_GUMHEAD:
        if ((st >= 4 && st <= 6) || st == 8 || (st >= 0x0A && st <= 0x0E) || st == 0x10 || st == 0x11) return EP_CHASE_OK;
        return st == 7 || st == 0x0F ? EP_CHASE : EP_NOT;
    case EP_FAM_DOG:
        if (st == 4) return extra == 0 || extra == 1 ? EP_CHASE_OK : EP_NOT;  // (100 a blood pool, 200 an object)
        return st == 7 || st == 8 || st == 9 || st == 0x0A || st == 0x13 || st == 0x14 || st == 0x17 ? EP_CHASE : EP_NOT;
    case EP_FAM_TWIN:
        if (st == 2) return step == 6 ? EP_NOT : EP_CHASE_OK;  // (step 6: asleep)
        if (st == 3) return step <= 1 ? EP_CHASE_OK : EP_CHASE;
        if (st == 4 || st == 5 || st == 6 || st == 10) return EP_CHASE_OK;
        return st >= 7 && st <= 9 ? EP_CHASE : EP_NOT;
    case EP_FAM_WALLMAN:
        if (st == 6) return EP_CHASE_OK;
        return st == 3 || st == 4 || st == 5 || st == 8 ? EP_CHASE : EP_NOT;
    case EP_FAM_GHOST:
        if (st == 2 || st == 3 || st == 6 || st == 0x0C || st == 0x0D) return EP_CHASE_OK;
        return st == 4 || st == 5 || (st >= 7 && st <= 0x0A) ? EP_CHASE : EP_NOT;
    }
    return EP_NOT;
}
// the AI state of its own routine to go back to (from a state EpChase calls EP_CHASE_OK); -1 = the dog (its own give-up)
static int EpGiveUpState(int fam, int st)
{
    switch (fam) {
    case EP_FAM_HUMMER: return 4;
    case EP_FAM_GUMHEAD: return (st >= 4 && st <= 6) || st == 8 ? 3 : 9;
    case EP_FAM_DOG: return -1;
    case EP_FAM_TWIN: return 1;
    case EP_FAM_WALLMAN: return 7;
    case EP_FAM_GHOST: return 1;
    }
    return -1;
}
// a ghost touching you: its grab states
static int EpGhostTouchState(int st) { return st == 4 || st == 5; }
// the monsters' senses (the stock tests' range and their near sphere, which ignores walls): it sees you when you are
// within near, or within range with nothing between (Refined's line test). Cones are not modelled (a monster "sees" a
// little more than the game does: it gives up a little later, never earlier).
static float EpSenseRange(int fam)
{
    switch (fam) {
    case EP_FAM_HUMMER: return 2000.0f;   // HUMMER_SENSE (4000 while you attack)
    case EP_FAM_GUMHEAD: return 3000.0f;  // GumHeadSenseNear 0x495860
    case EP_FAM_DOG: return 1500.0f;      // SnifferDogCheckHenryTarget 0x48EAA0
    case EP_FAM_TWIN: return 3000.0f;     // TwinSenseLook 0x4C5F80
    case EP_FAM_WALLMAN: return 2400.0f;  // its wake distance
    }
    return 0.0f;
}
static float EpNearRange(int fam)
{
    switch (fam) {
    case EP_FAM_HUMMER: return 1000.0f;
    case EP_FAM_GUMHEAD: return 600.0f;
    case EP_FAM_DOG: return 500.0f;
    case EP_FAM_TWIN: return 1000.0f;
    case EP_FAM_WALLMAN: return 700.0f;
    }
    return 0.0f;
}

// ---------------------------------------------------------------- the rules by Enemy Pursuit's value (pursuit_sim.h)
typedef struct {
    float memory;  // monsters: seconds without sensing you before they give up
    float rest;    // monsters: seconds after a give-up they ignore you (unless hit or within EP_CLOSE)
    float farR, farT;  // ghosts: farther than farR for farT seconds -> give up
    float chaseT;  // ghosts: chasing this long without touching you -> give up
    float gRest;   // ghosts: the rest after a give-up (they drift back towards their spot)
} EpRules;
static int EpRulesOf(int mode, EpRules *r)  // 0 = no in-room rule (Relentless = stock)
{
    if (mode == EF_PURSUIT_NORMAL) {
        r->memory = EF_MEMORY_NORMAL, r->rest = 0.0f, r->farR = 2500.0f, r->farT = 12.0f, r->chaseT = 25.0f, r->gRest = 8.0f;
        return 1;
    }
    if (mode == EF_PURSUIT_FORGETFUL) {
        r->memory = EF_MEMORY_FORGETFUL, r->rest = 3.0f, r->farR = 1500.0f, r->farT = 6.0f, r->chaseT = 12.0f, r->gRest = 15.0f;
        return 1;
    }
    return 0;
}
#define EP_CLOSE 500.0f        // a monster's rest ends when you come this close
#define EP_GHOST_CLOSE 400.0f  // a ghost's rest ends when you come this close
#define EP_TOUCH 300.0f        // a ghost this close touches you (its chase timer starts again)
#define EP_PEND_MAX 3.0f       // the dog's own give-up must come within this (else the game kept it chasing)
#define EP_QUICK 1.5f          // a chase taken up again this soon after a give-up (no rest) is "quick"
#define EP_QUICK_MAX 2         // after this many quick ones the game's own rules have it for this room visit

// ---------------------------------------------------------------- the per-room-visit table
// One entry per enemy seen in this room visit (serial = enemies.inc g_enmDoorSerial; a room load or a save load resets
// it: no timer carries over). Times are the play clock (g_enmClock: no time passes in pauses, menus, loads).
typedef struct {
    unsigned id;
    int fam;
    float seen;     // monsters: it last sensed you (saw you, near, hit, or took up a chase); ghosts: chase start / touch / hit
    float chase0;   // ghosts: this chase began, or it last touched you
    float far0;     // ghosts: farther than farR since (< 0: not)
    float restEnd;  // its rest lasts until (0: none)
    float gaveAt;   // its last give-up (0: none yet)
    float pendAt;   // a dog's give-up waits for its own step since (0: none)
    float hp;       // its last HP (< 0: unknown); a drop = you hit it
    int chasing, downLost, quick, stock, gives;
} EpE;
#define EP_N 96  // = the enemy list walk's guard
typedef struct {
    int serial, n, full;
    int nMon, nGhost, nAgain, nDown, nStock, nHeld;  // this room visit: give-ups (monsters, ghosts), of them a second or later,
                                                     // of them "down when you walked off", left to the game, pushed back in a rest
    EpE e[EP_N];
} EpRoom;
static void EpRoomReset(EpRoom *r, int serial)
{
    r->serial = serial, r->n = 0, r->full = 0;
    r->nMon = r->nGhost = r->nAgain = r->nDown = r->nStock = r->nHeld = 0;
}
static EpE *EpFind(EpRoom *r, unsigned id)
{
    int i;
    for (i = 0; i < r->n; ++i)
        if (r->e[i].id == id) return &r->e[i];
    return 0;
}
// one tick's look at an enemy
typedef struct {
    int chase;   // EpChase
    int sees;    // monsters: it sees you now (EpSenseRange / EpNearRange, the line test)
    int down;    // monsters: lying down / not standing
    int touch;   // ghosts: in a grab state
    float dist;  // to you (< 0: unknown)
    float hp;    // < 0: unknown
} EpLook;
// what to do now
enum { EP_KEEP,     // nothing
       EP_GIVEUP,   // it gives up: set its routine's state (then EpCommit)
       EP_AGAIN,    // in its rest it took up the chase: set its routine's state again (EpHeld)
       EP_REAPPLY,  // the dog's give-up is on its way: keep timer[0] at 0
       EP_HOLD };   // in its rest, not chasing: keep it from noticing you (and a ghost drifting home)
enum { EP_WHY_NONE, EP_WHY_UNSEEN, EP_WHY_DOWN, EP_WHY_FAR, EP_WHY_LONG };
static const char *const kEpWhy[5] = { "-", "it had not sensed you", "it was down when you walked off",
                                       "it stayed far from you", "it chased you without touching you" };
static int EpStep(EpRoom *r, unsigned id, int fam, int mode, float now, const EpLook *k, int *why)
{
    EpRules R;
    EpE *e;
    const int ghost = fam == EP_FAM_GHOST;
    int hit = 0, close, starting, resting;
    if (why) *why = EP_WHY_NONE;
    if (fam == EP_FAM_NONE || !EpRulesOf(mode, &R)) return EP_KEEP;
    if ((e = EpFind(r, id)) == 0) {
        if (r->n >= EP_N) {
            r->full = 1;
            return EP_KEEP;
        }
        e = &r->e[r->n++];
        memset(e, 0, sizeof *e);
        e->id = id, e->fam = fam, e->seen = now, e->chase0 = now, e->far0 = -1.0f;
        e->hp = k->hp >= 0.0f && k->hp < 1.0e9f ? k->hp : -1.0f;
    } else if (k->hp >= 0.0f && k->hp < 1.0e9f) {
        if (e->hp >= 0.0f && k->hp < e->hp - 0.01f) hit = 1;
        e->hp = k->hp;
    }
    if (now < e->seen) e->seen = now;  // (a clock that went back: never a negative age)
    if (now < e->chase0) e->chase0 = now;
    close = k->dist >= 0.0f && k->dist < (ghost ? EP_GHOST_CLOSE : EP_CLOSE);
    // what it sensed this tick
    if (!ghost) {
        if (k->sees || hit || close) e->seen = now, e->downLost = 0;
        else if (k->down) e->downLost = 1;  // (down and you are out of its senses: it forgets you at once)
    } else if (hit || k->touch || (k->dist >= 0.0f && k->dist < EP_TOUCH)) {
        e->seen = now, e->chase0 = now;
    }
    if (e->restEnd > 0.0f && (hit || close || (ghost && k->touch))) e->restEnd = 0.0f;  // (you woke it)
    resting = e->restEnd > 0.0f && now < e->restEnd;
    if (e->restEnd > 0.0f && !resting) e->restEnd = 0.0f;
    // the dog's own give-up on its way
    if (e->pendAt > 0.0f) {
        if (k->chase == EP_NOT) {
            e->pendAt = 0.0f, e->chasing = 0, e->far0 = -1.0f;
            return resting ? EP_HOLD : EP_KEEP;
        }
        if (now - e->pendAt < EP_PEND_MAX) {
            e->chasing = 1;
            return k->chase == EP_CHASE_OK ? EP_REAPPLY : EP_KEEP;
        }
        e->pendAt = 0.0f, e->restEnd = 0.0f, resting = 0;  // (the game kept it chasing: a chase of its own from now)
        e->seen = now, e->chase0 = now, e->chasing = 1, e->downLost = 0;
        return EP_KEEP;
    }
    if (k->chase == EP_NOT) {
        e->chasing = 0, e->far0 = -1.0f;
        return resting ? EP_HOLD : EP_KEEP;
    }
    starting = !e->chasing;
    if (resting) {
        if (k->chase == EP_CHASE_OK) {  // it noticed you by a way the hold does not cover: back to its routine
            e->chasing = 0;
            return EP_AGAIN;
        }
        e->restEnd = 0.0f, resting = 0, starting = 1;  // (an attack of its own: it is chasing you again)
    }
    if (starting) {
        if (!ghost) {
            if (!e->downLost) e->seen = now;  // (it noticed you: the stock rules say so)
        } else {
            e->seen = now, e->chase0 = now, e->far0 = -1.0f;
        }
        if (e->gaveAt > 0.0f && now - e->gaveAt < EP_QUICK && !e->stock && ++e->quick >= EP_QUICK_MAX) {
            e->stock = 1;  // (the game keeps seeing what Refined's test does not: its own rules have it now)
            ++r->nStock;
        }
    }
    e->chasing = 1;
    if (e->stock) return EP_KEEP;
    if (!ghost) {
        const int downX = e->downLost != 0, unseenX = now - e->seen >= R.memory;
        if (!downX && !unseenX) return EP_KEEP;
        if (k->chase != EP_CHASE_OK) return EP_KEEP;  // (it waits for a state that may be changed)
        if (why) *why = downX ? EP_WHY_DOWN : EP_WHY_UNSEEN;
        return EP_GIVEUP;
    }
    if (k->dist > R.farR) {
        if (e->far0 < 0.0f || now < e->far0) e->far0 = now;
    } else {
        e->far0 = -1.0f;
    }
    {
        const int farX = e->far0 >= 0.0f && now - e->far0 >= R.farT, longX = now - e->chase0 >= R.chaseT;
        if (!farX && !longX) return EP_KEEP;
        if (k->chase != EP_CHASE_OK) return EP_KEEP;
        if (why) *why = farX ? EP_WHY_FAR : EP_WHY_LONG;
        return EP_GIVEUP;
    }
}
// the give-up was set (soft: the dog's own, on its way). Returns 1 when it is a second or later give-up of this visit.
static int EpCommit(EpRoom *r, unsigned id, int mode, float now, int soft, int why)
{
    EpRules R;
    EpE *e = EpFind(r, id);
    int again;
    if (!e || !EpRulesOf(mode, &R)) return 0;
    again = e->gives++ > 0;
    e->gaveAt = now > 0.0f ? now : 1.0e-6f;
    e->restEnd = e->fam == EP_FAM_GHOST ? now + R.gRest : R.rest > 0.0f ? now + R.rest : 0.0f;
    e->downLost = 0, e->far0 = -1.0f;
    if (soft) e->pendAt = e->gaveAt;
    else e->chasing = 0;
    if (e->fam == EP_FAM_GHOST) ++r->nGhost;
    else ++r->nMon;
    r->nAgain += again;
    r->nDown += why == EP_WHY_DOWN;
    return again;
}
static void EpHeld(EpRoom *r) { ++r->nHeld; }
// v3.51 MERGE (EP351 x PT351): a ghost Fade In's tar portal holds (fadein.inc FiHolds: up to 2.5 s after it arrives) is
// not judged: its entry (if it has one) restarts every held tick, so its far / chase / unseen timers start when the hold
// ends (its rest and its give-up count are kept); no entry is made for it
static void EpPortalHeld(EpRoom *r, unsigned id, float now)
{
    EpE *e = EpFind(r, id);
    if (!e) return;
    e->chasing = 0, e->downLost = 0, e->pendAt = 0.0f, e->far0 = -1.0f, e->seen = now, e->chase0 = now;
}
// it gave up the chase in this room visit and has not taken it up again (in its rest, or not sensed you since): it does
// not follow you through the next door
static int EpIgnoresYou(const EpRoom *r, unsigned id, int mode, float now)
{
    int i;
    EpRules R;
    if (!EpRulesOf(mode, &R)) return 0;
    for (i = 0; i < r->n; ++i)
        if (r->e[i].id == id) {
            const EpE *e = &r->e[i];
            if (!(e->gaveAt > 0.0f) || (e->chasing && !(e->pendAt > 0.0f))) return 0;
            return (e->restEnd > 0.0f && now < e->restEnd) || e->seen <= e->gaveAt || e->pendAt > 0.0f;
        }
    return 0;
}
// seconds a ghost's give-up rule uses for a line: the far / chase time of the value
static float EpGhostSecs(int mode, int why)
{
    EpRules R;
    if (!EpRulesOf(mode, &R)) return 0.0f;
    return why == EP_WHY_FAR ? R.farT : R.chaseT;
}
#endif
