// v3.39 PR339: the Renderer row's pure part (test\rend_test.c): MD5, SH4 Graphics Plus's manifest, the row's state and
// its choice list. No file or Windows call here: renderer.inc reads the files and hands their bytes / md5s in.
// RENDERER_SPEC.md (binding): dgVoodoo_variants\manifest.csv is ASCII, CRLF, no quotes, '#' lines are comments, the
// header is `name,file,md5,label`; a row with a file is offered (its label is the value's text), a row with an empty
// file is recognised only ("Graphics Plus: <name>"). The md5s come from the manifest only (never written in here).
// dgVoodoo.conf may be replaced only when its md5 is ANY manifest row's; an unknown md5 or a missing conf is Custom
// (greyed); a missing / broken manifest or a d3d9.dll that is not dgVoodoo2 2.87.6 greys the row too.
#ifndef RENDERER_SIM_H
#define RENDERER_SIM_H
#include <string.h>
#include <stddef.h>

// ---------------------------------------------------------------- MD5 (RFC 1321), written for this file
typedef struct {
    unsigned int s[4];
    unsigned int lo, hi;  // the message length in bytes (64 bits)
    unsigned char buf[64];
} RdMd5;

static const unsigned int kRdMd5K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
static const unsigned char kRdMd5R[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static void RdMd5Block(RdMd5 *c, const unsigned char *p)
{
    unsigned int w[16], a = c->s[0], b = c->s[1], cc = c->s[2], d = c->s[3];
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = (unsigned int)p[i * 4] | (unsigned int)p[i * 4 + 1] << 8 | (unsigned int)p[i * 4 + 2] << 16 |
               (unsigned int)p[i * 4 + 3] << 24;
    for (i = 0; i < 64; ++i) {
        unsigned int f, t;
        int g;
        if (i < 16) f = (b & cc) | (~b & d), g = i;
        else if (i < 32) f = (d & b) | (~d & cc), g = (5 * i + 1) & 15;
        else if (i < 48) f = b ^ cc ^ d, g = (3 * i + 5) & 15;
        else f = cc ^ (b | ~d), g = (7 * i) & 15;
        t = d;
        d = cc;
        cc = b;
        f += a + kRdMd5K[i] + w[g];
        b += (f << kRdMd5R[i]) | (f >> (32 - kRdMd5R[i]));
        a = t;
    }
    c->s[0] += a, c->s[1] += b, c->s[2] += cc, c->s[3] += d;
}
static void RdMd5Init(RdMd5 *c)
{
    c->s[0] = 0x67452301, c->s[1] = 0xefcdab89, c->s[2] = 0x98badcfe, c->s[3] = 0x10325476;
    c->lo = c->hi = 0;
}
static void RdMd5Update(RdMd5 *c, const void *data, size_t n)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t used = c->lo & 63;
    const unsigned int lo = c->lo;
    c->lo += (unsigned int)n;
    if (c->lo < lo) ++c->hi;
    c->hi += (unsigned int)((unsigned long long)n >> 32);
    if (used) {
        const size_t take = 64 - used < n ? 64 - used : n;
        memcpy(c->buf + used, p, take);
        p += take, n -= take, used += take;
        if (used < 64) return;
        RdMd5Block(c, c->buf);
    }
    for (; n >= 64; p += 64, n -= 64) RdMd5Block(c, p);
    if (n) memcpy(c->buf, p, n);
}
// The digest as 32 upper-case hex digits (out holds 33)
static void RdMd5Final(RdMd5 *c, char out[33])
{
    static const char kHex[] = "0123456789ABCDEF";
    unsigned char pad[72], len[8];
    const unsigned int bitsLo = c->lo << 3, bitsHi = c->hi << 3 | c->lo >> 29;
    size_t padN = 64 - ((c->lo + 8) & 63);
    int i;
    if (padN == 0) padN = 64;  // (never: lo + 8 + padN is a multiple of 64 with padN in 1..64)
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (i = 0; i < 4; ++i) len[i] = (unsigned char)(bitsLo >> (8 * i)), len[4 + i] = (unsigned char)(bitsHi >> (8 * i));
    RdMd5Update(c, pad, padN);
    RdMd5Update(c, len, 8);
    for (i = 0; i < 16; ++i) {
        const unsigned char b = (unsigned char)(c->s[i >> 2] >> (8 * (i & 3)));
        out[i * 2] = kHex[b >> 4];
        out[i * 2 + 1] = kHex[b & 15];
    }
    out[32] = 0;
}
static void RdMd5Of(const void *data, size_t n, char out[33])
{
    RdMd5 c;
    RdMd5Init(&c);
    RdMd5Update(&c, data, n);
    RdMd5Final(&c, out);
}

