// v3.53 OL1: native test of src\ol_words.h (test\build_ol_words_test.cmd), the Options layout's words (round R1).
// 1. Every text the build shows from the header keeps the plan's limits (staging\options_layout\PLAN.md section 3):
//    labels 18, values 13, descriptions one line of 52, titles 28, an added sentence 52 (a format's %d / %g counted as
//    4 characters); no control character, no leading / trailing / double blank (the game's byte strings show them).
// 2. Every screen word that differs from its ini word (PLAN section 7) round-trips: OlShow(ini) is the screen word,
//    OlIni(screen) the same ini word; inside a map no two screen or ini words alike; a word outside a map shows as
//    written. The maps' ini words are the code's own ini words (the sim headers' tables where the code keeps them there,
//    else a copy of the .inc table, named).
// 3. The composed lines the code builds (a line + " Enter: ...", + " F7 in play: ...", the Renderer's backend, the
//    Regeneration delay) are listed with their worst case.
// Usage: out\ol_words_test.exe
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ol_words.h"
#include "../src/fadein_sim.h"   // kFiModeName (the [Gameplay] FadeIn words)
#include "../src/rando_sim.h"    // kRzR302Name (the [Randomizer] Room302 words)  // v3.56 RI356: checked equal to the row's values below
#include "../src/font_sim.h"     // kFnRowStem (the [Textures] MenuFont words)

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int FmtLen(const char *s)  // as tools\gen_ol_words.py fmt_len: %d / %g at 4 characters, %% one
{
    int n = 0;
    for (; *s; ++s) {
        if (s[0] == '%' && (s[1] == 'd' || s[1] == 'g')) { n += 4; ++s; continue; }
        if (s[0] == '%' && s[1] == '%') { n += 1; ++s; continue; }
        ++n;
    }
    return n;
}
static int Max(char kind) { return kind == 'L' ? 18 : kind == 'V' ? 13 : kind == 'T' ? 28 : 52; }

typedef struct { char kind; const char *s, *name; } Word;
#define OL_W(k, s, n) { k, s, n },
static const Word kWords[] = { OL_WORDS_LIST(OL_W) };
#undef OL_W

static void TestLimits(void)
{
    int i, perKind[256] = { 0 };
    const int n = (int)(sizeof kWords / sizeof kWords[0]);
    for (i = 0; i < n; ++i) {
        const Word *w = &kWords[i];
        const int len = FmtLen(w->s), sentence = w->kind == 'S';
        const char *p;
        ++perKind[(unsigned char)w->kind];
        CHECK(strchr("LVDTS", w->kind) != NULL, "%s: kind %c", w->name, w->kind);
        CHECK(len <= Max(w->kind), "%s (%c): %d > %d: \"%s\"", w->name, w->kind, len, Max(w->kind), w->s);
        if (!strcmp(w->name, "OLW_RESERVED")) continue;
        CHECK(w->s[0] != 0, "%s is empty", w->name);
        for (p = w->s; *p; ++p) CHECK((unsigned char)*p >= 0x20 && (unsigned char)*p < 0x7F, "%s: byte %02X", w->name, (unsigned char)*p);
        CHECK(sentence ? w->s[0] == ' ' && w->s[1] != ' ' : w->s[0] != ' ', "%s: leading blank", w->name);
        CHECK(w->s[strlen(w->s) - 1] != ' ', "%s: trailing blank", w->name);
        CHECK(strstr(w->s + (sentence ? 1 : 0), "  ") == NULL, "%s: two blanks", w->name);
        if (w->kind == 'D' || w->kind == 'S') CHECK(w->s[strlen(w->s) - 1] == '.', "%s: a description ends with a full stop", w->name);
        CHECK(strstr(w->s, "Option ") == NULL || !strcmp(w->name, "OLW_RESERVED"), "%s names \"Option N\" (PLAN section 3)", w->name);
        CHECK(strstr(w->s, "Next door") == NULL, "%s: \"Next door.\" is \"Next room.\" now", w->name);
    }
    printf("limits: %d texts (labels %d, values %d, descriptions %d, titles %d, sentences %d)\n", n, perKind['L'], perKind['V'],
           perKind['D'], perKind['T'], perKind['S']);
}

