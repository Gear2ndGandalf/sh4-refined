// ================================================================ v3.39 PR339: Option 4's Renderer and Opening rows
// USER_FEEDBACK_v3381 (items 1, 2, 4 and the host's questions): Option 4 (Presentation) lost V-Sync, Anti-Aliasing,
// Anisotropic Filtering (GOG's DirectX Settings tool, dxcfg.exe, sets them in dxcfg.ini) and Performance (hidden key
// [Graphics] PerfOverlay=1), and gained, above Exit:
//   * Renderer: SH4 Graphics Plus's dgVoodoo2 configs (RENDERER_SPEC.md, binding). The choices are the manifest's offered
//     rows (Direct3D 11 / Direct3D 11 (16-bit) / Direct3D 12 / Stock as shipped); dgVoodoo.conf's state is read at every
//     Options opening (renderer.inc RdLoad: manifest, d3d9.dll, dgVoodoo.conf and the variant files, once, not per frame).
//     Greyed: no / a broken manifest, no dgVoodoo2 2.87.6 d3d9.dll, no offered file ("Unavailable"), a conf that is no
//     manifest row's ("Custom"). A recognised-only conf shows "Graphics Plus: <name>" and can be replaced. A change is
//     made when Options are left with "Yes" (SetResIndex, like the restart rows before it), by renderer.inc RdApply (every
//     check again, dgVoodoo.conf.new, md5, MoveFileEx); it applies at the next start (dgVoodoo reads its conf at load).
//   * Opening: Full (the game's own) / Skip Logos (the default) / Fast, [Window] Opening, read at start: the boot screens
//     run once, before any menu. Skip Logos redirects the call at 0x42CD6A (mode 1's `call 0x519160`, the KONAMI / KCET
//     logos and the notice) to OpeningBootC, which does what their end does (ScreenSetGamma 0x5212E0 with the options
//     gamma [0x1091AEC], the fade 0x565CA0(128), mode 1's next step 0x555C80) at once. Fast also turns the first-start
//     movie branch 0x42CDA2 `74 12` into nops (straight to the title even without system data) and passes the title's
//     short intro (0x42CDAB `6A 01` -> `6A 04`: 0x527A50(4), TitleInit 0x52A728 picks the table 0x6551E0). Byte-checked,
//     all or nothing, listed in SH4WM_Patches; Full patches nothing. No cheat-table site (ct_conflicts.py --span) and no
//     Playable Eileen hook is in 0x42CD65..0x42CDB4. RESEARCH.md A.
// Texts: the freed V-Sync / Anti-Aliasing / Anisotropic ids (MSG_GFX0 .. MSG_SUBT - 1, 24 ids; the enum is not renumbered).
#include "opening_sim.h"

enum { PRT_RLABEL, PRT_RDESC0, PRT_RVAL0 = PRT_RDESC0 + RD_DK_N, PRT_OLABEL = PRT_RVAL0 + RD_VALS, PRT_ODESC0,
       PRT_OVAL0 = PRT_ODESC0 + OP_N, PRT_N = PRT_OVAL0 + OP_N };
typedef char PresTextsFit[PRT_N <= MSG_SUBT - MSG_GFX0 ? 1 : -1];  // the freed V-Sync / AA / AF block holds them
static BYTE g_txtPres[PRT_N][112];
static DWORD g_rendIds[RD_VALS], g_openIds[OP_N];
static RdInfo g_rd;                 // the state the row shows (start, then every Options opening)
static int g_rdRead;
static char g_rdBootVariant[48];    // the variant dgVoodoo loaded at this start ("" = not a manifest one)
static volatile LONG g_rendSel;     // the value the row shows
static LONG g_rendSnap;             // its value as Options opened (= dgVoodoo.conf then)
static int g_presRend = -1, g_presOpen = -1;  // their Option 4 rows
static DWORD *g_rendRow;            // the Renderer row's record (its value count follows the state)
static int g_openIni = OP_DEFAULT;  // [Window] Opening at this start (what runs now)
static volatile LONG g_openSel;     // the row
static LONG g_openSnap;
static unsigned g_openSites;        // the patches in place (OpSites bits)
static DWORD g_openT0;              // GetTickCount at the install (the log's "after N ms")
static volatile LONG g_openFired;

