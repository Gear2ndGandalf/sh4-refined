// v3.54 OL34: native test of src\layout_sim.h (test\build_layout_test.cmd), the Options layout of rounds R3 + R4.
// v3.55 OL56: every check runs for both layouts: StockMoves=1 (R5: every stock row on its plan page) and StockMoves=0
// (v3.54's interim layout), plus an exact check of each one.
// 1. The page tree (PLAN section 6): six top pages in the cycle (LP_N 6: not 4 or 7), the ids of the plan's table, every
//    hidden page's parent a page and its slot (id mod 3) not its parent's, every hidden page reachable from a top page
//    through exactly one folder (or combo) row of its parent, at most three levels (Controls > Camera > Steering), the
//    free ids 14 / 17 / 20 empty; no row opens a top page (the Sound page, v3.52's LP_SND, is top page 3: never hidden).
// 2. Back / Exit: every hidden page ends with Back (its line "Back to <parent>."), every top page with Exit, no other row
//    of either kind; at most 11 rows a page (PLAN section 4), 11 only on the pages the fallbacks name (StockMoves=0), at
//    most 10 with StockMoves=1 (PLAN section 5); the last row ends above the description (y 360).
// 3. The row placement against the plan (layout_data.py through ol_words.h's OL_PLAN_*): every plan row but Back / Exit
//    placed exactly once; every row's key is a plan row; a row on its plan page in the plan's order; a row on another
//    page only as a listed interim place (StockMoves=0 only), each listed place used; a folder row where the plan has a
//    folder, a value row where it has a value; each source used once; the titles the plan's (one listed rename with
//    StockMoves=0), "Parent > Page" within 28 characters; the new texts fit the MSG_V54 + MSG_V55 pool (192).
// 4. v3.55 OL56, exact: with StockMoves=1 every live page IS its plan page (the same rows in the same order, the same kinds,
//    each folder opening the plan's page, Back / Exit last); with StockMoves=0 every page is v3.54's interim page (the
//    table below, from staging/v354_layout/PLAN.md "The pages").
// Usage: out\layout_sim_test.exe
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/layout_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { const char *id, *title, *parent; } PlanPage;
typedef struct { const char *page, *name; char kind; const char *opens; } PlanRow;
#define PP_X(i, t, p) { i, t, p },
#define PR_X(p, n, k, o) { p, n, k, o },
#define PT_X(i) i,
static const PlanPage kPlanPages[] = { OL_PLAN_PAGES(PP_X) };
static const PlanRow kPlanRows[] = { OL_PLAN_ROWS(PR_X) };
static const char *const kPlanTop[] = { OL_PLAN_TOP(PT_X) };
#define NPP ((int)(sizeof kPlanPages / sizeof kPlanPages[0]))
#define NPR ((int)(sizeof kPlanRows / sizeof kPlanRows[0]))
#define NPT ((int)(sizeof kPlanTop / sizeof kPlanTop[0]))
#define OL_POOL 192  // v3.55 OL56: layout.inc's texts: MSG_V54 + 0 .. 127 and MSG_V55 + 0 .. 63 (one pool)

static int PlanPageIx(const char *id)
{
    int i;
    for (i = 0; id && i < NPP; ++i)
        if (!strcmp(kPlanPages[i].id, id)) return i;
    return -1;
}
static int PlanRowIx(const char *page, const char *name)
{
    int i;
    for (i = 0; page && name && i < NPR; ++i)
        if (!strcmp(kPlanRows[i].page, page) && !strcmp(kPlanRows[i].name, name)) return i;
    return -1;
}
static int LiveOfPlan(const char *id)  // the live page that stands for plan page id
{
    int lp;
    for (lp = 0; lp < LP_MAX; ++lp)
        if (kOlPage[lp].plan && id && !strcmp(kOlPage[lp].plan, id)) return lp;
    return -1;
}
static const char *Leaf(const char *t)
{
    const char *s = strstr(t, " > ");
    return s ? s + 3 : t;
}
static int Renamed(int moves, const char *pp, const char *pn, const char *now)
{
    int i;
    if (moves) return 0;  // v3.55 OL56: the renames are the fallbacks' (StockMoves=0)
    for (i = 0; i < OL_RENAME_N; ++i)
        if (!strcmp(kOlRenames[i].pp, pp) && !strcmp(kOlRenames[i].pn, pn) && !strcmp(kOlRenames[i].now, now)) return 1;
    return 0;
}
#define ROW(lp, r) OlRowNth(kOlPage[lp].rows, moves, r)

