// v3.55 OL56: native test of src\volbar_sim.h (test\build_volbar_test.cmd), the 0x525672 volume-bar rule.
// 1. The stub VbLabelCmp run inside the game's own sequence (0x525678 `mov [esp+38h],ebp`, 0x52567C `jl text`, 0x525682
//    `cmp eax,26h`, 0x525685 `jg text`; Harness below): a bar exactly when VbBarRule says so, for the two stock volume
//    inputs with any label (the renamed Music / Effects Volume), the stock labels 0x25 / 0x26 with any input (the game's
//    own rule: StockMoves=0 and every other menu keep it), and never for any other record (labels 0..0x400, around 0x25 /
//    0x26, negative); the record and ebp / esp / ecx untouched.
// 2. The patch bytes: E8 rel32 to the stub + one nop; rel32 = to - (va + 5).
// 3. The stock exe (read only, if present: SH4Decomp\orig\sh4.exe, GOG v7) has 8B 41 14 83 F8 25 at 0x525672 (file offset
//    by its section table), the volume inputs' first bytes, and the text / bar branches after the site.
// Usage: out\volbar_test.exe
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/volbar_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned long g_ecxAfter, g_ebpAfter, g_espDelta;
// the game's code around the site, with the patched call: 1 = the bar path, 0 = the text path
static __declspec(naked) int __cdecl Harness(const unsigned long *rec)
{
    __asm {
        push ebp
        push ebx
        mov ebx, esp
        mov ecx, [esp + 0Ch]     // the record
        mov ebp, 1234h           // the x the game keeps in ebp
        sub esp, 40h             // room for [esp+38h]
        call VbLabelCmp          // 0x525672 (5 bytes) + nop
        mov [esp + 38h], ebp     // 0x525678: a mov (the flags stay)
        jl text                  // 0x52567C
        cmp eax, 26h             // 0x525682
        jg text                  // 0x525685
        mov eax, 1               // 0x52568B: the bar
        jmp done
    text:
        xor eax, eax             // 0x52570E: the value text
    done:
        add esp, 40h
        mov g_ecxAfter, ecx
        mov g_ebpAfter, ebp
        mov edx, esp
        sub edx, ebx
        mov g_espDelta, edx
        pop ebx
        pop ebp
        ret
    }
}

static int Bar(unsigned long input, unsigned long label)
{
    unsigned long rec[7] = { input, 0x413C80, 0, 0x654C58, 11, label, 0x523130 }, copy[7];
    int r;
    memcpy(copy, rec, sizeof rec);
    r = Harness(rec);
    CHECK(!memcmp(copy, rec, sizeof rec) && g_ecxAfter == (unsigned long)(size_t)rec && g_ebpAfter == 0x1234 && g_espDelta == 0,
          "input %06lX label %lX: the record, ecx, ebp and the stack stay", input, label);
    return r;
}

static void TestStub(void)
{
    static const unsigned long kIn[] = { VB_IN_BGM, VB_IN_SE, 0x524B60, 0x524C01, 0x524BFF, 0x524CCF, 0x524CD1, 0x524E40, 0, 0xFFFFFFFF,
                                         0x10001000, 0x524C00 ^ 0x100000 };
    long label;
    int i, bars = 0, texts = 0;
    for (i = 0; i < (int)(sizeof kIn / sizeof kIn[0]); ++i) {
        for (label = -3; label <= 0x400; ++label) {
            const unsigned long l = (unsigned long)label;
            const int want = VbBarRule(kIn[i], l), got = Bar(kIn[i], l);
            CHECK(got == want, "input %06lX label %lX: %s (the rule: %s)", kIn[i], l, got ? "bar" : "text", want ? "bar" : "text");
            if (got) ++bars; else ++texts;
        }
    }
    // the cases by name
    CHECK(Bar(VB_IN_BGM, 30547 + 10) && Bar(VB_IN_SE, 30547 + 11), "Music / Effects Volume with private labels: bars");
    CHECK(Bar(VB_IN_BGM, 0x25) && Bar(VB_IN_SE, 0x26), "BGM / SE Volume with the stock labels: bars (StockMoves=0)");
    CHECK(Bar(0x12345678, 0x25) && Bar(0x12345678, 0x26), "the game's own rule: a stock bar label on any record");
    CHECK(!Bar(0x524B60, 0x24) && !Bar(0x524B60, 0x27) && !Bar(0x524A20, 0x2A) && !Bar(0x524C00 + 1, 0x24), "other rows: text");
    CHECK(!Bar(0x524C00 + 0xD0 - 1, 0x157), "an input next to SE's: text");
    printf("stub: %d bar, %d text cases\n", bars, texts);
}

