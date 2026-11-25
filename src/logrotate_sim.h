// v3.40 LG340: the session-log rotation's pure parts (no Windows calls; test\logrotate_test.c, logrotate.inc does the files).
// SH4 Refined keeps the last [Log] Keep sessions' logs (hidden key, default 8; 0 or 1 = off: only SH4Refined.log and
// SH4Refined.prev.log, as up to v3.39) in the game folder's logs\ subfolder, one file a session, named after its start
// (local time) and version: "SH4Refined 2026-11-22 0724 v3.40.0.log" (" #2" .. " #9" before ".log" when two sessions
// start in the same minute). The names sort by time, so a file list or logcheck.py shows them in order.
// SH4Refined.log (this session) and SH4Refined.prev.log (the one before) stay where they were for the tools and the user.
// Only names of exactly this form are Refined's own (LrParseName); nothing else in logs\ is ever counted or deleted.
#ifndef LOGROTATE_SIM_H
#define LOGROTATE_SIM_H
#include <string.h>
#include <stdio.h>

#define LR_DIR "logs"
#define LR_KEEP_DEFAULT 8
#define LR_KEEP_MAX 99
#define LR_NAME_MAX 96         // a name LrMakeName writes is at most 11 + 16 + 1 + LR_VER_MAX + 3 + 4 + 1 = 60 bytes
#define LR_VER_MAX 24
#define LR_HEAD_BYTES 8192     // how much of a log's start is read for its start line and version
#define LR_DUP_MAX 9

typedef struct { int y, mo, d, h, mi, s; } LrTime;

// [Log] Keep as read from the ini: 0 = off (0, 1 and anything below), else 2 .. LR_KEEP_MAX.
static int LrKeep(int ini)
{
    if (ini <= 1) return 0;
    return ini > LR_KEEP_MAX ? LR_KEEP_MAX : ini;
}

static int LrDaysIn(int y, int mo)
{
    static const int k[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return k[(mo - 1) % 12];
}

static int LrTimeOk(const LrTime *t)
{
    return t->y >= 1980 && t->y <= 9999 && t->mo >= 1 && t->mo <= 12 && t->d >= 1 && t->d <= LrDaysIn(t->y, t->mo) &&
           t->h >= 0 && t->h <= 23 && t->mi >= 0 && t->mi <= 59 && t->s >= 0 && t->s <= 59;
}

static void LrPrevDay(LrTime *t)
{
    if (--t->d >= 1) return;
    if (--t->mo < 1) { t->mo = 12; --t->y; }
    t->d = LrDaysIn(t->y, t->mo);
}

// The version as a file-name part: letters, digits and . - _ + kept, anything else '_', at most LR_VER_MAX characters.
static void LrSafeVer(const char *ver, char *out, size_t n)
{
    size_t i = 0;
    if (!n) return;
    for (; ver && *ver && i + 1 < n && i < LR_VER_MAX; ++ver) {
        const char c = *ver;
        out[i++] = ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' ||
                    c == '_' || c == '+') ? c : '_';
    }
    out[i] = 0;
}

// "SH4Refined YYYY-MM-DD HHMM v<ver>[ #dup].log" ("unknown" for an empty version); dup 1 = no suffix. 0 when it cannot.
static int LrMakeName(char *out, size_t n, const LrTime *t, const char *ver, int dup)
{
    char v[LR_VER_MAX + 1], d[8] = "";
    int r;
    if (!out || n < 8 || !LrTimeOk(t) || dup < 1 || dup > LR_DUP_MAX) return 0;
    LrSafeVer(ver, v, sizeof v);
    if (dup > 1) sprintf_s(d, sizeof d, " #%d", dup);
    r = sprintf_s(out, n, "SH4Refined %04d-%02d-%02d %02d%02d %s%s%s.log", t->y, t->mo, t->d, t->h, t->mi, v[0] ? "v" : "unknown",
                  v, d);
    return r > 0;
}

