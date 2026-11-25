// ================================================================ [Textures] Pack: hash-keyed texture replacement (PROTOTYPE)
//
// 2026-11-09 (S18 Q11, SH4PlayableEileen\staging\s18_hires\research.md; tools and notes in
// SH4HDTextures\texpack). Off by default; nothing below is installed unless [Textures] Pack=1 or
// LogTextures=1.
//
// Every texture of a texture bank (NN00NN00, the only texture container in data\*.bin) is created by one of four
// loaders, at five call sites of 0x417480 (CreateTexture(dev, w, h, levels, 0, fmt, MANAGED, &hdr[+4], list flag) plus
// the registration that lets the arena release it, 0x416B80). Each site then copies every stored level verbatim
// (LockRect 0x415E70, memcpy 0x404A00, UnlockRect 0x415E90; no pitch, format or size checks):
//   0x42AB0B (0x42AAC0), 0x42AC62 (0x42ABF0), 0x42AD2D (0x42ACD0), 0x417979 (0x417900, first loop): record in ebx,
//      level count in edi, `add esp,24h / xor esi,esi / test edi,edi / jle <after the copy loop>`;
//   0x417A7E (0x417900, sub-texture loop): record in edi, level count in the stack slot [esp+38h] after the call
//      ([esp+3Ch] at the hook's entry), `mov eax,[esp+38h] / add esp,24h / xor esi,esi / test eax,eax / jle`.
// The record is the texture's 0x70-byte header minus 0x18: +0x1C the IDirect3DTexture8 *, +0x20 w, +0x24 h,
// +0x28 format, +0x2C levels, +0x30 level sizes [8], +0x50 level offsets [8] (relative to the record).
// The other ten callers of 0x417480 (render targets, procedural ARGB pictures) do not upload bank data: left alone.
//
// The hook replaces each of the five `call 0x417480` (the 5 bytes only). It hashes level 0 as stored (TpKeyOf: the
// first 16 hex digits of its MD5, the extractor's key). With Pack=1, a file <key>.dds in Folder and the key not in
// Folder\exclude.txt, the texture is created through 0x417480 itself with the DDS's size, levels and format (so the
// arena release list holds it like any bank texture), every DDS level is copied in (pitch-aware), and the site's copy
// loop is skipped (edi = 0 / the stack count = 0). The bank's own header, UVs and sprite maths never see the new size,
// and the arena does not grow: only the managed texture (D3D9 keeps a system-memory copy of it, in this 2 GB process)
// does. Anything wrong with a file (unreadable, bad DDS, CreateTexture refused) falls back to the stock texture and is
// logged once per key.
//
// Not to be replaced (extractor's exclude.txt, and never exported): textures the Playable Eileen plugin rewrites in the
// live texture at run time, which a replacement would hide or which its writes would corrupt: every texture of
// henry01*.bin (her skin: the curse frames are streamed into entry 171 by LockRect) and system_load.bin entries 4 and 5
// (HUD atlas F004F1DF with the Room 302 hand cell, belt icons F005F1DF). The plugin checks each texture's level size
// before writing, so a replacement that slipped through makes it skip its effect, not crash.
//
// v3.4.0: [Textures] Pool (staging\texpool\REPORT.md). 0 (default) = everything above, unchanged: MANAGED replacements,
// whose system-memory copy (D3D9 keeps one for every MANAGED texture) costs their full size in the 2 GB address space.
// 1 = the replacement is made in D3DPOOL_DEFAULT: the file goes into a SYSTEMMEM staging texture, UpdateTexture copies
// it to the DEFAULT one, the staging texture is released, and the texture is registered in the game's list itself
// (*pp = texture, insert {texture, pp} with the list's own insert 0x416B80), as 0x417480 would. Anything that fails
// falls back to the MANAGED path above.
// Device reset (static RE of GOG v7 and its wrapper, REPORT.md): the game's only Reset is 0x402CD0 (lost device:
// TestCooperativeLevel says NOTRESET, from the main tick 0x41507E; a size / mode change: 0x4149B5). Before Reset it calls
// 0x417680, which Releases every texture in the lists 0xED5774 / 0xED57B4 whose GetLevelDesc says DEFAULT and erases
// its node, without touching the record (*pp): a DEFAULT replacement would be destroyed and its record left dangling.
// GOG's wrapper (ori_d3d8.dll 1.0.0.6361, D3D8 over D3D9) passes the pool through, and at Reset releases the D3D9 side
// of every DEFAULT texture still alive and makes it again, empty, afterwards. So: at 0x402E17 (the call of 0x417680)
// every live DEFAULT replacement gets one extra reference first, so the game's Release only drops its list node; the
// object, and the record's pointer to it, stay. At 0x402E2E (right after Reset, hr in esi) the nodes are inserted again
// and, if Reset worked, each texture is filled again from its file (staging + UpdateTexture). If that fails the texture
// is swapped for a MANAGED one from the file, else for the stock texture rebuilt from the bank record, else kept
// (empty). After a FAILED Reset (the game tries again) every DEFAULT replacement is swapped the same way and Pool is off
// for the rest of the session, in case the wrapper could not let go of one. No record ever points at a released texture.
#include "texpack_core.h"

