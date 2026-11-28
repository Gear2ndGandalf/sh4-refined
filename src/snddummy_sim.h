// v3.59 SD359: Refined's silent stand-in sound buffers (staging/v359_feedback/w10_sound/PLAN.md; LOGCHECK_v358.md section 1).
// When the game's DirectSound device is gone (a headset switched off), IDirectSound8::CreateSoundBuffer returns
// DSERR_BUFFERLOST (0x88780096) and leaves the buffer NULL; the game never tests it and reads through it at once (the
// 22:48 crash at 0x40A1E5 in SndPack::PlayMusic). sound.inc's stub at the create sites hands the game one of these instead:
// a COM object of Refined's with the IDirectSoundBuffer8 vtable (and IDirectSound3DBuffer / IDirectSoundFXWavesReverb faces
// for QueryInterface / GetObjectInPath). Every method returns success with the right stdcall argument count; nothing is
// heard. The play cursor moves at the buffer's byte rate while it "plays", so the game's quarter refills (SndPack::Update
// 0x408D50, SndVoice::Service 0x40B780) run on and its sounds end as they would: a slot is freed, a stream reaches its end,
// a wait for a sound to end is not held for ever. Lock hands out one bounded scratch block (0x80000 bytes: the largest
// the game asks for, a whole buffer at its creation); what the game writes there is never read.
// No game memory here: test\snddummy_test.c calls every method through the vtables (and the clock is the test's).
#ifndef SNDDUMMY_SIM_H
#define SNDDUMMY_SIM_H

#include <windows.h>
#include <string.h>

#define SDB_POOL 128                 // stand-ins alive at once (the game holds 60 SndPack slots + 4 stream channels)
#define SDB_SCRATCH 0x80000u         // the Lock block: the game's largest request (a whole 0x80000-byte buffer)
#define SDB_DEF_BYTES 0x80000u
#define SDB_DEF_AVG 176400u          // 44100 Hz, 2 ch, 16-bit: when the format says nothing usable
enum { SDB_REAL, SDB_DUMMY };        // the stub's decision
enum { SDB_SITE_2D, SDB_SITE_3D, SDB_SITE_MUSIC, SDB_SITE_STREAM, SDB_SITE_N };

typedef struct { WORD tag, ch; DWORD rate, avg; WORD align, bits, cb; } SdbWfx;           // WAVEFORMATEX (18 bytes used)
typedef struct { DWORD size, flags, bytes, reserved; const SdbWfx *wfx; GUID alg; } SdbDesc;  // DSBUFFERDESC (0x24)
typedef struct { float x, y, z; } SdbVec;
typedef struct { DWORD size; SdbVec pos, vel; DWORD inAngle, outAngle; SdbVec coneDir; LONG coneVol; float minD, maxD; DWORD mode; } Sdb3D;  // DS3DBUFFER (64)
typedef struct { float inGain, mix, time, hfRatio; } SdbRev;                                // DSFXWavesReverb

struct SdbBuf;
typedef struct { const void *const *vtbl; struct SdbBuf *owner; } SdbFace;
typedef struct SdbBuf {
    const void *const *vtbl;         // IDirectSoundBuffer8 (first: the object pointer the game holds)
    SdbFace f3d, frev;               // IDirectSound3DBuffer, IDirectSoundFXWavesReverb
    volatile LONG refs, inUse;
    int spare;                       // the shared stand-in when the pool is full (never freed)
    DWORD flags, bytes, avg, rate, align;
    SdbWfx wfx;
    volatile LONG playing, looping;
    DWORD pos0, t0;                  // the play cursor at t0 (ms); it moves while playing
    LONG vol, pan;
    DWORD freq;
    Sdb3D d3;
    SdbRev rev;
} SdbBuf;

