// ================================================================ v3.9.0: Eileen outfit engine (Eileen Outfit v4 in C)
// A translation of the cheat table's entry 7100 "Eileen Outfit v4" (staging/eileen_outfit/ct_7100_eileen_outfit.txt,
// Auto Assembler). Plan: staging/eileen_outfit/PLAN.md (O1-O6); RE: staging/extra_menu/RE.md 2.5 (the eo_api contract).
// The table's symbol names are kept (eo_*). All addresses are absolute VAs of the GOG v7 exe. Included by
// sh4windowmod.c after savebanks.inc (uses Site / SiteOk, Poke / PokeBranch, Log, SH4R_Extra, g_iniFallback / g_iniPath,
// g_eoIniOutfit / g_eoIniAuto / g_eoIniEink and extramenu.inc's EmOurPatch).
//
// Hooks (the table's 9 byte ranges): 57342F eo_remap (6), 4F3490 eo_stage (6), 4F3514 eo_room (10), 445BDB eo_gate (5),
// 44ADE0 eo_tick (6), 5733F0 eo_fid (8), 4F1C16 eo_csid (8), 4F2100 eo_cloth (5), 4FC62A eo_load (5).
// Not ported (CE only): the [ENABLE] Lua (stale eo_api symbol drop; the 500 ms timer that wrote eo_api into
// SH4R_Extra+0C: Refined sets SH4R_Extra[3] itself at install), [DISABLE] (no disable here, so its 13D19EC fix-up is not
// needed), the table rows 7101-7108 (Outfit = [EileenOutfit] Outfit and the pause row, Instant = eo_auto, Cutscenes =
// eo_eink; row 7106 "Change Now" has no control: eo_apply is set by eo_tick / eo_menu_go only). The Advanced rows are
// fixed at the table's defaults below.

#define EO_I(a) (*(volatile int *)(ULONG_PTR)(a))
#define EO_U(a) (*(volatile DWORD *)(ULONG_PTR)(a))
#define EO_W(a) (*(volatile WORD *)(ULONG_PTR)(a))
#define EO_B(a) (*(volatile BYTE *)(ULONG_PTR)(a))
#define EO_FLAG EO_I(0x13D19EC)  // the game's outfit flag (56F570 reads it, 56F580 writes it)
typedef int(__cdecl *EoFn1)(DWORD);
typedef int(__cdecl *EoFn2)(DWORD, DWORD);
#define EO_CALL1(va, a) (((EoFn1)(ULONG_PTR)(va))((DWORD)(a)))
#define EO_CALL2(va, a, b) (((EoFn2)(ULONG_PTR)(va))((DWORD)(a), (DWORD)(b)))
#define EO_ID_EIL 0xF000F058u  // eil.bin (Dress); eol.bin (Nurse) = +1

// ---------------------------------------------------------------- the table's Advanced rows, at their defaults
static const int eo_keep_pos = 1, eo_force = 0, eo_cs143 = 1, eo_cs_cloth = 1;
static const DWORD eo_okmask = 0x67E, eo_timeout = 0x384;

// ---------------------------------------------------------------- data (CT eo_mem)
static volatile int eo_outfit = -1;  // -1 Default (game decides), 0 Dress, 1 Nurse (read by eo_remap / eo_stage stubs)
static int eo_apply;                 // 1 = swap now
static volatile int eo_status;       // 0 idle, 1 waiting, 2 loading, 3 swapped, 4 already, 5 room load; -2..-9 failed
static int eo_why;                   // last reason a pending swap waited (-2/-3/-4)
static volatile int eo_cur = -1;     // outfit of her model now (0/1, -1 unknown)
static DWORD eo_loaded;              // bit0 eil live, bit1 eol live, bit4 eil present, bit5 eol present
static int eo_blocked;               // 1 = the wanted model was not resident (other one used)
static int eo_auto = 1;              // 1 = a new eo_outfit is applied on the fly
static DWORD eo_count, eo_builds, eo_roomloads;
static int eo_phase;                 // 1 = our own file request is in flight
static int eo_target;
static DWORD eo_frames;
static DWORD eo_reqid;
static int eo_auto_last = -1;
static DWORD eo_sv_fd, eo_sv_cmd;
static volatile int eo_world;        // models the world's file list loaded: bit0 eil, bit1 eol (0 none / unknown)
static int eo_eink = 1;              // 0 off, 1 eink -> eisk / eosk, 2 eink normal / eosk nurse
static DWORD eo_cs_last_id, eo_cs_last_file;
static int eo_cs_last_nurse;
static DWORD eo_cs_count, eo_eink_reqs;
static DWORD eo_cs_c[3];             // candidate file ids for eo_cs_pick (0 = skip)
static int eo_game = -1;             // the game's own outfit flag (0/1) = what Default uses; -1 not seen yet
static int eo_flagw = -2;            // last value WE wrote to 13D19EC (-2 none yet)
// eo_api (32 bytes; RE 2.5): +0 'EOV2', +4 eo_menu_can, +8 eo_menu_go, +C &eo_cur, +10 &eo_status, +14 'EOV4',
// +18 eo_menu_set(choice), +1C &eo_outfit. Filled at install; SH4R_Extra[3] = its address.
static volatile DWORD eo_api[8];
// [not in the table] Refined bookkeeping
static int g_eoSaved = -1;           // eo_outfit as last written to the ini / logged
static int g_eoLogged;               // eo_status last logged

