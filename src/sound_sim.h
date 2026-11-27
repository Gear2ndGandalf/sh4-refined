// v3.52 SND352: the sound round's pure maths (staging/v352_feedback/w1_sound/PLAN.md; research staging/research_sound/
// REPORT.md sections 1.3-1.6, 3). No game memory: sound.inc builds its tables and decides with these; test\sound_sim_test.c
// runs them. Volumes and pans are DirectSound's units: hundredths of a dB (volume 0 = full, -10000 = silent; pan + = right,
// the other channel attenuated by |pan| / 100 dB, never boosted).
//
// The game (GOG v7, checked against SH4Decomp/orig/sh4.exe md5 15a85581, 2026-11-26):
//   SetVolumePan 0x409040(slot, vol, pan 0..127): if mode != 0, SetPan((pan - 63) * 156) (0x4090BD `add ecx,-3Fh` /
//   `imul ecx,ecx,9Ch`): 1.56 dB of the other ear per pan step, so a sound 20-30 degrees off the camera's axis is in one ear
//   only. Streams (ApplyVolume 0x40B550): SetPan(panL * 666) for panL -15..15 (0x40B617 `imul eax,eax,29Ah`): 6.7 dB a step.
//   The 2D pan is (sin(azimuth from the camera) + 1) / 2 * 127 (0x56C5E0), so p - 63 is already a sine of the angle.
// The smooth law: x = the pan's position -1..1 (63 = 0; 127 = 1, 0 = -1), times Stereo Width; equal-power gains
// L = cos((x + 1) pi / 4), R = sin(...); DirectSound can only attenuate one side, so the pan is their ratio,
// 2000 log10(R / L) (centre 0, hard sides +-10000). Headphones: the attenuation of the other ear stops at the floor
// (-9 dB by default: every sound reaches both ears, as with speakers in a room). The report's table (other ear):
//   off centre (deg)    0   5   10  15  20  30  45  90
//   stock (dB)          0  -9  -17 -25 -34 -50 -70 -100
//   equal-power (dB)    0  -1  -2  -3  -5  -8  -12  off
// Night Mode: a range compression of the volume each sound is SET to (the three SetVolume calls 0x4090A6 2D, 0x409285 3D,
// 0x40B5E7 streams): dB' = P + (dB - P) * ratio, P = -20 dB, ratio Low 0.7 / High 0.5; silence (<= -90 dB) stays silent,
// nothing goes above 0 dB. Quiet sounds come up, the loudest go down: the spread of the levels the game sets narrows (it is
// not a signal compressor: a clip's own peaks stay).
#ifndef SOUND_SIM_H
#define SOUND_SIM_H

#include <math.h>
#include <string.h>

enum { SND_MODE_MONO, SND_MODE_STEREO, SND_MODE_HEADPHONES, SND_MODE_SURROUND, SND_MODE_N };
static const char *const kSndModeName[SND_MODE_N] = { "Mono", "Stereo", "Headphones", "Surround" };          // the ini
static const char *const kSndModeLabel[SND_MODE_N] = { "Mono", "Stereo", "Headphones", "Surround (3D)" };   // the row
static const char *const kSndByteName[3] = { "Monaural", "Stereo", "Dolby Digital" };  // the game's own Sound values

#define SND_PAN_MAX 10000      // DSBPAN_RIGHT
#define SND_VOL_MIN (-10000)   // DSBVOLUME_MIN
#define SND_SILENT (-9000)     // at or below: left alone by Night Mode (the game's floor for a 2D sound is -6000)
#define SND_NIGHT_PIVOT (-2000)
#define SND_FLOOR_DEF 900      // Headphones: the other ear at most 9 dB down (hidden [Sound] HeadphonesFloor 6..12 dB)
#define SND_FLOOR_MIN 600
#define SND_FLOOR_MAX 1200
#define SND_WIDTH_STEP 10      // the Stereo Width row: 0, 10, .., 100 %
#define SND_NIGHT_N 3
static const char *const kSndNightName[SND_NIGHT_N] = { "Off", "Low", "High" };
static const int kSndNightRatio[SND_NIGHT_N] = { 100, 70, 50 };  // percent
#define SND_ONE_EAR 4000       // |pan| at or over: the other ear 40 dB down or more ("one ear only" in the log)

