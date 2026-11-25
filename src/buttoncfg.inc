// ================================================================ v3.10.0: Button Config readable pad names
// Plan and review: staging/button_config/PLAN.md (claudex-loop, Codex), RE: staging/button_config/RE.md. All
// addresses are absolute VAs of the GOG v7 exe. Included by sh4windowmod.c after eileenoutfit.inc.
//
// Options > Option 2 > Button Config draws each pad binding as `50B5B0(0, [0x654020 + index * 4], x, y, 0, 0)` in
// its row draw 0x5176A0 (0x517848 `mov ebp,[eax*4+654020h]`, the table's only reader); index = the binding's place
// in the pad code table 0x5FF6F0 (45 codes: 0-3 POV up / right / down / left, 4-11 the two sticks, 12-44 buttons
// with a stock duplicate at 27 / 28). The stock names are raw ("Pov U", "Ana Y-", "Button04"), buttons 16..30 are
// labelled one too high and button 31 reads past the table ("Move Forward"). That operand now points at
// g_padNameId[45] = private message ids answered by Refined's message hook 0x50C440 (MsgText -> PadText), with
// names of the connected pad's family. GOG's dinput8.dll maps every pad onto one Xbox-style positional layout
// (goginput.ini onelayout = TRUE), so a family only changes the names, not the meaning.
// Also: the pad column moves from x 394 to 376 (imm32 at 0x51785B / 0x517887) so the long names fit, and the
// screen's "Default" (0x405C00) gets GOG's shipped pad layout instead of the old PS2-adapter one.

#define PAD_N 45
enum { PADFAM_GENERIC, PADFAM_XBOX, PADFAM_PS, PADFAM_NINTENDO, PADFAM_N };
static const char *const kPadFamName[PADFAM_N] = { "Generic", "Xbox", "PlayStation", "Nintendo" };
static DWORD g_padNameId[PAD_N];          // 0x51784B points here: MSG_PAD0 + index
static BYTE g_txtPad[PAD_N][24];          // the names of the current family (byte-mode strings)
static int g_padFam = -1, g_padShare;     // the family the texts were made for (-1 = none yet: the first refresh
                                          // always builds them); PlayStation select: 0 Create, 1 Share (DS4), 2 Select (DS3),
                                          // 3 Sony models disagree, 4 PadNames=PlayStation without a detected DualSense
static int g_padForce = -1;               // [Input] PadNames: -1 Auto, else a PADFAM_*
static DWORD g_padCheck;                  // GetTickCount of the last detection (at most every 2 s)
static int g_padInstalled, g_padProbed;

// index -> name. 0..11 POV and sticks (same for every family), 12..27 the 16 virtual-pad buttons, 28 = the stock
// duplicate of 27's code, 29..44 buttons 16..31. Buttons whose meaning on the virtual pad is not confirmed (12 and up)
// keep their number (RE 3.3: Guide / Share / Edge paddles are inferred).
static const char *const kPadAxis[12] = { "D-Pad Up", "D-Pad Right", "D-Pad Down", "D-Pad Left", "LS Up", "LS Down",
                                          "LS Left", "LS Right", "RS Up", "RS Down", "RS Left", "RS Right" };
static const char *const kPadBtn[PADFAM_N][12] = {
    { "Button 0", "Button 1", "Button 2", "Button 3", "Button 4", "Button 5", "Button 6", "Button 7", "Button 8",
      "Button 9", "Button 10", "Button 11" },  // the game's own 0-based button numbers (review PADNAME-003)
    { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "View", "Menu", "LS Click", "RS Click" },
    { "Cross", "Circle", "Square", "Triangle", "L1", "R1", "L2", "R2", "Create", "Options", "L3", "R3" },
    { "B", "A", "Y", "X", "L", "R", "ZL", "ZR", "Minus", "Plus", "LS Click", "RS Click" },
};

