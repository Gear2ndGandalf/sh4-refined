// ================================================================ HUD BARS (p2_hud candidate, 2026-11-17)
// Enemy Health Bars (Option 5 > Enemies) and the Eileen Curse Meter (Option 5 > Player). Plan, evidence and the live
// checklist: staging/special_mods/player_cheats/p2_hud/PLAN.md. Included by sh4windowmod.c after
// deathpresence_draw.inc (it uses screenfx.inc's FxSave / FxRestore / FxBase, GvXform / GvScreen and g_gvM, and
// gameplay.inc's lock, ini and row helpers). The pure state code (fades, trail, down timer, curse stages) is
// hudbars_sim.h (test\hudbars_sim_test.c). GOG v7 VAs. Draw only: no exe byte is patched.
//
// Both draw from Refined's HUD hook (items.inc SparkleFrame, the HUD call 0x4EFBFD), after the Screen Effects scene
// filter and before the game's own HUD 0x4F4A20, the message window and the text flush 0x4EFC7B, so they are never
// filtered and the game's HUD and text draw over them. Only in gameplay (FxGameplay, phase [0xFD5C20] == 0) while the
// game shows its HUD (the HUD gate byte [0x1079C30]: closed in pause, events, pickups, the item close-up) and never in
// the frame the pause picture is taken from ([0x12E3E2C] == 1), so a frozen overlay picture holds no bar.
//
// Enemy Health Bars: Off / Locked Target / All Near. The enemy list EnemyListNext 0x45E760 (enemy_util.h), work
// [task+0x408]: +0 position (Y down), +0x20 type, +0x28 the ghost block (down countdown +0x98, seconds), +0x30 model,
// +0x34 HP block (+0 hp, +4 max: EnemyHp, enemy_chara.h), +0x3C class (9 down, 0x0A down at HP 0, 0x0B dead, >= 0x10
// scripted). The lock-on target is the player block's +0x23C (gPlayer 0x10A0F20, player.h): [0x10A115C]. A bar shows
// over a monster that is on screen and not behind a wall (the camera eye [0xFD5AC0] row 3 to the bar through the
// camera's collision query 0x561CD0, as Item Shine) when it is the locked target, lost health in the last 4 s, or (All
// Near) is within 1500 units; it fades in over 0.2 s and out over 0.8 s. The part just lost stays pale for 0.45 s and
// then drains. v3.30 HB330 (RE_GHOST_HEALTH.md): a ghost's bar is always its HP (v3.29 drew the down countdown there:
// empty on a stumble, full on a real down), with a stagger line (the gauge, hp+0x0C / +0x10: empty = it falls), a rise
// preview while down (the get-up heal: 50 % at HP 0, Andrew 40 %, else max / (count + 4); none with an aura on it) and
// a second bar, the revive countdown, while down with a running countdown (class 0x0A); pinned by the Sword of Obedience
// (class 0x0B, types 0x0B..0x0F, or the mailbox 0xFD0BC0) that bar is sealed: two steel halves and a cross. A ghost
// seen when it went down keeps its bars up for the whole down. Bosses (Walter
// / the Conjoined Bodies type 0x10, the Wall boss type 5 placement mode 8) get a wider bar. Never types 1, 4, 0x12+.
// Anchor: the highest bone of the enemy's model ([model+0xC0] matrices, [model+0xE0] skeleton, +0x14 bone count; the
// same reads as Death Presence) plus 40 units, smoothed; a per-type height when the bones are not this frame's.
//
// Curse Meter: Off / On / Companion. Companion = companion Eileen's shown curse (EileenWork +0x0C, 0..1, the value
// every reader of the game uses; eileen.h, 0x43E930), divided at the game's own tiers 0.25 / 0.5 / 0.7 (EileenCurseTier
// in 0x43E930 / 0x43E980), with a small mark at 0.4 (live curse > 0.4 changes Apartment World's second visit, 0x44C620).
// Her task [0xFCE6B4] (0x44ADE0 in play, 0x44B110 final boss, 0x44B3D0 idle-only); in the party [0xFD5BC4] but not
// spawned or in another room: the saved curse rec+0x20 = [0xFCE810], drawn dimmed. On = Playable Eileen's own curse
// meter (export SH4PE_Curse in dsound.dll, 0..100) when that mod is loaded, divided at its [Curse] FitsFrom (fits
// start), CursedIdleFrom (her possessed pose) and full (100, latched); else the companion's. The stage is named under the
// meter for 3.5 s when it changes or the meter appears; the meter is bright for 3.5 s after a change, then rests dimmer.
// v3.30 HB330: the meter is a ring of thorns around a sigil (DESIGN.md design R): a stage crossing swells a ring out,
// Possessed beats like a heart.
// v3.32 DHS332 (staging/v332_feedback/w3_drops_hud_shine/DESIGN.md; the user: "give NPC Eileen her own curse meter"):
// a fourth value, Both (added at the end; the ini key stays CurseMeter): with Playable Eileen loaded, the player's ring
// (PE's own curse) plus a second, smaller ring left of it for companion Eileen (her game curse w+0x0C, tiers 0.25 /
// 0.5 / 0.7, the 0.4 mark), labelled EILEEN in a small stroke font; it fades in and out with her (HbCompanion) and
// has its own wake, pulse and beat. Without Playable Eileen, Both = On. Both's texts are at the end of the HB block.
// v3.32 MERGE (the user's choice after W3): while Playable Eileen locks her curse ([Ghost] LockCurse=1 and the curse held
// at exactly 0) the small ring is grey and dim with a small LOCKED tag under EILEEN (hudbars_sim.h HbRing2Locked).
// v3.35 CM335 (staging/v335_feedback/w3_curse/PLAN.md, DESIGN.md; the user's choice): companion Eileen's ring (the main
// ring when it shows her, Both's small ring) gets a faint notch where her curse is heading: the target w+0x10 while her
// task is alive, else 0x43E790's formula over her record 0xFCE7F0 (hudbars_sim.h HbCurseTarget, read only); while she is
// left behind (party mode [0xFD5BC4] = 2, no task) a colder mark and a thin arc show her target at the next spawn (one
// more door: the pending danger +0x2D joins the total). A small procedural "left alone" glyph (her figure and claw marks,
// one more for danger 2) shows left of her ring while she stayed behind in a room with monsters (+0x2D > 0) and is not
// with the player. Playable Eileen's LockCurse holds the target at 0: no notch. The main ring's stage name is now drawn
// in the small ring's stroke font, compact and close under the ring (it was the game's font 1 at canvas y 63).
// v3.37 MN337 (staging/v337_feedback/w1_menu/PLAN.md; the user's v3.36.1 QA): the Curse Meter row moved to Option 5's top
// level (under No Clip; HbRowChanged asks for page 4); its value On is shown as "Player" (the ini still writes "On", reads
// both); without Playable Eileen loaded (g_peLoaded, checked again as Options opens) Player and Both are grey (RowValueC
// asks HbValueIdGreyed) and Left / Right step over them (hudbars_sim.h HbCmStepFrom), a saved one shows the companion's
// curse meanwhile; Playable Eileen's Haunted stage starts at its [Curse] FitsFrom whether Fits is on or not, Cursed at
// CursedIdleFrom whether CursedIdle is on or not (HbPeStageMarks); the first stage is named Clear (was Untouched).
// v3.38 HB338 (staging/v338_feedback/w1_hud/PLAN.md; USER_FEEDBACK_v337 items 1-3, 17): both rings read on black (a
// stronger dark plate, bone hairlines outside and inside the band, a dusky empty band, a lifted fill, brighter unlit
// thorns); companion Eileen's notch, next-spawn mark and left-alone glyph are not dimmed by away and never under 0.85
// (HbAlertQ), the glyph's claws brighter, wider, shadowed; away dims her ring to 0.7 (was 0.5), the small ring rests at
// 0.70 (was 0.55), the main ring too (the user, 2026-11-21). PLAYER under the player's own ring (EILEEN's size / colour / strength / shadow; the font gained a Y),
// EILEEN under the main ring when it shows her; the stage name moved under the name. Hide Gauge (Option 3, [0x1091AD9]) on: the meters fade with the health gauge (its fade
// [0x653B8C] / 0.7) or their own 5 s + 1 s timer (HbHideStep), up while Possessed, put to sleep when the HUD is hidden
// (HudDisable 0x4F5090 -> 0x50ED90, as the gauge); every element and label x that k. Off: always shown, as before.
// v3.41 CM341 (staging/v341_feedback/w5_curse/PLAN.md; the contract SH4PlayableEileen/staging/s58_offsound/
// REFINED_HANDOVER.md): Playable Eileen v1.23.1+ exports SH4PE_DrawsPlayerCurse() = 1 and draws the player's curse ring
// itself (its [Curse] settings, its pause menu). Then Refined never draws a player ring: Player = nothing, Both = companion
// Eileen's small ring only, Companion = her ring as the main ring while PE's ring cannot show (PE Off, or its Meter=0),
// else her small ring (never on top of PE's). The small ring follows PE's ring (SH4PE_Curse version 2: ringX / ringY /
// ringR at +0x40; hudbars_sim.h HbRing2Place). Options: Player grey, Both / Player re-described; the ini keeps the value.
// Without the export (PE up to v1.23.0, or no PE) everything is as before.
// v3.42 SM342 (USER_FEEDBACK_v341 item 8; hudbars_sim.h HB_CMV_*): the row is Off / On, On = companion Eileen's ring only
// (v3.41's Companion: her main ring place while PE's ring cannot show, else the small ring beside PE's). Refined never
// draws the player's ring again, whatever Playable Eileen version (said once at the start). The ini writes "Companion"
// for On (every older build reads it as her ring); its old On / Player read as Off, Both / Companion as On (HbCmvParse).
// No greyed values any more (HbValueIdGreyed returns 0); the v3.37 / v3.41 texts of Player / Both stay allocated, unused.
// v3.52 HB352 (staging/v352_feedback/w2_hud/PLAN.md; USER_FEEDBACK_v351 D1, D2): a bar only for an enemy the game draws
// and that has a place in the room. D2's bars over the player were arriving victims' ghosts: state 0x0D / 0x0E, not drawn,
// sliding through the walls onto her while the game looks for a wall spot to bring them in (none in a tiny room: the
// elevators, the conductor's cab); no bar for the states 0, 0x0D..0x0F or a position at the origin / parked away
// (hudbars_sim.h HbRefuseWhy; verbose lines "no bar for", a room line). D1: a boss's bar (Walter / the Bodies 0x10, the
// Wall boss) shows at any distance while it is up and in view (HbWantBoss), a tall body whose top is above the view keeps
// its bar at the top edge (HbBarPlace), a boss is also in view by its centre; a verbose line says when a boss's bar first
// came up or why it has none 5 s after the boss became active. A new being in a reused task slot is told by its spawn id.
// Still draw only: no exe byte is patched, no ini key, no text id (V52 + 24..27 unused).
#include "hudbars_sim.h"

enum { HB_K_BARS, HB_K_CURSE, HB_K_N };
#define HB_VN 3     // values of the v3.29 rows (the text id layout below keeps it: ids are never renumbered)
#define HB_VMAX4 4  // v3.32 DHS332: values a row may have (Curse Meter + Both)
static const int kHbCount[HB_K_N] = { 3, HB_CMV_N };  // v3.42 SM342: Curse Meter Off / On (v3.32 .. v3.41: Off / On / Companion / Both)
static const char *const kHbIniKey[HB_K_N] = { "EnemyHealthBars", "CurseMeter" };
static const char *const kHbName[HB_K_N][HB_VMAX4] = { { "Off", "Locked Target", "All Near", "" },
                                                      { "Off", "Companion", "Companion", "Both" } };  // v3.42 SM342: On is written "Companion" (kHbCmvIni)
// v3.37 MN337: the names the menu shows; v3.42 SM342: Curse Meter Off / On (slots 2 / 3 unused)
static const char *const kHbShown[HB_K_N][HB_VMAX4] = { { "Off", "Locked Target", "All Near", "" },
                                                       { "Off", "On", "Companion", "Both" } };
static const char *const kHbLabel[HB_K_N] = { OLW_INTERFACE_ENEMY_HEALTH_L, OLW_INTERFACE_CURSE_METER_L };  // v3.53 OL1: Enemy Health (was Enemy Health Bars)
static const char *const kHbDescTxt[HB_K_N][HB_VMAX4] = {  // one line per value (Off: what the row is); Encode keeps 52 characters
    { OLW_INTERFACE_ENEMY_HEALTH_VD_OFF, OLW_INTERFACE_ENEMY_HEALTH_VD_TARGET, OLW_INTERFACE_ENEMY_HEALTH_VD_ALL_NEAR, "" },  // v3.53 OL1
    { OLW_INTERFACE_CURSE_METER_VD_OFF, OLW_INTERFACE_CURSE_METER_VD_ON,  // v3.53 OL1  // v3.42 SM342
      "Companion Eileen's own curse, as the game keeps it.",
      "Your ring plus a small ring for companion Eileen." } };  // v3.32 DHS332 (49 characters)
static const char *const kHbStageName[HB_STAGES] = { "Clear", "Haunted", "Cursed", "Possessed" };  // v3.37 MN337: Untouched -> Clear
// v3.32 DHS332: Both's description and value text at the end of the block (the v3.29 ids stay where they are)
enum { HBM_L0, HBM_D0 = HBM_L0 + HB_K_N, HBM_V0 = HBM_D0 + HB_K_N * HB_VN, HBM_BOTH_D = HBM_V0 + HB_K_N * HB_VN, HBM_BOTH_V,
       HBM_NOPE_D,  // v3.37 MN337: a greyed Player / Both while Playable Eileen is not loaded (new id at the end)
       HBM_PE_PD, HBM_PE_BD,  // v3.41 CM341: Player (grey) / Both while Playable Eileen draws the player's ring (ids at the end)
       HBM_N };
typedef char HbTextsFit[HBM_N <= MSG_CAM0 - MSG_HB0 ? 1 : -1];  // v3.33 MERGE: the block ends at MSG_CAM0 (W4)
static BYTE g_txtHb[HBM_N][56];  // v3.35 CM335: the stage names are no longer the game's text (g_txtHbStage gone)
static DWORD g_hbIds[HB_K_N][HB_VMAX4];
static volatile LONG g_hb[HB_K_N];
static LONG g_hbSnap[HB_K_N];
static int g_hbUnsaved[HB_K_N];
static int g_hbPlayerRow = -1, g_hbEnemyRow = -1;  // where the rows are on their pages (-1 = not there)
#define HB_CURSE_PAGE 4  // v3.37 MN337: the Curse Meter row is on Option 5's top level (logical page 4; was OP_PLAYER)

static const BYTE *HbText(int id) { return id >= MSG_HB0 && id < MSG_HB0 + HBM_N ? g_txtHb[id - MSG_HB0] : g_txtHb[0]; }

