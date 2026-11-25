// ================================================================ v3.27.0 Death Presence (special-mod round 2 of 5)
// Plan and review: staging/v327_death_presence/PLAN.md (claudex-loop, Codex), PLAN-REVIEW-LOG.md. RE: re_allies_ghosts.md,
// re_room302.md, re_avoid.md (this round) and staging/special_mods/re_special.md. Included by sh4windowmod.c after
// screenfx.inc. GOG v7 VAs. The pure state code (ramp, victims, avoidance cycle, Eileen gate, ghost hold) is dp_sim.h.
//
// A black-violet fog around the player drains every being inside it except the player, more the longer it stays:
// monsters and ghosts through the game's own HP calls (0x44FE60 / 0x44FED0) and a last blow by an injected hit record
// (so their own death / down runs); companion Eileen a real haunting (0x43EB70 + the 1 % curse 0x43E9D0); Cynthia
// recoils (no HP path exists). Downed ghosts inside stay down (their running countdown is held). Monsters hesitate at
// its edge (DpCommit at 0x462987, before the mover commit pass 0x456C60), standing ghosts get the stock candle slow (an
// epsilon aura). Eileen and Cynthia keep back from it (their follow distances read g_dpStop / g_dpStart). In Room 302
// the fog drains the active hauntings and every haunting it finishes counts as cleared.
#include "dp_sim.h"

#define DP_AURA_EPS 1e-6f      // the ghost-slow marker (PLAN 7 B): no damage, never equal to a candle's aura
#define DP_DY       250.0f     // the fog's half height for the inside test (+Y is down)
#define DP_ALLY_K   0.25f      // v3.27.1 (the user, 2026-11-17): companion Eileen's haunting at 1/4 of the enemy rate
#define DP_ART_MAX  32

typedef struct { const BYTE *task; float pos[3], r; int kind, held; } DpArt;  // kind 0 monster, 1 ghost, 2 Eileen, 3 Cynthia
static DpTable g_dpTab;
static DpEileen g_dpEil;
static int g_dpInstalled;            // the set of sites is in (g_dpReady: gameplay.inc)
static volatile int g_dpFail;        // fail-closed (review DP-006): any fault, or a failed install
static float g_dpStop = 200.0f, g_dpStart = 500.0f;  // the companions' follow stop / restart (redirected operands)
static int g_dpWasOn;                // the last tick ran with a level (the off tick resets once)
static int g_dpOhkOn;                // the table's One Hit Kills is on at 0x44FEF2 (no ghost marker: review DP-005)
static int g_dpMedHook;              // the table's Stronger Medallions is on at 0x44FED7 (ghosts take 0x44FE60: DP-008)
static DpArt g_dpArt[DP_ART_MAX];    // what the fog draw shows around beings this tick (game thread, tick -> draw)
static int g_dpArtN;
static float g_dpHauntCol[15][4];    // Room 302: x, y, z, strength of a haunting being eaten (w 0 = none)
static int g_dpInR302;               // the last tick was in Room 302's sub-rooms
static float g_dpEileenR, g_dpCynR;  // ramp of the companions (art)
static int g_dpLogType;              // bit per type: first damage logged this room
static int g_dpLogged[8];            // one-shot log lines
static int g_dpRoom302;              // v3.42 SM342: hidden [ScreenEffects] DeathPresenceRoom302 (0: off in Room 302, dp_sim.h DpOffHere)
static int g_dpR302In = -1;          // v3.42 SM342: the last tick's stage was Room 302 (DpR302Enter: one line per entry)
static int DpOffNow(void) { return DpOffHere(*(volatile DWORD *)0xFD5A90, g_dpRoom302); }  // v3.42 SM342: off in this stage

