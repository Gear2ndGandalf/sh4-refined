// v3.32 NC332 / v3.33 NC333: native test of src/noclip_sim.h (staging/v333_feedback/w3_noclip/PLAN.md, Verification).
// A synthetic level with the tree2 semantics the game has (solid volumes; a line that starts inside solid finds
// nothing): a staircase of 40-unit risers 85 deep flanked by full-height walls, a 400-thick wall, a low box, a bed, a
// seat with a backrest, a column prop, a pit (no floor) and the room's box; a simplified stock step (blocked when a box
// is near her body, a step when the upper body is free, else no move). Walks at 30 / 60 / 120 / 180 Hz (300 units / s):
// stairs up / down (middle, drifting into the side wall and back), props passed at floor level, the thick wall crossed,
// the pit and the box edge held with the way back open, non-finite input, the no-tree2 fallback; Fly (pitch, back,
// strafe, no input, the box); the landing; the latch; the companions' catch-up timers; the softlock watch.
// Build and run: test\build_nc_test.cmd (cl /O2 x86, as build.cmd). The real-collision walk (the Subway's dumped
// collision through the game's own queries): staging/v333_feedback/w3_noclip/re/walk_real.py.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "../src/noclip_sim.h"

static int g_fail, g_checks;
static void Check(int ok, const char *what)
{
    ++g_checks;
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}

// ---------------------------------------------------------------- the synthetic level (y grows downward)
typedef struct { float x0, x1, y0, y1, z0, z1; } Box;  // solid for x0..x1, y0 (top) .. y1 (bottom), z0..z1
#define NB 64
static Box g_box[NB];
static int g_nb, g_noTree2;
static void Add(float x0, float x1, float y0, float y1, float z0, float z1)
{
    Box b = { x0, x1, y0, y1, z0, z1 };
    g_box[g_nb++] = b;
}
// no ground in the pit and outside the room's box (-2000 .. 3000)
static int InPit(float x, float z)
{
    return (x >= 1500.0f && x <= 3000.0f && z >= -500.0f && z <= 500.0f) || x < -2000.0f || x > 3000.0f || z < -2000.0f || z > 3000.0f;
}
static int SolidAt(float x, float y, float z)
{
    int i;
    if (y > 0.0f && !InPit(x, z)) return 1;  // the ground
    for (i = 0; i < g_nb; ++i) {
        const Box *b = &g_box[i];
        if (x >= b->x0 && x <= b->x1 && z >= b->z0 && z <= b->z1 && y > b->y0 && y <= b->y1) return 1;
    }
    return 0;
}
static int WSolid(void *c, float x, float y, float z)
{
    (void)c;
    return g_noTree2 ? -1 : SolidAt(x, y, z);
}
// the first surface from yTop down to yBot; nothing when yTop is inside solid (as the BSP query)
static int WFloor(void *c, float x, float yTop, float yBot, float z, float *fy)
{
    float best = 1e30f;
    int i;
    (void)c;
    if (!g_noTree2 && SolidAt(x, yTop, z)) return 0;
    if (!InPit(x, z) && 0.0f >= yTop && 0.0f <= yBot) best = 0.0f;
    for (i = 0; i < g_nb; ++i) {
        const Box *b = &g_box[i];
        if (x >= b->x0 && x <= b->x1 && z >= b->z0 && z <= b->z1 && b->y0 >= yTop && b->y0 <= yBot && b->y0 < best) best = b->y0;
    }
    if (best > 1e29f) return 0;
    *fy = best;
    return 1;
}
// near: a box within r (xz) of p overlapping the band y0 .. y1 (y0 above y1)
static int BandHit(float x, float z, float y0, float y1, float r)
{
    int i;
    for (i = 0; i < g_nb; ++i) {
        const Box *b = &g_box[i];
        if (x >= b->x0 - r && x <= b->x1 + r && z >= b->z0 - r && z <= b->z1 + r && b->y0 < y1 && b->y1 > y0) return 1;
    }
    return 0;
}
// the stock step, simplified: the body (2..142 up, 40 round) free -> the floor +-100 at the wanted point; blocked low
// but free 100..240 up -> a step (the same snap); else no move
static void WStock(void *c, const float *old, const float *want, float *out)
{
    float fy;
    (void)c;
    out[0] = old[0], out[1] = old[1], out[2] = old[2];
    if (BandHit(want[0], want[2], want[1] - 142.0f, want[1] - 2.0f, 40.0f) &&
        BandHit(want[0], want[2], want[1] - 240.0f, want[1] - 100.0f, 40.0f))
        return;
    if (WFloor(0, want[0], want[1] - 100.0f, want[1] + 100.0f, want[2], &fy)) out[0] = want[0], out[1] = fy, out[2] = want[2];
}
static NcWorld g_wd;
static void Level(void)
{
    int k;
    g_nb = 0;
    // the staircase: x 0..400, step k from z 1000 + 85 (k - 1), top 40 k; the landing (k = 12) runs to z 2400
    for (k = 1; k <= 12; ++k) Add(0.0f, 400.0f, -40.0f * (float)k, 0.0f, 1000.0f + 85.0f * (float)(k - 1), 2400.0f);
    Add(400.0f, 450.0f, -1000.0f, 0.0f, 900.0f, 2400.0f);   // its right-hand wall, full height
    Add(-50.0f, 0.0f, -1000.0f, 0.0f, 900.0f, 2400.0f);     // its left-hand wall
    Add(600.0f, 1000.0f, -1000.0f, 0.0f, -500.0f, 500.0f);  // a wall 400 thick
    Add(-500.0f, -400.0f, -40.0f, 0.0f, -100.0f, 100.0f);   // a low box, 40 high (lane z 0)
    Add(-500.0f, -300.0f, -50.0f, 0.0f, 300.0f, 500.0f);    // a bed, 50 high (lane z 400)
    Add(-500.0f, -450.0f, -42.0f, 0.0f, 700.0f, 900.0f);    // a seat, 42 high (lane z 800) ...
    Add(-450.0f, -440.0f, -100.0f, 0.0f, 700.0f, 900.0f);   // ... and its backrest
    Add(-1100.0f, -1050.0f, -1000.0f, 0.0f, -450.0f, -350.0f);  // a column prop (lane z -400)
    Add(-1800.0f, -1400.0f, -40.0f, 0.0f, 1000.0f, 1600.0f);    // a raised floor, 40 high, 600 long (a platform)
    g_wd.solid = WSolid;
    g_wd.floorAt = WFloor;
    g_wd.stock = WStock;
    g_wd.ctx = 0;
    g_wd.boxValid = 1;
    g_wd.boxMin[0] = -2000.0f, g_wd.boxMin[1] = -1000.0f, g_wd.boxMin[2] = -2000.0f;
    g_wd.boxMax[0] = 3000.0f, g_wd.boxMax[1] = 0.0f, g_wd.boxMax[2] = 3000.0f;
}

