// ================================================================ v3.50 EF350: Enemy Pursuit's row, the followers' side file
// Plan: staging/v350_feedback/w5_follow/PLAN.md (the user's v3.49 QA C4, C5). Included at the end of enemies.inc (its state
// and helpers). The pure parts are pursuit_sim.h (test/pursuit_sim_test.c). GOG v7 VAs, checked against the stock exe.
//
// Enemy Pursuit (Option 5 > Enemies, under Enemies Follow; [Gameplay] EnemyPursuit = Relentless / Normal / Forgetful,
// default Normal): the lose-track rules of pursuit_sim.h, applied in enemies.inc (EnmTakePursuers marks, EnmDoorPass /
// EnmDoorAway decide at each door). Relentless = v3.38 - v3.49.
//
// The followers' side file (C4: "Saving and Loading bypasses our persistence mod and despawns the enemies that followed
// me"). A save holds the game's enemy records, not Refined's own follower state, and every load dropped that state: the
// victims' ghosts that followed you into the room were not let in again (the room rule says no to them there), the
// copies that followed you went home, the followers on their way through your last door were lost (v3.49's anomaly:
// follower 02040001 came in after the Quick Save, the Quick Load went back before it), and nobody pursued you any more.
//   save: 0x4FC1A4, the `call 465410` (EnemyManagerSave) in SaveSnapshotBuild 0x4FC0E0 (the save list, Quick Save, the
//         death-continue checkpoint). After the stock call (the Randomizer's save filter 0x465468 runs inside it) the
//         image's enemy block (0x107D450, 0x1B90 bytes) is final: its CRC-32 with the image's game time (+0x04), stage
//         (+0x14) and room (+0x18) is the key. Refined's follower state of that moment (pursuit_sim.h EfState: the
//         pursuers, the followers on their way here with the door spot, the ghosts let into this room, the copies'
//         ledger) goes into the store under that key; the store (64 entries, the oldest goes) is written whole to
//         <game>\save\SH4Refined_follow.dat (a temp file, then MoveFileEx WRITE_THROUGH). Nothing is written while
//         nobody pursues you. The game's save format is untouched.
//   load: 0x4FC6ED, the `call 464740` (EnemyManagerLoad) in SaveSnapshotApply 0x4FC5B0 (a Load, a Continue, Quick Load):
//         the key of the image being applied; its entry is staged and put back at that stage / room's room load
//         (EfRestoreAtLoad, EnmWrap after the door pass, before the room's script): the pursuits, the queue, the ghost
//         tags, the copies' ledger, each only when its kind follows now (Enemies Follow's value, Follow Copies, Enemy
//         Copies) and, for a monster or a ghost, while the loaded save holds its record (one the Randomizer's save filter
//         left out comes back as placed: it does not pursue). No entry (another save, a save of v3.49, a damaged file)
//         = v3.49: nothing restored.
// [Gameplay] FollowSaveFile=0 (ini only) leaves both sites stock.

#define MSG_EF0 (MSG_V50 + 16)  // v3.50 EF350: this worker's ids (BRIEF_COMMON: V50 + 16 .. 31)
enum { EFM_L, EFM_V0, EFM_D0 = EFM_V0 + EF_PURSUIT_N, EFM_NA = EFM_D0 + EF_PURSUIT_N, EFM_N };
typedef char EfTextsFit[EFM_N <= 16 ? 1 : -1];
static BYTE g_txtEf[EFM_N][64];
static DWORD g_efIds[EF_PURSUIT_N];
static LONG g_efSnap = EF_PURSUIT_DEFAULT;
static int g_efUnsaved, g_efPursuitRow = -1;

