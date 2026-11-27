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

#endif