// ---------------------------------------------------------------- v3.11.0: pad glyphs (staging/button_glyphs/PLAN.md)
// The text renderer draws `F0 nn` as icon nn of the table 0x5C5E98 (20 bytes per icon, 0x14 stock entries) from the
// icon atlas (fontdata_ex.bin entry 0: 256 x 256 DXT3, 7 levels; its bottom half y 128..255 is empty in levels 0..3).
// The table is copied into g_iconTab[0x5A] (its six readers repointed) with icons 0x20..0x3F = 32 cells in that empty
// half; the cells get the current family's Kenney glyphs (tools/gen_glyphs.py -> glyphs_data.h), written with
// LockRect only where the half is still free (zero alpha or our own blocks: review GLYPH-002).
#include "glyphs_data.h"
#define GLYPH_NN0 0x20               // icon number of cell 0
#define ICON_N 0x5A                  // entries the renderer indexes directly (0x5571B2: nn >= 0x5A is remapped)
static BYTE g_iconTab[ICON_N][20];   // the six table readers point here (+0 / +8 / +9)
static int g_padGlyphs = 1;          // [Input] PadGlyphs
static int g_glyphTab;               // 1 = the readers point at g_iconTab
static int g_glyphFam = -1;          // the family whose cells g_txtPad shows (-1 = names)
static void *g_glyphLogTex;          // the texture last logged about (log once per texture and outcome)
static int g_glyphLogOk = -1;

static const BYTE *GlyphData(int fam, DWORD *mask)
{
    switch (fam) {
    case PADFAM_XBOX: *mask = GLYPH_XBOX_MASK; return kGlyphXbox;
    case PADFAM_PS: *mask = GLYPH_PS_MASK; return kGlyphPs;
    case PADFAM_NINTENDO: *mask = GLYPH_NINTENDO_MASK; return kGlyphNintendo;
    }
    *mask = 0;
    return NULL;
}

typedef struct { DWORD Format, Type, Usage, Pool, Size, MultiSampleType, Width, Height; } SurfDesc8;  // D3DSURFACE_DESC (d3d8)
typedef struct { INT Pitch; void *pBits; } LockedRect8;

