// ================================================================ v3.8.0: Save Banks (Save Banks v7 in C)
// A translation of the cheat table's entry 6040 "Save Banks" v7 (staging/save_banks/ct_6040_save_banks.txt, CT lines
// 39415-41420, Auto Assembler). Plan: staging/save_banks/PLAN.md (B1-B8); RE: staging/extra_menu/RE.md section 3.
// The table's symbol names are kept (sb_*). All addresses are absolute VAs of the GOG v7 exe. Included by
// sh4windowmod.c after extramenu.inc (uses Site / SiteOk, PokeBranch / Poke, Log, SbSetBank, g_sbOn / g_sbBank /
// SB_MAX / g_sbInstalled, SH4R_Extra, SH4WM_Patches and extramenu.inc's EmOurPatch).
//
// Files: bank 1 = <game>\save\sh4.sav (vanilla), bank N = <game>\save\sh4_bankN.sav (N = 2..SB_MAX).
// Patches (the table's byte ranges): 407D2D sb_name (8), 56F158 sb_latch (10), 570F30 sb_input (5), 56FF20 sb_draw (6),
// 4FBEB0 sb_catrd (6), 4FBDC0 sb_catwr (10), 56FDA0 sb_lbar (5), 56F440 sb_dexec (8).
// Not ported (PLAN 1): the v7 extra delete inputs sb_dkey 57122A, sb_ldel 571C50, sb_dhint 570320 (those sites stay
// stock: the save list deletes with Del as stock), the sb_api block (Refined's g_sbBank / SB_MAX instead), the table's
// user controls (fixed to the table's defaults below), sb_force_off and the [DISABLE] guard (no disable here).
// g_sbOn (Option 6, [SaveBanks] Enabled) is followed LIVE (review SB-001): while it is 0 every hook passes through to
// the stock code (sb_active = 1, sh4.sav, no label, no bank row, no input handling, no write block, no backup); only
// invisible bookkeeping runs then (sb_cat_bank / sb_boot_ok at a catalog read, the per-bank backup flag at a save).
//
// v3.47 SB347 (USER_FEEDBACK_v346.md item 4; staging\v347_feedback\w3_banks\PLAN.md): the Bank row draws "\hBank N", Latin
// glyphs of the list's own font, so [Textures] MenuFont covers it like the rows (v3.8 .. v3.46 drew plain ASCII: the
// game's special monospace glyphs, the old stroke font); the "Q/E L1/R1:" hint is gone; the row is centred over the list
// (savebanks_sim.h SbLayout); the mouse's hover box is bar + arrows, a click on the left arrow = the previous bank, on the
// right arrow (or the bar, as before) = the next; a click on the row while the list has the input selects the row
// instead of confirming the list's slot. Keys and pad unchanged. No new patch site.
// v3.48 SB348 (USER_FEEDBACK_v347.md section B; staging\v348_feedback\w2_banks\PLAN.md): on the highlighted Bank row Left /
// Right (keys, D-pad, the stick: the game's direction bits 5542A0 / 5542C0) = 1 bank, Q / E and the pad's shoulders (buttons
// 4 / 5: LB / RB, L1 / R1, L / R) = 10 banks, clamped, all with the accelerating hold repeat (savebanks_sim.h SbRepStep); on
// a slot Q / E / L1 / R1 stay 1 bank. Round 2 (the user, 2026-11-25): the shoulders only, the triggers do nothing.
// Holding the left mouse button on a blue arrow repeats the same way (SbMhFrame). The list's own input (570F30: Left /
// Right page it by 10 slots) is skipped while the row has the input, as before. The per-step "save banks: bank N" is
// verbose (sh4windowmod.c SbSetBank, g_sbListStep); one normal line per settled change (SbSettle). No new patch site.
#include "savebanks_sim.h"  // v3.47 SB347

#define SB_I(a) (*(volatile int *)(a))
#define SB_U(a) (*(volatile DWORD *)(a))
#define SB_B(a) (*(volatile BYTE *)(a))
typedef int(__cdecl *SbFn0)(void);
typedef int(__cdecl *SbFn1)(int);
typedef int(__cdecl *SbFn4)(int, int, int, int);
typedef void(__cdecl *SbText5)(int, int, int, int, int);
typedef void(__cdecl *SbRect10)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void(__cdecl *SbFlush14)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                 DWORD, DWORD);
typedef void(__cdecl *SbTri3)(void *, void *, void *);
#define SB_CALL0(va) (((SbFn0)(va))())
#define SB_CALL1(va, a) (((SbFn1)(va))((int)(a)))
#define SB_CALL4(va, a, b, c, d) (((SbFn4)(va))((int)(a), (int)(b), (int)(c), (int)(d)))
#define SB_SOUND(id) SB_CALL1(0x56CC40, id)  // 9C40 cursor, 9C44 buzzer
#define SB_IOBUF SB_U(0x107CD68)             // save I/O buffer: only allocated while a save / load list is open
#define SB_FRAME SB_U(0x13D1A08)             // menu frame counter (both menus increment it every frame)
#define SB_CURSOR SB_I(0x13D1AB0)            // list cursor (0..29)
#define SB_ONE 0x3F800000u                   // 1.0f
#define SB_MONE 0xBF800000u                  // -1.0f

// ---------------------------------------------------------------- the table's controls, at their defaults (CT 395-509)
static const int sb_flip_on = 1, sb_row_on = 1;
static const int sb_lbl_show = 1, sb_lbl_align = 1;  // v3.47 SB347: centred (was x 1C2h right-aligned); x / y: sb_L
static const DWORD sb_lbl_col = 0x808080;
// v3.47 SB347: the bar x (was 146h..1C8h) and the hint (sb_hint_*, "Q/E L1/R1:") are gone: savebanks_sim.h's SbLayout
static const DWORD sb_kb_prev = 0x10, sb_kb_next = 0x12;  // DIK Q / E
static const DWORD sb_pad_prev = 4, sb_pad_next = 5;      // DirectInput buttons 4 / 5 (L1 / R1)
static const int sb_key_prev = 0, sb_key_next = 0;        // optional action masks: off (the table's default)
// v3.48 SB348: the hold repeat's sb_rep_delay 15 / mid 45 / fast 90 / i1 4 / i2 2 / i3 1 are savebanks_sim.h's kSbRep
static const int sb_arw_on = 1, sb_arw_bob = 0;  // v3.47 SB347: gap / length in savebanks_sim.h (SB_ARW_GAP / _W)
static const DWORD sb_arw_col = 0x80600000;
// sb_bank = g_sbBank (SbSetBank writes it, the ini and the log); sb_max = SB_MAX; sb_del_bk = 1 (first delete per bank,
// again after a save into it)

// ---------------------------------------------------------------- state (CT 395-536, 1338-1408)
static int sb_active = 1;      // the bank whose file 407C10 names now (latched at a list's catalog read)
static int sb_status;          // 1 latched, 2 / 3 bank 1 forced, 5 flipped, 6 flipped to a missing file, -1 bad bank,
                               // -2 path too long, -3 write blocked, -4 flip to an unreadable file
