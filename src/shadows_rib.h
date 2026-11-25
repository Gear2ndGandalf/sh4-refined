// v3.49 SH349: the stub at 0x45F4CA (was `call 45F1E0` in EffectDrawRibbonShadow 0x45F480): the Tremer's wall shadow's
// colour is in eax there (`mov eax, [esp+2Ch]` at 0x45F4C3; 0x45F1E0 reads it at 0x45F1F9: `mov ebx, eax; shr eax, 18h`),
// its three stack arguments (buf, count, piece) are the caller's. The stub hands eax to ShRibbonC (cdecl: the colour at
// the Darkness), keeps ecx / edx, and jumps to 0x45F1E0 (g_shRibFn) with the caller's return address on top, so 0x45F1E0
// returns straight to 0x45F4CF. Its own header so that test\shribbon_test.c runs this exact code against a stand-in.
// Before including: DWORD __cdecl ShRibbonC(DWORD) and DWORD g_shRibFn (the jump target).
#ifndef SHADOWS_RIB_H
#define SHADOWS_RIB_H
static __declspec(naked) void hkShRibbon(void)
{
    __asm {
        push ecx
        push edx
        push eax
        call ShRibbonC
        add esp, 4
        pop edx
        pop ecx
        jmp dword ptr [g_shRibFn]
    }
}
#endif