static int LrDigits(const char *p, int n, int *v)
{
    int i;
    *v = 0;
    for (i = 0; i < n; ++i) {
        if (p[i] < '0' || p[i] > '9') return 0;
        *v = *v * 10 + (p[i] - '0');
    }
    return 1;
}

// 1 when name is one of Refined's rotated logs (exactly LrMakeName's form; ".log" in any case); *key sorts them by start
// (YYYYMMDDHHMM * 10 + dup). Anything else (another program's file, a copy the user renamed) is 0: never touched.
static int LrParseName(const char *name, unsigned long long *key)
{
    LrTime t = { 0 };
    const char *p, *e;
    size_t len, tag = 0;
    int dup = 1;
    if (!name || strncmp(name, "SH4Refined ", 11)) return 0;
    len = strlen(name);
    if (len < 11 + 16 + 1 + 4 || len >= LR_NAME_MAX) return 0;
    p = name + 11;  // "YYYY-MM-DD HHMM "
    if (!LrDigits(p, 4, &t.y) || p[4] != '-' || !LrDigits(p + 5, 2, &t.mo) || p[7] != '-' || !LrDigits(p + 8, 2, &t.d) ||
        p[10] != ' ' || !LrDigits(p + 11, 2, &t.h) || !LrDigits(p + 13, 2, &t.mi) || p[15] != ' ' || !LrTimeOk(&t))
        return 0;
    p += 16;
    e = name + len - 4;
    if (_stricmp(e, ".log")) return 0;
    if (e - p >= 3 && e[-3] == ' ' && e[-2] == '#' && e[-1] >= '2' && e[-1] <= '9') {
        dup = e[-1] - '0';
        e -= 3;
    }
    if (e <= p) return 0;
    if (!(e - p == 7 && !strncmp(p, "unknown", 7))) {  // else "v" + the safe version
        if (*p != 'v') return 0;
        for (++p; p + tag < e; ++tag) {
            const char c = p[tag];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_' ||
                  c == '+'))
                return 0;
        }
        if (!tag || tag > LR_VER_MAX) return 0;
    }
    if (key)
        *key = ((((unsigned long long)t.y * 100 + t.mo) * 100 + t.d) * 100 + t.h) * 100 * 10 + (unsigned long long)t.mi * 10 + dup;
    return 1;
}

// a is newer than b (by start, then by name, so the order is total)
static int LrNewer(const char *a, const char *b)
{
    unsigned long long ka = 0, kb = 0;
    LrParseName(a, &ka);
    LrParseName(b, &kb);
    if (ka != kb) return ka > kb;
    return strcmp(a, b) > 0;
}

// The prune plan. names[0..n): Refined's own rotated logs that may be deleted (the caller leaves out read-only files:
// a log the user marked read-only is kept for good and not counted). Marks del[i] = 1 for each one but the newest keep;
// keep < 1 marks nothing (rotation off: logs\ is left as it is). Returns how many are marked.
static int LrPlanPrune(const char *const *names, int n, int keep, unsigned char *del)
{
    int i, j, m = 0;
    for (i = 0; i < n; ++i) {
        int newer = 0;
        del[i] = 0;
        if (keep < 1 || !LrParseName(names[i], NULL)) continue;
        for (j = 0; j < n; ++j)
            if (j != i && LrParseName(names[j], NULL) && LrNewer(names[j], names[i])) ++newer;
        if (newer >= keep) { del[i] = 1; ++m; }
    }
    return m;
}

// 1 when a new rotated log named cand would be among the newest keep next to names[0..n) (else copying it is pointless:
// the prune would delete it at once, e.g. after Keep was lowered).
static int LrWouldKeep(const char *const *names, int n, const char *cand, int keep)
{
    int j, newer = 0;
    if (keep < 1 || !LrParseName(cand, NULL)) return 0;
    for (j = 0; j < n; ++j)
        if (LrParseName(names[j], NULL) && strcmp(names[j], cand) && LrNewer(names[j], cand)) ++newer;
    return newer < keep;
}