static int g_texPack, g_texLog;              // [Textures] Pack, LogTextures
static int g_texPool;                        // v3.4.0: [Textures] Pool (1 = DEFAULT-pool replacements; 0 = MANAGED)
static char g_texFolderIni[MAX_PATH];        // [Textures] Folder (relative to the game folder, or a full path)
static char g_texDir[MAX_PATH];
static CRITICAL_SECTION g_tpLock;
static int g_tpScanned;
static TpKey *g_tpHave, *g_tpSkip;           // sorted: keys of the <key>.dds files / keys listed in exclude.txt
static int g_tpHaveN, g_tpSkipN;
#define TP_SEEN 32768                        // keys seen this session (open addressing), for once-only log lines
#define TP_SEEN_LOGGED 1
#define TP_SEEN_REPLACED 2
#define TP_SEEN_BAD 4
#define TP_SEEN_SKIPPED 8
static TpKey g_tpSeen[TP_SEEN];
static BYTE g_tpSeenFlag[TP_SEEN];
static int g_tpSeenN;
static DWORD g_tpReplaced, g_tpCreated, g_tpLastSummary;
static unsigned __int64 g_tpBytesNew, g_tpBytesStock;

typedef HRESULT(__cdecl *TpCreateFn)(void *dev, DWORD w, DWORD h, DWORD levels, DWORD usage, DWORD fmt, DWORD pool,
                                     void **pp, DWORD listFlag);
#define TP_CREATE ((TpCreateFn)0x417480)
typedef struct { int Pitch; void *pBits; } TpLocked;
typedef HRESULT(__stdcall *TpLockFn)(void *tex, UINT level, TpLocked *out, const RECT *r, DWORD flags);
typedef HRESULT(__stdcall *TpUnlockFn)(void *tex, UINT level);

static int TpKeyCmp(const void *a, const void *b)
{
    const TpKey x = *(const TpKey *)a, y = *(const TpKey *)b;
    return x < y ? -1 : x > y;
}

static int TpIn(const TpKey *v, int n, TpKey k)
{
    return n > 0 && bsearch(&k, v, n, sizeof *v, TpKeyCmp) != NULL;
}

static int TpPush(TpKey **v, int *n, int *cap, TpKey k)
{
    if (*n == *cap) {
        const int nc = *cap ? *cap * 2 : 256;
        TpKey *p = (TpKey *)realloc(*v, nc * sizeof **v);
        if (!p) return 0;
        *v = p;
        *cap = nc;
    }
    (*v)[(*n)++] = k;
    return 1;
}

static void TpScan(void)  // first texture load (game thread): which <key>.dds files exist, which keys are excluded
{
    char pat[MAX_PATH], line[256];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    FILE *f = NULL;
    int cap = 0, odd = 0;
    g_tpScanned = 1;
    sprintf_s(pat, sizeof pat, "%s\\*.dds", g_texDir);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            TpKey k;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (strlen(fd.cFileName) != 20 || _stricmp(fd.cFileName + 16, ".dds") || !TpHexKey(fd.cFileName, &k)) {
                ++odd;
                continue;
            }
            TpPush(&g_tpHave, &g_tpHaveN, &cap, k);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (g_tpHaveN) qsort(g_tpHave, g_tpHaveN, sizeof *g_tpHave, TpKeyCmp);
    sprintf_s(pat, sizeof pat, "%s\\exclude.txt", g_texDir);
    cap = 0;
    if (!fopen_s(&f, pat, "r") && f) {
        while (fgets(line, sizeof line, f)) {
            const char *s = line;
            TpKey k;
            while (*s == ' ' || *s == '\t') ++s;
            if (TpHexKey(s, &k)) TpPush(&g_tpSkip, &g_tpSkipN, &cap, k);
        }
        fclose(f);
        if (g_tpSkipN) qsort(g_tpSkip, g_tpSkipN, sizeof *g_tpSkip, TpKeyCmp);
    }
    if (g_texPack)
        Log("textures: %d replacement file(s) in %s%s, %d excluded key(s)%s%s", g_tpHaveN, g_texDir,
            GetFileAttributesA(g_texDir) == INVALID_FILE_ATTRIBUTES ? " (the folder is missing)" : "", g_tpSkipN,
            g_tpSkipN ? "" : " (no exclude.txt: Playable Eileen's live textures are not protected)",
            odd ? "; other .dds names ignored (a replacement is named <16 hex digits>.dds)" : "");
}

static BYTE *TpSeen(TpKey k)  // the flags of key k (added on first sight); NULL when the table is full
{
    unsigned int i = (unsigned int)(k ^ (k >> 32)) & (TP_SEEN - 1), n;
    for (n = 0; n < TP_SEEN; ++n, i = (i + 1) & (TP_SEEN - 1)) {
        if (!(g_tpSeenFlag[i] & 0x80)) {  // a free slot: k is new
            if (g_tpSeenN >= TP_SEEN * 3 / 4) return NULL;
            g_tpSeen[i] = k;
            g_tpSeenFlag[i] = 0x80;  // used
            ++g_tpSeenN;
            return &g_tpSeenFlag[i];
        }
        if (g_tpSeen[i] == k) return &g_tpSeenFlag[i];
    }
    return NULL;
}

static double TpMB(unsigned __int64 b) { return (double)b / 1048576.0; }

// Reads <key>.dds whole and parses it. 1 = *out (VirtualAlloc, the caller frees it) and t filled.
static int TpLoadFile(TpKey key, BYTE **out, TpDds *t, char *why, size_t whyN)
{
    char path[MAX_PATH];
    HANDLE f;
    LARGE_INTEGER fs;
    BYTE *buf = NULL;
    DWORD got = 0;
    const char *err;
    sprintf_s(path, sizeof path, "%s\\%016I64x.dds", g_texDir, key);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        sprintf_s(why, whyN, "cannot open the file (error %lu)", GetLastError());
        return 0;
    }
    if (!GetFileSizeEx(f, &fs) || fs.QuadPart < 128 || fs.QuadPart > 0x10000000) {
        CloseHandle(f);
        sprintf_s(why, whyN, "file size out of range");
        return 0;
    }
    buf = (BYTE *)VirtualAlloc(NULL, (SIZE_T)fs.QuadPart, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf || !ReadFile(f, buf, (DWORD)fs.QuadPart, &got, NULL) || got != (DWORD)fs.QuadPart) {
        sprintf_s(why, whyN, buf ? "read failed (error %lu)" : "no memory for the file (error %lu)", GetLastError());
        CloseHandle(f);
        if (buf) VirtualFree(buf, 0, MEM_RELEASE);
        return 0;
    }
    CloseHandle(f);
    if ((err = TpDdsParse(buf, got, t)) != NULL) {
        sprintf_s(why, whyN, "%s", err);
        VirtualFree(buf, 0, MEM_RELEASE);
        return 0;
    }
    *out = buf;
    return 1;
}