static void TestTree(int moves)
{
    int lp, top, i;
    CHECK(LP_N == 6 && LP_N % 3 != 1 && LP_MAX == 22, "the cycle: LP_N %d, LP_MAX %d", LP_N, LP_MAX);
    CHECK(NPT == LP_N, "the plan's cycle has %d pages", NPT);
    for (top = 0; top < LP_N && top < NPT; ++top) {
        CHECK(kOlPage[top].parent == -1 && kOlPage[top].plan && !strcmp(kOlPage[top].plan, kPlanTop[top]),
              "top page %d is the plan's %s (%s)", top, kPlanTop[top], kOlPage[top].plan ? kOlPage[top].plan : "-");
    }
    // PLAN section 6's id table
    {
        static const struct { int id; const char *plan; int parent; } kIds[] = {
            { 6, "subtitles", 2 }, { 7, "pause", 2 }, { 8, "grain", 4 }, { 9, "shadows", 4 }, { 10, "player", 5 }, { 11, "camera", 1 },
            { 12, "room302", 1 }, { 13, "combat", 5 }, { 14, "dof", 4 } /* v3.58 PV358 */, { 15, "enemies", 5 }, { 16, "items", 5 }, { 18, "powers", 5 }, { 19, "rando", 5 },
            { 21, "steering", 11 } };
        for (i = 0; i < (int)(sizeof kIds / sizeof kIds[0]); ++i)
            CHECK(kOlPage[kIds[i].id].plan && !strcmp(kOlPage[kIds[i].id].plan, kIds[i].plan) && kOlPage[kIds[i].id].parent == kIds[i].parent,
                  "page %d is %s under %d (PLAN section 6)", kIds[i].id, kIds[i].plan, kIds[i].parent);
        CHECK(kOlPage[17].parent == -2 && kOlPage[20].parent == -2 && !kOlPage[17].rows && !kOlPage[20].rows,
              "ids 17, 20 stay free");  // v3.58 PV358: 14 is Effects > Depth of Field
    }
    for (lp = LP_N; lp < LP_MAX; ++lp) {
        const int par = kOlPage[lp].parent;
        int opens = 0, depth = 0, p = lp, r;
        const OlRowDef *d;
        if (par == -2) continue;
        CHECK(par >= 0 && par < LP_MAX && kOlPage[par].parent != -2, "page %d: its parent %d is a page", lp, par);
        if (par < 0 || par >= LP_MAX) continue;
        CHECK(lp % 3 != par % 3, "page %d (slot %d): its parent %d is in slot %d", lp, lp % 3, par, par % 3);
        for (r = 0; (d = ROW(par, r)) != 0; ++r)
            if (OlOpensKind(d->kind) && d->arg == lp) ++opens;
        CHECK(opens == 1, "moves %d: page %d: %d row(s) of page %d open it", moves, lp, opens, par);
        while (p >= LP_N && depth < 8) p = kOlPage[p].parent, ++depth;
        CHECK(p >= 0 && p < LP_N && depth <= 3 && OlTopOf(lp) == p, "page %d reaches top page %d in %d level(s)", lp, p, depth);
        CHECK(kPlanPages[PlanPageIx(kOlPage[lp].plan)].parent && LiveOfPlan(kPlanPages[PlanPageIx(kOlPage[lp].plan)].parent) == par,
              "page %d's parent %d is the plan's (%s)", lp, par, kPlanPages[PlanPageIx(kOlPage[lp].plan)].parent);
    }
    CHECK(kOlPage[LP_STEER].parent == LP_CAMERA && kOlPage[LP_CAMERA].parent == LP_CONTROLS && OlTopOf(LP_STEER) == LP_CONTROLS,
          "the third level: Controls > Camera > Steering");
    // no row opens a top page (Sound is top page 3: no request names it as a hidden page)
    for (lp = 0; lp < LP_MAX; ++lp) {
        int r;
        const OlRowDef *d;
        if (!kOlPage[lp].rows) continue;
        for (r = 0; (d = ROW(lp, r)) != 0; ++r)
            if (OlOpensKind(d->kind))
                CHECK(d->arg >= LP_N && d->arg < LP_MAX && kOlPage[d->arg].parent == lp,
                      "moves %d: page %d row %d opens page %d (a hidden child of this page)", moves, lp, r, d->arg);
    }
    // every plan page has its live page
    for (i = 0; i < NPP; ++i) CHECK(LiveOfPlan(kPlanPages[i].id) >= 0, "plan page %s is built", kPlanPages[i].id);
}

