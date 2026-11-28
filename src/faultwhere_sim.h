// v3.59 SD359: where a fault happened, and the Direct3D fault rule (loghealth.inc; staging/v359_feedback/w10_sound/PLAN.md,
// LOGCHECK_v358.md section 2: the 22:34 crash was dgVoodoo's d3d9.dll after an AMD driver event, and Refined's two guards that
// swallowed faults first logged only a code). The pure part, no Windows: test\faultwhere_test.c builds it alone.
// - FwGfxModule: a module of the display chain or a GPU driver's user-mode part, by its file name.
// - FwKind: inside SH4 Refined / the game / Direct3D or the driver / another module.
// - FwD3dStep: two Direct3D faults within FW_D3D_MS stop Refined's own Direct3D work for the session (the warm-up already
//   stops itself after three).
#ifndef FAULTWHERE_SIM_H
#define FAULTWHERE_SIM_H

#include <string.h>

#define FW_D3D_MS 1000ul  // two Direct3D faults within this: Refined's Direct3D work stops
enum { FW_REFINED, FW_GAME, FW_GFX, FW_OTHER, FW_UNKNOWN, FW_N };
static const char *const kFwKind[FW_N] = { "inside SH4 Refined", "inside the game", "inside Direct3D or the graphics driver",
                                          "inside another module", "outside any module" };

static int FwLower(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
static int FwStartsI(const char *s, const char *pre)
{
    for (; *pre; ++s, ++pre)
        if (FwLower((unsigned char)*s) != *pre) return 0;
    return 1;
}
static int FwSameI(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (FwLower((unsigned char)*a) != FwLower((unsigned char)*b)) return 0;
    return !*a && !*b;
}
// name = a module's file name without its folder
static int FwGfxModule(const char *name)
{
    static const char *const kExact[] = { "d3d8.dll", "ori_d3d8.dll", "d3d9.dll", "d3d10.dll", "d3d10_1.dll", "d3d10core.dll",
                                          "d3d10_1core.dll", "d3d11.dll", "d3d12.dll", "d3d12core.dll", "dxgi.dll", "d3d9on12.dll",
                                          "d3d11on12.dll", "dxcore.dll", "dgvoodoo.dll", "ddraw.dll" };
    static const char *const kPrefix[] = { "amdxx", "amdxc", "amdxn", "atidxx", "atiumd", "aticfx", "atiu9p", "atiadl", "amdihk",
                                           "nvd3dum", "nvwgf2um", "nvldumd", "nvumdshim", "nvgpucomp", "nvoglv",
                                           "igd10iumd", "igd12umd", "igdumdim", "igd9dxva", "igxelpicd", "igdgmm", "igc32", "igdusc" };
    size_t i;
    if (!name || !*name) return 0;
    for (i = 0; i < sizeof kExact / sizeof kExact[0]; ++i)
        if (FwSameI(name, kExact[i])) return 1;
    for (i = 0; i < sizeof kPrefix / sizeof kPrefix[0]; ++i)
        if (FwStartsI(name, kPrefix[i])) return 1;
    return 0;
}
// the fault's place: self = this dll's image, exe = the game's image, gfx = FwGfxModule, haveModule = an image at all
static int FwKind(int haveModule, int self, int exe, int gfx)
{
    if (!haveModule) return FW_UNKNOWN;
    if (self) return FW_REFINED;
    if (exe) return FW_GAME;
    if (gfx) return FW_GFX;
    return FW_OTHER;
}
static const char *FwCodeName(unsigned long code)
{
    switch (code) {
    case 0xC0000005ul: return "access violation";
    case 0xC000001Dul: return "illegal instruction";
    case 0xC0000096ul: return "privileged instruction";
    case 0xC0000094ul: return "integer divide by zero";
    case 0xC0000095ul: return "integer overflow";
    case 0xC00000FDul: return "stack overflow";
    case 0xC0000409ul: return "stack buffer overrun";
    case 0xC0000374ul: return "heap corruption";
    case 0xC0000006ul: return "in-page error";
    case 0xC000008Cul: return "array bounds";
    }
    if (code >= 0xC000008Dul && code <= 0xC0000093ul) return "floating-point fault";
    return "exception";
}
// the exceptions the fault watch notes: errors only (0xC...): not breakpoints, guard pages, C++ throws (0xE06D7363), the
// debugger's thread names (0x406D1388) or OutputDebugString (0x40010006)
static int FwNoted(unsigned long code) { return (code & 0xF0000000ul) == 0xC0000000ul; }

// two Direct3D faults within FW_D3D_MS: 1 = this one trips the rule (once a session; the caller stops the work)
typedef struct { unsigned long last; int have, n, tripped; } FwD3d;
static int FwD3dStep(FwD3d *s, unsigned long nowMs)
{
    const int close = s->have && nowMs - s->last <= FW_D3D_MS;
    ++s->n;
    s->have = 1, s->last = nowMs;
    if (close && !s->tripped) {
        s->tripped = 1;
        return 1;
    }
    return 0;
}

#endif
