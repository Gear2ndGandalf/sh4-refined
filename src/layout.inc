// ================================================================ v3.54 OL34: the Options layout, rounds R3 + R4
// Spec: staging/options_layout/PLAN.md (sections 2, 4, 5, 6, 9), its data tools/layout_data.py and R0_REPORT.md (every
// move GO; R4's code changes 1-7); brief staging/v354_layout/BRIEF.md. The data is layout_sim.h (test\layout_sim_test.c).
//   R3  Option 5 becomes Gameplay: Player, Combat, Items, Enemies, Powers, Randomizer (Refined's rows only); Curse Meter and
//       Enemy Health go to Interface, Supply Drops to Items.
//   R4  Option 4's rows go to Display (Aspect Ratio, Display Mode, Monitor, Frame Rate) and Controls > Camera / Room 302
//       (the Fields of View, Room 302's rows); Option 6 becomes Interface > Menus with Menu Font and Start-Up; Controls >
//       Camera holds the pause camera's settings, Steering the third level; Screen Effects becomes the top page Effects
//       with Film Grain > and Shadows >; v3.52's Sound page is top page 3 (Exit, no parent; Sound Mode lost its Enter).
//   The stock rows stay on their stock pages until R5 (the sound three, the first-person three, the Subtitles / Noise /
//   Shadow switches, Blood Colour): PLAN section 4's fallbacks keep every page at 11 rows at most (Renderer moves to
//   Effects; Menu Font and Start-Up into the Pause Menu folder, renamed Menus).
// How: the v3.53 builders still build their pages, as origin pages (OP_*, g_opTable). OlLayoutBuild copies each record the
// plan places (found by its own input function: R0 R4-4, never by index) into the live page's table and keeps its origin
// page / row: RowChangedDraw, the greys, the long values and the arrow rules (sh4windowmod.c) ask in origin terms, so a
// moved row keeps every v3.53 answer (and a stock record answers with its own cfg byte: R0 R4-2). New rows: the folders
// (value-less, opening their page through REQ_OPEN), Back (REQ_BACK), Exit (the game's), the camera settings (the pause
// Extra Menu > Camera's own steps, camera.inc CamRowStep). No exe site of its own; every ini key and written word stays.
// v3.55 OL56 (rounds R5 + R6; staging/v355_layout/BRIEF.md, R0_REPORT.md's R5 list): the stock moves. layout_sim.h gives
// every row a mode; the hidden [Options] StockMoves (default 1) picks the layout:
//   1  every page is the plan's section 5 page: the sound three on Sound (Music / Effects Volume by name: the one exe patch
//      at 0x525672, volbar_sim.h, makes the bar test ask the record's input), Invert Look / Head Bob / Left / Right in
//      Controls > Room 302, the Subtitles / Film Grain / Shadows switches row 0 of their folders (the stock inputs: Enter
//      flips them, as the game's own On / Off rows), Interface's Subtitles a pure folder, Blood Colour on Gameplay,
//      Renderer on Display, Menu Font / Start-Up on Interface, the folder "Pause Menu" again;
//   0  v3.54's interim layout, row for row (no exe patch). Also the fallback when the sub-screen fade is not in place (the
//      folders that hold the three switches need it).
// The moved stock records keep their origin (page, row): the yellow (StockRowChangedAt: the record's own cfg byte), the
// exit scan 0x523D20 and the discard 0x526B6F are the game's, by byte (R0 R5-3 / R5-4); g_stockView is 7 on every page
// (R4-1), so no stock page / row / language case of the 13 page reads fires on a moved record.

typedef char OlCamRowsMatch[OL_CAM_ENABLE == CAMR_ENABLE && OL_CAM_DIST == CAMR_DIST && OL_CAM_SIDE == CAMR_SIDE &&
                            OL_CAM_HEIGHT == CAMR_HEIGHT && OL_CAM_TILT == CAMR_TILT && OL_CAM_TURN == CAMR_TURN &&
                            OL_CAM_TILTSPD == CAMR_TILTSPD && OL_CAM_INVX == CAMR_INVX && OL_CAM_INVY == CAMR_INVY &&
                            OL_CAM_WALL == CAMR_WALL && OL_CAM_MARGIN == CAMR_MARGIN && OL_CAM_FIXED == CAMR_FIXED &&
                            OL_CAM_N == CAMR_N ? 1 : -1];
typedef char OlIdsFit[LP_MAX <= 24 && OL_ROWS <= 16 ? 1 : -1];  // look.inc's per-page arrays hold 24; BYTE kinds / args

