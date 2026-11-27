// v3.54 OL34: the Options layout of the plan's rounds R3 + R4 as data (staging/options_layout/PLAN.md sections 2, 4, 5,
// 6 and 9; its tools/layout_data.py). The page ids and the page tree, and every page's rows in order, each with the
// source it is built from and the plan's key for it (page id, row name). layout.inc builds the Options tables from this;
// test\layout_sim_test.c checks it against the plan's own pages and rows (OL_PLAN_* in ol_words.h, generated from
// layout_data.py): the tree, the slots, Back / Exit, every plan row placed once, and the interim places (R5's stock rows
// and PLAN section 4's fallbacks) named one by one.
// v3.55 OL56 (rounds R5 + R6; staging/v355_layout/BRIEF.md): the stock moves. Every row has a mode: both, StockMoves=1 only
// (the plan's own place of a stock row, R5) or StockMoves=0 only (v3.54's interim place). With [Options] StockMoves=1
// (the default) every page is the plan's section 5 page, row for row; with 0 it is v3.54's interim layout, row for row.
// The test checks both.
#ifndef LAYOUT_SIM_H
#define LAYOUT_SIM_H
#include "ol_words.h"

// ---------------------------------------------------------------- the page ids (PLAN section 6)
// Logical page L lives in physical slot L mod 3. Pages 0-2 are the game's stock pages (its special cases read these ids);
// 3-5 are Refined's top pages; the cycle is 0 .. LP_N - 1 (6: a cycle of 4 or 7 would put the wrap pair in one slot). A
// hidden page's slot differs from its parent's. Ids 14, 17 and 20 stay free.
enum {
    LP_DISPLAY, LP_CONTROLS, LP_INTERFACE, LP_SOUND, LP_EFFECTS, LP_GAMEPLAY,
    LP_SUBT,     // Interface > Subtitles (was LP_SUB 6)
    LP_MENUS,    // Interface > Menus (the plan's Pause Menu; PLAN section 4's Interface fallback renames it)
    LP_GRAIN,    // Effects > Film Grain
    LP_SHADOWS,  // Effects > Shadows
    LP_PLAYER,   // Gameplay > Player
    LP_CAMERA,   // Controls > Camera
    LP_R302,     // Controls > Room 302
    LP_COMBAT,   // Gameplay > Combat
    LP_FREE14,
    LP_ENEMIES,  // Gameplay > Enemies
    LP_ITEMS,    // Gameplay > Items
    LP_FREE17,
    LP_POWERS,   // Gameplay > Powers
    LP_RANDO,    // Gameplay > Randomizer
    LP_FREE20,
    LP_STEER,    // Camera > Steering (the third level)
    LP_MAX };
#define LP_N 6
typedef char OlCycleOk[LP_N % 3 != 1 && LP_MAX == 22 ? 1 : -1];
#define OL_ROWS 12      // the most rows a page's table holds (the end mark after it)
#define OL_MAX_ROWS 11  // PLAN section 4: 11 rows at most (pitch 24: the last row ends at y 360, where the description starts)

// ---------------------------------------------------------------- the row kinds
enum {
    OLK_END,     // the end of a page's list
    OLK_VALUE,   // a copy of an existing record (a setting, a volume bar, the Seed, New Seed)
    OLK_SCREEN,  // a copy of a value-less stock record that opens the game's own screen (Brightness, Gamma, Buttons): drawn as a folder
    OLK_COMBO,   // a copy of a record with a value whose Enter opens page arg (Subtitles until R5)
    OLK_FOLDER,  // a new value-less row that opens page arg
    OLK_CAMSW,   // the camera switch: Option 6's Camera record, its values in the plan's order (Game first)
    OLK_CAM,     // a new camera setting row: arg = the camera's row (OL_CAM_*), the same settings as the pause Extra Menu > Camera
    OLK_BACK,    // the last row of a hidden page
    OLK_EXIT     // the last row of a top page (the game's Exit)
};
// the camera's rows (camera.inc CAMR_*, checked equal in layout.inc)
#define OL_CAM_ENABLE   0
#define OL_CAM_DIST     1
#define OL_CAM_SIDE     2
#define OL_CAM_HEIGHT   3
#define OL_CAM_TILT     4
#define OL_CAM_TURN     5
#define OL_CAM_TILTSPD  6
#define OL_CAM_INVX     7
#define OL_CAM_INVY     8
#define OL_CAM_WALL     9
#define OL_CAM_MARGIN   10
#define OL_CAM_FIXED    11
#define OL_CAM_N        12