// The MANAGED replacement (v3.3.5): created through the game's own 0x417480 (pool a[6]), every level filled.
static int TpFillManaged(TpKey key, const BYTE *buf, const TpDds *t, const DWORD *a, HRESULT *hr, DWORD *bytes, char *why,
                         size_t whyN)
{
    DWORD l, r, badLock = 0;
    void *tex;
    *hr = TP_CREATE((void *)a[0], t->w, t->h, t->levels, a[4], t->fmt, a[6], (void **)a[7], a[8]);
    tex = *(void **)a[7];
    if (*hr < 0 || !tex) {
        sprintf_s(why, whyN, "CreateTexture %ux%u %s %u level(s) failed (hr %08lX)", t->w, t->h, TpFmtName(t->fmt),
                  t->levels, (DWORD)*hr);
        return 0;  // nothing was registered (0x417480 registers on success only): the stock path creates it
    }
    *bytes = 0;
    for (l = 0; l < t->levels; ++l) {
        TpLocked lr;
        if (((TpLockFn)VT(tex, 16))(tex, l, &lr, NULL, 0x800 /* D3DLOCK_NOSYSLOCK, as the game */) < 0 || !lr.pBits) {
            ++badLock;
            continue;
        }
        for (r = 0; r < t->rows[l]; ++r)
            memcpy((BYTE *)lr.pBits + (size_t)r * lr.Pitch, buf + t->off[l] + (size_t)r * t->rowBytes[l], t->rowBytes[l]);
        ((TpUnlockFn)VT(tex, 17))(tex, l);
        *bytes += t->size[l];
    }
    if (badLock) Log("textures: %016I64x: LockRect failed on %lu of %u level(s); the texture is in place", key, badLock, t->levels);
    return 1;
}

// Creates the texture from <key>.dds through the game's own 0x417480 and fills every level. 1 = done (*hr set).
static int TpReplace(TpKey key, const DWORD *a, HRESULT *hr, DWORD *bytes, TpDds *t, char *why, size_t whyN)
{
    BYTE *buf = NULL;
    int ok;
    if (!TpLoadFile(key, &buf, t, why, whyN)) return 0;
    ok = TpFillManaged(key, buf, t, a, hr, bytes, why, whyN);
    VirtualFree(buf, 0, MEM_RELEASE);
    return ok;
}

// ---------------------------------------------------------------- v3.4.0: Pool=1, DEFAULT-pool replacements
// The game's lists of D3D objects (0x417480 registers in 0xED5774 when its last argument is 0, else in 0xED57B4): MSVC
// std::map<object, void **slot>. Map +4 = head (nil sentinel), head +4 = root; node +0 left, +4 parent, +8 right,
// +0xC key (the texture), +0x10 the record's slot, +0x15 isnil. Insert 0x416B80 (&result, &pair) and erase 0x416C60
// (&out, node) are thiscall, ret 8. The arena's release 0x4172C0 Releases the KEY and zeroes *slot, so key and *slot
// must always be the same texture.
#define TP_MAP_A 0xED5774u
#define TP_MAP_B 0xED57B4u
typedef struct TpNode { struct TpNode *l, *p, *r; void *key; void **val; BYTE color, isnil; } TpNode;
typedef HRESULT(__stdcall *TpDevCreateTexFn)(void *dev, UINT w, UINT h, UINT levels, DWORD usage, DWORD fmt, DWORD pool,
                                              void **out);
typedef HRESULT(__stdcall *TpDevUpdateTexFn)(void *dev, void *src, void *dst);
typedef ULONG(__stdcall *TpRefFn)(void *o);
typedef HRESULT(__stdcall *TpLevelDescFn)(void *tex, UINT level, DWORD *desc);  // D3DSURFACE_DESC (D3D8): 8 DWORDs
#define TP_ADDREF(o) ((TpRefFn)VT(o, 1))(o)
#define TP_RELEASE(o) ((TpRefFn)VT(o, 2))(o)

typedef struct {
    void *tex;          // the DEFAULT texture (the record's reference is the game's)
    void **pp;          // the record's slot (record + 0x1C)
    DWORD map;          // TP_MAP_A / TP_MAP_B
    TpKey key;
    DWORD w, h, fmt, levels;
    BYTE held;          // a reference of ours over the game's pre-reset release (0x417680)
    BYTE reinsert;      // its list node was erased by that release: insert it again after Reset
    BYTE reset;         // alive at a Reset: the wrapper made its D3D9 side again, empty: refill it
} TpDef;
static TpDef *g_tpDef;
static int g_tpDefN, g_tpDefCap, g_tpDefAdds;
static DWORD g_tpDefMade, g_tpDefFallback, g_tpResets;

static TpNode *TpMapFind(DWORD map, const void *key)  // read only; NULL = not in the list
{
    TpNode *head = *(TpNode **)(ULONG_PTR)(map + 4), *n;
    int guard = 0;
    if (!head) return NULL;
    for (n = head->p; n && !n->isnil && ++guard < 64;) {
        if ((ULONG_PTR)key < (ULONG_PTR)n->key) n = n->l;
        else if ((ULONG_PTR)key > (ULONG_PTR)n->key) n = n->r;
        else return n;
    }
    return NULL;
}