static void TestRows(int moves)
{
    int lp;
    for (lp = 0; lp < LP_MAX; ++lp) {
        const OlRowDef *last, *d;
        int n, r, backs = 0, exits = 0;
        if (!kOlPage[lp].rows) continue;
        n = OlRowCount(kOlPage[lp].rows, moves);
        CHECK(n >= 2 && n <= OL_MAX_ROWS && n < OL_ROWS, "moves %d: page %d (%s): %d rows", moves, lp, kOlPage[lp].title, n);
        if (moves)  // v3.55 OL56: PLAN section 5's counts (10 rows at most: pitch 26)  // v3.58 PV358: Effects 11 (the session's decision)
            CHECK(n <= 10 || (lp == LP_EFFECTS && n == OL_MAX_ROWS), "moves 1: page %d (%s): %d rows (the plan's ceiling is 10; Effects 11)", lp,
                  kOlPage[lp].title, n);
        else
            CHECK(n <= 10 || lp == LP_DISPLAY || lp == LP_INTERFACE || lp == LP_EFFECTS,
                  "moves 0: page %d: 11 rows only where PLAN section 4's fallbacks put them (Display, Interface, Effects)", lp);
        CHECK(OlLastRowEnd(n) <= 360, "page %d: the last row ends at y %d (the description starts at 360)", lp, OlLastRowEnd(n));
        for (r = 0; r < n; ++r) {
            backs += ROW(lp, r)->kind == OLK_BACK;
            exits += ROW(lp, r)->kind == OLK_EXIT;
        }
        last = ROW(lp, n - 1);
        if (lp < LP_N) {
            CHECK(last->kind == OLK_EXIT && exits == 1 && !backs, "moves %d: top page %d ends with its one Exit", moves, lp);
        } else {
            char want[64];
            CHECK(last->kind == OLK_BACK && backs == 1 && !exits, "moves %d: page %d ends with its one Back", moves, lp);
            sprintf(want, "Back to %s.", Leaf(OlTitle(kOlPage[lp].parent, moves)));
            CHECK(last->desc && !strcmp(last->desc, want) && last->label && !strcmp(last->label, "Back"),
                  "page %d: its Back says \"%s\" (has \"%s\")", lp, want, last->desc ? last->desc : "-");
        }
        for (r = 0; r < n; ++r) {
            d = ROW(lp, r);
            if (d->kind == OLK_FOLDER || d->kind == OLK_CAM) CHECK(d->label && d->desc ? 1 : d->label && d->kind == OLK_FOLDER && d->arg == LP_R302 && !moves,
                                                                   "moves %d: page %d row %d: a new row has its words", moves, lp, r);
            if (d->kind == OLK_VALUE || d->kind == OLK_SCREEN || d->kind == OLK_COMBO || d->kind == OLK_CAMSW)
                CHECK(d->src > OLS_NONE && d->src < OLS_N, "page %d row %d: a copied row has a source", lp, r);
            if (d->kind == OLK_CAM) CHECK(d->arg > OL_CAM_ENABLE && d->arg < OL_CAM_N, "page %d row %d: a camera setting", lp, r);
            CHECK(d->mode <= OLM_STOCK, "page %d row %d: a mode", lp, r);
        }
    }
    {   // each source once
        int used[OLS_N] = { 0 }, s;
        for (lp = 0; lp < LP_MAX; ++lp) {
            int r;
            const OlRowDef *d;
            if (!kOlPage[lp].rows) continue;
            for (r = 0; (d = ROW(lp, r)) != 0; ++r) if (d->src) ++used[d->src];
        }
        for (s = 1; s < OLS_N; ++s) CHECK(used[s] == 1, "moves %d: source %d is placed %d time(s)", moves, s, used[s]);
    }
    {   // each camera setting once (with the switch: the pause page's twelve rows)
        int used[OL_CAM_N] = { 0 }, c;
        for (lp = 0; lp < LP_MAX; ++lp) {
            int r;
            const OlRowDef *d;
            if (!kOlPage[lp].rows) continue;
            for (r = 0; (d = ROW(lp, r)) != 0; ++r)
                if (d->kind == OLK_CAM || d->kind == OLK_CAMSW) ++used[d->arg];
        }
        for (c = 0; c < OL_CAM_N; ++c) CHECK(used[c] == 1, "camera row %d is placed %d time(s)", c, used[c]);
    }
}

