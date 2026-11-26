// ================================================================ v3.7.0: the pause Extra Menu (Pause Menu+ v11 in C)
// A translation of the cheat table's entry 6017 "Pause Menu+" v11 (SILENT HILL 4.CT lines 35-2660, Auto Assembler).
// Plan and RE: staging/extra_menu/PLAN.md, RE.md. The table's symbol names are kept (pm_*, sa_*, la_*, ch_*, eo_*).
// All addresses are absolute VAs of the GOG v7 exe. Included by sh4windowmod.c after quicksave.inc (uses Site / SiteOk,
// Poke / PokeBranch, Encode, Log, g_emOn, SH4R_Extra, SH4WM_Patches and quicksave.inc's QsGuard / QsSaveEx / QsLoad /
// QsAnyValid / QsRestart / g_qsAnyValid / g_qsMsg*).
// Patches (RE 1.2, the table's 8, same byte ranges): 4F8A30 pause draw (5), 4F8F2E mouse rect y (8), 4F8F5D mouse row
// count (7), 4F8FAC Down limit (8), 4F8FE9 confirm dispatch (5), 4F8EC1 / 4F8ED3 the two cancel jne (6 each), 512235
// item-box "put in" check (6). The table's per-frame engine sa_code (its hook at 4F8BA0) is EmTick, run first in every
// game tick by the 0x4EF982 dispatcher (quicksave.inc QsTick -> g_extraTick); 4F8BA0 stays stock.
// Not ported (PLAN 5): the RAM quick save (Refined's disk QsSaveEx / QsLoad instead), sa_force, pm_quit (no disable),
// the table's user controls (ch_label / pm_back_label / pm_eo_label / pm_eo_x / pm_hdr_col / names: fixed to the
// table's defaults), pm_return (always 1).
// v3.37 CP337 (USER_FEEDBACK_v3361 item 4): a Camera row in the Extra Menu (shown while SH4 Refined's camera owns the view,
// OTS or Game) opens the Camera page, a third list level (pm_sub 2): the twelve settings of camera.inc + Back, label and
// value, Left / Right change a value at once (CamPauseStep: the same tables, texts and CamWriteKey as Option 6's switch),
// Up / Down move, Cancel / Back return to the Extra Menu on the Camera row. 13 rows at 0x16 (the row step pm_step also
// feeds the mouse rows, hkPmMouseN). The world is not drawn under the pause (the frozen picture), so the view changes when
// play resumes; the page says so. No new exe site: the page lives in the 8 hooks above and EmTick.

#define EM_I(a) (*(volatile int *)(a))
#define EM_U(a) (*(volatile DWORD *)(a))
#define EM_B(a) (*(volatile BYTE *)(a))
typedef int(__cdecl *EmFn0)(void);
typedef int(__cdecl *EmFn1)(int);
typedef int(__cdecl *EmFn2)(int, int);
typedef void(__cdecl *EmText)(int, int, int, int, int, int);
#define EM_CALL0(va) (((EmFn0)(va))())
#define EM_CALL1(va, a) (((EmFn1)(va))((int)(a)))
#define EM_CALL2(va, a, b) (((EmFn2)(va))((int)(a), (int)(b)))
#define EM_SOUND(id) EM_CALL1(0x56CC40, id)  // 9C48 select, 9C44 buzzer, 9C42 cancel, 9C40 cursor, 9C49 notebook
#define EM_SUB EM_I(0x107BF54)               // sub-mode: 0 gameplay, 1 overlay, 2 pause, 8 closing
#define EM_PEND EM_I(0x107BF58)              // pending sub-mode
#define EM_CUR EM_I(0x107BF68)               // pause cursor (visual row)
#define EM_EPI 0x4F90E6                      // 4F8E90 epilogue: the pause stays
#define EM_RESUME 0x4F90C2                   // stock Resume: 4031F0, 406C30, 56E960(-1), 55A160(0), sub-mode 8
// v3.32 PM332: the seamless hand-over (USER_FEEDBACK_v331 G: "the game briefly unpauses while entering / exiting the
// Storage Chest, Save Game, Load Game menus"). v3.7.0 closed the pause like Resume (sub-mode 8 -> 0: 0x4F9166 runs
// 0x4EFAE0, game phase 0, sound back, the frozen picture dropped) and opened the target overlay from gameplay (0x4F8DD0:
// sub-mode -1, then 1 and phase 2 a tick later): 2 world ticks, plus every tick of the save / load file wait (snap_save.bin
// 1.1 MB). On the way back the overlay left (sub-mode 8 -> 0) and the pause was reopened only after the fade-in: the world
// ran through the whole fade-in. Now the pause turns into a sub-mode 1 overlay of ours in place (phase 2, the frozen
// picture [0x12E3E2C] = 2 and the sound pause all stay), the request runs inside it, the target's callback replaces ours;
// when the target leaves, it turns straight back into the pause (sub-mode 2) in the same tick, frozen picture kept.
#define EM_CB EM_U(0x107BF5C)                // overlay callback (4F9110 calls it every frame while the sub-mode > 0)
#define EM_CAPTURE EM_I(0x12E3E2C)           // frozen picture: 0 none, 1 take at frame end, 2 shown, 3 drop at frame end
#define EM_PHASE EM_I(0xFD5C20)              // game phase: 0 gameplay, 1 cutscene, 2 overlay (world frozen)
#include "em_seam_sim.h"                     // EmSeamCanOpen / EmSeamCanReturn (test\em_seam_test.c)
// v3.51 MU351: the game's sound pause (SH4Decomp include/snd_fade.h gSndSys 0x13D6620; pausesound_sim.h, test\pausesound_test.c)
#define MU_PAUSEMODE EM_I(0x13D6668)              // gSndSys.pauseMode: the last 0x56E960 mode (read only by its -1 case)
#define MU_FADEMODE EM_I(0x13D666C)               // gSndSys.fadeMode: the master fade
#define MU_MASTER (*(volatile float *)0x13CDBDC)  // the master fade volume (0x56C580): the music, the world's sounds
#include "pausesound_sim.h"                       // MuBridgeAct / MuMusicState

// ---------------------------------------------------------------- data (RE 1.3; CT 537-749)
// row ids (pm_rows / pm_okt index), CT 598-612
enum { PM_RESUME, PM_EXTRA, PM_OPTIONS, PM_MAP, PM_SCRAP, PM_QUIT, PM_CHEST, PM_SAVE, PM_LOAD, PM_EILEEN, PM_BACK,
       PM_QSAVE, PM_QLOAD, PM_PE,
       PM_CAMERA, PM_CAMR0, PM_CAMBACK = PM_CAMR0 + CAMR_N,  // v3.37 CP337: the Camera row and its page's rows (+ Back)
       PM_IDS };

static int sa_request, la_request, ch_request;
static int ql_request;                   // the Quick Load row: QsLoad() at the first tick with sub-mode 0 (table: ql_request)
static int sa_active, sa_status, sa_frames, sa_abort, sa_loading, sa_kind;
static int sa_stuck;                     // [not in the table] frames the save overlay looked stuck (internal sa_abort)
static int la_active, la_restart, la_state, la_phase, la_fade;
static int pm_defer, pm_defer_frames;
static DWORD pm_col;
static volatile LONG ch_active, ch_blocked;  // read / counted by hkChDep
static int ch_status, ch_phase = 3, ch_frames;
static int pm_okt[PM_IDS] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static volatile LONG pm_n = 5, pm_my0 = 0x86;  // read by the naked stubs
static int pm_y0 = 0x7E, pm_sub, pm_want = -1, pm_nf;
static int pm_vis[16] = { 0, 2, 3, 4, 5 };
// v3.37 CP337: the Camera page. pm_step = the row step (read by hkPmMouseN; 0x1C as stock, PM_CAM_STEP on the page)
#define PM_CAM_STEP 0x16  // 13 rows (the step v3.33's Options Camera page used)
#define PM_CAM_MID 0xBE   // their middle row's y (the stock lists: 0xB6)
#define PM_CAM_HDR 0x20   // the header above the first row (the Extra Menu's: 0x2A)
static volatile LONG pm_step = 0x1C;
static int pm_camRow;     // the page's row when it was left (opened on it again; Back: the first row)
static CamRepeat pm_camRep;
static int pm_heldOk;     // 0x5542A0 (held directions) is stock or Playable Eileen's: Left / Right repeat while held
static BYTE pm_str_camera[32], pm_str_lt[8], pm_str_gt[8], pm_str_camhint[64], pm_str_camhint2[64];
typedef char PmVisFits[CAMR_N + 1 <= 16 ? 1 : -1];
typedef char PmOktFits[PM_IDS == 28 ? 1 : -1];  // (the pm_okt initialiser above lists 28)
static int pm_ret_row, pm_ret_go, pm_ret_frames, pm_ret_n;
static DWORD la_sm[16], la_sm_save[16];  // private copy of the nested state block 1236D48 (40h bytes, CT 649-651)
static BYTE la_buf[0x5A40];
static int pm_rows[PM_IDS][2];           // bank, id; bank -1 = id is a raw string pointer (CT 653-681)
static const int pm_eo_label = 1, pm_eo_x = 0xFA;
static const DWORD pm_hdr_col = 0x606060;
static BYTE pm_str_cheats[32], pm_str_back[32], pm_str_save[32], pm_str_load[32], pm_str_chest[32], pm_str_qsave[32],
    pm_str_qload[32], pm_str_pe[32], pm_str_eileen[32], pm_str_def[32], pm_str_dress[32], pm_str_nurse[32];
static const BYTE *pm_eo_strs[3] = { pm_str_def, pm_str_dress, pm_str_nurse };  // by eo_outfit -1 / 0 / 1
// v3.32 PM332: the seamless hand-over (hidden [ExtraMenu] Seamless=0: v3.31's close-and-reopen)
static int g_emSeamless = 1;
static int g_emBridge, g_emBridgeFrames, g_emBridgeIdle, g_emSeamN;
static void hkEmBridge(void);
static int g_emPauseSound = 1;  // v3.51 MU351: hidden [ExtraMenu] PauseSound (1 = the pause's silence goes on in the lists; 0 = v3.50)

// v3.51 MU351: one line per list opened / closed with the sound state (at most 400 a session); `watch` = opened or back
// in the pause with PauseSound on: an ANOMALY when the music is not paused then
static void MuLogSound(const char *list, const char *event, int watch)
{
    static LONG s_q, s_a;
    const float master = MU_MASTER;
    const int fade = MU_FADEMODE, pm = MU_PAUSEMODE, st = MuMusicState(master, fade);
    if (LogQuota(&s_q, 400))
        Log("extra menu: sound in the %s: %s: pause mode %d, master %.2f, fade %d: %s%s", list, event, pm, master, fade,
            MuMusicName(st), g_emPauseSound ? "" : " (PauseSound=0: v3.50)");
    if (watch && g_emPauseSound && st != MU_MUSIC_PAUSED && LogQuota(&s_a, 20))
        LogAnomaly("extra menu", "the level music is not paused in the %s (%s; pause mode %d, master %.2f, fade %d)", list, event,
                   pm, master, fade);
}

