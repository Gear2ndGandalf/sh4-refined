// harness_game.c: built as "SILENT HILL 4.exe" in a scratch folder next to the proxy winmm.dll (and, in some runs, a
// Playable Eileen dsound.dll). It imports WINMM.dll (timeGetTime ...), USER32 window functions and DINPUT8
// DirectInput8Create like the game, so the window mod hooks it at import resolution. It then plays the game's part:
//   harness_game.exe expect-active <client w> <client h>   checks the window behaviour (exit 0 = all passed)
//   harness_game.exe expect-idle                            checks that the window mod left the window alone
//   harness_game.exe expect-window <w> <h>                  window checks only (reference run with plugin v0.81's window.c)
//   harness_game.exe smoke                                  creates and shows the window, no checks
// The window is shown without activation (never takes focus or the mouse from the desktop).
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <mmsystem.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")

static int g_fail;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); ++g_fail; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef WITH_DSOUND
__declspec(dllimport) HRESULT WINAPI DirectSoundCreate8(LPCGUID, void **, void *);
#endif

static volatile LONG g_activateApp, g_activateAppOff;

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_ACTIVATEAPP) {
        if (w) ++g_activateApp;
        else ++g_activateAppOff;
    }
    return DefWindowProcA(h, m, w, l);
}

