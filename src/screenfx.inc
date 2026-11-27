// ================================================================ v3.16.0: Screen Effects v2 (scene filters, Noise Type, Thermal)
// Plan: staging/screen_effects_v2/PLAN.md (Codex APPROVED, round 3); RE: RE_fade.md, RE_order.md, RE_thermal.md beside
// it; build notes: BUILD_NOTES.md there. v3.15.0 (staging/screen_effects/) is the base. Absolute VAs (GOG v7). Included
// by sh4windowmod.c after subtitles.inc (items.inc / items_draw.inc before it give the device typedefs, vtable indices
// and g_spInstalled). The settings, the page rows and the preview stamp live in sh4windowmod.c.
//
// Core patches (all or nothing, as v3.15.0):
//   0x432FBE `call 567040` (grain grid, type 1) and 0x432F9E `call 567040` (dust, type 0) -> NoiseDraw(type, level):
//   the level x Noise Strength; Noise Type other than Stock draws our own animated layer instead of the type-1 grid
//   (E5; the stock gate Option 3 Noise Effect Off still stops both calls, so it stops our layer too).
//   0x55AC29 (after the stock post pass `call 55A260` at 0x55AC24, which stays stock) -> hkFxAfterPost: the Screen
//   Effects page preview (same-frame stamp, FX-002) and the frame that takes the pause / overlay picture
//   ([0x12E3E2C] == 1, RE_order 2): whole-frame pass there.
// The scene pass (E6) runs from items.inc's hkHudSparkle at 0x4EFBFD (FxSceneFrame, before Item Shine and the HUD):
// gameplay and real-time cutscenes, and the examine / message overlay (overlay 1 with callback 0x50A5A0). No hook
// there (another mod) -> no gameplay filter, logged (FXV2-002: no whole-frame fallback).
// Thermal patches (a second all-or-nothing group, RE_thermal 6, fallback design): 0x52AEB5 / 0x52AF95 (model draw /
// its shadow volume: ghosts skipped), 0x4273A6 (ambient copy: raised for enemies), 0x47DFC3 (Jasper's flame emitters
// skipped), all gated on Filter == Thermal and gameplay phase 0 (FXV2-004: cutscenes show the palette only).
// Every device state a pass touches is read first (any failed query: the pass is skipped) and put back; the render
// target must be the back buffer; our textures are MANAGED (made once per device, one LockRect at creation); our
// ps.1.1 shaders are made lazily per device and each CreatePixelShader result is logged (smoke test). Live finding
// (v3.15.0): GOG's wrapper draws nothing for an instruction reading TWO constant registers: every instruction below
// reads at most one.
// Ghost Vision (PLAN section 7, RE_ghostvision.md; review GV-001..003, FXV2-005): three edge passes on the frame copy
// (black world, lines); in gameplay phase 0 the ghosts collected at 0x52AEB5 are drawn again after them with an additive
// glow shader (a third all-or-nothing group: 0x4292EF / 0x429338, the renderer's two draw calls, override the states
// only while our redraw runs; without it the ghosts glow as additive sprites) and the haunting damage areas of the
// game's aura snapshot 0x10A1EC0 are drawn as rings on the player's floor. The game's grain / dust and our Noise Type
// layers are skipped while it draws.

// ================================================================ tuning (fixed parameters; host: tune from screenshots)
// Pixel sizes are at 720 lines (scaled by the game's render height). Strengths are 0..1 lerps from "no change".
// Overlays are 2x-modulate layers (factor = 1 + strength * (texture factor - 1)), so grain, scanlines and the CRT mask
// keep the mean brightness; vignettes / the film burn only darken the edges.
// --- CRT: three offset samples (R left, G centre, B right), saturation, contrast, aperture-grille mask, scanlines, vignette
#define CRT_CHROMA_PX      1.0f    // red / blue sample offset
#define CRT_SAT            0.12f   // extra saturation (0 = none)
#define CRT_CON            1.06f   // contrast around 0.5 (0..2)
#define CRT_BRI            0.03f   // added after (compensates mask / scanlines / vignette)
#define CRT_MASK           0.18f   // RGB aperture grille, 3-pixel period
#define CRT_SCAN           0.30f   // scanlines (about 360 per screen)
#define CRT_VIG            0.30f
// --- Vivid: saturation plus a light unsharp mask (centre minus the mean of three offset samples)
#define VIVID_SAT          0.85f   // user 2026-11-15: "more drastic and wide" (saturation 1 + SAT, max 2)
#define VIVID_SHARP        0.75f   // 0..1
#define VIVID_RADIUS_PX    1.0f
#define VIVID_CON          1.15f
#define VIVID_BRI          0.0f
// --- Sepia: luma x tint + warm highlights (luma^2 x warm), mixed with the picture; soft vignette
#define SEPIA_TINT_R 1.12f
#define SEPIA_TINT_G 0.94f
#define SEPIA_TINT_B 0.70f
#define SEPIA_WARM_R 0.10f
#define SEPIA_WARM_G 0.04f
#define SEPIA_WARM_B (-0.06f)
#define SEPIA_MIX          0.88f
#define SEPIA_CON          1.04f
#define SEPIA_BRI          0.01f
#define SEPIA_VIG          0.30f
// --- Old Film (animated per 30 Hz tick): sepia-grey tone, flicker, gate weave, scratches / dust / hair, edge burn, grain
#define OLDF_TINT_R 1.06f
#define OLDF_TINT_G 0.96f
#define OLDF_TINT_B 0.82f
#define OLDF_WARM_R 0.06f
#define OLDF_WARM_G 0.03f
#define OLDF_WARM_B (-0.02f)
#define OLDF_MIX           0.92f
#define OLDF_CON           1.12f
#define OLDF_BRI           0.02f
#define OLDF_FLICKER       0.035f  // +- brightness per tick
#define OLDF_WEAVE_PX      0.6f    // +- frame jitter per tick (vertical half of it); v3.23.0 (user item 10): not used by
                                   // Old Film any more (the look table gives it weave 0: no shake)
#define OLDF_GRAIN         0.12f
#define OLDF_BURN          0.62f   // edge burn / heavy vignette
#define OLDF_SPECKS        0.90f   // artifact strength
#define OLDF_DUST_MAX      3       // dust / white specks born per tick at most
#define OLDF_HAIR_P        0.10f   // chance per tick of a hair
#define OLDF_SCRATCH_P     0.18f   // chance per tick of a vertical scratch
// --- Noir: luma, a shadow lift x' = x + LIFT x (1 - x), an S-curve y = x' + CURVE d - 4 CURVE d^3 (d = x' - 0.5), grain, vignette
#define NOIR_GAIN          1.0f
#define NOIR_LIFT          0.55f   // 0..1
#define NOIR_CURVE         0.30f   // 0..0.5 (0.5 = flat at black and white)
#define NOIR_GRAIN         0.10f
#define NOIR_VIG           0.40f
// --- Night Vision: luma gain x green phosphor, bloom of the blurred highlights, animated noise, goggle vignette
#define NV_GAIN            1.62f
#define NV_TINT_R 0.30f
#define NV_TINT_G 1.00f
#define NV_TINT_B 0.40f
#define NV_FLOOR_G         0.035f  // phosphor glow added everywhere
#define NV_BLOOM_TH        0.45f   // blurred luma above this blooms
#define NV_BLOOM           0.70f
#define NV_BLOOM_PX        2.5f
#define NV_GRAIN           0.22f
#define NV_VIG             0.40f   // user 2026-11-15: a soft corner vignette like Night Vision Mono, no goggle edge
// --- Night Vision Mono (Sam Fisher style): grey, gain, light grain, slight vignette
#define NVM_GAIN           1.27f
#define NVM_CON            1.06f
#define NVM_BRI            0.02f
#define NVM_GRAIN          0.12f
#define NVM_VIG            0.30f
// --- Thermal: luma x GAIN = L; R = sat((L - R0) x RS), G = sat((L - G0) x GS), B = sat((L - B0) x BS) + a blue hump
// (rise sat(L x RISE) x fall sat((FALL_END - L) x FALL_S)) x HUMP colour, + FLOOR: black - deep blue - purple - red -
// orange - yellow - white. Slopes at most 4 (ps.1.1 _x4 with constants <= 1).
// v3.22.0 (user item 12: "behaves like light / shadow vision"): the WORLD maps onto a cold ramp only (navy - blue -
// azure - pale cyan: a lamp is never hot); living bodies are redrawn after it in heat colours (ThHeat, section 2 of
// staging/feedback_v322/PLAN.md). The v3.21 hot ramp (R0 0.25 / 2.2, G0 0.55 / 2.5, B0 0.80 / 4.0, cold 0.20 0.02 0.58
// below 0.55) is the fallback look of THERM_HOT_* when the heat redraw is not available.
#define THERM_GAIN         1.35f
#define THERM_R0 0.92f              // red: almost never (a hint of white at the very brightest)
#define THERM_RS 1.2f
#define THERM_G0 0.30f              // green from mid grey: blue -> azure -> pale cyan
#define THERM_GS 1.4f
#define THERM_B0 0.05f              // blue first
#define THERM_BS 1.6f
#define THERM_COLD_END     0.35f    // below this the navy floor fades in (full at COLD_END - 1 / COLD_S)
#define THERM_COLD_S       3.0f
#define THERM_COLD_R 0.04f
#define THERM_COLD_G 0.03f
#define THERM_COLD_B 0.22f
#define THERM_HOT_R0 0.25f          // the v3.21 hot ramp (fallback: no heat redraw)
#define THERM_HOT_RS 2.2f
#define THERM_HOT_G0 0.55f
#define THERM_HOT_GS 2.5f
#define THERM_HOT_B0 0.80f
#define THERM_HOT_BS 4.0f
#define THERM_HOT_COLD_END 0.55f
#define THERM_HOT_COLD_S   2.0f
#define THERM_HOT_COLD_R 0.20f
#define THERM_HOT_COLD_G 0.02f
#define THERM_HOT_COLD_B 0.58f
#define THERM_HOT_AMBIENT  2.0f    // fallback only: the model ambient (all three channels) of a hot draw
// --- v3.22.0 the body heat redraw (kPsHeat): V = luma(texel) x DETAIL + BODY x heat; R / G / B = sat((V - start) x
// slope): dark red - red - orange - yellow - white. Slopes / 4 must stay <= 1 (ps.1.1 constants).
#define TH_DETAIL          0.45f
#define TH_BODY            0.62f
#define TH_R0 0.15f
#define TH_RS 2.2f
#define TH_G0 0.50f
#define TH_GS 2.6f
#define TH_B0 0.82f
#define TH_BS 4.0f
#define TH_ALPHA_K         1.6f    // body opacity = min(1, heat x this): it reddens before it fades into the world
#define TH_MAX             32      // bodies redrawn a frame
#define TH_MIN_HEAT        0.04f   // a body cooler than this is left to the world palette
#define TH_ZBIAS           0.0003f // v3.24.0 (user's flicker report): the heat redraw's clip z x (1 + this): 0.03 % nearer
                                   // in the reversed depth (about 0.3 units at 1 000, 0.05 at 150), so it wins the near-ties
                                   // with the stock draw; [ScreenEffects] ThermalDepthBias (per mille, 0..1, 0 = off; review
                                   // SH4-324-R1: kept small so a surface in front still hides the body)
                                   // v3.29.1 (user: blue flicker on both Eileens, stage 5 room 4): the clip z row is c9 only
                                   // for rooms / rigid models; skinned characters project with c10..c13 (z row c12), so
                                   // until v3.29.1 the bias never reached a body (staging/v3291_feedback/thermal/RE_THERMAL.md)
#define TH_ZBIAS_TEST_MAX  10.0f   // v3.29.1: ThermalDepthBias above 1 per mille is accepted as a test value (logged)
#define TP_HEAT_R 1.00f            // v3.22.0 footprints in heat colours (drawn after the world palette)
#define TP_HEAT_G 0.42f
#define TP_HEAT_B 0.08f
// --- Ghost Vision (RE_ghostvision 3): pass L lifts the darks y = x + 2 LIFT (x - x^2); pass E the gradient
// sat(8 sum (dX^2 + dY^2)) from samples +-EDGE_PX; pass G lrp(MIX, blur, sharp) - THRESH, x 8, saturated, x the line
// colour (black elsewhere). Then (gameplay phase 0) the ghosts glow and the damage areas are drawn as rings.
#define GV_LIFT            0.35f   // 0..1
#define GV_EDGE_PX         1.5f    // gradient sample offset
#define GV_GRADE_PX        1.5f    // the blur triangle of the grade pass
#define GV_MIX             0.35f   // share of the soft halo (the blur; the rest is the sharp line)
#define GV_THRESH          0.20f   // below this no line (noise floor)
#define GV_LINE_R          0.30f   // the world's lines: cold blue-grey
#define GV_LINE_G          0.50f
#define GV_LINE_B          0.62f
#define GV_GLOW_R          0.95f   // the ghost colour: v3.23.0 (user item 3) a cold white (v3.22: 0.35 / 1.00 / 0.75 green)
#define GV_GLOW_G          0.97f
#define GV_GLOW_B          1.00f
#define GV_GLOW_TEX        0.90f   // model glow: texture x GLOW x TEX ...
#define GV_GLOW_FLOOR      0.25f   // ... + GLOW x FLOOR (also untextured parts glow)
#define GV_SPRITE          0.80f   // the sprite fallback (no draw-call override): its strength
// v3.26.0 (user, round 1: "blood-rooted"): [ScreenEffects] SpiritGhost=Blood ramps the ghost from a deep crimson (dark
// texels) to a pale rose white (bright texels); White = the v3.23.0 look above
#define GV_BLOOD_PALE_R    1.00f
#define GV_BLOOD_PALE_G    0.86f
#define GV_BLOOD_PALE_B    0.84f
#define GV_BLOOD_DEEP_R    0.42f
#define GV_BLOOD_DEEP_G    0.02f
#define GV_BLOOD_DEEP_B    0.06f
#define GV_BLOOD_GAIN      1.00f   // luma x this: a white texel = the pale end exactly (review SVR-101; tune live)
#define GV_PULSE           0.25f   // a slow pulse: x (1 - PULSE .. 1)
#define GV_PULSE_S         1.6f    // seconds per pulse
// --- v3.17.0 Spirit Vision (staging/spirit_vision/PLAN.md, RE_spiritvision.md)
#define GV_DEPTH           1       // S3: 1 = hidden part dim (ZFUNC LESS) + visible part full (GREATEREQUAL); 0 = Z off
#define SV_HIDDEN          0.45f   // strength of what lies behind scene geometry
#define SV_ZSCALE          (1.0f / 128.0f)  // S3, live: the stored depth's effective near plane = [0x12E3E7C] / 128
#define SV_ZBIAS           1.0f
#define SV_NODEPTH         0.70f   // GV_DEPTH 0: one pass at this strength
#define SV_COL_SEG         32      // the SpiritTest probe column (SvColumn): segments around, height bands, shells
#define SV_COL_BANDS       8
#define SV_COL_K           0.16f   // strength of one shell (additive, 3 shells + rim)
#define SV_COL_RIM         0.55f   // extra at the silhouette (1 - |n . view|)^2
#define SV_COL_DY          400.0f  // half height of the probe column
#define SV_COL_R302_H      350.0f  // Room 302 hauntings: their volume from the player's feet up (a visual bound)
#define SV_GROW_S          1.8f    // S4: a haunting aura grows back over this many seconds
#define SV_FADE_S          0.4f    // ... and fades out over this many
#define SV_PREVIEW         0.25f   // a downed ghost's preview at most this scale ...
#define SV_PREVIEW_S       4.0f    // ... reached as its get-up countdown falls from this to 0
#define SV_REGROW_S        2.5f    // v3.23.0 (user item 17): a downed ghost's veins grow back over its last 2.5 s down ...
#define SV_RETRACT_S       0.6f    // ... after pulling back into its body over this when it falls
#define SV_BONES           24      // v3.23.0 (user item 15): ghost bones read for the vein roots
#define SV_TRACKS          64
#define SV_PORTAL_MAX      8
#define SV_HOLY_MAX        8       // candles a frame
#define SV_PORTAL_K        0.85f   // S5 wall portals
#define SV_HOLY_R          400.0f  // S6 candle reach (3D): the flare test (a damage source this near)
#define SV_HOLY_FLARE      1.6f    // a flaring candle: this much brighter ...
#define SV_WARD_S          1.0f    // the medallion's glow fades over this many seconds after the last repelled frame
// --- v3.21.0 cinematic auras (staging/feedback_v321/PLAN.md section 3): sprites from the FXX_SVATLAS atlas
#define SV_SPR_MAX         1000    // v3.26.0: 400 -> 1000 (aura, seep, drips); sprites a depth pass (built once a frame, drawn in each pass)
#define SV_SRC_MAX         12      // sources drawn a frame (hauntings, candles, the medallion), nearest to the player first
#define SV_OWNERS          8       // ... the nearest this many own particles
#define SV_PT_MAX          256     // the particle pool
#define SV_OWN_MAX         16      // per-source state (first seen, halo phase)
#define SV_SPR_MAXH        0.80f   // a sprite's screen radius at most this x the viewport height
#define SV_NEAR0           60.0f   // near-camera fade: none at (eye distance - world radius) <= NEAR0 ...
#define SV_NEAR1           250.0f  // ... full at >= NEAR1
#define SV_MIST_MAX        350.0f  // haunting: mist puff size min(R, this) (also the vein / blob scale)
#define SV_SPREAD          0.60f   // haunting: sprites spread over min(R x SPREAD, SPREAD_MAX) around the centre
#define SV_SPREAD_MAX      500.0f
#define SV_HAUNT_UP        320.0f  // haunting volume outside Room 302: from the source y this far up ...
#define SV_HAUNT_DN        80.0f   // ... to this far down (y down)
#define SV_MIST_K          0.50f   // mist puff opacity (premultiplied, ONE / INVSRCALPHA)
#define SV_VEIN_K          0.80f   // vein brightness (additive)
#define SV_VEIN_DARK_K     0.55f   // the black vein's opacity
#define SV_GLOW_K          0.30f   // haunting base glow (additive)
#define SV_BLOB_K          0.60f   // plasma-blood blobs (additive) ...
#define SV_BLOB_DARK_K     0.50f   // ... over a dark copy (premultiplied)
#define SV_CRACK_P         0.07f   // chance a vein crackles bright in an 80 ms slot
#define SV_CRACK_S         0.08f
#define SV_ASH_N           16      // ash flakes alive per haunting (1 in 5 an ember)
#define SV_ASH_K           0.85f
#define SV_REPEL_K         0.50f   // a haunting the medallion repels: its crimson look at this strength, plus gold sparks
#define SV_HOLY_VR         400.0f  // candle visual radius R (v3.22.0: 240 -> the candle's real reach 400; user item 3b)
#define SV_MEDAL_VR        150.0f  // the medallion's glow radius
#define SV_MEDAL_CHEST     250.0f  // ... at the player's chest (above the feet; v3.22.0: the fallback only, bone 9 first)
#define SV_MEDAL_FWD       14.0f   // v3.22.0: in front of the chest bone (the medallion hangs on the sternum)
#define SV_MEDAL_CALM      0.55f   // v3.22.0 (user item 4): worn, not repelling: a calm glow at this strength
#define SV_THIN_S          1.1f    // v3.22.0 (item 3b): a thin fast ring every this many seconds ...
#define SV_THIN_LIFE       1.6f    // ... living this long ...
#define SV_THIN_R0         0.25f   // ... growing from R x R0 ...
#define SV_THIN_R1         1.6f    // ... to R x R1 (it travels further than the main pulse)
#define SV_THIN_K          0.55f
#define SV_HOLY_GLOW_K     0.40f   // gold core glow
#define SV_HALO_K          0.45f   // pulse halo start strength
#define SV_PULSE_S         2.60f   // a new halo every this many seconds (v3.22.0: 0.9 -> 2.6, user item 3b: much slower) ...
#define SV_PULSE_FLARE_S   1.40f   // ... while flaring (a haunting near the candle)
#define SV_HALO_LIFE       3.4f    // a halo's life (at the calm period; scaled with the pulse period; v3.22.0: 1.2 -> 3.4)
#define SV_ORB_N           12      // orbs per holy source: 12 .. 16
#define SV_ORB_K           0.85f
#define SV_ORPHAN_S        0.5f    // a particle whose source went away fades over this many seconds
#define SV_FLAME_H         72.0f   // S8 soul flame height (world units; v3.22.0: 45 -> 72, user item 5; v3.28 FIRE: SF_H)
#define SV_FLAME_WALTER    2.2f    // v3.22.0 (item 8): Walter's fire, x this size
#define SV_SOUL_PLAYER     0x40    // v3.22.0 (item 7): the soul list's type for the player (bone 9, as the humanoids)
#define SV_FLAME_PLAYER    0.8f    // v3.22.0 (item 7): the player's fire, x this size
#define SV_SOUL_NPC        0x41    // v3.24.0 (user items 2 / 3): a living person (companion Eileen / Cynthia, the trapped
                                   // Cynthia, other non-enemy full-skeleton models): the player's white-gold fire, no HP
#define SV_FLAME_K         0.90f   // (v3.28 FIRE: SF_K)
#define SV_SOULS           32
#define TP_STEP            40.0f   // S9 Thermal footprints: one print per this much walking
#define TP_SIDE            10.0f   // left / right of the walking line
#define TP_W               24.0f   // print size (world units)
#define TP_L               42.0f
#define TP_LIFE_S          6.0f
#define TP_K               1.00f   // brightness of a fresh print (white = hot under the palette)
#define TP_MAX             64
#define TP_ENEMIES         12      // v3.17.1: walking enemies with footprints (Gum Head, Twin Victim, Sniffer Dog, Walter)
#define THERM_LIVING_BONES 30      // v3.17.1: a non-enemy model with at least this many bones is alive (hot)
// --- Vignette
#define VIG_ONLY           0.92f   // user 2026-11-15: "a bit more visible"
// --- Noise Type layers: strength = the game's grain level (after Noise Strength) / 255 x gain
#define FXN_FINE_GAIN      1.6f
#define FXN_FINE_PX        1.0f    // grain texel size
#define FXN_COARSE_GAIN    1.8f
#define FXN_COARSE_PX      2.5f
#define FXN_FILM_GAIN      1.5f
#define FXN_FILM_DUST_GAIN 5.0f    // dust strength (the same level share)
#define FXN_FILM_DUST_MAX  2
#define FXN_VHS_GAIN       1.6f    // horizontal line noise
#define FXN_VHS_BAND_GAIN  4.0f    // the tracking band
#define FXN_VHS_BAND_H     0.07f   // its height (share of the screen)
#define FXN_VHS_BAND_TICKS 240     // ticks for one pass down the screen
// v3.18.0 (screen_effects_v3 PLAN N1 / N2)
#define FXN_STOCK_GAIN     1.5f    // Stock type with a Noise Colour: Refined's replica of the game's grid ...
#define FXN_STOCK_PX       1.5f    // ... 40 x 30 cells of 16 texels at 720 lines
#define FXN_ROLL_GAIN      3.0f    // Scan Roll: the band's strength (x level / 255)
#define FXN_ROLL_H         0.06f   // its height (share of the screen)
#define FXN_ROLL_S         1.5f    // seconds for one roll over the screen
#define FXN_ROLL_GAP0      4.0f    // seconds between rolls: 4 .. 10
#define FXN_ROLL_GAP1      10.0f
#define FXN_RAIN_GRAIN     0.6f    // Rain: a faint Fine grain under the drops
#define FXN_RAIN_GAIN      4.0f    // drops / streaks strength (x level / 255)
#define FXN_RAIN_DROPS     24
#define FXN_RAIN_STREAKS   5
#define NVH_T              0.55f   // v3.18.0 N6 night-vision highlights: luminance above this flares ...
#define NVH_K              1.4f    // ... this much (added)

// ================================================================ the filter table
enum { FXF_NONE, FXF_CRT, FXF_VIVID, FXF_SEPIA, FXF_OLDFILM, FXF_NV, FXF_NVMONO, FXF_THERMAL, FXF_GHOST,
       FXF_VIGNETTE };  // v3.18.0: Noir removed (its technique stays unused)
enum { FXT_NONE, FXT_TONE, FXT_CRT, FXT_VIVID, FXT_NOIR, FXT_NV, FXT_THERMAL, FXT_GVLIFT, FXT_GVEDGE, FXT_GVGRADE,
       FXT_GVGLOW, FXT_NCOL, FXT_HILITE, FXT_HEAT, FXT_GVBLOOD,
       FXT_PPBRIGHT, FXT_PPMEAN, FXT_PPUP, FXT_PPCOMP, FXT_PPGRADE, FXT_PPLENS,  // v3.43 PP343: post.inc's (bloom, grading, lens)
       FXT_N };  // shader techniques (Ghost Vision: four, drawn by GvPass;
                                                              // v3.22.0 HEAT: Thermal's body redraw)
enum { FXX_VIG, FXX_BURN, FXX_GOGGLE, FXX_SCAN, FXX_MASK, FXX_NOISE, FXX_CNOISE, FXX_VHS, FXX_ATLAS, FXX_HEAT, FXX_GLOW,
       FXX_FLAME, FXX_ROLL, FXX_DROP, FXX_STREAK, FXX_SVATLAS, FXX_TRAIL, FXX_FOG, FXX_WISP, FXX_SMOKE, FXX_N };  // textures (v3.26.0: TRAIL; v3.27.0: FOG, WISP; v3.27.1: SMOKE)
#include "spiritfire_sim.h"  // v3.28 FIRE: the spirit fires' pure code (SfAtlasTexel for FXX_FLAME; spiritfire.inc draws)
static DWORD DpFogTexel(int x, int y);  // v3.27.0 deathpresence_draw.inc
static DWORD DpWispTexel(int x, int y);
static DWORD DpSmokeTexel(int x, int y);  // v3.27.1: 4 x 2 cells of 256 (smoke, crest, halo, soft, strand)
static void DpPostDraw(void *dev, const Viewport8 *vp);  // v3.27.2: the fog's Spirit Vision / Thermal look, after the filter
static int g_dpProbe;  // v3.27.2: hidden [ScreenEffects] DeathPresenceProbe (the depth probe squares, deathpresence_draw.inc)
static int g_dpZExact = 1;  // v3.27.3: hidden [ScreenEffects] DeathPresenceDepth (1 exact depth, 0 v3.27.2's x 1/128)
static void DpCollect(const BYTE *task, BYTE *model);
static void EchoCollect(const BYTE *task, BYTE *model);  // v3.30 ECHO: echo.inc (her body drawn this frame, the 3D viewport)
struct FxSavedTag;  // v3.30 ECHO: FxSaved (defined below)
static void EchoPostDraw(void *dev, const struct FxSavedTag *gameSv, int look);  // v3.30 ECHO: after Spirit Vision / Thermal
static float DpHeatK(const BYTE *task);  // v3.27.1: a Death Presence victim's body heat factor (deathpresence.inc)
static void AiCollect(const BYTE *task, BYTE *model);  // v3.29 AFTERIMAGE: afterimage.inc (her frame, shocked bodies)
static void AiPostDraw(void *dev, int look);  // v3.29 AFTERIMAGE: the trail after Thermal (1) / Spirit Vision (3)
static void DfxCollect(const BYTE *task, BYTE *model);  // v3.31 DEPTH331: depthfix.inc (the bodies whose depth it restores)
static volatile LONG g_aiZ;  // v3.29 AFTERIMAGE: set only while an after-image redraw runs: the glow override keeps the depth test
// v3.56 PV356: preview.inc (the Effects page's preview: the pause's look layers, the title scene; included after shadows.inc)
static void PvCollect(const BYTE *task, BYTE *model);  // 0x52AEB5: the capture frame's bodies / ghosts for the layers
static void PvCyn(BYTE *model);                        // 0x4EE376: the trapped Cynthia, the same
static void PvCapture(void);                           // 0x4EFBFD, the capture frame, after FxCaptureClean
static void PvPageDraw(int where);                     // the layer of the filter over the page's / the pause's picture
static int PvSceneDraw(void *dev);
static void PvBackNote(int back);
static void PvFrameEnd(void);                          // FxAfterPost
static void PvInstall(void);                           // ScreenFxInstall: [ScreenEffects] PreviewLayers / PreviewScene
static int PvF7Hidden(void);                           // FxFilterDesc: no PreviewKey sentence while the scene is the backdrop
static void PvBeforeReset(void);                       // shadows.inc ShBeforeReset: the layers' targets released
// v3.58 PV358: g_pvNoZw is gone (the preview's heat redraw writes the living bodies' depth again, as the live one: their
// near faces win; v3.56's no-depth redraw showed the inside of a close-camera-faded body: USER_FEEDBACK_v357 "Thermal ...
// only displaying the inside faces of her model")
static int g_pvColl;                                   // GvNote: the preview's own collection logs no "collected" line
// v3.58 PV358: [ScreenEffects] Preview / FilterStrength (preview.inc)
static int PvBackPick(void *dev, void **tex, UINT *pw, UINT *ph, int *pause);  // FxBackdropC: by Preview (was PvBackPick(dev))
static int PvPreviewOff(void);                         // Preview Off: no backdrop on the Effects pages (shadows.inc ShMode too)
static void PvOffNote(int lp);                         // FxBackdropC: the page opened with Preview Off (logged once an opening)
static int PvIsStockPath(const char *path);            // FxPrevWrite: F7 / the auto-capture never write the stock picture
static void PvFsGrab(void *dev, int f);                // FxPass: Filter Strength Low / Medium: the plain frame before the filter
static void PvFsMix(void *dev);                        // FxPass / GvPass: ... laid back over the filter (before the heat / ghosts)
static int __cdecl PvFsIn(BYTE *cfg, int count);       // Effects > Filter Strength (look.inc: grey with Filter None)

typedef struct {
    BYTE tech, vigTex, specks, pad;
    float tint[3], warm[3], mix, con, bri;  // tone part (TONE; CRT / VIVID use mix / con / bri)
    float px;                               // sample offset (CRT chroma, Vivid radius, NV bloom radius)
    float vig, scan, mask, grain;           // overlay strengths (0 = none)
    float flicker, weavePx;                 // Old Film
} FxLook;
static const FxLook kFxLook[FX_FILTERS] = {
    { FXT_NONE },  // None (never drawn)
    { FXT_CRT, FXX_VIG, 0, 0, { 1, 1, 1 }, { 0, 0, 0 }, -CRT_SAT, CRT_CON, CRT_BRI, CRT_CHROMA_PX, CRT_VIG, CRT_SCAN, CRT_MASK, 0, 0, 0 },
    { FXT_VIVID, FXX_VIG, 0, 0, { 1, 1, 1 }, { 0, 0, 0 }, -VIVID_SAT, VIVID_CON, VIVID_BRI, VIVID_RADIUS_PX, 0, 0, 0, 0, 0, 0 },
    { FXT_TONE, FXX_VIG, 0, 0, { SEPIA_TINT_R, SEPIA_TINT_G, SEPIA_TINT_B }, { SEPIA_WARM_R, SEPIA_WARM_G, SEPIA_WARM_B },
      SEPIA_MIX, SEPIA_CON, SEPIA_BRI, 0, SEPIA_VIG, 0, 0, 0, 0, 0 },
    { FXT_TONE, FXX_BURN, 1, 0, { OLDF_TINT_R, OLDF_TINT_G, OLDF_TINT_B }, { OLDF_WARM_R, OLDF_WARM_G, OLDF_WARM_B },
      OLDF_MIX, OLDF_CON, OLDF_BRI, 0, OLDF_BURN, 0, 0, OLDF_GRAIN, OLDF_FLICKER, 0.0f /*v3.23.0: no weave*/ },
    { FXT_NV, FXX_VIG, 0, 0, { NV_TINT_R, NV_TINT_G, NV_TINT_B }, { 0, 0, 0 }, 1, 1, 0, NV_BLOOM_PX, NV_VIG, 0, 0, NV_GRAIN, 0, 0 },
    { FXT_TONE, FXX_VIG, 0, 0, { NVM_GAIN, NVM_GAIN, NVM_GAIN }, { 0, 0, 0 }, 1, NVM_CON, NVM_BRI, 0, NVM_VIG, 0, 0, NVM_GRAIN, 0, 0 },
    { FXT_THERMAL, FXX_VIG, 0, 0, { 1, 1, 1 }, { 0, 0, 0 }, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { FXT_GVLIFT, FXX_VIG, 0, 0, { 1, 1, 1 }, { 0, 0, 0 }, 1, 1, 0, GV_EDGE_PX, 0, 0, 0, 0, 0, 0 },  // Ghost Vision (GvPass)
    { FXT_NONE, FXX_VIG, 0, 0, { 1, 1, 1 }, { 0, 0, 0 }, 0, 1, 0, 0, VIG_ONLY, 0, 0, 0, 0, 0 },
};
// v3.58 PV358 (preview.inc): Filter Strength Ultra scales a copy of the look and two macro constants; 0 = untouched (High)
static int PvFsLookApply(FxLook *L, int f);
static void PvFsConstsApply(int f, float c[][4]);

// ================================================================ ps.1.1 shaders (hand-assembled D3D8 tokens)
// Encoding as the game's own ps0 at 0x5D52A0 and v3.15.0's live-tested shader: opcode token, then destination
// 0x800F000n (r, write mask rgba; result modifier _sat = bit 20, shift _x2 = 1 << 24, _x4 = 2 << 24), sources r
// 0x80E4000n, c 0xA0E4000n, t 0xB0E4000n (swizzle .rgba = E4; .a replicate = FF; source modifier bits 24..27: _bias 2,
// 1-x 6). Opcodes: add 2, sub 3, mad 4, mul 5, dp3 8, lrp 0x12, tex 0x42. At most 8 arithmetic instructions, at most
// ONE constant register read per instruction (live finding).
#define PS_VER   0xFFFF0101u
#define PS_END   0x0000FFFFu
#define PS_TEX(n) 0x00000042u, (0xB00F0000u | (n))
#define D_R(n)   (0x800F0000u | (n))
#define D_SAT    0x00100000u
#define D_X2     0x01000000u
#define D_X4     0x02000000u
#define S_R(n)   (0x80E40000u | (n))
#define S_C(n)   (0xA0E40000u | (n))
#define S_T(n)   (0xB0E40000u | (n))
#define S_RA(n)  (0x80FF0000u | (n))  // r.a replicated
#define S_BIAS   0x02000000u          // x - 0.5
#define S_INV    0x06000000u          // 1 - x
#define I_ADD 2u
#define I_SUB 3u
#define I_MAD 4u
#define I_MUL 5u
#define I_DP3 8u
#define I_LRP 0x12u

// TONE (Sepia, Old Film, Night Vision Mono). c0 luma weights, c1 tint / 2, c2 warm, c3 mix, c4 0.5, c5 con / 2,
// c6 0.5 + bri (+ flicker). out = sat(con (lrp(mix, L tint + L^2 warm, t0) - 0.5) + 0.5 + bri)
static const DWORD kPsTone[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_MUL, D_R(0), S_R(1), S_R(1),                 // r0 = L^2
    I_MUL, D_R(1) | D_X2, S_R(1), S_C(1),          // r1 = L tint
    I_MAD, D_R(1), S_R(0), S_C(2), S_R(1),         // r1 += L^2 warm
    I_LRP, D_R(0), S_C(3), S_R(1), S_T(0),         // r0 = lrp(mix, r1, picture)
    I_SUB, D_R(0), S_R(0), S_C(4),                 // - 0.5
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(5),          // x con
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(6),         // + 0.5 + bri
    PS_END };
// CRT. t0 / t1 / t2 = the frame at -chroma / 0 / +chroma px. c0..c2 channel masks, c3 luma, c4 -sat, c5 0.5,
// c6 con / 2, c7 0.5 + bri
static const DWORD kPsCrt[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2),
    I_MUL, D_R(0), S_T(0), S_C(0),                 // r0 = red of the left sample
    I_MAD, D_R(0), S_T(1), S_C(1), S_R(0),         // + green of the centre
    I_MAD, D_R(0), S_T(2), S_C(2), S_R(0),         // + blue of the right
    I_DP3, D_R(1), S_R(0), S_C(3),                 // r1 = L
    I_LRP, D_R(0), S_C(4), S_R(1), S_R(0),         // saturation (negative mix)
    I_SUB, D_R(0), S_R(0), S_C(5),
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(6),
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(7),
    PS_END };
// VIVID. t0 centre, t1..t3 a triangle of radius px. c0 0.5, c1 1/3, c2 sharpen, c3 luma, c4 -sat, c5 con / 2,
// c6 0.5 - 0.5 con + bri
static const DWORD kPsVivid[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(1), S_C(0), S_T(1), S_T(2),         // r1 = (t1 + t2) / 2
    I_LRP, D_R(1), S_C(1), S_T(3), S_R(1),         // r1 = (t1 + t2 + t3) / 3: the blur
    I_SUB, D_R(1), S_T(0), S_R(1),                 // detail = centre - blur
    I_MAD, D_R(0), S_R(1), S_C(2), S_T(0),         // centre + sharpen x detail
    I_DP3, D_R(1), S_R(0), S_C(3),                 // L
    I_LRP, D_R(0), S_C(4), S_R(1), S_R(0),         // saturation
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(5),          // x con
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(6),         // + 0.5 - 0.5 con + bri
    PS_END };
// NOIR. c0 luma x gain, c1 lift, c2 -curve, c3 curve. x' = x + lift x (1 - x); y = x' + curve d - 4 curve d^3, d = x' - 0.5
static const DWORD kPsNoir[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = x
    I_MUL, D_R(0), S_R(1), S_R(1) | S_INV,         // r0 = x (1 - x)
    I_MAD, D_R(1), S_R(0), S_C(1), S_R(1),         // r1 = x' (shadow lift)
    I_MUL, D_R(0), S_R(1) | S_BIAS, S_R(1) | S_BIAS,  // d^2
    I_MUL, D_R(0), S_R(0), S_R(1) | S_BIAS,        // d^3
    I_MUL, D_R(0) | D_X4, S_R(0), S_C(2),          // -4 curve d^3
    I_MAD, D_R(0), S_R(1) | S_BIAS, S_C(3), S_R(0),  // + curve d
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(1),         // + x'
    PS_END };
// NIGHT VISION. t0 centre, t1..t3 a triangle of radius px. c0 0.5, c1 1/3, c2 luma, c3 bloom threshold, c4 bloom,
// c5 gain x tint / 4, c6 phosphor floor
static const DWORD kPsNv[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(1), S_C(0), S_T(1), S_T(2),
    I_LRP, D_R(1), S_C(1), S_T(3), S_R(1),         // blur
    I_DP3, D_R(1), S_R(1), S_C(2),                 // its luma
    I_DP3, D_R(0), S_T(0), S_C(2),                 // the centre's luma
    I_SUB, D_R(1) | D_SAT, S_R(1), S_C(3),         // highlights only
    I_MAD, D_R(0), S_R(1), S_C(4), S_R(0),         // + bloom
    I_MUL, D_R(0) | D_X4, S_R(0), S_C(5),          // x gain x tint
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(6),         // + floor
    PS_END };
// THERMAL. c0 luma x gain, c1 (R0, G0, B0, 0), c2 (RS, GS, BS, RISE) / 4, c3 FALL_END, c4 FALL_S / 4, c5 hump colour,
// c6 floor
// THERMAL (live findings 2026-11-15: an `_x4` / r0.a version drew black and a texreg2gb palette lookup drew nothing
// through GOG's wrapper). Only forms proven live here: dp3, sub, mul_x2, add_sat, mad, one constant each.
// L = luma x gain; heat ramps R/G/B = sat((L - start) x slope) (slope = 4 x c2: mul_x2 then add r, r);
// cold = sat((COLD_END - L) x COLD_S) x cold colour (deep blue / purple where the scene is dark).
// c0 luma x gain, c1 ramp starts, c2 slopes / 4, c3 COLD_END, c4 COLD_S / 4, c5 cold colour
static const DWORD kPsThermal[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_SUB, D_R(0), S_R(1), S_C(1),                 // L - starts
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(2),          // x slope / 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2, saturated: the heat ramps
    I_SUB, D_R(1), S_C(3), S_R(1),                 // COLD_END - L
    I_MUL, D_R(1) | D_X2, S_R(1), S_C(4),          // x COLD_S / 2
    I_ADD, D_R(1) | D_SAT, S_R(1), S_R(1),         // x 2, saturated: cold factor
    I_MAD, D_R(0), S_R(1), S_C(5), S_R(0),         // + cold x cold colour
    PS_END };
// GHOST VISION (RE_ghostvision 1.2 / 3). Only the forms proven live through GOG's wrapper (tex, dp3, sub, mul / mul_x2,
// add / add_sat, mad, lrp; one constant each, at most 8 arithmetic; no _x4, no dependent read, no .a replicate).
// L: lift the darks. c0 = (LIFT, LIFT, LIFT, 0). y = x + 2 LIFT (x - x^2)
static const DWORD kPsGvLift[] = { PS_VER, PS_TEX(0),
    I_MUL, D_R(1), S_T(0), S_T(0),                 // r1 = x^2
    I_SUB, D_R(1), S_T(0), S_R(1),                 // r1 = x - x^2
    I_MAD, D_R(0), S_R(1), S_C(0), S_T(0),         // x + LIFT (x - x^2)
    I_MAD, D_R(0), S_R(1), S_C(0), S_R(0),         // + LIFT (x - x^2) again
    PS_END };
// E: the gradient. t0 / t1 = -d / +d in x, t2 / t3 = -d / +d in y; c0 = (1, 1, 1, 0), c1 = 1. out = sat(8 sum (dX^2 + dY^2))
static const DWORD kPsGvEdge[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_SUB, D_R(0), S_T(1), S_T(0),                 // dX
    I_SUB, D_R(1), S_T(3), S_T(2),                 // dY
    I_MUL, D_R(0), S_R(0), S_R(0),                 // dX^2
    I_MAD, D_R(0), S_R(1), S_R(1), S_R(0),         // + dY^2 (per channel)
    I_DP3, D_R(0), S_R(0), S_C(0),                 // summed over the channels
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(1),          // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    PS_END };
// G: the grade. t0 centre, t1..t3 a triangle of radius GRADE_PX; c0 0.5, c1 1/3, c2 MIX, c3 -THRESH, c4 1, c5 line colour.
// out = colour x sat(8 (lrp(MIX, blur, sharp) - THRESH))
static const DWORD kPsGvGrade[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(1), S_C(0), S_T(1), S_T(2),
    I_LRP, D_R(1), S_C(1), S_T(3), S_R(1),         // blur
    I_LRP, D_R(0), S_C(2), S_R(1), S_T(0),         // soft halo + sharp line
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(3),         // - THRESH (negatives -> 0)
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(4),          // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    I_MUL, D_R(0), S_R(0), S_C(5),                 // x line colour (black elsewhere)
    PS_END };
// GLOW (the ghost redraw, behind the game's model vertex shader). c0 = GLOW x TEX / 2 (a 0.5: alpha = t0.a for the
// game's alpha test), c1 = GLOW x FLOOR (a 0). out = sat(2 t0 c0 + c1)
static const DWORD kPsGvGlow[] = { PS_VER, PS_TEX(0),
    I_MUL, D_R(0) | D_X2, S_T(0), S_C(0),
    I_ADD, D_R(0) | D_SAT, S_R(0), S_C(1),
    PS_END };

// v3.18.0 NOISE COLOUR (N1): d = n - 0.5; out = 0.5 + s (d + max(d, 0) (colour - 1)): the darkening stays neutral, the
// brightening takes the colour (live: tinting both made amber drops' dark rims look blue). c0 colour - 1, c1 0.5,
// c2 0, c3 strength; one constant per instruction (proven forms), then the overlay's 2x-modulate blend as before
static const DWORD kPsNCol[] = { PS_VER, PS_TEX(0),
    I_SUB, D_R(1), S_T(0), S_C(1),                 // r1 = d
    I_ADD, D_R(0) | D_SAT, S_R(1), S_C(2),         // r0 = max(d, 0)
    I_MAD, D_R(1), S_R(0), S_C(0), S_R(1),         // r1 = d + max(d, 0) (colour - 1)
    I_MUL, D_R(1), S_R(1), S_C(3),                 // x strength
    I_ADD, D_R(0), S_R(1), S_C(1),                 // + 0.5
    PS_END };
// v3.18.0 HILITE (N6, the night visions): out = sat(2 (L - T) c2 x c3), added (ONE / ONE) over the filtered frame:
// light above T flares, darks untouched. c0 luma weights, c1 T, c2 K / 2, c3 the tint
static const DWORD kPsHilite[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_SUB, D_R(1), S_R(1), S_C(1),                 // - T
    I_MUL, D_R(1) | D_X2, S_R(1), S_C(2),          // x K
    I_MUL, D_R(0) | D_SAT, S_R(1), S_C(3),         // x tint, clamped (negatives -> 0)
    PS_END };
// v3.22.0 HEAT (Thermal's body redraw, behind the game's model vertex shader; PLAN section 2). Proven forms only, one
// constant each, 7 arithmetic. c0 luma x DETAIL, c1 BODY x heat, c2 ramp starts, c3 slopes / 4, c4 (1, 1, 1, 0),
// c5 (0, 0, 0, heat). out.rgb = the heat ramp of V; out.a = t0.a x heat (the game's alpha test keeps cut-out hair; a
// cooling body fades into the world under SRCALPHA / INVSRCALPHA)
static const DWORD kPsHeat[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = luma x DETAIL
    I_ADD, D_R(1), S_R(1), S_C(1),                 // V = + BODY x heat
    I_SUB, D_R(0), S_R(1), S_C(2),                 // V - starts
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(3),          // x slope / 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2, saturated: the ramps
    I_MUL, D_R(0), S_R(0), S_C(4),                 // alpha 0
    I_MAD, D_R(0), S_T(0), S_C(5), S_R(0),         // alpha = t0.a x heat
    PS_END };
// v3.26.0 BLOOD (Spirit Vision's ghost redraw, "blood-rooted", PLAN 2.1): a ramp on the texel's luma from a deep
// crimson (dark texels, untextured parts) to a pale rose white (bright texels). Proven forms only, one constant each.
// c0 luma weights x GV_BLOOD_GAIN x pulse, c1 (pale - deep) / 2, c2 deep / 2 (c1.a = c2.a = 0), c3 (1, 1, 1, 0),
// c4 (0, 0, 0, 1). out.rgb = sat(2 (L (pale - deep) / 2 + deep / 2)) = the ramp; out.a = t0.a (the game's alpha test)
static const DWORD kPsGvBlood[] = { PS_VER, PS_TEX(0),
    I_DP3, D_R(1), S_T(0), S_C(0),                 // r1 = L
    I_MUL, D_R(0), S_R(1), S_C(1),                 // L x (pale - deep) / 2
    I_ADD, D_R(0), S_R(0), S_C(2),                 // + deep / 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2, saturated
    I_MUL, D_R(0), S_R(0), S_C(3),                 // alpha 0
    I_MAD, D_R(0), S_T(0), S_C(4), S_R(0),         // alpha = t0.a
    PS_END };
// v3.43 PP343: post.inc's shaders (proven forms only: tex, lrp, sub, mul / mul_x2, add / add_sat, mad, dp3; one constant
// each; at most 8 arithmetic). BRIGHT: the mean of 4 bilinear taps (16 texels), sat((m - th) gain): c0 0.5, c1 th,
// c2 gain / 4. MEAN: the mean of 4 taps (down / Kawase): c0 0.5. UP: the mean, alpha = up (blended SRCALPHA over the
// level's own content): c0 0.5, c1 (1, 1, 1, 0), c2 (0, 0, 0, up). COMP: the mean x k (screen-blended): c0 0.5, c1 k / 2 (v3.52 FX352).
static const DWORD kPsPpBright[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),         // the mean
    I_SUB, D_R(0), S_R(0), S_C(1),                 // - th
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(2),          // x gain / 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2, saturated
    PS_END };
static const DWORD kPsPpMean[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),
    PS_END };
static const DWORD kPsPpUp[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),
    I_MUL, D_R(0), S_R(0), S_C(1),                 // alpha 0
    I_ADD, D_R(0), S_R(0), S_C(2),                 // alpha = up
    PS_END };
static const DWORD kPsPpComp[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),
    I_MUL, D_R(0) | D_X2, S_R(0), S_C(1),          // x k (v3.52 FX352: c1 = k / 2, _x2: k up to 2; mul_x2 as BRIGHT's)
    PS_END };
// GRADE: v3.44 CS344: post_sim.h's kPpGradePs (proven forms only: v3.43's `mad r0, 1-r1, c4, r0` was dropped silently by
// GOG's translator, the frame came through unchanged); post_sim.h is pure (included again by post.inc: guarded)
#include "post_sim.h"  // v3.44 CS344
#include "preview_sim.h"  // v3.56 PV356: the Effects preview's rules (PVB_* for FxBackdropC; preview.inc draws)
#define kPsPpGrade kPpGradePs  // v3.44 CS344
// LENS: t0 / t1 / t2 = the frame at the red / green / blue warped coordinates; c0..c2 the channel masks
static const DWORD kPsPpLens[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2),
    I_MUL, D_R(0), S_T(0), S_C(0),
    I_MAD, D_R(0), S_T(1), S_C(1), S_R(0),
    I_MAD, D_R(0), S_T(2), S_C(2), S_R(0),
    PS_END };
static const DWORD *const kFxPsCode[FXT_N] = { NULL, kPsTone, kPsCrt, kPsVivid, kPsNoir, kPsNv, kPsThermal, kPsGvLift,
                                               kPsGvEdge, kPsGvGrade, kPsGvGlow, kPsNCol, kPsHilite, kPsHeat, kPsGvBlood,
                                               kPsPpBright, kPsPpMean, kPsPpUp, kPsPpComp, kPsPpGrade, kPsPpLens };  // v3.43 PP343: + PP
static const char *const kFxPsName[FXT_N] = { "", "tone", "CRT", "vivid", "noir", "night vision", "thermal", "gv lift",
                                              "gv edge", "gv grade", "gv glow", "noise colour", "highlight", "heat", "gv blood",
                                              "bloom bright", "bloom mean", "bloom up", "bloom composite", "grading", "lens" };  // v3.43 PP343
static const int kFxPsTex[FXT_N] = { 0, 1, 3, 4, 1, 4, 1, 1, 4, 4, 1, 1, 1, 1, 1, 4, 4, 4, 4, 1, 3 };  // frame samples t0.. (Thermal: t1 = the heat palette); v3.43 PP343: + PP

typedef HRESULT(STDMETHODCALLTYPE *FxCreatePs_t)(void *, const DWORD *, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *FxPsc_t)(void *, DWORD, void *, DWORD);
enum { FXV_SETVP = 40, FXV_GETVP = 41, FXV_SETVSC = 79, FXV_GETVSC = 80, FXV_CREATEPS = 87, FXV_SETPSC = 91, FXV_GETPSC = 92 };

static void *g_fxDev;                          // the device our shaders / textures belong to
static DWORD g_fxPs[FXT_N];                    // our pixel shaders (0 = not made)
static BYTE g_fxPsFailed[FXT_N];
static void *g_fxTex[FXX_N];                   // our managed textures
static BYTE g_fxTexFailed[FXX_N];
static int g_fxLastF = -1, g_fxLastWhere = -1; // the last filter / place the pass logged
static int g_thermHooks;                       // the Thermal draw hooks are in place
static volatile LONG g_thermHot;               // set while 0x52AEB5 draws an enemy (the ambient hook reads it)
static float g_thermAmb = THERM_HOT_AMBIENT;
static BYTE g_thermGhost[0x13], g_thermCold[0x13];  // per enemy type (1..0x12): hidden / no heat
static const DWORD g_fnModel = 0x55C650, g_fnShadow = 0x55C860, g_fnJasper = 0x47DEC0;
// Ghost Vision: the ghosts collected at 0x52AEB5 this frame (stamp = g_fxFrame) and the 3D viewport then in force
#define GV_MAX 32
enum { GVK_ACTOR, GVK_WALL, GVK_ROOM302 };
typedef struct { const BYTE *task, *rec; BYTE *model, *recModel; DWORD fn; short type; BYTE kind, pad; } GvEnt;
static GvEnt g_gv[GV_MAX];
static int g_gvN, g_gvVpOk;
static LONG g_gvStamp = -1;
static Viewport8 g_gvVp;
static int g_gvDipHooks;                       // the draw-call override group (0x4292EF / 0x429338) is in place
static volatile LONG g_gvGlow;                 // set only while our ghost redraw runs: the override acts
static DWORD g_gvGlowPs;                       // the glow shader handle for the override
static float g_gvGlowC[5][4];                  // its constants c0..c1 (v3.26.0 blood ramp: c0..c4)
static int g_gvGlowCN = 2;                     // v3.26.0: how many of them the bound shader reads
static int g_gvBloodOn = 1;                    // v3.26.0 [ScreenEffects] SpiritGhost: 1 Blood (default), 0 White
static int g_gvFaults;                         // faults in a redraw (3: the model glow is off for the session)
static void (__cdecl *volatile g_fxDipOvr)(void *dev);  // v3.30 ECHO: set only while echo.inc's redraw runs: its own override
// v3.22.0 Thermal v2: the hot bodies collected this frame (0x52AEB5, the trapped Cynthia at 0x4EE376), redrawn in heat
// colours after the world palette (ThHeatPass) through the same draw-call override in heat mode (g_thMode)
typedef struct { const BYTE *task, *rec; BYTE *model, *recModel; DWORD fn; float heat; } ThEnt;
static ThEnt g_th[TH_MAX];
static int g_thN, g_thFaults, g_thVpOk;
static LONG g_thStamp = -1;
static Viewport8 g_thVp;
static volatile LONG g_thMode;                 // set only while the heat redraw runs: the override acts in heat mode
static volatile LONG g_thGtPass;               // v3.58 GT358: 1 = Ghost Teleport's faded heat body's depth pre-pass (teleport.inc)
static float GtHeatOp(const BYTE *model);      // v3.58 GT358: teleport.inc: her opacity in the fade (1 = not fading)
static void GtHeatFade(BYTE *m, float op);     // v3.58 GT358: teleport.inc: her faded heat body (pre-pass + blend)
static float GtSoulOp(void);                   // v3.58 GT358: teleport.inc: her soul fire's alpha factor (spiritfire.inc)
static int GtShadowC(const BYTE *task);        // v3.58 GT358: teleport.inc: 0 = her shadow not drawn (below half opacity)
static DWORD g_thPs;                           // the heat shader handle for the override
static float g_thC[6][4];                      // its constants c0..c5 (c1 / c5 per body)
static int g_thermHeatOn = 1;                  // [ScreenEffects] ThermalHeat (0 = the v3.21 look)
static BYTE g_svNoSoul[0x13];                  // v3.22.0: enemy types without a soul fire ([ScreenEffects] SoulNone)
static int ThHeatOk(void)                      // the heat redraw can run (else the v3.21 ambient look)
{
    return g_thermHeatOn && g_gvDipHooks && g_thFaults < 3 && !g_fxPsFailed[FXT_HEAT];
}
static __declspec(align(16)) float g_gvM[16];  // world -> GS matrix (0x55BF50, as Item Shine)
// v3.17.0 S8: the non-ghost enemies drawn at 0x52AEB5 this frame (same stamp as g_gv): their soul flames
typedef struct { const BYTE *task; BYTE *model; short type; float heat, age; } SvSoul;  // v3.21.0: heat (death fade); v3.28 FIRE: age (s since death, < 0 alive)
static SvSoul g_svSoul[SV_SOULS];
static int g_svSoulN;

// ---------------------------------------------------------------- small helpers
static DWORD FxHash(DWORD a)
{
    a ^= a >> 16;
    a *= 0x7FEB352Du;
    a ^= a >> 15;
    a *= 0x846CA68Bu;
    a ^= a >> 16;
    return a;
}
static float FxRnd(DWORD a) { return (float)(FxHash(a) & 0xFFFFFF) / 16777216.0f; }  // 0..1
static float FxSat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float FxSmooth(float e0, float e1, float x)
{
    const float t = FxSat((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
static float FxValueNoise(float x, float y, DWORD seed)  // smooth 0..1 noise on a unit grid
{
    const int ix = (int)floorf(x), iy = (int)floorf(y);
    const float fx = x - (float)ix, fy = y - (float)iy;
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    const DWORD x0 = (DWORD)ix * 73856093u, x1 = (DWORD)(ix + 1) * 73856093u;
    const DWORD y0 = (DWORD)iy * 19349663u, y1 = (DWORD)(iy + 1) * 19349663u;
    const float a = FxRnd(seed ^ x0 ^ y0), b = FxRnd(seed ^ x1 ^ y0), c = FxRnd(seed ^ x0 ^ y1), d = FxRnd(seed ^ x1 ^ y1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}
// The animation clock: 30 ticks a second whatever the frame rate (artifacts live 1-6 ticks; FrameMode 1 replays the
// game-thread draws, so the layers change per game frame there as well).
static LARGE_INTEGER g_fxQf, g_fxQb;
static LONGLONG FxClock(void)  // QPC counts since the first call
{
    LARGE_INTEGER n;
    if (!g_fxQf.QuadPart) {
        QueryPerformanceFrequency(&g_fxQf);
        QueryPerformanceCounter(&g_fxQb);
        if (!g_fxQf.QuadPart) g_fxQf.QuadPart = 1;
    }
    QueryPerformanceCounter(&n);
    return n.QuadPart - g_fxQb.QuadPart;
}
static DWORD FxTick(void) { return (DWORD)(FxClock() * 30 / g_fxQf.QuadPart); }
static float FxPulse(void)  // Ghost Vision: 1 - PULSE .. 1, one period every GV_PULSE_S seconds (smooth at any frame rate)
{
    const LONGLONG c = FxClock(), per = (LONGLONG)(GV_PULSE_S * (double)g_fxQf.QuadPart);
    const float ph = per > 0 ? (float)(c % per) / (float)per : 0.0f;
    return 1.0f - GV_PULSE * (0.5f - 0.5f * cosf(6.2831853f * ph));
}
static float FxW(void) { return (float)*(volatile int *)0x5D524C; }
static float FxH(void) { return (float)*(volatile int *)0x5D5250; }

// ---------------------------------------------------------------- Noise Strength / Noise Type
static int FxNoiseLayer(int level, int preview);  // v3.26.0: preview 1 = the Screen Effects page (its own rain state)
static void FxRainDraw(void *dev, float s0, float w, float h, DWORD tick, int preview);
static int GvNoiseOff(void);
static LONG g_fxNoiseFrame = -1;
static int FxCutOff(void);
static int __cdecl NoiseDraw(int type, int level)
{
    static const int kHalves[4] = { 1, 2, 3, 4 };  // x0.5, x1, x1.5, x2
    const int s = g_fxOn ? (int)g_fx[FX_NOISE] : 1;
    int l = level;
    if (GvNoiseOff()) return 0;  // G2: Ghost Vision draws this frame: no grain / dust (the edge pass would light it up)
    if (s != 1 && s >= 0 && s < 4 && level > 0) {
        l = level * kHalves[s] / 2;
        if (l < 1) l = 1;
        if (l > 255) l = 255;  // 567040 treats anything above 255 as 255
    }
    if (type == 1 && l > 0) g_fxNoiseFrame = g_fxFrame;  // v3.18.0 N3: grain drawn this frame (no doubled preview)
    if (type == 1 && g_fxOn && (g_fx[FX_NTYPE] > 0 || g_fx[FX_NCOLOUR] > 0) && !FxCutOff() && FxNoiseLayer(l, 0))
        return 0;  // E5 / v3.18.0 N1; v3.22.0 G5 (review V322-001): Cut-scenes Off -> the game's grain in cut-scenes
    return ((int(__cdecl *)(int, int))0x567040)(type, l);
}

// ---------------------------------------------------------------- resources (game thread, lazily, per device)
static void FxTexSize(int id, int *w, int *h)
{
    static const BYTE kW[FXX_N] = { 8, 8, 8, 3, 8, 8, 8, 6, 7, 8, 6, 9, 3, 8, 3, 8, 6, 7, 8, 10 }, kH[FXX_N] = { 8, 8, 8, 3, 2, 8, 8, 8, 7, 8, 6, 9, 6, 8, 6, 8, 3, 7, 7, 9 };  // v3.28 FIRE: FLAME 512 x 512 (the spirit fire atlas)  // v3.27.0: FOG 128 x 128, WISP 256 x 128; v3.27.1: SMOKE 1024 x 512  // log2 (v3.26.0: DROP 256 x 256 atlas, TRAIL 64 x 8)
    *w = 1 << kW[id];
    *h = 1 << kH[id];
}
static BYTE FxByte(float f) { return (BYTE)(FxSat(f * 0.5f) * 255.0f + 0.5f); }  // a 2x-modulate factor 0..2
static DWORD FxGrey(float f) { const DWORD c = FxByte(f); return 0xFF000000u | c << 16 | c << 8 | c; }
static DWORD FxRgb(float r, float g, float b) { return 0xFF000000u | (DWORD)FxByte(r) << 16 | (DWORD)FxByte(g) << 8 | FxByte(b); }
static float FxGauss(DWORD a)  // about -0.5..0.5, sigma 0.17
{
    return (FxRnd(a) + FxRnd(a ^ 0x5bd1e995u) + FxRnd(a ^ 0x68e31da4u)) / 3.0f - 0.5f;
}
// v3.21.0 FXX_SVATLAS (feedback_v321 PLAN 3): 256 x 256, 4 x 4 cells of 64, premultiplied grey (RGB = A; the colour
// comes from the vertex diffuse). Cells: 0 soft radial, 1-4 lightning veins, 5-6 mist puffs, 7-8 ash flakes, 9 halo
// annulus, 10 orb, 11 blood blob, 12 spark, 13 thin ring (v3.22.0), 14-15 spare (soft radial). Every cell is 0 at its border (no bleeding).
static float SvRidge(float x, float y, DWORD seed)  // ridged noise (1 - |2n - 1|)^8: thin branching lines
{
    const float n = 1.0f - fabsf(2.0f * FxValueNoise(x, y, seed) - 1.0f), n2 = n * n, n4 = n2 * n2;
    return n4 * n4;
}
static float SvFbm(float x, float y, DWORD seed, int oct)  // 0..1
{
    float s = 0.0f, t = 0.0f, a = 0.5f;
    int o;
    for (o = 0; o < oct; ++o) {
        s += a * FxValueNoise(x, y, seed + (DWORD)o * 101u);
        t += a;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 9.7f;
        a *= 0.5f;
    }
    return s / t;
}
static DWORD SvAtlasTexel(int x, int y)
{
    const int cell = (x >> 6) + 4 * (y >> 6);
    const float cu = ((x & 63) + 0.5f) / 64.0f, cv = ((y & 63) + 0.5f) / 64.0f, du = cu - 0.5f, dv = cv - 0.5f;
    const float cr = sqrtf(du * du + dv * dv), edge = 1.0f - FxSmooth(0.38f, 0.5f, cr);  // 0 from r 0.5 (the border) out
    float a;
    DWORD c;
    switch (cell) {
    case 1: case 2: case 3: case 4: {  // veins: domain-warped ridged multi-octave noise x a radial fall-off
        const DWORD sd = 0x7E1Au + (DWORD)cell * 977u;
        const float wx = cu * 3.0f + 0.9f * (FxValueNoise(cu * 2.5f, cv * 2.5f, sd ^ 0x55u) - 0.5f);
        const float wy = cv * 3.0f + 0.9f * (FxValueNoise(cu * 2.5f + 5.3f, cv * 2.5f, sd ^ 0x66u) - 0.5f);
        const float n1 = FxValueNoise(wx, wy, sd), r1 = SvRidge(wx, wy, sd);
        const float r2 = SvRidge(wx * 2.1f + 3.1f, wy * 2.1f, sd + 1u), r3 = SvRidge(wx * 4.3f, wy * 4.3f + 1.7f, sd + 2u);
        const float g = 1.0f - fabsf(2.0f * n1 - 1.0f), fall = 1.0f - FxSmooth(0.04f, 0.46f, cr);
        a = (0.95f * r1 + 0.55f * r2 * (0.4f + 0.6f * g) + 0.30f * r3 * g) * fall + 0.16f * g * g * g * fall * fall;
        break;
    }
    case 5: case 6: {  // mist puffs: fBm x a radial fall-off with a ragged edge
        const float f = SvFbm(cu * 3.5f, cv * 3.5f, 0x3157u + (DWORD)cell * 131u, 4);
        const float lo = SvFbm(cu * 1.6f + 4.0f, cv * 1.6f, 0x3158u + (DWORD)cell * 131u, 2);  // a lumpy outline
        const float rr = cr * (1.0f + 1.2f * (lo - 0.5f) + 0.4f * (f - 0.5f));
        a = 0.95f * FxSat((f - 0.25f) * 2.0f) * (1.0f - FxSmooth(0.05f, 0.44f, rr));
        break;
    }
    case 7: case 8: {  // ash flakes: a torn, slightly curled flake
        const float ax = cell == 7 ? 1.0f : 1.3f, ay = cell == 7 ? 1.9f : 1.45f;
        const float n = FxValueNoise(cu * 9.0f, cv * 9.0f, 0xA5A5u + (DWORD)cell), n2 = FxValueNoise(cu * 5.0f, cv * 5.0f, 0xA6A6u);
        const float bend = cell == 8 ? 0.18f * du * du * 8.0f : 0.0f;
        const float rr = sqrtf(du * ax * du * ax + (dv - bend) * ay * (dv - bend) * ay) * (1.0f + 0.55f * (n - 0.5f));
        a = (1.0f - FxSmooth(0.24f, 0.34f, rr)) * (0.55f + 0.45f * n2);
        break;
    }
    case 9:  // halo annulus: a soft ring with a faint fill
        a = expf(-((cr - 0.33f) / 0.085f) * ((cr - 0.33f) / 0.085f)) + 0.12f * (1.0f - FxSmooth(0.0f, 0.4f, cr));
        break;
    case 10:  // orb: a hot core and a wide halo
        a = expf(-(cr / 0.05f) * (cr / 0.05f)) + 0.45f * expf(-(cr / 0.15f) * (cr / 0.15f)) + 0.15f * expf(-(cr / 0.30f) * (cr / 0.30f));
        break;
    case 11: {  // blood blob: a lumpy body, swirling inside, a lighter rim
        const float f = SvFbm(cu * 4.0f, cv * 4.0f, 0xB10Bu, 3), sw = SvFbm(cu * 7.0f + 3.0f, cv * 7.0f, 0xB10Cu, 3);
        const float rr = cr * (1.0f + 0.7f * (f - 0.5f));
        a = (1.0f - FxSmooth(0.20f, 0.38f, rr)) * (0.55f + 0.45f * sw) + 0.30f * expf(-((rr - 0.29f) / 0.05f) * ((rr - 0.29f) / 0.05f));
        break;
    }
    case 13:  // v3.22.0 thin ring (the candles' fast pulse): a narrow line at r 0.40 with a faint inner glow
        a = expf(-((cr - 0.40f) / 0.022f) * ((cr - 0.40f) / 0.022f)) + 0.06f * (1.0f - FxSmooth(0.0f, 0.4f, cr));
        break;
    case 12:  // spark: a four-point glint
        a = expf(-(cr / 0.04f) * (cr / 0.04f)) + 0.2f * expf(-(cr / 0.14f) * (cr / 0.14f)) +
            0.6f * expf(-fabsf(du) / 0.012f) * expf(-fabsf(dv) / 0.15f) + 0.6f * expf(-fabsf(dv) / 0.012f) * expf(-fabsf(du) / 0.15f);
        break;
    default: {  // 0, 13-15: soft radial
        const float s = 1.0f - FxSmooth(0.0f, 0.5f, cr);
        a = s * s;
        break;
    }
    }
    c = (DWORD)(FxSat(a * edge) * 255.0f + 0.5f);
    return c << 24 | c << 16 | c << 8 | c;
}
// v3.26.0 rain-on-glass drop atlas (PLAN 3.1 / 3.3): cell c (2 x 2 of 128 texels); in the cell, (a, b) in -1..1 with
// the drop's radius 0.86 (a zero border: no bleeding). The outline wobbles with three low harmonics per cell, the top
// is a little flatter and the lower half heavier. Alpha: the shape with a 1.5-texel soft edge. Grey (2x modulate,
// 0x80 = no change; outside the drop exactly neutral): a dark inner rim (stronger lower-left), a sharp glint
// upper-left, a soft bright caustic crescent in the lower third.
static DWORD FxDropTexel(int x, int y)
{
    const int cell = (x >= 128) + 2 * (y >= 128);
    const float a = ((float)(x & 127) + 0.5f) / 64.0f - 1.0f, b = ((float)(y & 127) + 0.5f) / 64.0f - 1.0f;
    const DWORD hc = FxHash((DWORD)cell * 0x9E3779B9u + 0xD20Bu);
    const float bb = b < 0.0f ? b * 1.12f : b * 0.96f, ba = bb - 0.04f;  // flatter top, heavier bottom
    const float th = atan2f(ba, a), rr = sqrtf(a * a + ba * ba);
    const float edge = 0.86f * (1.0f + 0.05f * sinf(2.0f * th + 6.2831853f * FxRnd(hc)) + 0.035f * sinf(3.0f * th + 6.2831853f * FxRnd(hc ^ 1u)) +
                                0.02f * sinf(5.0f * th + 6.2831853f * FxRnd(hc ^ 2u)));
    const float q = rr / edge, al = 1.0f - FxSmooth(1.0f - 1.5f / 55.0f, 1.0f, q);
    float g = 1.0f;
    if (q < 1.0f) {
        const float rim = expf(-((q - 0.86f) / 0.10f) * ((q - 0.86f) / 0.10f)) * (0.75f + 0.25f * FxSat(0.5f - 0.5f * a + 0.5f * b));
        const float gx = a + 0.34f, gy = b + 0.40f, glint = expf(-(gx * gx + gy * gy) / (0.13f * 0.13f));
        const float glint2 = expf(-((a - 0.22f) * (a - 0.22f) + (b + 0.30f) * (b + 0.30f)) / (0.06f * 0.06f));
        const float caustic = FxSmooth(0.15f, 0.55f, b) * FxSmooth(0.95f, 0.55f, q) * (1.0f - FxSmooth(0.5f, 0.9f, fabsf(a)));
        g = 1.0f - 0.62f * rim + 0.95f * glint + 0.35f * glint2 + 0.30f * caustic;
        g = 1.0f + (g - 1.0f) * al;  // the soft edge blends back to neutral
    }
    return (DWORD)(FxSat(al) * 255.0f + 0.5f) << 24 | (FxGrey(g) & 0xFFFFFFu);
}
static DWORD FxTexel(int id, int x, int y, int w, int h)
{
    const float u = (x + 0.5f) / (w * 0.5f) - 1.0f, v = (y + 0.5f) / (h * 0.5f) - 1.0f, r = sqrtf(u * u + v * v);
    const DWORD at = (DWORD)(y * 4099 + x * 7 + 1);
    switch (id) {
    case FXX_VIG:  // radial darkening toward the corners (v3.15.0's profile)
        return FxGrey(1.0f - FxSmooth(0.45f, 1.41421356f, r));
    case FXX_BURN: {  // old film: an irregular burnt edge, dark brown toward the corners
        const float n = FxValueNoise(x / 22.0f, y / 22.0f, 0xB0B0u) * 0.65f + FxValueNoise(x / 7.0f, y / 7.0f, 0xC1C1u) * 0.35f;
        const float t = FxSmooth(0.62f, 1.30f, r + 0.20f * (n - 0.5f));
        return FxRgb(1.0f - t * (1.0f - 0.38f), 1.0f - t * (1.0f - 0.26f), 1.0f - t * (1.0f - 0.16f));
    }
    case FXX_GOGGLE:  // night vision: a round goggle edge, black corners
        return FxGrey(1.0f - FxSmooth(0.78f, 1.02f, r));
    case FXX_SCAN:  // one band per texture height, mean 1 (bright line, dark gap)
        return FxGrey(1.0f + cosf(6.2831853f * (y + 0.5f) / (float)h));
    case FXX_MASK: {  // RGB aperture grille, period 3 px (256 wide: one seam per 256 px), mean 1 per channel
        const int c = x % 3;
        return FxRgb(c == 0 ? 2.0f : 0.5f, c == 1 ? 2.0f : 0.5f, c == 2 ? 2.0f : 0.5f);
    }
    case FXX_NOISE:
        return FxGrey(1.0f + 2.6f * FxGauss(at));
    case FXX_CNOISE:
        return FxRgb(1.0f + 2.6f * FxGauss(at * 3), 1.0f + 2.6f * FxGauss(at * 3 + 1), 1.0f + 2.6f * FxGauss(at * 3 + 2));
    case FXX_VHS: {  // rows: mostly quiet, a few bright / dark streak lines, a little noise along each row
        const float row = FxRnd((DWORD)y * 7919u + 17u);
        float line = (row - 0.5f) * 0.25f;
        if (row > 0.92f) line = (row - 0.92f) / 0.08f * 0.9f;
        else if (row < 0.06f) line = -0.6f * (0.06f - row) / 0.06f;
        return FxGrey(1.0f + line + (FxRnd(at) - 0.5f) * 0.25f);
    }
    case FXX_HEAT: {  // Thermal palette, read by the frame colour (texreg2gb: u = green, v = blue). Plain colours (not 2x)
        static const float kStop[7][4] = { { 0.00f, 0.00f, 0.00f, 0.00f }, { 0.12f, 0.04f, 0.00f, 0.32f },
                                           { 0.30f, 0.38f, 0.02f, 0.62f }, { 0.48f, 0.82f, 0.08f, 0.30f },
                                           { 0.64f, 1.00f, 0.42f, 0.02f }, { 0.80f, 1.00f, 0.85f, 0.15f },
                                           { 1.00f, 1.00f, 1.00f, 1.00f } };
        float L = (0.84f * x / (w - 1.0f) + 0.16f * y / (h - 1.0f)) * THERM_GAIN, c[3];
        int s, k;
        if (L > 1.0f) L = 1.0f;
        for (s = 0; s < 5 && L > kStop[s + 1][0]; ++s) {}
        for (k = 0; k < 3; ++k) {
            const float a = (L - kStop[s][0]) / (kStop[s + 1][0] - kStop[s][0]);
            c[k] = kStop[s][k + 1] + (kStop[s + 1][k + 1] - kStop[s][k + 1]) * (a < 0 ? 0 : a > 1 ? 1 : a);
        }
        return 0xFF000000u | (DWORD)(c[0] * 255.0f + 0.5f) << 16 | (DWORD)(c[1] * 255.0f + 0.5f) << 8 | (DWORD)(c[2] * 255.0f + 0.5f);
    }
    case FXX_GLOW: {  // Ghost Vision sprite fallback: a soft round glow, premultiplied white (additive ONE / ONE)
        const float a = 1.0f - FxSmooth(0.0f, 1.0f, r), k = a * a;
        const DWORD c = (DWORD)(FxSat(k) * 255.0f + 0.5f);
        return c << 24 | c << 16 | c << 8 | c;
    }
    case FXX_ROLL: {  // v3.18.0 Scan Roll band (2x modulate grey): a brighter middle, darker rims
        const float d = (y + 0.5f) / (float)h - 0.5f;
        return FxGrey(1.0f + 0.45f * expf(-(d / 0.18f) * (d / 0.18f)) - 0.3f * expf(-((fabsf(d) - 0.40f) / 0.06f) * ((fabsf(d) - 0.40f) / 0.06f)));
    }
    case FXX_DROP:  // v3.26.0: 2 x 2 cells of 128, four drop shapes (alpha = the shape, grey = the 2x-modulate rim / glint)
        return FxDropTexel(x, y);
    case FXX_FOG:  // v3.27.0 Death Presence (deathpresence_draw.inc): A = the coverage curve, RGB = the crest curve
        return DpFogTexel(x, y);
    case FXX_WISP:  // v3.27.0: four 64 x 128 vertical strands (premultiplied grey)
        return DpWispTexel(x, y);
    case FXX_SMOKE:  // v3.27.1: the volume's smoke puffs and the ribbon strand (premultiplied grey)
        return DpSmokeTexel(x, y);
    case FXX_TRAIL: {  // v3.26.0: across a trail (u -1..1): alpha = a soft profile, grey = a thin light line, faint dark edges
        const float a = expf(-powf(fabsf(u) / 0.62f, 4.0f));
        return (DWORD)(FxSat(a) * 255.0f + 0.5f) << 24 | (FxGrey(1.0f + 0.55f * expf(-(u / 0.18f) * (u / 0.18f)) -
                                                                 0.18f * expf(-((fabsf(u) - 0.72f) / 0.12f) * ((fabsf(u) - 0.72f) / 0.12f))) & 0xFFFFFFu);
    }
    case FXX_STREAK: {  // v3.18.0 Rain streak: a thin light line, soft at both ends
        const float fy = (y + 0.5f) / (float)h;
        return FxGrey(1.0f + 0.55f * expf(-(u / 0.3f) * (u / 0.3f)) * FxSmooth(0.0f, 0.3f, fy) * FxSmooth(0.0f, 0.3f, 1.0f - fy));
    }
    case FXX_FLAME: {  // v3.28 FIRE: the spirit fire atlas (spiritfire_sim.h): 28 flipbook frames + ember / glow / spark /
        const DWORD c = (DWORD)(SfAtlasTexel(x, y) * 255.0f + 0.5f);  // coal / smoke cells, premultiplied grey
        return c << 24 | c << 16 | c << 8 | c;
    }
    case FXX_SVATLAS:
        return SvAtlasTexel(x, y);
    case FXX_ATLAS: {  // 2 x 2 cells of 64: 0 dust (dark), 1 hair (dark), 2 scratch (light, vertical), 3 white speck
        const int cell = (x >= w / 2) + 2 * (y >= h / 2);
        const float cu = (float)(x % (w / 2)) / (w / 2) , cv = (float)(y % (h / 2)) / (h / 2);
        const float du = cu - 0.5f, dv = cv - 0.5f, cr = sqrtf(du * du + dv * dv);
        float s = 0.0f, f = 1.0f;
        if (cell == 0 || cell == 3) {
            const float rr = cr * (1.0f + 0.45f * (FxValueNoise(cu * 6.0f, cv * 6.0f, 0xD5u + cell) - 0.5f));
            s = 1.0f - FxSmooth(0.22f, 0.42f, rr);
            f = cell == 0 ? 1.0f - 0.85f * s : 1.0f + 0.8f * s;
        } else if (cell == 1) {
            const float yc = 0.5f + 0.18f * sinf(3.2f * cu * 3.14159265f * 0.6f + 0.7f), d = fabsf(cv - yc);
            s = (1.0f - FxSmooth(0.006f, 0.028f, d)) * FxSmooth(0.04f, 0.16f, cu) * FxSmooth(0.04f, 0.16f, 1.0f - cu);
            f = 1.0f - 0.8f * s;
        } else {
            s = (1.0f - FxSmooth(0.03f, 0.12f, fabsf(du))) * (0.75f + 0.25f * FxValueNoise(0.5f, cv * 9.0f, 0xE7u));
            s *= FxSmooth(0.0f, 0.08f, cv) * FxSmooth(0.0f, 0.08f, 1.0f - cv);
            f = 1.0f + 0.7f * s;
        }
        return FxGrey(f);
    }
    }
    return FxGrey(1.0f);
}
static void *FxMakeTex(void *dev, int id)
{
    struct { int pitch; BYTE *bits; } lr;
    void *tex = NULL;
    int x, y, w, h;
    FxTexSize(id, &w, &h);
    if (FAILED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, w, h, 1, 0, 21 /*A8R8G8B8*/, 1 /*MANAGED*/, &tex)) || !tex)
        return NULL;
    if (FAILED(((SpLock_t)VT(tex, 16))(tex, 0, &lr, NULL, 0))) {
        ((Release_t)VT(tex, VT_RELEASE))(tex);
        return NULL;
    }
    for (y = 0; y < h; ++y) {
        DWORD *row = (DWORD *)(lr.bits + y * lr.pitch);
        for (x = 0; x < w; ++x) row[x] = FxTexel(id, x, y, w, h);
    }
    ((SpUnlock_t)VT(tex, 17))(tex, 0);
    return tex;
}
static void FxDeviceCheck(void *dev);  // v3.35 FX335 (below)
static void *FxTexture(void *dev, int id)
{
    static const char *const kName[FXX_N] = { "vignette", "film burn", "goggle", "scanline", "CRT mask", "grain",
                                              "colour grain", "VHS lines", "film specks", "heat palette", "ghost glow", "spirit fire atlas", "scan roll band", "rain drop", "rain streak",
                                              "spirit aura atlas", "rain trail", "death presence fog", "death presence wisps", "death presence smoke" };
    FxDeviceCheck(dev);  // v3.35 FX335: (see FxShader)
    if (g_fxTex[id]) return g_fxTex[id];
    if (g_fxTexFailed[id] > 3) return NULL;
    g_fxTex[id] = FxMakeTex(dev, id);
    if (!g_fxTex[id] && ++g_fxTexFailed[id] == 1) Log("screen effects: the %s texture could not be made (tried again later)", kName[id]);
    return g_fxTex[id];
}

// A new device: the old resources went with it (not released: they were the old device's).
static void FxDeviceCheck(void *dev)
{
    if (dev == g_fxDev) return;
    g_fxDev = dev;
    memset(g_fxPs, 0, sizeof g_fxPs);
    memset(g_fxPsFailed, 0, sizeof g_fxPsFailed);
    memset(g_fxTex, 0, sizeof g_fxTex);
    memset(g_fxTexFailed, 0, sizeof g_fxTexFailed);
    g_gvGlowPs = 0;
}

static int FxShader(void *dev, int tech)
{
    HRESULT hr;
    DWORD h = 0;
    if (tech <= FXT_NONE || tech >= FXT_N) return 0;
    // v3.35 FX335: the device check here (and in FxTexture), not only in the filter / noise / backdrop passes: the effects
    // (Echo's warm-up, After Shock, Death Presence) made their shaders and textures before any of those passes had set
    // g_fxDev, and the first such pass then dropped them as "another device's" (the user's log: "heat shader made" twice
    // on one device 0.7 s apart, the second at Echo's first draw: a 69 ms frame right after its warm-up)
    FxDeviceCheck(dev);
    if (g_fxPs[tech]) return 1;
    if (g_fxPsFailed[tech]) return 0;
    hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kFxPsCode[tech], &h);
    if (FAILED(hr) || !h) {
        g_fxPsFailed[tech] = 1;  // once per device: the token stream does not change
        Log("screen effects: CreatePixelShader (%s) failed (hr %08lX, handle %lu): that filter draws nothing",
            kFxPsName[tech], (unsigned long)hr, h);
        return 0;
    }
    g_fxPs[tech] = h;
    Log("screen effects: %s shader made (ps.1.1, hr %08lX, handle %lu, device %p)", kFxPsName[tech], (unsigned long)hr, h, dev);
    return 1;
}

// v3.35 FX335: the warm-up's draw: two triangles of zero colour and alpha at the screen's top left corner with whatever
// states the caller set (premultiplied over, additive or alpha blend: no pixel changes), so the driver makes that state
// combination's pipeline now (dgVoodoo2 / D3D12 builds one at its first draw: the first-use hitch)
static void FxWarmQuad(void *dev)
{
    SpVtx q[6];
    int i;
    memset(q, 0, sizeof q);
    for (i = 0; i < 6; ++i) {
        static const BYTE kC[6] = { 0, 1, 2, 1, 3, 2 };
        q[i].x = (float)(kC[i] & 1) * 2.0f + 1.0f, q[i].y = (float)(kC[i] >> 1 & 1) * 2.0f + 1.0f, q[i].z = 0.5f, q[i].rhw = 1.0f;
    }
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, 2, q, sizeof(SpVtx));
}
// v3.35 FX335: the screen effects made ready (echo.inc RfWarmFrame, once a device): every filter's pixel shader and the
// small textures of the filters, the noise types and the rain (the effects' big textures are their own warm-ups': Death
// Presence, Echo, After Shock). Returns the count made; *tex the textures.
static int FxWarmScreen(void *dev, int *tex)
{
    static const BYTE kTex[] = { FXX_VIG, FXX_BURN, FXX_GOGGLE, FXX_SCAN, FXX_MASK, FXX_NOISE, FXX_CNOISE, FXX_VHS, FXX_ATLAS,
                                 FXX_HEAT, FXX_GLOW, FXX_ROLL, FXX_DROP, FXX_STREAK, FXX_TRAIL, FXX_SVATLAS };
    int t, n = 0;
    *tex = 0;
    for (t = FXT_NONE + 1; t < FXT_N; ++t) n += FxShader(dev, t);
    for (t = 0; t < (int)sizeof kTex; ++t) *tex += FxTexture(dev, kTex[t]) != NULL;
    return n;
}

// review FX-006 / FX-008: draw only when the current target is verified to be the back buffer (both asked here)
static int FxOnBackBuffer(void *dev)
{
    void *rt = NULL, *bb = NULL;
    int same;
    if (FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) || !rt) return 0;
    if (FAILED(((GetBB_t)VT(dev, VT_GETBACKBUFFER))(dev, 0, 0, &bb)) || !bb) {
        ((Release_t)VT(rt, VT_RELEASE))(rt);
        return 0;
    }
    same = rt == bb;
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    ((Release_t)VT(bb, VT_RELEASE))(bb);
    return same;
}

// ---------------------------------------------------------------- device state: capture, our base, restore
static const DWORD kFxRs[] = { 7 /*ZENABLE*/, 14 /*ZWRITEENABLE*/, 15 /*ALPHATESTENABLE*/, 19 /*SRCBLEND*/, 20 /*DESTBLEND*/,
                               22 /*CULLMODE*/, 23 /*ZFUNC*/, 27 /*ALPHABLENDENABLE*/, 28 /*FOGENABLE*/,
                               29 /*SPECULARENABLE*/, 52 /*STENCILENABLE*/, 137 /*LIGHTING*/, 168 /*COLORWRITEENABLE*/,
                               171 /*BLENDOP*/, 128 /*WRAP0*/, 129, 130, 131 /*WRAP3*/ };
// COLORWRITEENABLE 7 (RGB): the back buffer's alpha is never written by our draws
static const DWORD kFxRsSet[] = { 0, 0, 0, 2 /*ONE*/, 1 /*ZERO*/, 1 /*NONE*/, 8 /*ALWAYS*/, 0, 0, 0, 0, 0, 7, 1 /*ADD*/, 0, 0, 0, 0 };
static const BYTE kFxTss[][2] = {
    { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 11 }, { 0, 13 }, { 0, 14 }, { 0, 16 }, { 0, 17 },
    { 0, 18 }, { 0, 24 },
    { 1, 1 }, { 1, 4 }, { 1, 11 }, { 1, 13 }, { 1, 14 }, { 1, 16 }, { 1, 17 }, { 1, 18 }, { 1, 24 },
    { 2, 11 }, { 2, 13 }, { 2, 14 }, { 2, 16 }, { 2, 17 }, { 2, 18 }, { 2, 24 },
    { 3, 11 }, { 3, 13 }, { 3, 14 }, { 3, 16 }, { 3, 17 }, { 3, 18 }, { 3, 24 },
    { 1, 2 }, { 1, 3 }, { 1, 5 }, { 1, 6 }, { 2, 1 }, { 2, 4 } };  // v3.26.0 (review SVR-002): the Rain's stage 1 / 2 states
// stage n: TEXCOORDINDEX n, CLAMP, LINEAR (exact at the texel centres the quad hits), no mip, no transform;
// stage 0 MODULATE texture x diffuse, stage 1 off (with a pixel shader set the colour ops are not used)
static const DWORD kFxTssSet[] = {
    4 /*MODULATE*/, 2 /*TEXTURE*/, 0 /*DIFFUSE*/, 2 /*SELECTARG1*/, 2, 0, 0, 3 /*CLAMP*/, 3, 2 /*LINEAR*/, 2, 0, 0,
    1 /*DISABLE*/, 1, 1, 3, 3, 2, 2, 0, 0,
    2, 3, 3, 2, 2, 0, 0,
    3, 3, 3, 2, 2, 0, 0,
    2 /*TEXTURE*/, 1 /*CURRENT*/, 2, 1, 1 /*DISABLE*/, 1 };
enum { FX_NRS = sizeof kFxRs / sizeof kFxRs[0], FX_NTSS = sizeof kFxTssSet / sizeof kFxTssSet[0], FX_NPSC = 8 };

typedef struct FxSavedTag {  // v3.30 ECHO: tagged (echo.inc's forward declaration)
    DWORD rs[FX_NRS], tss[FX_NTSS], vs, ps;
    void *tex[4], *vb;
    UINT stride;
    float psc[FX_NPSC][4];
    int npsc, vpSet;
    Viewport8 vp;
} FxSaved;

static void FxRelease(FxSaved *s)
{
    int i;
    for (i = 0; i < 4; ++i)
        if (s->tex[i]) ((Release_t)VT(s->tex[i], VT_RELEASE))(s->tex[i]);
    if (s->vb) ((Release_t)VT(s->vb, VT_RELEASE))(s->vb);
}
// review FX-007 / FX-009 / FX-010: everything is captured before anything changes; any failed query -> 0, nothing touched
static int FxSave(void *dev, FxSaved *s, int npsc)
{
    int i, bad = 0;
    memset(s, 0, sizeof *s);
    for (i = 0; i < FX_NRS; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kFxRs[i], &s->rs[i]))) bad = 1;
    for (i = 0; i < FX_NTSS; ++i)
        if (FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, kFxTss[i][0], kFxTss[i][1], &s->tss[i]))) bad = 1;
    if (FAILED(((SpGetDW_t)VT(dev, SPV_GETVS))(dev, &s->vs))) bad = 1;
    if (FAILED(((SpGetDW_t)VT(dev, SPV_GETPS))(dev, &s->ps))) bad = 1;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetTex_t)VT(dev, SPV_GETTEX))(dev, (DWORD)i, &s->tex[i]))) {  // AddRef'd on success
            s->tex[i] = NULL;
            bad = 1;
        }
    if (FAILED(((SpGetStream_t)VT(dev, SPV_GETSTREAM))(dev, 0, &s->vb, &s->stride))) {  // AddRef'd
        s->vb = NULL;
        bad = 1;
    }
    for (i = 0; i < npsc; ++i)
        if (FAILED(((FxPsc_t)VT(dev, FXV_GETPSC))(dev, (DWORD)i, s->psc[i], 1))) bad = 1;
    if (FAILED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &s->vp))) bad = 1;  // review FXV2-005: a failed capture too
    if (bad) {
        static int s_logged;
        FxRelease(s);
        if (!s_logged++) Log("screen effects: a device state query failed: the pass is skipped (logged once)");
        return 0;
    }
    s->npsc = npsc;
    {   // a full-screen pass: the whole target
        Viewport8 full;
        full.x = full.y = 0;
        full.w = (DWORD)*(volatile int *)0x5D524C;
        full.h = (DWORD)*(volatile int *)0x5D5250;
        full.minZ = s->vp.minZ;
        full.maxZ = s->vp.maxZ;
        if (s->vp.x != full.x || s->vp.y != full.y || s->vp.w != full.w || s->vp.h != full.h) {
            ((Viewport_t)VT(dev, FXV_SETVP))(dev, &full);
            s->vpSet = 1;
        }
    }
    return 1;
}
static void FxBase(void *dev)
{
    int i;
    for (i = 0; i < FX_NRS; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kFxRs[i], kFxRsSet[i]);
    for (i = 0; i < FX_NTSS; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kFxTss[i][0], kFxTss[i][1], kFxTssSet[i]);
}
static void FxRestore(void *dev, FxSaved *s)
{
    int i;
    if (s->vpSet) ((Viewport_t)VT(dev, FXV_SETVP))(dev, &s->vp);
    if (s->npsc) ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, s->psc, (DWORD)s->npsc);
    for (i = 0; i < 4; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, s->tex[i]);
    ((SpSetStream_t)VT(dev, SPV_SETSTREAM))(dev, 0, s->vb, s->stride);  // FX-010: also an empty stream 0 is put back
    FxRelease(s);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, s->vs);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, s->ps);
    for (i = 0; i < FX_NTSS; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kFxTss[i][0], kFxTss[i][1], s->tss[i]);
    for (i = 0; i < FX_NRS; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kFxRs[i], s->rs[i]);
}

// ---------------------------------------------------------------- quads
typedef struct { float x, y, z, rhw; DWORD c; float t[4][2]; } FxVtx4;  // FVF 0x444: XYZRHW | DIFFUSE | TEX4
static void FxQuad4(void *dev, const float off[4][2])
{
    const float w = FxW(), h = FxH();
    FxVtx4 q[4];
    int m, k;
    for (m = 0; m < 4; ++m) {
        q[m].x = (m & 1) ? w - 0.5f : -0.5f;
        q[m].y = (m & 2) ? h - 0.5f : -0.5f;
        q[m].z = 0.0f;
        q[m].rhw = 1.0f;
        q[m].c = 0xFFFFFFFF;
        for (k = 0; k < 4; ++k) {
            q[m].t[k][0] = ((m & 1) ? 1.0f : 0.0f) + off[k][0];
            q[m].t[k][1] = ((m & 2) ? 1.0f : 0.0f) + off[k][1];
        }
    }
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(FxVtx4));
}
static void FxRect(void *dev, DWORD c, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1)
{
    SpVtx q[4];
    int m;
    for (m = 0; m < 4; ++m) {
        q[m].x = ((m & 1) ? x1 : x0) - 0.5f;
        q[m].y = ((m & 2) ? y1 : y0) - 0.5f;
        q[m].z = 0.0f;
        q[m].rhw = 1.0f;
        q[m].c = c;
        q[m].u = (m & 1) ? u1 : u0;
        q[m].v = (m & 2) ? v1 : v0;
    }
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(SpVtx));
}
static DWORD FxAlpha(float s) { return (DWORD)(FxSat(s) * 255.0f + 0.5f) << 24 | 0x808080; }  // lerp(neutral grey, texel, s)

// The overlay states: no shader, 2x modulate (SRC DESTCOLOR, DEST SRCCOLOR: dest x 2 x source), source =
// lerp(diffuse grey 0x80, texel, diffuse alpha) (BLENDDIFFUSEALPHA), so a 0x80 texel or strength 0 leaves the picture.
static void FxOverlayStates(void *dev)
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 9 /*DESTCOLOR*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 3 /*SRCCOLOR*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 12 /*BLENDDIFFUSEALPHA*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 1 /*COLOROP*/, 1 /*DISABLE*/);
}
static void FxSampler(void *dev, void *tex, int wrap, int point)
{
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, tex);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 13 /*ADDRESSU*/, wrap ? 1 /*WRAP*/ : 3 /*CLAMP*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 14 /*ADDRESSV*/, wrap ? 1 : 3);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 16 /*MAGFILTER*/, point ? 1 /*POINT*/ : 2 /*LINEAR*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 17 /*MINFILTER*/, point ? 1 : 2);
}
// animated grain: the 256 x 256 tile at `px` screen pixels per texel (at 720 lines), a new random offset each tick
// v3.18.0 N1: while a Noise Colour shader is bound for the layer draws (g_fxNCol), each draw sets its colour x strength
static const float kFxNColMask[8][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 0.12f, 0.12f }, { 0.12f, 1, 0.12f },
                                         { 0.15f, 0.3f, 1 }, { 1, 0.62f, 0.12f }, { 0.9f, 0.66f, 0.42f } };
static int g_fxNCol;
static void FxNColS(void *dev, float s)
{
    float c[4][4];
    const int col = (int)g_fx[FX_NCOLOUR];
    const float *m = kFxNColMask[col >= 0 && col < 8 ? col : 1];
    if (!g_fxNCol) return;
    s = FxSat(s);
    c[0][0] = m[0] - 1.0f, c[0][1] = m[1] - 1.0f, c[0][2] = m[2] - 1.0f, c[0][3] = 0.0f;
    c[1][0] = c[1][1] = c[1][2] = c[1][3] = 0.5f;
    c[2][0] = c[2][1] = c[2][2] = c[2][3] = 0.0f;
    c[3][0] = c[3][1] = c[3][2] = c[3][3] = s;
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, 4);
}
static void FxGrain(void *dev, void *tex, float s, float px, DWORD tick, DWORD salt)
{
    const float w = FxW(), h = FxH(), t = px * h / 720.0f > 0.5f ? px * h / 720.0f : 0.5f;
    const float u0 = FxRnd(tick * 2654435761u ^ salt), v0 = FxRnd(tick * 2246822519u ^ salt ^ 0x1234u);
    if (s <= 0.0f) return;
    FxNColS(dev, s);
    FxSampler(dev, tex, 1, 0);
    FxRect(dev, FxAlpha(s), 0, 0, w, h, u0, v0, u0 + w / t / 256.0f, v0 + h / t / 256.0f);
}

// ---------------------------------------------------------------- film artifacts (dust, hair, scratches)
#define FX_SPECKS 20
typedef struct { BYTE cell, life; WORD pad; float x, y, w, h, rot; DWORD born; } FxSpeck;
typedef struct { int n; DWORD tick; FxSpeck s[FX_SPECKS]; } FxSpeckSet;
static FxSpeckSet g_fxFilm, g_fxNoiseDust;
// one step per new tick: the old ones die after their 1-6 ticks, new ones are born at random places
static void FxSpeckStep(FxSpeckSet *ss, DWORD tick, int dustMax, float pHair, float pScratch)
{
    const float w = FxW(), h = FxH(), unit = h / 720.0f;
    DWORD seed = tick * 0x9E3779B1u ^ (ss == &g_fxFilm ? 0xF11Au : 0xD057u);
    int i, k, nd;
    if (tick == ss->tick) return;
    ss->tick = tick;
    for (i = k = 0; i < ss->n; ++i)
        if (tick - ss->s[i].born < ss->s[i].life) ss->s[k++] = ss->s[i];
    ss->n = k;
    nd = (int)(FxHash(seed++) % (DWORD)(dustMax + 1));
    for (i = 0; i < nd + 2 && ss->n < FX_SPECKS; ++i) {
        FxSpeck *p = &ss->s[ss->n];
        int kind;
        if (i < nd) kind = FxRnd(seed++) < 0.7f ? 0 : 3;            // dust (dark) or a white speck
        else if (i == nd) kind = FxRnd(seed++) < pHair ? 1 : -1;     // a hair
        else kind = FxRnd(seed++) < pScratch ? 2 : -1;               // a vertical scratch
        if (kind < 0) continue;
        p->cell = (BYTE)kind;
        p->born = tick;
        p->life = (BYTE)(1 + FxHash(seed++) % 6);
        p->x = FxRnd(seed++) * w;
        p->y = FxRnd(seed++) * h;
        p->rot = FxRnd(seed++) * 6.2831853f;
        if (kind == 0 || kind == 3) p->w = p->h = (3.0f + 7.0f * FxRnd(seed++)) * unit;
        else if (kind == 1) p->w = p->h = (24.0f + 40.0f * FxRnd(seed++)) * unit;
        else {
            p->w = (6.0f + 4.0f * FxRnd(seed++)) * unit;     // the cell's line is about a quarter of its width
            p->h = h * (0.4f + 0.7f * FxRnd(seed++));
            p->rot = 0.0f;
        }
        ++ss->n;
    }
}
static void FxSpeckDraw(void *dev, const FxSpeckSet *ss, void *atlas, float s)
{
    SpVtx v[6 * FX_SPECKS];
    int i, m, n = 0;
    const DWORD c = FxAlpha(s);
    if (!ss->n || s <= 0.0f) return;
    FxNColS(dev, s);
    FxSampler(dev, atlas, 0, 0);
    for (i = 0; i < ss->n; ++i) {
        const FxSpeck *p = &ss->s[i];
        const float cu = (p->cell & 1) * 0.5f, cv = (p->cell >> 1) * 0.5f, cs = cosf(p->rot), sn = sinf(p->rot);
        SpVtx q[4];
        for (m = 0; m < 4; ++m) {
            const float ox = ((m & 1) ? 0.5f : -0.5f) * p->w, oy = ((m & 2) ? 0.5f : -0.5f) * p->h;
            q[m].x = p->x + ox * cs - oy * sn - 0.5f;
            q[m].y = p->y + ox * sn + oy * cs - 0.5f;
            q[m].z = 0.0f;
            q[m].rhw = 1.0f;
            q[m].c = c;
            q[m].u = cu + ((m & 1) ? 0.5f : 0.0f);
            q[m].v = cv + ((m & 2) ? 0.5f : 0.0f);
        }
        v[n++] = q[0];
        v[n++] = q[1];
        v[n++] = q[2];
        v[n++] = q[1];
        v[n++] = q[3];
        v[n++] = q[2];
    }
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
}

// ---------------------------------------------------------------- the shader constants of each technique
static void FxC(float c[FX_NPSC][4], int i, float r, float g, float b, float a)
{
    c[i][0] = r < -1.0f ? -1.0f : r > 1.0f ? 1.0f : r;  // ps.1.x constants are -1..1
    c[i][1] = g < -1.0f ? -1.0f : g > 1.0f ? 1.0f : g;
    c[i][2] = b < -1.0f ? -1.0f : b > 1.0f ? 1.0f : b;
    c[i][3] = a < -1.0f ? -1.0f : a > 1.0f ? 1.0f : a;
}
static void FxConsts(const FxLook *L, DWORD tick, float c[FX_NPSC][4])
{
    const float lr = 0.299f, lg = 0.587f, lb = 0.114f;
    memset(c, 0, sizeof(float) * FX_NPSC * 4);
    switch (L->tech) {
    case FXT_TONE: {
        const float fl = L->flicker > 0.0f ? (FxRnd(tick * 0x51ED27u + 11u) - 0.5f) * 2.0f * L->flicker : 0.0f;
        FxC(c, 0, lr, lg, lb, 0);
        FxC(c, 1, L->tint[0] * 0.5f, L->tint[1] * 0.5f, L->tint[2] * 0.5f, 0);
        FxC(c, 2, L->warm[0], L->warm[1], L->warm[2], 0);
        FxC(c, 3, L->mix, L->mix, L->mix, 0);
        FxC(c, 4, 0.5f, 0.5f, 0.5f, 0);
        FxC(c, 5, L->con * 0.5f, L->con * 0.5f, L->con * 0.5f, 0);
        FxC(c, 6, 0.5f + L->bri + fl, 0.5f + L->bri + fl, 0.5f + L->bri + fl, 0);
        break;
    }
    case FXT_CRT:
        FxC(c, 0, 1, 0, 0, 0);
        FxC(c, 1, 0, 1, 0, 0);
        FxC(c, 2, 0, 0, 1, 0);
        FxC(c, 3, lr, lg, lb, 0);
        FxC(c, 4, L->mix, L->mix, L->mix, 0);
        FxC(c, 5, 0.5f, 0.5f, 0.5f, 0);
        FxC(c, 6, L->con * 0.5f, L->con * 0.5f, L->con * 0.5f, 0);
        FxC(c, 7, 0.5f + L->bri, 0.5f + L->bri, 0.5f + L->bri, 0);
        break;
    case FXT_VIVID: {
        const float off = 0.5f - 0.5f * L->con + L->bri;
        FxC(c, 0, 0.5f, 0.5f, 0.5f, 0.5f);
        FxC(c, 1, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);
        FxC(c, 2, VIVID_SHARP, VIVID_SHARP, VIVID_SHARP, 0);
        FxC(c, 3, lr, lg, lb, 0);
        FxC(c, 4, L->mix, L->mix, L->mix, 0);
        FxC(c, 5, L->con * 0.5f, L->con * 0.5f, L->con * 0.5f, 0);
        FxC(c, 6, off, off, off, 0);
        break;
    }
    case FXT_NOIR:
        FxC(c, 0, lr * NOIR_GAIN, lg * NOIR_GAIN, lb * NOIR_GAIN, 0);
        FxC(c, 1, NOIR_LIFT, NOIR_LIFT, NOIR_LIFT, 0);
        FxC(c, 2, -NOIR_CURVE, -NOIR_CURVE, -NOIR_CURVE, 0);
        FxC(c, 3, NOIR_CURVE, NOIR_CURVE, NOIR_CURVE, 0);
        break;
    case FXT_NV:
        FxC(c, 0, 0.5f, 0.5f, 0.5f, 0.5f);
        FxC(c, 1, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);
        FxC(c, 2, lr, lg, lb, 0);
        FxC(c, 3, NV_BLOOM_TH, NV_BLOOM_TH, NV_BLOOM_TH, 0);
        FxC(c, 4, NV_BLOOM, NV_BLOOM, NV_BLOOM, 0);
        FxC(c, 5, NV_GAIN * L->tint[0] * 0.25f, NV_GAIN * L->tint[1] * 0.25f, NV_GAIN * L->tint[2] * 0.25f, 0);
        FxC(c, 6, 0, NV_FLOOR_G, NV_FLOOR_G * 0.3f, 0);
        break;
    case FXT_THERMAL:
        FxC(c, 0, lr * THERM_GAIN, lg * THERM_GAIN, lb * THERM_GAIN, 0);
        if (ThHeatOk()) {  // v3.22.0: a cold world; the bodies are redrawn hot after it (ThHeatPass)
            FxC(c, 1, THERM_R0, THERM_G0, THERM_B0, 0);
            FxC(c, 2, THERM_RS * 0.25f, THERM_GS * 0.25f, THERM_BS * 0.25f, 0);
            FxC(c, 3, THERM_COLD_END, THERM_COLD_END, THERM_COLD_END, 0);
            FxC(c, 4, THERM_COLD_S * 0.25f, THERM_COLD_S * 0.25f, THERM_COLD_S * 0.25f, 0);
            FxC(c, 5, THERM_COLD_R, THERM_COLD_G, THERM_COLD_B, 0);
        } else {           // fallback: the v3.21 luminance heat ramp with the ambient-boosted bodies
            FxC(c, 1, THERM_HOT_R0, THERM_HOT_G0, THERM_HOT_B0, 0);
            FxC(c, 2, THERM_HOT_RS * 0.25f, THERM_HOT_GS * 0.25f, THERM_HOT_BS * 0.25f, 0);
            FxC(c, 3, THERM_HOT_COLD_END, THERM_HOT_COLD_END, THERM_HOT_COLD_END, 0);
            FxC(c, 4, THERM_HOT_COLD_S * 0.25f, THERM_HOT_COLD_S * 0.25f, THERM_HOT_COLD_S * 0.25f, 0);
            FxC(c, 5, THERM_HOT_COLD_R, THERM_HOT_COLD_G, THERM_HOT_COLD_B, 0);
        }
        break;
    }
}
static void FxOffsets(const FxLook *L, DWORD tick, float off[4][2])
{
    const float w = FxW(), h = FxH(), unit = h / 720.0f, du = 1.0f / w, dv = 1.0f / h, px = L->px * unit;
    int k;
    memset(off, 0, sizeof(float) * 8);
    if (L->tech == FXT_CRT) {
        off[0][0] = -px * du;  // t0 red from the left, t1 green centre, t2 blue from the right
        off[2][0] = px * du;
    } else if (L->tech == FXT_VIVID || L->tech == FXT_NV) {
        off[1][0] = -px * du;  // t1..t3: a triangle around the centre
        off[1][1] = -0.5f * px * dv;
        off[2][0] = px * du;
        off[2][1] = -0.5f * px * dv;
        off[3][1] = px * dv;
    }
    if (L->weavePx > 0.0f) {  // Old Film gate weave: the whole picture jitters a little each tick
        const float ox = (FxRnd(tick * 0x2C1B3C6Du + 5u) - 0.5f) * 2.0f * L->weavePx * unit * du;
        const float oy = (FxRnd(tick * 0x297A2D39u + 9u) - 0.5f) * L->weavePx * unit * dv;
        for (k = 0; k < 4; ++k) {
            off[k][0] += ox;
            off[k][1] += oy;
        }
    }
}

// ---------------------------------------------------------------- the filter pass
static int FxGameplay(void)  // the running game: engine mode 6, sub-mode [0x1236D50] == 2 (live 2026-11-15: gameplay logs
                              // sub-mode 2; KB: "[0x1236D50]==2 running"; the RE's 1 was wrong)
{
    return *(volatile int *)0x1236D4C == 6 && *(volatile int *)0x1236D50 == 2;
}
// E6 at 0x4EFBFD: gameplay and real-time cutscenes (not the frame that takes the overlay picture: its copy must stay
// clean, 0x55AC29 filters that frame), and the examine / message overlay (overlay 1, callback 0x50A5A0). Off: pause,
// map, items, Options, chests, saves, prompts (their menus are drawn in the tick, before this), game over, FMV, loading.
// v3.22.0 G3 (user items 1, 10, 14; review V322-002): the scene pass also runs in the capture frame ([0x12E3E2C] == 1,
// phase 0 / 1), so the copy the game takes at frame end (0x55AC0C) already shows the filter with its auras, soul fires,
// ghost glow, hot bodies and the filtered Item Shine. Every overlay frame that draws that frozen copy ([0x12E3E2C] == 2:
// pause, quit prompt, item ring, map, Options fading in, ...) then needs no filter of its own: menus stay clean, the
// pause keeps everything the user saw (frozen), and Options no longer flashes the clean picture before its fade.
static int g_fxCopyLook = -1;  // the filter baked into the overlay copy (-1: none: the v3.18 overlay rules filter it)
static int g_fxBackLook = -1;  // review V322-003: what this frame's backdrop shows (FxBackdropFrame, frame start)
static LONG g_fxBackFrame = -1;
static int FxCurLook(void) { return g_fx[FX_FILTER] > 0 ? (int)g_fx[FX_FILTER] : -1; }
static int FxCutOff(void) { return g_fx[FX_CUT] == 1 && *(volatile int *)0xFD5C20 == 1; }  // v3.22.0 G5 (item 17)
static int FxSceneGate(void)
{
    const int phase = *(volatile int *)0xFD5C20;
    if (!FxGameplay()) return 0;
    if (phase == 1 && FxCutOff()) return 0;  // v3.22.0 G5: Cut-scenes Off
    if (phase == 0 || phase == 1) return 1;  // v3.22.0: the capture frame too (G3)
    if (phase != 2) return 0;
    if (*(volatile int *)0x12E3E2C == 2) {  // G3: an overlay over the frozen copy
        const int look = g_fxBackFrame == g_fxFrame ? g_fxBackLook : g_fxCopyLook;
        if (look >= 0 || look == FxCurLook()) return 0;  // filtered already (with the chosen filter: review V322-003)
    }
    // v3.18.0 N5: the pause menu (with the Extra Menu pages and Playable Eileen's menu) and the quit prompt keep the
    // filter; the overlay is the one 0x4F9110 runs this tick (QsTick's snapshot), never [0x107BF54] read here
    // (v3.22.0: only for a copy taken without a filter, e.g. the filter was chosen in Options during this pause)
    if (g_qsTickOn && (g_ovlTick == 2 || g_ovlTick == 9)) return 1;
    return *(volatile int *)0x107BF54 == 1 && *(volatile DWORD *)0x107BF5C == 0x50A5A0;
}
static LONG g_fxCapScene = -1;  // v3.22.0 G3: the frame whose capture-frame scene pass ran (0x55AC29 then does nothing)
static int FxCaptureGate(void)  // 0x55AC29: the frame whose copy becomes the overlay picture (taken at 0x55AC0C)
{                               // v3.22.0: only a fallback, when the scene pass did not run in that frame
    const int phase = *(volatile int *)0xFD5C20;
    return FxGameplay() && (phase == 0 || (phase == 1 && !FxCutOff())) && *(volatile int *)0x12E3E2C == 1 &&
           g_fxCapScene != g_fxFrame;
}

static const char *const kFxWhere[4] = { "the page preview (under the page text, 00526A53)", "the scene (004EFBFD)",
                                         "the overlay-picture frame (whole frame, 0055AC29)",
                                         "the paused scene, filter changed (frame start 0055AE31 / 0055AE44)" };
static int GvPass(void *dev, int where);
static float SvChestH(int type);
static int __cdecl ThermalLive(void);
static void ThHeatPass(void *dev);
static int FxPass(int where)  // v3.22.0 (review V322-007): 1 = the filter was drawn
{
    const int f = (int)g_fx[FX_FILTER];
    const FxLook *L;
    FxLook Lu;  // v3.58 PV358: Filter Strength Ultra's copy of the look
    void *dev = DEV, *frame = NULL, *vig = NULL, *scan = NULL, *mask = NULL, *grain = NULL, *atlas = NULL;
    FxSaved sv;
    float c[FX_NPSC][4], off[4][2], fw = 0, fh = 0;
    DWORD tick;
    int i, tech;
    if (f <= 0 || f >= FX_FILTERS) return 0;
    if (!dev || !*(void *volatile *)0x13DC3A0) return 0;
    if (!FxOnBackBuffer(dev)) return 0;
    Lu = kFxLook[f];
    L = PvFsLookApply(&Lu, f) ? &Lu : &kFxLook[f];  // v3.58 PV358: Ultra scales a copy; High / Low / Medium: the table's own
    tech = L->tech;
    FxDeviceCheck(dev);
    PvFsGrab(dev, f);  // v3.58 PV358: Low / Medium: the plain frame, before any filter pass (PvFsMix lays it back)
    if (f == FXF_GHOST) {  // three edge passes, then (gameplay phase 0) the ghosts and their damage areas
        const int ok = GvPass(dev, where);  // v3.29 AFTERIMAGE
        if (ok && where == 1) AiPostDraw(dev, 3);  // v3.29 AFTERIMAGE: the trail as spirit light, after Spirit Vision's own pass
        return ok;
    }
    // every resource this filter needs, or nothing is drawn (fail closed)
    if (tech != FXT_NONE && !FxShader(dev, tech)) return 0;
    if (L->vig > 0.0f && (vig = FxTexture(dev, L->vigTex)) == NULL) return 0;
    if (L->scan > 0.0f && (scan = FxTexture(dev, FXX_SCAN)) == NULL) return 0;
    if (L->mask > 0.0f && (mask = FxTexture(dev, FXX_MASK)) == NULL) return 0;
    if (L->grain > 0.0f && (grain = FxTexture(dev, FXX_NOISE)) == NULL) return 0;
    if (L->specks && (atlas = FxTexture(dev, FXX_ATLAS)) == NULL) return 0;

    if (f != g_fxLastF || where != g_fxLastWhere) {
        g_fxLastF = f;
        g_fxLastWhere = where;
        Log("screen effects: drawing %s over %s (shader %s; vignette %.2f, scanlines %.2f, mask %.2f, grain %.2f%s)",
            kFxName[FX_FILTER][f], kFxWhere[where], tech ? kFxPsName[tech] : "none", L->vig, L->scan, L->mask, L->grain,
            L->specks ? ", film artifacts" : "");
    }
    if (!FxSave(dev, &sv, tech != FXT_NONE ? FX_NPSC : 0)) return 0;
    FxBase(dev);
    tick = FxTick();

    if (tech != FXT_NONE) {  // 1. the frame through the technique's shader (no blending)
        frame = ((void *(__cdecl *)(float *, float *))0x4036E0)(&fw, &fh);  // the frame so far
        if (frame) {
            FxConsts(L, tick, c);
            PvFsConstsApply(f, c);  // v3.58 PV358: Ultra: Vivid's sharpening, Night Vision's bloom (High: nothing)
            FxOffsets(L, tick, off);
            ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);  // FVF XYZRHW | DIFFUSE | TEX4
            ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, FX_NPSC);
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[tech]);
            for (i = 0; i < kFxPsTex[tech]; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, frame);
            FxQuad4(dev, off);
            for (i = 1; i < kFxPsTex[tech]; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, NULL);
        }
        if (frame && (f == FXF_NV || f == FXF_NVMONO) && FxShader(dev, FXT_HILITE)) {  // v3.18.0 N6: lights flare
            void *lit;
            float hc[FX_NPSC][4], hoff[4][2];
            ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
            lit = ((void *(__cdecl *)(float *, float *))0x4036E0)(&fw, &fh);  // the night-vision frame
            if (lit) {
                memset(hc, 0, sizeof hc);
                memset(hoff, 0, sizeof hoff);
                FxC(hc, 0, 0.299f, 0.587f, 0.114f, 0);
                FxC(hc, 1, NVH_T, NVH_T, NVH_T, NVH_T);
                FxC(hc, 2, NVH_K * 0.5f, NVH_K * 0.5f, NVH_K * 0.5f, 0);
                if (f == FXF_NV) FxC(hc, 3, NV_TINT_R, NV_TINT_G, NV_TINT_B, 0);
                else FxC(hc, 3, 1, 1, 1, 0);
                ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, hc, FX_NPSC);
                ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_HILITE]);
                ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, lit);
                ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
                ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
                ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 2 /*ONE*/);
                FxQuad4(dev, hoff);
                ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
            }
        }
        // v3.27.2 (user item 7; review DP3272-001): Death Presence after the night-vision shader (its phosphor floor
        // would lift a black drawn before it) and before the overlays (vignette, grain); its own FxSave / FxRestore
        if (frame && (f == FXF_NV || f == FXF_NVMONO) && where == 1) BG_RUN(BGC_DP, DpPostDraw(dev, NULL));  // v3.35 FX335: BG_RUN (the frame ledger)
    }
    // review FX-004: a filter whose shader pass found no frame copy draws nothing else either (fail closed)
    if (tech == FXT_NONE || frame) {  // 2. the overlays, 2x modulate
        const float w = FxW(), h = FxH();
        FxOverlayStates(dev);
        if (mask) {  // the aperture grille: one texel per pixel, 3-pixel period
            FxSampler(dev, mask, 1, 1);
            FxRect(dev, FxAlpha(L->mask), 0, 0, w, h, 0, 0, w / 256.0f, h / 4.0f);
        }
        if (scan) {  // one band per `period` pixels (about 360 lines on any height)
            int period = ((int)h + 180) / 360;
            if (period < 2) period = 2;
            FxSampler(dev, scan, 1, 0);
            FxRect(dev, FxAlpha(L->scan), 0, 0, w, h, 0, 0, 1.0f, h / (float)period);
        }
        if (grain) FxGrain(dev, grain, L->grain, 1.0f, tick, (DWORD)f);
        if (atlas) {
            FxSpeckStep(&g_fxFilm, tick, OLDF_DUST_MAX, OLDF_HAIR_P, OLDF_SCRATCH_P);
            FxSpeckDraw(dev, &g_fxFilm, atlas, OLDF_SPECKS);
        }
        if (vig) {
            FxSampler(dev, vig, 0, 0);
            FxRect(dev, FxAlpha(L->vig), 0, 0, w, h, 0, 0, 1.0f, 1.0f);
        }
    }
    FxRestore(dev, &sv);
    if (tech == FXT_NONE || frame) PvFsMix(dev);  // v3.58 PV358: Low / Medium: the plain picture back over the filter (before the hot bodies)
    if (f == FXF_THERMAL && where == 1 && frame) {
        ThHeatPass(dev);  // v3.22.0: footprints and hot bodies over the cold world
        BG_RUN(BGC_DP, DpPostDraw(dev, NULL));  // v3.27.2 (user item 6): Death Presence black over the heat palette (v3.35 FX335: BG_RUN)
        AiPostDraw(dev, 1);  // v3.29 AFTERIMAGE: the trail as residual body heat, after the hot bodies (its own ledger part)
        BG_RUN(BGC_ECHO, EchoPostDraw(dev, NULL, 1));  // v3.30 ECHO: the Echo double, cold (a faint deep blue-black) (v3.35 FX335: BG_RUN)
    }
    return tech == FXT_NONE || frame != NULL;
}

// E5: a Noise Type layer instead of the game's grain grid (0x432FBE). 1 = drawn; 0 = the stock call draws (nothing
// of ours could be drawn: logged once).
// v3.18.0 N2: the Scan Roll band and the Rain drops / streaks (real time, independent of the frame rate)
typedef struct { float x, t0, len; } FxStreak;  // v3.26.0: the drops moved to rain_sim.h / rain.inc (streaks per state there)
static float g_fxRollAt = -1.0f, g_fxRollDir = 1.0f;
static float FxSecs(void) { return (float)((double)FxClock() / (double)g_fxQf.QuadPart); }
static void FxScanRoll(void *dev, float s0, float w, float h, DWORD tick)
{
    const float now = FxSecs(), bh = FXN_ROLL_H * h;
    float ph, y;
    void *band = FxTexture(dev, FXX_ROLL), *grain = FxTexture(dev, FXX_NOISE);
    if (g_fxRollAt < 0.0f || now - g_fxRollAt > FXN_ROLL_S + FXN_ROLL_GAP1 * 2.0f) {  // first use / after a long pause
        g_fxRollAt = now + FXN_ROLL_GAP0 + (FXN_ROLL_GAP1 - FXN_ROLL_GAP0) * FxRnd(tick * 0x9E3779B9u);
    }
    ph = (now - g_fxRollAt) / FXN_ROLL_S;
    if (ph >= 1.0f) {  // the next roll: a random gap and direction
        g_fxRollAt = now + FXN_ROLL_GAP0 + (FXN_ROLL_GAP1 - FXN_ROLL_GAP0) * FxRnd(tick * 0x85EBCA6Bu + 7u);
        g_fxRollDir = FxRnd(tick * 0xC2B2AE35u + 3u) < 0.5f ? 1.0f : -1.0f;
        return;
    }
    if (ph < 0.0f || !band || !grain) return;
    y = g_fxRollDir > 0.0f ? -bh + ph * (h + bh) : h - ph * (h + bh);
    FxSampler(dev, band, 0, 0);
    FxNColS(dev, s0 * FXN_ROLL_GAIN);
    FxRect(dev, FxAlpha(s0 * FXN_ROLL_GAIN), 0, y, w, y + bh, 0, 0, 1, 1);
    FxSampler(dev, grain, 1, 0);  // a horizontal grain smear inside the band
    {
        const float u0 = FxRnd(tick * 0x27D4EB2Fu), bv = FxRnd(tick * 0x165667B1u);
        FxNColS(dev, s0 * FXN_ROLL_GAIN * 0.6f);
        FxRect(dev, FxAlpha(s0 * FXN_ROLL_GAIN * 0.6f), 0, y, w, y + bh, u0, bv, u0 + 0.05f, bv + bh / 3.0f / 256.0f);
    }
}
// E5: a Noise Type layer instead of the game's grain grid (0x432FBE). 1 = drawn; 0 = the stock call draws (nothing
// of ours could be drawn: logged once). v3.18.0: types 1 Fine, 2 Coarse, 3 Film, 4 VHS, 5 Scan Roll, 6 Rain; Stock (0)
// comes here only with a Noise Colour (Refined's replica of the game's grid in that colour); N1 colour shader.
static int FxNoiseLayer(int level, int preview)
{
    const int nt = (int)g_fx[FX_NTYPE], col = (int)g_fx[FX_NCOLOUR];
    const float s0 = (float)level / 255.0f;
    void *dev = DEV, *noise = NULL, *atlas = NULL;
    FxSaved sv;
    DWORD tick;
    float w, h;
    if (nt < 0 || nt > 6 || (nt == 0 && col <= 0) || level <= 0 || !dev) return 0;
    if (!FxOnBackBuffer(dev)) return 0;
    FxDeviceCheck(dev);
    noise = FxTexture(dev, nt == 4 ? FXX_VHS : col == 2 ? FXX_CNOISE : FXX_NOISE);
    if (nt == 3) atlas = FxTexture(dev, FXX_ATLAS);
    if (!noise || (nt == 3 && !atlas) || (nt == 4 && !FxTexture(dev, FXX_NOISE)) ||
        (nt == 5 && (!FxTexture(dev, FXX_ROLL))) || (nt == 6 && (!FxTexture(dev, FXX_DROP) || !FxTexture(dev, FXX_STREAK) || !FxTexture(dev, FXX_TRAIL)))) {
        static int s_logged;
        if (!s_logged++) Log("screen effects: a Noise Type texture is missing: the game's own grain is drawn (logged once)");
        return 0;
    }
    if (!FxSave(dev, &sv, col > 0 ? 4 : 0)) return 0;
    FxBase(dev);
    FxOverlayStates(dev);
    g_fxNCol = 0;
    if (col > 0) {  // N1: the colour shader (fail closed: the layer is drawn untinted, logged by FxShader)
        if (FxShader(dev, FXT_NCOL)) {
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_NCOL]);
            g_fxNCol = 1;
        }
    }
    tick = FxTick();
    w = FxW();
    h = FxH();
    switch (nt) {
    case 0: FxGrain(dev, noise, s0 * FXN_STOCK_GAIN, FXN_STOCK_PX, tick, 0x0F0u); break;              // Stock replica
    case 1: FxGrain(dev, noise, s0 * FXN_FINE_GAIN, FXN_FINE_PX, tick, 0x101u); break;                // Fine
    case 2: FxGrain(dev, noise, s0 * FXN_COARSE_GAIN, FXN_COARSE_PX, tick, 0x202u); break;            // Coarse
    case 3:                                                                                           // Film
        FxGrain(dev, noise, s0 * FXN_FILM_GAIN, FXN_FINE_PX, tick, 0x404u);
        FxSpeckStep(&g_fxNoiseDust, tick, FXN_FILM_DUST_MAX, 0.0f, 0.0f);
        FxSpeckDraw(dev, &g_fxNoiseDust, atlas, s0 * FXN_FILM_DUST_GAIN);
        break;
    case 4: {                                                                                         // VHS
        // horizontal line noise: 64 x 256 rows, one row per 2 px at 720 lines, a new offset each tick
        const float v0 = FxRnd(tick * 0x3C6EF372u + 3u), rows = 360.0f / 256.0f;
        const float band = FxSat(s0 * FXN_VHS_BAND_GAIN), bh = FXN_VHS_BAND_H * h;
        const float y = (float)(tick % FXN_VHS_BAND_TICKS) / (float)FXN_VHS_BAND_TICKS * (h + bh) - bh;
        FxSampler(dev, noise, 1, 0);
        FxNColS(dev, s0 * FXN_VHS_GAIN);
        FxRect(dev, FxAlpha(s0 * FXN_VHS_GAIN), 0, 0, w, h, FxRnd(tick + 77u), v0, FxRnd(tick + 77u) + 1.0f, v0 + rows);
        // the tracking band: streaky grain (the tile stretched sideways), moving slowly down the screen
        FxSampler(dev, FxTexture(dev, col == 2 ? FXX_CNOISE : FXX_NOISE), 1, 0);
        {
            const float u0 = FxRnd(tick * 0x85EBCA6Bu), bv = FxRnd(tick * 0xC2B2AE35u);
            FxNColS(dev, band);
            FxRect(dev, FxAlpha(band), 0, y, w, y + bh, u0, bv, u0 + 0.06f, bv + bh / 3.0f / 256.0f);
        }
        break;
    }
    case 5:                                                                                           // Scan Roll
        FxGrain(dev, noise, s0 * FXN_FINE_GAIN, FXN_FINE_PX, tick, 0x505u);
        FxScanRoll(dev, s0, w, h, tick);
        break;
    case 6:                                                                                           // Rain
        FxGrain(dev, noise, s0 * FXN_FINE_GAIN * FXN_RAIN_GRAIN, FXN_FINE_PX, tick, 0x606u);
        FxRainDraw(dev, s0, w, h, tick, preview);  // v3.26.0: rain on glass (rain.inc)
        break;
    }
    g_fxNCol = 0;
    FxRestore(dev, &sv);
    return 1;
}

// 0x4EFBFD (items.inc hkHudSparkle, before Item Shine and the HUD): the scene pass. Logs the gate inputs when they change
// (live check 1 of RE_order: the examine text is overlay 1 with callback 0x50A5A0), at most 60 lines.
static void FxCaptureClean(void);
static void FxPreviewKeyFrame(void);
static void PpScene(int capture);  // v3.43 PP343: post.inc (bloom, grading, lens: after the clean grabs, before the filter)
static void PpPaused(void);
static void PpPreview(void);
static int PpCopyStale(void);
static void PpInstall(void);
static void RfShaderLint(void);  // v3.44 CS344-LINT-1: dof.inc (after every shader stream: screen, shadows, depth of field)
static void FxSceneFrame(void)
{
    static int s_prev[5] = { -1, -1, -1, -1, -1 }, s_logs;
    const int capture = *(volatile int *)0x12E3E2C == 1 && *(volatile int *)0x1236D4C == 6;
    if (!g_fxOn) return;
    if (g_fx[FX_FILTER] <= 0) {  // no filter: only the clean grabs (PreviewKey; a pause preview is not needed)
        if (capture) g_fxCopyLook = -1, FxCaptureClean(), PvCapture();  // v3.56 PV356: + the look layers (preview.inc)
        else FxPreviewKeyFrame();
        PpScene(capture);  // v3.43 PP343
        return;
    }
    if (s_logs < 60 && *(volatile int *)0x1236D4C == 6) {
        const int now[5] = { *(volatile int *)0x1236D50, *(volatile int *)0xFD5C20, *(volatile int *)0x107BF54,
                             (int)*(volatile DWORD *)0x107BF5C, *(volatile int *)0x12E3E2C };
        if (memcmp(now, s_prev, sizeof now)) {
            memcpy(s_prev, now, sizeof now);
            ++s_logs;
            Log("screen effects: sub-mode %d, phase %d, overlay %d, callback %08X, picture %d: scene filter %s", now[0], now[1],
                now[2], now[3], now[4], FxSceneGate() ? "on" : "off");
        }
    }
    if (capture) {  // v3.22.0 G3: the capture frame: clean grabs first, then the filter the copy will keep
        const int on = FxSceneGate();
        FxCaptureClean();
        PvCapture();  // v3.56 PV356: the pause's look layers for the Effects page (the clean frame is drawn back; preview.inc)
        PpScene(1);  // v3.43 PP343: the copy keeps them too
        if (on && FxPass(1)) {  // review V322-007: only a drawn filter marks the copy (else 0x55AC29's fallback runs and
            InterlockedExchange(&g_fxCapScene, g_fxFrame);  // the copy stays clean: the v3.18 overlay rules filter it)
            g_fxCopyLook = (int)g_fx[FX_FILTER];
        } else {
            g_fxCopyLook = -1;
        }
        return;
    }
    FxPreviewKeyFrame();  // v3.22.0 (item 3a): a PreviewKey press grabs this frame, still clean (before the filter)
    PpScene(0);  // v3.43 PP343
    if (FxSceneGate()) FxPass(1);
}

// 0x55AC29 (right after the stock post pass `call 55A260` at 0x55AC24, which stays stock): `mov eax,[6E7D94h]` (5 bytes)
// -> a call to hkFxAfterPost. Live finding (v3.15.0): 55A260 uses the stack slot above its three arguments as scratch,
// so the pass runs after the untouched call, every register preserved.
// ---------------------------------------------------------------- v3.18.0 N4: the page backdrop and its auto-capture
// PLAN N4, RE_capture.md. The picture is the user's own file (nothing is shipped); without one, the first gameplay
// pause saves the clean capture copy ([0x13DC3A0] before FxPass(2) touches it) as an uncompressed BGRX DDS.
typedef struct { DWORD fmt, type, usage, pool; UINT size; DWORD ms; UINT w, h; } FxSurfDesc8;  // D3DSURFACE_DESC (D3D8)
typedef struct { INT pitch; BYTE *bits; } FxLockRect8;
typedef HRESULT(STDMETHODCALLTYPE *FxGetLevel_t)(void *, UINT, void **);
typedef HRESULT(STDMETHODCALLTYPE *FxGetDesc_t)(void *, FxSurfDesc8 *);
typedef HRESULT(STDMETHODCALLTYPE *FxImgSurf_t)(void *, UINT, UINT, DWORD, void **);
typedef HRESULT(STDMETHODCALLTYPE *FxRtSurf_t)(void *, UINT, UINT, DWORD, DWORD, BOOL, void **);
typedef HRESULT(STDMETHODCALLTYPE *FxCopyRects_t)(void *, void *, const RECT *, UINT, void *, const POINT *);
typedef HRESULT(STDMETHODCALLTYPE *FxSurfLock_t)(void *, FxLockRect8 *, const RECT *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *FxSurfUnlock_t)(void *);
#include "rain.inc"  // v3.26.0: Noise Type Rain on glass (needs the typedefs above; called from FxNoiseLayer)
static char g_fxPrevPath[MAX_PATH];
static int g_fxPrevCapture, g_fxPrevHook, g_fxPrevFailed, g_fxPrevCaptured;
static void *g_fxPrevTex, *g_fxPrevDev;
static UINT g_fxPrevW, g_fxPrevH;
static const DWORD g_fnPageFrame = 0x524010;
static int g_fxPrevDrew;  // FxBackdropC drew the picture: the hook skips 0x524010 (the Options hallway widget, F000F1E8)
static int g_fxPrevPause;  // v3.44 DF344: the picture drawn is this pause's (dof.inc: its depth map fits it)
// v3.22.0 (item 3a): the clean picture of the latest gameplay pause, for the Screen Effects page opened from that pause
static DWORD *g_fxPausePic;
static UINT g_fxPausePicW, g_fxPausePicH;
static LONG g_fxPausePicSeq;          // bumped at every new pause picture (the page re-uploads its texture)
static int g_fxKeyVk, g_fxKeyDown, g_fxKeyArmed;  // [ScreenEffects] PreviewKey (0 = none)
static int FxPrevExists(void) { return g_fxPrevPath[0] && GetFileAttributesA(g_fxPrevPath) != INVALID_FILE_ATTRIBUTES; }
// v3.58 PV358: the file part of FxPrevLoad, by path (PreviewImage and the stock picture SH4Refined_preview_stock.dds): a
// DDS (DXT1/3/5 or 32-bit, any size TpDdsParse reads; F7 writes 32-bit BGRX up to 1280 x 720) into a MANAGED texture.
// *missing 1: no such file (nothing logged: the callers say what they show instead). The lines as v3.18 - v3.57's.
static void *FxDdsLoad(void *dev, const char *path, UINT *pw, UINT *ph, int *missing)
{
    HANDLE f;
    DWORD sz = 0, got = 0;
    BYTE *buf;
    TpDds dds;
    const char *why;
    void *tex = NULL;
    struct { int pitch; BYTE *bits; } lr;
    UINT r;
    *missing = 0;
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        *missing = 1;
        return NULL;
    }
    sz = GetFileSize(f, NULL);
    if (sz < 128 || sz > 96u * 1024 * 1024 || (buf = (BYTE *)malloc(sz)) == NULL) {
        CloseHandle(f);
        Log("screen effects: preview picture %s: unusable size (%lu bytes)", path, sz);
        return NULL;
    }
    if (!ReadFile(f, buf, sz, &got, NULL) || got != sz) got = 0;
    CloseHandle(f);
    why = got ? TpDdsParse(buf, sz, &dds) : "read failed";
    if (why) {
        Log("screen effects: preview picture %s not used: %s", path, why);
        free(buf);
        return NULL;
    }
    if (FAILED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, dds.w, dds.h, 1, 0, dds.fmt, 1 /*MANAGED*/, &tex)) || !tex) {
        Log("screen effects: preview picture %s (%ux%u %s): the texture could not be made", path, dds.w, dds.h, TpFmtName(dds.fmt));
        free(buf);
        return NULL;
    }
    if (FAILED(((SpLock_t)VT(tex, 16))(tex, 0, &lr, NULL, 0))) {
        ((Release_t)VT(tex, VT_RELEASE))(tex);
        free(buf);
        return NULL;
    }
    for (r = 0; r < dds.rows[0]; ++r) memcpy(lr.bits + r * lr.pitch, buf + dds.off[0] + r * dds.rowBytes[0], dds.rowBytes[0]);
    ((SpUnlock_t)VT(tex, 17))(tex, 0);
    free(buf);
    *pw = dds.w;
    *ph = dds.h;
    Log("screen effects: preview picture %s: %ux%u %s", path, dds.w, dds.h, TpFmtName(dds.fmt));
    return tex;
}
static void *FxPrevLoad(void *dev)
{
    int missing;
    void *tex;
    UINT w = 0, h = 0;
    if (g_fxPrevDev != dev) {  // a new device: the old texture went with the old one
        g_fxPrevDev = dev;
        g_fxPrevTex = NULL;
        g_fxPrevFailed = 0;
    }
    if (g_fxPrevTex || g_fxPrevFailed || !g_fxPrevPath[0]) return g_fxPrevTex;
    g_fxPrevFailed = 1;  // until it is made (a missing file is checked again after a capture)
    if ((tex = FxDdsLoad(dev, g_fxPrevPath, &w, &h, &missing)) == NULL) return NULL;  // v3.58 PV358: FxDdsLoad (the same lines)
    g_fxPrevTex = tex;
    g_fxPrevW = w;
    g_fxPrevH = h;
    g_fxPrevFailed = 0;
    return tex;
}
// v3.21.0 (review V321-004): the Screen Effects page is on screen when it is the committed page or the target of the
// slide that runs (g_lpage changes only at SlideCommit); the backdrop and its filter share this gate.
static int FxPageShown(void) { return OlFxPage(g_lpage) || OlFxPage(g_pending); }  // v3.54 OL34: Effects and Effects > Film Grain (layout.inc)
// v3.22.0 (item 3a): Options opened from a gameplay pause: the page shows the clean picture of that pause (FxCaptureClean)
static void *g_fxPauseTex, *g_fxPauseDev;
static LONG g_fxPauseTexSeq = -1;
static UINT g_fxPauseTexW, g_fxPauseTexH;
static void *FxPauseTex(void *dev)
{
    struct { int pitch; BYTE *bits; } lr;
    UINT r;
    if (FxGameplay() && *(volatile int *)0xFD5C20 == 2) {  // v3.41 SH341: this pause's picture kept on the GPU (shadows.inc)
        static LONG s_logSeq = -1;
        UINT gw = 0, gh = 0;
        void *gt = ShPauseTex(dev, g_fxPausePicSeq, &gw, &gh);
        if (gt) {
            g_fxPauseTexW = gw, g_fxPauseTexH = gh, g_fxPauseTexSeq = -1;  // (the MANAGED copy below is uploaded again if used)
            if (s_logSeq != g_fxPausePicSeq) {
                s_logSeq = g_fxPausePicSeq;
                Log("screen effects: the Screen Effects page shows the paused scene (%ux%u, kept on the GPU)", gw, gh);
            }
            return gt;
        }
    }
    if (!g_fxPausePic || !FxGameplay() || *(volatile int *)0xFD5C20 != 2) return NULL;
    if (g_fxPauseDev != dev) g_fxPauseDev = dev, g_fxPauseTex = NULL, g_fxPauseTexSeq = -1;  // the old one went with it
    if (g_fxPauseTex && (g_fxPauseTexW != g_fxPausePicW || g_fxPauseTexH != g_fxPausePicH)) {
        ((Release_t)VT(g_fxPauseTex, VT_RELEASE))(g_fxPauseTex);
        g_fxPauseTex = NULL;
    }
    if (!g_fxPauseTex) {
        if (FAILED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, g_fxPausePicW, g_fxPausePicH, 1, 0, 22 /*X8R8G8B8*/, 1 /*MANAGED*/,
                                                           &g_fxPauseTex)) || !g_fxPauseTex) {
            g_fxPauseTex = NULL;
            return NULL;
        }
        g_fxPauseTexW = g_fxPausePicW, g_fxPauseTexH = g_fxPausePicH, g_fxPauseTexSeq = -1;
    }
    if (g_fxPauseTexSeq != g_fxPausePicSeq) {
        if (FAILED(((SpLock_t)VT(g_fxPauseTex, 16))(g_fxPauseTex, 0, &lr, NULL, 0))) return NULL;
        for (r = 0; r < g_fxPauseTexH; ++r) memcpy(lr.bits + r * lr.pitch, g_fxPausePic + (size_t)r * g_fxPauseTexW, g_fxPauseTexW * 4);
        ((SpUnlock_t)VT(g_fxPauseTex, 17))(g_fxPauseTex, 0);
        g_fxPauseTexSeq = g_fxPausePicSeq;
        Log("screen effects: the Screen Effects page shows the paused scene (%ux%u)", g_fxPauseTexW, g_fxPauseTexH);
    }
    return g_fxPauseTex;
}
// review V322-003: the filter was changed in Options during this pause: the frozen copy shows the old one. At the frame
// start of an overlay frame (0x55AE31 / 0x55AE44, right after the game drew the copy), the clean pause picture
// (FxCaptureClean, at most 1280 x 720) is drawn over it and filtered with the chosen filter (edges / palette only: the 3D
// auras and heat of the paused frame are not in a clean picture). Options itself (overlay 6) draws its own backdrop.
static void FxDrawPic(void *dev, void *tex, UINT pw, UINT ph);
static void __cdecl FxBackdropFrame(void)
{
    static int s_logged;
    void *dev = DEV, *tex;
    g_fxBackFrame = g_fxFrame;
    g_fxBackLook = g_fxCopyLook;
    if (!g_fxOn || !FxGameplay() || (g_fxCopyLook == FxCurLook() && !PpCopyStale()) || *(volatile int *)0x107BF54 == 6) return;  // v3.43 PP343: + PpCopyStale
    if (!dev || !FxOnBackBuffer(dev) || (tex = FxPauseTex(dev)) == NULL) return;
    __try {
        FxDrawPic(dev, tex, g_fxPauseTexW, g_fxPauseTexH);
        PpPaused();  // v3.43 PP343: bloom / grading / lens over the clean picture, before the filter
        g_fxBackLook = FxCurLook() <= 0 || FxPass(3) ? FxCurLook() : -1;  // (V322-007: a failed pass leaves it clean)
        if (g_fxBackLook > 0) PvPageDraw(1);  // v3.56 PV356: Thermal's heat / Spirit Vision's ghosts of this pause over it (preview.inc)
        if (!s_logged++) Log("screen effects: the filter changed during a pause: the paused scene is shown with the new one");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
static const DWORD g_fnCopyDraw = 0x55A8C0;
static __declspec(naked) void hkFxCopyDraw(void)  // 55AE31 / 55AE44: was `call 55A8C0` (one argument, the caller pops it)
{
    __asm {
        push dword ptr [esp + 4]
        call dword ptr [g_fnCopyDraw]
        add esp, 4
        pushad
        pushfd
        call FxBackdropFrame
        popfd
        popad
        ret
    }
}

// 0x526A53 (the Options page frame, before its frame widget and rows): the picture behind the Screen Effects page
static void __cdecl FxBackdropC(void)
{
    void *dev = DEV, *tex;
    FxSaved sv;
    float w, h, a, b, u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    int pause = 0;  // v3.44 DF344
    g_fxPrevDrew = 0;
    g_fxPrevPause = 0;
    if (ShPageShown()) {  // v3.40 SH340: the Shadows page's backdrop (the paused scene without its shadow, or preview.dds)
        if (PvPreviewOff()) PvOffNote(LP_SHADOWS);  // v3.58 PV358: Preview Off (ShMode answers none: the game's background)
        __try {
            g_fxPrevDrew = dev ? ShBackdrop(dev) : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_fxPrevDrew = 0;
        }
        return;
    }
    if (!g_fxOn || !FxPageShown() || !dev) return;
    __try {
        if (!FxOnBackBuffer(dev)) return;
        FxDeviceCheck(dev);
        {
            UINT pw = 0, ph = 0;
            // v3.58 PV358 (USER_FEEDBACK_v357, Effects Preview): by [ScreenEffects] Preview (preview.inc PvBackPick): Off no
            // backdrop (the game's own background), Stock the stock picture (the title menu and a pause), Live a pause's own
            // picture (v3.22.0), else the stock picture, else PreviewImage; the scene without a picture (v3.56 PV356)
            const int back = PvBackPick(dev, &tex, &pw, &ph, &pause);
            PvBackNote(back);  // v3.56 PV356: what this frame's backdrop is (the layer over it follows it)
            if (back == PVB_OFF) {
                PvOffNote(g_lpage);
                return;  // (g_fxPrevDrew 0: 0x524010 draws the game's own background; FxBackdropFilter adds nothing)
            }
            if (back == PVB_SCENE) {
                if (PvSceneDraw(dev)) g_fxPrevDrew = 1;
                else PvBackNote(PVB_STOCK);
                return;
            }
            if (!tex || !pw || !ph) {
                PvBackNote(PVB_STOCK);
                return;
            }
            if (!FxSave(dev, &sv, 0)) return;
            FxBase(dev);
            w = FxW();
            h = FxH();
            a = w / h;
            b = (float)pw / (float)ph;
        }
        if (b > a) u0 = 0.5f - 0.5f * a / b, u1 = 1.0f - u0;  // aspect-fill: crop the sides
        else v0 = 0.5f - 0.5f * b / a, v1 = 1.0f - v0;         // or top and bottom
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
        FxSampler(dev, tex, 0, 0);
        FxRect(dev, 0xFFFFFFFFu, 0, 0, w, h, u0, v0, u1, v1);
        FxRestore(dev, &sv);
        g_fxPrevDrew = 1;
        g_fxPrevPause = pause;  // v3.44 DF344
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) Log("screen effects: the preview picture draw faulted (logged once)");
    }
}
// v3.22.0: a picture over the whole frame, aspect-filled (FxBackdropC's draw)
static void FxDrawPic(void *dev, void *tex, UINT pw, UINT ph)
{
    FxSaved sv;
    float w, h, a, b, u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (!pw || !ph || !FxSave(dev, &sv, 0)) return;
    FxBase(dev);
    w = FxW();
    h = FxH();
    a = w / h;
    b = (float)pw / (float)ph;
    if (b > a) u0 = 0.5f - 0.5f * a / b, u1 = 1.0f - u0;
    else v0 = 0.5f - 0.5f * b / a, v1 = 1.0f - v0;
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    FxSampler(dev, tex, 0, 0);
    FxRect(dev, 0xFFFFFFFFu, 0, 0, w, h, u0, v0, u1, v1);
    FxRestore(dev, &sv);
}
// v3.21.0 (user item 3: the Screen Effects page text was unreadable under Spirit Vision): the preview filter runs here,
// on the backdrop only, before the page frame / rows / title / description are drawn (it ran at 0x55AC29 on the finished
// frame, text included). FxPass reads the frame so far through 0x4036E0, as the 0x4EFBFD scene slot does in pause.
static void FxPreviewNoise(void);
static int FxPass(int where);
// v3.22.0 (user item 16, screenshot 04: the text was clean but drowned in Spirit Vision's busy edge lines): soft dark
// panels behind the title + rows and behind the description (canvas units, 512 x 448, the 4:3 interface area), under
// the page text, over the filtered preview. A panel = 3 x 3 quads: the inner rect at the shade, the border feathered
// to 0 over FX_PANEL_F.
// v3.44 DF344 (USER_FEEDBACK_v343: "In our previews the black transparent background makes it hard to view our previews",
// screenshot 10): a light shade (was 72 %) behind the text only: the title, the label column and the value column, with
// the middle of the picture (where the player stands) clear; on every page with a preview (Screen Effects, Shadows).
// Hidden [ScreenEffects] PanelShade = the shade in % (0 = none: the Screen Effects text keeps its black ring; at most 80).
#define FX_PANEL_SHADE 30  // %
#define FX_PANEL_F 22.0f
#define FX_PANEL_LABEL_X1 222.0f  // the label column's right edge (the widest label, "Otherworld Lens", ends near 205)
#define FX_PANEL_VALUE_X0 330.0f  // the value column (the left arrow near 345, the values from 365)
static float FxPanelShade(void)
{
    static int s_pct = -1;
    if (s_pct < 0) {
        const int v = GetPrivateProfileIntA("ScreenEffects", "PanelShade", FX_PANEL_SHADE, g_iniPath);
        s_pct = v < 0 ? 0 : v > 80 ? 80 : v;
        LogV("screen effects: the preview pages' text panels: shade %d %% behind the title, the labels and the values%s", s_pct,
             s_pct != FX_PANEL_SHADE ? " ([ScreenEffects] PanelShade)" : "");
    }
    return (float)s_pct / 100.0f;
}
static void FxPanel(void *dev, float x0, float y0, float x1, float y1)
{
    const float w = FxW(), h = FxH(), S = SH4WM_Scale2D(), sx = w / (512.0f * (S > 0.0f ? S : 1.0f)), sy = h / 448.0f;
    const float xs[4] = { x0 - FX_PANEL_F, x0, x1, x1 + FX_PANEL_F }, ys[4] = { y0 - FX_PANEL_F, y0, y1, y1 + FX_PANEL_F };
    const float shade = FxPanelShade();
    const DWORD in = (DWORD)(shade * 255.0f + 0.5f) << 24;
    SpVtx v[54];
    int i, j, n = 0;
    if (!(shade > 0.0f) || !(x1 > x0) || !(y1 > y0)) return;  // v3.44 DF344: PanelShade=0: no panel
    for (j = 0; j < 3; ++j)
        for (i = 0; i < 3; ++i) {
            SpVtx q[4];
            int k;
            for (k = 0; k < 4; ++k) {
                const int xi = i + (k & 1), yi = j + (k >> 1);
                q[k].x = 0.5f * w + (xs[xi] - 256.0f) * sx - 0.5f;
                q[k].y = ys[yi] * sy - 0.5f;
                q[k].z = 0.0f;
                q[k].rhw = 1.0f;
                q[k].c = (xi == 1 || xi == 2) && (yi == 1 || yi == 2) ? in : 0;  // black, alpha at the inner corners only
                q[k].u = q[k].v = 0.0f;
            }
            v[n++] = q[0], v[n++] = q[1], v[n++] = q[2];
            v[n++] = q[1], v[n++] = q[3], v[n++] = q[2];
        }
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
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
}
// v3.44 DF344: the light panels behind a preview page's title and rows (the rows' text ends at rowsBottom): the title
// (y ~54), the label column and the value column; the description has its own (DescPanelBox). FxBase states set.
static void FxTextPanels(void *dev, float rowsBottom)
{
    FxPanel(dev, 112.0f, 44.0f, 400.0f, 68.0f);                           // the page title
    FxPanel(dev, 34.0f, 94.0f, FX_PANEL_LABEL_X1, rowsBottom);            // the labels
    {   // the values (and their arrows)
        float x0 = FX_PANEL_VALUE_X0, x1 = 478.0f;
        VbxPanelX(g_lpage, &x0, &x1);  // v3.58 MN358: round the page's value box
        FxPanel(dev, x0, 94.0f, x1, rowsBottom);
    }
}
static void FxPanels(void)
{
    void *dev = DEV;
    FxSaved sv;
    if (!dev || !FxOnBackBuffer(dev) || !FxSave(dev, &sv, 0)) return;
    FxBase(dev);
    FxTextPanels(dev, DescRowsBottom() + 2.0f);  // v3.44 DF344: the title and the rows' two columns (v3.43: one 72 % panel)
    {   // the description line (font 1, about y 360..380); v3.32 DESC332: grown to a wrapped description's box
        float x1 = 472.0f, y0 = 352.0f, y1 = 392.0f;
        DescPanelBox(g_lpage, &x1, &y0, &y1);  // v3.54 OL34: the page shown
        FxPanel(dev, 40.0f, y0 + 4.0f, x1, y1 - 4.0f);
    }
    FxRestore(dev, &sv);
}
static void DofFxPreview(void);  // v3.44 DF344: dof.inc (included after this file)
static void __cdecl FxBackdropFilter(void)
{
    static int s_logged;
    if (ShPageShown()) {  // v3.40 SH340: the Shadows page: its split preview and text panels instead of a filter
        ShPreviewDraw();
        return;
    }
    if (!g_fxOn || !FxPageShown() || !DEV) return;
    if (PvPreviewOff()) return;  // v3.58 PV358: Preview Off: the game's own background, nothing of ours over it (no filter, no panels)
    DofFxPreview();  // v3.44 DF344: the depth of field first (in play it runs before every other effect too)
    PpPreview();  // v3.43 PP343: bloom / grading / lens (the lens at full strength) before the noise and the filter
    FxPreviewNoise();
    FxPass(0);
    PvPageDraw(0);  // v3.56 PV356 (issue 2, improvement 19): Thermal's heat / Spirit Vision's ghosts and fires over it (preview.inc)
    FxPanels();  // v3.22.0 (item 16)
    if (!s_logged++) Log("screen effects: the Screen Effects preview filter runs under the page text (0x526A53, %s)",
                         g_fxPrevDrew ? "preview picture" : "the stock hallway");
}
// 526A53: was `call 524010` (no stack arguments, result not used; it draws the hallway picture immediately, over
// anything drawn before it, so with our picture on the Screen Effects page it is skipped for that frame). v3.21.0: it is
// called from here, then the preview filter runs over the backdrop.
static __declspec(naked) void hkFxPageFrame(void)
{
    __asm {
        pushad
        call FxBackdropC
        popad
        cmp dword ptr [g_fxPrevDrew], 0
        jne drew
        call dword ptr [g_fnPageFrame]
    drew:
        pushad
        pushfd
        call FxBackdropFilter
        popfd
        popad
        ret
    }
}
// The picture: 32-bit BGRX, box-downscaled to at most 1280 x 720 (a malloc'd buffer the caller frees)
static DWORD *FxPrevShrink(const BYTE *bits, int pitch, UINT w, UINT h, UINT *pow, UINT *poh)
{
    UINT f = 1, ow, oh, x, y, i, j;
    DWORD *out;
    while (w / f > 1280 || h / f > 720) ++f;
    ow = w / f;
    oh = h / f;
    if (!ow || !oh || (out = (DWORD *)malloc((size_t)ow * oh * 4)) == NULL) return NULL;
    for (y = 0; y < oh; ++y)
        for (x = 0; x < ow; ++x) {
            DWORD sr = 0, sg = 0, sb = 0;
            for (j = 0; j < f; ++j) {
                const BYTE *p = bits + (size_t)(y * f + j) * pitch + (size_t)x * f * 4;
                for (i = 0; i < f; ++i, p += 4) sb += p[0], sg += p[1], sr += p[2];
            }
            out[y * ow + x] = 0xFF000000u | (sr / (f * f)) << 16 | (sg / (f * f)) << 8 | sb / (f * f);
        }
    *pow = ow;
    *poh = oh;
    return out;
}
// ... written as an uncompressed DDS to a temp file, then renamed over the picture
static int FxPrevWrite(const DWORD *out, UINT ow, UINT oh, UINT w, UINT h)
{
    DWORD hdr[32], done = 0;
    char tmp[MAX_PATH];
    HANDLE fh;
    int ok;
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x20534444u;            // "DDS "
    hdr[1] = 124;
    hdr[2] = 0x100F;                 // CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    hdr[3] = oh;
    hdr[4] = ow;
    hdr[5] = ow * 4;
    hdr[19] = 32;                    // pixel format: size, RGB, 32 bits, masks, no alpha
    hdr[20] = 0x40;
    hdr[22] = 32;
    hdr[23] = 0x00FF0000u;
    hdr[24] = 0x0000FF00u;
    hdr[25] = 0x000000FFu;
    hdr[27] = 0x1000;                // TEXTURE
    if (PvIsStockPath(g_fxPrevPath)) {  // v3.58 PV358: PreviewImage names the stock picture: F7 / the auto-capture never replace it
        static int s_logged;
        if (!s_logged++) Log("screen effects: preview picture %s is the stock picture: F7 and the first-pause capture do not overwrite it "
                             "(set [ScreenEffects] PreviewImage to another file for your own)", g_fxPrevPath);
        return 0;
    }
    sprintf_s(tmp, sizeof tmp, "%s.tmp", g_fxPrevPath);
    fh = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok = fh != INVALID_HANDLE_VALUE;
    if (ok) {
        ok = WriteFile(fh, hdr, 128, &done, NULL) && done == 128 && WriteFile(fh, out, ow * oh * 4, &done, NULL) && done == ow * oh * 4;
        CloseHandle(fh);
        ok = ok && MoveFileExA(tmp, g_fxPrevPath, MOVEFILE_REPLACE_EXISTING);  // v3.22.0: PreviewKey replaces the picture
        if (!ok) DeleteFileA(tmp);
    }
    if (ok) Log("screen effects: preview picture saved: %s (%ux%u from the %ux%u frame)", g_fxPrevPath, ow, oh, w, h);
    return ok;
}
// v3.22.0: the clean frame as a downscaled BGRX buffer. The back buffer is copied into the game's frame texture
// (0x4036E0 -> [0x13DC3A0], as FxPass does) and read back (route 1 system memory; route 2, with MSAA, a lockable render
// target). Only from the scene slot 0x4EFBFD before any filter of this frame (the frame is still clean there).
static DWORD *FxGrabClean(UINT *pow, UINT *poh, UINT *pw, UINT *ph)
{
    void *dev = DEV, *tex;
    void *volatile src = NULL, *volatile mem = NULL;
    FxSurfDesc8 d;
    FxLockRect8 lr;
    HRESULT hr = E_FAIL;
    float fw = 0, fh = 0;
    DWORD *out = NULL;
    volatile int route = 0, locked = 0, owned = 0;
    LARGE_INTEGER q0, q1, q2, q3, qf;  // v3.41 SH341: the read back timed (the pause hitch: 22-36 ms in the user's v3.40 log)
    if (!dev || !FxOnBackBuffer(dev)) return NULL;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    tex = ((void *(__cdecl *)(float *, float *))0x4036E0)(&fw, &fh);  // the frame so far (clean: before our filter)
    if (!tex) return NULL;
    __try {
        if (FAILED(((FxGetLevel_t)VT(tex, 15))(tex, 0, (void **)&src)) || !src) return NULL;
        if (FAILED(((FxGetDesc_t)VT(src, 8))(src, &d)) || (d.fmt != 21 && d.fmt != 22) || d.w < 64 || d.h < 64) {
            Log("screen effects: preview capture: the frame copy is not a 32-bit surface: no capture");
            ((Release_t)VT(src, VT_RELEASE))(src);
            return NULL;
        }
        Interp_OwnBegin();
        owned = 1;
        // route 1 (RE_capture 3): a system-memory surface (works without MSAA; fails cleanly with it)
        if (SUCCEEDED(((FxImgSurf_t)VT(dev, 27))(dev, d.w, d.h, d.fmt, (void **)&mem)) && mem) {
            hr = ((FxCopyRects_t)VT(dev, 28))(dev, src, NULL, 0, mem, NULL);
            if (SUCCEEDED(hr)) route = 1;
            else ((Release_t)VT(mem, VT_RELEASE))(mem), mem = NULL;
        }
        // route 2, only after route 1 failed: a lockable non-multisampled render target (the wrapper resolves into it)
        if (!route && SUCCEEDED(((FxRtSurf_t)VT(dev, 25))(dev, d.w, d.h, d.fmt, 0, TRUE, (void **)&mem)) && mem) {
            hr = ((FxCopyRects_t)VT(dev, 28))(dev, src, NULL, 0, mem, NULL);
            if (SUCCEEDED(hr)) route = 2;
            else ((Release_t)VT(mem, VT_RELEASE))(mem), mem = NULL;
        }
        QueryPerformanceCounter(&q1);
        if (route && SUCCEEDED(((FxSurfLock_t)VT(mem, 9))(mem, &lr, NULL, 0x10 /*READONLY*/)) && lr.bits) locked = 1;
        QueryPerformanceCounter(&q2);
        Interp_OwnEnd();
        owned = 0;
        if (locked) {
            static volatile LONG s_q;
            out = FxPrevShrink(lr.bits, lr.pitch, d.w, d.h, pow, poh);
            *pw = d.w, *ph = d.h;
            ((FxSurfUnlock_t)VT(mem, 10))(mem);
            QueryPerformanceCounter(&q3);
            if (qf.QuadPart && LogQuota(&s_q, 40))  // v3.41 SH341
                LogV("screen effects: a frame read back from the GPU (%ux%u, route %d): copy %.2f ms, read back %.2f ms, shrink %.2f ms",
                     d.w, d.h, (int)route, (double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart,
                     (double)(q2.QuadPart - q1.QuadPart) * 1000.0 / (double)qf.QuadPart, (double)(q3.QuadPart - q2.QuadPart) * 1000.0 / (double)qf.QuadPart);
        } else {
            static int s_logged;
            if (!s_logged++) Log("screen effects: preview capture: not available here (copy %08lX, route %d): add your own preview picture",
                                 (unsigned long)hr, route);
        }
        if (mem) ((Release_t)VT(mem, VT_RELEASE))(mem);
        ((Release_t)VT(src, VT_RELEASE))(src);
    } __except (EXCEPTION_EXECUTE_HANDLER) {  // review SEFX3-002: balance the interpolation guard, release what we hold
        if (owned) Interp_OwnEnd();
        __try {
            if (locked && mem) ((FxSurfUnlock_t)VT(mem, 10))(mem);
            if (mem) ((Release_t)VT(mem, VT_RELEASE))(mem);
            if (src) ((Release_t)VT(src, VT_RELEASE))(src);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        if (out) free(out);
        Log("screen effects: preview capture faulted (logged each time)");
        return NULL;
    }
    return out;
}
static void FxPrevSaved(void)          // the file changed: the page loads it again
{
    g_fxPrevFailed = 0;
    g_fxPrevTex = NULL;  // (a MANAGED texture of the old picture is left to the device; one per save)
}
// the capture frame (0x4EFBFD, phase 0 / 1, before the filter): the auto-capture (once a session, only without a file),
// the pause picture (with a filter chosen: the Screen Effects page from this pause shows the scene under it) and an
// armed PreviewKey
static void FxCaptureClean(void)
{
    const int needAuto = g_fxPrevCapture && !g_fxPrevCaptured && g_fxPrevPath[0] && *(volatile int *)0xFD5C20 == 0;
    const int needPause = 1, needKey = g_fxKeyArmed && g_fxPrevPath[0];  // (a pause picture at every pause: any filter can be
                                                                       // picked on the page from there)
    UINT ow = 0, oh = 0, w = 0, h = 0;
    DWORD *pic;
    if (needAuto) g_fxPrevCaptured = 1;  // once a session, whatever happens
    if (needPause && !needKey && !(needAuto && !FxPrevExists())) {  // v3.41 SH341: the pause picture stays on the GPU (shadows.inc
        const LONG seq = InterlockedIncrement(&g_fxPausePicSeq);    // ShPauseGrab): no read back, no hitch at a pause; the
        if (ShPauseGrab(DEV, seq)) {                                // PreviewKey / first-pause file grabs still read back
            DWORD *old = g_fxPausePic;
            g_fxPausePic = NULL;
            if (old) free(old);
            return;
        }
    }
    if (!(needAuto && !FxPrevExists()) && !needPause && !needKey) return;
    if ((pic = FxGrabClean(&ow, &oh, &w, &h)) == NULL) return;
    if (needKey || (needAuto && !FxPrevExists())) {
        if (FxPrevWrite(pic, ow, oh, w, h)) FxPrevSaved();
        g_fxKeyArmed = 0;
    }
    if (needPause) {
        DWORD *old = g_fxPausePic;
        g_fxPausePic = pic, g_fxPausePicW = ow, g_fxPausePicH = oh;
        InterlockedIncrement(&g_fxPausePicSeq);
        if (old) free(old);
    } else {
        free(pic);
    }
}
// v3.39 PR339 (USER_FEEDBACK_v3381 item 3, the user's "a capture key"): PreviewKey defaults to F7 (was None). F7: no other
// key of Refined (Quick Save F5 / Quick Load F9, the hidden tests F10 / F11, Ctrl+F12), none of Playable Eileen's (its F8
// menu key is gone since v0.67) nor the cheat table's (one hotkey, F12); not F10 (Windows' menu key) nor F12 (the
// debugger break key); one key away from F9's Quick Load is F8, so F7.
// v3.39 REV339 (Codex PR339-001): no migration. F7 is the default only for an ini without the key (IniStr adds it); an
// existing PreviewKey=None stays None, since it may be the user's own choice (the installer sets this user's ini to F7).
// The Filter row's description names the key (sh4windowmod.c ApplyAspectMenu, after ScreenFxInstall read it)
// v3.53 OL1: only the key's sentence ("" without a key); sh4windowmod.c puts it after the row's and each filter's line
static const char *FxFilterDesc(void)
{
    static char s[64];
    if (PvF7Hidden()) return "";  // v3.56 PV356: the page shows SH4 Refined's scene, not the F7 picture (PreviewScene=Scene)  // v3.58 PV358: always
                                  // (the sentence is added where F7 changes the page: preview.inc PvFilterDesc, FxDesc3)
    if (g_fxKeyVk >= VK_F1 && g_fxKeyVk <= VK_F12) {
        sprintf_s(s, sizeof s, OLW_FILTER_F7, g_fxKeyVk - VK_F1 + 1);  // v3.53 OL1: " F%d in play: new preview."  // v3.57 OLP1: " Press F%d in play for a new preview." (39 at most)
        return s;
    }
    return "";
}
// any other gameplay frame (0x4EFBFD, before the filter): PreviewKey pressed -> this clean frame is saved
static void FxPreviewKeyFrame(void)
{
    UINT ow = 0, oh = 0, w = 0, h = 0;
    DWORD *pic;
    int down;
    if (!g_fxKeyVk || !g_fxPrevPath[0] || !FxGameplay() || *(volatile int *)0xFD5C20 != 0) return;
    down = (GetAsyncKeyState(g_fxKeyVk) & 0x8000) != 0 && GetForegroundWindow() == g_hwnd;
    if (!down || g_fxKeyDown) {
        g_fxKeyDown = down;
        return;
    }
    g_fxKeyDown = 1;
    if ((pic = FxGrabClean(&ow, &oh, &w, &h)) == NULL) return;
    if (FxPrevWrite(pic, ow, oh, w, h)) {
        FxPrevSaved();
        QsSound(1);  // v3.39 PR339: the stock save sound says the picture was taken (a sound: nothing lands in the next grab)
    }
    free(pic);
}

// v3.18.0 N3: the grain previews on the Screen Effects page like the filter (before it, so the filter tints it): level
// 48 (the game's common stage level) x Noise Strength, only with Noise Effect On, not under Spirit Vision (G2), and not
// when the game drew its grain this frame already (from pause it runs under Options anyway)
static void FxPreviewNoise(void)
{
    static const int kHalves[4] = { 1, 2, 3, 4 };
    const int s = (int)g_fx[FX_NOISE];
    int l = 48 * kHalves[s >= 0 && s < 4 ? s : 1] / 2;
    if (*(volatile BYTE *)0x1091AE4 == 0 || g_fx[FX_FILTER] == FXF_GHOST || g_fxNoiseFrame == g_fxFrame) return;
    if (l > 255) l = 255;
    if ((g_fx[FX_NTYPE] > 0 || g_fx[FX_NCOLOUR] > 0) && FxNoiseLayer(l, 1)) return;
    __try {
        if (((int(__cdecl *)(int))0x5736D0)((int)0xF000F1DF))  // the grain file is resident (0x567040 does not check)
            ((int(__cdecl *)(int, int))0x567040)(1, l);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
static void __cdecl FxAfterPost(void)
{
    if (g_fxOn) {
        // v3.21.0: the Screen Effects page preview moved to 0x526A53 (FxBackdropFilter), under the page text; on that
        // page's frames nothing runs here (g_fxStamp still marks them).
        if (g_fxStamp != -1 && g_fxStamp == g_fxFrame) {
        }
        else if (g_spInstalled && FxCaptureGate()) {  // the overlay-picture frame (its copy is taken); v3.22.0: only
            FxPass(2);                                    // when the scene pass did not run in it (G3; the clean grabs
        }                                                 // are taken there, FxCaptureClean)
    }
    ShFrameEnd();  // v3.40 SH340: the pause picture's frame, the Shadows preview's copies (before g_fxFrame moves on)
    PvFrameEnd();  // v3.56 PV356: the Effects preview's page open / close, its layers at a pause's end (preview.inc)
    PerfFrame(1);  // v3.37 CP337: the performance overlay, over the finished frame (perfhud.inc)
    if (InterlockedIncrement(&g_fxFrame) == -1) InterlockedExchange(&g_fxFrame, 0);  // FX-002: at the end; never -1 (FX-003)
}
static __declspec(naked) void hkFxAfterPost(void)
{
    __asm {
        pushad
        pushfd
        call FxAfterPost
        popfd
        popad
        mov eax, dword ptr ds:[6E7D94h]  // the 5 stolen bytes
        ret
    }
}

// ---------------------------------------------------------------- Thermal: ghosts hidden, enemies hot (RE_thermal)
// Gameplay only (phase 0, the registry model loop); real-time cutscenes keep their characters (FXV2-004).
static int __cdecl ThermalLive(void)
{
    return g_thermHooks && g_fxOn && g_spInstalled && g_fx[FX_FILTER] == FXF_THERMAL && !g_fxPsFailed[FXT_THERMAL] &&
           FxGameplay() && *(volatile int *)0xFD5C20 == 0;
}
// v3.22.0: body heat also in real-time cut-scenes (phase 1; the trapped Cynthia talks through in-world scenes); ghost
// hiding stays ThermalLive (phase 0, FXV2-004)
static int ThermHeatLive(void)
{
    const int phase = *(volatile int *)0xFD5C20;
    return g_thermHooks && g_fxOn && g_spInstalled && g_fx[FX_FILTER] == FXF_THERMAL && !g_fxPsFailed[FXT_THERMAL] &&
           FxGameplay() && (phase == 0 || phase == 1);
}
static void ThAdd(const BYTE *task, BYTE *model, float heat)  // a hot body for this frame's heat redraw
{
    const LONG now = g_fxFrame;
    ThEnt *e;
    int i;
    if (g_thStamp != now) {  // the first body of a frame: a new list, and the 3D viewport in force (as GvCollect)
        void *dev = DEV;
        g_thStamp = now;
        g_thN = 0;
        g_thVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_thVp)) && g_thVp.w >= 16 && g_thVp.h >= 16;
    }
    if (!model || heat < TH_MIN_HEAT || g_thN >= TH_MAX) return;
    for (i = 0; i < g_thN; ++i)
        if (g_th[i].model == model) return;
    e = &g_th[g_thN];
    memset(e, 0, sizeof *e);
    e->model = model;
    e->heat = heat > 1.0f ? 1.0f : heat;
    if (task) {
        __try {
            e->task = task;
            e->rec = *(const BYTE *const *)(task + 0x41C);
            e->recModel = e->rec ? *(BYTE *const *)(e->rec + 0x14) : NULL;
            e->fn = *(const DWORD *)(task + 0x404);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }
    ++g_thN;
    {   // v3.24.0 (diagnostic for the flicker report): two different bodies drawn at the same spot this frame, logged once
        static int s_dupLogged;
        __try {
            const float *p = (const float *)(model + 0x10);
            for (i = 0; i < g_thN - 1 && !s_dupLogged; ++i) {
                const float *q = (const float *)(g_th[i].model + 0x10);
                if (fabsf(p[0] - q[0]) < 3.0f && fabsf(p[1] - q[1]) < 3.0f && fabsf(p[2] - q[2]) < 3.0f) {
                    s_dupLogged = 1;
                    Log("screen effects: Thermal: two bodies drawn at the same spot (models %p / %p, task fn %08lX / %08lX)",
                        g_th[i].model, model, g_th[i].fn, e->fn);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}
static int ThValid(const ThEnt *e)  // a task entry: still the same live task, record and model; a model-only entry
{                                   // (the trapped Cynthia, drawn earlier this frame): valid within its frame
    if (!e->task) return e->model != NULL;
    __try {
        return e->fn && *(const DWORD *)(e->task + 0x404) == e->fn && *(const BYTE *const *)(e->task + 0x41C) == e->rec &&
               (!e->rec || *(BYTE *const *)(e->rec + 0x14) == e->recModel);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static void ThermNote(int cls, int type, DWORD fn)
{
    static BYTE seenType[0x13][3], seenFn[6];
    static const DWORD kFn[6] = { 0x487650, 0x486A80, 0x488540, 0x486030, 0x5473A0, 0 };
    int k;
    if (type >= 1 && type <= 0x12) {
        if (seenType[type][cls]) return;
        seenType[type][cls] = 1;
        Log("screen effects: Thermal: enemy type %02X %s", type, cls == 2 ? "hidden (ghost)" : cls == 1 ? "drawn hot" : "drawn as is (ThermalCold)");
        return;
    }
    for (k = 0; k < 5 && kFn[k] != fn; ++k) {}
    if (seenFn[k]) return;
    seenFn[k] = 1;
    Log("screen effects: Thermal: %s %08lX hidden", k == 4 ? "a Room 302 haunting object (pre-draw)" : "a wall ghost (update)", fn);
}
// 0 drawn as is, 1 hot (enemy), 2 hidden (ghost / victim / wall ghost / haunting). task = a pool task (rec+0x10).
// note = 1: Thermal's first-sight log lines; *kind / *type (if given): what decided a 2 (Ghost Vision).
static int ThermClassify(const BYTE *t, int note, int *kind, int *type_)
{
    if (kind) *kind = GVK_ACTOR;
    if (type_) *type_ = 0;
    __try {
        const BYTE *rec, *w;
        DWORD fn;
        int type;
        if (!t) return 0;
        if ((const BYTE *)*(void *volatile *)0xFCE6B4 == t || (const BYTE *)*(void *volatile *)0xFD5A50 == t)
            return 1;  // v3.17.0 (user item 18): Eileen / Cynthia as companions are alive: hot (type stays 0: no soul flame)
        rec = *(const BYTE *const *)(t + 0x41C);
        if (rec) {
            if (*(const DWORD *)(rec + 0x24) == 0x5473A0) {  // Room 302 haunting visual (pre-draw)
                if (note) ThermNote(2, 0, 0x5473A0);
                if (kind) *kind = GVK_ROOM302;
                return 2;
            }
            if (*(void *const *)(rec + 0x14) && *(void *const *)(rec + 0x14) == *(void *volatile *)(0x10A0F20 + 0x1B0))
                return 1;  // the player: hot too (v3.17.0, item 18; type 0)
        }
        fn = *(const DWORD *)(t + 0x404);
        if (fn == 0x487650 || fn == 0x486A80 || fn == 0x488540 || fn == 0x486030) {  // wall ghosts / victim placeholder
            if (note) ThermNote(2, 0, fn);
            if (kind) *kind = GVK_WALL;
            return 2;
        }
        type = *(const short *)(t + 0x414);
        if (type < 1 || type > 0x12) return 0;
        w = *(const BYTE *const *)(t + 0x408);
        if (!w || *(const short *)(w + 0x20) != type) return 0;  // not an enemy actor's own type (RE_thermal 1.1)
        if (type_) *type_ = type;
        if (g_thermGhost[type]) {
            if (note) ThermNote(2, type, 0);
            return 2;
        }
        if (note) ThermNote(g_thermCold[type] ? 0 : 1, type, 0);
        return g_thermCold[type] ? 0 : 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static int ThermClass(const BYTE *t) { return ThermClassify(t, 1, NULL, NULL); }
static int __cdecl GvLive(void);
static void GvCollect(const BYTE *task, BYTE *model);
static void GvFrameBegin(void);  // v3.24.0
static void SvSoulAdd(const BYTE *task, BYTE *model, int type, float heat, float age);  // v3.28 FIRE: age
static void SvNpcNote(int which, DWORD fn);
// v3.17.1 (user: Cynthia seen behind the train door stayed cold, "considered an object scenery piece"): a model that is
// not an enemy actor but has a full skeleton (people, creatures; props have few bones) is alive: hot.
static int ThermLiving(const BYTE *task, const BYTE *model)
{
    static DWORD s_seen[16];
    static int s_n;
    int nb = 0, i;
    DWORD fn = 0;
    __try {
        const BYTE *skel = *(const BYTE *const *)(model + 0xE0);
        if (skel) nb = *(const int *)(skel + 0x14);
        fn = *(const DWORD *)(task + 0x404);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (nb < THERM_LIVING_BONES || nb > 200) return 0;
    for (i = 0; i < s_n && s_seen[i] != fn; ++i) {}
    if (i == s_n && s_n < 16) {
        s_seen[s_n++] = fn;
        Log("screen effects: Thermal: a %d-bone model (task update %08lX) drawn hot as living", nb, fn);
    }
    return 1;
}
// ---- v3.21.0 (user items 4 / 5): dead enemies cool down (Thermal) and their soul fire dies (Spirit Vision) slowly.
// Dead = the game's own EnemyIsDead 0x45BB90(task) (cdecl; 1 for state 11, or state 10 with HP <= 0, or a task 0x52B1C0
// no longer accepts). One table for both looks, keyed by task + work (pool slots are reused); an entry is dropped as
// soon as its actor is seen alive (V321-002), when unseen for 5 s, and on a room change. The clock runs while enemies
// are drawn; a gap counts at most 0.5 s (V321-007 / 009: real time at slow frame rates; a pause or load adds <= 0.5 s).
typedef struct { const BYTE *task, *work; double tDeath, tSeen; } DfEnt;
static DfEnt g_df[32];
static int g_dfOk = -1;                 // -1 not checked, 0 EnemyIsDead not stock (no fades: heat 1), 1 ok
static DWORD g_dfRoom = 0xFFFFFFFFu;
static double g_dfClock;
static LARGE_INTEGER g_dfLast;
static float g_thermCoolS = 30.0f, g_soulFadeS = 6.0f; // v3.28: default 6 (was 10: the new going-out sequence is longer)  // [ScreenEffects] ThermalCoolSeconds / SoulFadeSeconds
static float g_thermHeat = 1.0f;        // the heat of the model 0x52AEB5 draws hot right now (hkThermAmbient)
static float g_dfAge = -1.0f;           // v3.28 FIRE: DfHeat's last answer as seconds since death (< 0 alive / unknown)
static float DfHeat(const BYTE *task, float T)
{
    static const BYTE kStock[] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x56, 0x33, 0xFF, 0xE8 };
    LARGE_INTEGER now, f;
    const BYTE *work = NULL;
    DWORD room;
    int dead = 0, i, k = -1, freeI = -1, oldI = 0;
    double t;
    g_dfAge = -1.0f;  // v3.28 FIRE
    if (g_dfOk < 0) {
        g_dfOk = memcmp((const void *)0x45BB90, kStock, sizeof kStock) == 0;
        Log("screen effects: death fades %s (EnemyIsDead 0045BB90%s); Thermal cools in %.0f s, the soul fire dies in %.0f s",
            g_dfOk ? "on" : "off", g_dfOk ? "" : " not stock", g_thermCoolS, g_soulFadeS);
    }
    if (!g_dfOk || !task) return 1.0f;
    QueryPerformanceCounter(&now);
    if (g_dfLast.QuadPart && QueryPerformanceFrequency(&f) && f.QuadPart) {
        const double dt = (double)(now.QuadPart - g_dfLast.QuadPart) / (double)f.QuadPart;
        if (dt > 0.0) g_dfClock += dt < 0.5 ? dt : 0.5;  // V321-007 / 009: real time down to 2 fps; a pause or load (nothing drawn) adds <= 0.5 s
    }
    g_dfLast = now;
    t = g_dfClock;
    room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (room != g_dfRoom) {
        g_dfRoom = room;
        memset(g_df, 0, sizeof g_df);
    }
    __try {
        work = *(const BYTE *const *)(task + 0x408);
        dead = ((int(__cdecl *)(const BYTE *))0x45BB90)(task) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1.0f;
    }
    for (i = 0; i < 32; ++i) {
        if (g_df[i].task == task && g_df[i].work == work) k = i;
        else if (g_df[i].task && t - g_df[i].tSeen > 5.0) g_df[i].task = NULL;
        if (!g_df[i].task && freeI < 0) freeI = i;
        if (g_df[i].tSeen < g_df[oldI].tSeen) oldI = i;
    }
    if (!dead) {
        if (k >= 0) g_df[k].task = NULL;  // alive (again): no death time
        return 1.0f;
    }
    if (k < 0) {
        k = freeI >= 0 ? freeI : oldI;
        g_df[k].task = task;
        g_df[k].work = work;
        g_df[k].tDeath = t;
    }
    g_df[k].tSeen = t;
    g_dfAge = (float)(t - g_df[k].tDeath);  // v3.28 FIRE
    if (T <= 0.0f) return 0.0f;  // V321-001: instant
    return 1.0f - FxSmooth(0.0f, T, (float)(t - g_df[k].tDeath));
}
static void __cdecl ThermAmbC(void)  // after 0x428020 wrote the model ambient, for a hot draw
{
    volatile float *a = (volatile float *)0xF4E6D0;
    const float h = g_thermHeat;
    int i;
    for (i = 0; i < 3; ++i) a[i] = h >= 1.0f ? g_thermAmb : a[i] + (g_thermAmb - a[i]) * h;  // to the model's own ambient
}
static void TpEnemySeen(const BYTE *task, const BYTE *model, int type);
static void ThermPrintsEarly(void);
static int __cdecl ThermDrawC(const BYTE *task, BYTE *model)  // 0x52AEB5: 0 = not drawn
{
    static LONG s_printFrame = -1;
    int cls, type = 0;
    DpCollect(task, model);  // v3.27.0: Death Presence finds its victims' bones (before any filter early-out)
    AiCollect(task, model);  // v3.29 AFTERIMAGE: her frame and the shocked bodies (before any filter early-out)
    EchoCollect(task, model);  // v3.30 ECHO: the Echo double (her body drawn this frame, the 3D viewport)
    DfxCollect(task, model);  // v3.31 DEPTH331: the player / companions, for the depth restore (depthfix.inc)
    PvCollect(task, model);  // v3.56 PV356: a pause's capture frame: the bodies / ghosts the Effects page's preview needs (preview.inc)
    if (GvLive()) {  // Ghost Vision (exclusive with Thermal): the ghost is drawn as usual and remembered for the glow
        GvCollect(task, model);
        return 1;
    }
    if (!ThermHeatLive()) return 1;
    if (!ThHeatOk() && ThermalLive() && s_printFrame != g_fxFrame) {  // fallback look only: the v3.17.1 footprints
        s_printFrame = g_fxFrame;                                      // before the first character (v3.22.0: they are
        ThermPrintsEarly();                                            // drawn after the world palette, ThHeatPass)
    }
    cls = ThermClassify(task, 1, NULL, &type);
    if (cls == 2) return ThermalLive() ? 0 : 1;  // ghosts: hidden in gameplay only (FXV2-004)
    if (cls == 0 && type == 0 && model && ThermLiving(task, model)) cls = 1;  // not an enemy actor (ThermalCold stays)
    g_thermHeat = 1.0f;
    if (cls == 1 && type > 0) {  // v3.21.0: an enemy cools after death
        g_thermHeat = DfHeat(task, g_thermCoolS);
        if (g_thermHeat <= 0.0f) cls = 0;
    }
    if (cls == 1) g_thermHeat *= DpHeatK(task);  // v3.27.1 (user item 3): Death Presence victims cool with their stain
    if (cls == 1) {
        if (ThHeatOk()) ThAdd(task, model, g_thermHeat);  // v3.22.0: redrawn in heat colours after the world palette
        else if (ThermalLive()) InterlockedExchange(&g_thermHot, 1);  // fallback: the ambient boost (phase 0, as v3.21)
    }
    if (type > 0 && model) TpEnemySeen(task, model, type);  // v3.17.1: walking enemies leave footprints too
    return 1;
}
static int FiDrawC(const BYTE *task, BYTE *model);  // v3.50 FI350: fadein.inc (1 = drawn see-through there)
static int FiShadowC(const BYTE *task);             // v3.50 FI350: fadein.inc (0 = no shadow yet)
// v3.50 FI350: Fade In: where the stock draw would run, an enemy that has just appeared is drawn see-through by the
// game's own 0x45C880 (fadein.inc FiDrawC) and the stock call is skipped
static int __cdecl ThermDrawFi(const BYTE *task, BYTE *model)
{
    const int r = ThermDrawC(task, model);
    if (r && FiDrawC(task, model)) {
        InterlockedExchange(&g_thermHot, 0);  // (the stock path clears it after its draw)
        return 0;
    }
    return r;
}
static int __cdecl ThermShadowC(const BYTE *task) { return (!ThermalLive() || ThermClass(task) != 2) && FiShadowC(task) && GtShadowC(task); }  // v3.50 FI350: + FiShadowC  // v3.58 GT358: + GtShadowC
// v3.21.0 (user item 6): the trapped Cynthia behind the Subway train door is an event actor (record 0x62A6FC, model
// cynl.bin 29 bones / cybl.bin 30): its late handler 0x4EE360 draws her with its own `call 55C650` at 0x4EE376, in
// 0x52B380 before the registry model loop, so 0x52AEB5 never saw her (staging/feedback_v321/cynthia_re.md). Only that
// record uses 0x4EE360, so the model is always her: hot while Thermal is live. (Footprints come later at 0x52AEB5.)
// v3.22.0 (user item 12: she was still cold; no "drawn hot" line in the user's log): heat in phase 0 or 1 (her lines
// play through in-world scenes), redrawn with the bodies (model-only entry); the first entry logs the gate inputs.
// Returns 1 = the v3.21 ambient draw (fallback look), 0 = the plain stock draw.
static int __cdecl ThermCynC(BYTE *model)
{
    static int s_logged, s_seen;
    if (!s_seen++)
        Log("screen effects: Thermal: the trapped Cynthia's draw 004EE376 runs (filter %d, mode %d, sub-mode %d, phase %d, "
            "overlay %d; heat %s)", (int)g_fx[FX_FILTER], *(volatile int *)0x1236D4C, *(volatile int *)0x1236D50,
            *(volatile int *)0xFD5C20, *(volatile int *)0x107BF54, ThermHeatLive() ? "on" : "off");
    PvCyn(model);  // v3.56 PV356: a pause's capture frame: her heat / soul for the Effects page's preview (preview.inc)
    if (GvLive()) {  // v3.24.0 (user item 2): Spirit Vision: her soul fire (the plain draw as before)
        GvFrameBegin();
        SvNpcNote(3, 0x4EE360);
        SvSoulAdd(NULL, model, SV_SOUL_NPC, 1.0f, -1.0f);  // v3.28 FIRE: alive
        return 0;
    }
    if (!ThermHeatLive()) return 0;
    if (!s_logged++) Log("screen effects: Thermal: the trapped Cynthia (event 0062A6FC, draw 004EE376) drawn hot (%s)",
                         ThHeatOk() ? "heat redraw" : "ambient fallback");
    if (ThHeatOk()) {
        ThAdd(NULL, model, 1.0f);
        return 0;
    }
    return 1;
}
static __declspec(naked) void hkThermCyn(void)  // 4EE376: was `call 55C650` ([esp+4] model = esi)
{
    __asm {
        pushad
        push dword ptr [esp + 24h]  // the model (above the 8 saved registers and our return address)
        call ThermCynC
        add esp, 4
        mov [esp + 1Ch], eax
        popad
        test eax, eax
        jz plain
        mov dword ptr [g_thermHeat], 3F800000h  // review V321-005: full heat for her draw (ThermAmbC reads it)
        mov dword ptr [g_thermHot], 1
        push dword ptr [esp + 4]
        call dword ptr [g_fnModel]
        add esp, 4
        mov dword ptr [g_thermHot], 0
        ret
    plain:
        jmp dword ptr [g_fnModel]
    }
}
static __declspec(naked) void hkThermDraw(void)  // 52AEB5: was `call 55C650` ([esp+4] model; edi task, esi record)
{
    __asm {
        pushad
        push dword ptr [esp + 24h]  // the model (above the 8 saved registers and our return address)
        push edi
        call ThermDrawFi  // v3.50 FI350 (was ThermDrawC: it calls it)
        add esp, 8
        mov [esp + 1Ch], eax     // the result into the saved eax
        popad
        test eax, eax
        jz skip
        push dword ptr [esp + 4] // the model
        call dword ptr [g_fnModel]
        add esp, 4
        mov dword ptr [g_thermHot], 0
    skip:
        ret
    }
}
static __declspec(naked) void hkThermShadow(void)  // 52AF95: was `call 55C860` ([esp+4] model; edi = the slot, [edi] task)
{
    __asm {
        pushad
        push dword ptr [edi]
        call ThermShadowC
        add esp, 4
        mov [esp + 1Ch], eax
        popad
        test eax, eax
        jz skip
        jmp dword ptr [g_fnShadow]
    skip:
        ret
    }
}
static __declspec(naked) void hkThermAmbient(void)  // 4273A6: was `call 428020` (model ambient -> [F4E6D0..DC])
{
    __asm {
        mov eax, 428020h
        call eax
        cmp dword ptr [g_thermHot], 0
        je done
        pushad
        call ThermAmbC           // v3.21.0: the hot ambient, faded by the death heat
        popad
    done:
        ret
    }
}
static int __cdecl JasperFlamesOff(void)
{
    static int s_logged;
    if (!ThermalLive() || !g_thermGhost[0x0D]) return 0;
    if (!s_logged++) Log("screen effects: Thermal: Jasper's flame emitters skipped (live flames end on their own)");
    return 1;
}
static __declspec(naked) void hkJasperFlames(void)  // 47DFC3: was `call 47DEC0` (eax = the task)
{
    __asm {
        push eax
        call JasperFlamesOff
        test eax, eax
        pop eax
        jnz skip
        jmp dword ptr [g_fnJasper]
    skip:
        ret
    }
}

// ---------------------------------------------------------------- Ghost Vision (PLAN section 7, RE_ghostvision.md)
// World: edge passes (GvPass). Ghosts: collected at 0x52AEB5 (hkThermDraw; Thermal's classification, ini lists
// included), drawn again after the edge passes with 0x55C650 while the draw-call override forces Z off, additive
// ONE / ONE and the glow shader (GvOverride); without the override group: additive sprites at the same entries (GV-003).
// Auras: the game's snapshot of this frame's haunting-damage sources (0x10A1EC0 + 0x20 i, count [0x10A22C0]; read only)
// as rings on the player's floor. Ghost parts: gameplay phase 0 only; cutscenes / examine frames: the edge world only.
typedef HRESULT(STDMETHODCALLTYPE *GvGetIdx_t)(void *, void **, UINT *);
typedef HRESULT(STDMETHODCALLTYPE *GvSetIdx_t)(void *, void *, UINT);
enum { FXV_SETIDX = 85, FXV_GETIDX = 86 };
#define GV_NEAR_W 10.0f  // rings / sprites: clipped where the projected w (depth) is below this

static int GvEdgesOk(void)
{
    return !g_fxPsFailed[FXT_GVLIFT] && !g_fxPsFailed[FXT_GVEDGE] && !g_fxPsFailed[FXT_GVGRADE];
}
// the ghost parts (collection, glow, rings): gameplay phase 0 (FXV2-004, as Thermal)
static int GvPhase0(void) { return g_fxOn && g_spInstalled && FxGameplay() && *(volatile int *)0xFD5C20 == 0; }
static int __cdecl GvLive(void)
{
    return g_thermHooks && g_fx[FX_FILTER] == FXF_GHOST && GvEdgesOk() && GvPhase0();
}
// v3.24.0 (user item 3): the Room 302 peephole view (a real-time cut-scene, phase 1, while the peeping flag [0xFD5BE8],
// set / cleared only by the Room 302 look code 0x4D6BC6..0x4D93A1, is on) shows Eileen's soul fire, and nothing else of
// the gameplay layer (cut-scenes stay the edge world only, FXV2-004)
static int g_peepHook;
static int GvPeep(void)
{
    return g_peepHook && g_fx[FX_FILTER] == FXF_GHOST && GvEdgesOk() && g_fxOn && g_spInstalled && FxGameplay() &&
           *(volatile int *)0xFD5C20 == 1 && *(volatile int *)0xFD5BE8 != 0 && FxSceneGate();
}
// 0x52AF34 (the scene-character loop 0x52AEF0: `push eax; call 55C650`, eax model, esi task, edi record): the peephole
// Eileen is a scene character ([model + 0xF0] char id 0x133 eink or 0x12E eisk, written at 0x4F1FEF)
static void __cdecl PeepDrawC(const BYTE *task, BYTE *model)
{
    DWORD id = 0;
    if (!model || !GvPeep()) return;
    __try {
        id = *(const DWORD *)(model + 0xF0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (id != 0x133 && id != 0x12E) return;
    GvFrameBegin();
    SvNpcNote(4, id);
    SvSoulAdd(task, model, SV_SOUL_NPC, 1.0f, -1.0f);  // v3.28 FIRE: alive
}
static __declspec(naked) void hkPeepDraw(void)  // 52AF34: was `call 55C650` ([esp+4] model; esi task)
{
    __asm {
        pushad
        push dword ptr [esp + 24h]  // the model (above the 8 saved registers and our return address)
        push esi
        call PeepDrawC
        add esp, 8
        popad
        jmp dword ptr [g_fnModel]   // the stock draw, with the caller's stack
    }
}
// G2: a frame Ghost Vision draws over gets no grain / dust and no Noise Type layer (0x432FBE / 0x432F9E run at
// 0x4EFBF8, before the scene pass; the edge pass would light every grain up)
static int GvNoiseOff(void)
{
    static int s_logged;
    if (!g_fxOn || !g_spInstalled || g_fx[FX_FILTER] != FXF_GHOST || !GvEdgesOk()) return 0;
    if (!FxSceneGate() && !FxCaptureGate()) return 0;
    if (!s_logged++) Log("screen effects: Spirit Vision: the game's grain / dust and the Noise Type layers are skipped while it draws");
    return 1;
}
static void GvNote(int kind, int type, DWORD fn)
{
    static BYTE seenType[0x13], seenKind[3];
    if (g_pvColl) return;  // v3.56 PV356: the preview's own collection (Spirit Vision is not live): not "collected (glows)"
    if (kind == GVK_ACTOR) {
        if (type < 1 || type > 0x12 || seenType[type]) return;
        seenType[type] = 1;
        Log("screen effects: Spirit Vision: enemy type %02X collected (glows)", type);
        return;
    }
    if (seenKind[kind]) return;
    seenKind[kind] = 1;
    Log("screen effects: Spirit Vision: %s %08lX collected (glows)",
        kind == GVK_WALL ? "a wall ghost (update)" : "a Room 302 haunting object (pre-draw 005473A0), first", fn);
}
// v3.24.0: the per-frame reset of the ghost / soul lists, for every collector of the frame (the trapped Cynthia's draw
// 0x4EE376 runs before the registry loop 0x52AEB5: a reset inside the first GvCollect would drop her soul)
static void GvFrameBegin(void)
{
    const LONG now = g_fxFrame;
    if (g_gvStamp != now) {  // the first collector of a frame: new lists, and the 3D viewport in force
        void *dev = DEV;
        g_gvStamp = now;
        g_gvN = 0;
        g_svSoulN = 0;
        g_gvVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_gvVp)) && g_gvVp.w >= 16 && g_gvVp.h >= 16;
    }
}
static void SvSoulAdd(const BYTE *task, BYTE *model, int type, float heat, float age)
{
    int i;
    if (!model || heat <= 0.0f || g_svSoulN >= SV_SOULS) return;
    for (i = 0; i < g_svSoulN; ++i)
        if (g_svSoul[i].model == model) return;
    g_svSoul[g_svSoulN].task = task;
    g_svSoul[g_svSoulN].model = model;
    g_svSoul[g_svSoulN].type = (short)type;
    g_svSoul[g_svSoulN].heat = heat;
    g_svSoul[g_svSoulN].age = age;  // v3.28 FIRE: the going-out timeline (spiritfire.inc)
    ++g_svSoulN;
}
static void SvNpcNote(int which, DWORD fn)  // once a session per kind of new soul source (live check)
{
    static const char *const kWhat[5] = { "escort Eileen (companion)", "Cynthia (companion)", "a living person (full skeleton)",
                                          "the trapped Cynthia (0x4EE376)", "Eileen through the Room 302 peephole" };
    static int s_seen;
    if (which < 0 || which > 4 || (s_seen >> which & 1)) return;
    s_seen |= 1 << which;
    Log("screen effects: Spirit Vision: a soul fire for %s (task fn %08lX)", kWhat[which], fn);
}
// 0x52AEB5, the stock draw still runs (the ghost's visible part becomes edge lines like the world)
static void GvCollect(const BYTE *task, BYTE *model)
{
    int kind, type, i;
    GvFrameBegin();
    if (!model) return;
    if (ThermClassify(task, 0, &kind, &type) != 2) {  // v3.17.0 S8: a living enemy (type 4, the hanging figure, skipped)
        if (type == 0 && model == *(BYTE *const volatile *)(0x10A0F20 + 0x1B0)) type = SV_SOUL_PLAYER;  // v3.22.0 (item 7)
        if (type == 0 && task) {  // v3.24.0 (user items 2 / 3): living people have a soul too
            const BYTE *const eil = *(const BYTE *const volatile *)0xFCE6B4, *const cyn = *(const BYTE *const volatile *)0xFD5A50;
            const int which = task == eil ? 0 : task == cyn ? 1 : ThermLiving(task, model) ? 2 : -1;
            if (which >= 0) {
                DWORD fn = 0;
                __try {
                    fn = *(const DWORD *)(task + 0x404);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
                SvNpcNote(which, fn);
                type = SV_SOUL_NPC;
            }
        }
        if ((type >= 1 && type <= 0x12 && type != 4 && !g_svNoSoul[type]) || type == SV_SOUL_PLAYER || type == SV_SOUL_NPC)
        {   // v3.21.0: dies slowly; v3.28 FIRE: with its age since death (the fire goes out on spiritfire.inc's timeline)
            const float heat = type >= SV_SOUL_PLAYER ? 1.0f : DfHeat(task, g_soulFadeS);
            SvSoulAdd(task, model, type, heat, type >= SV_SOUL_PLAYER ? -1.0f : g_dfAge);
        }
        return;
    }
    if (g_gvN >= GV_MAX) return;
    __try {
        const BYTE *rec = *(const BYTE *const *)(task + 0x41C);
        GvEnt *e;
        if (rec && *(const DWORD *)(rec + 0x24) == 0x45C860) return;  // Cynthia's depth-only sub-actor (RE_thermal 4)
        for (i = 0; i < g_gvN; ++i)
            if (g_gv[i].model == model) return;
        e = &g_gv[g_gvN];
        e->task = task;
        e->rec = rec;
        e->model = model;
        e->recModel = rec ? *(BYTE *const *)(rec + 0x14) : NULL;
        e->fn = *(const DWORD *)(task + 0x404);
        e->type = (short)type;
        e->kind = (BYTE)kind;
        ++g_gvN;
        GvNote(kind, type, e->fn);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
static int GvValid(const GvEnt *e)  // still the same live task, record and model as at 0x52AEB5
{
    __try {
        return e->fn && *(const DWORD *)(e->task + 0x404) == e->fn && *(const BYTE *const *)(e->task + 0x41C) == e->rec &&
               (!e->rec || *(BYTE *const *)(e->rec + 0x14) == e->recModel);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// The override, at each of the renderer's two draw calls while g_gvGlow (after every state the game set for it): Z test
// and write off, additive ONE / ONE, fog / stencil off, RGB writes, the glow shader. Alpha test, cull, textures, the
// vertex shader and its constants stay the game's (cut-out hair keeps its shape).
static int g_thOpaque, g_thZLogged;  // v3.23.0: the body being redrawn is alive (no blending); the Z states logged
static DWORD g_thZSeen[2];
static int g_thBiasOn;              // v3.24.0: the heat redraw's depth bias (c9, the view-projection's z row, x 1 + TH_ZBIAS)
static float g_thBiasOut[4];        // the c9 we wrote last (the game's own upload differs: biased again)
static float g_thZBias = TH_ZBIAS;  // v3.24.0: [ScreenEffects] ThermalDepthBias / 1000
static float g_thBiasOut12[4];      // v3.29.1: the c12 we wrote last (skinned characters' clip z row)
static int g_thRedrawZ = 1;         // v3.29.1: hidden [ScreenEffects] ThermalRedrawDepth=0: a test, the redraw ignores depth
static LONG g_thDiagN[3];           // v3.29.1: this redraw's draw calls: skinned (z row c12), rigid / other (c9), Get failed
static int g_thDiagOn;              // v3.29.1: count them (the once-per-room log line)
// v3.29.1: the skinned-character vertex shader handles (re_depth.md 2.2: `m4x4 r4, r0, c10`), as 0x4050D0 stored them:
// runs of {handle, decl, func, usage} (stride 0x10) at these addresses
static int ThSkinnedVs(DWORD h)
{
    static const DWORD kRun[5][2] = { { 0x5EBBF0, 4 }, { 0x5F0640, 20 }, { 0x5F5410, 20 }, { 0x5FA1E0, 20 }, { 0x5FF0A0, 20 } };
    int r, i;
    if (!h) return 0;
    __try {
        for (r = 0; r < 5; ++r)
            for (i = 0; i < (int)kRun[r][1]; ++i)
                if (*(const DWORD *)(ULONG_PTR)(kRun[r][0] + 0x10 * i) == h) return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return 0;
}
static void __cdecl GvOverride(void)
{
    static const DWORD kRs[][2] = { { 7, 0 } /*ZENABLE*/, { 14, 0 } /*ZWRITEENABLE*/, { 27, 1 } /*ALPHABLENDENABLE*/,
                                    { 19, 2 } /*SRCBLEND ONE*/, { 20, 2 } /*DESTBLEND ONE*/, { 171, 1 } /*BLENDOP ADD*/,
                                    { 28, 0 } /*FOGENABLE*/, { 52, 0 } /*STENCILENABLE*/, { 168, 7 } /*COLORWRITEENABLE*/ };
    // v3.22.0 heat mode (Thermal): the depth test stays the game's, relaxed to its -EQUAL form so the redraw lands on the
    // visible pixels of the first draw (same vertex shader, same transforms); no depth write; SRCALPHA / INVSRCALPHA
    static const DWORD kHeatRs[][2] = { { 14, 0 } /*ZWRITEENABLE*/, { 27, 1 } /*ALPHABLENDENABLE*/, { 19, 5 } /*SRCALPHA*/,
                                        { 20, 6 } /*INVSRCALPHA*/, { 171, 1 } /*BLENDOP ADD*/, { 28, 0 } /*FOGENABLE*/,
                                        { 52, 0 } /*STENCILENABLE*/, { 168, 7 } /*COLORWRITEENABLE*/ };
    void *dev = DEV;
    int i;
    if (!dev) return;
    if (g_fxDipOvr) {  // v3.30 ECHO: the Echo double's passes (echo.inc EchoOverride)
        g_fxDipOvr(dev);
        return;
    }
    if (g_thMode) {
        DWORD zf = 0, ze = 0;
        if (!g_thPs) return;
        if (!g_thZLogged) {  // v3.23.0: what the game left at the call (diagnostic, once a session)
            g_thZLogged = 1;
            g_thZSeen[0] = SUCCEEDED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 7 /*ZENABLE*/, &ze)) ? ze : 0xFFFFFFFFu;
            g_thZSeen[1] = SUCCEEDED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 23 /*ZFUNC*/, &zf)) ? zf : 0xFFFFFFFFu;
        }
        for (i = 0; i < (int)(sizeof kHeatRs / sizeof kHeatRs[0]); ++i)
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kHeatRs[i][0], kHeatRs[i][1]);
        // v3.23.0 (user items 7 / 8: bodies seen through each other and through walls): the redraw runs in the 2D scene
        // slot, whose states (FxReapply) have Z off, and the model draw does not turn it on: depth test on, reversed Z
        // GREATEREQUAL (the characters' own function, KB 113), so the redraw lands only on its own visible pixels
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 7 /*ZENABLE*/, 1);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 23 /*ZFUNC*/, 7 /*GREATEREQUAL*/);
        if (g_thBiasOn) {  // v3.24.0: c7..c10 = transpose(0xED8510) (uploaded by 0x424090 from the camera setup); c9 = the
            float c9[4];   // clip z row: x (1 + TH_ZBIAS) = nearer in the reversed depth; only a fresh game value is scaled
            int j;
            if (SUCCEEDED(((FxPsc_t)VT(dev, FXV_GETVSC))(dev, 9, c9, 1)) && memcmp(c9, g_thBiasOut, sizeof c9)) {
                for (j = 0; j < 4; ++j) g_thBiasOut[j] = c9[j] * (1.0f + g_thZBias);
                ((FxPsc_t)VT(dev, FXV_SETVSC))(dev, 9, g_thBiasOut, 1);
            }
            // v3.29.1: skinned characters (every living body) take clip z from c12 (c10..c13 = transpose(0xF4E760),
            // uploaded per model by 0x427250; the bone palette starts at c34). Scaling both rows is safe in either
            // family: rigid shaders read c12 only as a light direction, skinned ones c9 only as a colour add, and the
            // heat shader reads neither (no v0)
            if (SUCCEEDED(((FxPsc_t)VT(dev, FXV_GETVSC))(dev, 12, c9, 1)) && memcmp(c9, g_thBiasOut12, sizeof c9)) {
                for (j = 0; j < 4; ++j) g_thBiasOut12[j] = c9[j] * (1.0f + g_thZBias);
                ((FxPsc_t)VT(dev, FXV_SETVSC))(dev, 12, g_thBiasOut12, 1);
            }
        }
        if (g_thDiagOn) {  // v3.29.1: which family the redraw's draw calls use (the once-per-room log line)
            DWORD vs = 0;
            if (FAILED(((SpGetDW_t)VT(dev, SPV_GETVS))(dev, &vs))) ++g_thDiagN[2];
            else ++g_thDiagN[ThSkinnedVs(vs) ? 0 : 1];
        }
        if (!g_thRedrawZ) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 23 /*ZFUNC*/, 8 /*ALWAYS*/);  // v3.29.1: test only
        if (g_thOpaque) {  // a living body: solid; v3.24.0 (user item 1): and it writes its depth, so its near faces win
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);  // over its far ones even where the stock
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 14 /*ZWRITEENABLE*/, 1);  // draw left no depth (the close-camera fade)
        }   // v3.58 PV358: also the preview's heat layer again (v3.56's g_pvNoZw showed the faded body's inside faces)
        if (g_thGtPass == 1) {  // v3.58 GT358: Ghost Teleport's faded heat body: its nearest surface's depth first (no colour)
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 0);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 14 /*ZWRITEENABLE*/, 1);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 15 /*ALPHATESTENABLE*/, 1);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 25 /*ALPHAFUNC*/, 5 /*GREATER*/);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 24 /*ALPHAREF*/, 127 /*teleport_sim.h GT_PRE_REF*/);
        }
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_thPs);
        ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, g_thC, 6);
        return;
    }
    if (!g_gvGlowPs) return;
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i][0], kRs[i][1]);
    if (g_aiZ) {  // v3.29 AFTERIMAGE: the echoes / shocked bodies are hidden by what is in front (reversed Z), no z write
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 7 /*ZENABLE*/, 1);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 23 /*ZFUNC*/, 7 /*GREATEREQUAL*/);
    }
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_gvGlowPs);
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, g_gvGlowC, (DWORD)g_gvGlowCN);
}
// 0x4292EF `call [ecx+124h]` (DrawIndexedPrimitiveUP) and 0x429338 `call [ecx+11Ch]` (DrawIndexedPrimitive), each now
// `call stub; nop`: eax = the device, ecx = its vtable, the arguments on the stack as for the method, which returns to
// the nop (site + 5) as it returned to site + 6 before.
static __declspec(naked) void hkGvDipUp(void)
{
    __asm {
        cmp dword ptr [g_gvGlow], 0
        jne ovr
        cmp dword ptr [g_thMode], 0  // v3.22.0: Thermal's heat redraw
        jne ovr
        cmp dword ptr [g_fxDipOvr], 0  // v3.30 ECHO: the Echo double's redraw
        je go
    ovr:
        pushad
        pushfd
        call GvOverride
        popfd
        popad
    go:
        jmp dword ptr [ecx + 124h]
    }
}
static __declspec(naked) void hkGvDip(void)
{
    __asm {
        cmp dword ptr [g_gvGlow], 0
        jne ovr
        cmp dword ptr [g_thMode], 0  // v3.22.0: Thermal's heat redraw
        jne ovr
        cmp dword ptr [g_fxDipOvr], 0  // v3.30 ECHO: the Echo double's redraw
        je go
    ovr:
        pushad
        pushfd
        call GvOverride
        popfd
        popad
    go:
        jmp dword ptr [ecx + 11Ch]
    }
}

// the game's render / stage states as captured (before our base), for the redraw: its model draws expect them
static void FxReapply(void *dev, const FxSaved *s)
{
    int i;
    for (i = 0; i < FX_NTSS; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kFxTss[i][0], kFxTss[i][1], s->tss[i]);
    for (i = 0; i < FX_NRS; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kFxRs[i], s->rs[i]);
}
static DWORD g_gvFaultCode;
static void *g_gvFaultAt;
static void GvFaultNote(EXCEPTION_POINTERS *e)
{
    g_gvFaultCode = e->ExceptionRecord->ExceptionCode;
    g_gvFaultAt = e->ExceptionRecord->ExceptionAddress;
}
// The model glow. 1 = done (drawn, or nothing to draw); 0 = not possible this frame: the caller draws the sprites.
static int GvRedraw(void *dev, FxSaved *sv, int *drawn)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    static const BYTE kTss[][2] = { { 1, 2 }, { 1, 3 }, { 1, 5 }, { 1, 6 } };  // stage 1 colour / alpha arguments
    static int s_logged;
    DWORD rs[4], tss[4];
    void *ib = NULL;
    UINT base = 0;
    int i, bad = 0, blood;
    *drawn = 0;
    if (!g_gvDipHooks || g_gvFaults >= 3) return 0;
    blood = g_gvBloodOn && FxShader(dev, FXT_GVBLOOD);  // v3.26.0: fail closed to the white glow (FxShader logs once)
    if (!blood && !FxShader(dev, FXT_GVGLOW)) return 0;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) bad = 1;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, kTss[i][0], kTss[i][1], &tss[i]))) bad = 1;
    if (FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) {  // AddRef'd on success
        ib = NULL;
        bad = 1;
    }
    if (bad) {
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        if (!s_logged++) Log("screen effects: Spirit Vision: a device state query for the ghost redraw failed: ghosts glow as sprites (logged once)");
        return 0;
    }
    if (blood) {  // v3.26.0 PLAN 2.1: c0 luma x gain x pulse, c1 (pale - deep) / 2, c2 deep x pulse / 2, c3 / c4 the alpha
        const float p = FxPulse(), g = GV_BLOOD_GAIN * p;
        static const float kPale[3] = { GV_BLOOD_PALE_R, GV_BLOOD_PALE_G, GV_BLOOD_PALE_B };
        static const float kDeep[3] = { GV_BLOOD_DEEP_R, GV_BLOOD_DEEP_G, GV_BLOOD_DEEP_B };
        int j;
        g_gvGlowC[0][0] = 0.299f * g, g_gvGlowC[0][1] = 0.587f * g, g_gvGlowC[0][2] = 0.114f * g, g_gvGlowC[0][3] = 0.0f;
        for (j = 0; j < 3; ++j) {
            g_gvGlowC[1][j] = 0.5f * (kPale[j] - kDeep[j]);  // L carries the pulse
            g_gvGlowC[2][j] = 0.5f * kDeep[j] * p;
            g_gvGlowC[3][j] = 1.0f;
            g_gvGlowC[4][j] = 0.0f;
        }
        g_gvGlowC[1][3] = g_gvGlowC[2][3] = g_gvGlowC[3][3] = 0.0f;
        g_gvGlowC[4][3] = 1.0f;
        g_gvGlowCN = 5;
    } else {
        const float p = FxPulse(), a = GV_GLOW_TEX * 0.5f * p, b = GV_GLOW_FLOOR * p;
        g_gvGlowC[0][0] = FxSat(GV_GLOW_R * a);
        g_gvGlowC[0][1] = FxSat(GV_GLOW_G * a);
        g_gvGlowC[0][2] = FxSat(GV_GLOW_B * a);
        g_gvGlowC[0][3] = 0.5f;
        g_gvGlowC[1][0] = FxSat(GV_GLOW_R * b);
        g_gvGlowC[1][1] = FxSat(GV_GLOW_G * b);
        g_gvGlowC[1][2] = FxSat(GV_GLOW_B * b);
        g_gvGlowC[1][3] = 0.0f;
        g_gvGlowCN = 2;
    }
    g_gvGlowPs = g_fxPs[blood ? FXT_GVBLOOD : FXT_GVGLOW];
    FxReapply(dev, sv);  // the game's states (its renderer sets per material only what it changes)
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, g_gvVpOk ? &g_gvVp : &sv->vp);  // the 3D viewport the ghosts were drawn in
    sv->vpSet = 1;
    SmoothApply();  // EntitySmooth (EntityGate, > 30 fps): the glow on the smoothed pose, as the stock draw
    InterlockedExchange(&g_gvGlow, 1);
    for (i = 0; i < g_gvN; ++i) {
        if (!GvValid(&g_gv[i])) continue;
        __try {
            ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(g_gv[i].model);
            ++*drawn;
        } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
            ++g_gvFaults;
            Log("screen effects: Spirit Vision: a ghost redraw faulted (code %08lX at %p)%s", g_gvFaultCode, g_gvFaultAt,
                g_gvFaults >= 3 ? "; the model glow is off for this session (sprites instead)" : "");
            break;
        }
    }
    InterlockedExchange(&g_gvGlow, 0);
    SmoothRestore();
    ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
    if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
    for (i = 0; i < 4; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kTss[i][0], kTss[i][1], tss[i]);
    for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
    return 1;
}

// ---- world -> screen (Item Shine's path: 0x55BF50 matrix, row vector p x M, divide by w, GS -> clip -> pixels)
static void GvXform(const float p[3], float h[4])
{
    int j;
    for (j = 0; j < 4; ++j) h[j] = p[0] * g_gvM[j] + p[1] * g_gvM[4 + j] + p[2] * g_gvM[8 + j] + g_gvM[12 + j];
}
static int GvScreen(const float h[4], const Viewport8 *vp, float *x, float *y)
{
    float gx, gy, cx, cy;
    if (!(h[3] >= GV_NEAR_W * 0.999f)) return 0;
    gx = h[0] / h[3];
    gy = h[1] / h[3];
    if (!_finite(gx) || !_finite(gy)) return 0;
    if (g_haveConsts && g_native.c2[0] != 0.0f && g_native.c2[1] != 0.0f) {
        cx = gx * g_native.c2[0] + g_native.c3[0];
        cy = gy * g_native.c2[1] + g_native.c3[1];
    } else {
        cx = (gx - 2048.0f) / 256.0f;
        cy = -(gy - 2048.0f) / 224.0f;
    }
    *x = (float)vp->x + (cx + 1.0f) * 0.5f * (float)vp->w;
    *y = (float)vp->y + (1.0f - cy) * 0.5f * (float)vp->h;
    return _finite(*x) && _finite(*y);
}
typedef struct { float x, y, z, rhw; DWORD c; } GvVtx;  // FVF 0x44: XYZRHW | DIFFUSE
typedef struct { float h[4], k, rgb[3]; } GvHv;         // a homogeneous point, its strength (and colour: v3.17.0)
// v3.17.0 S3: depth for our XYZRHW triangles (reversed Z, RE_spiritvision 6): z = n / (f - n) (f / w - 1), rhw = 1 / w
static int g_svZ;                 // 1: write z / rhw from w (set per pass); 0: z 0, rhw 1 (Z off)
static float g_svN = 50.0f, g_svF = 40000.0f, g_svMul = 1.0f;  // near / far of this frame; the pass's strength
static int g_svTest;  // hidden [ScreenEffects] SpiritTest: 1 a test column around the player, 2 / 3 depth probes
static float g_svZProbe;  // SpiritTest=2: every z x this (a near-far-plane probe)
// Live 2026-11-15: GvXform's w is NOT the view depth (z from w drew every fragment as visible). Each frame the view
// depth zv is related to w by the game's view matrix (0xF4E720, row vectors): w = A zv + B, fitted on two points and
// checked on a third (SvNearFar); no fit -> depth off for the frame.
static float g_svA = 1.0f, g_svB = 0.0f, g_svPull, g_svScale = SV_ZSCALE;
// v3.27.3 (staging/v3273_depth/re_depth.md): the 3D pass stores exactly P22 + P32 / w (no 1/128 in any shader or in GOG's
// wrapper); Death Presence sets this to 1 (exact) for its own draws and back to SV_ZSCALE after; Spirit Vision keeps 1/128
static float g_svZScaleNow = SV_ZSCALE;
static int g_svFit;
// v3.17.1: the game's own depth for CPU-projected geometry (its decals; Item Shine's projection, items.inc): the GS z
// h[2] / w through the 2D vertex shader's constants c2.z / c3.z (default 2^-24, 0): exact, no calibration. The depth
// buffer is reversed (near large): GREATEREQUAL = visible.
static float SvDepthZ(float w);
static float g_svP[16];  // this frame's projection (0xF4E760 = flipY x proj, row vectors), read by SvNearFar
static int g_svPOk;
static float SvZH(const float h[4])  // live 2026-11-15: the 2D-decal depth (h[2] / w x c2.z) is not the 3D world's
{   // the world's depth: z_ndc = (w P22 + P32) / (w P23 + P33), w = the view depth (GvXform's w, fitted)
    float z;
    if (!(h[3] > 0.0f)) return 0.0f;
    if (g_svPOk) {
        const float d = h[3] * g_svP[11] + g_svP[15];
        // live: the stored depth = this matrix depth x 1/128 (camera 012E3ED0 near 50, far 40000; the factor is applied
        // after the matrix, by the game's shader or GOG's wrapper; probes K_sheet / N*.png)
        z = d != 0.0f ? (h[3] * g_svP[10] + g_svP[14]) / d * g_svZScaleNow : 0.0f;
    } else {
        z = SvDepthZ(h[3]);
    }
    if (g_svZProbe > 0.0f) z *= g_svZProbe;
    return z < 0.0f ? 0.0f : z > 1.0f ? 1.0f : z;
}
static float SvDepthZ(float w)  // v3.17.0's calibrated formula (kept for the SpiritTest probes 2 / 3 only)
{
    // Live sweep 2026-11-15 (SpiritTest=3, staging/spirit_vision/live/K_sheet.png): the stored depth matches this formula
    // with the near plane / 128 (z x 1/64: every fragment visible; x 1/128: under-floor and behind-wall parts hidden,
    // open air visible). SV_ZBIAS keeps our floor rims on the visible side.
    const float zv = (w - g_svB) / g_svA - g_svPull, ne = g_svN * g_svScale;
    float z = zv > 1.0f ? ne / (g_svF - ne) * (g_svF / zv - 1.0f) * SV_ZBIAS : 1.0f;
    if (g_svZProbe > 0.0f) z *= g_svZProbe;
    return z < 0.0f ? 0.0f : z > 1.0f ? 1.0f : z;
}
#define GV_VB 1500
static GvVtx g_gvVb[GV_VB];
static int g_gvVn;
static void GvFlush(void *dev)
{
    if (g_gvVn >= 3) ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(g_gvVn / 3), g_gvVb, sizeof(GvVtx));
    g_gvVn = 0;
}
static DWORD GvColour(const float rgb[3], float k)  // additive: the colour carries the strength
{
    k = FxSat(k);
    return 0xFF000000u | (DWORD)(FxSat(rgb[0] * k) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(rgb[1] * k) * 255.0f + 0.5f) << 8 |
           (DWORD)(FxSat(rgb[2] * k) * 255.0f + 0.5f);
}
// one world triangle, clipped against w >= GV_NEAR_W in homogeneous space (Sutherland-Hodgman, one plane), then drawn
static void GvTri(void *dev, const GvHv *a, const GvHv *b, const GvHv *c, const Viewport8 *vp, const float rgb[3])
{   // rgb NULL: each point's own colour (v3.17.0)
    const GvHv *in[3] = { a, b, c };
    GvHv out[4];
    float sx[4], sy[4];
    int i, j, n = 0;
    if (a->k <= 0.002f && b->k <= 0.002f && c->k <= 0.002f) return;
    for (i = 0; i < 3; ++i) {
        const GvHv *p = in[i], *q = in[(i + 1) % 3];
        const float dp = p->h[3] - GV_NEAR_W, dq = q->h[3] - GV_NEAR_W;
        if (dp >= 0.0f) out[n++] = *p;
        if ((dp >= 0.0f) != (dq >= 0.0f)) {
            const float t = dp / (dp - dq);
            for (j = 0; j < 4; ++j) out[n].h[j] = p->h[j] + (q->h[j] - p->h[j]) * t;
            out[n].k = p->k + (q->k - p->k) * t;
            for (j = 0; j < 3; ++j) out[n].rgb[j] = p->rgb[j] + (q->rgb[j] - p->rgb[j]) * t;
            ++n;
        }
    }
    if (n < 3) return;
    for (i = 0; i < n; ++i)
        if (!GvScreen(out[i].h, vp, &sx[i], &sy[i])) return;
    for (i = 1; i + 1 < n; ++i) {
        const int idx[3] = { 0, i, i + 1 };
        if (g_gvVn + 3 > GV_VB) GvFlush(dev);
        for (j = 0; j < 3; ++j) {
            GvVtx *v = &g_gvVb[g_gvVn++];
            v->x = sx[idx[j]] - 0.5f;
            v->y = sy[idx[j]] - 0.5f;
            v->z = g_svZ ? SvZH(out[idx[j]].h) : 0.0f;
            v->rhw = g_svZ ? 1.0f / out[idx[j]].h[3] : 1.0f;
            v->c = GvColour(rgb ? rgb : out[idx[j]].rgb, out[idx[j]].k * g_svMul);
        }
    }
}
// GV-003 fallback (no draw-call override, the glow shader failed, or a redraw state query failed): an additive glow
// sprite per collected entry (ghosts, wall ghosts, Room 302 haunting objects) at its model root
static int GvSprites(void *dev, const Viewport8 *vp, float pulse)
{
    void *tex = FxTexture(dev, FXX_GLOW);
    const float unit = (float)vp->h / 720.0f;
    const float rgb[3] = { (g_gvBloodOn ? GV_BLOOD_PALE_R : GV_GLOW_R) * GV_SPRITE * pulse,  // v3.26.0: Blood = the pale end
                           (g_gvBloodOn ? GV_BLOOD_PALE_G : GV_GLOW_G) * GV_SPRITE * pulse,
                           (g_gvBloodOn ? GV_BLOOD_PALE_B : GV_GLOW_B) * GV_SPRITE * pulse };
    SpVtx v[6 * GV_MAX];
    int i, m, n = 0;
    if (!tex || !g_gvN) return 0;
    for (i = 0; i < g_gvN; ++i) {
        const GvEnt *e = &g_gv[i];
        // v3.17.0 (RE_spiritvision 4.2): actors at the chest (the old 90 was knee height); the size from 180 as before
        const float lift = e->kind == GVK_ACTOR ? SvChestH(e->type) : 0.0f, tall = e->kind == GVK_ACTOR ? 180.0f : 120.0f;
        float p[3], q[3], h[4], x, y, x2, y2, size;
        DWORD col;
        if (!GvValid(e)) continue;
        __try {
            memcpy(p, e->model + 0x10, sizeof p);  // the model's root node (as EntitySmooth reads it)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
        if (!_finite(p[0] + p[1] + p[2])) continue;
        p[1] -= lift;  // y points down: the chest
        GvXform(p, h);
        if (!GvScreen(h, vp, &x, &y)) continue;
        q[0] = p[0];
        q[1] = p[1] - tall * 0.5f;
        q[2] = p[2];
        GvXform(q, h);
        size = GvScreen(h, vp, &x2, &y2) ? sqrtf((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y)) * 3.2f : 120.0f * unit;
        if (size < 24.0f * unit) size = 24.0f * unit;
        if (size > 700.0f * unit) size = 700.0f * unit;
        col = GvColour(rgb, 1.0f);
        {
            SpVtx qv[4];
            for (m = 0; m < 4; ++m) {
                qv[m].x = x + ((m & 1) ? 0.5f : -0.5f) * size - 0.5f;
                qv[m].y = y + ((m & 2) ? 0.5f : -0.5f) * size - 0.5f;
                qv[m].z = 0.0f;
                qv[m].rhw = 1.0f;
                qv[m].c = col;
                qv[m].u = (m & 1) ? 1.0f : 0.0f;
                qv[m].v = (m & 2) ? 1.0f : 0.0f;
            }
            v[n++] = qv[0];
            v[n++] = qv[1];
            v[n++] = qv[2];
            v[n++] = qv[1];
            v[n++] = qv[3];
            v[n++] = qv[2];
        }
    }
    if (!n) return 0;
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    FxSampler(dev, tex, 0, 0);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    return n / 6;
}

// ---------------------------------------------------------------- v3.17.0 Spirit Vision (staging/spirit_vision/PLAN.md)
// Everything below reads game memory only (guarded) and draws additive XYZRHW geometry in the 0x4EFBFD scene pass.
static float SvSecs(void) { return (float)((double)FxClock() / (double)g_fxQf.QuadPart); }
static void SvMix(float o[3], const float a[3], const float b[3], float t)
{
    int j;
    t = FxSat(t);
    for (j = 0; j < 3; ++j) o[j] = a[j] + (b[j] - a[j]) * t;
}
static float SvEase(float s) { s = FxSat(s); return 1.0f - (1.0f - s) * (1.0f - s); }
enum { SVP_CRIMSON, SVP_GOLD, SVP_BILE };  // ghosts / repelled by the medallion / Eileen's fit (colours: kSvAura)
// S3: 0 Z off (SV_NODEPTH), 1 the hidden part (ZFUNC LESS, dim), 2 the visible part (GREATEREQUAL)
static void SvZ(void *dev, int pass)
{
    // v3.17.1: exact depth (SvZH): no fit needed
    if (!pass) {
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 7 /*ZENABLE*/, 0);
        g_svZ = 0;
        g_svMul = SV_NODEPTH;
        return;
    }
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 7 /*ZENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 14 /*ZWRITEENABLE*/, 0);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 23 /*ZFUNC*/, pass == 1 ? 2 /*LESS*/ : 7 /*GREATEREQUAL*/);
    g_svZ = 1;
    g_svMul = pass == 1 ? SV_HIDDEN : 1.0f;
}
static void SvZOff(void *dev)
{
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 7 /*ZENABLE*/, 0);
    g_svZ = 0;
    g_svMul = 1.0f;
}
static void SvNearFar(void)  // after g_gvM is this frame's: near / far and the w -> view depth fit
{
    static const float kP[3][3] = { { 0.0f, 0.0f, 0.0f }, { 3000.0f, -1500.0f, 2000.0f }, { -2500.0f, 800.0f, -3500.0f } };
    static int s_logged, s_bad;
    const float n = *(volatile float *)0x12E3E7C, f = *(volatile float *)0x12E3E80;
    float V[16], w[3], zv[3], h[4], pred;
    int i;
    if (_finite(n) && _finite(f) && n > 0.01f && f > n * 2.0f) {
        g_svN = n;
        g_svF = f;
    } else {
        g_svN = 50.0f;
        g_svF = 40000.0f;
    }
    g_svFit = 0;
    if (g_svTest == 3) {
        static int s_step = -1;
        const int step = (int)(SvSecs() / 2.0f) % 10;
        if (step != s_step) {
            s_step = step;
            g_svZProbe = 1.0f / (float)(1 << step);
            Log("screen effects: SpiritTest sweep: z x %.5f", g_svZProbe);
        }
    }
    __try {
        static int s_pLogged;
        static const DWORD kSrc[2] = { 0xF4E760, 0x12E3ED0 };  // flipY x proj (per model), the camera's own (cam+0xA0)
        int s;
        g_svPOk = 0;
        for (s = 0; s < 2 && !g_svPOk; ++s) {
            memcpy(g_svP, (const void *)(ULONG_PTR)kSrc[s], sizeof g_svP);
            g_svPOk = _finite(g_svP[10] + g_svP[11] + g_svP[14] + g_svP[15]) && fabsf(g_svP[11]) > 0.5f && g_svP[10] != 0.0f &&
                      g_svP[10] != 1.0f;
            if (g_svPOk && s_pLogged < 2) {
                const float far_ = -g_svP[14] / g_svP[10], near_ = -g_svP[10] * far_ / (1.0f - g_svP[10]);
                s_pLogged = 2;
                Log("screen effects: Spirit Vision: depth from the projection at %08lX: m22 %g m23 %g m32 %g m33 %g (near %g, far %g)",
                    kSrc[s], g_svP[10], g_svP[11], g_svP[14], g_svP[15], near_, far_);
            }
        }
        if (!g_svPOk && !s_pLogged) {
            s_pLogged = 1;
            Log("screen effects: Spirit Vision: no projection matrix found (00F4E760 / 012E3ED0): the calibrated depth is used");
        }
        memcpy(V, (const void *)0xF4E720, sizeof V);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    for (i = 0; i < 3; ++i) {
        GvXform(kP[i], h);
        w[i] = h[3];
        zv[i] = kP[i][0] * V[2] + kP[i][1] * V[6] + kP[i][2] * V[10] + V[14];
        if (!_finite(w[i] + zv[i])) return;
    }
    if (fabsf(zv[1] - zv[0]) < 1.0f) return;
    g_svA = (w[1] - w[0]) / (zv[1] - zv[0]);
    g_svB = w[0] - g_svA * zv[0];
    pred = g_svA * zv[2] + g_svB;
    g_svFit = fabsf(g_svA) > 1e-6f && fabsf(pred - w[2]) <= 0.01f * (fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2])) + 1.0f;
    if (g_svFit && !s_logged++)
        Log("screen effects: Spirit Vision: depth fit w = %.4f zv + %.2f (view matrix 00F4E720; near %.0f, far %.0f)", g_svA, g_svB, g_svN, g_svF);
    if (!g_svFit && s_bad++ < 3)
        Log("screen effects: Spirit Vision: no depth fit this frame (w %.1f vs %.1f predicted): drawn without depth", w[2], pred);
}
static void SvUntextured(void *dev)  // additive vertex colour, as GvRings always drew
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x44);  // FVF XYZRHW | DIFFUSE
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 2 /*ONE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
}
static int SvEye(float eye[3])  // the camera position (Item Shine's 0xFD5AC0 + 48)
{
    __try {
        memcpy(eye, (const void *)(0xFD5AC0 + 48), 12);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return _finite(eye[0] + eye[1] + eye[2]);
}
static float SvRim(const float p[3], const float n[3], const float eye[3])  // 1 - |n . view|
{
    float v[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };
    const float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(l > 1.0f)) return 0.0f;
    return 1.0f - fabsf((v[0] * n[0] + v[1] * n[1] + v[2] * n[2]) / l);
}
static float g_svEye[3];  // this frame's camera position (SvEye), for the floor-decal pull
static void SvPoint(GvHv *o, const float p[3], float k, const float rgb[3])
{
    if (g_svPull > 0.0f) {  // v3.17.1: moved toward the camera along its ray: same screen point, nearer depth
        const float d[3] = { g_svEye[0] - p[0], g_svEye[1] - p[1], g_svEye[2] - p[2] };
        const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > g_svPull * 2.0f) {
            const float q[3] = { p[0] + d[0] * g_svPull / l, p[1] + d[1] * g_svPull / l, p[2] + d[2] * g_svPull / l };
            GvXform(q, o->h);
        } else {
            GvXform(p, o->h);
        }
    } else {
        GvXform(p, o->h);
    }
    o->k = k;
    o->rgb[0] = rgb[0];
    o->rgb[1] = rgb[1];
    o->rgb[2] = rgb[2];
}
static int SvHvOk(const GvHv *p) { return _finite(p->h[0] + p->h[1] + p->h[2] + p->h[3]); }

// ---- S4 tracks: the snapshot's damage sources followed from frame to frame (grow-back, palette)
typedef struct {
    float p[3], R, inner, s;
    LONG seen;
    int pal, active, room302;
    const BYTE *ghost, *work;  // the ghost actor matched at first sight (NULL: a portal, Eileen, a haunting ...)
    DWORD id;                  // v3.21.0: a stable key for the aura's particles (the array is compacted)
    float reach, reachT, kv;   // v3.23.0 (item 17): the veins' reach 0..1 and its target, their strength
    int down;                  // v3.23.0: the matched ghost is down this frame (class 9 / 0x0A)
    float lastCd, cdRunT;      // review SH4-323-03: the countdown last frame, when it was last seen falling
    int cdLogged;              // ... (logged once a track)
} SvTrack;
static SvTrack g_svTr[SV_TRACKS];
static int g_svTrN;
static DWORD g_svTrId;
static float g_svWardT = -100.0f;
static int g_svFitSeen, g_svGold;  // this frame: an Eileen fit entry / a repelled entry (for the log)
static int SvGhostState(const SvTrack *t, float pos[3], float *cd, int *clsOut)  // 1 down (9 / 0x0A), 0 up, -1 gone
{
    __try {
        const BYTE *w;
        BYTE cls;
        if (!t->ghost || *(const BYTE *const *)(t->ghost + 0x408) != t->work || !*(const DWORD *)(t->ghost + 0x404)) return -1;
        w = t->work;
        memcpy(pos, w, 12);
        cls = *(const BYTE *)(w + 0x3C);
        if (clsOut) *clsOut = cls;
        if (cls != 9 && cls != 0x0A) return 0;
        *cd = *(const float *)(*(const BYTE *const *)(w + 0x28) + 0x98);
        if (!_finite(*cd)) *cd = 30.0f;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}
static void SvMatchGhost(SvTrack *t)  // review SV-R1: a 3D match (x / z within 60, y within 150), the nearest wins
{
    float best = 1e30f;
    int i;
    if (g_gvStamp != g_fxFrame) return;
    for (i = 0; i < g_gvN; ++i) {
        const GvEnt *e = &g_gv[i];
        if (e->kind != GVK_ACTOR) continue;
        __try {
            const BYTE *w = *(const BYTE *const *)(e->task + 0x408);
            const float *q = (const float *)w;
            if (w) {
                const float dx = q[0] - t->p[0], dy = q[1] - t->p[1], dz = q[2] - t->p[2], d = dx * dx + dy * dy + dz * dz;
                if (fabsf(dx) < 60.0f && fabsf(dz) < 60.0f && fabsf(dy) < 150.0f && d < best) {
                    best = d;
                    t->ghost = e->task;
                    t->work = w;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}
static void SvTrackStep(float now, float dt)
{
    const BYTE *snap = (const BYTE *)0x10A1EC0;
    const DWORD stage = *(volatile DWORD *)0xFD5A90;
    const int room302 = stage == 1 || stage == 0x0A, medal = *(volatile BYTE *)0x13D7FF4 != 0;
    const float py = *(volatile float *)0x10A0F64;
    static int s_fitLast;
    float ep[3] = { 0, 0, 0 }, best = 1e9f;
    int n = *(volatile int *)0x10A22C0, i, j, fitOk = 0, fitIdx = -1;
    g_svFitSeen = g_svGold = 0;
    if (n < 0) n = 0;
    if (n > 32) n = 32;
    __try {  // S7: companion Eileen in her fit (state 0x13 this or the last frame) and her registration point
        const BYTE *t = *(const BYTE *const *)0xFCE6B4;
        int fit = 0;
        if (t) {
            const BYTE *w = *(const BYTE *const *)(t + 0x408), *m = w ? *(const BYTE *const *)(w + 0x18) : NULL;
            fit = *(const WORD *)(t + 0x410) == 0x13;
            if (m) {
                memcpy(ep, m + 0x10, 12);
                fitOk = (fit || s_fitLast) && _finite(ep[0] + ep[1] + ep[2]);
            }
        }
        s_fitLast = fit;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fitOk = 0;
    }
    for (i = 0; i < n && fitOk; ++i) {  // the closest R 800 entry at her point (review SV-002)
        const float *e = (const float *)(snap + 0x20 * i);
        float dx, dz, d;
        if (fabsf(e[4] - 800.0f) > 0.5f || *(const int *)(e + 6) != 0) continue;
        dx = e[0] - ep[0];
        dz = e[2] - ep[2];
        d = sqrtf(dx * dx + dz * dz);
        if (d < 60.0f && fabsf(e[1] - (ep[1] - 100.0f)) < 40.0f && d < best) {
            best = d;
            fitIdx = i;
        }
    }
    for (j = 0; j < g_svTrN; ++j) g_svTr[j].active = 0;
    for (i = 0; i < n; ++i) {
        const float *e = (const float *)(snap + 0x20 * i);
        const float R = e[4], rate = e[5], in = e[7];
        const int flag = *(const int *)(e + 6);
        SvTrack *t = NULL;
        if (!_finite(e[0] + e[1] + e[2] + R) || !(R > 1.0f) || R > 20000.0f) continue;
        // v3.17.1 (user: "aura columns disappear when the player is below a ghost"): every source is tracked and its
        // aura drawn whatever the height difference (v3.21.0: sprites, SvAuraFrame)
        for (j = 0; j < g_svTrN && !t; ++j) {
            SvTrack *c = &g_svTr[j];
            if (!c->active && fabsf(c->R - R) < 1.0f && fabsf(c->p[0] - e[0]) < 60.0f && fabsf(c->p[2] - e[2]) < 60.0f &&
                fabsf(c->p[1] - e[1]) < 60.0f)
                t = c;
        }
        if (!t) {
            if (g_svTrN >= SV_TRACKS) continue;
            t = &g_svTr[g_svTrN++];
            memset(t, 0, sizeof *t);
            t->id = ++g_svTrId ? g_svTrId : ++g_svTrId;  // never 0
            t->reach = t->reachT = 1.0f;
            memcpy(t->p, e, 12);
            SvMatchGhost(t);
        }
        memcpy(t->p, e, 12);
        t->R = R;
        t->inner = flag == 1 && rate > 0.0f && _finite(in) && in > 0.0f && in < R ? in : 0.0f;  // GV-002
        t->active = 1;
        t->seen = g_fxFrame;
        t->room302 = room302;
        // v3.27.2 (user item 8): Eileen's fit takes the ghosts' crimson palette and veins (was SVP_BILE)
        t->pal = medal && rate == 0.0f ? SVP_GOLD : SVP_CRIMSON;
        if (t->pal == SVP_GOLD) {
            g_svWardT = now;
            g_svGold = 1;
        }
        if (i == fitIdx && t->pal != SVP_GOLD) g_svFitSeen = 1;
        if (!t->ghost) SvMatchGhost(t);
    }
    for (j = 0; j < g_svTrN; ++j) {  // S4: grow / preview / fade, then drop the finished ones
        SvTrack *t = &g_svTr[j];
        float tgt = 0.0f, cd = 0.0f, gp[3], kt;
        // v3.23.0 (user item 17): the down state for every matched ghost, active or not (Cynthia's aura source stays
        // registered while she is down, KB 113): the same downed preview for all, and the veins retract / regrow
        int cls = 0;
        const int down = t->ghost ? SvGhostState(t, gp, &cd, &cls) : -1;
        if (down == 1) {
            // review SH4-323-03: regrow only while the get-up countdown is seen running (class 9, falling since the last
            // frame, within 0.25 s): a pinned ghost (Sword of Obedience) or a stopped / HP-0 state keeps none
            const int fresh = !t->down, running = !fresh && cls == 9 && _finite(cd) && cd < t->lastCd - 1e-4f;
            if (running) t->cdRunT = now;
            if (fresh) t->cdRunT = -100.0f;
            tgt = SV_PREVIEW * FxSat(1.0f - cd / SV_PREVIEW_S);
            if (!t->active) {
                t->p[0] = gp[0];  // follow the lying ghost
                t->p[2] = gp[2];
            }
            t->reachT = cls == 9 && now - t->cdRunT <= 0.25f && _finite(cd) && cd >= 0.0f ? FxSat(1.0f - cd / SV_REGROW_S) : 0.0f;
            if (!t->cdLogged && !fresh) {
                t->cdLogged = 1;
                Log("screen effects: Spirit Vision: a downed ghost (class %02X, countdown %.2f s, %s): its veins retract%s", cls,
                    cd, running ? "running" : "not running", cls == 9 ? " and regrow over its last 2.5 s" : " (no regrow while in this class)");
            }
            t->lastCd = _finite(cd) ? cd : 1.0e9f;
        } else {
            if (t->active) tgt = 1.0f;
            t->reachT = 1.0f;
        }
        t->down = down == 1;
        if (t->reach > t->reachT) t->reach = t->reach - dt / SV_RETRACT_S < t->reachT ? t->reachT : t->reach - dt / SV_RETRACT_S;
        else t->reach = t->reach + dt / 0.4f > t->reachT ? t->reachT : t->reach + dt / 0.4f;
        if (t->s < tgt) t->s = t->s + dt / SV_GROW_S > tgt ? tgt : t->s + dt / SV_GROW_S;
        else t->s = t->s - dt / SV_FADE_S < tgt ? tgt : t->s - dt / SV_FADE_S;
        kt = t->down ? t->reach : SvEase(t->s);  // the veins' strength: their reach while down, else the aura's
        t->kv = kt >= t->kv ? kt : t->kv - dt / SV_RETRACT_S < kt ? kt : t->kv - dt / SV_RETRACT_S;
        if (!t->active && t->s <= 0.0f && down != 1) {
            g_svTr[j--] = g_svTr[--g_svTrN];
            continue;
        }
        if (!t->active && g_fxFrame - t->seen > 180 * 120) {  // 2 minutes unseen (at 180 fps): gone
            g_svTr[j--] = g_svTr[--g_svTrN];
        }
    }
}

// ---- S2 the column: 3 shells, SV_COL_BANDS bands from y0 (top, smaller y) to y1; sym: fade toward both ends
// (v3.21.0: only the hidden SpiritTest depth probe draws it; the auras are sprites, SvAuraFrame)
static void SvColumn(void *dev, const Viewport8 *vp, const float c[3], float R, float y0, float y1, int sym,
                     const float pal[2][3], float k0, float now, const float eye[3])
{
    static const float kShell[3] = { 1.0f, 0.82f, 0.6f };
    static float s_cos[SV_COL_SEG + 1], s_sin[SV_COL_SEG + 1];
    GvHv ring[2][SV_COL_SEG + 1];
    float dx = c[0] - eye[0], dz = c[2] - eye[2];
    const int step = dx * dx + dz * dz > 3000.0f * 3000.0f ? 2 : 1;
    int L, b, s, cur;
    if (!s_cos[0]) {
        for (s = 0; s <= SV_COL_SEG; ++s) {
            s_cos[s] = cosf(6.2831853f * (float)s / SV_COL_SEG);
            s_sin[s] = sinf(6.2831853f * (float)s / SV_COL_SEG);
        }
    }
    if (!(k0 > 0.002f) || !(R > 1.0f) || !(y1 > y0)) return;
    for (L = 0; L < 3; ++L) {
        const float rr = R * kShell[L], om = (L & 1 ? -1.0f : 1.0f) * (0.5f + 0.25f * (float)L);
        cur = 0;
        for (b = 0; b <= SV_COL_BANDS; ++b) {
            const float y = y0 + (y1 - y0) * (float)b / SV_COL_BANDS, hl = (float)b / SV_COL_BANDS;
            const float hn = sym ? FxSat(fabsf(y - c[1]) / (0.5f * (y1 - y0))) : 1.0f - hl;
            const float fade = sym ? powf(1.0f - hn, 1.2f) : powf(1.0f - hn, 1.5f);
            float rgb[3];
            SvMix(rgb, pal[0], pal[1], hn);
            for (s = 0; s <= SV_COL_SEG; s += step) {
                const float th = 6.2831853f * (float)s / SV_COL_SEG;
                const float p[3] = { c[0] + rr * s_cos[s], y, c[2] + rr * s_sin[s] }, nn[3] = { s_cos[s], 0.0f, s_sin[s] };
                const float sw = 0.5f + 0.5f * sinf(3.0f * th + 3.14159265f * 3.0f * hl - om * now);
                const float rim = SvRim(p, nn, eye);
                SvPoint(&ring[cur][s], p, k0 * fade * (SV_COL_K * (0.35f + 0.65f * sw) + SV_COL_RIM * rim * rim / 3.0f), rgb);
            }
            if (b)
                for (s = 0; s < SV_COL_SEG; s += step) {
                    const GvHv *a = &ring[!cur][s], *a2 = &ring[!cur][s + step], *d = &ring[cur][s], *d2 = &ring[cur][s + step];
                    if (!SvHvOk(a) || !SvHvOk(a2) || !SvHvOk(d) || !SvHvOk(d2)) continue;
                    GvTri(dev, a, a2, d2, vp, NULL);
                    GvTri(dev, a, d2, d, vp, NULL);
                }
            cur = !cur;
        }
    }
}
// ---- S5 wall blood portals (task pool scan, our own vortex in the wall plane)
static int SvPortals(void *dev, const Viewport8 *vp, float now)
{
    static const float kCore[3] = { 1.0f, 0.18f, 0.10f }, kEdge[3] = { 0.55f, 0.0f, 0.12f };
    enum { NR = 12, NS = 32 };
    int i, drawn = 0;
    for (i = 0; i < g_tpN && drawn < SV_PORTAL_MAX; ++i) {  // v3.36 TP336: every slot of the task pool
        const BYTE *t = TaskSlotAt(i);
        float c[3], nn[3], u[3], v[3], a, b, open, l;
        GvHv g[2][NS + 1];
        int r, s, cur = 0;
        __try {
            const BYTE *w;
            if (*(const DWORD *)(t + 0x404) != 0x487650 || (w = *(const BYTE *const *)(t + 0x408)) == NULL) continue;
            memcpy(c, w, 12);
            memcpy(nn, w + 0x10, 12);
            a = *(const float *)(w + 0x40);
            b = *(const float *)(w + 0x44);
            open = *(const float *)(w + 0xEC);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
        if (!_finite(c[0] + c[1] + c[2] + nn[0] + nn[1] + nn[2] + a + b + open)) continue;
        open = FxSat(open);
        if (open < 0.02f || a < 10.0f || b < 10.0f || a > 2000.0f || b > 2000.0f) continue;
        l = sqrtf(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
        if (l < 1e-4f) continue;
        nn[0] /= l, nn[1] /= l, nn[2] /= l;
        u[0] = -nn[2], u[1] = 0.0f, u[2] = nn[0];  // n x up (up = -y)
        l = sqrtf(u[0] * u[0] + u[2] * u[2]);
        if (l < 1e-3f) u[0] = 1.0f, u[2] = 0.0f, l = 1.0f;
        u[0] /= l, u[2] /= l;
        v[0] = nn[1] * u[2] - nn[2] * u[1], v[1] = nn[2] * u[0] - nn[0] * u[2], v[2] = nn[0] * u[1] - nn[1] * u[0];
        c[0] += 8.0f * nn[0], c[1] += 8.0f * nn[1], c[2] += 8.0f * nn[2];
        a *= 0.5f * 1.15f * open;
        b *= 0.5f * 1.15f * open;
        for (r = 0; r <= NR; ++r) {
            const float rn = (float)r / NR;
            float rgb[3];
            SvMix(rgb, kCore, kEdge, rn);
            for (s = 0; s <= NS; ++s) {
                const float th = 6.2831853f * (float)s / NS, cs = cosf(th), sn = sinf(th);
                float k = 0.35f * powf(1.0f - rn, 0.7f), p[3];
                int m;
                for (m = 0; m < 3; ++m) {
                    const float ri = 0.35f + 0.30f * (float)m, rim = 1.0f - fabsf(rn - ri) / 0.08f;
                    if (rim > 0.0f) k += rim * (0.6f + 0.4f * sinf(5.0f * th + (m & 1 ? -1.0f : 1.0f) * now * (1.2f + 0.5f * (float)m)));
                }
                p[0] = c[0] + rn * (a * cs * u[0] + b * sn * v[0]);
                p[1] = c[1] + rn * (a * cs * u[1] + b * sn * v[1]);
                p[2] = c[2] + rn * (a * cs * u[2] + b * sn * v[2]);
                SvPoint(&g[cur][s], p, k * SV_PORTAL_K * open, rgb);
            }
            if (r)
                for (s = 0; s < NS; ++s) {
                    GvTri(dev, &g[!cur][s], &g[!cur][s + 1], &g[cur][s + 1], vp, NULL);
                    GvTri(dev, &g[!cur][s], &g[cur][s + 1], &g[cur][s], vp, NULL);
                }
            cur = !cur;
        }
        ++drawn;
    }
    return drawn;
}

// ---- v3.21.0 cinematic auras (staging/feedback_v321/PLAN.md section 3): camera-facing sprites and particles
// Replaces the v3.17 haunting columns + floor rims, the 400-unit holy sphere and the medallion's gold column.
// SvAuraFrame (once a frame): sources (hauntings from the S4 tracks, candles from the holy snapshot, the medallion),
// the particle pool, and the sprite list in screen space. SvAuraDraw (each depth pass): premultiplied alpha
// (ONE / INVSRCALPHA; the atlas is premultiplied, review V321-003) for the dark mist and ash, then additive (ONE / ONE)
// for veins, embers, glows, halos and orbs. Every random choice is a hash of a source key and a time slot or a spawn
// serial, every motion uses real seconds: the look does not depend on the frame rate (60-180 fps).
enum { SVC_SOFT, SVC_VEIN0, SVC_MIST0 = 5, SVC_ASH0 = 7, SVC_HALO = 9, SVC_ORB, SVC_BLOB, SVC_SPARK, SVC_THIN };  // atlas cells
enum { SVS_HAUNT, SVS_HOLY };
typedef struct { float vein[3], crack[3], mist[2][3], glow[3], blob[3], blobDark[3], ash[3], ember[3]; } SvAuraPal;
static const SvAuraPal kSvAura[2] = {
    { { 0.95f, 0.10f, 0.06f }, { 1.00f, 0.32f, 0.25f }, { { 0.02f, 0.0f, 0.0f }, { 0.16f, 0.0f, 0.02f } }, { 0.40f, 0.02f, 0.05f },
      { 0.75f, 0.02f, 0.06f }, { 0.12f, 0.0f, 0.01f }, { 0.05f, 0.035f, 0.03f }, { 1.0f, 0.38f, 0.08f } },  // ghosts: crimson / black
    { { 0.60f, 0.70f, 0.06f }, { 0.90f, 1.00f, 0.50f }, { { 0.01f, 0.02f, 0.0f }, { 0.05f, 0.08f, 0.01f } }, { 0.30f, 0.36f, 0.03f },
      { 0.45f, 0.55f, 0.03f }, { 0.05f, 0.08f, 0.0f }, { 0.04f, 0.05f, 0.02f }, { 0.85f, 0.90f, 0.20f } } }; // Eileen's fit: bile
static const float kSvGold[3] = { 1.0f, 0.78f, 0.35f }, kSvHot[3] = { 1.0f, 0.95f, 0.80f }, kSvBlack[3] = { 0.0f, 0.0f, 0.0f };
static const float kSvOrbA[3] = { 1.0f, 0.92f, 0.60f }, kSvOrbB[3] = { 1.0f, 1.0f, 0.95f };  // pale yellow .. white
typedef struct { float x, y, z, rhw, rx, ry, cs, sn, rgb[3], a; int cell, add; } SvSpr;
typedef struct { DWORD key; int kind, pal, flare, own; float c[3], R, Rs, Pm, y0, y1, k, dist;
                 int nb, down, noVeins; float kv, reach, reachT, bone[SV_BONES][3]; const BYTE *ghost; } SvSrc;  // v3.26.0: the matched ghost task (INSP326-011)  // v3.23.0: the ghost's bones, the veins' state
typedef struct { DWORD key; int kind; float t0, phase; LONG seen; } SvOwn;
typedef struct {
    DWORD key;
    int kind, pal, cell, ember, live;
    float off[3], vel[3], c[3], t0, life, rot, spin, size, ph, fq, amp, k, orphan, tint;
} SvPt;
static SvSpr g_svSpr[SV_SPR_MAX];
static SvSrc g_svSrc[SV_SRC_MAX];
static SvOwn g_svOwn[SV_OWN_MAX];
static SvPt g_svPt[SV_PT_MAX];
static int g_svSprN, g_svSprDrop, g_svSrcN, g_svEyeOk, g_svHaunts, g_svCandles;
static float g_svPxX, g_svPxY, g_svUnit = 1.0f;  // pixels per world unit at w = 1 (x / y); pixels per 720-line pixel
static const Viewport8 *g_svVp;
static DWORD g_svSerial, g_svRoomKey = 0xFFFFFFFFu;

// pixels per world unit at w = 1 for an offset perpendicular to the view (GvXform's w gradient), per screen axis
static int SvSprScale(const Viewport8 *vp)
{
    const float c3[3] = { g_gvM[3], g_gvM[7], g_gvM[11] };
    const float l3 = sqrtf(c3[0] * c3[0] + c3[1] * c3[1] + c3[2] * c3[2]);
    float gs[2] = { 1.0f / 256.0f, 1.0f / 224.0f }, s[2];
    int a, j;
    if (!_finite(l3) || !(l3 > 1e-6f)) return 0;
    if (g_haveConsts && g_native.c2[0] != 0.0f && g_native.c2[1] != 0.0f) gs[0] = fabsf(g_native.c2[0]), gs[1] = fabsf(g_native.c2[1]);
    for (a = 0; a < 2; ++a) {
        float c[3], d = 0.0f;
        for (j = 0; j < 3; ++j) c[j] = g_gvM[4 * j + a];
        for (j = 0; j < 3; ++j) d += c[j] * c3[j] / l3;
        for (j = 0; j < 3; ++j) c[j] -= d * c3[j] / l3;  // without the part along the view (the GS 2048 offset x w)
        s[a] = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    }
    g_svPxX = s[0] * gs[0] * 0.5f * (float)vp->w;
    g_svPxY = s[1] * gs[1] * 0.5f * (float)vp->h;
    return _finite(g_svPxX + g_svPxY) && g_svPxX > 0.0f && g_svPxY > 0.0f;
}
// one camera-facing sprite: world centre p, world radius wr, screen rotation rot, atlas cell, batch (add 1 = additive),
// colour and strength (additive: brightness; premultiplied: opacity), a minimum screen radius in pixels (0 = none)
static void SvSprAdd(const float p[3], float wr, float rot, int cell, int add, const float rgb[3], float a, float minPx)
{
    const Viewport8 *vp = g_svVp;
    float h[4], x, y, rx, ry, m, d;
    SvSpr *s;
    if (!(a > 0.004f) || !(wr > 0.0f) || !vp) return;
    if (g_svSprN >= SV_SPR_MAX) {
        ++g_svSprDrop;
        return;
    }
    GvXform(p, h);
    if (!GvScreen(h, vp, &x, &y)) return;  // also the w >= GV_NEAR_W near clip
    if (g_svEyeOk) {
        const float dx = p[0] - g_svEye[0], dy = p[1] - g_svEye[1], dz = p[2] - g_svEye[2];
        d = sqrtf(dx * dx + dy * dy + dz * dz);
    } else {
        d = h[3];
    }
    a *= FxSmooth(SV_NEAR0, SV_NEAR1, d - wr);  // near-camera fade: the camera inside a source sees no wall of colour
    if (!(a > 0.004f)) return;
    rx = wr * g_svPxX / h[3];
    ry = wr * g_svPxY / h[3];
    m = rx > ry ? rx : ry;
    if (!_finite(m) || !(m > 0.0f)) return;
    if (m > SV_SPR_MAXH * (float)vp->h) {
        const float f = SV_SPR_MAXH * (float)vp->h / m;
        rx *= f, ry *= f;
    } else if (m < minPx) {
        const float f = minPx / m;
        rx *= f, ry *= f;
    }
    m = rx > ry ? rx : ry;  // the rotated quad's reach
    if (x + 1.415f * m < (float)vp->x || x - 1.415f * m > (float)(vp->x + vp->w) || y + 1.415f * m < (float)vp->y ||
        y - 1.415f * m > (float)(vp->y + vp->h))
        return;
    s = &g_svSpr[g_svSprN++];
    s->x = x, s->y = y, s->z = SvZH(h), s->rhw = 1.0f / h[3];
    s->rx = rx, s->ry = ry, s->cs = cosf(rot), s->sn = sinf(rot);
    memcpy(s->rgb, rgb, sizeof s->rgb);
    s->a = a;
    s->cell = cell, s->add = add;
}
// per-source state kept across frames (keyed: tracks by id, candles by their task, the medallion 1)
static int SvOwner(DWORD key, int kind, float now)
{
    LONG oldest = 0x7FFFFFFF;
    int i, best = 0;
    for (i = 0; i < SV_OWN_MAX; ++i)
        if (g_svOwn[i].key == key && g_svOwn[i].kind == kind) break;
    if (i == SV_OWN_MAX) {
        for (i = 0; i < SV_OWN_MAX; ++i) {  // a free slot, else the one unseen longest (never one of this frame:
            if (!g_svOwn[i].key) {          // SV_OWN_MAX > SV_SRC_MAX)
                best = i;
                break;
            }
            if (g_svOwn[i].seen < oldest) oldest = g_svOwn[i].seen, best = i;
        }
        i = best;
        g_svOwn[i].key = key;
        g_svOwn[i].kind = kind;
        g_svOwn[i].t0 = now;
        g_svOwn[i].phase = FxRnd(key * 2654435761u + (DWORD)kind);
    }
    g_svOwn[i].seen = g_fxFrame;
    return i;
}
static void SvSrcAdd(const SvSrc *src, const float pp[3])  // keep the SV_SRC_MAX nearest to the player, sorted
{
    const float dx = src->c[0] - pp[0], dy = src->c[1] - pp[1], dz = src->c[2] - pp[2], d = dx * dx + dy * dy + dz * dz;
    int i;
    if (!_finite(d)) return;
    if (g_svSrcN == SV_SRC_MAX && d >= g_svSrc[SV_SRC_MAX - 1].dist) return;
    i = g_svSrcN < SV_SRC_MAX ? g_svSrcN++ : SV_SRC_MAX - 1;
    for (; i > 0 && g_svSrc[i - 1].dist > d; --i) g_svSrc[i] = g_svSrc[i - 1];
    g_svSrc[i] = *src;
    g_svSrc[i].dist = d;
}
// v3.22.0 (user item 4: "appears near the groin"): the player's chest bone 9 (henry01: 9 upper spine, the breasts hang
// from it) + SV_MEDAL_FWD along the facing (sin, cos)(yaw [0x10A0FC4]); fallback the feet - SV_MEDAL_CHEST
static void SvMedalPos(const float pp[3], float c[3])
{
    c[0] = pp[0], c[1] = pp[1] - SV_MEDAL_CHEST, c[2] = pp[2];
    __try {
        const BYTE *m = *(const BYTE *const *)(0x10A0F20 + 0x1B0), *skel = m ? *(const BYTE *const *)(m + 0xE0) : NULL;
        const float *mats = m ? *(const float *const *)(m + 0xC0) : NULL;
        const float yaw = *(volatile float *)0x10A0FC4;
        if (mats && skel && *(const int *)(skel + 0x14) > 9) {
            const float *b = mats + 16 * 9 + 12;
            if (_finite(b[0] + b[1] + b[2] + yaw) && fabsf(b[0] - pp[0]) < 400.0f && fabsf(b[1] - pp[1]) < 600.0f && fabsf(b[2] - pp[2]) < 400.0f) {
                c[0] = b[0] + SV_MEDAL_FWD * sinf(yaw);
                c[1] = b[1];
                c[2] = b[2] + SV_MEDAL_FWD * cosf(yaw);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
// v3.23.0 (user item 15: "the veins do not come directly from the ghost's body"): the matched ghost's skeleton this
// frame (its model from this frame's ghost list; bone b at [model + 0xC0] + 0x40 b + 0x30, count [[model + 0xE0] + 0x14],
// KB 113), the bones kept only if finite and near its work position; with 3 or more the source's centre becomes their
// mean and the veins root on them (a lying ghost's bones lie on the floor). Review SH4-323-01 / 04 / 05: no cache: a
// frame without this frame's skeleton gives a matched ghost no tendrils (o->noVeins), never the old floating root (the
// collect hook 0x52AEB5 sees every model each frame, the cull is inside 0x55C650: KB 113, so this is rare).
static int SvWorkPos(const SvTrack *t, float wp[3])
{
    __try {
        const BYTE *w = *(const BYTE *const *)(t->ghost + 0x408);
        if (!w || w != t->work || !*(const DWORD *)(t->ghost + 0x404)) return 0;
        memcpy(wp, w, 12);
        return _finite(wp[0] + wp[1] + wp[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
// v3.28 CORE (user, v3.27.3 item 1: the heart also for possessed Eileen): her fit's source (R 800 at her model root - 100
// y, KB 113) is never a ghost-list actor; her companion model ([[0xFCE6B4] + 0x408] + 0x18, as SvTrackStep / Death
// Presence) gives its skeleton the same way, so the source centres on her body and the heart sits on her chest bone 9.
// Not her: nb stays 0 (the source-centre heart, as a wheelchair's). Not live-verified (staging/v328_spirit/a_tendrils).
static void SvEileenBones(const SvTrack *t, SvSrc *o)
{
    float ep[3], c[3] = { 0, 0, 0 };
    int b, nb;
    if (!(fabsf(t->R - 800.0f) < 0.5f)) return;
    __try {
        const BYTE *task = *(const BYTE *const *)0xFCE6B4, *w, *m, *skel;
        const float *mats;
        if (!task || !(w = *(const BYTE *const *)(task + 0x408)) || !(m = *(const BYTE *const *)(w + 0x18))) return;
        memcpy(ep, m + 0x10, 12);
        if (!_finite(ep[0] + ep[1] + ep[2]) || fabsf(ep[0] - t->p[0]) > 60.0f || fabsf(ep[2] - t->p[2]) > 60.0f ||
            fabsf(ep[1] - 100.0f - t->p[1]) > 60.0f)
            return;
        mats = *(const float *const *)(m + 0xC0);
        skel = *(const BYTE *const *)(m + 0xE0);
        if (!mats) return;
        nb = skel ? *(const int *)(skel + 0x14) : SV_BONES;
        if (nb > SV_BONES) nb = SV_BONES;
        for (b = 0; b < nb; ++b) {
            const float *q = mats + 16 * b + 12;
            if (!_finite(q[0] + q[1] + q[2]) || fabsf(q[0] - ep[0]) > 400.0f || fabsf(q[2] - ep[2]) > 400.0f || fabsf(q[1] - ep[1]) > 600.0f)
                continue;
            memcpy(o->bone[o->nb++], q, 12);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        o->nb = 0;
    }
    if (o->nb < 3) {
        o->nb = 0;
        return;
    }
    for (b = 0; b < o->nb; ++b) c[0] += o->bone[b][0], c[1] += o->bone[b][1], c[2] += o->bone[b][2];
    o->c[0] = c[0] / (float)o->nb, o->c[1] = c[1] / (float)o->nb, o->c[2] = c[2] / (float)o->nb;
}
static void SvTrackBones(const SvTrack *t, SvSrc *o)
{
    float wp[3], c[3] = { 0, 0, 0 };
    int i, b;
    o->nb = 0;
    o->noVeins = 0;
    if (!t->ghost) {  // a portal, a wall ghost, a Room 302 object, a wheelchair: the source-centre heart
        SvEileenBones(t, o);  // v3.28 CORE: ... unless it is possessed Eileen's fit
        return;
    }
    if (!SvWorkPos(t, wp)) {
        o->noVeins = 1;
        return;
    }
    for (i = 0; g_gvStamp == g_fxFrame && i < g_gvN && !o->nb; ++i) {
        const GvEnt *e = &g_gv[i];
        if (e->task != t->ghost || e->kind != GVK_ACTOR || !GvValid(e)) continue;
        __try {
            const BYTE *m = e->model, *skel = *(const BYTE *const *)(m + 0xE0);
            const float *mats = *(const float *const *)(m + 0xC0);
            int nb = SV_BONES;
            if (mats) {
                if (skel) nb = *(const int *)(skel + 0x14);
                if (nb > SV_BONES) nb = SV_BONES;
                for (b = 0; b < nb; ++b) {
                    const float *q = mats + 16 * b + 12;
                    if (!_finite(q[0] + q[1] + q[2]) || fabsf(q[0] - wp[0]) > 400.0f || fabsf(q[2] - wp[2]) > 400.0f ||
                        fabsf(q[1] - wp[1]) > 600.0f)
                        continue;
                    memcpy(o->bone[o->nb++], q, 12);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            o->nb = 0;
        }
        if (o->nb < 3) o->nb = 0;
    }
    if (!o->nb) {
        o->noVeins = 1;
        return;
    }
    for (b = 0; b < o->nb; ++b) c[0] += o->bone[b][0], c[1] += o->bone[b][1], c[2] += o->bone[b][2];
    o->c[0] = c[0] / (float)o->nb, o->c[1] = c[1] / (float)o->nb, o->c[2] = c[2] / (float)o->nb;
}
static void SvAuraSources(float now, float dt)
{
    const BYTE *hs = (const BYTE *)0x10A28E0, *ds = (const BYTE *)0x10A1EC0;
    int n = *(volatile int *)0x10A1EB4, nd = *(volatile int *)0x10A22C0, i, j, nc = 0;
    float pp[3], cd[32];
    SvSrc s, cand[32];  // review V321-R1: every valid candle first, then the SV_HOLY_MAX nearest
    g_svSrcN = g_svHaunts = g_svCandles = 0;
    memcpy(pp, (const void *)0x10A0F60, 12);
    if (!_finite(pp[0] + pp[1] + pp[2])) return;
    for (j = 0; j < g_svTrN; ++j) {  // hauntings: the S4 tracks (grow / fade / downed preview in t->s)
        SvTrack *t = &g_svTr[j];
        const float e = SvEase(t->s);
        if (!(e > 0.002f) || !_finite(t->p[0] + t->p[1] + t->p[2] + t->R)) continue;
        memset(&s, 0, sizeof s);
        s.kind = SVS_HAUNT;
        s.key = t->id;
        s.pal = t->pal;
        s.R = t->R;
        s.Pm = (t->R < SV_MIST_MAX ? t->R : SV_MIST_MAX) * (0.35f + 0.65f * e);
        s.Rs = (t->R * SV_SPREAD < SV_SPREAD_MAX ? t->R * SV_SPREAD : SV_SPREAD_MAX) * (0.3f + 0.7f * e);
        if (t->room302) s.y0 = pp[1] - SV_COL_R302_H, s.y1 = pp[1];  // Room 302: the player's feet up (as the old column)
        else s.y0 = t->p[1] - SV_HAUNT_UP, s.y1 = t->p[1] + SV_HAUNT_DN;
        s.c[0] = t->p[0], s.c[1] = 0.5f * (s.y0 + s.y1), s.c[2] = t->p[2];
        s.k = e * (t->pal == SVP_GOLD ? SV_REPEL_K : 1.0f);  // repelled: the crimson look at half strength + gold sparks
        s.kv = t->kv * (t->pal == SVP_GOLD ? SV_REPEL_K : 1.0f);  // v3.23.0: the veins (item 17)
        s.reach = t->reach, s.reachT = t->reachT, s.down = t->down;
        s.ghost = t->ghost;
        SvTrackBones(t, &s);  // v3.23.0 (item 15): centre on the body, the bones for the vein roots
        if (s.nb) {
            const float hh = 0.5f * (s.y1 - s.y0);
            s.y0 = s.c[1] - hh, s.y1 = s.c[1] + hh;
        }
        SvSrcAdd(&s, pp);
    }
    if (n < 0) n = 0;
    if (n > 32) n = 32;
    if (nd < 0) nd = 0;
    if (nd > 32) nd = 32;
    for (i = 0; i < n; ++i) {  // S6 holy candles (the holy snapshot; pos = the candle + 80 up)
        const BYTE *e = hs + 0x30 * i, *own = NULL;
        const float *p = (const float *)(e + 0x10);
        float fade = 1.0f, burn;
        int flare = 0;
        if (*(const int *)e != 2 || !_finite(p[0] + p[1] + p[2])) continue;
        __try {  // burn 0..100: [[[owner + 0x408] + 0xF0] + 0x20]
            const BYTE *o = *(const BYTE *const *)(e + 4), *w = o ? *(const BYTE *const *)(o + 0x408) : NULL;
            const BYTE *rec = w ? *(const BYTE *const *)(w + 0xF0) : NULL;
            own = o;
            if (rec) {
                burn = *(const float *)(rec + 0x20);
                if (_finite(burn)) fade = 0.15f + 0.85f * FxSat(1.0f - burn / 100.0f);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        for (j = 0; j < nd; ++j) {  // repelling now: a damage source centre within the reach -> flare
            const float *d = (const float *)(ds + 0x20 * j);
            const float dx = d[0] - p[0], dy = d[1] - p[1], dz = d[2] - p[2];
            if (dx * dx + dy * dy + dz * dz < SV_HOLY_R * SV_HOLY_R) {
                flare = 1;
                break;
            }
        }
        memset(&s, 0, sizeof s);
        s.kind = SVS_HOLY;
        s.key = own ? (DWORD)(ULONG_PTR)own : 0x100u + (DWORD)i;
        s.flare = flare;
        memcpy(s.c, p, 12);
        s.R = SV_HOLY_VR;
        s.k = fade;
        cand[nc] = s;
        cd[nc] = (p[0] - pp[0]) * (p[0] - pp[0]) + (p[1] - pp[1]) * (p[1] - pp[1]) + (p[2] - pp[2]) * (p[2] - pp[2]);
        if (!_finite(cd[nc])) cd[nc] = 3.0e38f;
        ++nc;
    }
    for (j = 0; j < SV_HOLY_MAX && j < nc; ++j) {  // review V321-R1: the nearest SV_HOLY_MAX candles
        int b = j;
        for (i = j + 1; i < nc; ++i)
            if (cd[i] < cd[b]) b = i;
        if (b != j) {
            const SvSrc ts = cand[j];
            const float td = cd[j];
            cand[j] = cand[b], cd[j] = cd[b];
            cand[b] = ts, cd[b] = td;
        }
        SvSrcAdd(&cand[j], pp);
    }
    {   // the Saint Medallion: v3.22.0 (user item 4) shown the whole time it is worn ([0x13D7FF4]), calm; repelling
        // (g_svWardT, SvTrackStep) it flares as before. At the medallion: the chest bone 9 + SV_MEDAL_FWD forward.
        const float age = now - g_svWardT;
        const int worn = *(volatile BYTE *)0x13D7FF4 != 0, rep = age >= 0.0f && age < SV_WARD_S;
        if (worn || rep) {
            memset(&s, 0, sizeof s);
            s.kind = SVS_HOLY;
            s.key = 1;
            s.flare = rep;
            SvMedalPos(pp, s.c);
            s.R = SV_MEDAL_VR;
            s.k = rep ? SV_MEDAL_CALM + (1.0f - SV_MEDAL_CALM) * (1.0f - FxSmooth(0.0f, SV_WARD_S, age)) : SV_MEDAL_CALM;
            SvSrcAdd(&s, pp);
        }
    }
    for (i = 0; i < g_svSrcN; ++i) {
        SvSrc *q = &g_svSrc[i];
        SvOwn *o;
        q->own = SvOwner(q->key, q->kind, now);
        o = &g_svOwn[q->own];
        if (q->kind == SVS_HOLY) {
            o->phase += dt / (q->flare ? SV_PULSE_FLARE_S : SV_PULSE_S);  // the halo pulse (integrated: no jump on a flare)
            if (o->phase > 1000.0f) o->phase -= 1000.0f;
            if (q->key != 1) {
                q->k *= FxSmooth(0.0f, 0.6f, now - o->t0);  // a candle seen first fades in
                ++g_svCandles;
            }
        } else {
            ++g_svHaunts;
        }
    }
}
// ---- the particle pool: ash / embers (hauntings), orbs (holy); owners = the SV_OWNERS nearest sources
#define SV_RND(n) FxRnd(h + (DWORD)(n) * 0x632BE5ABu)
static void SvPtSpawn(SvPt *p, const SvSrc *s, float now)
{
    const DWORD h = FxHash(s->key * 0x9E3779B9u ^ ++g_svSerial * 0x85EBCA6Bu ^ (DWORD)s->kind);
    memset(p, 0, sizeof *p);
    p->live = 1;
    p->key = s->key;
    p->kind = s->kind;
    p->pal = s->pal;
    p->t0 = now;
    p->k = s->k;
    memcpy(p->c, s->c, sizeof p->c);
    p->ph = 6.2831853f * SV_RND(1);
    p->rot = 6.2831853f * SV_RND(2);
    if (s->kind == SVS_HAUNT) {  // ash rising and fluttering from the lower part of the volume; 1 in 5 an ember
        const float a = 6.2831853f * SV_RND(3), r = 0.7f * s->Rs * sqrtf(SV_RND(4));
        p->off[0] = r * cosf(a);
        p->off[2] = r * sinf(a);
        p->off[1] = (s->y1 - s->c[1]) - (s->y1 - s->y0) * (0.05f + 0.6f * SV_RND(5));
        p->life = 2.5f + 1.5f * SV_RND(6);
        p->ember = SV_RND(7) < 0.2f;
        p->vel[0] = (SV_RND(8) - 0.5f) * 24.0f;
        p->vel[2] = (SV_RND(9) - 0.5f) * 24.0f;
        p->vel[1] = p->ember ? -(45.0f + 45.0f * SV_RND(10)) : -(22.0f + 30.0f * SV_RND(10));  // y down: rising
        p->size = p->ember ? 2.5f + 2.5f * SV_RND(11) : 6.0f + 7.0f * SV_RND(11);
        p->cell = SVC_ASH0 + (SV_RND(12) < 0.5f);
        p->spin = (SV_RND(13) - 0.5f) * 4.0f;
        p->fq = 2.5f + 3.0f * SV_RND(14);
        p->amp = 6.0f + 14.0f * SV_RND(15);
    } else {  // an orb somewhere in the 0.6 R ball, drifting up
        const float a = 6.2831853f * SV_RND(3), z = 2.0f * SV_RND(4) - 1.0f, q = sqrtf(FxSat(1.0f - z * z));
        const float r = 0.6f * s->R * powf(SV_RND(5), 1.0f / 3.0f);
        p->off[0] = r * q * cosf(a);
        p->off[1] = r * z;
        p->off[2] = r * q * sinf(a);
        p->life = 2.0f + 2.0f * SV_RND(6);
        p->vel[0] = (SV_RND(8) - 0.5f) * 8.0f;
        p->vel[2] = (SV_RND(9) - 0.5f) * 8.0f;
        p->vel[1] = -(10.0f + 18.0f * SV_RND(10));
        p->size = 3.5f + 3.5f * SV_RND(11);
        p->cell = SVC_ORB;
        p->tint = SV_RND(12);
        p->fq = 0.8f + 0.8f * SV_RND(14);
        p->amp = 4.0f + 8.0f * SV_RND(15);
    }
}
#undef SV_RND
static void SvPtStep(float now, float dt)
{
    int i, j, cnt[SV_OWNERS];
    const int owners = g_svSrcN < SV_OWNERS ? g_svSrcN : SV_OWNERS;
    memset(cnt, 0, sizeof cnt);
    for (i = 0; i < SV_PT_MAX; ++i) {
        SvPt *p = &g_svPt[i];
        int own = -1;
        if (!p->live) continue;
        if (!(now - p->t0 < p->life) || now < p->t0) {
            p->live = 0;
            continue;
        }
        for (j = 0; j < owners && own < 0; ++j)
            if (g_svSrc[j].key == p->key && g_svSrc[j].kind == p->kind) own = j;
        if (own >= 0) {  // follow the source (a moving ghost)
            memcpy(p->c, g_svSrc[own].c, sizeof p->c);
            p->k = g_svSrc[own].k;
            p->pal = g_svSrc[own].pal;
            p->orphan = 0.0f;
            ++cnt[own];
        } else if ((p->orphan += dt) >= SV_ORPHAN_S) {  // its source went away (or is no longer among the nearest)
            p->live = 0;
            continue;
        }
        for (j = 0; j < 3; ++j) p->off[j] += p->vel[j] * dt;
    }
    for (j = 0; j < owners; ++j) {  // top each owner up to its population (at most 2 new a frame each)
        const SvSrc *s = &g_svSrc[j];
        const int base = s->kind == SVS_HAUNT ? SV_ASH_N : SV_ORB_N + (int)(FxHash(s->key) % 5u);
        int want = (int)((float)base * FxSat(s->k * 1.5f) + 0.5f) - cnt[j], spawn = 2;
        for (i = 0; i < SV_PT_MAX && want > 0 && spawn > 0; ++i)
            if (!g_svPt[i].live) {
                SvPtSpawn(&g_svPt[i], s, now);
                --want, --spawn;
            }
    }
}
static void SvPtSprites(float now)
{
    int i;
    for (i = 0; i < SV_PT_MAX; ++i) {
        const SvPt *q = &g_svPt[i];
        const float age = now - q->t0;
        float fade, p[3], a;
        if (!q->live || age < 0.0f || age >= q->life) continue;
        fade = 1.0f - FxSat(q->orphan / SV_ORPHAN_S);
        p[0] = q->c[0] + q->off[0] + q->amp * sinf(q->fq * age + q->ph);  // flutter / wander (closed form)
        p[1] = q->c[1] + q->off[1];
        p[2] = q->c[2] + q->off[2] + q->amp * cosf(0.8f * q->fq * age + 1.3f * q->ph);
        if (q->kind == SVS_HAUNT) {
            const SvAuraPal *P = &kSvAura[q->pal == SVP_BILE];
            a = FxSmooth(0.0f, 0.4f, age) * (1.0f - FxSmooth(q->life - 0.8f, q->life, age)) * q->k * fade;
            if (q->ember)
                SvSprAdd(p, q->size, 0.0f, SVC_ORB, 1, P->ember, a * (0.65f + 0.35f * sinf(23.0f * age + q->ph)), 1.5f * g_svUnit);
            else
                SvSprAdd(p, q->size, q->rot + q->spin * age + 0.5f * sinf(2.0f * q->fq * age + q->ph), q->cell, 0, P->ash,
                         SV_ASH_K * a, 1.2f * g_svUnit);
        } else {
            float rgb[3];
            SvMix(rgb, kSvOrbA, kSvOrbB, q->tint);
            a = sinf(3.14159265f * FxSat(age / q->life));
            SvSprAdd(p, q->size, q->rot, SVC_ORB, 1, rgb, SV_ORB_K * a * q->k * fade, 2.5f * g_svUnit);
        }
    }
}
#include "svveins.inc"  // v3.22.0: SvHauntSprites + the haunting veins (item 9)
// ---- holy (candle, medallion): a gold core glow, a hot core, pulse halos (0.2 R -> R over SV_HALO_LIFE, two overlap)
static void SvHolySprites(const SvSrc *s, float now)
{
    const float R = s->R, k = s->k * (s->flare ? SV_HOLY_FLARE : 1.0f), ph = 6.2831853f * FxRnd(s->key * 0x9E3779B9u + 7u);
    const float br = 0.85f + 0.15f * sinf(2.1f * now + ph), phase = g_svOwn[s->own].phase, f = phase - floorf(phase);
    const float lp = SV_HALO_LIFE / SV_PULSE_S;  // the halo's life in pulses
    int m;
    SvSprAdd(s->c, 0.45f * R, 0.0f, SVC_SOFT, 1, kSvGold, SV_HOLY_GLOW_K * k * br, 0.0f);
    SvSprAdd(s->c, 0.14f * R, 0.2f * now, SVC_ORB, 1, kSvHot, 0.8f * k * br, 3.0f * g_svUnit);
    for (m = 0; m < 2; ++m) {
        const float u = (f + (float)m) / lp, e = 1.0f - (1.0f - u) * (1.0f - u);
        if (u >= 1.0f) continue;
        SvSprAdd(s->c, R * (0.2f + 0.8f * e), ph, SVC_HALO, 1, kSvGold, SV_HALO_K * k * powf(1.0f - u, 1.5f) * FxSmooth(0.0f, 0.06f, u), 0.0f);
    }
    {   // v3.22.0 (user item 3b): a faster, thinner ring that travels further (two overlap: life > period)
        const float per = s->flare ? 0.6f * SV_THIN_S : SV_THIN_S, f2 = now / per + FxRnd(s->key * 0x2545F491u);
        for (m = 0; m < 2; ++m) {
            const float u = ((f2 - floorf(f2)) + (float)m) * per / SV_THIN_LIFE, e = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
            if (u >= 1.0f) continue;
            SvSprAdd(s->c, R * (SV_THIN_R0 + (SV_THIN_R1 - SV_THIN_R0) * e), ph + 1.3f, SVC_THIN, 1, kSvGold,
                     SV_THIN_K * k * powf(1.0f - u, 1.2f) * FxSmooth(0.0f, 0.05f, u), 0.0f);
        }
    }
}
// once a frame (GvPass, the scene pass, after SvTrackStep / SvEye / g_gvM): sources, particles, the sprite list
static void SvAuraFrame(const Viewport8 *vp, float now, float dt, int eyeOk)
{
    static int s_logged, s_scaleBad;
    static float s_t0 = -1.0f;
    const DWORD rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    int i;
    if (rk != g_svRoomKey) {  // a new stage / room: the pool and the per-source state start clean
        g_svRoomKey = rk;
        memset(g_svPt, 0, sizeof g_svPt);
        memset(g_svOwn, 0, sizeof g_svOwn);
        s_logged = 0;
        s_t0 = -1.0f;
    }
    g_svVp = vp;
    g_svSprN = g_svSprDrop = 0;
    g_svEyeOk = eyeOk;
    g_svUnit = (float)vp->h / 720.0f;
    SvAuraSources(now, dt);
    SvPtStep(now, dt);
    if (!SvSprScale(vp)) {
        if (!s_scaleBad++) Log("screen effects: Spirit Vision auras: no sprite scale from the camera matrix: auras skipped this frame");
        return;
    }
    for (i = 0; i < g_svSrcN; ++i) {  // nearest first: the budget drops the farthest
        if (g_svSrc[i].kind == SVS_HAUNT) SvHauntSprites(&g_svSrc[i], now);
        else SvHolySprites(&g_svSrc[i], now);
    }
    SvVeinsFrame(now, dt);  // v3.22.0: the haunting veins (svveins.inc)
    SvPtSprites(now);
    SvBloodFrame(now);      // v3.26.0: the blood-rooted ghost's aura and blood (svveins.inc; SpiritGhost=Blood), after the
                            // existing sprites so they keep their priority under SV_SPR_MAX (INSP326-012)
    if (!s_logged && g_svSrcN) {  // once a room, 2 s after the first source shows (the particles have filled in)
        if (s_t0 < 0.0f) {
            s_t0 = now;
        } else if (now - s_t0 >= 2.0f) {
            int nAdd = 0, nPt = 0;
            s_logged = 1;
            for (i = 0; i < g_svSprN; ++i) nAdd += g_svSpr[i].add;
            for (i = 0; i < SV_PT_MAX; ++i) nPt += g_svPt[i].live;
            Log("screen effects: Spirit Vision auras (stage %lX room %lX): %d source(s) (%d haunting(s), %d candle(s)%s), "
                "%d sprite(s) a pass (%d mist / ash, %d additive; %d over the budget of %d), %d particle(s) of %d",
                rk >> 16, rk & 0xFFFF, g_svSrcN, g_svHaunts, g_svCandles, g_svSrcN > g_svHaunts + g_svCandles ? ", the medallion" : "",
                g_svSprN, g_svSprN - nAdd, nAdd, g_svSprDrop, SV_SPR_MAX, nPt, SV_PT_MAX);
        }
    }
}
// each depth pass (after SvZ): the premultiplied batch, then the additive one; leaves SvUntextured's states
static int SvAuraDraw(void *dev)
{
    static SpVtx v[6 * SV_SPR_MAX];
    void *tex;
    int b, i, m, n, drawn = 0;
    if (!g_svSprN || (tex = FxTexture(dev, FXX_SVATLAS)) == NULL) return 0;
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    FxSampler(dev, tex, 0, 0);                    // clamp, linear
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
    for (b = 0; b < 2; ++b) {  // 0: premultiplied (texture x diffuse, the diffuse RGB x its alpha), 1: additive
        n = 0;
        for (i = 0; i < g_svSprN; ++i) {
            const SvSpr *s = &g_svSpr[i];
            const float u0 = 0.25f * (float)(s->cell & 3) + 0.5f / 256.0f, v0 = 0.25f * (float)(s->cell >> 2) + 0.5f / 256.0f;
            const float u1 = u0 + 0.25f - 1.0f / 256.0f, v1 = v0 + 0.25f - 1.0f / 256.0f;
            SpVtx q[4];
            DWORD col;
            if (s->add != b) continue;
            if (b) {
                const float k = s->a * g_svMul;
                col = 0xFF000000u | (DWORD)(FxSat(s->rgb[0] * k) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(s->rgb[1] * k) * 255.0f + 0.5f) << 8 |
                      (DWORD)(FxSat(s->rgb[2] * k) * 255.0f + 0.5f);
            } else {
                const float a = FxSat(s->a * g_svMul);
                col = (DWORD)(a * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(s->rgb[0]) * a * 255.0f + 0.5f) << 16 |
                      (DWORD)(FxSat(s->rgb[1]) * a * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(s->rgb[2]) * a * 255.0f + 0.5f);
            }
            for (m = 0; m < 4; ++m) {
                const float cx = (m & 1) ? 1.0f : -1.0f, cy = (m & 2) ? 1.0f : -1.0f;
                q[m].x = s->x + (cx * s->cs - cy * s->sn) * s->rx - 0.5f;
                q[m].y = s->y + (cx * s->sn + cy * s->cs) * s->ry - 0.5f;
                q[m].z = s->z;
                q[m].rhw = s->rhw;
                q[m].c = col;
                q[m].u = (m & 1) ? u1 : u0;
                q[m].v = (m & 2) ? v1 : v0;
            }
            v[n++] = q[0], v[n++] = q[1], v[n++] = q[2];
            v[n++] = q[1], v[n++] = q[3], v[n++] = q[2];
        }
        if (!n) continue;
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, b ? 2 /*ONE*/ : 6 /*INVSRCALPHA*/);
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
        drawn += n / 6;
    }
    SvUntextured(dev);  // the states the following draws (SvFlames, GvPass) expect: no texture, additive diffuse
    return drawn;
}

// ---- S8 soul flames at the chest bone (RE_spiritvision 4.2); also the sprite fallback's chest height
static int SvChestBone(int type)
{
    switch (type) {
    case 0x02: return 4;
    case 0x03: return 0;
    case 0x05: return 2;
    case 0x06: return 8;
    case 0x09: return -1;  // Tremer: the model root
    default: return 9;
    }
}
static float SvChestH(int type)  // above the model root (y up), without bones
{
    switch (type) {
    case 0x02: return 140.0f;
    case 0x03: return 23.0f;
    case 0x05: return 240.0f;
    case 0x06: return 160.0f;
    case 0x09: return 0.0f;
    case 0x0A: return 130.0f;
    case 0x0C: return 239.0f;
    case 0x0E: return 41.0f;   // Andrew: his origin is his pelvis
    default: return 252.0f;
    }
}
static int SvChest(BYTE *model, int type, float p[3])
{
    __try {
        const float *root = (const float *)(model + 0x10);
        const float *mats = *(const float *const *)(model + 0xC0);
        const BYTE *skel = *(const BYTE *const *)(model + 0xE0);
        const int b = SvChestBone(type);
        p[0] = root[0];
        p[1] = root[1] - SvChestH(type);
        p[2] = root[2];
        if (b >= 0 && mats && skel && b < *(const int *)(skel + 0x14)) {
            const float *m = mats + 16 * b + 12;
            if (_finite(m[0] + m[1] + m[2]) && fabsf(m[0] - root[0]) < 600.0f && fabsf(m[1] - root[1]) < 600.0f &&
                fabsf(m[2] - root[2]) < 600.0f)
                memcpy(p, m, 12);
        }
        return _finite(p[0] + p[1] + p[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
#include "spiritfire.inc"  // v3.28 FIRE: SvFlames, the spirit fires (was v3.17-v3.24's three teardrop quads; the player's
                           // v3.23.0 health size SvPlayerFireK is now every being's, spiritfire_sim.h)

// ---- S9 Thermal footprints (the player and the companions), drawn before Thermal's colour pass
typedef struct { float p[3], dx, dz, t0, sc; } TpPrint;
typedef struct { float last[3], prevY, pos[3], step, side_d, sc; int side, have; const BYTE *task; LONG seen; } TpWalker;
static TpPrint g_tp[TP_MAX];
static int g_tpNext, g_tpLogged;
static TpWalker g_tpW[3 + TP_ENEMIES];  // 0 player, 1 Eileen, 2 Cynthia (companions), then enemies by task
// 0x52AEB5 (Thermal): an enemy drawn this frame; walking types get a walker (the model root = its feet)
static void TpEnemySeen(const BYTE *task, const BYTE *model, int type)
{
    float p[3];
    int i, freeI = -1;
    if (type != 0x07 && type != 0x08 && type != 0x0A && type != 0x10) return;
    __try {
        memcpy(p, model + 0x10, 12);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (!_finite(p[0] + p[1] + p[2])) return;
    for (i = 3; i < 3 + TP_ENEMIES; ++i) {
        TpWalker *w = &g_tpW[i];
        if (w->task == task) break;
        if (freeI < 0 && (!w->task || g_fxFrame - w->seen > 120)) freeI = i;
    }
    if (i == 3 + TP_ENEMIES) {
        if (freeI < 0) return;
        i = freeI;
        memset(&g_tpW[i], 0, sizeof g_tpW[i]);
        g_tpW[i].task = task;
    }
    memcpy(g_tpW[i].pos, p, 12);
    g_tpW[i].seen = g_fxFrame;
    g_tpW[i].step = type == 0x0A ? 28.0f : TP_STEP;  // the Sniffer Dog: shorter, smaller paws
    g_tpW[i].side_d = type == 0x0A ? 7.0f : TP_SIDE;
    g_tpW[i].sc = type == 0x0A ? 0.65f : 1.0f;
}
static int TpPos(int k, float p[3])
{
    __try {
        const BYTE *t, *rec, *m;
        if (k == 0) {
            memcpy(p, (const void *)0x10A0F60, 12);
        } else {
            t = *(const BYTE *const *)(k == 1 ? 0xFCE6B4 : 0xFD5A50);
            rec = t ? *(const BYTE *const *)(t + 0x41C) : NULL;
            m = rec ? *(const BYTE *const *)(rec + 0x14) : NULL;
            if (!m) return 0;
            memcpy(p, m + 0x10, 12);
        }
        return _finite(p[0] + p[1] + p[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static void TpStep(float now)
{
    int k;
    for (k = 0; k < 3 + TP_ENEMIES; ++k) {
        TpWalker *w = &g_tpW[k];
        float p[3], dx, dz, d, gy;
        if (k < 3) {
            w->step = TP_STEP;
            w->side_d = TP_SIDE;
            w->sc = 1.0f;
        }
        // review SV171-001: TpStep runs at the frame's first character draw, before this frame's TpEnemySeen calls:
        // enemies seen in the previous frame (or this one) count (a one-frame lag)
        if (k < 3 ? !TpPos(k, p) : (!w->task || g_fxFrame - w->seen > 1)) {
            w->have = 0;
            continue;
        }
        if (k >= 3) memcpy(p, w->pos, 12);
        if (!w->have) {
            memcpy(w->last, p, 12);
            w->prevY = p[1];
            w->have = 1;
            w->side = 1;
            continue;
        }
        gy = fabsf(p[1] - w->prevY);
        w->prevY = p[1];
        dx = p[0] - w->last[0];
        dz = p[2] - w->last[2];
        d = sqrtf(dx * dx + dz * dz);
        if (d > 400.0f || gy >= 4.0f) {  // a jump / teleport / fall: start again here
            memcpy(w->last, p, 12);
            continue;
        }
        if (d >= w->step) {
            TpPrint *f = &g_tp[g_tpNext];
            const float ux = dx / d, uz = dz / d;
            g_tpNext = (g_tpNext + 1) % TP_MAX;
            f->p[0] = p[0] - uz * w->side_d * (float)w->side;
            f->p[1] = p[1];
            f->p[2] = p[2] + ux * w->side_d * (float)w->side;
            f->dx = ux;
            f->dz = uz;
            f->t0 = now;
            f->sc = w->sc;
            w->side = -w->side;
            memcpy(w->last, p, 12);
        }
    }
}
// v3.17.1: a collision wall between the camera and the print (the game's segment test 0x561CD0, as Item Shine's
// SpBlocked): the prints are drawn without depth (live: the depth buffer test hid them, as it hid Item Shine in v3.3.2)
static __declspec(align(16)) float g_tpSeg[8], g_tpPlane[4];
static int TpBlocked(const float *p)
{
    float a, b;
    if (!*(volatile DWORD *)0x12F6D74) return 0;  // no collision loaded
    g_tpSeg[0] = g_svEye[0], g_tpSeg[1] = g_svEye[1], g_tpSeg[2] = g_svEye[2], g_tpSeg[3] = 1.0f;
    g_tpSeg[4] = p[0], g_tpSeg[5] = p[1] - 6.0f, g_tpSeg[6] = p[2], g_tpSeg[7] = 1.0f;  // just above the floor
    __try {
        if (!((int(__cdecl *)(const float *, float *))0x561CD0)(g_tpSeg, g_tpPlane)) return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    a = g_tpPlane[0] * g_tpSeg[0] + g_tpPlane[1] * g_tpSeg[1] + g_tpPlane[2] * g_tpSeg[2] + g_tpPlane[3];
    b = g_tpPlane[0] * g_tpSeg[4] + g_tpPlane[1] * g_tpSeg[5] + g_tpPlane[2] * g_tpSeg[6] + g_tpPlane[3];
    if (!((a < 0.0f) != (b < 0.0f)) || a == b) return 0;
    return a / (a - b) < 0.93f;  // crossed well before the print (not the floor it lies on)
}
static int TpDraw(void *dev, const Viewport8 *vp, float now, const float kWhite[3])  // v3.22.0: the print colour
{
    enum { NS = 12 };
    int i, s, drawn = 0;
    for (i = 0; i < TP_MAX; ++i) {
        const TpPrint *f = &g_tp[i];
        const float age = now - f->t0;
        GvHv c, e[NS + 1];
        float k, cp[3];
        if (f->t0 <= 0.0f || age < 0.0f || age > TP_LIFE_S) continue;
        if (TpBlocked(f->p)) continue;
        k = TP_K * powf(1.0f - age / TP_LIFE_S, 1.5f);
        cp[0] = f->p[0], cp[1] = f->p[1] - 2.0f, cp[2] = f->p[2];
        SvPoint(&c, cp, k, kWhite);
        for (s = 0; s <= NS; ++s) {
            const float th = 6.2831853f * (float)s / NS, a = 0.5f * TP_L * f->sc * cosf(th), b = 0.5f * TP_W * f->sc * sinf(th);
            float p[3];
            p[0] = cp[0] + a * f->dx - b * f->dz;
            p[1] = cp[1];
            p[2] = cp[2] + a * f->dz + b * f->dx;
            SvPoint(&e[s], p, 0.35f * k, kWhite);  // a plateau, not a dot (live: 14 x 26 read as specks)
        }
        for (s = 0; s < NS; ++s) GvTri(dev, &c, &e[s], &e[s + 1], vp, NULL);
        ++drawn;
    }
    return drawn;
}
// v3.17.1: from 0x52AEB5 before the frame's first character model (the room is drawn by then; the characters drawn
// after cover the prints), so they are in the frame before Thermal's copy and the palette colours them.
static void ThermPrintsEarly(void)
{
    const float now = SvSecs();
    void *dev = DEV;
    FxSaved sv;
    int n;
    static int s_why;
    TpStep(now);
    if (!dev || !FxOnBackBuffer(dev)) {
        if (!(s_why & 1)) s_why |= 1, Log("screen effects: Thermal footprints: the render target at 0052AEB5 is not the back buffer");
        return;
    }
    if (!SvEye(g_svEye)) {
        if (!(s_why & 2)) s_why |= 2, Log("screen effects: Thermal footprints: no camera position");
        return;
    }
    if (!FxSave(dev, &sv, 0)) return;
    if (sv.vpSet) ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);  // the 3D viewport in force (FxSave widened it)
    if (!(s_why & 8)) {
        s_why |= 8;
        Log("screen effects: Thermal footprints: viewport at 0052AEB5 %lu,%lu %lux%lu z %g..%g", sv.vp.x, sv.vp.y, sv.vp.w, sv.vp.h,
            sv.vp.minZ, sv.vp.maxZ);
    }
    FxBase(dev);
    ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
    SvNearFar();
    SvUntextured(dev);
    SvZ(dev, 0);  // no depth: the characters drawn after cover the prints; walls: TpBlocked
    g_svMul = 1.0f;
    g_svPull = 0.0f;
    {
        static const float kWhite[3] = { 1.0f, 1.0f, 1.0f };  // white = hot under the v3.21 palette (fallback look)
        n = TpDraw(dev, &sv.vp, now, kWhite);
    }
    GvFlush(dev);
    g_svPull = 0.0f;
    SvZOff(dev);
    FxRestore(dev, &sv);
    if (n && !g_tpLogged++) Log("screen effects: Thermal: footprints drawn before the characters (%d now; depth %s)", n,
                                GV_DEPTH ? "visible part" : "off");
    if (!n && !(s_why & 4)) {
        int k, w = 0;
        for (k = 0; k < 3 + TP_ENEMIES; ++k) w += g_tpW[k].have;
        s_why |= 4;
        Log("screen effects: Thermal footprints: pass ran, none to draw yet (%d walker(s) tracked)", w);
    }
}

// ---- v3.22.0 Thermal v2 (staging/feedback_v322/PLAN.md section 2): after the world palette (FxPass(1), Thermal), the
// footprints in heat colours, then every hot body collected this frame redrawn through 0x55C650 with the override in
// heat mode (kPsHeat; per body c1 / c5 = its heat). The ghost glow's redraw (GvRedraw) is the model for this.
static void ThConsts(float h)
{
    float lr = 0.299f * TH_DETAIL, lg = 0.587f * TH_DETAIL, lb = 0.114f * TH_DETAIL;
    memset(g_thC, 0, sizeof g_thC);
    g_thC[0][0] = lr, g_thC[0][1] = lg, g_thC[0][2] = lb;
    g_thC[1][0] = g_thC[1][1] = g_thC[1][2] = TH_BODY * h;
    g_thC[2][0] = TH_R0, g_thC[2][1] = TH_G0, g_thC[2][2] = TH_B0;
    g_thC[3][0] = TH_RS * 0.25f, g_thC[3][1] = TH_GS * 0.25f, g_thC[3][2] = TH_BS * 0.25f;
    g_thC[4][0] = g_thC[4][1] = g_thC[4][2] = 1.0f;
    g_thC[5][3] = h * TH_ALPHA_K < 1.0f ? h * TH_ALPHA_K : 1.0f;  // a cooling body reddens first, then fades
}
static int ThRedraw(void *dev, FxSaved *sv, int *drawn)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    static const BYTE kTss[][2] = { { 1, 2 }, { 1, 3 }, { 1, 5 }, { 1, 6 } };
    static int s_logged;
    DWORD rs[4], tss[4];
    void *ib = NULL;
    UINT base = 0;
    int i, k, bad = 0;
    float c9Save[4], c12Save[4];
    *drawn = 0;
    if (!ThHeatOk() || !FxShader(dev, FXT_HEAT)) return 0;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) bad = 1;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, kTss[i][0], kTss[i][1], &tss[i]))) bad = 1;
    if (FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) {
        ib = NULL;
        bad = 1;
    }
    if (bad) {
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        ++g_thFaults;  // review V322-008: counts toward the fallback (3: the v3.21 look for the session)
        if (!s_logged++) Log("screen effects: Thermal: a device state query for the heat redraw failed (%d of 3 before the v3.21 look)", g_thFaults);
        return 0;
    }
    g_thPs = g_fxPs[FXT_HEAT];
    FxReapply(dev, sv);  // the game's states (its renderer sets per material only what it changes)
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, g_thVpOk ? &g_thVp : &sv->vp);  // the 3D viewport the bodies were drawn in
    sv->vpSet = 1;
    SmoothApply();  // EntitySmooth: the redraw on the smoothed pose, exactly as the stock draw
    {   // v3.24.0: the depth bias for the redraw: the game's c9 kept, restored after the loop
        g_thBiasOn = g_thZBias > 0.0f && SUCCEEDED(((FxPsc_t)VT(dev, FXV_GETVSC))(dev, 9, c9Save, 1)) &&
                     _finite(c9Save[0] + c9Save[1] + c9Save[2] + c9Save[3]) &&
                     SUCCEEDED(((FxPsc_t)VT(dev, FXV_GETVSC))(dev, 12, c12Save, 1));  // v3.29.1: c12 kept too
        memset(g_thBiasOut, 0xFF, sizeof g_thBiasOut);  // (a NaN pattern: the first draw call always biases)
        memset(g_thBiasOut12, 0xFF, sizeof g_thBiasOut12);
    }
    for (k = 0; k < 2 * g_thN; ++k) {  // v3.24.0: every living (solid) body first, then the cooling (blended) ones
        float gtOp;
        i = k % g_thN;
        gtOp = GtHeatOp(g_th[i].model);  // v3.58 GT358: Ghost Teleport's fade: her heat body blended at her opacity (1 = not fading)
        if ((g_th[i].heat * TH_ALPHA_K >= 1.0f && gtOp >= 1.0f) != (k < g_thN)) continue;
        if (!ThValid(&g_th[i])) continue;
        if (!(gtOp > 0.0f)) continue;  // v3.58 GT358: the warp's middle: she is gone, so is her heat
        ThConsts(g_th[i].heat);
        g_thOpaque = g_th[i].heat * TH_ALPHA_K >= 1.0f && gtOp >= 1.0f;  // v3.23.0: alive (or early in the cool-down): solid
        InterlockedExchange(&g_thMode, 1);
        __try {
            if (gtOp < 1.0f) GtHeatFade(g_th[i].model, gtOp);  // v3.58 GT358: a depth pre-pass, then the heat x her opacity
            else ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(g_th[i].model);
            ++*drawn;
        } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&g_thGtPass, 0);  // v3.58 GT358
            InterlockedExchange(&g_thMode, 0);
            ++g_thFaults;
            Log("screen effects: Thermal: a heat redraw faulted (code %08lX at %p)%s", g_gvFaultCode, g_gvFaultAt,
                g_thFaults >= 3 ? "; the heat redraw is off for this session (the v3.21 look instead)" : "");
            break;
        }
        InterlockedExchange(&g_thMode, 0);
    }
    SmoothRestore();
    if (g_thBiasOn) {
        ((FxPsc_t)VT(dev, FXV_SETVSC))(dev, 9, c9Save, 1);    // v3.24.0: the game's z row back
        ((FxPsc_t)VT(dev, FXV_SETVSC))(dev, 12, c12Save, 1);  // v3.29.1: and c12
    }
    g_thBiasOn = 0;
    ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
    if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
    for (i = 0; i < 4; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kTss[i][0], kTss[i][1], tss[i]);
    for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
    return 1;
}
static void ThHeatPass(void *dev)
{
    static int s_logs, s_lastN = -1, s_lastP = -1;
    static const float kPrint[3] = { TP_HEAT_R, TP_HEAT_G, TP_HEAT_B };
    FxSaved sv;
    float now;
    int prints = 0, bodies = 0, ok = 0;
    if (!ThermHeatLive() || !ThHeatOk()) return;
    if (!FxSave(dev, &sv, 6)) return;
    now = SvSecs();
    TpStep(now);  // footprints: the walkers' new prints (they were stepped before the first character in v3.21)
    if (SvEye(g_svEye)) {
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, g_thVpOk && g_thStamp == g_fxFrame ? &g_thVp : &sv.vp);
        sv.vpSet = 1;
        FxBase(dev);
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
        SvNearFar();
        SvUntextured(dev);
        SvZ(dev, 0);  // no depth (floor decals, KB 114); walls: TpBlocked; hot bodies are redrawn over them next
        g_svMul = 1.0f;
        g_svPull = 0.0f;
        prints = TpDraw(dev, g_thVpOk && g_thStamp == g_fxFrame ? &g_thVp : &sv.vp, now, kPrint);
        GvFlush(dev);
        SvZOff(dev);
    }
    if (g_thStamp == g_fxFrame && g_thN) {
        static DWORD s_diagRoom = 0xFFFFFFFFu;  // v3.29.1: one line per room: the draw calls the depth bias reached
        static int s_diagLines;
        const DWORD room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
        g_thDiagOn = room != s_diagRoom && s_diagLines < 64;
        g_thDiagN[0] = g_thDiagN[1] = g_thDiagN[2] = 0;
        FxBase(dev);
        ok = ThRedraw(dev, &sv, &bodies);
        if (g_thDiagOn && bodies) {
            s_diagRoom = room;
            ++s_diagLines;
            Log("screen effects: Thermal: stage %lu room %lX: %d bod%s redrawn in %ld draw call(s): %ld skinned (clip z c12), "
                "%ld rigid / other (c9)%s; depth bias %.2f per mille%s", room >> 16, room & 0xFFFF, bodies,
                bodies == 1 ? "y" : "ies", g_thDiagN[0] + g_thDiagN[1] + g_thDiagN[2], g_thDiagN[0], g_thDiagN[1],
                g_thDiagN[2] ? " (shader query failed on some)" : "", g_thZBias * 1000.0f,
                g_thRedrawZ ? "" : "; ThermalRedrawDepth=0: depth ignored (test)");
        }
        g_thDiagOn = 0;
        if (g_thZLogged == 1) {  // v3.23.0: logged here, not inside the draw-call override
            g_thZLogged = 2;
            Log("screen effects: Thermal: the heat redraw found ZENABLE %ld ZFUNC %ld at the draw call; it draws with Z on, "
                "GREATEREQUAL, no Z write, living bodies unblended", (long)g_thZSeen[0], (long)g_thZSeen[1]);
        }
    }
    FxRestore(dev, &sv);
    if (s_logs < 40 && (bodies != s_lastN || (prints > 0) != s_lastP)) {
        ++s_logs;
        s_lastN = bodies;
        s_lastP = prints > 0;
        Log("screen effects: Thermal: %d hot bod%s redrawn in heat colours (%d collected%s), %d footprint(s)", bodies,
            bodies == 1 ? "y" : "ies", g_thStamp == g_fxFrame ? g_thN : 0, ok || !g_thN ? "" : "; redraw not possible",
            prints);
    }
}

// The Ghost Vision filter (FxPass, every place: page preview, scene, overlay-picture frame). Edges always; the ghosts
// and their damage areas only in the scene pass of gameplay phase 0.
static int GvPass(void *dev, int where)  // v3.22.0: 1 = the edge world was drawn
{
    static const int kTech[3] = { FXT_GVLIFT, FXT_GVEDGE, FXT_GVGRADE };
    static int s_lastGhosts = -1, s_lastRings = -1, s_lastMode = -1, s_logs, s_svLogged, s_lastExtra = -1, s_svDepthLast;
    const float w = FxW(), h = FxH(), unit = h / 720.0f, du = 1.0f / w, dv = 1.0f / h;
    FxSaved sv;
    float c[FX_NPSC][4], off[4][2], fw = 0, fh = 0;
    void *frame = NULL;
    int i, pass;
    for (pass = 0; pass < 3; ++pass)
        if (!FxShader(dev, kTech[pass])) return 0;  // fail closed: a failed shader (logged) draws nothing of Ghost Vision
    if (FXF_GHOST != g_fxLastF || where != g_fxLastWhere) {
        g_fxLastF = FXF_GHOST;
        g_fxLastWhere = where;
        Log("screen effects: drawing Spirit Vision over %s (lift %.2f, edge %.1f px, grade %.1f px, halo %.2f, threshold %.3f; "
            "ghosts: %s; damage areas: %s)", kFxWhere[where], GV_LIFT, GV_EDGE_PX, GV_GRADE_PX, GV_MIX, GV_THRESH,
            where != 1 ? "not here" : !g_thermHooks ? "none (no 0052AEB5 hook)" : g_gvDipHooks ? "model glow" : "sprites",
            where != 1 ? "not here" : "auras (gameplay)");
    }
    if (!FxSave(dev, &sv, FX_NPSC)) return 0;
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);  // FVF XYZRHW | DIFFUSE | TEX4
    for (pass = 0; pass < 3; ++pass) {  // L (lift), E (gradient), G (grade): each on a fresh copy of the frame so far
        const int tech = kTech[pass];
        for (i = 0; i < 4; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, NULL);  // the copy is not bound while taken
        frame = ((void *(__cdecl *)(float *, float *))0x4036E0)(&fw, &fh);
        if (!frame) break;  // FX-004: nothing more is drawn
        memset(c, 0, sizeof c);
        memset(off, 0, sizeof off);
        if (pass == 0) {
            FxC(c, 0, GV_LIFT, GV_LIFT, GV_LIFT, 0);
        } else if (pass == 1) {
            const float d = GV_EDGE_PX * unit;
            off[0][0] = -d * du;
            off[1][0] = d * du;
            off[2][1] = -d * dv;
            off[3][1] = d * dv;
            FxC(c, 0, 1, 1, 1, 0);
            FxC(c, 1, 1, 1, 1, 1);
        } else {
            const float r = GV_GRADE_PX * unit;
            off[1][0] = -r * du;
            off[1][1] = -0.5f * r * dv;
            off[2][0] = r * du;
            off[2][1] = -0.5f * r * dv;
            off[3][1] = r * dv;
            FxC(c, 0, 0.5f, 0.5f, 0.5f, 0.5f);
            FxC(c, 1, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);
            FxC(c, 2, GV_MIX, GV_MIX, GV_MIX, GV_MIX);
            FxC(c, 3, -GV_THRESH, -GV_THRESH, -GV_THRESH, -GV_THRESH);
            FxC(c, 4, 1, 1, 1, 1);
            FxC(c, 5, GV_LINE_R, GV_LINE_G, GV_LINE_B, 1);
        }
        ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, FX_NPSC);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[tech]);
        for (i = 0; i < kFxPsTex[tech]; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, frame);
        FxQuad4(dev, off);
    }
    for (i = 0; i < 4; ++i) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)i, NULL);
    if (frame) PvFsMix(dev);  // v3.58 PV358: Low / Medium: the plain picture back over the edge world (the ghosts, auras and fires after it stay full)
    if (frame && where == 1 && !GvPhase0() && GvPeep() && g_gvStamp == g_fxFrame && g_svSoulN) {  // v3.24.0: the peephole:
        int k, flames = 0;                                                                           // soul fires only
        static int s_peepLogged;
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);
        sv.vpSet = 1;
        SvNearFar();
        g_svEyeOk = SvEye(g_svEye);
        for (k = 0; k < (GV_DEPTH ? 2 : 1); ++k) {
            SvUntextured(dev);
            SvZ(dev, GV_DEPTH ? k + 1 : 0);
            flames = SvFlames(dev, &sv.vp, SvSecs());
        }
        SvZOff(dev);
        SvUntextured(dev);
        if (!s_peepLogged++) Log("screen effects: Spirit Vision: the Room 302 peephole: %d soul fire(s) drawn", flames);
    }
    if (frame && where == 1 && GvPhase0()) {  // G3 / G4: gameplay only (cutscenes / examine frames: the edge world)
        const float pulse = FxPulse();
        int ghosts = 0, rings = 0, mode = 0, portals = 0, candles = 0, flames = 0;  // mode 0 none, 1 model glow, 2 sprites
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);  // the world -> GS matrix of this frame's camera (as Item Shine)
        if (GvLive() && g_gvStamp == g_fxFrame && g_gvN) {
            mode = GvRedraw(dev, &sv, &ghosts) ? 1 : 2;
            FxBase(dev);  // our states again (the redraw leaves the game's)
        }
        BG_RUN(BGC_ECHO, EchoPostDraw(dev, &sv, 3));  // v3.30 ECHO: the Echo double as her spirit (pale cyan glow); puts every state back (v3.35 FX335: BG_RUN)
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);  // the 3D viewport (Item Shine's projection uses it too)
        sv.vpSet = 1;
        {   // v3.17.0: portals, soul flames (v3.21.0: haunting / holy auras as sprites); hidden part dim, visible full (S3)
            static float s_last;
            const float now = SvSecs();
            float dt = now - s_last, eye[3];
            int pass, k, eyeOk;
            s_last = now;
            if (!(dt >= 0.0f) || dt > 0.1f) dt = 0.1f;
            SvNearFar();
            {
                static int s_vpLogged;
                if (!s_vpLogged++)
                    Log("screen effects: Spirit Vision: viewport at the scene pass %lu,%lu %lux%lu z %g..%g", sv.vp.x, sv.vp.y, sv.vp.w,
                        sv.vp.h, sv.vp.minZ, sv.vp.maxZ);
            }
            SvTrackStep(now, dt);
            eyeOk = SvEye(eye);
            if (!eyeOk) eye[0] = eye[1] = eye[2] = 0.0f;
            memcpy(g_svEye, eye, sizeof g_svEye);
            SvAuraFrame(&sv.vp, now, dt, eyeOk);  // v3.21.0: sources, particles, sprites (once; drawn in each pass)
            if (s_svLogged < 20 && GV_DEPTH + 1 != s_svDepthLast) {  // review SV-C1: the state actually used
                ++s_svLogged;
                s_svDepthLast = GV_DEPTH + 1;
                Log("screen effects: Spirit Vision: depth %s (the game's 2D depth: c2.z %g, c3.z %g)",
                    GV_DEPTH ? "on: hidden parts dim, visible parts full" : "off (GV_DEPTH 0)",
                    g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c2[2] : 1.0f / 16777216.0f,
                    g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c3[2] : 0.0f);
            }
            for (k = 0; k < (GV_DEPTH ? 2 : 1); ++k) {
                pass = GV_DEPTH ? k + 1 : 0;
                SvUntextured(dev);
                SvZ(dev, pass);
                portals = SvPortals(dev, &sv.vp, now);
                rings = g_svHaunts;
                candles = g_svCandles;
                if (g_svTest == 4 && pass != 1) {  // v3.17.1 floor probe: 9 squares at the feet, depth scale 1/128 .. 1/64
                    static const float kCol[9][3] = { { 1, 0, 0 }, { 1, 0.5f, 0 }, { 1, 1, 0 }, { 0.5f, 1, 0 }, { 0, 1, 0 },
                                                      { 0, 1, 1 }, { 0, 0.3f, 1 }, { 0.7f, 0, 1 }, { 1, 1, 1 } };
                    float pp[3];
                    int q;
                    memcpy(pp, (const void *)0x10A0F60, 12);
                    {   // between her and the camera: 120 in front of her, a row across the view; scale 1/256 .. 1/64
                        float dx = eye[0] - pp[0], dz = eye[2] - pp[2], l = sqrtf(dx * dx + dz * dz);
                        if (!(l > 1.0f)) dx = 0.0f, dz = 1.0f, l = 1.0f;
                        dx /= l, dz /= l;
                        for (q = 0; q < 9 && _finite(pp[0] + pp[1] + pp[2]); ++q) {
                            const float cx = pp[0] + dx * 120.0f + (-dz) * (40.0f * (float)q - 160.0f);
                            const float cz = pp[2] + dz * 120.0f + dx * (40.0f * (float)q - 160.0f), y = pp[1] - 1.0f;
                            const float c0[3] = { cx - 16.0f, y, cz - 16.0f }, c1[3] = { cx + 16.0f, y, cz - 16.0f },
                                        c2[3] = { cx - 16.0f, y, cz + 16.0f }, c3[3] = { cx + 16.0f, y, cz + 16.0f };
                            GvHv a, b, c, d;
                            g_svZProbe = powf(2.0f, (float)(q - 4) / 4.0f);  // x0.5 .. x2 of the exact depth
                            SvPoint(&a, c0, 1.0f, kCol[q]);
                            SvPoint(&b, c1, 1.0f, kCol[q]);
                            SvPoint(&c, c2, 1.0f, kCol[q]);
                            SvPoint(&d, c3, 1.0f, kCol[q]);
                            GvTri(dev, &a, &b, &d, &sv.vp, NULL);
                            GvTri(dev, &a, &d, &c, &sv.vp, NULL);
                            GvFlush(dev);
                        }
                    }
                    g_svZProbe = 0.0f;
                }
                if (g_svTest && g_svTest != 4) {  // the depth probe: the half under the floor and the part behind her must be dimmer
                    float pp[3];
                    memcpy(pp, (const void *)0x10A0F60, 12);
                    if (_finite(pp[0] + pp[1] + pp[2]))
                    {   // hidden pass blue, visible pass red: the screenshot shows which fragments each test passed
                        static const float kTest[2][2][3] = { { { 0.1f, 0.3f, 1.0f }, { 0.1f, 0.3f, 1.0f } },
                                                              { { 1.0f, 0.1f, 0.1f }, { 1.0f, 0.1f, 0.1f } } };
                        SvColumn(dev, &sv.vp, pp, 300.0f, pp[1] - SV_COL_DY, pp[1] + SV_COL_DY, 1, kTest[pass == 1 ? 0 : 1],
                                 1.0f / g_svMul, now, eye);
                    }
                }
                GvFlush(dev);
                SvAuraDraw(dev);  // premultiplied mist / ash, then additive; leaves SvUntextured's states
                SvVeinsDraw(dev);  // v3.22.0: the haunting veins (svveins.inc)
                flames = SvFlames(dev, &sv.vp, now);
            }
            BG_RUN(BGC_DP, DpPostDraw(dev, &sv.vp));  // v3.27.1 (PLAN 5.1): Death Presence as violet spirit light, after the lift / edge passes (v3.35 FX335: BG_RUN)
            SvZOff(dev);
            SvUntextured(dev);
        }
        if (mode == 2) {
            ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
            ghosts = GvSprites(dev, &sv.vp, pulse);
        }
        {
            const int extra = portals | candles << 8 | flames << 16 | g_svFitSeen << 24 | g_svGold << 25;
            if (s_logs < 120 && (ghosts != s_lastGhosts || rings != s_lastRings || mode != s_lastMode || extra != s_lastExtra)) {
                ++s_logs;
                s_lastGhosts = ghosts;
                s_lastRings = rings;
                s_lastMode = mode;
                s_lastExtra = extra;
                Log("screen effects: Spirit Vision: %d ghost(s) glowing (%s; %d collected), %d haunting aura(s) (snapshot %d, "
                    "%d tracked), %d portal(s), %d candle(s), %d soul flame(s)%s%s",
                    ghosts, mode == 1 ? "model glow" : mode == 2 ? "sprites" : "none", g_gvStamp == g_fxFrame ? g_gvN : 0,
                    rings, *(volatile int *)0x10A22C0, g_svTrN, portals, candles, flames,
                    g_svFitSeen ? ", Eileen's fit" : "", g_svGold ? ", medallion repelling" : "");
            }
        }
    }
    FxRestore(dev, &sv);
    return frame != NULL;
}

// ---------------------------------------------------------------- install (Init, before the Options tables)
static void FxTypeList(const char *key, BYTE *set, int on)  // "0B, 0C" (hex, 01..12)
{
    char buf[64];
    const char *p = buf;
    IniStr("ScreenEffects", key, buf, sizeof buf);
    while (*p) {
        char *end;
        const long v = strtol(p, &end, 16);
        if (end == p) {
            ++p;
            continue;
        }
        if (v >= 1 && v <= 0x12) set[v] = (BYTE)on;
        p = end;
    }
}
static int FxPatchGroup(const char *what, const DWORD *va, const BYTE (*stock)[5], const void *const *to, int n)
{
    int i, done, lost = 0;
    for (done = 0; done < n; ++done)
        if (!PokeBranch(va[done], 0xE8, to[done], 5)) break;
    if (done == n) return 1;
    for (i = 0; i < done; ++i)  // all or nothing: put back every call written so far
        if (!Poke(va[i], stock[i], 5) || memcmp((const void *)(ULONG_PTR)va[i], stock[i], 5)) {
            ++lost;
            Log("screen effects: %08lX could not be put back", va[i]);
        }
    Log("screen effects: a %s patch could not be written%s", what, lost ? "; some sites stay patched (they forward to the stock calls)" : "");
    return 0;
}
// Ghost Vision's model glow (RE_ghostvision 1.2 / 5, a third all-or-nothing group, after the Thermal group whose
// 0x52AEB5 hook collects the ghosts): the renderer's two draw calls `call [ecx+124h]` / `call [ecx+11Ch]` (6 bytes) ->
// `call hkGvDipUp / hkGvDip; nop`. Not installed -> the ghosts glow as sprites (GV-003), logged.
static void GvInstall(void)
{
    static const Site kCtx[] = {
        { 0x4292EE, { 0x50, 0xFF, 0x91, 0x24, 0x01, 0x00, 0x00, 0xEB, 0x47 }, 9 },                    // push eax; call [ecx+124h]; jmp 42933E
        { 0x429337, { 0x50, 0xFF, 0x91, 0x1C, 0x01, 0x00, 0x00, 0x8B, 0x44, 0x24, 0x38 }, 11 },       // push eax; call [ecx+11Ch]; mov eax,[esp+38h]
    };
    static const DWORD kVa[2] = { 0x4292EF, 0x429338 };
    static const BYTE kStock[2][6] = { { 0xFF, 0x91, 0x24, 0x01, 0x00, 0x00 }, { 0xFF, 0x91, 0x1C, 0x01, 0x00, 0x00 } };
    static const void *const kTo[2] = { hkGvDipUp, hkGvDip };
    int i, done, lost = 0;
    for (i = 0; i < 2; ++i)
        if (!SiteOk(&kCtx[i])) {
            Log("screen effects: %08lX is not stock (another mod?): Spirit Vision's ghosts glow as sprites (no model glow)", kVa[i]);
            return;
        }
    for (done = 0; done < 2; ++done)
        if (!PokeBranch(kVa[done], 0xE8, kTo[done], 6)) break;
    if (done < 2) {
        for (i = 0; i < done; ++i)  // all or nothing: put back what was written
            if (!Poke(kVa[i], kStock[i], 6) || memcmp((const void *)(ULONG_PTR)kVa[i], kStock[i], 6)) {
                ++lost;
                Log("screen effects: %08lX could not be put back", kVa[i]);
            }
        Log("screen effects: a Spirit Vision draw-call patch could not be written%s: its ghosts glow as sprites",
            lost ? "; a site stays patched (it forwards to the stock call)" : "");
        return;
    }
    g_gvDipHooks = 1;
    Log("screen effects: Spirit Vision draw-call override in place (004292EF / 00429338, active only during its ghost redraw)");
}
static void ScreenFxInstall(void)
{
    static const DWORD kVa[] = { 0x432FBE, 0x432F9E, 0x55AC29 };
    static const BYTE kStock[][5] = { { 0xE8, 0x7D, 0x40, 0x13, 0x00 }, { 0xE8, 0x9D, 0x40, 0x13, 0x00 }, { 0xA1, 0x94, 0x7D, 0x6E, 0x00 } };
    static const void *const kTo[] = { NoiseDraw, NoiseDraw, hkFxAfterPost };
    static const Site kCallees[] = {
        { 0x567040, { 0x83, 0xEC, 0x1C, 0x57, 0x8B, 0x7C, 0x24, 0x24 }, 8 },
        { 0x55A260, { 0xA1, 0x4C, 0x52, 0x5D, 0x00, 0xDB, 0x05, 0x4C, 0x52, 0x5D, 0x00 }, 11 },
        { 0x4036E0, { 0x83, 0xEC, 0x14, 0xA1, 0xE0, 0x8D, 0x6E, 0x00 }, 8 },
    };
    // Thermal (RE_thermal 6, fallback design): the call sites with their neighbours, and the callees
    static const DWORD kTVa[] = { 0x52AEB5, 0x52AF95, 0x4273A6, 0x47DFC3 };
    static const BYTE kTStock[][5] = { { 0xE8, 0x96, 0x17, 0x03, 0x00 }, { 0xE8, 0xC6, 0x18, 0x03, 0x00 },
                                       { 0xE8, 0x75, 0x0C, 0x00, 0x00 }, { 0xE8, 0xF8, 0xFE, 0xFF, 0xFF } };
    static const void *const kTTo[] = { hkThermDraw, hkThermShadow, hkThermAmbient, hkJasperFlames };
    static const Site kTSites[] = {
        { 0x52AEB4, { 0x53, 0xE8, 0x96, 0x17, 0x03, 0x00, 0x83, 0xC4, 0x04, 0x8B, 0x76, 0x28 }, 12 },  // push ebx; call; add esp,4; mov esi,[esi+28h]
        { 0x52AF94, { 0x50, 0xE8, 0xC6, 0x18, 0x03, 0x00, 0x8B, 0x0D, 0x30, 0x09, 0x0A, 0x01 }, 12 },  // push eax; call; mov ecx,[10A0930h]
        { 0x4273A1, { 0xE8, 0x2A, 0x0C, 0x00, 0x00, 0xE8, 0x75, 0x0C, 0x00, 0x00, 0x57, 0x56 }, 12 },
        { 0x47DFC1, { 0x8B, 0xC6, 0xE8, 0xF8, 0xFE, 0xFF, 0xFF, 0x5E, 0xE9 }, 9 },                       // mov eax,esi; call; pop esi; jmp
        { 0x55C650, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x24, 0x53, 0x8B, 0x5D, 0x08 }, 13 },
        { 0x55C860, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x54, 0x53, 0x8B, 0x5D, 0x08 }, 13 },
        { 0x428020, { 0xA1, 0x24, 0x62, 0x3D, 0x01, 0x8B, 0x0D, 0x28, 0x62, 0x3D, 0x01, 0x8B, 0x15, 0x2C, 0x62, 0x3D, 0x01 }, 17 },
        { 0x47DEC0, { 0x50, 0xE8, 0xCA, 0xD7, 0xFD, 0xFF, 0x8B, 0x80, 0xB4, 0x00, 0x00, 0x00 }, 12 },
    };
    const int n = (int)(sizeof kVa / sizeof kVa[0]);
    int i, k, v, legacyColour = 0;
    char buf[32];
    for (k = 0; k < FX_N; ++k) {  // [ScreenEffects] (unknown text = the game's look)
        IniStr("ScreenEffects", kFxIniKey[k], buf, sizeof buf);
        for (v = kFxCount[k] - 1; v >= 0 && _stricmp(buf, kFxName[k][v]); --v) {}
        if (v < 0 && k == FX_FILTER && !_stricmp(buf, "Noir")) {  // v3.18.0: Noir removed -> Night Vision Mono
            v = FXF_NVMONO;
            if (!g_iniFallback) WritePrivateProfileStringA("ScreenEffects", "Filter", kFxName[FX_FILTER][FXF_NVMONO], g_iniPath);
            Log("screen effects: [ScreenEffects] Filter=Noir (removed) is now Night Vision Mono");
        }
        if (v < 0 && k == FX_NTYPE && !_stricmp(buf, "Colour")) {  // v3.18.0: Colour is a Noise Colour now
            v = 1;  // Fine
            legacyColour = 1;
        }
        if (v < 0 && k == FX_NOISE && buf[0]) {  // v3.58 MN358: the screen word Ultra (typed by hand) reads as the ini's Very High
            const char *iw = OlIni(OLM_GRAIN_STRENGTH, buf);
            for (v = kFxCount[k] - 1; v >= 0 && _stricmp(iw, kFxName[k][v]); --v) {}
            if (v >= 0) Log("screen effects: [ScreenEffects] NoiseStrength=%s (the Options word) reads as %s", buf, kFxName[k][v]);
        }
        if (v < 0 && k == FX_FILTER && !_stricmp(buf, "Ghost Vision")) {  // v3.17.0: renamed Spirit Vision
            v = FXF_GHOST;
            if (!g_iniFallback) WritePrivateProfileStringA("ScreenEffects", "Filter", kFxName[FX_FILTER][FXF_GHOST], g_iniPath);  // SV-C2
            Log("screen effects: [ScreenEffects] Filter=Ghost Vision is now Spirit Vision%s", g_iniFallback ? "" : " (ini updated)");
        }
        g_fx[k] = v >= 0 ? v : kFxStock[k];
        if (v < 0 && buf[0])  // e.g. v3.15.0's Filter=Otherworld / High Contrast (review: read as None)
            Log("screen effects: [ScreenEffects] %s=%s is not a value of this version: %s", kFxIniKey[k], buf, kFxName[k][kFxStock[k]]);
    }
    {   // v3.21.0: the death fades (V321-001: clamped 0..600 s, 0 = instant)
        int c = IniInt("ScreenEffects", "ThermalCoolSeconds"), d = IniInt("ScreenEffects", "SoulFadeSeconds");
        g_thermCoolS = (float)(c < 0 ? 0 : c > 600 ? 600 : c);
        g_soulFadeS = (float)(d < 0 ? 0 : d > 600 ? 600 : d);
    }
    g_thermHeatOn = IniInt("ScreenEffects", "ThermalHeat") != 0;  // v3.22.0 (0 = the v3.21 look)
    {   // v3.26.0 PLAN 2.1: SpiritGhost = Blood (default) / White (the v3.23.0 white glow); read at start
        char gb[16];
        GetPrivateProfileStringA("ScreenEffects", "SpiritGhost", "Blood", gb, sizeof gb, g_iniPath);
        g_gvBloodOn = _stricmp(gb, "White") != 0;
        Log("screen effects: Spirit Vision ghosts: %s", g_gvBloodOn ? "blood-rooted (pale core, crimson ramp, aura, blood)" : "white glow (v3.23.0)");
    }
    {   // v3.24.0: the heat redraw's depth bias, per mille (hidden key; absent = 0.3; clamped 0..1; 0 = off)
        char zb[32];
        GetPrivateProfileStringA("ScreenEffects", "ThermalDepthBias", "", zb, sizeof zb, g_iniPath);
        if (zb[0]) {
            const double v = atof(zb);
            // inspection THERM-001: max 1; v3.29.1: up to TH_ZBIAS_TEST_MAX as a test value (a surface that close in
            // front of a body no longer hides it)
            g_thZBias = (float)((v < 0.0 || !(v == v)) ? 0.0 : v > TH_ZBIAS_TEST_MAX ? TH_ZBIAS_TEST_MAX : v) / 1000.0f;
            Log("screen effects: [ScreenEffects] ThermalDepthBias=%s: the heat redraw %s%s", zb,
                g_thZBias > 0.0f ? "is drawn nearer by that per mille" : "has no depth bias",
                g_thZBias > 0.001f ? " (above 1: a test value)" : "");
        }
    }
    // v3.29.1: hidden test key: 0 = the heat redraw ignores the depth buffer (bodies show through walls; settles whether
    // anything but the stock draw itself wins the depth test over a body)
    g_thRedrawZ = GetPrivateProfileIntA("ScreenEffects", "ThermalRedrawDepth", 1, g_iniPath) != 0;
    if (!g_thRedrawZ) Log("screen effects: [ScreenEffects] ThermalRedrawDepth=0: the heat redraw ignores depth (a test)");
    g_svTest = GetPrivateProfileIntA("ScreenEffects", "SpiritTest", 0, g_iniPath);
    g_dpProbe = GetPrivateProfileIntA("ScreenEffects", "DeathPresenceProbe", 0, g_iniPath);  // v3.27.2: hidden, not in the ini
    // v3.27.3: hidden: 1 = the fog's depth exact (re_depth.md, default), 0 = v3.27.2's x 1/128 (for comparison)
    g_dpZExact = GetPrivateProfileIntA("ScreenEffects", "DeathPresenceDepth", 1, g_iniPath) != 0;
    if (g_svTest == 2) g_svZProbe = 0.001f;
    if (g_svTest == 3) g_svZProbe = 1.0f;  // a sweep: halved every 2 s (SvNearFar), logged  // v3.17.0: hidden, not in the ini comments
    if (g_svTest) Log("screen effects: SpiritTest=1: Spirit Vision draws a test column around the player");
    if (legacyColour) {  // v3.18.0: NoiseType=Colour -> Fine + NoiseColour Full Colour (unless a colour is already set)
        if (g_fx[FX_NCOLOUR] == 0) g_fx[FX_NCOLOUR] = 2;
        if (!g_iniFallback) {
            WritePrivateProfileStringA("ScreenEffects", "NoiseType", kFxName[FX_NTYPE][1], g_iniPath);
            WritePrivateProfileStringA("ScreenEffects", "NoiseColour", kFxName[FX_NCOLOUR][g_fx[FX_NCOLOUR]], g_iniPath);
        }
        Log("screen effects: [ScreenEffects] NoiseType=Colour is now Fine with Noise Colour %s", kFxName[FX_NCOLOUR][g_fx[FX_NCOLOUR]]);
    }
    for (k = 0x0B; k <= 0x0F; ++k) g_thermGhost[k] = 1;  // ghosts / victims (RE_thermal 1.2)
    FxTypeList("ThermalGhost", g_thermGhost, 1);
    FxTypeList("ThermalNotGhost", g_thermGhost, 0);
    FxTypeList("ThermalCold", g_thermCold, 1);
    FxTypeList("SoulNone", g_svNoSoul, 1);  // v3.22.0 (user item 6): default 06, the Wheelchair (a ghost type to the user)
    for (i = 0; i < (int)(sizeof kCallees / sizeof kCallees[0]); ++i)
        if (!SiteOk(&kCallees[i])) {
            Log("screen effects: %08lX is not stock (another mod?): no Screen Effects", kCallees[i].va);
            return;
        }
    for (i = 0; i < n; ++i)
        if (memcmp((const void *)(ULONG_PTR)kVa[i], kStock[i], 5)) {
            Log("screen effects: %08lX is not the stock call (another mod?): no Screen Effects", kVa[i]);
            return;
        }
    if (!FxPatchGroup("Screen Effects", kVa, kStock, kTo, n)) {
        Log("screen effects: the game's own noise, no filters");
        return;  // g_fxOn stays 0: every hook forwards unchanged
    }
    g_fxOn = 1;
    Log("screen effects: hooks in place (noise 00432FBE / 00432F9E, after the post pass 0055AC29); Noise Strength %s, "
        "Noise Type %s, Noise Colour %s, Filter %s", kFxName[0][g_fx[0]], kFxName[1][g_fx[1]], kFxName[2][g_fx[2]], kFxName[3][g_fx[3]]);
    PpInstall();  // v3.43 PP343: Bloom, Colour Grading, Otherworld Lens (post.inc)
    {   // v3.18.0 N4: the page backdrop (0x526A53) and the auto-capture
        static const Site kPf = { 0x526A53, { 0xE8, 0xB8, 0xD5, 0xFF, 0xFF }, 5 };
        char pbuf[MAX_PATH];  // review SEFX3-003: a full path, not the 32-byte value buffer
        IniStr("ScreenEffects", "PreviewImage", pbuf, sizeof pbuf);
        if (!pbuf[0]) strcpy_s(pbuf, sizeof pbuf, "preview.dds");
        if (pbuf[1] == ':' || (pbuf[0] == '\\' && pbuf[1] == '\\')) strcpy_s(g_fxPrevPath, sizeof g_fxPrevPath, pbuf);
        else if (_snprintf_s(g_fxPrevPath, sizeof g_fxPrevPath, _TRUNCATE, "%s%s%s", g_dir,
                             g_dir[0] && g_dir[strlen(g_dir) - 1] == '\\' ? "" : "\\", pbuf) < 0) {
            g_fxPrevPath[0] = 0;
            Log("screen effects: [ScreenEffects] PreviewImage path too long: no preview picture");
        }
        g_fxPrevCapture = IniInt("ScreenEffects", "PreviewCapture") != 0;
        {   // v3.22.0 (item 3a): PreviewKey saves the next clean gameplay frame as the picture (None = off)
            char kb[32];
            IniStr("ScreenEffects", "PreviewKey", kb, sizeof kb);
            g_fxKeyVk = _stricmp(kb, "None") && kb[0] ? QsKey(kb, 0) : 0;
            if (g_fxKeyVk) Log("screen effects: PreviewKey %s saves the next clean gameplay frame as the preview picture", kb);
        }
        if (SiteOk(&kPf) && PokeBranch(0x526A53, 0xE8, hkFxPageFrame, 5)) g_fxPrevHook = 1;
        {   // v3.22.0 (review V322-003): the frozen copy's frame-start draws (optional: without them a filter changed
            // during a pause shows the old one until the game resumes)
            static const Site kCd[2] = { { 0x55AE31, { 0xE8, 0x8A, 0xFA, 0xFF, 0xFF }, 5 }, { 0x55AE44, { 0xE8, 0x77, 0xFA, 0xFF, 0xFF }, 5 } };
            int k, n = 0;
            for (k = 0; k < 2; ++k)
                if (SiteOk(&kCd[k]) && PokeBranch(kCd[k].va, 0xE8, hkFxCopyDraw, 5)) ++n;
            Log("screen effects: the paused scene follows a filter changed during the pause (%d of 2 frame-start sites)", n);
        }
        Log("screen effects: preview picture %s (%s); auto-capture %s; page backdrop hook %s", g_fxPrevPath,
            FxPrevExists() ? "present" : "none yet", g_fxPrevCapture ? "on" : "off", g_fxPrevHook ? "0x526A53" : "not in place (not stock)");
        PvInstall();  // v3.56 PV356: [ScreenEffects] PreviewLayers / PreviewScene (preview.inc)
    }
    for (i = 0; i < (int)(sizeof kTSites / sizeof kTSites[0]); ++i)
        if (!SiteOk(&kTSites[i])) {
            Log("screen effects: %08lX is not stock (another mod?): Thermal shows the heat palette only (ghosts stay, no "
                "enemy heat); Spirit Vision shows the edge world and the damage areas, no ghost glow", kTSites[i].va);
            return;
        }
    if (!FxPatchGroup("Thermal", kTVa, kTStock, kTTo, 4)) {
        Log("screen effects: Thermal shows the heat palette only; Spirit Vision no ghost glow");
        return;
    }
    g_thermHooks = 1;
    {   // v3.21.0: the trapped Cynthia's own draw (optional: Thermal works without it)
        static const Site kCyn = { 0x4EE375, { 0x56, 0xE8, 0xD5, 0xE2, 0x06, 0x00, 0x68, 0x00, 0x00, 0x00, 0x40 }, 11 };  // push esi; call; push 40000000h
        const int ok = SiteOk(&kCyn) && PokeBranch(0x4EE376, 0xE8, hkThermCyn, 5);
        Log("screen effects: Thermal: trapped Cynthia draw 004EE376 %s", ok ? "hooked" : "not stock: left alone");
    }
    {   // v3.24.0 (user item 3): the scene-character draw, for the Room 302 peephole Eileen's soul fire (optional)
        static const Site kPeep = { 0x52AF33, { 0x50, 0xE8, 0x17, 0x17, 0x03, 0x00, 0x83, 0xC4, 0x04, 0x8B, 0x7F, 0x28 }, 12 };  // push eax; call; add esp,4; mov edi,[edi+28h]
        g_peepHook = SiteOk(&kPeep) && PokeBranch(0x52AF34, 0xE8, hkPeepDraw, 5);
        Log("screen effects: Spirit Vision: scene-character draw 0052AF34 %s (the peephole Eileen's soul fire)",
            g_peepHook ? "hooked" : "not stock: left alone");
    }
    {
        char list[96];
        int o = 0;
        for (k = 1; k <= 0x12 && o < (int)sizeof list - 4; ++k)
            if (g_thermGhost[k]) o += sprintf_s(list + o, sizeof list - o, " %02X", k);
        if (!o) strcpy_s(list, sizeof list, " none");
        Log("screen effects: Thermal hooks in place (model 0052AEB5, shadow 0052AF95, ambient 004273A6, Jasper's flames "
            "0047DFC3); hidden types%s; enemy ambient %.2f", list, g_thermAmb);
    }
    GvInstall();
}

// Init, after SparkleInstall (items.inc): the scene pass needs its HUD hook (review FXV2-002: no whole-frame fallback)
static void ScreenFxReport(void)
{
    if (!g_fxOn) return;
    if (g_spInstalled)
        Log("screen effects: the scene filter runs at 004EFBFD (before Item Shine and the HUD)");
    else
        Log("screen effects: the HUD hook 004EFBFD is not in place (see items:): no filter in gameplay or examine frames "
            "(the Screen Effects page preview still works)");
}
#include "post.inc"  // v3.43 PP343: Bloom, Colour Grading, Otherworld Lens (the Screen Effects page's last three rows)