// ---------------------------------------------------------------- the sources (layout.inc finds each record by its input)
enum {
    OLS_NONE,
    // Display (stock page 0 and Option 4)
    OLS_RES, OLS_ASPECT, OLS_DISPMODE, OLS_MONITOR, OLS_FPS, OLS_BRIGHT, OLS_GAMMA, OLS_SNDMODE, OLS_BGM, OLS_SE, OLS_RENDERER,
    // Controls (stock page 1, Option 4's FOVs, Room 302)
    OLS_BUTTONS, OLS_DEFSPEED, OLS_VIB, OLS_OMNI, OLS_HEADBOB, OLS_LEFTRIGHT, OLS_INVLOOK, OLS_FOV, OLS_FOVFP, OLS_CAMSW,
    OLS_R3_DOWN, OLS_R3_UP, OLS_R3_FEEL, OLS_R3_WALK,
    // Interface (stock page 2, Option 5's Curse Meter / Enemy Health, Option 6, Menu Font, Start-Up, Subtitle Style)
    OLS_LANG, OLS_SUBTITLES, OLS_HGAUGE, OLS_ICONS, OLS_GRAIN_SW, OLS_SHADOW_SW, OLS_BLOOD, OLS_ENEMYHP, OLS_CURSE,
    OLS_EM0, OLS_EM1, OLS_EM2, OLS_EM3, OLS_EM4, OLS_EM5, OLS_SAVEBANKS, OLS_MENUFONT, OLS_STARTUP,
    OLS_SUB0, OLS_SUB1, OLS_SUB2, OLS_SUB3, OLS_SUB4, OLS_SUB5,
    // Sound (v3.52's Sound page)
    OLS_WIDTH, OLS_REVERB, OLS_NIGHT,
    OLS_SNDBG,  // v3.56 SD356: Sound > Background
    // Effects (the Screen Effects page, the Shadows page)
    OLS_FILTER, OLS_CUT, OLS_BLOOM, OLS_GRADE, OLS_LENS, OLS_DOF, OLS_BLUR, OLS_NSTR, OLS_NTYPE, OLS_NCOL, OLS_SHEDGE, OLS_SHDARK,
    // Gameplay (Option 5 and its folders)
    OLS_MOVE, OLS_REGEN, OLS_MED, OLS_GRAB, OLS_NOCLIP, OLS_OHK, OLS_PACT, OLS_QC, OLS_STEAL, OLS_AMMO,
    OLS_SHINE, OLS_TOOLBELT, OLS_STACKS, OLS_DROPS, OLS_COPIES, OLS_FOLLOW, OLS_PURSUIT, OLS_REVIVE, OLS_FADEIN,
    OLS_DP, OLS_AI, OLS_ECHO, OLS_ECHON, OLS_GT,
    OLS_RZ_ITEMS, OLS_RZ_KEEP, OLS_RZ_R302, OLS_RZ_MON, OLS_RZ_GHOSTS, OLS_RZ_OUTFIT, OLS_RZ_PERVISIT,  // v3.56 RI356: KEEP = Specials, PERVISIT = Reroll Visits; - SEED, NEW (the rows are gone)
    OLS_N };