static int TpMapInsert(DWORD map, void *tex, void **pp)  // the game's own insert; 1 = inserted
{
    struct { void *k; void **v; } pr;
    struct { void *node; BYTE ok, pad[3]; } res;
    pr.k = tex;
    pr.v = pp;
    res.node = NULL;
    res.ok = 0;
    __asm {
        lea eax, pr
        push eax
        lea eax, res
        push eax
        mov ecx, map
        mov eax, 416B80h
        call eax
    }
    return res.ok != 0;
}

static void TpMapErase(DWORD map, TpNode *n)  // the game's own erase (n must be a real node of map)
{
    void *it = NULL;  // the iterator it returns (unused)
    __asm {
        push n
        lea eax, it
        push eax
        mov ecx, map
        mov eax, 416C60h
        call eax
    }
}

// A live entry: still in its list under its slot, the slot still points at it, and (when the wrapper can say) still a
// DEFAULT texture of its size. The list check comes first, so a released object is never touched.
static int TpDefLive(const TpDef *e, int askDesc)
{
    const TpNode *n = TpMapFind(e->map, e->tex);
    DWORD d[8];
    if (!n || n->val != e->pp || *e->pp != e->tex) return 0;
    if (askDesc && ((TpLevelDescFn)VT(e->tex, 14))(e->tex, 0, d) >= 0 &&
        (d[3] != 0 /* Pool */ || d[2] != 0 /* Usage */ || d[6] != e->w || d[7] != e->h))
        return 0;  // the address now belongs to another texture
    return 1;
}

static int TpDefAdd(void *tex, void **pp, DWORD map, TpKey key, const TpDds *t)  // under g_tpLock; 0 = no memory
{
    TpDef *e = NULL;
    int i;
    if ((++g_tpDefAdds & 255) == 0) {  // drop the entries whose texture the arena has released (no node any more)
        for (i = 0; i < g_tpDefN;)
            if (g_tpDef[i].tex != tex && !TpDefLive(&g_tpDef[i], 0)) g_tpDef[i] = g_tpDef[--g_tpDefN];
            else ++i;
    }
    for (i = 0; i < g_tpDefN; ++i)
        if (g_tpDef[i].tex == tex) { e = &g_tpDef[i]; break; }  // a new object at a released one's address
    if (!e) {
        if (g_tpDefN == g_tpDefCap) {
            const int nc = g_tpDefCap ? g_tpDefCap * 2 : 256;
            TpDef *p = (TpDef *)realloc(g_tpDef, nc * sizeof *p);
            if (!p) return 0;
            g_tpDef = p;
            g_tpDefCap = nc;
        }
        e = &g_tpDef[g_tpDefN++];
    }
    memset(e, 0, sizeof *e);
    e->tex = tex;
    e->pp = pp;
    e->map = map;
    e->key = key;
    e->w = t->w;
    e->h = t->h;
    e->fmt = t->fmt;
    e->levels = t->levels;
    return 1;
}

// A SYSTEMMEM copy of the file (every level written whole, so all of it is dirty for UpdateTexture).
static int TpStage(void *dev, const BYTE *buf, const TpDds *t, void **out, char *why, size_t whyN)
{
    void *stg = NULL;
    DWORD l, r;
    HRESULT h = ((TpDevCreateTexFn)VT(dev, 20))(dev, t->w, t->h, t->levels, 0, t->fmt, 2 /* SYSTEMMEM */, &stg);
    if (h < 0 || !stg) {
        sprintf_s(why, whyN, "staging CreateTexture %ux%u %s (SYSTEMMEM) failed (hr %08lX)", t->w, t->h, TpFmtName(t->fmt),
                  (DWORD)h);
        return 0;
    }
    for (l = 0; l < t->levels; ++l) {
        TpLocked lr;
        if (((TpLockFn)VT(stg, 16))(stg, l, &lr, NULL, 0) < 0 || !lr.pBits) {
            sprintf_s(why, whyN, "staging LockRect failed on level %lu", l);
            TP_RELEASE(stg);
            return 0;
        }
        for (r = 0; r < t->rows[l]; ++r)
            memcpy((BYTE *)lr.pBits + (size_t)r * lr.Pitch, buf + t->off[l] + (size_t)r * t->rowBytes[l], t->rowBytes[l]);
        ((TpUnlockFn)VT(stg, 17))(stg, l);
    }
    *out = stg;
    return 1;
}

// Uploads the file into dst (a DEFAULT texture of the file's size, format and levels). The wrapper's CreateTexture says
// S_OK even when its D3D9 texture could not be made; UpdateTexture then fails, so this is also the check that dst exists.
static int TpUpload(void *dev, const BYTE *buf, const TpDds *t, void *dst, char *why, size_t whyN)
{
    void *stg;
    HRESULT h;
    if (!TpStage(dev, buf, t, &stg, why, whyN)) return 0;
    h = ((TpDevUpdateTexFn)VT(dev, 29))(dev, stg, dst);
    TP_RELEASE(stg);
    if (h < 0) {
        sprintf_s(why, whyN, "UpdateTexture to the DEFAULT texture failed (hr %08lX)", (DWORD)h);
        return 0;
    }
    return 1;
}

