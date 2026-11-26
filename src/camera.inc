// ================================================================ v3.33 CAM333: the over-the-shoulder camera (Option 6 > Camera)
// The user (2026-11-20): "The OTS camera will go into SH4 Refined now ... The switch will sit in Option 6 (Extra Menu)
// and replace 'Save Bank' at the bottom ... Camera will be a menu within Extra Menu and should have all the same
// settings that exist in the Playable Eileen Camera menu." Refined DEFERS to Playable Eileen's camera while that dll
// still has it. Plan and live checklist: staging/v333_feedback/w4_camera/PLAN.md. Pure code: camera_sim.h
// (test/camera_sim_test.c, which also compares it with a transcription of Playable Eileen's code).
//
// A port of Playable Eileen v1.18.2's [Camera] (SH4EileenWeapons.cpp OtsFilter / OtsRay / OtsFovSet / OtsZone*; KB 35,
// 77), itself the cheat table's "Over The Shoulder" v5 (entry 7880). The world camera hands its finished matrix to
// 0x4F7080 (CameraSetNextMatrix: copied to 0xFD5B00, drawn next frame). When that matrix is the rig's normal output
// 0x10843B0 (not a scripted shot 0x1084360, not Room 302's 0x10834D0) and the stage is not 1 (Room 302) or 0x0A, CamFilter
// returns its own matrix: own yaw turned by the look input (right stick 0x1236C54 / 0x1236C58, which the camera keys
// drive; Follow = her facing 0x10A0FC4), rotation 0x4FEEF0(out, pitch, yaw), the eye behind / beside / above the player
// 0x10A0F60 (Height above the feet, whatever the model: Henry or Playable Eileen's Eileen), wall zoom with two rays
// through the camera's own collision query 0x561CD0, the stair spring. Per-30 Hz-tick constants are scaled by the tick
// length ([0x5D5248], as Playable Eileen's RateK / RateExp).
// Sites (GOG v7; byte-checked; listed in SH4WM_Patches through Poke):
//   0x4F7080 `55 8B EC 8B 45 08` (the entry) -> jmp hkCamSet (6 bytes): required; the cheat table's entry 7880 AOBs the
//            same entry (it is saved unticked; with this hook in, its aobscan fails, as with Playable Eileen's)
//   0x50629D `E8 FE 0D FF FF` (the world rig's call 4F70A0, the FOV) -> call hkCamFov: optional ([Camera] Fov)
//   0x4CEA5A `E8 91 59 03 00` / 0x4CEA76 `E8 15 5B 03 00` (the room camera zones' shots 5043F0 / 504590) -> our calls:
//            optional, both or neither ([Camera] FixedCameras)
// The defer rule (camera_sim.h CamDecideOwner): Playable Eileen (dsound.dll, export SH4PE_Patches) patches in its DllMain;
// the order of the two DllMains is the loader's. At this dll's Init: the site already jumps into dsound.dll = Playable
// Eileen owns it; stock with Playable Eileen's patch list filled but without the site = its build has no camera:
// Refined's; stock, its list still empty and its image holding the camera (its log text "byte check failed at 0x4f7080",
// or version 1.18 or older) = wait: nothing is installed, and the first game tick (every DllMain has run) decides again.
// So there are never two hooks, in any load order; the log says who owns the view.
// v3.37 CP337 (USER_FEEDBACK_v3361 item 4, the user's choice "Own pause Camera page"): the settings left Options. Option
// 6's Camera row is the plain OTS / Game switch (CamRow6In); the twelve settings + Back are the pause menu's Extra Menu >
// Camera page (extramenu.inc pm_cam_*), which steps them with CamPauseStep (camera_sim.h CamStepRange / CamStepList) and
// shows these same texts (g_txtCam): one source of truth, one CamWriteKey (saved at once, as before). The Options page
// LP_CAM is gone (its registration, rows and the blue submenu cue). The world is not drawn under the pause (the frozen
// picture), so a change shows the moment play resumes.

#include "camera_sim.h"

static int HasExport(HMODULE mod, const char *name);  // sh4windowmod.c (below)
static DWORD ModuleVersion(HMODULE mod);              // sh4windowmod.c (below)