static void TestPlacement(int moves)
{
    int placed[512] = { 0 }, interimUsed[64] = { 0 }, lp, i;
    CHECK(NPR < 512 && OL_INTERIM_N < 64, "the plan has %d rows", NPR);
    for (lp = 0; lp < LP_MAX; ++lp) {
        int r, lastOwn = -1;
        const OlRowDef *d;
        if (!kOlPage[lp].rows) continue;
        for (r = 0; (d = ROW(lp, r)) != 0; ++r) {
            int k;
            for (k = 0; k < 2; ++k) {
                const char *pp = k ? d->pp2 : d->pp, *pn = k ? d->pn2 : d->pn;
                int ix, j, listed = -1;
                if (!pp) continue;
                ix = PlanRowIx(pp, pn);
                CHECK(ix >= 0, "page %d row %d: \"%s / %s\" is a plan row", lp, r, pp, pn);
                if (ix < 0) continue;
                ++placed[ix];
                if (!strcmp(pp, kOlPage[lp].plan)) {  // on its own page: in the plan's order
                    if (!k) {
                        CHECK(ix > lastOwn, "moves %d: page %d row %d: %s comes in the plan's order", moves, lp, r, pn);
                        lastOwn = ix;
                    }
                } else {
                    for (j = 0; !moves && j < OL_INTERIM_N; ++j)  // v3.55 OL56: no interim place with StockMoves=1
                        if (!strcmp(kOlInterim[j].pp, pp) && !strcmp(kOlInterim[j].pn, pn) && !strcmp(kOlInterim[j].now, kOlPage[lp].plan)) listed = j;
                    CHECK(listed >= 0, "moves %d: page %d row %d: %s / %s sits on %s: not a listed interim place", moves, lp, r, pp, pn,
                          kOlPage[lp].plan);
                    if (listed >= 0) ++interimUsed[listed];
                }
                if (!k) {  // the row's kind as the plan's (a folder where the plan has one; the combo stands for a folder)
                    const char pk = kPlanRows[ix].kind;
                    if (d->kind == OLK_FOLDER) CHECK(pk == 'f', "page %d row %d (%s): a folder where the plan has one", lp, r, pn);
                    if (d->kind == OLK_SCREEN) CHECK(pk == 's', "page %d row %d (%s): a sub-screen row", lp, r, pn);
                    if (d->kind == OLK_COMBO) CHECK(!moves && (pk == 'f' || pk == 'c'), "moves %d: page %d row %d (%s): the combo for a folder",
                                                    moves, lp, r, pn);
                    if (d->kind == OLK_VALUE || d->kind == OLK_CAM || d->kind == OLK_CAMSW)
                        CHECK(pk == 'v' || pk == 'b' || pk == 'a' || pk == 'i', "page %d row %d (%s): a value row where the plan has one (%c)", lp, r, pn, pk);
                    if (d->kind == OLK_FOLDER || d->kind == OLK_CAM)  // a new row's own label: the plan's word (or a listed rename)
                        CHECK(d->label && (!strcmp(d->label, pn) || Renamed(moves, pp, pn, d->label)), "moves %d: page %d row %d: label \"%s\" for %s",
                              moves, lp, r, d->label ? d->label : "-", pn);
                    if ((d->kind == OLK_VALUE) && d->label) CHECK(!strcmp(d->label, pn), "page %d row %d: its new label is the plan's %s", lp, r, pn);
                }
            }
        }
    }
    for (i = 0; i < NPR; ++i) CHECK(placed[i] == 1, "moves %d: plan row %s / %s placed %d time(s)", moves, kPlanRows[i].page, kPlanRows[i].name, placed[i]);
    for (i = 0; i < OL_INTERIM_N; ++i)
        CHECK(interimUsed[i] == (moves ? 0 : 1), "moves %d: interim place %s / %s on %s used %d time(s)", moves, kOlInterim[i].pp, kOlInterim[i].pn,
              kOlInterim[i].now, interimUsed[i]);
}

