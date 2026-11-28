// v3.59 SD359: native test of src\snddummy_sim.h (test\build_snddummy_test.cmd): the create stub's decision, the site owner
// rule (Playable Eileen's own guard at Play2D / Play3D), every method of the stand-in buffer, its 3D face and its reverb face
// called through their vtables with their stdcall argument counts (esp checked around each call), the moving play cursor,
// Lock's bounded block, references and the pool, and the game's own quarter refill (SndPack::Update 0x408D50, as the decomp
// has it) run against a stand-in: a one-shot sound ends and frees its slot, a loop runs on.
#include <stdio.h>
#include <stdlib.h>
#include "../src/snddummy_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static DWORD g_now = 1000;
static DWORD TestClock(void) { return g_now; }

// calls fn(args[0], .., args[n-1]) as stdcall; 1 = esp came back where it was (the callee popped exactly n arguments)
static int CallChecked(const void *fn, const DWORD *args, int n, DWORD *ret)
{
    DWORD e0 = 0, e1 = 0, r = 0;
    __asm {
        mov e0, esp
        mov ecx, n
        mov edx, args
    push_more:
        test ecx, ecx
        jz do_call
        dec ecx
        push dword ptr [edx + ecx * 4]
        jmp push_more
    do_call:
        call fn
        mov r, eax
        mov e1, esp
        mov esp, e0
    }
    *ret = r;
    return e0 == e1;
}
static DWORD F2D(float f) { DWORD d; memcpy(&d, &f, 4); return d; }
#define VT(o, i) ((*(const void *const *const *)(o))[i])

static void TestDecide(void)
{
    int x = 1;
    CHECK(SdbDecide(S_OK, &x) == SDB_REAL, "a made buffer is the game's own");
    CHECK(SdbDecide(1 /*S_FALSE*/, &x) == SDB_REAL, "a success code with a buffer is the game's own");
    CHECK(SdbDecide(S_OK, NULL) == SDB_DUMMY, "success with no buffer: a stand-in");
    CHECK(SdbDecide((HRESULT)0x88780096, NULL) == SDB_DUMMY, "DSERR_BUFFERLOST (the 22:48 crash): a stand-in");
    CHECK(SdbDecide((HRESULT)0x88780096, &x) == SDB_DUMMY, "a failure that left a pointer: a stand-in (the pointer not trusted)");
    CHECK(SdbDecide(E_OUTOFMEMORY, NULL) == SDB_DUMMY, "out of memory: a stand-in");
    // the Play2D / Play3D owner: Playable Eileen's guard wins; stock with PE loaded waits for the first frame
    CHECK(SdbSiteOwner(0, 1, 1, 0) == SDG_PE && SdbSiteOwner(0, 1, 1, 1) == SDG_PE, "PE's call at the site: PE's");
    CHECK(SdbSiteOwner(1, 0, 1, 0) == SDG_WAIT, "stock, PE loaded, before its DllMain may have run: wait");
    CHECK(SdbSiteOwner(1, 0, 1, 1) == SDG_OURS, "stock at the first frame with PE loaded (its build has no guard): ours");
    CHECK(SdbSiteOwner(1, 0, 0, 0) == SDG_OURS && SdbSiteOwner(1, 0, 0, 1) == SDG_OURS, "no PE: ours at once");
    CHECK(SdbSiteOwner(0, 0, 0, 0) == SDG_OTHER && SdbSiteOwner(0, 0, 1, 1) == SDG_OTHER, "another mod's bytes: left");
}

static const SdbWfx kMusicWfx = { 1, 2, 11025, 44100, 4, 16, 0 };  // the 22:48 descriptor: PCM 2 ch 11025 Hz 16-bit
static const GUID kNoGuid = { 0 };