static int EoFault(EXCEPTION_POINTERS *e, const char *where)
{
    static int n;
    if (++n <= 5)
        Log("eileen outfit: %s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
            e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void EoLogStatus(void)  // [not in the table] one line per eo_status change (bounded)
{
    static int n;
    const int s = eo_status;
    if (s == g_eoLogged) return;
    g_eoLogged = s;
    if (++n <= 200)
        Log("eileen outfit: status %d (why %d, cur %d, target %d, outfit %d, game %d, swaps %lu)", s, eo_why, eo_cur,
            eo_target, eo_outfit, eo_game, eo_count);
}

// ---------------------------------------------------------------- helpers (CT eo_live .. eo_valid)
// eo_live: 1 if id is in the file cache 13D1BF0 (128 x 14h: id, buf, size, parked, used), not parked, inside the
// live arena [11B6000, 11B6004]; *entry = the cache entry or 0
static int eo_live(DWORD id, DWORD *entry)
{
    const volatile DWORD *e = (const volatile DWORD *)(ULONG_PTR)0x13D1BF0;
    DWORD buf;
    int n;
    for (n = 0; n < 0x80; ++n, e += 5)
        if (e[0] == id) break;
    if (n == 0x80) {
        if (entry) *entry = 0;
        return 0;
    }
    if (entry) *entry = (DWORD)(ULONG_PTR)e;
    if (e[3]) return 0;  // parked above the arena top (5736D0 would return 0)
    buf = e[1];
    if (!buf || buf < EO_U(0x11B6000)) return 0;
    if (buf + e[2] > EO_U(0x11B6004)) return 0;
    return 1;
}

// eo_scan: refresh eo_loaded, return it
static DWORD eo_scan(void)
{
    DWORD b = 0, ent;
    b |= (DWORD)eo_live(EO_ID_EIL, &ent);
    if (ent) b |= 0x10;
    b |= (DWORD)eo_live(EO_ID_EIL + 1, &ent) << 1;
    if (ent) b |= 0x20;
    eo_loaded = b;
    return b;
}

// eo_want: the chosen outfit 0/1, or < 0 for Default
static int eo_want(void)
{
    const int a = eo_outfit;
    if (a < 0) return a;
    return a > 1 ? 1 : a;
}

// eo_seen: a 13D19EC value we did not write (save load, new game, title) is kept as eo_game
static void eo_seen(void)
{
    const int f = EO_FLAG;
    if (f == eo_flagw && eo_game >= 0) return;
    eo_flagw = f;
    eo_game = f != 0;
}

// eo_put: 0/1 -> the game's flag 13D19EC, noted in eo_flagw as written by us
static void eo_put(int v)
{
    EO_FLAG = v;
    eo_flagw = v;
}

// eo_mwant: the outfit for her in-game model: the choice 0/1, or for Default the game's own flag
static int eo_mwant(void)
{
    int a;
    eo_seen();
    a = eo_want();
    if (a < 0) a = eo_game;
    return a;
}

// eo_getcur: which file her live model is built from ([model+B4] = sub-file 1 of eil / eol): 0/1/-1, into eo_cur
static int eo_getcur(void)
{
    DWORD t, m, d;
    int r = -1;
    t = EO_U(0xFCE6B4);
    if (t && (t = EO_U(t + 0x408)) != 0 && (m = EO_U(t + 0x18)) != 0 && (d = EO_U(m + 0xB4)) != 0) {
        if ((DWORD)EO_CALL1(0x5736D0, 0xF001F058) == d) r = 0;
        else if ((DWORD)EO_CALL1(0x5736D0, 0xF001F059) == d) r = 1;
    }
    eo_cur = r;
    return r;
}

// eo_valid: 1 if sub-files 0, 1, 2 of outfit o resolve inside the live arena
static int eo_valid(int o)
{
    DWORD id = EO_ID_EIL + (DWORD)o, p;
    int i;
    for (i = 0; i < 3; ++i, id += 0x10000) {
        p = (DWORD)EO_CALL1(0x5736D0, id);
        if (!p || p < EO_U(0x11B6000) || p >= EO_U(0x11B6004)) return 0;
    }
    return 1;
}

// eo_check: menu 0 (her tick, world running) / 1 (pause menu) -> 0 ok, -2 absent, -3 game busy, -4 Eileen busy
static int eo_check(int menu)
{
    DWORD t, w;
    DWORD st;
    if (EO_I(0xFD5AA0) || EO_I(0xFD5AA4)) return -3;                    // stage / room change pending
    if (EO_I(0xFD5BB0) || EO_I(0xFD5BB4) || EO_I(0xFD5BB8)) return -3;  // world exit (death / ending)
    if (EO_I(0xFD5C28)) return -3;                                      // interaction runner
    if (EO_I(0x10A1914)) return -3;                                     // player frozen by an event
    if (EO_I(0x10A0F28) <= 0) return -3;                                // Henry HP (float bits) > 0
    if (!EO_I(0x107BF50)) return -3;                                    // sub-menus allowed
    if (!menu) {
        if (EO_I(0x107BF54) || EO_I(0x107BF58)) return -3;  // a sub-mode running / pending
        if (EO_B(0x1079C30) != 1) return -3;                // HUD + input on
        if (EO_I(0xFD6C78) != 1) return -3;                 // room geometry drawn
    }
    t = EO_U(0xFCE6B4);
    if (!t || EO_U(t + 0x404) != 0x44ADE0) return -2;  // her normal task function
    w = EO_U(t + 0x408);
    if (!w || EO_U(w + 0x18) != 0x10A1400) return -2;  // the in-game model instance
    if (eo_force) return 0;
    if (EO_I(w + 0xF0)) return -4;                      // still walking in through a door
    if (EO_I(EO_U(w)) < 0x3F800000) return -4;          // HP >= 1.0 (float bits)
    st = EO_W(t + 0x410);                               // her state
    if (st > 0x1F || !(eo_okmask & (1u << st))) return -4;
    return 0;
}

// 44ABB0: task init, edi = task (ebx / esi / ebp / edi kept)
static void EoTaskInit(DWORD task)
{
    __asm {
        push edi
        mov edi, task
        mov eax, 44ABB0h
        call eax
        pop edi
    }
}

// eo_swap: task = Eileen task (= [FCE6B4]), o = outfit. The game's own destroy + spawn, same slot.
static void eo_swap(DWORD task, int o)
{
    DWORD w = EO_U(task + 0x408), wp;
    EO_CALL1(0x44D9E0, w);           // store live state -> FCE7F0
    eo_sv_fd = EO_U(0xFD5BC4);       // follow mode 1 / waiting 2
    eo_sv_cmd = EO_W(w + 0x356);     // follow / wait command
    eo_put(o);                       // 13D19EC = the outfit she is rebuilt with
    wp = EO_U(0xFCE5B0);             // CT-FIX 2026-10-25: stale weapon task pointer
    if (wp && EO_U(wp + 0x404) != 0x4456C0) EO_U(0xFCE5B0) = 0;
    EO_CALL1(0x52B020, task);        // unregister + destroy
    EO_CALL2(0x5538A0, task, 0x44ADE0);  // same slot, same task function
    EO_U(0xFCE6B4) = task;
    if (eo_keep_pos && EO_U(0xFD5BC4) == 1) EO_U(0xFD5BC4) = 2;  // 445BD0 keeps the stored position
    EoTaskInit(task);
    EO_U(0xFD5BC4) = eo_sv_fd;
    w = EO_U(task + 0x408);
    if (w) EO_W(w + 0x356) = (WORD)eo_sv_cmd;
}

static void eo_sv_fail(int code)
{
    eo_status = code;
    eo_apply = 0;
    eo_phase = 0;
    eo_frames = 0;
}

// eo_service: called from her tick while eo_apply != 0
static void eo_service(DWORD task)
{
    int o, cur, r;
    if (eo_phase != 1) eo_target = eo_mwant();  // v4: a read in flight keeps its own target
    o = eo_target;
    cur = eo_getcur();
    if (cur == -1) {
        eo_sv_fail(-9);
        return;
    }
    if (cur == o) {  // already wearing it
        eo_status = 4;
        eo_apply = 0;
        eo_phase = 0;
        eo_frames = 0;
        return;
    }
    eo_reqid = EO_ID_EIL + (DWORD)o;
    if (eo_phase == 1) {  // eo_sv_wait
        if (!EO_CALL1(0x573370, 1)) {  // read queue not idle
            if (++eo_frames < eo_timeout) return;
            eo_sv_fail(-7);
            return;
        }
        eo_phase = 0;  // eo_sv_loaded
        eo_frames = 0;
        if (!eo_live(eo_reqid, NULL)) {
            eo_sv_fail(-8);
            return;
        }
    } else if (!eo_live(eo_reqid, NULL)) {  // not resident: request it
        if (EO_CALL1(0x573470, eo_reqid)) {
            eo_sv_fail(-6);
            return;
        }
        eo_phase = 1;
        eo_status = 2;
        eo_frames = 0;
        return;
    }
    eo_scan();  // eo_sv_valid
    if (!eo_valid(o)) {
        eo_sv_fail(-8);
        return;
    }
    r = eo_check(0);  // eo_sv_chk
    if (r) {
        eo_why = r;
        eo_status = 1;
        if (EO_I(0x107BF54) || EO_I(0x107BF58)) return;  // v4: a menu is open or pending: wait, no timeout
        if (++eo_frames < eo_timeout) return;
        eo_sv_fail(r);
        return;
    }
    eo_swap(task, o);  // eo_sv_pend
    eo_getcur();
    ++eo_count;
    eo_status = 3;
    eo_why = 0;
    eo_apply = 0;
    eo_phase = 0;
    eo_frames = 0;
    if (eo_mwant() != eo_target) eo_apply = 1;  // v4: the choice changed meanwhile: go again next tick
}

// ---------------------------------------------------------------- pause menu API (eo_api; cdecl, eax result)
static void EoSaveOutfit(void)  // [not in the table] [EileenOutfit] Outfit + one log line per change
{
    char v[8];
    const int o = eo_outfit;
    if (o == g_eoSaved) return;
    sprintf_s(v, sizeof v, "%d", o);
    Log("eileen outfit: outfit %s (cutscenes %d)", o < 0 ? "Default" : o ? "Nurse" : "Dress", eo_eink);
    // inspection EO-002: marked saved only after a successful write (else retried at the next change / call)
    if (g_iniFallback) {
        Log("eileen outfit: SH4Refined.ini is not writable: the outfit is kept for this session only");
        g_eoSaved = o;  // Refined's fallback mode: no ini writes at all this session (as for every other setting)
    } else if (WritePrivateProfileStringA("EileenOutfit", "Outfit", v, g_iniPath)) {
        g_eoSaved = o;
    } else {
        Log("eileen outfit: could not write [EileenOutfit] Outfit (error %lu); tried again at the next change",
            GetLastError());
    }
}

// eo_menu_can: 1 = the old toggle row may be used now (Eileen here and idle)
static int __cdecl eo_menu_can_c(void)
{
    int r = 0;
    __try {
        r = eo_check(1);
        if (!r) {
            if (eo_getcur() != -1) return 1;
            r = -9;  // model not recognised
        }
        eo_why = r;
        r = 0;
    } __except (EoFault(GetExceptionInformation(), "eo_menu_can")) {
        r = 0;
    }
    return r;
}

// eo_menu_go: swap to the other outfit; it runs on the first unpaused frame
static int __cdecl eo_menu_go_c(void)
{
    int o;
    if (!eo_menu_can_c()) return 0;
    o = eo_cur ^ 1;
    eo_outfit = o;
    eo_auto_last = o;
    eo_frames = 0;
    eo_status = 1;
    eo_apply = 1;
    EoSaveOutfit();
    return 1;
}

// eo_menu_set(choice): -1 Default, 0 Dress, 1 Nurse (clamped); always allowed, only stores the choice. Dress / Nurse
// also switch "Cutscene Outfit" on (eo_eink 0 -> 1; 2 is kept). Returns the stored value.
static int __cdecl eo_menu_set_c(int v)
{
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    eo_outfit = v;
    if (v >= 0 && !eo_eink) eo_eink = 1;
    __try {
        EoSaveOutfit();
    } __except (EoFault(GetExceptionInformation(), "eo_menu_set")) {
    }
    return v;
}

// the table's API preserves ecx / edx
static __declspec(naked) void eo_menu_can(void)
{
    __asm {
        push ecx
        push edx
        call eo_menu_can_c
        pop edx
        pop ecx
        ret
    }
}

static __declspec(naked) void eo_menu_go(void)
{
    __asm {
        push ecx
        push edx
        call eo_menu_go_c
        pop edx
        pop ecx
        ret
    }
}

static __declspec(naked) void eo_menu_set(void)
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 0Ch]  // choice
        call eo_menu_set_c
        add esp, 4
        pop edx
        pop ecx
        ret
    }
}