// ---------------------------------------------------------------- state
static CamCfg g_cam;                    // the clamped settings (game thread)
static float g_camRaw[CAMK_N];          // atof of each key's text (the menu shows these)
static char g_camTxt[CAMK_N][32];       // each key's text as in SH4Refined.ini
static volatile LONG g_camOwner = CAM_OWN_NONE;
static int g_camHooked, g_camFovOn, g_camZoneOn, g_camFault, g_camRechecked;
static CamState g_camSt;
static __declspec(align(16)) float g_camMat[16];   // matrices handed to game code are 16-byte aligned (movaps 0x402030)
static __declspec(align(16)) float g_camSeg[8];    // 0x561CD0 segment: from x y z 1, to x y z 1
static __declspec(align(16)) float g_camPlane[4];  // its plane out: nx ny nz d
static volatile LONG g_camShownTick;               // GetTickCount of the last frame our matrix was shown
static HMODULE g_camPe;                            // Playable Eileen's dsound.dll (with SH4PE_Patches), or NULL
static DWORD g_camPeLo, g_camPeHi, g_camUsLo, g_camUsHi;
static DWORD g_camPeVer;
static int g_camPeCode;                            // its image has the camera
typedef struct { DWORD magic, version, count; struct { DWORD va, len; } item[512]; } CamPeList;
static const CamPeList *g_camPeList;
static char g_camPeIni[MAX_PATH];
static volatile LONG g_camPeEnable = 1;            // its [Camera] Enable as last read (its default 1)
static const BYTE kCamStock[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
static const char *const kCamOwnerName[4] = { "nobody (the game's cameras)", "SH4 Refined", "Playable Eileen", "waiting for Playable Eileen" };

// ---------------------------------------------------------------- texts and the rows
#define LP_CAM_PARENT OP_EXTRA             // Option 6 (Extra Menu)  // v3.54 OL34: its origin page (layout.inc moves the row to Controls > Camera)
#define CAM_ROW6 (EM_N + 1)                // its Camera row (was Save Bank)
enum { CAMR_ENABLE, CAMR_DIST, CAMR_SIDE, CAMR_HEIGHT, CAMR_TILT, CAMR_TURN, CAMR_TILTSPD, CAMR_INVX, CAMR_INVY, CAMR_WALL,
       CAMR_MARGIN, CAMR_FIXED, CAMR_N };  // Playable Eileen's Camera folder, in its order; + Back
static const int kCamRowKey[CAMR_N] = { CAMK_ENABLE, CAMK_DISTANCE, CAMK_SIDE, CAMK_HEIGHT, CAMK_TILTDOWN, CAMK_TURNSPEED,
                                        CAMK_TILTSPEED, CAMK_INVERTX, CAMK_INVERTY, CAMK_WALLZOOM, CAMK_WALLMARGIN, CAMK_FIXED };
// v3.55 OL56 (R6, PLAN section 8): the pause Extra Menu > Camera page uses the names of Options > Controls > Camera / Steering
// (ol_words.h, from layout_data.py; were "Tilt down", "Turn speed", "Tilt speed", "Horizontal invert", "Vertical invert",
// "Wall zoom", "Wall margin", "Fixed cameras"), and their lines
static const char *const kCamRowLabel[CAMR_N + 1] = { OLW_CAMERA_CAMERA_L, OLW_CAMERA_DISTANCE_L, OLW_CAMERA_SIDE_L, OLW_CAMERA_HEIGHT_L,
                                                      OLW_CAMERA_TILT_DOWN_L, OLW_STEERING_TURN_SPEED_L, OLW_STEERING_TILT_SPEED_L,
                                                      OLW_STEERING_INVERT_TURN_L, OLW_STEERING_INVERT_TILT_L, OLW_STEERING_WALL_ZOOM_L,
                                                      OLW_STEERING_WALL_MARGIN_L, OLW_CAMERA_FIXED_ANGLES_L, OLW_BACK };
// v3.37 CP337: drawn on the pause page now (font 0, centred: at most 480 px, tools\pause_cam_layout.py), four shortened
static const char *const kCamRowDesc[CAMR_N + 1] = {  // v3.55 OL56: the Options rows' lines (each one line of at most 52)
    OLW_CAMERA_CAMERA_D, OLW_CAMERA_DISTANCE_D, OLW_CAMERA_SIDE_D, OLW_CAMERA_HEIGHT_D, OLW_CAMERA_TILT_DOWN_D,
    OLW_STEERING_TURN_SPEED_D, OLW_STEERING_TILT_SPEED_D, OLW_STEERING_INVERT_TURN_D, OLW_STEERING_INVERT_TILT_D,
    OLW_STEERING_WALL_ZOOM_D, OLW_STEERING_WALL_MARGIN_D, OLW_CAMERA_FIXED_ANGLES_D,
    "Back to the Extra Menu." };  // v3.37 CP337: the pause page
// range rows (Playable Eileen's lo / hi / step; 0 = a list row): Left / Right clamp at the ends, Enter steps up and wraps
static const float kCamLo[CAMR_N] = { 0.0f, 0.0f, 0.0f, 150.0f, 0.0f, 0.02f, 0.02f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static const float kCamHi[CAMR_N] = { 0.0f, 1000.0f, 0.0f, 400.0f, 40.0f, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f, 200.0f, 0.0f };
static const float kCamStep[CAMR_N] = { 0.0f, 25.0f, 0.0f, 10.0f, 1.0f, 0.02f, 0.02f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f };
// list rows (lists wrap; Enable / the switches match on zero / non-zero, as the camera reads them; Side on the number)
#define CAM_OPTMAX 13
static const char *const kCamOptName[CAMR_N][CAM_OPTMAX] = {
    { OLW_CAMERA_CAMERA_V_SHOULDER, OLW_CAMERA_CAMERA_V_GAME }, { NULL },  // v3.55 OL56: Shoulder (was "OTS"): the Options word; [Camera] Enable stays 1 / 0
    { "Left 150", "Left 125", "Left 100", "Left 75", "Left 50", "Left 25", "Centre", "Right 25", "Right 50", "Right 75",
      "Right 100", "Right 125", "Right 150" },
    { NULL }, { NULL }, { NULL }, { NULL }, { "Off", "On" }, { "Off", "On" }, { "On", "Off" }, { NULL }, { "Off", "On" } };
static const float kCamOptVal[CAMR_N][CAM_OPTMAX] = {
    { 1.0f, 0.0f }, { 0.0f },
    { -150.0f, -125.0f, -100.0f, -75.0f, -50.0f, -25.0f, 0.0f, 25.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f },
    { 0.0f }, { 0.0f }, { 0.0f }, { 0.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f }, { 0.0f, 1.0f } };
static const int kCamOptN[CAMR_N] = { 2, 0, 13, 0, 0, 0, 0, 2, 2, 2, 0, 2 };
#define CAM_VMAX 42                        // the most values a row has: Distance / Tilt down, 41 + Custom
// the text block MSG_CAM0.. (sh4windowmod.c): fixed ids first, then the rows' value ids (CamTexts hands them out)
enum { CAMT_ROWL, CAMT_ROWD, CAMT_ROWD_PE, CAMT_ROWD_NA, CAMT_ROWV0, CAMT_TITLE = CAMT_ROWV0 + 4, CAMT_L0,
       CAMT_D0 = CAMT_L0 + CAMR_N + 1, CAMT_VALS = CAMT_D0 + CAMR_N + 1, CAMT_MAX = 256 };
typedef char CamTextsFit[CAMT_MAX <= MSG_PERF0 - MSG_CAM0 ? 1 : -1];  // v3.37 MERGE: the block ends at MSG_PERF0
static BYTE g_txtCam[CAMT_MAX][64];  // v3.37 CP337: 64 (the Camera row's longer description)
#define CAM_TXT ((size_t)sizeof g_txtCam[0])
static DWORD g_camIds[CAMR_N][CAM_VMAX], g_camRowIds[4];
static int g_camN[CAMR_N];                 // values of each row (lists: options; ranges: grid points) + 1 Custom (Side, ranges)
static int g_camIdx[CAMR_N];               // each row's value now (g_camN[r] = Custom)
static int g_camCust[CAMR_N];              // the text index of each row's Custom value (-1 none)
static char g_camSnap[CAMR_N][32];         // the rows' texts as Options opened

static const BYTE *CamText(int id)
{
    const int i = id - MSG_CAM0;
    return i >= 0 && i < CAMT_MAX ? g_txtCam[i] : g_txtCam[CAMT_ROWL];
}
static int CamIsRange(int r) { return kCamStep[r] > 0.0f; }
static int CamGridN(int r) { return CamIsRange(r) ? (int)((kCamHi[r] - kCamLo[r]) / kCamStep[r] + 0.5f) + 1 : kCamOptN[r]; }
static int CamHasCustom(int r) { return CamIsRange(r) || r == CAMR_SIDE; }
static void CamFmt(char *out, size_t n, float v)
{
    sprintf_s(out, n, "%g", v);
    if (!strcmp(out, "-0")) strcpy_s(out, n, "0");
}

static void CamTexts(void)
{
    // v3.53 OL1: Option 6's row (the pause page keeps its own words until R6): OTS -> Shoulder; "Playable Eileen" (15
    // characters, it moved the value column) -> Eileen; the ini keeps [Camera] Enable 1 / 0
    static const char *const kRowV[4] = { OLW_CAMERA_CAMERA_V_SHOULDER, OLW_CAMERA_CAMERA_V_GAME, OLW_CAM_V_PE, OLW_CAM_V_NA };
    int r, v, t = CAMT_VALS;
    Encode(g_txtCam[CAMT_ROWL], CAM_TXT, OLW_CAMERA_CAMERA_L);
    Encode(g_txtCam[CAMT_ROWD], CAM_TXT, OLW_CAMERA_CAMERA_D);  // v3.37 CP337  // v3.53 OL1: (CamRow6Desc: a line per value + where the settings are)
    Encode(g_txtCam[CAMT_ROWD_PE], CAM_TXT, OLW_CAM_PE);  // v3.53 OL1
    Encode(g_txtCam[CAMT_ROWD_NA], CAM_TXT, OLW_NA_GAME);
    for (v = 0; v < 4; ++v) {
        Encode(g_txtCam[CAMT_ROWV0 + v], CAM_TXT, kRowV[v]);
        g_camRowIds[v] = MSG_CAM0 + CAMT_ROWV0 + v;
    }
    Encode(g_txtCam[CAMT_TITLE], CAM_TXT, "Camera");  // v3.37 CP337: the pause page's header
    for (r = 0; r <= CAMR_N; ++r) {
        Encode(g_txtCam[CAMT_L0 + r], CAM_TXT, kCamRowLabel[r]);
        Encode(g_txtCam[CAMT_D0 + r], CAM_TXT, kCamRowDesc[r]);
    }
    for (r = 0; r < CAMR_N; ++r) {
        const int n = CamGridN(r);
        g_camN[r] = n;
        g_camCust[r] = -1;
        for (v = 0; v < n && v < CAM_VMAX - 1 && t < CAMT_MAX; ++v, ++t) {
            char s[24];
            if (CamIsRange(r)) CamFmt(s, sizeof s, kCamLo[r] + (float)v * kCamStep[r]);
            else strcpy_s(s, sizeof s, kCamOptName[r][v]);
            Encode(g_txtCam[t], CAM_TXT, s);
            g_camIds[r][v] = MSG_CAM0 + t;
        }
        if (CamHasCustom(r) && t < CAMT_MAX) {
            g_camCust[r] = t;
            Encode(g_txtCam[t], CAM_TXT, "Custom");
            g_camIds[r][n] = MSG_CAM0 + t++;
        }
    }
    if (t > CAMT_MAX - 1) Log("camera: the text block is full (%d of %d)", t, CAMT_MAX);  // (cannot happen: 224 used)
}

// Each row's value index from the key's number now; a Custom value shows its number (ranges) or "Custom" (Side).
static void CamRefreshRows(void)
{
    int r;
    for (r = 0; r < CAMR_N; ++r) {
        const float raw = g_camRaw[kCamRowKey[r]];
        const int n = g_camN[r];
        int idx = n;
        if (CamIsRange(r)) {
            const float tt = (raw - kCamLo[r]) / kCamStep[r];
            const float k = floorf(tt + 0.5f);
            if (fabsf(tt - k) < 1e-3f && k >= 0.0f && k < (float)n) idx = (int)k;
        } else if (r == CAMR_SIDE) {
            int o;
            for (o = 0; o < n; ++o)
                if (fabsf(raw - kCamOptVal[r][o]) < 0.01f) idx = o;
        } else {
            int o;
            for (o = n - 1; o > 0 && (kCamOptVal[r][o] != 0.0f) != (raw != 0.0f); --o) {}
            idx = o;
        }
        if (idx >= n && g_camCust[r] >= 0) {
            char s[24];
            if (CamIsRange(r)) CamFmt(s, sizeof s, raw);
            else strcpy_s(s, sizeof s, "Custom");
            Encode(g_txtCam[g_camCust[r]], CAM_TXT, s);
        }
        if (idx >= n && g_camCust[r] < 0) idx = 0;
        g_camIdx[r] = idx;
    }
}

// The texts read: clamp as Playable Eileen does, log a change, restart behind the player when it was just turned on
static void CamApply(const char *why)
{
    CamCfg c;
    int k;
    for (k = 0; k < CAMK_N; ++k) g_camRaw[k] = (float)atof(g_camTxt[k]);
    CamCfgFrom(g_camRaw, &c);
    if (memcmp(&c, &g_cam, sizeof c)) {
        if (c.on && !g_cam.on) g_camSt.init = 0;  // switched on: start behind the player
        g_cam = c;
        Log("camera: %s (%s): distance %g side %g height %g tilt %g, smoothing %g, turn %g tilt %g speed, invert %d %d, "
            "follow %d, wall zoom %d (margin %g, out %g, in %g, ceiling %g), stairs %g, fov %g, fixed cameras %s",
            c.on ? "over the shoulder" : "the game's", why, c.dist, c.side, c.height, c.tiltDeg, c.smooth, c.turnSpeed,
            c.tiltSpeed, c.invX, c.invY, c.follow, c.wall, c.margin, c.zoomOut, c.zoomIn, c.ceil, c.hSmooth, c.fov,
            c.fixedCams ? "the game's" : "off");
    }
    CamRefreshRows();
}

// [Camera] of SH4Refined.ini: at Init through IniStr (a missing key is added with its default); later straight reads
// (an edit outside the game: the Options snapshot follows it, so "No" at the exit never undoes it, as [Gameplay])
static void CamReadIni(int atInit)
{
    int k, outside = 0;
    for (k = 0; k < CAMK_N; ++k) {
        char buf[32];
        if (atInit) {
            IniStr("Camera", kCamKey[k], buf, sizeof buf);
        } else {
            if (g_iniFallback) return;
            GetPrivateProfileStringA("Camera", kCamKey[k], g_camTxt[k], buf, sizeof buf, g_iniPath);
            if (strcmp(buf, g_camTxt[k])) {
                int r;
                ++outside;
                for (r = 0; r < CAMR_N; ++r)
                    if (kCamRowKey[r] == k && g_snapOn) strcpy_s(g_camSnap[r], sizeof g_camSnap[r], buf);
                LogV("camera: [Camera] %s read again: %s (was %s)", kCamKey[k], buf, g_camTxt[k]);
            }
        }
        strcpy_s(g_camTxt[k], sizeof g_camTxt[k], buf);
    }
    if (atInit || outside) CamApply(atInit ? "SH4Refined.ini" : "SH4Refined.ini edited");
}

// One key from the menu: saved at once (the value applies at once either way)
static void CamWriteKey(int k, const char *text)
{
    char was[32];
    if (k < 0 || k >= CAMK_N || !strcmp(g_camTxt[k], text)) return;
    strcpy_s(was, sizeof was, g_camTxt[k]);
    strcpy_s(g_camTxt[k], sizeof g_camTxt[k], text);
    if (!g_iniFallback && !WritePrivateProfileStringA("Camera", kCamKey[k], text, g_iniPath))
        Log("camera: [Camera] %s=%s applies now but could not be saved in SH4Refined.ini (error %lu)", kCamKey[k], text, GetLastError());
    else
        Log("camera: [Camera] %s=%s (was %s)", kCamKey[k], text, was);
    CamApply("menu");
}

// Playable Eileen's [Camera] into SH4Refined.ini: once at the first start of this version (the user's settings), and
// again whenever they change while its camera owns the view (so the takeover starts from the latest). Returns the keys written.
static int CamImportPe(const char *why)
{
    char sec[64], list[512];
    int k, n = 0, len = 0;
    if (g_iniFallback || GetPrivateProfileSectionA("Camera", sec, sizeof sec, g_camPeIni) == 0) return 0;
    list[0] = 0;
    for (k = 0; k < CAMK_N; ++k) {
        char v[32], cur[32];
        GetPrivateProfileStringA("Camera", kCamKey[k], "\x01", v, sizeof v, g_camPeIni);
        if (!strcmp(v, "\x01")) continue;  // missing there: ours stays
        GetPrivateProfileStringA("Camera", kCamKey[k], "\x01", cur, sizeof cur, g_iniPath);
        if (!strcmp(v, cur)) continue;
        if (!WritePrivateProfileStringA("Camera", kCamKey[k], v, g_iniPath)) continue;
        ++n;
        if (len < (int)sizeof list - 40) len += sprintf_s(list + len, sizeof list - len, "%s%s %s", n > 1 ? ", " : "", kCamKey[k], v);
    }
    if (n) Log("camera: [Camera] %s from SH4PlayableEileen.ini: %d key(s): %s", why, n, list);
    return n;
}

// ---------------------------------------------------------------- Playable Eileen and the hook site
static void CamModRange(HMODULE m, DWORD *lo, DWORD *hi)
{
    const BYTE *b = (const BYTE *)m;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(b + ((const IMAGE_DOS_HEADER *)b)->e_lfanew);
    *lo = (DWORD)(ULONG_PTR)b;
    *hi = *lo + nt->OptionalHeader.SizeOfImage;
}
// Its image holds the camera: the log text of its 004F7080 byte check (every build since v0.84 has it)
static int CamImageHas(HMODULE m, const char *needle)
{
    const BYTE *b = (const BYTE *)m;
    const size_t nl = strlen(needle);
    int found = 0;
    __try {
        const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(b + ((const IMAGE_DOS_HEADER *)b)->e_lfanew);
        const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
        WORD i;
        for (i = 0; i < nt->FileHeader.NumberOfSections && !found; ++i, ++s) {
            const BYTE *p = b + s->VirtualAddress;
            DWORD n = s->Misc.VirtualSize < s->SizeOfRawData || !s->SizeOfRawData ? s->Misc.VirtualSize : s->SizeOfRawData, j;
            if (!(s->Characteristics & IMAGE_SCN_MEM_READ) || (s->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) || n < nl) continue;
            for (j = 0; j + nl <= n; ++j)
                if (p[j] == (BYTE)needle[0] && !memcmp(p + j, needle, nl)) {
                    found = 1;
                    break;
                }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        found = 0;
    }
    return found;
}
static void CamProbePe(void)
{
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    g_camPe = NULL;
    g_camPeList = NULL;
    if (!ds || !HasExport(ds, "SH4PE_Patches")) return;
    g_camPe = ds;
    CamModRange(ds, &g_camPeLo, &g_camPeHi);
    g_camPeVer = ModuleVersion(ds);
    g_camPeList = (const CamPeList *)GetProcAddress(ds, "SH4PE_Patches");
    if (g_camPeList && g_camPeList->magic != 0x45504853) g_camPeList = NULL;
    g_camPeCode = CamImageHas(ds, "byte check failed at 0x4f7080") || (g_camPeVer && g_camPeVer <= 0x00010012);
}
static int CamPeRan(void) { return g_camPeList && g_camPeList->count > 0; }
// v3.34 PEC334: a game function SH4 Refined only CALLS (never patches) that Playable Eileen has hooked: a jmp into its
// dsound.dll at the entry, the entry inside a range of its SH4PE_Patches list. PE's hooks there keep the function's
// contract (0x5736D0 resolves its live pack's keys and passes the rest on, 0x553AA0 guards her animation slots,
// 0x4F55E0 answers the Eileen-weapon class), so calls through them are fine. v3.33 refused such a site when PE's dll had
// patched before SH4 Refined's start (a start-up order race), which left the Extra Menu, Supply Drops and the Eileen
// outfit out for the session. `who` names the feature for the one log line.
static int PeHookedCallee(DWORD va, const char *who)
{
    const BYTE *p = (const BYTE *)(ULONG_PTR)va;
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    const CamPeList *L;
    DWORD lo = 0, hi = 0, to, i, n;
    if (!ds || p[0] != 0xE9 || !HasExport(ds, "SH4PE_Patches")) return 0;
    CamModRange(ds, &lo, &hi);
    to = va + 5 + *(const DWORD *)(p + 1);
    if (to < lo || to >= hi) return 0;
    L = (const CamPeList *)GetProcAddress(ds, "SH4PE_Patches");
    if (!L || L->magic != 0x45504853) return 0;
    n = L->count > 512 ? 512 : L->count;
    for (i = 0; i < n; ++i)
        if (L->item[i].va <= va && va < L->item[i].va + L->item[i].len) {
            Log("%s: %08lX is hooked by Playable Eileen (a jmp into its dsound.dll, listed in its patches): SH4 Refined calls "
                "it through PE's hook", who, va);
            LhSiteForgive(va);  // v3.35 W0: not a failed byte check in the start-up summary
            return 1;
        }
    return 0;
}
static int CamPeListsSite(void)
{
    DWORD i, n;
    if (!g_camPeList) return 0;
    n = g_camPeList->count;
    if (n > 512) n = 512;
    for (i = 0; i < n; ++i)
        if (g_camPeList->item[i].va <= 0x4F7080 && 0x4F7080 < g_camPeList->item[i].va + g_camPeList->item[i].len) return 1;
    return 0;
}
static int CamSiteState(BYTE *b6)
{
    const BYTE *p = (const BYTE *)(ULONG_PTR)0x4F7080;
    DWORD to;
    memcpy(b6, p, 6);
    if (!memcmp(b6, kCamStock, 6)) return CAMS_STOCK;
    if (b6[0] != 0xE9) return CAMS_OTHER;
    to = 0x4F7085 + *(const DWORD *)(b6 + 1);
    if (g_camPe && to >= g_camPeLo && to < g_camPeHi) return CAMS_JMP_PE;
    if (to >= g_camUsLo && to < g_camUsHi) return CAMS_JMP_US;
    return CAMS_JMP_OTHER;
}
static const char *CamHex6(const BYTE *b)
{
    static char s[24];
    sprintf_s(s, sizeof s, "%02X %02X %02X %02X %02X %02X", b[0], b[1], b[2], b[3], b[4], b[5]);
    return s;
}
static void CamReadPeEnable(void)
{
    char v[32];
    if (!g_camPeIni[0]) return;
    GetPrivateProfileStringA("Camera", "Enable", "1", v, sizeof v, g_camPeIni);
    InterlockedExchange(&g_camPeEnable, atof(v) != 0.0);
}

// ---------------------------------------------------------------- the camera (game thread)
static int CamRayGame(void *ctx, const float *seg, float *plane)
{
    int r;
    (void)ctx;
    memcpy(g_camSeg, seg, sizeof g_camSeg);
    r = ((int(__cdecl *)(float *, float *))0x561CD0)(g_camSeg, g_camPlane);
    memcpy(plane, g_camPlane, sizeof g_camPlane);
    return r;
}
enum { CAMM_OTS, CAMM_FP, CAMM_OFF, CAMM_R302, CAMM_OTHER, CAMM_RESET };
static void CamMode(int mode, const void *m, DWORD stage)
{
    static int s_mode = -1;
    static volatile LONG s_q;
    static const char *const kWhat[6] = { "over the shoulder", "first person (a wall is too close)", "the game's (Camera Game)",
                                          "the game's (Room 302)", "the game's (a scripted shot or another camera)",
                                          "the game's for one frame (restarting behind the player)" };
    if (mode == s_mode) return;
    s_mode = mode;
    if (LogQuota(&s_q, 300)) LogV("camera: view %s (stage %lu, matrix %p)", kWhat[mode], stage, m);
}
static const float *CamFrame(const float *m, DWORD stage)
{
    static int s_wall, s_stair;
    static volatile LONG s_qw, s_qs, s_qa;
    CamState *const s = &g_camSt;
    const float *pos = (const float *)0x10A0F60;
    const float facing = *(volatile const float *)0x10A0FC4;
    const float rk = CamRateK(*(volatile const int *)0x5D5248);
    const float lx = *(volatile const float *)0x1236C54, ly = *(volatile const float *)0x1236C58;
    const unsigned coll = *(volatile const unsigned *)0x12F6D70;
    float hy, lag;
    int flags = 0;
    if (!CamLook(s, &g_cam, facing, lx, ly, rk)) {
        if (LogQuota(&s_qa, 20))
            LogAnomaly("camera", "non-finite angles (facing %g, look %g %g, yaw %g pitch %g): restarted behind the player",
                       facing, lx, ly, s->yaw, s->pitch);
        CamMode(CAMM_RESET, m, stage);
        return m;
    }
    ((void(__cdecl *)(float *, float, float))0x4FEEF0)(g_camMat, s->curP, s->cur);
    hy = CamHeight(s, &g_cam, pos, coll, rk);
    if (!CamEye(s, &g_cam, g_camMat, g_camMat + 8, pos, hy, coll, *(volatile const DWORD *)0x12F6D74 != 0, rk, CamRayGame, NULL,
                g_camMat + 12, &flags)) {
        if (LogQuota(&s_qa, 20))
            LogAnomaly("camera", "non-finite eye (player %g %g %g, height %g, distance %g side %g): the game's camera this frame",
                       pos[0], pos[1], pos[2], hy, g_cam.dist, g_cam.side);
        CamMode(CAMM_RESET, m, stage);
        return m;
    }
    {   // rate-limited notes for a tester: the wall zoom and the stair spring engaging
        const int wall = (flags & CAMF_WALL) != 0;
        if (wall != s_wall) {
            s_wall = wall;
            if (wall && LogQuota(&s_qw, 100))
                LogV("camera: wall zoom: a wall holds the eye at %.0f of %.0f (margin %.0f)%s", s->curB, g_cam.dist, g_cam.margin,
                     flags & CAMF_FP ? ", first person" : "");
            else if (!wall)
                LogT("camera: wall zoom released");
        }
        lag = hy - pos[1];
        if (!s_stair && fabsf(lag) > 20.0f) {
            s_stair = 1;
            if (LogQuota(&s_qs, 100)) LogV("camera: stair smoothing: the view trails the player's height by %.0f (StairSmoothing %g)", lag, g_cam.hSmooth);
        } else if (s_stair && fabsf(lag) < 2.0f) {
            s_stair = 0;
            LogT("camera: stair smoothing caught up");
        }
    }
    CamMode(flags & CAMF_FP ? CAMM_FP : CAMM_OTS, m, stage);
    InterlockedExchange(&g_camShownTick, (LONG)GetTickCount());
    return g_camMat;
}
// Called at the entry of 0x4F7080 with the matrix to show next frame; returns the matrix to show instead.
static const float *__cdecl CamFilter(const float *m)
{
    const DWORD stage = *(volatile const DWORD *)0xFD5A90;
    const float *r = m;
    if (!g_camHooked || g_camFault) return m;
    if (!g_cam.on) {
        CamMode(CAMM_OFF, m, stage);
        return m;
    }
    if (m != (const float *)0x10843B0) {
        CamMode(CAMM_OTHER, m, stage);
        return m;
    }
    if (stage == 1 || stage == 0x0A) {
        CamMode(CAMM_R302, m, stage);
        return m;
    }
    __try {
        r = CamFrame(m, stage);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_camFault = 1;  // fail closed: the game's camera for this session
        LogAnomaly("camera", "the over-the-shoulder camera faulted (code %08lX): the game's cameras for this session", GetExceptionCode());
        r = m;
    }
    return r;
}
static const DWORD g_camBack = 0x4F7086;
// 0x4F7080 (push ebp / mov ebp, esp / mov eax, [ebp+8]): the matrix argument goes through CamFilter
static __declspec(naked) void hkCamSet(void)
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 12]  // the matrix
        call CamFilter
        add esp, 4
        pop edx
        pop ecx
        mov [esp + 4], eax
        push ebp                   // the three stolen instructions
        mov ebp, esp
        mov eax, [ebp + 8]
        jmp dword ptr [g_camBack]
    }
}
// [Camera] Fov (Playable Eileen v1.01): the world rig sets the gameplay FOV each frame through 0x4F70A0 at 0x50629D
// (a scripted shot's while [0x1084350] is set, else the room record's times the run zoom). With this camera on, Fov
// replaces the rig's, so rooms and running no longer zoom it; scripted shots, cutscenes and Room 302 keep theirs. The
// call goes on to 0x4F70A0 (SH4 Refined's own FOV hook: Option 4's 3rd Person FOV still scales it, as with Playable Eileen).
static void __cdecl CamFovC(float fov)
{
    if (g_camHooked && !g_camFault && g_cam.on && g_cam.fov > 0.0f && !*(volatile const DWORD *)0x1084350) {
        const DWORD stage = *(volatile const DWORD *)0xFD5A90;
        if (stage != 1 && stage != 0x0A) fov = g_cam.fov * CAM_DEG;
    }
    ((void(__cdecl *)(float))0x4F70A0)(fov);
}
static __declspec(naked) void hkCamFov(void)  // 0x50629D: was `call 4F70A0` (cdecl float); ecx / edx kept as the stock setter does
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 12]  // the angle
        call CamFovC
        add esp, 4
        pop edx
        pop ecx
        ret
    }
}
// [Camera] FixedCameras (Playable Eileen v1.15): rooms place camera zone objects (type 0xB, tick 0x4CEA00) that set a
// one-shot scripted shot every frame while the player is inside: kind 1 eye / target through 0x5043F0 (call at 0x4CEA5A),
// kind 0 position / pitch / yaw through 0x504590 (0x4CEA76). With this camera showing and FixedCameras 0 both are skipped;
// every other scripted shot, cutscenes and Room 302 are untouched.
static int CamTakesZoneShot(void)
{
    static DWORD s_skips, s_tick;
    DWORD stage, now;
    if (!g_camHooked || g_camFault || !g_cam.on || g_cam.fixedCams) return 0;
    stage = *(volatile const DWORD *)0xFD5A90;
    if (stage == 1 || stage == 0x0A) return 0;
    ++s_skips;
    now = GetTickCount();
    if (now - s_tick > 10000) {  // at most one line per 10 s
        s_tick = now;
        LogV("camera: a room camera zone wanted the view; the over-the-shoulder camera kept it (%lu frame(s))", s_skips);
        s_skips = 0;
    }
    return 1;
}
static void __cdecl CamZoneShotEye(const float *eye, const float *target, float fov)  // replaces 0x5043F0 at 0x4CEA5A
{
    if (!CamTakesZoneShot()) ((void(__cdecl *)(const float *, const float *, float))0x5043F0)(eye, target, fov);
}
static void __cdecl CamZoneShotAngles(const float *pos, float pitch, float yaw, float fov)  // replaces 0x504590 at 0x4CEA76
{
    if (!CamTakesZoneShot()) ((void(__cdecl *)(const float *, float, float, float))0x504590)(pos, pitch, yaw, fov);
}

