// ================================================================ v3.52 SND352: Sound (queue item Q1, the sound round)
// Plan: staging/v352_feedback/w1_sound/PLAN.md; research: staging/research_sound/REPORT.md (sections 1.3-1.6, 2, 3, 5); the
// user's answers: questions 3-8 as recommended, stereo speakers (Stereo works: only its pan law is steep).
// - Options > Option 1 > Sound has four values: Mono / Stereo / Headphones / Surround (3D), kept in [Sound] Mode. The game's
//   own Sound byte (cfg+0xC, saved in sh4.sav) stays 0 / 1 / 2 as the game writes it: Mono 0, Stereo and Headphones 1,
//   Surround 2 (the game's Dolby Digital 3D mode). The first start takes the mode from that byte (0 Mono, 1 Stereo, 2
//   Surround). Enter on the row opens the Sound page (Stereo Width, Room Reverb, Night Mode, Back), as Realtime Shadow opens
//   Shadows. Every row applies at once; changed rows are yellow and "No" at the exit puts them back.
// - Stereo pan law: the game's SetPan((pan - 63) * 156) / SetPan(panL * 666) become an equal-power law times Stereo Width
//   (tables, sound_sim.h). Headphones = the same law with the other ear at most 9 dB down. Hidden [Sound] PanLaw=Stock:
//   the game's own law (the two pan sites not patched).
// - Mono = the game's Monaural (no pan) plus a fold of stereo streams (both channels averaged; the stock decoder kept the
//   left one only) and of the seq*.pac music (averaged in the loaded pack, the unfolded copy kept for switching back).
//   Hidden [Sound] MonoFold=0: the game's own Monaural.
// - The Options "No" discard re-applies the Sound mode (the game's discard 0x524320 skips it: the engine kept the last value
//   passed over); the frame end also re-applies it whenever the engine's mode [0x13D6658] differs from the Sound byte.
// - Room Reverb: the four sound reads of the CPU class [0xED4F40] (0 in the GOG build: its CheckSpec test is jumped over at
//   0x41464B) read g_sndCpuClass instead: 2 = the game's own Waves Reverb on 3D buffers and the area presets (On, after a
//   probe buffer with the reverb was made at the first frame), 0 = Off. The render read 0x4030B2 stays the game's.
// - Night Mode: range compression of the volume the game sets at its three SetVolume calls (sound_sim.h SndNightDb).
// - Voice volume (in-game voices) is planned, not built (PLAN.md: tag Play2D / Play3D slots by sound.pac voice entries).
// Sites (all byte-checked, all or nothing per group with rollback, listed in SH4WM_Patches; none is one of Playable Eileen's
// 14 sound sites, which sit at the functions' entries: these are inside the bodies past PE's resume points; no cheat-table
// aob / assert touches them, ct_conflicts.py --span):
//   pan     0x4090BD (9: add ecx,-3Fh; imul ecx,ecx,9Ch -> call; 4 nop), 0x40B617 (6: imul eax,eax,29Ah -> call; nop)
//   volume  0x4090A6 / 0x409285 / 0x40B5E7 (5 each: push eax; push reg; call [reg+3Ch] -> call a stub doing the same)
//   reverb  the address operands of 0x409AD0 (mov edi,[]), 0x409C63 (cmp [],1), 0x41076F (mov eax,[]), 0x4125DA (cmp [],1)
//   mono    0x40AB4A (6: the mode-0 stereo copy loop -> jmp; both channels decoded and averaged), 0x4115E8 (5: the only call
//           of LoadMusic 0x4092F0 -> a wrapper that notes the pack's music data and folds it in Mono)
// Hidden keys: [Sound] Enabled (1; 0 = none of this: the stock Sound row and sound), PanLaw (Smooth / Stock), HeadphonesFloor
// (9; 6 to 12 dB), MonoFold (1).
#include "sound_sim.h"
static int QlSoundHeld(void);  // v3.52 MERGE: extramenu.inc (QL352): 1 while Quick Load from the pause holds the sound

#define SND_BYTE (CFG[0x0C])                                  // the live Options block's Sound (0 / 1 / 2)
#define SND_CACHE (*(volatile int *)0x13D6658)               // 0x56C420's last mode (what the engine was told)
#define SND_STATE (*(volatile int *)0x6E8FD8)                // gSndMgr.state (the engine's mode)
#define SND_DEV (*(void *volatile *)0x6E8FDC)                // gSndMgr.ds (IDirectSound8 *)
#define SND_HAVE (*(volatile int *)0x6018D8)                 // gSndEnabled (0 = no sound device)
#define SND_OPTS_READ (*(volatile int *)0x1092224)           // OptionsLoadFromSave 0x524470 has run (set at its end only)
#define SND_PACK ((BYTE *)0xEC5560)                          // SndPack (gSndMgr + 0x7DC588): music[6] +0x10A4, musicData +0x10F0
#define SND_APPLY(m) ((void(__cdecl *)(int))0x56C420)(m)    // the game's Sound apply (the row's and the boot's)
#define SND_VTC(o, i) ((*(void ***)(o))[i])

static int g_sndOn;                    // [Sound] Enabled (hidden, default 1) and the install ran
static volatile LONG g_sndMode = -1;   // SND_MODE_*; -1 = not known before the first frame (the first start)
static int g_sndMigrate;               // 1 = [Sound] Mode missing (or a new ini): from the save's byte at the first frame; 2 = not a mode
static char g_sndModeRaw[64];          // the ini's Mode text when it was not a mode
static volatile LONG g_sndWidth = 100, g_sndReverb = 1, g_sndNight;
static int g_sndLawStock;              // hidden [Sound] PanLaw=Stock
static int g_sndFloor = SND_FLOOR_DEF; // hidden [Sound] HeadphonesFloor, in hundredths of a dB
static int g_sndMonoFold = 1;          // hidden [Sound] MonoFold
static int g_sndPanOn, g_sndVolOn, g_sndRevOn, g_sndMonoOn, g_sndMusicOn;  // the site groups in place
static volatile LONG g_sndRevProbe = -1;    // -1 not probed yet, 0 DirectSound could not make a buffer with the reverb, 1 ok
static volatile DWORD g_sndCpuClass;        // what the four sound reads of [0xED4F40] see (2 = reverb, 0 = none)
static int g_sndPanA[128], g_sndPanB[128], g_sndStrA[31], g_sndStrB[31];
static const int *volatile g_sndPan2D = g_sndPanA, *volatile g_sndPanStr = g_sndStrA;
static int g_sndStarted;               // the first frame's work is done
static int g_sndTickOff;               // the frame-end work faulted: off for this session
static DWORD g_sndMinuteAt;
static LONG g_snapSndMode = -1, g_snapSndWidth, g_snapSndReverb, g_snapSndNight;
static int g_sndRowOn, g_sndMenu, g_sndPageBuilt;
static DWORD g_rowsSnd[4 * 7 + 1];     // the Sound page: Stereo Width, Room Reverb, Night Mode, Back
static DWORD g_rowsSnd1[8 * 7];        // Option 1's own copy when the Menu Font row did not make one (7 rows + the end mark)
static DWORD g_sndModeIds[SND_MODE_N], g_sndWidthIds[11], g_sndRevIds[2], g_sndNightIds[SND_NIGHT_N];
// counters (the hooks may run on any thread the game's sound code runs on: interlocked)
enum { SNC_V2D, SNC_V3D, SNC_VSTR, SNC_NIGHT, SNC_P2D, SNC_P2D_ONE, SNC_P2D_STOCKONE, SNC_PSTR, SNC_PSTR_ONE,
       SNC_PSTR_STOCKONE, SNC_MUSICFOLD, SNC_N };
