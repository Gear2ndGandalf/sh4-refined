// v3.59 FR359: pure logic of the cut-scene / Room 302 view frame rate (ratephase.inc; test\fr359_test.c).
// No game addresses here: the callers pass the game's values in.
//
// 1. Which phase-1 scenes run at the gameplay rate (CrViewKind, CrPhase1Fast).
// 2. The stream clock: the cut-scene stream frame follows real time at 30 frames a second (CrClockStep), so a late
//    frame at a high rate never puts the pictures behind the voice, and no float rounding drifts (S2 of the research).
//    v3.59 FR359-001: a hitch is caught up (bounded per tick); only a known break restarts the clock (CrClockBreak).
// 3. The camera cut: a cut-scene stream holds one block per 30 Hz frame (the stock game never interpolates, t = 0 at
//    30); at a higher rate the game itself blends two blocks by the fractional frame, which would smear a camera cut
//    (or an actor put elsewhere between two shots) over the in-between pictures. A pair of blocks that jumps is shown
//    as its first block until the integer frame moves on: exactly the stock moment of the cut (CrJudgeBlocks).
#ifndef CUTRATE_SIM_H
#define CUTRATE_SIM_H
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------- 1. the phase rule
enum { CR_SCENE = 0, CR_WINDOW = 1, CR_PEEP = 2 };
// stage = [0xFD5A90] (1 = Room 302); viewRec = the Room 302 view record that runs (0 = none); peep = the peeping flag
static int CrViewKind(unsigned stage, unsigned viewRec, int peep)
{
    if (stage != 1) return CR_SCENE;
    if (viewRec == 0x69843Cu || viewRec == 0x698468u) return CR_WINDOW;  // the window look-out (both windows)
    if (viewRec == 0x61B6B8u || viewRec == 0x61B86Cu || viewRec == 0x61B790u || peep) return CR_PEEP;  // peepholes, door scope, hole
    return CR_SCENE;
}
// 1: this phase-1 frame runs at the gameplay rate. cutRate / viewRate = the hidden keys; snapOk = the camera-cut hook,
// clockOk = the stream clock hook, wvOk = the window view's sites (all or none)
static int CrPhase1Fast(int kind, int cutRate, int viewRate, int snapOk, int clockOk, int wvOk)
{
    if (kind == CR_WINDOW) return viewRate && wvOk;
    if (kind == CR_PEEP) return viewRate && snapOk && clockOk;
    return cutRate && snapOk && clockOk;
}