// Writes family fam's cells into the atlas (game thread). 1 = they are there now. why = the reason when not.
static int GlyphUpload(int fam, void **texOut, const char **why)
{
    static const BYTE *const kAll[3] = { kGlyphXbox, kGlyphPs, kGlyphNintendo };
    DWORD mask;
    const BYTE *data = GlyphData(fam, &mask);
    LockedRect8 lr[GLYPH_LEVELS];
    volatile int locked = 0;  // read by the __except cleanup
    int ok = 0, L;
    void **volatile tex = NULL, **volatile vt = NULL;
    *texOut = NULL;
    *why = "no glyphs for this family";
    if (!data) return 0;
    __try {
        BYTE *g = *(BYTE **)0x1261BAC;  // the icon atlas's texture group (556960)
        SurfDesc8 d;
        *why = "the icon atlas is not loaded";
        if (!g) return 0;
        if (!*(const DWORD *)(g + 0xC)) {
            // The PC port never links entry 0 to its DXT3 record (live finding, PLAN 3): the record sits at g + 0x40.
            // Link it only when it is exactly the stock one, so the game's own 0x42AAC0 creates the texture.
            // review GLYPH-003: every field 0x42AAC0 consumes must equal the stock file's (all 7 level sizes and offsets;
            // the last level ends at record + 0x156B0 = bank + 0x15710, inside the loaded bank)
            static const DWORD kSize[7] = { 0x10000, 0x4000, 0x1000, 0x400, 0x100, 0x40, 0x10 };
            static const DWORD kOff[7] = { 0x120, 0x10120, 0x14120, 0x15120, 0x15520, 0x15620, 0x156A0 };
            const BYTE *rec = g + 0x40;
            int l;
            *why = "the icon atlas record is not the stock one (no link)";
            if (*(const DWORD *)(rec + 0x1C) || *(const DWORD *)(rec + 0x20) != 256 || *(const DWORD *)(rec + 0x24) != 256 ||
                *(const DWORD *)(rec + 0x28) != 0x33545844u || *(const DWORD *)(rec + 0x2C) != 7)
                return 0;
            for (l = 0; l < 7; ++l)
                if (*(const DWORD *)(rec + 0x30 + 4 * l) != kSize[l] || *(const DWORD *)(rec + 0x50 + 4 * l) != kOff[l]) return 0;
            *(DWORD *)(g + 0xC) = 0x40;
            Log("button config: linked the icon atlas (fontdata_ex.bin entry 0) to its DXT3 record (the PC port leaves it unlinked)");
        }
        tex = ((void **(__cdecl *)(void *))0x42AAC0)(g);  // the game's own getter: creates the texture on first use
        *texOut = tex;
        if (!tex) return 0;
        vt = *(void ***)tex;
        *why = "the icon atlas is not the stock 256 x 256 DXT3 managed texture";
        if (((DWORD(__stdcall *)(void *))vt[13])(tex) < GLYPH_LEVELS) return 0;           // GetLevelCount
        memset(&d, 0, sizeof d);
        if (((HRESULT(__stdcall *)(void *, UINT, SurfDesc8 *))vt[14])(tex, 0, &d) < 0) return 0;  // GetLevelDesc
        if (d.Width != 256 || d.Height != 256 || d.Format != 0x33545844u /* DXT3 */ || d.Pool != 1 /* MANAGED */) return 0;
        *why = "LockRect failed";
        for (locked = 0; locked < GLYPH_LEVELS; ++locked)
            if (((HRESULT(__stdcall *)(void *, UINT, LockedRect8 *, const RECT *, DWORD))vt[16])(tex, locked, &lr[locked], NULL, 0) < 0)
                break;
        if (locked == GLYPH_LEVELS) {
            // free = every bottom-half block of levels 0..3 is zero alpha (stock) or one of our families' blocks
            size_t off = 0;
            int foreign = 0;
            for (L = 0; L < GLYPH_LEVELS && !foreign; ++L) {
                const int bw = 64 >> L, rows = 32 >> L, row0 = 32 >> L;  // blocks per row, bottom-half rows, first row
                int r, b, f;
                for (r = 0; r < rows && !foreign; ++r)
                    for (b = 0; b < bw && !foreign; ++b) {
                        const BYTE *p = (const BYTE *)lr[L].pBits + (size_t)(row0 + r) * lr[L].Pitch + 16 * b;
                        const size_t k = off + ((size_t)r * bw + b) * 16;
                        if (!*(const DWORD *)p && !*(const DWORD *)(p + 4)) continue;
                        for (f = 0; f < 3 && memcmp(p, kAll[f] + k, 16); ++f) {}
                        if (f == 3) foreign = 1;
                    }
                off += (size_t)bw * rows * 16;
            }
            *why = "the atlas's bottom half holds other art (a texture pack?)";
            if (!foreign) {
                off = 0;
                for (L = 0; L < GLYPH_LEVELS; ++L) {
                    const int bw = 64 >> L, rows = 32 >> L, row0 = 32 >> L;
                    int r;
                    for (r = 0; r < rows; ++r)
                        memcpy((BYTE *)lr[L].pBits + (size_t)(row0 + r) * lr[L].Pitch, data + off + (size_t)r * bw * 16, (size_t)bw * 16);
                    off += (size_t)bw * rows * 16;
                }
                ok = 1;
            }
        }
        for (L = 0; L < locked; ++L) ((HRESULT(__stdcall *)(void *, UINT))vt[17])(tex, L);  // UnlockRect
        locked = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *why = "fault";
        ok = 0;
        // review GLYPH-REV-002: release every level still locked, so the next refresh can lock again
        __try {
            for (L = 0; L < locked; ++L) ((HRESULT(__stdcall *)(void *, UINT))vt[17])(tex, L);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *why = "fault (unlock faulted too)";
        }
    }
    return ok;
}

