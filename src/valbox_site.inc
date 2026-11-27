// ================================================================ v3.58 MN358: the value box's exe sites (valbox.inc)
// Included from sh4windowmod.c after Poke / PokeBranch / SiteOk: the volume bars' two arrow sites and VbxInstall (the
// install line). The rules and the hooks' C side are in valbox.inc.
// the volume bars' arrows (0x5256B6 left, 0x5256E2 right; each was `push 80808080h`): ebp is the bar's x, the left arrow
// goes at ebp - 20 (0x5256C0), the right one at ebp + 100 (0x5256EC, after the left arrow's draw)
static __declspec(naked) void hkVbxBarL(void)
{
    __asm {
        sub ebp, dword ptr [g_vbxBarMove]
        push dword ptr [esp]                // the return address one slot down
        mov dword ptr [esp + 4], 80808080h  // the stock push in its place
        ret
    }
}
static __declspec(naked) void hkVbxBarR(void)
{
    __asm {
        add ebp, dword ptr [g_vbxBarShift]
        push dword ptr [esp]
        mov dword ptr [esp + 4], 80808080h
        ret
    }
}
#define VBX_BAR_L 0x5256B6
#define VBX_BAR_R 0x5256E2
static int VbxBarWritten(DWORD va, const void *to)
{
    const BYTE *p = (const BYTE *)(ULONG_PTR)va;
    const DWORD rel = (DWORD)(ULONG_PTR)to - (va + 5);
    return p[0] == 0xE8 && !memcmp(p + 1, &rel, 4);
}
// ApplyAspectMenu, after the 0x52570E / 0x525CE3 / 0x525802 installs (g_vbxHooks)
static void VbxInstall(void)
{
    static const Site kBar[2] = { { VBX_BAR_L, { 0x68, 0x80, 0x80, 0x80, 0x80 }, 5 }, { VBX_BAR_R, { 0x68, 0x80, 0x80, 0x80, 0x80 }, 5 } };
    const int ini = GetPrivateProfileIntA("Options", "ValueBox", 1, g_iniPath) != 0;
    const char *bars = "";
    if (!ini) {
        Log("options: value box off ([Options] ValueBox=0): v3.57's arrows (each row's own place, long values move the column)");
        return;
    }
    if (g_vbxHooks != 3) {
        Log("options: value box NOT on: %s not stock (another mod?): v3.57's arrows", !(g_vbxHooks & 1) ? "0052570E / 00525CE3" : "00525802");
        return;
    }
    g_vbxOn = 1;
    if (VbxBarWritten(VBX_BAR_L, hkVbxBarL) && VbxBarWritten(VBX_BAR_R, hkVbxBarR)) {
        g_vbxBarOn = 1;  // (already ours: a second install in this session)
    } else if (SiteOk(&kBar[0]) && SiteOk(&kBar[1])) {
        if (PokeBranch(VBX_BAR_L, 0xE8, hkVbxBarL, 5) && VbxBarWritten(VBX_BAR_L, hkVbxBarL) &&
            PokeBranch(VBX_BAR_R, 0xE8, hkVbxBarR, 5) && VbxBarWritten(VBX_BAR_R, hkVbxBarR)) {
            g_vbxBarOn = 1;
        } else {
            int k, back = 1;
            for (k = 0; k < 2; ++k)
                if (memcmp((const void *)(ULONG_PTR)kBar[k].va, kBar[k].stock, 5)) {
                    Poke(kBar[k].va, kBar[k].stock, 5);
                    back &= !memcmp((const void *)(ULONG_PTR)kBar[k].va, kBar[k].stock, 5);
                }
            LogAnomaly("options", "value box: writing the bar arrows' sites 005256B6 / 005256E2 failed: rolled back (%s); the volume "
                       "bars keep their stock arrows", back ? "the stock bytes are back" : "the stock bytes did not come back");
            bars = "; the volume bars' arrows stock (the write failed)";
        }
    } else {
        bars = "; 005256B6 / 005256E2 not stock: the volume bars' arrows stay where the game puts them";
    }
    Log("options: value box on: each page's values centred in one box, both arrows at its edges on every row and for every "
        "value, the mouse rects with them (0052570E, 00525802, 0052587C, 00525CE3%s)%s", g_vbxBarOn ? "; the volume bars' arrows "
        "0x5256B6 / 0x5256E2" : "", bars);
}
