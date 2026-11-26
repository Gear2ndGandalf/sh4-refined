// ================================================================ v3.51 EP351: Enemy Pursuit inside the room
// Plan: staging/v351_feedback/w2_pursuit/PLAN.md (the user's v3.50 QA item 9: "monsters and especially the victims are
// relentless even with Forgetful setting"; v3.50 acted only at doors). Included at the end of enemies.inc (its state and
// helpers). The pure rules are pursuit_room_sim.h (test/pursuit_room_test.c). GOG v7 VAs, checked against the stock exe.
//
// Every normal-play tick (EnmTickC, the 0x4EF982 dispatcher: before this frame's enemy manager and enemy updates), each
// enemy of a family pursuit_room_sim.h knows is looked at: its AI state (task+0x410 / +0x412), whether it sees you (its
// sense range with nothing between: the line test 0x454000; or its near sphere), how far it is, its HP (a drop = a hit).
// Normal / Forgetful:
//   monsters: chasing you and not sensing you for 10 / 4 s (at once if it lay down while you walked off) -> it gives up:
//     its own routine's AI state is set the way the game's deferred requests set one (0x45BBF0: one honoured request a
//     frame, then 0x45B5F0); the requests it queued last frame are refused for this frame by that same bit. The Sniffer
//     Dog gets its own give-up instead (timer[0] = 0: at its next step it sets its notice cooldown and wanders).
//     Forgetful: for 3 s after, its sight requests are withdrawn (0x462800) and a chase it takes up is set back, unless
//     you hit it or come within 500.
//   victims' ghosts (0B): farther than 2500 / 1500 for 12 / 6 s, or chasing 25 / 12 s without touching you -> idle float
//     (state 1) and a rest of 8 / 15 s: its sight request is withdrawn every tick, its idle timer held (no dive into a
//     wall), its goal set to the spot it was made at (0x45B4D0) until it is there; a hit or you within 400 ends the rest.
//   A monster or ghost that gave up and has not taken the chase up again does not follow you through the next door
//   (enemies.inc EnmTakePursuers), and any pursuit it had ends at the give-up.
//   v3.52 EP352 (the user's v3.51 QA 2: victims' ghosts very hard to shake even on Forgetful): Forgetful's ghosts give up
//     when farther than 1200 for 3 s, after 8 s without touching you, or when you are out of their sight (the line test
//     below) for 3 s; after their 15 s rest they stay calm (held as in the rest) until they sense you again (within 1200
//     with nothing between; a hit, their touch or you within 400 wake them at once). A dive into a wall towards you that
//     one begins from its idle float in its rest or calm (AI state 7, steps 0..2, before its wall portal opens) is set back
//     to the idle float (EpUndive). Host decision: Normal keeps its numbers (2500 / 12 s, 25 s, 8 s rest, no sight rule),
//     but gets the dive fix, the calm after the rest (the game's own 2000 with nothing between) and no quick-guard hand-over.
// Relentless = stock (nothing is looked at). Pinned ghosts, downed or dying enemies, bosses, story ghosts and events are
// never touched; states that hold you (grabs, the Hummer's latch) or an attack are never interrupted.
// [Gameplay] PursuitInRoom=0 (ini only) = v3.50 (the door rules only). No exe byte is patched: only stock functions are
// called (their entries are checked at the start).

static EpRoom g_epRoom;
static int g_epFns, g_epIniOn = 1, g_epSaid;
static float g_epClockSeen = -1.0f;
static int g_epRoomStage = -1, g_epRoomNo = -1;
static int g_epGiveMon, g_epGiveGhost;  // this session (the install line, the room lines say the rest)
static volatile LONG g_epQuotaGive, g_epQuotaRoom, g_epQuotaStock;
static volatile LONG g_epQuotaCalm, g_epQuotaUndive;  // v3.52 EP352: the calm's end, the dives set back (LogV, quota'd)
#define EP_SETSTATE(t, s) ((void(__cdecl *)(BYTE *, int))0x45B5F0)(t, s)             // EnemySetAiState
#define EP_GATE(t)        ((int(__cdecl *)(BYTE *))0x45BBF0)(t)                     // one honoured request a frame (work+0x5C bit 0)
#define EP_UNPOST(c, o)   ((void(__cdecl *)(DWORD, const void *))0x462800)(c, o)    // withdraw a deferred action (check, poster)
#define EP_SENSECHECK     0x46E540u                                                  // the sense records' check (0x46E540)
#define EP_SPAWNPOS(t, o) ((void(__cdecl *)(BYTE *, float *))0x45B4D0)(t, o)        // the point it was made at (ObjInfo +4)
#define EP_AIMPOINT()     ((const float *(__cdecl *)(int))0x46B330)(0)              // EnemyGetTargetAimPoint(0): her aim point

