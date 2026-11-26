// v3.51 EP351: native test of src\pursuit_room_sim.h (test\build_pursuit_room_test.cmd): Enemy Pursuit inside the room:
// the families and their states, each setting's timers, sight lost / regained, a hit or coming close, the rests, the
// dog's own give-up, the quick re-notice guard, a room change or a load mid-timer (no stale state), the door's question.
#include <stdio.h>
#include <stdlib.h>
#include "../src/pursuit_sim.h"
#include "../src/pursuit_room_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static EpRoom R;
static EpLook Look(int chase, int sees, float dist, float hp)
{
    EpLook k;
    memset(&k, 0, sizeof k);
    k.chase = chase, k.sees = sees, k.dist = dist, k.hp = hp;
    return k;
}
// runs one enemy from t0 to t1 in steps of dt with the same look; returns the first tick's time with result want (-1: none)
static float RunUntil(unsigned id, int fam, int mode, float t0, float t1, float dt, EpLook k, int want, int *why)
{
    float t;
    for (t = t0; t <= t1 + 1.0e-4f; t += dt)
        if (EpStep(&R, id, fam, mode, t, &k, why) == want) return t;
    return -1.0f;
}

static void TestFamilies(void)
{
    CHECK(EpFamily(0x03, 0, 0, 0) == EP_FAM_HUMMER, "Hummer");
    CHECK(EpFamily(0x07, 0, 0, 0) == EP_FAM_GUMHEAD && EpFamily(0x07, 1, 0, 0) == EP_FAM_GUMHEAD, "Gum Head v0 / v1");
    CHECK(EpFamily(0x07, 2, 0, 0) == EP_FAM_NONE && EpFamily(0x07, 0, 6, 0) == EP_FAM_NONE && EpFamily(0x07, 0, 7, 0) == EP_FAM_NONE,
          "Gum Head perch variant / events left alone");
    CHECK(EpFamily(0x0A, 0, 3, 0) == EP_FAM_DOG, "dog (released event dogs too)");
    CHECK(EpFamily(0x08, 0, 0, 0) == EP_FAM_TWIN && EpFamily(0x08, 0, 6, 0) == EP_FAM_NONE, "Twin; its event kind left alone");
    CHECK(EpFamily(0x05, 0, 0, 0) == EP_FAM_WALLMAN && EpFamily(0x05, 0, 8, 0) == EP_FAM_NONE && EpFamily(0x05, 0, 0, 1) == EP_FAM_NONE,
          "Wall Man; the boss and the floor variant left alone");
    CHECK(EpFamily(0x0B, 0, 0, 0) == EP_FAM_GHOST, "victims' ghosts");
    CHECK(EpFamily(0x09, 0, 0, 0) == EP_FAM_NONE && EpFamily(0x0C, 1, 0, 0) == EP_FAM_NONE && EpFamily(0x0D, 1, 0, 0) == EP_FAM_NONE &&
          EpFamily(0x0E, 0, 0, 0) == EP_FAM_NONE && EpFamily(0x10, 0, 0, 0) == EP_FAM_NONE && EpFamily(0x02, 0, 0, 0) == EP_FAM_NONE,
          "Tremer, 0C / 0D victims, story ghosts, Walter, Whitestool: none");
}