// ---------------------------------------------------------------- walks
typedef struct { float dx, dz, dist; } Leg;
typedef struct { float minY, maxY; int dec[6]; float maxX, minX; } Stat;
static NcWalk g_w;
static void Walk(int hz, float *pos, const Leg *legs, int nl, Stat *st, int fresh)
{
    const float step = 300.0f / (float)hz;
    int l;
    if (fresh) NcWalkReset(&g_w);
    memset(st, 0, sizeof *st);
    st->minY = st->maxY = pos[1];
    st->minX = st->maxX = pos[0];
    for (l = 0; l < nl; ++l) {
        const float len = sqrtf(legs[l].dx * legs[l].dx + legs[l].dz * legs[l].dz);
        long n = (long)(legs[l].dist / step + 0.5f), i;
        for (i = 0; i < n; ++i) {
            float want[3], out[3];
            int d;
            want[0] = pos[0] + legs[l].dx / len * step;
            want[1] = pos[1];
            want[2] = pos[2] + legs[l].dz / len * step;
            d = NcDecideOn(&g_w, &g_wd, pos, want, out);
            ++st->dec[d];
            memcpy(pos, out, sizeof out);
            if (pos[1] < st->minY) st->minY = pos[1];
            if (pos[1] > st->maxY) st->maxY = pos[1];
            if (pos[0] < st->minX) st->minX = pos[0];
            if (pos[0] > st->maxX) st->maxX = pos[0];
        }
    }
}
static void Msg(char *m, size_t n, const char *what, int hz, const float *p, const Stat *s)
{
    _snprintf(m, n, "%s at %d Hz: end %.0f, %.1f, %.0f (y %.0f .. %.0f; stock %d floor %d climb %d solid %d air %d hold %d)",
              what, hz, p[0], p[1], p[2], s->minY, s->maxY, s->dec[NC_STOCK], s->dec[NC_FLOOR], s->dec[NC_CLIMB], s->dec[NC_SOLID],
              s->dec[NC_AIR], s->dec[NC_HOLD]);
    m[n - 1] = 0;
}

static void TestWalks(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[300];
    int i;
    Level();
    for (i = 0; i < 4; ++i) {
        const int hz = kHz[i];
        Stat s;
        float p[3];
        {   // stairs up the middle (the stock step), then back down
            static const Leg up[1] = { { 0, 1, 1500 } }, dn[1] = { { 0, -1, 1500 } };
            p[0] = 200, p[1] = 0, p[2] = 800;
            Walk(hz, p, up, 1, &s, 1);
            Msg(m, sizeof m, "stairs up (middle)", hz, p, &s);
            Check(fabsf(p[1] + 480.0f) < 0.5f && p[2] > 2200.0f, m);
            Walk(hz, p, dn, 1, &s, 0);
            Msg(m, sizeof m, "stairs down (middle)", hz, p, &s);
            Check(fabsf(p[1]) < 0.5f && p[2] < 900.0f, m);
        }
        {   // stairs up drifting into the right-hand wall, inside it, back out to the middle and on up
            static const Leg legs[4] = { { 1, 4, 700 }, { 0, 1, 250 }, { -1, 1, 300 }, { 0, 1, 380 } };  // (the landing ends at z 2400)
            p[0] = 250, p[1] = 0, p[2] = 800;
            Walk(hz, p, legs, 4, &s, 1);
            Msg(m, sizeof m, "stairs up through the side wall", hz, p, &s);
            Check(fabsf(p[1] + 480.0f) < 0.5f && s.maxX > 400.0f && s.dec[NC_SOLID] > 0 && s.dec[NC_HOLD] == 0, m);
        }
        {   // down the stairs drifting into the left-hand wall, back to the middle and down
            static const Leg legs[4] = { { -1, -5, 900 }, { 0, -1, 250 }, { 1, -1, 300 }, { 0, -1, 300 } };
            p[0] = 150, p[1] = -480, p[2] = 2300;
            Walk(hz, p, legs, 4, &s, 1);
            Msg(m, sizeof m, "stairs down through the side wall", hz, p, &s);
            Check(fabsf(p[1]) < 0.5f && s.minX < 0.0f && s.dec[NC_HOLD] == 0, m);
        }
        {   // into the stair block from its side at floor level (through the wall), and back out: never stuck
            static const Leg legs[2] = { { 1, 0, 400 }, { -1, 0, 400 } };
            p[0] = -150, p[1] = 0, p[2] = 1600;
            Walk(hz, p, legs, 2, &s, 1);
            Msg(m, sizeof m, "into the stair block from the side and back", hz, p, &s);
            Check(fabsf(p[1]) < 0.5f && fabsf(p[0] + 150.0f) < 2.0f && s.dec[NC_HOLD] == 0 && s.maxX > 200.0f, m);
        }
        {   // a low box (40), a bed (50), a seat with a backrest (42 / 100), a column: passed at floor level both ways
            static const Leg legs[2] = { { -1, 0, 500 }, { 1, 0, 500 } };
            static const float lanes[3] = { 0.0f, 400.0f, 800.0f };
            static const char *names[3] = { "low box 40", "bed 50", "seat 42 + backrest" };
            int k;
            for (k = 0; k < 3; ++k) {
                char what[64];
                p[0] = -200, p[1] = 0, p[2] = lanes[k];
                Walk(hz, p, legs, 2, &s, 1);
                _snprintf(what, sizeof what, "%s passed at floor level", names[k]);
                Msg(m, sizeof m, what, hz, p, &s);
                Check(s.minY > -0.5f && s.maxY < 0.5f && s.minX < -550.0f && fabsf(p[0] + 200.0f) < 2.0f && s.dec[NC_SOLID] > 0, m);
            }
            {
                static const Leg col[2] = { { -1, 0, 400 }, { 1, 0, 400 } };
                p[0] = -900, p[1] = 0, p[2] = -400;
                Walk(hz, p, col, 2, &s, 1);
                Msg(m, sizeof m, "column prop passed", hz, p, &s);
                Check(s.minY > -0.5f && s.maxY < 0.5f && s.minX < -1250.0f && fabsf(p[0] + 900.0f) < 2.0f, m);
            }
        }
        {   // a raised floor 40 high and 600 long (no stairs): stepped onto (a floor, not a prop), and off its far end
            static const Leg legs[1] = { { 0, 1, 1000 } };
            p[0] = -1600, p[1] = 0, p[2] = 800;
            Walk(hz, p, legs, 1, &s, 1);
            Msg(m, sizeof m, "raised floor 40 x 600: stepped onto and off", hz, p, &s);
            Check(fabsf(s.minY + 40.0f) < 0.5f && fabsf(p[1]) < 0.5f && p[2] > 1700.0f && s.dec[NC_SOLID] == 0, m);
        }
        {   // the 400-thick wall: crossed (v3.32 held 120 inside it)
            static const Leg legs[1] = { { 1, 0, 650 } };
            p[0] = 450, p[1] = 0, p[2] = 0;
            Walk(hz, p, legs, 1, &s, 1);
            Msg(m, sizeof m, "400-thick wall crossed", hz, p, &s);
            Check(p[0] > 1090.0f && fabsf(p[1]) < 0.5f && s.dec[NC_HOLD] == 0, m);
        }
        {   // the pit (no floor): a short way in at her height, then she holds; back out is allowed
            static const Leg legs[2] = { { 1, 0, 600 }, { -1, 0, 600 } };
            p[0] = 1200, p[1] = 0, p[2] = 0;
            Walk(hz, p, legs, 2, &s, 1);
            Msg(m, sizeof m, "pit: holds, the way back is open", hz, p, &s);
            Check(s.maxX <= 1500.0f + NC_GAP + 300.0f / hz + 0.01f && s.maxX > 1550.0f && s.maxY < 0.5f && s.minY > -0.5f &&
                  p[0] < 1100.0f && s.dec[NC_HOLD] > 0, m);
        }
        {   // the room's box edge (x -2000): holds outside within NC_GAP, back is allowed
            static const Leg legs[2] = { { -1, 0, 500 }, { 1, 0, 500 } };
            p[0] = -1800, p[1] = 0, p[2] = 0;
            Walk(hz, p, legs, 2, &s, 1);
            Msg(m, sizeof m, "room box edge: holds, back is allowed", hz, p, &s);
            Check(s.minX >= -2000.0f - NC_GAP - 300.0f / hz - 0.01f && p[0] > -1850.0f && s.dec[NC_HOLD] > 0, m);
        }
    }
    {   // non-finite input: hold
        float o[3] = { 0, 0, 0 }, wnt[3] = { NAN, 0, 0 }, out[3];
        NcWalkReset(&g_w);
        Check(NcDecideOn(&g_w, &g_wd, o, wnt, out) == NC_HOLD && out[0] == 0.0f, "a NaN wanted point holds");
        wnt[0] = 1.0f, wnt[2] = INFINITY;
        Check(NcDecideOn(&g_w, &g_wd, o, wnt, out) == NC_HOLD && out[2] == 0.0f, "an infinite wanted point holds");
    }
    {   // no tree2 (solid unknown): the floor probe from 24 up / 100 down (a polygon line test finds the floor under a
        // wall: crossed); the pit still holds
        static const Leg legs[1] = { { 1, 0, 1400 } };
        float p[3] = { 450, 0, 0 };
        Stat s;
        g_noTree2 = 1;
        Walk(60, p, legs, 1, &s, 1);
        g_noTree2 = 0;
        _snprintf(m, sizeof m, "no solid tree: the thick wall crossed, the pit holds (end x %.0f, floor %d air %d hold %d)", p[0],
                  s.dec[NC_FLOOR], s.dec[NC_AIR], s.dec[NC_HOLD]);
        Check(p[0] > 1500.0f && p[0] <= 1500.0f + NC_GAP + 5.01f && s.dec[NC_HOLD] > 0 && fabsf(p[1]) < 0.5f, m);
    }
}