static void TestEveryMethod(void)
{
    static const BYTE nBuf[24] = { 2, 0, 0, 1, 2, 3, 1, 1, 1, 1, 2, 7, 3, 1, 1, 1, 1, 1, 0, 4, 0, 3, 3, 4 };  // arguments after this
    static const BYTE n3D[21] = { 2, 0, 0, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 2, 2, 2, 2, 4, 4 };
    static const BYTE nRev[5] = { 2, 0, 0, 1, 1 };
    SdbDesc d;
    SdbBuf *b;
    DWORD caps[5] = { 20, 0, 0, 0, 0 }, u0 = 0, u1 = 0, u2 = 0, fxres[2] = { 9, 9 }, r;
    void *p1 = NULL, *p2 = NULL, *o = NULL, *o3 = NULL, *orv = NULL;
    SdbWfx wf;
    Sdb3D all;
    SdbVec v;
    SdbRev rv = { -1.0f, -2.0f, 1000.0f, 0.5f }, rv2;
    float fl = 0.0f;
    LONG l = 0;
    int i, bal = 1, ok = 1;
    memset(&d, 0, sizeof d);
    d.size = sizeof d, d.flags = 0x80C0, d.bytes = 0x80000, d.wfx = &kMusicWfx;
    b = SdbMake(&d);
    CHECK(b && !b->spare && b->refs == 1 && SdbIs(b) && SdbIs(&b->f3d) && SdbIs(&b->frev), "made: one reference, told apart");
    memset(&all, 0, sizeof all);
    all.size = sizeof all;
    for (i = 0; i < 24; ++i) {
        DWORD a[8] = { (DWORD)(ULONG_PTR)b, 0, 0, 0, 0, 0, 0, 0 };
        switch (i) {
        case 0: a[1] = (DWORD)(ULONG_PTR)&kSdbIidBuf8, a[2] = (DWORD)(ULONG_PTR)&o; break;
        case 3: a[1] = (DWORD)(ULONG_PTR)caps; break;
        case 4: a[1] = (DWORD)(ULONG_PTR)&u0, a[2] = (DWORD)(ULONG_PTR)&u1; break;
        case 5: a[1] = (DWORD)(ULONG_PTR)&wf, a[2] = 18, a[3] = (DWORD)(ULONG_PTR)&u2; break;
        case 6: case 7: a[1] = (DWORD)(ULONG_PTR)&l; break;
        case 8: case 9: a[1] = (DWORD)(ULONG_PTR)&u0; break;
        case 10: a[1] = 0, a[2] = (DWORD)(ULONG_PTR)&d; break;
        case 11: a[1] = 0, a[2] = 0x20000, a[3] = (DWORD)(ULONG_PTR)&p1, a[4] = (DWORD)(ULONG_PTR)&u0, a[5] = (DWORD)(ULONG_PTR)&p2,
                 a[6] = (DWORD)(ULONG_PTR)&u1, a[7] = 0; break;
        case 12: a[3] = 1; break;
        case 13: a[1] = 0x100; break;
        case 14: a[1] = (DWORD)(ULONG_PTR)&kMusicWfx; break;
        case 15: a[1] = (DWORD)-1200; break;
        case 16: a[1] = (DWORD)-300; break;
        case 17: a[1] = 0; break;
        case 19: a[1] = (DWORD)(ULONG_PTR)g_sdbScratch, a[2] = 0x20000; break;
        case 21: a[1] = 2, a[2] = 0, a[3] = (DWORD)(ULONG_PTR)fxres; break;
        case 22: a[1] = 0, a[2] = 2, a[3] = (DWORD)(ULONG_PTR)fxres; break;
        case 23: a[1] = (DWORD)(ULONG_PTR)&kNoGuid, a[2] = 0, a[3] = (DWORD)(ULONG_PTR)&kSdbIidRev, a[4] = (DWORD)(ULONG_PTR)&orv; break;
        }
        if (!CallChecked(VT(b, i), a, nBuf[i] + 1, &r)) bal = 0, printf("  buffer method %d: esp not balanced\n", i);
        if (i != 1 && i != 2 && FAILED((HRESULT)r)) ok = 0, printf("  buffer method %d: hr %08lX\n", i, r);
    }
    CHECK(bal, "every IDirectSoundBuffer8 method pops its own arguments (stdcall)");
    CHECK(ok, "every IDirectSoundBuffer8 method succeeds");
    CHECK(o == b && caps[1] == 0x80C0 && caps[2] == 0x80000 && wf.rate == 11025 && wf.ch == 2 && u2 == 18, "QI / caps / format");
    CHECK(p1 == g_sdbScratch && fxres[0] == 0 && fxres[1] == 0 && orv == &b->frev, "Lock's block, SetFX results 0, the reverb face");
    CHECK(b->vol == -1200 && b->pan == -300 && b->freq == 11025, "volume / pan / frequency kept (0 = the original rate)");
    CHECK(b->refs == 3, "QI and GetObjectInPath added a reference each (%ld)", b->refs);
    // the 3D face, every method
    ((HRESULT(__stdcall *)(void *, const GUID *, void **))VT(b, 0))(b, &kSdbIid3D, &o3);
    CHECK(o3 == &b->f3d && b->refs == 4, "QI IDirectSound3DBuffer: the 3D face, one life with the buffer");
    bal = 1, ok = 1;
    for (i = 0; i < 21; ++i) {
        DWORD a[6] = { (DWORD)(ULONG_PTR)o3, 0, 0, 0, 0, 0 };
        void *q = NULL;
        switch (i) {
        case 0: a[1] = (DWORD)(ULONG_PTR)&kSdbIid3D, a[2] = (DWORD)(ULONG_PTR)&q; break;
        case 3: case 12: a[1] = (DWORD)(ULONG_PTR)&all; break;
        case 4: a[1] = (DWORD)(ULONG_PTR)&u0, a[2] = (DWORD)(ULONG_PTR)&u1; break;
        case 5: case 10: case 11: a[1] = (DWORD)(ULONG_PTR)&v; break;
        case 6: a[1] = (DWORD)(ULONG_PTR)&l; break;
        case 7: case 8: a[1] = (DWORD)(ULONG_PTR)&fl; break;
        case 9: a[1] = (DWORD)(ULONG_PTR)&u0; break;
        case 13: a[1] = 30, a[2] = 90, a[3] = 1; break;
        case 14: a[1] = F2D(0.0f), a[2] = F2D(0.0f), a[3] = F2D(1.0f), a[4] = 1; break;
        case 15: a[1] = (DWORD)-500, a[2] = 1; break;
        case 16: a[1] = F2D(9000.0f), a[2] = 1; break;
        case 17: a[1] = F2D(400.0f), a[2] = 1; break;
        case 18: a[1] = 0, a[2] = 1; break;
        case 19: a[1] = F2D(1.0f), a[2] = F2D(2.0f), a[3] = F2D(3.0f), a[4] = 1; break;
        case 20: a[1] = F2D(4.0f), a[2] = F2D(5.0f), a[3] = F2D(6.0f), a[4] = 1; break;
        }
        if (!CallChecked(VT(o3, i), a, n3D[i] + 1, &r)) bal = 0, printf("  3D method %d: esp not balanced\n", i);
        if (i != 1 && i != 2 && FAILED((HRESULT)r)) ok = 0, printf("  3D method %d: hr %08lX\n", i, r);
        if (i == 0 && q) ((ULONG(__stdcall *)(void *))VT(q, 2))(q);
    }
    CHECK(bal, "every IDirectSound3DBuffer method pops its own arguments");
    CHECK(ok, "every IDirectSound3DBuffer method succeeds");
    CHECK(b->d3.pos.x == 1.0f && b->d3.pos.z == 3.0f && b->d3.vel.y == 5.0f && b->d3.minD == 400.0f && b->d3.maxD == 9000.0f &&
              b->d3.inAngle == 30 && b->d3.coneVol == -500 && b->d3.coneDir.z == 1.0f,
          "the 3D settings kept (Play3D's SetMinDistance / SetMaxDistance / SetPosition / SetVelocity)");
    // the reverb face
    bal = 1, ok = 1;
    for (i = 0; i < 5; ++i) {
        DWORD a[3] = { (DWORD)(ULONG_PTR)orv, 0, 0 };
        void *q = NULL;
        if (i == 0) a[1] = (DWORD)(ULONG_PTR)&kSdbIidRev, a[2] = (DWORD)(ULONG_PTR)&q;
        if (i == 3) a[1] = (DWORD)(ULONG_PTR)&rv;
        if (i == 4) a[1] = (DWORD)(ULONG_PTR)&rv2;
        if (!CallChecked(VT(orv, i), a, nRev[i] + 1, &r)) bal = 0, printf("  reverb method %d: esp not balanced\n", i);
        if (i != 1 && i != 2 && FAILED((HRESULT)r)) ok = 0;
        if (i == 0 && q) ((ULONG(__stdcall *)(void *))VT(q, 2))(q);
    }
    CHECK(bal && ok && rv2.time == 1000.0f && rv2.inGain == -1.0f, "every IDirectSoundFXWavesReverb method, the parameters kept");
    // an unknown interface: none, and no reference
    o = (void *)1;
    r = (DWORD)((HRESULT(__stdcall *)(void *, const GUID *, void **))VT(b, 0))(b, &kNoGuid, &o);
    CHECK((HRESULT)r == E_NOINTERFACE && o == NULL, "an unknown interface: E_NOINTERFACE, NULL");
    // back to none: the 3D face, the reverb face, the QI'd buffer, the made one (references across faces are one count)
    ((ULONG(__stdcall *)(void *))VT(o3, 2))(o3);
    ((ULONG(__stdcall *)(void *))VT(orv, 2))(orv);
    ((ULONG(__stdcall *)(void *))VT(b, 2))(b);
    CHECK(b->inUse == 1 && b->refs == 1, "one reference left (%ld)", b->refs);
    ((ULONG(__stdcall *)(void *))VT(b, 2))(b);
    CHECK(b->inUse == 0 && b->refs == 0 && g_sdbLive == 0, "the last Release frees the stand-in (live %ld)", g_sdbLive);
}