static void TestBytes(void)
{
    unsigned char b[VB_LEN];
    VbPatchBytes(VB_SITE, 0x10001000, b);
    CHECK(b[0] == 0xE8 && b[5] == 0x90, "E8 .. 90");
    {
        const unsigned long rel = (unsigned long)b[1] | (unsigned long)b[2] << 8 | (unsigned long)b[3] << 16 | (unsigned long)b[4] << 24;
        CHECK(rel == 0x10001000 - (VB_SITE + 5), "rel32 %08lX", rel);
        CHECK(VB_SITE + 5 + rel == 0x10001000, "the call lands on the stub");
    }
    VbPatchBytes(VB_SITE, 0x400000, b);  // a target below the site: a negative rel32
    CHECK(b[4] == 0xFF && ((unsigned long)b[1] | (unsigned long)b[2] << 8 | (unsigned long)b[3] << 16 | (unsigned long)b[4] << 24) + VB_SITE + 5 ==
          0x400000, "a backward call");
    CHECK(VB_LEN == 6 && kVbStock[0] == 0x8B && kVbStock[5] == 0x25, "6 stolen bytes");
}

// the stock exe, read only (skipped when absent)
static int ExeBytes(const unsigned char *img, long n, unsigned long va, unsigned char *out, int len)
{
    const unsigned long pe = *(const unsigned long *)(img + 0x3C);
    const int nsec = *(const unsigned short *)(img + pe + 6), so = *(const unsigned short *)(img + pe + 20);
    int i;
    for (i = 0; i < nsec; ++i) {
        const unsigned char *s = img + pe + 24 + so + 40 * i;
        const unsigned long vs = *(const unsigned long *)(s + 8), sva = *(const unsigned long *)(s + 12) + 0x400000,
                            rs = *(const unsigned long *)(s + 16), ro = *(const unsigned long *)(s + 20);
        const unsigned long size = vs > rs ? vs : rs;
        if (va >= sva && va + len <= sva + size && ro + (va - sva) + len <= (unsigned long)n) {
            memcpy(out, img + ro + (va - sva), len);
            return 1;
        }
    }
    return 0;
}
static void TestExe(void)
{
    const char *path = "sh4.exe";  /* a copy of your stock GOG v7 exe */
    FILE *f = fopen(path, "rb");
    unsigned char *img, b[32];
    long n;
    if (!f) {
        printf("(the stock exe %s is not here: its byte checks skipped)\n", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    img = (unsigned char *)malloc(n);
    if (!img || fread(img, 1, n, f) != (size_t)n) {
        fclose(f);
        CHECK(0, "reading %s", path);
        free(img);
        return;
    }
    fclose(f);
    CHECK(ExeBytes(img, n, VB_SITE, b, VB_LEN) && !memcmp(b, kVbStock, VB_LEN), "0x525672: 8B 41 14 83 F8 25");
    {   // the rest of the sequence the stub's flags feed: mov [esp+38h],ebp / jl 52570E / cmp eax,26h / jg 52570E
        static const unsigned char kAfter[19] = { 0x89, 0x6C, 0x24, 0x38, 0x0F, 0x8C, 0x8C, 0x00, 0x00, 0x00, 0x83, 0xF8, 0x26, 0x0F, 0x8F,
                                                  0x83, 0x00, 0x00, 0x00 };
        static const unsigned char kBefore[6] = { 0x8B, 0x4C, 0x24, 0x1C, 0x8B, 0xE8 };  // 0x52566C mov ecx,[esp+1Ch] (the record) / mov ebp,eax
        CHECK(ExeBytes(img, n, 0x525678, b, 19) && !memcmp(b, kAfter, 19), "0x525678..0x52568A: the jl / cmp / jg the flags feed");
        CHECK(ExeBytes(img, n, 0x52566C, b, 6) && !memcmp(b, kBefore, 6), "0x52566C: ecx = the record");
    }
    {   // the two inputs (push ebx / push 0 / call ...): real functions at those addresses
        static const unsigned char kIn[3] = { 0x53, 0x6A, 0x00 };
        CHECK(ExeBytes(img, n, VB_IN_BGM, b, 3) && !memcmp(b, kIn, 3), "0x524C00 is a row input");
        CHECK(ExeBytes(img, n, VB_IN_SE, b, 3) && !memcmp(b, kIn, 3), "0x524CD0 is a row input");
    }
    {   // the stock Option 1 records name them: BGM (row 3) 0x654CC0 + 3 * 0x1C, SE (row 4) + 4 * 0x1C: input, label 0x25 / 0x26
        unsigned long r[7];
        CHECK(ExeBytes(img, n, 0x654CC0 + 3 * 0x1C, (unsigned char *)r, 28) && r[0] == VB_IN_BGM && r[5] == 0x25, "Option 1 row 3: BGM (%06lX, %lX)", r[0], r[5]);
        CHECK(ExeBytes(img, n, 0x654CC0 + 4 * 0x1C, (unsigned char *)r, 28) && r[0] == VB_IN_SE && r[5] == 0x26, "Option 1 row 4: SE (%06lX, %lX)", r[0], r[5]);
    }
    free(img);
}

int main(void)
{
    TestStub();
    TestBytes();
    TestExe();
    printf("%s: %d checks, %d failed\n", g_fail ? "FAILED" : "OK", g_n, g_fail);
    return g_fail ? 1 : 0;
}