static volatile LONG g_sndC[SNC_N];
static volatile LONG g_sndMonoBlocks;  // stereo stream blocks folded (the 0x40AB4A stub)
// the unfolded copy of the seq*.pac music (made at the first fold of a load)
static BYTE *g_sndMusicData, *g_sndMusicCopy;
static SIZE_T g_sndMusicSize;
static int g_sndMusicFolded;

// ---------------------------------------------------------------- texts (MSG_V52 + 0 .. 23: SND352's ids)
enum { SNDM_T, SNDM_L_WIDTH, SNDM_L_REV, SNDM_L_NIGHT, SNDM_L_BACK, SNDM_D_MODE, SNDM_D_WIDTH, SNDM_D_REV, SNDM_D_NIGHT,
       SNDM_D_BACK, SNDM_V_MODE0, SNDM_V_OFF = SNDM_V_MODE0 + SND_MODE_N, SNDM_V_ON, SNDM_V_LOW, SNDM_V_HIGH, SNDM_V_WIDTH,
       SNDM_N };  // 19 of 24 (V52 + 19 .. 23 kept for the Voice row: label, description, value)
typedef char SndTextsFit[SNDM_N <= 24 ? 1 : -1];
static BYTE g_txtSnd[SNDM_N][192];
static const BYTE *SndText(int id)
{
    const int i = id - MSG_V52;
    return g_txtSnd[i >= 0 && i < SNDM_N ? i : SNDM_D_BACK];
}
static void SndTextsDynamic(void)  // the texts that follow the settings (one id each: the row shows the current one)
{
    static const char *const kModeD[SND_MODE_N] = {
        "Mono: every sound in the middle; stereo music and streams are folded to one channel.",
        "Stereo: sounds placed smoothly left and right (Stereo Width).",
        "Headphones: Stereo, and every sound also reaches the other ear (at most %d dB down).",
        "Surround (3D): the game's Dolby Digital 3D sound, for 5.1 / 7.1 speakers or Windows spatial sound.",
    };
    char t[192], d[160];
    const int m = (unsigned)g_sndMode < SND_MODE_N ? (int)g_sndMode : SND_MODE_STEREO;
    sprintf_s(d, sizeof d, kModeD[m], g_sndFloor / 100);
    if (!g_sndPanOn && (m == SND_MODE_STEREO || m == SND_MODE_HEADPHONES))
        sprintf_s(d, sizeof d, "%s: the game's own steep pan (%s).", kSndModeLabel[m], g_sndLawStock ? "PanLaw=Stock" : "see SH4Refined.log");
    sprintf_s(t, sizeof t, "%s%s", d, g_sndMenu ? " Enter: the Sound page." : "");
    Encode(g_txtSnd[SNDM_D_MODE], sizeof g_txtSnd[0], t);
    sprintf_s(t, sizeof t, "%d %%", (int)g_sndWidth);
    Encode(g_txtSnd[SNDM_V_WIDTH], sizeof g_txtSnd[0], t);
    if (!g_sndPanOn)
        strcpy_s(t, sizeof t, g_sndLawStock ? "PanLaw=Stock in SH4Refined.ini: the game's own steep pan; Width does nothing."
                                            : "Not available this session (see SH4Refined.log): the game's own pan.");
    else
        strcpy_s(t, sizeof t, m == SND_MODE_MONO ? "How far sounds spread left and right (100 % full, 0 % all in the middle). Not in Mono."
                                                 : "How far sounds spread left and right: 100 % full, 0 % all in the middle.");
    Encode(g_txtSnd[SNDM_D_WIDTH], sizeof g_txtSnd[0], t);
    if (!g_sndRevOn) strcpy_s(t, sizeof t, "Not available this session (see SH4Refined.log): no reverb, as the PC port.");
    else if (g_sndRevProbe == 0) strcpy_s(t, sizeof t, "DirectSound could not add the reverb on this PC (see SH4Refined.log): no reverb.");
    else strcpy_s(t, sizeof t, "The game's own room echo (12 area presets), off in the PC port until now. Off: dry sound.");
    Encode(g_txtSnd[SNDM_D_REV], sizeof g_txtSnd[0], t);
    if (!g_sndVolOn) strcpy_s(t, sizeof t, "Not available this session (see SH4Refined.log): the game's own loudness.");
    else if (g_sndNight == 0) strcpy_s(t, sizeof t, "Off: the game's own loudness range.");
    else if (g_sndNight == 1) strcpy_s(t, sizeof t, "Low: loud sounds a little quieter, quiet ones louder.");
    else strcpy_s(t, sizeof t, "High: the loudest sounds much quieter, quiet ones louder: for low volume at night.");
    Encode(g_txtSnd[SNDM_D_NIGHT], sizeof g_txtSnd[0], t);
}
static void SndTexts(void)
{
    int k;
    Encode(g_txtSnd[SNDM_T], sizeof g_txtSnd[0], "Sound");
    Encode(g_txtSnd[SNDM_L_WIDTH], sizeof g_txtSnd[0], "Stereo Width");
    Encode(g_txtSnd[SNDM_L_REV], sizeof g_txtSnd[0], "Room Reverb");
    Encode(g_txtSnd[SNDM_L_NIGHT], sizeof g_txtSnd[0], "Night Mode");
    Encode(g_txtSnd[SNDM_L_BACK], sizeof g_txtSnd[0], "Back");
    Encode(g_txtSnd[SNDM_D_BACK], sizeof g_txtSnd[0], "Back to Option 1.");
    for (k = 0; k < SND_MODE_N; ++k) {
        Encode(g_txtSnd[SNDM_V_MODE0 + k], sizeof g_txtSnd[0], kSndModeLabel[k]);
        g_sndModeIds[k] = MSG_V52 + SNDM_V_MODE0 + k;
    }
    Encode(g_txtSnd[SNDM_V_OFF], sizeof g_txtSnd[0], "Off");
    Encode(g_txtSnd[SNDM_V_ON], sizeof g_txtSnd[0], "On");
    Encode(g_txtSnd[SNDM_V_LOW], sizeof g_txtSnd[0], "Low");
    Encode(g_txtSnd[SNDM_V_HIGH], sizeof g_txtSnd[0], "High");
    for (k = 0; k < 11; ++k) g_sndWidthIds[k] = MSG_V52 + SNDM_V_WIDTH;  // one text: the current width (SndTextsDynamic)
    g_sndRevIds[0] = MSG_V52 + SNDM_V_OFF, g_sndRevIds[1] = MSG_V52 + SNDM_V_ON;
    g_sndNightIds[0] = MSG_V52 + SNDM_V_OFF, g_sndNightIds[1] = MSG_V52 + SNDM_V_LOW, g_sndNightIds[2] = MSG_V52 + SNDM_V_HIGH;
    SndTextsDynamic();
}

// ---------------------------------------------------------------- the pan tables
static int SndModeNow(void) { return (unsigned)g_sndMode < SND_MODE_N ? (int)g_sndMode : SND_MODE_STEREO; }
static void SndRebuild(void)  // the hooks read one pair while the other is built, then the pointers swap
{
    const int hp = SndModeNow() == SND_MODE_HEADPHONES;
    int *t = g_sndPan2D == g_sndPanA ? g_sndPanB : g_sndPanA, *s = g_sndPanStr == g_sndStrA ? g_sndStrB : g_sndStrA;
    SndBuildPans(t, s, (int)g_sndWidth, hp ? g_sndFloor : 0);
    InterlockedExchangePointer((PVOID volatile *)&g_sndPanStr, s);
    InterlockedExchangePointer((PVOID volatile *)&g_sndPan2D, t);
}