static SdbBuf g_sdbPool[SDB_POOL];
static SdbBuf g_sdbSpare;
static BYTE g_sdbScratch[SDB_SCRATCH];
static volatile LONG g_sdbLive, g_sdbMade, g_sdbSpareUses, g_sdbFreed;
static DWORD (*g_sdbClock)(void);   // the test's clock; NULL = GetTickCount

static const GUID kSdbIidUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID kSdbIidBuf = { 0x279AFA85, 0x4981, 0x11CE, { 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 } };
static const GUID kSdbIidBuf8 = { 0x6825A449, 0x7524, 0x4D82, { 0x92, 0x0F, 0x50, 0xE3, 0x6A, 0xB3, 0xAB, 0x1E } };   // exe 0x5D22C4
static const GUID kSdbIid3D = { 0x279AFA86, 0x4981, 0x11CE, { 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 } };    // exe 0x5D22A4
static const GUID kSdbIidRev = { 0x46858C3A, 0x0DC6, 0x45E3, { 0xB7, 0x60, 0xD4, 0xEE, 0xF1, 0x6C, 0xB3, 0x25 } };   // exe 0x5D2294

// the stub's rule: the game's own buffer when the call worked and gave one; otherwise a stand-in (a failed call that left a
// pointer is not trusted: it is neither used nor released)
static int SdbDecide(HRESULT hr, const void *buf) { return SUCCEEDED(hr) && buf != NULL ? SDB_REAL : SDB_DUMMY; }
// The create sites' stub (one per site; sound.inc and the test make them from this one text). At the site the 7 bytes
// `call [ecx+0Ch]` + `mov reg,[esp+N]` become `call stub` + 2 nops. Entry: [esp] = the site + 5, [esp+4 .. +10h] = the
// game's four pushed arguments (ds, &desc, &buffer, NULL). fn (HRESULT __stdcall fn(ds, desc, pp, outer, site)) pops its
// five (the four copies and the site); the displaced load reads at N + 14h (the return address and the four arguments are
// still on the stack); ret 10h pops the four as the real call would. eax = fn's result, flags and edx as a call leaves them.
#define SDB_STUB(name, site, reg, off, fn)          \
    static __declspec(naked) void name(void)        \
    {                                               \
        __asm push site                             \
        __asm push dword ptr [esp + 14h]            \
        __asm push dword ptr [esp + 14h]            \
        __asm push dword ptr [esp + 14h]            \
        __asm push dword ptr [esp + 14h]            \
        __asm call fn                               \
        __asm mov reg, dword ptr [esp + off]        \
        __asm ret 10h                               \
    }
// who guards a Play2D / Play3D create site. Playable Eileen v1.22.1+ has its own guard there (SndGuard2D / SndGuard3D: the
// same 7 bytes become a call into its dsound.dll; a failed sound is skipped) and patches in its DllMain, whose order with
// this dll's is the loader's. stock = the site's 7 bytes are the stock ones; intoPe = a call into Playable Eileen's dll;
// pe = Playable Eileen is loaded; first = the first frame (every DllMain has run). Never two hooks on one site.
enum { SDG_OURS, SDG_PE, SDG_WAIT, SDG_OTHER };
static int SdbSiteOwner(int stock, int intoPe, int pe, int first)
{
    if (intoPe) return SDG_PE;
    if (!stock) return SDG_OTHER;  // another mod's bytes: left alone
    if (pe && !first) return SDG_WAIT;  // Playable Eileen may not have patched yet: decided at the first frame
    return SDG_OURS;
}

