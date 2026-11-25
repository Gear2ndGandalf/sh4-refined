// ================================================================ v3.39 PR339: the Renderer row's file side
// For SH4 Graphics Plus's review (RENDERER_SPEC.md, binding), self-contained: it needs <windows.h>, renderer_sim.h (MD5,
// the manifest parse, the state) and the caller's Log / LogV / LogAnomaly (printf-style). Paths are built from `dir`, the
// game folder (with or without its trailing backslash). It only ever:
//   * reads dgVoodoo_variants\manifest.csv, the variant files it names, d3d9.dll and dgVoodoo.conf (never writes into
//     dgVoodoo_variants\, never touches d3d9.dll);
//   * writes dgVoodoo.conf.new and moves it over dgVoodoo.conf (MoveFileEx MOVEFILE_REPLACE_EXISTING), and only when
//     every check below holds at that moment: the manifest parses, d3d9.dll has the dgVoodoo2 2.87.6 md5 (the pin), the
//     dgVoodoo.conf there NOW has the md5 of a manifest row (offered or recognised only), the chosen row is offered,
//     its file has the manifest's md5, and the .new file read back has it too. dgVoodoo.conf is never created (a missing
//     one is Custom). A failure deletes the .new file and leaves dgVoodoo.conf as it was.
// It runs on the game thread: at start (RdLoad, the live variant for the log and the Direct3D line), at each Options
// opening (RdLoad: the row's state, read once per opening) and at the Options exit with "Yes" (RdApply). dgVoodoo reads
// its conf when d3d9.dll loads: a replaced conf applies at the next start.
#define RD_D3D9_PIN "CAA75C55D9B3121D17433BD3B38C7BB2"  // RENDERER_SPEC rule 2: the game folder's d3d9.dll = dgVoodoo2 2.87.6
// v3.39 REV339 (Graphics Plus's review): the allowed d3d9.dll md5s are the manifest's `#d3d9.dll=<md5>` lines (before its
// header, one per dll); RD_D3D9_PIN is only the fallback for a manifest without any (renderer_sim.h RdD3d9Allowed). The
// manifest's own md5 is never pinned (Graphics Plus rewrites it).
#define RD_CONF_MAX (1u << 20)                          // a dgVoodoo.conf / variant file is ~21 KB; 1 MB at most is read
#define RD_D3D9_MAX (64u << 20)                         // d3d9.dll is hashed in 64 KB pieces up to this size

typedef struct {
    RdManifest m;
    RdState s;
    int haveConf, haveD3d9, d3d9Ok, haveNew;
    char conf[33], d3d9[33];
    int srcOk[RD_ROWS_MAX];       // row i's file is in dgVoodoo_variants with its md5
    char why[160];                // the reason in words (the log)
} RdInfo;

static void RdPath(const char *dir, const char *rel, char *out, size_t n)
{
    const size_t k = strlen(dir);
    _snprintf_s(out, n, _TRUNCATE, "%s%s%s", dir, k && (dir[k - 1] == '\\' || dir[k - 1] == '/') ? "" : "\\", rel);
}
// The whole file into buf (cap bytes): 1 read, 0 missing / unreadable, -1 larger than cap
static int RdReadAll(const char *path, BYTE *buf, DWORD cap, DWORD *len)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    int r = 0;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) r = 0;
    else if (size > cap) r = -1;
    else if (ReadFile(h, buf, size, &got, NULL) && got == size) r = 1, *len = size;
    CloseHandle(h);
    return r;
}
// md5 of a file, read in 64 KB pieces: 1, 0 missing / unreadable, -1 larger than cap
static int RdMd5File(const char *path, char hex[33], DWORD cap)
{
    static BYTE s_piece[65536];
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, total = 0, got;
    RdMd5 c;
    int r = 1;
    hex[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > cap) {
        CloseHandle(h);
        return size == INVALID_FILE_SIZE ? 0 : -1;
    }
    RdMd5Init(&c);
    for (;;) {
        if (!ReadFile(h, s_piece, sizeof s_piece, &got, NULL)) { r = 0; break; }
        if (!got) break;
        RdMd5Update(&c, s_piece, got);
        total += got;
        if (total > cap) { r = -1; break; }
    }
    CloseHandle(h);
    if (r == 1 && total != size) r = 0;  // the file changed while it was read
    if (r == 1) RdMd5Final(&c, hex);
    return r;
}

