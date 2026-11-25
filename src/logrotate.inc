// v3.40 LG340: keeps the last [Log] Keep sessions' logs in the game folder's logs\ (the rules: logrotate_sim.h; tests:
// test\logrotate_test.c). LogRotate runs in Init before RenameOldFiles moves SH4Refined.log to SH4Refined.prev.log and
// before the new log opens (in DllMain: kernel32 file calls only, a few small files). It copies
//   * SH4Refined.prev.log (once: the session before the last one, when it is not in logs\ yet, e.g. the first start of
//     v3.40) and
//   * SH4Refined.log (the session before this one)
// into logs\ under their start and version (CopyFile keeps the last-write time, so the same session is found again next
// time and never copied twice), then deletes the oldest of Refined's own rotated logs past Keep. Copies, not moves:
// SH4Refined.prev.log goes on as before for the tools. Never deletes anything else: a name not exactly of LrMakeName's
// form, a folder, or a read-only file (a session the user wants to keep for good) is left alone and not counted. A
// read-only game folder, a locked or unreadable log, a failed copy or delete: a note in the log, the game goes on.
// The notes are kept in note (normal lines) and anom (ANOMALY lines) until the log is open.
// v3.40 REV340 (Codex LG340-002 / -003): a file is Refined's only when its NAME has the rotated form AND its first bytes
// are an SH4 Refined log's first line (LrIsOwnLog, a bounded read); a file only named like one is left alone and named
// in the log. A same-name file in logs\ is the same session only when its bytes are the same (else the next " #n").
#define LR_LIST_MAX 256
#define LR_ODD_MAX 3

typedef struct {
    int n, more;                          // listed own rotated logs that may be deleted; how many more were not listed
    int pinned, foreign;                  // read-only own logs (kept for good); other files / folders in logs\ (left alone)
    int odd;                              // v3.40 REV340: named like a rotated log but not an SH4 Refined log (left alone)
    char oddName[LR_ODD_MAX][LR_NAME_MAX];
    char name[LR_LIST_MAX][LR_NAME_MAX];
    const char *ptr[LR_LIST_MAX];
} LrList;
static LrList g_lrList;                    // Init runs once: no stack for 24 KB in DllMain
static int g_lrKeep;                       // the Keep in use this session (0 = off)
static int g_lrCopied, g_lrDeleted, g_lrFailed;

static void LrAdd(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    size_t o;
    if (!buf || !size) return;
    o = strlen(buf);
    if (o && o + 3 < size) { strcat_s(buf, size, "; "); o += 2; }
    if (o + 1 >= size) return;
    va_start(ap, fmt);
    _vsnprintf_s(buf + o, size - o, _TRUNCATE, fmt, ap);
    va_end(ap);
}

static int LrLocal(const FILETIME *ft, LrTime *t)
{
    FILETIME lf;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(ft, &lf) || !FileTimeToSystemTime(&lf, &st)) return 0;
    t->y = st.wYear; t->mo = st.wMonth; t->d = st.wDay; t->h = st.wHour; t->mi = st.wMinute; t->s = st.wSecond;
    return 1;
}

// v3.40 REV340: the first n bytes of a file (NUL-terminated in buf[n + 1]); the count read, -1 when it cannot be opened.
static int LrReadStart(const char *path, char *buf, DWORD n)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD got = 0;
    buf[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!ReadFile(h, buf, n, &got, NULL)) got = 0;
    CloseHandle(h);
    buf[got] = 0;
    return (int)got;
}

// v3.40 REV340 (LG340-003): 1 when the two files hold the same bytes (the caller has checked the sizes); 0 when they
// differ or either cannot be read (then the session goes under the next " #n" name: nothing is ever overwritten).
static int LrSameBytes(const char *a, const char *b)
{
    static char ba[65536], bb[65536];
    HANDLE ha, hb;
    DWORD na, nb;
    int same = 1;
    ha = CreateFileA(a, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                     FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (ha == INVALID_HANDLE_VALUE) return 0;
    hb = CreateFileA(b, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                     FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hb == INVALID_HANDLE_VALUE) { CloseHandle(ha); return 0; }
    for (;;) {
        if (!ReadFile(ha, ba, sizeof ba, &na, NULL) || !ReadFile(hb, bb, sizeof bb, &nb, NULL) || na != nb ||
            memcmp(ba, bb, na)) { same = 0; break; }
        if (!na) break;
    }
    CloseHandle(ha);
    CloseHandle(hb);
    return same;
}

// The listing of logs\ (own rotated logs that may be deleted; the pinned, the foreign and the odd are only counted).
static void LrListDir(const char *logs, LrList *l)
{
    char pat[MAX_PATH], p[MAX_PATH], sig[LR_SIG_BYTES + 1];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    l->n = l->more = l->pinned = l->foreign = l->odd = 0;
    if (sprintf_s(pat, sizeof pat, "%s*", logs) < 0) return;
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !LrParseName(fd.cFileName, NULL)) { ++l->foreign; continue; }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) { ++l->pinned; continue; }
        // v3.40 REV340 (LG340-002): its first line must be an SH4 Refined log's, else it is not ours (never deleted)
        if (sprintf_s(p, sizeof p, "%s%s", logs, fd.cFileName) < 0 || LrReadStart(p, sig, LR_SIG_BYTES) < 0 || !LrIsOwnLog(sig)) {
            if (l->odd < LR_ODD_MAX) strcpy_s(l->oddName[l->odd], LR_NAME_MAX, fd.cFileName);
            ++l->odd;
            continue;
        }
        if (l->n >= LR_LIST_MAX) { ++l->more; continue; }
        strcpy_s(l->name[l->n], LR_NAME_MAX, fd.cFileName);
        l->ptr[l->n] = l->name[l->n];
        ++l->n;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// One log into logs\: 1 copied, 0 nothing to do (missing, empty, already there, older than every kept one), -1 failed.