static DWORD SdbNow(void) { return g_sdbClock ? g_sdbClock() : GetTickCount(); }
static SdbBuf *SdbOf(void *self) { return (SdbBuf *)self; }
static SdbBuf *SdbFaceOwner(void *self) { return ((SdbFace *)self)->owner; }
// the play cursor now (and a stopped non-looping buffer at its end)
static DWORD SdbCursor(SdbBuf *b)
{
    ULONGLONG adv;
    DWORD p;
    if (!b->playing) return b->pos0 % b->bytes;
    adv = (ULONGLONG)(DWORD)(SdbNow() - b->t0) * b->avg / 1000u;
    adv -= adv % b->align;
    if (b->looping) return (DWORD)((b->pos0 + adv) % b->bytes);
    if (b->pos0 + adv >= b->bytes) {
        b->playing = 0;
        b->pos0 = 0;
        return 0;
    }
    p = (DWORD)(b->pos0 + adv);
    return p;
}
static void SdbFormat(SdbBuf *b, const SdbWfx *w)
{
    DWORD align = 4, avg = 0, rate = 44100;
    memset(&b->wfx, 0, sizeof b->wfx);
    if (w) {
        b->wfx = *w;
        b->wfx.cb = 0;
        if (w->align) align = w->align;
        else if (w->ch && w->bits) align = (DWORD)w->ch * w->bits / 8u;
        if (w->rate) rate = w->rate;
        avg = w->avg ? w->avg : rate * align;
    }
    if (!align || align > 64) align = 4;
    if (!avg || avg > 64u * 192000u) avg = SDB_DEF_AVG, rate = 44100;
    b->align = align, b->avg = avg, b->rate = rate;
}