static void TestStates(void)
{
    int s;
    // Hummer: 6 / 7 / 8 -> 4; 9 latch / 0xA sting never; perch / wander not chasing
    for (s = 6; s <= 8; ++s) CHECK(EpChase(EP_FAM_HUMMER, s, 0, 0) == EP_CHASE_OK && EpGiveUpState(EP_FAM_HUMMER, s) == 4, "Hummer %d", s);
    CHECK(EpChase(EP_FAM_HUMMER, 9, 0, 0) == EP_CHASE && EpChase(EP_FAM_HUMMER, 0x0A, 0, 0) == EP_CHASE, "Hummer latch / sting");
    CHECK(EpChase(EP_FAM_HUMMER, 4, 0, 0) == EP_NOT && EpChase(EP_FAM_HUMMER, 1, 0, 0) == EP_NOT && EpChase(EP_FAM_HUMMER, 0x0D, 0, 0) == EP_NOT &&
          EpChase(EP_FAM_HUMMER, 0x0B, 0, 0) == EP_NOT, "Hummer wander / perch / falling / retreat");
    // Gum Head: standing -> 3, crawling -> 9; bites never; idle / hit / down not chasing
    CHECK(EpGiveUpState(EP_FAM_GUMHEAD, 4) == 3 && EpGiveUpState(EP_FAM_GUMHEAD, 5) == 3 && EpGiveUpState(EP_FAM_GUMHEAD, 6) == 3 &&
          EpGiveUpState(EP_FAM_GUMHEAD, 8) == 3, "Gum Head standing -> 3");
    for (s = 0x0A; s <= 0x0E; ++s) CHECK(EpChase(EP_FAM_GUMHEAD, s, 0, 0) == EP_CHASE_OK && EpGiveUpState(EP_FAM_GUMHEAD, s) == 9, "Gum Head %X", s);
    CHECK(EpGiveUpState(EP_FAM_GUMHEAD, 0x10) == 9 && EpGiveUpState(EP_FAM_GUMHEAD, 0x11) == 9, "Gum Head 10 / 11 -> 9");
    CHECK(EpChase(EP_FAM_GUMHEAD, 7, 0, 0) == EP_CHASE && EpChase(EP_FAM_GUMHEAD, 0x0F, 0, 0) == EP_CHASE, "Gum Head bites");
    CHECK(EpChase(EP_FAM_GUMHEAD, 3, 0, 0) == EP_NOT && EpChase(EP_FAM_GUMHEAD, 9, 0, 0) == EP_NOT && EpChase(EP_FAM_GUMHEAD, 0x12, 0, 0) == EP_NOT &&
          EpChase(EP_FAM_GUMHEAD, 0x13, 0, 0) == EP_NOT && EpChase(EP_FAM_GUMHEAD, 0x15, 0, 0) == EP_NOT, "Gum Head idle / hit / down / home");
    // dog: 4 with a player target only; its attacks and grabs never
    CHECK(EpChase(EP_FAM_DOG, 4, 0, 0) == EP_CHASE_OK && EpChase(EP_FAM_DOG, 4, 0, 1) == EP_CHASE_OK, "dog hunting a player");
    CHECK(EpChase(EP_FAM_DOG, 4, 0, 100) == EP_NOT && EpChase(EP_FAM_DOG, 4, 0, 200) == EP_NOT && EpChase(EP_FAM_DOG, 4, 0, -1) == EP_NOT,
          "dog after a blood pool / an object / none");
    CHECK(EpChase(EP_FAM_DOG, 7, 0, 0) == EP_CHASE && EpChase(EP_FAM_DOG, 8, 0, 0) == EP_CHASE && EpChase(EP_FAM_DOG, 9, 0, 0) == EP_CHASE &&
          EpChase(EP_FAM_DOG, 0x13, 0, 0) == EP_CHASE && EpChase(EP_FAM_DOG, 0x17, 0, 0) == EP_CHASE, "dog bites / grabs / growl / charge");
    CHECK(EpChase(EP_FAM_DOG, 2, 0, 0) == EP_NOT && EpChase(EP_FAM_DOG, 6, 0, 0) == EP_NOT && EpChase(EP_FAM_DOG, 0x0D, 0, 0) == EP_NOT &&
          EpGiveUpState(EP_FAM_DOG, 4) == -1, "dog wander / sniff / down; its own give-up");
    // Twin
    CHECK(EpChase(EP_FAM_TWIN, 2, 0, 0) == EP_CHASE_OK && EpChase(EP_FAM_TWIN, 2, 6, 0) == EP_NOT, "Twin notice; asleep");
    CHECK(EpChase(EP_FAM_TWIN, 3, 1, 0) == EP_CHASE_OK && EpChase(EP_FAM_TWIN, 3, 2, 0) == EP_CHASE && EpChase(EP_FAM_TWIN, 3, 3, 0) == EP_CHASE,
          "Twin approach; holding her / its blow");
    CHECK(EpChase(EP_FAM_TWIN, 10, 0, 0) == EP_CHASE_OK && EpChase(EP_FAM_TWIN, 8, 0, 0) == EP_CHASE && EpChase(EP_FAM_TWIN, 11, 0, 0) == EP_NOT &&
          EpChase(EP_FAM_TWIN, 1, 0, 0) == EP_NOT && EpChase(EP_FAM_TWIN, 14, 0, 0) == EP_NOT && EpGiveUpState(EP_FAM_TWIN, 5) == 1, "Twin");
    // Wall Man: 6 -> 7 only
    CHECK(EpChase(EP_FAM_WALLMAN, 6, 0, 0) == EP_CHASE_OK && EpGiveUpState(EP_FAM_WALLMAN, 6) == 7, "Wall Man out of the wall");
    CHECK(EpChase(EP_FAM_WALLMAN, 4, 0, 0) == EP_CHASE && EpChase(EP_FAM_WALLMAN, 8, 0, 0) == EP_CHASE && EpChase(EP_FAM_WALLMAN, 2, 0, 0) == EP_NOT &&
          EpChase(EP_FAM_WALLMAN, 1, 0, 0) == EP_NOT && EpChase(EP_FAM_WALLMAN, 0x0A, 0, 0) == EP_NOT, "Wall Man attacks / in the wall / down");
    // ghosts
    CHECK(EpChase(EP_FAM_GHOST, 2, 0, 0) == EP_CHASE_OK && EpChase(EP_FAM_GHOST, 0x0D, 0, 0) == EP_CHASE_OK && EpGiveUpState(EP_FAM_GHOST, 2) == 1,
          "ghost homing -> 1");
    CHECK(EpChase(EP_FAM_GHOST, 4, 0, 0) == EP_CHASE && EpChase(EP_FAM_GHOST, 8, 0, 0) == EP_CHASE && EpChase(EP_FAM_GHOST, 9, 0, 0) == EP_CHASE,
          "ghost grab / in the wall: never");
    CHECK(EpChase(EP_FAM_GHOST, 1, 0, 0) == EP_NOT && EpChase(EP_FAM_GHOST, 0x10, 0, 0) == EP_NOT && EpChase(EP_FAM_GHOST, 0x0E, 0, 0) == EP_NOT &&
          EpChase(EP_FAM_GHOST, 0x11, 0, 0) == EP_NOT, "ghost idle / pinned / lying / story rise");
    CHECK(EpGhostTouchState(4) && EpGhostTouchState(5) && !EpGhostTouchState(2), "ghost touch states");
    CHECK(EpNearRange(EP_FAM_HUMMER) < EpSenseRange(EP_FAM_HUMMER) && EpNearRange(EP_FAM_DOG) < EpSenseRange(EP_FAM_DOG), "ranges");
}

