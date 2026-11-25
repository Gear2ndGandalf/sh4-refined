// v3.40 LG340: the end-to-end check of the log rotation with the built dll. Built as "SILENT HILL 4.exe" (base 0x01000000,
// so the dll applies no exe patch) in test\out\lr_e2e next to a copy of out_w2\winmm.dll; it only loads the dll and exits
// (the dll's Init rotates the logs, its DllMain detach writes the session end). Driven by test\lr_e2e.py.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
int main(void)
{
    char p[MAX_PATH], *s;
    HMODULE m;
    GetModuleFileNameA(NULL, p, MAX_PATH);
    s = strrchr(p, '\\');
    if (s) s[1] = 0;
    strcat_s(p, sizeof p, "winmm.dll");
    m = LoadLibraryA(p);
    printf("loaded %s: %p\n", p, (void *)m);
    return m ? 0 : 1;
}