// the code's ini words for each map: the sim headers' tables, else a copy of the .inc's (named; keep them equal)
typedef struct { int map; const char *const *ini; int n; const char *where; } MapIni;
static const char *const kGpEcho[] = { "Off", "Shadow Copy", "Shadow Cast" };                     // gameplay.inc kGpName[GP_ECHO]
static const char *const kGpFollow[] = { "Off", "Monsters", "Victims", "Monsters + Victims" };    // gameplay.inc kGpName[GP_FOLLOW]
static const char *const kHbBars[] = { "Off", "Locked Target", "All Near" };                     // hudbars.inc kHbName[HB_K_BARS]
static const char *const kSubEdge[] = { "Shadow", "None", "Outline", "Outline+Shadow" };          // sh4windowmod.c kSubName[SUB_EDGE]
static const char *const kFxFilter[] = { "None", "CRT", "Vivid", "Sepia", "Old Film", "Night Vision", "Night Vision Mono",
                                         "Thermal", "Spirit Vision", "Vignette" };               // sh4windowmod.c kFxName[FX_FILTER]
static const char *const kInvOnOff[] = { "Off", "On" };                                          // inventory.inc kInvName
static const char *const kZeroOne[] = { "0", "1" };                                              // [ExtraMenu] *, [Camera] Enable, cfg bytes
static const char *const kPcSteal[] = { "Off", "5", "10" };                                      // playercheats.inc kPcIni[PC_LS]
static const char *const kHeadMotion[] = { "Off", "Slight", "On" };                              // the game's Head Motion words
static const char *const kFpMove[] = { "Rotate", "Sideways" }, *const kFpCam[] = { "Normal", "Reverse" };
static const char *const kPcQc[] = { "Off", "1.5x", "2x", "3x" };                                 // playercheats.inc kPcIni[PC_QC]  // v3.57 OLP1
static const char *const kFxNoise[] = { "Low", "Normal", "High", "Very High" };                  // sh4windowmod.c kFxName[FX_NOISE]  // v3.58 MN358
static const MapIni kMapIni[] = {
    { OLM_POWERS_ECHO_DOUBLES, kGpEcho, 3, "gameplay.inc kGpName[GP_ECHO]" },
    { OLM_ENEMIES_DOOR_CHASE, kGpFollow, 4, "gameplay.inc kGpName[GP_FOLLOW]" },
    { OLM_INTERFACE_ENEMY_HEALTH, kHbBars, 3, "hudbars.inc kHbName" },
    { OLM_SUBTITLES_EDGE, kSubEdge, 4, "sh4windowmod.c kSubName[SUB_EDGE]" },
    { OLM_EFFECTS_FILTER, kFxFilter, 10, "sh4windowmod.c kFxName[FX_FILTER]" },
    { OLM_ENEMIES_REVEAL_METHOD, kFiModeName, FI_MODE_N, "fadein_sim.h kFiModeName" },  // v3.58 MN358: Reveal Method (was Fade In)
    { OLM_GRAIN_STRENGTH, kFxNoise, 4, "sh4windowmod.c kFxName[FX_NOISE]" },  // v3.58 MN358: Ultra on screen, Very High in the ini
    // v3.56 RI356: Room 302 Items shows its ini words (Off / Shuffle / Random): no map any more
    { OLM_INTERFACE_MENU_FONT, kFnRowStem, FN_ROW_N, "font_sim.h kFnRowStem" },
    { OLM_ITEMS_TOOLBELT, kInvOnOff, 2, "inventory.inc kInvName" },
    { OLM_COMBAT_LIFE_STEAL, kPcSteal, 3, "playercheats.inc kPcIni" },
    { OLM_PAUSE_STORAGE_CHEST, kZeroOne, 2, "[ExtraMenu] Chest" }, { OLM_PAUSE_SAVE_GAME, kZeroOne, 2, "[ExtraMenu] Save" },
    { OLM_PAUSE_LOAD_GAME, kZeroOne, 2, "[ExtraMenu] Load" }, { OLM_PAUSE_QUICK_SAVE, kZeroOne, 2, "[ExtraMenu] QuickSave" },
    { OLM_PAUSE_QUICK_LOAD, kZeroOne, 2, "[ExtraMenu] QuickLoad" }, { OLM_PAUSE_EILEEN, kZeroOne, 2, "[ExtraMenu] Eileen" },  // v3.56 MN356
    { OLM_CAMERA_CAMERA, kZeroOne, 2, "[Camera] Enable" }, { OLM_CONTROLS_DEFAULT_SPEED, kZeroOne, 2, "cfg+0x18" },
    { OLM_SUBTITLES_SUBTITLES, kZeroOne, 2, "cfg+0x12" }, { OLM_GRAIN_FILM_GRAIN, kZeroOne, 2, "cfg+0x14" },
    { OLM_SHADOWS_SHADOWS, kZeroOne, 2, "cfg+0x19" },
    { OLM_INTERFACE_HEALTH_GAUGE, kInvOnOff, 2, "cfg+0x09 (the game's Off / On)" },
    { OLM_INTERFACE_ITEM_ICONS, kInvOnOff, 2, "cfg+0x0A (the game's Off / On)" },
    { OLM_ROOM302_HEAD_BOB, kHeadMotion, 3, "cfg+0x08 (the game's Off / Slight / On)" },
    { OLM_ROOM302_TURN_OR_STRAFE, kFpMove, 2, "cfg+0x17 (the game's Rotate / Sideways)" },  // v3.57 OLP1: Turn or Strafe (was Left / Right)
    { OLM_COMBAT_QUICK_CHARGE, kPcQc, 4, "playercheats.inc kPcIni[PC_QC]" },  // v3.57 OLP1: x1.5 / x2 / x3 on screen, 1.5x / 2x / 3x in the ini
    { OLM_ROOM302_INVERT_LOOK, kFpCam, 2, "cfg+0x16 (the game's Normal / Reverse)" },
};