static void TestCursor(void)
{
    SdbDesc d;
    SdbBuf *b;
    DWORD p = 0, w = 0, st = 9;
    void *p1, *p2;
    DWORD n1, n2;
    memset(&d, 0, sizeof d);
    d.size = sizeof d, d.flags = 0x80C0, d.bytes = 0x80000, d.wfx = &kMusicWfx;
    g_now = 5000;
    b = SdbMake(&d);
    SdbGetPos(b, &p, &w);
    SdbGetStatus(b, &st);
    CHECK(p == 0 && w == 0 && st == 0, "made: stopped at 0 (play %lu write %lu status %lu)", p, w, st);
    SdbPlay(b, 0, 0, 1);
    g_now += 1000;
    SdbGetPos(b, &p, &w);
    SdbGetStatus(b, &st);
    CHECK(p == 44100 && w == 44100 + 440 && st == 5, "1 s of 11025 Hz stereo 16-bit: play %lu write %lu status %lu", p, w, st);
    g_now += 12000;  // 13 s = 573300 bytes: past 0x80000, looping
    SdbGetPos(b, &p, &w);
    CHECK(p == 573300u % 0x80000u, "looping: the cursor wraps (%lu)", p);
    SdbStop(b);
    g_now += 5000;
    SdbGetPos(b, &p, &w);
    SdbGetStatus(b, &st);
    CHECK(p == 573300u % 0x80000u && w == p && st == 0, "stopped: the cursor holds (%lu / %lu)", p, w);
    SdbSetPos(b, 0x40001);
    SdbGetPos(b, &p, NULL);
    CHECK(p == 0x40000, "SetCurrentPosition, block-aligned (%lX)", p);
    SdbPlay(b, 0, 0, 0);  // one-shot from 0x40000: 0x40000 bytes = 5.94 s
    g_now += 5900;
    SdbGetStatus(b, &st);
    CHECK(st == 1, "one-shot still playing at 5.9 s (%lu)", st);
    g_now += 100;
    SdbGetStatus(b, &st);
    CHECK(st == 0, "one-shot ended at 6.0 s (%lu)", st);
    SdbSetFreq(b, 22050);
    SdbPlay(b, 0, 0, 1);
    g_now += 1000;
    SdbGetPos(b, &p, NULL);
    CHECK(p == 88200, "SetFrequency 22050: twice the byte rate (%lu)", p);
    // Lock: bounded, split as DirectSound when the caller takes a second part
    SdbLock(b, 0, 0, &p1, &n1, &p2, &n2, 2);
    CHECK(p1 == g_sdbScratch && n1 == 0x80000 && p2 == NULL && n2 == 0, "ENTIREBUFFER: the whole block (%lX)", n1);
    SdbLock(b, 0x60000, 0x40000, &p1, &n1, &p2, &n2, 0);
    CHECK(p1 == g_sdbScratch && n1 == 0x20000 && p2 == g_sdbScratch + 0x20000 && n2 == 0x20000, "a wrap: two parts (%lX + %lX)", n1, n2);
    SdbLock(b, 0x60000, 0x40000, &p1, &n1, NULL, NULL, 0);
    CHECK(p1 == g_sdbScratch && n1 == 0x40000, "no second part asked for: one part inside the block (%lX)", n1);
    SdbLock(b, 0, 0x200000, &p1, &n1, NULL, NULL, 0);
    CHECK(n1 == 0x80000, "more than the buffer: the buffer (%lX)", n1);
    SdbRelease(b);
    CHECK(g_sdbLive == 0, "freed");
}