// ---------------------------------------------------------------- 2. eo_stage (4F3490, world file list loader 4F3460)
// After the stub's own `push eax / call 573470`: entry = esi (current list entry), r = its result. Returns the eax the
// table leaves for 4F3496: r, or the eol.bin request's result, or eo_world (vanilla path).
static int __cdecl eo_stage_c(const DWORD *entry, int r)
{
    __try {
        if (*entry != EO_ID_EIL) return r;
        RzlOutfitWorld();  // v3.49 RZ349L: Randomizer > Eileen Outfit Random draws this world visit's outfit (rzlooks.inc)
        if (eo_outfit >= 0) {
            eo_world = 3;                              // this world loads both models
            return EO_CALL1(0x573470, EO_ID_EIL + 1);  // request eol.bin too
        }
        eo_world = (EO_FLAG != 0) + 1;  // vanilla: 5733F0 picked eil (bit0) or eol (bit1) by 13D19EC
        return eo_world;
    } __except (EoFault(GetExceptionInformation(), "eo_stage")) {
    }
    return r;
}

// ---------------------------------------------------------------- 3. eo_room (4F3514, room file loader 4F3510)
static void __cdecl eo_room_c(DWORD ret)
{
    int w;
    DWORD l;
    if (ret == 0x4F3826) return;  // the neighbour preload task's call: leave it alone
    __try {
        RzlOutfitRoom();  // v3.49 RZ349L: Randomizer > Eileen Outfit (a draw switched on mid-visit; Off / no Eileen: set outfit back)
        if (!(eo_scan() & 0x33)) eo_world = 0;  // no Eileen model in this world
        if (eo_apply) {                         // a pending swap is done by this room's spawn instead
            eo_apply = 0;
            eo_phase = 0;
            eo_frames = 0;
            eo_status = 5;
        }
        w = eo_mwant();
        l = eo_loaded;
        if (!(l & (1u << (w & 31))) && (l & 0x33)) {  // Eileen world without the wanted model: load it with the room
            if (EO_CALL1(0x573470, EO_ID_EIL + (DWORD)w)) {
                eo_blocked = 1;
                EoLogStatus();
                return;
            }
            ++eo_roomloads;
        }
        eo_put(w);
        eo_blocked = 0;
        EoLogStatus();
    } __except (EoFault(GetExceptionInformation(), "eo_room")) {
    }
}

