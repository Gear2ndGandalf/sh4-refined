// v3.52 QL352: native test of src\qlpause_sim.h (test\build_qlpause_test.cmd): the Extra Menu's Quick Load inside the pause.
// The pure choices (QlPath, QlFadeStep, QlSoundStep, QlSoundMark), then a model of the game's frame in the order of
// 0x4EF980 (Refined's dispatcher first, then 0x4F9110, then the phase's step list: only phase 0 runs the world state
// machine) with MuSim (pausesound_sim.h, the sound pause model of MU351) for the sound. v3.51's path (the pause closed like
// Resume, then QsLoad at its gameplay guards) is run beside v3.52's on every trip: the bug is reproduced (world frames
// on screen, the level music up), then shown gone (none, and the music silent until the loaded room runs).
#include <stdio.h>
#include <string.h>
#include "../src/pausesound_sim.h"
#include "../src/qlpause_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- the pure choices
static void TestPure(void)
{
    CHECK(QlPath(1, 1, 1, 0) == QLW_PAUSE, "on, seamless, settled, no message: inside the pause");
    CHECK(QlPath(0, 1, 1, 0) == QLW_OFF && QlPath(0, 0, 0, 1) == QLW_OFF, "QuickLoadInPause=0 wins: v3.51");
    CHECK(QlPath(1, 0, 1, 0) == QLW_NOSEAM, "Seamless=0: v3.51");
    CHECK(QlPath(1, 1, 0, 0) == QLW_UNSETTLED && QlPath(1, 1, 0, 1) == QLW_UNSETTLED, "the pause not settled: v3.51");
    CHECK(QlPath(1, 1, 1, 1) == QLW_MESSAGE, "a message up: v3.51 (it runs out after the pause)");
    {
        int w;
        for (w = QLW_PAUSE; w <= QLW_MESSAGE + 1; ++w) CHECK(QlPathName(w)[0] != 0, "path %d has a name", w);
        CHECK(!strcmp(QlPathName(QLW_PAUSE), "the Extra Menu inside the pause"), "the log's name of the new path");
        CHECK(strstr(QlPathName(QLW_OFF), "v3.51") && strstr(QlPathName(QLW_MESSAGE), "v3.51"), "the fallbacks say v3.51");
    }
    CHECK(QlFadeStep(1, 8, QL_FADE_MAX) == QLF_WAIT && QlFadeStep(15, 0x78, QL_FADE_MAX) == QLF_WAIT, "not black: wait");
    CHECK(QlFadeStep(16, QL_BLACK, QL_FADE_MAX) == QLF_GO && QlFadeStep(1, 0x90, QL_FADE_MAX) == QLF_GO, "black: go");
    CHECK(QlFadeStep(QL_FADE_MAX, 0x40, QL_FADE_MAX) == QLF_GO_LATE && QlFadeStep(QL_FADE_MAX - 1, 0x40, QL_FADE_MAX) == QLF_WAIT,
          "never black: go late at the limit, not before");
    CHECK(QlFadeStep(QL_FADE_MAX + 5, QL_BLACK, QL_FADE_MAX) == QLF_GO, "black and late: plain go");
    // the sound: every request field and the target, one at a time
    CHECK(QlSoundStep(0, 0, 0, 0, 0, 10, QL_SOUND_MAX_MS) == QLS_BACK, "the loaded room runs: back");
    CHECK(QlSoundStep(1, 0, 0, 0, 0, 10, QL_SOUND_MAX_MS) == QLS_HOLD, "start mode still set: hold");
    CHECK(QlSoundStep(0, 2, 0, 0, 0, 10, QL_SOUND_MAX_MS) == QLS_HOLD, "next stage still set: hold");
    CHECK(QlSoundStep(0, 0, 4, 0, 0, 10, QL_SOUND_MAX_MS) == QLS_HOLD, "next room still set: hold");
    CHECK(QlSoundStep(0, 0, 0, QL_BLACK, 0, 10, QL_SOUND_MAX_MS) == QLS_HOLD, "the fade target black (load): hold");
    CHECK(QlSoundStep(0, 0, 0, 0, 1, 10, QL_SOUND_MAX_MS) == QLS_DROP, "a menu / cut-scene owns it: drop");
    CHECK(QlSoundStep(1, 2, 4, QL_BLACK, 0, QL_SOUND_MAX_MS, QL_SOUND_MAX_MS) == QLS_BACK_LATE, "30 s: back anyway");
    CHECK(QlSoundStep(1, 2, 4, QL_BLACK, 1, QL_SOUND_MAX_MS, QL_SOUND_MAX_MS) == QLS_DROP, "30 s with an owner: drop");
    CHECK(QlSoundStep(1, 2, 4, QL_BLACK, 0, QL_SOUND_MAX_MS - 1, QL_SOUND_MAX_MS) == QLS_HOLD, "29.999 s: hold");
    CHECK(QlSoundMark(0) == 2 && QlSoundMark(2) == 2 && QlSoundMark(-1) == -1 && QlSoundMark(1) == 1 && QlSoundMark(3) == 3,
          "only the pause's own mode 0 is marked 2");
}