// The game's Option 1 Sound byte (cfg+0xC: 0 Monaural, 1 Stereo, 2 Dolby Digital) of a mode, and back. Headphones is the
// Stereo byte (the stock save stays 0 / 1 / 2): byte 1 keeps Headphones when that was the mode.
static int SndModeByte(int mode) { return mode == SND_MODE_MONO ? 0 : mode == SND_MODE_SURROUND ? 2 : 1; }
static int SndModeFromByte(int b, int prev)
{
    if (b == 0) return SND_MODE_MONO;
    if (b == 2) return SND_MODE_SURROUND;
    return prev == SND_MODE_HEADPHONES ? SND_MODE_HEADPHONES : SND_MODE_STEREO;  // 1, or an edited save's other byte
}
// The ini's Mode text (case does not matter); -1 = not a mode. "Monaural" / "Dolby" / "Dolby Digital" (the game's own
// names) and "Surround (3D)" (the row's) read too. No numbers: 0 / 1 / 2 would be taken for the game's byte.
static int SndParseMode(const char *s)
{
    static const struct { const char *t; int m; } k[] = {
        { "Mono", SND_MODE_MONO }, { "Monaural", SND_MODE_MONO }, { "Stereo", SND_MODE_STEREO },
        { "Headphones", SND_MODE_HEADPHONES }, { "Headphone", SND_MODE_HEADPHONES }, { "Surround", SND_MODE_SURROUND },
        { "Surround (3D)", SND_MODE_SURROUND }, { "Dolby", SND_MODE_SURROUND }, { "Dolby Digital", SND_MODE_SURROUND },
    };
    int i;
    char b[32];
    size_t n;
    if (!s) return -1;
    while (*s == ' ' || *s == '\t') ++s;
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) --n;
    if (!n || n >= sizeof b) return -1;
    memcpy(b, s, n);
    b[n] = 0;
    for (i = 0; i < (int)(sizeof k / sizeof k[0]); ++i)
        if (!_stricmp(b, k[i].t)) return k[i].m;
    return -1;
}
// Off / On (also 0 / 1); -1 = neither
static int SndParseOnOff(const char *s)
{
    if (!s) return -1;
    while (*s == ' ') ++s;
    if (!_stricmp(s, "On") || !strcmp(s, "1")) return 1;
    if (!_stricmp(s, "Off") || !strcmp(s, "0")) return 0;
    return -1;
}
// Night Mode Off / Low / High (also 0 / 1 / 2); -1 = none
static int SndParseNight(const char *s)
{
    int i;
    if (!s) return -1;
    while (*s == ' ') ++s;
    for (i = 0; i < SND_NIGHT_N; ++i)
        if (!_stricmp(s, kSndNightName[i]) || (s[0] == (char)('0' + i) && !s[1])) return i;
    return -1;
}
// Stereo Width 0..100 (a number, "%" allowed); -1 = not a number in range
static int SndParseWidth(const char *s)
{
    int v = 0, d = 0;
    if (!s) return -1;
    while (*s == ' ') ++s;
    for (; *s >= '0' && *s <= '9' && d < 4; ++s, ++d) v = v * 10 + (*s - '0');
    while (*s == ' ') ++s;
    if (*s == '%') ++s;
    if (!d || *s || v > 100) return -1;
    return v;
}

// The game's own pans (the PanLaw=Stock reference)
static int SndPanStock2D(int p) { return (p - 63) * 156; }
static int SndPanStockStream(int p) { return p * 666; }