// ---------------------------------------------------------------- hooks (integer work only: the sites hold x87 state)
static int __cdecl SndPan2DC(int p)  // 0x4090BD: the game's pan 0..127 -> DirectSound's
{
    int v, s;
    if ((unsigned)p > 127) return SndPanStock2D(p);  // never seen; the game's own formula
    v = g_sndPan2D[p];
    s = SndPanStock2D(p);
    InterlockedIncrement(&g_sndC[SNC_P2D]);
    if (SndAbs(v) >= SND_ONE_EAR) InterlockedIncrement(&g_sndC[SNC_P2D_ONE]);
    if (SndAbs(s) >= SND_ONE_EAR) InterlockedIncrement(&g_sndC[SNC_P2D_STOCKONE]);
    return v;
}
static int __cdecl SndPanStrC(int p)  // 0x40B617: a stream's pan -15..15 (the caller checked the range)
{
    int v;
    if (p < -15 || p > 15) return SndPanStockStream(p);
    v = g_sndPanStr[p + 15];
    InterlockedIncrement(&g_sndC[SNC_PSTR]);
    if (SndAbs(v) >= SND_ONE_EAR) InterlockedIncrement(&g_sndC[SNC_PSTR_ONE]);
    if (SndAbs(SndPanStockStream(p)) >= SND_ONE_EAR) InterlockedIncrement(&g_sndC[SNC_PSTR_STOCKONE]);
    return v;
}
static int SndVolAdj(int db)
{
    const int n = (int)g_sndNight;
    int o;
    if (n <= 0) return db;
    o = SndNightDb(db, n);
    if (o != db) InterlockedIncrement(&g_sndC[SNC_NIGHT]);
    return o;
}
static int __cdecl SndVol2DC(int db) { InterlockedIncrement(&g_sndC[SNC_V2D]); return SndVolAdj(db); }
static int __cdecl SndVol3DC(int db) { InterlockedIncrement(&g_sndC[SNC_V3D]); return SndVolAdj(db); }
static int __cdecl SndVolStrC(int db) { InterlockedIncrement(&g_sndC[SNC_VSTR]); return SndVolAdj(db); }

static __declspec(naked) void hkSndPan2D(void)  // 4090BD (call): ecx = the pan 0..127 in, DirectSound's pan out; eax kept
{
    __asm {
        push eax
        push edx
        push ecx
        call SndPan2DC
        add esp, 4
        mov ecx, eax
        pop edx
        pop eax
        ret
    }
}
static __declspec(naked) void hkSndPanStr(void)  // 40B617 (call): eax = panL in, DirectSound's pan out; ecx / edx kept
{
    __asm {
        push ecx
        push edx
        push eax
        call SndPanStrC
        add esp, 4
        pop edx
        pop ecx
        ret
    }
}
static __declspec(naked) void hkSndVol2D(void)  // 4090A6 (call): was push eax; push ecx; call [edx+3Ch] (SetVolume)
{
    __asm {
        push ecx
        push edx
        push eax
        call SndVol2DC
        add esp, 4
        pop edx
        pop ecx
        push eax
        push ecx
        call dword ptr [edx + 3Ch]
        ret
    }
}
static __declspec(naked) void hkSndVol3D(void)  // 409285 (call): was push eax; push esi; call [edi+3Ch]
{
    __asm {
        push ecx
        push edx
        push eax
        call SndVol3DC
        add esp, 4
        pop edx
        pop ecx
        push eax
        push esi
        call dword ptr [edi + 3Ch]
        ret
    }
}
static __declspec(naked) void hkSndVolStr(void)  // 40B5E7 (call): was push eax; push ecx; call [edx+3Ch]
{
    __asm {
        push ecx
        push edx
        push eax
        call SndVolStrC
        add esp, 4
        pop edx
        pop ecx
        push eax
        push ecx
        call dword ptr [edx + 3Ch]
        ret
    }
}
// 0x40AB4A (jmp, inside SndVoice::Decode 0x40A960's frame): Monaural with a stereo stream. The stock loop copied the left
// block [esp+1Ch] (16 samples) to the mono buffer [esp+68h]; here the right block is decoded too, exactly as the stereo
// path at 0x40AB6D does (DecodeBlock 0x40A880 is stateless: each 13-byte block carries its own history), into [esp+3Ch],
// and the average is written. Then the common tail 0x40ABC9 with eax = 32 bytes written, as the stock loop.
static const DWORD g_sndMonoBack = 0x40ABC9, g_sndDecode = 0x40A880;
static __declspec(naked) void hkSndMonoFold(void)
{
    __asm {
        mov eax, dword ptr [esi + ebp + 12C09Ch]
        imul eax, eax, 96000h
        add eax, ebp
        add eax, ebx
        lea ecx, [eax + esi + 1AC0DDh]
        push ecx
        lea edx, [esp + 40h]
        push edx
        mov ecx, esi
        call dword ptr [g_sndDecode]
        push edi
        mov ecx, dword ptr [esp + 6Ch]
        xor eax, eax
    fold:
        movsx edx, word ptr [esp + eax * 2 + 20h]
        movsx edi, word ptr [esp + eax * 2 + 40h]
        add edx, edi
        sar edx, 1
        mov word ptr [ecx + eax * 2], dx
        inc eax
        cmp eax, 10h
        jl fold
        pop edi
        lock inc dword ptr [g_sndMonoBlocks]
        mov eax, 20h
        jmp dword ptr [g_sndMonoBack]
    }
}

// ---------------------------------------------------------------- Mono: the seq*.pac music folded in the loaded pack
// LoadMusic 0x4092F0 reads a whole seq*.pac (all six installed are 2-channel 16-bit 11025 Hz, at most 2.6 MB) into a
// VirtualAlloc block (pack +0x10F0) and its sound table (12-byte records: offset, size, loop) into pack +0x10A4 (six).
// PlayMusic and Update copy from that block into each 0x80000-byte buffer, so a fold shows within one buffer (about 12 s).
static int SndMusicWav(const BYTE *md, SIZE_T size, DWORD off, DWORD len)  // a 2-channel 16-bit PCM WAV image inside the block
{
    if (off >= size || len <= 0x2C || len > size - off) return 0;
    if (memcmp(md + off, "RIFF", 4) || memcmp(md + off + 8, "WAVE", 4)) return 0;
    return *(const WORD *)(md + off + 0x14) == 1 && *(const WORD *)(md + off + 0x16) == 2 && *(const WORD *)(md + off + 0x22) == 16;
}
static void SndMusicFold(int on, const char *why)
{
    BYTE *md;
    int k, n = 0;
    if (!g_sndMusicOn) return;
    __try {
        md = *(BYTE *volatile *)(SND_PACK + 0x10F0);
        if (!md || md != g_sndMusicData || !g_sndMusicSize) return;  // no music loaded through the hook (yet)
        if (on == g_sndMusicFolded) return;
        if (on) {
            if (!g_sndMusicCopy) {
                g_sndMusicCopy = (BYTE *)VirtualAlloc(NULL, g_sndMusicSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);  // v3.52 SND352-001 (Codex): the documented reserve + commit
                if (!g_sndMusicCopy) {
                    static LONG s_q;
                    if (LogQuota(&s_q, 5)) Log("sound: Mono: no memory for the music's stereo copy (%lu KB): the music stays stereo",
                                               (unsigned long)(g_sndMusicSize >> 10));
                    return;
                }
                memcpy(g_sndMusicCopy, md, g_sndMusicSize);
            }
            for (k = 0; k < 6; ++k) {
                const DWORD off = *(const DWORD *)(SND_PACK + 0x10A4 + 12 * k), len = *(const DWORD *)(SND_PACK + 0x10A4 + 12 * k + 4);
                if (!SndMusicWav(md, g_sndMusicSize, off, len)) continue;
                SndFoldStereo16((short *)(md + off + 0x2C), (len - 0x2C) / 4);
                ++n;
            }
        } else {
            if (!g_sndMusicCopy) return;
            memcpy(md, g_sndMusicCopy, g_sndMusicSize);
        }
        g_sndMusicFolded = on;
        InterlockedIncrement(&g_sndC[SNC_MUSICFOLD]);
        LogV("sound: the music %s (%s; %lu KB%s; playing music follows within its buffer, about 12 s)", on ? "folded to mono" : "back in stereo",
             why, (unsigned long)(g_sndMusicSize >> 10), on ? (n ? "" : "; no stereo entry found") : "");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static LONG s_f;
        g_sndMusicData = NULL;  // nothing more on this block
        if (LogQuota(&s_f, 3)) LogAnomaly("sound", "the music fold faulted (%s): the music stays as it is", why);
    }
}
static void __cdecl SndMusicLoaded(BYTE *pack, int ok)  // after each LoadMusic (the 0x4115E8 wrapper)
{
    MEMORY_BASIC_INFORMATION mbi;
    BYTE *md;
    if (pack != SND_PACK) return;
    if (g_sndMusicCopy) VirtualFree(g_sndMusicCopy, 0, MEM_RELEASE);  // the last load's copy (its block is gone)
    g_sndMusicCopy = NULL;
    g_sndMusicFolded = 0;
    g_sndMusicData = NULL;
    g_sndMusicSize = 0;
    if ((ok & 0xFF) == 0) return;  // a failed load: nothing to fold (a later fold finds no block of ours)
    md = *(BYTE *volatile *)(pack + 0x10F0);
    if (!md || !VirtualQuery(md, &mbi, sizeof mbi) || mbi.BaseAddress != md || mbi.State != MEM_COMMIT) return;
    g_sndMusicData = md;
    g_sndMusicSize = mbi.RegionSize;
    if (SndModeNow() == SND_MODE_MONO && g_sndMode >= 0) SndMusicFold(1, "a music file loaded in Mono");
}
static __declspec(naked) void hkSndLoadMusic(void)  // 4115E8 (call): thiscall LoadMusic(n) (ecx = the pack), ret 4, result in al
{
    __asm {
        push ecx
        push dword ptr [esp + 8]
        mov eax, 4092F0h
        call eax
        pop ecx
        push eax
        push eax
        push ecx
        call SndMusicLoaded
        add esp, 8
        pop eax
        ret 4
    }
}