// one row: its kind, source (OLS_*), argument (a folder's / combo's page; a camera row), a label / line of its own (NULL =
// the record's), and the plan's key: page id and row name in layout_data.py (pp2 / pn2: a second plan row it also stands
// for, the combo row until R5). A key on another page than the row's own is an interim place (kOlInterim).
// v3.55 OL56: + the row's mode (OLM_*): in both layouts, only with StockMoves=1 (R5's places), only with StockMoves=0
// (v3.54's interim places)
enum { OLM_BOTH, OLM_MOVES, OLM_STOCK };
typedef struct {
    unsigned char kind, src, arg;
    const char *label, *desc;
    const char *pp, *pn, *pp2, *pn2;
    unsigned char mode;  // v3.55 OL56
} OlRowDef;
#define OLX(mode, kind, src, arg, label, desc, pp, pn) { kind, src, arg, label, desc, pp, pn, 0, 0, mode }  // v3.55 OL56
#define OLR(kind, src, arg, label, desc, pp, pn) OLX(OLM_BOTH, kind, src, arg, label, desc, pp, pn)
#define OLV(src, pp, pn) OLR(OLK_VALUE, src, 0, 0, 0, pp, pn)
#define OLF(page, label, desc, pp, pn) OLR(OLK_FOLDER, OLS_NONE, page, label, desc, pp, pn)
#define OLC(cam, label, desc, pp, pn) OLR(OLK_CAM, OLS_NONE, cam, label, desc, pp, pn)
// v3.55 OL56: a row of one layout only (_M: StockMoves=1, R5's place; _S: StockMoves=0, v3.54's interim place)
#define OLR_M(kind, src, arg, label, desc, pp, pn) OLX(OLM_MOVES, kind, src, arg, label, desc, pp, pn)
#define OLR_S(kind, src, arg, label, desc, pp, pn) OLX(OLM_STOCK, kind, src, arg, label, desc, pp, pn)
#define OLV_M(src, pp, pn) OLR_M(OLK_VALUE, src, 0, 0, 0, pp, pn)
#define OLV_S(src, pp, pn) OLR_S(OLK_VALUE, src, 0, 0, 0, pp, pn)
#define OLF_M(page, label, desc, pp, pn) OLR_M(OLK_FOLDER, OLS_NONE, page, label, desc, pp, pn)
#define OLF_S(page, label, desc, pp, pn) OLR_S(OLK_FOLDER, OLS_NONE, page, label, desc, pp, pn)
#define OL_BACK(line) OLR(OLK_BACK, OLS_NONE, 0, OLW_BACK, line, 0, 0)
#define OL_EXIT OLR(OLK_EXIT, OLS_NONE, 0, 0, 0, 0, 0)
#define OL_END OLR(OLK_END, OLS_NONE, 0, 0, 0, 0, 0)

static const OlRowDef kOlDisplay[] = {
    OLV(OLS_RES, "display", "Resolution"),  // row 0 (R0: the exit scan's page 0 row 0 call carries Aspect Ratio / Display Mode / Monitor)
    OLV(OLS_ASPECT, "display", "Aspect Ratio"),
    OLV(OLS_DISPMODE, "display", "Display Mode"),
    OLV(OLS_MONITOR, "display", "Monitor"),
    OLV(OLS_FPS, "display", "Frame Rate"),
    OLV_M(OLS_RENDERER, "display", "Renderer"),  // v3.55 OL56: back on Display (R5 moved the sound rows off it)
    OLR(OLK_SCREEN, OLS_BRIGHT, 0, 0, 0, "display", "Brightness"),
    OLR(OLK_SCREEN, OLS_GAMMA, 0, 0, 0, "display", "Gamma"),
    OLV_S(OLS_SNDMODE, "sound", "Sound Mode"),  // interim (StockMoves=0): value-only, no Enter
    OLV_S(OLS_BGM, "sound", "Music Volume"),     // interim (StockMoves=0): still "BGM Volume" (the bar is drawn by label id 0x25)
    OLV_S(OLS_SE, "sound", "Effects Volume"),    // interim (StockMoves=0): still "SE Volume" (0x26)
    OL_EXIT, OL_END };
static const OlRowDef kOlControls[] = {
    OLR(OLK_SCREEN, OLS_BUTTONS, 0, 0, 0, "controls", "Buttons"),
    OLV(OLS_DEFSPEED, "controls", "Default Speed"),
    OLV(OLS_VIB, "controls", "Vibration"),
    OLV(OLS_OMNI, "controls", "Analog Movement"),
    OLV_S(OLS_HEADBOB, "room302", "Head Bob"),        // interim (StockMoves=0): above Camera (PLAN section 4)
    OLV_S(OLS_LEFTRIGHT, "room302", "Turn or Strafe"),  // interim (StockMoves=0)  // v3.57 OLP1: the plan's name (was Left / Right)
    OLV_S(OLS_INVLOOK, "room302", "Invert Look"),     // interim (StockMoves=0)
    OLF(LP_CAMERA, OLW_CONTROLS_CAMERA_L, OLW_CONTROLS_CAMERA_D, "controls", "Camera"),
    OLF_S(LP_R302, OLW_CONTROLS_ROOM_302_L, 0, "controls", "Room 302"),  // (its line: room302.inc R3DescFolder, Playable Eileen's)
    OLF_M(LP_R302, OLW_CONTROLS_ROOM_302_L, OLW_CONTROLS_ROOM_302_D, "controls", "Room 302"),  // v3.55 OL56: the plan's line (the folder
                                                                                              // holds rows Playable Eileen never owns now)
    OL_EXIT, OL_END };