// ---------------------------------------------------------------- 4. eo_gate (445BDB, 'call 56F570' in 445BD0)
// Returns the outfit used; keeps 13D19EC equal to it.
static int __cdecl eo_gate_c(void)
{
    int b, a;
    __try {
        b = eo_mwant();
        if (!(eo_phase == 1 && b == eo_target) && eo_live(EO_ID_EIL + (DWORD)b, NULL)) goto use;
        a = b ^ 1;  // eo_gate_other: the other model if resident, unless it is the one still loading
        if (eo_phase == 1 && a == eo_target) goto use;
        if (eo_live(EO_ID_EIL + (DWORD)a, NULL)) {
            b ^= 1;
            eo_blocked = 1;
        }
    use:
        eo_scan();
        eo_put(b);
        eo_cur = b;
        ++eo_builds;
        return b;
    } __except (EoFault(GetExceptionInformation(), "eo_gate")) {
    }
    return EO_FLAG;  // what 56F570 returns
}

// ---------------------------------------------------------------- 5. eo_tick (44ADE0, her task function entry)
static void __cdecl eo_tick_c(DWORD task)
{
    __try {
        if (eo_auto) {
            const int o = eo_outfit;
            if (o != eo_auto_last) {  // v4: Default (-1) too
                eo_auto_last = o;
                eo_frames = 0;
                eo_apply = 1;
            }
        }
        if (eo_apply && task == EO_U(0xFCE6B4)) eo_service(task);
    } __except (EoFault(GetExceptionInformation(), "eo_tick")) {
        eo_apply = 0;  // [not in the table] no fault loop
        eo_phase = 0;
        eo_frames = 0;
    }
    EoLogStatus();
}