static void TestMonsterTimers(void)
{
    int why = -1;
    float t;
    EpLook k;
    // Relentless: never
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 3000.0f, 100.0f);
    CHECK(RunUntil(1, EP_FAM_HUMMER, EF_PURSUIT_RELENTLESS, 0.0f, 100.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f && R.n == 0,
          "Relentless: no give-up, nothing stored");
    // Normal: a chase starts (the stock AI noticed her): 10 s unseen from then
    EpRoomReset(&R, 1);
    t = RunUntil(1, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 9.99f && t <= 10.3f && why == EP_WHY_UNSEEN, "Normal: 10 s (%.2f, why %d)", t, why);
    // Forgetful 4 s
    EpRoomReset(&R, 1);
    t = RunUntil(2, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 3.99f && t <= 4.3f, "Forgetful: 4 s (%.2f)", t);
    // seeing her keeps it going; losing sight starts the time
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 1, 1500.0f, 100.0f);
    CHECK(RunUntil(3, EP_FAM_GUMHEAD, EF_PURSUIT_NORMAL, 0.0f, 60.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "in sight 60 s: no give-up");
    k.sees = 0;
    t = RunUntil(3, EP_FAM_GUMHEAD, EF_PURSUIT_NORMAL, 60.5f, 90.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 70.0f && t <= 70.6f, "unseen from 60 s: gives up at 70 (%.2f)", t);
    // sight regained mid-timer resets it
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    CHECK(RunUntil(4, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 8.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "8 s unseen: still chasing");
    k.sees = 1;
    EpStep(&R, 4, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 8.5f, &k, &why);
    k.sees = 0;
    t = RunUntil(4, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 9.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 18.5f && t <= 19.1f, "seen at 8.5 s: gives up 10 s later (%.2f)", t);
    // a hit (HP drop) counts as sensing; coming close too
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    RunUntil(5, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 9.0f, 0.5f, k, EP_GIVEUP, &why);
    k.hp = 80.0f;
    CHECK(EpStep(&R, 5, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 9.5f, &k, &why) == EP_KEEP, "a hit at 9.5 s: keep");
    t = RunUntil(5, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 10.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 19.5f && t <= 20.1f, "the hit restarted its memory (%.2f)", t);
    // a state that may not be changed: it waits (still chasing), then gives up at the first allowed look
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE, 0, 2500.0f, 100.0f);
    CHECK(RunUntil(6, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 20.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "latched: never");
    k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 6, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 20.5f, &k, &why) == EP_GIVEUP, "then at once");
    // not chasing: never (an idle monster out of sight)
    EpRoomReset(&R, 1);
    k = Look(EP_NOT, 0, 2500.0f, 100.0f);
    CHECK(RunUntil(7, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, 0.0f, 60.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "idle: never");
}