// ---------------------------------------------------------------- the row and [Gameplay] EnemyPursuit
static const BYTE *EfText(int id)
{
    return id >= MSG_EF0 && id < MSG_EF0 + EFM_N ? g_txtEf[id - MSG_EF0] : g_txtEf[EFM_NA];
}
static void EfTexts(void)  // gameplay.inc GpTexts
{
    int v;
    Encode(g_txtEf[EFM_L], 64, "Enemy Pursuit");
    for (v = 0; v < EF_PURSUIT_N; ++v) Encode(g_txtEf[EFM_V0 + v], 64, kEfPursuitName[v]);
    Encode(g_txtEf[EFM_D0 + EF_PURSUIT_RELENTLESS], 64, "Relentless: they spot you once, never give up.");
    Encode(g_txtEf[EFM_D0 + EF_PURSUIT_NORMAL], 64, "Normal: unseen 10 s or downed, they lose you.");
    Encode(g_txtEf[EFM_D0 + EF_PURSUIT_FORGETFUL], 64, "Forgetful: unseen 4 s, they lose you.");
    Encode(g_txtEf[EFM_NA], 64, "Not available in this game (see SH4Refined.log).");
}
static void EfSetLocked(int v)
{
    if (!EfPursuitOk(v)) return;
    InterlockedExchange(&g_enmPursuit, v);
    g_efUnsaved = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", "EnemyPursuit", kEfPursuitName[v], g_iniPath)) {
        g_efUnsaved = 1;
        Log("gameplay: EnemyPursuit %s applies now but could not be saved in SH4Refined.ini (error %lu)", kEfPursuitName[v],
            GetLastError());
    } else {
        Log("gameplay: EnemyPursuit %s", kEfPursuitName[v]);
    }
}
static int EfAvail(void) { return g_enmFollowReady && !g_enmFail; }
static int __cdecl EfIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    if (!EfAvail()) {
        ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue((int)g_enmPursuit, EF_PURSUIT_N);
    if (v >= 0) EfSetLocked(v);
    GpUnlock();
    return 0;
}
static int __cdecl EfGet(void) { return EfPursuitOk((int)g_enmPursuit) ? (int)g_enmPursuit : EF_PURSUIT_DEFAULT; }
static int __cdecl EfDesc(void) { return MSG_EF0 + (!EfAvail() ? EFM_NA : EFM_D0 + EfGet()); }
static void EfPursuitFillRow(DWORD *r)  // a complete 7-dword row (gameplay.inc GpBuildPages: the Enemies page, under Enemies Follow)
{
    int v;
    for (v = 0; v < EF_PURSUIT_N; ++v) g_efIds[v] = MSG_EF0 + EFM_V0 + v;
    FillRow(r, EfIn, EfGet, g_efIds, EF_PURSUIT_N, MSG_EF0 + EFM_L, EfDesc);
}
static void EfReadIni(void)  // gameplay.inc GpReadIni (missing = Normal, added by IniStr; unknown text = Normal)
{
    char buf[32];
    IniStr("Gameplay", "EnemyPursuit", buf, sizeof buf);
    g_enmPursuit = EfPursuitParse(buf);
    g_efSnap = g_enmPursuit;
}
static void EfIniReread(void)  // under GpLock (GpIniReread)
{
    char buf[32];
    int v;
    if (g_iniFallback || g_efUnsaved) return;
    GetPrivateProfileStringA("Gameplay", "EnemyPursuit", kEfPursuitName[EfGet()], buf, sizeof buf, g_iniPath);
    v = EfPursuitParse(buf);
    if (v == (int)g_enmPursuit) return;
    InterlockedExchange(&g_enmPursuit, v);
    g_efSnap = v;
    Log("gameplay: [Gameplay] EnemyPursuit read again: %s", kEfPursuitName[v]);
}
static void EfSnapshot(void) { g_efSnap = g_enmPursuit; }
static int EfChanged(void) { return g_enmPursuit != g_efSnap; }
static int EfRowChanged(void) { return g_snapOn && g_enmPursuit != g_efSnap; }
static void EfDiscard(void)  // under GpLock (GpDiscard)
{
    if (g_enmPursuit != g_efSnap) EfSetLocked((int)g_efSnap);
}