// ---------------------------------------------------------------- IDirectSoundBuffer8 (24 methods)
static ULONG __stdcall SdbAddRef(void *self);
static HRESULT __stdcall SdbQi(void *self, const GUID *iid, void **out)
{
    SdbBuf *const b = SdbOf(self);
    void *o = NULL;
    if (!out) return E_POINTER;
    if (iid && (IsEqualGUID(iid, &kSdbIidUnknown) || IsEqualGUID(iid, &kSdbIidBuf) || IsEqualGUID(iid, &kSdbIidBuf8))) o = b;
    else if (iid && IsEqualGUID(iid, &kSdbIid3D)) o = &b->f3d;
    *out = o;
    if (!o) return E_NOINTERFACE;
    SdbAddRef(b);
    return S_OK;
}
static ULONG __stdcall SdbAddRef(void *self)
{
    SdbBuf *const b = SdbOf(self);
    if (b->spare) return 1;
    return (ULONG)InterlockedIncrement(&b->refs);
}
static ULONG __stdcall SdbRelease(void *self)
{
    SdbBuf *const b = SdbOf(self);
    LONG n;
    if (b->spare) return 1;
    n = InterlockedDecrement(&b->refs);
    if (n == 0) {
        b->playing = 0;
        InterlockedDecrement(&g_sdbLive);
        InterlockedIncrement(&g_sdbFreed);
        InterlockedExchange(&b->inUse, 0);
    }
    return n > 0 ? (ULONG)n : 0;
}
static HRESULT __stdcall SdbGetCaps(void *self, DWORD *caps)  // DSBCAPS {size, flags, bytes, transfer rate, cpu}
{
    SdbBuf *const b = SdbOf(self);
    if (!caps || caps[0] < 20) return E_INVALIDARG;
    caps[1] = b->flags, caps[2] = b->bytes, caps[3] = 0, caps[4] = 0;
    return S_OK;
}
static HRESULT __stdcall SdbGetPos(void *self, DWORD *play, DWORD *write)
{
    SdbBuf *const b = SdbOf(self);
    const DWORD p = SdbCursor(b);
    DWORD lead = b->avg / 100u;  // the write cursor about 10 ms ahead while playing
    lead -= lead % b->align;
    if (!lead) lead = b->align;
    if (play) *play = p;
    if (write) *write = b->playing ? (p + lead) % b->bytes : p;
    return S_OK;
}
static HRESULT __stdcall SdbGetFormat(void *self, SdbWfx *w, DWORD size, DWORD *written)
{
    SdbBuf *const b = SdbOf(self);
    const DWORD n = size < 18u ? size : 18u;
    if (w && n) memcpy(w, &b->wfx, n);
    if (written) *written = w ? n : 18u;
    return S_OK;
}
static HRESULT __stdcall SdbGetVolume(void *self, LONG *v) { if (v) *v = SdbOf(self)->vol; return S_OK; }
static HRESULT __stdcall SdbGetPan(void *self, LONG *p) { if (p) *p = SdbOf(self)->pan; return S_OK; }
static HRESULT __stdcall SdbGetFreq(void *self, DWORD *f) { if (f) *f = SdbOf(self)->freq; return S_OK; }
static HRESULT __stdcall SdbGetStatus(void *self, DWORD *st)
{
    SdbBuf *const b = SdbOf(self);
    (void)SdbCursor(b);  // a non-looping buffer past its end stops here
    if (st) *st = b->playing ? (1u | (b->looping ? 4u : 0u)) : 0u;  // DSBSTATUS_PLAYING, LOOPING
    return S_OK;
}
static HRESULT __stdcall SdbInit(void *self, void *ds, const SdbDesc *d) { (void)self, (void)ds, (void)d; return S_OK; }
static HRESULT __stdcall SdbLock(void *self, DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2, DWORD flags)
{
    SdbBuf *const b = SdbOf(self);
    DWORD a, c;
    if (flags & 2u) off = 0, bytes = b->bytes;  // DSBLOCK_ENTIREBUFFER
    else if (flags & 1u) SdbGetPos(self, NULL, &off);  // DSBLOCK_FROMWRITECURSOR
    off %= b->bytes;
    if (bytes > b->bytes) bytes = b->bytes;
    a = bytes, c = 0;
    if (p2 && off + bytes > b->bytes) a = b->bytes - off, c = bytes - a;  // split as DirectSound does, both parts in the block
    if (a > SDB_SCRATCH) a = SDB_SCRATCH;
    if (c > SDB_SCRATCH - a) c = SDB_SCRATCH - a;
    if (p1) *p1 = g_sdbScratch;
    if (n1) *n1 = a;
    if (p2) *p2 = c ? g_sdbScratch + a : NULL;
    if (n2) *n2 = c;
    return S_OK;
}
static HRESULT __stdcall SdbPlay(void *self, DWORD res, DWORD prio, DWORD flags)
{
    SdbBuf *const b = SdbOf(self);
    (void)res, (void)prio;
    b->pos0 = SdbCursor(b);
    b->t0 = SdbNow();
    b->looping = (flags & 1u) != 0;  // DSBPLAY_LOOPING
    b->playing = 1;
    return S_OK;
}
static HRESULT __stdcall SdbSetPos(void *self, DWORD pos)
{
    SdbBuf *const b = SdbOf(self);
    pos %= b->bytes;
    b->pos0 = pos - pos % b->align;
    b->t0 = SdbNow();
    return S_OK;
}
static HRESULT __stdcall SdbSetFormat(void *self, const SdbWfx *w) { SdbFormat(SdbOf(self), w); return S_OK; }
static HRESULT __stdcall SdbSetVolume(void *self, LONG v) { SdbOf(self)->vol = v; return S_OK; }
static HRESULT __stdcall SdbSetPan(void *self, LONG p) { SdbOf(self)->pan = p; return S_OK; }
static HRESULT __stdcall SdbSetFreq(void *self, DWORD f)
{
    SdbBuf *const b = SdbOf(self);
    b->pos0 = SdbCursor(b);  // the cursor so far at the old rate
    b->t0 = SdbNow();
    b->freq = f ? f : b->rate;  // 0 = DSBFREQUENCY_ORIGINAL
    b->avg = b->freq * b->align;
    if (!b->avg) b->avg = SDB_DEF_AVG;
    return S_OK;
}
static HRESULT __stdcall SdbStop(void *self)
{
    SdbBuf *const b = SdbOf(self);
    b->pos0 = SdbCursor(b);
    b->playing = 0;
    return S_OK;
}
static HRESULT __stdcall SdbUnlock(void *self, void *p1, DWORD n1, void *p2, DWORD n2) { (void)self, (void)p1, (void)n1, (void)p2, (void)n2; return S_OK; }
static HRESULT __stdcall SdbRestore(void *self) { (void)self; return S_OK; }
static HRESULT __stdcall SdbSetFx(void *self, DWORD n, const void *desc, DWORD *res)  // every effect "present" (0): the game
{                                                                                      // then asks for no effect object
    DWORD i;
    (void)self, (void)desc;
    if (res)
        for (i = 0; i < n && i < 16; ++i) res[i] = 0;
    return S_OK;
}
static HRESULT __stdcall SdbAcquire(void *self, DWORD flags, DWORD n, DWORD *res)
{
    DWORD i;
    (void)self, (void)flags;
    if (res)
        for (i = 0; i < n && i < 16; ++i) res[i] = 0;
    return S_OK;
}
static HRESULT __stdcall SdbGetObj(void *self, const GUID *cls, DWORD idx, const GUID *iid, void **out)
{
    SdbBuf *const b = SdbOf(self);
    (void)cls, (void)idx;
    if (!out) return E_POINTER;
    if (iid && IsEqualGUID(iid, &kSdbIidRev)) {
        *out = &b->frev;
        SdbAddRef(b);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static const void *const kSdbVtbl[24] = {
    (const void *)SdbQi, (const void *)SdbAddRef, (const void *)SdbRelease, (const void *)SdbGetCaps, (const void *)SdbGetPos,
    (const void *)SdbGetFormat, (const void *)SdbGetVolume, (const void *)SdbGetPan, (const void *)SdbGetFreq,
    (const void *)SdbGetStatus, (const void *)SdbInit, (const void *)SdbLock, (const void *)SdbPlay, (const void *)SdbSetPos,
    (const void *)SdbSetFormat, (const void *)SdbSetVolume, (const void *)SdbSetPan, (const void *)SdbSetFreq, (const void *)SdbStop,
    (const void *)SdbUnlock, (const void *)SdbRestore, (const void *)SdbSetFx, (const void *)SdbAcquire, (const void *)SdbGetObj };

// ---------------------------------------------------------------- IDirectSound3DBuffer (21 methods; one life with its buffer)
static HRESULT __stdcall Sdb3Qi(void *self, const GUID *iid, void **out) { return SdbQi(SdbFaceOwner(self), iid, out); }
static ULONG __stdcall Sdb3AddRef(void *self) { return SdbAddRef(SdbFaceOwner(self)); }
static ULONG __stdcall Sdb3Release(void *self) { return SdbRelease(SdbFaceOwner(self)); }
static HRESULT __stdcall Sdb3GetAll(void *self, Sdb3D *p)
{
    if (!p || p->size < sizeof(Sdb3D)) return E_INVALIDARG;
    *p = SdbFaceOwner(self)->d3;
    p->size = sizeof(Sdb3D);
    return S_OK;
}
static HRESULT __stdcall Sdb3GetCone(void *self, DWORD *in, DWORD *out)
{
    const Sdb3D *const d = &SdbFaceOwner(self)->d3;
    if (in) *in = d->inAngle;
    if (out) *out = d->outAngle;
    return S_OK;
}
static HRESULT __stdcall Sdb3GetVec(void *self, SdbVec *v, int which)
{
    const Sdb3D *const d = &SdbFaceOwner(self)->d3;
    if (v) *v = which == 0 ? d->coneDir : which == 1 ? d->pos : d->vel;
    return S_OK;
}
static HRESULT __stdcall Sdb3GetConeDir(void *self, SdbVec *v) { return Sdb3GetVec(self, v, 0); }
static HRESULT __stdcall Sdb3GetConeVol(void *self, LONG *v) { if (v) *v = SdbFaceOwner(self)->d3.coneVol; return S_OK; }
static HRESULT __stdcall Sdb3GetMax(void *self, float *v) { if (v) *v = SdbFaceOwner(self)->d3.maxD; return S_OK; }
static HRESULT __stdcall Sdb3GetMin(void *self, float *v) { if (v) *v = SdbFaceOwner(self)->d3.minD; return S_OK; }
static HRESULT __stdcall Sdb3GetMode(void *self, DWORD *v) { if (v) *v = SdbFaceOwner(self)->d3.mode; return S_OK; }
static HRESULT __stdcall Sdb3GetPos(void *self, SdbVec *v) { return Sdb3GetVec(self, v, 1); }
static HRESULT __stdcall Sdb3GetVel(void *self, SdbVec *v) { return Sdb3GetVec(self, v, 2); }
static HRESULT __stdcall Sdb3SetAll(void *self, const Sdb3D *p, DWORD apply)
{
    (void)apply;
    if (!p || p->size < sizeof(Sdb3D)) return E_INVALIDARG;
    SdbFaceOwner(self)->d3 = *p;
    return S_OK;
}
static HRESULT __stdcall Sdb3SetCone(void *self, DWORD in, DWORD out, DWORD apply)
{
    Sdb3D *const d = &SdbFaceOwner(self)->d3;
    (void)apply;
    d->inAngle = in, d->outAngle = out;
    return S_OK;
}
static HRESULT __stdcall Sdb3SetConeDir(void *self, float x, float y, float z, DWORD apply)
{
    Sdb3D *const d = &SdbFaceOwner(self)->d3;
    (void)apply;
    d->coneDir.x = x, d->coneDir.y = y, d->coneDir.z = z;
    return S_OK;
}
static HRESULT __stdcall Sdb3SetConeVol(void *self, LONG v, DWORD apply) { (void)apply; SdbFaceOwner(self)->d3.coneVol = v; return S_OK; }
static HRESULT __stdcall Sdb3SetMax(void *self, float v, DWORD apply) { (void)apply; SdbFaceOwner(self)->d3.maxD = v; return S_OK; }
static HRESULT __stdcall Sdb3SetMin(void *self, float v, DWORD apply) { (void)apply; SdbFaceOwner(self)->d3.minD = v; return S_OK; }
static HRESULT __stdcall Sdb3SetMode(void *self, DWORD v, DWORD apply) { (void)apply; SdbFaceOwner(self)->d3.mode = v; return S_OK; }
static HRESULT __stdcall Sdb3SetPos(void *self, float x, float y, float z, DWORD apply)
{
    Sdb3D *const d = &SdbFaceOwner(self)->d3;
    (void)apply;
    d->pos.x = x, d->pos.y = y, d->pos.z = z;
    return S_OK;
}
static HRESULT __stdcall Sdb3SetVel(void *self, float x, float y, float z, DWORD apply)
{
    Sdb3D *const d = &SdbFaceOwner(self)->d3;
    (void)apply;
    d->vel.x = x, d->vel.y = y, d->vel.z = z;
    return S_OK;
}
static const void *const kSdb3Vtbl[21] = {
    (const void *)Sdb3Qi, (const void *)Sdb3AddRef, (const void *)Sdb3Release, (const void *)Sdb3GetAll, (const void *)Sdb3GetCone,
    (const void *)Sdb3GetConeDir, (const void *)Sdb3GetConeVol, (const void *)Sdb3GetMax, (const void *)Sdb3GetMin,
    (const void *)Sdb3GetMode, (const void *)Sdb3GetPos, (const void *)Sdb3GetVel, (const void *)Sdb3SetAll, (const void *)Sdb3SetCone,
    (const void *)Sdb3SetConeDir, (const void *)Sdb3SetConeVol, (const void *)Sdb3SetMax, (const void *)Sdb3SetMin,
    (const void *)Sdb3SetMode, (const void *)Sdb3SetPos, (const void *)Sdb3SetVel };

// ---------------------------------------------------------------- IDirectSoundFXWavesReverb (5 methods)
static HRESULT __stdcall SdbRQi(void *self, const GUID *iid, void **out)
{
    if (!out) return E_POINTER;
    if (iid && (IsEqualGUID(iid, &kSdbIidRev) || IsEqualGUID(iid, &kSdbIidUnknown))) {
        *out = self;
        SdbAddRef(SdbFaceOwner(self));
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG __stdcall SdbRAddRef(void *self) { return SdbAddRef(SdbFaceOwner(self)); }
static ULONG __stdcall SdbRRelease(void *self) { return SdbRelease(SdbFaceOwner(self)); }
static HRESULT __stdcall SdbRSet(void *self, const SdbRev *p) { if (p) SdbFaceOwner(self)->rev = *p; return S_OK; }
static HRESULT __stdcall SdbRGet(void *self, SdbRev *p) { if (p) *p = SdbFaceOwner(self)->rev; return S_OK; }
static const void *const kSdbRVtbl[5] = { (const void *)SdbRQi, (const void *)SdbRAddRef, (const void *)SdbRRelease, (const void *)SdbRSet,
                                          (const void *)SdbRGet };

// ---------------------------------------------------------------- make one (the stub) / tell one (Refined's own readers)
static void SdbSetUp(SdbBuf *b, const SdbDesc *d)
{
    b->vtbl = kSdbVtbl;
    b->f3d.vtbl = kSdb3Vtbl, b->f3d.owner = b;
    b->frev.vtbl = kSdbRVtbl, b->frev.owner = b;
    b->flags = d ? d->flags : 0;
    b->bytes = d && d->bytes >= 4u && d->bytes <= 0x1000000u ? d->bytes : SDB_DEF_BYTES;
    SdbFormat(b, d ? d->wfx : NULL);
    b->bytes -= b->bytes % b->align;
    if (!b->bytes) b->bytes = SDB_DEF_BYTES;
    b->playing = 0, b->looping = 0, b->pos0 = 0, b->t0 = SdbNow();
    b->vol = 0, b->pan = 0, b->freq = b->rate;
    memset(&b->d3, 0, sizeof b->d3);
    b->d3.size = sizeof(Sdb3D), b->d3.inAngle = b->d3.outAngle = 360u, b->d3.minD = 1.0f, b->d3.maxD = 1.0e9f;
    memset(&b->rev, 0, sizeof b->rev);
}
// a stand-in for one CreateSoundBuffer (one reference, as the game's own); the shared spare when every one is in use
static SdbBuf *SdbMake(const SdbDesc *d)
{
    int i;
    for (i = 0; i < SDB_POOL; ++i)
        if (InterlockedCompareExchange(&g_sdbPool[i].inUse, 1, 0) == 0) {
            SdbBuf *const b = &g_sdbPool[i];
            SdbSetUp(b, d);
            b->spare = 0;
            InterlockedExchange(&b->refs, 1);
            InterlockedIncrement(&g_sdbLive);
            InterlockedIncrement(&g_sdbMade);
            return b;
        }
    if (!g_sdbSpare.vtbl) SdbSetUp(&g_sdbSpare, d);
    g_sdbSpare.spare = 1;
    g_sdbSpare.refs = 1;
    InterlockedIncrement(&g_sdbSpareUses);
    return &g_sdbSpare;
}
// 1 = p is one of the stand-ins (the buffer, its 3D or reverb face): Refined's readers of the game's buffers skip it
static int SdbIs(const void *p)
{
    const BYTE *const q = (const BYTE *)p;
    return p && ((q >= (const BYTE *)g_sdbPool && q < (const BYTE *)(g_sdbPool + SDB_POOL)) ||
                 (q >= (const BYTE *)&g_sdbSpare && q < (const BYTE *)(&g_sdbSpare + 1)));
}

#endif
