// ================================================================ v3.43 FN343: the HD menu font ([Textures] MenuFont)
// USER_FEEDBACK_v342.md: "HD menu font: a look-alike open-licensed (OFL) font drawn into the game's font sheet layout at
// high resolution; shippable (no Konami art)". staging\v343_feedback\w6_font\PLAN.md. Included from sh4windowmod.c
// after texpack.inc.
//
// The game draws all its Latin text (menus, memos, subtitles, item names) from ONE glyph cache page (font_sim.h): 19 x 15
// cells of 26 x 34 texels on a 512 x 512 A8R8G8B8 texture, gText.texture 0x12A1D7C (MANAGED, made once at boot by
// 0x556880 through 0x417480 with keep = 1: the "kept" resource map holds the texture object, so the shutdown release
// 0x416740 releases the stock texture whatever the pointer holds). Glyphs are decoded into the 4-bit page on demand;
// 0x558860 calls 0x5567F0 (TextRewritePageTexture, its only caller) when the page changed, which unpacks it into
// gText.pixels 0x1261D78 and writes the texture. The glyph batches (0x559DA0: 0x5592F0 / 0x559327 / 0x559D5C) pass
// [0x12A1D7C] to the flush 0x564210 with a texture size of 512.0f, so u, v are normalised by 512 whatever the texture.
//
// What: with MenuFont = <name>, fonts\<name>.sh4font (tools\font\sh4font_make.py: an OFL font drawn at 4x into the
// stock metrics: every width, kerning pair, baseline and cap height kept, so no text moves) is loaded at start. One
// hook, 0x5567F0's entry (5 bytes, byte-checked, listed in SH4WM_Patches): the stock rewrite runs as always into the
// stock texture (it also fills gText.pixels), then every cell whose glyph code changed is drawn into a 2048 x 2048
// MANAGED A8R8G8B8 texture at 4x: from the sheet when the glyph is a font 0 / 1 Latin glyph and that font's width +
// kerning CRC matches the sheet's, else the stock cell upscaled (bilinear 4x: Japanese, the icon-free special glyphs, a
// font the sheet was not made for). gText.texture then points at the HD texture; the stock texture stays alive (the map
// releases it at shutdown) and comes back if anything fails (CreateTexture, LockRect: logged, the rest of the session
// stock). MenuFont = Stock (the default): nothing is installed.
// Cost: 16 MB (the MANAGED texture's system copy + video memory); a cell redraw is 104 x 136 texels; the first rewrite
// draws all 285 cells once, later ones only the cells the game re-decoded.
//
// v3.44 FN344 (USER_FEEDBACK_v343.md item 8: "give a menu option for them all"): a Menu Font row on Options > Option 1
// (Video&Sound: its 6 stock rows, Menu Font, Exit; 8 rows keep the stock 32-unit step), Stock / EB Garamond / Cormorant
// Garamond / Crimson Text (+ Custom when the ini names another sheet), [Textures] MenuFont written at each step, "No" at
// "Do you want to change the options?" puts it back. The switch is LIVE: the game's own 4-bit cache page never changes
// with the font (the stock glyphs are decoded as always), only this file's HD copy does, so nothing of the game is flushed:
// the new sheet is swapped in, every HD cell is marked stale (g_fnCell) and gText.dirty (0x12A1D78, the flag
// TextCacheGlyph 0x557041 sets and 0x558860 tests at 0x558872) is set to 1, so the next frame with text calls the rewrite
// and the hook redraws all 285 cells from the new sheet (one whole-level lock). To Stock: gText.texture points at the
// stock texture again at once (the stock rewrite keeps it current every time) and the sheet is freed; the HD texture
// stays for a later switch. Starting with Stock installs nothing; the first switch to a font hooks 0x5567F0 then (the
// Options input runs on the game thread, the only caller of 0x558860: input_4052d0 / game_4efbc0 / the menu loops).
#include "font_sim.h"