// v3.41 CM341: Playable Eileen draws the player's curse ring (its export SH4PE_DrawsPlayerCurse() returned 1; the
// contract REFINED_HANDOVER.md). Decided when its curse export is found (HbPeFind, every 2 s until then) and again as
// Options opens (HbSnapshot); it stays once seen (dsound.dll is never unloaded, the function returns 1 always).
typedef int(__cdecl *HbPeIntFn)(void);
static int g_hbPeOwns;
static HbPeIntFn g_hbPeOnFn;  // its SH4PE_Enabled() (since v1.23.0; NULL = take it as On)
static int g_hbPeOwnLog;      // the "which way" line was written
static void HbPeOwnCheck(HMODULE ds, int curseFound)
{
    HbPeIntFn fn;
    int r = 0;
    if (g_hbPeOwns || !ds) return;
    fn = (HbPeIntFn)(void *)GetProcAddress(ds, "SH4PE_DrawsPlayerCurse");
    if (fn) r = fn();
    if (fn && r == 1) {
        g_hbPeOnFn = (HbPeIntFn)(void *)GetProcAddress(ds, "SH4PE_Enabled");
        g_hbPeOwns = 1;
        g_hbPeOwnLog = 1;
        Log("hud bars: Playable Eileen draws the player's curse ring (SH4PE_DrawsPlayerCurse): only companion Eileen's ring "
            "here (CurseMeter %s; her small ring beside PE's while PE's can show)", kHbCmvShown[g_hb[HB_K_CURSE] == HB_CMV_ON]);  // v3.42 SM342
    } else if (curseFound && !g_hbPeOwnLog) {
        g_hbPeOwnLog = 1;
        if (fn)  // v3.42 SM342: Refined no longer draws the player's ring with any Playable Eileen
            Log("hud bars: SH4PE_DrawsPlayerCurse() returned %d (not 1): no player curse ring (Refined no longer draws it); "
                "companion Eileen's ring at the main place", r);
        else
            Log("hud bars: SH4PE_DrawsPlayerCurse is not exported (Playable Eileen up to v1.23.0): no player curse ring (Refined "
                "no longer draws it; Playable Eileen v1.23.1+ does); companion Eileen's ring at the main place");
    }
}

// ---------------------------------------------------------------- settings (gameplay.inc's pattern, its lock)
static void HbSetLocked(int k, int v)
{
    if (k < 0 || k >= HB_K_N || v < 0 || v >= kHbCount[k]) return;  // v3.32 DHS332: per row
    InterlockedExchange(&g_hb[k], v);
    g_hbUnsaved[k] = 0;
    if (!g_iniFallback && !WritePrivateProfileStringA("Gameplay", kHbIniKey[k], kHbName[k][v], g_iniPath)) {
        g_hbUnsaved[k] = 1;
        Log("hud bars: %s %s applies now but could not be saved in SH4Refined.ini (error %lu)", kHbIniKey[k], kHbName[k][v],
            GetLastError());
    } else if (k == HB_K_CURSE) {  // v3.42 SM342: the menu's name and what the ini holds
        Log("hud bars: CurseMeter %s (the ini: %s)", kHbCmvShown[v == HB_CMV_ON], kHbCmvIni[v == HB_CMV_ON]);
    } else {
        Log("hud bars: %s %s", kHbIniKey[k], kHbName[k][v]);
    }
}
static int HbInput(int k)
{
    int v, cur;
    GpLock();
    cur = (int)g_hb[k];
    v = StepValue(cur, kHbCount[k]);  // v3.32 DHS332; v3.42 SM342: Curse Meter Off / On, nothing greyed (v3.37 .. v3.41 stepped over Player / Both)
    if (v >= 0) HbSetLocked(k, v);
    GpUnlock();
    return 0;
}
static int __cdecl HbIn0(BYTE *cfg, int count) { (void)cfg; (void)count; return HbInput(HB_K_BARS); }
static int __cdecl HbIn1(BYTE *cfg, int count) { (void)cfg; (void)count; return HbInput(HB_K_CURSE); }
static int __cdecl HbGet0(void) { return (int)g_hb[HB_K_BARS]; }
static int __cdecl HbGet1(void) { return (int)g_hb[HB_K_CURSE]; }
static int HbDescOf(int k)
{
    const int v = (int)g_hb[k];
    // v3.42 SM342: Curse Meter Off / On have their own texts (v3.37 .. v3.41's NOPE / PE_PD / PE_BD / BOTH_D: unused)
    return MSG_HB0 + HBM_D0 + k * HB_VN + (v >= 0 && v < HB_VN ? v : 0);
}
static int __cdecl HbDesc0(void) { return HbDescOf(HB_K_BARS); }
static int __cdecl HbDesc1(void) { return HbDescOf(HB_K_CURSE); }

// v3.37 MN337: 1 = draw this value id grey (sh4windowmod.c RowValueC, the value hook 0x525802). v3.42 SM342: nothing is
// grey any more (Curse Meter is Off / On; v3.37 .. v3.41 greyed Player / Both)
static int HbValueIdGreyed(int id)
{
    (void)id;
    return 0;
}
// gameplay.inc GpBuildPages: the rows (t = the row's 7 dwords); v3.37 MN337: Curse Meter on Option 5's top level
static int HbPlayerRow(DWORD *t, int n)
{
    FillRow(t + 7 * n, HbIn1, HbGet1, g_hbIds[HB_K_CURSE], kHbCount[HB_K_CURSE], MSG_HB0 + HBM_L0 + HB_K_CURSE, HbDesc1);  // v3.32 DHS332
    g_hbPlayerRow = n;
    return n + 1;
}
static int HbEnemyRow(DWORD *t, int n)
{
    FillRow(t + 7 * n, HbIn0, HbGet0, g_hbIds[HB_K_BARS], kHbCount[HB_K_BARS], MSG_HB0 + HBM_L0 + HB_K_BARS, HbDesc0);
    g_hbEnemyRow = n;
    return n + 1;
}
// snapshot / changed / discard (gameplay.inc GpSnapshot / GpChanged / GpRowChanged / GpDiscard, under its lock)
static void HbSnapshot(void)
{
    int k;
    for (k = 0; k < HB_K_N; ++k) g_hbSnap[k] = g_hb[k];
    if (g_peLoaded) HbPeOwnCheck(GetModuleHandleA("dsound.dll"), 0);  // v3.41 CM341: Options opens (v3.42 SM342: where her ring goes)
}
static int HbChanged(void)
{
    int k;
    for (k = 0; k < HB_K_N; ++k)
        if (g_hb[k] != g_hbSnap[k]) return 1;
    return 0;
}
static int HbRowChanged(int page, int row)  // -1 = not one of our rows
{
    int k;
    if (page == HB_CURSE_PAGE && row == g_hbPlayerRow && row >= 0 && g_gpMenu) k = HB_K_CURSE;  // v3.37 MN337: was OP_PLAYER (no row without the folders: GpNoFadeLayout)
    else if (page == OP_ENEMY && row == g_hbEnemyRow && row >= 0) k = HB_K_BARS;
    else return -1;
    return g_snapOn && g_hb[k] != g_hbSnap[k];
}
static void HbDiscard(void)
{
    int k;
    for (k = 0; k < HB_K_N; ++k)
        if (g_hb[k] != g_hbSnap[k]) HbSetLocked(k, g_hbSnap[k]);
}
static int HbParse(int k, const char *s)  // unknown text = Off
{
    int v;
    if (k == HB_K_CURSE) return HbCmvParse(s, NULL);  // v3.42 SM342: Off / On (old On / Player -> Off, Companion / Both -> On)
    for (v = kHbCount[k] - 1; v > 0 && _stricmp(s, kHbName[k][v]); --v) {}  // v3.32 DHS332: per row
    if (!v && !_stricmp(s, kHbShown[k][1])) v = 1;  // v3.37 MN337: "Player" (the menu's name) reads as On too
    return v;
}
static void HbIniReread(void)  // gameplay.inc GpIniReread (the ini changed on disk; under the lock)
{
    char buf[32];
    int k, v;
    if (g_iniFallback) return;
    for (k = 0; k < HB_K_N; ++k) {
        if (g_hbUnsaved[k]) continue;
        GetPrivateProfileStringA("Gameplay", kHbIniKey[k], kHbName[k][g_hb[k]], buf, sizeof buf, g_iniPath);
        v = HbParse(k, buf);
        if (v == g_hb[k]) continue;
        InterlockedExchange(&g_hb[k], v);
        g_hbSnap[k] = v;
        Log("hud bars: [Gameplay] %s read again: %s", kHbIniKey[k], k == HB_K_CURSE ? kHbCmvShown[v == HB_CMV_ON] : kHbName[k][v]);  // v3.42 SM342
    }
}
static void HbInit(void)  // gameplay.inc GameplayInstall (Init, before the Options tables)
{
    char buf[32];
    int k, v;
    for (k = 0; k < HB_K_N; ++k) {
        IniStr("Gameplay", kHbIniKey[k], buf, sizeof buf);
        g_hb[k] = HbParse(k, buf);
        if (k == HB_K_CURSE) {  // v3.42 SM342: an old value read another way (the ini keeps it until the row is changed)
            int old = 0;
            HbCmvParse(buf, &old);
            if (old)
                Log("hud bars: [Gameplay] CurseMeter=%s (v3.41 or older) reads as %s: %s", buf, kHbCmvShown[g_hb[k] == HB_CMV_ON],
                    g_hb[k] == HB_CMV_ON ? "companion Eileen's ring (Both's player ring is Playable Eileen's own)" :
                    "Refined no longer draws the player's ring (Playable Eileen v1.23.1+ does)");
        }
        Encode(g_txtHb[HBM_L0 + k], 56, kHbLabel[k]);
        for (v = 0; v < HB_VN; ++v) {
            Encode(g_txtHb[HBM_D0 + k * HB_VN + v], 56, kHbDescTxt[k][v]);
            Encode(g_txtHb[HBM_V0 + k * HB_VN + v], 56, k == HB_K_BARS ? OlShow(OLM_INTERFACE_ENEMY_HEALTH, kHbShown[k][v]) : kHbShown[k][v]);  // v3.37 MN337: the shown name (Player)  // v3.53 OL1: Locked Target shows Target (the ini keeps it)
            g_hbIds[k][v] = MSG_HB0 + HBM_V0 + k * HB_VN + v;
        }
    }
    Encode(g_txtHb[HBM_BOTH_D], 56, kHbDescTxt[HB_K_CURSE][HB_CM_BOTH]);  // v3.32 DHS332: Both, at the end of the block
    Encode(g_txtHb[HBM_BOTH_V], 56, kHbShown[HB_K_CURSE][HB_CM_BOTH]);
    Encode(g_txtHb[HBM_NOPE_D], 56, "Without Playable Eileen this shows Companion.");  // v3.37 MN337
    Encode(g_txtHb[HBM_PE_PD], 56, "Playable Eileen draws your ring (its pause menu).");  // v3.41 CM341 (49 characters)
    Encode(g_txtHb[HBM_PE_BD], 56, "Companion ring; yours is in Playable Eileen's menu.");  // v3.41 CM341 (51 characters)
    g_hbIds[HB_K_CURSE][HB_CM_BOTH] = MSG_HB0 + HBM_BOTH_V;
    Log("hud bars: EnemyHealthBars %s, CurseMeter %s (drawn from the HUD hook 004EFBFD; no exe patch)",
        kHbName[0][g_hb[0]], kHbCmvShown[g_hb[1] == HB_CMV_ON]);  // v3.42 SM342: the menu's name
    Log("hud bars: Curse Meter Off / On (v3.42): On = companion Eileen's ring only; Refined never draws the player's curse ring "
        "(Playable Eileen v1.23.1+ draws it; with an older one or none, nobody does)");  // v3.42 SM342 (once)
}

// ---------------------------------------------------------------- drawing helpers (pixels of the whole back buffer)
#define HB_VMAX 12288  // v3.30 HB330: was 1536 (the ring meter and the ghost revive bars); v3.32 DHS332: 6144 -> 8192 (Both: a second ring); v3.38 HB338: 8192 -> 12288 (plates, hairlines, claw shadows)
static SpVtx g_hbV[HB_VMAX];
static int g_hbVn;
static float g_hbGreyK;  // v3.32 MERGE: > 0 while the locked small ring is drawn (HbArgb greys every colour by it)
static DWORD HbArgb(DWORD rgb, float a)
{
    const DWORD al = (DWORD)(HbClamp01(a) * 255.0f + 0.5f);
    if (g_hbGreyK > 0.0f) rgb = HbLockRgb(rgb & 0xFFFFFF, g_hbGreyK);  // v3.32 MERGE
    return al << 24 | (rgb & 0xFFFFFF);
}
// a rectangle, top colour ct, bottom colour cb (vertical gradient)
static void HbRect(float x0, float y0, float x1, float y1, DWORD ct, DWORD cb)
{
    SpVtx q[4];
    int m;
    if (g_hbVn + 6 > HB_VMAX || !(x1 > x0) || !(y1 > y0)) return;
    for (m = 0; m < 4; ++m) {
        q[m].x = ((m & 1) ? x1 : x0) - 0.5f;
        q[m].y = ((m & 2) ? y1 : y0) - 0.5f;
        q[m].z = 0.0f;
        q[m].rhw = 1.0f;
        q[m].c = (m & 2) ? cb : ct;
        q[m].u = q[m].v = 0.0f;
    }
    g_hbV[g_hbVn++] = q[0], g_hbV[g_hbVn++] = q[1], g_hbV[g_hbVn++] = q[2];
    g_hbV[g_hbVn++] = q[1], g_hbV[g_hbVn++] = q[3], g_hbV[g_hbVn++] = q[2];
}
// v3.30 HB330: one triangle with a colour per vertex (the ring, the thorns, the diamonds, the slashes)
static void HbTri(float x0, float y0, DWORD c0, float x1, float y1, DWORD c1, float x2, float y2, DWORD c2)
{
    SpVtx *v;
    if (g_hbVn + 3 > HB_VMAX) return;
    v = &g_hbV[g_hbVn];
    g_hbVn += 3;
    v[0].x = x0 - 0.5f, v[0].y = y0 - 0.5f, v[0].c = c0;
    v[1].x = x1 - 0.5f, v[1].y = y1 - 0.5f, v[1].c = c1;
    v[2].x = x2 - 0.5f, v[2].y = y2 - 0.5f, v[2].c = c2;
    v[0].z = v[1].z = v[2].z = 0.0f;
    v[0].rhw = v[1].rhw = v[2].rhw = 1.0f;
    v[0].u = v[0].v = v[1].u = v[1].v = v[2].u = v[2].v = 0.0f;
}
// a quad: corners top-left, top-right, bottom-left, bottom-right (HbRect's order), one colour each
static void HbQuad(const float *x, const float *y, const DWORD *c)
{
    if (g_hbVn + 6 > HB_VMAX) return;
    HbTri(x[0], y[0], c[0], x[1], y[1], c[1], x[2], y[2], c[2]);
    HbTri(x[1], y[1], c[1], x[3], y[3], c[3], x[2], y[2], c[2]);
}
// a diamond of half size s centred at (cx, cy)
static void HbDiamond(float cx, float cy, float s, DWORD c)
{
    const float x[4] = { cx, cx + s, cx - s, cx }, y[4] = { cy - s, cy, cy, cy + s };
    const DWORD cc[4] = { c, c, c, c };
    HbQuad(x, y, cc);
}
// an annulus band r0..r1 from angle a0 to a1 (radians, 0 = up, clockwise on screen), inner / outer colour; seg = the
// largest step (radians)
static void HbRing(float cx, float cy, float r0, float r1, float a0, float a1, DWORD cin, DWORD cout, float seg)
{
    int n, i;
    float s0, c0;
    if (!(r1 > r0) || !(seg > 0.0f)) return;
    n = (int)ceilf(fabsf(a1 - a0) / seg);
    if (n < 1) n = 1;
    if (n > 96) n = 96;
    s0 = sinf(a0), c0 = cosf(a0);
    for (i = 1; i <= n; ++i) {
        const float t = a0 + (a1 - a0) * (float)i / (float)n, s1 = sinf(t), c1 = cosf(t);
        const float x[4] = { cx + r1 * s0, cx + r1 * s1, cx + r0 * s0, cx + r0 * s1 };
        const float y[4] = { cy - r1 * c0, cy - r1 * c1, cy - r0 * c0, cy - r0 * c1 };
        const DWORD cc[4] = { cout, cout, cin, cin };
        HbQuad(x, y, cc);
        s0 = s1, c0 = c1;
    }
}
// a disc: centre colour cc, edge colour ce (24 sectors)
static void HbDisc(float cx, float cy, float r, DWORD cc, DWORD ce)
{
    int i;
    for (i = 0; i < 24; ++i) {
        const float t0 = 6.2831853f * (float)i / 24.0f, t1 = 6.2831853f * (float)(i + 1) / 24.0f;
        HbTri(cx, cy, cc, cx + r * sinf(t0), cy - r * cosf(t0), ce, cx + r * sinf(t1), cy - r * cosf(t1), ce);
    }
}
static DWORD HbLerpRgb(DWORD a, DWORD b, float t)  // 0xRRGGBB
{
    DWORD r = 0;
    int sh;
    t = HbClamp01(t);
    for (sh = 0; sh <= 16; sh += 8) {
        const float ca = (float)((a >> sh) & 0xFF), cb = (float)((b >> sh) & 0xFF);
        r |= (DWORD)(ca + (cb - ca) * t + 0.5f) << sh;
    }
    return r;
}
static void HbFlush(void *dev)  // FxPanel's states: untextured, diffuse colour and alpha, alpha blending
{
    if (!g_hbVn) return;
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(g_hbVn / 3), g_hbV, sizeof(SpVtx));
    g_hbVn = 0;
}

