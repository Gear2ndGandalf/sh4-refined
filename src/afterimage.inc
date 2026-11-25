// ================================================================ v3.29 AFTERIMAGE: the after-image stun "Volt Trail" (special round 3)
// v3.30 AS330: renamed After Shock (row, description, log lines; the ini keys keep their AfterImage* names). The row's
// value is the trail length: Off / Short (3 echoes over 0.2 s) / Medium (5 over 0.35 s = v3.29's On) / Long (8 over 0.6
// s); v3.29's "On" reads as Medium (gameplay.inc GpLegacyValue). The hidden AfterImageTrail key is retired (logged once
// when present). The Henry audit (staging/v330_feedback/w2_aftershock_henry_drops/PLAN.md section B): the body key is
// the model object + its file + its skeleton (the player's model object 0x10A0FE0 is inline in the player slot, so a live
// body switch keeps its pointer), and a stun keeps the enemy's last-swing serial (hp + 0x1C: 0x44F410's once-per-swing
// rule for the player's weapon still holding it).
// Plan: staging/special_mods/r3_afterimage/PLAN.md (the user's choice: Volt Trail; every other question the recommended
// answer). RE: r3_afterimage/re_afterimage.md, ../re_special.md (A Stun Gun, C3 pose redraw). Included by sh4windowmod.c
// after deathpresence_draw.inc. GOG v7 VAs. The pure state code (pose ring, lags, gates, contact, cooldown) is ai_sim.h.
// NO new exe patch: it runs inside Refined's own hooks (the tick 0x4EF982 QsTick, the scene pass 0x4EFBFD SparkleFrame,
// the model hook 0x52AEB5 ThermDrawC, the draw-call override 0x4292EF / 0x429338 GvOverride, the post-filter calls in
// FxPass), and reuses Death Presence's hit-record writer (DpInjectHit), enemy walk / gates (DpNext, DpTypeOk,
// DpUpdatesNow), sprite list and ribbons (DpSlAdd / DpSlDraw, DpRibbon / DpRibDraw) and camera projection (DpCamProj).
//
// While she moves, a chain of glowing electric copies of her (her own model drawn again additively with stored bone
// matrices: 5 echoes over 0.35 s by default) trails behind her, crackling with small arcs; standing still it folds back
// into her. An enemy the trail touches gets the Stun Gun's own hit (attack 0x2A injected as a hit record: each type plays
// its own taser reaction), a forked arc, a spark burst, a short blue shock on its body and the gun's zap 0x9C66. One stun
// per enemy, then 3 s and 2 s on its feet before the next. Off in Room 302 and outside gameplay phase 0.
// v3.32 AS332 (staging/v332_feedback/w2_aftershock/PLAN.md, DESIGN.md; the user's v3.31 test, item A):
//   - seen from behind: v3.29's lens gate faded every copy nearer the camera than her by 10-90 u, so with the camera
//     behind her (the copies between it and her) a walk lost most copies and a run all of them. Gone: a copy along the
//     view axis is moved sideways into a fan (AiFanShift: at least 0.5 x its distance along the axis, the side following
//     the trail's own side), and fades only when it would be over 3.3 x her size on screen (AiNearK, relative to her
//     depth; v3.31 120-300 u absolute). The contact keeps the true trail.
//   - the broken trail: the user's log stepped Long 8 -> 5 -> 3 copies within 0.5 s (3.3-3.9 ms a frame under dgVoodoo2:
//     ~0.42 ms a body redraw), and the capped spread drew copies 3, 6, 8 (Medium -> 3: 2, 4, 5): a gap at her and paired
//     copies. Also the straight-distance gate folded to 0 mid-trail on an about-turn and the stack fade blanked the copies
//     where she paused. Now the copies sit evenly along her PATH over the span (AiEchoAt; a capped trail = fewer copies
//     along the same path), shown by the path she has come since (AiGate, with an overlap fade inside her body); the
//     guard's caps are 4.5 ms (6-8 copies) / 3.0 ms (4-5); continuous streaks from her head, hands and feet along the path
//     tie the copies together.
//   - the aura: crackling arcs over her body (pushed in front of it toward the camera), tendrils leaping off her
//     silhouette, ground strikes while she moves, a soft blue-white corona along her outline, a faint bloom over her and
//     shed sparks, all scaled by a charge that follows her speed (0.3 standing .. 1 at a run, AiChargeStep); Thermal:
//     white-hot points instead (no additive light there).
//   Per frame at most: 8 body redraws (unchanged), 128 ribbons, ~150 sprites, one 64-slot ring walk and ~32 path lookups (64 steps each).
// v3.33 AS333 (staging/v333_feedback/w2_aftershock/PLAN.md, DESIGN.md; the user's v3.32 test, items 4 and 6):
//   - in line: the copies are no longer fanned out to the side from behind; they stay along her travelled path in every camera
//     (the hidden AfterImageFan now defaults to 0; 1..200 = v3.32's fan x pct / 100). The lens gate stays removed.
//   - the cloak: the corona sprites (60) and the bloom (3), the "blue orbs", are one skin-tight electric-blue cloak hugging her
//     body (ai_sim.h AiCloakBuild: per chain a soft fill as wide as her body there and two crackling edge lines on her outline,
//     a light crawling along them), sized from the active body (Henry is broader than Eileen: hip spacing + bone lengths),
//     strength by the charge (faint standing, stronger walking, strong running); drawn as its own ribbon batch (at most 32
//     ribbons, 17 points each). The arcs now crawl on the cloak and the tendrils leave from its outline (her real girth; v3.32
//     used Death Presence's stand-in radii, about half of it). Thermal: white-hot points crawl along her outline.
//   - the log (W0): trail length changes, guard cuts, the aura's level, the body size (LogV, rate-limited); ANOMALY when the
//     guard holds the trail under its setting for 5 s (v3.33 MERGE: a LogV line instead), a copy position is not finite,
//     the ring stalls while she is drawn.
//   Per frame at most: 8 body redraws (unchanged), 128 + 32 ribbons, ~90 sprites (the corona's 63 gone).
// v3.34 FX334 (staging/v334_feedback/w2_fx/DESIGN.md; the user's v3.33 QA item 6: keep the cloak, remove the hard edges):
//   the cloak's edge lines (a thin core + glow on her outline per chain, the bright outline lines of the user's screenshot)
//   are no longer drawn; the soft fill fades out at her silhouette (ai_sim.h AiCloakBuild). 6 strips a frame (v3.33: 28).
//   Thermal's white-hot points still crawl along her outline; the arcs, tendrils and strikes are unchanged.
// v3.35 FX335 (staging/v335_feedback/w2_fx/DESIGN.md; the user's v3.34 QA: "Go ahead and remove the blue glow from the player
//   on After Shock"): no cloak. The soft blue fill over her body is not built or drawn any more (6 strips a frame, ~66 quads,
//   7-10 us of build gone); her body is still measured (AiBodyMeasure: the arcs' girth). What stays on her is lightning, not
//   a glow: the thin white-cyan arcs crawling over her limbs, the tendrils off her outline, the ground strikes and the shed
//   sparks; the copies of the trail and the streaks; the stuns. Thermal keeps its white-hot points on her outline (the
//   outline is built only under Thermal). Also: the copies thin out by the effect budget (budget.inc: every other copy
//   fades, the trail keeps its length; the stun still uses every copy), the copies' glow pass, the heat pass and each enemy
//   type's shock pass are made ready at a room's first frame (AiWarm), and a slow frame names its part (AiSlowLog).
#include "ai_sim.h"
// v3.33 AS333: ai_sim.h's bone order (AIB_*) = kDpHerBone's (HB_*): the same numbers, checked one enum at a time
typedef char AiHbOrderCheck[(HB_HEAD == 0 && HB_PELVIS == 3 && HB_LSH == 4 && HB_LHIP == 10 && HB_RFOOT == 15 && HB_N == 16) ? 1 : -1];
typedef char AiAibOrderCheck[(AIB_HEAD == 0 && AIB_PELVIS == 3 && AIB_LSH == 4 && AIB_LHIP == 10 && AIB_RFOOT == 15 && AIB_LTOE == 16) ? 1 : -1];

#define AI_BONES      64        // matrices a pose sample holds at most (her Playable Eileen body: 36; vanilla Henry: 30)
#define AI_G0         0.55f     // the nearest echo's glow strength
#define AI_TEX_K      0.35f     // the glow: texture detail x colour x this ...
#define AI_FILL_K     0.45f     // ... + a flat fill x colour x this (kPsGvGlow: sat(2 t0 c0 + c1))
// (v3.32 AS332: AI_NEAR0 / AI_NEAR1, the 120-300 u near fade, are replaced by ai_sim.h AiNearK, relative to her depth)
#define AI_NV_K       0.7f      // night vision: the glow x this (its bloom lifts it again)
#define AI_ID         0x2A      // the Stun Gun's attack (item 0x32, kind 0x12: re_special A1)
#define AI_DMG        5.0f      // its damage x (0.85 + 0.3 r) and push (tables 0x63D530 / 0x63D534)
#define AI_PUSH       100.0f
#define AI_SND        0x9C66    // the Stun Gun's zap (0x5366DB / 0x5366FE)
#define AI_SND_GAP    0.35      // seconds between zap sounds at most
#define AI_SHOCK_S    0.6f      // the blue shock on a stunned body; 1.2 s on the knock-down types
#define AI_SHOCK_K_S  1.2f
#define AI_ZAP_S      0.25f     // the forked arc to a stunned enemy
#define AI_MOTE_N     48
#define AI_ZAP_N      8
#define AI_SHOCK_N    8
#define AI_MS_CAP     3.0f      // a frame of ours over this (CPU ms) for 30 frames in a row: fewer echoes for the room
#define AI_MS_CAP_LONG 4.5f     // v3.30 AS330: the Long trail's cap (8 redraws); the ladder is AiCapStep (8 -> 5 -> 3)
                                // v3.32 AS332: 2.0 / 3.0 -> 3.0 / 4.5 (dgVoodoo2: ~0.42 ms a redraw; the user's log hit 3 copies)
#define AI_STREAKS    5         // v3.32 AS332: the streaks (head, hands, feet) ...
#define AI_STREAK_PTS 17        // ... points along the path, her first
#define AI_STREAK_K   0.55f     // their core brightness

static const float kAiColNear[3] = { 0.805f, 0.9025f, 1.0f };  // the Stun Gun's light (0.70, 0.85, 1.00) 35 % to white
static const float kAiColTail[3] = { 0.25f, 0.35f, 1.0f };     // deep electric blue at the tail
static const float kAiColSv[3] = { 0.55f, 0.75f, 1.0f };       // Spirit Vision: pale spirit light
static const float kAiArcCol[3] = { 0.85f, 0.95f, 1.0f };      // arcs and motes: white-cyan
static const float kAiHotCol[3] = { 1.0f, 0.95f, 0.80f };      // Thermal: white-hot points
static const float kAiShockCol[3] = { 0.45f, 0.65f, 1.0f };
// (v3.34 FX334: kAiCkCore, the colour of the cloak's edge lines, is gone with them: the fill is kAiShockCol)
// v3.33 AS333: her 15 Death Presence edges' half extents (W across her body, D front to back; Playable Eileen / Henry; lat 1 the
// hip line, 2 the shoulder line, 0 round), from the cloak's measured presets: the arcs crawl ON the cloak and the tendrils leave
// from its outline (v3.32: kDpHerEdgeR, Death Presence's stand-in radii, about half her real girth, put both inside her body)
typedef struct { float W[2], D[2]; signed char lat; } AiEdgeCk;
static const AiEdgeCk kAiEdgeCk[15] = {
    { { 17, 18 }, { 20, 22 }, 2 }, { { 18, 28 }, { 16, 22 }, 2 }, { { 25, 36 }, { 19, 27 }, 2 },  // head-neck, neck-chest, chest-pelvis
    { { 11, 17 }, { 11, 17 }, 0 }, { { 11, 17 }, { 11, 17 }, 0 },                                  // neck-shoulders
    { { 8.5f, 16 }, { 8.5f, 16 }, 0 }, { { 7, 13.5f }, { 7, 13.5f }, 0 },                          // left upper arm, forearm
    { { 8.5f, 16 }, { 8.5f, 16 }, 0 }, { { 7, 13.5f }, { 7, 13.5f }, 0 },                          // right
    { { 28, 34 }, { 20, 27 }, 1 }, { { 28, 34 }, { 20, 27 }, 1 },                                  // pelvis-hips
    { { 15, 20 }, { 15, 20 }, 0 }, { { 10, 17.5f }, { 10, 17.5f }, 0 },                            // left thigh, shin
    { { 15, 20 }, { 15, 20 }, 0 }, { { 10, 17.5f }, { 10, 17.5f }, 0 } };                          // right
// the capsules of an echo's body (kDpHerBone indices, deathpresence_sprites.inc): spine, shoulders, arms, legs
static const BYTE kAiCap[7][2] = { { HB_PELVIS, HB_CHEST }, { HB_CHEST, HB_HEAD }, { HB_LSH, HB_RSH }, { HB_LSH, HB_LHAND },
                                   { HB_RSH, HB_RHAND }, { HB_LHIP, HB_LFOOT }, { HB_RHIP, HB_RFOOT } };