typedef void(__cdecl *FnRewrite_t)(void *tex, unsigned char *src, int w, int h);
typedef HRESULT(STDMETHODCALLTYPE *FnCreateTex_t)(void *, UINT, UINT, UINT, DWORD, DWORD, DWORD, void **);
typedef HRESULT(STDMETHODCALLTYPE *FnLock_t)(void *, UINT, void *, const RECT *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *FnUnlock_t)(void *, UINT);
typedef ULONG(STDMETHODCALLTYPE *FnRelease_t)(void *);
typedef struct { INT Pitch; void *pBits; } FnLocked;

#define FN_VA_TEX 0x12A1D7C     // gText.texture
#define FN_VA_PIX 0x1261D78     // gText.pixels (512 x 512, a palette index a texel)
#define FN_VA_PAL 0x1257208     // gText.palette[16]
#define FN_VA_CACHE 0x125BBD8   // gText.cache[285]: {SlotLink, u16 code}
#define FN_VA_FONT 0x1261B98    // gText.font[4]
#define FN_VA_DIRTY 0x12A1D78   // v3.44 FN344: gText.dirty (byte): 1 = 0x558860 rewrites the page at its next call
#define FN_SITE 0x5567F0

static char g_fnIni[64];               // [Textures] MenuFont as read (v3.44: then as the row last wrote it)
static char g_fnStem[32];              // the sheet in use ("" = Stock; v3.44: kept only while g_fnState == 1)
static char g_fnCustom[32];            // v3.44 FN344: the ini's own sheet at start (the row's Custom value), "" = none
static unsigned char *g_fnFile;        // the .sh4font image (VirtualAlloc)
static FnSheet g_fnSheet;
static FnRewrite_t g_fnTramp;
static void *g_fnStockTex, *g_fnHdTex;
static int g_fnState;                  // 0 off (Stock or not installed), 1 installed (HD from the first rewrite), -1 failed
static int g_fnHooked;                 // v3.44 FN344: 0x5567F0 hooked (at start, or at the first live switch to a font)
static int g_fnRead;                   // v3.44 FN344: FnReadIni ran
static volatile LONG g_fnSel;          // v3.44 FN344: the row's value (FN_ROW_*)
static LONG g_fnSnap;                  // v3.44 FN344: its value as Options opened
static int g_fnRowN = FN_ROW_N;        // v3.44 FN344: the row's values (5 with a Custom sheet)
static int g_fnRow = -1;               // v3.44 FN344: the row's number on Option 1 (-1 = no row)
static DWORD g_fnRewriteTid;           // v3.44 FN344: the thread of the page rewrites (a live switch runs on it)
static DWORD g_fnSwitches;             // v3.44 FN344: live switches this session
static unsigned int g_fnCell[FN_CELLS];  // the code each HD cell shows (0xFFFFFFFF = not drawn yet)
static const unsigned char *g_fnFontSeen[FN_NFONTS];  // the live font struct last checked
static int g_fnFontOk[FN_NFONTS];      // 1 = that live font's widths + kerning match the sheet's
static volatile LONG g_fnQAnom, g_fnQRedraw, g_fnQThread;
static DWORD g_fnRewrites, g_fnCellsHd, g_fnCellsStock;

static void FnFree(unsigned char **file)
{
    if (*file) VirtualFree(*file, 0, MEM_RELEASE);
    *file = NULL;
}

// Reads fonts\<stem>.sh4font (in the game folder) into *file and checks it into *sheet. 1 = good; 0 = logged why not.
static int FnLoadSheet(const char *stem, unsigned char **file, FnSheet *sheet)
{
    char path[MAX_PATH];
    HANDLE f;
    DWORD size, got = 0;
    const char *why;
    *file = NULL;
    sprintf_s(path, sizeof path, "%sfonts\\%s.sh4font", g_dir, stem);  // g_dir ends with a backslash
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        Log("font: [Textures] MenuFont %s: fonts\\%s.sh4font not found (error %lu): the stock font", g_fnIni, stem, GetLastError());
        return 0;
    }
    size = GetFileSize(f, NULL);
    if (size != (DWORD)FN_FILE_BYTES) {
        CloseHandle(f);
        Log("font: fonts\\%s.sh4font has %lu bytes, a sheet has %lu: the stock font", stem, size, (DWORD)FN_FILE_BYTES);
        return 0;
    }
    *file = (unsigned char *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!*file || !ReadFile(f, *file, size, &got, NULL) || got != size) {
        CloseHandle(f);
        FnFree(file);
        Log("font: fonts\\%s.sh4font could not be read: the stock font", stem);
        return 0;
    }
    CloseHandle(f);
    if ((why = FnSheetParse(*file, size, sheet)) != NULL) {
        FnFree(file);
        Log("font: fonts\\%s.sh4font: %s: the stock font", stem, why);
        return 0;
    }
    return 1;
}

