// v3.32 PM332: native test of src\em_seam_sim.h (test\build_em_seam_test.cmd) and a tick model of the Extra Menu's
// Storage Chest / Save Game / Load Game round trips, v3.31 against v3.32.
//
// The model follows SH4Decomp: the main loop draws the frozen picture at the frame start when [0x12E3E2C] is 1 or 2
// (0x55AE20), runs the tick (Refined's 0x4EF982 engine EmTick, then the overlay manager 0x4F9110 with the sub-mode's
// callback, then the phase's task list: the world moves only in phase 0), and ends the frame (0x55ABF0: 3 -> 0, 1 -> the
// copy). 0x4F8DD0 = sub-mode -1 + pending; 0x4F9110 applies a pending sub-mode one tick later (phase 2); -1 -> 0 + take
// the picture; 8 -> 0, phase 0, picture dropped. 0x4F8E60 = sub-mode 8, picture marked to drop (3). Resume (4F90C2) = the
// same. The targets: the chest fades to black (8 ticks), runs, fades out (8) and leaves; the save menu fades and leaves
// at full picture; the load list is cancelled. Measured from the confirm to the pause list back: ticks in phase 0
// (the world ran) and frames with phase 2 and no picture (a black frame).
#include <stdio.h>
#include "../src/em_seam_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

enum { CB_NONE, CB_BRIDGE, CB_TARGET };
enum { T_CHEST, T_SAVE, T_LOAD };
typedef struct {
    int sub, pend, cb, phase, cap;
    int seamless, target, fileWait, runTicks;
    int stage;      // 0 requested, 1 waiting for files, 2 target overlay runs, 3 done (target left), 4 back in the pause
    int tgtTicks, fadeIn, worldTicks, blackFrames, ticks, reopenWait;
} M;

static void Leave(M *m) { m->sub = EMS_LEAVING; m->cap = EMC_END; }  // 0x4F8E60 / Resume
static void RequestOverlay(M *m) { m->sub = EMS_STARTING; m->pend = EMS_OVERLAY; m->cb = CB_NONE; }  // 0x4F8DD0

// v3.32: EmToPause
static void ToPause(M *m)
{
    if (m->cap == EMC_END) m->cap = EMC_SHOW;
    m->cb = CB_NONE, m->pend = 0, m->sub = EMS_PAUSE;
    m->stage = 4;
}

// the target overlay's callback, one frame
static void Target(M *m)
{
    ++m->tgtTicks;
    if (m->tgtTicks < m->runTicks) return;
    if (m->target == T_LOAD) {  // cancel: la_cb leaves itself (v3.31) or turns into the pause (v3.32)
        if (m->seamless && EmSeamCanReturn(1, m->sub, m->pend, m->phase, m->cap, 0)) { ToPause(m); return; }
        Leave(m);
        m->stage = 3;
        return;
    }
    Leave(m);  // the chest task / the save task call 4D0C90 themselves
    m->stage = 3;
    if (m->seamless && EmSeamCanReturn(1, m->sub, m->pend, m->phase, m->cap, 0)) ToPause(m);  // ch_to_pause / sa_cb_body
}

static void Engine(M *m)  // EmTick
{
    if (m->stage == 0) {
        const int inBridge = m->sub == EMS_OVERLAY && m->cb == CB_BRIDGE && !m->pend;
        if ((m->sub || m->pend) && !inBridge) return;  // pm_defer / EmSubBusy
        m->stage = m->fileWait ? 1 : 2;
        if (m->stage == 2) {
            if (inBridge) m->cb = CB_TARGET;  // EmOverlay
            else RequestOverlay(m), m->cb = CB_TARGET;  // 0x4F8E40 right after
        }
        return;
    }
    if (m->stage == 1) {
        const int inBridge = m->sub == EMS_OVERLAY && m->cb == CB_BRIDGE && !m->pend;
        if (--m->fileWait > 0) return;
        m->stage = 2;
        if (inBridge) m->cb = CB_TARGET;
        else RequestOverlay(m), m->cb = CB_TARGET;
        return;
    }
    if (m->stage == 3 && m->sub == EMS_NONE && !m->pend) {  // v3.31: ch_run / finish, then pm_ret after the fade-in
        if (m->target == T_CHEST || m->target == T_LOAD) {
            if (!m->fadeIn) m->fadeIn = 16;  // 565CB0(0, 8): 0x80 / 8
            if (--m->fadeIn > 0) return;
        }
        if (++m->reopenWait < 2) return;  // pm_ret_arm, then pm_ret_wait
        m->sub = EMS_STARTING, m->pend = EMS_PAUSE, m->cb = CB_NONE;  // pm_reopen
        m->stage = 5;
    }
}