static void TestMaps(void)
{
    int i, j, m, covered[OLM_N] = { 0 };
    for (i = 0; i < OL_MAP_N; ++i) {
        const OlMapEntry *e = &kOlMap[i];
        CHECK(e->map >= 0 && e->map < OLM_N, "entry %d: map %d", i, e->map);
        CHECK(!strcmp(OlShow(e->map, e->ini), e->show), "map %d: %s shows %s, not %s", e->map, e->ini, OlShow(e->map, e->ini), e->show);
        CHECK(!strcmp(OlIni(e->map, e->show), e->ini), "map %d: %s stands for %s, not %s", e->map, e->show, OlIni(e->map, e->show), e->ini);
        CHECK(!strcmp(OlIni(e->map, OlShow(e->map, e->ini)), e->ini), "map %d: %s does not round-trip", e->map, e->ini);
        CHECK((int)strlen(e->show) <= 13, "map %d: the screen word %s is over 13", e->map, e->show);
        CHECK(strcmp(e->show, e->ini) != 0, "map %d: %s maps to itself", e->map, e->ini);
        for (j = 0; j < OL_MAP_N; ++j) {
            if (j == i || kOlMap[j].map != e->map) continue;
            CHECK(!OlSame(kOlMap[j].show, e->show), "map %d: two values show %s", e->map, e->show);
            CHECK(!OlSame(kOlMap[j].ini, e->ini), "map %d: %s twice", e->map, e->ini);
        }
    }
    for (m = 0; m < (int)(sizeof kMapIni / sizeof kMapIni[0]); ++m) {
        const MapIni *mi = &kMapIni[m];
        covered[mi->map] = 1;
        for (i = 0; i < OL_MAP_N; ++i) {  // every mapped ini word is one the code writes
            int found = 0;
            if (kOlMap[i].map != mi->map) continue;
            for (j = 0; j < mi->n; ++j) found |= !strcmp(mi->ini[j], kOlMap[i].ini);
            CHECK(found, "map %d: %s is not one of %s's words", mi->map, kOlMap[i].ini, mi->where);
        }
        for (j = 0; j < mi->n; ++j) {  // every word the code writes shows as one word, and its shown word leads back to it
            const char *show = OlShow(mi->map, mi->ini[j]);
            int k;
            CHECK(!strcmp(OlIni(mi->map, show), mi->ini[j]), "%s: %s -> %s -> %s", mi->where, mi->ini[j], show, OlIni(mi->map, show));
            CHECK((int)strlen(show) <= 13, "%s: %s shows %s (over 13)", mi->where, mi->ini[j], show);
            for (k = 0; k < mi->n; ++k)
                if (k != j) CHECK(strcmp(show, OlShow(mi->map, mi->ini[k])) != 0, "%s: %s and %s show alike", mi->where, mi->ini[j], mi->ini[k]);
        }
    }
    for (m = 0; m < OLM_N; ++m) CHECK(covered[m], "map %d has no ini table in this test", m);
    CHECK(!strcmp(OlShow(OLM_POWERS_ECHO_DOUBLES, "Off"), "Off"), "a word outside the map shows as written");
    CHECK(!strcmp(OlShow(OLM_EFFECTS_FILTER, "night vision mono"), "Night Mono"), "the ini reads case-blind");
    CHECK(!strcmp(OlShow(-1, "Monsters + Victims"), "Monsters + Victims"), "map -1: no map");
    CHECK(OlShow(OLM_ENEMIES_DOOR_CHASE, NULL) == NULL, "NULL stays NULL");
    printf("maps: %d maps, %d entries\n", OLM_N, OL_MAP_N);
}