// v3.53 OL1: the plan's one-line words (the 16-bit line no longer loses its end: it was 110 characters in 108)
static const char *const kRdDescTxt[RD_DK_N] = {
    OLW_DISPLAY_RENDERER_VD_DIRECT3D_11, OLW_REND_16BIT, OLW_DISPLAY_RENDERER_VD_DIRECT3D_12, OLW_DISPLAY_RENDERER_VD_STOCK,
    OLW_REND_OTHER, OLW_REND_TEST, OLW_DISPLAY_RENDERER_VD_CUSTOM, OLW_DISPLAY_RENDERER_VD_UNAVAILABLE,
};
static const char *const kOpDescTxt[OP_N] = {  // v3.53 OL1: Opening -> Start-Up
    OLW_INTERFACE_START_UP_VD_FULL, OLW_INTERFACE_START_UP_VD_SKIP_LOGOS, OLW_INTERFACE_START_UP_VD_FAST,
};
static const char *const kOpShown[OP_N] = { OLW_INTERFACE_START_UP_V_FULL, OLW_INTERFACE_START_UP_V_SKIP_LOGOS,
                                            OLW_INTERFACE_START_UP_V_FAST };  // v3.53 OL1 (the ini words stay kOpName: the same words)
static const char *PerfBackendLine(void);  // v3.53 OL1: perfhud.inc: the running backend ("" while not known)

static const BYTE *PresText(int id)
{
    const int i = id - MSG_GFX0;
    return g_txtPres[i >= 0 && i < PRT_N ? i : PRT_RLABEL];
}