static void Manager(M *m)  // 0x4F9110 (pause input aside)
{
    if (m->pend && m->sub != EMS_STARTING && m->sub != m->pend) m->sub = m->pend, m->pend = 0, m->phase = 2;
    switch (m->sub) {
    case EMS_STARTING:
        m->sub = EMS_NONE;
        if (m->cap == EMC_NONE) m->cap = EMC_TAKE;
        break;
    case EMS_LEAVING:
        m->sub = EMS_NONE, m->pend = 0, m->cb = CB_NONE, m->cap = EMC_END, m->phase = 0;
        break;
    }
    if (m->sub > 0 && m->cb == CB_TARGET) Target(m);
}

static void Frame(M *m)
{
    if (m->cap == EMC_TAKE) m->cap = EMC_SHOW;            // 0x55AE20
    if (m->phase == 2 && m->cap != EMC_SHOW) ++m->blackFrames;  // phase 2 draws no world
    Engine(m);
    Manager(m);
    if (m->phase == 0) ++m->worldTicks;
    if (m->cap == EMC_END) m->cap = EMC_NONE;            // 0x55ABF0
    ++m->ticks;
}

static M Run(int seamless, int target, int fileWait)
{
    M m = { 0 };
    m.sub = EMS_PAUSE, m.phase = 2, m.cap = EMC_SHOW;  // the pause, settled
    m.seamless = seamless, m.target = target, m.fileWait = fileWait;
    m.runTicks = target == T_CHEST ? 40 : target == T_SAVE ? 60 : 30;
    // the confirm (inside the pause input): v3.32 the hand-over overlay, v3.31 Resume
    if (seamless && EmSeamCanOpen(1, m.sub, m.pend, m.phase, m.cap, 0)) m.sub = EMS_OVERLAY, m.cb = CB_BRIDGE;
    else Leave(&m);
    while (m.ticks < 2000) {
        Frame(&m);
        if (m.sub == EMS_PAUSE && (m.stage == 4 || m.stage == 5)) break;
    }
    return m;
}

static void TestPredicates(void)
{
    int sub, pend, phase, cap, pe, s;
    for (s = 0; s < 2; ++s)
        for (sub = -1; sub <= 9; ++sub)
            for (pend = 0; pend <= 2; ++pend)
                for (phase = 0; phase <= 2; ++phase)
                    for (cap = 0; cap <= 3; ++cap)
                        for (pe = 0; pe <= 1; ++pe) {
                            const int open = EmSeamCanOpen(s, sub, pend, phase, cap, pe);
                            const int ret = EmSeamCanReturn(s, sub, pend, phase, cap, pe);
                            CHECK(open == (s && sub == 2 && !pend && phase == 2 && cap == 2 && !pe), "open %d %d %d %d %d %d",
                                  s, sub, pend, phase, cap, pe);
                            CHECK(ret == (s && phase == 2 && !pend && (sub == 1 || sub == 8) && (cap == 2 || cap == 3) && !pe),
                                  "return %d %d %d %d %d %d", s, sub, pend, phase, cap, pe);
                            if (phase != 2) CHECK(!open && !ret, "never outside phase 2");
                        }
}

static void TestRoundTrips(void)
{
    static const char *kName[3] = { "chest", "save", "load (cancelled)" };
    int t;
    for (t = 0; t < 3; ++t) {
        const int wait = t == T_CHEST ? 0 : 12;  // snap_save.bin + save_icon.bin: some ticks
        const M o = Run(0, t, wait), n = Run(1, t, wait);
        printf("  %-17s v3.31: %3d world ticks, %d black frames | v3.32: %d world ticks, %d black frames\n", kName[t],
               o.worldTicks, o.blackFrames, n.worldTicks, n.blackFrames);
        CHECK(o.worldTicks >= 2, "%s: the model shows v3.31's unpause (%d)", kName[t], o.worldTicks);
        CHECK(n.worldTicks == 0, "%s: the world never runs (%d ticks)", kName[t], n.worldTicks);
        CHECK(n.blackFrames == 0, "%s: no frame without the frozen picture (%d)", kName[t], n.blackFrames);
        CHECK(n.sub == EMS_PAUSE && n.phase == 2 && n.cap == EMC_SHOW && n.stage == 4, "%s: back in the pause", kName[t]);
    }
}

int main(void)
{
    TestPredicates();
    TestRoundTrips();
    printf("em_seam_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