static const OlRowDef kOlCamera[] = {
    OLR(OLK_VALUE, OLS_FOV, 0, OLW_CAMERA_FIELD_OF_VIEW_L, 0, "camera", "Field of View"),
    OLR(OLK_CAMSW, OLS_CAMSW, OL_CAM_ENABLE, 0, 0, "camera", "Camera"),
    OLC(OL_CAM_DIST, OLW_CAMERA_DISTANCE_L, OLW_CAMERA_DISTANCE_D, "camera", "Distance"),
    OLC(OL_CAM_SIDE, OLW_CAMERA_SIDE_L, OLW_CAMERA_SIDE_D, "camera", "Side"),
    OLC(OL_CAM_HEIGHT, OLW_CAMERA_HEIGHT_L, OLW_CAMERA_HEIGHT_D, "camera", "Height"),
    OLC(OL_CAM_TILT, OLW_CAMERA_TILT_DOWN_L, OLW_CAMERA_TILT_DOWN_D, "camera", "Tilt Down"),
    OLC(OL_CAM_FIXED, OLW_CAMERA_FIXED_ANGLES_L, OLW_CAMERA_FIXED_ANGLES_D, "camera", "Fixed Angles"),
    OLF(LP_STEER, OLW_CAMERA_STEERING_L, OLW_CAMERA_STEERING_D, "camera", "Steering"),
    OL_BACK(OLW_BACK_CONTROLS), OL_END };
static const OlRowDef kOlSteer[] = {
    OLC(OL_CAM_TURN, OLW_STEERING_TURN_SPEED_L, OLW_STEERING_TURN_SPEED_D, "steering", "Turn Speed"),
    OLC(OL_CAM_TILTSPD, OLW_STEERING_TILT_SPEED_L, OLW_STEERING_TILT_SPEED_D, "steering", "Tilt Speed"),
    OLC(OL_CAM_INVX, OLW_STEERING_INVERT_TURN_L, OLW_STEERING_INVERT_TURN_D, "steering", "Invert Turn"),
    OLC(OL_CAM_INVY, OLW_STEERING_INVERT_TILT_L, OLW_STEERING_INVERT_TILT_D, "steering", "Invert Tilt"),
    OLC(OL_CAM_WALL, OLW_STEERING_WALL_ZOOM_L, OLW_STEERING_WALL_ZOOM_D, "steering", "Wall Zoom"),
    OLC(OL_CAM_MARGIN, OLW_STEERING_WALL_MARGIN_L, OLW_STEERING_WALL_MARGIN_D, "steering", "Wall Margin"),
    OL_BACK(OLW_BACK_CAMERA), OL_END };
static const OlRowDef kOlR302[] = {
    OLR(OLK_VALUE, OLS_FOVFP, 0, OLW_ROOM302_FIELD_OF_VIEW_L, 0, "room302", "Field of View"),
    OLV(OLS_R3_DOWN, "room302", "Look Down"),
    OLV(OLS_R3_UP, "room302", "Look Up"),
    OLV(OLS_R3_FEEL, "room302", "Look Smoothing"),
    OLV_M(OLS_INVLOOK, "room302", "Invert Look"),     // v3.55 OL56 (R5): the stock First Person Camera record (cfg+0x16)
    OLV_M(OLS_HEADBOB, "room302", "Head Bob"),        // the stock Head Motion record (cfg+0x08)
    OLV_M(OLS_LEFTRIGHT, "room302", "Turn or Strafe"),  // the stock First Person Movement record (cfg+0x17)  // v3.57 OLP1
    OLV(OLS_R3_WALK, "room302", "Walk Speed"),
    OL_BACK(OLW_BACK_CONTROLS), OL_END };