// ---------------------------------------------------------------- settings
static void SndIniWrite(const char *key, const char *val, const char *what)
{
    if (g_iniFallback) return;
    if (!WritePrivateProfileStringA("Sound", key, val, g_iniPath))
        Log("sound: %s applies now but could not be saved in SH4Refined.ini (error %lu)", what, GetLastError());
}
static void SndApplyEngine(const char *why)  // the Sound byte and the engine's mode follow the Refined mode
{
    const int b = SndModeByte(SndModeNow());
    if (g_sndMode < 0) return;
    if (SND_BYTE != b) SND_BYTE = (BYTE)b;
    if (SND_CACHE != b) {
        LogV("sound: the engine's mode %d -> %d (%s)", SND_CACHE, b, why);
        SND_APPLY(b);
    }
}
static void SndSetMode(int m, const char *why)
{
    const int was = (int)g_sndMode;
    if ((unsigned)m >= SND_MODE_N) return;
    InterlockedExchange(&g_sndMode, m);
    SndRebuild();
    SndApplyEngine(why);
    SndMusicFold(m == SND_MODE_MONO && g_sndMonoFold, why);
    SndTextsDynamic();
    if (was != m) {
        char what[48];
        sprintf_s(what, sizeof what, "Mode %s", kSndModeName[m]);
        SndIniWrite("Mode", kSndModeName[m], what);
        char hp[64] = "";
        if (m == SND_MODE_HEADPHONES)
            sprintf_s(hp, sizeof hp, g_sndPanOn ? ", the other ear at most %d dB down" : ", no pan hook: as Stereo", g_sndFloor / 100);
        Log("sound: Mode %s -> %s (%s): the game's Sound %d (%s)%s", (unsigned)was < SND_MODE_N ? kSndModeName[was] : "-",
            kSndModeName[m], why, SndModeByte(m), kSndByteName[SndModeByte(m)], hp);
    }
}
static void SndSetWidth(int w, const char *why)
{
    const int was = (int)g_sndWidth;
    char t[16];
    if (w < 0 || w > 100) return;
    InterlockedExchange(&g_sndWidth, w);
    SndRebuild();
    SndTextsDynamic();
    if (was == w) return;
    sprintf_s(t, sizeof t, "%d", w);
    SndIniWrite("Width", t, "Stereo Width");
    Log("sound: Stereo Width %d -> %d %% (%s)%s", was, w, why, g_sndPanOn ? "" : " (no pan hook: no effect)");
}
static void SndRevApply(void)
{
    InterlockedExchange((volatile LONG *)&g_sndCpuClass, g_sndRevOn && g_sndReverb && g_sndRevProbe == 1 ? 2 : 0);
}
static void SndSetReverb(int on, const char *why)
{
    const int was = (int)g_sndReverb;
    InterlockedExchange(&g_sndReverb, on ? 1 : 0);
    SndRevApply();
    SndTextsDynamic();
    if (was == (on ? 1 : 0)) return;
    SndIniWrite("Reverb", on ? "On" : "Off", "Room Reverb");
    Log("sound: Room Reverb %s -> %s (%s): the sound reads of the CPU class see %lu%s", was ? "On" : "Off", on ? "On" : "Off", why,
        (unsigned long)g_sndCpuClass, on && g_sndRevProbe == 0 ? " (DirectSound refused the reverb at the start: none)" : "");
}
static void SndSetNight(int n, const char *why)
{
    const int was = (int)g_sndNight;
    if (n < 0 || n >= SND_NIGHT_N) return;
    InterlockedExchange(&g_sndNight, n);
    SndTextsDynamic();
    if (was == n) return;
    SndIniWrite("NightMode", kSndNightName[n], "Night Mode");
    Log("sound: Night Mode %s -> %s (%s)%s", kSndNightName[was], kSndNightName[n], why, g_sndVolOn ? "" : " (no volume hook: no effect)");
}
static void SndReadIni(void)
{
    char t[64];
    int v;
    g_sndOn = GetPrivateProfileIntA("Sound", "Enabled", 1, g_iniPath) != 0;
    // Mode: read raw (IniStr would add a missing key with its default): missing = the first start, from the save's byte
    GetPrivateProfileStringA("Sound", "Mode", "\x01", t, sizeof t, g_iniPath);
    if (g_iniCreated || g_iniFallback || !strcmp(t, "\x01")) {
        g_sndMigrate = 1;
    } else if ((v = SndParseMode(t)) < 0) {
        g_sndMigrate = 2;
        strcpy_s(g_sndModeRaw, sizeof g_sndModeRaw, t);
    } else {
        g_sndMode = v;
    }
    IniStr("Sound", "Width", t, sizeof t);
    if ((v = SndParseWidth(t)) < 0) Log("sound: [Sound] Width=\"%s\" is not 0 to 100: 100", t), v = 100;
    g_sndWidth = v;
    IniStr("Sound", "Reverb", t, sizeof t);
    if ((v = SndParseOnOff(t)) < 0) Log("sound: [Sound] Reverb=\"%s\" is not Off / On: On", t), v = 1;
    g_sndReverb = v;
    IniStr("Sound", "NightMode", t, sizeof t);
    if ((v = SndParseNight(t)) < 0) Log("sound: [Sound] NightMode=\"%s\" is not Off / Low / High: Off", t), v = 0;
    g_sndNight = v;
    GetPrivateProfileStringA("Sound", "PanLaw", "Smooth", t, sizeof t, g_iniPath);  // hidden
    g_sndLawStock = !_stricmp(t, "Stock");
    if (!g_sndLawStock && _stricmp(t, "Smooth")) Log("sound: [Sound] PanLaw=\"%s\" is not Smooth / Stock: Smooth", t);
    v = GetPrivateProfileIntA("Sound", "HeadphonesFloor", SND_FLOOR_DEF / 100, g_iniPath);  // hidden, dB
    if (v < SND_FLOOR_MIN / 100 || v > SND_FLOOR_MAX / 100) {
        Log("sound: [Sound] HeadphonesFloor=%d is not 6 to 12 (dB): %d", v, SND_FLOOR_DEF / 100);
        v = SND_FLOOR_DEF / 100;
    }
    g_sndFloor = v * 100;
    g_sndMonoFold = GetPrivateProfileIntA("Sound", "MonoFold", 1, g_iniPath) != 0;  // hidden
}

