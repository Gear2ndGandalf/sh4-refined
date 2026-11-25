// v3.40 LG340: tests of the session-log rotation: src\logrotate_sim.h (names, the prune plan, a log's start) and
// src\logrotate.inc (the copies and deletes) on its own folder test\out\lr_game. Build: test\build_logrotate_test.cmd
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static int fails, checks;
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static char g_said[4096];
static void Say(const char *pre, const char *fmt, va_list ap)
{
    char b[2048];
    _vsnprintf_s(b, sizeof b, _TRUNCATE, fmt, ap);
    printf("    %s%s\n", pre, b);
    strcat_s(g_said, sizeof g_said, b);
    strcat_s(g_said, sizeof g_said, "\n");
}
static void Log(const char *fmt, ...) { va_list ap; va_start(ap, fmt); Say("", fmt, ap); va_end(ap); }
static void LogV(const char *fmt, ...) { va_list ap; va_start(ap, fmt); Say("[v] ", fmt, ap); va_end(ap); }
static void LogAnomaly(const char *area, const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("    ANOMALY: %s: ", area); Say("", fmt, ap); va_end(ap); }
#include "..\src\file_names.inc"
#include "..\src\logrotate_sim.h"
#include "..\src\logrotate.inc"

static char D[MAX_PATH], L[MAX_PATH];
static void P(char *o, const char *n) { sprintf_s(o, MAX_PATH, "%s%s", D, n); }
static LrTime T(int y, int mo, int d, int h, int mi, int s) { LrTime t; t.y = y; t.mo = mo; t.d = d; t.h = h; t.mi = mi; t.s = s; return t; }
static int Same(const LrTime *a, const LrTime *b)
{
    return a->y == b->y && a->mo == b->mo && a->d == b->d && a->h == b->h && a->mi == b->mi && a->s == b->s;
}
static void SetLocalMtime(const char *p, const LrTime *t)
{
    SYSTEMTIME st = { 0 };
    FILETIME lf, ft;
    HANDLE h;
    st.wYear = (WORD)t->y; st.wMonth = (WORD)t->mo; st.wDay = (WORD)t->d; st.wHour = (WORD)t->h; st.wMinute = (WORD)t->mi; st.wSecond = (WORD)t->s;
    SystemTimeToFileTime(&st, &lf);
    LocalFileTimeToFileTime(&lf, &ft);  // the inverse of LrLocal's FileTimeToLocalFileTime
    h = CreateFileA(p, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    SetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
}
static void PutAt(const char *full, const char *text, const LrTime *mt)
{
    FILE *f;
    if (fopen_s(&f, full, "wb") || !f) { printf("cannot write %s\n", full); return; }
    fputs(text, f);
    fclose(f);
    if (mt) SetLocalMtime(full, mt);
}
static void Put(const char *n, const char *text, const LrTime *mt) { char p[MAX_PATH]; P(p, n); PutAt(p, text, mt); }
static int InLogs(const char *n) { char p[MAX_PATH]; sprintf_s(p, sizeof p, "%s%s", L, n); return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static int CountOwn(void) { LrList *l = &g_lrList; LrListDir(L, l); return l->n; }
static void Wipe(const char *dir)
{
    char pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    sprintf_s(pat, sizeof pat, "%s*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        sprintf_s(p, sizeof p, "%s%s", dir, fd.cFileName);
        SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { strcat_s(p, sizeof p, "\\"); Wipe(p); RemoveDirectoryA(p); }
        else DeleteFileA(p);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
// one game start as Init does it: rotate, rename, the new log with its v3.40 start line
static char note[2048], anom[512];
static void Start(int keep, const LrTime *now, const char *ver)
{
    char names[1024], text[512], p[MAX_PATH];
    LrTime end = *now;
    LogRotate(D, keep, note, sizeof note, anom, sizeof anom);
    RenameOldFiles(D, names, sizeof names);
    sprintf_s(text, sizeof text, "%02d:%02d:%02d.100 SH4 Refined v%s loading (winmm.dll proxy, game base 00400000)\r\n"
              "%02d:%02d:%02d.101 log: level 2 (verbose)\r\n%02d:%02d:%02d.102 log: started %04d-%02d-%02d %02d:%02d:%02d (local time)\r\n"
              "session body\r\n", now->h, now->mi, now->s, ver, now->h, now->mi, now->s, now->h, now->mi, now->s,
              now->y, now->mo, now->d, now->h, now->mi, now->s);
    end.mi = (end.mi + 20) % 60;  // the session's last write a little later (same day in these tests)
    if (end.mi < now->mi) end.h += 1;
    P(p, SH4R_LOG);
    PutAt(p, text, &end);
}

static void TestPure(void)
{
    char n[LR_NAME_MAX], v[32];
    unsigned long long k1 = 0, k2 = 0, k3 = 0;
    LrTime t = T(2026, 11, 22, 7, 24, 21), s, last;
    // Keep
    CHECK(LrKeep(-5) == 0 && LrKeep(0) == 0 && LrKeep(1) == 0 && LrKeep(2) == 2 && LrKeep(8) == 8 && LrKeep(1000) == LR_KEEP_MAX);
    // names
    CHECK(LrMakeName(n, sizeof n, &t, "3.40.0", 1) && !strcmp(n, "SH4Refined 2026-11-22 0724 v3.40.0.log"));
    CHECK(LrMakeName(n, sizeof n, &t, "3.40.0", 2) && !strcmp(n, "SH4Refined 2026-11-22 0724 v3.40.0 #2.log"));
    CHECK(LrMakeName(n, sizeof n, &t, "", 1) && !strcmp(n, "SH4Refined 2026-11-22 0724 unknown.log"));
    CHECK(LrMakeName(n, sizeof n, &t, "3.40/0 x:y", 1) && !strcmp(n, "SH4Refined 2026-11-22 0724 v3.40_0_x_y.log"));
    CHECK(LrMakeName(n, sizeof n, &t, "123456789012345678901234567890", 1) && strlen(n) < 60);
    { LrTime bad = T(2026, 2, 30, 7, 24, 0); CHECK(!LrMakeName(n, sizeof n, &bad, "3.40.0", 1)); }
    CHECK(!LrMakeName(n, sizeof n, &t, "3.40.0", 0) && !LrMakeName(n, sizeof n, &t, "3.40.0", LR_DUP_MAX + 1));
    // what is ours
    CHECK(LrParseName("SH4Refined 2026-11-22 0724 v3.40.0.log", &k1));
    CHECK(LrParseName("SH4Refined 2026-11-22 0724 v3.40.0 #2.log", &k2) && k2 > k1);
    CHECK(LrParseName("SH4Refined 2026-11-22 0725 v3.39.0.LOG", &k3) && k3 > k2);
    CHECK(LrParseName("SH4Refined 2026-11-22 0724 unknown.log", NULL));
    CHECK(!LrParseName("SH4Refined.log", NULL) && !LrParseName("SH4Refined.prev.log", NULL) && !LrParseName("notes.txt", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 v3.40.0.log.bak", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 v3.40.0 copy.log", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 v3.40.0 #1.log", NULL));
    CHECK(!LrParseName("SH4Refined 2026-13-22 0724 v3.40.0.log", NULL) && !LrParseName("SH4Refined 2026-11-22 2460 v3.40.0.log", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 .log", NULL) && !LrParseName("SH4Refined 2026-11-22 0724 v.log", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 x3.40.log", NULL) && !LrParseName("sh4refined 2026-11-22 0724 v3.40.0.log", NULL));
    CHECK(!LrParseName("SH4Refined 2026-11-22 0724 v3.40.0", NULL) && !LrParseName(NULL, NULL));
    // the prune plan: 10 own logs (shuffled) + 2 foreign names: the 2 oldest own ones go, foreign ones never
    {
        const char *nm[12] = { "SH4Refined 2026-11-20 2300 v3.39.0.log", "notes.txt", "SH4Refined 2026-11-22 0724 v3.40.0.log",
                               "SH4Refined 2026-11-19 0101 v3.38.0.log", "SH4Refined 2026-11-21 0800 v3.39.0.log",
                               "SH4Refined 2026-11-22 0724 v3.40.0 #2.log", "SH4Refined 2026-11-21 0900 v3.39.0.log",
                               "SH4Refined 2025-01-01 0000 v1.0.log.txt", "SH4Refined 2026-11-21 1000 v3.39.0.log",
                               "SH4Refined 2026-11-21 1100 v3.39.0.log", "SH4Refined 2026-11-21 1200 v3.39.0.log",
                               "SH4Refined 2026-11-21 1300 v3.39.0.log" };
        unsigned char del[12];
        int m = LrPlanPrune(nm, 12, 8, del);
        CHECK(m == 2 && del[0] && del[3]);
        CHECK(!del[1] && !del[7] && !del[2] && !del[5] && !del[4]);
        CHECK(LrPlanPrune(nm, 12, 0, del) == 0 && LrPlanPrune(nm, 12, 99, del) == 0);
        CHECK(LrPlanPrune(nm, 12, 1, del) == 9 && !del[5] && del[2]);  // same minute: " #2" is the newer
        CHECK(LrWouldKeep(nm, 12, "SH4Refined 2026-11-23 0000 v3.40.0.log", 8));
        CHECK(!LrWouldKeep(nm, 12, "SH4Refined 2026-11-01 0000 v3.30.0.log", 8));
        CHECK(LrWouldKeep(nm, 12, "SH4Refined 2026-11-01 0000 v3.30.0.log", 20));
        CHECK(!LrWouldKeep(nm, 12, "notes.txt", 8) && !LrWouldKeep(nm, 12, "SH4Refined 2026-11-23 0000 v3.40.0.log", 0));
    }
    // a log's start: v3.40's dated line; v3.39's first line + the last write's date; past midnight; nothing
    last = T(2026, 11, 22, 7, 26, 5);
    CHECK(LrHeader("07:24:21.162 SH4 Refined v3.40.0 loading (winmm.dll proxy)\n07:24:21.163 log: level 2\n"
                   "07:24:21.164 log: started 2026-11-21 07:24:21 (local time)\n", &last, &s, v, sizeof v) == 2);
    t = T(2026, 11, 21, 7, 24, 21);
    CHECK(Same(&s, &t) && !strcmp(v, "3.40.0"));
    CHECK(LrHeader("07:24:21.162 SH4 Refined v3.39.0 loading (winmm.dll proxy, game base 00400000)\r\n07:24:21.163 log: level 2\r\n"
                   "07:24:21.315 session: SH4 Refined v3.39.0 (file version 3.39.0.0)\r\n", &last, &s, v, sizeof v) == 1);
    t = T(2026, 11, 22, 7, 24, 21);
    CHECK(Same(&s, &t) && !strcmp(v, "3.39.0"));
    last = T(2027, 1, 1, 0, 4, 0);
    CHECK(LrHeader("23:58:00.001 SH4 Refined v3.33.0 loading\n", &last, &s, v, sizeof v) == 1);
    t = T(2026, 12, 31, 23, 58, 0);
    CHECK(Same(&s, &t));
    last = T(2026, 3, 1, 0, 4, 0);
    CHECK(LrHeader("23:58:00.001 SH4 Window Mod v2.1 loading\n", &last, &s, v, sizeof v) == 1 && !strcmp(v, "2.1"));
    t = T(2026, 2, 28, 23, 58, 0);
    CHECK(Same(&s, &t));
    last = T(2028, 3, 1, 0, 4, 0);
    CHECK(LrHeader("23:58:00.001 x\n", &last, &s, v, sizeof v) == 1 && v[0] == 0);
    t = T(2028, 2, 29, 23, 58, 0);  // a leap year
    CHECK(Same(&s, &t));
    CHECK(LrHeader("garbage\nlog: started 2026-99-01 00:00:00\n", &last, &s, v, sizeof v) == 0 && Same(&s, &last) && !v[0]);
    CHECK(LrHeader("", &last, &s, v, sizeof v) == 0 && Same(&s, &last));
    CHECK(LrHeader("12:00:00.000 hello\n12:00:01.000 session: SH4 Refined v9.9 loading\n", &last, &s, v, sizeof v) == 1 && !v[0]);
    // v3.40 REV340 (LG340-002): Refined's own first line
    CHECK(LrIsOwnLog("07:24:21.162 SH4 Refined v3.40.0 loading (winmm.dll proxy, game base 00400000)\r\n"));
    CHECK(LrIsOwnLog("07:24:21.162 SH4 Refined v3.40.0-test loading (winmm.dll proxy"));
    CHECK(!LrIsOwnLog("") && !LrIsOwnLog(NULL) && !LrIsOwnLog("my notes") && !LrIsOwnLog("SH4 Refined v3.40.0 loading (winmm.dll proxy"));
    CHECK(!LrIsOwnLog("07:24:21 SH4 Refined v3.40.0 loading (winmm.dll proxy") && !LrIsOwnLog("07:24:21.162 SH4 Refined v loading (winmm.dll proxy"));
    CHECK(!LrIsOwnLog("07:24:21.162 SH4 Refined v3.40.0 loading (another proxy") && !LrIsOwnLog("07:24:21.162 SH4 Window Mod v2.1 loading (winmm.dll proxy"));
    CHECK(!LrIsOwnLog("25:24:21.162 SH4 Refined v3.40.0 loading (winmm.dll proxy") && !LrIsOwnLog("07:24:21.162 SH4 Refined v3.40.0\r\n loading (winmm.dll proxy"));
}

static void TestFiles(void)
{
    LrTime t;
    char p[MAX_PATH];
    int i;
    // A. the first start of v3.40 over a v3.39 folder: both old logs are kept, named by their first line + last write
    Wipe(D);
    t = T(2026, 11, 22, 7, 24, 18);
    Put(SH4R_PREV, "07:21:43.000 SH4 Refined v3.39.0 loading (winmm.dll proxy)\r\nStock session\r\n", &t);
    t = T(2026, 11, 22, 7, 26, 5);
    Put(SH4R_LOG, "07:24:21.162 SH4 Refined v3.39.0 loading (winmm.dll proxy)\r\nD3D11 session\r\n", &t);
    g_said[0] = 0;
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    LogRotateReport(note, anom);
    CHECK(InLogs("SH4Refined 2026-11-22 0721 v3.39.0.log") && InLogs("SH4Refined 2026-11-22 0724 v3.39.0.log"));
    CHECK(g_lrCopied == 2 && g_lrDeleted == 0 && g_lrFailed == 0 && !anom[0]);
    CHECK(strstr(g_said, "log: started ") && strstr(g_said, "the last 8 in logs\\") && strstr(g_said, "(dated by its last write)"));
    // the same files again (a start that crashed before writing, run twice): nothing copied twice
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    CHECK(g_lrCopied == 0 && g_lrFailed == 0 && CountOwn() == 2);
    // B. 12 more starts of v3.40 on 12 days, with things in logs\ that are not Refined's or pinned read-only
    sprintf_s(p, sizeof p, "%snotes.txt", L); PutAt(p, "mine", NULL);
    sprintf_s(p, sizeof p, "%sSH4Refined 2026-11-22 0724 v3.39.0.log.txt", L); PutAt(p, "mine", NULL);
    sprintf_s(p, sizeof p, "%sSH4Refined 2020-01-01 0000 v1.0.log", L); PutAt(p, "pinned", NULL);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    sprintf_s(p, sizeof p, "%sSH4Refined 2019-01-01 0000 v1.0.log", L); CreateDirectoryA(p, NULL);  // a folder of our name form
    RenameOldFiles(D, note, sizeof note);  // (finish start A: the log moves to prev)
    for (i = 0; i < 12; ++i) {
        t = T(2026, 12, 1 + i, 20, 5, 0);
        Start(8, &t, "3.40.0");
        CHECK(g_lrFailed == 0 && !anom[0]);
    }
    CHECK(CountOwn() == 8 && g_lrList.pinned == 1 && g_lrList.foreign == 3);
    CHECK(InLogs("SH4Refined 2026-12-11 2005 v3.40.0.log") && InLogs("SH4Refined 2026-12-04 2005 v3.40.0.log"));
    CHECK(!InLogs("SH4Refined 2026-12-03 2005 v3.40.0.log") && !InLogs("SH4Refined 2026-11-22 0724 v3.39.0.log"));
    CHECK(InLogs("notes.txt") && InLogs("SH4Refined 2026-11-22 0724 v3.39.0.log.txt") && InLogs("SH4Refined 2020-01-01 0000 v1.0.log"));
    CHECK(InLogs("SH4Refined 2019-01-01 0000 v1.0.log"));
    // the current log (12-12) is not in logs\ yet; prev.log (12-11) is, and the next start adds 12-12
    CHECK(!InLogs("SH4Refined 2026-12-12 2005 v3.40.0.log"));
    // C. two sessions in the same minute: " #2"
    t = T(2026, 12, 12, 20, 5, 30);
    Start(8, &t, "3.40.0");
    t = T(2026, 12, 12, 20, 5, 50);
    Start(8, &t, "3.40.0");
    CHECK(InLogs("SH4Refined 2026-12-12 2005 v3.40.0.log") && InLogs("SH4Refined 2026-12-12 2005 v3.40.0 #2.log") && CountOwn() == 8);
    // D. Keep lowered to 3: the prune keeps 3; an old session is not copied in just to be deleted
    t = T(2026, 12, 13, 9, 0, 0);
    Start(3, &t, "3.40.0");
    CHECK(CountOwn() == 3 && g_lrDeleted >= 5);
    // E. Keep 0 / 1: nothing in logs\ changes, SH4Refined.prev.log still made
    t = T(2026, 12, 14, 9, 0, 0);
    Start(1, &t, "3.40.0");
    CHECK(CountOwn() == 3 && g_lrCopied == 0 && g_lrDeleted == 0 && GetFileAttributesA((P(p, SH4R_PREV), p)) != INVALID_FILE_ATTRIBUTES);
    // F. a locked log (opened without sharing): a note, nothing else stops
    {
        HANDLE h;
        P(p, SH4R_LOG);
        h = CreateFileA(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        CHECK(h != INVALID_HANDLE_VALUE);
        LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
        CloseHandle(h);
        CHECK(g_lrFailed == 1 && strstr(note, "could not be read") != NULL);
        printf("    F: %s\n", note);
    }
    // G. a log of ours that cannot be deleted (open without share-delete): a note, the other deletes go on
    {
        HANDLE h;
        int before;
        LogRotate(D, 8, note, sizeof note, anom, sizeof anom);  // the 12-14 session in (Keep 8 again)
        before = CountOwn();
        sprintf_s(p, sizeof p, "%s%s", L, g_lrList.name[0]);
        for (i = 1; i < g_lrList.n; ++i)
            if (LrNewer(p + strlen(L), g_lrList.name[i])) sprintf_s(p, sizeof p, "%s%s", L, g_lrList.name[i]);  // the oldest
        h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        LogRotate(D, 2, note, sizeof note, anom, sizeof anom);
        CloseHandle(h);
        CHECK(before == 5 && g_lrDeleted == 2 && g_lrFailed == 1 && strstr(note, "could not delete") != NULL);
        CHECK(CountOwn() == 3);  // the locked one stays until a later start
        printf("    G: %s\n", note);
    }
    // H. "logs" is a file: no rotation, a note
    Wipe(D);
    P(p, LR_DIR);
    PutAt(p, "a file", NULL);
    t = T(2026, 12, 20, 9, 0, 0);
    Put(SH4R_LOG, "09:00:00.000 SH4 Refined v3.40.0 loading\r\n", &t);
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    CHECK(g_lrFailed == 1 && strstr(note, "a file named logs") != NULL);
    // I. an empty log and a missing prev: nothing to keep, no note
    Wipe(D);
    Put(SH4R_LOG, "", NULL);
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    CHECK(g_lrCopied == 0 && g_lrFailed == 0 && note[0] == 0 && (GetFileAttributesA((P(p, LR_DIR), p)) & FILE_ATTRIBUTE_DIRECTORY));
    // J. v3.40 REV340: a SH4Refined.log that does not start like Refined's log is not copied in (so never pruned later)
    t = T(2026, 12, 21, 10, 11, 12);
    Put(SH4R_LOG, "something else\r\n", &t);
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    CHECK(!InLogs("SH4Refined 2026-12-21 1011 unknown.log") && CountOwn() == 0 && g_lrFailed == 0 &&
          strstr(note, "does not start like an SH4 Refined log") != NULL);
    // K. v3.40 REV340 (LG340-002): a file NAMED like the oldest rotated log but not one (the user's notes) is never counted
    //    or deleted, even past Keep; it is named in the log
    Wipe(D);
    CreateDirectoryA((P(p, LR_DIR), p), NULL);
    sprintf_s(p, sizeof p, "%sSH4Refined 2000-01-01 0000 v1.0.log", L); PutAt(p, "my own notes, not a game log\r\n", NULL);
    sprintf_s(p, sizeof p, "%sSH4Refined 2000-01-02 0000 v1.0.log", L); PutAt(p, "", NULL);  // empty: not ours either
    sprintf_s(p, sizeof p, "%sSH4Refined 2000-01-03 0000 v1.0.log", L);  // a near miss of the first line
    PutAt(p, "00:00:00.000 SH4 Refined v1.0 loading (another proxy)\r\n", NULL);
    for (i = 0; i < 4; ++i) {
        t = T(2027, 1, 1 + i, 12, 0, 0);
        Start(2, &t, "3.40.0");
    }
    LogRotate(D, 2, note, sizeof note, anom, sizeof anom);
    LogRotateReport(note, anom);
    CHECK(InLogs("SH4Refined 2000-01-01 0000 v1.0.log") && InLogs("SH4Refined 2000-01-02 0000 v1.0.log") &&
          InLogs("SH4Refined 2000-01-03 0000 v1.0.log"));
    CHECK(CountOwn() == 2 && g_lrList.odd == 3 && strstr(note, "3 file(s) named like Refined's logs but not starting like one") &&
          strstr(note, "SH4Refined 2000-01-01 0000 v1.0.log") != NULL);
    CHECK(InLogs("SH4Refined 2027-01-04 1200 v3.40.0.log") && InLogs("SH4Refined 2027-01-03 1200 v3.40.0.log") &&
          !InLogs("SH4Refined 2027-01-02 1200 v3.40.0.log"));
    // L. v3.40 REV340 (LG340-003): two different sessions of the same minute, same size, same last write: both kept
    Wipe(D);
    t = T(2027, 2, 1, 8, 1, 0);
    Put(SH4R_PREV, "08:00:05.000 SH4 Refined v3.40.0 loading (winmm.dll proxy, game base 00400000)\r\nsession AAAA\r\n", &t);
    Put(SH4R_LOG, "08:00:40.000 SH4 Refined v3.40.0 loading (winmm.dll proxy, game base 00400000)\r\nsession BBBB\r\n", &t);
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);
    CHECK(g_lrCopied == 2 && InLogs("SH4Refined 2027-02-01 0800 v3.40.0.log") && InLogs("SH4Refined 2027-02-01 0800 v3.40.0 #2.log"));
    LogRotate(D, 8, note, sizeof note, anom, sizeof anom);  // again: both found by their bytes, nothing copied twice
    CHECK(g_lrCopied == 0 && CountOwn() == 2);
}

int main(void)
{
    char here[MAX_PATH], *s;
    GetModuleFileNameA(NULL, here, MAX_PATH);  // test\out\logrotate_test.exe: its folder test\out\lr_game
    s = strrchr(here, '\\');
    if (s) s[1] = 0;
    sprintf_s(D, sizeof D, "%slr_game\\", here);
    sprintf_s(L, sizeof L, "%s" LR_DIR "\\", D);
    CreateDirectoryA(D, NULL);
    TestPure();
    TestFiles();
    Wipe(D);
    RemoveDirectoryA(D);
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