// The live fonts against the sheet (their structs can move when the bank is looked up again: 0x556960).
static void FnCheckFonts(void)
{
    int f;
    for (f = 0; f < FN_NFONTS; ++f) {
        const unsigned char *font = *(const unsigned char *const volatile *)(ULONG_PTR)(FN_VA_FONT + 4 * f);
        unsigned int crc;
        if (font == g_fnFontSeen[f]) continue;
        g_fnFontSeen[f] = font;
        if (!font) {
            g_fnFontOk[f] = 0;
            continue;
        }
        crc = FnMetricsCrc(font);
        g_fnFontOk[f] = g_fnSheet.font[f].present && crc == g_fnSheet.font[f].crc;
        if (g_fnFontOk[f]) LogV("font: font %d (struct %p) matches %s's metrics (crc %08X): its Latin glyphs draw from the sheet",
                                f, font, g_fnSheet.name, crc);
        else Log("font: font %d (struct %p, metrics crc %08X) is not the font %s was made for (crc %08X): its glyphs stay the "
                 "stock ones (upscaled)", f, font, crc, g_fnSheet.name, g_fnSheet.font[f].crc);
        memset(g_fnCell, 0xFF, sizeof g_fnCell);  // a font changed: draw every cell again
    }
}

static void FnDrawCell(unsigned int *dst, int pitch, int c, unsigned int code)
{
    int font, glyph, x, y;
    FnCellXY(c, &x, &y);
    if (FnCodeGlyph(code, &font, &glyph) && g_fnFontOk[font] && g_fnSheet.font[font].have[glyph]) {
        FnFillSheetCell(dst, pitch, &g_fnSheet.font[font], glyph);
        ++g_fnCellsHd;
    } else {
        FnFillStockCell(dst, pitch, (const unsigned char *)FN_VA_PIX, (const unsigned int *)FN_VA_PAL, x, y);
        ++g_fnCellsStock;
    }
}

// Draws the cells whose code changed into the HD texture. 0 = a LockRect failed.
static int FnUpdate(void)
{
    const unsigned char *cache = (const unsigned char *)FN_VA_CACHE;
    int c, changed = 0, done = 0;
    FnCheckFonts();
    for (c = 0; c < FN_CELLS; ++c)
        if (*(const unsigned short *)(cache + 6 * c + 4) != g_fnCell[c]) ++changed;
    if (!changed) return 1;
    if (changed > 64) {  // many cells (the first rewrite, a new screen, v3.44: a live switch): one lock of the whole level
        FnLocked lr;
        if (((FnLock_t)VT(g_fnHdTex, 16))(g_fnHdTex, 0, &lr, NULL, 0) < 0 || !lr.pBits || lr.Pitch < FN_HD * 4) return 0;
        for (c = 0; c < FN_CELLS; ++c) {
            const unsigned int code = *(const unsigned short *)(cache + 6 * c + 4);
            int x, y;
            if (code == g_fnCell[c]) continue;
            FnCellXY(c, &x, &y);
            FnDrawCell((unsigned int *)((unsigned char *)lr.pBits + (size_t)y * FN_SCALE * lr.Pitch) + x * FN_SCALE, lr.Pitch / 4, c, code);
            g_fnCell[c] = code;
            ++done;
        }
        ((FnUnlock_t)VT(g_fnHdTex, 17))(g_fnHdTex, 0);
    } else {
        for (c = 0; c < FN_CELLS; ++c) {
            const unsigned int code = *(const unsigned short *)(cache + 6 * c + 4);
            FnLocked lr;
            RECT r;
            int x, y;
            if (code == g_fnCell[c]) continue;
            FnCellXY(c, &x, &y);
            r.left = x * FN_SCALE;
            r.top = y * FN_SCALE;
            r.right = r.left + FN_HCW;
            r.bottom = r.top + FN_HCH;
            if (((FnLock_t)VT(g_fnHdTex, 16))(g_fnHdTex, 0, &lr, &r, 0) < 0 || !lr.pBits || lr.Pitch < FN_HCW * 4) return 0;
            FnDrawCell((unsigned int *)lr.pBits, lr.Pitch / 4, c, code);
            ((FnUnlock_t)VT(g_fnHdTex, 17))(g_fnHdTex, 0);
            g_fnCell[c] = code;
            ++done;
        }
    }
    ++g_fnRewrites;
    if (LogQuota(&g_fnQRedraw, 20))
        LogT("font: rewrite %lu: %d cell(s) drawn (HD %lu, stock-upscaled %lu so far)", g_fnRewrites, done, g_fnCellsHd, g_fnCellsStock);
    return 1;
}

