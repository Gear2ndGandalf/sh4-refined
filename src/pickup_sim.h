// v3.32 PM332: the pure rules of pickup.inc (a dropped item under a downed ghost). No game memory: pickup_sim_test.c
// builds it alone (test\build_pickup_test.cmd).
#ifndef PICKUP_SIM_H
#define PICKUP_SIM_H

#define PK_GRACE_S 3.0f  // seconds after the last attack on a downed ghost (the user: "within 3 seconds")
#define PK_DT_MAX 0.1f   // one tick never counts more than this (the game's dt is 1 / fps: 1/30 .. 1/180)

// the downed beings this applies to: the ghosts (Victims 0x0B, Cynthia / Victim 13 0x0C, Jasper / Victim 04 0x0D,
// Andrew 0x0E, Richard 0x0F, the Patient 0x11) and the stalker Walter 0x10 (he never dies either: downed at 0 HP for
// 30-60 s, and Supply Drops gives him the same top-level drop as the four named ghosts)
static int PkGhostType(int type) { return type >= 0x0B && type <= 0x11; }
// downed: 9 down (countdown 0), 0x0A down (countdown > 0), 0x0B pinned by the Sword (staging/v330_feedback/w4_hud/
// RE_GHOST_HEALTH.md); the stock stomp (0x537C30 state 0 -> 6) takes class 0x0A only (EnemyIsDown 0x45BB60)
static int PkDownedClass(int cls) { return cls >= 9 && cls <= 0x0B; }
// the player attacks: 3 swing, 4 charged swing, 5 stomp from the ready stance, 6 stomp from standing (0x537C30)
static int PkAttackState(int st) { return st >= 3 && st <= 6; }

// One gameplay tick: an attack on a downed ghost holds the window full, so it counts from the end of the attack;
// otherwise it runs down by the tick's dt (frame-rate independent: 30 / 60 / 120 / 180 all reach 0 at 3 s).
static float PkGraceStep(float grace, int attacking, float dt)
{
    if (attacking) return PK_GRACE_S;
    if (!(dt > 0.0f)) return grace;  // also NaN
    if (dt > PK_DT_MAX) dt = PK_DT_MAX;
    grace -= dt;
    return grace > 0.0f ? grace : 0.0f;
}

// The stomp the player is about to start (state 0 -> 6 at 0x53890B) gives way to the pickup: the feature on, inside the
// window, the target a downed ghost (class 0x0A: what the stock stomp takes), and an item pickup would take this very
// press (the game's own zone test: in reach, facing it, Action pressed this tick, player free).
static int PkYield(int on, float grace, int type, int cls, int pickupTakesPress)
{
    return on && grace > 0.0f && PkGhostType(type) && cls == 0x0A && pickupTakesPress;
}

#endif