static int g_aiInstalled;           // the feature is in (g_aiReady: gameplay.inc, the menu row)
static volatile int g_aiFail;       // fail-closed: any fault
// v3.30 AS330: the trail is the row's value (g_aiLevel 1 Short, 2 Medium, 3 Long); the hidden AfterImageTrail key is retired
static const AiTrail *AiTrailNow(void)
{
    const int i = AiTrailIndex((int)g_aiLevel);
    return &kAiTrail[i >= 0 ? i : 1];
}
static int g_aiSound = 1, g_aiDry;  // hidden AfterImageSound, AfterImageDryRun (1: contacts logged, nothing injected)
static BYTE g_aiSkip[0x13];         // hidden AfterImageSkip: types never stunned ("0A, 0C", hex)
static double g_aiClock;            // game seconds: + 1 / [0x5D5248] per gameplay tick (the ring's stamps)
static AiRing g_aiRing;
static __declspec(align(16)) float g_aiMtx[AI_RING][AI_BONES][16];  // the ring's bone matrices (256 KB)
static int g_aiN, g_aiCopyN;        // her skeleton's bone count; matrices copied (the model file's count when larger)
static const BYTE *g_aiModelSeen;   // the model the ring belongs to
static const void *g_aiFileSeen, *g_aiSkelSeen;  // v3.30 AS330: ... with this model file and skeleton (a live body switch)
static DWORD g_aiRoomKey = 0xFFFFFFFFu;
static AiFoes g_aiFoes;
static int g_aiWasOn, g_aiStunsRoom, g_aiDryLogs, g_aiLogType;
static double g_aiLastSnd = -100.0;
static AiU32 g_aiSeed = 0xA17E5EEDu;
static int g_aiLogged[16];
// tick -> draw: the zaps and the shocked bodies
typedef struct { double t0; float a[3], b[3]; AiU32 seed; int burst; } AiZap;
static AiZap g_aiZap[AI_ZAP_N];
static int g_aiZapNext;
typedef struct { const BYTE *task; double t0, until; BYTE *model[2]; LONG stamp; int nm, warm; } AiShock;  // v3.35 FX335: + warm (zero light)
static AiShock g_aiShock[AI_SHOCK_N];
// 0x52AEB5 -> draw
static LONG g_aiHerFrame = -1, g_aiVpStamp = -1;
static Viewport8 g_aiVp;
static int g_aiVpOk;

static void AiClearFx(void)
{
    memset(g_aiZap, 0, sizeof g_aiZap);
    memset(g_aiShock, 0, sizeof g_aiShock);
}
static void AiFailNow(const char *where, DWORD code, const void *at)
{
    if (g_aiFail) return;
    g_aiFail = 1;
    g_aiReady = 0;  // the menu row says "not available"
    InterlockedExchange(&g_gvGlow, 0);
    InterlockedExchange(&g_thMode, 0);
    g_aiZ = 0;
    AiRingReset(&g_aiRing);
    AiClearFx();
    Log("after shock: %s faulted (code %08lX at %p): the feature is off for this session", where, code, at);
}
static DWORD g_aiFaultCode;
static void *g_aiFaultAt;
static int AiFaultNote(EXCEPTION_POINTERS *e)
{
    g_aiFaultCode = e->ExceptionRecord->ExceptionCode;
    g_aiFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
static const float *AiBone(const float (*mtx)[16], int hb) { return mtx[kDpHerBone[hb]] + 12; }  // row 3 = position

// ---------------------------------------------------------------- the tick (QsTick, 0x4EF982, game thread): the stun
// the 16 key bones of echo lag `lag` from the ring (0 = no such echo)
static volatile LONG g_aiQNan;  // v3.33 AS333: the "not finite" ANOMALY lines (copies, contact keys)
static int AiEchoKeys(double t, float out[HB_N][3])
{
    int i0, i1, b;
    float f;
    if (!AiRingAt(&g_aiRing, t, &i0, &i1, &f)) return 0;
    for (b = 0; b < HB_N; ++b) AiLerp3(out[b], AiBone((const float(*)[16])g_aiMtx[i0], b), AiBone((const float(*)[16])g_aiMtx[i1], b), f);
    if (!DpFinite3(out[HB_PELVIS])) {  // v3.33 AS333
        if (LogQuota(&g_aiQNan, 20))
            LogAnomaly("after shock", "a copy's contact pelvis is not finite (ring slots %d / %d, blend %.2f, %.3f s back): no contact "
                       "from it", i0, i1, f, g_aiClock - t);
        return 0;
    }
    return 1;
}
static void AiSound(void)
{
    DWORD vol, pan;
    float v;
    if (!g_aiSound || g_aiClock - g_aiLastSnd < AI_SND_GAP) return;
    if (((int(__cdecl *)(void))0x530040)()) return;  // as the stock block 0x53668B: no zap while this is set
    vol = *(volatile DWORD *)0x10A1D74;
    pan = *(volatile DWORD *)0x10A1D70;
    memcpy(&v, &vol, 4);
    if (*(volatile DWORD *)0xFD5A90 == 9 && *(volatile DWORD *)0xFD5A94 == 2 && v > 1.0f) {  // stage 9 room 2: at most 1
        v = 1.0f;
        memcpy(&vol, &v, 4);
    }
    ((int(__cdecl *)(int, DWORD, DWORD, int, int))0x56C9B0)(AI_SND, vol, pan, 0, 1);  // (cdecl, 5 args, as 0x5366FE)
    g_aiLastSnd = g_aiClock;
}
static void AiZapAdd(const float a[3], const float b[3])
{
    AiZap *z = &g_aiZap[g_aiZapNext];
    g_aiZapNext = (g_aiZapNext + 1) % AI_ZAP_N;
    z->t0 = g_aiClock;
    memcpy(z->a, a, sizeof z->a);
    memcpy(z->b, b, sizeof z->b);
    z->seed = AiHash(g_aiSeed++);
    z->burst = 0;
}
static AiShock *AiShockAdd(const BYTE *task, float secs)  // v3.35 FX335: returns the entry (the warm-up marks it)
{
    int i, old = 0;
    for (i = 0; i < AI_SHOCK_N; ++i) {
        if (g_aiShock[i].task == task || g_aiShock[i].until < g_aiClock) break;
        if (g_aiShock[i].until < g_aiShock[old].until) old = i;
    }
    if (i >= AI_SHOCK_N) i = old;
    memset(&g_aiShock[i], 0, sizeof g_aiShock[i]);
    g_aiShock[i].task = task;
    g_aiShock[i].t0 = g_aiClock;
    g_aiShock[i].until = g_aiClock + secs;
    g_aiShock[i].stamp = -1;
    return &g_aiShock[i];
}
static void AiStuns(float dt)
{
    const AiTrail *T = AiTrailNow();  // v3.30 AS330
    float eb[AI_ECHO_MAX][HB_N][3];
    int live[AI_ECHO_MAX], nLive = 0, k, stuns = 0, guard = 0;
    const BYTE *t = NULL;
    memset(live, 0, sizeof live);
    if (g_aiRing.count >= 2) {
        const int nw = AiRingNewest(&g_aiRing);
        if (g_aiClock - g_aiRing.stamp[nw] <= 0.1) {  // a ring the draw keeps filling (else no echo touches)
            static float cum[AI_RING];  // v3.32 AS332: the copies along her path, the path gate (as the look; no lens fan)
            AiRingCum(&g_aiRing, g_aiRing.pelvis[nw], cum);
            for (k = 0; k < T->n; ++k) {
                double te;
                float path;
                if (AiEchoAt(&g_aiRing, cum, g_aiClock, T->span, k + 1, T->n, &te, &path) && AiEchoKeys(te, eb[k]) &&
                    AiGate(path, AiDistXZ(eb[k][HB_PELVIS], g_aiRing.pelvis[nw])) >= AI_LIVE_K)
                    live[k] = 1, ++nLive;
            }
        }
    }
    while ((t = DpNext(t)) != NULL && guard++ < 96) {
        BYTE *w = *(BYTE *const *)(t + 0x408);
        float *hp, c[3], r, best = 1.0e9f, at[3], from[3];
        int type, cls, out, j, hitK;
        DWORD serial0;
        AiFoe *f;
        if (!w) continue;
        type = *(const int *)(w + 0x20);
        if (type < 0 || type >= 0x13 || !DpTypeOk(type) || !((r = AiTypeR(type)) > 0.0f) || g_aiSkip[type]) continue;
        cls = *(const int *)(w + 0x3C);
        hp = *(float *const *)(w + 0x34);
        if (!hp || !(hp[1] > 0.0f) || !_finite(hp[0])) continue;
        memcpy(c, w, sizeof c);  // the work position: the body centre (KB 138)
        if (!DpFinite3(c)) continue;
        f = AiFoeFind(&g_aiFoes, t, w);
        if (!f) continue;
        out = cls == 9 || cls == 0x0A || cls == 0x0B || cls >= 0x10;  // down (a downed ghost would get up faster), dead, scripted
        AiFoeStep(f, out, dt);
        if (out || !nLive || !(hp[0] > 0.0f) || !AiFoeReady(f) || stuns >= AI_STUNS_TICK) continue;
        // the nearest contact over every live echo's capsules and the trail lines (v3.30 AS330: ai_sim.h AiBestContact;
        // one contact per enemy and tick at any echo count: Long's 8 echoes never stun more often than Short's 3)
        hitK = AiBestContact(c, r, (const float(*)[AI_KEYS][3])eb, live, T->n, kAiCap, 7, HB_PELVIS, at, &best);
        if (hitK < 0) continue;
        if (!DpUpdatesNow(t)) continue;  // EntityGate: a gated monster only on its own update ticks (the record is read now)
        if (((int(__cdecl *)(const BYTE *))0x450F30)(t)) continue;  // a real hit (or Death Presence's blow) is queued this tick
        if (g_aiDry) {  // the first live test: contacts logged, nothing injected (the cooldown runs as if stunned)
            if (g_aiDryLogs++ < 200)
                Log("after shock (dry run): contact with type %02X (class %d, hp %.0f / %.0f) at %.0f u from echo %d's axis "
                    "(r %.0f + %.0f)", type, cls, hp[0], hp[1], best, hitK + 1, r, AI_CAP_R);
            AiFoeStunned(f);
            continue;
        }
        // the record's direction runs from the contact point to the enemy: DpInjectHit takes it from `from` to `pos`,
        // so `from` is the contact point mirrored through itself (2 at - c)
        for (j = 0; j < 3; ++j) from[j] = 2.0f * at[j] - c[j];
        // v3.30 AS330 (Henry audit): the enemy's last-hit serial stays as it was. 0x44F410 lets the player's weapon hit a
        // body once per swing (it skips an attack whose serial equals hp + 0x1C); a stun's fresh serial there let the same
        // swing of Henry's (or Playable Eileen's) melee weapon hit again after it. The record keeps its own serial (the
        // summary 0x44FAF0 groups by it); hp + 0x20 = 0x2A stays (the 3-10 s downs and the kill credit read it).
        serial0 = *(const DWORD *)((const BYTE *)hp + 0x1C);
        if (!DpInjectHit(t, hp, at, from, AI_ID, AI_DMG * (0.85f + 0.3f * AiRnd01(g_aiSeed++)), AI_PUSH)) {
            if (!g_aiLogged[0]++) Log("after shock: no hit record could be allocated (heap): the stun waits a tick");
            continue;
        }
        *(DWORD *)((BYTE *)hp + 0x1C) = serial0;
        AiFoeStunned(f);
        ++stuns, ++g_aiStunsRoom;
        AiZapAdd(eb[hitK][HB_CHEST], c);
        AiShockAdd(t, type == 0x07 || type == 0x08 || type == 0x11 ? AI_SHOCK_K_S : AI_SHOCK_S);
        AiSound();
        if (!(g_aiLogType >> type & 1)) {
            g_aiLogType |= 1 << type;
            Log("after shock: first stun on type %02X this room (class %d, hp %.0f / %.0f, %.0f u from echo %d's axis, %d stun(s) "
                "on it so far)", type, cls, hp[0], hp[1], best, hitK + 1, f->stuns);
        }
    }
}
static void AiTickC(void)
{
    const int rate = *(volatile int *)0x5D5248;
    DWORD roomKey;
    float dt;
    {   // v3.33 AS333: the row's value (the trail length) as the tick sees it change
        static int s_level = -1;
        static volatile LONG s_q, s_qBad;
        const int lv = (int)g_aiLevel;
        if (lv != s_level) {
            if (lv < 0 || lv > AI_TRAILS) {
                if (LogQuota(&s_qBad, 5)) LogAnomaly("after shock", "the row's value is %d (0..%d expected): the Medium trail is used", lv, AI_TRAILS);
            } else if (s_level >= 0 && LogQuota(&s_q, 200)) {
                const AiTrail *T = &kAiTrail[lv > 0 ? lv - 1 : 0];
                LogV("after shock: trail %s -> %s%s (%d copies over %.2f s)", kGpName[GP_AI][s_level > 0 && s_level <= AI_TRAILS ? s_level : 0],
                     kGpName[GP_AI][lv], lv ? "" : ": off, no copies", lv ? T->n : 0, lv ? T->span : 0.0f);
            }
            s_level = lv;
        }
    }
    if (g_aiLevel <= 0) {
        if (g_aiWasOn) {
            g_aiWasOn = 0;
            AiRingReset(&g_aiRing);
            AiFoesReset(&g_aiFoes);
            AiClearFx();
        }
        return;
    }
    g_aiWasOn = 1;
    if (*(volatile int *)0xFD5C20 != 0 || *(volatile int *)0xECB1B4 != 0) {  // pause, overlays, cut-scenes, movies: out
        AiRingReset(&g_aiRing);
        return;
    }
    if (rate < 1 || rate > 240 || !*(void *volatile *)0xFD5BA0) return;
    if (!DpFinite3((const float *)0x10A0F60)) return;
    roomKey = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (roomKey != g_aiRoomKey) {  // a new room: the trail, the cooldowns, the effects start clean
        g_aiRoomKey = roomKey;
        AiRingReset(&g_aiRing);
        AiFoesReset(&g_aiFoes);
        AiClearFx();
        g_aiLogType = 0;
        g_aiStunsRoom = 0;
    }
    if (*(volatile DWORD *)0xFD5A90 == 1) {  // Room 302 (stage 1): off (first person, no enemies; PLAN Q6)
        AiRingReset(&g_aiRing);
        return;
    }
    dt = 1.0f / (float)rate;
    g_aiClock += dt;
    g_aiFoes.clock = g_aiClock;
    AiStuns(dt);
}
static void AiTick(void)
{
    if (!g_aiInstalled || g_aiFail) return;
    __try {
        AiTickC();
    } __except (AiFaultNote(GetExceptionInformation())) {
        AiFailNow("the tick", g_aiFaultCode, g_aiFaultAt);
    }
}

// ---------------------------------------------------------------- 0x52AEB5 (ThermDrawC, every registry draw)
static void AiCollect(const BYTE *task, BYTE *model)
{
    int i;
    if (!g_aiInstalled || g_aiFail || g_aiLevel <= 0 || !model) return;
    if (g_aiVpStamp != g_fxFrame) {  // the first body of a frame: the 3D viewport in force
        void *dev = DEV;
        g_aiVpStamp = g_fxFrame;
        g_aiVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_aiVp)) && g_aiVp.w >= 16 && g_aiVp.h >= 16;
    }
    if (model == *(BYTE *const volatile *)(0x10A0F20 + 0x1B0)) g_aiHerFrame = g_fxFrame;  // her pose is final at the scene pass
    if (!task) return;
    for (i = 0; i < AI_SHOCK_N; ++i) {  // a shocked enemy's models of this frame (a Twin Victim has two)
        AiShock *s = &g_aiShock[i];
        if (s->task != task || !(s->until > g_aiClock)) continue;
        if (s->stamp != g_fxFrame) s->stamp = g_fxFrame, s->nm = 0;
        if (s->nm < 2) s->model[s->nm++] = model;
        return;
    }
}