static void FnGiveUp(const char *why)
{
    g_fnState = -1;
    if (g_fnStockTex && *(void *volatile *)FN_VA_TEX == g_fnHdTex) *(void *volatile *)FN_VA_TEX = g_fnStockTex;
    Log("font: %s: the stock font for the rest of this session", why);
}

// 0x5567F0 (TextRewritePageTexture(tex, page, 0x200, 0x200), called by 0x558860 on the game thread).
static void __cdecl FnRewrite(void *tex, unsigned char *src, int w, int h)
{
    if (!g_fnRewriteTid) g_fnRewriteTid = GetCurrentThreadId();  // v3.44 FN344: a live switch checks it runs here
    if (g_fnState != 1 || w != FN_PAGE || h != FN_PAGE || !tex) {
        if (g_fnState == 1 && LogQuota(&g_fnQAnom, 3))
            LogAnomaly("font", "the glyph page rewrite was called with %p, %d x %d (always the 512 x 512 page): left stock", tex, w, h);
        g_fnTramp(tex == g_fnHdTex && g_fnStockTex ? g_fnStockTex : tex, src, w, h);
        return;
    }
    if (tex != g_fnHdTex) {  // the first rewrite (or the game made its page texture again): this is the stock texture
        g_fnStockTex = tex;
        if (!g_fnHdTex) {
            void *dev = DEV;
            HRESULT hr = dev ? ((FnCreateTex_t)VT(dev, 20))(dev, FN_HD, FN_HD, 1, 0, 21 /*A8R8G8B8*/, 1 /*MANAGED*/, &g_fnHdTex) : E_POINTER;
            if (FAILED(hr) || !g_fnHdTex) {
                g_fnHdTex = NULL;
                g_fnTramp(tex, src, w, h);
                {
                    char why[96];
                    sprintf_s(why, sizeof why, "CreateTexture 2048 x 2048 A8R8G8B8 failed (hr %08lX)", (DWORD)hr);
                    FnGiveUp(why);
                }
                return;
            }
            Log("font: %s (fonts\\%s.sh4font): the glyph page is now 2048 x 2048 (4x, MANAGED A8R8G8B8, 16 MB); the stock "
                "page %p stays alive", g_fnSheet.name, g_fnStem, tex);
        }
        memset(g_fnCell, 0xFF, sizeof g_fnCell);
    }
    g_fnTramp(g_fnStockTex, src, w, h);  // the stock page and gText.pixels, as always
    if (!FnUpdate()) {
        FnGiveUp("LockRect on the HD glyph page failed");
        return;
    }
    *(void *volatile *)FN_VA_TEX = g_fnHdTex;
}

// v3.44 FN344: hooks 0x5567F0 once (at start, or at the first live switch to a font). 1 = hooked.
static int FnHook(void)
{
    static const Site kSite = { FN_SITE, { 0x8B, 0x44, 0x24, 0x08, 0x53, 0x56, 0x8B, 0x74, 0x24, 0x18, 0x57 }, 11 };
    if (g_fnHooked) return 1;
    if (!SiteOk(&kSite)) {
        Log("font: 005567F0 (the glyph page rewrite) is not stock (another mod?): the stock font");
        return 0;
    }
    g_fnTramp = (FnRewrite_t)InlineHook(FN_SITE, 5, (void *)FnRewrite);
    if (!g_fnTramp) {
        Log("font: 005567F0 could not be hooked: the stock font");
        return 0;
    }
    g_fnHooked = 1;
    return 1;
}