// ---------------------------------------------------------------- Renderer
static void RendValues(void)  // the value texts and the row's count from g_rd
{
    int v;
    for (v = 0; v < RD_VALS; ++v) {
        char s[48];
        RdValueText(&g_rd.m, &g_rd.s, v < g_rd.s.nVal ? v : 0, s, sizeof s);
        Encode(g_txtPres[PRT_RVAL0 + v], sizeof g_txtPres[0], s);
    }
    if (g_rendRow) g_rendRow[4] = (DWORD)(g_rd.s.nVal > 0 ? g_rd.s.nVal : 1);
    g_rendSel = g_rendSnap = g_rd.s.cur;
}
// Init (DllMain): the variant dgVoodoo will load at this start, for the log and the Direct3D line (perfhud.inc)
static void RendBootRead(void)
{
    char nm[48];
    int i, nKnown = 0;
    RdLoad(g_dir, RD_D3D9_PIN, &g_rd);
    g_rdRead = 1;
    g_rdBootVariant[0] = 0;
    if (g_rd.d3d9Ok && RdVariantName(&g_rd.m, g_rd.haveConf ? g_rd.conf : NULL, nm, sizeof nm))  // v3.39 REV339: only with an allowed d3d9.dll
        strcpy_s(g_rdBootVariant, sizeof g_rdBootVariant, nm);
    for (i = 0; i < g_rd.m.n; ++i) nKnown += !g_rd.m.row[i].offered;
    Log("renderer: %s; row %s (%s); manifest %s (%d offered, %d recognised only, %d d3d9.dll pin(s)%s); d3d9.dll %s%s", g_rd.why,
        g_rd.s.greyed ? "greyed" : "open", kRdStName[g_rd.s.st], g_rd.m.ok ? "read" : "not usable", g_rd.m.nOffered, nKnown,
        g_rd.m.nPin, g_rd.m.nPin ? "" : ": the built-in 2.87.6 one", g_rd.haveD3d9 ? g_rd.d3d9 : "missing",
        g_rd.d3d9Ok ? " (allowed)" : "");  // v3.39 REV339: the manifest's #d3d9.dll= pins
    if (g_rd.haveNew) {  // v3.39 REV339 (Graphics Plus's suggestion): a stale .new from a replace cut short is deleted
        DWORD err;
        const int r = RdDropStaleNew(g_dir, &g_rd, &err);  // v3.39 REV339: after RdLoad's read (manifest, d3d9 pin)
        if (r == 1) Log("renderer: a dgVoodoo.conf.new was left by a replace cut short: deleted (dgVoodoo.conf as it is: %s)", g_rd.why);
        else if (r == -1) Log("renderer: a stale dgVoodoo.conf.new could not be deleted (error %lu): the next replace writes over it", err);
        else if (r == 2) {  // v3.39 REV339 (Graphics Plus): the half-done switch finished; read the files again
            LogAnomaly("renderer", "dgVoodoo.conf was missing and Refined's dgVoodoo.conf.new was there: renamed to dgVoodoo.conf "
                       "(a replace cut short); it applies at the next start");
            RdLoad(g_dir, RD_D3D9_PIN, &g_rd);
            if (g_rd.d3d9Ok && RdVariantName(&g_rd.m, g_rd.haveConf ? g_rd.conf : NULL, nm, sizeof nm))
                strcpy_s(g_rdBootVariant, sizeof g_rdBootVariant, nm);
        } else if (r == 3)
            LogAnomaly("renderer", "dgVoodoo.conf is missing and a dgVoodoo.conf.new was there that is not a verified switch "
                       "(%s): deleted, no dgVoodoo.conf created", g_rd.d3d9Ok ? "not an offered manifest file" : "d3d9.dll is not "
                       "Graphics Plus's dgVoodoo");
        else if (r == -3)
            LogAnomaly("renderer", "an unverified dgVoodoo.conf.new could not be deleted (error %lu): left as is", err);
        else if (r == -2)
            LogAnomaly("renderer", "dgVoodoo.conf is missing and dgVoodoo.conf.new could not be renamed to it (error %lu): rename it "
                       "by hand, or dgVoodoo runs on its defaults", err);
        g_rd.haveNew = 0;
    }
    if (RdBootRetire(g_dir, RD_D3D9_PIN, &g_rd) == 2 && g_rd.d3d9Ok &&  // v3.44 FN344: Direct3D 11 (16-bit) reads as Direct3D 11
        RdVariantName(&g_rd.m, g_rd.haveConf ? g_rd.conf : NULL, nm, sizeof nm))
        strcpy_s(g_rdBootVariant, sizeof g_rdBootVariant, nm);
    if (g_rd.m.ok && g_rd.m.nOffered > RD_CHOICE_MAX) LogV("renderer: the manifest offers %d rows: the first %d are the choices",
                                                          g_rd.m.nOffered, RD_CHOICE_MAX);
}
// ResetPages (every Options opening): the files again, so the row shows dgVoodoo.conf as it is now
static void RendSnapshot(void)
{
    static volatile LONG s_q;
    char vals[RD_VALS * 40];
    int v;
    __try {
        RdLoad(g_dir, RD_D3D9_PIN, &g_rd);
        g_rdRead = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(&g_rd, 0, sizeof g_rd);
        RdDecide(&g_rd.m, 0, NULL, NULL, &g_rd.s);  // (an empty manifest: Unavailable)
        LogAnomaly("renderer", "reading the renderer files faulted (code %08lX): the row is greyed", GetExceptionCode());
    }
    RendValues();
    vals[0] = 0;
    for (v = 0; v < g_rd.s.nVal; ++v) {
        char s[48];
        RdValueText(&g_rd.m, &g_rd.s, v, s, sizeof s);
        strcat_s(vals, sizeof vals, v ? " | " : "");
        strcat_s(vals, sizeof vals, s);
    }
    if (LogQuota(&s_q, 40))
        LogV("renderer: Options opened: %s; the row %s, value %d of %d (%s)", g_rd.why, g_rd.s.greyed ? "greyed" : "open",
             g_rd.s.cur + 1, g_rd.s.nVal, vals);
}
static int __cdecl RendGet(void) { return (int)g_rendSel; }
// v3.53 OL1: the value's line, then the running backend (PLAN: it was Display Mode's second line); composed on a change only
static int __cdecl RendDesc(void)
{
    static int s_kind = -1;
    static char s_back[96];
    const int k = RdDescKind(&g_rd.m, &g_rd.s, (int)g_rendSel);
    const char *const b = PerfBackendLine();
    if (!b[0] || (unsigned)k >= RD_DK_N) return MSG_GFX0 + PRT_RDESC0 + k;
    if (k != s_kind || strcmp(b, s_back)) {
        char t[192];
        sprintf_s(t, sizeof t, "%s %s.", kRdDescTxt[k], b);
        Encode(g_txtOlRend, sizeof g_txtOlRend, t);
        s_kind = k;
        strcpy_s(s_back, sizeof s_back, b);
    }
    return MSG_V53 + OLT_REND;
}
static int __cdecl RendIn(BYTE *cfg, int count)
{
    int d;
    (void)cfg;
    (void)count;
    if (!g_rdRead || g_rd.s.greyed) return 0;  // greyed: no step, no sound
    d = StepValue(0, 3);                       // the stock tests and sound; 1 = right, 2 = left
    if (d < 0) return 0;
    InterlockedExchange(&g_rendSel, RdStep(&g_rd.s, (int)g_rendSel, d == 1 ? 1 : -1));
    return 0;
}
static void RendApply(void)
{
    char msg[512], name[RD_NAME_MAX + 1], label[RD_LABEL_MAX + 1];
    const int t = RdTargetRow(&g_rd.s, (int)g_rendSel);
    int r;
    if (g_rendSel == g_rendSnap) return;
    if (t < 0) {  // the state value again (dgVoodoo.conf as it is)
        LogV("renderer: kept as it is (%s)", g_rd.why);
    } else {
        strcpy_s(name, sizeof name, g_rd.m.row[t].name);
        strcpy_s(label, sizeof label, g_rd.m.row[t].label);
        __try {
            r = RdApply(g_dir, RD_D3D9_PIN, name, msg, sizeof msg);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            r = 0;
            sprintf_s(msg, sizeof msg, "not replaced: the file side faulted (code %08lX)", GetExceptionCode());
            LogAnomaly("renderer", "RdApply faulted (code %08lX)", GetExceptionCode());
        }
        if (r == 1) Log("renderer: %s; it applies at the next start (dgVoodoo reads dgVoodoo.conf when the game starts; running now: %s)",
                        msg, g_rdBootVariant[0] ? g_rdBootVariant : "not a Graphics Plus variant");
        else if (r == 2) LogV("renderer: %s", msg);
        else Log("renderer: %s (%s): %s; dgVoodoo.conf left as it was", label, name, msg);
    }
    __try {  // the row shows the files as they are now
        RdLoad(g_dir, RD_D3D9_PIN, &g_rd);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(&g_rd, 0, sizeof g_rd);
        RdDecide(&g_rd.m, 0, NULL, NULL, &g_rd.s);
    }
    RendValues();
}

