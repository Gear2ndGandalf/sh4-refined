// v3.39 PR339: the Opening setting's pure part (test\rend_test.c): [Window] Opening's values and which exe sites each
// level patches (presentation.inc installs them at start; RESEARCH.md A).
#ifndef OPENING_SIM_H
#define OPENING_SIM_H
#include <string.h>

enum { OP_FULL, OP_SKIP, OP_FAST, OP_N };
static const char *const kOpName[OP_N] = { "Full", "Skip Logos", "Fast" };
#define OP_DEFAULT OP_SKIP

static int OpIeq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        const int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (x != y) return 0;
    }
    return *a == *b;
}
// The level a value names: the row's names, their short forms, the numbers 0 / 1 / 2 and the research's names
// ("Stock", "Straight to Menu"); leading / trailing spaces ignored. -1 = not a level (the caller keeps the default).
static int OpParse(const char *v)
{
    static const struct { const char *s; int lv; } kAlias[] = {
        { "Full", OP_FULL }, { "Stock", OP_FULL }, { "Off", OP_FULL }, { "0", OP_FULL },
        { "Skip Logos", OP_SKIP }, { "SkipLogos", OP_SKIP }, { "Skip", OP_SKIP }, { "On", OP_SKIP }, { "1", OP_SKIP },
        { "Fast", OP_FAST }, { "Straight to Menu", OP_FAST }, { "2", OP_FAST },
    };
    char b[40];
    size_t n, k;
    if (!v) return -1;
    while (*v == ' ' || *v == '\t') ++v;
    n = strlen(v);
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r' || v[n - 1] == '\n')) --n;
    if (!n || n >= sizeof b) return -1;
    memcpy(b, v, n);
    b[n] = 0;
    for (k = 0; k < sizeof kAlias / sizeof kAlias[0]; ++k)
        if (OpIeq(b, kAlias[k].s)) return kAlias[k].lv;
    return -1;
}
// Which patches a level needs: bit 0 = the boot-screen call 0x42CD6A, bit 1 = the first-start movie branch 0x42CDA2,
// bit 2 = the short title intro 0x42CDAB
static unsigned OpSites(int lv) { return lv == OP_FAST ? 7u : lv == OP_SKIP ? 1u : 0u; }
#endif