// ---------------------------------------------------------------- the frame (scene pass): capture, echoes, motes
typedef struct { int k; float gain, alpha, heat, col[3], pel[3]; } AiEcho;  // (pel: the true place, before the lens fan)
static __declspec(align(16)) float g_aiEchoMtx[AI_ECHO_MAX][AI_BONES][16];
static AiEcho g_aiEcho[AI_ECHO_MAX];
static int g_aiEchoN, g_aiCap = AI_ECHO_MAX, g_aiHerOk, g_aiDrawOk;
static float g_aiHer[HB_N][3], g_aiEye[3];
static int g_aiEyeOk;
static float g_aiToe[2][3];          // v3.33 AS333: her toe joints (bones 24 left / 22 right: the cloak's feet)
static int g_aiToeOk;
// v3.32 AS332: this frame's path back to every ring slot, the lens fan (eye -> her, horizontal: forward F, across R), her
// view distance, the aura's charge, the streaks
static float g_aiCum[AI_RING], g_aiFanSide = 1.0f, g_aiFanF[3], g_aiFanR[3], g_aiHerDepth, g_aiCharge = AI_CHARGE_IDLE;
static int g_aiFanOk, g_aiStreakN;
static float g_aiFanK = 0.0f;  // hidden [Gameplay] AfterImageFan (%, 0..200): the lens fan's width x this; v3.33 AS333: default 0 =
                               // the copies in line along her path (v3.32: 100); any value > 0 brings v3.32's fan back