static const OlRowDef kOlInterface[] = {
    OLV(OLS_LANG, "interface", "Language"),
    OLF_M(LP_SUBT, OLW_INTERFACE_SUBTITLES_L, OLW_INTERFACE_SUBTITLES_D, "interface", "Subtitles"),  // v3.55 OL56 (R5, Q2): a pure folder
    { OLK_COMBO, OLS_SUBTITLES, LP_SUBT, 0, 0, "interface", "Subtitles", "subtitles", "Subtitles", OLM_STOCK },  // the combo row (StockMoves=0)
    OLV(OLS_HGAUGE, "interface", "Health Gauge"),
    OLV(OLS_ICONS, "interface", "Item Icons"),
    OLR_S(OLK_VALUE, OLS_GRAIN_SW, 0, 0, OLW_GRAIN_SWITCH_D, "grain", "Film Grain"),  // interim (StockMoves=0): a plain switch (stock input)
    OLR_S(OLK_VALUE, OLS_SHADOW_SW, 0, 0, OLW_SHADOWS_SWITCH_D, "shadows", "Shadows"),  // interim (StockMoves=0)
    OLV_S(OLS_BLOOD, "gameplay", "Blood Colour"),  // interim (StockMoves=0)
    OLV(OLS_ENEMYHP, "interface", "Enemy Health"),
    OLV(OLS_CURSE, "interface", "Curse Meter"),
    OLV_M(OLS_MENUFONT, "interface", "Menu Font"),  // v3.55 OL56: back on Interface (R5 freed its rows)
    OLV_M(OLS_STARTUP, "interface", "Start-Up"),
    OLF_M(LP_MENUS, OLW_INTERFACE_PAUSE_MENU_L, OLW_INTERFACE_PAUSE_MENU_D, "interface", "Pause Menu"),  // v3.55 OL56: Pause Menu again
    OLF_S(LP_MENUS, OLW_MENUS_L, OLW_MENUS_D, "interface", "Pause Menu"),  // PLAN section 4's fallback (StockMoves=0): renamed Menus
    OL_EXIT, OL_END };
static const OlRowDef kOlSubt[] = {
    OLV_M(OLS_SUBTITLES, "subtitles", "Subtitles"),  // v3.55 OL56 (R5): the stock switch (cfg+0x12), the stock input 0x524AF0
    OLV(OLS_SUB0, "subtitles", "Size"), OLV(OLS_SUB1, "subtitles", "Colour"), OLV(OLS_SUB2, "subtitles", "Edge"),
    OLV(OLS_SUB3, "subtitles", "Box"), OLV(OLS_SUB4, "subtitles", "Appear"), OLV(OLS_SUB5, "subtitles", "Vanish"),
    OL_BACK(OLW_BACK_INTERFACE), OL_END };
static const OlRowDef kOlMenus[] = {
    OLV(OLS_EM0, "pause", "Storage Chest"), OLV(OLS_EM1, "pause", "Save Game"), OLV(OLS_EM2, "pause", "Load Game"),
    OLV(OLS_EM3, "pause", "Quick Save"), OLV(OLS_EM4, "pause", "Quick Load"), OLV(OLS_EM5, "pause", "Eileen"),  // v3.56 MN356: was "Eileen Outfit"
    OLV(OLS_SAVEBANKS, "pause", "Save Banks"),
    OLV_S(OLS_MENUFONT, "interface", "Menu Font"),  // interim (StockMoves=0): PLAN section 4's Interface fallback (13 -> 11 rows)
    OLV_S(OLS_STARTUP, "interface", "Start-Up"),    // interim (StockMoves=0): the same
    OL_BACK(OLW_BACK_INTERFACE), OL_END };
static const OlRowDef kOlSound[] = {
    OLV_M(OLS_SNDMODE, "sound", "Sound Mode"),  // v3.55 OL56 (R5): the stock Sound record (SndRowIn, cfg+0x0C), value-only
    OLR_M(OLK_VALUE, OLS_BGM, 0, OLW_SOUND_MUSIC_VOLUME_L, 0, "sound", "Music Volume"),      // the stock BGM Volume record: its bar
    OLR_M(OLK_VALUE, OLS_SE, 0, OLW_SOUND_EFFECTS_VOLUME_L, 0, "sound", "Effects Volume"),  // by its input (0x525672, volbar_sim.h)
    OLV(OLS_WIDTH, "sound", "Stereo Width"), OLV(OLS_REVERB, "sound", "Room Reverb"), OLV(OLS_NIGHT, "sound", "Night Mode"),
    OLV(OLS_SNDBG, "sound", "Background"),  // v3.56 SD356: Mute / Play (sound.inc SndBgIn)
    OL_EXIT, OL_END };
static const OlRowDef kOlEffects[] = {
    OLF(LP_GRAIN, OLW_EFFECTS_FILM_GRAIN_L, OLW_EFFECTS_FILM_GRAIN_D, "effects", "Film Grain"),
    OLV(OLS_FILTER, "effects", "Filter"), OLV(OLS_CUT, "effects", "Cut-scene Filter"), OLV(OLS_BLOOM, "effects", "Bloom"),
    OLV(OLS_GRADE, "effects", "Colour Grade"), OLV(OLS_LENS, "effects", "Otherworld Lens"), OLV(OLS_DOF, "effects", "Depth of Field"),
    OLV(OLS_BLUR, "effects", "Blur Strength"),
    OLF(LP_SHADOWS, OLW_EFFECTS_SHADOWS_L, OLW_EFFECTS_SHADOWS_D, "effects", "Shadows"),
    OLV_S(OLS_RENDERER, "display", "Renderer"),  // interim (StockMoves=0): PLAN section 4's Display fallback (the sound rows stay: 12 -> 11)
    OL_EXIT, OL_END };
