// Tests file_names.inc (v2.1.1 SH4WindowMod.* -> SH4Refined.*). Build: test\build_test_names.cmd
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "..\src\file_names.inc"
static int fails, checks;
static char D[MAX_PATH];
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static void P(char *o, const char *n) { sprintf_s(o, MAX_PATH, "%s%s", D, n); }
static void Put(const char *n, const char *text, int ageSec)
{
    char p[MAX_PATH]; FILE *f; HANDLE h; FILETIME ft; ULARGE_INTEGER u;
    P(p, n); fopen_s(&f, p, "wb"); fputs(text, f); fclose(f);
    GetSystemTimeAsFileTime(&ft); u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    u.QuadPart -= (ULONGLONG)ageSec * 10000000ull; ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    h = CreateFileA(p, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL); SetFileTime(h, NULL, NULL, &ft); CloseHandle(h);
}
static int Has(const char *n, const char *text)
{
    char p[MAX_PATH], b[256] = {0}; FILE *f; P(p, n);
    if (fopen_s(&f, p, "rb")) return 0; fread(b, 1, sizeof b - 1, f); fclose(f);
    return !text || !strcmp(b, text);
}
static void Clear(void)
{
    const char *all[] = { SH4R_INI, SH4R_LOG, SH4R_PREV, SH4R_OLD_INI, SH4R_OLD_LOG, SH4R_OLD_PREV };
    char p[MAX_PATH]; int i; for (i = 0; i < 6; ++i) { P(p, all[i]); DeleteFileA(p); }
}
int main(void)
{
    char note[1024], ini[MAX_PATH], want[MAX_PATH];
    GetTempPathA(MAX_PATH, D); strcat_s(D, MAX_PATH, "sh4r_names_test\\"); CreateDirectoryA(D, NULL);
    // 1. a folder that ran v2.1: everything moves
    Clear(); Put(SH4R_OLD_INI, "oldini", 100); Put(SH4R_OLD_LOG, "oldlog", 50); Put(SH4R_OLD_PREV, "oldprev", 200);
    RenameOldFiles(D, note, sizeof note); SettingsPath(D, ini, sizeof ini); P(want, SH4R_INI);
    CHECK(Has(SH4R_INI, "oldini")); CHECK(!Has(SH4R_OLD_INI, NULL)); CHECK(Has(SH4R_PREV, "oldlog"));
    CHECK(!Has(SH4R_OLD_LOG, NULL)); CHECK(!Has(SH4R_OLD_PREV, NULL)); CHECK(!strcmp(ini, want)); CHECK(strstr(note, "renamed") != NULL);
    printf("1: %s\n", note);
    // 2. second start: the new log becomes prev, nothing else happens
    Put(SH4R_LOG, "newlog", 10);
    RenameOldFiles(D, note, sizeof note);
    CHECK(Has(SH4R_PREV, "newlog")); CHECK(!Has(SH4R_LOG, NULL)); CHECK(Has(SH4R_INI, "oldini")); CHECK(note[0] == 0);
    // 3. both ini files (downgrade then upgrade): the new one is used, the old one is left
    Clear(); Put(SH4R_INI, "newini", 100); Put(SH4R_OLD_INI, "oldini", 50);
    RenameOldFiles(D, note, sizeof note); SettingsPath(D, ini, sizeof ini); P(want, SH4R_INI);
    CHECK(Has(SH4R_INI, "newini")); CHECK(Has(SH4R_OLD_INI, "oldini")); CHECK(!strcmp(ini, want)); CHECK(strstr(note, "left alone") != NULL);
    // 4. the old log is older than the new one: the new one is prev, the old one deleted
    Clear(); Put(SH4R_LOG, "newlog", 10); Put(SH4R_OLD_LOG, "oldlog", 1000);
    RenameOldFiles(D, note, sizeof note);
    CHECK(Has(SH4R_PREV, "newlog")); CHECK(!Has(SH4R_OLD_LOG, NULL)); printf("4: %s\n", note);
    // 5. the old log is newer (the old version ran last): it is prev
    Clear(); Put(SH4R_LOG, "newlog", 1000); Put(SH4R_OLD_LOG, "oldlog", 10);
    RenameOldFiles(D, note, sizeof note);
    CHECK(Has(SH4R_PREV, "oldlog")); CHECK(!Has(SH4R_OLD_LOG, NULL));
    // 6. a fresh install: nothing there
    Clear(); RenameOldFiles(D, note, sizeof note); SettingsPath(D, ini, sizeof ini); P(want, SH4R_INI);
    CHECK(note[0] == 0); CHECK(!strcmp(ini, want)); CHECK(!Has(SH4R_PREV, NULL));
    // 7. the old ini is open without sharing delete (rename fails): it is still read
    Clear(); Put(SH4R_OLD_INI, "oldini", 100);
    { char p[MAX_PATH]; HANDLE h; P(p, SH4R_OLD_INI); h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      RenameOldFiles(D, note, sizeof note); SettingsPath(D, ini, sizeof ini); CloseHandle(h); }
    P(want, SH4R_OLD_INI); CHECK(!strcmp(ini, want)); CHECK(strstr(note, "could not rename") != NULL); printf("7: %s\n", note);
    Clear(); RemoveDirectoryA(D);
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