static void TestMonsterGiveUpAndRest(void)
{
    int why = -1, a;
    float t;
    EpLook k;
    // Normal: no rest; the stock rules may take the chase up again (a new chase: 10 s from then)
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    t = RunUntil(10, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(EpCommit(&R, 10, EF_PURSUIT_NORMAL, t, 0, why) == 0 && R.nMon == 1, "Normal: committed (first)");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 10, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 0.5f, &k, &why) == EP_KEEP, "Normal: no rest");
    CHECK(EpIgnoresYou(&R, 10, EF_PURSUIT_NORMAL, t + 0.5f), "it gave up: does not follow at the door");
    k.chase = EP_CHASE_OK;  // stock re-notices 5 s later
    CHECK(EpStep(&R, 10, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 5.0f, &k, &why) == EP_KEEP, "a new chase");
    CHECK(!EpIgnoresYou(&R, 10, EF_PURSUIT_NORMAL, t + 5.0f), "chasing again: it follows by the door rules");
    CHECK(RunUntil(10, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 5.5f, t + 14.6f, 0.5f, k, EP_GIVEUP, &why) < 0.0f &&
          EpStep(&R, 10, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 15.0f, &k, &why) == EP_GIVEUP, "again 10 s after the new chase");
    CHECK(EpCommit(&R, 10, EF_PURSUIT_NORMAL, t + 15.0f, 0, why) == 1 && R.nAgain == 1, "the second is counted again");
    // Forgetful: 3 s rest: holds, pushes back, a hit or close wakes it
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    t = RunUntil(11, EP_FAM_GUMHEAD, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 11, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 11, EP_FAM_GUMHEAD, EF_PURSUIT_FORGETFUL, t + 0.5f, &k, &why) == EP_HOLD, "rest: hold");
    k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 11, EP_FAM_GUMHEAD, EF_PURSUIT_FORGETFUL, t + 1.0f, &k, &why) == EP_AGAIN, "rest: a chase is set back");
    EpHeld(&R);
    CHECK(R.nHeld == 1, "held counted");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 11, EP_FAM_GUMHEAD, EF_PURSUIT_FORGETFUL, t + 3.5f, &k, &why) == EP_KEEP, "after 3 s: no hold");
    k.chase = EP_CHASE_OK;  // re-noticed after the rest by the stock rules
    a = EpStep(&R, 11, EP_FAM_GUMHEAD, EF_PURSUIT_FORGETFUL, t + 4.0f, &k, &why);
    CHECK(a == EP_KEEP && !EpIgnoresYou(&R, 11, EF_PURSUIT_FORGETFUL, t + 4.0f), "after the rest: a chase of its own");
    // a hit in the rest wakes it
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    t = RunUntil(12, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 12, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_NOT, k.hp = 90.0f;
    CHECK(EpStep(&R, 12, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, t + 0.5f, &k, &why) == EP_KEEP, "hit in the rest: woken (no hold)");
    k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 12, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, t + 1.0f, &k, &why) == EP_KEEP, "and its chase stays");
    // coming close in the rest wakes it
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    t = RunUntil(13, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 13, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_CHASE_OK, k.dist = 450.0f;
    CHECK(EpStep(&R, 13, EP_FAM_TWIN, EF_PURSUIT_FORGETFUL, t + 0.5f, &k, &why) == EP_KEEP, "within 500 in the rest: woken");
    // an attack of its own in the rest (a state that may not be changed): the rest ends, it chases
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    t = RunUntil(14, EP_FAM_HUMMER, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 14, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_CHASE;
    CHECK(EpStep(&R, 14, EP_FAM_HUMMER, EF_PURSUIT_FORGETFUL, t + 0.5f, &k, &why) == EP_KEEP, "latch in the rest: no set");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 14, EP_FAM_HUMMER, EF_PURSUIT_FORGETFUL, t + 1.0f, &k, &why) == EP_KEEP, "rest is over after it");
}