// The position -1..1 of a 2D pan 0..127 (63 = centre; out of range = clamped)
static double SndPos2D(int p)
{
    if (p < 0) p = 0;
    if (p > 127) p = 127;
    return p >= 63 ? (p - 63) / 64.0 : (p - 63) / 63.0;
}
static double SndPosStream(int p)
{
    if (p < -15) p = -15;
    if (p > 15) p = 15;
    return p / 15.0;
}
// The smooth (equal-power) law at x -1..1: width 0..100 %, floor = Headphones' cap of the other ear's attenuation (0 = none)
static int SndPanSmooth(double x, int widthPct, int floor)
{
    const double kPi = 3.14159265358979323846;
    double th, l, r, pan;
    if (widthPct < 0) widthPct = 0;
    if (widthPct > 100) widthPct = 100;
    x *= widthPct / 100.0;
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;
    th = (x + 1.0) * kPi / 4.0;
    l = cos(th), r = sin(th);
    if (l <= 1e-5) pan = SND_PAN_MAX;
    else if (r <= 1e-5) pan = -SND_PAN_MAX;
    else pan = 2000.0 * log10(r / l);
    if (pan > SND_PAN_MAX) pan = SND_PAN_MAX;
    if (pan < -SND_PAN_MAX) pan = -SND_PAN_MAX;
    if (floor > 0 && pan > floor) pan = floor;
    if (floor > 0 && pan < -floor) pan = -floor;
    return (int)(pan >= 0 ? pan + 0.5 : pan - 0.5);
}
// The tables the two pan hooks read: t2d[128] by the game's 2D pan, tstr[31] by the stream pan + 15
static void SndBuildPans(int *t2d, int *tstr, int widthPct, int floor)
{
    int p;
    for (p = 0; p < 128; ++p) t2d[p] = SndPanSmooth(SndPos2D(p), widthPct, floor);
    for (p = -15; p <= 15; ++p) tstr[p + 15] = SndPanSmooth(SndPosStream(p), widthPct, floor);
}
static int SndAbs(int v) { return v < 0 ? -v : v; }

// v3.52 MERGE: the frame-end keeper (sound.inc SndFrameTick) and Quick Load from the pause (QL352, extramenu.inc).
// The keeper's re-apply (0x56C420 -> SndMgr 0x40F880: every active request is played again at its stored volume)
// waits while QL352 holds the sound (its fade to black, the leave, the load, until the loaded room's one
// 0x56E960(-1)), then runs on the first frame after. QL352 gives the sound back only through the game's own
// 0x56E960(-1), so the volumes come through the three SetVolume sites (Night Mode) and the decoder / LoadMusic
// (the Mono fold) like any other; Night Mode leaves silence (SND_SILENT and below) alone, so a master of 0 stays silent.
enum { SND_KEEP_NONE, SND_KEEP_APPLY, SND_KEEP_WAIT };
static int SndKeepStep(int engineMode, int soundByte, int qlHeld)
{
    if (engineMode == soundByte) return SND_KEEP_NONE;
    return qlHeld ? SND_KEEP_WAIT : SND_KEEP_APPLY;
}

// Night Mode on a volume the game sets (hundredths of a dB)
static int SndNightDb(int db, int level)
{
    int out;
    if (level <= 0 || level >= SND_NIGHT_N || db <= SND_SILENT) return db;
    if (db > 0) db = 0;
    out = SND_NIGHT_PIVOT + (db - SND_NIGHT_PIVOT) * kSndNightRatio[level] / 100;
    if (out > 0) out = 0;
    if (out < SND_VOL_MIN) out = SND_VOL_MIN;
    return out;
}

// Mono: a stereo sample pair folded to one channel (the stock Monaural kept the left channel of a stereo stream only)
static short SndFold(short l, short r) { return (short)(((int)l + (int)r) >> 1); }
// interleaved 16-bit stereo, frames pairs: both channels set to the fold (idempotent: a folded pair folds to itself)
static void SndFoldStereo16(short *s, unsigned frames)
{
    unsigned i;
    for (i = 0; i < frames; ++i) s[2 * i] = s[2 * i + 1] = SndFold(s[2 * i], s[2 * i + 1]);
}

// DirectSound's speaker configuration (GetSpeakerConfig) in words
static const char *SndSpeakerName(unsigned long cfg)
{
    switch (cfg & 0xFF) {
    case 0: return "direct out";
    case 1: return "headphones";
    case 2: return "mono";
    case 3: return "quad";
    case 4: return "stereo";
    case 5: return "surround";
    case 6: return "5.1 (back)";
    case 7: return "7.1 (wide)";
    case 8: return "7.1 surround";
    case 9: return "5.1 surround";
    }
    return "unknown";
}