// The icon table copy and its six readers (install time). Stock bytes of every instruction checked; all or nothing.
static void GlyphInstall(void)
{
    static const struct { DWORD va; BYTE stock[8]; int len, field; } kRd[] = {  // field = table byte the reader adds
        { 0x55721F, { 0x0F, 0xB6, 0x04, 0xBD, 0xA1, 0x5E, 0x5C, 0x00 }, 8, 9 },
        { 0x55722A, { 0x8D, 0x3C, 0xBD, 0x98, 0x5E, 0x5C, 0x00 }, 7, 0 },
        { 0x557A73, { 0x0F, 0xB6, 0x04, 0x8D, 0xA0, 0x5E, 0x5C, 0x00 }, 8, 8 },
        { 0x558170, { 0x0F, 0xB6, 0x04, 0x8D, 0xA0, 0x5E, 0x5C, 0x00 }, 8, 8 },
        { 0x559520, { 0x8D, 0x3C, 0xBD, 0x98, 0x5E, 0x5C, 0x00 }, 7, 0 },
        { 0x55972A, { 0x8D, 0x3C, 0xBD, 0x98, 0x5E, 0x5C, 0x00 }, 7, 0 },
    };
    const int n = (int)(sizeof kRd / sizeof kRd[0]);
    int i, done;
    if (!g_padGlyphs) {
        Log("button config: glyphs off ([Input] PadGlyphs = 0)");
        return;
    }
    for (i = 0; i < n; ++i)
        if (memcmp((const void *)(ULONG_PTR)kRd[i].va, kRd[i].stock, kRd[i].len)) {
            Log("button config: the icon table reader %08lX is not stock (another mod?): no glyphs", kRd[i].va);
            return;
        }
    memset(g_iconTab, 0, sizeof g_iconTab);
    memcpy(g_iconTab, (const void *)0x5C5E98, 0x14 * 20);  // the stock entries, unchanged
    for (i = 0; i < 32; ++i) {
        BYTE *e = g_iconTab[GLYPH_NN0 + i];
        const WORD u = (WORD)(32 * (i % 8)), v = (WORD)(128 + 32 * (i / 8)), s = 32;
        memcpy(e + 0, &u, 2);
        memcpy(e + 2, &v, 2);
        memcpy(e + 4, &s, 2);
        memcpy(e + 6, &s, 2);
        e[8] = 0x23;  // advance (stock)
        e[9] = 0x20;  // height (stock)
        memset(e + 0x10, 0x80, 4);  // colour (stock)
    }
    for (done = 0; done < n; ++done) {
        const DWORD to = (DWORD)(ULONG_PTR)g_iconTab + kRd[done].field;
        if (!Poke(kRd[done].va + kRd[done].len - 4, &to, 4)) break;
    }
    if (done < n) {
        int lost = 0;
        for (i = 0; i < done; ++i) {
            const DWORD *stock = (const DWORD *)(kRd[i].stock + kRd[i].len - 4);
            if (!Poke(kRd[i].va + kRd[i].len - 4, stock, 4) || memcmp((const void *)(ULONG_PTR)(kRd[i].va + kRd[i].len - 4), stock, 4)) {
                ++lost;
                Log("button config: %08lX could not be put back", kRd[i].va);
            }
        }
        // review GLYPH-REV-001: never emit icons 0x20+ unless every reader points at the copy (fail closed)
        Log("button config: an icon table patch could not be written%s: no glyphs", lost ? "; some readers stay patched (their copy has the stock entries)" : "");
        return;
    }
    g_glyphTab = 1;
    Log("button config: icon table copied (0x%X entries, icons 0x%X..0x%X = glyph cells), %d readers", ICON_N, GLYPH_NN0, GLYPH_NN0 + 31, n);
}

static void EncodeIcon(BYTE *out, BYTE nn)  // byte-mode string holding icon nn only (Encode maps characters to 0x100 - c)
{
    out[0] = 0x00;
    out[1] = 0x80;
    out[2] = 0xF0;
    out[3] = nn;
    out[4] = 0xFF;
    out[5] = 0xFF;
}

// glyphs = 1: indices whose cell has a glyph get it, the rest their name
static void PadMakeTexts(int fam, int share, int glyphs)
{
    DWORD mask = 0;
    int i;
    if (glyphs) GlyphData(fam, &mask);
    for (i = 0; i < PAD_N; ++i) {
        char s[24];
        int cell = -1;
        if (i < 12) {
            strcpy_s(s, sizeof s, kPadAxis[i]);
            cell = i;  // cells 0..11 follow kPadAxis's order
        } else {
            const int btn = i < 28 ? i - 12 : i == 28 ? 15 : i - 13;  // 28 = the duplicate of 27 (button 15)
            if (btn < 12) {
                strcpy_s(s, sizeof s, kPadBtn[fam][btn]);
                cell = 12 + btn;
            } else {
                sprintf_s(s, sizeof s, "Button %d", btn);  // 0-based, as the game's own numbers
            }
            if (fam == PADFAM_PS && btn == 8 && share && share != 4)  // DS4 "Share", DS3 "Select", other Sony pads numbered (PADNAME-006)
                strcpy_s(s, sizeof s, share == 1 ? "Share" : share == 2 ? "Select" : "Button 8");
            if (fam == PADFAM_PS && btn == 9 && share && share != 4)  // v3.25.0: DS3 "Start", disagreeing Sony models numbered
                strcpy_s(s, sizeof s, share == 1 ? "Options" : share == 2 ? "Start" : "Button 9");
            // v3.25.0 (staging/feedback_v325/PLAN.md 2): the composed Create / Options cells show the DualSense's own
            // markings, so only a detected DualSense (share 0) gets them; share 4 = PadNames=PlayStation without a
            // detected DualSense (review REV-001): the words "Create" / "Options" as before
            if (fam == PADFAM_PS && (btn == 8 || btn == 9) && share) cell = -1;
        }
        if (cell >= 0 && (mask >> cell & 1)) EncodeIcon(g_txtPad[i], (BYTE)(GLYPH_NN0 + cell));
        else Encode(g_txtPad[i], sizeof g_txtPad[i], s);
    }
}

