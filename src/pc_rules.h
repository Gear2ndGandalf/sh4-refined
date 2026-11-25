// ================================================================ PLAYER PACK: the pure rules of the five Player-page cheats
// staging/special_mods/player_cheats/p1_combat/PLAN.md. Pure C (no game memory): included by playercheats.inc and by the
// native test test/pc_rules_test.c. Every rule is per event (a shot, a hit, a grab start) or a factor on the game's own
// dt, so none depends on the tick rate (30 / 60 / 120 / 180 Hz).
#ifndef PC_RULES_H
#define PC_RULES_H
#include <string.h>  // v3.43 QC343: _stricmp (PcQcAlias)

enum { PC_AMMO, PC_PACT, PC_GRAB, PC_QC, PC_STEAL, PC_COUNT };  // v3.43 QC343: PC_QC (Quick Charge) was PC_TIRE (Tireless)
#define PC_VMAX 4  // v3.43 QC343: 3 -> 4 (Quick Charge's four values; only that row has a 4th)

// Ammo Saver: 0 Normal, 1 Saver (every other shot is free), 2 Endless. *shots counts this gun's shots under Saver.
// Returns 1 when the shot takes its round (the stock call runs), 0 when it is free.
static int PcAmmoRule(int mode, unsigned *shots)
{
    if (mode == 2) return 0;
    if (mode == 1 && shots) return (*shots)++ % 2 == 0;  // shots 1, 3, 5 ... take a round; 2, 4, 6 ... are free
    return 1;
}
// the guns it covers: the pistol (0x10, silver bullets too) and Richard's revolver (0x11); Playable Eileen's SMG and
// any other id stay stock
static int PcAmmoGun(int item) { return item == 0x10 ? 0 : item == 0x11 ? 1 : -1; }

// Dark Pact: 0 Off, 1 Pact (x1.5 both ways), 2 Blood (x2 both ways)
static float PcPactK(int v) { return v == 1 ? 1.5f : v == 2 ? 2.0f : 1.0f; }

// Grab Breaker: 0 Stock, 1 Quick (half the hold), 2 Instant (the hold starts at PC_GRAB_INSTANT_S at most). The hold
// loop takes dt off every tick (and more for stick shaking) and releases her below 0, so the start is in seconds at any
// tick rate. Instant keeps a tenth of a second (3 ticks at 30 Hz) rather than 0, so the grab's start (her grabbed
// state 0x536E20, the monster's own grab state) has run before she is let go [I: a release on the very first tick is
// never reached in the stock game].
#define PC_GRAB_INSTANT_S 0.1f
static float PcGrabK(int v) { return v == 1 ? 0.5f : 1.0f; }
static float PcGrabStart(float hold, int v)
{
    if (!(hold == hold)) return hold;  // (a NaN hold stays the game's)
    if (v == 2) return hold < PC_GRAB_INSTANT_S ? hold : PC_GRAB_INSTANT_S;
    return hold * PcGrabK(v);
}

// v3.43 QC343: Quick Charge (replaces v3.42's Tireless): 0 Off, 1 1.5x, 2 2x, 3 3x. The game's charge rate
// PlayerItemGetChargeRate 0x544D40 returns dt x 100 / the item's charge seconds (fld time; call 55AE60; fmul [5B7E0C]
// (100.0); fdiv time), added to the charge P+0x21C every tick the attack is held in the ready stance (six callers, all
// `fadd [10A113C]`); full at 100. The patched fmul uses 100 x k, so a full charge takes charge seconds / k at any tick
// rate (the rate is per second: dt). PcQcMul100 is also SH4R_QuickCharge()'s value (the factor x 100).
static int PcQcMul100(int v) { return v == 1 ? 150 : v == 2 ? 200 : v == 3 ? 300 : 100; }
static float PcQcRate(float dt, float chargeSeconds, int v)  // the patched 0x544D69..0x544D74, in its order
{
    float r = dt * (float)PcQcMul100(v);
    return r / chargeSeconds;
}
// the ini's spellings: Off / 1.5x / 2x / 3x (also x1.5 / 1.5 / 150 ...); -1 = unknown (the caller reads Off)
static int PcQcAlias(const char *s)
{
    static const char *const kAl[4][4] = { { "Off", "0", "x1", "100" }, { "1.5x", "x1.5", "1.5", "150" },
                                           { "2x", "x2", "2", "200" }, { "3x", "x3", "3", "300" } };
    int v, a;
    if (!s) return -1;
    for (v = 0; v < 4; ++v)
        for (a = 0; a < 4; ++a)
            if (!_stricmp(s, kAl[v][a])) return v;
    return -1;
}

// Life Steal: 0 Off, 1 5 %, 2 10 % of the damage dealt to a monster, counted only up to the HP it had (a corpse or a
// one-hit kill heals no more than the monster's life), at most 10 % of her max health a hit, never past her max
static float PcStealPct(int v) { return v == 1 ? 0.05f : v == 2 ? 0.10f : 0.0f; }
static float PcStealHeal(int v, float dmg, float targetHp, float herHp, float herMax)
{
    float d, h, cap;
    const float pct = PcStealPct(v);
    if (!(pct > 0.0f) || !(dmg > 0.0f) || !(targetHp > 0.0f) || !(herHp > 0.0f) || !(herMax > 0.0f) || !(herHp < herMax))
        return 0.0f;  // (NaNs fail every test)
    d = dmg < targetHp ? dmg : targetHp;
    h = d * pct;
    cap = herMax * 0.10f;
    if (h > cap) h = cap;
    if (h > herMax - herHp) h = herMax - herHp;
    return h > 0.0f ? h : 0.0f;
}

#endif
