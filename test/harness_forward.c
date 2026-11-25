// harness_forward.c: loads the proxy winmm.dll with LoadLibrary in a 32-bit test process (not the game) and checks
// the forwarding: every export of the def resolves by name and by ordinal, calls through the proxy give the real
// winmm's results with a balanced stack (built with /RTC1: every call through a function pointer checks ESP), the real
// dll is the SysWOW64 one, the proxy does nothing else in a non-game host (no log, SH4WM_Patches empty).
//   harness_forward.exe <proxy winmm.dll> <winmm.def>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); ++g_fail; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef DWORD (WINAPI *timeGetTime_t)(void);
typedef MMRESULT (WINAPI *timePeriod_t)(UINT);
typedef MMRESULT (WINAPI *timeGetDevCaps_t)(LPTIMECAPS, UINT);
typedef UINT (WINAPI *count_t)(void);
typedef BOOL (WINAPI *PlaySoundA_t)(LPCSTR, HMODULE, DWORD);
typedef BOOL (WINAPI *PlaySoundW_t)(LPCWSTR, HMODULE, DWORD);
typedef MMRESULT (WINAPI *timeSetEvent_t)(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT);
typedef MMRESULT (WINAPI *timeKillEvent_t)(UINT);
typedef FOURCC (WINAPI *mmioStringToFOURCCA_t)(LPCSTR, UINT);
typedef MMRESULT (WINAPI *waveOutGetDevCapsA_t)(UINT_PTR, LPWAVEOUTCAPSA, UINT);

static HANDLE g_ev;
static void CALLBACK OnTimer(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR a, DWORD_PTR b)
{
    (void)id; (void)msg; (void)a; (void)b;
    *(volatile LONG *)user = 1;
    SetEvent(g_ev);
}

