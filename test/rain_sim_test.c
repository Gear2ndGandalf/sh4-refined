// v3.26.0 rain_sim.h native test (PLAN 3.2 / 5; review SVR-302): the production simulation, fed different frame
// sequences, must reach bit-identical states at the same step counts; the pool admission must reuse any expired slot;
// a forced merge must merge. Optional: `rain_sim_test dump <file> <seconds> <strength>` writes the states of a 30 fps run
// as JSON lines for tools/rain_preview.py.
// Build: test\build_rain_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <stdlib.h>
#include "../src/rain_sim.h"

#define AW 1280.0f
static RnState g_run;
static int g_fail;

#define FREQ 10000000LL  // a QueryPerformanceFrequency (ticks a second)
// frame k of sequence `kind` ends at this many ticks (integer, as QPC deltas are)
static long long SeqEnd(int kind, int k)
{
    static const int kFps[4] = { 30, 60, 120, 180 };
    if (kind < 4) return (long long)k * FREQ / kFps[kind];
    if (kind == 4) {  // uneven: 1/200 .. 1/20 s frames (hashed), cumulative
        static long long acc[100000];
        static int built;
        if (!built) {
            int i;
            built = 1;
            for (i = 1; i < 100000; ++i)
                acc[i] = acc[i - 1] + FREQ / 200 + (long long)((double)(FREQ / 20 - FREQ / 200) * RnRnd((RnU32)i * 0x9E3779B9u + 17u));
        }
        return acc[k];
    }
    return k <= 50 ? (long long)k * FREQ / 144 : 50LL * FREQ / 144 + 4000000LL + (long long)(k - 50) * FREQ / 144;  // a 0.4 s frame
}
// INSP326-007: the production RnAdvance only, frames ending exactly at each checkpoint time (a frame that would pass a
// checkpoint is cut there: a frame boundary at that time); the WHOLE RnState is compared, nothing reconstructed.
static void RunSeq(int kind, const long long *cpt, int ncp, RnState refs[], int *have, float str)
{
    long long t = 0;
    int k = 1, c = 0, frames = 0;
    RnInit(&g_run, AW);
    while (c < ncp) {
        long long end = SeqEnd(kind, k);
        if (end > cpt[c]) end = cpt[c];
        else ++k;
        RnAdvance(&g_run, end - t, FREQ, AW, str);
        t = end;
        ++frames;
        if (t == cpt[c]) {
            const long long want = cpt[c] * RN_HZ / FREQ;
            if (g_run.step != want) {
                printf("FAIL: sequence %d: %d steps at %lld ticks (want %lld)\n", kind, g_run.step, t, want);
                g_fail = 1;
            }
            if (!have[c]) {
                memcpy(&refs[c], &g_run, sizeof g_run);
                have[c] = 1;
            } else if (memcmp(&refs[c], &g_run, sizeof g_run)) {
                printf("FAIL: sequence %d: the state at %.1f s differs from sequence 0's\n", kind, (double)cpt[c] / FREQ);
                g_fail = 1;
            }
            ++c;
        }
    }
    printf("sequence %d: %d frames, %.3f s, %d steps, segs refused %d, beads refused %d\n", kind, frames, (double)t / FREQ,
           g_run.step, g_run.segRefused, g_run.beadRefused);
}
static void TestIdentity(float str)
{
    static RnState refs[3];
    const long long cpt[3] = { 2 * FREQ, 5 * FREQ, 10 * FREQ };
    int have[3] = { 0, 0, 0 }, k, i, alive = 0, segs = 0, beads = 0;
    for (k = 0; k < 6; ++k) RunSeq(k, cpt, 3, refs, have, str);
    for (i = 0; i < RN_DROPS; ++i) alive += refs[2].d[i].st != 0;
    for (i = 0; i < RN_SEGS; ++i) segs += refs[2].seg[i].life > 0 && refs[2].step - refs[2].seg[i].born < refs[2].seg[i].life;
    for (i = 0; i < RN_BEADS; ++i) beads += refs[2].bead[i].life > 0 && refs[2].step - refs[2].bead[i].born < refs[2].bead[i].life;
    printf("strength %.2f at 10 s: %u drops spawned, %d alive, %d live segments, %d live beads\n", str, refs[2].serial, alive, segs, beads);
    if (segs == 0 || beads == 0) {
        printf("FAIL: no trails / beads in 10 s (the test would prove little)\n");
        g_fail = 1;
    }
}
static void TestStall(void)  // one 0.8 s frame: at most 60 steps (0.5 s) taken, the excess dropped, the clock re-based
{
    RnInit(&g_run, AW);
    RnAdvance(&g_run, FREQ / 10, FREQ, AW, 0.5f);           // 12 steps
    RnAdvance(&g_run, 8 * FREQ / 10, FREQ, AW, 0.5f);       // a stall: 96 due, 60 taken
    if (g_run.step != 12 + RN_MAX_STEPS) {
        printf("FAIL: stall: %d steps after the stall (want %d)\n", g_run.step, 12 + RN_MAX_STEPS);
        g_fail = 1;
    }
    RnAdvance(&g_run, FREQ / 60, FREQ, AW, 0.5f);           // the next frame (1/60 s): 1 or 2 steps, no catch-up
    if (g_run.step < 12 + RN_MAX_STEPS + 1 || g_run.step > 12 + RN_MAX_STEPS + 2) {
        printf("FAIL: stall: %d steps after the next frame\n", g_run.step);
        g_fail = 1;
    } else {
        printf("stall: a 0.8 s frame took %d steps; the next 1/60 s frame %d (no catch-up)\n", RN_MAX_STEPS, g_run.step - 12 - RN_MAX_STEPS);
    }
}
static void TestAdmission(void)
{
    int i, j0, j1;
    RnInit(&g_run, AW);
    g_run.step = 1000;
    for (i = 0; i < RN_BEADS; ++i) g_run.bead[i].life = 100, g_run.bead[i].born = 950;  // all alive (50 of 100 steps)
    g_run.beadNext = 10;
    g_run.bead[10].life = 5000, g_run.bead[10].born = 990;  // a long-lived bead at the cursor
    g_run.bead[200].born = 800;                              // the first expired slot after the cursor
    g_run.bead[500].born = 800;
    RnBeadAdd(&g_run, 1, 2, 1.5f, 50, 7u);
    j0 = g_run.bead[200].born == 1000;
    RnBeadAdd(&g_run, 1, 2, 1.5f, 50, 8u);
    j1 = g_run.bead[500].born == 1000;
    RnBeadAdd(&g_run, 1, 2, 1.5f, 50, 9u);  // none free now: refused, nothing overwritten
    if (!j0 || !j1 || g_run.beadRefused != 1 || g_run.bead[10].born != 990) {
        printf("FAIL: bead admission (first %d, second %d, refused %d, long-lived kept %d)\n", j0, j1, g_run.beadRefused,
               g_run.bead[10].born == 990);
        g_fail = 1;
    } else {
        printf("admission: expired slots 200 / 500 reused past a live entry at the cursor; a full ring refuses\n");
    }
}
static void TestMerge(void)
{
    RnDrop *a, *b;
    RnInit(&g_run, AW);
    g_run.spawnLeft = 1 << 30;  // no spawns
    a = &g_run.d[0], b = &g_run.d[1];
    a->st = 2, a->ser = 1, a->x = 100, a->y = 100, a->r = 8, a->lx = 100, a->ly = 100, a->trail = 400, a->hd = a->hd0 = 0;
    b->st = 1, b->ser = 2, b->x = 100, b->y = 112, b->r = 6, b->sit = 100000, b->life = 200000;
    {
        int k;
        for (k = 0; k < 240 && b->st; ++k) RnStep(&g_run);
    }
    if (b->st || !(a->r > 8.5f || a->st != 2)) {
        printf("FAIL: merge (b st %d, a r %.2f)\n", b->st, a->r);
        g_fail = 1;
    } else {
        printf("merge: the slider swallowed the drop below it (r now %.2f)\n", a->r);
    }
}
static void Dump(const char *path, float secs, float str)
{
    FILE *f = fopen(path, "w");
    int fr, i, nd, n = (int)(secs * 30.0f);
    if (!f) {
        printf("cannot write %s\n", path);
        exit(2);
    }
    RnInit(&g_run, AW);
    for (fr = 0; fr < n; ++fr) {
        RnAdvance(&g_run, (long long)(fr + 1) * FREQ / 30 - (long long)fr * FREQ / 30, FREQ, AW, str);
        fprintf(f, "{\"t\":%.4f,\"drops\":[", g_run.step / (double)RN_HZ);
        for (i = 0, nd = 0; i < RN_DROPS; ++i) {
            const RnDrop *d = &g_run.d[i];
            if (!d->st) continue;
            fprintf(f, "%s[%.2f,%.2f,%.3f,%d,%d,%d,%.2f,%d]", nd++ ? "," : "", d->x, d->y, d->r, d->st, d->age, d->shape, d->spd, d->bead);
        }
        fprintf(f, "],\"segs\":[");
        {
            int first = 1;
            for (i = 0; i < RN_SEGS; ++i) {
                const RnSeg *g = &g_run.seg[i];
                const int age = g_run.step - g->born;
                if (g->life <= 0 || age < 0 || age >= g->life) continue;
                fprintf(f, "%s[%.2f,%.2f,%.2f,%.2f,%.3f,%.4f]", first ? "" : ",", g->x0, g->y0, g->x1, g->y1, g->w,
                        1.0 - (double)age / g->life);
                first = 0;
            }
            fprintf(f, "],\"beads\":[");
            first = 1;
            for (i = 0; i < RN_BEADS; ++i) {
                const RnBead *b = &g_run.bead[i];
                const int age = g_run.step - b->born;
                if (b->life <= 0 || age < 0 || age >= b->life) continue;
                fprintf(f, "%s[%.2f,%.2f,%.3f,%d,%d,%u]", first ? "" : ",", b->x, b->y, b->r, age, b->life, (b->seed >> 8) & 3);
                first = 0;
            }
            fprintf(f, "],\"mist\":[");
            for (i = 0; i < RN_MIST; ++i) {
                float x, y, r;
                RnMistPos(&g_run, i, &x, &y, &r);
                fprintf(f, "%s[%.2f,%.2f,%.3f,%d,%d]", i ? "," : "", x, y, r, g_run.mist[i].cleared >= 0 ? g_run.step - g_run.mist[i].cleared : -1,
                        g_run.step - g_run.mist[i].appear);
            }
        }
        fprintf(f, "]}\n");
    }
    fclose(f);
    printf("dumped %d frames to %s\n", n, path);
}
int main(int argc, char **argv)
{
    if (argc >= 5 && !strcmp(argv[1], "dump")) {
        Dump(argv[2], (float)atof(argv[3]), (float)atof(argv[4]));
        return 0;
    }
    printf("RnState %u bytes\n", (unsigned)sizeof(RnState));
    TestIdentity(0.25f);
    TestIdentity(1.0f);  // saturated
    TestStall();
    TestAdmission();
    TestMerge();
    printf(g_fail ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return g_fail;
}