// ---------------------------------------------------------------- v3.37 MN337: Fly's own speed
// The game's step gives the direction only: running flies NoClipFlySpeed x 663 u/s (default 2.5), walking 663 u/s, at
// every frame rate, whatever the step's length (Movement Speed x2 does not stack); the vertical part along the pitch.
static void TestFlySpeed(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[220];
    int i, r, ms;
    Level();
    Check(NcFlyK(2.5f) == 2.5f && NcFlyK(0.5f) == 1.0f && NcFlyK(9.0f) == 4.0f && NcFlyK(NAN) == NC_FLY_K_DEF && NcFlyK(1.0f) == 1.0f,
          "fly speed: NoClipFlySpeed clamped to 1 .. 4 (not a number = 2.5)");
    Check(fabsf(NcFlySpeed(0.0f, 2.5f) - 663.0f) < 0.01f && fabsf(NcFlySpeed(1.0f, 2.5f) - 1657.5f) < 0.01f &&
              fabsf(NcFlySpeed(0.5f, 2.5f) - 1160.25f) < 0.01f && fabsf(NcFlySpeed(NAN, 2.5f) - 663.0f) < 0.01f &&
              fabsf(NcFlySpeed(3.0f, 2.5f) - 1657.5f) < 0.01f && fabsf(NcFlySpeed(-1.0f, 4.0f) - 663.0f) < 0.01f &&
              fabsf(NcFlySpeed(1.0f, 4.0f) - 2652.0f) < 0.01f,
          "fly speed: walk 663 u/s, run 2.5 x 663 = 1657.5, half-eased run between; the run value clamped to 0 .. 1");
    for (i = 0; i < 4; ++i)
        for (r = 0; r < 2; ++r)
            for (ms = 1; ms <= 2; ++ms) {  // Movement Speed Normal / x2: the game's step twice as long
                const int hz = kHz[i];
                const float dt = 1.0f / (float)hz, run = r ? 1.0f : 0.0f;
                const float gameStep = (r ? 663.0f : 175.5f) * (float)ms / (float)hz;
                const float level[3] = { 0.0f, 0.0f, 1.0f };
                const float want0 = NcFlySpeed(run, NC_FLY_K_DEF);
                float p[3] = { 0, -200, -1000 }, out[3], w2[3];
                int t, ok = 1;
                for (t = 0; t < hz; ++t) {
                    const float want[3] = { p[0], p[1], p[2] + gameStep };
                    ok &= NcFlyAbsStep(p, want, NcFlySpeed(run, NC_FLY_K_DEF), dt, w2);
                    NcFlyStep(&g_wd, p, w2, level, out);
                    memcpy(p, out, sizeof p);
                }
                _snprintf(m, sizeof m, "fly speed: 1 s of %s input at %d Hz, Movement Speed x%d: %.1f units (%.1f)", r ? "running" : "walking",
                          hz, ms, p[2] + 1000.0f, want0);
                Check(ok && fabsf(p[2] + 1000.0f - want0) < 0.5f && fabsf(p[1] + 200.0f) < 0.01f, m);
            }
    {   // looking 30 degrees up while walking: the speed is the length along the view (rise = half of it)
        const float pitch = 0.5235988f, fwd[3] = { 0.0f, -sinf(pitch), cosf(pitch) };
        float p[3] = { 0, -200, -1000 }, out[3], w2[3];
        int t;
        for (t = 0; t < 60; ++t) {
            const float want[3] = { p[0], p[1], p[2] + 3.0f };
            NcFlyAbsStep(p, want, NcFlySpeed(0.0f, NC_FLY_K_DEF), 1.0f / 60.0f, w2);
            NcFlyStep(&g_wd, p, w2, fwd, out);
            memcpy(p, out, sizeof p);
        }
        _snprintf(m, sizeof m, "fly speed: 1 s walking, looking 30 deg up: rose %.1f (331.5), forward %.1f (574.2)", -200.0f - p[1], p[2] + 1000.0f);
        Check(fabsf(-200.0f - p[1] - 331.5f) < 0.5f && fabsf(p[2] + 1000.0f - 574.2f) < 0.5f, m);
    }
    {
        const float p[3] = { 5, -300, 5 }, same[3] = { 5, -300, 5 }, tiny[3] = { 5.00001f, -300, 5 }, fw[3] = { 5, -300, 6 };
        const float bad[3] = { NAN, -300, 5 };
        float w2[3];
        Check(!NcFlyAbsStep(p, same, 1657.5f, 1.0f / 60.0f, w2) && w2[0] == 5.0f && w2[2] == 5.0f &&
                  !NcFlyAbsStep(p, tiny, 1657.5f, 1.0f / 60.0f, w2),
              "fly speed: no input (no step), no move");
        Check(!NcFlyAbsStep(p, fw, 1657.5f, 0.0f, w2) && !NcFlyAbsStep(p, bad, 1657.5f, 0.1f, w2) && !NcFlyAbsStep(p, fw, NAN, 0.1f, w2) &&
                  w2[0] == 5.0f && w2[2] == 5.0f,
              "fly speed: no time (a paused tick), a broken step or speed: no move");
        Check(NcFlyAbsStep(p, fw, 1657.5f, 0.1f, w2) && fabsf(w2[2] - 5.0f - 165.75f) < 0.01f && w2[1] == -300.0f,
              "fly speed: the clamped 0.1 s tick at a run moves 165.75 at most");
    }
    // the input amount from the game's input block (forward / side = frame time x stick x 30)
    for (i = 0; i < 4; ++i) {
        const float dt = 1.0f / (float)kHz[i], u = dt * 30.0f;
        Check(NcFlyInputAmount(u, 0, dt) == 1.0f && NcFlyInputAmount(-0.8f * u, 0, dt) == 1.0f && NcFlyInputAmount(0, 0.8f * u, dt) == 1.0f &&
                  NcFlyInputAmount(0, -0.8f * u, dt) == 1.0f && NcFlyInputAmount(0, 0, dt) == 0.0f &&
                  fabsf(NcFlyInputAmount(0.4f * u, 0, dt) - 0.5f) < 1e-5f,
              "fly input: full stick / back / side step = 1, none = 0, a 40 % stick = half (every frame rate)");
    }
    Check(NcFlyInputAmount(NAN, 0, 0.016f) == 0.0f && NcFlyInputAmount(0.5f, 0, 0.0f) == 0.0f && NcFlyInputAmount(6.0f, 0, 0.1f) == 1.0f,
          "fly input: not a number or no time = 0; a long tick (frame time over the 0.1 clamp) stays 1");
    // v3.37 MN337 (the coordinator: stop within about 0.15 s / 150 units, a short start): Fly's own envelope over the
    // stick, at every frame rate; the game's step keeps its eased direction for 0.67 s after the release (52F260)
    Check(NcFlyTarget(0, 0, 0, 0.016f) == 0.0f && NcFlyTarget(0, -1.0f, 0, 0.016f) == 1.0f && NcFlyTarget(0.8f, 0, 0, 0.016f) == 1.0f &&
              fabsf(NcFlyTarget(0.4f, 0.1f, 0, 0.016f) - 0.5f) < 1e-5f && NcFlyTarget(0.03f, 0.04f, 0, 0.016f) == 0.0f &&
              NcFlyTarget(0, 0, 0.8f * 0.016f * 30.0f, 0.016f) > 0.999f && NcFlyTarget(NAN, NAN, NAN, 0.016f) == 0.0f,
          "fly target: no stick 0; full / 0.8 stick 1; 40 % half; under the dead zone 0; the tank side step 1; not a number 0");
    Check(NcFlyEnvStep(0.0f, 1.0f, 0.0f) == 0.0f && NcFlyEnvStep(NAN, 0.0f, 0.1f) == 0.0f && NcFlyEnvStep(1.0f, 1.0f, 0.1f) == 1.0f &&
              NcFlyEnvStep(0.0f, 0.5f, 1.0f) == 0.5f && NcFlyEnvStep(1.0f, 0.5f, 1.0f) == 0.5f,
          "fly envelope: no time holds; it never passes its target");
    for (i = 0; i < 4; ++i) {
        const int hz = kHz[i];
        const float dt = 1.0f / (float)hz, level[3] = { 0.0f, 0.0f, 1.0f }, full = NcFlySpeed(1.0f, NC_FLY_K_DEF);
        float p[3] = { 0, -200, -1000 }, out[3], w2[3], env = 0.0f, last = 1e9f, z0, tUp = -1.0f, tStop = -1.0f;
        int t, slowOk = 1, pushOk = 1;
        for (t = 0; t < hz; ++t) {  // 1 s of a full stick at a run (the start ramp)
            const float want[3] = { p[0], p[1], p[2] + 5.0f };
            env = NcFlyEnvStep(env, NcFlyTarget(0.0f, -1.0f, 0.0f, dt), dt);
            if (tUp < 0.0f && env >= 1.0f) tUp = (float)(t + 1) * dt;
            if (NcFlyAbsStep(p, want, full * env, dt, w2)) NcFlyStep(&g_wd, p, w2, level, out), memcpy(p, out, sizeof p);
        }
        z0 = p[2];
        for (t = 0; t < hz; ++t) {  // let go: the game's eased step still points ahead, the stick reads 0
            const float want[3] = { p[0], p[1], p[2] + 5.0f };
            float moved;
            env = NcFlyEnvStep(env, NcFlyTarget(0.0f, 0.0f, 0.0f, dt), dt);
            if (NcFlyAbsStep(p, want, full * env, dt, w2)) NcFlyStep(&g_wd, p, w2, level, out);
            else memcpy(out, p, sizeof out);
            moved = out[2] - p[2];
            if (moved > last + 1e-3f) slowOk = 0;
            if (tStop < 0.0f && moved <= 0.0f) tStop = (float)t * dt;
            last = moved;
            memcpy(p, out, sizeof p);
        }
        for (t = 0; t < hz; ++t) {  // then an enemy pushes her for 1 s, no stick: she never flies
            const float want[3] = { p[0] + 2.0f, p[1], p[2] };
            const float before = p[0];
            env = NcFlyEnvStep(env, NcFlyTarget(0.0f, 0.0f, 0.0f, dt), dt);
            if (NcFlyAbsStep(p, want, full * env, dt, w2)) pushOk = 0, NcFlyStep(&g_wd, p, w2, level, out), memcpy(p, out, sizeof p);
            if (p[0] != before) pushOk = 0;
        }
        _snprintf(m, sizeof m, "fly envelope at %d Hz: full in %.3f s (0.12 + a tick); let go: stopped in %.3f s (<= 0.15) over %.1f units "
                  "(< 150), never faster on the way; a push alone moves her %s", hz, tUp, tStop, p[2] - z0, pushOk ? "not at all" : "(FAIL)");
        Check(tUp > 0.0f && tUp <= NC_FLY_UP_S + dt + 1e-4f && tStop >= 0.0f && tStop <= 0.15f && p[2] - z0 < 150.0f && p[2] - z0 > 0.0f && slowOk && pushOk, m);
    }
}