static int LrArchive(const char *dir, const char *logs, const char *file, char *note, size_t noteSize, char *anom,
                     size_t anomSize)
{
    char src[MAX_PATH], dst[MAX_PATH], name[LR_NAME_MAX], ver[LR_VER_MAX + 8];
    static char head[LR_HEAD_BYTES + 1];
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    LrTime last, start;
    int dup, how;
    if (sprintf_s(src, sizeof src, "%s%s", dir, file) < 0) return -1;
    if (!GetFileAttributesExA(src, GetFileExInfoStandard, &a) || (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    if (!a.nFileSizeHigh && !a.nFileSizeLow) return 0;  // an empty log: nothing to compare later
    if (LrReadStart(src, head, LR_HEAD_BYTES) < 0) {
        LrAdd(note, noteSize, "%s could not be read (error %lu): not kept in " LR_DIR "\\", file, GetLastError());
        return -1;
    }
    if (!LrIsOwnLog(head)) {  // v3.40 REV340 (LG340-002): only Refined's own logs go in (and so can be pruned later)
        LrAdd(note, noteSize, "%s does not start like an SH4 Refined log: not kept in " LR_DIR "\\", file);
        return 0;
    }
    if (!LrLocal(&a.ftLastWriteTime, &last)) {
        LrAdd(note, noteSize, "%s has no readable time: not kept in " LR_DIR "\\", file);
        return -1;
    }
    how = LrHeader(head, &last, &start, ver, sizeof ver);
    for (dup = 1; dup <= LR_DUP_MAX; ++dup) {
        if (!LrMakeName(name, sizeof name, &start, ver, dup) || sprintf_s(dst, sizeof dst, "%s%s", logs, name) < 0) return -1;
        if (!GetFileAttributesExA(dst, GetFileExInfoStandard, &b)) break;  // a free name
        if (b.nFileSizeHigh == a.nFileSizeHigh && b.nFileSizeLow == a.nFileSizeLow && LrSameBytes(src, dst))
            return 0;  // this session is there already (the usual case for SH4Refined.prev.log); v3.40 REV340: by its bytes
    }
    if (dup > LR_DUP_MAX) {
        LrAdd(note, noteSize, "%s not kept: " LR_DIR "\\ has %d other sessions of the same minute", file, LR_DUP_MAX);
        return -1;
    }
    if (!LrWouldKeep(g_lrList.ptr, g_lrList.n, name, g_lrKeep)) return 0;  // older than the newest Keep: the prune's anyway
    if (!CopyFileA(src, dst, TRUE)) {  // never over an existing file
        LrAdd(note, noteSize, "%s could not be copied to " LR_DIR "\\%s (error %lu)", file, name, GetLastError());
        return -1;
    }
    if (a.dwFileAttributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesA(dst, FILE_ATTRIBUTE_NORMAL);  // not pinned by accident
    if (!GetFileAttributesExA(dst, GetFileExInfoStandard, &b) || b.nFileSizeLow != a.nFileSizeLow || b.nFileSizeHigh != a.nFileSizeHigh)
        LrAdd(anom, anomSize, "the copy " LR_DIR "\\%s is not the size of %s (%lu bytes)", name, file, a.nFileSizeLow);
    LrAdd(note, noteSize, "%s kept as " LR_DIR "\\%s%s", file, name,
          how == 2 ? "" : how == 1 ? " (dated by its last write)" : " (no start line: named by its last write)");
    if (g_lrList.n < LR_LIST_MAX) {
        strcpy_s(g_lrList.name[g_lrList.n], LR_NAME_MAX, name);
        g_lrList.ptr[g_lrList.n] = g_lrList.name[g_lrList.n];
        ++g_lrList.n;
    }
    return 1;
}

// keep: LrKeep([Log] Keep). note / anom: what happened, for the log once it is open.
static void LogRotate(const char *dir, int keep, char *note, size_t noteSize, char *anom, size_t anomSize)
{
    char base[MAX_PATH], logs[MAX_PATH], p[MAX_PATH];
    unsigned char del[LR_LIST_MAX];
    DWORD at;
    int i, r;
    if (note && noteSize) note[0] = 0;
    if (anom && anomSize) anom[0] = 0;
    g_lrKeep = keep;
    g_lrCopied = g_lrDeleted = g_lrFailed = 0;
    g_lrList.n = g_lrList.more = g_lrList.pinned = g_lrList.foreign = g_lrList.odd = 0;  // v3.40 REV340: + odd
    if (keep < 2) return;  // 0 / 1: as up to v3.39 (SH4Refined.log + .prev.log); an existing logs\ is left as it is
    if (sprintf_s(base, sizeof base, "%s" LR_DIR, dir) < 0 || sprintf_s(logs, sizeof logs, "%s\\", base) < 0) return;
    at = GetFileAttributesA(base);  // (without the trailing backslash: a file named logs must be seen as one)
    if (at == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryA(base, NULL)) {
            LrAdd(note, noteSize, LR_DIR "\\ could not be made (error %lu; a read-only game folder?): only SH4Refined.log and "
                  "SH4Refined.prev.log are kept", GetLastError());
            g_lrFailed = 1;
            return;
        }
    } else if (!(at & FILE_ATTRIBUTE_DIRECTORY)) {
        LrAdd(note, noteSize, "a file named " LR_DIR " is in the game folder: no session logs kept (only SH4Refined.log and .prev.log)");
        g_lrFailed = 1;
        return;
    }
    LrListDir(logs, &g_lrList);
    r = LrArchive(dir, logs, SH4R_PREV, note, noteSize, anom, anomSize);
    g_lrCopied += r > 0; g_lrFailed += r < 0;
    r = LrArchive(dir, logs, SH4R_LOG, note, noteSize, anom, anomSize);
    g_lrCopied += r > 0; g_lrFailed += r < 0;
    LrPlanPrune(g_lrList.ptr, g_lrList.n, keep, del);
    for (i = 0; i < g_lrList.n; ++i) {
        if (!del[i]) continue;
        if (sprintf_s(p, sizeof p, "%s%s", logs, g_lrList.name[i]) < 0) continue;
        if (DeleteFileA(p)) ++g_lrDeleted;
        else {
            LrAdd(note, noteSize, "could not delete " LR_DIR "\\%s (error %lu)", g_lrList.name[i], GetLastError());
            ++g_lrFailed;
        }
    }
    if (g_lrList.n - g_lrDeleted > keep)  // the plan keeps exactly Keep of the deletable ones
        LrAdd(anom, anomSize, LR_DIR "\\ holds %d of its own deletable logs after the prune, Keep %d", g_lrList.n - g_lrDeleted, keep);
    if (g_lrList.more)
        LrAdd(note, noteSize, LR_DIR "\\ lists more than %d of Refined's logs: %d left for a later start", LR_LIST_MAX, g_lrList.more);
    if (g_lrList.odd)  // v3.40 REV340 (LG340-002): named like a rotated log, but its first line is not Refined's
        LrAdd(note, noteSize, "%d file(s) named like Refined's logs but not starting like one, left alone: %s%s%s%s%s%s", g_lrList.odd,
              g_lrList.oddName[0], g_lrList.odd > 1 ? ", " : "", g_lrList.odd > 1 ? g_lrList.oddName[1] : "",
              g_lrList.odd > 2 ? ", " : "", g_lrList.odd > 2 ? g_lrList.oddName[2] : "", g_lrList.odd > LR_ODD_MAX ? ", ..." : "");
}

// After the log opens: the start line (the date the next start names this session by) and what LogRotate did.
static void LogRotateReport(const char *note, const char *anom)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    Log("log: started %04d-%02d-%02d %02d:%02d:%02d (local time)", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    if (!g_lrKeep)
        LogV("log: session logs: only SH4Refined.log and SH4Refined.prev.log (hidden [Log] Keep 0 or 1; " LR_DIR "\\ is left as it is)");
    else
        Log("log: session logs: the last %d in " LR_DIR "\\ (hidden [Log] Keep=%d; 0 or 1 = only SH4Refined.log and .prev.log): "
            "%d copied in, %d older deleted, %d left alone (%d read-only = kept for good, %d not Refined's)%s%s",
            g_lrKeep, g_lrKeep, g_lrCopied, g_lrDeleted, g_lrList.pinned + g_lrList.foreign + g_lrList.odd, g_lrList.pinned,
            g_lrList.foreign + g_lrList.odd,  // v3.40 REV340: + the ones only named like Refined's
            note && note[0] ? "; " : "", note ? note : "");
    if (anom && anom[0]) LogAnomaly("log", "%s", anom);
}