// ---------------------------------------------------------------- the frame model
enum { W_FRAME, W_LEAVE, W_START, W_ROOMLOAD, W_NEW };
enum { P_PAUSE, P_V351 };
#define LEAVE_WAIT 16   // 0x4F0260 / 0x4EFEF0 case 1: 16 frames after the fade request
#define START_FRAMES 45 // world start steps 1 .. 7 (files): frames (any number; the load's length)
#define LOAD_FRAMES 20  // the room load until 0x4F0AC0

typedef struct {
    // the game
    int sub, pend, phase, capture, subEnabled, hud, msg;
    int fadeLevel, fadeTarget, fadeSpeed;
    int startMode, nextStage, nextRoom;
    int world, wstep, firstDone, cutAtFirst;
    int fadeHold;   // test: something else keeps setting the fade target (never seen)
    int startHang;  // test: the world start never ends
    MuSim snd;
    // Refined
    int path, keep;           // P_PAUSE / P_V351; PauseSound
    int confirmed, qlPhase, qlFrames, qlOwed, qlLate, qlDone, refuseAtBlack, backInPause, request;
    int bridge;
    unsigned ms, owedAt, soundBackMs, result;
    // what was seen
    int oldWorldFrames, visible, loudBefore, minus1, minus1Before, selectsAtStart, newFrames, dropSeen;
    float dt;
} G;

static void Fade(G *g, int target, int speed) { g->fadeTarget = target; g->fadeSpeed = speed; }

// the stock pause opened in play (0x4F8BA0: 0x56E960(0), sub-mode -1 -> 2 a frame later, phase 2, picture taken / shown)
static void Start(G *g, int path, int keep, float dt)
{
    memset(g, 0, sizeof *g);
    g->dt = dt;
    g->path = path;
    g->keep = keep;
    g->subEnabled = 1;
    g->world = W_FRAME;
    g->firstDone = 1;
    g->snd.pauseMode = -1, g->snd.master = 1.0f, g->snd.bf74 = 1;
    g->snd.st[0] = 2, g->snd.snd[0] = 15000;  // the level's music stream (scaled by the master)
    g->snd.st[1] = 2, g->snd.snd[1] = 30010;  // a stream the pause pauses (30000 ..)
    MuSimSetPause(&g->snd, 0);
    g->sub = 2, g->phase = 2, g->capture = 2, g->hud = 0;
    g->selectsAtStart = g->snd.selects;
}

static int RoomRuns(const G *g) { return g->world == W_NEW && g->newFrames > 0; }
static void SetPause(G *g, int mode)
{
    if (mode == -1) {
        ++g->minus1;
        if (!RoomRuns(g)) ++g->minus1Before;
    }
    MuSimSetPause(&g->snd, mode);
}

// v3.52 ql_go: the restart and the silent leave
static void QlGo(G *g, int late)
{
    g->qlPhase = 0;
    g->qlLate = late;
    if (g->refuseAtBlack) {  // EmBridgeBack -> EmToPause(PM_QLOAD, 1): the pause again, sound pause 0, fading in
        SetPause(g, 0);
        g->bridge = 0, g->sub = 2, g->capture = 2;
        Fade(g, 0, 8);
        g->backInPause = 1;
        return;
    }
    g->nextStage = 2, g->nextRoom = 4, g->startMode = 1;  // QsRestart: 0x4F72B0 + [0xFD5A74] = 1
    g->bridge = 0;
    g->subEnabled = 0;               // 0x4F8DA0
    g->capture = 3;                  // 0x55A160(0)
    g->sub = 0, g->pend = 0;         // 0x4F8B80
    g->phase = 0;                    // phase only (no -1)
    g->qlOwed = 1, g->owedAt = g->ms;
    g->qlDone = 1;
}