// ---------------------------------------------------------------- the store and its file (game thread only)
typedef struct { EfKey key; unsigned stamp; int n, used; unsigned char *blob; } EfSlot;
static EfSlot g_efStore[EF_STORE_N];
static unsigned g_efStamp;
static int g_efStoreRead, g_efSiteOn, g_efIniOn = 1, g_efFault, g_efWriteFails;
static char g_efPath[MAX_PATH], g_efTmp[MAX_PATH];
static EfState g_efCap, g_efPend;  // the snapshot's state; the one staged by a load (game thread)
static int g_efPendOn, g_efPendTries;
static int g_efSaves, g_efLoadsFound, g_efLoadsNone, g_efRestores;  // this session (the end line)
static volatile LONG g_efQuotaSave, g_efQuotaLoad;
static int EfStoreCount(void)
{
    int i, n = 0;
    for (i = 0; i < EF_STORE_N; ++i) n += g_efStore[i].used;
    return n;
}
static void EfStorePut(const EfKey *k, const unsigned char *blob, int n, unsigned stamp)
{
    EfKey keys[EF_STORE_N];
    unsigned stamps[EF_STORE_N];
    int used[EF_STORE_N], i, s;
    unsigned char *b = (unsigned char *)malloc((size_t)n);
    if (!b) return;
    for (i = 0; i < EF_STORE_N; ++i) keys[i] = g_efStore[i].key, stamps[i] = g_efStore[i].stamp, used[i] = g_efStore[i].used;
    s = EfStoreSlot(keys, stamps, used, EF_STORE_N, k);
    free(g_efStore[s].blob);
    memcpy(b, blob, (size_t)n);
    g_efStore[s].key = *k, g_efStore[s].stamp = stamp, g_efStore[s].n = n, g_efStore[s].blob = b, g_efStore[s].used = 1;
    if (stamp > g_efStamp) g_efStamp = stamp;
}
static EfSlot *EfStoreFind(const EfKey *k)
{
    int i;
    for (i = 0; i < EF_STORE_N; ++i)
        if (g_efStore[i].used && EfKeySame(&g_efStore[i].key, k)) return &g_efStore[i];
    return NULL;
}
// the file into the store, once a session (at the first save or load); a damaged entry ends the read (the rest is dropped
// and the next write leaves it out)
static void EfStoreLoad(void)
{
    HANDLE h;
    DWORD size, got = 0;
    unsigned char *buf;
    int count, pos = EF_FILE_HEAD, r, n, read = 0;
    const unsigned char *blob;
    unsigned stamp;
    static EfState s_st;
    if (g_efStoreRead) return;
    g_efStoreRead = 1;
    h = CreateFileA(g_efPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;  // (none yet)
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size < EF_FILE_HEAD || size > (DWORD)(EF_FILE_HEAD + EF_STORE_N * (EF_ENTRY_HEAD + EF_BLOB_MAX))) {
        CloseHandle(h);
        LogAnomaly("enemies", "the followers' side file %s has a size of %lu bytes: it is not read (rewritten at the next save "
                   "with followers)", g_efPath, size);
        return;
    }
    buf = (unsigned char *)malloc(size);
    if (!buf) {
        CloseHandle(h);
        return;
    }
    if (!ReadFile(h, buf, size, &got, NULL) || got != size || !EfFileHeadOk(buf, (int)size, &count)) {
        CloseHandle(h);
        free(buf);
        LogAnomaly("enemies", "the followers' side file %s could not be read or is not one (it is rewritten at the next save "
                   "with followers)", g_efPath);
        return;
    }
    CloseHandle(h);
    while ((r = EfFileNext(buf, (int)size, &pos, &blob, &n, &stamp)) == 1 && read < count) {
        if (EfUnpack(blob, n, &s_st)) EfStorePut(&s_st.key, blob, n, stamp), ++read;
        else break;
    }
    if (r == -1 || read < count)
        LogAnomaly("enemies", "the followers' side file %s is damaged after %d of %d entr(ies): the rest is dropped", g_efPath,
                   read, count);
    LogV("enemies: follow saves: %d entr(ies) read from %s", read, g_efPath);
    free(buf);
}
static int EfStoreWrite(void)  // 1 = the whole store is in the file
{
    int i, total = EF_FILE_HEAD, at, count = EfStoreCount(), ok;
    unsigned char *buf;
    HANDLE h;
    DWORD wrote = 0, err = 0;
    for (i = 0; i < EF_STORE_N; ++i)
        if (g_efStore[i].used) total += EF_ENTRY_HEAD + g_efStore[i].n;
    if ((buf = (unsigned char *)malloc((size_t)total)) == NULL) return 0;
    at = EfFilePutHead(buf, count);
    for (i = 0; i < EF_STORE_N; ++i)
        if (g_efStore[i].used) at += EfFilePutEntry(buf + at, g_efStore[i].blob, g_efStore[i].n, g_efStore[i].stamp);
    h = CreateFileA(g_efTmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ok = h != INVALID_HANDLE_VALUE;
    if (ok) {
        ok = WriteFile(h, buf, (DWORD)at, &wrote, NULL) && wrote == (DWORD)at && FlushFileBuffers(h);
        if (!ok) err = GetLastError();
        CloseHandle(h);
    } else {
        err = GetLastError();
    }
    if (ok && !MoveFileExA(g_efTmp, g_efPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) err = GetLastError(), ok = 0;
    if (!ok) {
        DeleteFileA(g_efTmp);
        if (g_efWriteFails++ < 5)
            Log("enemies: the followers' side file %s could not be written (error %lu): this session keeps it in memory "
                "(a load in this session still finds it)", g_efPath, err);
    }
    free(buf);
    return ok;
}

// ---------------------------------------------------------------- the snapshot (0x4FC1A4)
static int EfCaptureHere(unsigned id) { return ENM_FIND(id) != NULL; }
// Refined's follower state now, for the snapshot being built (inside SaveSnapshotBuild, the game thread)
static void EfCapture(EfState *s)
{
    const int stage = ENM_STAGE, room = ENM_ROOM, half = ENM_HALF() ? 1 : 0;
    int i;
    memset(s, 0, sizeof *s);
    if (!EnmFollowLive() || stage < 2 || stage > 8) return;
    for (i = 0; i < ENM_PURSUERS_N && s->nPur < EF_PUR_N; ++i) {
        const EnmPursuer *e = &g_enmPursue.e[i];
        EfPurS *o;
        if (!e->used || e->stage != stage) continue;
        o = &s->pur[s->nPur++];
        o->id = e->id, o->type = (unsigned char)e->type, o->variant = (unsigned char)e->variant;
        o->misses = (unsigned char)(e->misses < 0 ? 0 : e->misses > 255 ? 255 : e->misses);
        o->flags = (unsigned char)((e->copy ? EFP_COPY : 0) | (e->ghost ? EFP_GHOST : 0) | (EfCaptureHere(e->id) ? EFP_HERE : 0) |
                                   (e->standing ? EFP_STANDING : 0));
    }
    if (g_enmQStage == stage && g_enmQRoom == room) {  // the followers on their way into this room
        for (i = 0; i < g_enmQN && i < ENM_FOLLOW_MAX && s->nQ < EF_Q_N; ++i) {
            const EnmFollower *f = &g_enmQ[i];
            EfQS *o;
            if (f->done) continue;
            o = &s->q[s->nQ++];
            o->id = f->id, o->type = (unsigned char)f->type, o->variant = (unsigned char)f->variant;
            o->flags = (unsigned char)((f->copy ? EFP_COPY : 0) | (f->ghost ? EFP_GHOST : 0));
            o->fromRoom = (signed char)f->fromRoom, o->age = g_enmClock - f->clock;
            memcpy(o->rec, f->rec, EF_REC);
        }
        if (s->nQ && g_enmDoorSet) s->doorSet = 1, memcpy(s->door, g_enmDoor, sizeof s->door);
    }
    if (g_enmGhostTagStage == stage)  // the ghosts let into this room
        for (i = 0; i < g_enmGhostTagN && i < ENM_FOLLOW_MAX && s->nTag < EF_TAG_N; ++i)
            if (g_enmGhostTag[i]) s->tag[s->nTag++] = g_enmGhostTag[i];
    for (i = 0; i < ENM_CW_N && s->nCw < EF_CW_N; ++i) {  // the copies that followed you (this world, this half)
        const EnmCw *e = &g_enmCw.e[i];
        EfCwS *o;
        if (!e->used || (int)(e->key >> 17 & 0x1F) != stage || (int)(e->key >> 16 & 1) != half) continue;
        o = &s->cw[s->nCw++];
        o->key = e->key, o->cid = e->cid, o->room = (signed char)e->room, o->type = (unsigned char)e->type;
        o->variant = (unsigned char)e->variant, o->age = g_enmClock - e->clock;
        memcpy(o->rec, e->rec, EF_REC);
        if (e->room == room && !EnmGhostType(e->type)) {  // a monster copy standing here: its place and HP now
            const BYTE *t = ENM_FIND(e->cid), *w = t ? EnmWork(t) : NULL;
            if (w && EnmStanding(*(const int *)(w + 0x3C)) && !ENM_DEAD(t) && EnmFinite3((const float *)w)) {
                __declspec(align(16)) float in[4], st[4];
                const float hp = ENM_HPSAFE(t, -1.0f);
                in[0] = ((const float *)w)[0], in[1] = ((const float *)w)[1], in[2] = ((const float *)w)[2];
                in[3] = ((const float *)o->rec)[3];
                ENM_TO_REC(st, in);
                if (EnmFinite3(st)) memcpy(o->rec, st, 16);
                if (hp > 0.0f && hp < 1.0e9f) memcpy(o->rec + 0x28, &hp, 4);
            }
        }
    }
}
static void EfKeyOf(EfKey *k, const BYTE *blk)  // the image in 0x107CD70 (gSaveSlot), its enemy block at +0x6E0
{
    k->crc = EfCrc32(blk, 0x1B90, 0);
    k->time = *(const unsigned *)0x107CD74;
    k->stage = *(const int *)0x107CD84;
    k->room = *(const int *)0x107CD88;
}
static void EfOnSnapshot(const BYTE *blk)
{
    static unsigned char s_blob[EF_BLOB_MAX];
    EfKey k;
    int n, here = 0, i, wrote;
    EfCapture(&g_efCap);
    if (EfStateEmpty(&g_efCap)) return;  // (nobody pursues you: a load of this save is v3.49's, and nothing is lost)
    EfKeyOf(&k, blk);
    g_efCap.key = k;
    EfStoreLoad();
    if ((n = EfPack(&g_efCap, s_blob, sizeof s_blob)) == 0) return;
    EfStorePut(&k, s_blob, n, ++g_efStamp);
    wrote = EfStoreWrite();
    ++g_efSaves;
    for (i = 0; i < g_efCap.nPur; ++i) here += (g_efCap.pur[i].flags & EFP_HERE) != 0;
    if (LogQuota(&g_efQuotaSave, 300))
        Log("enemies: follow save: kept for this save (stage %d room %d, game time %u, key %08X): %d pursuer(s) (%d here, %d "
            "away), %d on their way here, %d ghost(s) let into this room, %d cop(ies) that followed you; side file %d entr(ies)%s",
            k.stage, k.room, k.time, k.crc, g_efCap.nPur, here, g_efCap.nPur - here, g_efCap.nQ, g_efCap.nTag, g_efCap.nCw,
            EfStoreCount(), wrote ? "" : " (not written: kept for this session)");
}
static const DWORD g_efSaveFn = 0x465410, g_efLoadFn = 0x464740;  // EnemyManagerSave / EnemyManagerLoad (stock, checked)
static void __cdecl EfMgrSave(void *dst)
{
    ((void(__cdecl *)(void *))g_efSaveFn)(dst);
    if (g_efFault || !g_enmInstalled || g_enmFail || dst != (void *)0x107D450) return;
    __try {
        EfOnSnapshot((const BYTE *)dst);
    } __except (EnmFaultNote(GetExceptionInformation())) {
        g_efFault = 1;
        LogAnomaly("enemies", "the followers' side file faulted at a save (code %08lX at %p): off for this session (saves stay the "
                   "game's)", g_enmFaultCode, g_enmFaultAt);
    }
}

// ---------------------------------------------------------------- the load (0x4FC6ED) and the room load after it
static void EfOnApply(const EfKey *k)
{
    EfSlot *e;
    EnmLoadWatch("the save's apply");  // (Refined's state of the abandoned timeline goes, if no tick saw the load yet)
    g_efPendOn = 0, g_efPendTries = 0;
    if (!EnmFollowLive()) return;
    EfStoreLoad();
    if ((e = EfStoreFind(k)) == NULL) {
        ++g_efLoadsNone;
        if (LogQuota(&g_efQuotaLoad, 300))
            LogV("enemies: follow load: no followers kept for this save (stage %d room %d, game time %u, key %08X): nothing to "
                 "restore", k->stage, k->room, k->time, k->crc);
        return;
    }
    if (!EfUnpack(e->blob, e->n, &g_efPend) || !EfKeySame(&g_efPend.key, k)) {
        LogAnomaly("enemies", "the followers kept for this save (key %08X) do not read: nothing restored", k->crc);
        e->used = 0;
        return;
    }
    e->stamp = ++g_efStamp;  // (the newest again: not evicted soon; the file keeps its stamp until the next write)
    g_efPendOn = 1;
    ++g_efLoadsFound;
    if (LogQuota(&g_efQuotaLoad, 300))
        Log("enemies: follow load: this save's followers are kept (stage %d room %d, key %08X): %d pursuer(s), %d on their way, "
            "%d ghost(s) let in, %d cop(ies): they come back at its room load", k->stage, k->room, k->crc, g_efPend.nPur,
            g_efPend.nQ, g_efPend.nTag, g_efPend.nCw);
}
static void __cdecl EfMgrLoad(void *src)
{
    EfKey k;
    int have = 0;
    if (!g_efFault && g_enmInstalled && !g_enmFail && src == (void *)0x107D450) {
        __try {
            EfKeyOf(&k, (const BYTE *)src);
            have = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            have = 0;
        }
    }
    ((void(__cdecl *)(void *))g_efLoadFn)(src);
    if (!have) return;
    __try {
        EfOnApply(&k);
    } __except (EnmFaultNote(GetExceptionInformation())) {
        g_efFault = 1, g_efPendOn = 0;
        LogAnomaly("enemies", "the followers' side file faulted at a load (code %08lX at %p): off for this session", g_enmFaultCode,
                   g_enmFaultAt);
    }
}
// EnmWrap (the first __try, after the door pass): the staged followers come back at the saved stage / room's load. The
// world start may load another room first (room 0): the entry waits for up to 4 room loads.
static void EfRestoreAtLoad(int room)
{
    EfState *s = &g_efPend;
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
    const int monsters = EnmFollowTakesMonsters(g_enmFollow), victims = EnmFollowTakesVictims(g_enmFollow) && g_enmGhostOn;
    const int copies = g_enmFollowCopies && EnmCopiesLive();
    int i, nPur = 0, here = 0, nQ = 0, nTag = 0, nCw = 0, noRec = 0, kind = 0;
    if (!g_efPendOn) return;
    if (s->key.stage != stage || s->key.room != room) {
        if (++g_efPendTries >= 4) {
            g_efPendOn = 0;
            Log("enemies: follow restore: the load went on to room %d:%d, not the saved %d:%d: its followers are not restored", stage,
                room, s->key.stage, s->key.room);
        }
        return;
    }
    g_efPendOn = 0;
    if (!EnmFollowLive()) {
        Log("enemies: follow restore: Enemies Follow is off: this save's followers are not restored");
        return;
    }
#define EF_TAKES(copy, ghost) (((ghost) ? victims : monsters) && (!(copy) || copies))
    for (i = 0; i < s->nCw; ++i) {  // the copies' ledger first (the pursuers and the queue below find their entries)
        const EfCwS *c = &s->cw[i];
        EnmCw *e;
        if (!EF_TAKES(1, EnmGhostType(c->type))) {
            ++kind;
            continue;
        }
        if ((int)(c->key >> 17 & 0x1F) != stage || (int)(c->key >> 16 & 1) != half || !EnmIsCopyId(c->cid)) {
            ++noRec;
            continue;
        }
        EnmCwPut(&g_enmCw, c->key, c->cid, c->room, c->type, c->variant, c->rec);
        if ((e = EnmCwFind(&g_enmCw, c->key)) != NULL) e->clock = g_enmClock - (_finite(c->age) && c->age > 0.0f ? c->age : 0.0f);
        ++nCw;
    }
    for (i = 0; i < s->nPur; ++i) {  // the pursuits
        const EfPurS *p = &s->pur[i];
        const int copy = (p->flags & EFP_COPY) != 0, ghost = (p->flags & EFP_GHOST) != 0, isHere = (p->flags & EFP_HERE) != 0;
        EnmPursuer *e;
        if (!EF_TAKES(copy, ghost)) {
            ++kind;
            continue;
        }
        if (!copy) {  // a monster or a ghost: the loaded save holds its record (else it comes back as placed, or is gone)
            const BYTE *r = ENM_RECFIND(p->id);
            if (!r || *(const short *)(r + 0x26) == 0 || *(const signed char *)(r + 0x36) != stage) {
                ++noRec;
                continue;
            }
        } else if (!EnmCwFind(&g_enmCw, EnmCopyKey(stage, half, p->id)) && EnmCopyRoomOf(p->id) != room) {
            ++noRec;  // (a copy away with no ledger entry would only be made at home)
            continue;
        }
        if (EnmPursueAdd2(&g_enmPursue, p->id, p->type, p->variant, stage, isHere ? g_enmDoorSerial : g_enmDoorSerial - 1, copy,
                          ghost) < 0)
            break;  // (the table is full: never with the save's own count)
        if ((e = EnmPursueFind(&g_enmPursue, p->id)) != NULL) {
            e->misses = p->misses, e->standing = (p->flags & EFP_STANDING) != 0, e->door = g_enmDoorSerial;
            if (!isHere) e->seen = 0;  // (not in this room visit)
        }
        ++nPur, here += isHere;
        if (ghost && !copy && isHere && g_enmGhostTagN < ENM_FOLLOW_MAX) {  // with you when you saved: let in again
            int j, dup = 0;
            for (j = 0; j < g_enmGhostTagN; ++j) dup |= g_enmGhostTag[j] == p->id;
            if (!dup) g_enmGhostTag[g_enmGhostTagN++] = p->id, ++nTag;
        }
    }
    for (i = 0; i < s->nQ; ++i) {  // the followers on their way through the door you came in by
        const EfQS *q = &s->q[i];
        const int copy = (q->flags & EFP_COPY) != 0, ghost = (q->flags & EFP_GHOST) != 0;
        if (!EF_TAKES(copy, ghost)) {
            ++kind;
            continue;
        }
        if (!EnmPursueFind(&g_enmPursue, q->id)) {  // (its pursuit was not restored: it stays where its record is)
            ++noRec;
            continue;
        }
        if (!copy) {
            const BYTE *r = ENM_RECFIND(q->id);
            if (!r || *(const signed char *)(r + 0x36) != stage || *(const signed char *)(r + 0x37) != q->fromRoom ||
                *(const short *)(r + 0x26) == 0 || *(const short *)(r + 0x26) == 0x0B) {
                ++noRec;
                continue;
            }
        }
        if (EnmQueueAdd(q->id, q->type, q->variant, q->fromRoom, copy, ghost, copy ? q->rec : NULL,
                        g_enmClock - (_finite(q->age) && q->age > 0.0f ? q->age : 0.0f)))
            ++nQ;
    }
    if (nQ) {
        g_enmQStage = stage, g_enmQRoom = room;
        if (s->doorSet && EnmFinite3(s->door)) memcpy(g_enmDoor, s->door, sizeof g_enmDoor), g_enmDoorSet = 1;
    }
    if (victims)
        for (i = 0; i < s->nTag && g_enmGhostTagN < ENM_FOLLOW_MAX; ++i) {  // the ghosts let in when you saved, not in yet
            const BYTE *r = ENM_RECFIND(s->tag[i]);
            int j, dup = 0;
            if (!r || *(const signed char *)(r + 0x36) != stage || !EnmPursueFind(&g_enmPursue, s->tag[i])) continue;
            for (j = 0; j < g_enmGhostTagN; ++j) dup |= g_enmGhostTag[j] == s->tag[i];
            if (!dup) g_enmGhostTag[g_enmGhostTagN++] = s->tag[i], ++nTag;
        }
    if (nTag) g_enmGhostTagStage = stage;
#undef EF_TAKES
    ++g_efRestores;
    Log("enemies: follow restore: room %d:%d after a load: %d pursuer(s) back (%d here with you, %d away), %d on their way here, "
        "%d ghost(s) let in, %d cop(ies) in the ledger; left out: %d without their record (the save filter, or gone), %d of a "
        "kind Enemies Follow %s takes no longer", stage, room, nPur, here, nPur - here, nQ, nTag, nCw, noRec, kind,
        kGpName[GP_FOLLOW][g_enmFollow >= 0 && g_enmFollow < ENM_FOLLOW_N ? g_enmFollow : 0]);
}

// ---------------------------------------------------------------- install (EnemiesInstall, after the core set)
static void EfInstall(void)
{
    // SaveSnapshotBuild: push 107D450h; mov [107CDA4],6E0h; call 465410 (EnemyManagerSave)
    static const Site kSave = { 0x4FC195, { 0x68, 0x50, 0xD4, 0x07, 0x01, 0xC7, 0x05, 0xA4, 0xCD, 0x07, 0x01, 0xE0, 0x06, 0x00, 0x00,
                                            0xE8, 0x67, 0x92, 0xF6, 0xFF }, 20 };
    // SaveSnapshotApply: mov eax,[107CDA4]; lea ecx,[eax+107CD70h]; push ecx; call 464740 (EnemyManagerLoad)
    static const Site kLoad = { 0x4FC6E1, { 0xA1, 0xA4, 0xCD, 0x07, 0x01, 0x8D, 0x88, 0x70, 0xCD, 0x07, 0x01, 0x51, 0xE8, 0x4E, 0x80,
                                            0xF6, 0xFF }, 17 };
    // the two callees' entries (called, never patched)
    static const Site kFns[2] = {
        { 0x465410, { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00 }, 9 },
        { 0x464740, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 }, 6 },
    };
    GpPatch set[2];
    const char *why = NULL;
    sprintf_s(g_efPath, sizeof g_efPath, "%ssave\\SH4Refined_follow.dat", g_dir);
    sprintf_s(g_efTmp, sizeof g_efTmp, "%ssave\\SH4Refined_follow.tmp", g_dir);
    if (!g_iniFallback) g_efIniOn = GetPrivateProfileIntA("Gameplay", "FollowSaveFile", 1, g_iniPath) != 0;
    if (!g_efIniOn) why = "off ([Gameplay] FollowSaveFile=0)";
    else if (!SiteOk(&kSave) || !SiteOk(&kLoad) || !SiteOk(&kFns[0]) || !SiteOk(&kFns[1]))
        why = "NOT in (004FC1A4 / 004FC6ED or their callees differ from the GOG v7 exe: another mod?)";
    else {
        GpBranch(&set[0], 0x4FC1A4, 0xE8, EfMgrSave, 5);
        GpBranch(&set[1], 0x4FC6ED, 0xE8, EfMgrLoad, 5);
        g_efSiteOn = GpApplyAll(set, 2, "enemies (the followers' side file)");
        if (!g_efSiteOn) why = "NOT in (a write failed: rolled back)";
    }
    Log("enemies: followers across saves %s%s%s; Enemy Pursuit %s (memory %.0f s, doors away %d; Relentless = v3.49)",
        why ? why : "in (snapshot 004FC1A4, load 004FC6ED; side file ", why ? "" : g_efPath, why ? "" : ")",
        EfPursuitNameOf((int)g_enmPursuit), EfMemorySec((int)g_enmPursuit) < 0.0f ? 0.0f : EfMemorySec((int)g_enmPursuit),
        EfMaxMisses((int)g_enmPursuit, g_enmKeepDoors));
}