static const OlRowDef kOlGrain[] = {
    OLR_M(OLK_VALUE, OLS_GRAIN_SW, 0, 0, OLW_GRAIN_FILM_GRAIN_D, "grain", "Film Grain"),  // v3.55 OL56 (R5): the stock Noise Effect switch
    OLR(OLK_VALUE, OLS_NSTR, 0, OLW_GRAIN_STRENGTH_L, 0, "grain", "Strength"), OLV(OLS_NTYPE, "grain", "Grain Type"),
    OLV(OLS_NCOL, "grain", "Grain Colour"), OL_BACK(OLW_BACK_EFFECTS), OL_END };
static const OlRowDef kOlShadows[] = {
    OLR_M(OLK_VALUE, OLS_SHADOW_SW, 0, 0, OLW_SHADOWS_SHADOWS_D, "shadows", "Shadows"),  // v3.55 OL56 (R5): the stock Realtime Shadow switch
    OLV(OLS_SHEDGE, "shadows", "Edge"), OLV(OLS_SHDARK, "shadows", "Darkness"), OL_BACK(OLW_BACK_EFFECTS), OL_END };
static const OlRowDef kOlGameplay[] = {
    OLF(LP_PLAYER, OLW_GAMEPLAY_PLAYER_L, OLW_GAMEPLAY_PLAYER_D, "gameplay", "Player"),
    OLF(LP_COMBAT, OLW_GAMEPLAY_COMBAT_L, OLW_GAMEPLAY_COMBAT_D, "gameplay", "Combat"),
    OLF(LP_ITEMS, OLW_GAMEPLAY_ITEMS_L, OLW_GAMEPLAY_ITEMS_D, "gameplay", "Items"),
    OLF(LP_ENEMIES, OLW_GAMEPLAY_ENEMIES_L, OLW_GAMEPLAY_ENEMIES_D, "gameplay", "Enemies"),
    OLF(LP_POWERS, OLW_GAMEPLAY_POWERS_L, OLW_GAMEPLAY_POWERS_D, "gameplay", "Powers"),
    OLF(LP_RANDO, OLW_GAMEPLAY_RANDOMIZER_L, OLW_GAMEPLAY_RANDOMIZER_D, "gameplay", "Randomizer"),
    OLV_M(OLS_BLOOD, "gameplay", "Blood Colour"),  // v3.55 OL56 (R5): the stock Blood Colour record (BldIn, cfg+0x13)
    OL_EXIT, OL_END };
static const OlRowDef kOlPlayer[] = {
    OLV(OLS_MOVE, "player", "Move Speed"), OLV(OLS_REGEN, "player", "Regeneration"), OLV(OLS_MED, "player", "Medallion"),
    OLV(OLS_GRAB, "player", "Grab Escape"), OLV(OLS_NOCLIP, "player", "No Clip"), OL_BACK(OLW_BACK_GAMEPLAY), OL_END };
static const OlRowDef kOlCombat[] = {
    OLV(OLS_OHK, "combat", "One-Hit Kill"), OLV(OLS_PACT, "combat", "Dark Pact"), OLV(OLS_QC, "combat", "Quick Charge"),
    OLV(OLS_STEAL, "combat", "Life Steal"), OLV(OLS_AMMO, "combat", "Ammo Saver"), OL_BACK(OLW_BACK_GAMEPLAY), OL_END };
static const OlRowDef kOlItems[] = {
    OLV(OLS_SHINE, "items", "Item Shine"), OLV(OLS_TOOLBELT, "items", "Toolbelt"), OLV(OLS_STACKS, "items", "Item Stacks"),
    OLV(OLS_DROPS, "items", "Supply Drops"), OL_BACK(OLW_BACK_GAMEPLAY), OL_END };
static const OlRowDef kOlEnemies[] = {
    OLV(OLS_COPIES, "enemies", "Extra Enemies"), OLV(OLS_FOLLOW, "enemies", "Door Chase"), OLV(OLS_PURSUIT, "enemies", "Pursuit"),
    OLV(OLS_REVIVE, "enemies", "Enemies Return"), OLV(OLS_FADEIN, "enemies", "Fade In"), OL_BACK(OLW_BACK_GAMEPLAY), OL_END };
