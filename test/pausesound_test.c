// v3.51 MU351: native test of src\pausesound_sim.h (test\build_pausesound_test.cmd): Refined's choice at the Extra Menu's
// hand-over (MuBridgeAct), the log's music verdict (MuMusicState), and MuSim, a model of the game's sound pause
// (0x56E960 SoundSetPause + the master fade 0x56E310), run through the user's round trips: play -> pause -> Extra Menu >
// Load Game -> the list -> cancel -> pause -> Resume; -> pick a save; Save Game; Storage Chest; the fail-safe ways out.
// v3.50 (PauseSound=0) is run beside v3.51 on every trip: the bug is reproduced, then shown gone.
#include <stdio.h>
#include <math.h>
#include "../src/pausesound_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// play: music on, two streams playing: a world stream 15000 (scaled by the master, not paused by mode 0) and a stream
// below 10000 (not scaled by the master; mode 2 pauses it, mode 0 does not)
static void Play(MuSim *m, int sndA, int sndB)
{
    m->pauseMode = -1, m->fadeMode = 0, m->unk50 = 0, m->master = 1.0f;
    m->bf74 = 1, m->oneShot62FFE0 = 0;  // the last pause left 0x107BF74 at 1 (only the map / memo clear it)
    m->st[0] = m->st[1] = 2, m->saved[0] = m->saved[1] = 0;
    m->snd[0] = sndA, m->snd[1] = sndB;
    m->selects = m->resumes = m->pauses = 0;
}
static void Frames(MuSim *m, int n, float dt, float *maxOut, float *minOut)
{
    int i;
    for (i = 0; i < n; ++i) {
        MuSimFrame(m, dt);
        if (maxOut && m->master > *maxOut) *maxOut = m->master;
        if (minOut && m->master < *minOut) *minOut = m->master;
    }
}
// the stock pause opens (0x4F8BA0: 0x56E960(0)), then the pause input runs ([0x107BF74] = 1)
static void PauseOpen(MuSim *m, float dt)
{
    MuSimSetPause(m, 0);
    m->bf74 = 1;
    Frames(m, 30, dt, NULL, NULL);
}
// Refined's hand-over (EmBridgeOpen): keep = PauseSound; mark = 0 only for the ablation (no pauseMode mark)
static int BridgeOpen(MuSim *m, int keep, int mark)
{
    const int act = MuBridgeAct(keep, m->pauseMode);
    if (act == MU_ACT_V350) {
        MuSimSetPause(m, -1);
        MuSimSetPause(m, 2);
    } else {
        if (act == MU_ACT_REPAUSE) MuSimSetPause(m, 0);
        if (act != MU_ACT_LEAVE && mark) m->pauseMode = MU_MARK_MODE;
    }
    return act;
}

static void TestAct(void)
{
    int pm;
    for (pm = -1; pm <= 3; ++pm) CHECK(MuBridgeAct(0, pm) == MU_ACT_V350, "PauseSound=0 is v3.50 at mode %d", pm);
    CHECK(MuBridgeAct(1, 0) == MU_ACT_KEEP, "the pause's mode 0: no call");
    CHECK(MuBridgeAct(1, -1) == MU_ACT_REPAUSE && MuBridgeAct(1, 1) == MU_ACT_REPAUSE && MuBridgeAct(1, 2) == MU_ACT_REPAUSE,
          "a pause that is not in mode 0: paused again");
    CHECK(MuBridgeAct(1, 3) == MU_ACT_LEAVE && MuBridgeAct(1, 7) == MU_ACT_LEAVE && MuBridgeAct(1, -2) == MU_ACT_LEAVE,
          "mode 3 (a film) and unknown values: left alone");
    CHECK(MuMusicState(0.0f, 0) == MU_MUSIC_PAUSED && MuMusicState(0.0f, 4) == MU_MUSIC_PAUSED, "master 0: paused");
    CHECK(MuMusicState(0.2f, 0) == MU_MUSIC_QUIET && MuMusicState(0.6f, 2) == MU_MUSIC_QUIET, "0.2 / falling: quiet");
    CHECK(MuMusicState(0.0f, 1) == MU_MUSIC_RISING && MuMusicState(1.0f, 5) == MU_MUSIC_RISING, "fade 1 / 5: comes back");
    CHECK(MuMusicState(1.0f, 0) == MU_MUSIC_PLAYING, "1.0: playing");
    CHECK(MuMusicName(MU_MUSIC_PAUSED)[0] && MuMusicName(MU_MUSIC_PLAYING)[0] && MuMusicName(99)[0], "every state has a name");
}