// Pool=1 at load: the DEFAULT texture, filled and registered as 0x417480 would (*pp, then the list). Falls back to the
// MANAGED path (v3.3.5) when a step fails; file problems fail both (as in v3.3.5).
static int TpReplaceDefault(TpKey key, const DWORD *a, HRESULT *hr, DWORD *bytes, TpDds *t, char *why, size_t whyN)
{
    void *const dev = (void *)a[0];
    void **const pp = (void **)a[7];
    const DWORD map = a[8] ? TP_MAP_B : TP_MAP_A;
    BYTE *buf = NULL;
    void *tex = NULL;
    HRESULT h = 0;
    int ok;
    char why2[160];
    if (!TpLoadFile(key, &buf, t, why, whyN)) return 0;
    why2[0] = 0;
    if (a[4] || a[6] != 1)
        sprintf_s(why2, sizeof why2, "the game asked for usage %lu, pool %lu", a[4], a[6]);
    else if ((h = ((TpDevCreateTexFn)VT(dev, 20))(dev, t->w, t->h, t->levels, 0, t->fmt, 0 /* DEFAULT */, &tex)) < 0 || !tex) {
        sprintf_s(why2, sizeof why2, "CreateTexture (DEFAULT) failed (hr %08lX)", (DWORD)h);
        tex = NULL;
    } else if (!TpUpload(dev, buf, t, tex, why2, sizeof why2)) {
        TP_RELEASE(tex);
        tex = NULL;
    } else {
        int tracked;
        *pp = tex;
        if (TpMapInsert(map, tex, pp) && TpMapFind(map, tex)) {
            DWORD l;
            *hr = 0;
            for (*bytes = 0, l = 0; l < t->levels; ++l) *bytes += t->size[l];
            EnterCriticalSection(&g_tpLock);
            tracked = TpDefAdd(tex, pp, map, key, t);
            ++g_tpDefMade;
            if (!tracked) Log("textures: %016I64x: no memory to track the DEFAULT texture; a device reset releases it", key);
            LeaveCriticalSection(&g_tpLock);
            VirtualFree(buf, 0, MEM_RELEASE);
            return 1;
        }
        sprintf_s(why2, sizeof why2, "the game's list did not take it");
        *pp = NULL;
        TP_RELEASE(tex);
        tex = NULL;
    }
    ok = TpFillManaged(key, buf, t, a, hr, bytes, why, whyN);  // v3.3.5's path
    VirtualFree(buf, 0, MEM_RELEASE);
    if (ok) {
        EnterCriticalSection(&g_tpLock);
        ++g_tpDefFallback;
        Log("textures: %016I64x: no DEFAULT texture (%s); MANAGED used", key, why2);
        LeaveCriticalSection(&g_tpLock);
    }
    return ok;
}

// A MANAGED texture of the stock bank data in the record (as the stock copy loop: each level's stored bytes verbatim).
static void *TpStockManaged(void *dev, const BYTE *rec)
{
    const DWORD w = *(const DWORD *)(rec + 0x20), h = *(const DWORD *)(rec + 0x24), fmt = *(const DWORD *)(rec + 0x28),
                lv = *(const DWORD *)(rec + 0x2C);
    void *tex = NULL;
    DWORD l;
    if (!w || !h || w > 4096 || h > 4096 || !lv || lv > 8) return NULL;
    if (((TpDevCreateTexFn)VT(dev, 20))(dev, w, h, lv, 0, fmt, 1 /* MANAGED */, &tex) < 0 || !tex) return NULL;
    for (l = 0; l < lv; ++l) {
        const DWORD sz = *(const DWORD *)(rec + 0x30 + 4 * l), off = *(const DWORD *)(rec + 0x50 + 4 * l);
        TpLocked lr;
        if (!sz || sz > 0x4000000 || !off) break;
        if (((TpLockFn)VT(tex, 16))(tex, l, &lr, NULL, 0x800) < 0 || !lr.pBits) {
            if (!l) {
                TP_RELEASE(tex);
                return NULL;
            }
            break;
        }
        memcpy(lr.pBits, rec + off, sz);
        ((TpUnlockFn)VT(tex, 17))(tex, l);
    }
    return tex;
}

// A MANAGED texture of the file, not registered (for a swap after Reset).
static void *TpFileManaged(void *dev, const BYTE *buf, const TpDds *t)
{
    void *tex = NULL;
    DWORD l, r;
    if (((TpDevCreateTexFn)VT(dev, 20))(dev, t->w, t->h, t->levels, 0, t->fmt, 1 /* MANAGED */, &tex) < 0 || !tex)
        return NULL;
    for (l = 0; l < t->levels; ++l) {
        TpLocked lr;
        if (((TpLockFn)VT(tex, 16))(tex, l, &lr, NULL, 0x800) < 0 || !lr.pBits) {
            TP_RELEASE(tex);
            return NULL;
        }
        for (r = 0; r < t->rows[l]; ++r)
            memcpy((BYTE *)lr.pBits + (size_t)r * lr.Pitch, buf + t->off[l] + (size_t)r * t->rowBytes[l], t->rowBytes[l]);
        ((TpUnlockFn)VT(tex, 17))(tex, l);
    }
    return tex;
}

// 0x402E17 (was `call 0x417680`, inside the reset 0x402CD0, game thread): one reference of ours on every live DEFAULT
// replacement over the game's release of its DEFAULT objects, so only their list nodes go.
static void __cdecl TpBeforeReset(void)
{
    int i;
    EnterCriticalSection(&g_tpLock);
    for (i = 0; i < g_tpDefN;) {
        TpDef *e = &g_tpDef[i];
        if (!TpDefLive(e, 1)) {  // released by the arena (or its address reused): forget it
            *e = g_tpDef[--g_tpDefN];
            continue;
        }
        TP_ADDREF(e->tex);
        e->held = 1;
        e->reinsert = 0;
        e->reset = 1;
        ++i;
    }
    LeaveCriticalSection(&g_tpLock);
    ((void(__cdecl *)(void))0x417680)();  // the game: Release + erase every DEFAULT texture / buffer in its lists
    EnterCriticalSection(&g_tpLock);
    for (i = 0; i < g_tpDefN; ++i) {
        TpDef *e = &g_tpDef[i];
        if (!e->held) continue;
        e->held = 0;
        if (TpMapFind(e->map, e->tex)) TP_RELEASE(e->tex);  // the game kept it (its GetLevelDesc failed): ours goes
        else e->reinsert = 1;  // the game's reference went, ours is now the record's; *pp still points at it
    }
    LeaveCriticalSection(&g_tpLock);
}

