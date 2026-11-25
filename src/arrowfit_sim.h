// v3.33 MENU333: where the right value arrow of the selected Options row goes (sh4windowmod.c ArrowShift, its mouse
// rect hkArrowShifted, the column move LongValW). No game memory: test\arrowfit_test.c (test\build_arrowfit_test.cmd).
//
// The stock row (0x5254F0): the value text left-aligned at x (canvas 364 at rest), the left arrow at x - 20, the right
// arrow (8 x 24, 0x523620) at x + 100. A value wider than 94 px ran under the right arrow (v3.32 QA: Echo Doubles
// "Shadow Cast"; the old rule shifted only rows it knew, by character count). Now every row is measured with the
// game's own font metrics (LabelW): the arrow goes AF_GAP px after the row's widest value, at most AF_MAXSHIFT (the arrow
// ends by canvas x 508 inside the 512 canvas); a value wider than AF_COLW moves the whole column left (LongValW,
// v3.19.0 C2b). The canvas is 512 wide at every Aspect Ratio (2D is mapped onto it, Menus43 or stretched), so canvas
// px decide at 4:3, 16:9 and the rest alike.
#ifndef ARROWFIT_SIM_H
#define ARROWFIT_SIM_H

#define AF_SLOT 100                                  // value x -> the right arrow's x (stock)
#define AF_GAP 6                                     // the text's end -> the right arrow
#define AF_MAXSHIFT 36                               // 464 + 36 + 8 = 508
#define AF_COLW (AF_SLOT - AF_GAP + AF_MAXSHIFT)     // 130: wider values move the column (LongMove's 130)

// The shift that puts the arrow AF_GAP px after a value w px wide; 0 when it already fits.
static int AfNeed(float w)
{
    const int n = (int)(w + 0.999f) + AF_GAP - AF_SLOT;
    return n > 0 ? n : 0;
}

// The row's shift: its widest value that fits without moving the column decides, so the arrow stays put while the
// value changes (the v1.7 rule for Display Mode / Monitor); at most AF_MAXSHIFT; never less than the old rule's.
static int AfRowShift(const float *w, int n, int old)
{
    int i, s = 0;
    for (i = 0; i < n; ++i)
        if (w[i] <= (float)AF_COLW) {
            const int k = AfNeed(w[i]);
            if (k > s) s = k;
        }
    if (s > AF_MAXSHIFT) s = AF_MAXSHIFT;
    return old > s ? old : s;
}

// Overlap test for the checks: the text (x .. x + w) reaches the right arrow at x + AF_SLOT + shift (2 px of air kept).
static int AfOverlaps(float w, int shift) { return w + 2.0f > (float)(AF_SLOT + shift); }

#endif