// SndPack::Update (0x408D50, src/sound/sndpack_4089e0.cpp) against a stand-in: -1 free slot, 0 ended (stopped, freed), 1 on
typedef struct { int active, quarter, loop; DWORD rd, end, loopStart; char qstate[4]; SdbBuf *buf; } Slot;
static int GameUpdate(Slot *s)
{
    DWORD play, write, lo;
    int a = 1, d, q;
    void *p1;
    DWORD n1;
    if (!s->active) return -1;
    q = s->quarter;
    SdbGetPos(s->buf, &play, &write);
    lo = (DWORD)q << 17;
    if (play <= lo + 0x20000) a = play < lo ? -1 : 0;
    if (write > lo + 0x20000) d = a;
    else if (write < lo) d = -a;
    else d = 0;
    if (d == 1 || (d == -1 && play > write)) {
        char *st = &s->qstate[q];
        if (*st == 2) {
            SdbStop(s->buf);
            s->active = 0;
            return 0;
        }
        SdbLock(s->buf, lo, 0x20000, &p1, &n1, NULL, NULL, 0);
        if (s->rd + 0x20000 <= s->end) s->rd += 0x20000, *st = 1;
        else if (s->loop) s->rd = s->loopStart + (0x20000 - (s->end - s->rd)) % (s->end - s->loopStart), *st = 1;
        else if (s->end > s->rd) s->rd = s->end, *st = 2;
        else *st = 2;
        memset(p1, 0, 0x20000);  // the game copies its data here (never read)
        SdbUnlock(s->buf, p1, n1, NULL, 0);
        s->quarter = (s->quarter + 1) % 4;
    }
    return 1;
}
static void TestGameRefill(void)
{
    static const SdbWfx w22 = { 1, 1, 22050, 44100, 2, 16, 0 };  // a mono 22050 Hz footstep, as Play2D makes it
    SdbDesc d;
    Slot s;
    int t, ended = -1, k;
    memset(&d, 0, sizeof d);
    d.size = sizeof d, d.flags = 0xC0, d.bytes = 0x80000, d.wfx = &w22;
    for (k = 0; k < 2; ++k) {
        memset(&s, 0, sizeof s);
        g_now = 100000;
        s.buf = SdbMake(&d);
        s.active = 1, s.loop = k, s.rd = 0x2C, s.end = 0x2C + 0xA0000, s.loopStart = 0x2C;  // a 14.9 s clip at 44100 B/s
        // Play2D's first fill: four quarters
        for (t = 0; t < 4; ++t) {
            if (s.rd + 0x20000 <= s.end) s.rd += 0x20000, s.qstate[t] = 1;
            else s.rd = s.end, s.qstate[t] = 2;
        }
        SdbPlay(s.buf, 0, 0, 1);
        ended = -1;
        for (t = 0; t < 120000; t += 16) {  // two minutes of frames at ~60 fps
            g_now += 16;
            if (GameUpdate(&s) == 0) {
                ended = t;
                break;
            }
        }
        if (!k) CHECK(ended > 14000 && ended < 30000, "a one-shot clip ends and frees its slot (after %d ms)", ended);
        else CHECK(ended < 0 && s.active, "a looping clip plays on (no end in two minutes)");
        SdbRelease(s.buf);
    }
}