// Swaps entry e's texture for nt (MANAGED): out of the list, released, the record pointed at nt, nt into the list.
static void TpSwap(TpDef *e, void *nt)
{
    TpNode *n = TpMapFind(e->map, e->tex);
    if (n && n->val == e->pp) TpMapErase(e->map, n);
    TP_RELEASE(e->tex);
    *e->pp = nt;
    TpMapInsert(e->map, nt, e->pp);
}

// Entry e becomes MANAGED: a copy of its file (buf / t when already read), else the stock texture of its record.
// 1 = from the file, 2 = stock, 0 = nothing could be made (e is kept as it is). Not ours to look after when 1 or 2.
static int TpToManaged(void *dev, TpDef *e, const BYTE *buf, const TpDds *t)
{
    BYTE *own = NULL;
    TpDds t2;
    char why[160];
    void *nt = NULL;
    int k = 0;
    if (!buf && TpLoadFile(e->key, &own, &t2, why, sizeof why)) {
        buf = own;
        t = &t2;
    }
    if (buf && (nt = TpFileManaged(dev, buf, t)) != NULL) k = 1;
    else if ((nt = TpStockManaged(dev, (const BYTE *)e->pp - 0x1C)) != NULL) k = 2;
    if (own) VirtualFree(own, 0, MEM_RELEASE);
    if (nt) TpSwap(e, nt);
    return k;
}

// 0x402E2E (was `call 0x5230D0`, right after Reset; hr = its result): the nodes back, then every texture refilled.
static void __cdecl TpAfterReset(HRESULT hr)
{
    void *const dev = (void *)DEV;
    DWORD t0 = GetTickCount(), refilled = 0, toFile = 0, toStock = 0, blank = 0, lost = 0, n = 0;
    int i;
    MEMORYSTATUSEX ms;
    EnterCriticalSection(&g_tpLock);
    for (i = 0; i < g_tpDefN; ++i) {
        TpDef *e = &g_tpDef[i];
        if (!e->reinsert) continue;
        e->reinsert = 0;
        if (!TpMapFind(e->map, e->tex) && !TpMapInsert(e->map, e->tex, e->pp)) ++lost;
    }
    ++g_tpResets;
    if (hr < 0) {
        // Safety: a failed Reset could be caused by a DEFAULT texture the wrapper could not let go of, and the game tries
        // again every frame. Every DEFAULT replacement becomes MANAGED now (v3.3.5's state; MANAGED textures can be made
        // while the device is lost) and later loads stay MANAGED for the rest of the session.
        for (i = 0; i < g_tpDefN;) {
            const int k = g_tpDef[i].reset ? TpToManaged(dev, &g_tpDef[i], NULL, NULL) : 0;
            ++n;
            if (k == 1) ++toFile;
            else if (k == 2) ++toStock;
            else {
                ++blank;
                ++i;
                continue;
            }
            g_tpDef[i] = g_tpDef[--g_tpDefN];
        }
        g_texPool = 0;
        Log("textures: device reset %lu failed (hr %08lX): Pool=1 is off for the rest of this session; of %lu DEFAULT "
            "replacement(s) %lu are now MANAGED from their files, %lu stock, %lu left as they were%s", g_tpResets, (DWORD)hr,
            n, toFile, toStock, blank, lost ? "; the list did not take some back" : "");
        LeaveCriticalSection(&g_tpLock);
        return;
    }
    for (i = 0; i < g_tpDefN;) {
        TpDef *e = &g_tpDef[i];
        BYTE *buf = NULL;
        TpDds t;
        char why[160];
        int fileOk, same;
        if (!e->reset) {
            ++i;
            continue;
        }
        e->reset = 0;
        ++n;
        fileOk = TpLoadFile(e->key, &buf, &t, why, sizeof why);
        same = fileOk && t.w == e->w && t.h == e->h && t.fmt == e->fmt && t.levels == e->levels;
        if (same && TpUpload(dev, buf, &t, e->tex, why, sizeof why)) {
            ++refilled;
            VirtualFree(buf, 0, MEM_RELEASE);
            ++i;
            continue;
        }
        if (fileOk && !same) sprintf_s(why, sizeof why, "the file changed (now %ux%u %s)", t.w, t.h, TpFmtName(t.fmt));
        {
            const TpKey key = e->key;
            const int k = TpToManaged(dev, e, fileOk ? buf : NULL, fileOk ? &t : NULL);  // the file read once, or not at all
            if (buf) VirtualFree(buf, 0, MEM_RELEASE);
            if (k == 1) ++toFile;
            else if (k == 2) ++toStock;
            else ++blank;
            if (toFile + toStock + blank <= 20)
                Log("textures: %016I64x not refilled after the reset (%s): %s", key, why,
                    k == 1 ? "a MANAGED copy of the file is used" : k == 2 ? "the stock texture is back"
                                                                   : "kept, empty (nothing else could be made)");
            if (!k) {
                ++i;
                continue;
            }
        }
        g_tpDef[i] = g_tpDef[--g_tpDefN];  // MANAGED now: not ours to look after
    }
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms))
        Log("textures: device reset %lu: %lu of %lu DEFAULT replacement(s) refilled from their files in %lu ms; %lu now "
            "MANAGED from the file, %lu stock, %lu empty%s; address space in use %.0f of %.0f MB", g_tpResets, refilled, n,
            GetTickCount() - t0, toFile, toStock, blank, lost ? "; the list did not take some back" : "",
            TpMB(ms.ullTotalVirtual - ms.ullAvailVirtual), TpMB(ms.ullTotalVirtual));
    LeaveCriticalSection(&g_tpLock);
}