static void TestDownAndDog(void)
{
    int why = -1;
    float t;
    EpLook k;
    // down while she walks off out of its senses: it forgets at once when it is up and chasing
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 1, 800.0f, 100.0f);
    EpStep(&R, 20, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 0.0f, &k, &why);
    k = Look(EP_NOT, 0, 2500.0f, 60.0f);
    k.down = 1;
    EpStep(&R, 20, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 1.0f, &k, &why);  // (a hit put it down: hp 60)
    CHECK(R.e[0].downLost == 0, "the hit that put it down: it knows (%d)", R.e[0].downLost);
    EpStep(&R, 20, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 1.5f, &k, &why);
    CHECK(R.e[0].downLost == 1, "down, out of its senses: forgot");
    k.down = 0, k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 20, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 3.0f, &k, &why) == EP_GIVEUP && why == EP_WHY_DOWN, "up and chasing: gives up at once");
    EpCommit(&R, 20, EF_PURSUIT_NORMAL, 3.0f, 0, why);
    CHECK(R.nDown == 1, "counted down");
    // down but she stays in its senses: no
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 1, 800.0f, 100.0f);
    EpStep(&R, 21, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 0.0f, &k, &why);
    k.chase = EP_NOT, k.down = 1;
    EpStep(&R, 21, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 1.0f, &k, &why);
    k.chase = EP_CHASE_OK, k.down = 0;
    CHECK(EpStep(&R, 21, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 3.0f, &k, &why) == EP_KEEP, "down in sight: it keeps the chase");
    // the dog: its own give-up on its way (soft): reapplied while it is still hunting, done once it wanders
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 1800.0f, 100.0f);
    t = RunUntil(22, EP_FAM_DOG, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 10.0f && t < 10.6f, "dog 10 s (%.2f)", t);
    EpCommit(&R, 22, EF_PURSUIT_NORMAL, t, 1, why);
    CHECK(EpStep(&R, 22, EP_FAM_DOG, EF_PURSUIT_NORMAL, t + 0.5f, &k, &why) == EP_REAPPLY, "pending: reapply");
    CHECK(EpIgnoresYou(&R, 22, EF_PURSUIT_NORMAL, t + 0.5f), "pending: it does not follow");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 22, EP_FAM_DOG, EF_PURSUIT_NORMAL, t + 1.0f, &k, &why) == EP_KEEP && R.e[0].pendAt == 0.0f, "it wanders: done");
    // the game keeps it hunting past EP_PEND_MAX: a chase of its own from then (no endless reapply)
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 1800.0f, 100.0f);
    t = RunUntil(23, EP_FAM_DOG, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 23, EF_PURSUIT_NORMAL, t, 1, why);
    CHECK(RunUntil(23, EP_FAM_DOG, EF_PURSUIT_NORMAL, t + 0.5f, t + 2.5f, 0.5f, k, EP_KEEP, &why) < 0.0f, "reapplied for 3 s");
    CHECK(EpStep(&R, 23, EP_FAM_DOG, EF_PURSUIT_NORMAL, t + 3.5f, &k, &why) == EP_KEEP, "then its own chase");
    CHECK(RunUntil(23, EP_FAM_DOG, EF_PURSUIT_NORMAL, t + 4.0f, t + 13.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "10 s from then");
}