// ---------------------------------------------------------------- Fly
static void TestFly(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    const float pitch = 0.5235988f;  // 30 degrees up
    const float fwd[3] = { 0.0f, -sinf(pitch), cosf(pitch) };  // heading +z, looking up (y grows downward)
    char m[200];
    int i;
    Level();
    for (i = 0; i < 4; ++i) {
        const int hz = kHz[i], n = hz;  // one second
        const float step = 300.0f / (float)hz;
        float p[3] = { 0, -200, -1000 }, out[3];
        int t;
        for (t = 0; t < n; ++t) {
            const float want[3] = { p[0], p[1], p[2] + step };
            NcFlyStep(&g_wd, p, want, fwd, out);
            memcpy(p, out, sizeof p);
        }
        _snprintf(m, sizeof m, "fly 1 s forward at %d Hz looking 30 deg up: rose %.1f (150), moved %.1f (259.8)", hz, -200.0f - p[1], p[2] + 1000.0f);
        Check(fabsf(-200.0f - p[1] - 150.0f) < 0.5f && fabsf(p[2] + 1000.0f - 259.8f) < 0.5f, m);
        for (t = 0; t < n; ++t) {
            const float want[3] = { p[0], p[1], p[2] - step };
            NcFlyStep(&g_wd, p, want, fwd, out);
            memcpy(p, out, sizeof p);
        }
        _snprintf(m, sizeof m, "fly 1 s back at %d Hz: back where she started (%.2f, %.2f)", hz, p[1], p[2]);
        Check(fabsf(p[1] + 200.0f) < 0.05f && fabsf(p[2] + 1000.0f) < 0.05f, m);
        for (t = 0; t < n; ++t) {
            const float want[3] = { p[0] + step, p[1], p[2] };
            NcFlyStep(&g_wd, p, want, fwd, out);
            memcpy(p, out, sizeof p);
        }
        _snprintf(m, sizeof m, "fly sideways at %d Hz: level (y %.2f), 300 across (%.1f)", hz, p[1], p[0]);
        Check(fabsf(p[1] + 200.0f) < 0.05f && fabsf(p[0] - 300.0f) < 0.1f, m);
    }
    {
        float p[3] = { 5, -300, 5 }, out[3];
        const float want[3] = { 5, -300, 5 };
        Check(!NcFlyStep(&g_wd, p, want, fwd, out) && out[0] == 5.0f && out[1] == -300.0f && out[2] == 5.0f, "fly: no input, she stays (no fall)");
    }
    {
        const float down[3] = { 0.0f, 1.0f, 0.0f };  // straight down: the pitch is held at about 84 degrees
        float p[3] = { 0, -500, 0 }, out[3];
        const float want[3] = { 0, -500, 10 };
        NcFlyStep(&g_wd, p, want, down, out);
        _snprintf(m, sizeof m, "fly looking straight down: sinks %.2f, forward %.2f (finite)", out[1] + 500.0f, out[2]);
        Check(out[1] > -500.0f && NcFinite3(out) && out[2] >= 0.0f, m);
    }
    {
        float p[3] = { 0, -990, 2990 }, out[3];
        const float want[3] = { 0, -990, 3010 };
        NcFlyStep(&g_wd, p, want, fwd, out);
        _snprintf(m, sizeof m, "fly: the room's box holds her in (%.1f, %.1f)", out[1], out[2]);
        Check(out[2] <= 3000.0f && out[1] >= -1000.0f, m);
    }
    {
        const float bad[3] = { NAN, 0, 1 };
        float p[3] = { 1, -2, 3 }, out[3];
        const float want[3] = { 1, -2, 13 };
        Check(!NcFlyStep(&g_wd, p, want, bad, out) && out[2] == 3.0f, "fly: a non-finite camera axis: she stays");
    }
}