// the loaded room: QlSoundTick
static void QlSoundTick(G *g)
{
    int r;
    if (!g->qlOwed) return;
    r = QlSoundStep(g->startMode, g->nextStage, g->nextRoom, g->fadeTarget, g->sub || g->pend || g->phase == 1,
                    g->ms - g->owedAt, QL_SOUND_MAX_MS);
    if (r == QLS_HOLD) return;
    g->qlOwed = 0;
    g->result = (unsigned)r;
    if (r == QLS_DROP) { g->dropSeen = 1; return; }
    g->snd.pauseMode = QlSoundMark(g->snd.pauseMode);
    SetPause(g, -1);
    g->soundBackMs = g->ms - g->owedAt;
}

// the confirm on the Quick Load row, inside 0x4F8E90 (sub-mode 2)
static void Confirm(G *g)
{
    g->confirmed = 1;
    if (g->path == P_PAUSE) {  // QlPauseStart: EmBridgeOpen (MuBridgeAct) + the fade to black
        const int act = MuBridgeAct(g->keep, g->snd.pauseMode);
        if (act == MU_ACT_V350) { SetPause(g, -1); SetPause(g, 2); }
        else { if (act == MU_ACT_REPAUSE) SetPause(g, 0); if (act != MU_ACT_LEAVE) g->snd.pauseMode = MU_MARK_MODE; }
        g->sub = 1, g->bridge = 1;
        Fade(g, QL_BLACK, QL_FADE_SPEED);
        g->qlPhase = 1, g->qlFrames = 0;
    } else {                   // v3.51: pm_close(0) -> the stock Resume 0x4F90C2
        SetPause(g, -1);
        g->capture = 3;
        g->sub = 8;
        g->request = 1;
    }
}

// one game frame (0x4EF980)
static void Frame(G *g, int confirmNow)
{
    // [0x4EF982] QsTick: EmTick first
    QlSoundTick(g);
    if (g->qlPhase) {
        const int r = QlFadeStep(++g->qlFrames, g->fadeLevel, QL_FADE_MAX);
        if (r != QLF_WAIT) QlGo(g, r == QLF_GO_LATE);
    } else if (g->request && g->sub == 0 && g->phase == 0 && g->hud && g->fadeLevel == 0 && g->fadeTarget == 0) {
        g->request = 0;  // v3.51 sa_check_req -> QsLoad at its gameplay guards
        g->nextStage = 2, g->nextRoom = 4, g->startMode = 1;
    }
    // [0x4EF987] 0x4F9110
    if (g->sub == 8) {  // case 8: 0x50BE50, the reset, 0x4EFAE0 (phase 0, -1), HUD on
        g->sub = 0, g->capture = 3;
        g->phase = 0;
        SetPause(g, -1);
        g->hud = 1;
    } else if (g->sub == 2 && confirmNow) {
        Confirm(g);
    }
    if (g->fadeHold && g->qlPhase) Fade(g, 0x40, 8);  // test: another writer holds the target (never seen)
    // the phase's step list: phase 0 only runs the world state machine
    if (g->phase == 0) {
        switch (g->world) {
        case W_FRAME:  // the old room's frame (0x4F0210 / 0x4EFEB0)
            ++g->oldWorldFrames;
            if (g->fadeLevel < QL_BLACK) ++g->visible;
            if (g->nextRoom) g->world = W_LEAVE, g->wstep = 0;
            break;
        case W_LEAVE:  // 0x4F0260: step 0 (sub-modes off, fade to black, 0x56E760), 16 frames, unload
            ++g->oldWorldFrames;  // the old room's tasks still run under the fade (as at any door)
            if (g->fadeLevel < QL_BLACK) ++g->visible;
            if (g->wstep == 0) {
                g->subEnabled = 0;
                Fade(g, QL_BLACK, 0x10);
                if (g->snd.st[1] == 3) g->snd.st[1] = 0;  // 0x56E760: the 30000 stream faded out (0x56BF40 takes a paused one)
                else if (g->snd.st[1]) g->snd.st[1] = 0;
            }
            if (++g->wstep > LEAVE_WAIT) g->world = W_START, g->wstep = 0;
            break;
        case W_START:  // 0x4FA4B0: step 0 applies the snapshot, clears the request and gGame +0x1A8 (phase), fade 0x80
            if (g->wstep == 0) {
                g->startMode = 0, g->nextStage = 0, g->phase = 0;
                g->fadeLevel = g->fadeTarget = QL_BLACK;
                g->snd.st[0] = 0;  // 0x56BE50: the sequence / the old music gone
            }
            if (!g->startHang && ++g->wstep > START_FRAMES) g->world = W_ROOMLOAD, g->wstep = 0;
            break;
        case W_ROOMLOAD:  // 0x4F0A50 (nextRoom 0), files, 0x4F0AC0 (HUD on, sub-modes on)
            if (g->wstep == 0) g->nextRoom = 0;
            if (++g->wstep > LOAD_FRAMES) {
                g->hud = 1, g->subEnabled = 1;
                g->world = W_NEW, g->firstDone = 0;
                g->snd.st[0] = 2, g->snd.snd[0] = 15001;  // the loaded room's music starts (at the master as it is)
            }
            break;
        case W_NEW:  // 0x4F0C50: the first frame fades in; a room-entry cut-scene may start on it
            if (!g->firstDone) {
                Fade(g, 0, 0x10);
                g->firstDone = 1;
                if (g->cutAtFirst) { g->phase = 1; SetPause(g, 2); }  // 0x4EFB30
            }
            ++g->newFrames;
            break;
        }
    } else if (g->phase == 1 && g->world == W_NEW) {
        ++g->newFrames;
        if (g->newFrames == 60) { g->phase = 0; SetPause(g, -1); }  // the cut-scene's end 0x4EFAE0
    }
    // the frame end: 0x565CD0, the sound fade
    {
        const int d = g->fadeTarget - g->fadeLevel, a = d < 0 ? -d : d;
        if (a <= g->fadeSpeed) g->fadeLevel = g->fadeTarget;
        else g->fadeLevel += d > 0 ? g->fadeSpeed : -g->fadeSpeed;
    }
    if (g->capture == 3) g->capture = 0;
    MuSimFrame(&g->snd, g->dt);
    if (g->confirmed && !RoomRuns(g) && g->snd.master > MU_SILENT) ++g->loudBefore;
    g->ms += (unsigned)(g->dt * 1000.0f + 0.5f);
}

