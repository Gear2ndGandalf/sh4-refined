// ================================================================ v3.13.0: subtitle style (size, colour, edge, box)
// Plan and review: staging/subtitles/PLAN.md (claudex-loop, Codex), RE: staging/subtitles/RE.md. Absolute VAs (GOG v7).
// Included by sh4windowmod.c after buttoncfg.inc.
//
// Every subtitle is laid out by 557660(str, 256, y, 3, 2) from the message window (L1 0x50C114 state 7, L2 0x50C0F7
// states 1 / 3 / 8, first frames L3 0x50B230 and L4 0x50B324) and drawn by the text flush's glyph pass 0x559A10: per
// glyph a shadow quad (D1 0x559B64) and the glyph quad (D2 0x559D34) through 563D20(x0, y0, x1, y1, u0, v0, u1, v1,
// c0, c1), in movies two extra half-alpha triangles (D3 0x559C94 / D4 0x559CAE, 563F40(p0, p1, p2)). The L hooks lay
// the line out with the chosen colour, remember which glyph-queue entries it made (n0..n1) and queue a box behind it
// (rect queue 557300, drawn before the glyphs); the D hooks scale those entries' quads around the block's bottom
// centre and draw the chosen edge. With every setting stock the hooks call the stock functions unchanged.


// review SUB-002: a range also keeps a copy of its first and last glyph-queue entries (20 bytes each); it applies only
// while both still hold those bytes, so a queue reset followed by other text can never be restyled.
#define SUB_Q(i) ((const BYTE *)(ULONG_PTR)(0x1257248 + 0x14 * (i)))
typedef struct { int n0, n1; float ax, ay, s; int edge; BYTE first[0x14], last[0x14];
                 int fx, ex; float t, p; } SubRange;  // v3.20.0: effect, exit (p >= 0: exit progress), line time
#define SUB_RANGES 8
static SubRange g_subR[SUB_RANGES];
static int g_subRN;
static DWORD g_subEsi;         // the glyph pass's entry pointer (esi = entry + 4) at D1 / D2 / D3 / D4

static int SubStock(void)
{
    int k;
    if (!g_subOn) return 1;  // hooks missing or partly installed: always the stock style (fail closed)
    for (k = 0; k < SUB_N; ++k)
        if (g_sub[k] != kSubStock[k]) return 0;
    return 1;
}

// The 2D vertex list the glyph quads go to (RE 4.3): {type, used, count} x 2 at 0x12F6DC0; 10 000 vertices each and no
// overflow check in 563C10 / 563E30. Returns the count of the type-3 list (0x7FFFFFFF if unreadable).
static int SubVerts(void)
{
    const int *t = (const int *)0x12F6DC0;
    int l;
    for (l = 0; l < 2; ++l)
        if (t[3 * l] == 3) return t[3 * l + 2];
    return 0x7FFFFFFF;
}
// v3.23.0 (review SH4-323-06): the vertices an extra copy at queue entry idx may use: the list's room less the stock
// D1 + D2 quads (12 vertices) of every entry from idx to the end of the queue, so no outline / glitch / ring path (menu
// or subtitle) can take the room of the glyphs still to come; <= 0 = none. Every extra-copy path checks it.
static int SubSpare(int idx)
{
    const int used = SubVerts(), cnt = *(volatile const int *)0x1261BF4;
    if (used < 0 || used > 9900 || cnt < idx || idx < 0 || cnt > 800) return 0;
    return 9900 - used - 12 * (cnt - idx);
}

