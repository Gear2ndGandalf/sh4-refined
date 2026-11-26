// v3.51 MU351: the level music stays paused in the Extra Menu's Storage Chest / Save Game / Load Game (USER_FEEDBACK_v350
// item 11: "When in the load menu I can hear the level music unpauses"). Pure: no game memory. extramenu.inc decides with
// MuBridgeAct / MuMusicState; test\pausesound_test.c runs them against MuSim, a model of the game's sound pause.
//
// The game (SH4Decomp src/sound/sndfade_56dc30.c, checked against orig/sh4.exe 2026-11-25):
//   SoundSetPause 0x56E960(mode) writes gSndSys.pauseMode [0x13D6668] last; that field is read in one place only, its own
//   -1 case (0x56E97A). The master fade volume is [0x13CDBDC] (0x56C580 = fld [13CDBDC]); the music sequence, the streams
//   10000 .. 39999 and the world's sound effects are scaled by it, the menu sounds (0x56CC40) and the streams below 10000
//   are not. gSndSys.fadeMode [0x13D666C]: 1 up to 1.0, 2 down to 0.2, 3 down to 0, 4 / 5 wait for 0 / 1 (0x56E310).
//   -1: was 0 -> select sound 0x9C48 if [0x107BF74] (the pause input ran), master 1.0 at once, fade 5; else fade 1 (and
//       from 3 [0x13D6670] = 0); every paused stream (state 3) plays again.
//    0: select sound 0x9C48 if [0x107BF74] or the one-shot [0x62FFE0]; master 0 at once, fade 4; streams of 30000 .. or in
//       the list 0x69DAB0 (10047 .. 10061) are paused. The stock pause menu's own call (0x4F8BA0).
//    1: fade 2; the same streams as 0 paused.
//    2: fade 2 (down to 0.2: 0x56C4E0 never goes below 0.2); streams 0 .. 9999, 30000 .. and the list paused. The stock
//       overlays from play (0x4F8DD0: the save notebook, the chest, the map, the inventory).
//    3: [0x13D6670] = 1, fade 3, 0x56DAF0 (the streams stop): films.
// The stock pause menu opens the map and the memo list from the pause without any sound call (0x4F8E90 cases 2 / 3 / 4:
// they also clear [0x107BF74] so that leaving the map makes no select sound): the pause's silence goes on. v3.32 PM332's
// hand-over called -1 then 2 (as Resume + 0x4F8DD0 would): the master jumped to 1.0 and fell to 0.2 in 0.4 s, and stayed
// there for the whole list: the level music came back, quietly. MU351 makes no call at all (the pause's mode 0 stays: master
// 0, the streams as the pause left them) and marks pauseMode 2 (a plain write, MU_MARK_MODE), so that each way out behaves
// as it did in v3.50: the target's own leave (0x4F8E60 = -1) then rises from 0.2 over 0.4 s with no extra select sound,
// and EmToPause's 0 mutes again. [ExtraMenu] PauseSound=0 = v3.50 (-1 then 2).
#ifndef PAUSESOUND_SIM_H
#define PAUSESOUND_SIM_H

#define MU_MARK_MODE 2   // what pauseMode says while the list is up (as in v3.50): the next -1 fades in, no select sound
#define MU_SILENT 0.001f // the master at or below this: silent (the game's own values are 0 / 0.2 / 1)

// what EmBridgeOpen does with the sound
enum {
    MU_ACT_V350 = 0,   // PauseSound=0: 0x56E960(-1), then (2) (v3.32 .. v3.50)
    MU_ACT_KEEP,       // the pause's own mode 0: no call, pauseMode marked MU_MARK_MODE
    MU_ACT_REPAUSE,    // the pause was not in mode 0 (never seen: the stock pause always is): 0x56E960(0) first, then KEEP
    MU_ACT_LEAVE       // mode 3 (a film's streams stopped) or an unknown value: no call, no mark (logged)
};
static int MuBridgeAct(int keep, int pauseMode)
{
    if (!keep) return MU_ACT_V350;
    if (pauseMode == 0) return MU_ACT_KEEP;
    if (pauseMode == -1 || pauseMode == 1 || pauseMode == 2) return MU_ACT_REPAUSE;
    return MU_ACT_LEAVE;
}

