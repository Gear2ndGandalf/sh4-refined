// v3.53 OL2: the Options look (staging/options_layout/PLAN.md section 3, round R2): pure helpers, no game access
// (test\look_sim_test.c, test\build_look_test.cmd). look.inc draws with them.
//   LkDecode / LkEncode  the game's byte-mode strings (00 80 header, 00 = space, 0x100 - c, FF FF end) <-> ASCII
//   LkLeaf               a page's own name: its title as it is, spaces trimmed (v3.53 MERGE: w2 OL1's titles are plain
//                        names; no "Option N (Name)" / " > " / ": " parsing that could cut a name)
//   LkCrumb              a sub page's title "Parent > Page" (the last two levels, at most LK_TITLE_MAX characters)
//   LkGlyphs             the glyphs a string queues (the text queue holds 800 a frame: 5570B0)
//   LkComposeLine        a grey row's description: the reason first, then the row's own line, one line of at most 52
//                        characters (v3.53 MERGE: was LkCompose, the two on two lines)
//   LkStripFit           the page strip's gap and letter spacing so that the six names fit the canvas
#ifndef LOOK_SIM_H
#define LOOK_SIM_H
#include <string.h>
#include "ol_words.h"  // v3.53 MERGE: the reasons name w2 OL1's rows (the generated words of the plan)

#define LK_TITLE_MAX 28  // PLAN section 3: "Gameplay > Enemies"; the last two levels only, at most 28 characters
#define LK_DESC_MAX 52   // PLAN section 3: one description line of at most 52 characters
#define LK_TXT 64        // an ASCII name / title buffer

// The reasons a dependent row is grey (PLAN section 3: "The description starts with the reason"), one per rule of
// layout_data.py's DEPS, worded with the plan's row names (R1's labels; v3.53 MERGE: built from ol_words.h, so they are
// always w2 OL1's row and value names).
enum { LKR_GRAIN, LKR_DOF, LKR_ECHO, LKR_WIDTH, LKR_SHADOW, LKR_CAM, LKR_MARGIN, LKR_FILTER, LKR_N };  // v3.58 PV358: + LKR_FILTER
static const char *const kLkReason[LKR_N] = {
    "Needs " OLW_L_FILM_GRAIN " on.",  // Grain Strength / Type / Colour (Film Grain = the stock Noise Effect, cfg+0x14)
    "Needs " OLW_DOF_DEPTH_OF_FIELD_L " on.",  // Blur Strength (was DoF Strength)  // v3.58 PV358: Effects > Depth of Field > Strength
    "Needs " OLW_POWERS_ECHO_DOUBLES_L " on.",  // Echo Count
    "Needs a " OLW_SOUND_SOUND_MODE_L " other than " OLW_SOUND_SOUND_MODE_V_MONO ".",  // Stereo Width (Stereo, Headphones or Surround)
    "Needs " OLW_L_SHADOWS " on.",  // Shadows > Edge / Darkness (Shadows = the stock Realtime Shadow, cfg+0x19)
    "Needs the " OLW_CAMERA_CAMERA_V_SHOULDER " camera.",  // the camera's settings (Camera = Game)
    "Needs " OLW_STEERING_WALL_ZOOM_L " on.",  // Wall Margin
    "Needs a " OLW_EFFECTS_FILTER_L " other than " OLW_EFFECTS_FILTER_V_NONE ".",  // v3.58 PV358: Filter Strength (Filter None)
    // v3.53 MERGE: the same seven texts as w3 OL2 wrote, now from w2's names
};

// 1 = s is a plain byte-mode string (characters and spaces only), out its ASCII; 0 = anything else (out "")
static int LkDecode(const unsigned char *s, char *out, int n)
{
    int i = 2, k = 0;
    if (n <= 0) return 0;
    out[0] = 0;
    if (!s || s[0] != 0x00 || s[1] != 0x80) return 0;
    for (; i < 512; ++i) {
        const unsigned char c = s[i];
        if (c == 0xFF) {
            if (s[i + 1] != 0xFF) break;  // a control: not plain
            out[k] = 0;
            return 1;
        }
        if (c >= 0xE0 || k + 1 >= n) break;
        out[k++] = c == 0x00 ? ' ' : (char)(0x100 - c);
    }
    out[0] = 0;
    return 0;
}