// v3.57 OLP1: the renamed rows keep their ini words. Quick Charge shows x1.5 / x2 / x3 and the ini keeps 1.5x / 2x / 3x
// (playercheats.inc writes kPcIni, read here from the source itself); Turn or Strafe keeps the game's Rotate / Sideways;
// Reroll's values (By Room / By World) are its ini words (rando_sim.h kRzRerollName)
static void TestIniWords(void)
{
    static const char *const kShow[4] = { OLW_COMBAT_QUICK_CHARGE_V_OFF, OLW_COMBAT_QUICK_CHARGE_V_X1_5, OLW_COMBAT_QUICK_CHARGE_V_X2,
                                          OLW_COMBAT_QUICK_CHARGE_V_X3 };
    static const char *const kWant[4] = { "Off", "x1.5", "x2", "x3" };
    char line[4096];
    int i, found = 0;
    FILE *f = fopen("../src/playercheats.inc", "r");
    for (i = 0; i < 4; ++i) {
        CHECK(!strcmp(kShow[i], kWant[i]), "Quick Charge value %d shows %s, not %s", i, kShow[i], kWant[i]);
        CHECK(!strcmp(OlShow(OLM_COMBAT_QUICK_CHARGE, kPcQc[i]), kShow[i]), "Quick Charge: ini %s shows %s", kPcQc[i],
              OlShow(OLM_COMBAT_QUICK_CHARGE, kPcQc[i]));
        CHECK(!strcmp(OlIni(OLM_COMBAT_QUICK_CHARGE, kShow[i]), kPcQc[i]), "Quick Charge: %s writes %s", kShow[i],
              OlIni(OLM_COMBAT_QUICK_CHARGE, kShow[i]));
    }
    CHECK(!strcmp(OlIni(OLM_COMBAT_QUICK_CHARGE, "x1.5"), "1.5x") && !strcmp(OlIni(OLM_COMBAT_QUICK_CHARGE, "x2"), "2x") &&
          !strcmp(OlIni(OLM_COMBAT_QUICK_CHARGE, "x3"), "3x"), "Quick Charge's ini words stay 1.5x / 2x / 3x");
    CHECK(!strcmp(OlShow(OLM_COMBAT_QUICK_CHARGE, "1.5X"), "x1.5"), "Quick Charge: the ini reads case-blind");
    CHECK(f != NULL, "../src/playercheats.inc not found (run from test\\)");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (strstr(line, "{ \"Off\", \"1.5x\", \"2x\", \"3x\" },")) ++found;
        fclose(f);
    }
    CHECK(found == 1, "playercheats.inc kPcIni[PC_QC] is { \"Off\", \"1.5x\", \"2x\", \"3x\" } (%d line(s))", found);
    CHECK(!strcmp(OlShow(OLM_ROOM302_TURN_OR_STRAFE, "Rotate"), OLW_ROOM302_TURN_OR_STRAFE_V_TURN) &&
          !strcmp(OlShow(OLM_ROOM302_TURN_OR_STRAFE, "Sideways"), OLW_ROOM302_TURN_OR_STRAFE_V_STRAFE) &&
          !strcmp(OlIni(OLM_ROOM302_TURN_OR_STRAFE, "Turn"), "Rotate") && !strcmp(OlIni(OLM_ROOM302_TURN_OR_STRAFE, "Strafe"), "Sideways"),
          "Turn or Strafe: Turn / Strafe stand for the game's Rotate / Sideways");
    CHECK(!strcmp(kRzRerollName[RZR_ROOM], OLW_RANDO_REROLL_V_BY_ROOM) && !strcmp(kRzRerollName[RZR_WORLD], OLW_RANDO_REROLL_V_BY_WORLD),
          "Reroll: the screen words are the ini words By Room / By World");
    CHECK(!strcmp(OLW_ROOM302_TURN_OR_STRAFE_L, "Turn or Strafe") && !strcmp(OLW_RANDO_REROLL_L, "Reroll"), "the two renamed labels");
    {   // v3.58 MN358: Reveal Method (was Fade In): Fade In on screen = Fade in the ini, Portal = Fade + Portal; Film Grain >
        // Strength: Ultra on screen = Very High in the ini (the code writes kFiModeName / kFxName; both words read)
        CHECK(!strcmp(OLW_ENEMIES_REVEAL_METHOD_L, "Reveal Method") && !strcmp(OLW_ENEMIES_REVEAL_METHOD_V_FADE_IN, "Fade In") &&
              !strcmp(OLW_ENEMIES_REVEAL_METHOD_V_PORTAL, "Portal") && !strcmp(OLW_GRAIN_STRENGTH_V_ULTRA, "Ultra"),
              "v3.58: Reveal Method / Fade In / Portal / Ultra on screen");
        CHECK(!strcmp(OlShow(OLM_ENEMIES_REVEAL_METHOD, "Fade"), "Fade In") && !strcmp(OlIni(OLM_ENEMIES_REVEAL_METHOD, "Fade In"), "Fade") &&
              !strcmp(OlShow(OLM_ENEMIES_REVEAL_METHOD, "Fade + Portal"), "Portal") &&
              !strcmp(OlIni(OLM_ENEMIES_REVEAL_METHOD, "Portal"), "Fade + Portal") &&
              !strcmp(OlShow(OLM_ENEMIES_REVEAL_METHOD, "Off"), "Off"), "Reveal Method: Fade / Fade + Portal stay the ini words");
        CHECK(!strcmp(kFiModeName[FI_MODE_FADE], "Fade") && !strcmp(kFiModeName[FI_MODE_PORTAL], "Fade + Portal"),
              "fadein_sim.h writes Fade / Fade + Portal");
        CHECK(FiModeParse("Fade", NULL) == FI_MODE_FADE && FiModeParse("Fade In", NULL) == FI_MODE_FADE &&
              FiModeParse("fadein", NULL) == FI_MODE_FADE && FiModeParse("Fade + Portal", NULL) == FI_MODE_PORTAL &&
              FiModeParse("Portal", NULL) == FI_MODE_PORTAL && FiModeParse("Off", NULL) == FI_MODE_OFF,
              "[Gameplay] FadeIn reads Fade, Fade In, Fade + Portal, Portal, Off");
        CHECK(!strcmp(OlShow(OLM_GRAIN_STRENGTH, "Very High"), "Ultra") && !strcmp(OlIni(OLM_GRAIN_STRENGTH, "Ultra"), "Very High") &&
              !strcmp(OlIni(OLM_GRAIN_STRENGTH, "ultra"), "Very High") && !strcmp(OlShow(OLM_GRAIN_STRENGTH, "High"), "High"),
              "Film Grain > Strength: Very High stays the ini word (Ultra reads as it)");
    }
    printf("ini words: Quick Charge, Turn or Strafe, Reroll, Reveal Method, Strength checked\n");
}