static int TestTitles(int moves)
{
    int lp, texts = 0, i;
    const char *seen[220];
    for (lp = 0; lp < LP_MAX; ++lp) {
        const int ix = PlanPageIx(kOlPage[lp].plan);
        const char *title = OlTitle(lp, moves);
        char crumb[96];
        int r;
        const OlRowDef *d;
        if (!kOlPage[lp].rows) continue;
        CHECK(ix >= 0, "page %d: plan page %s", lp, kOlPage[lp].plan);
        if (ix < 0) continue;
        CHECK(!strcmp(Leaf(kPlanPages[ix].title), title) || Renamed(moves, kOlPage[lp].plan, "", title),
              "moves %d: page %d: title \"%s\" (the plan: \"%s\")", moves, lp, title, kPlanPages[ix].title);
        if (lp >= LP_N) {
            sprintf(crumb, "%s > %s", Leaf(OlTitle(kOlPage[lp].parent, moves)), title);
            CHECK(strlen(crumb) <= 28, "page %d: \"%s\" within 28", lp, crumb);
        }
        // the texts layout.inc hands out (titles of pages 3+, new rows' labels and lines, the camera's value lines): count them
        if (lp >= 3) seen[texts++] = title;
        for (r = 0; (d = ROW(lp, r)) != 0; ++r) {
            const char *s2[2];
            int k;
            s2[0] = d->label, s2[1] = d->desc;
            for (k = 0; k < 2; ++k) {
                int dup = 0;
                if (!s2[k]) continue;
                for (i = 0; i < texts; ++i) dup |= !strcmp(seen[i], s2[k]);
                if (!dup && texts < 210) seen[texts++] = s2[k];
            }
        }
    }
    CHECK(texts + 4 <= OL_POOL, "moves %d: %d texts + the camera's 4 value lines fit layout.inc's pool of %d", moves, texts, OL_POOL);
    printf("layout (StockMoves=%d): %d texts of the %d in MSG_V54 + MSG_V55\n", moves, texts + 4, OL_POOL);
    return texts + 4;
}