// ---------------------------------------------------------------- v3.56 SD356: the background, keep-awake and device watch
// (staging/v356_feedback/w8_sound/PLAN.md; the user's v3.55 issues 4 and 5). Checked against SH4Decomp/orig/sh4.exe:
// - The game's DirectSound: DirectSoundCreate8(NULL) (the default device at that moment: DirectSound stays on it for the
//   session), SetCooperativeLevel(hwnd [0x6E7D90], DSSCL_PRIORITY) (SndMgr 0x40C0F0). Its buffers are made per play, none with
//   DSBCAPS_GLOBALFOCUS (0x8000): DirectSound mutes them while another window is in front (Refined keeps the game running
//   behind other windows, so its sounds go on, muted; a wireless headset then gets a silent stream and may fall asleep, and
//   takes seconds to wake when the game comes back).
// - The four places the game makes a buffer; each one's flags carry 0x8000 when the flags' second byte is 0x80 (the
//   immediate's bits 8..15; every stock one is 0x00): Background Play writes 0x80, Mute the stock 0x00 (one byte, one store).
#define SND_DSBCAPS_GLOBALFOCUS 0x8000u
enum { SND_BG_MUTE, SND_BG_PLAY, SND_BG_N };
static const char *const kSndBgName[SND_BG_N] = { "Mute", "Play" };
typedef struct { unsigned long va; int len; unsigned char stock[8]; int at; const char *what; } SndBgSite;
static const SndBgSite kSndBgSites[4] = {
    { 0x4095FE, 6, { 0x81, 0xC2, 0xC0, 0x00, 0x00, 0x00 }, 3, "Play2D 00409530 (add edx,0C0h)" },
    { 0x409B02, 5, { 0x05, 0x90, 0x00, 0x00, 0x00 }, 2, "Play3D 00409A30 (add eax,90h)" },
    { 0x40A14C, 8, { 0xC7, 0x44, 0x24, 0x38, 0xC0, 0x00, 0x00, 0x00 }, 5, "PlayMusic 0040A080 (mov [esp+38h],0C0h)" },
    { 0x40A723, 8, { 0xC7, 0x44, 0x24, 0x20, 0xC0, 0x00, 0x00, 0x00 }, 5, "SndVoice::CreateBuffer 0040A6D0 (mov [esp+20h],0C0h)" },
};
static unsigned char SndBgByte(int play) { return (unsigned char)(play ? SND_DSBCAPS_GLOBALFOCUS >> 8 : 0); }
// [Sound] Background: Mute / Play (also 0 / 1); -1 = neither
static int SndParseBg(const char *s)
{
    if (!s) return -1;
    while (*s == ' ') ++s;
    if (!_stricmp(s, "Mute") || !strcmp(s, "0")) return SND_BG_MUTE;
    if (!_stricmp(s, "Play") || !strcmp(s, "1")) return SND_BG_PLAY;
    return -1;
}
// The keep-awake signal: a 45 Hz sine (490 samples a period at 22050 Hz; one second = 45 whole periods, so the loop has no
// seam), mono 16-bit, amplitude `level` of 32767 (8 = -72 dBFS, far below hearing at 45 Hz; 0 = digital silence, which
// still keeps the stream open but not a headset that sleeps on silence). It never carries pure digital silence.
#define SND_KA_RATE 22050
#define SND_KA_PERIOD 490
#define SND_KA_SAMPLES 22050
#define SND_KA_LEVEL_DEF 8
#define SND_KA_LEVEL_MAX 64
static void SndKeepAwakeFill(short *s, int n, int level)
{
    const double kPi = 3.14159265358979323846;
    int i;
    if (level < 0) level = 0;
    if (level > SND_KA_LEVEL_MAX) level = SND_KA_LEVEL_MAX;
    for (i = 0; i < n; ++i) {
        const double v = level * sin(2.0 * kPi * (i % SND_KA_PERIOD) / SND_KA_PERIOD);
        s[i] = (short)(v >= 0 ? v + 0.5 : v - 0.5);
    }
}
static int SndKeepLevelOk(int v) { return v >= 0 && v <= SND_KA_LEVEL_MAX; }
// its peak in tenths of a dB below full scale (8 -> -722); level 0 -> -1000 (silence)
static int SndKeepLevelDb10(int level)
{
    double d;
    if (level <= 0) return -1000;
    d = 200.0 * log10(level / 32768.0);
    return (int)(d >= 0 ? d + 0.5 : d - 0.5);
}
// The device watch (IMMNotificationClient::OnDefaultDeviceChanged, render / console role: DirectSound's default device).
// isGame = the new default is the device the game's DirectSound plays on; *pending = a different default was announced.
enum { SND_DEV_SAME, SND_DEV_NEW, SND_DEV_NEW_AGAIN, SND_DEV_BACK };
static int SndDevStep(int isGame, int *pending)
{
    if (!isGame) {
        const int was = *pending;
        *pending = 1;
        return was ? SND_DEV_NEW_AGAIN : SND_DEV_NEW;
    }
    if (*pending) {
        *pending = 0;
        return SND_DEV_BACK;
    }
    return SND_DEV_SAME;
}
// a device's state (DEVICE_STATE_ACTIVE 1, DISABLED 2, NOTPRESENT 4, UNPLUGGED 8) in words
static const char *SndDevStateName(unsigned long st)
{
    switch (st) {
    case 1: return "active";
    case 2: return "disabled";
    case 4: return "not present";
    case 8: return "unplugged";
    }
    return "in an unknown state";
}
// a device name (UTF-16) for the log: ASCII only (anything else '?'), at most n - 1 characters, no control characters
static void SndDevNameAscii(const unsigned short *w, char *out, int n)
{
    int i = 0;
    if (n <= 0) return;
    if (w)
        for (; *w && i < n - 1; ++w) out[i++] = *w >= 0x20 && *w < 0x7F && *w != '"' ? (char)*w : '?';
    out[i] = 0;
}