// ---------------------------------------------------------------- 6. eo_fid (5733F0, file-id remap entry)
// id -> the id to return at once, or 0 = run the original 5733F0 with id.
static DWORD __cdecl eo_fid_c(DWORD id)
{
    __try {
        if (id == 0xF000F255) {  // eink.bin (char 133h): eisk.bin / eosk.bin
            int w;
            if (!eo_eink) return 0;
            w = eo_want();
            if (w < 0) return 0;
            if (w == 0 && eo_eink == 2) return 0;  // mode 2 + Dress: eink.bin as installed
            ++eo_eink_reqs;
            return 0xF000F250 + (DWORD)w;
        }
        if (id == 0xF000F250 || id == 0xF000F253 || id == 0xF000F259) {  // eisk / eihk / eis_zk
            if (!eo_eink) return 0;                                       // Off: by 13D19EC
            return id + (DWORD)eo_mwant();                                // normal id + 0/1
        }
    } __except (EoFault(GetExceptionInformation(), "eo_fid")) {
    }
    return 0;
}

// ---------------------------------------------------------------- 7. eo_csid (4F1C16, tail of 4F1B20)
// eo_cs_pick: the first of eo_cs_c[0..2] that is resident and live, 0 if none
static DWORD eo_cs_pick(void)
{
    int i;
    for (i = 0; i < 3; ++i)
        if (eo_cs_c[i] && eo_live(eo_cs_c[i], NULL)) return eo_cs_c[i];
    return 0;
}

// r = the stub's pushad block (r[0] edi = row sub 0, r[2] ebp = char id, r[4] ebx = row sub 1); r + 8 = the game's
// stack at 4F1C16 ([esp+1C] = row sub 2). Row order sub 0, 1, 2 (131h / 12Eh / 133h / 137h) or 1, 0, 2 (143h).
static void eo_csid_set(DWORD *r, DWORD f, int is143)
{
    if (is143) {
        r[0] = f + 0x10000;
        r[4] = f;
    } else {
        r[0] = f;
        r[4] = f + 0x10000;
    }
    r[8 + 7] = f + 0x20000;
    eo_cs_last_file = f;  // eo_csid_rec
    eo_cs_last_nurse = f == 0xF000F251 || f == 0xF000F254 || f == 0xF000F25A || f == 0xF000F059;
}

static void __cdecl eo_csid_c(DWORD *r)
{
    const DWORD id = r[2], row0 = r[0] & 0xF800FFFF;
    DWORD a, c;
    int w;
    __try {
        eo_cs_last_id = id;
        eo_cs_last_file = 0;
        eo_cs_last_nurse = EO_FLAG != 0;  // default = the game's flag
        if (id == 0x133) {  // eink (peephole / door viewer / endings)
            w = eo_want();
            if (w >= 0 && eo_eink && !(w == 0 && eo_eink == 2)) {  // wanted: eisk / eosk, the other one, then eink
                eo_cs_c[0] = 0xF000F250 + (DWORD)w;
                eo_cs_c[1] = eo_cs_c[0] ^ 1;
                eo_cs_c[2] = 0xF000F255;
            } else {  // Default / Off: eink first, eisk / eosk only if eink is not resident
                eo_cs_c[0] = 0xF000F255;
                eo_cs_c[1] = 0xF000F250 + (DWORD)eo_cs_last_nurse;
                eo_cs_c[2] = eo_cs_c[1] ^ 1;
            }
            a = eo_cs_pick();
            if (!a) return;
            if (a != 0xF000F255) ++eo_cs_count;
            eo_csid_set(r, a, 0);
            return;
        }
        if (id == 0x143) {  // the in-game model inside a cutscene (ls #9)
            if (!eo_cs143) return;
            w = eo_want();
            if (w < 0) w = eo_cs_last_nurse;  // Default: the game's flag
            eo_cs_c[0] = EO_ID_EIL + (DWORD)w;
            eo_cs_c[1] = eo_cs_c[0] ^ 1;
            eo_cs_c[2] = 0;
            if (eo_phase == 1) {  // our own read of eil / eol may still be in flight: never build from it
                a = EO_ID_EIL + (DWORD)eo_target;
                if (a == eo_cs_c[0]) eo_cs_c[0] = 0;
                if (a == eo_cs_c[1]) eo_cs_c[1] = 0;
            }
            a = eo_cs_pick();
            if (!a) return;
            if (a != EO_ID_EIL) ++eo_cs_count;
            eo_csid_set(r, a, 1);
            return;
        }
        if (id == 0x131) a = 0xF000F253;       // eihk / eohk
        else if (id == 0x12E) a = 0xF000F250;  // eisk / eosk
        else if (id == 0x137) a = 0xF000F259;  // eis_zk / eos_zk
        else return;
        if (row0 != a && row0 != a + 1) return;  // unexpected row: leave it alone
        c = row0;                                // Off: the game's pick first
        if (eo_eink) c = a + (DWORD)eo_mwant();  // v4: the chosen variant first
        eo_cs_c[0] = c;
        eo_cs_c[1] = c == a ? a + 1 : a;
        eo_cs_c[2] = 0;
        a = eo_cs_pick();
        if (!a) return;  // neither resident: vanilla row
        if (a != row0) ++eo_cs_count;
        eo_csid_set(r, a, 0);
    } __except (EoFault(GetExceptionInformation(), "eo_csid")) {
    }
}