static const OlRowDef kOlPowers[] = {
    OLV(OLS_DP, "powers", "Death Presence"), OLV(OLS_AI, "powers", "After Shock"), OLV(OLS_ECHO, "powers", "Echo Doubles"),
    OLV(OLS_ECHON, "powers", "Echo Count"), OLV(OLS_GT, "powers", "Ghost Teleport"), OL_BACK(OLW_BACK_GAMEPLAY), OL_END };
static const OlRowDef kOlRando[] = {  // v3.56 RI356: Specials (was Keep Specials); no Seed / New Seed rows (the seed is drawn under the line)  // v3.56 RE356: Monsters -> Enemies, Ghosts -> Spawn Site
    OLV(OLS_RZ_ITEMS, "rando", "Items"), OLV(OLS_RZ_KEEP, "rando", "Specials"), OLV(OLS_RZ_R302, "rando", "Room 302 Items"),
    OLV(OLS_RZ_MON, "rando", "Enemies"), OLV(OLS_RZ_GHOSTS, "rando", "Spawn Site"), OLV(OLS_RZ_OUTFIT, "rando", "Eileen Outfit"),
    OLV(OLS_RZ_PERVISIT, "rando", "Reroll"),  // v3.57 OLP1: the plan's name (was Reroll Visits)
    OL_BACK(OLW_BACK_GAMEPLAY), OL_END };

// the page tree: parent (-1 = a top page; -2 = a free id), the title (a hidden page's leaf: look.inc draws "Parent > Page"),
// the plan's page id, the rows
typedef struct { int parent; const char *title, *plan; const OlRowDef *rows; const char *titleStock; } OlPageDef;  // v3.55 OL56: + the
                                                                                                         // title with StockMoves=0 (0 = the same)
static const OlPageDef kOlPage[LP_MAX] = {
    { -1, OLW_T_DISPLAY, "display", kOlDisplay },
    { -1, OLW_T_CONTROLS, "controls", kOlControls },
    { -1, OLW_T_INTERFACE, "interface", kOlInterface },
    { -1, OLW_T_SOUND, "sound", kOlSound },
    { -1, OLW_T_EFFECTS, "effects", kOlEffects },
    { -1, OLW_T_GAMEPLAY, "gameplay", kOlGameplay },
    { LP_INTERFACE, OLW_T_SUBTITLES, "subtitles", kOlSubt },
    { LP_INTERFACE, OLW_T_PAUSE, "pause", kOlMenus, OLW_T_MENUS },  // v3.55 OL56: Pause Menu again (StockMoves=0: Menus)
    { LP_EFFECTS, OLW_T_GRAIN, "grain", kOlGrain },
    { LP_EFFECTS, OLW_T_SHADOWS, "shadows", kOlShadows },
    { LP_GAMEPLAY, OLW_T_PLAYER, "player", kOlPlayer },
    { LP_CONTROLS, OLW_T_CAMERA, "camera", kOlCamera },
    { LP_CONTROLS, OLW_T_ROOM302, "room302", kOlR302 },
    { LP_GAMEPLAY, OLW_T_COMBAT, "combat", kOlCombat },
    { -2, 0, 0, 0 },
    { LP_GAMEPLAY, OLW_T_ENEMIES, "enemies", kOlEnemies },
    { LP_GAMEPLAY, OLW_T_ITEMS, "items", kOlItems },
    { -2, 0, 0, 0 },
    { LP_GAMEPLAY, OLW_T_POWERS, "powers", kOlPowers },
    { LP_GAMEPLAY, OLW_T_RANDO, "rando", kOlRando },
    { -2, 0, 0, 0 },
    { LP_CAMERA, OLW_T_STEERING, "steering", kOlSteer },
};
// the order layout.inc builds the pages in: children before their parents (a folder row needs its page built)
static const int kOlBuildOrder[LP_MAX - 3] = { LP_STEER, LP_CAMERA, LP_R302, LP_SUBT, LP_MENUS, LP_GRAIN, LP_SHADOWS, LP_PLAYER,
                                               LP_COMBAT, LP_ITEMS, LP_ENEMIES, LP_POWERS, LP_RANDO, LP_DISPLAY, LP_CONTROLS,
                                               LP_INTERFACE, LP_SOUND, LP_EFFECTS, LP_GAMEPLAY };