static int sb_latches, sb_names, sb_blocked, sb_flips;
static int sb_cat_bank = 1;    // the bank the RAM catalog 107C1A0 was read from
static int sb_wblock;          // 1 = no file access (invalid name) until the next catalog read / flip
static int sb_boot_ok;         // a catalog has been read from sh4.sav (v5 gate; 0 at dll start, as the table enabled
                               // before the boot card check)
static int sb_onrow;           // the Bank row is selected
static DWORD sb_last_in;       // SB_FRAME at the last list input
static int sb_m_over, sb_quiet, sb_entered;  // v3.48 SB348: sb_hdir / sb_hold / sb_rep_next -> sb_rk (SbRep)
static SbRep sb_rk, sb_rm;     // v3.48 SB348: the keys / pad hold repeat, the mouse hold's repeat
static SbMh sb_mh;             // v3.48 SB348: the arrow the left mouse button holds
static SbSettle sb_set;        // v3.48 SB348: the bank change not logged yet
static int sb_list_kind;       // v3.48 SB348: 1 save list, 2 load list (sb_latch_c)
static volatile LONG g_sbQSet;  // v3.48 SB348: log quota
static float sb_rect[4];       // mouse rect for 405760 (512 x 448 menu space): x, y, w, h
static SbLayout sb_L;          // v3.47 SB347: the row's layout (SaveBanksInstall; savebanks_sim.h)
static int sb_m_where;         // v3.47 SB347: the part of the row under the mouse (SB_HIT_*)
static int sb_lbl_w = -1;      // v3.47 SB347: the label's width (5587F0 after the draw), -1 = not drawn yet
static volatile LONG g_sbQMouse, g_sbQWide;  // v3.47 SB347: log quotas
static int sb_lbl_logged;     // v3.47 SB347: the row's first draw was logged
static DWORD sb_vtx[21];       // 3 vertices for 563F40, 1Ch bytes each: x, y, 1.0, 1.0, colour, 0, 0
static char sb_lbl_buf[0x40];
static int sb_dabort, sb_dels, sb_dfails, sb_bk_count, sb_del_slot = -1, sb_del_bank;
static DWORD sb_bk_err;
static DWORD sb_bk_bits[32];   // one bit per bank 0..1023: backed up in this session
static char sb_bk_src[0x104], sb_bk_path[0x140];
static int g_sbBroken;         // [not in the table] a failed install left sites patched: they pass through to stock
static volatile LONG g_sbInSkip, g_sbLbarSkip, g_sbDxFail;  // read by the naked stubs