static void TestQuickGuard(void)
{
    int why = -1, i;
    float t = 0.0f;
    EpLook k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    EpRoomReset(&R, 1);
    for (i = 0; i < 2; ++i) {  // give up, re-noticed 0.5 s later (the game sees what the line test did not)
        t = RunUntil(30, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t, t + 30.0f, 0.5f, k, EP_GIVEUP, &why);
        EpCommit(&R, 30, EF_PURSUIT_NORMAL, t, 0, why);
        k.chase = EP_NOT;
        EpStep(&R, 30, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 0.25f, &k, &why);
        k.chase = EP_CHASE_OK;
        EpStep(&R, 30, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t + 0.5f, &k, &why);
        t += 0.75f;
    }
    CHECK(R.nStock == 1 && R.e[0].stock, "two quick re-notices: left to the game (%d)", R.nStock);
    CHECK(RunUntil(30, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, t, t + 60.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "then never in this visit");
    EpRoomReset(&R, 2);
    CHECK(R.n == 0 && R.nStock == 0, "a new room visit forgets it");
}

static void TestGhosts(void)
{
    int why = -1, a;
    float t;
    EpLook k;
    // Normal: farther than 2500 for 12 s
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 3000.0f, 50.0f);
    t = RunUntil(40, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 11.99f && t <= 12.3f && why == EP_WHY_FAR, "ghost Normal far 12 s (%.2f %d)", t, why);
    // 2400 is not far: 25 s of chasing without touching
    EpRoomReset(&R, 1);
    k.dist = 2400.0f;
    t = RunUntil(41, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 60.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 24.99f && t <= 25.3f && why == EP_WHY_LONG, "ghost Normal 25 s chase (%.2f %d)", t, why);
    // Forgetful: 1500 for 6 s; 12 s chase
    EpRoomReset(&R, 1);
    k.dist = 1600.0f;
    t = RunUntil(42, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 5.99f && t <= 6.3f && why == EP_WHY_FAR, "ghost Forgetful far 6 s (%.2f)", t);
    EpRoomReset(&R, 1);
    k.dist = 1000.0f;
    t = RunUntil(43, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 11.99f && t <= 12.3f && why == EP_WHY_LONG, "ghost Forgetful 12 s chase (%.2f)", t);
    // coming near interrupts the far timer; touching restarts the chase timer
    EpRoomReset(&R, 1);
    k.dist = 3000.0f;
    CHECK(RunUntil(44, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 10.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "far 10 s: still");
    k.dist = 2000.0f;
    EpStep(&R, 44, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 10.5f, &k, &why);
    k.dist = 3000.0f;
    t = RunUntil(44, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 11.0f, 40.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 23.0f && t <= 23.6f, "far again from 11 s: 23 (%.2f)", t);
    EpRoomReset(&R, 1);
    k.dist = 2000.0f;
    RunUntil(45, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 20.0f, 0.5f, k, EP_GIVEUP, &why);
    k.chase = EP_CHASE, k.touch = 1;  // it grabbed her
    EpStep(&R, 45, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 20.5f, &k, &why);
    k.chase = EP_CHASE_OK, k.touch = 0;
    t = RunUntil(45, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 21.0f, 60.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 45.5f && t <= 46.1f, "a grab at 20.5 s: 25 s from then (%.2f)", t);
    // in the wall (not to be changed): it waits; its timer keeps counting (no restart at the exit)
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE, 0, 3000.0f, 50.0f);
    CHECK(RunUntil(46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 20.0f, 0.5f, k, EP_GIVEUP, &why) < 0.0f, "in the wall: waits");
    k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 20.5f, &k, &why) == EP_GIVEUP, "out of it: gives up at once");
    // the rest: 8 s Normal, held while idle, set back when it homes again, a hit or 400 ends it
    EpCommit(&R, 46, EF_PURSUIT_NORMAL, 20.5f, 0, EP_WHY_FAR);
    CHECK(R.nGhost == 1, "ghost counted");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 21.0f, &k, &why) == EP_HOLD, "rest: hold (drift home)");
    k.chase = EP_CHASE_OK;
    CHECK(EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 25.0f, &k, &why) == EP_AGAIN, "rest: homing set back");
    CHECK(EpIgnoresYou(&R, 46, EF_PURSUIT_NORMAL, 25.0f), "resting: does not follow");
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 28.4f, &k, &why) == EP_HOLD, "7.9 s: still resting");
    CHECK(EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 28.6f, &k, &why) == EP_KEEP, "8 s: rest over");
    CHECK(EpIgnoresYou(&R, 46, EF_PURSUIT_NORMAL, 28.6f), "idle after the rest, not re-noticed: does not follow");
    k.chase = EP_CHASE_OK;
    a = EpStep(&R, 46, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 29.0f, &k, &why);
    CHECK(a == EP_KEEP && !EpIgnoresYou(&R, 46, EF_PURSUIT_NORMAL, 29.0f), "re-noticed by the stock rules: chases, follows");
    // Forgetful rest 15 s; a hit ends it
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 3000.0f, 50.0f);
    t = RunUntil(47, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 47, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_NOT;
    CHECK(EpStep(&R, 47, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, t + 14.5f, &k, &why) == EP_HOLD, "Forgetful rest 15 s");
    k.hp = 40.0f;
    CHECK(EpStep(&R, 47, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, t + 14.7f, &k, &why) == EP_KEEP, "a hit ends the rest");
    // 400 ends it too
    EpRoomReset(&R, 1);
    k = Look(EP_CHASE_OK, 0, 3000.0f, 50.0f);
    t = RunUntil(48, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 48, EF_PURSUIT_FORGETFUL, t, 0, why);
    k.chase = EP_NOT, k.dist = 350.0f;
    CHECK(EpStep(&R, 48, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, t + 1.0f, &k, &why) == EP_KEEP, "within 400: rest over");
}

