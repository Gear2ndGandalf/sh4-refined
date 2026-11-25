// ---------------------------------------------------------------- diagnostics ([Window] InterpDiag=1)
// Off by default. Research aid for v2.2 (frame pacing, things that do not move smoothly):
// * SH4Refined_pacing.csv: one line per presented tick: the game's own time up to its Present, the replay, each Present
//   call (start and duration, relative to the tick start), the next tick, and the DWM refresh interval each picture
//   was handed over in (two pictures in one interval = only the second one is seen).
// * InterpDiag=2: only the frame dumps (no pacing file).
// * Frame dump: Ctrl+F12 (the next 2 recorded frames), or create the file SH4Refined_dump.flag next to the dll; the next recorded frame is written to
//   SH4Refined_frame_<n>.txt (every draw with its game call sites, shader, texture, vertex data sample and how each
//   vertex-shader constant upload before it was paired: S same, M blended, R kept new (jump), N no partner), then the
//   flag is deleted.
#include <intrin.h>
#include <stdio.h>
#pragma intrinsic(_AddressOfReturnAddress)

static int g_diag;
static char g_diagDir[MAX_PATH];
static FILE *g_trace;
typedef struct { DWORD ra[3]; WORD op, pad; int up0, up1; DWORD vs, tex0, stride, nv, prim; float v0[4]; } DDraw;
#define DD_N 6144
static DDraw g_dd[DD_N];
static int g_ndd, g_ddUp;
typedef struct { DWORD ra[3]; const void *buf; DWORD off, size; int draw; float v0[4]; } DBuf;
#define DB_N 2048
static DBuf g_db[DB_N];
static int g_ndb;
static int g_dumpReq, g_dumpN;
static unsigned g_dumpCheck;

// DWM timing (dwmapi.dll loaded on demand)
#include <dwmapi.h>
typedef HRESULT(WINAPI *DwmTiming_t)(HWND, void *);
static DwmTiming_t g_dwmTiming;
static int g_dwmTried;
static unsigned __int64 g_lastIval;
static unsigned g_ivals;  // refresh intervals that got a new picture (per Stats window)

// the refresh interval (DWM's refresh counter) that time t falls in; 0 = unknown
static unsigned __int64 DiagInterval(unsigned __int64 t)
{
    DWM_TIMING_INFO ti;
    if (!g_dwmTried) {
        HMODULE m = LoadLibraryA("dwmapi.dll");
        g_dwmTried = 1;
        g_dwmTiming = m ? (DwmTiming_t)GetProcAddress(m, "DwmGetCompositionTimingInfo") : NULL;
    }
    if (!g_dwmTiming) return 0;
    memset(&ti, 0, sizeof ti);
    ti.cbSize = sizeof ti;
    if (FAILED(g_dwmTiming(NULL, &ti)) || !ti.qpcRefreshPeriod) return 0;
    if (t < ti.qpcVBlank) return ti.cRefresh - (ti.qpcVBlank - t) / ti.qpcRefreshPeriod;
    return ti.cRefresh + (t - ti.qpcVBlank) / ti.qpcRefreshPeriod + 1;  // +1: the interval after the last vblank
}

static void DiagCountPicture(unsigned __int64 t)
{
    const unsigned __int64 iv = DiagInterval(t);
    if (iv && iv != g_lastIval) g_ivals++;
    g_lastIval = iv;
}

static int IsExeCode(DWORD v) { return v >= 0x401000 && v < 0x5B0000; }

static void DiagStack(DWORD *ra, void *retSlot)
{
    const DWORD *s = (const DWORD *)retSlot;
    int i, n = 1;
    ra[0] = s[0];
    ra[1] = ra[2] = 0;
    for (i = 1; i < 128 && n < 3; ++i) {
        DWORD v = 0;
        __try { v = s[i]; } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        if (IsExeCode(v) && v != ra[n - 1]) ra[n++] = v;
    }
}