// ---------------------------------------------------------------- Opening
// 0x42CD6A (was `call 0x519160`, mode 1 "boot screens", from mode 1's frame: step 0 falls through into it, step 1 calls
// it): the end of 0x519160's last picture at once. Mode 1 then runs step 2 (next) and step 3 (arena pop, the movie /
// title branch) on the next ticks as stock.
static int __cdecl OpeningBootC(void)
{
    const int gamma = *(volatile int *)0x1091AEC;  // 0x5230D0's value: the Options gamma
    if (InterlockedIncrement(&g_openFired) > 1) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("opening", "the boot screens were asked for again (call %ld): skipped again", g_openFired);
    }
    __try {
        ((void(__cdecl *)(int))0x5212E0)(gamma);  // ScreenSetGamma, as 0x5190E9
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogAnomaly("opening", "ScreenSetGamma(%d) faulted (code %08lX): the next device reset applies it", gamma, GetExceptionCode());
    }
    ((void(__cdecl *)(int))0x565CA0)(128);        // the screen fade black, as 0x519132
    ((void(__cdecl *)(void))0x555C80)();          // mode 1's next step (0x519196's jmp)
    Log("opening: the boot screens skipped (KONAMI, KCET, the notice; %s), %lu ms after the start: gamma %d applied, the fade black, "
        "on to the title%s", kOpName[g_openIni], GetTickCount() - g_openT0, gamma,
        g_openSites & 4 ? " with its short intro" : "");
    return 1;
}
// Init (DllMain, before the exe's first tick): [Window] Opening and its patches
static void OpeningInstall(void)
{
    static const Site kCheck[2] = {  // mode 1 step 0 / 1 / 2 around the call, mode 1 step 3's branch (check only)
        { 0x42CD65, { 0xE8, 0x16, 0x8F, 0x12, 0x00, 0xE8, 0xF1, 0xC3, 0x0E, 0x00, 0xE9, 0xBC, 0x8E, 0x12, 0x00 }, 15 },
        { 0x42CD9B, { 0xE8, 0x90, 0x23, 0x14, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x6A, 0x03, 0xE8, 0xA5, 0x8E, 0x12, 0x00, 0x6A, 0x01,
                      0xE8, 0x9E, 0xAC, 0x0F, 0x00 }, 23 },
    };
    static const struct { DWORD va; BYTE stock[5], to[5]; int len; const char *what; } kPatch[3] = {
        { 0x42CD6A, { 0xE8, 0xF1, 0xC3, 0x0E, 0x00 }, { 0 }, 5, "the boot screens' call" },
        { 0x42CDA2, { 0x74, 0x12 }, { 0x90, 0x90 }, 2, "the first-start movie branch" },
        { 0x42CDAB, { 0x6A, 0x01 }, { 0x6A, 0x04 }, 2, "the title intro (short)" },
    };
    char b[40];
    unsigned want, done = 0;
    int i, bad = 0;
    g_openT0 = GetTickCount();
    IniStr("Window", "Opening", b, sizeof b);
    g_openIni = OpParse(b);
    if (g_openIni < 0) {
        Log("opening: [Window] Opening=\"%s\" is not Full, Skip Logos or Fast: %s (the default)", b, kOpName[OP_DEFAULT]);
        g_openIni = OP_DEFAULT;
    }
    g_openSel = g_openSnap = g_openIni;
    Encode(g_txtPres[PRT_OLABEL], sizeof g_txtPres[0], OLW_INTERFACE_START_UP_L);  // v3.53 OL1: Start-Up (was Opening)
    for (i = 0; i < OP_N; ++i) {
        Encode(g_txtPres[PRT_ODESC0 + i], sizeof g_txtPres[0], kOpDescTxt[i]);
        Encode(g_txtPres[PRT_OVAL0 + i], sizeof g_txtPres[0], kOpShown[i]);  // v3.53 OL1
        g_openIds[i] = MSG_GFX0 + PRT_OVAL0 + i;
    }
    want = OpSites(g_openIni);
    if (!want) {
        Log("opening: Full ([Window] Opening): the game's own boot screens, movie and title intro (nothing patched)");
        return;
    }
    for (i = 0; i < 2; ++i)
        if (!SiteOk(&kCheck[i])) ++bad;
    if (bad) {
        Log("opening: %s wanted, but mode 1's code at 0042CD65 / 0042CD9B is not the GOG v7 exe's (another mod?): the game's own "
            "opening (nothing patched)", kOpName[g_openIni]);
        return;
    }
    for (i = 0; i < 3; ++i) {
        BYTE to[5];
        if (!(want & (1u << i))) continue;
        if (i == 0) {
            const DWORD rel = (DWORD)(ULONG_PTR)OpeningBootC - (kPatch[0].va + 5);
            to[0] = 0xE8;
            memcpy(to + 1, &rel, 4);
        } else {
            memcpy(to, kPatch[i].to, kPatch[i].len);
        }
        if (!Poke(kPatch[i].va, to, kPatch[i].len)) {  // all or nothing: put back what went in
            const DWORD e = GetLastError();
            int k, back = 1;
            for (k = 0; k < i; ++k)  // v3.39 REV339 (inspection 2): each restore checked against the stock bytes
                if (done & (1u << k))
                    if (!Poke(kPatch[k].va, kPatch[k].stock, kPatch[k].len) ||
                        memcmp((const void *)(ULONG_PTR)kPatch[k].va, kPatch[k].stock, kPatch[k].len)) {
                        back = 0;
                        LogAnomaly("opening", "%s at %08lX could not be put back to the game's bytes", kPatch[k].what, kPatch[k].va);
                    }
            Log("opening: writing %s at %08lX failed (error %lu): %s", kPatch[i].what, kPatch[i].va, e,
                back ? "put back, the game's own opening" : "an earlier site could NOT be put back (see the ANOMALY)");
            return;
        }
        done |= 1u << i;
    }
    g_openSites = done;
    Log("opening: %s ([Window] Opening): 0042CD6A -> the skip (no KONAMI / KCET logos, no notice)%s%s", kOpName[g_openIni],
        done & 2 ? "; 0042CDA2 je -> nops (no first-start movie)" : "", done & 4 ? "; 0042CDAB push 1 -> push 4 (the short title intro)" : "");
}
static int __cdecl OpGet(void) { return (int)g_openSel; }
static int __cdecl OpDesc(void) { return MSG_GFX0 + PRT_ODESC0 + ((unsigned)g_openSel < OP_N ? (int)g_openSel : 0); }
static int __cdecl OpIn(BYTE *cfg, int count)
{
    const int v = StepValue((int)g_openSel, OP_N);
    (void)cfg;
    (void)count;
    if (v >= 0) InterlockedExchange(&g_openSel, v);
    return 0;
}
static void OpApply(void)
{
    if (g_openSel == g_openSnap || (unsigned)g_openSel >= OP_N) return;
    if (!g_iniFallback && !WritePrivateProfileStringA("Window", "Opening", kOpName[g_openSel], g_iniPath)) {
        // v3.39 REV339 (inspection 5): not saved: the row goes back to the saved value (nothing looks settled that is not)
        LogAnomaly("opening", "%s could not be saved (error %lu): the row is back on %s", kOpName[g_openSel], GetLastError(),
                   kOpName[g_openSnap]);
        g_openSel = g_openSnap;
        return;
    }
    g_openSnap = g_openSel;
    Log("opening: %s saved ([Window] Opening); it applies at the next start (this start: %s)", kOpName[g_openSel], kOpName[g_openIni]);
}