// ---------------------------------------------------------------- the text block MSG_V54 + 0 .. 127 (handed out in order)
// v3.55 OL56: + MSG_V55 + 0 .. 63 (MSG_V55 = MSG_V54 + 128: one pool of 192 ids handed out in order; the test counts each
// layout's texts)
#define OL4_N 192
typedef char Ol4PoolFits[MSG_V55 == MSG_V54 + 128 && MSG_V56 == MSG_V55 + 64 && OL4_N == 128 + 64 ? 1 : -1];  // v3.56 MERGE: the pool ends where MSG_V56 starts (was MSG_END; written by seven workers, kept once)
static BYTE g_txtOl4[OL4_N][96];
static const char *g_ol4Src[OL4_N];
static int g_ol4Used, g_ol4Full;
static const BYTE g_ol4Empty[4] = { 0x00, 0x80, 0xFF, 0xFF };  // an id of the block not handed out: an empty text
static const BYTE *Ol4Text(int id)
{
    const int i = id - MSG_V54;
    return i >= 0 && i < g_ol4Used ? g_txtOl4[i] : g_ol4Empty;
}
static int Ol4Add(const char *s)  // the id of text s (the same text: the same id)
{
    int i;
    if (!s) s = "";
    for (i = 0; i < g_ol4Used; ++i)
        if (g_ol4Src[i] == s || !strcmp(g_ol4Src[i], s)) return MSG_V54 + i;
    if (g_ol4Used >= OL4_N) {
        ++g_ol4Full;
        return MSG_V54;  // (cannot happen: the test counts the texts)
    }
    g_ol4Src[g_ol4Used] = s;
    Encode(g_txtOl4[g_ol4Used], sizeof g_txtOl4[0], s);
    return MSG_V54 + g_ol4Used++;
}

#include "vlines.inc"  // v3.56 MN356: every value its own line (VlWrapRow / VlBuilt)

// ---------------------------------------------------------------- the per-row tables of the live pages
static DWORD g_olLive[LP_MAX][OL_ROWS * 7 + 1];
static signed char g_olOp[LP_MAX][OL_ROWS], g_olOr[LP_MAX][OL_ROWS];  // the origin page / row (-1: a new row)
static BYTE g_olKind[LP_MAX][OL_ROWS], g_olArg[LP_MAX][OL_ROWS];
static int g_olPlaced, g_olMissing, g_olFolders;
static char g_olMissed[400];
// v3.55 OL56: the layout ([Options] StockMoves: 1 the plan's places of the stock rows, 0 v3.54's interim places) and why
static int g_olMoves;
static const char *g_olMovesWhy = "";
static int g_vbOn;  // 0x525672 asks the record's input (Music / Effects Volume have the plan's names)

static int OlRowAt(int page, int row, int *op, int *orow, int *arg)
{
    *arg = 0;
    if (!g_olOn) {  // the stock pages only (no Refined pages): identity
        *op = page, *orow = row;
        return OLK_VALUE;
    }
    if ((unsigned)page >= LP_MAX || (unsigned)row >= (unsigned)g_lpCount[page] || row >= OL_ROWS) {
        *op = *orow = -1;
        return OLK_END;
    }
    *op = g_olOp[page][row], *orow = g_olOr[page][row], *arg = g_olArg[page][row];
    return g_olKind[page][row];
}
static int OlBlue(int page, int row)
{
    if (!g_olOn) return (page == 0 && (row == 1 || row == 2)) || (page == 1 && row == 0);  // the stock pages: Brightness, Gamma, Button Config
    return (unsigned)page < LP_MAX && (unsigned)row < (unsigned)g_lpCount[page] && row < OL_ROWS && OlBlueKind(g_olKind[page][row]);
}
static int OlFxPage(int lp) { return g_olOn && (lp == LP_EFFECTS || lp == LP_GRAIN || lp == LP_DOF); }  // v3.58 PV358: + Effects > Depth of Field