static const DWORD g_tp5230 = 0x5230D0;
static __declspec(naked) void hkTexAfterReset(void)  // 0x402E2E; esi = Reset's HRESULT (0x402E2C mov esi,eax)
{
    __asm {
        pushad
        push esi
        call TpAfterReset
        add esp, 4
        popad
        jmp dword ptr [g_tp5230]  // the replaced call (returns to 0x402E33)
    }
}

// The five hooks' common part. rec = the texture record, a = 0x417480's nine arguments (a[-1] = the return address).
static HRESULT __cdecl TexCreate(const BYTE *rec, DWORD *a, int *replaced)
{
    const DWORD size0 = *(const DWORD *)(rec + 0x30), off0 = *(const DWORD *)(rec + 0x50);
    TpKey key;
    BYTE *seen;
    int want = 0;
    if (!size0 || size0 > 0x4000000 || !off0) return TP_CREATE((void *)a[0], a[1], a[2], a[3], a[4], a[5], a[6], (void **)a[7], a[8]);
    key = TpKeyOf(rec + off0, size0);
    EnterCriticalSection(&g_tpLock);
    if (!g_tpScanned) TpScan();
    ++g_tpCreated;
    seen = TpSeen(key);
    if (g_texLog && seen && !(*seen & TP_SEEN_LOGGED)) {
        *seen |= TP_SEEN_LOGGED;
        Log("textures: load %016I64x %lux%lu %s %lu level(s) (site %08lX)", key, a[1], a[2], TpFmtName(a[5]), a[3], a[-1] - 5);
    }
    if (g_texPack && TpIn(g_tpHave, g_tpHaveN, key) && !(seen && (*seen & TP_SEEN_BAD))) {
        if (TpIn(g_tpSkip, g_tpSkipN, key)) {
            if (seen && !(*seen & TP_SEEN_SKIPPED)) {
                *seen |= TP_SEEN_SKIPPED;
                Log("textures: %016I64x.dds ignored: the key is in exclude.txt (Playable Eileen writes this texture live)", key);
            }
        } else
            want = 1;
    }
    LeaveCriticalSection(&g_tpLock);
    if (want) {
        HRESULT hr = 0;
        DWORD bytes = 0, now;
        TpDds t;
        char why[160];
        if (g_texPool ? TpReplaceDefault(key, a, &hr, &bytes, &t, why, sizeof why)
                      : TpReplace(key, a, &hr, &bytes, &t, why, sizeof why)) {
            unsigned __int64 stock = 0;
            DWORD l;
            for (l = 0; l < a[3] && l < 8; ++l) stock += *(const DWORD *)(rec + 0x30 + 4 * l);
            *replaced = 1;
            EnterCriticalSection(&g_tpLock);
            ++g_tpReplaced;
            g_tpBytesNew += bytes;
            g_tpBytesStock += stock;
            if (seen && !(*seen & TP_SEEN_REPLACED)) {
                *seen |= TP_SEEN_REPLACED;
                Log("textures: replaced %016I64x: %lux%lu %s -> %ux%u %s, %u level(s), %.2f MB (stock %.2f MB)", key, a[1],
                    a[2], TpFmtName(a[5]), t.w, t.h, TpFmtName(t.fmt), t.levels, TpMB(bytes), TpMB(stock));
            }
            now = GetTickCount();
            if (now - g_tpLastSummary > 3000) {
                MEMORYSTATUSEX ms;
                ms.dwLength = sizeof ms;
                g_tpLastSummary = now;
                if (!GlobalMemoryStatusEx(&ms)) {
                } else if (g_texPool)  // v3.4.0: the pool in the line
                    Log("textures: %lu of %lu bank texture(s) replaced so far, %.1f MB created for them (stock %.1f MB); "
                        "pool DEFAULT (%lu made, %lu fell back to MANAGED); address space in use %.0f of %.0f MB",
                        g_tpReplaced, g_tpCreated, TpMB(g_tpBytesNew), TpMB(g_tpBytesStock), g_tpDefMade,
                        g_tpDefFallback, TpMB(ms.ullTotalVirtual - ms.ullAvailVirtual), TpMB(ms.ullTotalVirtual));
                else
                    Log("textures: %lu of %lu bank texture(s) replaced so far, %.1f MB created for them (stock %.1f MB); "
                        "pool MANAGED; address space in use %.0f of %.0f MB", g_tpReplaced, g_tpCreated, TpMB(g_tpBytesNew),
                        TpMB(g_tpBytesStock), TpMB(ms.ullTotalVirtual - ms.ullAvailVirtual), TpMB(ms.ullTotalVirtual));
            }
            LeaveCriticalSection(&g_tpLock);
            return hr;
        }
        EnterCriticalSection(&g_tpLock);
        if (seen) *seen |= TP_SEEN_BAD;  // not tried again this session
        Log("textures: %016I64x.dds not used (%s); the stock texture is kept", key, why);
        LeaveCriticalSection(&g_tpLock);
    }
    return TP_CREATE((void *)a[0], a[1], a[2], a[3], a[4], a[5], a[6], (void **)a[7], a[8]);
}

// 0x42AB0B / 0x42AC62 / 0x42AD2D / 0x417979: record in ebx; a replaced texture sets edi (the level count) to 0.
static __declspec(naked) void hkTexCreateEbx(void)
{
    __asm {
        push 0                  // [esp] = replaced
        mov eax, esp
        push eax
        lea eax, [esp + 0Ch]    // 0x417480's arguments (above the flag and the return address)
        push eax
        push ebx
        call TexCreate
        add esp, 0Ch
        pop ecx
        test ecx, ecx
        jz done
        xor edi, edi            // `test edi,edi / jle` skips the stock copy loop
    done:
        ret
    }
}