int main(int argc, char **argv)
{
    char line[512], dir[MAX_PATH], logPath[MAX_PATH], *slash;
    HMODULE px, real = NULL, mods[512];
    DWORD need, i;
    FILE *def;
    int names = 0, byOrd = 0, noname = 0;
    if (argc < 3) return 2;
    px = LoadLibraryA(argv[1]);
    CHECK(px != NULL, "LoadLibrary %s -> %p", argv[1], px);
    if (!px) return 1;
    // 1. every export of the def
    if (fopen_s(&def, argv[2], "r") || !def) return 2;
    while (fgets(line, sizeof line, def)) {
        char name[128], stub[64];
        int ord;
        if (sscanf_s(line, " %127s = %63s @%d", name, (unsigned)sizeof name, stub, (unsigned)sizeof stub, &ord) == 3) {
            FARPROC a = GetProcAddress(px, name), b = GetProcAddress(px, MAKEINTRESOURCEA(ord));
            names += a != NULL;
            byOrd += a && a == b;
            if (!a || a != b) printf("FAIL  export %s @%d: by name %p, by ordinal %p\n", name, ord, a, b), ++g_fail;
        } else if (sscanf_s(line, " %63s @%d NONAME", stub, (unsigned)sizeof stub, &ord) == 2 && strstr(line, "NONAME")) {
            noname += GetProcAddress(px, MAKEINTRESOURCEA(ord)) != NULL;
        }
    }
    fclose(def);
    CHECK(names == 192 && byOrd == 192 && noname == 1, "exports: %d named resolve, %d match their ordinal, %d NONAME resolves (expect 192/192/1)",
          names, byOrd, noname);
    {
        const DWORD *pl = (const DWORD *)GetProcAddress(px, "SH4WM_Patches");
        CHECK(pl && pl[0] == 0x4D574853 && pl[1] == 1 && pl[2] == 0, "SH4WM_Patches: magic %08lX version %lu count %lu (not the game: empty)",
              pl ? pl[0] : 0, pl ? pl[1] : 0, pl ? pl[2] : 0);
    }
    // 2. calls through the proxy (the first call of each loads the real dll)
    {
        timeGetTime_t tgt = (timeGetTime_t)GetProcAddress(px, "timeGetTime");
        DWORD t1 = tgt(), t2;
        timePeriod_t beg = (timePeriod_t)GetProcAddress(px, "timeBeginPeriod"), end = (timePeriod_t)GetProcAddress(px, "timeEndPeriod");
        timeGetDevCaps_t caps = (timeGetDevCaps_t)GetProcAddress(px, "timeGetDevCaps");
        TIMECAPS tc = { 0 };
        MMRESULT r1 = beg(1), r2 = end(1), r3 = caps(&tc, sizeof tc);
        // find the real dll the proxy loaded
        GetModuleFileNameA(px, dir, MAX_PATH);
        if (EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &need))
            for (i = 0; i < need / sizeof(HMODULE); ++i) {
                char p[MAX_PATH];
                GetModuleFileNameA(mods[i], p, MAX_PATH);
                if (mods[i] != px && (slash = strrchr(p, '\\')) && !_stricmp(slash + 1, "winmm.dll")) {
                    real = mods[i];
                    CHECK(strstr(_strlwr(p), "\\syswow64\\winmm.dll") != NULL || strstr(p, "\\system32\\winmm.dll") != NULL,
                          "real dll loaded by the proxy: %s", p);
                }
            }
        CHECK(real != NULL, "the real winmm.dll is loaded (%p), the proxy is %p", real, px);
        if (!real) return 1;
        t2 = ((timeGetTime_t)GetProcAddress(real, "timeGetTime"))();
        CHECK(t2 - t1 < 1000, "timeGetTime through the proxy %lu, real %lu", t1, t2);
        CHECK(r1 == TIMERR_NOERROR && r2 == TIMERR_NOERROR, "timeBeginPeriod(1) %u, timeEndPeriod(1) %u", r1, r2);
        {
            TIMECAPS tr = { 0 };
            ((timeGetDevCaps_t)GetProcAddress(real, "timeGetDevCaps"))(&tr, sizeof tr);
            CHECK(r3 == 0 && tc.wPeriodMin == tr.wPeriodMin && tc.wPeriodMax == tr.wPeriodMax, "timeGetDevCaps %u: %u-%u ms (real %u-%u)", r3,
                  tc.wPeriodMin, tc.wPeriodMax, tr.wPeriodMin, tr.wPeriodMax);
        }
        {
            static const char *const kCounts[] = { "waveOutGetNumDevs", "waveInGetNumDevs", "midiOutGetNumDevs", "joyGetNumDevs",
                                                   "mixerGetNumDevs", "auxGetNumDevs", "mmsystemGetVersion" };
            int k;
            for (k = 0; k < 7; ++k) {
                UINT a = ((count_t)GetProcAddress(px, kCounts[k]))(), b = ((count_t)GetProcAddress(real, kCounts[k]))();
                CHECK(a == b, "%s through the proxy %u, real %u", kCounts[k], a, b);
            }
        }
        {
            WAVEOUTCAPSA a = { 0 }, b = { 0 };
            MMRESULT ra = ((waveOutGetDevCapsA_t)GetProcAddress(px, "waveOutGetDevCapsA"))(WAVE_MAPPER, &a, sizeof a);
            MMRESULT rb = ((waveOutGetDevCapsA_t)GetProcAddress(real, "waveOutGetDevCapsA"))(WAVE_MAPPER, &b, sizeof b);
            CHECK(ra == rb && !memcmp(&a, &b, sizeof a), "waveOutGetDevCapsA(WAVE_MAPPER) %u \"%s\" (real %u)", ra, a.szPname, rb);
        }
        {
            FOURCC f = ((mmioStringToFOURCCA_t)GetProcAddress(px, "mmioStringToFOURCCA"))("WAVE", 0);
            CHECK(f == mmioFOURCC('W', 'A', 'V', 'E'), "mmioStringToFOURCCA(\"WAVE\") %08lX", f);
        }
        {   // PlaySound: stop (NULL), a silent in-memory wave played synchronously, and the NONAME ordinal 2 (alias)
            static BYTE wav[44 + 800];
            DWORD *d = (DWORD *)wav;
            BOOL s1, s2, s3, s4;
            memcpy(wav, "RIFF", 4); d[1] = 36 + 800; memcpy(wav + 8, "WAVEfmt ", 8); d[4] = 16;
            *(WORD *)(wav + 20) = 1; *(WORD *)(wav + 22) = 1; d[6] = 8000; d[7] = 8000; *(WORD *)(wav + 32) = 1;
            *(WORD *)(wav + 34) = 8; memcpy(wav + 36, "data", 4); d[10] = 800;
            memset(wav + 44, 0x80, 800);  // 0.1 s of 8-bit silence
            s1 = ((PlaySoundA_t)GetProcAddress(px, "PlaySoundA"))(NULL, NULL, 0);
            s2 = ((PlaySoundW_t)GetProcAddress(px, "PlaySoundW"))((LPCWSTR)wav, NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
            s3 = ((PlaySoundA_t)GetProcAddress(real, "PlaySoundA"))((LPCSTR)wav, NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
            s4 = ((PlaySoundA_t)GetProcAddress(px, MAKEINTRESOURCEA(2)))(NULL, NULL, 0);
            CHECK(s1 && s2 == s3 && s4, "PlaySoundA(NULL) %d, PlaySoundW(silent wave) %d (real %d), ordinal 2 (PlaySound alias) %d", s1, s2, s3, s4);
        }
        {
            volatile LONG fired = 0;
            MMRESULT id;
            g_ev = CreateEventA(NULL, TRUE, FALSE, NULL);
            id = ((timeSetEvent_t)GetProcAddress(px, "timeSetEvent"))(20, 1, OnTimer, (DWORD_PTR)&fired, TIME_ONESHOT);
            WaitForSingleObject(g_ev, 2000);
            CHECK(id && fired && ((timeKillEvent_t)GetProcAddress(px, "timeKillEvent"))(id) != 0xFFFF,
                  "timeSetEvent one-shot fired %ld (id %u), timeKillEvent ok", fired, id);
        }
    }
    // 3. nothing else in a non-game host
    strcpy_s(logPath, sizeof logPath, dir);
    if ((slash = strrchr(logPath, '\\')) != NULL) strcpy_s(slash + 1, logPath + sizeof logPath - slash - 1, "SH4WindowMod.log");
    CHECK(GetFileAttributesA(logPath) == INVALID_FILE_ATTRIBUTES, "no SH4WindowMod.log next to the proxy in a non-game host");
    CHECK(FreeLibrary(px), "FreeLibrary(proxy)");
    printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