// ---------------------------------------------------------------- v3.20.0 animated subtitles (staging/subtitles_v320)
// v3.21.0 (user's second list): Tremble removed, Flicker replaced by Blood Drip (kSubName in sh4windowmod.c)
enum { SFX_NONE, SFX_TYPE, SFX_FADEIN, SFX_WAVE, SFX_GLITCH, SFX_BLOOD, SFX_EMBERS };
#define SUB_TYPE_RATE 14.0f  // Typewriter letters a second (v3.20.0: 30)
enum { SEX_NONE, SEX_FADE, SEX_BURN, SEX_PEEL };
static LARGE_INTEGER g_subQf, g_subQ0;
static double SubQpc(void)  // seconds since the first call
{
    LARGE_INTEGER n;
    if (!g_subQf.QuadPart) {
        QueryPerformanceFrequency(&g_subQf);
        QueryPerformanceCounter(&g_subQ0);
        if (!g_subQf.QuadPart) g_subQf.QuadPart = 1;
    }
    QueryPerformanceCounter(&n);
    return (double)(n.QuadPart - g_subQ0.QuadPart) / (double)g_subQf.QuadPart;
}
static double g_subGame, g_subLastQ;  // review SUB320-004: the pause-aware clock (SubExitFrame advances it out of overlays)
static int SubMovie(void) { return *(volatile BYTE *)0x12A1DA6 != 0; }
static double SubNow(void) { return SubMovie() ? SubQpc() : g_subGame; }  // review SUB320-005: movies run on QPC
static float SubHash(DWORD a)  // 0..1
{
    a ^= a >> 16, a *= 0x7FEB352Du, a ^= a >> 15, a *= 0x846CA68Bu, a ^= a >> 16;
    return (float)(a & 0xFFFFFF) / 16777216.0f;
}
static float SubSat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
// the current real line: identity and start (review SUB320-002: L3 / L4 always start a new line)
static const BYTE *g_subLinePtr;
static DWORD g_subLineHash;
static double g_subLineT0;
static DWORD SubStrHash(const BYTE *s, int *len)
{
    DWORD h = 2166136261u;
    int i = 0;
    __try {
        for (; i < 255; ++i) {
            h = (h ^ s[i]) * 16777619u;
            if (i && s[i - 1] == 0xFF && s[i] == 0xFF) { ++i; break; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (len) *len = i;
    return h;
}
// v3.22.0 (user item 13, staging/feedback_v322/re_subtitles_gameplay.md): gameplay voice lines (window state 8, the
// event poll 0x4D1F60) end and re-start the same line EVERY tick through L4 (0x4D1EE3 -> 50B2A0 -> 0x50B324), which
// restarted the clock each frame (t stayed 0: Typewriter / Fade In hid the line, Glitch never burst). start 2 (L4): the
// same line (pointer and content) laid out under SUB_SAME_S ago continues. Returns 1 when the clock (re)started.
#define SUB_SAME_S 0.25
static double g_subLineSeen = -1.0;  // SubQpc() of the last layout of the current line
static int SubLineTouch(const BYTE *str, int start)
{
    const DWORD h = SubStrHash(str, NULL);
    const double q = SubQpc();
    const int same = str == g_subLinePtr && h == g_subLineHash;
    const int fresh = !same || start == 1 || (start == 2 && (g_subLineSeen < 0.0 || q - g_subLineSeen > SUB_SAME_S));
    if (fresh) {
        g_subLinePtr = str;
        g_subLineHash = h;
        g_subLineT0 = SubNow();
    }
    g_subLineSeen = q;
    return fresh;
}
// the exit copy of the last real line (review SUB320-003: with its effective alpha)
static BYTE g_subCopy[256];
static int g_subCopyOk, g_subCopyX, g_subCopyY, g_subCopyHa, g_subCopyVa;
static DWORD g_subCopyAlpha;
static LONG g_subFrame, g_subCopyFrame = -10;
static int g_subGone;  // v3.22.0: SubExitFrame calls in a row without a line up
static int g_subExitOn, g_subExitKill;  // review SUB320-R2: a line that starts after the exit was queued hides it
static double g_subExitT0;
static DWORD g_subCopyRgb, g_subForceRgb;  // review SUB320-R3: the line's own colour (0 = use the setting)
static float SubExitDur(int ex) { return ex == SEX_FADE ? 0.6f : 1.0f; }
// v3.21.0: Fade In brings its own fade-out when no Exit is chosen (user: "fade in and back out fully")
static int SubExitMode(void)
{
    const int ex = (int)g_sub[SUB_EXIT];
    return ex ? ex : g_sub[SUB_EFFECT] == SFX_FADEIN ? SEX_FADE : SEX_NONE;
}
// v3.21.0 Typewriter cursor: the newest visible glyph of the typing line (640-space), recorded by SubGlyphC, drawn by
// the next layout of the line through the rect queue (one frame behind)
static struct { float x, y0, y1, t, done; double q; int ok; } g_subCur;

// one glyph's animation (g of n in its line; t seconds since the line started; p exit progress or < 0)
typedef struct { float dx, dy, a, xs, sc; int hide; DWORD tint; float tintK, tintKB; int glitch; DWORD gtick; } SubAnimT;
// tintKB: the bottom colour's tint when > 0 (c1 is the bottom of 563D20's quad); glitch: a slice burst (SubGlyphC)
static void SubAnim(const SubRange *q, int i, SubAnimT *o)
{
    const int g = i - q->n0, n = q->n1 - q->n0 > 0 ? q->n1 - q->n0 : 1;
    const float t = q->t, s = q->s;
    memset(o, 0, sizeof *o);
    o->a = o->xs = o->sc = 1.0f;
    if (q->p >= 0.0f && g_subExitKill) {  // a new line started in this pass after the exit was queued
        o->hide = 1;
        return;
    }
    if (q->p >= 0.0f) {  // the exit
        const float p = q->p;
        if (g_sub[SUB_EFFECT] == SFX_BLOOD) o->tint = 0x000040, o->tintK = 0.45f, o->tintKB = 0.85f;  // v3.21.0: still stained
        if (q->ex == SEX_FADE) {
            o->a = 1.0f - p;
            o->dy = -4.0f * p * s;
        } else if (q->ex == SEX_BURN) {
            const float k = SubSat(1.6f * p - 0.6f * (float)g / n);
            o->tint = 0x104080;  // ember orange (0xBBGGRR, 0x80 = full)
            o->tintK = SubSat(k * 2.0f);
            o->a = 1.0f - k;
            o->dy = -12.0f * k * k * s;
            o->dx = 2.0f * sinf((float)g + p * 10.0f) * k * s;
        } else if (q->ex == SEX_PEEL) {
            const float k = SubSat(1.8f * p - 0.8f * (float)g / n);
            o->xs = cosf(k * 1.3962634f);  // turns away like a page (80 deg)
            o->dy = -10.0f * k * s;
            o->sc = 1.0f - 0.5f * k;
            o->a = 1.0f - k;
        }
        return;
    }
    switch (q->fx) {
    case SFX_TYPE: {  // v3.21.0: slower (SUB_TYPE_RATE), a short fade a letter; the cursor is SubGlyphC / SubCursor
        const float v = SubSat(t * SUB_TYPE_RATE - (float)g);
        if (v <= 0.0f) o->hide = 1;
        o->a = v;
        break;
    }
    case SFX_FADEIN: {  // v3.21.0: the whole line together (the left-to-right sweep read like Typewriter)
        const float k = SubSat(t / 0.8f);
        o->a = k * k * (3.0f - 2.0f * k);
        if (o->a <= 0.0f) o->hide = 1;
        break;
    }
    case SFX_WAVE:
        o->dy = 2.0f * s * sinf(4.0f * t + 0.5f * (float)g);
        break;
    case SFX_GLITCH: {  // v3.21.0: a 0.15 s burst every 2 s cuts the glyphs into blocks (SubGlyphC); dx = the fallback
        const DWORD slot = (DWORD)(t / 2.0f);
        const float b0 = (float)slot * 2.0f + 0.3f + 1.5f * SubHash(slot * 0x9E3779B9u);
        if (t >= b0 && t < b0 + 0.15f) {
            const DWORD tick = (DWORD)(t * 30.0f);
            o->glitch = 1;
            o->gtick = tick;
            o->dx = (SubHash(tick * 31u + (DWORD)g * 7u) - 0.5f) * 8.0f * s;
        }
        break;
    }
    case SFX_BLOOD: {  // v3.21.0 (replaces Flicker): blood soaks up from the bottom of each letter; drips in SubDrips
        const float soak = SubSat(t / 1.5f - 0.25f * SubHash((DWORD)g * 2654435761u));
        o->tint = 0x000040;  // deep red (0xBBGGRR, 0x80 = full)
        o->tintK = 0.45f * soak;
        o->tintKB = 0.15f + 0.7f * soak;
        break;
    }
    }
}
static DWORD SubAlpha(DWORD c, float a)  // scale the alpha byte, keep RGB
{
    DWORD al = (DWORD)((float)(c >> 24) * SubSat(a) + 0.5f);
    return (c & 0x00FFFFFFu) | (al > 0xFF ? 0xFF : al) << 24;
}
static DWORD SubTint(DWORD c, DWORD to, float k)  // lerp RGB bytes toward `to` (same byte order), keep alpha
{
    DWORD r = c & 0xFF000000u;
    int sh;
    for (sh = 0; sh < 24; sh += 8) {
        const float a = (float)((c >> sh) & 0xFF), b = (float)((to >> sh) & 0xFF);
        r |= (DWORD)(a + (b - a) * SubSat(k) + 0.5f) << sh;
    }
    return r;
}
// a glyph quad through the animation: offsets, page turn (x scale from its left edge), shrink about its centre
static void SubAnimQuad(const SubAnimT *o, float *x0, float *y0, float *x1, float *y1)
{
    float cx, cy, hw, hh;
    *x1 = *x0 + (*x1 - *x0) * o->xs;
    cx = (*x0 + *x1) * 0.5f, cy = (*y0 + *y1) * 0.5f, hw = (*x1 - *x0) * 0.5f * o->sc, hh = (*y1 - *y0) * 0.5f * o->sc;
    *x0 = cx - hw + o->dx, *x1 = cx + hw + o->dx;
    *y0 = cy - hh + o->dy, *y1 = cy + hh + o->dy;
}
// Embers: small squares through the rect queue 557300 (drawn before the glyphs), positions from time and index
static void SubEmbers(int x, int y, int w, int h, float t, int count, float strength)
{
    const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    int k;
    if (w <= 0 || strength <= 0.0f) return;
    for (k = 0; k < count && *(volatile int *)0x1261BF8 < 0x3E; ++k) {
        const float life = 1.2f + 0.8f * SubHash((DWORD)k * 7919u + 1u);
        float ph = t / life + SubHash((DWORD)k * 104729u + 2u), fl;
        int ex, ey, sz;
        DWORD a;
        ph -= floorf(ph);
        fl = 0.7f + 0.3f * SubHash((DWORD)(t * 15.0f) * 31u + (DWORD)k);
        ex = (int)((float)(x - w / 2) + (float)w * SubHash((DWORD)k * 15485863u + 3u) + 6.0f * sinf(t * 2.0f + (float)k));
        ey = (int)((float)y - ph * 40.0f);
        sz = SubHash((DWORD)k * 32452843u + 4u) > 0.6f ? 3 : 2;
        a = (DWORD)(96.0f * sinf(3.14159265f * ph) * fl * SubSat(strength));
        if (!a) continue;
        *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = (a > 0xFF ? 0xFF : a) << 24 | 0x104080;
        ((void(__cdecl *)(int, int, int, int))0x557300)(ex, ey - sz, sz, sz);
    }
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
}

// v3.21.0 Blood Drip: drips run down from under the line (rect queue, behind the glyphs; at most 2 rects a drip)
static void SubDrips(int x, int y, int w, float t, float s, int count)
{
    const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    const DWORD al = *(volatile DWORD *)0x1261C1C & 0xFF;  // the line's fade alpha (0x80 = full)
    int k;
    if (w <= 0) return;
    for (k = 0; k < count && *(volatile int *)0x1261BF8 < 0x3C; ++k) {
        const float period = 1.4f + 1.6f * SubHash((DWORD)k * 7919u + 11u);
        const float u = t / period + SubHash((DWORD)k * 104729u + 12u);
        const DWORD cyc = (DWORD)u;
        const float ph = u - floorf(u), grow = SubSat(ph / 0.75f);
        const float fade = ph < 0.08f ? ph / 0.08f : ph > 0.85f ? (1.0f - ph) / 0.15f : 1.0f;
        const float len = (4.0f + 24.0f * SubHash(cyc * 31u + (DWORD)k * 977u + 13u)) * s * (grow * (2.0f - grow));
        const int dx = (int)((float)(x - w / 2) + (float)w * SubHash(cyc * 15485863u + (DWORD)k * 131u + 14u));
        const int dw = SubHash((DWORD)k * 32452843u + 15u) > 0.6f ? 3 : 2, top = y - (int)(3.0f * s);
        DWORD a = (DWORD)((float)al * SubSat(fade) * 1.4f);
        if (a > 0x80) a = 0x80;
        if (!a || len < 1.0f) continue;
        *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = a << 24 | 0x000048;  // dark blood
        ((void(__cdecl *)(int, int, int, int))0x557300)(dx, top, dw, (int)len);
        if (*(volatile int *)0x1261BF8 >= 0x3C) break;
        *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = a << 24 | 0x000060;  // the drop at its end
        ((void(__cdecl *)(int, int, int, int))0x557300)(dx - 1, top + (int)len - 1, dw + 2, dw + 1);
    }
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
}
// v3.21.0 Typewriter cursor: a thin bar right of the newest letter, solid while typing, then blinking for 1.5 s
static void SubCursor(float t)
{
    const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    const DWORD al = *(volatile DWORD *)0x1261C1C & 0xFF;
    float after;
    int x, y0, h, w;
    if (!g_subCur.ok || SubQpc() - g_subCur.q > 0.15 || t + 0.05f < g_subCur.t) return;  // stale or another line
    after = t - g_subCur.done;
    if (after > 1.5f || (after > 0.0f && fmodf(after, 1.0f) >= 0.5f)) return;  // V321-010: 0.5 s on / 0.5 s off
    if (*(volatile int *)0x1261BF8 >= 0x3E) return;
    x = (int)(g_subCur.x / 1.25f) + 1;
    y0 = (int)(g_subCur.y0 / 1.0714286f);
    h = (int)((g_subCur.y1 - g_subCur.y0) / 1.0714286f);
    w = h > 24 ? 3 : 2;
    if (h < 4) return;
    *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = (al > 0x80 ? 0x80 : al) << 24 |
        (g_subForceRgb ? g_subForceRgb : kSubRgb[g_sub[SUB_COLOUR]]);
    ((void(__cdecl *)(int, int, int, int))0x557300)(x, y0 + h / 6, w, h - h / 3);
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
}

// L1-L4 and the preview / exit: the layout call with this line's clock (real = 1: a game subtitle line)
static int SubLayoutEx(const BYTE *str, int x, int y, int ha, int va, float t, float p, int real)
{
    typedef int(__cdecl *Layout)(const BYTE *, int, int, int, int);
    const Layout layout = (Layout)0x557660;
    int r, n0, n1, w, h, box, k;
    float s;
    if (SubStock()) return layout(str, x, y, ha, va);
    if (real && SubExitMode() && !SubMovie()) {  // the exit copy (string, place, colour alpha) of the line drawn now
        int len;
        SubStrHash(str, &len);
        if (len > 0 && len <= (int)sizeof g_subCopy) {
            __try {
                memcpy(g_subCopy, str, len);
                g_subCopyOk = 1;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                g_subCopyOk = 0;
            }
            g_subCopyX = x, g_subCopyY = y, g_subCopyHa = ha, g_subCopyVa = va;
            g_subCopyAlpha = *(volatile DWORD *)0x1261C1C & 0xFF;
            g_subCopyRgb = kSubRgb[g_sub[SUB_COLOUR]];
            g_subCopyFrame = g_subFrame;
        }
    }
    n0 = *(volatile int *)0x1261BF4;
    ((void(__cdecl *)(DWORD))0x556A90)(g_subForceRgb ? g_subForceRgb : kSubRgb[g_sub[SUB_COLOUR]]);  // keeps the fade alpha (RE 4.2)
    r = layout(str, x, y, ha, va);
    ((void(__cdecl *)(DWORD))0x556A90)(0x808080);  // the stock colour state
    n1 = *(volatile int *)0x1261BF4;
    w = *(volatile int *)0x1261C28;
    h = *(volatile int *)0x1261C2C;
    s = kSubScale[g_sub[SUB_SIZE]];
    if (w > 0 && w * s > 496.0f) s = 496.0f / (float)w;  // phase 1: fit the 4:3 canvas (no re-wrap)
    if (s < 0.5f) s = 0.5f;
    box = g_sub[SUB_BOX];
    if (box && w > 0 && h > 0 && *(volatile int *)0x1261BF8 < 0x3F) {  // the rect queue holds 0x40 (RE 5.1)
        const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
        DWORD a = (box == 1 ? 0x40u : 0x80u) * (*(volatile DWORD *)0x1261C1C & 0xFF) / 0x80u;
        if (p >= 0.0f) a = (DWORD)((float)a * (1.0f - SubSat(p)));  // review SUB320-R1: the box leaves with the text
        const float pad = 5.0f, bw = w * s + 2 * pad, bh = h * s + 2 * pad;
        *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = (a > 0xFF ? 0xFF : a) << 24;  // black
        ((void(__cdecl *)(int, int, int, int))0x557300)((int)(x - bw / 2), (int)(y - h * s - pad), (int)bw, (int)bh);
        *(volatile DWORD *)0x1261C14 = c14;
        *(volatile DWORD *)0x1261C18 = c18;
    }
    for (k = 0; k < g_subRN; ++k)  // a range ending past this layout's start predates a queue reset: stale
        if (g_subR[k].n1 > n0) { g_subR[k] = g_subR[--g_subRN]; --k; }
    if (n1 > n0 && g_subRN < SUB_RANGES && (s != 1.0f || g_sub[SUB_EDGE] || g_sub[SUB_EFFECT] || p >= 0.0f)) {
        SubRange *q = &g_subR[g_subRN++];
        q->n0 = n0;
        q->n1 = n1;
        q->ax = x * 1.25f;              // 640-space (557660: x * 1.25, y * 1.0714286; RE 3.1)
        q->ay = y * 1.0714286f;
        q->s = s;
        q->edge = g_sub[SUB_EDGE];
        q->fx = p >= 0.0f ? SFX_NONE : (int)g_sub[SUB_EFFECT];
        q->ex = SubExitMode();
        q->t = t;
        q->p = p;
        memcpy(q->first, SUB_Q(n0), 0x14);
        memcpy(q->last, SUB_Q(n1 - 1), 0x14);
    }
    if (g_sub[SUB_EFFECT] == SFX_EMBERS && p < 0.0f) SubEmbers(x, y, (int)(w * s), (int)(h * s), t, 14, 1.0f);
    if (g_sub[SUB_EFFECT] == SFX_BLOOD && p < 0.0f) SubDrips(x, y, (int)(w * s), t, s, 10);  // v3.21.0
    if (g_sub[SUB_EFFECT] == SFX_TYPE && p < 0.0f && !SubMovie()) SubCursor(t);               // v3.21.0
    if (p >= 0.0f && SubExitMode() == SEX_BURN) SubEmbers(x, y, (int)(w * s), (int)(h * s), t + p * 2.0f, 20, 1.0f - p);
    return r;
}
static int __cdecl SubLayout(const BYTE *str, int x, int y, int ha, int va)  // L1 (per-frame layout, state 7)
{
    if (SubStock()) return ((int(__cdecl *)(const BYTE *, int, int, int, int))0x557660)(str, x, y, ha, va);
    {
        const double t0 = g_subLineT0;
        SubLineTouch(str, 0);
        if (g_subLineT0 != t0) g_subCur.ok = 0;  // review V321-010: another line
    }
    return SubLayoutEx(str, x, y, ha, va, (float)(SubNow() - g_subLineT0), -1.0f, 1);
}
static int __cdecl SubLayoutStart2(const BYTE *str, int x, int y, int ha, int va);
static int __cdecl SubLayoutStart(const BYTE *str, int x, int y, int ha, int va)  // L3 / L4: a line begins
{
    if (SubStock()) return ((int(__cdecl *)(const BYTE *, int, int, int, int))0x557660)(str, x, y, ha, va);
    SubLineTouch(str, 1);
    if (g_subExitOn) g_subExitKill = 1;  // review SUB320-R2: an exit already queued this pass is hidden
    g_subCur.ok = 0;  // review V321-010: a new line has no cursor until its own glyph pass records one
    g_subExitOn = 0;  // a new line ends any exit
    return SubLayoutEx(str, x, y, ha, va, 0.0f, -1.0f, 1);
}
static int __cdecl SubLayoutStart2(const BYTE *str, int x, int y, int ha, int va)  // v3.22.0: L4 (gameplay lines too)
{
    static int s_logged;
    if (SubStock()) return ((int(__cdecl *)(const BYTE *, int, int, int, int))0x557660)(str, x, y, ha, va);
    if (!SubLineTouch(str, 2)) {  // the same line again (the gameplay poll re-starts it every tick): it continues
        if (!s_logged++) Log("subtitles: a line re-started every frame (gameplay dialogue) keeps its effect clock");
        return SubLayoutEx(str, x, y, ha, va, (float)(SubNow() - g_subLineT0), -1.0f, 1);
    }
    if (g_subExitOn) g_subExitKill = 1;
    g_subCur.ok = 0;
    g_subExitOn = 0;
    return SubLayoutEx(str, x, y, ha, va, 0.0f, -1.0f, 1);
}
static int __cdecl SubLayoutL2(const BYTE *str, int x, int y, int ha, int va)  // states 1 / 3 / 8: only 8 is a subtitle
{
    if (*(volatile int *)0x108CF60 != 8) return ((int(__cdecl *)(const BYTE *, int, int, int, int))0x557660)(str, x, y, ha, va);
    return SubLayout(str, x, y, ha, va);
}

// The range of glyph-queue entry i, or NULL. Ranges whose end is past the queue count are stale (a reset happened).
static SubRange *SubFind(int i)
{
    const int cnt = *(volatile int *)0x1261BF4;
    int k;
    for (k = 0; k < g_subRN; ++k) {
        SubRange *q = &g_subR[k];
        if (q->n1 > cnt || memcmp(q->first, SUB_Q(q->n0), 0x14) || memcmp(q->last, SUB_Q(q->n1 - 1), 0x14)) {
            *q = g_subR[--g_subRN];  // stale (a queue reset since the layout)
            --k;
            continue;
        }
        if (i >= q->n0 && i < q->n1) return q;
    }
    return NULL;
}
static int SubIndex(void) { return (int)((g_subEsi - 4 - 0x1257248) / 0x14); }

typedef int(__cdecl *Quad)(float, float, float, float, float, float, float, float, DWORD, DWORD);
#define QUAD ((Quad)0x563D20)

// v3.21.0 Glitch (user: "letters separate by blocks and stretch a bit, r/g/b distortions on the separated blocks"): the
// glyph quad (raw 640-space x0..y1, before the range scale) as SUB_SLICES horizontal bands of the same glyph; the bands a
// hash picks for this 1/30 s tick shift sideways and stretch, and only they get red / blue copies at half alpha; the
// rest (the letter's core) stays as it is. shadow > 0: the shadow pass (no fringes, offset by the scaled stock +2).
#define SUB_SLICES 4
static DWORD SubLerpC(DWORD a, DWORD b, float k)  // all four bytes
{
    DWORD r = 0;
    int sh;
    for (sh = 0; sh < 32; sh += 8) {
        const float x = (float)((a >> sh) & 0xFF), y = (float)((b >> sh) & 0xFF);
        r |= (DWORD)(x + (y - x) * k + 0.5f) << sh;
    }
    return r;
}
static int SubSlices(const SubRange *q, int i, const SubAnimT *an, float x0, float y0, float x1, float y1, float u0,
                     float v0, float u1, float v1, DWORD c0, DWORD c1, int shadow)
{
    const int g = i - q->n0;
    const float gx0 = q->ax + (x0 - q->ax) * q->s, gy0 = q->ay + (y0 - q->ay) * q->s;
    const float gx1 = q->ax + (x1 - q->ax) * q->s, gy1 = q->ay + (y1 - q->ay) * q->s;
    const float o = shadow ? 2.0f * q->s : 0.0f, cx = 0.5f * (gx0 + gx1), hw = 0.5f * (gx1 - gx0);
    int b, r = 0;
    for (b = 0; b < SUB_SLICES; ++b) {
        const float k0 = (float)b / SUB_SLICES, k1 = (float)(b + 1) / SUB_SLICES;
        const float sy0 = gy0 + (gy1 - gy0) * k0, sy1 = gy0 + (gy1 - gy0) * k1;
        const float sv0 = v0 + (v1 - v0) * k0, sv1 = v0 + (v1 - v0) * k1;
        const DWORD sc0 = SubLerpC(c0, c1, k0), sc1 = SubLerpC(c0, c1, k1);
        const DWORD hsh = an->gtick * 2654435761u + (DWORD)g * 40503u + (DWORD)b * 9176u;
        if (SubHash(hsh) > 0.5f) {  // a separated block
            const float dx = (SubHash(hsh + 1u) - 0.5f) * 12.0f * q->s, st = 1.0f + 0.35f * SubHash(hsh + 2u);
            const float bx0 = cx - hw * st + dx, bx1 = cx + hw * st + dx, f = 1.5f * q->s;
            if (!shadow) {
                const DWORD rr0 = SubAlpha(SubTint(sc0, 0x000080, 1.0f), 0.5f), rr1 = SubAlpha(SubTint(sc1, 0x000080, 1.0f), 0.5f);
                const DWORD bb0 = SubAlpha(SubTint(sc0, 0x800000, 1.0f), 0.5f), bb1 = SubAlpha(SubTint(sc1, 0x800000, 1.0f), 0.5f);
                QUAD(bx0 - f, sy0, bx1 - f, sy1, u0, sv0, u1, sv1, rr0, rr1);  // red fringe
                QUAD(bx0 + f, sy0, bx1 + f, sy1, u0, sv0, u1, sv1, bb0, bb1);  // blue fringe
                r = QUAD(bx0, sy0, bx1, sy1, u0, sv0, u1, sv1, SubTint(sc0, 0x008000, 0.35f), SubTint(sc1, 0x008000, 0.35f));
            } else {
                r = QUAD(bx0 + o, sy0 + o, bx1 + o, sy1 + o, u0, sv0, u1, sv1, sc0, sc1);
            }
        } else {
            r = QUAD(gx0 + o, sy0 + o, gx1 + o, sy1 + o, u0, sv0, u1, sv1, sc0, sc1);
        }
    }
    return r;
}

// v3.23.0 (user items 1 / 4 / 5): the Screen Effects page's black outline, drawn here instead of queued (the text
// queue holds 800 entries: sh4windowmod.c OutlineShadow records the ranges). The ring goes round the glyph's own
// position (the D1 quad minus its stock +2, +2) in the D1 colours (alpha-only black), then the stock D1 quad.
static int OptRingSame(const OptRing *o)  // the main string followed its shadow string: the same glyphs (u / v words)
{
    const int cnt = *(volatile const int *)0x1261BF4;
    if (o->n0 - o->g < 0 || o->n0 + o->g > cnt || o->n0 + o->g > 800) return 0;
    return *(const DWORD *)(SUB_Q(o->n0) + 8) == *(const DWORD *)(SUB_Q(o->n0 - o->g) + 8) &&
           *(const DWORD *)(SUB_Q(o->n0 + o->g - 1) + 8) == *(const DWORD *)(SUB_Q(o->n0 - 1) + 8);
}
static int OptRingFits(int idx, int ringLeft, int n)  // every entry still to come keeps its D1 + D2 quads (12 vertices)
{
    if (ringLeft < 0 || ringLeft > 800) return 0;
    return SubSpare(idx) >= 6 * n * ringLeft;
}
static int OptRingQuad(int idx, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, DWORD c0,
                       DWORD c1, int *done)
{
    static const float kDx[8] = { 1, -1, 1, -1, 1.4f, -1.4f, 0, 0 }, kDy[8] = { 1, 1, -1, -1, 0, 0, 1.4f, -1.4f };
    const float d = 1.25f;
    int k, j;
    *done = 0;
    for (k = 0; k < g_optRingN; ++k) {
        OptRing *o = &g_optRing[k];
        if (idx < o->n0 || idx >= o->n0 + o->g) continue;
        if (!o->chk) o->chk = OptRingSame(o) ? 1 : -1;
        if (o->chk < 0) return 0;
        if (g_optRingCopies < 0) {  // once a pass: 8, 4 or 0 copies for every ring entry from here on
            int left = 0;
            for (j = 0; j < g_optRingN; ++j) {
                const OptRing *q = &g_optRing[j];
                if (q->chk < 0 || q->n0 + q->g <= idx) continue;
                left += q->n0 >= idx ? q->g : q->n0 + q->g - idx;
            }
            g_optRingCopies = OptRingFits(idx, left, 8) ? 8 : OptRingFits(idx, left, 4) ? 4 : 0;
        }
        if (g_optRingCopies && OptRingFits(idx, 1, g_optRingCopies))  // and still room for this one (fail safe)
            for (j = 0; j < g_optRingCopies; ++j)
                QUAD(x0 - 2 + kDx[j] * d, y0 - 2 + kDy[j] * d, x1 - 2 + kDx[j] * d, y1 - 2 + kDy[j] * d, u0, v0, u1, v1, c0, c1);
        *done = 1;
        return QUAD(x0, y0, x1, y1, u0, v0, u1, v1, c0, c1);
    }
    return 0;
}

// D1: the glyph's shadow quad (the glyph at +2, +2 with alpha-only black).
static int __cdecl SubShadowC(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, DWORD c0, DWORD c1)
{
    const int idx = SubIndex();
    const SubRange *q = SubFind(idx);
    float gx0, gy0, gx1, gy1;
    int k, n;
    SubAnimT an;
    if (g_optRingN && !q) {  // v3.23.0: the Screen Effects page's outline
        const int r = OptRingQuad(idx, x0, y0, x1, y1, u0, v0, u1, v1, c0, c1, &n);
        if (n) return r;
    }
    if (!q) return QUAD(x0, y0, x1, y1, u0, v0, u1, v1, c0, c1);
    SubAnim(q, idx, &an);  // v3.20.0
    if (an.hide) return 0;
    c0 = SubAlpha(c0, an.a);
    c1 = SubAlpha(c1, an.a);
    if (an.glitch && q->edge != 1 && SubSpare(idx) >= 6 * SUB_SLICES + 6 * 3 * SUB_SLICES)  // v3.21.0: blocks, the
        return SubSlices(q, idx, &an, x0 - 2, y0 - 2, x1 - 2, y1 - 2, u0, v0, u1, v1, c0, c1, 2);  // stock offset only
    // the glyph itself (shadow minus its +2, +2), scaled around the anchor
    gx0 = q->ax + (x0 - 2 - q->ax) * q->s;
    gy0 = q->ay + (y0 - 2 - q->ay) * q->s;
    gx1 = q->ax + (x1 - 2 - q->ax) * q->s;
    gy1 = q->ay + (y1 - 2 - q->ay) * q->s;
    SubAnimQuad(&an, &gx0, &gy0, &gx1, &gy1);
    if (q->edge == 1) return 0;  // None
    if (q->edge >= 2) {          // Outline (+ Shadow): a ring of copies in the shadow colour, if the vertex list has room
        static const float kDx[8] = { 1, -1, 1, -1, 1.4f, -1.4f, 0, 0 }, kDy[8] = { 1, 1, -1, -1, 0, 0, 1.4f, -1.4f };
        const float d = 1.6f * q->s;
        const int room = SubSpare(idx);  // v3.23.0 (SH4-323-06): the glyphs still to come keep their room
        n = room >= 6 * 8 ? 8 : room >= 6 * 4 ? 4 : 0;
        for (k = 0; k < n; ++k)
            QUAD(gx0 + kDx[k] * d, gy0 + kDy[k] * d, gx1 + kDx[k] * d, gy1 + kDy[k] * d, u0, v0, u1, v1, c0, c1);
        if (q->edge == 2 && n) return 0;
    }
    {   // the stock shadow, its offset scaled (Shadow, Outline + Shadow, or an outline that had no room)
        const float o = 2 * q->s;
        return QUAD(gx0 + o, gy0 + o, gx1 + o, gy1 + o, u0, v0, u1, v1, c0, c1);
    }
}

// D2: the glyph quad. The last entry of the queue ends every range (the pass is over).
static int __cdecl SubGlyphC(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, DWORD c0, DWORD c1)
{
    const int i = SubIndex();
    const SubRange *q = SubFind(i);
    int r;
    if (q) {
        SubAnimT an;
        float gx0 = q->ax + (x0 - q->ax) * q->s, gy0 = q->ay + (y0 - q->ay) * q->s;
        float gx1 = q->ax + (x1 - q->ax) * q->s, gy1 = q->ay + (y1 - q->ay) * q->s;
        SubAnim(q, i, &an);  // v3.20.0
        if (an.hide) {
            r = 0;
        } else if (an.glitch && SubSpare(i) >= 6 * 3 * SUB_SLICES) {  // v3.21.0: the glyph in blocks (v3.23.0: SubSpare)
            r = SubSlices(q, i, &an, gx0, gy0, gx1, gy1, u0, v0, u1, v1, SubAlpha(c0, an.a), SubAlpha(c1, an.a), 0);
        } else {
            SubAnimQuad(&an, &gx0, &gy0, &gx1, &gy1);
            if (an.tint) c0 = SubTint(c0, an.tint, an.tintK), c1 = SubTint(c1, an.tint, an.tintKB > 0.0f ? an.tintKB : an.tintK);
            r = QUAD(gx0, gy0, gx1, gy1, u0, v0, u1, v1, SubAlpha(c0, an.a), SubAlpha(c1, an.a));
            if (q->fx == SFX_TYPE && q->p < 0.0f) {  // v3.21.0: the newest letter so far (the cursor follows it)
                const int g = i - q->n0, n = q->n1 - q->n0;
                const int newest = (int)(q->t * SUB_TYPE_RATE) < n - 1 ? (int)(q->t * SUB_TYPE_RATE) : n - 1;
                if (g == newest) {
                    g_subCur.x = gx1, g_subCur.y0 = gy0, g_subCur.y1 = gy1;
                    g_subCur.t = q->t;
                    g_subCur.done = (float)n / SUB_TYPE_RATE;
                    g_subCur.q = SubQpc();
                    g_subCur.ok = 1;
                }
            }
        }
    } else {
        r = QUAD(x0, y0, x1, y1, u0, v0, u1, v1, c0, c1);
    }
    if (i >= *(volatile int *)0x1261BF4 - 1) {
        g_subRN = 0;
        if (g_optRingN) g_optRingLast = g_optRingCopies;  // v3.23.0: the pass is over: the menu ring ranges end too
        g_optRingN = g_optRingEnd = 0;
        g_optRingCopies = -1;
    }
    return r;
}

// D3 / D4 (movies): a half-alpha triangle of three 0x1C-byte vertices {x, y, z, rhw, colour, u, v}; transformed copies.
static int __cdecl SubTriC(const float *p0, const float *p1, const float *p2)
{
    const int idx = SubIndex();
    const SubRange *q = SubFind(idx);
    float v[3][7];
    const float *src[3];
    int k;
    SubAnimT an;
    src[0] = p0;
    src[1] = p1;
    src[2] = p2;
    if (!q) return ((int(__cdecl *)(const float *, const float *, const float *))0x563F40)(p0, p1, p2);
    SubAnim(q, idx, &an);  // v3.20.0 (review SUB320-001: offsets and alpha / tint on every vertex colour too)
    if (an.hide) return 0;
    for (k = 0; k < 3; ++k) {
        DWORD c;
        memcpy(v[k], src[k], sizeof v[k]);
        v[k][0] = q->ax + (v[k][0] - q->ax) * q->s + an.dx;
        v[k][1] = q->ay + (v[k][1] - q->ay) * q->s + an.dy;
        memcpy(&c, &v[k][4], 4);
        if (an.tint) c = SubTint(c, an.tint, an.tintK);
        c = SubAlpha(c, an.a);
        memcpy(&v[k][4], &c, 4);
    }
    return ((int(__cdecl *)(const float *, const float *, const float *))0x563F40)(v[0], v[1], v[2]);
}

// review SUB-004: the glyph-queue reset 559E90 (6 callers) forgets every range, so a range never outlives its queue
// generation. Entry hook (its own first 9 bytes, `push esi / xor esi,esi / mov [1261BF4],esi`; the caller 0x4EFC80 sits
// inside the cheat table's "HUD Text" pattern, so the callers stay stock).
static const DWORD g_backSubReset = 0x559E99;
static __declspec(naked) void hkSubReset(void)
{
    __asm {
        mov dword ptr [g_subRN], 0
        push esi                         // the 9 stolen bytes
        xor esi, esi
        mov dword ptr ds:[1261BF4h], esi
        jmp dword ptr [g_backSubReset]
    }
}

static __declspec(naked) void hkSubShadow(void) { __asm { mov dword ptr [g_subEsi], esi
                                                          jmp SubShadowC } }
static __declspec(naked) void hkSubGlyph(void) { __asm { mov dword ptr [g_subEsi], esi
                                                         jmp SubGlyphC } }
static __declspec(naked) void hkSubTri(void) { __asm { mov dword ptr [g_subEsi], esi
                                                       jmp SubTriC } }

// ---------------------------------------------------------------- the preview on the Subtitle Style page
static BYTE g_subSample[64];
static void SubPreview(void)  // the row draw of the page's row 0 in the active slot: once per frame
{
    // the menu's own text colour is current here (real subtitles start from 559DF0's white): set the subtitle colour
    // even for stock settings, and put the menu's colours back afterwards
    const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    if (!g_subOn) return;
    ((void(__cdecl *)(DWORD))0x556A90)(kSubRgb[g_sub[SUB_COLOUR]]);
    {   // v3.20.0 (review SUB320-002): a 4 s cycle with its own clock: 2.8 s with the Effect, then the Exit
        const float cyc = (float)fmod(SubQpc(), 4.0);
        if (cyc < 2.8f) SubLayoutEx(g_subSample, 256, 424, 3, 2, cyc, -1.0f, 0);
        else if (SubExitMode() && cyc < 2.8f + SubExitDur(SubExitMode()))
            SubLayoutEx(g_subSample, 256, 424, 3, 2, cyc, (cyc - 2.8f) / SubExitDur(SubExitMode()), 0);
        else if (!SubExitMode())
            SubLayoutEx(g_subSample, 256, 424, 3, 2, cyc, -1.0f, 0);
    }
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
}

// v3.20.0: every main-loop pass from the 0x4EFBFD hook (items.inc SparkleFrame): the pause-aware clock, and the exit
// of a line the game ended in this tick (laid out in the previous pass, state 0 now), replayed from its copy
static void SubExitFrame(void)
{
    const double q = SubQpc();
    double dt = q - g_subLastQ;
    const int phase = *(volatile int *)0xFD5C20;
    g_subLastQ = q;
    if (dt < 0.0 || dt > 0.1) dt = 0.1;
    if (phase != 2) g_subGame += dt;
    ++g_subFrame;
    if (!g_subOn || !SubExitMode() || SubMovie()) { g_subExitOn = 0; return; }
    if (phase == 2) return;  // overlays: the exit waits (frozen clock), nothing drawn over their own text
    if (*(volatile int *)0x108CF60 != 0) { g_subExitOn = 0; g_subGone = 0; return; }  // a line is up
    ++g_subGone;  // v3.22.0: an exit starts on the 2nd frame without the line (a one-frame gap of the gameplay poll)
    if (!g_subExitOn && g_subCopyOk && g_subGone == 2 && g_subCopyFrame == g_subFrame - 2) {
        g_subExitOn = 1;
        g_subExitKill = 0;
        g_subExitT0 = g_subGame;
    }
    if (g_subExitOn) {
        const float p = (float)((g_subGame - g_subExitT0) / SubExitDur(SubExitMode()));
        const DWORD c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18, c1c = *(volatile DWORD *)0x1261C1C;
        if (p >= 1.0f) {
            g_subExitOn = 0;
            g_subCopyOk = 0;
            return;
        }
        __try {
            ((void(__cdecl *)(DWORD))0x556AE0)(g_subCopyAlpha);
            ((void(__cdecl *)(DWORD))0x556A90)(g_subCopyRgb);
            g_subForceRgb = g_subCopyRgb;
            SubLayoutEx(g_subCopy, g_subCopyX, g_subCopyY, g_subCopyHa, g_subCopyVa, (float)(g_subGame - g_subExitT0), p, 0);
            g_subForceRgb = 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_subExitOn = 0;
            g_subCopyOk = 0;
            g_subForceRgb = 0;
        }
        *(volatile DWORD *)0x1261C14 = c14;
        *(volatile DWORD *)0x1261C18 = c18;
        *(volatile DWORD *)0x1261C1C = c1c;
    }
}

// ---------------------------------------------------------------- test trigger ([Subtitles] Test=1, F11 in gameplay)
static int g_subTest;
static DWORD g_subTestEnd;
static BYTE g_subTestStr[96];
static void __cdecl SubTestTick(void)  // game thread, every frame (QsTick dispatcher)
{
    static int s_down;
    const int down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (!g_subTest) return;
    if (g_subTestEnd && (int)(GetTickCount() - g_subTestEnd) >= 0) {
        g_subTestEnd = 0;
        ((void(__cdecl *)(void))0x50AE00)();
    }
    if (down && !s_down && !g_subTestEnd && *(volatile int *)0x1236D4C == 6 && *(volatile int *)0x108CF60 == 0) {
        ((void(__cdecl *)(int, const BYTE *))0x50B1C0)(-1, g_subTestStr);
        g_subTestEnd = GetTickCount() + 4000;
        Log("subtitles: test line shown (F11)");
    }
    s_down = down;
}

static void SubtitlesInstall(void)
{
    static const struct { DWORD va; BYTE stock[5]; const void *to; } kSites[] = {
        { 0x50C114, { 0xE8, 0x47, 0xB5, 0x04, 0x00 }, SubLayout },
        { 0x50C0F7, { 0xE8, 0x64, 0xB5, 0x04, 0x00 }, SubLayoutL2 },
        { 0x50B230, { 0xE8, 0x2B, 0xC4, 0x04, 0x00 }, SubLayoutStart },  // v3.20.0: a line begins
        { 0x50B324, { 0xE8, 0x37, 0xC3, 0x04, 0x00 }, SubLayoutStart2 },  // v3.22.0: continues a re-started line
        { 0x559B64, { 0xE8, 0xB7, 0xA1, 0x00, 0x00 }, hkSubShadow },
        { 0x559D34, { 0xE8, 0xE7, 0x9F, 0x00, 0x00 }, hkSubGlyph },
        { 0x559C94, { 0xE8, 0xA7, 0xA2, 0x00, 0x00 }, hkSubTri },
        { 0x559CAE, { 0xE8, 0x8D, 0xA2, 0x00, 0x00 }, hkSubTri },
    };
    static const Site kCallees[] = {
        { 0x557660, { 0x83, 0xEC, 0x50, 0x56, 0x8B, 0x74, 0x24, 0x58 }, 8 },
        { 0x563D20, { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x8B, 0x5C, 0x24, 0x18 }, 9 },
        { 0x563F40, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0x6A, 0x02, 0xE8, 0xE4, 0xFE, 0xFF, 0xFF }, 12 },
        { 0x557300, { 0x8B, 0x0D, 0xF8, 0x1B, 0x26, 0x01, 0x66, 0x8B, 0x54, 0x24 }, 10 },
        { 0x559A10, { 0xA1, 0xF4, 0x1B, 0x26, 0x01, 0x81, 0xEC, 0xA8, 0x00, 0x00 }, 10 },
        { 0x556A90, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0x1C, 0x1C, 0x26, 0x01 }, 10 },
        { 0x559E90, { 0x56, 0x33, 0xF6, 0x89, 0x35, 0xF4, 0x1B, 0x26, 0x01 }, 9 },  // the queue reset (SUB-004)
    };
    const int n = (int)(sizeof kSites / sizeof kSites[0]);
    int i, done, k, v, resetOk;
    char buf[32];
    for (k = 0; k < SUB_N; ++k) {  // [Subtitles] (unknown text = stock)
        IniStr("Subtitles", kSubIniKey[k], buf, sizeof buf);
        for (v = kSubCount[k] - 1; v >= 0 && _stricmp(buf, kSubName[k][v]); --v) {}
        if (v < 0 && k == SUB_COLOUR && !_stricmp(buf, "Grey")) {  // v3.19.0: Grey became Red (user item 1)
            v = 4;
            if (!g_iniFallback) WritePrivateProfileStringA("Subtitles", "Colour", kSubName[SUB_COLOUR][4], g_iniPath);
            Log("subtitles: [Subtitles] Colour=Grey (removed) is now Red");
        }
        if (v < 0 && k == SUB_EFFECT && (!_stricmp(buf, "Tremble") || !_stricmp(buf, "Flicker"))) {  // v3.21.0
            v = !_stricmp(buf, "Flicker") ? SFX_BLOOD : SFX_NONE;  // Flicker's place went to Blood Drip; Tremble is gone
            if (!g_iniFallback) WritePrivateProfileStringA("Subtitles", "Effect", kSubName[SUB_EFFECT][v], g_iniPath);
            Log("subtitles: [Subtitles] Effect=%s (removed) is now %s", buf, kSubName[SUB_EFFECT][v]);
        }
        g_sub[k] = v >= 0 ? v : kSubStock[k];
    }
    g_subTest = GetPrivateProfileIntA("Subtitles", "Test", 0, g_iniPath) != 0;  // hidden, not in the default ini
    Encode(g_subSample, sizeof g_subSample, "Subtitles look like this.");
    Encode(g_subTestStr, sizeof g_subTestStr, "This is a test subtitle, two lines long,");
    {   // second line: replace the end marker with FF FD and append
        size_t len = 0;
        while (len + 1 < sizeof g_subTestStr && !(g_subTestStr[len] == 0xFF && g_subTestStr[len + 1] == 0xFF)) ++len;
        if (len + 2 < sizeof g_subTestStr) {
            BYTE tail[48];
            size_t m;
            g_subTestStr[len] = 0xFF;
            g_subTestStr[len + 1] = 0xFD;
            Encode(tail, sizeof tail, "to check the style.");
            for (m = 2; m < sizeof tail && len + m < sizeof g_subTestStr; ++m) g_subTestStr[len + m] = tail[m];  // skip 00 80
        }
    }
    for (i = 0; i < (int)(sizeof kCallees / sizeof kCallees[0]); ++i)
        if (!SiteOk(&kCallees[i])) {
            Log("subtitles: %08lX is not stock (another mod?): subtitles keep the game's style", kCallees[i].va);
            return;
        }
    for (i = 0; i < n; ++i)
        if (memcmp((const void *)(ULONG_PTR)kSites[i].va, kSites[i].stock, 5)) {
            Log("subtitles: %08lX is not the stock call (another mod?): subtitles keep the game's style", kSites[i].va);
            return;
        }
    for (done = 0; done < n; ++done)
        if (!PokeBranch(kSites[done].va, 0xE8, kSites[done].to, 5)) break;
    // the queue-reset hook last (review SUB-004); if it fails, every call written so far is put back
    resetOk = done == n && PokeBranch(0x559E90, 0xE9, hkSubReset, 9);
    if (!resetOk) {
        static const BYTE kReset[9] = { 0x56, 0x33, 0xF6, 0x89, 0x35, 0xF4, 0x1B, 0x26, 0x01 };
        int lost = 0;
        if (done == n && memcmp((const void *)0x559E90, kReset, 9) &&
            (!Poke(0x559E90, kReset, 9) || memcmp((const void *)0x559E90, kReset, 9))) {
            ++lost;
            Log("subtitles: 00559E90 could not be put back");
        }
        for (i = 0; i < done; ++i)
            if (!Poke(kSites[i].va, kSites[i].stock, 5) || memcmp((const void *)(ULONG_PTR)kSites[i].va, kSites[i].stock, 5)) {
                ++lost;
                Log("subtitles: %08lX could not be put back", kSites[i].va);
            }
        Log("subtitles: a patch could not be written%s: the game's style stays", lost ? "; some sites stay patched (they forward to the stock calls)" : "");
        return;  // g_subOn stays 0: every hook forwards unchanged
    }
    g_subOn = 1;
    if (g_subTest) g_subTick = SubTestTick;
    Log("subtitles: style hooks in place (%d calls); Size %s, Colour %s, Edge %s, Box %s%s", n, kSubName[0][g_sub[0]],
        kSubName[1][g_sub[1]], kSubName[2][g_sub[2]], kSubName[3][g_sub[3]], g_subTest ? "; test line on F11" : "");
}