// ---------------------------------------------------------------- v3.58 SD358: Volume 0 = silent; Mute by the sound session
// (staging/v358_feedback/w8_sound/PLAN.md; the user's v3.57 QA: "Music volume at zero does not fully silence the music" and
// "when setting to Mute again the app never mutes in the background on my headset"). Checked against SH4Decomp/orig/sh4.exe:
// - SndPack::SetVolumePan 0x409040 (every 2D sound and every layer of the seq*.pac music) and SndVoice::ApplyVolume 0x40B550
//   (every stream: bgm.sh4, se.sh4, the world's bank, the scene streams) hand DirectSound
//   2000 log10(max(0.2 x 10^(v / 2000), 0.001)) (the mode-2 attenuation is unconditional: the post-link nops at 0x40904B /
//   0x40B59C): 14 dB down and never below -60 dB. The game's own "0" (SndDb(0) = -10000 for a sound; the stream table
//   0x69CA18's level 0 = -960, x 10 = -9600) so reaches DirectSound as -6000: at Music Volume 0 the music still plays at
//   -60 dB, and Night Mode (which lifts quiet sounds) made that -40 dB (High) / -48 dB (Low). SetVolume3D 0x409230 hands
//   2000 log10(min(1.5 x 10^(v / 2000), 1)): -10000 -> -9648, -9600 -> -9247 (silent enough, made exact here too).
// - ApplyVolume adds 500 to stream 0x2774's volume at every update (its dirty flag is never cleared) up to 0 dB, whatever the
//   game set (se.sh4 #100, which the bank table gives Room 302's rl.sh4 #20): the level the game set is the one
//   SndVoice::SetVolume 0x40B2D0 stores (0x40B2E7 `mov [eax+12C010h], ecx`).
#define SND_ZERO_RAW (-9600)      // a level the game sets at or below this is its own "0": DirectSound's silence
#define SND_ZERO_3D_OUT (-9247)   // SetVolume3D's output for -9600 and below (x 1.5, truncated)
#define SND_STR_UNSET 1           // no SetVolume seen for a stream channel yet (every level is <= 0)
// the game's own two attenuations (as the exe: float maths, truncated by _ftol); the test compares Refined's with them
static int SndGameAtt2D(int v)
{
    double f = 0.2 * pow(10.0, v * 0.0005);
    if (f < 0.001) f = 0.001;
    return (int)(log10(f) * 2000.0);
}
static int SndGameAtt3D(int v)
{
    double f = 1.5 * pow(10.0, v * 0.0005);
    if (f > 1.0) f = 1.0;
    return (int)(log10(f) * 2000.0);
}
// what DirectSound gets: the game's own value, or silence when the game set its "0" (raw = the level before the attenuation)
static int SndZero2D(int raw, int out) { return raw <= SND_ZERO_RAW ? SND_VOL_MIN : out; }
static int SndZero3D(int out) { return out <= SND_ZERO_3D_OUT ? SND_VOL_MIN : out; }
// a stream: set = the level the game last set on its channel (SND_STR_UNSET: none seen: the level ApplyVolume works with)
static int SndZeroStream(int set, int raw, int out) { return (set <= 0 ? set : raw) <= SND_ZERO_RAW ? SND_VOL_MIN : out; }