// the interim places of R3 + R4 (a plan row on another page than the plan's, until R5 moves the stock rows; PLAN section 4's
// fallbacks): the plan's page and row, the page (plan id) it is on now, why. v3.55 OL56: StockMoves=0 only (with StockMoves=1
// every row is on its plan page: none)
typedef struct { const char *pp, *pn, *now, *why; } OlInterim;
static const OlInterim kOlInterim[] = {
    { "sound", "Sound Mode", "display", "R5 moves the stock Sound row; until then value-only on Display (no Enter)" },
    { "sound", "Music Volume", "display", "R5 (still BGM Volume: the bar is drawn by its stock label id)" },
    { "sound", "Effects Volume", "display", "R5 (still SE Volume)" },
    { "display", "Renderer", "effects", "PLAN 4 fallback: the sound rows stay on Display, so Renderer goes to Effects" },
    { "room302", "Head Bob", "controls", "R5 moves the first-person three; until then above Camera" },
    { "room302", "Turn or Strafe", "controls", "R5" },  // v3.57 OLP1
    { "room302", "Invert Look", "controls", "R5" },
    { "subtitles", "Subtitles", "interface", "R5 makes the combo row a folder; until then the switch is Interface's row" },
    { "grain", "Film Grain", "interface", "R5 moves the stock Noise Effect switch" },
    { "shadows", "Shadows", "interface", "R5 moves the stock Realtime Shadow switch" },
    { "gameplay", "Blood Colour", "interface", "R5 moves the stock Blood Colour row" },
    { "interface", "Menu Font", "pause", "PLAN 4 fallback: Interface 13 -> 11, Menu Font into Menus" },
    { "interface", "Start-Up", "pause", "PLAN 4 fallback: the same" },
};
#define OL_INTERIM_N ((int)(sizeof kOlInterim / sizeof kOlInterim[0]))
// the renames of PLAN section 4's fallbacks: a new row's or page's own word that is not the plan's (v3.55 OL56: StockMoves=0 only)
typedef struct { const char *pp, *pn, *now; } OlRename;
static const OlRename kOlRenames[] = {
    { "interface", "Pause Menu", "Menus" },  // the folder row
    { "pause", "", "Menus" },                // the page's title
};
#define OL_RENAME_N ((int)(sizeof kOlRenames / sizeof kOlRenames[0]))

// a row count's pitch (sh4windowmod.c SlotRowStep) and the y the last row ends at (rows from y 96, 24 high)
static int OlPitch(int n) { return n <= 8 ? 32 : n == 9 ? 28 : n == 10 ? 26 : n == 11 ? 24 : 22; }
static int OlLastRowEnd(int n) { return n > 0 ? 96 + OlPitch(n) * (n - 1) + 24 : 96; }
// v3.55 OL56: a row is in the layout of moves (1 = StockMoves=1)
static int OlRowOn(const OlRowDef *d, int moves)
{
    return d->mode == OLM_BOTH || (d->mode == OLM_MOVES) == (moves != 0);
}
// the rows of a page's list in the layout of moves: the i-th row (NULL past the end) and the count
static const OlRowDef *OlRowNth(const OlRowDef *r, int moves, int i)
{
    int k;
    for (k = 0; r && r[k].kind != OLK_END; ++k)
        if (OlRowOn(&r[k], moves) && i-- == 0) return &r[k];
    return 0;
}
static int OlRowCount(const OlRowDef *r, int moves)
{
    int n = 0;
    while (OlRowNth(r, moves, n)) ++n;
    return n;
}
// a page's title in the layout of moves (the leaf; look.inc draws "Parent > Page")
static const char *OlTitle(int lp, int moves)
{
    if (lp < 0 || lp >= LP_MAX) return 0;
    return !moves && kOlPage[lp].titleStock ? kOlPage[lp].titleStock : kOlPage[lp].title;
}
static int OlBlueKind(int kind) { return kind == OLK_SCREEN || kind == OLK_COMBO || kind == OLK_FOLDER; }  // drawn blue
static int OlOpensKind(int kind) { return kind == OLK_COMBO || kind == OLK_FOLDER; }  // Enter opens page arg
// the top page of lp (walks the tree; -1 = none within 4 levels)
static int OlTopOf(int lp)
{
    int i;
    for (i = 0; i < 4 && lp >= LP_N && lp < LP_MAX; ++i) lp = kOlPage[lp].parent;
    return lp >= 0 && lp < LP_N ? lp : -1;
}
#endif