int main(int argc, char **argv)
{
    // expect-window: the same window checks without the window mod's own (SH4WM_Patches), for the v0.81 reference run
    int reference = argc > 1 && !strcmp(argv[1], "expect-window");
    int active = argc > 1 && (!strcmp(argv[1], "expect-active") || reference);
    int wantW = argc > 3 ? atoi(argv[2]) : 0, wantH = argc > 3 ? atoi(argv[3]) : 0;
    HMODULE wm = GetModuleHandleA("winmm.dll"), user = GetModuleHandleA("user32.dll");
    const DWORD *pl = wm ? (const DWORD *)GetProcAddress(wm, "SH4WM_Patches") : NULL;
    BOOL (WINAPI *realIsIconic)(HWND) = (BOOL (WINAPI *)(HWND))GetProcAddress(user, "IsIconic");
    BOOL (WINAPI *realShowWindow)(HWND, int) = (BOOL (WINAPI *)(HWND, int))GetProcAddress(user, "ShowWindow");
    WNDCLASSA wc = { 0 };
    HWND hwnd;
    RECT rc, before, after;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD t = timeGetTime();  // a real WINMM import, through the proxy
#ifdef WITH_DSOUND
    volatile void *keep = (void *)DirectSoundCreate8;  // keeps the DSOUND import (the plugin dll)
    (void)keep;
#endif
    CHECK(t != 0, "timeGetTime through the WINMM import: %lu", t);
    if (pl || (active && !reference))
        CHECK(wm && pl && pl[0] == 0x4D574853, "winmm.dll %p exports SH4WM_Patches (count %lu)", wm, pl ? pl[2] : 0);
    else
        printf("info  winmm.dll %p is the system dll (no window mod)\n", wm);
    if (active && !reference && pl) {  // every listed range is an IAT slot of this exe that now points into the proxy
        DWORD i, okSlots = 0;
        for (i = 0; i < pl[2]; ++i) {
            HMODULE owner = NULL;
            void *target = *(void **)(ULONG_PTR)pl[3 + 2 * i];
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)target, &owner);
            okSlots += pl[4 + 2 * i] == 4 && owner == wm;
        }
        CHECK(pl[2] >= 6 && okSlots == pl[2], "SH4WM_Patches: %lu ranges, %lu of them exe IAT slots hooked into the proxy", pl[2], okSlots);
    }
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SH4WMHarness";
    RegisterClassA(&wc);
    // like the game: a borderless topmost popup over the whole screen (not shown yet)
    hwnd = CreateWindowExA(WS_EX_TOPMOST, wc.lpszClassName, "SH4 Window Mod harness", WS_POPUP, 0, 0, sw, sh, NULL, NULL, wc.hInstance, NULL);
    CHECK(hwnd != NULL, "CreateWindowExA -> %p", hwnd);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    GetClientRect(hwnd, &rc);
    if (!active && strcmp(argc > 1 ? argv[1] : "", "expect-idle")) {
        printf("smoke run: window created and shown, no checks\n");
    } else if (!active) {
        DWORD st = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
        CHECK((st & WS_POPUP) && !(st & WS_CAPTION) && rc.right == sw && rc.bottom == sh,
              "window mod idle: the game's own popup %ldx%ld, style %08lX", rc.right, rc.bottom, st);
    } else {
        DWORD st = (DWORD)GetWindowLongA(hwnd, GWL_STYLE), ex = (DWORD)GetWindowLongA(hwnd, GWL_EXSTYLE);
        MSG msg;
        CHECK((st & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW && !(st & WS_POPUP), "style %08lX: overlapped, resizable", st);
        CHECK(!(ex & WS_EX_TOPMOST), "ex style %08lX: not topmost", ex);
        CHECK(rc.right == wantW && rc.bottom == wantH, "client %ldx%ld (ini %dx%d)", rc.right, rc.bottom, wantW, wantH);
        GetWindowRect(hwnd, &before);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, sw, sh, 0);  // the game / wrapper stretching it over the screen
        GetWindowRect(hwnd, &after);
        CHECK(!memcmp(&before, &after, sizeof before), "SetWindowPos(full screen) refused: still %ld,%ld %ldx%ld (before %ld,%ld %ldx%ld)",
              after.left, after.top, after.right - after.left, after.bottom - after.top, before.left, before.top,
              before.right - before.left, before.bottom - before.top);
        // info only: a direct SetWindowPos(HWND_TOPMOST) does make it topmost - the WM_WINDOWPOSCHANGING rewrite comes
        // too late for the z-order band. Same in window.c v0.29-v0.81 (behaviour kept); the game's CreateWindowExA /
        // SetWindowLong paths are the ones that are covered.
        printf("info  SetWindowPos(HWND_TOPMOST) direct: ex style %08lX (%s, as with window.c v0.81)\n",
               (DWORD)GetWindowLongA(hwnd, GWL_EXSTYLE), GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST ? "topmost" : "not topmost");
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);  // hooked import
        st = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
        CHECK((st & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW, "SetWindowLongA(GWL_STYLE, WS_POPUP) kept the frame: %08lX", st);
        ShowWindow(hwnd, SW_MINIMIZE);  // hooked import
        CHECK(!realIsIconic(hwnd), "ShowWindow(SW_MINIMIZE) from the exe: not minimised");
        realShowWindow(hwnd, SW_MINIMIZE);  // not through the exe's import: the CBT hook vetoes it
        CHECK(!realIsIconic(hwnd), "ShowWindow(SW_MINIMIZE) from elsewhere: not minimised (CBT veto)");
        SendMessageA(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        CHECK(!realIsIconic(hwnd), "WM_SYSCOMMAND SC_MINIMIZE swallowed");
        CHECK(GetForegroundWindow() == hwnd, "GetForegroundWindow (hooked import) reports the game window although it is in the background");
        SendMessageA(hwnd, WM_ACTIVATEAPP, FALSE, 0);
        SendMessageA(hwnd, WM_ACTIVATEAPP, TRUE, 0);
        CHECK(g_activateAppOff == 0 && g_activateApp >= 1, "WM_ACTIVATEAPP(FALSE) never reaches the game (%ld), TRUE does (%ld)",
              g_activateAppOff, g_activateApp);
        {   // drag-resize snapping: bottom-right drag to about 1000x740 client snaps to a standard 4:3 size
            RECT fr = { 0, 0, 0, 0 }, r;
            int cw, ch;
            AdjustWindowRect(&fr, (DWORD)GetWindowLongA(hwnd, GWL_STYLE), FALSE);
            r.left = 100; r.top = 100; r.right = 100 + 1000 + (fr.right - fr.left); r.bottom = 100 + 740 + (fr.bottom - fr.top);
            SendMessageA(hwnd, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&r);
            cw = r.right - r.left - (fr.right - fr.left);
            ch = r.bottom - r.top - (fr.bottom - fr.top);
            CHECK(cw * 3 == ch * 4 && r.left == 100 && r.top == 100, "WM_SIZING 1000x740 snapped to %dx%d (4:3, top-left kept)", cw, ch);
        }
        {   // the game recentres the cursor every frame: not while it is in the background
            // only while the harness window really is in the background (Windows may hand the new window the focus;
            // then the game may move the cursor, as designed, and the user's pointer must not be moved by a test)
            HWND (WINAPI *realFg)(void) = (HWND (WINAPI *)(void))GetProcAddress(user, "GetForegroundWindow");
            POINT p0, p1;
            if (realFg() == hwnd) {
                printf("info  the harness window got the focus: SetCursorPos check skipped\n");
            } else {
                GetCursorPos(&p0);
                SetCursorPos(p0.x > 50 ? 3 : 200, p0.y > 50 ? 3 : 200);  // hooked import
                GetCursorPos(&p1);
                CHECK(p0.x == p1.x && p0.y == p1.y, "SetCursorPos from the background game ignored (cursor stays at %ld,%ld)", p1.x, p1.y);
            }
        }
        {   // DirectInput: exclusive + background is invalid for the keyboard; the hook makes it non-exclusive
            IDirectInput8A *di = NULL;
            IDirectInputDevice8A *kb = NULL, *mouse = NULL;
            HRESULT hr = DirectInput8Create(GetModuleHandleA(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void **)&di, NULL);
            if (SUCCEEDED(hr)) hr = di->lpVtbl->CreateDevice(di, &GUID_SysKeyboard, &kb, NULL);
            if (SUCCEEDED(hr)) di->lpVtbl->CreateDevice(di, &GUID_SysMouse, &mouse, NULL);
            if (SUCCEEDED(hr)) hr = kb->lpVtbl->SetCooperativeLevel(kb, hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
            CHECK(hr == DI_OK, "keyboard SetCooperativeLevel(EXCLUSIVE | BACKGROUND) -> %08lX (DI_OK: turned non-exclusive)", hr);
            if (mouse) {
                hr = mouse->lpVtbl->SetCooperativeLevel(mouse, hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
                CHECK(hr == DI_OK, "mouse SetCooperativeLevel(EXCLUSIVE | FOREGROUND) -> %08lX", hr);
                mouse->lpVtbl->Release(mouse);
            }
            if (kb) kb->lpVtbl->Release(kb);
            if (di) di->lpVtbl->Release(di);
        }
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    }
    DestroyWindow(hwnd);
    {   // never leave the desktop's cursor confined or hidden after a test
        BOOL (WINAPI *realClip)(const RECT *) = (BOOL (WINAPI *)(const RECT *))GetProcAddress(user, "ClipCursor");
        realClip(NULL);
    }
    printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