// The colours (0xRRGGBB): the game's HUD is sparse and dark; these are dried blood, ectoplasm, bone and bruise, never
// pure. v3.30 HB330: a bevelled dark frame, the damage chip hot when it lands, quarter notches; ghosts in ectoplasm with
// a stagger line, a rise preview and the revive bar (violet; steel and broken when the Sword pins them). Mock-ups:
// tools/hudbars_mockup.py (it draws with the same primitives), design: staging/v330_feedback/w4_hud/DESIGN.md.
#define HB_RIM     0x000000u
#define HB_BACK0   0x17110Fu
#define HB_BACK1   0x090706u
#define HB_CHIPHOT 0xFFF0D8u  // the part just lost: hot when it lands ...
#define HB_TRAIL   0xC2B094u  // ... pale bone while it waits and drains
#define HB_SHEEN   0xFFFFFFu
#define HB_GUARD   0xDDF4F0u  // a ghost's stagger line
#define HB_LOCK    0xE2D8C8u
static const DWORD kHbFill[3][2] = {  // top, bottom: 0 monster, 1 ghost (any state), 2 boss
    { 0xB0362Cu, 0x5C0E0Bu }, { 0x9CC0C0u, 0x3A5A60u }, { 0xC0441Eu, 0x5A0C06u } };
static const DWORD kHbRev[2] = { 0xD2C4FFu, 0x6A54B0u };    // the revive countdown
static const DWORD kHbSteel[2] = { 0xB4BCC8u, 0x4C5460u };  // sealed by the Sword of Obedience

// ---------------------------------------------------------------- the clock (real seconds, frozen while hidden)
static LARGE_INTEGER g_hbQf, g_hbQLast;
static float HbClock(void)  // seconds since the last call (every HUD frame calls it, drawn or not)
{
    LARGE_INTEGER n;
    float dt = 0.0f;
    if (!g_hbQf.QuadPart && (!QueryPerformanceFrequency(&g_hbQf) || !g_hbQf.QuadPart)) g_hbQf.QuadPart = 1;
    QueryPerformanceCounter(&n);
    if (g_hbQLast.QuadPart) dt = (float)((double)(n.QuadPart - g_hbQLast.QuadPart) / (double)g_hbQf.QuadPart);
    g_hbQLast = n;
    return dt;
}
static float g_hbNow;  // seconds of drawn frames (the Possessed beat, the rise preview's pulse)