// ---------------------------------------------------------------- install (all or nothing per set, with rollback)
typedef struct { DWORD va; int len; BYTE b[8], stock[8]; } CamPatch;
static void CamBranch(CamPatch *p, DWORD va, BYTE op, const void *to, int len)
{
    const DWORD rel = (DWORD)(ULONG_PTR)to - (va + 5);
    p->va = va;
    p->len = len;
    memcpy(p->stock, (const void *)(ULONG_PTR)va, len);
    memset(p->b, 0x90, sizeof p->b);
    p->b[0] = op;
    memcpy(p->b + 1, &rel, 4);
}
static int CamApplySet(const CamPatch *p, int n, const char *what)
{
    int i, j;
    for (i = 0; i < n; ++i)
        if (!Poke(p[i].va, p[i].b, p[i].len) || memcmp((const void *)(ULONG_PTR)p[i].va, p[i].b, p[i].len)) {
            for (j = i; j >= 0; --j) Poke(p[j].va, p[j].stock, p[j].len);
            LogAnomaly("camera", "%s: %08lX could not be written: the set is rolled back", what, p[i].va);
            return 0;
        }
    return 1;
}
static void CamInstallHooks(const char *when)
{
    static const Site kFov = { 0x50629D, { 0xE8, 0xFE, 0x0D, 0xFF, 0xFF }, 5 };
    static const Site kZ1 = { 0x4CEA5A, { 0xE8, 0x91, 0x59, 0x03, 0x00 }, 5 }, kZ2 = { 0x4CEA76, { 0xE8, 0x15, 0x5B, 0x03, 0x00 }, 5 };
    CamPatch p[2];
    BYTE b6[6];
    if (g_camHooked) return;
    if (CamSiteState(b6) != CAMS_STOCK) {
        LogAnomaly("camera", "004F7080 is not stock (%s) when SH4 Refined's camera was to hook it: no camera", CamHex6(b6));
        InterlockedExchange(&g_camOwner, CAM_OWN_NONE);
        return;
    }
    CamBranch(&p[0], 0x4F7080, 0xE9, hkCamSet, 6);
    if (!CamApplySet(p, 1, "camera hook 004F7080")) {
        InterlockedExchange(&g_camOwner, CAM_OWN_NONE);
        return;
    }
    g_camSt.init = 0;
    g_camHooked = 1;
    if (SiteOk(&kFov)) {
        CamBranch(&p[0], 0x50629D, 0xE8, hkCamFov, 5);
        g_camFovOn = CamApplySet(p, 1, "camera fov 0050629D");
    } else {
        LogAnomaly("camera", "0050629D is not the stock `call 004F70A0` (another mod?): rooms still zoom the over-the-shoulder view");
    }
    if (SiteOk(&kZ1) && SiteOk(&kZ2)) {
        CamBranch(&p[0], 0x4CEA5A, 0xE8, CamZoneShotEye, 5);
        CamBranch(&p[1], 0x4CEA76, 0xE8, CamZoneShotAngles, 5);
        g_camZoneOn = CamApplySet(p, 2, "camera zones 004CEA5A / 004CEA76");
    } else {
        LogAnomaly("camera", "004CEA5A / 004CEA76 are not stock (another mod?): the rooms' fixed cameras still take the view");
    }
    Log("camera: SH4 Refined's over-the-shoulder camera is in (%s): 004F7080%s%s; [Camera] Enable %s", when,
        g_camFovOn ? ", Fov 0050629D" : "", g_camZoneOn ? ", camera zones 004CEA5A / 004CEA76" : "", g_cam.on ? "1" : "0");
}