// ---------------------------------------------------------------- the landing
static void TestLand(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[200];
    int i;
    Level();
    for (i = 0; i < 4; ++i) {
        const float dt = 1.0f / (float)kHz[i];
        NcLand l;
        NcWalk w;
        float p[3] = { -1500, -500, -1500 }, out[3];
        int t = 0, r;
        NcWalkReset(&w);
        r = NcLandBegin(&l, &g_wd, &w, p);
        while (NcLandStep(&l, &w, p, dt, out) && t < 1000) memcpy(p, out, sizeof p), ++t;
        memcpy(p, out, sizeof p);
        _snprintf(m, sizeof m, "landing from 500 up at %d Hz: on the floor (y %.2f) after %d ticks (%.3f s; 500 / %.0f = %.3f)", kHz[i], p[1], t + 1,
                  (t + 1) * dt, NC_LAND_SPEED, 500.0f / NC_LAND_SPEED);
        Check(r == 1 && p[1] == 0.0f && (t + 1) * dt <= 500.0f / NC_LAND_SPEED + dt + 1e-4f, m);
    }
    {   // inside the thick wall up high: back to the last floor point
        NcLand l;
        NcWalk w;
        float p[3] = { 800, -500, 0 }, out[3];
        NcWalkReset(&w);
        w.valid = 1, w.fx = 450, w.fy = 0, w.fz = 0;
        Check(NcLandBegin(&l, &g_wd, &w, p) == 2 && !NcLandStep(&l, &w, p, 0.016f, out) && out[0] == 450.0f && out[1] == 0.0f,
              "landing inside a wall: back to the last floor point");
    }
    {   // over the pit: back to the last floor point
        NcLand l;
        NcWalk w;
        float p[3] = { 2000, -300, 0 };
        NcWalkReset(&w);
        w.valid = 1, w.fx = 1400, w.fy = 0, w.fz = 0;
        Check(NcLandBegin(&l, &g_wd, &w, p) == 2, "landing over the pit: back to the last floor point");
        w.valid = 0;
        Check(NcLandBegin(&l, &g_wd, &w, p) == 3 && !l.active, "landing over the pit with no floor point known: she stays (3)");
    }
    {   // on the floor already
        NcLand l;
        NcWalk w;
        float p[3] = { -1500, 0, -1500 };
        NcWalkReset(&w);
        Check(NcLandBegin(&l, &g_wd, &w, p) == 0 && !l.active, "landing while on the floor: nothing to do");
    }
}

// ---------------------------------------------------------------- latch, catch-up, softlock watch
static void TestLatch(void)
{
    int latched = 0;
    Check(NcLatchStep(&latched, 1, 0, 0, 0) == 1 && latched == 1, "latch: On uses No Clip");
    Check(NcLatchStep(&latched, 0, 0, 0, 0) == 1 && latched == 1, "latch: turned off inside geometry: still active");
    Check(NcLatchStep(&latched, 0, 1, 0, 0) == 0 && latched == 1, "latch: suspended (Room 302, a scene): stock, latch kept");
    Check(NcLatchStep(&latched, 0, 0, 0, 1) == 0 && latched == 0, "latch: clear: dropped");
    latched = 1;
    Check(NcLatchStep(&latched, 0, 0, 1, 0) == 0 && latched == 0, "latch: a room change drops it");
}