// ---------------------------------------------------------------- thunks: a folder's input, fixed lines, the camera rows
#define OL_OPEN(n) static int __cdecl OlOpen##n(BYTE *c, int k) { (void)c; (void)k; return GpOpen(n); }
OL_OPEN(6) OL_OPEN(7) OL_OPEN(8) OL_OPEN(9) OL_OPEN(10) OL_OPEN(11) OL_OPEN(12) OL_OPEN(13) OL_OPEN(14) OL_OPEN(15) OL_OPEN(16)  // v3.58 PV358: + 14
OL_OPEN(18) OL_OPEN(19) OL_OPEN(21)
static const void *OlOpenFn(int lp)
{
    switch (lp) {
    case 6: return OlOpen6;
    case 7: return OlOpen7;
    case 8: return OlOpen8;
    case 9: return OlOpen9;
    case 10: return OlOpen10;
    case 11: return OlOpen11;
    case 12: return OlOpen12;
    case 13: return OlOpen13;
    case 14: return OlOpen14;  // v3.58 PV358: Effects > Depth of Field
    case 15: return OlOpen15;
    case 16: return OlOpen16;
    case 18: return OlOpen18;
    case 19: return OlOpen19;
    case 21: return OlOpen21;
    }
    return NULL;
}
#define OL_FIX_N 24
static DWORD g_olFixId[OL_FIX_N];
static int g_olFixUsed;
#define OL_FD(n) static int __cdecl OlFd##n(void) { return (int)g_olFixId[n]; }
OL_FD(0) OL_FD(1) OL_FD(2) OL_FD(3) OL_FD(4) OL_FD(5) OL_FD(6) OL_FD(7) OL_FD(8) OL_FD(9) OL_FD(10) OL_FD(11)
OL_FD(12) OL_FD(13) OL_FD(14) OL_FD(15) OL_FD(16) OL_FD(17) OL_FD(18) OL_FD(19) OL_FD(20) OL_FD(21) OL_FD(22) OL_FD(23)
static const void *const kOlFd[OL_FIX_N] = { OlFd0, OlFd1, OlFd2, OlFd3, OlFd4, OlFd5, OlFd6, OlFd7, OlFd8, OlFd9, OlFd10, OlFd11,
                                             OlFd12, OlFd13, OlFd14, OlFd15, OlFd16, OlFd17, OlFd18, OlFd19, OlFd20, OlFd21, OlFd22, OlFd23 };
static const void *OlFixDesc(const char *s)  // a description getter that answers the text s
{
    const DWORD id = (DWORD)Ol4Add(s);
    int i;
    for (i = 0; i < g_olFixUsed; ++i)
        if (g_olFixId[i] == id) return kOlFd[i];
    if (g_olFixUsed >= OL_FIX_N) return kOlFd[0];  // (cannot happen: 18 distinct lines)
    g_olFixId[g_olFixUsed] = id;
    return kOlFd[g_olFixUsed++];
}

