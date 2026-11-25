// v3.47 SB347: the save / load list's Bank row, its pure logic (src\savebanks.inc; native test test\savebanks_sim_test.c,
// test\build_sb_test.cmd). USER_FEEDBACK_v346.md item 4: the row in the Menu Font like the list rows, no "Q/E L1/R1:" hint,
// the mouse clicks the left / right bank arrows.
//
// Units: the menu canvas, 512 x 448 (the list rows are 24 high from y 0x43; the list's cursor bar runs x 32..480). The row
// is centred over the list: "Bank N" centred at x 256, the blue bar 192..320 behind it while the row is selected, and the
// game's selector arrows (56FB80's triangle, 20 x 24) 14 units outside the bar, only towards the banks one can still go to.
// The mouse: the row's hover box is bar + arrows (and the gaps between), so moving onto an arrow keeps the row (in v3.46
// the hover box was the bar alone: moving onto an arrow dropped the row and hid the arrows). A click on the left arrow's
// box = the previous bank, on the right arrow's box or the bar = the next bank (the bar: as v3.8 .. v3.46).
//
// The label: the game's ASCII path (556730 TextAsciiToRaw) turns plain letters into its special monospace glyph codes
// (>= 0xE0: the old stroke font, never the Menu Font); after the "\h" escape (half width) a character c becomes 0x100 - c,
// a Latin glyph of the current font (font 0 / 1): what the list's own "\h01." / "\h00:04" use, so the Menu Font draws it.
// A bare 'e' / 'E' after a backslash ends the text in 556730 (TextAsciiConvertEscapes): the label never has an 'e'.
#ifndef SAVEBANKS_SIM_H
#define SAVEBANKS_SIM_H

#include <stdio.h>

#define SB_ROW_CX 256     // the row's centre x (the list's middle)
#define SB_ROW_Y 0x24     // the row's top / text y (as v3.8 .. v3.46)
#define SB_ROW_H 24       // one list row
#define SB_BAR_HALF 64    // the bar 192..320 (v3.46: 326..456 beside the hint; "Bank 99?" is under 100 units wide)
#define SB_ARW_GAP 0x0E   // the arrow's base this far outside the bar (the table's sb_arw_gap)
#define SB_ARW_W 0x14     // the arrow's length (56FB80: 20)
#define SB_ARW_SLACK 4    // a click this far beyond an arrow's tip still counts
#define SB_LABEL_MAX 24   // the label buffer (bytes)

typedef struct { int x0, y0, x1, y1; } SbBox;  // [x0, x1) x [y0, y1)

typedef struct {
    int textX, textY;              // the label's centre x and top y (557660 align 1)
    SbBox bar;                     // the blue bar (drawn y + 2 .. + 0x18, as the list's bar)
    int lTip, lBase, rTip, rBase;  // the arrows' x (tip, base)
    SbBox left, right;             // the arrows' click boxes: tip - slack .. the bar, the bar .. tip + slack
    SbBox row;                     // the whole row (hover): left.x0 .. right.x1
} SbLayout;

enum { SB_HIT_NONE, SB_HIT_LEFT, SB_HIT_BAR, SB_HIT_RIGHT };

static void SbLayoutMake(SbLayout *L)
{
    L->textX = SB_ROW_CX;
    L->textY = SB_ROW_Y;
    L->bar.x0 = SB_ROW_CX - SB_BAR_HALF;
    L->bar.x1 = SB_ROW_CX + SB_BAR_HALF;
    L->bar.y0 = SB_ROW_Y;
    L->bar.y1 = SB_ROW_Y + SB_ROW_H;
    L->lBase = L->bar.x0 - SB_ARW_GAP;
    L->lTip = L->lBase - SB_ARW_W;
    L->rBase = L->bar.x1 + SB_ARW_GAP;
    L->rTip = L->rBase + SB_ARW_W;
    L->left.x0 = L->lTip - SB_ARW_SLACK;
    L->left.x1 = L->bar.x0;
    L->right.x0 = L->bar.x1;
    L->right.x1 = L->rTip + SB_ARW_SLACK;
    L->left.y0 = L->right.y0 = L->row.y0 = L->bar.y0;
    L->left.y1 = L->right.y1 = L->row.y1 = L->bar.y1;
    L->row.x0 = L->left.x0;
    L->row.x1 = L->right.x1;
}