// ---------------------------------------------------------------- the manifest
#define RD_ROWS_MAX 16      // data rows at most (Graphics Plus ships 8)
#define RD_CHOICE_MAX 6     // offered rows the row shows at most (4 shipped); more are recognised only
#define RD_VALS (RD_CHOICE_MAX + 1)  // + the state value (Graphics Plus: <name> / Custom / Unavailable)
#define RD_NAME_MAX 31
#define RD_FILE_MAX 63
#define RD_LABEL_MAX 31
#define RD_MANIFEST_MAX 16384  // bytes; a bigger file is not Graphics Plus's
#define RD_PIN_MAX 8           // v3.39 REV339: `#d3d9.dll=<md5>` lines at most
#define RD_PIN_PREFIX "#d3d9.dll="
typedef struct {
    char name[RD_NAME_MAX + 1], file[RD_FILE_MAX + 1], md5[33], label[RD_LABEL_MAX + 1];
    int offered;            // a file is named: the row can be chosen
} RdRow;
typedef struct {
    int ok;                 // parsed, a header, every row well formed, at least one offered row
    int n, nOffered;
    RdRow row[RD_ROWS_MAX];
    int line;               // the line of the first problem (1-based; 0 = none / the whole file)
    int nPin;               // v3.39 REV339: the allowed d3d9.dll md5s, from `#d3d9.dll=<32 hex>` lines before the header
    char pin[RD_PIN_MAX][33];
    char err[96];           // what it was
} RdManifest;

