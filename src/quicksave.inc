// ================================================================ v3.6.0: Quick Save / Quick Load (F5 / F9)
// Plan and review: staging/quick_save/PLAN.md (claudex-loop, Codex), RE: staging/quick_save/RE.md. All addresses are
// absolute VAs of the GOG v7 exe. Included by sh4windowmod.c after items.inc (uses Log, Site / SiteOk, PokeBranch,
// Encode, InFront, FovViewNow, IniInt / IniStr, g_dir).
//
// Quick save = the game's own snapshot routine 0x4FC0E0, which builds one full save-slot image (0x5A30 bytes) in the
// Continue buffer 0x107CD70 (the cheat table's Pause Menu+ quick save does the same, live-verified there); the buffer
// and its valid flag [0x13D1A30] are put back afterwards, so the game's own Continue is unchanged. The image plus a
// 64-byte header (game mode / All Weapons / save count, which a title Load takes from the catalog) is written to
// <game>\save\quick\qs1..3.bin: three rotating slots, a new save replaces an invalid or the oldest one, written to a
// temp file and renamed (MoveFileEx WRITE_THROUGH) so a failure never touches the others.
// Quick load = the title Load path: the image goes back into 0x107CD70, [0xFD5A74] = 1 makes the world start apply it
// (0x4FA4B0 -> 0x4FC5B0 / 0x4FC620) and 0x4F72B0(stage, room) requests the stage change.
// Both run on the game thread at the first call of the game tick (0x4EF982, `call 4F8BA0`), the moment the table's own
// quick save uses; keys are polled every frame from the HUD hook (items.inc SparkleFrame).

#define QS_IMG 0x5A30
#define QS_HDR 64
#define QS_SLOTS 3
#define QS_MAGIC 0x51344853  // 'SH4Q'
#include "qs_avail_sim.h"  // v3.33 MENU333: QsAvail (the Extra Menu rows), QsIsSoundPath (test\qs_avail_test.c)

#pragma pack(push, 1)
typedef struct {
    DWORD magic;
    WORD ver, hsize;
    DWORD seq;
    FILETIME ft;
    DWORD exeStamp, stage, room, gtime, mode, awUnlock, saveCount, difficulty, flags, reserved, crc;
} QsHeader;
#pragma pack(pop)

static int g_qsEnabled = 1, g_qsFeedback = 1, g_qsInstalled;
static int g_qsTickOn;            // v3.7.0: the 0x4EF982 dispatcher is in place (quick save and / or the Extra Menu)
static volatile LONG g_ovlTick = -2;  // v3.18.0 N5: the overlay 0x4F9110 dispatches this tick (snapshot after 0x4F8BA0)
static void(__cdecl *g_extraTick)(void);  // v3.7.0: the Extra Menu engine (extramenu.inc), run first every tick
static int g_qsAnyValid;          // v3.7.0: a valid quick save exists (refreshed by QsAnyValid)
static int g_qsLoadSay;           // v3.7.0: show "Quick loaded" when the running restart ends (not after Load Game)
static int g_qsSaveVk = VK_F5, g_qsLoadVk = VK_F9;
static char g_qsDir[MAX_PATH];
static int g_qsReq;              // 1 quick save, 2 quick load (latched by QsPoll, run by QsTick; game thread only)
static DWORD g_qsReqTick;
static int g_qsLoading;          // a quick load was started: until the world start has applied it
static int g_qsPrevSave, g_qsPrevLoad;
static BYTE g_qsFile[QS_HDR + QS_IMG], g_qsBak[QS_IMG], g_qsProbe[QS_HDR + QS_IMG];  // static: 23 KB each
static DWORD g_qsLoadTick;
static BYTE g_qsMsg[64];
static DWORD g_qsMsgUntil, g_qsMsgCol;

static const DWORD kQsSections[12] = { 0x80, 0x1C0, 0x6E0, 0x2270, 0x36C0, 0x46C0, 0x5170, 0x51D0, 0x51E0, 0x57D0,
                                       0x5A00, 0x5A20 };  // image +0x2C, written by 0x4FC0E0