// A box as the float rect 405760 (InputMouseInRectFloat) takes: x, y, w, h in canvas units.
static void SbBoxRect(const SbBox *b, float r[4])
{
    r[0] = (float)b->x0;
    r[1] = (float)b->y0;
    r[2] = (float)(b->x1 - b->x0);
    r[3] = (float)(b->y1 - b->y0);
}

static int SbInBox(const SbBox *b, float x, float y) { return x >= b->x0 && x < b->x1 && y >= b->y0 && y < b->y1; }

// The part of the row under a point (the native test's model of the three 405760 tests the game does).
static int SbHitAt(const SbLayout *L, float x, float y)
{
    if (SbInBox(&L->left, x, y)) return SB_HIT_LEFT;
    if (SbInBox(&L->bar, x, y)) return SB_HIT_BAR;
    if (SbInBox(&L->right, x, y)) return SB_HIT_RIGHT;
    return SB_HIT_NONE;
}

// The game's three 405760 answers -> the part (left first, as SbHitAt: the boxes do not overlap).
static int SbHitOf(int inLeft, int inBar, int inRight)
{
    return inLeft ? SB_HIT_LEFT : inBar ? SB_HIT_BAR : inRight ? SB_HIT_RIGHT : SB_HIT_NONE;
}

// A click on the selected row: -1 = the previous bank, +1 = the next, 0 = nothing. A click on an arrow's place while that
// arrow is not drawn (bank 1 / the last bank) still asks for that step: the flip buzzes, as Q / E do there.
static int SbClickStep(int hit) { return hit == SB_HIT_LEFT ? -1 : hit == SB_HIT_BAR || hit == SB_HIT_RIGHT ? +1 : 0; }

// The arrow towards dir (-1 / +1) is drawn: a bank is there.
static int SbArrowShown(int bank, int bankMax, int dir) { return dir < 0 ? bank > 1 : bank < bankMax && bank < 999; }

// The label for 556730: "\hBank N" (+ "?" while the list shows another bank's catalog). Returns its length, -1 = no room.
static int SbLabel(char *out, int n, int bank, int stale)
{
    int k;
    if (!out || n < 1) return -1;
    k = _snprintf_s(out, (size_t)n, _TRUNCATE, "\\hBank %d%s", bank, stale ? "?" : "");  // never the CRT's abort
    return k > 0 ? k : -1;
}

// ================================================================ v3.48 SB348 (USER_FEEDBACK_v347.md section B)
// On the highlighted Bank row: Left / Right (keys, D-pad, the stick) = 1 bank, Q / E and the pad's shoulders (round 2: not
// the triggers) = 10 banks (clamped to the first / last bank), both with the accelerating hold repeat; holding the left mouse button on
// a blue arrow repeats the same way. On a slot of the list Q / E and L1 / R1 stay 1 bank (v3.47). The per-step log line is
// verbose; one normal line per settled change (SbSettle).
#define SB_BIG 10  // the batch step

// ---------------------------------------------------------------- the hold repeat (v3.8 .. v3.47 sb_rep, made pure)
// Frames are list input frames (the menus run at 30 a second). First frame of a hold: a step at once; then after
// `delay` frames one step every i1 frames, from `mid` frames every i2, from `fast` frames every i3. A new press while
// nothing is held (a tap shorter than a frame) steps once. A change of the held value (direction or size) = a new hold.
typedef struct { unsigned delay, mid, fast, i1, i2, i3; } SbRepCfg;
static const SbRepCfg kSbRep = { 15, 45, 90, 4, 2, 1 };  // the table's sb_rep_* (0.5 s, then 7.5 / 15 / 30 steps a second)
typedef struct { int dir; unsigned hold, next; } SbRep;

// Returns the step this frame (0 = none); *quiet = 1 on a repeat step (no buzzer at the first / last bank).
static int SbRepStep(SbRep *r, const SbRepCfg *c, int held, int press, int *quiet)
{
    unsigned n, iv;
    *quiet = 0;
    if (!held) {
        r->dir = 0;
        r->hold = 0;
        return press;
    }
    if (held != r->dir) {
        r->dir = held;
        r->hold = 0;
        r->next = c->delay;
        return held;
    }
    n = ++r->hold;
    if (n < r->next) return 0;
    iv = c->i1;
    if (n >= c->mid) {
        iv = c->i2;
        if (n >= c->fast) iv = c->i3;
    }
    if (!iv) iv = 1;  // an interval 0 would never step again
    r->next = n + iv;
    *quiet = 1;
    return held;
}
static void SbRepReset(SbRep *r) { r->dir = 0, r->hold = 0, r->next = 0; }

