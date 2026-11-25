// v3.39 PR339: native test of the Renderer row (src\renderer_sim.h: MD5, the manifest, the state, the choices; src\renderer.inc:
// the file side on a folder of its own, test\out\rd_game) and of the Opening setting (src\opening_sim.h).
//   rend_test.exe [game folder]   with the game folder: also the md5s of its dgVoodoo_variants files against its manifest
//                                 (read only; skipped when the folder or the files are not there)
// test\build_rend_test.cmd builds and runs it (x86 /O2 /W3 as build.cmd).
#define _CRT_SECURE_NO_WARNINGS  // (test code: plain strcpy / sprintf)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int g_logs, g_anoms;
static void Log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("      log | ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    ++g_logs;
}
static void LogV(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("      [v] | ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}
static void LogAnomaly(const char *area, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("      ANOMALY: %s: ", area);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    ++g_anoms;
}

#include "../src/renderer_sim.h"
#include "../src/renderer.inc"
#include "../src/opening_sim.h"

static int g_n, g_fail;
static void Check(int ok, const char *fmt, ...)
{
    va_list ap;
    ++g_n;
    if (ok) return;
    ++g_fail;
    va_start(ap, fmt);
    printf("FAIL  ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

// ---------------------------------------------------------------- MD5
static void TestMd5(void)
{
    static const struct { const char *in, *md5; } kV[] = {  // RFC 1321 A.5
        { "", "D41D8CD98F00B204E9800998ECF8427E" },
        { "a", "0CC175B9C0F1B6A831C399E269772661" },
        { "abc", "900150983CD24FB0D6963F7D28E17F72" },
        { "message digest", "F96B697D7CB7938D525A2F31AAF161D0" },
        { "abcdefghijklmnopqrstuvwxyz", "C3FCD3D76192E4007DFB496CCA67E13B" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "D174AB98D277D9F5A5611C2C9F419D9F" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57EDF4A22BE3C955AC49DA2E2107B67A" },
        { "The quick brown fox jumps over the lazy dog", "9E107D9D372BB6826BD81D3542A419D6" },
    };
    unsigned char buf[300];
    char a[33], b[33];
    int i, cut;
    for (i = 0; i < (int)(sizeof kV / sizeof kV[0]); ++i) {
        RdMd5Of(kV[i].in, strlen(kV[i].in), a);
        Check(!strcmp(a, kV[i].md5), "md5(\"%s\") = %s, want %s", kV[i].in, a, kV[i].md5);
    }
    for (i = 0; i < (int)sizeof buf; ++i) buf[i] = (unsigned char)(i * 131 + 7);
    for (i = 0; i <= (int)sizeof buf; ++i) {  // every length (the 55 / 56 / 63 / 64 padding edges): one call = byte by byte
        RdMd5 c;
        int k;
        RdMd5Of(buf, (size_t)i, a);
        RdMd5Init(&c);
        for (k = 0; k < i; ++k) RdMd5Update(&c, buf + k, 1);
        RdMd5Final(&c, b);
        Check(!strcmp(a, b), "md5 of %d bytes: one call %s, byte by byte %s", i, a, b);
    }
    for (cut = 0; cut <= 300; cut += 7) {  // any split into two updates
        RdMd5 c;
        RdMd5Of(buf, 300, a);
        RdMd5Init(&c);
        RdMd5Update(&c, buf, (size_t)cut);
        RdMd5Update(&c, buf + cut, (size_t)(300 - cut));
        RdMd5Final(&c, b);
        Check(!strcmp(a, b), "md5 split at %d: %s vs %s", cut, a, b);
    }
    {   // a million 'a' (a long message through the 64-byte blocks)
        static char m[1000000];
        memset(m, 'a', sizeof m);
        RdMd5Of(m, sizeof m, a);
        Check(!strcmp(a, "7707D6AE4E027C70EEA2A935C2296F21"), "md5 of a million a = %s", a);
    }
}

// ---------------------------------------------------------------- the manifest (Graphics Plus's, 2026-11-21 23:31)
static const char kManifest[] =
    "# SH4 Graphics Plus dgVoodoo conf variants (written by install_variants_folder.ps1; do not edit).\r\n"
    "# A program may copy <file> over ..\\dgVoodoo.conf only if: the game is not yet drawing with it (it applies at the next start);\r\n"
    "# ..\\d3d9.dll has md5 CAA75C55D9B3121D17433BD3B38C7BB2 (dgVoodoo2 2.87.6); the current ..\\dgVoodoo.conf md5 is one of the md5s below;\r\n"
    "# <file> has its md5 below. Write a temporary file, check its md5, then replace dgVoodoo.conf. Rows with an empty file are\r\n"
    "# ours but not offered. Lines starting with # are comments.\r\n"
    "name,file,md5,label\r\n"
    "d3d11_flipdiscard,d3d11_flipdiscard.conf,702E9E0AF46385A7B59091D5ACA3C404,Direct3D 11\r\n"
    "d3d11fd_rt16,d3d11fd_rt16.conf,C7E9253A95A07A1466306F417978204A,Direct3D 11 (16-bit)\r\n"
    "d3d12,d3d12.conf,67FDE0CF1FA6E95EFB53F0F398540F4E,Direct3D 12\r\n"
    "passthru,passthru.conf,C500F5C9E089D915288AD20FF89CD68A,Stock\r\n"
    "d3d11,,6643C79F839016623A083B687C4FAC34,\r\n"
    "d3d12_10bit,,A38A9DBEDA5F91B8CAD8B7836F05A885,\r\n"
    "d3d12_hdr,,4FFF0B9E29A63CA772F9E7FE8FE15E5D,\r\n"
    "d3d11fd_rt10,,94912DBFE6D57EB23F47A1B8DC13A893,\r\n";
#define MD5_RT16 "C7E9253A95A07A1466306F417978204A"
#define MD5_HDR "4FFF0B9E29A63CA772F9E7FE8FE15E5D"

static int Parse(const char *s, RdManifest *m) { return RdParseManifest(s, strlen(s), m); }
static char *Swap(const char *s, const char *from, const char *to)  // a copy of s with the first `from` replaced
{
    static char out[RD_MANIFEST_MAX + 256];
    const char *p = strstr(s, from);
    size_t a;
    if (!p) return strcpy(out, s);
    a = (size_t)(p - s);
    memcpy(out, s, a);
    strcpy(out + a, to);
    strcat(out, p + strlen(from));
    return out;
}
// v3.39 REV339: the d3d9.dll pins from `#d3d9.dll=` lines (Graphics Plus's 2026-11-22 manifest has one)
static void TestPins(void)
{
    static const char kHead[] = "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n";
    char s[1024];
    RdManifest m;
    Check(Parse(kManifest, &m) && m.nPin == 0, "the old manifest: no pin line");
    Check(RdD3d9Allowed(&m, "CAA75C55D9B3121D17433BD3B38C7BB2", "CAA75C55D9B3121D17433BD3B38C7BB2") &&
              !RdD3d9Allowed(&m, "CAA75C55D9B3121D17433BD3B38C7BB2", "00000000000000000000000000000000"), "no pin line: the built-in pin");
    sprintf(s, "# a comment\r\n#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2\r\n%s", kHead);
    Check(Parse(s, &m) && m.nPin == 1 && !strcmp(m.pin[0], "CAA75C55D9B3121D17433BD3B38C7BB2"), "one pin line (%s)", m.err);
    sprintf(s, "#d3d9.dll=caa75c55d9b3121d17433bd3b38c7bb2  \r\n#d3d9.dll=11111111111111111111111111111111\r\n"
               "#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2\r\n%s", kHead);
    Check(Parse(s, &m) && m.nPin == 2 && !strcmp(m.pin[0], "CAA75C55D9B3121D17433BD3B38C7BB2") &&
              !strcmp(m.pin[1], "11111111111111111111111111111111"), "several pin lines (lower case, trailing spaces, a repeat)");
    Check(RdD3d9Allowed(&m, NULL, "11111111111111111111111111111111") && RdD3d9Allowed(&m, NULL, "CAA75C55D9B3121D17433BD3B38C7BB2") &&
              !RdD3d9Allowed(&m, "22222222222222222222222222222222", "22222222222222222222222222222222"),
          "with pin lines only they count (the fallback is not used)");
    sprintf(s, "#d3d9.dll=11111111111111111111111111111111\r\n%s", kHead);
    Parse(s, &m);
    Check(!RdD3d9Allowed(&m, "CAA75C55D9B3121D17433BD3B38C7BB2", "CAA75C55D9B3121D17433BD3B38C7BB2"), "a pin line replaces the built-in pin");
    {
        static const char *const kBadPin[] = { "#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB\r\n", "#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2X\r\n",
                                               "#d3d9.dll=GAA75C55D9B3121D17433BD3B38C7BB2\r\n", "#d3d9.dll=\r\n" };
        int i;
        for (i = 0; i < 4; ++i) {
            sprintf(s, "%s%s", kBadPin[i], kHead);
            Check(!Parse(s, &m) && strstr(m.err, "#d3d9.dll="), "a malformed pin line breaks the manifest (%d: %s)", i, m.err);
        }
        s[0] = 0;
        for (i = 0; i < 9; ++i) sprintf(s + strlen(s), "#d3d9.dll=%032X\r\n", i + 1);
        strcat(s, kHead);
        Check(!Parse(s, &m), "9 pin lines are too many (%s)", m.err);
    }
    sprintf(s, "# d3d9.dll=11111111111111111111111111111111\r\n#D3D9.DLL=11111111111111111111111111111111\r\n"
               " #d3d9.dll=11111111111111111111111111111111\r\n%s#d3d9.dll=11111111111111111111111111111111\r\n#d3d9.dll=bad\r\n", kHead);
    Check(Parse(s, &m) && m.nPin == 0, "not exactly the prefix, or after the header: a plain comment (%s)", m.err);
}

static void TestManifest(void)
{
    RdManifest m;
    char lf[sizeof kManifest], bom[sizeof kManifest + 3], big[RD_MANIFEST_MAX + 64];
    int i, k;
    Check(Parse(kManifest, &m) && m.ok, "the shipped manifest parses (%s, line %d)", m.err, m.line);
    Check(m.n == 8 && m.nOffered == 4, "8 rows, 4 offered (%d, %d)", m.n, m.nOffered);
    Check(!strcmp(m.row[0].name, "d3d11_flipdiscard") && !strcmp(m.row[0].file, "d3d11_flipdiscard.conf") &&
              !strcmp(m.row[0].label, "Direct3D 11") && m.row[0].offered, "row 0");
    Check(!strcmp(m.row[1].label, "Direct3D 11 (16-bit)") && !strcmp(m.row[1].md5, MD5_RT16), "row 1 (the user's)");
    Check(!strcmp(m.row[3].label, "Stock") && !strcmp(m.row[3].name, "passthru"), "row 3 Stock");
    Check(!m.row[6].offered && !m.row[6].file[0] && !m.row[6].label[0] && !strcmp(m.row[6].name, "d3d12_hdr"), "row 6 recognised only");
    Check(RdFindMd5(&m, MD5_RT16) == 1 && RdFindMd5(&m, "c7e9253a95a07a1466306f417978204a") == 1, "md5 lookup, any case");
    Check(RdFindMd5(&m, "C7E9253A95A07A1466306F417978204") < 0 && RdFindMd5(&m, "C7E9253A95A07A1466306F417978204AA") < 0 &&
              RdFindMd5(&m, NULL) < 0, "md5 lookup: 31 / 33 digits / NULL");
    Check(RdFindName(&m, "D3D12") == 2 && RdFindName(&m, "nothing") < 0, "name lookup");
    for (i = k = 0; kManifest[i]; ++i)
        if (kManifest[i] != '\r') lf[k++] = kManifest[i];
    lf[k] = 0;
    Check(Parse(lf, &m) && m.n == 8, "LF line ends parse too");
    lf[k - 1] = 0;  // no line end after the last row
    Check(Parse(lf, &m) && m.n == 8 && !strcmp(m.row[7].md5, "94912DBFE6D57EB23F47A1B8DC13A893"), "no last line end");
    bom[0] = (char)0xEF, bom[1] = (char)0xBB, bom[2] = (char)0xBF;
    strcpy(bom + 3, kManifest);
    Check(Parse(bom, &m) && m.n == 8, "a UTF-8 mark at the start is skipped");
    Check(Parse("name,file,md5,label\r\n\r\n   \r\n# c\r\n  # indented comment\r\nx,x.conf,702e9e0af46385a7b59091d5aca3c404,  X label  \r\n", &m) &&
              m.n == 1 && !strcmp(m.row[0].md5, "702E9E0AF46385A7B59091D5ACA3C404") && !strcmp(m.row[0].label, "X label"),
          "blank lines, comments, trimmed fields, the md5 upper-cased (%s)", m.err);
    Check(Parse(" Name , FILE ,Md5, label \r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", &m), "the header in any case, spaces");
    Check(Parse("name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,\r\n", &m) && !strcmp(m.row[0].label, "x"),
          "an offered row without a label shows its name");
    Check(Parse("name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,An extremely long label that goes on and on\r\n", &m) &&
              strlen(m.row[0].label) <= RD_LABEL_MAX && !strncmp(m.row[0].label, "An extremely long label that go", 31),
          "a long label is cut to 31 (\"%s\")", m.row[0].label);
    {   // broken ones: every one greys the row
        static const struct { const char *s, *why; } kBad[] = {
            { "", "empty" },
            { "# only comments\r\n", "no header" },
            { "x,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "no header (a row first)" },
            { "name,file,md5\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404\r\n", "a 3-field header" },
            { "name,file,label,md5\r\nx,x.conf,X,702E9E0AF46385A7B59091D5ACA3C404\r\n", "another header" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404\r\n", "3 fields" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X,Y\r\n", "5 fields" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C40,X\r\n", "an md5 of 31" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404A,X\r\n", "an md5 of 33" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C40G,X\r\n", "an md5 with G" },
            { "name,file,md5,label\r\n,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "no name" },
            { "name,file,md5,label\r\nx y,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "a space in the name" },
            { "name,file,md5,label\r\nx,..\\d3d9.dll,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "a path out of the folder" },
            { "name,file,md5,label\r\nx,sub/x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "a sub-folder" },
            { "name,file,md5,label\r\nx,C:x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "a drive" },
            { "name,file,md5,label\r\nx,..,702E9E0AF46385A7B59091D5ACA3C404,X\r\n", "a dot name" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\nX,y.conf,C7E9253A95A07A1466306F417978204A,Y\r\n", "a name twice (any case)" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,X\r\ny,y.conf,702e9e0af46385a7b59091d5aca3c404,Y\r\n", "an md5 twice" },
            { "name,file,md5,label\r\nx,,702E9E0AF46385A7B59091D5ACA3C404,\r\n", "no offered row" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,Dire\xE7t\r\n", "a byte that is not ASCII" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,Di\x01rect\r\n", "a control character" },
            { "name,file,md5,label\r\nx,x.conf,702E9E0AF46385A7B59091D5ACA3C404,\"Direct3D\"\r\nname,file,md5,label\r\n", "the header line again (its md5 field is no md5)" },
        };
        for (i = 0; i < (int)(sizeof kBad / sizeof kBad[0]); ++i) {
            const int r = Parse(kBad[i].s, &m);
            Check(!r && !m.ok && m.err[0], "broken (%s): parse says %d (%s, line %d)", kBad[i].why, r, m.err, m.line);
        }
    }
    Check(!Parse(Swap(kManifest, "d3d12,d3d12.conf", "d3d12,d3d12.conf,extra"), &m), "a real row with a 5th field");
    Check(!Parse(Swap(kManifest, "C7E9253A95A07A1466306F417978204A", "702E9E0AF46385A7B59091D5ACA3C404"), &m), "a real md5 twice");
    {   // 16 rows parse, 17 do not
        char s[4096];
        strcpy(s, "name,file,md5,label\r\n");
        for (i = 0; i < 17; ++i) {
            char r[96];
            sprintf(r, "n%d,f%d.conf,%032X,L%d\r\n", i, i, i + 1, i);
            strcat(s, r);
            if (i == 15) Check(Parse(s, &m) && m.n == 16 && m.nOffered == 16, "16 rows parse");
        }
        Check(!Parse(s, &m), "17 rows do not (%s)", m.err);
        Check(RdParseManifest(s, strlen(s), &m) == 0, "17 rows again");
    }
    memset(big, '#', sizeof big - 1);
    big[sizeof big - 1] = 0;
    Check(!RdParseManifest(big, sizeof big - 1, &m) && strstr(m.err, "16 KB"), "a file over 16 KB is refused");
    Check(!RdParseManifest(NULL, 0, &m), "no bytes");
}

// ---------------------------------------------------------------- the state, the values, the steps
static void TestState(void)
{
    RdManifest m;
    RdState s;
    char t[64];
    int src[RD_ROWS_MAX], i;
    Parse(kManifest, &m);
    RdDecide(&m, 1, "702E9E0AF46385A7B59091D5ACA3C404", NULL, &s);  // v3.44 FN344: the user's live conf (Direct3D 11 since 2026-11-22)
    Check(s.st == RD_ST_OFFERED && !s.greyed && s.nChoice == 3 && s.nVal == 3 && s.cur == 0 && !s.extra && s.row == 0 && s.alias < 0,
          "the user's set-up: offered, value 1 of 3 (st %d cur %d nVal %d alias %d)", s.st, s.cur, s.nVal, s.alias);
    Check(s.choice[0] == 0 && s.choice[1] == 2 && s.choice[2] == 3, "v3.44: the choices are rows 0, 2, 3 (16-bit retired)");
    RdValueText(&m, &s, 0, t, sizeof t);
    Check(!strcmp(t, "Direct3D 11"), "value 1 \"%s\"", t);
    RdValueText(&m, &s, 1, t, sizeof t);
    Check(!strcmp(t, "Direct3D 12"), "value 2 \"%s\"", t);
    RdValueText(&m, &s, 2, t, sizeof t);
    Check(!strcmp(t, "Stock"), "value 3 \"%s\"", t);
    Check(RdStep(&s, 1, 1) == 2 && RdStep(&s, 2, 1) == 0 && RdStep(&s, 0, -1) == 2 && RdStep(&s, 1, -1) == 0, "the ring of 3");
    Check(RdTargetRow(&s, 0) < 0 && RdTargetRow(&s, 1) == 2 && RdTargetRow(&s, 2) == 3 && RdTargetRow(&s, 3) < 0 &&
              RdTargetRow(&s, -1) < 0, "targets: the live one is none, the others their rows");
    Check(RdDescKind(&m, &s, 0) == RD_DK_D3D11 && RdDescKind(&m, &s, 1) == RD_DK_D3D12 && RdDescKind(&m, &s, 2) == RD_DK_STOCK,
          "help texts by Graphics Plus's names");
    {   // v3.44 FN344: a live Direct3D 11 (16-bit) conf (the user's before 2026-11-22) reads as Direct3D 11
        Check(RdRetiredAs("d3d11fd_rt16") && !strcmp(RdRetiredAs("D3D11FD_RT16"), "d3d11_flipdiscard") && !RdRetiredAs("d3d11_flipdiscard") &&
                  !RdRetiredAs(NULL) && !RdRetiredAs("d3d11fd_rt10"), "v3.44: d3d11fd_rt16 is the one retired row");
        RdDecide(&m, 1, MD5_RT16, NULL, &s);
        Check(s.st == RD_ST_OFFERED && !s.greyed && s.nVal == 3 && s.cur == 0 && s.row == 1 && s.alias == 1 && !s.extra,
              "v3.44: a 16-bit conf reads as Direct3D 11 (st %d cur %d nVal %d row %d alias %d)", s.st, s.cur, s.nVal, s.row, s.alias);
        RdValueText(&m, &s, s.cur, t, sizeof t);
        Check(!strcmp(t, "Direct3D 11") && RdDescKind(&m, &s, s.cur) == RD_DK_D3D11, "v3.44: shown as \"%s\"", t);
        Check(RdTargetRow(&s, 0) == 0 && RdTargetRow(&s, 1) == 2 && RdTargetRow(&s, 2) == 3,
              "v3.44: its value targets Direct3D 11's row (the start replaces the conf with it)");
        for (i = 0; i < RD_ROWS_MAX; ++i) src[i] = 1;
        src[0] = 0;  // Direct3D 11's file gone: nothing to read it as
        RdDecide(&m, 1, MD5_RT16, src, &s);
        RdValueText(&m, &s, s.cur, t, sizeof t);
        Check(s.st == RD_ST_KNOWN && s.alias < 0 && s.nVal == 3 && !strcmp(t, "Graphics Plus: d3d11fd_rt16"),
              "v3.44: without Direct3D 11's file a 16-bit conf is recognised only (\"%s\")", t);
        {
            RdManifest only;
            Parse("name,file,md5,label\r\nd3d11fd_rt16,d3d11fd_rt16.conf," MD5_RT16 ",Direct3D 11 (16-bit)\r\n", &only);
            RdDecide(&only, 1, MD5_RT16, NULL, &s);
            Check(only.ok && s.st == RD_ST_NOSOURCE && s.greyed, "v3.44: a manifest offering only the retired row: no choice, greyed");
        }
    }
    Check(RdVariantName(&m, MD5_RT16, t, sizeof t) && !strcmp(t, "Direct3D 11 (16-bit)"), "the live variant \"%s\"", t);
    Check(RdVariantName(&m, MD5_HDR, t, sizeof t) && !strcmp(t, "Graphics Plus d3d12_hdr"), "a recognised-only variant \"%s\"", t);
    Check(!RdVariantName(&m, "00000000000000000000000000000000", t, sizeof t) && !RdVariantName(&m, NULL, t, sizeof t), "no variant");

    RdDecide(&m, 1, MD5_HDR, NULL, &s);  // a recognised-only conf
    Check(s.st == RD_ST_KNOWN && !s.greyed && s.extra && s.nVal == 4 && s.cur == 3 && s.row == 6, "recognised only: value 4 of 4");
    RdValueText(&m, &s, 3, t, sizeof t);
    Check(!strcmp(t, "Graphics Plus: d3d12_hdr"), "its value \"%s\"", t);
    Check(RdStep(&s, 3, 1) == 0 && RdStep(&s, 3, -1) == 2 && RdStep(&s, 2, 1) == 3, "the state value is in the ring");
    Check(RdTargetRow(&s, 3) < 0 && RdTargetRow(&s, 1) == 2, "picking it again replaces nothing; a choice replaces it");
    Check(RdDescKind(&m, &s, 3) == RD_DK_KNOWN, "its help text");

    RdDecide(&m, 1, "0123456789ABCDEF0123456789ABCDEF", NULL, &s);  // the user's own conf
    Check(s.st == RD_ST_CUSTOM && s.greyed && s.nVal == 4 && s.cur == 3, "an unknown md5: Custom, greyed");
    RdValueText(&m, &s, s.cur, t, sizeof t);
    Check(!strcmp(t, "Custom") && RdStep(&s, s.cur, 1) == s.cur && RdTargetRow(&s, 0) < 0 && RdDescKind(&m, &s, s.cur) == RD_DK_CUSTOM,
          "Custom never moves nor replaces (\"%s\")", t);
    RdDecide(&m, 1, NULL, NULL, &s);
    Check(s.st == RD_ST_CUSTOM && s.greyed, "a missing dgVoodoo.conf: Custom (never created)");

    RdDecide(&m, 0, MD5_RT16, NULL, &s);
    RdValueText(&m, &s, s.cur, t, sizeof t);
    Check(s.st == RD_ST_NOD3D9 && s.greyed && s.nVal == 1 && s.cur == 0 && !strcmp(t, "Unavailable") &&
              RdDescKind(&m, &s, 0) == RD_DK_UNAVAIL && RdTargetRow(&s, 0) < 0, "another d3d9.dll: Unavailable, greyed");
    {
        RdManifest bad;
        Parse("name,file,md5\r\n", &bad);
        RdDecide(&bad, 1, MD5_RT16, NULL, &s);
        Check(s.st == RD_ST_NOMANIFEST && s.greyed && s.nVal == 1, "a broken manifest: greyed");
        RdDecide(NULL, 1, MD5_RT16, NULL, &s);
        Check(s.st == RD_ST_NOMANIFEST && s.greyed, "no manifest: greyed");
    }
    for (i = 0; i < RD_ROWS_MAX; ++i) src[i] = 1;
    src[2] = 0;  // d3d12.conf gone
    RdDecide(&m, 1, MD5_RT16, src, &s);
    Check(s.st == RD_ST_OFFERED && s.nChoice == 2 && s.choice[1] == 3 && s.cur == 0 && s.alias == 1, "a missing variant file is no choice");
    RdDecide(&m, 1, "67FDE0CF1FA6E95EFB53F0F398540F4E", src, &s);
    RdValueText(&m, &s, s.cur, t, sizeof t);
    Check(s.st == RD_ST_KNOWN && s.nVal == 3 && !strcmp(t, "Graphics Plus: d3d12"), "a conf whose file is gone: recognised (\"%s\")", t);
    for (i = 0; i < RD_ROWS_MAX; ++i) src[i] = 0;
    RdDecide(&m, 1, MD5_RT16, src, &s);
    Check(s.st == RD_ST_NOSOURCE && s.greyed && s.nVal == 1, "no variant file at all: greyed");
    {   // more offered rows than the row shows: the first RD_CHOICE_MAX
        char big[2048];
        RdManifest mm;
        strcpy(big, "name,file,md5,label\r\n");
        for (i = 0; i < 9; ++i) {
            char r[96];
            sprintf(r, "n%d,f%d.conf,%032X,L%d\r\n", i, i, i + 1, i);
            strcat(big, r);
        }
        Parse(big, &mm);
        RdDecide(&mm, 1, "00000000000000000000000000000008", NULL, &s);  // row 7: past the choices
        Check(s.nChoice == RD_CHOICE_MAX && s.st == RD_ST_KNOWN && s.nVal == RD_VALS && s.cur == RD_CHOICE_MAX,
              "9 offered rows: %d choices, the 8th is recognised", RD_CHOICE_MAX);
        RdValueText(&mm, &s, s.cur, t, sizeof t);
        Check(!strcmp(t, "Graphics Plus: n7"), "\"%s\"", t);
    }
    {   // a long name is cut in the value
        RdManifest mm;
        Parse("name,file,md5,label\r\na,a.conf,00000000000000000000000000000001,A\r\nabcdefghijklmnopqrstuvwxyz_0123,,00000000000000000000000000000002,\r\n", &mm);
        RdDecide(&mm, 1, "00000000000000000000000000000002", NULL, &s);
        RdValueText(&mm, &s, s.cur, t, sizeof t);
        Check(mm.ok && !strcmp(t, "Graphics Plus: abcdefghijklmnopqrst") && strlen(t) < 40, "a long name cut to 20 (\"%s\")", t);
        RdValueText(&mm, &s, s.cur, t, 10);
        Check(strlen(t) == 9, "a small buffer (\"%s\")", t);
    }
}

// ---------------------------------------------------------------- the file side on a folder of its own
static char g_game[MAX_PATH];
static void Put(const char *rel, const void *data, size_t n)
{
    char p[MAX_PATH];
    FILE *f;
    RdPath(g_game, rel, p, sizeof p);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    f = fopen(p, "wb");
    if (f) {
        fwrite(data, 1, n, f);
        fclose(f);
    }
}
static void PutS(const char *rel, const char *s) { Put(rel, s, strlen(s)); }
static int Md5Of(const char *rel, char out[33])
{
    char p[MAX_PATH];
    RdPath(g_game, rel, p, sizeof p);
    return RdMd5File(p, out, RD_CONF_MAX) == 1;
}
static int Exists(const char *rel)
{
    char p[MAX_PATH];
    RdPath(g_game, rel, p, sizeof p);
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}
static int CountVariants(void)
{
    char p[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int n = 0;
    RdPath(g_game, "dgVoodoo_variants\\*", p, sizeof p);
    h = FindFirstFileA(p, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++n;
    while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}
static void TestFiles(void)
{
    static const char *const kName[6] = { "d3d11_flipdiscard", "d3d11fd_rt16", "d3d12", "passthru", "d3d12_hdr", "d3d11" };
    static const char *const kLabel[4] = { "Direct3D 11", "Direct3D 11 (16-bit)", "Direct3D 12", "Stock" };
    char body[6][256], md5[6][33], pin[33], man[2048], now[33], msg[512], v0[4][33];
    char p[MAX_PATH];
    RdInfo info;
    int i, r;
    GetFullPathNameA("out\\rd_game", sizeof g_game, g_game, NULL);
    CreateDirectoryA("out", NULL);
    CreateDirectoryA(g_game, NULL);
    RdPath(g_game, "dgVoodoo_variants", p, sizeof p);
    CreateDirectoryA(p, NULL);
    {   // a clean start: no conf, no .new
        static const char *const kDel[] = { "dgVoodoo.conf", "dgVoodoo.conf.new", "d3d9.dll", "dgVoodoo_variants\\manifest.csv" };
        for (i = 0; i < 4; ++i) {
            RdPath(g_game, kDel[i], p, sizeof p);
            SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(p);
        }
    }
    for (i = 0; i < 6; ++i) {  // six "confs": four offered (files), two recognised only (no file)
        sprintf(body[i], "[General]\r\n; test conf %s\r\nOutputAPI = %s\r\n", kName[i], i == 2 ? "d3d12_fl12_0" : "d3d11_fl11_0");
        RdMd5Of(body[i], strlen(body[i]), md5[i]);
    }
    for (i = 0; i < 4; ++i) {
        char rel[96];
        sprintf(rel, "dgVoodoo_variants\\%s.conf", kName[i]);
        PutS(rel, body[i]);
    }
    strcpy(man, "# test manifest\r\nname,file,md5,label\r\n");
    for (i = 0; i < 6; ++i) {
        char row[200];
        if (i < 4) sprintf(row, "%s,%s.conf,%s,%s\r\n", kName[i], kName[i], md5[i], kLabel[i]);
        else sprintf(row, "%s,,%s,\r\n", kName[i], md5[i]);
        strcat(man, row);
    }
    PutS("dgVoodoo_variants\\manifest.csv", man);
    PutS("d3d9.dll", "MZ not really dgVoodoo2, a test stand-in");
    Check(Md5Of("d3d9.dll", pin), "the stand-in d3d9.dll hashes");
    PutS("dgVoodoo.conf", body[1]);  // the user's: 16-bit
    for (i = 0; i < 4; ++i) {
        char rel[96];
        sprintf(rel, "dgVoodoo_variants\\%s.conf", kName[i]);
        Md5Of(rel, v0[i]);
    }

    RdLoad(g_game, pin, &info);
    Check(info.m.ok && info.d3d9Ok && info.haveConf && info.s.st == RD_ST_OFFERED && info.s.cur == 0 && info.s.alias == 1 && !info.haveNew &&
              strstr(info.why, "no longer offered") && strstr(info.why, "read as Direct3D 11"),
          "load: a 16-bit conf reads as Direct3D 11 (%s)", info.why);  // v3.44 FN344
    for (i = 0; i < 4; ++i)
        if (i == 1) Check(!info.srcOk[i], "load: the retired %s's file is not checked (no choice)", kName[i]);  // v3.44 FN344
        else Check(info.srcOk[i], "load: %s's file checked", kName[i]);
    RdLoad(g_game, RD_D3D9_PIN, &info);
    Check(info.s.st == RD_ST_NOD3D9 && info.s.greyed, "load with the real pin (the stand-in is no dgVoodoo2): greyed (%s)", info.why);
    {   // v3.39 REV339: the manifest's #d3d9.dll= lines decide over the built-in pin
        char man2[2200];
        sprintf(man2, "#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2\r\n#d3d9.dll=%s\r\n%s", pin, man);
        PutS("dgVoodoo_variants\\manifest.csv", man2);
        RdLoad(g_game, RD_D3D9_PIN, &info);
        Check(info.m.nPin == 2 && info.d3d9Ok && info.s.st == RD_ST_OFFERED, "a #d3d9.dll= line allows the stand-in (%s)", info.why);
        r = RdApply(g_game, RD_D3D9_PIN, "d3d11_flipdiscard", msg, sizeof msg);
        Check(r == 1 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[0]), "apply through the pin line (%s)", msg);
        PutS("dgVoodoo.conf", body[1]);
        sprintf(man2, "#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2\r\n%s", man);
        PutS("dgVoodoo_variants\\manifest.csv", man2);
        RdLoad(g_game, pin, &info);
        Check(info.m.nPin == 1 && !info.d3d9Ok && info.s.st == RD_ST_NOD3D9, "a pin line without the stand-in: greyed, the fallback unused (%s)", info.why);
        r = RdApply(g_game, pin, "d3d12", msg, sizeof msg);
        Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]), "refused by the pin line (%s)", msg);
        PutS("dgVoodoo_variants\\manifest.csv", man);
    }

    r = RdApply(g_game, pin, "d3d12", msg, sizeof msg);
    Check(r == 1 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[2]) && !Exists("dgVoodoo.conf.new"),
          "apply Direct3D 12: replaced, no .new left (%d: %s)", r, msg);
    Check(strstr(msg, md5[1]) && strstr(msg, md5[2]) && strstr(msg, "d3d11fd_rt16") && strstr(msg, "d3d12") && strstr(msg, "->"),
          "the message names old -> new with md5s (%s)", msg);
    r = RdApply(g_game, pin, "d3d12", msg, sizeof msg);
    Check(r == 2 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[2]), "apply it again: nothing written (%d: %s)", r, msg);
    r = RdApply(g_game, RD_D3D9_PIN, "passthru", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[2]), "another d3d9.dll: refused, untouched (%s)", msg);
    r = RdApply(g_game, pin, "d3d12_hdr", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[2]), "a recognised-only row is no target (%s)", msg);
    r = RdApply(g_game, pin, "nothing", msg, sizeof msg);
    Check(r == 0, "an unknown name is refused (%s)", msg);

    PutS("dgVoodoo.conf", body[4]);  // a recognised-only conf (the user tried d3d12_hdr with Graphics Plus)
    RdLoad(g_game, pin, &info);
    Check(info.s.st == RD_ST_KNOWN && info.s.cur == 3 && info.s.nVal == 4, "load: recognised only (%s)", info.why);
    r = RdApply(g_game, pin, "passthru", msg, sizeof msg);
    Check(r == 1 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[3]), "a recognised-only conf may be replaced (%s)", msg);

    PutS("dgVoodoo.conf", "[General]\r\n; the user's own conf\r\n");
    RdLoad(g_game, pin, &info);
    Check(info.s.st == RD_ST_CUSTOM && info.s.greyed, "load: Custom (%s)", info.why);
    r = RdApply(g_game, pin, "d3d11_flipdiscard", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && strcmp(now, md5[0]) && !Exists("dgVoodoo.conf.new"), "Custom: refused, untouched (%s)", msg);

    RdPath(g_game, "dgVoodoo.conf", p, sizeof p);
    DeleteFileA(p);
    r = RdApply(g_game, pin, "d3d11_flipdiscard", msg, sizeof msg);
    Check(r == 0 && !Exists("dgVoodoo.conf") && !Exists("dgVoodoo.conf.new"), "no dgVoodoo.conf: refused, never created (%s)", msg);

    PutS("dgVoodoo.conf", body[1]);
    PutS("dgVoodoo_variants\\d3d12.conf", "[General]\r\n; changed by hand\r\n");  // a variant that is not its md5 any more
    RdLoad(g_game, pin, &info);
    Check(info.s.st == RD_ST_OFFERED && info.s.nChoice == 2 && !info.srcOk[2], "load: a changed variant is no choice");
    r = RdApply(g_game, pin, "d3d12", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]) && !Exists("dgVoodoo.conf.new"),
          "apply a changed variant: refused by its md5, untouched (%s)", msg);
    PutS("dgVoodoo_variants\\d3d12.conf", body[2]);

    RdPath(g_game, "dgVoodoo.conf", p, sizeof p);  // a read-only conf: the move fails, nothing is left half done
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    r = RdApply(g_game, pin, "d3d11_flipdiscard", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]) && !Exists("dgVoodoo.conf.new"),
          "a read-only conf: not replaced, the .new deleted (%s)", msg);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);

    PutS("dgVoodoo.conf.new", "left over from a cut-short replace");
    RdLoad(g_game, pin, &info);
    Check(info.haveNew, "load sees a left-over .new");
    {   // v3.39 REV339: the start deletes it; inspection 3 / Graphics Plus: a lone .new is promoted only when verified
        DWORD err;
        RdInfo bad;
        Check(RdDropStaleNew(g_game, &info, &err) == 1 && !Exists("dgVoodoo.conf.new") && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]),
              "a stale .new is deleted, dgVoodoo.conf untouched");
        Check(RdDropStaleNew(g_game, &info, &err) == 0, "no .new: nothing to delete");
        RdPath(g_game, "dgVoodoo.conf", p, sizeof p);
        DeleteFileA(p);  // dgVoodoo.conf gone, a .new with an offered row's bytes: the cut-short switch is finished
        PutS("dgVoodoo.conf.new", body[2]);
        Check(info.d3d9Ok && RdDropStaleNew(g_game, &info, &err) == 2 && !Exists("dgVoodoo.conf.new") && Md5Of("dgVoodoo.conf", now) &&
              !strcmp(now, md5[2]), "a lone verified .new becomes dgVoodoo.conf");
        DeleteFileA(p);  // an unverified .new (not a manifest file): deleted, no conf created
        PutS("dgVoodoo.conf.new", "half written");
        Check(RdDropStaleNew(g_game, &info, &err) == 3 && !Exists("dgVoodoo.conf.new") && !Exists("dgVoodoo.conf"),
              "a lone unverified .new is deleted, no dgVoodoo.conf made");
        bad = info, bad.d3d9Ok = 0;  // an offered file but d3d9.dll not dgVoodoo (Graphics Plus removed): deleted, no conf
        PutS("dgVoodoo.conf.new", body[0]);
        Check(RdDropStaleNew(g_game, &bad, &err) == 3 && !Exists("dgVoodoo.conf.new") && !Exists("dgVoodoo.conf"),
              "a lone .new without an allowed d3d9.dll is deleted, no dgVoodoo.conf made");
        PutS("dgVoodoo.conf", body[1]);  // (back to the state the next checks expect)
    }
    PutS("dgVoodoo.conf.new", "left over from a cut-short replace");
    r = RdApply(g_game, pin, "d3d11_flipdiscard", msg, sizeof msg);
    Check(r == 1 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[0]) && !Exists("dgVoodoo.conf.new"),
          "a left-over .new is written over (%s)", msg);

    RdPath(g_game, "dgVoodoo_variants\\manifest.csv", p, sizeof p);
    DeleteFileA(p);
    RdLoad(g_game, pin, &info);
    Check(info.s.st == RD_ST_NOMANIFEST && info.s.greyed && strstr(info.why, "missing"), "no manifest: greyed (%s)", info.why);
    r = RdApply(g_game, pin, "d3d12", msg, sizeof msg);
    Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[0]), "no manifest: refused (%s)", msg);
    PutS("dgVoodoo_variants\\manifest.csv", man);

    {   // v3.44 FN344: RdBootRetire at start: a 16-bit conf is replaced with Direct3D 11 through RdApply's rules
        Check(GetModuleHandleA("d3d9.dll") == NULL, "(the test process has no d3d9.dll loaded)");
        PutS("dgVoodoo.conf", body[1]);
        RdLoad(g_game, pin, &info);
        r = RdBootRetire(g_game, pin, &info);
        Check(r == 2 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[0]) && !Exists("dgVoodoo.conf.new") && info.s.alias < 0 &&
                  info.s.cur == 0 && info.s.st == RD_ST_OFFERED, "v3.44: a 16-bit conf at start becomes Direct3D 11 (%d: %s)", r, info.why);
        r = RdBootRetire(g_game, pin, &info);
        Check(r == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[0]), "v3.44: then nothing to do (%d)", r);
        PutS("dgVoodoo.conf", body[3]);
        RdLoad(g_game, pin, &info);
        Check(RdBootRetire(g_game, pin, &info) == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[3]), "v3.44: Stock is left alone");
        PutS("dgVoodoo.conf", body[1]);
        RdLoad(g_game, RD_D3D9_PIN, &info);  // another d3d9.dll: nothing written
        Check(RdBootRetire(g_game, RD_D3D9_PIN, &info) == 0 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]),
              "v3.44: without an allowed d3d9.dll nothing is replaced");
        RdLoad(g_game, pin, &info);
        RdPath(g_game, "dgVoodoo.conf", p, sizeof p);
        SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
        r = RdBootRetire(g_game, pin, &info);
        Check(r == -1 && Md5Of("dgVoodoo.conf", now) && !strcmp(now, md5[1]) && !Exists("dgVoodoo.conf.new") && info.s.alias == 1,
              "v3.44: a read-only 16-bit conf: refused, untouched, still read as Direct3D 11 (%d)", r);
        SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
        PutS("dgVoodoo_variants\\d3d11_flipdiscard.conf", "[General]\r\n; changed by hand\r\n");
        RdLoad(g_game, pin, &info);
        Check(info.s.st == RD_ST_KNOWN && info.s.alias < 0 && RdBootRetire(g_game, pin, &info) == 0 && Md5Of("dgVoodoo.conf", now) &&
                  !strcmp(now, md5[1]), "v3.44: without a good Direct3D 11 file the 16-bit conf stays (recognised only)");
        PutS("dgVoodoo_variants\\d3d11_flipdiscard.conf", body[0]);
        PutS("dgVoodoo.conf", body[0]);
    }
    for (i = 0; i < 4; ++i) {  // dgVoodoo_variants: read only, all through
        char rel[96], m2[33];
        sprintf(rel, "dgVoodoo_variants\\%s.conf", kName[i]);
        Check(Md5Of(rel, m2) && !strcmp(m2, v0[i]), "%s untouched", rel);
    }
    Check(CountVariants() == 5, "dgVoodoo_variants holds its 5 files, nothing new (%d)", CountVariants());
    Check(Md5Of("d3d9.dll", now) && !strcmp(now, pin), "d3d9.dll untouched");
    Check(g_anoms == 0, "no ANOMALY line (%d)", g_anoms);
}