static int SbFault(EXCEPTION_POINTERS *e, const char *where)
{
    static int n;
    if (++n <= 5)
        Log("save banks: %s faulted (code %08lX at %p)", where, e->ExceptionRecord->ExceptionCode,
            e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// banks Off (or a broken install): the hooks pass through, and the file is sh4.sav (sb_active = 1)
static int SbOff(void)
{
    if (g_sbOn && !g_sbBroken) return 0;
    sb_active = 1;
    return 1;
}

static DWORD SbF(float f)  // float bits for the game's float arguments (the table's fild / fstp)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// v3.48 SB348: the settled bank change, one normal line: 1 s after its last step (SbLogTick: every frame end, and every
// list input frame), or at once (force) when a list opens again, a save writes or a delete starts. Game thread only.
static void SbLogFlush(int force)
{
    char line[160];
    if (!SbSettleDue(&sb_set, GetTickCount(), force)) return;
    sb_set.open = 0;
    if (SbSettleLine(&sb_set, sb_active, sb_list_kind == 1 ? "save" : sb_list_kind == 2 ? "load" : "save / load", line,
                     (int)sizeof line) < 0)
        return;
    if (LogQuota(&g_sbQSet, 300)) Log("save banks: %s", line);
    else LogV("save banks: %s", line);  // (300 changes in a session: the rest verbose)
}
static void SbLogTick(void) { SbLogFlush(0); }  // v3.48 SB348: FrameEndC (sh4windowmod.c), every frame

// 407D60 (__thiscall, ecx = &handle; access, disposition; ret 8): the game's CreateFileA wrapper, path from 407C10
static int SbOpen(HANDLE *fh)
{
    int r;
    __asm {
        push 3              // OPEN_EXISTING
        push 80000000h      // GENERIC_READ
        mov ecx, fh
        mov eax, 407D60h
        call eax
        mov r, eax
    }
    return r;
}

// 4081A0 (__thiscall, ecx = &handle): CloseHandle if it was opened
static void SbClose(HANDLE *fh)
{
    __asm {
        mov ecx, fh
        mov eax, 4081A0h
        call eax
    }
}

// ---------------------------------------------------------------- file name (407D2D, inside 407C10; CT 549-607)
// In: edi = end of "<exe dir>\save\" (its 0 byte), ebx = path buffer - 1. The game continues at 407D35 with
// mov ecx,[esp+224] / mov edx,[5B744C] ("sav"+0) / xor ecx,[ebp+4] / mov [edi+4],edx. Returns the new edi.
static BYTE *SbNameBad(BYTE *edi)
{
    *edi = '|';  // "|" + "sav": invalid file name, CreateFileA fails, nothing is read or written
    return edi - 3;
}

static BYTE *__cdecl sb_name_c(BYTE *edi, BYTE *ebx)
{
    __try {
        if (!SbOff()) {
            if (sb_wblock) {
                ++sb_blocked;
                return SbNameBad(edi);
            }
            if (sb_active > 1) {
                char num[12];
                int n;
                if ((DWORD)(edi - ebx) > 230) {  // path length + 1; the buffer is 260 bytes
                    sb_status = -2;
                    return SbNameBad(edi);
                }
                memcpy(edi, "sh4_bank", 8);
                n = sprintf_s(num, sizeof num, "%u", (unsigned)sb_active);
                memcpy(edi + 8, num, n);
                edi[8 + n] = '.';
                ++sb_names;
                return edi + 8 + n - 3;  // the game's mov [edi+4],"sav"+0 lands right after the dot
            }
        }
        *(DWORD *)edi = *(const DWORD *)0x5B7448;  // original: "sh4."
    } __except (SbFault(GetExceptionInformation(), "sb_name")) {
    }
    return edi;
}

// ---------------------------------------------------------------- latch (56F158, first step of 56F150; CT 609-645)
static void __cdecl sb_latch_c(DWORD ret)
{
    int b;
    SbLogFlush(1);  // v3.48 SB348: the last list's change, before this list's lines
    sb_wblock = 0;  // a fresh catalog read starts
    sb_onrow = 0;   // every menu opens on the list, not on the Bank row
    if (SbOff()) return;
    b = g_sbBank;
    if (b < 1 || b > SB_MAX || b > 999) {
        SbSetBank(1);
        sb_status = -1;
        b = 1;
    } else if (ret == 0x5711BA || ret == 0x571BE2) {  // save menu 571130 (call at 5711B5) / load menu 571B40 (571BDD)
        if (sb_boot_ok) {
            sb_status = 1;
        } else {  // v5: no sh4.sav catalog read yet
            sb_status = 3;
            b = 1;
        }
        sb_list_kind = ret == 0x5711BA ? 1 : 2;  // v3.48 SB348
        Log("save banks: %s list on bank %d%s", ret == 0x5711BA ? "save" : "load", b,
            sb_status == 3 ? " (sh4.sav not read yet)" : "");
    } else {  // any other caller (boot card check 570B90): sh4.sav
        sb_status = 2;
        b = 1;
    }
    sb_active = b;
    ++sb_latches;
}

// ---------------------------------------------------------------- catalog read (4FBEB0 entry; CT 647-661)
// 4FBEB0(idx) decrypts the I/O buffer (just filled by a read of the sb_active file) into the RAM catalog.
static void __cdecl sb_catrd_c(void)
{
    SbOff();
    sb_cat_bank = sb_active;
    if (sb_active == 1) sb_boot_ok = 1;  // v5: a catalog was read from sh4.sav -> banks may be used from now on
}

// ---------------------------------------------------------------- catalog write (4FBDC0 entry; CT 663-685)
// 4FBDC0(idx) encrypts the RAM catalog into the I/O buffer; the caller then writes it as the file's global block.
static void __cdecl sb_catwr_c(DWORD ret)
{
    const int off = SbOff(), a = sb_active;
    SbLogFlush(1);  // v3.48 SB348: the bank change before the save / delete's lines
    if (ret != 0x56F49F && (unsigned)a <= 999)  // a save (56F2FA / 56F3DA), not the delete (56F480): the next
        sb_bk_bits[a >> 5] &= ~(1u << (a & 31));  // delete in this bank backs the file up again
    if (off) return;  // banks Off: no write block (stock)
    if (sb_cat_bank != sb_active) {
        static int n;
        sb_wblock = 1;  // the write and everything after it fails until the next read
        sb_status = -3;
        if (++n <= 20)
            Log("save banks: write blocked: the catalog in memory is bank %d's, the file is bank %d's", sb_cat_bank,
                sb_active);
    }
}

// ---------------------------------------------------------------- list input helpers (CT 837-1037)
// v3.48 SB348: a key of the game's Left / Right (gInputButtonKeys[2] / [3], 0x5FF2E8 / 0x5FF2EC: DIK codes) is held: a
// direction step then counts as the keyboard's in the log (else the pad's: D-pad, stick)
static int sb_kb_dir(const BYTE *in)
{
    const volatile int *const keys = (const volatile int *)0x5FF2E8;
    int i;
    for (i = 0; i < 2; ++i) {
        const int k = keys[i];
        if (k > 0 && k < 0x100 && (in[0x50 + k] & 0x80)) return 1;
    }
    return 0;
}

// sb_dir: returns the held step, *press = the new-press step (0 none or both), *src = SB_SRC_KEYS / SB_SRC_PAD.
// Sources: keyboard DIK sb_kb_prev / next and pad buttons straight from the input object [6E8EA4] (+50h+DIK, 80h = held;
// +180h+button, 1 = held), the optional action masks sb_key_prev / next (held mask 554230), and on the Bank row Left /
// Right (held dirs 5542A0 incl. the stick; new presses 5542C0).
// v3.48 SB348: on the Bank row Left / Right = 1, Q / E and buttons 4 / 5 (the shoulders) = SB_BIG (savebanks_sim.h SbPick);
// on a slot Q / E / buttons 4 / 5 = 1 as v3.47. The triggers (buttons 6 / 7) are not read (round 2).
static int sb_dir(int *press, int *src)
{
    int q = 0, e = 0, b4 = 0, b5 = 0, kdir = 0, h1, p1 = 0, h10, a, held;
    const BYTE *in = (const BYTE *)(ULONG_PTR)SB_U(0x6E8EA4);  // 0 before the input system exists
    if (in) {
        q = sb_kb_prev < 0x100 && (in[sb_kb_prev + 0x50] & 0x80);
        e = sb_kb_next < 0x100 && (in[sb_kb_next + 0x50] & 0x80);
        b4 = sb_pad_prev < 0x80 && in[sb_pad_prev + 0x180];
        b5 = sb_pad_next < 0x80 && in[sb_pad_next + 0x180];
        if (sb_onrow) kdir = sb_kb_dir(in);
    }
    a = SB_CALL1(0x554230, 0);  // held action mask (1236C30)
    h1 = ((sb_key_next & a) != 0) - ((sb_key_prev & a) != 0);
    if (!sb_onrow) {  // v3.47: 1 bank on a slot
        const int k = (e || b5) - (q || b4);
        *press = 0;
        *src = q || e ? SB_SRC_KEYS : SB_SRC_PAD;
        return k ? k : h1;
    }
    a = SB_CALL1(0x5542A0, 0);  // held directions incl. the stick (1236C3C)
    if (!h1) h1 = ((a & 8) != 0) - ((a & 4) != 0);  // Right / Left
    a = SB_CALL1(0x5542C0, 0);  // new direction presses (1236C40): a tap shorter than a frame
    p1 = ((a & 8) != 0) - ((a & 4) != 0);
    h10 = (e || b5) - (q || b4);
    SbPick(h1, p1, h10, 0, &held, press);
    *src = (h10 ? q || e : kdir) ? SB_SRC_KEYS : SB_SRC_PAD;
    return held;
}

// sb_flip(step): the bank step banks on at once (v3.48 SB348: clamped to the first / last bank, savebanks_sim.h
// SbTarget), its catalog read from its file (CT 963-1037). 1 = the bank changed.
static int sb_flip(int dir)
{
    int n;
    if (!SB_IOBUF || !sb_boot_ok) goto no;  // only while a list is open; v5: no sh4.sav catalog read yet: no banks
    n = SbTarget(sb_active, dir, SB_MAX);   // v3.48 SB348 (was: n = sb_active + dir, out of range = no)
    if (n == sb_active) goto no;
    g_sbListStep = 1;  // v3.48 SB348: SbSetBank's per-step line verbose
    SbSetBank(n);
    g_sbListStep = 0;
    sb_active = n;
    sb_wblock = 0;
    ++sb_flips;
    if (SB_CALL4(0x408920, 0, 0, SB_IOBUF, 0x5E4) == 1) {  // the global block of the new file (read only, MD5 checked)
        SB_CALL1(0x4FBEB0, 0);                             // decrypt into 107C1A0 (sb_catrd sets sb_cat_bank)
        sb_status = 5;
    } else {  // empty ONLY if the file does not exist; any other failure blocks writes
        HANDLE fh = INVALID_HANDLE_VALUE;
        DWORD e = 0;
        const int opened = SbOpen(&fh);
        if (!opened) e = GetLastError();
        if (!opened && (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)) {
            memset((void *)0x107C1A0, 0, 0x5C0);  // header + 30 catalog entries, options at +5C0 kept
            SB_U(0x107C1A0) = 0x5E4;
            sb_cat_bank = sb_active;
            sb_status = 6;
        } else {  // the file exists but could not be read / has a bad MD5
            SbClose(&fh);
            sb_wblock = 1;            // the RAM catalog still belongs to sb_cat_bank: no file access until the next read
            g_sbListStep = 1;         // v3.48 SB348
            SbSetBank(sb_cat_bank);   // the next menu opens on the bank that is really shown, not on the bad file
            g_sbListStep = 0;
            sb_status = -4;
            Log("save banks: bank %d's file exists but cannot be read (error %lu): the list keeps bank %d's catalog, "
                "writes are blocked", n, opened ? 0 : e, sb_cat_bank);
            goto no;
        }
    }
    SB_U(0x13D1AE0) = 0;  // load menu "read error" footer off
    SB_SOUND(0x9C40);     // cursor sound
    return 1;
no:
    if (!sb_quiet) SB_SOUND(0x9C44);  // v4: a held key that reaches bank 1 / SB_MAX does not buzz on every repeat
    return 0;
}

// v3.48 SB348: one step through sb_flip, counted for the settled-change line (src SB_SRC_*); 1 = the bank changed
static int sb_step(int step, int src)
{
    const int was = sb_active;
    if (!sb_flip(step)) return 0;
    SbSettleStep(&sb_set, was, src, step >= SB_BIG || step <= -SB_BIG, GetTickCount());
    return 1;
}

// v3.48 SB348: the row's hold state starts over (a new visit of the row or of the list)
static void sb_hold_reset(void)
{
    SbRepReset(&sb_rk);
    SbRepReset(&sb_rm);
    sb_mh.part = SB_HIT_NONE;
}

// v3.47 SB347: the part of the Bank row under the mouse, through 405760 (Refined's HitRectF maps the 4:3 menu area, as for
// the list rows); the boxes are SbLayout's (no overlap: the first hit wins)
static int sb_mouse_where(void)
{
    int in[3], i;
    const SbBox *b[3];
    b[0] = &sb_L.left;
    b[1] = &sb_L.bar;
    b[2] = &sb_L.right;
    for (i = 0; i < 3; ++i) {
        SbBoxRect(b[i], sb_rect);
        in[i] = ((int(__cdecl *)(float *))0x405760)(sb_rect);  // mouse inside the rect (as the list rows, 571050)
    }
    return SbHitOf(in[0], in[1], in[2]);
}

// ---------------------------------------------------------------- list input (570F30 entry; CT 687-835)
// 570F30(arg) with eax = list index 0; returns the cursor [13D1AB0]. Called once per frame in the list state only.
// 1 = 570F30 is skipped this frame (the Bank row has the input), 0 = the list's own input runs.
static int sb_input_body(void)
{
    const DWORD frame = SB_FRAME, d = frame - sb_last_in;
    int c;
    sb_last_in = frame;
    sb_entered = 0;
    if (d > 2) {  // the list was not running a moment ago: start on the list, no held key
        sb_onrow = 0;
        sb_hold_reset();  // v3.48 SB348 (was sb_hdir = sb_hold = 0)
    }
    if (!sb_row_on) {
        sb_onrow = 0;
    } else {
        sb_m_where = sb_mouse_where();  // v3.47 SB347: left arrow / bar / right arrow (was the bar alone)
        sb_m_over = sb_m_where != SB_HIT_NONE;
        if (SB_CALL0(0x405850)) {  // mouse moved this frame (as the list, 571005)
            if (!sb_m_over) {
                sb_onrow = 0;  // mouse moved away: the list's own mouse hover takes over
            } else if (!sb_onrow) {
                sb_onrow = 1;  // mouse moved onto the Bank row
                SB_SOUND(0x9C40);
            }
        } else if (!sb_onrow && (SB_CALL1(0x5542C0, 0) & 1) && SB_CURSOR == 0) {  // Up on the first slot
            sb_onrow = 1;
            sb_entered = 1;  // this frame's keys belong to the list: no Down / Enter / wheel / click on the row yet
            SB_SOUND(0x9C40);
        }
        // v3.47 SB347: a click on the row while the list has the input (the mouse rested there, or Down took the input
        // back): the row takes the click (shows its arrows) instead of the list confirming its slot (a load / save)
        if (!sb_onrow && sb_m_over && SB_CALL0(0x405490)) {
            SB_I(0x5FF2BC) = 1;  // "button was down": the menu's 405490 sees no click this frame
            sb_onrow = 1;
            sb_entered = 1;
            SB_SOUND(0x9C40);
        }
    }
    if (sb_flip_on) {
        int press, src, q, step;
        const int held = sb_dir(&press, &src);              // v3.48 SB348: 1 or SB_BIG, and who stepped
        step = SbRepStep(&sb_rk, &kSbRep, held, press, &q);  // with the accelerating hold repeat (v3.48 SB348: savebanks_sim.h)
        sb_quiet = q;
        if (step) sb_step(step, src);
    }
    if (!sb_onrow) {
        SbRepReset(&sb_rm);  // v3.48 SB348: no mouse hold off the row
        sb_mh.part = SB_HIT_NONE;
        SbLogFlush(0);
        return 0;
    }
    if (!sb_entered) {  // on the Bank row: wheel / click (single steps), Down / Enter back to the list
        int click, md;
        c = 0;
        if (sb_flip_on) {
            if (SB_CALL0(0x405530)) --c;  // wheel up
            if (SB_CALL0(0x405550)) ++c;  // wheel down
        }
        click = SB_CALL0(0x405490);  // left click (this frame)
        if (click) SB_I(0x5FF2BC) = 1;  // "button was down": the menu's 405490 sees no click this frame
        // v3.48 SB348: a press on a blue arrow = one step, then the hold repeat while the left button stays down on that
        // arrow (gMouseButton[0] 0x5FF28C, what 0x405490 / 0x4054F0 read); off the arrow or released = stop. The bar:
        // one step per click (the next bank, v3.8 .. v3.47). A click is always taken from the menu (no load / save).
        md = SbMhFrame(&sb_mh, click, SB_I(0x5FF28C) == 1, sb_m_where);
        if (sb_flip_on) {
            const int was = sb_active;
            int q, k = SbRepStep(&sb_rm, &kSbRep, md, 0, &q);
            if (!k && click && sb_m_where == SB_HIT_BAR) k = 1, q = 0;
            if (k) {
                sb_quiet = q;
                sb_step(k, SB_SRC_MOUSE);
            }
            if (click && SbClickStep(sb_m_where) && LogQuota(&g_sbQMouse, 20))  // v3.47 SB347 (the press of a hold too)
                LogV("save banks: mouse click on the %s: bank %d -> %d", sb_m_where == SB_HIT_LEFT ? "left arrow" :
                     sb_m_where == SB_HIT_RIGHT ? "right arrow" : "Bank row", was, sb_active);
        }
        if (c) {
            sb_quiet = 0;
            sb_step(c < 0 ? -1 : 1, SB_SRC_MOUSE);
        }
        if ((SB_CALL1(0x5542C0, 0) & 2) || (SB_U(0x1236C34) & SB_U(0x107A1E4))) {  // Down, or confirm (action 11h)
            sb_onrow = 0;
            SB_SOUND(0x9C40);
        }
    } else {  // v3.48 SB348: the frame the row lit: no mouse hold yet
        SbRepReset(&sb_rm);
        sb_mh.part = SB_HIT_NONE;
    }
    SbLogFlush(0);  // v3.48 SB348
    // hide confirm-only bits from the menu this frame (cancel = action 1 stays)
    SB_U(0x1236C34) &= ~(SB_U(0x107A1E4) & ~SB_U(0x107A1A4));
    return 1;
}

static int __cdecl sb_input_c(void)
{
    int r = 0;
    if (SbOff()) return 0;
    __try {
        r = sb_input_body();
    } __except (SbFault(GetExceptionInformation(), "sb_input")) {
        r = 0;
    }
    return r;
}

// ---------------------------------------------------------------- list draw (56FF20 entry; CT 1039-1137)
// sb_label: "\hBank <sb_active>" with the game's ASCII text path (556730 -> 557660), as the list draws its slot numbers
// and play time (5702AF: "\h%02d.", "\h00:00"). v3.47 SB347: "\h" = Latin glyphs of the list's font, so the Menu Font
// covers the row (plain ASCII gave the special monospace glyphs); centred at sb_L.textX; no key hint. Text colours
// 1261C14 / 1261C18 are restored afterwards.
static void sb_label(void)
{
    int raw;
    if (SbLabel(sb_lbl_buf, (int)sizeof sb_lbl_buf, sb_active, sb_cat_bank != sb_active || sb_wblock) < 0) return;
    ((void(__cdecl *)(DWORD))0x556A90)(sb_lbl_col);  // text colour (keeps the current fade alpha)
    raw = SB_CALL1(0x556730, sb_lbl_buf);             // ASCII -> the game's raw string (ring buffer)
    ((SbText5)0x557660)(raw, sb_L.textX, sb_L.textY, sb_lbl_align, 0);  // (raw, x, y, align, 0) - as 50B5B0 calls it
    sb_lbl_w = SB_CALL0(0x5587F0);                                      // v3.47 SB347: its width (gText.blockW, canvas units)
    SB_CALL1(0x5573D0, 0);                                              // as 50B5B0 does after 557660
    if (!sb_lbl_logged) {
        sb_lbl_logged = 1;
        LogV("save banks: the Bank row draws \"%s\" in the list's own font (Menu Font %s), %d units wide, centred at x %d; "
             "arrows at x %d..%d / %d..%d click to the previous / next bank", sb_lbl_buf + 2,
             g_fnState == 1 ? g_fnSheet.name : "Stock", sb_lbl_w, sb_L.textX, sb_L.lTip, sb_L.lBase, sb_L.rBase, sb_L.rTip);
    }
    if (sb_lbl_w > sb_L.bar.x1 - sb_L.bar.x0 && LogQuota(&g_sbQWide, 1))
        LogAnomaly("save banks", "the Bank row's \"%s\" is %d units wide, wider than its bar (%d)", sb_lbl_buf + 2, sb_lbl_w,
                   sb_L.bar.x1 - sb_L.bar.x0);
}

static void __cdecl sb_draw_c(int arg1)
{
    DWORD c14, c18;
    if (SbOff() || !arg1 || !sb_lbl_show) return;  // 56FF20 draws nothing when arg1 = 0
    c14 = SB_U(0x1261C14);
    c18 = SB_U(0x1261C18);
    __try {
        sb_label();
    } __except (SbFault(GetExceptionInformation(), "sb_draw")) {
    }
    SB_U(0x1261C18) = c18;
    SB_U(0x1261C14) = c14;
}

// ---------------------------------------------------------------- list cursor bar (56FDA0 entry; CT 1139-1334)
// sb_tri(tip x, base x): vertices (base, y), (tip, y+0Ch), (base, y+18h), y = the row's top - as 56FB80. Vertex layout of
// 563E30: x, y, z, rhw, colour, u, v.
static void sb_tri(int tip, int base)
{
    DWORD *v = sb_vtx;
    int i;
    v[0] = v[14] = SbF((float)base);  // v0.x, v2.x
    v[7] = SbF((float)tip);           // v1.x
    v[1] = SbF((float)sb_L.bar.y0);  // v3.47 SB347: the layout's row (was sb_lbl_y)
    v[8] = SbF((float)(sb_L.bar.y0 + 0x0C));
    v[15] = SbF((float)(sb_L.bar.y0 + 0x18));
    for (i = 0; i < 3; ++i) {
        v[7 * i + 2] = SB_ONE;
        v[7 * i + 3] = SB_ONE;
        v[7 * i + 4] = sb_arw_col;
        v[7 * i + 5] = 0;
        v[7 * i + 6] = 0;
    }
    SB_CALL0(0x563BA0);
    ((SbTri3)0x563F40)(v, v + 7, v + 14);
    ((SbFlush14)0x564210)(0x48, 0, 0x43, 0, 0x31001, 0, 0, 0, SB_MONE, SB_MONE, 0, SB_ONE, 1, 0);
}

// sb_bar: the A Brand-new Fear row bar (56FCC0 style 0 = blue, pulsing with 13D1A08) at x sb_L.bar (v3.47 SB347),
// y row+2 .. +16h (the list's bar 56FDA0 uses text y+2, height 16h). Same calls as 56FCC0 / 56FDA0.
static void sb_bar(void)
{
    BYTE cl = SB_B(0x13D1A08);
    DWORD col;
    int y0, y1;
    if (cl & 0x20) cl = (BYTE)(((cl & 0x1F) + 0x20) << 1);
    else cl = (BYTE)((0x40 - (cl & 0x1F)) << 1);
    col = (0xFFFF8000u | cl) << 16;  // 80xx0000 = blue, as 56FCC0 with eax = 0
    y0 = sb_L.bar.y0 + 2;
    y1 = y0 + 0x16;
    SB_CALL0(0x563BA0);
    ((SbRect10)0x563D20)(SbF((float)sb_L.bar.x0), SbF((float)y0), SbF((float)sb_L.bar.x1), SbF((float)y1), SB_ONE, SB_ONE,
                         SB_ONE, SB_ONE, col, col);
    ((SbFlush14)0x564210)(0x48, 0, 0x46, 0, 0x31003, 0, 0, 0, SB_MONE, SB_MONE, 0, SB_ONE, 1, 0);
}

// sb_arrows: the A Brand-new Fear selector arrows (56FB80): a triangle 20 x 24, base 14 px outside the bar; only the
// directions you can still go (left when sb_active > 1, right when < SB_MAX). Optional bob (0 = static like the game).
// v3.47 SB347: x from sb_L (savebanks_sim.h), the same numbers the mouse's click boxes use.
static void sb_arrows(void)
{
    int bob = 0;
    if (!sb_arw_on) return;
    if (sb_arw_bob) {
        DWORD c = SB_FRAME & 0x1F;
        if (c >= 0x10) c = 0x1F - c;
        bob = (int)((c * (DWORD)sb_arw_bob) >> 4);  // bob offset 0..sb_arw_bob
    }
    if (SbArrowShown(sb_active, SB_MAX, -1)) sb_tri(sb_L.lTip - bob, sb_L.lBase - bob);
    if (SbArrowShown(sb_active, SB_MAX, +1)) sb_tri(sb_L.rTip + bob, sb_L.rBase + bob);
}

// While the Bank row is selected (and the list input ran this or the previous frame) the blue bar is drawn at the
// Bank row and the list's own cursor bar is skipped. 56FDA0 has no arguments (also reached by jmp, 57097F).
// 1 = drawn here, 56FDA0 returns at once.
static int __cdecl sb_lbar_c(void)
{
    if (SbOff()) return 0;
    if (!sb_onrow || SB_FRAME - sb_last_in > 2) return 0;
    __try {
        sb_bar();
        sb_arrows();
    } __except (SbFault(GetExceptionInformation(), "sb_lbar")) {
    }
    return 1;
}

// ---------------------------------------------------------------- backup before a delete (CT 1685-1806)
// sb_backup: copy the sb_active file to <save dir>\backup\<name>.<YYYYMMDD-HHMMSS-mmm>[-n] (CopyFileA, never
// overwrites). sb_del_bk = 1: once per bank (bit sb_bk_bits[bank]; sb_catwr clears it when a save writes the bank).
// 1 = done or not needed, 0 = failed (sb_bk_err = GetLastError, 0 = path problem).
// [review SB-003, not the table] milliseconds in the name and a -2, -3 .. suffix when that name exists; an existing
// destination is never taken as a backup (the table took ERROR_FILE_EXISTS as done).
static int sb_backup(void)
{
    const int b = sb_active;
    int len, cut = 0, n;
    SYSTEMTIME st;
    sb_bk_err = 0;
    if (b < 1 || b > 999) return 0;
    if (sb_bk_bits[b >> 5] & (1u << (b & 31))) return 1;  // backed up already in this session
    sb_bk_src[0] = 0;
    ((void(__stdcall *)(char *))0x407C10)(sb_bk_src);  // the game's save path (sb_name gives the bank file); 104h, ret 4
    for (len = 0; len < (int)sizeof sb_bk_src && sb_bk_src[len]; ++len)
        if (sb_bk_src[len] == '\\') cut = len + 1;  // length up to and including the last backslash
    if (!cut || len >= (int)sizeof sb_bk_src || len > 200) {  // room for "backup\" + "." + the stamp in the 140h buffer
        Log("save banks: backup: bad save path \"%.*s\"", len, sb_bk_src);
        return 0;
    }
    memcpy(sb_bk_path, sb_bk_src, cut);
    strcpy_s(sb_bk_path + cut, sizeof sb_bk_path - cut, "backup");  // "<exe dir>\save\backup"
    CreateDirectoryA(sb_bk_path, NULL);                               // fails harmlessly when it exists
    GetLocalTime(&st);
    for (n = 1; n <= 99; ++n) {
        int k = _snprintf_s(sb_bk_path + cut, sizeof sb_bk_path - cut, _TRUNCATE,
                            "backup\\%s.%04u%02u%02u-%02u%02u%02u-%03u", sb_bk_src + cut, st.wYear, st.wMonth, st.wDay,
                            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        if (k > 0 && n > 1)
            k = _snprintf_s(sb_bk_path + cut + k, sizeof sb_bk_path - cut - k, _TRUNCATE, "-%d", n);
        if (k < 0) {
            Log("save banks: backup: path too long");
            return 0;
        }
        if (CopyFileA(sb_bk_src, sb_bk_path, TRUE)) {  // bFailIfExists: a backup is never overwritten
            sb_bk_bits[b >> 5] |= 1u << (b & 31);
            ++sb_bk_count;
            Log("save banks: bank %d backed up to %s", b, sb_bk_path);
            return 1;
        }
        sb_bk_err = GetLastError();
        if (sb_bk_err != ERROR_FILE_EXISTS && sb_bk_err != ERROR_ALREADY_EXISTS) break;  // else: the next -n name
    }
    Log("save banks: backup of %s to %s failed (error %lu)", sb_bk_src, sb_bk_path, sb_bk_err);
    return 0;
}

// ---------------------------------------------------------------- the game's delete (56F440 entry; CT 1628-1683)
// 56F440(idx 0, step0, slot) - both menus: state C9h calls it with step0 = 0 (start), CAh with 1 every frame until it
// returns != 1. On the start call: guards, the catalog read again from the file, the backup. If any of that fails the
// next call returns -2 at once ("Delete failed.") and nothing is written.
static void sb_dexec_start(int slot)
{
    const int c = sb_active;
    const char *why = NULL;
    SbLogFlush(1);  // v3.48 SB348: the bank change before the delete's lines
    sb_dabort = 0;
    sb_del_slot = slot;
    sb_del_bank = c;
    if ((unsigned)slot >= 0x1E) why = "bad slot";
    else if (sb_wblock) why = "the bank file could not be read";  // no file access
    else if (c != sb_cat_bank) why = "the list shows another bank's catalog";
    else if (!SB_IOBUF) why = "no save I/O buffer";  // only while a save / load menu is open
    else if (SB_CALL4(0x408920, 0, 0, SB_IOBUF, 0x5E4) != 1) why = "the catalog could not be read again";  // MD5 checked
    else {
        SB_CALL1(0x4FBEB0, 0);  // decrypt into 107C1A0: only what is in the file gets written back
        if (!sb_backup()) why = "the backup failed";
    }
    if (why) {
        sb_dabort = 1;
        ++sb_dfails;
        Log("save banks: delete of slot %d in bank %d refused (%s): \"Delete failed.\"", slot + 1, c, why);
    } else {
        ++sb_dels;
        Log("save banks: deleting slot %d in bank %d", slot + 1, c);
    }
}

// 1 = 56F440 returns -2 now (as a failed step 2: "Delete failed."), 0 = the game's code runs
static int __cdecl sb_dexec_c(int step0, int slot)
{
    if (SbOff()) return 0;
    if (step0) {
        if (!sb_dabort) return 0;
        sb_dabort = 0;
        return 1;
    }
    __try {
        sb_dexec_start(slot);
    } __except (SbFault(GetExceptionInformation(), "sb_dexec")) {
        sb_dabort = 1;  // fail closed: nothing is deleted
        ++sb_dfails;
    }
    return 0;
}

// ---------------------------------------------------------------- naked stubs (the table's patch targets, CT 1816-1876)
static const DWORD g_sbNameRet = 0x407D35, g_sbLatchRet = 0x56F162, g_sbInputRet = 0x570F35, g_sbDrawRet = 0x56FF26,
                   g_sbCatrdRet = 0x4FBEB6, g_sbCatwrRet = 0x4FBDCA, g_sbLbarRet = 0x56FDA5, g_sbDexecRet = 0x56F448,
                   g_sbDexecJne = 0x56F452;

// 407D2D (was `mov ecx,[5B7448] / mov [edi],ecx`): edi = the new end (pushad's edi slot), ecx / edx are reloaded next
static __declspec(naked) void hkSbName(void)
{
    __asm {
        pushad
        cld
        push ebx
        push edi
        call sb_name_c
        add esp, 8
        mov dword ptr [esp], eax
        popad
        jmp dword ptr [g_sbNameRet]
    }
}

// 56F158 (was `mov [13D1978],0`): [esp] = 56F150's return address (nothing pushed yet)
static __declspec(naked) void hkSbLatch(void)
{
    __asm {
        pushad
        cld
        push dword ptr [esp + 20h]
        call sb_latch_c
        add esp, 4
        popad
        mov dword ptr ds:[13D1978h], 0
        jmp dword ptr [g_sbLatchRet]
    }
}

// 570F30 (was `sub esp,14h / push ebx / push esi`): skipped = return the unchanged cursor (cdecl, caller pops arg)
static __declspec(naked) void hkSbInput(void)
{
    __asm {
        pushad
        cld
        call sb_input_c
        mov dword ptr [g_sbInSkip], eax
        popad
        cmp dword ptr [g_sbInSkip], 0
        jne skip
        sub esp, 14h
        push ebx
        push esi
        jmp dword ptr [g_sbInputRet]
    skip:
        mov eax, dword ptr ds:[13D1AB0h]
        ret
    }
}

// 56FF20 (was `sub esp,110h`): ecx is an input of 56FF20 (pushad keeps it); arg1 at [esp+4]
static __declspec(naked) void hkSbDraw(void)
{
    __asm {
        pushad
        cld
        push dword ptr [esp + 24h]
        call sb_draw_c
        add esp, 4
        popad
        sub esp, 110h
        jmp dword ptr [g_sbDrawRet]
    }
}

// 56FDA0 (was `mov al,[13D1A08]`)
static __declspec(naked) void hkSbLbar(void)
{
    __asm {
        pushad
        cld
        call sb_lbar_c
        mov dword ptr [g_sbLbarSkip], eax
        popad
        cmp dword ptr [g_sbLbarSkip], 0
        je orig
        ret
    orig:
        mov al, byte ptr ds:[13D1A08h]
        jmp dword ptr [g_sbLbarRet]
    }
}

// 4FBEB0 (was `push ebp / mov ebp,esp / mov edx,[ebp+8]`)
static __declspec(naked) void hkSbCatrd(void)
{
    __asm {
        pushad
        cld
        call sb_catrd_c
        popad
        push ebp
        mov ebp, esp
        mov edx, dword ptr [ebp + 8]
        jmp dword ptr [g_sbCatrdRet]
    }
}

// 4FBDC0 (was `push ebp / mov ebp,esp / push ebx / mov ebx,[107CD68]`): [esp] = the caller (56F2FA / 56F3DA / 56F49F)
static __declspec(naked) void hkSbCatwr(void)
{
    __asm {
        pushad
        cld
        push dword ptr [esp + 20h]
        call sb_catwr_c
        add esp, 4
        popad
        push ebp
        mov ebp, esp
        push ebx
        mov ebx, dword ptr ds:[107CD68h]
        jmp dword ptr [g_sbCatwrRet]
    }
}

// 56F440 (was `mov eax,[esp+8] / test eax,eax / jne 56F452`): [esp+8] = step0, [esp+0Ch] = slot
static __declspec(naked) void hkSbDexec(void)
{
    __asm {
        pushad
        cld
        push dword ptr [esp + 2Ch]  // slot (after pushad: 20h + return + idx + step0)
        push dword ptr [esp + 2Ch]  // step0
        call sb_dexec_c
        add esp, 8
        mov dword ptr [g_sbDxFail], eax
        popad
        cmp dword ptr [g_sbDxFail], 0
        je orig
        mov eax, 0FFFFFFFEh
        ret
    orig:
        mov eax, dword ptr [esp + 8]
        test eax, eax
        jne run
        jmp dword ptr [g_sbDexecRet]
    run:
        jmp dword ptr [g_sbDexecJne]
    }
}

// ---------------------------------------------------------------- install
static void SbLogDiff(const Site *s)
{
    char a[100], b[100];
    int i;
    a[0] = b[0] = 0;
    for (i = 0; i < s->len && i < 32; ++i) {
        sprintf_s(a + 3 * i, sizeof a - 3 * i, "%02X ", s->stock[i]);
        sprintf_s(b + 3 * i, sizeof b - 3 * i, "%02X ", ((const BYTE *)(ULONG_PTR)s->va)[i]);
    }
    Log("save banks: %08lX is not stock: expected %s", s->va, a);
    Log("save banks: %08lX found               %s", s->va, b);
}

static void SaveBanksInstall(void)
{
    // the 8 patch sites with the table's AOB bytes at their VAs (56F150 / 56F440 / 56FF20: the first 32 bytes)
    static const Site kSites[] = {
        { 0x407D2D, { 0x8B, 0x0D, 0x48, 0x74, 0x5B, 0x00, 0x89, 0x0F, 0x8B, 0x8C, 0x24, 0x24, 0x02, 0x00, 0x00, 0x8B, 0x15,
                      0x4C, 0x74, 0x5B, 0x00 }, 21 },                                                      // sb_name
        { 0x56F150, { 0x8B, 0x44, 0x24, 0x08, 0x85, 0xC0, 0x75, 0x0A, 0xC7, 0x05, 0x78, 0x19, 0x3D, 0x01, 0x00, 0x00, 0x00,
                      0x00, 0xA1, 0x78, 0x19, 0x3D, 0x01, 0x83, 0xE8, 0x00, 0x56, 0xBE, 0x01, 0x00, 0x00, 0x00 }, 32 },  // sb_latch
        { 0x570F30, { 0x83, 0xEC, 0x14, 0x53, 0x56, 0x6A, 0x00, 0x8B, 0xF0 }, 9 },                              // sb_input
        { 0x56FF20, { 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00, 0xA1, 0xB8, 0x2F, 0x6A, 0x00, 0x33, 0x84, 0x24, 0x10, 0x01, 0x00,
                      0x00, 0x56, 0x89, 0x84, 0x24, 0x10, 0x01, 0x00, 0x00, 0x8B, 0x84, 0x24, 0x20, 0x01, 0x00 }, 32 },  // sb_draw
        { 0x4FBEB0, { 0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x08, 0xA1, 0x68, 0xCD, 0x07, 0x01, 0x69, 0xD2, 0xE4, 0x05, 0x00, 0x00,
                      0x8B, 0x48, 0x10 }, 20 },                                                            // sb_catrd
        { 0x4FBDC0, { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x1D, 0x68, 0xCD, 0x07, 0x01, 0x56, 0x57, 0xE8 }, 13 },    // sb_catwr
        { 0x56FDA0, { 0xA0, 0x08, 0x1A, 0x3D, 0x01, 0x83, 0xEC, 0x08, 0xA8, 0x20, 0x74, 0x0A, 0x24, 0x1F, 0x04, 0x20, 0xD0,
                      0xE0, 0x8A, 0xC8, 0xEB, 0x08 }, 22 },                                                // sb_lbar
        { 0x56F440, { 0x8B, 0x44, 0x24, 0x08, 0x85, 0xC0, 0x75, 0x0A, 0xC7, 0x05, 0x78, 0x19, 0x3D, 0x01, 0x00, 0x00, 0x00,
                      0x00, 0xA1, 0x78, 0x19, 0x3D, 0x01, 0x83, 0xE8, 0x00, 0x56, 0xBE, 0x01, 0x00, 0x00, 0x00 }, 32 },  // sb_dexec
    };
    // entries of the game functions called here (a range Refined itself hooks is skipped: calls go through it)
    static const Site kFns[] = {
        { 0x405760, { 0x83, 0xEC, 0x10, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00 }, 9 },             // mouse in rect (Refined)
        { 0x405850, { 0xA1, 0x70, 0xF2, 0x5F, 0x00, 0x85, 0xC0 }, 7 },                         // mouse moved
        { 0x405530, { 0x8B, 0x0D, 0x78, 0xF2, 0x5F, 0x00, 0x33, 0xC0, 0x85, 0xC9, 0x7E }, 11 },  // wheel up
        { 0x405550, { 0x8B, 0x0D, 0x78, 0xF2, 0x5F, 0x00, 0x33, 0xC0, 0x85, 0xC9, 0x7D }, 11 },  // wheel down
        { 0x405490, { 0x8B, 0x0D, 0x8C, 0xF2, 0x5F, 0x00, 0x33, 0xC0, 0x83, 0xF9, 0x01 }, 11 },  // left click
        { 0x554230, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9, 0x8B, 0x0D, 0x30, 0x6C }, 12 },  // held actions
        { 0x5542A0, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9, 0x8B, 0x0D, 0x3C, 0x6C }, 12 },  // held directions
        { 0x5542C0, { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9, 0x8B, 0x0D, 0x40, 0x6C }, 12 },  // new directions
        { 0x56CC40, { 0x83, 0xEC, 0x08, 0xE8, 0x48, 0x15, 0x00, 0x00 }, 8 },                   // sound
        { 0x408920, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x75, 0x13 }, 8 },                   // read a global block
        { 0x407D60, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00 }, 12 },  // CreateFileA wrapper
        { 0x4081A0, { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x85, 0xC0, 0x74, 0x12 }, 9 },             // close the handle
        { 0x407C10, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x1C, 0x02, 0x00, 0x00 }, 12 },  // the save path
        { 0x556A90, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0x1C, 0x1C, 0x26, 0x01 }, 10 },       // text colour
        { 0x556730, { 0xA1, 0x00, 0x72, 0x23, 0x01, 0x56, 0x0F, 0xB7, 0x35, 0x00, 0x72, 0x23 }, 12 },  // ASCII -> raw
        { 0x557660, { 0x83, 0xEC, 0x50, 0x56, 0x8B, 0x74, 0x24, 0x58 }, 8 },                   // raw text draw
        { 0x5573D0, { 0x8B, 0x44, 0x24, 0x04, 0xA3, 0x94, 0x1D, 0x2A, 0x01, 0xC3 }, 10 },
        { 0x563BA0, { 0xB8, 0xC8, 0x6D, 0x2F, 0x01, 0x33, 0xC9, 0x89, 0x48, 0xFC }, 10 },       // batch start
        { 0x563D20, { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x8B, 0x5C, 0x24, 0x18 }, 9 },             // quad
        { 0x563F40, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0x6A, 0x02, 0xE8, 0xE4, 0xFE, 0xFF, 0xFF }, 12 },  // triangle
        { 0x564210, { 0xA1, 0xD4, 0x6D, 0x2F, 0x01 }, 5 },                                     // batch flush (Refined)
        { 0x5587F0, { 0xA1, 0x28, 0x1C, 0x26, 0x01, 0xC3 }, 6 },                               // v3.47 SB347: text width
    };
    static const struct { DWORD va; int len; const void *to; } kPatch[8] = {
        { 0x407D2D, 8, hkSbName }, { 0x56F158, 10, hkSbLatch }, { 0x570F30, 5, hkSbInput }, { 0x56FF20, 6, hkSbDraw },
        { 0x4FBEB0, 6, hkSbCatrd }, { 0x4FBDC0, 10, hkSbCatwr }, { 0x56FDA0, 5, hkSbLbar }, { 0x56F440, 8, hkSbDexec },
    };
    BYTE saved[8][16];
    int i, bad = 0, ok = 1, done = 0;
    SbLayoutMake(&sb_L);  // v3.47 SB347: before any hook can run
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            SbLogDiff(&kSites[i]);
            bad = 1;
        }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i) {
        if (EmOurPatch(kFns[i].va, kFns[i].len)) continue;  // Refined's own hook there (calls go through it)
        if (!SiteOk(&kFns[i])) {
            SbLogDiff(&kFns[i]);
            bad = 1;
        }
    }
    if (bad) {
        Log("save banks: stock bytes differ (another mod, or the cheat table's Save Banks attached first?): nothing "
            "installed, the game's own sh4.sav only");
        return;
    }
    // all or nothing: keep the stock bytes, put every written site back if one write fails
    for (i = 0; i < 8; ++i) memcpy(saved[i], (const void *)(ULONG_PTR)kPatch[i].va, kPatch[i].len);
    for (i = 0; i < 8 && ok; ++i) {
        ok = PokeBranch(kPatch[i].va, 0xE9, kPatch[i].to, kPatch[i].len);
        if (ok) ++done;
    }
    if (!ok) {
        int lost = 0;  // a restore can fail too
        for (i = 0; i < done; ++i)
            if (!Poke(kPatch[i].va, saved[i], kPatch[i].len)) {
                ++lost;
                Log("save banks: %08lX could not be put back", kPatch[i].va);
            }
        if (!lost) {
            Log("save banks: a patch could not be written (VirtualProtect failed): the %d written site(s) are stock "
                "again; the game's own sh4.sav only", done);
            return;
        }
        // fail closed: the sites that stayed patched pass through to the stock code (g_sbBroken), and the table keeps
        // out (its AOBs cannot match them)
        g_sbBroken = 1;
        SH4R_Extra[2] |= 2;
        Log("save banks: %d site(s) stayed patched after a failed install: they run the stock code, banks are off", lost);
        return;
    }
    g_sbInstalled = 1;
    SH4R_Extra[2] |= 2;  // owns bit 1: the table's Save Banks stays off
    Log("save banks: installed, 8 patches (407D2D 56F158 570F30 56FF20 4FBEB0 4FBDC0 56FDA0 56F440); banks %s, bank %ld "
        "(Option 6; 1 = sh4.sav)", g_sbOn ? "on" : "off", g_sbBank);
}