// ---------------------------------------------------------------- v3.55 OL56: exact
// StockMoves=1: each live page is its plan page, row for row
static void TestExactPlan(void)
{
    const int moves = 1;
    int p;
    for (p = 0; p < NPP; ++p) {
        const int lp = LiveOfPlan(kPlanPages[p].id);
        int r = 0, i, n;
        if (lp < 0) continue;
        n = OlRowCount(kOlPage[lp].rows, moves);
        for (i = 0; i < NPR; ++i) {
            const OlRowDef *d;
            const char pk = kPlanRows[i].kind;
            if (strcmp(kPlanRows[i].page, kPlanPages[p].id)) continue;
            d = ROW(lp, r);
            CHECK(d && d->pp && !strcmp(d->pp, kPlanRows[i].page) && !strcmp(d->pn, kPlanRows[i].name) && !d->pp2,
                  "exact: %s row %d is the plan's %s (has %s / %s)", kPlanPages[p].id, r, kPlanRows[i].name, d && d->pp ? d->pp : "-",
                  d && d->pn ? d->pn : "-");
            if (d) {
                const int kindOk = pk == 'f' ? d->kind == OLK_FOLDER : pk == 's' ? d->kind == OLK_SCREEN :
                                   (d->kind == OLK_VALUE || d->kind == OLK_CAM || d->kind == OLK_CAMSW);
                CHECK(kindOk, "exact: %s row %d (%s): kind %d for the plan's '%c'", kPlanPages[p].id, r, kPlanRows[i].name, d->kind, pk);
                if (pk == 'f')
                    CHECK(d->arg < LP_MAX && kOlPage[d->arg].plan && !strcmp(kOlPage[d->arg].plan, kPlanRows[i].opens),
                          "exact: %s row %d (%s) opens the plan's %s", kPlanPages[p].id, r, kPlanRows[i].name, kPlanRows[i].opens);
                if (pk == 'b') CHECK(d->src == OLS_BGM || d->src == OLS_SE, "exact: %s row %d (%s): a stock volume bar", kPlanPages[p].id, r, kPlanRows[i].name);
            }
            ++r;
        }
        CHECK(r == n - 1, "exact: %s has %d rows + %s (live %d)", kPlanPages[p].id, r, lp < LP_N ? "Exit" : "Back", n);
        CHECK(ROW(lp, n - 1) && ROW(lp, n - 1)->kind == (lp < LP_N ? OLK_EXIT : OLK_BACK), "exact: %s ends with %s", kPlanPages[p].id,
              lp < LP_N ? "Exit" : "Back");
    }
}
// StockMoves=0: v3.54's interim pages (staging/v354_layout/PLAN.md "The pages"), each row by its plan key ("page/row";
// "+" = Exit / Back)
typedef struct { int lp; const char *rows[12]; } Interim54;
static const Interim54 kV354[] = {
    { LP_DISPLAY, { "display/Resolution", "display/Aspect Ratio", "display/Display Mode", "display/Monitor", "display/Frame Rate",
                    "display/Brightness", "display/Gamma", "sound/Sound Mode", "sound/Music Volume", "sound/Effects Volume", "+" } },
    { LP_CONTROLS, { "controls/Buttons", "controls/Default Speed", "controls/Vibration", "controls/Analog Movement", "room302/Head Bob",
                     "room302/Turn or Strafe", "room302/Invert Look", "controls/Camera", "controls/Room 302", "+" } },  // v3.57 OLP1: Turn or Strafe (was Left / Right)
    { LP_CAMERA, { "camera/Field of View", "camera/Camera", "camera/Distance", "camera/Side", "camera/Height", "camera/Tilt Down",
                   "camera/Fixed Angles", "camera/Steering", "+" } },
    { LP_STEER, { "steering/Turn Speed", "steering/Tilt Speed", "steering/Invert Turn", "steering/Invert Tilt", "steering/Wall Zoom",
                  "steering/Wall Margin", "+" } },
    { LP_R302, { "room302/Field of View", "room302/Look Down", "room302/Look Up", "room302/Look Smoothing", "room302/Walk Speed", "+" } },
    { LP_INTERFACE, { "interface/Language", "interface/Subtitles", "interface/Health Gauge", "interface/Item Icons", "grain/Film Grain",
                      "shadows/Shadows", "gameplay/Blood Colour", "interface/Enemy Health", "interface/Curse Meter", "interface/Pause Menu", "+" } },
    { LP_SUBT, { "subtitles/Size", "subtitles/Colour", "subtitles/Edge", "subtitles/Box", "subtitles/Appear", "subtitles/Vanish", "+" } },
    { LP_MENUS, { "pause/Storage Chest", "pause/Save Game", "pause/Load Game", "pause/Quick Save", "pause/Quick Load", "pause/Eileen",
                  "pause/Save Banks", "interface/Menu Font", "interface/Start-Up", "+" } },  // v3.56 MN356: "pause/Eileen" (was Eileen Outfit)
    { LP_SOUND, { "sound/Stereo Width", "sound/Room Reverb", "sound/Night Mode", "sound/Background", "display/Renderer", "+" } },  // v3.56 SD356: + Background  // v3.58 PV358: + Renderer (was on Effects)
    { LP_EFFECTS, { "effects/Film Grain", "effects/Filter", "effects/Filter Strength", "effects/Cut-scene Filter", "effects/Bloom",
                    "effects/Colour Grade", "effects/Otherworld Lens", "effects/Depth of Field", "effects/Shadows", "effects/Preview", "+" } },  // v3.58 PV358
    { LP_DOF, { "dof/Depth of Field", "dof/Strength", "+" } },  // v3.58 PV358
    { LP_GRAIN, { "grain/Strength", "grain/Grain Type", "grain/Grain Colour", "+" } },
    { LP_SHADOWS, { "shadows/Edge", "shadows/Darkness", "+" } },
    { LP_GAMEPLAY, { "gameplay/Player", "gameplay/Combat", "gameplay/Items", "gameplay/Enemies", "gameplay/Powers", "gameplay/Randomizer", "+" } },
    { LP_PLAYER, { "player/Move Speed", "player/Regeneration", "player/Medallion", "player/Grab Escape", "player/No Clip", "+" } },
    { LP_COMBAT, { "combat/One-Hit Kill", "combat/Dark Pact", "combat/Quick Charge", "combat/Life Steal", "combat/Ammo Saver", "+" } },
    { LP_ITEMS, { "items/Item Shine", "items/Toolbelt", "items/Item Stacks", "items/Supply Drops", "+" } },
    { LP_ENEMIES, { "enemies/Extra Enemies", "enemies/Door Chase", "enemies/Pursuit", "enemies/Enemies Return", "enemies/Reveal Method", "+" } },  // v3.58 MN358: Reveal Method (was Fade In)
    { LP_POWERS, { "powers/Death Presence", "powers/After Shock", "powers/Echo Doubles", "powers/Echo Count", "powers/Ghost Teleport", "+" } },
    { LP_RANDO, { "rando/Items", "rando/Specials", "rando/Room 302 Items", "rando/Enemies", "rando/Spawn Site", "rando/Eileen Outfit",
                  "rando/Reroll", "+" } },  // v3.57 OLP1: Reroll (was Reroll Visits)  // v3.56 RI356: Specials (was Keep Specials); no Seed / New Seed rows; v3.56 RE356: Monsters -> Enemies, Ghosts -> Spawn Site
};
static void TestExactV354(void)
{
    const int moves = 0;
    int i, pages = 0, lp;
    for (i = 0; i < (int)(sizeof kV354 / sizeof kV354[0]); ++i) {
        const int p = kV354[i].lp;
        int r, n = OlRowCount(kOlPage[p].rows, moves);
        ++pages;
        for (r = 0; r < 12 && kV354[i].rows[r]; ++r) {
            const OlRowDef *d = ROW(p, r);
            char key[96];
            if (!strcmp(kV354[i].rows[r], "+")) {
                CHECK(d && (d->kind == OLK_EXIT || d->kind == OLK_BACK) && r == n - 1, "v3.54: page %d ends at row %d (has %d rows)", p, r, n);
                break;
            }
            sprintf(key, "%s/%s", d && d->pp ? d->pp : "-", d && d->pn ? d->pn : "-");
            CHECK(!strcmp(key, kV354[i].rows[r]), "v3.54: page %d row %d is %s (has %s)", p, r, kV354[i].rows[r], key);
        }
    }
    for (lp = 0, i = 0; lp < LP_MAX; ++lp) i += kOlPage[lp].rows != 0;
    CHECK(pages == i && pages == 20, "v3.54: all %d pages listed (%d)", i, pages);  // v3.58 PV358: 20 (+ Depth of Field)
    // the combo row and the fallback names
    CHECK(ROW(LP_INTERFACE, 1)->kind == OLK_COMBO && ROW(LP_INTERFACE, 1)->arg == LP_SUBT, "v3.54: Interface's Subtitles is the combo row");
    CHECK(!strcmp(OlTitle(LP_MENUS, 0), "Menus") && !strcmp(OlTitle(LP_MENUS, 1), "Pause Menu"), "the Pause Menu folder: Menus with StockMoves=0");
}