static DWORD QsCrc(const BYTE *p, size_t n, DWORD crc)
{
    size_t i;
    int k;
    crc = ~crc;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static DWORD QsExeStamp(void)
{
    const BYTE *b = (const BYTE *)0x400000;
    return *(const DWORD *)(b + *(const DWORD *)(b + 0x3C) + 8);  // IMAGE_FILE_HEADER.TimeDateStamp
}

static void QsSay(const char *text, int ok)
{
    if (!g_qsFeedback) return;
    Encode(g_qsMsg, sizeof g_qsMsg, text);
    g_qsMsgCol = ok ? 0x808080 : 0x303080;  // 0x00BBGGRR, 0x80 = full: white / red
    g_qsMsgUntil = GetTickCount() + 2000;
}

static void QsSound(int ok) { ((void(__cdecl *)(int))0x56CC40)(ok ? 0x9C48 : 0x9C44); }

static DWORD g_qsFaultCode;
static void *g_qsFaultAt;
static void QsFaultNote(EXCEPTION_POINTERS *e)
{
    g_qsFaultCode = e->ExceptionRecord->ExceptionCode;
    g_qsFaultAt = e->ExceptionRecord->ExceptionAddress;
}

static void QsPath(int slot, const char *ext, char *out, size_t n)
{
    sprintf_s(out, n, "%s\\qs%d.%s", g_qsDir, slot + 1, ext);
}

// A whole file is valid: header fields, exe, CRC, the image's own size field / section table / place. Returns 1 and
// fills buf (QS_HDR + QS_IMG bytes).
static int QsReadSlot(int slot, BYTE *buf)
{
    char path[MAX_PATH];
    HANDLE h;
    DWORD got = 0;
    const QsHeader *hd = (const QsHeader *)buf;
    const BYTE *img = buf + QS_HDR;
    int ok;
    QsPath(slot, "bin", path, sizeof path);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = ReadFile(h, buf, QS_HDR + QS_IMG, &got, NULL) && got == QS_HDR + QS_IMG && GetFileSize(h, NULL) == got;
    CloseHandle(h);
    if (!ok) return 0;
    if (hd->magic != QS_MAGIC || hd->ver != 1 || hd->hsize != QS_HDR || hd->exeStamp != QsExeStamp()) return 0;
    if (QsCrc(img, QS_IMG, QsCrc(buf, QS_HDR - 4, 0)) != hd->crc) return 0;
    if (*(const DWORD *)img != QS_IMG || memcmp(img + 0x2C, kQsSections, sizeof kQsSections)) return 0;
    if (hd->stage != *(const DWORD *)(img + 0x14) || hd->room != *(const DWORD *)(img + 0x18)) return 0;
    if (hd->stage < 1 || hd->stage > 0x11 || hd->stage == 9 || hd->stage == 0x0A) return 0;
    if (hd->room < 1 || hd->room > 0xFF) return 0;
    return 1;
}

// v3.33 MENU333: the game's read queue holds a DATA file (0x13C7850 busy flag, 32 entries of 0x214 bytes at 0x13C7854:
// +0 used, +0x10 path; SH4Decomp snd_stream.h). v3.32 asked 0x573370(1) "is the queue empty", but the music / ambience
// streams refill through the same queue (0x96000-byte reads of bgm.sh4 / se.sh4, also in the pause): the Quick Save /
// Quick Load rows went grey for every refill and stayed grey where they come back to back. Sound packs ("*.sh4") never
// touch a save image or a world restart, so only the other files count. what: the file's name (for the log).
static int QsFilesLoading(char *what, size_t n)
{
    int i;
    if (what && n) what[0] = 0;
    if (!*(volatile int *)0x13C7850) return 0;
    for (i = 0; i < 32; ++i) {
        const BYTE *e = (const BYTE *)(ULONG_PTR)(0x13C7854 + 0x214 * i);
        const char *path = (const char *)(e + 0x10);
        const char *b;
        if (!*(const volatile int *)e || QsIsSoundPath(path, 0x200)) continue;
        if (what && n) {
            b = path + strnlen(path, 0x1FF);
            while (b > path && b[-1] != '\\' && b[-1] != '/') --b;
            strncpy_s(what, n, b, _TRUNCATE);
        }
        return 1;
    }
    return 0;
}

// The guards (PLAN 4.3): 0 = allowed, else the reason. load: F9 (the checkpoint gate and the "Henry busy" states do
// not apply, except scripted control and a frozen player update). v3.7.0 inPause: the Extra Menu's Quick Save row
// runs inside the pause menu (sub-mode 2, nothing pending; the game phase is 2 and player input is off there), the
// other tests read the state frozen at the moment of the pause (the table's Pause Menu+ saved there the same way).
// v3.33 MENU333: split. QsGuardHard = the state itself (where Henry is, what runs); closing = the pause is closing for
// the Extra Menu's Quick Load (the sub-mode, phase and input tests are left out: they settle within frames).
// QsGuardPassing = what ends by itself within frames (a fade, a data file loading, a message): the Extra Menu rows do
// not go grey for it, a confirm or a key waits for it (QS_WAIT_MS / 1 s).
static const char *QsGuardHard(int load, int inPause, int closing)
{
    const int stage = *(volatile int *)0xFD5A90, room = *(volatile int *)0xFD5A94;
    const int act = *(volatile int *)0x10A1134;
    if (g_qsLoading) return "a quick load is still running";
    if (closing) {
    } else if (inPause) {
        if (*(volatile int *)0x107BF54 != 2 || *(volatile int *)0x107BF58) return "not in the pause menu";
    } else {
        if (*(volatile int *)0x107BF54 || *(volatile int *)0x107BF58) return "a menu or overlay is open";
        if (*(volatile int *)0xFD5C20) return "not in plain gameplay (cutscene or overlay)";
    }
    if (!*(volatile int *)0x107BF50 && !closing) return "the game does not allow menus now (event, door, interaction)";
    if (*(volatile int *)0xFD5AA0 || *(volatile int *)0xFD5AA4) return "a room change is pending";
    if (*(volatile int *)0xFD5BB0 || *(volatile int *)0xFD5BB4 || *(volatile int *)0xFD5BB8) return "death, game over or ending";
    if (!(*(volatile float *)0x10A0F28 > 0.0f) || act == 0x64 || act == 0x65 || *(volatile int *)0x10A1C34 == 0x0A)
        return "Henry is dying";
    if (*(volatile int *)0xFD5C28) return "an interaction runs";
    if (*(volatile int *)0x10A1914) return "the player is frozen by an event";
    if (!inPause && !closing && !*(volatile BYTE *)0x1079C30) return "player input is off";
    if (!load && !(*(volatile BYTE *)0x13D815B & 0x20)) return "the story does not allow saving here yet";
    if (*(volatile BYTE *)0x13D8113 & 0x20) return "the elevator is moving";
    if (stage == 9) return "the final rooms";
    if (stage == 0x0A) return "the hole tunnel";
    // the fight is in stage 0x0B room 7 only; FD8188 bits 0-2 are monotonic door flags ("Wall Boss Exit Door 2 / 3",
    // "Wall Boss Fight Door", research flags_derived.tsv): they stay set after the fight, so they are no guard (in-game
    // test: byte 05 refused every later room)
    if (stage == 0x0B && room == 7) return "the Wall Man fight";
    if (stage == 2 && room == 0x0D) return "the Wall Men room";
    if (*(volatile int *)0x10A1138 || *(volatile int *)0x10A1E98) return "Henry is under scripted control";
    if (!load && ((act != 0 && act != 1 && act != 2) || *(volatile int *)0x10A1140 || *(volatile int *)0x10A1148 ||
                  *(volatile int *)0x10A12D0 || *(volatile int *)0x10A1E84))
        return "Henry is busy (attack, hit, grab)";
    if (stage == 1 && (FovViewNow() || *(volatile int *)0x13D7B14 == 1)) return "a Room 302 view runs";
    return NULL;
}
static char g_qsPassWhat[64];  // v3.33 MENU333: the data file QsGuardPassing saw (log only)
static const char *QsGuardPassing(void)
{
    if (*(volatile int *)0x137F98C || *(volatile int *)0x137F990) return "the screen is fading";
    if (QsFilesLoading(g_qsPassWhat, sizeof g_qsPassWhat)) return "a data file is loading";
    if (*(volatile int *)0x108CE78) return "a message is showing";
    return NULL;
}
static const char *QsGuard(int load, int inPause)
{
    const char *why = QsGuardHard(load, inPause, 0);
    return why ? why : QsGuardPassing();
}

static void QsSaveEx(int inPause)
{
    const char *why = QsGuard(0, inPause);
    QsHeader *hd = (QsHeader *)g_qsFile;
    BYTE *img = g_qsFile + QS_HDR;
    BYTE *const probe = g_qsProbe;
    DWORD seq = 0, best = 0xFFFFFFFF, valid, wrote = 0, t0, err = 0;
    char tmp[MAX_PATH], dst[MAX_PATH];
    int k, target = -1, ok;
    HANDLE h;
    if (why) {
        Log("quick save: refused: %s%s%s", why, strcmp(why, "a data file is loading") ? "" : ": ",  // v3.33 MENU333
            strcmp(why, "a data file is loading") ? "" : g_qsPassWhat);
        QsSound(0);
        QsSay("Quick save not possible here", 0);
        return;
    }
    // the game's own snapshot into its Continue buffer, then the buffer and its flag back
    memcpy(g_qsBak, (const void *)0x107CD70, QS_IMG);
    valid = *(volatile DWORD *)0x13D1A30;
    ok = 0;
    __try {  // review QS-001 (inspection): the Continue buffer and its flag are put back even if the snapshot faults
        __try {
            ((void(__cdecl *)(void))0x4FC0E0)();
            memcpy(img, (const void *)0x107CD70, QS_IMG);
            ok = 1;
        } __finally {
            memcpy((void *)0x107CD70, g_qsBak, QS_IMG);
            *(volatile DWORD *)0x13D1A30 = valid;
        }
    } __except (QsFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
    }
    if (!ok) {
        Log("quick save: the game's snapshot faulted (code %08lX at %p); Continue snapshot restored, nothing written",
            g_qsFaultCode, g_qsFaultAt);
        QsSound(0);
        QsSay("Quick save failed", 0);
        return;
    }
    // the slot: an invalid one first, else the oldest (lowest sequence); the new sequence follows the highest
    for (k = 0; k < QS_SLOTS; ++k) {
        if (!QsReadSlot(k, probe)) {
            if (target < 0 || best != 0) { target = k; best = 0; }
            continue;
        }
        if (((QsHeader *)probe)->seq > seq) seq = ((QsHeader *)probe)->seq;
        if (best != 0 && ((QsHeader *)probe)->seq < best) { best = ((QsHeader *)probe)->seq; target = k; }
    }
    if (target < 0) target = 0;
    memset(hd, 0, QS_HDR);
    hd->magic = QS_MAGIC;
    hd->ver = 1;
    hd->hsize = QS_HDR;
    hd->seq = seq + 1;
    GetSystemTimeAsFileTime(&hd->ft);
    hd->exeStamp = QsExeStamp();
    hd->stage = *(const DWORD *)(img + 0x14);
    hd->room = *(const DWORD *)(img + 0x18);
    hd->gtime = *(const DWORD *)(img + 0x04);
    hd->mode = *(volatile DWORD *)0x13D1AA0;
    hd->awUnlock = *(volatile DWORD *)0x13D1A68;
    hd->saveCount = *(volatile DWORD *)0x13D1A90;
    hd->difficulty = *(const DWORD *)(img + 0x0C);
    hd->crc = QsCrc(img, QS_IMG, QsCrc(g_qsFile, QS_HDR - 4, 0));
    // synchronous, committed before any success is shown (review QS-001)
    t0 = GetTickCount();
    CreateDirectoryA(g_qsDir, NULL);
    QsPath(target, "tmp", tmp, sizeof tmp);
    QsPath(target, "bin", dst, sizeof dst);
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ok = h != INVALID_HANDLE_VALUE;
    if (ok) {
        ok = WriteFile(h, g_qsFile, QS_HDR + QS_IMG, &wrote, NULL) && wrote == QS_HDR + QS_IMG && FlushFileBuffers(h);
        if (!ok) err = GetLastError();
        CloseHandle(h);
    } else {
        err = GetLastError();
    }
    if (ok && !MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = GetLastError();
        ok = 0;
    }
    if (!ok) {
        DeleteFileA(tmp);
        Log("quick save: writing %s failed (error %lu); the other slots are unchanged", dst, err);
        QsSound(0);
        QsSay("Quick save failed", 0);
        return;
    }
    Log("quick save: slot %d (sequence %lu), stage %lu room %lu, game time %lu, written in %lu ms%s", target + 1,
        hd->seq, hd->stage, hd->room, hd->gtime, GetTickCount() - t0, inPause ? " (pause menu)" : "");
    g_qsAnyValid = 1;
    QsSound(1);
    QsSay("Quick saved", 1);
}
static void QsSave(void) { QsSaveEx(0); }

// v3.7.0: is there a valid quick save at all (reads the 3 files; not for every frame)
static int QsAnyValid(void)
{
    int k;
    g_qsAnyValid = 0;
    for (k = 0; k < QS_SLOTS && !g_qsAnyValid; ++k) g_qsAnyValid = QsReadSlot(k, g_qsProbe);
    return g_qsAnyValid;
}

// v3.7.0: the world restart from a slot image (the table's sa_restart), shared by Quick Load and the Extra Menu's
// Load Game. hd = the quick-save header with the session values a title Load takes from the catalog, or NULL when the
// game's own load menu has already set them. Returns 0 (nothing changed) when every catalog entry is clear data.
static int QsRestart(BYTE *img, const QsHeader *hd)
{
    const int stage = *(const int *)(img + 0x14), room = *(const int *)(img + 0x18);
    // a title Load of a slot runs with the clear-data test of the current catalog entry: when that entry is clear data
    // (an NG+ session), point it at the first entry that is not
    if (((int(__cdecl *)(void))0x56F540)()) {
        const BYTE *cat = ((const BYTE *(__cdecl *)(int))0x4FBCE0)(0);
        int n, found = -1;
        for (n = 0; n < 30 && found < 0; ++n)
            if (*(const DWORD *)(cat + 0x20 + n * 0x30 + 0x10) != 0x12) found = n;
        if (found < 0) return 0;
        *(volatile int *)0x13D1A9C = found;
    }
    if (hd) {  // the session values a title Load takes from the catalog
        *(volatile DWORD *)0x13D1AA0 = hd->mode;
        *(volatile DWORD *)0x13D1A68 = hd->awUnlock;
        *(volatile DWORD *)0x13D1A90 = hd->saveCount;
    }
    // system data rides in the image header and 0x4FC5B0 writes it back: never take back newer unlocks
    *(WORD *)(img + 0x10) |= *(volatile WORD *)0x13D1A98;
    img[0x1C] |= *(volatile BYTE *)0x13D1A50;
    img[0x1D] |= *(volatile BYTE *)0x13D1A38;
    if (*(volatile WORD *)0x13D1A74 > *(WORD *)(img + 0x1E)) *(WORD *)(img + 0x1E) = *(volatile WORD *)0x13D1A74;
    memcpy((void *)0x107CD70, img, QS_IMG);
    *(volatile BYTE *)0x107CE63 &= 0xDF;  // image +0xF3 = story byte 13D8113: no elevator ride
    *(volatile DWORD *)0x13D1A30 = 1;
    *(volatile DWORD *)0xFD5BE0 = 0;      // the room checkpoint must not overwrite 0x107CD70 now
    *(volatile DWORD *)0xFD5A74 = 1;      // world start applies the image like a title Load
    ((void(__cdecl *)(int, int))0x4F72B0)(stage, room);
    g_qsLoading = 1;
    g_qsLoadTick = GetTickCount();
    g_qsLoadSay = 0;  // QsLoad sets it; the Extra Menu's Load Game restarts quietly
    return 1;
}

static void QsLoad(void)
{
    const char *why = QsGuard(1, 0);
    BYTE *const probe = g_qsProbe;
    const QsHeader *hd = (const QsHeader *)g_qsFile;
    BYTE *img = g_qsFile + QS_HDR;
    DWORD best = 0;
    int k, from = -1;
    if (why) {
        Log("quick load: refused: %s%s%s", why, strcmp(why, "a data file is loading") ? "" : ": ",  // v3.33 MENU333
            strcmp(why, "a data file is loading") ? "" : g_qsPassWhat);
        QsSound(0);
        QsSay("Quick load not possible here", 0);
        return;
    }
    for (k = 0; k < QS_SLOTS; ++k)
        if (QsReadSlot(k, probe) && ((QsHeader *)probe)->seq > best) {
            best = ((QsHeader *)probe)->seq;
            from = k;
            memcpy(g_qsFile, probe, QS_HDR + QS_IMG);
        }
    if (from < 0) {
        g_qsAnyValid = 0;
        Log("quick load: no valid quick save in %s", g_qsDir);
        QsSound(0);
        QsSay("No quick save", 0);
        return;
    }
    g_qsAnyValid = 1;
    if (!QsRestart(img, hd)) {
        Log("quick load: refused: every catalog entry is clear data");
        QsSound(0);
        QsSay("Quick load not possible here", 0);
        return;
    }
    g_qsLoadSay = 1;
    Log("quick load: slot %d (sequence %lu), stage %lu room %lu, game time %lu", from + 1, hd->seq, hd->stage, hd->room,
        hd->gtime);
    QsSound(1);
}

// 0x4EF982: was `call 4F8BA0`; v3.7.0 the shared dispatcher: the Extra Menu engine, a pending quick save / load,
// then the stock call (its eax is returned)
static int __cdecl QsTick(void)
{
    const int req = g_qsReq;
    g_qsReq = 0;
    if (g_extraTick) {
        BgPush(BGC_MENU);  // v3.35 FX335: the frame ledger (budget.inc)
        __try {
            g_extraTick();
        } __except (QsFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
            static int faults;
            if (++faults <= 3) Log("extra menu: the engine faulted (code %08lX at %p)", g_qsFaultCode, g_qsFaultAt);
        }
        BgPop();  // v3.35 FX335
    }
    if (g_vibTick) {  // v3.14.0: the vibration test trigger ([Input] VibrationTest=1)
        __try {
            g_vibTick();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_vibTick = NULL;
            Log("vibration: the test trigger faulted; off");
        }
    }
    GpIniPoll();  // v3.25.0 (review REV-325-007): [Gameplay] read again on an ini edit
    // v3.35 FX335: each tick part timed in the frame ledger (budget.inc BG_RUN)
    BG_RUN(BGC_T_DP, DpTick());     // v3.27.0: Death Presence (deathpresence.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_AI, AiTick());     // v3.29 AFTERIMAGE: the after-image stun, after Death Presence (afterimage.inc; its own __try)
    BG_RUN(BGC_T_ECHO, EchoTick());   // v3.30 ECHO: the Echo double (echo.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_ENM, EnmTick());     // v3.31 ENEMIES: Enemy Copies / Enemies Follow (enemies.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_OTHER, GtTick());    // v3.43 GT343: Ghost Teleport (teleport.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_DROP, DropTick());   // SUPPLY DROPS (drops.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_OTHER, RgTick());    // v3.47 RG347: Refined's own Regeneration (regen.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_OTHER, PkTick());     // v3.32 PM332: the downed-ghost pickup window (pickup.inc; its own __try, fail-closed)
    BG_RUN(BGC_T_OTHER, PpTick());     // v3.35 PK335: the pickup pool's per-room line (pickpool.inc; its own __try)
    BG_RUN(BGC_T_OTHER, PoolsTick());  // v3.36 TP336: the task / IK pools' use and per-room line (pools.inc; its own __try)
    BG_RUN(BGC_T_OTHER, InvTick());    // v3.37 INV337: Extended Toolbelt / Stackable Items switching and watch (inventory.inc; its own __try)
    BG_RUN(BGC_T_OTHER, HpTick());     // v3.36 HE336: the hit-shape pool's free count (hitpool.inc; its own __try)
    BG_RUN(BGC_T_OTHER, EvTick());     // v3.36 HE336: the enemy records' per-room line, after EnmTick's purge (recevict.inc)
    BG_RUN(BGC_T_CAM, CamTick());    // v3.33 CAM333: the camera's owner re-check, ini polls, hook integrity (camera.inc; its own __try)
    if (g_bldTick) {  // v3.25.0: Blood Colour without its reader patches: keep the saved value stock (review REV-008)
        __try {
            g_bldTick();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_bldTick = NULL;
        }
    }
    if (g_subTick) {  // v3.13.0: the subtitle test trigger ([Subtitles] Test=1)
        __try {
            g_subTick();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_subTick = NULL;
            Log("subtitles: the test trigger faulted; off");
        }
    }
    if (req && g_qsInstalled && GetTickCount() - g_qsReqTick < 700 && !QsGuardHard(req == 2, 0, 0) &&
        QsGuardPassing()) {  // v3.33 MENU333: a fade / data file / message ends within frames: try again next tick
        static LONG s_q;
        if (!g_qsReq) g_qsReq = req;
        if (LogQuota(&s_q, 100)) LogT("quick %s: waiting: %s", req == 1 ? "save" : "load", QsGuardPassing());
    } else if (req && g_qsInstalled && GetTickCount() - g_qsReqTick < 1000) {
        __try {
            if (req == 1) QsSave();
            else QsLoad();
        } __except (QsFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
            Log("quick %s: faulted (code %08lX at %p)", req == 1 ? "save" : "load", g_qsFaultCode, g_qsFaultAt);
        }
    }
    {   // v3.18.0 N5 (RE_menu_polish 1.3 A): the overlay manager's promotion rule, read right before 0x4EF987 calls it
        const int r = ((int(__cdecl *)(void))0x4F8BA0)();
        const int o54 = *(volatile int *)0x107BF54, o58 = *(volatile int *)0x107BF58;
        InterlockedExchange(&g_ovlTick, o58 != 0 && o54 != -1 && o54 != o58 ? o58 : o54);
        return r;
    }
}

// Every in-game frame from the HUD hook (game thread): keys, the end of a quick load, the feedback line.
static void QsFrame(void)
{
    int s, l, mods;
    if (!g_qsTickOn) return;
    if (g_qsLoading && !*(volatile int *)0xFD5A74 && !*(volatile int *)0xFD5AA0 && !*(volatile int *)0xFD5C20) {
        g_qsLoading = 0;  // the world start applied the image (it clears FD5A74) and gameplay runs
        if (g_qsLoadSay) QsSay("Quick loaded", 1);
    } else if (g_qsLoading && GetTickCount() - g_qsLoadTick > 30000) {
        g_qsLoading = 0;  // never seen to end: do not block quick saves for the rest of the session
        Log("quick load: no world start seen within 30 s; quick save / load allowed again");
    }
    s = (GetAsyncKeyState(g_qsSaveVk) & 0x8000) != 0;
    l = (GetAsyncKeyState(g_qsLoadVk) & 0x8000) != 0;
    mods = (GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_SHIFT)) & 0x8000;
    if (g_qsInstalled && InFront() && !mods && !g_qsReq) {
        if (s && !g_qsPrevSave) { g_qsReq = 1; g_qsReqTick = GetTickCount(); }
        else if (l && !g_qsPrevLoad) { g_qsReq = 2; g_qsReqTick = GetTickCount(); }
    }
    g_qsPrevSave = s;
    g_qsPrevLoad = l;
    if (g_qsMsgUntil && (int)(g_qsMsgUntil - GetTickCount()) > 0) {
        typedef void(__cdecl *Text)(int, int, int, int, int, int);
        ((void(__cdecl *)(int))0x556A50)(8);  // shadow: palette 8
        ((Text)0x50B5B0)(-1, (int)(ULONG_PTR)g_qsMsg, 256 + 2, 392 + 2, 1, 0);
        ((void(__cdecl *)(DWORD))0x556A90)(g_qsMsgCol);
        ((Text)0x50B5B0)(-1, (int)(ULONG_PTR)g_qsMsg, 256, 392, 1, 0);
    } else {
        g_qsMsgUntil = 0;
    }
}