// v3.51 MERGE (EP351 x PT351): a ghost in Fade In's tar-portal hold is not judged; its timers start when the hold ends
static void TestPortalHold(void)
{
    int why = -1;
    float t;
    EpLook k = Look(EP_CHASE_OK, 0, 3000.0f, 50.0f);
    // a ghost first seen in its hold: no entry is made while held
    EpRoomReset(&R, 7);
    for (t = 0.0f; t < 2.5f; t += 0.25f) EpPortalHeld(&R, 60, t);
    CHECK(R.n == 0 && !EpIgnoresYou(&R, 60, EF_PURSUIT_NORMAL, 2.5f), "held, never looked at: no entry");
    t = RunUntil(60, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 2.5f, 40.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 14.49f && t <= 14.8f && why == EP_WHY_FAR, "far: 12 s from the end of the hold (%.2f %d)", t, why);
    // a ghost looked at before its hold (the arrival's placeholder): the hold restarts its far timer
    EpRoomReset(&R, 7);
    CHECK(RunUntil(61, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 1.0f, 0.25f, k, EP_GIVEUP, &why) < 0.0f, "1 s far before the hold");
    for (t = 1.25f; t < 3.75f; t += 0.25f) EpPortalHeld(&R, 61, t);
    CHECK(R.n == 1 && R.e[0].chasing == 0 && R.e[0].far0 < 0.0f && R.e[0].seen >= 3.49f, "held: restarted");
    t = RunUntil(61, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 3.75f, 40.0f, 0.25f, k, EP_GIVEUP, &why);
    CHECK(t >= 15.74f && t <= 16.1f && why == EP_WHY_FAR, "far: 12 s from the end of the hold, not from 0 (%.2f)", t);
    // the chase timer too (Forgetful 12 s chase without touching)
    EpRoomReset(&R, 7);
    k.dist = 1000.0f;
    RunUntil(62, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 0.0f, 10.0f, 0.5f, k, EP_GIVEUP, &why);
    for (t = 10.25f; t < 12.75f; t += 0.25f) EpPortalHeld(&R, 62, t);
    t = RunUntil(62, EP_FAM_GHOST, EF_PURSUIT_FORGETFUL, 12.75f, 60.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 24.7f && t <= 25.3f && why == EP_WHY_LONG, "chase 12 s from the end of the hold (%.2f)", t);
    // its rest and give-up count are kept; an unknown id is nothing
    EpRoomReset(&R, 7);
    k.dist = 3000.0f;
    t = RunUntil(63, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 63, EF_PURSUIT_NORMAL, t, 0, why);
    EpPortalHeld(&R, 63, t + 1.0f);
    CHECK(R.e[0].gives == 1 && R.e[0].restEnd > t + 7.9f, "rest and count kept");
    EpPortalHeld(&R, 9999u, 1.0f);
    CHECK(R.n == 1, "unknown id: no entry");
}