// ---------------------------------------------------------------- Enemy Health Bars
#define HB_MAXE 32
#define HB_DRAW_MAX 8
typedef struct {
    const BYTE *task, *work;
    HbBar bar;
    HbRevive rev;     // v3.30 HB330: ghosts: the revive countdown / the seal
    float y;          // the anchor's smoothed height (world Y, down)
    float rise;       // ghosts: the share of health it gets back when it rises (-1 = none)
    int yOk, gstate, downShow, alive;
    unsigned id;      // v3.52 HB352: its spawn id (a new being in a reused task slot has the same work pointer)
    int refSaid;      // v3.52 HB352: the refusal reasons already counted / logged this room visit (bit per HB_RF_*)
} HbEnt;
static HbEnt g_hbE[HB_MAXE];
static int g_hbEN;
static DWORD g_hbRoom = 0xFFFFFFFFu;
static __declspec(align(16)) float g_hbSeg[8], g_hbPlane[4];
static int HbTypeOk(int type) { return (type >= 0x02 && type <= 0x11 && type != 0x04); }  // never 1, 4 (mm), 0x12+
static float HbTypeHeight(int type)  // [assumed] above the work position when the bones are not this frame's
{
    switch (type) {
    case 0x0A: return 150.0f;   // Sniffer Dog
    case 0x03: return 200.0f;   // Hummer (flies; its position is its body)
    case 0x08: return 380.0f;   // Twin Victim
    case 0x10: return 380.0f;   // Walter / the Conjoined Bodies
    default: return 340.0f;     // humanoids and ghosts (Eileen is about 340 tall)
    }
}
// 1 = a wall plane lies between the camera eye and the point (Item Shine's SpBlocked, without its through-walls key)
static int HbBlocked(const float *eye, const float *p)
{
    float a, b;
    const float *n = g_hbPlane;
    if (!*(volatile DWORD *)0x12F6D74) return 0;  // no collision loaded
    g_hbSeg[0] = eye[0], g_hbSeg[1] = eye[1], g_hbSeg[2] = eye[2], g_hbSeg[3] = 1.0f;
    g_hbSeg[4] = p[0], g_hbSeg[5] = p[1], g_hbSeg[6] = p[2], g_hbSeg[7] = 1.0f;
    if (!((int(__cdecl *)(const float *, float *))0x561CD0)(g_hbSeg, g_hbPlane)) return 0;
    a = n[0] * g_hbSeg[0] + n[1] * g_hbSeg[1] + n[2] * g_hbSeg[2] + n[3];
    b = n[0] * g_hbSeg[4] + n[1] * g_hbSeg[5] + n[2] * g_hbSeg[6] + n[3];
    if (!((a < 0.0f) != (b < 0.0f)) || a == b) return 0;
    return a / (a - b) < 0.93f;
}
// the top of the body: the highest bone of this frame's skeleton (Y down), or -1 when the bones are not usable
static int HbTop(const BYTE *work, const float *pos, float *top, int *hidden)
{
    const BYTE *m = *(const BYTE *const *)(work + 0x30), *skel;
    const float *mats;
    int nb, q, ok = 0;
    float t = 0.0f;
    *hidden = 0;
    if (!m) return 0;
    *hidden = (*(const DWORD *)(m + 0xE4) & 4) != 0;  // not drawn (0x55C650 tests the same bit)
    mats = *(const float *const *)(m + 0xC0);
    skel = *(const BYTE *const *)(m + 0xE0);
    if (!mats || !skel) return 0;
    nb = *(const int *)(skel + 0x14);
    if (nb <= 0) return 0;
    if (nb > 64) nb = 64;
    for (q = 0; q < nb; ++q) {
        const float *b = mats + 16 * q + 12;
        if (!_finite(b[0]) || !_finite(b[1]) || !_finite(b[2])) return 0;
        if (fabsf(b[0] - pos[0]) > 400.0f || fabsf(b[2] - pos[2]) > 400.0f) return 0;  // not this frame's (as Death Presence)
        if (!ok || b[1] < t) t = b[1], ok = 1;
    }
    if (!ok || !(t < pos[1] + 50.0f)) return 0;
    *top = t;
    return 1;
}
static HbEnt *HbFind(const BYTE *task, const BYTE *work, unsigned id)
{
    int i;
    for (i = 0; i < g_hbEN; ++i)
        if (g_hbE[i].task == task) {
            if (g_hbE[i].work != work || g_hbE[i].id != id) {  // the slot holds a new being (v3.52 HB352: + its spawn id)
                memset(&g_hbE[i], 0, sizeof g_hbE[i]);
                g_hbE[i].task = task, g_hbE[i].work = work, g_hbE[i].id = id, g_hbE[i].rise = -1.0f;
            }
            return &g_hbE[i];
        }
    if (g_hbEN >= HB_MAXE) return NULL;
    memset(&g_hbE[g_hbEN], 0, sizeof g_hbE[0]);
    g_hbE[g_hbEN].task = task, g_hbE[g_hbEN].work = work, g_hbE[g_hbEN].id = id, g_hbE[g_hbEN].rise = -1.0f;
    return &g_hbE[g_hbEN++];
}
// ---------------------------------------------------------------- v3.52 HB352: refusals and the bosses' first bar (logs)
// A bar refused for an enemy that is not in the room yet or has no place in it (hudbars_sim.h HbRefuseWhy): one verbose
// line per being and reason a room visit (60 a session), and one verbose room line when the room is left (or the bars are
// turned off) with the count of beings by reason (tools\logcheck.py sums them). The bosses (Walter / the Bodies 0x10, the
// Wall boss): one verbose line when a boss's bar is first up in a room visit (how long after the boss became active), or,
// if it has none 5 s after it became active, why (D1's proof in the next QA). Bounded: 40 a session.
static HbRefCount g_hbRef;                 // this room visit's refused beings by reason
static DWORD g_hbRefRoom = 0xFFFFFFFFu;    // the room g_hbRef counts (stage << 16 | room)
static volatile LONG g_hbRefQ, g_hbBossQ;  // the per-being and the boss line quotas
static int g_hbRefTot[HB_RF_N];            // this session (the line at a room end says them too)
#define HB_BOSS_N 4
typedef struct { const BYTE *task; unsigned id; float tActive; int active, said; } HbBossNote;
static HbBossNote g_hbBoss[HB_BOSS_N];
static int g_hbBossN;
static void HbRefRoomEnd(void)  // the room visit counted in g_hbRef ends (room change, bars off)
{
    if (g_hbRefRoom != 0xFFFFFFFFu && (g_hbRef.n[HB_RF_HIDDEN] || g_hbRef.n[HB_RF_PLACE]))
        LogV("hud bars: room %lu:%lX: no bar for %d enemy(ies) not in the room yet (hidden, arriving or a placeholder), %d not at a "
             "place in the room; this session %d / %d", g_hbRefRoom >> 16, g_hbRefRoom & 0xFFFF, g_hbRef.n[HB_RF_HIDDEN],
             g_hbRef.n[HB_RF_PLACE], g_hbRefTot[HB_RF_HIDDEN], g_hbRefTot[HB_RF_PLACE]);
    memset(&g_hbRef, 0, sizeof g_hbRef);
    g_hbRefRoom = 0xFFFFFFFFu;
    g_hbBossN = 0;
}
static void HbRefuseNote(HbEnt *e, int why, int type, int cls, const float *pos, float dist, const char *more)  // v3.58 EN358: + more
{
    if (!HbRefNote(&g_hbRef, &e->refSaid, why)) return;
    ++g_hbRefTot[why];
    if (LogQuota(&g_hbRefQ, 60))
        LogV("hud bars: no bar for %08X (type %02X) in room %lu:%lX: %s%s (state %X, at %.0f %.0f %.0f, %.0f from you)", e->id, type,
             g_hbRefRoom >> 16, g_hbRefRoom & 0xFFFF, kHbRfName[why], more ? more : "", cls, pos[0], pos[1], pos[2], dist);
}
// v3.58 EN358: Walter's down timers are read only where the stock code keeps them (byte-checked once, read only; no patch):
// bit 0 the stalker's [[work+0x24]+0x94] (0x4A4930 step 1), bit 1 the final Walter's [[work+0x24]+0x80] (0x4A8690)
static int g_hbWalterSite = -1;
static int HbWalterSites(void)
{
    if (g_hbWalterSite < 0) {
        static const Site kS1 = { 0x4A4A12, { 0xD9, 0x87, 0x94, 0x00, 0x00, 0x00 }, 6 }, kS2 = { 0x4A4A1C, { 0xD9, 0x97, 0x94, 0x00, 0x00, 0x00 }, 6 };
        static const Site kF1 = { 0x4A8760, { 0xD8, 0xAF, 0x80, 0x00, 0x00, 0x00 }, 6 }, kF2 = { 0x4A8801, { 0xD8, 0xAF, 0x80, 0x00, 0x00, 0x00 }, 6 };
        g_hbWalterSite = (SiteOk(&kS1) && SiteOk(&kS2) ? 1 : 0) | (SiteOk(&kF1) && SiteOk(&kF2) ? 2 : 0);
        Log("hud bars: Walter downed keeps his bar (boss rules) with a revive bar from the game's own down timer: the stalker's "
            "(004A4A12) %s, the final Walter's (004A8760) %s", g_hbWalterSite & 1 ? "read" : "differs from GOG v7: health bar only",
            g_hbWalterSite & 2 ? "read" : "differs from GOG v7: health bar only");
    }
    return g_hbWalterSite;
}
static volatile LONG g_hbWalterQ;
// v3.58 EN358: Fade In's share of an enemy's body shown now (fadein.inc, after this file; 1 = all or not fading)
static float FiBodyShown(const BYTE *task, const BYTE *work, unsigned id);
static HbBossNote *HbBossOf(const BYTE *task, unsigned id)
{
    int i;
    for (i = 0; i < g_hbBossN; ++i)
        if (g_hbBoss[i].task == task && g_hbBoss[i].id == id) return &g_hbBoss[i];
    if (g_hbBossN >= HB_BOSS_N) return NULL;
    memset(&g_hbBoss[g_hbBossN], 0, sizeof g_hbBoss[0]);
    g_hbBoss[g_hbBossN].task = task, g_hbBoss[g_hbBossN].id = id;
    return &g_hbBoss[g_hbBossN++];
}
// one frame of a boss in the list: active = drawn by the game and alive; drawn = its bar is drawn this frame
static void HbBossFrame(const BYTE *task, unsigned id, int type, int cls, int active, int drawn, const char *why,
                        const float *hp, float dist)
{
    HbBossNote *b = HbBossOf(task, id);
    const char *name = type == 0x10 ? "Walter" : "the Wall boss";
    if (!b || b->said >= 2) return;
    if (active && !b->active) b->active = 1, b->tActive = g_hbNow;
    if (!b->active) return;
    if (drawn) {
        b->said = 2;
        if (LogQuota(&g_hbBossQ, 40))
            LogV("hud bars: boss %s %08X (type %02X) in room %lu:%lX: its bar is up %.1f s after it became active (state %X, %.0f from "
                 "you, hp %.0f / %.0f)", name, id, type, g_hbRefRoom >> 16, g_hbRefRoom & 0xFFFF, g_hbNow - b->tActive, cls, dist,
                 hp[0], hp[1]);
    } else if (!b->said && g_hbNow - b->tActive >= 5.0f) {
        b->said = 1;
        if (LogQuota(&g_hbBossQ, 40))
            LogV("hud bars: boss %s %08X (type %02X) in room %lu:%lX: active 5 s and no bar yet: %s (state %X, %.0f from you, hp %.0f / "
                 "%.0f)", name, id, type, g_hbRefRoom >> 16, g_hbRefRoom & 0xFFFF, why, cls, dist, hp[0], hp[1]);
    }
}
typedef struct {
    float x, y, dist, a, shown, trail, heat;
    float guard;              // ghosts: stagger gauge share (-1 = none)
    float rise;               // ghosts: rise preview share (-1 = none)
    float revA, revFrac, seal, flash;
    int ghost, locked, boss;
} HbDraw;
static float HbPx(float H) { return H / 1080.0f > 1.0f ? H / 1080.0f : 1.0f; }
static float HbBarW(float H, int boss) { return H * 0.075f * (boss ? 1.6f : 1.0f); }
// the revive bar under a ghost's bar: the countdown (violet, draining left) crossfaded with the seal (two steel halves,
// slashed, the Sword's cross in the gap; they close in as the pin lands, with a white flash)
static void HbReviveQuads(const HbDraw *d, float x0, float x1, float y1, float H, float px)
{
    const float W = x1 - x0, rh = H * 0.0030f > 2.0f ? H * 0.0030f : 2.0f, ry0 = y1 + 3.0f * px, ry1 = ry0 + rh;
    const float a = d->a * d->revA, s = d->seal;
    if (s < 1.0f) {
        const float k = (1.0f - s) * a, xe = x0 + W * d->revFrac;
        HbRect(x0 - px, ry0 - px, x1 + px, ry1 + px, HbArgb(HB_RIM, 0.80f * k), HbArgb(HB_RIM, 0.80f * k));
        HbRect(x0, ry0, x1, ry1, HbArgb(0x100C18u, 0.75f * k), HbArgb(0x100C18u, 0.75f * k));
        HbRect(x0, ry0, xe, ry1, HbArgb(kHbRev[0], 0.92f * k), HbArgb(kHbRev[1], 0.92f * k));
        if (d->revFrac > 0.01f) HbRect(xe - px, ry0 - px, xe + px, ry1 + px, HbArgb(0xF4EEFFu, 0.75f * k), HbArgb(0xF4EEFFu, 0.75f * k));
    }
    if (s > 0.0f) {
        const float k = s * a, g = 3.0f * px + 2.0f * px * (1.0f - s), xc = 0.5f * (x0 + x1);
        const DWORD st0 = HbLerpRgb(kHbSteel[0], 0xFFFFFFu, d->flash), st1 = HbLerpRgb(kHbSteel[1], 0xFFFFFFu, 0.7f * d->flash);
        int h, i;
        for (h = 0; h < 2; ++h) {
            const float hx0 = h ? xc + g : x0, hx1 = h ? x1 : xc - g;
            if (!(hx1 > hx0)) continue;
            HbRect(hx0 - px, ry0 - px, hx1 + px, ry1 + px, HbArgb(HB_RIM, 0.85f * k), HbArgb(HB_RIM, 0.85f * k));
            HbRect(hx0, ry0, hx1, ry1, HbArgb(st0, 0.80f * k), HbArgb(st1, 0.80f * k));
            for (i = 0; i < 3; ++i) {  // three dark slashes
                const float sx = hx0 + (hx1 - hx0) * ((float)i + 0.7f) / 3.4f;
                const float qx[4] = { sx + rh, sx + rh + px, sx, sx + px }, qy[4] = { ry0, ry0, ry1, ry1 };
                const DWORD c = HbArgb(0x000000u, 0.55f * k), qc[4] = { c, c, c, c };
                HbQuad(qx, qy, qc);
            }
        }
        // the cross: a blade through the gap and its guard
        HbRect(xc - 1.5f * px, ry0 - 3.0f * px, xc + 1.5f * px, ry1 + 3.0f * px, HbArgb(HB_RIM, 0.6f * k), HbArgb(HB_RIM, 0.6f * k));
        HbRect(xc - 0.5f * px, ry0 - 3.0f * px, xc + 0.5f * px, ry1 + 2.0f * px, HbArgb(0xF2F4F8u, 0.95f * k), HbArgb(0x9AA2AEu, 0.95f * k));
        HbRect(xc - 2.5f * px, ry0 - 2.0f * px, xc + 2.5f * px, ry0 - px, HbArgb(0xF2F4F8u, 0.95f * k), HbArgb(0xF2F4F8u, 0.95f * k));
    }
}
static void HbBarQuads(const HbDraw *d, float H)
{
    const float W = HbBarW(H, d->boss), hh = (H * 0.0065f > 4.0f ? H * 0.0065f : 4.0f) * (d->boss ? 1.25f : 1.0f);
    const float x0 = d->x - 0.5f * W, x1 = d->x + 0.5f * W, y1 = d->y, y0 = y1 - hh, px = HbPx(H);
    const float a = d->a * (d->locked ? 1.0f : 0.85f);
    const int k = d->ghost ? 1 : d->boss ? 2 : 0, nq = d->boss ? 10 : 4;
    const float gl = d->ghost ? (floorf(hh * 0.22f + 0.5f) > px ? floorf(hh * 0.22f + 0.5f) : px) : 0.0f;
    const float hy1 = y1 - (d->ghost ? gl + px : 0.0f), xe = x0 + W * d->shown;
    int i;
    // the frame: a soft shadow, the rim, a dark back
    HbRect(x0 - 2.0f * px, y0 - 2.0f * px, x1 + 2.0f * px, y1 + 2.0f * px, HbArgb(HB_RIM, 0.22f * a), HbArgb(HB_RIM, 0.22f * a));
    HbRect(x0 - px, y0 - px, x1 + px, y1 + px, HbArgb(HB_RIM, 0.85f * a), HbArgb(HB_RIM, 0.85f * a));
    HbRect(x0, y0, x1, y1, HbArgb(HB_BACK0, 0.80f * a), HbArgb(HB_BACK1, 0.80f * a));
    // a ghost down: the health it gets back when it rises, pulsing slowly, a bright mark where it ends
    if (d->rise > d->shown + 0.004f) {
        const float kk = (0.40f + 0.12f * sinf(g_hbNow * 5.0265f)) * a, xr = x0 + W * d->rise;
        HbRect(xe, y0, xr, hy1, HbArgb(HbLerpRgb(kHbFill[k][0], 0xFFFFFFu, 0.25f), kk), HbArgb(kHbFill[k][1], kk));
        HbRect(xr - px, y0, xr, hy1, HbArgb(0xE8FFFFu, 0.70f * a), HbArgb(0xE8FFFFu, 0.70f * a));
    }
    // the damage chip: hot when it lands, bone while it waits and drains
    if (d->trail > d->shown + 0.002f) {
        const DWORD ch = HbLerpRgb(HB_TRAIL, HB_CHIPHOT, d->heat);
        HbRect(xe, y0, x0 + W * d->trail, hy1, HbArgb(ch, 0.85f * a), HbArgb(ch, 0.62f * a));
    }
    // the fill: a gradient, a light top row, a darker bottom row
    HbRect(x0, y0, xe, hy1, HbArgb(kHbFill[k][0], 0.96f * a), HbArgb(kHbFill[k][1], 0.96f * a));
    HbRect(x0, y0, xe, y0 + px, HbArgb(HB_SHEEN, 0.24f * a), HbArgb(HB_SHEEN, 0.24f * a));
    if (hy1 - y0 > 3.0f * px) HbRect(x0, hy1 - px, xe, hy1, HbArgb(0x000000u, 0.25f * a), HbArgb(0x000000u, 0.25f * a));
    for (i = 1; i < nq; ++i) {  // quarter notches (bosses: tenths), the middle one darker
        const float xq = x0 + W * (float)i / (float)nq, na = (i * 2 == nq ? 0.45f : 0.30f) * a;
        HbRect(xq - 0.5f * px, y0, xq + 0.5f * px, hy1, HbArgb(0x000000u, na), HbArgb(0x000000u, na));
    }
    HbRect(x0, y0, x1, y0 + px, HbArgb(HB_SHEEN, 0.07f * a), HbArgb(HB_SHEEN, 0.07f * a));  // the bevel's light edge
    if (d->ghost && d->guard >= 0.0f)  // the stagger line: the gauge (empty = it falls)
        HbRect(x0, y1 - gl, x0 + W * d->guard, y1, HbArgb(HB_GUARD, 0.85f * a), HbArgb(HB_GUARD, 0.55f * a));
    if (d->locked) {  // the target: a pale diamond at each end
        const float my = 0.5f * (y0 + y1), sz = 2.5f * px + 0.25f * hh;
        HbDiamond(x0 - 3.0f * px - sz, my, sz + 1.5f * px, HbArgb(HB_RIM, 0.8f * a));
        HbDiamond(x0 - 3.0f * px - sz, my, sz, HbArgb(HB_LOCK, 0.95f * a));
        HbDiamond(x1 + 3.0f * px + sz, my, sz + 1.5f * px, HbArgb(HB_RIM, 0.8f * a));
        HbDiamond(x1 + 3.0f * px + sz, my, sz, HbArgb(HB_LOCK, 0.95f * a));
    }
    if ((d->ghost || d->boss) && d->revA > 0.004f) HbReviveQuads(d, x0, x1, y1, H, px);  // v3.58 EN358: + Walter downed (a boss: revA only for him)
}
static int g_hbGhostLog;  // the first ghost states logged (live check of the RE)
static void HbEnemies(int mode, const Viewport8 *vp, float dt)
{
    static HbDraw dl[HB_MAXE];
    const float *pp = (const float *)0x10A0F60, *eye = (const float *)(0xFD5AC0 + 48);
    const BYTE *lock = *(const BYTE *const *)0x10A115C;  // gPlayer[0].target
    const BYTE *pinReq = *(const BYTE *const volatile *)0xFD0BC0;  // the Sword's "pin this ghost" mailbox (0x464E00)
    const BYTE *t = NULL;
    const float H = FxH();
    int i, n = 0, guard = 0;
    if (!_finite(pp[0] + pp[1] + pp[2]) || !_finite(eye[0] + eye[1] + eye[2])) return;
    ((void(__cdecl *)(float *))0x55BF50)(g_gvM);  // this frame's world -> GS matrix (as Item Shine, Death Presence)
    for (i = 0; i < g_hbEN; ++i) g_hbE[i].alive = 0;
    while ((t = ((const BYTE *(__cdecl *)(const BYTE *))0x45E760)(t)) != NULL && guard++ < 96) {  // EnemyListNext
        const BYTE *w = *(const BYTE *const *)(t + 0x408), *pr;
        const float *hp;
        float pos[3], anc[3], top, frac, want, h[4], sx = 0.0f, sy = 0.0f, dx, dy, dz, dist, cd = 0.0f;
        float hm[4], tx = 0.0f, ty = 0.0f, mx = 0.0f, my = 0.0f;  // v3.52 HB352: the top's and the middle's projections
        int type, cls, ghost, gs = HB_GS_STAND, gone, boss = 0, hidden = 0, seen = 0, onScreen, why, okTop, okMid;
        int ud, walter = 0;  // v3.58 EN358: nothing draws it yet (HB_UD_*); a Walter whose downs keep his bars
        HbEnt *e;
        if (!w) continue;
        type = *(const int *)(w + 0x20);
        if (!HbTypeOk(type)) continue;
        hp = *(const float *const *)(w + 0x34);
        if (!hp || !(hp[1] > 0.0f) || !_finite(hp[0])) continue;
        memcpy(pos, w, sizeof pos);
        if (!_finite(pos[0] + pos[1] + pos[2])) continue;
        if ((e = HbFind(t, w, ((unsigned(__cdecl *)(const BYTE *))0x45B6D0)(t))) == NULL) continue;  // v3.52 HB352: + the spawn id (ENM_ID)
        e->alive = 1;
        cls = *(const int *)(w + 0x3C);
        pr = *(const BYTE *const *)(t + 0x41C);  // the Wall boss: type 5, placement mode 8 (as One-Hit Kill's lookup)
        if (type == 0x10) boss = 1;
        else if (type == 5 && (ULONG_PTR)(pr + 0x2C) >= 0x10000) boss = (*(const unsigned *)(pr + 0x40) & 0xF) == 8;
        dx = pos[0] - pp[0], dy = pos[1] - pp[1], dz = pos[2] - pp[2];
        dist = sqrtf(dx * dx + dy * dy + dz * dz);
        frac = hp[0] / hp[1];
        // v3.52 HB352 (D2): no bar at all for an enemy the game does not draw yet (state 0 / 0x0D..0x0F: hidden, an arriving
        // ghost sliding onto the player, a placeholder) or with no place in the room; its bar's clocks go on, the anchor
        // starts afresh when it appears (hudbars_sim.h HbRefuseWhy)
        // v3.58 EN358: nor for one the game draws nothing for yet (its actor record has no model and no draw callbacks: an
        // event dog before story flag 0x130, a task that returns before its draw registration; hudbars_sim.h HbUndrawnWhy)
        {
            const int recOk = (ULONG_PTR)pr >= 0x10000;
            ud = HbUndrawnWhy(recOk, recOk ? *(void *const *)(pr + 0x14) : NULL, recOk ? *(void *const *)(pr + 0x20) : NULL,
                              recOk ? *(void *const *)(pr + 0x24) : NULL, recOk ? *(void *const *)(pr + 0x28) : NULL, type,
                              EnmModeWord(t), ENM_DOGS_AWAKE);
        }
        if ((why = HbRefuseWhy2(cls, pos, ud)) != HB_RF_NONE) {  // v3.58 EN358: + ud (was HbRefuseWhy)
            HbRefuseNote(e, why, type, cls, pos, dist, kHbUdName[ud]);
            HbBarStep(&e->bar, frac, 0.0f, 0, dt);
            e->bar.alpha = 0.0f, e->yOk = 0;
            if (boss) HbBossFrame(t, e->id, type, cls, 0, 0, kHbRfName[why], hp, dist);
            continue;
        }
        ghost = HbIsGhostType(type);
        if (ghost) {
            const BYTE *gw = *(const BYTE *const *)(w + 0x28);
            cd = gw ? *(const float *)(gw + 0x98) : 0.0f;
            gs = HbGhostState(type, cls, cd, pinReq == t);
            gone = gs == HB_GS_GONE;
        } else if (type == 0x10 && HbWalterKind(EnmModeWord(t)) != HB_WK_BODIES) {
            // v3.58 EN358: Walter downed (HP 0 included) keeps his bar and gets the revive bar of the game's down timer
            // (hudbars_sim.h HbWalterState); only his death or a script takes it away
            const int wk = HbWalterKind(EnmModeWord(t)), sites = HbWalterSites();
            const BYTE *wb = *(const BYTE *const *)(w + 0x24);
            const int tOk = (ULONG_PTR)wb >= 0x10000 && (sites & (wk == HB_WK_FINAL ? 2 : 1)) != 0;
            const int ai = *(const WORD *)(t + 0x410), step = *(const WORD *)(t + 0x412);
            gs = HbWalterState(wk, cls, ai, step, hp[0], tOk ? *(const float *)(wb + (wk == HB_WK_FINAL ? 0x80 : 0x94)) : 0.0f, tOk, &cd);
            gone = gs == HB_GS_GONE;
            walter = 1;
            if (gs != e->gstate && (gs == HB_GS_DOWN || gs == HB_GS_STUMBLE || e->gstate == HB_GS_DOWN) && LogQuota(&g_hbWalterQ, 40)) {
                static const char *const kGs[] = { "standing", "rising", "stumble", "down", "pinned", "gone" };
                LogV("hud bars: %s Walter %08X %s (class %X, AI %X step %d, hp %.0f / %.0f, down timer %.1f s): his bar %s", wk == HB_WK_FINAL ?
                     "the final" : "the stalker", e->id, kGs[gs], cls, ai, step, hp[0], hp[1], cd, gs == HB_GS_DOWN ? "and the revive bar stay up" :
                     gs == HB_GS_GONE ? "fades out" : "stays up");
            }
            e->gstate = gs;
            HbReviveStep(&e->rev, gs, cd, dt);
        } else {
            // dead, removed, scripted, or a monster at 0 health (its death plays): the bar fades out, the lost part draining
            gone = cls == 0x0B || cls >= 0x10 || !(hp[0] > 0.0f);
        }
        // v3.30 HB330: the bar is the health, always (RE_GHOST_HEALTH.md: a stumble keeps it, a real down is 0) (v3.52 HB352: frac,
        // the boss test and the distance are read above)
        if (ghost) {
            const int wasDown = e->gstate == HB_GS_DOWN || e->gstate == HB_GS_PINNED;
            if (gs == HB_GS_STUMBLE || gs == HB_GS_DOWN) {  // the rise preview (the get-up heal; none with an aura on it)
                const int off = HbGhostCountOff(type);
                const BYTE *tb = *(const BYTE *const *)(w + 0x24);
                const int okc = off >= 0 && (ULONG_PTR)tb >= 0x10000;
                const float r = HbGhostRiseHp(type, hp[0], hp[1], okc ? *(const int *)(tb + off) : 0, okc, hp[5]);
                e->rise = r > 0.0f ? r / hp[1] : -1.0f;
            } else if (gs != HB_GS_RISING || hp[0] >= e->rise * hp[1] - 0.5f) {
                e->rise = -1.0f;  // (rising: kept until the heal lands)
            }
            if ((gs == HB_GS_DOWN || gs == HB_GS_PINNED) && !wasDown) e->downShow = e->bar.alpha > 0.5f;  // seen going down
            if (gs == HB_GS_STAND || gs == HB_GS_GONE) e->downShow = 0;
            if (gs != e->gstate && g_hbGhostLog < 24) {
                static const char *const kGs[] = { "standing", "rising", "stumble", "down", "pinned", "gone" };
                ++g_hbGhostLog;
                Log("hud bars: ghost %X %s (class %X, hp %.0f / %.0f, gauge %.0f / %.0f, countdown %.1f s)", type, kGs[gs], cls,
                    hp[0], hp[1], hp[3], hp[4], cd);
            }
            e->gstate = gs;
            HbReviveStep(&e->rev, gs, cd, dt);
        }
        // the anchor: over the top bone, smoothed (animation bobbing)
        if (HbTop(w, pos, &top, &hidden)) top -= 40.0f;
        else top = pos[1] - HbTypeHeight(type);
        if (!e->yOk) e->y = top, e->yOk = 1;
        else e->y += (top - e->y) * (dt >= 0.12f ? 1.0f : dt / 0.12f);
        anc[0] = pos[0], anc[1] = e->y, anc[2] = pos[2];
        {
            const float mid[3] = { pos[0], 0.5f * (pos[1] + e->y), pos[2] };
            GvXform(anc, h);
            GvXform(mid, hm);
            okTop = GvScreen(h, vp, &tx, &ty);
            okMid = GvScreen(hm, vp, &mx, &my);
            // v3.52 HB352 (D1): a body whose top is above the 3D view while its middle is in it gets the bar at the top edge
            onScreen = HbBarPlace(okTop, tx, ty, okMid, mx, my, (float)vp->x, (float)vp->y, (float)vp->w, (float)vp->h, &sx, &sy);
            want = gone ? 0.0f : HbWantBoss(mode, boss, t == lock, e->bar.sinceHit, dist);  // v3.52 HB352 (D1): a boss's bar at any distance
            if (gs == HB_GS_DOWN && e->downShow && !gone) want = 1.0f;  // a ghost seen going down keeps its timer up
            if (ghost && HbPinShow(&e->rev)) want = 1.0f;  // v3.30 MERGE: the sealed bar 4 s after the pin (the user's choice)
            // in view: on screen, drawn, and not walled off (the bar or the body's middle visible; v3.52 HB352: a boss also by its
            // centre); the collision queries only for a bar that shows or wants to (a drop this frame is caught by the next frame's test)
            if (onScreen && !hidden && (want > 0.0f || e->bar.alpha > 0.0f || hp[0] < e->bar.shown * hp[1])) {
                seen = !HbBlocked(eye, anc) || !HbBlocked(eye, mid);
                if (!seen && boss) seen = !HbBlocked(eye, pos);
            }
        }
        HbBarStep(&e->bar, frac, want, seen, dt);
        if (boss)  // v3.52 HB352 (D1's proof): the boss's first bar of the room visit, or why it has none 5 s after it became active
            HbBossFrame(t, e->id, type, cls, HbStateDrawn(cls) && !gone, onScreen && e->bar.alpha > 0.004f,
                        gone ? "down, dead or scripted" : !onScreen ? "its bar's place is outside the 3D view" :
                        hidden ? "its model is not drawn (no-draw bit)" : !seen ? "walled off from the camera" : "fading in",
                        hp, dist);
        if (onScreen && e->bar.alpha > 0.004f && n < HB_MAXE) {
            HbDraw *d = &dl[n++];
            d->x = sx, d->y = sy - H * 0.012f, d->dist = dist;
            d->a = HbWithBody(e->bar.alpha, FiBodyShown(t, w, e->id)), d->shown = e->bar.shown, d->trail = e->bar.trail, d->heat = HbChipHeat(&e->bar);  // v3.58 EN358: the bar appears with the body (Fade In's fade)
            d->ghost = ghost;
            d->guard = ghost && hp[4] > 0.0f ? HbClamp01(hp[3] / hp[4]) : -1.0f;
            d->rise = ghost ? e->rise : -1.0f;
            d->revA = ghost || walter ? e->rev.alpha : 0.0f, d->revFrac = e->rev.frac, d->seal = e->rev.seal, d->flash = e->rev.flash;  // v3.58 EN358: + Walter
            d->locked = t == lock && !gone;
            d->boss = boss;
        }
    }
    for (i = 0; i < g_hbEN;)  // beings that left the list
        if (!g_hbE[i].alive) g_hbE[i] = g_hbE[--g_hbEN];
        else ++i;
    // the nearest HB_DRAW_MAX, drawn far to near (the nearest on top)
    for (i = 1; i < n; ++i) {
        HbDraw k = dl[i];
        int j = i - 1;
        while (j >= 0 && dl[j].dist < k.dist) dl[j + 1] = dl[j], --j;
        dl[j + 1] = k;
    }
    for (i = n > HB_DRAW_MAX ? n - HB_DRAW_MAX : 0; i < n; ++i) {
        const float W = HbBarW(H, dl[i].boss) + 2.0f * (dl[i].locked ? 10.0f * HbPx(H) + 0.004f * H : 0.0f);  // inside the 3D viewport
        if (dl[i].x - 0.5f * W < (float)vp->x + 4.0f) dl[i].x = (float)vp->x + 4.0f + 0.5f * W;
        if (dl[i].x + 0.5f * W > (float)(vp->x + vp->w) - 4.0f) dl[i].x = (float)(vp->x + vp->w) - 4.0f - 0.5f * W;
        if (dl[i].y < (float)vp->y + 0.02f * H) dl[i].y = (float)vp->y + 0.02f * H;
        HbBarQuads(&dl[i], H);
    }
}

