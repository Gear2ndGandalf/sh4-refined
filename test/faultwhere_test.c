// v3.59 SD359: native test of src\faultwhere_sim.h (test\build_faultwhere_test.cmd): which modules are Direct3D or a GPU
// driver, the place of a fault, the codes the watch notes, and the rule "two Direct3D faults within a second".
#include <stdio.h>
#include "../src/faultwhere_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestModules(void)
{
    static const char *const kGfx[] = { "d3d9.dll", "D3D9.DLL", "ori_d3d8.dll", "d3d8.dll", "d3d11.dll", "dxgi.dll", "amdxx32.dll",
                                         "amdxc32.dll", "atidxx32.dll", "atiumdag.dll", "nvd3dum.dll", "nvwgf2um.dll", "nvldumd.dll",
                                         "igd10iumd32.dll", "igdumdim32.dll", "d3d12core.dll" };
    static const char *const kNot[] = { "winmm.dll", "SILENT HILL 4.exe", "dsound.dll", "USER32.dll", "ntdll.dll", "KERNELBASE.dll",
                                        "d3d9x.dll", "amd.dll", "nv.dll", "", "opera_browser.dll", "dinput8.dll", "d3dx9_43.dll" };
    size_t i;
    for (i = 0; i < sizeof kGfx / sizeof kGfx[0]; ++i) CHECK(FwGfxModule(kGfx[i]), "%s is Direct3D / a driver", kGfx[i]);
    for (i = 0; i < sizeof kNot / sizeof kNot[0]; ++i) CHECK(!FwGfxModule(kNot[i]), "%s is not", kNot[i]);
    CHECK(!FwGfxModule(NULL), "no name: not");
}

static void TestKind(void)
{
    CHECK(FwKind(1, 1, 0, 0) == FW_REFINED && FwKind(1, 0, 1, 0) == FW_GAME && FwKind(1, 0, 0, 1) == FW_GFX &&
              FwKind(1, 0, 0, 0) == FW_OTHER && FwKind(0, 0, 0, 0) == FW_UNKNOWN,
          "the place of a fault");
    CHECK(!strcmp(kFwKind[FW_GFX], "inside Direct3D or the graphics driver"), "the words");
    CHECK(FwNoted(0xC0000005ul) && FwNoted(0xC0000409ul) && FwNoted(0xC00000FDul), "errors are noted");
    CHECK(!FwNoted(0x80000003ul) && !FwNoted(0x80000001ul) && !FwNoted(0xE06D7363ul) && !FwNoted(0x406D1388ul) && !FwNoted(0x40010006ul),
          "breakpoints, guard pages, C++ throws, thread names, debug strings are not");
    CHECK(!strcmp(FwCodeName(0xC0000005ul), "access violation") && !strcmp(FwCodeName(0xC0000090ul), "floating-point fault") &&
              !strcmp(FwCodeName(0xC0000001ul), "exception"),
          "code names");
}

static void TestD3dRule(void)
{
    FwD3d s;
    memset(&s, 0, sizeof s);
    CHECK(!FwD3dStep(&s, 5000), "one fault: on");
    CHECK(!FwD3dStep(&s, 6200), "the next 1.2 s later: on");
    CHECK(FwD3dStep(&s, 7100), "two within a second (0.9 s): stop");
    CHECK(!FwD3dStep(&s, 7150) && s.n == 4, "once a session: no second stop");
    memset(&s, 0, sizeof s);
    CHECK(!FwD3dStep(&s, 0xFFFFFF00ul) && FwD3dStep(&s, 0x100ul), "across the tick counter's wrap (0.5 s)");
    memset(&s, 0, sizeof s);
    CHECK(!FwD3dStep(&s, 100) && FwD3dStep(&s, 1100), "exactly 1000 ms: stop");
    memset(&s, 0, sizeof s);
    // the 22:34 log: the warm-up's fault at .72, the vignette's failed texture (an hr, not a fault), the composite's fault at .745
    CHECK(!FwD3dStep(&s, 37720) && FwD3dStep(&s, 37745), "the 22:34 pattern (25 ms apart): stop");
}

int main(void)
{
    TestModules();
    TestKind();
    TestD3dRule();
    printf("faultwhere_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