// the user's trip: the list opened from the pause, held 4 s; returns the loudest master seen in the list
static float ListOpen(MuSim *m, int keep, float dt, float *last)
{
    float mx;
    BridgeOpen(m, keep, 1);
    mx = m->master;  // (right after the calls; the next fade step pushes the music one step lower: 0.93 at 30 fps)
    Frames(m, (int)(4.0f / dt), dt, &mx, NULL);
    *last = m->master;
    return mx;
}

static void TestTrips(float dt)
{
    MuSim a, b;
    float mxA, mxB, lastA, lastB, mn;
    int stA0, stA1, sel, i;

    // 1. open the load list from the pause: v3.50 (b) brings the music back; v3.51 (a) keeps the pause's silence
    Play(&a, 15000, 5000), Play(&b, 15000, 5000);
    PauseOpen(&a, dt), PauseOpen(&b, dt);
    CHECK(a.master == 0.0f && a.pauseMode == 0 && MuMusicState(a.master, a.fadeMode) == MU_MUSIC_PAUSED, "dt %.4f: the pause is silent", dt);
    stA0 = a.st[0], stA1 = a.st[1], sel = a.selects;
    mxA = ListOpen(&a, 1, dt, &lastA);
    mxB = ListOpen(&b, 0, dt, &lastB);
    CHECK(mxA == 0.0f && lastA == 0.0f && MuMusicState(a.master, a.fadeMode) == MU_MUSIC_PAUSED,
          "dt %.4f: v3.51: the music stays at 0 for the whole list (max %.2f)", dt, mxA);
    CHECK(a.st[0] == stA0 && a.st[1] == stA1 && a.selects == sel && a.pauseMode == MU_MARK_MODE,
          "dt %.4f: v3.51: the streams and sounds as the pause left them, no select sound, pauseMode marked", dt);
    CHECK(mxB == 1.0f && fabsf(lastB - 0.2f) < 1e-4f && MuMusicState(b.master, b.fadeMode) == MU_MUSIC_QUIET && b.selects == sel + 1,
          "dt %.4f: v3.50 reproduced: master to 1.0, then 0.2 for the whole list, a second select sound (max %.2f last %.2f)", dt, mxB,
          lastB);
    CHECK(b.st[1] == 3 && a.st[1] == 2, "dt %.4f: v3.50 paused the stream below 10000 (mode 2), v3.51 keeps the pause's (mode 0)", dt);

    // 2. cancel: back in the pause (EmToPause: 0x56E960(0)), both silent, one select sound each (as v3.50)
    sel = a.selects;
    MuSimSetPause(&a, 0), MuSimSetPause(&b, 0);
    Frames(&a, 10, dt, NULL, NULL), Frames(&b, 10, dt, NULL, NULL);
    CHECK(a.master == 0.0f && b.master == 0.0f && a.selects == sel + 1 && a.pauseMode == 0, "dt %.4f: cancel: the pause again, silent", dt);
    // 3. Resume: the music back once, at once (the stock Resume)
    sel = a.selects;
    i = a.resumes;
    MuSimSetPause(&a, -1);
    mn = 2.0f;
    Frames(&a, 60, dt, NULL, &mn);
    CHECK(a.master == 1.0f && mn == 1.0f && a.selects == sel + 1 && a.resumes == i && a.st[0] == 2 && a.st[1] == 2 &&
          MuMusicState(a.master, a.fadeMode) == MU_MUSIC_PLAYING,
          "dt %.4f: Resume: the music back once, full, the streams playing (resumes %d)", dt, a.resumes - i);
    MuSimSetPause(&a, -1);  // 0x4EFAE0 at the sub-mode's 8 -> 0: a second -1, from -1
    Frames(&a, 30, dt, NULL, &mn);
    CHECK(a.master == 1.0f && mn == 1.0f && a.selects == sel + 1, "dt %.4f: the second -1 of 8 -> 0 changes nothing (no cut, no sound)", dt);

    // 4. pick a save: the 16-frame fade to black inside the list, 0x4F8E60 (-1), then 0x4EFAE0 (-1): v3.51 silent in the
    //    black fade, then the same rise as v3.50 (from 0.2, no select sound), never louder than v3.50
    Play(&a, 15000, 5000), Play(&b, 15000, 5000);
    PauseOpen(&a, dt), PauseOpen(&b, dt);
    ListOpen(&a, 1, dt, &lastA), ListOpen(&b, 0, dt, &lastB);
    mxA = mxB = 0.0f;
    Frames(&a, 16, dt, &mxA, NULL), Frames(&b, 16, dt, &mxB, NULL);
    CHECK(mxA == 0.0f && fabsf(mxB - 0.2f) < 1e-4f, "dt %.4f: the fade to black: v3.51 silent, v3.50 at 0.2", dt);
    sel = a.selects;
    MuSimSetPause(&a, -1), MuSimSetPause(&b, -1);
    CHECK(a.fadeMode == 1 && b.fadeMode == 1 && a.selects == sel && MuMusicState(a.master, a.fadeMode) == MU_MUSIC_RISING,
          "dt %.4f: the overlay's leave: a fade-in as in v3.50, no select sound", dt);
    {
        int k, louder = 0, done = -1;
        for (k = 0; k < (int)(1.0f / dt); ++k) {
            if (k == 1) MuSimSetPause(&a, -1), MuSimSetPause(&b, -1);  // 0x4EFAE0
            MuSimFrame(&a, dt), MuSimFrame(&b, dt);
            if (a.master > b.master + 1e-5f) louder = 1;
            if (done < 0 && a.master == 1.0f) done = k;
        }
        CHECK(!louder && done >= 0 && (done + 1) * dt <= 0.5f + 2 * dt && a.master == 1.0f && b.master == 1.0f && a.selects == sel,
              "dt %.4f: the rise ends at 1.0 within 0.5 s (%d frames), never above v3.50's", dt, done + 1);
    }

    // 5. Save Game: the save task's own leave (0x4F8E60: -1) and EmToPause (0) in one tick: one select sound, as v3.50;
    //    without the pauseMode mark it would be two (and a jump to 1.0 in between)
    {
        MuSim c;
        int act;
        Play(&a, 15000, 31000), Play(&b, 15000, 31000), Play(&c, 15000, 31000);
        PauseOpen(&a, dt), PauseOpen(&b, dt), PauseOpen(&c, dt);
        BridgeOpen(&a, 1, 1), BridgeOpen(&b, 0, 1);
        act = BridgeOpen(&c, 1, 0);  // the ablation: no mark
        CHECK(act == MU_ACT_KEEP, "dt %.4f: the save from the pause: keep", dt);
        Frames(&a, 90, dt, NULL, NULL), Frames(&b, 90, dt, NULL, NULL), Frames(&c, 90, dt, NULL, NULL);
        CHECK(a.st[1] == 3 && c.st[1] == 3, "dt %.4f: a 31000 stream stays paused in the save menu", dt);
        sel = a.selects;
        i = b.selects;
        MuSimSetPause(&a, -1), MuSimSetPause(&a, 0);
        MuSimSetPause(&b, -1), MuSimSetPause(&b, 0);
        stA0 = c.selects;
        MuSimSetPause(&c, -1);
        mxA = c.master;
        MuSimSetPause(&c, 0);
        CHECK(a.selects - sel == 1 && b.selects - i == 1 && c.selects - stA0 == 2 && mxA == 1.0f,
              "dt %.4f: the save's close: one select sound as v3.50 (without the mark: two, master 1.0 between)", dt);
        Frames(&a, 30, dt, NULL, NULL);
        CHECK(a.master == 0.0f && a.pauseMode == 0 && a.st[1] == 3, "dt %.4f: back in the pause: silent, paused", dt);
    }

    // 6. the hand-over cannot turn back into the pause (EmBridgeBack: 0x4F8E60): like Resume, with v3.50's fade-in
    Play(&a, 15000, 5000);
    PauseOpen(&a, dt);
    BridgeOpen(&a, 1, 1);
    sel = a.selects;
    MuSimSetPause(&a, -1);
    Frames(&a, 30, dt, NULL, NULL);
    CHECK(a.master == 1.0f && a.selects == sel && a.pauseMode == -1, "dt %.4f: the fail-safe leave: the music back (a fade-in)", dt);

    // 7. a pause that was not in mode 0 (never seen): paused again, then kept
    Play(&a, 15000, 5000);
    MuSimSetPause(&a, 2);  // e.g. the overlay mode
    Frames(&a, 30, dt, NULL, NULL);
    CHECK(BridgeOpen(&a, 1, 1) == MU_ACT_REPAUSE && a.master == 0.0f && a.pauseMode == MU_MARK_MODE, "dt %.4f: re-paused", dt);
    mxA = 0.0f;
    Frames(&a, 60, dt, &mxA, NULL);
    CHECK(mxA == 0.0f, "dt %.4f: and silent after it", dt);

    // 8. mode 3 (a film): nothing touched, no mark
    Play(&a, 15000, 5000);
    MuSimSetPause(&a, 3);
    CHECK(BridgeOpen(&a, 1, 1) == MU_ACT_LEAVE && a.pauseMode == 3 && a.unk50 == 1, "dt %.4f: mode 3 left alone", dt);
}