// ---------------------------------------------------------------- 2. the stream clock
// v3.59 FR359-001 (Codex inspection 2): a slow frame or a hitch of any length is real time the voice has played on, so
// it is kept as debt and caught up; the clock restarts only on a known break (CrClockBreak: the pause or another
// overlay, the game out of front, a quick load, a new scene; or the stream frame itself jumping away). Catching up is
// bounded per tick: at most CR_CLOCK_HI nominal steps and never more than one stream frame a tick (the stock 30 Hz
// pace: one block a tick, the game's own feed and frame triggers never see more), so nothing jumps or bursts.
#define CR_CLOCK_LO 0.5       // a tick advances the stream at least 0.5 and at most 3 of its nominal steps
#define CR_CLOCK_HI 3.0
#define CR_ADV_MAX 1.0        // v3.59 FR359-001: and at most one stream frame a tick (the stock pace)
#define CR_DEBT_MAX 300.0     // v3.59 FR359-001: stream frames of real time it may still catch up (10 s; was 0.1 s)
#define CR_DEBT_MIN (-1.0)
#define CR_STALL_S 30.0       // v3.59 FR359-001: a tick gap longer than this is no hitch (the PC slept): a restart
#define CR_RESYNC 2.0         // the stream frame moved away from the clock by more (a new scene, a jump): restart
typedef struct { int valid; double ideal, debt, realS, streamF, lostF, maxDebt; int breaks; } CrClock;
static void CrClockReset(CrClock *c) { memset(c, 0, sizeof *c); }
// v3.59 FR359-001: a known break (the scene paused, out of front, loaded or replaced): the next tick restarts the
// clock at the stream's own frame with no debt; the run's figures (realS, streamF) are kept for the end line
static void CrClockBreak(CrClock *c)
{
    if (c->valid) ++c->breaks;
    c->valid = 0;
    c->debt = 0.0;
}
// rate = the rate in force, dt = real seconds since the last tick of the stream (0 = unknown), frame = the stream
// frame now (player +0xE0). Returns the step to add: frame + step lands on the clock's ideal frame (no rounding drift).
static float CrClockStep(CrClock *c, int rate, double dt, float frame)
{
    const double nom = 30.0 / (double)rate;
    const double hi = CR_CLOCK_HI * nom < CR_ADV_MAX ? CR_CLOCK_HI * nom : CR_ADV_MAX;
    double adv, want, step;
    if (rate < 10 || rate > 1000) return 1.0f;
    if (!c->valid || !(dt > 0.0) || dt > CR_STALL_S || fabs((double)frame - c->ideal) > CR_RESYNC) {  // FR359-001: no 0.25 s gap rule
        c->valid = 1;
        c->ideal = frame;
        c->debt = 0.0;
        adv = nom;
    } else {
        want = 30.0 * dt + c->debt;
        adv = want < CR_CLOCK_LO * nom ? CR_CLOCK_LO * nom : want > hi ? hi : want;
        c->debt = want - adv;
        if (c->debt > CR_DEBT_MAX) {  // more than 10 s behind: the rest is lost, as in stock
            c->lostF += c->debt - CR_DEBT_MAX;
            c->debt = CR_DEBT_MAX;
        }
        if (c->debt < CR_DEBT_MIN) c->debt = CR_DEBT_MIN;
        if (c->debt > c->maxDebt) c->maxDebt = c->debt;
        c->realS += dt;
        c->streamF += adv;
    }
    c->ideal += adv;
    step = c->ideal - (double)frame;
    if (!(step > 0.0)) step = CR_CLOCK_LO * nom;
    return (float)step;
}
// the stream against real time over the clock's run, in ms (+ = the pictures ahead of the voice)
static double CrClockDriftMs(const CrClock *c) { return (c->streamF / 30.0 - c->realS) * 1000.0; }

// ---------------------------------------------------------------- 3. the camera cut
// Thresholds per 30 Hz stream frame (a cut-scene's camera dollies a few units a frame; a cut moves or turns it at once)
#define CR_CAM_MOVE 150.0f     // the camera moved this far in one frame: a cut
#define CR_CAM_MOVE_MIN 40.0f  // ... or this far and over 4 x its last frame's move (+ 10)
#define CR_CAM_TURN 12.0f      // degrees in one frame
#define CR_CAM_FOV 0.08f       // the view angle changed by 8 % in one frame
#define CR_OBJ_MOVE 250.0f     // an actor's bone moved this far in one frame: it was put elsewhere
enum { CR_CUT_CAM_MOVE = 1, CR_CUT_CAM_TURN = 2, CR_CUT_CAM_FOV = 4, CR_CUT_OBJ = 8 };

static float CrDist3(const float *a, const float *b)
{
    const float x = b[0] - a[0], y = b[1] - a[1], z = b[2] - a[2];
    return sqrtf(x * x + y * y + z * z);
}
static float CrQuatAngleDeg(const float *a, const float *b)  // the angle between two rotations (quaternions x y z w)
{
    const float na = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + a[3] * a[3]);
    const float nb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2] + b[3] * b[3]);
    float d;
    if (!(na > 1e-6f) || !(nb > 1e-6f)) return 0.0f;
    d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]) / (na * nb);
    if (d > 1.0f) d = 1.0f;
    return 2.0f * acosf(d) * 57.29578f;
}
static int CrCamJudge(float move, float turnDeg, float fovRel, float prevMove)
{
    int m = 0;
    if (!(move <= CR_CAM_MOVE) || (move > CR_CAM_MOVE_MIN && move > 4.0f * prevMove + 10.0f)) m |= CR_CUT_CAM_MOVE;  // NaN too
    if (!(turnDeg <= CR_CAM_TURN)) m |= CR_CUT_CAM_TURN;
    if (!(fovRel <= CR_CAM_FOV)) m |= CR_CUT_CAM_FOV;
    return m;
}