// review PADNAME-004: a pad is only named after its family when GOG's wrapper really maps it onto the virtual layout:
// its DirectInput product GUID (PPPPVVVV-0000-0000-0000-504944564944) has a section in goginput.ini, or it is an
// XInput device (the wrapper reads those through XInput, Xbox layout). Anything else may keep its native order.
static char *g_gogIni;  // goginput.ini's text (read once)
static int PadListed(DWORD vid, DWORD pid)
{
    char key[48];
    const char *p;
    if (!g_gogIni) {
        char path[MAX_PATH];
        HANDLE h;
        DWORD size, got = 0;
        sprintf_s(path, sizeof path, "%s\\goginput.ini", g_dir);
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return 0;
        size = GetFileSize(h, NULL);
        if (size == INVALID_FILE_SIZE || size > (1u << 20)) { CloseHandle(h); return 0; }
        g_gogIni = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1);
        if (g_gogIni && !ReadFile(h, g_gogIni, size, &got, NULL)) g_gogIni[0] = 0;
        CloseHandle(h);
        if (!g_gogIni) return 0;
        for (p = g_gogIni; *p; ++p) *(char *)p = (char)toupper((unsigned char)*p);
    }
    sprintf_s(key, sizeof key, "%04lX%04lX-0000-0000-0000-504944564944", pid & 0xFFFF, vid & 0xFFFF);
    return strstr(g_gogIni, key) != NULL;
}

// The pad family: Raw Input HID game pads (usage page 1, usage 4 joystick / 5 gamepad), first recognised one wins.
// *share = 1 for a DualShock 4 (its select button is "Share"). Generic when goginput.ini no longer maps every pad
// onto the virtual layout (onelayout = FALSE or keyboardandmouse = TRUE).
static int PadDetect(int *share)
{
    char ini[MAX_PATH], v[16];
    UINT n = 0, i;
    RAWINPUTDEVICELIST *list;
    int fam = -1, famHere, sh, mixed = 0;
    *share = 0;
    sprintf_s(ini, sizeof ini, "%s\\goginput.ini", g_dir);
    GetPrivateProfileStringA("general", "onelayout", "TRUE", v, sizeof v, ini);
    if (_stricmp(v, "TRUE")) return PADFAM_GENERIC;
    GetPrivateProfileStringA("general", "keyboardandmouse", "FALSE", v, sizeof v, ini);
    if (!_stricmp(v, "TRUE")) return PADFAM_GENERIC;
    if (GetRawInputDeviceList(NULL, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || !n || n > 256) return PADFAM_GENERIC;
    list = (RAWINPUTDEVICELIST *)HeapAlloc(GetProcessHeap(), 0, n * sizeof *list);
    if (!list) return PADFAM_GENERIC;
    n = GetRawInputDeviceList(list, &n, sizeof(RAWINPUTDEVICELIST));
    for (i = 0; i < n && n != (UINT)-1; ++i) {
        RID_DEVICE_INFO di;
        UINT sz = sizeof di;
        char name[256];
        UINT nsz = sizeof name;
        DWORD vid, pid;
        if (list[i].dwType != RIM_TYPEHID) continue;
        memset(&di, 0, sizeof di);
        di.cbSize = sizeof di;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICEINFO, &di, &sz) == (UINT)-1) continue;
        if (di.hid.usUsagePage != 1 || (di.hid.usUsage != 4 && di.hid.usUsage != 5)) continue;
        vid = di.hid.dwVendorId;
        pid = di.hid.dwProductId;
        name[0] = 0;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name, &nsz) == (UINT)-1) name[0] = 0;
        name[sizeof name - 1] = 0;
        if (!(strstr(name, "IG_") || strstr(name, "ig_")) && !PadListed(vid, pid)) continue;  // not remapped by GOG
        famHere = -1;
        sh = 0;
        if (vid == 0x054C) {
            // review PADNAME-001: DualShock 4 (and its USB adapter) "Share", DualShock 3 "Select", DualSense "Create"
            // review PADNAME-006: "Create" only for the known DualSense PIDs (0CE6, 0DF2, Edge 0DF2 family); others 3
            sh = (pid == 0x05C4 || pid == 0x09CC || pid == 0x0BA0) ? 1 : pid == 0x0268 ? 2 :
                    (pid == 0x0CE6 || pid == 0x0DF2) ? 0 : 3;
            famHere = PADFAM_PS;
        } else if (vid == 0x057E) {
            famHere = PADFAM_NINTENDO;
        } else if (vid == 0x045E || strstr(name, "IG_") || strstr(name, "ig_")) {
            famHere = PADFAM_XBOX;  // Microsoft, or any XInput device
        }
        // review PADNAME-005: the game reads the first DirectInput pad, which may not be the first Raw Input one; with
        // pads of two different families Auto cannot tell which one GOG shows to the game: Generic (PadNames forces one)
        if (famHere < 0) continue;
        if (fam >= 0 && fam != famHere) { mixed = 1; break; }
        if (fam < 0) { fam = famHere; *share = sh; }
        else if (famHere == PADFAM_PS && sh != *share) *share = 3;  // review PADNAME-007: Sony models disagree: "Button 8"
    }
    HeapFree(GetProcessHeap(), 0, list);
    if (mixed || fam < 0) {
        *share = 0;
        return PADFAM_GENERIC;
    }
    return fam;
}

