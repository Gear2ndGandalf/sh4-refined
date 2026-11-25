// v3.32 PM332: the pure predicates of extramenu.inc's seamless hand-over (pause <-> Storage Chest / Save Game / Load
// Game without a gameplay tick). No game memory: test\em_seam_test.c builds them with a model of the overlay manager
// 0x4F9110 (test\build_em_seam_test.cmd).
#ifndef EM_SEAM_SIM_H
#define EM_SEAM_SIM_H

// sub-mode [0x107BF54] values (SH4Decomp include/menu.h) and the frozen picture [0x12E3E2C] (include/system.h)
enum { EMS_STARTING = -1, EMS_NONE = 0, EMS_OVERLAY = 1, EMS_PAUSE = 2, EMS_LEAVING = 8 };
enum { EMC_NONE = 0, EMC_TAKE = 1, EMC_SHOW = 2, EMC_END = 3 };

// The pause may turn into our hand-over overlay: the pause up and settled (nothing pending, phase 2, the picture shown),
// Playable Eileen's own menu not open.
static int EmSeamCanOpen(int seamless, int sub, int pend, int phase, int capture, int peOpen)
{
    return seamless && sub == EMS_PAUSE && !pend && phase == 2 && capture == EMC_SHOW && !peOpen;
}

// An overlay of ours may turn back into the pause: phase 2 still (the world never ran), nothing pending, sub-mode 1 (ours)
// or 8 (the target's own 0x4F8E60 in this tick), the picture shown or only marked to drop at this frame's end.
static int EmSeamCanReturn(int seamless, int sub, int pend, int phase, int capture, int peOpen)
{
    return seamless && phase == 2 && !pend && (sub == EMS_OVERLAY || sub == EMS_LEAVING) &&
           (capture == EMC_SHOW || capture == EMC_END) && !peOpen;
}

#endif