static int QsKey(const char *s, int def)
{
    if ((s[0] == 'F' || s[0] == 'f') && atoi(s + 1) >= 1 && atoi(s + 1) <= 12) return VK_F1 + atoi(s + 1) - 1;
    if (atoi(s) > 0 && atoi(s) < 256) return atoi(s);
    return def;
}

// v3.7.0: wantExtra = the Extra Menu will need the dispatcher even with [QuickSave] Enabled=0 (review EM-001).
// Returns 1 when the dispatcher is in place.
static int QuickSaveInstall(int wantExtra)
{
    // only the 5 bytes patched here: Playable Eileen patches the next call (0x4EF987)
    static const Site kSite = { 0x4EF982, { 0xE8, 0x19, 0x92, 0x00, 0x00 }, 5 };
    static const Site kFns[] = {
        { 0x4FC0E0, { 0x56, 0x57, 0x6A, 0x01, 0xE8, 0x37, 0x34, 0x07, 0x00 }, 9 },   // snapshot
        { 0x4F72B0, { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x0C, 0x8B, 0x45, 0x08 }, 9 },   // stage change request
        { 0x56F540, { 0x56, 0x6A, 0x00, 0xE8, 0x98, 0xC7, 0xF8, 0xFF }, 8 },         // clear-data test
        { 0x4FBCE0, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x69, 0xC0, 0xE4, 0x05 }, 10 },  // global block
        { 0x573370, { 0xE9, 0xAB, 0x61, 0xFF, 0xFF }, 5 },                           // file queue
        { 0x4F8BA0, { 0xA1, 0x50, 0xBF, 0x07, 0x01 }, 5 },                           // sub-menu check (callee)
    };
    char buf[MAX_PATH];
    int i;
    g_qsEnabled = IniInt("QuickSave", "Enabled");
    g_qsFeedback = IniInt("QuickSave", "Feedback");
    IniStr("QuickSave", "SaveKey", buf, sizeof buf);
    g_qsSaveVk = QsKey(buf, VK_F5);
    IniStr("QuickSave", "LoadKey", buf, sizeof buf);
    g_qsLoadVk = QsKey(buf, VK_F9);
    IniStr("QuickSave", "Folder", buf, sizeof buf);
    if (!buf[0]) strcpy_s(buf, sizeof buf, "save\\quick");
    if (buf[1] == ':' || (buf[0] == '\\' && buf[1] == '\\')) strcpy_s(g_qsDir, sizeof g_qsDir, buf);
    else sprintf_s(g_qsDir, sizeof g_qsDir, "%s\\%s", g_dir, buf);
    if (!g_qsEnabled) Log("quick save: [QuickSave] Enabled=0: off");
    if (!g_qsEnabled && !wantExtra) return 0;
    if (!g_spInstalled) {
        Log("quick save: the HUD hook is not in place (see items:): no quick save, no Extra Menu");
        return 0;
    }
    if (!SiteOk(&kSite)) {
        Log("quick save: 004EF982 is not the stock `call 004F8BA0` (another mod?): no quick save, no Extra Menu");
        return 0;
    }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i)
        if (!SiteOk(&kFns[i])) {
            Log("quick save: %08lX is not the stock function (another mod?): no quick save, no Extra Menu", kFns[i].va);
            return 0;
        }
    if (!PokeBranch(0x4EF982, 0xE8, QsTick, 5)) {
        Log("quick save: could not patch 004EF982: no quick save, no Extra Menu");
        return 0;
    }
    g_qsTickOn = 1;
    QsAnyValid();
    if (!g_qsEnabled) return 1;
    g_qsInstalled = 1;
    Log("quick save: on, key %d saves, key %d loads (VK codes; F5 = 116, F9 = 120), 3 slots in %s", g_qsSaveVk,
        g_qsLoadVk, g_qsDir);
    return 1;
}