static int RdIsHex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
static int RdIsNameChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}
static int RdLower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int RdIeq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (RdLower((unsigned char)*a) != RdLower((unsigned char)*b)) return 0;
    return *a == *b;
}
static void RdErr(RdManifest *m, int line, const char *what)
{
    size_t k;
    m->ok = 0;
    m->line = line;
    for (k = 0; what[k] && k + 1 < sizeof m->err; ++k) m->err[k] = what[k];
    m->err[k] = 0;
}
// Copies the field [s, e) trimmed of spaces / tabs into out (cap bytes with the 0); -1 when it does not fit
static int RdField(const char *s, const char *e, char *out, size_t cap)
{
    size_t n;
    while (s < e && (*s == ' ' || *s == '\t')) ++s;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
    n = (size_t)(e - s);
    if (n + 1 > cap) return -1;
    memcpy(out, s, n);
    out[n] = 0;
    return (int)n;
}
static int RdNameOk(const char *s, int needed)
{
    if (!*s) return !needed;
    if (*s == '.') return 0;  // no "..", no hidden names
    for (; *s; ++s)
        if (!RdIsNameChar(*s)) return 0;
    return 1;
}
// Parses the manifest's bytes. Any malformed line (bad header, field count, name, file, md5, a non-ASCII byte, a
// duplicate name or md5, too many rows) makes the whole manifest broken: the row is greyed (Graphics Plus's installer
// writes this file; a damaged one is not guessed at).
static int RdParseManifest(const char *buf, size_t len, RdManifest *m)
{
    const char *p = buf, *end = buf + len;
    int line = 0, header = 0;
    memset(m, 0, sizeof *m);
    m->ok = 1;
    if (!buf || len == 0) {
        RdErr(m, 0, "empty");
        return 0;
    }
    if (len > RD_MANIFEST_MAX) {
        RdErr(m, 0, "larger than 16 KB");
        return 0;
    }
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;  // a UTF-8 mark
    while (p < end) {
        const char *e = p, *le, *s, *f[5];
        int nf = 0, k;
        while (e < end && *e != '\n') ++e;
        le = e;
        if (le > p && le[-1] == '\r') --le;
        ++line;
        for (s = p; s < le; ++s)
            if ((unsigned char)*s < 0x20 && *s != '\t') { RdErr(m, line, "a control character"); return 0; }
            else if ((unsigned char)*s >= 0x7F) { RdErr(m, line, "a byte that is not ASCII"); return 0; }
        if (!header && (size_t)(le - p) >= sizeof RD_PIN_PREFIX - 1 && !memcmp(p, RD_PIN_PREFIX, sizeof RD_PIN_PREFIX - 1)) {
            // v3.39 REV339 (Graphics Plus): a directive `#d3d9.dll=<32 hex>` (exactly that prefix, at the line start, before the
            // header): a d3d9.dll md5 the confs are for. A malformed one breaks the manifest (no guessing); after the header
            // such a line is a plain comment.
            char v[40];
            int j;
            if (RdField(p + sizeof RD_PIN_PREFIX - 1, le, v, sizeof v) != 32) { RdErr(m, line, "a #d3d9.dll= line without a 32-digit md5"); return 0; }
            for (k = 0; k < 32; ++k) {
                if (!RdIsHex(v[k])) { RdErr(m, line, "a #d3d9.dll= line without a 32-digit md5"); return 0; }
                if (v[k] >= 'a') v[k] = (char)(v[k] - 32);
            }
            for (j = 0; j < m->nPin && strcmp(m->pin[j], v); ++j) {}
            if (j == m->nPin) {
                if (m->nPin >= RD_PIN_MAX) { RdErr(m, line, "more than 8 #d3d9.dll= lines"); return 0; }
                memcpy(m->pin[m->nPin++], v, 33);
            }
            p = e < end ? e + 1 : end;
            continue;
        }
        for (s = p; s < le && (*s == ' ' || *s == '\t'); ++s) {}
        if (s == le || *s == '#') {  // blank or a comment
            p = e < end ? e + 1 : end;
            continue;
        }
        f[nf++] = p;
        for (s = p; s < le; ++s)
            if (*s == ',') {
                if (nf >= 5) break;
                f[nf++] = s + 1;
            }
        if (nf != 4) { RdErr(m, line, header ? "a row without exactly 4 fields" : "a header without exactly 4 fields"); return 0; }
        f[4] = le + 1;  // (field k ends at f[k + 1] - 1)
        if (!header) {
            static const char *const kHead[4] = { "name", "file", "md5", "label" };
            char h[16];
            for (k = 0; k < 4; ++k)
                if (RdField(f[k], f[k + 1] - 1, h, sizeof h) < 0 || !RdIeq(h, kHead[k])) {
                    RdErr(m, line, "the header is not name,file,md5,label");
                    return 0;
                }
            header = 1;
        } else {
            RdRow *r;
            int j;
            if (m->n >= RD_ROWS_MAX) { RdErr(m, line, "more than 16 rows"); return 0; }
            r = &m->row[m->n];
            if (RdField(f[0], f[1] - 1, r->name, sizeof r->name) < 0 || !RdNameOk(r->name, 1)) { RdErr(m, line, "a bad name"); return 0; }
            if (RdField(f[1], f[2] - 1, r->file, sizeof r->file) < 0 || !RdNameOk(r->file, 0)) { RdErr(m, line, "a bad file name"); return 0; }
            if (RdField(f[2], f[3] - 1, r->md5, sizeof r->md5) != 32) { RdErr(m, line, "an md5 that is not 32 hex digits"); return 0; }
            for (k = 0; k < 32; ++k) {
                if (!RdIsHex(r->md5[k])) { RdErr(m, line, "an md5 that is not 32 hex digits"); return 0; }
                if (r->md5[k] >= 'a') r->md5[k] = (char)(r->md5[k] - 32);
            }
            {   // the label: printable already (the byte check above); a longer one is cut
                char lab[256];
                const int n = RdField(f[3], f[4] - 1, lab, sizeof lab);
                if (n < 0) { RdErr(m, line, "a label longer than 255 characters"); return 0; }
                lab[RD_LABEL_MAX] = 0;
                memcpy(r->label, lab, RD_LABEL_MAX + 1);
                {   // a cut label keeps no trailing space
                    size_t t = strlen(r->label);
                    while (t && r->label[t - 1] == ' ') r->label[--t] = 0;
                }
            }
            for (j = 0; j < m->n; ++j) {
                if (RdIeq(m->row[j].name, r->name)) { RdErr(m, line, "a name twice"); return 0; }
                if (!strcmp(m->row[j].md5, r->md5)) { RdErr(m, line, "an md5 twice"); return 0; }
            }
            r->offered = r->file[0] != 0;
            if (r->offered && !r->label[0]) {  // an offered row without a label shows its name
                memcpy(r->label, r->name, RD_LABEL_MAX + 1);
                r->label[RD_LABEL_MAX] = 0;
            }
            if (r->offered) ++m->nOffered;
            ++m->n;
        }
        p = e < end ? e + 1 : end;
    }
    if (!header) { RdErr(m, 0, "no header line"); return 0; }
    if (!m->nOffered) { RdErr(m, 0, "no row with a file"); return 0; }
    return 1;
}
// v3.39 REV339: a d3d9.dll md5 (upper case) is allowed: one of the manifest's `#d3d9.dll=` md5s, or `fallback` (the
// built-in dgVoodoo2 2.87.6 pin) when the manifest has none
static int RdD3d9Allowed(const RdManifest *m, const char *fallback, const char *md5)
{
    int i;
    if (!md5 || !md5[0]) return 0;
    if (m && m->ok && m->nPin) {
        for (i = 0; i < m->nPin; ++i)
            if (!strcmp(m->pin[i], md5)) return 1;
        return 0;
    }
    return fallback && !strcmp(fallback, md5);
}
// The row whose md5 is hex (any case), or -1
static int RdFindMd5(const RdManifest *m, const char *hex)
{
    int i, k;
    if (!m->ok || !hex) return -1;
    for (i = 0; i < m->n; ++i) {
        for (k = 0; k < 32 && hex[k] && (char)(hex[k] >= 'a' && hex[k] <= 'f' ? hex[k] - 32 : hex[k]) == m->row[i].md5[k]; ++k) {}
        if (k == 32 && !hex[32]) return i;
    }
    return -1;
}
static int RdFindName(const RdManifest *m, const char *name)
{
    int i;
    if (!m->ok || !name) return -1;
    for (i = 0; i < m->n; ++i)
        if (RdIeq(m->row[i].name, name)) return i;
    return -1;
}

