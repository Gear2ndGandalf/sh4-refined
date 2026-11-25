// v3.36 TP336: native test of src\pools_sim.h (test\build_pools_test.cmd): the task pool's size from the ini, the ten
// writes of its growth (values against the audit's N = 400 table), the range helper, the copy gate, the IK free-list
// walk (over a list built here, with a cycle and a stray link) and the per-room use.
#include <stdio.h>
#include <string.h>
#include "../src/pools_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestPoolN(void)
{
    CHECK(TpPoolN(0) == 300, "0 = the stock pool");
    CHECK(TpPoolN(-1) == 300, "negative = the stock pool");
    CHECK(TpPoolN(300) == 300, "300 = the stock pool");
    CHECK(TpPoolN(301) == 301, "301");
    CHECK(TpPoolN(400) == 400, "400");
    CHECK(TpPoolN(TP_DEFAULT_N) == 400, "the default grows the pool to 400");
    CHECK(TpPoolN(100000) == TP_MAX_N, "clamped to 448");
    CHECK(IkPoolN(0) == 160 && IkPoolN(160) == 160, "IK: 0 and 160 = stock");
    CHECK(IkPoolN(IK_DEFAULT_N) == 512, "IK default 512");
    CHECK(IkPoolN(99999) == IK_MAX_N, "IK clamped");
}

static void TestPlan(void)
{
    TpDword w[TP_NSITES];
    int i, n;
    TpPlan(400, w);  // the audit's table (REPORT.md section 2): E = 0x121EB20, size 0x15D40
    CHECK(TpEnd(400) == 0x121EB20u, "N 400: the end %08X", TpEnd(400));
    CHECK(TpPacketSize(400) == 0x15D40u, "N 400: the packet buffer %X bytes", TpPacketSize(400));
    CHECK(w[0].va == 0x553846 && w[0].stock == 0x01204860 && w[0].val == 0x121EB20, "InitAll end");
    CHECK(w[1].va == 0x55385F && w[1].stock == 0x12C && w[1].val == 400, "RunGroup count");
    CHECK(w[2].va == 0x553A40 && w[2].stock == 0x12C && w[2].val == 400, "FindByFunc count");
    CHECK(w[3].va == 0x553A72 && w[3].stock == 0x12C && w[3].val == 400, "Alloc count");
    CHECK(w[4].va == 0x553BC9 && w[4].stock == 0x01204860 && w[4].val == 0x121EB20, "Get base");
    CHECK(w[5].va == 0x553BCE && w[5].stock == 0x2F000 && w[5].val == 0x14D40, "Get limit %X", w[5].val);
    CHECK(w[6].va == 0x553C04 && w[6].stock == 0x01204860 && w[6].val == 0x121EB20, "Fix base");
    CHECK(w[7].va == 0x553C0A && w[7].stock == 0x30000 && w[7].val == 0x15D40, "Fix limit");
    CHECK(w[8].va == 0x553C5B && w[8].val == 0x121EB20 && w[9].va == 0x553C88 && w[9].val == 0x121EB20, "the two resets");
    TpPlan(300, w);
    for (i = 0; i < TP_NSITES; ++i) CHECK(w[i].val == w[i].stock, "N 300 writes the stock value at %X", w[i].va);
    for (n = 301; n <= TP_MAX_N; ++n) {
        TpPlan(n, w);
        CHECK(!(w[0].val & 15), "N %d: the packet buffer stays 16-aligned", n);
        CHECK(w[7].val >= 0x8000 && w[5].val + TP_PK_GET_SLACK == w[7].val, "N %d: the packet buffer keeps %X bytes", n, w[7].val);
        CHECK(w[0].val + w[7].val == TP_PK_END, "N %d: the buffer keeps its end", n);
        CHECK((w[0].val & 0x0FFFFFFFu) == w[0].val && (w[0].val & 0x4FFFFFFFu), "N %d: the PS2 address masks keep the base", n);
    }
}

