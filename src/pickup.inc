// ================================================================ v3.32 PM332: a dropped item under a downed ghost
// The user (USER_FEEDBACK_v331 F): "For downed ghosts only: allow picking up the dropped item after attacking the downed
// ghost within 3 seconds. This will prevent us from always attacking while never getting the dropped item due to ghost
// proximity." Included by sh4windowmod.c after drops.inc. RE: staging/v332_feedback/w5_pickup_menu/PLAN.md section F.
//
// Why the press attacks (SH4Decomp, GOG v7 VAs):
// * Attack (action 0) and Action / examine (action 0xE) are the same button (default mask 0x40 for both, 0x5BF320) and the
//   left click is both (0x52F260 / 0x52C410).
// * In one gameplay tick the player's task (actor kind 1 = task group 8, ActorRecordAlloc 0x52B2C0; run by 0x4EF880)
//   runs BEFORE the event objects' zone tests (0x4EF660 -> EventFrameUpdate 0x4D0250 -> EventObjectsTick 0x4D00F0 ->
//   ActorZoneTest 0x52C490).
// * The player in state 0 with Attack held and the nearest lock target a downed enemy (EnemyIsDown 0x45BB60: class 0x0A)
//   within 200 and 150 below starts the stomp, state 6, at 0x53891D (PlayerStateMachineUpdate 0x537C30, default case):
//   at once when its HP > 0, and at 0 HP when Attack was held the tick before and the 1.5 s stomp timer [0x10A1EA0] is 0.
// * The pickup's zone (flag 4, the action button) needs the player free (0x5328C0: state 0, +0x218 / +0x220 / +0x228 /
//   +0x3B0 all 0) and Action pressed this tick. Its test comes later in the same tick and sees state 6: refused. During
//   the stomp (about 2.2 s) every press is lost, and a held button stomps again. So an item under a downed ghost
//   stays out of reach while the player keeps attacking.
//
// The fix: one exe site, 0x53890B (7 bytes `push edi / mov [esi+23Ch],edi`: the first instruction of the stomp start,
// reached only when the stock code has already decided to stomp). hkPkStomp asks PkStompYield: inside 3 s after the last
// attack on a downed ghost (PkTick, every gameplay tick), the stomp target a downed ghost (or Walter), and an item pickup
// that takes this very press (the game's own tests: record not running, enabled, EventCondExamine, ActorZoneTest), the
// stomp is skipped (0x538A02, the stock "no stomp" exit of that case): the player stays in state 0 and the same tick's
// zone test picks the item up. Everything else: the stock stomp. Never for standing enemies (the stock code only stomps
// class 0x0A), never while Attack is only held (the zone test wants a fresh press), never in the ready stance (the stock
// game allows no pickup there).
// On whenever SH4 Refined runs (a fix, no menu row); hidden [Gameplay] DownedPickup=0 = v3.31 (the site is not patched).

#include "pickup_sim.h"

static int g_pkOn = 1, g_pkInstalled, g_pkFail, g_pkYields;
static float g_pkGrace;  // seconds left of the window (PkGraceStep)
static const DWORD g_pkBack = 0x538912, g_pkSkip = 0x538A02;
typedef int(__cdecl *PkFn1)(const void *);

