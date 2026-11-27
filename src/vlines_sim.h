// v3.56 MN356 (USER_FEEDBACK_v355 issue 3: "Supply Drops description when Off reads 'Kills sometimes leave an item.'"):
// the pure part of src\vlines.inc, tested by test\vlines_test.c. Every Options value has its own "Value: what it does."
// line (house style). A row whose own code answers the row's line for some value (its Off, mostly) gets the plan's line
// for that value instead: the row's description getter is wrapped; the wrapper answers the plan's value line only when
//   - the row's own getter answered exactly the row's line (or the row's line + " Next room.": the R1 tag), compared as
//     the byte-mode game strings (Encode: 00 80, one byte a letter, FF FF), so every other line the row's code gives (a
//     value line of its own, "Not available ...", a Playable Eileen line) stays as it is;
//   - and the plan has a line for the value shown (its screen word, compared the same way).
// The lines are answered through a small ring of text ids (the 16 ids V56 + 0 .. 15 of MN356): a (row, value) keeps its
// slot while it is in the ring, so the id stays the same frame after frame; the 17th distinct pair takes the oldest slot.
#ifndef VLINES_SIM_H
#define VLINES_SIM_H

#define VL_RING 16   // text ids MSG_V56 + 0 .. 15
#define VL_TXT 64    // a slot's bytes (Encode: 2 + 52 letters + 2 = 56 at most)

// 1 = the two byte-mode strings are the same text (each ends at FF FF; at most max bytes looked at)
static int VlSame(const unsigned char *a, const unsigned char *b, int max)
{
    int i;
    if (!a || !b) return 0;
    for (i = 0; i + 1 < max; ++i) {
        if (a[i] != b[i]) return 0;
        if (i >= 2 && a[i] == 0xFF && a[i + 1] == 0xFF) return b[i + 1] == 0xFF;
    }
    return 0;  // no end within max: not a string we made
}

// the ring of answered lines: slot s holds row k's value v (k -1 = free)
typedef struct { int k[VL_RING], v[VL_RING], next; } VlRing;
static void VlRingInit(VlRing *r)
{
    int s;
    for (s = 0; s < VL_RING; ++s) r->k[s] = -1, r->v[s] = -1;
    r->next = 0;
}
// the slot of (k, v); *fresh = 1 when it was given now (the caller writes its text)
static int VlRingSlot(VlRing *r, int k, int v, int *fresh)
{
    int s;
    for (s = 0; s < VL_RING; ++s)
        if (r->k[s] == k && r->v[s] == v) {
            *fresh = 0;
            return s;
        }
    s = r->next;
    r->next = (r->next + 1) % VL_RING;
    r->k[s] = k, r->v[s] = v;
    *fresh = 1;
    return s;
}

// the decision: rowLine = the row's getter answered the row's line; v / count = the value shown; line = the plan's line
// index for that value (-1 none). Returns the line index to answer, or -1 = the row's own answer stays.
static int VlPick(int rowLine, int v, int count, int line)
{
    if (!rowLine || v < 0 || v >= count) return -1;
    return line >= 0 ? line : -1;
}

// The plan's order for a row with stock words: m[v] = the plan value row value v's word matched (VL_M_STOCK: a game
// text, not compared; VL_M_NONE: a word of ours the plan does not list). 1 = the counts agree, every word of ours sits in
// its own place and at least one does (so a stock word may be answered by its place: Blood Colour's first three).
#define VL_M_STOCK -2
#define VL_M_NONE -1
static int VlOrderOk(const int *m, int count, int nv)
{
    int v, ours = 0;
    if (count != nv || count <= 0) return 0;
    for (v = 0; v < count; ++v) {
        if (m[v] == VL_M_STOCK) continue;
        if (m[v] != v) return 0;
        ++ours;
    }
    return ours > 0;
}

#endif