// as sh4windowmod.c's Encode: the bytes written, FF FF included (0 = no room)
static int LkEncode(unsigned char *out, int n, const char *s)
{
    int k = 0;
    if (n < 4) return 0;
    out[k++] = 0x00;
    out[k++] = 0x80;
    for (; *s && k + 2 < n; ++s) out[k++] = *s == ' ' ? 0 : (unsigned char)(0x100 - (unsigned char)*s);
    out[k++] = 0xFF;
    out[k++] = 0xFF;
    return k;
}

static void LkCopy(char *out, int n, const char *s, int len)
{
    int k;
    if (n <= 0) return;
    while (len > 0 && *s == ' ') ++s, --len;
    while (len > 0 && s[len - 1] == ' ') --len;
    for (k = 0; k < len && k + 1 < n; ++k) out[k] = s[k];
    out[k] = 0;
}

// the page's own name: its title (v3.53 MERGE: w2 OL1's titles are plain names, "Display" .. "Pause Menu", "Subtitles" ..;
// taken whole, so no name is cut; the crumb's parent comes from the page tree, not from the title)
static void LkLeaf(const char *t, char *out, int n) { LkCopy(out, n, t, (int)strlen(t)); }

// "Parent > Page": 1 = composed; 0 = no parent or longer than maxChars (out = page, the caller keeps the page's own title)
static int LkCrumb(const char *parent, const char *page, int maxChars, char *out, int n)
{
    const int lp = (int)strlen(parent), lg = (int)strlen(page);
    if (lp && lg && lp + 3 + lg <= maxChars && lp + 3 + lg < n) {
        memcpy(out, parent, lp);
        memcpy(out + lp, " > ", 3);
        memcpy(out + lp + 3, page, lg + 1);
        return 1;
    }
    LkCopy(out, n, page, lg);
    return 0;
}

// the glyphs a string queues (byte mode: no space, no control; word mode: every u16 below the controls but 0)
static int LkGlyphs(const unsigned char *s)
{
    int i, g = 0;
    if (!s) return 0;
    if (s[0] == 0x00 && s[1] == 0x80) {
        for (i = 2; i < 2048;) {
            const unsigned char c = s[i];
            if (c == 0xFF) {
                if (s[i + 1] == 0xFF) break;
                i += 2;
            } else if (c == 0xE0) {
                ++g, i += 3;  // a u16 character
            } else if (c == 0xF0) {
                ++g, i += 2;  // an inline icon
            } else if (c == 0xFC || c == 0xFD || c == 0xFE) {
                i += 2;  // a move
            } else {
                if (c != 0x00) ++g;
                ++i;
            }
        }
        return g;
    }
    for (i = 0; i < 2048; i += 2) {
        const unsigned c = (unsigned)s[i] | (unsigned)s[i + 1] << 8;
        if (c == 0xFFFF) break;
        if (c != 0 && c < 0xE0) ++g;
        else if ((c & 0xFF00) == 0xF000) ++g;
    }
    return g;
}

// v3.53 MERGE (w2 OL1 x w3 OL2): a grey row's description, in ASCII: the reason, then " " and the row's own line when the
// two fit max characters (PLAN section 3: one line of at most 52), else the own line's first sentences that fit (cut after
// a "." that ends a sentence), else the reason alone. The characters written (out ends with 0).
static int LkComposeLine(const char *reason, const char *own, int max, char *out, int n)
{
    int lr, lo = 0, cut = 0, k;
    if (n <= 0) return 0;
    if (max > n - 1) max = n - 1;
    LkCopy(out, max + 1 < n ? max + 1 : n, reason ? reason : "", reason ? (int)strlen(reason) : 0);
    lr = (int)strlen(out);
    if (!own) return lr;
    while (*own == ' ') ++own;
    lo = (int)strlen(own);
    while (lo > 0 && own[lo - 1] == ' ') --lo;
    if (lo <= 0 || lr == 0) return lr;
    if (lr + 1 + lo <= max) {
        cut = lo;
    } else {
        for (k = 1; k <= lo && lr + 1 + k <= max; ++k)
            if (own[k - 1] == '.' && (k == lo || own[k] == ' ')) cut = k;
    }
    if (cut <= 0) return lr;
    out[lr] = ' ';
    memcpy(out + lr + 1, own, cut);
    out[lr + 1 + cut] = 0;
    return lr + 1 + cut;
}