// Background Mute by the sound session: Windows' audio sessions of the game's process (DirectSound's stream, the films'
// DirectShow sound and the keep-awake tone are all in them) are muted while another window is in front, and only those
// Refined muted are unmuted. Then DirectSound's own focus rule is not needed: the four buffer sites carry the global focus in
// both modes, so a switch either way applies at once to every sound. Without a session to mute (no COM, no session of the
// process found, SetMute refused) Mute falls back to v3.56: the game's own focus rule for sounds made from then on, and the
// keep-awake tone stopped behind other windows.
static int SndBgWantMute(int bg, int behind) { return bg == SND_BG_MUTE && behind; }
static int SndBgSitesPlay(int bg, int sessOk) { return bg == SND_BG_PLAY || sessOk; }  // 1: the sites' byte is 0x80
static int SndKeepHold(int wantMute, int sessOk) { return wantMute && !sessOk; }       // 1: the keep-awake tone stops
// v3.58 SD358-001 (Codex): Refined's mutes are told apart by the session's identity, never by "a marker exists".
// - A session's identifier (IAudioSessionControl2::GetSessionIdentifier: the endpoint, the exe's path and the session GUID)
//   is the same at every start of the game. SH4Refined.muted names each session Refined holds muted (written BEFORE the
//   SetMute), with the time; only a session named there and still muted is unmuted at the next start. A muted session it does
//   not name (the user's own mute, or one whose identity cannot be read) is left muted and logged.
// - Refined's SetMute carries its own event context (kSdCtx). A held session gets an IAudioSessionEvents callback: a change
//   with another context (Windows' Volume Mixer, another program) is the user's: unmuted by them -> Refined does not mute it
//   again this time behind other windows; muted again by them -> theirs: Refined does not unmute it (the marker drops it).
//   A volume change that leaves the mute as it was changes nothing.
// One session at a mute sweep: 1 = Refined mutes it (held = its identity is in Refined's held list, touched or not; one
// already muted is the user's or Refined's own: left)
static int SndSessMuteAct(int mutedNow, int held) { return !mutedNow && !held; }
enum { SND_TOUCH_NONE, SND_TOUCH_UNMUTED, SND_TOUCH_USER };
// a held session changed by someone else (the event context is not Refined's): the new touch state; *lastMute follows it
static int SndSessTouchStep(int touched, int *lastMute, int mute)
{
    if (!mute) {
        *lastMute = 0;
        return SND_TOUCH_UNMUTED;  // the user unmuted it: they want to hear it
    }
    if (!*lastMute) {
        *lastMute = 1;
        return SND_TOUCH_USER;     // muted again after that: the user's mute now
    }
    return touched;                // a volume change only: the mute is as it was
}
// a held session when the game is in front again: 1 = unmute (still muted and nobody else touched it)
static int SndSessUnmuteAct(int mutedNow, int touched) { return mutedNow && touched == SND_TOUCH_NONE; }
// the marker names a held session while nobody else touched it (an unmuted or user-muted one is not Refined's to undo)
static int SndMarkKeep(int touched) { return touched == SND_TOUCH_NONE; }
// one session of the game at the start: 1 = unmute (muted and named in SH4Refined.muted: Refined's own mute, left by a game
// that ended behind other windows); 0 = leave (not muted, or muted and not named: the user's; the game is muted in Windows'
// Volume Mixer, which the log says)
static int SndSessStartAct(int mutedNow, int named) { return mutedNow && named; }
// SH4Refined.muted: a line "<seconds since 1970>\t<state>\t<session identifier, UTF-8>"; '#' lines are text. 1 = an entry
// (*t, *st, *id at the identifier, *n its length without the line end)
// v3.58 SD358-003 (Codex): each entry carries a state. "pending" is written BEFORE the SetMute (a mute that may or may not have
// happened: a failed SetMute whose clean-up write failed too leaves one); "muted" is written after the SetMute worked (a
// confirmed mute of Refined's). Only a confirmed entry may unmute a session at the next start (SndMarkMayUnmute): a pending one
// never does (the session is left as it is, the log says how to unmute it in the Volume Mixer, the entry is dropped). A line
// without a state ("<time>\t<identifier>": the v3.58 test builds before SD358-003, never installed) counts as confirmed, as
// those builds read it; an unknown state counts as pending (never an unmute).
#define SND_MARK_MAX_AGE (30ul * 24ul * 3600ul)  // an entry older than 30 days is dropped (its device never came back)
enum { SND_MARK_NONE, SND_MARK_PENDING, SND_MARK_MUTED };  // v3.58 SD358-003: none = not named
static int SndMarkParse(const char *line, unsigned long *t, int *st, const char **id, size_t *n)
{
    unsigned long v = 0;
    int d = 0, s = SND_MARK_MUTED;
    size_t k, tab = 0;
    if (!line || line[0] == '#') return 0;
    for (; *line >= '0' && *line <= '9' && d < 11; ++line, ++d) v = v * 10 + (unsigned long)(*line - '0');
    if (!d || *line != '\t') return 0;
    ++line;
    for (k = 0; line[k] && line[k] != '\r' && line[k] != '\n'; ++k)
        if (line[k] == '\t' && !tab) tab = k + 1;  // v3.58 SD358-003: a state field (a session identifier has no tab)
    if (tab) {
        s = tab == 6 && !memcmp(line, "muted", 5) ? SND_MARK_MUTED : SND_MARK_PENDING;  // "pending", or a state not known
        line += tab, k -= tab;
    }
    if (!k) return 0;
    *t = v, *st = s, *id = line, *n = k;
    return 1;
}
// v3.58 SD358-003: the state's word in the file
static const char *SndMarkWord(int st) { return st == SND_MARK_MUTED ? "muted" : "pending"; }
// v3.58 SD358-003: 1 = the entry may unmute its session at the start (a confirmed mute of Refined's only)
static int SndMarkMayUnmute(int st) { return st == SND_MARK_MUTED; }
// v3.58 SD358-003: an entry the start did not use (its session not found, or found and its unmute failed) is kept for a later
// start only when confirmed; a pending one is dropped (it can never authorize an unmute)
static int SndMarkKeepAtStart(int used, int st) { return !used && st == SND_MARK_MUTED; }
// v3.58 SD358-003: a held session's entry: pending until its SetMute worked, then confirmed
static int SndMarkHeldState(int confirmed) { return confirmed ? SND_MARK_MUTED : SND_MARK_PENDING; }
static int SndMarkExpired(unsigned long now, unsigned long t) { return t > now + 86400ul || now - t > SND_MARK_MAX_AGE; }

// v3.58 SD358-002 (Codex): SH4Refined.muted is written transactionally (a temporary file, every write whole, flushed, closed,
// then moved over the old one with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH). One write is whole when the call
// worked and wrote every byte (a short write is a failure)
static int SndMarkWriteOk(unsigned long want, unsigned long got, int callOk) { return callOk && got == want; }
// the new marker is in place only when every step worked; otherwise the temporary file is removed and the old marker stays
static int SndMarkCommit(int opened, int writesOk, int flushed, int closed, int replaced)
{
    return opened && writesOk && flushed && closed && replaced;
}
// a session is muted only after the marker that names it is in place (else: not muted, DirectSound's focus rule for it)
static int SndSessMayMute(int recorded) { return recorded != 0; }
// v3.58 SD358-003 (Codex): a marker write that failed leaves the file one step behind (a confirm not written, a failed
// attempt's pending entry not removed, a dropped session still named): 1 = write it again at the watch thread's next pass
// (the next focus change, the 2 s re-sweep behind other windows, a Volume Mixer change); a whole write clears it
static int SndMarkDirty(int wrote) { return !wrote; }

#endif