static void TestComposed(void)
{
    // the combo rows: the longest value line + the Enter sentence (two lines at most: descfit wraps 416 px a line)
    const char *const kSnd[] = { OLW_SOUND_SOUND_MODE_VD_MONO, OLW_SOUND_SOUND_MODE_VD_STEREO, OLW_SOUND_SOUND_MODE_VD_HEADPHONES,
                                 OLW_SOUND_SOUND_MODE_VD_SURROUND, OLW_SND_PAN_STOCK, OLW_SND_PAN_NA };
    const char *const kFilter[] = { OLW_EFFECTS_FILTER_D, OLW_EFFECTS_FILTER_VD_NONE, OLW_EFFECTS_FILTER_VD_CRT,
                                    OLW_EFFECTS_FILTER_VD_VIVID, OLW_EFFECTS_FILTER_VD_SEPIA, OLW_EFFECTS_FILTER_VD_OLD_FILM,
                                    OLW_EFFECTS_FILTER_VD_NIGHT_VISION, OLW_EFFECTS_FILTER_VD_NIGHT_MONO, OLW_EFFECTS_FILTER_VD_THERMAL,
                                    OLW_EFFECTS_FILTER_VD_SPIRIT_VISION, OLW_EFFECTS_FILTER_VD_VIGNETTE };
    const char *const kRend[] = { OLW_DISPLAY_RENDERER_VD_DIRECT3D_11, OLW_REND_16BIT, OLW_DISPLAY_RENDERER_VD_DIRECT3D_12,
                                  OLW_DISPLAY_RENDERER_VD_STOCK, OLW_REND_OTHER, OLW_REND_TEST, OLW_DISPLAY_RENDERER_VD_CUSTOM,
                                  OLW_DISPLAY_RENDERER_VD_UNAVAILABLE };
    const char *const kRegen[] = { OLW_PLAYER_REGENERATION_VD_X1, OLW_PLAYER_REGENERATION_VD_X1_25, OLW_PLAYER_REGENERATION_VD_X2 };
    char t[256];
    int i;
    for (i = 0; i < (int)(sizeof kSnd / sizeof kSnd[0]); ++i) {
        sprintf(t, "%s%s", kSnd[i], OLW_SND_ENTER);
        CHECK((int)strlen(t) <= 2 * 52 && FmtLen(kSnd[i]) <= 52, "Sound Mode + Enter: \"%s\"", t);
    }
    for (i = 0; i < (int)(sizeof kFilter / sizeof kFilter[0]); ++i) {
        sprintf(t, "%s", kFilter[i]);
        sprintf(t + strlen(t), OLW_FILTER_F7, 7);  // the default PreviewKey F7  // v3.57 OLP1: two lines at most (was: the row line one)
        CHECK(strlen(t) <= 2 * 52, "Filter + F7: \"%s\" (%u; two lines at most)", t, (unsigned)strlen(t));
        sprintf(t, "%s", kFilter[i]);
        sprintf(t + strlen(t), OLW_FILTER_F7, 12);  // F10 - F12: one character more
        CHECK(strlen(t) <= 2 * 52 && strlen(t) + 4 <= 128 && (i || strlen(t) + 4 <= 80), "Filter + F12: \"%s\" (%u; g_txtFxD 80 / g_txtOlFilter 128)",
              t, (unsigned)strlen(t));  // v3.57 OLP1: g_txtFxD 64 -> 80
    }
    for (i = 0; i < (int)(sizeof kRend / sizeof kRend[0]); ++i) {  // + the longest backend text (perfhud_sim.h PerfBackendName)
        sprintf(t, "%s %s.", kRend[i], "Direct3D: GOG -> dgVoodoo2, a_manifest_variant_named_long");
        CHECK(strlen(t) < 160 && strlen(t) <= 3 * 52, "Renderer + backend: \"%s\" (%u)", t, (unsigned)strlen(t));
    }
    // v3.57 OLP1: only x1's line names the delay ("after 30 seconds"; x1.25 / x2 say "faster than x1"); with RegenDelay 120 it
    // still fits one line
    CHECK(strstr(kRegen[0], "after 30 seconds") != NULL, "%s: regen.inc replaces \"after 30 seconds\" by the delay", kRegen[0]);
    for (i = 1; i < 3; ++i) CHECK(strstr(kRegen[i], "after 30") == NULL, "%s: no delay in this line (regen.inc leaves it)", kRegen[i]);
    CHECK(strlen(kRegen[0]) + 1 <= 52, "x1's line with a 3-digit RegenDelay: %u", (unsigned)strlen(kRegen[0]) + 1);
    CHECK(strlen(OLW_CAMERA_CAMERA_VD_SHOULDER OLW_CAM_SETTINGS) < 96 && strlen(OLW_CAMERA_CAMERA_VD_GAME OLW_CAM_SETTINGS) < 96,
          "the Camera row's lines fit g_txtOl's 96 bytes");
    CHECK(strlen(OLW_DISPLAY_DISPLAY_MODE_D) + 4 <= 64, "Display Mode's line fits g_txtDispDesc[64]");
    CHECK(strlen(OLW_EFFECTS_FILTER_D) + strlen(OLW_FILTER_F7) + 1 + 4 <= 80, "Filter's row line + the F-key sentence fit g_txtFxD's 80 bytes");  // v3.57 OLP1: was 26 / 64
    CHECK(strlen(OLW_REGEN_PE_LEGACY) - 2 + 3 <= 52 && strlen(OLW_REGEN_PE_LEGACY) - 2 + 3 + 4 <= 56, "Playable Eileen's Regeneration line with "
          "RegenDelay 120 fits one line and gameplay.inc's d[56]");  // v3.57 OLP1
    CHECK(strlen(OLW_TELEPORT_ON_CD) + 8 < 64, "Ghost Teleport's On line with a cooldown fits teleport.inc's d[64]");  // v3.57 OLP1
    printf("composed lines checked\n");
}

int main(void)
{
    TestLimits();
    TestMaps();
    TestIniWords();  // v3.57 OLP1
    TestComposed();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "OK", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