// ---------------------------------------------------------------- v3.44 FN344: retired rows
// The user (v3.43 QA, USER_FEEDBACK_v343.md): "Remove the renderer value Direct3D 11 (16-bit)". The manifest is SH4
// Graphics Plus's (graphics\README.md) and keeps the row and its file; Refined no longer offers it: a retired row is never a
// choice, and a dgVoodoo.conf that is one reads as its replacement (renderer.inc RdBootRetire replaces it at start).
static const char *const kRdRetired[][2] = { { "d3d11fd_rt16", "d3d11_flipdiscard" } };
// The row name a retired row reads as, or NULL (not retired)
static const char *RdRetiredAs(const char *name)
{
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < sizeof kRdRetired / sizeof kRdRetired[0]; ++i)
        if (RdIeq(name, kRdRetired[i][0])) return kRdRetired[i][1];
    return NULL;
}

// ---------------------------------------------------------------- the row's state
enum {
    RD_ST_NOMANIFEST,  // no / a broken manifest: greyed, "Unavailable"
    RD_ST_NOD3D9,      // the game folder's d3d9.dll is missing or not dgVoodoo2 2.87.6: greyed, "Unavailable"
    RD_ST_NOSOURCE,    // no offered row's file is there with its md5: greyed, "Unavailable"
    RD_ST_CUSTOM,      // dgVoodoo.conf is missing or no manifest row's: greyed, "Custom", never replaced
    RD_ST_KNOWN,       // a manifest row that is not a choice (recognised only, or its file is gone): "Graphics Plus: <name>"
    RD_ST_OFFERED,     // one of the choices
    RD_ST_N
};
typedef struct {
    int st;
    int row;                    // the manifest row of dgVoodoo.conf (-1 none)
    int nChoice, choice[RD_CHOICE_MAX];  // the offered rows that can be chosen, in the manifest's order
    int nVal;                   // the row's values: the choices, then the state value when extra
    int extra;                  // 1: value nChoice is the state value (KNOWN / CUSTOM / the Unavailable ones)
    int cur;                    // the value dgVoodoo.conf is now
    int greyed;
    int alias;                  // v3.44 FN344: the retired manifest row dgVoodoo.conf is (it reads as value cur); -1 none
} RdState;
// srcOk[i]: row i's file is in dgVoodoo_variants with the row's md5 (NULL = all assumed there); conf = dgVoodoo.conf's
// md5, NULL when the file is missing
static void RdDecide(const RdManifest *m, int d3d9Ok, const char *conf, const int *srcOk, RdState *s)
{
    int i;
    memset(s, 0, sizeof *s);
    s->row = -1;
    s->alias = -1;  // v3.44 FN344
    if (!m || !m->ok) s->st = RD_ST_NOMANIFEST;
    else if (!d3d9Ok) s->st = RD_ST_NOD3D9;
    else {
        for (i = 0; i < m->n && s->nChoice < RD_CHOICE_MAX; ++i)
            if (m->row[i].offered && !RdRetiredAs(m->row[i].name) && (!srcOk || srcOk[i]))  // v3.44 FN344: + not retired
                s->choice[s->nChoice++] = i;
        s->row = RdFindMd5(m, conf);
        if (!s->nChoice) s->st = RD_ST_NOSOURCE;
        else if (s->row < 0) s->st = RD_ST_CUSTOM;
        else {
            const char *as = RdRetiredAs(m->row[s->row].name);  // v3.44 FN344
            s->st = RD_ST_KNOWN;
            for (i = 0; i < s->nChoice; ++i)
                if (s->choice[i] == s->row) {
                    s->st = RD_ST_OFFERED;
                    s->cur = i;
                }
            for (i = 0; as && s->st == RD_ST_KNOWN && i < s->nChoice; ++i)  // v3.44 FN344: a retired conf reads as its replacement
                if (RdIeq(m->row[s->choice[i]].name, as)) {
                    s->st = RD_ST_OFFERED;
                    s->cur = i;
                    s->alias = s->row;
                }
        }
    }
    if (s->st == RD_ST_OFFERED) {
        s->nVal = s->nChoice;
    } else if (s->st == RD_ST_KNOWN || s->st == RD_ST_CUSTOM) {
        s->extra = 1;
        s->cur = s->nChoice;
        s->nVal = s->nChoice + 1;
    } else {  // the Unavailable states show that one value only
        s->nChoice = 0;
        s->extra = 1;
        s->cur = 0;
        s->nVal = 1;
    }
    s->greyed = s->st != RD_ST_OFFERED && s->st != RD_ST_KNOWN;
}
// The value after v when the row steps by dir (+1 right / -1 left). A greyed row never moves. In the KNOWN state the
// state value is part of the ring (picking it again = keep dgVoodoo.conf as it is).
static int RdStep(const RdState *s, int v, int dir)
{
    const int n = s->nVal;
    if (s->greyed || n <= 1) return v;
    if (v < 0 || v >= n) v = s->cur;
    return dir > 0 ? (v + 1) % n : (v + n - 1) % n;
}
// The manifest row a value replaces dgVoodoo.conf with, or -1 (the state value, or the value dgVoodoo.conf is now)
static int RdTargetRow(const RdState *s, int v)
{
    if (s->greyed || v < 0 || v >= s->nChoice) return -1;
    return s->choice[v] == s->row ? -1 : s->choice[v];
}
// The text of value v (out holds at least 40)
static void RdValueText(const RdManifest *m, const RdState *s, int v, char *out, size_t n)
{
    const char *t = "Unavailable";
    if (!n) return;
    out[0] = 0;
    if (v >= 0 && v < s->nChoice && m && m->ok) t = m->row[s->choice[v]].label;
    else if (s->st == RD_ST_CUSTOM) t = "Custom";
    else if (s->st == RD_ST_KNOWN && m && s->row >= 0) {
        char nm[21];
        size_t k;
        for (k = 0; k < 20 && m->row[s->row].name[k]; ++k) nm[k] = m->row[s->row].name[k];
        nm[k] = 0;
        {
            const char *pre = "Graphics Plus: ";
            size_t a = strlen(pre), b = strlen(nm);
            if (a + b + 1 > n) b = n > a + 1 ? n - a - 1 : 0;
            if (a + 1 > n) a = n - 1;
            memcpy(out, pre, a);
            memcpy(out + a, nm, b);
            out[a + b] = 0;
        }
        return;
    }
    {
        size_t k;
        for (k = 0; t[k] && k + 1 < n; ++k) out[k] = t[k];
        out[k] = 0;
    }
}
// Which help text value v gets (renderer.inc / presentation.inc hold the texts; by Graphics Plus's row names)
enum { RD_DK_D3D11, RD_DK_RT16, RD_DK_D3D12, RD_DK_STOCK, RD_DK_OTHER, RD_DK_KNOWN, RD_DK_CUSTOM, RD_DK_UNAVAIL, RD_DK_N };
static int RdDescKind(const RdManifest *m, const RdState *s, int v)
{
    if (v >= 0 && v < s->nChoice && m && m->ok) {
        const char *nm = m->row[s->choice[v]].name;
        if (RdIeq(nm, "d3d11_flipdiscard")) return RD_DK_D3D11;
        if (RdIeq(nm, "d3d11fd_rt16")) return RD_DK_RT16;
        if (RdIeq(nm, "d3d12")) return RD_DK_D3D12;
        if (RdIeq(nm, "passthru")) return RD_DK_STOCK;
        return RD_DK_OTHER;
    }
    if (s->st == RD_ST_KNOWN) return RD_DK_KNOWN;
    if (s->st == RD_ST_CUSTOM) return RD_DK_CUSTOM;
    return RD_DK_UNAVAIL;
}
// The name of the variant a conf md5 is (the live one, for the Direct3D line): an offered row's label, a recognised-only
// row's "Graphics Plus <name>", NULL when none (out holds at least 48)
static const char *RdVariantName(const RdManifest *m, const char *conf, char *out, size_t n)
{
    const int r = RdFindMd5(m, conf);
    if (r < 0 || n < 2) return NULL;
    if (m->row[r].offered) {
        size_t k;
        for (k = 0; m->row[r].label[k] && k + 1 < n; ++k) out[k] = m->row[r].label[k];
        out[k] = 0;
    } else {
        const char *pre = "Graphics Plus ";
        size_t k = 0, j;
        for (j = 0; pre[j] && k + 1 < n; ++j) out[k++] = pre[j];
        for (j = 0; m->row[r].name[j] && k + 1 < n; ++j) out[k++] = m->row[r].name[j];
        out[k] = 0;
    }
    return out;
}
static const char *const kRdStName[RD_ST_N] = { "no manifest", "no dgVoodoo2 2.87.6 d3d9.dll", "no offered file", "Custom",
                                                "recognised only", "offered" };
#endif