// the stubs as sound.inc makes them (the same SDB_STUB text), run at mock sites that push and read as the game does
static void *g_mockDs, *g_mockOut, *g_mockOuter = (void *)1;
static const SdbDesc *g_mockDesc;
static int g_mockSite = -1;
static HRESULT __stdcall MockCreateC(void *ds, const SdbDesc *d, void **pp, void *outer, int site)
{
    g_mockDs = ds, g_mockDesc = d, g_mockOuter = outer, g_mockSite = site;
    *pp = g_mockOut;
    return (HRESULT)0x12345678;
}
SDB_STUB(tstCr2D, 0, eax, 2Ch, MockCreateC)
SDB_STUB(tstCr3D, 1, eax, 28h, MockCreateC)
SDB_STUB(tstCrMusic, 2, ecx, 28h, MockCreateC)
SDB_STUB(tstCrStream, 3, ecx, 54h, MockCreateC)
static void *g_chanBuf;  // the stream channel's buffer field (CreateBuffer passes &channel+0x2D80D0 as the out pointer)
// one mock site: the game's frame (0x80 bytes), its four pushes (NULL, &the buffer variable, &desc, ds), the call, then the
// register the displaced load fills; *espOk = esp back at the frame, *regsOk = ebx / esi / edi / ebp kept
static DWORD MockSite(int site, void *dsv, const SdbDesc *d, int *espOk, int *regsOk)
{
    DWORD got = 0, e0 = 0, e1 = 0, s0 = 0, s1 = 0;
    void *stub = site == 0 ? (void *)tstCr2D : site == 1 ? (void *)tstCr3D : site == 2 ? (void *)tstCrMusic : (void *)tstCrStream;
    void **chan = &g_chanBuf;
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 0B0B0B0Bh
        mov esi, 05151515h
        mov edi, 0D1D1D1D1h
        mov s0, ebp
        sub esp, 80h
        mov e0, esp
        mov eax, site
        cmp eax, 3
        je stream_site
        // the buffer variable at the post-call [esp+N]: 2D N = 18h, 3D / music N = 14h
        mov ecx, 14h
        cmp eax, 0
        jne have_n
        mov ecx, 18h
    have_n:
        mov dword ptr [esp + ecx], 0
        push 0
        lea edx, [esp + ecx + 4]
        push edx
        push d
        push dsv
        mov eax, dsv
        call stub
        cmp site, 2
        je music_got
        mov got, eax  // 2D / 3D: eax = the buffer
        jmp done
    music_got:
        mov got, ecx  // music: ecx = the buffer (then 0x40A1E5 mov edx,[ecx])
        jmp done
    stream_site:
        mov dword ptr [esp + 40h], 0C00C1E5h  // the cookie the displaced load reads
        push 0
        push chan
        push d
        push dsv
        call stub
        mov got, ecx
    done:
        mov e1, esp
        add esp, 80h
        mov s1, ebp
        cmp ebx, 0B0B0B0Bh
        jne regs_bad
        cmp esi, 05151515h
        jne regs_bad
        cmp edi, 0D1D1D1D1h
        jne regs_bad
        jmp regs_ok
    regs_bad:
        mov s1, 0
    regs_ok:
        pop edi
        pop esi
        pop ebx
    }
    *espOk = e0 == e1;
    *regsOk = s0 == s1;
    return got;
}
static void TestStubs(void)
{
    SdbDesc d;
    int site, espOk, regsOk;
    int fakeDs = 0, fakeBuf = 0;
    memset(&d, 0, sizeof d);
    for (site = 0; site < 4; ++site) {
        DWORD got;
        g_mockOut = &fakeBuf;
        g_chanBuf = NULL;
        g_mockSite = -1;
        g_mockOuter = (void *)1;
        got = MockSite(site, &fakeDs, &d, &espOk, &regsOk);
        CHECK(espOk && regsOk && g_mockSite == site && g_mockDs == &fakeDs && g_mockDesc == &d && g_mockOuter == NULL,
              "site %d: the stub pops the four arguments, keeps ebx / esi / edi / ebp, passes ds / desc / NULL and its site", site);
        CHECK(site < 3 ? got == (DWORD)(ULONG_PTR)&fakeBuf : got == 0xC00C1E5u && g_chanBuf == &fakeBuf,
              "site %d: the displaced load reads what the game's would (%08lX)", site, got);
    }
}