// Who owns the view now (atTick: the first-tick re-check / the periodic check). Installs Refined's hooks when it is ours.
static void CamDecide(int atTick, const char *when)
{
    BYTE b6[6];
    int anomaly;
    const int site = CamSiteState(b6), was = (int)g_camOwner;
    const int peRan = CamPeRan(), peSite = CamPeListsSite();
    const int own = CamDecideOwner(site, g_camPe != NULL, g_camPeCode, peRan, peSite, atTick, &anomaly);
    if (own != was || !atTick) {
        static const char *const kSite[5] = { "stock", "a jmp into Playable Eileen's dsound.dll", "SH4 Refined's hook",
                                              "a jmp into another module", "other bytes" };
        LogV("camera: %s: the view belongs to %s (004F7080 %s [%s]; Playable Eileen %s v%lu.%lu%s, its patch list %lu range(s)%s)",
             when, kCamOwnerName[own], kSite[site], CamHex6(b6), g_camPe ? "loaded" : "not loaded", g_camPeVer >> 16,
             g_camPeVer & 0xFFFF, g_camPe ? (g_camPeCode ? " with its camera" : " without a camera") : "",
             g_camPeList ? g_camPeList->count : 0UL, peSite ? ", 004F7080 among them" : "");
        if (anomaly && site == CAMS_STOCK)
            LogAnomaly("camera", "Playable Eileen lists 004F7080 in its patches but the bytes are stock: nobody's camera hook is in");
        else if (anomaly)
            LogAnomaly("camera", "004F7080 holds another mod's code (%s; the cheat table's Over The Shoulder?): SH4 Refined's camera "
                       "stays out", CamHex6(b6));
    }
    InterlockedExchange(&g_camOwner, own);
    if (own == CAM_OWN_REFINED && !g_camHooked) {
        CamInstallHooks(when);  // also with Enable 0: the row turns it on live
        // a takeover in this session from a waiting / Playable Eileen state: the latest of its settings first
        if ((was == CAM_OWN_WAIT || was == CAM_OWN_PE) && CamImportPe("imported at the takeover") > 0) CamReadIni(0);
    }
    {
        const int now = (int)g_camOwner;  // (a failed install leaves nobody)
        if (now != was)
            Log("camera: the view belongs to %s%s", kCamOwnerName[now],
                now == CAM_OWN_PE ? ": its own camera menu and [Camera] in SH4PlayableEileen.ini apply; Option 6 shows its row greyed" :
                now == CAM_OWN_WAIT ? ": its dll has a camera and has not patched yet; decided again on the first game tick" : "");
    }
}

