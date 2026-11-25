// Native test of the FRAME INTERPOLATION module (interp.c) against a fake IDirect3DDevice8: pairing and blending,
// replay order and state restore, UP data copies, dynamic buffer preludes, the safety fallbacks, reference counts.
// Build: test\build_test.cmd (x86). Run: out_test\test_interp.exe (exit code = number of failures).
#define INTERP_TEST
#include "../src/interp.c"
#include <stdio.h>

static int g_fail, g_checks;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEARF(a, b) (fabsf((a) - (b)) < 1e-4f)

// ---------------------------------------------------------------- fake resources
typedef struct FObj { void **vt; LONG ref; int kind; BYTE data[256]; DWORD size; float content; struct FObj *surf; } FObj;
enum { K_VB = 1, K_IB, K_TEX, K_SURF };
static FObj g_pool[64];
static int g_npool;
static void *vtVB[16], *vtTex[20], *vtSurf[12];

static ULONG STDMETHODCALLTYPE fAddRef(FObj *o) { return (ULONG)++o->ref; }
static ULONG STDMETHODCALLTYPE fRelease(FObj *o) { return (ULONG)--o->ref; }
static HRESULT STDMETHODCALLTYPE fLock(FObj *o, DWORD off, DWORD size, void **pp, DWORD flags) { (void)size; (void)flags; *pp = o->data + off; return S_OK; }
static HRESULT STDMETHODCALLTYPE fUnlock(FObj *o) { (void)o; return S_OK; }
static HRESULT STDMETHODCALLTYPE fBufDesc(FObj *o, DWORD *d) { memset(d, 0, 20); d[4] = o->size; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetSurf(FObj *o, DWORD lv, FObj **out) { (void)lv; *out = o->surf; o->surf->ref++; return S_OK; }
static int g_texLocks;
static HRESULT STDMETHODCALLTYPE fTexLock(FObj *o, DWORD lv, void *lr, const RECT *r, DWORD fl) { (void)o; (void)lv; (void)lr; (void)r; (void)fl; g_texLocks++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSurfDesc(FObj *o, DWORD *d) { (void)o; memset(d, 0, 32); d[0] = 21; d[6] = 640; d[7] = 480; return S_OK; }

static FObj *NewObj(int kind, DWORD size)
{
    FObj *o = &g_pool[g_npool++];
    memset(o, 0, sizeof *o);
    o->kind = kind;
    o->ref = 1;
    o->size = size;
    o->vt = kind == K_TEX ? vtTex : kind == K_SURF ? vtSurf : vtVB;
    if (kind == K_TEX) o->surf = NewObj(K_SURF, 0), o->surf->ref = 0;  // the level surface lives with the texture
    return o;
}

// ---------------------------------------------------------------- fake device
static void *dvt[97];
static struct { void **vt; } g_fd = { dvt };
static DWORD f_rs[256], f_vs, f_ps;
static float f_vsc[256][4];
static FObj *f_tex[8], *f_vb[16], *f_ib, *f_rt, *f_ds, *f_bb;
static DWORD f_stride[16], f_vp[6];
typedef struct { float c7x; DWORD rs7; int vb0; float up0; } Draw;
static Draw f_draws[256];
static int f_nd;
static float f_pres[64];
static int f_np, f_copies, f_resets;

static void Drawn(float c7x, float up0)
{
    if (f_nd < 256) {
        f_draws[f_nd].c7x = c7x;
        f_draws[f_nd].rs7 = f_rs[7];
        f_draws[f_nd].vb0 = f_vb[0] ? f_vb[0]->data[0] : -1;
        f_draws[f_nd].up0 = up0;
        f_nd++;
    }
    if (f_rt) f_rt->content = c7x + up0;
}
#define D(d) (void)(d)
static HRESULT STDMETHODCALLTYPE fReset(void *d, void *pp) { D(d); (void)pp; f_resets++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fPresent(void *d, const RECT *a, const RECT *b, HWND w, const void *r)
{ D(d); (void)a; (void)b; (void)w; (void)r; f_pres[f_np % 64] = f_bb->content; f_np++;  /* ring */ return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetBB(void *d, DWORD i, DWORD t, FObj **out) { D(d); (void)i; (void)t; *out = f_bb; f_bb->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fCreateTex(void *d, DWORD w, DWORD h, DWORD l, DWORD u, DWORD f, DWORD p, FObj **out)
{ D(d); (void)w; (void)h; (void)l; (void)u; (void)f; (void)p; *out = NewObj(K_TEX, 0); return S_OK; }
static HRESULT STDMETHODCALLTYPE fCreateVB(void *d, DWORD len, DWORD u, DWORD f, DWORD p, FObj **out)
{ D(d); (void)u; (void)f; (void)p; *out = NewObj(K_VB, len > 256 ? 256 : len); return S_OK; }
static int f_failCopyToBB, f_crashDraw, f_inReplay;
static HRESULT STDMETHODCALLTYPE fCopyRects(void *d, FObj *s, const RECT *r, DWORD n, FObj *t, const POINT *pt)
{ D(d); (void)r; (void)n; (void)pt; if (f_failCopyToBB && t == f_bb) return E_FAIL; t->content = s->content; f_copies++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetRT(void *d, FObj *rt, FObj *ds) { D(d); if (rt) f_rt = rt; f_ds = ds; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetRT(void *d, FObj **o) { D(d); *o = f_rt; f_rt->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetDS(void *d, FObj **o) { D(d); *o = f_ds; if (!f_ds) return 0x8876086Cu; f_ds->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fScene(void *d) { D(d); return S_OK; }
static HRESULT STDMETHODCALLTYPE fClear(void *d, DWORD n, const void *r, DWORD f, DWORD c, float z, DWORD s)
{ D(d); (void)n; (void)r; (void)f; (void)c; (void)z; (void)s; if (f_rt) f_rt->content = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetVP(void *d, const DWORD *v) { D(d); memcpy(f_vp, v, 24); return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetVP(void *d, DWORD *v) { D(d); memcpy(v, f_vp, 24); return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetRS(void *d, DWORD s, DWORD v) { D(d); f_rs[s & 255] = v; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetRS(void *d, DWORD s, DWORD *v) { D(d); *v = f_rs[s & 255]; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetTex(void *d, DWORD s, FObj *t) { D(d); f_tex[s & 7] = t; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetTex(void *d, DWORD s, FObj **t) { D(d); *t = f_tex[s & 7]; if (*t) (*t)->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fTSS(void *d, DWORD s, DWORD t, DWORD v) { D(d); (void)s; (void)t; (void)v; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetTSS(void *d, DWORD s, DWORD t, DWORD *v) { D(d); (void)s; (void)t; *v = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE fDP(void *d, DWORD t, DWORD s, DWORD c) { D(d); (void)t; (void)s; (void)c; Drawn(f_vsc[7][0], 0); return S_OK; }
static HRESULT STDMETHODCALLTYPE fDIP(void *d, DWORD t, DWORD a, DWORD b, DWORD c, DWORD e)
{ D(d); (void)t; (void)a; (void)b; (void)c; (void)e; if (f_crashDraw && g_inOwn) *(volatile int *)0 = 1; Drawn(f_vsc[7][0], 0); return S_OK; }
static HRESULT STDMETHODCALLTYPE fDPUP(void *d, DWORD t, DWORD p, const float *v, DWORD s)
{ D(d); (void)t; (void)p; (void)s; f_vb[0] = NULL; Drawn(f_vsc[7][0], v[0]); return S_OK; }
static HRESULT STDMETHODCALLTYPE fDIPUP(void *d, DWORD t, DWORD a, DWORD b, DWORD p, const void *i, DWORD f, const float *v, DWORD s)
{ D(d); (void)t; (void)a; (void)b; (void)p; (void)i; (void)f; (void)s; f_vb[0] = NULL; f_ib = NULL; Drawn(f_vsc[7][0], v[0]); return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetVS(void *d, DWORD h) { D(d); f_vs = h; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetVS(void *d, DWORD *h) { D(d); *h = f_vs; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetPS(void *d, DWORD h) { D(d); f_ps = h; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetPS(void *d, DWORD *h) { D(d); *h = f_ps; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetVSC(void *d, DWORD r, const float *v, DWORD n) { D(d); memcpy(f_vsc[r], v, n * 16); return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetVSC(void *d, DWORD r, float *v, DWORD n) { D(d); memcpy(v, f_vsc[r], n * 16); return S_OK; }
static HRESULT STDMETHODCALLTYPE fPSC(void *d, DWORD r, const float *v, DWORD n) { D(d); (void)r; (void)v; (void)n; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetStream(void *d, DWORD n, FObj *vb, DWORD s) { D(d); f_vb[n & 15] = vb; f_stride[n & 15] = s; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetStream(void *d, DWORD n, FObj **vb, DWORD *s) { D(d); *vb = f_vb[n & 15]; *s = f_stride[n & 15]; if (*vb) (*vb)->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE fSetIdx(void *d, FObj *ib, DWORD b) { D(d); (void)b; f_ib = ib; return S_OK; }
static HRESULT STDMETHODCALLTYPE fGetIdx(void *d, FObj **ib, DWORD *b) { D(d); *ib = f_ib; *b = 0; if (f_ib) f_ib->ref++; return S_OK; }
static HRESULT STDMETHODCALLTYPE f1(void *d, DWORD a) { D(d); (void)a; return S_OK; }
static HRESULT STDMETHODCALLTYPE f2(void *d, DWORD a, DWORD b) { D(d); (void)a; (void)b; return S_OK; }
static HRESULT STDMETHODCALLTYPE f0(void *d) { D(d); return S_OK; }
static int f_lost;
static HRESULT STDMETHODCALLTYPE fTestCoop(void *d) { D(d); return f_lost ? (HRESULT)0x88760868L : S_OK; }
static HRESULT STDMETHODCALLTYPE f3(void *d, DWORD a, DWORD b, DWORD c) { D(d); (void)a; (void)b; (void)c; return S_OK; }
static HRESULT STDMETHODCALLTYPE f5(void *d, DWORD a, DWORD b, DWORD c, DWORD e, DWORD f) { D(d); (void)a; (void)b; (void)c; (void)e; (void)f; return S_OK; }

static void SetupFake(void)
{
    static const int k1[] = { 30, 42, 53, 54, 55, 58, 68, 78, 90, 96 }, k2[] = { 29, 37, 39, 44, 46, 48, 57, 66 }, k3[] = { 94, 95 };
    int i;
    vtVB[1] = vtTex[1] = vtSurf[1] = (void *)fAddRef;
    vtVB[2] = vtTex[2] = vtSurf[2] = (void *)fRelease;
    vtVB[11] = (void *)fLock; vtVB[12] = (void *)fUnlock; vtVB[13] = (void *)fBufDesc;
    vtTex[15] = (void *)fGetSurf; vtTex[16] = (void *)fTexLock;
    vtSurf[8] = (void *)fSurfDesc;
    for (i = 0; i < 10; ++i) dvt[k1[i]] = (void *)f1;
    for (i = 0; i < 8; ++i) dvt[k2[i]] = (void *)f2;
    for (i = 0; i < 2; ++i) dvt[k3[i]] = (void *)f3;
    dvt[52] = (void *)f0; dvt[74] = (void *)f5; dvt[3] = (void *)fTestCoop;
    dvt[14] = (void *)fReset; dvt[15] = (void *)fPresent; dvt[16] = (void *)fGetBB; dvt[20] = (void *)fCreateTex;
    dvt[23] = dvt[24] = (void *)fCreateVB; dvt[28] = (void *)fCopyRects; dvt[31] = (void *)fSetRT; dvt[32] = (void *)fGetRT;
    dvt[33] = (void *)fGetDS; dvt[34] = dvt[35] = (void *)fScene; dvt[36] = (void *)fClear; dvt[40] = (void *)fSetVP;
    dvt[41] = (void *)fGetVP; dvt[50] = (void *)fSetRS; dvt[51] = (void *)fGetRS; dvt[60] = (void *)fGetTex;
    dvt[61] = (void *)fSetTex; dvt[62] = (void *)fGetTSS; dvt[63] = (void *)fTSS; dvt[70] = (void *)fDP; dvt[71] = (void *)fDIP;
    dvt[72] = (void *)fDPUP; dvt[73] = (void *)fDIPUP; dvt[76] = (void *)fSetVS; dvt[77] = (void *)fGetVS;
    dvt[79] = (void *)fSetVSC; dvt[80] = (void *)fGetVSC; dvt[83] = (void *)fSetStream; dvt[84] = (void *)fGetStream;
    dvt[85] = (void *)fSetIdx; dvt[86] = (void *)fGetIdx; dvt[88] = (void *)fSetPS; dvt[89] = (void *)fGetPS;
    dvt[91] = (void *)fPSC; dvt[92] = (void *)fPSC;
    f_bb = NewObj(K_SURF, 0);
    f_rt = f_bb;
    f_vp[2] = 640; f_vp[3] = 480;
}

// calls through the (hooked) vtable, as the game does
#define DEV ((void *)&g_fd)
static void SetVSC7(float x) { float v[4] = { x, 2.0f, 3.0f, 1.0f }; ((F3)dvt[79])(DEV, 7, P(v), 1); }
static void Present(void) { ((HRESULT(STDMETHODCALLTYPE *)(void *, const RECT *, const RECT *, HWND, const void *))dvt[15])(DEV, NULL, NULL, NULL, NULL); }

// host
static int h_gameplay = 1;
static unsigned h_tick;
static void HLog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("    log: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}
static int HGameplay(void) { return h_gameplay; }
static unsigned HTick(void) { return h_tick; }
// a fake game clock (QPC units): this tick's start and step; off unless h_timing (the earlier tests run without timing)
static int h_timing;
static unsigned __int64 h_fs, h_step;
static int HTiming(unsigned __int64 *fs, unsigned __int64 *next, unsigned __int64 *step)
{
    if (!h_timing) return 0;
    *fs = h_fs;
    *step = h_step;
    *next = h_fs + h_step;
    return 1;
}

static FObj *g_vbA, *g_tex0;

#define CALL0(i) ((F0)dvt[i])(DEV)
#define CALLN(i, T, ...) ((T)dvt[i])(DEV, __VA_ARGS__)
static void Clear(void) { ((HRESULT(STDMETHODCALLTYPE *)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD))dvt[36])(DEV, 0, NULL, 7, 0, 1.0f, 0); }
static void ModelDraw(float x)
{
    CALLN(83, F3, 0, P(g_vbA), 64);
    CALLN(85, F2, P(g_vbA), 0);
    CALLN(61, F2, 0, P(g_tex0));
    CALLN(76, F1, 0x10000001);
    SetVSC7(x);
    CALLN(71, F5, 4, 0, 4, 0, 2);
}
static void Frame1(float x)  // one model draw
{
    h_tick++;
    Clear();
    CALL0(34);
    ModelDraw(x);
    CALL0(35);
    Present();
}

static float LastPresents(int n, int i) { return f_pres[(f_np - n + i) % 64]; }

int main(void)
{
    InterpHost host = { HLog, HGameplay, HTiming, HTick };
    int np0, nd0, i;
    SetupFake();
    // ---- 1. the blend rule
    {
        float c[8] = { 11, 2, 3, 1, 5, 5, 5, 5 }, p[8] = { 10, 2, 3, 1, 5, 5, 5, 5 }, o[8];
        int chg = 0, rej = 0;
        CHECK(Classify(c, p, 2, &chg, &rej) == ST_MOVE && chg == 1 && rej == 0, "small move: MOVE, 1 changed vec4 (%d, %d)", chg, rej);
        Blend(o, c, p, 2, 0.25f);
        CHECK(NEARF(o[0], 10.25f) && NEARF(o[4], 5.0f), "blend at 1/4: %f %f", o[0], o[4]);
        c[0] = 100;
        chg = rej = 0;
        CHECK(Classify(c, p, 2, &chg, &rej) == ST_REJ && rej == 1, "jump 10 -> 100: rejected");
        c[0] = 10;
        chg = rej = 0;
        CHECK(Classify(c, p, 2, &chg, &rej) == ST_SAME && chg == 0, "no change: SAME");
        c[1] = 0.0f; p[1] = 0.0f; c[0] = 0.0f; p[0] = 0.0f; c[2] = 0.0f; p[2] = 0.0f; c[3] = 0.0f; p[3] = 0.00005f;
        chg = rej = 0;
        CHECK(Classify(c, p, 1, &chg, &rej) == ST_MOVE, "tiny values: 1e-4 floor (not a jump)");
        c[3] = 0.0f; p[3] = 1.0f;
        chg = rej = 0;
        CHECK(Classify(c, p, 1, &chg, &rej) == ST_REJ, "0 -> 1 is a jump");
        { float nan = sqrtf(-1.0f); c[0] = nan; chg = rej = 0; CHECK(Classify(c, p, 1, &chg, &rej) == ST_REJ, "NaN: rejected"); }
    }
    // ---- attach (rate 60; tests run without waits and without the adaptive fallback)
    Interp_TestSet(1, 0, 0);
    Interp_Init(&host, 60, 0);
    Interp_FrameEnd(DEV);
    CHECK(dvt[15] == (void *)hkPresent && dvt[79] == (void *)hkSetVSC, "device methods hooked");
    CHECK(vtVB[11] == (void *)hkBufLock && vtTex[16] == (void *)hkTexLock, "buffer / texture lock methods hooked");
    {
        FObj *o = NULL;
        CALLN(23, F5, 64, 0, 0, 1, P(&o));
        g_vbA = o;
        CALLN(20, F7, 4, 4, 1, 0, 21, 1, P(&o));
        g_tex0 = o;
    }
    // ---- 2. 60 fps: one in-between frame at 1/2
    Frame1(10.0f);  // first present after attaching: plain
    Frame1(10.0f);  // no previous frame yet: shown once
    np0 = f_np; nd0 = f_nd;
    Frame1(11.0f);  // 10 -> 11: one in-between frame at 10.5, then the real one
    CHECK(f_np - np0 == 2, "60: two presents in the tick (%d)", f_np - np0);
    CHECK(f_nd - nd0 == 2 && NEARF(f_draws[nd0].c7x, 11.0f) && NEARF(f_draws[nd0 + 1].c7x, 10.5f), "60: real draw 11 then replay at 10.5 (%d draws: %f %f)",
          f_nd - nd0, f_draws[nd0].c7x, f_draws[nd0 + 1].c7x);
    CHECK(f_np - np0 == 2 && NEARF(LastPresents(2, 0), 10.5f) && NEARF(LastPresents(2, 1), 11.0f), "60: shown 10.5 then 11 (%f %f)", LastPresents(2, 0), LastPresents(2, 1));
    CHECK(NEARF(f_vsc[7][0], 11.0f), "60: c7 is the real value after the replay (%f)", f_vsc[7][0]);
    // ---- 3. 120 fps: 1/4, 2/4, 3/4
    Interp_SetRate(120);
    np0 = f_np; nd0 = f_nd;
    Frame1(12.0f);
    CHECK(f_np - np0 == 4, "120: four presents (%d)", f_np - np0);
    CHECK(f_np - np0 == 4 && NEARF(LastPresents(4, 0), 11.25f) && NEARF(LastPresents(4, 1), 11.5f) && NEARF(LastPresents(4, 2), 11.75f) &&
          NEARF(LastPresents(4, 3), 12.0f), "120: shown 11.25 11.5 11.75 12 (%f %f %f %f)", LastPresents(4, 0), LastPresents(4, 1),
          LastPresents(4, 2), LastPresents(4, 3));
    Interp_SetRate(60);
    Frame1(12.0f);
    // ---- 4. a jump (camera cut): shown once
    np0 = f_np;
    Frame1(200.0f);
    CHECK(f_np - np0 == 1, "cut: one present (%d)", f_np - np0);
    // ---- 5. nothing moves: shown once
    np0 = f_np;
    Frame1(200.0f);
    CHECK(f_np - np0 == 1, "still: one present (%d)", f_np - np0);
    // ---- 6. state at the replay's start and end (S0 / S1): render state 7 changes mid-frame
    for (i = 0; i < 3; ++i) {
        h_tick++;
        Clear();
        CALL0(34);
        ModelDraw(300.0f + (float)i);
        CALLN(50, F2, 7, 100 + i);
        ModelDraw(300.0f + (float)i);
        CALL0(35);
        nd0 = f_nd;
        np0 = f_np;
        Present();
    }
    CHECK(f_np - np0 == 2 && f_nd - nd0 == 2, "S0: interpolated (%d presents, %d replay draws)", f_np - np0, f_nd - nd0);
    CHECK(f_draws[nd0].rs7 == 101 && f_draws[nd0 + 1].rs7 == 102, "S0: the replay starts with RS7 = 101, then 102 (%lu %lu)",
          f_draws[nd0].rs7, f_draws[nd0 + 1].rs7);
    CHECK(f_rs[7] == 102, "S1: RS7 = 102 after the replay (%lu)", f_rs[7]);
    // ---- 7. UP vertices are copied when recorded
    for (i = 0; i < 3; ++i) {
        float v[12] = { 0 };
        v[0] = 7.0f + (float)i;
        h_tick++;
        Clear();
        CALL0(34);
        SetVSC7(400.0f + (float)i);
        CALLN(72, F4, 4, 1, P(v), 16);
        v[0] = -999.0f;  // the caller reuses its buffer
        CALL0(35);
        nd0 = f_nd;
        np0 = f_np;
        Present();
    }
    CHECK(f_np - np0 == 2 && f_nd - nd0 == 1 && NEARF(f_draws[nd0].up0, 9.0f), "UP: the replay draws the copied vertices (%f)", f_draws[nd0].up0);
    // ---- 8. a vertex buffer drawn, then rewritten, then drawn again in one frame: prelude + write
    for (i = 0; i < 3; ++i) {
        void *p = NULL;
        h_tick++;
        Clear();
        CALL0(34);
        ModelDraw(500.0f + (float)i);  // draws with the bytes of the frame before
        ((Lock_t)vtVB[11])(g_vbA, 0, 0, &p, 0);
        ((BYTE *)p)[0] = (BYTE)(50 + i);
        ((F0)vtVB[12])(g_vbA);
        ModelDraw(500.0f + (float)i);
        CALL0(35);
        nd0 = f_nd;
        np0 = f_np;
        Present();
    }
    CHECK(f_np - np0 == 2 && f_nd - nd0 == 2 && f_draws[nd0].vb0 == 51 && f_draws[nd0 + 1].vb0 == 52,
          "buffer: replay sees the old bytes then the new (%d %d)", f_draws[nd0].vb0, f_draws[nd0 + 1].vb0);
    CHECK(g_vbA->data[0] == 52, "buffer: the new bytes after the replay (%d)", g_vbA->data[0]);
    // ---- 9. a method the replay cannot reproduce (SetTransform): shown once
    Frame1(600.0f);
    h_tick++;
    Clear(); CALL0(34); ModelDraw(601.0f);
    { float m[16] = { 0 }; CALLN(37, F2, 2, P(m)); }
    CALL0(35);
    np0 = f_np;
    Present();
    CHECK(f_np - np0 == 1, "SetTransform: one present (%d)", f_np - np0);
    Frame1(602.0f);
    np0 = f_np;
    Frame1(603.0f);
    CHECK(f_np - np0 == 2, "after it: interpolated again (%d)", f_np - np0);
    // ---- 10. a texture written after it was used in the frame: shown once
    h_tick++;
    Clear(); CALL0(34); ModelDraw(604.0f);
    { char lr[8]; ((TexLock_t)vtTex[16])(g_tex0, 0, lr, NULL, 0); }
    CALL0(35);
    np0 = f_np;
    Present();
    CHECK(f_np - np0 == 1 && g_texLocks == 1, "texture lock after use: one present (%d)", f_np - np0);
    // ---- 11. record over the cap: shown once
    Frame1(605.0f);
    Interp_TestSet(1, 0, 64);
    np0 = f_np;
    Frame1(606.0f);
    CHECK(f_np - np0 == 1, "too big: one present (%d)", f_np - np0);
    Interp_TestSet(1, 0, 32u << 20);
    // ---- 12. menus: not recorded, shown once
    h_gameplay = 0;
    np0 = f_np;
    Frame1(700.0f);
    Frame1(701.0f);
    CHECK(f_np - np0 == 2, "menu: one present per tick (%d)", f_np - np0);
    h_gameplay = 1;
    Frame1(702.0f);
    np0 = f_np;
    Frame1(703.0f);
    Frame1(704.0f);
    CHECK(f_np - np0 == 3, "back in gameplay: first frame once, then interpolated (%d)", f_np - np0);
    // ---- 13. a skipped tick: shown once
    h_tick++;
    np0 = f_np;
    Frame1(705.0f);
    CHECK(f_np - np0 == 1, "tick gap: one present (%d)", f_np - np0);
    // ---- 14. device reset mid-frame
    Frame1(706.0f);
    Clear();
    CALLN(14, F1, 0);
    np0 = f_np;
    Frame1(707.0f);  // (the Present ends the frame the reset cut)
    Frame1(708.0f);
    CHECK(f_resets == 1 && f_np - np0 == 3, "reset: frames continue, interpolation resumes (%d)", f_np - np0);
    // ---- 15. 30: no recording, one present
    Interp_SetRate(30);
    np0 = f_np;
    Frame1(709.0f);
    Frame1(710.0f);
    CHECK(f_np - np0 == 2, "30: one present per tick (%d)", f_np - np0);
    // ---- 19-21. auto-lowering on a fake game clock (presents are not waited for: g_noWait)
    {
        LARGE_INTEGER q;
        float x = 1000.0f;
        int lowered = 0, back = 0;
        QueryPerformanceCounter(&q);
        h_step = g_qpf / 30;
        h_fs = (unsigned __int64)q.QuadPart - 6000 * h_step;  // stays behind the real clock for the whole test
        h_timing = 1;
        Interp_TestSet(1, 1, 0);
        Interp_SetRate(60);
#define TICK(spanSteps) (h_fs += (unsigned __int64)((double)h_step * (spanSteps)), Frame1(x += 0.1f))
        // 19. a load hitch (20 steps), slow first ticks (1.6 steps) while warming up, then steady: must stay at 60
        for (i = 0; i < 5; ++i) TICK(1.0);
        TICK(20.0);
        for (i = 0; i < 30; ++i) TICK(1.6);
        for (i = 0; i < 400; ++i) {
            TICK(1.0);
            if (Interp_RateInForce() != 60) lowered = 1;
        }
        np0 = f_np;
        TICK(1.0);
        CHECK(!lowered && f_np - np0 == 2, "hitch, then steady: stays at 60 and interpolates (%d presents)", f_np - np0);
        // 20. ticks stretched by 20 % for good: lowered to 30; a minute of play later back to 60
        for (i = 0; i < 400 && Interp_RateInForce() == 60; ++i) TICK(1.2);
        CHECK(Interp_RateInForce() == 30, "sustained stretch: lowered to 30 (after %d ticks)", i);
        np0 = f_np;
        TICK(1.0);
        CHECK(f_np - np0 == 1, "lowered: one present per tick (%d)", f_np - np0);
        for (i = 0; i < 1900 && !back; ++i) {
            TICK(1.0);
            if (Interp_RateInForce() == 60) back = i + 1;
        }
        CHECK(back >= 1790 && back <= 1810, "lowered: tried again at 60 after about 1800 ticks (%d)", back);
        TICK(1.0);  // the first tick at 60 again has no previous frame
        np0 = f_np;
        TICK(1.0);
        CHECK(f_np - np0 == 2, "retried: interpolating at 60 again (%d presents)", f_np - np0);
        // 21. InterpAutoLower=0: the same stretch never lowers
        Interp_TestSet(1, 0, 0);
        for (i = 0; i < 600; ++i) TICK(1.2);
        CHECK(Interp_RateInForce() == 60, "InterpAutoLower=0: stays at 60 (%d)", Interp_RateInForce());
#undef TICK
        h_timing = 0;
    }
    // ---- 22 (review v2.1). a hitch tick (began 10 steps ago) must not push the pacing phase past the tick
    {
        LARGE_INTEGER q;
        float x = 2000.0f;
        const unsigned __int64 phi0 = g_phi;
        h_step = g_qpf / 30;
        h_timing = 1;
        Interp_TestSet(1, 0, 0);
        Interp_SetRate(60);
        CHECK(phi0 < h_step, "phase after the clock-behind tests stays inside a tick (%.1f steps)", (double)phi0 / (double)h_step);
        for (i = 0; i < 5; ++i) { QueryPerformanceCounter(&q); h_fs = (unsigned __int64)q.QuadPart; Frame1(x += 0.1f); }
        QueryPerformanceCounter(&q); h_fs = (unsigned __int64)q.QuadPart - 10 * h_step; Frame1(x += 0.1f);
        QueryPerformanceCounter(&q); h_fs = (unsigned __int64)q.QuadPart; Frame1(x += 0.1f);
        CHECK(g_phi < h_step, "hitch: the pacing phase stays inside a tick (%.2f steps)", (double)g_phi / (double)h_step);
        h_timing = 0;
    }
    // ---- 23 (review v2.1). a lost device: shown once, interpolation stays on
    Interp_SetRate(60);
    Frame1(790.0f);
    Frame1(790.0f);
    f_lost = 1;
    np0 = f_np;
    Frame1(791.0f);
    CHECK(f_np - np0 == 1 && !g_disabled, "lost device: one present, not disabled (%d)", f_np - np0);
    f_lost = 0;
    np0 = f_np;
    Frame1(792.0f);
    CHECK(f_np - np0 == 2, "device back: interpolating again (%d)", f_np - np0);
    // ---- 17. the real frame cannot be copied back: it is replayed at the real values instead
    Interp_SetRate(60);
    Frame1(800.0f);
    Frame1(800.0f);
    f_failCopyToBB = 1;
    np0 = f_np;
    Frame1(801.0f);
    CHECK(f_np - np0 == 2 && NEARF(LastPresents(2, 0), 800.5f) && NEARF(LastPresents(2, 1), 801.0f), "copy back fails: 800.5 then a replayed 801 (%f %f)",
          LastPresents(2, 0), LastPresents(2, 1));
    np0 = f_np;
    Frame1(802.0f);
    CHECK(f_np - np0 == 2 && NEARF(LastPresents(2, 1), 802.0f), "copy back fails: next tick fine too (%f)", LastPresents(2, 1));
    f_failCopyToBB = 0;
    // ---- 18. an exception inside a replay: the real frame is shown and interpolation is off for the session
    f_crashDraw = 1;
    np0 = f_np;
    Frame1(803.0f);
    CHECK(f_np - np0 == 1, "exception: one present (%d)", f_np - np0);
    f_crashDraw = 0;
    np0 = f_np;
    Frame1(804.0f);
    Frame1(805.0f);
    CHECK(f_np - np0 == 2 && Interp_RateInForce() == 30, "after the exception: 30, one present per tick (%d)", f_np - np0);
    // ---- 16. references: everything this module took is given back
    {
        int bad = 0;
        for (i = 0; i < g_npool; ++i) {
            const LONG want = g_pool[i].kind == K_SURF ? (&g_pool[i] == f_bb ? 1 : g_pool[i].ref) : 1;
            if (g_pool[i].kind != K_SURF && g_pool[i].ref != want && &g_pool[i] != g_vbA && &g_pool[i] != g_tex0 && g_pool[i].ref != 0) bad++;
        }
        CHECK(g_vbA->ref == 1 && g_tex0->ref == 1 && f_bb->ref == 1, "references balanced (vb %ld, tex %ld, back buffer %ld)", g_vbA->ref,
              g_tex0->ref, f_bb->ref);
        CHECK(!bad, "%d other objects keep references", bad);
    }
    printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail;
}