static float WarpAfter(int hz, int thru, float dist, int *fired)
{
    NcWarp s = { 0, 0 };
    const float dt = 1.0f / (float)hz;
    int t;
    *fired = 0;
    for (t = 1; t <= hz * 10; ++t)
        if (NcWarpStep(&s, 1, 1, dist, thru, dt)) {
            *fired = 1;
            return (float)t * dt;
        }
    return 0.0f;
}
static void TestWarp(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[200];
    int i, f;
    for (i = 0; i < 4; ++i) {
        const int hz = kHz[i];
        const float dt = 1.0f / (float)hz;
        float t = WarpAfter(hz, 1, 300.0f, &f);
        _snprintf(m, sizeof m, "catch-up after a pass-through at %d Hz: 300 away, blocked: placed after %.3f s (%.1f)", hz, t, NC_WARP_WAIT);
        Check(f && t >= NC_WARP_WAIT - 1e-4f && t <= NC_WARP_WAIT + dt + 1e-4f, m);
        t = WarpAfter(hz, 0, 300.0f, &f);
        _snprintf(m, sizeof m, "no pass-through at %d Hz: 300 away is not placed", hz);
        Check(!f, m);
        t = WarpAfter(hz, 0, 700.0f, &f);
        _snprintf(m, sizeof m, "no pass-through at %d Hz: 700 away placed after %.3f s (%.1f)", hz, t, NC_WARP_WAIT_FAR);
        Check(f && t >= NC_WARP_WAIT_FAR - 1e-4f && t <= NC_WARP_WAIT_FAR + dt + 1e-4f, m);
        t = WarpAfter(hz, 1, 200.0f, &f);
        _snprintf(m, sizeof m, "at %d Hz: 200 away (under %.0f) is never placed", hz, NC_WARP_NEAR);
        Check(!f, m);
    }
    {
        NcWarp s = { 0, 0 };
        int t, n = 0;
        for (t = 0; t < 600; ++t) n += NcWarpStep(&s, 1, 1, 400.0f, 1, 1.0f / 60.0f);
        _snprintf(m, sizeof m, "catch-up: 10 s blocked = %d placements (every %.0f s; the %.0f s cool-down runs inside the wait: 4 .. 5)", n,
                  NC_WARP_WAIT, NC_WARP_COOL);
        Check(n >= 4 && n <= 5, m);
        s.behind = 1.9f;
        NcWarpStep(&s, 1, 0, 400.0f, 1, 1.0f / 60.0f);
        Check(s.behind == 0.0f, "catch-up: the line free again restarts the wait");
        s.behind = 1.9f;
        NcWarpStep(&s, 0, 1, 400.0f, 1, 1.0f / 60.0f);
        Check(s.behind == 0.0f, "catch-up: not eligible restarts the wait");
        s.behind = 0.0f, s.cool = 0.0f;
        Check(!NcWarpStep(&s, 1, 1, 400.0f, 1, 5.0f) && s.behind <= 0.1f + 1e-6f, "catch-up: a 5 s hitch counts as one 0.1 s tick");
    }
}

static void TestHeld(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[160];
    int i;
    for (i = 0; i < 4; ++i) {
        NcHeld h = { 0, 0 };
        const float dt = 1.0f / (float)kHz[i];
        int t, at = -1, n = 0;
        for (t = 1; t <= kHz[i] * 5; ++t)
            if (NcHeldStep(&h, 1, 0, dt)) at = t, ++n;
        _snprintf(m, sizeof m, "softlock watch at %d Hz: reported once (%d) after %.3f s", kHz[i], n, at * dt);
        Check(n == 1 && at * dt >= NC_HELD_ANOMALY - 1e-4f && at * dt <= NC_HELD_ANOMALY + dt + 1e-4f, m);
        NcHeldStep(&h, 1, 1, dt);
        Check(h.t == 0.0f && !h.reported, "softlock watch: moving resets it");
    }
}