// The strip's layout: w[i] each name's width at letter spacing sp (canvas px), adv[i] its characters (each one carries the
// spacing). The gap goes from gapMax down to gapMin, then the spacing from sp down to 1.0 (as descfit's tighter spacing).
// Out: the gap, the spacing to draw with, the strip's total width. 1 = it fits room; 0 = still wider (drawn centred anyway).
static int LkStripFit(const float *w, const int *adv, int n, float sp, float room, float gapMin, float gapMax, float *gap,
                      float *spOut, float *total)
{
    float sum = 0.0f, need;
    int i, a = 0;
    *gap = gapMax, *spOut = sp, *total = 0.0f;
    if (n <= 0) return 1;
    for (i = 0; i < n; ++i) sum += w[i], a += adv[i];
    if (n == 1 || sum + (n - 1) * gapMax <= room) {
        *total = sum + (n - 1) * gapMax;
        return 1;
    }
    if (sum + (n - 1) * gapMin <= room) {
        *gap = (room - sum) / (float)(n - 1);
        *total = room;
        return 1;
    }
    *gap = gapMin;
    need = sum + (n - 1) * gapMin - room;  // px to win back from the spacing (1.25 font units a canvas px)
    if (a > 0 && sp > 1.0f) {
        float s = sp - need * 1.25f / (float)a;
        if (s < 1.0f) s = 1.0f;
        *spOut = s;
        sum -= (float)a * (sp - s) / 1.25f;
    }
    *total = sum + (n - 1) * gapMin;
    return *total <= room + 0.01f;
}

// a name's width at another spacing (as LkStripFit counts it)
static float LkWidthAt(float w, int adv, float sp, float sp2) { return w - (float)adv * (sp - sp2) / 1.25f; }

// v3.55 OL56 (R6): the page strip's mouse click (PLAN section 3: "You can click one with the mouse").
// The name under the cursor (cx, cy; the 512 x 448 canvas): the names as drawn at rest (x, w each), the strip's top y and
// height h; each name's box reaches LK_HIT_PAD px past its ends (half of the strip's smallest gap, 6: no two boxes meet).
// -1 = none.
#define LK_HIT_PAD 2.0f
static int LkStripHit(const float *x, const float *w, int n, float y, float h, float cx, float cy)
{
    int i;
    if (!x || !w || cy < y || cy >= y + h) return -1;
    for (i = 0; i < n; ++i)
        if (w[i] > 0.0f && cx >= x[i] - LK_HIT_PAD && cx < x[i] + w[i] + LK_HIT_PAD) return i;
    return -1;
}
// how a click on top page t is reached from page cur (pages 0 .. nTop - 1 are the cycle; cur >= nTop is a hidden page):
// the slide when cur is a top page and t lives in another slot (slot = id mod 3: the slide refills the target slot, which
// must not be the one on screen); else the fade route (SubScreen loads any page while the screen is black: from a hidden
// page, or to the top page in the same slot); none for the page shown, a bad id, or no fade route
enum { LK_GO_NONE, LK_GO_SLIDE, LK_GO_FADE };
static int LkStripRoute(int cur, int t, int nTop, int fadeOn)
{
    if (t < 0 || t >= nTop || t == cur || cur < 0) return LK_GO_NONE;
    if (cur < nTop && t % 3 != cur % 3) return LK_GO_SLIDE;
    return fadeOn ? LK_GO_FADE : LK_GO_NONE;
}

#endif