static void DiagDraw(void *retSlot, int op, DWORD stride, DWORD nv, DWORD prim, const void *vtx)
{
    DDraw *d;
    if (!g_dumpReq || g_ndd >= DD_N) return;
    d = &g_dd[g_ndd++];
    DiagStack(d->ra, retSlot);
    d->op = (WORD)op;
    d->up0 = g_ddUp;
    d->up1 = g_nu;
    g_ddUp = g_nu;
    d->vs = g_sv[EL_VS][0];
    d->tex0 = g_sv[EL_TEX(0)][0];
    d->stride = stride;
    d->nv = nv;
    d->prim = prim;
    memset(d->v0, 0, sizeof d->v0);
    if (vtx && stride >= 12) memcpy(d->v0, vtx, stride >= 16 ? 16 : 12);
}

static void DiagBuf(void *retSlot, const void *b, DWORD off, DWORD size, const void *bytes)
{
    DBuf *e;
    if (!g_dumpReq || g_ndb >= DB_N) return;
    e = &g_db[g_ndb++];
    DiagStack(e->ra, retSlot);
    e->buf = b;
    e->off = off;
    e->size = size;
    e->draw = g_ndd;
    memset(e->v0, 0, sizeof e->v0);
    if (bytes && size >= 16) memcpy(e->v0, bytes, 16);
}

static void DiagFrameStart(void)
{
    static int keyWas;
    int key;
    g_ndd = g_ndb = g_ddUp = 0;
    if (!g_diag) return;
    key = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F12) & 0x8000);  // Ctrl+F12: dump now
    if (key && !keyWas && !g_dumpReq) {
        g_dumpReq = 2;
        LOG("interp diag: Ctrl+F12: the next 2 recorded frames are written");
    }
    keyWas = key;
    if (++g_dumpCheck >= 15) {
        char f[MAX_PATH];
        g_dumpCheck = 0;
        sprintf_s(f, sizeof f, "%sSH4Refined_dump.flag", g_diagDir);
        if (GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA(f);
            if (!g_dumpReq) g_dumpReq = 1;
        }
    }
}

static float RelChange(const Up *u)
{
    const float *c = (const float *)(g_arena[g_cur].p + u->off), *p = (const float *)(g_arena[g_cur ^ 1].p + u->prevOff);
    float worst = 0.0f;
    int r, l;
    for (r = 0; r < u->n; ++r) {
        float diff = 0.0f, mag = 0.0f;
        for (l = 0; l < 4; ++l) {
            const float d = fabsf(c[r * 4 + l] - p[r * 4 + l]), a = fabsf(c[r * 4 + l]), b = fabsf(p[r * 4 + l]);
            if (d > diff) diff = d;
            if (a > mag) mag = a;
            if (b > mag) mag = b;
        }
        if (mag > 0.0f && diff / mag > worst) worst = diff / mag;
    }
    return worst;
}

static const char *OpName(int op)
{
    switch (op) {
    case OP_DP: return "DP";
    case OP_DIP: return "DIP";
    case OP_DPUP: return "DPUP";
    case OP_DIPUP: return "DIPUP";
    }
    return "?";
}