// what the log says of the music: from the master and the fade (pauseMode only says what the next -1 will do)
enum { MU_MUSIC_PAUSED = 0, MU_MUSIC_QUIET, MU_MUSIC_RISING, MU_MUSIC_PLAYING };
static int MuMusicState(float master, int fadeMode)
{
    if (fadeMode == 1 || fadeMode == 5) return MU_MUSIC_RISING;
    if (master <= MU_SILENT) return MU_MUSIC_PAUSED;
    if (fadeMode == 2 || master < 0.999f) return MU_MUSIC_QUIET;
    return MU_MUSIC_PLAYING;
}
static const char *MuMusicName(int s)
{
    switch (s) {
    case MU_MUSIC_PAUSED: return "the level music stays paused";
    case MU_MUSIC_QUIET: return "the level music PLAYS quietly";
    case MU_MUSIC_RISING: return "the level music comes back";
    default: return "the level music PLAYS";
    }
}

// ---------------------------------------------------------------- MuSim: a model of the game's side (for the test)
#define MU_LIST_N 15  // 0x69DAB0: 10047 .. 10061
typedef struct {
    int pauseMode, fadeMode, unk50;
    float master;
    int bf74, oneShot62FFE0;   // [0x107BF74], [0x62FFE0]
    int st[2], saved[2], snd[2];  // the two streams: state (3 = paused), the state kept while paused, the sound id
    int selects;               // 0x9C48 started by the pause calls
    int resumes, pauses;       // stream play / pause calls (0x5698B0(i, 0 / 1))
} MuSim;

static int MuSimListed(int s) { return s >= 10047 && s < 10047 + MU_LIST_N; }
static float MuSimClamp(float v, float lo) { return v < lo ? lo : v > 1.0f ? 1.0f : v; }

static void MuSimSetPause(MuSim *m, int mode)
{
    int i;
    switch (mode) {
    case -1:
        if (m->pauseMode == 0) {
            if (m->bf74) ++m->selects;
            m->master = 1.0f;
            m->fadeMode = 5;
        } else {
            m->fadeMode = 1;
            if (m->pauseMode == 3) m->unk50 = 0;
        }
        for (i = 0; i < 2; ++i)
            if (m->st[i] == 3) {
                ++m->resumes;
                m->st[i] = m->saved[i];
                m->saved[i] = 3;
            }
        break;
    case 0:
    case 1:
        if (mode == 0) {
            if (m->bf74) {
                ++m->selects;
            } else if (m->oneShot62FFE0) {  // 0x4F8B40 is asked only when [0x107BF74] is 0, and clears it
                m->oneShot62FFE0 = 0;
                ++m->selects;
            }
            m->master = 0.0f;
            m->fadeMode = 4;
        } else {
            m->fadeMode = 2;
        }
        for (i = 0; i < 2; ++i)
            if (m->st[i] != 3 && (m->snd[i] >= 30000 || MuSimListed(m->snd[i]))) {
                ++m->pauses;
                m->saved[i] = m->st[i];
                m->st[i] = 3;
            }
        break;
    case 2:
        m->fadeMode = 2;
        for (i = 0; i < 2; ++i)
            if (m->st[i] != 3 && ((m->snd[i] >= 0 && m->snd[i] < 10000) || m->snd[i] >= 30000 || MuSimListed(m->snd[i]))) {
                ++m->pauses;
                m->saved[i] = m->st[i];
                m->st[i] = 3;
            }
        break;
    case 3:
        m->unk50 = 1;
        m->fadeMode = 3;
        break;
    }
    m->pauseMode = mode;
}

// 0x56E310 SoundFadeUpdateC, one frame of dt seconds (fade times: [2] = 0.5 s, [1] = 1.5 s)
static void MuSimFrame(MuSim *m, float dt)
{
    float v;
    switch (m->fadeMode) {
    case 1:
        v = dt / 0.5f + m->master;
        if (v > 1.0f) m->fadeMode = 0;
        m->master = MuSimClamp(v, 0.2f);
        break;
    case 2:
        v = m->master - dt / 0.5f;
        if (v < 0.2f) m->fadeMode = 0;
        m->master = MuSimClamp(v, 0.2f);
        break;
    case 3:
        v = m->master - dt / 1.5f;
        if (v < 0.0f) m->fadeMode = 0;
        m->master = MuSimClamp(v, 0.0f);
        break;
    }
    if (m->fadeMode == 4 && m->master == 0.0f) m->fadeMode = 0;
    else if (m->fadeMode == 5 && m->master == 1.0f) m->fadeMode = 0;
}

#endif