// ---------------------------------------------------------------- 8. eo_cloth (4F2100, 'call 56F570' in 4F1EE0)
// edi = char id, ebx = model. Returns the nurse flag for the game's own cloth switch.
static int __cdecl eo_cloth_c(DWORD id, DWORD model)
{
    int f = EO_FLAG;  // what 56F570 returns
    __try {
        if (id != eo_cs_last_id || !eo_cs_last_file) return f;
        if (id != 0x133) return eo_cs_last_nurse;  // the model actually used
        if (eo_cs_last_nurse && eo_cs_cloth) EO_CALL2(0x445120, model, 2);  // 133h + eosk: the 12Eh dress cloth
        f = 0;  // the game skips its own cloth switch
    } __except (EoFault(GetExceptionInformation(), "eo_cloth")) {
    }
    return f;
}

// ---------------------------------------------------------------- 9. eo_load (4FC62A, 'call 56F580' in 4FC620)
static int __cdecl eo_load_c(int v)
{
    EO_FLAG = v;    // what 56F580 does
    eo_flagw = v;   // seen: eo_seen will not take it again
    eo_game = v != 0;
    return v;       // 56F580 returns its argument
}

// ---------------------------------------------------------------- naked stubs (the table's patch targets)
static const DWORD g_eoRemapSet = 0x573437, g_eoRemapKeep = 0x57343C, g_eoFnReq = 0x573470, g_eoStageRet = 0x4F3496,
                   g_eoRoomRet = 0x4F351E, g_eoTickRet = 0x44ADE6, g_eoFnFlag = 0x56F570, g_eoFidRet = 0x5733F8,
                   g_eoCsidRet = 0x4F1C1E;

// 1. eo_remap 57342F (was `cmp esi,F000F058`; then jne 57343C / 573437 mov esi,F000F059): keep eil.bin while overriding
static __declspec(naked) void hkEoRemap(void)
{
    __asm {
        cmp dword ptr [eo_outfit], 0
        jl orig
        jmp dword ptr [g_eoRemapKeep]  // 57343C: return id unchanged
    orig:
        cmp esi, 0F000F058h
        jne keep
        jmp dword ptr [g_eoRemapSet]   // 573437
    keep:
        jmp dword ptr [g_eoRemapKeep]
    }
}

// 2. eo_stage 4F3490 (was `push eax / call 573470`; the arg is popped at 4F349C): back to 4F3496 (Playable Eileen's
// stage-list stub may sit there), eax as the table leaves it
static __declspec(naked) void hkEoStage(void)
{
    __asm {
        push eax
        call dword ptr [g_eoFnReq]
        pushad
        cld
        push eax
        push esi
        call eo_stage_c
        add esp, 8
        mov dword ptr [esp + 1Ch], eax  // pushad's eax slot
        popad
        jmp dword ptr [g_eoStageRet]
    }
}

// 3. eo_room 4F3514 (was `mov [ECB324],0`): all registers and flags kept; [ebp+4] = 4F3510's return address
static __declspec(naked) void hkEoRoom(void)
{
    __asm {
        pushfd
        pushad
        cld
        push dword ptr [ebp + 4]
        call eo_room_c
        add esp, 4
        popad
        popfd
        mov dword ptr ds:[0ECB324h], 0
        jmp dword ptr [g_eoRoomRet]
    }
}

// 4. eo_gate 445BDB (was `call 56F570`): eax = outfit used; everything else kept
static __declspec(naked) void hkEoGate(void)
{
    __asm {
        push ecx
        push edx
        cld
        call eo_gate_c
        pop edx
        pop ecx
        ret
    }
}

// 5. eo_tick 44ADE0 (was `push esi / push edi / mov edi,[esp+0C]`): [esp+28h] after pushfd + pushad = the task
static __declspec(naked) void hkEoTick(void)
{
    __asm {
        pushfd
        pushad
        cld
        push dword ptr [esp + 28h]
        call eo_tick_c
        add esp, 4
        popad
        popfd
        push esi
        push edi
        mov edi, dword ptr [esp + 0Ch]
        jmp dword ptr [g_eoTickRet]
    }
}

// 6. eo_fid 5733F0 (was `push esi / mov esi,eax / call 56F570`): eax = id in / out, ecx / edx kept
static __declspec(naked) void hkEoFid(void)
{
    __asm {
        push ecx
        push edx
        push eax
        cld
        call eo_fid_c
        test eax, eax
        je orig
        add esp, 4
        pop edx
        pop ecx
        ret                            // straight back to 573470 / 573810
    orig:
        pop eax
        pop edx
        pop ecx
        push esi
        mov esi, eax
        call dword ptr [g_eoFnFlag]    // 56F570
        jmp dword ptr [g_eoFidRet]     // 5733F8
    }
}