// 0x417A7E: record in edi; the level count is the caller's [esp+38h] after the call ([esp+3Ch] here, at entry).
static __declspec(naked) void hkTexCreateEdi(void)
{
    __asm {
        push 0
        mov eax, esp
        push eax
        lea eax, [esp + 0Ch]
        push eax
        push edi
        call TexCreate
        add esp, 0Ch
        pop ecx
        test ecx, ecx
        jz done
        mov dword ptr [esp + 3Ch], 0  // `mov eax,[esp+38h] / test eax,eax / jle` skips the stock copy loop
    done:
        ret
    }
}

static void TexPackReadIni(void)
{
    g_texPack = IniInt("Textures", "Pack") ? 1 : 0;
    g_texLog = IniInt("Textures", "LogTextures") ? 1 : 0;
    g_texPool = IniInt("Textures", "Pool") == 1 ? 1 : 0;  // v3.4.0
    IniStr("Textures", "Folder", g_texFolderIni, sizeof g_texFolderIni);
    if (!g_texFolderIni[0]) strcpy_s(g_texFolderIni, sizeof g_texFolderIni, "textures");
    if (g_texFolderIni[1] == ':' || (g_texFolderIni[0] == '\\' && g_texFolderIni[1] == '\\'))
        strcpy_s(g_texDir, sizeof g_texDir, g_texFolderIni);
    else
        sprintf_s(g_texDir, sizeof g_texDir, "%s%s", g_dir, g_texFolderIni);  // g_dir ends with a backslash
    {
        size_t n = strlen(g_texDir);
        while (n && (g_texDir[n - 1] == '\\' || g_texDir[n - 1] == '/')) g_texDir[--n] = 0;
    }
    Log("settings: [Textures] Pack %d, LogTextures %d, Pool %d (%s), Folder \"%s\"", g_texPack, g_texLog, g_texPool,
        g_texPool ? "DEFAULT" : "MANAGED", g_texFolderIni);
}

static void TexPackInstall(void)  // DllMain (Init), before the game's code runs
{
    static const Site kSites[5] = {
        { 0x42AB0B, { 0xE8, 0x70, 0xC9, 0xFE, 0xFF, 0x83, 0xC4, 0x24, 0x33, 0xF6, 0x85, 0xFF, 0x7E }, 13 },
        { 0x42AC62, { 0xE8, 0x19, 0xC8, 0xFE, 0xFF, 0x83, 0xC4, 0x24, 0x33, 0xF6, 0x85, 0xFF, 0x7E }, 13 },
        { 0x42AD2D, { 0xE8, 0x4E, 0xC7, 0xFE, 0xFF, 0x83, 0xC4, 0x24, 0x33, 0xF6, 0x85, 0xFF, 0x7E }, 13 },
        { 0x417979, { 0xE8, 0x02, 0xFB, 0xFF, 0xFF, 0x83, 0xC4, 0x24, 0x33, 0xF6, 0x85, 0xFF, 0x7E }, 13 },
        { 0x417A7E, { 0xE8, 0xFD, 0xF9, 0xFF, 0xFF, 0x8B, 0x44, 0x24, 0x38, 0x83, 0xC4, 0x24, 0x33, 0xF6, 0x85, 0xC0, 0x7E }, 17 },
    };
    // v3.4.0, Pool=1: the two device-reset sites in 0x402CD0 (checked with their neighbours; both or none)
    static const Site kReset[2] = {
        { 0x402E12, { 0xE8, 0x59, 0xFE, 0xFF, 0xFF, 0xE8, 0x64, 0x48, 0x01, 0x00, 0xA1, 0x94, 0x7D, 0x6E, 0x00 }, 15 },
        { 0x402E29, { 0xFF, 0x51, 0x38, 0x8B, 0xF0, 0xE8, 0x9D, 0x02, 0x12, 0x00, 0x48, 0x50 }, 12 },
    };
    int i, n = 0;
    if (!g_texPack && !g_texLog) return;
    InitializeCriticalSection(&g_tpLock);
    if (g_texPool && !g_texPack) g_texPool = 0;
    if (g_texPool) {
        if (!SiteOk(&kReset[0]) || !SiteOk(&kReset[1])) {
            Log("textures: the device reset code 00402E12 / 00402E29 is not stock: Pool=1 cannot keep DEFAULT textures "
                "across a reset, so replacements stay MANAGED");
            g_texPool = 0;
        } else if (!PokeBranch(0x402E17, 0xE8, (const void *)TpBeforeReset, 5)) {
            Log("textures: 00402E17 could not be patched: replacements stay MANAGED");
            g_texPool = 0;
        } else if (!PokeBranch(0x402E2E, 0xE8, (const void *)hkTexAfterReset, 5)) {
            static const BYTE kStock[5] = { 0xE8, 0x64, 0x48, 0x01, 0x00 };
            Poke(0x402E17, kStock, 5);  // both or none
            Log("textures: 00402E2E could not be patched: replacements stay MANAGED");
            g_texPool = 0;
        } else
            Log("textures: Pool=1: replacements in D3DPOOL_DEFAULT; device reset hooks at 00402E17 / 00402E2E");
    }
    for (i = 0; i < 5; ++i) {
        if (!SiteOk(&kSites[i])) {
            Log("textures: %08lX is not the stock loader call: left (textures created there stay stock)", kSites[i].va);
            continue;
        }
        if (PokeBranch(kSites[i].va, 0xE8, i < 4 ? (const void *)hkTexCreateEbx : (const void *)hkTexCreateEdi, 5)) ++n;
    }
    Log("textures: %d/5 bank texture loader hooks in place (%s)", n,
        !g_texPack ? "LogTextures only, nothing replaced" : g_texPool ? "replacement on, pool DEFAULT" : "replacement on, pool MANAGED");
}