// Init (DllMain thread), before the Options tables are built
static void CameraInstall(void)
{
    char v[8];
    CamModRange(g_self, &g_camUsLo, &g_camUsHi);
    sprintf_s(g_camPeIni, sizeof g_camPeIni, "%sSH4PlayableEileen.ini", g_dir);
    CamProbePe();
    // the user's own settings, once: SH4Refined.ini made in this start, or one from before v3.33 (no [Camera] Enable)
    if (!g_iniFallback && (g_iniCreated || !GetPrivateProfileStringA("Camera", "Enable", "", v, sizeof v, g_iniPath))) {
        if (!CamImportPe("imported once")) LogV("camera: nothing to import (SH4PlayableEileen.ini has no [Camera]): SH4 Refined's defaults");
    }
    CamTexts();
    CamReadIni(1);
    CamReadPeEnable();
    CamDecide(0, "at start");
}

// ---------------------------------------------------------------- the dispatcher tick (0x4EF982, game thread)
// v3.33 REV333-CAM-001 (plan review 1): Playable Eileen's dll may load (and patch 004F7080) after SH4 Refined's start:
// look for it again before each ownership check, so its module range, version and patch list are current.
static void CamReprobe(const char *when)
{
    const HMODULE was = g_camPe;
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    if (was && ds == was) return;
    if (!was && !ds) return;
    CamProbePe();
    if (g_camPe != was) {
        Log("camera: %s: Playable Eileen's dll %s (v%lu.%lu%s)", when, g_camPe ? "found after start" : "gone",
            g_camPeVer >> 16, g_camPeVer & 0xFFFF, g_camPe ? (g_camPeCode ? ", with its camera" : ", without a camera") : "");
        CamReadPeEnable();
    }
}
static void CamIntegrity(void)
{
    static int s_gone, s_two, s_peGone, s_retry, s_late;
    BYTE b6[6];
    int site, own;
    CamReprobe("check");
    site = CamSiteState(b6);
    own = (int)g_camOwner;
    if (site == CAMS_JMP_PE && own != CAM_OWN_PE && s_late++ < 3) {  // REV333-CAM-001: it hooked after us
        CamDecide(1, "Playable Eileen hooked 004F7080 after start");
        return;
    }
    if (own == CAM_OWN_REFINED && g_camHooked && site != CAMS_JMP_US && !s_gone++)
        LogAnomaly("camera", "004F7080 is no longer SH4 Refined's hook (%s): another mod rewrote it", CamHex6(b6));
    if (own == CAM_OWN_REFINED && g_camHooked && CamPeListsSite() && !s_two++)
        LogAnomaly("camera", "two owners: Playable Eileen lists 004F7080 in its patches while SH4 Refined's hook is there");
    if (own == CAM_OWN_PE && site != CAMS_JMP_PE && !s_peGone++)
        LogAnomaly("camera", "Playable Eileen's camera hook at 004F7080 is gone (%s)", CamHex6(b6));
    if ((own == CAM_OWN_NONE || own == CAM_OWN_WAIT) && site == CAMS_STOCK && s_retry++ < 3) CamDecide(1, "the site is stock again");
}
static FILETIME g_camIniTime, g_camPeTime;
static void CamPoll(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!g_iniFallback && GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &a) && CompareFileTime(&a.ftLastWriteTime, &g_camIniTime)) {
        g_camIniTime = a.ftLastWriteTime;
        CamReadIni(0);
    }
    if (g_camPe && GetFileAttributesExA(g_camPeIni, GetFileExInfoStandard, &a) && CompareFileTime(&a.ftLastWriteTime, &g_camPeTime)) {
        const LONG was = g_camPeEnable;
        g_camPeTime = a.ftLastWriteTime;
        CamReadPeEnable();
        if (g_camOwner == CAM_OWN_PE) {
            if (was != g_camPeEnable) LogV("camera: Playable Eileen's camera %s ([Camera] Enable in SH4PlayableEileen.ini)", g_camPeEnable ? "on" : "off");
            if (CamImportPe("kept in step while Playable Eileen's camera owns the view") > 0) CamReadIni(0);
        }
    }
}
static void CamTick(void)
{
    static DWORD s_poll, s_check;
    const DWORD now = GetTickCount();
    __try {
        if (!g_camRechecked) {
            g_camRechecked = 1;
            CamReprobe("first game tick");  // v3.33 REV333-CAM-001
            CamDecide(1, "first game tick");
            if (g_camOwner == CAM_OWN_REFINED && g_camPe && g_camPeCode && !CamPeListsSite())
                LogV("camera: Playable Eileen's dll has a camera but SH4 Refined's hook was in first: Refined's camera owns "
                     "the view (one owner); PE's own camera rows do nothing this session");
        }
        if (now - s_poll >= 1000) {
            s_poll = now;
            CamPoll();
        }
        if (now - s_check >= 5000) {
            s_check = now;
            CamIntegrity();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("camera", "the camera's tick faulted (code %08lX)", GetExceptionCode());
    }
}

// ---------------------------------------------------------------- for other features (W3 No Clip's Fly)
// 1 while an over-the-shoulder camera is the gameplay view's choice: SH4 Refined's (its hook in, [Camera] Enable 1), or,
// while Refined defers, Playable Eileen's (its jmp at 004F7080 and its [Camera] Enable 1 in SH4PlayableEileen.ini, read
// once a second: up to 1 s late after a change in its menu). A setting-level answer, also true in the pause / Options
// (Fly's row greys on it); it does not see Room 302, scripted shots or cutscenes: CamOtsDriving does.
static int CamOtsOn(void)
{
    const int own = (int)g_camOwner;
    if (own == CAM_OWN_REFINED) return g_camHooked && !g_camFault && g_cam.on;
    if (own == CAM_OWN_PE) {
        BYTE b6[6];
        return g_camPeEnable && CamSiteState(b6) == CAMS_JMP_PE;
    }
    return 0;
}
// 1 while that camera drew the last gameplay frame. Refined's: its matrix was shown in the last 250 ms. Playable Eileen's
// (its state is not exported): CamOtsOn, not Room 302, no scripted shot pending ([0x1084350]) and the view set
// (0xFD5B00) differs from the rig's own matrix (0x10843B0), which is what its filter does.
static int CamOtsDriving(void)
{
    const int own = (int)g_camOwner;
    DWORD stage;
    if (own == CAM_OWN_REFINED) return CamOtsOn() && GetTickCount() - (DWORD)g_camShownTick < 250;
    if (own != CAM_OWN_PE || !CamOtsOn()) return 0;
    stage = *(volatile const DWORD *)0xFD5A90;
    if (stage == 1 || stage == 0x0A || *(volatile const DWORD *)0x1084350) return 0;
    return memcmp((const void *)0xFD5B00, (const void *)0x10843B0, 64) != 0;
}

// ---------------------------------------------------------------- the Options row and the pause page
// Left / Right / arrow clicks / Enter, as StepValue (0x524B60's tests and sound). 0 = nothing; *enter = Enter alone.
static int CamDir(int *enter)
{
    const int pad = ((Fn1)0x5542C0)(0), conf = ((Fn2)0x4F84A0)(0, 0x11);
    const int lc = *(int *)0x1092214 && ((Fn0)0x405490)(), rc = *(int *)0x1092218 && ((Fn0)0x405490)();
    *enter = 0;
    if (!conf && !lc && !rc && !(pad & 0x0C)) return 0;
    ((Fn1)0x56CC40)(0x9C43);  // the stock "value changed" sound
    if ((pad & 4) || lc) return -1;
    *enter = conf && !rc && !(pad & 8);
    return 1;
}
static int CamOurs(void) { return g_camOwner == CAM_OWN_REFINED && g_camHooked; }
// v3.37 CP337: row r's next value (dir -1 / +1; wrap 1 = Enter / a click: a range steps up and wraps), as Playable
// Eileen's PmStep writes it ("%g" of the option's value or grid point), saved at once. 1 = the value changed.
static int CamRowStep(int r, int dir, int wrap)
{
    const int k = (unsigned)r < CAMR_N ? kCamRowKey[r] : -1;
    char s[32], was[32];
    if (k < 0 || !CamOurs() || !dir) return 0;
    if (CamIsRange(r)) {
        CamFmt(s, sizeof s, CamStepRange(g_camRaw[k], kCamLo[r], kCamHi[r], kCamStep[r], dir, wrap));
    } else {
        const int n = kCamOptN[r];
        CamFmt(s, sizeof s, kCamOptVal[r][CamStepList(g_camIdx[r] >= n ? -1 : g_camIdx[r], n, dir)]);
    }
    strcpy_s(was, sizeof was, g_camTxt[k]);
    CamWriteKey(k, s);
    return strcmp(was, g_camTxt[k]) != 0;
}
// Option 6's Camera row: the OTS / Game switch (v3.37 CP337: Left / Right / Enter all switch it; the settings are the
// pause menu's Extra Menu > Camera). While Playable Eileen's camera owns the view: "Playable Eileen", greyed, inert;
// another mod on the site: "Unavailable".
static int __cdecl CamRow6In(BYTE *cfg, int count)
{
    int enter, dir;
    (void)cfg;
    (void)count;
    if (!CamOurs()) return 0;
    dir = CamDir(&enter);
    if (dir) CamWriteKey(CAMK_ENABLE, g_cam.on ? "0" : "1");
    return 0;
}
static int __cdecl CamRow6Get(void)
{
    const int own = (int)g_camOwner;
    if (own == CAM_OWN_REFINED && g_camHooked) return g_cam.on ? 0 : 1;
    return own == CAM_OWN_PE || own == CAM_OWN_WAIT ? 2 : 3;
}
static int __cdecl CamRow6Desc(void)
{
    const int own = (int)g_camOwner;
    if (own == CAM_OWN_REFINED && g_camHooked) return MSG_V53 + (g_cam.on ? OLT_CAM_VD_SHOULDER : OLT_CAM_VD_GAME);  // v3.53 OL1: a line per value
    return MSG_CAM0 + (own == CAM_OWN_PE || own == CAM_OWN_WAIT ? CAMT_ROWD_PE : CAMT_ROWD_NA);
}
static void CamFillRow(DWORD *r)  // Option 6, row EM_N + 1
{
    FillRow(r, CamRow6In, CamRow6Get, g_camRowIds, 4, MSG_CAM0 + CAMT_ROWL, CamRow6Desc);
}
// v3.37 CP337: the pause menu's Camera page (extramenu.inc) reads the rows through these: shown while SH4 Refined's
// camera owns the view (OTS or Game: its first row is the switch); r = 0 .. CAMR_N - 1 the settings, CAMR_N Back.
static int CamPauseOn(void) { return CamOurs(); }
static const BYTE *CamPauseTitle(void) { return g_txtCam[CAMT_TITLE]; }
static const BYTE *CamPauseLabel(int r) { return g_txtCam[CAMT_L0 + ((unsigned)r <= CAMR_N ? r : CAMR_N)]; }
static const BYTE *CamPauseDesc(int r) { return g_txtCam[CAMT_D0 + ((unsigned)r <= CAMR_N ? r : CAMR_N)]; }
static const BYTE *CamPauseValue(int r)  // the value shown now (the same text Option 6 and v3.33's page used)
{
    int i;
    if ((unsigned)r >= CAMR_N) return NULL;
    i = g_camIdx[r];
    if (i < 0 || i > g_camN[r] || (i == g_camN[r] && g_camCust[r] < 0)) i = 0;
    return CamText((int)g_camIds[r][i]);
}
static int CamPauseStep(int r, int dir, int wrap) { return CamRowStep(r, dir, wrap); }
// snapshot / changed / discard (ResetPages, PadChanged, RowChangedDraw, DiscardOwn)
static void CamSnapshot(void)
{
    int r;
    for (r = 0; r < CAMR_N; ++r) strcpy_s(g_camSnap[r], sizeof g_camSnap[r], g_camTxt[kCamRowKey[r]]);
    CamReadPeEnable();
}
static int CamRowChanged(int row)
{
    return g_snapOn && CamOurs() && row >= 0 && row < CAMR_N && strcmp(g_camSnap[row], g_camTxt[kCamRowKey[row]]) != 0;
}
static int CamChanged(void)
{
    int r;
    for (r = 0; r < CAMR_N; ++r)
        if (CamRowChanged(r)) return 1;
    return 0;
}
static void CamDiscard(void)
{
    int r;
    if (!g_snapOn || !CamOurs()) return;
    for (r = 0; r < CAMR_N; ++r)
        if (strcmp(g_camSnap[r], g_camTxt[kCamRowKey[r]])) CamWriteKey(kCamRowKey[r], g_camSnap[r]);
}
// The row draw's helpers: Option 6's Camera row is grey while Playable Eileen's (v3.37 CP337: no longer a submenu row)
static int CamRowGrey(int page, int row)  // v3.54 OL34: by the row's origin page (Option 6) and row
{
    return page == LP_CAM_PARENT && row == CAM_ROW6 && !CamOurs();
}
static int CamLongValW(int page, int row)  // "Playable Eileen" (15 characters) moves the value column
{
    int w;
    if (page != LP_CAM_PARENT || row != CAM_ROW6 || CamRow6Get() != 2) return 0;
    w = (int)(LabelW(g_txtCam[CAMT_ROWV0 + 2]) + 0.5f);
    return w > AF_COLW ? w : 0;  // v3.53 OL1: "Eileen" fits the column (only a long text moves it)
}
static int CamArrowShift(int page, int cur)  // -1 = not ours
{
    if (page == LP_CAM_PARENT && cur == CAM_ROW6) return CamRow6Get() == 3 ? 36 : 0;  // "Unavailable"
    return -1;
}
