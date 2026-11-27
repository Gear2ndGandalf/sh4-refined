// v3.56 FR356: the values of the final room's per-tick sites (true60_final.inc) at a rate index, pure (test\fr356_test.c).
// ri = RateIndex: 0 / 1 / 2 = 60 / 120 / 180, -1 = 30 (or a rate not in the tables): the stock value itself.
//  * T60F_SCALE: x += c a tick -> c x s (s = 30 / N).
//  * T60F_APPROACH: x += (t - x) x a a tick -> 1 - (1 - a)^s.
#ifndef T60FINAL_SIM_H
#define T60FINAL_SIM_H
#include <math.h>

enum { T60F_SCALE, T60F_APPROACH };

static double T60fS(int ri) { return ri == 0 ? 0.5 : ri == 1 ? 0.25 : ri == 2 ? 1.0 / 6.0 : 1.0; }
static double T60fValue(double base, int kind, int ri)
{
    const double s = T60fS(ri);
    if (ri < 0 || ri > 2) return base;
    return kind == T60F_SCALE ? base * s : 1.0 - pow(1.0 - base, s);
}

#endif
