// v2.1.1: the mod's files are named after SH4 Refined: SH4Refined.ini, SH4Refined.log, SH4Refined.prev.log.
// A folder that ran v2.1 or older has SH4WindowMod.ini / .log / .prev.log. RenameOldFiles (before the log opens):
//   * the settings move to the new name once (only when SH4Refined.ini is missing; with both, the old one is left
//     alone and SH4Refined.ini is used);
//   * SH4Refined.log becomes SH4Refined.prev.log; then SH4WindowMod.log (the session of the old version) becomes the
//     previous log instead when it is the newer of the two, else it is deleted; SH4WindowMod.prev.log is deleted.
// The notes for the log are written to note (the log is not open yet). Used by sh4windowmod.c and test\test_names.c.
#define SH4R_INI "SH4Refined.ini"
#define SH4R_LOG "SH4Refined.log"
#define SH4R_PREV "SH4Refined.prev.log"
#define SH4R_OLD_INI "SH4WindowMod.ini"
#define SH4R_OLD_LOG "SH4WindowMod.log"
#define SH4R_OLD_PREV "SH4WindowMod.prev.log"

static int NamesExist(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

static void NamesNote(char *note, size_t size, const char *text)
{
    if (!note || !size) return;
    if (note[0]) strcat_s(note, size, "; ");
    strcat_s(note, size, text);
}

static void RenameOldFiles(const char *dir, char *note, size_t noteSize)
{
    char ini[MAX_PATH], log[MAX_PATH], prev[MAX_PATH], oIni[MAX_PATH], oLog[MAX_PATH], oPrev[MAX_PATH], t[MAX_PATH + 96];
    if (note && noteSize) note[0] = 0;
    sprintf_s(ini, sizeof ini, "%s" SH4R_INI, dir);
    sprintf_s(log, sizeof log, "%s" SH4R_LOG, dir);
    sprintf_s(prev, sizeof prev, "%s" SH4R_PREV, dir);
    sprintf_s(oIni, sizeof oIni, "%s" SH4R_OLD_INI, dir);
    sprintf_s(oLog, sizeof oLog, "%s" SH4R_OLD_LOG, dir);
    sprintf_s(oPrev, sizeof oPrev, "%s" SH4R_OLD_PREV, dir);

    // the previous session's log
    if (NamesExist(log)) MoveFileExA(log, prev, MOVEFILE_REPLACE_EXISTING);
    if (NamesExist(oLog)) {
        WIN32_FILE_ATTRIBUTE_DATA a, b;
        int oldNewer = !GetFileAttributesExA(prev, GetFileExInfoStandard, &b) ||
                       (GetFileAttributesExA(oLog, GetFileExInfoStandard, &a) &&
                        CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) >= 0);
        if (oldNewer && MoveFileExA(oLog, prev, MOVEFILE_REPLACE_EXISTING))
            NamesNote(note, noteSize, SH4R_OLD_LOG " kept as " SH4R_PREV);
        else if (!oldNewer && DeleteFileA(oLog))
            NamesNote(note, noteSize, SH4R_OLD_LOG " deleted (older than " SH4R_PREV ")");
    }
    if (NamesExist(oPrev) && DeleteFileA(oPrev)) NamesNote(note, noteSize, SH4R_OLD_PREV " deleted");

    // the settings
    if (NamesExist(oIni)) {
        if (!NamesExist(ini)) {
            if (MoveFileExA(oIni, ini, 0))
                NamesNote(note, noteSize, SH4R_OLD_INI " renamed " SH4R_INI " (every value kept)");
            else {
                sprintf_s(t, sizeof t, "could not rename " SH4R_OLD_INI " (error %lu)", GetLastError());
                NamesNote(note, noteSize, t);
            }
        } else {
            NamesNote(note, noteSize, SH4R_OLD_INI " left alone: " SH4R_INI " exists and is used");
        }
    }
}

// The settings file to read: SH4Refined.ini, or SH4WindowMod.ini when the rename above failed and only it exists.
static void SettingsPath(const char *dir, char *out, size_t size)
{
    char o[MAX_PATH];
    sprintf_s(out, size, "%s" SH4R_INI, dir);
    sprintf_s(o, sizeof o, "%s" SH4R_OLD_INI, dir);
    if (!NamesExist(out) && NamesExist(o)) strcpy_s(out, size, o);
}