static void TestPool(void)
{
    SdbDesc d;
    SdbBuf *all[SDB_POOL], *sp, *sp2;
    int i, distinct = 1, stack = 0;
    memset(&d, 0, sizeof d);
    d.size = sizeof d, d.bytes = 0x80000, d.wfx = &kMusicWfx;
    for (i = 0; i < SDB_POOL; ++i) {
        all[i] = SdbMake(&d);
        if (all[i]->spare || (i && all[i] == all[i - 1])) distinct = 0;
    }
    sp = SdbMake(&d);
    sp2 = SdbMake(&d);
    CHECK(distinct && sp->spare && sp == sp2 && g_sdbSpareUses == 2, "128 stand-ins, then the shared spare");
    CHECK(SdbAddRef(sp) == 1 && SdbRelease(sp) == 1 && SdbRelease(sp) == 1 && sp->spare, "the spare is never freed");
    CHECK(SdbIs(sp) && SdbIs(&sp->f3d) && !SdbIs(&stack) && !SdbIs(NULL) && !SdbIs(&d), "SdbIs: the pool and the spare only");
    for (i = 0; i < SDB_POOL; ++i) SdbRelease(all[i]);
    CHECK(g_sdbLive == 0, "all freed (%ld)", g_sdbLive);
    sp = SdbMake(&d);
    CHECK(!sp->spare && sp == &g_sdbPool[0], "a freed slot is used again");
    SdbRelease(sp);
    // a descriptor with nothing usable: the defaults
    memset(&d, 0, sizeof d);
    sp = SdbMake(&d);
    CHECK(sp->bytes == SDB_DEF_BYTES && sp->avg == SDB_DEF_AVG && sp->align == 4, "no format: 0x80000 bytes, 176400 B/s");
    SdbRelease(sp);
    sp = SdbMake(NULL);
    CHECK(sp->bytes == SDB_DEF_BYTES && !sp->spare, "no descriptor: the defaults");
    SdbRelease(sp);
}

int main(void)
{
    g_sdbClock = TestClock;
    TestDecide();
    TestEveryMethod();
    TestCursor();
    TestGameRefill();
    TestStubs();
    TestPool();
    printf("snddummy_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