int main(void)
{
    int lp, moves, t[2];
    for (moves = 1; moves >= 0; --moves) {
        TestTree(moves);
        TestRows(moves);
        TestPlacement(moves);
        t[moves] = TestTitles(moves);
        for (lp = 0; lp < LP_MAX; ++lp)
            if (kOlPage[lp].rows)
                printf("  StockMoves=%d page %2d %-12s slot %d parent %2d: %2d rows\n", moves, lp, OlTitle(lp, moves), lp % 3, kOlPage[lp].parent,
                       OlRowCount(kOlPage[lp].rows, moves));
    }
    TestExactPlan();
    TestExactV354();
    // the counts of PLAN section 5 (StockMoves=1) and of v3.54 (StockMoves=0)
    {
        // v3.56 RI356: the Randomizer (page 19) 10 -> 8 rows (no Seed / New Seed rows) in both layouts; v3.56 SD356: Sound 7 -> 8 / 4 -> 5
        // v3.58 PV358: Effects 10 -> 11 (+ Filter Strength, Preview; Depth of Field > holds Depth of Field / Strength), page 14
        // Depth of Field 3; StockMoves=0: Effects stays 11 (Renderer to Sound: 5 -> 6)
        static const int kPlanN[LP_MAX] = { 9, 7, 10, 8, 11, 8, 8, 8, 5, 4, 6, 9, 9, 6, 3, 6, 5, 0, 6, 8, 0, 7 };
        static const int kV354N[LP_MAX] = { 11, 10, 11, 6, 11, 7, 7, 10, 4, 3, 6, 9, 6, 6, 3, 6, 5, 0, 6, 8, 0, 7 };
        for (lp = 0; lp < LP_MAX; ++lp) {
            CHECK(OlRowCount(kOlPage[lp].rows, 1) == kPlanN[lp], "StockMoves=1: page %d has %d rows (PLAN section 5: %d)", lp,
                  OlRowCount(kOlPage[lp].rows, 1), kPlanN[lp]);
            CHECK(OlRowCount(kOlPage[lp].rows, 0) == kV354N[lp], "StockMoves=0: page %d has %d rows (v3.54: %d)", lp,
                  OlRowCount(kOlPage[lp].rows, 0), kV354N[lp]);
        }
    }
    (void)t;
    printf("%s: %d checks, %d failed\n", g_fail ? "FAILED" : "OK", g_n, g_fail);
    return g_fail ? 1 : 0;
}