// ---------------------------------------------------------------- install (DllMain; all or nothing per group, rollback)
typedef struct { DWORD va; int len; BYTE b[12], stock[12]; } SndPatch;
static void SndCall(SndPatch *p, DWORD va, BYTE op, const void *to, int len, const BYTE *stock)
{
    const DWORD rel = (DWORD)(ULONG_PTR)to - (va + 5);
    p->va = va, p->len = len;
    memcpy(p->stock, stock, len);
    memset(p->b, 0x90, sizeof p->b);
    p->b[0] = op;
    memcpy(p->b + 1, &rel, 4);
}
static void SndOperand(SndPatch *p, DWORD va, const BYTE *stock, int len, int at)  // an absolute operand -> &g_sndCpuClass
{
    const DWORD a = (DWORD)(ULONG_PTR)&g_sndCpuClass;
    p->va = va, p->len = len;
    memcpy(p->stock, stock, len);
    memcpy(p->b, stock, len);
    memcpy(p->b + at, &a, 4);
}
static int SndApplySet(const SndPatch *p, int n, const char *what)
{
    int i, j;
    for (i = 0; i < n; ++i)
        if (memcmp((const void *)(ULONG_PTR)p[i].va, p[i].stock, p[i].len)) {
            const BYTE *q = (const BYTE *)(ULONG_PTR)p[i].va;
            Log("sound: %s: %08lX is not the GOG v7 code (%02X %02X %02X %02X ...; another mod?): not installed", what, p[i].va,
                q[0], q[1], q[2], q[3]);
            return 0;
        }
    for (i = 0; i < n; ++i)
        if (!Poke(p[i].va, p[i].b, p[i].len) || memcmp((const void *)(ULONG_PTR)p[i].va, p[i].b, p[i].len)) {
            for (j = i; j >= 0; --j) Poke(p[j].va, p[j].stock, p[j].len);
            LogAnomaly("sound", "%s: %08lX could not be written: the set is rolled back", what, p[i].va);
            return 0;
        }
    return 1;
}
static void SndInstall(void)
{
    static const BYTE kPan2D[9] = { 0x83, 0xC1, 0xC1, 0x69, 0xC9, 0x9C, 0x00, 0x00, 0x00 };
    static const BYTE kPanStr[6] = { 0x69, 0xC0, 0x9A, 0x02, 0x00, 0x00 };
    static const BYTE kVolEcx[5] = { 0x50, 0x51, 0xFF, 0x52, 0x3C }, kVolEsi[5] = { 0x50, 0x56, 0xFF, 0x57, 0x3C };
    static const BYTE kRev1[6] = { 0x8B, 0x3D, 0x40, 0x4F, 0xED, 0x00 }, kRev2[6] = { 0x83, 0x3D, 0x40, 0x4F, 0xED, 0x00 };
    static const BYTE kRev3[5] = { 0xA1, 0x40, 0x4F, 0xED, 0x00 };
    static const BYTE kMono[6] = { 0x8B, 0x4C, 0x24, 0x68, 0x33, 0xC0 }, kMusic[5] = { 0xE8, 0x03, 0x7D, 0xFF, 0xFF };
    // what the hooks call or rely on (checked, never written)
    static const Site kCallee[] = {
        { 0x56C420, { 0xA1, 0x58, 0x66, 0x3D, 0x01, 0x56, 0x8B, 0x74, 0x24, 0x08, 0x3B, 0xC6 }, 12 },  // the Sound apply
        { 0x40A880, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53 }, 7 },                                // DecodeBlock
        { 0x4092F0, { 0x83, 0xEC, 0x78, 0xA1, 0xB8, 0x2F, 0x6A, 0x00 }, 8 },                          // LoadMusic
        { 0x40AB45, { 0x83, 0x3E, 0x00, 0x75, 0x23 }, 5 },                                            // Decode: mode == 0?
        { 0x40AB6D, { 0x8B, 0x84, 0x2E, 0x9C, 0xC0, 0x12, 0x00, 0x69, 0xC0, 0x00, 0x60, 0x09, 0x00, 0x03, 0xC5, 0x03, 0xC3,
                      0x8D, 0x8C, 0x30, 0xDD, 0xC0, 0x1A, 0x00, 0x51, 0x8D, 0x54, 0x24, 0x40, 0x52, 0x8B, 0xCE }, 32 },  // its right decode
        { 0x40AB4E, { 0x33, 0xC0, 0x8A, 0x54, 0x44, 0x1C, 0x88, 0x11, 0x8A, 0x54, 0x44, 0x1D, 0x88, 0x51, 0x01, 0x40, 0x83, 0xC1,
                      0x02, 0x83, 0xF8, 0x10, 0x7C, 0xEA, 0xB8, 0x20, 0x00, 0x00, 0x00, 0xEB, 0x5C }, 31 },  // the left-only loop
        { 0x40ABC9, { 0x8B, 0x54, 0x24, 0x14, 0x8B, 0x4C, 0x24, 0x68, 0x03, 0xD0 }, 10 },                 // its tail
    };
    SndPatch p[4];
    int i, calleeOk = 1, strOk = 1, musOk = 1;
    SndReadIni();
    if (!g_sndOn) {
        Log("sound: [Sound] Enabled=0: the game's own sound and Sound row (no Refined sound site)");
        return;
    }
    for (i = 0; i < (int)(sizeof kCallee / sizeof kCallee[0]); ++i)
        if (!SiteOk(&kCallee[i])) {
            Log("sound: %08lX is not the GOG v7 code (another mod?)%s", kCallee[i].va,
                i == 0 ? "" : i == 2 ? ": no Mono music fold" : ": no Mono stream fold");
            if (i == 0) calleeOk = 0;  // without the apply the mode row cannot switch the engine
            else if (i == 2) musOk = 0;  // LoadMusic (the music fold's wrapper calls it)
            else strOk = 0;  // DecodeBlock and the decoder's frame the stream fold relies on
        }
    if (!calleeOk) {
        g_sndOn = 0;
        Log("sound: the game's Sound apply 0056C420 differs: no Refined sound (the stock Sound row stays)");
        return;
    }
    SndRebuild();
    SndTexts();
    if (!g_sndLawStock) {
        SndCall(&p[0], 0x4090BD, 0xE8, hkSndPan2D, 9, kPan2D);
        SndCall(&p[1], 0x40B617, 0xE8, hkSndPanStr, 6, kPanStr);
        g_sndPanOn = SndApplySet(p, 2, "pan law (004090BD / 0040B617)");
    }
    SndCall(&p[0], 0x4090A6, 0xE8, hkSndVol2D, 5, kVolEcx);
    SndCall(&p[1], 0x409285, 0xE8, hkSndVol3D, 5, kVolEsi);
    SndCall(&p[2], 0x40B5E7, 0xE8, hkSndVolStr, 5, kVolEcx);
    g_sndVolOn = SndApplySet(p, 3, "volume (004090A6 / 00409285 / 0040B5E7)");
    SndOperand(&p[0], 0x409AD0, kRev1, 6, 2);
    SndOperand(&p[1], 0x409C63, kRev2, 6, 2);
    SndOperand(&p[2], 0x41076F, kRev3, 5, 1);
    SndOperand(&p[3], 0x4125DA, kRev2, 6, 2);
    g_sndRevOn = SndApplySet(p, 4, "Room Reverb (00409AD0 / 00409C63 / 0041076F / 004125DA)");
    if (g_sndMonoFold && strOk) {
        SndCall(&p[0], 0x40AB4A, 0xE9, hkSndMonoFold, 6, kMono);
        g_sndMonoOn = SndApplySet(p, 1, "Mono stream fold (0040AB4A)");
    }
    if (g_sndMonoFold && musOk) {
        SndCall(&p[0], 0x4115E8, 0xE8, hkSndLoadMusic, 5, kMusic);
        g_sndMusicOn = SndApplySet(p, 1, "Mono music fold (004115E8)");
    }
    SndTextsDynamic();
    Log("sound: in: pan law %s, volume hooks %s (Night Mode), Room Reverb %s, Mono fold %s (streams) / %s (music); "
        "the mode at the first frame (%s)",
        g_sndPanOn ? "Smooth (004090BD / 0040B617)" : g_sndLawStock ? "Stock ([Sound] PanLaw=Stock: not patched)" : "NOT IN (the game's own)",
        g_sndVolOn ? "in" : "NOT IN", g_sndRevOn ? "in (the sound reads of 00ED4F40)" : "NOT IN",
        g_sndMonoOn ? "in" : g_sndMonoFold ? "NOT IN" : "off (MonoFold=0)", g_sndMusicOn ? "in" : g_sndMonoFold ? "NOT IN" : "off",
        g_sndMigrate == 1 ? "first start: from the save's Sound" : g_sndMigrate == 2 ? "[Sound] Mode is not a mode: from the save's Sound"
                                                                                    : kSndModeName[SndModeNow()]);
}