static float g_aiSpeed;        // v3.33 AS333: her speed (u/s) this frame, the aura's level (0 standing, 1 walking, 2 running) for the log
static int g_aiBand = -1, g_aiBandPend, g_aiBandChanges;
static double g_aiBandLogT = -100.0, g_aiStallT0 = -1.0;
static AiCutW g_aiCut = { -1.0, 0 };  // (AiCutReset at every room visit)
static AiBody g_aiBody = { 0.0f, 1.0f, 1.0f, AI_CK_HIPS_E };  // her body's size (AiAura, every frame)
static int g_aiBodyOk, g_aiCkRibs, g_aiCkPts;
static float g_aiStreakP[AI_STREAKS][AI_STREAK_PTS][3], g_aiStreakK[AI_STREAK_PTS], g_aiAuraAcc;
static const BYTE kAiStreakBone[AI_STREAKS] = { HB_HEAD, HB_LHAND, HB_RHAND, HB_LFOOT, HB_RFOOT };
static LONG g_aiFrameStamp = -1;
typedef struct { float p[3], v[3], size; double t0; float life; } AiMote;
static AiMote g_aiMote[AI_MOTE_N];
static int g_aiMoteNext;
static float g_aiMoteAcc;
static double g_aiLastFrameClock;
static int g_aiSlow;                 // consecutive frames over AI_MS_CAP
static float g_aiMsMax;              // the slowest frame of ours this room (scene + post-filter part)
static float g_aiMsFrame;
static LONG g_aiMsStamp = -1;
static DWORD g_aiVisit = 1, g_aiDrawRoom = 0xFFFFFFFFu;  // a room visit (the post-filter "broken" rule)
static int g_aiSprites;
static float g_aiKeepK[AI_ECHO_MAX] = { 1, 1, 1, 1, 1, 1, 1, 1 };  // v3.35 FX335: each copy's budget fade (1 drawn .. 0 thinned)
static int g_aiKeepN;                // v3.35 FX335: copies the budget lets through this frame (the log)
static int g_aiWarmNow;              // v3.35 FX335: AiRedraw draws the warm-up (her own pose, zero light)
static float g_aiMsPrep, g_aiMsRedraw, g_aiMsSpark;  // v3.35 FX335: this frame's parts (AiSlowLog)
static void AiCapture(const float pp[3])
{
    BYTE *m = *(BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    const float *mats, *pel;
    const BYTE *skel;
    int n, slot, broke;
    g_aiHerOk = 0;
    if (g_aiHerFrame != g_fxFrame || !m) return;  // her model was not drawn this frame
    if (*(const WORD *)(m + 0xE4) & 4) {          // first person (her body hidden): no trail
        AiRingReset(&g_aiRing);
        return;
    }
    mats = *(const float *const *)(m + 0xC0);
    skel = *(const BYTE *const *)(m + 0xE0);
    if (!mats || !skel) return;
    n = *(const int *)(skel + 0x14);
    if (n < 25 || n > AI_BONES) {
        if (!g_aiLogged[1]++) Log("after shock: her skeleton has %d bones (25..%d expected): no trail", n, AI_BONES);
        return;
    }
    // v3.30 AS330 (Henry audit): a new body = a new model object, model file, skeleton or bone count. The player's model
    // object is inline in the slot (0x10A0FE0, whoever the player is), so Playable Eileen's live switch (her 36-bone bodies,
    // a 30-bone Henry body) keeps the pointer: v3.29 saw only a bone-count change, and a same-count body with a larger
    // file count drew matrices the ring never filled
    if (m != g_aiModelSeen || n != g_aiN || *(const void *const *)(m + 0xB4) != g_aiFileSeen || (const void *)skel != g_aiSkelSeen) {
        int hdr = -1;
        const void *const fileP = *(const void *const *)(m + 0xB4);
        __try {
            const BYTE *file = (const BYTE *)fileP;
            if (file) hdr = *(const int *)(file + 0x0C);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hdr = -1;
        }
        g_aiCopyN = hdr >= n && hdr <= AI_BONES ? hdr : n;  // the palettes may index up to the file's count: copy those too
        g_aiDrawOk = 1;
        Log("after shock: her model %p (file %p, skeleton %p): %d bones (skeleton), %d in the model file, %d matrices kept a "
            "pose; ring %d x %.4f s", m, fileP, skel, n, hdr, g_aiCopyN, AI_RING, AI_SAMPLE_S);
        g_aiModelSeen = m;
        g_aiFileSeen = fileP;
        g_aiSkelSeen = skel;
        g_aiN = n;
        AiRingReset(&g_aiRing);
    }
    pel = mats + 16 * kDpHerBone[HB_PELVIS] + 12;
    if (!DpFinite3(pel) || fabsf(pel[0] - pp[0]) > 400.0f || fabsf(pel[1] - pp[1]) > 400.0f || fabsf(pel[2] - pp[2]) > 400.0f) return;
    {
        int b;
        for (b = 0; b < HB_N; ++b) memcpy(g_aiHer[b], mats + 16 * kDpHerBone[b] + 12, 12);
        g_aiHerOk = 1;
        memcpy(g_aiToe[0], mats + 16 * 24 + 12, 12);  // v3.33 AS333 (n >= 25: checked above)
        memcpy(g_aiToe[1], mats + 16 * 22 + 12, 12);
        g_aiToeOk = DpFinite3(g_aiToe[0]) && DpFinite3(g_aiToe[1]);
    }
    if (!AiRingDue(&g_aiRing, g_aiClock)) return;
    slot = AiRingPush(&g_aiRing, g_aiClock, pel, &broke);
    memcpy(g_aiMtx[slot], mats, (size_t)g_aiCopyN * 64);
    if (broke && !g_aiLogged[2]++) Log("after shock: the trail broke (a jump of her pelvis or a gap in game time): it starts again");
}
static void AiMoteAdd(const float p[3], float sp0, float sp1, float up0, float up1, float life0, float life1, float size, int sphere)
{
    AiMote *m = &g_aiMote[g_aiMoteNext];
    const float a = 6.2831853f * AiRnd01(g_aiSeed++), sp = sp0 + (sp1 - sp0) * AiRnd01(g_aiSeed++);
    if (g_aiClock - m->t0 < m->life) return;  // only expired slots are reused
    g_aiMoteNext = (g_aiMoteNext + 1) % AI_MOTE_N;
    memcpy(m->p, p, sizeof m->p);
    m->v[0] = cosf(a) * sp;
    m->v[2] = sinf(a) * sp;
    m->v[1] = sphere ? (AiRnd01(g_aiSeed++) - 0.5f) * 2.0f * sp : -(up0 + (up1 - up0) * AiRnd01(g_aiSeed++));  // Y is down
    m->t0 = g_aiClock;
    m->life = life0 + (life1 - life0) * AiRnd01(g_aiSeed++);
    m->size = size * (0.75f + 0.5f * AiRnd01(g_aiSeed++));
}
static int AiLookNow(void)  // 1 Thermal (its heat redraw after the palette), 3 Spirit Vision (after GvPass), 2 night vision, 0 the rest
{
    const int f = g_fxOn ? (int)g_fx[FX_FILTER] : 0;
    return f == FXF_THERMAL && ThHeatOk() ? 1 : f == FXF_GHOST ? 3 : (f == FXF_NV || f == FXF_NVMONO) ? 2 : 0;
}
// v3.32 AS332: the lens fan's shift (world, horizontal) of a copy whose true pelvis is `pel` (0 without a fan this frame)
static void AiFanVec(const float pel[3], float sh[3])
{
    const float vx = pel[0] - g_aiHer[HB_PELVIS][0], vz = pel[2] - g_aiHer[HB_PELVIS][2];
    const float d = g_aiFanOk ? g_aiFanK * AiFanShift(vx * g_aiFanF[0] + vz * g_aiFanF[2], vx * g_aiFanR[0] + vz * g_aiFanR[2], g_aiFanSide) : 0.0f;
    sh[0] = g_aiFanR[0] * d, sh[1] = 0.0f, sh[2] = g_aiFanR[2] * d;
}
// v3.32 AS332: the near fade of a (moved) copy at p: its view depth along the eye -> her ray against hers
static float AiNearAt(const float p[3])
{
    const float *h = g_aiHer[HB_PELVIS];
    if (!g_aiEyeOk || !(g_aiHerDepth > 1.0f)) return 1.0f;
    return AiNearK(((p[0] - g_aiEye[0]) * (h[0] - g_aiEye[0]) + (p[1] - g_aiEye[1]) * (h[1] - g_aiEye[1]) + (p[2] - g_aiEye[2]) * (h[2] - g_aiEye[2])) /
                       g_aiHerDepth, g_aiHerDepth);
}
static void AiFrame(const float pp[3])
{
    const AiTrail *T = AiTrailNow();  // v3.30 AS330
    // v3.32 AS332: a capped trail = fewer copies along the same path; v3.35 FX335: with the effect budget on, every copy of the
    // trail keeps its place and the budget fades some out (g_aiKeepK: the nearest and the tail stay, the rest evenly)
    const int n = g_bgOn ? T->n : T->n < g_aiCap ? T->n : g_aiCap;
    double dtg = g_aiClock - g_aiLastFrameClock, te[AI_ECHO_MAX];
    float path[AI_ECHO_MAX];
    int k, okE[AI_ECHO_MAX], fresh, tail = -1;
    (void)pp;
    g_aiFrameStamp = g_fxFrame;
    g_aiLastFrameClock = g_aiClock;
    if (!(dtg >= 0.0) || dtg > 0.1) dtg = 0.0;
    g_aiEyeOk = SvEye(g_aiEye);
    g_aiEchoN = 0;
    g_aiStreakN = 0;
    g_aiFanOk = 0;
    // v3.32 AS332: the path back along the ring (a stalled ring draws no copies: no stale pose piles up), her charge
    fresh = g_aiHerOk && AiRingFresh(&g_aiRing, g_aiClock);
    if (fresh) AiRingCum(&g_aiRing, g_aiHer[HB_PELVIS], g_aiCum);
    g_aiSpeed = fresh ? AiRingSpeed(&g_aiRing, g_aiCum, g_aiClock) : 0.0f;  // v3.33 AS333: kept for the log
    g_aiCharge = AiChargeStep(g_aiCharge, g_aiSpeed, (float)dtg);
    {   // v3.35 FX335: the budget's copies (budget_sim.h BgAiCopies / BgAiKeep), each fading in / out over 0.35 s of game time
        const int m = g_bgOn ? BgAiCopies(BgLevel(), n) : n;
        g_aiKeepN = m;
        for (k = 0; k < AI_ECHO_MAX; ++k)
            g_aiKeepK[k] = k < n ? BgRamp(g_aiKeepK[k], BgAiKeep(k + 1, n, m) ? 1.0f : 0.0f, (float)dtg, 1.0f / 0.35f) : 1.0f;
    }
    if (g_aiHerOk && !fresh && g_aiRing.count >= 2) {  // v3.33 AS333: she is drawn, so every due sample is taken: never stale
        static volatile LONG s_q;
        static DWORD s_visit;
        if (g_aiStallT0 < 0.0) g_aiStallT0 = g_aiClock;
        if (g_aiClock - g_aiStallT0 > 2.0 && s_visit != g_aiVisit && LogQuota(&s_q, 20)) {
            s_visit = g_aiVisit;
            LogAnomaly("after shock", "her model is drawn but the pose ring has not been filled for %.1f s (newest sample %.2f s old, %d "
                       "samples): no copies", g_aiClock - g_aiStallT0, g_aiClock - g_aiRing.stamp[AiRingNewest(&g_aiRing)], g_aiRing.count);
        }
    } else {
        g_aiStallT0 = -1.0;
    }
    if (g_aiHerOk) {  // v3.33 AS333: the aura's level (standing / walking / running) for the log: a change at most every 5 s
        static volatile LONG s_q;
        static const char *const kBand[3] = { "standing (faint)", "walking (stronger)", "running (strong)" };
        const int band = AiAuraBand(g_aiBand, g_aiCharge);
        if (band != g_aiBand) g_aiBand = band, g_aiBandPend = 1, ++g_aiBandChanges;
        if (g_aiBandPend && g_aiClock - g_aiBandLogT >= 5.0 && LogQuota(&s_q, 1500)) {
            LogV("after shock: aura %s: charge %.2f, speed %.0f u/s; no glow on her body, arcs and sparks only (body build %.2f, radii "
                 "x%.2f); %d level change(s) since the last line", kBand[g_aiBand], g_aiCharge, g_aiSpeed, g_aiBody.build, g_aiBody.sizeK,
                 g_aiBandChanges);  // v3.35 FX335: no cloak (v3.34: "soft cloak N strips")
            g_aiBandPend = 0, g_aiBandChanges = 0, g_aiBandLogT = g_aiClock;
        }
    }
    if (fresh && g_aiEyeOk && g_aiFanK > 0.0f) {  // the fan's frame: forward = eye -> her (horizontal), across = its perpendicular
        // (v3.33 AS333: only with AfterImageFan > 0; the default keeps the copies in line)
        const float *h = g_aiHer[HB_PELVIS];
        const float fx = h[0] - g_aiEye[0], fy = h[1] - g_aiEye[1], fz = h[2] - g_aiEye[2], l = sqrtf(fx * fx + fz * fz);
        g_aiHerDepth = sqrtf(fx * fx + fy * fy + fz * fz);
        if (l > 30.0f) {  // (a camera right above her: no fan)
            g_aiFanF[0] = fx / l, g_aiFanF[1] = 0.0f, g_aiFanF[2] = fz / l;
            g_aiFanR[0] = -g_aiFanF[2], g_aiFanR[1] = 0.0f, g_aiFanR[2] = g_aiFanF[0];
            g_aiFanOk = 1;
        }
    }
    for (k = 1; k <= n; ++k) {  // each copy's moment along her path
        okE[k - 1] = fresh && AiEchoAt(&g_aiRing, g_aiCum, g_aiClock, T->span, k, n, &te[k - 1], &path[k - 1]);
        if (okE[k - 1]) tail = k - 1;
    }
    if (g_aiFanOk && tail >= 0) {  // the fan's side follows the tail's own (true) side
        int i0, i1;
        float f, p[3];
        if (AiRingAt(&g_aiRing, te[tail], &i0, &i1, &f)) {
            const float *h = g_aiHer[HB_PELVIS];
            AiLerp3(p, g_aiRing.pelvis[i0], g_aiRing.pelvis[i1], f);
            g_aiFanSide = AiFanSideStep(g_aiFanSide, (p[0] - h[0]) * g_aiFanR[0] + (p[2] - h[2]) * g_aiFanR[2],
                                        (p[0] - h[0]) * g_aiFanF[0] + (p[2] - h[2]) * g_aiFanF[2], (float)dtg);
        }
    }
    for (k = 1; k <= n; ++k) {
        const float u = n > 1 ? (float)(k - 1) / (float)(n - 1) : 0.0f;
        int i0, i1, b, j;
        float f, gate, nr, fl, sh[3], dp[3];
        AiEcho *E = &g_aiEcho[g_aiEchoN];
        if (!okE[k - 1] || !AiRingAt(&g_aiRing, te[k - 1], &i0, &i1, &f)) continue;
        for (b = 0; b < g_aiCopyN; ++b)
            for (j = 0; j < 16; ++j) g_aiEchoMtx[g_aiEchoN][b][j] = g_aiMtx[i0][b][j] + (g_aiMtx[i1][b][j] - g_aiMtx[i0][b][j]) * f;
        memcpy(E->pel, AiBone((const float(*)[16])g_aiEchoMtx[g_aiEchoN], HB_PELVIS), sizeof E->pel);
        if (!DpFinite3(E->pel)) {  // v3.33 AS333
            if (LogQuota(&g_aiQNan, 20))
                LogAnomaly("after shock", "copy %d of %d has a non-finite position (ring slots %d / %d, blend %.2f, %.3f s back, path %.0f u): "
                           "not drawn", k, n, i0, i1, f, g_aiClock - te[k - 1], path[k - 1]);
            continue;
        }
        gate = AiGate(path[k - 1], AiDistXZ(E->pel, g_aiHer[HB_PELVIS]));  // (v3.31: AiMoveK, the lens gate, the stack fade)
        AiFanVec(E->pel, sh);
        for (j = 0; j < 3; ++j) dp[j] = E->pel[j] + sh[j];
        nr = AiNearAt(dp);
        fl = AiFlicker(g_aiClock, k);
        E->k = k;
        E->gain = AI_G0 * AiFall(k, n) * gate * nr * fl * g_aiKeepK[k - 1];  // v3.35 FX335: x the budget's fade
        E->alpha = 0.60f * AiFall(k, n) * gate * nr * (0.85f + 0.15f * fl) * g_aiKeepK[k - 1];  // Thermal: residual heat
        E->heat = 0.8f - 0.55f * u;
        for (j = 0; j < 3; ++j) E->col[j] = kAiColNear[j] + (kAiColTail[j] - kAiColNear[j]) * u;
        if (E->gain < 0.004f) continue;
        if (sh[0] != 0.0f || sh[2] != 0.0f)  // the fan: the whole copy moved aside (row 3 of every bone matrix)
            for (b = 0; b < g_aiCopyN; ++b) g_aiEchoMtx[g_aiEchoN][b][12] += sh[0], g_aiEchoMtx[g_aiEchoN][b][14] += sh[2];
        ++g_aiEchoN;
    }
    if (fresh) {  // v3.32 AS332: the streaks: her head, hands and feet along her path, the same gate, fan and near fade
        const int J = AI_STREAK_PTS - 1;
        int j, s;
        for (s = 0; s < AI_STREAKS; ++s) memcpy(g_aiStreakP[s][0], g_aiHer[kAiStreakBone[s]], 12);
        g_aiStreakK[0] = 0.0f;
        for (j = 1; j <= J; ++j) {
            double ts;
            float ps, f, pel[3], sh[3], dp[3];
            int i0, i1, q;
            if (!AiEchoAt(&g_aiRing, g_aiCum, g_aiClock, T->span, j, J, &ts, &ps) || !AiRingAt(&g_aiRing, ts, &i0, &i1, &f)) break;
            AiLerp3(pel, g_aiRing.pelvis[i0], g_aiRing.pelvis[i1], f);
            AiFanVec(pel, sh);
            for (q = 0; q < 3; ++q) dp[q] = pel[q] + sh[q];
            g_aiStreakK[j] = AI_STREAK_K * AiGate(ps, AiDistXZ(pel, g_aiHer[HB_PELVIS])) * powf(1.0f - (float)j / (float)(J + 1), 1.2f) * AiNearAt(dp);
            for (s = 0; s < AI_STREAKS; ++s) {
                const int bi = kDpHerBone[kAiStreakBone[s]];
                for (q = 0; q < 3; ++q) g_aiStreakP[s][j][q] = g_aiMtx[i0][bi][12 + q] + (g_aiMtx[i1][bi][12 + q] - g_aiMtx[i0][bi][12 + q]) * f + sh[q];
            }
        }
        g_aiStreakN = j;
    }
    if (g_aiHerOk) {  // v3.32 AS332: the aura's shed sparks: 4 a second standing .. 22 at a run, from points on her body
        g_aiAuraAcc += (float)dtg * 22.0f * AiSmooth(AI_CHARGE_IDLE - 0.12f, 1.0f, g_aiCharge);
        if (g_aiAuraAcc > 4.0f) g_aiAuraAcc = 4.0f;
        while (g_aiAuraAcc >= 1.0f) {
            const int e = (int)(AiHash(g_aiSeed++) % 15u);
            const float s = AiRnd01(g_aiSeed++);
            float p[3];
            int q;
            g_aiAuraAcc -= 1.0f;
            for (q = 0; q < 3; ++q) p[q] = g_aiHer[kDpHerEdge[e][0]][q] + (g_aiHer[kDpHerEdge[e][1]][q] - g_aiHer[kDpHerEdge[e][0]][q]) * s;
            AiMoteAdd(p, 30.0f, 90.0f, 10.0f, 50.0f, 0.15f, 0.30f, 1.8f, 1);
        }
    } else {
        g_aiAuraAcc = 0.0f;
    }
    {   // afterglow motes from the hands and feet of the two newest echoes (an accumulator: the same count a second at any rate)
        static const BYTE kEnd[4] = { HB_LHAND, HB_RHAND, HB_LFOOT, HB_RFOOT };
        const int src = g_aiEchoN < 2 ? g_aiEchoN : 2;
        float w = 0.0f;
        for (k = 0; k < src; ++k) w += g_aiEcho[k].gain / AI_G0;
        g_aiMoteAcc += (float)dtg * 40.0f * (w > 1.0f ? 1.0f : w);
        if (g_aiMoteAcc > 8.0f) g_aiMoteAcc = 8.0f;
        while (src && g_aiMoteAcc >= 1.0f) {
            const int e = (int)(AiRnd01(g_aiSeed++) * (float)src) % src, b = kEnd[AiHash(g_aiSeed++) & 3];
            g_aiMoteAcc -= 1.0f;
            AiMoteAdd(AiBone((const float(*)[16])g_aiEchoMtx[e], b), 60.0f, 140.0f, 20.0f, 60.0f, 0.18f, 0.35f, 2.4f, 0);
        }
        if (!src) g_aiMoteAcc = 0.0f;
    }
    for (k = 0; k < AI_ZAP_N; ++k) {  // a new stun: 12 motes burst at the enemy
        AiZap *z = &g_aiZap[k];
        int q;
        if (z->burst || !(z->t0 > 0.0) || g_aiClock - z->t0 > AI_ZAP_S) continue;
        z->burst = 1;
        for (q = 0; q < 12; ++q) AiMoteAdd(z->b, 80.0f, 200.0f, 0.0f, 0.0f, 0.25f, 0.45f, 3.0f, 1);
    }
}

// ---------------------------------------------------------------- the redraw: her model on a stored pose (re_special C3)
// Pointer swap: +0xC0 = the echo's matrices, +0xE0 = NULL (0x55C650 then skips the rebuild 0x418440, so Playable Eileen's
// wrapper on 0x55C794 does not run a second time), restored on every path. The draw-call override does the rest: glow
// mode (ONE / ONE, kPsGvGlow) with the after-image's depth rule (g_aiZ: ZENABLE, GREATEREQUAL, no z write), or Thermal's
// heat mode (kPsHeat, its own depth rule).
static int AiDrawSwapped(BYTE *m, float (*mtx)[16], int heat)
{
    void *const c0 = *(void **)(m + 0xC0), *const e0 = *(void **)(m + 0xE0);
    int ok = 1;
    __try {
        __try {
            *(void **)(m + 0xC0) = mtx;
            *(void **)(m + 0xE0) = NULL;
            if (heat) {
                InterlockedExchange(&g_thMode, 1);
            } else {
                g_aiZ = 1;
                InterlockedExchange(&g_gvGlow, 1);
            }
            ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(m);
        } __finally {
            InterlockedExchange(&g_gvGlow, 0);
            InterlockedExchange(&g_thMode, 0);
            g_aiZ = 0;
            *(void **)(m + 0xC0) = c0;
            *(void **)(m + 0xE0) = e0;
        }
    } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
    }
    return ok;
}
static void AiGlowConsts(const float col[3], float g)
{
    int j;
    for (j = 0; j < 3; ++j) {
        g_gvGlowC[0][j] = FxSat(0.5f * AI_TEX_K * g * col[j]);  // 2 t0 c0 = t0 x TEX_K x g x col
        g_gvGlowC[1][j] = FxSat(AI_FILL_K * g * col[j]);
    }
    g_gvGlowC[0][3] = 0.5f;  // alpha = t0.a (the game's alpha test keeps cut-out hair)
    g_gvGlowC[1][3] = 0.0f;
    g_gvGlowCN = 2;
    g_gvGlowPs = g_fxPs[FXT_GVGLOW];
}
static int AiRedraw(void *dev, FxSaved *sv, const Viewport8 *vp, int look, int *bodies)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    static const BYTE kTss[][2] = { { 1, 2 }, { 1, 3 }, { 1, 5 }, { 1, 6 } };
    DWORD rs[4], tss[4], savedPs = g_gvGlowPs;
    float savedC[5][4];
    void *ib = NULL;
    UINT base = 0;
    int i, e, bad = 0, savedCN = g_gvGlowCN, drawn = 0;
    BYTE *m = *(BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    *bodies = 0;
    if (!g_gvDipHooks || !(look == 1 ? FxShader(dev, FXT_HEAT) : FxShader(dev, FXT_GVGLOW))) return 0;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) bad = 1;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, kTss[i][0], kTss[i][1], &tss[i]))) bad = 1;
    if (FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) ib = NULL, bad = 1;
    if (bad) {
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        if (!g_aiLogged[3]++) Log("after shock: a device state query for the redraw failed: no echoes this frame (logged once)");
        return 0;
    }
    memcpy(savedC, g_gvGlowC, sizeof savedC);
    FxReapply(dev, sv);  // the game's states (its renderer sets per material only what it changes)
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv->vpSet = 1;
    if (g_aiWarmNow) {  // v3.35 FX335: the warm-up: her own pose in the copies' glow pass (zero light) and in Thermal's heat
        // pass (alpha 0): the draw states every copy will use are made now, nothing shows
        float (*cur)[16] = m ? *(float (**)[16])(m + 0xC0) : NULL;
        if (cur && !(*(const WORD *)(m + 0xE4) & 4)) {
            AiGlowConsts(kAiShockCol, 0.0f);
            if (AiDrawSwapped(m, cur, 0)) ++drawn;
            ThConsts(0.5f);
            g_thC[5][3] = 0.0f;
            g_thPs = g_fxPs[FXT_HEAT];
            g_thOpaque = 0;
            g_thBiasOn = 0;
            if (g_thPs && AiDrawSwapped(m, cur, 1)) ++drawn;
        }
    } else if (m && m == g_aiModelSeen && *(const void *const *)(m + 0xB4) == g_aiFileSeen && !(*(const WORD *)(m + 0xE4) & 4))  // v3.30 AS330: + file
        for (e = 0; e < g_aiEchoN; ++e) {
            const AiEcho *E = &g_aiEcho[e];
            if (look == 1) {  // Thermal: residual body heat, cooling to red along the chain, translucent
                ThConsts(E->heat);
                g_thC[5][3] = E->alpha;
                g_thPs = g_fxPs[FXT_HEAT];
                g_thOpaque = 0;
                g_thBiasOn = 0;
            } else {
                AiGlowConsts(look == 3 ? kAiColSv : E->col, E->gain * (look == 2 ? AI_NV_K : look == 3 ? 0.8f : 1.0f));
            }
            if (!AiDrawSwapped(m, g_aiEchoMtx[e], look == 1)) {
                AiFailNow("an echo redraw", g_gvFaultCode, g_gvFaultAt);
                break;
            }
            ++drawn;
        }
    if (!g_aiFail && look != 1 && look != 3 && !g_aiWarmNow) {  // the blue shock on stunned bodies (their own pose; not on ghosts in Thermal / SV)
        SmoothApply();  // EntitySmooth: on the smoothed pose, as the stock draw
        for (i = 0; i < AI_SHOCK_N && !g_aiFail; ++i) {
            const AiShock *s = &g_aiShock[i];
            const float span = (float)(s->until - s->t0), age = (float)(g_aiClock - s->t0);
            int q;
            if (!s->task || s->stamp != g_fxFrame || !(span > 0.0f) || age < 0.0f || age >= span) continue;
            AiGlowConsts(kAiShockCol, s->warm ? 0.0f : 0.35f * (1.0f - age / span) * (0.55f + 0.45f * (AiFlicker(g_aiClock * 1.25, 50 + i) - 0.8f) / 0.7f) *
                                          (look == 2 ? AI_NV_K : 1.0f));  // v3.35 FX335: a warm-up shock draws no light
            for (q = 0; q < s->nm; ++q) {
                __try {
                    g_aiZ = 1;
                    InterlockedExchange(&g_gvGlow, 1);
                    ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(s->model[q]);
                    ++*bodies;
                } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
                    InterlockedExchange(&g_gvGlow, 0);
                    g_aiZ = 0;
                    AiFailNow("a shock redraw", g_gvFaultCode, g_gvFaultAt);
                    break;
                }
                InterlockedExchange(&g_gvGlow, 0);
                g_aiZ = 0;
            }
        }
        SmoothRestore();
    }
    ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
    if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
    for (i = 0; i < 4; ++i) ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kTss[i][0], kTss[i][1], tss[i]);
    for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
    g_gvGlowPs = savedPs;  // Spirit Vision's ghost glow sets its own before each use; put them back anyway
    memcpy(g_gvGlowC, savedC, sizeof savedC);
    g_gvGlowCN = savedCN;
    return drawn;
}