// the camera's settings in Options (Controls > Camera / Steering): the pause Extra Menu > Camera's rows, its steps and words
static int g_olCamD[OL_CAM_N], g_olCamFixD[2], g_olCamSwD[2];
static DWORD g_olCamSwIds[4];  // Game, Shoulder, Eileen, Unavailable (the plan's order: the default first)
static int OlCamGrey(int r)
{
    if ((unsigned)r >= OL_CAM_N) return 0;
    if (r == OL_CAM_ENABLE) return 0;  // the switch: its origin grey (camera.inc CamRowGrey)
    return !CamOurs() || LkCamRowGrey(r) >= 0;
}
static int OlCamIn(int r)
{
    int enter, dir;
    if (OlCamGrey(r)) {  // inert: the buzzer on a press (as Room 302's grey rows)
        if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    dir = CamDir(&enter);
    if (dir) CamRowStep(r, enter ? 1 : dir, enter);  // Left / Right step (a range clamps at its ends); Enter steps up and wraps
    return 0;
}
static int OlCamGet(int r) { return (unsigned)g_camIdx[r] <= (unsigned)g_camN[r] ? g_camIdx[r] : 0; }
static int OlCamDesc(int r)
{
    if (!CamOurs()) return MSG_CAM0 + (g_camOwner == CAM_OWN_PE || g_camOwner == CAM_OWN_WAIT ? CAMT_ROWD_PE : CAMT_ROWD_NA);
    {
        const int g = LkCamRowGrey(r);  // the reason first ("Needs the Shoulder camera.", "Needs Wall Zoom on.")
        if (g >= 0) return MSG_V53 + LK_R0 + g;
    }
    if (r == OL_CAM_FIXED) return g_olCamFixD[OlCamGet(r) == 1];
    return g_olCamD[r];
}
#define OL_CAMROW(n) \
    static int __cdecl OlCamIn##n(BYTE *c, int k) { (void)c; (void)k; return OlCamIn(n); } \
    static int __cdecl OlCamGet##n(void) { return OlCamGet(n); } \
    static int __cdecl OlCamDesc##n(void) { return OlCamDesc(n); }
OL_CAMROW(1) OL_CAMROW(2) OL_CAMROW(3) OL_CAMROW(4) OL_CAMROW(5) OL_CAMROW(6) OL_CAMROW(7) OL_CAMROW(8) OL_CAMROW(9)
OL_CAMROW(10) OL_CAMROW(11)
static const void *const kOlCamIn[OL_CAM_N] = { NULL, OlCamIn1, OlCamIn2, OlCamIn3, OlCamIn4, OlCamIn5, OlCamIn6, OlCamIn7, OlCamIn8,
                                                OlCamIn9, OlCamIn10, OlCamIn11 };
static const void *const kOlCamGet[OL_CAM_N] = { NULL, OlCamGet1, OlCamGet2, OlCamGet3, OlCamGet4, OlCamGet5, OlCamGet6, OlCamGet7,
                                                 OlCamGet8, OlCamGet9, OlCamGet10, OlCamGet11 };
static const void *const kOlCamDesc[OL_CAM_N] = { NULL, OlCamDesc1, OlCamDesc2, OlCamDesc3, OlCamDesc4, OlCamDesc5, OlCamDesc6,
                                                  OlCamDesc7, OlCamDesc8, OlCamDesc9, OlCamDesc10, OlCamDesc11 };
static int __cdecl OlCamSwGet(void) { const int v = CamRow6Get(); return v == 0 ? 1 : v == 1 ? 0 : v; }  // CamRow6Get: 0 Shoulder, 1 Game
static int __cdecl OlCamSwDesc(void)  // a line per value (the settings are on this page now: no "Settings: pause > ..." part)
{
    if (CamOurs()) return g_olCamSwD[g_cam.on ? 1 : 0];
    return CamRow6Desc();
}

// ---------------------------------------------------------------- the sources (each record by its own input function)
static void OlSrcIn(int s, const void **a, const void **b)
{
    *a = *b = NULL;
    switch (s) {
    case OLS_RES: *a = (const void *)0x525030; break;
    case OLS_ASPECT: *a = AspectInput; break;
    case OLS_DISPMODE: *a = DispInput; break;
    case OLS_MONITOR: *a = MonInput; break;
    case OLS_FPS: *a = FpsInput; break;
    case OLS_BRIGHT: *a = (const void *)0x524900; break;
    case OLS_GAMMA: *a = (const void *)0x525100; break;
    case OLS_SNDMODE: *a = SndRowIn, *b = (const void *)0x524B60; break;
    case OLS_BGM: *a = (const void *)0x524C00; break;
    case OLS_SE: *a = (const void *)0x524CD0; break;
    case OLS_RENDERER: *a = RendIn; break;
    case OLS_BUTTONS: *a = (const void *)0x524940; break;
    case OLS_DEFSPEED: *a = (const void *)0x524EB0; break;
    case OLS_VIB: *a = VibIn, *b = (const void *)0x524980; break;
    case OLS_OMNI: *a = OmIn; break;
    case OLS_HEADBOB: *a = (const void *)0x524690; break;
    case OLS_LEFTRIGHT: *a = (const void *)0x525140; break;
    case OLS_INVLOOK: *a = (const void *)0x5251E0; break;
    case OLS_FOV: *a = FovInput; break;
    case OLS_FOVFP: *a = FovFpInput; break;
    case OLS_CAMSW: *a = CamRow6In; break;
    case OLS_R3_DOWN: *a = R3In0; break;
    case OLS_R3_UP: *a = R3In1; break;
    case OLS_R3_FEEL: *a = R3In2; break;
    case OLS_R3_WALK: *a = R3In3; break;
    case OLS_LANG: *a = (const void *)0x524A20; break;
    case OLS_SUBTITLES: *a = SubRowInput, *b = (const void *)0x524AF0; break;
    case OLS_HGAUGE: *a = (const void *)0x524760; break;
    case OLS_ICONS: *a = (const void *)0x524830; break;
    case OLS_GRAIN_SW: *a = FxRowInput, *b = (const void *)0x524E40; break;
    case OLS_SHADOW_SW: *a = ShRowInput, *b = (const void *)0x524F20; break;
    case OLS_BLOOD: *a = BldIn, *b = (const void *)0x524DA0; break;
    case OLS_ENEMYHP: *a = HbIn0; break;
    case OLS_CURSE: *a = HbIn1; break;
    case OLS_EM0: *a = EmIn0; break;
    case OLS_EM1: *a = EmIn1; break;
    case OLS_EM2: *a = EmIn2; break;
    case OLS_EM3: *a = EmIn3; break;
    case OLS_EM4: *a = EmIn4; break;
    case OLS_EM5: *a = EmIn5; break;
    case OLS_SAVEBANKS: *a = SbOnIn; break;
    case OLS_MENUFONT: *a = FnIn; break;
    case OLS_STARTUP: *a = OpIn; break;
    case OLS_SUB0: *a = SubIn0; break;
    case OLS_SUB1: *a = SubIn1; break;
    case OLS_SUB2: *a = SubIn2; break;
    case OLS_SUB3: *a = SubIn3; break;
    case OLS_SUB4: *a = SubIn4; break;
    case OLS_SUB5: *a = SubIn5; break;
    case OLS_WIDTH: *a = SndWidthIn; break;
    case OLS_REVERB: *a = SndRevIn; break;
    case OLS_NIGHT: *a = SndNightIn; break;
    case OLS_SNDBG: *a = SndBgIn; break;  // v3.56 SD356
    case OLS_FILTER: *a = FxIn3; break;
    case OLS_CUT: *a = FxIn4; break;
    case OLS_BLOOM: *a = PpIn0; break;
    case OLS_GRADE: *a = PpIn1; break;
    case OLS_LENS: *a = PpIn2; break;
    case OLS_DOF: *a = DofModeIn; break;
    case OLS_BLUR: *a = DofStrIn; break;
    case OLS_NSTR: *a = FxIn0; break;
    case OLS_NTYPE: *a = FxIn1; break;
    case OLS_NCOL: *a = FxIn2; break;
    case OLS_SHEDGE: *a = ShEdgeIn; break;
    case OLS_SHDARK: *a = ShDarkIn; break;
    case OLS_FSTR: *a = PvFsIn; break;  // v3.58 PV358
    case OLS_PREVIEW: *a = PvModeIn; break;  // v3.58 PV358
    case OLS_MOVE: *a = kGpIn[GP_MOVE]; break;
    case OLS_REGEN: *a = kGpIn[GP_REGEN]; break;
    case OLS_MED: *a = kGpIn[GP_MED]; break;
    case OLS_GRAB: *a = kPcIn[PC_GRAB]; break;
    case OLS_NOCLIP: *a = NcIn; break;
    case OLS_OHK: *a = kGpIn[GP_OHK]; break;
    case OLS_PACT: *a = kPcIn[PC_PACT]; break;
    case OLS_QC: *a = kPcIn[PC_QC]; break;
    case OLS_STEAL: *a = kPcIn[PC_STEAL]; break;
    case OLS_AMMO: *a = kPcIn[PC_AMMO]; break;
    case OLS_SHINE: *a = ItemInput; break;
    case OLS_TOOLBELT: *a = InvTbIn; break;
    case OLS_STACKS: *a = InvStIn; break;
    case OLS_DROPS: *a = kGpIn[GP_DROP]; break;
    case OLS_COPIES: *a = kGpIn[GP_COPIES]; break;
    case OLS_FOLLOW: *a = kGpIn[GP_FOLLOW]; break;
    case OLS_PURSUIT: *a = EfIn; break;
    case OLS_REVIVE: *a = kGpIn[GP_REVIVE]; break;
    case OLS_FADEIN: *a = FiIn; break;
    case OLS_DP: *a = kGpIn[GP_DP]; break;
    case OLS_AI: *a = kGpIn[GP_AI]; break;
    case OLS_ECHO: *a = kGpIn[GP_ECHO]; break;
    case OLS_ECHON: *a = kGpIn[GP_ECHON]; break;
    case OLS_GT: *a = GtIn; break;
    case OLS_RZ_ITEMS: *a = RzInItems; break;
    case OLS_RZ_KEEP: *a = RzInKeep; break;
    case OLS_RZ_R302: *a = RzInR302; break;
    case OLS_RZ_MON: *a = RmInMon; break;
    case OLS_RZ_GHOSTS: *a = RzlInGhosts; break;
    case OLS_RZ_OUTFIT: *a = RzlInOutfit; break;
    case OLS_RZ_PERVISIT: *a = RzInPerVisit; break;  // v3.56 RI356: Reroll Visits (OLS_RZ_KEEP = Specials); Seed / New Seed gone
    case OLS_LOCKWALK: *a = LwIn; break;  // v3.59 CT359: Lock-On Walk (omnilatch.inc)
    }
}
static int OlFindSrc(int s, int *op, int *orow)  // the origin record of source s (first match over the origin pages)
{
    const void *a, *b;
    int p, i;
    OlSrcIn(s, &a, &b);
    if (!a) return 0;
    for (p = 0; p < OP_MAX; ++p) {
        const DWORD *t = g_opTable[p];
        if (!t || g_opCount[p] <= 0 || g_opCount[p] > 40) continue;
        for (i = 0; i < g_opCount[p]; ++i)
            if (t[7 * i] == (DWORD)(ULONG_PTR)a || (b && t[7 * i] == (DWORD)(ULONG_PTR)b)) {
                *op = p, *orow = i;
                return 1;
            }
    }
    return 0;
}

// ---------------------------------------------------------------- building
static void OlMiss(int lp, const OlRowDef *d, const char *why)
{
    char s[96];
    ++g_olMissing;
    sprintf_s(s, sizeof s, "%s%s (%s)", g_olMissed[0] ? ", " : "", d->pn ? d->pn : "?", why);
    if (strlen(g_olMissed) + strlen(s) < sizeof g_olMissed - 4) strcat_s(g_olMissed, sizeof g_olMissed, s);
    LogV("options layout: page %d (%s): no %s row: %s", lp, kOlPage[lp].title, d->pn ? d->pn : "?", why);
}
static int OlBuildPage(int lp, const DWORD *stockExit)
{
    const OlPageDef *pd = &kOlPage[lp];
    DWORD *const t = g_olLive[lp];
    const OlRowDef *d;
    int n = 0, i;
    for (i = 0; (d = OlRowNth(pd->rows, g_olMoves, i)) != NULL && n < OL_ROWS - 1; ++i) {  // v3.55 OL56: the rows of this layout
        DWORD *const r = t + 7 * n;
        int kind = d->kind, op = -1, orow = -1, arg = d->arg;
        switch (kind) {
        case OLK_VALUE: case OLK_SCREEN: case OLK_COMBO: case OLK_CAMSW:
            if (!OlFindSrc(d->src, &op, &orow)) {
                OlMiss(lp, d, "not built this session");
                continue;
            }
            memcpy(r, g_opTable[op] + 7 * orow, 7 * 4);
            if (d->label && (d->src == OLS_BGM || d->src == OLS_SE) && !g_vbOn) {  // v3.55 OL56: no 0x525672 patch: the bar
                LogV("options layout: page %d: %s keeps its stock label (the bar is drawn by label id without the 0x525672 patch)",
                     lp, d->pn);
            } else if (d->label) {
                r[5] = (DWORD)Ol4Add(d->label);
            }
            if (d->desc) r[6] = (DWORD)(ULONG_PTR)OlFixDesc(d->desc);
            if (d->src == OLS_GRAIN_SW && r[0] == (DWORD)(ULONG_PTR)FxRowInput) r[0] = g_fxStockIn;  // a plain switch (R5 moves it)
            if (d->src == OLS_SHADOW_SW && r[0] == (DWORD)(ULONG_PTR)ShRowInput) r[0] = g_shStockIn;
            if (kind == OLK_VALUE && d->src == OLS_SUBTITLES) {  // v3.55 OL56 (R5): Interface > Subtitles' row 0, the stock switch
                r[0] = 0x524AF0;                               // (SubRowInput's Enter opened the page it is on now)
                r[6] = (DWORD)(ULONG_PTR)OlSubDesc;
            }
            if (kind == OLK_COMBO && (r[0] != (DWORD)(ULONG_PTR)SubRowInput || !g_subScreenOn || !g_lpCount[arg])) {
                kind = OLK_VALUE, arg = 0;  // no Subtitles page: the stock switch with its own line
                r[0] = 0x524AF0;
                r[6] = (DWORD)(ULONG_PTR)OlSubDesc;
            }
            if (kind == OLK_CAMSW) {
                r[1] = (DWORD)(ULONG_PTR)OlCamSwGet;
                r[3] = (DWORD)(ULONG_PTR)g_olCamSwIds;
                r[4] = 4;
                r[6] = (DWORD)(ULONG_PTR)OlCamSwDesc;
            }
            break;
        case OLK_FOLDER:
            if (!g_subScreenOn || (unsigned)arg >= LP_MAX || g_lpCount[arg] <= 0 || !OlOpenFn(arg)) {
                OlMiss(lp, d, g_subScreenOn ? "its page has no row" : "no sub-screen fade");
                continue;
            }
            FillRow(r, OlOpenFn(arg), (const void *)0x51EB10, (const DWORD *)0x654C54, 1, Ol4Add(d->label),
                    d->desc ? OlFixDesc(d->desc) : (const void *)R3DescFolder);  // (a copy of Button Config's shape: value-less, "Press")
            ++g_olFolders;
            break;
        case OLK_CAM:
            if (arg <= OL_CAM_ENABLE || arg >= OL_CAM_N || g_camN[arg] <= 0) {
                OlMiss(lp, d, "no camera texts");
                continue;
            }
            g_olCamD[arg] = Ol4Add(d->desc);
            FillRow(r, kOlCamIn[arg], kOlCamGet[arg], g_camIds[arg], g_camN[arg] + (g_camCust[arg] >= 0 ? 1 : 0), Ol4Add(d->label),
                    kOlCamDesc[arg]);
            break;
        case OLK_BACK:
            memcpy(r, stockExit, 7 * 4);  // Exit's record with Back's input / label / line (as gameplay.inc GpBackRow)
            r[0] = (DWORD)(ULONG_PTR)GpBackIn;
            r[5] = (DWORD)Ol4Add(d->label);
            r[6] = (DWORD)(ULONG_PTR)OlFixDesc(d->desc);
            break;
        case OLK_EXIT:
            memcpy(r, lp < 3 && g_opTable[lp] && g_opCount[lp] > 0 ? g_opTable[lp] + 7 * (g_opCount[lp] - 1) : stockExit, 7 * 4);
            break;
        default:
            continue;
        }
        VlWrapRow(r, kind, d);  // v3.56 MN356: the plan's value line where the row's code answers the row's line
        r[2] = 0;
        g_olOp[lp][n] = (signed char)op, g_olOr[lp][n] = (signed char)orow;
        g_olKind[lp][n] = (BYTE)kind, g_olArg[lp][n] = (BYTE)arg;
        if (kind != OLK_BACK && kind != OLK_EXIT) ++g_olPlaced;
        ++n;
    }
    if (lp >= LP_N && n <= 1) n = 0;  // a folder with Back alone: no page (its folder row is left out)
    t[7 * n] = 0;
    g_lpTable[lp] = n ? t : NULL;
    g_lpCount[lp] = n;
    g_lpTitle[lp] = lp == 0 ? MSG_V53 + OLT_T_DISPLAY : lp == 1 ? MSG_V53 + OLT_T_CONTROLS : lp == 2 ? MSG_V53 + OLT_T_INTERFACE
                                                     : (DWORD)Ol4Add(OlTitle(lp, g_olMoves));  // v3.55 OL56: Pause Menu / Menus
    return n;
}
// v3.55 OL56 (R0 R5-1): the volume bars by record. 0x525672 `mov eax,[ecx+14h]; cmp eax,25h` (6 bytes) -> `call VbLabelCmp;
// nop` (volbar_sim.h): BGM / SE Volume's inputs draw as bars whatever their label, every other record as the game decides.
// Byte-checked against the stock exe, listed in SH4WM_Patches (Poke), rolled back if the write does not read back; with
// [Options] StockMoves=0 never written. Without it the two rows keep their stock labels (BGM / SE Volume) and still draw.
static void VbInstall(void)
{
    static const Site kSite = { VB_SITE, { 0x8B, 0x41, 0x14, 0x83, 0xF8, 0x25 }, VB_LEN };
    BYTE want[VB_LEN];
    const BYTE *const at = (const BYTE *)(ULONG_PTR)VB_SITE;
    if (memcmp(kSite.stock, kVbStock, VB_LEN)) return;  // (one list of the stock bytes: volbar_sim.h)
    VbPatchBytes(VB_SITE, (unsigned long)(ULONG_PTR)VbLabelCmp, want);
    if (!memcmp(at, want, VB_LEN)) {  // (already ours: a second build in this session)
        g_vbOn = 1;
        return;
    }
    if (!SiteOk(&kSite)) {
        Log("options layout: 0x525672 is %02X %02X %02X %02X %02X %02X, not the stock 8B 41 14 83 F8 25 (another mod?): the volume bars "
            "stay by label id; Music / Effects Volume keep the names BGM / SE Volume", at[0], at[1], at[2], at[3], at[4], at[5]);
        return;
    }
    if (Poke(VB_SITE, want, VB_LEN) && !memcmp(at, want, VB_LEN)) {
        g_vbOn = 1;
        Log("options layout: the volume bars by record (0x525672, 6 bytes: a call to the stub; BGM / SE Volume's inputs 0x524C00 / "
            "0x524CD0): Music Volume and Effects Volume have the plan's names");
        return;
    }
    if (memcmp(at, kSite.stock, VB_LEN)) Poke(VB_SITE, kSite.stock, VB_LEN);  // roll back whatever was written
    LogAnomaly("options", "layout: writing 0x525672 failed (%s): rolled back; Music / Effects Volume keep the names BGM / SE Volume",
               memcmp(at, kSite.stock, VB_LEN) ? "the stock bytes did not come back either" : "the stock bytes are back");
}
// ApplyAspectMenu, after every install that reads or changes the origin pages (OlStockRows last)
static void OlLayoutBuild(void)
{
    static const DWORD kExitAt = 0x654CC0 + 6 * 7 * 4;  // Option 1's stock Exit record (row 6)
    int i, lp, over = 0, pages = 0;
    if (!g_pagesOn) {
        Log("options layout: no Options pages (see above): the game's own three pages");
        return;
    }
    {   // v3.55 OL56: the layout (the hidden [Options] StockMoves; the folders that hold the stock switches need the fade route)
        const int ini = GetPrivateProfileIntA("Options", "StockMoves", 1, g_iniPath) != 0;
        g_olMoves = ini && g_subScreenOn;
        g_olMovesWhy = g_olMoves ? "" : !ini ? " ([Options] StockMoves=0: v3.54's places)" : " (no sub-screen fade: v3.54's places)";
        if (g_olMoves) VbInstall();
    }
    g_olCamSwIds[0] = MSG_CAM0 + CAMT_ROWV0 + 1, g_olCamSwIds[1] = MSG_CAM0 + CAMT_ROWV0 + 0;  // Game, Shoulder
    g_olCamSwIds[2] = MSG_CAM0 + CAMT_ROWV0 + 2, g_olCamSwIds[3] = MSG_CAM0 + CAMT_ROWV0 + 3;
    g_olCamSwD[0] = Ol4Add(OLW_CAMERA_CAMERA_VD_GAME), g_olCamSwD[1] = Ol4Add(OLW_CAMERA_CAMERA_VD_SHOULDER);
    g_olCamFixD[0] = Ol4Add(OLW_CAMERA_FIXED_ANGLES_VD_OFF), g_olCamFixD[1] = Ol4Add(OLW_CAMERA_FIXED_ANGLES_VD_ON);
    for (lp = 0; lp < LP_MAX; ++lp) {
        g_lpTable[lp] = NULL, g_lpCount[lp] = 0, g_lpTitle[lp] = 0;
        g_lpParent[lp] = -1, g_lpParentRow[lp] = -1;
        memset(g_olOp[lp], -1, sizeof g_olOp[lp]);
        memset(g_olOr[lp], -1, sizeof g_olOr[lp]);
    }
    for (i = 0; i < OL_PAGES; ++i) OlBuildPage(kOlBuildOrder[i], (const DWORD *)(ULONG_PTR)kExitAt);  // v3.58 PV358: OL_PAGES (was LP_MAX - 3)
    for (lp = LP_N; lp < LP_MAX; ++lp) {  // the tree: a page's parent and the row that opens it (Back lands there)
        const int par = kOlPage[lp].parent;
        int r;
        if (!g_lpCount[lp] || par < 0) continue;
        for (r = 0; r < g_lpCount[par] && r < OL_ROWS; ++r)
            if (OlOpensKind(g_olKind[par][r]) && g_olArg[par][r] == lp) {
                g_lpParent[lp] = par, g_lpParentRow[lp] = r;
                break;
            }
        if (g_lpParent[lp] < 0) {  // (cannot happen: the folder row is built from this page)
            LogAnomaly("options", "layout: page %d (%s) has rows but no row of page %d opens it", lp, kOlPage[lp].title, par);
            g_lpCount[lp] = 0, g_lpTable[lp] = NULL;
        }
    }
    for (lp = 0; lp < LP_MAX; ++lp) {
        if (g_lpCount[lp] > OL_MAX_ROWS) ++over;
        if (g_lpCount[lp]) ++pages;
    }
    g_olOn = 1;
    VlBuilt();  // v3.56 MN356
    SetLogicalPage(0);  // (g_stockView 7 from here on: R0 R4-1)
    {   // shadows.inc: its page is in with its own rows (its changed / preview tests ask)  // v3.55 OL56: + its own rows (the
        int r, own = 0;                                                                     // switch alone is not its page)
        for (r = 0; r < g_lpCount[LP_SHADOWS] && r < OL_ROWS; ++r) own |= g_olOp[LP_SHADOWS][r] == OP_SHD;
        if (own && g_lpParent[LP_SHADOWS] >= 0) g_shMenu = 1;
    }
    Log("options layout: R5 + R6 (OL56), stock moves %s%s: the cycle Display, Controls, Interface, Sound, Effects, Gameplay; %d pages "  // v3.55 OL56
        "(folders: Subtitles, %s, Film Grain, Depth of Field, Shadows, Player, Camera, Room 302, Combat, Enemies, Items, Powers, Randomizer, Steering); "  // v3.58 PV358: + Depth of Field
        "%d rows placed, %d folder rows, %d left out%s%s; %d of %d texts%s",
        g_olMoves ? "on" : "off", g_olMovesWhy, pages, OlTitle(LP_MENUS, g_olMoves), g_olPlaced, g_olFolders, g_olMissing,
        g_olMissing ? ": " : "", g_olMissed, g_ol4Used, OL4_N, over || g_ol4Full ? " (OVER A LIMIT)" : "");
    if (over || g_ol4Full) LogAnomaly("options", "layout: %d page(s) over %d rows, %d text(s) over the block", over, OL_MAX_ROWS, g_ol4Full);
}

// ---------------------------------------------------------------- the line at every Options opening (ResetPages, verbose)
static void OlOpenLog(void)
{
    static LONG s_q;
    char s[1400];
    int lp, n = 0;
    if (!g_olOn || !LogQuota(&s_q, 200)) return;
    s[0] = 0;
    for (lp = 0; lp < LP_MAX && n < (int)sizeof s - 80; ++lp) {
        if (!g_lpCount[lp]) continue;
        if (lp < LP_N)
            n += sprintf_s(s + n, sizeof s - n, "%s%d %s %d", n ? ", " : "", lp, kOlPage[lp].title, g_lpCount[lp]);
        else  // v3.55 OL56: the titles of this layout (Pause Menu / Menus)
            n += sprintf_s(s + n, sizeof s - n, ", %d %s > %s %d (from %d row %d)", lp, OlTitle(g_lpParent[lp] >= 0 ? g_lpParent[lp] : 0, g_olMoves),
                           OlTitle(lp, g_olMoves), g_lpCount[lp], g_lpParent[lp], g_lpParentRow[lp]);
    }
    LogV("options layout: open: the cycle %s, %s, %s, %s, %s, %s; pages: %s", kOlPage[0].title, kOlPage[1].title, kOlPage[2].title,
         kOlPage[3].title, kOlPage[4].title, kOlPage[5].title, s);
}