// ---------------------------------------------------------------- the Curse Meter
typedef struct { DWORD magic, version; float meter, base, floor_; LONG latched, tier, tiers;  // PE's export head
                 LONG setSeq; float setValue; LONG doneSeq; float rate; LONG recovering;  // v3.41 CM341: version 1's tail ...
                 LONG ringOwner, ringMode, ringShown; float ringX, ringY, ringR; } HbPeCurse;  // ... version 2 (+0x34; read only then)
typedef char HbPeCurseV2At34[offsetof(HbPeCurse, ringOwner) == 0x34 && offsetof(HbPeCurse, ringR) == 0x48 ? 1 : -1];  // v3.41 CM341
static const volatile HbPeCurse *g_hbPe;
static DWORD g_hbPeLook;
static float g_hbPeTh[3] = { 0.35f, 0.50f, 1.0f };  // FitsFrom, CursedIdleFrom, full (Playable Eileen's defaults)
static FILETIME g_hbPeIniTime;
static DWORD g_hbPeIniCheck;
static int g_hbPeLock = 1;  // v3.32 MERGE: PE's [Ghost] LockCurse (its default 1: her curse held at 0)
static HbMeter g_hbMeter;
static int g_hbSrc = -1;   // the source drawn last: 0 companion, 1 Playable Eileen (logged when it changes)
static int g_hbStageLog = -1;
static void HbPeFind(void)  // Playable Eileen may load after this dll: looked for every 2 s until found
{
    const DWORD now = GetTickCount();
    HMODULE ds;
    if (g_hbPe || now - g_hbPeLook < 2000) return;
    g_hbPeLook = now;
    if ((ds = GetModuleHandleA("dsound.dll")) != NULL) {
        const volatile HbPeCurse *p = (const volatile HbPeCurse *)GetProcAddress(ds, "SH4PE_Curse");
        if (p && p->magic == 0x4D434853u && p->version >= 1) {
            g_hbPe = p;
            Log("hud bars: Playable Eileen's curse meter found (SH4PE_Curse, version %lu)", p->version);
        }
        HbPeOwnCheck(ds, g_hbPe != NULL);  // v3.41 CM341: also without a usable SH4PE_Curse (the function decides)
    }
}
// v3.41 CM341: HbCurseRings' peRing this frame (hudbars_sim.h HbPeRingState), from PE's SH4PE_Curse version 2 fields
static volatile LONG g_hbPeRingAQ;
static int HbPeRing(void)
{
    const int v2 = g_hbPe && g_hbPe->version >= 2;
    const int on = g_hbPeOnFn ? g_hbPeOnFn() != 0 : 1;
    if (!g_hbPeOwns) return HB_PR_NONE;
    if (v2 && g_hbPe->ringOwner != 1 && LogQuota(&g_hbPeRingAQ, 3))
        LogAnomaly("curse meter", "SH4PE_DrawsPlayerCurse() = 1 but SH4PE_Curse.ringOwner = %ld (1 expected): the function decides",
                   g_hbPe->ringOwner);
    return HbPeRingState(1, g_hbPe != NULL, g_hbPe ? (unsigned)g_hbPe->version : 0u, v2 ? g_hbPe->ringMode : 0,
                         v2 ? g_hbPe->ringShown : 0, on);
}
// v3.41 CM341: the small ring's centre (canvas; returns y, *x): its own place, or beside PE's ring while PE draws the
// player's (hudbars_sim.h HbRing2Place; a line when the place changes)
static float HbRing2At(int peRing, float *x)
{
    static int s_place = -1;
    static volatile LONG s_placeQ;
    float y;
    int v2, place;
    *x = HB_RING2_CX;
    if (peRing == HB_PR_NONE) return HB_RING2_CY;
    v2 = g_hbPe && g_hbPe->version >= 2;
    {
        const float rx = v2 ? g_hbPe->ringX : 452.0f, ry = v2 ? g_hbPe->ringY : 42.0f, rr = v2 ? g_hbPe->ringR : 17.0f;
        place = HbRing2Place(v2, rx, ry, rr, x, &y);
        if (place != s_place && LogQuota(&s_placeQ, 60))
            LogV("hud bars: companion Eileen's small ring at %.0f, %.0f (%s; Playable Eileen's ring at %.0f, %.0f R %.1f)", *x, y,
                 place == 1 ? "left of PE's ring" : place == 2 ? "right of PE's ring: no room on its left" :
                 v2 ? "its own place: PE's ring values are out of range" : "its own place: SH4PE_Curse below version 2", rx, ry, rr);
        if (!place && v2 && LogQuota(&g_hbPeRingAQ, 3))
            LogAnomaly("curse meter", "SH4PE_Curse ring %.1f, %.1f R %.2f is off the 512 x 448 canvas or not a number", rx, ry, rr);
    }
    s_place = place;
    return y;
}
static void HbPeIni(void)  // its [Curse] marks, read again when SH4PlayableEileen.ini changes (checked every 2 s)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    const DWORD now = GetTickCount();
    int fits, idle;  // v3.37 MN337: PE's 0..100 marks (was 0..1 floats)
    if (now - g_hbPeIniCheck < 2000 && g_hbPeIniCheck) return;
    g_hbPeIniCheck = now;
    if (!GetFileAttributesExA(g_peIni, GetFileExInfoStandard, &a) || !CompareFileTime(&a.ftLastWriteTime, &g_hbPeIniTime)) return;
    g_hbPeIniTime = a.ftLastWriteTime;
    // v3.37 MN337: Haunted from FitsFrom and Cursed from CursedIdleFrom whether PE's Fits / CursedIdle are on or not (v3.36
    // skipped Haunted with Fits=0); their defaults 35 / 50 when missing (HbPeStageMarks)
    fits = (int)GetPrivateProfileIntA("Curse", "FitsFrom", 35, g_peIni);
    idle = (int)GetPrivateProfileIntA("Curse", "CursedIdleFrom", 50, g_peIni);
    HbPeStageMarks(fits, idle, g_hbPeTh);
    g_hbPeLock = GetPrivateProfileIntA("Ghost", "LockCurse", 1, g_peIni) != 0;  // v3.32 MERGE: the small ring's LOCKED
    Log("hud bars: Playable Eileen's curse stages: Haunted from %.0f (FitsFrom; fits %s), Cursed from %.0f (CursedIdleFrom; "
        "possessed pose %s), Possessed at full 100; companion curse lock %d", g_hbPeTh[0] * 100.0f,
        GetPrivateProfileIntA("Curse", "Fits", 1, g_peIni) ? "on" : "off", g_hbPeTh[1] * 100.0f,
        GetPrivateProfileIntA("Curse", "CursedIdle", 1, g_peIni) ? "on" : "off", g_hbPeLock);  // v3.32 MERGE: + the lock
}
// companion Eileen: 1 = she exists (in the party, or her final-boss / idle task); *v = her shown curse; *away = the
// saved value (she is not spawned here) or her task in another room
static const BYTE *g_hbCompW;  // v3.35 CM335: her work while HbCompanion found her task alive (else NULL) ...
static DWORD g_hbCompFn;        // ... and its task function
static int HbCompanion(float *v, int *away)
{
    const BYTE *t = *(const BYTE *const *)0xFCE6B4, *w = NULL;
    const DWORD fn = t ? *(const DWORD *)(t + 0x404) : 0;
    const int party = *(volatile int *)0xFD5BC4 != 0;
    *away = 0;
    g_hbCompW = NULL, g_hbCompFn = 0;  // v3.35 CM335
    if (t && (fn == 0x44ADE0 || fn == 0x44B110 || fn == 0x44B3D0) && (w = *(const BYTE *const *)(t + 0x408)) != NULL &&
        *(const WORD *)(t + 0x410) && *(void *const *)w && *(void *const *)(w + 4)) {
        const float c = *(const float *)(w + 0x0C);  // curse_shown: what every reader of the game uses
        if (fn == 0x44ADE0 && !party) return 0;      // a task without party membership: not with the player
        if (!(c >= 0.0f && c <= 1.0f)) return 0;
        *v = c;
        *away = fn == 0x44ADE0 && *(const int *)(w + 0xF0) != 0;  // in another room (her curse does not move there)
        g_hbCompW = w, g_hbCompFn = fn;  // v3.35 CM335
        return 1;
    }
    if (party) {
        const float c = *(volatile const float *)0xFCE810;  // EileenRecord +0x20 (the saved live curse)
        if (!(c >= 0.0f && c <= 1.0f)) return 0;
        *v = c;
        *away = 1;
        return 1;
    }
    return 0;
}
// v3.30 HB330: the meter is a ring of thorns around a sigil (design R of DESIGN.md), top right: a 270-degree band
// filling clockwise with dark crimson, cut and thorned at each stage mark (a thorn lights when passed), an eye in the
// middle that opens with the stage; a ring swells out when a stage is crossed (red up, cold down); Possessed beats.
static const DWORD kHbSeg[HB_STAGES][2] = {  // segment colours by stage (top / light, bottom / dark): ash rose .. blood
    { 0x7A5864u, 0x34222Au }, { 0x9A3050u, 0x420E20u }, { 0xB01A30u, 0x500610u }, { 0xD0182Cu, 0x640812u } };