// v3.44 FN344: [Textures] MenuFont once (FontInstall at start, or FnRowInstall first: the Options tables are built before).
static void FnReadIni(void)
{
    if (g_fnRead) return;
    g_fnRead = 1;
    IniStr("Textures", "MenuFont", g_fnIni, sizeof g_fnIni);
    g_fnSel = FnRowOfIni(g_fnIni, g_fnCustom, sizeof g_fnCustom);
    g_fnRowN = g_fnCustom[0] ? FN_ROW_MAX : FN_ROW_N;
}

static void FontInstall(void)  // DllMain (Init), before the game's code runs
{
    char stem[32];
    FnReadIni();
    if (!FnIniName(g_fnIni, stem, sizeof stem)) {
        Log("settings: [Textures] MenuFont %s: the stock font", g_fnIni[0] ? g_fnIni : "Stock");
        return;
    }
    if (!FnLoadSheet(stem, &g_fnFile, &g_fnSheet)) return;
    if (!FnHook()) {
        FnFree(&g_fnFile);
        return;
    }
    strcpy_s(g_fnStem, sizeof g_fnStem, stem);
    g_fnState = 1;
    LogV("settings: [Textures] MenuFont %s: fonts\\%s.sh4font (%s, 4x; fonts 0 / 1: %d / %d glyphs), hook 005567F0", g_fnIni,
         g_fnStem, g_fnSheet.name, FnHaveCount(&g_fnSheet.font[0]), FnHaveCount(&g_fnSheet.font[1]));
}

// ---------------------------------------------------------------- v3.44 FN344: the live switch and the Menu Font row
static const char *FnValueLabel(int v) { return v > 0 && v < FN_ROW_N ? kFnRowLabel[v] : v == FN_ROW_CUSTOM ? g_fnCustom : "Stock"; }

// The font in use goes back to Stock (the row's Stock, or a font that could not be loaded / hooked).
static void FnGoStock(const char *why)
{
    const int was = g_fnState == 1;
    g_fnState = 0;
    if (g_fnStockTex && *(void *volatile *)FN_VA_TEX == g_fnHdTex) *(void *volatile *)FN_VA_TEX = g_fnStockTex;
    FnFree(&g_fnFile);
    memset(&g_fnSheet, 0, sizeof g_fnSheet);
    g_fnStem[0] = 0;
    if (was) Log("font: Menu Font -> Stock (Options, live%s%s): the stock glyph page is back%s", why ? ": " : "", why ? why : "",
                 g_fnHdTex ? " (the HD page, 16 MB, stays for a later switch)" : "");
}