// the model against the bytes of 0x56E960 (orig/sh4.exe): which streams each mode pauses
static void TestStreams(void)
{
    static const int kSnd[] = { 0, 5000, 9999, 10000, 10046, 10047, 10061, 10062, 19999, 20000, 29999, 30000, 39999 };
    int k, mode;
    for (k = 0; k < (int)(sizeof kSnd / sizeof kSnd[0]); ++k)
        for (mode = 0; mode <= 2; ++mode) {
            MuSim m;
            int want;
            Play(&m, kSnd[k], kSnd[k]);
            MuSimSetPause(&m, mode);
            want = kSnd[k] >= 30000 || (kSnd[k] >= 10047 && kSnd[k] <= 10061) || (mode == 2 && kSnd[k] < 10000);
            CHECK((m.st[0] == 3) == want && (m.st[1] == 3) == want && m.pauses == 2 * want, "stream %d mode %d paused %d", kSnd[k],
                  mode, want);
            MuSimSetPause(&m, -1);
            CHECK(m.st[0] == 2 && m.st[1] == 2 && m.resumes == 2 * want, "stream %d mode %d: -1 plays it again", kSnd[k], mode);
        }
    {   // the one-shot [0x62FFE0] is asked only when [0x107BF74] is 0
        MuSim m;
        Play(&m, 1, 1);
        m.bf74 = 1, m.oneShot62FFE0 = 1;
        MuSimSetPause(&m, 0);
        CHECK(m.selects == 1 && m.oneShot62FFE0 == 1, "0x4F8B40 not asked while 0x107BF74 is set");
        m.bf74 = 0;
        MuSimSetPause(&m, 0);
        CHECK(m.selects == 2 && m.oneShot62FFE0 == 0, "0x4F8B40 asked and cleared");
        MuSimSetPause(&m, 0);
        CHECK(m.selects == 2, "no sound once both are 0");
    }
}

int main(void)
{
    TestAct();
    TestTrips(1.0f / 30.0f);
    TestTrips(1.0f / 60.0f);
    TestStreams();
    printf("pausesound_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