// ---------------------------------------------------------------- the first frame: mode, probe, start line
typedef struct { WORD tag, ch; DWORD rate, bps; WORD align, bits, cb; } SndWfx;
typedef struct { DWORD size, flags, bytes, reserved; SndWfx *wfx; GUID alg; } SndDesc;  // DSBUFFERDESC (0x24)
typedef struct { DWORD size, flags; GUID cls; DWORD_PTR r1, r2; } SndFxDesc;             // DSEFFECTDESC (0x20)
static const GUID kSndHrtfFull = { 0xC2413340, 0x1C1B, 0x11D2, { 0x94, 0xF5, 0x00, 0xC0, 0x4F, 0xC2, 0x8A, 0xCA } };
static const GUID kSndFxReverb = { 0x87FC0268, 0x9A55, 0x4360, { 0x95, 0xAA, 0x00, 0x4A, 0x1D, 0x9D, 0xE2, 0x6C } };
static const GUID kSndIidBuf8 = { 0x6825A449, 0x7524, 0x4D82, { 0x92, 0x0F, 0x50, 0xE3, 0x6A, 0xB3, 0xAB, 0x1E } };
static const GUID kSndIidReverb = { 0x46858C3A, 0x0DC6, 0x45E3, { 0xB7, 0x60, 0xD4, 0xEE, 0xF1, 0x6C, 0xB3, 0x25 } };
typedef HRESULT(__stdcall *SndCreate_t)(void *, const SndDesc *, void **, void *);
typedef HRESULT(__stdcall *SndQi_t)(void *, const GUID *, void **);
typedef ULONG(__stdcall *SndRel_t)(void *);
typedef HRESULT(__stdcall *SndSetFx_t)(void *, DWORD, SndFxDesc *, DWORD *);
typedef HRESULT(__stdcall *SndGetObj_t)(void *, const GUID *, DWORD, const GUID *, void **);
typedef HRESULT(__stdcall *SndSpk_t)(void *, DWORD *);
// a buffer as the game's Play3D (or Play2D with fx) makes it, with the Waves Reverb: 1 = it works here
static int SndProbe(void *ds, int threeD, char *out, size_t n)
{
    SndWfx w = { 1, 1, 22050, 44100, 2, 16, 0 };
    SndDesc d;
    SndFxDesc fx;
    void *b = NULL, *b8 = NULL, *rv = NULL;
    DWORD res = 0;
    HRESULT hc, hf = E_FAIL, ho = E_FAIL;
    int ok = 0;
    memset(&d, 0, sizeof d);
    d.size = sizeof d;
    d.flags = (threeD ? 0x10 : 0x40) | 0x80 | 0x200;  // CTRL3D or CTRLPAN, CTRLVOLUME, CTRLFX
    d.bytes = 0x80000;
    d.wfx = &w;
    if (threeD) d.alg = kSndHrtfFull;
    hc = ((SndCreate_t)SND_VTC(ds, 3))(ds, &d, &b, NULL);
    if (SUCCEEDED(hc) && b && SUCCEEDED(((SndQi_t)SND_VTC(b, 0))(b, &kSndIidBuf8, &b8)) && b8) {
        memset(&fx, 0, sizeof fx);
        fx.size = sizeof fx;
        fx.cls = kSndFxReverb;
        hf = ((SndSetFx_t)SND_VTC(b8, 21))(b8, 1, &fx, &res);
        if (SUCCEEDED(hf) && (res & 3)) {
            ho = ((SndGetObj_t)SND_VTC(b8, 23))(b8, &kSndFxReverb, 0, &kSndIidReverb, &rv);
            ok = SUCCEEDED(ho) && rv != NULL;
        }
        if (rv) ((SndRel_t)SND_VTC(rv, 2))(rv);
    }
    if (b8) ((SndRel_t)SND_VTC(b8, 2))(b8);
    if (b) ((SndRel_t)SND_VTC(b, 2))(b);
    sprintf_s(out, n, "%s buffer hr %08lX%s, SetFX hr %08lX result %lu, reverb object hr %08lX", threeD ? "3D" : "2D",
              (unsigned long)hc, hc == 0x0878000A ? " (DS_NO_VIRTUALIZATION: no HRTF, plain 3D)" : "", (unsigned long)hf,
              (unsigned long)res, (unsigned long)ho);
    return ok;
}
static void SndFirstFrame(void)
{
    char spk[64] = "unknown", p3[160] = "", p2[160] = "";
    void *ds = SND_HAVE ? SND_DEV : NULL;
    const int saved = SND_BYTE;
    if (g_sndMigrate) {
        const int m = SndModeFromByte(saved, -1);
        if (g_sndMigrate == 2)
            Log("sound: [Sound] Mode=\"%s\" is not a mode (Mono, Stereo, Headphones, Surround): %s, from the save's Sound %d (%s)",
                g_sndModeRaw, kSndModeName[m], saved, saved >= 0 && saved <= 2 ? kSndByteName[saved] : "not a stock value");
        else
            Log("sound: first start: [Sound] Mode=%s, from the save's Option 1 Sound %d (%s)", kSndModeName[m], saved,
                saved >= 0 && saved <= 2 ? kSndByteName[saved] : "not a stock value: Stereo");
        InterlockedExchange(&g_sndMode, m);
        SndIniWrite("Mode", kSndModeName[m], "the Sound mode");
    } else if (SND_BYTE != SndModeByte(SndModeNow())) {
        Log("sound: the save's Sound %d (%s) differs from [Sound] Mode=%s: the ini's mode is used (Sound %d)", saved,
            saved >= 0 && saved <= 2 ? kSndByteName[saved] : "not a stock value", kSndModeName[SndModeNow()],
            SndModeByte(SndModeNow()));
    }
    SndRebuild();
    SndApplyEngine("the start");
    if (SndModeNow() == SND_MODE_MONO && g_sndMonoFold) SndMusicFold(1, "the start in Mono");
    if (ds) {
        __try {
            DWORD c = 0;
            if (SUCCEEDED(((SndSpk_t)SND_VTC(ds, 8))(ds, &c)))
                sprintf_s(spk, sizeof spk, "%s (%08lX)", SndSpeakerName(c), (unsigned long)c);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            strcpy_s(spk, sizeof spk, "the query faulted");
        }
        if (g_sndRevOn) {
            int ok3 = 0, ok2 = 0;
            __try {
                ok3 = SndProbe(ds, 1, p3, sizeof p3);
                ok2 = SndProbe(ds, 0, p2, sizeof p2);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                strcpy_s(p3, sizeof p3, "the probe faulted");
            }
            InterlockedExchange(&g_sndRevProbe, ok3 && ok2);
            if (!(ok3 && ok2)) LogAnomaly("sound", "Room Reverb: DirectSound would not make a buffer with the Waves Reverb (%s; %s): "
                                          "no reverb this session", p3, p2[0] ? p2 : "-");
        }
    } else {
        InterlockedExchange(&g_sndRevProbe, 0);
        strcpy_s(spk, sizeof spk, "no sound device");
    }
    SndRevApply();
    SndTextsDynamic();
    Log("sound: Mode %s (the game's Sound %d, engine %d), pan law %s, Stereo Width %d %%, Headphones floor -%d dB, Room Reverb %s "
        "(%s), Night Mode %s, Voice volume not in this build, Mono fold %s; speakers %s",
        kSndModeName[SndModeNow()], SND_BYTE, SND_STATE, g_sndPanOn ? "Smooth" : g_sndLawStock ? "Stock" : "Stock (no hook)",
        (int)g_sndWidth, g_sndFloor / 100, g_sndReverb ? "On" : "Off",
        !g_sndRevOn ? "not installed" : g_sndRevProbe == 1 ? "probe ok" : "PROBE FAILED: none", kSndNightName[g_sndNight],
        g_sndMonoOn || g_sndMusicOn ? "on" : "off", spk);
    if (p3[0]) LogV("sound: reverb probe: %s; %s", p3, p2);
    g_sndMinuteAt = GetTickCount();
}