static int FiHolds(const BYTE *task, unsigned id);  // v3.51 MERGE (EP351 x PT351): fadein.inc (included after enemies.inc)
static volatile LONG g_epQuotaPortal;                // v3.51 MERGE: the LogT line of a ghost left alone in its hold
static int EpRayFree(void)  // a free mover ray (0x454000 takes one; teleport.inc GtRayFree's rule, own copy)
{
    int k, n = 0;
    for (k = 0; k < 4; ++k) n |= *(const volatile int *)(0xFCEE40 + 0x30 * k) == 3;
    return n;
}
// 1 = nothing between a and b, 0 = the room's collision is between, -1 = not tested (no free ray)
static int EpLineClear(const float *a, const float *b)
{
    __declspec(align(16)) float pa[4], pb[4];
    if (!EpRayFree()) return -1;
    pa[0] = a[0], pa[1] = a[1], pa[2] = a[2], pa[3] = 1.0f;
    pb[0] = b[0], pb[1] = b[1], pb[2] = b[2], pb[3] = 1.0f;
    return ((int(__cdecl *)(void *, float *, float *))0x454000)(NULL, pa, pb) == 0;
}
// the type block [work+0x24] (0x45B690), NULL when not a pointer
static BYTE *EpBlock(const BYTE *w)
{
    BYTE *b = *(BYTE *const *)(w + 0x24);
    return (ULONG_PTR)b >= 0x10000 ? b : NULL;
}
// its sense record: the one whose deferred sight request (check 0x46E540, poster = the record) starts its chase
static const void *EpSenseOf(int fam, const BYTE *blk)
{
    int off = 0;
    const void *s;
    switch (fam) {
    case EP_FAM_HUMMER: off = 0x44; break;   // HummerWork.sense
    case EP_FAM_GUMHEAD: off = 0x7C; break;  // GumHeadWork.sense
    case EP_FAM_TWIN: off = 0x24; break;     // TwinWork.sense
    case EP_FAM_GHOST: off = 0x5C; break;    // VictimWork.sense (0x4AEC60)
    }
    if (!off || !blk) return NULL;
    s = *(const void *const *)(blk + off);
    return (ULONG_PTR)s >= 0x10000 ? s : NULL;
}
// set its routine's state the way the game's deferred request does (0x462700): 1 = set
static int EpSetState(BYTE *t, int s)
{
    if (s < 0 || EP_GATE(t)) return 0;  // (a request was honoured already this frame: next tick)
    EP_SETSTATE(t, s);
    return 1;
}
// a victim's ghost in its rest, idle (state 1): keep its idle timer from running out (step 2 would dive into a wall
// towards you) and its goal on the spot it was made at until it is near it
static void EpGhostHome(BYTE *t, const BYTE *w, BYTE *blk)
{
    __declspec(align(16)) float home[4];
    BYTE *g = *(BYTE *const *)(w + 0x28);  // GhostWork (0x45B6A0): +0x00 the goal of state 1
    const float *pos = (const float *)w;
    float dx, dz;
    if (*(const WORD *)(t + 0x410) != 1 || *(const WORD *)(t + 0x412) != 2) return;
    if (blk && _finite(*(const float *)(blk + 0x34)) && *(const float *)(blk + 0x34) < 0.5f) *(float *)(blk + 0x34) = 0.5f;
    if ((ULONG_PTR)g < 0x10000 || !EnmFinite3(pos)) return;
    EP_SPAWNPOS(t, home);
    if (!EnmFinite3(home) || (home[0] == 0.0f && home[1] == 0.0f && home[2] == 0.0f)) return;
    dx = home[0] - pos[0], dz = home[2] - pos[2];
    if (dx * dx + dz * dz < 300.0f * 300.0f) return;  // (there: its own idle float)
    ((float *)g)[0] = home[0], ((float *)g)[1] = pos[1], ((float *)g)[2] = home[2];  // (its own height: no climb or dive)
}
// v3.52 EP352: a ghost in its rest / calm (Normal, Forgetful) began a dive into a wall towards you (AI state 7 at step 0..2: flying to
// its wall node, the portal not open; the stock exit 7 -> 1 is the "no node" case of 0x4B1DD0). Back to its idle float:
// at step 0 the float's own start (step 0) already ran in the frame it chose to dive (its step 1: you > 4000 away), so it
// goes on at the float itself (step 2: its step 1 would choose the dive again) with its goal on its spot (else 500 away
// from you) and its timer held; at steps 1..2 (a set-back one frame late) the float starts afresh (its step 0 undoes the
// dive's height mode and path). 1 = set back; 0 = not now (another request this frame, or not such a dive).
static int EpUndive(BYTE *t, const BYTE *w, BYTE *blk, const float *pp, int step)
{
    __declspec(align(16)) float home[4];
    BYTE *g = *(BYTE *const *)(w + 0x28);
    const float *pos = (const float *)w;
    if (*(const WORD *)(t + 0x410) != 7 || step < 0 || step > 2 || EP_GATE(t)) return 0;
    EP_SETSTATE(t, 1);
    if (step != 0) return 1;
    *(WORD *)(t + 0x412) = 2;  // (EnemySetAiStep: task+0x412)
    if (blk && _finite(*(const float *)(blk + 0x34)) && *(const float *)(blk + 0x34) < 0.5f) *(float *)(blk + 0x34) = 0.5f;
    if ((ULONG_PTR)g < 0x10000 || !EnmFinite3(pos)) return 1;
    EP_SPAWNPOS(t, home);
    if (EnmFinite3(home) && !(home[0] == 0.0f && home[1] == 0.0f && home[2] == 0.0f)) {
        ((float *)g)[0] = home[0], ((float *)g)[1] = pos[1], ((float *)g)[2] = home[2];
    } else if (pp && EnmFinite3(pp)) {
        const float dx = pos[0] - pp[0], dz = pos[2] - pp[2], d = sqrtf(dx * dx + dz * dz);
        if (d > 1.0f && _finite(d))
            ((float *)g)[0] = pos[0] + dx / d * 500.0f, ((float *)g)[1] = pos[1], ((float *)g)[2] = pos[2] + dz / d * 500.0f;
    }
    return 1;
}
static void EpSay(const char *what, unsigned id, int type, int variant, int fam, int st, int to, int why, int again, int pmode,
                  float secs)
{
    char b[96];
    EpRules R;
    if (!LogQuota(&g_epQuotaGive, 400)) return;
    EpRulesOf(pmode, &R);
    if (why == EP_WHY_UNSEEN) sprintf_s(b, sizeof b, "it had not sensed you for %.0f s", secs);
    else if (why == EP_WHY_FAR) sprintf_s(b, sizeof b, "farther than %.0f from you for %.0f s", R.farR, secs);
    else if (why == EP_WHY_LONG) sprintf_s(b, sizeof b, "it chased you %.0f s without touching you", secs);
    else if (why == EP_WHY_SIGHT) sprintf_s(b, sizeof b, "you were out of its sight for %.0f s", secs);  // v3.52 EP352
    else strcpy_s(b, sizeof b, kEpWhy[why >= 0 && why < EP_WHY_N ? why : 0]);
    if (fam == EP_FAM_GHOST)
        LogV("enemies: %s %08X (type %02X v%d) gave up the chase in room %d:%d: %s (Enemy Pursuit %s; AI state %d -> %d; it drifts "
             "back towards its spot and rests %.0f s%s)%s", what, id, type, variant, ENM_STAGE, ENM_ROOM, b, EfPursuitNameOf(pmode),
             st, to, R.gRest, R.gSense > 0.0f ? ", then stays calm until it senses you again" : "",  // v3.52 EP352
             again ? " (again)" : "");
    else if (to < 0)
        LogV("enemies: %s %08X (%s, type %02X v%d) gave up the chase in room %d:%d: %s (Enemy Pursuit %s; AI state %d: the dog's own "
             "give-up at its next step, then it wanders)%s", what, id, kEpFamName[fam], type, variant, ENM_STAGE, ENM_ROOM, b,
             EfPursuitNameOf(pmode), st, again ? " (again)" : "");
    else
        LogV("enemies: %s %08X (%s, type %02X v%d) gave up the chase in room %d:%d: %s (Enemy Pursuit %s; AI state %d -> %d, its own "
             "routine%s)%s", what, id, kEpFamName[fam], type, variant, ENM_STAGE, ENM_ROOM, b, EfPursuitNameOf(pmode), st, to,
             R.rest > 0.0f ? "; it ignores you 3 s unless you hit it or come close" : "", again ? " (again)" : "");
}
// the room's line (always; quota'd): called when the room visit ends (a room load, a save load)
static void EpRoomEnd(const char *why)
{
    EpRoom *r = &g_epRoom;
    if (r->nMon + r->nGhost && g_epRoomStage >= 0 && LogQuota(&g_epQuotaRoom, 600)) {
        char calm[112] = "";  // v3.52 EP352: the ghosts' calm (only when there is something to say: older lines unchanged)
        if (r->nUndive + r->nSensed)
            sprintf_s(calm, sizeof calm, "; ghosts' calm: %d ghost dive(s) into a wall stopped, %d ghost(s) sensed you again",
                      r->nUndive, r->nSensed);
        Log("enemies: room %d:%d (Enemy Pursuit %s): %d gave up the chase in the room (%d monster(s), %d ghost(s); %d of them "
            "again, %d down when you walked off); %d set back during their rest; %d left to the game's own rules (%s)%s",
            g_epRoomStage, g_epRoomNo, EfPursuitNameOf((int)g_enmPursuit), r->nMon + r->nGhost, r->nMon, r->nGhost, r->nAgain,
            r->nDown, r->nHeld, r->nStock, why, calm);
    }
    EpRoomReset(r, 0);
    g_epRoomStage = g_epRoomNo = -1;
}
static int EpLive(void) { return g_epFns && g_epIniOn && g_enmInstalled && !g_enmFail && EfLoses((int)g_enmPursuit); }
// enemies.inc EnmTakePursuers: it gave up the chase in this room visit and has not taken it up again
static int EpIgnores(unsigned id)
{
    return EpLive() && g_epRoom.serial == g_enmDoorSerial && EpIgnoresYou(&g_epRoom, id, (int)g_enmPursuit, g_enmClock);
}
// one enemy (the list walk below)
static void EpOne(BYTE *t, const float *pp, const float *aim, int pmode)
{
    BYTE *w = EnmWork(t), *blk;
    const float *q = (const float *)w;
    unsigned id, mw;
    int type, variant, kind, fam, st, step, cls, extra = -1, act, why = 0, done = 0, copy;
    float dx, dy, dz;
    EpLook k;
    if (!w) return;
    type = *(const int *)(w + 0x20);
    if (type != 0x03 && type != 0x05 && type != 0x07 && type != 0x08 && type != 0x0A && type != 0x0B) return;
    if (!EnmFinite3(q)) return;
    mw = EnmModeWord(t);
    variant = (int)(mw >> 8 & 0xF), kind = (int)(mw & 0xF);
    fam = EpFamily(type, variant, kind, type == 0x05 && *(const int *)(w + 0x40) == 1);
    if (fam == EP_FAM_NONE || EnmIsBoss(t, w)) return;
    id = ENM_ID(t);
    if (FiHolds(t, id)) {  // v3.51 MERGE (EP351 x PT351): in Fade In's tar-portal hold (2.5 s): not judged, never set or moved;
        EpPortalHeld(&g_epRoom, id, g_enmClock);  // its timers start when the hold ends
        if (LogQuota(&g_epQuotaPortal, 200))
            LogT("enemies: %08X (type %02X) in room %d:%d is held by its tar portal (Fade In): Enemy Pursuit waits for the end of "
                 "the hold", id, type, ENM_STAGE, ENM_ROOM);
        return;
    }
    copy = EnmIsCopyId(id);
    if (fam == EP_FAM_GHOST && !copy && !EnmIsGhostId(id)) return;  // (every victim placement has a ghost id: not one of those)
    if (fam == EP_FAM_DOG && (kind == 6 || kind == 7) && !ENM_DOGS_AWAKE) return;  // (an event dog not released: no update runs)
    blk = EpBlock(w);
    st = *(const WORD *)(t + 0x410), step = *(const WORD *)(t + 0x412);
    cls = *(const int *)(w + 0x3C);
    if (fam == EP_FAM_DOG && blk) extra = *(const int *)(blk + 0x24);
    memset(&k, 0, sizeof k);
    k.hp = ENM_HPSAFE(t, -1.0f);
    dx = q[0] - pp[0], dy = q[1] - pp[1], dz = q[2] - pp[2];
    k.dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!_finite(k.dist)) k.dist = -1.0f;
    if (fam == EP_FAM_GHOST) {
        // a pinned or downed ghost (HP 0; an unknown HP too) is not chasing; in its down / wall classes it is never set
        k.chase = cls < 0 || cls == 0x0B || cls >= 0x10 || !(k.hp > 0.0f) ? EP_NOT : EpChase(fam, st, step, extra);
        if (k.chase == EP_CHASE_OK && (cls == 9 || cls == 0x0A || cls == 0x0D)) k.chase = EP_CHASE;
        k.touch = EpGhostTouchState(st);
        // v3.52 EP352: Forgetful: its idle float / a dive just begun, and whether anything is between it and you (only while
        // it homes on you in the open, or idles within its calm's range: there the line decides; not tested = in its sight)
        {
            const int up = !(cls < 0 || cls == 0x0B || cls >= 0x10 || !(k.hp > 0.0f));  // (not pinned, down or in a wall class)
            k.idle = up && st == 1;
            k.dive = up && st == 7 && step <= 2 && cls != 7;  // (class 7 = at its wall node: 0x485490, steps 3 / 4 on)
        }
        k.sees = 1;
        {
            EpRules R;
            if (aim && k.dist >= 0.0f && EpRulesOf(pmode, &R) &&
                ((R.sightT > 0.0f && k.chase == EP_CHASE_OK) || (R.gSense > 0.0f && k.chase == EP_NOT && k.dist < R.gSense))) {
                const float *eye = (const float *)(w + 0x10);  // its aim point (pos2), else its position
                k.sees = EpLineClear(EnmFinite3(eye) ? eye : q, aim) != 0;
            }
        }
    } else {
        const int dead = ENM_DEAD(t);
        // whom it is after: its target (Hummer +0x48, Gum Head +0xB4, dog +0x28 short, Twin +0x40, Wall Man +0x08; 0 = you,
        // 1 Eileen, 2 Cynthia): one after someone else is not chasing you
        const int toff = fam == EP_FAM_HUMMER ? 0x48 : fam == EP_FAM_GUMHEAD ? 0xB4 : fam == EP_FAM_TWIN ? 0x40 : fam == EP_FAM_WALLMAN ? 0x08 : 0;
        const int target = !blk ? -1 : fam == EP_FAM_DOG ? *(const short *)(blk + 0x28) : *(const int *)(blk + toff);
        k.down = !EnmStanding(cls) || dead;
        k.chase = dead || cls == 0x0B || target != 0 ? EP_NOT : EpChase(fam, st, step, extra);
        if (k.chase == EP_CHASE_OK && k.down) k.chase = EP_CHASE;  // (never set while it lies down)
        if (k.dist >= 0.0f && k.dist < EpNearRange(fam)) k.sees = 1;
        else if (!aim || k.dist < 0.0f) k.sees = 1;  // (her point or the distance unknown: counted as seeing you)
        else if (k.chase == EP_NOT && !k.down) k.sees = 0;  // (an idle one: no line test; only a chase or a down needs it)
        else if (k.dist < EpSenseRange(fam)) {
            const float *eye = (const float *)(w + 0x10);  // its aim point (pos2), else its position
            const int c = EpLineClear(EnmFinite3(eye) ? eye : q, aim);
            k.sees = c != 0;  // (not tested: counted as seeing you, nobody gives up for a busy ray)
        }
    }
    {
        const int stock0 = g_epRoom.nStock;
        act = EpStep(&g_epRoom, id, fam, pmode, g_enmClock, &k, &why);
        if (g_epRoom.nStock != stock0 && LogQuota(&g_epQuotaStock, 50))
            LogV("enemies: %s %08X (type %02X) in room %d:%d took the chase up again twice right after giving up (it notices you "
                 "where Refined's test says it cannot; AI state %d step %d, %.0f u away): the game's own rules have it for this room "
                 "visit", kEpFamName[fam], id, type, ENM_STAGE, ENM_ROOM, st, step, k.dist);  // v3.52 EP352: + its state
    }
    {   // v3.52 EP352: a Forgetful ghost's calm ended (LogV, quota'd)
        EpE *e = fam == EP_FAM_GHOST ? EpFind(&g_epRoom, id) : NULL;
        if (e && e->calmEnd) {
            const int ce = e->calmEnd;
            e->calmEnd = EP_CALM_NONE;
            if (LogQuota(&g_epQuotaCalm, 200))
                LogV("enemies: ghost %08X in room %d:%d %s (%.0f u away; AI state %d): its calm after the give-up ends (Enemy "
                     "Pursuit %s)", id, ENM_STAGE, ENM_ROOM,
                     ce == EP_CALM_SENSED ? "sensed you again and took up the chase" :
                     ce == EP_CALM_WOKEN ? "was woken (a hit, its touch or you within 400)" : "attacked of its own (a wall or a bump)",
                     k.dist, st, EfPursuitNameOf(pmode));
        }
    }
    if (act == EP_KEEP) return;
    if (act == EP_UNDIVE) {  // v3.52 EP352
        if (!EpUndive(t, w, blk, pp, step)) return;  // (not now: looked at again next tick)
        EpUndived(&g_epRoom, id);
        if (LogQuota(&g_epQuotaUndive, 100))
            LogV("enemies: ghost %08X in room %d:%d began a dive into a wall towards you in its rest (%.0f u away; AI state 7 step "
                 "%d): set back to its idle float (Enemy Pursuit %s)", id, ENM_STAGE, ENM_ROOM, k.dist, step,
                 EfPursuitNameOf(pmode));
        return;
    }
    if (act == EP_HOLD) {  // in its rest: its sight request goes (it was posted last frame for this frame's run)
        const void *sense = EpSenseOf(fam, blk);
        if (sense) EP_UNPOST(EP_SENSECHECK, sense);
        if (fam == EP_FAM_GHOST) EpGhostHome(t, w, blk);
        return;
    }
    if (fam == EP_FAM_DOG) {  // the dog's own give-up: timer[0] (+0x66) at 0, it gives up at its next step
        if (!blk || st != 4 || (extra != 0 && extra != 1)) return;
        *(short *)(blk + 0x66) = 0;
        done = 1;
    } else {
        done = EpSetState(t, EpGiveUpState(fam, st));
    }
    if (!done) return;
    if (act == EP_REAPPLY) return;
    if (act == EP_AGAIN) {
        EpHeld(&g_epRoom);
        if (fam == EP_FAM_GHOST) EpPutIdle(&g_epRoom, id);  // v3.52 EP352: set to its idle float (a dive from it may be set back)
        return;
    }
    {   // EP_GIVEUP
        EpE *e = EpFind(&g_epRoom, id);
        const float secs = !e ? 0.0f : fam == EP_FAM_GHOST ? EpGhostSecs(pmode, why) : g_enmClock - e->seen;
        const int again = EpCommit(&g_epRoom, id, pmode, g_enmClock, fam == EP_FAM_DOG, why);
        EnmPursuer *p = EnmPursueFind(&g_enmPursue, id);
        if (fam == EP_FAM_GHOST) ++g_epGiveGhost;
        else ++g_epGiveMon;
        EpSay(copy ? (fam == EP_FAM_GHOST ? "ghost copy" : "copy") : fam == EP_FAM_GHOST ? "ghost" : "monster", id, type, variant,
              fam, st, EpGiveUpState(fam, st), why, again, pmode, secs);
        if (p) {  // (its pursuit of you ends: it does not come through your next door)
            p->used = 0;
            if (p->copy) EnmTransitHome(p->id, p->stage);
        }
    }
}
// the tick (EnmTickC, normal play, any Enemies Follow value)
static void EpTick(const float *pp)
{
    const int pmode = (int)g_enmPursuit;
    const BYTE *t = NULL;
    const float *aim = NULL;
    int guard = 0;
    if (!EpLive()) {
        if (g_epRoom.n) EpRoomEnd("Enemy Pursuit is Relentless now");
        return;
    }
    if (g_epRoom.serial != g_enmDoorSerial) {  // a new room visit (EnmDoorPass ended the last one): no timer carries over
        if (g_epRoom.nMon + g_epRoom.nGhost) EpRoomEnd("a room change");
        EpRoomReset(&g_epRoom, g_enmDoorSerial);
        g_epRoomStage = ENM_STAGE, g_epRoomNo = ENM_ROOM;
    }
    if (g_enmClock == g_epClockSeen) return;  // (no play time passed: a pause, a menu, an event that stops the room timer)
    g_epClockSeen = g_enmClock;
    {
        const float *a = EP_AIMPOINT();
        if (a && EnmFinite3(a)) aim = a;
    }
    while ((t = ENM_NEXT(t)) != NULL && guard++ < 96) EpOne((BYTE *)t, pp, aim, pmode);
    if (g_epRoom.full && !g_epSaid++)
        LogAnomaly("enemies", "Enemy Pursuit's room table is full (%d enemies in room %d:%d): the rest keep the game's own chase",
                   EP_N, ENM_STAGE, ENM_ROOM);
}
// install (EnemiesInstall, after EfInstall): the stock functions it calls (never patched)
static void EpInstall(void)
{
    static const Site kFns[] = {
        { 0x45B5F0, { 0x8B, 0x44, 0x24, 0x04, 0x66, 0x8B, 0x4C, 0x24, 0x08 }, 9 },        // EnemySetAiState
        { 0x45BBF0, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x88, 0x08, 0x04, 0x00, 0x00 }, 10 },  // the request gate
        { 0x462800, { 0x56, 0x68, 0xE0, 0xEF, 0xFC, 0x00, 0xE8, 0xA5, 0xE7, 0x0E }, 10 },  // withdraw deferred actions
        { 0x45B4D0, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0xB6, 0xFD, 0x0C, 0x00 }, 10 },  // its spawn point
        { 0x46E540, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x1C }, 9 },        // the sense records' check
        { 0x454000, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x28 }, 9 },        // the line test
        { 0x46B330, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x83, 0xF8, 0xFF }, 9 },        // her aim point
    };
    int i;
    const char *bad = NULL;
    char b[16];
    EpRoomReset(&g_epRoom, 0);
    if (!g_iniFallback) g_epIniOn = GetPrivateProfileIntA("Gameplay", "PursuitInRoom", 1, g_iniPath) != 0;
    g_epFns = 1;
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i)
        if (!SiteOk(&kFns[i])) {
            g_epFns = 0;
            sprintf_s(b, sizeof b, "%08lX", kFns[i].va);
            bad = b;
            break;
        }
    if (!g_epIniOn) Log("enemies: Enemy Pursuit in the room off ([Gameplay] PursuitInRoom=0): only the door rules act");
    else if (!g_epFns)
        Log("enemies: Enemy Pursuit in the room NOT in (%s is not the stock function: another mod?): only the door rules act", bad);
    else {  // v3.52 EP352: the ghosts' numbers from the rules; + Forgetful's sight rule and calm
        EpRules n, f;
        EpRulesOf(EF_PURSUIT_NORMAL, &n);
        EpRulesOf(EF_PURSUIT_FORGETFUL, &f);
        Log("enemies: Enemy Pursuit in the room in (Normal / Forgetful: monsters give up after %.0f / %.0f s unsensed, ghosts after "
            "%.0f / %.0f s far (%.0f / %.0f u) or %.0f / %.0f s without touching you, Forgetful's also after %.0f s out of their "
            "sight, then rest %.0f / %.0f s and stay calm until they sense you again (%.0f / %.0f u, nothing between); "
            "Hummer, Gum Head, Sniffer Dog, Twin Victim, Wall Man, victims' ghosts; Relentless = stock)", EF_MEMORY_NORMAL,
            EF_MEMORY_FORGETFUL, n.farT, f.farT, n.farR, f.farR, n.chaseT, f.chaseT, f.sightT, n.gRest, f.gRest, n.gSense, f.gSense);
    }
}
