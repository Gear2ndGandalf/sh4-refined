// ================================================================ v3.56 MN356: a mouse click opens a folder row
// USER_FEEDBACK_v355 issue 1: "Sometimes, submenus cannot be accessed via mouse click on the blue right arrow. Only the
// Enter key works all the time." The causes and the rule: folderclick_sim.h (test\folderclick_test.c). Three parts, no exe
// site of its own:
//   - FcClickHit: Refined's hook of the game's int rect test 0x405690 (HitRectI, Menus43) answers the page input's first
//     test of a frame (the cursor row's value column, return address 0x525E05) itself when the left button went down this
//     frame on a folder-kind row (folder, Brightness / Gamma / Buttons, Back; a combo row on its label): the cursor moves
//     to that row as the stock hover moves it (0x552C00 once a row, [0x1090FF4]), the answer is "on the row", and the page
//     input calls that row's own input (0x5260FF) as for a click on its value column. Off a folder-kind row: the stock test.
//   - FcClickOk: the folder inputs (GpOpen, GpBackIn, the combo SubRowInput) take the click too (the stock screen rows'
//     own test, 0x405490), refused while a slide, a fade or a page request is pending (logged).
//   - FcFrameNote: a click that lands while a page slides or fades (the page input does not run then) is logged once.
// Needs the Menus43 mouse hooks ([Window] Menus43=1, the default) for the first part; without them a click on the cursor
// row's value column or triangle still opens it (the second part). Hidden [Options] FolderClick=0: v3.55's behaviour.
#include "folderclick_sim.h"

static int g_fcOn = -1;              // [Options] FolderClick (read at the first use)
static int g_fcOpenRow = -1;         // the row the click hook opened this frame (taken by the combo input)
static DWORD g_fcOpenTick;
static DWORD g_fcNoteTick;           // the last mid-slide click logged (one line a click)
static volatile LONG g_fcQ, g_fcQRefuse, g_fcQFault, g_fcN, g_fcRefused;

static int FcOn(void)
{
    if (g_fcOn < 0) {
        g_fcOn = GetPrivateProfileIntA("Options", "FolderClick", 1, g_iniPath) != 0;
        Log("options: folder rows open on a mouse click %s ([Options] FolderClick=%d%s)", g_fcOn ? "anywhere on the row (label, "
            "triangle, value column; a combo row on its label)" : "only as in v3.55 (Enter)", g_fcOn,
            g_hitITramp ? "" : "; no Menus43 mouse hooks: on the cursor row's value column only");
    }
    return g_fcOn;
}

// a slide, a fade or a page request is pending: a click now is refused (the next one works)
static const char *FcBusy(void)
{
    if (g_pending >= 0) return "a page slide is under way";
    if (g_subReq) return "a page change is pending (the fade)";
    if (g_lkStripGo >= 0) return "a page strip click waits";
    return NULL;
}
static void FcRefuseLog(const char *why, int row)
{
    const DWORD now = GetTickCount();
    InterlockedIncrement(&g_fcRefused);
    if (now - g_fcNoteTick < 40) return;  // the same click (the hook and the row input in one frame)
    g_fcNoteTick = now;
    if (LogQuota(&g_fcQRefuse, 40))
        LogV("options: a click on page %d row %d not taken: %s (click again)", g_lpage, row, why);
}

// the click class of a live row (folderclick_sim.h FC_C_*)
static int FcClass(int lp, int row)
{
    int op, orow, arg;
    const int kind = OlRowAt(lp, row, &op, &orow, &arg);
    if (kind == OLK_FOLDER || kind == OLK_SCREEN || kind == OLK_BACK) return FC_C_ROW;
    if (kind == OLK_COMBO) return FC_C_LABEL;
    return FC_C_NONE;
}