static void TestSlots(void)
{
    g_tpN = 300;
    CHECK(TaskSlotOk((void *)(size_t)0x11B6020) && TaskSlotOk((void *)(size_t)(0x11B6020 + 299 * 0x430)), "stock: slots 0 and 299");
    CHECK(!TaskSlotOk((void *)(size_t)0x1204860), "stock: slot 300 is the packet buffer");
    CHECK(!TaskSlotOk((void *)(size_t)0x11B6024) && !TaskSlotOk(NULL) && !TaskSlotOk((void *)(size_t)0x11B6000), "not a slot start");
    g_tpN = 400;
    CHECK(TaskSlotOk((void *)(size_t)0x1204860) && TaskSlotOk((void *)(size_t)(0x11B6020 + 399 * 0x430)), "grown: slots 300, 399");
    CHECK(!TaskSlotOk((void *)(size_t)0x121EB20), "grown: slot 400 is the packet buffer");
    CHECK(TaskSlotAt(0) == (unsigned char *)(size_t)0x11B6020 && TaskSlotAt(300) == (unsigned char *)(size_t)0x1204860, "TaskSlotAt");
    g_tpN = TP_STOCK_N;
    CHECK(TpGateFree(40, 300, 40) && !TpGateFree(39, 300, 40), "stock gate: 40 free");
    CHECK(TpGateFree(140, 400, 40) && !TpGateFree(139, 400, 40), "grown gate: the same 260 in use as stock");
}

typedef struct { unsigned char b[IK_REC]; } Ik;
static Ik g_ik[16];
static void IkLink(int a, int b) { *(unsigned char **)(g_ik[a].b + IK_NEXT) = b < 0 ? NULL : g_ik[b].b; }

static void TestIk(void)
{
    const unsigned char *base = g_ik[0].b;
    int i;
    memset(g_ik, 0, sizeof g_ik);
    for (i = 0; i < 16; ++i) IkLink(i, i + 1 < 16 ? i + 1 : -1);
    CHECK(IkCountFree(base, base, 16) == 16, "all 16 free");
    CHECK(IkCountFree(g_ik[10].b, base, 16) == 6, "from 10: 6 free");
    CHECK(IkCountFree(NULL, base, 16) == 0, "empty list: 0");
    CHECK(IkCountFree(NULL, NULL, 0) == 0, "no pool (the world reset's moment): 0");
    IkLink(15, 3);
    CHECK(IkCountFree(base, base, 16) == -1, "a cycle: -1");
    IkLink(15, -1);
    *(unsigned char **)(g_ik[4].b + IK_NEXT) = g_ik[5].b + 4;
    CHECK(IkCountFree(base, base, 16) == -1, "a link inside a record: -1");
    IkLink(4, 5);
    CHECK(IkCountFree(base, base, 8) == -1, "a link past num: -1");
    CHECK(IkCountFree(base, g_ik[1].b, 15) == -1, "the head below the base: -1");
}

static void TestRoom(void)
{
    TpRoom r;
    TpRoomClear(&r);
    TpRoomAdd(&r, 400, 120, 0, 512, 500, 250, 9000);
    TpRoomAdd(&r, 400, 310, 10, 512, 470, 230, 9100);
    TpRoomAdd(&r, 400, 200, 0, 512, -1, 240, 8000);
    CHECK(r.samples == 3 && r.tpUsedMax == 310 && r.tpFreeMin == 90 && r.tpHighMax == 10, "tasks: max 310, fewest free 90, high 10");
    CHECK(r.ikUsedMax == 42 && r.ikFreeMin == 470 && r.ikBad == 1 && r.ikNum == 512, "IK: 42 used at most, 1 unreadable");
    CHECK(r.actFreeMin == 230 && r.heapMax == 9100, "actors 230, heap 9100");
    TpRoomClear(&r);
    CHECK(r.samples == 0 && r.tpFreeMin == 0x7FFFFFFF && r.ikFreeMin == 0x7FFFFFFF, "cleared");
}

static void TestLift3361(void)  // v3.36.1 TL3361
{
    CHECK(TpGateFreeAll(40, 400, 40, 1) && !TpGateFreeAll(39, 400, 40, 1), "all: keep 40 free of the whole pool");
    CHECK(!TpGateFreeAll(139, 400, 40, 0) && TpGateFreeAll(140, 400, 40, 0), "reserve: keep 40 of the first 300 (100 more)");
    CHECK(TpGateFreeAll(40, 300, 40, 0) == TpGateFreeAll(40, 300, 40, 1), "a stock pool: both the same");
    CHECK(TpPeReadsCount(0x00010014ul, 0x00010000ul) && TpPeReadsCount(0x00010015ul, 0) && TpPeReadsCount(0x00020000ul, 0),
          "PE 1.20.1, 1.21, 2.0 read the count");
    CHECK(!TpPeReadsCount(0x00010014ul, 0) && !TpPeReadsCount(0x00010013ul, 0x00090000ul) && !TpPeReadsCount(0, 0),
          "PE 1.20.0, 1.19.9, 0.0 do not");
}
int main(void)
{
    TestLift3361();  // v3.36.1 TL3361
    TestPoolN();
    TestPlan();
    TestSlots();
    TestIk();
    TestRoom();
    printf("pools_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