// Reads the manifest, the md5s of d3d9.dll, dgVoodoo.conf and every offered file, and decides the row's state.
static void RdLoad(const char *dir, const char *pin, RdInfo *out)
{
    static char s_buf[RD_MANIFEST_MAX + 1];
    static int s_said;  // the missing-file lines, 20 a session
    char path[MAX_PATH], var[MAX_PATH];
    DWORD len = 0;
    int r, i;
    memset(out, 0, sizeof *out);
    RdPath(dir, "dgVoodoo_variants\\manifest.csv", path, sizeof path);
    r = RdReadAll(path, (BYTE *)s_buf, RD_MANIFEST_MAX, &len);
    if (r == 1) RdParseManifest(s_buf, len, &out->m);
    else {
        memset(&out->m, 0, sizeof out->m);
        RdErr(&out->m, 0, r < 0 ? "larger than 16 KB" : "missing or unreadable");
    }
    RdPath(dir, "d3d9.dll", path, sizeof path);
    out->haveD3d9 = RdMd5File(path, out->d3d9, RD_D3D9_MAX) == 1;
    out->d3d9Ok = out->haveD3d9 && RdD3d9Allowed(&out->m, pin, out->d3d9);  // v3.39 REV339: the manifest's pins, else `pin`
    RdPath(dir, "dgVoodoo.conf", path, sizeof path);
    out->haveConf = RdMd5File(path, out->conf, RD_CONF_MAX) == 1;
    RdPath(dir, "dgVoodoo.conf.new", path, sizeof path);
    out->haveNew = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    for (i = 0; i < out->m.n; ++i) {
        char md5[33];
        if (!out->m.row[i].offered || RdRetiredAs(out->m.row[i].name)) continue;  // v3.44 FN344: a retired row is no choice
        _snprintf_s(var, sizeof var, _TRUNCATE, "dgVoodoo_variants\\%s", out->m.row[i].file);
        RdPath(dir, var, path, sizeof path);
        out->srcOk[i] = RdMd5File(path, md5, RD_CONF_MAX) == 1 && !strcmp(md5, out->m.row[i].md5);
        if (!out->srcOk[i] && s_said++ < 20) LogV("renderer: %s's file dgVoodoo_variants\\%s is missing or not its manifest md5 (%s): not offered",
                                 out->m.row[i].name, out->m.row[i].file, out->m.row[i].md5);
    }
    RdDecide(&out->m, out->d3d9Ok, out->haveConf ? out->conf : NULL, out->srcOk, &out->s);
    switch (out->s.st) {
    case RD_ST_NOMANIFEST:
        _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "dgVoodoo_variants\\manifest.csv %s (line %d)", out->m.err, out->m.line);
        break;
    case RD_ST_NOD3D9:
        _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "d3d9.dll %s%s (allowed: %s%s)", out->haveD3d9 ? "has md5 " : "is missing",
                    out->haveD3d9 ? out->d3d9 : "", out->m.nPin ? out->m.pin[0] : pin ? pin : "?",
                    out->m.nPin > 1 ? " and more #d3d9.dll= lines" : out->m.nPin ? " (the manifest's #d3d9.dll=)" : " (built in)");
        break;
    case RD_ST_NOSOURCE:
        _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "no offered file of the manifest is there with its md5");
        break;
    case RD_ST_CUSTOM:
        _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "dgVoodoo.conf %s%s is no manifest row's: kept as it is",
                    out->haveConf ? "md5 " : "is missing", out->haveConf ? out->conf : "");
        break;
    case RD_ST_KNOWN:
        _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "dgVoodoo.conf is %s (%s, recognised only%s)", out->m.row[out->s.row].name,
                    out->conf, out->m.row[out->s.row].offered ? ": its variant file is missing" : "");
        break;
    default:
        if (out->s.alias >= 0)  // v3.44 FN344: a retired row (no longer offered) reads as its replacement
            _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "dgVoodoo.conf is %s (%s, %s, no longer offered): read as %s",
                        out->m.row[out->s.row].label, out->m.row[out->s.row].name, out->conf,
                        out->m.row[out->s.choice[out->s.cur]].label);
        else
            _snprintf_s(out->why, sizeof out->why, _TRUNCATE, "dgVoodoo.conf is %s (%s, %s)", out->m.row[out->s.row].label,
                        out->m.row[out->s.row].name, out->conf);
        break;
    }
}