// ---------------------------------------------------------------- the step: 1 or 10, clamped
// h1 / p1: held / newly pressed single-step direction (-1, 0, +1); h10 / p10 the batch ones. The batch wins when both.
static void SbPick(int h1, int p1, int h10, int p10, int *held, int *press)
{
    *held = h10 ? h10 * SB_BIG : h1;
    *press = p10 ? p10 * SB_BIG : h10 ? 0 : p1;
}
// The bank a step lands on: clamped to 1 .. min(bankMax, 999) (a jump of -10 from bank 4 lands on bank 1). The same
// bank = no step (the buzzer, as at the end today).
static int SbTarget(int bank, int step, int bankMax)
{
    const int hi = bankMax < 999 ? bankMax : 999;
    int n = bank + step;
    if (n < 1) n = 1;
    if (n > hi) n = hi;
    if (bank < 1 || bank > hi) n = bank;  // a bad current bank: no step (sb_flip's guards)
    return n;
}

// ---------------------------------------------------------------- the pad's shoulders (round 2, the user 2026-11-25)
// The batch step on the pad is the shoulders only: DirectInput buttons 4 / 5 ([InputDI + 0x180 + n], 1 = down), which GOG's
// dinput8.dll (goginput.ini onelayout = TRUE) gives every family as LB / RB, L1 / R1, L / R. The triggers (buttons 6 / 7
// there) are not read on the Bank row ("I mistakenly said L1/L2 for PlayStation when I meant L1/R1").

// ---------------------------------------------------------------- the mouse hold on a blue arrow
// click: a new left press on the selected row this frame (taken from the menu); down: the left button is down; hit: the
// part of the row under the mouse (SB_HIT_*). A press on an arrow starts a hold; it ends when the button is released or
// the mouse leaves that arrow's box, and does not come back without a new press. Returns the held direction for
// SbRepStep (-1 left arrow, +1 right arrow, 0 none): one step on the press, then the repeat. The bar is a click only.
typedef struct { int part; } SbMh;
static int SbMhFrame(SbMh *m, int click, int down, int hit)
{
    if (click) m->part = hit == SB_HIT_LEFT || hit == SB_HIT_RIGHT ? hit : SB_HIT_NONE;
    else if (!down || hit != m->part) m->part = SB_HIT_NONE;
    return m->part == SB_HIT_LEFT ? -1 : m->part == SB_HIT_RIGHT ? +1 : 0;
}

// ---------------------------------------------------------------- the log: one line per settled change
enum { SB_SRC_KEYS, SB_SRC_PAD, SB_SRC_MOUSE, SB_SRC_N };
#define SB_SETTLE_MS 1000  // a change has settled after this long without a step
typedef struct { int open, from, steps, by10, src[SB_SRC_N]; unsigned last; } SbSettle;
static void SbSettleStep(SbSettle *s, int was, int src, int big, unsigned now)
{
    int i;
    if (!s->open) {
        s->open = 1;
        s->from = was;
        s->steps = s->by10 = 0;
        for (i = 0; i < SB_SRC_N; ++i) s->src[i] = 0;
    }
    ++s->steps;
    s->by10 += big != 0;
    if (src >= 0 && src < SB_SRC_N) ++s->src[src];
    s->last = now;
}
// 1 = write the line now (force: the list opened again, a save / delete, Options): the change is closed by the caller.
static int SbSettleDue(const SbSettle *s, unsigned now, int force) { return s->open && (force || now - s->last >= SB_SETTLE_MS); }
// "bank 3 -> 27 in the load list (5 steps: keys 2, pad 3, mouse 0; by 10: 2)"; to = the bank the list shows now; list =
// "save" / "load" / other. Returns the length, -1 = no room.
static int SbSettleLine(const SbSettle *s, int to, const char *list, char *out, int n)
{
    int k;
    if (!out || n < 1) return -1;
    k = _snprintf_s(out, (size_t)n, _TRUNCATE, "bank %d -> %d in the %s list (%d step%s: keys %d, pad %d, mouse %d; by 10: %d)",
                    s->from, to, list ? list : "?", s->steps, s->steps == 1 ? "" : "s", s->src[SB_SRC_KEYS],
                    s->src[SB_SRC_PAD], s->src[SB_SRC_MOUSE], s->by10);
    return k > 0 ? k : -1;
}

#endif