// HitRectI, at 0x525E05 (the cursor row's value-column test). -1 = the stock test; 0 / 1 = the answer.
static int FcClickHit(void)
{
    BYTE *const obj = (BYTE *)(ULONG_PTR)0x1091A20;
    int slot, lp, n, i, hit = -1, cur, d;
    int inside[OL_ROWS], open[OL_ROWS];
    if (!g_olOn || !FcOn() || !((Fn0)0x405490)()) return -1;
    __try {
        slot = *(volatile int *)0x1091B1C;
        if ((unsigned)slot >= 3) return -1;
        lp = g_slotPage[slot];
        if ((unsigned)lp >= LP_MAX || lp != g_lpage) return -1;
        n = g_lpCount[lp];
        if (n <= 0 || n > OL_ROWS) return -1;
        for (i = 0; i < n; ++i) {
            RowRect r;
            int box[4];
            inside[i] = open[i] = 0;
            memset(&r, 0, sizeof r);
            ((void *(__cdecl *)(RowRect *, const void *, int))0x552270)(&r, obj + 0x38 * slot, i);
            if (FcBox(FcClass(lp, i), r.x, r.y, r.h, box)) open[i] = HitRectI(box);
            inside[i] = open[i] || (FcHoverBox(r.x, r.y, r.h, box) && HitRectI(box));  // (the row the stock hover finds)
        }
        hit = FcPickByHit(inside, n);
        switch (FcDecide(1, hit, hit >= 0 && open[hit], FcBusy() != NULL)) {
        case FC_REFUSE:
            FcRefuseLog(FcBusy(), hit);
            return 0;
        case FC_OPEN:
            break;
        default:
            return -1;
        }
        cur = ((int(__cdecl *)(void *))0x552470)(obj + 0x38 * slot);
        cur = (short)cur;
        for (d = hit - cur; d > 0; --d) ((void(__cdecl *)(void *, int))0x552C00)(obj + 0x38 * slot, 1);  // as the stock hover (0x525FD0)
        for (d = cur - hit; d > 0; --d) ((void(__cdecl *)(void *, int))0x552C00)(obj + 0x38 * slot, 0);  // (0x526038)
        *(int *)(obj + 0x38 * slot + 0x30) = hit;  // 0x5260FF reads the row from here this frame
        *(volatile int *)0x1090FF4 = hit;           // the cursor after a move (0x525FF1)
        g_fcOpenRow = FcClass(lp, hit) == FC_C_LABEL ? hit : -1;  // (only a combo's input asks for it)
        g_fcOpenTick = GetTickCount();
        InterlockedIncrement(&g_fcN);
        if (LogQuota(&g_fcQ, 60))
            LogV("options: a click on page %d (%s) row %d (%s%s): its input runs as for a click on its value column", lp,
                 kOlPage[lp].title, hit, FcClass(lp, hit) == FC_C_LABEL ? "a combo row's label" : "a folder-kind row",
                 hit != cur ? ", the cursor moved there" : "");
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (LogQuota(&g_fcQFault, 3)) LogAnomaly("options", "the folder click test faulted: the stock test");
        return -1;
    }
}

// the folder inputs: 1 = open (Enter / the pad's confirm, or a left click on the row); a refused click is logged
static int FcClickOk(void)
{
    const char *why;
    const int key = ((Fn2)0x4F84A0)(0, 0x11) != 0;
    const int click = !key && FcOn() && ((Fn0)0x405490)() != 0;
    if (!key && !click) return 0;
    if ((why = FcBusy()) != NULL) {
        FcRefuseLog(why, -1);
        return 0;
    }
    return 1;
}
// the combo row's input (SubRowInput): opens on Enter, or on the click the hook placed on its label this frame
static int FcComboOk(void)
{
    const int mine = g_fcOpenRow >= 0 && GetTickCount() - g_fcOpenTick < 200 && ((Fn0)0x405490)();
    g_fcOpenRow = -1;
    if (!((Fn2)0x4F84A0)(0, 0x11) && !mine) return 0;
    return FcBusy() == NULL;
}

// RowChangedDraw, row 0 of each slot drawn: a click while the page slides or fades (the page input waits) is logged once
static void FcFrameNote(int slot)
{
    const char *why;
    if (g_fcOn < 0 && g_olOn) FcOn();  // (the ini read and its line at the first Options frame)
    if (!g_olOn || g_fcOn <= 0 || (unsigned)slot >= 3) return;
    if (!(why = FcBusy())) return;
    if (!((Fn0)0x405490)()) return;
    FcRefuseLog(why, -1);
}