// ---------------------------------------------------------------- the sparks: arcs (ribbons) and motes (sprites)
// a jagged arc a -> b of n points: hashed perpendicular offsets (a sine envelope: 0 at the ends), a thin bright core and
// a 3 x wide dim glow (DpRibbon: additive strips, FXX_SMOKE cell 6)
static void AiArc(const float a[3], const float b[3], int n, float amp, AiU32 seed, float w, float k, const float rgb[3])
{
    float P[12][3], W[12], K[12], d[3], u[3], v[3], e[3], len, l;
    int i, j;
    if (n > 12) n = 12;
    if (n < 2 || !(k > 0.004f)) return;
    for (j = 0; j < 3; ++j) d[j] = b[j] - a[j], e[j] = g_aiEyeOk ? 0.5f * (a[j] + b[j]) - g_aiEye[j] : (j == 2 ? 1.0f : 0.0f);
    len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(len > 0.5f)) return;
    u[0] = d[1] * e[2] - d[2] * e[1], u[1] = d[2] * e[0] - d[0] * e[2], u[2] = d[0] * e[1] - d[1] * e[0];  // across the view
    l = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (!(l > 1e-4f)) u[0] = 1.0f, u[1] = u[2] = 0.0f, l = 1.0f;
    for (j = 0; j < 3; ++j) u[j] /= l;
    v[0] = d[1] * u[2] - d[2] * u[1], v[1] = d[2] * u[0] - d[0] * u[2], v[2] = d[0] * u[1] - d[1] * u[0];
    for (j = 0; j < 3; ++j) v[j] /= len;
    for (i = 0; i < n; ++i) {
        const float s = (float)i / (float)(n - 1), env = sinf(3.14159265f * s);
        const float r1 = (AiRnd01(seed + (AiU32)i * 2u) - 0.5f) * 2.0f, r2 = AiRnd01(seed + (AiU32)i * 2u + 1u) - 0.5f;
        for (j = 0; j < 3; ++j) P[i][j] = a[j] + d[j] * s + (u[j] * r1 + v[j] * r2) * amp * len * env;
        W[i] = w * (0.55f + 0.45f * env);
        K[i] = k;
    }
    DpRibbon((const float(*)[3])P, W, K, n, rgb, 0.75f * g_svUnit);
    for (i = 0; i < n; ++i) W[i] *= 3.0f, K[i] = 0.30f * k;
    DpRibbon((const float(*)[3])P, W, K, n, rgb, 1.5f * g_svUnit);
}
// v3.32 AS332: p moved d units toward the camera (an arc over her body is drawn in front of it, not inside it)
static void AiToEye(const float p[3], float d, float o[3])
{
    float e[3], l;
    int j;
    memcpy(o, p, 12);
    if (!g_aiEyeOk) return;
    for (j = 0; j < 3; ++j) e[j] = g_aiEye[j] - p[j];
    l = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    if (!(l > d + 20.0f)) return;
    for (j = 0; j < 3; ++j) o[j] = p[j] + e[j] / l * d;
}
// v3.32 AS332: a unit vector across the view at p: along the screen perpendicular of direction d (d 0: the screen's
// horizontal), so a sprite or tendril offset by it lies beside her silhouette, not in front of or behind it
static void AiAcross(const float p[3], const float d[3], float o[3])
{
    float e[3], l;
    int j;
    for (j = 0; j < 3; ++j) e[j] = g_aiEyeOk ? p[j] - g_aiEye[j] : (j == 2 ? 1.0f : 0.0f);
    if (d) o[0] = d[1] * e[2] - d[2] * e[1], o[1] = d[2] * e[0] - d[0] * e[2], o[2] = d[0] * e[1] - d[1] * e[0];
    else o[0] = -e[2], o[1] = 0.0f, o[2] = e[0];  // (up x e: horizontal)
    l = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
    if (!(l > 1e-4f)) o[0] = 1.0f, o[1] = o[2] = 0.0f, l = 1.0f;
    for (j = 0; j < 3; ++j) o[j] /= l;
}
// Thermal has no additive light and no ribbons (DpSlAdd / DpRibbon): an arc there is 3 white-hot points
static void AiHotArc(const float a[3], const float b[3], AiU32 seed, float k)
{
    int i, j;
    for (i = 0; i < 3; ++i) {
        const float s = 0.2f + 0.3f * (float)i + 0.1f * AiRnd01(seed + (AiU32)i);
        float p[3];
        for (j = 0; j < 3; ++j) p[j] = a[j] + (b[j] - a[j]) * s;
        DpSlAdd(p, 1.6f, 0.0f, DPT_SMOKE, DPS_SOFT, 0, kAiHotCol, 0.6f * k, 1.0f * g_svUnit, 0, 2, 0.0f);
    }
}
// v3.33 AS333: the cloak (ai_sim.h AiCloakBuild) for this frame: her body measured (its size for the arcs too), the ribbons
// built; drawn by AiCloakDraw as their own batch. 0 = no cloak this frame (her bones are not usable: an ANOMALY line)
// v3.35 FX335: no cloak (the user: "remove the blue glow from the player"): her body is only measured (AiBodyMeasure: the
// arcs' and tendrils' girth); under Thermal her outline is built for the white-hot points (AiCloakDraw). 1 = draw those.
static AiCloak g_aiCk;  // (13 KB: static, not on the stack)
static int AiCloakMake(int look)
{
    static volatile LONG s_qBad, s_qSize;
    static float s_build = -9.0f, s_len = -9.0f;
    float bn[AIB_N][3];
    int i, ok;
    g_aiCkRibs = g_aiCkPts = 0;
    if (!g_aiHerOk) return 0;
    for (i = 0; i < HB_N; ++i) memcpy(bn[i], g_aiHer[i], 12);
    memcpy(bn[AIB_LTOE], g_aiToe[0], 12);
    memcpy(bn[AIB_RTOE], g_aiToe[1], 12);
    if (look == 1) {  // Thermal: the outline for the hot points (no light is drawn from the cloak)
        ok = AiCloakBuild((const float(*)[3])bn, g_aiToeOk, g_aiEye, g_aiEyeOk, g_aiCharge, g_aiClock, &g_aiCk) > 0;
        if (ok) g_aiBody = g_aiCk.body, g_aiCkPts = g_aiCk.nPts;
    } else {
        AiBody b;
        ok = AiBodyMeasure((const float(*)[3])bn, &b);
        if (ok) g_aiBody = b;
    }
    if (!ok) {
        g_aiBodyOk = 0;
        if (LogQuota(&s_qBad, 10))
            LogAnomaly("after shock", "her bones give no usable skeleton (hips %.1f u apart, pelvis %.0f %.0f %.0f, head %.0f %.0f %.0f): "
                       "the arcs keep the last size this frame", AiLen3(bn[AIB_LHIP], bn[AIB_RHIP]), bn[AIB_PELVIS][0], bn[AIB_PELVIS][1],
                       bn[AIB_PELVIS][2], bn[AIB_HEAD][0], bn[AIB_HEAD][1], bn[AIB_HEAD][2]);
        return 0;
    }
    g_aiBodyOk = 1;
    if ((fabsf(g_aiBody.build - s_build) > 0.1f || fabsf(g_aiBody.lenScale - s_len) > 0.03f) && LogQuota(&s_qSize, 50)) {
        LogV("after shock: her body: hips %.1f u apart, bone lengths x%.3f of Playable Eileen's -> build %.2f (0 = Playable Eileen's "
             "body, 1 = vanilla Henry's), radii x%.2f (the arcs' girth); toe joints %s", g_aiBody.hips, g_aiBody.lenScale, g_aiBody.build,
             g_aiBody.sizeK, g_aiToeOk ? "read" : "estimated");
        s_build = g_aiBody.build, s_len = g_aiBody.lenScale;
    }
    return look == 1;
}
static void AiCloakDraw(int look)
{
    const float nvk = look == 2 ? AI_NV_K : 1.0f;
    int r, i;
    if (look == 1) {  // Thermal (no additive light, no ribbons): 4 .. 14 white-hot points crawl along her outline, every 50 ms
        const AiU32 e50 = (AiU32)(long long)floor(g_aiClock / 0.05);
        const int nHot = 4 + (int)(10.0f * g_aiCharge + 0.5f);
        int ne = 0, edge[AI_CK_RIBS];
        for (r = 0; r < g_aiCk.nRib; ++r)
            if (g_aiCk.r[r].kind == 1 && g_aiCk.r[r].n >= 2) edge[ne++] = r;
        for (i = 0; i < nHot && ne; ++i) {
            const AiU32 h = AiHash(e50 * 0x632BE5ABu + (AiU32)i * 0x9E3779B9u);
            const AiCkRib *q = &g_aiCk.r[edge[h % (AiU32)ne]];
            DpSlAdd(q->P[(h >> 8) % (AiU32)q->n], 1.4f * g_aiBody.sizeK, 0.0f, DPT_SMOKE, DPS_SOFT, 0, kAiHotCol,
                    (0.25f + 0.45f * g_aiCharge) * (0.7f + 0.3f * AiRnd01(h ^ 0x3Du)), 1.0f * g_svUnit, 0, 2, 0.0f);
        }
        return;
    }
    // v3.35 FX335: no fill (v3.34 drew 6 soft blue strips over her body here: the blue glow the user asked to remove)
    (void)nvk;
}
// v3.33 AS333: the half extent of her edge e along unit u (her real girth there, from the cloak's presets and her body's size)
static float AiEdgeR(int e, const float u[3])
{
    const AiEdgeCk *k = &kAiEdgeCk[e];
    const float b = g_aiBody.build, g = g_aiBody.sizeK;
    float X[3];
    int j;
    for (j = 0; j < 3; ++j) X[j] = k->lat == 1 ? g_aiHer[HB_RHIP][j] - g_aiHer[HB_LHIP][j] : g_aiHer[HB_RSH][j] - g_aiHer[HB_LSH][j];
    AiNorm3(X, 1.0f, 0.0f, 0.0f);
    return AiCkExtent((k->W[0] + (k->W[1] - k->W[0]) * b) * g, (k->D[0] + (k->D[1] - k->D[0]) * b) * g, X, u, k->lat);
}
// v3.33 AS333: p moved toward the camera to just in front of her surface on edge e (+ 3 u x her size): an arc lies ON her cloak
static void AiOnSurface(const float p[3], int e, float o[3])
{
    float u[3];
    int j;
    memcpy(o, p, 12);
    if (!g_aiEyeOk) return;
    for (j = 0; j < 3; ++j) u[j] = g_aiEye[j] - p[j];
    AiNorm3(u, 0.0f, 0.0f, 1.0f);
    AiToEye(p, AiEdgeR(e, u) + 3.0f * g_aiBody.sizeK, o);
}
// v3.32 AS332: the aura on her own body (AiFrame's charge c: 0.3 standing .. 1 at a run). Every 50 ms 2 + 8 c arcs crawl
// over her limbs (a random part of a random edge, pushed its radius + 3 u toward the camera), 4 c tendrils leap off her
// silhouette (one in three forks); while she moves, a ground strike every 0.12 s (at 0.6 c odds) from a foot or hand to
// the floor beside her (v3.33 AS333: the corona and bloom sprites are gone: the cloak, AiCloakDraw; the arcs lie on her
// cloak and the tendrils leave from its outline). At most 38 ribbons and 1 sprite (Thermal: white-hot points only: no
// additive light there)
static void AiAura(int look)
{
    const double now = g_aiClock;
    const float c = g_aiCharge, *arc = look == 1 ? kAiHotCol : kAiArcCol, *glow = look == 3 ? kAiColSv : kAiShockCol;
    const float nvk = look == 2 ? AI_NV_K : 1.0f;
    const AiU32 e50 = (AiU32)(long long)floor(now / 0.05), e12 = (AiU32)(long long)floor(now / 0.12);
    const int nA = 2 + (int)(8.0f * c + 0.5f), nT = (int)(4.0f * c + 0.2f);
    int i, j;
    if (!g_aiHerOk) return;
    for (i = 0; i < nA; ++i) {  // the crackle over her limbs
        const AiU32 h = AiHash(e50 * 0x2545F491u + (AiU32)i * 977u + 0x3Bu);
        const int e = (int)(h % 15u);
        const float *pa = g_aiHer[kDpHerEdge[e][0]], *pb = g_aiHer[kDpHerEdge[e][1]];
        float a[3], b[3], s0 = 0.5f * AiRnd01(h ^ 0x51u), s1 = s0 + 0.4f + 0.45f * AiRnd01(h ^ 0x73u);
        const float k = (0.45f + 0.55f * c) * (0.75f + 0.25f * AiRnd01(h ^ 0x99u)) * nvk;
        if (AiRnd01(h ^ 0x1Fu) > 0.55f + 0.4f * c) continue;  // lit: 67 % standing, 95 % at a run
        if (s1 > 1.0f) s1 = 1.0f;
        for (j = 0; j < 3; ++j) a[j] = pa[j] + (pb[j] - pa[j]) * s0, b[j] = pa[j] + (pb[j] - pa[j]) * s1;
        AiOnSurface(a, e, a);  // v3.33 AS333: on her cloak (v3.32: the stand-in radius + 3 u, inside her body)
        AiOnSurface(b, e, b);
        if (look == 1) AiHotArc(a, b, h, k);
        else AiArc(a, b, 6, 0.32f, h, 0.9f, k, arc);
    }
    for (i = 0; i < nT && look != 1; ++i) {  // tendrils off her silhouette
        const AiU32 h = AiHash(e50 * 0x9E3779B1u + (AiU32)i * 0x85EBCA77u + 0x5Du);
        const int e = (int)(h % 15u);
        const float *pa = g_aiHer[kDpHerEdge[e][0]], *pb = g_aiHer[kDpHerEdge[e][1]];
        const float s = AiRnd01(h ^ 0x21u), side = (h >> 9 & 1u) ? 1.0f : -1.0f, len = (14.0f + 26.0f * AiRnd01(h ^ 0x43u)) * g_aiBody.sizeK;
        float p[3], d[3], w[3], a[3], tip[3], up, R;
        for (j = 0; j < 3; ++j) p[j] = pa[j] + (pb[j] - pa[j]) * s, d[j] = pb[j] - pa[j];
        AiAcross(p, d, w);
        R = AiEdgeR(e, w);  // v3.33 AS333: from her outline (the cloak's edge)
        up = -0.6f * AiRnd01(h ^ 0x65u);  // (a little upward: Y is down)
        for (j = 0; j < 3; ++j) {
            a[j] = p[j] + side * w[j] * R;
            tip[j] = a[j] + side * w[j] * len + (j == 1 ? up * len : 0.0f);
        }
        AiArc(a, tip, 5, 0.35f, h, 0.7f, 0.9f * c * nvk, arc);
        if ((h >> 13) % 3u == 0u) {  // a fork from its middle
            float mid[3], t2[3];
            for (j = 0; j < 3; ++j) mid[j] = 0.5f * (a[j] + tip[j]), t2[j] = mid[j] + (AiRnd01(h + 7u + (AiU32)j) - 0.5f) * 0.8f * len;
            AiArc(mid, t2, 4, 0.3f, h ^ 0xABu, 0.5f, 0.6f * c * nvk, arc);
        }
    }
    {   // a ground strike while she moves: lit for the first 60 % of its 0.12 s
        const AiU32 h = AiHash(e12 * 0x7F4A7C15u + 0x77u);
        const float ph = (float)(now / 0.12 - floor(now / 0.12));
        if (c > 0.45f && ph < 0.6f && AiRnd01(h) < 0.6f * c) {
            static const BYTE kEnd[4] = { HB_LFOOT, HB_RFOOT, HB_LHAND, HB_RHAND };
            const int src = kEnd[(h >> 5) & 3u];
            const float floorY = g_aiHer[HB_LFOOT][1] > g_aiHer[HB_RFOOT][1] ? g_aiHer[HB_LFOOT][1] : g_aiHer[HB_RFOOT][1];  // (Y down)
            const float ang = 6.2831853f * AiRnd01(h ^ 0x3Cu), dist = 25.0f + 35.0f * AiRnd01(h ^ 0x5Au);
            float tip[3];
            tip[0] = g_aiHer[src][0] + cosf(ang) * dist, tip[1] = floorY, tip[2] = g_aiHer[src][2] + sinf(ang) * dist;
            if (look == 1) {
                AiHotArc(g_aiHer[src], tip, h, c);
            } else {
                AiArc(g_aiHer[src], tip, 7, 0.3f, h ^ 0x11u, 1.0f, 0.9f * c * nvk, arc);
                DpSlAdd(tip, 10.0f, 0.0f, DPT_SMOKE, DPS_SOFT, 1, glow, 0.35f * c * nvk, 1.5f * g_svUnit, 0, 2, 0.0f);
            }
        }
    }
    // v3.33 AS333: v3.32's corona (60 soft sprites along her outline) and bloom (3 over head, chest, pelvis), the "blue orbs",
    // are replaced by the cloak (AiCloakDraw, drawn first as its own ribbon batch by AiSparks)
}
// v3.32 AS332: the streaks (AiFrame: her head, hands and feet along her path, the lens fan applied), jagged by +-1.5 u
// re-rolled every 50 ms: a thin core and a 3 x wide dim glow, 10 ribbons
static void AiStreaks(int look)
{
    const AiU32 e50 = (AiU32)(long long)floor(g_aiClock / 0.05);
    const float *arc = look == 3 ? kAiColSv : kAiColNear;
    float P[AI_STREAK_PTS][3], W[AI_STREAK_PTS], K[AI_STREAK_PTS];
    int s, j, q, lit = 0;
    if (look == 1 || g_aiStreakN < 3) return;
    for (j = 1; j < g_aiStreakN; ++j) lit |= g_aiStreakK[j] > 0.01f;
    if (!lit) return;
    for (s = 0; s < AI_STREAKS; ++s) {
        for (j = 0; j < g_aiStreakN; ++j) {
            const AiU32 h = AiHash(e50 * 0x6A09E667u + (AiU32)s * 131u + (AiU32)j * 7u);
            for (q = 0; q < 3; ++q) P[j][q] = g_aiStreakP[s][j][q] + (j ? (AiRnd01(h + (AiU32)q) - 0.5f) * 3.0f : 0.0f);
            W[j] = 0.8f, K[j] = g_aiStreakK[j] * (s == 0 ? 0.8f : s < 3 ? 1.0f : 0.6f) * (look == 2 ? AI_NV_K : 1.0f);  // head, hands, feet
        }
        DpRibbon((const float(*)[3])P, W, K, g_aiStreakN, arc, 0.75f * g_svUnit);
        for (j = 0; j < g_aiStreakN; ++j) W[j] = 3.0f, K[j] *= 0.35f;
        DpRibbon((const float(*)[3])P, W, K, g_aiStreakN, arc, 1.5f * g_svUnit);
    }
}
static void AiSparks(void *dev, const Viewport8 *vp, int look)
{
    const int saveLook = g_dpLook, saveDrawn = g_dpSLDrawn;
    const float saveA = g_dpAlphaK, saveS = g_dpStretchK;
    const double now = g_aiClock;
    const AiU32 e60 = (AiU32)(long long)floor(now / 0.06), e80 = (AiU32)(long long)floor(now / 0.08), e40 = (AiU32)(long long)floor(now / 0.04);
    const float *arc = look == 1 ? kAiHotCol : kAiArcCol;
    int e, i;
    g_svVp = vp;
    if (!SvSprScale(vp)) return;
    g_svUnit = (float)vp->h / 720.0f;
    g_dpSLN = 0;
    g_dpRibN = g_dpRibs = 0;
    g_dpAlphaK = 1.0f, g_dpStretchK = 1.0f;
    g_dpLook = look == 1 ? 1 : look == 3 ? 3 : 0;  // (DpSlAdd / DpRibbon: Thermal has no additive light and no ribbons)
    if (AiCloakMake(look)) AiCloakDraw(look);  // v3.35 FX335: Thermal's hot points only (v3.34: + the cloak's own ribbon batch)
    // v3.32 AS332: the ribbon budget (128) goes first to the zaps (48 at most), the aura (38), the streaks (10), then the
    // copies' limb arcs (32) and the chain (14): only the copies' arcs can be cut, and only with 6 or more zaps at once
    if (look != 1)
        for (i = 0; i < AI_ZAP_N; ++i) {  // the zaps: a 3-fork arc from the touching echo to the enemy, re-jagged every 40 ms
            const AiZap *z = &g_aiZap[i];
            const float age = (float)(now - z->t0);
            float k, mid[3], za[3], sh[3];
            int f, q;
            if (!(z->t0 > 0.0) || age < 0.0f || age > AI_ZAP_S) continue;
            k = sqrtf(1.0f - age / AI_ZAP_S);
            AiFanVec(z->a, sh);  // v3.32 AS332: from where that copy is drawn (the lens fan)
            for (q = 0; q < 3; ++q) za[q] = z->a[q] + sh[q];
            AiArc(za, z->b, 10, 0.16f, AiHash(z->seed ^ e40 * 0x9E3779B9u), 1.5f, k, kAiArcCol);
            for (f = 0; f < 2; ++f) {
                const AiU32 h = AiHash(z->seed ^ e40 * 0x85EBCA6Bu ^ (AiU32)f * 0x27D4EB2Fu);
                const float s = 0.25f + 0.4f * AiRnd01(h);
                float tip[3];
                for (q = 0; q < 3; ++q) {
                    mid[q] = za[q] + (z->b[q] - za[q]) * s;
                    tip[q] = z->b[q] + (AiRnd01(h + 11u + (AiU32)q) - 0.5f) * 70.0f;
                }
                AiArc(mid, tip, 6, 0.25f, h, 0.9f, 0.7f * k, kAiArcCol);
            }
            DpSlAdd(z->b, 26.0f, 0.0f, DPT_SMOKE, DPS_SOFT, 1, kAiShockCol, 0.45f * k, 0.0f, 0, 2, 0.0f);  // a flash at the enemy
        }
    AiAura(look);     // v3.32 AS332 (replaces v3.29's idle hand crackle)
    AiStreaks(look);  // v3.32 AS332
    for (e = 0; e < g_aiEchoN; ++e) {
        const AiEcho *E = &g_aiEcho[e];
        const float (*M)[16] = (const float(*)[16])g_aiEchoMtx[e], rel = E->gain / AI_G0;
        int j;
        if (rel < 0.05f) continue;
        for (j = 0; j < 2; ++j) {  // crackle along the limbs: 2 arcs an echo, re-rolled every 60 ms (70 % of them lit)
            const AiU32 h = AiHash(e60 * 977u + (AiU32)E->k * 131u + (AiU32)j * 7u);
            const int edge = 3 + (int)(h % 12u);  // kDpHerEdge 3..14: shoulders, arms, hips, legs
            const float *pa = AiBone(M, kDpHerEdge[edge][0]), *pb = AiBone(M, kDpHerEdge[edge][1]);
            float a[3], b[3], s0 = 0.6f * AiRnd01(h ^ 0x55u), s1 = s0 + 0.35f + 0.4f * AiRnd01(h ^ 0x77u);
            int q;
            if ((h >> 8 & 255u) > 178u) continue;
            if (s1 > 1.0f) s1 = 1.0f;
            for (q = 0; q < 3; ++q) a[q] = pa[q] + (pb[q] - pa[q]) * s0, b[q] = pa[q] + (pb[q] - pa[q]) * s1;
            AiArc(a, b, 5, 0.28f, h, 0.75f, 0.95f * rel, arc);
        }
    }
    if (look != 1) {
        const float ph = (float)(now / 0.08 - floor(now / 0.08));
        for (e = 0; e + 1 < g_aiEchoN && ph < 0.75f; ++e) {  // the chain: one arc between consecutive echoes every 80 ms
            const AiU32 h = AiHash(e80 * 2654435761u + (AiU32)g_aiEcho[e].k * 40503u);
            const int b = (int)(h % (AiU32)HB_N);
            const float rel = (g_aiEcho[e].gain < g_aiEcho[e + 1].gain ? g_aiEcho[e].gain : g_aiEcho[e + 1].gain) / AI_G0;
            if (rel < 0.05f) continue;
            AiArc(AiBone((const float(*)[16])g_aiEchoMtx[e], b), AiBone((const float(*)[16])g_aiEchoMtx[e + 1], b), 7, 0.15f, h, 0.6f,
                  0.75f * rel, arc);
        }
    }
    for (i = 0; i < AI_MOTE_N; ++i) {  // the motes: straight flights, fading (Thermal: white-hot, premultiplied)
        const AiMote *m = &g_aiMote[i];
        const float age = (float)(now - m->t0);
        float p[3], a;
        int q;
        if (!(m->life > 0.0f) || age < 0.0f || age >= m->life) continue;
        for (q = 0; q < 3; ++q) p[q] = m->p[q] + m->v[q] * age;
        a = 1.0f - age / m->life;
        DpSlAdd(p, m->size, 0.0f, DPT_SMOKE, DPS_SOFT, look != 1, look == 1 ? kAiHotCol : kAiArcCol, (look == 1 ? 0.7f : 0.9f) * a * a,
                1.2f * g_svUnit, 0, 2, 0.0f);
    }
    g_aiSprites = g_dpSLN;
    DpSlDraw(dev);
    DpRibDraw(dev);
    g_dpLook = saveLook, g_dpSLDrawn = saveDrawn, g_dpAlphaK = saveA, g_dpStretchK = saveS;  // (Death Presence's frame state)
}