static const DWORD kHbGlow[HB_STAGES] = { 0xC8A8B4u, 0xF06A8Cu, 0xFF5060u, 0xFF3040u };
// v3.35 CM335: the stage name in the stroke font (0xRRGGBB): the small ring's pale bone, tinted toward the stage
static const DWORD kHbStageRgb[HB_STAGES] = { 0xC6BCC2u, 0xD0A8C6u, 0xDC8EAEu, 0xEC7478u };
static const char *const kHbStageTag[HB_STAGES] = { "CLEAR", "HAUNTED", "CURSED", "POSSESSED" };  // v3.37 MN337: UNTOUCHED -> CLEAR
#define HB_RING_CX 452.0f  // canvas (512 x 448, the 4:3 interface area): the ring's centre ...
#define HB_RING_CY 42.0f
#define HB_RING_R  17.0f   // ... its outer radius and band width in canvas heights (a circle at any squeeze)
#define HB_RING_BW 3.6f
// v3.38 HB338 (USER_FEEDBACK_v337 item 1: both meters hard to see on a dark background): renders before / after on a
// near-black, a mid-grey and a light backdrop: tools/hudbars_mockup.py --v338 (staging/v338_feedback/w1_hud/mockups)
#define HB_HAIR_RGB      0xCDBFC6u  // the bone hairlines outside and inside the band (HB_TAG_RGB's bone) ...
#define HB_HAIR_A        0.30f      // ... at this x the ring's strength
#define HB_THORN_OFF_RGB 0x8E8496u  // an unlit thorn (v3.37 0x6A6070)
static float g_hbLabelA;  // the stage name's strength (v3.35 CM335: drawn with the ring, HbTinyText)
static HbMeter g_hbMeter2;  // v3.32 DHS332: the companion's small ring (Both)
static int g_hbSrc2 = -1;   // v3.32 DHS332: the small ring drawn last (1) or not (0): logged when it changes
static int g_hbStageLog2 = -1;
static float g_hbLockK;        // v3.32 MERGE: the small ring's grey, 0 normal .. 1 locked (eased, HbLockStep)
static int g_hbLockLog = -1;   // v3.32 MERGE: the locked state last logged
// One ring of thorns (v3.30 design R; v3.32 DHS332: drawn for the main ring and the companion's small one). R / bw = the
// band's outer radius and width (pixels), k = the size scale of the pixel-sized parts (1 = the main ring: exactly the
// v3.31 drawing), a = its strength; th / minor / stage as HbCurse's. The meter's pulse comes from m.
static void HbRingDraw(const HbMeter *m, float cx, float cy, float R, float bw, float k, float px, float v, const float th[3],
                       float minor, int stage, float a)
{
    const float A0 = -2.3561945f, SPAN = 4.712389f;  // from 7:30 o'clock, 270 degrees clockwise
    const float r1 = R, r0 = R - bw, seg = 0.105f / k;
    const float beat = stage == 3 ? HbBeat(g_hbNow) : 0.0f, pulse = m->pulse;
    const float plate = 5.0f * k * px > 3.0f * px ? 5.0f * k * px : 3.0f * px;  // v3.38 HB338
    int i;
    // a soft dark plate behind (v3.38 HB338: 0.55a at the centre, 0.45a at R + 2 px, 0 at R + 5k px; was 0.35a -> 0 at
    // R + 3k px), the rim, a bone hairline outside it (it carries the ring on black; the rim on light walls), the empty
    // band (v3.38 HB338: a dusky track 0x3A2630 / 0x241A20 at 0.85a, visible on black; was near-black at 0.80a)
    HbDisc(cx, cy, R + 2.0f * px, HbArgb(HB_RIM, 0.55f * a), HbArgb(HB_RIM, 0.45f * a));
    HbRing(cx, cy, R + 2.0f * px, R + plate, -3.1415927f, 3.1415927f, HbArgb(HB_RIM, 0.45f * a), HbArgb(HB_RIM, 0.0f), seg);
    HbRing(cx, cy, r0 - px, r1 + px, A0 - 0.03f, A0 + SPAN + 0.03f, HbArgb(HB_RIM, 0.85f * a), HbArgb(HB_RIM, 0.85f * a), seg);
    HbRing(cx, cy, r1 + px, r1 + 2.0f * px, A0 - 0.03f, A0 + SPAN + 0.03f, HbArgb(HB_HAIR_RGB, HB_HAIR_A * a),
           HbArgb(HB_HAIR_RGB, HB_HAIR_A * a), seg);
    HbRing(cx, cy, r0, r1, A0, A0 + SPAN, HbArgb(0x3A2630u, 0.85f * a), HbArgb(0x241A20u, 0.85f * a), seg);
    {   // the fill, piece by piece in the colour of the stage it belongs to; a source whose last stage is "full"
        // (Playable Eileen: 100, latched) turns the whole fill to that colour when it is reached
        float bs[5];
        int bst[5], nb = 1, j;
        bs[0] = 0.0f, bst[0] = 0;
        for (i = 0; i < 3; ++i)  // the marks inside the band, rising (a mark out of order in an edited ini is left out)
            if (th[i] > bs[nb - 1] && th[i] < 1.0f) bs[nb] = th[i], bst[nb++] = i + 1;
        bs[nb] = 1.0f;
        for (j = 0; j < nb; ++j) {
            const float e = v < bs[j + 1] ? v : bs[j + 1];
            const int ss = stage == 3 && !(th[2] < 1.0f) ? 3 : bst[j];
            // v3.38 HB338: lifted: x 1.15 (was 0.95, capped at 1) and the inner colour 35 % toward the outer one
            const float kk0 = 1.15f * a * (1.0f + 0.25f * beat), kk = kk0 < 1.0f ? kk0 : 1.0f;
            const DWORD c0 = HbLerpRgb(kHbSeg[ss][0], kHbGlow[ss], 0.25f * beat + 0.35f * pulse);
            const DWORD c1 = HbLerpRgb(kHbSeg[ss][1], kHbSeg[ss][0], 0.35f);
            if (e > bs[j]) HbRing(cx, cy, r0, r1, A0 + SPAN * bs[j], A0 + SPAN * e, HbArgb(c1, kk), HbArgb(c0, kk), seg);
        }
    }
    if (v > 0.004f) {  // the light outer row and the leading edge
        const float ang = A0 + SPAN * v;
        HbRing(cx, cy, r1 - px, r1, A0, ang, HbArgb(HB_SHEEN, 0.18f * a), HbArgb(HB_SHEEN, 0.18f * a), seg);
        HbRing(cx, cy, r0 - px, r1 + px, ang - 0.035f, ang, HbArgb(0xFFE0E8u, 0.80f * a), HbArgb(0xFFE0E8u, 0.80f * a), seg);
    }
    for (i = 0; i < 3; ++i) {  // a thorn at each stage mark, pointing out (lit when passed), and a cut through the band
        const float mk = th[i];
        float ang, sn, cs, L, base, wv, ta;
        DWORD col;
        if (!(mk > 0.0f && mk < 1.0f)) continue;
        ang = A0 + SPAN * mk, sn = sinf(ang), cs = -cosf(ang);
        L = HbThornLen(R, px, k), base = r1 + 0.5f * px, wv = k * 1.6f * px + 0.04f * R;
        col = v >= mk ? kHbGlow[i + 1] : HB_THORN_OFF_RGB;  // v3.38 HB338: unlit 0x8E8496 at 1.0a (was 0x6A6070 at 0.9a)
        ta = v >= mk ? 0.9f * a : a;
        HbTri(cx + sn * base - cs * wv, cy + cs * base + sn * wv, HbArgb(col, ta),
              cx + sn * base + cs * wv, cy + cs * base - sn * wv, HbArgb(col, ta),
              cx + sn * (base + L), cy + cs * (base + L), HbArgb(HbLerpRgb(col, 0xFFFFFFu, 0.3f), ta));
        HbRing(cx, cy, r0, r1, ang - 0.018f, ang + 0.018f, HbArgb(HB_RIM, 0.9f * a), HbArgb(HB_RIM, 0.9f * a), seg);
    }
    {   // the sigil: a dark well with an eye of the stage colour that opens with the stage
        static const float kOpen[HB_STAGES] = { 0.15f, 0.40f, 0.70f, 1.0f };
        const DWORD ec = kHbGlow[stage];
        const float ew = (r0 - 4.0f * k * px) * 0.85f, eh = ew * (0.18f + 0.42f * kOpen[stage]);
        float glow = (0.45f + 0.35f * kOpen[stage]) * a * (1.0f + 0.6f * beat + 0.8f * pulse), core;
        const float ex[4] = { cx, cx + ew, cx - ew, cx }, ey[4] = { cy - eh, cy, cy, cy + eh };
        DWORD ecc[4];
        if (glow > 1.0f) glow = 1.0f;
        core = glow * 1.4f > 1.0f ? 1.0f : glow * 1.4f;
        ecc[0] = ecc[3] = HbArgb(HbLerpRgb(ec, 0x000000u, 0.4f), glow), ecc[1] = ecc[2] = HbArgb(ec, 0.0f);
        HbDisc(cx, cy, r0 - 2.0f * k * px, HbArgb(0x0C080Au, 0.80f * a), HbArgb(0x1A1014u, 0.80f * a));
        // v3.38 HB338: the inner bone hairline over the well's edge, and the minor mark (moved here, after the well, at
        // 0.70a and reaching 1.5 px further in, so it shows over the hairline; was 0.4a before the well)
        HbRing(cx, cy, r0 - 2.0f * px, r0 - px, A0 - 0.03f, A0 + SPAN + 0.03f, HbArgb(HB_HAIR_RGB, HB_HAIR_A * a),
               HbArgb(HB_HAIR_RGB, HB_HAIR_A * a), seg);
        if (minor > 0.0f) {  // the minor mark (inside the band)
            const float ang = A0 + SPAN * minor;
            HbRing(cx, cy, r0 - 3.5f * px, r0 - px, ang - 0.03f, ang + 0.03f, HbArgb(0xCFC4D8u, 0.70f * a),
                   HbArgb(0xCFC4D8u, 0.70f * a), seg);
        }
        HbQuad(ex, ey, ecc);
        HbDisc(cx, cy, eh * 0.55f > 1.5f * k * px ? eh * 0.55f : 1.5f * k * px, HbArgb(HbLerpRgb(ec, 0xFFFFFFu, 0.35f), core),
               HbArgb(ec, 0.6f * glow));
    }
    if (pulse > 0.0f) {  // the stage crossing: a ring that swells out and fades
        const float rr = R + (2.0f + 16.0f * (1.0f - pulse)) * k * px, pa = 0.85f * pulse * a;
        const DWORD pc = m->pulseUp ? kHbGlow[stage] : 0xB8C8E0u;
        HbRing(cx, cy, rr - 2.5f * k * px, rr, -3.1415927f, 3.1415927f, HbArgb(pc, 0.0f), HbArgb(pc, pa), 0.14f / k);
        HbRing(cx, cy, rr, rr + 2.5f * k * px, -3.1415927f, 3.1415927f, HbArgb(pc, pa), HbArgb(pc, 0.0f), 0.14f / k);
    }
}
// v3.32 DHS332: the small ring's label in the stroke font of hudbars_sim.h (kHbGlyph), centred on cx, its top at top, u
// pixels a grid unit; a dark copy one pixel down-right first
#define HB_TAG_RGB 0xCDBFC6u  // pale bone with a rose cast
#define HB_LOCK_RGB 0xB8C4D0u  // v3.32 MERGE: the LOCKED tag, a cold steel grey
static void HbTinyText(float cx, float top, float u, const char *t, DWORD rgb, float a, float px)
{
    const float x0 = cx - 0.5f * HbTinyWidth(t) * u;
    int pass;
    for (pass = 0; pass < 2; ++pass) {
        const float o = pass ? 0.0f : px;
        const DWORD c = pass ? HbArgb(rgb, 0.92f * a) : HbArgb(0x000000u, 0.70f * a), cc[4] = { c, c, c, c };
        const char *p;
        float x = x0;
        for (p = t; *p; ++p) {
            const HbGlyphDef *g = HbGlyphOf(*p);
            int s;
            if (!g) continue;
            for (s = 0; s < g->n; ++s) {
                const HbStroke *k = &g->s[s];
                const float X0 = x + k->x0 * u + o, X1 = x + k->x1 * u + o, Y0 = top + k->y0 * u + o, Y1 = top + k->y1 * u + o;
                const float d = k->slant * u, qx[4] = { X0, X1, X0 + d, X1 + d }, qy[4] = { Y0, Y0, Y1, Y1 };
                HbQuad(qx, qy, cc);
            }
            x += (float)(g->adv + 1) * u;
        }
    }
}
// ---------------------------------------------------------------- v3.35 CM335: where her curse is heading
// The target (hudbars_sim.h HbCurseTarget = 0x43E790 in C): her task alive (HbCompanion: g_hbCompW) = the target in
// force w+0x10 (0x43EC20 set it at her spawn from the same formula; it also shows what a script holds); in the party
// without a task = the formula over her record as it stands, and while she is left behind (party mode 2) the target at
// her next spawn (one more door, HbCurseAfterExit). Playable Eileen's [Ghost] LockCurse=1 holds live, shown, target and
// the saved curse at 0 and makes 0x43E790 return 0: the target is 0 then. Read only: no game function is called.
typedef struct { float target, next, formula; int haveNext, mode, danger, alone; } HbTgt;
static HbNotch g_hbNotch, g_hbNotch2;  // the main ring (when it shows her) / Both's small ring
static HbCurseRec HbRecRead(void)
{
    const BYTE *r = (const BYTE *)0xFCE7F0;
    HbCurseRec c;
    c.damage = *(volatile const float *)(r + 0x24);
    c.danger = *(volatile const BYTE *)(r + 0x2D);
    c.dangerTotal = *(volatile const WORD *)(r + 0x2E);
    c.rooms = *(volatile const WORD *)(r + 0x30);
    c.spawned = *(volatile const WORD *)(r + 0x32);
    c.collapses = *(volatile const WORD *)(r + 0x34);
    c.flinches = *(volatile const WORD *)(r + 0x36);
    c.frames = *(volatile const int *)(r + 0x38);
    c.nearFrames = *(volatile const int *)(r + 0x3C);
    return c;
}
static volatile LONG g_hbTgtQ, g_hbTgtXQ, g_hbTgtAQ;
static float g_hbTgtLogT = -9.0f, g_hbTgtLogN = -9.0f, g_hbTgtMis;
static int g_hbTgtLogMode = -9, g_hbTgtLogDanger = -9, g_hbTgtLogTask = -9, g_hbTgtLogAlone = -9;
static const BYTE *g_hbTgtXW;  // the work whose target was checked against the formula (once per spawn) ...
static DWORD g_hbTgtXRoom;     // ... and its room (a new task may reuse the address)
// v3.35 CM335: away = HbCompanion's (no task here, or her task in a neighbour room); shown = the curse her ring shows
static void HbTargetRead(HbTgt *o, int away, float shown, float dt)
{
    const HbCurseRec rec = HbRecRead();
    const int peLocked = g_hbPe != NULL && g_hbPeLock;
    o->mode = *(volatile const int *)0xFD5BC4;
    o->danger = rec.danger;
    o->formula = HbCurseTarget(&rec);
    o->haveNext = 0, o->next = -1.0f;
    if (peLocked) {
        o->target = 0.0f;
    } else if (g_hbCompW) {
        const float t = *(volatile const float *)(g_hbCompW + 0x10);
        o->target = t >= 0.0f && t <= 1.0f ? t : -1.0f;
    } else {
        o->target = o->formula >= 0.0f && o->formula <= 1.0f ? o->formula : -1.0f;
        if (HbShowNext(o->mode, 0)) {
            const HbCurseRec nx = HbCurseAfterExit(rec);
            o->next = HbCurseTarget(&nx);
            o->haveNext = o->next >= 0.0f && o->next <= 1.0f;
        }
    }
    o->alone = HbLeftAlone(o->mode, away, rec.danger);
    if (rec.danger > 2 && LogQuota(&g_hbTgtAQ, 3))
        LogAnomaly("curse meter", "companion Eileen's room danger +0x2D = %d (0..2 expected; 0x43F8E0)", rec.danger);
    if (o->formula != o->formula && rec.rooms >= 10 && LogQuota(&g_hbTgtAQ, 3))
        LogAnomaly("curse meter", "companion Eileen's record has %d room exits and 0 frames: the game's target (0x43E790) is "
                   "NaN at her next spawn", rec.rooms);
    // the formula against the game's own value, once per spawn (her task, not locked); a mismatch held 1 s is logged
    if (g_hbCompW && g_hbCompFn == 0x44ADE0 && !peLocked && o->target >= 0.0f && o->formula == o->formula) {
        const float d = fabsf(o->target - o->formula);
        g_hbTgtMis = d > 0.002f ? g_hbTgtMis + dt : 0.0f;
        if ((g_hbTgtXW != g_hbCompW || g_hbTgtXRoom != g_hbRoom) && (d <= 0.002f || g_hbTgtMis >= 1.0f)) {
            g_hbTgtXW = g_hbCompW, g_hbTgtXRoom = g_hbRoom;
            if (LogQuota(&g_hbTgtXQ, 200))
                LogV("hud bars: companion target check: the game's w+0x10 %.4f, 0x43E790's formula over her record %.4f%s "
                     "(rooms %d, spawned %d, danger total %d, collapses %d, flinches %d, damage %.0f, near %d / %d frames)",
                     o->target, o->formula, d <= 0.002f ? ": the same" : ": DIFFERENT (a script holds it, or her record "
                     "changed after her spawn)", rec.rooms, rec.spawned, rec.dangerTotal, rec.collapses, rec.flinches,
                     rec.damage, rec.nearFrames, rec.frames);
        }
    }
    // the target, the next-spawn value, the party mode, the danger: a line when one changes (rate-limited)
    if (fabsf(o->target - g_hbTgtLogT) > 0.0005f || fabsf(o->next - g_hbTgtLogN) > 0.0005f || o->mode != g_hbTgtLogMode ||
        o->danger != g_hbTgtLogDanger || (g_hbCompW != NULL) != g_hbTgtLogTask || o->alone != g_hbTgtLogAlone) {
        static const char *const kSrc[3] = { "her target in force, w+0x10", "0x43E790's formula over her record",
                                             "held at 0 by Playable Eileen's LockCurse" };
        if (LogQuota(&g_hbTgtQ, 400))
            LogV("hud bars: companion curse target %.3f (%s), shown %.3f; party mode %d, her room's danger %d%s", o->target,
                 kSrc[peLocked ? 2 : g_hbCompW ? 0 : 1], shown, o->mode, o->danger,
                 o->haveNext ? (o->alone ? ": left alone with monsters, the next door raises it" : ": left behind") :
                 o->alone ? ": left alone with monsters" : "");
        if (o->haveNext && LogQuota(&g_hbTgtQ, 400))
            LogV("hud bars: companion curse target at her next spawn %.3f (one more door: rooms %d -> %d, danger total %d -> %d)",
                 o->next, rec.rooms, (rec.rooms + 1) & 0xFFFF, rec.dangerTotal, (rec.dangerTotal + rec.danger) & 0xFFFF);
        g_hbTgtLogT = o->target, g_hbTgtLogN = o->next, g_hbTgtLogMode = o->mode, g_hbTgtLogDanger = o->danger;
        g_hbTgtLogTask = g_hbCompW != NULL, g_hbTgtLogAlone = o->alone;
    }
}
#define HB_NOTCH_RGB 0xF0E4ECu  // the notch: pale bone (the fill's leading edge is a warmer 0xFFE0E8)
#define HB_NEXT_RGB  0xB8C8E0u  // the next-spawn mark: cold (the "down" pulse's colour)
#define HB_CLAW_RGB  0xFF3A4Au  // the glyph's claw marks: blood (v3.38 HB338: brighter, was 0xD02838)
// the notch and the next mark on a ring (HbRingDraw's geometry: R / bw pixels, k the size scale). v3.38 HB338: a = the
// alert strength (HbAlertQ x Hide Gauge's k: not dimmed by away, at least 0.85 of the ring's fade-in); the notch's cut
// 0.85 / 0.95 (was 0.62 / 0.72), its dark edges 0.75 (0.55), its pointer 0.90 (0.65); the next mark 0.85 / 0.95 (0.55 /
// 0.70) and wider, its arc 0.55 (0.30) moved just outside the outer hairline (r1 + 2 .. 3.5 px)
static void HbNotchDraw(float cx, float cy, float R, float bw, float k, float px, const HbNotch *n, float a)
{
    const float A0 = -2.3561945f, SPAN = 4.712389f, r1 = R, r0 = R - bw, rm = R - 0.5f * bw, seg = 0.105f / k;
    if (n->nextA * a > 0.004f) {  // first, under the notch: a thin arc outside the band to the next mark, and the mark
        const float q = n->nextA * a, an = A0 + SPAN * HbClamp01(n->next), ap = A0 + SPAN * HbClamp01(n->pos);
        const float hw = (0.9f + 0.4f * k) * px / rm, ro = r1 + 3.5f * px;
        HbRing(cx, cy, r1 + 2.0f * px, ro, an < ap ? an : ap, an < ap ? ap : an, HbArgb(HB_NEXT_RGB, 0.55f * q),
               HbArgb(HB_NEXT_RGB, 0.55f * q), seg);
        HbRing(cx, cy, rm - px, ro + px, an - hw - px / rm, an + hw + px / rm, HbArgb(HB_RIM, 0.65f * q),
               HbArgb(HB_RIM, 0.65f * q), seg);
        HbRing(cx, cy, rm, ro, an - hw, an + hw, HbArgb(HB_NEXT_RGB, 0.85f * q), HbArgb(HB_NEXT_RGB, 0.95f * q), seg);
    }
    if (n->a * a > 0.004f) {  // the notch: a pale cut across the band (dark edges) and a small pointer inside it
        const float q = n->a * a, ang = A0 + SPAN * HbClamp01(n->pos), sn = sinf(ang), cs = -cosf(ang);
        const float hw = (0.55f + 0.35f * k) * px / rm, ho = hw + px / rm;
        const float tl = (1.5f + 2.0f * k) * px, tw = (1.0f + 1.2f * k) * px, tb = r0 - 0.5f * px;
        HbRing(cx, cy, r0 - px, r1 + px, ang - ho, ang + ho, HbArgb(HB_RIM, 0.75f * q), HbArgb(HB_RIM, 0.75f * q), seg);
        HbRing(cx, cy, r0, r1, ang - hw, ang + hw, HbArgb(HB_NOTCH_RGB, 0.85f * q), HbArgb(HB_NOTCH_RGB, 0.95f * q), seg);
        HbTri(cx + sn * tb, cy + cs * tb, HbArgb(HB_NOTCH_RGB, 0.90f * q),
              cx + sn * (tb - tl) - cs * tw, cy + cs * (tb - tl) + sn * tw, HbArgb(HB_NOTCH_RGB, 0.12f * q),
              cx + sn * (tb - tl) + cs * tw, cy + cs * (tb - tl) - sn * tw, HbArgb(HB_NOTCH_RGB, 0.12f * q));
    }
}
// the left-alone glyph left of a ring (R pixels, Rc canvas heights): her figure (a head and a long coat) in pale bone with
// claw marks beside it (two for danger 1, three for danger 2), on a soft dark spot; it breathes slowly (2.4 s).
// v3.38 HB338: strength = HbAlertQ(aNoAway x the breath, the ring's fade-in) x k (Hide Gauge): never dimmed by away, at
// least 0.85; the spot 0.70 (0.50), the figure 1.0 / 0.85 (0.90 / 0.70), the claws 0.60 .. 1.0 (0.25 .. 0.90) and wider
// (half width 0.09 s + 0.5 px, was 0.07 s + 0.25), each over a black copy one pixel larger (it reads on light walls too)
static void HbAloneDraw(float cx, float cy, float R, float Rc, float sy, float px, const HbNotch *n, float aNoAway,
                        float meterAlpha, float k)
{
    const float q = n->alone * HbAlertQ(aNoAway * (0.80f + 0.20f * sinf(g_hbNow * 2.618f)), meterAlpha) * HbClamp01(k);
    const float s = HbAloneSize(Rc) * sy, gx = cx - R - HbAloneGap(Rc) * sy, gy = cy;
    const float fx = gx + 0.15f * s;
    int i, pass;
    if (!(q > 0.004f)) return;
    HbDisc(gx - 0.45f * s, gy, 1.45f * s, HbArgb(HB_RIM, 0.70f * q), HbArgb(HB_RIM, 0.0f));
    HbDisc(fx, gy - 0.58f * s, 0.24f * s, HbArgb(HB_TAG_RGB, q), HbArgb(HB_TAG_RGB, q));
    {
        const float x[4] = { fx - 0.22f * s, fx + 0.22f * s, fx - 0.36f * s, fx + 0.36f * s };
        const float y[4] = { gy - 0.28f * s, gy - 0.28f * s, gy + 0.85f * s, gy + 0.85f * s };
        const DWORD ct = HbArgb(HB_TAG_RGB, q), cb = HbArgb(HB_TAG_RGB, 0.85f * q), c[4] = { ct, ct, cb, cb };
        HbQuad(x, y, c);
    }
    for (i = 0; i < (n->danger >= 2 ? 3 : 2); ++i)
        for (pass = 0; pass < 2; ++pass) {  // the black copy one pixel larger, then the claw
            const float x0 = gx - 0.62f * s - (float)i * 0.34f * s, w = 0.09f * s + 0.5f * px, o = pass ? 0.0f : px;
            const float x[4] = { x0 + 0.20f * s - w - o, x0 + 0.20f * s + w + o, x0 - 0.20f * s - w - o, x0 - 0.20f * s + w + o };
            const float y[4] = { gy - 0.62f * s - o, gy - 0.62f * s - o, gy + 0.62f * s + o, gy + 0.62f * s + o };
            const DWORD c0 = pass ? HbArgb(HB_CLAW_RGB, 0.60f * q) : HbArgb(HB_RIM, 0.75f * q);
            const DWORD c1 = pass ? HbArgb(HB_CLAW_RGB, q) : HbArgb(HB_RIM, 0.75f * q), c[4] = { c0, c0, c1, c1 };
            HbQuad(x, y, c);
        }
}