// ---------------------------------------------------------------- the frame end (FrameEndC, the game thread, every frame)
static void SndMinute(void)
{
    LONG c[SNC_N], mono;
    int k, any = 0;
    for (k = 0; k < SNC_N; ++k) any |= (c[k] = InterlockedExchange(&g_sndC[k], 0)) != 0;
    mono = InterlockedExchange(&g_sndMonoBlocks, 0);
    if (!any && !mono) return;
    LogV("sound: minute: volumes 2D %ld, 3D %ld, streams %ld (Night Mode %s: %ld changed); pans 2D %ld (one ear only %ld; the "
         "stock law %ld), streams %ld (one ear only %ld; the stock law %ld); Mono: %ld stream block(s) folded, %ld music fold(s); "
         "Mode %s, engine %d",
         c[SNC_V2D], c[SNC_V3D], c[SNC_VSTR], kSndNightName[g_sndNight], c[SNC_NIGHT], c[SNC_P2D], c[SNC_P2D_ONE], c[SNC_P2D_STOCKONE],
         c[SNC_PSTR], c[SNC_PSTR_ONE], c[SNC_PSTR_STOCKONE], mono, c[SNC_MUSICFOLD], kSndModeName[SndModeNow()], SND_STATE);
}
static void SndFrameTick(void)
{
    if (!g_sndOn || g_sndTickOff) return;
    __try {
        int b;
        if (!SND_OPTS_READ) return;  // the Options block is not read from the save yet
        if (!g_sndStarted) {
            g_sndStarted = 1;
            SndFirstFrame();
        }
        b = SND_BYTE;
        if (!g_sndRowOn && b != SndModeByte(SndModeNow()) && b >= 0 && b <= 2) {
            SndSetMode(SndModeFromByte(b, SndModeNow()), "the game's own Sound row");  // no Refined row: the byte decides
        } else if (b != SndModeByte(SndModeNow())) {
            static LONG s_q;
            if (LogQuota(&s_q, 20))
                Log("sound: the Options block's Sound came in as %d (a save's or the memory card's copy): %d for Mode %s", b,
                    SndModeByte(SndModeNow()), kSndModeName[SndModeNow()]);
            SND_BYTE = (BYTE)SndModeByte(SndModeNow());
        }
        {  // v3.52 MERGE: the re-apply waits while Quick Load from the pause holds the sound (sound_sim.h SndKeepStep)
            static int s_waited;
            const int k = SndKeepStep(SND_CACHE, SND_BYTE, QlSoundHeld());
            if (k == SND_KEEP_WAIT && !s_waited) {
                s_waited = 1;
                LogV("sound: the engine was told Sound %d but the Options say %d: applied after Quick Load gives the sound back",
                     SND_CACHE, SND_BYTE);
            }
            if (k == SND_KEEP_APPLY) {  // the Options "No" discard of the stock row (and anything else that skipped the apply)
                static LONG s_q;
                if (LogQuota(&s_q, 20))
                    Log("sound: the engine was told Sound %d but the Options say %d (the game's discard skips Sound): applied%s",
                        SND_CACHE, SND_BYTE, s_waited ? " (after Quick Load's hold)" : "");
                s_waited = 0;
                SND_APPLY(SND_BYTE);
            }
        }
        if (GetTickCount() - g_sndMinuteAt >= 60000) {
            g_sndMinuteAt = GetTickCount();
            SndMinute();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_sndTickOff = 1;
        LogAnomaly("sound", "the frame-end work faulted: the Sound mode is no longer kept in step this session");
    }
}
// LoadOptions (0x570D11 / 0x571DED, a save loads its own Options block): Sound is a device setting, not per save
static void SndLoadKeep(void)
{
    const int b = SndModeByte(SndModeNow());
    if (!g_sndOn || g_sndMode < 0 || SND_BYTE == b) return;
    {
        static LONG s_q;
        if (LogQuota(&s_q, 20))
            LogV("sound: the loaded save's Options say Sound %d: kept %d (Mode %s; a device setting, not per save)", SND_BYTE, b,
                 kSndModeName[SndModeNow()]);
    }
    SND_BYTE = (BYTE)b;
}

// ---------------------------------------------------------------- Options: Option 1's Sound row and the Sound page
static int __cdecl SndModeGet(void)
{
    if (g_sndMode >= 0 && g_sndMode < SND_MODE_N) return (int)g_sndMode;
    return SndModeFromByte(SND_BYTE, -1);
}
static int __cdecl SndRowDesc(void) { return MSG_V52 + SNDM_D_MODE; }
static int __cdecl SndRowIn(BYTE *cfg, int count)  // Option 1 row 5: Left / Right / the arrows step the mode; Enter opens the page
{
    int v;
    (void)cfg;
    (void)count;
    if (g_sndMenu && g_subScreenOn && ((Fn2)0x4F84A0)(0, 0x11)) {
        InterlockedExchange(&g_subReq, REQ_OPEN | LP_SND);
        ((Fn1)0x56CC40)(0xA411);  // the stock sub-screen sound (Button Config, 52495B)
        return SubPageExit();
    }
    v = StepValue(SndModeGet(), SND_MODE_N);
    if (v >= 0) {
        if (g_sndMode < 0) InterlockedExchange(&g_sndMode, SndModeGet());
        SndSetMode(v, "Options");
    }
    return 0;
}
static int __cdecl SndWidthGet(void) { return ((int)g_sndWidth + SND_WIDTH_STEP / 2) / SND_WIDTH_STEP; }
static int __cdecl SndWidthIn(BYTE *cfg, int count)
{
    const int v = StepValue(SndWidthGet(), 11);
    (void)cfg;
    (void)count;
    if (v >= 0) SndSetWidth(v * SND_WIDTH_STEP, "Options");
    return 0;
}
static int __cdecl SndWidthDesc(void) { return MSG_V52 + SNDM_D_WIDTH; }
static int __cdecl SndRevGet(void) { return g_sndReverb ? 1 : 0; }
static int __cdecl SndRevIn(BYTE *cfg, int count)
{
    const int v = StepValue(SndRevGet(), 2);
    (void)cfg;
    (void)count;
    if (v >= 0) SndSetReverb(v, "Options");
    return 0;
}
static int __cdecl SndRevDesc(void) { return MSG_V52 + SNDM_D_REV; }
static int __cdecl SndNightGet(void) { return (unsigned)g_sndNight < SND_NIGHT_N ? (int)g_sndNight : 0; }
static int __cdecl SndNightIn(BYTE *cfg, int count)
{
    const int v = StepValue(SndNightGet(), SND_NIGHT_N);
    (void)cfg;
    (void)count;
    if (v >= 0) SndSetNight(v, "Options");
    return 0;
}
static int __cdecl SndNightDesc(void) { return MSG_V52 + SNDM_D_NIGHT; }
static int __cdecl SndBackDesc(void) { return MSG_V52 + SNDM_D_BACK; }
static int __cdecl SndBackIn(BYTE *cfg, int count)  // Enter fades back to Option 1, on Sound
{
    (void)cfg;
    (void)count;
    if (!((Fn2)0x4F84A0)(0, 0x11)) return 0;
    InterlockedExchange(&g_subReq, REQ_BACK);
    ((Fn1)0x56CC40)(0x9C42);  // the stock confirm sound (0x5261CB)
    return SubPageExit();
}
// ApplyAspectMenu, before InstallPages: the page's table, from Option 1's stock Exit record (row 6)
static void SndBuildPage(const DWORD *stockRows)
{
    if (!g_sndOn) return;
    FillRow(g_rowsSnd, SndWidthIn, SndWidthGet, g_sndWidthIds, 11, MSG_V52 + SNDM_L_WIDTH, SndWidthDesc);
    FillRow(g_rowsSnd + 7, SndRevIn, SndRevGet, g_sndRevIds, 2, MSG_V52 + SNDM_L_REV, SndRevDesc);
    FillRow(g_rowsSnd + 14, SndNightIn, SndNightGet, g_sndNightIds, SND_NIGHT_N, MSG_V52 + SNDM_L_NIGHT, SndNightDesc);
    memcpy(g_rowsSnd + 21, stockRows + 6 * 7, 7 * 4);
    g_rowsSnd[21 + 0] = (DWORD)(ULONG_PTR)SndBackIn;
    g_rowsSnd[21 + 5] = MSG_V52 + SNDM_L_BACK;
    g_rowsSnd[21 + 6] = (DWORD)(ULONG_PTR)SndBackDesc;
    g_rowsSnd[28] = 0;
    g_lpTable[LP_SND] = g_rowsSnd;
    g_lpCount[LP_SND] = 4;
    g_lpTitle[LP_SND] = MSG_V52 + SNDM_T;
    g_sndPageBuilt = 1;
}
// ApplyAspectMenu, after InstallPages and FnRowInstall (which may have made Option 1 a copy with the Menu Font row): row 5
// (the stock Sound row: input 0x524B60, getter 0x413C70 (cfg+0xC), values 0x654C20, 3 values, label 0x24, description
// 0x525460) gets the four modes and Enter
static void SndRowInstall(void)
{
    DWORD *t = g_lpTable[0], *r;
    if (!g_sndOn) return;
    if (!g_pagesOn) {
        Log("sound: no Options pages (see above): the stock Sound row; [Sound] in SH4Refined.ini still applies");
        return;
    }
    r = t ? t + 5 * 7 : NULL;
    if (!t || g_lpCount[0] < 7 || r[0] != 0x524B60 || r[1] != 0x413C70 || r[3] != 0x654C20 || r[4] != 3 || r[5] != 0x24 ||
        r[6] != 0x525460) {
        Log("sound: Option 1's Sound row is not the stock one (%p, %d rows): it stays Monaural / Stereo / Dolby Digital (the "
            "byte decides the mode; Headphones only from the ini)", (const void *)t, g_lpCount[0]);
        return;
    }
    if (t == (DWORD *)0x654CC0) {  // no Menu Font copy: our own copy (the stock table has no terminator: by count)
        memcpy(g_rowsSnd1, t, 7 * 7 * 4);
        g_rowsSnd1[7 * 7] = 0;
        t = g_rowsSnd1;
        g_lpTable[0] = t;
        r = t + 5 * 7;
    }
    r[0] = (DWORD)(ULONG_PTR)SndRowIn;
    r[1] = (DWORD)(ULONG_PTR)SndModeGet;
    r[3] = (DWORD)(ULONG_PTR)g_sndModeIds;
    r[4] = SND_MODE_N;
    r[6] = (DWORD)(ULONG_PTR)SndRowDesc;
    g_sndRowOn = 1;
    if (g_sndPageBuilt && g_subScreenOn) {
        g_sndMenu = 1;
        g_lpParent[LP_SND] = 0;
        g_lpParentRow[LP_SND] = 5;
    }
    SndTextsDynamic();
    LogV("sound: Options > Option 1 > Sound: Mono / Stereo / Headphones / Surround (3D)%s",
         g_sndMenu ? "; Enter opens the Sound page (Stereo Width, Room Reverb, Night Mode)" : " (no sub-screen fade: no Sound page)");
}
static int SndMenuOn(void) { return g_sndMenu; }
static void SndSnapshot(void)
{
    g_snapSndMode = SndModeGet(), g_snapSndWidth = g_sndWidth, g_snapSndReverb = g_sndReverb, g_snapSndNight = g_sndNight;
}
static int SndChanged(void)
{
    return g_snapOn && g_sndRowOn && (SndModeGet() != g_snapSndMode || g_sndWidth != g_snapSndWidth || g_sndReverb != g_snapSndReverb ||
                                      g_sndNight != g_snapSndNight);
}
static int SndRowChanged(int row)  // Option 1's row 5; -1 = not ours
{
    if (!g_sndRowOn || row != 5) return -1;
    return g_snapOn && SndModeGet() != g_snapSndMode;
}
static int SndPageRowChanged(int row)
{
    if (!g_snapOn) return 0;
    return row == 0 ? g_sndWidth != g_snapSndWidth : row == 1 ? g_sndReverb != g_snapSndReverb : row == 2 ? g_sndNight != g_snapSndNight : 0;
}
// DiscardOwn ("No" at "Do you want to change the options?", after the game copied its backup back and ran 0x524320, which
// does not apply Sound): the rows back to how Options opened, and the engine to the Sound byte the backup holds
static void SndDiscard(void)
{
    if (!g_sndOn) return;
    if (g_sndRowOn && g_snapSndMode >= 0 && SndModeGet() != g_snapSndMode) SndSetMode((int)g_snapSndMode, "Options discarded");
    if (g_sndWidth != g_snapSndWidth) SndSetWidth((int)g_snapSndWidth, "Options discarded");
    if (g_sndReverb != g_snapSndReverb) SndSetReverb((int)g_snapSndReverb, "Options discarded");
    if (g_sndNight != g_snapSndNight) SndSetNight((int)g_snapSndNight, "Options discarded");
    if (SND_BYTE <= 2 && SND_CACHE != SND_BYTE) {
        Log("sound: Options discarded: the engine back to Sound %d (it kept %d, the last value passed over)", SND_BYTE, SND_CACHE);
        SND_APPLY(SND_BYTE);
    }
}