static void DpLevelChanged(int v)  // gameplay.inc GpApply (any thread): the follow distances follow the level
{
    if (g_dpFail || v <= 0 || v >= DP_LEVELS || !g_dpInstalled) {
        g_dpStop = 200.0f;
        g_dpStart = 500.0f;
        return;
    }
    g_dpStop = kDpLevel[v].R + 50.0f;
    g_dpStart = kDpLevel[v].R + 350.0f;
}
static void DpFailNow(const char *where, DWORD code, const void *at)
{
    if (g_dpFail) return;
    g_dpFail = 1;
    g_dpReady = 0;  // review DP-I005: the menu shows "not available"
    g_dpStop = 200.0f;
    g_dpStart = 500.0f;
    g_dpArtN = 0;
    __try {  // review DP-I005: no marker of ours left behind (an exact DP_AURA_EPS only: a candle's is never equal)
        const BYTE *t = NULL;
        int guard = 0;
        while ((t = ((const BYTE *(__cdecl *)(const BYTE *))0x45E760)(t)) != NULL && guard++ < 96) {
            const BYTE *w = *(const BYTE *const *)(t + 0x408);
            float *hp = w ? *(float *const *)(w + 0x34) : NULL;
            const int type = w ? *(const int *)(w + 0x20) : 0;
            if (hp && ((type >= 0x0B && type <= 0x0F) || type == 0x11) && hp[5] == DP_AURA_EPS) hp[5] = 0.0f;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    memset(g_dpHauntCol, 0, sizeof g_dpHauntCol);
    Log("death presence: %s faulted (code %08lX at %p): the feature is off for this session (follow distances stock)",
        where, code, at);
}
static DWORD g_dpFaultCode;
static void *g_dpFaultAt;
static int DpFaultNote(EXCEPTION_POINTERS *e)
{
    g_dpFaultCode = e->ExceptionRecord->ExceptionCode;
    g_dpFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------- helpers
static const BYTE *DpNext(const BYTE *t) { return ((const BYTE *(__cdecl *)(const BYTE *))0x45E760)(t); }
static int DpFinite3(const float *p) { return _finite(p[0]) && _finite(p[1]) && _finite(p[2]); }
static int DpIsGhost(int type) { return (type >= 0x0B && type <= 0x0F) || type == 0x11; }
static int DpTypeOk(int type) { return type == 0x02 || type == 0x03 || (type >= 0x06 && type <= 0x0F) || type == 0x11; }
// does this task's update run in the coming task loop? (EntityGate: gated monsters only on 30 Hz ticks)
static int DpUpdatesNow(const BYTE *task)
{
    if (!g_gateHooks) return 1;
    return GateKind(*(const DWORD *)(task + 0x404)) != GK_MONSTER || g_gateTick;
}
static void DpArtAdd(const BYTE *task, const float *p, float r, int kind, int held)
{
    DpArt *a;
    if (g_dpArtN >= DP_ART_MAX) return;
    a = &g_dpArt[g_dpArtN++];
    a->task = task;
    memcpy(a->pos, p, sizeof a->pos);
    a->r = r;
    a->kind = kind;
    a->held = held;
}
// v3.27.1 (user item 3, PLAN 5.2): Thermal: a being in the art list is drawn with its body heat x (1 - 0.7 s) (ThermDrawC,
// the draw thread = the game thread, after this tick filled the list); 1 when not in the list or the feature is off
// v3.27.1 (review DP3271-R7-101 / R7-102): the art's exposure is its own ramp (seconds inside, falling 2 x as fast outside,
// as the damage ramp's shape), stepped on every tick the being is seen, whatever its damage state; the stain follows it.
// The damage ramp (tin, lastIn) is untouched by the art.
static float DpArtStep(float *artT, float *stain, int inside, float dt)
{
    DpRampStep(artT, inside, dt, 0.0f, 0.0f);
    return DpStainStep(stain, inside, DpRampFrac(*artT), dt);
}
static float DpHeatK(const BYTE *task)
{
    int i;
    if (!task || !g_dpInstalled || g_dpFail || g_dpLevel <= 0) return 1.0f;
    for (i = 0; i < g_dpArtN; ++i)
        if (g_dpArt[i].task == task) {
            const float s = g_dpArt[i].r;
            return s > 0.0f ? (s < 1.0f ? 1.0f - 0.7f * s : 0.3f) : 1.0f;
        }
    return 1.0f;
}
static void DpAuraClear(float *hp)  // an aura still exactly our marker goes back to 0 (a candle's is never equal)
{
    if (hp[5] == DP_AURA_EPS) hp[5] = 0.0f;
}

// v3.29 AFTERIMAGE: DpLastBlow's record writer, generalised (attack id, damage, push; the record's position and the
// point its direction is taken from) so the after-image's Stun Gun hit (afterimage.inc, attack 0x2A) uses the same
// code. DpLastBlow below calls it with exactly the v3.27 values (byte for byte the same record).
// One hit record in the live list 0xFCE8B0 (this tick's monster updates read it, this tick's 0x44F900 frees it):
// +0 pos (w 1), +0x10 the unit horizontal direction from `from` to `pos`, +0x20 id, +0x24 its reaction kind (0x4FDF40),
// +0x28 a fresh 0xBA1E serial, +0x2C damage, +0x30 push, +0x40 the task; the HP block's +0x1C serial / +0x20 id (as
// 0x450D90: kill credit, the Stun Gun's 3-10 s holds). 0 = no record (heap): nothing written.
static int DpInjectHit(const BYTE *task, float *hp, const float *pos, const float *from, int id, float damage, float push)
{
    BYTE *rec = ((BYTE *(__cdecl *)(DWORD))0x550D20)(0xFCE8B0);
    float dx, dz, len;
    DWORD s, serial;
    if (!rec) return 0;
    memset(rec, 0, 0x50);
    memcpy(rec, pos, 12);
    ((float *)rec)[3] = 1.0f;
    dx = pos[0] - from[0];
    dz = pos[2] - from[2];
    len = sqrtf(dx * dx + dz * dz);
    if (len >= 1.0f) {
        ((float *)rec)[4] = dx / len;
        ((float *)rec)[6] = dz / len;
    } else {
        ((float *)rec)[6] = 1.0f;
    }
    *(int *)(rec + 0x20) = id;
    *(int *)(rec + 0x24) = ((int(__cdecl *)(int))0x4FDF40)(id);  // the reaction kind of the attack (as 0x44F410)
    s = *(volatile DWORD *)0x10834BC;  // the serial as 0x4FE100 makes it, the counter advanced
    serial = (s & 0xFFFF) | 0xBA1E0000u;
    *(volatile DWORD *)0x10834BC = s + 1;
    *(DWORD *)(rec + 0x28) = serial;
    *(float *)(rec + 0x2C) = damage;
    *(float *)(rec + 0x30) = push;
    *(const BYTE **)(rec + 0x40) = task;
    *(DWORD *)((BYTE *)hp + 0x1C) = serial;  // the HP block: the last serial and attack (as 0x450D90; kill credit)
    *(int *)((BYTE *)hp + 0x20) = id;
    return 1;
}
// The last blow (PLAN 5, re_special A3, review DP-001): attack 0x08 (the bat: kill credit, kind 4, push 40), damage
// hp + 1, at the work position, its direction from the player.
static int DpLastBlow(const BYTE *task, BYTE *work, float *hp, const float *pp)
{
    if (!DpInjectHit(task, hp, (const float *)work, pp, 0x08, hp[0] + 1.0f, 40.0f)) {  // v3.29 AFTERIMAGE: the shared writer
        if (!g_dpLogged[0]++) Log("death presence: no hit record could be allocated (heap): the last blow waits");
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------- monsters and ghosts (PLAN 5, 7 B)
static void DpMonsters(int level, float dt, const float *pp)
{
    const DpLevel *L = &kDpLevel[level];
    const float R2 = L->R * L->R;
    const BYTE *t = NULL;
    int guard = 0;
    while ((t = DpNext(t)) != NULL && guard++ < 96) {
        BYTE *w = *(BYTE *const *)(t + 0x408);
        float *hp, pos[3], dx, dz, dy, frac, amount;
        int type, cls, inside, ghost, upd;
        DpVictim *v;
        if (!w) continue;
        type = *(const int *)(w + 0x20);
        if (!DpTypeOk(type)) continue;  // never 1, 4, 5 (Wall Man / the Wall boss), 0x10 (Walter, the Bodies), 0x12+
        cls = *(const int *)(w + 0x3C);
        hp = *(float *const *)(w + 0x34);
        if (!hp || !(hp[1] > 0.0f) || !_finite(hp[0])) continue;
        memcpy(pos, w, sizeof pos);  // the work position (KB 138)
        if (!DpFinite3(pos)) continue;
        dx = pos[0] - pp[0];
        dz = pos[2] - pp[2];
        dy = pos[1] - pp[1];
        inside = dx * dx + dz * dz < R2 && fabsf(dy) < DP_DY;
        ghost = DpIsGhost(type);
        v = DpFind(&g_dpTab, t, w);
        if (!v) continue;
        upd = DpUpdatesNow(t);
        if (cls != v->lastCls) {  // one last blow per class (a new down / rise / death starts afresh)
            if (v->lastCls == 0x0B) {  // review DP-R401: dead before, alive now: a re-made being, clean state
                memset(&v->av, 0, sizeof v->av);
                v->tin = v->pend = v->stain = v->artT = 0.0f, v->hold = 0;
            }
            v->blowSent = 0;
            v->lastCls = cls;
        }
        if (cls == 0x0B || cls >= 0x10) {  // dead, removed or scripted
            if (ghost) DpAuraClear(hp);
            // v3.27.1 (user item 2): a fog kill keeps its stain on the body while it fades (dead only, not scripted)
            if (cls == 0x0B && DpArtStep(&v->artT, &v->stain, 0, dt) > DP_STAIN_MIN) DpArtAdd(t, pos, v->stain, ghost, 0);
            continue;
        }
        if (cls == 9 || cls == 0x0A) {  // down: no damage; a ghost's running countdown is held while inside (PLAN 5 G)
            if (ghost) {
                BYTE *gw = *(BYTE *const *)(w + 0x28);
                DpAuraClear(hp);
                if (gw) {
                    float *cd = (float *)(gw + 0x98);
                    const float hv = DpHoldStep(v, inside, *cd, *(const DWORD *)((const BYTE *)hp + 0x1C));
                    if (hv >= 0.0f) {
                        *cd = hv;
                        if (!g_dpLogged[1]++) Log("death presence: a downed ghost (type %02X) is held down in the fog (%.1f s left)", type, hv);
                    }
                }
                DpArtStep(&v->artT, &v->stain, inside, dt);
                if (inside && gw) v->stain = 1.0f;  // v3.27.1: held down inside = the full stain
            } else {
                DpArtStep(&v->artT, &v->stain, inside, dt);  // a downed monster: its own art ramp (DP3271-R7-002 / R7-101)
            }
            if (v->stain > DP_STAIN_MIN) DpArtAdd(t, pos, v->stain, ghost, ghost ? v->hold : 0);
            continue;
        }
        v->hold = 0;
        DpVictimSeen(v, inside, g_dpTab.clock);  // review DP-I002
        frac = DpRampStep(&v->tin, inside, dt, L->lo, L->hi);  // (0 outside: no new accrual)
        if (!inside && ghost) DpAuraClear(hp);
        // v3.27.1 (user item 2): the art follows the stain (the ramp inside, a 3 s half-life after it leaves: B4)
        if (DpArtStep(&v->artT, &v->stain, inside, dt) > DP_STAIN_MIN) DpArtAdd(t, pos, v->stain, ghost, 0);
        // review DP-007: damage and the last blow are one step on the tick the target's own update runs; between, the
        // amount is carried (capped at 0.25 s of the full rate); review DP-I003: what was accrued inside is applied on
        // that update tick even if it has stepped out since
        v->pend += frac;
        if (v->pend > 0.25f * L->hi) v->pend = 0.25f * L->hi;
        if (!upd || !(v->pend > 0.0f)) continue;
        amount = v->pend * hp[1];
        v->pend = 0.0f;
        if (type < 32 && !(g_dpLogType >> type & 1)) {
            g_dpLogType |= 1u << type;
            Log("death presence: first damage on type %02X (hp %.0f / %.0f, %.1f %% of max a second now)", type, hp[0], hp[1],
                dt > 0.0f ? 100.0f * frac / dt : 0.0f);
        }
        {
            BYTE snap[0x30];
            int lethal = !(hp[0] - amount > 0.5f), called = 0;
            memcpy(snap, hp, sizeof snap);
            if (!lethal) {  // review DP-008: the holy path only while the table's Stronger Medallions is not at 0x44FED7
                if (ghost && !g_dpMedHook) ((float(__cdecl *)(const BYTE *, float))0x44FED0)(t, amount);
                else ((float(__cdecl *)(const BYTE *, float))0x44FE60)(t, amount);
                called = 1;
                if (hp[0] > 0.0f) {  // not lethal after all calls (the table's One Hit Kills zeroes it: then the blow)
                    // PLAN 7 B: the stock candle slow for standing ghosts (not 0x11: no reader); this is an update tick
                    if (ghost && inside && type != 0x11 && !g_dpOhkOn && hp[0] > 1.0f && !(hp[5] > 0.0f)) hp[5] = DP_AURA_EPS;  // DP-R603: inside
                    continue;
                }
            }
            if (ghost) DpAuraClear(hp);  // never a marker under a lethal record (review DP-002)
            if (v->blowSent) continue;
            if (((int(__cdecl *)(const BYTE *))0x450F30)(t)) {  // a damaging real hit is queued this tick
                // review DP-009: at hp <= 0 it carries the death; above 0 it may not kill: the fog's amount stays
                // pending (not applied, not lost) and the blow is decided again on the next update tick
                if (hp[0] > 0.0f && !called) {
                    v->pend += amount / hp[1];
                    if (v->pend > 0.25f * L->hi) v->pend = 0.25f * L->hi;
                }
                continue;
            }
            if (DpLastBlow(t, w, hp, pp)) {
                v->blowSent = 1;
                if (!g_dpLogged[2]++) Log("death presence: first last blow (type %02X, %.1f hp): its own death runs", type, hp[0]);
            } else {  // no record: the HP call is undone and the amount goes back to pending (review DP-I206), retried
                if (called) memcpy(hp, snap, sizeof snap);  // on the next update tick (bounded by the 0.25 s cap)
                v->pend += amount / hp[1];
                if (v->pend > 0.25f * L->hi) v->pend = 0.25f * L->hi;
            }
        }
    }
}

// ---------------------------------------------------------------- companion Eileen: a full haunting (PLAN 4, E1)
static __declspec(naked) void __cdecl DpEileenDamage(void *w, float d)  // 0x43EB70: ecx = w, one float, the caller pops
{
    __asm {
        mov ecx, [esp + 4]
        push dword ptr [esp + 8]
        mov eax, 43EB70h
        call eax
        add esp, 4
        ret
    }
}
static void DpEileenTick(int level, float dt, const float *pp)
{
    static int s_lastFrames, s_logged;
    BYTE *t = *(BYTE *const *)0xFCE6B4, *w, *mdl;
    const DpLevel *L = &kDpLevel[level];
    float pos[3], dx, dz, dy, frac, before;
    float **hpp;
    WORD st;
    int inside, fr, gate, event;
    g_dpEileenR = 0.0f;
    if (g_dpTab.clock - g_dpEil.lastIn > DP_FORGET_S) g_dpEil.tin = 0.0f;  // review DP-I204: away / outside 3 s = forgotten
    if (!t || *(const DWORD *)(t + 0x404) != 0x44ADE0 || (w = *(BYTE *const *)(t + 0x408)) == NULL) {
        g_dpEil.acc = 0.0f;
        return;
    }
    st = *(const WORD *)(t + 0x410);
    hpp = (float **)w;
    if (!st || !hpp[0] || !hpp[1]) return;  // not built yet
    if (*(const int *)(w + 0xF0) || !*(volatile int *)0xFD5BC4) {  // in another room / not in the party
        g_dpEil.acc = 0.0f;
        return;
    }
    // the damage gate first, independent of her model (review DP3271-R2-002): down = no damage, then the grace after
    gate = DpEileenGate(&g_dpEil, st == 0x14 || st == 0x15 || *(volatile int *)0x10A1D04 == 10, dt);  // down: let her up
    event = *(const BYTE *)(w + 0x28C) != 0;  // the stock gate: no haunting HP in her event state 0x13
    if (gate && event) g_dpEil.acc = 0.0f;
    mdl = *(BYTE *const *)(w + 0x18);
    if (!mdl) return;
    memcpy(pos, mdl + 0x10, sizeof pos);
    if (!DpFinite3(pos)) return;
    dx = pos[0] - pp[0];
    dz = pos[2] - pp[2];
    dy = pos[1] - pp[1];
    inside = dx * dx + dz * dz < L->R * L->R && fabsf(dy) < DP_DY;
    // v3.27.1 (B4; review DP3271-002): the art lingers and fades on every tick her model is valid, also while she is down,
    // in her grace or in an event (the gate decides only the haunting)
    g_dpEileenR = DpArtStep(&g_dpEil.artT, &g_dpEil.stain, inside, dt);  // its own exposure (review DP3271-R7-102)
    if (g_dpEileenR > DP_STAIN_MIN) DpArtAdd(t, pos, g_dpEileenR, 2, 0);
    if (!gate || event) return;
    frac = DP_ALLY_K * DpRampStep(&g_dpEil.tin, inside, dt, L->lo, L->hi);  // (0 outside); v3.27.1: 1/4 of the enemy rate
    if (inside) g_dpEil.lastIn = g_dpTab.clock;  // (as v3.27.0: the damage ramp's own bookkeeping, after the gate)
    // carried while her task did not run (capped: 0.25 s at the full rate); review DP-I302: what she took inside is
    // applied at her next update even if she stepped out since; it is discarded only when she is down, in another room,
    // out of the party, or the room changes (the paths above)
    g_dpEil.acc += frac;
    if (!(g_dpEil.acc > 0.0f)) return;
    if (g_dpEil.acc > 0.25f * L->hi) g_dpEil.acc = 0.25f * L->hi;
    fr = *(const int *)(w + 0xA4);
    if (fr == s_lastFrames) return;
    s_lastFrames = fr;
    {
        const float d = g_dpEil.acc * *hpp[1];  // x her max HP
        g_dpEil.acc = 0.0f;
        if (!(d > 0.0f)) return;
        before = *(const float *)(w + 0x14);
        DpEileenDamage(w, d);
        if (*(const float *)(w + 0x14) != before) {  // Playable Eileen's ghost Invulnerable swallows the call: no curse
            ((void(__cdecl *)(void *, float))0x43E9D0)(w, 0.01f * d);
            if (!s_logged++) Log("death presence: Eileen takes a haunting in the fog (%.2f hp this tick, hp %.1f / %.1f)", d, *hpp[0], *hpp[1]);
        }
    }
}

// ---------------------------------------------------------------- Cynthia: startle and back off, no HP (PLAN 4, C)
static void DpCySet(BYTE *t, int s)  // = 0x4EB780 (matched) plus w+0x210 (re_allies_ghosts 2.2)
{
    BYTE *w = *(BYTE *const *)(t + 0x408);
    if (*(const WORD *)(t + 0x410) != s) {
        *(WORD *)(t + 0x410) = (WORD)s;
        *(WORD *)(t + 0x412) = 0;
        w[0x214] = 1;
    }
    w[0x210] = (BYTE)s;
}
typedef struct { float startle, back, tin, lastIn; int wasInside; const BYTE *task; float stain; } DpCyState;  // v3.27.1: stain
static DpCyState g_dpCy;  // review DP-R501: reset with the feature (Off), a room change, or a new task of hers
static void DpCynthia(int level, float dt, const float *pp)
{
#define s_startle g_dpCy.startle
#define s_back g_dpCy.back
#define s_tin g_dpCy.tin
#define s_lastIn g_dpCy.lastIn
#define s_wasInside g_dpCy.wasInside
    static int s_logged;
    BYTE *t, *w;
    float dx, dz, d, pos[3];
    WORD st;
    int in;
    const float R = kDpLevel[level].R;
    g_dpCynR = 0.0f;
    s_startle -= dt;
    s_back -= dt;
    if (((int(__cdecl *)(void))0x4ED460)() != 1) {
        s_wasInside = 0;
        return;
    }
    t = *(BYTE *const *)0xFD5A50;
    if (!t || *(const DWORD *)(t + 0x404) != 0x4ED000 || (w = *(BYTE *const *)(t + 0x408)) == NULL || !*(void *const *)(w + 0x18))
        return;
    if (g_dpCy.task != t) {  // a new task of hers: clean state
        memset(&g_dpCy, 0, sizeof g_dpCy);
        g_dpCy.task = t;
        g_dpCy.lastIn = g_dpTab.clock;
    }
    memcpy(pos, *(BYTE *const *)(w + 0x18) + 0x10, sizeof pos);
    if (!DpFinite3(pos)) return;
    dx = pos[0] - pp[0];
    dz = pos[2] - pp[2];
    d = sqrtf(dx * dx + dz * dz);
    in = d < R && fabsf(pos[1] - pp[1]) < DP_DY;  // review DP-I006: the same cylinder as every being
    st = *(const WORD *)(t + 0x410);
    if (g_dpTab.clock - s_lastIn > DP_FORGET_S) s_tin = 0.0f;  // review DP-I203: her shimmer follows the time-in ramp
    DpRampStep(&s_tin, in, dt, 0.0f, 0.0f);
    if (in) s_lastIn = g_dpTab.clock;
    g_dpCynR = DpStainStep(&g_dpCy.stain, in, DpRampFrac(s_tin), dt);  // v3.27.1: the art lingers (B4)
    if (g_dpCynR > DP_STAIN_MIN) DpArtAdd(t, pos, g_dpCynR, 3, 0);
    if (st < 2 || st == 5 || st == 6) {  // 0 not built, 1 her scripted walk-off; 5 / 6 a recoil playing
        s_wasInside = in;
        return;
    }
    if (in) {
        if (!s_wasInside && s_startle <= 0.0f) {  // entering: startled (her cry), steps away from the player
            memcpy(w + 0xC0, w + 0x60, 16);   // contact point = the player's position as she sees it
            DpCySet(t, 6);
            s_startle = 4.0f;
            s_back = 0.6f;
            if (!s_logged++) Log("death presence: Cynthia recoils from the fog");
        } else if (s_back <= 0.0f) {  // then a back-off every 0.6 s while inside (time-based: her timer is per tick)
            DpCySet(t, 5);
            s_back = 0.6f;
        }
    }
    s_wasInside = in;
#undef s_startle
#undef s_back
#undef s_tin
#undef s_lastIn
#undef s_wasInside
}

// ---------------------------------------------------------------- Room 302: the hauntings fade (PLAN 6, re_room302)
#define DP_H_REC(i) ((BYTE *)0x10A5100 + 0x40 * (i))
#define DP_H_DEF(i) ((const BYTE *)0x697C40 + 0x50 * (i))
static const float kDpHauntRate[DP_LEVELS] = { 0.0f, 111.0f, 250.0f };  // intensity a second
static void DpRoom302(int level, float dt, const float *pp)
{
    static int s_logged[15];
    DWORD room;
    int i;
    memset(g_dpHauntCol, 0, sizeof g_dpHauntCol);
    g_dpInR302 = 0;
    if (!(*(volatile BYTE *)0x13D8143 & 0x80)) return;  // the second half: the haunting system is on
    if (!*(volatile int *)0x10A5624) return;            // enabled (0x547820)
    if (*(volatile int *)0x10A1E98) return;             // a room change is pending
    room = *(volatile DWORD *)0xFD5A94;
    if (room < 1 || room > 4) return;
    g_dpInR302 = 1;
    for (i = 0; i < 15; ++i) {
        BYTE *h = DP_H_REC(i);
        const float *c = (const float *)(h + 0x10);
        float I, take, dx, dz, core, reach;
        if (*(volatile int *)(h + 0x28) != 1) {  // active only (no double end); log the end once
            if (s_logged[i] == 1 && *(volatile int *)(h + 0x28) == 2) {
                s_logged[i] = 2;
                Log("death presence: Room 302 haunting #%d ended (removed %.0f: %s)", i, *(volatile float *)(h + 0x24),
                    *(volatile float *)(h + 0x24) >= 3000.0f ? "cleared" : "expired");
            }
            continue;
        }
        if (!(*(const DWORD *)(DP_H_DEF(i) + 0x3C) & (1u << room))) continue;
        if (i == 5 && (*(volatile BYTE *)0x13D8165 & 0x10)) continue;  // frozen by the front door, as stock
        I = *(volatile float *)(h + 0x20);
        if (!_finite(I) || !(I > 0.0f) || !_finite(c[0]) || !_finite(c[2])) continue;
        core = (float)*(const int *)(DP_H_DEF(i) + 0x38) - 250.0f;
        reach = kDpLevel[level].R + (core > 0.0f ? core : 0.0f);
        dx = pp[0] - c[0];
        dz = pp[2] - c[2];
        if (dx * dx + dz * dz >= reach * reach) continue;
        take = kDpHauntRate[level] * dt;
        if (take >= I) {
            take = I;
            if (*(volatile float *)(h + 0x24) + take < 3000.0f)  // the user: every fog kill counts as cleared
                *(volatile float *)(h + 0x24) = 3000.0f - take;
        }
        *(volatile float *)(h + 0x24) += take;
        *(volatile float *)(h + 0x20) = I - take;
        if (i == 4 && !((int(__cdecl *)(void))0x54A660)()) ((void(__cdecl *)(void))0x54A670)();  // the sofa, as stock
        g_dpHauntCol[i][0] = c[0];
        g_dpHauntCol[i][1] = c[1];
        g_dpHauntCol[i][2] = c[2];
        g_dpHauntCol[i][3] = 1.0f;
        if (!s_logged[i]) {
            s_logged[i] = 1;
            Log("death presence: Room 302 haunting #%d is being drained (intensity %.0f, removed %.0f)", i, I,
                *(volatile float *)(h + 0x24));
        }
    }
}

// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread)
static void DpTickC(void)
{
    const int level = (int)g_dpLevel;
    const int rate = *(volatile int *)0x5D5248;
    DWORD roomKey;
    float pp[3], dt;
    if (level <= 0 || level >= DP_LEVELS) {
        if (g_dpWasOn) {
            const BYTE *t = NULL;
            int guard = 0;
            g_dpWasOn = 0;
            g_dpArtN = 0;
            memset(g_dpHauntCol, 0, sizeof g_dpHauntCol);
            while ((t = DpNext(t)) != NULL && guard++ < 96) {  // no marker left behind
                const BYTE *w = *(const BYTE *const *)(t + 0x408);
                float *hp = w ? *(float *const *)(w + 0x34) : NULL;
                if (w && hp && DpIsGhost(*(const int *)(w + 0x20))) DpAuraClear(hp);
            }
            DpTableReset(&g_dpTab, 0xFFFFFFFFu);
            memset(&g_dpEil, 0, sizeof g_dpEil);
            memset(&g_dpCy, 0, sizeof g_dpCy);  // review DP-R501
        }
        return;
    }
    g_dpArtN = 0;
    g_dpInR302 = 0;  // review DP-I301: the Room 302 columns exist only on a tick that drained (DpRoom302 fills them)
    memset(g_dpHauntCol, 0, sizeof g_dpHauntCol);
    if (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0) return;  // gameplay only, no movie
    {   // v3.42 SM342 (USER_FEEDBACK_v341 item 7): off in Room 302 (stages 1 and 0x0A): no fog, no haunting drained
        const DWORD stage = *(volatile DWORD *)0xFD5A90;
        if (DpR302Enter(&g_dpR302In, stage))
            LogV("death presence: Room 302 (stage %lX room %lX): %s", stage, *(volatile DWORD *)0xFD5A94 & 0xFFFF, g_dpRoom302 ?
                 "on ([ScreenEffects] DeathPresenceRoom302=1: the fog and the hauntings' clearing as in v3.41)" :
                 "off here (no fog; the hauntings run as stock; [ScreenEffects] DeathPresenceRoom302=1 brings it back)");
        if (DpOffHere(stage, g_dpRoom302)) return;
    }
    if (rate < 1 || rate > 240) return;
    if (!*(void *volatile *)0xFD5BA0) return;
    memcpy(pp, (const void *)0x10A0F60, sizeof pp);
    if (!DpFinite3(pp)) return;
    dt = 1.0f / (float)rate;
    g_dpWasOn = 1;
    g_dpOhkOn = memcmp((const void *)0x44FEF2, "\x7A\x13\x33\xC0\xDD\xD8", 6) != 0;
    g_dpMedHook = memcmp((const void *)0x44FED7, "\xB8\x00\x00\x8B\xC8\xD9\x01\x83\xC4\x04", 10) != 0;
    if (g_dpMedHook && !g_dpLogged[4]++) Log("death presence: 0044FED7 is hooked (the cheat table's Stronger Medallions?): ghosts take the plain HP call");
    if (g_dpOhkOn && !g_dpLogged[3]++) Log("death presence: 0044FEF2 is hooked (the cheat table's One Hit Kills?): standing ghosts are not slowed");
    roomKey = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (roomKey != g_dpTab.roomKey) {
        DpTableReset(&g_dpTab, roomKey);
        memset(&g_dpEil, 0, sizeof g_dpEil);  // review DP-I204: a new room: the companions' exposure starts clean
        memset(&g_dpCy, 0, sizeof g_dpCy);    // review DP-R501
        g_dpEil.lastIn = g_dpTab.clock;
        g_dpLogType = 0;
    }
    g_dpTab.clock += dt;
    if (*(volatile DWORD *)0xFD5A90 == 1) {  // Room 302 (stage 1): its own branch
        DpRoom302(level, dt, pp);
        return;
    }
    g_dpInR302 = 0;
    DpMonsters(level, dt, pp);
    DpEileenTick(level, dt, pp);
    DpCynthia(level, dt, pp);
}
static void DpTick(void)
{
    if (!g_dpInstalled || g_dpFail) return;
    __try {
        DpTickC();
    } __except (DpFaultNote(GetExceptionInformation())) {
        DpFailNow("the tick", g_dpFaultCode, g_dpFaultAt);
    }
}

// ---------------------------------------------------------------- avoidance: before the mover commit (PLAN 7 A2)
static int g_dpAvoidLog[32];
static void DpAvoidShape(void)
{
    const int level = (int)g_dpLevel;
    const int rate = *(volatile int *)0x5D5248;
    const DpAvoidK *K;
    const float *pp = (const float *)0x10A0F60;
    float R;
    int driftOk, guard = 0;
    BYTE *m;
    if (level <= 0 || level >= DP_LEVELS || *(volatile int *)0xFD5C20 != 0 || rate < 1 || rate > 1000 || !DpFinite3(pp)) return;
    if (*(volatile int *)0xECB1B4 != 0 || !*(void *volatile *)0xFD5BA0) return;  // review DP-I205: the tick's gates (movie, player)
    if (*(volatile DWORD *)0xFD5A90 == 1 || DpOffNow()) return;  // Room 302 (v3.42 SM342: + 0x0A while off there)
    K = &kDpAvoid[level];
    R = kDpLevel[level].R;
    driftOk = *(const BYTE *)0x456D7B == 0x75;  // the table's 780 changes this jump: no drift then
    for (m = *(BYTE *const *)0xFCEF44; m && guard < 96; m = *(BYTE *const *)(m + 0x9C), ++guard) {
        BYTE *t = *(BYTE *const *)(m + 0x54), *w;
        float dx, dz, dy, d, nx, nz, h, vin, dtS, *disp;
        int type, cls, mode, gk;
        DpVictim *v;
        if (!TaskSlotOk(t)) continue;  // v3.36 TP336: a slot of the task pool (pools_sim.h)
        {   // a monster's task (its update function in the gate table), whatever EntityGate says
            const DWORD fn = *(const DWORD *)(t + 0x404);
            int i, isMon = 0;
            for (i = 0; i < NGATEFN; ++i)
                if (kGateFn[i].fn == fn) {
                    isMon = kGateFn[i].kind == GK_MONSTER;
                    break;
                }
            if (!isMon) continue;
            gk = g_gateHooks ? GateKind(fn) : GK_NONE;
        }
        w = *(BYTE *const *)(t + 0x408);
        if (!w || *(BYTE *const *)(w + 0x38) != m) continue;  // really this enemy's mover
        type = *(const int *)(w + 0x20);
        cls = *(const int *)(w + 0x3C);
        mode = *(const int *)(m + 0x50);
        if (!(type == 0x02 || type == 0x03 || (type >= 0x06 && type <= 0x0A) || type == 0x11) || mode == 8) continue;
        if (gk == GK_MONSTER && !g_gateTick) continue;  // review DP-I001: a gated monster is shaped on its own update ticks only
        dtS = gk == GK_MONSTER && rate > 30 ? 1.0f / 30.0f : 1.0f / (float)rate;  // the mover's own step
        v = DpFind(&g_dpTab, t, w);
        if (!v) continue;
        dx = *(const float *)(w + 0) - pp[0];
        dz = *(const float *)(w + 8) - pp[2];
        dy = *(const float *)(w + 4) - pp[1];
        if (!_finite(dx) || !_finite(dz) || !_finite(dy)) continue;
        d = sqrtf(dx * dx + dz * dz);
        {
            const float *hp = *(float *const *)(w + 0x34);
            const DWORD ser = hp ? *(const DWORD *)((const BYTE *)hp + 0x1C) : 0;
            const int hit = hp && ser != v->avSerial && (ser >> 16) == 0xBA1E;  // her attacks (and ours) carry 0xBA1E serials
            if (hp) v->avSerial = ser;
            DpAvoidStep(&v->av, K, d < R && fabsf(dy) < DP_DY, cls == 5, hit, dtS, &v->rng);
        }
        if (!(d < R) || d < 1.0f || fabsf(dy) >= DP_DY) continue;
        if (cls < 1 || cls > 4) continue;  // shape the moving classes only (5 attack, >= 8 down / dead, 0x12 scripted)
        nx = dx / d;
        nz = dz / d;
        h = DpAvoidCut(&v->av, K, R, d);
        disp = (float *)(m + 0x64);
        vin = -(disp[0] * nx + disp[2] * nz);  // the inward part of this step
        if (vin > 0.0f) {
            disp[0] += h * vin * nx;
            disp[2] += h * vin * nz;
        }
        if (v->av.phase == AV_HESITATE && mode != 7 && driftOk) {
            const float u = h / (K->hMax > 0.0f ? K->hMax : 1.0f), out = K->vBack * u * dtS;
            disp[0] += out * nx;
            disp[2] += out * nz;
        }
        if (type < 32 && !g_dpAvoidLog[type]++)
            Log("death presence: type %02X hesitates at the fog (depth %.0f, cut %.2f, inward %.2f, phase %d, step %.4f s)", type,
                R - d, h, vin, v->av.phase, dtS);
    }
}
static void __cdecl DpCommit(void)  // 0x462987: was `call 456C60` (the mover commit pass); forwards always
{
    if (g_dpInstalled && !g_dpFail && g_dpLevel > 0) {
        __try {
            DpAvoidShape();
        } __except (DpFaultNote(GetExceptionInformation())) {
            DpFailNow("the avoidance", g_dpFaultCode, g_dpFaultAt);
        }
    }
    ((void(__cdecl *)(void))0x456C60)();
}

// ---------------------------------------------------------------- v3.27.1 companion Eileen walks out of the fog (PLAN 6.3)
// re_follow.md sections 1.10 / 3 / 4: EileenBrain calls EileenFollowDecide only at 0x44A863 (esi = her work); CombatBrain
// runs right after and still wins. When the player comes within R + 25 (horizontally) and nothing else wants her, she
// turns away and walks out to R + 100 with her own walk (request 3, mode 0 = walk mode 1: no stop test, her wall slide),
// then the stock pant / idle; she gives up when cornered. Counts in real seconds: 1 / [0x5D5248] per brain call (the rate
// in force inside the brain, with EntityGate 0 and 1).
typedef struct { int on; float t, stuckT, cool; DWORD room; int logged; } DpBackOffState;
static DpBackOffState g_dpBo;
static void DpBackOffEnd(BYTE *w, const char *why, float dh, float R)
{
    g_dpBo.on = 0;
    w[0x212] = 1;  // state 3 + request 1 -> 9 (pant) -> idle (0x44A883); FollowDecide turns her back next tick
    w[0x213] = 1;
    if (!(g_dpBo.logged & 2)) {
        g_dpBo.logged |= 2;
        Log("death presence: Eileen's back-off ends (%s) at %.0f from the player (R %.0f)", why, dh, R);
    }
}
static void DpBackOffC(BYTE *w)
{
    const int level = (int)g_dpLevel, rate = *(volatile int *)0x5D5248;
    const float dt = rate >= 15 && rate <= 240 ? 1.0f / (float)rate : 1.0f / 30.0f;
    const DWORD stage = *(volatile DWORD *)0xFD5A90, room = *(volatile DWORD *)0xFD5A94 & 0xFFFF;
    const BYTE *mdl;
    float R, pp[3], pos[3], dx, dz, dh, ax, az;
    BYTE req;
    if ((stage << 16 | room) != g_dpBo.room) {  // a new room: clean state, log again
        memset(&g_dpBo, 0, sizeof g_dpBo);
        g_dpBo.room = stage << 16 | room;
    }
    if (g_dpBo.cool > 0.0f) {
        g_dpBo.cool -= dt;
        g_dpBo.on = 0;
        return;
    }
    if (level <= 0 || level >= DP_LEVELS || *(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xFD5BC4 != 1 ||
        *(const int *)(w + 0xF0) || *(const int *)(w + 0xEC) || *(void *const *)(w + 0x240) ||  // other room, line blocked, target
        *(volatile int *)0xFCE7D4 != -1 || w[0x28C] ||                                          // a talk, her event state 0x13
        stage == 9 || (stage == 11 && room == 7) || (stage == 2 && room == 13) || (stage == 8 && room == 1) ||  // scripted
        DpOffHere(stage, g_dpRoom302) ||                                                        // v3.42 SM342: Room 302
        w[0x210] == 0x14 || w[0x210] == 0x15 || *(volatile int *)0x10A1D04 == 10) {           // she or the player is down
        g_dpBo.on = 0;  // any off condition ends a running back-off without a write (the stock brain decides)
        return;
    }
    req = w[0x212];
    if (req != 0 && req != 2 && req != 3) {  // FollowDecide's own requests only (none, turn, walk)
        g_dpBo.on = 0;
        return;
    }
    mdl = *(const BYTE *const *)(w + 0x18);
    if (!mdl) return;
    memcpy(pos, mdl + 0x10, sizeof pos);
    memcpy(pp, (const void *)0x10A0F60, sizeof pp);
    if (!DpFinite3(pos) || !DpFinite3(pp) || !(fabsf(pos[1] - pp[1]) < DP_DY)) {
        g_dpBo.on = 0;
        return;
    }
    R = kDpLevel[level].R;
    dx = pos[0] - pp[0];
    dz = pos[2] - pp[2];
    dh = sqrtf(dx * dx + dz * dz);
    if (!g_dpBo.on) {
        if (!(dh < R + 25.0f)) return;
        g_dpBo.on = 1, g_dpBo.t = 0.0f, g_dpBo.stuckT = 0.0f;
        if (!(g_dpBo.logged & 1)) {
            g_dpBo.logged |= 1;
            Log("death presence: Eileen backs off out of the fog (%.0f from the player, R %.0f, out to %.0f)", dh, R, R + 100.0f);
        }
    }
    g_dpBo.t += dt;
    if (dh > R + 100.0f) {
        DpBackOffEnd(w, "out", dh, R);
        return;
    }
    if (g_dpBo.t > 5.0f) {
        DpBackOffEnd(w, "5 s", dh, R);
        return;
    }
    if (*(const signed char *)(w + 0x204) == 2) {  // under 10 % of the walk: cornered after 0.33 s, then 3 s of rest
        g_dpBo.stuckT += dt;
        if (g_dpBo.stuckT > 0.33f) {
            DpBackOffEnd(w, "cornered", dh, R);
            g_dpBo.cool = 3.0f;
            return;
        }
    } else {
        g_dpBo.stuckT = 0.0f;
    }
    if (dh < 1.0f) ax = 0.0f, az = 1.0f;  // (on top of each other: any way out)
    else ax = dx / dh, az = dz / dh;      // away from the player, horizontal
    *(float *)(w + 0x40) = ax;  // move_vec
    *(float *)(w + 0x44) = 0.0f;
    *(float *)(w + 0x48) = az;
    *(float *)(w + 0x4C) = 0.0f;
    *(float *)(w + 0x50) = dh < R - 100.0f ? 20000.0f : 0.0f;  // deep inside: the gait climbs to a run; else a slow walk
    w[0x212] = *(const float *)(w + 0x170) * ax + *(const float *)(w + 0x178) * az < 0.0f ? 2 : 3;  // facing in: turn first
    w[0x213] = 0;  // her own request: walk mode 1 (no stop test at R + 50)
}
static void __cdecl DpBackOff(BYTE *w)
{
    if (!g_dpInstalled || g_dpFail || !w) return;
    __try {
        DpBackOffC(w);
    } __except (DpFaultNote(GetExceptionInformation())) {
        DpFailNow("Eileen's back-off", g_dpFaultCode, g_dpFaultAt);
    }
}
static DWORD g_dpFollowFn = 0x445FC0;  // EileenFollowDecide (esi = work)
static __declspec(naked) void DpFollowStub(void)  // 0x44A863: was `call 445FC0`; every register as the stock call leaves them
{
    __asm {
        call dword ptr [g_dpFollowFn]  // the stock FollowDecide first (eax and the flags untouched before it)
        pushad
        pushfd
        push esi
        call DpBackOff
        add esp, 4
        popfd
        popad
        ret
    }
}

// ---------------------------------------------------------------- install (Init, after QuickSaveInstall)
static void DeathPresenceInstall(void)
{
    static const Site kSites[6] = {
        { 0x462987, { 0xE8, 0xD4, 0x42, 0xFF, 0xFF }, 5 },                     // call 456C60 (the mover commit)
        { 0x4477EC, { 0xD8, 0x1D, 0x48, 0x7D, 0x5B, 0x00 }, 6 },               // fcomp [5B7D48] (Eileen stops: 200)
        { 0x4460F8, { 0xD8, 0x1D, 0x1C, 0x80, 0x5B, 0x00 }, 6 },               // fcomp [5B801C] (Eileen follows: 500)
        { 0x4EC5BF, { 0xD9, 0x05, 0x48, 0x7D, 0x5B, 0x00 }, 6 },               // fld [5B7D48] (Cynthia stops)
        { 0x4EBD16, { 0xD9, 0x05, 0x1C, 0x80, 0x5B, 0x00 }, 6 },               // fld [5B801C] (Cynthia follows: 500) (v3.27.1:
                                                                                // 0x4EC3AB was her walk-speed band, re_follow.md)
        { 0x44A863, { 0xE8, 0x58, 0xB7, 0xFF, 0xFF }, 5 },                     // call 445FC0 (EileenBrain -> FollowDecide)
    };
    const DWORD stop = (DWORD)(ULONG_PTR)&g_dpStop, start = (DWORD)(ULONG_PTR)&g_dpStart;
    GpPatch set[6];
    int i;
    g_dpTab.roomKey = 0xFFFFFFFFu;
    // v3.42 SM342 (USER_FEEDBACK_v341 item 7): hidden, not written to the ini; 1 = v3.41's Room 302 (the fog clears the hauntings)
    g_dpRoom302 = GetPrivateProfileIntA("ScreenEffects", "DeathPresenceRoom302", 0, g_iniPath) != 0;
    Log("death presence: Room 302 (stages 1 and 0A): %s", g_dpRoom302 ? "on ([ScreenEffects] DeathPresenceRoom302=1: the fog "
        "clears the hauntings as in v3.41)" : "off (no fog there, the hauntings as stock; hidden [ScreenEffects] "
        "DeathPresenceRoom302=1 brings it back)");
    if (!g_qsTickOn) {
        Log("death presence: no 0x4EF982 tick dispatcher: the feature is off");
        g_dpFail = 1;
        return;
    }
    if (!g_fxOn || !g_spInstalled || !g_thermHooks) {  // review DP-I004: no fog without its draw path, and no effect without
        // the fog; review DP-R602: the victims' body cues need this frame's models from the 0x52AEB5 hook (Thermal's group)
        Log("death presence: the scene pass is not in (screen effects %s, HUD hook %s, model hook %s): the feature is off",
            g_fxOn ? "in" : "off", g_spInstalled ? "in" : "off", g_thermHooks ? "in" : "off");
        g_dpFail = 1;
        return;
    }
    for (i = 0; i < 6; ++i)
        if (!SiteOk(&kSites[i])) {
            Log("death presence: unexpected bytes at %08lX (another mod?): the feature is off", kSites[i].va);
            g_dpFail = 1;
            return;
        }
    GpBranch(&set[0], 0x462987, 0xE8, DpCommit, 5);
    GpData(&set[1], 0x4477EE, &stop, 4);
    GpData(&set[2], 0x4460FA, &start, 4);
    GpData(&set[3], 0x4EC5C1, &stop, 4);
    GpData(&set[4], 0x4EBD18, &start, 4);  // v3.27.1 (re_follow.md 4): CynthiaFollowDecide's restart, not 0x4EC3AD
    GpBranch(&set[5], 0x44A863, 0xE8, DpFollowStub, 5);  // v3.27.1 (PLAN 6.3): Eileen walks out of the fog
    g_dpStop = 200.0f;  // stock until the level says otherwise
    g_dpStart = 500.0f;
    if (!GpApplyAll(set, 6, "death presence")) {
        g_dpFail = 1;
        return;
    }
    g_dpInstalled = 1;
    g_dpReady = 1;
    DpLevelChanged((int)g_dpLevel);
    Log("death presence: in (commit 00462987, follow distances 004477EE / 004460FA / 004EC5C1 / 004EBD18, Eileen's back-off "
        "0044A863); level %s",
        kGpName[GP_DP][g_gp[GP_DP]]);
}