// Makes value v's font the one in use, now (the Options input, on the game thread).
static void FnSwitchLive(int v)
{
    const char *to = FnRowStem(v, g_fnCustom);
    const int kind = FnSwitchKind(g_fnState == 1 ? g_fnStem : "", to);
    const DWORD tid = GetCurrentThreadId();
    unsigned char *file = NULL;
    FnSheet sheet;
    if (g_fnState < 0) {
        Log("font: Menu Font %s saved; the HD glyph page failed earlier this session: it applies at the next start", FnValueLabel(v));
        return;
    }
    if (g_fnRewriteTid && tid != g_fnRewriteTid) {  // never seen: the Options input and 0x558860 share the game thread
        if (LogQuota(&g_fnQThread, 3))
            LogAnomaly("font", "a live switch on thread %lu, the glyph page is rewritten on %lu: %s applies at the next start", tid,
                       g_fnRewriteTid, FnValueLabel(v));
        return;
    }
    if (kind == FN_SW_NONE) return;
    ++g_fnSwitches;
    if (kind == FN_SW_STOCK) {
        FnGoStock(NULL);
        return;
    }
    if (!FnLoadSheet(to, &file, &sheet)) {  // (logged: "...: the stock font")
        FnGoStock("the new font could not be loaded");
        return;
    }
    if (!FnHook()) {
        FnFree(&file);
        FnGoStock("the glyph page rewrite could not be hooked");
        return;
    }
    FnFree(&g_fnFile);
    g_fnFile = file;
    g_fnSheet = sheet;  // (its pointers are into file, now g_fnFile)
    strcpy_s(g_fnStem, sizeof g_fnStem, to);
    memset((void *)g_fnFontSeen, 0, sizeof g_fnFontSeen);  // the live fonts are checked against the new sheet's CRCs
    memset(g_fnFontOk, 0, sizeof g_fnFontOk);
    memset(g_fnCell, 0xFF, sizeof g_fnCell);  // every HD cell stale
    g_fnState = 1;
    *(volatile BYTE *)FN_VA_DIRTY = 1;  // 0x558860 rewrites the page at the next frame with text: the hook redraws all cells
    Log("font: Menu Font -> %s (Options, live): fonts\\%s.sh4font (%s, 4x; fonts 0 / 1: %d / %d glyphs); every glyph is drawn "
        "again at the next frame with text%s", FnValueLabel(v), to, g_fnSheet.name, FnHaveCount(&g_fnSheet.font[0]),
        FnHaveCount(&g_fnSheet.font[1]), g_fnHdTex ? "" : " (the 2048 x 2048 page is made then)");
}

// The row's value v: kept, written to the ini and switched live.
static void FnSelect(int v)
{
    const char *word = v == FN_ROW_CUSTOM ? g_fnCustom : v > 0 && v < FN_ROW_N ? kFnRowStem[v] : "Stock";
    if (v < 0 || v >= g_fnRowN) return;
    InterlockedExchange(&g_fnSel, v);
    strcpy_s(g_fnIni, sizeof g_fnIni, word);
    if (!g_iniFallback && !WritePrivateProfileStringA("Textures", "MenuFont", word, g_iniPath))
        Log("font: Menu Font %s applies now but could not be saved in SH4Refined.ini (error %lu)", FnValueLabel(v), GetLastError());
    FnSwitchLive(v);
}

enum { FNM_L, FNM_V0, FNM_D0 = FNM_V0 + FN_ROW_MAX, FNM_DMISS = FNM_D0 + FN_ROW_MAX, FNM_DFAIL, FNM_N };
typedef char FnTextsFit[FNM_N <= MSG_OM0 - MSG_FN0 ? 1 : -1];  // the MSG_FN0 block (16 ids)  // v3.46 OM346: ends at MSG_OM0 (was MSG_END)
static BYTE g_txtFn[FNM_N][112];
static DWORD g_fnIds[FN_ROW_MAX];
static DWORD g_rowsVideo[(8 + 1) * 7];  // Option 1: its 6 stock rows, Menu Font, Exit, the end mark

static const BYTE *FnText(int id)
{
    const int i = id - MSG_FN0;
    return g_txtFn[i >= 0 && i < FNM_N ? i : FNM_L];
}
static int __cdecl FnGet(void) { return (int)g_fnSel; }
static int __cdecl FnDesc(void)
{
    if (g_fnState < 0) return MSG_FN0 + FNM_DFAIL;
    if (g_fnSel != 0 && g_fnState != 1) return MSG_FN0 + FNM_DMISS;  // the chosen sheet is missing / could not be hooked
    return MSG_FN0 + FNM_D0 + ((unsigned)g_fnSel < FN_ROW_MAX ? (int)g_fnSel : 0);
}
static int __cdecl FnIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    v = StepValue((int)g_fnSel, g_fnRowN);
    if (v >= 0) FnSelect(v);
    return 0;
}
static void FnSnapshot(void) { g_fnSnap = g_fnSel; }
static int FnChanged(void) { return g_snapOn && g_fnRow >= 0 && g_fnSel != g_fnSnap; }
static int FnRowChanged(int row) { return g_fnRow < 0 || row != g_fnRow ? -1 : FnChanged(); }  // -1 = not the Menu Font row
static void FnDiscard(void)
{
    if (g_fnRow < 0 || g_fnSel == g_fnSnap) return;
    LogV("font: Menu Font back to %s (Options discarded)", FnValueLabel((int)g_fnSnap));
    FnSelect((int)g_fnSnap);
}