static int LrHms(const char *p, LrTime *t)  // "HH:MM:SS"
{
    return LrDigits(p, 2, &t->h) && p[2] == ':' && LrDigits(p + 3, 2, &t->mi) && p[5] == ':' && LrDigits(p + 6, 2, &t->s) &&
           t->h <= 23 && t->mi <= 59 && t->s <= 59;
}

// v3.40 REV340 (LG340-002): 1 when text (the first bytes of a file, NUL-terminated; LR_SIG_BYTES are enough) starts with
// the first line every SH4 Refined log has (sh4windowmod.c Init): "HH:MM:SS.mmm SH4 Refined v<ver> loading (winmm.dll
// proxy". Only such a file is archived, counted or deleted; a file merely named like a rotated log is left alone.
#define LR_SIG_BYTES 128
static int LrIsOwnLog(const char *text)
{
    static const char kTail[] = " loading (winmm.dll proxy";
    LrTime t;
    const char *p;
    size_t n = 0;
    int ms;
    if (!text || !LrHms(text, &t) || text[8] != '.' || !LrDigits(text + 9, 3, &ms) || strncmp(text + 12, " SH4 Refined v", 14))
        return 0;
    for (p = text + 26; p[n] && p[n] != ' ' && p[n] != '\r' && p[n] != '\n'; ++n) {
        const char c = p[n];
        if (n >= LR_VER_MAX || c < '!' || c > '~') return 0;
    }
    return n > 0 && !strncmp(p + n, kTail, sizeof kTail - 1);
}

// A log's start and version from its first bytes (text: NUL-terminated). The version: the first line's
// "SH4 Refined v<ver> loading" (or v2.1's "SH4 Window Mod v<ver>"); "" when not found. The start:
//   2: v3.40's "log: started YYYY-MM-DD HH:MM:SS" line (written right after the first line);
//   1: an older log: the first line's HH:MM:SS with the date of last (the file's last write, local time), a day earlier
//      when the start's time of day is after the last write's (a session that went past midnight; one over 24 h is
//      dated a day late);
//   0: no time found (an empty or foreign first line): *start = last.
static int LrHeader(const char *text, const LrTime *last, LrTime *start, char *ver, size_t vn)
{
    const char *p, *nl;
    LrTime t;
    if (ver && vn) ver[0] = 0;
    *start = *last;
    if (!text) return 0;
    nl = strchr(text, '\n');
    {   // the version, from the first line only
        const char *v = strstr(text, "SH4 Refined v");
        size_t skip = 13;
        if (!v || (nl && v > nl)) { v = strstr(text, "SH4 Window Mod v"); skip = 16; }
        if (v && (!nl || v < nl) && ver && vn) {
            size_t i = 0;
            v += skip;
            while (v[i] && v[i] != ' ' && v[i] != '\r' && v[i] != '\n' && i + 1 < vn) { ver[i] = v[i]; ++i; }
            ver[i] = 0;
        }
    }
    for (p = strstr(text, "log: started "); p; p = strstr(p + 1, "log: started ")) {
        const char *q = p + 13;
        t = *last;
        if (LrDigits(q, 4, &t.y) && q[4] == '-' && LrDigits(q + 5, 2, &t.mo) && q[7] == '-' && LrDigits(q + 8, 2, &t.d) &&
            q[10] == ' ' && LrHms(q + 11, &t) && LrTimeOk(&t)) {
            *start = t;
            return 2;
        }
    }
    t = *last;
    if (LrHms(text, &t) && text[8] == '.') {
        if (t.h * 3600 + t.mi * 60 + t.s > last->h * 3600 + last->mi * 60 + last->s) LrPrevDay(&t);
        if (LrTimeOk(&t)) {
            *start = t;
            return 1;
        }
    }
    return 0;
}
#endif