// 7. eo_csid 4F1C16 (was `mov eax,[esp+24] / mov edx,[esp+1C]`): edi / ebx / [esp+1C] may be rewritten
static __declspec(naked) void hkEoCsid(void)
{
    __asm {
        pushad
        cld
        push esp                       // -> the pushad block
        call eo_csid_c
        add esp, 4
        popad
        mov eax, dword ptr [esp + 24h]
        mov edx, dword ptr [esp + 1Ch]
        jmp dword ptr [g_eoCsidRet]
    }
}

// 8. eo_cloth 4F2100 (was `call 56F570`): edi = char id, ebx = model; eax out, everything else kept
static __declspec(naked) void hkEoCloth(void)
{
    __asm {
        push ecx
        push edx
        cld
        push ebx
        push edi
        call eo_cloth_c
        add esp, 8
        pop edx
        pop ecx
        ret
    }
}

// 9. eo_load 4FC62A (was `call 56F580`): [esp+4] = the loaded save's flag; eax = it, everything else kept
static __declspec(naked) void hkEoLoad(void)
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 0Ch]
        call eo_load_c
        add esp, 4
        pop edx
        pop ecx
        ret
    }
}

// ---------------------------------------------------------------- install
static void EoLogDiff(const Site *s)
{
    char a[100], b[100];
    int i;
    a[0] = b[0] = 0;
    for (i = 0; i < s->len && i < 32; ++i) {
        sprintf_s(a + 3 * i, sizeof a - 3 * i, "%02X ", s->stock[i]);
        sprintf_s(b + 3 * i, sizeof b - 3 * i, "%02X ", ((const BYTE *)(ULONG_PTR)s->va)[i]);
    }
    Log("eileen outfit: %08lX is not stock: expected %s", s->va, a);
    Log("eileen outfit: %08lX found               %s", s->va, b);
}

static void EoPublish(void)
{
    eo_api[1] = (DWORD)(ULONG_PTR)eo_menu_can;
    eo_api[2] = (DWORD)(ULONG_PTR)eo_menu_go;
    eo_api[3] = (DWORD)(ULONG_PTR)&eo_cur;
    eo_api[4] = (DWORD)(ULONG_PTR)&eo_status;
    eo_api[5] = 0x34564F45;  // 'EOV4'
    eo_api[6] = (DWORD)(ULONG_PTR)eo_menu_set;
    eo_api[7] = (DWORD)(ULONG_PTR)&eo_outfit;
    MemoryBarrier();
    eo_api[0] = 0x32564F45;  // 'EOV2'
    SH4R_Extra[3] = (DWORD)(ULONG_PTR)eo_api;
    SH4R_Extra[2] |= 4;  // owns bit 2: the table's Eileen Outfit stays off
}

