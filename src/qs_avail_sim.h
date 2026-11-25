// v3.33 MENU333: the pure logic behind the Extra Menu's Quick Save / Quick Load rows (extramenu.inc EmQsRows) and the
// quick-save guard's file test (quicksave.inc QsFilesLoading). No game memory: test\qs_avail_test.c
// (test\build_qs_avail_test.cmd).
//
// v3.32 drew both rows from a test made every frame that also asked "is the game's read queue empty" (0x573370(1)).
// The music / ambience streams refill through that same queue (SndVoice 0x40B..., 0x96000-byte reads from bgm.sh4 /
// se.sh4, also while the pause is up), so the rows went grey for the few frames of every refill ("flash greyed out at
// random intervals") and stayed grey where the streams refill back to back. Now: sound-pack reads do not count, the
// passing states (a fade, a data file loading, a message) do not grey a row (the confirm waits for them instead), and
// what is left is held for QS_HOLD_MS before the row changes its look.
#ifndef QS_AVAIL_SIM_H
#define QS_AVAIL_SIM_H

#define QS_HOLD_MS 300       // a row changes its look only after the new state held this long
#define QS_FLAP_WIN_MS 1000  // the window the raw changes are counted in
#define QS_FLAP_MAX 4        // more raw changes than this in one window: an anomaly (logged once a window)
#define QS_WAIT_MS 2000      // a confirm waits this long for a passing state (fade, data file, message) to end

typedef struct {
    int shown;               // what the row shows: 1 available, 0 grey
    int raw;                 // the last raw test
    unsigned rawSince;       // when raw last changed (ms, GetTickCount)
    unsigned winStart;       // the flap window's start
    int flips;               // raw changes in this window
    int flagged;             // this window has been reported
} QsAvail;

// The sub-list opens: the row shows the state of this moment at once.
static void QsAvailOpen(QsAvail *a, int raw, unsigned now)
{
    a->shown = a->raw = raw ? 1 : 0;
    a->rawSince = a->winStart = now;
    a->flips = 0;
    a->flagged = 0;
}

// One evaluation, at any rate (every drawn frame). Returns bit 0: the shown state changed; bit 1: the raw state changed
// more than QS_FLAP_MAX times inside the current window (once per window). Times are unsigned ms: wrap-safe.
static int QsAvailStep(QsAvail *a, int raw, unsigned now)
{
    int r = 0;
    raw = raw ? 1 : 0;
    if (now - a->winStart >= QS_FLAP_WIN_MS) {
        a->winStart = now;
        a->flips = 0;
        a->flagged = 0;
    }
    if (raw != a->raw) {
        a->raw = raw;
        a->rawSince = now;
        if (++a->flips > QS_FLAP_MAX && !a->flagged) {
            a->flagged = 1;
            r |= 2;
        }
    }
    if (a->shown != a->raw && now - a->rawSince >= QS_HOLD_MS) {
        a->shown = a->raw;
        r |= 1;
    }
    return r;
}

// A read-queue entry's path names a sound pack ("*.sh4": bgm.sh4, se.sh4, bd.sh4 ... the streams, sound banks and
// cutscene voices). The game's data files are "*.bin" (file.c gFileTable); a sound read never touches a save image.
static int QsIsSoundPath(const char *p, unsigned max)
{
    unsigned n = 0;
    while (n < max && p[n]) ++n;
    if (n < 4) return 0;
    return p[n - 4] == '.' && (p[n - 3] | 0x20) == 's' && (p[n - 2] | 0x20) == 'h' && p[n - 1] == '4';
}

#endif