// v3.39 REV339 (Graphics Plus's suggestion): at start, a dgVoodoo.conf.new is a replace cut short (only Refined writes that
// name; Graphics Plus's installer runs with the game closed): deleted. 1 deleted, 0 none, -1 could not (error in *err).
// info: RdLoad's read of this start (the manifest, d3d9Ok). 1 = a stale .new deleted (dgVoodoo.conf there); 2 = the .new
// renamed to dgVoodoo.conf (it was missing; d3d9.dll allowed and the .new is an offered row: a switch cut short);
// 3 = the lone .new deleted (not a verified switch: dgVoodoo.conf stays missing); < 0 = that failed (err)
static int RdDropStaleNew(const char *dir, const RdInfo *info, DWORD *err)
{
    char p[MAX_PATH];
    *err = 0;
    char conf[MAX_PATH];
    RdPath(dir, "dgVoodoo.conf.new", p, sizeof p);
    RdPath(dir, "dgVoodoo.conf", conf, sizeof conf);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) return 0;
    // v3.39 REV339 (Graphics Plus): with dgVoodoo.conf gone, the .new is the switch a cut-short replace left half done:
    // finish it (2) rather than leave dgVoodoo on its built-in defaults; with dgVoodoo.conf there, the .new is stale (1)
    if (GetFileAttributesA(conf) == INVALID_FILE_ATTRIBUTES) {
        char md5[33];
        int i, offered = 0;
        if (info->d3d9Ok && info->m.ok && RdMd5File(p, md5, RD_CONF_MAX) == 1)  // v3.39 REV339 (inspection 3, Graphics Plus)
            for (i = 0; i < info->m.n; ++i)
                if (info->m.row[i].offered && !strcmp(info->m.row[i].md5, md5)) offered = 1;
        if (offered) {
            if (MoveFileExA(p, conf, MOVEFILE_WRITE_THROUGH)) return 2;
            *err = GetLastError();
            return -2;
        }
        if (DeleteFileA(p)) return 3;  // not a verified switch: dgVoodoo.conf stays missing (Custom), never created
        *err = GetLastError();
        return -3;
    }
    if (DeleteFileA(p)) return 1;
    *err = GetLastError();
    return -1;
}