static void EileenOutfitInstall(void)
{
    // the table's 9 AOBs at their VAs (eo_stage: 4F3480..4F3495 only; Playable Eileen patches 4F3496)
    static const Site kSites[] = {
        { 0x57342F, { 0x81, 0xFE, 0x58, 0xF0, 0x00, 0xF0, 0x75, 0x05, 0xBE, 0x59, 0xF0, 0x00, 0xF0 }, 13 },  // eo_remap
        { 0x4F3480, { 0x85, 0xC0, 0x74, 0x37, 0x56, 0x8B, 0x30, 0x8B, 0x06, 0x3D, 0xFF, 0xFF, 0xFF, 0x0F, 0x74, 0x16, 0x50,
                      0xE8, 0xDA, 0xFF, 0x07, 0x00 }, 22 },                                              // eo_stage
        { 0x4F3510, { 0x55, 0x8B, 0xEC, 0x53, 0xC7, 0x05, 0x24, 0xB3, 0xEC, 0x00, 0x00, 0x00, 0x00, 0x00 }, 14 },  // eo_room
        { 0x445BD0, { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x57, 0x33, 0xFF, 0x89, 0x7D, 0xFC, 0xE8, 0x90, 0x99, 0x12, 0x00, 0x85,
                      0xC0, 0x74, 0x05, 0xBF, 0x01, 0x00, 0x00, 0x00 }, 25 },                            // eo_gate
        { 0x44ADE0, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x66, 0x83, 0xBF, 0x10, 0x04, 0x00, 0x00, 0x00, 0x75, 0x05, 0xE8,
                      0xBB, 0xFD, 0xFF, 0xFF, 0x8B, 0xB7, 0x08, 0x04, 0x00, 0x00 }, 27 },                // eo_tick
        { 0x5733F0, { 0x56, 0x8B, 0xF0, 0xE8, 0x78, 0xC1, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x40, 0x81, 0xFE, 0x53, 0xF2, 0x00,
                      0xF0 }, 18 },                                                                      // eo_fid
        { 0x4F1C16, { 0x8B, 0x44, 0x24, 0x24, 0x8B, 0x54, 0x24, 0x1C, 0x8B, 0xC8, 0x89, 0x31, 0x89, 0x79, 0x04 }, 15 },  // eo_csid
        { 0x4F2100, { 0xE8, 0x6B, 0xD4, 0x07, 0x00, 0x85, 0xC0, 0x74, 0x42, 0x81, 0xFF, 0x31, 0x01, 0x00, 0x00 }, 15 },  // eo_cloth
        { 0x4FC620, { 0x0F, 0xBF, 0x0D, 0x82, 0xCD, 0x07, 0x01, 0x56, 0x57, 0x51, 0xE8, 0x51, 0x2F, 0x07, 0x00 }, 15 },  // eo_load
    };
    // entries of the game functions called here (and 56F580, whose call eo_load replaces); a range Refined itself hooks
    // is skipped (calls go through it)
    static const Site kFns[] = {
        { 0x56F570, { 0xA1, 0xEC, 0x19, 0x3D, 0x01, 0xC3 }, 6 },                         // get outfit flag
        { 0x56F580, { 0x8B, 0x44, 0x24, 0x04, 0xA3, 0xEC, 0x19, 0x3D, 0x01, 0xC3 }, 10 }, // set outfit flag
        { 0x5736D0, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C }, 6 },                         // resident sub-file
        { 0x573470, { 0x81, 0xEC, 0x10, 0x02, 0x00, 0x00 }, 6 },                         // file request
        { 0x573370, { 0xE9, 0xAB, 0x61, 0xFF, 0xFF }, 5 },                               // file queue idle
        { 0x44D9E0, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x2C }, 9 },       // store her live state
        { 0x52B020, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, 5 },                               // unregister + destroy a task
        { 0x5538A0, { 0x8B, 0x54, 0x24, 0x04, 0x85, 0xD2 }, 6 },                         // set a task's function
        { 0x44ABB0, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x38 }, 9 },       // her task init (edi = task)
        { 0x445120, { 0x56, 0xE8, 0x2A, 0xE9, 0x10, 0x00 }, 6 },                         // cutscene cloth task
    };
    static const struct { DWORD va; int len; BYTE op; const void *to; } kPatch[9] = {
        { 0x57342F, 6, 0xE9, hkEoRemap }, { 0x4F3490, 6, 0xE9, hkEoStage }, { 0x4F3514, 10, 0xE9, hkEoRoom },
        { 0x445BDB, 5, 0xE8, hkEoGate },  { 0x44ADE0, 6, 0xE9, hkEoTick },  { 0x5733F0, 8, 0xE9, hkEoFid },
        { 0x4F1C16, 8, 0xE9, hkEoCsid },  { 0x4F2100, 5, 0xE8, hkEoCloth }, { 0x4FC62A, 5, 0xE8, hkEoLoad },
    };
    BYTE saved[9][16];
    int i, bad = 0, ok = 1, done = 0;
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            EoLogDiff(&kSites[i]);
            bad = 1;
        }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i) {
        if (EmOurPatch(kFns[i].va, kFns[i].len)) continue;  // Refined's own hook there (calls go through it)
        if (!SiteOk(&kFns[i]) && PeHookedCallee(kFns[i].va, "eileen outfit")) continue;  // v3.34 PEC334
        if (!SiteOk(&kFns[i])) {
            EoLogDiff(&kFns[i]);
            bad = 1;
        }
    }
    if (bad) {
        Log("eileen outfit: stock bytes differ (another mod, or the cheat table's Eileen Outfit attached first?): nothing "
            "installed, the game's own outfit");
        return;
    }
    // data: the ini's choice and switches, the table's defaults for the rest
    eo_outfit = g_eoIniOutfit < -1 ? -1 : g_eoIniOutfit > 1 ? 1 : g_eoIniOutfit;
    eo_auto = g_eoIniAuto ? 1 : 0;
    eo_eink = g_eoIniEink < 0 || g_eoIniEink > 2 ? 1 : g_eoIniEink;
    g_eoSaved = eo_outfit;
    // all or nothing: keep the stock bytes, put every written site back if one write fails
    for (i = 0; i < 9; ++i) memcpy(saved[i], (const void *)(ULONG_PTR)kPatch[i].va, kPatch[i].len);
    for (i = 0; i < 9 && ok; ++i) {
        ok = PokeBranch(kPatch[i].va, kPatch[i].op, kPatch[i].to, kPatch[i].len);
        if (ok) ++done;
    }
    if (!ok) {
        int lost = 0;  // a restore can fail too
        for (i = 0; i < done; ++i)
            if (!Poke(kPatch[i].va, saved[i], kPatch[i].len) ||
                memcmp((const void *)(ULONG_PTR)kPatch[i].va, saved[i], kPatch[i].len)) {
                ++lost;
                Log("eileen outfit: %08lX could not be put back", kPatch[i].va);
            }
        if (!lost) {
            Log("eileen outfit: a patch could not be written (VirtualProtect failed): the %d written site(s) are stock "
                "again; the game's own outfit", done);
            return;
        }
        // fail closed: the sites that stayed patched run the engine (their stubs are complete), the table keeps out
        // (its AOBs cannot match them), and the pause row drives it
        EoPublish();
        Log("eileen outfit: %d site(s) stayed patched after a failed install: the outfit engine stays on for them", lost);
        return;
    }
    EoPublish();
    Log("eileen outfit: installed, 9 patches (57342F 4F3490 4F3514 445BDB 44ADE0 5733F0 4F1C16 4F2100 4FC62A); outfit "
        "%s, instant %d, cutscenes %d; eo_api at %p",
        eo_outfit < 0 ? "Default" : eo_outfit ? "Nurse" : "Dress", eo_auto, eo_eink, (void *)eo_api);
}