// v3.25.0 (review REV-325-012): the connected Sony model alone, for PadNames=PlayStation: every Raw Input HID game
// pad with Sony's vendor id, whatever goginput.ini's layout options or other pads say. 0 DualSense, 1 DualShock 4,
// 2 DualShock 3, 3 other Sony pad or models that disagree, 4 no Sony pad.
static int PadSonyModel(void)
{
    UINT n = 0, i;
    RAWINPUTDEVICELIST *list;
    int model = 4;
    if (GetRawInputDeviceList(NULL, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || !n || n > 256) return 4;
    list = (RAWINPUTDEVICELIST *)HeapAlloc(GetProcessHeap(), 0, n * sizeof *list);
    if (!list) return 4;
    n = GetRawInputDeviceList(list, &n, sizeof(RAWINPUTDEVICELIST));
    for (i = 0; i < n && n != (UINT)-1; ++i) {
        RID_DEVICE_INFO di;
        UINT sz = sizeof di;
        DWORD pid;
        int sh;
        if (list[i].dwType != RIM_TYPEHID) continue;
        memset(&di, 0, sizeof di);
        di.cbSize = sizeof di;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICEINFO, &di, &sz) == (UINT)-1) continue;
        if (di.hid.usUsagePage != 1 || (di.hid.usUsage != 4 && di.hid.usUsage != 5) || di.hid.dwVendorId != 0x054C) continue;
        pid = di.hid.dwProductId;
        sh = (pid == 0x05C4 || pid == 0x09CC || pid == 0x0BA0) ? 1 : pid == 0x0268 ? 2 : (pid == 0x0CE6 || pid == 0x0DF2) ? 0 : 3;
        model = model == 4 || model == sh ? sh : 3;  // two different Sony models: numbered (as PADNAME-007)
    }
    HeapFree(GetProcessHeap(), 0, list);
    return model;
}