// v3.34 NC334: the companion's foot IK carried to the placement spot, and the feet watch
static NcFootIk Foot(int mode, int step, float x, float y, float z)
{
    NcFootIk f;
    memset(&f, 0, sizeof f);
    f.target = &f;  // any non-NULL bone
    f.mode = mode, f.step = step;
    f.offset = -7.0f, f.unk04 = -3.0f, f.blend = 0.4f, f.roll = 0.5f;
    f.pos[0] = x, f.pos[1] = y, f.pos[2] = z, f.pos[3] = 1.0f;
    f.ground = y + 4.0f;
    return f;
}
static void TestIk(void)
{
    // the user's case: Eileen 1141 away at y 0 placed at 654, 0, 680 (flat floor)
    const float from[3] = { -400.0f, 0.0f, 1150.0f }, to[3] = { 654.0f, 0.0f, 680.0f };
    const float d[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
    const float dUp[3] = { to[0] - from[0], -448.0f, to[2] - from[2] };  // placed 448 higher (a floor up the stairs)
    static const int kMode[4] = { 0, 1, 2, 3 };
    char m[160];
    int i;
    for (i = 0; i < 4; ++i) {
        NcFootIk f = Foot(kMode[i], 1, from[0] + 12.0f, -30.0f, from[2] - 9.0f);
        const int rc = NcIkCarry(&f, from, d, 1, 0.0f);
        _snprintf(m, sizeof m, "foot IK mode %d: carried with her (rc %d), target %.1f, %.1f, %.1f beside her body, mode / step %d / %d, offset %.1f",
                  kMode[i], rc, f.pos[0], f.pos[1], f.pos[2], f.mode, f.step, f.offset);
        Check(rc == 1 && fabsf(f.pos[0] - (to[0] + 12.0f)) < 1e-3f && fabsf(f.pos[1] + 30.0f) < 1e-3f &&
              fabsf(f.pos[2] - (to[2] - 9.0f)) < 1e-3f && f.mode == 0 && f.step == 0 && fabsf(f.offset) < 1e-3f &&
              f.unk04 == 0.0f && f.pos[3] == 1.0f, m);
        Check(fabsf(f.ground - (-26.0f)) < 1e-3f && f.roll == 0.5f, "foot IK: the stance height moves with her, the roll is kept");
        Check(NcFootGap(f.pos, to, 1, 0.0f) < 1e-3f, "foot IK: the carried foot is on the floor at once (flat)");
    }
    {   // without the carry (v3.33): the planted foot stays where she was = 1141 away: counts as off the floor
        NcFootIk f = Foot(1, 1, from[0], -30.0f, from[2]);
        const float g = NcFootGap(f.pos, to, 1, 0.0f);
        _snprintf(m, sizeof m, "v3.33 (no carry): the planted foot is %.0f off (>= %.0f, her feet float)", g, NC_FEET_FAR);
        Check(g > 1000.0f, m);
    }
    {   // stairs: placed 448 higher; the floor under this foot is a tread 40 below the one under her body
        NcFootIk f = Foot(3, 1, from[0] + 10.0f, -30.0f, from[2]);
        const int rc = NcIkCarry(&f, from, dUp, 1, -448.0f + 40.0f);
        _snprintf(m, sizeof m, "stairs: the foot starts at the lower tread (offset %.1f, target %.1f, floor -408)", f.offset, f.pos[1] + f.offset);
        Check(rc == 1 && fabsf(f.pos[1] - (-478.0f)) < 1e-3f && fabsf(f.offset - 40.0f) < 1e-3f && fabsf(f.pos[1] + f.offset - (-408.0f - NC_ANKLE)) < 1e-3f, m);
        f = Foot(1, 1, from[0] + 10.0f, -30.0f, from[2]);
        NcIkCarry(&f, from, dUp, 1, -448.0f - 35.0f);
        Check(fabsf(f.offset + 35.0f) < 1e-3f, "stairs: a foot over the higher tread starts 35 up");
        f = Foot(1, 1, from[0] + 10.0f, -30.0f, from[2]);
        NcIkCarry(&f, from, dUp, 1, -448.0f + 90.0f);
        Check(f.offset == 0.0f && f.mode == 0 && f.step == 0, "stairs: a floor 90 off (past the 60 limit) starts the foot at the clip (offset 0)");
        f = Foot(1, 1, from[0] + 10.0f, -30.0f, from[2]);
        NcIkCarry(&f, from, dUp, 0, 0.0f);
        Check(f.offset == 0.0f && fabsf(f.pos[1] + 478.0f) < 1e-3f, "no floor under the foot: offset 0, carried");
    }
    {   // not hers / not usable: left alone
        NcFootIk f = Foot(1, 1, from[0] + 500.0f, -30.0f, from[2]), g;
        g = f;
        Check(NcIkCarry(&f, from, d, 1, 0.0f) == -1 && !memcmp(&f, &g, sizeof f), "foot IK 500 from her old spot (another body's): left alone");
        f = Foot(1, 1, from[0], -30.0f, from[2]);
        f.target = 0;
        g = f;
        Check(NcIkCarry(&f, from, d, 1, 0.0f) == 0 && !memcmp(&f, &g, sizeof f), "no IK target bone: nothing done");
        f = Foot(1, 1, from[0], -30.0f, from[2]);
        f.pos[1] = NAN;
        Check(NcIkCarry(&f, from, d, 1, 0.0f) == -1 && f.mode == 1, "foot IK target not a number: left alone");
        f = Foot(1, 1, from[0], -30.0f, from[2]);
        {
            const float dn[3] = { INFINITY, 0.0f, 0.0f };
            Check(NcIkCarry(&f, from, dn, 1, 0.0f) == -1 && f.mode == 1 && f.pos[0] == from[0], "a move that is not a number: left alone");
        }
    }
    Check(sizeof(NcFootIk) == 0x50 && offsetof(NcFootIk, pos) == 0x20 && offsetof(NcFootIk, ground) == 0x44 &&
          offsetof(NcFootIk, target) == 0x14 && offsetof(NcFootIk, step) == 0x0C, "NcFootIk = EileenFootIk's layout (0x50; pos +0x20, ground +0x44)");
    {   // the gap
        const float body[3] = { 0.0f, 0.0f, 0.0f }, foot[3] = { 10.0f, -30.0f, 5.0f }, up[3] = { 10.0f, -55.0f, 5.0f };
        const float far[3] = { 200.0f, -30.0f, 0.0f };
        Check(fabsf(NcFootGap(foot, body, 1, 0.0f)) < 1e-4f && fabsf(NcFootGap(up, body, 1, 0.0f) - 25.0f) < 1e-4f &&
              NcFootGap(foot, body, 0, 0.0f) < 0.0f && fabsf(NcFootGap(far, body, 1, 0.0f) - 200.0f) < 1e-3f,
              "foot gap: planted 0, 25 up 25, no floor unknown, 200 away (xz) 200");
    }
}
// The game's companion foot IK, ported from SH4Decomp src/eileen/eileen_43d120.c (0x43E450 mode choice by the frame of
// the walk clip, 0x43D290 stance, 0x43D120 swing, 0x43D3E0 the other clips; per-tick constants as in the exe), on a
// synthetic walk: her root walks +x at 6 per tick, the clip's ankle targets swing 40 forward / back and lift 20 in the
// swing frames, the solver reports out of reach beyond 110 from the hip. A placement moves the root by d between two of
// her ticks (the player's update runs first). Returns the ticks after it in which a foot target is more than 40 from
// where the clip puts it at her new place plus the floor offset (the stretch the user saw as floating legs).
static float g_ikFloor;  // the floor's y everywhere
static float Approach(float c, float t, float s) { return fabsf(c - t) <= s ? t : c > t ? c - s : c + s; }
static int IkStance(int k, int fr) { return k ? (fr >= 6 && fr <= 16) : !(fr < 19 && fr > 4); }
static void ClipTarget(int k, int fr, const float *root, float *out)
{
    // stance: the ankle stays put in the world (it slides back 6 per frame against the root); swing: forward again
    const int s0 = k ? 6 : 19, nSt = k ? 11 : 10, nSw = 24 - nSt;
    const int i = (fr - s0 + 24) % 24;
    const float back = 6.0f * (float)(nSt - 1);
    const float rel = i < nSt ? 30.0f - 6.0f * (float)i : 30.0f - back + (float)(i - nSt + 1) * back / (float)nSw;
    out[0] = root[0] + rel;
    out[1] = root[1] - NC_ANKLE - (IkStance(k, fr) ? 0.0f : 20.0f);
    out[2] = root[2] + (k ? 12.0f : -12.0f);
    out[3] = 1.0f;
}
static int IkSimRun(int carry, float dy, float floorNew, int *before)
{
    NcFootIk ft[2];
    float root[3] = { 0.0f, 0.0f, 0.0f };
    int t, k, i, fr = 0, bad = 0, result[2] = { 0, 0 };
    *before = 0;
    memset(ft, 0, sizeof ft);
    g_ikFloor = 0.0f;
    for (k = 0; k < 2; ++k) ft[k].target = &ft[k], ClipTarget(k, 0, root, ft[k].pos);
    for (t = 0; t < 120; ++t) {
        float clip[4];
        if (t == 60) {  // the placement: 1141 away (and dy up / down), between two of her ticks
            const float from[3] = { root[0], root[1], root[2] }, d[3] = { 1100.0f, dy, -300.0f };
            root[0] += d[0], root[1] += d[1], root[2] += d[2];
            g_ikFloor = floorNew;
            if (carry)
                for (k = 0; k < 2; ++k) NcIkCarry(&ft[k], from, d, 1, g_ikFloor);
        }
        root[0] += 6.0f;  // her walk (EileenWalkIntegrate), then the clip and the IK (EileenAnimIkUpdate)
        fr = (fr + 1) % 24;
        for (k = 0; k < 2; ++k) {
            NcFootIk *f = &ft[k];
            const int stance = IkStance(k, fr);
            float hip[3], dx, dz, dyh;
            if (stance) { if (f->mode != 1 && f->mode != 2) f->mode = 1, f->step = 0; }
            else if (f->mode != 3) f->mode = 3, f->step = 0;
            ClipTarget(k, fr, root, clip);
            if (f->mode == 1) {
                if (f->step == 0) {
                    memcpy(f->pos, clip, sizeof clip);
                    f->pos[1] += f->offset;
                    f->ground = fabsf(g_ikFloor - NC_ANKLE - f->pos[1]) > 100.0f ? root[1] - NC_ANKLE : g_ikFloor - NC_ANKLE;
                    f->step = 1;
                } else if (result[k] == 2) {
                    f->mode = 2, f->step = 0;
                } else {
                    f->pos[1] = Approach(f->pos[1], f->ground, 4.0f);
                }
            } else if (f->mode == 3) {
                float h;
                if (f->step == 0) f->blend = 0.0f, f->step = 1;
                f->blend = f->blend + 0.3f > 1.0f ? 1.0f : f->blend + 0.3f;
                h = g_ikFloor - 34.0f - clip[1];
                f->offset = Approach(f->offset, h < 0.0f ? h : 0.0f, 4.0f);
                for (i = 0; i < 3; ++i) f->pos[i] = (clip[i] + (i == 1 ? f->offset : 0.0f) - f->pos[i]) * f->blend + f->pos[i];
            } else {
                float h;
                if (f->step == 0) f->offset = 0.0f, f->blend = 0.0f, f->step = 1;
                f->blend = f->blend + 0.2f > 1.0f ? 1.0f : f->blend + 0.2f;
                h = g_ikFloor - NC_ANKLE - clip[1];
                if (fabsf(h) > 100.0f) h = 0.0f;
                if (h > 0.0f) h = 0.0f;
                f->offset = Approach(f->offset, h, 4.0f);
                for (i = 0; i < 3; ++i) f->pos[i] = (clip[i] + (i == 1 ? f->offset : 0.0f) - f->pos[i]) * f->blend + f->pos[i];
            }
            hip[0] = root[0], hip[1] = root[1] - 90.0f, hip[2] = root[2];
            dx = f->pos[0] - hip[0], dyh = f->pos[1] - hip[1], dz = f->pos[2] - hip[2];
            result[k] = dx * dx + dyh * dyh + dz * dz > 110.0f * 110.0f ? 2 : 1;  // the solver, seen next tick
            if (t >= 20) {  // (after the first steps from the start pose)
                const float ex = f->pos[0] - clip[0], ez = f->pos[2] - clip[2];
                const float ey = f->pos[1] - (stance ? g_ikFloor - NC_ANKLE : clip[1]);
                if (ex * ex + ey * ey + ez * ez > 40.0f * 40.0f) {
                    if (t >= 60) ++bad;
                    else ++*before;
                    break;
                }
            }
        }
    }
    return bad;
}
static void TestIkSim(void)
{
    static const float kDy[3] = { 0.0f, -448.0f, 32.0f };
    static const char *const kWhat[3] = { "flat floor", "placed 448 higher (up the stairs)", "placed 32 lower (a step down)" };
    char m[200];
    int c, a, b, a0, b0;
    for (c = 0; c < 3; ++c) {
        a = IkSimRun(0, kDy[c], kDy[c], &a0), b = IkSimRun(1, kDy[c], kDy[c], &b0);
        _snprintf(m, sizeof m, "game foot IK port, %s: v3.33 (no carry) %d tick(s) of a foot target > 40 off (stretched legs), v3.34 %d "
                  "(before the placement: %d / %d)", kWhat[c], a, b, a0, b0);
        Check(a > 0 && b == 0 && a0 == 0 && b0 == 0, m);
    }
}
static int FeetRun(int hz, float early, float late, float *at)
{
    NcFeetWatch w;
    const float dt = 1.0f / (float)hz;
    int t, r = 0, n = 0;
    NcFeetBegin(&w);
    for (t = 1; t <= hz * 3; ++t) {
        const float s = t * dt;
        // a walking foot: lifted (gap 30) 2 ticks in every 6; `early` before 0.4 s, `late` after it
        const float g = (t % 6) < 2 ? 30.0f : s < 0.4f ? early : late;
        const int k = NcFeetStep(&w, g, dt);
        if (k) r = k, *at = s, ++n;
    }
    return n == 1 ? r : -n;
}
static void TestFeet(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    char m[160];
    int i;
    for (i = 0; i < 4; ++i) {
        float at = 0.0f;
        int r = FeetRun(kHz[i], 2.0f, 2.0f, &at);
        _snprintf(m, sizeof m, "feet watch at %d Hz: on the floor -> fine (%d), judged once at %.3f s", kHz[i], r, at);
        Check(r == 1 && at >= NC_FEET_CHECK + NC_FEET_SPAN - 1e-3f && at <= NC_FEET_CHECK + NC_FEET_SPAN + 1.0f / kHz[i] + 1e-3f, m);
        r = FeetRun(kHz[i], 2.0f, 25.0f, &at);
        _snprintf(m, sizeof m, "feet watch at %d Hz: 25 off from 0.4 s on -> anomaly (%d)", kHz[i], r);
        Check(r == 2, m);
        r = FeetRun(kHz[i], 600.0f, 3.0f, &at);
        _snprintf(m, sizeof m, "feet watch at %d Hz: far off at first, settled by 0.4 s -> fine (%d)", kHz[i], r);
        Check(r == 1, m);
        r = FeetRun(kHz[i], -1.0f, -1.0f, &at);
        _snprintf(m, sizeof m, "feet watch at %d Hz: lifted foot only (30) otherwise unknown -> anomaly (%d)", kHz[i], r);
        Check(r == 2, m);
    }
    {
        NcFeetWatch w;
        int t, r = 0;
        NcFeetBegin(&w);
        for (t = 0; t < 60 && !r; ++t) r = NcFeetStep(&w, -1.0f, 1.0f / 60.0f);
        Check(r == 3, "feet watch: no floor known at all -> not judged (3)");
        NcFeetBegin(&w);
        Check(NcFeetStep(&w, 0.0f, 5.0f) == 0 && w.t <= 0.1f + 1e-6f, "feet watch: a 5 s hitch counts as one 0.1 s tick");
        NcFeetBegin(&w);
        r = 0;
        for (t = 0; t < 60 && !r; ++t) r = NcFeetStep(&w, NAN, 1.0f / 60.0f);
        Check(r == 3, "feet watch: a gap that is not a number is ignored");
        Check(NcFeetStep(&w, 0.0f, 1.0f / 60.0f) == 0, "feet watch: once judged it stays quiet");
    }
}

int main(void)
{
    TestWalks();
    TestFly();
    TestFlySpeed();  // v3.37 MN337
    TestLand();
    TestLatch();
    TestWarp();
    TestHeld();
    TestIk();
    TestIkSim();
    TestFeet();
    Check(fabsf(NcSegDist2XZ(0, 5, -10, 0, 10, 0) - 25.0f) < 1e-4f && fabsf(NcSegDist2XZ(20, 0, -10, 0, 10, 0) - 100.0f) < 1e-4f,
          "segment distance (middle, past an end)");
    printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
