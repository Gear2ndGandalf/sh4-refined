// v3.52 QL352: the Extra Menu's Quick Load inside the pause (USER_FEEDBACK_v351 Q2, MU351's open question 2: "Quick Load
// still un-pauses for a moment first"). Pure: no game memory. extramenu.inc decides with QlPath / QlFadeStep / QlSoundStep;
// test\qlpause_test.c runs them in a model of the game's frame (the tick order below) beside v3.51's path.
//
// The game (SH4Decomp, matched; every byte checked in orig/sh4.exe md5 15a85581, 2026-11-26):
//   One game frame 0x4EF980: [0x4EF982 Refined's dispatcher QsTick (EmTick first; then 0x4F8BA0 MenuSubModeInput)]
//   [0x4EF987 0x4F9110 MenuSubModeUpdate: the pause input 0x4F8E90, the overlay callback] [the step list of the phase
//   [0xFD5C20]: 0x62BD00 -> phase 0 0x62BC78, 1 0x62BCB8, 2 0x62BCF0]. Only phase 0's list holds 0x4EF870, which runs the
//   world state machine 0x4FA750 (the room frame 0x4F0210 / 0x4EFEB0, the room leave 0x4F0260 / 0x4EFEF0, the world start
//   0x4FA4B0): a room change requested in the pause (phase 2) waits until phase 0.
//   v3.51 (pm_close(0) = the stock Resume 0x4F90C2): 0x4031F0, 0x406C30, 0x56E960(-1) (pauseMode 0: master 1.0 at once and
//   a select sound), 0x55A160(0), sub-mode 8; next frame 0x4F9110 case 8: 0x50BE50, the reset, 0x4EFAE0 (phase 0 and a
//   second -1), HUD on; then Refined's QsLoad waits for its gameplay guards (input on, no fade): the world ran and the level
//   music played, visibly, for some frames, then the door's leave (16 frames of fade over the running world, then the load).
//   v3.52: the pause turns into Refined's hand-over overlay (v3.32 PM332; the frozen picture, phase 2 and MU351's silence
//   kept), the screen fades to black there (0x565CB0(0x80, 8): 16 frames, as the load list's own fade), and only at black
//   the world restart is asked (QsRestart: 0x4F72B0) and the overlay left without its sound call: 0x4F8DA0 (no new pause
//   before the room change, as a door's 0x4F72F0 does), 0x4031F0, 0x406C30, 0x55A160(0) (Resume without 0x56E960(-1)),
//   0x50BE50, 0x4F8B80 (case 8's reset), phase 0 (0x4EFAE0 from phase 2 without its -1), 0x553CF0. The HUD stays off: the
//   loaded room's 0x4F0AC0 turns it and the sub-modes on. The master stays 0 through the leave and the world start; the
//   loaded room's first frame (0x4F0C50: fade target 0) is followed by one 0x56E960(-1) from pauseMode 2: the music fades in
//   (fade 1: from 0.2, the floor of 0x56C4E0, to full in about 0.4 s, as after any overlay), no select sound.
#ifndef QLPAUSE_SIM_H
#define QLPAUSE_SIM_H

#define QL_BLACK 0x80            // the screen fade level of black (0x565D50)
#define QL_FADE_SPEED 8          // fade steps a frame: black in 16 frames (the load list's la_cb: +8 a frame)
#define QL_FADE_MAX 90           // frames: still not black (never seen; nothing else sets the target in the pause): go anyway
#define QL_SOUND_MAX_MS 30000u   // the loaded room never seen running: the sound given back anyway (as g_qsLoading's 30 s)

// the path a Quick Load confirm takes
enum {
    QLW_PAUSE = 0,     // v3.52: inside the pause
    QLW_OFF,           // [ExtraMenu] QuickLoadInPause=0: v3.51
    QLW_NOSEAM,        // [ExtraMenu] Seamless=0: no hand-over overlay, v3.51
    QLW_UNSETTLED,     // the pause is not settled (picture, phase, a pending sub-mode, Playable Eileen's menu): v3.51
    QLW_MESSAGE        // a message is up: it is frozen by the pause; v3.51 lets it run out first (QsGuardPassing)
};
static int QlPath(int inPauseOn, int seamless, int canOpen, int message)
{
    if (!inPauseOn) return QLW_OFF;
    if (!seamless) return QLW_NOSEAM;
    if (!canOpen) return QLW_UNSETTLED;
    if (message) return QLW_MESSAGE;
    return QLW_PAUSE;
}
static const char *QlPathName(int w)
{
    switch (w) {
    case QLW_PAUSE: return "the Extra Menu inside the pause";
    case QLW_OFF: return "the Extra Menu after the pause closed (v3.51: [ExtraMenu] QuickLoadInPause=0)";
    case QLW_NOSEAM: return "the Extra Menu after the pause closed (v3.51: [ExtraMenu] Seamless=0)";
    case QLW_UNSETTLED: return "the Extra Menu after the pause closed (v3.51: the pause was not settled)";
    case QLW_MESSAGE: return "the Extra Menu after the pause closed (v3.51: a message was up)";
    default: return "the Extra Menu (?)";
    }
}

// each frame of the fade inside the overlay: frames counted from the confirm, level = [0x137F98C]
enum { QLF_WAIT = 0, QLF_GO, QLF_GO_LATE };
static int QlFadeStep(int frames, int level, int maxFrames)
{
    if (level >= QL_BLACK) return QLF_GO;
    if (frames >= maxFrames) return QLF_GO_LATE;
    return QLF_WAIT;
}

// each frame after the restart while the sound is owed. The loaded room runs once the world start has taken the request
// (startMode [0xFD5A74], nextStage [0xFD5AA0], nextRoom [0xFD5AA4] all 0) and the room's first frame has set the fade
// target [0x137F990] below black (0x4F0C50; it is black from the confirm through the leave and the world start's
// 0x565CA0(0x80)). owner: something else owns the sound now (a sub-mode up or pending, or a cut-scene, phase 1: each
// started with its own 0x56E960 call, and its close / 0x4EFAE0 calls -1): DROP, no call.
enum { QLS_HOLD = 0, QLS_BACK, QLS_BACK_LATE, QLS_DROP };
static int QlRoomRuns(int startMode, int nextStage, int nextRoom, int fadeTarget)
{
    return !startMode && !nextStage && !nextRoom && fadeTarget < QL_BLACK;
}
static int QlSoundStep(int startMode, int nextStage, int nextRoom, int fadeTarget, int owner, unsigned ms, unsigned maxMs)
{
    if (QlRoomRuns(startMode, nextStage, nextRoom, fadeTarget)) return owner ? QLS_DROP : QLS_BACK;
    if (ms >= maxMs) return owner ? QLS_DROP : QLS_BACK_LATE;
    return QLS_HOLD;
}
// what 0x56E960(-1) is given: pauseMode 0 (the pause's own) would put the master to 1.0 at once and play a select sound
// ([0x107BF74] is 1 after any pause input): marked 2 (MU351's MU_MARK_MODE) the master fades in (fade 1), no sound
static int QlSoundMark(int pauseMode) { return pauseMode == 0 ? 2 : pauseMode; }

#endif