static void Run(G *g, int frames)
{
    int i;
    Frame(g, 1);  // the confirm
    for (i = 1; i < frames; ++i) Frame(g, 0);
}

static void TestTrips(float dt)
{
    const int fps = (int)(1.0f / dt + 0.5f), total = 20 * fps;
    G g;
    // v3.51 reproduced
    Start(&g, P_V351, 1, dt);
    CHECK(g.snd.master == 0.0f && g.phase == 2, "%d fps: the pause is silent and frozen", fps);
    Run(&g, total);
    CHECK(g.visible >= 2, "%d fps: v3.51: world frames on screen before the black (%d)", fps, g.visible);
    CHECK(g.loudBefore > 0 && g.minus1Before >= 2, "%d fps: v3.51: the level music up before the loaded room (%d frames, %d x -1)",
          fps, g.loudBefore, g.minus1Before);
    CHECK(g.snd.selects > g.selectsAtStart, "%d fps: v3.51: Resume's extra select sound", fps);
    CHECK(g.world == W_NEW && g.snd.master == 1.0f, "%d fps: v3.51 loads and plays", fps);
    // v3.52
    Start(&g, P_PAUSE, 1, dt);
    Run(&g, total);
    CHECK(g.qlDone && !g.qlLate && g.qlFrames == QL_BLACK / QL_FADE_SPEED, "%d fps: black in 16 frames, then the restart (%d)",
          fps, g.qlFrames);
    CHECK(g.visible == 0, "%d fps: v3.52: no world frame on screen (%d; %d behind black)", fps, g.visible, g.oldWorldFrames);
    CHECK(g.oldWorldFrames == 1 + LEAVE_WAIT + 1, "%d fps: behind black: one frame into the leave + the leave (%d)", fps,
          g.oldWorldFrames);
    CHECK(g.loudBefore == 0, "%d fps: v3.52: the level music silent until the loaded room runs (%d loud frames)", fps, g.loudBefore);
    CHECK(g.minus1 == 1 && g.minus1Before == 0, "%d fps: exactly one -1, after the loaded room's first frame (%d / %d)", fps,
          g.minus1, g.minus1Before);
    CHECK(g.snd.selects == g.selectsAtStart, "%d fps: no extra select sound", fps);
    CHECK(g.result == QLS_BACK && g.soundBackMs > 0 && g.soundBackMs < 5000, "%d fps: the sound back when the room ran (%u ms)",
          fps, g.soundBackMs);
    CHECK(g.snd.master == 1.0f && g.snd.fadeMode == 0, "%d fps: the music at full after its fade in", fps);
    CHECK(g.sub == 0 && g.phase == 0 && g.hud && g.subEnabled && g.capture == 0 && !g.bridge, "%d fps: the loaded room plays "
          "normally (sub %d phase %d hud %d menus %d picture %d)", fps, g.sub, g.phase, g.hud, g.subEnabled, g.capture);
    CHECK(g.fadeLevel == 0, "%d fps: faded in", fps);
    CHECK(g.snd.st[1] == 0, "%d fps: the paused 30000 stream was stopped by the leave, not resumed", fps);
    // the music rises over 0.5 s after the room runs (a fade in, not a jump)
    {
        G h;
        int i, back = -1;
        float m1 = -1.0f;
        Start(&h, P_PAUSE, 1, dt);
        Frame(&h, 1);
        for (i = 1; i < total; ++i) {
            Frame(&h, 0);
            if (back < 0 && h.minus1) back = i;
            if (back >= 0 && i == back + 1) m1 = h.snd.master;
        }
        // (0x56C4E0 never sets the master below 0.2: the game's every fade in from a mute starts there and rises)
        CHECK(back > 0 && m1 >= 0.2f && m1 < 0.35f, "%d fps: one frame after -1 the master is %.3f (a fade in from 0.2, not a jump "
              "to 1.0)", fps, m1);
    }
    // PauseSound=0 (v3.50's quiet music in the overlay): no frame on screen still; the music at 0.2 is the user's choice
    Start(&g, P_PAUSE, 0, dt);
    Run(&g, total);
    CHECK(g.visible == 0 && g.loudBefore > 0 && g.snd.master == 1.0f, "%d fps: PauseSound=0: hidden, quiet music (v3.50), then full",
          fps);
}