// The game's stream layout (decomp include/cutscene.h): player slots[16] x 12 bytes {obj +0, short id +4, rec +8};
// rec {short id, short ofs (in a block), short chanNum, short chanOfs (from rec)}; channel 8 bytes {short type,
// short index, short ofs}; a block {short magic, short frame, data}. Kinds after the version shift (version < 1: kinds
// >= 0xD + 0x73): 0x80 camera rotation, 0x81 camera position, 0x82 camera value (index 0 = the view angle),
// 4 / 0x84 positions (bones, lights).
#define CR_BLOCK_MAX 0x2600
#define CR_CHAN_MAX 128
typedef struct { int mask; float camMove, camTurn, camFov, objMove; int haveCam; } CrJudge;
static void CrJudgeBlocks(const unsigned char *player, const unsigned char *cur, const unsigned char *next, int version,
                          float prevCamMove, CrJudge *out)
{
    int i, j;
    memset(out, 0, sizeof *out);
    for (i = 0; i < 16; ++i) {
        const unsigned char *slot = player + i * 12;
        const unsigned char *rec;
        int ofs, n, cofs;
        if (!*(void *const *)slot) continue;
        rec = *(const unsigned char *const *)(slot + 8);
        if (!rec) continue;
        ofs = *(const short *)(rec + 2);
        n = *(const short *)(rec + 4);
        cofs = *(const short *)(rec + 6);
        if (ofs < 0 || n <= 0 || n > CR_CHAN_MAX) continue;
        for (j = 0; j < n; ++j) {
            const unsigned char *ch = rec + cofs + j * 8;
            int kind = *(const short *)ch;
            const int index = *(const short *)(ch + 2), at = ofs + *(const short *)(ch + 4);
            const float *a, *b;
            if (at < 4 || at + 16 > CR_BLOCK_MAX) continue;
            if (version < 1 && kind >= 0xD) kind += 0x73;
            a = (const float *)(cur + at);
            b = (const float *)(next + at);
            if (kind == 0x81) {
                const float d = CrDist3(a, b);
                out->haveCam = 1;
                if (!(d <= out->camMove)) out->camMove = d;
            } else if (kind == 0x80) {
                const float t = CrQuatAngleDeg(a, b);
                out->haveCam = 1;
                if (t > out->camTurn) out->camTurn = t;
            } else if (kind == 0x82 && index == 0) {
                const float base = fabsf(a[0]) > 1e-3f ? fabsf(a[0]) : 1e-3f;
                const float r = fabsf(b[0] - a[0]) / base;
                if (!(r <= out->camFov)) out->camFov = r;
            } else if (kind == 4) {
                const float d = CrDist3(a, b);
                if (!(d <= out->objMove)) out->objMove = d;
            }
        }
    }
    out->mask = CrCamJudge(out->camMove, out->camTurn, out->camFov, prevCamMove);
    if (!(out->objMove <= CR_OBJ_MOVE)) out->mask |= CR_CUT_OBJ;
}

// ---------------------------------------------------------------- 4. the window view's spawn delays
// A delay rand() % M + B ticks at 30 is (M - 1) m + 1 / B m at m = N / 30. An imm8 base holds at most 127: the modulus
// then grows by twice what the base lacks, so the mean delay (B + (M - 1) / 2) stays exact.
static void CrDelay(int M, int B, int m, int *Mo, int *Bo)
{
    int b = B * m, mm = (M - 1) * m + 1;
    if (b > 127) {
        mm += 2 * (b - 127);
        b = 127;
    }
    *Mo = mm;
    *Bo = b;
}
#endif