static void DiagDump(int why, int k)
{
    char f[MAX_PATH];
    FILE *o = NULL;
    int i, j;
    if (!g_dumpReq || !g_rec) return;
    g_dumpReq--;
    sprintf_s(f, sizeof f, "%sSH4Refined_frame_%d.txt", g_diagDir, ++g_dumpN);
    if (fopen_s(&o, f, "w") || !o) return;
    fprintf(o, "frame dump %d: verdict %d (0 interpolated, 1 still, 2 few pairs, 3 cut, 100 unsafe, 101 menu, 102 gap, 103 no timing), "
               "k %d, uploads %d (matched %d, moved %d, changed vec4 %d, jumped vec4 %d), draws %d, buffer writes %d, record %u KB\n",
            g_dumpN, why, k, g_nu, g_pmMatched, g_pmMoved, g_pmChg, g_pmRej, g_ndd, g_ndb, g_arena[g_cur].n >> 10);
    fprintf(o, "draw: #, op, call sites (caller, then 2 more exe return addresses up the stack), vs, tex0, stride, vertices, prims, "
               "first vertex (UP draws), uploads before it: c<reg>x<n>:<S same|M blended|R kept new|N no partner>[relative change]\n");
    for (i = 0, j = 0; i < g_ndd; ++i) {
        const DDraw *d = &g_dd[i];
        int u;
        while (j < g_ndb && g_db[j].draw <= i) {
            const DBuf *b = &g_db[j++];
            fprintf(o, "  buf %p off %u size %u from %06X %06X %06X first %g %g %g %g\n", b->buf, b->off, b->size,
                    b->ra[0], b->ra[1], b->ra[2], b->v0[0], b->v0[1], b->v0[2], b->v0[3]);
        }
        fprintf(o, "%4d %-5s %06X %06X %06X vs %08X tex %08X st %u nv %u pr %u", i, OpName(d->op), d->ra[0], d->ra[1], d->ra[2],
                d->vs, d->tex0, d->stride, d->nv, d->prim);
        if (d->op == OP_DPUP || d->op == OP_DIPUP) fprintf(o, " v0 %g %g %g %g", d->v0[0], d->v0[1], d->v0[2], d->v0[3]);
        for (u = d->up0; u < d->up1 && u < U_MAX; ++u) {
            const Up *p = &g_up[u];
            const char st = p->st == ST_SAME ? 'S' : p->st == ST_MOVE ? 'M' : p->st == ST_REJ ? 'R' : 'N';
            if (p->st == ST_MOVE || p->st == ST_REJ) fprintf(o, " c%ux%u:%c%.3f", p->reg, p->n, st, RelChange(p));
            else fprintf(o, " c%ux%u:%c", p->reg, p->n, st);
        }
        fputc('\n', o);
        for (u = d->up0; u < d->up1 && u < U_MAX; ++u) {  // the values of uploads that were not blended (R, N)
            const Up *p = &g_up[u];
            int r;
            if ((p->st != ST_REJ && p->st != ST_NONE) || p->n > 4) continue;
            for (r = 0; r < p->n; ++r) {
                const float *c = (const float *)(g_arena[g_cur].p + p->off) + r * 4;
                fprintf(o, "        c%u now %10.4f %10.4f %10.4f %10.4f", p->reg + r, c[0], c[1], c[2], c[3]);
                if (p->st == ST_REJ) {
                    const float *q = (const float *)(g_arena[g_cur ^ 1].p + p->prevOff) + r * 4;
                    fprintf(o, "   prev %10.4f %10.4f %10.4f %10.4f", q[0], q[1], q[2], q[3]);
                }
                fputc('\n', o);
            }
        }
    }
    fclose(o);
    LOG("interp diag: frame written to %s", f);
}

typedef struct { unsigned __int64 fs, t0, next, rep, p1s, p1e, p2s, p2e, iv1, iv2; int k, why, interp, sq; } DTick;
static DTick g_dt;

static void DiagTickLine(void)
{
    const double ms = 1000.0 / (double)g_qpf;
    if (g_diag != 1) return;
    if (!g_trace) {
        char f[MAX_PATH];
        sprintf_s(f, sizeof f, "%sSH4Refined_pacing.csv", g_diagDir);
        g_trace = _fsopen(f, "w", 0x40 /* _SH_DENYNO */);
        if (!g_trace) { g_diag = 0; return; }
        fprintf(g_trace, "fs_ms,work_ms,replay_ms,p1_at,p1_dur,p2_at,p2_dur,next_at,k,verdict,interp,squeezed,ival1,ival2\n");
    }
    fprintf(g_trace, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%llu,%llu\n",
            (double)g_dt.fs * ms, (double)(g_dt.t0 - g_dt.fs) * ms, (double)g_dt.rep * ms,
            g_dt.p1s ? (double)(g_dt.p1s - g_dt.fs) * ms : -1.0, g_dt.p1s ? (double)(g_dt.p1e - g_dt.p1s) * ms : -1.0,
            (double)(g_dt.p2s - g_dt.fs) * ms, (double)(g_dt.p2e - g_dt.p2s) * ms, (double)(g_dt.next - g_dt.fs) * ms,
            g_dt.k, g_dt.why, g_dt.interp, g_dt.sq, g_dt.iv1, g_dt.iv2);
    { static int n; if (++n % 30 == 0) fflush(g_trace); }
    memset(&g_dt, 0, sizeof g_dt);
}

void Interp_SetDiag(int on, const char *dir)
{
    g_diag = on;
    strcpy_s(g_diagDir, sizeof g_diagDir, dir ? dir : "");
}