// ---------------------------------------------------------------- one look: echoes, shocks, sparks (FxSave .. FxRestore)
static int AiShockLive(void)  // a stunned body collected this frame still shocks
{
    int i;
    for (i = 0; i < AI_SHOCK_N; ++i)
        if (g_aiShock[i].task && g_aiShock[i].stamp == g_fxFrame && g_aiShock[i].until > g_aiClock) return 1;
    return 0;
}
static int AiDraw(void *dev, int look)  // 0 = nothing drawn (no device state)
{
    FxSaved sv;
    const Viewport8 *vp;
    int bodies = 0;
    if (!FxSave(dev, &sv, 6)) return 0;  // (c0..c5: the glow / heat constants of the redraw, as ThHeatPass)
    vp = g_aiVpOk && g_aiVpStamp == g_fxFrame ? &g_aiVp : &sv.vp;  // the 3D viewport the world was drawn in
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv.vpSet = 1;
    __try {
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);  // this frame's world -> GS matrix (as Item Shine)
        SvNearFar();
        DpCamProj();  // the camera's projection for the sprites' depth (v3.27.2 / v3.27.3: exact)
        g_svEyeOk = SvEye(g_svEye);
        {   // v3.35 FX335: the redraws and the sparks timed apart (AiSlowLog)
            LARGE_INTEGER q0;
            QueryPerformanceCounter(&q0);
            if ((g_aiEchoN || AiShockLive()) && g_aiDrawOk) AiRedraw(dev, &sv, vp, look, &bodies);
            g_aiMsRedraw += DpMsSince(&q0);
            QueryPerformanceCounter(&q0);
            if (!g_aiFail) {
                FxBase(dev);
                SvZ(dev, 2);  // the visible part only (reversed Z: GREATEREQUAL), no z write
                AiSparks(dev, vp, look);
            }
            g_aiMsSpark += DpMsSince(&q0);
        }
    } __finally {
        g_svZScaleNow = SV_ZSCALE;  // (DpCamProj set the exact depth: Spirit Vision keeps its own calibration)
        FxRestore(dev, &sv);
    }
    return 1;
}
static void AiMsAdd(const LARGE_INTEGER *t0)
{
    const float ms = DpMsSince(t0);
    if (g_aiMsStamp != g_fxFrame) g_aiMsStamp = g_fxFrame, g_aiMsFrame = 0.0f;
    g_aiMsFrame += ms;
    if (g_aiMsFrame > g_aiMsMax) g_aiMsMax = g_aiMsFrame;
}
static LONG g_aiAskFrame = -10, g_aiDoneFrame = -10;  // the post-filter request (Thermal / Spirit Vision) and its service
static int g_aiAskLook;
static DWORD g_aiAskVisit, g_aiBroken[4];
static void AiRoomLog(DWORD room, int look)
{
    static DWORD s_room = 0xFFFFFFFFu;
    static double s_t0;
    static const char *const kLook[4] = { "full", "Thermal", "night vision", "Spirit Vision" };
    if (room != s_room) {
        s_room = room, s_t0 = g_aiClock;
        return;
    }
    if (s_t0 < 0.0 || g_aiClock - s_t0 < 3.0) return;
    s_t0 = -1.0;
    char bud[64];
    if (g_bgOn) sprintf_s(bud, sizeof bud, g_aiKeepN < AiTrailNow()->n ? " (budget level %d: %d of %d copies)" : "", BgLevel(), g_aiKeepN, AiTrailNow()->n);
    else strcpy_s(bud, sizeof bud, g_aiCap < AiTrailNow()->n ? " (fewer echoes: over budget)" : "");
    Log("after shock: stage %lX room %lX (look %s, %s trail): her model %s (%d bones, %d kept), ring %d samples, %d echoes and %d "
        "sprites this frame, %d stun(s) so far, the slowest frame %.2f ms%s; charge %.2f, %s (her view distance %.0f u); no glow on her "
        "body (Thermal hot points over %d outline points; body build %.2f, radii x%.2f)", room >> 16, room & 0xFFFF, kLook[look & 3],  // v3.32 AS332: + charge, fan; v3.35 FX335: no cloak
        kGpName[GP_AI][g_aiLevel > 0 && g_aiLevel < 4 ? g_aiLevel : 0], g_aiHerOk ? "drawn" : "not drawn", g_aiN, g_aiCopyN,
        g_aiRing.count, g_aiEchoN, g_aiSprites, g_aiStunsRoom, g_aiMsMax, bud,
        g_aiCharge, g_aiFanK > 0.0f ? (g_aiFanOk ? "lens fan on (AfterImageFan)" : "lens fan idle") : "copies in line", g_aiHerDepth,
        g_aiCkPts, g_aiBody.build, g_aiBody.sizeK);  // v3.33 AS333: + in line / cloak
}
// v3.35 FX335: one of After Shock's own frames took over 30 ms (the user's log: "after shock 259.2 ms" in one frame right
// after a load): which part, so the next log says whether it was its work or a draw call that waited (rate-limited)
static void AiSlowLog(float ms, DWORD room)
{
    static volatile LONG s_q;
    if (!(ms > 30.0f) || !LogQuota(&s_q, 40)) return;
    LogV("after shock: a slow frame of its own, %.1f ms (stage %lX room %lX): her pose and the copies' places %.2f ms, the redraws "
         "(%d copies, shocks) %.2f ms, the arcs and sprites (%d) %.2f ms; a single part this long with little drawn is a draw call "
         "that waited on the driver or the GPU (a load just before?), not After Shock's own work", ms, room >> 16, room & 0xFFFF,
         g_aiMsPrep, g_aiEchoN, g_aiMsRedraw, g_aiSprites, g_aiMsSpark);
}
static void AiDrawC(void)
{
    void *dev = DEV;
    float pp[3];
    DWORD room;
    int look;
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    g_aiMsPrep = g_aiMsRedraw = g_aiMsSpark = 0.0f;  // v3.35 FX335
    if (!dev || !FxOnBackBuffer(dev)) return;
    memcpy(pp, (const void *)0x10A0F60, sizeof pp);
    if (!DpFinite3(pp)) return;
    room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (room != g_aiRoomKey || (room >> 16) == 1) return;  // the tick has not seen this room yet / Room 302
    if (room != g_aiDrawRoom) {  // a new visit: the post-filter looks are asked again, the budget starts again
        g_aiDrawRoom = room;
        ++g_aiVisit;
        g_aiCap = AI_ECHO_MAX;
        g_aiSlow = 0;
        g_aiMsMax = 0.0f;
        AiCutReset(&g_aiCut);  // v3.33 AS333
        memset(g_aiMote, 0, sizeof g_aiMote);
    }
    AiCapture(pp);
    AiFrame(pp);
    g_aiMsPrep = DpMsSince(&t0);  // v3.35 FX335
    {   // v3.42 SM342 (USER_FEEDBACK_v341: hidden in peep / window views, with Death Presence's signals): not drawn while
        // the room is off, the peeping flag is on or a move of the player is pending (the tick, her pose ring and the
        // copies' places run on: nothing is reset)
        static DpViewFx s_view = { { 0 }, { 0 }, -10 };
        if (!DpViewFrame(&s_view, "after shock")) {
            g_aiEchoN = 0;  // (loghealth.inc: not an active frame)
            return;
        }
    }
    look = AiLookNow();
    if (look == 1 || look == 3) {  // Thermal / Spirit Vision: drawn after their own passes (AiPostDraw from FxPass)
        if (g_aiAskFrame == g_fxFrame - 1 && g_aiDoneFrame != g_fxFrame - 1 && g_aiAskLook == look && g_aiAskVisit == g_aiVisit &&
            g_aiBroken[look] != g_aiVisit) {
            g_aiBroken[look] = g_aiVisit;  // not served last frame: the scene-pass look for the rest of this room
            Log("after shock: stage %lX room %lX: %s did not draw the after shock's look: the plain look in this room", room >> 16,
                room & 0xFFFF, look == 3 ? "Spirit Vision" : "Thermal");
        }
        if (g_aiBroken[look] != g_aiVisit) {
            g_aiAskFrame = g_fxFrame, g_aiAskLook = look, g_aiAskVisit = g_aiVisit;
            AiMsAdd(&t0);
            AiRoomLog(room, look);
            return;
        }
        look = 0;
    }
    if (!AiDraw(dev, look)) {
        if (!g_aiLogged[4]++) Log("after shock: the device state could not be saved: nothing drawn this frame (logged once)");
        return;
    }
    AiMsAdd(&t0);
    AiSlowLog(DpMsSince(&t0), room);  // v3.35 FX335
    if (!g_bgOn) {   // v3.30 AS330: the drawn-echo ladder (ai_sim.h AiCapStep): 8 -> 5 -> 3; Long's 8 redraws get a 3 ms cap
        // (v3.35 FX335: only with the effect budget off; with it on, budget.inc's level thins the copies and they come back)
        const int drawn = AiTrailNow()->n < g_aiCap ? AiTrailNow()->n : g_aiCap;
        const float cap = drawn > 5 ? AI_MS_CAP_LONG : AI_MS_CAP;
        if (g_aiMsFrame > cap) {
            if (++g_aiSlow >= 30 && drawn > 3) {
                static volatile LONG s_q;
                g_aiCap = AiCapStep(drawn);
                g_aiSlow = 0;
                if (LogQuota(&s_q, 300))  // v3.33 AS333: verbose, rate-limited (at most 2 a room visit anyway)
                    LogV("after shock: guard cut: over %.1f ms a frame for 30 frames (%.2f ms) with %d copies: %d of the %s trail's %d for the "
                         "rest of stage %lX room %lX", cap, g_aiMsFrame, drawn, g_aiCap, kGpName[GP_AI][g_aiLevel > 0 && g_aiLevel < 4 ? g_aiLevel : 0],
                         AiTrailNow()->n, room >> 16, room & 0xFFFF);
            }
        } else {
            g_aiSlow = 0;
        }
        {   // v3.33 AS333: the trail held under its setting for 5 s of game time (once a visit); v3.33 MERGE: LogV, not an
            // ANOMALY (host decision: a guard cut in a busy room is expected)
            static volatile LONG s_qa;
            if (AiCutWatch(&g_aiCut, g_aiCap < AiTrailNow()->n, g_aiClock) && LogQuota(&s_qa, 40))
                LogV("after shock: the frame guard has held the %s trail at %d of %d copies for %.0f s (stage %lX room %lX; our part of "
                     "a frame %.2f ms, the slowest %.2f ms this visit, cap %.1f / %.1f ms): copies are missing from the trail",
                     kGpName[GP_AI][g_aiLevel > 0 && g_aiLevel < 4 ? g_aiLevel : 0], g_aiCap, AiTrailNow()->n, AI_CUT_S, room >> 16,
                     room & 0xFFFF, g_aiMsFrame, g_aiMsMax, AI_MS_CAP, AI_MS_CAP_LONG);
        }
    }
    AiRoomLog(room, look);
    if (!g_aiLogged[5]++) Log("after shock: the trail draws (%d echo(es) in its first frame, look %d)", g_aiEchoN, look);
}
static void AiDrawFrame(void)  // the scene pass (items.inc SparkleFrame), after Death Presence's fog, before Item Shine and the filter
{
    if (!g_aiInstalled || g_aiFail || g_aiLevel <= 0 || !g_fxOn) return;
    if (*(volatile int *)0xFD5C20 != 0 || !g_spInstalled || !FxGameplay()) return;
    __try {
        AiDrawC();
    } __except (AiFaultNote(GetExceptionInformation())) {
        AiFailNow("the draw", g_aiFaultCode, g_aiFaultAt);
    }
}
// after the filter (FxPass, where 1): look 1 after ThHeatPass and Death Presence's Thermal pass, look 3 after GvPass
static void AiPostDraw(void *dev, int look)
{
    LARGE_INTEGER t0;
    if (!g_aiInstalled || g_aiFail || g_aiLevel <= 0 || !dev || g_aiAskFrame != g_fxFrame || g_aiAskLook != look) return;
    if (*(volatile int *)0xFD5C20 != 0 || g_aiFrameStamp != g_fxFrame) return;
    QueryPerformanceCounter(&t0);
    BgPush(BGC_AI);  // v3.35 FX335: the frame ledger (inside the screen effects' part)
    g_aiMsRedraw = g_aiMsSpark = 0.0f;
    __try {
        if (AiDraw(dev, look)) g_aiDoneFrame = g_fxFrame;  // served
    } __except (AiFaultNote(GetExceptionInformation())) {
        AiFailNow("the post-filter draw", g_aiFaultCode, g_aiFaultAt);
    }
    BgPop();
    AiMsAdd(&t0);
    AiSlowLog(DpMsSince(&t0), g_aiDrawRoom);
}