static void TestResets(void)
{
    int why = -1;
    float t;
    EpLook k = Look(EP_CHASE_OK, 0, 2500.0f, 100.0f);
    // a room change mid-timer: the new visit starts afresh (9 s spent in the old room count for nothing)
    EpRoomReset(&R, 1);
    RunUntil(50, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 0.0f, 9.0f, 0.5f, k, EP_GIVEUP, &why);
    EpRoomReset(&R, 2);
    t = RunUntil(50, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 9.5f, 40.0f, 0.5f, k, EP_GIVEUP, &why);
    CHECK(t >= 19.5f && t <= 20.1f, "room change: 10 s in the new room (%.2f)", t);
    // a load mid-timer (serial 0, and the clock may be lower after it): no stale state, no negative age
    EpRoomReset(&R, 3);
    RunUntil(51, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 100.0f, 110.0f, 0.5f, Look(EP_CHASE_OK, 0, 3000.0f, 50.0f), EP_GIVEUP, &why);
    EpRoomReset(&R, 0);
    CHECK(R.n == 0 && !EpIgnoresYou(&R, 51, EF_PURSUIT_NORMAL, 5.0f), "a load: empty");
    t = RunUntil(51, EP_FAM_GHOST, EF_PURSUIT_NORMAL, 5.0f, 40.0f, 0.5f, Look(EP_CHASE_OK, 0, 3000.0f, 50.0f), EP_GIVEUP, &why);
    CHECK(t >= 17.0f && t <= 17.6f, "after the load: 12 s from its first look (%.2f)", t);
    // a clock that goes back inside a visit: never a negative age (no instant give-up)
    EpRoomReset(&R, 4);
    EpStep(&R, 52, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 50.0f, &k, &why);
    CHECK(EpStep(&R, 52, EP_FAM_HUMMER, EF_PURSUIT_NORMAL, 20.0f, &k, &why) == EP_KEEP && R.e[0].seen == 20.0f, "clock back: clamped");
    // the table: full = nothing for the rest (logged), never out of bounds
    {
        int i;
        EpRoomReset(&R, 5);
        for (i = 0; i < EP_N + 10; ++i) EpStep(&R, 1000u + (unsigned)i, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 0.0f, &k, &why);
        CHECK(R.n == EP_N && R.full, "full table (%d)", R.n);
        CHECK(EpStep(&R, 5000u, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 50.0f, &k, &why) == EP_KEEP, "one more: keep");
    }
    // a setting change mid-timer: Relentless stops everything at once; the door question says no
    EpRoomReset(&R, 6);
    t = RunUntil(53, EP_FAM_TWIN, EF_PURSUIT_NORMAL, 0.0f, 30.0f, 0.5f, k, EP_GIVEUP, &why);
    EpCommit(&R, 53, EF_PURSUIT_NORMAL, t, 0, why);
    CHECK(!EpIgnoresYou(&R, 53, EF_PURSUIT_RELENTLESS, t + 1.0f), "Relentless: the door rules of v3.50 only");
    CHECK(EpStep(&R, 53, EP_FAM_TWIN, EF_PURSUIT_RELENTLESS, t + 1.0f, &k, &why) == EP_KEEP, "Relentless: keep");
    CHECK(EpCommit(&R, 9999u, EF_PURSUIT_NORMAL, 1.0f, 0, 0) == 0, "commit of an unknown id: nothing");
    CHECK(EpGhostSecs(EF_PURSUIT_NORMAL, EP_WHY_FAR) == 12.0f && EpGhostSecs(EF_PURSUIT_FORGETFUL, EP_WHY_LONG) == 12.0f &&
          EpGhostSecs(EF_PURSUIT_RELENTLESS, EP_WHY_FAR) == 0.0f, "the ghosts' seconds for the log");
}

int main(void)
{
    TestFamilies();
    TestStates();
    TestMonsterTimers();
    TestMonsterGiveUpAndRest();
    TestDownAndDog();
    TestQuickGuard();
    TestGhosts();
    TestResets();
    TestPortalHold();  // v3.51 MERGE
    printf("pursuit_room_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