// Writes `data` as dgVoodoo.conf.new, reads it back, then moves it over dgVoodoo.conf. 1 = done; 0 = failed (the .new
// file deleted, dgVoodoo.conf untouched), what in `err`.
static int RdReplaceConf(const char *dir, const BYTE *data, DWORD len, const char *md5, char *err, size_t n)
{
    char conf[MAX_PATH], tmp[MAX_PATH], back[33];
    HANDLE h;
    DWORD put = 0;
    int ok;
    RdPath(dir, "dgVoodoo.conf", conf, sizeof conf);
    RdPath(dir, "dgVoodoo.conf.new", tmp, sizeof tmp);
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        _snprintf_s(err, n, _TRUNCATE, "dgVoodoo.conf.new cannot be made (error %lu)", GetLastError());
        return 0;
    }
    ok = WriteFile(h, data, len, &put, NULL) && put == len;
    if (ok) ok = FlushFileBuffers(h) != 0;
    if (!CloseHandle(h)) ok = 0;
    if (!ok) {
        _snprintf_s(err, n, _TRUNCATE, "writing dgVoodoo.conf.new failed (error %lu)", GetLastError());
        DeleteFileA(tmp);
        return 0;
    }
    if (RdMd5File(tmp, back, RD_CONF_MAX) != 1 || strcmp(back, md5)) {
        _snprintf_s(err, n, _TRUNCATE, "dgVoodoo.conf.new read back as %s, not %s", back[0] ? back : "nothing", md5);
        DeleteFileA(tmp);
        return 0;
    }
    // v3.39 REV339 (inspection 2): ReplaceFileA needs the old dgVoodoo.conf to exist (MoveFileEx would create it if it
    // vanished after the checks); a missing one is refused, as the spec's "never create one"
    if (GetFileAttributesA(conf) == INVALID_FILE_ATTRIBUTES) {  // gone since the checks: refused, never created
        _snprintf_s(err, n, _TRUNCATE, "dgVoodoo.conf is gone (error %lu): nothing replaced", GetLastError());
        DeleteFileA(tmp);
        return 0;
    }
    if (!ReplaceFileA(conf, tmp, NULL, 0, NULL, NULL)) {
        const DWORD e = GetLastError();
        // v3.39 REV339 (Graphics Plus): ERROR_UNABLE_TO_MOVE_REPLACEMENT (1176) without a backup name means the old
        // dgVoodoo.conf is already gone and the new one still has its .new name: finish the switch (it existed at the
        // check, so this completes it rather than creating a conf from nothing); never leave the folder without one
        if (e == ERROR_UNABLE_TO_MOVE_REPLACEMENT || GetFileAttributesA(conf) == INVALID_FILE_ATTRIBUTES) {
            const int moved = MoveFileExA(tmp, conf, MOVEFILE_WRITE_THROUGH) != 0;
            const int okBack = moved && RdMd5File(conf, back, RD_CONF_MAX) == 1 && !strcmp(back, md5);
            LogAnomaly("renderer", "ReplaceFile failed (error %lu) with the old dgVoodoo.conf gone: the switch %s%s", e,
                       moved ? "was finished by a move" : "could NOT be finished (dgVoodoo.conf.new is kept)",
                       moved ? (okBack ? ", read back as the chosen file" : ", but it does not read back as the chosen file") : "");
            if (moved && okBack) return 1;
            _snprintf_s(err, n, _TRUNCATE, "dgVoodoo.conf could not be replaced cleanly (error %lu)", e);
            return 0;
        }
        _snprintf_s(err, n, _TRUNCATE, "dgVoodoo.conf could not be replaced (error %lu)", e);
        DeleteFileA(tmp);  // (the old dgVoodoo.conf is still there: 1175 and the rest leave both files as they were)
        return 0;
    }
    return 1;
}