static void TestFailures(float dt)
{
    const int fps = (int)(1.0f / dt + 0.5f), total = 40 * fps;
    G g;
    // refused at black (every catalog entry clear data): back in the pause, frozen and silent, fading in, nothing owed
    Start(&g, P_PAUSE, 1, dt);
    g.refuseAtBlack = 1;
    Run(&g, 5 * fps);
    CHECK(g.backInPause && g.sub == 2 && g.phase == 2 && g.oldWorldFrames == 0 && g.snd.master == 0.0f && !g.qlOwed &&
          g.fadeLevel == 0 && g.world == W_FRAME, "%d fps: refused at black: the pause again (sub %d phase %d frames %d master "
          "%.2f fade %d)", fps, g.sub, g.phase, g.oldWorldFrames, g.snd.master, g.fadeLevel);
    // the fade target held by something else (never seen): go late (the door's leave fades to black itself)
    Start(&g, P_PAUSE, 1, dt);
    g.fadeHold = 1;
    Run(&g, total);
    CHECK(g.qlLate && g.qlFrames == QL_FADE_MAX && g.world == W_NEW && g.loudBefore == 0 && g.minus1 == 1,
          "%d fps: never black: loaded anyway after %d frames, the music still held (%d)", fps, g.qlFrames, g.loudBefore);
    // the world start never ends: the sound given back after 30 s (BACK_LATE), once
    Start(&g, P_PAUSE, 1, dt);
    g.startHang = 1;
    Run(&g, total);
    CHECK(g.result == QLS_BACK_LATE && g.minus1 == 1 && g.soundBackMs >= QL_SOUND_MAX_MS && g.soundBackMs < QL_SOUND_MAX_MS + 100,
          "%d fps: no loaded room: the sound back at 30 s (%u ms)", fps, g.soundBackMs);
    // a cut-scene on the loaded room's first frame: its own sound calls own it; ours is dropped, the music back at its end
    Start(&g, P_PAUSE, 1, dt);
    g.cutAtFirst = 1;
    Run(&g, total);
    CHECK(g.dropSeen && g.result == QLS_DROP && g.minus1 == 1 && g.snd.master == 1.0f && g.phase == 0,
          "%d fps: a room-entry cut-scene: dropped, the cut-scene's end gives the music back (%d x -1, master %.2f)", fps, g.minus1,
          g.snd.master);
}

int main(void)
{
    TestPure();
    TestTrips(1.0f / 30.0f);
    TestTrips(1.0f / 60.0f);
    TestFailures(1.0f / 30.0f);
    TestFailures(1.0f / 60.0f);
    printf("qlpause_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