// ApplyAspectMenu, after InstallPages and Resolution's value labels: Option 1 becomes a copy of its stock table with the
// Menu Font row above Exit (only when the table is the stock one: 7 rows, Resolution first, Sound row 5, Exit row 6).
static void FnRowInstall(void)
{
    const DWORD *s = g_opTable[0];
    int v;
    FnReadIni();
    if (!g_pagesOn) {
        Log("font: no Options pages (see above): no Menu Font row; [Textures] MenuFont still applies at the start");
        return;
    }
    if (s != (const DWORD *)0x654CC0 || g_opCount[0] != 7 || s[0] != 0x525030 || s[5 * 7 + 5] != 0x24 || s[6 * 7 + 5] != 0x30) {
        Log("font: Option 1's table is not the stock one (%p, %d rows): no Menu Font row; [Textures] MenuFont still applies at the "
            "start", (const void *)s, g_opCount[0]);
        return;
    }
    {   // v3.53 OL1: the plan's words (Cormorant / Crimson on screen; the ini keeps CormorantGaramond / CrimsonText)
        static const char *const kV[FN_ROW_N] = { OLW_INTERFACE_MENU_FONT_V_STOCK, OLW_INTERFACE_MENU_FONT_V_EB_GARAMOND,
                                                  OLW_INTERFACE_MENU_FONT_V_CORMORANT, OLW_INTERFACE_MENU_FONT_V_CRIMSON };
        static const char *const kD[FN_ROW_N] = { OLW_INTERFACE_MENU_FONT_VD_STOCK, OLW_INTERFACE_MENU_FONT_VD_EB_GARAMOND,
                                                  OLW_INTERFACE_MENU_FONT_VD_CORMORANT, OLW_INTERFACE_MENU_FONT_VD_CRIMSON };
        Encode(g_txtFn[FNM_L], sizeof g_txtFn[0], OLW_INTERFACE_MENU_FONT_L);
        for (v = 0; v < FN_ROW_N; ++v) {
            Encode(g_txtFn[FNM_V0 + v], sizeof g_txtFn[0], kV[v]);
            Encode(g_txtFn[FNM_D0 + v], sizeof g_txtFn[0], kD[v]);
        }
        Encode(g_txtFn[FNM_V0 + FN_ROW_CUSTOM], sizeof g_txtFn[0], OLW_INTERFACE_MENU_FONT_V_CUSTOM);
        Encode(g_txtFn[FNM_D0 + FN_ROW_CUSTOM], sizeof g_txtFn[0], OLW_INTERFACE_MENU_FONT_VD_CUSTOM);
        Encode(g_txtFn[FNM_DMISS], sizeof g_txtFn[0], OLW_FONT_MISSING);
        Encode(g_txtFn[FNM_DFAIL], sizeof g_txtFn[0], OLW_FONT_FAILED);
    }
    for (v = 0; v < FN_ROW_MAX; ++v) g_fnIds[v] = MSG_FN0 + FNM_V0 + v;
    memcpy(g_rowsVideo, s, 6 * 7 * 4);  // Resolution (its labels already g_resIds), Brightness, Gamma, BGM, SE, Sound
    FillRow(g_rowsVideo + 6 * 7, FnIn, FnGet, g_fnIds, g_fnRowN, MSG_FN0 + FNM_L, FnDesc);
    memcpy(g_rowsVideo + 7 * 7, s + 6 * 7, 7 * 4);  // Exit
    g_rowsVideo[8 * 7] = 0;
    g_opTable[0] = g_rowsVideo;
    g_opCount[0] = 8;
    g_fnRow = 6;
    LogV("font: Options > Option 1 has a Menu Font row (row 7 of 8, above Exit; %d values%s; it switches live): %s", g_fnRowN,
         g_fnCustom[0] ? ", Custom = the ini's own sheet" : "", FnValueLabel((int)g_fnSel));
}
