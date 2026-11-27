// ================================================================ v3.56 MN356: every Options value has its own line
// USER_FEEDBACK_v355 issue 3: "Supply Drops description when Off reads, 'Kills sometimes leave an item.'" The house style
// (staging/options_layout/PLAN.md section 3) gives every value of every row its own line "Value: what it does."; v3.55
// still answered the row's line for some values (Off, mostly) because those rows' getters had no line of their own for
// them. The plan's data (layout_data.py) now carries a line for every named value (the numbers and ranges keep the row's
// line: BUILD_NOTES), generated into ol_words.h's OL_PLAN_VLINES. layout.inc's OlBuildPage wraps the description getter
// of every live row the plan gives value lines (VlWrapRow): the wrapper calls the row's own getter and, only when that
// answered exactly the row's line, answers the plan's line for the value shown (vlines_sim.h). Every other answer of a
// row's code (its own value lines, "Not available ...", Playable Eileen's lines, look.inc's grey reasons, which wrap
// outside this) stays as it was. No exe site, no ini key; the texts go through MN356's 16 ids MSG_V56 + 0 .. 15 (a ring:
// a row's value keeps its id while it is shown). Included by layout.inc (it needs OlRowDef and MsgText).
#include "vlines_sim.h"

typedef struct { const char *pp, *pn, *rowD, *label, *line; } VlPlan;
static const VlPlan kVlPlan[] = {
#define VL_P(pp, pn, d, l, vd) { pp, pn, d, l, vd },
    OL_PLAN_VLINES(VL_P)
#undef VL_P
};
#define VL_PLAN_N ((int)(sizeof kVlPlan / sizeof kVlPlan[0]))
#define VL_ROWS 100  // rows with value lines: 90 in the plan of v3.56 (test\vlines_test.c checks the count fits)
#define VL_VMAX 16   // plan values a row has at most (Colour 9, Field of View 14)
typedef char VlIdsFit[MSG_V56 + VL_RING <= MSG_V56 + 16 ? 1 : -1];  // MN356 owns V56 + 0 .. 15

typedef struct {
    const char *pp, *pn;
    DWORD desc, get, ids;  // the row's own description getter, value getter, value-id table
    int count, nv;         // the row's values; the plan's values with a line
    BYTE rowD[VL_TXT], rowDT[VL_TXT + 16];
    BYTE label[VL_VMAX][32];
    const char *line[VL_VMAX], *word[VL_VMAX];
    LONG answered;         // values answered with the plan's line (log)
    DWORD missed;          // values (bit v) logged as without a line
    int order;             // -1 not checked; 1 = the row's words follow the plan's order (a stock word by its place)
} VlRow;
static VlRow g_vl[VL_ROWS];
static int g_vlN, g_vlFull, g_vlLines;
static VlRing g_vlRing;
static int g_vlRingOk;
static BYTE g_vlTxt[VL_RING][VL_TXT];
static volatile LONG g_vlQ, g_vlQMiss, g_vlQFault;

static const BYTE *VlText(int id)  // MsgText: MSG_V56 + 0 .. 15
{
    const int s = id - MSG_V56;
    return g_vlTxt[(unsigned)s < VL_RING ? s : 0];
}

// a byte-mode string as text, for the log (Encode backwards; "?" for what Encode never writes)
static const char *VlWord(const BYTE *t, char *out, int n)
{
    int i = 0, k = 2;
    out[0] = 0;
    if (!t || t[0] != 0x00 || t[1] != 0x80) return "?";
    while (i + 1 < n && k < 40 && !(t[k] == 0xFF && t[k + 1] == 0xFF)) {
        out[i++] = t[k] == 0 ? ' ' : (char)(0x100 - t[k]);
        ++k;
    }
    out[i] = 0;
    return out;
}
// every value word of ours on the row (a Refined text id) stands in the plan's place for it: 1 (a stock word may then be
// taken by its place); 0 otherwise (also when the counts differ or no word of ours confirms the order)
static int VlOrder(const VlRow *w)
{
    int v, i, m[VL_VMAX];
    if (w->count != w->nv || w->count > VL_VMAX || !w->ids) return 0;
    for (v = 0; v < w->count; ++v) {
        const int lid = (int)((const DWORD *)(ULONG_PTR)w->ids)[v];
        m[v] = VL_M_STOCK;
        if (lid >= MSG_BASE && lid < MSG_END) {
            const BYTE *const lt = MsgText(lid);
            m[v] = VL_M_NONE;
            for (i = 0; i < w->nv; ++i)
                if (VlSame(lt, w->label[i], 32)) {
                    m[v] = i;
                    break;
                }
        }
    }
    return VlOrderOk(m, w->count, w->nv);
}