// ---------------------------------------------------------------- v3.38 HB338: Hide Gauge (hudbars_sim.h HbHideStep)
static HbHide g_hbHide, g_hbHide2;   // the main ring's / the small ring's own timer
static int g_hbGateShut;             // the HUD gate [0x1079C30] was seen closed since the last drawn frame
static int g_hbHideLog = -1;         // Hide Gauge's state last logged
static int g_hbShownLog[2] = { -1, -1 };  // each ring shown (1) / faded out (0) as last logged (-1: Hide Gauge off)
static volatile LONG g_hbHideQ, g_hbHideAQ;
// LogV when a ring fades out or comes back under Hide Gauge, with what brought it back (rate-limited: 300 a session)
static void HbHideNote(int ring, int hideOn, float k, const HbHide *hd, int meterWoke, int notchWoke, int lockWake, int hold,
                       float fade)
{
    const int shown = k > 0.0f;
    char why[160];
    if (!hideOn) {
        g_hbShownLog[ring] = -1;
        return;
    }
    if (shown == g_hbShownLog[ring]) return;
    if (g_hbShownLog[ring] >= 0 && LogQuota(&g_hbHideQ, 300)) {
        why[0] = 0;
        if (shown) {
            strcpy_s(why, sizeof why, ": woken by");
            if (meterWoke) strcat_s(why, sizeof why, " its value (appeared or a stage crossed)");
            if (notchWoke & HB_NW_NOTCH) strcat_s(why, sizeof why, " her target notch");
            if (notchWoke & HB_NW_NEXT) strcat_s(why, sizeof why, " the next-spawn mark");
            if (notchWoke & HB_NW_ALONE) strcat_s(why, sizeof why, " the left-alone glyph");
            if (lockWake) strcat_s(why, sizeof why, " the lock");
            if (hold) strcat_s(why, sizeof why, " Possessed (held up)");
            if (!meterWoke && !notchWoke && !lockWake && !hold) strcpy_s(why, sizeof why, ": with the health gauge");
        } else {
            strcpy_s(why, sizeof why, hd->asleep ? ": hidden with the HUD (pause, event)" : ": 5 s without a change, then 1 s");
        }
        LogV("hud bars: Hide Gauge: %s %s%s (gauge fade %.2f)", ring ? "companion Eileen's small ring" : "the curse meter",
             shown ? "shown" : "faded out", why, fade);
    }
    g_hbShownLog[ring] = shown;
}