// ---------------------------------------------------------------- the rows (sh4windowmod.c's Options code calls these)
static int PresFillRows(DWORD *t, int n)  // ApplyAspectMenu: Renderer and Opening at t + 7 * n; the next free row
{
    int v;
    Encode(g_txtPres[PRT_RLABEL], sizeof g_txtPres[0], OLW_DISPLAY_RENDERER_L);  // v3.53 OL1
    for (v = 0; v < RD_DK_N; ++v) Encode(g_txtPres[PRT_RDESC0 + v], sizeof g_txtPres[0], kRdDescTxt[v]);
    for (v = 0; v < RD_VALS; ++v) g_rendIds[v] = MSG_GFX0 + PRT_RVAL0 + v;
    g_presRend = n;
    g_rendRow = t + 7 * n;
    FillRow(t + 7 * n++, RendIn, RendGet, g_rendIds, 1, MSG_GFX0 + PRT_RLABEL, RendDesc);
    if (g_rdRead) RendValues();
    g_presOpen = n;
    FillRow(t + 7 * n++, OpIn, OpGet, g_openIds, OP_N, MSG_GFX0 + PRT_OLABEL, OpDesc);
    return n;
}
static void PresSnapshot(void)  // ResetPages
{
    if (g_presRend >= 0) RendSnapshot();
    g_openSnap = g_openSel;
}
static int PresChanged(void) { return g_snapOn && (g_rendSel != g_rendSnap || g_openSel != g_openSnap); }
static int PresRowChanged(int row)  // -1 = not one of these rows
{
    if (row >= 0 && row == g_presRend) return g_snapOn && g_rendSel != g_rendSnap;
    if (row >= 0 && row == g_presOpen) return g_snapOn && g_openSel != g_openSnap;
    return -1;
}
static void PresApply(void)  // SetResIndex at the Options exit with "Yes" (never inside the discard)
{
    if (!g_snapOn) return;
    if (g_presRend >= 0) RendApply();
    OpApply();
}
static void PresDiscard(void)  // DiscardOwn ("No")
{
    InterlockedExchange(&g_rendSel, g_rendSnap);
    InterlockedExchange(&g_openSel, g_openSnap);
}
static int PresRowGrey(int page, int row)  // v3.54 OL34: by the row's origin page (Option 4 = 3) and row
{
    return page == 3 && row >= 0 && row == g_presRend && g_rdRead && g_rd.s.greyed;
}
static int PresValueIdGreyed(int id)
{
    return g_presRend >= 0 && g_rdRead && g_rd.s.greyed && id >= MSG_GFX0 + PRT_RVAL0 && id < MSG_GFX0 + PRT_RVAL0 + RD_VALS;
}
// perfhud.inc's Direct3D line: the variant dgVoodoo loaded at this start (NULL = not a manifest one)
static const char *PresBootVariant(void) { return g_rdBootVariant[0] ? g_rdBootVariant : NULL; }