static int VlDesc(int k)
{
    VlRow *const w = &g_vl[k];
    int id0, v = -1, i, line = -1, rowLine = 0, r;
    const BYTE *lt = NULL;
    if ((unsigned)k >= (unsigned)g_vlN || !w->desc) return 0;
    id0 = ((int(__cdecl *)(void))(ULONG_PTR)w->desc)();
    r = id0;
    if (id0 < MSG_BASE || id0 >= MSG_END) return id0;  // a game text: never the plan's row line
    __try {
        const BYTE *const t = MsgText(id0);
        rowLine = VlSame(t, w->rowD, VL_TXT) || VlSame(t, w->rowDT, VL_TXT + 16);
        if (rowLine) {
            v = w->get ? ((int(__cdecl *)(void))(ULONG_PTR)w->get)() : -1;
            if (v >= 0 && v < w->count && w->ids) {
                const int lid = (int)((const DWORD *)(ULONG_PTR)w->ids)[v];
                // the value's word as drawn: ours through MsgText, a stock word (the Film Grain / Shadows switches' "On" /
                // "Off") through the game's lookup 0x50C440 (bank 0), as descfit.inc reads a line
                lt = lid >= MSG_BASE && lid < MSG_END ? MsgText(lid) : lid > 0 ? ((const BYTE *(__cdecl *)(int, int))0x50C440)(0, lid) : NULL;
                for (i = 0; lt && i < w->nv && line < 0; ++i)
                    if (VlSame(lt, w->label[i], 32)) line = i;
                if (line < 0 && (lid < MSG_BASE || lid >= MSG_END)) {  // a stock word the plan spells otherwise (Blood
                    if (w->order < 0) w->order = VlOrder(w);           // Colour's Red / Green / Purple): by its place when
                    if (w->order == 1 && v < w->nv) line = v;          // every word of ours on the row is in the plan's place
                }
            }
            line = VlPick(rowLine, v, w->count, line);
            if (line >= 0) {
                int fresh;
                const int s = VlRingSlot(&g_vlRing, k, line, &fresh);
                if (fresh) Encode(g_vlTxt[s], VL_TXT, w->line[line]);
                r = MSG_V56 + s;
                if (InterlockedIncrement(&w->answered) == 1 && LogQuota(&g_vlQ, 200))
                    LogV("options: value lines: %s / %s: \"%s\" (its row code answered the row's line)", w->pp, w->pn, w->line[line]);
            } else if (v >= 0 && v < 32 && !(w->missed & (1u << v))) {
                char word[32];
                w->missed |= 1u << v;
                if (LogQuota(&g_vlQMiss, 60))
                    LogV("options: value lines: %s / %s value %d \"%s\" has no line in the plan (a number or a range): the row's "
                         "line stays", w->pp, w->pn, v, VlWord(lt, word, sizeof word));
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (LogQuota(&g_vlQFault, 3)) LogAnomaly("options", "value lines: row %s / %s faulted: its own line", w->pp, w->pn);
        r = id0;
    }
    return r;
}

#define VL_T(n) static int __cdecl VlD##n(void) { return VlDesc(n); }
#define VL_T10(a) VL_T(a##0) VL_T(a##1) VL_T(a##2) VL_T(a##3) VL_T(a##4) VL_T(a##5) VL_T(a##6) VL_T(a##7) VL_T(a##8) VL_T(a##9)
#define VL_A10(a) VlD##a##0, VlD##a##1, VlD##a##2, VlD##a##3, VlD##a##4, VlD##a##5, VlD##a##6, VlD##a##7, VlD##a##8, VlD##a##9
VL_T(0) VL_T(1) VL_T(2) VL_T(3) VL_T(4) VL_T(5) VL_T(6) VL_T(7) VL_T(8) VL_T(9)
VL_T10(1) VL_T10(2) VL_T10(3) VL_T10(4) VL_T10(5) VL_T10(6) VL_T10(7) VL_T10(8) VL_T10(9)
static int(__cdecl *const kVlThunk[VL_ROWS])(void) = { VlD0, VlD1, VlD2, VlD3, VlD4, VlD5, VlD6, VlD7, VlD8, VlD9, VL_A10(1),
                                                     VL_A10(2), VL_A10(3), VL_A10(4), VL_A10(5), VL_A10(6), VL_A10(7),
                                                     VL_A10(8), VL_A10(9) };  // thunk n answers row n
#undef VL_T
#undef VL_T10
#undef VL_A10

static int VlIsThunk(DWORD f)
{
    int k;
    for (k = 0; k < VL_ROWS; ++k)
        if (f == (DWORD)(ULONG_PTR)kVlThunk[k]) return 1;
    return 0;
}

// layout.inc OlBuildPage: row record r (a value row or a camera row of the live page) gets the wrapper when the plan has
// value lines for it. A row built again (a second build) keeps its slot; a record that already carries a wrapper keeps
// the getter it wraps.
static void VlWrapRow(DWORD *r, int kind, const OlRowDef *d)
{
    VlRow *w = NULL;
    int i, k, n = 0;
    if (!g_vlRingOk) VlRingInit(&g_vlRing), g_vlRingOk = 1;
    if (!r || !d || !d->pp || !d->pn || (kind != OLK_VALUE && kind != OLK_CAM)) return;
    if (!r[6] || !r[1] || !r[3] || (int)r[4] <= 0 || r[1] == 0x51EB10) return;  // value-less (a getter -1) or no line at all
    for (i = 0; i < VL_PLAN_N; ++i)
        if (!strcmp(kVlPlan[i].pp, d->pp) && !strcmp(kVlPlan[i].pn, d->pn)) break;
    if (i == VL_PLAN_N) return;
    for (k = 0; k < g_vlN; ++k)
        if (!strcmp(g_vl[k].pp, d->pp) && !strcmp(g_vl[k].pn, d->pn)) break;
    if (k == g_vlN) {
        if (g_vlN >= VL_ROWS) {
            ++g_vlFull;
            return;
        }
        ++g_vlN;
    }
    w = &g_vl[k];
    if (!VlIsThunk(r[6])) w->desc = r[6];  // (a record wrapped before keeps the getter it wraps)
    w->pp = d->pp, w->pn = d->pn, w->get = r[1], w->ids = r[3], w->count = (int)r[4];
    Encode(w->rowD, sizeof w->rowD, kVlPlan[i].rowD);
    {
        char dt[VL_TXT + 16];
        sprintf_s(dt, sizeof dt, "%s Next room.", kVlPlan[i].rowD);  // the R1 tag (gen_ol_words.py TAG)
        Encode(w->rowDT, sizeof w->rowDT, dt);
    }
    for (; i < VL_PLAN_N && n < VL_VMAX; ++i) {
        if (strcmp(kVlPlan[i].pp, d->pp) || strcmp(kVlPlan[i].pn, d->pn)) continue;
        Encode(w->label[n], sizeof w->label[n], kVlPlan[i].label);
        w->word[n] = kVlPlan[i].label;
        w->line[n++] = kVlPlan[i].line;
    }
    if (w->nv == 0) g_vlLines += n;
    w->nv = n;
    w->order = -1;  // (checked at the first answer: the words may be written after the build)
    w->missed = 0;
    if (w->desc) r[6] = (DWORD)(ULONG_PTR)kVlThunk[k];
}

// layout.inc OlLayoutBuild, after the pages: one line a build
static void VlBuilt(void)
{
    static LONG s_q;
    if (!LogQuota(&s_q, 4)) return;
    Log("options: value lines: %d row(s) carry the plan's %d value lines (%d in the plan); a value whose row code answers the "
        "row's line shows its own line (text ids V56 + 0 .. %d)%s", g_vlN, g_vlLines, VL_PLAN_N, VL_RING - 1,
        g_vlFull ? " (OVER THE ROW LIMIT: some rows keep the row's line)" : "");
    if (g_vlFull) LogAnomaly("options", "value lines: %d row(s) over the %d the wrapper holds keep the row's line", g_vlFull, VL_ROWS);
}