// The real game folder, read only: the variant files' md5s against its manifest (the md5 code on Graphics Plus's files)
static void TestGameFolder(const char *dir)
{
    RdInfo info;
    RdLoad(dir, RD_D3D9_PIN, &info);
    if (!info.m.ok) {
        printf("      (the game folder's manifest: %s; skipped)\n", info.why);
        return;
    }
    {
        int i, n = 0;
        for (i = 0; i < info.m.n; ++i)
            if (info.m.row[i].offered) {
                Check(info.srcOk[i], "game folder: %s's file has its manifest md5 %s", info.m.row[i].name, info.m.row[i].md5);
                ++n;
            }
        Check(info.d3d9Ok, "game folder: its d3d9.dll is allowed (%d #d3d9.dll= pin(s))", info.m.nPin);  // v3.39 REV339
        printf("      game folder: %d variant file(s) checked; %d pin line(s); d3d9.dll %s (%s); dgVoodoo.conf: %s\n", n, info.m.nPin,
               info.d3d9, info.d3d9Ok ? "allowed" : "not allowed", info.why);
    }
}

static void TestOpening(void)
{
    static const struct { const char *s; int lv; } kT[] = {
        { "Full", OP_FULL }, { "full", OP_FULL }, { "Stock", OP_FULL }, { "0", OP_FULL }, { "Off", OP_FULL },
        { "Skip Logos", OP_SKIP }, { "skip logos", OP_SKIP }, { "  Skip Logos  ", OP_SKIP }, { "SkipLogos", OP_SKIP }, { "1", OP_SKIP },
        { "Fast", OP_FAST }, { "FAST", OP_FAST }, { "2", OP_FAST }, { "Straight to Menu", OP_FAST },
        { "", -1 }, { "3", -1 }, { "Skip  Logos", -1 }, { "Faster", -1 }, { "a value that is far too long to be any level at all", -1 },
    };
    int i;
    for (i = 0; i < (int)(sizeof kT / sizeof kT[0]); ++i)
        Check(OpParse(kT[i].s) == kT[i].lv, "Opening \"%s\" -> %d (want %d)", kT[i].s, OpParse(kT[i].s), kT[i].lv);
    Check(OpParse(NULL) == -1, "Opening NULL");
    for (i = 0; i < OP_N; ++i) Check(OpParse(kOpName[i]) == i, "Opening's own name %s reads back", kOpName[i]);
    Check(OpSites(OP_FULL) == 0 && OpSites(OP_SKIP) == 1 && OpSites(OP_FAST) == 7, "the sites: Full none, Skip the call, Fast all three");
    Check(OP_DEFAULT == OP_SKIP && !strcmp(kOpName[OP_DEFAULT], "Skip Logos"), "the default is Skip Logos");
}

int main(int argc, char **argv)
{
    TestMd5();
    TestManifest();
    TestPins();  // v3.39 REV339
    TestState();
    TestFiles();
    TestOpening();
    if (argc > 1) TestGameFolder(argv[1]);
    printf("rend_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