static int PkFaultNote(EXCEPTION_POINTERS *e, const char *where)
{
    static int n;
    if (++n <= 3)
        Log("downed pickup: %s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
            e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// an enemy task's type ([work+0x20]) and class ([work+0x3C]); work = [task+0x408] (as hudbars.inc / drops.inc)
static int PkEnemyKind(const BYTE *t, int *type, int *cls)
{
    const BYTE *w;
    if ((ULONG_PTR)t < 0x10000) return 0;
    w = *(const BYTE *const *)(t + 0x408);
    if ((ULONG_PTR)w < 0x10000) return 0;
    *type = *(const int *)(w + 0x20);
    *cls = *(const int *)(w + 0x3C);
    return 1;
}

// 1 = an item pickup would take this tick's press: EventObjectsTick's own test for every event object of the room
// (list 0x13D8200: +0 record, +4 task, +0x0C next), item pickups only (actor type 4), without the use-item objects.
// Read-only: ActorIsEnabled reads the bit the last tick's 0x4CF300 set (the call itself is left to the game).
static int PkPickupTakesPress(void)
{
    const BYTE *obj = *(const BYTE *const volatile *)0x13D8200;
    int guard = 0;
    for (; (ULONG_PTR)obj >= 0x10000 && guard < 512; obj = *(const BYTE *const *)(obj + 0x0C), ++guard) {
        const void *task = *(const void *const *)(obj + 4);
        if ((ULONG_PTR)task < 0x10000) continue;
        if (((PkFn1)0x52B230)(task) != 4) continue;                          // ActorGetType: an item pickup
        if (((PkFn1)0x4CF090)(task) != 0) continue;                          // wants a use-item: not this test
        if (((PkFn1)0x4CFFC0)(*(const void *const *)obj)) continue;          // EventIsRecordRunning
        if (!((PkFn1)0x52B1C0)(task)) continue;                              // ActorIsEnabled
        if (!((PkFn1)0x52C100)(task)) continue;                              // ActorZoneCallCustom (EventCondExamine)
        if (((PkFn1)0x52C490)(task)) return 1;                               // ActorZoneTest
    }
    return 0;
}

// hkPkStomp (inside 0x537C30, game thread): 1 = skip the stomp, the press goes to the item
static int __cdecl PkStompYield(const BYTE *t)
{
    int type = 0, cls = 0, r = 0;
    __try {
        if (g_pkOn && !g_pkFail && g_pkGrace > 0.0f && PkEnemyKind(t, &type, &cls))
            r = PkYield(1, g_pkGrace, type, cls, PkGhostType(type) && cls == 0x0A && PkPickupTakesPress());
    } __except (PkFaultNote(GetExceptionInformation(), "the stomp test")) {
        g_pkFail = 1;
        r = 0;
    }
    if (r && ++g_pkYields <= 20)
        Log("downed pickup: the press takes the item, not a stomp on the downed type %X (%.2f s of the window left; %d)",
            type, g_pkGrace, g_pkYields);
    return r;
}

// quicksave.inc QsTick (0x4EF982, the start of every game tick): the window. Gameplay ticks only (phase 0): a pause, a
// menu or a scene freezes it. dt = the game's tick length 0x55AE60 (1 / fps).
static void PkTick(void)
{
    const BYTE *t;
    int type = 0, cls = 0, att = 0, st;
    if (!g_pkInstalled || g_pkFail) return;
    __try {
        if (*(volatile int *)0xFD5C20 != 0) return;
        st = *(volatile int *)0x10A1134;              // gPlayer[0].state
        t = *(const BYTE *const volatile *)0x10A115C;  // gPlayer[0].target
        if (PkAttackState(st) && PkEnemyKind(t, &type, &cls)) att = PkGhostType(type) && PkDownedClass(cls);
        g_pkGrace = PkGraceStep(g_pkGrace, att, ((float(__cdecl *)(void))0x55AE60)());
    } __except (PkFaultNote(GetExceptionInformation(), "the tick")) {
        g_pkFail = 1;
        g_pkGrace = 0.0f;
    }
}

// 0x53890B (was `push edi / mov [esi+23Ch],edi`, edi = the downed target, esi = gPlayer[0], ebx = 0, x87 stack empty)
static __declspec(naked) void hkPkStomp(void)
{
    __asm {
        pushad
        cld
        push edi
        call PkStompYield
        add esp, 4
        test eax, eax
        popad                       // flags survive popad
        jnz pk_yield
        push edi                    // the stock stomp start
        mov dword ptr [esi + 23Ch], edi
        jmp dword ptr [g_pkBack]    // 538912: call 537AA0 / state 6 / timer 1.5 s
    pk_yield:
        jmp dword ptr [g_pkSkip]    // 538A02: the stock exit of the case without a stomp (as 5388EE)
    }
}

static void PickupInstall(void)
{
    // the site with the bytes around it (the stomp decision 5388D1.., the start 53890B.., the exit 538A02) and the
    // game functions called (first bytes as in the GOG v7 exe)
    static const Site kSites[] = {
        { 0x5388D1, { 0x57, 0xE8, 0x59, 0x74, 0xF1, 0xFF, 0xD8, 0x1D, 0x64, 0xE0, 0x5C, 0x00, 0x83, 0xC4, 0x04, 0xDF, 0xE0,
                      0xF6, 0xC4, 0x41, 0x74, 0x24, 0x8B, 0x4C, 0x24, 0x18, 0x39, 0x59, 0x58, 0x0F, 0x84, 0x0E }, 32 },
        { 0x53890B, { 0x57, 0x89, 0xBE, 0x3C, 0x02, 0x00, 0x00, 0xE8, 0x89, 0xF1, 0xFF, 0xFF, 0x89, 0x86, 0x40, 0x02, 0x00,
                      0x00, 0xC7, 0x86, 0x14, 0x02, 0x00 }, 23 },
        { 0x538A02, { 0x8B, 0x4C, 0x24, 0x18, 0x8B, 0x41, 0x64 }, 7 },
        { 0x52C490, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x88, 0x24, 0x04, 0x00, 0x00 }, 10 },  // ActorZoneTest
        { 0x52C100, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x81, 0x24, 0x04, 0x00, 0x00 }, 10 },  // ActorZoneCallCustom
        { 0x4CFFC0, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, 5 },                                 // EventIsRecordRunning
        { 0x4CF090, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8 }, 6 },                           // the object's use-item
        { 0x52B1C0, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x75, 0x04 }, 8 },               // ActorIsEnabled
        { 0x52B230, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x0A }, 8 },               // ActorGetType
        { 0x55AE60, { 0xDB, 0x05, 0x48, 0x52, 0x5D, 0x00 }, 6 },                           // SystemGetFrameTime
    };
    int i;
    g_pkOn = GetPrivateProfileIntA("Gameplay", "DownedPickup", 1, g_iniPath) != 0;
    if (!g_pkOn) {
        Log("downed pickup: off ([Gameplay] DownedPickup=0): attacks on a downed ghost keep the stock priority");
        return;
    }
    if (!g_qsTickOn) {
        Log("downed pickup: no 0x4EF982 tick dispatcher: off");
        return;
    }
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            Log("downed pickup: %08lX is not stock (another mod?): off", kSites[i].va);
            return;
        }
    if (!PokeBranch(0x53890B, 0xE9, hkPkStomp, 7)) {  // one write: nothing to roll back if it fails
        Log("downed pickup: 0053890B could not be written: off");
        return;
    }
    g_pkInstalled = 1;
    Log("downed pickup: on (0053890B, 7 bytes): for %.0f s after an attack on a downed ghost, a fresh Action press "
        "next to an item picks it up instead of a stomp", PK_GRACE_S);
}