static int EmFault(EXCEPTION_POINTERS *e, const char *where)
{
    static int n;
    if (++n <= 5)
        Log("extra menu: %s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
            e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------- Playable Eileen (RE 1.5; pm_pe_on CT 934)
typedef struct { DWORD magic, version; volatile LONG ready, open; int(__cdecl *Open)(void); DWORD hotkey; } PeMenuApi;
static PeMenuApi *pm_pe_api;
static int pm_pe_tried;

// 1 = the Playable Eileen row is shown: SH4PE_Menu exported by dsound.dll, 'SHPM', version >= 1, ready 1, Open != 0.
// Looked up once, on the game thread (as the table).
static int pm_pe_on(void)
{
    const PeMenuApi *p;
    if (!pm_pe_tried) {
        HMODULE m = GetModuleHandleA("dsound.dll");
        pm_pe_tried = 1;
        if (m) pm_pe_api = (PeMenuApi *)(void *)GetProcAddress(m, "SH4PE_Menu");
        Log("extra menu: Playable Eileen's SH4PE_Menu %s", pm_pe_api ? "found" : "not exported (no Playable Eileen row)");
    }
    p = pm_pe_api;
    return p && p->magic == 0x4D504853 && p->version >= 1 && p->ready == 1 && p->Open;
}

// E5: PE's own menu is up ([SH4PE_Menu+0C] = 1): no Extra Menu logic (CT 1069-1074)
static int pm_pe_open(void) { return pm_pe_on() && pm_pe_api->open != 0; }

// ---------------------------------------------------------------- Eileen Outfit bridge (RE 2.5; CT 969-1057)
// SH4R_Extra[3] = the table's eo_api: +0 'EOV2', +4 eo_menu_can, +8 eo_menu_go, +C &eo_cur, +10 &eo_status, +14
// 'EOV4', +18 eo_menu_set(choice), +1C &eo_outfit. CE memory: re-read and checked before every use, under SEH.
#define EO_V2 0x32564F45
#define EO_V4 0x34564F45
static volatile DWORD *EoApi(void) { return (volatile DWORD *)(ULONG_PTR)SH4R_Extra[3]; }

// pm_eo_on (+ the Option 6 switch): the Eileen row is shown
static int pm_eo_on(void)
{
    volatile DWORD *e;
    int r = 0;
    if (!g_emOn[EM_EILEEN]) return 0;
    __try {
        e = EoApi();
        r = e && e[0] == EO_V2 && e[1] && e[2];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 0;
    }
    return r;
}

// pm_eo4: Outfit v4 (the 3-way row)
static int pm_eo4(void)
{
    volatile DWORD *e;
    int r = 0;
    __try {
        e = EoApi();
        r = e && e[0] == EO_V2 && e[5] == EO_V4 && e[6] && e[7];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 0;
    }
    return r;
}

// pm_eo_val: eo_outfit clamped to -1 Default / 0 Dress / 1 Nurse (-1 when not v4)
static int pm_eo_val(void)
{
    volatile DWORD *e;
    int v = -1;
    __try {
        e = EoApi();
        if (e && e[0] == EO_V2 && e[5] == EO_V4 && e[6] && e[7]) {
            v = *(volatile int *)(ULONG_PTR)e[7];
            if (v > 1) v = 1;
            if (v < -1) v = -1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        v = -1;
    }
    return v;
}

// pm_eo_step: dir +1 next / -1 previous (wraps both ways), then eo_menu_set(new). 1 done, 0 outfit script gone.
static int pm_eo_step(int dir)
{
    volatile DWORD *e;
    int v = pm_eo_val() + dir, r = 0;
    if (v > 1) v = -1;
    if (v < -1) v = 1;
    __try {
        e = EoApi();
        if (e && e[0] == EO_V2 && e[5] == EO_V4 && e[6]) {
            ((void(__cdecl *)(int))(ULONG_PTR)e[6])(v);
            r = 1;
        }
    } __except (EmFault(GetExceptionInformation(), "eo_menu_set")) {
        r = 0;
    }
    return r;
}

// eo_menu_can() / eo_menu_go() (Outfit v2 / v3), 0 when the outfit script is gone
static int EoCall(int slot)
{
    volatile DWORD *e;
    int r = 0;
    __try {
        e = EoApi();
        if (e && e[0] == EO_V2 && e[slot]) r = ((int(__cdecl *)(void))(ULONG_PTR)e[slot])();
    } __except (EmFault(GetExceptionInformation(), "eo_api call")) {
        r = 0;
    }
    return r;
}

// [eo_cur] (Outfit v2 / v3: 0 normal, 1 nurse, -1 unknown), -1 when not readable
static int pm_eo_cur(void)
{
    volatile DWORD *e;
    int v = -1;
    __try {
        e = EoApi();
        if (e && e[0] == EO_V2 && e[3]) v = *(volatile int *)(ULONG_PTR)e[3];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        v = -1;
    }
    return v;
}

// ---------------------------------------------------------------- guards (RE 1.4; CT 750-929)
static int sa_hard(void)
{
    return EM_I(0x107BF50) != 0 && !EM_I(0xFD5AA0) && !EM_I(0xFD5AA4) && !EM_I(0xFD5BB0) && !EM_I(0xFD5BB4) &&
           !EM_I(0xFD5BB8) && !EM_I(0xFD5C28);
}
static int ch_hard(void) { return !EM_I(0x10A1914); }
static int sa_soft_load(void) { return EM_I(0x10A0F28) > 0 ? 1 : 2; }  // Henry HP (float bits, signed compare as CT 795)
static int sa_soft_save(void)
{
    if (EM_I(0xFD5A90) == 9 || !(EM_B(0x13D815B) & 0x20) || (EM_B(0x13D8113) & 0x20)) return 2;
    return sa_soft_load();
}
// v3.32 PM332: our hand-over overlay is up (sub-mode 1 with hkEmBridge as its callback, nothing pending)
static int EmInBridge(void) { return g_emBridge && EM_SUB == 1 && !EM_PEND && EM_CB == (DWORD)(ULONG_PTR)hkEmBridge; }
// a sub-mode is up or pending, other than our hand-over overlay (the requests run inside that one)
static int EmSubBusy(void) { return (EM_SUB || EM_PEND) && !EmInBridge(); }
// sa_idle / la_idle / ch_idle: 1 idle, 0 busy, 2 soft guard
static int sa_idle(void) { return EmSubBusy() || !sa_hard() ? 0 : sa_soft_save(); }  // v3.32 PM332: EmSubBusy
static int la_idle(void) { return EmSubBusy() || !sa_hard() ? 0 : sa_soft_load(); }
static int ch_idle(void) { return EmSubBusy() || !sa_hard() || !ch_hard() ? 0 : sa_soft_load(); }
static int pm_busy(void) { return sa_active || sa_loading || la_active || la_restart || ch_active || pm_defer; }
static int pm_can_save(void) { return !pm_busy() && sa_hard() && sa_soft_save() == 1; }
static int pm_can_load(void) { return !pm_busy() && sa_hard() && sa_soft_load() == 1; }
static int pm_can_chest(void) { return !pm_busy() && sa_hard() && ch_hard() && sa_soft_load() == 1; }
static int pm_can_eo(void) { return !pm_busy() && sa_hard() && EoCall(1) != 0; }
// Quick Save row 11 = pm_can_save + Refined's in-pause quick-save guard; Quick Load row 12 = that + a valid file
// (pm_can_ql: pm_can_save and qs_valid)
// v3.33 MENU333 (v3.32 QA: "flash greyed out at random intervals ... stay greyed out in Subway World"): the rows ask the
// HARD guards only (QsGuardHard: a fade, a data file loading or a message make the confirm wait, they no longer grey
// the row; the sound streams' reads no longer count at all, QsFilesLoading). Quick Load asks the LOAD guards (as F9:
// no story gate, no "Henry busy") + a valid file, not Quick Save's. NULL = available, else why (for the log).
static const char *pm_why_soft_save(void)
{
    if (EM_I(0xFD5A90) == 9) return "the final rooms (stage 9)";
    if (!(EM_B(0x13D815B) & 0x20)) return "the story does not allow saving here yet (story bit 13D815B.5)";
    if (EM_B(0x13D8113) & 0x20) return "the elevator is moving (story bit 13D8113.5)";
    return sa_soft_load() == 1 ? NULL : "Henry is dying (HP)";
}
static const char *pm_why_hard(void)
{
    if (pm_busy()) return "another Extra Menu action runs";
    if (!EM_I(0x107BF50)) return "the game does not allow menus now (107BF50 = 0)";
    if (EM_I(0xFD5AA0) || EM_I(0xFD5AA4)) return "a room change is pending";
    if (EM_I(0xFD5BB0) || EM_I(0xFD5BB4) || EM_I(0xFD5BB8)) return "death, game over or ending";
    if (EM_I(0xFD5C28)) return "an interaction runs (FD5C28)";
    return NULL;
}
static const char *pm_why_qs(void)
{
    const char *w = pm_why_hard();
    if (!w) w = pm_why_soft_save();
    return w ? w : QsGuardHard(0, 1, 0);
}
static const char *pm_why_ql(void)
{
    const char *w = pm_why_hard();
    if (!w && sa_soft_load() != 1) w = "Henry is dying (HP)";
    if (!w && !g_qsAnyValid) w = "no valid quick save on disk";
    return w ? w : QsGuardHard(1, 1, 0);
}
static int pm_can_qs(void) { return !pm_why_qs(); }
static int pm_can_ql(void) { return !pm_why_ql(); }

// v3.33 MENU333: the two rows' look. The sub-list's first frame shows the state as it is; after that a row changes its
// look only once the new state held QS_HOLD_MS (qs_avail_sim.h). LogV: why a row is grey, when it changes; LogAnomaly:
// a test that flips more than QS_FLAP_MAX times in a second (the pause holds the world still: nothing should flip).
static QsAvail g_emQsAv[2];
static int g_emQsOpen;
static DWORD g_emQsLastDraw;
static const char *g_emQsWhy[2];   // the last reason seen per row (for the anomaly line)
static int g_emQsWait;             // a Quick Save confirm waits for a passing state (em_qs_wait)
static DWORD g_emQsWaitT, g_emQlTick;
static void EmQsRows(void)
{
    static const char *const kName[2] = { "Quick Save", "Quick Load" };
    static LONG s_qv, s_qa, s_qp;
    static const char *s_pass;
    const DWORD now = GetTickCount();
    const char *why[2], *pass;
    int k, r;
    why[0] = pm_why_qs();
    why[1] = pm_why_ql();
    for (k = 0; k < 2; ++k)
        if (why[k]) g_emQsWhy[k] = why[k];
    if (!g_emQsOpen || now - g_emQsLastDraw > 250) {  // the sub-list opened (or the pause was away a while)
        for (k = 0; k < 2; ++k) QsAvailOpen(&g_emQsAv[k], !why[k], now);
        g_emQsOpen = 1;
        s_pass = NULL;
        if ((g_emOn[EM_QSAVE] || g_emOn[EM_QLOAD]) && LogQuota(&s_qv, 400))
            LogV("extra menu: Quick Save %s%s, Quick Load %s%s (stage %d room %d)", why[0] ? "grey: " : "available",
                 why[0] ? why[0] : "", why[1] ? "grey: " : "available", why[1] ? why[1] : "", EM_I(0xFD5A90), EM_I(0xFD5A94));
    } else {
        for (k = 0; k < 2; ++k) {
            r = QsAvailStep(&g_emQsAv[k], !why[k], now);
            if (!g_emOn[k ? EM_QLOAD : EM_QSAVE]) continue;
            if ((r & 1) && LogQuota(&s_qv, 400))
                LogV("extra menu: %s now %s%s (held %d ms)", kName[k], why[k] ? "grey: " : "available", why[k] ? why[k] : "",
                     QS_HOLD_MS);
            if ((r & 2) && LogQuota(&s_qa, 20))
                LogAnomaly("extra menu", "%s's guard flipped more than %d times in 1 s while the pause is up (last reason: "
                           "%s; stage %d room %d): the row is held steady", kName[k], QS_FLAP_MAX,
                           g_emQsWhy[k] ? g_emQsWhy[k] : "?", EM_I(0xFD5A90), EM_I(0xFD5A94));
        }
    }
    g_emQsLastDraw = now;
    pass = QsGuardPassing();
    if (pass != s_pass) {  // a passing state came or went (it does not grey the rows)
        s_pass = pass;
        if (pass && LogQuota(&s_qp, 100)) LogT("extra menu: passing state while the pause is up: %s %s", pass, g_qsPassWhat);
    }
    pm_okt[PM_QSAVE] = g_emQsAv[0].shown;
    pm_okt[PM_QLOAD] = g_emQsAv[1].shown;
}

// v3.33 MENU333: a Quick Save confirm made during a passing state (a fade, a data file, a message): saved as soon as it
// ends, at most QS_WAIT_MS later (then QsSaveEx refuses with its own sound, text and log line); dropped if the pause
// closes. EmTick, every tick (the pause too).
static void em_qs_wait(void)
{
    static LONG s_q;
    if (!g_emQsWait) return;
    if (EM_SUB != 2 || EM_PEND || !pm_sub) {
        g_emQsWait = 0;
        if (LogQuota(&s_q, 50)) LogV("extra menu: Quick Save: the pause closed while it waited: dropped");
        return;
    }
    if (!pm_why_qs() && QsGuardPassing() && GetTickCount() - g_emQsWaitT < QS_WAIT_MS) return;
    g_emQsWait = 0;
    if (LogQuota(&s_q, 50)) LogV("extra menu: Quick Save: waited %lu ms", GetTickCount() - g_emQsWaitT);
    QsSaveEx(1);
}

// ---------------------------------------------------------------- layout (pm_layout, CT 1965-2105)
// pm_vis / pm_n / pm_y0 / pm_my0 and the cursor: pm_want (a row id; not shown -> row 0), else the same row id as in
// the previous list, else clamped. Several calls per frame give the same result.
static void pm_layout(void)
{
    int keep = -1, wanted = 0, n = 0, i, cur, eo, cam, on[EM_N];
    for (i = 0; i < EM_N; ++i) on[i] = g_emOn[i] != 0;
    if (pm_want != -1) {
        keep = pm_want;
        pm_want = -1;
        wanted = 1;
    } else {
        cur = EM_CUR;
        if ((unsigned)cur < (unsigned)pm_n) keep = pm_vis[cur];
    }
    eo = pm_eo_on();  // pm_feat
    cam = CamPauseOn();  // v3.37 CP337: the Camera row (SH4 Refined's camera owns the view)
    pm_nf = eo + cam + on[EM_CHEST] + on[EM_SAVE] + on[EM_LOAD] + on[EM_QSAVE] + on[EM_QLOAD];
    if (pm_sub == 2 && !cam) {  // v3.37 CP337: the page lost its camera (never seen: the owner is decided at start)
        static LONG s_q;
        pm_sub = 1;
        keep = PM_CAMERA;
        wanted = 1;
        if (LogQuota(&s_q, 5)) LogAnomaly("extra menu", "the Camera page was open while SH4 Refined's camera does not own the view "
                                          "(owner %ld): back to the Extra Menu", (long)g_camOwner);
    }
    if (!pm_nf) {  // vanilla: 0 (13) 2 3 4 5; a sub-menu that became empty drops to Resume
        if (pm_sub) {
            pm_sub = 0;
            keep = PM_RESUME;
            wanted = 1;
        }
        pm_vis[n++] = PM_RESUME;
        if (pm_pe_on()) pm_vis[n++] = PM_PE;
        pm_vis[n++] = PM_OPTIONS;
        pm_vis[n++] = PM_MAP;
        pm_vis[n++] = PM_SCRAP;
        pm_vis[n++] = PM_QUIT;
    } else if (!pm_sub) {  // top level: 0 (13) 1 2 3 4 5
        pm_vis[n++] = PM_RESUME;
        if (pm_pe_on()) pm_vis[n++] = PM_PE;
        pm_vis[n++] = PM_EXTRA;
        pm_vis[n++] = PM_OPTIONS;
        pm_vis[n++] = PM_MAP;
        pm_vis[n++] = PM_SCRAP;
        pm_vis[n++] = PM_QUIT;
    } else if (pm_sub == 2) {  // v3.37 CP337: the Camera page: its 12 settings, Back
        for (i = 0; i <= CAMR_N; ++i) pm_vis[n++] = PM_CAMR0 + i;
    } else {  // sub-menu: (6) (7) (8) (11) (12) (9) (14) 10
        if (on[EM_CHEST]) pm_vis[n++] = PM_CHEST;
        if (on[EM_SAVE]) pm_vis[n++] = PM_SAVE;
        if (on[EM_LOAD]) pm_vis[n++] = PM_LOAD;
        if (on[EM_QSAVE]) pm_vis[n++] = PM_QSAVE;
        if (on[EM_QLOAD]) pm_vis[n++] = PM_QLOAD;
        if (eo) pm_vis[n++] = PM_EILEEN;
        if (cam) pm_vis[n++] = PM_CAMERA;  // v3.37 CP337
        pm_vis[n++] = PM_BACK;
    }
    pm_n = n;
    pm_step = pm_sub == 2 ? PM_CAM_STEP : 0x1C;  // v3.37 CP337
    pm_y0 = (pm_sub == 2 ? PM_CAM_MID : 0xB6) - (n - 1) * pm_step / 2;  // centred on B6h: 5 rows -> 7Eh (vanilla)
    pm_my0 = pm_y0 + 8;
    if (keep != -1) {
        for (i = 0; i < n; ++i)
            if (pm_vis[i] == keep) {
                EM_CUR = i;
                return;
            }
        if (wanted) {
            EM_CUR = 0;
            return;
        }
    }
    cur = EM_CUR;
    if (cur < 0) EM_CUR = 0;
    else if (cur >= n) EM_CUR = n - 1;
}

static void __cdecl pm_layout_c(void)  // from the naked stubs (pushad around it)
{
    __try {
        pm_layout();
    } __except (EmFault(GetExceptionInformation(), "pm_layout")) {
    }
}

// ---------------------------------------------------------------- draw (pm_text CT 2142, pm_draw CT 2175-2327)
static void pm_text(int bank, int id, int x, int y, int align)
{
    ((void(__cdecl *)(int))0x556A50)(8);  // shadow: palette 8
    ((EmText)0x50B5B0)(bank, id, x + 2, y + 2, align, 0);
    ((void(__cdecl *)(DWORD))0x556A90)(pm_col);
    ((EmText)0x50B5B0)(bank, id, x, y, align, 0);
}
#define EM_RAW(p) ((int)(ULONG_PTR)(p))

// v3.37 CP337: the Camera page: the header, each setting right-aligned label / value (the Eileen row's columns), the
// cursor row's value between < >, Back centred; under the rows the cursor row's description and the hint (the view
// changes when play resumes: the world is not drawn under the pause).
static void pm_cam_draw(void)
{
    int i, r, yl = pm_y0;
    pm_col = pm_hdr_col;
    pm_text(-1, EM_RAW(CamPauseTitle()), 0x100, pm_y0 - PM_CAM_HDR, 1);
    for (i = 0; i < pm_n; ++i) {
        const int y = pm_y0 + i * pm_step, sel = i == EM_CUR;
        const BYTE *v;
        yl = y;
        r = pm_vis[i] - PM_CAMR0;
        pm_col = sel ? 0x808080 : 0x404040;
        if (r < 0 || r >= CAMR_N) {  // Back
            pm_text(-1, EM_RAW(CamPauseLabel(CAMR_N)), 0x100, y, 1);
            continue;
        }
        pm_text(-1, EM_RAW(CamPauseLabel(r)), pm_eo_x - 0x0A, y, 2);
        v = CamPauseValue(r);
        if (!v) continue;
        pm_text(-1, EM_RAW(v), pm_eo_x + 0x12, y, 0);
        if (sel) {
            pm_col = 0x606060;
            pm_text(-1, EM_RAW(pm_str_lt), pm_eo_x - 4, y, 0);
            pm_text(-1, EM_RAW(pm_str_gt), pm_eo_x + 0x12 + (int)(LabelW(v) + 0.5f) + 6, y, 0);
        }
    }
    r = (unsigned)EM_CUR < (unsigned)pm_n ? pm_vis[EM_CUR] - PM_CAMR0 : CAMR_N;
    pm_col = 0x505050;
    pm_text(-1, EM_RAW(CamPauseDesc(r)), 0x100, yl + 0x22, 1);
    pm_col = 0x383838;
    pm_text(-1, EM_RAW(pm_str_camhint), 0x100, yl + 0x22 + 0x18, 1);
    pm_text(-1, EM_RAW(pm_str_camhint2), 0x100, yl + 0x22 + 0x30, 1);
}

static void pm_draw_body(void)
{
    int i, rid, y, eo;
    pm_layout();
    if (pm_sub == 2) {  // v3.37 CP337
        g_emQsOpen = 0;
        pm_cam_draw();
        return;
    }
    pm_okt[PM_CHEST] = pm_can_chest();
    pm_okt[PM_SAVE] = pm_can_save();
    pm_okt[PM_LOAD] = pm_can_load();
    if (pm_sub) EmQsRows();  // v3.33 MENU333: held, logged (was pm_can_qs / pm_can_ql every frame)
    else g_emQsOpen = 0;
    eo = 0;  // the Eileen row only lives in the sub-menu: ask the outfit script only then (v4: always available)
    if (pm_sub && g_emOn[EM_EILEEN]) eo = pm_eo4() ? 1 : pm_can_eo();
    pm_okt[PM_EILEEN] = eo;
    pm_okt[PM_MAP] = EM_CALL0(0x521100) != 0;  // map available (0 in Room 302), as stock
    if (pm_sub && pm_hdr_col) {  // header above the sub-menu, centred on x 100h, 2Ah above the first row
        pm_col = pm_hdr_col;
        pm_text(-1, EM_RAW(pm_str_cheats), 0x100, pm_y0 - 0x2A, 1);
    }
    for (i = 0; i < pm_n; ++i) {
        rid = pm_vis[i];
        y = pm_y0 + i * 0x1C;
        pm_col = i == EM_CUR ? 0x808080 : pm_okt[rid] ? 0x404040 : 0x101010;
        if (rid == PM_EILEEN && pm_eo_label) {  // pm_draw_eo: "Eileen" right-aligned at pm_eo_x + the value after it
            int vb = -1, vid = 0, c;
            if (pm_eo4()) {
                vid = EM_RAW(pm_eo_strs[pm_eo_val() + 1]);
            } else if (pm_okt[PM_EILEEN] && (unsigned)(c = pm_eo_cur()) <= 1) {
                vb = 0;
                vid = 0xCB + c;  // 0CBh "Normal" / 0CCh "Nurse"
            }
            if (vid) {
                pm_text(-1, EM_RAW(pm_str_eileen), pm_eo_x, y, 2);
                pm_text(vb, vid, pm_eo_x + 0x0A, y, 0);
                continue;
            }
        }
        pm_text(pm_rows[rid][0], pm_rows[rid][1], 0x100, y, 1);
    }
    // the Quick Save row's feedback line (QsFrame, from the HUD hook, does not run in the pause): QsFrame's draw
    if (g_qsMsgUntil && (int)(g_qsMsgUntil - GetTickCount()) > 0) {
        ((void(__cdecl *)(int))0x556A50)(8);
        ((EmText)0x50B5B0)(-1, EM_RAW(g_qsMsg), 256 + 2, 392 + 2, 1, 0);
        ((void(__cdecl *)(DWORD))0x556A90)(g_qsMsgCol);
        ((EmText)0x50B5B0)(-1, EM_RAW(g_qsMsg), 256, 392, 1, 0);
    }
}

// 4F8A30 (jmp here): replaces the whole pause draw. cdecl: keeps ebx / esi / edi / ebp, as pm_draw does.
static void __cdecl pm_draw(void)
{
    if (EmInBridge()) return;  // v3.32 PM332: the confirm turned the pause into our overlay this tick: no list (no flash)
    __try {
        pm_draw_body();
    } __except (EmFault(GetExceptionInformation(), "pm_draw")) {
    }
}

// ---------------------------------------------------------------- v3.32 PM332: the seamless hand-over
// The pause -> our overlay, in the confirm (game thread, inside 4F8E90 at sub-mode 2): what Resume (4F90C2) and then
// 0x4F8DD0 would leave, minus everything that lets the world run. Kept: phase 2 (0x4EFAE0 never runs), the frozen
// picture (no 4031F0 backup release, no 55A160(0)), the rumble stop, HUD off, events off. The sound went the same way
// as Resume + 0x4F8DD0 in one tick (56E960(-1), then (2)): the master to 1.0, then down to 0.2 for the whole list.
// v3.51 MU351 (USER_FEEDBACK_v350 item 11): the pause's silence goes on, as the stock pause opens its map and memo list
// (no sound call): pauseMode is only marked 2, so the targets' own leave (-1) fades in as in v3.50 (pausesound_sim.h).
static int EmBridgeOpen(void)
{
    if (!EmSeamCanOpen(g_emSeamless, EM_SUB, EM_PEND, EM_PHASE, EM_CAPTURE, pm_pe_open())) return 0;
    {   // v3.51 MU351
        static LONG s_q;
        const int pm = MU_PAUSEMODE, act = MuBridgeAct(g_emPauseSound, pm);
        if (act == MU_ACT_V350) {
            EM_CALL1(0x56E960, -1);
            EM_CALL1(0x56E960, 2);
        } else {
            if (act == MU_ACT_REPAUSE) EM_CALL1(0x56E960, 0);
            if (act != MU_ACT_LEAVE) MU_PAUSEMODE = MU_MARK_MODE;
            if (act != MU_ACT_KEEP && LogQuota(&s_q, 20))
                LogAnomaly("extra menu", "the pause menu's sound was in mode %d (the pause's own is 0): %s", pm,
                           act == MU_ACT_REPAUSE ? "paused again (0x56E960(0))" : "left as it is");
        }
    }
    EM_CB = (DWORD)(ULONG_PTR)hkEmBridge;
    EM_SUB = 1;  // 4F9110: no case of its own, only the callback runs: the hand-over overlay
    g_emBridge = 1;
    g_emBridgeFrames = 0;
    g_emBridgeIdle = 0;
    return 1;
}

// May an overlay of ours turn back into the pause now? Phase 2 still (the world never ran), sub-mode 1 (ours) or 8 (the
// target's own 4F8E60 this tick), the frozen picture shown or only dropped this tick (3: the frame end has not run yet).
static int EmPauseOk(void)
{
    return EmSeamCanReturn(g_emSeamless, EM_SUB, EM_PEND, EM_PHASE, EM_CAPTURE, pm_pe_open());
}

// The overlay -> the pause, in place, on the Extra Menu at row `row` (call only after EmPauseOk). If the target's own
// 4F8E60 ran this tick, its 4031F0 / 406C30 / 55A160(0) are undone: the picture is shown again (fixed target 1, which
// only 0x55A5C0 ever writes, still holds it), its backups are taken again (0x403250, as 0x55ABF0 does after a capture:
// a device reset restores the picture from them) and the rumble is stopped (0x406C10). Then the stock pause entry's
// state (0x4F8BA0 / pm_reopen): sound pause 0, events off, first-person look off, HUD off. fadeIn: from black (chest,
// cancelled load list); the save menu's own fade has already shown the picture again.
static void EmToPause(int row, int fadeIn)
{
    if (EM_CAPTURE == EMC_END) {
        EM_CAPTURE = EMC_SHOW;
        EM_CALL0(0x403250);
        EM_CALL0(0x406C10);
    }
    EM_CALL1(0x56E960, 0);
    EM_CB = 0;
    EM_PEND = 0;
    EM_SUB = 2;
    EM_I(0xFD5BC0) = 1;
    EM_CALL0(0x5000E0);
    EM_CALL0(0x4F5090);
    if (fadeIn) EM_CALL2(0x565CB0, 0, 8);
    g_emBridge = 0;
    pm_sub = 1;
    pm_want = row ? row : PM_EXTRA;
    pm_ret_row = 0;  // nothing left for pm_ret_check
    pm_ret_go = 0;
    QsAnyValid();    // the sub-list opens (pm_reopen)
    if (++g_emSeamN <= 20) Log("extra menu: back in the pause menu on the Extra Menu, the world kept frozen (%d)", g_emSeamN);
}

// Something refused or failed while our hand-over overlay is up: back to the pause with the buzzer (v3.31 closed the
// pause and logged it in gameplay). If the pause cannot come back (never seen: the picture or the phase changed under
// us), the overlay is left like Resume. 0 = the overlay is not ours (the old path goes on).
static int EmBridgeBack(void)
{
    if (!EmInBridge()) return 0;
    if (EmPauseOk()) {
        EmToPause(pm_ret_row, 1);
        EM_SOUND(0x9C44);
        return 1;
    }
    Log("extra menu: the hand-over overlay could not turn back into the pause (sub %d phase %d picture %d): left",
        EM_SUB, EM_PHASE, EM_CAPTURE);
    g_emBridge = 0;
    pm_ret_row = 0;
    EM_CALL0(0x4F8E60);
    return 1;
}

// The target overlay starts: inside our hand-over overlay only its callback changes; otherwise as v3.31 (0x4F8DD0).
static int EmOverlay(DWORD cb)
{
    if (EmInBridge()) {
        g_emBridge = 0;
        EM_CB = cb;
        return 1;
    }
    if (!EM_CALL0(0x4F8DD0)) return 0;
    EM_CALL1(0x4F8E40, cb);
    return 1;
}

// A refusal on the way in: back to the pause inside our overlay, else (v3.31) the HUD on again in gameplay.
static void EmHudBack(void)
{
    if (!EmBridgeBack()) EM_CALL0(0x4F5070);
}

// hkEmBridge, every frame while our hand-over overlay is up: a watchdog. Nothing pending and nothing loading for 3
// frames (a request that was dropped), or 1200 frames in all (40 s; the file wait itself gives up at 900): back to the
// pause.
static void em_bridge_body(void)
{
    ++g_emBridgeFrames;
    if (sa_request | la_request | ch_request | sa_loading) g_emBridgeIdle = 0;
    else ++g_emBridgeIdle;
    if (g_emBridgeIdle >= 3 || g_emBridgeFrames > 1200) {
        Log("extra menu: the hand-over overlay waited %d frames for nothing: back to the pause menu", g_emBridgeFrames);
        EmBridgeBack();
    }
}
static void __cdecl em_bridge_c(void)
{
    __try {
        em_bridge_body();
    } __except (EmFault(GetExceptionInformation(), "em_bridge")) {
    }
}

// ---------------------------------------------------------------- confirm / cancel (pm_dispatch CT 2362-2514)
static DWORD g_emJmp, g_emEax;  // where the naked stub goes next, and the eax it takes there

static void pm_leave_sub(void)
{
    pm_sub = 0;
    pm_want = PM_EXTRA;
}
static void pm_buzz(void)
{
    EM_SOUND(0x9C44);
    g_emJmp = EM_EPI;
}
// pm_close: a feature row closes the pause like Resume; the request runs at sub-mode 0 (pm_defer).
// v3.32 PM332: seamless (Storage Chest, Save Game, Load Game): the pause becomes our hand-over overlay instead, the
// request runs inside it (EmInBridge); Quick Load (a world restart from disk, QsLoad's gameplay guards) stays v3.31's.
static void pm_close(int seamless)
{
    pm_ret_go = 0;
    pm_leave_sub();
    pm_defer_frames = 0;
    pm_defer = 1;
    EM_SOUND(0x9C48);
    g_emJmp = seamless && EmBridgeOpen() ? EM_EPI : EM_RESUME;
}
// pm_back: Back row, or cancel in the sub-menu: top level, cursor on Extra Menu
static void pm_back(void)
{
    EM_SOUND(0x9C42);
    pm_sub = 0;
    pm_want = PM_EXTRA;
    pm_layout();
    g_emJmp = EM_EPI;
}

// v3.37 CP337: the Camera page. Open on the row it was left on (first time: the OTS / Game switch); Back / Cancel: the
// Extra Menu, cursor on the Camera row; a confirm on a setting steps it up (a range wraps at its end, as Enter did on
// v3.33's Options page).
static void pm_cam_open(void)
{
    static LONG s_q;
    EM_SOUND(0x9C48);
    pm_sub = 2;
    pm_want = PM_CAMR0 + ((unsigned)pm_camRow < CAMR_N ? pm_camRow : 0);
    CamRepeatReset(&pm_camRep);  // a Left / Right still held does not run on
    pm_layout();
    if (LogQuota(&s_q, 200)) LogV("extra menu: Camera page opened (camera %s; the view changes when play resumes)", g_cam.on ? "OTS" : "Game");
}
static void pm_cam_back(void)
{
    static LONG s_q;
    const int cur = EM_CUR;
    pm_camRow = (unsigned)cur < (unsigned)pm_n && pm_vis[cur] >= PM_CAMR0 && pm_vis[cur] < PM_CAMBACK ? pm_vis[cur] - PM_CAMR0 : 0;
    EM_SOUND(0x9C42);
    pm_sub = 1;
    pm_want = PM_CAMERA;
    pm_layout();
    g_emJmp = EM_EPI;
    if (LogQuota(&s_q, 200))
        LogV("extra menu: Camera page closed: camera %s, distance %g side %g height %g tilt %g, turn %g tilt %g, invert %d %d, wall "
             "zoom %d margin %g, fixed cameras %s", g_cam.on ? "OTS" : "Game", g_cam.dist, g_cam.side, g_cam.height, g_cam.tiltDeg,
             g_cam.turnSpeed, g_cam.tiltSpeed, g_cam.invX, g_cam.invY, g_cam.wall, g_cam.margin, g_cam.fixedCams ? "the game's" : "off");
}
static void pm_cam_confirm(int id)
{
    g_emJmp = EM_EPI;
    if (id == PM_CAMBACK) {
        pm_cam_back();
        return;
    }
    if (!CamPauseOn()) {
        pm_buzz();
        return;
    }
    if (id == PM_CAMERA) {
        pm_cam_open();
        return;
    }
    if (CamPauseStep(id - PM_CAMR0, 1, 1)) EM_SOUND(0x9C40);
}

static void pm_dispatch_body(void)
{
    int cur, id;
    pm_layout();
    cur = EM_CUR;
    g_emJmp = EM_EPI;
    if ((unsigned)cur >= (unsigned)pm_n) return;  // outside the menu
    id = pm_vis[cur];
    g_emEax = id;
    if (id >= PM_CAMERA && id <= PM_CAMBACK) {  // v3.37 CP337: the Camera row and its page
        pm_cam_confirm(id);
        return;
    }
    switch (id) {
    case PM_EXTRA:  // pm_do_cheats: open the sub-menu on its first row
        EM_SOUND(0x9C48);
        pm_sub = 1;
        pm_want = PM_CHEST;
        QsAnyValid();  // the Quick Load row's file test, refreshed when the sub-list opens
        pm_layout();
        return;
    case PM_CHEST:
        if (!pm_can_chest()) { pm_buzz(); return; }
        ch_request = 1;
        pm_ret_row = PM_CHEST;  // reopen on this row when the chest has closed
        pm_close(1);  // v3.32 PM332: seamless
        return;
    case PM_SAVE:
        if (!pm_can_save()) { pm_buzz(); return; }
        sa_request = 1;
        pm_ret_row = PM_SAVE;
        pm_close(1);  // v3.32 PM332: seamless
        return;
    case PM_LOAD:
        if (!pm_can_load()) { pm_buzz(); return; }
        la_request = 1;
        pm_ret_row = PM_LOAD;
        pm_close(1);  // v3.32 PM332: seamless
        return;
    case PM_EILEEN:  // pm_do_eo: v4 next choice, the pause stays; v2 / v3 eo_menu_go and Resume
        if (pm_eo4()) {
            if (!pm_eo_step(1)) { pm_buzz(); return; }
            EM_SOUND(0x9C48);
            return;
        }
        if (!pm_can_eo() || !EoCall(2)) { pm_buzz(); return; }
        pm_leave_sub();
        EM_SOUND(0x9C48);
        g_emJmp = EM_RESUME;
        return;
    case PM_BACK:
        pm_back();
        return;
    case PM_QSAVE:  // pm_do_qs: Refined's disk quick save right here (its own sound and text), the pause stays
        {   // v3.33 MENU333: the reason logged; a passing state is waited for (em_qs_wait)
            const char *w = pm_why_qs();
            static LONG s_q;
            if (w) {
                if (LogQuota(&s_q, 100)) LogV("extra menu: Quick Save refused: %s", w);
                pm_buzz();
                return;
            }
            if (g_emQsWait) return;
            if (QsGuardPassing()) {
                g_emQsWait = 1;
                g_emQsWaitT = GetTickCount();
                if (LogQuota(&s_q, 100)) LogV("extra menu: Quick Save waits: %s %s", QsGuardPassing(), g_qsPassWhat);
                return;
            }
        }
        QsSaveEx(1);
        return;
    case PM_QLOAD:  // pm_do_ql: files checked again; then close the pause, QsLoad at sub-mode 0; no return to menu
        {   // v3.33 MENU333: the load guards, the reason logged
            const char *w = QsAnyValid() ? pm_why_ql() : "no valid quick save on disk";
            static LONG s_q;
            if (w) {
                if (LogQuota(&s_q, 100)) LogV("extra menu: Quick Load refused: %s", w);
                pm_buzz();
                return;
            }
        }
        g_emQlTick = GetTickCount();  // v3.33 MENU333
        ql_request = 1;
        pm_ret_row = 0;
        pm_close(0);  // v3.32 PM332: Quick Load keeps v3.31's close
        return;
    case PM_PE:  // pm_do_pe: PE's Open() (its own sound); 1 = its menu is up, the list stays as it is
        if (!pm_pe_on() || !pm_pe_api->Open()) pm_buzz();
        return;
    }
    if (id > PM_QUIT) return;
    g_emEax = id ? id - 1 : 0;  // 0 Resume -> table 0, 2..5 -> table 1..4 (stock jump table 4F90F0)
    g_emJmp = *(const DWORD *)(ULONG_PTR)(0x4F90F0 + 4 * g_emEax);
}

static void __cdecl pm_dispatch_c(void)
{
    __try {
        pm_dispatch_body();
    } __except (EmFault(GetExceptionInformation(), "pm_dispatch")) {
        g_emJmp = EM_EPI;
    }
}

// pm_cancel_code: top level = the stock close (4F90C2), sub-menu = pm_back
static void __cdecl pm_cancel_c(void)
{
    g_emJmp = EM_RESUME;
    __try {
        pm_layout();
        if (pm_sub == 2) pm_cam_back();  // v3.37 CP337: the Camera page -> the Extra Menu
        else if (pm_sub) pm_back();
    } __except (EmFault(GetExceptionInformation(), "pm_cancel")) {
        g_emJmp = EM_RESUME;
    }
}

// ---------------------------------------------------------------- storage chest (RE 2.1; CT 1740-1928)
// ch_resident(id): the sub-file is resident AND inside the arena [11B6000, 11B6008)
static int ch_resident(DWORD id)
{
    const DWORD p = (DWORD)EM_CALL1(0x5736D0, id);
    return p && p >= EM_U(0x11B6000) && p < EM_U(0x11B6008);
}

static void ch_fail(void)  // ch_cb_fail: leave the overlay, fade back in
{
    if (EmPauseOk()) {  // v3.32 PM332: our overlay never left: back into the pause (buzzer), fading in
        EmToPause(PM_CHEST, 1);
        EM_SOUND(0x9C44);
    } else {
        EM_CALL0(0x4F8E60);
        EM_CALL2(0x565CB0, 0, 8);
    }
    ch_active = 0;
    ch_phase = 3;
    Log("extra menu: storage chest ended early (ch_status %d)", ch_status);
}

// v3.32 PM332: the chest task left its overlay this tick (4CE1A0 -> 4D0C90 = 4F8E60, sub-mode 8, black screen): the
// chest object's f4 (4CE430) without its HUD on and sound back, then straight back into the pause, fading in. 0 = not
// possible (the old ch_run at sub-mode 0 and the reopen after it).
static int ch_to_pause(void)
{
    if (pm_ret_row != PM_CHEST || !EmPauseOk()) return 0;
    EM_CALL0(0x510970);           // MenuChestUiTerm
    EM_CALL0(0x4F0860);           // RoomGeometryDrawOn
    EM_B(0x13D8168) |= 0x80;      // story flag 0x647
    EM_CALL0(0x5171A0);           // HudBeltStateClear
    EmToPause(PM_CHEST, 1);
    ch_active = 0;
    ch_phase = 3;
    ch_status = 3;
    Log("extra menu: storage chest closed (held weapon refused %ld times)", ch_blocked);
    MuLogSound("storage chest", "closed, back in the pause", 1);  // v3.51 MU351
    return 1;
}

// ch_cb: the sub-mode 1 overlay callback (4F9110 calls [107BF5C] every frame)
static void ch_cb_body(void)
{
    ++ch_frames;
    if (ch_phase == 0) {  // wait for the file queue and the fade to black, then the resident checks
        if (EM_CALL1(0x573370, 1) != 1) {
            if (ch_frames < 900) return;
            ch_status = -7;
            ch_fail();
            return;
        }
        if (!EM_CALL0(0x565D20) && ch_frames < 900) return;  // fade finished? (never wait forever)
        if (!ch_resident(0xF000F007) || !ch_resident(0xF000F008) || !EM_CALL1(0x5736D0, 0xF004F1DF) ||
            !EM_CALL1(0x5736D0, 0xF005F1DF)) {
            ch_status = -8;
            ch_fail();
            return;
        }
        *(volatile WORD *)0xFD18F8 = 0;  // = the chest object's f1 4CE360 without the object calls and the lid sound
        EM_B(0xFD18FA) = 0;
        EM_CALL0(0x514A60);
        ch_phase = 1;
        ch_status = 2;
    }
    if (ch_phase == 1) {
        EM_CALL0(0x4CE1A0);  // the game's chest overlay task, one frame
        if (EM_B(0xFD18FA)) {  // it called 4D0C90 (= 4F8E60); ch_run finishes at sub-mode 0
            ch_phase = 2;
            ch_to_pause();     // v3.32 PM332: or straight back into the pause, this tick
        }
    }
}
static void __cdecl ch_cb_c(void)
{
    __try {
        ch_cb_body();
    } __except (EmFault(GetExceptionInformation(), "ch_cb")) {
    }
}

// ch_open (sub-mode 0): the two chest texture files, then the overlay with ch_cb and a fade to black
static void hkChCb(void);
static void ch_open(void)
{
    const int r = ch_idle();
    ch_request = 0;
    if (r != 1) {
        ch_status = r == 2 ? -4 : -1;
        Log("extra menu: storage chest refused (ch_status %d)", ch_status);
        EmBridgeBack();  // v3.32 PM332: from our hand-over overlay: back to the pause
        return;
    }
    if (EM_CALL1(0x573470, 0xF000F007) || EM_CALL1(0x573470, 0xF000F008)) {  // snap_itembox.bin, item_l.bin
        ch_status = -6;
        Log("extra menu: storage chest: file request refused");
        EmBridgeBack();  // v3.32 PM332
        return;
    }
    ch_phase = 0;
    ch_frames = 0;
    ch_blocked = 0;
    {
    const int seam = EmInBridge();  // v3.51 MU351: opened inside the pause
    if (!EmOverlay((DWORD)(ULONG_PTR)hkChCb)) {  // sub-mode 1: world and sound paused, input off (v3.32 PM332: EmOverlay)
        ch_status = -2;
        Log("extra menu: storage chest: overlay request failed");
        return;
    }
    EM_CALL2(0x565CB0, 0x80, 0x10);
    ch_active = 1;
    ch_status = 1;
    Log("extra menu: storage chest opened");
    MuLogSound("storage chest", seam ? "opened from the pause" : "opened from play", seam);  // v3.51 MU351
    }
}

// ch_run: after 4CE1A0 left the overlay and the sub-mode is 0: the chest object's f4 4CE430 + the fade in
static void ch_run(void)
{
    if (ch_phase != 2 || EM_SUB || EM_PEND) return;
    EM_CALL0(0x4F5070);
    EM_CALL0(0x510970);
    EM_CALL0(0x4F0860);
    EM_B(0x13D8168) |= 0x80;
    EM_CALL0(0x5171A0);
    EM_CALL1(0x56E960, -1);
    EM_CALL2(0x565CB0, 0, 8);
    ch_active = 0;
    ch_phase = 3;
    ch_status = 3;
    Log("extra menu: storage chest closed (held weapon refused %ld times)", ch_blocked);
}

// ---------------------------------------------------------------- save / load (RE 2.2 / 2.3; CT 1209-1383, 1606-1738)
static void sa_refuse(int r, const char *what)
{
    sa_status = r == 2 ? -4 : -1;
    Log("extra menu: %s refused (sa_status %d)", what, sa_status);
    EmBridgeBack();  // v3.32 PM332: from our hand-over overlay: back to the pause
}

// sa_preload: snap_save.bin + save_icon.bin, like the game's non-302 save path 4CD060
static void sa_preload(void)
{
    if (EM_CALL1(0x573470, 0xF000F00A) || EM_CALL1(0x573470, 0xF000F1EA)) {
        sa_status = -6;
        Log("extra menu: %s: file request refused", sa_kind == 2 ? "load game" : "save game");
        EmBridgeBack();  // v3.32 PM332
        return;
    }
    EM_CALL0(0x4F5090);  // player input off while waiting
    sa_loading = 1;
    sa_frames = 0;
    sa_status = sa_kind == 2 ? 10 : 5;
}
static void sa_open(void)
{
    const int r = sa_idle();
    sa_request = 0;
    if (r != 1) { sa_refuse(r, "save game"); return; }
    sa_kind = 1;
    sa_preload();
}
static void la_open(void)
{
    const int r = la_idle();
    la_request = 0;
    if (r != 1) { sa_refuse(r, "load game"); return; }
    sa_kind = 2;
    sa_preload();
}

static void hkLaCb(void);
static void hkSaCb(void);  // v3.32 PM332
// la_start_overlay: a fresh private nested-SM block, then the overlay with la_cb
static void la_start_overlay(void)
{
    const int seam = EmInBridge();  // v3.51 MU351: opened inside the pause
    memset(la_sm, 0, sizeof la_sm);
    la_phase = 0;
    la_state = 0;
    EM_CALL0(0x4141A0);
    if (!EmOverlay((DWORD)(ULONG_PTR)hkLaCb)) {  // v3.32 PM332: EmOverlay (inside our hand-over overlay: no 4F8DD0)
        EM_CALL0(0x4141B0);
        EM_CALL0(0x4F5070);
        sa_status = -2;
        Log("extra menu: load game: overlay request failed");
        return;
    }
    la_active = 1;
    sa_frames = 0;
    sa_status = 11;
    Log("extra menu: load list opened");
    MuLogSound("load list", seam ? "opened from the pause" : "opened from play", seam);  // v3.51 MU351
}

// sa_start_overlay: snap_save.bin must be resident; save = the notebook object 0BC7h's f1 + f2 state 0 (task 4CCDC0)
static void sa_start_overlay(void)
{
    const int seam = EmInBridge();  // v3.51 MU351: opened inside the pause
    sa_loading = 0;
    if (!ch_resident(0xF000F00A)) {
        EmHudBack();  // v3.32 PM332: was 4F5070 (HUD on); from our overlay: back to the pause
        sa_status = -8;
        Log("extra menu: snap_save.bin not resident: nothing opened");
        return;
    }
    if (sa_kind == 2) {
        la_start_overlay();
        return;
    }
    EM_CALL0(0x4CCD60);  // reset the save-task globals FD14F8..FD1578, create the fade task
    EM_CALL0(0x4F5090);
    EM_SOUND(0x9C49);
    EM_CALL0(0x4141A0);
    if (!EmOverlay((DWORD)(ULONG_PTR)hkSaCb)) {  // v3.32 PM332: EmOverlay; the task 4CCDC0 runs from hkSaCb
        EM_CALL0(0x4141B0);
        EM_CALL0(0x4F5070);
        if (EM_U(0xFD1578)) {  // delete the fade task again
            EM_CALL1(0x553AA0, EM_U(0xFD1578));
            EM_U(0xFD1578) = 0;
        }
        sa_status = -2;
        Log("extra menu: save game: overlay request failed");
        return;
    }
    // the stock save sequence task 4CCDC0 (fade -> 571AC0 save menu -> fade), called by hkSaCb (v3.32 PM332)
    sa_active = 1;
    sa_frames = 0;
    sa_stuck = 0;
    sa_status = 1;
    Log("extra menu: save menu opened");
    MuLogSound("save menu", seam ? "opened from the pause" : "opened from play", seam);  // v3.51 MU351
}

// sa_wait: the file queue (timeout 900 frames, -7), re-checking idle every frame
static void sa_wait(void)
{
    ++sa_frames;
    if ((sa_kind == 2 ? la_idle() : sa_idle()) == 1) {
        if (EM_CALL1(0x573370, 1) == 1) {
            sa_start_overlay();
            return;
        }
        if (sa_frames < 900) return;
        sa_status = -7;
    }
    sa_loading = 0;  // sa_wait_cancel
    if (sa_status != -7) sa_status = -1;
    Log("extra menu: %s: %s (sa_status %d)", sa_kind == 2 ? "load game" : "save game",
        sa_status == -7 ? "the files did not load within 900 frames" : "no longer possible", sa_status);
    EmHudBack();  // v3.32 PM332: was 4F5070 (HUD on) first; from our overlay: back to the pause
}

// the save overlay runs: finish when the task set FD1577 (= the notebook's f2 state 1 + f4)
static void sa_overlay(void)
{
    ++sa_frames;
    // [not in the table] internal sa_abort ("Fix Stuck Menu" was a manual table row): task 4CCDC0 waits in state 2
    // ([FD1575]) for 570760 = [13D1A3C] "menu finished", but the save-menu task 571130 is gone; 300 frames of that
    if (EM_B(0xFD1575) == 2 && !EM_I(0x13D1A3C) && !EM_CALL1(0x553A20, 0x571130)) {
        if (++sa_stuck >= 300) sa_abort = 1;
    } else {
        sa_stuck = 0;
    }
    if (sa_abort) {  // CT 1131-1147
        const int t = EM_CALL1(0x553A20, 0x571130);  // find the save-menu task by its function
        sa_abort = 0;
        sa_stuck = 0;
        if (t) {
            EM_CALL1(0x52B020, t);                    // delete it
            if (EM_I(0x107CD68)) EM_CALL0(0x4FBD30);  // free the save I/O buffer
        }
        EM_I(0x13D1A3C) = 1;  // "menu finished": 4CCDC0 fades out and leaves the overlay normally
        sa_status = -5;
        Log("extra menu: the save menu looked stuck for 300 frames: closed (sa_abort)");
    }
    if (!EM_B(0xFD1577)) return;
    sa_active = 0;
    EM_I(0xFD1570) = 0;
    EM_CALL0(0x4141B0);
    EM_CALL0(0x4F5070);
    EM_CALL1(0x56E960, -1);
    sa_status = 2;
    Log("extra menu: save menu closed");
}

// v3.32 PM332: hkSaCb = the overlay callback of the save: the stock task 4CCDC0 for one frame, then, in the tick its
// step 5 left the overlay (4D0C90 = 4F8E60, FD1577 set; its own fade has shown the frozen picture again), the finish of
// sa_overlay without the HUD on and sound back, straight into the pause. Otherwise sa_overlay finishes at the next tick.
static void sa_cb_body(void)
{
    if (!sa_active || !EM_B(0xFD1577) || pm_ret_row != PM_SAVE || !EmPauseOk()) return;
    sa_active = 0;
    EM_I(0xFD1570) = 0;
    EM_CALL0(0x4141B0);
    EmToPause(PM_SAVE, 0);
    if (sa_status != -5) sa_status = 2;
    Log("extra menu: save menu closed");
    MuLogSound("save menu", "closed, back in the pause", 1);  // v3.51 MU351
}
static void __cdecl sa_cb_c(void)
{
    ((void(__cdecl *)(void))0x4CCDC0)();
    __try {
        sa_cb_body();
    } __except (EmFault(GetExceptionInformation(), "sa_cb")) {
    }
}

// la_cb_run: the title's load menu 571B40 for one frame on the private block la_sm (swapped in for 1236D48)
static void la_cb_run(void)
{
    memcpy(la_sm_save, (const void *)0x1236D48, 0x40);
    memcpy((void *)0x1236D48, la_sm, 0x40);
    __try {
        EM_CALL0(0x571B40);
    } __finally {
        memcpy(la_sm, (const void *)0x1236D48, 0x40);
        memcpy((void *)0x1236D48, la_sm_save, 0x40);
    }
}

// 570490(0, 0, 200h, 1C0h, 0, 0) with eax = F000F00A: the list background while fading out (as 5725E2)
static void la_draw_bg(void)
{
    __asm {
        push 0
        push 0
        push 1C0h
        push 200h
        push 0
        push 0
        mov eax, 0F000F00Ah
        mov ecx, 570490h
        call ecx
        add esp, 18h
    }
}

// la_cb: the sub-mode 1 overlay callback of the load list
static void la_cb_body(void)
{
    ++sa_frames;
    if (la_phase == 0) {
        const DWORD st = la_sm[2];
        la_state = (int)st;
        if (st == 0x3E6) {  // "Loading completed" ended: the slot is in 107CD70. Do NOT run 3E6h (414A00 / 4F53F0)
            la_phase = 1;
            la_fade = 0;
        } else {
            if (st == 0x64) {  // confirm on a used entry: a clear-data entry is refused (no ABNF menu, no NG+ start)
                const BYTE *cat = (const BYTE *)(ULONG_PTR)EM_CALL1(0x4FBCE0, 0);
                if (*(const DWORD *)(cat + 0x30 + EM_I(0x13D1A0C) * 0x30) == 0x12) {
                    la_sm[2] = 7;  // back to the list
                    EM_SOUND(0x9C44);
                    sa_status = -11;
                    Log("extra menu: load game: a clear-data slot is refused");
                }
            } else if (st == 0x6E) {  // the ABNF menu (clear data) never runs in game: back to the list
                la_sm[2] = 7;
            }
            la_cb_run();
            la_state = (int)la_sm[2];
            if (la_sm[1]) {  // parent state advanced = left by cancel (it freed the I/O buffer, faded to black)
                int back = 0;  // v3.51 MU351
                la_phase = 3;
                if (pm_ret_row == PM_LOAD && EmPauseOk()) {  // v3.32 PM332: our overlay -> the pause, fading in
                    EM_CALL0(0x4141B0);
                    EmToPause(PM_LOAD, 1);
                    back = 1;
                } else {
                    EM_CALL0(0x4F8E60);
                    EM_CALL0(0x4141B0);
                    EM_CALL2(0x565CB0, 0, 8);
                }
                la_active = 0;
                sa_status = 12;
                Log("extra menu: load list cancelled");
                MuLogSound("load list", back ? "closed, back in the pause" : "closed, back to play", back);  // v3.51 MU351
            }
            return;
        }
    }
    if (la_phase != 1) return;
    if (EM_CALL1(0x5736D0, 0xF000F00A)) la_draw_bg();
    la_fade += 8;
    EM_CALL1(0x565CA0, la_fade);  // global fade level (current = target)
    if (la_fade < 0x80) return;
    la_phase = 2;  // black: keep the image, close the overlay, restart from the engine
    memcpy(la_buf, (const void *)0x107CD70, QS_IMG);
    EM_U(0x13D1A30) = 1;
    if (EM_I(0x107CD68)) EM_CALL0(0x4FBD30);  // 3E6h never frees the I/O buffer
    EM_CALL0(0x4F8E60);                       // screen stays black until the world restart
    la_active = 0;
    la_restart = 1;
    sa_status = 13;
    Log("extra menu: load game: slot read (stage %d room %d)", *(const int *)(la_buf + 0x14), *(const int *)(la_buf + 0x18));
    MuLogSound("load list", "closed, a save loads (the room change takes the music over)", 0);  // v3.51 MU351
}
static void __cdecl la_cb_c(void)
{
    __try {
        la_cb_body();
    } __except (EmFault(GetExceptionInformation(), "la_cb")) {
    }
}

// la_do_restart at sub-mode 0: sa_restart's checks (CT 1550-1562), then Refined's shared restart (QsRestart)
static void la_do_restart(void)
{
    const int stage = *(const int *)(la_buf + 0x14), room = *(const int *)(la_buf + 0x18);
    if (EM_SUB || EM_PEND) return;
    la_restart = 0;
    if (*(const DWORD *)la_buf == QS_IMG && stage > 0 && stage < 0x12 && room > 0 && room < 0x100 &&
        QsRestart(la_buf, NULL)) {
        sa_status = 14;
        Log("extra menu: load game: world restart to stage %d room %d", stage, room);
        return;
    }
    EM_CALL0(0x4141B0);  // sa_rs_fail
    EM_CALL2(0x565CB0, 0, 8);
    sa_status = -13;
    Log("extra menu: load game: restart refused (bad slot image or every catalog entry is clear data)");
}

// ---------------------------------------------------------------- return to the menu (RE 1.4; CT 1426-1527)
// pm_reopen: the game's own Escape path 4F8BC6..4F8C12, opening on the Extra Menu at row pm_ret_row
static void pm_reopen(void)
{
    EM_CALL1(0x56E960, 0);
    EM_I(0x107BF54) = -1;
    EM_I(0x107BF58) = 2;
    EM_I(0x107BF5C) = 0;
    EM_I(0xFD5BC0) = 1;
    EM_CALL0(0x5000E0);
    EM_CALL0(0x553CF0);
    EM_CALL0(0x4F5090);
    pm_sub = 1;
    pm_want = pm_ret_row;
    ++pm_ret_n;
    QsAnyValid();  // the sub-list opens
    Log("extra menu: pause menu reopened on the Extra Menu (%d)", pm_ret_n);
}

static void pm_ret_check(void)
{
    if (pm_ret_go) {  // pm_ret_wait
        if (++pm_ret_frames > 300 || EM_SUB == 2 || EM_PEND == 2) goto drop;
        if (EM_SUB || EM_PEND || pm_busy() || sa_request || la_request || ch_request || ql_request) return;
        if (!sa_hard() || !ch_hard() || EM_I(0x10A0F28) <= 0) return;
        if (EM_I(0x137F98C) || EM_I(0x137F990)) return;  // a fade runs
        if (EM_CALL1(0x573370, 1) != 1) return;          // file queue not empty
        pm_reopen();
        goto drop;
    }
    if (!pm_ret_row || pm_busy()) return;
    if (pm_ret_row == PM_CHEST ? ch_status == 3 : pm_ret_row == PM_SAVE ? sa_status == 2 : sa_status == 12) {
        pm_ret_go = 1;  // pm_ret_arm
        pm_ret_frames = 0;
        return;
    }
drop:
    pm_ret_go = 0;
    pm_ret_row = 0;
}

// pm_eo_lr: Left / Right on the Eileen row of the open sub-list (Outfit v4), new presses of 5542C0(0): 4 Left, 8 Right
static void pm_eo_lr(void)
{
    const int cur = EM_CUR;
    int k;
    if (EM_SUB != 2 || EM_PEND || !pm_sub || pm_pe_open()) return;
    if ((unsigned)cur >= (unsigned)pm_n || pm_vis[cur] != PM_EILEEN || !pm_eo4()) return;
    k = EM_CALL1(0x5542C0, 0) & 0x0C;
    if (k != 8 && k != 4) return;  // none, or both at once
    if (pm_eo_step(k == 8 ? 1 : -1)) EM_SOUND(0x9C40);
}

// v3.37 CP337: Left / Right on a setting of the open Camera page, with the menu repeat (camera_sim.h CamRepeatStep):
// held directions 5542A0(0), new presses 5542C0(0)
static void pm_cam_lr(void)
{
    const int cur = EM_CUR;
    int d, r;
    if (EM_SUB != 2 || EM_PEND || pm_sub != 2 || pm_pe_open()) {
        CamRepeatReset(&pm_camRep);
        return;
    }
    {
        const int pressed = EM_CALL1(0x5542C0, 0);
        d = CamRepeatStep(&pm_camRep, pm_heldOk ? EM_CALL1(0x5542A0, 0) : pressed, pressed);  // (no repeat without 0x5542A0)
    }
    if (!d || (unsigned)cur >= (unsigned)pm_n) return;
    r = pm_vis[cur] - PM_CAMR0;
    if (r < 0 || r >= CAMR_N || !CamPauseOn()) return;
    if (CamPauseStep(r, d, 0)) EM_SOUND(0x9C40);
}

// ---------------------------------------------------------------- the per-frame engine (sa_code, CT 1104-1201)
static void sa_check_req(void)
{
    if (!(sa_request | la_request | ch_request | ql_request)) return;
    if (pm_defer) {  // a request from the pause: wait until the pause sub-mode has closed
        if (EmSubBusy()) {  // v3.32 PM332: not inside our hand-over overlay (it runs there at once)
            if (++pm_defer_frames < 120) return;
            pm_defer = 0;  // sa_defer_wait timeout (the table keeps ql_request: it then runs undeferred)
            sa_request = la_request = 0;
            sa_status = -9;
            if (ch_request) ch_status = -9;
            ch_request = 0;
            Log("extra menu: the pause menu did not close within 120 frames: request dropped");
            return;
        }
        pm_defer = 0;
    }
    if (sa_request) sa_open();
    else if (la_request) la_open();
    else if (ch_request) ch_open();
    else if (ql_request) {  // sa_ql: Refined's quick load (its own guards, sound and text)
        // v3.33 MENU333: what only the closing pause or a passing state blocks (sub-mode, phase, input, a fade, a data
        // file, a message) is waited for, up to 1.5 s; then QsLoad decides (and logs a refusal)
        if (!QsGuardHard(1, 0, 1) && QsGuard(1, 0) && GetTickCount() - g_emQlTick < 1500) return;
        ql_request = 0;
        QsLoad();
    }
}

// g_extraTick: first thing of every game tick (0x4EF982 dispatcher, inside its SEH), the table's sa_code moment
static void __cdecl EmTick(void)
{
    if (pm_pe_open()) return;  // E5: Playable Eileen's menu is up
    if (EM_SUB != 2 && EM_PEND != 2 && pm_sub) {  // the sub-list only lives while the pause is up or pending
        pm_sub = 0;
        pm_want = PM_EXTRA;
    }
    pm_ret_check();
    pm_eo_lr();
    pm_cam_lr();  // v3.37 CP337
    em_qs_wait();  // v3.33 MENU333
    if (ch_active) { ch_run(); return; }
    if (la_restart) { la_do_restart(); return; }
    if (sa_loading) { sa_wait(); return; }
    if (la_active) return;  // driven by la_cb
    if (sa_active) { sa_overlay(); return; }
    sa_check_req();
}

// ---------------------------------------------------------------- naked stubs (the table's patch targets)
static const DWORD g_emBackMouseY = 0x4F8F36, g_emBackMouseN = 0x4F8F64, g_emBackDown = 0x4F8FB4;
static const DWORD g_emDepBack = 0x51223B, g_emDepRefuse = 0x512254, g_em4F55E0 = 0x4F55E0;

// overlay callbacks for 4F8E40 (called with `call eax` at 4F92CF): every register kept, as ch_cb / la_cb (pushad)
static __declspec(naked) void hkChCb(void)
{
    __asm {
        pushad
        cld
        call ch_cb_c
        popad
        ret
    }
}
static __declspec(naked) void hkLaCb(void)
{
    __asm {
        pushad
        cld
        call la_cb_c
        popad
        ret
    }
}
// v3.32 PM332: the save's callback (4CCDC0 + the seamless finish) and the hand-over overlay's watchdog
static __declspec(naked) void hkSaCb(void)
{
    __asm {
        pushad
        cld
        call sa_cb_c
        popad
        ret
    }
}
static __declspec(naked) void hkEmBridge(void)
{
    __asm {
        pushad
        cld
        call em_bridge_c
        popad
        ret
    }
}

// 4F8F2E (was `mov [esp+10h],86h`): pm_mouse_code: layout, ebx / edi = the cursor again, first rect y = pm_my0
static __declspec(naked) void hkPmMouseY(void)
{
    __asm {
        pushad
        call pm_layout_c
        popad
        mov eax, dword ptr ds:[107BF68h]
        mov ebx, eax
        mov edi, eax
        mov eax, pm_my0
        mov dword ptr [esp + 10h], eax
        jmp dword ptr [g_emBackMouseY]
    }
}

// 4F8F5D (was `add edx,1Ch / inc esi / cmp esi,5`): pm_mn_code; the cmp's flags feed the jl at 4F8F68 (v3.37 CP337: the
// step is pm_step)
static __declspec(naked) void hkPmMouseN(void)
{
    __asm {
        add edx, pm_step  // v3.37 CP337: was 1Ch (the Camera page's rows are closer)
        inc esi
        cmp esi, pm_n
        jmp dword ptr [g_emBackMouseN]
    }
}

// 4F8FAC (was `mov eax,[107BF68] / cmp eax,4`): pm_down_code; the flags feed `jge 4F90E6` at 4F8FB4
static __declspec(naked) void hkPmDown(void)
{
    __asm {
        pushad
        call pm_layout_c
        popad
        mov edx, pm_n
        dec edx
        mov eax, dword ptr ds:[107BF68h]
        cmp eax, edx
        jmp dword ptr [g_emBackDown]
    }
}

// 4F8FE9 (was `mov eax,[107BF68]`, the confirm): pm_dispatch; the 4F8E90 frame is intact (esi 0, edi 1)
static __declspec(naked) void hkPmDisp(void)
{
    __asm {
        pushad
        call pm_dispatch_c
        popad
        mov eax, g_emEax
        jmp dword ptr [g_emJmp]
    }
}

// 4F8EC1 / 4F8ED3 (were `jne 4F90C2`, right click / cancel key): pm_cancel_code
static __declspec(naked) void hkPmCancel(void)
{
    __asm {
        pushad
        call pm_cancel_c
        popad
        jmp dword ptr [g_emJmp]
    }
}

// 512235 (was `push edi / call 4F55E0`, edi = the item to put in the box): ch_dep_code. While our chest runs, the
// weapon in Henry's hand [10A1164] is refused like an item equipped by Eileen (512254: buzzer, nothing moved).
static __declspec(naked) void hkChDep(void)
{
    __asm {
        cmp dword ptr [ch_active], 0
        je orig
        cmp edi, dword ptr ds:[10A1164h]
        jne orig
        inc dword ptr [ch_blocked]
        jmp dword ptr [g_emDepRefuse]
    orig:
        push edi
        call dword ptr [g_em4F55E0]
        jmp dword ptr [g_emDepBack]  // 51223B: add esp,4
    }
}

// ---------------------------------------------------------------- install
static int EmOurPatch(DWORD va, int len)  // the range is already patched by Refined itself (e.g. 565CB0, frame rate)
{
    DWORD i;
    for (i = 0; i < SH4WM_Patches.count; ++i)
        if (va < SH4WM_Patches.item[i].va + SH4WM_Patches.item[i].len && SH4WM_Patches.item[i].va < va + (DWORD)len)
            return 1;
    return 0;
}

static void EmLogDiff(const Site *s)
{
    char a[100], b[100];
    int i;
    a[0] = b[0] = 0;
    for (i = 0; i < s->len && i < 32; ++i) {
        sprintf_s(a + 3 * i, sizeof a - 3 * i, "%02X ", s->stock[i]);
        sprintf_s(b + 3 * i, sizeof b - 3 * i, "%02X ", ((const BYTE *)(ULONG_PTR)s->va)[i]);
    }
    Log("extra menu: %08lX is not stock: expected %s", s->va, a);
    Log("extra menu: %08lX found               %s", s->va, b);
}

static int PokeJne(DWORD va, const void *to)  // 0F 85 rel32 (6 bytes)
{
    BYTE b[6] = { 0x0F, 0x85 };
    DWORD rel = (DWORD)(ULONG_PTR)to - (va + 6);
    memcpy(b + 2, &rel, 4);
    return Poke(va, b, 6);
}

static void ExtraInstall(void)
{
    // the 8 patch sites (RE 1.2, the table's AOBs at their VAs)
    static const Site kSites[] = {
        { 0x4F8A30, { 0x53, 0x56, 0x57, 0x33, 0xFF, 0x3B, 0x3D, 0x68, 0xBF, 0x07, 0x01, 0x75, 0x32 }, 13 },
        { 0x4F8F2E, { 0xC7, 0x44, 0x24, 0x10, 0x86, 0x00, 0x00, 0x00, 0xC7, 0x44, 0x24, 0x14, 0x00, 0x02, 0x00, 0x00 }, 16 },
        { 0x4F8F5D, { 0x83, 0xC2, 0x1C, 0x46, 0x83, 0xFE, 0x05, 0x89, 0x54, 0x24, 0x10, 0x7C, 0xDC }, 13 },
        { 0x4F8FAC, { 0xA1, 0x68, 0xBF, 0x07, 0x01, 0x83, 0xF8, 0x04, 0x0F, 0x8D }, 10 },
        { 0x4F8FE9, { 0xA1, 0x68, 0xBF, 0x07, 0x01, 0x83, 0xF8, 0x04, 0x0F, 0x87, 0xEF, 0x00, 0x00, 0x00, 0xFF, 0x24, 0x85,
                      0xF0, 0x90, 0x4F, 0x00 }, 21 },
        { 0x4F8EBA, { 0xE8, 0xF1, 0xC5, 0xF0, 0xFF, 0x85, 0xC0, 0x0F, 0x85, 0xFB, 0x01, 0x00, 0x00, 0x57, 0x56, 0xE8, 0xD2,
                      0xF5, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x85, 0xC0, 0x0F, 0x85, 0xE9, 0x01, 0x00, 0x00 }, 31 },
        { 0x512231, { 0x85, 0xFF, 0x74, 0x1F, 0x57, 0xE8, 0xA5, 0x33, 0xFE, 0xFF, 0x83, 0xC4, 0x04, 0x85, 0xC0, 0x74, 0x23,
                      0xE8 }, 18 },
    };
    // entries of the game functions called here and the jump targets (those QuickSaveInstall does not check)
    static const Site kFns[] = {
        { 0x4F90F0, { 0xC2, 0x90, 0x4F, 0x00, 0xFE, 0x8F, 0x4F, 0x00, 0x25, 0x90, 0x4F, 0x00, 0x6A, 0x90, 0x4F, 0x00, 0x96,
                      0x90, 0x4F, 0x00 }, 20 },                                          // stock confirm jump table
        { 0x4F90C2, { 0xE8, 0x29, 0xA1, 0xF0, 0xFF }, 5 },                               // Resume path
        { 0x4F90E6, { 0x5F, 0x5E, 0x5B, 0x83, 0xC4, 0x18, 0xC3 }, 7 },                   // 4F8E90 epilogue
        { 0x512254, { 0x68, 0x44, 0x9C, 0x00, 0x00 }, 5 },                               // put-in refused (buzzer)
        { 0x56CC40, { 0x83, 0xEC, 0x08, 0xE8, 0x48, 0x15, 0x00, 0x00 }, 8 },             // sound
        { 0x556A50, { 0xA1, 0x1C, 0x1C, 0x26, 0x01 }, 5 },                               // text shadow palette
        { 0x50B5B0, { 0x8B, 0x44, 0x24, 0x18, 0x8B, 0x4C, 0x24, 0x14 }, 8 },             // text
        { 0x556A90, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0x1C, 0x1C, 0x26, 0x01 }, 10 }, // text colour
        { 0x521100, { 0xE9, 0xFB, 0xF0, 0xFF, 0xFF }, 5 },                               // map available
        { 0x573470, { 0x81, 0xEC, 0x10, 0x02, 0x00, 0x00 }, 6 },                         // file request
        { 0x5736D0, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C }, 6 },                         // resident sub-file
        { 0x4F5090, { 0xC6, 0x05, 0x30, 0x9C, 0x07, 0x01, 0x00 }, 7 },                   // player input off
        { 0x4F5070, { 0xC6, 0x05, 0x30, 0x9C, 0x07, 0x01, 0x01 }, 7 },                   // player input on
        { 0x4CCD60, { 0x57, 0x33, 0xC0, 0xB9, 0x20, 0x00, 0x00, 0x00 }, 8 },             // save-task reset
        { 0x4CCDC0, { 0x0F, 0xB6, 0x05, 0x75, 0x15, 0xFD, 0x00 }, 7 },                   // save sequence task
        { 0x4141A0, { 0xC7, 0x05, 0x10, 0xB2, 0xEC, 0x00, 0x00, 0x00, 0x00, 0x00 }, 10 },
        { 0x4141B0, { 0xC7, 0x05, 0x10, 0xB2, 0xEC, 0x00, 0x01, 0x00, 0x00, 0x00 }, 10 },
        { 0x4F8DD0, { 0xA1, 0x50, 0xBF, 0x07, 0x01, 0x85, 0xC0 }, 7 },                   // overlay request (sub-mode 1)
        { 0x4F8E40, { 0xA1, 0x54, 0xBF, 0x07, 0x01, 0x85, 0xC0 }, 7 },                   // overlay callback
        { 0x4F8E60, { 0xE8, 0x8B, 0xA3, 0xF0, 0xFF }, 5 },                               // leave the overlay
        { 0x553AA0, { 0x57, 0x8B, 0x7C, 0x24, 0x08 }, 5 },                               // delete a task
        { 0x553A20, { 0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x08 }, 6 },                         // find a task
        { 0x52B020, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, 5 },                               // delete a task
        { 0x4FBD30, { 0xA1, 0x68, 0xCD, 0x07, 0x01 }, 5 },                               // free the save I/O buffer
        { 0x56E960, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 }, 6 },
        { 0x571B40, { 0x83, 0xEC, 0x38, 0x53, 0x55, 0x56, 0x57 }, 7 },                   // title load menu
        { 0x570490, { 0x56, 0x57, 0x50, 0xE8, 0x38, 0x32, 0x00, 0x00 }, 8 },             // list background
        { 0x565CA0, { 0x8B, 0x44, 0x24, 0x04, 0xA3, 0x8C, 0xF9, 0x37, 0x01 }, 9 },       // fade level
        { 0x565CB0, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x4C, 0x24, 0x08 }, 8 },             // fade (Refined hooks it)
        { 0x565D20, { 0x8B, 0x0D, 0x8C, 0xF9, 0x37, 0x01 }, 6 },                         // fade finished?
        { 0x514A60, { 0xC6, 0x05, 0xA9, 0xEC, 0x08, 0x01, 0x00, 0xC3 }, 8 },
        { 0x4CE1A0, { 0x53, 0xE8, 0xEA, 0x68, 0x04, 0x00 }, 6 },                         // chest overlay task
        { 0x510970, { 0xE8, 0xEB, 0x47, 0x00, 0x00 }, 5 },
        { 0x4F0860, { 0xC7, 0x05, 0x78, 0x6C, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00 }, 10 },
        { 0x5171A0, { 0x33, 0xC0, 0xA3, 0x98, 0xEC, 0x08, 0x01 }, 7 },
        { 0x5000E0, { 0xC7, 0x05, 0x1C, 0x7B, 0x3D, 0x01, 0x01, 0x00, 0x00, 0x00 }, 10 },
        { 0x553CF0, { 0x56, 0x33, 0xF6, 0x6A, 0x00 }, 5 },
        { 0x5542C0, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0 }, 6 },                         // new direction presses
        { 0x4F55E0, { 0x8B, 0x44, 0x24, 0x04, 0x3D, 0x80, 0x00, 0x00, 0x00 }, 9 },       // Eileen-weapon class
        { 0x403250, { 0x83, 0xEC, 0x34, 0x56, 0x57 }, 5 },  // v3.32 PM332: RenderFixedTargetsBackup
        { 0x406C10, { 0xE8, 0x0B, 0xFD, 0xFF, 0xFF }, 5 },  // v3.32 PM332: InputForceStop (rumble)
    };
    int i, bad = 0, ok = 1;
    if (!g_qsTickOn) {
        Log("extra menu: the 0x4EF982 dispatcher is not in place: nothing installed");
        return;
    }
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            EmLogDiff(&kSites[i]);
            bad = 1;
        }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i) {
        if (EmOurPatch(kFns[i].va, kFns[i].len)) continue;  // Refined's own hook there (calls go through it)
        if (!SiteOk(&kFns[i]) && PeHookedCallee(kFns[i].va, "extra menu")) continue;  // v3.34 PEC334
        if (!SiteOk(&kFns[i])) {
            EmLogDiff(&kFns[i]);
            bad = 1;
        }
    }
    if (bad) {
        Log("extra menu: stock bytes differ (another mod, or the cheat table's Pause Menu+ attached first?): nothing "
            "installed, the game's own pause menu stays");
        return;
    }
    // texts (pm_rows, CT 653-748; the table's defaults ch_label 1, pm_back_label 1)
    Encode(pm_str_cheats, sizeof pm_str_cheats, "Extra Menu");
    Encode(pm_str_back, sizeof pm_str_back, "Back");
    Encode(pm_str_save, sizeof pm_str_save, "Save Game");
    Encode(pm_str_load, sizeof pm_str_load, "Load Game");
    Encode(pm_str_chest, sizeof pm_str_chest, "Storage Chest");
    Encode(pm_str_qsave, sizeof pm_str_qsave, "Quick Save");
    Encode(pm_str_qload, sizeof pm_str_qload, "Quick Load");
    Encode(pm_str_pe, sizeof pm_str_pe, "Playable Eileen");
    Encode(pm_str_eileen, sizeof pm_str_eileen, "Eileen");
    Encode(pm_str_def, sizeof pm_str_def, "Default");
    Encode(pm_str_dress, sizeof pm_str_dress, "Dress");
    Encode(pm_str_nurse, sizeof pm_str_nurse, "Nurse");
    Encode(pm_str_camera, sizeof pm_str_camera, "Camera");  // v3.37 CP337
    Encode(pm_str_lt, sizeof pm_str_lt, "<");
    Encode(pm_str_gt, sizeof pm_str_gt, ">");
    Encode(pm_str_camhint, sizeof pm_str_camhint, "Left / Right change it, saved at once.");
    Encode(pm_str_camhint2, sizeof pm_str_camhint2, "The view follows when you resume.");
    {
        static const int kStock[6][2] = { { 0, 0x7A }, { -1, 0 }, { 0, 0x7B }, { 0, 0x7C }, { 0, 0x7D }, { 0, 0x7E } };
        const BYTE *raw[PM_IDS] = { NULL, pm_str_cheats, NULL, NULL, NULL, NULL, pm_str_chest, pm_str_save,
                                    pm_str_load, NULL, pm_str_back, pm_str_qsave, pm_str_qload, pm_str_pe, pm_str_camera };
        for (i = PM_CAMR0; i <= PM_CAMBACK; ++i) raw[i] = pm_str_back;  // v3.37 CP337 (the page draws its own rows)
        for (i = 0; i < PM_IDS; ++i) {
            pm_rows[i][0] = raw[i] ? -1 : 0;
            pm_rows[i][1] = raw[i] ? EM_RAW(raw[i]) : i < 6 ? kStock[i][1] : 0xCA;  // 9: 0CAh "Eileen Costume"
        }
    }
    {   // all or nothing (inspection EM-001): keep the stock bytes, put every written site back if one write fails
        static const struct { DWORD va; int len; } kPatch[8] = { { 0x4F8A30, 5 }, { 0x4F8F2E, 8 }, { 0x4F8F5D, 7 },
            { 0x4F8FAC, 8 }, { 0x4F8FE9, 5 }, { 0x4F8EC1, 6 }, { 0x4F8ED3, 6 }, { 0x512235, 6 } };
        BYTE saved[8][8];
        int done = 0;
        for (i = 0; i < 8; ++i) memcpy(saved[i], (const void *)(ULONG_PTR)kPatch[i].va, kPatch[i].len);
        ok = PokeBranch(0x4F8A30, 0xE9, pm_draw, 5);                 // pm_draw_hook
        if (ok) { ++done; ok = PokeBranch(0x4F8F2E, 0xE9, hkPmMouseY, 8); }  // pm_mouse_y
        if (ok) { ++done; ok = PokeBranch(0x4F8F5D, 0xE9, hkPmMouseN, 7); }  // pm_mouse_n
        if (ok) { ++done; ok = PokeBranch(0x4F8FAC, 0xE9, hkPmDown, 8); }    // pm_down_max
        if (ok) { ++done; ok = PokeBranch(0x4F8FE9, 0xE9, hkPmDisp, 5); }    // pm_disp
        if (ok) { ++done; ok = PokeJne(0x4F8EC1, hkPmCancel); }              // pm_cancel+7 (right click)
        if (ok) { ++done; ok = PokeJne(0x4F8ED3, hkPmCancel); }              // pm_cancel+19 (cancel key)
        if (ok) { ++done; ok = PokeBranch(0x512235, 0xE9, hkChDep, 6); }     // ch_dep+4
        if (!ok) {
            int lost = 0;  // inspection EM-ROLLBACK-001: a restore can fail too
            for (i = 0; i < done; ++i)
                if (!Poke(kPatch[i].va, saved[i], kPatch[i].len)) {
                    ++lost;
                    Log("extra menu: %08lX could not be put back", kPatch[i].va);
                }
            if (!lost) {
                Log("extra menu: a patch could not be written (VirtualProtect failed): the %d written site(s) are stock "
                    "again; the game's own pause menu", done);
                return;
            }
            // fail closed: hooks stayed in place, so their engine runs and the table keeps out (its AOBs cannot match)
            Log("extra menu: %d site(s) stayed patched after a failed install: the Extra Menu engine stays on for them", lost);
        }
    }
    {   // v3.37 CP337: the Camera page's Left / Right repeat reads the held directions; optional (without it: one step a press)
        static const Site kHeld = { 0x5542A0, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9 }, 8 };
        pm_heldOk = EmOurPatch(kHeld.va, kHeld.len) || SiteOk(&kHeld) || PeHookedCallee(kHeld.va, "extra menu");
        if (!pm_heldOk) {
            EmLogDiff(&kHeld);
            Log("extra menu: 005542A0 (held directions) is not stock: the Camera page steps once a press (no repeat while held)");
        }
    }
    g_extraTick = EmTick;
    SH4R_Extra[2] |= 1;  // owns bit 0: the table's Pause Menu+ stays off
    // v3.32 PM332: hidden [ExtraMenu] Seamless (default 1); 0 = v3.31's close-and-reopen around the three menus
    g_emSeamless = GetPrivateProfileIntA("ExtraMenu", "Seamless", 1, g_iniPath) != 0;
    Log("extra menu: Storage Chest / Save Game / Load Game %s", g_emSeamless ? "open and close inside the pause (the "
        "world stays frozen)" : "close and reopen the pause (Seamless=0: v3.31)");
    // v3.51 MU351: hidden [ExtraMenu] PauseSound (default 1); 0 = v3.50's sound (the music back at 0.2 in the three menus)
    g_emPauseSound = GetPrivateProfileIntA("ExtraMenu", "PauseSound", 1, g_iniPath) != 0;
    if (g_emSeamless)
        Log("extra menu: sound in the Storage Chest / Save Game / Load Game: %s ([ExtraMenu] PauseSound=%d)",
            g_emPauseSound ? "paused as in the pause menu" : "plays quietly as in v3.50", g_emPauseSound);
    Log("extra menu: on, 8 patches (4F8A30 4F8F2E 4F8F5D 4F8FAC 4F8FE9 4F8EC1 4F8ED3 512235), engine in the 0x4EF982 "
        "dispatcher; rows Chest %ld Save %ld Load %ld QuickSave %ld QuickLoad %ld Eileen %ld (Option 6); Camera row (its page: "
        "the camera settings) while SH4 Refined's camera owns the view, now %s",
        g_emOn[EM_CHEST], g_emOn[EM_SAVE], g_emOn[EM_LOAD], g_emOn[EM_QSAVE], g_emOn[EM_QLOAD], g_emOn[EM_EILEEN],
        kCamOwnerName[(unsigned)g_camOwner < 4 ? g_camOwner : 0]);  // v3.37 CP337
}
