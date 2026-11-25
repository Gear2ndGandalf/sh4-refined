// FRAME INTERPOLATION (SH4 Refined v2.1 prototype): logic stays at 30 Hz, the display runs at 60 / 120 / 180 by
// presenting interpolated copies of each recorded frame (the sh4vita method, ported to the PC's Direct3D 8 device).
// See interp.c for the design; research\framerate\v3\interpolation.md for the write-up.
#ifndef SH4_INTERP_H
#define SH4_INTERP_H

typedef struct InterpHost {
    void (*log)(const char *fmt, ...);
    // 1 while the world runs (gameplay, real-time cutscenes); 0 on the front end, in menus, while loading
    int (*gameplay)(void);
    // The logic tick in QueryPerformanceCounter units: when this tick started, when the next one will start, the
    // step. 0 = unknown (no QPC): the frame is then shown once, as without the mod.
    int (*tick)(unsigned __int64 *frameStart, unsigned __int64 *nextTick, unsigned __int64 *step);
    // a counter that advances once per logic tick (0 = none: not checked)
    unsigned (*tickId)(void);
} InterpHost;

// DllMain: displayRate = the Options "Frame Rate" row (30 / 60 / 120 / 180); refreshHz = the monitor's refresh
// (0 = unknown): the display rate is capped at the highest of 60 / 120 / 180 that the monitor shows.
void Interp_Init(const InterpHost *host, int displayRate, int refreshHz);
// The Options row changed (game thread or menu code): applies at the next frame.
void Interp_SetRate(int displayRate);
// The game thread at the end of every logic tick, before the frame's Present (0x415202): attaches the device hooks
// (once a rate above 30 is chosen) and follows a new device.
void Interp_FrameEnd(void *device);
// The display rate in force (after the monitor cap and the adaptive fallback).
int Interp_RateInForce(void);
// [Window] InterpAutoLower: 1 (default) = lower the display rate one step when the game's own ticks stretch over the
// steady state (and try again later); 0 = never (diagnosis).
void Interp_SetAutoLower(int on);
// [Window] InterpDiag=1 (not in the default ini): pacing trace + frame dumps into dir (see interp_diag.inc).
void Interp_SetDiag(int on, const char *dir);
// [Window] InterpPacing=0 (not in the default ini): the v2.1 spacing rule (diagnosis / comparison).
void Interp_SetPacing(int v21);
// [Window] InterpMatrixRule=0 (not in the default ini): judge 3- and 4-register uploads row by row as v2.1 did.
void Interp_SetMatrixRule(int on);

// v3.18.0: device calls the host makes outside the game's frame stream (the Screen Effects preview capture) pass
// through the recorder like the replays' own calls
void Interp_OwnBegin(void);
void Interp_OwnEnd(void);
#ifdef INTERP_TEST
void Interp_TestSet(int noWait, int adaptive, unsigned arenaCapBytes);
#endif
#endif