// Replaces dgVoodoo.conf with the offered row `name`, every check made again now (the state the Options showed may be
// old). 1 = replaced, 2 = dgVoodoo.conf is that row already (nothing written), 0 = refused / failed (the reason in msg).
// msg: "old -> new" with names and md5s.
static int RdApply(const char *dir, const char *pin, const char *name, char *msg, size_t n)
{
    static RdInfo s_now;
    static BYTE s_data[RD_CONF_MAX];
    RdInfo *const i = &s_now;
    char path[MAX_PATH], var[MAX_PATH], md5[33], err[160], from[96];
    DWORD len = 0;
    int r, t;
    msg[0] = 0;
    RdLoad(dir, pin, i);
    if (!i->m.ok) { _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s", i->why); return 0; }
    if (!i->d3d9Ok) { _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s", i->why); return 0; }
    t = RdFindName(&i->m, name);
    if (t < 0 || !i->m.row[t].offered) {
        _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s is no offered row of the manifest now", name ? name : "?");
        return 0;
    }
    r = i->haveConf ? RdFindMd5(&i->m, i->conf) : -1;
    if (r < 0) {  // rule 1: only a manifest conf is replaced; a missing one is never created
        _snprintf_s(msg, n, _TRUNCATE, "not replaced: dgVoodoo.conf %s%s is no manifest row's (Custom)", i->haveConf ? "md5 " : "is missing",
                    i->haveConf ? i->conf : "");
        return 0;
    }
    _snprintf_s(from, sizeof from, _TRUNCATE, "%s (%s, %s)", i->m.row[r].offered ? i->m.row[r].label : "recognised only",
                i->m.row[r].name, i->conf);
    if (r == t) {
        _snprintf_s(msg, n, _TRUNCATE, "%s already: nothing written", from);
        return 2;
    }
    _snprintf_s(var, sizeof var, _TRUNCATE, "dgVoodoo_variants\\%s", i->m.row[t].file);
    RdPath(dir, var, path, sizeof path);
    if (RdReadAll(path, s_data, RD_CONF_MAX, &len) != 1) {
        _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s is missing, unreadable or larger than 1 MB", var);
        return 0;
    }
    RdMd5Of(s_data, len, md5);
    if (strcmp(md5, i->m.row[t].md5)) {  // rule 3: the source's md5 first
        _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s has md5 %s, the manifest says %s", var, md5, i->m.row[t].md5);
        return 0;
    }
    if (!RdReplaceConf(dir, s_data, len, md5, err, sizeof err)) {
        _snprintf_s(msg, n, _TRUNCATE, "not replaced: %s", err);
        return 0;
    }
    RdPath(dir, "dgVoodoo.conf", path, sizeof path);
    if (RdMd5File(path, md5, RD_CONF_MAX) != 1 || strcmp(md5, i->m.row[t].md5))
        LogAnomaly("renderer", "dgVoodoo.conf read back as %s after the move, not %s", md5[0] ? md5 : "nothing", i->m.row[t].md5);
    _snprintf_s(msg, n, _TRUNCATE, "%s -> %s (%s, %s): dgVoodoo.conf replaced (dgVoodoo.conf.new written and checked first)", from,
                i->m.row[t].label, i->m.row[t].name, i->m.row[t].md5);
    return 1;
}

// v3.44 FN344 (the user, v3.43 QA: "Remove the renderer value Direct3D 11 (16-bit)"): at start (RendBootRead), a
// dgVoodoo.conf that is a retired manifest row (renderer_sim.h kRdRetired) is replaced with the row it reads as, through
// RdApply (every RENDERER_SPEC rule checked again: manifest, d3d9.dll pin, the conf a manifest row's, the source's md5,
// dgVoodoo.conf.new read back). info is read again afterwards. 2 = replaced before dgVoodoo's d3d9.dll was loaded (this
// start draws with it), 1 = replaced (dgVoodoo had read the old one: the next start), 0 = nothing to do, -1 = refused
// (logged; the row still reads it as its replacement and a pick in Options can replace it).
static int RdBootRetire(const char *dir, const char *pin, RdInfo *info)
{
    char msg[512], old[RD_LABEL_MAX + RD_NAME_MAX + 8], as[RD_LABEL_MAX + 1];
    int r, early;
    if (!info->m.ok || !info->d3d9Ok || info->s.alias < 0 || info->s.st != RD_ST_OFFERED) return 0;
    _snprintf_s(old, sizeof old, _TRUNCATE, "%s (%s)", info->m.row[info->s.alias].label, info->m.row[info->s.alias].name);
    strcpy_s(as, sizeof as, info->m.row[info->s.choice[info->s.cur]].label);
    early = GetModuleHandleA("d3d9.dll") == NULL;
    r = RdApply(dir, pin, info->m.row[info->s.choice[info->s.cur]].name, msg, sizeof msg);
    if (r == 1)
        Log("renderer: dgVoodoo.conf was %s, a value the Renderer row no longer offers: it reads as %s; %s; %s", old, as, msg,
            early ? "d3d9.dll is not loaded yet, so this start already draws with it" : "it applies at the next start");
    else
        Log("renderer: dgVoodoo.conf is %s, a value the Renderer row no longer offers: the row reads it as %s; %s", old, as, msg);
    RdLoad(dir, pin, info);
    return r == 1 ? (early ? 2 : 1) : r == 2 ? 0 : -1;
}
