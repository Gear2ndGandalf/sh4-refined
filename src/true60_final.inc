// ================================================================ v3.56 FR356: the final room's per-tick leftovers
// USER_FEEDBACK_v355 issue 7: "In the final boss room ... the strange metal contraption used to defeat Eileen in the blood
// pool spins faster the higher our frame rate setting is. Super fast at 180fps." Stage 9 room 2's room script (0x655420
// object list, the kind-3 object at the room's origin: init 0x4E4B10, update 0x4E4C50, model 0xF005F18F) turns five bones
// of the contraption every tick: bone 0 by -dt (0x55AE60: rate-correct, 1 rad a second at every rate), bones 2 / 4 / 6 / 8
// by angle += 0.0785 x sin(phase), the phase itself advancing by dt (rate-correct) but the 0.0785 added per TICK
// (`fmul qword ptr [5BD3B8h]` at 0x4E4CCE): the parts' swing is N / 30 times as wide in the same time, 6x at 180. The v3.0
// research row (research\framerate\v5\true60_companions.json, 0x4E4CCE, kind float_scale, operand ptr_f64) was never
// installed: tools\gen_true60.py turns only f32 / i8 / i16 / i32 / ptr_f32 rows into table rows and listed this double as
// "by hand". Here: its disp32 points at a double of this dll, 0.0785 x 30 / N (the stock double at 30).
// The same room's audit (BUILD_NOTES: the Walter final form 0x4A4E20.., the Conjoined Bodies 0x4A93E0.., the spears
// 0x541FB0.., the ambient motes 0x4378C0, the rings 0x431220, Eileen's walk 0x44B110 (table rows 0x44B214 / 0x44B292)):
// one more per-tick step, Henry's step onto a spear spot (0x5424F0, mode 1 of the spear handler: draw_pos += (spot - pos)
// x 0.2 a tick, x at 0x542529 and z at 0x54253F through the shared float 0x5B7460, read by 103 functions: never changed in place): 1 - 0.8^(30 / N).
// Like the table's RT_PTR rows: installed once with the tail (TailInstall, FrameMode 0, every site byte-checked, all or
// none, a failed write puts the written ones back), the values set at every rate switch (TailApply) and around a gated
// entity update (TailWrite(-1) = the stock values). Listed in SH4WM_Patches by Poke.
#include "t60final_sim.h"  // T60F_SCALE / T60F_APPROACH, T60fValue (test\fr356_test.c)
static const struct { DWORD va; BYTE orig[6]; int dbl, kind; const char *what; } kT60Final[] = {
    { 0x4E4CCE, { 0xDC, 0x0D, 0xB8, 0xD3, 0x5B, 0x00 }, 1, T60F_SCALE, "the final room's contraption: its parts' swing step a tick (0.0785)" },
    { 0x542529, { 0xD8, 0x0D, 0x60, 0x74, 0x5B, 0x00 }, 0, T60F_APPROACH, "the final room's spear spot: Henry's step onto it, x (0.2 of the way a tick)" },
    { 0x54253F, { 0xD8, 0x0D, 0x60, 0x74, 0x5B, 0x00 }, 0, T60F_APPROACH, "the final room's spear spot: Henry's step onto it, z (0.2 of the way a tick)" },
};
#define NT60F ((int)(sizeof kT60Final / sizeof kT60Final[0]))
static double g_t60fD[NT60F], g_t60fD30[NT60F];  // the double the instruction reads now, the stock one (dbl rows)
static float g_t60fF[NT60F], g_t60fF30[NT60F];   // the same for float rows
static double g_t60fV[3][NT60F];                 // the values at 60 / 120 / 180 (made once at install)
static int g_t60fIn;

// ri = RateIndex (0 / 1 / 2 = 60 / 120 / 180; -1 = 30 or a rate not in the tables: the stock values). Plain stores: it also
// runs around every gated entity update (TailWrite).
static void T60FinalRate(int ri)
{
    int i;
    if (!g_t60fIn) return;
    for (i = 0; i < NT60F; ++i) {
        if (kT60Final[i].dbl) g_t60fD[i] = ri < 0 || ri > 2 ? g_t60fD30[i] : g_t60fV[ri][i];  // at 30 the stock bits themselves
        else g_t60fF[i] = ri < 0 || ri > 2 ? g_t60fF30[i] : (float)g_t60fV[ri][i];
    }
}

static void T60FinalInstall(void)
{
    int i, bad = 0;
    for (i = 0; i < NT60F; ++i)
        if (memcmp((const void *)(ULONG_PTR)kT60Final[i].va, kT60Final[i].orig, 6)) {
            ++bad;
            Log("true 60: %08lX (%s) is not stock: none of the %d final-room sites is used", kT60Final[i].va, kT60Final[i].what, NT60F);
        }
    if (bad) return;
    for (i = 0; i < NT60F; ++i) {  // the stock constants, read through the instructions' own operands
        const DWORD from = *(const DWORD *)(ULONG_PTR)(kT60Final[i].va + 2);
        int r;
        if (kT60Final[i].dbl) g_t60fD30[i] = g_t60fD[i] = *(const double *)(ULONG_PTR)from;
        else g_t60fF30[i] = g_t60fF[i] = *(const float *)(ULONG_PTR)from;
        for (r = 0; r < 3; ++r)
            g_t60fV[r][i] = T60fValue(kT60Final[i].dbl ? g_t60fD30[i] : (double)g_t60fF30[i], kT60Final[i].kind, r);
    }
    for (i = 0; i < NT60F; ++i) {
        const DWORD to = kT60Final[i].dbl ? (DWORD)(ULONG_PTR)&g_t60fD[i] : (DWORD)(ULONG_PTR)&g_t60fF[i];
        if (!Poke(kT60Final[i].va + 2, &to, 4)) {
            int j;
            for (j = 0; j < i; ++j) Poke(kT60Final[j].va + 2, kT60Final[j].orig + 2, 4);
            Log("true 60: %08lX could not be written: the final-room sites are put back (stock 30 Hz steps)", kT60Final[i].va);
            return;
        }
    }
    g_t60fIn = 1;
    Log("true 60: %d/%d final-room sites follow the rate (stage 9 room 2: the contraption's swing %.4f a tick at 30, the spear "
        "spot's step %.2f)", NT60F, NT60F, g_t60fD30[0], g_t60fF30[1]);
}
