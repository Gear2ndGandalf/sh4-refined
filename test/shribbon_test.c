// v3.49 SH349: native test of the Tremer wall shadow stub (src\shadows_rib.h, the exact code shadows.inc installs at
// 0x45F4CA) against a stand-in for 0x45F1E0: the colour arrives in eax at the Darkness (shadows_sim.h ShRibbonColour),
// ecx / edx / ebx / esi / edi / ebp and the three stack arguments reach the callee unchanged, the callee returns straight
// to the caller (the stub leaves no frame behind), the callee's eax comes back. Build: test\build_shribbon_test.cmd (x86).
#include <stdio.h>
#include <windows.h>
#include "../src/shadows_sim.h"

static int g_n, g_fail;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int g_pct = 85;  // the Darkness the stand-in ShRibbonC reads (shadows.inc: g_shDark)
static unsigned g_seenC;  // what ShRibbonC was given
static DWORD __cdecl ShRibbonC(DWORD c)
{
    g_seenC = c;
    return (DWORD)ShRibbonColour(c, g_pct);
}
// what the stand-in for 0x45F1E0 saw: eax, ecx, edx, ebx, esi, edi, ebp, its return address and its three arguments
static DWORD g_in[7], g_ret, g_arg[3];
static __declspec(naked) void Fake45F1E0(void)
{
    __asm {
        mov g_in[0], eax
        mov g_in[4], ecx
        mov g_in[8], edx
        mov g_in[12], ebx
        mov g_in[16], esi
        mov g_in[20], edi
        mov g_in[24], ebp
        mov eax, [esp]
        mov g_ret, eax
        mov eax, [esp + 4]
        mov g_arg[0], eax
        mov eax, [esp + 8]
        mov g_arg[4], eax
        mov eax, [esp + 12]
        mov g_arg[8], eax
        mov eax, 1  // 0x45F1E0 returns 1
        ret
    }
}
static DWORD g_shRibFn;
#include "../src/shadows_rib.h"

// the caller's side, as 0x45F4C3..0x45F4CF: eax = colour, push piece / count / buf, call, add esp, 0Ch
static DWORD g_out[8], g_espIn, g_espOut, g_expRet, g_colIn;
static __declspec(naked) void CallStubAsm(void)  // naked: it sets ebp itself (no frame to lose)
{
    __asm {
        push ebx
        push esi
        push edi
        push ebp
        mov g_espIn, esp
        mov ebx, 0xB0B0B0B0
        mov esi, 0x51515151
        mov edi, 0xD1D1D1D1
        mov ebp, 0xEBEBEBEB
        mov ecx, 0xC1C1C1C1
        mov edx, 0xD2D2D2D2
        mov eax, offset colour_ret
        mov g_expRet, eax
        mov eax, g_colIn
        push 0x33333333  // piece
        push 13          // count
        push 0x11111111  // buf
        call hkShRibbon
    colour_ret:
        add esp, 0Ch
        mov g_out[0], eax
        mov g_out[4], ecx
        mov g_out[8], edx
        mov g_out[12], ebx
        mov g_out[16], esi
        mov g_out[20], edi
        mov g_out[24], ebp
        mov g_espOut, esp
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
static void CallStub(DWORD colour)
{
    g_colIn = colour;
    CallStubAsm();
}

int main(void)
{
    static const int kPct[] = { 100, 85, 70, 55, 40, 25, 0 };
    static const DWORD kCol[] = { 0x30808080u, 0x1A808080u, 0x01808080u, 0xFF123456u };
    int i, j;
    g_shRibFn = (DWORD)(ULONG_PTR)Fake45F1E0;
    for (i = 0; i < (int)(sizeof kPct / sizeof kPct[0]); ++i)
        for (j = 0; j < (int)(sizeof kCol / sizeof kCol[0]); ++j) {
            const DWORD want = (DWORD)ShRibbonColour(kCol[j], kPct[i]);
            g_pct = kPct[i];
            memset(g_in, 0, sizeof g_in), memset(g_arg, 0, sizeof g_arg), g_ret = 0;
            CallStub(kCol[j]);
            CHECK(g_seenC == kCol[j], "ShRibbonC got eax %08lX (asked %08lX)", (unsigned long)g_seenC, (unsigned long)kCol[j]);
            CHECK(g_in[0] == want, "%d %%: the callee's eax %08lX, want %08lX", kPct[i], (unsigned long)g_in[0], (unsigned long)want);
            CHECK(g_in[1] == 0xC1C1C1C1u && g_in[2] == 0xD2D2D2D2u, "ecx / edx kept (%08lX %08lX)", (unsigned long)g_in[1], (unsigned long)g_in[2]);
            CHECK(g_in[3] == 0xB0B0B0B0u && g_in[4] == 0x51515151u && g_in[5] == 0xD1D1D1D1u && g_in[6] == 0xEBEBEBEBu,
                  "ebx / esi / edi / ebp kept");
            CHECK(g_arg[0] == 0x11111111u && g_arg[1] == 13u && g_arg[2] == 0x33333333u, "the three stack arguments (%08lX %lu %08lX)",
                  (unsigned long)g_arg[0], (unsigned long)g_arg[1], (unsigned long)g_arg[2]);
            CHECK(g_ret == g_expRet, "the callee returns to the caller (%08lX, want %08lX)", (unsigned long)g_ret, (unsigned long)g_expRet);
            CHECK(g_out[0] == 1u && g_out[3] == 0xB0B0B0B0u && g_out[4] == 0x51515151u && g_out[5] == 0xD1D1D1D1u,
                  "the callee's eax and the callee-saved registers back at the caller");
            CHECK(g_espOut == g_espIn, "the stack balanced (%08lX vs %08lX)", (unsigned long)g_espOut, (unsigned long)g_espIn);
        }
    CHECK(ShRibbonColour(0x30808080u, 85) == 0x29808080u, "85 %%: 0x30 -> 0x29");
    printf("shribbon_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