// ---------------------------------------------------------------- v3.35 FX335: made ready at a room's first frame
// (echo.inc RfWarmFrame, with the row on). Once a device: the glow and heat shaders, the textures of its arcs and sprites,
// her body once in the copies' glow pass (zero light) and once in Thermal's heat pass (alpha 0), the sprite and ribbon
// states (deathpresence_draw.inc DpWarmStates: zero-colour quads); then, every room, each enemy type not seen yet this
// session gets one warm shock: its body drawn in the shock's glow pass at zero light on the next frame (the stun's first
// redraw of that type is not its first draw). Nothing shows. The user's log: the copies' first draw after a load cost a
// 276 ms frame ("after shock 259.2 ms"). Returns 1 when it did something now (bodies, quads, types: what).
static void *g_aiWarmDev;
static DWORD g_aiWarmTypes;  // enemy types whose shock pass was made (this device)
static int AiWarm(void *dev, int *bodies, int *quads, int *types, char *typeList, size_t n)
{
    BYTE *m = *(BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    int did = 0, o = 0;
    *bodies = *quads = *types = 0;
    if (n) typeList[0] = 0;
    if (!g_aiInstalled || g_aiFail || g_aiLevel <= 0 || !dev) return 0;
    if (g_aiWarmDev != dev) g_aiWarmTypes = 0;
    if (g_aiWarmDev != dev && m && g_aiHerFrame == g_fxFrame && g_gvDipHooks && FxShader(dev, FXT_GVGLOW) && FxShader(dev, FXT_HEAT)) {
        FxSaved sv;
        FxTexture(dev, FXX_SMOKE), FxTexture(dev, FXX_SVATLAS), FxTexture(dev, FXX_FLAME);
        if (FxSave(dev, &sv, 6)) {
            const Viewport8 *vp = g_aiVpOk && g_aiVpStamp == g_fxFrame ? &g_aiVp : &sv.vp;
            ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
            sv.vpSet = 1;
            __try {
                int b = 0;
                ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
                g_aiWarmNow = 1;
                *bodies = AiRedraw(dev, &sv, vp, 0, &b);
                g_aiWarmNow = 0;
                if (!g_aiFail) {
                    FxBase(dev);
                    SvZ(dev, 2);
                    *quads = DpWarmStates(dev);
                }
            } __finally {
                g_aiWarmNow = 0;
                FxRestore(dev, &sv);
            }
            g_aiWarmDev = dev;  // (also after a fault: never twice)
            did = 1;
        }
    }
    if (g_aiWarmDev == dev) {  // the room's enemy types not warmed yet: one warm shock each (drawn at zero light next frame)
        const BYTE *t = NULL;
        int guard = 0;
        while ((t = DpNext(t)) != NULL && guard++ < 96) {
            const BYTE *w = *(const BYTE *const *)(t + 0x408);
            int type;
            AiShock *s;
            if (!w) continue;
            type = *(const int *)(w + 0x20);
            if (type < 0 || type >= 0x13 || !DpTypeOk(type) || (g_aiWarmTypes >> type & 1u)) continue;
            g_aiWarmTypes |= 1u << type;
            s = AiShockAdd(t, 0.03f);  // (about two frames of game time: the model is collected, then drawn once)
            s->warm = 1;
            ++*types;
            if (o >= 0 && (size_t)o + 4 < n) o += sprintf_s(typeList + o, n - (size_t)o, "%s%02X", o ? " " : "", type);
            did = 1;
        }
    }
    return did;
}

// ---------------------------------------------------------------- install (Init, after DeathPresenceInstall)
static void AfterImageInstall(void)
{
    char buf[64];
    const char *p;
    // v3.30 AS330: the hidden AfterImageTrail key is retired (the row's value is the trail length): logged once if present
    if (GetPrivateProfileStringA("Gameplay", "AfterImageTrail", "", buf, sizeof buf, g_iniPath) && buf[0])
        Log("after shock: [Gameplay] AfterImageTrail=%s is no longer read: the After Shock row's value (Short / Medium / Long) "
            "is the trail length", buf);
    g_aiSound = GetPrivateProfileIntA("Gameplay", "AfterImageSound", 1, g_iniPath) != 0;
    g_aiDry = GetPrivateProfileIntA("Gameplay", "AfterImageDryRun", 0, g_iniPath) != 0;
    {   // v3.32 AS332: hidden AfterImageFan (%): the lens fan's width (100 = 0.5 x a copy's distance along the view axis)
        // v3.33 AS333: default 0 = the copies in line along her path in every camera (the user's v3.32 test); > 0 = v3.32's fan
        const int fan = GetPrivateProfileIntA("Gameplay", "AfterImageFan", AI_FAN_DEFAULT, g_iniPath);
        g_aiFanK = AiFanWidthK(fan);
        if (fan != AI_FAN_DEFAULT) Log("after shock: [Gameplay] AfterImageFan=%d: v3.32's lens fan x %.2f (0 = copies in line)", fan, g_aiFanK);
    }
    GetPrivateProfileStringA("Gameplay", "AfterImageSkip", "", buf, sizeof buf, g_iniPath);
    for (p = buf; *p;) {
        char *end;
        const long v = strtol(p, &end, 16);
        if (end == p) { ++p; continue; }
        if (v > 0 && v < 0x13) g_aiSkip[v] = 1;
        p = end;
    }
    g_aiRing.count = 0;
    if (!g_qsTickOn) {
        Log("after shock: no 0x4EF982 tick dispatcher: the feature is off");
        g_aiFail = 1;
        return;
    }
    if (!g_fxOn || !g_spInstalled || !g_thermHooks || !g_gvDipHooks) {  // no trail without its draw path, no stun without the trail
        Log("after shock: the draw path is not in (screen effects %s, HUD hook %s, model hook %s, draw-call override %s): the "
            "feature is off", g_fxOn ? "in" : "off", g_spInstalled ? "in" : "off", g_thermHooks ? "in" : "off", g_gvDipHooks ? "in" : "off");
        g_aiFail = 1;
        return;
    }
    g_aiInstalled = 1;
    g_aiReady = 1;
    Log("after shock: in (v3.35: copies in line along her path, no glow on her body (arcs and sparks stay), thinned by the effect budget; no exe patch: tick 004EF982, scene pass 004EFBFD, model "
        "hook 0052AEB5, draw-call override 004292EF / 00429338); %s (Short %d echoes over %.2f s, Medium %d over %.2f s, Long %d over %.2f s), sound %s%s; %s",
        kGpName[GP_AI][g_gp[GP_AI]], kAiTrail[0].n, kAiTrail[0].span, kAiTrail[1].n, kAiTrail[1].span, kAiTrail[2].n,
        kAiTrail[2].span, g_aiSound ? "on" : "off", g_aiDry ? ", DRY RUN (contacts logged, nothing injected)" : "",
        buf[0] ? buf : "no type skipped");  // v3.30 AS330; v3.35 FX335: "v3.35 ... no glow on her body"
}