// The game's own joystick device ([[0x6E8EA4] + 0xC], GOG's "Virtual Gamepad"): its DIPROP_VIDPID, logged once as
// a probe (whether the wrapper reports the real pad; not used for the names).
static void PadProbe(void)
{
    struct { DWORD dwSize, dwHeaderSize, dwObj, dwHow, dwData; } p = { 20, 16, 0, 0, 0 };
    BYTE *in;
    void **dev;
    HRESULT hr;
    if (g_padProbed) return;
    g_padProbed = 1;
    __try {
        in = *(BYTE **)0x6E8EA4;
        dev = in ? *(void ***)(in + 0xC) : NULL;
        if (!dev) {
            Log("button config: no game pad device open (probe)");
            return;
        }
        // IDirectInputDevice8::GetProperty (vtable 5), DIPROP_VIDPID = MAKEDIPROP(24)
        hr = ((HRESULT(__stdcall *)(void *, const void *, void *))(*(void ***)dev)[5])(dev, (const void *)24, &p);
        Log("button config: probe: the game's pad device reports VID/PID %08lX (hr %08lX)", p.dwData, (DWORD)hr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("button config: probe faulted");
    }
}

// MsgText (0x50C440 hook) for ids MSG_PAD0..MSG_PAD0 + 44: the family is checked again at most every 2 s, so a pad
// plugged in while the game runs is picked up the next time Button Config is drawn.
static const BYTE *PadText(int i)
{
    const DWORD now = GetTickCount();
    if (g_padFam < 0 || now - g_padCheck > 2000) {
        int share = 0, fam;
        g_padCheck = now;
        int glyphs = 0;
        fam = g_padForce >= 0 ? g_padForce : PadDetect(&share);
        if (g_padForce == PADFAM_PS) share = PadSonyModel();  // v3.25.0 (REV-001 / REV-325-012): the forced family's model
        if (g_glyphTab) {  // v3.11.0: rewrite the cells at every refresh (cheap; heals a recreated texture)
            DWORD mask;
            if (GlyphData(fam, &mask)) {
                void *tex;
                const char *why;
                glyphs = GlyphUpload(fam, &tex, &why);
                if (tex != g_glyphLogTex || glyphs != g_glyphLogOk) {
                    g_glyphLogTex = tex;
                    g_glyphLogOk = glyphs;
                    if (glyphs) Log("button config: %s glyphs written to the icon atlas (texture %p)", kPadFamName[fam], tex);
                    else Log("button config: no glyphs (%s; texture %p): names", why, tex);
                }
            }
        }
        if (fam != g_padFam || share != g_padShare || (glyphs ? fam : -1) != g_glyphFam) {
            g_padFam = fam;
            g_padShare = share;
            g_glyphFam = glyphs ? fam : -1;
            PadMakeTexts(fam, share, glyphs);
            Log("button config: pad %s %s%s%s", glyphs ? "glyphs" : "names", kPadFamName[fam], fam == PADFAM_PS && share ? (share == 1 ? " (DualShock 4)" : share == 2 ? " (DualShock 3)" : share == 4 ? " (no DualSense found)" : " (other Sony pad)") : "",
                g_padForce >= 0 ? " ([Input] PadNames)" : " (detected)");
        }
        PadProbe();
    }
    if (i < 0 || i >= PAD_N) i = 0;
    return g_txtPad[i];
}

static void ButtonConfigInstall(void)
{
    // GOG's shipped pad layout for "Default" (the installed input.ini, PLAN K5): per patched immediate, its stock code
    // and GOG's. 0x405C00 fills 27 stack slots in input.ini order; ecx at 0x405D83 serves Enter + Action, ecx at
    // 0x405D95 SkipDemo + Pause.
    // disp = the stack slot byte of `mov dword ptr [esp+disp], imm32` (C7 44 24 disp imm32; review PADNAME-008: every
    // instruction byte is checked, only the immediate differs after the patch); 0 for `mov ecx, imm32` (B9 imm32)
    static const struct { DWORD va; int len, off; BYTE disp; DWORD stock, gog; } kDef[] = {
        { 0x405D83, 5, 1, 0, 0x3280, 0x3080 },     // Enter, Action: X -> A
        { 0x405D95, 5, 1, 0, 0x3880, 0x3980 },     // SkipDemo, Pause: View -> Menu
        { 0x405DB2, 8, 4, 0x18, 0x3680, 0x3480 },  // MoveLeft: LT -> LB
        { 0x405DBA, 8, 4, 0x1C, 0x3780, 0x3580 },  // MoveRight: RT -> RB
        { 0x405DFA, 8, 4, 0x4C, 0x3580, 0x3780 },  // Pose (Ready Weapon): RB -> RT
        { 0x405E0A, 8, 4, 0x54, 0x3480, 0x3680 },  // RearCamera: LB -> LT
        { 0x405E12, 8, 4, 0x5C, 0x3B80, 0x3380 },  // ScrapBook: RS Click -> Y
        { 0x405E1A, 8, 4, 0x60, 0x3080, 0x3B80 },  // Map: A -> RS Click
        { 0x405E22, 8, 4, 0x64, 0x3380, 0x3280 },  // Item: Y -> X
        { 0x405E2A, 8, 4, 0x68, 0x3980, 0x3880 },  // Iteminfo: Menu -> View
    };
    static const Site kName = { 0x517848, { 0x8B, 0x2C, 0x85, 0x20, 0x40, 0x65, 0x00 }, 7 };
    static const Site kX1 = { 0x517859, { 0x81, 0xC6, 0x8A, 0x01, 0x00, 0x00 }, 6 };
    static const Site kX2 = { 0x517885, { 0x81, 0xC6, 0x8A, 0x01, 0x00, 0x00 }, 6 };
    struct { DWORD va; int len; BYTE stock[4], want[4]; } p[2 + 1 + (int)(sizeof kDef / sizeof kDef[0])];
    char buf[32];
    int i, n = 0, done = 0, bad = 0;
    DWORD v;
    IniStr("Input", "PadNames", buf, sizeof buf);
    g_padForce = !_stricmp(buf, "Xbox") ? PADFAM_XBOX : !_stricmp(buf, "PlayStation") ? PADFAM_PS :
                 !_stricmp(buf, "Nintendo") ? PADFAM_NINTENDO : !_stricmp(buf, "Generic") ? PADFAM_GENERIC : -1;
    if (!SiteOk(&kName) || !SiteOk(&kX1) || !SiteOk(&kX2)) {
        Log("button config: the row draw 0x517848 / 0x517859 / 0x517885 is not stock (another mod?): stock names");
        return;
    }
    for (i = 0; i < (int)(sizeof kDef / sizeof kDef[0]); ++i) {
        const BYTE *s = (const BYTE *)(ULONG_PTR)kDef[i].va;
        if ((kDef[i].off == 1 ? s[0] != 0xB9 : (s[0] != 0xC7 || s[1] != 0x44 || s[2] != 0x24 || s[3] != kDef[i].disp)) ||
            *(const DWORD *)(s + kDef[i].off) != kDef[i].stock) {
            Log("button config: %08lX is not the stock Default immediate (another mod?): stock names", kDef[i].va);
            ++bad;
        }
    }
    if (bad) return;
    for (i = 0; i < PAD_N; ++i) g_padNameId[i] = MSG_PAD0 + i;
    // the patches: 4-byte operands only
    p[n].va = 0x51784B; v = (DWORD)(ULONG_PTR)g_padNameId; memcpy(p[n].want, &v, 4); ++n;  // name table
    p[n].va = 0x51785B; v = 0x178; memcpy(p[n].want, &v, 4); ++n;                           // name x 394 -> 376
    p[n].va = 0x517887; v = 0x178; memcpy(p[n].want, &v, 4); ++n;                           // "------" x
    for (i = 0; i < (int)(sizeof kDef / sizeof kDef[0]); ++i) {
        p[n].va = kDef[i].va + kDef[i].off;
        memcpy(p[n].want, &kDef[i].gog, 4);
        ++n;
    }
    for (i = 0; i < n; ++i) memcpy(p[i].stock, (const void *)(ULONG_PTR)p[i].va, 4);
    for (done = 0; done < n; ++done)
        if (!Poke(p[done].va, p[done].want, 4)) break;
    if (done < n) {  // all or nothing: put back what was written, each restore checked (fail closed: log)
        int lost = 0;
        for (i = 0; i < done; ++i)
            if (!Poke(p[i].va, p[i].stock, 4) || memcmp((const void *)(ULONG_PTR)p[i].va, p[i].stock, 4)) {
                ++lost;
                Log("button config: %08lX could not be put back", p[i].va);
            }
        Log("button config: a patch could not be written%s", lost ? "; some sites stay patched" : ": stock names");
        if (!lost) return;
    }
    g_padInstalled = 1;
    Log("button config: readable pad names (%s), pad column at x 376, Default = GOG's pad layout; %d patches",
        g_padForce >= 0 ? kPadFamName[g_padForce] : "Auto", n);
    g_padGlyphs = IniInt("Input", "PadGlyphs") != 0;  // v3.11.0
    GlyphInstall();
}