static void HbCurse(int cm, float dt)
{
    const float w = FxW(), h = FxH(), S = SH4WM_Scale2D(), sx = w / (512.0f * (S > 0.0f ? S : 1.0f)), sy = h / 448.0f;
    float v = 0.0f, th[3], minor = -1.0f, cx, cy, px, a;
    float k1 = 1.0f, k2 = 1.0f, fade, aw;  // v3.38 HB338
    int hideOn, lockWake = 0;            // v3.38 HB338
    int present, away = 0, src, stage, second, peMain, mainRing, peRing;  // v3.41 CM341: + mainRing, peRing
    g_hbLabelA = 0.0f;
    HbPeFind();
    if (g_hbPe) HbPeIni();  // v3.35 CM335: its LockCurse also decides the companion's notch (every mode)
    peRing = HbPeRing();    // v3.41 CM341: HB_PR_NONE without SH4PE_DrawsPlayerCurse (as before)
    // v3.42 SM342: cm is the row's Off / On; On asks v3.41's Companion rule (peMain is never 1: no player ring from Refined)
    mainRing = HbCurseRings(HbCmvMode(cm), g_hbPe != NULL, peRing, &peMain, &second);
    {   // v3.41 CM341: while PE draws the player's ring, where companion Eileen's ring is (a line when it changes)
        static int s_lay = -1;
        const int lay = peRing == HB_PR_NONE ? -1 : !mainRing && !second ? 0 : mainRing ? 1 : 2;
        static volatile LONG s_layQ;
        if (lay != s_lay && lay >= 0 && LogQuota(&s_layQ, 60))
            LogV("hud bars: CurseMeter %s with Playable Eileen drawing the player's ring (%s): %s", kHbCmvShown[cm == HB_CMV_ON],
                 peRing == HB_PR_SHOWN ? "its ring can show" : "its ring cannot show now: Off or [Curse] Meter=0",
                 lay == 0 ? "no ring from Refined" : lay == 1 ? "companion Eileen's ring at the main place" :
                 "companion Eileen's small ring beside PE's ring");
        s_lay = lay;
    }
    // v3.42 SM342: v3.37's "a saved Player / Both acts as Companion" lines are gone (no Player / Both any more)
    src = peMain && g_hbPe && !g_hbPeOwns;  // v3.41 CM341: never PE's curse while PE draws the player's ring (HbCurseRings)
    if (src) {
        present = 1;
        v = g_hbPe->latched ? 1.0f : g_hbPe->meter / 100.0f;
        memcpy(th, g_hbPeTh, sizeof th);
    } else {
        present = mainRing && HbCompanion(&v, &away);  // v3.41 CM341: + mainRing (no main ring: it fades out)
        th[0] = 0.25f, th[1] = 0.50f, th[2] = 0.70f;  // EileenCurseTier (0x43E930 / 0x43E980)
        minor = 0.40f;                                // Apartment World's second visit reads live > 0.4 (0x44C620)
    }
    v = HbClamp01(v);
    {   // v3.35 CM335: the main ring shows her: where her curse is heading (else its notch fades)
        HbTgt tg;
        if (!src && present) {
            HbTargetRead(&tg, away, v, dt);
            HbNotchStep(&g_hbNotch, tg.target, v, tg.next, tg.haveNext, tg.alone, dt);
        } else {
            HbNotchStep(&g_hbNotch, -1.0f, v, -1.0f, 0, 0, dt);
        }
    }
    stage = HbCurseStage(v, th);
    if (src != g_hbSrc && present) {
        g_hbSrc = src;
        Log("hud bars: curse meter shows %s (%.2f, stage %s)", src ? "Playable Eileen's own curse" : "companion Eileen's curse",
            v, kHbStageName[stage]);
    }
    if (present && stage != g_hbStageLog) {
        if (g_hbStageLog >= 0) Log("hud bars: curse stage %s -> %s (%.3f)", kHbStageName[g_hbStageLog], kHbStageName[stage], v);
        g_hbStageLog = stage;
    }
    HbMeterStep(&g_hbMeter, v, stage, present, dt);
    px = HbPx(h);
    // v3.38 HB338: Hide Gauge (Option 3, [0x1091AD9]) fades the meters with the health gauge (hudbars_sim.h HbHideStep)
    hideOn = CFG[9] != 0;
    fade = *(volatile const float *)0x653B8C;
    if (hideOn != g_hbHideLog) {
        Log("hud bars: Hide Gauge %s: the curse meter %s", hideOn ? "on" : "off", hideOn ? "fades with the health gauge "
            "(5 s without a change of its own, then 1 s; up while Possessed)" : "stays shown");
        g_hbHideLog = hideOn;
    }
    if (g_hbGateShut) {  // the HUD was hidden since the last drawn frame (pause, event, pickup): the gauge comes back faded
        if (hideOn) HbHideSleep(&g_hbHide), HbHideSleep(&g_hbHide2);
        g_hbGateShut = 0;
    }
    if (hideOn && !(fade >= -0.001f && fade <= HB_GAUGE_FULL + 0.001f) && LogQuota(&g_hbHideAQ, 3))
        LogAnomaly("curse meter", "the health gauge's fade [0x653B8C] reads %.3f (0 .. 0.7 expected): the meters follow their own timer", fade);
    {
        const int wake1 = g_hbMeter.woke || g_hbNotch.woke, hold1 = present && stage == 3;
        k1 = HbHideStep(&g_hbHide, hideOn, wake1, hold1, fade, dt);
        if (g_hbMeter.alpha > 0.004f) HbHideNote(0, hideOn, k1, &g_hbHide, g_hbMeter.woke, g_hbNotch.woke, 0, hold1, fade);
    }
    {   // v3.32 DHS332: the companion's small ring (Both with Playable Eileen): her game curse, the game's tiers, the 0.4 mark
        static const float kTh2[3] = { 0.25f, 0.50f, 0.70f };
        float v2 = 0.0f;
        int away2 = 0, stage2 = 0;
        const int present2 = second && HbCompanion(&v2, &away2);
        v2 = HbClamp01(v2);
        stage2 = HbCurseStage(v2, kTh2);
        {   // v3.35 CM335: where her curse is heading (the small ring)
            HbTgt tg;
            if (present2) {
                HbTargetRead(&tg, away2, v2, dt);
                HbNotchStep(&g_hbNotch2, tg.target, v2, tg.next, tg.haveNext, tg.alone, dt);
            } else {
                HbNotchStep(&g_hbNotch2, -1.0f, v2, -1.0f, 0, 0, dt);
            }
        }
        if (present2 != g_hbSrc2 && (present2 || g_hbSrc2 > 0)) {  // (no line while it was never shown)
            Log("hud bars: companion Eileen's small ring %s (%.2f, stage %s%s)", present2 ? "shown" : "gone", v2,
                kHbStageName[stage2], present2 && away2 ? ", saved value: she is not here" : "");
        }
        g_hbSrc2 = present2;
        if (present2 && stage2 != g_hbStageLog2) {
            if (g_hbStageLog2 >= 0)
                Log("hud bars: companion Eileen's curse stage %s -> %s (%.3f)", kHbStageName[g_hbStageLog2], kHbStageName[stage2], v2);
            g_hbStageLog2 = stage2;
        }
        HbMeterStep(&g_hbMeter2, v2, stage2, present2, dt);  // (not Both: it fades out over 0.4 s)
        {   // v3.32 MERGE: Playable Eileen holds her curse at 0 ([Ghost] LockCurse=1): grey, dim, LOCKED
            const int locked = HbRing2Locked(second, present2, g_hbPeLock, v2);
            if (present2 && locked != g_hbLockLog && (locked || g_hbLockLog >= 0))  // (no "unlocked" line at the first show)
                Log("hud bars: companion Eileen's small ring %s", locked ? "locked (Playable Eileen [Ghost] LockCurse=1 holds "
                    "her curse at 0): grey with LOCKED" : "unlocked: her curse as the game keeps it");
            lockWake = present2 && g_hbLockLog >= 0 && locked != g_hbLockLog;  // v3.38 HB338: the lock changing wakes it
            if (present2) {
                g_hbLockLog = locked;
                // the ring's first moments (still fading in): it takes its state at once; else the grey eases (0.3 s)
                g_hbLockK = g_hbMeter2.alpha < 0.05f ? (float)locked : HbLockStep(g_hbLockK, locked, dt);
            }
        }
        {   // v3.38 HB338: the small ring's own Hide Gauge timer (her events only), the gauge's fade shared
            const int wake2 = g_hbMeter2.woke || g_hbNotch2.woke || lockWake, hold2 = present2 && stage2 == 3;
            k2 = HbHideStep(&g_hbHide2, hideOn, wake2, hold2, fade, dt);
            if (g_hbMeter2.alpha > 0.004f) HbHideNote(1, hideOn, k2, &g_hbHide2, g_hbMeter2.woke, g_hbNotch2.woke, lockWake, hold2, fade);
        }
        if (g_hbMeter2.alpha > 0.004f && k2 > 0.002f) {
            // v3.38 HB338: rests at HB_REST2_A (0.70; was 0.55), away x HB_AWAY_A (0.7; was 0.5), every part x k2; her
            // notch / next mark / glyph at HbAlertQ (not dimmed by away, at least 0.85 of the ring's fade-in) x k2
            const float aw2 = HbMeterStrengthRest(&g_hbMeter2, HB_REST2_A) * HbLockAlpha(g_hbLockK);
            const float a2 = aw2 * (away2 ? HB_AWAY_A : 1.0f) * k2, R2 = HB_RING2_R * sy;
            float c2x;
            const float c2y = HbRing2At(peRing, &c2x);  // v3.41 CM341: canvas; beside PE's own ring while it draws the player's
            const float cx2 = 0.5f * w + (c2x - 256.0f) * sx, cy2 = c2y * sy;  // v3.41 CM341: was HB_RING2_CX / CY
            const float tagTop = (c2y + HB_RING2_R + HB_TAG_GAP) * sy;
            g_hbGreyK = g_hbLockK;  // v3.32 MERGE: HbArgb greys the ring and its EILEEN label
            HbRingDraw(&g_hbMeter2, cx2, cy2, R2, HB_RING_BW * HB_RING2_K * sy, HB_RING2_K, px, v2, kTh2, 0.40f, stage2, a2);
            HbNotchDraw(cx2, cy2, R2, HB_RING_BW * HB_RING2_K * sy, HB_RING2_K, px, &g_hbNotch2,
                        HbAlertQ(aw2, g_hbMeter2.alpha) * k2 * (1.0f - g_hbLockK));  // v3.35 CM335; v3.38 HB338
            HbAloneDraw(cx2, cy2, R2, HB_RING2_R, sy, px, &g_hbNotch2, aw2, g_hbMeter2.alpha, k2);  // v3.35 CM335; v3.38 HB338
            HbTinyText(cx2, tagTop, HB_TAG_H / 5.0f * sy, "EILEEN", HB_TAG_RGB, 0.9f * a2, px);
            g_hbGreyK = 0.0f;
            if (g_hbLockK > 0.004f)  // v3.32 MERGE: the small LOCKED tag (cold steel, not greyed further), under EILEEN
                HbTinyText(cx2, tagTop + (HB_TAG_H + HB_LOCK_GAP) * sy, HB_LOCK_H / 5.0f * sy, "LOCKED", HB_LOCK_RGB,
                           0.95f * g_hbLockK * HbMeterStrengthRest(&g_hbMeter2, HB_REST2_A) * (away2 ? HB_AWAY_A : 1.0f) * k2, px);
        }
    }
    if (!(g_hbMeter.alpha > 0.004f) || !(k1 > 0.002f)) return;  // v3.38 HB338: + Hide Gauge
    aw = HbMeterStrengthRest(&g_hbMeter, HB_REST2_A);  // v3.38 HB338: rest 0.70 as the small ring (the user); without away
    a = aw * (away ? HB_AWAY_A : 1.0f) * k1;  // v3.38 HB338: away 0.5 -> HB_AWAY_A, x Hide Gauge's k
    cx = 0.5f * w + (HB_RING_CX - 256.0f) * sx, cy = HB_RING_CY * sy;
    HbRingDraw(&g_hbMeter, cx, cy, HB_RING_R * sy, HB_RING_BW * sy, 1.0f, px, v, th, minor, stage, a);
    if (!src) {  // v3.35 CM335: her notch, next mark and left-alone glyph on the main ring (v3.38 HB338: alert strength)
        HbNotchDraw(cx, cy, HB_RING_R * sy, HB_RING_BW * sy, 1.0f, px, &g_hbNotch, HbAlertQ(aw, g_hbMeter.alpha) * k1);
        HbAloneDraw(cx, cy, HB_RING_R * sy, HB_RING_R, sy, px, &g_hbNotch, aw, g_hbMeter.alpha, k1);
    }
    // v3.38 HB338: every ring named: PLAYER under the player's own ring, EILEEN under the main ring when it shows companion
    // Eileen (Companion; Player / Both without Playable Eileen), in the small ring's EILEEN size, colour, strength, shadow
    HbTinyText(cx, HB_PLAYER_TOP * sy, HB_TAG_H / 5.0f * sy, src ? "PLAYER" : "EILEEN", HB_TAG_RGB, 0.9f * a, px);
    g_hbLabelA = HbLabelAlpha(&g_hbMeter) * (away ? 0.6f : 1.0f) * k1;  // v3.38 HB338: x k1
    if (g_hbLabelA > 0.02f)  // v3.35 CM335: the stage name in the stroke font (v3.38 HB338: under the ring's name)
        HbTinyText(cx, HbStageTop(1) * sy, HB_LABEL_H / 5.0f * sy, kHbStageTag[stage], kHbStageRgb[stage], g_hbLabelA, px);
}
// v3.35 CM335: the stage name is no longer the game's text (HbLabel, font 1 at canvas y 63, queued for the text flush
// 0x4EFC7B): HbCurse draws it in the stroke font with the ring.

// ---------------------------------------------------------------- the frame (items.inc SparkleFrame, game thread)
static int g_hbFaults;
static DWORD g_hbFaultCode;
static void *g_hbFaultAt;
static int HbFaultNote(EXCEPTION_POINTERS *e)
{
    g_hbFaultCode = e->ExceptionRecord->ExceptionCode;
    g_hbFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static void HbFrameC(void)
{
    static int s_logged;
    const int mode = (int)g_hb[HB_K_BARS], cm = (int)g_hb[HB_K_CURSE];
    const float dt = HbClock();
    void *dev = DEV;
    FxSaved sv;
    DWORD room;
    g_hbLabelA = 0.0f;
    if (!mode) g_hbEN = 0;  // off: forget every bar (none pops up from an old state later)
    if (!mode && g_hbRefRoom != 0xFFFFFFFFu) HbRefRoomEnd();  // v3.52 HB352: the visit's refusal line (bars off)
    if (!cm) memset(&g_hbMeter, 0, sizeof g_hbMeter), g_hbSrc = -1, g_hbStageLog = -1;
    if (!cm) memset(&g_hbMeter2, 0, sizeof g_hbMeter2), g_hbSrc2 = -1, g_hbStageLog2 = -1;  // v3.32 DHS332
    if (!cm) g_hbLockK = 0.0f, g_hbLockLog = -1;  // v3.32 MERGE
    if (!cm) memset(&g_hbNotch, 0, sizeof g_hbNotch), memset(&g_hbNotch2, 0, sizeof g_hbNotch2);  // v3.35 CM335
    if (!cm) memset(&g_hbHide, 0, sizeof g_hbHide), memset(&g_hbHide2, 0, sizeof g_hbHide2), g_hbGateShut = 0;  // v3.38 HB338
    if ((!mode && !cm) || !dev) return;
    if (cm && !*(volatile BYTE *)0x1079C30) g_hbGateShut = 1;  // v3.38 HB338: HudDisable ran (pause, event): Hide Gauge sleeps
    // the game shows its HUD now: gameplay, phase 0, the HUD gate open, not the frame the pause picture is taken from
    if (!FxGameplay() || *(volatile int *)0xFD5C20 != 0 || !*(volatile BYTE *)0x1079C30 || *(volatile int *)0x12E3E2C == 1)
        return;
    room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (room != g_hbRoom) {
        g_hbRoom = room;
        g_hbEN = 0;
    }
    if (mode && room != g_hbRefRoom) {  // v3.52 HB352: a new room visit for the refusal count and the boss notes
        HbRefRoomEnd();
        g_hbRefRoom = room;
    }
    if (!FxOnBackBuffer(dev) || !FxSave(dev, &sv, 0)) return;
    g_hbNow += dt > HB_DT_MAX ? HB_DT_MAX : dt;
    g_hbVn = 0;
    __try {
        if (mode) HbEnemies(mode, &sv.vp, dt);  // (sv.vp: the viewport in force at the HUD call, as Item Shine's)
        if (cm) HbCurse(cm, dt);
        if (g_hbVn) {
            FxBase(dev);
            HbFlush(dev);
        }
    } __finally {
        g_hbVn = 0;
        FxRestore(dev, &sv);
    }
    if (!s_logged++) Log("hud bars: first frame drawn (viewport %lu,%lu %lux%lu, %d being(s) tracked)", sv.vp.x, sv.vp.y, sv.vp.w, sv.vp.h, g_hbEN);
}
static void HbFrame(void)
{
    if (g_hbFaults > 3) return;
    __try {
        HbFrameC();
    } __except (HbFaultNote(GetExceptionInformation())) {
        g_hbVn = 0;
        g_hbEN = 0;
        g_hbBossN = 0;  // v3.52 HB352
        if (++g_hbFaults == 1) Log("hud bars: the draw faulted (code %08lX at %p); it stops after 3 more", g_hbFaultCode, g_hbFaultAt);
        if (g_hbFaults > 3) Log("hud bars: off for this session (faults)");
    }
}
